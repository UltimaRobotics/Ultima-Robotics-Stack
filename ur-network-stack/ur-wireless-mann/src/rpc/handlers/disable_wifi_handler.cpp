#include "urwt/rpc/handlers/disable_wifi_handler.hpp"
#include "urwt/rpc/rpc_service.hpp"
#include "urwt/utils/process_executor.hpp"

namespace urwt {
namespace rpc {

DisableWifiHandler::DisableWifiHandler(
    std::shared_ptr<config::WirelessConfigManager> configManager,
    std::shared_ptr<state::SystemStateAnalyzer> stateAnalyzer,
    std::shared_ptr<ThreadMgr::ThreadManager> threadManager,
    DirectTemplate::ClientThread* rpcClient,
    const std::string& responseTopic,
    std::shared_ptr<OperationTracker> operationTracker,
    RPCService* rpcService
) : config_manager_(configManager)
  , state_analyzer_(stateAnalyzer)
  , thread_manager_(threadManager)
  , rpc_client_(rpcClient)
  , response_topic_(responseTopic)
  , operation_tracker_(operationTracker)
  , rpc_service_(rpcService) {}

DisableWifiHandler::~DisableWifiHandler() {}

RPCResponse DisableWifiHandler::handle(const RPCRequest& request) {
    std::string interface = "wlan0";
    if (request.params.contains("interface") && request.params["interface"].is_string()) {
        interface = request.params["interface"].get<std::string>();
    }
    
    std::string effectiveInterface = rpc_service_->getEffectiveInterface(interface);
    std::string transactionId = request.transaction_id;
    
    try {
        auto threadId = thread_manager_->createThread([this, transactionId, effectiveInterface]() {
            performDisableWifiInThread(transactionId, effectiveInterface);
        });
        
        uint64_t workerNum = rpc_service_->getNextWorkerNumber();
        std::string attachmentId = "worker-disablewifi-" + std::to_string(workerNum) + "-" + transactionId;
        thread_manager_->registerThread(threadId, attachmentId);
        
        json acceptedData;
        acceptedData["message"] = "Disable WiFi operation started";
        acceptedData["interface"] = interface;
        acceptedData["worker_thread_id"] = threadId;
        
        return createSuccessResponse(transactionId, acceptedData, 0);
    } catch (const std::exception& e) {
        operation_tracker_->completeOperation(request.transaction_id, false);
        return createErrorResponse(
            request.transaction_id,
            ErrorCodes::INTERNAL_ERROR,
            std::string("Failed to spawn worker thread: ") + e.what()
        );
    }
}

void DisableWifiHandler::performDisableWifiInThread(const std::string& transactionId, const std::string& interface) {
    try {
        auto startTime = std::chrono::steady_clock::now();
        
        ProcessExecutor executor;
        
        // Kill any running processes
        executor.executeShell("killall wpa_supplicant 2>/dev/null", std::chrono::seconds(5));
        executor.executeShell("killall hostapd 2>/dev/null", std::chrono::seconds(5));
        
        // Bring interface down
        auto downResult = executor.executeShell("ip link set " + interface + " down 2>&1", std::chrono::seconds(5));
        
        auto endTime = std::chrono::steady_clock::now();
        auto executionTime = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime).count();
        
        if (!downResult.isOk() || downResult.value().exit_code != 0) {
            auto response = createErrorResponse(
                transactionId,
                ErrorCodes::OPERATION_FAILED,
                "Failed to bring interface down: " + (downResult.isOk() ? downResult.value().stderr_output : downResult.error())
            );
            rpc_client_->publishRawMessage(response_topic_, response.toJSON());
            operation_tracker_->completeOperation(transactionId, false);
            return;
        }
        
        // Update config
        config_manager_->setWirelessEnabled(false);
        
        json data;
        data["message"] = "WiFi disabled successfully";
        data["interface"] = interface;
        
        auto response = createSuccessResponse(transactionId, data, executionTime);
        rpc_client_->publishRawMessage(response_topic_, response.toJSON());
        operation_tracker_->completeOperation(transactionId, true);
    } catch (const std::exception& e) {
        std::cerr << "Worker thread exception: " << e.what() << std::endl;
        operation_tracker_->completeOperation(transactionId, false);
    }
}

RPCAction DisableWifiHandler::getSupportedAction() const {
    return RPCAction::DisableWifi;
}

} // namespace rpc
} // namespace urwt
