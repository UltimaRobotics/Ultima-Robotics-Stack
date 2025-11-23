#include "DeviceVerifier.hpp"
#include "SerialPort.hpp"
#include "MAVLinkParser.hpp"
#include "DeviceStateDB.hpp"
#include "CallbackDispatcher.hpp"
#ifdef HTTP_ENABLED
#include "HTTPClient.hpp"
#endif
#include "Logger.hpp"
#include <chrono>
#include <thread>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <libudev.h>

// Use the json namespace from DeviceInfo.hpp which includes the correct header
using json = nlohmann::json;

DeviceVerifier::DeviceVerifier(const std::string& devicePath, const DeviceConfig& config, ThreadMgr::ThreadManager& threadManager)
    : devicePath_(devicePath), config_(config), running_(false), shouldStop_(false), 
      threadManager_(threadManager), threadId_(0), threadCreated_(false) {}

DeviceVerifier::~DeviceVerifier() {
    stop();
}

void DeviceVerifier::start() {
    if (running_) return;

    shouldStop_ = false;

    // Create thread using ThreadManager
    threadId_ = threadManager_.createThread([this]() {
        // Register thread from within the thread to avoid race condition
        try {
            threadManager_.registerThread(threadId_, devicePath_);
            LOG_DEBUG("Verification thread registered with attachment: " + devicePath_);
        } catch (const ThreadMgr::ThreadManagerException& e) {
            LOG_WARNING("Failed to register verification thread: " + std::string(e.what()));
            // Continue with verification even if registration fails
        } catch (...) {
            LOG_WARNING("Failed to register verification thread: unknown error");
            // Continue with verification even if registration fails
        }
        this->verifyThread();
    });

    threadCreated_ = true;
    running_ = true;

    LOG_INFO("Started verification thread " + std::to_string(threadId_) + " for: " + devicePath_);
}

void DeviceVerifier::stop() {
    if (!running_ || !threadCreated_) return;

    shouldStop_ = true;

    // Unregister thread FIRST to free up the attachment before the thread exits
    try {
        threadManager_.unregisterThread(devicePath_);
        LOG_DEBUG("Unregistered thread attachment: " + devicePath_);
    } catch (const ThreadMgr::ThreadManagerException& e) {
        LOG_DEBUG("Thread already unregistered: " + std::string(e.what()));
    } catch (...) {
        LOG_DEBUG("Thread unregistration failed for: " + devicePath_);
    }

    // Wait for thread to complete with timeout
    if (threadManager_.isThreadAlive(threadId_)) {
        bool completed = threadManager_.joinThread(threadId_, std::chrono::seconds(5));

        if (!completed) {
            LOG_WARNING("Thread " + std::to_string(threadId_) + " did not complete in time, stopping forcefully");
            try {
                threadManager_.stopThread(threadId_);
                threadManager_.joinThread(threadId_, std::chrono::seconds(2));
            } catch (const ThreadMgr::ThreadManagerException& e) {
                LOG_WARNING("Failed to stop thread forcefully: " + std::string(e.what()));
            }
        }
    }

    threadCreated_ = false;
    running_ = false;

    LOG_INFO("Stopped verification for: " + devicePath_);
}

bool DeviceVerifier::isRunning() const {
    return running_ && threadCreated_ && threadManager_.isThreadAlive(threadId_);
}

