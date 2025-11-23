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

#include "endpoint_monitor.h"
#include "common/log.h"
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <nlohmann/json.hpp>

namespace EndpointMonitoring {

// Static member definitions
std::unique_ptr<EndpointMonitor> GlobalMonitor::instance_;
std::mutex GlobalMonitor::instance_mutex_;

EndpointMonitor::EndpointMonitor(Mainloop& main_router, const MonitorConfig& config)
    : main_router_(main_router)
    , config_(config)
    , last_publish_time_(std::chrono::steady_clock::now())
    , publish_sequence_(0)
{
    log_info("EndpointMonitor initialized with check interval %d ms", config_.check_interval_ms);
    
    if (config_.enable_realtime_publishing) {
        log_info("Real-time monitoring publishing enabled on topic: %s", config_.realtime_topic.c_str());
    }
    
    // Default to monitoring TCP and UDP endpoints
    if (config_.monitored_types.empty()) {
        config_.monitored_types = {"TCP", "UDP"};
    }
}

EndpointMonitor::~EndpointMonitor()
{
    if (is_running()) {
        stop();
    }
    log_info("EndpointMonitor destroyed");
}

bool EndpointMonitor::start()
{
    if (running_.load()) {
        log_warning("EndpointMonitor is already running");
        return false;
    }

    should_stop_.store(false);
    monitor_thread_ = std::make_unique<std::thread>(&EndpointMonitor::monitor_thread_func, this);
    
    // Wait for thread to start
    std::unique_lock<std::mutex> lock(stop_mutex_);
    if (stop_cv_.wait_for(lock, std::chrono::seconds(2), [this] { return running_.load(); })) {
        log_info("EndpointMonitor started successfully");
        return true;
    } else {
        log_error("EndpointMonitor failed to start within timeout");
        monitor_thread_->join();
        monitor_thread_.reset();
        return false;
    }
}

bool EndpointMonitor::stop()
{
    if (!running_.load()) {
        log_warning("EndpointMonitor is not running");
        return false;
    }

    log_info("Stopping EndpointMonitor...");
    should_stop_.store(true);
    
    // Notify thread to stop
    {
        std::lock_guard<std::mutex> lock(stop_mutex_);
        stop_cv_.notify_all();
    }

    // Wait for thread to finish
    if (monitor_thread_ && monitor_thread_->joinable()) {
        monitor_thread_->join();
    }
    monitor_thread_.reset();
    
    running_.store(false);
    log_info("EndpointMonitor stopped successfully");
    return true;
}

std::vector<EndpointStatus> EndpointMonitor::get_endpoint_status() const
{
    std::lock_guard<std::mutex> lock(status_mutex_);
    std::vector<EndpointStatus> result;
    
    for (const auto& pair : endpoint_status_) {
        result.push_back(pair.second);
    }
    
    return result;
}

EndpointStatus EndpointMonitor::get_endpoint_status(const std::string& name) const
{
    std::lock_guard<std::mutex> lock(status_mutex_);
    auto it = endpoint_status_.find(name);
    if (it != endpoint_status_.end()) {
        return it->second;
    }
    return EndpointStatus{};
}

std::vector<std::string> EndpointMonitor::get_occupied_endpoints() const
{
    std::lock_guard<std::mutex> lock(status_mutex_);
    std::vector<std::string> result;
    
    for (const auto& pair : endpoint_status_) {
        if (pair.second.occupied) {
            result.push_back(pair.first);
        }
    }
    
    return result;
}

void EndpointMonitor::register_occupancy_callback(std::function<void(const std::string&, bool)> callback)
{
    std::lock_guard<std::mutex> lock(callback_mutex_);
    occupancy_callbacks_.push_back(callback);
}

void EndpointMonitor::update_config(const MonitorConfig& config)
{
    config_ = config;
    log_info("EndpointMonitor configuration updated");
}

void EndpointMonitor::add_extension_mainloop(const std::string& name, Mainloop* mainloop)
{
    if (!mainloop) {
        log_warning("Attempted to add null mainloop for extension '%s'", name.c_str());
        return;
    }
    
    std::lock_guard<std::mutex> lock(status_mutex_);
    extension_loops_[name] = mainloop;
    log_info("Added extension mainloop '%s' to monitoring", name.c_str());
}

void EndpointMonitor::remove_extension_mainloop(const std::string& name)
{
    std::lock_guard<std::mutex> lock(status_mutex_);
    auto it = extension_loops_.find(name);
    if (it != extension_loops_.end()) {
        extension_loops_.erase(it);
        log_info("Removed extension mainloop '%s' from monitoring", name.c_str());
    }
}

void EndpointMonitor::monitor_thread_func()
{
    log_info("EndpointMonitor thread started");
    running_.store(true);
    
    // Notify that we've started
    {
        std::lock_guard<std::mutex> lock(stop_mutex_);
        stop_cv_.notify_all();
    }

    while (!should_stop_.load()) {
        try {
            // Monitor main router endpoints
            analyze_mainloop_endpoints(&main_router_, "main_router");
            
            // Monitor extension endpoints
            std::unordered_map<std::string, Mainloop*> extensions;
            {
                std::lock_guard<std::mutex> lock(status_mutex_);
                extensions = extension_loops_;
            }
            
            for (const auto& pair : extensions) {
                analyze_mainloop_endpoints(pair.second, "extension_" + pair.first);
            }
            
            // Publish real-time monitoring data
            if (config_.enable_realtime_publishing) {
                publish_monitoring_data();
            }
            
            // Cleanup stale connections periodically
            if (config_.enable_connection_tracking) {
                static auto last_cleanup = std::chrono::steady_clock::now();
                auto now = std::chrono::steady_clock::now();
                if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_cleanup).count() 
                    >= static_cast<int64_t>(config_.cleanup_interval_ms)) {
                    cleanup_stale_connections();
                    last_cleanup = now;
                }
            }
            
            // Sleep for configured interval
            std::unique_lock<std::mutex> lock(stop_mutex_);
            stop_cv_.wait_for(lock, std::chrono::milliseconds(config_.check_interval_ms),
                           [this] { return should_stop_.load(); });
                           
        } catch (const std::exception& e) {
            log_error("Exception in EndpointMonitor thread: %s", e.what());
        } catch (...) {
            log_error("Unknown exception in EndpointMonitor thread");
        }
    }

    log_info("EndpointMonitor thread exiting");
    running_.store(false);
}

