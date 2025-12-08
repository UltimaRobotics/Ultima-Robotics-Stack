#include "RpcOperationProcessor.h"
#include "RpcClient.h"
#include "NetworkCollectorThread.h"
#include <iostream>
#include <sstream>
#include <thread>
#include <chrono>

// C includes for ur-rpc-template
extern "C" {
#include "../thirdparty/ur-rpc-template/deps/cJSON/cJSON.h"
#include "../thirdparty/ur-rpc-template/extensions/direct_template.h"
#include "../thirdparty/ur-rpc-template/ur-rpc-template.h"
}

NetworkCollectorConfig NetworkCollectorConfig::from_json(const json& j) {
    NetworkCollectorConfig config;
    
    if (j.contains("collectVlan")) config.collectVlan = j["collectVlan"];
    if (j.contains("collectNat")) config.collectNat = j["collectNat"];
    if (j.contains("collectFirewall")) config.collectFirewall = j["collectFirewall"];
    if (j.contains("collectRoutes")) config.collectRoutes = j["collectRoutes"];
    if (j.contains("collectBridges")) config.collectBridges = j["collectBridges"];
    if (j.contains("collectAll")) config.collectAll = j["collectAll"];
    if (j.contains("outputText")) config.outputText = j["outputText"];
    if (j.contains("quietMode")) config.quietMode = j["quietMode"];
    if (j.contains("outputFile")) config.outputFile = j["outputFile"];
    if (j.contains("collectionInterval")) config.collectionInterval = j["collectionInterval"];
    
    return config;
}

json NetworkCollectorConfig::to_json() const {
    json j;
    j["collectVlan"] = collectVlan;
    j["collectNat"] = collectNat;
    j["collectFirewall"] = collectFirewall;
    j["collectRoutes"] = collectRoutes;
    j["collectBridges"] = collectBridges;
    j["collectAll"] = collectAll;
    j["outputText"] = outputText;
    j["quietMode"] = quietMode;
    j["outputFile"] = outputFile;
    j["collectionInterval"] = collectionInterval;
    return j;
}

RpcOperationProcessor::RpcOperationProcessor(const NetworkCollectorConfig& config, bool verbose)
    : config_(config)
    , verbose_(verbose) {
    
    // Initialize thread manager with configurable pool size
    threadManager_ = std::make_shared<ThreadMgr::ThreadManager>(50);
    
    if (verbose_) {
        std::cout << "[RPC Processor] Initialized with thread pool size: 50" << std::endl;
    }
}

RpcOperationProcessor::~RpcOperationProcessor() {
    shutdown();
}

RpcOperationProcessor::RpcOperationProcessor(RpcOperationProcessor&& other) noexcept
    : threadManager_(std::move(other.threadManager_))
    , activeThreads_(std::move(other.activeThreads_))
    , isShuttingDown_(other.isShuttingDown_.load())
    , config_(std::move(other.config_))
    , verbose_(other.verbose_)
    , responseTopic_(std::move(other.responseTopic_))
    , rpcClient_(std::move(other.rpcClient_)) {
}

RpcOperationProcessor& RpcOperationProcessor::operator=(RpcOperationProcessor&& other) noexcept {
    if (this != &other) {
        shutdown();
        
        threadManager_ = std::move(other.threadManager_);
        activeThreads_ = std::move(other.activeThreads_);
        isShuttingDown_ = other.isShuttingDown_.load();
        config_ = std::move(other.config_);
        verbose_ = other.verbose_;
        responseTopic_ = std::move(other.responseTopic_);
        rpcClient_ = std::move(other.rpcClient_);
    }
    return *this;
}

