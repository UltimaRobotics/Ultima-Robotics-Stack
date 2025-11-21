#include "ThreadRpcTypes.hpp"
#include <algorithm>
#include <cctype>

namespace URMavRouterShared {

std::string ThreadTypeConverter::threadOperationToString(ThreadOperation operation) {
    switch (operation) {
        case ThreadOperation::START: return "start";
        case ThreadOperation::STOP: return "stop";
        case ThreadOperation::PAUSE: return "pause";
        case ThreadOperation::RESUME: return "resume";
        case ThreadOperation::RESTART: return "restart";
        case ThreadOperation::STATUS: return "status";
        default: return "unknown";
    }
}

ThreadOperation ThreadTypeConverter::stringToThreadOperation(const std::string& operation) {
    std::string lowerOp = operation;
    std::transform(lowerOp.begin(), lowerOp.end(), lowerOp.begin(), ::tolower);
    
    if (lowerOp == "start") return ThreadOperation::START;
    if (lowerOp == "stop") return ThreadOperation::STOP;
    if (lowerOp == "pause") return ThreadOperation::PAUSE;
    if (lowerOp == "resume") return ThreadOperation::RESUME;
    if (lowerOp == "restart") return ThreadOperation::RESTART;
    if (lowerOp == "status") return ThreadOperation::STATUS;
    
    return ThreadOperation::STATUS; // Default
}

std::string ThreadTypeConverter::threadTargetToString(ThreadTarget target) {
    switch (target) {
        case ThreadTarget::MAINLOOP: return "mainloop";
        case ThreadTarget::HTTP_SERVER: return "http_server";
        case ThreadTarget::STATISTICS: return "statistics";
        case ThreadTarget::ALL: return "all";
        default: return "unknown";
    }
}

ThreadTarget ThreadTypeConverter::stringToThreadTarget(const std::string& target) {
    std::string lowerTarget = target;
    std::transform(lowerTarget.begin(), lowerTarget.end(), lowerTarget.begin(), ::tolower);
    
    if (lowerTarget == "mainloop") return ThreadTarget::MAINLOOP;
    if (lowerTarget == "http_server") return ThreadTarget::HTTP_SERVER;
    if (lowerTarget == "statistics") return ThreadTarget::STATISTICS;
    if (lowerTarget == "all") return ThreadTarget::ALL;
    
    return ThreadTarget::ALL; // Default
}

std::string ThreadTypeConverter::operationStatusToString(OperationStatus status) {
    switch (status) {
        case OperationStatus::SUCCESS: return "success";
        case OperationStatus::FAILED: return "failed";
        case OperationStatus::THREAD_NOT_FOUND: return "thread_not_found";
        case OperationStatus::INVALID_OPERATION: return "invalid_operation";
        case OperationStatus::ALREADY_IN_STATE: return "already_in_state";
        case OperationStatus::TIMEOUT: return "timeout";
        case OperationStatus::CONFIGURATION_ERROR: return "configuration_error";
        case OperationStatus::EXTENSION_ERROR: return "extension_error";
        default: return "unknown";
    }
}

OperationStatus ThreadTypeConverter::stringToOperationStatus(const std::string& status) {
    std::string lowerStatus = status;
    std::transform(lowerStatus.begin(), lowerStatus.end(), lowerStatus.begin(), ::tolower);
    
    if (lowerStatus == "success") return OperationStatus::SUCCESS;
    if (lowerStatus == "failed") return OperationStatus::FAILED;
    if (lowerStatus == "thread_not_found") return OperationStatus::THREAD_NOT_FOUND;
    if (lowerStatus == "invalid_operation") return OperationStatus::INVALID_OPERATION;
    if (lowerStatus == "already_in_state") return OperationStatus::ALREADY_IN_STATE;
    if (lowerStatus == "timeout") return OperationStatus::TIMEOUT;
    if (lowerStatus == "configuration_error") return OperationStatus::CONFIGURATION_ERROR;
    if (lowerStatus == "extension_error") return OperationStatus::EXTENSION_ERROR;
    
    return OperationStatus::FAILED; // Default
}

std::string ThreadTypeConverter::threadStateToString(int state) {
    switch (state) {
        case 0: return "created";
        case 1: return "running";
        case 2: return "paused";
        case 3: return "stopped";
        case 4: return "error";
        default: return "unknown";
    }
}

int ThreadTypeConverter::stringToThreadState(const std::string& state) {
    std::string lowerState = state;
    std::transform(lowerState.begin(), lowerState.end(), lowerState.begin(), ::tolower);
    
    if (lowerState == "created") return 0;
    if (lowerState == "running") return 1;
    if (lowerState == "paused") return 2;
    if (lowerState == "stopped") return 3;
    if (lowerState == "error") return 4;
    
    return 0; // Default
}

} // namespace URMavRouterShared
