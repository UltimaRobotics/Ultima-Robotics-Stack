#ifndef VPN_INSTANCE_MANAGER_HPP
#define VPN_INSTANCE_MANAGER_HPP

#include <string>
#include <map>
#include <memory>
#include <mutex>
#include <atomic>
#include <functional>
#include <nlohmann/json.hpp>
#include "../ur-threadder-api/cpp/include/ThreadManager.hpp"
#include "vpn_cleanup.hpp"
#include "internal/vpn_manager_utils.hpp"
#include "vpn_routing_interface.hpp"
#include "cleanup_tracker.hpp"
#include "cleanup_verifier.hpp"
#include "cleanup_cron_job.hpp"

using json = nlohmann::json;

namespace vpn_manager {

enum class VPNType {
    OPENVPN,
    WIREGUARD,
    UNKNOWN
};

enum class ConnectionState {
    INITIAL,
    CONNECTING,
    AUTHENTICATING,
    CONNECTED,
    DISCONNECTED,
    ERROR_STATE,
    RECONNECTING
};

struct RoutingRule {
    std::string id;
    std::string name;
    std::string vpn_instance;
    std::string vpn_profile;
    std::string source_type;  // "IP Address", "Network", "Any"
    std::string source_value;
    std::string destination;  // Format: IP/CIDR (e.g., "10.0.0.0/24", "192.168.1.1/32")
    std::string gateway;      // "VPN Server" or specific gateway IP
    std::string protocol;     // "tcp", "udp", "both"
    std::string type;         // "tunnel_all", "tunnel_specific", "exclude"
    int priority;
    bool enabled;
    bool log_traffic;
    bool apply_to_existing;
    std::string description;
    std::string created_date;
    std::string last_modified;
    bool is_applied;  // Runtime state tracking
    bool is_automatic;  // True if auto-detected from route -n
    bool user_modified;  // True if user modified an automatic rule
};

struct VPNInstance {
    // Profile data (from config)
    std::string id;
    std::string name;
    VPNType type;
    std::string server;
    int port;
    std::string protocol;
    std::string encryption;
    std::string auth_method;
    std::string username;
    std::string password;
    std::string config_content;
    std::string created_date;
    json parsed_config;
    json connection_stats;

    // Runtime state (from cached-data.json)
    bool enabled;
    bool auto_connect; // Corrected from auto_reconnect
    std::string interface_name; // Network interface name (e.g., tun0, wg0)
    std::string status;
    std::string last_used;
    unsigned int thread_id;
    ConnectionState current_state;
    time_t start_time;
    std::shared_ptr<void> wrapper_instance;
    std::atomic<bool> should_stop;

    // Provider-specific routing
    std::unique_ptr<IVPNRoutingProvider> routing_provider;
    bool routing_initialized;

    // Connection metrics
    struct {
        uint64_t upload_bytes;   // Real-time upload in current session
        uint64_t download_bytes; // Real-time download in current session
    } data_transfer;

    struct {
        uint64_t current_session_bytes; // Bytes transferred in current session
        uint64_t total_bytes;           // Total bytes from profile creation
    } total_data_transferred;

    struct {
        time_t current_session_start;   // Current session start time
        uint64_t current_session_seconds; // Current session duration
        uint64_t total_seconds;         // Total connection time from creation
    } connection_time;

    // Default constructor needed for map insertion when default constructing elements.
    VPNInstance() :
        type(VPNType::UNKNOWN),
        port(0),
        enabled(false),
        auto_connect(false),
        interface_name(""),
        thread_id(0),
        current_state(ConnectionState::INITIAL),
        start_time(0),
        should_stop(false),
        routing_provider(nullptr),
        routing_initialized(false) {
        data_transfer.upload_bytes = 0;
        data_transfer.download_bytes = 0;
        total_data_transferred.current_session_bytes = 0;
        total_data_transferred.total_bytes = 0;
        connection_time.current_session_start = 0;
        connection_time.current_session_seconds = 0;
        connection_time.total_seconds = 0;
    }

    // Custom copy assignment operator to handle std::atomic
    VPNInstance& operator=(const VPNInstance& other) {
        if (this != &other) {
            id = other.id;
            name = other.name;
            type = other.type;
            server = other.server;
            port = other.port;
            protocol = other.protocol;
            encryption = other.encryption;
            auth_method = other.auth_method;
            username = other.username;
            password = other.password;
            config_content = other.config_content;
            created_date = other.created_date;
            parsed_config = other.parsed_config;
            connection_stats = other.connection_stats;

            enabled = other.enabled;
            auto_connect = other.auto_connect;
            interface_name = other.interface_name;
            status = other.status;
            last_used = other.last_used;
            thread_id = other.thread_id;
            current_state = other.current_state;
            start_time = other.start_time;
            wrapper_instance = other.wrapper_instance;
            should_stop.store(other.should_stop.load()); // Store the atomic value
        }
        return *this;
    }

