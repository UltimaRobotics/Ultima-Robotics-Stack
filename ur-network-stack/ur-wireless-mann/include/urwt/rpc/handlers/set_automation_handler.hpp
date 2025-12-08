#ifndef URWT_RPC_SET_AUTOMATION_HANDLER_HPP
#define URWT_RPC_SET_AUTOMATION_HANDLER_HPP

#include "urwt/rpc/handlers/i_request_handler.hpp"
#include "urwt/rpc/operation_tracker.hpp"
#include "urwt/config/wireless_config_manager.hpp"
#include <memory>

namespace urwt {
namespace rpc {

class RPCService;

class SetAutomationHandler : public IRequestHandler {
public:
    SetAutomationHandler(
        std::shared_ptr<config::WirelessConfigManager> configManager,
        std::shared_ptr<OperationTracker> operationTracker,
        RPCService* rpcService
    );
    ~SetAutomationHandler() override;

    RPCResponse handle(const RPCRequest& request) override;
    RPCAction getSupportedAction() const override;
    bool isAsync() const override { return false; }

private:
    std::shared_ptr<config::WirelessConfigManager> config_manager_;
    std::shared_ptr<OperationTracker> operation_tracker_;
    RPCService* rpc_service_;
};

} // namespace rpc
} // namespace urwt

#endif // URWT_RPC_SET_AUTOMATION_HANDLER_HPP