void RpcOperationProcessor::processRequest(const char* payload, size_t payload_len) {
    // Input validation
    if (!payload || payload_len == 0) {
        std::cerr << "[RPC Processor] Empty payload received" << std::endl;
        return;
    }

    // Size validation (prevent memory exhaustion)
    const size_t MAX_PAYLOAD_SIZE = 1024 * 1024; // 1MB
    if (payload_len > MAX_PAYLOAD_SIZE) {
        std::cerr << "[RPC Processor] Payload too large: " << payload_len << " bytes" << std::endl;
        return;
    }

    try {
        // JSON parsing
        json root = json::parse(payload, payload + payload_len);

        // JSON-RPC 2.0 validation
        if (!root.contains("jsonrpc") || root["jsonrpc"].get<std::string>() != "2.0") {
            std::cerr << "[RPC Processor] Invalid or missing JSON-RPC version" << std::endl;
            return;
        }

        // Extract transaction ID
        std::string transactionId = extractTransactionId(root);

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
        json paramsObj = root["params"];

        // Check shutdown state
        bool shuttingDown = isShuttingDown_.load();
        if (shuttingDown) {
            sendResponse(transactionId, false, "", "Server is shutting down");
            return;
        }

        // Create processing context
        auto context = std::make_shared<RequestContext>(
            std::string(payload, payload_len),  // Request data
            transactionId,                       // Transaction identifier
            responseTopic_,                      // Response topic
            std::make_shared<const NetworkCollectorConfig>(config_),  // Shared configuration
            verbose_,                            // Verbosity setting
            rpcClient_                           // RPC client for responses
        );

        // Launch asynchronous processing
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
            
            // Set thread ID for worker access
            context->threadIdPromise->set_value(threadId);

            if (verbose_) {
                std::cout << "[RPC Processor] Launched operation thread " << threadId 
                         << " for method: " << method << std::endl;
            }

        } catch (const std::exception& e) {
            std::cerr << "[RPC Processor] Failed to create thread: " << e.what() << std::endl;
            
            // Fallback to synchronous processing
            context->threadIdPromise->set_value(0);
            RpcOperationProcessor::processOperationThreadStatic(context);
        }

        // Cleanup completed threads periodically
        cleanupCompletedThreads();

    } catch (const json::parse_error& e) {
        std::cerr << "[RPC Processor] JSON parse error: " << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[RPC Processor] Exception processing request: " << e.what() << std::endl;
    }
}

void RpcOperationProcessor::setResponseTopic(const std::string& topic) {
    responseTopic_ = topic;
    if (verbose_) {
        std::cout << "[RPC Processor] Response topic set to: " << topic << std::endl;
    }
}

void RpcOperationProcessor::setRpcClient(std::shared_ptr<RpcClient> rpcClient) {
    rpcClient_ = rpcClient;
    if (verbose_) {
        std::cout << "[RPC Processor] RPC client reference set" << std::endl;
    }
}

NetworkCollectorConfig RpcOperationProcessor::getConfig() const {
    return config_;
}

void RpcOperationProcessor::updateConfig(const NetworkCollectorConfig& config) {
    config_ = config;
    if (verbose_) {
        std::cout << "[RPC Processor] Configuration updated" << std::endl;
    }
}

size_t RpcOperationProcessor::getActiveThreadCount() const {
    std::lock_guard<std::mutex> lock(threadsMutex_);
    return activeThreads_.size();
}

void RpcOperationProcessor::shutdown() {
    if (verbose_) {
        std::cout << "[RPC Processor] Initiating shutdown..." << std::endl;
    }
    
    // Set shutdown flag to prevent new thread creation
    isShuttingDown_.store(true);
    
    // Collect all active threads for joining
    std::vector<unsigned int> threadsToJoin;
    {
        std::lock_guard<std::mutex> lock(threadsMutex_);
        threadsToJoin.assign(activeThreads_.begin(), activeThreads_.end());
    }
    
    if (verbose_) {
        std::cout << "[RPC Processor] Joining " << threadsToJoin.size() << " active threads..." << std::endl;
    }
    
    // Join all threads with timeout
    for (unsigned int threadId : threadsToJoin) {
        if (threadManager_->isThreadAlive(threadId)) {
            bool completed = threadManager_->joinThread(threadId, std::chrono::minutes(5));
            if (!completed) {
                std::cerr << "[RPC Processor] WARNING: Thread " << threadId 
                         << " did not complete after 5 minutes" << std::endl;
            }
        }
    }
    
    // Clear tracking
    {
        std::lock_guard<std::mutex> lock(threadsMutex_);
        activeThreads_.clear();
    }
    
    if (verbose_) {
        std::cout << "[RPC Processor] Shutdown completed" << std::endl;
    }
}

