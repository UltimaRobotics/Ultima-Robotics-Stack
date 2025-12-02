#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>

#include "Fact.h"
#include "FactMetaData.h"

// MAVLink headers for mavlink_param_union_t
#include "../thirdparty/c_library_v2/common/mavlink.h"

// Forward declarations
class Vehicle;

/// ParameterManager handles vehicle parameters using the Fact system.
/// This is a Qt-free port of QGroundControl's ParameterManager.
class ParameterManager
{
public:
    // Type definitions for cache system - needed before method declarations
    typedef std::pair<FactMetaData::ValueType_t, Fact::ValueVariant_t> ParamTypeVal;
    typedef std::map<std::string, ParamTypeVal> CacheMapName2ParamTypeVal;

    ParameterManager(Vehicle *vehicle);
    ~ParameterManager();

    bool parametersReady() const { return _parametersReady; }
    bool missingParameters() const { return _missingParameters; }
    double loadProgress() const { return _loadProgress; }

    /// @return Location of parameter cache file
    static std::string parameterCacheFile(int vehicleId, int componentId);

    void mavlinkMessageReceived(const mavlink_message_t &message);

    std::vector<int> componentIds() const;

    /// Re-request the full set of parameters from the autopilot
    void refreshAllParameters(uint8_t componentID = 0);

    /// Request a refresh on the specific parameter
    void refreshParameter(int componentId, const std::string &paramName);

    /// Request a refresh on all parameters that begin with the specified prefix
    void refreshParametersPrefix(int componentId, const std::string &namePrefix);

    void resetAllParametersToDefaults();
    void resetAllToVehicleConfiguration();

    /// Returns true if the specified parameter exists
    ///     @param componentId: Component id or ParameterManager::defaultComponentId
    ///     @param name: Parameter name
    bool parameterExists(int componentId, const std::string &paramName) const;

    /// Returns all parameter names
    std::vector<std::string> parameterNames(int componentId) const;

    /// Returns the specified Parameter. Returns a default empty fact if parameter does not exist. Also will pop
    /// a missing parameter error to user if parameter does not exist.
    ///     @param componentId: Component id or ParameterManager::defaultComponentId
    ///     @param name: Parameter name
    std::shared_ptr<Fact> getParameter(int componentId, const std::string &paramName);

    /// Returns error messages from loading
    std::string readParametersFromStream(std::istream &stream);

    void writeParametersToStream(std::ostream &stream) const;

    bool pendingWrites() const;

    Vehicle *vehicle() { return _vehicle; }

    static uint8_t factTypeToMavType(FactMetaData::ValueType_t factType);
    static FactMetaData::ValueType_t mavTypeToFactType(uint8_t mavType);

    static constexpr int defaultComponentId = -1;

    // These are public for creating unit tests
    static constexpr int kParamSetRetryCount = 2;                   ///< Number of retries for PARAM_SET
    static constexpr int kParamRequestReadRetryCount = 2;           ///< Number of retries for PARAM_REQUEST_READ
    static constexpr int kWaitForParamValueAckMs = 1000;    ///< Time to wait for param value ack after set param

    // Callback support for Qt-free implementation
    typedef std::function<void(bool)> ParametersReadyCallback;
    void setParametersReadyCallback(ParametersReadyCallback callback) { _parametersReadyCallback = callback; }

    typedef std::function<void(double)> LoadProgressCallback;
    void setLoadProgressCallback(LoadProgressCallback callback) { _loadProgressCallback = callback; }

    typedef std::function<void(int, std::shared_ptr<Fact>)> FactAddedCallback;
    void setFactAddedCallback(FactAddedCallback callback) { _factAddedCallback = callback; }

private:
    void _factRawValueUpdated(const std::shared_ptr<Fact> &fact, const Fact::ValueVariant_t &rawValue);

    /// Called whenever a parameter is updated or first seen.
    void _handleParamValue(int componentId, const std::string &parameterName, int parameterCount, int parameterIndex, uint8_t mavParamType, const Fact::ValueVariant_t &parameterValue);
    
    /// Writes the parameter update to mavlink, sets up for write wait
    void _mavlinkParamSet(int componentId, const std::string &name, FactMetaData::ValueType_t valueType, const Fact::ValueVariant_t &rawValue);
    void _tryCacheLookup();
    
    /// Translates ParameterManager::defaultComponentId to real component id if needed
    int _actualComponentId(int componentId) const;
    void _mavlinkParamRequestRead(int componentId, const std::string &paramName, int paramIndex, bool notifyFailure);
    void _loadMetaData();
    void _clearMetaData();
    
    /// Remap a parameter from one firmware version to another
    std::string _remapParamNameToVersion(const std::string &paramName) const;
    bool _fillMavlinkParamUnion(FactMetaData::ValueType_t valueType, const Fact::ValueVariant_t &rawValue, mavlink_param_union_t &paramUnion) const;
    bool _mavlinkParamUnionToVariant(const mavlink_param_union_t &paramUnion, Fact::ValueVariant_t &outValue) const;
    
    /// The offline editing vehicle can have custom loaded params bolted into it.
    void _loadOfflineEditingParams();
    std::string _logVehiclePrefix(int componentId) const;
    void _setLoadProgress(double loadProgress);
    
