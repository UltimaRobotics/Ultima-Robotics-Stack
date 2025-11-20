
#include "HttpClient.hpp"
#include <curl/curl.h>
#include <sstream>
#include <iostream>

namespace HttpClient {

// Callback for curl to write response data
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

struct Client::Impl {
    CURL* curl;
    
    Impl() : curl(nullptr) {
        curl = curl_easy_init();
    }
    
    ~Impl() {
        if (curl) {
            curl_easy_cleanup(curl);
        }
    }
};

Client::Client(const HttpConfig& config) 
    : pImpl(std::make_unique<Impl>()), config_(config) {
}

Client::~Client() = default;

HttpResponse Client::sendRequest(const HttpRequest& request) {
    HttpResponse response;
    response.success = false;
    response.statusCode = 0;

    if (!pImpl->curl) {
        std::cerr << "[HttpClient] CURL not initialized" << std::endl;
        return response;
    }

    // Build URL
    std::string url = "http://" + config_.serverAddress + ":" + 
                      std::to_string(config_.serverPort) + request.endpoint;

    std::string responseBody;
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    curl_easy_setopt(pImpl->curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(pImpl->curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(pImpl->curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(pImpl->curl, CURLOPT_WRITEDATA, &responseBody);
    curl_easy_setopt(pImpl->curl, CURLOPT_TIMEOUT_MS, config_.timeoutMs);

    if (request.method == "POST") {
        curl_easy_setopt(pImpl->curl, CURLOPT_POST, 1L);
        curl_easy_setopt(pImpl->curl, CURLOPT_POSTFIELDS, request.body.c_str());
    } else if (request.method == "GET") {
        curl_easy_setopt(pImpl->curl, CURLOPT_HTTPGET, 1L);
    }

    // Only log for non-GET requests to reduce verbosity during passive polling
    if (request.method != "GET") {
        std::cout << "[HttpClient] Sending " << request.method << " " << url << std::endl;
        if (!request.body.empty()) {
            std::cout << "[HttpClient] Body: " << request.body << std::endl;
        }
    }

    CURLcode res = curl_easy_perform(pImpl->curl);

    curl_slist_free_all(headers);

    if (res != CURLE_OK) {
        // Only log connection errors for non-GET requests
        if (request.method != "GET") {
            std::cerr << "[HttpClient] Request failed: " << curl_easy_strerror(res) << std::endl;
        }
        return response;
    }

    long httpCode = 0;
    curl_easy_getinfo(pImpl->curl, CURLINFO_RESPONSE_CODE, &httpCode);

    response.statusCode = static_cast<int>(httpCode);
    response.body = responseBody;
    response.success = (httpCode >= 200 && httpCode < 300);

    // Only log successful responses for GET requests when they indicate a state change
    if (request.method != "GET" || (httpCode == 200)) {
        std::cout << "[HttpClient] Response: " << httpCode << std::endl;
        if (!responseBody.empty()) {
            std::cout << "[HttpClient] Response body: " << responseBody << std::endl;
        }
    }

    return response;
}

HttpResponse Client::get(const std::string& endpoint) {
    HttpRequest request;
    request.method = "GET";
    request.endpoint = endpoint;
    return sendRequest(request);
}

HttpResponse Client::post(const std::string& endpoint, const std::string& body) {
    HttpRequest request;
    request.method = "POST";
    request.endpoint = endpoint;
    request.body = body;
    return sendRequest(request);
}

HttpConfig parseHttpConfig(const nlohmann::json& config) {
    HttpConfig httpConfig;
    
    if (config.contains("httpConfig")) {
        auto httpCfg = config["httpConfig"];
        httpConfig.serverAddress = httpCfg.value("serverAddress", "127.0.0.1");
        httpConfig.serverPort = httpCfg.value("serverPort", 8080);
        httpConfig.timeoutMs = httpCfg.value("timeoutMs", 5000);
    } else {
        httpConfig.serverAddress = "127.0.0.1";
        httpConfig.serverPort = 8080;
        httpConfig.timeoutMs = 5000;
    }
    
    return httpConfig;
}

} // namespace HttpClient

