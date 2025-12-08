#include "urwt/rpc/handlers/list_interfaces_handler.hpp"

namespace urwt {
namespace rpc {

ListInterfacesHandler::ListInterfacesHandler(std::shared_ptr<WirelessToolsAPI> api)
    : api_(api) {}

ListInterfacesHandler::~ListInterfacesHandler() {}

RPCResponse ListInterfacesHandler::handle(const RPCRequest& request) {
    auto startTime = std::chrono::steady_clock::now();
    
    auto result = api_->listInterfaces();
    
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
    
    json interfacesArray = json::array();
    for (const auto& iface : result.value()) {
        json ifaceObj;
        ifaceObj["name"] = iface.name();
        ifaceObj["mac_address"] = iface.mac().toString();
        ifaceObj["status"] = to_string(iface.status());
        ifaceObj["driver"] = iface.type();
        interfacesArray.push_back(ifaceObj);
    }
    
    json data;
    data["interfaces"] = interfacesArray;
    data["count"] = result.value().size();
    
    return createSuccessResponse(request.transaction_id, data, executionTime);
}

RPCAction ListInterfacesHandler::getSupportedAction() const {
    return RPCAction::ListInterfaces;
}

} // namespace rpc
} // namespace urwt
