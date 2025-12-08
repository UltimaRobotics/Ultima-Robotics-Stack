#ifndef URWT_RPC_SHUTDOWN_HANDLER_HPP
#define URWT_RPC_SHUTDOWN_HANDLER_HPP

#include "urwt/rpc/handlers/i_request_handler.hpp"
#include "urwt/rpc/operation_tracker.hpp"
#include <memory>
#include <atomic>

namespace urwt {
namespace rpc {

class ShutdownHandler : public IRequestHandler {
public:
    ShutdownHandler(
        std::shared_ptr<OperationTracker> opTracker,
        std::atomic<bool>& shutdownFlag
    );
    ~ShutdownHandler() override;

    RPCResponse handle(const RPCRequest& request) override;
    RPCAction getSupportedAction() const override;

private:
    std::shared_ptr<OperationTracker> op_tracker_;
    std::atomic<bool>& shutdown_flag_;
};

} // namespace rpc
} // namespace urwt

#endif // URWT_RPC_SHUTDOWN_HANDLER_HPP
