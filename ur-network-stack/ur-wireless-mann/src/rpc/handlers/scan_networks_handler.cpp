#include "urwt/rpc/handlers/scan_networks_handler.hpp"
#include "urwt/rpc/rpc_service.hpp"

namespace urwt {
namespace rpc {

ScanNetworksHandler::ScanNetworksHandler(
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

ScanNetworksHandler::~ScanNetworksHandler() {}

RPCResponse ScanNetworksHandler::handle(const RPCRequest& request) {
    if (!request.params.contains("interface") || !request.params["interface"].is_string()) {
        operation_tracker_->completeOperation(request.transaction_id, false);
        return createErrorResponse(
            request.transaction_id,
            ErrorCodes::INVALID_PARAMS,
            "Missing or invalid 'interface' parameter"
        );
    }
    
    std::string interfaceName = request.params["interface"].get<std::string>();
    std::string effectiveInterface = rpc_service_->getEffectiveInterface(interfaceName);
    std::string transactionId = request.transaction_id;
    
    try {
        auto threadId = thread_manager_->createThread([this, transactionId, effectiveInterface]() {
            performScanInThread(transactionId, effectiveInterface);
        });
        
        uint64_t workerNum = rpc_service_->getNextWorkerNumber();
        std::string attachmentId = "worker-scan-" + std::to_string(workerNum) + "-" + transactionId;
        thread_manager_->registerThread(threadId, attachmentId);
        
        json acceptedData;
        acceptedData["message"] = "Scan operation started";
        acceptedData["interface"] = interfaceName;
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

void ScanNetworksHandler::performScanInThread(const std::string& transactionId, const std::string& interfaceName) {
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
        
        auto scanResult = api_->scan(ifaceResult.value());
        
        auto endTime = std::chrono::steady_clock::now();
        auto executionTime = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime).count();
        
        if (!scanResult.isOk()) {
            auto response = createErrorResponse(
                transactionId,
                ErrorCodes::OPERATION_FAILED,
                scanResult.error()
            );
            rpc_client_->publishRawMessage(response_topic_, response.toJSON());
            operation_tracker_->completeOperation(transactionId, false);
            return;
        }
        
        json interfaceData;
        interfaceData["name"] = ifaceResult.value().name();
        interfaceData["mac_address"] = ifaceResult.value().mac().toString();
        
        json networksArray = json::array();
        for (const auto& network : scanResult.value().networks()) {
            json netObj;
            netObj["ssid"] = network.ssid();
            netObj["bssid"] = network.bssid().toString();
            netObj["frequency"] = network.frequency();
            netObj["channel"] = network.channel();
            netObj["signal_strength"] = network.signalStrength();
            netObj["security"] = network.security();
            networksArray.push_back(netObj);
        }
        
        json data;
        data["interface"] = interfaceData;
        data["networks"] = networksArray;
        data["count"] = scanResult.value().networks().size();
        data["scan_duration_ms"] = scanResult.value().duration().count();
        
        auto response = createSuccessResponse(transactionId, data, executionTime);
        rpc_client_->publishRawMessage(response_topic_, response.toJSON());
        operation_tracker_->completeOperation(transactionId, true);
    } catch (const std::exception& e) {
        std::cerr << "Worker thread exception: " << e.what() << std::endl;
        operation_tracker_->completeOperation(transactionId, false);
    }
    
    // Thread will be automatically cleaned up by ThreadManager when it completes
}

RPCAction ScanNetworksHandler::getSupportedAction() const {
    return RPCAction::ScanNetworks;
}

} // namespace rpc
} // namespace urwt
