#include "ParameterManager.h"
#include "Vehicle.h"
#include <mavlink/v2.0/common/mavlink.h>
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <filesystem>
#include <cstring>

ParameterManager::ParameterManager(Vehicle *vehicle)
    : _vehicle(vehicle)
    , _timersRunning(true)
{
    _defaultFact = std::make_shared<Fact>();
    
    // Create cache directory
    std::filesystem::create_directories("ParamCache");
    
    std::cout << "ParameterManager initialized" << std::endl;
}

ParameterManager::~ParameterManager()
{
    _timersRunning = false;
    if (_initialRequestTimeoutThread.joinable()) {
        _initialRequestTimeoutThread.join();
    }
    if (_waitingParamTimeoutThread.joinable()) {
        _waitingParamTimeoutThread.join();
    }
}

void ParameterManager::mavlinkMessageReceived(const mavlink_message_t &message)
{
    switch (message.msgid) {
        case MAVLINK_MSG_ID_PARAM_VALUE:
            {
                mavlink_param_value_t paramValue;
                mavlink_msg_param_value_decode(&message, &paramValue);
                
                std::string paramName(reinterpret_cast<char*>(paramValue.param_id));
                mavlink_param_union_t paramUnion;
                paramUnion.param_float = paramValue.param_value;
                paramUnion.type = paramValue.param_type;
                
                Fact::ValueVariant_t variantValue;
                if (_mavlinkParamUnionToVariant(paramUnion, variantValue)) {
                    _handleParamValue(message.compid, paramName, paramValue.param_count, 
                                    paramValue.param_index, paramValue.param_type, 
                                    variantValue);
                }
            }
            break;
            
        case MAVLINK_MSG_ID_PARAM_SET:
            // Handle parameter set acknowledgment
            break;
            
        default:
            break;
    }
}

std::vector<int> ParameterManager::componentIds() const
{
    std::vector<int> componentIds;
    std::lock_guard<std::mutex> lock(_paramMutex);
    
    for (const auto& pair : _mapCompId2FactMap) {
        componentIds.push_back(pair.first);
    }
    
    return componentIds;
}

void ParameterManager::refreshAllParameters(uint8_t componentID)
{
    if (!_vehicle) {
        return;
    }
    
    int actualComponentId = _actualComponentId(componentID);
    
    // Reset loading state
    _parametersReady = false;
    _loadProgress = 0.0;
    _initialLoadComplete = false;
    _missingParameters = false;
    _initialRequestRetryCount = 0;
    _waitingForDefaultComponent = false;
    
    // Clear existing parameters for this component
    {
        std::lock_guard<std::mutex> lock(_paramMutex);
        if (componentID == 0) {
            _mapCompId2FactMap.clear();
            _paramCountMap.clear();
            _waitingReadParamIndexMap.clear();
            _indexBatchQueue.clear();
            _indexBatchQueueActive = false;
            _totalParamCount = 0;
        } else {
            _mapCompId2FactMap.erase(actualComponentId);
            _paramCountMap.erase(actualComponentId);
            _waitingReadParamIndexMap.erase(actualComponentId);
        }
    }
    
    _setLoadProgress(0.0);
    
    // Start initial request timer
    _startInitialRequestTimer();
    
    // Send PARAM_REQUEST_LIST to request all parameters
    mavlink_message_t message;
    mavlink_msg_param_request_list_pack(255, MAV_COMP_ID_MISSIONPLANNER, &message,
                                       actualComponentId, actualComponentId);
    
    _vehicle->sendMessage(message);
    
    std::cout << "Requesting all parameters for component " << actualComponentId << std::endl;
}

void ParameterManager::refreshParameter(int componentId, const std::string &paramName)
{
    if (!_vehicle) {
        return;
    }
    
    int actualComponentId = _actualComponentId(componentId);
    
    mavlink_message_t message;
    mavlink_msg_param_request_read_pack(255, MAV_COMP_ID_MISSIONPLANNER, &message,
                                       actualComponentId, actualComponentId,
                                       paramName.c_str(), -1);
    
    _vehicle->sendMessage(message);
}

void ParameterManager::refreshParametersPrefix(int componentId, const std::string &namePrefix)
{
    // This would require iterating through known parameters and refreshing those with matching prefix
    // For now, just refresh all parameters
    refreshAllParameters(componentId);
}