void EndpointMonitor::analyze_mainloop_endpoints(Mainloop* mainloop, const std::string& context)
{
    if (!mainloop) {
        return;
    }

    try {
        // Get endpoints from the mainloop
        const auto& endpoints = mainloop->g_endpoints;
        
        if (config_.enable_detailed_logging) {
            log_debug("Analyzing %zu endpoints in %s", endpoints.size(), context.c_str());
        }

        for (const auto& endpoint : endpoints) {
            if (!endpoint) {
                continue;
            }

            const std::string& endpoint_name = endpoint->get_name();
            const std::string& endpoint_type = endpoint->get_type();
            
            // Check if we should monitor this endpoint type
            if (!should_monitor_endpoint_type(endpoint_type)) {
                continue;
            }

            // Create current status
            EndpointStatus current_status;
            current_status.name = endpoint_name;
            current_status.type = endpoint_type;
            current_status.fd = endpoint->fd;
            current_status.last_check = std::chrono::steady_clock::now();
            
            // Analyze occupancy
            current_status.occupied = is_endpoint_occupied(endpoint);
            current_status.has_server = has_mavlink_server(endpoint);
            current_status.has_client = has_mavlink_client(endpoint);
            
            // Get previous status
            EndpointStatus previous_status;
            {
                std::lock_guard<std::mutex> lock(status_mutex_);
                auto it = endpoint_status_.find(endpoint_name);
                if (it != endpoint_status_.end()) {
                    previous_status = it->second;
                    
                    // Update connection counts
                    current_status.server_connections = previous_status.server_connections;
                    current_status.client_connections = previous_status.client_connections;
                    
                    if (current_status.has_server && !previous_status.has_server) {
                        current_status.server_connections++;
                    }
                    if (current_status.has_client && !previous_status.has_client) {
                        current_status.client_connections++;
                    }
                }
            }
            
            current_status.last_activity = format_timestamp(current_status.last_check);
            
            // Check for occupancy changes
            if (previous_status.occupied != current_status.occupied) {
                notify_occupancy_change(endpoint_name, current_status.occupied);
                
                if (config_.enable_detailed_logging) {
                    log_info("Endpoint '%s' occupancy changed: %s -> %s (server: %s, client: %s)",
                            endpoint_name.c_str(),
                            previous_status.occupied ? "occupied" : "free",
                            current_status.occupied ? "occupied" : "free",
                            current_status.has_server ? "yes" : "no",
                            current_status.has_client ? "yes" : "no");
                }
            }
            
            // Update status
            update_endpoint_status(endpoint_name, current_status);
        }
        
    } catch (const std::exception& e) {
        log_error("Error analyzing endpoints in %s: %s", context.c_str(), e.what());
    }
}

