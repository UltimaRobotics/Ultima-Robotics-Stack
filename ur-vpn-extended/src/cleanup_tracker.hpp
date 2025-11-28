#ifndef CLEANUP_TRACKER_HPP
#define CLEANUP_TRACKER_HPP

#include <string>
#include <map>
#include <chrono>
#include <atomic>
#include <mutex>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace vpn_manager {

enum class CleanupStatus {
    NOT_STARTED,
    IN_PROGRESS,
    COMPLETED,
    FAILED,
    VERIFIED
};

enum class CleanupComponent {
    THREAD_TERMINATION,
    ROUTING_RULES_CLEAR,
    VPN_DISCONNECT,
    CONFIGURATION_UPDATE,
    VERIFICATION_JOB
};

struct CleanupStep {
    std::string component_name;
    CleanupStatus status;
    std::chrono::system_clock::time_point start_time;
    std::chrono::system_clock::time_point end_time;
    std::string error_message;
    json verification_data;
    
    json to_json() const {
        json j;
        j["component_name"] = component_name;
        j["status"] = status_to_string(status);
        j["start_time"] = std::chrono::duration_cast<std::chrono::seconds>(
            start_time.time_since_epoch()).count();
        if (end_time > start_time) {
            j["end_time"] = std::chrono::duration_cast<std::chrono::seconds>(
                end_time.time_since_epoch()).count();
            j["duration_ms"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                end_time - start_time).count();
        }
        if (!error_message.empty()) {
            j["error_message"] = error_message;
        }
        if (!verification_data.empty()) {
            j["verification_data"] = verification_data;
        }
        return j;
    }
    
    static std::string status_to_string(CleanupStatus status) {
        switch (status) {
            case CleanupStatus::NOT_STARTED: return "not_started";
            case CleanupStatus::IN_PROGRESS: return "in_progress";
            case CleanupStatus::COMPLETED: return "completed";
            case CleanupStatus::FAILED: return "failed";
            case CleanupStatus::VERIFIED: return "verified";
            default: return "unknown";
        }
    }
};

class CleanupTracker {
private:
    std::map<std::string, std::map<CleanupComponent, CleanupStep>> cleanup_operations_;
    std::mutex tracker_mutex_;
    std::atomic<uint64_t> operation_counter_{0};
    
public:
    std::string startCleanupOperation(const std::string& instance_name) {
        std::lock_guard<std::mutex> lock(tracker_mutex_);
        
        std::string operation_id = "cleanup_" + std::to_string(operation_counter_++) + "_" + instance_name;
        
        // Initialize all cleanup steps
        cleanup_operations_[operation_id][CleanupComponent::THREAD_TERMINATION] = 
            {"thread_termination", CleanupStatus::NOT_STARTED, std::chrono::system_clock::now(), {}, ""};
        cleanup_operations_[operation_id][CleanupComponent::ROUTING_RULES_CLEAR] = 
            {"routing_rules_cleared", CleanupStatus::NOT_STARTED, std::chrono::system_clock::now(), {}, ""};
        cleanup_operations_[operation_id][CleanupComponent::VPN_DISCONNECT] = 
            {"vpn_disconnected", CleanupStatus::NOT_STARTED, std::chrono::system_clock::now(), {}, ""};
        cleanup_operations_[operation_id][CleanupComponent::CONFIGURATION_UPDATE] = 
            {"configuration_updated", CleanupStatus::NOT_STARTED, std::chrono::system_clock::now(), {}, ""};
        cleanup_operations_[operation_id][CleanupComponent::VERIFICATION_JOB] = 
            {"verification_job", CleanupStatus::NOT_STARTED, std::chrono::system_clock::now(), {}, ""};
        
        return operation_id;
    }
    
    void setComponentStatus(const std::string& operation_id, CleanupComponent component, 
                           CleanupStatus status, const std::string& error_message = "",
                           const json& verification_data = json{}) {
        std::lock_guard<std::mutex> lock(tracker_mutex_);
        
        auto it = cleanup_operations_.find(operation_id);
        if (it != cleanup_operations_.end()) {
            auto& step = it->second[component];
            step.status = status;
            step.error_message = error_message;
            step.verification_data = verification_data;
            
            if (status == CleanupStatus::IN_PROGRESS) {
                step.start_time = std::chrono::system_clock::now();
            } else if (status == CleanupStatus::COMPLETED || status == CleanupStatus::FAILED || status == CleanupStatus::VERIFIED) {
                step.end_time = std::chrono::system_clock::now();
            }
        }
    }
    
    json getCleanupStatus(const std::string& operation_id) {
        std::lock_guard<std::mutex> lock(tracker_mutex_);
        
        json result;
        auto it = cleanup_operations_.find(operation_id);
        if (it == cleanup_operations_.end()) {
            result["error"] = "Operation not found";
            return result;
        }
        
        result["operation_id"] = operation_id;
        result["components"] = json::object();
        
        bool all_completed = true;
        bool any_failed = false;
        
        for (const auto& [component, step] : it->second) {
            result["components"][step.component_name] = step.to_json();
            
            if (step.status != CleanupStatus::COMPLETED && step.status != CleanupStatus::VERIFIED) {
                all_completed = false;
            }
            if (step.status == CleanupStatus::FAILED) {
                any_failed = true;
            }
        }
        
        result["overall_status"] = any_failed ? "failed" : (all_completed ? "completed" : "in_progress");
        result["success"] = !any_failed && all_completed;
        
        return result;
    }
    
    void completeOperation(const std::string& operation_id) {
        std::lock_guard<std::mutex> lock(tracker_mutex_);
        
        auto it = cleanup_operations_.find(operation_id);
        if (it != cleanup_operations_.end()) {
            // Mark all incomplete steps as failed
            for (auto& [component, step] : it->second) {
                if (step.status == CleanupStatus::NOT_STARTED || step.status == CleanupStatus::IN_PROGRESS) {
                    step.status = CleanupStatus::FAILED;
                    step.error_message = "Operation timed out or incomplete";
                    step.end_time = std::chrono::system_clock::now();
                }
            }
        }
    }
    
    void cleanupOldOperations(int max_age_seconds = 300) {
        std::lock_guard<std::mutex> lock(tracker_mutex_);
        
        auto now = std::chrono::system_clock::now();
        auto it = cleanup_operations_.begin();
        
        while (it != cleanup_operations_.end()) {
            bool should_remove = true;
            for (const auto& [component, step] : it->second) {
                auto age = std::chrono::duration_cast<std::chrono::seconds>(now - step.start_time);
                if (age.count() < max_age_seconds) {
                    should_remove = false;
                    break;
                }
            }
            
            if (should_remove) {
                it = cleanup_operations_.erase(it);
            } else {
                ++it;
            }
        }
    }
};

}

#endif
