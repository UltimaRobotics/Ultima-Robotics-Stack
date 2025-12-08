#include "urwt/rpc/handlers/get_status_handler.hpp"

namespace urwt {
namespace rpc {

GetStatusHandler::GetStatusHandler(
    std::shared_ptr<OperationTracker> opTracker,
    std::shared_ptr<ThreadMgr::ThreadManager> threadManager,
    DirectTemplate::ClientThread* rpcClient,
    const std::string& brokerAddress
) : op_tracker_(opTracker)
  , thread_manager_(threadManager)
  , rpc_client_(rpcClient)
  , broker_address_(brokerAddress)
  , start_time_(std::chrono::steady_clock::now()) {}

GetStatusHandler::~GetStatusHandler() {}

void GetStatusHandler::setStartTime(std::chrono::steady_clock::time_point startTime) {
    start_time_ = startTime;
}

RPCResponse GetStatusHandler::handle(const RPCRequest& request) {
    auto startTime = std::chrono::steady_clock::now();
    
    auto now = std::chrono::steady_clock::now();
    auto uptimeSeconds = std::chrono::duration_cast<std::chrono::seconds>(
        now - start_time_).count();
    
    json data;
    data["service_status"] = "running";
    data["uptime_seconds"] = uptimeSeconds;
    data["active_operations"] = op_tracker_->getActiveCount();
    data["total_operations_processed"] = op_tracker_->getTotalOperations();
    
    json statistics;
    statistics["successful_operations"] = op_tracker_->getSuccessfulOperations();
    statistics["failed_operations"] = op_tracker_->getFailedOperations();
    statistics["average_operation_time_ms"] = op_tracker_->getAverageExecutionTime();
    data["statistics"] = statistics;
    
    json threads;
    threads["rpc_client"] = "running";
    threads["active_workers"] = op_tracker_->getActiveCount();
    threads["total_workers_spawned"] = op_tracker_->getTotalOperations();
    data["threads"] = threads;
    
    json mqttConnection;
    mqttConnection["broker"] = broker_address_;
    mqttConnection["connected"] = rpc_client_->isConnected();
    mqttConnection["last_heartbeat"] = std::chrono::system_clock::now().time_since_epoch().count();
    data["mqtt_connection"] = mqttConnection;
    
    auto endTime = std::chrono::steady_clock::now();
    auto executionTime = std::chrono::duration_cast<std::chrono::milliseconds>(
        endTime - startTime).count();
    
    return createSuccessResponse(request.transaction_id, data, executionTime);
}

RPCAction GetStatusHandler::getSupportedAction() const {
    return RPCAction::GetStatus;
}

} // namespace rpc
} // namespace urwt