bool EndpointMonitor::is_endpoint_occupied(const std::shared_ptr<Endpoint>& endpoint)
{
    if (!endpoint || endpoint->fd < 0) {
        return false;
    }
    
    // An endpoint is occupied when it has both server and client connections
    return has_mavlink_server(endpoint) && has_mavlink_client(endpoint);
}

bool EndpointMonitor::has_mavlink_server(const std::shared_ptr<Endpoint>& endpoint)
{
    if (!endpoint || endpoint->fd < 0) {
        return false;
    }
    
    const std::string& type = endpoint->get_type();
    
    // TCP endpoints in server mode (listening socket)
    if (type == ENDPOINT_TYPE_TCP) {
        // Check if this is a listening TCP server socket
        // TCP server sockets typically have no sys_comp_ids (they're just listeners)
        // but they accept client connections
        return endpoint->fd >= 0 && !endpoint->has_any_sys_comp_id();
    }
    
    // UDP endpoints in server mode
    if (type == ENDPOINT_TYPE_UDP) {
        // UDP server endpoints are typically bound and listening
        // We can identify them by checking if they have a valid socket and are configured as server
        return endpoint->fd >= 0;
    }
    
    return false;
}

bool EndpointMonitor::has_mavlink_client(const std::shared_ptr<Endpoint>& endpoint)
{
    if (!endpoint || endpoint->fd < 0) {
        return false;
    }
    
    const std::string& type = endpoint->get_type();
    
    // TCP client endpoints have established connections and MAVLink communication
    if (type == ENDPOINT_TYPE_TCP) {
        // TCP clients have sys_comp_ids from MAVLink communication
        // Use the public method to check if endpoint has any system/component IDs
        return endpoint->fd >= 0 && endpoint->has_any_sys_comp_id();
    }
    
    // UDP client endpoints
    if (type == ENDPOINT_TYPE_UDP) {
        // UDP clients also have sys_comp_ids from active communication
        // Use the public method to check if endpoint has any system/component IDs
        return endpoint->fd >= 0 && endpoint->has_any_sys_comp_id();
    }
    
    return false;
}

void EndpointMonitor::update_endpoint_status(const std::string& name, const EndpointStatus& status)
{
    std::lock_guard<std::mutex> lock(status_mutex_);
    endpoint_status_[name] = status;
}

void EndpointMonitor::notify_occupancy_change(const std::string& name, bool occupied)
{
    // Get old status for change notification
    EndpointStatus old_status;
    EndpointStatus new_status;
    bool status_found = false;
    
    {
        std::lock_guard<std::mutex> lock(status_mutex_);
        auto it = endpoint_status_.find(name);
        if (it != endpoint_status_.end()) {
            old_status = last_published_status_[name]; // Get previously published status
            new_status = it->second; // Get current status
            status_found = true;
        }
    }
    
    // Call registered callbacks
    {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        for (auto& callback : occupancy_callbacks_) {
            try {
                callback(name, occupied);
            } catch (const std::exception& e) {
                log_error("Occupancy callback error for endpoint '%s': %s", name.c_str(), e.what());
            } catch (...) {
                log_error("Unknown occupancy callback error for endpoint '%s'", name.c_str());
            }
        }
    }
    
    // Publish status change notification
    if (status_found && config_.enable_realtime_publishing && config_.publish_on_change) {
        // Check if there's actually a change worth publishing
        bool has_change = (old_status.occupied != new_status.occupied) ||
                         (old_status.connection_state != new_status.connection_state) ||
                         (old_status.active_connections != new_status.active_connections);
        
        if (has_change) {
            publish_status_change(name, old_status, new_status);
        }
    }
}

