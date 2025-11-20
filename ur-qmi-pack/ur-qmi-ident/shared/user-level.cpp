
#include "user-level.h"
#include <json/json.h>
#include <iostream>
#include <sstream>
#include <iostream>
#include <sstream>

// Static member definition
std::string TargetedRequestParser::last_error_;

bool TargetedRequestParser::verifyTargetedRequestFormat(const std::string& json_string) {
    clearError();
    
    if (json_string.empty()) {
        setError("Input JSON string is empty");
        return false;
    }
    
    try {
        Json::Value root;
        Json::Reader reader;
        
        if (!reader.parse(json_string, root)) {
            setError("Invalid JSON format: " + reader.getFormattedErrorMessages());
            return false;
        }
        
        // Verify required top-level fields
        if (!root.isMember("type") || !root["type"].isString()) {
            setError("Missing or invalid 'type' field");
            return false;
        }
        
        if (root["type"].asString() != "targeted_request") {
            setError("Invalid type, expected 'targeted_request', got: " + root["type"].asString());
            return false;
        }
        
        // Check all required fields
        std::vector<std::string> required_string_fields = {
            "sender", "target", "method", "transaction_id", "response_topic"
        };
        
        for (const auto& field : required_string_fields) {
            if (!root.isMember(field) || !root[field].isString()) {
                setError("Missing or invalid '" + field + "' field");
                return false;
            }
        }
        
        // Check numeric fields
        if (!root.isMember("timestamp") || !root["timestamp"].isNumeric()) {
            setError("Missing or invalid 'timestamp' field");
            return false;
        }
        
        if (!root.isMember("request_number") || !root["request_number"].isNumeric()) {
            setError("Missing or invalid 'request_number' field");
            return false;
        }
        
        // Check params object
        if (!root.isMember("params") || !root["params"].isObject()) {
            setError("Missing or invalid 'params' object");
            return false;
        }
        
        const Json::Value& params = root["params"];
        if (!params.isMember("data") || !params["data"].isString()) {
            setError("Missing or invalid 'data' field in params");
            return false;
        }
        
        if (!params.isMember("priority") || !params["priority"].isString()) {
            setError("Missing or invalid 'priority' field in params");
            return false;
        }
        
        return true;
        
    } catch (const std::exception& e) {
        setError("Exception during parsing: " + std::string(e.what()));
        return false;
    }
}

TargetedRequestData TargetedRequestParser::parseTargetedRequest(const std::string& json_string) {
    TargetedRequestData result;
    clearError();
    
    if (!verifyTargetedRequestFormat(json_string)) {
        return result; // is_valid remains false
    }
    
    try {
        Json::Value root;
        Json::Reader reader;
        
        if (!reader.parse(json_string, root)) {
            setError("Failed to parse JSON");
            return result;
        }
        
        // Extract all fields
        result.type = root["type"].asString();
        result.sender = root["sender"].asString();
        result.target = root["target"].asString();
        result.method = root["method"].asString();
        result.transaction_id = root["transaction_id"].asString();
        result.response_topic = root["response_topic"].asString();
        result.timestamp = root["timestamp"].asInt64();
        result.request_number = root["request_number"].asInt();
        
        const Json::Value& params = root["params"];
        result.data_payload = params["data"].asString();
        result.priority = params["priority"].asString();
        
        result.is_valid = true;
        
    } catch (const std::exception& e) {
        setError("Exception during parsing: " + std::string(e.what()));
        result.is_valid = false;
    }
    
    return result;
}

std::string TargetedRequestParser::extractDataPayload(const std::string& json_string) {
    clearError();
    
    if (!verifyTargetedRequestFormat(json_string)) {
        return "";
    }
    
    try {
        Json::Value root;
        Json::Reader reader;
        
        if (!reader.parse(json_string, root)) {
            setError("Failed to parse JSON");
            return "";
        }
        
        return root["params"]["data"].asString();
        
    } catch (const std::exception& e) {
        setError("Exception during extraction: " + std::string(e.what()));
        return "";
    }
}