void ParameterManager::resetAllParametersToDefaults()
{
    if (!_vehicle) {
        return;
    }
    
    // Send command to reset parameters to defaults
    _vehicle->sendCommand(MAV_CMD_PREFLIGHT_STORAGE, 1, 2.0f); // Reset to defaults
}

void ParameterManager::resetAllToVehicleConfiguration()
{
    if (!_vehicle) {
        return;
    }
    
    // Send command to reset to vehicle configuration
    _vehicle->sendCommand(MAV_CMD_PREFLIGHT_STORAGE, 1, 1.0f); // Reset to vehicle config
}

bool ParameterManager::parameterExists(int componentId, const std::string &paramName) const
{
    std::lock_guard<std::mutex> lock(_paramMutex);
    
    int actualComponentId = _actualComponentId(componentId);
    
    auto compIt = _mapCompId2FactMap.find(actualComponentId);
    if (compIt == _mapCompId2FactMap.end()) {
        return false;
    }
    
    return compIt->second.find(paramName) != compIt->second.end();
}

std::vector<std::string> ParameterManager::parameterNames(int componentId) const
{
    std::vector<std::string> paramNames;
    std::lock_guard<std::mutex> lock(_paramMutex);
    
    int actualComponentId = _actualComponentId(componentId);
    
    auto compIt = _mapCompId2FactMap.find(actualComponentId);
    if (compIt == _mapCompId2FactMap.end()) {
        return paramNames;
    }
    
    for (const auto& pair : compIt->second) {
        paramNames.push_back(pair.first);
    }
    
    return paramNames;
}

// Timer management methods
void ParameterManager::_startInitialRequestTimer()
{
    _stopInitialRequestTimer();
    _initialRequestTimerActive = true;
    _initialRequestStartTime = std::chrono::steady_clock::now();
    
    _initialRequestTimeoutThread = std::thread([this]() {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        if (_initialRequestTimerActive.load() && _timersRunning.load()) {
            _initialRequestTimeout();
        }
    });
    _initialRequestTimeoutThread.detach();
}

void ParameterManager::_startWaitingParamTimer()
{
    _stopWaitingParamTimer();
    _waitingParamTimerActive = true;
    _waitingParamStartTime = std::chrono::steady_clock::now();
    
    _waitingParamTimeoutThread = std::thread([this]() {
        std::this_thread::sleep_for(std::chrono::seconds(3));
        if (_waitingParamTimerActive.load() && _timersRunning.load()) {
            _waitingParamTimeout();
        }
    });
    _waitingParamTimeoutThread.detach();
}

void ParameterManager::_stopInitialRequestTimer()
{
    _initialRequestTimerActive = false;
}

void ParameterManager::_stopWaitingParamTimer()
{
    _waitingParamTimerActive = false;
}

void ParameterManager::_initialRequestTimeout()
{
    if (!_initialRequestTimerActive.load()) {
        return;
    }
    
    _stopInitialRequestTimer();
    
    std::cout << "Initial request timeout" << std::endl;
    
    if (!_disableAllRetries && (++_initialRequestRetryCount <= _maxInitialRequestListRetry)) {
        std::cout << "Retrying initial parameter request list, attempt " << _initialRequestRetryCount << std::endl;
        refreshAllParameters();
        _startInitialRequestTimer();
    } else {
        std::cout << "Initial parameter request failed after " << _initialRequestRetryCount << " retries" << std::endl;
        _missingParameters = true;
        _initialLoadComplete = true;
        _parametersReady = true;
        
        if (_parametersReadyCallback) {
            _parametersReadyCallback(true);
        }
    }
}

void ParameterManager::_waitingParamTimeout()
{
    if (!_waitingParamTimerActive.load()) {
        return;
    }
    
    _stopWaitingParamTimer();
    
    std::cout << "Waiting parameter timeout" << std::endl;
    
    // Activate index batch queue on first timeout
    _indexBatchQueueActive = true;
    
    // Request missing parameters
    bool paramsRequested = _fillIndexBatchQueue(true);
    
    if (paramsRequested) {
        _startWaitingParamTimer();
        std::cout << "Restarting waiting param timer - re-request" << std::endl;
    } else {
        _checkInitialLoadComplete();
    }
}

// Cache management methods
void ParameterManager::_handleHashCheck(int componentId, const Fact::ValueVariant_t &hashValue)
{
    if (_initialLoadComplete) {
        return;
    }
    
    std::cout << "Received hash check, attempting cache load" << std::endl;
    _tryCacheHashLoad(_vehicle->systemId(), componentId, hashValue);
}

