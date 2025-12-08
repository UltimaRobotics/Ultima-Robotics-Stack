#ifndef URWT_RPC_REQUEST_DISPATCHER_HPP
#define URWT_RPC_REQUEST_DISPATCHER_HPP

#include "urwt/rpc/rpc_types.hpp"
#include "urwt/rpc/handlers/i_request_handler.hpp"
#include <memory>
#include <map>

namespace urwt {
namespace rpc {

class RequestDispatcher {
public:
    RequestDispatcher();
    ~RequestDispatcher();

    void registerHandler(RPCAction action, std::shared_ptr<IRequestHandler> handler);
    void unregisterHandler(RPCAction action);
    
    RPCResponse dispatch(const RPCRequest& request);
    
    bool hasHandler(RPCAction action) const;
    bool isHandlerAsync(RPCAction action) const;
    size_t getHandlerCount() const;

private:
    std::map<RPCAction, std::shared_ptr<IRequestHandler>> handlers_;
};

} // namespace rpc
} // namespace urwt

#endif // URWT_RPC_REQUEST_DISPATCHER_HPP
