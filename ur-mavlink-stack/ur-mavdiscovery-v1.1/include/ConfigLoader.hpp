
#pragma once
#include <string>
#include <vector>
#include <json.hpp>

using json = nlohmann::json;

struct HTTPConfig {
    std::string serverAddress;
    int serverPort;
    int timeoutMs;
};

struct HeartbeatConfig {
    bool enabled;
    int intervalSeconds;
    std::string topic;
    std::string payload;
};

struct RpcConfig {
    std::string clientId;
    std::string brokerHost;
    int brokerPort;
    int keepalive;
    int qos;
    bool autoReconnect;
    int reconnectDelayMin;
    int reconnectDelayMax;
    bool useTls;
    int connectTimeout;
    int messageTimeout;
    HeartbeatConfig heartbeat;
    std::vector<std::string> publishTopics;
    std::vector<std::string> subscribeTopics;
};

struct DeviceConfig {
    std::vector<std::string> devicePathFilters;
    std::vector<int> baudrates;
    int readTimeoutMs;
    int packetTimeoutMs;
    int maxPacketSize;
    bool enableHTTP;
    HTTPConfig httpConfig;
    std::string logFile;
    std::string logLevel;
    std::string runtimeDeviceFile;
    
    // RPC config fields for validation
    std::string brokerHost;
    int brokerPort;
};

class ConfigLoader {
public:
    ConfigLoader();
    bool loadFromFile(const std::string& filename);
    DeviceConfig getConfig() const;
    RpcConfig getRpcConfig() const;
    
private:
    DeviceConfig config_;
    RpcConfig rpcConfig_;
    void setDefaults();
    void setRpcDefaults();
    bool loadDeviceConfig(const json& jsonConfig);
    bool loadRpcConfig(const json& jsonConfig);
};
