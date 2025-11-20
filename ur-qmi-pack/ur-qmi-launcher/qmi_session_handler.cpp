#include "qmi_session_handler.h"
#include "command_logger.h"
#include <iostream>
#include <sstream>
#include <regex>
#include <cstdlib>
#include <thread>
#include <chrono>
#include <algorithm> // Required for std::remove

// Define a placeholder for QMIConnectionSettings if not already defined
// Ensure this matches the definition in "qmi_session_handler.h"
struct QMIConnectionSettings {
    std::string interface_name = "";
    std::string ip_address = "";
    std::string subnet_mask = "";
    std::string gateway = "";
    std::string dns_primary = "";
    std::string dns_secondary = "";
    int mtu = 0;
};


QMISessionHandler::QMISessionHandler(const std::string& device_path, const std::string& interface_name)
    : m_device_path(device_path), m_session_active(false), 
      m_connection_timeout(15), m_max_retries(3), m_interface_auto_detected(false),
      m_client_id(0), m_connection_id(0), m_packet_data_handle("") {
    m_session_info.connection_id = 0;
    m_session_info.is_active = false;
    m_session_info.retry_count = 0;

    // Generate unique device instance id from device path
    std::string dev_name = device_path.substr(device_path.find_last_of('/') + 1);
    m_device_instance_id = dev_name;

    // Set interface name or auto-detect
    if (!interface_name.empty()) {
        m_interface_name = interface_name;
    } else {
        m_interface_name = autoDetectInterfaceName();
        m_interface_auto_detected = true;
    }

    std::cout << "QMI Session Handler initialized for device: " << m_device_path 
              << ", interface: " << m_interface_name 
              << " (auto-detected: " << (m_interface_auto_detected ? "yes" : "no") << ")" << std::endl;
}

QMISessionHandler::~QMISessionHandler() {
    if (m_session_active) {
        stopDataSession();
    }
    // Release client if it was opened
    if (m_client_id != 0) {
        releaseClient();
    }
}

bool QMISessionHandler::initialize() {
    std::lock_guard<std::mutex> lock(m_session_mutex);
    std::cout << "Initializing QMI session handler for device: " << m_device_path << std::endl;

    // Stop ModemManager to prevent conflicts (following script pattern)
    if (!stopModemManager()) {
        std::cout << "Warning: Could not stop ModemManager" << std::endl;
    }

    // Initialize modem following enhanced script pattern
    if (!initializeModem()) {
        std::cerr << "Failed to initialize modem" << std::endl;
        return false;
    }

    // Skip client opening in initialize - it will be done when needed
    std::cout << "Modem initialization completed" << std::endl;

    std::cout << "QMI session handler initialized successfully" << std::endl;
    return true;
}

// Enhanced modem initialization following script pattern
bool QMISessionHandler::initializeModem() {
    std::lock_guard<std::mutex> dms_lock(m_dms_mutex);
    std::cout << "Initializing modem..." << std::endl;

    // Set modem to online mode (following script: qmicli --dms-set-operating-mode='online')
    std::ostringstream cmd;
    cmd << "qmicli -d " << m_device_path << " --dms-set-operating-mode='online'";
    std::string output = executeQMICommand(cmd.str());

    if (output.find("error") != std::string::npos) {
        std::cerr << "Failed to set modem online: " << output << std::endl;
        return false;
    }

    std::cout << "Modem set to online mode" << std::endl;
    return true;
}

bool QMISessionHandler::resetWDSService() {
    std::lock_guard<std::mutex> wds_lock(m_wds_mutex);
    std::cout << "Resetting WDS service..." << std::endl;

    // Reset WDS service (following script: qmicli --wds-reset)
    std::ostringstream cmd;
    cmd << "qmicli -d " << m_device_path << " --wds-reset";
    std::string output = executeQMICommand(cmd.str());

    if (output.find("successfully reset") != std::string::npos) {
        std::cout << "WDS service reset successfully" << std::endl;
        return true;
    } else {
        std::cout << "WDS reset failed, continuing..." << std::endl;
        return false; // Not critical, continue anyway
    }
}

bool QMISessionHandler::prepareInterface(const std::string& interface_name) {
    std::string target_interface = interface_name.empty() ? m_interface_name : interface_name;

    // Update interface name if provided
    if (!interface_name.empty() && interface_name != m_interface_name) {
        std::cout << "Updating interface from " << m_interface_name << " to " << interface_name << std::endl;
        m_interface_name = interface_name;
        m_interface_auto_detected = false;
    }

    std::cout << "Preparing interface: " << target_interface << " for device: " << m_device_path << std::endl;

    // Check if interface exists, if auto-detected try to find the right one
    if (m_interface_auto_detected && !isInterfaceAvailable(target_interface)) {
        std::cout << "Auto-detected interface " << target_interface << " not available, re-detecting..." << std::endl;
        std::string new_interface = autoDetectInterfaceName();
        if (!new_interface.empty() && new_interface != target_interface) {
            target_interface = new_interface;
            m_interface_name = new_interface;
            std::cout << "Re-detected interface: " << target_interface << std::endl;
        }
    }

    // Bring interface down then up (following script pattern)
    if (!bringInterfaceDown(target_interface)) {
        std::cout << "Warning: Could not bring interface " << target_interface << " down" << std::endl;
    }

    if (!bringInterfaceUp(target_interface)) {
        std::cerr << "Failed to prepare interface " << target_interface << std::endl;
        return false;
    }

    std::cout << "Interface " << target_interface << " is ready for device " << m_device_path << std::endl;
    return true;
}

