#ifndef URWT_RPC_REMOVE_NETWORK_HANDLER_HPP
#define URWT_RPC_REMOVE_NETWORK_HANDLER_HPP

#include "urwt/rpc/handlers/i_request_handler.hpp"
#include "urwt/rpc/operation_tracker.hpp"
#include "urwt/network/saved_network_manager.hpp"
#include <memory>

namespace urwt {
namespace rpc {

class RPCService;

class RemoveNetworkHandler : public IRequestHandler {
public:
    RemoveNetworkHandler(
        std::shared_ptr<network::SavedNetworkManager> networkManager,
        std::shared_ptr<OperationTracker> operationTracker,
        RPCService* rpcService
    );
    ~RemoveNetworkHandler() override;

    RPCResponse handle(const RPCRequest& request) override;
    RPCAction getSupportedAction() const override;
    bool isAsync() const override { return false; }

private:
    std::shared_ptr<network::SavedNetworkManager> network_manager_;
    std::shared_ptr<OperationTracker> operation_tracker_;
    RPCService* rpc_service_;
};

} // namespace rpc
} // namespace urwt

#endif // URWT_RPC_REMOVE_NETWORK_HANDLER_HPP