    // Move constructor
    VPNInstance(VPNInstance&& other) noexcept
        : id(std::move(other.id)),
          name(std::move(other.name)),
          type(other.type),
          server(std::move(other.server)),
          port(other.port),
          protocol(std::move(other.protocol)),
          encryption(std::move(other.encryption)),
          auth_method(std::move(other.auth_method)),
          username(std::move(other.username)),
          password(std::move(other.password)),
          config_content(std::move(other.config_content)),
          created_date(std::move(other.created_date)),
          parsed_config(std::move(other.parsed_config)),
          connection_stats(std::move(other.connection_stats)),
          enabled(other.enabled),
          auto_connect(other.auto_connect),
          interface_name(std::move(other.interface_name)),
          status(std::move(other.status)),
          last_used(std::move(other.last_used)),
          thread_id(other.thread_id),
          current_state(other.current_state),
          start_time(other.start_time),
          wrapper_instance(std::move(other.wrapper_instance)),
          should_stop(other.should_stop.load()) // Load the atomic value
    {
        // Reset other's atomic to a default state if necessary
        other.should_stop.store(false);
        other.thread_id = 0;
        other.current_state = ConnectionState::INITIAL;
    }

    // Move assignment operator
    VPNInstance& operator=(VPNInstance&& other) noexcept {
        if (this != &other) {
            id = std::move(other.id);
            name = std::move(other.name);
            type = other.type;
            server = std::move(other.server);
            port = other.port;
            protocol = std::move(other.protocol);
            encryption = std::move(other.encryption);
            auth_method = std::move(other.auth_method);
            username = std::move(other.username);
            password = std::move(other.password);
            config_content = std::move(other.config_content);
            created_date = std::move(other.created_date);
            parsed_config = std::move(other.parsed_config);
            connection_stats = std::move(other.connection_stats);
            enabled = other.enabled;
            auto_connect = other.auto_connect;
            interface_name = std::move(other.interface_name);
            status = std::move(other.status);
            last_used = std::move(other.last_used);
            thread_id = other.thread_id;
            current_state = other.current_state;
            start_time = other.start_time;
            wrapper_instance = std::move(other.wrapper_instance);
            should_stop.store(other.should_stop.load()); // Store the atomic value

            // Reset other's atomic to a default state if necessary
            other.should_stop.store(false);
            other.thread_id = 0;
            other.current_state = ConnectionState::INITIAL;
        }
        return *this;
    }
};

struct AggregatedEvent {
    std::string instance_name;
    std::string event_type;
    std::string message;
    json data;
    time_t timestamp;
};

using EventCallback = std::function<void(const AggregatedEvent&)>;

class VPNInstanceManager {
public:
    VPNInstanceManager();
    ~VPNInstanceManager();

    // Configuration & Lifecycle
    bool loadConfiguration(const std::string& json_config);
    bool loadConfigurationFromFile(const std::string& config_file, const std::string& cache_file = "", const std::string& cleanup_config_file = "");
    bool saveConfiguration(const std::string& filepath);
    bool saveCachedData(const std::string& cache_file);
    bool saveOriginalConfigToCache(const std::string& cache_file, const std::string& instance_name, const std::string& original_config);
    std::string loadOriginalConfigFromCache(const std::string& cache_file, const std::string& instance_name);
    void initializeCleanupSystem();
    nlohmann::json purgeCleanup(bool confirm = false);

    // Instance Control
    bool startInstance(const std::string& instance_name);
    bool stopInstance(const std::string& instance_name);
    bool restartInstance(const std::string& instance_name);
    bool startAllEnabled();
    bool stopAll();
    bool enableInstance(const std::string& instance_name);
    bool disableInstance(const std::string& instance_name);
    
    // Instance Management
    bool addInstance(const std::string& name, const std::string& vpn_type, 
                     const std::string& config_content, bool auto_start = true);
    bool deleteInstance(const std::string& instance_name);
    bool updateInstance(const std::string& instance_name, const std::string& config_content, const std::string& protocol = "");
    bool setInstanceAutoRouting(const std::string& instance_name, bool enable_auto_routing);

    // Status & Monitoring
    json getInstanceStatus(const std::string& instance_name);
    json getAllInstancesStatus();
    json getAggregatedStats();

    // Event Handling
    void setGlobalEventCallback(EventCallback callback);
    
    // Verbose mode
    void setVerbose(bool verbose);
    bool isVerbose() const;
    
    // Stats logging control
    void setStatsLoggingEnabled(bool enabled);
    bool isStatsLoggingEnabled() const;
    void setOpenVPNStatsLogging(bool enabled);
    bool isOpenVPNStatsLoggingEnabled() const;
    void setWireGuardStatsLogging(bool enabled);
    bool isWireGuardStatsLoggingEnabled() const;

