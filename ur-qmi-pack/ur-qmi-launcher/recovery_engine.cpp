
#include "recovery_engine.h"
#include "command_logger.h"
#include "qmi_session_handler.h"
#include "interface_controller.h"
#include "connectivity_monitor.h"
#include <iostream>
#include <algorithm>

RecoveryEngine::RecoveryEngine(QMISessionHandler* session_handler,
                              InterfaceController* interface_controller,
                              ConnectivityMonitor* connectivity_monitor)
    : m_session_handler(session_handler)
    , m_interface_controller(interface_controller)
    , m_connectivity_monitor(connectivity_monitor)
    , m_running(false)
    , m_auto_recovery_enabled(true)
    , m_recovery_in_progress(false)
    , m_max_concurrent_recoveries(1)
{
    setDefaultRecoveryPlan();
}

RecoveryEngine::~RecoveryEngine() {
    stopRecoveryEngine();
}

void RecoveryEngine::startRecoveryEngine() {
    std::lock_guard<std::mutex> lock(m_queue_mutex);
    
    if (m_running) {
        return;
    }
    
    m_running = true;
    m_recovery_thread = std::make_unique<std::thread>(&RecoveryEngine::recoveryLoop, this);
    
    std::cout << "Recovery engine started" << std::endl;
}

void RecoveryEngine::stopRecoveryEngine() {
    {
        std::lock_guard<std::mutex> lock(m_queue_mutex);
        m_running = false;
    }
    
    m_recovery_cv.notify_all();
    
    if (m_recovery_thread && m_recovery_thread->joinable()) {
        m_recovery_thread->join();
    }
    
    std::cout << "Recovery engine stopped" << std::endl;
}

bool RecoveryEngine::isRunning() const {
    return m_running;
}

bool RecoveryEngine::triggerRecovery(const FailureEvent& failure) {
    if (!m_auto_recovery_enabled) {
        std::cout << "Auto-recovery disabled, skipping recovery for failure: " 
                  << failure.description << std::endl;
        return false;
    }
    
    if (!failure.auto_recoverable) {
        std::cout << "Failure not auto-recoverable: " << failure.description << std::endl;
        return false;
    }
    
    std::lock_guard<std::mutex> plans_lock(m_plans_mutex);
    auto it = m_recovery_plans.find(failure.type);
    if (it == m_recovery_plans.end()) {
        std::cout << "No recovery plan for failure type: " << static_cast<int>(failure.type) << std::endl;
        return false;
    }
    
    return executeRecoveryPlan(it->second);
}

bool RecoveryEngine::executeRecoveryPlan(const RecoveryPlan& plan) {
    if (m_recovery_in_progress) {
        std::cout << "Recovery already in progress, queuing new recovery" << std::endl;
        return false;
    }
    
    m_recovery_in_progress = true;
    auto start_time = std::chrono::steady_clock::now();
    
    std::cout << "Executing recovery plan for failure type: " 
              << static_cast<int>(plan.failure_type) << std::endl;
    
    bool recovery_successful = false;
    int cycle = 0;
    
    while (cycle < plan.max_cycles && !recovery_successful) {
        cycle++;
        std::cout << "Recovery cycle " << cycle << "/" << plan.max_cycles << std::endl;
        
        bool cycle_successful = true;
        for (const auto& step : plan.steps) {
            std::cout << "Executing recovery step: " << step.description << std::endl;
            
            int attempt = 0;
            bool step_successful = false;
            
            while (attempt < step.max_attempts && !step_successful) {
                attempt++;
                
                auto step_result = executeRecoveryStep(step, attempt);
                step_successful = step_result.success;
                
                if (!step_successful && attempt < step.max_attempts) {
                    std::cout << "Step failed, retrying (attempt " << attempt + 1 
                              << "/" << step.max_attempts << ")" << std::endl;
                    std::this_thread::sleep_for(std::chrono::seconds(2));
                }
            }
            
            if (!step_successful) {
                std::cout << "Recovery step failed after " << step.max_attempts << " attempts" << std::endl;
                cycle_successful = false;
                break;
            }
        }
        
        if (cycle_successful) {
            // Verify recovery success
            std::this_thread::sleep_for(std::chrono::seconds(5));
            
            if (m_connectivity_monitor && m_connectivity_monitor->testConnectivity()) {
                recovery_successful = true;
                std::cout << "Recovery successful!" << std::endl;
            } else if (m_interface_controller && m_interface_controller->testConnectivity()) {
                recovery_successful = true;
                std::cout << "Recovery successful!" << std::endl;
            }
        }
        
        if (!recovery_successful && cycle < plan.max_cycles) {
            std::cout << "Recovery cycle failed, waiting before next cycle..." << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(10));
        }
    }
    
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    // Create recovery result
    RecoveryResult result;
    result.success = recovery_successful;
    result.description = recovery_successful ? "Recovery completed successfully" : "Recovery failed";
    result.duration = duration;
    result.attempts_made = cycle;
    result.original_failure = plan.failure_type;
    
    // Store in history
    {
        std::lock_guard<std::mutex> lock(m_history_mutex);
        m_recovery_history.push_back(result);
        
        if (m_recovery_history.size() > MAX_HISTORY_SIZE) {
            m_recovery_history.erase(m_recovery_history.begin());
        }
    }
    
    // Notify callback
    if (m_recovery_callback) {
        m_recovery_callback(result);
    }
    
    m_recovery_in_progress = false;
    
    if (recovery_successful) {
        std::cout << "Recovery completed successfully in " << duration.count() << "ms" << std::endl;
    } else {
        std::cout << "Recovery failed after " << cycle << " cycles (" << duration.count() << "ms)" << std::endl;
        
        if (plan.escalate_on_failure) {
            std::cout << "Escalating recovery failure..." << std::endl;
            // Implement escalation logic here
        }
    }
    
    return recovery_successful;
}

