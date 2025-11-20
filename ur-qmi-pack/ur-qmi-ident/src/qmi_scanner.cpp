#include "qmi_scanner.h"
#include "qmi_dev_scan_agent.h"
#include <iostream>
#include <sstream>
#include <fstream>
#include <regex>
#include <cstdlib>
#include <unistd.h>
#include <sys/select.h>
#include <libudev.h>
#include <mutex>
#include <json/json.h>
#include <filesystem>
#include <qmi_device_registry.h>

#include <gateway.hpp>

using namespace DirectTemplate;

QMIScanner::QMIScanner()
    : m_profile_mode(ProfileMode::BASIC), m_monitoring(false), m_udev(nullptr), m_monitor(nullptr), m_monitor_fd(-1) {
    m_json_agent = std::make_unique<QMIDevScanAgent>(); 
    QMIDeviceRegistry::getInstance().clear();
}

QMIScanner::~QMIScanner() {
    stopMonitoring();
    cleanupUdev();
}

bool QMIScanner::initialize(ProfileMode mode) {
    m_profile_mode = mode;

    // Setup udev for monitoring
    setupUdev();

    // Get registry instance
    auto& registry = QMIDeviceRegistry::getInstance();
    
    // Clear registry for fresh initialization
    registry.clear();
    
    std::cout << "Starting initial QMI device scan..." << std::endl;

    // Perform initial scan based on mode and collect discovered devices
    std::vector<QMIDevice> discovered_devices;
    
    if (mode == ProfileMode::BASIC) {
        m_current_profiles = scanDeviceProfiles();
        
        // Convert profiles to QMI devices for registry
        for (const auto& profile : m_current_profiles) {
            QMIDevice device;
            device.device_path = profile.path;
            device.imei = profile.imei;
            device.model = profile.model;
            device.firmware_version = profile.firmware;
            device.is_available = true;
            device.action = "added";
            // Initialize empty SIM status - will be updated by individual queries if needed
            device.sim_status.card_state = "unknown";
            device.sim_status.application_state = "unknown";
            discovered_devices.push_back(device);
        }
        
        // Report profiles
        for (const auto& profile : m_current_profiles) {
            reportProfile(profile, true);
        }
        
    } else if (mode == ProfileMode::ADVANCED) {
        m_current_advanced_profiles = scanAdvancedDeviceProfiles();
        
        // Convert advanced profiles to QMI devices for registry
        for (const auto& profile : m_current_advanced_profiles) {
            QMIDevice device;
            device.device_path = profile.basic.path;
            device.imei = profile.basic.imei;
            device.model = profile.basic.model;
            device.manufacturer = profile.manufacturer;
            device.firmware_version = profile.basic.firmware;
            device.is_available = true;
            device.action = "added";
            // Initialize empty SIM status - will be updated by individual queries if needed
            device.sim_status.card_state = "unknown";
            device.sim_status.application_state = "unknown";
            discovered_devices.push_back(device);
        }
        
        // Report advanced profiles
        for (const auto& profile : m_current_advanced_profiles) {
            reportAdvancedProfile(profile, true);
        }
        
    } else if (mode == ProfileMode::MANAGER) {
        // Scan full devices with complete information including SIM status
        m_current_devices = scanDevices();
        discovered_devices = m_current_devices;
        
        // Report devices
        for (const auto& device : m_current_devices) {
            reportDevice(device, true);
        }
    }

    // Initialize registry with all discovered devices in one operation
    registry.initializeFromStartupScan(discovered_devices);
    
    std::cout << "QMI Scanner initialization completed. Registry contains " 
              << registry.getDeviceCount() << " devices." << std::endl;

    return true;
}

void QMIScanner::startMonitoring() {
    if (m_monitoring.load()) {
        return;
    }

    m_monitoring = true;
    m_monitor_thread = std::make_unique<std::thread>(&QMIScanner::monitorLoop, this);
}

void QMIScanner::stopMonitoring() {
    if (m_monitoring.load()) {
        m_monitoring = false;
        if (m_monitor_thread && m_monitor_thread->joinable()) {
            m_monitor_thread->join();
        }
    }
}

void QMIScanner::setDeviceCallback(DeviceCallback callback) {
    m_callback = callback;
}

void QMIScanner::setProfileCallback(ProfileCallback callback) {
    m_profile_callback = callback;
}

void QMIScanner::setAdvancedProfileCallback(AdvancedProfileCallback callback) {
    m_advanced_callback = callback;
}

std::vector<QMIDevice> QMIScanner::getCurrentDevices() const {
    std::lock_guard<std::mutex> lock(m_devices_mutex);
    return m_current_devices;
}

std::vector<DeviceProfile> QMIScanner::getCurrentProfiles() const {
    std::lock_guard<std::mutex> lock(m_devices_mutex);
    return m_current_profiles;
}

std::vector<AdvancedDeviceProfile> QMIScanner::getCurrentAdvancedProfiles() const {
    std::lock_guard<std::mutex> lock(m_devices_mutex);
    return m_current_advanced_profiles;
}

std::vector<QMIDevice> QMIScanner::scanDevices() {
    std::vector<QMIDevice> devices;
    std::vector<std::string> device_paths = findQMIDevices();

    for (const auto& path : device_paths) {
        QMIDevice device = queryDeviceInfo(path);
        if (!device.device_path.empty()) {
            devices.push_back(device);
        }
    }

    return devices;
}

std::vector<DeviceProfile> QMIScanner::scanDeviceProfiles() {
    std::vector<DeviceProfile> profiles;
    std::vector<std::string> device_paths = findQMIDevices();

    for (const auto& path : device_paths) {
        DeviceProfile profile = queryDeviceProfile(path);
        if (!profile.path.empty()) {
            profiles.push_back(profile);
        }
    }

    return profiles;
}

std::vector<AdvancedDeviceProfile> QMIScanner::scanAdvancedDeviceProfiles() {
    std::vector<AdvancedDeviceProfile> profiles;
    std::vector<std::string> device_paths = findQMIDevices();

    for (const auto& path : device_paths) {
        AdvancedDeviceProfile profile = queryAdvancedDeviceProfile(path);
        if (!profile.basic.path.empty()) {
            profiles.push_back(profile);
        }
    }

    return profiles;
}

std::vector<std::string> QMIScanner::findQMIDevices() {
    std::vector<std::string> devices;

    // Look for QMI devices in /dev
    for (const auto& entry : std::filesystem::directory_iterator("/dev")) {
        std::string filename = entry.path().filename().string();
        if (filename.find("cdc-wdm") == 0) {
            devices.push_back(entry.path().string());
        }
    }

    return devices;
}

