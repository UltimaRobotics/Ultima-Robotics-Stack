#include "vpn_instance_manager.hpp"
#include "internal/vpn_manager_utils.hpp"
#include <iostream>
#include <thread>
#include <chrono>

namespace vpn_manager {

VPNInstanceManager::VPNInstanceManager()
    : thread_manager_(std::make_unique<ThreadMgr::ThreadManager>(20)),
      running_(true),
      verbose_(false),
      stats_logging_enabled_(true),
      openvpn_stats_logging_(true),
      wireguard_stats_logging_(true),
      config_save_pending_(false) {

    json cleanup_start_log;
    cleanup_start_log["type"] = "startup";
    cleanup_start_log["message"] = "VPNInstanceManager starting - running auto-cleanup";
    std::cout << cleanup_start_log.dump() << std::endl;

    VPNCleanup::cleanupAll(false);

    json cleanup_complete_log;
    cleanup_complete_log["type"] = "startup";
    cleanup_complete_log["message"] = "Auto-cleanup completed - ready for normal operations";
    std::cout << cleanup_complete_log.dump() << std::endl;

    config_save_thread_ = std::thread([this]() {
        while (running_) {
            std::this_thread::sleep_for(std::chrono::seconds(5));

            if (config_save_pending_.exchange(false)) {
                if (!config_file_path_.empty()) {
                    saveConfiguration(config_file_path_);
                }
            }
        }
    });
    
    startRouteMonitoring();
}

VPNInstanceManager::~VPNInstanceManager() {
    stopAll();

    if (config_save_thread_.joinable()) {
        config_save_thread_.join();
    }
    
    if (route_monitor_thread_.joinable()) {
        route_monitor_thread_.join();
    }
}

} // namespace vpn_manager
