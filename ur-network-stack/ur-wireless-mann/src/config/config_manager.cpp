#include "urwt/config/config_manager.hpp"
#include <fstream>
#include <sstream>

namespace urwt {
namespace config {

ConfigManager::ConfigManager() {}

ConfigManager::~ConfigManager() {}

Result<bool, std::string> ConfigManager::loadFromFile(const std::string& configPath) {
    std::ifstream file(configPath);
    if (!file.is_open()) {
        return Result<bool, std::string>::error("Failed to open config file: " + configPath);
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();

    try {
        json j = json::parse(content);
        return loadFromJSON(j);
    } catch (const json::exception& e) {
        return Result<bool, std::string>::error(std::string("JSON parse error: ") + e.what());
    }
}

Result<bool, std::string> ConfigManager::loadFromJSON(const json& configJson) {
    if (!configJson.contains("client_id") || !configJson["client_id"].is_string()) {
        return Result<bool, std::string>::error("Missing or invalid client_id in config");
    }
    
    config_.client_id = configJson["client_id"].get<std::string>();

    auto brokerResult = parseBrokerConfig(configJson);
    if (!brokerResult.isOk()) {
        return Result<bool, std::string>::error("Broker config error: " + brokerResult.error());
    }
    config_.broker = brokerResult.value();

    auto heartbeatResult = parseHeartbeatConfig(configJson);
    if (!heartbeatResult.isOk()) {
        return Result<bool, std::string>::error("Heartbeat config error: " + heartbeatResult.error());
    }
    config_.heartbeat = heartbeatResult.value();

    auto topicsResult = parseTopicConfig(configJson, config_.client_id);
    if (!topicsResult.isOk()) {
        return Result<bool, std::string>::error("Topics config error: " + topicsResult.error());
    }
    config_.topics = topicsResult.value();

    return Result<bool, std::string>::ok(true);
}

Result<BrokerConfig, std::string> ConfigManager::parseBrokerConfig(const json& j) {
    BrokerConfig broker;

    if (j.contains("broker_host") && j["broker_host"].is_string()) {
        broker.host = j["broker_host"].get<std::string>();
    }

    if (j.contains("broker_port") && j["broker_port"].is_number()) {
        broker.port = j["broker_port"].get<int>();
    }

    if (j.contains("username") && j["username"].is_string()) {
        broker.username = j["username"].get<std::string>();
    }

    if (j.contains("password") && j["password"].is_string()) {
        broker.password = j["password"].get<std::string>();
    }

    if (j.contains("keepalive") && j["keepalive"].is_number()) {
        broker.keepalive = j["keepalive"].get<int>();
    }

    if (j.contains("qos") && j["qos"].is_number()) {
        broker.qos = j["qos"].get<int>();
    }

    if (j.contains("use_tls") && j["use_tls"].is_boolean()) {
        broker.use_tls = j["use_tls"].get<bool>();
    }

    if (j.contains("ca_file") && j["ca_file"].is_string()) {
        broker.ca_file = j["ca_file"].get<std::string>();
    }

    if (j.contains("cert_file") && j["cert_file"].is_string()) {
        broker.cert_file = j["cert_file"].get<std::string>();
    }

    if (j.contains("key_file") && j["key_file"].is_string()) {
        broker.key_file = j["key_file"].get<std::string>();
    }

    if (j.contains("tls_insecure") && j["tls_insecure"].is_boolean()) {
        broker.tls_insecure = j["tls_insecure"].get<bool>();
    }

    if (j.contains("tls_version") && j["tls_version"].is_string()) {
        broker.tls_version = j["tls_version"].get<std::string>();
    }

    if (j.contains("clean_session") && j["clean_session"].is_boolean()) {
        broker.clean_session = j["clean_session"].get<bool>();
    }

    if (j.contains("auto_reconnect") && j["auto_reconnect"].is_boolean()) {
        broker.auto_reconnect = j["auto_reconnect"].get<bool>();
    }

    if (j.contains("max_reconnect_attempts") && j["max_reconnect_attempts"].is_number()) {
        broker.max_reconnect_attempts = j["max_reconnect_attempts"].get<int>();
    }

    if (j.contains("reconnect_delay_seconds") && j["reconnect_delay_seconds"].is_number()) {
        broker.reconnect_delay_seconds = j["reconnect_delay_seconds"].get<int>();
    }

    return Result<BrokerConfig, std::string>::ok(broker);
}

Result<HeartbeatConfig, std::string> ConfigManager::parseHeartbeatConfig(const json& j) {
    HeartbeatConfig heartbeat;

    if (j.contains("heartbeat") && j["heartbeat"].is_object()) {
        const auto& hb = j["heartbeat"];

        if (hb.contains("enabled") && hb["enabled"].is_boolean()) {
            heartbeat.enabled = hb["enabled"].get<bool>();
        }

        if (hb.contains("topic") && hb["topic"].is_string()) {
            heartbeat.topic = hb["topic"].get<std::string>();
        }

        if (hb.contains("interval_seconds") && hb["interval_seconds"].is_number()) {
            heartbeat.interval_seconds = hb["interval_seconds"].get<int>();
        }

        if (hb.contains("payload") && hb["payload"].is_string()) {
            heartbeat.payload = hb["payload"].get<std::string>();
        }
    }

    return Result<HeartbeatConfig, std::string>::ok(heartbeat);
}

Result<TopicConfig, std::string> ConfigManager::parseTopicConfig(const json& j, const std::string& clientId) {
    TopicConfig topics;

    if (j.contains("topics") && j["topics"].is_object()) {
        const auto& t = j["topics"];

        if (t.contains("request_topic") && t["request_topic"].is_string()) {
            topics.request_topic = t["request_topic"].get<std::string>();
        } else {
            topics.request_topic = "direct_messaging/" + clientId + "/requests";
        }

        if (t.contains("response_topic") && t["response_topic"].is_string()) {
            topics.response_topic = t["response_topic"].get<std::string>();
        } else {
            topics.response_topic = "direct_messaging/" + clientId + "/responses";
        }

        if (t.contains("heartbeat_topic") && t["heartbeat_topic"].is_string()) {
            topics.heartbeat_topic = t["heartbeat_topic"].get<std::string>();
        } else {
            topics.heartbeat_topic = "clients/" + clientId + "/heartbeat";
        }
    } else {
        topics.request_topic = "direct_messaging/" + clientId + "/requests";
        topics.response_topic = "direct_messaging/" + clientId + "/responses";
        topics.heartbeat_topic = "clients/" + clientId + "/heartbeat";
    }

    return Result<TopicConfig, std::string>::ok(topics);
}

} // namespace config
} // namespace urwt