bool QMISessionHandler::isModemReady() {
    {
        std::lock_guard<std::mutex> dms_lock(m_dms_mutex);
        std::ostringstream cmd;
        cmd << "qmicli -d " << m_device_path << " --dms-get-operating-mode";
        std::string output = executeCommand(cmd.str());

        if (output.find("error") != std::string::npos || 
            output.find("endpoint hangup") != std::string::npos ||
            output.find("CID allocation failed") != std::string::npos) {
            
            std::cout << "Primary modem check failed, attempting recovery..." << std::endl;
            
            // Try to recover the DMS service
            executeCommand("qmicli -d " + m_device_path + " --dms-noop >/dev/null 2>&1");
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            
            // Retry with fresh command
            output = executeCommand(cmd.str());
            
            if (output.find("error") == std::string::npos) {
                bool is_online = output.find("online") != std::string::npos;
                std::cout << "Modem readiness check: " << (is_online ? "READY" : "NOT READY") << std::endl;
                return is_online;
            }
            
            std::cout << "Modem readiness check failed after recovery: " << output << std::endl;
        } else {
            bool is_online = output.find("online") != std::string::npos;
            std::cout << "Modem readiness check: " << (is_online ? "READY" : "NOT READY") << std::endl;
            return is_online;
        }
    } // Release DMS mutex here

    // Try setting modem online as last resort (this will acquire its own mutex)
    std::cout << "Attempting to set modem online..." << std::endl;
    setModemOnline();
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    // Final check with new mutex
    {
        std::lock_guard<std::mutex> dms_lock(m_dms_mutex);
        std::ostringstream cmd;
        cmd << "qmicli -d " << m_device_path << " --dms-get-operating-mode";
        std::string output = executeCommand(cmd.str());
        
        bool is_online = output.find("online") != std::string::npos && output.find("error") == std::string::npos;
        std::cout << "Final modem readiness check: " << (is_online ? "READY" : "NOT READY") << std::endl;
        return is_online;
    }
}

bool QMISessionHandler::startDataSession(const std::string& apn, int ip_type,
                                        const std::string& username,
                                        const std::string& password,
                                        const std::string& auth_type,
                                        const std::string& interface_name) {
    std::lock_guard<std::mutex> lock(m_session_mutex);

    if (m_session_active) {
        std::cout << "Session already active, stopping current session first" << std::endl;
        if (!stopDataSession()) {
            std::cout << "Failed to stop current session" << std::endl;
            return false;
        }
    }

    // Store session parameters
    m_session_info.apn = apn;
    m_session_info.ip_type = ip_type;
    m_session_info.auth_type = auth_type;

    // Update interface if provided
    if (!interface_name.empty()) {
        m_interface_name = interface_name;
        m_interface_auto_detected = false;
    }

    // Reset WDS service before connection
    resetWDSService();

    // Prepare interface (will use provided interface_name or current m_interface_name)
    prepareInterface(interface_name);

    // Diagnose connection prerequisites
    if (!diagnoseConnectionPrerequisites()) {
        std::cout << "Connection prerequisites check failed, continuing anyway..." << std::endl;
    }

    // Use the enhanced connection method with retries
    return connectWithRetries(apn, ip_type, username, password, auth_type, m_max_retries);
}

// Enhanced connection with retries following script pattern
bool QMISessionHandler::connectWithRetries(const std::string& apn, int ip_type, 
                                          const std::string& username, 
                                          const std::string& password,
                                          const std::string& auth_type,
                                          int max_retries) {
    std::cout << "Starting connection with retries..." << std::endl;
    std::cout << "APN: " << apn << ", IP Type: " << ip_type << ", Auth: " << auth_type << std::endl;

    int attempt = 0;
    std::string last_error = "";

    while (attempt < max_retries) {
        attempt++;
        std::cout << "Connection attempt " << attempt << "/" << max_retries << std::endl;

        // Build QMI command following script pattern:
        // qmicli -d '$DEVICE' --device-open-net='net-raw-ip|net-no-qos-header' --wds-start-network="apn='$APN',ip-type=$IPTYPE,auth=$AUTH" --client-no-release-cid
        std::ostringstream qmi_cmd;
        qmi_cmd << "qmicli -d '" << m_device_path << "' "
                << "--device-open-net='net-raw-ip|net-no-qos-header' "
                << "--wds-start-network=\"apn='" << apn << "',ip-type=" << ip_type;

        if (!username.empty() && !password.empty()) {
            qmi_cmd << ",username='" << username << "',password='" << password << "'";
        }

        if (auth_type != "none") {
            qmi_cmd << ",auth=" << auth_type;
        }

        qmi_cmd << "\" --client-no-release-cid";

        std::cout << "Executing QMI command: " << qmi_cmd.str() << std::endl;

        // Execute command with timeout (WDS operation)
        std::string qmi_output;
        {
            std::lock_guard<std::mutex> wds_lock(m_wds_mutex);
            qmi_output = executeCommandWithTimeout(qmi_cmd.str(), m_connection_timeout);
        }

        logConnectionAttempt(attempt, qmi_output);

        // Check for successful connection (following script pattern)
        if (qmi_output.find("Network started") != std::string::npos) {
            std::cout << "QMI network session started successfully" << std::endl;

            // Extract connection identifiers
            uint32_t cid = extractConnectionId(qmi_output);
            std::string pdh = extractPacketDataHandle(qmi_output);

            if (cid > 0) {
                m_session_info.connection_id = cid;
                std::cout << "Connected with CID " << cid << std::endl;
                if (!pdh.empty()) {
                    m_session_info.packet_data_handle = pdh;
                    std::cout << "Packet Data Handle: " << pdh << std::endl;
                }
                break;
            } else if (!pdh.empty()) {
                std::cout << "Connection established with Packet Data Handle: " << pdh << std::endl;
                m_session_info.packet_data_handle = pdh;
                // Assuming CID can be derived from PDH if not explicitly given
                m_session_info.connection_id = std::stoul(pdh); 
                break;
            } else {
                std::cout << "Warning: QMI command succeeded but no CID or PDH found" << std::endl;

                // Check packet service status as fallback
                if (isPacketServiceConnected()) {
                    std::cout << "Packet service shows connected state, proceeding" << std::endl;
                    m_session_info.connection_id = 1; // Use placeholder
                    break;
                }
            }
        } else {
            // Analyze and handle the error
            last_error = analyzeConnectionError(qmi_output);
            std::cout << "Connection attempt failed: " << last_error << std::endl;

            // Try recovery if it's a retryable error
            if (isRetryableError(last_error)) {
                attemptConnectionRecovery(last_error);
            }
        }

        if (attempt < max_retries) {
            std::cout << "Waiting before retry..." << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(attempt * 2)); // Progressive backoff
        }
    }

    if (m_session_info.connection_id == 0) {
        std::cerr << "Failed to establish connection after " << max_retries << " attempts" << std::endl;
        std::cerr << "Last error: " << last_error << std::endl;
        return false;
    }

    // Verify and retrieve connection settings
    if (!verifyConnection() || !retrieveConnectionSettings()) {
        std::cout << "Connection established but verification/settings retrieval failed" << std::endl;
        // Don't fail here as connection might still work
    }

    m_session_active = true;
    m_session_info.is_active = true;
    m_session_info.retry_count = attempt - 1;

    std::cout << "Connection established successfully" << std::endl;
    return true;
}

