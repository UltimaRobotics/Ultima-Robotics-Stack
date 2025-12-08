
#include "../include/ServersStatusMonitor.hpp"
#include "../include/LoggingMacros.hpp"
#include "../ur-netbench-shared/include/ServerStatusSerializer.hpp"
#include "../ur-netbench-shared/include/ServerStatusProgressSerializer.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <ctime>
#include <sys/stat.h>
#include <unistd.h>
#include <queue>
#include <condition_variable>

using namespace NetBench::Shared;

ServersStatusMonitor::ServersStatusMonitor(const std::string& output_dir) 
    : monitoring_active_(false), 
      output_dir_(output_dir) {
    thread_manager_ = std::make_unique<ThreadMgr::ThreadManager>();

    // Create output directory hierarchy
    size_t pos = 0;
    std::string path;
    while ((pos = output_dir_.find('/', pos)) != std::string::npos) {
        path = output_dir_.substr(0, pos++);
        if (!path.empty()) {
            mkdir(path.c_str(), 0755);
        }
    }
    mkdir(output_dir_.c_str(), 0755);
}

ServersStatusMonitor::~ServersStatusMonitor() {
    stopMonitoring();
}

bool ServersStatusMonitor::loadServersConfig(const std::string& config_file_path) {
    std::ifstream file(config_file_path);
    if (!file.is_open()) {
        LOG_ERROR("[ServersStatus] Error: Cannot open config file: " << config_file_path << std::endl);
        return false;
    }

    try {
        json config;
        file >> config;

        if (!config.is_array()) {
            LOG_ERROR("[ServersStatus] Error: Config must be an array of servers" << std::endl);
            return false;
        }

        servers_.clear();
        int server_count = 0;

        for (const auto& server_json : config) {
            ServerInfo server;

            server.host = server_json.value("IP/HOST", "");

            if (server_json.contains("PORT")) {
                if (server_json["PORT"].is_string()) {
                    server.port = server_json["PORT"].get<std::string>();
                } else if (server_json["PORT"].is_number()) {
                    server.port = std::to_string(server_json["PORT"].get<int>());
                }
            }

            server.continent = server_json.value("CONTINENT", "");
            server.country = server_json.value("COUNTRY", "");
            server.site = server_json.value("SITE", "");
            server.provider = server_json.value("PROVIDER", "");

            if (server.host.empty()) {
                continue;
            }

            std::string clean_host = server.host;
            for (char& c : clean_host) {
                if (!isalnum(c) && c != '.') c = '_';
            }
            server.id = clean_host + "_" + std::to_string(server_count++);

            server.name = server.provider.empty() ? server.host : (server.provider + " - " + server.site);
            if (server.name.length() > 40) {
                server.name = server.name.substr(0, 37) + "...";
            }

            server.description = server.continent + "/" + server.country + "/" + server.site;

            // Fast ping configuration for quick testing
            server.ping_interval_sec = 1;
            server.ping_count = 1;

            servers_.push_back(server);

            ServerStatus status;
            status.server_id = server.id;
            status.quality = ConnectionQuality::UNKNOWN;
            {
                std::lock_guard<std::mutex> lock(status_mutex_);
                server_statuses_[server.id] = status;
            }
        }

        LOG_INFO("[ServersStatus] Loaded " << servers_.size() << " servers from config" << std::endl);
        return !servers_.empty();

    } catch (const std::exception& e) {
        LOG_ERROR("[ServersStatus] Error parsing config: " << e.what() << std::endl);
        return false;
    }
}

bool ServersStatusMonitor::startMonitoring() {
    if (servers_.empty()) {
        LOG_ERROR("[ServersStatus] Error: No servers configured" << std::endl);
        return false;
    }

    monitoring_active_.store(true);

    LOG_INFO("[ServersStatus] Starting sequential monitoring for " << servers_.size() << " servers..." << std::endl);

    // Create a single coordinator thread that runs tests sequentially
    auto coordinator = [this]() {
        this->sequentialCoordinatorThread();
    };

    try {
        unsigned int thread_id = thread_manager_->createThread(coordinator);
        thread_ids_.push_back(thread_id);

        std::string attachment_id = "sequential_coordinator";
        thread_manager_->registerThread(thread_id, attachment_id);

        LOG_INFO("[ServersStatus] Created sequential coordinator thread " << thread_id << std::endl);
    } catch (const std::exception& e) {
        LOG_ERROR("[ServersStatus] Error creating coordinator thread: " << e.what() << std::endl);
        return false;
    }

    return true;
}

