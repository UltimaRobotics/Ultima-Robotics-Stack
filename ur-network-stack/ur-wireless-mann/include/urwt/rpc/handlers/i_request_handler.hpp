#ifndef URWT_RPC_I_REQUEST_HANDLER_HPP
#define URWT_RPC_I_REQUEST_HANDLER_HPP

#include "urwt/rpc/rpc_types.hpp"

namespace urwt {
namespace rpc {

class IRequestHandler {
public:
    virtual ~IRequestHandler() = default;

    virtual RPCResponse handle(const RPCRequest& request) = 0;
    virtual RPCAction getSupportedAction() const = 0;
    virtual bool isAsync() const { return false; }
};

} // namespace rpc
} // namespace urwt

#endif // URWT_RPC_I_REQUEST_HANDLER_HPP
