
#include "rpc_client.hpp"
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <chrono>

extern "C" {
#include "../thirdparty/ur-rpc-template/deps/cJSON/cJSON.h"
#include "../thirdparty/ur-rpc-template/extensions/direct_template.h"
#include "../thirdparty/ur-rpc-template/ur-rpc-template.h"
}

RpcClient::RpcClient(const std::string &configPath, const std::string &clientId)
    : configPath_(configPath), clientId_(clientId) {
    // Initialize thread manager
    threadManager_ = std::make_unique<ThreadMgr::ThreadManager>(10);
}

RpcClient::~RpcClient() {
  if (running_.load()) {
    stop();
  }
}

void RpcClient::staticMessageHandler(const char *topic, const char *payload,
                                     size_t payload_len, void *user_data) {
  RpcClient *self = static_cast<RpcClient *>(user_data);
  if (!self || !self->messageHandler_) {
    return;
  }

  const std::string topicStr(topic ? topic : "");
  const std::string payloadStr(payload ? payload : "", payload_len);

  self->messageHandler_(topicStr, payloadStr);
}

void RpcClient::rpcClientThreadFunc() {
    try {
        // Validate message handler is set before starting
        if (!messageHandler_) {
            std::cerr << "[RPC] ERROR: No message handler set!" << std::endl;
            running_.store(false);
            return;
        }

        // Create thread context - this will handle library initialization and client creation
        direct_client_thread_t* ctx = direct_client_thread_create(configPath_.c_str());
        
        if (!ctx) {
            std::cerr << "[RPC] Failed to create client thread context" << std::endl;
            running_.store(false);
            return;
        }

        // CRITICAL: Set message handler BEFORE starting the thread
        // The thread context now stores the handler and will use it when creating the client
        // This ensures the custom handler is set before any messages can arrive
        direct_client_set_message_handler(ctx, staticMessageHandler, this);
        std::cout << "[RPC] Custom message handler registered on thread context" << std::endl;

        // Start the client thread - it will initialize the library, create a client with our custom handler,
        // set it as the global client, connect, and subscribe to topics
        if (direct_client_thread_start(ctx) != 0) {
            std::cerr << "[RPC] Failed to start client thread" << std::endl;
            direct_client_thread_destroy(ctx);
            running_.store(false);
            return;
        }

        // Set running flag immediately after thread starts to prevent premature shutdown
        // The connection will complete asynchronously in the background
        running_.store(true);
        std::cout << "[RPC] Client thread started, waiting for connection..." << std::endl;

        // Wait for connection - give it more time to establish connection
        if (!direct_client_thread_wait_for_connection(ctx, 10000)) {
            std::cerr << "[RPC] Connection timeout" << std::endl;
            direct_client_thread_stop(ctx);
            direct_client_thread_destroy(ctx);
            running_.store(false);
            return;
        }

        std::cout << "[RPC] Connected successfully with custom handler active" << std::endl;

        // Keep the thread running until stop is called
        while (running_.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        // Cleanup - stop the thread context which will clean up its client
        // The thread context sets g_global_client internally, so stopping it will handle cleanup
        direct_client_thread_stop(ctx);
        direct_client_thread_destroy(ctx);
        
        // Note: We don't call direct_client_cleanup_global() here because:
        // 1. The thread context manages its own client lifecycle
        // 2. The thread context sets g_global_client = thread_ctx->client when the client is created
        // 3. The thread context cleans up its client when stopped, which clears g_global_client
        
    } catch (const std::exception &e) {
        std::cerr << "[RPC] Thread function error: " << e.what() << std::endl;
        running_.store(false);
    }
}

bool RpcClient::start() {
  if (running_.load()) {
    return true;
  }

  try {
    // Create and start the RPC client thread using ThreadManager
    rpcThreadId_ = threadManager_->createThread([this]() {
        this->rpcClientThreadFunc();
    });

    // Poll for the running flag with timeout to give connection time to establish
    const int MAX_WAIT_MS = 3000;
    const int POLL_INTERVAL_MS = 100;
    int elapsed = 0;
    
    while (elapsed < MAX_WAIT_MS && !running_.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(POLL_INTERVAL_MS));
        elapsed += POLL_INTERVAL_MS;
    }

    if (!running_.load()) {
        std::cerr << "[RPC] Thread failed to start after " << MAX_WAIT_MS << "ms" << std::endl;
        return false;
    }

    std::cout << "[RPC] Thread started with ID: " << rpcThreadId_ << std::endl;
    return true;
    
  } catch (const std::exception &e) {
    std::cerr << "[RPC] start() failed: " << e.what() << std::endl;
    return false;
  }
}

void RpcClient::stop() {
  if (!running_.load()) {
    return;
  }
  
  try {
    running_.store(false);
    
    // Stop the thread using ThreadManager
    if (rpcThreadId_ > 0) {
        threadManager_->stopThread(rpcThreadId_);
    }
    
    std::cout << "[RPC] Client stopped" << std::endl;
  } catch (const std::exception &e) {
    std::cerr << "[RPC] stop() error: " << e.what() << std::endl;
  }
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
  if (!running_.load()) {
    std::cerr << "[RPC] Cannot send response - client not running" << std::endl;
    return;
  }

  try {
    if (direct_client_publish_raw_message(topic.c_str(), response.c_str(),
                                          response.size()) != 0) {
      std::cerr << "[RPC] Failed to publish message" << std::endl;
    }
  } catch (const std::exception &e) {
    std::cerr << "[RPC] Failed to send response: " << e.what() << std::endl;
  }
}

