#include "vpn_instance_manager.hpp"
#include "internal/vpn_manager_utils.hpp"
#include "../ur-openvpn-library/src/openvpn_wrapper.hpp"
#include "../ur-wg_library/wireguard-wrapper/include/wireguard_wrapper.hpp"
#include <fstream>
#include <iostream>
#include <thread>
#include <chrono>
#include <future>

namespace vpn_manager {

void VPNInstanceManager::launchInstanceThread(VPNInstance& instance) {
    if (instance.type == VPNType::OPENVPN) {
        auto wrapper = std::make_shared<openvpn::OpenVPNWrapper>();
        instance.wrapper_instance = wrapper;

        // Write config to temp file
        std::string config_file = "/tmp/vpn_" + instance.name + ".ovpn";
        std::ofstream file(config_file);
        file << instance.config_content;
        file.close();

        // Set callbacks
        wrapper->setEventCallback([this, name = instance.name](const openvpn::VPNEvent& event) {
            json data;
            data["state"] = static_cast<int>(event.state);
            data["event_data"] = event.data;
            emitEvent(name, event.type, event.message, data);
            
            // Apply routing rules and start monitoring when connected
            if (event.type == "connected") {
                applyRoutingRulesForInstance(name);
                
                // Clear previous snapshot to force fresh detection
                last_route_snapshots_.erase(name);
                
                if (verbose_) {
                    std::cout << json({
                        {"type", "verbose"},
                        {"message", "VPN connected - route monitoring active"},
                        {"instance", name}
                    }).dump() << std::endl;
                }
            }
        });

        wrapper->setStatsCallback([this, name = instance.name](const openvpn::VPNStats& stats) {
            // Capture by value to avoid holding mutex during callback
            uint64_t session_start = 0;
            uint64_t session_seconds = 0;

            {
                std::lock_guard<std::mutex> lock(instances_mutex_);
                auto it = instances_.find(name);
                if (it != instances_.end()) {
                    // Store cumulative byte totals (not rates!)
                    it->second.data_transfer.upload_bytes = stats.bytes_sent;
                    it->second.data_transfer.download_bytes = stats.bytes_received;

                    // Update session totals (cumulative bytes)
                    it->second.total_data_transferred.current_session_bytes = stats.bytes_sent + stats.bytes_received;

                    // Update connection time
                    if (it->second.connection_time.current_session_start > 0) {
                        it->second.connection_time.current_session_seconds = time(nullptr) - it->second.connection_time.current_session_start;
                    }

                    // Status is updated via event callbacks, not from stats
                    // Keep current status as-is during stats updates

                    session_start = it->second.connection_time.current_session_start;
                    session_seconds = it->second.connection_time.current_session_seconds;
                }
            } // Release mutex before emitting event

            // Format data for display (outside mutex) matching target JSON structure
            json data;
            data["upload_bytes"] = stats.bytes_sent;
            data["download_bytes"] = stats.bytes_received;
            data["upload_rate_bps"] = stats.upload_rate_bps;
            data["download_rate_bps"] = stats.download_rate_bps;
            data["upload_rate_formatted"] = formatBytes(stats.upload_rate_bps) + "/s";
            data["download_rate_formatted"] = formatBytes(stats.download_rate_bps) + "/s";
            data["upload_formatted"] = formatBytes(stats.bytes_sent);
            data["download_formatted"] = formatBytes(stats.bytes_received);
            data["total_session_mb"] = (stats.bytes_sent + stats.bytes_received) / (1024.0 * 1024.0);
            data["connection_time"] = formatTime(session_seconds);
            data["ping_ms"] = stats.ping_ms;

            emitEvent(name, "stats", "Statistics update", data);

            // Update connection_stats with current statistics
            {
                std::lock_guard<std::mutex> lock(instances_mutex_);
                auto it = instances_.find(name);
                if (it != instances_.end()) {
                    it->second.connection_stats = data;
                }
            }

            // Trigger config save
            config_save_pending_.store(true);
        });

        // Create thread function
        auto thread_func = [wrapper, config_file, &instance, this]() {
            // Block signals in this thread - let main thread handle them
            sigset_t signal_mask;
            sigfillset(&signal_mask);
            pthread_sigmask(SIG_BLOCK, &signal_mask, nullptr);

            if (!wrapper->initializeFromFile(config_file)) {
                emitEvent(instance.name, "error", "Failed to initialize OpenVPN");
                return;
            }

            if (!wrapper->connect()) {
                emitEvent(instance.name, "error", "Failed to connect OpenVPN");
                return;
            }

            while (!instance.should_stop && running_) {
                if (!wrapper->isConnected()) {
                    if (instance.auto_connect) {
                        std::cout << json({
                            {"type", "auto_reconnect"},
                            {"instance", instance.name},
                            {"message", "Attempting auto-reconnect"}
                        }).dump() << std::endl;
                        wrapper->reconnect();
                    } else {
                        break;
                    }
                }
                std::this_thread::sleep_for(std::chrono::seconds(5));
            }

            wrapper->disconnect();
        };

        instance.thread_id = thread_manager_->createThread(thread_func);
        instance.start_time = time(nullptr);

    } else if (instance.type == VPNType::WIREGUARD) {
        auto wrapper = std::make_shared<wireguard::WireGuardWrapper>();
        instance.wrapper_instance = wrapper;

        // Write config to temp file
        std::string config_file = "/tmp/vpn_" + instance.name + ".conf";
        std::ofstream file(config_file);
        file << instance.config_content;
        file.close();

        // Set callbacks
        wrapper->setEventCallback([this, name = instance.name](const wireguard::VPNEvent& event) {
            json data;
            data["state"] = static_cast<int>(event.state);
            data["event_data"] = event.data;
            emitEvent(name, event.type, event.message, data);
            
            // Apply routing rules and start monitoring when connected
            if (event.type == "connected") {
                applyRoutingRulesForInstance(name);
                
                // Clear previous snapshot to force fresh detection
                last_route_snapshots_.erase(name);
                
                if (verbose_) {
                    std::cout << json({
                        {"type", "verbose"},
                        {"message", "VPN connected - route monitoring active"},
                        {"instance", name}
                    }).dump() << std::endl;
                }
            }
        });

        wrapper->setStatsCallback([this, name = instance.name](const wireguard::VPNStats& stats) {
            // Capture by value to avoid holding mutex during callback
            uint64_t session_start = 0;
            uint64_t session_seconds = 0;

            {
                std::lock_guard<std::mutex> lock(instances_mutex_);
                auto it = instances_.find(name);
                if (it != instances_.end()) {
                    // Store cumulative byte totals (not rates!)
                    it->second.data_transfer.upload_bytes = stats.bytes_sent;
                    it->second.data_transfer.download_bytes = stats.bytes_received;

                    // Update session totals (cumulative bytes)
                    it->second.total_data_transferred.current_session_bytes = stats.bytes_sent + stats.bytes_received;

                    // Update connection time
                    if (it->second.connection_time.current_session_start > 0) {
                        it->second.connection_time.current_session_seconds = time(nullptr) - it->second.connection_time.current_session_start;
                    }

                    // Status is updated via event callbacks, not from stats
                    // Keep current status as-is during stats updates

                    session_start = it->second.connection_time.current_session_start;
                    session_seconds = it->second.connection_time.current_session_seconds;
                }
            } // Release mutex before emitting event

            // Format data for display (outside mutex) matching target JSON structure
            json data;
            data["upload_bytes"] = stats.bytes_sent;
            data["download_bytes"] = stats.bytes_received;
            data["upload_rate_bps"] = stats.upload_rate_bps;
            data["download_rate_bps"] = stats.download_rate_bps;
            data["upload_rate_formatted"] = formatBytes(stats.upload_rate_bps) + "/s";
            data["download_rate_formatted"] = formatBytes(stats.download_rate_bps) + "/s";
            data["upload_formatted"] = formatBytes(stats.bytes_sent);
            data["download_formatted"] = formatBytes(stats.bytes_received);
            data["total_session_mb"] = (stats.bytes_sent + stats.bytes_received) / (1024.0 * 1024.0);
            data["connection_time"] = formatTime(session_seconds);
            data["latency_ms"] = stats.latency_ms;

            emitEvent(name, "stats", "Statistics update", data);

            // Update connection_stats with current statistics
            {
                std::lock_guard<std::mutex> lock(instances_mutex_);
                auto it = instances_.find(name);
                if (it != instances_.end()) {
                    it->second.connection_stats = data;
                }
            }

            // Trigger config save
            config_save_pending_.store(true);
        });

        // Create thread function
        auto thread_func = [wrapper, config_file, &instance, this]() {
            // Block signals in this thread - let main thread handle them
            sigset_t signal_mask;
            sigfillset(&signal_mask);
            pthread_sigmask(SIG_BLOCK, &signal_mask, nullptr);

            if (!wrapper->initializeFromFile(config_file)) {
                emitEvent(instance.name, "error", "Failed to initialize WireGuard");
                return;
            }

            if (!wrapper->connect()) {
                emitEvent(instance.name, "error", "Failed to connect WireGuard");
                return;
            }

            while (!instance.should_stop && running_) {
                if (!wrapper->isConnected()) {
                    if (instance.auto_connect) {
                        std::cout << json({
                            {"type", "auto_reconnect"},
                            {"instance", instance.name},
                            {"message", "Attempting auto-reconnect"}
                        }).dump() << std::endl;
                        wrapper->reconnect();
                    } else {
                        break;
                    }
                }
                std::this_thread::sleep_for(std::chrono::seconds(5));
            }

            // Disconnect is now called from stopInstance() to ensure it completes before thread termination
            // Only disconnect here if thread exits naturally (not from stopInstance)
            if (!instance.should_stop) {
                wrapper->disconnect();
            }
        };

        instance.thread_id = thread_manager_->createThread(thread_func);
        instance.start_time = time(nullptr);
    }

    // Register thread with attachment
    thread_manager_->registerThread(instance.thread_id, instance.name);
}

bool VPNInstanceManager::startInstance(const std::string& instance_id) {
    std::lock_guard<std::mutex> lock(instances_mutex_);

    auto it = instances_.find(instance_id);
    if (it == instances_.end()) {
        return false;
    }

    it->second.enabled = true;
    it->second.status = "Connecting";
    it->second.last_used = std::to_string(time(nullptr));

    // Initialize connection metrics
    time_t now = time(nullptr);
    it->second.connection_time.current_session_start = now;
    it->second.connection_time.current_session_seconds = 0;
    it->second.data_transfer.upload_bytes = 0;
    it->second.data_transfer.download_bytes = 0;
    it->second.total_data_transferred.current_session_bytes = 0;

    launchInstanceThread(it->second);
    emitEvent(instance_id, "started", "Instance started");
    
    // Note: Routing rules will be applied when instance connects (in event callback)
    return true;
}

void VPNInstanceManager::forceCleanupNetworkInterface(const std::string& interface_name, VPNType vpn_type) {
    std::cout << json({
        {"type", "shutdown_verbose"},
        {"step", "FORCE_CLEANUP_INTERFACE_START"},
        {"interface", interface_name},
        {"vpn_type", vpnTypeToString(vpn_type)},
        {"message", "Starting forced manual cleanup of network resources"}
    }).dump() << std::endl;
    std::cout.flush();

    // Step 1: Clear routes associated with the interface
    std::cout << json({
        {"type", "shutdown_verbose"},
        {"step", "CLEANUP_ROUTES"},
        {"interface", interface_name},
        {"message", "Removing all routes for interface"}
    }).dump() << std::endl;
    std::cout.flush();

    std::string route_cleanup_cmd = "ip route flush dev " + interface_name + " 2>/dev/null || true";
    int route_result = system(route_cleanup_cmd.c_str());

    std::cout << json({
        {"type", "shutdown_verbose"},
        {"step", "CLEANUP_ROUTES_RESULT"},
        {"interface", interface_name},
        {"command", route_cleanup_cmd},
        {"result_code", route_result},
        {"status", route_result == 0 ? "success" : "completed_with_warnings"}
    }).dump() << std::endl;
    std::cout.flush();

    // Step 2: Bring interface down
    std::cout << json({
        {"type", "shutdown_verbose"},
        {"step", "INTERFACE_DOWN"},
        {"interface", interface_name},
        {"message", "Bringing network interface down"}
    }).dump() << std::endl;
    std::cout.flush();

    std::string down_cmd = "ip link set " + interface_name + " down 2>/dev/null || true";
    int down_result = system(down_cmd.c_str());

    std::cout << json({
        {"type", "shutdown_verbose"},
        {"step", "INTERFACE_DOWN_RESULT"},
        {"interface", interface_name},
        {"command", down_cmd},
        {"result_code", down_result},
        {"status", down_result == 0 ? "success" : "completed_with_warnings"}
    }).dump() << std::endl;
    std::cout.flush();

    // Step 3: Delete the interface
    std::cout << json({
        {"type", "shutdown_verbose"},
        {"step", "INTERFACE_DELETE"},
        {"interface", interface_name},
        {"message", "Deleting network interface"}
    }).dump() << std::endl;
    std::cout.flush();

    std::string delete_cmd = "ip link del " + interface_name + " 2>/dev/null || true";
    int delete_result = system(delete_cmd.c_str());

    std::cout << json({
        {"type", "shutdown_verbose"},
        {"step", "INTERFACE_DELETE_RESULT"},
        {"interface", interface_name},
        {"command", delete_cmd},
        {"result_code", delete_result},
        {"status", delete_result == 0 ? "success" : "completed_with_warnings"}
    }).dump() << std::endl;
    std::cout.flush();

    // Step 4: Verify interface is gone
    std::string verify_cmd = "ip link show " + interface_name + " 2>/dev/null";
    int verify_result = system(verify_cmd.c_str());

    std::cout << json({
        {"type", "shutdown_verbose"},
        {"step", "FORCE_CLEANUP_VERIFICATION"},
        {"interface", interface_name},
        {"interface_still_exists", verify_result == 0},
        {"cleanup_status", verify_result == 0 ? "partial_cleanup" : "complete_cleanup"}
    }).dump() << std::endl;
    std::cout.flush();

    std::cout << json({
        {"type", "shutdown_verbose"},
        {"step", "FORCE_CLEANUP_INTERFACE_COMPLETE"},
        {"interface", interface_name},
        {"vpn_type", vpnTypeToString(vpn_type)},
        {"message", "Forced cleanup completed - routes flushed, interface down and deleted"}
    }).dump() << std::endl;
    std::cout.flush();
}

bool VPNInstanceManager::disconnectWrapperWithTimeout(std::shared_ptr<void> wrapper_instance,
                                                      VPNType vpn_type,
                                                      const std::string& instance_id,
                                                      int timeout_seconds) {
    std::cout << json({
        {"type", "shutdown_verbose"},
        {"step", "WRAPPER_DISCONNECT_START"},
        {"instance_id", instance_id},
        {"vpn_type", vpnTypeToString(vpn_type)},
        {"timeout_seconds", timeout_seconds}
    }).dump() << std::endl;
    std::cout.flush();

    std::atomic<bool> disconnect_completed(false);
    std::atomic<bool> disconnect_failed(false);

    auto disconnect_task = std::async(std::launch::async, [&]() {
        try {
            if (vpn_type == VPNType::OPENVPN) {
                auto ovpn_wrapper = std::static_pointer_cast<openvpn::OpenVPNWrapper>(wrapper_instance);
                ovpn_wrapper->disconnect();
            } else if (vpn_type == VPNType::WIREGUARD) {
                auto wg_wrapper = std::static_pointer_cast<wireguard::WireGuardWrapper>(wrapper_instance);
                wg_wrapper->disconnect();
            }
            disconnect_completed = true;
        } catch (const std::exception& e) {
            std::cerr << json({
                {"type", "error"},
                {"message", "Exception in wrapper disconnect"},
                {"instance_id", instance_id},
                {"error", e.what()}
            }).dump() << std::endl;
            disconnect_failed = true;
        }
    });

    auto status = disconnect_task.wait_for(std::chrono::seconds(timeout_seconds));

    if (status == std::future_status::deferred) {
        std::cerr << json({
            {"type", "error"},
            {"message", "Future returned deferred status - forcing execution"},
            {"instance_id", instance_id}
        }).dump() << std::endl;
        disconnect_task.get();
    }

    if (status == std::future_status::ready && disconnect_completed) {
        std::cout << json({
            {"type", "shutdown_verbose"},
            {"step", "WRAPPER_DISCONNECT_SUCCESS"},
            {"instance_id", instance_id}
        }).dump() << std::endl;
        std::cout.flush();
        return true;
    } else {
        std::cout << json({
            {"type", "shutdown_verbose"},
            {"step", "WRAPPER_DISCONNECT_TIMEOUT"},
            {"instance_id", instance_id},
            {"message", "Wrapper disconnect timed out or failed, will force cleanup"}
        }).dump() << std::endl;
        std::cout.flush();
        return false;
    }
}

bool VPNInstanceManager::stopThreadWithTimeout(unsigned int thread_id,
                                               const std::string& instance_id,
                                               int timeout_seconds) {
    std::cout << json({
        {"type", "shutdown_verbose"},
        {"step", "THREAD_STOP_START"},
        {"instance_id", instance_id},
        {"thread_id", thread_id},
        {"timeout_seconds", timeout_seconds}
    }).dump() << std::endl;
    std::cout.flush();

    std::atomic<bool> thread_stopped(false);

    auto stop_task = std::async(std::launch::async, [&]() {
        try {
            thread_manager_->stopThreadByAttachment(instance_id);
            thread_stopped = true;
        } catch (const std::exception& e) {
            std::cerr << json({
                {"type", "error"},
                {"message", "Exception stopping thread"},
                {"instance_id", instance_id},
                {"error", e.what()}
            }).dump() << std::endl;
        }
    });

    auto status = stop_task.wait_for(std::chrono::seconds(timeout_seconds));

    if (status == std::future_status::deferred) {
        std::cerr << json({
            {"type", "error"},
            {"message", "Future returned deferred status - forcing execution"},
            {"instance_id", instance_id}
        }).dump() << std::endl;
        stop_task.get();
    }

    if (status == std::future_status::ready && thread_stopped) {
        std::cout << json({
            {"type", "shutdown_verbose"},
            {"step", "THREAD_STOP_SUCCESS"},
            {"instance_id", instance_id}
        }).dump() << std::endl;
        std::cout.flush();
        return true;
    } else {
        std::cout << json({
            {"type", "shutdown_verbose"},
            {"step", "THREAD_STOP_TIMEOUT"},
            {"instance_id", instance_id},
            {"message", "Thread stop timed out, thread may still be running"}
        }).dump() << std::endl;
        std::cout.flush();
        return false;
    }
}

bool VPNInstanceManager::stopInstance(const std::string& instance_id) {
    std::cout << json({
        {"type", "shutdown_verbose"},
        {"step", "STOP_INSTANCE_START"},
        {"instance_id", instance_id},
        {"message", "Starting robust shutdown with timeout-based cleanup"}
    }).dump() << std::endl;
    std::cout.flush();

    std::shared_ptr<void> wrapper_instance;
    VPNType vpn_type;
    unsigned int thread_id = 0;
    std::string interface_name;

    {
        std::lock_guard<std::mutex> lock(instances_mutex_);

        auto it = instances_.find(instance_id);
        if (it == instances_.end()) {
            return false;
        }

        it->second.should_stop = true;
        // Don't automatically set enabled to false - only via HTTP requests
        it->second.status = "Disconnecting";

        wrapper_instance = it->second.wrapper_instance;
        vpn_type = it->second.type;
        thread_id = it->second.thread_id;

        if (vpn_type == VPNType::WIREGUARD) {
            interface_name = "wg0";
        } else if (vpn_type == VPNType::OPENVPN) {
            interface_name = "tun0";
        }
    }

    bool wrapper_cleanup_success = false;
    bool thread_stop_success = false;

    if (wrapper_instance) {
        wrapper_cleanup_success = disconnectWrapperWithTimeout(wrapper_instance, vpn_type, instance_id, 5);

        if (wrapper_cleanup_success) {
            std::this_thread::sleep_for(std::chrono::milliseconds(800));
        } else {
            std::cout << json({
                {"type", "shutdown_verbose"},
                {"step", "WRAPPER_CLEANUP_FAILED"},
                {"instance_id", instance_id},
                {"message", "Wrapper cleanup failed or timed out, forcing manual interface cleanup"}
            }).dump() << std::endl;
            std::cout.flush();

            if (!interface_name.empty()) {
                forceCleanupNetworkInterface(interface_name, vpn_type);
            }
        }
    }

    if (thread_id > 0) {
        thread_stop_success = stopThreadWithTimeout(thread_id, instance_id, 3);

        if (!thread_stop_success) {
            std::cout << json({
                {"type", "shutdown_verbose"},
                {"step", "THREAD_FORCE_ABANDONED"},
                {"instance_id", instance_id},
                {"thread_id", thread_id},
                {"message", "Thread did not stop gracefully within timeout, abandoning (may leak)"
            }
            }).dump() << std::endl;
            std::cout.flush();
        }
    }

    // Remove routing rules for this instance
    removeRoutingRulesForInstance(instance_id);
    
    // Clean up route monitoring snapshot
    last_route_snapshots_.erase(instance_id);
    
    {
        std::lock_guard<std::mutex> lock(instances_mutex_);
        auto it = instances_.find(instance_id);
        if (it != instances_.end()) {
            it->second.total_data_transferred.total_bytes += it->second.total_data_transferred.current_session_bytes;
            it->second.connection_time.total_seconds += it->second.connection_time.current_session_seconds;

            it->second.status = "Disconnected";
            it->second.thread_id = 0;
            it->second.wrapper_instance.reset();
            it->second.current_state = ConnectionState::DISCONNECTED;

            config_save_pending_.store(true);
        }
    }

    emitEvent(instance_id, "stopped", "Instance stopped with robust cleanup");

    std::cout << json({
        {"type", "shutdown_verbose"},
        {"step", "STOP_INSTANCE_COMPLETE"},
        {"instance_id", instance_id},
        {"wrapper_cleanup_success", wrapper_cleanup_success},
        {"thread_stop_success", thread_stop_success}
    }).dump() << std::endl;
    std::cout.flush();

    return true;
}

bool VPNInstanceManager::restartInstance(const std::string& instance_name) {
    stopInstance(instance_name);
    std::this_thread::sleep_for(std::chrono::seconds(2));
    return startInstance(instance_name);
}

bool VPNInstanceManager::enableInstance(const std::string& instance_name) {
    std::lock_guard<std::mutex> lock(instances_mutex_);

    auto it = instances_.find(instance_name);
    if (it == instances_.end()) {
        return false;
    }

    // If already enabled, just return success
    if (it->second.enabled) {
        emitEvent(instance_name, "enable", "Instance already enabled");
        return true;
    }

    // Set enabled to true
    it->second.enabled = true;
    config_save_pending_.store(true);

    // Start the instance
    it->second.status = "Connecting";
    it->second.last_used = std::to_string(time(nullptr));

    // Initialize connection metrics
    time_t now = time(nullptr);
    it->second.connection_time.current_session_start = now;
    it->second.connection_time.current_session_seconds = 0;
    it->second.data_transfer.upload_bytes = 0;
    it->second.data_transfer.download_bytes = 0;
    it->second.total_data_transferred.current_session_bytes = 0;

    launchInstanceThread(it->second);
    emitEvent(instance_name, "enabled", "Instance enabled and started");
    return true;
}

bool VPNInstanceManager::disableInstance(const std::string& instance_name) {
    // First check if instance exists and is enabled
    {
        std::lock_guard<std::mutex> lock(instances_mutex_);
        auto it = instances_.find(instance_name);
        if (it == instances_.end()) {
            return false;
        }

        // If already disabled, just return success
        if (!it->second.enabled) {
            emitEvent(instance_name, "disable", "Instance already disabled");
            return true;
        }

        // Set enabled to false
        it->second.enabled = false;
        config_save_pending_.store(true);
    }

    // Stop the instance (outside the lock to avoid deadlock)
    stopInstance(instance_name);
    emitEvent(instance_name, "disabled", "Instance disabled and stopped");
    return true;
}

bool VPNInstanceManager::startAllEnabled() {
    std::lock_guard<std::mutex> lock(instances_mutex_);

    int total_instances = instances_.size();
    int enabled_count = 0;

    if (verbose_) {
        json verbose_log;
        verbose_log["type"] = "verbose";
        verbose_log["message"] = "VPNInstanceManager::startAllEnabled - checking instances";
        verbose_log["total_instances"] = total_instances;
        std::cout << verbose_log.dump() << std::endl;
    }

    for (auto& [name, inst] : instances_) {
        if (inst.enabled) {
            enabled_count++;
            if (verbose_) {
                json verbose_log;
                verbose_log["type"] = "verbose";
                verbose_log["message"] = "Starting enabled instance";
                verbose_log["instance_name"] = name;
                std::cout << verbose_log.dump() << std::endl;
            }
            launchInstanceThread(inst);
            emitEvent(name, "started", "Instance started");
        } else {
            if (verbose_) {
                json verbose_log;
                verbose_log["type"] = "verbose";
                verbose_log["message"] = "Skipping disabled instance";
                verbose_log["instance_name"] = name;
                std::cout << verbose_log.dump() << std::endl;
            }
        }
    }

    if (verbose_) {
        json verbose_log;
        verbose_log["type"] = "verbose";
        verbose_log["message"] = "VPNInstanceManager::startAllEnabled - complete";
        verbose_log["total_instances"] = total_instances;
        verbose_log["enabled_instances"] = enabled_count;
        std::cout << verbose_log.dump() << std::endl;
    }

    // Always log if no instances were started
    if (enabled_count == 0) {
        json info_log;
        info_log["type"] = "info";
        info_log["message"] = "No VPN instances enabled for auto-start";
        info_log["total_instances"] = total_instances;
        info_log["hint"] = "Use HTTP API to enable/start instances or set 'auto_connect: true' in config";
        std::cout << info_log.dump() << std::endl;
    }

    return true;
}

bool VPNInstanceManager::stopAll() {
    std::cout << json({
        {"type", "shutdown_verbose"},
        {"step", "STOP_ALL_START"},
        {"message", "VPNInstanceManager::stopAll - Direct instance tracking and stopping (NO MUTEX)"}
    }).dump() << std::endl;
    std::cout.flush();

    running_ = false;

    // Step 1: Directly track all instances and their shutdown data (NO MUTEX - direct access)
    struct InstanceShutdownData {
        std::string name;
        VPNType type;
        std::shared_ptr<void> wrapper;
        unsigned int thread_id;
        std::string interface_name;
        bool should_stop;
    };

    std::vector<InstanceShutdownData> instances_to_stop;

    std::cout << json({
        {"type", "shutdown_verbose"},
        {"step", "DIRECT_INSTANCE_TRACKING"},
        {"message", "Tracking instances directly without mutex"}
    }).dump() << std::endl;
    std::cout.flush();

    // Direct access to instances_ map - no lock needed during shutdown
    for (auto& [name, inst] : instances_) {
        if (inst.thread_id > 0 || inst.wrapper_instance) {
            InstanceShutdownData data;
            data.name = name;
            data.type = inst.type;
            data.wrapper = inst.wrapper_instance;
            data.thread_id = inst.thread_id;
            data.should_stop = inst.should_stop.load();

            // Set interface name based on type
            if (inst.type == VPNType::WIREGUARD) {
                data.interface_name = "wg0";
            } else if (inst.type == VPNType::OPENVPN) {
                data.interface_name = "tun0";
            }

            // Immediately signal stop on the instance
            inst.should_stop = true;
            // Don't automatically set enabled to false - only via HTTP requests
            inst.status = "Disconnecting";

            instances_to_stop.push_back(data);

            std::cout << json({
                {"type", "shutdown_verbose"},
                {"step", "INSTANCE_TRACKED"},
                {"instance", name},
                {"vpn_type", vpnTypeToString(inst.type)},
                {"thread_id", inst.thread_id}
            }).dump() << std::endl;
            std::cout.flush();
        }
    }

    std::cout << json({
        {"type", "shutdown_verbose"},
        {"step", "TRACKED_COUNT"},
        {"total_instances", instances_to_stop.size()},
        {"message", "Starting direct shutdown of all instances"}
    }).dump() << std::endl;
    std::cout.flush();

    // Step 2: Process each instance - disconnect wrapper and stop thread
    for (auto& data : instances_to_stop) {
        std::cout << json({
            {"type", "shutdown_verbose"},
            {"step", "DIRECT_SHUTDOWN_START"},
            {"instance", data.name},
            {"vpn_type", vpnTypeToString(data.type)}
        }).dump() << std::endl;
        std::cout.flush();

        bool wrapper_cleanup_success = false;
        bool thread_stop_success = false;

        // Disconnect wrapper with timeout
        if (data.wrapper) {
            wrapper_cleanup_success = disconnectWrapperWithTimeout(data.wrapper, data.type, data.name, 5);

            if (wrapper_cleanup_success) {
                std::this_thread::sleep_for(std::chrono::milliseconds(800));
            } else {
                std::cout << json({
                    {"type", "shutdown_verbose"},
                    {"step", "FORCE_CLEANUP_NEEDED"},
                    {"instance", data.name}
                }).dump() << std::endl;
                std::cout.flush();

                if (!data.interface_name.empty()) {
                    forceCleanupNetworkInterface(data.interface_name, data.type);
                }
            }
        }

        // Stop thread with timeout
        if (data.thread_id > 0) {
            thread_stop_success = stopThreadWithTimeout(data.thread_id, data.name, 3);

            if (!thread_stop_success) {
                std::cout << json({
                    {"type", "shutdown_verbose"},
                    {"step", "THREAD_ABANDONED"},
                    {"instance", data.name},
                    {"thread_id", data.thread_id}
                }).dump() << std::endl;
                std::cout.flush();
            }
        }

        // Update instance state directly (no mutex)
        auto it = instances_.find(data.name);
        if (it != instances_.end()) {
            it->second.total_data_transferred.total_bytes += it->second.total_data_transferred.current_session_bytes;
            it->second.connection_time.total_seconds += it->second.connection_time.current_session_seconds;
            it->second.status = "Disconnected";
            it->second.thread_id = 0;
            it->second.wrapper_instance.reset();
            it->second.current_state = ConnectionState::DISCONNECTED;
        }

        std::cout << json({
            {"type", "shutdown_verbose"},
            {"step", "DIRECT_SHUTDOWN_COMPLETE"},
            {"instance", data.name},
            {"wrapper_success", wrapper_cleanup_success},
            {"thread_success", thread_stop_success}
        }).dump() << std::endl;
        std::cout.flush();
    }

    std::cout << json({
        {"type", "shutdown_verbose"},
        {"step", "STOP_ALL_COMPLETE"},
        {"message", "All instances stopped via direct shutdown (no mutex blocking)"}
    }).dump() << std::endl;
    std::cout.flush();

    return true;
}

bool VPNInstanceManager::addInstance(const std::string& name, const std::string& vpn_type,
                                      const std::string& config_content, bool auto_start) {
    if (verbose_) {
        std::cout << json({
            {"type", "verbose"},
            {"message", "VPNInstanceManager::addInstance - Starting"},
            {"instance_name", name},
            {"vpn_type", vpn_type},
            {"auto_start", auto_start}
        }).dump() << std::endl;
    }

    {
        std::lock_guard<std::mutex> lock(instances_mutex_);

        // Check if instance already exists
        if (instances_.find(name) != instances_.end()) {
            std::cerr << "Instance " << name << " already exists" << std::endl;
            return false;
        }

        VPNInstance instance;
        instance.id = name;
        instance.name = name;
        instance.protocol = vpn_type.empty() ? "OpenVPN" : vpn_type;
        instance.type = parseVPNType(instance.protocol);
        instance.config_content = config_content;
        instance.enabled = auto_start;
        instance.auto_connect = true;
        instance.status = "Ready";
        instance.created_date = std::to_string(time(nullptr));
        instance.current_state = ConnectionState::INITIAL;
        instance.start_time = 0;
        instance.thread_id = 0;
        instance.should_stop = false;

        if (instance.type == VPNType::UNKNOWN) {
            std::cerr << "Unknown VPN type: " << vpn_type << std::endl;
            return false;
        }

        // Add to instances map
        instances_[name] = instance;

        if (verbose_) {
            std::cout << json({
                {"type", "verbose"},
                {"message", "VPNInstanceManager::addInstance - Instance added to map"},
                {"instance_name", name}
            }).dump() << std::endl;
        }
    } // Release mutex here before saving and launching thread


// Save configuration to disk (outside mutex lock)
    if (!config_file_path_.empty()) {
        if (!saveConfiguration(config_file_path_)) {
            std::cerr << "Failed to save configuration" << std::endl;
        }
    }

    if (!cache_file_path_.empty()) {
        if (!saveCachedData(cache_file_path_)) {
            std::cerr << "Failed to save cached data" << std::endl;
        }
    }

    // Start the instance if auto_start is true (outside mutex lock)
    if (auto_start) {
        if (verbose_) {
            std::cout << json({
                {"type", "verbose"},
                {"message", "VPNInstanceManager::addInstance - Launching thread"},
                {"instance_name", name}
            }).dump() << std::endl;
        }

        // Get a reference to the instance (need to lock again briefly)
        {
            std::lock_guard<std::mutex> lock(instances_mutex_);
            launchInstanceThread(instances_[name]);
        }

        emitEvent(name, "started", "Instance added and started");
    } else {
        emitEvent(name, "added", "Instance added");
    }

    if (verbose_) {
        std::cout << json({
            {"type", "verbose"},
            {"message", "VPNInstanceManager::addInstance - Completed"},
            {"instance_name", name}
        }).dump() << std::endl;
    }

    return true;
}
    bool VPNInstanceManager::deleteInstance(const std::string& instance_name) {
    std::lock_guard<std::mutex> lock(instances_mutex_);

    auto it = instances_.find(instance_name);
    if (it == instances_.end()) {
        std::cerr << "Instance " << instance_name << " not found" << std::endl;
        return false;
    }

    // Stop the instance thread if running
    it->second.should_stop = true;
    try {
        if (it->second.thread_id > 0) {
            thread_manager_->stopThreadByAttachment(instance_name);
        }
    } catch (const std::exception& e) {
        std::cerr << "Error stopping instance thread: " << e.what() << std::endl;
    }

    // Remove from instances map
    instances_.erase(it);

    // Save updated configuration to disk
    if (!config_file_path_.empty()) {
        if (!saveConfiguration(config_file_path_)) {
            std::cerr << "Failed to save configuration" << std::endl;
        }
    }

    if (!cache_file_path_.empty()) {
        if (!saveCachedData(cache_file_path_)) {
            std::cerr << "Failed to save cached data" << std::endl;
        }
    }

    emitEvent(instance_name, "deleted", "Instance deleted");
    return true;
}

bool VPNInstanceManager::updateInstance(const std::string& instance_name,
                                         const std::string& config_content) {
    std::lock_guard<std::mutex> lock(instances_mutex_);

    auto it = instances_.find(instance_name);
    if (it == instances_.end()) {
        std::cerr << "Instance " << instance_name << " not found" << std::endl;
        return false;
    }

    bool was_enabled = it->second.enabled;

    // Stop the instance thread if running
    it->second.should_stop = true;
    try {
        if (it->second.thread_id > 0) {
            thread_manager_->stopThreadByAttachment(instance_name);
        }
    } catch (const std::exception& e) {
        std::cerr << "Error stopping instance thread: " << e.what() << std::endl;
    }

    // Update configuration
    it->second.config_content = config_content;
    it->second.last_used = std::to_string(time(nullptr));
    it->second.should_stop = false;
    it->second.thread_id = 0;

    // Save updated configuration to disk
    if (!config_file_path_.empty()) {
        if (!saveConfiguration(config_file_path_)) {
            std::cerr << "Failed to save configuration" << std::endl;
        }
    }

    if (!cache_file_path_.empty()) {
        if (!saveCachedData(cache_file_path_)) {
            std::cerr << "Failed to save cached data" << std::endl;
        }
    }

    // Restart the instance if it was previously enabled
    if (was_enabled) {
        launchInstanceThread(it->second);
        emitEvent(instance_name, "updated", "Instance updated and restarted");
    } else {
        emitEvent(instance_name, "updated", "Instance configuration updated");
    }

    return true;
}

} // namespace vpn_manager
