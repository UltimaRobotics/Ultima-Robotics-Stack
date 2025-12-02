#pragma once

#include <string>
#include <memory>
#include <chrono>
#include "nlohmann/json.hpp"
#include "JsonConfig.h"
#include "Vehicle.h"
#include "MAVLinkUdpConnection.h"

/// JSON Data Printer for MAVLink Data Collector
/// Handles configurable JSON output for different data types
class JsonDataPrinter
{
public:
    JsonDataPrinter(const JsonConfig& config);
    ~JsonDataPrinter() = default;
    
    /// Print vehicle information in JSON format
    /// @param vehicle Vehicle to get data from
    /// @return JSON string containing vehicle info
    std::string printVehicleInfo(Vehicle* vehicle) const;
    
    /// Print telemetry data in JSON format
    /// @param vehicle Vehicle to get data from
    /// @return JSON string containing telemetry data
    std::string printTelemetryData(Vehicle* vehicle) const;
    
    /// Print parameters in JSON format
    /// @param vehicle Vehicle to get data from
    /// @return JSON string containing parameters
    std::string printParameters(Vehicle* vehicle) const;
    
    /// Print connection statistics in JSON format
    /// @param connection Connection to get data from
    /// @return JSON string containing connection stats
    std::string printConnectionStats(const MAVLinkUdpConnection* connection) const;
    
    /// Print heartbeat data in JSON format
    /// @param vehicle Vehicle to get data from
    /// @return JSON string containing heartbeat data
    std::string printHeartbeat(Vehicle* vehicle) const;
    
    /// Print autopilot version in JSON format
    /// @param vehicle Vehicle to get data from
    /// @return JSON string containing autopilot version
    std::string printAutopilotVersion(Vehicle* vehicle) const;
    
    /// Print all enabled data types in a single JSON object
    /// @param vehicle Vehicle to get data from
    /// @param connection Connection to get data from
    /// @return Complete JSON string with all enabled data
    std::string printAllData(Vehicle* vehicle, const MAVLinkUdpConnection* connection) const;
    
    /// Print system status data in JSON format
    /// @param vehicle Vehicle to get data from
    /// @return JSON string containing system status
    std::string printSystemStatus(Vehicle* vehicle) const;
    
    /// Print GPS data in JSON format
    /// @param vehicle Vehicle to get data from
    /// @return JSON string containing GPS data
    std::string printGpsData(Vehicle* vehicle) const;
    
    /// Print GPS2 data in JSON format
    /// @param vehicle Vehicle to get data from
    /// @return JSON string containing GPS2 data
    std::string printGps2Data(Vehicle* vehicle) const;
    
    /// Print battery data in JSON format
    /// @param vehicle Vehicle to get data from
    /// @return JSON string containing battery data
    std::string printBatteryData(Vehicle* vehicle) const;
    
    /// Print RC data in JSON format
    /// @param vehicle Vehicle to get data from
    /// @return JSON string containing RC data
    std::string printRcData(Vehicle* vehicle) const;
    
    /// Print vibration data in JSON format
    /// @param vehicle Vehicle to get data from
    /// @return JSON string containing vibration data
    std::string printVibrationData(Vehicle* vehicle) const;
    
    /// Print temperature data in JSON format
    /// @param vehicle Vehicle to get data from
    /// @return JSON string containing temperature data
    std::string printTemperatureData(Vehicle* vehicle) const;
    
    /// Print estimator status in JSON format
    /// @param vehicle Vehicle to get data from
    /// @return JSON string containing estimator status
    std::string printEstimatorStatus(Vehicle* vehicle) const;
    
    /// Print wind data in JSON format
    /// @param vehicle Vehicle to get data from
    /// @return JSON string containing wind data
    std::string printWindData(Vehicle* vehicle) const;
    
    /// Check if a specific data type should be printed
    /// @param dataType Name of the data type
    /// @return true if data type should be printed
    bool shouldPrint(const std::string& dataType) const;

private:
    const JsonConfig& _config;
    nlohmann::json _jsonOutputConfig;
    
    /// Add timestamp to JSON object if enabled
    /// @param j JSON object to add timestamp to
    void addTimestamp(nlohmann::json& j) const;
    
