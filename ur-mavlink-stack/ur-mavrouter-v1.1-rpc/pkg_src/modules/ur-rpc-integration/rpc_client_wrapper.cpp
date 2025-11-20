#include "rpc_client_wrapper.hpp"
#include <fstream>
#include <iostream>
#include <chrono>
#include <exception>
#include <thread>
#include "../ur-threadder-api/thirdparty/cJSON.h"
#include "direct_template.h"

namespace URRpcIntegration {

RpcClientWrapper::RpcClientWrapper(const std::string& configPath, 
                                   const std::string& clientId,
                                   ThreadMgr::ThreadManager& threadManager)
    : configPath_(configPath), clientId_(clientId), threadManager_(threadManager) {
    
    // Validate configuration path
    if (configPath_.empty()) {
        throw std::invalid_argument("Configuration path cannot be empty");
    }
    
    if (clientId_.empty()) {
        throw std::invalid_argument("Client ID cannot be empty");
    }
}

RpcClientWrapper::~RpcClientWrapper() {
    stop();
    destroyClientContext();
}

bool RpcClientWrapper::initialize() {
    std::lock_guard<std::mutex> lock(clientMutex_);
    
    if (initialized_.load()) {
        return true; // Already initialized
    }
    
    try {
        // Load and validate configuration
        auto config = RpcConfigLoader::loadConfig(configPath_);
        if (!RpcConfigLoader::validateConfig(config)) {
            std::cerr << "Invalid RPC configuration" << std::endl;
            return false;
        }
        
        // Create client context
        if (!createClientContext()) {
            std::cerr << "Failed to create client context" << std::endl;
            return false;
        }
        
        initialized_.store(true);
        std::cout << "RPC client initialized successfully" << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "Initialization failed: " << e.what() << std::endl;
        return false;
    }
}

bool RpcClientWrapper::start() {
    if (!initialized_.load()) {
        std::cerr << "Client not initialized" << std::endl;
        return false;
    }
    
    if (running_.load()) {
        return true; // Already running
    }
    
    try {
        // Create RPC client thread using ThreadManager
        rpcThreadId_ = threadManager_.createThread([this]() {
            this->rpcClientThreadFunc();
        });
        
        if (rpcThreadId_ == 0) {
            std::cerr << "Failed to create RPC client thread" << std::endl;
            return false;
        }
        
        // Wait for thread to become ready
        std::unique_lock<std::mutex> lock(readyMutex_);
        if (!readyCondition_.wait_for(lock, std::chrono::seconds(10), [this] { return ready_; })) {
            std::cerr << "RPC client thread failed to start within timeout" << std::endl;
            stop();
            return false;
        }
        
        running_.store(true);
        std::cout << "RPC client started successfully" << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "Failed to start RPC client: " << e.what() << std::endl;
        return false;
    }
}

void RpcClientWrapper::stop() {
    if (!running_.load()) {
        return;
    }
    
    running_.store(false);
    
    try {
        // Stop client thread
        if (clientThread_) {
            direct_client_thread_stop(clientThread_);
        }
        
        // Join thread using ThreadManager
        if (rpcThreadId_ != 0) {
            bool joined = threadManager_.joinThread(rpcThreadId_, std::chrono::seconds(5));
            if (!joined) {
                std::cerr << "WARNING: RPC client thread did not complete within timeout" << std::endl;
            }
        }
        
        std::cout << "RPC client stopped" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error stopping RPC client: " << e.what() << std::endl;
    }
}

bool RpcClientWrapper::isRunning() const {
    return running_.load();
}

void RpcClientWrapper::setMessageHandler(MessageHandler handler) {
    messageHandler_ = handler;
    
    if (clientThread_) {
        direct_client_set_message_handler(clientThread_, staticMessageHandler, this);
    }
}

void RpcClientWrapper::sendResponse(const std::string& topic, const std::string& response) {
    if (!running_.load() || !clientThread_) {
        std::cerr << "Cannot send response - client not running" << std::endl;
        return;
    }
    
    try {
        int result = direct_client_publish_raw_message(topic.c_str(), 
                                                       response.c_str(), 
                                                       response.length());
        if (result != UR_RPC_SUCCESS) {
            std::cerr << "Failed to send response: error code " << result << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "Exception sending response: " << e.what() << std::endl;
    }
}

bool RpcClientWrapper::sendRequest(const std::string& method, 
                                   const std::string& service,
                                   const std::string& params,
                                   int authority) {
    if (!running_.load() || !clientThread_) {
        std::cerr << "Cannot send request - client not running" << std::endl;
        return false;
    }
    
    try {
        // Parse params as JSON if provided
        cJSON* paramsJson = nullptr;
        if (!params.empty()) {
            paramsJson = cJSON_Parse(params.c_str());
            if (!paramsJson) {
                std::cerr << "Invalid JSON parameters" << std::endl;
                return false;
            }
        }
        
        int result = direct_client_send_async_rpc(method.c_str(), 
                                                 service.c_str(), 
                                                 paramsJson, 
                                                 static_cast<ur_rpc_authority_t>(authority));
        
        if (paramsJson) {
            cJSON_Delete(paramsJson);
        }
        
        if (result == UR_RPC_SUCCESS) {
            return true;
        } else {
            std::cerr << "Failed to send request: error code " << result << std::endl;
            return false;
        }
    } catch (const std::exception& e) {
        std::cerr << "Exception sending request: " << e.what() << std::endl;
        return false;
    }
}

void RpcClientWrapper::setConnectionStatusCallback(ConnectionStatusCallback callback) {
    connectionCallback_ = callback;
}

ur_rpc_connection_status_t RpcClientWrapper::getConnectionStatus() const {
    if (!clientThread_) {
        return UR_RPC_CONN_DISCONNECTED;
    }
    
    if (direct_client_thread_is_connected(clientThread_)) {
        return UR_RPC_CONN_CONNECTED;
    } else {
        return UR_RPC_CONN_DISCONNECTED;
    }
}

nlohmann::json RpcClientWrapper::getStatistics() const {
    nlohmann::json stats;
    
    stats["client_id"] = clientId_;
    stats["running"] = running_.load();
    stats["initialized"] = initialized_.load();
    stats["connection_status"] = static_cast<int>(getConnectionStatus());
    stats["thread_id"] = rpcThreadId_;
    
    // Get additional statistics from UR-RPC template if available
    direct_client_statistics_t rpc_stats;
    if (direct_client_get_statistics(&rpc_stats) == UR_RPC_SUCCESS) {
        stats["messages_sent"] = rpc_stats.messages_sent;
        stats["messages_received"] = rpc_stats.messages_received;
        stats["requests_sent"] = rpc_stats.requests_sent;
        stats["responses_received"] = rpc_stats.responses_received;
        stats["errors_count"] = rpc_stats.errors_count;
        stats["uptime_seconds"] = rpc_stats.uptime_seconds;
    }
    
    return stats;
}

void RpcClientWrapper::rpcClientThreadFunc() {
    try {
        std::lock_guard<std::mutex> lock(clientMutex_);
        
        if (!clientThread_) {
            std::cerr << "ERROR: Client thread context not created!" << std::endl;
            return;
        }
        
        // Set message handler
        direct_client_set_message_handler(clientThread_, staticMessageHandler, this);
        
        // Start the client thread
        if (direct_client_thread_start(clientThread_) != 0) {
            std::cerr << "Failed to start client thread" << std::endl;
            return;
        }
        
        // Signal that we're ready
        {
            std::lock_guard<std::mutex> readyLock(readyMutex_);
            ready_ = true;
        }
        readyCondition_.notify_all();
        
        // Wait for connection establishment
        waitForConnection();
        
        // Main thread loop
        while (running_.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
            // Check connection status
            if (!direct_client_thread_is_connected(clientThread_)) {
                handleConnectionStatus(false, "Connection lost");
                // Wait for reconnection
                waitForConnection();
            }
        }
        
    } catch (const std::exception& e) {
        std::cerr << "RPC client thread exception: " << e.what() << std::endl;
    }
}

bool RpcClientWrapper::createClientContext() {
    try {
        // Initialize global UR-RPC client
        if (direct_client_init_global(configPath_.c_str()) != UR_RPC_SUCCESS) {
            std::cerr << "Failed to initialize global UR-RPC client" << std::endl;
            return false;
        }
        
        // Create client thread context
        clientThread_ = direct_client_thread_create(configPath_.c_str());
        if (!clientThread_) {
            std::cerr << "Failed to create client thread context" << std::endl;
            direct_client_cleanup_global();
            return false;
        }
        
        // Get global client instance
        client_ = direct_client_get_global();
        if (!client_) {
            std::cerr << "Failed to get global client instance" << std::endl;
            destroyClientContext();
            return false;
        }
        
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "Exception creating client context: " << e.what() << std::endl;
        destroyClientContext();
        return false;
    }
}

void RpcClientWrapper::destroyClientContext() {
    std::lock_guard<std::mutex> lock(clientMutex_);
    
    if (clientThread_) {
        direct_client_thread_destroy(clientThread_);
        clientThread_ = nullptr;
    }
    
    // Clean up global client
    direct_client_cleanup_global();
    client_ = nullptr;
}

void RpcClientWrapper::waitForConnection() {
    if (!clientThread_) {
        return;
    }
    
    // Wait for connection with timeout
    bool connected = direct_client_thread_wait_for_connection(clientThread_, 10000);
    handleConnectionStatus(connected, connected ? "Connected" : "Connection timeout");
}

void RpcClientWrapper::handleConnectionStatus(bool connected, const std::string& reason) {
    if (connectionCallback_) {
        connectionCallback_(connected, reason);
    }
}

void RpcClientWrapper::staticMessageHandler(const char* topic, const char* payload,
                                            size_t payload_len, void* user_data) {
    RpcClientWrapper* self = static_cast<RpcClientWrapper*>(user_data);
    if (!self || !self->messageHandler_) {
        return;
    }
    
    try {
        const std::string topicStr(topic ? topic : "");
        const std::string payloadStr(payload ? payload : "", payload_len);
        
        self->messageHandler_(topicStr, payloadStr);
        
    } catch (const std::exception& e) {
        std::cerr << "Exception in message handler: " << e.what() << std::endl;
    }
}

void RpcClientWrapper::staticConnectionHandler(bool connected, const char* reason, void* user_data) {
    RpcClientWrapper* self = static_cast<RpcClientWrapper*>(user_data);
    if (!self) {
        return;
    }
    
    try {
        const std::string reasonStr(reason ? reason : "");
        self->handleConnectionStatus(connected, reasonStr);
        
    } catch (const std::exception& e) {
        std::cerr << "Exception in connection handler: " << e.what() << std::endl;
    }
}

// RpcClientFactory implementation
std::shared_ptr<RpcClientWrapper> RpcClientFactory::create(const std::string& configPath,
                                                           const std::string& clientId,
                                                           ThreadMgr::ThreadManager& threadManager) {
    try {
        auto client = std::make_shared<RpcClientWrapper>(configPath, clientId, threadManager);
        if (client->initialize()) {
            return client;
        }
        return nullptr;
        
    } catch (const std::exception& e) {
        std::cerr << "Failed to create RPC client: " << e.what() << std::endl;
        return nullptr;
    }
}

// RpcConfigLoader implementation
nlohmann::json RpcConfigLoader::loadConfig(const std::string& configPath) {
    std::ifstream file(configPath);
    if (!file.good()) {
        throw std::runtime_error("Cannot open configuration file: " + configPath);
    }
    
    try {
        nlohmann::json config;
        file >> config;
        return config;
        
    } catch (const nlohmann::json::parse_error& e) {
        throw std::runtime_error("JSON parse error in configuration: " + std::string(e.what()));
    }
}

bool RpcConfigLoader::validateConfig(const nlohmann::json& config) {
    // Check required fields
    if (!config.contains("client_id") || !config["client_id"].is_string()) {
        std::cerr << "Missing or invalid client_id in configuration" << std::endl;
        return false;
    }
    
    if (!config.contains("broker_host") || !config["broker_host"].is_string()) {
        std::cerr << "Missing or invalid broker_host in configuration" << std::endl;
        return false;
    }
    
    if (!config.contains("broker_port") || !config["broker_port"].is_number()) {
        std::cerr << "Missing or invalid broker_port in configuration" << std::endl;
        return false;
    }
    
    // Check optional topic configurations
    if (config.contains("json_added_subs") && config["json_added_subs"].contains("topics")) {
        for (const auto& topic : config["json_added_subs"]["topics"]) {
            if (!topic.is_string()) {
                std::cerr << "Invalid topic in subscription list" << std::endl;
                return false;
            }
        }
    }
    
    return true;
}

nlohmann::json RpcConfigLoader::createDefaultConfig(const std::string& clientId) {
    nlohmann::json config;
    
    config["client_id"] = clientId;
    config["broker_host"] = "127.0.0.1";
    config["broker_port"] = 1899;
    config["keepalive"] = 60;
    config["qos"] = 1;
    config["auto_reconnect"] = true;
    config["reconnect_delay_min"] = 1;
    config["reconnect_delay_max"] = 60;
    config["use_tls"] = false;
    
    // Heartbeat configuration
    config["heartbeat"]["enabled"] = true;
    config["heartbeat"]["interval_seconds"] = 5;
    config["heartbeat"]["topic"] = "clients/" + clientId + "/heartbeat";
    config["heartbeat"]["payload"] = "{\"client\":\"" + clientId + "\",\"status\":\"alive\"}";
    
    // Topic configuration
    config["json_added_pubs"]["topics"] = {"direct_messaging/" + clientId + "/responses"};
    config["json_added_subs"]["topics"] = {"direct_messaging/" + clientId + "/requests"};
    
    return config;
}

} // namespace URRpcIntegration
