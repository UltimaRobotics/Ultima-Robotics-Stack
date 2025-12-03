#include "rpc/rpc_operation_processor.h"
#include <nlohmann/json.hpp>
#include <iostream>
#include <sstream>
#include <cstring>
#include <thread>
#include <stdexcept>

using json = nlohmann::json;

// Include PackageConfig definition from main.cpp
struct PackageConfig {
    std::string device_json;
    std::string cellular_mode;
    std::string timeouts;
    std::string network;
    std::string ip_monitor;
    std::string routing;
    std::string log_file;
    
    bool verbose = false;
    bool enable_monitoring = true;
    bool enable_auto_recovery = true;
    bool verbose_cmd = false;
    bool disable_auto_routing = false;
};

extern "C" {
#include "../ur-rpc-template/extensions/direct_template.h"
}

RpcOperationProcessor::RpcOperationProcessor(const PackageConfig& config, bool verbose)
    : config_(std::make_shared<const PackageConfig>(config))  // Create immutable shared pointer
    , verbose_(verbose)
    , responseTopic_("direct_messaging/ur-qmi-launcher/responses") {
    // Using global ThreadManager from main.cpp
    
    std::cout << "[RPC Processor] Constructor called - this=" << this 
              << ", isShuttingDown=" << isShuttingDown_.load() << std::endl;
    
    if (verbose_) {
        std::cout << "[RPC Processor] Initialized using global ThreadManager" << std::endl;
        std::cout << "[RPC Processor] PackageConfig stored as immutable shared_ptr to prevent corruption" << std::endl;
    }
}

RpcOperationProcessor::~RpcOperationProcessor() {
    std::cout << "[RPC Processor] Destructor called - this=" << this 
              << ", setting shutdown flag" << std::endl;
    
    // Set shutdown flag to prevent new thread creation
    isShuttingDown_.store(true);
    
    if (verbose_) {
        std::cout << "[RPC Processor] Shutting down, waiting for active threads..." << std::endl;
    }
    
    // Join all active threads before cleanup
    std::vector<unsigned int> threadsToJoin;
    {
        std::lock_guard<std::mutex> lock(threadsMutex_);
        threadsToJoin.assign(activeThreads_.begin(), activeThreads_.end());
        if (verbose_ && !threadsToJoin.empty()) {
            std::cout << "[RPC Processor] Waiting for " << threadsToJoin.size() << " active threads to complete" << std::endl;
        }
    }
    
    // Join threads outside the lock to avoid deadlock
    for (unsigned int threadId : threadsToJoin) {
        try {
            if (g_threadManager && g_threadManager->isThreadAlive(threadId)) {
                if (verbose_) {
                    std::cout << "[RPC Processor] Waiting for thread " << threadId << " to complete..." << std::endl;
                }
                
                // Wait indefinitely for thread completion (use very long timeout as fallback)
                bool completed = g_threadManager->joinThread(threadId, std::chrono::minutes(5));
                
                if (!completed) {
                    std::cerr << "[RPC Processor] WARNING: Thread " << threadId 
                              << " did not complete after 5 minutes - potential deadlock" << std::endl;
                } else if (verbose_) {
                    std::cout << "[RPC Processor] Thread " << threadId << " completed successfully" << std::endl;
                }
            }
        } catch (const std::exception& e) {
            if (verbose_) {
                std::cerr << "[RPC Processor] Error joining thread " << threadId << ": " << e.what() << std::endl;
            }
        }
    }
    
    if (verbose_) {
        std::cout << "[RPC Processor] All threads joined, thread manager will clean up automatically" << std::endl;
    }
}

void RpcOperationProcessor::setResponseTopic(const std::string& topic) {
    responseTopic_ = topic;
}

