#include "JsonDataPrinter.h"
#include <iomanip>
#include <sstream>
#include "FactMetaData.h"
#include "ParameterManager.h"

JsonDataPrinter::JsonDataPrinter(const JsonConfig& config) 
    : _config(config)
{
    _jsonOutputConfig = config.getJsonOutputConfig();
}

std::string JsonDataPrinter::printVehicleInfo(Vehicle* vehicle) const
{
    if (!vehicle || !_config.shouldPrint("vehicle_info")) {
        return "";
    }
    
    nlohmann::json j;
    
    auto fields = _config.getFieldsForType("vehicle_info");
    nlohmann::json vehicleData;
    
    if (fields.empty() || std::find(fields.begin(), fields.end(), "system_id") != fields.end()) {
        vehicleData["system_id"] = static_cast<int>(vehicle->systemId());
    }
    if (fields.empty() || std::find(fields.begin(), fields.end(), "component_id") != fields.end()) {
        vehicleData["component_id"] = static_cast<int>(vehicle->componentId());
    }
    if (fields.empty() || std::find(fields.begin(), fields.end(), "vehicle_type") != fields.end()) {
        vehicleData["vehicle_type"] = vehicle->vehicleTypeString();
    }
    if (fields.empty() || std::find(fields.begin(), fields.end(), "autopilot_type") != fields.end()) {
        vehicleData["autopilot_type"] = vehicle->autopilotTypeString();
    }
    if (fields.empty() || std::find(fields.begin(), fields.end(), "flight_mode") != fields.end()) {
        vehicleData["flight_mode"] = vehicle->flightMode();
    }
    if (fields.empty() || std::find(fields.begin(), fields.end(), "armed") != fields.end()) {
        vehicleData["armed"] = vehicle->armed();
    }
    if (fields.empty() || std::find(fields.begin(), fields.end(), "flying") != fields.end()) {
        vehicleData["flying"] = vehicle->flying();
    }
    
    j["vehicle_info"] = vehicleData;
    return formatJson(j);
}

std::string JsonDataPrinter::printTelemetryData(Vehicle* vehicle) const
{
    if (!vehicle) {
        return "";
    }
    
    nlohmann::json j;
    
    nlohmann::json telemetry;
    
    // Main vehicle facts
    addFactToJson(telemetry, vehicle, "roll", "roll");
    addFactToJson(telemetry, vehicle, "pitch", "pitch");
    addFactToJson(telemetry, vehicle, "heading", "heading");
    addFactToJson(telemetry, vehicle, "groundSpeed", "ground_speed");
    addFactToJson(telemetry, vehicle, "altitudeAMSL", "altitude_amsl");
    addFactToJson(telemetry, vehicle, "altitudeRelative", "altitude_relative");
    addFactToJson(telemetry, vehicle, "climbRate", "climb_rate");
    addFactToJson(telemetry, vehicle, "throttlePct", "throttle_percent");
    
    j["telemetry_data"] = telemetry;
    return formatJson(j);
}

std::string JsonDataPrinter::printSystemStatus(Vehicle* vehicle) const
{
    if (!vehicle || !_config.shouldPrint("system_status")) {
        return "";
    }
    
    nlohmann::json j;
    
    auto systemStatusGroup = vehicle->systemStatusFactGroup();
    if (!systemStatusGroup) {
        return formatJson(j);
    }
    
    auto fields = _config.getFieldsForType("system_status");
    nlohmann::json statusData;
    
    addFactsToJson(statusData, systemStatusGroup, fields, {
        {"sensor_present", "sensorPresent"},
        {"sensor_health", "sensorHealth"},
        {"sensor_errors", "sensorErrors"},
        {"control_sensors_present", "onboardControlSensorsPresent"},
        {"control_sensors_health", "onboardControlSensorsHealth"},
        {"control_sensors_errors", "onboardControlSensorsErrors"}
    });
    
    // Decode sensor bitfields into meaningful names
    if (fields.empty() || std::find(fields.begin(), fields.end(), "control_sensors_present_decoded") != fields.end()) {
        auto presentFact = systemStatusGroup->getFact("onboardControlSensorsPresent");
        if (presentFact) {
            auto value = presentFact->cookedValue();
            if (std::holds_alternative<uint32_t>(value)) {
                uint32_t bitfield = std::get<uint32_t>(value);
                statusData["control_sensors_present_decoded"] = decodeSensorBitfield(bitfield);
            }
        }
    }
    
    if (fields.empty() || std::find(fields.begin(), fields.end(), "control_sensors_health_decoded") != fields.end()) {
        auto healthFact = systemStatusGroup->getFact("onboardControlSensorsHealth");
        if (healthFact) {
            auto value = healthFact->cookedValue();
            if (std::holds_alternative<uint32_t>(value)) {
                uint32_t bitfield = std::get<uint32_t>(value);
                statusData["control_sensors_health_decoded"] = decodeSensorBitfield(bitfield);
            }
        }
    }
    
    // Analyze sensor status and provide warnings
    if (fields.empty() || std::find(fields.begin(), fields.end(), "sensor_status_analysis") != fields.end()) {
        auto presentFact = systemStatusGroup->getFact("onboardControlSensorsPresent");
        auto healthFact = systemStatusGroup->getFact("onboardControlSensorsHealth");
        
        if (presentFact && healthFact) {
            auto presentValue = presentFact->cookedValue();
            auto healthValue = healthFact->cookedValue();
            
            if (std::holds_alternative<uint32_t>(presentValue) && std::holds_alternative<uint32_t>(healthValue)) {
                uint32_t presentBitfield = std::get<uint32_t>(presentValue);
                uint32_t healthBitfield = std::get<uint32_t>(healthValue);
                statusData["sensor_status_analysis"] = analyzeSensorStatus(presentBitfield, healthBitfield);
            }
        }
    }
    
    j["system_status"] = statusData;
    return formatJson(j);
}

