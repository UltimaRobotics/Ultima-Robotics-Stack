#include "JsonFormatter.h"
#include <json/json.h>
#include <iomanip>
#include <sstream>
#include <chrono>
#include <cmath>

namespace FlightCollector {

std::string JsonFormatter::formatFlightData(const FlightDataCollection& data, bool verbose) {
    Json::Value root;
    
    root["timestamp"] = formatTimestamp(data.last_update);
    root["vehicle"] = Json::Value(Json::objectValue);
    root["diagnostics"] = Json::Value(Json::objectValue);
    
    // Only include verbose data when verbose flag is set
    if (verbose) {
        root["parameters"] = Json::Value(Json::objectValue);
        root["message_rates"] = Json::Value(Json::objectValue);
    }
    
    // Parse vehicle data JSON
    Json::Reader reader;
    Json::Value vehicle_json;
    reader.parse(formatVehicleData(data.vehicle), vehicle_json);
    root["vehicle"] = vehicle_json;
    
    // Parse diagnostics JSON
    Json::Value diagnostics_json;
    reader.parse(formatDiagnosticData(data.diagnostics), diagnostics_json);
    root["diagnostics"] = diagnostics_json;
    
    // Only include verbose data when verbose flag is set
    if (verbose) {
        // Parse parameters JSON
        Json::Value parameters_json;
        reader.parse(formatParameters(data.parameters), parameters_json);
        root["parameters"] = parameters_json;
        
        // Parse message rates JSON
        Json::Value message_rates_json;
        reader.parse(formatMessageRates(data.message_rates), message_rates_json);
        root["message_rates"] = message_rates_json;
    }
    
    Json::StreamWriterBuilder builder;
    builder["indentation"] = "  ";
    std::ostringstream oss;
    std::unique_ptr<Json::StreamWriter> writer(builder.newStreamWriter());
    writer->write(root, &oss);
    
    return oss.str();
}

std::string JsonFormatter::formatVehicleData(const VehicleData& vehicle) {
    Json::Value root;
    
    root["model"] = vehicle.model;
    root["system_id"] = static_cast<int>(vehicle.system_id);
    root["component_id"] = static_cast<int>(vehicle.component_id);
    root["flight_mode"] = vehicle.flight_mode;
    root["armed"] = vehicle.armed;
    root["battery_voltage"] = std::round(vehicle.battery_voltage * 1000.0) / 1000.0; // 3 decimal places
    root["last_heartbeat"] = formatTimestamp(vehicle.last_heartbeat);
    root["firmware"] = vehicle.firmware;
    root["last_activity"] = formatTimestamp(vehicle.last_activity);
    root["messages_received"] = static_cast<Json::UInt64>(vehicle.messages_received);
    root["start_time"] = formatTimestamp(vehicle.start_time);
    
    // Component information
    root["vendor_name"] = vehicle.vendor_name;
    root["component_model_name"] = vehicle.component_model_name;
    root["software_version"] = vehicle.software_version;
    root["hardware_version"] = vehicle.hardware_version;
    root["serial_number"] = vehicle.serial_number;
    
    Json::StreamWriterBuilder builder;
    builder["indentation"] = "  ";
    std::ostringstream oss;
    std::unique_ptr<Json::StreamWriter> writer(builder.newStreamWriter());
    writer->write(root, &oss);
    
    return oss.str();
}

std::string JsonFormatter::formatDiagnosticData(const DiagnosticData& diagnostics) {
    Json::Value root;
    
    // Airframe information
    root["airframe_type"] = diagnostics.airframe_type;
    root["vehicle"] = diagnostics.vehicle;
    root["firmware_version"] = diagnostics.firmware_version;
    root["custom_fw_ver"] = diagnostics.custom_fw_ver;
    
    // Sensor status
    root["sensors"] = Json::Value(Json::objectValue);
    Json::Value sensors_json;
    Json::Reader reader;
    reader.parse(formatSensorStatus(diagnostics.sensors), sensors_json);
    root["sensors"] = sensors_json;
    
    // Radio channel mapping
    root["radio"] = Json::Value(Json::objectValue);
    root["radio"]["roll_channel"] = diagnostics.roll_channel;
    root["radio"]["pitch_channel"] = diagnostics.pitch_channel;
    root["radio"]["yaw_channel"] = diagnostics.yaw_channel;
    root["radio"]["throttle_channel"] = diagnostics.throttle_channel;
    root["radio"]["aux1"] = diagnostics.aux1;
    root["radio"]["aux2"] = diagnostics.aux2;
    
    // Flight mode configuration
    root["flight_modes"] = Json::Value(Json::objectValue);
    root["flight_modes"]["mode_switch"] = diagnostics.mode_switch;
    root["flight_modes"]["flight_mode_1"] = diagnostics.flight_mode_1;
    root["flight_modes"]["flight_mode_2"] = diagnostics.flight_mode_2;
    root["flight_modes"]["flight_mode_3"] = diagnostics.flight_mode_3;
    root["flight_modes"]["flight_mode_4"] = diagnostics.flight_mode_4;
    root["flight_modes"]["flight_mode_5"] = diagnostics.flight_mode_5;
    root["flight_modes"]["flight_mode_6"] = diagnostics.flight_mode_6;
    
    // Power system
    root["power"] = Json::Value(Json::objectValue);
    Json::Value power_json;
    reader.parse(formatPowerStatus(diagnostics.power_status), power_json);
    root["power"] = power_json;
    root["power"]["battery_full_voltage"] = std::round(diagnostics.battery_full_voltage * 1000.0) / 1000.0;
    root["power"]["battery_empty_voltage"] = std::round(diagnostics.battery_empty_voltage * 1000.0) / 1000.0;
    root["power"]["number_of_cells"] = diagnostics.number_of_cells;
    
    // Battery info and status
    Json::Value battery_info_json;
    reader.parse(formatBatteryInfo(diagnostics.battery_info_map), battery_info_json);
    root["power"]["battery_info"] = battery_info_json;
    
    Json::Value battery_status_json;
    reader.parse(formatBatteryStatus(diagnostics.battery_status_map), battery_status_json);
    root["power"]["battery_status"] = battery_status_json;
    
    // Safety configuration
    root["safety"] = Json::Value(Json::objectValue);
    root["safety"]["low_battery_failsafe"] = diagnostics.low_battery_failsafe;
    root["safety"]["rc_loss_failsafe"] = diagnostics.rc_loss_failsafe;
    root["safety"]["rc_loss_timeout"] = diagnostics.rc_loss_timeout;
    root["safety"]["data_link_loss_failsafe"] = diagnostics.data_link_loss_failsafe;
    root["safety"]["rtl_climb_to"] = diagnostics.rtl_climb_to;
    root["safety"]["rtl_then"] = diagnostics.rtl_then;
    
    Json::StreamWriterBuilder builder;
    builder["indentation"] = "  ";
    std::ostringstream oss;
    std::unique_ptr<Json::StreamWriter> writer(builder.newStreamWriter());
    writer->write(root, &oss);
    
    return oss.str();
}

std::string JsonFormatter::formatBatteryStatus(const std::map<uint8_t, BatteryStatus>& battery_status) {
    Json::Value root(Json::objectValue);
    
    for (const auto& pair : battery_status) {
        const BatteryStatus& status = pair.second;
        Json::Value battery_json;
        
        battery_json["id"] = static_cast<int>(status.id);
        battery_json["battery_function"] = static_cast<int>(status.battery_function);
        battery_json["type"] = static_cast<int>(status.type);
        battery_json["temperature"] = static_cast<int>(status.temperature);
        battery_json["current_battery"] = static_cast<int>(status.current_battery);
        battery_json["current_consumed"] = static_cast<int>(status.current_consumed);
        battery_json["energy_consumed"] = static_cast<int>(status.energy_consumed);
        battery_json["battery_remaining"] = static_cast<int>(status.battery_remaining);
        battery_json["charge_state"] = static_cast<int>(status.charge_state);
        battery_json["mode"] = static_cast<int>(status.mode);
        battery_json["fault_bitmask"] = static_cast<Json::UInt>(status.fault_bitmask);
        battery_json["time_remaining"] = static_cast<int>(status.time_remaining);
        
        // Cell voltages
        Json::Value voltages(Json::arrayValue);
        for (uint16_t voltage : status.voltages) {
            voltages.append(static_cast<int>(voltage));
        }
        battery_json["voltages"] = voltages;
        
        Json::Value voltages_ext(Json::arrayValue);
        for (uint16_t voltage : status.voltages_ext) {
            voltages_ext.append(static_cast<int>(voltage));
        }
        battery_json["voltages_ext"] = voltages_ext;
        
        root[std::to_string(pair.first)] = battery_json;
    }
    
    Json::StreamWriterBuilder builder;
    builder["indentation"] = "  ";
    std::ostringstream oss;
    std::unique_ptr<Json::StreamWriter> writer(builder.newStreamWriter());
    writer->write(root, &oss);
    
    return oss.str();
}

std::string JsonFormatter::formatBatteryInfo(const std::map<uint8_t, BatteryInfo>& battery_info) {
    Json::Value root(Json::objectValue);
    
    for (const auto& pair : battery_info) {
        const BatteryInfo& info = pair.second;
        Json::Value battery_json;
        
        battery_json["id"] = static_cast<int>(info.id);
        battery_json["battery_function"] = static_cast<int>(info.battery_function);
        battery_json["type"] = static_cast<int>(info.type);
        battery_json["state_of_health"] = static_cast<int>(info.state_of_health);
        battery_json["cells_in_series"] = static_cast<int>(info.cells_in_series);
        battery_json["cycle_count"] = static_cast<int>(info.cycle_count);
        battery_json["weight"] = static_cast<int>(info.weight);
        battery_json["discharge_minimum_voltage"] = std::round(info.discharge_minimum_voltage * 1000.0) / 1000.0;
        battery_json["charging_minimum_voltage"] = std::round(info.charging_minimum_voltage * 1000.0) / 1000.0;
        battery_json["resting_minimum_voltage"] = std::round(info.resting_minimum_voltage * 1000.0) / 1000.0;
        battery_json["charging_maximum_voltage"] = std::round(info.charging_maximum_voltage * 1000.0) / 1000.0;
        battery_json["charging_maximum_current"] = std::round(info.charging_maximum_current * 1000.0) / 1000.0;
        battery_json["nominal_voltage"] = std::round(info.nominal_voltage * 1000.0) / 1000.0;
        battery_json["discharge_maximum_current"] = std::round(info.discharge_maximum_current * 1000.0) / 1000.0;
        battery_json["discharge_maximum_burst_current"] = std::round(info.discharge_maximum_burst_current * 1000.0) / 1000.0;
        battery_json["design_capacity"] = std::round(info.design_capacity * 1000.0) / 1000.0;
        battery_json["full_charge_capacity"] = std::round(info.full_charge_capacity * 1000.0) / 1000.0;
        battery_json["manufacture_date"] = info.manufacture_date;
        battery_json["serial_number"] = info.serial_number;
        battery_json["name"] = info.name;
        
        root[std::to_string(pair.first)] = battery_json;
    }
    
    Json::StreamWriterBuilder builder;
    builder["indentation"] = "  ";
    std::ostringstream oss;
    std::unique_ptr<Json::StreamWriter> writer(builder.newStreamWriter());
    writer->write(root, &oss);
    
    return oss.str();
}

std::string JsonFormatter::formatParameters(const std::map<std::string, ParameterInfo>& parameters) {
    Json::Value root(Json::objectValue);
    
    for (const auto& pair : parameters) {
        const ParameterInfo& param = pair.second;
        Json::Value param_json;
        
        param_json["value"] = param.value;
        param_json["type"] = static_cast<int>(param.type);
        param_json["timestamp"] = formatTimestamp(param.timestamp);
        
        root[pair.first] = param_json;
    }
    
    Json::StreamWriterBuilder builder;
    builder["indentation"] = "  ";
    std::ostringstream oss;
    std::unique_ptr<Json::StreamWriter> writer(builder.newStreamWriter());
    writer->write(root, &oss);
    
    return oss.str();
}

std::string JsonFormatter::formatMessageRates(const std::map<uint16_t, MessageRateInfo>& message_rates) {
    Json::Value root(Json::objectValue);
    
    for (const auto& pair : message_rates) {
        const MessageRateInfo& rate = pair.second;
        Json::Value rate_json;
        
        rate_json["count"] = static_cast<Json::UInt64>(rate.count);
        rate_json["current_rate_hz"] = std::round(rate.current_rate_hz * 100.0) / 100.0;
        rate_json["expected_rate_hz"] = std::round(rate.expected_rate_hz * 100.0) / 100.0;
        rate_json["last_update"] = formatTimestamp(rate.last_update);
        
        root[std::to_string(pair.first)] = rate_json;
    }
    
    Json::StreamWriterBuilder builder;
    builder["indentation"] = "  ";
    std::ostringstream oss;
    std::unique_ptr<Json::StreamWriter> writer(builder.newStreamWriter());
    writer->write(root, &oss);
    
    return oss.str();
}

std::string JsonFormatter::formatSensorStatus(const SensorStatus& sensors) {
    Json::Value root;
    
    root["gyro"] = sensors.gyro;
    root["accelerometer"] = sensors.accelerometer;
    root["compass_0"] = sensors.compass_0;
    root["compass_1"] = sensors.compass_1;
    root["sensors_present"] = static_cast<Json::UInt>(sensors.sensors_present);
    root["sensors_enabled"] = static_cast<Json::UInt>(sensors.sensors_enabled);
    root["sensors_health"] = static_cast<Json::UInt>(sensors.sensors_health);
    
    Json::StreamWriterBuilder builder;
    builder["indentation"] = "  ";
    std::ostringstream oss;
    std::unique_ptr<Json::StreamWriter> writer(builder.newStreamWriter());
    writer->write(root, &oss);
    
    return oss.str();
}

std::string JsonFormatter::formatPowerStatus(const PowerStatus& power) {
    Json::Value root;
    
    root["Vcc"] = static_cast<int>(power.Vcc);
    root["Vservo"] = static_cast<int>(power.Vservo);
    root["flags"] = static_cast<int>(power.flags);
    
    Json::StreamWriterBuilder builder;
    builder["indentation"] = "  ";
    std::ostringstream oss;
    std::unique_ptr<Json::StreamWriter> writer(builder.newStreamWriter());
    writer->write(root, &oss);
    
    return oss.str();
}

std::string JsonFormatter::getCurrentTimestamp() {
    return formatTimestamp(std::chrono::system_clock::now());
}

std::string JsonFormatter::formatTimestamp(const std::chrono::system_clock::time_point& time_point) {
    if (time_point == std::chrono::system_clock::time_point{}) {
        return "null";
    }
    
    auto time_t = std::chrono::system_clock::to_time_t(time_point);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        time_point.time_since_epoch()) % 1000;
    