QMIDevice QMIScanner::queryDeviceInfo(const std::string& device_path) {
    QMIDevice device;
    device.device_path = device_path;
    device.is_available = true;
    device.action = "added";  // Default action

    // Query device capabilities
    std::string caps_cmd = "qmicli -d " + device_path + " --dms-get-capabilities";
    std::string caps_output = executeCommand(caps_cmd);

    // Query device IDs
    std::string ids_cmd = "qmicli -d " + device_path + " --dms-get-ids";
    std::string ids_output = executeCommand(ids_cmd);

    // Query device model
    std::string model_cmd = "qmicli -d " + device_path + " --dms-get-model";
    std::string model_output = executeCommand(model_cmd);

    // Query device manufacturer
    std::string mfr_cmd = "qmicli -d " + device_path + " --dms-get-manufacturer";
    std::string mfr_output = executeCommand(mfr_cmd);

    // Query firmware version
    std::string fw_cmd = "qmicli -d " + device_path + " --dms-get-revision";
    std::string fw_output = executeCommand(fw_cmd);

    // Parse all outputs
    parseDeviceInfo(caps_output + "\n" + ids_output + "\n" + model_output + "\n" + mfr_output + "\n" + fw_output, device);

    // Collect SIM status
    device.sim_status = collectSIMStatus(device_path);

    return device;
}

DeviceProfile QMIScanner::queryDeviceProfile(const std::string& device_path) {
    DeviceProfile profile;
    profile.path = device_path;
    parseDeviceProfile(device_path, profile);
    return profile;
}

AdvancedDeviceProfile QMIScanner::queryAdvancedDeviceProfile(const std::string& device_path) {
    AdvancedDeviceProfile profile;
    profile.basic.path = device_path;
    parseAdvancedDeviceProfile(device_path, profile);
    return profile;
}

void QMIScanner::parseDeviceProfile(const std::string& device_path, DeviceProfile& profile) {
    // Basic info queries with safe execution
    std::string ids_output = executeCommandSafe("qmicli -d " + device_path + " --dms-get-ids");
    std::string model_output = executeCommandSafe("qmicli -d " + device_path + " --dms-get-model");
    std::string fw_output = executeCommandSafe("qmicli -d " + device_path + " --dms-get-revision");
    std::string caps_output = executeCommandSafe("qmicli -d " + device_path + " --dms-get-capabilities");
    
    // Use UIM service for SIM status instead of DMS
    std::string uim_status_output = executeCommandSafe("qmicli -d " + device_path + " --uim-get-card-status");

    // Parse IMEI
    std::regex imei_regex(R"(IMEI:\s*'([^']+)')");
    std::smatch match;
    if (std::regex_search(ids_output, match, imei_regex)) {
        profile.imei = match[1].str();
    }

    // Parse Model
    std::regex model_regex(R"(Model:\s*'([^']+)')");
    if (std::regex_search(model_output, match, model_regex)) {
        profile.model = match[1].str();
    }

    // Parse Firmware
    std::regex fw_regex(R"(Revision:\s*'([^']+)')");
    if (std::regex_search(fw_output, match, fw_regex)) {
        profile.firmware = match[1].str();
    }

    // Parse bands from capabilities - more robust parsing
    std::istringstream iss(caps_output);
    std::string line;
    while (std::getline(iss, line)) {
        if ((line.find("Band") != std::string::npos || line.find("LTE") != std::string::npos || 
             line.find("WCDMA") != std::string::npos || line.find("GSM") != std::string::npos) &&
            line.find(":") != std::string::npos) {
            profile.bands.push_back(line);
        }
    }

    // Check SIM presence using UIM card status
    profile.sim_present = (uim_status_output.find("Card state: 'present'") != std::string::npos) ||
                         (uim_status_output.find("ready") != std::string::npos);

    // Check PIN lock status
    profile.pin_locked = (uim_status_output.find("PIN1 state: 'enabled-not-verified'") != std::string::npos) ||
                        (uim_status_output.find("PIN1 state: 'blocked'") != std::string::npos);

    // Check GPS support from capabilities
    profile.gps_supported = (caps_output.find("gps") != std::string::npos) ||
                           (caps_output.find("location") != std::string::npos) ||
                           (caps_output.find("positioning") != std::string::npos);

    // Determine max carriers from capabilities
    profile.max_carriers = 1;
    if (caps_output.find("carrier aggregation") != std::string::npos) {
        // Try to extract number
        std::regex ca_regex(R"(carrier[s]?\s*[:\-]\s*(\d+))");
        if (std::regex_search(caps_output, match, ca_regex)) {
            profile.max_carriers = static_cast<uint8_t>(std::stoi(match[1].str()));
        } else {
            profile.max_carriers = 3; // Default for CA-enabled devices
        }
    }

    // Set reasonable defaults if parsing failed
    if (profile.imei.empty()) profile.imei = "Unknown";
    if (profile.model.empty()) profile.model = "Unknown";
    if (profile.firmware.empty()) profile.firmware = "Unknown";
}

