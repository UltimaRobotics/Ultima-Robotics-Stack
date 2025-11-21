#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>

namespace URMavRouterShared {

/**
 * @brief Thread operation types for RPC requests
 */
enum class ThreadOperation {
    START = 0,
    STOP = 1,
    PAUSE = 2,
    RESUME = 3,
    RESTART = 4,
    STATUS = 5
};

/**
 * @brief Thread target types for RPC requests
 */
enum class ThreadTarget {
    MAINLOOP = 0,
    HTTP_SERVER = 1,
    STATISTICS = 2,
    ALL = 3
};

/**
 * @brief Operation result status
 */
enum class OperationStatus {
    SUCCESS = 0,
    FAILED = 1,
    THREAD_NOT_FOUND = 2,
    INVALID_OPERATION = 3,
    ALREADY_IN_STATE = 4,
    TIMEOUT = 5,
    CONFIGURATION_ERROR = 6,
    EXTENSION_ERROR = 7
};

/**
 * @brief Thread state information
 */
struct ThreadStateInfo {
    std::string threadName;
    unsigned int threadId;
    int state;  // Corresponds to ThreadMgr::ThreadState
    bool isAlive;
    std::string attachmentId;
    
    ThreadStateInfo() 
        : threadName("")
        , threadId(0)
        , state(0)
        , isAlive(false)
        , attachmentId("") {}
    
    ThreadStateInfo(const std::string& name, unsigned int id, int st, bool alive, const std::string& attachment)
        : threadName(name)
        , threadId(id)
        , state(st)
        , isAlive(alive)
        , attachmentId(attachment) {}
};

/**
 * @brief Thread RPC request structure
 */
struct ThreadRpcRequest {
    ThreadOperation operation;
    ThreadTarget target;
    std::string threadName;
    std::map<std::string, std::string> parameters;
    
    ThreadRpcRequest() 
        : operation(ThreadOperation::STATUS)
        , target(ThreadTarget::ALL) {}
    
    ThreadRpcRequest(ThreadOperation op, ThreadTarget tgt, const std::string& name = "")
        : operation(op), target(tgt), threadName(name) {}
};

/**
 * @brief Thread RPC response structure
 */
struct ThreadRpcResponse {
    OperationStatus status;
    std::string message;
    std::map<std::string, ThreadStateInfo> threadStates;
    
    ThreadRpcResponse() 
        : status(OperationStatus::SUCCESS)
        , message("") {}
    
    ThreadRpcResponse(OperationStatus stat, const std::string& msg)
        : status(stat), message(msg) {}
};

/**
 * @brief Device configuration for mainloop start
 */
struct DeviceConfig {
    std::string devicePath;
    int baudrate;
    
    DeviceConfig() 
        : devicePath("")
        , baudrate(57600) {}
    
    DeviceConfig(const std::string& path, int baud = 57600)
        : devicePath(path), baudrate(baud) {}
};

/**
 * @brief Mainloop start request with device configuration
 */
struct MainloopStartRequest {
    DeviceConfig deviceConfig;
    bool loadExtensions;
    std::string extensionConfigDir;
    
    MainloopStartRequest() 
        : loadExtensions(true)
        , extensionConfigDir("config") {}
};

/**
 * @brief Conversion functions
 */
class ThreadTypeConverter {
public:
    static std::string threadOperationToString(ThreadOperation operation);
    static ThreadOperation stringToThreadOperation(const std::string& operation);
    
    static std::string threadTargetToString(ThreadTarget target);
    static ThreadTarget stringToThreadTarget(const std::string& target);
    
    static std::string operationStatusToString(OperationStatus status);
    static OperationStatus stringToOperationStatus(const std::string& status);
    
    static std::string threadStateToString(int state);
    static int stringToThreadState(const std::string& state);
};

} // namespace URMavRouterShared