void ParameterManager::_tryCacheHashLoad(int vehicleId, int componentId, const Fact::ValueVariant_t &hashValue)
{
    std::map<std::string, ParamTypeVal> cacheMap;
    
    if (!_readCacheFile(vehicleId, componentId, cacheMap)) {
        std::cout << "No cache file found, waiting for parameters" << std::endl;
        return;
    }
    
    // Calculate CRC of cache
    uint32_t cacheCRC = _calculateCacheCRC(cacheMap);
    uint32_t vehicleHash = 0;
    
    // Extract hash value (assuming it's stored as uint32)
    if (std::holds_alternative<int32_t>(hashValue)) {
        vehicleHash = static_cast<uint32_t>(std::get<int32_t>(hashValue));
    }
    
    if (cacheCRC == vehicleHash) {
        std::cout << "Cache hash matches, loading " << cacheMap.size() << " parameters from cache" << std::endl;
        
        // Load all parameters from cache
        int index = 0;
        for (const auto& pair : cacheMap) {
            const auto& [name, typeVal] = pair;
            const auto& [type, value] = typeVal;
            uint8_t mavType = factTypeToMavType(type);
            
            _handleParamValue(componentId, name, static_cast<int>(cacheMap.size()), 
                             index++, mavType, value);
        }
        
        // Send hash acknowledgment to stop streaming
        _sendHashAck(componentId, cacheCRC);
    } else {
        std::cout << "Cache hash mismatch, cache: " << cacheCRC << ", vehicle: " << vehicleHash << std::endl;
    }
}

void ParameterManager::_sendHashAck(int componentId, uint32_t crcValue)
{
    mavlink_message_t message;
    mavlink_param_set_t paramSet{};
    
    strncpy(paramSet.param_id, "_HASH_CHECK", sizeof(paramSet.param_id) - 1);
    paramSet.param_id[sizeof(paramSet.param_id) - 1] = '\0';
    paramSet.param_value = static_cast<float>(crcValue);
    paramSet.param_type = MAV_PARAM_TYPE_UINT32;
    paramSet.target_system = static_cast<uint8_t>(_vehicle->systemId());
    paramSet.target_component = static_cast<uint8_t>(componentId);
    
    mavlink_msg_param_set_encode(255, MAV_COMP_ID_MISSIONPLANNER, &message, &paramSet);
    _vehicle->sendMessage(message);
    
    std::cout << "Sent hash acknowledgment: " << crcValue << std::endl;
}

std::string ParameterManager::_parameterCacheFile(int vehicleId, int componentId)
{
    return "ParamCache/" + std::to_string(vehicleId) + "_" + std::to_string(componentId) + ".cache";
}

bool ParameterManager::_readCacheFile(int vehicleId, int componentId, std::map<std::string, ParamTypeVal> &cacheMap)
{
    std::string filename = _parameterCacheFile(vehicleId, componentId);
    std::ifstream file(filename, std::ios::binary);
    
    if (!file.is_open()) {
        return false;
    }
    
    // Simple binary format: count, then name/value pairs
    size_t count;
    file.read(reinterpret_cast<char*>(&count), sizeof(count));
    
    for (size_t i = 0; i < count; i++) {
        // Read parameter name
        size_t nameLen;
        file.read(reinterpret_cast<char*>(&nameLen), sizeof(nameLen));
        
        std::string name(nameLen, '\0');
        file.read(&name[0], nameLen);
        
        // Read parameter type and value
        int32_t type;
        Fact::ValueVariant_t value;
        
        file.read(reinterpret_cast<char*>(&type), sizeof(type));
        
        // Read value based on type
        switch (static_cast<FactMetaData::ValueType_t>(type)) {
            case FactMetaData::valueTypeInt32: {
                int32_t val;
                file.read(reinterpret_cast<char*>(&val), sizeof(val));
                value = val;
                break;
            }
            case FactMetaData::valueTypeFloat: {
                float val;
                file.read(reinterpret_cast<char*>(&val), sizeof(val));
                value = val;
                break;
            }
            case FactMetaData::valueTypeDouble: {
                double val;
                file.read(reinterpret_cast<char*>(&val), sizeof(val));
                value = val;
                break;
            }
            default:
                value = int32_t(0);
                break;
        }
        
        cacheMap[name] = {static_cast<FactMetaData::ValueType_t>(type), value};
    }
    
    return true;
}