    // Routing Rules Management (Legacy)
    bool addRoutingRule(const RoutingRule& rule);
    bool updateRoutingRule(const std::string& rule_id, const RoutingRule& rule);
    bool deleteRoutingRule(const std::string& rule_id);
    json getRoutingRule(const std::string& rule_id);
    json getAllRoutingRules();
    bool loadRoutingRules(const std::string& filepath);
    bool saveRoutingRules(const std::string& filepath);
    
    // Per-Instance Routing Management (New)
    bool initializeRoutingForInstance(const std::string& instance_name);
    void handleRoutingEvent(const std::string& instance_name, 
                           RouteEventType event_type,
                           const UnifiedRouteRule& rule,
                           const std::string& error_msg);
    bool loadRoutingRulesForInstance(const std::string& instance_name);
    bool saveRoutingRulesForInstance(const std::string& instance_name);
    bool migrateRoutingRules();
    
    // Per-Instance Routing API Methods
    json getInstanceRoutes(const std::string& instance_name);
    bool addInstanceRoute(const std::string& instance_name, const UnifiedRouteRule& rule);
    bool deleteInstanceRoute(const std::string& instance_name, const std::string& rule_id);
    bool applyInstanceRoutes(const std::string& instance_name);
    int detectInstanceRoutes(const std::string& instance_name);
    
    // Cleanup tracking methods
    CleanupTracker* getCleanupTracker() { return cleanup_tracker_.get(); }
    CleanupVerifier* getCleanupVerifier() { return cleanup_verifier_.get(); }
    CleanupCronJob* getCleanupCronJob() { return cleanup_cron_job_.get(); }
    ThreadMgr::ThreadManager* getThreadManager() { return thread_manager_.get(); }

private:
    std::map<std::string, VPNInstance> instances_;
    std::unique_ptr<ThreadMgr::ThreadManager> thread_manager_;
    std::mutex instances_mutex_;
    EventCallback global_event_callback_;
    std::atomic<bool> running_;
    std::atomic<bool> verbose_;
    std::atomic<bool> stats_logging_enabled_;
    std::atomic<bool> openvpn_stats_logging_;
    std::atomic<bool> wireguard_stats_logging_;
    std::atomic<bool> config_save_pending_;
    
    // Cleanup tracking system
    std::unique_ptr<CleanupTracker> cleanup_tracker_;
    std::unique_ptr<CleanupVerifier> cleanup_verifier_;
    std::unique_ptr<CleanupCronJob> cleanup_cron_job_;
    std::thread config_save_thread_;
    std::thread route_monitor_thread_;
    std::string config_file_path_;
    std::string cache_file_path_;
    std::string routing_rules_file_path_;
    std::string cleanup_config_path_;
    std::string routing_config_dir_;
    std::map<std::string, RoutingRule> routing_rules_;
    std::mutex routing_mutex_;
    std::map<std::string, std::string> last_route_snapshots_;  // instance_name -> route output hash

    void launchInstanceThread(VPNInstance& instance);
    void emitEvent(const std::string& instance_name, const std::string& event_type,
                   const std::string& message, const json& data = json::object());
    VPNType parseVPNType(const std::string& type_str);
    std::string vpnTypeToString(VPNType type);
    std::string formatBytes(uint64_t bytes);
    std::string formatTime(uint64_t seconds);
    
    void forceCleanupNetworkInterface(const std::string& interface_name, VPNType vpn_type);
    bool disconnectWrapperWithTimeout(std::shared_ptr<void> wrapper_instance, VPNType vpn_type, 
                                      const std::string& instance_id, int timeout_seconds);
    bool stopThreadWithTimeout(unsigned int thread_id, const std::string& instance_id, int timeout_seconds);
    
    // Routing helper methods
    void applyRoutingRulesForInstance(const std::string& instance_name);
    void removeRoutingRulesForInstance(const std::string& instance_name);
    bool applyRoutingRule(const RoutingRule& rule, const std::string& interface_name);
    bool removeRoutingRule(const RoutingRule& rule, const std::string& interface_name);
    std::string getInterfaceForInstance(const std::string& instance_name);
    
    // Automatic route detection
    void detectAndSaveAutomaticRoutes(const std::string& instance_name, const std::string& interface_name);
    std::vector<RoutingRule> parseRouteOutput(const std::string& route_output, const std::string& instance_name);
    std::string executeCommand(const std::string& cmd);
    void mergeAutomaticRoutes(const std::vector<RoutingRule>& detected_rules, const std::string& instance_name);
    std::string getCidrFromNetmask(const std::string& netmask);
    
    // Route monitoring
    void startRouteMonitoring();
    void monitorRoutesForAllInstances();
    inline std::string hashString(const std::string& str) {
        return VPNManagerUtils::hashString(str);
    }
};

} // namespace vpn_manager

#endif // VPN_INSTANCE_MANAGER_HPP
