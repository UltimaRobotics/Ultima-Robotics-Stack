
#pragma once

#include <string>
#include <map>
#include <nlohmann/json.hpp>

namespace urlic {

enum class OperationType {
    VERIFY,
    UPDATE,
    GET_LICENSE_INFO,
    GET_LICENSE_PLAN,
    GET_LICENSE_DEFINITIONS,
    UPDATE_LICENSE_DEFINITIONS,
    INIT
};

struct OperationRequest {
    OperationType operation;
    std::map<std::string, std::string> parameters;

    // JSON serialization
    nlohmann::json to_json() const;
    static OperationRequest from_json(const nlohmann::json& j);
};

struct OperationResult {
    bool success;
    int exit_code;
    std::string message;
    std::map<std::string, std::string> data;

    // JSON serialization
    nlohmann::json to_json() const;
    static OperationResult from_json(const nlohmann::json& j);
};

struct LicenseInfo {
    std::string license_id;
    std::string user_name;
    std::string user_email;
    std::string product_name;
    std::string product_version;
    std::string device_hardware_id;
    std::string device_model;
    std::string device_mac;
    std::string issued_at;
    std::string valid_until;
    std::string license_tier;
    std::string license_type;
    std::string signature_algorithm;

    // JSON serialization
    nlohmann::json to_json() const;
    static LicenseInfo from_json(const nlohmann::json& j);
};

struct VerificationResult {
    bool valid;
    std::string error_message;
    LicenseInfo license_info;

    // JSON serialization
    nlohmann::json to_json() const;
    static VerificationResult from_json(const nlohmann::json& j);
};

struct LicensePlan {
    std::string license_type;
    std::string license_tier;
    std::string product;
    std::string version;
    std::string expiry;

    // JSON serialization
    nlohmann::json to_json() const;
    static LicensePlan from_json(const nlohmann::json& j);
};

// Utility functions
std::string operation_type_to_string(OperationType op);
OperationType string_to_operation_type(const std::string& str);

} // namespace urlic