void RecoveryEngine::addRecoveryToQueue(const FailureEvent& failure) {
    std::lock_guard<std::mutex> lock(m_queue_mutex);
    m_recovery_queue.push(failure);
    m_recovery_cv.notify_one();
    
    std::cout << "Recovery queued for failure: " << failure.description << std::endl;
}

void RecoveryEngine::clearRecoveryQueue() {
    std::lock_guard<std::mutex> lock(m_queue_mutex);
    
    while (!m_recovery_queue.empty()) {
        m_recovery_queue.pop();
    }
    
    std::cout << "Recovery queue cleared" << std::endl;
}

void RecoveryEngine::setRecoveryCallback(RecoveryCallback callback) {
    m_recovery_callback = callback;
}

void RecoveryEngine::addRecoveryPlan(const RecoveryPlan& plan) {
    std::lock_guard<std::mutex> lock(m_plans_mutex);
    m_recovery_plans[plan.failure_type] = plan;
    
    std::cout << "Recovery plan added for failure type: " << static_cast<int>(plan.failure_type) << std::endl;
}

void RecoveryEngine::removeRecoveryPlan(FailureType failure_type) {
    std::lock_guard<std::mutex> lock(m_plans_mutex);
    m_recovery_plans.erase(failure_type);
    
    std::cout << "Recovery plan removed for failure type: " << static_cast<int>(failure_type) << std::endl;
}

void RecoveryEngine::setDefaultRecoveryPlan() {
    // Session recovery plan
    addRecoveryPlan(createSessionRecoveryPlan());
    
    // IP configuration recovery plan
    addRecoveryPlan(createIPRecoveryPlan());
    
    // Connectivity recovery plan
    addRecoveryPlan(createConnectivityRecoveryPlan());
    
    // Signal recovery plan
    addRecoveryPlan(createSignalRecoveryPlan());
    
    // Modem recovery plan
    addRecoveryPlan(createModemRecoveryPlan());
    
    // Interface recovery plan
    addRecoveryPlan(createInterfaceRecoveryPlan());
    
    std::cout << "Default recovery plans configured" << std::endl;
}

void RecoveryEngine::enableAutoRecovery(bool enable) {
    m_auto_recovery_enabled = enable;
    std::cout << "Auto-recovery " << (enable ? "enabled" : "disabled") << std::endl;
}

void RecoveryEngine::setMaxConcurrentRecoveries(int max) {
    m_max_concurrent_recoveries = max;
}

RecoveryPlan RecoveryEngine::createSessionRecoveryPlan() {
    RecoveryPlan plan;
    plan.failure_type = FailureType::SESSION_LOST;
    plan.max_cycles = 3;
    plan.escalate_on_failure = true;
    
    plan.steps = {
        {RecoveryAction::RESTART_SESSION, "Restart QMI data session", 30000, 2, false, nullptr},
        {RecoveryAction::WAIT_DELAY, "Wait for session stabilization", 5000, 1, false, nullptr},
        {RecoveryAction::RENEW_DHCP, "Renew DHCP lease", 30000, 2, false, nullptr}
    };
    
    return plan;
}