void RpcOperationProcessor::processOperationThread(std::shared_ptr<RequestContext> context) {
    try {
        // Wait for thread ID synchronization
        unsigned int threadId = context->threadIdFuture.get();
        
        // Extract context data safely
        const std::string& requestJson = context->requestJson;
        const std::string& transactionId = context->transactionId;
        std::shared_ptr<const NetworkCollectorConfig> config = context->config;
        bool verbose = context->verbose;
        
        if (verbose) {
            std::cout << "[RPC Processor] Thread " << threadId 
                     << " processing transaction: " << transactionId << std::endl;
        }
        
        // Parse request in thread context
        json root = json::parse(requestJson);
        
        // Extract method and parameters
        std::string method = root["method"].get<std::string>();
        json paramsObj = root["params"];
        
        // Execute operation
        std::string result = executeNetworkOperation(method, paramsObj, *config, verbose);
        
        // Send success response
        sendResponseStatic(transactionId, true, result, "", 
                          context->responseTopic, context->rpcClient);
        
        if (verbose) {
            std::cout << "[RPC Processor] Thread " << threadId 
                     << " completed operation: " << method << std::endl;
        }
        
    } catch (const std::exception& e) {
        sendResponseStatic(context->transactionId, false, "", 
                          std::string("Exception: ") + e.what(),
                          context->responseTopic, context->rpcClient);
    }

    // Cleanup thread from tracking
    removeThreadFromTracking(context->threadIdFuture.get());
}

void RpcOperationProcessor::processOperationThreadStatic(std::shared_ptr<RequestContext> context) {
    // This is a static entry point that forwards to the instance method
    // In a real implementation, we'd need to store the instance pointer
    // For now, we'll implement the logic directly here
    
    try {
        // Wait for thread ID synchronization
        unsigned int threadId = context->threadIdFuture.get();
        
        // Extract context data safely
        const std::string& requestJson = context->requestJson;
        const std::string& transactionId = context->transactionId;
        std::shared_ptr<const NetworkCollectorConfig> config = context->config;
        bool verbose = context->verbose;
        
        if (verbose) {
            std::cout << "[RPC Processor] Thread " << threadId 
                     << " processing transaction: " << transactionId << std::endl;
        }
        
        // Parse request in thread context
        json root = json::parse(requestJson);
        
        // Extract method and parameters
        std::string method = root["method"].get<std::string>();
        json paramsObj = root["params"];
        
        // Execute operation based on method
        std::string result;
        
        if (method == "collect_network_data") {
            // Create a temporary network collector thread for this operation
            NetworkCollectorThread collector;
            
            // Update collector configuration from params if provided
            CollectionConfig opConfig;
            opConfig.collectVlan = config->collectVlan;
            opConfig.collectNat = config->collectNat;
            opConfig.collectFirewall = config->collectFirewall;
            opConfig.collectRoutes = config->collectRoutes;
            opConfig.collectBridges = config->collectBridges;
            opConfig.collectAll = config->collectAll;
            opConfig.outputText = config->outputText;
            opConfig.quietMode = config->quietMode;
            opConfig.outputFile = config->outputFile;
            opConfig.collectionInterval = config->collectionInterval;
            
            if (paramsObj.contains("config")) {
                NetworkCollectorConfig paramConfig = NetworkCollectorConfig::from_json(paramsObj["config"]);
                opConfig.collectVlan = paramConfig.collectVlan;
                opConfig.collectNat = paramConfig.collectNat;
                opConfig.collectFirewall = paramConfig.collectFirewall;
                opConfig.collectRoutes = paramConfig.collectRoutes;
                opConfig.collectBridges = paramConfig.collectBridges;
                opConfig.collectAll = paramConfig.collectAll;
                opConfig.outputText = paramConfig.outputText;
                opConfig.quietMode = paramConfig.quietMode;
                opConfig.outputFile = paramConfig.outputFile;
                opConfig.collectionInterval = paramConfig.collectionInterval;
            }
            
            // Start collection and wait for completion
            if (collector.start(opConfig)) {
                // Wait for collection to complete
                std::this_thread::sleep_for(std::chrono::seconds(3));
                result = collector.getLastCollectedData();
                collector.stop();
            } else {
                throw std::runtime_error("Failed to start network collector");
            }
            
        } else if (method == "get_collector_status") {
            json status;
            status["running"] = false;  // We don't have persistent collector in this context
            status["config"] = config->to_json();
            result = status.dump();
            
        } else if (method == "update_collector_config") {
            // This would update the global config in a real implementation
            json response;
            response["message"] = "Configuration updated (simulated)";
            result = response.dump();
            
        } else {
            throw std::runtime_error("Unknown method: " + method);
        }
        
        // Send success response
        sendResponseStatic(transactionId, true, result, "", 
                          context->responseTopic, context->rpcClient);
        
        if (verbose) {
            std::cout << "[RPC Processor] Thread " << threadId 
                     << " completed operation: " << method << std::endl;
        }
        
    } catch (const std::exception& e) {
        sendResponseStatic(context->transactionId, false, "", 
                          std::string("Exception: ") + e.what(),
                          context->responseTopic, context->rpcClient);
    }

    // Note: Thread cleanup would be handled by the instance in a real implementation
    // For now, we can't call removeThreadFromTracking without access to the instance
}

