#include "FactGroup.h"
#include <algorithm>
#include <thread>
#include <chrono>

FactGroup::FactGroup(int updateRateMsecs, const std::string &metaDataFile, bool ignoreCamelCase)
    : _updateRateMSecs(updateRateMsecs)
    , _ignoreCamelCase(ignoreCamelCase)
{
    _setupTimer();
    if (!metaDataFile.empty()) {
        _loadFromJsonArray(metaDataFile);
    }
}

FactGroup::FactGroup(int updateRateMsecs, bool ignoreCamelCase)
    : _updateRateMSecs(updateRateMsecs)
    , _ignoreCamelCase(ignoreCamelCase)
{
    _setupTimer();
}

bool FactGroup::factExists(const std::string &name) const
{
    return _nameToFactMap.find(name) != _nameToFactMap.end();
}

std::shared_ptr<Fact> FactGroup::getFact(const std::string &name) const
{
    auto it = _nameToFactMap.find(name);
    if (it == _nameToFactMap.end()) {
        // Return empty shared_ptr instead of null
        return std::shared_ptr<Fact>();
    }
    return it->second;
}

std::shared_ptr<FactGroup> FactGroup::getFactGroup(const std::string &name) const
{
    auto it = _nameToFactGroupMap.find(name);
    if (it == _nameToFactGroupMap.end()) {
        return std::shared_ptr<FactGroup>();
    }
    return it->second;
}

void FactGroup::setLiveUpdates(bool liveUpdates)
{
    _liveUpdates = liveUpdates;
}

std::vector<std::string> FactGroup::factGroupNames() const
{
    std::vector<std::string> names;
    for (const auto& pair : _nameToFactGroupMap) {
        names.push_back(pair.first);
    }
    return names;
}

void FactGroup::handleMessage(Vehicle *vehicle, const mavlink_message_t &message)
{
    // Default implementation does nothing
    // Subclasses should override to handle specific messages
}

void FactGroup::_updateAllValues()
{
    // Send value changed signals for all facts
    for (const auto& pair : _nameToFactMap) {
        const auto& fact = pair.second;
        if (fact && fact->sendValueChangedSignals()) {
            fact->sendDeferredValueChangedSignal();
        }
    }
    
    // Update all fact groups
    for (const auto& pair : _nameToFactGroupMap) {
        const auto& factGroup = pair.second;
        if (factGroup) {
            factGroup->_updateAllValues();
        }
    }
}

void FactGroup::_addFact(std::shared_ptr<Fact> fact, const std::string &name)
{
    if (!fact) {
        return;
    }
    
    _nameToFactMap[name] = fact;
    
    // Add to fact names list if not already present
    if (std::find(_factNames.begin(), _factNames.end(), name) == _factNames.end()) {
        _factNames.push_back(name);
    }
    
    // Set up fact change callback to propagate to our callback
    fact->setValueChangedCallback([this, name](const Fact* changedFact, const Fact::ValueVariant_t& value) {
        if (_factChangedCallback) {
            _factChangedCallback(this, name, value);
        }
    });
}

void FactGroup::_addFactGroup(std::shared_ptr<FactGroup> factGroup, const std::string &name)
{
    if (!factGroup) {
        return;
    }
    
    _nameToFactGroupMap[name] = factGroup;
    factGroup->setObjectName(name);
}

void FactGroup::_loadFromJsonArray(const std::string &jsonArray)
{
    // TODO: Implement JSON loading when needed
    // For now, this is a placeholder
}

void FactGroup::_setTelemetryAvailable(bool telemetryAvailable)
{
    if (_telemetryAvailable != telemetryAvailable) {
        _telemetryAvailable = telemetryAvailable;
        if (_telemetryAvailableCallback) {
            _telemetryAvailableCallback(this);
        }
    }
}

void FactGroup::_setupTimer()
{
    if (_updateRateMSecs <= 0) {
        return;
    }
    
    _timerRunning = true;
    _timerThread = std::thread([this]() {
        while (_timerRunning) {
            std::this_thread::sleep_for(std::chrono::milliseconds(_updateRateMSecs));
            if (_timerRunning) {
                _updateTimerCallback();
            }
        }
    });
}

std::string FactGroup::_camelCase(const std::string &text)
{
    if (_ignoreCamelCase || text.empty()) {
        return text;
    }
    
    std::string result = text;
    bool capitalizeNext = false;
    
    for (size_t i = 0; i < result.size(); ++i) {
        if (result[i] == '_' || result[i] == ' ') {
            capitalizeNext = true;
            result[i] = ' '; // Replace underscore with space for readability
        } else if (capitalizeNext) {
            result[i] = std::toupper(result[i]);
            capitalizeNext = false;
        }
    }
    
    return result;
}

void FactGroup::_updateTimerCallback()
{
    std::lock_guard<std::mutex> lock(_timerMutex);
    _updateAllValues();
}

// Destructor implementation
FactGroup::~FactGroup()
{
    _timerRunning = false;
    if (_timerThread.joinable()) {
        _timerThread.join();
    }
}