void ServersStatusMonitor::sequentialCoordinatorThread() {
    LOG_INFO("[ServersStatus] Sequential coordinator started" << std::endl);

    int cycle = 0;
    bool first_cycle_complete = false;
    
    std::string progress_json_file = output_dir_ + "/progress.json";
    std::string current_status_json_file = output_dir_ + "/current_status.json";

    while (monitoring_active_.load()) {
        cycle++;
        
        if (!first_cycle_complete) {
            LOG_INFO("\n[ServersStatus] ========== Initial Scan (Cycle " << cycle << ") ==========" << std::endl);
            LOG_INFO("[ServersStatus] Testing " << servers_.size() << " servers..." << std::endl);
        } else {
            LOG_INFO("\n[ServersStatus] ========== Cycle " << cycle << " ==========" << std::endl);
        }

        for (size_t i = 0; i < servers_.size() && monitoring_active_.load(); i++) {
            const auto& server = servers_[i];
            
            // Update progress
            int progress = (int)(((i + 1) * 100.0) / servers_.size());
            {
                std::lock_guard<std::mutex> lock(progress_mutex_);
                current_progress_.total_servers = servers_.size();
                current_progress_.tested_servers = i + 1;
                current_progress_.percentage = progress;
                current_progress_.current_server_name = server.name;
                current_progress_.current_server_host = server.host;
                current_progress_.timestamp = getCurrentTimestamp();
            }
            
            // Export progress JSON
            exportProgressJSON(progress_json_file);
            
            // Show percentage progress for all cycles
            LOG_INFO("\r[ServersStatus] Progress: " << progress << "% [" << (i + 1) << "/" << servers_.size() << "] Testing: " << server.name << "          " << std::flush);
            
            // Run ping test for this server (blocking call)
            pingWorkerThread(server);
            
            // Export current status JSON every second (after each server test)
            exportCurrentStatusJSON(current_status_json_file);
        }

        if (monitoring_active_.load()) {
            // Clear the progress line
            LOG_INFO("\r" << std::string(80, ' ') << "\r" << std::flush);
            
            if (!first_cycle_complete) {
                LOG_INFO("[ServersStatus] Progress: 100% - Initial scan complete!" << std::endl);
                LOG_INFO("\n[ServersStatus] ========== Initial Scan Complete ==========" << std::endl);
                LOG_INFO("[ServersStatus] Starting continuous monitoring (updates every 1 second)...\n" << std::endl);
                first_cycle_complete = true;
                
                // Display the initial results table
                displayStatus();
                
                // Export final progress and status
                exportProgressJSON(progress_json_file);
                exportCurrentStatusJSON(current_status_json_file);
                
                // Short pause before continuous monitoring
                sleep(1);
            } else {
                LOG_INFO("[ServersStatus] Progress: 100% - Cycle " << cycle << " complete!" << std::endl);
                
                // After first cycle, display status every 1 second
                displayStatus();
                
                // Export progress and status
                exportProgressJSON(progress_json_file);
                exportCurrentStatusJSON(current_status_json_file);
                
                // Wait 1 second before next cycle
                sleep(1);
            }
        }
    }

    LOG_INFO("[ServersStatus] Sequential coordinator stopped after " << cycle << " cycles" << std::endl);
}

