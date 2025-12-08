#include "urwt/rpc/handlers/get_interface_info_handler.hpp"
#include "urwt/rpc/rpc_service.hpp"

namespace urwt {
namespace rpc {

GetInterfaceInfoHandler::GetInterfaceInfoHandler(std::shared_ptr<WirelessToolsAPI> api, RPCService* rpcService)
    : api_(api), rpc_service_(rpcService) {}

GetInterfaceInfoHandler::~GetInterfaceInfoHandler() {}

RPCResponse GetInterfaceInfoHandler::handle(const RPCRequest& request) {
    auto startTime = std::chrono::steady_clock::now();
    
    if (!request.params.contains("interface") || !request.params["interface"].is_string()) {
        return createErrorResponse(
            request.transaction_id,
            ErrorCodes::INVALID_PARAMS,
            "Missing or invalid 'interface' parameter"
        );
    }
    
    std::string interfaceName = request.params["interface"].get<std::string>();
    std::string effectiveInterface = rpc_service_->getEffectiveInterface(interfaceName);
    
    auto result = api_->getInterface(effectiveInterface);
    
    auto endTime = std::chrono::steady_clock::now();
    auto executionTime = std::chrono::duration_cast<std::chrono::milliseconds>(
        endTime - startTime).count();
    
    if (!result.isOk()) {
        return createErrorResponse(
            request.transaction_id,
            ErrorCodes::OPERATION_FAILED,
            result.error()
        );
    }
    
    json data;
    data["name"] = result.value().name();
    data["mac_address"] = result.value().mac().toString();
    data["status"] = to_string(result.value().status());
    data["driver"] = result.value().type();
    
    json capabilities;
    capabilities["supported_bands"] = json::array({"2.4GHz", "5GHz"});
    capabilities["max_tx_power"] = 20;
    capabilities["supported_modes"] = json::array({"managed", "monitor"});
    data["capabilities"] = capabilities;
    
    return createSuccessResponse(request.transaction_id, data, executionTime);
}

RPCAction GetInterfaceInfoHandler::getSupportedAction() const {
    return RPCAction::GetInterfaceInfo;
}

} // namespace rpc
} // namespace urwt
