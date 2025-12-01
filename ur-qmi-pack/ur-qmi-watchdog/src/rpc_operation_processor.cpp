#include "rpc_operation_processor.hpp"
#include <iostream>
#include <sstream>
#include <thread>
#include <chrono>

// Use nlohmann::json for JSON parsing (already included in qmi_watchdog.cpp)
#include <nlohmann/json.hpp>
using json = nlohmann::json;

extern "C" {
#include "../ur-rpc-template/deps/cJSON/cJSON.h"
#include "../ur-rpc-template/extensions/direct_template.h"
#include "../ur-rpc-template/ur-rpc-template.h"
}

RpcOperationProcessor::RpcOperationProcessor(bool verbose)
    : verbose_(verbose) {
    // Initialize thread manager for concurrent operations
    threadManager_ = std::make_shared<ThreadMgr::ThreadManager>(50);
}

RpcOperationProcessor::~RpcOperationProcessor() {
    // Set shutdown flag to prevent new thread creation
    isShuttingDown_.store(true);
    
    // Collect all active threads for stopping
    std::vector<unsigned int> threadsToStop;
    {
        std::lock_guard<std::mutex> lock(threadsMutex_);
        threadsToStop.assign(activeThreads_.begin(), activeThreads_.end());
    }
    
    // Stop all threads
    for (unsigned int threadId : threadsToStop) {
        if (threadManager_->isThreadAlive(threadId)) {
            threadManager_->stopThread(threadId);
        }
    }
}

void RpcOperationProcessor::processRequest(const char* payload, size_t payload_len) {
    // Input validation
    if (!payload || payload_len == 0) {
        std::cerr << "[RPC] Empty payload received" << std::endl;
        return;
    }

    // Size validation (prevent memory exhaustion)
    const size_t MAX_PAYLOAD_SIZE = 1024 * 1024; // 1MB
    if (payload_len > MAX_PAYLOAD_SIZE) {
        std::cerr << "[RPC] Payload too large: " << payload_len << " bytes" << std::endl;
        return;
    }

    try {
        // JSON parsing
        json root = json::parse(payload, payload + payload_len);

        // JSON-RPC 2.0 validation
        if (!root.contains("jsonrpc") || root["jsonrpc"].get<std::string>() != "2.0") {
            std::cerr << "[RPC] Invalid or missing JSON-RPC version" << std::endl;
            return;
        }

        // Extract transaction ID
        std::string transactionId;
        if (root.contains("id")) {
            if (root["id"].is_string()) {
                transactionId = root["id"].get<std::string>();
            } else if (root["id"].is_number()) {
                transactionId = std::to_string(root["id"].get<int>());
            }
        }

        // Extract method
        if (!root.contains("method") || !root["method"].is_string()) {
            sendResponse(transactionId, false, "", "Missing method in request");
            return;
        }
        std::string method = root["method"].get<std::string>();

        // Extract parameters
        if (!root.contains("params") || !root["params"].is_object()) {
            sendResponse(transactionId, false, "", "Missing or invalid params in request");
            return;
        }

        // Create processing context
        auto context = std::make_shared<RequestContext>(
            std::string(payload, payload_len),
            transactionId,
            responseTopic_,
            verbose_
        );

        // Check shutdown state
        bool shuttingDown = isShuttingDown_.load();
        if (shuttingDown) {
            sendResponse(transactionId, false, "", "Server is shutting down");
            return;
        }

        try {
            // Create processing thread
            unsigned int threadId = threadManager_->createThread([context]() {
                RpcOperationProcessor::processOperationThreadStatic(context);
            });
            
            // Register thread in tracking set
            {
                std::lock_guard<std::mutex> lock(threadsMutex_);
                activeThreads_.insert(threadId);
            }

            if (verbose_) {
                std::cout << "[RPC] Launched operation thread " << threadId 
                         << " for method: " << method << std::endl;
            }

        } catch (const std::exception& e) {
            std::cerr << "[RPC] Failed to create thread: " << e.what() << std::endl;
            
            // Fallback to synchronous processing
            RpcOperationProcessor::processOperationThreadStatic(context);
        }

    } catch (const json::parse_error& e) {
        std::cerr << "[RPC] JSON parse error: " << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[RPC] Exception processing request: " << e.what() << std::endl;
    }
}

void RpcOperationProcessor::setResponseTopic(const std::string& topic) {
    responseTopic_ = topic;
}

void RpcOperationProcessor::processOperationThreadStatic(std::shared_ptr<RequestContext> context) {
    if (!context) {
        return;
    }
    
    try {
        // Parse request in thread context
        json root = json::parse(context->requestJson);
        
        // Extract method and parameters
        std::string method = root["method"].get<std::string>();
        json paramsObj = root["params"];
        
        // For now, just acknowledge the operation without specific handling
        std::string result = "QMI Watchdog RPC operation '" + method + "' received successfully";
        
        if (context->verbose) {
            std::cout << "[RPC] Processing operation: " << method << std::endl;
        }
        
        // Send success response
        sendResponseStatic(context->transactionId, true, result, "", context->responseTopic);
        
    } catch (const std::exception& e) {
        sendResponseStatic(context->transactionId, false, "", 
                          std::string("Exception: ") + e.what(), 
                          context->responseTopic);
    }
}

void RpcOperationProcessor::sendResponse(const std::string& transactionId, bool success, 
                                         const std::string& result, const std::string& error) {
    sendResponseStatic(transactionId, success, result, error, responseTopic_);
}

void RpcOperationProcessor::sendResponseStatic(const std::string& transactionId, bool success,
                                               const std::string& result, const std::string& error,
                                               const std::string& responseTopic) {
    try {
        json response;
        response["jsonrpc"] = "2.0";
        response["id"] = transactionId;

        if (success) {
            // Handle JSON result parsing
            if (!result.empty() && result[0] == '{') {
                try {
                    json parsedResult = json::parse(result);
                    response["result"] = parsedResult;
                } catch (const json::parse_error&) {
                    response["result"] = result;
                }
            } else if (!result.empty()) {
                response["result"] = result;
            } else {
                response["result"] = "Operation completed successfully";
            }
        } else {
            // Error response format
            json errorObj;
            errorObj["code"] = -1;
            errorObj["message"] = error;
            response["error"] = errorObj;
        }

        // Publish response
        std::string responseJson = response.dump();
        direct_client_publish_raw_message(responseTopic.c_str(), 
                                         responseJson.c_str(), 
                                         responseJson.size());

    } catch (const std::exception& e) {
        std::cerr << "[RPC] Failed to send response: " << e.what() << std::endl;
    }
}

void RpcOperationProcessor::cleanupThreadTracking(unsigned int threadId) {
    std::lock_guard<std::mutex> lock(threadsMutex_);
    activeThreads_.erase(threadId);
}
