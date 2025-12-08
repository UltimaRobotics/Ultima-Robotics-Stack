
#ifndef SERVERS_STATUS_MONITOR_HPP
#define SERVERS_STATUS_MONITOR_HPP

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <mutex>
#include <atomic>
#include <thread>
#include "json.hpp"
#include "../thirdparty/ur-threadder-api/cpp/include/ThreadManager.hpp"
#include "../thirdparty/ur-ping-api/include/PingAPI.hpp"
#include "../ur-netbench-shared/include/ServerStatusSerializer.hpp"
#include "../ur-netbench-shared/include/ServerStatusProgressSerializer.hpp"

using json = nlohmann::json;
using ConnectionQuality = NetBench::Shared::ConnectionQuality;

struct ServerInfo {
    std::string id;
    std::string host;
    std::string name;
    std::string description;
    std::string port;
    std::string continent;
    std::string country;
    std::string site;
    std::string provider;
    int ping_interval_sec;
    int ping_count;

    ServerInfo() : ping_interval_sec(5), ping_count(4) {}
};

struct ServerStatus {
    std::string server_id;
    ConnectionQuality quality;
    double avg_rtt_ms;
    double packet_loss_percent;
    std::string last_update_time;
    bool is_reachable;
    int consecutive_failures;

    ServerStatus() 
        : quality(ConnectionQuality::UNKNOWN), 
          avg_rtt_ms(0.0), 
          packet_loss_percent(0.0),
          is_reachable(false),
          consecutive_failures(0) {}
};

class ServersStatusMonitor {
public:
    ServersStatusMonitor(const std::string& output_dir = "runtime-data/server-status");
    ~ServersStatusMonitor();

    bool loadServersConfig(const std::string& config_file_path);

    bool startMonitoring();

    void stopMonitoring();

    void displayStatus() const;

    void displayContinuousStatus(int refresh_interval_sec = 2);

    std::map<std::string, ServerStatus> getServerStatuses() const;

    void exportAggregatedResults(const std::string& output_file);

    void exportProgressJSON(const std::string& filepath) const;

    void exportCurrentStatusJSON(const std::string& filepath) const;

    static std::string qualityToString(ConnectionQuality quality);
    static std::string qualityToColorCode(ConnectionQuality quality);

private:
    std::vector<ServerInfo> servers_;
    std::map<std::string, ServerStatus> server_statuses_;
    mutable std::mutex status_mutex_;

    NetBench::Shared::ServerStatusProgress current_progress_;
    mutable std::mutex progress_mutex_;

    std::unique_ptr<ThreadMgr::ThreadManager> thread_manager_;
    std::vector<unsigned int> thread_ids_;

    std::atomic<bool> monitoring_active_;
    std::thread spectator_thread_;

    std::string output_dir_;

    void pingWorkerThread(const ServerInfo& server);

    void sequentialCoordinatorThread();

    void spectatorThread();

    void updateServerStatus(const std::string& server_id, const std::string& result_file);

    ConnectionQuality calculateQuality(double avg_rtt_ms, double packet_loss);

    std::string getCurrentTimestamp() const;

    std::string getResultFilePath(const std::string& server_id) const;
};

#endif