// Enhanced verification following script pattern
bool QMISessionHandler::verifyConnection() {
    std::cout << "Verifying connection establishment..." << std::endl;

    PacketServiceStatus status = getPacketServiceStatus();
    std::cout << "Packet service status: " << status.connection_status << std::endl;

    if (!status.connected) {
        std::cout << "Warning: Packet service not showing connected state" << std::endl;
        return false;
    }

    std::cout << "Packet service confirmed as connected" << std::endl;
    return true;
}

bool QMISessionHandler::retrieveConnectionSettings() {
    std::lock_guard<std::mutex> wds_lock(m_wds_mutex);
    std::cout << "Retrieving connection settings..." << std::endl;

    std::ostringstream cmd;
    cmd << "qmicli -d " << m_device_path << " --wds-get-current-settings";
    std::string conn_info = executeQMICommand(cmd.str());

    std::cout << "Raw connection settings:" << std::endl;
    std::istringstream iss(conn_info);
    std::string line;
    while (std::getline(iss, line)) {
        std::cout << "  " << line << std::endl;
    }

    if (conn_info.empty() || conn_info.find("error") != std::string::npos) {
        std::cout << "Failed to get connection settings" << std::endl;
        return false;
    }

    // Parse network parameters
    CurrentSettings settings;
    if (parseConnectionSettings(conn_info, settings)) {
        m_session_info.ip_address = settings.ip_address;
        m_session_info.gateway = settings.gateway;
        m_session_info.dns_primary = settings.dns_primary;
        m_session_info.dns_secondary = settings.dns_secondary;

        std::cout << "Network parameters extracted:" << std::endl;
        std::cout << "  IP: " << settings.ip_address << std::endl;
        std::cout << "  Gateway: " << settings.gateway << std::endl;
        std::cout << "  DNS1: " << settings.dns_primary << std::endl;
        std::cout << "  DNS2: " << settings.dns_secondary << std::endl;

        return true;
    }

    return false;
}

bool QMISessionHandler::stopDataSession() {
    std::lock_guard<std::mutex> lock(m_session_mutex);

    if (!m_session_active || m_session_info.connection_id == 0) {
        std::cout << "No active session to stop" << std::endl;
        return true;
    }

    return stopDataSession(m_session_info.connection_id);
}

bool QMISessionHandler::stopDataSession(uint32_t connection_id) {
    std::lock_guard<std::mutex> wds_lock(m_wds_mutex);
    std::ostringstream cmd;
    cmd << "qmicli -d " << m_device_path << " --wds-stop-network=" << connection_id;

    std::cout << "Stopping data session with command: " << cmd.str() << std::endl;
    std::string output = executeCommand(cmd.str());

    if (output.find("error") != std::string::npos) {
        std::cout << "Error stopping data session: " << output << std::endl;
        // Don't return false, try to clean up other resources
    }

    // Reset session info
    m_session_info = {}; // Clear session info
    m_session_active = false;
    m_connection_id = 0; // Reset connection ID
    m_packet_data_handle = ""; // Reset packet data handle

    std::cout << "Data session stopped successfully" << std::endl;
    return true;
}

PacketServiceStatus QMISessionHandler::getPacketServiceStatus() const {
    std::lock_guard<std::mutex> wds_lock(m_wds_mutex);
    PacketServiceStatus status = {};
    
    // Try with fresh client allocation to avoid stale CID issues
    std::ostringstream cmd;
    cmd << "qmicli -d " << m_device_path << " --wds-get-packet-service-status --client-no-release-cid";

    std::string output = executeCommand(cmd.str());
    
    // If we get CID allocation errors, try alternative approach
    if (output.find("CID allocation failed") != std::string::npos || 
        output.find("endpoint hangup") != std::string::npos ||
        output.find("Service mismatch") != std::string::npos) {
        
        std::cout << "Primary method failed, trying alternative packet status check..." << std::endl;
        
        // Reset and retry with simpler command
        executeCommand("qmicli -d " + m_device_path + " --wds-noop >/dev/null 2>&1");
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        cmd.str("");
        cmd << "qmicli -d " << m_device_path + " --wds-get-packet-service-status";
        output = executeCommand(cmd.str());
        
        if (output.find("error") != std::string::npos) {
            std::cout << "Packet service status unavailable, assuming disconnected" << std::endl;
            status.connected = false;
            status.connection_status = "unavailable";
            return status;
        }
    }
    
    return parsePacketServiceStatus(output);
}

CurrentSettings QMISessionHandler::getCurrentSettings() {
    std::lock_guard<std::mutex> wds_lock(m_wds_mutex);
    CurrentSettings settings = {};

    if (!m_session_active) {
        return settings; // Return empty settings if session is not active
    }

    std::ostringstream cmd;
    // Use the client ID if available for a more specific query
    if (m_client_id != 0) {
        cmd << "qmicli -d '" << m_device_path << "' --wds-get-current-settings --client-cid=" << m_client_id;
    } else {
        cmd << "qmicli -d '" << m_device_path << "' --wds-get-current-settings";
    }

    std::string output = executeCommand(cmd.str());

    parseConnectionSettings(output, settings);
    return settings;
}