std::string EndpointMonitor::format_timestamp(const std::chrono::steady_clock::time_point& tp) const
{
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    ss << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return ss.str();
}

bool EndpointMonitor::should_monitor_endpoint_type(const std::string& type) const
{
    return std::find(config_.monitored_types.begin(), config_.monitored_types.end(), type) 
           != config_.monitored_types.end();
}

// Enhanced connection-based tracking implementations
void EndpointMonitor::notify_tcp_connection_accepted(const std::string& endpoint_name, int client_fd, 
                                                      const std::string& client_ip, uint16_t client_port)
{
    std::lock_guard<std::mutex> lock(status_mutex_);
    
    auto it = endpoint_status_.find(endpoint_name);
    if (it != endpoint_status_.end()) {
        auto& status = it->second;
        auto now = std::chrono::steady_clock::now();
        
        // Update connection state and metrics
        status.tcp_connection_accepted = true;
        status.connection_state = EndpointStatus::ConnectionState::Connected;
        status.last_connection_time = now;
        status.last_activity_time = now;
        status.total_connections++;
        status.active_connections++;
        status.connection_attempts++;
        
        // Update TCP-specific tracking
        status.tcp_tracking.client_fds.push_back(client_fd);
        status.tcp_tracking.connection_times[client_fd] = now;
        status.tcp_tracking.last_client_ip = client_ip;
        status.tcp_tracking.last_client_port = client_port;
        
        // Update connection info
        status.connection_info = "TCP: " + client_ip + ":" + std::to_string(client_port) + 
                                " (fd:" + std::to_string(client_fd) + ")";
        status.last_activity = format_timestamp(now);
        
        if (status.first_connection_time == std::chrono::steady_clock::time_point{}) {
            status.first_connection_time = now;
        }
        
        // Enhanced occupancy detection using connection events
        bool new_occupied = has_connection_activity(status);
        if (new_occupied != status.occupied) {
            status.occupied = new_occupied;
            log_info("Endpoint '%s' occupancy changed to %s (TCP connection: %s:%d fd=%d, active: %u)", 
                     endpoint_name.c_str(), new_occupied ? "OCCUPIED" : "FREE", 
                     client_ip.c_str(), client_port, client_fd, status.active_connections);
            notify_occupancy_change(endpoint_name, new_occupied);
        }
        
        if (config_.enable_detailed_logging) {
            log_debug("TCP connection accepted on '%s': %s:%d (fd=%d), total connections: %u, active: %u",
                     endpoint_name.c_str(), client_ip.c_str(), client_port, client_fd, 
                     status.total_connections, status.active_connections);
        }
    }
}

void EndpointMonitor::notify_tcp_connection_closed(const std::string& endpoint_name, int client_fd)
{
    std::lock_guard<std::mutex> lock(status_mutex_);
    
    auto it = endpoint_status_.find(endpoint_name);
    if (it != endpoint_status_.end()) {
        auto& status = it->second;
        auto now = std::chrono::steady_clock::now();
        
        // Update connection metrics
        if (status.active_connections > 0) {
            status.active_connections--;
        }
        
        // Remove from TCP tracking
        auto& client_fds = status.tcp_tracking.client_fds;
        auto fd_it = std::find(client_fds.begin(), client_fds.end(), client_fd);
        if (fd_it != client_fds.end()) {
            client_fds.erase(fd_it);
        }
        
        status.tcp_tracking.connection_times.erase(client_fd);
        status.last_activity_time = now;
        status.last_activity = format_timestamp(now);
        
        // Update connection state
        if (status.active_connections == 0) {
            status.connection_state = EndpointStatus::ConnectionState::Disconnected;
        }
        
        // Update connection info
        status.connection_info = "TCP: Connection closed (fd:" + std::to_string(client_fd) + ")";
        
        // Enhanced occupancy detection
        bool new_occupied = has_connection_activity(status);
        if (new_occupied != status.occupied) {
            status.occupied = new_occupied;
            log_info("Endpoint '%s' occupancy changed to %s (TCP connection closed: fd=%d, active: %u)", 
                     endpoint_name.c_str(), new_occupied ? "OCCUPIED" : "FREE", 
                     client_fd, status.active_connections);
            notify_occupancy_change(endpoint_name, new_occupied);
        }
        
        if (config_.enable_detailed_logging) {
            log_debug("TCP connection closed on '%s': fd=%d, remaining active connections: %u",
                     endpoint_name.c_str(), client_fd, status.active_connections);
        }
    }
}