void ParameterManager::_writeCacheFile(int vehicleId, int componentId, const std::map<std::string, ParamTypeVal> &cacheMap)
{
    // Create cache directory if it doesn't exist
    std::filesystem::create_directories("ParamCache");
    
    std::string filename = _parameterCacheFile(vehicleId, componentId);
    std::ofstream file(filename, std::ios::binary);
    
    if (!file.is_open()) {
        std::cout << "Failed to open cache file for writing: " << filename << std::endl;
        return;
    }
    
    // Write count
    size_t count = cacheMap.size();
    file.write(reinterpret_cast<const char*>(&count), sizeof(count));
    
    // Write each parameter
    for (const auto& pair : cacheMap) {
        const auto& [name, typeVal] = pair;
        const auto& [type, value] = typeVal;
        
        // Write name length and name
        size_t nameLen = name.length();
        file.write(reinterpret_cast<const char*>(&nameLen), sizeof(nameLen));
        file.write(name.c_str(), nameLen);
        
        // Write type
        int32_t typeInt = static_cast<int32_t>(type);
        file.write(reinterpret_cast<const char*>(&typeInt), sizeof(typeInt));
        
        // Write value based on type
        switch (type) {
            case FactMetaData::valueTypeInt32: {
                int32_t val = std::get<int32_t>(value);
                file.write(reinterpret_cast<const char*>(&val), sizeof(val));
                break;
            }
            case FactMetaData::valueTypeFloat: {
                float val = std::get<float>(value);
                file.write(reinterpret_cast<const char*>(&val), sizeof(val));
                break;
            }
            case FactMetaData::valueTypeDouble: {
                double val = std::get<double>(value);
                file.write(reinterpret_cast<const char*>(&val), sizeof(val));
                break;
            }
            default:
                break;
        }
    }
    
    std::cout << "Wrote " << count << " parameters to cache file: " << filename << std::endl;
}

uint32_t ParameterManager::_calculateCacheCRC(const std::map<std::string, ParamTypeVal> &cacheMap)
{
    uint32_t crc = 0;
    
    for (const auto& pair : cacheMap) {
        const auto& [name, typeVal] = pair;
        const auto& [type, value] = typeVal;
        
        // Add name to CRC
        for (char c : name) {
            crc = crc * 31 + static_cast<uint32_t>(c);
        }
        
        // Add value to CRC (simplified)
        switch (type) {
            case FactMetaData::valueTypeInt32: {
                int32_t val = std::get<int32_t>(value);
                crc ^= static_cast<uint32_t>(val);
                break;
            }
            case FactMetaData::valueTypeFloat: {
                float val = std::get<float>(value);
                uint32_t *valPtr = reinterpret_cast<uint32_t*>(&val);
                crc ^= *valPtr;
                break;
            }
            case FactMetaData::valueTypeDouble: {
                double val = std::get<double>(value);
                uint64_t *valPtr = reinterpret_cast<uint64_t*>(&val);
                crc ^= static_cast<uint32_t>(*valPtr) ^ static_cast<uint32_t>(*valPtr >> 32);
                break;
            }
            default:
                break;
        }
    }
    
    return crc;
}