void ServersStatusMonitor::stopMonitoring() {
    if (!monitoring_active_.load()) {
        return;
    }

    LOG_INFO("[ServersStatus] Stopping monitoring..." << std::endl);
    monitoring_active_.store(false);

    if (thread_manager_) {
        try {
            std::vector<std::string> attachments = thread_manager_->getAllAttachments();
            for (const auto& attachment : attachments) {
                if (attachment.find("ping_worker_") == 0) {
                    try {
                        LOG_INFO("[ServersStatus] Stopping thread with attachment: " << attachment << std::endl);
                        thread_manager_->stopThreadByAttachment(attachment);
                        thread_manager_->unregisterThread(attachment);
                    } catch (const std::exception& e) {
                        LOG_ERROR("[ServersStatus] Error stopping thread by attachment " << attachment << ": " << e.what() << std::endl);
                    }
                }
            }
        } catch (const std::exception& e) {
            LOG_ERROR("[ServersStatus] Error getting attachments: " << e.what() << std::endl);
        }

        for (auto thread_id : thread_ids_) {
            try {
                if (thread_manager_->isThreadAlive(thread_id)) {
                    thread_manager_->stopThread(thread_id);
                }
            } catch (const std::exception& e) {
                LOG_ERROR("[ServersStatus] Error stopping thread " << thread_id << ": " << e.what() << std::endl);
            }
        }
        thread_ids_.clear();
    }
}

void ServersStatusMonitor::pingWorkerThread(const ServerInfo& server) {
    PingConfig config;
    config.destination = server.host;
    config.count = server.ping_count;
    config.interval_ms = 200;
    config.timeout_ms = 500;
    config.packet_size = 64;
    config.ttl = 64;
    config.resolve_hostname = true;

    bool success = false;
    PingResult result;

    // Single attempt, no retries
    try {
        PingAPI ping_api;
        ping_api.setConfig(config);
        result = ping_api.execute();

        // Validate result
        if (result.packets_sent > 0 && result.packets_received > 0) {
            success = true;
        }
    } catch (const std::exception& e) {
        LOG_ERROR("[ServersStatus] Ping exception for " << server.name 
                  << ": " << e.what() << std::endl);
    }

    // Update status directly in memory (thread-safe)
    ServerStatusResult status_result;
    {
        std::lock_guard<std::mutex> lock(status_mutex_);
        ServerStatus& status = server_statuses_[server.id];
        status.server_id = server.id;
        status.last_update_time = getCurrentTimestamp();

        if (success) {
            status.avg_rtt_ms = result.avg_rtt_ms;
            status.packet_loss_percent = result.loss_percentage;
            status.is_reachable = true;
            status.consecutive_failures = 0;
            status.quality = calculateQuality(status.avg_rtt_ms, status.packet_loss_percent);
        } else {
            status.consecutive_failures++;
            status.is_reachable = false;
            status.quality = ConnectionQuality::UNREACHABLE;
        }
        
        // Prepare result for serialization
        status_result.server_id = server.id;
        status_result.server_name = server.name;
        status_result.server_host = server.host;
        status_result.server_port = server.port;
        status_result.continent = server.continent;
        status_result.country = server.country;
        status_result.site = server.site;
        status_result.provider = server.provider;
        status_result.quality = static_cast<NetBench::Shared::ConnectionQuality>(status.quality);
        status_result.avg_rtt_ms = status.avg_rtt_ms;
        status_result.packet_loss_percent = status.packet_loss_percent;
        status_result.last_update_time = status.last_update_time;
        status_result.is_reachable = status.is_reachable;
        status_result.consecutive_failures = status.consecutive_failures;
    }

    // Write real-time JSON result using shared serializer (atomic write)
    std::string result_file = getResultFilePath(server.id);
    std::string temp_file = result_file + ".tmp";

    json result_json = ServerStatusSerializer::serializeResult(status_result);
    result_json["ping_details"] = {
        {"target", config.destination},
        {"resolved_ip", success ? result.ip_address : ""},
        {"packets_sent", success ? result.packets_sent : 0},
        {"packets_received", success ? result.packets_received : 0},
        {"min_rtt_ms", success ? result.min_rtt_ms : 0.0},
        {"max_rtt_ms", success ? result.max_rtt_ms : 0.0},
        {"stddev_rtt_ms", success ? result.stddev_rtt_ms : 0.0}
    };

    // Atomic write: write to temp file, then rename
    std::ofstream out(temp_file, std::ios::trunc);
    if (out.is_open()) {
        out << result_json.dump(2);
        out.close();
        std::rename(temp_file.c_str(), result_file.c_str());
    }
}

