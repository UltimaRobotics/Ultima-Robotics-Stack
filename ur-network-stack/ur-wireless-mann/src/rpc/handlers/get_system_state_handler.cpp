#include "urwt/rpc/handlers/get_system_state_handler.hpp"
#include "urwt/rpc/rpc_service.hpp"
#include "urwt/config/wireless_config_types.hpp"

namespace urwt {
namespace rpc {

GetSystemStateHandler::GetSystemStateHandler(
    std::shared_ptr<state::SystemStateAnalyzer> stateAnalyzer,
    std::shared_ptr<OperationTracker> operationTracker,
    RPCService* rpcService
) : state_analyzer_(stateAnalyzer)
  , operation_tracker_(operationTracker)
  , rpc_service_(rpcService) {}

GetSystemStateHandler::~GetSystemStateHandler() {}

RPCResponse GetSystemStateHandler::handle(const RPCRequest& request) {
    auto startTime = std::chrono::steady_clock::now();
    
    auto stateResult = state_analyzer_->analyzeCurrentState();
    
    auto endTime = std::chrono::steady_clock::now();
    auto executionTime = std::chrono::duration_cast<std::chrono::milliseconds>(
        endTime - startTime).count();
    
    if (!stateResult.isOk()) {
        operation_tracker_->completeOperation(request.transaction_id, false);
        return createErrorResponse(
            request.transaction_id,
            ErrorCodes::OPERATION_FAILED,
            stateResult.error()
        );
    }
    
    auto sysState = stateResult.value();
    
    json data;
    data["interface_name"] = sysState.interface_name;
    data["hardware_state"] = static_cast<int>(sysState.hardware_state);
    data["interface_state"] = static_cast<int>(sysState.interface_state);
    data["connection_state"] = static_cast<int>(sysState.connection_state);
    data["current_mode"] = config::wirelessModeToString(sysState.current_mode);
    
    if (sysState.connection.has_value() && sysState.connection.value().isConnected()) {
        json conn;
        conn["ssid"] = sysState.connection.value().ssid;
        conn["bssid"] = sysState.connection.value().bssid;
        conn["ip_address"] = sysState.connection.value().ip_address;
        conn["signal_strength"] = sysState.connection.value().signal_strength;
        data["connection"] = conn;
    }
    
    if (sysState.ap_state.has_value() && sysState.ap_state.value().active) {
        json ap;
        ap["ssid"] = sysState.ap_state.value().ssid;
        ap["channel"] = sysState.ap_state.value().channel;
        ap["client_count"] = sysState.ap_state.value().connected_clients;
        data["ap_state"] = ap;
    }
    
    operation_tracker_->completeOperation(request.transaction_id, true);
    return createSuccessResponse(request.transaction_id, data, executionTime);
}

RPCAction GetSystemStateHandler::getSupportedAction() const {
    return RPCAction::GetSystemState;
}

} // namespace rpc
} // namespace urwt
