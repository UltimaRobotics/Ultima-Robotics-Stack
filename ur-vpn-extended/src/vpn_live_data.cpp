#include "vpn_live_data.hpp"
#include "vpn_rpc_client.hpp"
#include "vpn_instance_manager.hpp"
#include "ThreadManager.hpp"
#include "wireguard_wrapper.hpp"
#include <nlohmann/json.hpp>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <ctime>

using json = nlohmann::json;

namespace vpn_manager {

std::string VpnLiveData::formatBytes(uint64_t bytes) {
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int unitIndex = 0;
    double size = static_cast<double>(bytes);
    
    while (size >= 1024.0 && unitIndex < 4) {
        size /= 1024.0;
        unitIndex++;
    }
    
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << size << " " << units[unitIndex];
    return oss.str();
}

std::string VpnLiveData::formatDuration(uint64_t seconds) {
    uint64_t hours = seconds / 3600;
    uint64_t minutes = (seconds % 3600) / 60;
    uint64_t secs = seconds % 60;
    
    std::ostringstream oss;
    oss << hours << "h " << minutes << "m " << secs << "s";
    return oss.str();
}

std::string VpnLiveData::formatTimestamp(const std::chrono::system_clock::time_point& tp) {
    auto time_t = std::chrono::system_clock::to_time_t(tp);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        tp.time_since_epoch()) % 1000;
    
    std::ostringstream oss;
    oss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%S");
    oss << '.' << std::setfill('0') << std::setw(3) << ms.count() << "Z";
    return oss.str();
}

void VpnLiveData::updateTimestamp() {
    last_update_time = std::chrono::system_clock::now();
    last_update_timestamp = formatTimestamp(last_update_time);
    update_sequence_number++;
}

std::string VpnLiveData::toJson() const {
    json j;
    
    // Basic information
    j["instance_id"] = instance_id;
    j["instance_name"] = instance_name;
    j["vpn_type"] = vpn_type;
    j["status"] = status;
    
    // Connection metrics
    j["connection"]["session_duration_seconds"] = connection.session_duration_seconds;
    j["connection"]["session_duration_formatted"] = connection.session_duration_formatted;
    j["connection"]["last_handshake_time"] = connection.last_handshake_time;
    j["connection"]["total_connection_time"] = connection.total_connection_time;
    j["connection"]["local_ip"] = connection.local_ip;
    j["connection"]["remote_endpoint"] = connection.remote_endpoint;
    j["connection"]["latency_ms"] = connection.latency_ms;
    
    // Data transfer metrics
    j["data_transfer"]["upload_bytes"] = data_transfer.upload_bytes;
    j["data_transfer"]["download_bytes"] = data_transfer.download_bytes;
    j["data_transfer"]["upload_rate_bps"] = data_transfer.upload_rate_bps;
    j["data_transfer"]["download_rate_bps"] = data_transfer.download_rate_bps;
    j["data_transfer"]["upload_formatted"] = data_transfer.upload_formatted;
    j["data_transfer"]["download_formatted"] = data_transfer.download_formatted;
    j["data_transfer"]["upload_rate_formatted"] = data_transfer.upload_rate_formatted;
    j["data_transfer"]["download_rate_formatted"] = data_transfer.download_rate_formatted;
    j["data_transfer"]["total_session_bytes"] = data_transfer.total_session_bytes;
    j["data_transfer"]["total_session_mb"] = data_transfer.total_session_mb;
    
    // Timestamps
    j["last_update_timestamp"] = last_update_timestamp;
    j["update_sequence_number"] = update_sequence_number;
    
    return j.dump();
}

// VpnLiveDataCollector implementation
VpnLiveDataCollector::VpnLiveDataCollector(VpnRpcClient& rpcClient, 
                                          ThreadMgr::ThreadManager& threadManager,
                                          VPNInstanceManager& instanceManager,
                                          uint32_t publishIntervalMs)
    : rpcClient_(rpcClient)
    , threadManager_(threadManager)
    , instanceManager_(instanceManager)
    , publishIntervalMs_(publishIntervalMs)
    , verbose_(false) {
    
    // Register event callbacks for both OpenVPN and WireGuard instances
    registerOpenVPNEventCallbacks();
    registerWireGuardEventCallbacks();
}

VpnLiveDataCollector::~VpnLiveDataCollector() {
    stop();
}

bool VpnLiveDataCollector::start() {
    if (running_.load()) {
        std::cout << json({
            {"type", "warning"},
            {"message", "Live data collector already running"}
        }).dump() << std::endl;
        return true;
    }
    
    shouldStop_.store(false);
    
    // Create collector thread using ThreadManager
    collectorThreadId_ = threadManager_.createThread([this]() {
        this->collectorThreadFunc();
    });
    
    // Debug: Log thread creation result (non-verbose)
    std::cout << json({
        {"type", "debug"},
        {"message", "Live data collector thread creation attempted"},
        {"thread_id", collectorThreadId_}
    }).dump() << std::endl;
    
    if (collectorThreadId_ == 0) {
        std::cout << json({
            {"type", "error"},
            {"message", "Failed to create live data collector thread"}
        }).dump() << std::endl;
        return false;
    }
    
    // Wait a moment for thread to start
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    if (verbose_) {
        std::cout << json({
            {"type", "verbose"},
            {"message", "Live data collector started"},
            {"thread_id", collectorThreadId_},
            {"interval_ms", publishIntervalMs_}
        }).dump() << std::endl;
    }
    
    return true;
}

void VpnLiveDataCollector::stop() {
    if (!running_.load()) {
        return;
    }
    
    shouldStop_.store(true);
    
    if (verbose_) {
        std::cout << json({
            {"type", "verbose"},
            {"message", "Stopping live data collector"}
        }).dump() << std::endl;
    }
    
    // Thread will exit on next iteration
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    running_.store(false);
    
    if (verbose_) {
        std::cout << json({
            {"type", "verbose"},
            {"message", "Live data collector stopped"}
        }).dump() << std::endl;
    }
}

