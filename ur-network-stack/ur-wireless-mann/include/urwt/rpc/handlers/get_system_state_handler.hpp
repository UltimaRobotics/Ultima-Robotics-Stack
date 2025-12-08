#ifndef URWT_RPC_GET_SYSTEM_STATE_HANDLER_HPP
#define URWT_RPC_GET_SYSTEM_STATE_HANDLER_HPP

#include "urwt/rpc/handlers/i_request_handler.hpp"
#include "urwt/rpc/operation_tracker.hpp"
#include "urwt/state/system_state_analyzer.hpp"
#include <memory>

namespace urwt {
namespace rpc {

class RPCService;

class GetSystemStateHandler : public IRequestHandler {
public:
    GetSystemStateHandler(
        std::shared_ptr<state::SystemStateAnalyzer> stateAnalyzer,
        std::shared_ptr<OperationTracker> operationTracker,
        RPCService* rpcService
    );
    ~GetSystemStateHandler() override;

    RPCResponse handle(const RPCRequest& request) override;
    RPCAction getSupportedAction() const override;
    bool isAsync() const override { return false; }

private:
    std::shared_ptr<state::SystemStateAnalyzer> state_analyzer_;
    std::shared_ptr<OperationTracker> operation_tracker_;
    RPCService* rpc_service_;
};

} // namespace rpc
} // namespace urwt

#endif // URWT_RPC_GET_SYSTEM_STATE_HANDLER_HPP
