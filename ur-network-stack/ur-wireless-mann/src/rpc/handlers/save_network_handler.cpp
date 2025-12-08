#include "urwt/rpc/handlers/save_network_handler.hpp"
#include "urwt/rpc/rpc_service.hpp"
#include "urwt/config/wireless_config_types.hpp"

namespace urwt {
namespace rpc {

SaveNetworkHandler::SaveNetworkHandler(
    std::shared_ptr<network::SavedNetworkManager> networkManager,
    std::shared_ptr<OperationTracker> operationTracker,
    RPCService* rpcService
) : network_manager_(networkManager)
  , operation_tracker_(operationTracker)
  , rpc_service_(rpcService) {}

SaveNetworkHandler::~SaveNetworkHandler() {}

RPCResponse SaveNetworkHandler::handle(const RPCRequest& request) {
    auto startTime = std::chrono::steady_clock::now();
    
    if (!request.params.contains("ssid") || !request.params["ssid"].is_string()) {
        operation_tracker_->completeOperation(request.transaction_id, false);
        return createErrorResponse(
            request.transaction_id,
            ErrorCodes::INVALID_PARAMS,
            "Missing or invalid 'ssid' parameter"
        );
    }
    
    config::NetworkProfile profile;
    profile.ssid = request.params["ssid"].get<std::string>();
    
    if (request.params.contains("password") && request.params["password"].is_string()) {
        profile.password = request.params["password"].get<std::string>();
    }
    
    if (request.params.contains("security") && request.params["security"].is_string()) {
        profile.security = config::stringToSecurityType(request.params["security"].get<std::string>());
    } else {
        profile.security = config::SecurityType::WPA2;
    }
    
    if (request.params.contains("priority") && request.params["priority"].is_number()) {
        profile.priority = request.params["priority"].get<int>();
    }
    
    if (request.params.contains("auto_connect") && request.params["auto_connect"].is_boolean()) {
        profile.auto_connect = request.params["auto_connect"].get<bool>();
    }
    
    if (request.params.contains("hidden") && request.params["hidden"].is_boolean()) {
        profile.hidden = request.params["hidden"].get<bool>();
    }
    
    auto result = network_manager_->addNetwork(profile);
    
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
    data["message"] = "Network saved successfully";
    data["ssid"] = profile.ssid;
    
    operation_tracker_->completeOperation(request.transaction_id, true);
    return createSuccessResponse(request.transaction_id, data, executionTime);
}

RPCAction SaveNetworkHandler::getSupportedAction() const {
    return RPCAction::SaveNetwork;
}

} // namespace rpc
} // namespace urwt