bool VpnLiveDataCollector::isRunning() const {
    return running_.load();
}

void VpnLiveDataCollector::setPublishInterval(uint32_t intervalMs) {
    publishIntervalMs_ = intervalMs;
    
    if (verbose_) {
        std::cout << json({
            {"type", "verbose"},
            {"message", "Live data publish interval updated"},
            {"interval_ms", intervalMs}
        }).dump() << std::endl;
    }
}

void VpnLiveDataCollector::setVerbose(bool verbose) {
    verbose_ = verbose;
}

void VpnLiveDataCollector::collectorThreadFunc() {
    running_.store(true);
    
    // Debug: Log that thread has started (non-verbose)
    std::cout << json({
        {"type", "debug"},
        {"message", "Live data collector thread function started"}
    }).dump() << std::endl;
    
    // Debug: Check shouldStop flag
    std::cout << json({
        {"type", "debug"},
        {"message", "shouldStop flag status"},
        {"should_stop", shouldStop_.load()}
    }).dump() << std::endl;
    
    if (verbose_) {
        std::cout << json({
            {"type", "verbose"},
            {"message", "Live data collector thread started"}
        }).dump() << std::endl;
    }
    
    // Debug: About to enter while loop
    std::cout << json({
        {"type", "debug"},
        {"message", "About to enter collection while loop"}
    }).dump() << std::endl;
    
    while (!shouldStop_.load()) {
        // Debug: Log that we're entering the collection loop (non-verbose)
        std::cout << json({
            {"type", "debug"},
            {"message", "Entering live data collection loop"}
        }).dump() << std::endl;
        
        try {
            // Debug: Log that collection cycle is starting (non-verbose)
            std::cout << json({
                {"type", "debug"},
                {"message", "Live data collection cycle starting"}
            }).dump() << std::endl;
            
            // Collect live data from all instances
            auto liveData = collectLiveData();
            
            // Debug: Log collection results (non-verbose)
            std::cout << json({
                {"type", "debug"},
                {"message", "Live data collected"},
                {"instance_count", liveData.size()}
            }).dump() << std::endl;
            
            // Publish to MQTT topic
            publishLiveData(liveData);
            
        } catch (const std::exception& e) {
            std::cout << json({
                {"type", "error"},
                {"message", "Exception in live data collection loop"},
                {"error", e.what()}
            }).dump() << std::endl;
            
            // Continue running despite errors - use cached data only
            try {
                std::vector<VpnLiveData> cached_data;
                std::lock_guard<std::mutex> lock(cachedDataMutex_);
                for (const auto& [name, data] : cachedLiveData_) {
                    cached_data.push_back(data);
                }
                
                if (!cached_data.empty()) {
                    publishLiveData(cached_data);
                }
            } catch (...) {
                // Even cached data failed - skip this cycle
                std::cout << json({
                    {"type", "error"},
                    {"message", "Failed to publish cached data - skipping cycle"}
                }).dump() << std::endl;
            }
        } catch (...) {
            std::cout << json({
                {"type", "error"},
                {"message", "Unknown exception in live data collection - recovering"}
            }).dump() << std::endl;
        }
        
        // Sleep for configured interval
        auto sleepTime = std::chrono::milliseconds(publishIntervalMs_);
        auto start = std::chrono::steady_clock::now();
        
        while (!shouldStop_.load() && 
               (std::chrono::steady_clock::now() - start) < sleepTime) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
    
    running_.store(false);
    
    if (verbose_) {
        std::cout << json({
            {"type", "verbose"},
            {"message", "Live data collector thread exited"}
        }).dump() << std::endl;
    }
}

std::vector<VpnLiveData> VpnLiveDataCollector::collectLiveData() {
    // Debug: Log entry into collectLiveData (non-verbose)
    std::cout << json({
        {"type", "debug"},
        {"message", "collectLiveData function called"}
    }).dump() << std::endl;
    
    if (!running_.load()) {
        std::cout << json({
            {"type", "debug"},
            {"message", "collectLiveData returning early - not running"}
        }).dump() << std::endl;
        return {};
    }
    
    // Initialize result vector
        std::vector<VpnLiveData> data;
        
    try {
        if (verbose_) {
            std::cout << json({
                {"type", "verbose"},
                {"message", "collectLiveData - starting"}
            }).dump() << std::endl;
        }
        
        // Re-register event callbacks for any new instances
        // Add safety check to prevent crashes during registration
        std::cout << json({
            {"type", "debug"},
            {"message", "About to register event callbacks"}
        }).dump() << std::endl;
        
        try {
            // Only register events if system is stable (no ongoing connection issues)
            static int failed_registrations = 0;
            const int max_failures = 3;
            
            if (failed_registrations < max_failures) {
                std::cout << json({
                    {"type", "debug"},
                    {"message", "Registering OpenVPN and WireGuard callbacks"}
                }).dump() << std::endl;
                
                registerOpenVPNEventCallbacks();
                registerWireGuardEventCallbacks();
                failed_registrations = 0; // Reset on success
                
                std::cout << json({
                    {"type", "debug"},
                    {"message", "Event callbacks registered successfully"}
                }).dump() << std::endl;
            } else {
                if (verbose_) {
                    std::cout << json({
                        {"type", "warning"},
                        {"message", "Skipping event registration due to previous failures"},
                        {"failed_count", failed_registrations}
                    }).dump() << std::endl;
                }
            }
        } catch (const std::exception& e) {
            static int registration_failures = 0;
            registration_failures++;
            
            std::cout << json({
                {"type", "warning"},
                {"message", "Failed to register event callbacks - continuing with cached data"},
                {"error", e.what()},
                {"failure_count", registration_failures}
            }).dump() << std::endl;
            
            // Don't try to register events again if we keep failing
            if (registration_failures > 3) {
                std::cout << json({
                    {"type", "warning"},
                    {"message", "Too many registration failures - disabling event registration temporarily"}
                }).dump() << std::endl;
            }
        }
        
        // Get instances from the instance manager using a public method
        std::cout << json({
            {"type", "debug"},
            {"message", "About to get instances from manager"}
        }).dump() << std::endl;
        
        std::vector<const VPNInstance*> instances;
        try {
            instances = instanceManager_.getAllInstancesForLiveData(); // Returns pointers
            
            std::cout << json({
                {"type", "debug"},
                {"message", "Got instances from manager"},
                {"count", instances.size()}
            }).dump() << std::endl;
            
        } catch (const std::exception& e) {
            std::cout << json({
                {"type", "error"},
                {"message", "Failed to get instances from manager - using cached data only"},
                {"error", e.what()}
            }).dump() << std::endl;
            
            // Publish only cached data if available
            std::vector<VpnLiveData> live_data;
            std::lock_guard<std::mutex> lock(cachedDataMutex_);
            for (const auto& [name, data] : cachedLiveData_) {
                live_data.push_back(data);
            }
            
            if (!live_data.empty()) {
                publishLiveData(live_data);
            }
            return {};
        }
        
        if (verbose_) {
            std::cout << json({
                {"type", "verbose"},
                {"message", "Live data collection - got instances"},
                {"instance_count", instances.size()}
            }).dump() << std::endl;
        }
        
        if (verbose_) {
            std::cout << json({
                {"type", "verbose"},
                {"message", "About to start processing instances loop"},
                {"instances_size", instances.size()}
            }).dump() << std::endl;
        }
        
        for (const auto* instance : instances) {
            std::cout << json({
                {"type", "debug"},
                {"message", "Processing instance"},
                {"instance_name", instance ? instance->name : "null"}
            }).dump() << std::endl;
            
            if (!instance) {
                continue; // Skip null instances
            }
            
            try {
                VpnLiveData instanceData;
                
                // Basic information
                try {
                    instanceData.instance_id = instance->id;
                    instanceData.instance_name = instance->name;
                    instanceData.vpn_type = (instance->type == VPNType::WIREGUARD) ? "wireguard" : "openvpn";
                    instanceData.status = instance->status;
                } catch (const std::exception& e) {
                    if (verbose_) {
                        std::cout << json({
                            {"type", "error"},
                            {"message", "Failed to set basic info for instance"},
                            {"error", e.what()}
                        }).dump() << std::endl;
                    }
                    continue;
                }
            
            // Collect protocol-specific data using event-driven approach
            if (instance->type == VPNType::WIREGUARD) {
                try {
                    // Use cached event-driven data for WireGuard instances
                    std::lock_guard<std::mutex> lock(cachedDataMutex_);
                    auto it = cachedLiveData_.find(instance->name);
                    if (it != cachedLiveData_.end()) {
                        instanceData = it->second;
                    } else {
                        // Fallback to traditional collection if no cached data
                        instanceData = collectWireGuardData(*instance);
                        // Cache the fallback data
                        cachedLiveData_[instance->name] = instanceData;
                    }
                } catch (const std::exception& e) {
                    if (verbose_) {
                        std::cout << json({
                            {"type", "error"},
                            {"message", "Failed to collect WireGuard data"},
                            {"error", e.what()}
                        }).dump() << std::endl;
                    }
                    continue;
                }
            } else if (instance->type == VPNType::OPENVPN) {
                try {
                    // Use cached event-driven data for OpenVPN instances
                    std::lock_guard<std::mutex> lock(cachedDataMutex_);
                    auto it = cachedLiveData_.find(instance->name);
                    if (it != cachedLiveData_.end()) {
                        instanceData = it->second;
                    } else {
                        // Fallback to traditional collection if no cached data
                        instanceData = collectOpenVpnData(*instance);
                        // Cache the fallback data
                        cachedLiveData_[instance->name] = instanceData;
                    }
                } catch (const std::exception& e) {
                    if (verbose_) {
                        std::cout << json({
                            {"type", "error"},
                            {"message", "Failed to collect OpenVPN data"},
                            {"error", e.what()}
                        }).dump() << std::endl;
                    }
                    continue;
                }
            }
            
            // Update timestamp
            instanceData.updateTimestamp();
            
            data.push_back(instanceData);
            } catch (const std::exception& e) {
                if (verbose_) {
                    std::cout << json({
                        {"type", "error"},
                        {"message", "Failed to process instance"},
                        {"error", e.what()}
                    }).dump() << std::endl;
                }
                continue;
            }
        }
        
        if (verbose_) {
            std::cout << json({
                {"type", "verbose"},
                {"message", "Live data collection - completed"},
                {"collected_instances", data.size()}
            }).dump() << std::endl;
        }
        
    } catch (const std::exception& e) {
        std::cout << json({
            {"type", "error"},
            {"message", "Failed to collect live data"},
            {"error", e.what()}
        }).dump() << std::endl;
    }
    
    return data;
}

VpnLiveData VpnLiveDataCollector::collectWireGuardData(const VPNInstance& instance) {
    VpnLiveData data;
    
    // Basic info - with null checks
    data.instance_id = !instance.id.empty() ? instance.id : "unknown";
    data.instance_name = !instance.name.empty() ? instance.name : "unknown";
    data.vpn_type = "wireguard";
    data.status = !instance.status.empty() ? instance.status : "unknown";
    
    // Connection metrics
    data.connection.session_duration_seconds = instance.connection_time.current_session_seconds;
    data.connection.session_duration_formatted = VpnLiveData::formatDuration(data.connection.session_duration_seconds);
    data.connection.total_connection_time = instance.connection_time.total_seconds;
    data.connection.local_ip = ""; // Get from WireGuard wrapper if available
    data.connection.remote_endpoint = ""; // Get from WireGuard wrapper if available
    
    // Data transfer metrics
    data.data_transfer.upload_bytes = instance.data_transfer.upload_bytes;
    data.data_transfer.download_bytes = instance.data_transfer.download_bytes;
    data.data_transfer.upload_formatted = VpnLiveData::formatBytes(data.data_transfer.upload_bytes);
    data.data_transfer.download_formatted = VpnLiveData::formatBytes(data.data_transfer.download_bytes);
    data.data_transfer.total_session_bytes = instance.total_data_transferred.current_session_bytes;
    data.data_transfer.total_session_mb = data.data_transfer.total_session_bytes / (1024.0 * 1024.0);
    
    // Get real-time stats from WireGuard wrapper if available
    if (instance.wrapper_instance) {
        try {
            // Cast to WireGuard wrapper and get current stats
            auto wg_wrapper = std::static_pointer_cast<wireguard::WireGuardWrapper>(instance.wrapper_instance);
            auto wgStats = wg_wrapper->getStats();
            
            // Update data transfer metrics with real-time values
            data.data_transfer.upload_bytes = wgStats.bytes_sent;
            data.data_transfer.download_bytes = wgStats.bytes_received;
            data.data_transfer.upload_formatted = VpnLiveData::formatBytes(data.data_transfer.upload_bytes);
            data.data_transfer.download_formatted = VpnLiveData::formatBytes(data.data_transfer.download_bytes);
            data.data_transfer.upload_rate_bps = wgStats.upload_rate_bps;
            data.data_transfer.download_rate_bps = wgStats.download_rate_bps;
            data.data_transfer.upload_rate_formatted = VpnLiveData::formatBytes(data.data_transfer.upload_rate_bps) + "/s";
            data.data_transfer.download_rate_formatted = VpnLiveData::formatBytes(data.data_transfer.download_rate_bps) + "/s";
            
            // Update connection metrics
            data.connection.latency_ms = wgStats.latency_ms;
            data.connection.local_ip = wgStats.local_ip;
            data.connection.remote_endpoint = wgStats.endpoint;
            data.connection.last_handshake_time = (wgStats.last_handshake > 0) ? 
                std::to_string(wgStats.last_handshake) : "";
            
            // Update protocol-specific metrics
            data.protocol.peer_public_key = wgStats.peer_public_key;
            data.protocol.allowed_ips = wgStats.allowed_ips;
            data.protocol.interface_name = wgStats.interface_name;
            data.protocol.routes_json = wgStats.routes;
            data.protocol.tx_packets = wgStats.tx_packets;
            data.protocol.rx_packets = wgStats.rx_packets;
            
            if (verbose_) {
                std::cout << json({
                    {"type", "verbose"},
                    {"message", "Updated WireGuard data with real-time stats"},
                    {"instance", instance.name},
                    {"upload_bytes", data.data_transfer.upload_bytes},
                    {"download_bytes", data.data_transfer.download_bytes}
                }).dump() << std::endl;
            }
            
        } catch (const std::exception& e) {
            if (verbose_) {
                std::cout << json({
                    {"type", "warning"},
                    {"message", "Failed to get real-time WireGuard stats"},
                    {"instance", instance.name},
                    {"error", e.what()}
                }).dump() << std::endl;
            }
        }
    }
    
    return data;
}

VpnLiveData VpnLiveDataCollector::collectOpenVpnData(const VPNInstance& instance) {
    if (verbose_) {
        std::cout << json({
            {"type", "verbose"},
            {"message", "collectOpenVpnData - starting"},
            {"instance_name", instance.name}
        }).dump() << std::endl;
    }
    
    VpnLiveData data;
    
    // Basic info - with null checks
    data.instance_id = !instance.id.empty() ? instance.id : "unknown";
    data.instance_name = !instance.name.empty() ? instance.name : "unknown";
    data.vpn_type = "openvpn";
    data.status = !instance.status.empty() ? instance.status : "unknown";
    
    if (verbose_) {
        std::cout << json({
            {"type", "verbose"},
            {"message", "collectOpenVpnData - basic info set"},
            {"data_instance_id", data.instance_id},
            {"data_instance_name", data.instance_name}
        }).dump() << std::endl;
    }
    
    // Connection metrics
    data.connection.session_duration_seconds = instance.connection_time.current_session_seconds;
    data.connection.session_duration_formatted = VpnLiveData::formatDuration(data.connection.session_duration_seconds);
    data.connection.total_connection_time = instance.connection_time.total_seconds;
    
    if (verbose_) {
        std::cout << json({
            {"type", "verbose"},
            {"message", "collectOpenVpnData - connection metrics set"}
        }).dump() << std::endl;
    }
    
    // Data transfer metrics
    data.data_transfer.upload_bytes = instance.data_transfer.upload_bytes;
    data.data_transfer.download_bytes = instance.data_transfer.download_bytes;
    data.data_transfer.upload_formatted = VpnLiveData::formatBytes(data.data_transfer.upload_bytes);
    data.data_transfer.download_formatted = VpnLiveData::formatBytes(data.data_transfer.download_bytes);
    data.data_transfer.total_session_bytes = instance.total_data_transferred.current_session_bytes;
    data.data_transfer.total_session_mb = data.data_transfer.total_session_bytes / (1024.0 * 1024.0);
    
    if (verbose_) {
        std::cout << json({
            {"type", "verbose"},
            {"message", "collectOpenVpnData - data transfer metrics set"}
        }).dump() << std::endl;
    }
    
    // Get real-time stats from OpenVPN wrapper if connected
    if (instance.wrapper_instance && instance.status == "connected") {
        try {
            // This would need to be implemented in OpenVPNWrapper
            // to get current stats including rates
            // auto stats = instance.wrapper_instance->getCurrentStats();
            // data.data_transfer.upload_rate_bps = stats.upload_rate_bps;
            // data.data_transfer.download_rate_bps = stats.download_rate_bps;
            // data.protocol.cipher = stats.cipher;
            // etc.
        } catch (const std::exception& e) {
            if (verbose_) {
                std::cout << json({
                    {"type", "warning"},
                    {"message", "Failed to get real-time OpenVPN stats"},
                    {"instance", instance.name},
                    {"error", e.what()}
                }).dump() << std::endl;
            }
        }
    }
    
    return data;
}

void VpnLiveDataCollector::publishLiveData(const std::vector<VpnLiveData>& data) {
    // Debug: Log entry into publishLiveData (non-verbose)
    std::cout << json({
        {"type", "debug"},
        {"message", "publishLiveData function called"},
        {"instance_count", data.size()}
    }).dump() << std::endl;
    
    try {
        json publishMessage;
        publishMessage["type"] = "live_data";
        publishMessage["source"] = "ur-vpn-manager";
        publishMessage["timestamp"] = VpnLiveData::formatTimestamp(std::chrono::system_clock::now());
        publishMessage["sequence_number"] = sequenceCounter_.fetch_add(1);
        publishMessage["instance_count"] = data.size();
        publishMessage["instances"] = json::array();
        
        for (const auto& instance : data) {
            try {
                std::string instanceJsonStr = instance.toJson();
                if (verbose_) {
                    std::cout << json({
                        {"type", "verbose"},
                        {"message", "Generated instance JSON"},
                        {"instance", instance.instance_name},
                        {"json", instanceJsonStr}
                    }).dump() << std::endl;
                }
                // Parse the JSON string to ensure it's valid before adding
                json instanceJson = json::parse(instanceJsonStr);
                publishMessage["instances"].push_back(instanceJson);
            } catch (const std::exception& e) {
                if (verbose_) {
                    std::cout << json({
                        {"type", "error"},
                        {"message", "Failed to parse instance JSON for publishing"},
                        {"instance", instance.instance_name},
                        {"error", e.what()}
                    }).dump() << std::endl;
                }
                // Skip this instance but continue with others
                continue;
            }
        }
        
        // Publish using the existing RPC client
        std::string topic = "ur-shared-bus/ur-mavlink-stack/ur-vpn-manager/live-data";
        
        // Debug: Check if RPC client is running before publishing
        std::cout << json({
            {"type", "debug"},
            {"message", "Checking RPC client status"}
        }).dump() << std::endl;
        
        if (!rpcClient_.isRunning()) {
            std::cout << json({
                {"type", "error"},
                {"message", "RPC client not running - cannot publish live data"},
                {"topic", topic}
            }).dump() << std::endl;
            return;
        }
        
        std::cout << json({
            {"type", "debug"},
            {"message", "RPC client is running - preparing to publish"}
        }).dump() << std::endl;
        
        std::string messageStr = publishMessage.dump();
        
        // Debug: Show the actual message being published
        std::cout << json({
            {"type", "debug"},
            {"message", "Publishing live data message"},
            {"topic", topic},
            {"message_size", messageStr.length()},
            {"message_preview", messageStr.substr(0, 200) + (messageStr.length() > 200 ? "..." : "")}
        }).dump() << std::endl;
        
        if (verbose_) {
            std::cout << json({
                {"type", "verbose"},
                {"message", "About to publish live data"},
                {"topic", topic},
                {"message_size", messageStr.length()},
                {"instance_count", data.size()}
            }).dump() << std::endl;
        }
        
        std::cout << json({
            {"type", "debug"},
            {"message", "About to call RPC client publishMessage"}
        }).dump() << std::endl;
        
        rpcClient_.publishMessage(topic, messageStr);
        
        std::cout << json({
            {"type", "debug"},
            {"message", "RPC client publishMessage returned"}
        }).dump() << std::endl;
        
        if (verbose_) {
            std::cout << json({
                {"type", "verbose"},
                {"message", "Live data published successfully"},
                {"topic", topic},
                {"instance_count", data.size()},
                {"sequence", publishMessage["sequence_number"]}
            }).dump() << std::endl;
        }
        
    } catch (const std::exception& e) {
        std::cout << json({
            {"type", "error"},
            {"message", "Failed to publish live data"},
            {"error", e.what()}
        }).dump() << std::endl;
    }
}

VPNInstanceManager& VpnLiveDataCollector::getInstanceManager() {
    return instanceManager_;
}

void VpnLiveDataCollector::registerOpenVPNEventCallbacks() {
    try {
        // Get all current instances and register callbacks for OpenVPN ones
        auto instances = getInstanceManager().getAllInstancesForLiveData();
        
        for (const auto* instance : instances) {
            if (!instance || instance->type != VPNType::OPENVPN) continue;
            
            // Safety check: ensure instance name is valid
            if (instance->name.empty()) {
                if (verbose_) {
                    std::cout << json({
                        {"type", "warning"},
                        {"message", "Skipping OpenVPN instance with empty name"}
                    }).dump() << std::endl;
                }
                continue;
            }
            
            if (instance->wrapper_instance) {
                try {
                    auto ovpn_wrapper = std::static_pointer_cast<openvpn::OpenVPNWrapper>(instance->wrapper_instance);
                    
                    // Safety check: ensure wrapper is valid
                    if (!ovpn_wrapper) {
                        if (verbose_) {
                            std::cout << json({
                                {"type", "warning"},
                                {"message", "Invalid OpenVPN wrapper for instance"},
                                {"instance", instance->name}
                            }).dump() << std::endl;
                        }
                        continue;
                    }
                    
                    // Register event callback
                    auto event_callback = [this, instance_name = instance->name](const openvpn::VPNEvent& event) {
                        try {
                            // Additional safety check in callback
                            if (instance_name.empty()) return;
                            this->onOpenVPNEvent(instance_name, event);
                        } catch (const std::exception& e) {
                            std::cout << json({
                                {"type", "error"},
                                {"message", "Exception in OpenVPN event callback"},
                                {"instance", instance_name},
                                {"error", e.what()}
                            }).dump() << std::endl;
                        }
                    };
                    
                    ovpn_wrapper->setEventCallback(event_callback);
                    
                    // Initialize cached data for already connected instances
                    if (instance->status == "connected") {
                        // Create a synthetic "connected" event to initialize data
                        openvpn::VPNEvent init_event;
                        init_event.type = "connected";
                        init_event.message = "Initializing cached data for connected instance";
                        init_event.state = openvpn::ConnectionState::CONNECTED;
                        init_event.timestamp = time(nullptr);
                        
                        updateOpenVPNConnectionData(instance->name, init_event);
                        
                        if (verbose_) {
                            std::cout << json({
                                {"type", "verbose"},
                                {"message", "Initialized cached data for connected OpenVPN instance"},
                                {"instance", instance->name}
                            }).dump() << std::endl;
                        }
                    }
                    
                    if (verbose_) {
                        std::cout << json({
                            {"type", "verbose"},
                            {"message", "Registered OpenVPN event callback"},
                            {"instance", instance->name}
                        }).dump() << std::endl;
                    }
                    
                } catch (const std::exception& e) {
                    std::cout << json({
                        {"type", "error"},
                        {"message", "Failed to register OpenVPN callback for instance"},
                        {"instance", instance->name},
                        {"error", e.what()}
                    }).dump() << std::endl;
                }
            }
        }
    } catch (const std::exception& e) {
        std::cout << json({
            {"type", "error"},
            {"message", "Failed to register OpenVPN event callbacks"},
            {"error", e.what()}
        }).dump() << std::endl;
    }
}

void VpnLiveDataCollector::onOpenVPNEvent(const std::string& instance_name, const openvpn::VPNEvent& event) {
    try {
        // Safety checks
        if (instance_name.empty()) {
            if (verbose_) {
                std::cout << json({
                    {"type", "warning"},
                    {"message", "Received OpenVPN event with empty instance name"}
                }).dump() << std::endl;
            }
            return;
        }
        
        if (verbose_) {
            std::cout << json({
                {"type", "verbose"},
                {"message", "Received OpenVPN event"},
                {"instance", instance_name},
                {"event_type", event.type},
                {"event_message", event.message}
            }).dump() << std::endl;
        }
        
        // Update cached connection data based on event
        updateOpenVPNConnectionData(instance_name, event);
        
        // Trigger immediate live data publish for important events
        if (event.type == "connected" || event.type == "disconnected" || 
            event.type == "state_change" || event.type == "error") {
            
            // Create live data vector with cached data
            std::vector<VpnLiveData> live_data;
            
            std::lock_guard<std::mutex> lock(cachedDataMutex_);
            for (const auto& [name, data] : cachedLiveData_) {
                live_data.push_back(data);
            }
            
            // Publish immediately
            publishLiveData(live_data);
        }
        
    } catch (const std::exception& e) {
        std::cout << json({
            {"type", "error"},
            {"message", "Failed to handle OpenVPN event"},
            {"instance", instance_name},
            {"error", e.what()}
        }).dump() << std::endl;
    } catch (...) {
        std::cout << json({
            {"type", "error"},
            {"message", "Unknown exception in OpenVPN event handler"},
            {"instance", instance_name}
        }).dump() << std::endl;
    }
}

void VpnLiveDataCollector::updateOpenVPNConnectionData(const std::string& instance_name, const openvpn::VPNEvent& event) {
    std::lock_guard<std::mutex> lock(cachedDataMutex_);
    
    // Get or create cached data for this instance
    VpnLiveData& data = cachedLiveData_[instance_name];
    data.instance_id = instance_name;
    data.instance_name = instance_name;
    data.vpn_type = "openvpn";
    data.updateTimestamp();
    data.update_sequence_number++;
    
    // Update connection metrics based on event type
    if (event.type == "connected") {
        data.status = "Connected";
        data.connection.last_handshake_time = std::to_string(event.timestamp);
        
        // Extract real connection data from OpenVPN bridge stats
        try {
            // Get a snapshot of instances to avoid threading issues
            auto instances = getInstanceManager().getAllInstancesForLiveData();
            const VPNInstance* target_instance = nullptr;
            
            // Find the target instance first
            for (const auto* instance : instances) {
                if (instance && instance->name == instance_name) {
                    target_instance = instance;
                    break;
                }
            }
            
            // Only proceed if we found the instance and it has a wrapper
            if (target_instance && target_instance->wrapper_instance) {
                auto ovpn_wrapper = std::static_pointer_cast<openvpn::OpenVPNWrapper>(target_instance->wrapper_instance);
                
                // Get real-time stats from OpenVPN bridge
                auto stats = ovpn_wrapper->getStats();
                
                // Update connection metrics with real data
                data.connection.local_ip = stats.local_ip;
                data.connection.remote_endpoint = stats.server_ip;
                data.connection.latency_ms = stats.ping_ms;
                
                // Calculate session duration from connection time
                if (stats.connected_since > 0) {
                    auto now = std::chrono::system_clock::now();
                    auto connection_time = std::chrono::system_clock::from_time_t(stats.connected_since);
                    auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - connection_time);
                    data.connection.session_duration_seconds = duration.count();
                    data.connection.session_duration_formatted = VpnLiveData::formatDuration(duration.count());
                }
                
                // Update data transfer metrics with real data
                data.data_transfer.upload_bytes = stats.bytes_sent;
                data.data_transfer.download_bytes = stats.bytes_received;
                data.data_transfer.upload_formatted = VpnLiveData::formatBytes(stats.bytes_sent);
                data.data_transfer.download_formatted = VpnLiveData::formatBytes(stats.bytes_received);
                data.data_transfer.total_session_bytes = stats.bytes_sent + stats.bytes_received;
                data.data_transfer.total_session_mb = data.data_transfer.total_session_bytes / (1024.0 * 1024.0);
            }
        } catch (const std::exception& e) {
            if (verbose_) {
                std::cout << json({
                    {"type", "warning"},
                    {"message", "Failed to get real OpenVPN stats"},
                    {"instance", instance_name},
                    {"error", e.what()}
                }).dump() << std::endl;
            }
        }
        
    } else if (event.type == "disconnected") {
        data.status = "Disconnected";
        data.connection.session_duration_seconds = 0;
        data.connection.session_duration_formatted = "0h 0m 0s";
        data.connection.local_ip = "";
        data.connection.remote_endpoint = "";
        data.connection.last_handshake_time = "";
        data.connection.latency_ms = 0;
        
    } else if (event.type == "state_change") {
        // Update status based on new state
        if (event.data.contains("new_state")) {
            std::string new_state = event.data["new_state"];
            if (new_state == "CONNECTED") {
                data.status = "Connected";
                // Re-extract connection data when state changes to connected
                openvpn::VPNEvent reconnect_event;
                reconnect_event.type = "connected";
                reconnect_event.message = "State changed to connected";
                reconnect_event.state = openvpn::ConnectionState::CONNECTED;
                reconnect_event.timestamp = event.timestamp;
                reconnect_event.data = event.data;
                updateOpenVPNConnectionData(instance_name, reconnect_event);
            } else if (new_state == "DISCONNECTED") {
                data.status = "Disconnected";
            } else if (new_state == "CONNECTING") {
                data.status = "Connecting";
            } else if (new_state == "AUTHENTICATING") {
                data.status = "Authenticating";
            }
        }
        
    } else if (event.type == "error") {
        data.status = "Error";
    }
    
    if (verbose_) {
        std::cout << json({
            {"type", "verbose"},
            {"message", "Updated OpenVPN connection data from event"},
            {"instance", instance_name},
            {"status", data.status},
            {"local_ip", data.connection.local_ip},
            {"remote_endpoint", data.connection.remote_endpoint},
            {"session_duration", data.connection.session_duration_seconds},
            {"upload_bytes", data.data_transfer.upload_bytes},
            {"download_bytes", data.data_transfer.download_bytes}
        }).dump() << std::endl;
    }
}