void QMIScanner::parseAdvancedDeviceProfile(const std::string& device_path, AdvancedDeviceProfile& profile) {
    // First get basic profile
    parseDeviceProfile(device_path, profile.basic);

    // Advanced queries with proper error handling and parsing (only working commands)
    profile.manufacturer = parseManufacturer(executeCommandSafe("qmicli -d " + device_path + " --dms-get-manufacturer"));
    profile.msisdn = parseMSISDN(executeCommandSafe("qmicli -d " + device_path + " --dms-get-msisdn"));
    profile.power_state = parsePowerState(executeCommandSafe("qmicli -d " + device_path + " --dms-get-power-state"));
    profile.hardware_revision = parseHardwareRevision(executeCommandSafe("qmicli -d " + device_path + " --dms-get-hardware-revision"));
    profile.operating_mode = parseOperatingMode(executeCommandSafe("qmicli -d " + device_path + " --dms-get-operating-mode"));
    
    // Use UIM service for SIM-related queries
    std::string uim_status_output = executeCommandSafe("qmicli -d " + device_path + " --uim-get-card-status");
    profile.iccid = parseICCID(uim_status_output);
    profile.uim_state = parseUIMState(uim_status_output);
    profile.pin_status = parsePINStatus(uim_status_output);
    
    // Working advanced commands
    profile.time = parseTime(executeCommandSafe("qmicli -d " + device_path + " --dms-get-time"));
    profile.band_capabilities = parseBandCapabilities(executeCommandSafe("qmicli -d " + device_path + " --dms-get-band-capabilities"));
    profile.software_version = parseSoftwareVersion(executeCommandSafe("qmicli -d " + device_path + " --dms-get-software-version"));
    
    // Try factory SKU (usually works)
    profile.factory_sku = parseFactorySKU(executeCommandSafe("qmicli -d " + device_path + " --dms-get-factory-sku"));
    
    // Network interface MAC addresses (alternative approach)
    profile.mac_address_wlan = getMACAddress("wlan0");
    profile.mac_address_bt = getMACAddress("hci0");
    
    // Set defaults for removed/failing commands
    profile.prl_version = "";  // Removed: NotSupported
    profile.activation_state = "";  // Removed: DeviceUnsupported
    profile.user_lock_state = "";  // Removed: NotSupported
    profile.firmware_preference = "";  // Removed: WmsInvalidMessageId
    profile.boot_image_download_mode = "";  // Removed: WmsInvalidMessageId
    profile.usb_composition = "";  // Removed: WmsInvalidMessageId
    profile.stored_images.clear();  // Removed: WmsInvalidMessageId
    profile.imsi = "";  // Removed: Unknown option
    
    // Set reasonable defaults for empty values
    if (profile.manufacturer.empty()) profile.manufacturer = "Unknown";
    if (profile.power_state.empty()) profile.power_state = "Online";
    if (profile.operating_mode.empty()) profile.operating_mode = "Online";
}

std::string QMIScanner::executeCommand(const std::string& command) {
    std::string result;
    FILE* pipe = popen(command.c_str(), "r");

    if (pipe) {
        char buffer[256];
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            result += buffer;
        }
        pclose(pipe);
    }

    return result;
}

std::string QMIScanner::executeCommandSafe(const std::string& command) {
    std::string result = executeCommand(command + " 2>&1");
    
    // Log errors to stderr but don't fail completely
    if (result.find("error:") != std::string::npos) {
        std::cerr << "QMI Command failed: " << command << std::endl;
        std::cerr << "Output: " << result << std::endl;
        return "";
    }
    
    return result;
}

std::string QMIScanner::parseManufacturer(const std::string& output) {
    if (output.empty()) return "Unknown";
    
    std::regex mfr_regex(R"(Manufacturer:\s*'([^']+)')");
    std::smatch match;
    if (std::regex_search(output, match, mfr_regex)) {
        return match[1].str();
    }
    return "Unknown";
}

std::string QMIScanner::parseMSISDN(const std::string& output) {
    if (output.empty()) return "";
    
    std::regex msisdn_regex(R"(MSISDN:\s*'([^']+)')");
    std::smatch match;
    if (std::regex_search(output, match, msisdn_regex)) {
        return match[1].str();
    }
    return "";
}

std::string QMIScanner::parsePowerState(const std::string& output) {
    if (output.empty()) return "Online";
    
    std::regex power_regex(R"(Power state:\s*'([^']+)')");
    std::smatch match;
    if (std::regex_search(output, match, power_regex)) {
        return match[1].str();
    }
    
    // Handle case where power state is shown differently
    if (output.find("external-source") != std::string::npos) return "External Source";
    if (output.find("battery") != std::string::npos) return "Battery";
    
    return "Online";
}

std::string QMIScanner::parseHardwareRevision(const std::string& output) {
    if (output.empty()) return "";
    
    std::regex hw_regex(R"(Revision:\s*'([^']+)')");
    std::smatch match;
    if (std::regex_search(output, match, hw_regex)) {
        return match[1].str();
    }
    
    // Alternative format without quotes
    std::regex hw_regex2(R"(Revision:\s*([^\n\r]+))");
    if (std::regex_search(output, match, hw_regex2)) {
        return match[1].str();
    }
    
    return "";
}

std::string QMIScanner::parseOperatingMode(const std::string& output) {
    if (output.empty()) return "Online";
    
    std::regex mode_regex(R"(Mode:\s*'([^']+)')");
    std::smatch match;
    if (std::regex_search(output, match, mode_regex)) {
        return match[1].str();
    }
    
    // Check for mode in different format
    if (output.find("online") != std::string::npos) return "Online";
    if (output.find("offline") != std::string::npos) return "Offline";
    if (output.find("low-power") != std::string::npos) return "Low Power";
    
    return "Online";
}

std::string QMIScanner::parseICCID(const std::string& output) {
    if (output.empty()) return "";
    
    // Look for Application ID which contains ICCID-like information
    std::regex app_id_regex(R"(Application ID:\s*\n\s*([A-F0-9:]+))");
    std::smatch match;
    if (std::regex_search(output, match, app_id_regex)) {
        std::string app_id = match[1].str();
        // Remove colons and extract meaningful part
        app_id.erase(std::remove(app_id.begin(), app_id.end(), ':'), app_id.end());
        if (app_id.length() >= 16) {
            return app_id.substr(0, 20);  // Return first 20 chars as ICCID
        }
    }
    
    return "";
}

std::string QMIScanner::parseIMSI(const std::string& output) {
    if (output.empty()) return "";
    
    std::regex imsi_regex(R"(IMSI:\s*'([^']+)')");
    std::smatch match;
    if (std::regex_search(output, match, imsi_regex)) {
        return match[1].str();
    }
    
    // Alternative format
    std::regex imsi_regex2(R"(IMSI:\s*([0-9]+))");
    if (std::regex_search(output, match, imsi_regex2)) {
        return match[1].str();
    }
    
    return "";
}

std::string QMIScanner::parseUIMState(const std::string& output) {
    if (output.empty()) return "Unknown";
    
    if (output.find("Card state: 'present'") != std::string::npos) {
        return "Present";
    }
    if (output.find("Card state: 'absent'") != std::string::npos) {
        return "Absent";
    }
    if (output.find("Card state: 'error'") != std::string::npos) {
        return "Error";
    }
    
    return "Unknown";
}

std::string QMIScanner::parsePINStatus(const std::string& output) {
    if (output.empty()) return "Unknown";
    
    if (output.find("PIN1 state: 'enabled-verified'") != std::string::npos) {
        return "Verified";
    }
    if (output.find("PIN1 state: 'enabled-not-verified'") != std::string::npos) {
        return "Not Verified";
    }
    if (output.find("PIN1 state: 'disabled'") != std::string::npos) {
        return "Disabled";
    }
    if (output.find("PIN1 state: 'blocked'") != std::string::npos) {
        return "Blocked";
    }
    
    return "Unknown";
}

