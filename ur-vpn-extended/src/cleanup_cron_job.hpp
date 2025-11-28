#ifndef CLEANUP_CRON_JOB_HPP
#define CLEANUP_CRON_JOB_HPP

#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <vector>
#include <filesystem>
#include <nlohmann/json.hpp>
#include "cleanup_tracker.hpp"
#include "cleanup_verifier.hpp"

// Forward declarations
namespace ThreadMgr {
    class ThreadManager;
}

using json = nlohmann::json;

namespace vpn_manager {

// Forward declaration
class VPNInstanceManager;

struct VerificationTask {
    std::string operation_id;
    std::string instance_name;
    std::chrono::system_clock::time_point scheduled_time;
    int retry_count;
};

class CleanupCronJob {
private:
    VPNInstanceManager* manager_;
    CleanupTracker* tracker_;
    CleanupVerifier verifier_;
    ThreadMgr::ThreadManager* thread_manager_;
    
    std::atomic<bool> running_;
    unsigned int thread_id_;
    int cleanup_interval_seconds_;
    json config_;
    std::string cleanup_config_path_;
    
    std::vector<VerificationTask> pending_verifications_;
    std::mutex pending_verifications_mutex_;
    
    void loadConfiguration();
    void runCleanupLoop();
    void processPendingVerifications();
    void processVerificationTask(const VerificationTask& task);
    void sendVerificationResponse(const std::string& operation_id,
                                 const std::string& instance_name,
                                 const json& verification_report,
                                 bool cleanup_successful);

public:
    CleanupCronJob(VPNInstanceManager* manager, 
                   CleanupTracker* tracker,
                   const std::string& config_path,
                   const std::string& routing_path,
                   const std::string& cleanup_config_path = "");
    ~CleanupCronJob();
    
    void start();
    void stop();
    
    void scheduleVerification(const std::string& operation_id, const std::string& instance_name);
    
    json getCronJobStatus();
    
    // Configuration methods
    void setCleanupInterval(int seconds) { cleanup_interval_seconds_ = seconds; }
    int getCleanupInterval() const { return cleanup_interval_seconds_; }
    bool isRunning() const { return running_.load(); }
};

}

#endif