std::string JsonDataPrinter::printGpsData(Vehicle* vehicle) const
{
    if (!vehicle || !_config.shouldPrint("gps_data")) {
        return "";
    }
    
    nlohmann::json j;
    
    auto gpsGroup = vehicle->gpsFactGroup();
    if (!gpsGroup) {
        return formatJson(j);
    }
    
    auto fields = _config.getFieldsForType("gps_data");
    nlohmann::json gpsData;
    
    addFactsToJson(gpsData, gpsGroup, fields, {
        {"lat", "lat"},
        {"lon", "lon"},
        {"alt", "alt"},
        {"mgrs", "mgrs"},
        {"hdop", "hdop"},
        {"vdop", "vdop"},
        {"eph", "eph"},
        {"epv", "epv"},
        {"ground_speed", "groundSpeed"},
        {"course_over_ground", "courseOverGround"},
        {"satellites_visible", "satellitesVisible"},
        {"fix_type", "fixType"}
    });
    
    // Convert lat/lon from degreesE7 to degrees for better readability
    if (gpsData.contains("lat") && gpsData.contains("lon")) {
        auto latFact = gpsGroup->getFact("lat");
        auto lonFact = gpsGroup->getFact("lon");
        if (latFact && lonFact) {
            gpsData["latitude_deg"] = safeGetVariant<int32_t>(latFact->cookedValue()) / 1e7;
            gpsData["longitude_deg"] = safeGetVariant<int32_t>(lonFact->cookedValue()) / 1e7;
        }
    }
    
    j["gps_data"] = gpsData;
    return formatJson(j);
}

std::string JsonDataPrinter::printGps2Data(Vehicle* vehicle) const
{
    if (!vehicle || !_config.shouldPrint("gps2_data")) {
        return "";
    }
    
    nlohmann::json j;
    
    auto gps2Group = vehicle->gps2FactGroup();
    if (!gps2Group) {
        return formatJson(j);
    }
    
    auto fields = _config.getFieldsForType("gps2_data");
    nlohmann::json gps2Data;
    
    addFactsToJson(gps2Data, gps2Group, fields, {
        {"lat", "lat"},
        {"lon", "lon"},
        {"alt", "alt"},
        {"hdop", "hdop"},
        {"vdop", "vdop"},
        {"eph", "eph"},
        {"epv", "epv"},
        {"ground_speed", "groundSpeed"},
        {"course_over_ground", "courseOverGround"},
        {"satellites_visible", "satellitesVisible"},
        {"fix_type", "fixType"}
    });
    
    // Convert lat/lon from degreesE7 to degrees for better readability
    if (gps2Data.contains("lat") && gps2Data.contains("lon")) {
        auto latFact = gps2Group->getFact("lat");
        auto lonFact = gps2Group->getFact("lon");
        if (latFact && lonFact) {
            gps2Data["latitude_deg"] = safeGetVariant<int32_t>(latFact->cookedValue()) / 1e7;
            gps2Data["longitude_deg"] = safeGetVariant<int32_t>(lonFact->cookedValue()) / 1e7;
        }
    }
    
    j["gps2_data"] = gps2Data;
    return formatJson(j);
}

std::string JsonDataPrinter::printBatteryData(Vehicle* vehicle) const
{
    if (!vehicle || !_config.shouldPrint("battery_data")) {
        return "";
    }
    
    nlohmann::json j;
    
    auto batteryGroup = vehicle->batteryFactGroup();
    if (!batteryGroup) {
        return formatJson(j);
    }
    
    auto fields = _config.getFieldsForType("battery_data");
    nlohmann::json batteryData;
    
    // Add basic battery facts
    addFactsToJson(batteryData, batteryGroup, fields, {
        {"voltage", "voltage"},
        {"current", "current"},
        {"percent", "percent"},
        {"consumed", "consumed"},
        {"remaining", "remaining"},
        {"temperature", "temperature"},
        {"id", "id"},
        {"function", "function"},
        {"type", "type"},
        {"cell_count", "cellCount"},
        {"time_remaining", "timeRemaining"},
        {"charge_state", "chargeState"},
        {"mode", "mode"},
        {"fault_bitmask", "faultBitmask"},
        {"cell_count_detected", "cellCountDetected"},
        {"battery_count", "batteryCount"}
    });
    
    // Add decoded enum values
    if (fields.empty() || std::find(fields.begin(), fields.end(), "function_name") != fields.end()) {
        auto functionFact = batteryGroup->getFact("function");
        if (functionFact) {
            auto value = functionFact->cookedValue();
            if (std::holds_alternative<uint8_t>(value)) {
                batteryData["function_name"] = getBatteryFunctionName(std::get<uint8_t>(value));
            }
        }
    }
    
    if (fields.empty() || std::find(fields.begin(), fields.end(), "type_name") != fields.end()) {
        auto typeFact = batteryGroup->getFact("type");
        if (typeFact) {
            auto value = typeFact->cookedValue();
            if (std::holds_alternative<uint8_t>(value)) {
                batteryData["type_name"] = getBatteryTypeName(std::get<uint8_t>(value));
            }
        }
    }
    
    if (fields.empty() || std::find(fields.begin(), fields.end(), "charge_state_name") != fields.end()) {
        auto chargeStateFact = batteryGroup->getFact("chargeState");
        if (chargeStateFact) {
            auto value = chargeStateFact->cookedValue();
            if (std::holds_alternative<uint8_t>(value)) {
                batteryData["charge_state_name"] = getBatteryChargeStateName(std::get<uint8_t>(value));
            }
        }
    }
    
    if (fields.empty() || std::find(fields.begin(), fields.end(), "mode_name") != fields.end()) {
        auto modeFact = batteryGroup->getFact("mode");
        if (modeFact) {
            auto value = modeFact->cookedValue();
            if (std::holds_alternative<uint8_t>(value)) {
                batteryData["mode_name"] = getBatteryModeName(std::get<uint8_t>(value));
            }
        }
    }
    
    if (fields.empty() || std::find(fields.begin(), fields.end(), "faults_decoded") != fields.end()) {
        auto faultFact = batteryGroup->getFact("faultBitmask");
        if (faultFact) {
            auto value = faultFact->cookedValue();
            if (std::holds_alternative<uint32_t>(value)) {
                batteryData["faults_decoded"] = decodeBatteryFaults(std::get<uint32_t>(value));
            }
        }
    }
    
    // Add cell voltages dynamically based on detected cell count - QGC style
    if (fields.empty() || std::find(fields.begin(), fields.end(), "cell_voltages") != fields.end()) {
        auto cellCountFact = batteryGroup->getFact("cellCountDetected");
        
        if (cellCountFact) {
            auto cellCountValue = cellCountFact->cookedValue();
            if (std::holds_alternative<uint8_t>(cellCountValue) && std::get<uint8_t>(cellCountValue) > 0) {
                uint8_t cellCount = std::get<uint8_t>(cellCountValue);
                nlohmann::json cellVoltages = nlohmann::json::array();
                
                // Get stored cell voltages
                for (uint8_t i = 0; i < cellCount; i++) {
                    std::string cellFactName = "cell" + std::to_string(i + 1) + "Voltage";
                    auto cellFact = batteryGroup->getFact(cellFactName);
                    if (cellFact) {
                        auto cellValue = cellFact->cookedValue();
                        if (std::holds_alternative<float>(cellValue)) {
                            float voltage = std::get<float>(cellValue);
                            if (!std::isnan(voltage) && voltage > 0.0f) {
                                cellVoltages.push_back(voltage);
                            } else {
                                cellVoltages.push_back(nullptr);  // QGC-style NaN indication
                            }
                        } else {
                            cellVoltages.push_back(nullptr);
                        }
                    } else {
                        cellVoltages.push_back(nullptr);
                    }
                }
                
                batteryData["cell_voltages"] = cellVoltages;
            } else {
                batteryData["cell_voltages"] = nullptr;  // QGC-style: null for no data
            }
        } else {
            batteryData["cell_voltages"] = nullptr;
        }
    }
    
    // Add battery configuration analysis
    if (fields.empty() || std::find(fields.begin(), fields.end(), "configuration_analysis") != fields.end()) {
        batteryData["configuration_analysis"] = analyzeBatteryConfiguration(vehicle);
    }
    
    // Add battery parameters
    if (fields.empty() || std::find(fields.begin(), fields.end(), "parameters") != fields.end()) {
        batteryData["parameters"] = extractBatteryParameters(vehicle);
    }
    
    j["battery_data"] = batteryData;
    return formatJson(j);
}

