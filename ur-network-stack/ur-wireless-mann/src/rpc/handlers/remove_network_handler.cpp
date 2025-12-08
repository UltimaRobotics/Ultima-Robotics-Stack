#include "urwt/rpc/handlers/remove_network_handler.hpp"
#include "urwt/rpc/rpc_service.hpp"

namespace urwt {
namespace rpc {

RemoveNetworkHandler::RemoveNetworkHandler(
    std::shared_ptr<network::SavedNetworkManager> networkManager,
    std::shared_ptr<OperationTracker> operationTracker,
    RPCService* rpcService
) : network_manager_(networkManager)
  , operation_tracker_(operationTracker)
  , rpc_service_(rpcService) {}

RemoveNetworkHandler::~RemoveNetworkHandler() {}

RPCResponse RemoveNetworkHandler::handle(const RPCRequest& request) {
    auto startTime = std::chrono::steady_clock::now();
    
    if (!request.params.contains("ssid") || !request.params["ssid"].is_string()) {
        operation_tracker_->completeOperation(request.transaction_id, false);
        return createErrorResponse(
            request.transaction_id,
            ErrorCodes::INVALID_PARAMS,
            "Missing or invalid 'ssid' parameter"
        );
    }
    
    std::string ssid = request.params["ssid"].get<std::string>();
    auto result = network_manager_->removeNetwork(ssid);
    
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
    data["message"] = "Network removed successfully";
    data["ssid"] = ssid;
    
    operation_tracker_->completeOperation(request.transaction_id, true);
    return createSuccessResponse(request.transaction_id, data, executionTime);
}

RPCAction RemoveNetworkHandler::getSupportedAction() const {
    return RPCAction::RemoveNetwork;
}

} // namespace rpc
} // namespace urwt
