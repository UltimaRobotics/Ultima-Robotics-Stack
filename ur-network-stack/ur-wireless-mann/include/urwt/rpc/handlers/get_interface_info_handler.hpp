#ifndef URWT_RPC_GET_INTERFACE_INFO_HANDLER_HPP
#define URWT_RPC_GET_INTERFACE_INFO_HANDLER_HPP

#include "urwt/rpc/handlers/i_request_handler.hpp"
#include "urwt/api.hpp"
#include <memory>

namespace urwt {
namespace rpc {

class RPCService;

class GetInterfaceInfoHandler : public IRequestHandler {
public:
    explicit GetInterfaceInfoHandler(std::shared_ptr<WirelessToolsAPI> api, RPCService* rpcService);
    ~GetInterfaceInfoHandler() override;

    RPCResponse handle(const RPCRequest& request) override;
    RPCAction getSupportedAction() const override;

private:
    std::shared_ptr<WirelessToolsAPI> api_;
    RPCService* rpc_service_;
};

} // namespace rpc
} // namespace urwt

#endif // URWT_RPC_GET_INTERFACE_INFO_HANDLER_HPP
