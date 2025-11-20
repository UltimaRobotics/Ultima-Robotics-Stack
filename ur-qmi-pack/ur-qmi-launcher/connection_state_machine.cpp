
#include "connection_state_machine.h"
#include "qmi_session_handler.h"
#include "interface_controller.h"
#include <iostream>
#include <algorithm>
#include <thread>

ConnectionStateMachine::ConnectionStateMachine(QMISessionHandler* session_handler, 
                                             InterfaceController* interface_controller)
    : m_session_handler(session_handler)
    , m_interface_controller(interface_controller)
    , m_current_state(ConnectionState::IDLE)
    , m_running(false)
    , m_trigger_pending(false)
{
    // Initialize state transitions
    m_transitions = {
        {ConnectionState::IDLE, ConnectionState::MODEM_ONLINE, "initialize", 
         [this]() { return conditionModemReady(); }, 
         [this]() { return actionInitialize(); }},
        
        {ConnectionState::MODEM_ONLINE, ConnectionState::SESSION_ACTIVE, "start_session",
         [this]() { return true; },
         [this]() { return actionStartSession(); }},
        
        {ConnectionState::SESSION_ACTIVE, ConnectionState::IP_CONFIGURED, "configure_interface",
         [this]() { return conditionSessionActive(); },
         [this]() { return actionConfigureInterface(); }},
        
        {ConnectionState::IP_CONFIGURED, ConnectionState::CONNECTED, "verify_connectivity",
         [this]() { return conditionInterfaceConfigured(); },
         [this]() { return actionVerifyConnectivity(); }},
        
        {ConnectionState::CONNECTED, ConnectionState::RECONNECTING, "connection_lost",
         [this]() { return !conditionConnectivityVerified(); },
         [this]() { return actionStartRecovery(); }},
        
        {ConnectionState::RECONNECTING, ConnectionState::CONNECTED, "recovery_complete",
         [this]() { return conditionRecoveryComplete(); },
         [this]() { return actionVerifyConnectivity(); }},
        
        {ConnectionState::RECONNECTING, ConnectionState::IDLE, "recovery_failed",
         [this]() { return true; },
         [this]() { return actionCleanup(); }}
    };
    
    // Set default timeouts
    m_state_timeouts[ConnectionState::MODEM_ONLINE] = std::chrono::milliseconds(30000);
    m_state_timeouts[ConnectionState::SESSION_ACTIVE] = std::chrono::milliseconds(60000);
    m_state_timeouts[ConnectionState::IP_CONFIGURED] = std::chrono::milliseconds(30000);
    m_state_timeouts[ConnectionState::RECONNECTING] = std::chrono::milliseconds(120000);
}

ConnectionStateMachine::~ConnectionStateMachine() {
    stop();
}

bool ConnectionStateMachine::initialize() {
    std::lock_guard<std::mutex> lock(m_state_mutex);
    
    if (!m_session_handler || !m_interface_controller) {
        std::cerr << "State machine: Missing required components" << std::endl;
        return false;
    }
    
    m_current_state = ConnectionState::IDLE;
    std::cout << "State machine initialized" << std::endl;
    return true;
}

void ConnectionStateMachine::start() {
    std::lock_guard<std::mutex> lock(m_state_mutex);
    
    if (m_running) {
        return;
    }
    
    m_running = true;
    m_state_thread = std::make_unique<std::thread>(&ConnectionStateMachine::stateMachineLoop, this);
    std::cout << "State machine started" << std::endl;
}

void ConnectionStateMachine::stop() {
    {
        std::lock_guard<std::mutex> lock(m_state_mutex);
        m_running = false;
    }
    
    m_state_cv.notify_all();
    
    if (m_state_thread && m_state_thread->joinable()) {
        m_state_thread->join();
    }
    
    std::cout << "State machine stopped" << std::endl;
}

void ConnectionStateMachine::reset() {
    std::lock_guard<std::mutex> lock(m_state_mutex);
    executeStateExit(m_current_state);
    m_current_state = ConnectionState::IDLE;
    m_state_entry_times.clear();
    executeStateEntry(m_current_state);
}

bool ConnectionStateMachine::triggerTransition(const std::string& trigger, const std::string& reason) {
    std::lock_guard<std::mutex> lock(m_state_mutex);
    
    m_pending_trigger = trigger;
    m_pending_reason = reason;
    m_trigger_pending = true;
    
    m_state_cv.notify_one();
    return true;
}

