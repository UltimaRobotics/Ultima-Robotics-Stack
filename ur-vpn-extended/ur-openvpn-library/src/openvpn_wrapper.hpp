#ifndef OPENVPN_WRAPPER_HPP
#define OPENVPN_WRAPPER_HPP

#include "openvpn_c_bridge.h"
#include <atomic>
#include <chrono>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <string>
#include <thread>

using json = nlohmann::json;

namespace openvpn {

enum class ConnectionState {
  INITIAL = BRIDGE_STATE_INITIAL,
  CONNECTING = BRIDGE_STATE_CONNECTING,
  WAIT = BRIDGE_STATE_WAIT,
  AUTHENTICATING = BRIDGE_STATE_AUTH,
  GET_CONFIG = BRIDGE_STATE_GET_CONFIG,
  ASSIGN_IP = BRIDGE_STATE_ASSIGN_IP,
  ADD_ROUTES = BRIDGE_STATE_ADD_ROUTES,
  CONNECTED = BRIDGE_STATE_CONNECTED,
  RECONNECTING = BRIDGE_STATE_RECONNECTING,
  EXITING = BRIDGE_STATE_EXITING,
  DISCONNECTED = BRIDGE_STATE_DISCONNECTED,
  ERROR_STATE = BRIDGE_STATE_ERROR
};

struct VPNStats {
  uint64_t bytes_sent = 0;
  uint64_t bytes_received = 0;
  uint64_t tun_read_bytes = 0;
  uint64_t tun_write_bytes = 0;
  time_t connected_since = 0;
  uint32_t ping_ms = 0;
  std::string local_ip;
  std::string remote_ip;
  std::string server_ip;
  std::string interface_name;
  std::string routes;  // JSON array of route objects

  // Real-time bandwidth rates (bytes per second)
  uint64_t upload_rate_bps = 0;
  uint64_t download_rate_bps = 0;
};

struct VPNEvent {
  std::string type;
  std::string message;
  ConnectionState state;
  time_t timestamp;
  json data;
};

class OpenVPNWrapper {
public:
  using EventCallback = std::function<void(const VPNEvent &)>;
  using StatsCallback = std::function<void(const VPNStats &)>;

  OpenVPNWrapper();
  ~OpenVPNWrapper();

  bool initializeFromFile(const std::string &config_file);

  bool connect();
  bool disconnect();
  bool reconnect();

  ConnectionState getState() const;
  VPNStats getStats() const;
  bool isConnected() const;

  void setEventCallback(EventCallback callback);
  void setStatsCallback(StatsCallback callback);

  json getStatusJson() const;
  json getStatsJson() const;
  json getLastErrorJson() const;

  // Routing Management
  struct RouteRule {
      std::string id;
      std::string name;
      std::string type;
      std::string destination;
      std::string gateway;
      std::string source_type;
      std::string source_value;
      std::string protocol;
      uint32_t metric;
      bool enabled;
      bool is_automatic;
      std::string description;
      
      json to_json() const;
      static RouteRule from_json(const json& j);
  };
  
  using RouteEventCallback = std::function<void(
      const std::string& event_type,
      const RouteRule& rule,
      const std::string& error_msg
  )>;
  
  bool addRouteRule(const RouteRule& rule);
  bool removeRouteRule(const std::string& rule_id);
  std::vector<RouteRule> getRouteRules() const;
  RouteRule getRouteRule(const std::string& rule_id) const;
  bool applyPreConnectionRoutes();
  bool detectPostConnectionRoutes();
  void setRouteEventCallback(RouteEventCallback callback);
  
  // Route Control System Methods
  bool setRouteControlMode(bool preventDefaultRoutes, bool selectiveRouting);
  bool setPreventDefaultRoutes(bool prevent);
  bool setSelectiveRouting(bool selective);
  bool addCustomRouteRule(const RouteRule& rule);
  std::string getRouteStatistics() const;

private:
  openvpn_bridge_ctx_t *bridge_ctx_;
  std::atomic<ConnectionState> state_;
  std::atomic<bool> running_;
  std::atomic<bool> connected_;

  mutable std::mutex stats_mutex_;
  VPNStats current_stats_;

  std::string config_file_;
  std::string last_error_;

  EventCallback event_callback_;
  StatsCallback stats_callback_;

  std::unique_ptr<std::thread> worker_thread_;
  std::unique_ptr<std::thread> stats_thread_;

  openvpn_routing_ctx_t routing_ctx_;
  RouteEventCallback route_event_callback_;

  void workerLoop();
  void statsLoop();
  void updateStats();
  void emitEvent(const std::string &type, const std::string &message,
                 const json &data = json::object());
  void setState(ConnectionState new_state);
  std::string stateToString(ConnectionState state) const;
  
  // Getters
  ConnectionState getConnectionState() const;

  static void route_callback_wrapper(
      const char* event_type,
      const char* rule_json,
      const char* error_msg,
      void* user_data
  );
};

} // namespace openvpn

#endif // OPENVPN_WRAPPER_HPP