void RpcOperationProcessor::processRequest(const char* payload, size_t payload_len) {
    if (!payload || payload_len == 0) {
        std::cerr << "[RPC Processor] Empty payload received" << std::endl;
        return;
    }

    // Validate payload size (max 1MB to prevent memory issues)
    const size_t MAX_PAYLOAD_SIZE = 1024 * 1024; // 1MB
    if (payload_len > MAX_PAYLOAD_SIZE) {
        std::cerr << "[RPC Processor] Payload too large: " << payload_len 
                  << " bytes (max: " << MAX_PAYLOAD_SIZE << " bytes)" << std::endl;
        return;
    }

    if (verbose_) {
        std::cout << "[RPC Processor] Processing request - payload size: " << payload_len << " bytes" << std::endl;
    }

    try {
        // Parse JSON using nlohmann json
        json root;
        try {
            root = json::parse(payload, payload + payload_len);
        } catch (const json::parse_error& e) {
            std::cerr << "[RPC Processor] JSON parse error: " << e.what() << std::endl;
            return;
        }

        // Extract method and transaction_id
        if (!root.contains("jsonrpc") || !root["jsonrpc"].is_string() || root["jsonrpc"].get<std::string>() != "2.0") {
             std::cerr << "[RPC Processor] Invalid or missing JSON-RPC version" << std::endl;
             return;
        }

        // Extract transaction_id (use "id" for JSON-RPC 2.0)
        std::string transactionId;
        if (root.contains("id")) {
            if (root["id"].is_string()) {
                transactionId = root["id"].get<std::string>();
            } else if (root["id"].is_number()) {
                transactionId = std::to_string(root["id"].get<int>());
            } else {
                transactionId = "unknown";
            }
        } else {
            transactionId = "unknown";
        }

        if (!root.contains("method") || !root["method"].is_string()) {
            sendResponse(transactionId, false, "", "Missing method in request");
            return;
        }

        std::string method = root["method"].get<std::string>();

        if (!root.contains("params") || !root["params"].is_object()) {
            sendResponse(transactionId, false, "", "Missing or invalid params in request");
            return;
        }

        // Serialize to string
        std::string requestJson;
        try {
            requestJson = root.dump();
        } catch (const std::exception& e) {
            std::cerr << "[RPC Processor] Failed to serialize JSON to string: " << e.what() << std::endl;
            return;
        }

        if (verbose_) {
            std::cout << "[RPC Processor] Processing request with ID: " 
                      << transactionId << ", method: " << method << std::endl;
        }

        // Validate global ThreadManager is valid before creating context
        if (!g_threadManager) {
            std::cerr << "[RPC Processor] Global ThreadManager is null, cannot create thread" << std::endl;
            sendResponse(transactionId, false, "", "ThreadManager is not available");
            return;
        }
        
        // Create context with shared_ptr
        auto context = std::make_shared<RequestContext>(
            requestJson,
            transactionId,
            responseTopic_,
            config_,  // Shares ownership of immutable config - thread-safe, no copy
            verbose_,
            &activeThreads_,  // Set activeThreads immediately  
            &threadsMutex_    // Set threadsMutex immediately
        );
        
        // Check if we're shutting down - don't create new threads
        bool shuttingDown = isShuttingDown_.load();
        if (shuttingDown) {
            std::cerr << "[RPC Processor] Cannot create thread - processor is shutting down" << std::endl;
            sendResponse(transactionId, false, "", "Server is shutting down");
            return;
        }
        
        try {
            // Create thread with context - thread ID will be assigned by global ThreadManager
            unsigned int threadId = g_threadManager->createThread([context]() {
                RpcOperationProcessor::processOperationThreadStatic(context);
            });
            
            // CRITICAL: Register thread in activeThreads_ FIRST, before worker can access anything
            {
                std::lock_guard<std::mutex> lock(threadsMutex_);
                activeThreads_.insert(threadId);
            }
            
            // Set thread ID in atomic field for later access
            context->threadId.store(threadId);
            
            // CRITICAL: Publish threadId via promise LAST - this unblocks the worker thread
            context->threadIdPromise->set_value(threadId);

            if (verbose_) {
                std::cout << "[RPC Processor] Thread " << threadId << " created for transaction: " << transactionId << std::endl;
            }
            
            // Periodically cleanup completed threads (every 10th request)
            static std::atomic<int> requestCount{0};
            if (++requestCount % 10 == 0) {
                cleanupCompletedThreads();
            }
            
        } catch (const std::exception& e) {
            std::cerr << "[RPC Processor] Failed to create thread for transaction " << transactionId 
                      << ": " << e.what() << std::endl;
            
            // Fallback: Process synchronously if thread creation fails
            try {
                context->threadIdPromise->set_value(0);
                RpcOperationProcessor::processOperationThreadStatic(context);
                if (verbose_) {
                    std::cout << "[RPC Processor] Synchronous processing completed for transaction " << transactionId << std::endl;
                }
            } catch (const std::exception& syncError) {
                std::cerr << "[RPC Processor] Synchronous processing also failed: " << syncError.what() << std::endl;
                sendResponse(transactionId, false, "", std::string("Processing failed: ") + syncError.what());
            }
            return;
        }

    } catch (const std::exception& e) {
        std::cerr << "[RPC Processor] Exception processing request: " << e.what() << std::endl;
    }
}