bool ConnectionStateMachine::forceState(ConnectionState state, const std::string& reason) {
    std::lock_guard<std::mutex> lock(m_state_mutex);
    
    if (state != m_current_state) {
        executeStateExit(m_current_state);
        
        ConnectionState old_state = m_current_state;
        m_current_state = state;
        
        if (m_transition_callback) {
            m_transition_callback(old_state, state, reason);
        }
        
        executeStateEntry(m_current_state);
        std::cout << "Forced state transition: " << static_cast<int>(old_state) 
                  << " -> " << static_cast<int>(state) << " (" << reason << ")" << std::endl;
    }
    
    return true;
}

ConnectionState ConnectionStateMachine::getCurrentState() const {
    std::lock_guard<std::mutex> lock(m_state_mutex);
    return m_current_state;
}

std::string ConnectionStateMachine::getCurrentStateString() const {
    ConnectionState state = getCurrentState();
    switch (state) {
        case ConnectionState::IDLE: return "IDLE";
        case ConnectionState::MODEM_ONLINE: return "MODEM_ONLINE";
        case ConnectionState::SESSION_ACTIVE: return "SESSION_ACTIVE";
        case ConnectionState::IP_CONFIGURED: return "IP_CONFIGURED";
        case ConnectionState::CONNECTED: return "CONNECTED";
        case ConnectionState::RECONNECTING: return "RECONNECTING";
        case ConnectionState::ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

std::vector<std::string> ConnectionStateMachine::getAvailableTransitions() const {
    std::lock_guard<std::mutex> lock(m_state_mutex);
    std::vector<std::string> transitions;
    
    for (const auto& transition : m_transitions) {
        if (transition.from_state == m_current_state && transition.condition()) {
            transitions.push_back(transition.trigger);
        }
    }
    
    return transitions;
}

void ConnectionStateMachine::setTransitionCallback(StateTransitionCallback callback) {
    m_transition_callback = callback;
}

void ConnectionStateMachine::setConnectionConfig(const ConnectionConfig& config) {
    std::lock_guard<std::mutex> lock(m_state_mutex);
    m_config = config;
}

void ConnectionStateMachine::setStateTimeout(ConnectionState state, std::chrono::milliseconds timeout) {
    std::lock_guard<std::mutex> lock(m_state_mutex);
    m_state_timeouts[state] = timeout;
}

void ConnectionStateMachine::clearStateTimeout(ConnectionState state) {
    std::lock_guard<std::mutex> lock(m_state_mutex);
    m_state_timeouts.erase(state);
}

void ConnectionStateMachine::stateMachineLoop() {
    while (m_running) {
        std::unique_lock<std::mutex> lock(m_state_mutex);
        
        // Check for timeouts
        auto now = std::chrono::steady_clock::now();
        auto entry_time_it = m_state_entry_times.find(m_current_state);
        auto timeout_it = m_state_timeouts.find(m_current_state);
        
        if (entry_time_it != m_state_entry_times.end() && timeout_it != m_state_timeouts.end()) {
            auto elapsed = now - entry_time_it->second;
            if (elapsed > timeout_it->second) {
                std::cout << "State timeout detected after " 
                          << std::chrono::duration_cast<std::chrono::seconds>(elapsed).count() 
                          << " seconds" << std::endl;
                handleStateTimeout(m_current_state);
                continue;
            }
        }
        
        // Process pending transitions
        if (m_trigger_pending) {
            std::cout << "Processing pending trigger: " << m_pending_trigger << std::endl;
            processTransitions();
            m_trigger_pending = false;
        } else {
            // Check for automatic state progressions
            checkAutomaticProgressions();
        }
        
        lock.unlock();
        
        // Reduce wait time to make state machine more responsive
        std::unique_lock<std::mutex> wait_lock(m_state_mutex);
        m_state_cv.wait_for(wait_lock, std::chrono::milliseconds(500));
    }
}

void ConnectionStateMachine::processTransitions() {
    std::cout << "Processing transitions for current state: " << static_cast<int>(m_current_state) 
              << ", trigger: " << m_pending_trigger << std::endl;
    
    bool transition_executed = false;
    
    for (const auto& transition : m_transitions) {
        if (transition.from_state == m_current_state && 
            transition.trigger == m_pending_trigger) {
            
            std::cout << "Found matching transition, checking condition..." << std::endl;
            
            if (transition.condition()) {
                std::cout << "Condition passed, executing transition action..." << std::endl;
                executeStateExit(m_current_state);
                
                if (transition.action()) {
                    ConnectionState old_state = m_current_state;
                    m_current_state = transition.to_state;
                    
                    std::cout << "Action succeeded, transition complete" << std::endl;
                    
                    if (m_transition_callback) {
                        m_transition_callback(old_state, m_current_state, m_pending_reason);
                    }
                    
                    executeStateEntry(m_current_state);
                    std::cout << "State transition: " << static_cast<int>(old_state) 
                              << " -> " << static_cast<int>(m_current_state) 
                              << " (" << m_pending_reason << ")" << std::endl;
                    
                    transition_executed = true;
                    
                    // Automatically trigger next state progression
                    triggerNextStateProgression();
                } else {
                    // Action failed, transition to error state
                    std::cout << "Transition action failed!" << std::endl;
                    m_current_state = ConnectionState::ERROR;
                    executeStateEntry(m_current_state);
                    std::cerr << "State transition action failed, entering error state" << std::endl;
                    transition_executed = true;
                }
                break;
            } else {
                std::cout << "Condition failed for transition" << std::endl;
            }
        }
    }
    
    if (!transition_executed) {
        std::cout << "No matching transition found for trigger: " << m_pending_trigger 
                  << " in state: " << static_cast<int>(m_current_state) << std::endl;
    }
}

void ConnectionStateMachine::handleStateTimeout(ConnectionState state) {
    std::cout << "State timeout for state: " << static_cast<int>(state) << std::endl;
    
    switch (state) {
        case ConnectionState::MODEM_ONLINE:
        case ConnectionState::SESSION_ACTIVE:
        case ConnectionState::IP_CONFIGURED:
            forceState(ConnectionState::ERROR, "Timeout in state");
            break;
        case ConnectionState::RECONNECTING:
            forceState(ConnectionState::IDLE, "Recovery timeout");
            break;
        default:
            break;
    }
}

void ConnectionStateMachine::executeStateEntry(ConnectionState state) {
    m_state_entry_times[state] = std::chrono::steady_clock::now();
    std::cout << "Entering state: " << static_cast<int>(state) << std::endl;
}

void ConnectionStateMachine::executeStateExit(ConnectionState state) {
    m_state_entry_times.erase(state);
    std::cout << "Exiting state: " << static_cast<int>(state) << std::endl;
}

void ConnectionStateMachine::triggerNextStateProgression() {
    // Automatically trigger the next logical state transition
    switch (m_current_state) {
        case ConnectionState::MODEM_ONLINE:
            // After successfully initializing, start the data session
            std::cout << "Auto-triggering start_session from MODEM_ONLINE state" << std::endl;
            m_pending_trigger = "start_session";
            m_pending_reason = "Automatic progression";
            m_trigger_pending = true;
            break;
            
        case ConnectionState::SESSION_ACTIVE:
            // After starting session, configure the interface
            std::cout << "Auto-triggering configure_interface from SESSION_ACTIVE state" << std::endl;
            m_pending_trigger = "configure_interface";
            m_pending_reason = "Automatic progression";
            m_trigger_pending = true;
            break;
            
        case ConnectionState::IP_CONFIGURED:
            // After configuring interface, verify connectivity
            std::cout << "Auto-triggering verify_connectivity from IP_CONFIGURED state" << std::endl;
            m_pending_trigger = "verify_connectivity";
            m_pending_reason = "Automatic progression";
            m_trigger_pending = true;
            break;
            
        case ConnectionState::CONNECTED:
            // Connected is the final state for normal operation
            std::cout << "Reached CONNECTED state - no auto progression needed" << std::endl;
            break;
            
        default:
            // No automatic progression for other states
            break;
    }
}

void ConnectionStateMachine::checkAutomaticProgressions() {
    // Check if we should automatically progress from current state
    // This handles cases where no explicit trigger was set but conditions are met
    
    bool should_progress = false;
    std::string next_trigger = "";
    
    switch (m_current_state) {
        case ConnectionState::MODEM_ONLINE:
            // If we're in MODEM_ONLINE and the modem is ready, we should start the session
            if (conditionModemReady()) {
                next_trigger = "start_session";
                should_progress = true;
            }
            break;
            
        case ConnectionState::SESSION_ACTIVE:
            // If we're in SESSION_ACTIVE and session is ready, configure the interface
            if (conditionSessionActive()) {
                next_trigger = "configure_interface";
                should_progress = true;
            }
            break;
            
        case ConnectionState::IP_CONFIGURED:
            // If we're in IP_CONFIGURED and interface is ready, verify connectivity
            if (conditionInterfaceConfigured()) {
                next_trigger = "verify_connectivity";
                should_progress = true;
            }
            break;
            
        default:
            // No automatic progression for other states
            break;
    }
    
    if (should_progress && !next_trigger.empty()) {
        std::cout << "Auto-checking conditions: triggering " << next_trigger 
                  << " for state " << static_cast<int>(m_current_state) << std::endl;
        m_pending_trigger = next_trigger;
        m_pending_reason = "Automatic condition check";
        m_trigger_pending = true;
        m_state_cv.notify_one();
    }
}

// State actions
bool ConnectionStateMachine::actionInitialize() {
    std::cout << "Executing actionInitialize..." << std::endl;
    
    if (!m_session_handler) {
        std::cerr << "Session handler is null!" << std::endl;
        return false;
    }
    
    std::cout << "Calling session handler initialize..." << std::endl;
    bool result = m_session_handler->initialize();
    
    if (result) {
        std::cout << "Session handler initialize: SUCCESS" << std::endl;
        std::cout << "Checking if modem is ready..." << std::endl;
        
        // Verify modem is ready after initialization
        if (m_session_handler->isModemReady()) {
            std::cout << "Modem ready check: PASSED" << std::endl;
        } else {
            std::cout << "Modem ready check: FAILED - but continuing anyway" << std::endl;
        }
    } else {
        std::cout << "Session handler initialize: FAILED" << std::endl;
    }
    
    return result;
}

bool ConnectionStateMachine::actionStartSession() {
    if (!m_session_handler) return false;
    return m_session_handler->startDataSession(m_config.apn, m_config.ip_type, 
                                              m_config.username, m_config.password);
}

bool ConnectionStateMachine::actionConfigureInterface() {
    if (!m_interface_controller || !m_session_handler) return false;
    
    auto settings = m_session_handler->getCurrentSettings();
    InterfaceConfig config;
    config.interface_name = settings.interface_name;
    config.ip_address = settings.ip_address;
    config.gateway = settings.gateway;
    config.dns_primary = settings.dns_primary;
    config.dns_secondary = settings.dns_secondary;
    config.use_dhcp = false;
    config.mtu = 1500;
    
    return m_interface_controller->configureInterface(config);
}

bool ConnectionStateMachine::actionVerifyConnectivity() {
    if (!m_interface_controller) return false;
    
    bool connectivity_result = m_interface_controller->testConnectivity();
    
    if (connectivity_result) {
        std::cout << "Connectivity verified, starting IP monitoring..." << std::endl;
        // Trigger IP monitoring start in the connection manager
        // This will be handled by the connection manager after state transition
    }
    
    return connectivity_result;
}

bool ConnectionStateMachine::actionStartRecovery() {
    // Basic recovery: restart session
    if (m_session_handler) {
        m_session_handler->stopDataSession();
        std::this_thread::sleep_for(std::chrono::seconds(2));
        return m_session_handler->startDataSession(m_config.apn, m_config.ip_type, 
                                                  m_config.username, m_config.password);
    }
    return false;
}

bool ConnectionStateMachine::actionCleanup() {
    if (m_session_handler) {
        m_session_handler->stopDataSession();
    }
    return true;
}

// State conditions
bool ConnectionStateMachine::conditionModemReady() {
    if (!m_session_handler) {
        std::cout << "conditionModemReady: Session handler is null" << std::endl;
        return false;
    }
    
    bool is_ready = m_session_handler->isModemReady();
    std::cout << "conditionModemReady: " << (is_ready ? "TRUE" : "FALSE") << std::endl;
    return is_ready;
}

bool ConnectionStateMachine::conditionSessionActive() {
    return m_session_handler && m_session_handler->isSessionActive();
}

bool ConnectionStateMachine::conditionInterfaceConfigured() {
    if (!m_interface_controller || !m_session_handler) return false;
    
    auto settings = m_session_handler->getCurrentSettings();
    if (settings.interface_name.empty()) return false;
    
    auto status = m_interface_controller->getInterfaceStatus(settings.interface_name);
    return status.is_up && status.has_ip;
}

bool ConnectionStateMachine::conditionConnectivityVerified() {
    return m_interface_controller && m_interface_controller->testConnectivity();
}

bool ConnectionStateMachine::conditionRecoveryComplete() {
    return conditionSessionActive() && conditionInterfaceConfigured() && conditionConnectivityVerified();
}
