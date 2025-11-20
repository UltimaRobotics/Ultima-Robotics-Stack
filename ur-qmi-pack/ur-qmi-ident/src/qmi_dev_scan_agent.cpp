
#include "qmi_dev_scan_agent.h"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <random>

QMIDevScanAgent::QMIDevScanAgent() : m_last_error{false, "", -1, -1} {
    // Configure pretty writer
    m_pretty_writer_builder = std::make_unique<Json::StreamWriterBuilder>();
    m_pretty_writer_builder->settings_["indentation"] = "  ";
    m_pretty_writer_builder->settings_["precision"] = 6;
    
    // Configure compact writer
    m_compact_writer_builder = std::make_unique<Json::StreamWriterBuilder>();
    m_compact_writer_builder->settings_["indentation"] = "";
    
    // Configure reader
    m_reader_builder = std::make_unique<Json::CharReaderBuilder>();
    m_reader_builder->settings_["allowComments"] = true;
    m_reader_builder->settings_["strictRoot"] = false;
}

QMIDevScanAgent::~QMIDevScanAgent() = default;

std::string QMIDevScanAgent::formatJsonPretty(const Json::Value& json) {
    clearLastError();
    try {
        std::unique_ptr<Json::StreamWriter> writer(m_pretty_writer_builder->newStreamWriter());
        std::ostringstream oss;
        writer->write(json, &oss);
        return oss.str();
    } catch (const std::exception& e) {
        setError("Failed to format JSON: " + std::string(e.what()));
        return "";
    }
}

std::string QMIDevScanAgent::formatJsonCompact(const Json::Value& json) {
    clearLastError();
    try {
        std::unique_ptr<Json::StreamWriter> writer(m_compact_writer_builder->newStreamWriter());
        std::ostringstream oss;
        writer->write(json, &oss);
        return oss.str();
    } catch (const std::exception& e) {
        setError("Failed to format JSON: " + std::string(e.what()));
        return "";
    }
}

bool QMIDevScanAgent::validateJsonString(const std::string& json_str) {
    clearLastError();
    Json::Value root;
    std::unique_ptr<Json::CharReader> reader(m_reader_builder->newCharReader());
    std::string errors;
    
    bool success = reader->parse(json_str.c_str(), json_str.c_str() + json_str.length(), &root, &errors);
    if (!success) {
        setError("JSON validation failed: " + errors);
    }
    return success;
}

Json::Value QMIDevScanAgent::parseJsonString(const std::string& json_str) {
    clearLastError();
    Json::Value root;
    std::unique_ptr<Json::CharReader> reader(m_reader_builder->newCharReader());
    std::string errors;
    
    bool success = reader->parse(json_str.c_str(), json_str.c_str() + json_str.length(), &root, &errors);
    if (!success) {
        setError("JSON parsing failed: " + errors);
        return Json::Value();
    }
    return root;
}

Json::Value QMIDevScanAgent::deviceProfileToJson(const DeviceProfile& profile) {
    Json::Value json;
    json["path"] = profile.path;
    json["imei"] = profile.imei;
    json["model"] = profile.model;
    json["firmware"] = profile.firmware;
    json["bands"] = stringVectorToJsonArray(profile.bands);
    json["sim_present"] = profile.sim_present;
    json["pin_locked"] = profile.pin_locked;
    json["gps_supported"] = profile.gps_supported;
    json["max_carriers"] = static_cast<int>(profile.max_carriers);
    return json;
}

Json::Value QMIDevScanAgent::advancedDeviceProfileToJson(const AdvancedDeviceProfile& profile) {
    Json::Value json;
    json["basic"] = deviceProfileToJson(profile.basic);
    json["manufacturer"] = profile.manufacturer;
    json["msisdn"] = profile.msisdn;
    json["power_state"] = profile.power_state;
    json["hardware_revision"] = profile.hardware_revision;
    json["operating_mode"] = profile.operating_mode;
    json["prl_version"] = profile.prl_version;
    json["activation_state"] = profile.activation_state;
    json["user_lock_state"] = profile.user_lock_state;
    json["band_capabilities"] = profile.band_capabilities;
    json["factory_sku"] = profile.factory_sku;
    json["software_version"] = profile.software_version;
    json["iccid"] = profile.iccid;
    json["imsi"] = profile.imsi;
    json["uim_state"] = profile.uim_state;
    json["pin_status"] = profile.pin_status;
    json["time"] = profile.time;
    json["stored_images"] = stringVectorToJsonArray(profile.stored_images);
    json["firmware_preference"] = profile.firmware_preference;
    json["boot_image_download_mode"] = profile.boot_image_download_mode;
    json["usb_composition"] = profile.usb_composition;
    json["mac_address_wlan"] = profile.mac_address_wlan;
    json["mac_address_bt"] = profile.mac_address_bt;
    return json;
}