// Index-based parameter loading
bool ParameterManager::_fillIndexBatchQueue(bool waitingParamTimeout)
{
    if (!_indexBatchQueueActive) {
        return false;
    }
    
    constexpr int maxBatchSize = 10;
    
    if (waitingParamTimeout) {
        // Clear queue on timeout and rebuild
        std::cout << "Refilling index batch queue due to timeout" << std::endl;
        _indexBatchQueue.clear();
    } else {
        std::cout << "Refilling index batch queue due to received parameter" << std::endl;
    }
    
    // Fill queue with missing parameters
    for (const auto& pair : _waitingReadParamIndexMap) {
        int componentId = pair.first;
        const auto& waitingMap = pair.second;
        
        if (!waitingMap.empty()) {
            std::cout << "Component " << componentId << " has " << waitingMap.size() << " missing parameters" << std::endl;
            
            for (const auto& indexPair : waitingMap) {
                int paramIndex = indexPair.first;
                int retryCount = indexPair.second;
                
                // Add to queue if retry count is below maximum
                if (retryCount <= _maxInitialLoadRetrySingleParam) {
                    _indexBatchQueue.push_back(paramIndex);
                    
                    if (_indexBatchQueue.size() >= maxBatchSize) {
                        break;
                    }
                }
            }
        }
        
        if (_indexBatchQueue.size() >= maxBatchSize) {
            break;
        }
    }
    
    // Send batch requests
    if (!_indexBatchQueue.empty()) {
        std::cout << "Sending batch requests for " << _indexBatchQueue.size() << " parameters" << std::endl;
        
        for (int paramIndex : _indexBatchQueue) {
            // Find which component this parameter belongs to
            for (const auto& pair : _waitingReadParamIndexMap) {
                int componentId = pair.first;
                const auto& waitingMap = pair.second;
                
                if (waitingMap.find(paramIndex) != waitingMap.end()) {
                    // Send PARAM_REQUEST_READ for this index
                    mavlink_message_t message;
                    mavlink_msg_param_request_read_pack(255, MAV_COMP_ID_MISSIONPLANNER, &message,
                                                       _vehicle->systemId(), componentId,
                                                       "", paramIndex);
                    
                    _vehicle->sendMessage(message);
                    
                    // Increment retry count
                    _waitingReadParamIndexMap[componentId][paramIndex]++;
                    break;
                }
            }
        }
        
        return true;
    }
    
    return false;
}

void ParameterManager::_updateProgressBar()
{
    int totalWaiting = 0;
    
    for (const auto& pair : _waitingReadParamIndexMap) {
        totalWaiting += pair.second.size();
    }
    
    if (_totalParamCount > 0) {
        double progress = static_cast<double>(_totalParamCount - totalWaiting) / static_cast<double>(_totalParamCount);
        _setLoadProgress(progress);
        
        if (totalWaiting == 0) {
            _readParamIndexProgressActive = false;
        } else {
            _readParamIndexProgressActive = true;
        }
    }
}

void ParameterManager::_checkInitialLoadComplete()
{
    // Check if all parameters are loaded
    int totalWaiting = 0;
    
    for (const auto& pair : _waitingReadParamIndexMap) {
        totalWaiting += pair.second.size();
    }
    
    if (totalWaiting == 0 && !_initialLoadComplete) {
        std::cout << "Initial parameter load complete" << std::endl;
        
        _initialLoadComplete = true;
        _parametersReady = true;
        _missingParameters = false;
        
        // Write cache file for PX4 (if supported)
        if (!_mapCompId2FactMap.empty()) {
            int componentId = _mapCompId2FactMap.begin()->first;
            _writeLocalParamCache(_vehicle->systemId(), componentId);
        }
        
        if (_parametersReadyCallback) {
            _parametersReadyCallback(true);
        }
    }
}

void ParameterManager::_setLoadProgress(double loadProgress)
{
    _loadProgress = loadProgress;
    
    if (_loadProgressCallback) {
        _loadProgressCallback(loadProgress);
    }
    
    // Notify progress updates
    std::cout << "Parameter load progress: " << (loadProgress * 100.0) << "%" << std::endl;
}

int ParameterManager::_actualComponentId(int componentId) const
{
    if (componentId == defaultComponentId) {
        // Return the first available component id (usually the autopilot)
        if (!_mapCompId2FactMap.empty()) {
            return _mapCompId2FactMap.begin()->first;
        }
        return MAV_COMP_ID_AUTOPILOT1; // Default to autopilot component
    }
    return componentId;
}

std::string ParameterManager::_logVehiclePrefix(int componentId) const
{
    return "Vehicle[" + std::to_string(_vehicle->systemId()) + ":" + std::to_string(componentId) + "]";
}

void ParameterManager::_writeLocalParamCache(int vehicleId, int componentId)
{
    std::map<std::string, ParamTypeVal> cacheMap;
    
    // Build cache map from current parameters
    auto compIt = _mapCompId2FactMap.find(componentId);
    if (compIt != _mapCompId2FactMap.end()) {
        for (const auto& pair : compIt->second) {
            const auto& [name, fact] = pair;
            cacheMap[name] = {fact->type(), fact->rawValue()};
        }
    }
    
    _writeCacheFile(vehicleId, componentId, cacheMap);
}

