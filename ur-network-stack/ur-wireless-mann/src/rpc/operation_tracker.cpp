#include "urwt/rpc/operation_tracker.hpp"

namespace urwt {
namespace rpc {

OperationTracker::OperationTracker() {}

OperationTracker::~OperationTracker() {}

void OperationTracker::startOperation(const std::string& transaction_id, const std::string& action) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    OperationInfo info;
    info.transaction_id = transaction_id;
    info.action = action;
    info.start_time = std::chrono::steady_clock::now();
    info.completed = false;
    
    active_operations_[transaction_id] = info;
    total_operations_++;
}

void OperationTracker::completeOperation(const std::string& transaction_id, bool success) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = active_operations_.find(transaction_id);
    if (it != active_operations_.end()) {
        auto now = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - it->second.start_time).count();
        
        it->second.completed = true;
        it->second.success = success;
        it->second.execution_time_ms = duration;
        
        total_execution_time_ms_ += duration;
        
        if (success) {
            successful_operations_++;
        } else {
            failed_operations_++;
        }
        
        active_operations_.erase(it);
    }
}

void OperationTracker::failOperation(const std::string& transaction_id) {
    completeOperation(transaction_id, false);
}

size_t OperationTracker::getActiveCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return active_operations_.size();
}

size_t OperationTracker::getTotalOperations() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return total_operations_;
}

size_t OperationTracker::getSuccessfulOperations() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return successful_operations_;
}

size_t OperationTracker::getFailedOperations() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return failed_operations_;
}

double OperationTracker::getAverageExecutionTime() const {
    std::lock_guard<std::mutex> lock(mutex_);
    size_t completed = successful_operations_ + failed_operations_;
    if (completed == 0) {
        return 0.0;
    }
    return static_cast<double>(total_execution_time_ms_) / completed;
}

bool OperationTracker::isOperationActive(const std::string& transaction_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return active_operations_.find(transaction_id) != active_operations_.end();
}

OperationInfo OperationTracker::getOperationInfo(const std::string& transaction_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = active_operations_.find(transaction_id);
    if (it != active_operations_.end()) {
        return it->second;
    }
    return OperationInfo{};
}

} // namespace rpc
} // namespace urwt