void EndpointMonitor::notify_udp_unknown_messages(const std::string& endpoint_name, uint32_t message_count,
                                                   const std::string& remote_ip, uint16_t remote_port, 
                                                   uint32_t message_id)
{
    std::lock_guard<std::mutex> lock(status_mutex_);
    
    auto it = endpoint_status_.find(endpoint_name);
    if (it != endpoint_status_.end()) {
        auto& status = it->second;
        auto now = std::chrono::steady_clock::now();
        
        // Update UDP tracking
        status.udp_messages_received = true;
        status.unknown_message_count += message_count;
        status.last_activity_time = now;
        status.connection_state = EndpointStatus::ConnectionState::Activity;
        
        // Update UDP-specific tracking
        std::string remote_endpoint = remote_ip + ":" + std::to_string(remote_port);
        status.udp_tracking.remote_endpoints.insert(remote_endpoint);
        status.udp_tracking.message_counts[remote_endpoint] += message_count;
        status.udp_tracking.last_remote_ip = remote_ip;
        status.udp_tracking.last_remote_port = remote_port;
        
        // Update connection info
        status.connection_info = "UDP: " + remote_endpoint + " (msg_id:" + std::to_string(message_id) + 
                                " count:" + std::to_string(message_count) + ")";
        status.last_activity = format_timestamp(now);
        
        if (status.first_connection_time == std::chrono::steady_clock::time_point{}) {
            status.first_connection_time = now;
        }
        
        // Enhanced occupancy detection using message events
        bool new_occupied = has_connection_activity(status);
        if (new_occupied != status.occupied) {
            status.occupied = new_occupied;
            log_info("Endpoint '%s' occupancy changed to %s (UDP activity: %s:%d msg_id=%u count=%u, unique endpoints: %zu)", 
                     endpoint_name.c_str(), new_occupied ? "OCCUPIED" : "FREE", 
                     remote_ip.c_str(), remote_port, message_id, message_count,
                     status.udp_tracking.remote_endpoints.size());
            notify_occupancy_change(endpoint_name, new_occupied);
        }
        
        if (config_.enable_detailed_logging) {
            log_debug("UDP unknown messages on '%s': %s:%d (msg_id=%u, count=%u), total unknown: %u, unique endpoints: %zu",
                     endpoint_name.c_str(), remote_ip.c_str(), remote_port, message_id, message_count,
                     status.unknown_message_count, status.udp_tracking.remote_endpoints.size());
        }
    }
}

void EndpointMonitor::notify_endpoint_activity(const std::string& endpoint_name, 
                                                const std::string& activity_type, 
                                                const std::string& details)
{
    std::lock_guard<std::mutex> lock(status_mutex_);
    
    auto it = endpoint_status_.find(endpoint_name);
    if (it != endpoint_status_.end()) {
        auto& status = it->second;
        auto now = std::chrono::steady_clock::now();
        
        status.last_activity_time = now;
        status.last_activity = format_timestamp(now);
        
        if (activity_type == "send" || activity_type == "receive") {
            status.connection_state = EndpointStatus::ConnectionState::Activity;
        }
        
        if (config_.enable_detailed_logging) {
            log_debug("Endpoint activity on '%s': %s - %s", 
                     endpoint_name.c_str(), activity_type.c_str(), details.c_str());
        }
    }
}

