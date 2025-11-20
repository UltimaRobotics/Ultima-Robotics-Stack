
#ifndef CONNECTION_STATE_MACHINE_H
#define CONNECTION_STATE_MACHINE_H

#include "connection_manager.h"
#include <functional>
#include <map>
#include <thread>
#include <atomic>
#include <chrono>

class QMISessionHandler;
class InterfaceController;

using StateTransitionCallback = std::function<void(ConnectionState, ConnectionState, const std::string&)>;

struct StateTransition {
    ConnectionState from_state;
    ConnectionState to_state;
    std::string trigger;
    std::function<bool()> condition;
    std::function<bool()> action;
};

class ConnectionStateMachine {
public:
    ConnectionStateMachine(QMISessionHandler* session_handler, 
                          InterfaceController* interface_controller);
    ~ConnectionStateMachine();
    
    // State management
    bool initialize();
    void start();
    void stop();
    void reset();
    
    // State transitions
    bool triggerTransition(const std::string& trigger, const std::string& reason = "");
    bool forceState(ConnectionState state, const std::string& reason = "");
    
    // State queries
    ConnectionState getCurrentState() const;
    std::string getCurrentStateString() const;
    std::vector<std::string> getAvailableTransitions() const;
    
    // Configuration
    void setTransitionCallback(StateTransitionCallback callback);
    void setConnectionConfig(const ConnectionConfig& config);
    
    // Timeout management
    void setStateTimeout(ConnectionState state, std::chrono::milliseconds timeout);
    void clearStateTimeout(ConnectionState state);
    
private:
    void stateMachineLoop();
    void processTransitions();
    void handleStateTimeout(ConnectionState state);
    void executeStateEntry(ConnectionState state);
    void executeStateExit(ConnectionState state);
    void triggerNextStateProgression();
    void checkAutomaticProgressions();
    
    // State actions
    bool actionInitialize();
    bool actionStartSession();
    bool actionConfigureInterface();
    bool actionVerifyConnectivity();
    bool actionStartRecovery();
    bool actionCleanup();
    
    // State conditions
    bool conditionModemReady();
    bool conditionSessionActive();
    bool conditionInterfaceConfigured();
    bool conditionConnectivityVerified();
    bool conditionRecoveryComplete();
    
    QMISessionHandler* m_session_handler;
    InterfaceController* m_interface_controller;
    
    ConnectionState m_current_state;
    ConnectionConfig m_config;
    
    std::vector<StateTransition> m_transitions;
    std::map<ConnectionState, std::chrono::milliseconds> m_state_timeouts;
    std::map<ConnectionState, std::chrono::steady_clock::time_point> m_state_entry_times;
    
    StateTransitionCallback m_transition_callback;
    
    std::unique_ptr<std::thread> m_state_thread;
    std::atomic<bool> m_running;
    mutable std::mutex m_state_mutex;
    std::condition_variable m_state_cv;
    
    std::string m_pending_trigger;
    std::string m_pending_reason;
    bool m_trigger_pending;
};

#endif // CONNECTION_STATE_MACHINE_H
