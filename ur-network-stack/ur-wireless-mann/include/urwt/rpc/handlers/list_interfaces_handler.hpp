#ifndef URWT_RPC_LIST_INTERFACES_HANDLER_HPP
#define URWT_RPC_LIST_INTERFACES_HANDLER_HPP

#include "urwt/rpc/handlers/i_request_handler.hpp"
#include "urwt/api.hpp"
#include <memory>

namespace urwt {
namespace rpc {

class ListInterfacesHandler : public IRequestHandler {
public:
    explicit ListInterfacesHandler(std::shared_ptr<WirelessToolsAPI> api);
    ~ListInterfacesHandler() override;

    RPCResponse handle(const RPCRequest& request) override;
    RPCAction getSupportedAction() const override;

private:
    std::shared_ptr<WirelessToolsAPI> api_;
};

} // namespace rpc
} // namespace urwt

#endif // URWT_RPC_LIST_INTERFACES_HANDLER_HPP