bool TargetedRequestParser::verifyQMIDeviceFormat(const std::string& json_string) {
    clearError();
    
    if (json_string.empty()) {
        setError("QMI device JSON string is empty");
        return false;
    }
    
    try {
        Json::Value root;
        Json::Reader reader;
        
        if (!reader.parse(json_string, root)) {
            setError("Invalid QMI device JSON format: " + reader.getFormattedErrorMessages());
            return false;
        }
        
        // Check required string fields
        std::vector<std::string> required_string_fields = {
            "action", "device_path", "firmware_version", "imei", 
            "manufacturer", "model"
        };
        
        for (const auto& field : required_string_fields) {
            if (!root.isMember(field) || !root[field].isString()) {
                setError("Missing or invalid '" + field + "' field in QMI device data");
                return false;
            }
        }
        
        // Check boolean field
        if (!root.isMember("is_available") || !root["is_available"].isBool()) {
            setError("Missing or invalid 'is_available' field");
            return false;
        }
        
        // Check sim-status object
        if (!root.isMember("sim-status") || !root["sim-status"].isObject()) {
            setError("Missing or invalid 'sim-status' object");
            return false;
        }
        
        const Json::Value& sim_status = root["sim-status"];
        std::vector<std::string> sim_string_fields = {
            "application_id", "application_state", "application_type",
            "card_state", "personalization_state", "pin1_state", "pin2_state",
            "primary_1x_status", "primary_gw_application", "primary_gw_slot",
            "secondary_1x_status", "secondary_gw_status", "upin_state"
        };
        
        for (const auto& field : sim_string_fields) {
            if (!sim_status.isMember(field) || !sim_status[field].isString()) {
                setError("Missing or invalid '" + field + "' field in sim-status");
                return false;
            }
        }
        
        // Check sim numeric fields
        std::vector<std::string> sim_numeric_fields = {
            "pin1_retries", "pin2_retries", "puk1_retries", "puk2_retries",
            "upin_retries", "upuk_retries"
        };
        
        for (const auto& field : sim_numeric_fields) {
            if (!sim_status.isMember(field) || !sim_status[field].isNumeric()) {
                setError("Missing or invalid '" + field + "' field in sim-status");
                return false;
            }
        }
        
        // Check boolean field in sim-status
        if (!sim_status.isMember("upin_replaces_pin1") || !sim_status["upin_replaces_pin1"].isBool()) {
            setError("Missing or invalid 'upin_replaces_pin1' field in sim-status");
            return false;
        }
        
        // Check supported_bands array
        if (!root.isMember("supported_bands") || !root["supported_bands"].isArray()) {
            setError("Missing or invalid 'supported_bands' array");
            return false;
        }
        
        return true;
        
    } catch (const std::exception& e) {
        setError("Exception during QMI device format verification: " + std::string(e.what()));
        return false;
    }
}

QMIDeviceData TargetedRequestParser::parseQMIDeviceData(const std::string& data_payload) {
    QMIDeviceData result;
    clearError();
    
    if (!verifyQMIDeviceFormat(data_payload)) {
        return result; // is_valid remains false
    }
    
    try {
        Json::Value root;
        Json::Reader reader;
        
        if (!reader.parse(data_payload, root)) {
            setError("Failed to parse QMI device JSON");
            return result;
        }
        
        // Extract basic device information
        result.action = root["action"].asString();
        result.device_path = root["device_path"].asString();
        result.firmware_version = root["firmware_version"].asString();
        result.imei = root["imei"].asString();
        result.is_available = root["is_available"].asBool();
        result.manufacturer = root["manufacturer"].asString();
        result.model = root["model"].asString();
        
        // Extract SIM status
        const Json::Value& sim_status = root["sim-status"];
        result.sim_status.application_id = sim_status["application_id"].asString();
        result.sim_status.application_state = sim_status["application_state"].asString();
        result.sim_status.application_type = sim_status["application_type"].asString();
        result.sim_status.card_state = sim_status["card_state"].asString();
        result.sim_status.personalization_state = sim_status["personalization_state"].asString();
        result.sim_status.pin1_retries = sim_status["pin1_retries"].asInt();
        result.sim_status.pin1_state = sim_status["pin1_state"].asString();
        result.sim_status.pin2_retries = sim_status["pin2_retries"].asInt();
        result.sim_status.pin2_state = sim_status["pin2_state"].asString();
        result.sim_status.primary_1x_status = sim_status["primary_1x_status"].asString();
        result.sim_status.primary_gw_application = sim_status["primary_gw_application"].asString();
        result.sim_status.primary_gw_slot = sim_status["primary_gw_slot"].asString();
        result.sim_status.puk1_retries = sim_status["puk1_retries"].asInt();
        result.sim_status.puk2_retries = sim_status["puk2_retries"].asInt();
        result.sim_status.secondary_1x_status = sim_status["secondary_1x_status"].asString();
        result.sim_status.secondary_gw_status = sim_status["secondary_gw_status"].asString();
        result.sim_status.upin_replaces_pin1 = sim_status["upin_replaces_pin1"].asBool();
        result.sim_status.upin_retries = sim_status["upin_retries"].asInt();
        result.sim_status.upin_state = sim_status["upin_state"].asString();
        result.sim_status.upuk_retries = sim_status["upuk_retries"].asInt();
        
        // Extract supported bands
        const Json::Value& bands = root["supported_bands"];
        for (const auto& band : bands) {
            if (band.isString()) {
                result.supported_bands.push_back(band.asString());
            }
        }
        
        result.is_valid = true;
        
    } catch (const std::exception& e) {
        setError("Exception during QMI device data parsing: " + std::string(e.what()));
        result.is_valid = false;
    }
    
    return result;
}

std::string TargetedRequestParser::getLastError() {
    return last_error_;
}

void TargetedRequestParser::setError(const std::string& error) {
    last_error_ = error;
}

void TargetedRequestParser::clearError() {
    last_error_.clear();
}