std::string QMIScanner::parseTime(const std::string& output) {
    if (output.empty()) return "";
    
    // Parse system time from the output
    std::regex system_time_regex(R"(System time:\s*'([^']+)')");
    std::smatch match;
    if (std::regex_search(output, match, system_time_regex)) {
        return match[1].str();
    }
    
    // Alternative format - Time count
    std::regex time_count_regex(R"(Time count:\s*'([^']+)')");
    if (std::regex_search(output, match, time_count_regex)) {
        return match[1].str();
    }
    
    return "";
}

std::string QMIScanner::parseBandCapabilities(const std::string& output) {
    if (output.empty()) return "";
    
    std::string capabilities;
    std::istringstream iss(output);
    std::string line;
    
    while (std::getline(iss, line)) {
        // Look for band information lines
        if ((line.find("Bands:") != std::string::npos || 
             line.find("LTE bands:") != std::string::npos ||
             line.find("NR5G bands:") != std::string::npos) &&
            line.find(":") != std::string::npos) {
            if (!capabilities.empty()) capabilities += "; ";
            capabilities += line.substr(line.find(":") + 1);
        }
    }
    
    // Clean up the string
    std::regex spaces_regex(R"(\s+)");
    capabilities = std::regex_replace(capabilities, spaces_regex, " ");
    
    // Trim leading/trailing spaces
    size_t start = capabilities.find_first_not_of(" \t");
    size_t end = capabilities.find_last_not_of(" \t");
    if (start != std::string::npos && end != std::string::npos) {
        capabilities = capabilities.substr(start, end - start + 1);
    }
    
    return capabilities;
}

std::vector<std::string> QMIScanner::parseStoredImages(const std::string& output) {
    std::vector<std::string> images;
    std::istringstream iss(output);
    std::string line;
    
    while (std::getline(iss, line)) {
        if (line.find("Image") != std::string::npos || line.find("Type:") != std::string::npos) {
            images.push_back(line);
        }
    }
    
    return images;
}

std::string QMIScanner::parseSoftwareVersion(const std::string& output) {
    if (output.empty()) return "";
    
    std::regex sw_regex(R"(Software version:\s*'([^']+)')");
    std::smatch match;
    if (std::regex_search(output, match, sw_regex)) {
        return match[1].str();
    }
    
    return "";
}

std::string QMIScanner::parseUSBComposition(const std::string& output) {
    if (output.empty()) return "";
    
    std::regex usb_regex(R"(USB composition:\s*'([^']+)')");
    std::smatch match;
    if (std::regex_search(output, match, usb_regex)) {
        return match[1].str();
    }
    
    return "";
}

std::string QMIScanner::parseFactorySKU(const std::string& output) {
    if (output.empty()) return "";
    
    std::regex sku_regex(R"(SKU:\s*'([^']+)')");
    std::smatch match;
    if (std::regex_search(output, match, sku_regex)) {
        return match[1].str();
    }
    
    return "";
}

std::string QMIScanner::getMACAddress(const std::string& interface) {
    std::string command = "cat /sys/class/net/" + interface + "/address 2>/dev/null";
    std::string result = executeCommand(command);
    
    // Remove newline
    if (!result.empty() && result.back() == '\n') {
        result.pop_back();
    }
    
    return result;
}

SIMStatus QMIScanner::collectSIMStatus(const std::string& device_path) {
    std::string sim_cmd = "qmicli -d " + device_path + " --uim-get-card-status";
    std::string sim_output = executeCommandSafe(sim_cmd);
    
    return parseSIMCardStatus(sim_output);
}

