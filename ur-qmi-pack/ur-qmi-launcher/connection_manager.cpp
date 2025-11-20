#include "connection_manager.h"
#include "smart_routing.h"
#include "qmi_session_handler.h"
#include "connection_state_machine.h"
#include "interface_controller.h"
#include "connectivity_monitor.h"
#include "failure_detector.h"
#include "recovery_engine.h"
#include "metrics_reporter.h"
#include "connection_registry.h" // Include for ConnectionLifecycleManager
#include "ip_monitor.h" // Include for IP monitor
#include <json/json.h>
#include <iostream>
#include <sstream>
#include <thread>
#include <chrono>
#include <algorithm>

// Static instance for signal handling
ConnectionManager* ConnectionManager::s_active_instance = nullptr;

ConnectionManager::ConnectionManager() 
    : m_current_state(ConnectionState::IDLE),
      m_initialized(false),
      m_monitoring_enabled(false),
      m_auto_recovery_enabled(false) {
    s_active_instance = this;
}

ConnectionManager::~ConnectionManager() {
    if (m_monitoring_enabled) {
        stopMonitoring();
    }

    if (m_current_state != ConnectionState::IDLE) {
        disconnect();
    }
}

bool ConnectionManager::initialize(const std::string& device_json) {
    try {
        parseDeviceJson(device_json);

        if (m_current_device.device_path.empty()) {
            std::cerr << "No valid device found in JSON" << std::endl;
            return false;
        }

        // Initialize session handler
        m_session_handler = std::make_unique<QMISessionHandler>(m_current_device.device_path);

        // Initialize lifecycle manager for connection registry tracking
        m_lifecycle_manager = std::make_unique<ConnectionLifecycleManager>(
            m_current_device.device_path, "", m_config.apn);

        // Initialize all components
        m_interface_controller = std::make_unique<InterfaceController>();
        m_connectivity_monitor = std::make_unique<ConnectivityMonitor>();
        m_failure_detector = std::make_unique<FailureDetector>(
            m_session_handler.get(), m_interface_controller.get(), m_connectivity_monitor.get());
        m_recovery_engine = std::make_unique<RecoveryEngine>(
            m_session_handler.get(), m_interface_controller.get(), m_connectivity_monitor.get());
        m_metrics_reporter = std::make_unique<MetricsReporter>(
            m_session_handler.get(), m_interface_controller.get(), m_connectivity_monitor.get());
        m_state_machine = std::make_unique<ConnectionStateMachine>(
            m_session_handler.get(), m_interface_controller.get());
        m_ip_monitor = std::make_unique<IPMonitor>();

        // Initialize state machine
        m_state_machine->initialize();
        m_state_machine->setConnectionConfig(m_config);
        
        // Load IP monitor configuration
        if (!m_ip_monitor->loadConfigFromFile("ip-monitor.json")) {
            std::cout << "IP monitor config not found, using defaults" << std::endl;
        }

        // Set up callbacks
        m_connectivity_monitor->setConnectivityCallback(
            [this](bool connected, const std::string& reason) {
                if (!connected && m_auto_recovery_enabled) {
                    FailureEvent failure;
                    failure.type = FailureType::CONNECTIVITY_LOST;
                    failure.description = reason;
                    failure.timestamp = std::chrono::system_clock::now();
                    failure.severity = 6;
                    failure.auto_recoverable = true;
                    m_recovery_engine->triggerRecovery(failure);
                }
            });

        m_failure_detector->setFailureCallback(
            [this](const FailureEvent& failure) {
                m_metrics_reporter->incrementConnectivityError();
                if (m_auto_recovery_enabled) {
                    m_recovery_engine->triggerRecovery(failure);
                }
            });

        m_recovery_engine->setRecoveryCallback(
            [this](const RecoveryResult& result) {
                if (result.success) {
                    m_metrics_reporter->incrementSuccessfulRecovery();
                } else {
                    m_metrics_reporter->incrementRecoveryAttempt();
                }
            });

        m_initialized = true;
        transitionToState(ConnectionState::IDLE, "Initialized");

        std::cout << "Connection manager initialized with device: " 
                  << m_current_device.device_path << std::endl;

        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error initializing connection manager: " << e.what() << std::endl;
        return false;
    }
}

