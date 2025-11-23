#pragma once
#include <string>
#include <memory>
#include <functional>
#include <map>
#include <mutex>
#include <atomic>
#include <iostream>
#include <cstdio>
#include "../../modules/ur-threadder-api/cpp/include/ThreadManager.hpp"
#include "../../modules/nholmann/json.hpp"

// Forward declaration for ExtensionManager
namespace MavlinkExtensions {
    class ExtensionManager;
}

namespace RpcMechanisms {

/**
 * @brief Thread operation types
 */
enum class ThreadOperation {
    START,
    STOP,
    PAUSE,
    RESUME,
    RESTART,
    STATUS
};

/**
 * @brief Thread target types
 */
enum class ThreadTarget {
    MAINLOOP,
    HTTP_SERVER,
    STATISTICS,
    ALL
};

/**
 * @brief Operation result status
 */
enum class OperationStatus {
    SUCCESS,
    FAILED,
    THREAD_NOT_FOUND,
    INVALID_OPERATION,
    ALREADY_IN_STATE,
    TIMEOUT
};

/**
 * @brief Thread state information
 */
struct ThreadStateInfo {
    std::string threadName;
    unsigned int threadId;
    ThreadMgr::ThreadState state;
    bool isAlive;
    std::string attachmentId;
    
    ThreadStateInfo() 
        : threadName("")
        , threadId(0)
        , state(ThreadMgr::ThreadState::Created)
        , isAlive(false)
        , attachmentId("") {}
};

/**
 * @brief RPC request structure
 */
struct RpcRequest {
    ThreadOperation operation;
    ThreadTarget target;
    std::map<std::string, std::string> parameters;
    
    RpcRequest(ThreadOperation op = ThreadOperation::STATUS, 
               ThreadTarget tgt = ThreadTarget::ALL)
        : operation(op), target(tgt) {}
};

/**
 * @brief RPC response structure
 */
struct RpcResponse {
    OperationStatus status;
    std::string message;
    std::map<std::string, ThreadStateInfo> threadStates;
    
    RpcResponse() : status(OperationStatus::SUCCESS), message("") {}
    
    std::string toJson() const;
};

/**
 * @brief RPC Operations for thread management
 * 
 * This class handles the thread management operations that can be called via RPC.
 */
class RpcOperations {
public:
    /**
     * @brief Constructor
     * @param threadManager Reference to the thread manager
     * @param routerConfigPath Path to the router configuration file
     */
    explicit RpcOperations(ThreadMgr::ThreadManager& threadManager, const std::string& routerConfigPath = "");
    
    /**
     * @brief Destructor
     */
    ~RpcOperations();
    
    // Disable copy
    RpcOperations(const RpcOperations&) = delete;
    RpcOperations& operator=(const RpcOperations&) = delete;
    
    /**
     * @brief Register a thread for RPC control
     * @param threadName Logical name for the thread
     * @param threadId Thread ID from thread manager
     * @param attachmentId Attachment identifier
     */
    void registerThread(const std::string& threadName, 
                       unsigned int threadId,
                       const std::string& attachmentId);
    
    /**
     * @brief Unregister a thread
     * @param threadName Thread name
     */
    void unregisterThread(const std::string& threadName);
    
    /**
     * @brief Execute an RPC request
     * @param request RPC request structure
     * @return RPC response structure
     */
    RpcResponse executeRequest(const RpcRequest& request);
    
    /**
     * @brief Get status of all threads
     * @return RPC response with all thread states
     */
    RpcResponse getAllThreadStatus();
    
    /**
     * @brief Get status of a specific thread
     * @param threadName Thread name
     * @return RPC response with thread state
     */
    RpcResponse getThreadStatus(const std::string& threadName);
    
    /**
     * @brief Execute operation on a specific thread
     * @param threadName Thread name
     * @param operation Operation to execute
     * @return RPC response with operation result
     */
    RpcResponse executeOperationOnThread(const std::string& threadName, 
                                        ThreadOperation operation);
    
    /**
     * @brief Register a restart callback for a thread
     * @param threadName Thread name
     * @param restartCallback Function to call for restart
     */
    void registerRestartCallback(const std::string& threadName, 
                               std::function<unsigned int()> restartCallback);
    
    /**
     * @brief Set extension manager
     * @param extensionManager Pointer to extension manager
     */
    void setExtensionManager(MavlinkExtensions::ExtensionManager* extensionManager);
    
    /**
     * @brief Get extension manager
     * @return Pointer to extension manager
     */
    MavlinkExtensions::ExtensionManager* getExtensionManager();
    
    /**
     * @brief Get thread names for a target
     * @param target Thread target
     * @return Vector of thread names
     */
    std::vector<std::string> getThreadNamesForTarget(ThreadTarget target);
    
    /**
     * @brief Execute restart callback for a specific thread
     * @param threadName Thread name to restart
     * @return Thread ID of the new thread, 0 if failed
     */
    unsigned int executeRestartCallback(const std::string& threadName);
    
    /**
     * @brief Convert string to thread operation
     * @param operation String representation of operation
     * @return Thread operation enum
     */
    static ThreadOperation stringToThreadOperation(const std::string& operation);

private:
    ThreadMgr::ThreadManager& threadManager_;
    std::map<std::string, unsigned int> threadRegistry_;
    std::map<std::string, std::string> threadAttachments_;
    std::map<std::string, std::function<unsigned int()>> restartCallbacks_;
    mutable std::mutex registryMutex_;
    
    MavlinkExtensions::ExtensionManager* extensionManager_{nullptr};
    bool verbose_{false};
    std::string routerConfigPath_;  // Path to router configuration file
    mutable std::mutex operationsMutex_;
    
    ThreadStateInfo getThreadStateInfo(const std::string& threadName);
};

} // namespace RpcMechanisms