void RpcOperationProcessor::sendResponse(const std::string& transactionId, bool success, 
                                         const std::string& result, const std::string& error) {
    sendResponseStatic(transactionId, success, result, error, responseTopic_, rpcClient_);
}

void RpcOperationProcessor::sendResponseStatic(const std::string& transactionId, bool success,
                                               const std::string& result, const std::string& error,
                                               const std::string& responseTopic,
                                               std::weak_ptr<RpcClient> rpcClient) {
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

        // Publish response via RPC client
        if (auto client = rpcClient.lock()) {
            std::string responseJson = response.dump();
            direct_client_publish_raw_message(responseTopic.c_str(), 
                                             responseJson.c_str(), 
                                             responseJson.size());
        } else {
            std::cerr << "[RPC Processor] RPC client no longer available for response" << std::endl;
        }

    } catch (const std::exception& e) {
        std::cerr << "[RPC Processor] Failed to send response: " << e.what() << std::endl;
    }
}

std::string RpcOperationProcessor::extractTransactionId(const json& request) {
    if (request.contains("id")) {
        if (request["id"].is_string()) {
            return request["id"];
        } else if (request["id"].is_number()) {
            return std::to_string(static_cast<long long>(request["id"]));
        }
    }
    return "unknown";
}

std::string RpcOperationProcessor::executeNetworkOperation(const std::string& method,
                                                           const json& params,
                                                           const NetworkCollectorConfig& config,
                                                           bool verbose) {
    // This would contain the actual network operation logic
    // For now, return a placeholder response
    json response;
    response["method"] = method;
    response["params"] = params;
    response["config"] = config.to_json();
    response["message"] = "Network operation executed (simulated)";
    
    return response.dump();
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
        
        // Remove from tracking
        for (unsigned int threadId : threadsToClean) {
            activeThreads_.erase(threadId);
        }
    }
    
    // Log cleanup statistics
    if (!threadsToClean.empty() && verbose_) {
        std::cout << "[RPC Processor] Cleaned up " << threadsToClean.size() 
                  << " completed threads" << std::endl;
    }
}

void RpcOperationProcessor::removeThreadFromTracking(unsigned int threadId) {
    std::lock_guard<std::mutex> lock(threadsMutex_);
    activeThreads_.erase(threadId);
}
