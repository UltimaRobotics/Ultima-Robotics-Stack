#include "rpc_operation_processor.hpp"
#include "MavlinkCollectorThread.h"
#include <nlohmann/json.hpp>
#include <iostream>
#include <sstream>
#include <cstring>
#include <thread>
#include <stdexcept>

using json = nlohmann::json;

// Global MAVLink collector thread ID for RPC operations
static unsigned int g_collectorThreadId = 0;

extern "C" {
#include "../../ur-rpc-template/extensions/direct_template.h"
}

RpcOperationProcessor::RpcOperationProcessor(const PackageConfig& config, bool verbose)
    : config_(std::make_shared<const PackageConfig>(config))  // Create immutable shared pointer
    , verbose_(verbose)
    , responseTopic_("direct_messaging/ur-mavcollector/responses") {
    // Initialize thread manager with larger pool to handle concurrent requests
    threadManager_ = std::make_shared<ThreadMgr::ThreadManager>(100);
    
    std::cout << "[RPC Processor] Constructor called - this=" << this 
              << ", isShuttingDown=" << isShuttingDown_.load() << std::endl;
    
    if (verbose_) {
        std::cout << "[RPC Processor] Initialized with thread pool size: 100" << std::endl;
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
            if (threadManager_ && threadManager_->isThreadAlive(threadId)) {
                if (verbose_) {
                    std::cout << "[RPC Processor] Waiting for thread " << threadId << " to complete..." << std::endl;
                }
                
                bool completed = threadManager_->joinThread(threadId, std::chrono::minutes(5));
                
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
            std::cerr << "[RPC Processor]   - Exception id: " << e.id << std::endl;
            std::cerr << "[RPC Processor]   - Byte position: " << e.byte << std::endl;
            return;
        }

        if (verbose_) {
            std::cout << "[RPC Processor] JSON parsed successfully" << std::endl;
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

        size_t jsonStrLen = requestJson.size();
        if (verbose_) {
            std::cout << "[RPC Processor] JSON string length: " << jsonStrLen << " bytes" << std::endl;
        }

        // Validate JSON string size
        const size_t MAX_JSON_SIZE = 512 * 1024; // 512KB
        if (jsonStrLen > MAX_JSON_SIZE) {
            std::cerr << "[RPC Processor] JSON string too large: " << jsonStrLen 
                      << " bytes (max: " << MAX_JSON_SIZE << " bytes)" << std::endl;
            return;
        }

        if (verbose_) {
            std::cout << "[RPC Processor] Processing request with ID: " 
                      << transactionId << ", method: " << method << std::endl;
        }

        // Validate threadManager_ is valid before creating context
        if (!threadManager_) {
            std::cerr << "[RPC Processor] ThreadManager is null, cannot create thread" << std::endl;
            sendResponse(transactionId, false, "", "ThreadManager is not available");
            return;
        }

        // Capture threadManager_ in a local shared_ptr to keep it alive
        auto threadMgr = threadManager_;
        
        // Create context with shared_ptr
        auto context = std::make_shared<RequestContext>(
            requestJson,
            transactionId,
            responseTopic_,
            config_,
            verbose_,
            threadMgr,
            &activeThreads_,
            &threadsMutex_
        );
        
        // Check if we're shutting down - don't create new threads
        bool shuttingDown = isShuttingDown_.load();
        if (verbose_) {
            std::cout << "[RPC Processor] Shutdown flag state: " << (shuttingDown ? "true" : "false") << std::endl;
        }
        if (shuttingDown) {
            std::cerr << "[RPC Processor] Cannot create thread - processor is shutting down" << std::endl;
            sendResponse(transactionId, false, "", "Server is shutting down");
            return;
        }
        
        try {
            // Create thread with context
            unsigned int threadId = threadMgr->createThread([context]() {
                RpcOperationProcessor::processOperationThreadStatic(context);
            });
            
            // Register thread in activeThreads_ FIRST
            {
                std::lock_guard<std::mutex> lock(threadsMutex_);
                activeThreads_.insert(threadId);
            }
            
            // Set thread ID in atomic field
            context->threadId.store(threadId);
            
            // Publish threadId via promise LAST
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

    } catch (const std::bad_alloc& e) {
        std::cerr << "[RPC Processor] CRITICAL: std::bad_alloc caught: " << e.what() << std::endl;
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
    
    // Wait for threadId to be published
    unsigned int threadId = context->threadIdFuture.get();
    
    // Extract cleanup info
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

        // Extract params
        if (!root.contains("params") || !root["params"].is_object()) {
            sendResponseStatic(transactionId, false, "", "Missing or invalid params in request", responseTopic);
            return;
        }
        json paramsObj = root["params"];

        // Process MAVLink-specific operations
        json result;
        bool success = true;
        std::string errorMsg = "";

        if (method == "start_collector") {
            // Start MAVLink collector operation
            if (g_collectorThreadId > 0) {
                result["status"] = "already_running";
                result["thread_id"] = g_collectorThreadId;
                result["message"] = "MAVLink collector is already running";
            } else {
                // Create a running flag for the collector
                static std::atomic<bool> collector_running(true);
                
                g_collectorThreadId = startMavlinkCollector(*config, &collector_running);
                if (g_collectorThreadId > 0) {
                    result["status"] = "started";
                    result["thread_id"] = g_collectorThreadId;
                    result["message"] = "MAVLink collector started successfully";
                } else {
                    success = false;
                    errorMsg = "Failed to start MAVLink collector";
                }
            }
            if (verbose) {
                std::cerr << "[RPC Thread " << transactionId << "] Start collector operation completed" << std::endl;
            }
        } else if (method == "stop_collector") {
            // Stop MAVLink collector operation
            if (g_collectorThreadId > 0) {
                stopMavlinkCollector(g_collectorThreadId);
                result["status"] = "stopped";
                result["thread_id"] = g_collectorThreadId;
                result["message"] = "MAVLink collector stopped successfully";
                g_collectorThreadId = 0;
            } else {
                result["status"] = "not_running";
                result["message"] = "MAVLink collector was not running";
            }
            if (verbose) {
                std::cerr << "[RPC Thread " << transactionId << "] Stop collector operation completed" << std::endl;
            }
        } else if (method == "get_status") {
            // Get collector status operation
            if (g_collectorThreadId > 0) {
                result["status"] = "running";
                result["thread_id"] = g_collectorThreadId;
                result["uptime"] = "unknown";
                result["vehicles_detected"] = 0;
                result["message"] = "MAVLink collector is operational";
            } else {
                result["status"] = "stopped";
                result["message"] = "MAVLink collector is not running";
            }
            if (verbose) {
                std::cerr << "[RPC Thread " << transactionId << "] Get status operation completed" << std::endl;
            }
        } else if (method == "get_vehicle_info") {
            // Get vehicle information operation
            result["vehicles"] = json::array();
            result["message"] = "Vehicle information not yet implemented";
            if (verbose) {
                std::cerr << "[RPC Thread " << transactionId << "] Get vehicle info operation completed" << std::endl;
            }
        } else {
            success = false;
            errorMsg = "Unknown operation: " + method;
        }

        // Send response
        if (success) {
            sendResponseStatic(transactionId, true, result.dump(), "", responseTopic);
            if (verbose) {
                std::cerr << "[RPC Thread " << transactionId << "] Operation completed successfully, response sent" << std::endl;
            }
        } else {
            sendResponseStatic(transactionId, false, "", errorMsg, responseTopic);
            if (verbose) {
                std::cerr << "[RPC Thread " << transactionId << "] Operation failed: " << errorMsg << std::endl;
            }
        }

    } catch (const std::bad_alloc& e) {
        std::cerr << "[RPC Thread " << transactionId << "] CRITICAL: std::bad_alloc: " << e.what() << std::endl;
        sendResponseStatic(transactionId, false, "", "Server error - out of memory", responseTopic);
    } catch (const std::exception& e) {
        std::cerr << "[RPC Thread " << transactionId << "] Exception: " << e.what() << std::endl;
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

        if (success) {
            // Add result as object or string
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

        // Convert to string and publish
        std::string responseJson = response.dump();
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
    if (!threadManager_) {
        return;
    }
    
    std::vector<unsigned int> threadsToClean;
    {
        std::lock_guard<std::mutex> lock(threadsMutex_);
        // Find completed threads
        for (unsigned int threadId : activeThreads_) {
            if (!threadManager_->isThreadAlive(threadId)) {
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