void DeviceVerifier::verifyThread() {
    LOG_INFO("Starting verification for: " + devicePath_);

    DeviceInfo info;
    info.devicePath = devicePath_;
    info.state = DeviceState::VERIFYING;

    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");
    info.timestamp = ss.str();

    DeviceStateDB::getInstance().updateDevice(devicePath_, info);

    bool verified = false;

    for (int baudrate : config_.baudrates) {
        if (shouldStop_) break;

        if (testBaudrate(baudrate, info)) {
            verified = true;
            break;
        }
    }

    if (verified) {
        info.state = DeviceState::VERIFIED;
        extractUSBInfo(info);
        LOG_INFO("Device VERIFIED: " + devicePath_ + " @ " + std::to_string(info.baudrate) + 
                 " baud, sysid=" + std::to_string(info.sysid) + 
                 ", compid=" + std::to_string(info.compid));
    } else {
        info.state = DeviceState::NON_MAVLINK;
        LOG_INFO("Device NON-MAVLINK: " + devicePath_);
    }

    DeviceStateDB::getInstance().updateDevice(devicePath_, info);
    CallbackDispatcher::getInstance().notify(info);

// Save verified device to runtime file
    if (verified && !config_.runtimeDeviceFile.empty()) {
        saveDeviceToRuntimeFile(info);
    }

#ifdef HTTP_ENABLED
    if (config_.enableHTTP && verified) {
        LOG_INFO("Sending device verification notification to endpoint");
        std::string deviceEndpoint = "http://" + config_.httpConfig.serverAddress + ":" + 
                                     std::to_string(config_.httpConfig.serverPort) + "/api/devices";
        HTTPClient client(deviceEndpoint, config_.httpConfig.timeoutMs);
        client.postDeviceInfo(info);

        // Send start request to MAVRouter mainloop
        LOG_INFO("Sending mainloop start request to MAVRouter");
        std::string startEndpoint = "http://" + config_.httpConfig.serverAddress + ":" + 
                                   std::to_string(config_.httpConfig.serverPort) + "/api/threads/mainloop/start";
        HTTPClient routerClient(startEndpoint, config_.httpConfig.timeoutMs);
        // Use the verified device info for the start command
        routerClient.postDeviceInfo(info);
    }
#else
    if (config_.enableHTTP && verified) {
        LOG_WARNING("HTTP notifications requested but HTTP support not compiled (rebuild with -DHTTP_ENABLED=ON)");
    }
#endif

    running_ = false;
}

bool DeviceVerifier::testBaudrate(int baudrate, DeviceInfo& info) {
    LOG_DEBUG("Testing " + devicePath_ + " @ " + std::to_string(baudrate) + " baud");

    SerialPort port(devicePath_);
    if (!port.open(baudrate)) {
        return false;
    }

    MAVLinkParser parser;
    std::vector<uint8_t> buffer(config_.maxPacketSize);

    auto startTime = std::chrono::steady_clock::now();
    bool foundMagic = false;

    while (!shouldStop_) {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - startTime).count();

        if (elapsed > config_.packetTimeoutMs) {
            break;
        }

        int bytesRead = port.read(buffer.data(), buffer.size(), config_.readTimeoutMs);
        if (bytesRead > 0) {
            for (int i = 0; i < bytesRead; i++) {
                if (parser.isMagicByte(buffer[i])) {
                    foundMagic = true;
                }
            }

            ParsedPacket packet = parser.parse(buffer.data(), bytesRead);
            if (packet.valid) {
                info.baudrate = baudrate;
                info.sysid = packet.sysid;
                info.compid = packet.compid;
                info.mavlinkVersion = packet.mavlinkVersion;

                MAVLinkMessage msg;
                msg.msgid = packet.msgid;
                msg.name = packet.messageName;

                bool found = false;
                for (const auto& m : info.messages) {
                    if (m.msgid == msg.msgid) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    info.messages.push_back(msg);
                }

                port.close();
                return true;
            }
        }
    }

    port.close();
    return false;
}