std::string JsonDataPrinter::printRcData(Vehicle* vehicle) const
{
    if (!vehicle || !_config.shouldPrint("rc_data")) {
        return "";
    }
    
    nlohmann::json j;
    
    auto rcGroup = vehicle->rcFactGroup();
    if (!rcGroup) {
        return formatJson(j);
    }
    
    auto fields = _config.getFieldsForType("rc_data");
    nlohmann::json rcData;
    
    addFactsToJson(rcData, rcGroup, fields, {
        {"rssi", "rssi"},
        {"rssi_dbm", "rssiDbm"},
        {"rssi_percent", "rssiPercent"},
        {"channel_count", "channelCount"}
    });
    
    j["rc_data"] = rcData;
    return formatJson(j);
}

std::string JsonDataPrinter::printVibrationData(Vehicle* vehicle) const
{
    if (!vehicle || !_config.shouldPrint("vibration_data")) {
        return "";
    }
    
    nlohmann::json j;
    
    auto vibrationGroup = vehicle->vibrationFactGroup();
    if (!vibrationGroup) {
        return formatJson(j);
    }
    
    auto fields = _config.getFieldsForType("vibration_data");
    nlohmann::json vibrationData;
    
    addFactsToJson(vibrationData, vibrationGroup, fields, {
        {"vibration_x", "vibrationX"},
        {"vibration_y", "vibrationY"},
        {"vibration_z", "vibrationZ"},
        {"clipping_x", "clippingX"},
        {"clipping_y", "clippingY"},
        {"clipping_z", "clippingZ"}
    });
    
    j["vibration_data"] = vibrationData;
    return formatJson(j);
}

std::string JsonDataPrinter::printTemperatureData(Vehicle* vehicle) const
{
    if (!vehicle || !_config.shouldPrint("temperature_data")) {
        return "";
    }
    
    nlohmann::json j;
    
    auto temperatureGroup = vehicle->temperatureFactGroup();
    if (!temperatureGroup) {
        return formatJson(j);
    }
    
    auto fields = _config.getFieldsForType("temperature_data");
    nlohmann::json temperatureData;
    
    addFactsToJson(temperatureData, temperatureGroup, fields, {
        {"temperature1", "temperature1"},
        {"temperature2", "temperature2"},
        {"temperature3", "temperature3"}
    });
    
    j["temperature_data"] = temperatureData;
    return formatJson(j);
}

std::string JsonDataPrinter::printEstimatorStatus(Vehicle* vehicle) const
{
    if (!vehicle || !_config.shouldPrint("estimator_status")) {
        return "";
    }
    
    nlohmann::json j;
    
    auto estimatorStatusGroup = vehicle->estimatorStatusFactGroup();
    if (!estimatorStatusGroup) {
        return formatJson(j);
    }
    
    auto fields = _config.getFieldsForType("estimator_status");
    nlohmann::json estimatorData;
    
    addFactsToJson(estimatorData, estimatorStatusGroup, fields, {
        {"flags", "flags"},
        {"innovation_pos_horiz", "innovationPosHoriz"},
        {"innovation_pos_vert", "innovationPosVert"},
        {"innovation_vel_horiz", "innovationVelHoriz"},
        {"innovation_vel_vert", "innovationVelVert"},
        {"innovation_mag", "innovationMag"},
        {"innovation_yaw", "innovationYaw"}
    });
    
    j["estimator_status"] = estimatorData;
    return formatJson(j);
}

