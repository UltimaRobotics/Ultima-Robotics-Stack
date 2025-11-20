#include "rpc_operation_processor.hpp"
#include "operation_handler.hpp"
#include <nlohmann/json.hpp>
#include <iostream>
#include <sstream>
#include <cstring>
#include <thread>
#include <stdexcept>

using json = nlohmann::json;

extern "C" {
#include "../thirdparty/ur-rpc-template/extensions/direct_template.h"
}

RpcOperationProcessor::RpcOperationProcessor(const PackageConfig& config, bool verbose)
    : config_(std::make_shared<const PackageConfig>(config))  // Create immutable shared pointer
    , verbose_(verbose)
    , responseTopic_("direct_messaging/ur-licence-mann/responses") {
    // Initialize thread manager with larger pool to handle concurrent requests
    // Increased from 20 to 100 to prevent memory allocation failures under load
    // Use shared_ptr for lifetime safety (same pattern as g_rpcClient in main.cpp)
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
    // CRITICAL: Wait indefinitely for all threads to complete to prevent dangling references
    for (unsigned int threadId : threadsToJoin) {
        try {
            if (threadManager_ && threadManager_->isThreadAlive(threadId)) {
                if (verbose_) {
                    std::cout << "[RPC Processor] Waiting for thread " << threadId << " to complete..." << std::endl;
                }
                
                // Wait indefinitely for thread completion (use very long timeout as fallback)
                // This ensures no worker runs after processor teardown, preventing crashes
                bool completed = threadManager_->joinThread(threadId, std::chrono::minutes(5));
                
                if (!completed) {
                    // If thread takes more than 5 minutes, something is seriously wrong
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

        // Launch operation processing in a separate thread
        // Use move semantics to avoid unnecessary copies
        if (verbose_) {
            std::cout << "[RPC Processor] Preparing data for thread processing" << std::endl;
        }

        // Validate size before processing
        if (requestJson.size() > 512 * 1024) {
            std::cerr << "[RPC Processor] Request JSON too large: " << requestJson.size() << " bytes" << std::endl;
            sendResponse(transactionId, false, "", "Request too large");
            return;
        }

        // CRITICAL: Validate threadManager_ is valid before creating context
        if (!threadManager_) {
            std::cerr << "[RPC Processor] ThreadManager is null, cannot create thread" << std::endl;
            sendResponse(transactionId, false, "", "ThreadManager is not available");
            return;
        }

        // Capture threadManager_ in a local shared_ptr to keep it alive
        auto threadMgr = threadManager_;
        
        // Create context with shared_ptr - config_ is already an immutable shared_ptr so no copy occurs
        // Use promise/future pattern to synchronize threadId initialization
        auto context = std::make_shared<RequestContext>(
            requestJson,
            transactionId,
            responseTopic_,
            config_,  // Shares ownership of immutable config - thread-safe, no copy
            verbose_,
            threadMgr,        // Set threadManager immediately
            &activeThreads_,  // Set activeThreads immediately  
            &threadsMutex_    // Set threadsMutex immediately
        );
        
        // Launch operation processing in a separate thread
        if (verbose_) {
            std::cout << "[RPC Processor] Creating thread for transaction: " << transactionId << std::endl;
        }

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
            // Create thread with context - thread ID will be assigned by ThreadManager
            unsigned int threadId = threadMgr->createThread([context]() {
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
            // Worker will wait on the future before accessing threadId
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
            std::cerr << "[RPC Processor] Falling back to synchronous processing for transaction " << transactionId << std::endl;
            
            // Fallback: Process synchronously if thread creation fails
            // Set dummy threadId (0) in promise so worker doesn't block on future
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
        std::cerr << "[RPC Processor] CRITICAL: std::bad_alloc caught in outer try block: " << e.what() << std::endl;
        std::cerr << "[RPC Processor]   - Payload length: " << payload_len << " bytes" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[RPC Processor] Exception processing request: " << e.what() << std::endl;
        std::cerr << "[RPC Processor]   - Exception type: " << typeid(e).name() << std::endl;
    }
}

void RpcOperationProcessor::processOperationThreadStatic(std::shared_ptr<RequestContext> context) {
    const std::string& requestJson = context->requestJson;
    const std::string& transactionId = context->transactionId;
    const std::string& responseTopic = context->responseTopic;
    std::shared_ptr<const PackageConfig> config = context->config;
    bool verbose = context->verbose;
    
    // CRITICAL: Wait for threadId to be published before accessing it
    // This blocks until the main thread has registered us in activeThreads_
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

        // Build operation config
        OperationConfig opConfig;

        // Map method to operation type
        if (method == "verify") {
            opConfig.operation = urlic::OperationType::VERIFY;
        } else if (method == "update") {
            opConfig.operation = urlic::OperationType::UPDATE;
        } else if (method == "get_license_info") {
            opConfig.operation = urlic::OperationType::GET_LICENSE_INFO;
        } else if (method == "get_license_plan") {
            opConfig.operation = urlic::OperationType::GET_LICENSE_PLAN;
        } else if (method == "get_license_definitions") {
            opConfig.operation = urlic::OperationType::GET_LICENSE_DEFINITIONS;
        } else if (method == "update_license_definitions") {
            opConfig.operation = urlic::OperationType::UPDATE_LICENSE_DEFINITIONS;
        } else {
            sendResponseStatic(transactionId, false, "", "Unknown operation: " + method, responseTopic);
            return;
        }

        // Extract parameters directly from params object (JSON-RPC 2.0 format)
        if (verbose) {
            std::cerr << "[RPC Thread " << transactionId << "] Extracting parameters" << std::endl;
        }

        int paramCount = 0;
        for (auto const& [key, val] : paramsObj.items()) {
            // Skip license_file parameter - always use package config path
            if (key == "license_file") {
                if (verbose) {
                    std::cerr << "[RPC Thread " << transactionId << "] Ignoring 'license_file' parameter - using package config path" << std::endl;
                }
                continue;
            }
            
            try {
                if (val.is_string()) {
                    opConfig.parameters[key] = val.get<std::string>();
                } else if (val.is_number()) {
                    opConfig.parameters[key] = std::to_string(val.get<int>());
                } else if (val.is_boolean()) {
                    opConfig.parameters[key] = val.get<bool>() ? "true" : "false";
                }
                paramCount++;
            } catch (const std::bad_alloc& e) {
                std::cerr << "[RPC Thread " << transactionId << "] MEMORY ALLOCATION FAILED while adding parameter '" 
                          << key << "': " << e.what() << std::endl;
                sendResponseStatic(transactionId, false, "", "Server error - out of memory during parameter extraction", responseTopic);
                return;
            }
        }

        // Capture stdout for operations that output JSON data
        // Operations like get_license_info, get_license_plan, get_license_definitions output JSON to stdout
        bool shouldCaptureOutput = (opConfig.operation == urlic::OperationType::GET_LICENSE_INFO ||
                                    opConfig.operation == urlic::OperationType::GET_LICENSE_PLAN ||
                                    opConfig.operation == urlic::OperationType::GET_LICENSE_DEFINITIONS);
        
        // Log verbose information to stderr to avoid interfering with stdout capture
        if (verbose) {
            std::cerr << "[RPC Thread " << transactionId << "] Executing operation: " << method << std::endl;
            std::cerr << "[RPC Thread " << transactionId << "] Parameters extracted: " << paramCount << std::endl;
            for (const auto& [key, value] : opConfig.parameters) {
                std::cerr << "[RPC Thread " << transactionId << "]   " << key << " = " << value << std::endl;
            }
            if (shouldCaptureOutput) {
                std::cerr << "[RPC Thread " << transactionId << "] Capturing stdout for operation output" << std::endl;
            }
        }
        
        std::stringstream capturedOutput;
        std::streambuf* originalCoutBuf = nullptr;
        
        if (shouldCaptureOutput) {
            // Redirect stdout to capture operation output
            originalCoutBuf = std::cout.rdbuf();
            std::cout.rdbuf(capturedOutput.rdbuf());
        }

        // Execute the operation - dereference shared_ptr to get const PackageConfig&
        int exitCode = 0;
        try {
            exitCode = OperationHandler::execute(opConfig, *config, verbose);
        } catch (...) {
            // Restore stdout if we redirected it
            if (shouldCaptureOutput && originalCoutBuf) {
                std::cout.rdbuf(originalCoutBuf);
            }
            throw; // Re-throw to be caught by outer catch block
        }

        // Restore stdout after operation completes
        if (shouldCaptureOutput && originalCoutBuf) {
            std::cout.rdbuf(originalCoutBuf);
        }

        // Process the result
        if (exitCode == 0) {
            // Success - prepare result
            std::string result;
            
            if (shouldCaptureOutput) {
                // Get captured output and parse it as JSON if possible
                std::string outputStr = capturedOutput.str();
                // Remove any trailing whitespace/newlines
                while (!outputStr.empty() && (outputStr.back() == '\n' || outputStr.back() == '\r' || outputStr.back() == ' ')) {
                    outputStr.pop_back();
                }
                
                if (!outputStr.empty()) {
                    // Try to parse as JSON to validate and include as structured data
                    try {
                        json outputJson = json::parse(outputStr);
                        result = outputJson.dump(); // Valid JSON - use it directly
                    } catch (const json::parse_error&) {
                        // Not valid JSON, include as string
                        result = outputStr;
                    }
                } else {
                    result = "Operation completed successfully";
                }
            } else {
                // For operations that don't output JSON, use a simple success message
                result = "Operation completed successfully";
            }

            // Send response with captured output
            sendResponseStatic(transactionId, true, result, "", responseTopic);
            
            if (verbose) {
                std::cerr << "[RPC Thread " << transactionId << "] Operation completed successfully, response sent" << std::endl;
            }
        } else {
            // Operation failed - send error response
            std::string errorMsg = "Operation failed with exit code: " + std::to_string(exitCode);
            
            // If we captured output, include it in the error message for debugging
            if (shouldCaptureOutput) {
                std::string outputStr = capturedOutput.str();
                if (!outputStr.empty()) {
                    errorMsg += ". Output: " + outputStr;
                }
            }
            
            sendResponseStatic(transactionId, false, "", errorMsg, responseTopic);
            
            if (verbose) {
                std::cerr << "[RPC Thread " << transactionId << "] Operation failed with exit code: " << exitCode << std::endl;
            }
        }

    } catch (const std::bad_alloc& e) {
        std::cerr << "[RPC Thread " << transactionId << "] CRITICAL: std::bad_alloc in thread: " << e.what() << std::endl;
        std::cerr << "[RPC Thread " << transactionId << "]   - Request JSON size: " << requestJson.size() << " bytes" << std::endl;
        sendResponseStatic(transactionId, false, "", "Server error - out of memory during operation processing", responseTopic);
    } catch (const std::exception& e) {
        std::cerr << "[RPC Thread " << transactionId << "] Exception in thread: " << e.what() << std::endl;
        std::cerr << "[RPC Thread " << transactionId << "]   - Exception type: " << typeid(e).name() << std::endl;
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