QMIConnectionDetails QMISessionHandler::getConnectionDetails() {
    QMIConnectionDetails details;
    details.connection_id = m_session_info.connection_id; // Use the stored connection ID
    details.packet_data_handle = m_session_info.packet_data_handle; // Use stored handle
    details.client_id = std::to_string(m_client_id);
    details.is_active = m_session_active; // Reflect the session active status
    return details;
}


SignalInfo QMISessionHandler::getSignalInfo() {
    std::ostringstream cmd;
    cmd << "qmicli -d " << m_device_path << " --nas-get-signal-info";

    std::string output = executeCommand(cmd.str());
    return parseSignalInfo(output);
}

bool QMISessionHandler::setModemOnline() {
    std::lock_guard<std::mutex> dms_lock(m_dms_mutex);
    std::ostringstream cmd;
    cmd << "qmicli -d " << m_device_path << " --dms-set-operating-mode='online'";

    std::string output = executeCommand(cmd.str());

    if (output.find("error") != std::string::npos) {
        std::cout << "Error setting modem online: " << output << std::endl;
        return false;
    }

    // Wait a moment for the modem to come online
    std::this_thread::sleep_for(std::chrono::seconds(2));
    return true;
}

bool QMISessionHandler::setModemOffline() {
    std::lock_guard<std::mutex> dms_lock(m_dms_mutex);
    std::ostringstream cmd;
    cmd << "qmicli -d " << m_device_path << " --dms-set-operating-mode='offline'";

    std::string output = executeCommand(cmd.str());
    return output.find("error") == std::string::npos;
}

bool QMISessionHandler::setModemLowPower() {
    std::lock_guard<std::mutex> dms_lock(m_dms_mutex);
    std::ostringstream cmd;
    cmd << "qmicli -d " << m_device_path << " --dms-set-operating-mode='low-power'";

    std::string output = executeCommand(cmd.str());
    return output.find("error") == std::string::npos;
}

bool QMISessionHandler::resetModem() {
    {
        std::lock_guard<std::mutex> dms_lock(m_dms_mutex);
        std::ostringstream cmd;
        cmd << "qmicli -d " << m_device_path << " --dms-set-operating-mode='reset'";

        std::string output = executeCommand(cmd.str());

        if (output.find("error") != std::string::npos) {
            return false;
        }

        // Reset session state
        m_session_info = {};
        m_session_active = false;
        m_client_id = 0; // Reset client ID

        // Wait for modem to reset and come back online
        std::this_thread::sleep_for(std::chrono::seconds(10));
    } // Release DMS mutex here

    // Call setModemOnline() which will acquire its own mutex
    return setModemOnline();
}

bool QMISessionHandler::isSessionActive() const {
    // Return cached status to avoid QMI service calls that cause conflicts
    return m_session_active;
}

bool QMISessionHandler::validateConnection() {
    if (!m_session_active) {
        std::cout << "Session not marked as active" << std::endl;
        return false;
    }

    // Use cached session info to avoid QMI service conflicts
    if (m_session_info.ip_address.empty()) {
        std::cout << "No IP address in cached session info" << std::endl;
        return false;
    }

    std::cout << "Connection validation using cached data - IP: " << m_session_info.ip_address << std::endl;
    return true;
}

uint32_t QMISessionHandler::getCurrentConnectionId() const {
    std::lock_guard<std::mutex> lock(m_session_mutex);
    return m_session_info.connection_id;
}

SessionInfo QMISessionHandler::getSessionInfo() const {
    std::lock_guard<std::mutex> lock(m_session_mutex);
    return m_session_info;
}

bool QMISessionHandler::openClient() {
    std::cout << "Opening QMI client for device: " << m_device_path << std::endl;

    // Reset any hanging clients first (need mutexes for WDS and DMS)
    std::cout << "Resetting QMI services..." << std::endl;
    {
        std::lock_guard<std::mutex> wds_lock(m_wds_mutex);
        executeCommand("qmicli -d " + m_device_path + " --wds-noop >/dev/null 2>&1");
    }
    {
        std::lock_guard<std::mutex> dms_lock(m_dms_mutex);
        executeCommand("qmicli -d " + m_device_path + " --dms-noop >/dev/null 2>&1");
    }
    executeCommand("qmicli -d " + m_device_path + " --nas-noop >/dev/null 2>&1");
    
    // Wait for services to stabilize
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Test device accessibility with DMS service
    {
        std::lock_guard<std::mutex> dms_lock(m_dms_mutex);
        std::ostringstream cmd;
        cmd << "qmicli -d " << m_device_path << " --dms-get-operating-mode";
        std::string output = executeCommand(cmd.str());

        if (output.find("error") != std::string::npos || output.find("endpoint hangup") != std::string::npos) {
            std::cerr << "Failed to access QMI device: " << output << std::endl;
            
            // Try device reset
            std::cout << "Attempting device reset..." << std::endl;
            executeCommand("qmicli -d " + m_device_path + " --device-open-proxy");
            std::this_thread::sleep_for(std::chrono::seconds(3));
            
            // Retry
            output = executeCommand(cmd.str());
            if (output.find("error") != std::string::npos) {
                m_client_id = 0;
                return false;
            }
        }
    }

    m_client_id = 1;
    std::cout << "QMI device accessible, client ready" << std::endl;
    return true;
}

void QMISessionHandler::releaseClient() {
    if (m_client_id != 0) {
        std::cout << "Releasing QMI client with CID: " << m_client_id << std::endl;
        std::ostringstream cmd;
        cmd << "qmicli -d " << m_device_path << " --client-cid=" << m_client_id << " --remove-client";
        executeCommand(cmd.str()); // Execute but don't wait for result, just try to release
        m_client_id = 0; // Reset client ID
    }
}