    /// Add metadata to JSON object if enabled
    /// @param j JSON object to add metadata to
    /// @param dataType Type of data being printed
    void addMetadata(nlohmann::json& j, const std::string& dataType) const;
    
    /// Convert JSON object to string with configured formatting
    /// @param j JSON object to convert
    /// @return Formatted JSON string
    std::string formatJson(const nlohmann::json& j) const;
    
    /// Helper function to safely extract variant values
    template<typename T>
    T safeGetVariant(const FactMetaData::ValueVariant_t& variant, T defaultValue = T{}) const;
    
    /// Add fact value to JSON object if fact exists and field is enabled
    /// @param j JSON object to add value to
    /// @param factGroup Fact group containing the fact
    /// @param factName Name of the fact
    /// @param jsonKey Key to use in JSON object
    /// @param fields List of enabled fields (empty to include all)
    void addFactToJson(nlohmann::json& j, const FactGroup* factGroup, 
                      const std::string& factName, const std::string& jsonKey,
                      const std::vector<std::string>& fields = {}) const;
    
    /// Add multiple facts to JSON object based on field configuration
    /// @param j JSON object to add values to
    /// @param factGroup Fact group containing the facts
    /// @param fields List of enabled fields
    /// @param fieldMapping Map of field names to fact names (if different)
    void addFactsToJson(nlohmann::json& j, const std::shared_ptr<FactGroup>& factGroup,
                       const std::vector<std::string>& fields,
                       const std::map<std::string, std::string>& fieldMapping = {}) const;
    
    /// Decode sensor presence/health bitfield into meaningful names
    /// @param sensorsBitfield Bitfield value from SYS_STATUS message
    /// @return JSON array with decoded sensor names
    nlohmann::json decodeSensorBitfield(uint32_t sensorsBitfield) const;
    
    /// Get sensor name for bit position
    /// @param bitPosition Bit position in the bitfield
    /// @return Human-readable sensor name
    std::string getSensorName(uint32_t bitPosition) const;
    
    /// Analyze sensor health and return status summary
    /// @param presentBitfield Sensors that are present
    /// @param healthBitfield Sensors that are healthy
    /// @return JSON object with sensor status analysis
    nlohmann::json analyzeSensorStatus(uint32_t presentBitfield, uint32_t healthBitfield) const;
    
    /// Analyze battery configuration and return comprehensive battery data
    /// @param vehicle Vehicle to get battery data from
    /// @return JSON object with comprehensive battery information
    nlohmann::json analyzeBatteryConfiguration(Vehicle* vehicle) const;
    
    /// Extract battery parameters from parameter system
    /// @param vehicle Vehicle to get parameters from
    /// @return JSON object with battery configuration parameters
    nlohmann::json extractBatteryParameters(Vehicle* vehicle) const;
    
    /// Get battery function name from enum value
    /// @param functionValue Battery function enum value
    /// @return Human-readable battery function name
    std::string getBatteryFunctionName(uint8_t functionValue) const;
    
    /// Get battery type name from enum value
    /// @param typeValue Battery type enum value
    /// @return Human-readable battery type name
    std::string getBatteryTypeName(uint8_t typeValue) const;
    
    /// Get battery charge state name from enum value
    /// @param chargeStateValue Battery charge state enum value
    /// @return Human-readable charge state name
    std::string getBatteryChargeStateName(uint8_t chargeStateValue) const;
    
    /// Get battery mode name from enum value
    /// @param modeValue Battery mode enum value
    /// @return Human-readable battery mode name
    std::string getBatteryModeName(uint8_t modeValue) const;
    
    /// Decode battery fault bitmask into readable faults
    /// @param faultBitmask Battery fault bitmask
    /// @return JSON array with decoded fault names
    nlohmann::json decodeBatteryFaults(uint32_t faultBitmask) const;
    
    /// Assess battery health based on percentage and charge state
    /// @param batteryGroup Battery fact group to analyze
    /// @return Health status string
    std::string _assessBatteryHealth(const std::shared_ptr<FactGroup>& batteryGroup) const;
};
