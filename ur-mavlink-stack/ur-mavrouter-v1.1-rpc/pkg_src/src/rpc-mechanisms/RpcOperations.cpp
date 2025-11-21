#include "RpcOperations.hpp"
#include "../common/log.h"
#include "mainloop.h"
#include <sstream>
#include <algorithm>
#include <iostream>

namespace RpcMechanisms {

std::string RpcResponse::toJson() const {
    std::stringstream ss;
    ss << "{";
    ss << "\"status\":\"" << static_cast<int>(status) << "\",";
    ss << "\"message\":\"" << message << "\",";
    ss << "\"threads\":{";

    bool first = true;
    for (const auto& pair : threadStates) {
        if (!first) ss << ",";
        first = false;

        const auto& info = pair.second;
        ss << "\"" << pair.first << "\":{";
        ss << "\"threadId\":" << info.threadId << ",";
        ss << "\"state\":" << static_cast<int>(info.state) << ",";
        ss << "\"isAlive\":" << (info.isAlive ? "true" : "false") << ",";
        ss << "\"attachmentId\":\"" << info.attachmentId << "\"";
        ss << "}";
    }

    ss << "}}";
    return ss.str();
}

RpcOperations::RpcOperations(ThreadMgr::ThreadManager& threadManager)
    : threadManager_(threadManager) {
    log_info("RpcOperations initialized");
}

RpcOperations::~RpcOperations() {
    log_info("RpcOperations destroyed");
}

void RpcOperations::registerThread(const std::string& threadName, 
                                   unsigned int threadId,
                                   const std::string& attachmentId) {
    std::lock_guard<std::mutex> lock(registryMutex_);
    threadRegistry_[threadName] = threadId;
    threadAttachments_[threadName] = attachmentId;

    // Register with thread manager
    threadManager_.registerThread(threadId, attachmentId);

    log_info("RPC: Registered thread '%s' with ID %u and attachment '%s'",
             threadName.c_str(), threadId, attachmentId.c_str());
}

void RpcOperations::registerRestartCallback(const std::string& threadName, 
                                           std::function<unsigned int()> restartCallback) {
    std::lock_guard<std::mutex> lock(registryMutex_);
    restartCallbacks_[threadName] = restartCallback;
    log_info("RPC: Registered restart callback for thread '%s'", threadName.c_str());
}

void RpcOperations::unregisterThread(const std::string& threadName) {
    std::lock_guard<std::mutex> lock(registryMutex_);

    auto it = threadAttachments_.find(threadName);
    if (it != threadAttachments_.end()) {
        threadManager_.unregisterThread(it->second);
        threadAttachments_.erase(it);
    }

    threadRegistry_.erase(threadName);
    log_info("RPC: Unregistered thread '%s'", threadName.c_str());
}

ThreadStateInfo RpcOperations::getThreadStateInfo(const std::string& threadName) {
    ThreadStateInfo info;
    info.threadName = threadName;

    // Note: registryMutex_ should already be locked by caller
    auto it = threadRegistry_.find(threadName);
    if (it != threadRegistry_.end()) {
        info.threadId = it->second;

        try {
            info.state = threadManager_.getThreadState(it->second);
            info.isAlive = threadManager_.isThreadAlive(it->second);
        } catch (const ThreadMgr::ThreadManagerException& e) {
            log_error("RPC: Failed to get thread state for '%s': %s", threadName.c_str(), e.what());
            info.state = ThreadMgr::ThreadState::Error;
            info.isAlive = false;
        }

        auto attachIt = threadAttachments_.find(threadName);
        if (attachIt != threadAttachments_.end()) {
            info.attachmentId = attachIt->second;
        }
    }

    return info;
}

RpcResponse RpcOperations::getAllThreadStatus() {
    RpcResponse response;
    response.status = OperationStatus::SUCCESS;
    response.message = "Retrieved status for all threads";

    std::lock_guard<std::mutex> lock(registryMutex_);
    for (const auto& pair : threadRegistry_) {
        ThreadStateInfo info = getThreadStateInfo(pair.first);
        response.threadStates[pair.first] = info;
    }

    return response;
}

RpcResponse RpcOperations::getThreadStatus(const std::string& threadName) {
    RpcResponse response;

    std::lock_guard<std::mutex> lock(registryMutex_);
    if (threadRegistry_.find(threadName) == threadRegistry_.end()) {
        response.status = OperationStatus::THREAD_NOT_FOUND;
        response.message = "Thread not found: " + threadName;
        return response;
    }

    ThreadStateInfo info = getThreadStateInfo(threadName);

    response.status = OperationStatus::SUCCESS;
    response.message = "Retrieved thread status";
    response.threadStates[threadName] = info;

    return response;
}

RpcResponse RpcOperations::executeOperationOnThread(const std::string& threadName, 
                                                   ThreadOperation operation) {
    RpcResponse response;

    std::lock_guard<std::mutex> lock(registryMutex_);
    auto it = threadRegistry_.find(threadName);
    if (it == threadRegistry_.end()) {
        response.status = OperationStatus::THREAD_NOT_FOUND;
        response.message = "Thread not found: " + threadName;
        return response;
    }

    unsigned int threadId = it->second;
    ThreadStateInfo info = getThreadStateInfo(threadName);

    try {
        switch (operation) {
            case ThreadOperation::START:
                response.status = OperationStatus::INVALID_OPERATION;
                response.message = "Cannot start thread - use restart instead";
                break;

            case ThreadOperation::STOP:
                if (info.state == ThreadMgr::ThreadState::Running) {
                    threadManager_.stopThread(threadId);
                    response.status = OperationStatus::SUCCESS;
                    response.message = "Thread stopped successfully";
                } else {
                    response.status = OperationStatus::ALREADY_IN_STATE;
                    response.message = "Thread is not running";
                }
                break;

            case ThreadOperation::PAUSE:
                if (info.state == ThreadMgr::ThreadState::Running) {
                    threadManager_.pauseThread(threadId);
                    response.status = OperationStatus::SUCCESS;
                    response.message = "Thread paused successfully";
                } else {
                    response.status = OperationStatus::ALREADY_IN_STATE;
                    response.message = "Thread cannot be paused";
                }
                break;

            case ThreadOperation::RESUME:
                if (info.state == ThreadMgr::ThreadState::Paused) {
                    threadManager_.resumeThread(threadId);
                    response.status = OperationStatus::SUCCESS;
                    response.message = "Thread resumed successfully";
                } else {
                    response.status = OperationStatus::ALREADY_IN_STATE;
                    response.message = "Thread is not paused";
                }
                break;

            case ThreadOperation::RESTART: {
                auto callbackIt = restartCallbacks_.find(threadName);
                if (callbackIt != restartCallbacks_.end()) {
                    // Stop the old thread
                    if (info.state == ThreadMgr::ThreadState::Running) {
                        threadManager_.stopThread(threadId);
                    }
                    
                    // Wait for it to stop
                    threadManager_.joinThread(threadId, std::chrono::seconds(5));
                    
                    // Call the restart callback to create a new thread
                    unsigned int newThreadId = callbackIt->second();
                    if (newThreadId != 0) {
                        // Update the registry with the new thread ID
                        threadRegistry_[threadName] = newThreadId;
                        response.status = OperationStatus::SUCCESS;
                        response.message = "Thread restarted successfully with new ID: " + std::to_string(newThreadId);
                    } else {
                        response.status = OperationStatus::FAILED;
                        response.message = "Failed to restart thread";
                    }
                } else {
                    response.status = OperationStatus::INVALID_OPERATION;
                    response.message = "No restart callback registered for thread: " + threadName;
                }
                break;
            }

            case ThreadOperation::STATUS:
                response.status = OperationStatus::SUCCESS;
                response.message = "Thread status retrieved";
                response.threadStates[threadName] = info;
                break;

            default:
                response.status = OperationStatus::INVALID_OPERATION;
                response.message = "Unknown operation";
                break;
        }

        // Update thread state after operation
        if (operation != ThreadOperation::STATUS) {
            info = getThreadStateInfo(threadName);
            response.threadStates[threadName] = info;
        }

    } catch (const ThreadMgr::ThreadManagerException& e) {
        response.status = OperationStatus::FAILED;
        response.message = "Thread operation failed: " + std::string(e.what());
    } catch (const std::exception& e) {
        response.status = OperationStatus::FAILED;
        response.message = "Operation failed: " + std::string(e.what());
    }

    return response;
}

RpcResponse RpcOperations::executeRequest(const RpcRequest& request) {
    if (request.target == ThreadTarget::ALL) {
        return getAllThreadStatus();
    }

    std::vector<std::string> threadNames = getThreadNamesForTarget(request.target);
    if (threadNames.empty()) {
        RpcResponse response;
        response.status = OperationStatus::THREAD_NOT_FOUND;
        response.message = "No threads found for target";
        return response;
    }

    if (threadNames.size() == 1) {
        return executeOperationOnThread(threadNames[0], request.operation);
    }

    // Multiple threads - execute operation on all of them
    RpcResponse response;
    response.status = OperationStatus::SUCCESS;
    response.message = "Operation executed on multiple threads";

    for (const std::string& threadName : threadNames) {
        RpcResponse threadResponse = executeOperationOnThread(threadName, request.operation);
        
        // Merge the thread states
        for (const auto& pair : threadResponse.threadStates) {
            response.threadStates[pair.first] = pair.second;
        }

        // If any operation failed, update the overall status
        if (threadResponse.status != OperationStatus::SUCCESS) {
            response.status = threadResponse.status;
            response.message = threadResponse.message;
        }
    }

    return response;
}

std::vector<std::string> RpcOperations::getThreadNamesForTarget(ThreadTarget target) {
    std::vector<std::string> names;
    std::lock_guard<std::mutex> lock(registryMutex_);

    for (const auto& pair : threadRegistry_) {
        const std::string& threadName = pair.first;
        
        switch (target) {
            case ThreadTarget::MAINLOOP:
                if (threadName == "mainloop") {
                    names.push_back(threadName);
                }
                break;
            case ThreadTarget::HTTP_SERVER:
                if (threadName == "http_server") {
                    names.push_back(threadName);
                }
                break;
            case ThreadTarget::STATISTICS:
                if (threadName.find("stats") != std::string::npos) {
                    names.push_back(threadName);
                }
                break;
            case ThreadTarget::ALL:
                names.push_back(threadName);
                break;
        }
    }

    return names;
}

ThreadOperation RpcOperations::stringToThreadOperation(const std::string& operation) {
    std::string lower = operation;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    if (lower == "start") return ThreadOperation::START;
    if (lower == "stop") return ThreadOperation::STOP;
    if (lower == "pause") return ThreadOperation::PAUSE;
    if (lower == "resume") return ThreadOperation::RESUME;
    if (lower == "restart") return ThreadOperation::RESTART;
    if (lower == "status") return ThreadOperation::STATUS;

    return ThreadOperation::STATUS;
}

void RpcOperations::setExtensionManager(MavlinkExtensions::ExtensionManager* extensionManager) {
    extensionManager_ = extensionManager;
    log_info("Extension manager set for RPC operations");
}

MavlinkExtensions::ExtensionManager* RpcOperations::getExtensionManager() {
    return extensionManager_;
}

} // namespace RpcMechanisms
