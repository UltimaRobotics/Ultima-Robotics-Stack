#pragma once

#include "DataStructures.h"
#include <string>

namespace FlightCollector {

/**
 * @brief JSON formatter for flight data output
 */
class JsonFormatter {
public:
    /**
     * @brief Format complete flight data collection as JSON
     * @param data Flight data collection to format
     * @param verbose Whether to include verbose data (parameters, message_rates)
     * @return JSON string
     */
    static std::string formatFlightData(const FlightDataCollection& data, bool verbose = false);
    
    /**
     * @brief Format vehicle data as JSON
     * @param vehicle Vehicle data to format
     * @return JSON string
     */
    static std::string formatVehicleData(const VehicleData& vehicle);
    
    /**
     * @brief Format diagnostic data as JSON
     * @param diagnostics Diagnostic data to format
     * @return JSON string
     */
    static std::string formatDiagnosticData(const DiagnosticData& diagnostics);
    
    /**
     * @brief Format battery status as JSON
     * @param battery_status Map of battery status data
     * @return JSON string
     */
    static std::string formatBatteryStatus(const std::map<uint8_t, BatteryStatus>& battery_status);
    
    /**
     * @brief Format battery info as JSON
     * @param battery_info Map of battery info data
     * @return JSON string
     */
    static std::string formatBatteryInfo(const std::map<uint8_t, BatteryInfo>& battery_info);
    
    /**
     * @brief Format parameters as JSON
     * @param parameters Map of parameter data
     * @return JSON string
     */
    static std::string formatParameters(const std::map<std::string, ParameterInfo>& parameters);
    
    /**
     * @brief Format message rates as JSON
     * @param message_rates Map of message rate data
     * @return JSON string
     */
    static std::string formatMessageRates(const std::map<uint16_t, MessageRateInfo>& message_rates);
    
    /**
     * @brief Format sensor status as JSON
     * @param sensors Sensor status data
     * @return JSON string
     */
    static std::string formatSensorStatus(const SensorStatus& sensors);
    
    /**
     * @brief Format power status as JSON
     * @param power Power status data
     * @return JSON string
     */
    static std::string formatPowerStatus(const PowerStatus& power);
    
    /**
     * @brief Get current timestamp as ISO 8601 string
     * @return ISO 8601 timestamp string
     */
    static std::string getCurrentTimestamp();
    
    /**
     * @brief Format time point as ISO 8601 string
     * @param time_point Time point to format
     * @return ISO 8601 timestamp string
     */
    static std::string formatTimestamp(const std::chrono::system_clock::time_point& time_point);

private:
    /**
     * @brief Convert vehicle type enum to string
     * @param type MAVLink vehicle type
     * @return Human readable vehicle type string
     */
    static std::string vehicleTypeToString(uint8_t type);
    
    /**
     * @brief Convert autopilot type enum to string
     * @param autopilot MAVLink autopilot type
     * @return Human readable autopilot string
     */
    static std::string autopilotToString(uint8_t autopilot);
    
    /**
     * @brief Convert battery function enum to string
     * @param function MAVLink battery function
     * @return Human readable battery function string
     */
    static std::string batteryFunctionToString(uint8_t function);
    
    /**
     * @brief Convert battery type enum to string
     * @param type MAVLink battery type
     * @return Human readable battery type string
     */
    static std::string batteryTypeToString(uint8_t type);
};

} // namespace FlightCollector