RecoveryPlan RecoveryEngine::createIPRecoveryPlan() {
    RecoveryPlan plan;
    plan.failure_type = FailureType::IP_CONFIGURATION_LOST;
    plan.max_cycles = 2;
    plan.escalate_on_failure = true;
    
    plan.steps = {
        {RecoveryAction::RENEW_DHCP, "Renew DHCP lease", 30000, 3, false, nullptr},
        {RecoveryAction::RESET_INTERFACE, "Reset network interface", 15000, 2, false, nullptr},
        {RecoveryAction::RESTART_SESSION, "Restart session if needed", 30000, 1, false, nullptr}
    };
    
    return plan;
}

RecoveryPlan RecoveryEngine::createConnectivityRecoveryPlan() {
    RecoveryPlan plan;
    plan.failure_type = FailureType::CONNECTIVITY_LOST;
    plan.max_cycles = 2;
    plan.escalate_on_failure = false;
    
    plan.steps = {
        {RecoveryAction::WAIT_DELAY, "Wait for temporary connectivity issues", 10000, 1, false, nullptr},
        {RecoveryAction::RENEW_DHCP, "Renew DHCP lease", 30000, 2, false, nullptr},
        {RecoveryAction::RESTART_SESSION, "Restart data session", 30000, 1, false, nullptr}
    };
    
    return plan;
}

RecoveryPlan RecoveryEngine::createSignalRecoveryPlan() {
    RecoveryPlan plan;
    plan.failure_type = FailureType::SIGNAL_WEAK;
    plan.max_cycles = 1;
    plan.escalate_on_failure = false;
    
    plan.steps = {
        {RecoveryAction::SCAN_NETWORK, "Scan for better signal", 60000, 2, false, nullptr},
        {RecoveryAction::WAIT_DELAY, "Wait for signal improvement", 30000, 1, false, nullptr}
    };
    
    return plan;
}

RecoveryPlan RecoveryEngine::createModemRecoveryPlan() {
    RecoveryPlan plan;
    plan.failure_type = FailureType::MODEM_UNRESPONSIVE;
    plan.max_cycles = 2;
    plan.escalate_on_failure = true;
    
    plan.steps = {
        {RecoveryAction::REBOOT_MODEM, "Reboot modem", 60000, 1, false, nullptr},
        {RecoveryAction::WAIT_DELAY, "Wait for modem initialization", 30000, 1, false, nullptr},
        {RecoveryAction::RESTART_SESSION, "Restart session after reboot", 30000, 2, false, nullptr}
    };
    
    return plan;
}

RecoveryPlan RecoveryEngine::createInterfaceRecoveryPlan() {
    RecoveryPlan plan;
    plan.failure_type = FailureType::INTERFACE_DOWN;
    plan.max_cycles = 2;
    plan.escalate_on_failure = true;
    
    plan.steps = {
        {RecoveryAction::RESET_INTERFACE, "Reset network interface", 15000, 2, false, nullptr},
        {RecoveryAction::RENEW_DHCP, "Reconfigure IP", 30000, 2, false, nullptr}
    };
    
    return plan;
}

bool RecoveryEngine::isRecoveryInProgress() const {
    return m_recovery_in_progress;
}

int RecoveryEngine::getQueueSize() const {
    std::lock_guard<std::mutex> lock(m_queue_mutex);
    return m_recovery_queue.size();
}

std::vector<RecoveryResult> RecoveryEngine::getRecentResults(int count) const {
    std::lock_guard<std::mutex> lock(m_history_mutex);
    
    if (m_recovery_history.size() <= static_cast<size_t>(count)) {
        return m_recovery_history;
    }
    
    return std::vector<RecoveryResult>(
        m_recovery_history.end() - count, m_recovery_history.end());
}

RecoveryResult RecoveryEngine::getLastRecoveryResult() const {
    std::lock_guard<std::mutex> lock(m_history_mutex);
    
    if (m_recovery_history.empty()) {
        return RecoveryResult{false, "No recovery attempts", std::chrono::milliseconds(0), 0, FailureType::SESSION_LOST};
    }
    
    return m_recovery_history.back();
}

void RecoveryEngine::recoveryLoop() {
    while (m_running) {
        processRecoveryQueue();
        
        std::unique_lock<std::mutex> lock(m_queue_mutex);
        m_recovery_cv.wait_for(lock, std::chrono::seconds(5),
                              [this] { return !m_running || !m_recovery_queue.empty(); });
    }
}

void RecoveryEngine::processRecoveryQueue() {
    std::unique_lock<std::mutex> lock(m_queue_mutex);
    
    if (m_recovery_queue.empty()) {
        return;
    }
    
    FailureEvent failure = m_recovery_queue.front();
    m_recovery_queue.pop();
    lock.unlock();
    
    triggerRecovery(failure);
}