    std::ostringstream oss;
    oss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%S");
    oss << '.' << std::setfill('0') << std::setw(3) << ms.count() << 'Z';
    
    return oss.str();
}

std::string JsonFormatter::vehicleTypeToString(uint8_t type) {
    // MAV_TYPE enum mapping
    switch (type) {
        case 0: return "Generic";
        case 1: return "Fixed wing";
        case 2: return "Quadrotor";
        case 3: return "Coaxial helicopter";
        case 4: return "Normal helicopter with tail rotor";
        case 5: return "Ground installation";
        case 6: return "Operator control unit / ground control station";
        case 7: return "Airship, controlled";
        case 8: return "Free balloon, uncontrolled";
        case 9: return "Rocket";
        case 10: return "Ground rover";
        case 11: return "Surface vessel, boat, ship";
        case 12: return "Submarine";
        case 13: return "Hexarotor";
        case 14: return "Octorotor";
        case 15: return "Tricopter";
        case 16: return "Flapping wing";
        case 17: return "Kite";
        case 18: return "Onboard companion controller";
        case 19: return "Tiltrotor VTOL";
        case 20: return "VTOL reserved 5";
        case 21: return "Gimbal";
        case 22: return "ADSB system";
        case 23: return "Steerable, nonrigid airfoil";
        case 24: return "Dodecarotor";
        case 25: return "Camera";
        case 26: return "Charging station";
        case 27: return "FLARM collision avoidance system";
        case 28: return "Servo";
        case 29: return "Open Drone ID";
        case 30: return "Decarotor";
        case 31: return "Battery";
        case 32: return "Parachute";
        case 33: return "Log";
        case 34: return "OSD";
        case 35: return "IMU";
        case 36: return "GPS";
        case 37: return "Winch";
        case 43: return "Generic multirotor";
        default: return "Unknown (" + std::to_string(type) + ")";
    }
}

