#include "urwt/rpc/handlers/update_wireless_config_handler.hpp"
#include "urwt/rpc/rpc_service.hpp"

namespace urwt {
namespace rpc {

UpdateWirelessConfigHandler::UpdateWirelessConfigHandler(
    std::shared_ptr<config::WirelessConfigManager> configManager,
    std::shared_ptr<ThreadMgr::ThreadManager> threadManager,
    DirectTemplate::ClientThread* rpcClient,
    const std::string& responseTopic,
    std::shared_ptr<OperationTracker> operationTracker,
    RPCService* rpcService
) : config_manager_(configManager)
  , thread_manager_(threadManager)
  , rpc_client_(rpcClient)
  , response_topic_(responseTopic)
  , operation_tracker_(operationTracker)
  , rpc_service_(rpcService) {}

UpdateWirelessConfigHandler::~UpdateWirelessConfigHandler() {}

RPCResponse UpdateWirelessConfigHandler::handle(const RPCRequest& request) {
    if (!request.params.is_object() || request.params.empty()) {
        operation_tracker_->completeOperation(request.transaction_id, false);
        return createErrorResponse(
            request.transaction_id,
            ErrorCodes::INVALID_PARAMS,
            "Invalid or empty configuration updates"
        );
    }
    
    json configUpdates = request.params;
    std::string transactionId = request.transaction_id;
    
    try {
        auto threadId = thread_manager_->createThread([this, transactionId, configUpdates]() {
            performConfigUpdateInThread(transactionId, configUpdates);
        });
        
        uint64_t workerNum = rpc_service_->getNextWorkerNumber();
        std::string attachmentId = "worker-updateconfig-" + std::to_string(workerNum) + "-" + transactionId;
        thread_manager_->registerThread(threadId, attachmentId);
        
        json acceptedData;
        acceptedData["message"] = "Configuration update operation started";
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

void UpdateWirelessConfigHandler::performConfigUpdateInThread(const std::string& transactionId, const json& configUpdates) {
    try {
        auto startTime = std::chrono::steady_clock::now();
        
        // Apply configuration updates
        if (configUpdates.contains("enabled") && configUpdates["enabled"].is_boolean()) {
            auto result = config_manager_->setWirelessEnabled(configUpdates["enabled"].get<bool>());
            if (!result.isOk()) {
                auto response = createErrorResponse(
                    transactionId,
                    ErrorCodes::OPERATION_FAILED,
                    "Failed to update enabled: " + result.error()
                );
                rpc_client_->publishRawMessage(response_topic_, response.toJSON());
                operation_tracker_->completeOperation(transactionId, false);
                return;
            }
        }
        
        if (configUpdates.contains("automation") && configUpdates["automation"].is_object()) {
            auto automation = configUpdates["automation"];
            if (automation.contains("enabled") && automation["enabled"].is_boolean()) {
                auto result = config_manager_->setAutomationEnabled(automation["enabled"].get<bool>());
                if (!result.isOk()) {
                    auto response = createErrorResponse(
                        transactionId,
                        ErrorCodes::OPERATION_FAILED,
                        "Failed to update automation: " + result.error()
                    );
                    rpc_client_->publishRawMessage(response_topic_, response.toJSON());
                    operation_tracker_->completeOperation(transactionId, false);
                    return;
                }
            }
        }
        
        // Persist changes
        auto persistResult = config_manager_->persist();
        
        auto endTime = std::chrono::steady_clock::now();
        auto executionTime = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime).count();
        
        if (!persistResult.isOk()) {
            auto response = createErrorResponse(
                transactionId,
                ErrorCodes::OPERATION_FAILED,
                "Failed to persist configuration: " + persistResult.error()
            );
            rpc_client_->publishRawMessage(response_topic_, response.toJSON());
            operation_tracker_->completeOperation(transactionId, false);
            return;
        }
        
        json data;
        data["message"] = "Configuration updated successfully";
        
        auto response = createSuccessResponse(transactionId, data, executionTime);
        rpc_client_->publishRawMessage(response_topic_, response.toJSON());
        operation_tracker_->completeOperation(transactionId, true);
    } catch (const std::exception& e) {
        std::cerr << "Worker thread exception: " << e.what() << std::endl;
        operation_tracker_->completeOperation(transactionId, false);
    }
}

RPCAction UpdateWirelessConfigHandler::getSupportedAction() const {
    return RPCAction::UpdateWirelessConfig;
}

} // namespace rpc
} // namespace urwt