// Helper function to identify flight controller based on VID/PID
void identifyFlightController(USBDeviceInfo& usbInfo) {
    // Convert hex strings to integers for comparison
    int vid = 0, pid = 0;
    try {
        vid = std::stoi(usbInfo.vendorId, nullptr, 16);
        pid = std::stoi(usbInfo.productId, nullptr, 16);
    } catch (...) {
        return;
    }

    // PX4 FMU (Vendor: 3D Robotics / PX4)
    if (vid == 0x26AC) {
        usbInfo.boardClass = "Pixhawk";
        usbInfo.autopilotType = "PX4";
        
        switch (pid) {
            case 0x0010: usbInfo.boardName = "PX4 FMU V1"; break;
            case 0x0011: usbInfo.boardName = "PX4 FMU V2"; break;
            case 0x0012: usbInfo.boardName = "PX4 FMU V4"; break;
            case 0x0013: usbInfo.boardName = "PX4 FMU V4 PRO"; break;
            case 0x0030: usbInfo.boardName = "PX4 MindPX V2"; break;
            case 0x0032: usbInfo.boardName = "PX4 FMU V5"; break;
            case 0x0033: usbInfo.boardName = "PX4 FMU V5X"; break;
            case 0x0035: usbInfo.boardName = "PX4 FMU V6X"; break;
            case 0x0036: usbInfo.boardName = "PX4 FMU V6U"; break;
            case 0x0038: usbInfo.boardName = "PX4 FMU V6C"; break;
            case 0x001D: usbInfo.boardName = "PX4 FMU V6X-RT"; break;
            default: usbInfo.boardName = "PX4 FMU (Unknown Model)"; break;
        }
    }
    // ArduPilot ChibiOS
    else if (vid == 0x1209 && (pid == 0x5740 || pid == 0x5741)) {
        usbInfo.boardClass = "Pixhawk";
        usbInfo.autopilotType = "ArduPilot";
        usbInfo.boardName = "ArduPilot ChibiOS";
    }
    // CubePilot
    else if (vid == 0x2DAE) {
        usbInfo.boardClass = "Pixhawk";
        usbInfo.autopilotType = "PX4";
        
        switch (pid) {
            case 0x1011: usbInfo.boardName = "Cube Black"; break;
            case 0x1001: usbInfo.boardName = "Cube Black (Bootloader)"; break;
            case 0x1016: usbInfo.boardName = "Cube Orange"; break;
            case 0x1017: usbInfo.boardName = "Cube Orange 2"; break;
            case 0x1058: usbInfo.boardName = "Cube Orange Plus"; break;
            case 0x1012: usbInfo.boardName = "Cube Yellow"; break;
            case 0x1002: usbInfo.boardName = "Cube Yellow (Bootloader)"; break;
            case 0x1015: usbInfo.boardName = "Cube Purple"; break;
            case 0x1005: usbInfo.boardName = "Cube Purple (Bootloader)"; break;
            default: usbInfo.boardName = "CubePilot (Unknown Model)"; break;
        }
    }
    // Holybro
    else if (vid == 0x3162) {
        usbInfo.boardClass = "Pixhawk";
        usbInfo.autopilotType = "PX4";
        
        switch (pid) {
            case 0x0047: usbInfo.boardName = "Pixhawk 4"; break;
            case 0x0049: usbInfo.boardName = "Pixhawk 4 Mini"; break;
            case 0x004B: usbInfo.boardName = "Durandal"; break;
            default: usbInfo.boardName = "Holybro (Unknown Model)"; break;
        }
    }
    // CUAV
    else if (vid == 0x3163) {
        usbInfo.boardClass = "Pixhawk";
        usbInfo.autopilotType = "PX4";
        
        switch (pid) {
            case 0x004C: usbInfo.boardName = "CUAV Nora/X7 Pro"; break;
            default: usbInfo.boardName = "CUAV (Unknown Model)"; break;
        }
    }
    // U-blox GPS
    else if (vid == 0x1546) {
        usbInfo.boardClass = "RTK GPS";
        usbInfo.autopilotType = "GPS";
        
        switch (pid) {
            case 0x01A5: usbInfo.boardName = "U-blox 5"; break;
            case 0x01A6: usbInfo.boardName = "U-blox 6"; break;
            case 0x01A7: usbInfo.boardName = "U-blox 7"; break;
            case 0x01A8: usbInfo.boardName = "U-blox 8"; break;
            case 0x01A9: usbInfo.boardName = "U-blox 9"; break;
            default: usbInfo.boardName = "U-blox GPS"; break;
        }
    }
    // Generic fallback based on manufacturer
    else {
        usbInfo.autopilotType = "Generic";
        
        if (usbInfo.manufacturer.find("3D Robotics") != std::string::npos ||
            usbInfo.manufacturer.find("3DR") != std::string::npos) {
            usbInfo.boardClass = "Pixhawk";
            usbInfo.autopilotType = "PX4";
        } else if (usbInfo.manufacturer.find("ArduPilot") != std::string::npos) {
            usbInfo.boardClass = "Pixhawk";
            usbInfo.autopilotType = "ArduPilot";
        } else if (usbInfo.manufacturer.find("mRo") != std::string::npos) {
            usbInfo.boardClass = "Pixhawk";
        } else if (usbInfo.manufacturer.find("Holybro") != std::string::npos) {
            usbInfo.boardClass = "Pixhawk";
            usbInfo.autopilotType = "PX4";
        }
        
        // If still no boardName, use the device name
        if (usbInfo.boardName.empty()) {
            usbInfo.boardName = usbInfo.deviceName;
        }
    }
}

