#ifndef URWT_RPC_GET_STATUS_HANDLER_HPP
#define URWT_RPC_GET_STATUS_HANDLER_HPP

#include "urwt/rpc/handlers/i_request_handler.hpp"
#include "urwt/rpc/operation_tracker.hpp"
#include "ThreadManager.hpp"
#include <direct_template.hpp>
#include <memory>
#include <chrono>

namespace urwt {
namespace rpc {

class GetStatusHandler : public IRequestHandler {
public:
    GetStatusHandler(
        std::shared_ptr<OperationTracker> opTracker,
        std::shared_ptr<ThreadMgr::ThreadManager> threadManager,
        DirectTemplate::ClientThread* rpcClient,
        const std::string& brokerAddress
    );
    ~GetStatusHandler() override;

    RPCResponse handle(const RPCRequest& request) override;
    RPCAction getSupportedAction() const override;
    
    void setStartTime(std::chrono::steady_clock::time_point startTime);

private:
    std::shared_ptr<OperationTracker> op_tracker_;
    std::shared_ptr<ThreadMgr::ThreadManager> thread_manager_;
    DirectTemplate::ClientThread* rpc_client_;
    std::string broker_address_;
    std::chrono::steady_clock::time_point start_time_;
};

} // namespace rpc
} // namespace urwt

#endif // URWT_RPC_GET_STATUS_HANDLER_HPP
