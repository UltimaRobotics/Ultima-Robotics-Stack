#ifndef RECOVERY_ENGINE_H
#define RECOVERY_ENGINE_H

#include "failure_detector.h"
#include "connection_manager.h"
#include <memory>
#include <functional>
#include <thread>
#include <atomic>
#include <chrono>
#include <queue>
#include <vector>
#include <map>

struct FailureEvent; // Forward declaration

struct RecoveryResult {
    bool success;
    std::string description;
    std::chrono::milliseconds duration;
    int attempts_made;
    FailureType original_failure;
};

enum class RecoveryAction {
    RESTART_SESSION,
    RENEW_DHCP,
    RESET_INTERFACE,
    SCAN_NETWORK,
    REBOOT_MODEM,
    WAIT_DELAY,
    ESCALATE
};

struct RecoveryStep {
    RecoveryAction action;
    std::string description;
    int timeout_ms;
    int max_attempts;
    bool parallel_execution;
    std::function<bool()> custom_action;
};

struct RecoveryPlan {
    FailureType failure_type;
    std::vector<RecoveryStep> steps;
    int max_cycles;
    bool escalate_on_failure;
};


using RecoveryCallback = std::function<void(const RecoveryResult&)>;

class QMISessionHandler;
class InterfaceController;
class ConnectivityMonitor;

class RecoveryEngine {
public:
    RecoveryEngine(QMISessionHandler* session_handler,
                  InterfaceController* interface_controller,
                  ConnectivityMonitor* connectivity_monitor);
    ~RecoveryEngine();

    // Recovery control
    void startRecoveryEngine();
    void stopRecoveryEngine();
    bool isRunning() const;

    // Recovery execution
    bool triggerRecovery(const FailureEvent& failure);
    bool executeRecoveryPlan(const RecoveryPlan& plan);
    void addRecoveryToQueue(const FailureEvent& failure);
    void clearRecoveryQueue();

    // Configuration
    void setRecoveryCallback(RecoveryCallback callback);
    void addRecoveryPlan(const RecoveryPlan& plan);
    void removeRecoveryPlan(FailureType failure_type);
    void setDefaultRecoveryPlan();
    void enableAutoRecovery(bool enable);
    void setMaxConcurrentRecoveries(int max);

    // Recovery plans
    RecoveryPlan createSessionRecoveryPlan();
    RecoveryPlan createIPRecoveryPlan();
    RecoveryPlan createConnectivityRecoveryPlan();
    RecoveryPlan createSignalRecoveryPlan();
    RecoveryPlan createModemRecoveryPlan();
    RecoveryPlan createInterfaceRecoveryPlan();

    // Status
    bool isRecoveryInProgress() const;
    int getQueueSize() const;
    std::vector<RecoveryResult> getRecentResults(int count = 10) const;
    RecoveryResult getLastRecoveryResult() const;

private:
    void recoveryLoop();
    void processRecoveryQueue();
    RecoveryResult executeRecoveryStep(const RecoveryStep& step, int attempt);

    // Recovery actions
    bool actionRestartSession();
    bool actionRenewDHCP();
    bool actionResetInterface();
    bool actionScanNetwork();
    bool actionRebootModem();
    bool actionWaitDelay(int delay_ms);
    bool actionEscalate();

    QMISessionHandler* m_session_handler;
    InterfaceController* m_interface_controller;
    ConnectivityMonitor* m_connectivity_monitor;

    std::map<FailureType, RecoveryPlan> m_recovery_plans;
    std::queue<FailureEvent> m_recovery_queue;
    std::vector<RecoveryResult> m_recovery_history;

    RecoveryCallback m_recovery_callback;

    std::unique_ptr<std::thread> m_recovery_thread;
    std::atomic<bool> m_running;
    std::atomic<bool> m_auto_recovery_enabled;
    std::atomic<bool> m_recovery_in_progress;
    std::atomic<int> m_max_concurrent_recoveries;

    mutable std::mutex m_queue_mutex;
    mutable std::mutex m_plans_mutex;
    mutable std::mutex m_history_mutex;
    std::condition_variable m_recovery_cv;

    static const size_t MAX_HISTORY_SIZE = 50;
};

#endif // RECOVERY_ENGINE_H