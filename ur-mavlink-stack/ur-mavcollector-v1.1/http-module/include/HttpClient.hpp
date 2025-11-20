
#pragma once

#include <string>
#include <memory>
#include "../../nholmann/json.hpp"

namespace HttpClient {

struct HttpConfig {
    std::string serverAddress;
    int serverPort;
    int timeoutMs;
};

struct HttpRequest {
    std::string method;
    std::string endpoint;
    std::string body;
};

struct HttpResponse {
    int statusCode;
    std::string body;
    bool success;
};

class Client {
public:
    explicit Client(const HttpConfig& config);
    ~Client();

    // Send HTTP request
    HttpResponse sendRequest(const HttpRequest& request);
    
    // Helper methods
    HttpResponse get(const std::string& endpoint);
    HttpResponse post(const std::string& endpoint, const std::string& body);

private:
    struct Impl;
    std::unique_ptr<Impl> pImpl;
    HttpConfig config_;
};

// Parse HTTP config from JSON
HttpConfig parseHttpConfig(const nlohmann::json& config);

} // namespace HttpClient