std::shared_ptr<Fact> ParameterManager::getParameter(int componentId, const std::string &paramName)
{
    std::lock_guard<std::mutex> lock(_paramMutex);
    
    int actualComponentId = _actualComponentId(componentId);
    
    auto compIt = _mapCompId2FactMap.find(actualComponentId);
    if (compIt == _mapCompId2FactMap.end()) {
        return _defaultFact;
    }
    
    auto paramIt = compIt->second.find(paramName);
    if (paramIt == compIt->second.end()) {
        return _defaultFact;
    }
    
    return paramIt->second;
}

std::string ParameterManager::readParametersFromStream(std::istream &stream)
{
    std::string line;
    std::string errors;
    int lineNum = 0;
    
    while (std::getline(stream, line)) {
        lineNum++;
        
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#') {
            continue;
        }
        
        std::istringstream iss(line);
        std::string paramName, paramValue;
        
        if (std::getline(iss, paramName, ',') && std::getline(iss, paramValue)) {
            // Trim whitespace
            paramName.erase(0, paramName.find_first_not_of(" \t"));
            paramName.erase(paramName.find_last_not_of(" \t") + 1);
            paramValue.erase(0, paramValue.find_first_not_of(" \t"));
            paramValue.erase(paramValue.find_last_not_of(" \t") + 1);
            
            auto param = getParameter(defaultComponentId, paramName);
            if (param) {
                try {
                    // Convert string to appropriate type and set parameter
                    Fact::ValueVariant_t value = _stringToTypedVariant(paramValue, param->type());
                    param->setRawValue(value);
                    
                    // Send to vehicle
                    _mavlinkParamSet(defaultComponentId, paramName, param->type(), value);
                } catch (const std::exception& e) {
                    errors += "Line " + std::to_string(lineNum) + ": " + e.what() + "\n";
                }
            } else {
                errors += "Line " + std::to_string(lineNum) + ": Unknown parameter " + paramName + "\n";
            }
        }
    }
    
    return errors;
}

void ParameterManager::writeParametersToStream(std::ostream &stream) const
{
    std::lock_guard<std::mutex> lock(_paramMutex);
    
    stream << "# Onboard parameters" << std::endl;
    stream << "# Generated by MAVLink Data Collector" << std::endl;
    stream << std::endl;
    
    for (const auto& compPair : _mapCompId2FactMap) {
        for (const auto& paramPair : compPair.second) {
            const auto& paramName = paramPair.first;
            const auto& fact = paramPair.second;
            
            stream << paramName << "," << fact->rawValueString() << std::endl;
        }
    }
}

bool ParameterManager::pendingWrites() const
{
    return _pendingWritesCount > 0;
}

uint8_t ParameterManager::factTypeToMavType(FactMetaData::ValueType_t factType)
{
    switch (factType) {
        case FactMetaData::valueTypeUint8:
            return MAV_PARAM_TYPE_UINT8;
        case FactMetaData::valueTypeInt8:
            return MAV_PARAM_TYPE_INT8;
        case FactMetaData::valueTypeUint16:
            return MAV_PARAM_TYPE_UINT16;
        case FactMetaData::valueTypeInt16:
            return MAV_PARAM_TYPE_INT16;
        case FactMetaData::valueTypeUint32:
            return MAV_PARAM_TYPE_UINT32;
        case FactMetaData::valueTypeInt32:
            return MAV_PARAM_TYPE_INT32;
        case FactMetaData::valueTypeUint64:
            return MAV_PARAM_TYPE_UINT64;
        case FactMetaData::valueTypeInt64:
            return MAV_PARAM_TYPE_INT64;
        case FactMetaData::valueTypeFloat:
            return MAV_PARAM_TYPE_REAL32;
        case FactMetaData::valueTypeDouble:
            return MAV_PARAM_TYPE_REAL64;
        default:
            return MAV_PARAM_TYPE_INT32;
    }
}

FactMetaData::ValueType_t ParameterManager::mavTypeToFactType(uint8_t mavType)
{
    switch (mavType) {
        case MAV_PARAM_TYPE_UINT8:
            return FactMetaData::valueTypeUint8;
        case MAV_PARAM_TYPE_INT8:
            return FactMetaData::valueTypeInt8;
        case MAV_PARAM_TYPE_UINT16:
            return FactMetaData::valueTypeUint16;
        case MAV_PARAM_TYPE_INT16:
            return FactMetaData::valueTypeInt16;
        case MAV_PARAM_TYPE_UINT32:
            return FactMetaData::valueTypeUint32;
        case MAV_PARAM_TYPE_INT32:
            return FactMetaData::valueTypeInt32;
        case MAV_PARAM_TYPE_UINT64:
            return FactMetaData::valueTypeUint64;
        case MAV_PARAM_TYPE_INT64:
            return FactMetaData::valueTypeInt64;
        case MAV_PARAM_TYPE_REAL32:
            return FactMetaData::valueTypeFloat;
        case MAV_PARAM_TYPE_REAL64:
            return FactMetaData::valueTypeDouble;
        default:
            return FactMetaData::valueTypeInt32;
    }
}