SIMStatus QMIScanner::parseSIMCardStatus(const std::string& output) {
    SIMStatus sim_status;
    
    if (output.empty()) {
        // Set default values for empty output
        sim_status.card_state = "unknown";
        sim_status.application_state = "unknown";
        return sim_status;
    }
    
    std::istringstream iss(output);
    std::string line;
    bool in_slot_section = false;
    bool in_application_section = false;
    bool in_provisioning_section = false;
    
    while (std::getline(iss, line)) {
        // Trim whitespace
        line.erase(0, line.find_first_not_of(" \t"));
        line.erase(line.find_last_not_of(" \t") + 1);
        
        if (line.find("Provisioning applications:") != std::string::npos) {
            in_provisioning_section = true;
            continue;
        }
        
        if (in_provisioning_section && !in_slot_section) {
            if (line.find("Primary GW:") != std::string::npos) {
                std::regex gw_regex(R"(Primary GW:\s+slot '(\d+)', application '(\d+)')");
                std::smatch match;
                if (std::regex_search(line, match, gw_regex)) {
                    sim_status.primary_gw_slot = match[1].str();
                    sim_status.primary_gw_application = match[2].str();
                }
            } else if (line.find("Primary 1X:") != std::string::npos) {
                sim_status.primary_1x_status = (line.find("session doesn't exist") != std::string::npos) ? "none" : "active";
            } else if (line.find("Secondary GW:") != std::string::npos) {
                sim_status.secondary_gw_status = (line.find("session doesn't exist") != std::string::npos) ? "none" : "active";
            } else if (line.find("Secondary 1X:") != std::string::npos) {
                sim_status.secondary_1x_status = (line.find("session doesn't exist") != std::string::npos) ? "none" : "active";
            }
        }
        
        if (line.find("Slot [") != std::string::npos) {
            in_slot_section = true;
            in_provisioning_section = false;
            continue;
        }
        
        if (line.find("Application [") != std::string::npos) {
            in_application_section = true;
            continue;
        }
        
        if (in_slot_section && !in_application_section) {
            if (line.find("Card state:") != std::string::npos) {
                std::regex state_regex(R"(Card state:\s*'([^']+)')");
                std::smatch match;
                if (std::regex_search(line, match, state_regex)) {
                    sim_status.card_state = match[1].str();
                }
            } else if (line.find("UPIN state:") != std::string::npos) {
                std::regex upin_regex(R"(UPIN state:\s*'([^']+)')");
                std::smatch match;
                if (std::regex_search(line, match, upin_regex)) {
                    sim_status.upin_state = match[1].str();
                }
            } else if (line.find("UPIN retries:") != std::string::npos) {
                std::regex retries_regex(R"(UPIN retries:\s*'(\d+)')");
                std::smatch match;
                if (std::regex_search(line, match, retries_regex)) {
                    sim_status.upin_retries = std::stoi(match[1].str());
                }
            } else if (line.find("UPUK retries:") != std::string::npos) {
                std::regex retries_regex(R"(UPUK retries:\s*'(\d+)')");
                std::smatch match;
                if (std::regex_search(line, match, retries_regex)) {
                    sim_status.upuk_retries = std::stoi(match[1].str());
                }
            }
        }
        
        if (in_application_section) {
            if (line.find("Application type:") != std::string::npos) {
                std::regex type_regex(R"(Application type:\s*'([^']+)')");
                std::smatch match;
                if (std::regex_search(line, match, type_regex)) {
                    sim_status.application_type = match[1].str();
                }
            } else if (line.find("Application state:") != std::string::npos) {
                std::regex state_regex(R"(Application state:\s*'([^']+)')");
                std::smatch match;
                if (std::regex_search(line, match, state_regex)) {
                    sim_status.application_state = match[1].str();
                }
            } else if (line.find("Application ID:") != std::string::npos) {
                // Read next line for application ID
                if (std::getline(iss, line)) {
                    line.erase(0, line.find_first_not_of(" \t"));
                    sim_status.application_id = line;
                }
            } else if (line.find("Personalization state:") != std::string::npos) {
                std::regex pers_regex(R"(Personalization state:\s*'([^']+)')");
                std::smatch match;
                if (std::regex_search(line, match, pers_regex)) {
                    sim_status.personalization_state = match[1].str();
                }
            } else if (line.find("UPIN replaces PIN1:") != std::string::npos) {
                sim_status.upin_replaces_pin1 = (line.find("'yes'") != std::string::npos);
            } else if (line.find("PIN1 state:") != std::string::npos) {
                std::regex pin1_regex(R"(PIN1 state:\s*'([^']+)')");
                std::smatch match;
                if (std::regex_search(line, match, pin1_regex)) {
                    sim_status.pin1_state = match[1].str();
                }
            } else if (line.find("PIN1 retries:") != std::string::npos) {
                std::regex retries_regex(R"(PIN1 retries:\s*'(\d+)')");
                std::smatch match;
                if (std::regex_search(line, match, retries_regex)) {
                    sim_status.pin1_retries = std::stoi(match[1].str());
                }
            } else if (line.find("PUK1 retries:") != std::string::npos) {
                std::regex retries_regex(R"(PUK1 retries:\s*'(\d+)')");
                std::smatch match;
                if (std::regex_search(line, match, retries_regex)) {
                    sim_status.puk1_retries = std::stoi(match[1].str());
                }
            } else if (line.find("PIN2 state:") != std::string::npos) {
                std::regex pin2_regex(R"(PIN2 state:\s*'([^']+)')");
                std::smatch match;
                if (std::regex_search(line, match, pin2_regex)) {
                    sim_status.pin2_state = match[1].str();
                }
            } else if (line.find("PIN2 retries:") != std::string::npos) {
                std::regex retries_regex(R"(PIN2 retries:\s*'(\d+)')");
                std::smatch match;
                if (std::regex_search(line, match, retries_regex)) {
                    sim_status.pin2_retries = std::stoi(match[1].str());
                }
            } else if (line.find("PUK2 retries:") != std::string::npos) {
                std::regex retries_regex(R"(PUK2 retries:\s*'(\d+)')");
                std::smatch match;
                if (std::regex_search(line, match, retries_regex)) {
                    sim_status.puk2_retries = std::stoi(match[1].str());
                }
            }
        }
    }
    
    return sim_status;
}

std::string sendDeviceDataTargeted(const Json::Value& device_json, const std::string& target_client = "ur-dumped-messages") {
    try {
        while (!GlobalClientThraedRef || !GlobalClientThraedRef->isConnected()) {
            std::cerr << "Target Thread process Warning: Client Thread not connected, cannot send device data" << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            if (!g_running.load()) return "stopped";
        }
        
        if (!g_requester) {
            std::cerr << "Error: Targeted RPC Requester not initialized" << std::endl;
            return "error";
        }
        
        std::cout << "Target Thread process: Client Thread state is connected" << std::endl;
        
        Json::StreamWriterBuilder writer;
        std::string device_data = Json::writeString(writer, device_json);
        
        g_requester->sendTargetedRequest(target_client, "qmi-stack-notification", device_data, 
            [](bool success, const std::string& result, const std::string& error_message, int error_code) {
                if (success) {
                    Utils::logInfo("Device data processed successfully: " + result);
                } else {
                    Utils::logError("Failed to process device data: " + error_message);
                }
            });
            
        Utils::logInfo("Targeted device data request sent to " + target_client);
        return "sent";
        
    } catch (const DirectTemplateException& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return "error";
    }
}

std::string QMIScanner::generateDeviceWithSimStatusJson(const QMIDevice& device, bool pretty) {
    Json::Value device_json;
    
    device_json["device_path"] = device.device_path;
    device_json["imei"] = device.imei;
    device_json["model"] = device.model;
    device_json["manufacturer"] = device.manufacturer;
    device_json["firmware_version"] = device.firmware_version;
    device_json["is_available"] = device.is_available;
    device_json["action"] = device.action;
    
    Json::Value bands_array(Json::arrayValue);
    for (const auto& band : device.supported_bands) {
        bands_array.append(band);
    }
    device_json["supported_bands"] = bands_array;
    
    Json::Value sim_status_json;
    sim_status_json["card_state"] = device.sim_status.card_state;
    sim_status_json["upin_state"] = device.sim_status.upin_state;
    sim_status_json["upin_retries"] = device.sim_status.upin_retries;
    sim_status_json["upuk_retries"] = device.sim_status.upuk_retries;
    sim_status_json["application_type"] = device.sim_status.application_type;
    sim_status_json["application_state"] = device.sim_status.application_state;
    sim_status_json["application_id"] = device.sim_status.application_id;
    sim_status_json["personalization_state"] = device.sim_status.personalization_state;
    sim_status_json["upin_replaces_pin1"] = device.sim_status.upin_replaces_pin1;
    sim_status_json["pin1_state"] = device.sim_status.pin1_state;
    sim_status_json["pin1_retries"] = device.sim_status.pin1_retries;
    sim_status_json["puk1_retries"] = device.sim_status.puk1_retries;
    sim_status_json["pin2_state"] = device.sim_status.pin2_state;
    sim_status_json["pin2_retries"] = device.sim_status.pin2_retries;
    sim_status_json["puk2_retries"] = device.sim_status.puk2_retries;
    sim_status_json["primary_gw_slot"] = device.sim_status.primary_gw_slot;
    sim_status_json["primary_gw_application"] = device.sim_status.primary_gw_application;
    sim_status_json["primary_1x_status"] = device.sim_status.primary_1x_status;
    sim_status_json["secondary_gw_status"] = device.sim_status.secondary_gw_status;
    sim_status_json["secondary_1x_status"] = device.sim_status.secondary_1x_status;
    
    device_json["sim-status"] = sim_status_json;

    try {
        sendDeviceDataTargeted(device_json, "ur-qmi-watchdog");
    } catch (const DirectTemplateException& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return "";
    }
    
    return pretty ? m_json_agent->formatJsonPretty(device_json) : m_json_agent->formatJsonCompact(device_json);
}

