#ifndef URWT_RPC_SCAN_NETWORKS_HANDLER_HPP
#define URWT_RPC_SCAN_NETWORKS_HANDLER_HPP

#include "urwt/rpc/handlers/i_request_handler.hpp"
#include "urwt/rpc/operation_tracker.hpp"
#include "urwt/api.hpp"
#include "ThreadManager.hpp"
#include <direct_template.hpp>
#include <memory>

namespace urwt {
namespace rpc {

class RPCService;

class ScanNetworksHandler : public IRequestHandler {
public:
    ScanNetworksHandler(
        std::shared_ptr<WirelessToolsAPI> api,
        std::shared_ptr<ThreadMgr::ThreadManager> threadManager,
        DirectTemplate::ClientThread* rpcClient,
        const std::string& responseTopic,
        std::shared_ptr<OperationTracker> operationTracker,
        RPCService* rpcService
    );
    ~ScanNetworksHandler() override;

    RPCResponse handle(const RPCRequest& request) override;
    RPCAction getSupportedAction() const override;
    bool isAsync() const override { return true; }

private:
    std::shared_ptr<WirelessToolsAPI> api_;
    std::shared_ptr<ThreadMgr::ThreadManager> thread_manager_;
    DirectTemplate::ClientThread* rpc_client_;
    std::string response_topic_;
    std::shared_ptr<OperationTracker> operation_tracker_;
    RPCService* rpc_service_;
    
    void performScanInThread(const std::string& transactionId, const std::string& interfaceName);
};

} // namespace rpc
} // namespace urwt

#endif // URWT_RPC_SCAN_NETWORKS_HANDLER_HPP
