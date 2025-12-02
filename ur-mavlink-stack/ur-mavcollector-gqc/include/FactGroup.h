#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <chrono>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>

#include "Fact.h"

// MAVLink headers for mavlink_message_t
#include "../thirdparty/c_library_v2/common/mavlink.h"

// Forward declarations
class Vehicle;

/// Used to group Facts together into an object hierarchy.
/// This is a Qt-free port of QGroundControl's FactGroup.
class FactGroup
{
public:
    explicit FactGroup(int updateRateMsecs, const std::string &metaDataFile, bool ignoreCamelCase = false);
    explicit FactGroup(int updateRateMsecs, bool ignoreCamelCase = false);
    virtual ~FactGroup();

    /// @ return true: if the fact exists in the group
    bool factExists(const std::string &name) const;

    /// @return Fact for specified name, NULL if not found
    /// Note: Requesting a fact which doesn't exists is considered an internal error and will spit out a qWarning
    std::shared_ptr<Fact> getFact(const std::string &name) const;

    /// @return FactGroup for specified name, NULL if not found
    /// Note: Requesting a fact group which doesn't exists is considered an internal error and will spit out a qWarning
    std::shared_ptr<FactGroup> getFactGroup(const std::string &name) const;

    /// Turning on live updates will allow value changes to flow through as they are received.
    void setLiveUpdates(bool liveUpdates);

    std::vector<std::string> factNames() const { return _factNames; }
    std::vector<std::string> factGroupNames() const;
    bool telemetryAvailable() const { return _telemetryAvailable; }
    const std::map<std::string, std::shared_ptr<FactGroup>>& factGroups() const { return _nameToFactGroupMap; }

    /// Allows a FactGroup to parse incoming messages and fill in values
    virtual void handleMessage(Vehicle *vehicle, const mavlink_message_t &message);

    // Callback support for Qt-free implementation
    typedef std::function<void(const FactGroup*)> TelemetryAvailableCallback;
    void setTelemetryAvailableCallback(TelemetryAvailableCallback callback) { _telemetryAvailableCallback = callback; }

    typedef std::function<void(const FactGroup*, const std::string&, const Fact::ValueVariant_t&)> FactChangedCallback;
    void setFactChangedCallback(FactChangedCallback callback) { _factChangedCallback = callback; }

protected:
    virtual void _updateAllValues();

    void _addFact(std::shared_ptr<Fact> fact, const std::string &name);
    void _addFact(std::shared_ptr<Fact> fact) { _addFact(fact, fact->name()); }
    void _addFactGroup(std::shared_ptr<FactGroup> factGroup, const std::string &name);
    void _addFactGroup(std::shared_ptr<FactGroup> factGroup) { _addFactGroup(factGroup, factGroup->objectName()); }
    void _loadFromJsonArray(const std::string &jsonArray);
    void _setTelemetryAvailable(bool telemetryAvailable);

    const int _updateRateMSecs = 0;   ///< Update rate for Fact::valueChanged signals, 0: immediate update

    std::map<std::string, std::shared_ptr<Fact>> _nameToFactMap;
    std::map<std::string, std::shared_ptr<FactGroup>> _nameToFactGroupMap;
    std::map<std::string, std::shared_ptr<FactMetaData>> _nameToFactMetaDataMap;
    std::vector<std::string> _factNames;

private:
    void _setupTimer();
    std::string _camelCase(const std::string &text);
    void _updateTimerCallback();

    std::chrono::steady_clock::time_point _lastUpdateTime;
    const bool _ignoreCamelCase = false;
    bool _telemetryAvailable = false;
    bool _liveUpdates = false;
    TelemetryAvailableCallback _telemetryAvailableCallback;
    FactChangedCallback _factChangedCallback;

    // Timer simulation for Qt-free implementation
    std::thread _timerThread;
    std::atomic<bool> _timerRunning{false};
    std::mutex _timerMutex;

    std::string _objectName;
public:
    std::string objectName() const { return _objectName; }
    void setObjectName(const std::string &name) { _objectName = name; }
};