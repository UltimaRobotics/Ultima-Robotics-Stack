#ifndef URWT_RPC_OPERATION_TRACKER_HPP
#define URWT_RPC_OPERATION_TRACKER_HPP

#include <string>
#include <map>
#include <mutex>
#include <chrono>
#include "urwt/rpc/rpc_types.hpp"

namespace urwt {
namespace rpc {

struct OperationInfo {
    std::string transaction_id;
    std::string action;
    std::chrono::steady_clock::time_point start_time;
    bool completed{false};
    bool success{false};
    int64_t execution_time_ms{0};
};

class OperationTracker {
public:
    OperationTracker();
    ~OperationTracker();

    void startOperation(const std::string& transaction_id, const std::string& action);
    void completeOperation(const std::string& transaction_id, bool success);
    void failOperation(const std::string& transaction_id);
    
    size_t getActiveCount() const;
    size_t getTotalOperations() const;
    size_t getSuccessfulOperations() const;
    size_t getFailedOperations() const;
    
    double getAverageExecutionTime() const;
    
    bool isOperationActive(const std::string& transaction_id) const;
    OperationInfo getOperationInfo(const std::string& transaction_id) const;

private:
    mutable std::mutex mutex_;
    std::map<std::string, OperationInfo> active_operations_;
    size_t total_operations_{0};
    size_t successful_operations_{0};
    size_t failed_operations_{0};
    int64_t total_execution_time_ms_{0};
};

} // namespace rpc
} // namespace urwt

#endif // URWT_RPC_OPERATION_TRACKER_HPP
