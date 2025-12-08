#include "urwt/rpc/handlers/get_wireless_config_handler.hpp"
#include "urwt/rpc/rpc_service.hpp"
#include "urwt/config/wireless_config_types.hpp"

namespace urwt {
namespace rpc {

GetWirelessConfigHandler::GetWirelessConfigHandler(
    std::shared_ptr<config::WirelessConfigManager> configManager,
    std::shared_ptr<OperationTracker> operationTracker,
    RPCService* rpcService
) : config_manager_(configManager)
  , operation_tracker_(operationTracker)
  , rpc_service_(rpcService) {}

GetWirelessConfigHandler::~GetWirelessConfigHandler() {}

RPCResponse GetWirelessConfigHandler::handle(const RPCRequest& request) {
    auto startTime = std::chrono::steady_clock::now();
    
    auto wirelessConfig = config_manager_->getConfig();
    
    json data;
    data["enabled"] = wirelessConfig.enabled;
    data["mode"] = config::wirelessModeToString(wirelessConfig.mode);
    
    json automation;
    automation["enabled"] = wirelessConfig.automation.enabled;
    automation["auto_connect"] = wirelessConfig.automation.auto_connect;
    automation["auto_reconnect"] = wirelessConfig.automation.auto_reconnect;
    automation["reconnect_delay_seconds"] = wirelessConfig.automation.reconnect_delay_seconds;
    automation["connection_timeout_seconds"] = wirelessConfig.automation.connection_timeout_seconds;
    automation["scan_interval_seconds"] = wirelessConfig.automation.scan_interval_seconds;
    automation["max_connection_attempts"] = wirelessConfig.automation.max_connection_attempts;
    data["automation"] = automation;
    
    json staMode;
    staMode["interface"] = wirelessConfig.sta_mode.interface;
    staMode["dhcp_enabled"] = wirelessConfig.sta_mode.dhcp_enabled;
    staMode["power_save"] = wirelessConfig.sta_mode.power_save;
    staMode["regulatory_domain"] = wirelessConfig.sta_mode.regulatory_domain;
    data["sta_mode"] = staMode;
    
    json apMode;
    apMode["ssid"] = wirelessConfig.ap_mode.ssid;
    apMode["security"] = config::securityTypeToString(wirelessConfig.ap_mode.security);
    apMode["channel"] = wirelessConfig.ap_mode.channel;
    apMode["interface"] = wirelessConfig.ap_mode.interface;
    apMode["ip_address"] = wirelessConfig.ap_mode.ip_address;
    apMode["max_clients"] = wirelessConfig.ap_mode.max_clients;
    apMode["hidden"] = wirelessConfig.ap_mode.hidden;
    data["ap_mode"] = apMode;
    
    auto endTime = std::chrono::steady_clock::now();
    auto executionTime = std::chrono::duration_cast<std::chrono::milliseconds>(
        endTime - startTime).count();
    
    operation_tracker_->completeOperation(request.transaction_id, true);
    return createSuccessResponse(request.transaction_id, data, executionTime);
}

RPCAction GetWirelessConfigHandler::getSupportedAction() const {
    return RPCAction::GetWirelessConfig;
}

} // namespace rpc
} // namespace urwt