void VpnLiveDataCollector::registerWireGuardEventCallbacks() {
    try {
        // Get all current instances and register callbacks for WireGuard ones
        auto instances = getInstanceManager().getAllInstancesForLiveData();
        
        for (const auto* instance : instances) {
            if (!instance || instance->type != VPNType::WIREGUARD) continue;
            
            // Safety check: ensure instance name is valid
            if (instance->name.empty()) {
                if (verbose_) {
                    std::cout << json({
                        {"type", "warning"},
                        {"message", "Skipping WireGuard instance with empty name"}
                    }).dump() << std::endl;
                }
                continue;
            }
            
            if (instance->wrapper_instance) {
                try {
                    auto wg_wrapper = std::static_pointer_cast<wireguard::WireGuardWrapper>(instance->wrapper_instance);
                    
                    // Safety check: ensure wrapper is valid
                    if (!wg_wrapper) {
                        if (verbose_) {
                            std::cout << json({
                                {"type", "warning"},
                                {"message", "Invalid WireGuard wrapper for instance"},
                                {"instance", instance->name}
                            }).dump() << std::endl;
                        }
                        continue;
                    }
                    
                    // Register event callback
                    auto event_callback = [this, instance_name = instance->name](const wireguard::VPNEvent& event) {
                        try {
                            // Additional safety check in callback
                            if (instance_name.empty()) return;
                            this->onWireGuardEvent(instance_name, event);
                        } catch (const std::exception& e) {
                            std::cout << json({
                                {"type", "error"},
                                {"message", "Exception in WireGuard event callback"},
                                {"instance", instance_name},
                                {"error", e.what()}
                            }).dump() << std::endl;
                        }
                    };
                    
                    wg_wrapper->setEventCallback(event_callback);
                    
                    // Initialize cached data for already connected instances
                    if (instance->status == "connected") {
                        // Create a synthetic "connected" event to initialize data
                        wireguard::VPNEvent init_event;
                        init_event.type = "connected";
                        init_event.message = "Initializing cached data for connected instance";
                        init_event.state = wireguard::ConnectionState::CONNECTED;
                        init_event.timestamp = time(nullptr);
                        
                        updateWireGuardConnectionData(instance->name, init_event);
                        
                        if (verbose_) {
                            std::cout << json({
                                {"type", "verbose"},
                                {"message", "Initialized cached data for connected WireGuard instance"},
                                {"instance", instance->name}
                            }).dump() << std::endl;
                        }
                    }
                    
                    if (verbose_) {
                        std::cout << json({
                            {"type", "verbose"},
                            {"message", "Registered WireGuard event callback"},
                            {"instance", instance->name}
                        }).dump() << std::endl;
                    }
                    
                } catch (const std::exception& e) {
                    std::cout << json({
                        {"type", "error"},
                        {"message", "Failed to register WireGuard callback for instance"},
                        {"instance", instance->name},
                        {"error", e.what()}
                    }).dump() << std::endl;
                }
            }
        }
    } catch (const std::exception& e) {
        std::cout << json({
            {"type", "error"},
            {"message", "Failed to register WireGuard event callbacks"},
            {"error", e.what()}
        }).dump() << std::endl;
    }
}