RecoveryResult RecoveryEngine::executeRecoveryStep(const RecoveryStep& step, int attempt) {
    RecoveryResult result;
    result.success = false;
    result.description = step.description;
    result.attempts_made = attempt;
    
    auto start_time = std::chrono::steady_clock::now();
    
    try {
        switch (step.action) {
            case RecoveryAction::RESTART_SESSION:
                result.success = actionRestartSession();
                break;
            case RecoveryAction::RENEW_DHCP:
                result.success = actionRenewDHCP();
                break;
            case RecoveryAction::RESET_INTERFACE:
                result.success = actionResetInterface();
                break;
            case RecoveryAction::SCAN_NETWORK:
                result.success = actionScanNetwork();
                break;
            case RecoveryAction::REBOOT_MODEM:
                result.success = actionRebootModem();
                break;
            case RecoveryAction::WAIT_DELAY:
                result.success = actionWaitDelay(step.timeout_ms);
                break;
            case RecoveryAction::ESCALATE:
                result.success = actionEscalate();
                break;
        }
        
        if (step.custom_action) {
            result.success = step.custom_action();
        }
    } catch (const std::exception& e) {
        result.success = false;
        result.description += " (Exception: " + std::string(e.what()) + ")";
    }
    
    auto end_time = std::chrono::steady_clock::now();
    result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    return result;
}

bool RecoveryEngine::actionRestartSession() {
    if (!m_session_handler) {
        return false;
    }
    
    std::cout << "Stopping current data session..." << std::endl;
    m_session_handler->stopDataSession();
    
    std::this_thread::sleep_for(std::chrono::seconds(3));
    
    std::cout << "Starting new data session..." << std::endl;
    // Use a basic configuration for recovery
    return m_session_handler->startDataSession("internet", 4, "", "");
}

bool RecoveryEngine::actionRenewDHCP() {
    if (!m_interface_controller || !m_session_handler) {
        return false;
    }
    
    auto settings = m_session_handler->getCurrentSettings();
    if (settings.interface_name.empty()) {
        return false;
    }
    
    return m_interface_controller->renewDHCP(settings.interface_name);
}

bool RecoveryEngine::actionResetInterface() {
    if (!m_interface_controller || !m_session_handler) {
        return false;
    }
    
    auto settings = m_session_handler->getCurrentSettings();
    if (settings.interface_name.empty()) {
        return false;
    }
    
    return m_interface_controller->resetInterface(settings.interface_name);
}

bool RecoveryEngine::actionScanNetwork() {
    if (!m_session_handler) {
        return false;
    }
    
    // Trigger network scan - this is a simplified implementation
    std::string cmd = "qmicli -d " + m_session_handler->getDeviceInfo().device_path + 
                     " --nas-network-scan --timeout=30";
    
    CommandLogger::logCommand(cmd);
    int result = system(cmd.c_str());
    int exit_code = WEXITSTATUS(result);
    CommandLogger::logCommandResult(cmd, (exit_code == 0) ? "SUCCESS" : "FAILED", exit_code);
    return exit_code == 0;
}

bool RecoveryEngine::actionRebootModem() {
    if (!m_session_handler) {
        return false;
    }
    
    // Stop current session first
    m_session_handler->stopDataSession();
    
    // Reset modem
    std::string cmd = "qmicli -d " + m_session_handler->getDeviceInfo().device_path + 
                     " --dms-set-operating-mode=reset";
    
    CommandLogger::logCommand(cmd);
    int result = system(cmd.c_str());
    int exit_code = WEXITSTATUS(result);
    CommandLogger::logCommandResult(cmd, (exit_code == 0) ? "SUCCESS" : "FAILED", exit_code);
    if (exit_code != 0) {
        return false;
    }
    
    // Wait for modem to come back online
    std::this_thread::sleep_for(std::chrono::seconds(30));
    
    // Verify modem is ready
    for (int i = 0; i < 30; ++i) {
        if (m_session_handler->isModemReady()) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
    
    return false;
}

bool RecoveryEngine::actionWaitDelay(int delay_ms) {
    std::cout << "Waiting " << delay_ms << "ms..." << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
    return true;
}

bool RecoveryEngine::actionEscalate() {
    std::cout << "Escalating recovery failure - manual intervention may be required" << std::endl;
    // This could send notifications, create tickets, etc.
    return false; // Escalation doesn't "succeed" in the recovery sense
}
