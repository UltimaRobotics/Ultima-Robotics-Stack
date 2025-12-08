#include "urwt/rpc/handlers/list_saved_networks_handler.hpp"
#include "urwt/rpc/rpc_service.hpp"
#include "urwt/config/wireless_config_types.hpp"

namespace urwt {
namespace rpc {

ListSavedNetworksHandler::ListSavedNetworksHandler(
    std::shared_ptr<network::SavedNetworkManager> networkManager,
    std::shared_ptr<OperationTracker> operationTracker,
    RPCService* rpcService
) : network_manager_(networkManager)
  , operation_tracker_(operationTracker)
  , rpc_service_(rpcService) {}

ListSavedNetworksHandler::~ListSavedNetworksHandler() {}

RPCResponse ListSavedNetworksHandler::handle(const RPCRequest& request) {
    auto startTime = std::chrono::steady_clock::now();
    
    auto networks = network_manager_->getAllNetworks();
    
    json networksArray = json::array();
    for (const auto& network : networks) {
        json netObj;
        netObj["ssid"] = network.ssid;
        netObj["security"] = config::securityTypeToString(network.security);
        netObj["priority"] = network.priority;
        netObj["auto_connect"] = network.auto_connect;
        netObj["hidden"] = network.hidden;
        if (network.bssid) {
            netObj["bssid"] = *network.bssid;
        }
        networksArray.push_back(netObj);
    }
    
    auto endTime = std::chrono::steady_clock::now();
    auto executionTime = std::chrono::duration_cast<std::chrono::milliseconds>(
        endTime - startTime).count();
    
    json data;
    data["networks"] = networksArray;
    data["count"] = networks.size();
    
    operation_tracker_->completeOperation(request.transaction_id, true);
    return createSuccessResponse(request.transaction_id, data, executionTime);
}

RPCAction ListSavedNetworksHandler::getSupportedAction() const {
    return RPCAction::ListSavedNetworks;
}

} // namespace rpc
} // namespace urwt
