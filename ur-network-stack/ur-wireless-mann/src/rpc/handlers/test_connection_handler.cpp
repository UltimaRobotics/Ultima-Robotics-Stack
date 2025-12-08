#include "urwt/rpc/handlers/test_connection_handler.hpp"
#include "urwt/rpc/rpc_service.hpp"

namespace urwt {
namespace rpc {

TestConnectionHandler::TestConnectionHandler(
    std::shared_ptr<WirelessToolsAPI> api,
    std::shared_ptr<ThreadMgr::ThreadManager> threadManager,
    DirectTemplate::ClientThread* rpcClient,
    const std::string& responseTopic,
    std::shared_ptr<OperationTracker> operationTracker,
    RPCService* rpcService
) : api_(api)
  , thread_manager_(threadManager)
  , rpc_client_(rpcClient)
  , response_topic_(responseTopic)
  , operation_tracker_(operationTracker)
  , rpc_service_(rpcService) {}

TestConnectionHandler::~TestConnectionHandler() {}

RPCResponse TestConnectionHandler::handle(const RPCRequest& request) {
    if (!request.params.contains("interface") || !request.params["interface"].is_string()) {
        operation_tracker_->completeOperation(request.transaction_id, false);
        return createErrorResponse(
            request.transaction_id,
            ErrorCodes::INVALID_PARAMS,
            "Missing or invalid 'interface' parameter"
        );
    }
    
    if (!request.params.contains("ssid") || !request.params["ssid"].is_string()) {
        operation_tracker_->completeOperation(request.transaction_id, false);
        return createErrorResponse(
            request.transaction_id,
            ErrorCodes::INVALID_PARAMS,
            "Missing or invalid 'ssid' parameter"
        );
    }
    
    std::string interfaceName = request.params["interface"].get<std::string>();
    std::string effectiveInterface = rpc_service_->getEffectiveInterface(interfaceName);
    std::string ssid = request.params["ssid"].get<std::string>();
    std::string transactionId = request.transaction_id;
    
    try {
        auto threadId = thread_manager_->createThread([this, transactionId, effectiveInterface, ssid]() {
            performTestInThread(transactionId, effectiveInterface, ssid);
        });
        
        uint64_t workerNum = rpc_service_->getNextWorkerNumber();
        std::string attachmentId = "worker-test-" + std::to_string(workerNum) + "-" + transactionId;
        thread_manager_->registerThread(threadId, attachmentId);
        
        json acceptedData;
        acceptedData["message"] = "Connection test started";
        acceptedData["interface"] = interfaceName;
        acceptedData["ssid"] = ssid;
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

void TestConnectionHandler::performTestInThread(const std::string& transactionId, const std::string& interfaceName, const std::string& ssid) {
    try {
        auto startTime = std::chrono::steady_clock::now();
        
        auto ifaceResult = api_->getInterface(interfaceName);
        if (!ifaceResult.isOk()) {
            auto response = createErrorResponse(
                transactionId,
                ErrorCodes::OPERATION_FAILED,
                "Interface not found: " + ifaceResult.error()
            );
            rpc_client_->publishRawMessage(response_topic_, response.toJSON());
            operation_tracker_->completeOperation(transactionId, false);
            return;
        }
        
        auto testResult = api_->testConnection(ifaceResult.value(), ssid);
        
        auto endTime = std::chrono::steady_clock::now();
        auto executionTime = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime).count();
        
        if (!testResult.isOk()) {
            auto response = createErrorResponse(
                transactionId,
                ErrorCodes::OPERATION_FAILED,
                testResult.error()
            );
            rpc_client_->publishRawMessage(response_topic_, response.toJSON());
            operation_tracker_->completeOperation(transactionId, false);
            return;
        }
        
        json data;
        data["interface"] = testResult.value().interface;
        data["ssid"] = testResult.value().ssid;
        data["connection_type"] = testResult.value().connection_type;
        data["result"] = testResult.value().success ? "success" : "failed";
        data["duration_ms"] = testResult.value().duration.count();
        data["was_connected"] = testResult.value().was_connected;
        
        if (testResult.value().original_ssid.has_value()) {
            data["original_ssid"] = testResult.value().original_ssid.value();
        }
        
        if (testResult.value().error_message.has_value()) {
            data["error_message"] = testResult.value().error_message.value();
        }
        
        auto response = createSuccessResponse(transactionId, data, executionTime);
        rpc_client_->publishRawMessage(response_topic_, response.toJSON());
        operation_tracker_->completeOperation(transactionId, true);
    } catch (const std::exception& e) {
        std::cerr << "Worker thread exception: " << e.what() << std::endl;
        operation_tracker_->completeOperation(transactionId, false);
    }
    
    // Thread will be automatically cleaned up by ThreadManager when it completes
}

RPCAction TestConnectionHandler::getSupportedAction() const {
    return RPCAction::TestConnection;
}

} // namespace rpc
} // namespace urwt