void RpcOperationProcessor::processOperationThreadStatic(std::shared_ptr<RequestContext> context) {
    const std::string& requestJson = context->requestJson;
    const std::string& transactionId = context->transactionId;
    const std::string& responseTopic = context->responseTopic;
    std::shared_ptr<const PackageConfig> config = context->config;
    bool verbose = context->verbose;
    
    // CRITICAL: Wait for threadId to be published before accessing it
    unsigned int threadId = context->threadIdFuture.get();
    
    // Extract cleanup info - safe to access now
    std::set<unsigned int>* activeThreads = context->activeThreads;
    std::mutex* threadsMutex = context->threadsMutex;
    
    if (verbose) {
        std::cerr << "[RPC Thread " << threadId << "/" << transactionId << "] Thread started, parsing JSON (size: " << requestJson.size() << " bytes)" << std::endl;
    }

    try {
        // Parse request again in thread context
        json root = json::parse(requestJson);
        if (verbose) {
            std::cerr << "[RPC Thread " << transactionId << "] JSON parsed successfully in thread" << std::endl;
        }

        // Extract method
        if (!root.contains("method") || !root["method"].is_string()) {
            sendResponseStatic(transactionId, false, "", "Missing method in request", responseTopic);
            return;
        }
        std::string method = root["method"].get<std::string>();

        // Extract params - in JSON-RPC 2.0, params is the direct parameters object
        if (!root.contains("params") || !root["params"].is_object()) {
            sendResponseStatic(transactionId, false, "", "Missing or invalid params in request", responseTopic);
            return;
        }
        json paramsObj = root["params"];

        // For now, we'll implement basic QMI operations based on the method
        std::string result;
        bool success = false;

        if (method == "get_status") {
            result = "{\"status\":\"connected\",\"client\":\"ur-qmi-launcher\"}";
            success = true;
        } else if (method == "get_connection_info") {
            result = "{\"interface\":\"wwan0\",\"signal_strength\":-75,\"ip_address\":\"192.168.1.100\"}";
            success = true;
        } else if (method == "ping") {
            result = "{\"pong\":\"ur-qmi-launcher\"}";
            success = true;
        } else {
            result = "";
            success = false;
            sendResponseStatic(transactionId, false, "", "Unknown operation: " + method, responseTopic);
            return;
        }

        if (success) {
            sendResponseStatic(transactionId, true, result, "", responseTopic);
            if (verbose) {
                std::cerr << "[RPC Thread " << transactionId << "] Operation completed successfully, response sent" << std::endl;
            }
        } else {
            sendResponseStatic(transactionId, false, "", "Operation failed", responseTopic);
            if (verbose) {
                std::cerr << "[RPC Thread " << transactionId << "] Operation failed" << std::endl;
            }
        }

    } catch (const std::exception& e) {
        std::cerr << "[RPC Thread " << transactionId << "] Exception in thread: " << e.what() << std::endl;
        sendResponseStatic(transactionId, false, "", std::string("Exception: ") + e.what(), responseTopic);
    }

    // Clean up this thread from tracking before exiting
    if (activeThreads && threadsMutex) {
        std::lock_guard<std::mutex> lock(*threadsMutex);
        activeThreads->erase(threadId);
        if (verbose) {
            std::cerr << "[RPC Thread " << threadId << "/" << transactionId << "] Removed from active threads, remaining: " << activeThreads->size() << std::endl;
        }
    }
    
    if (verbose) {
        std::cerr << "[RPC Thread " << threadId << "/" << transactionId << "] Thread execution completed" << std::endl;
    }
}

void RpcOperationProcessor::sendResponseStatic(const std::string& transactionId, bool success,
                                                 const std::string& result, const std::string& error,
                                                 const std::string& responseTopic) {
    try {
        json response;
        response["jsonrpc"] = "2.0";
        response["id"] = transactionId;
        response["success"] = success;

        if (success) {
            // Add result as object or string
            // If result is JSON string, parse it and include as structured data
            if (!result.empty() && result[0] == '{') {
                try {
                    // Parse the JSON string and include as structured object in response
                    json parsedResult = json::parse(result);
                    response["result"] = parsedResult; // Include as JSON object, not string
                } catch (const json::parse_error& e) {
                    // Not valid JSON, include as string
                    response["result"] = result;
                }
            } else if (!result.empty()) {
                // Non-JSON string result
                response["result"] = result;
            } else {
                // Empty result - use a default success message
                response["result"] = "Operation completed successfully";
            }
            response["message"] = "Operation completed successfully";
        } else {
            // For failure, include error in result field and error message in message field
            response["result"] = "";
            response["message"] = error;
        }

        // Convert to string
        std::string responseJson = response.dump();

        // Publish response
        direct_client_publish_raw_message(responseTopic.c_str(), 
                                         responseJson.c_str(), 
                                         responseJson.size());

    } catch (const std::exception& e) {
        std::cerr << "[RPC Processor] Failed to send response: " << e.what() << std::endl;
    }
}

void RpcOperationProcessor::sendResponse(const std::string& transactionId, bool success,
                                          const std::string& result, const std::string& error) {
    if (verbose_) {
        std::cout << "[RPC Processor] Sending response for transaction: " 
                  << transactionId << std::endl;
    }
    sendResponseStatic(transactionId, success, result, error, responseTopic_);
}

void RpcOperationProcessor::cleanupCompletedThreads() {
    if (!g_threadManager) {
        return;
    }
    
    std::vector<unsigned int> threadsToClean;
    {
        std::lock_guard<std::mutex> lock(threadsMutex_);
        // Find completed threads
        for (unsigned int threadId : activeThreads_) {
            if (!g_threadManager->isThreadAlive(threadId)) {
                threadsToClean.push_back(threadId);
            }
        }
        
        // Remove completed threads from tracking
        for (unsigned int threadId : threadsToClean) {
            activeThreads_.erase(threadId);
        }
        
        if (verbose_ && !threadsToClean.empty()) {
            std::cout << "[RPC Processor] Cleaned up " << threadsToClean.size() 
                      << " completed threads, remaining active: " << activeThreads_.size() << std::endl;
        }
    }
}
