/*
 * This file is part of the MAVLink Router project
 *
 * Copyright (C) 2016  Intel Corporation. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <set>
#include <atomic>
#include <thread>
#include <condition_variable>
#include <mutex>
#include <functional>
#include <unordered_map>
#include <chrono>

#include "mainloop.h"
#include "endpoint.h"

// Forward declarations for RPC client
namespace ur_rpc {
    class RpcClient;
}

// JSON library
#include <nlohmann/json.hpp>

namespace EndpointMonitoring {

/**
 * @brief Structure to hold endpoint occupancy status
 */
struct EndpointStatus {
    std::string name;
    std::string type;
    int fd;
    bool occupied;
    bool has_server;
    bool has_client;
    std::string last_activity;
    uint64_t server_connections;
    uint64_t client_connections;
    std::chrono::steady_clock::time_point last_check;
    
    // Enhanced connection-based tracking
    bool tcp_connection_accepted;      // TCP: Connection was accepted
    bool udp_messages_received;       // UDP: Messages from unknown sources
    uint32_t unknown_message_count;    // UDP: Count of unknown messages
    std::string connection_info;       // Connection details (TCP fd, UDP source)
    std::chrono::steady_clock::time_point first_connection_time;
    
    // Real-time connection state tracking
    enum class ConnectionState {
        Disconnected,   // No connections
        Connecting,     // Connection in progress
        Connected,      // Active connection
        Activity        // Active data transmission
    } connection_state;
    
    // Connection metrics
    std::chrono::steady_clock::time_point last_connection_time;
    std::chrono::steady_clock::time_point last_activity_time;
    uint32_t total_connections;        // Total connections over lifetime
    uint32_t active_connections;       // Currently active connections
    uint32_t connection_attempts;      // Connection attempts
    uint32_t failed_connections;       // Failed connections
    
    // Protocol-specific tracking
    struct TcpTracking {
        std::vector<int> client_fds;    // Active client file descriptors
        std::map<int, std::chrono::steady_clock::time_point> connection_times;
        std::string last_client_ip;
        uint16_t last_client_port;
    } tcp_tracking;
    
    struct UdpTracking {
        std::set<std::string> remote_endpoints; // Unique remote endpoints seen
        std::map<std::string, uint32_t> message_counts; // Messages per endpoint
        std::string last_remote_ip;
        uint16_t last_remote_port;
        uint32_t broadcast_messages;
        uint32_t multicast_messages;
    } udp_tracking;
    
    EndpointStatus() : fd(-1), occupied(false), has_server(false), has_client(false), 
                      server_connections(0), client_connections(0), 
                      tcp_connection_accepted(false), udp_messages_received(false),
                      unknown_message_count(0), connection_state(ConnectionState::Disconnected),
                      total_connections(0), active_connections(0), connection_attempts(0), 
                      failed_connections(0) {}
};

/**
 * @brief Configuration for the endpoint monitoring system
 */
struct MonitorConfig {
    uint32_t check_interval_ms = 1000;        // Monitoring check interval
    uint32_t activity_timeout_ms = 5000;      // Consider inactive after this timeout
    bool enable_detailed_logging = false;     // Enable verbose logging
    std::vector<std::string> monitored_types; // Which endpoint types to monitor
    
    // Connection-based tracking configuration
    bool enable_connection_tracking = true;   // Enable real-time connection tracking
    bool track_connection_metrics = true;     // Track detailed connection metrics
    bool track_protocol_details = true;       // Track protocol-specific details
    uint32_t connection_history_size = 100;   // Max connection events to keep
    uint32_t cleanup_interval_ms = 30000;     // Cleanup stale connections every 30s
    bool enable_occupancy_prediction = false; // Predict endpoint occupancy based on patterns
    
    // Real-time monitoring publishing configuration
    bool enable_realtime_publishing = true;   // Enable real-time JSON publishing
    std::string realtime_topic = "ur-shared-bus/ur-mavlink-stack/ur-mavrouter/notification"; // MQTT topic
    uint32_t publish_interval_ms = 2000;      // Publishing interval (2 seconds)
    bool publish_on_change = true;            // Publish immediately when status changes
    bool include_connection_details = true;   // Include detailed connection information
    bool include_metrics = true;              // Include connection metrics in JSON
};

