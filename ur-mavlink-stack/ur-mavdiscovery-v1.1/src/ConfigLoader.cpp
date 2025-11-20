
#include "ConfigLoader.hpp"
#include "Logger.hpp"
#include <fstream>

ConfigLoader::ConfigLoader() {
    setDefaults();
    setRpcDefaults();
}

bool ConfigLoader::loadFromFile(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        LOG_WARNING("Config file not found: " + filename + ", using defaults");
        return false;
    }
    
    try {
        json j;
        file >> j;
        
        // Determine config type based on content
        if (j.contains("devicePathFilters") || j.contains("baudrates")) {
            // Device configuration
            return loadDeviceConfig(j);
        } else if (j.contains("client_id") || j.contains("broker_host")) {
            // RPC configuration
            return loadRpcConfig(j);
        } else {
            LOG_ERROR("Unknown configuration file format: " + filename);
            return false;
        }
        
    } catch (const std::exception& e) {
        LOG_ERROR("Error parsing config file: " + std::string(e.what()));
        return false;
    }
}

bool ConfigLoader::loadDeviceConfig(const json& j) {
    if (j.contains("devicePathFilters")) {
        config_.devicePathFilters = j["devicePathFilters"].get<std::vector<std::string>>();
    }
    if (j.contains("baudrates")) {
        config_.baudrates = j["baudrates"].get<std::vector<int>>();
    }
    if (j.contains("readTimeoutMs")) {
        config_.readTimeoutMs = j["readTimeoutMs"].get<int>();
    }
    if (j.contains("packetTimeoutMs")) {
        config_.packetTimeoutMs = j["packetTimeoutMs"].get<int>();
    }
    if (j.contains("maxPacketSize")) {
        config_.maxPacketSize = j["maxPacketSize"].get<int>();
    }
    if (j.contains("enableHTTP")) {
        config_.enableHTTP = j["enableHTTP"].get<bool>();
    }
    if (j.contains("httpConfig")) {
        auto httpCfg = j["httpConfig"];
        if (httpCfg.contains("serverAddress")) {
            config_.httpConfig.serverAddress = httpCfg["serverAddress"].get<std::string>();
        }
        if (httpCfg.contains("serverPort")) {
            config_.httpConfig.serverPort = httpCfg["serverPort"].get<int>();
        }
        if (httpCfg.contains("timeoutMs")) {
            config_.httpConfig.timeoutMs = httpCfg["timeoutMs"].get<int>();
        }
    }
    if (j.contains("logFile")) {
        config_.logFile = j["logFile"].get<std::string>();
    }
    if (j.contains("logLevel")) {
        config_.logLevel = j["logLevel"].get<std::string>();
    }
    if (j.contains("runtimeDeviceFile")) {
        config_.runtimeDeviceFile = j["runtimeDeviceFile"].get<std::string>();
    }
    
    LOG_INFO("Device configuration loaded successfully");
    return true;
}

