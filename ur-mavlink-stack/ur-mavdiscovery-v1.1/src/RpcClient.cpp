#include "RpcClient.hpp"
#include "Logger.hpp"
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <chrono>
#include "nlohmann/json.hpp"

using json = nlohmann::json;

// Static instance for message callback
static RpcClient* g_instance = nullptr;

RpcClient::RpcClient(const std::string &configPath, const std::string &clientId)
    : configPath_(configPath), clientId_(clientId) {
    // Initialize thread manager
    threadManager_ = std::make_shared<ThreadMgr::ThreadManager>(10);
    
    // Set static instance for callback
    g_instance = this;
    
    LOG_INFO("RpcClient initialized with ur-threadder-api thread manager and C++ wrapper");
}

RpcClient::~RpcClient() {
  if (running_.load()) {
    stop();
  }
  
  g_instance = nullptr;
  LOG_INFO("RpcClient cleaned up");
}

void RpcClient::messageCallbackWrapper(const std::string& topic, const std::string& payload) {
    if (g_instance && g_instance->messageHandler_) {
        g_instance->messageHandler_(topic, payload);
    }
}

void RpcClient::rpcClientThreadFunc() {
    try {
        // Validate message handler is set before starting
        if (!messageHandler_) {
            LOG_ERROR("[RPC] ERROR: No message handler set!");
            running_.store(false);
            return;
        }

        // Create configuration from file
        clientConfig_ = std::make_unique<UrRpc::ClientConfig>();
        clientConfig_->loadFromFile(configPath_);
        
        // Override client ID if provided
        if (!clientId_.empty()) {
            clientConfig_->setClientId(clientId_);
        }

        // Create topic configuration for fixed topics (no transaction ID)
        topicConfig_ = std::make_unique<UrRpc::TopicConfig>();
        topicConfig_->setPrefixes("direct_messaging", "ur-mavrouter");
        topicConfig_->setSuffixes("requests", "responses", "notifications");
        
        // Manually set include_transaction_id to false for fixed topics
        ur_rpc_topic_config_t* config = const_cast<ur_rpc_topic_config_t*>(topicConfig_->get());
        if (config) {
            config->include_transaction_id = false;
        }

        // Create and configure client
        urpcClient_ = std::make_unique<UrRpc::Client>(*clientConfig_, *topicConfig_);
        
        // Set message handler
        urpcClient_->setMessageHandler(messageCallbackWrapper);

        // Connect and start client
        urpcClient_->connect();
        urpcClient_->start();

        running_.store(true);
        LOG_INFO("[RPC] Connected successfully with C++ wrapper and fixed topics");

        // Keep the thread running until stop is called
        while (running_.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        // Cleanup
        urpcClient_->stop();
        urpcClient_->disconnect();
        
    } catch (const std::exception &e) {
        LOG_ERROR("[RPC] Exception in RPC client thread: " + std::string(e.what()));
        running_.store(false);
    }
}

bool RpcClient::start() {
    if (running_.load()) {
        LOG_WARNING("[RPC] Client already running");
        return true;
    }

    try {
        // Start RPC client thread
        rpcThreadId_ = threadManager_->createThread([this]() {
            this->rpcClientThreadFunc();
        });

        LOG_INFO("[RPC] Client thread started with ID: " + std::to_string(rpcThreadId_));
        return true;

    } catch (const std::exception &e) {
        LOG_ERROR("[RPC] Failed to start RPC client: " + std::string(e.what()));
        return false;
    }
}

void RpcClient::stop() {
    if (!running_.load()) {
        return;
    }

    running_.store(false);
    
    // Wait for thread to finish
    if (rpcThreadId_ != 0) {
        try {
            threadManager_->joinThread(rpcThreadId_, std::chrono::seconds(5));
        } catch (const std::exception &e) {
            LOG_WARNING("[RPC] Failed to join RPC client thread: " + std::string(e.what()));
        }
        rpcThreadId_ = 0;
    }

    LOG_INFO("[RPC] Client stopped");
}

bool RpcClient::isRunning() const {
    return running_.load();
}

void RpcClient::setMessageHandler(
    std::function<void(const std::string &, const std::string &)> handler) {
  messageHandler_ = std::move(handler);
}

void RpcClient::sendResponse(const std::string &topic,
                             const std::string &response) {
  if (!running_.load() || !urpcClient_) {
    LOG_ERROR("[RPC] Cannot send response - client not running");
    return;
  }

  try {
    urpcClient_->publishMessage(topic, response);
  } catch (const std::exception &e) {
    LOG_ERROR("[RPC] Failed to send response: " + std::string(e.what()));
  }
}

void RpcClient::sendRpcRequest(const std::string& service, const std::string& method, 
                               const std::string& paramsJson) {
    if (!running_.load() || !urpcClient_) {
        LOG_ERROR("[RPC] Cannot send RPC request - client not running");
        return;
    }

    try {
        LOG_INFO("[RPC] sendRpcRequest called for service: " + service + ", method: " + method);

        // Create full JSON-RPC request
        json request;
        request["jsonrpc"] = "2.0";
        request["method"] = method;
        request["service"] = service;
        request["authority"] = "USER";
        request["id"] = generateTransactionId();
        request["params"] = json::parse(paramsJson);

        std::string requestPayload = request.dump();

        // Determine the correct topic based on service
        std::string targetTopic;
        if (service == "ur-mavrouter") {
            targetTopic = "direct_messaging/ur-mavrouter/requests";
            LOG_INFO("[RPC] Using direct topic for ur-mavrouter: " + targetTopic);
        } else if (service == "ur-mavcollector") {
            targetTopic = "direct_messaging/ur-mavcollector/requests";
            LOG_INFO("[RPC] Using direct topic for ur-mavcollector: " + targetTopic);
        } else {
            // For other services, use the default ur-rpc-template topic generation
            LOG_INFO("[RPC] Using ur-rpc-template topic generation for service: " + service);
            UrRpc::Request urpcRequest;
            urpcRequest.setMethod(method, service)
                      .setAuthority(UrRpc::Authority::User)
                      .setParams(UrRpc::JsonValue(paramsJson));

            urpcClient_->callAsync(urpcRequest, [](bool success, const UrRpc::JsonValue& /* result */, 
                                                  const std::string& error_message, int error_code) {
                if (!success) {
                    LOG_WARNING("[RPC] Async request failed: " + error_message + " (code: " + std::to_string(error_code) + ")");
                }
            });

            LOG_INFO("[RPC] RPC request sent via ur-rpc-template: " + method + " to " + service);
            return;
        }

        // Publish directly to the specified topic
        LOG_INFO("[RPC] Publishing directly to topic: " + targetTopic);
        urpcClient_->publishMessage(targetTopic, requestPayload);
        LOG_INFO("[RPC] RPC request sent to topic " + targetTopic + ": " + method + " to " + service);

    } catch (const std::exception &e) {
        LOG_ERROR("[RPC] Failed to send RPC request: " + std::string(e.what()));
    }
}

std::string RpcClient::generateTransactionId() {
    // Generate a simple UUID-like transaction ID
    static uint64_t counter = 0;
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    return std::to_string(timestamp) + "-" + std::to_string(++counter);
}