ConnectionQuality ServersStatusMonitor::calculateQuality(double avg_rtt_ms, double packet_loss) {
    // Prioritize packet loss over RTT
    if (packet_loss >= 100.0) {
        return ConnectionQuality::UNREACHABLE;
    }
    if (packet_loss > 20.0) {
        return ConnectionQuality::POOR;
    }
    if (packet_loss > 10.0) {
        return ConnectionQuality::FAIR;
    }
    if (packet_loss > 5.0) {
        return ConnectionQuality::FAIR;
    }

    // If packet loss is acceptable, check RTT
    if (avg_rtt_ms < 20.0) {
        return ConnectionQuality::EXCELLENT;
    } else if (avg_rtt_ms < 50.0) {
        return ConnectionQuality::GOOD;
    } else if (avg_rtt_ms < 100.0) {
        return ConnectionQuality::FAIR;
    } else if (avg_rtt_ms < 200.0) {
        return ConnectionQuality::POOR;
    } else {
        return ConnectionQuality::POOR;
    }
}

void ServersStatusMonitor::displayStatus() const {
    std::lock_guard<std::mutex> lock(status_mutex_);

    LOG_INFO("\n" << std::string(80, '=') << "\n");
    LOG_INFO("  SERVERS CONNECTION STATUS\n");
    LOG_INFO(std::string(80, '=') << "\n\n");

    LOG_INFO(std::left << std::setw(25) << "Server"
              << std::setw(15) << "Quality"
              << std::setw(12) << "Avg RTT"
              << std::setw(12) << "Loss %"
              << std::setw(16) << "Status" << "\n");
    LOG_INFO(std::string(80, '-') << "\n");

    for (const auto& server : servers_) {
        auto it = server_statuses_.find(server.id);
        if (it == server_statuses_.end()) {
            continue;
        }

        const ServerStatus& status = it->second;

        LOG_INFO(std::left << std::setw(25) << server.name.substr(0, 24)
                  << std::setw(15) << qualityToString(status.quality));

        if (status.is_reachable) {
            LOG_INFO(std::setw(12) << (std::to_string(static_cast<int>(status.avg_rtt_ms)) + " ms")
                      << std::setw(12) << (std::to_string(static_cast<int>(status.packet_loss_percent)) + "%")
                      << std::setw(16) << "Reachable");
        } else {
            LOG_INFO(std::setw(12) << "N/A"
                      << std::setw(12) << "N/A"
                      << std::setw(16) << "Unreachable");
        }

        LOG_INFO("\n");
    }

    LOG_INFO(std::string(80, '=') << "\n");
}

void ServersStatusMonitor::displayContinuousStatus(int refresh_interval_sec) {
    // The continuous display is now handled by sequentialCoordinatorThread
    // This function just waits for monitoring to stop
    while (monitoring_active_.load()) {
        sleep(1);
    }
}

std::map<std::string, ServerStatus> ServersStatusMonitor::getServerStatuses() const {
    std::lock_guard<std::mutex> lock(status_mutex_);
    return server_statuses_;
}

std::string ServersStatusMonitor::qualityToString(ConnectionQuality quality) {
    switch (quality) {
        case ConnectionQuality::EXCELLENT:
            return "EXCELLENT";
        case ConnectionQuality::GOOD:
            return "GOOD";
        case ConnectionQuality::FAIR:
            return "FAIR";
        case ConnectionQuality::POOR:
            return "POOR";
        case ConnectionQuality::UNREACHABLE:
            return "UNREACHABLE";
        case ConnectionQuality::UNKNOWN:
        default:
            return "UNKNOWN";
    }
}

std::string ServersStatusMonitor::qualityToColorCode(ConnectionQuality quality) {
    switch (quality) {
        case ConnectionQuality::EXCELLENT:
            return "\033[1;32m";
        case ConnectionQuality::GOOD:
            return "\033[0;32m";
        case ConnectionQuality::FAIR:
            return "\033[1;33m";
        case ConnectionQuality::POOR:
            return "\033[0;31m";
        case ConnectionQuality::UNREACHABLE:
            return "\033[1;31m";
        case ConnectionQuality::UNKNOWN:
        default:
            return "\033[0m";
    }
}

