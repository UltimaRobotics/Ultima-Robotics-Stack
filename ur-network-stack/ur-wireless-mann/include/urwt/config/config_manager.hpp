#ifndef URWT_CONFIG_MANAGER_HPP
#define URWT_CONFIG_MANAGER_HPP

#include <string>
#include <memory>
#include <json.hpp>
#include "urwt/utils/result.hpp"

namespace urwt {
namespace config {

using json = nlohmann::json;

struct BrokerConfig {
    std::string host{"localhost"};
    int port{1883};
    std::string username;
    std::string password;
    int keepalive{60};
    int qos{1};
    bool use_tls{false};
    std::string ca_file;
    std::string cert_file;
    std::string key_file;
    bool tls_insecure{false};
    std::string tls_version;
    bool clean_session{true};
    bool auto_reconnect{true};
    int max_reconnect_attempts{10};
    int reconnect_delay_seconds{5};
};

struct HeartbeatConfig {
    bool enabled{true};
    std::string topic;
    int interval_seconds{30};
    std::string payload;
};

struct TopicConfig {
    std::string request_topic;
    std::string response_topic;
    std::string heartbeat_topic;
};

struct RPCConfig {
    std::string client_id;
    BrokerConfig broker;
    HeartbeatConfig heartbeat;
    TopicConfig topics;
};

class ConfigManager {
public:
    ConfigManager();
    ~ConfigManager();

    Result<bool, std::string> loadFromFile(const std::string& configPath);
    Result<bool, std::string> loadFromJSON(const json& configJson);

    const RPCConfig& getConfig() const { return config_; }
    const BrokerConfig& getBrokerConfig() const { return config_.broker; }
    const HeartbeatConfig& getHeartbeatConfig() const { return config_.heartbeat; }
    const TopicConfig& getTopicConfig() const { return config_.topics; }
    const std::string& getClientId() const { return config_.client_id; }

private:
    RPCConfig config_;
    
    Result<BrokerConfig, std::string> parseBrokerConfig(const json& j);
    Result<HeartbeatConfig, std::string> parseHeartbeatConfig(const json& j);
    Result<TopicConfig, std::string> parseTopicConfig(const json& j, const std::string& clientId);
};

} // namespace config
} // namespace urwt

#endif // URWT_CONFIG_MANAGER_HPP
