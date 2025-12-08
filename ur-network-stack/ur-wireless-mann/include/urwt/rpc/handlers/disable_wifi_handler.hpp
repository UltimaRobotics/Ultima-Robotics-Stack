#ifndef URWT_RPC_DISABLE_WIFI_HANDLER_HPP
#define URWT_RPC_DISABLE_WIFI_HANDLER_HPP

#include "urwt/rpc/handlers/i_request_handler.hpp"
#include "urwt/rpc/operation_tracker.hpp"
#include "urwt/config/wireless_config_manager.hpp"
#include "urwt/state/system_state_analyzer.hpp"
#include "ThreadManager.hpp"
#include <direct_template.hpp>
#include <memory>

namespace urwt {
namespace rpc {

class RPCService;

class DisableWifiHandler : public IRequestHandler {
public:
    DisableWifiHandler(
        std::shared_ptr<config::WirelessConfigManager> configManager,
        std::shared_ptr<state::SystemStateAnalyzer> stateAnalyzer,
        std::shared_ptr<ThreadMgr::ThreadManager> threadManager,
        DirectTemplate::ClientThread* rpcClient,
        const std::string& responseTopic,
        std::shared_ptr<OperationTracker> operationTracker,
        RPCService* rpcService
    );
    ~DisableWifiHandler() override;

    RPCResponse handle(const RPCRequest& request) override;
    RPCAction getSupportedAction() const override;
    bool isAsync() const override { return true; }

private:
    std::shared_ptr<config::WirelessConfigManager> config_manager_;
    std::shared_ptr<state::SystemStateAnalyzer> state_analyzer_;
    std::shared_ptr<ThreadMgr::ThreadManager> thread_manager_;
    DirectTemplate::ClientThread* rpc_client_;
    std::string response_topic_;
    std::shared_ptr<OperationTracker> operation_tracker_;
    RPCService* rpc_service_;

    void performDisableWifiInThread(const std::string& transactionId, const std::string& interface);
};

} // namespace rpc
} // namespace urwt

#endif // URWT_RPC_DISABLE_WIFI_HANDLER_HPP