bool ConnectionManager::initializeFromBasicProfile(const Json::Value& basic_profile) {
    try {
        DeviceInfo device;
        device.device_path = basic_profile["path"].asString();
        device.imei = basic_profile["imei"].asString();
        device.model = basic_profile["model"].asString();
        device.is_available = true;

        m_current_device = device;

        m_session_handler = std::make_unique<QMISessionHandler>(device.device_path);

        // Initialize lifecycle manager for connection registry tracking
        m_lifecycle_manager = std::make_unique<ConnectionLifecycleManager>(
            m_current_device.device_path, "", m_config.apn);

        m_initialized = true;
        transitionToState(ConnectionState::IDLE, "Initialized from basic profile");

        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error initializing from basic profile: " << e.what() << std::endl;
        return false;
    }
}

bool ConnectionManager::initializeFromAdvancedProfile(const Json::Value& advanced_profile) {
    try {
        const Json::Value& basic = advanced_profile["basic"];
        return initializeFromBasicProfile(basic);
    } catch (const std::exception& e) {
        std::cerr << "Error initializing from advanced profile: " << e.what() << std::endl;
        return false;
    }
}

bool ConnectionManager::connect(const ConnectionConfig& config) {
    if (!m_initialized) {
        std::cerr << "Connection manager not initialized" << std::endl;
        return false;
    }

    setConnectionConfig(config);

    // Increment connection attempt counter
    m_metrics_reporter->incrementConnectionAttempt();

    // Start state machine
    m_state_machine->start();
    m_state_machine->setConnectionConfig(config);

    // Start connection process
    transitionToState(ConnectionState::MODEM_ONLINE, "Starting connection");

    // Set cellular mode before establishing connection if configured
    if (config.enforce_mode_before_connection && 
        config.cellular_mode_config.mode != CellularMode::AUTO) {

        std::cout << "Setting cellular mode before connection..." << std::endl;
        if (!setCellularMode(config.cellular_mode_config)) {
            std::cerr << "Warning: Failed to set cellular mode, continuing anyway" << std::endl;
        } else {
            std::cout << "Cellular mode set successfully" << std::endl;
        }
    }

    // Perform startup cleanup before selecting interface
    performStartupCleanup();

    // Select optimal interface (reuse active or available inactive interface)
    std::string selected_interface = selectOptimalInterface(m_current_device.device_path);
    std::cout << "Selected interface for connection: " << selected_interface << std::endl;

    // Ensure the interface exists and is ready
    if (m_interface_controller) {
        if (!m_interface_controller->ensureInterfaceExists(selected_interface, m_current_device.device_path)) {
            std::cerr << "Warning: Could not ensure interface " << selected_interface << " exists" << std::endl;
        }
    }

    // Update session handler with selected interface name
    if (m_session_handler) {
        m_session_handler->setInterfaceName(selected_interface);
    }

    // CRITICAL: Enforce raw IP requirement before starting data session
    std::cout << "Checking raw IP requirement for interface: " << selected_interface << std::endl;
    if (m_interface_controller) {
        if (!m_interface_controller->enforceRawIPRequirement(selected_interface)) {
            std::cerr << "CRITICAL: Raw IP requirement not satisfied for interface " << selected_interface << std::endl;
            std::cerr << "Connection cannot proceed without raw IP mode enabled" << std::endl;
            m_metrics_reporter->incrementFailedConnection();
            transitionToState(ConnectionState::ERROR, "Raw IP requirement not satisfied");
            return false;
        }
    }

    // Start data session through state machine
    if (!m_state_machine->triggerTransition("initialize", "User requested connection")) {
        m_metrics_reporter->incrementFailedConnection();
        transitionToState(ConnectionState::ERROR, "Failed to initialize connection");
        return false;
    }

    // Wait for connection to establish (with timeout)
    auto start_time = std::chrono::steady_clock::now();
    auto timeout = std::chrono::seconds(60);

    while (std::chrono::steady_clock::now() - start_time < timeout) {
        ConnectionState current_state = m_state_machine->getCurrentState();

        if (current_state == ConnectionState::CONNECTED) {
            // Register connection details with lifecycle manager
            if (m_lifecycle_manager && m_session_handler) {
                QMIConnectionDetails conn_details = m_session_handler->getConnectionDetails();
                bool registered = m_lifecycle_manager->registerConnection(conn_details.connection_id, conn_details.packet_data_handle);

                if (!registered) {
                    std::cout << "Warning: Failed to register connection for lifecycle management" << std::endl;
                }
            }
            m_metrics_reporter->incrementSuccessfulConnection();
            break;
        } else if (current_state == ConnectionState::ERROR) {
            m_metrics_reporter->incrementFailedConnection();
            return false;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    if (m_state_machine->getCurrentState() != ConnectionState::CONNECTED) {
        m_metrics_reporter->incrementFailedConnection();
        transitionToState(ConnectionState::ERROR, "Connection timeout");
        return false;
    }

    transitionToState(ConnectionState::CONNECTED, "Connection established");
    
    // Start IP monitoring after successful connection
    if (m_ip_monitor && m_session_handler) {
        std::string interface_name = selected_interface;
        if (m_ip_monitor->startMonitoring(interface_name, m_session_handler.get())) {
            std::cout << "IP monitoring started for interface: " << interface_name << std::endl;
        } else {
            std::cout << "Failed to start IP monitoring or monitoring is disabled" << std::endl;
        }
    }

    // Apply smart routing for cellular interface
    if (m_interface_controller && m_session_handler) {
        auto settings = m_session_handler->getCurrentSettings();
        if (!settings.ip_address.empty() && !settings.gateway.empty()) {
            std::vector<std::string> interfaces = m_interface_controller->findWWANInterfaces();
            if (!interfaces.empty()) {
                std::string interface_name = interfaces[0]; // Use first found WWAN interface

                std::cout << "Applying smart routing for cellular interface: " << interface_name << std::endl;
                m_interface_controller->applyCellularRouting(interface_name, settings.gateway, settings.ip_address);
            }
        }
    }

    // Update metrics
    {
        std::lock_guard<std::mutex> lock(m_metrics_mutex);
        m_metrics.is_connected = true;
        m_metrics.connected_since = std::chrono::system_clock::now();
    }

    notifyMetrics(m_metrics);

    return true;
}

bool ConnectionManager::disconnect() {
    if (!m_initialized || m_current_state == ConnectionState::IDLE) {
        return true;
    }

    transitionToState(ConnectionState::IDLE, "Disconnecting");

    // Stop all monitoring and recovery
    if (m_monitoring_enabled) {
        stopMonitoring();
    }
    
    // Stop IP monitoring
    if (m_ip_monitor) {
        m_ip_monitor->stopMonitoring();
    }

    // Stop state machine
    if (m_state_machine) {
        m_state_machine->stop();
    }

    // Stop session
    if (m_session_handler) {
        m_session_handler->stopDataSession();
    }

    // Remove smart routing if configured
    if (m_interface_controller && m_session_handler) {
        auto settings = m_session_handler->getCurrentSettings();
        if (!settings.ip_address.empty()) {
            std::vector<std::string> interfaces = m_interface_controller->findWWANInterfaces();
            if (!interfaces.empty()) {
                std::string interface_name = interfaces[0]; // Use first found WWAN interface
                std::cout << "Removing smart routing for cellular interface: " << interface_name << std::endl;
                m_interface_controller->removeCellularRouting(interface_name);
            }
        }
    }

    // Update lifecycle manager status to inactive
    if (m_lifecycle_manager) {
        m_lifecycle_manager->updateStatus(false);
    }


    // Update metrics
    {
        std::lock_guard<std::mutex> lock(m_metrics_mutex);
        m_metrics.is_connected = false;
    }

    notifyMetrics(m_metrics);

    return true;
}

bool ConnectionManager::reconnect() {
    disconnect();
    std::this_thread::sleep_for(std::chrono::seconds(2));
    return connect(m_config);
}

ConnectionState ConnectionManager::getCurrentState() const {
    std::lock_guard<std::mutex> lock(m_state_mutex);
    return m_current_state;
}

std::string ConnectionManager::getStateString() const {
    switch (getCurrentState()) {
        case ConnectionState::IDLE: return "IDLE";
        case ConnectionState::MODEM_ONLINE: return "MODEM_ONLINE";
        case ConnectionState::SESSION_ACTIVE: return "SESSION_ACTIVE";
        case ConnectionState::IP_CONFIGURED: return "IP_CONFIGURED";
        case ConnectionState::CONNECTED: return "CONNECTED";
        case ConnectionState::RECONNECTING: return "RECONNECTING";
        case ConnectionState::ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

bool ConnectionManager::isConnected() const {
    return getCurrentState() == ConnectionState::CONNECTED;
}

void ConnectionManager::setConnectionConfig(const ConnectionConfig& config) {
    std::lock_guard<std::mutex> lock(m_config_mutex);
    m_config = config;
}

ConnectionConfig ConnectionManager::getConnectionConfig() const {
    std::lock_guard<std::mutex> lock(m_config_mutex);
    return m_config;
}

void ConnectionManager::setStateChangeCallback(StateChangeCallback callback) {
    m_state_callback = callback;
}

void ConnectionManager::setMetricsCallback(MetricsCallback callback) {
    m_metrics_callback = callback;
}

void ConnectionManager::startMonitoring() {
    if (m_monitoring_enabled) {
        return;
    }

    m_monitoring_enabled = true;

    if (m_connectivity_monitor) {
        m_connectivity_monitor->startMonitoring(m_config.health_check_interval_ms);
    }

    if (m_failure_detector) {
        m_failure_detector->startDetection();
    }

    if (m_recovery_engine) {
        m_recovery_engine->startRecoveryEngine();
    }

    if (m_metrics_reporter) {
        m_metrics_reporter->startReporting();
    }

    std::cout << "All monitoring components started" << std::endl;
}

void ConnectionManager::stopMonitoring() {
    if (!m_monitoring_enabled) {
        return;
    }

    m_monitoring_enabled = false;

    if (m_connectivity_monitor) {
        m_connectivity_monitor->stopMonitoring();
    }

    if (m_failure_detector) {
        m_failure_detector->stopDetection();
    }

    if (m_recovery_engine) {
        m_recovery_engine->stopRecoveryEngine();
    }

    if (m_metrics_reporter) {
        m_metrics_reporter->stopReporting();
    }

    // Note: IP monitor is managed separately and stopped during disconnect
    std::cout << "All monitoring components stopped" << std::endl;
}

ConnectionMetrics ConnectionManager::getCurrentMetrics() {
    std::lock_guard<std::mutex> lock(m_metrics_mutex);

    // Update metrics from cached session info to avoid QMI service conflicts
    if (m_session_handler && m_session_handler->isSessionActive()) {
        auto session_info = m_session_handler->getSessionInfo();
        m_metrics.ip_address = session_info.ip_address;
        m_metrics.dns_primary = session_info.dns_primary;
        m_metrics.dns_secondary = session_info.dns_secondary;
        
        // Set default values to avoid QMI calls
        m_metrics.signal_strength = -999;
        m_metrics.network_type = "Unknown";
    }

    return m_metrics;
}

std::vector<DeviceInfo> ConnectionManager::getAvailableDevices() {
    // TODO: Implement device discovery
    return {m_current_device};
}

bool ConnectionManager::selectDevice(const std::string& device_path) {
    if (m_current_state != ConnectionState::IDLE) {
        disconnect();
    }

    m_current_device.device_path = device_path;
    m_session_handler = std::make_unique<QMISessionHandler>(device_path);

    // Initialize lifecycle manager for connection registry tracking
    m_lifecycle_manager = std::make_unique<ConnectionLifecycleManager>(
        m_current_device.device_path, "", m_config.apn);


    return true;
}

DeviceInfo ConnectionManager::getCurrentDevice() const {
    return m_current_device;
}

std::string ConnectionManager::getStatusJson() const {
    Json::Value status;
    status["state"] = getStateString();
    status["connected"] = isConnected();
    status["device_path"] = m_current_device.device_path;
    status["device_model"] = m_current_device.model;
    status["device_imei"] = m_current_device.imei;

    Json::StreamWriterBuilder builder;
    return Json::writeString(builder, status);
}

std::string ConnectionManager::getMetricsJson() const {
    ConnectionMetrics metrics = const_cast<ConnectionManager*>(this)->getCurrentMetrics();

    Json::Value json_metrics;
    json_metrics["signal_strength"] = metrics.signal_strength;
    json_metrics["network_type"] = metrics.network_type;
    json_metrics["ip_address"] = metrics.ip_address;
    json_metrics["dns_primary"] = metrics.dns_primary;
    json_metrics["dns_secondary"] = metrics.dns_secondary;
    json_metrics["is_connected"] = metrics.is_connected;
    json_metrics["bytes_sent"] = static_cast<Json::UInt64>(metrics.bytes_sent);
    json_metrics["bytes_received"] = static_cast<Json::UInt64>(metrics.bytes_received);

    Json::StreamWriterBuilder builder;
    return Json::writeString(builder, json_metrics);
}

void ConnectionManager::enableAutoRecovery(bool enable) {
    m_auto_recovery_enabled = enable;
}

void ConnectionManager::transitionToState(ConnectionState new_state, const std::string& reason) {
    {
        std::lock_guard<std::mutex> lock(m_state_mutex);
        ConnectionState old_state = m_current_state;
        m_current_state = new_state;

        std::cout << "State transition: " << static_cast<int>(old_state) 
                  << " -> " << static_cast<int>(new_state) 
                  << " (" << reason << ")" << std::endl;
    }

    notifyStateChange(new_state, reason);
}

void ConnectionManager::notifyStateChange(ConnectionState state, const std::string& reason) {
    if (m_state_callback) {
        m_state_callback(state, reason);
    }
}

void ConnectionManager::notifyMetrics(const ConnectionMetrics& metrics) {
    if (m_metrics_callback) {
        m_metrics_callback(metrics);
    }
}

void ConnectionManager::parseDeviceJson(const std::string& device_json) {
    Json::Value root;
    Json::Reader reader;

    if (!reader.parse(device_json, root)) {
        throw std::runtime_error("Failed to parse device JSON");
    }

    // Try to parse as device list first
    if (root.isMember("devices") && root["devices"].isArray() && !root["devices"].empty()) {
        const Json::Value& device = root["devices"][0];
        m_current_device.device_path = device["device_path"].asString();
        m_current_device.imei = device["imei"].asString();
        m_current_device.model = device["model"].asString();
        m_current_device.manufacturer = device["manufacturer"].asString();
        m_current_device.is_available = device["is_available"].asBool();
    }
    // Try profiles
    else if (root.isMember("profiles") && root["profiles"].isArray() && !root["profiles"].empty()) {
        const Json::Value& profile = root["profiles"][0];
        m_current_device.device_path = profile["path"].asString();
        m_current_device.imei = profile["imei"].asString();
        m_current_device.model = profile["model"].asString();
        m_current_device.is_available = true;
    }
    // Try single device
    else if (root.isMember("device_path")) {
        m_current_device.device_path = root["device_path"].asString();
        m_current_device.imei = root["imei"].asString();
        m_current_device.model = root["model"].asString();
        m_current_device.is_available = true;
    }
    else {
        throw std::runtime_error("No valid device found in JSON");
    }
}

// Hot-disconnect support methods
void ConnectionManager::performEmergencyCleanup() {
    std::cout << "\n=== EMERGENCY CLEANUP INITIATED ===" << std::endl;
    std::cout << "Performing comprehensive emergency cleanup..." << std::endl;

    bool cleanup_success = true;
    int cleaned_interfaces = 0;
    int cleaned_routes = 0;

    // Stop monitoring first to prevent interference
    if (m_monitoring_enabled) {
        std::cout << "Stopping monitoring components..." << std::endl;
        stopMonitoring();
    }

    // Stop QMI session first
    if (m_session_handler) {
        std::cout << "Stopping QMI data session..." << std::endl;
        if (!m_session_handler->stopDataSession()) {
            std::cerr << "Warning: Failed to stop QMI data session" << std::endl;
            cleanup_success = false;
        } else {
            std::cout << "✓ QMI data session stopped successfully" << std::endl;
        }
    }

    if (m_interface_controller) {
        // Get all existing WWAN interfaces (both active and inactive)
        std::vector<std::string> all_interfaces = m_interface_controller->getExistingWWANInterfaces();
        std::cout << "Found " << all_interfaces.size() << " WWAN interfaces to clean up" << std::endl;

        // Clean up routes first (before bringing interfaces down)
        std::cout << "\n--- Cleaning up WWAN routes ---" << std::endl;
        std::vector<std::string> routes_before = m_interface_controller->getActiveRoutes();
        std::cout << "Found " << routes_before.size() << " WWAN routes to remove" << std::endl;

        // Remove all WWAN routes
        if (!m_interface_controller->cleanupAllRoutes()) {
            std::cerr << "Warning: Some routes may not have been cleaned up properly" << std::endl;
            cleanup_success = false;
        }

        // Verify route cleanup
        std::vector<std::string> routes_after = m_interface_controller->getActiveRoutes();
        cleaned_routes = routes_before.size() - routes_after.size();
        if (routes_after.empty()) {
            std::cout << "✓ All " << cleaned_routes << " WWAN routes removed successfully" << std::endl;
        } else {
            std::cout << "⚠ " << routes_after.size() << " routes still remain after cleanup" << std::endl;
            for (const auto& route : routes_after) {
                std::cout << "  Remaining route: " << route << std::endl;
            }
            cleanup_success = false;
        }

        // Clean up interfaces
        std::cout << "\n--- Cleaning up WWAN interfaces ---" << std::endl;
        for (const auto& interface_name : all_interfaces) {
            std::cout << "Cleaning up interface: " << interface_name << std::endl;

            // Check interface status before cleanup
            bool was_active = m_interface_controller->isInterfaceActive(interface_name);
            std::string ip_before = m_interface_controller->parseInterfaceIP(interface_name);

            std::cout << "  Status before cleanup: " << (was_active ? "ACTIVE" : "INACTIVE") 
                      << ", IP: " << (ip_before.empty() ? "none" : ip_before) << std::endl;

            // Perform interface cleanup
            if (m_interface_controller->cleanupInterface(interface_name)) {
                // Verify cleanup
                bool is_active_after = m_interface_controller->isInterfaceActive(interface_name);
                std::string ip_after = m_interface_controller->parseInterfaceIP(interface_name);

                if (!is_active_after && ip_after.empty()) {
                    std::cout << "  ✓ Interface " << interface_name << " cleaned up successfully" << std::endl;
                    cleaned_interfaces++;
                } else {
                    std::cout << "  ⚠ Interface " << interface_name << " cleanup incomplete" << std::endl;
                    std::cout << "    Status after: " << (is_active_after ? "ACTIVE" : "INACTIVE") 
                              << ", IP: " << (ip_after.empty() ? "none" : ip_after) << std::endl;
                    cleanup_success = false;
                }
            } else {
                std::cout << "  ✗ Failed to clean up interface: " << interface_name << std::endl;
                cleanup_success = false;
            }
        }

        // Additional verification: Check for any remaining WWAN interfaces
        std::cout << "\n--- Final verification ---" << std::endl;
        std::vector<std::string> remaining_interfaces = m_interface_controller->getActiveInterfaces();
        if (remaining_interfaces.empty()) {
            std::cout << "✓ No active WWAN interfaces remaining" << std::endl;
        } else {
            std::cout << "⚠ " << remaining_interfaces.size() << " WWAN interfaces still active:" << std::endl;
            for (const auto& iface : remaining_interfaces) {
                std::cout << "  - " << iface << " (status: " 
                          << (m_interface_controller->isInterfaceActive(iface) ? "ACTIVE" : "INACTIVE") << ")" << std::endl;
            }
            cleanup_success = false;
        }

        // Force cleanup any remaining problematic interfaces
        if (!remaining_interfaces.empty()) {
            std::cout << "\n--- Force cleanup of remaining interfaces ---" << std::endl;
            for (const auto& iface : remaining_interfaces) {
                std::cout << "Force bringing down interface: " << iface << std::endl;
                m_interface_controller->bringInterfaceDown(iface);

                // Force flush all addresses
                std::string flush_cmd = "ip addr flush dev " + iface + " 2>/dev/null";
                m_interface_controller->executeCommandSuccess(flush_cmd);

                // Force remove all routes
                std::string route_flush_cmd = "ip route flush dev " + iface + " 2>/dev/null";
                m_interface_controller->executeCommandSuccess(route_flush_cmd);
            }
        }
    }

    // Remove smart routing if configured
    std::cout << "\n--- Removing smart routing ---" << std::endl;
    if (m_interface_controller && m_session_handler) {
        auto settings = m_session_handler->getCurrentSettings();
        if (!settings.ip_address.empty()) {
            std::vector<std::string> interfaces = m_interface_controller->findWWANInterfaces();
            for (const auto& interface_name : interfaces) {
                std::cout << "Removing smart routing for: " << interface_name << std::endl;
                if (m_interface_controller->removeCellularRouting(interface_name)) {
                    std::cout << "✓ Smart routing removed for " << interface_name << std::endl;
                } else {
                    std::cout << "⚠ Failed to remove smart routing for " << interface_name << std::endl;
                    cleanup_success = false;
                }
            }
        }
    }

    // Update lifecycle manager status to inactive
    if (m_lifecycle_manager) {
        std::cout << "Updating lifecycle manager status to inactive..." << std::endl;
        m_lifecycle_manager->updateStatus(false);
    }

    // Update connection state
    transitionToState(ConnectionState::IDLE, "Emergency cleanup completed");

    // Final summary
    std::cout << "\n=== EMERGENCY CLEANUP SUMMARY ===" << std::endl;
    std::cout << "Cleaned interfaces: " << cleaned_interfaces << std::endl;
    std::cout << "Cleaned routes: " << cleaned_routes << std::endl;
    std::cout << "Overall status: " << (cleanup_success ? "SUCCESS" : "PARTIAL SUCCESS") << std::endl;

    if (!cleanup_success) {
        std::cout << "⚠ Some cleanup operations failed. Manual intervention may be required." << std::endl;
        std::cout << "  You can run 'ip link show' and 'ip route show' to check remaining interfaces/routes" << std::endl;
    } else {
        std::cout << "✓ Emergency cleanup completed successfully" << std::endl;
    }

    std::cout << "=== EMERGENCY CLEANUP FINISHED ===" << std::endl;
}

ConnectionManager* ConnectionManager::getActiveInstance() {
    return s_active_instance;
}

// Cellular mode configuration methods
bool ConnectionManager::setCellularMode(const CellularModeConfig& mode_config) {
    if (!m_session_handler) {
        std::cerr << "Session handler not initialized" << std::endl;
        return false;
    }

    std::cout << "Setting cellular mode: " << getCellularModeString(mode_config.mode) << std::endl;

    // Convert CellularMode to ModemTechnology
    ModemTechnology modem_tech;
    switch (mode_config.mode) {
        case CellularMode::AUTO:
            modem_tech = ModemTechnology::AUTOMATIC;
            break;
        case CellularMode::LTE_ONLY:
            modem_tech = ModemTechnology::LTE_ONLY;
            break;
        case CellularMode::FiveG_ONLY:
            modem_tech = ModemTechnology::FIVE_G_ONLY;
            break;
        case CellularMode::ThreeGPP_LEGACY:
            modem_tech = ModemTechnology::THREE_GPP_LEGACY;
            break;
        case CellularMode::WCDMA_GSM:
            modem_tech = ModemTechnology::WCDMA_GSM_AUTO;
            break;
        case CellularMode::GSM_ONLY:
            modem_tech = ModemTechnology::GSM_ONLY;
            break;
        case CellularMode::LTE_FiveG:
            modem_tech = ModemTechnology::LTE_FIVE_G_AUTO;
            break;
        default:
            modem_tech = ModemTechnology::AUTOMATIC;
            break;
    }

    bool success = false;
    if (mode_config.force_mode_selection) {
        success = m_session_handler->enforceNetworkMode(modem_tech);
    } else {
        success = m_session_handler->setCellularMode(modem_tech, mode_config.preferred_bands);
    }

    if (success) {
        // Update config
        std::lock_guard<std::mutex> lock(m_config_mutex);
        m_config.cellular_mode_config = mode_config;
        std::cout << "Cellular mode configuration updated successfully" << std::endl;
    }

    return success;
}

CellularMode ConnectionManager::getCurrentCellularMode() {
    if (!m_session_handler) {
        return CellularMode::AUTO;
    }

    ModemTechnology current_tech = m_session_handler->getCurrentModemTechnology();

    // Convert ModemTechnology to CellularMode
    switch (current_tech) {
        case ModemTechnology::AUTOMATIC:
            return CellularMode::AUTO;
        case ModemTechnology::LTE_ONLY:
            return CellularMode::LTE_ONLY;
        case ModemTechnology::FIVE_G_ONLY:
            return CellularMode::FiveG_ONLY;
        case ModemTechnology::THREE_GPP_LEGACY:
            return CellularMode::ThreeGPP_LEGACY;
        case ModemTechnology::WCDMA_GSM_AUTO:
            return CellularMode::WCDMA_GSM;
        case ModemTechnology::GSM_ONLY:
            return CellularMode::GSM_ONLY;
        case ModemTechnology::LTE_FIVE_G_AUTO:
            return CellularMode::LTE_FiveG;
        default:
            return CellularMode::AUTO;
    }
}

std::string ConnectionManager::getCellularModeString(CellularMode mode) {
    switch (mode) {
        case CellularMode::AUTO: return "Automatic";
        case CellularMode::LTE_ONLY: return "LTE Only";
        case CellularMode::FiveG_ONLY: return "5G Only";
        case CellularMode::ThreeGPP_LEGACY: return "3GPP Legacy";
        case CellularMode::WCDMA_GSM: return "WCDMA/GSM";
        case CellularMode::GSM_ONLY: return "GSM Only";
        case CellularMode::LTE_FiveG: return "LTE/5G";
        default: return "Unknown";
    }
}

bool ConnectionManager::loadCellularConfigFromJson(const Json::Value& config) {
    CellularModeConfig cellular_config;

    if (config.isMember("cellular_mode")) {
        std::string mode_str = config["cellular_mode"].asString();

        // Parse mode string
        if (mode_str == "auto") {
            cellular_config.mode = CellularMode::AUTO;
        } else if (mode_str == "lte_only") {
            cellular_config.mode = CellularMode::LTE_ONLY;
        } else if (mode_str == "5g_only") {
            cellular_config.mode = CellularMode::FiveG_ONLY;
        } else if (mode_str == "3gpp_legacy") {
            cellular_config.mode = CellularMode::ThreeGPP_LEGACY;
        } else if (mode_str == "wcdma_gsm") {
            cellular_config.mode = CellularMode::WCDMA_GSM;
        } else if (mode_str == "gsm_only") {
            cellular_config.mode = CellularMode::GSM_ONLY;
        } else if (mode_str == "lte_5g") {
            cellular_config.mode = CellularMode::LTE_FiveG;
        }

        cellular_config.mode_description = getCellularModeString(cellular_config.mode);
    }

    if (config.isMember("preferred_bands") && config["preferred_bands"].isArray()) {
        for (const auto& band : config["preferred_bands"]) {
            cellular_config.preferred_bands.push_back(band.asInt());
        }
    }

    if (config.isMember("preference_duration")) {
        cellular_config.preference_duration = config["preference_duration"].asInt();
    }

    if (config.isMember("force_mode_selection")) {
        cellular_config.force_mode_selection = config["force_mode_selection"].asBool();
    }

    // Update connection config
    std::lock_guard<std::mutex> lock(m_config_mutex);
    m_config.cellular_mode_config = cellular_config;

    if (config.isMember("enforce_mode_before_connection")) {
        m_config.enforce_mode_before_connection = config["enforce_mode_before_connection"].asBool();
    }

    std::cout << "Loaded cellular configuration: " << cellular_config.mode_description << std::endl;
    return true;
}

// Interface management methods
std::vector<std::string> ConnectionManager::getExistingWWANInterfaces() {
    if (m_interface_controller) {
        return m_interface_controller->getExistingWWANInterfaces();
    }
    return std::vector<std::string>();
}

std::string ConnectionManager::generateUniqueInterfaceName(const std::string& base_name) {
    if (m_interface_controller) {
        return m_interface_controller->generateUniqueWWANName(base_name);
    }
    return base_name + "0";
}

bool ConnectionManager::isInterfaceNameAvailable(const std::string& interface_name) {
    if (m_interface_controller) {
        return !m_interface_controller->isInterfaceNameTaken(interface_name);
    }
    return true;  // Assume available if we can't check
}

// Smart cleanup and optimal interface selection methods
bool ConnectionManager::performStartupCleanup() {
    std::cout << "Performing startup cleanup..." << std::endl;

    if (!m_interface_controller) {
        std::cerr << "Interface controller not initialized" << std::endl;
        return false;
    }

    // Perform smart cleanup of inactive interfaces
    bool cleanup_success = m_interface_controller->performSmartCleanup();

    if (cleanup_success) {
        std::cout << "Startup cleanup completed successfully" << std::endl;
    } else {
        std::cerr << "Startup cleanup completed with warnings" << std::endl;
    }

    return cleanup_success;
}

std::string ConnectionManager::selectOptimalInterface(const std::string& device_path) {
    std::cout << "Selecting optimal interface for device: " << device_path << std::endl;

    if (!m_interface_controller) {
        std::cerr << "Interface controller not initialized" << std::endl;
        return "wwan0";  // Fallback
    }

    // Check for active connected interfaces first
    std::vector<std::string> active_interfaces = getActiveWWANInterfaces();

    if (!active_interfaces.empty()) {
        std::string active_interface = active_interfaces[0];
        std::cout << "Found active interface to reuse: " << active_interface << std::endl;
        return active_interface;
    }

    // Try to find the first available interface (inactive that can be reused)
    std::string available_interface = m_interface_controller->findFirstAvailableInterface("wwan");

    if (!available_interface.empty()) {
        std::cout << "Selected interface: " << available_interface << std::endl;

        // Ensure the interface exists and is properly configured
        if (!m_interface_controller->ensureInterfaceExists(available_interface, device_path)) {
            std::cerr << "Warning: Could not ensure interface exists, proceeding anyway" << std::endl;
        }

        return available_interface;
    }

    // Fallback to generating unique name
    std::string unique_interface = generateUniqueInterfaceName("wwan");
    std::cout << "Generated new interface name: " << unique_interface << std::endl;

    return unique_interface;
}

bool ConnectionManager::cleanupInactiveConnections() {
    std::cout << "Cleaning up inactive connections..." << std::endl;

    if (!m_interface_controller) {
        return false;
    }

    return m_interface_controller->cleanupInactiveInterfaces();
}

std::vector<std::string> ConnectionManager::getActiveWWANInterfaces() {
    std::vector<std::string> active_interfaces;

    if (!m_interface_controller) {
        return active_interfaces;
    }

    std::vector<std::string> all_interfaces = m_interface_controller->getExistingWWANInterfaces();

    for (const auto& interface : all_interfaces) {
        if (m_interface_controller->isInterfaceConnected(interface)) {
            active_interfaces.push_back(interface);
        }
    }

    return active_interfaces;
}

std::vector<std::string> ConnectionManager::getInactiveWWANInterfaces() {
    if (!m_interface_controller) {
        return std::vector<std::string>();
    }

    return m_interface_controller->getInactiveWWANInterfaces();
}