void ParameterManager::_handleParamValue(int componentId, const std::string &parameterName, 
                                       int parameterCount, int parameterIndex, 
                                       uint8_t mavParamType, const Fact::ValueVariant_t &parameterValue)
{
    // Handle hash check parameter for PX4
    if (parameterName == "_HASH_CHECK") {
        _handleHashCheck(componentId, parameterValue);
        return;
    }
    
    // Disregard unrequested params prior to initial list response (ArduPilot behavior)
    if ((parameterIndex == 65535) && _initialRequestTimerActive.load()) {
        std::cout << "Disregarding unrequested param prior to initial list response: " << parameterName << std::endl;
        return;
    }
    
    // Stop timers since we received a response
    _stopInitialRequestTimer();
    _stopWaitingParamTimer();
    
    std::lock_guard<std::mutex> lock(_paramMutex);
    
    // Update parameter count if this is the first time seeing this component
    if (_paramCountMap.find(componentId) == _paramCountMap.end()) {
        _paramCountMap[componentId] = parameterCount;
        _totalParamCount += parameterCount;
        
        // Initialize waiting index map for this component
        for (int i = 0; i < parameterCount; i++) {
            _waitingReadParamIndexMap[componentId][i] = 0;
        }
        
        std::cout << "Seeing component " << componentId << " for first time - paramcount: " << parameterCount << std::endl;
    }
    
    // Remove from waiting list if present
    if (_waitingReadParamIndexMap.find(componentId) != _waitingReadParamIndexMap.end()) {
        auto& waitingMap = _waitingReadParamIndexMap[componentId];
        if (waitingMap.find(parameterIndex) != waitingMap.end()) {
            waitingMap.erase(parameterIndex);
            
            // Remove from batch queue if present
            auto it = std::find(_indexBatchQueue.begin(), _indexBatchQueue.end(), parameterIndex);
            if (it != _indexBatchQueue.end()) {
                _indexBatchQueue.erase(it);
            }
            
            // Fill batch queue for more parameters
            _fillIndexBatchQueue(false);
        }
    }
    
    // Create or update parameter fact
    auto fact = std::make_shared<Fact>(componentId, parameterName, mavTypeToFactType(mavParamType));
    fact->setRawValue(parameterValue);
    
    // Set up value change callback
    fact->setValueChangedCallback([this, componentId, parameterName](const Fact* changedFact, const Fact::ValueVariant_t& value) {
        _mavlinkParamSet(componentId, parameterName, changedFact->type(), value);
    });
    
    _mapCompId2FactMap[componentId][parameterName] = fact;
    
    if (_factAddedCallback) {
        _factAddedCallback(componentId, fact);
    }
    
    // Update progress
    _updateProgressBar();
    
    // Check if we need to restart waiting timer
    int totalWaiting = 0;
    for (const auto& pair : _waitingReadParamIndexMap) {
        totalWaiting += pair.second.size();
    }
    
    if (totalWaiting > 0) {
        _startWaitingParamTimer();
        std::cout << "Restarting waiting param timer - still waiting for " << totalWaiting << " parameters" << std::endl;
    } else {
        // Check if initial load is complete
        _checkInitialLoadComplete();
    }
}

void ParameterManager::_mavlinkParamSet(int componentId, const std::string &name, 
                                      FactMetaData::ValueType_t valueType, 
                                      const Fact::ValueVariant_t &rawValue)
{
    if (!_vehicle) {
        return;
    }
    
    mavlink_param_union_t paramUnion;
    if (!_fillMavlinkParamUnion(valueType, rawValue, paramUnion)) {
        return;
    }
    
    mavlink_message_t message;
    mavlink_msg_param_set_pack(255, MAV_COMP_ID_MISSIONPLANNER, &message,
                              componentId, componentId, name.c_str(), 
                              paramUnion.param_float, static_cast<uint8_t>(valueType));
    
    _vehicle->sendMessage(message);
    _pendingWritesCount++;
}