Json::Value QMIDevScanAgent::qmiDeviceToJson(const QMIDevice& device) {
    Json::Value json;
    json["device_path"] = device.device_path;
    json["imei"] = device.imei;
    json["model"] = device.model;
    json["manufacturer"] = device.manufacturer;
    json["firmware_version"] = device.firmware_version;
    json["supported_bands"] = stringVectorToJsonArray(device.supported_bands);
    json["is_available"] = device.is_available;
    json["action"] = device.action;
    json["sim-status"] = simStatusToJson(device.sim_status);
    return json;
}

Json::Value QMIDevScanAgent::simStatusToJson(const SIMStatus& sim_status) {
    Json::Value json;
    json["card_state"] = sim_status.card_state;
    json["upin_state"] = sim_status.upin_state;
    json["upin_retries"] = sim_status.upin_retries;
    json["upuk_retries"] = sim_status.upuk_retries;
    json["application_type"] = sim_status.application_type;
    json["application_state"] = sim_status.application_state;
    json["application_id"] = sim_status.application_id;
    json["personalization_state"] = sim_status.personalization_state;
    json["upin_replaces_pin1"] = sim_status.upin_replaces_pin1;
    json["pin1_state"] = sim_status.pin1_state;
    json["pin1_retries"] = sim_status.pin1_retries;
    json["puk1_retries"] = sim_status.puk1_retries;
    json["pin2_state"] = sim_status.pin2_state;
    json["pin2_retries"] = sim_status.pin2_retries;
    json["puk2_retries"] = sim_status.puk2_retries;
    json["primary_gw_slot"] = sim_status.primary_gw_slot;
    json["primary_gw_application"] = sim_status.primary_gw_application;
    json["primary_1x_status"] = sim_status.primary_1x_status;
    json["secondary_gw_status"] = sim_status.secondary_gw_status;
    json["secondary_1x_status"] = sim_status.secondary_1x_status;
    return json;
}

bool QMIDevScanAgent::jsonToDeviceProfile(const Json::Value& json, DeviceProfile& profile) {
    clearLastError();
    try {
        profile.path = json.get("path", "").asString();
        profile.imei = json.get("imei", "").asString();
        profile.model = json.get("model", "").asString();
        profile.firmware = json.get("firmware", "").asString();
        profile.bands = jsonArrayToStringVector(json["bands"]);
        profile.sim_present = json.get("sim_present", false).asBool();
        profile.pin_locked = json.get("pin_locked", false).asBool();
        profile.gps_supported = json.get("gps_supported", false).asBool();
        profile.max_carriers = static_cast<uint8_t>(json.get("max_carriers", 1).asInt());
        return true;
    } catch (const std::exception& e) {
        setError("Failed to convert JSON to DeviceProfile: " + std::string(e.what()));
        return false;
    }
}

bool QMIDevScanAgent::jsonToAdvancedDeviceProfile(const Json::Value& json, AdvancedDeviceProfile& profile) {
    clearLastError();
    try {
        if (!jsonToDeviceProfile(json["basic"], profile.basic)) {
            return false;
        }
        
        profile.manufacturer = json.get("manufacturer", "").asString();
        profile.msisdn = json.get("msisdn", "").asString();
        profile.power_state = json.get("power_state", "").asString();
        profile.hardware_revision = json.get("hardware_revision", "").asString();
        profile.operating_mode = json.get("operating_mode", "").asString();
        profile.prl_version = json.get("prl_version", "").asString();
        profile.activation_state = json.get("activation_state", "").asString();
        profile.user_lock_state = json.get("user_lock_state", "").asString();
        profile.band_capabilities = json.get("band_capabilities", "").asString();
        profile.factory_sku = json.get("factory_sku", "").asString();
        profile.software_version = json.get("software_version", "").asString();
        profile.iccid = json.get("iccid", "").asString();
        profile.imsi = json.get("imsi", "").asString();
        profile.uim_state = json.get("uim_state", "").asString();
        profile.pin_status = json.get("pin_status", "").asString();
        profile.time = json.get("time", "").asString();
        profile.stored_images = jsonArrayToStringVector(json["stored_images"]);
        profile.firmware_preference = json.get("firmware_preference", "").asString();
        profile.boot_image_download_mode = json.get("boot_image_download_mode", "").asString();
        profile.usb_composition = json.get("usb_composition", "").asString();
        profile.mac_address_wlan = json.get("mac_address_wlan", "").asString();
        profile.mac_address_bt = json.get("mac_address_bt", "").asString();
        return true;
    } catch (const std::exception& e) {
        setError("Failed to convert JSON to AdvancedDeviceProfile: " + std::string(e.what()));
        return false;
    }
}