std::string JsonDataPrinter::printWindData(Vehicle* vehicle) const
{
    if (!vehicle || !_config.shouldPrint("wind_data")) {
        return "";
    }
    
    nlohmann::json j;
    
    auto windGroup = vehicle->windFactGroup();
    if (!windGroup) {
        return formatJson(j);
    }
    
    auto fields = _config.getFieldsForType("wind_data");
    nlohmann::json windData;
    
    addFactsToJson(windData, windGroup, fields, {
        {"direction", "direction"},
        {"speed", "speed"},
        {"climb", "climb"}
    });
    
    j["wind_data"] = windData;
    return formatJson(j);
}

bool JsonDataPrinter::shouldPrint(const std::string& dataType) const
{
    return _config.shouldPrint(dataType);
}

std::string JsonDataPrinter::printParameters(Vehicle* vehicle) const
{
    if (!vehicle || !_config.shouldPrint("parameters")) {
        return "";
    }
    
    nlohmann::json j;
    
    auto paramManager = vehicle->parameterManager();
    if (!paramManager) {
        return formatJson(j);
    }
    
    auto fields = _config.getFieldsForType("parameters");
    nlohmann::json paramData;
    
    if (fields.empty() || std::find(fields.begin(), fields.end(), "total_count") != fields.end()) {
        auto paramNames = paramManager->parameterNames(1);
        paramData["total_count"] = static_cast<int>(paramNames.size());
    }
    
    if (fields.empty() || std::find(fields.begin(), fields.end(), "load_progress") != fields.end()) {
        paramData["load_progress"] = paramManager->loadProgress();
    }
    
    paramData["parameters_ready"] = paramManager->parametersReady();
    
    if (fields.empty() || std::find(fields.begin(), fields.end(), "critical_params") != fields.end()) {
        nlohmann::json criticalParams;
        const char* criticalParamNames[] = {
            "SYS_AUTOSTART", "SYS_ID_THISMAV", "SYSID_MYGCS", "SYS_COMPANION",
            "SYS_TYPE", "COM_FLTMODE1", "COM_FLTMODE2", "COM_ARMING_CHECK",
            "MPC_XY_VEL_MAX", "MPC_Z_VEL_MAX", "MPC_THR_HOVER", "MC_ROLL_P"
        };
        
        for (const char* paramName : criticalParamNames) {
            auto param = paramManager->getParameter(1, std::string(paramName));
            if (param) {
                criticalParams[paramName] = param->cookedValueString();
            }
        }
        paramData["critical_params"] = criticalParams;
    }
    
    j["parameters"] = paramData;
    return formatJson(j);
}

std::string JsonDataPrinter::printConnectionStats(const MAVLinkUdpConnection* connection) const
{
    if (!connection || !_config.shouldPrint("connection_stats")) {
        return "";
    }
    
    nlohmann::json j;
    
    auto fields = _config.getFieldsForType("connection_stats");
    nlohmann::json connData;
    
    if (fields.empty() || std::find(fields.begin(), fields.end(), "bytes_received") != fields.end()) {
        connData["bytes_received"] = connection->getBytesReceived();
    }
    if (fields.empty() || std::find(fields.begin(), fields.end(), "bytes_sent") != fields.end()) {
        connData["bytes_sent"] = connection->getBytesSent();
    }
    if (fields.empty() || std::find(fields.begin(), fields.end(), "packets_received") != fields.end()) {
        connData["packets_received"] = connection->getPacketsReceived();
    }
    if (fields.empty() || std::find(fields.begin(), fields.end(), "packets_sent") != fields.end()) {
        connData["packets_sent"] = connection->getPacketsSent();
    }
    if (fields.empty() || std::find(fields.begin(), fields.end(), "packets_lost") != fields.end()) {
        connData["packets_lost"] = connection->getPacketsLost();
    }
    if (fields.empty() || std::find(fields.begin(), fields.end(), "mavlink_version") != fields.end()) {
        connData["mavlink_version"] = connection->getDetectedMavlinkVersion();
    }
    if (fields.empty() || std::find(fields.begin(), fields.end(), "connection_healthy") != fields.end()) {
        connData["connection_healthy"] = connection->isConnectionHealthy();
    }
    
    j["connection_stats"] = connData;
    return formatJson(j);
}

std::string JsonDataPrinter::printHeartbeat(Vehicle* vehicle) const
{
    if (!vehicle || !_config.shouldPrint("heartbeat")) {
        return "";
    }
    
    nlohmann::json j;
    
    auto fields = _config.getFieldsForType("heartbeat");
    nlohmann::json heartbeatData;
    
    if (fields.empty() || std::find(fields.begin(), fields.end(), "type") != fields.end()) {
        heartbeatData["type"] = static_cast<int>(vehicle->vehicleType());
    }
    if (fields.empty() || std::find(fields.begin(), fields.end(), "autopilot") != fields.end()) {
        heartbeatData["autopilot"] = static_cast<int>(vehicle->autopilotType());
    }
    if (fields.empty() || std::find(fields.begin(), fields.end(), "system_status") != fields.end()) {
        heartbeatData["system_status"] = vehicle->systemStatusString();
    }
    if (fields.empty() || std::find(fields.begin(), fields.end(), "base_mode") != fields.end()) {
        heartbeatData["base_mode"] = static_cast<int>(vehicle->baseMode());
    }
    if (fields.empty() || std::find(fields.begin(), fields.end(), "custom_mode") != fields.end()) {
        heartbeatData["custom_mode"] = vehicle->customMode();
    }
    
    j["heartbeat"] = heartbeatData;
    return formatJson(j);
}

