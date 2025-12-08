#include "urwt/rpc/rpc_service.hpp"
#include "urwt/rpc/handlers/list_interfaces_handler.hpp"
#include "urwt/rpc/handlers/scan_networks_handler.hpp"
#include "urwt/rpc/handlers/get_interface_info_handler.hpp"
#include "urwt/rpc/handlers/test_connection_handler.hpp"
#include "urwt/rpc/handlers/get_status_handler.hpp"
#include "urwt/rpc/handlers/shutdown_handler.hpp"
#include "urwt/rpc/handlers/save_network_handler.hpp"
#include "urwt/rpc/handlers/remove_network_handler.hpp"
#include "urwt/rpc/handlers/list_saved_networks_handler.hpp"
#include "urwt/rpc/handlers/set_mode_handler.hpp"
#include "urwt/rpc/handlers/enable_wifi_handler.hpp"
#include "urwt/rpc/handlers/disable_wifi_handler.hpp"
#include "urwt/rpc/handlers/set_automation_handler.hpp"
#include "urwt/rpc/handlers/get_wireless_config_handler.hpp"
#include "urwt/rpc/handlers/update_wireless_config_handler.hpp"
#include "urwt/rpc/handlers/get_system_state_handler.hpp"
#include "urwt/mode/sta_mode_manager.hpp"
#include "urwt/mode/ap_mode_manager.hpp"
#include <iostream>
#include <thread>

