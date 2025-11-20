
#include "HTTPClient.hpp"
#include "Logger.hpp"
#include <curl/curl.h>
#include <sstream>
#include <json.hpp>

using json = nlohmann::json;

static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

HTTPClient::HTTPClient(const std::string& endpoint, int timeoutMs)
    : endpoint_(endpoint), timeoutMs_(timeoutMs) {}

bool HTTPClient::postDeviceInfo(const DeviceInfo& info) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        LOG_ERROR("Failed to initialize CURL");
        return false;
    }
    
    std::string jsonData = deviceInfoToJSON(info);
    std::string response;
    
    // Log the HTTP request details
    LOG_INFO("HTTP POST Request:");
    LOG_INFO("  Endpoint: " + endpoint_);
    LOG_INFO("  Timeout: " + std::to_string(timeoutMs_) + "ms");
    LOG_INFO("  Payload: " + jsonData);
    
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    
    curl_easy_setopt(curl, CURLOPT_URL, endpoint_.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonData.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, timeoutMs_);
    
    CURLcode res = curl_easy_perform(curl);
    
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    
    if (res != CURLE_OK) {
        LOG_ERROR("HTTP POST failed: " + std::string(curl_easy_strerror(res)));
        return false;
    }
    
    LOG_INFO("HTTP POST successful for device: " + info.devicePath);
    if (!response.empty()) {
        LOG_DEBUG("  Response: " + response);
    }
    return true;
}

std::string HTTPClient::deviceInfoToJSON(const DeviceInfo& info) {
    json j;
    j["devicePath"] = info.devicePath;
    j["state"] = static_cast<int>(info.state.load());
    j["baudrate"] = info.baudrate;
    j["sysid"] = info.sysid;
    j["compid"] = info.compid;
    j["mavlinkVersion"] = info.mavlinkVersion;
    j["timestamp"] = info.timestamp;
    
    json messages = json::array();
    for (const auto& msg : info.messages) {
        json msgObj;
        msgObj["msgid"] = msg.msgid;
        msgObj["name"] = msg.name;
        messages.push_back(msgObj);
    }
    j["messages"] = messages;
    
    return j.dump();
}