bool QMIDevScanAgent::jsonToQMIDevice(const Json::Value& json, QMIDevice& device) {
    clearLastError();
    try {
        device.device_path = json.get("device_path", "").asString();
        device.imei = json.get("imei", "").asString();
        device.model = json.get("model", "").asString();
        device.manufacturer = json.get("manufacturer", "").asString();
        device.firmware_version = json.get("firmware_version", "").asString();
        device.supported_bands = jsonArrayToStringVector(json["supported_bands"]);
        device.is_available = json.get("is_available", true).asBool();
        device.action = json.get("action", "added").asString();
        
        if (json.isMember("sim-status")) {
            jsonToSIMStatus(json["sim-status"], device.sim_status);
        }
        
        return true;
    } catch (const std::exception& e) {
        setError("Failed to convert JSON to QMIDevice: " + std::string(e.what()));
        return false;
    }
}

bool QMIDevScanAgent::jsonToSIMStatus(const Json::Value& json, SIMStatus& sim_status) {
    clearLastError();
    try {
        sim_status.card_state = json.get("card_state", "").asString();
        sim_status.upin_state = json.get("upin_state", "").asString();
        sim_status.upin_retries = json.get("upin_retries", 0).asInt();
        sim_status.upuk_retries = json.get("upuk_retries", 0).asInt();
        sim_status.application_type = json.get("application_type", "").asString();
        sim_status.application_state = json.get("application_state", "").asString();
        sim_status.application_id = json.get("application_id", "").asString();
        sim_status.personalization_state = json.get("personalization_state", "").asString();
        sim_status.upin_replaces_pin1 = json.get("upin_replaces_pin1", false).asBool();
        sim_status.pin1_state = json.get("pin1_state", "").asString();
        sim_status.pin1_retries = json.get("pin1_retries", 0).asInt();
        sim_status.puk1_retries = json.get("puk1_retries", 0).asInt();
        sim_status.pin2_state = json.get("pin2_state", "").asString();
        sim_status.pin2_retries = json.get("pin2_retries", 0).asInt();
        sim_status.puk2_retries = json.get("puk2_retries", 0).asInt();
        sim_status.primary_gw_slot = json.get("primary_gw_slot", "").asString();
        sim_status.primary_gw_application = json.get("primary_gw_application", "").asString();
        sim_status.primary_1x_status = json.get("primary_1x_status", "").asString();
        sim_status.secondary_gw_status = json.get("secondary_gw_status", "").asString();
        sim_status.secondary_1x_status = json.get("secondary_1x_status", "").asString();
        return true;
    } catch (const std::exception& e) {
        setError("Failed to convert JSON to SIMStatus: " + std::string(e.what()));
        return false;
    }
}

Json::Value QMIDevScanAgent::deviceProfilesArrayToJson(const std::vector<DeviceProfile>& profiles) {
    Json::Value array(Json::arrayValue);
    for (const auto& profile : profiles) {
        array.append(deviceProfileToJson(profile));
    }
    return array;
}

Json::Value QMIDevScanAgent::advancedDeviceProfilesArrayToJson(const std::vector<AdvancedDeviceProfile>& profiles) {
    Json::Value array(Json::arrayValue);
    for (const auto& profile : profiles) {
        array.append(advancedDeviceProfileToJson(profile));
    }
    return array;
}

Json::Value QMIDevScanAgent::qmiDevicesArrayToJson(const std::vector<QMIDevice>& devices) {
    Json::Value array(Json::arrayValue);
    for (const auto& device : devices) {
        array.append(qmiDeviceToJson(device));
    }
    return array;
}

std::vector<DeviceProfile> QMIDevScanAgent::jsonToDeviceProfilesArray(const Json::Value& json_array) {
    std::vector<DeviceProfile> profiles;
    if (!json_array.isArray()) {
        setError("JSON value is not an array");
        return profiles;
    }
    
    for (const auto& json_item : json_array) {
        DeviceProfile profile;
        if (jsonToDeviceProfile(json_item, profile)) {
            profiles.push_back(profile);
        }
    }
    return profiles;
}

std::vector<AdvancedDeviceProfile> QMIDevScanAgent::jsonToAdvancedDeviceProfilesArray(const Json::Value& json_array) {
    std::vector<AdvancedDeviceProfile> profiles;
    if (!json_array.isArray()) {
        setError("JSON value is not an array");
        return profiles;
    }
    
    for (const auto& json_item : json_array) {
        AdvancedDeviceProfile profile;
        if (jsonToAdvancedDeviceProfile(json_item, profile)) {
            profiles.push_back(profile);
        }
    }
    return profiles;
}