std::string ServersStatusMonitor::getCurrentTimestamp() const {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

std::string ServersStatusMonitor::getResultFilePath(const std::string& server_id) const {
    return output_dir_ + "/ping_" + server_id + ".json";
}

void ServersStatusMonitor::exportAggregatedResults(const std::string& output_file) {
    ServersStatusResults results;
    results.timestamp = getCurrentTimestamp();
    results.success = true;
    
    std::lock_guard<std::mutex> lock(status_mutex_);
    
    for (const auto& server : servers_) {
        auto it = server_statuses_.find(server.id);
        if (it != server_statuses_.end()) {
            const ServerStatus& status = it->second;
            
            ServerStatusResult result;
            result.server_id = server.id;
            result.server_name = server.name;
            result.server_host = server.host;
            result.server_port = server.port;
            result.continent = server.continent;
            result.country = server.country;
            result.site = server.site;
            result.provider = server.provider;
            result.quality = static_cast<NetBench::Shared::ConnectionQuality>(status.quality);
            result.avg_rtt_ms = status.avg_rtt_ms;
            result.packet_loss_percent = status.packet_loss_percent;
            result.last_update_time = status.last_update_time;
            result.is_reachable = status.is_reachable;
            result.consecutive_failures = status.consecutive_failures;
            
            results.servers.push_back(result);
            
            if (status.is_reachable) {
                results.reachable_servers++;
            } else {
                results.unreachable_servers++;
            }
        }
    }
    
    results.total_servers = servers_.size();
    
    ServerStatusSerializer::exportToFile(results, output_file);
    LOG_INFO("[ServersStatus] Exported aggregated results to: " << output_file << std::endl);
}



void ServersStatusMonitor::exportProgressJSON(const std::string& filepath) const {
    std::lock_guard<std::mutex> lock(progress_mutex_);
    
    json progress_json = NetBench::Shared::ServerStatusProgressSerializer::serialize(current_progress_);
    
    std::string temp_file = filepath + ".tmp";
    std::ofstream out(temp_file, std::ios::trunc);
    if (out.is_open()) {
        out << progress_json.dump(2);
        out.close();
        std::rename(temp_file.c_str(), filepath.c_str());
    }
    
    // Also print to console with flush and newline
    LOG_INFO("\n[ProgressJSON] " << progress_json.dump() << "\n" << std::flush);
}

void ServersStatusMonitor::exportCurrentStatusJSON(const std::string& filepath) const {
    ServersStatusResults results;
    results.timestamp = getCurrentTimestamp();
    results.success = true;
    
    std::lock_guard<std::mutex> lock(status_mutex_);
    
    for (const auto& server : servers_) {
        auto it = server_statuses_.find(server.id);
        if (it != server_statuses_.end()) {
            const ServerStatus& status = it->second;
            
            ServerStatusResult result;
            result.server_id = server.id;
            result.server_name = server.name;
            result.server_host = server.host;
            result.server_port = server.port;
            result.continent = server.continent;
            result.country = server.country;
            result.site = server.site;
            result.provider = server.provider;
            result.quality = static_cast<NetBench::Shared::ConnectionQuality>(status.quality);
            result.avg_rtt_ms = status.avg_rtt_ms;
            result.packet_loss_percent = status.packet_loss_percent;
            result.last_update_time = status.last_update_time;
            result.is_reachable = status.is_reachable;
            result.consecutive_failures = status.consecutive_failures;
            
            results.servers.push_back(result);
            
            if (status.is_reachable) {
                results.reachable_servers++;
            } else {
                results.unreachable_servers++;
            }
        }
    }
    
    results.total_servers = servers_.size();
    
    json status_json = NetBench::Shared::ServerStatusSerializer::serializeResults(results);
    
    std::string temp_file = filepath + ".tmp";
    std::ofstream out(temp_file, std::ios::trunc);
    if (out.is_open()) {
        out << status_json.dump(2);
        out.close();
        std::rename(temp_file.c_str(), filepath.c_str());
    }
    
    // Also print to console with flush and newline
    LOG_INFO("\n[CurrentStatusJSON] " << status_json.dump() << "\n" << std::flush);
}
