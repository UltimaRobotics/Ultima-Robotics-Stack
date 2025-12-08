#ifndef URWT_STATE_STATE_TRANSITION_HPP
#define URWT_STATE_STATE_TRANSITION_HPP

#include <vector>
#include <string>
#include <functional>
#include <json.hpp>
#include "urwt/state/wireless_state.hpp"
#include "urwt/config/wireless_config_types.hpp"
#include "urwt/utils/result.hpp"

namespace urwt {
namespace state {

enum class TransitionAction {
    None,
    EnableHardware,
    DisableHardware,
    BringInterfaceUp,
    BringInterfaceDown,
    DisconnectNetwork,
    StopAPMode,
    SwitchToSTAMode,
    SwitchToAPMode,
    ConnectToNetwork,
    StartAPMode,
    EnableAutomation,
    DisableAutomation
};

struct TransitionStep {
    TransitionAction action;
    std::string description;
    int priority{5};
    bool critical{false};

    std::function<Result<bool, std::string>()> executor;

    TransitionStep() : action(TransitionAction::None) {}
    TransitionStep(TransitionAction a, const std::string& desc, int prio = 5, bool crit = false)
        : action(a), description(desc), priority(prio), critical(crit) {}

    bool operator<(const TransitionStep& other) const {
        return priority < other.priority;
    }
};

struct StateTransitionPlan {
    SystemWirelessState from_state;
    config::WirelessConfig to_config;

    std::vector<TransitionStep> steps;
    bool requires_restart{false};
    int estimated_duration_seconds{0};

    StateTransitionPlan() = default;

    bool isEmpty() const {
        return steps.empty();
    }

    size_t stepCount() const {
        return steps.size();
    }
};

void to_json(nlohmann::json& j, const StateTransitionPlan& plan);
std::string transitionActionToString(TransitionAction action);

}
}

#endif