std::string JsonDataPrinter::printAutopilotVersion(Vehicle* vehicle) const
{
    if (!vehicle || !_config.shouldPrint("autopilot_version")) {
        return "";
    }
    
    nlohmann::json j;
    
    auto fields = _config.getFieldsForType("autopilot_version");
    nlohmann::json versionData;
    
    if (fields.empty() || std::find(fields.begin(), fields.end(), "board_identification") != fields.end()) {
        versionData["board_identification"] = vehicle->boardIdentification();
    }
    if (fields.empty() || std::find(fields.begin(), fields.end(), "board_class") != fields.end()) {
        versionData["board_class"] = vehicle->boardClass();
    }
    if (fields.empty() || std::find(fields.begin(), fields.end(), "board_name") != fields.end()) {
        versionData["board_name"] = vehicle->boardName();
    }
    if (fields.empty() || std::find(fields.begin(), fields.end(), "vendor_id") != fields.end()) {
        versionData["vendor_id"] = vehicle->vendorId();
    }
    if (fields.empty() || std::find(fields.begin(), fields.end(), "product_id") != fields.end()) {
        versionData["product_id"] = vehicle->productId();
    }
    if (fields.empty() || std::find(fields.begin(), fields.end(), "uid") != fields.end()) {
        versionData["uid"] = vehicle->uid();
    }
    if (fields.empty() || std::find(fields.begin(), fields.end(), "board_version") != fields.end()) {
        versionData["board_version"] = vehicle->boardVersion();
    }
    if (fields.empty() || std::find(fields.begin(), fields.end(), "flight_sw_version") != fields.end()) {
        versionData["flight_sw_version"] = vehicle->flightSwVersionString();
    }
    if (fields.empty() || std::find(fields.begin(), fields.end(), "flight_sw_version_raw") != fields.end()) {
        versionData["flight_sw_version_raw"] = vehicle->flightSwVersion();
    }
    if (fields.empty() || std::find(fields.begin(), fields.end(), "middleware_sw_version") != fields.end()) {
        versionData["middleware_sw_version"] = vehicle->middlewareSwVersionString();
    }
    if (fields.empty() || std::find(fields.begin(), fields.end(), "middleware_sw_version_raw") != fields.end()) {
        versionData["middleware_sw_version_raw"] = vehicle->middlewareSwVersion();
    }
    if (fields.empty() || std::find(fields.begin(), fields.end(), "os_sw_version") != fields.end()) {
        versionData["os_sw_version"] = vehicle->osSwVersionString();
    }
    if (fields.empty() || std::find(fields.begin(), fields.end(), "os_sw_version_raw") != fields.end()) {
        versionData["os_sw_version_raw"] = vehicle->osSwVersion();
    }
    if (fields.empty() || std::find(fields.begin(), fields.end(), "flight_custom_version") != fields.end()) {
        versionData["flight_custom_version"] = vehicle->flightCustomVersionString();
    }
    if (fields.empty() || std::find(fields.begin(), fields.end(), "capabilities") != fields.end()) {
        versionData["capabilities"] = vehicle->capabilitiesString();
    }
    
    j["autopilot_version"] = versionData;
    return formatJson(j);
}

std::string JsonDataPrinter::printAllData(Vehicle* vehicle, const MAVLinkUdpConnection* connection) const
{
    nlohmann::json j;
    
    // Add all enabled data types
    if (_config.shouldPrint("vehicle_info")) {
        auto vehicleInfo = printVehicleInfo(vehicle);
        if (!vehicleInfo.empty()) {
            j["vehicle_info"] = nlohmann::json::parse(vehicleInfo);
        }
    }
    
    if (_config.shouldPrint("system_status")) {
        auto systemStatus = printSystemStatus(vehicle);
        if (!systemStatus.empty()) {
            j["system_status"] = nlohmann::json::parse(systemStatus);
        }
    }
    
    if (_config.shouldPrint("gps_data")) {
        auto gpsData = printGpsData(vehicle);
        if (!gpsData.empty()) {
            j["gps_data"] = nlohmann::json::parse(gpsData);
        }
    }
    
    if (_config.shouldPrint("gps2_data")) {
        auto gps2Data = printGps2Data(vehicle);
        if (!gps2Data.empty()) {
            j["gps2_data"] = nlohmann::json::parse(gps2Data);
        }
    }
    
    if (_config.shouldPrint("battery_data")) {
        auto batteryData = printBatteryData(vehicle);
        if (!batteryData.empty()) {
            j["battery_data"] = nlohmann::json::parse(batteryData);
        }
    }
    
    if (_config.shouldPrint("rc_data")) {
        auto rcData = printRcData(vehicle);
        if (!rcData.empty()) {
            j["rc_data"] = nlohmann::json::parse(rcData);
        }
    }
    
    if (_config.shouldPrint("vibration_data")) {
        auto vibrationData = printVibrationData(vehicle);
        if (!vibrationData.empty()) {
            j["vibration_data"] = nlohmann::json::parse(vibrationData);
        }
    }
    
    if (_config.shouldPrint("temperature_data")) {
        auto temperatureData = printTemperatureData(vehicle);
        if (!temperatureData.empty()) {
            j["temperature_data"] = nlohmann::json::parse(temperatureData);
        }
    }
    
    if (_config.shouldPrint("estimator_status")) {
        auto estimatorStatus = printEstimatorStatus(vehicle);
        if (!estimatorStatus.empty()) {
            j["estimator_status"] = nlohmann::json::parse(estimatorStatus);
        }
    }
    
    if (_config.shouldPrint("wind_data")) {
        auto windData = printWindData(vehicle);
        if (!windData.empty()) {
            j["wind_data"] = nlohmann::json::parse(windData);
        }
    }
    
    if (_config.shouldPrint("parameters")) {
        auto parameters = printParameters(vehicle);
        if (!parameters.empty()) {
            j["parameters"] = nlohmann::json::parse(parameters);
        }
    }
    
    if (_config.shouldPrint("connection_stats") && connection) {
        auto connectionStats = printConnectionStats(connection);
        if (!connectionStats.empty()) {
            j["connection_stats"] = nlohmann::json::parse(connectionStats);
        }
    }
    
    if (_config.shouldPrint("heartbeat")) {
        auto heartbeat = printHeartbeat(vehicle);
        if (!heartbeat.empty()) {
            j["heartbeat"] = nlohmann::json::parse(heartbeat);
        }
    }
    
    if (_config.shouldPrint("autopilot_version")) {
        auto autopilotVersion = printAutopilotVersion(vehicle);
        if (!autopilotVersion.empty()) {
            j["autopilot_version"] = nlohmann::json::parse(autopilotVersion);
        }
    }
    
    return formatJson(j);
}