void DeviceVerifier::extractUSBInfo(DeviceInfo& info) {
    const int MAX_RETRIES = 3;
    const int RETRY_DELAY_MS = 200;
    
    for (int attempt = 0; attempt < MAX_RETRIES; attempt++) {
        if (attempt > 0) {
            LOG_INFO("Retrying USB info extraction, attempt " + std::to_string(attempt + 1) + "/" + std::to_string(MAX_RETRIES));
            std::this_thread::sleep_for(std::chrono::milliseconds(RETRY_DELAY_MS));
        }
        
        struct udev* udev = udev_new();
        if (!udev) {
            LOG_WARNING("Failed to create udev context for USB info extraction");
            continue;
        }

        struct udev_device* dev = udev_device_new_from_syspath(udev, devicePath_.c_str());
        if (!dev) {
            // Try to get device from subsystem
            struct udev_enumerate* enumerate = udev_enumerate_new(udev);
            udev_enumerate_add_match_subsystem(enumerate, "tty");
            udev_enumerate_scan_devices(enumerate);

            struct udev_list_entry* devices = udev_enumerate_get_list_entry(enumerate);
            struct udev_list_entry* entry;

            udev_list_entry_foreach(entry, devices) {
                const char* path = udev_list_entry_get_name(entry);
                struct udev_device* tmp_dev = udev_device_new_from_syspath(udev, path);
                
                if (tmp_dev) {
                    const char* devnode = udev_device_get_devnode(tmp_dev);
                    if (devnode && devicePath_ == devnode) {
                        dev = tmp_dev;
                        break;
                    }
                    udev_device_unref(tmp_dev);
                }
            }
            udev_enumerate_unref(enumerate);
        }

        if (dev) {
            // Get USB parent device
            struct udev_device* usb_dev = udev_device_get_parent_with_subsystem_devtype(dev, "usb", "usb_device");
            
            if (usb_dev) {
                const char* manufacturer = udev_device_get_sysattr_value(usb_dev, "manufacturer");
                const char* serial = udev_device_get_sysattr_value(usb_dev, "serial");
                const char* idVendor = udev_device_get_sysattr_value(usb_dev, "idVendor");
                const char* idProduct = udev_device_get_sysattr_value(usb_dev, "idProduct");
                const char* product = udev_device_get_sysattr_value(usb_dev, "product");
                
                // Get USB bus information
                const char* busnum = udev_device_get_sysattr_value(usb_dev, "busnum");
                const char* devnum = udev_device_get_sysattr_value(usb_dev, "devnum");

                if (manufacturer) info.usbInfo.manufacturer = manufacturer;
                if (serial) info.usbInfo.serialNumber = serial;
                if (idVendor) info.usbInfo.vendorId = idVendor;
                if (idProduct) info.usbInfo.productId = idProduct;
                if (product) info.usbInfo.deviceName = product;
                if (busnum) info.usbInfo.usbBusNumber = busnum;
                if (devnum) info.usbInfo.usbDeviceAddress = devnum;

                // Generate physical device ID if we have the required information
                if (!info.usbInfo.usbBusNumber.empty() && 
                    !info.usbInfo.vendorId.empty() && 
                    !info.usbInfo.productId.empty() && 
                    !info.usbInfo.serialNumber.empty()) {
                    info.usbInfo.physicalDeviceId = info.usbInfo.usbBusNumber + ":" + 
                                                   info.usbInfo.vendorId + ":" + 
                                                   info.usbInfo.productId + ":" + 
                                                   info.usbInfo.serialNumber;
                }

                // Check if all required fields are present
                bool dataComplete = !info.usbInfo.manufacturer.empty() &&
                                  !info.usbInfo.serialNumber.empty() &&
                                  !info.usbInfo.vendorId.empty() &&
                                  !info.usbInfo.productId.empty() &&
                                  !info.usbInfo.deviceName.empty();

                if (dataComplete) {
                    // Identify flight controller based on VID/PID
                    identifyFlightController(info.usbInfo);
                    
                    LOG_DEBUG("USB Info - Manufacturer: " + info.usbInfo.manufacturer + 
                             ", Serial: " + info.usbInfo.serialNumber +
                             ", VID: " + info.usbInfo.vendorId + 
                             ", PID: " + info.usbInfo.productId +
                             ", Bus: " + info.usbInfo.usbBusNumber +
                             ", DevAddr: " + info.usbInfo.usbDeviceAddress +
                             ", PhysicalID: " + info.usbInfo.physicalDeviceId +
                             ", Board: " + info.usbInfo.boardName +
                             ", Type: " + info.usbInfo.autopilotType);
                    
                    udev_device_unref(dev);
                    udev_unref(udev);
                    return;  // Success - all data collected
                } else {
                    LOG_WARNING("USB data incomplete on attempt " + std::to_string(attempt + 1) + 
                              " - Manufacturer: " + (info.usbInfo.manufacturer.empty() ? "MISSING" : "OK") +
                              ", Serial: " + (info.usbInfo.serialNumber.empty() ? "MISSING" : "OK") +
                              ", VID: " + (info.usbInfo.vendorId.empty() ? "MISSING" : "OK") +
                              ", PID: " + (info.usbInfo.productId.empty() ? "MISSING" : "OK") +
                              ", DeviceName: " + (info.usbInfo.deviceName.empty() ? "MISSING" : "OK") +
                              ", Bus: " + (info.usbInfo.usbBusNumber.empty() ? "MISSING" : "OK") +
                              ", DevAddr: " + (info.usbInfo.usbDeviceAddress.empty() ? "MISSING" : "OK"));
                    
                    // Clear partial data for retry
                    info.usbInfo.manufacturer.clear();
                    info.usbInfo.serialNumber.clear();
                    info.usbInfo.vendorId.clear();
                    info.usbInfo.productId.clear();
                    info.usbInfo.deviceName.clear();
                    info.usbInfo.usbBusNumber.clear();
                    info.usbInfo.usbDeviceAddress.clear();
                    info.usbInfo.physicalDeviceId.clear();
                }
            }

            udev_device_unref(dev);
        }

        udev_unref(udev);
    }
    
    LOG_ERROR("Failed to extract complete USB info after " + std::to_string(MAX_RETRIES) + " attempts");
}