bool ParameterManager::_fillMavlinkParamUnion(FactMetaData::ValueType_t valueType, 
                                            const Fact::ValueVariant_t &rawValue, 
                                            mavlink_param_union_t &paramUnion) const
{
    switch (valueType) {
        case FactMetaData::valueTypeUint8:
            paramUnion.param_uint8 = std::get<uint8_t>(rawValue);
            paramUnion.type = MAV_PARAM_TYPE_UINT8;
            break;
        case FactMetaData::valueTypeInt8:
            paramUnion.param_int8 = std::get<int8_t>(rawValue);
            paramUnion.type = MAV_PARAM_TYPE_INT8;
            break;
        case FactMetaData::valueTypeUint16:
            paramUnion.param_uint16 = std::get<uint16_t>(rawValue);
            paramUnion.type = MAV_PARAM_TYPE_UINT16;
            break;
        case FactMetaData::valueTypeInt16:
            paramUnion.param_int16 = std::get<int16_t>(rawValue);
            paramUnion.type = MAV_PARAM_TYPE_INT16;
            break;
        case FactMetaData::valueTypeUint32:
            paramUnion.param_uint32 = std::get<uint32_t>(rawValue);
            paramUnion.type = MAV_PARAM_TYPE_UINT32;
            break;
        case FactMetaData::valueTypeInt32:
            paramUnion.param_int32 = std::get<int32_t>(rawValue);
            paramUnion.type = MAV_PARAM_TYPE_INT32;
            break;
        case FactMetaData::valueTypeFloat:
            paramUnion.param_float = std::get<float>(rawValue);
            paramUnion.type = MAV_PARAM_TYPE_REAL32;
            break;
        default:
            return false;
    }
    
    return true;
}

bool ParameterManager::_mavlinkParamUnionToVariant(const mavlink_param_union_t &paramUnion, 
                                                 Fact::ValueVariant_t &outValue) const
{
    switch (paramUnion.type) {
        case MAV_PARAM_TYPE_UINT8:
            outValue = paramUnion.param_uint8;
            break;
        case MAV_PARAM_TYPE_INT8:
            outValue = paramUnion.param_int8;
            break;
        case MAV_PARAM_TYPE_UINT16:
            outValue = paramUnion.param_uint16;
            break;
        case MAV_PARAM_TYPE_INT16:
            outValue = paramUnion.param_int16;
            break;
        case MAV_PARAM_TYPE_UINT32:
            outValue = paramUnion.param_uint32;
            break;
        case MAV_PARAM_TYPE_INT32:
            outValue = paramUnion.param_int32;
            break;
        case MAV_PARAM_TYPE_REAL32:
            outValue = paramUnion.param_float;
            break;
        default:
            return false;
    }
    
    return true;
}

Fact::ValueVariant_t ParameterManager::_stringToTypedVariant(const std::string &string, FactMetaData::ValueType_t type, bool failOk)
{
    std::istringstream iss(string);
    
    switch (type) {
        case FactMetaData::valueTypeUint8: {
            uint16_t val;
            iss >> val;
            return static_cast<uint8_t>(val);
        }
        case FactMetaData::valueTypeInt8: {
            int16_t val;
            iss >> val;
            return static_cast<int8_t>(val);
        }
        case FactMetaData::valueTypeUint16: {
            uint16_t val;
            iss >> val;
            return val;
        }
        case FactMetaData::valueTypeInt16: {
            int16_t val;
            iss >> val;
            return val;
        }
        case FactMetaData::valueTypeUint32: {
            uint32_t val;
            iss >> val;
            return val;
        }
        case FactMetaData::valueTypeInt32: {
            int32_t val;
            iss >> val;
            return val;
        }
        case FactMetaData::valueTypeFloat: {
            float val;
            iss >> val;
            return val;
        }
        case FactMetaData::valueTypeDouble: {
            double val;
            iss >> val;
            return val;
        }
        default:
            return int32_t(0);
    }
}

void ParameterManager::_incrementPendingWriteCount()
{
    _pendingWritesCount++;
}

void ParameterManager::_decrementPendingWriteCount()
{
    if (_pendingWritesCount > 0) {
        _pendingWritesCount--;
    }
}

std::string ParameterManager::_vehicleAndComponentString(int componentId) const
{
    return "Vehicle:" + std::to_string(_actualComponentId(componentId));
}