/**
 * @brief Non-blocking endpoint monitoring thread
 * 
 * This class provides a monitoring mechanism that tracks TCP/UDP endpoint
 * occupancy status. An endpoint is considered "occupied" when it has both
 * MAVLink server and client connections active simultaneously.
 * 
 * The monitoring is designed to be non-blocking and runs independently
 * from the main router and extension threads.
 */
class EndpointMonitor {
public:
    /**
     * @brief Constructor
     * @param main_router Reference to the main router Mainloop instance
     * @param config Monitoring configuration
     */
    explicit EndpointMonitor(Mainloop& main_router, const MonitorConfig& config = MonitorConfig{});
    
    /**
     * @brief Destructor - ensures clean shutdown
     */
    ~EndpointMonitor();

    /**
     * @brief Start the monitoring thread
     * @return true if started successfully, false otherwise
     */
    bool start();

    /**
     * @brief Stop the monitoring thread gracefully
     * @return true if stopped successfully, false otherwise
     */
    bool stop();

    /**
     * @brief Check if monitoring is currently active
     * @return true if monitoring thread is running
     */
    bool is_running() const { return running_.load(); }

    /**
     * @brief Get current status of all monitored endpoints
     * @return Vector of endpoint status information
     */
    std::vector<EndpointStatus> get_endpoint_status() const;

    /**
     * @brief Get status of a specific endpoint
     * @param name Endpoint name to query
     * @return Endpoint status, empty if not found
     */
    EndpointStatus get_endpoint_status(const std::string& name) const;

    /**
     * @brief Get all endpoints that are currently occupied
     * @return Vector of names of occupied endpoints
     */
    std::vector<std::string> get_occupied_endpoints() const;

    /**
     * @brief Register a callback to be called when occupancy status changes
     * @param callback Function to call with endpoint name and new occupancy status
     */
    void register_occupancy_callback(std::function<void(const std::string&, bool)> callback);

    /**
     * @brief Update monitoring configuration
     * @param config New configuration to apply
     */
    void update_config(const MonitorConfig& config);

    /**
     * @brief Add an extension mainloop to monitor
     * @param name Extension name
     * @param mainloop Extension mainloop instance
     */
    void add_extension_mainloop(const std::string& name, Mainloop* mainloop);

    /**
     * @brief Remove an extension mainloop from monitoring
     * @param name Extension name
     */
    void remove_extension_mainloop(const std::string& name);

    // Enhanced connection-based tracking methods
    /**
     * @brief Notify monitor of TCP connection acceptance with enhanced tracking
     * @param endpoint_name Name of the TCP endpoint
     * @param client_fd Client file descriptor
     * @param client_ip Client IP address
     * @param client_port Client port
     */
    void notify_tcp_connection_accepted(const std::string& endpoint_name, int client_fd, 
                                        const std::string& client_ip, uint16_t client_port);

    /**
     * @brief Notify monitor of TCP connection closure
     * @param endpoint_name Name of the TCP endpoint
     * @param client_fd Client file descriptor that closed
     */
    void notify_tcp_connection_closed(const std::string& endpoint_name, int client_fd);

    /**
     * @brief Notify monitor of UDP messages from unknown sources with enhanced tracking
     * @param endpoint_name Name of the UDP endpoint
     * @param message_count Number of unknown messages
     * @param remote_ip Remote UDP endpoint IP
     * @param remote_port Remote UDP endpoint port
     * @param message_id MAVLink message ID
     */
    void notify_udp_unknown_messages(const std::string& endpoint_name, uint32_t message_count,
                                     const std::string& remote_ip, uint16_t remote_port, 
                                     uint32_t message_id);

    /**
     * @brief Notify monitor of general endpoint activity
     * @param endpoint_name Name of the endpoint
     * @param activity_type Type of activity (send/receive/connect/disconnect)
     * @param details Activity details
     */
    void notify_endpoint_activity(const std::string& endpoint_name, 
                                  const std::string& activity_type, 
                                  const std::string& details);

    /**
     * @brief Get connection metrics for a specific endpoint
     * @param endpoint_name Name of the endpoint
     * @return Connection metrics or empty if not found
     */
    std::map<std::string, uint32_t> get_connection_metrics(const std::string& endpoint_name) const;