bool ConfigLoader::loadRpcConfig(const json& j) {
    if (j.contains("client_id")) {
        rpcConfig_.clientId = j["client_id"].get<std::string>();
    }
    if (j.contains("broker_host")) {
        rpcConfig_.brokerHost = j["broker_host"].get<std::string>();
        // Also set in device config for validation
        config_.brokerHost = rpcConfig_.brokerHost;
    }
    if (j.contains("broker_port")) {
        rpcConfig_.brokerPort = j["broker_port"].get<int>();
        // Also set in device config for validation
        config_.brokerPort = rpcConfig_.brokerPort;
    }
    if (j.contains("keepalive")) {
        rpcConfig_.keepalive = j["keepalive"].get<int>();
    }
    if (j.contains("qos")) {
        rpcConfig_.qos = j["qos"].get<int>();
    }
    if (j.contains("auto_reconnect")) {
        rpcConfig_.autoReconnect = j["auto_reconnect"].get<bool>();
    }
    if (j.contains("reconnect_delay_min")) {
        rpcConfig_.reconnectDelayMin = j["reconnect_delay_min"].get<int>();
    }
    if (j.contains("reconnect_delay_max")) {
        rpcConfig_.reconnectDelayMax = j["reconnect_delay_max"].get<int>();
    }
    if (j.contains("use_tls")) {
        rpcConfig_.useTls = j["use_tls"].get<bool>();
    }
    if (j.contains("connect_timeout")) {
        rpcConfig_.connectTimeout = j["connect_timeout"].get<int>();
    }
    if (j.contains("message_timeout")) {
        rpcConfig_.messageTimeout = j["message_timeout"].get<int>();
    }
    if (j.contains("heartbeat")) {
        auto heartbeat = j["heartbeat"];
        if (heartbeat.contains("enabled")) {
            rpcConfig_.heartbeat.enabled = heartbeat["enabled"].get<bool>();
        }
        if (heartbeat.contains("interval_seconds")) {
            rpcConfig_.heartbeat.intervalSeconds = heartbeat["interval_seconds"].get<int>();
        }
        if (heartbeat.contains("topic")) {
            rpcConfig_.heartbeat.topic = heartbeat["topic"].get<std::string>();
        }
        if (heartbeat.contains("payload")) {
            rpcConfig_.heartbeat.payload = heartbeat["payload"].get<std::string>();
        }
    }
    if (j.contains("json_added_pubs") && j["json_added_pubs"].contains("topics")) {
        rpcConfig_.publishTopics = j["json_added_pubs"]["topics"].get<std::vector<std::string>>();
    }
    if (j.contains("json_added_subs") && j["json_added_subs"].contains("topics")) {
        rpcConfig_.subscribeTopics = j["json_added_subs"]["topics"].get<std::vector<std::string>>();
    }
    
    LOG_INFO("RPC configuration loaded successfully");
    return true;
}

DeviceConfig ConfigLoader::getConfig() const {
    return config_;
}

RpcConfig ConfigLoader::getRpcConfig() const {
    return rpcConfig_;
}

void ConfigLoader::setDefaults() {
    config_.devicePathFilters = {"/dev/ttyUSB", "/dev/ttyACM", "/dev/ttyS"};
    config_.baudrates = {57600, 115200, 921600, 500000, 1500000, 9600, 19200, 38400};
    config_.readTimeoutMs = 100;
    config_.packetTimeoutMs = 1000;
    config_.maxPacketSize = 280;
    config_.enableHTTP = false;
    config_.httpConfig.serverAddress = "0.0.0.0";
    config_.httpConfig.serverPort = 8080;
    config_.httpConfig.timeoutMs = 5000;
    config_.logFile = "mavdiscovery.log";
    config_.logLevel = "INFO";
    config_.runtimeDeviceFile = "current-runtime-device.json";
    
    // Initialize RPC validation fields
    config_.brokerHost = "";
    config_.brokerPort = 0;
}

void ConfigLoader::setRpcDefaults() {
    rpcConfig_.clientId = "ur-mavdiscovery";
    rpcConfig_.brokerHost = "127.0.0.1";
    rpcConfig_.brokerPort = 1899;
    rpcConfig_.keepalive = 60;
    rpcConfig_.qos = 1;
    rpcConfig_.autoReconnect = true;
    rpcConfig_.reconnectDelayMin = 1;
    rpcConfig_.reconnectDelayMax = 60;
    rpcConfig_.useTls = false;
    rpcConfig_.connectTimeout = 10;
    rpcConfig_.messageTimeout = 30;
    rpcConfig_.heartbeat.enabled = true;
    rpcConfig_.heartbeat.intervalSeconds = 5;
    rpcConfig_.heartbeat.topic = "clients/ur-mavdiscovery/heartbeat";
    rpcConfig_.heartbeat.payload = "{\"client\":\"ur-mavdiscovery\",\"status\":\"alive\",\"service\":\"device_discovery\"}";
    rpcConfig_.publishTopics = {"direct_messaging/ur-mavdiscovery/responses"};
    rpcConfig_.subscribeTopics = {"direct_messaging/ur-mavdiscovery/requests"};
}
