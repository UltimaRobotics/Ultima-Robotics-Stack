#include "urwt/rpc/handlers/set_automation_handler.hpp"
#include "urwt/rpc/rpc_service.hpp"

namespace urwt {
namespace rpc {

SetAutomationHandler::SetAutomationHandler(
    std::shared_ptr<config::WirelessConfigManager> configManager,
    std::shared_ptr<OperationTracker> operationTracker,
    RPCService* rpcService
) : config_manager_(configManager)
  , operation_tracker_(operationTracker)
  , rpc_service_(rpcService) {}

SetAutomationHandler::~SetAutomationHandler() {}

RPCResponse SetAutomationHandler::handle(const RPCRequest& request) {
    auto startTime = std::chrono::steady_clock::now();
    
    if (!request.params.contains("enabled") || !request.params["enabled"].is_boolean()) {
        operation_tracker_->completeOperation(request.transaction_id, false);
        return createErrorResponse(
            request.transaction_id,
            ErrorCodes::INVALID_PARAMS,
            "Missing or invalid 'enabled' parameter"
        );
    }
    
    bool enabled = request.params["enabled"].get<bool>();
    auto result = config_manager_->setAutomationEnabled(enabled);
    
    auto endTime = std::chrono::steady_clock::now();
    auto executionTime = std::chrono::duration_cast<std::chrono::milliseconds>(
        endTime - startTime).count();
    
    if (!result.isOk()) {
        operation_tracker_->completeOperation(request.transaction_id, false);
        return createErrorResponse(
            request.transaction_id,
            ErrorCodes::OPERATION_FAILED,
            result.error()
        );
    }
    
    json data;
    data["message"] = "Automation setting updated";
    data["enabled"] = enabled;
    
    operation_tracker_->completeOperation(request.transaction_id, true);
    return createSuccessResponse(request.transaction_id, data, executionTime);
}

RPCAction SetAutomationHandler::getSupportedAction() const {
    return RPCAction::SetAutomation;
}

} // namespace rpc
} // namespace urwt