void JsonDataPrinter::addTimestamp(nlohmann::json& j) const
{
    if (_jsonOutputConfig.value("include_timestamp", true)) {
        auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        j["timestamp"] = timestamp;
    }
}

void JsonDataPrinter::addMetadata(nlohmann::json& j, const std::string& dataType) const
{
    if (_jsonOutputConfig.value("include_metadata", true)) {
        nlohmann::json metadata;
        metadata["data_type"] = dataType;
        metadata["collector_version"] = "1.0";
        metadata["output_format"] = "json";
        j["metadata"] = metadata;
    }
}

std::string JsonDataPrinter::formatJson(const nlohmann::json& j) const
{
    int indent = _jsonOutputConfig.value("pretty_print", true) ? 2 : -1;
    return j.dump(indent);
}

template<typename T>
T JsonDataPrinter::safeGetVariant(const FactMetaData::ValueVariant_t& variant, T defaultValue) const
{
    try {
        if (std::holds_alternative<T>(variant)) {
            return std::get<T>(variant);
        }
    } catch (const std::bad_variant_access&) {
        // Fall through to default value
    }
    return defaultValue;
}

void JsonDataPrinter::addFactToJson(nlohmann::json& j, const FactGroup* factGroup, 
                                   const std::string& factName, const std::string& jsonKey,
                                   const std::vector<std::string>& fields) const
{
    if (!factGroup || (!fields.empty() && std::find(fields.begin(), fields.end(), jsonKey) == fields.end())) {
        return;
    }
    
    auto fact = factGroup->getFact(factName);
    if (!fact) {
        return;
    }
    
    auto value = fact->cookedValue();
    if (std::holds_alternative<int32_t>(value)) {
        j[jsonKey] = safeGetVariant<int32_t>(value);
    } else if (std::holds_alternative<uint32_t>(value)) {
        j[jsonKey] = safeGetVariant<uint32_t>(value);
    } else if (std::holds_alternative<int16_t>(value)) {
        j[jsonKey] = safeGetVariant<int16_t>(value);
    } else if (std::holds_alternative<uint16_t>(value)) {
        j[jsonKey] = safeGetVariant<uint16_t>(value);
    } else if (std::holds_alternative<int8_t>(value)) {
        j[jsonKey] = safeGetVariant<int8_t>(value);
    } else if (std::holds_alternative<uint8_t>(value)) {
        j[jsonKey] = safeGetVariant<uint8_t>(value);
    } else if (std::holds_alternative<float>(value)) {
        j[jsonKey] = safeGetVariant<float>(value);
    } else if (std::holds_alternative<double>(value)) {
        j[jsonKey] = safeGetVariant<double>(value);
    } else if (std::holds_alternative<std::string>(value)) {
        j[jsonKey] = safeGetVariant<std::string>(value);
    }
}

void JsonDataPrinter::addFactsToJson(nlohmann::json& j, const std::shared_ptr<FactGroup>& factGroup,
                                    const std::vector<std::string>& fields,
                                    const std::map<std::string, std::string>& fieldMapping) const
{
    if (!factGroup) {
        return;
    }
    
    for (const auto& field : fields) {
        std::string factName = field;
        std::string jsonKey = field;
        
        // Check if there's a mapping from field name to fact name
        auto mappingIt = fieldMapping.find(field);
        if (mappingIt != fieldMapping.end()) {
            factName = mappingIt->second;
        }
        
        addFactToJson(j, factGroup.get(), factName, jsonKey, fields);
    }
}

nlohmann::json JsonDataPrinter::decodeSensorBitfield(uint32_t sensorsBitfield) const
{
    nlohmann::json decodedSensors = nlohmann::json::array();
    
    for (int bit = 0; bit < 32; bit++) {
        if (sensorsBitfield & (1U << bit)) {
            std::string sensorName = getSensorName(bit);
            if (!sensorName.empty()) {
                decodedSensors.push_back(sensorName);
            }
        }
    }
    
    return decodedSensors;
}

std::string JsonDataPrinter::getSensorName(uint32_t bitPosition) const
{
    switch (1U << bitPosition) {
        case 0x00000001: return "3D_GYRO";
        case 0x00000002: return "3D_ACCEL";
        case 0x00000004: return "3D_MAG";
        case 0x00000008: return "ABSOLUTE_PRESSURE";
        case 0x00000010: return "DIFFERENTIAL_PRESSURE";
        case 0x00000020: return "GPS";
        case 0x00000040: return "OPTICAL_FLOW";
        case 0x00000080: return "VISION_POSITION";
        case 0x00000100: return "LASER_POSITION";
        case 0x00000200: return "EXTERNAL_GROUND_TRUTH";
        case 0x00000400: return "ANGULAR_RATE_CONTROL";
        case 0x00000800: return "ATTITUDE_STABILIZATION";
        case 0x00001000: return "YAW_POSITION";
        case 0x00002000: return "Z_ALTITUDE_CONTROL";
        case 0x00004000: return "XY_POSITION_CONTROL";
        case 0x00008000: return "MOTOR_OUTPUTS";
        case 0x00010000: return "RC_RECEIVER";
        case 0x00020000: return "3D_GYRO2";
        case 0x00040000: return "3D_ACCEL2";
        case 0x00080000: return "3D_MAG2";
        case 0x00100000: return "GEOFENCE";
        case 0x00200000: return "AHRS";
        case 0x00400000: return "TERRAIN";
        case 0x00800000: return "REVERSE_MOTOR";
        case 0x01000000: return "LOGGING";
        case 0x02000000: return "BATTERY";
        case 0x04000000: return "PROXIMITY";
        case 0x08000000: return "SATCOM";
        case 0x10000000: return "PREARM_CHECK";
        case 0x20000000: return "OBSTACLE_AVOIDANCE";
        case 0x40000000: return "PROPULSION";
        case 0x80000000: return "EXTENSION_USED";
        default: return "";
    }
}

