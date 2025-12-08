#ifndef URWT_RPC_SET_MODE_HANDLER_HPP
#define URWT_RPC_SET_MODE_HANDLER_HPP

#include "urwt/rpc/handlers/i_request_handler.hpp"
#include "urwt/rpc/operation_tracker.hpp"
#include "urwt/mode/mode_controller.hpp"
#include "urwt/config/wireless_config_manager.hpp"
#include "ThreadManager.hpp"
#include <direct_template.hpp>
#include <memory>

namespace urwt {
namespace rpc {

class RPCService;

class SetModeHandler : public IRequestHandler {
public:
    SetModeHandler(
        std::shared_ptr<mode::ModeController> modeController,
        std::shared_ptr<config::WirelessConfigManager> configManager,
        std::shared_ptr<ThreadMgr::ThreadManager> threadManager,
        DirectTemplate::ClientThread* rpcClient,
        const std::string& responseTopic,
        std::shared_ptr<OperationTracker> operationTracker,
        RPCService* rpcService
    );
    ~SetModeHandler() override;

    RPCResponse handle(const RPCRequest& request) override;
    RPCAction getSupportedAction() const override;
    bool isAsync() const override { return true; }

private:
    std::shared_ptr<mode::ModeController> mode_controller_;
    std::shared_ptr<config::WirelessConfigManager> config_manager_;
    std::shared_ptr<ThreadMgr::ThreadManager> thread_manager_;
    DirectTemplate::ClientThread* rpc_client_;
    std::string response_topic_;
    std::shared_ptr<OperationTracker> operation_tracker_;
    RPCService* rpc_service_;

    void performModeSwitchInThread(const std::string& transactionId, const std::string& mode);
};

} // namespace rpc
} // namespace urwt

#endif // URWT_RPC_SET_MODE_HANDLER_HPP
