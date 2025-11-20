
#include "operation_types.hpp"
#include <stdexcept>

namespace urlic {

// OperationRequest implementation
nlohmann::json OperationRequest::to_json() const {
    nlohmann::json j;
    j["operation"] = operation_type_to_string(operation);
    j["parameters"] = parameters;
    return j;
}

OperationRequest OperationRequest::from_json(const nlohmann::json& j) {
    OperationRequest req;
    req.operation = string_to_operation_type(j.value("operation", ""));
    
    if (j.contains("parameters") && j["parameters"].is_object()) {
        for (auto& [key, value] : j["parameters"].items()) {
            if (value.is_string()) {
                req.parameters[key] = value;
            } else {
                req.parameters[key] = value.dump();
            }
        }
    }
    
    return req;
}

// OperationResult implementation
nlohmann::json OperationResult::to_json() const {
    nlohmann::json j;
    j["success"] = success;
    j["exit_code"] = exit_code;
    j["message"] = message;
    j["data"] = data;
    return j;
}

OperationResult OperationResult::from_json(const nlohmann::json& j) {
    OperationResult result;
    result.success = j.value("success", false);
    result.exit_code = j.value("exit_code", 1);
    result.message = j.value("message", "");
    
    if (j.contains("data") && j["data"].is_object()) {
        for (auto& [key, value] : j["data"].items()) {
            if (value.is_string()) {
                result.data[key] = value;
            } else {
                result.data[key] = value.dump();
            }
        }
    }
    
    return result;
}

// LicenseInfo implementation
nlohmann::json LicenseInfo::to_json() const {
    nlohmann::json j;
    j["license_id"] = license_id;
    j["user_name"] = user_name;
    j["user_email"] = user_email;
    j["product_name"] = product_name;
    j["product_version"] = product_version;
    j["device_hardware_id"] = device_hardware_id;
    j["device_model"] = device_model;
    j["device_mac"] = device_mac;
    j["issued_at"] = issued_at;
    j["valid_until"] = valid_until;
    j["license_tier"] = license_tier;
    j["license_type"] = license_type;
    j["signature_algorithm"] = signature_algorithm;
    return j;
}

LicenseInfo LicenseInfo::from_json(const nlohmann::json& j) {
    LicenseInfo info;
    info.license_id = j.value("license_id", "");
    info.user_name = j.value("user_name", "");
    info.user_email = j.value("user_email", "");
    info.product_name = j.value("product_name", "");
    info.product_version = j.value("product_version", "");
    info.device_hardware_id = j.value("device_hardware_id", "");
    info.device_model = j.value("device_model", "");
    info.device_mac = j.value("device_mac", "");
    info.issued_at = j.value("issued_at", "");
    info.valid_until = j.value("valid_until", "");
    info.license_tier = j.value("license_tier", "");
    info.license_type = j.value("license_type", "");
    info.signature_algorithm = j.value("signature_algorithm", "");
    return info;
}

// VerificationResult implementation
nlohmann::json VerificationResult::to_json() const {
    nlohmann::json j;
    j["valid"] = valid;
    j["error_message"] = error_message;
    j["license_info"] = license_info.to_json();
    return j;
}

VerificationResult VerificationResult::from_json(const nlohmann::json& j) {
    VerificationResult result;
    result.valid = j.value("valid", false);
    result.error_message = j.value("error_message", "");
    if (j.contains("license_info")) {
        result.license_info = LicenseInfo::from_json(j["license_info"]);
    }
    return result;
}

// LicensePlan implementation
nlohmann::json LicensePlan::to_json() const {
    nlohmann::json j;
    j["license_type"] = license_type;
    j["license_tier"] = license_tier;
    j["product"] = product;
    j["version"] = version;
    j["expiry"] = expiry;
    return j;
}

LicensePlan LicensePlan::from_json(const nlohmann::json& j) {
    LicensePlan plan;
    plan.license_type = j.value("license_type", "");
    plan.license_tier = j.value("license_tier", "");
    plan.product = j.value("product", "");
    plan.version = j.value("version", "");
    plan.expiry = j.value("expiry", "");
    return plan;
}

// Utility functions
std::string operation_type_to_string(OperationType op) {
    switch (op) {
        case OperationType::VERIFY: return "verify";
        case OperationType::UPDATE: return "update";
        case OperationType::GET_LICENSE_INFO: return "get_license_info";
        case OperationType::GET_LICENSE_PLAN: return "get_license_plan";
        case OperationType::GET_LICENSE_DEFINITIONS: return "get_license_definitions";
        case OperationType::UPDATE_LICENSE_DEFINITIONS: return "update_license_definitions";
        case OperationType::INIT: return "init";
        default: throw std::runtime_error("Unknown operation type");
    }
}

OperationType string_to_operation_type(const std::string& str) {
    if (str == "verify") return OperationType::VERIFY;
    if (str == "update") return OperationType::UPDATE;
    if (str == "get_license_info") return OperationType::GET_LICENSE_INFO;
    if (str == "get_license_plan") return OperationType::GET_LICENSE_PLAN;
    if (str == "get_license_definitions") return OperationType::GET_LICENSE_DEFINITIONS;
    if (str == "update_license_definitions") return OperationType::UPDATE_LICENSE_DEFINITIONS;
    if (str == "init") return OperationType::INIT;
    throw std::runtime_error("Unknown operation type string: " + str);
}

} // namespace urlic