nlohmann::json JsonDataPrinter::analyzeSensorStatus(uint32_t presentBitfield, uint32_t healthBitfield) const
{
    nlohmann::json analysis;
    nlohmann::json healthySensors = nlohmann::json::array();
    nlohmann::json unhealthySensors = nlohmann::json::array();
    nlohmann::json missingSensors = nlohmann::json::array();
    
    bool allSensorsOk = true;
    bool hasCriticalSensors = false;
    
    // Check each sensor bit position
    for (int bit = 0; bit < 32; bit++) {
        uint32_t sensorBit = 1U << bit;
        std::string sensorName = getSensorName(bit);
        
        if (sensorName.empty()) {
            continue; // Skip unused bits
        }
        
        bool isPresent = (presentBitfield & sensorBit) != 0;
        bool isHealthy = (healthBitfield & sensorBit) != 0;
        
        if (isPresent) {
            hasCriticalSensors = true;
            if (isHealthy) {
                healthySensors.push_back(sensorName);
            } else {
                unhealthySensors.push_back(sensorName);
                allSensorsOk = false;
            }
        } else {
            // Only consider certain sensors as "missing" if they're typically expected
            if (sensorBit <= 0x00010000 || sensorBit == 0x02000000) { // Basic sensors + battery
                missingSensors.push_back(sensorName);
            }
        }
    }
    
    // Determine overall status
    std::string status;
    std::string message;
    
    if (!hasCriticalSensors) {
        status = "unknown";
        message = "No critical sensors detected";
    } else if (allSensorsOk && unhealthySensors.empty()) {
        status = "ok";
        message = "All sensors are operational";
    } else if (!unhealthySensors.empty()) {
        status = "warning";
        message = "Some sensors have errors";
    } else {
        status = "error";
        message = "Sensor status unclear";
    }
    
    analysis["overall_status"] = status;
    analysis["message"] = message;
    analysis["all_sensors_ok"] = allSensorsOk;
    analysis["healthy_sensors"] = healthySensors;
    analysis["unhealthy_sensors"] = unhealthySensors;
    analysis["missing_sensors"] = missingSensors;
    analysis["total_present"] = healthySensors.size() + unhealthySensors.size();
    analysis["total_healthy"] = healthySensors.size();
    analysis["total_unhealthy"] = unhealthySensors.size();
    
    return analysis;
}

nlohmann::json JsonDataPrinter::analyzeBatteryConfiguration(Vehicle* vehicle) const
{
    auto batteryGroup = vehicle->batteryFactGroup();
    if (!batteryGroup) {
        return nlohmann::json{};
    }
    
    nlohmann::json analysis;
    
    // Get basic battery information
    auto voltageFact = batteryGroup->getFact("voltage");
    auto currentFact = batteryGroup->getFact("current");
    auto percentFact = batteryGroup->getFact("percent");
    auto cellCountFact = batteryGroup->getFact("cellCountDetected");
    auto batteryCountFact = batteryGroup->getFact("batteryCount");
    auto functionFact = batteryGroup->getFact("function");
    auto typeFact = batteryGroup->getFact("type");
    
    // Enhanced battery detection logic
    float batteryVoltage = voltageFact ? safeGetVariant<float>(voltageFact->cookedValue(), 0.0f) : 0.0f;
    float batteryCurrent = currentFact ? safeGetVariant<float>(currentFact->cookedValue(), 0.0f) : 0.0f;
    uint8_t batteryPercent = percentFact ? safeGetVariant<uint8_t>(percentFact->cookedValue(), 0) : 0;
    uint8_t detectedCellCount = cellCountFact ? safeGetVariant<uint8_t>(cellCountFact->cookedValue(), 0) : 0;
    uint8_t detectedBatteryCount = batteryCountFact ? safeGetVariant<uint8_t>(batteryCountFact->cookedValue(), 0) : 0;
    
    // Improved battery detection
    analysis["battery_detected"] = (batteryVoltage > 0.0f && !std::isnan(batteryVoltage));
    
    // Current monitoring availability - check for valid current readings
    analysis["current_monitoring_available"] = (batteryCurrent != -1.0f && !std::isnan(batteryCurrent));
    
    // Percentage monitoring availability - check for valid percentage
    analysis["percentage_monitoring_available"] = (batteryPercent != 255 && batteryPercent > 0);
    
    // Cell count analysis - only use actual detected data
    if (detectedCellCount > 0) {
        analysis["cell_count"] = detectedCellCount;
    } else {
        analysis["cell_count"] = nullptr;  // No cell count data available
    }
    
    // Battery count analysis
    if (detectedBatteryCount > 0) {
        analysis["battery_count"] = detectedBatteryCount;
    } else {
        analysis["battery_count"] = analysis["battery_detected"].get<bool>() ? 1 : 0;
    }
    
    // Battery function and type analysis
    if (functionFact) {
        auto value = functionFact->cookedValue();
        if (std::holds_alternative<uint8_t>(value)) {
            uint8_t function = std::get<uint8_t>(value);
            analysis["function"] = function;
            analysis["function_name"] = getBatteryFunctionName(function);
        }
    }
    
    if (typeFact) {
        auto value = typeFact->cookedValue();
        if (std::holds_alternative<uint8_t>(value)) {
            uint8_t type = std::get<uint8_t>(value);
            analysis["type"] = type;
            analysis["type_name"] = getBatteryTypeName(type);
        }
    }
    
    // Enhanced battery health assessment
    analysis["health_status"] = _assessBatteryHealth(batteryGroup);
    
    return analysis;
}