std::map<std::string, uint32_t> EndpointMonitor::get_connection_metrics(const std::string& endpoint_name) const
{
    std::lock_guard<std::mutex> lock(status_mutex_);
    
    std::map<std::string, uint32_t> metrics;
    auto it = endpoint_status_.find(endpoint_name);
    
    if (it != endpoint_status_.end()) {
        const auto& status = it->second;
        metrics["total_connections"] = status.total_connections;
        metrics["active_connections"] = status.active_connections;
        metrics["connection_attempts"] = status.connection_attempts;
        metrics["failed_connections"] = status.failed_connections;
        metrics["unknown_messages"] = status.unknown_message_count;
        metrics["unique_remote_endpoints"] = status.udp_tracking.remote_endpoints.size();
        metrics["tcp_clients"] = status.tcp_tracking.client_fds.size();
    }
    
    return metrics;
}

std::map<std::string, std::string> EndpointMonitor::get_connection_states() const
{
    std::lock_guard<std::mutex> lock(status_mutex_);
    
    std::map<std::string, std::string> states;
    
    for (const auto& pair : endpoint_status_) {
        const auto& status = pair.second;
        std::string state_str;
        
        switch (status.connection_state) {
            case EndpointStatus::ConnectionState::Disconnected:
                state_str = "Disconnected";
                break;
            case EndpointStatus::ConnectionState::Connecting:
                state_str = "Connecting";
                break;
            case EndpointStatus::ConnectionState::Connected:
                state_str = "Connected";
                break;
            case EndpointStatus::ConnectionState::Activity:
                state_str = "Activity";
                break;
        }
        
        states[pair.first] = state_str + " (" + 
                             (status.occupied ? "Occupied" : "Free") + ")";
    }
    
    return states;
}

void EndpointMonitor::cleanup_stale_connections()
{
    std::lock_guard<std::mutex> lock(status_mutex_);
    
    auto now = std::chrono::steady_clock::now();
    auto timeout = std::chrono::milliseconds(config_.activity_timeout_ms);
    
    for (auto& pair : endpoint_status_) {
        auto& status = pair.second;
        
        // Check for stale TCP connections
        auto& tcp_times = status.tcp_tracking.connection_times;
        for (auto it = tcp_times.begin(); it != tcp_times.end();) {
            if (now - it->second > timeout) {
                // Remove stale connection
                int stale_fd = it->first;
                it = tcp_times.erase(it);
                
                auto& client_fds = status.tcp_tracking.client_fds;
                auto fd_it = std::find(client_fds.begin(), client_fds.end(), stale_fd);
                if (fd_it != client_fds.end()) {
                    client_fds.erase(fd_it);
                }
                
                if (status.active_connections > 0) {
                    status.active_connections--;
                }
                
                if (config_.enable_detailed_logging) {
                    log_debug("Cleaned up stale TCP connection on '%s': fd=%d", 
                             pair.first.c_str(), stale_fd);
                }
            } else {
                ++it;
            }
        }
        
        // Update connection state if no active connections
        if (status.active_connections == 0 && 
            status.connection_state != EndpointStatus::ConnectionState::Disconnected) {
            status.connection_state = EndpointStatus::ConnectionState::Disconnected;
        }
    }
}

bool EndpointMonitor::has_connection_activity(const EndpointStatus& status) const
{
    // Enhanced occupancy detection using connection-based tracking
    if (status.type == ENDPOINT_TYPE_TCP) {
        // TCP is occupied if has active connections OR has traditional server+client
        return status.active_connections > 0 || 
               status.tcp_connection_accepted || 
               (status.has_server && status.has_client);
    } else if (status.type == ENDPOINT_TYPE_UDP) {
        // UDP is occupied if has unknown messages OR has traditional server+client
        return status.udp_messages_received || 
               status.unknown_message_count > 0 || 
               (status.has_server && status.has_client);
    }
    
    // Fallback to traditional detection
    return status.has_server && status.has_client;
}

// Real-time monitoring publishing implementations
void EndpointMonitor::set_rpc_client(std::shared_ptr<ur_rpc::RpcClient> rpc_client)
{
    std::lock_guard<std::mutex> lock(publish_mutex_);
    rpc_client_ = rpc_client;
    log_info("RPC client set for endpoint monitoring publishing");
}