QMISessionHandler::DeviceInfo QMISessionHandler::getDeviceInfo() const {
    std::lock_guard<std::mutex> dms_lock(m_dms_mutex);
    DeviceInfo info;
    info.device_path = m_device_path;

    // Get IMEI
    std::string imei_output = executeCommand("qmicli -d " + m_device_path + " --dms-get-ids");
    info.imei = parseCommandOutput(imei_output, "IMEI");

    // Get model and manufacturer
    std::string model_output = executeCommand("qmicli -d " + m_device_path + " --dms-get-model");
    info.model = parseCommandOutput(model_output, "Model");

    std::string manufacturer_output = executeCommand("qmicli -d " + m_device_path + " --dms-get-manufacturer");
    info.manufacturer = parseCommandOutput(manufacturer_output, "Manufacturer");

    // Get firmware version
    std::string version_output = executeCommand("qmicli -d " + m_device_path + " --dms-get-revision");
    info.firmware_version = parseCommandOutput(version_output, "Revision");

    return info;
}

std::string QMISessionHandler::getDevicePath() const {
    return m_device_path;
}

std::string QMISessionHandler::getIMEI() const {
    std::lock_guard<std::mutex> dms_lock(m_dms_mutex);
    std::string imei_output = executeCommand("qmicli -d " + m_device_path + " --dms-get-ids");
    return parseCommandOutput(imei_output, "IMEI");
}

std::string QMISessionHandler::executeCommand(const std::string& command) const {
    CommandLogger::logCommand(command);

    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) {
        CommandLogger::logCommandResult(command, "", -1);
        return "";
    }

    std::string result;
    char buffer[256];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        result += buffer;
    }

    int exit_code = pclose(pipe);
    CommandLogger::logCommandResult(command, result, WEXITSTATUS(exit_code));
    return result;
}

std::string QMISessionHandler::parseCommandOutput(const std::string& output, const std::string& field) const {
    std::regex pattern(field + ":\\s*'([^']*)'");
    std::smatch match;

    if (std::regex_search(output, match, pattern)) {
        return match[1].str();
    }

    // Try without quotes
    std::regex pattern2(field + ":\\s*([^\\n\\r]+)");
    if (std::regex_search(output, match, pattern2)) {
        std::string result = match[1].str();
        // Trim whitespace
        result.erase(0, result.find_first_not_of(" \t\r\n"));
        result.erase(result.find_last_not_of(" \t\r\n") + 1);
        return result;
    }

    return "";
}

uint32_t QMISessionHandler::parseConnectionId(const std::string& output) {
    std::regex pattern("Connection\\s*ID:\\s*(\\d+)");
    std::smatch match;

    if (std::regex_search(output, match, pattern)) {
        return std::stoul(match[1].str());
    }

    // Alternative pattern
    std::regex pattern2("CID\\s*:\\s*(\\d+)");
    if (std::regex_search(output, match, pattern2)) {
        return std::stoul(match[1].str());
    }

    return 0;
}

PacketServiceStatus QMISessionHandler::parsePacketServiceStatus(const std::string& output) const {
    PacketServiceStatus status = {};

    std::string connection_status = parseCommandOutput(output, "Connection status");
    status.connected = (connection_status == "connected");
    status.connection_status = connection_status;

    std::string bearer_tech = parseCommandOutput(output, "Data bearer technology");
    if (!bearer_tech.empty()) {
        try {
            status.data_bearer_technology = std::stoul(bearer_tech);
        } catch (const std::invalid_argument& e) {
            std::cerr << "Could not convert bearer technology to integer: " << bearer_tech << std::endl;
            status.data_bearer_technology = 0;
        } catch (const std::out_of_range& e) {
            std::cerr << "Bearer technology value out of range: " << bearer_tech << std::endl;
            status.data_bearer_technology = 0;
        }
    }

    return status;
}

CurrentSettings QMISessionHandler::parseCurrentSettings(const std::string& output) {
    CurrentSettings settings = {};

    // Try to determine interface name from common QMI interface patterns
    std::ostringstream interface_cmd;
    interface_cmd << "ip link show | grep -E 'wwan[0-9]+|rmnet[0-9]+' | head -1 | awk '{print $2}' | sed 's/:$//'";
    std::string interface_output = const_cast<QMISessionHandler*>(this)->executeCommand(interface_cmd.str());
    if (!interface_output.empty()) {
        // Remove newlines and whitespace
        interface_output.erase(std::remove(interface_output.begin(), interface_output.end(), '\n'), interface_output.end());
        interface_output.erase(std::remove(interface_output.begin(), interface_output.end(), '\r'), interface_output.end());
        settings.interface_name = interface_output;
    } else {
        settings.interface_name = "wwan0"; // Default fallback
    }

    settings.ip_address = parseCommandOutput(output, "IPv4 address");
    settings.subnet_mask = parseCommandOutput(output, "IPv4 subnet mask");
    settings.gateway = parseCommandOutput(output, "IPv4 gateway address");
    settings.dns_primary = parseCommandOutput(output, "IPv4 primary DNS");
    settings.dns_secondary = parseCommandOutput(output, "IPv4 secondary DNS");

    std::string mtu_str = parseCommandOutput(output, "MTU");
    if (!mtu_str.empty()) {
        try {
            settings.mtu = std::stoul(mtu_str);
        } catch (const std::invalid_argument& e) {
            std::cerr << "Could not convert MTU to integer: " << mtu_str << std::endl;
            settings.mtu = 1500; // Default MTU on conversion failure
        } catch (const std::out_of_range& e) {
            std::cerr << "MTU value out of range: " << mtu_str << std::endl;
            settings.mtu = 1500; // Default MTU on conversion failure
        }
    } else {
        settings.mtu = 1500; // Default MTU
    }

    return settings;
}

