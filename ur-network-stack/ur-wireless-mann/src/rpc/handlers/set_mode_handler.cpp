#include "urwt/rpc/handlers/set_mode_handler.hpp"
#include "urwt/rpc/rpc_service.hpp"
#include "urwt/config/wireless_config_types.hpp"

namespace urwt {
namespace rpc {

SetModeHandler::SetModeHandler(
    std::shared_ptr<mode::ModeController> modeController,
    std::shared_ptr<config::WirelessConfigManager> configManager,
    std::shared_ptr<ThreadMgr::ThreadManager> threadManager,
    DirectTemplate::ClientThread* rpcClient,
    const std::string& responseTopic,
    std::shared_ptr<OperationTracker> operationTracker,
    RPCService* rpcService
) : mode_controller_(modeController)
  , config_manager_(configManager)
  , thread_manager_(threadManager)
  , rpc_client_(rpcClient)
  , response_topic_(responseTopic)
  , operation_tracker_(operationTracker)
  , rpc_service_(rpcService) {}

SetModeHandler::~SetModeHandler() {}

RPCResponse SetModeHandler::handle(const RPCRequest& request) {
    if (!request.params.contains("mode") || !request.params["mode"].is_string()) {
        operation_tracker_->completeOperation(request.transaction_id, false);
        return createErrorResponse(
            request.transaction_id,
            ErrorCodes::INVALID_PARAMS,
            "Missing or invalid 'mode' parameter"
        );
    }
    
    std::string mode = request.params["mode"].get<std::string>();
    std::string transactionId = request.transaction_id;
    
    try {
        auto threadId = thread_manager_->createThread([this, transactionId, mode]() {
            performModeSwitchInThread(transactionId, mode);
        });
        
        uint64_t workerNum = rpc_service_->getNextWorkerNumber();
        std::string attachmentId = "worker-setmode-" + std::to_string(workerNum) + "-" + transactionId;
        thread_manager_->registerThread(threadId, attachmentId);
        
        json acceptedData;
        acceptedData["message"] = "Mode switch operation started";
        acceptedData["mode"] = mode;
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

void SetModeHandler::performModeSwitchInThread(const std::string& transactionId, const std::string& modeStr) {
    try {
        auto startTime = std::chrono::steady_clock::now();
        
        auto targetMode = config::stringToWirelessMode(modeStr);
        if (targetMode == config::WirelessMode::Unknown) {
            auto response = createErrorResponse(
                transactionId,
                ErrorCodes::INVALID_PARAMS,
                "Invalid mode: " + modeStr
            );
            rpc_client_->publishRawMessage(response_topic_, response.toJSON());
            operation_tracker_->completeOperation(transactionId, false);
            return;
        }
        
        auto wirelessConfig = config_manager_->getConfig();
        auto result = mode_controller_->switchMode(targetMode, wirelessConfig);
        
        auto endTime = std::chrono::steady_clock::now();
        auto executionTime = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime).count();
        
        if (!result.isOk()) {
            auto response = createErrorResponse(
                transactionId,
                ErrorCodes::OPERATION_FAILED,
                result.error()
            );
            rpc_client_->publishRawMessage(response_topic_, response.toJSON());
            operation_tracker_->completeOperation(transactionId, false);
            return;
        }
        
        // Update config
        config_manager_->setMode(targetMode);
        
        json data;
        data["message"] = "Mode switched successfully";
        data["mode"] = modeStr;
        
        auto response = createSuccessResponse(transactionId, data, executionTime);
        rpc_client_->publishRawMessage(response_topic_, response.toJSON());
        operation_tracker_->completeOperation(transactionId, true);
    } catch (const std::exception& e) {
        std::cerr << "Worker thread exception: " << e.what() << std::endl;
        operation_tracker_->completeOperation(transactionId, false);
    }
}

RPCAction SetModeHandler::getSupportedAction() const {
    return RPCAction::SetMode;
}

} // namespace rpc
} // namespace urwt
