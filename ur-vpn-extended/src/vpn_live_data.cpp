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
    
    if (verbose_) {
        std::cout << json({
            {"type", "verbose"},
            {"message", "Live data collector thread started"}
        }).dump() << std::endl;
    }
    
    while (!shouldStop_.load()) {
        try {
            // Collect live data from all instances
            auto liveData = collectLiveData();
            
            // Publish to MQTT topic
            publishLiveData(liveData);
            
            // Sleep for configured interval
            auto sleepTime = std::chrono::milliseconds(publishIntervalMs_);
            auto start = std::chrono::steady_clock::now();
            
            while (!shouldStop_.load() && 
                   (std::chrono::steady_clock::now() - start) < sleepTime) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            
        } catch (const std::exception& e) {
            std::cout << json({
                {"type", "error"},
                {"message", "Exception in live data collector"},
                {"error", e.what()}
            }).dump() << std::endl;
            
            // Continue running despite errors
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
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
    std::vector<VpnLiveData> data;
    
    if (verbose_) {
        std::cout << json({
            {"type", "verbose"},
            {"message", "collectLiveData - starting"}
        }).dump() << std::endl;
    }
    
    try {
        if (verbose_) {
            std::cout << json({
                {"type", "verbose"},
                {"message", "collectLiveData - about to get instances"}
            }).dump() << std::endl;
        }
        
        // Get instances from the instance manager using a public method
        auto instances = instanceManager_.getAllInstancesForLiveData(); // Returns pointers
        
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
            
            // Collect protocol-specific data
            if (instance->type == VPNType::WIREGUARD) {
                try {
                    instanceData = collectWireGuardData(*instance);
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
                    instanceData = collectOpenVpnData(*instance);
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
        rpcClient_.publishMessage(topic, publishMessage.dump());
        
        if (verbose_) {
            std::cout << json({
                {"type", "verbose"},
                {"message", "Live data published"},
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

} // namespace vpn_manager