std::string QMIScanner::generateDevicesArrayWithSimStatusJson(const std::vector<QMIDevice>& devices, bool pretty) {
    Json::Value root;
    Json::Value devices_array(Json::arrayValue);
    
    for (const auto& device : devices) {
        Json::Value device_json;
        device_json["device_path"] = device.device_path;
        device_json["imei"] = device.imei;
        device_json["model"] = device.model;
        device_json["manufacturer"] = device.manufacturer;
        device_json["is_available"] = device.is_available;
        device_json["action"] = device.action;
        
        // Add SIM status
        Json::Value sim_status_json;
        sim_status_json["card_state"] = device.sim_status.card_state;
        sim_status_json["upin_state"] = device.sim_status.upin_state;
        sim_status_json["upin_retries"] = device.sim_status.upin_retries;
        sim_status_json["upuk_retries"] = device.sim_status.upuk_retries;
        sim_status_json["application_type"] = device.sim_status.application_type;
        sim_status_json["application_state"] = device.sim_status.application_state;
        sim_status_json["application_id"] = device.sim_status.application_id;
        sim_status_json["personalization_state"] = device.sim_status.personalization_state;
        sim_status_json["upin_replaces_pin1"] = device.sim_status.upin_replaces_pin1;
        sim_status_json["pin1_state"] = device.sim_status.pin1_state;
        sim_status_json["pin1_retries"] = device.sim_status.pin1_retries;
        sim_status_json["puk1_retries"] = device.sim_status.puk1_retries;
        sim_status_json["pin2_state"] = device.sim_status.pin2_state;
        sim_status_json["pin2_retries"] = device.sim_status.pin2_retries;
        sim_status_json["puk2_retries"] = device.sim_status.puk2_retries;
        sim_status_json["primary_gw_slot"] = device.sim_status.primary_gw_slot;
        sim_status_json["primary_gw_application"] = device.sim_status.primary_gw_application;
        sim_status_json["primary_1x_status"] = device.sim_status.primary_1x_status;
        sim_status_json["secondary_gw_status"] = device.sim_status.secondary_gw_status;
        sim_status_json["secondary_1x_status"] = device.sim_status.secondary_1x_status;
        
        device_json["sim-status"] = sim_status_json;
        devices_array.append(device_json);
    }
    
    root["devices"] = devices_array;
    return pretty ? m_json_agent->formatJsonPretty(root) : m_json_agent->formatJsonCompact(root);
}

std::string QMIScanner::validateAndExtractSIMJson(const std::string& json_string) {
    Json::Value root = m_json_agent->parseJsonString(json_string);
    
    if (root.isNull()) {
        return "Invalid JSON format";
    }
    
    if (!root.isMember("devices") || !root["devices"].isArray()) {
        return "Missing or invalid 'devices' array";
    }
    
    for (const auto& device : root["devices"]) {
        if (!device.isMember("device_path") || !device.isMember("sim-status")) {
            return "Missing required fields in device object";
        }
        
        const Json::Value& sim_status = device["sim-status"];
        if (!sim_status.isMember("card_state") || !sim_status.isMember("application_state")) {
            return "Missing required SIM status fields";
        }
    }
    
    return "Valid JSON format";
}

void QMIScanner::parseDeviceInfo(const std::string& output, QMIDevice& device) {
    std::istringstream iss(output);
    std::string line;

    while (std::getline(iss, line)) {
        // Parse IMEI
        if (line.find("IMEI:") != std::string::npos) {
            std::regex imei_regex(R"(IMEI:\s*'([^']+)')");
            std::smatch match;
            if (std::regex_search(line, match, imei_regex)) {
                device.imei = match[1].str();
            }
        }

        // Parse Model
        if (line.find("Model:") != std::string::npos) {
            std::regex model_regex(R"(Model:\s*'([^']+)')");
            std::smatch match;
            if (std::regex_search(line, match, model_regex)) {
                device.model = match[1].str();
            }
        }

        // Parse Manufacturer
        if (line.find("Manufacturer:") != std::string::npos) {
            std::regex mfr_regex(R"(Manufacturer:\s*'([^']+)')");
            std::smatch match;
            if (std::regex_search(line, match, mfr_regex)) {
                device.manufacturer = match[1].str();
            }
        }

        // Parse Firmware version
        if (line.find("Revision:") != std::string::npos) {
            std::regex fw_regex(R"(Revision:\s*'([^']+)')");
            std::smatch match;
            if (std::regex_search(line, match, fw_regex)) {
                device.firmware_version = match[1].str();
            }
        }

        // Parse supported bands (simplified - would need more detailed parsing)
        if (line.find("Band") != std::string::npos && line.find("supported") != std::string::npos) {
            device.supported_bands.push_back(line);
        }
    }
}

void QMIScanner::reportDevice(const QMIDevice& device, bool added) {
    auto& registry = QMIDeviceRegistry::getInstance();
    if (added) {
        registry.addDevice(device);
    } else {
        registry.removeDevice(device.device_path);
    }
    Json::Value report;
    Json::Value device_json;

    device_json["device_path"] = device.device_path;
    device_json["imei"] = device.imei;
    device_json["model"] = device.model;
    device_json["manufacturer"] = device.manufacturer;
    device_json["firmware_version"] = device.firmware_version;
    device_json["is_available"] = device.is_available;

    Json::Value bands(Json::arrayValue);
    for (const auto& band : device.supported_bands) {
        bands.append(band);
    }
    device_json["supported_bands"] = bands;

    report["event"] = added ? "device_added" : "device_removed";
    report["timestamp"] = static_cast<int64_t>(time(nullptr));
    report["device"] = device_json;
    report["registry_device_count"] = registry.getDeviceCount();

    Json::StreamWriterBuilder builder;
    builder["indentation"] = "";
    std::unique_ptr<Json::StreamWriter> writer(builder.newStreamWriter());

    std::ostringstream oss;
    writer->write(report, &oss);

    std::cout << oss.str() << std::endl;

    // Call callback if set
    if (m_callback) {
        m_callback(device, added);
    }
}

