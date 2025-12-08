#include "urwt/rpc/rpc_types.hpp"
#include <sstream>

namespace urwt {
namespace rpc {

Result<RPCRequest, std::string> RPCRequest::fromJSON(const std::string& jsonStr) {
    try {
        auto j = json::parse(jsonStr);
        
        RPCRequest req;
        
        if (j.contains("jsonrpc")) {
            req.jsonrpc = j["jsonrpc"].get<std::string>();
        }
        
        if (!j.contains("transaction_id") || !j["transaction_id"].is_string()) {
            return Result<RPCRequest, std::string>::error("Missing or invalid transaction_id");
        }
        req.transaction_id = j["transaction_id"].get<std::string>();
        
        if (!j.contains("action") || !j["action"].is_string()) {
            return Result<RPCRequest, std::string>::error("Missing or invalid action");
        }
        req.action = j["action"].get<std::string>();
        
        if (j.contains("params")) {
            req.params = j["params"];
        } else {
            req.params = json::object();
        }
        
        if (j.contains("metadata")) {
            req.metadata = j["metadata"];
        } else {
            req.metadata = json::object();
        }
        
        return Result<RPCRequest, std::string>::ok(req);
    }
    catch (const json::exception& e) {
        return Result<RPCRequest, std::string>::error(std::string("JSON parse error: ") + e.what());
    }
}

Result<bool, std::string> RPCRequest::validate() const {
    if (jsonrpc != "2.0") {
        return Result<bool, std::string>::error("Invalid jsonrpc version, must be 2.0");
    }
    
    if (transaction_id.empty()) {
        return Result<bool, std::string>::error("transaction_id cannot be empty");
    }
    
    if (action.empty()) {
        return Result<bool, std::string>::error("action cannot be empty");
    }
    
    return Result<bool, std::string>::ok(true);
}

std::string RPCResponse::toJSON() const {
    json j;
    j["jsonrpc"] = jsonrpc;
    j["transaction_id"] = transaction_id;
    
    if (!error.is_null()) {
        j["error"] = error;
    } else {
        j["result"] = result;
    }
    
    if (!metadata.is_null()) {
        j["metadata"] = metadata;
    }
    
    return j.dump();
}

json OperationResult::toJSON() const {
    json j;
    j["status"] = status;
    j["data"] = data;
    j["execution_time_ms"] = execution_time_ms;
    return j;
}

json RPCError::toJSON() const {
    json j;
    j["code"] = code;
    j["message"] = message;
    if (!data.is_null()) {
        j["data"] = data;
    }
    return j;
}

RPCAction stringToAction(const std::string& actionStr) {
    if (actionStr == "list_interfaces") return RPCAction::ListInterfaces;
    if (actionStr == "scan_networks") return RPCAction::ScanNetworks;
    if (actionStr == "get_interface_info") return RPCAction::GetInterfaceInfo;
    if (actionStr == "test_connection") return RPCAction::TestConnection;
    if (actionStr == "get_status") return RPCAction::GetStatus;
    if (actionStr == "shutdown") return RPCAction::Shutdown;
    if (actionStr == "save_network") return RPCAction::SaveNetwork;
    if (actionStr == "remove_network") return RPCAction::RemoveNetwork;
    if (actionStr == "list_saved_networks") return RPCAction::ListSavedNetworks;
    if (actionStr == "set_mode") return RPCAction::SetMode;
    if (actionStr == "enable_wifi") return RPCAction::EnableWifi;
    if (actionStr == "disable_wifi") return RPCAction::DisableWifi;
    if (actionStr == "set_automation") return RPCAction::SetAutomation;
    if (actionStr == "get_wireless_config") return RPCAction::GetWirelessConfig;
    if (actionStr == "update_wireless_config") return RPCAction::UpdateWirelessConfig;
    if (actionStr == "get_system_state") return RPCAction::GetSystemState;
    return RPCAction::Unknown;
}

std::string actionToString(RPCAction action) {
    switch (action) {
        case RPCAction::ListInterfaces: return "list_interfaces";
        case RPCAction::ScanNetworks: return "scan_networks";
        case RPCAction::GetInterfaceInfo: return "get_interface_info";
        case RPCAction::TestConnection: return "test_connection";
        case RPCAction::GetStatus: return "get_status";
        case RPCAction::Shutdown: return "shutdown";
        case RPCAction::SaveNetwork: return "save_network";
        case RPCAction::RemoveNetwork: return "remove_network";
        case RPCAction::ListSavedNetworks: return "list_saved_networks";
        case RPCAction::SetMode: return "set_mode";
        case RPCAction::EnableWifi: return "enable_wifi";
        case RPCAction::DisableWifi: return "disable_wifi";
        case RPCAction::SetAutomation: return "set_automation";
        case RPCAction::GetWirelessConfig: return "get_wireless_config";
        case RPCAction::UpdateWirelessConfig: return "update_wireless_config";
        case RPCAction::GetSystemState: return "get_system_state";
        case RPCAction::Unknown: return "unknown";
    }
    return "unknown";
}

RPCResponse createSuccessResponse(const std::string& transaction_id, const json& data, int64_t execution_time_ms) {
    RPCResponse response;
    response.transaction_id = transaction_id;
    
    OperationResult opResult;
    opResult.status = "success";
    opResult.data = data;
    opResult.execution_time_ms = execution_time_ms;
    
    response.result = opResult.toJSON();
    return response;
}

RPCResponse createErrorResponse(const std::string& transaction_id, int code, const std::string& message, const json& data) {
    RPCResponse response;
    response.transaction_id = transaction_id;
    
    RPCError error;
    error.code = code;
    error.message = message;
    error.data = data;
    
    response.error = error.toJSON();
    return response;
}

} // namespace rpc
} // namespace urwt
