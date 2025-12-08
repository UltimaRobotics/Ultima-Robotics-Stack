#include "urwt/rpc/handlers/shutdown_handler.hpp"
#include <thread>

namespace urwt {
namespace rpc {

ShutdownHandler::ShutdownHandler(
    std::shared_ptr<OperationTracker> opTracker,
    std::atomic<bool>& shutdownFlag
) : op_tracker_(opTracker)
  , shutdown_flag_(shutdownFlag) {}

ShutdownHandler::~ShutdownHandler() {}

RPCResponse ShutdownHandler::handle(const RPCRequest& request) {
    auto startTime = std::chrono::steady_clock::now();
    
    bool waitForCompletion = true;
    int timeoutSeconds = 30;
    
    if (request.params.contains("wait_for_completion") && request.params["wait_for_completion"].is_boolean()) {
        waitForCompletion = request.params["wait_for_completion"].get<bool>();
    }
    
    if (request.params.contains("timeout_seconds") && request.params["timeout_seconds"].is_number()) {
        timeoutSeconds = request.params["timeout_seconds"].get<int>();
    }
    
    size_t activeOpsAtStart = op_tracker_->getActiveCount();
    
    shutdown_flag_ = true;
    
    if (waitForCompletion) {
        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(timeoutSeconds);
        
        while (op_tracker_->getActiveCount() > 0 && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    
    auto endTime = std::chrono::steady_clock::now();
    auto executionTime = std::chrono::duration_cast<std::chrono::milliseconds>(
        endTime - startTime).count();
    
    json data;
    data["message"] = "Shutdown initiated";
    data["active_operations_terminated"] = activeOpsAtStart;
    data["graceful_shutdown"] = (op_tracker_->getActiveCount() == 0);
    
    return createSuccessResponse(request.transaction_id, data, executionTime);
}

RPCAction ShutdownHandler::getSupportedAction() const {
    return RPCAction::Shutdown;
}

} // namespace rpc
} // namespace urwt
