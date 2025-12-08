#ifndef RPC_OPERATION_PROCESSOR_HPP
#define RPC_OPERATION_PROCESSOR_HPP

#include <string>
#include <memory>
#include <set>
#include <map>
#include <mutex>
#include <atomic>
#include <thread>
#include <chrono>
#include <condition_variable>
#include "../thirdparty/nlohmann/json.hpp"
#include "../thirdparty/ur-threadder-api/cpp/include/ThreadManager.hpp"

using json = nlohmann::json;
using namespace ThreadMgr;

// Forward declarations
class ConfigManager;
class RpcClient;

// Thread tracking context for RPC operations
class ThreadTrackingContext {
public:
    enum class ThreadStatus {
        CREATED,
        RUNNING,
        FINISHED,
        FAILED,
        TIMEOUT
    };
    
    std::string transaction_id;
    std::string method;
    std::shared_ptr<ConfigManager> config_manager;
    std::string config_file;
    unsigned int thread_id;
    unsigned int worker_thread_id;
    std::chrono::system_clock::time_point start_time;
    std::atomic<ThreadStatus> status{ThreadStatus::CREATED};
    json progress_data;
    std::string error_message;
};

// Status broadcaster for real-time updates
class StatusBroadcaster {
private:
    std::shared_ptr<RpcClient> rpc_client_;
    std::string status_topic_ = "ur-shared-bus/ur-netbench-mann/runtime";
    std::atomic<bool> broadcasting_enabled_{true};
    std::map<std::string, std::string> last_published_status_;
    std::mutex status_mutex_;
    std::chrono::milliseconds min_update_interval_{1000};
    std::map<std::string, std::chrono::steady_clock::time_point> last_update_time_;
    
public:
    explicit StatusBroadcaster(std::shared_ptr<RpcClient> rpc_client);
    
    void publishStatusUpdate(const std::string& transaction_id,
                           const std::string& status,
                           const json& details = json::object());
    
    void enableBroadcasting(bool enabled);
    bool isBroadcastingEnabled() const;
    
private:
    void publishThrottledStatusUpdate(const std::string& transaction_id,
                                    const std::string& status,
                                    const json& details = json::object());
};

// RPC response handler
class RpcResponseHandler {
private:
    std::shared_ptr<RpcClient> rpc_client_;
    std::string response_topic_ = "direct_messaging/ur-netbench-mann/responses";
    
public:
    explicit RpcResponseHandler(std::shared_ptr<RpcClient> rpc_client);
    
    void sendSuccessResponse(const std::string& transaction_id,
                            const std::string& message,
                            const json& additional_data = json::object());
    
    void sendErrorResponse(const std::string& transaction_id,
                         int error_code,
                         const std::string& error_message,
                         const json& error_data = json::object());
    
private:
    void publishResponse(const json& response);
};

// RPC thread manager for enhanced tracking
class RpcThreadManager {
private:
    std::map<std::string, std::shared_ptr<ThreadTrackingContext>> active_threads_;
    std::mutex threads_mutex_;
    ThreadManager* thread_manager_;
    
public:
    explicit RpcThreadManager(ThreadManager* thread_manager);
    
    void registerThread(std::shared_ptr<ThreadTrackingContext> context);
    void updateThreadStatus(const std::string& transaction_id, 
                           ThreadTrackingContext::ThreadStatus status);
    void updateThreadProgress(const std::string& transaction_id, 
                             const json& progress_data);
    std::shared_ptr<ThreadTrackingContext> getThreadContext(const std::string& transaction_id);
    void cleanupThread(const std::string& transaction_id);
    std::vector<std::shared_ptr<ThreadTrackingContext>> getActiveThreads();
    void stopThread(const std::string& transaction_id);
    void shutdown();
};

// Configuration builder for RPC compatibility
class RpcConfigurationBuilder {
public:
    static json buildPackageConfig(const std::string& method, const json& rpc_params);
    
private:
    enum class OperationType {
        DNS, PING, TRACEROUTE, IPERF, SERVERS_STATUS
    };
    
    static OperationType getOperationType(const std::string& method);
    static json buildDnsConfig(json package_config, const json& rpc_params);
    static json buildPingConfig(json package_config, const json& rpc_params);
    static json buildTracerouteConfig(json package_config, const json& rpc_params);
    static json buildIperfConfig(json package_config, const json& rpc_params);
    static json buildServersStatusConfig(json package_config, const json& rpc_params);
    static void validateRequiredParams(const json& params, 
                                     const std::vector<std::string>& required,
                                     const std::string& operation_name);
};

class RpcOperationProcessor {
public:
    // Constructor with configuration and verbosity settings
    RpcOperationProcessor(const ConfigManager& configManager, bool verbose = false);
    ~RpcOperationProcessor();
    
    // Core processing interface
    void processRequest(const char* payload, size_t payload_len);
    void setResponseTopic(const std::string& topic);
    void setRpcClient(std::shared_ptr<RpcClient> rpcClient);
    
    // Get active threads count
    size_t getActiveThreadsCount() const;
    bool isRunning() const;
    
    // Shutdown
    void shutdown();
    
private:
    // Configuration
    const ConfigManager& configManager_;
    bool verbose_;
    std::string responseTopic_;
    std::shared_ptr<RpcClient> rpcClient_;
    
    // Enhanced components
    std::unique_ptr<RpcThreadManager> thread_manager_;
    std::unique_ptr<StatusBroadcaster> status_broadcaster_;
    std::unique_ptr<RpcResponseHandler> response_handler_;
    
    // Thread management for concurrent operations
    std::shared_ptr<ThreadManager> internalThreadManager_;
    std::atomic<bool> shutdown_requested_{false};
    std::thread status_monitoring_thread_;
    
    // Request processing methods
    void processValidatedOperation(const std::string& method,
                                  const std::string& transaction_id,
                                  const json& package_config);
    void executeOperationThread(std::shared_ptr<ThreadTrackingContext> context);
    
    // Operation-specific handlers
    void executeDnsOperation(std::shared_ptr<ThreadTrackingContext> context);
    void executePingOperation(std::shared_ptr<ThreadTrackingContext> context);
    void executeTracerouteOperation(std::shared_ptr<ThreadTrackingContext> context);
    void executeIperfOperation(std::shared_ptr<ThreadTrackingContext> context);
    void executeServersStatusOperation(std::shared_ptr<ThreadTrackingContext> context);
    
    // Request validation helpers
    bool validateJsonRpcRequest(const json& request);
    std::string extractTransactionId(const json& request);
    bool isValidOperation(const std::string& method);
    std::string createTempConfigFile(const json& config);
    
    // Status monitoring
    void statusMonitoringLoop();
    
    // Utility methods
    void logInfo(const std::string& message) const;
    void logError(const std::string& message) const;
    
    // Error codes
    enum class RpcErrorCode {
        PARSE_ERROR = -32700,
        INVALID_REQUEST = -32600,
        METHOD_NOT_FOUND = -32601,
        INVALID_PARAMS = -32602,
        INTERNAL_ERROR = -32603,
        CONFIG_VALIDATION_ERROR = -32000,
        THREAD_CREATION_ERROR = -32001,
        RESOURCE_UNAVAILABLE = -32002,
        TIMEOUT_ERROR = -32003
    };
};

#endif // RPC_OPERATION_PROCESSOR_HPP