void QMIScanner::reportProfile(const DeviceProfile& profile, bool added) {
    Json::Value report;
    Json::Value profile_json;

    profile_json["path"] = profile.path;
    profile_json["imei"] = profile.imei;
    profile_json["model"] = profile.model;
    profile_json["firmware"] = profile.firmware;
    profile_json["sim_present"] = profile.sim_present;
    profile_json["pin_locked"] = profile.pin_locked;
    profile_json["gps_supported"] = profile.gps_supported;
    profile_json["max_carriers"] = profile.max_carriers;

    Json::Value bands(Json::arrayValue);
    for (const auto& band : profile.bands) {
        bands.append(band);
    }
    profile_json["bands"] = bands;

    report["event"] = added ? "profile_added" : "profile_removed";
    report["timestamp"] = static_cast<int64_t>(time(nullptr));
    report["profile"] = profile_json;
    report["mode"] = "basic";

    Json::StreamWriterBuilder builder;
    builder["indentation"] = "";
    std::unique_ptr<Json::StreamWriter> writer(builder.newStreamWriter());

    std::ostringstream oss;
    writer->write(report, &oss);

    std::cout << oss.str() << std::endl;

    // Call callback if set
    if (m_profile_callback) {
        m_profile_callback(profile, added);
    }
}

void QMIScanner::reportAdvancedProfile(const AdvancedDeviceProfile& profile, bool added) {
    Json::Value report;
    Json::Value profile_json;

    // Basic profile data
    Json::Value basic_json;
    basic_json["path"] = profile.basic.path;
    basic_json["imei"] = profile.basic.imei;
    basic_json["model"] = profile.basic.model;
    basic_json["firmware"] = profile.basic.firmware;
    basic_json["sim_present"] = profile.basic.sim_present;
    basic_json["pin_locked"] = profile.basic.pin_locked;
    basic_json["gps_supported"] = profile.basic.gps_supported;
    basic_json["max_carriers"] = profile.basic.max_carriers;

    Json::Value bands(Json::arrayValue);
    for (const auto& band : profile.basic.bands) {
        bands.append(band);
    }
    basic_json["bands"] = bands;

    profile_json["basic"] = basic_json;

    // Advanced profile data
    profile_json["manufacturer"] = profile.manufacturer;
    profile_json["msisdn"] = profile.msisdn;
    profile_json["power_state"] = profile.power_state;
    profile_json["hardware_revision"] = profile.hardware_revision;
    profile_json["operating_mode"] = profile.operating_mode;
    profile_json["prl_version"] = profile.prl_version;
    profile_json["activation_state"] = profile.activation_state;
    profile_json["user_lock_state"] = profile.user_lock_state;
    profile_json["band_capabilities"] = profile.band_capabilities;
    profile_json["factory_sku"] = profile.factory_sku;
    profile_json["software_version"] = profile.software_version;
    profile_json["iccid"] = profile.iccid;
    profile_json["imsi"] = profile.imsi;
    profile_json["uim_state"] = profile.uim_state;
    profile_json["pin_status"] = profile.pin_status;
    profile_json["time"] = profile.time;
    profile_json["firmware_preference"] = profile.firmware_preference;
    profile_json["boot_image_download_mode"] = profile.boot_image_download_mode;
    profile_json["usb_composition"] = profile.usb_composition;
    profile_json["mac_address_wlan"] = profile.mac_address_wlan;
    profile_json["mac_address_bt"] = profile.mac_address_bt;

    Json::Value stored_images(Json::arrayValue);
    for (const auto& image : profile.stored_images) {
        stored_images.append(image);
    }
    profile_json["stored_images"] = stored_images;

    report["event"] = added ? "advanced_profile_added" : "advanced_profile_removed";
    report["timestamp"] = static_cast<int64_t>(time(nullptr));
    report["profile"] = profile_json;
    report["mode"] = "advanced";

    Json::StreamWriterBuilder builder;
    builder["indentation"] = "";
    std::unique_ptr<Json::StreamWriter> writer(builder.newStreamWriter());

    std::ostringstream oss;
    writer->write(report, &oss);

    std::cout << oss.str() << std::endl;

    // Call callback if set
    if (m_advanced_callback) {
        m_advanced_callback(profile, added);
    }
}

void QMIScanner::setupUdev() {
    m_udev = udev_new();
    if (!m_udev) {
        std::cerr << "Failed to create udev context" << std::endl;
        return;
    }

    m_monitor = udev_monitor_new_from_netlink(m_udev, "udev");
    if (!m_monitor) {
        std::cerr << "Failed to create udev monitor" << std::endl;
        return;
    }

    // Filter for USB devices (QMI devices are typically USB)
    udev_monitor_filter_add_match_subsystem_devtype(m_monitor, "usb", NULL);
    udev_monitor_filter_add_match_subsystem_devtype(m_monitor, "usbmisc", NULL);

    udev_monitor_enable_receiving(m_monitor);
    m_monitor_fd = udev_monitor_get_fd(m_monitor);
}

void QMIScanner::cleanupUdev() {
    if (m_monitor) {
        udev_monitor_unref(m_monitor);
        m_monitor = nullptr;
    }

    if (m_udev) {
        udev_unref(m_udev);
        m_udev = nullptr;
    }
}

