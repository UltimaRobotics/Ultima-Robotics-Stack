#include "RpcClient.hpp"
#include "Logger.hpp"
#include <thread>
#include <chrono>

RpcClient::RpcClient(const std::string& configPath, const std::string& clientId)
    : configPath_(configPath), clientId_(clientId) {
    // Initialize thread manager with configurable pool size
    threadManager_ = std::make_unique<ThreadMgr::ThreadManager>(10);
    
    // Initialize UR-RPC library
    library_ = std::make_unique<UrRpc::Library>();
}

RpcClient::~RpcClient() {
    stop();
}

bool RpcClient::start() {
    if (running_.load()) {
        return true; // Already running
    }

    try {
        // Create RPC client configuration
        UrRpc::ClientConfig config;
        
        if (!configPath_.empty()) {
            config.loadFromFile(configPath_);
            LOG_INFO("RPC configuration loaded from: " + configPath_);
        } else {
            // Use default configuration
            config.setBroker("127.0.0.1", 1899)
                  .setClientId(clientId_)
                  .setTimeouts(10, 30)
                  .setReconnect(true, 5, 30);
            LOG_INFO("Using default RPC configuration");
        }
        
        // Create topic configuration
        UrRpc::TopicConfig topicConfig;
        topicConfig.setPrefixes("direct_messaging", clientId_)
                   .setSuffixes("request", "response", "notification");
        
        // Create RPC client
        client_ = std::make_unique<UrRpc::Client>(config, topicConfig);
        LOG_INFO("RPC client created with ID: " + clientId_);
        
        // Set message handler BEFORE starting the client
        client_->setMessageHandler([this](const std::string &topic, const std::string &payload) {
            if (this->messageHandler_) {
                this->messageHandler_(topic, payload);
            }
        });
        
        // Set connection callback
        client_->setConnectionCallback([](UrRpc::ConnectionStatus status) {
            LOG_INFO("RPC connection status: " + UrRpc::connectionStatusToString(status));
        });
        
        // Create RPC client thread using ThreadManager
        rpcThreadId_ = threadManager_->createThread([this]() {
            this->rpcClientThreadFunc();
        });
        
        // Wait for thread initialization with timeout
        const int MAX_WAIT_MS = 3000;
        const int POLL_INTERVAL_MS = 100;
        int elapsed = 0;
        
        while (elapsed < MAX_WAIT_MS && !running_.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(POLL_INTERVAL_MS));
            elapsed += POLL_INTERVAL_MS;
        }

        if (running_.load()) {
            LOG_INFO("RPC client started successfully");
            return true;
        } else {
            LOG_ERROR("RPC client failed to start within timeout");
            return false;
        }
        
    } catch (const std::exception &e) {
        LOG_ERROR("RPC client start failed: " + std::string(e.what()));
        return false;
    }
}

void RpcClient::stop() {
    if (!running_.load()) {
        return; // Already stopped
    }
    
    LOG_INFO("Stopping RPC client...");
    
    // Set running flag to false
    running_.store(false);
    
    // Stop RPC client
    if (client_) {
        try {
            client_->stop();
            client_->disconnect();
        } catch (const std::exception &e) {
            LOG_ERROR("Error stopping RPC client: " + std::string(e.what()));
        }
    }
    
    // Wait for thread to finish
    if (threadManager_ && threadManager_->isThreadAlive(rpcThreadId_)) {
        bool completed = threadManager_->joinThread(rpcThreadId_, std::chrono::seconds(5));
        if (!completed) {
            LOG_WARNING("RPC thread did not complete within 5 seconds");
        }
    }
    
    LOG_INFO("RPC client stopped");
}

bool RpcClient::isRunning() const {
    return running_.load();
}

void RpcClient::setMessageHandler(std::function<void(const std::string&, const std::string&)> handler) {
    messageHandler_ = handler;
}

void RpcClient::sendResponse(const std::string& topic, const std::string& response) {
    if (client_ && running_.load()) {
        try {
            client_->publishMessage(topic, response);
        } catch (const std::exception &e) {
            LOG_ERROR("Failed to send response: " + std::string(e.what()));
        }
    }
}

void RpcClient::rpcClientThreadFunc() {
    try {
        // Validate message handler is set
        if (!messageHandler_) {
            LOG_ERROR("No message handler set for RPC client!");
            running_.store(false);
            return;
        }

        // Connect and start the client
        client_->connect();
        client_->start();
        
        // Wait for connection establishment
        int attempts = 0;
        const int MAX_CONNECTION_ATTEMPTS = 20;
        while (!client_->isConnected() && attempts < MAX_CONNECTION_ATTEMPTS && running_.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            attempts++;
        }
        
        if (!client_->isConnected()) {
            LOG_ERROR("RPC client failed to connect to broker");
            running_.store(false);
            return;
        }
        
        LOG_INFO("RPC client connected to broker");
        running_.store(true);
        
        // Main thread loop
        while (running_.load()) {
            if (!client_->isConnected()) {
                LOG_WARNING("RPC connection lost, waiting for reconnection...");
                // Reconnection is handled automatically by UR-RPC template
                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
    } catch (const std::exception &e) {
        LOG_ERROR("RPC client thread error: " + std::string(e.what()));
        running_.store(false);
    }
}

void RpcClient::staticMessageHandler(const char* topic, const char* payload,
                                     size_t payload_len, void* user_data) {
    RpcClient* self = static_cast<RpcClient*>(user_data);
    if (!self || !self->messageHandler_) {
        return;
    }

    // Convert C strings to C++ strings safely
    const std::string topicStr(topic ? topic : "");
    const std::string payloadStr(payload ? payload : "", payload_len);

    // Delegate to instance handler
    self->messageHandler_(topicStr, payloadStr);
}
