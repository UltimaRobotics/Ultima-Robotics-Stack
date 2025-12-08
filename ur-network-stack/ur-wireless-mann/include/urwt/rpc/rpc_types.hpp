#ifndef URWT_RPC_TYPES_HPP
#define URWT_RPC_TYPES_HPP

#include <string>
#include <cstdint>
#include <chrono>
#include "urwt/utils/result.hpp"
#include <json.hpp>

namespace urwt {
namespace rpc {

using json = nlohmann::json;

struct RPCRequest {
    std::string jsonrpc{"2.0"};
    std::string transaction_id;
    std::string action;
    json params;
    json metadata;
    
    static Result<RPCRequest, std::string> fromJSON(const std::string& jsonStr);
    Result<bool, std::string> validate() const;
};

struct RPCResponse {
    std::string jsonrpc{"2.0"};
    std::string transaction_id;
    json result;
    json error;
    json metadata;
    
    bool isError() const { return !error.is_null(); }
    std::string toJSON() const;
};

struct OperationResult {
    std::string status;
    json data;
    int64_t execution_time_ms;
    
    json toJSON() const;
};

struct RPCError {
    int code;
    std::string message;
    json data;
    
    json toJSON() const;
};

struct OperationContext {
    std::string transaction_id;
    std::string action;
    std::string requester_id;
    std::chrono::steady_clock::time_point start_time;
    
    OperationContext(const std::string& txnId)
        : transaction_id(txnId)
        , start_time(std::chrono::steady_clock::now()) {}
    
    int64_t elapsedMs() const {
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            now - start_time).count();
    }
};

enum class RPCAction {
    ListInterfaces,
    ScanNetworks,
    GetInterfaceInfo,
    TestConnection,
    GetStatus,
    Shutdown,
    SaveNetwork,
    RemoveNetwork,
    ListSavedNetworks,
    SetMode,
    EnableWifi,
    DisableWifi,
    SetAutomation,
    GetWirelessConfig,
    UpdateWirelessConfig,
    GetSystemState,
    Unknown
};

RPCAction stringToAction(const std::string& actionStr);
std::string actionToString(RPCAction action);

namespace ErrorCodes {
    constexpr int PARSE_ERROR = -32700;
    constexpr int INVALID_REQUEST = -32600;
    constexpr int METHOD_NOT_FOUND = -32601;
    constexpr int INVALID_PARAMS = -32602;
    constexpr int INTERNAL_ERROR = -32603;
    constexpr int OPERATION_FAILED = -32000;
    constexpr int TIMEOUT = -32001;
    constexpr int RESOURCE_BUSY = -32002;
    constexpr int PERMISSION_DENIED = -32003;
}

RPCResponse createSuccessResponse(const std::string& transaction_id, const json& data, int64_t execution_time_ms);
RPCResponse createErrorResponse(const std::string& transaction_id, int code, const std::string& message, const json& data = json::object());

} // namespace rpc
} // namespace urwt

#endif // URWT_RPC_TYPES_HPP