void VpnLiveDataCollector::onWireGuardEvent(const std::string& instance_name, const wireguard::VPNEvent& event) {
    try {
        if (verbose_) {
            std::cout << json({
                {"type", "verbose"},
                {"message", "Received WireGuard event"},
                {"instance", instance_name},
                {"event_type", event.type},
                {"event_message", event.message}
            }).dump() << std::endl;
        }
        
        // Update cached connection data based on event
        updateWireGuardConnectionData(instance_name, event);
        
        // Trigger immediate live data publish for important events
        if (event.type == "connected" || event.type == "disconnected" || 
            event.type == "handshaking" || event.type == "error" || event.type == "status") {
            
            // Create live data vector with cached data
            std::vector<VpnLiveData> live_data;
            
            std::lock_guard<std::mutex> lock(cachedDataMutex_);
            for (const auto& [name, data] : cachedLiveData_) {
                live_data.push_back(data);
            }
            
            // Publish immediately
            publishLiveData(live_data);
        }
        
    } catch (const std::exception& e) {
        std::cout << json({
            {"type", "error"},
            {"message", "Failed to handle WireGuard event"},
            {"instance", instance_name},
            {"error", e.what()}
        }).dump() << std::endl;
    }
}

void VpnLiveDataCollector::updateWireGuardConnectionData(const std::string& instance_name, const wireguard::VPNEvent& event) {
    std::lock_guard<std::mutex> lock(cachedDataMutex_);
    
    // Get or create cached data for this instance
    VpnLiveData& data = cachedLiveData_[instance_name];
    data.instance_id = instance_name;
    data.instance_name = instance_name;
    data.vpn_type = "wireguard";
    data.updateTimestamp();
    data.update_sequence_number++;
    
    // Update connection metrics based on event type
    if (event.type == "connected") {
        data.status = "Connected";
        data.connection.last_handshake_time = std::to_string(event.timestamp);
        
        // Extract real connection data from WireGuard stats
        try {
            // Get a snapshot of instances to avoid threading issues
            auto instances = getInstanceManager().getAllInstancesForLiveData();
            const VPNInstance* target_instance = nullptr;
            
            // Find the target instance first
            for (const auto* instance : instances) {
                if (instance && instance->name == instance_name) {
                    target_instance = instance;
                    break;
                }
            }
            
            // Only proceed if we found the instance and it has a wrapper
            if (target_instance && target_instance->wrapper_instance) {
                auto wg_wrapper = std::static_pointer_cast<wireguard::WireGuardWrapper>(target_instance->wrapper_instance);
                
                // Get real-time stats from WireGuard wrapper
                auto stats = wg_wrapper->getStats();
                
                // Update connection metrics with real data
                data.connection.local_ip = stats.local_ip;
                data.connection.remote_endpoint = stats.endpoint;
                data.connection.latency_ms = stats.latency_ms;
                
                // Calculate session duration from connection time
                if (stats.last_handshake > 0) {
                    auto now = std::chrono::system_clock::now();
                    auto connection_time = std::chrono::system_clock::from_time_t(stats.last_handshake);
                    auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - connection_time);
                    data.connection.session_duration_seconds = duration.count();
                    data.connection.session_duration_formatted = VpnLiveData::formatDuration(duration.count());
                }
                
                // Update data transfer metrics with real data
                data.data_transfer.upload_bytes = stats.bytes_sent;
                data.data_transfer.download_bytes = stats.bytes_received;
                data.data_transfer.upload_formatted = VpnLiveData::formatBytes(stats.bytes_sent);
                data.data_transfer.download_formatted = VpnLiveData::formatBytes(stats.bytes_received);
                data.data_transfer.total_session_bytes = stats.bytes_sent + stats.bytes_received;
                data.data_transfer.total_session_mb = data.data_transfer.total_session_bytes / (1024.0 * 1024.0);
            }
        } catch (const std::exception& e) {
            if (verbose_) {
                std::cout << json({
                    {"type", "warning"},
                    {"message", "Failed to get real WireGuard stats"},
                    {"instance", instance_name},
                    {"error", e.what()}
                }).dump() << std::endl;
            }
        }
        
    } else if (event.type == "disconnected") {
        data.status = "Disconnected";
        data.connection.session_duration_seconds = 0;
        data.connection.session_duration_formatted = "0h 0m 0s";
        data.connection.local_ip = "";
        data.connection.remote_endpoint = "";
        data.connection.last_handshake_time = "";
        data.connection.latency_ms = 0;
        
    } else if (event.type == "handshaking") {
        data.status = "Handshaking";
        
    } else if (event.type == "status") {
        // Update status based on event message
        if (event.message.find("CONNECTED") != std::string::npos) {
            data.status = "Connected";
            // Re-extract connection data when status changes to connected
            wireguard::VPNEvent reconnect_event;
            reconnect_event.type = "connected";
            reconnect_event.message = "Status changed to connected";
            reconnect_event.state = wireguard::ConnectionState::CONNECTED;
            reconnect_event.timestamp = event.timestamp;
            reconnect_event.data = event.data;
            updateWireGuardConnectionData(instance_name, reconnect_event);
        } else if (event.message.find("DISCONNECTED") != std::string::npos) {
            data.status = "Disconnected";
        } else if (event.message.find("CONNECTING") != std::string::npos) {
            data.status = "Connecting";
        } else if (event.message.find("HANDSHAKING") != std::string::npos) {
            data.status = "Handshaking";
        }
        
    } else if (event.type == "error") {
        data.status = "Error";
    } else if (event.type == "stats") {
        // Update statistics when stats event is received
        if (event.data.contains("upload_bytes")) {
            data.data_transfer.upload_bytes = event.data["upload_bytes"];
            data.data_transfer.upload_formatted = VpnLiveData::formatBytes(data.data_transfer.upload_bytes);
        }
        if (event.data.contains("download_bytes")) {
            data.data_transfer.download_bytes = event.data["download_bytes"];
            data.data_transfer.download_formatted = VpnLiveData::formatBytes(data.data_transfer.download_bytes);
        }
        if (event.data.contains("latency_ms")) {
            data.connection.latency_ms = event.data["latency_ms"];
        }
    }
    
    if (verbose_) {
        std::cout << json({
            {"type", "verbose"},
            {"message", "Updated WireGuard connection data from event"},
            {"instance", instance_name},
            {"status", data.status},
            {"local_ip", data.connection.local_ip},
            {"remote_endpoint", data.connection.remote_endpoint},
            {"session_duration", data.connection.session_duration_seconds},
            {"upload_bytes", data.data_transfer.upload_bytes},
            {"download_bytes", data.data_transfer.download_bytes}
        }).dump() << std::endl;
    }
}

} // namespace vpn_manager