nlohmann::json EndpointMonitor::create_monitoring_json() const
{
    nlohmann::json monitoring_data;
    auto now = std::chrono::steady_clock::now();
    
    // Create monitoring header
    monitoring_data["header"] = {
        {"timestamp", format_timestamp(now)},
        {"sequence", publish_sequence_.load()},
        {"source", "ur-mavrouter"},
        {"type", "endpoint_monitoring"},
        {"version", "1.0"}
    };
    
    // Main router endpoints
    nlohmann::json mainloop_endpoints = nlohmann::json::array();
    {
        std::lock_guard<std::mutex> lock(status_mutex_);
        
        for (const auto& pair : endpoint_status_) {
            const auto& status = pair.second;
            
            nlohmann::json endpoint_json;
            endpoint_json["name"] = status.name;
            endpoint_json["type"] = status.type;
            endpoint_json["fd"] = status.fd;
            endpoint_json["occupied"] = status.occupied;
            endpoint_json["has_server"] = status.has_server;
            endpoint_json["has_client"] = status.has_client;
            endpoint_json["last_activity"] = status.last_activity;
            endpoint_json["connection_state"] = static_cast<int>(status.connection_state);
            
            // Connection metrics
            if (config_.include_metrics) {
                endpoint_json["metrics"] = {
                    {"total_connections", status.total_connections},
                    {"active_connections", status.active_connections},
                    {"connection_attempts", status.connection_attempts},
                    {"failed_connections", status.failed_connections},
                    {"unknown_messages", status.unknown_message_count}
                };
            }
            
            // Protocol-specific details
            if (config_.include_connection_details) {
                if (status.type == ENDPOINT_TYPE_TCP) {
                    endpoint_json["tcp_details"] = {
                        {"last_client_ip", status.tcp_tracking.last_client_ip},
                        {"last_client_port", status.tcp_tracking.last_client_port},
                        {"active_clients", status.tcp_tracking.client_fds.size()}
                    };
                } else if (status.type == ENDPOINT_TYPE_UDP) {
                    endpoint_json["udp_details"] = {
                        {"last_remote_ip", status.udp_tracking.last_remote_ip},
                        {"last_remote_port", status.udp_tracking.last_remote_port},
                        {"unique_endpoints", status.udp_tracking.remote_endpoints.size()},
                        {"broadcast_messages", status.udp_tracking.broadcast_messages},
                        {"multicast_messages", status.udp_tracking.multicast_messages}
                    };
                }
            }
            
            endpoint_json["connection_info"] = status.connection_info;
            mainloop_endpoints.push_back(endpoint_json);
        }
    }
    
    monitoring_data["endpoints"] = {
        {"main_router", mainloop_endpoints}
    };
    
    // Extension endpoints
    nlohmann::json extension_endpoints = nlohmann::json::object();
    {
        std::lock_guard<std::mutex> lock(status_mutex_);
        
        for (const auto& ext_pair : extension_loops_) {
            const std::string& ext_name = ext_pair.first;
            // Note: Extension endpoints would be tracked similarly to main router
            // For now, we'll add a placeholder structure
            extension_endpoints[ext_name] = nlohmann::json::array();
        }
    }
    
    monitoring_data["endpoints"]["extensions"] = extension_endpoints;
    
    // Summary statistics
    monitoring_data["summary"] = {
        {"total_endpoints", mainloop_endpoints.size()},
        {"occupied_endpoints", std::count_if(endpoint_status_.begin(), endpoint_status_.end(),
                                            [](const auto& pair) { return pair.second.occupied; })},
        {"monitoring_enabled", config_.enable_realtime_publishing},
        {"connection_tracking", config_.enable_connection_tracking}
    };
    
    return monitoring_data;
}

