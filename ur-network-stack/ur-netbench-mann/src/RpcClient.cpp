#include "../include/RpcClient.hpp"
#include "../thirdparty/ur-rpc-template/extensions/direct_template.h"
#include <iostream>
#include <cstring>
#include <thread>
#include <chrono>

RpcClient::RpcClient(const std::string& configPath, const std::string& clientId)
    : configPath_(configPath), clientId_(clientId) {
    // Initialize thread manager with configurable pool size
    threadManager_ = std::make_unique<ThreadManager>(10);
    ctx_ = nullptr;
    
    logInfo("RpcClient initialized with config: " + configPath_ + ", client ID: " + clientId_);
}

RpcClient::~RpcClient() {
    if (isRunning()) {
        stop();
    }
    
    if (ctx_) {
        direct_client_thread_destroy(ctx_);
        ctx_ = nullptr;
    }
    
    logInfo("RpcClient destroyed");
}

bool RpcClient::start() {
    if (running_.load()) {
        logInfo("RpcClient is already running");
        return true; // Already running
    }

    try {
        // Create RPC client thread using ThreadManager
        rpcThreadId_ = threadManager_->createThread([this]() {
            this->rpcClientThreadFunc();
        });

        logInfo("Created RPC client thread with ID: " + std::to_string(rpcThreadId_));

        // Wait for thread initialization with timeout
        const int MAX_WAIT_MS = 3000;
        const int POLL_INTERVAL_MS = 100;
        int elapsed = 0;
        
        while (elapsed < MAX_WAIT_MS && !running_.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(POLL_INTERVAL_MS));
            elapsed += POLL_INTERVAL_MS;
        }

        bool success = running_.load();
        if (success) {
            logInfo("RPC client started successfully");
        } else {
            logError("RPC client failed to start within timeout");
        }
        
        return success;
        
    } catch (const std::exception &e) {
        logError("start() failed: " + std::string(e.what()));
        return false;
    }
}

void RpcClient::stop() {
    if (!running_.load()) {
        logInfo("RpcClient is not running");
        return;
    }

    logInfo("Stopping RPC client...");
    
    // Set running flag to false
    running_.store(false);
    
    // Stop the direct client thread
    if (ctx_) {
        direct_client_thread_stop(ctx_);
    }
    
    // Wait for thread to complete
    if (threadManager_ && threadManager_->isThreadAlive(rpcThreadId_)) {
        bool completed = threadManager_->joinThread(rpcThreadId_, std::chrono::seconds(10));
        if (!completed) {
            logError("RPC client thread did not complete within timeout");
            threadManager_->stopThread(rpcThreadId_);
        }
    }
    
    logInfo("RPC client stopped");
}

bool RpcClient::isRunning() const {
    return running_.load();
}

bool RpcClient::isConnected() const {
    if (ctx_) {
        return direct_client_thread_is_connected(ctx_);
    }
    return false;
}

void RpcClient::setMessageHandler(std::function<void(const std::string&, const std::string&)> handler) {
    messageHandler_ = handler;
    logInfo("Message handler set");
}

void RpcClient::sendResponse(const std::string& topic, const std::string& response) {
    if (!running_.load() || !ctx_) {
        logError("Cannot send response - client not running");
        return;
    }

    try {
        // Use the direct client to publish the response
        int result = direct_client_publish_raw_message(topic.c_str(), response.c_str(), response.length());
        if (result != 0) {
            logError("Failed to publish response to topic: " + topic);
        } else {
            logInfo("Published response to topic: " + topic);
        }
    } catch (const std::exception& e) {
        logError("Exception sending response: " + std::string(e.what()));
    }
}

void RpcClient::publishMessage(const std::string& topic, const std::string& message) {
    if (!running_.load() || !ctx_) {
        logError("Cannot publish message - client not running");
        return;
    }

    try {
        // Use the direct client to publish the message
        int result = direct_client_publish_raw_message(topic.c_str(), message.c_str(), message.length());
        if (result != 0) {
            logError("Failed to publish message to topic: " + topic);
        } else {
            logInfo("Published message to topic: " + topic);
        }
    } catch (const std::exception& e) {
        logError("Exception publishing message: " + std::string(e.what()));
    }
}

void RpcClient::getStatistics(direct_client_statistics_t* stats) const {
    if (stats) {
        direct_client_get_statistics(stats);
    }
}

void RpcClient::rpcClientThreadFunc() {
    try {
        // Validate message handler is set
        if (!messageHandler_) {
            logError("No message handler set!");
            running_.store(false);
            return;
        }

        // Create thread context for UR-RPC template
        ctx_ = direct_client_thread_create(configPath_.c_str());
        
        if (!ctx_) {
            logError("Failed to create client thread context");
            running_.store(false);
            return;
        }

        // Set message handler BEFORE starting the thread
        direct_client_set_message_handler(ctx_, staticMessageHandler, this);

        // Start the client thread
        if (direct_client_thread_start(ctx_) != 0) {
            logError("Failed to start client thread");
            direct_client_thread_destroy(ctx_);
            ctx_ = nullptr;
            running_.store(false);
            return;
        }

        // Wait for connection establishment
        if (!direct_client_thread_wait_for_connection(ctx_, 10000)) {
            logError("Connection timeout");
            direct_client_thread_stop(ctx_);
            direct_client_thread_destroy(ctx_);
            ctx_ = nullptr;
            running_.store(false);
            return;
        }

        running_.store(true);
        logInfo("RPC client thread connected and running");

        // Main thread loop
        while (running_.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        // Cleanup
        logInfo("RPC client thread shutting down...");
        if (ctx_) {
            direct_client_thread_stop(ctx_);
            direct_client_thread_destroy(ctx_);
            ctx_ = nullptr;
        }
        
    } catch (const std::exception &e) {
        logError("Thread function error: " + std::string(e.what()));
        running_.store(false);
    }
}

void RpcClient::staticMessageHandler(const char *topic, const char *payload,
                                     size_t payload_len, void *user_data) {
    RpcClient *self = static_cast<RpcClient *>(user_data);
    if (!self || !self->messageHandler_) {
        return;
    }

    // Convert C strings to C++ strings safely
    const std::string topicStr(topic ? topic : "");
    const std::string payloadStr(payload ? payload : "", payload_len);

    // Delegate to instance handler
    self->messageHandler_(topicStr, payloadStr);
}

void RpcClient::logInfo(const std::string& message) const {
    std::cout << "[RpcClient:" << clientId_ << "] " << message << std::endl;
}

void RpcClient::logError(const std::string& message) const {
    std::cerr << "[RpcClient:" << clientId_ << "] ERROR: " << message << std::endl;
}