    /**
     * @brief Get real-time connection state for all endpoints
     * @return Map of endpoint names to connection states
     */
    std::map<std::string, std::string> get_connection_states() const;

    /**
     * @brief Cleanup stale connection data
     */
    void cleanup_stale_connections();

    /**
     * @brief Check if endpoint has connection-based activity with enhanced logic
     * @param status Endpoint status to check
     * @return true if endpoint has connection activity
     */
    bool has_connection_activity(const EndpointStatus& status) const;

    /**
     * @brief Notify occupancy change
     * @param name Endpoint name
     * @param occupied New occupancy status
     */
    void notify_occupancy_change(const std::string& name, bool occupied);

    // Real-time monitoring publishing methods
    /**
     * @brief Set RPC client for publishing monitoring data
     * @param rpc_client Shared pointer to RPC client
     */
    void set_rpc_client(std::shared_ptr<ur_rpc::RpcClient> rpc_client);

    /**
     * @brief Publish real-time monitoring data to MQTT topic
     * @param force_publish Force publish even if no changes
     */
    void publish_monitoring_data(bool force_publish = false);

    /**
     * @brief Create JSON payload with endpoint status data
     * @return JSON object with monitoring data
     */
    nlohmann::json create_monitoring_json() const;

    /**
     * @brief Publish endpoint status change notification
     * @param endpoint_name Name of the endpoint that changed
     * @param old_status Previous endpoint status
     * @param new_status New endpoint status
     */
    void publish_status_change(const std::string& endpoint_name, 
                               const EndpointStatus& old_status, 
                               const EndpointStatus& new_status);

    // Utility functions
    std::string format_timestamp(const std::chrono::steady_clock::time_point& tp) const;
    bool should_monitor_endpoint_type(const std::string& type) const;

private:
    // Core monitoring thread function
    void monitor_thread_func();

    // Endpoint analysis functions
    void analyze_mainloop_endpoints(Mainloop* mainloop, const std::string& context);
    bool is_endpoint_occupied(const std::shared_ptr<Endpoint>& endpoint);
    bool has_mavlink_server(const std::shared_ptr<Endpoint>& endpoint);
    bool has_mavlink_client(const std::shared_ptr<Endpoint>& endpoint);
    
    // Status management
    void update_endpoint_status(const std::string& name, const EndpointStatus& status);

    // Member variables
    Mainloop& main_router_;                                    // Main router mainloop
    std::unordered_map<std::string, Mainloop*> extension_loops_; // Extension mainloops
    MonitorConfig config_;                                     // Current configuration
    
    // Thread management
    std::unique_ptr<std::thread> monitor_thread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> should_stop_{false};
    
    // Status tracking
    mutable std::mutex status_mutex_;
    std::unordered_map<std::string, EndpointStatus> endpoint_status_;
    
    // Callback management
    mutable std::mutex callback_mutex_;
    std::vector<std::function<void(const std::string&, bool)>> occupancy_callbacks_;
    
    // Real-time publishing
    std::shared_ptr<ur_rpc::RpcClient> rpc_client_;
    std::chrono::steady_clock::time_point last_publish_time_;
    std::unordered_map<std::string, EndpointStatus> last_published_status_;
    mutable std::mutex publish_mutex_;
    std::atomic<uint32_t> publish_sequence_{0};
    
    // Synchronization
    std::condition_variable stop_cv_;
    mutable std::mutex stop_mutex_;
};

/**
 * @brief Global endpoint monitor instance management
 */
class GlobalMonitor {
public:
    /**
     * @brief Get the global monitor instance
     * @return Reference to global monitor
     */
    static EndpointMonitor& get_instance();

    /**
     * @brief Initialize the global monitor
     * @param main_router Main router mainloop reference
     * @param config Monitor configuration
     */
    static void initialize(Mainloop& main_router, const MonitorConfig& config = MonitorConfig{});

    /**
     * @brief Cleanup the global monitor
     */
    static void cleanup();

    /**
     * @brief Check if global monitor instance exists
     * @return true if instance is initialized
     */
    static bool is_initialized();

    // Make mutex public for access from Mainloop
    static std::mutex instance_mutex_;

private:
    static std::unique_ptr<EndpointMonitor> instance_;
};

} // namespace EndpointMonitoring