std::vector<QMIDevice> QMIDevScanAgent::jsonToQMIDevicesArray(const Json::Value& json_array) {
    std::vector<QMIDevice> devices;
    if (!json_array.isArray()) {
        setError("JSON value is not an array");
        return devices;
    }
    
    for (const auto& json_item : json_array) {
        QMIDevice device;
        if (jsonToQMIDevice(json_item, device)) {
            devices.push_back(device);
        }
    }
    return devices;
}

Json::Value QMIDevScanAgent::createDeviceEvent(const std::string& event_type, const DeviceProfile& profile) {
    Json::Value event;
    event["event"] = event_type;
    event["timestamp"] = getCurrentTimestamp();
    event["profile"] = deviceProfileToJson(profile);
    event["mode"] = "basic";
    return event;
}

Json::Value QMIDevScanAgent::createAdvancedDeviceEvent(const std::string& event_type, const AdvancedDeviceProfile& profile) {
    Json::Value event;
    event["event"] = event_type;
    event["timestamp"] = getCurrentTimestamp();
    event["profile"] = advancedDeviceProfileToJson(profile);
    event["mode"] = "advanced";
    return event;
}

Json::Value QMIDevScanAgent::createQMIDeviceEvent(const std::string& event_type, const QMIDevice& device) {
    Json::Value event;
    event["event"] = event_type;
    event["timestamp"] = getCurrentTimestamp();
    event["device"] = qmiDeviceToJson(device);
    event["mode"] = "legacy";
    return event;
}

Json::Value QMIDevScanAgent::createScanConfiguration(ProfileMode mode, const std::vector<std::string>& options) {
    Json::Value config;
    config["mode"] = (mode == ProfileMode::BASIC) ? "basic" : "advanced";
    config["options"] = stringVectorToJsonArray(options);
    config["scan_id"] = generateScanId();
    config["timestamp"] = getCurrentTimestamp();
    return config;
}

Json::Value QMIDevScanAgent::createScanReport(const std::string& scan_id, int64_t timestamp,
                                             const std::vector<DeviceProfile>& basic_profiles,
                                             const std::vector<AdvancedDeviceProfile>& advanced_profiles) {
    Json::Value report;
    report["scan_id"] = scan_id;
    report["timestamp"] = timestamp;
    report["basic_profiles_count"] = static_cast<int>(basic_profiles.size());
    report["advanced_profiles_count"] = static_cast<int>(advanced_profiles.size());
    report["basic_profiles"] = deviceProfilesArrayToJson(basic_profiles);
    report["advanced_profiles"] = advancedDeviceProfilesArrayToJson(advanced_profiles);
    return report;
}

std::string QMIDevScanAgent::generateScanId() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(1000, 9999);
    
    std::ostringstream oss;
    oss << "scan_" << std::put_time(std::localtime(&time_t), "%Y%m%d_%H%M%S") << "_" << dis(gen);
    return oss.str();
}

int64_t QMIDevScanAgent::getCurrentTimestamp() {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

bool QMIDevScanAgent::saveJsonToFile(const Json::Value& json, const std::string& filename) {
    clearLastError();
    try {
        std::ofstream file(filename);
        if (!file.is_open()) {
            setError("Failed to open file for writing: " + filename);
            return false;
        }
        file << formatJsonPretty(json);
        return true;
    } catch (const std::exception& e) {
        setError("Failed to save JSON to file: " + std::string(e.what()));
        return false;
    }
}

Json::Value QMIDevScanAgent::loadJsonFromFile(const std::string& filename) {
    clearLastError();
    try {
        std::ifstream file(filename);
        if (!file.is_open()) {
            setError("Failed to open file for reading: " + filename);
            return Json::Value();
        }
        
        std::ostringstream oss;
        oss << file.rdbuf();
        return parseJsonString(oss.str());
    } catch (const std::exception& e) {
        setError("Failed to load JSON from file: " + std::string(e.what()));
        return Json::Value();
    }
}

QMIDevScanAgent::JsonError QMIDevScanAgent::getLastError() const {
    return m_last_error;
}

void QMIDevScanAgent::clearLastError() {
    m_last_error = {false, "", -1, -1};
}

void QMIDevScanAgent::setError(const std::string& message, int line, int column) {
    m_last_error = {true, message, line, column};
}

Json::Value QMIDevScanAgent::stringVectorToJsonArray(const std::vector<std::string>& vec) {
    Json::Value array(Json::arrayValue);
    for (const auto& str : vec) {
        array.append(str);
    }
    return array;
}

std::vector<std::string> QMIDevScanAgent::jsonArrayToStringVector(const Json::Value& json_array) {
    std::vector<std::string> vec;
    if (!json_array.isArray()) {
        return vec;
    }
    
    for (const auto& item : json_array) {
        vec.push_back(item.asString());
    }
    return vec;
}
