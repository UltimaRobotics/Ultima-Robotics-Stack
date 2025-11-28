#include "cleanup_cron_job.hpp"
#include "cleanup_tracker.hpp"
#include "cleanup_verifier.hpp"
#include "vpn_instance_manager.hpp"
#include <iostream>
#include <thread>
#include <chrono>

namespace vpn_manager {

CleanupCronJob::CleanupCronJob(VPNInstanceManager* manager, 
                             CleanupTracker* tracker, 
                             const std::string& config_path,
                             const std::string& routing_path,
                             const std::string& cleanup_config_path)
    : manager_(manager), tracker_(tracker), verifier_(config_path, routing_path),
      running_(false), cleanup_interval_seconds_(30), cleanup_config_path_(cleanup_config_path) {
    
    // Get thread manager from VPN instance manager
    thread_manager_ = manager_->getThreadManager();
    
    // Load configuration
    loadConfiguration();
}

CleanupCronJob::~CleanupCronJob() {
    stop();
}

void CleanupCronJob::loadConfiguration() {
    // Default configuration
    config_ = {
        {"cleanup_interval_seconds", 30},
        {"verification_delay_seconds", 5},
        {"max_retry_attempts", 3},
        {"cleanup_timeout_seconds", 60},
        {"enable_auto_cleanup", true},
        {"log_level", "info"}
    };
    
    // Try to load from config file
    std::string config_file = cleanup_config_path_.empty() ? "config/cleanup-config.json" : cleanup_config_path_;
    if (std::filesystem::exists(config_file)) {
        try {
            std::ifstream file(config_file);
            json file_config;
            file >> file_config;
            
            // Merge with defaults
            for (auto& [key, value] : file_config.items()) {
                config_[key] = value;
            }
        } catch (const std::exception& e) {
            std::cerr << "Failed to load cleanup config: " << e.what() << std::endl;
        }
    }
    
    cleanup_interval_seconds_ = config_.value("cleanup_interval_seconds", 30);
}

void CleanupCronJob::start() {
    if (running_.load()) {
        std::cout << "[CleanupCron] Already running" << std::endl;
        return;
    }
    
    running_.store(true);
    
    // Create thread function for Threader API
    auto cleanup_function = [this]() {
        this->runCleanupLoop();
    };
    
    // Register with ThreadManager
    thread_id_ = thread_manager_->createThread(cleanup_function);
    
    std::cout << "[CleanupCron] Started cleanup cron job with thread ID: " << thread_id_ << std::endl;
    std::cout << "[CleanupCron] Cleanup interval: " << cleanup_interval_seconds_ << " seconds" << std::endl;
}

void CleanupCronJob::stop() {
    if (!running_.load()) {
        return;
    }
    
    running_.store(false);
    
    if (thread_id_ > 0) {
        try {
            thread_manager_->stopThread(thread_id_);
        } catch (const std::exception& e) {
            std::cerr << "[CleanupCron] Error stopping cleanup thread: " << e.what() << std::endl;
        }
    }
    
    std::cout << "[CleanupCron] Stopped cleanup cron job" << std::endl;
}

void CleanupCronJob::scheduleVerification(const std::string& operation_id, 
                                         const std::string& instance_name) {
    std::lock_guard<std::mutex> lock(pending_verifications_mutex_);
    
    VerificationTask task;
    task.operation_id = operation_id;
    task.instance_name = instance_name;
    task.scheduled_time = std::chrono::system_clock::now() + 
                         std::chrono::seconds(config_.value("verification_delay_seconds", 5));
    task.retry_count = 0;
    
    pending_verifications_.push_back(task);
    
    std::cout << "[CleanupCron] Scheduled verification for operation: " << operation_id 
              << " instance: " << instance_name << std::endl;
}

void CleanupCronJob::runCleanupLoop() {
    std::cout << "[CleanupCron] Cleanup loop started" << std::endl;
    
    while (running_.load()) {
        try {
            // Process pending verifications
            processPendingVerifications();
            
            // Cleanup old operations
            tracker_->cleanupOldOperations();
            
            // Sleep for configured interval
            for (int i = 0; i < cleanup_interval_seconds_ && running_.load(); ++i) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
            
        } catch (const std::exception& e) {
            std::cerr << "[CleanupCron] Error in cleanup loop: " << e.what() << std::endl;
        }
    }
    
    std::cout << "[CleanupCron] Cleanup loop stopped" << std::endl;
}

void CleanupCronJob::processPendingVerifications() {
    std::lock_guard<std::mutex> lock(pending_verifications_mutex_);
    
    auto now = std::chrono::system_clock::now();
    auto it = pending_verifications_.begin();
    
    while (it != pending_verifications_.end()) {
        if (now >= it->scheduled_time) {
            // Process this verification task
            processVerificationTask(*it);
            it = pending_verifications_.erase(it);
        } else {
            ++it;
        }
    }
}

void CleanupCronJob::processVerificationTask(const VerificationTask& task) {
    std::cout << "[CleanupCron] Processing verification for operation: " << task.operation_id 
              << " instance: " << task.instance_name << std::endl;
    
    try {
        // Generate verification report
        json verification_report = verifier_.generateVerificationReport(task.instance_name);
        
        // Update tracker with verification results
        json verification_data = {
            {"verification_report", verification_report},
            {"verification_timestamp", std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count()},
            {"retry_count", task.retry_count}
        };
        
        // Determine if cleanup was successful
        bool cleanup_successful = !verification_report["summary"]["any_resources_exist"].get<bool>();
        
        if (cleanup_successful) {
            tracker_->setComponentStatus(task.operation_id, CleanupComponent::VERIFICATION_JOB,
                                       CleanupStatus::VERIFIED, "", verification_data);
            
            std::cout << "[CleanupCron] Verification passed - cleanup successful for: " << task.instance_name << std::endl;
        } else {
            // Cleanup incomplete - schedule retry if within limits
            int max_retries = config_.value("max_retry_attempts", 3);
            
            if (task.retry_count < max_retries) {
                std::cout << "[CleanupCron] Verification failed - scheduling retry (" 
                          << (task.retry_count + 1) << "/" << max_retries << ") for: " << task.instance_name << std::endl;
                
                // Schedule retry
                VerificationTask retry_task = task;
                retry_task.scheduled_time = std::chrono::system_clock::now() + 
                                           std::chrono::seconds(10 * (task.retry_count + 1)); // Exponential backoff
                retry_task.retry_count = task.retry_count + 1;
                
                std::lock_guard<std::mutex> retry_lock(pending_verifications_mutex_);
                pending_verifications_.push_back(retry_task);
                
                tracker_->setComponentStatus(task.operation_id, CleanupComponent::VERIFICATION_JOB,
                                           CleanupStatus::IN_PROGRESS, "Cleanup incomplete, retry scheduled", verification_data);
            } else {
                std::cout << "[CleanupCron] Verification failed - max retries exceeded for: " << task.instance_name << std::endl;
                
                tracker_->setComponentStatus(task.operation_id, CleanupComponent::VERIFICATION_JOB,
                                           CleanupStatus::FAILED, "Cleanup incomplete after " + std::to_string(max_retries) + " retries", verification_data);
            }
        }
        
        // Send RPC response with verification results
        sendVerificationResponse(task.operation_id, task.instance_name, verification_report, cleanup_successful);
        
    } catch (const std::exception& e) {
        std::cerr << "[CleanupCron] Error processing verification task: " << e.what() << std::endl;
        
        tracker_->setComponentStatus(task.operation_id, CleanupComponent::VERIFICATION_JOB,
                                   CleanupStatus::FAILED, "Verification error: " + std::string(e.what()));
    }
}

void CleanupCronJob::sendVerificationResponse(const std::string& operation_id,
                                             const std::string& instance_name,
                                             const json& verification_report,
                                             bool cleanup_successful) {
    // Create RPC response for cleanup verification
    json response = {
        {"jsonrpc", "2.0"},
        {"id", "cleanup_verification_" + operation_id},
        {"result", {
            {"type", "cleanup_verification"},
            {"operation_id", operation_id},
            {"instance_name", instance_name},
            {"cleanup_successful", cleanup_successful},
            {"verification_report", verification_report},
            {"timestamp", std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count()}
        }}
    };
    
    // Send via MQTT - this would need to be integrated with the existing MQTT client
    std::cout << "[CleanupCron] Verification response: " << response.dump(2) << std::endl;
    
    // TODO: Integrate with actual MQTT client to send response
    // mqtt_client_->publish("direct_messaging/ur-vpn-manager/responses", response.dump());
}

json CleanupCronJob::getCronJobStatus() {
    std::lock_guard<std::mutex> lock(pending_verifications_mutex_);
    
    json status = {
        {"running", running_.load()},
        {"thread_id", thread_id_},
        {"cleanup_interval_seconds", cleanup_interval_seconds_},
        {"pending_verifications_count", pending_verifications_.size()},
        {"config", config_}
    };
    
    // Add pending verification details
    json pending = json::array();
    for (const auto& task : pending_verifications_) {
        pending.push_back({
            {"operation_id", task.operation_id},
            {"instance_name", task.instance_name},
            {"retry_count", task.retry_count},
            {"scheduled_time", std::chrono::duration_cast<std::chrono::seconds>(
                task.scheduled_time.time_since_epoch()).count()}
        });
    }
    status["pending_verifications"] = pending;
    
    return status;
}

}