SignalInfo QMISessionHandler::parseSignalInfo(const std::string& output) {
    SignalInfo info = {};

    std::string rssi_str = parseCommandOutput(output, "RSSI");
    if (!rssi_str.empty()) {
        try {
            info.rssi = std::stoi(rssi_str);
        } catch (...) { /* ignore conversion errors */ }
    }

    std::string rsrp_str = parseCommandOutput(output, "RSRP");
    if (!rsrp_str.empty()) {
        try {
            info.rsrp = std::stoi(rsrp_str);
        } catch (...) { /* ignore conversion errors */ }
    }

    std::string rsrq_str = parseCommandOutput(output, "RSRQ");
    if (!rsrq_str.empty()) {
        try {
            info.rsrq = std::stoi(rsrq_str);
        } catch (...) { /* ignore conversion errors */ }
    }

    std::string sinr_str = parseCommandOutput(output, "SINR");
    if (!sinr_str.empty()) {
        try {
            info.sinr = std::stoi(sinr_str);
        } catch (...) { /* ignore conversion errors */ }
    }

    info.network_type = parseCommandOutput(output, "Radio interface");
    info.band = parseCommandOutput(output, "Band");
    info.carrier = parseCommandOutput(output, "Provider");

    return info;
}

// Additional enhanced methods
bool QMISessionHandler::stopModemManager() {
    // Execute without checking output, as ModemManager might not be installed or running
    executeCommand("systemctl stop ModemManager > /dev/null 2>&1");
    return true; 
}

bool QMISessionHandler::diagnoseConnectionPrerequisites() {
    std::cout << "Performing connection prerequisites check..." << std::endl;

    // Check if device exists
    std::ostringstream cmd_check_device;
    cmd_check_device << "test -e " << m_device_path;
    if (executeCommand(cmd_check_device.str()).find("error") != std::string::npos) {
        std::cout << "Device " << m_device_path << " not found" << std::endl;
        return false;
    }

    // Check modem status
    if (!isModemReady()) {
        std::cout << "Modem not ready" << std::endl;
        return false;
    }

    std::cout << "Prerequisites check passed" << std::endl;
    return true;
}

bool QMISessionHandler::performConnectionDiagnostics() {
    std::cout << "Performing connection diagnostics..." << std::endl;

    PacketServiceStatus status = getPacketServiceStatus();
    std::cout << "Packet service status: " << status.connection_status << std::endl;

    CurrentSettings settings = getCurrentSettings();
    std::cout << "Current settings - IP: " << settings.ip_address 
              << ", Gateway: " << settings.gateway << std::endl;

    return status.connected;
}

bool QMISessionHandler::isPacketServiceConnected() {
    PacketServiceStatus status = getPacketServiceStatus();
    return status.connected;
}

bool QMISessionHandler::validateConnectionParameters() {
    if (m_session_info.ip_address.empty()) {
        std::cout << "No IP address configured" << std::endl;
        return false;
    }

    if (!validateIPAddress(m_session_info.ip_address)) {
        std::cout << "Invalid IP address: " << m_session_info.ip_address << std::endl;
        return false;
    }

    if (!m_session_info.gateway.empty() && !validateIPAddress(m_session_info.gateway)) {
        std::cout << "Invalid gateway: " << m_session_info.gateway << std::endl;
        return false;
    }

    return true;
}

// Cellular mode configuration methods
bool QMISessionHandler::setCellularMode(ModemTechnology mode, const std::vector<int>& preferred_bands) {
    std::cout << "Setting cellular mode to: " << getModemTechnologyString(mode) << std::endl;

    // Create network mode preference
    NetworkModePreference preference;
    preference.technology = mode;
    preference.bands = preferred_bands;
    preference.persistent = true;

    return setNetworkModePreference(preference);
}

bool QMISessionHandler::setNetworkModePreference(const NetworkModePreference& preference) {
    std::string mode_value;

    // Map ModemTechnology to QMI NAS mode preference values
    switch (preference.technology) {
        case ModemTechnology::AUTOMATIC:
            mode_value = "auto";
            break;
        case ModemTechnology::LTE_ONLY:
            mode_value = "lte";
            break;
        case ModemTechnology::FIVE_G_ONLY:
            mode_value = "5g";
            break;
        case ModemTechnology::THREE_GPP_LEGACY:
            mode_value = "umts";
            break;
        case ModemTechnology::WCDMA_GSM_AUTO:
            mode_value = "gsm-wcdma-auto";
            break;
        case ModemTechnology::GSM_ONLY:
            mode_value = "gsm";
            break;
        case ModemTechnology::LTE_FIVE_G_AUTO:
            mode_value = "lte-5g";
            break;
        default:
            mode_value = "auto";
            break;
    }

    // Set system selection preference using qmicli
    std::string cmd = "qmicli -d " + m_device_path + " --nas-set-system-selection-preference=" + mode_value;

    if (!preference.bands.empty()) {
        // Add band preference if specified
        std::string band_list;
        for (size_t i = 0; i < preference.bands.size(); ++i) {
            if (i > 0) band_list += ",";
            band_list += std::to_string(preference.bands[i]);
        }
        cmd += " --nas-set-system-selection-preference-bands=" + band_list;
    }

    std::cout << "Executing mode selection command: " << cmd << std::endl;

    std::string result = executeCommand(cmd);
    bool success = (result.find("successfully") != std::string::npos || 
                   result.find("System selection preference") != std::string::npos);

    if (success) {
        std::cout << "Successfully set network mode preference" << std::endl;

        // Wait for mode change to take effect
        std::this_thread::sleep_for(std::chrono::seconds(3));

        // Verify the change
        ModemTechnology current_mode = getCurrentModemTechnology();
        if (current_mode == preference.technology) {
            std::cout << "Mode change verified successfully" << std::endl;
        } else {
            std::cout << "Warning: Mode change may still be in progress or verification failed" << std::endl;
        }
    } else {
        std::cerr << "Failed to set network mode preference: " << result << std::endl;
    }

    return success;
}