namespace urwt {
namespace rpc {

RPCService::RPCService()
    : running_(false)
    , shutdown_requested_(false)
    , start_time_(std::chrono::steady_clock::now()) {}

RPCService::~RPCService() {
    stop();
}

std::optional<std::string> RPCService::initialize(const std::string& configPath,
                                                   const std::string& wirelessConfigPath) {
    config_manager_ = std::make_shared<config::ConfigManager>();
    auto configResult = config_manager_->loadFromFile(configPath);
    if (!configResult.isOk()) {
        return "Failed to load config: " + configResult.error();
    }

    wireless_api_ = std::make_shared<WirelessToolsAPI>();
    thread_manager_ = std::make_shared<ThreadMgr::ThreadManager>();
    operation_tracker_ = std::make_shared<OperationTracker>();
    request_dispatcher_ = std::make_shared<RequestDispatcher>();

    // Initialize wireless configuration manager
    wireless_config_manager_ = std::make_shared<config::WirelessConfigManager>();

    auto wirelessConfigResult = wireless_config_manager_->setConfigFilePath(wirelessConfigPath);
    if (wirelessConfigResult.isError()) {
        return "Failed to set wireless config path: " + wirelessConfigResult.error();
    }

    auto enablePersistResult = wireless_config_manager_->enableAutoPersist(true);
    if (enablePersistResult.isError()) {
        return "Failed to enable auto-persist: " + enablePersistResult.error();
    }

    // Initialize network manager
    network_manager_ = std::make_shared<network::SavedNetworkManager>();

    // Initialize state analyzer
    state_analyzer_ = std::make_shared<state::SystemStateAnalyzer>(wireless_api_);

    // Initialize mode managers and controller
    auto sta_manager = std::make_shared<mode::STAModeManager>(wireless_api_);
    auto ap_manager = std::make_shared<mode::APModeManager>(wireless_api_);
    mode_controller_ = std::make_shared<mode::ModeController>(
        wireless_api_, sta_manager, ap_manager);

    // Load existing configuration (or create default)
    auto loadResult = loadAndApplyConfiguration();
    if (loadResult.has_value()) {
        std::cerr << "Warning: " << *loadResult << std::endl;
        std::cout << "Using default configuration" << std::endl;
    }

    auto interfacesResult = wireless_api_->listInterfaces();
    if (interfacesResult.isOk()) {
        detected_interfaces_ = interfacesResult.value();
        if (detected_interfaces_.size() == 1) {
            single_interface_mode_ = true;
            default_interface_name_ = detected_interfaces_[0].name();
            std::cout << "Single interface mode: using " << default_interface_name_ << " for all operations" << std::endl;
        } else {
            single_interface_mode_ = false;
            std::cout << "Found " << detected_interfaces_.size() << " interfaces" << std::endl;
        }
    }

    try {
        config_path_ = configPath;

        // Initialize the global client FIRST before creating ClientThread
        DirectTemplate::GlobalClient& globalClient = DirectTemplate::GlobalClient::getInstance();
        if (!globalClient.initialize(configPath)) {
            return "Failed to initialize global RPC client";
        }

        rpc_client_ = std::make_unique<DirectTemplate::ClientThread>(configPath);

        DirectTemplate::ReconnectConfig reconnectConfig(10, 5000, true);
        rpc_client_->setReconnectConfig(reconnectConfig);

    } catch (const DirectTemplate::DirectTemplateException& e) {
        return std::string("Failed to create RPC client: ") + e.what();
    } catch (const std::exception& e) {
        return std::string("Failed to create RPC client: ") + e.what();
    }

    setupHandlers();

    auto connectResult = connectToMQTT();
    if (connectResult.has_value()) {
        return "MQTT connection failed: " + *connectResult;
    }

    auto subscribeResult = subscribeToTopics();
    if (subscribeResult.has_value()) {
        return "Topic subscription failed: " + *subscribeResult;
    }

    start_time_ = std::chrono::steady_clock::now();

    return std::nullopt;
}

void RPCService::setupHandlers() {
    const auto& topics = config_manager_->getTopicConfig();
    const auto& broker = config_manager_->getBrokerConfig();
    std::string brokerAddress = broker.host + ":" + std::to_string(broker.port);

    auto listHandler = std::make_shared<ListInterfacesHandler>(wireless_api_);
    request_dispatcher_->registerHandler(RPCAction::ListInterfaces, listHandler);

    auto scanHandler = std::make_shared<ScanNetworksHandler>(
        wireless_api_, thread_manager_, rpc_client_.get(), topics.response_topic, operation_tracker_, this);
    request_dispatcher_->registerHandler(RPCAction::ScanNetworks, scanHandler);

    auto infoHandler = std::make_shared<GetInterfaceInfoHandler>(wireless_api_, this);
    request_dispatcher_->registerHandler(RPCAction::GetInterfaceInfo, infoHandler);

    auto testHandler = std::make_shared<TestConnectionHandler>(
        wireless_api_, thread_manager_, rpc_client_.get(), topics.response_topic, operation_tracker_, this);
    request_dispatcher_->registerHandler(RPCAction::TestConnection, testHandler);

    auto statusHandler = std::make_shared<GetStatusHandler>(
        operation_tracker_, thread_manager_, rpc_client_.get(), brokerAddress);
    statusHandler->setStartTime(start_time_);
    request_dispatcher_->registerHandler(RPCAction::GetStatus, statusHandler);

    auto shutdownHandler = std::make_shared<ShutdownHandler>(
        operation_tracker_, shutdown_requested_);
    request_dispatcher_->registerHandler(RPCAction::Shutdown, shutdownHandler);

    // Network management handlers
    auto saveNetworkHandler = std::make_shared<SaveNetworkHandler>(
        network_manager_, operation_tracker_, this);
    request_dispatcher_->registerHandler(RPCAction::SaveNetwork, saveNetworkHandler);

    auto removeNetworkHandler = std::make_shared<RemoveNetworkHandler>(
        network_manager_, operation_tracker_, this);
    request_dispatcher_->registerHandler(RPCAction::RemoveNetwork, removeNetworkHandler);

    auto listSavedNetworksHandler = std::make_shared<ListSavedNetworksHandler>(
        network_manager_, operation_tracker_, this);
    request_dispatcher_->registerHandler(RPCAction::ListSavedNetworks, listSavedNetworksHandler);

    // Mode control handlers
    auto setModeHandler = std::make_shared<SetModeHandler>(
        mode_controller_, wireless_config_manager_, thread_manager_,
        rpc_client_.get(), topics.response_topic, operation_tracker_, this);
    request_dispatcher_->registerHandler(RPCAction::SetMode, setModeHandler);

    auto enableWifiHandler = std::make_shared<EnableWifiHandler>(
        wireless_config_manager_, state_analyzer_, thread_manager_,
        rpc_client_.get(), topics.response_topic, operation_tracker_, this);
    request_dispatcher_->registerHandler(RPCAction::EnableWifi, enableWifiHandler);

    auto disableWifiHandler = std::make_shared<DisableWifiHandler>(
        wireless_config_manager_, state_analyzer_, thread_manager_,
        rpc_client_.get(), topics.response_topic, operation_tracker_, this);
    request_dispatcher_->registerHandler(RPCAction::DisableWifi, disableWifiHandler);

    // Configuration handlers
    auto setAutomationHandler = std::make_shared<SetAutomationHandler>(
        wireless_config_manager_, operation_tracker_, this);
    request_dispatcher_->registerHandler(RPCAction::SetAutomation, setAutomationHandler);

    auto getWirelessConfigHandler = std::make_shared<GetWirelessConfigHandler>(
        wireless_config_manager_, operation_tracker_, this);
    request_dispatcher_->registerHandler(RPCAction::GetWirelessConfig, getWirelessConfigHandler);

    auto updateWirelessConfigHandler = std::make_shared<UpdateWirelessConfigHandler>(
        wireless_config_manager_, thread_manager_, rpc_client_.get(),
        topics.response_topic, operation_tracker_, this);
    request_dispatcher_->registerHandler(RPCAction::UpdateWirelessConfig, updateWirelessConfigHandler);

    // State handler
    auto getSystemStateHandler = std::make_shared<GetSystemStateHandler>(
        state_analyzer_, operation_tracker_, this);
    request_dispatcher_->registerHandler(RPCAction::GetSystemState, getSystemStateHandler);
}

std::optional<std::string> RPCService::connectToMQTT() {
    try {
        rpc_client_->setMessageHandler([this](const std::string& topic, const std::string& payload) {
            onMessage(topic, payload);
        });

        rpc_client_->setConnectionStatusCallback([](bool connected, const std::string& reason) {
            if (connected) {
                std::cout << "RPC Client connected: " << reason << std::endl;
            } else {
                std::cerr << "RPC Client disconnected: " << reason << std::endl;
            }
        });

        if (!rpc_client_->start()) {
            return "Failed to start RPC client thread";
        }

        int connection_attempts = 0;
        while (!rpc_client_->isConnected() && connection_attempts < 20) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            connection_attempts++;
        }

        if (!rpc_client_->isConnected()) {
            return "Failed to connect to MQTT broker after 20 attempts";
        }

        return std::nullopt;
    } catch (const DirectTemplate::DirectTemplateException& e) {
        return std::string("Connection exception: ") + e.what();
    } catch (const std::exception& e) {
        return std::string("Connection exception: ") + e.what();
    }
}

std::optional<std::string> RPCService::subscribeToTopics() {
    try {
        const auto& topics = config_manager_->getTopicConfig();

        // The global client is already initialized in initialize(), so we can subscribe directly
        rpc_client_->subscribeTopic(topics.request_topic);

        return std::nullopt;
    } catch (const DirectTemplate::DirectTemplateException& e) {
        return std::string("Subscription exception: ") + e.what();
    } catch (const std::exception& e) {
        return std::string("Subscription exception: ") + e.what();
    }
}

void RPCService::onMessage(const std::string& topic, const std::string& payload) {
    auto requestResult = RPCRequest::fromJSON(payload);
    if (!requestResult.isOk()) {
        std::cerr << "Failed to parse RPC request: " << requestResult.error() << std::endl;
        return;
    }

    RPCRequest request = requestResult.value();

    operation_tracker_->startOperation(request.transaction_id, request.action);

    RPCResponse response = request_dispatcher_->dispatch(request);

    RPCAction action = stringToAction(request.action);
    bool isAsync = request_dispatcher_->isHandlerAsync(action);

    if (!isAsync) {
        bool success = !response.isError();
        operation_tracker_->completeOperation(request.transaction_id, success);
    }

    try {
        const auto& topics = config_manager_->getTopicConfig();
        rpc_client_->publishRawMessage(topics.response_topic, response.toJSON());
    } catch (const DirectTemplate::DirectTemplateException& e) {
        std::cerr << "Failed to publish response: " << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Failed to publish response: " << e.what() << std::endl;
    }
}

void RPCService::publishHeartbeat() {
    try {
        const auto& heartbeat = config_manager_->getHeartbeatConfig();
        const auto& broker = config_manager_->getBrokerConfig();

        if (!heartbeat.enabled) {
            return;
        }

        auto now = std::chrono::steady_clock::now();
        auto uptimeSeconds = std::chrono::duration_cast<std::chrono::seconds>(
            now - start_time_).count();

        json payload = json::parse(heartbeat.payload);
        payload["uptime_seconds"] = uptimeSeconds;
        payload["active_operations"] = operation_tracker_->getActiveCount();
        payload["total_processed"] = operation_tracker_->getTotalOperations();
        payload["timestamp"] = std::chrono::system_clock::now().time_since_epoch().count();

        rpc_client_->publishRawMessage(heartbeat.topic, payload.dump());
    } catch (const DirectTemplate::DirectTemplateException& e) {
        std::cerr << "Failed to publish heartbeat: " << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Failed to publish heartbeat: " << e.what() << std::endl;
    }
}

std::optional<std::string> RPCService::run() {
    running_ = true;

    const auto& heartbeat = config_manager_->getHeartbeatConfig();
    auto lastHeartbeat = std::chrono::steady_clock::now();

    while (!shutdown_requested_) {
        auto now = std::chrono::steady_clock::now();

        if (heartbeat.enabled) {
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                now - lastHeartbeat).count();

            if (elapsed >= heartbeat.interval_seconds) {
                publishHeartbeat();
                lastHeartbeat = now;
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    running_ = false;
    return std::nullopt;
}

void RPCService::stop() {
    shutdown_requested_ = true;

    if (rpc_client_ && rpc_client_->isRunning()) {
        try {
            rpc_client_->stop();
        } catch (...) {
        }
    }

    running_ = false;
}

void RPCService::shutdown() {
    stop();
}

bool RPCService::isRunning() const {
    return running_;
}

bool RPCService::isShutdownRequested() const {
    return shutdown_requested_;
}

std::string RPCService::getEffectiveInterface(const std::string& requestedInterface) const {
    if (single_interface_mode_) {
        return default_interface_name_;
    }
    return requestedInterface;
}

uint64_t RPCService::getNextWorkerNumber() {
    return worker_thread_counter_.fetch_add(1);
}

std::optional<std::string> RPCService::loadAndApplyConfiguration() {
    std::string configPath = wireless_config_manager_->getConfigFilePath();

    auto loadResult = wireless_config_manager_->loadFromFile(configPath);

    if (loadResult.isError()) {
        std::cout << "No existing configuration found, creating default..."
                  << std::endl;

        config::WirelessConfig defaultConfig;
        defaultConfig.enabled = true;
        defaultConfig.mode = config::WirelessMode::STA;
        defaultConfig.automation.enabled = true;

        auto saveResult = wireless_config_manager_->saveToFile(configPath);
        if (saveResult.isError()) {
            return "Failed to save default configuration: " + saveResult.error();
        }

        return std::nullopt;
    }

    std::cout << "Loaded wireless configuration from " << configPath << std::endl;

    startup_configurator_ = std::make_shared<config::StartupConfigurator>(
        wireless_api_, mode_controller_, wireless_config_manager_);

    auto config = wireless_config_manager_->getConfig();
    auto applyResult = startup_configurator_->applyConfiguration(config);

    if (applyResult.isError()) {
        return "Failed to apply configuration: " + applyResult.error();
    }

    std::cout << "Wireless configuration applied successfully" << std::endl;
    return std::nullopt;
}

} // namespace rpc
} // namespace urwt