void QMIScanner::monitorLoop() {
    if (m_monitor_fd < 0) {
        return;
    }

    while (m_monitoring.load()) {
        fd_set fds;
        struct timeval tv;

        FD_ZERO(&fds);
        FD_SET(m_monitor_fd, &fds);
        tv.tv_sec = 1;
        tv.tv_usec = 0;

        int ret = select(m_monitor_fd + 1, &fds, NULL, NULL, &tv);

        if (ret > 0 && FD_ISSET(m_monitor_fd, &fds)) {
            struct udev_device* dev = udev_monitor_receive_device(m_monitor);
            if (dev) {
                const char* action = udev_device_get_action(dev);
                const char* devnode = udev_device_get_devnode(dev);

                if (devnode && strstr(devnode, "cdc-wdm")) {
                    if (strcmp(action, "add") == 0) {
                        // Device added - wait for device to be ready
                        usleep(500000);

                        // Add device to legacy device list with SIM status
                        QMIDevice device = queryDeviceInfo(devnode);
                        device.action = "added";
                        if (!device.device_path.empty()) {
                            std::lock_guard<std::mutex> lock(m_devices_mutex);
                            m_current_devices.push_back(device);
                            reportDevice(device, true);
                        }

                        if (m_profile_mode == ProfileMode::BASIC) {
                            DeviceProfile profile = queryDeviceProfile(devnode);
                            if (!profile.path.empty()) {
                                std::lock_guard<std::mutex> lock(m_devices_mutex);
                                m_current_profiles.push_back(profile);
                                reportProfile(profile, true);
                            }
                        } else if (m_profile_mode == ProfileMode::ADVANCED) {
                            AdvancedDeviceProfile profile = queryAdvancedDeviceProfile(devnode);
                            if (!profile.basic.path.empty()) {
                                std::lock_guard<std::mutex> lock(m_devices_mutex);
                                m_current_advanced_profiles.push_back(profile);
                                reportAdvancedProfile(profile, true);
                            }
                        } else if (m_profile_mode == ProfileMode::MANAGER) {
                            // Already handled above for legacy device list with SIM status
                        }
                    } else if (strcmp(action, "remove") == 0) {
                        // Device removed
                        std::lock_guard<std::mutex> lock(m_devices_mutex);

                        // Remove from devices list and report removal
                        for (auto it = m_current_devices.begin(); it != m_current_devices.end(); ++it) {
                            if (it->device_path == devnode) {
                                QMIDevice removed_device = *it;
                                removed_device.action = "removed";
                                reportDevice(removed_device, false);
                                m_current_devices.erase(it);
                                break;
                            }
                        }

                        if (m_profile_mode == ProfileMode::BASIC) {
                            for (auto it = m_current_profiles.begin(); it != m_current_profiles.end(); ++it) {
                                if (it->path == devnode) {
                                    reportProfile(*it, false);
                                    m_current_profiles.erase(it);
                                    break;
                                }
                            }
                        } else if (m_profile_mode == ProfileMode::ADVANCED) {
                            for (auto it = m_current_advanced_profiles.begin(); it != m_current_advanced_profiles.end(); ++it) {
                                if (it->basic.path == devnode) {
                                    reportAdvancedProfile(*it, false);
                                    m_current_advanced_profiles.erase(it);
                                    break;
                                }
                            }
                        } else if (m_profile_mode == ProfileMode::MANAGER) {
                            // Already handled above for devices list
                        }
                    }
                }

                udev_device_unref(dev);
            }
        }
    }
}

std::string QMIScanner::getCurrentDevicesAsJson(bool pretty) {
    std::lock_guard<std::mutex> lock(m_devices_mutex);
    Json::Value devices_json = m_json_agent->qmiDevicesArrayToJson(m_current_devices);
    return pretty ? m_json_agent->formatJsonPretty(devices_json) : m_json_agent->formatJsonCompact(devices_json);
}

std::string QMIScanner::getCurrentProfilesAsJson(bool pretty) {
    std::lock_guard<std::mutex> lock(m_devices_mutex);
    Json::Value profiles_json = m_json_agent->deviceProfilesArrayToJson(m_current_profiles);
    return pretty ? m_json_agent->formatJsonPretty(profiles_json) : m_json_agent->formatJsonCompact(profiles_json);
}

std::string QMIScanner::getCurrentAdvancedProfilesAsJson(bool pretty) {
    std::lock_guard<std::mutex> lock(m_devices_mutex);
    Json::Value profiles_json = m_json_agent->advancedDeviceProfilesArrayToJson(m_current_advanced_profiles);
    return pretty ? m_json_agent->formatJsonPretty(profiles_json) : m_json_agent->formatJsonCompact(profiles_json);
}

std::string QMIScanner::createScanReportJson(const std::string& scan_id) {
    std::lock_guard<std::mutex> lock(m_devices_mutex);
    std::string id = scan_id.empty() ? m_json_agent->generateScanId() : scan_id;
    Json::Value report = m_json_agent->createScanReport(id, m_json_agent->getCurrentTimestamp(),
                                                       m_current_profiles, m_current_advanced_profiles);
    return m_json_agent->formatJsonPretty(report);
}

bool QMIScanner::saveCurrentStateToFile(const std::string& filename) {
    Json::Value state;
    state["mode"] = (m_profile_mode == ProfileMode::BASIC) ? "basic" : "advanced";
    state["timestamp"] = m_json_agent->getCurrentTimestamp();

    std::lock_guard<std::mutex> lock(m_devices_mutex);
    state["devices"] = m_json_agent->qmiDevicesArrayToJson(m_current_devices);
    state["basic_profiles"] = m_json_agent->deviceProfilesArrayToJson(m_current_profiles);
    state["advanced_profiles"] = m_json_agent->advancedDeviceProfilesArrayToJson(m_current_advanced_profiles);

    return m_json_agent->saveJsonToFile(state, filename);
}

bool QMIScanner::loadStateFromFile(const std::string& filename) {
    Json::Value state = m_json_agent->loadJsonFromFile(filename);
    if (state.isNull()) {
        return false;
    }

    std::lock_guard<std::mutex> lock(m_devices_mutex);

    // Load devices
    if (state.isMember("devices")) {
        m_current_devices = m_json_agent->jsonToQMIDevicesArray(state["devices"]);
    }

    // Load basic profiles
    if (state.isMember("basic_profiles")) {
        m_current_profiles = m_json_agent->jsonToDeviceProfilesArray(state["basic_profiles"]);
    }

    // Load advanced profiles
    if (state.isMember("advanced_profiles")) {
        m_current_advanced_profiles = m_json_agent->jsonToAdvancedDeviceProfilesArray(state["advanced_profiles"]);
    }

    return true;
}

QMIDevScanAgent* QMIScanner::getJsonAgent() {
    return m_json_agent.get();
}
std::string QMIScanner::getRegistryJson(bool pretty) {
    auto& registry = QMIDeviceRegistry::getInstance();
    return pretty ? registry.getCurrentPretty() : registry.getCurrent();
}

int QMIScanner::getRegistryDeviceCount() {
    return QMIDeviceRegistry::getInstance().getDeviceCount();
}

bool QMIScanner::hasRegistryDevice(const std::string& device_path) {
    return QMIDeviceRegistry::getInstance().hasDevice(device_path);
}