ModemTechnology QMISessionHandler::getCurrentModemTechnology() {
    std::string cmd = "qmicli -d " + m_device_path + " --nas-get-system-selection-preference";
    std::string result = executeCommand(cmd);

    // Parse the result to determine current mode
    if (result.find("LTE") != std::string::npos && result.find("5G") != std::string::npos) {
        return ModemTechnology::LTE_FIVE_G_AUTO;
    } else if (result.find("LTE") != std::string::npos) {
        return ModemTechnology::LTE_ONLY;
    } else if (result.find("5G") != std::string::npos) {
        return ModemTechnology::FIVE_G_ONLY;
    } else if (result.find("UMTS") != std::string::npos) {
        return ModemTechnology::THREE_GPP_LEGACY;
    } else if (result.find("GSM") != std::string::npos && result.find("WCDMA") != std::string::npos) {
        return ModemTechnology::WCDMA_GSM_AUTO;
    } else if (result.find("GSM") != std::string::npos) {
        return ModemTechnology::GSM_ONLY;
    }

    return ModemTechnology::AUTOMATIC;
}

bool QMISessionHandler::enforceNetworkMode(ModemTechnology mode) {
    std::cout << "Enforcing network mode: " << getModemTechnologyString(mode) << std::endl;

    // Set the mode with force flag
    NetworkModePreference preference;
    preference.technology = mode;
    preference.persistent = true;

    bool success = setNetworkModePreference(preference);

    if (success) {
        // Wait longer for forced mode change
        std::cout << "Waiting for network mode enforcement..." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(10));

        // Verify the enforced mode
        ModemTechnology current_mode = getCurrentModemTechnology();
        std::cout << "Current mode after enforcement: " << getModemTechnologyString(current_mode) << std::endl;
    }

    return success;
}

std::string QMISessionHandler::getModemTechnologyString(ModemTechnology mode) {
    switch (mode) {
        case ModemTechnology::AUTOMATIC: return "Automatic";
        case ModemTechnology::LTE_ONLY: return "LTE Only";
        case ModemTechnology::FIVE_G_ONLY: return "5G Only";
        case ModemTechnology::THREE_GPP_LEGACY: return "3GPP Legacy (UMTS)";
        case ModemTechnology::WCDMA_GSM_AUTO: return "WCDMA/GSM Auto";
        case ModemTechnology::GSM_ONLY: return "GSM Only";
        case ModemTechnology::LTE_FIVE_G_AUTO: return "LTE/5G Auto";
        default: return "Unknown";
    }
}

// --- Helper Functions ---

// Execute QMI command with enhanced logging (referenced by enhanced methods)
std::string QMISessionHandler::executeQMICommand(const std::string& command) {
    return executeCommandWithTimeout(command, m_connection_timeout);
}

// Mutex-protected WDS command execution
std::string QMISessionHandler::executeWDSCommand(const std::string& command) {
    std::lock_guard<std::mutex> wds_lock(m_wds_mutex);
    return executeCommand(command);
}

// Mutex-protected DMS command execution
std::string QMISessionHandler::executeDMSCommand(const std::string& command) {
    std::lock_guard<std::mutex> dms_lock(m_dms_mutex);
    return executeCommand(command);
}

// Execute command with timeout
std::string QMISessionHandler::executeCommandWithTimeout(const std::string& command, int timeout_seconds) {
    // For simplicity, we'll use the existing executeCommand. 
    // In a real scenario, this would involve more sophisticated process management.
    // TODO: Implement actual timeout functionality using timeout_seconds
    (void)timeout_seconds; // Suppress unused parameter warning
    return executeCommand(command);
}

// Enhanced parsing function for connection settings
bool QMISessionHandler::parseConnectionSettings(const std::string& output, CurrentSettings& settings) {
    // Use robust extraction methods following script patterns
    settings.ip_address = extractIPValue(output);

    std::istringstream iss(output);
    std::string line;
    while (std::getline(iss, line)) {
        if (line.find("IPv4 address:") != std::string::npos) {
            settings.ip_address = extractIPValue(line);
        } else if (line.find("IPv4 subnet mask:") != std::string::npos) {
            settings.subnet_mask = extractIPValue(line);
        } else if (line.find("IPv4 gateway address:") != std::string::npos) {
            settings.gateway = extractIPValue(line);
        } else if (line.find("IPv4 primary DNS:") != std::string::npos) {
            settings.dns_primary = extractIPValue(line);
        } else if (line.find("IPv4 secondary DNS:") != std::string::npos) {
            settings.dns_secondary = extractIPValue(line);
        } else if (line.find("MTU:") != std::string::npos) {
            std::string mtu_str = extractNumericValue(line);
            if (!mtu_str.empty()) {
                try {
                    settings.mtu = std::stoul(mtu_str);
                } catch (...) { /* ignore conversion errors */ }
            }
        }
    }

    // Set interface name from member variable if not extracted otherwise
    if (settings.interface_name.empty()) {
        settings.interface_name = m_interface_name;
    }

    // Return true if we found at least an IP address
    return !settings.ip_address.empty();
}

// Extract connection ID using multiple patterns (following script approach)
uint32_t QMISessionHandler::extractConnectionId(const std::string& output) {
    std::smatch match;

    // Pattern 1: CID: 'number' (most common)
    std::regex pattern1("CID\\s*:\\s*'(\\d+)'");
    if (std::regex_search(output, match, pattern1)) {
        return std::stoul(match[1].str());
    }

    // Pattern 2: CID: number (alternative format)
    std::regex pattern2("CID\\s*:\\s*(\\d+)");
    if (std::regex_search(output, match, pattern2)) {
        return std::stoul(match[1].str());
    }

    // Pattern 3: Client ID from "Client ID not released" message
    std::regex pattern3("Client ID not released.*CID:\\s*'(\\d+)'");
    if (std::regex_search(output, match, pattern3)) {
        return std::stoul(match[1].str());
    }

    return 0;
}

// Extract packet data handle
std::string QMISessionHandler::extractPacketDataHandle(const std::string& output) {
    std::smatch match;
    std::regex pattern("Packet data handle\\s*:\\s*'(\\d+)'");
    if (std::regex_search(output, match, pattern)) {
        return match[1].str();
    }
    return "";
}