void DeviceVerifier::saveDeviceToRuntimeFile(const DeviceInfo& info) {
    json deviceJson;
    
    // USB Hardware Information
    deviceJson["deviceName"] = info.usbInfo.deviceName;
    deviceJson["manufacturer"] = info.usbInfo.manufacturer;
    deviceJson["serialNumber"] = info.usbInfo.serialNumber;
    deviceJson["vendorId"] = info.usbInfo.vendorId;
    deviceJson["productId"] = info.usbInfo.productId;
    
    // USB Bus Tracking Information
    deviceJson["usbBusNumber"] = info.usbInfo.usbBusNumber;
    deviceJson["usbDeviceAddress"] = info.usbInfo.usbDeviceAddress;
    deviceJson["physicalDeviceId"] = info.usbInfo.physicalDeviceId;
    
    // Flight Controller Identification
    deviceJson["boardClass"] = info.usbInfo.boardClass;
    deviceJson["boardName"] = info.usbInfo.boardName;
    deviceJson["autopilotType"] = info.usbInfo.autopilotType;
    
    // Connection Information
    deviceJson["devicePath"] = info.devicePath;
    deviceJson["baudrate"] = info.baudrate;
    
    // MAVLink Information
    deviceJson["systemId"] = info.sysid;
    deviceJson["componentId"] = info.compid;
    deviceJson["mavlinkVersion"] = info.mavlinkVersion;
    
    // Metadata
    deviceJson["timestamp"] = info.timestamp;

    // Print USB device info JSON to terminal
    LOG_INFO("USB Device Info JSON: " + deviceJson.dump(2));

    // Write new data to file, replacing old contents
    std::ofstream outFile(config_.runtimeDeviceFile, std::ios::trunc);
    if (outFile.is_open()) {
        outFile << deviceJson.dump(2) << std::endl;
        outFile.close();
        LOG_INFO("Device saved to runtime file (replaced old contents): " + config_.runtimeDeviceFile);
    } else {
        LOG_ERROR("Failed to write to runtime device file: " + config_.runtimeDeviceFile);
    }
}