std::string JsonFormatter::autopilotToString(uint8_t autopilot) {
    // MAV_AUTOPILOT enum mapping
    switch (autopilot) {
        case 0: return "Generic";
        case 3: return "ArduPilot";
        case 4: return "PX4";
        case 12: return "Ardupilot";
        default: return "Unknown (" + std::to_string(autopilot) + ")";
    }
}

std::string JsonFormatter::batteryFunctionToString(uint8_t function) {
    // MAV_BATTERY_FUNCTION enum mapping
    switch (function) {
        case 0: return "Unknown";
        case 1: return "All";
        case 2: return "Propulsion";
        case 3: return "Avionics";
        case 4: return "Payload";
        default: return "Unknown (" + std::to_string(function) + ")";
    }
}

std::string JsonFormatter::batteryTypeToString(uint8_t type) {
    // MAV_BATTERY_TYPE enum mapping
    switch (type) {
        case 0: return "Unknown";
        case 1: return "LiPo";
        case 2: return "LiIon";
        case 3: return "LiFe";
        case 4: return "LiMn";
        case 5: return "NiCd";
        case 6: return "NiMh";
        case 7: return "LFP";
        case 8: return "Influx";
        default: return "Unknown (" + std::to_string(type) + ")";
    }
}

} // namespace FlightCollector