nlohmann::json JsonDataPrinter::extractBatteryParameters(Vehicle* vehicle) const
{
    nlohmann::json params;
    
    // Standard battery parameters to extract
    std::vector<std::string> batteryParams = {
        "BAT_N_CELLS", "BAT_V_CHARGED", "BAT_V_EMPTY", "BAT_SOURCE",
        "BAT_R_INTERNAL", "BAT_LOW_THR", "BAT_CRIT_THR", "BAT_EMERG_THR",
        "BAT_CAPACITY", "BAT_VOLT_PIN", "BAT_CURR_PIN", "BAT_VOLT_MULT",
        "BAT_AMP_PERVOLT", "BAT_MONITOR", "BAT_LOW_VOLT", "BAT_HIGH_VOLT",
        "BATT_CAPACITY", "BATT_VOLT_PIN", "BATT_CURR_PIN", "BATT_VOLT_MULT",
        "BATT_AMP_PERVOLT", "BATT_MONITOR", "BATT_LOW_VOLT", "BATT_HIGH_VOLT"
    };
    
    // Try to extract each parameter if available
    for (const auto& paramName : batteryParams) {
        // For now, return empty parameters - this would need parameter system integration
        // In a full implementation, you would query the vehicle's parameter system
        // params[paramName] = vehicle->getParameterValue(paramName);
    }
    
    return params;
}

std::string JsonDataPrinter::getBatteryFunctionName(uint8_t functionValue) const
{
    switch (functionValue) {
        case 0: return "UNKNOWN";
        case 1: return "ALL";
        case 2: return "PROPULSION";
        case 3: return "AVIONICS";
        case 4: return "PAYLOAD";
        default: return "UNKNOWN";
    }
}

std::string JsonDataPrinter::getBatteryTypeName(uint8_t typeValue) const
{
    switch (typeValue) {
        case 0: return "UNKNOWN";
        case 1: return "LIPO";
        case 2: return "LIFE";
        case 3: return "LION";
        case 4: return "NIMH";
        default: return "UNKNOWN";
    }
}

std::string JsonDataPrinter::getBatteryChargeStateName(uint8_t chargeStateValue) const
{
    switch (chargeStateValue) {
        case 0: return "UNDEFINED";
        case 1: return "OK";
        case 2: return "LOW";
        case 3: return "CRITICAL";
        case 4: return "EMERGENCY";
        case 5: return "FAILED";
        case 6: return "UNHEALTHY";
        case 7: return "CHARGING";
        default: return "UNKNOWN";
    }
}

std::string JsonDataPrinter::getBatteryModeName(uint8_t modeValue) const
{
    switch (modeValue) {
        case 0: return "UNKNOWN";
        case 1: return "AUTO_DISCHARGING";
        case 2: return "HOT_SWAP";
        default: return "UNKNOWN";
    }
}

nlohmann::json JsonDataPrinter::decodeBatteryFaults(uint32_t faultBitmask) const
{
    nlohmann::json faults = nlohmann::json::array();
    
    if (faultBitmask & 0x00000001) faults.push_back("DEEP_DISCHARGE");
    if (faultBitmask & 0x00000002) faults.push_back("SPIKES");
    if (faultBitmask & 0x00000004) faults.push_back("CELL_FAIL");
    if (faultBitmask & 0x00000008) faults.push_back("OVER_VOLTAGE");
    if (faultBitmask & 0x00000010) faults.push_back("UNDER_VOLTAGE");
    if (faultBitmask & 0x00000020) faults.push_back("OVER_CURRENT");
    if (faultBitmask & 0x00000040) faults.push_back("OVER_TEMP");
    if (faultBitmask & 0x00000080) faults.push_back("UNDER_TEMP");
    if (faultBitmask & 0x00000100) faults.push_back("CAPACITY_FADE");
    if (faultBitmask & 0x00000200) faults.push_back("UNBALANCED_CELLS");
    if (faultBitmask & 0x00000400) faults.push_back("COMMUNICATION_ERROR");
    if (faultBitmask & 0x00000800) faults.push_back("INTERNAL_FAILURE");
    if (faultBitmask & 0x00001000) faults.push_back("PROTECTION_TRIPPED");
    
    return faults;
}

std::string JsonDataPrinter::_assessBatteryHealth(const std::shared_ptr<FactGroup>& batteryGroup) const
{
    if (!batteryGroup) return "unknown";
    
    auto percentFact = batteryGroup->getFact("percent");
    auto voltageFact = batteryGroup->getFact("voltage");
    auto chargeStateFact = batteryGroup->getFact("chargeState");
    
    if (!percentFact || !voltageFact) return "unknown";
    
    auto percentValue = percentFact->cookedValue();
    auto chargeStateValue = chargeStateFact ? chargeStateFact->cookedValue() : FactMetaData::ValueVariant_t{static_cast<uint8_t>(0)};
    
    uint8_t percent = std::holds_alternative<uint8_t>(percentValue) ? std::get<uint8_t>(percentValue) : 0;
    uint8_t chargeState = std::holds_alternative<uint8_t>(chargeStateValue) ? std::get<uint8_t>(chargeStateValue) : 0;
    
    // Check charge state first
    switch (chargeState) {
        case 3: return "critical";
        case 4: return "emergency";
        case 5: return "failed";
        case 6: return "unhealthy";
        case 2: return "low";
        case 7: return "charging";
        case 1: return "good";
        default:
            // Fall back to percentage-based assessment
            if (percent <= 10) return "critical";
            if (percent <= 20) return "low";
            if (percent <= 50) return "moderate";
            return "good";
    }
}