void EndpointMonitor::publish_monitoring_data(bool force_publish)
{
    if (!config_.enable_realtime_publishing) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(publish_mutex_);
    
    if (!rpc_client_) {
        if (config_.enable_detailed_logging) {
            log_debug("RPC client not available for monitoring publishing");
        }
        return;
    }
    
    auto now = std::chrono::steady_clock::now();
    auto time_since_last_publish = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - last_publish_time_).count();
    
    // Check if we should publish (based on interval or force)
    if (!force_publish && time_since_last_publish < static_cast<int64_t>(config_.publish_interval_ms)) {
        return;
    }
    
    try {
        // Create monitoring JSON
        auto monitoring_json = create_monitoring_json();
        
        // Check for changes if publish_on_change is enabled
        if (!force_publish && config_.publish_on_change) {
            // Simple change detection - compare with last published status
            bool has_changes = false;
            // For now, always publish if interval has passed
            // TODO: Implement proper change detection
        }
        
        // Publish to MQTT topic
        std::string json_payload = monitoring_json.dump();
        
        // Use RPC client to publish to MQTT
        // Note: This would depend on the RPC client's MQTT publishing interface
        if (rpc_client_) {
            // Implementation would depend on RPC client API
            // For now, we'll log the intent
            log_info("Publishing endpoint monitoring data to topic: %s", config_.realtime_topic.c_str());
            
            if (config_.enable_detailed_logging) {
                log_debug("Monitoring JSON payload: %s", json_payload.c_str());
            }
            
            // TODO: Actual MQTT publish via RPC client
            // rpc_client_->publish_mqtt(config_.realtime_topic, json_payload);
        }
        
        last_publish_time_ = now;
        publish_sequence_++;
        
        // Update last published status
        {
            std::lock_guard<std::mutex> status_lock(status_mutex_);
            last_published_status_ = endpoint_status_;
        }
        
    } catch (const std::exception& e) {
        log_error("Failed to publish monitoring data: %s", e.what());
    }
}

void EndpointMonitor::publish_status_change(const std::string& endpoint_name, 
                                            const EndpointStatus& old_status, 
                                            const EndpointStatus& new_status)
{
    if (!config_.enable_realtime_publishing || !config_.publish_on_change) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(publish_mutex_);
    
    if (!rpc_client_) {
        return;
    }
    
    try {
        nlohmann::json change_notification;
        auto now = std::chrono::steady_clock::now();
        
        change_notification["header"] = {
            {"timestamp", format_timestamp(now)},
            {"sequence", publish_sequence_.load()},
            {"source", "ur-mavrouter"},
            {"type", "endpoint_status_change"},
            {"version", "1.0"}
        };
        
        change_notification["endpoint_name"] = endpoint_name;
        change_notification["change_type"] = (old_status.occupied != new_status.occupied) ? "occupancy" : "status";
        
        change_notification["old_status"] = {
            {"occupied", old_status.occupied},
            {"connection_state", static_cast<int>(old_status.connection_state)},
            {"active_connections", old_status.active_connections}
        };
        
        change_notification["new_status"] = {
            {"occupied", new_status.occupied},
            {"connection_state", static_cast<int>(new_status.connection_state)},
            {"active_connections", new_status.active_connections}
        };
        
        std::string json_payload = change_notification.dump();
        
        log_info("Publishing endpoint status change for '%s' to topic: %s", 
                endpoint_name.c_str(), config_.realtime_topic.c_str());
        
        if (config_.enable_detailed_logging) {
            log_debug("Status change JSON: %s", json_payload.c_str());
        }
        
        // TODO: Actual MQTT publish via RPC client
        // rpc_client_->publish_mqtt(config_.realtime_topic, json_payload);
        
        publish_sequence_++;
        
    } catch (const std::exception& e) {
        log_error("Failed to publish status change: %s", e.what());
    }
}

// GlobalMonitor implementation
EndpointMonitor& GlobalMonitor::get_instance()
{
    std::lock_guard<std::mutex> lock(instance_mutex_);
    if (!instance_) {
        throw std::runtime_error("GlobalMonitor not initialized. Call initialize() first.");
    }
    return *instance_;
}

void GlobalMonitor::initialize(Mainloop& main_router, const MonitorConfig& config)
{
    std::lock_guard<std::mutex> lock(instance_mutex_);
    if (instance_) {
        log_warning("GlobalMonitor already initialized");
        return;
    }
    
    instance_ = std::make_unique<EndpointMonitor>(main_router, config);
    log_info("GlobalMonitor initialized");
}

void GlobalMonitor::cleanup()
{
    std::lock_guard<std::mutex> lock(instance_mutex_);
    if (instance_) {
        if (instance_->is_running()) {
            instance_->stop();
        }
        instance_.reset();
        log_info("GlobalMonitor cleaned up");
    }
}

bool GlobalMonitor::is_initialized()
{
    std::lock_guard<std::mutex> lock(instance_mutex_);
    return instance_ != nullptr;
}

} // namespace EndpointMonitoring