    /// Requests missing index based parameters from the vehicle.
    ///     @param waitingParamTimeout: true: being called due to timeout, false: being called to re-fill the batch queue
    /// return true: Parameters were requested, false: No more requests needed
    bool _fillIndexBatchQueue(bool waitingParamTimeout);
    void _updateProgressBar();
    void _checkInitialLoadComplete();
    
    /// Timer management methods
    void _startInitialRequestTimer();
    void _startWaitingParamTimer();
    void _stopInitialRequestTimer();
    void _stopWaitingParamTimer();
    void _initialRequestTimeout();
    void _waitingParamTimeout();
    
    /// Cache management methods
    void _tryCacheHashLoad(int vehicleId, int componentId, const Fact::ValueVariant_t &hashValue);
    void _writeLocalParamCache(int vehicleId, int componentId);
    std::string _parameterCacheFile(int vehicleId, int componentId);
    bool _readCacheFile(int vehicleId, int componentId, std::map<std::string, ParamTypeVal> &cacheMap);
    void _writeCacheFile(int vehicleId, int componentId, const std::map<std::string, ParamTypeVal> &cacheMap);
    uint32_t _calculateCacheCRC(const std::map<std::string, ParamTypeVal> &cacheMap);
    
    /// Hash check handling
    void _handleHashCheck(int componentId, const Fact::ValueVariant_t &hashValue);
    void _sendHashAck(int componentId, uint32_t crcValue);
    void _ftpDownloadComplete(const std::string &fileName, const std::string &errorMsg);
    void _ftpDownloadProgress(float progress);
    
    /// Parse the binary parameter file and inject the parameters in the qgc fact system.
    /// See: https://github.com/ArduPilot/ardupilot/tree/master/libraries/AP_Filesystem
    bool _parseParamFile(const std::string &filename);
    void _incrementPendingWriteCount();
    void _decrementPendingWriteCount();
    std::string _vehicleAndComponentString(int componentId) const;

    static Fact::ValueVariant_t _stringToTypedVariant(const std::string &string, FactMetaData::ValueType_t type, bool failOk = false);

    Vehicle *_vehicle = nullptr;

    std::map<int /* comp id */, std::map<std::string /* parameter name */, std::shared_ptr<Fact>>> _mapCompId2FactMap;
    mutable std::mutex _paramMutex;

    double _loadProgress = 0;                   ///< Parameter load progress, [0.0,1.0]
    bool _parametersReady = false;              ///< true: parameter load complete
    bool _missingParameters = false;            ///< true: parameter missing from initial load
    bool _initialLoadComplete = false;          ///< true: Initial load of all parameters complete, whether successful or not
    bool _waitingForDefaultComponent = false;   ///< true: last chance wait for default component params
    bool _metaDataAddedToFacts = false;         ///< true: FactMetaData has been added to the default component facts
    bool _logReplay = false;                    ///< true: running with log replay link

    std::map<int /* component id */, bool> _debugCacheCRC; ///< true: debug cache crc failure
    std::map<int /* component id */, CacheMapName2ParamTypeVal> _debugCacheMap;
    std::map<int /* component id */, std::map<std::string, bool>> _debugCacheParamSeen;

    // Wait counts from previous parameter update cycle
    int _prevWaitingReadParamIndexCount = 0;

    bool _readParamIndexProgressActive = false;

    static constexpr int _maxInitialRequestListRetry = 4;       ///< Maximum retries for request list
    int _initialRequestRetryCount = 0;                          ///< Current retry count for request list
    static constexpr int _maxInitialLoadRetrySingleParam = 5;   ///< Maximum retries for initial index based load of a single param
    bool _disableAllRetries = false;                            ///< true: Don't retry any requests (used for testing and logReplay)

    bool _indexBatchQueueActive = false;    ///< true: we are actively batching re-requests for missing index base params, false: index based re-request has not yet started
    std::vector<int> _indexBatchQueue;      ///< The current queue of index re-requests

    std::map<int, int> _paramCountMap;                              ///< Key: Component id, Value: count of parameters in this component
    std::map<int, std::map<int, int>> _waitingReadParamIndexMap;    ///< Key: Component id, Value: Map { Key: parameter index still waiting for, Value: retry count }
    std::map<int, std::vector<int>> _failedReadParamIndexMap;       ///< Key: Component id, Value: failed parameter index

    int _totalParamCount = 0;                   ///< Number of parameters across all components
    int _pendingWritesCount = 0;                ///< Number of parameters with pending writes

    // Timer simulation for Qt-free implementation
    std::thread _initialRequestTimeoutThread;
    std::thread _waitingParamTimeoutThread;
    std::atomic<bool> _timersRunning{false};
    std::mutex _timerMutex;
    
    // Timer state tracking
    std::atomic<bool> _initialRequestTimerActive{false};
    std::atomic<bool> _waitingParamTimerActive{false};
    std::chrono::steady_clock::time_point _initialRequestStartTime;
    std::chrono::steady_clock::time_point _waitingParamStartTime;

    std::shared_ptr<Fact> _defaultFact;   ///< Used to return default fact, when parameter not found

    bool _tryftp = false;

    // Callbacks
    ParametersReadyCallback _parametersReadyCallback;
    LoadProgressCallback _loadProgressCallback;
    FactAddedCallback _factAddedCallback;
};