// Network parameter extraction (following script methods)
std::string QMISessionHandler::extractIPValue(const std::string& line) {
    // Extract IP address after colon - handle both formats
    std::regex pattern1(":\\s*([0-9]{1,3}\\.[0-9]{1,3}\\.[0-9]{1,3}\\.[0-9]{1,3})");
    std::smatch match;
    if (std::regex_search(line, match, pattern1)) {
        return match[1].str();
    }

    // Alternative pattern for different QMI output formats
    std::regex pattern2("([0-9]{1,3}\\.[0-9]{1,3}\\.[0-9]{1,3}\\.[0-9]{1,3})");
    if (std::regex_search(line, match, pattern2)) {
        std::string ip = match[1].str();
        if (validateIPAddress(ip)) {
            return ip;
        }
    }

    return "";
}

std::string QMISessionHandler::extractNumericValue(const std::string& line) {
    // Extract numeric value after colon
    std::regex pattern(":\\s*(\\d+)");
    std::smatch match;
    if (std::regex_search(line, match, pattern)) {
        return match[1].str();
    }
    return "";
}

// IP validation following script pattern
bool QMISessionHandler::validateIPAddress(const std::string& ip) {
    if (ip.empty()) return false;

    std::regex pattern("^([0-9]{1,3}\\.){3}[0-9]{1,3}$");
    if (!std::regex_match(ip, pattern)) {
        return false;
    }

    // Check each octet is 0-255
    std::istringstream iss(ip);
    std::string octet;
    while (std::getline(iss, octet, '.')) {
        try {
            int val = std::stoi(octet);
            if (val < 0 || val > 255) {
                return false;
            }
        } catch (...) {
            return false; // Invalid octet
        }
    }

    return true;
}

bool QMISessionHandler::validateSubnetMask(const std::string& mask) {
    if (!validateIPAddress(mask)) {
        return false;
    }

    // Convert to binary and check if it's a valid subnet mask (contiguous 1s followed by 0s)
    std::istringstream iss(mask);
    std::string octet;
    std::string binary = "";

    while (std::getline(iss, octet, '.')) {
        try {
            int val = std::stoi(octet);
            for (int i = 7; i >= 0; i--) {
                binary += ((val >> i) & 1) ? "1" : "0";
            }
        } catch (...) {
            return false; // Invalid octet
        }
    }

    // Check pattern: 1*0*
    std::regex pattern("^1*0*$");
    return std::regex_match(binary, pattern);
}

bool QMISessionHandler::validateGateway(const std::string& gateway, const std::string& ip, const std::string& mask) {
    if (!validateIPAddress(gateway) || !validateIPAddress(ip) || !validateSubnetMask(mask)) {
        return false;
    }

    // Check if gateway is in the same subnet as IP
    // This is a simplified check - a full implementation would do proper subnet calculation
    return true; // For now, assume it's valid if IP format is correct
}

// Error analysis and recovery following script pattern
std::string QMISessionHandler::analyzeConnectionError(const std::string& error_output) {
    return categorizeQMIError(error_output);
}

std::string QMISessionHandler::categorizeQMIError(const std::string& error_output) {
    if (error_output.find("Couldn't create client") != std::string::npos) {
        return "client_creation_failed";
    }
    if (error_output.find("call failed") != std::string::npos) {
        return "call_failed";
    }
    if (error_output.find("timeout") != std::string::npos) {
        return "timeout";
    }
    if (error_output.find("Invalid APN") != std::string::npos) {
        return "invalid_apn";
    }
    if (error_output.find("authentication") != std::string::npos) {
        return "authentication_failed";
    }
    if (error_output.find("already connected") != std::string::npos) {
        return "already_connected";
    }
    if (error_output.find("Invalid operation") != std::string::npos) {
        return "invalid_operation";
    }
    if (error_output.find("Operation not supported") != std::string::npos) {
        return "operation_not_supported";
    }


    return "unknown_error";
}

bool QMISessionHandler::isRetryableError(const std::string& error_type) {
    return (error_type == "timeout" || 
            error_type == "call_failed" || 
            error_type == "client_creation_failed");
}

void QMISessionHandler::logConnectionAttempt(int attempt, const std::string& result) {
    std::cout << "Connection attempt " << attempt << " result:" << std::endl;
    std::istringstream iss(result);
    std::string line;
    while (std::getline(iss, line)) {
        std::cout << "  " << line << std::endl;
    }
}

bool QMISessionHandler::attemptConnectionRecovery(const std::string& error_type) {
    std::cout << "Attempting recovery for error: " << error_type << std::endl;

    if (error_type == "client_creation_failed") {
        // Reset WDS service
        resetWDSService();
        // Re-open client
        return openClient();
    }

    if (error_type == "timeout") {
        // Increase timeout for next attempt
        m_connection_timeout += 5;
        return true;
    }

    if (error_type == "invalid_operation" || error_type == "operation_not_supported") {
        // Sometimes a modem reset can help with these errors
        std::cout << "Attempting modem reset for operational error..." << std::endl;
        return resetModem();
    }

    return false;
}

// Interface management
bool QMISessionHandler::bringInterfaceDown(const std::string& interface_name) {
    std::ostringstream cmd;
    cmd << "ip link set " << interface_name << " down";
    std::string output = executeCommand(cmd.str());
    // Check for specific error messages indicating failure, otherwise assume success
    return output.find("error") == std::string::npos || output.find("not found") != std::string::npos;
}

bool QMISessionHandler::bringInterfaceUp(const std::string& interface_name) {
    std::ostringstream cmd;
    cmd << "ip link set " << interface_name << " up";
    std::string output = executeCommand(cmd.str());
    // Check for specific error messages indicating failure, otherwise assume success
    return output.find("error") == std::string::npos || output.find("not found") != std::string::npos;
}







std::string QMISessionHandler::getInterfaceName() const {
    return m_interface_name;
}