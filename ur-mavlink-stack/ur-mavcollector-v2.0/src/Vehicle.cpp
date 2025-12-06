#include "Vehicle.h"
#include "BoardIdentifier.h"
#include <sstream>
#include <iomanip>
#include <bitset>
#include <cstring>
#include "../thirdparty/nlohmann/json.hpp"

using json = nlohmann::json;

Vehicle::Vehicle() : _hasAutopilotVersion(false), _hasGPSData(false), _hasSystemStatus(false), _hasDeviceInfo(false) {
    memset(&_autopilotVersion, 0, sizeof(_autopilotVersion));
}

void Vehicle::setAutopilotVersionInfo(const MavlinkAutopilotVersionInfo& info) {
    _autopilotVersion = info;
    _hasAutopilotVersion = true;
}

const MavlinkAutopilotVersionInfo& Vehicle::getAutopilotVersionInfo() const {
    return _autopilotVersion;
}

std::string Vehicle::getBoardIdentification() const {
    if (!_hasAutopilotVersion) {
        return "Unknown (no autopilot version received)";
    }
    
    return BoardIdentifier::instance().identifyBoard(_autopilotVersion.vendor_id, _autopilotVersion.product_id);
}

std::string Vehicle::getBoardClass() const {
    if (!_hasAutopilotVersion) {
        return "Unknown";
    }
    
    return BoardIdentifier::instance().getBoardClass(_autopilotVersion.vendor_id, _autopilotVersion.product_id);
}

std::string Vehicle::getBoardName() const {
    if (!_hasAutopilotVersion) {
        return "Unknown Board";
    }
    
    return BoardIdentifier::instance().getBoardName(_autopilotVersion.vendor_id, _autopilotVersion.product_id);
}

bool Vehicle::hasAutopilotVersion() const {
    return _hasAutopilotVersion;
}

void Vehicle::setGPSData(const MavlinkGPSData& data) {
    _gpsData = data;
    _hasGPSData = true;
}

const MavlinkGPSData& Vehicle::getGPSData() const {
    return _gpsData;
}

bool Vehicle::hasGPSData() const {
    return _hasGPSData;
}

void Vehicle::setSystemStatus(const MavlinkSystemStatus& data) {
    _systemStatus = data;
    _hasSystemStatus = true;
}

const MavlinkSystemStatus& Vehicle::getSystemStatus() const {
    return _systemStatus;
}

void Vehicle::setDeviceInfo(const DeviceInfo& info) {
    _deviceInfo = info;
    _hasDeviceInfo = true;
}

const DeviceInfo& Vehicle::getDeviceInfo() const {
    return _deviceInfo;
}

bool Vehicle::hasDeviceInfo() const {
    return _hasDeviceInfo;
}

std::string Vehicle::getVehicleInfoJson() const {
    json vehicleInfo;
    
    // Device information
    if (_hasDeviceInfo) {
        vehicleInfo["device"] = {
            {"devicePath", _deviceInfo.devicePath},
            {"baudrate", _deviceInfo.baudrate},
            {"systemId", _deviceInfo.systemId},
            {"componentId", _deviceInfo.componentId},
            {"mavlinkVersion", _deviceInfo.mavlinkVersion},
            {"state", _deviceInfo.state},
            {"timestamp", _deviceInfo.timestamp},
            {"autopilotType", _deviceInfo.autopilotType},
            {"boardClass", _deviceInfo.boardClass},
            {"boardName", _deviceInfo.boardName},
            {"deviceName", _deviceInfo.deviceName},
            {"manufacturer", _deviceInfo.manufacturer},
            {"serialNumber", _deviceInfo.serialNumber},
            {"vendorId", _deviceInfo.vendorId},
            {"productId", _deviceInfo.productId},
            {"isValid", _deviceInfo.isValid}
        };
    }
    
    // Autopilot version information
    if (_hasAutopilotVersion) {
        vehicleInfo["autopilot"] = {
            {"capabilities", _autopilotVersion.capabilities},
            {"flight_sw_version", _autopilotVersion.flight_sw_version},
            {"middleware_sw_version", _autopilotVersion.middleware_sw_version},
            {"os_sw_version", _autopilotVersion.os_sw_version},
            {"board_version", _autopilotVersion.board_version},
            {"vendor_id", _autopilotVersion.vendor_id},
            {"product_id", _autopilotVersion.product_id},
            {"uid", _autopilotVersion.uid},
            {"board_identification", getBoardIdentification()},
            {"board_class", getBoardClass()},
            {"board_name", getBoardName()}
        };
    }
    
    // GPS information
    if (_hasGPSData) {
        vehicleInfo["gps"] = {
            {"state", static_cast<int>(_gpsData.state)},
            {"fix_type", static_cast<int>(_gpsData.fix_type)},
            {"satellites_visible", static_cast<int>(_gpsData.satellites_visible)},
            {"satellites_used", static_cast<int>(_gpsData.satellites_used)},
            {"has_origin", _gpsData.has_origin},
            {"has_position", _gpsData.has_position},
            {"position_time_usec", _gpsData.position_time_usec},
            {"position", {
                {"latitude_deg", _gpsData.latitude / 1e7},
                {"longitude_deg", _gpsData.longitude / 1e7},
                {"altitude_m", _gpsData.altitude / 1000.0f},
                {"relative_altitude_m", _gpsData.relative_altitude / 1000.0f},
                {"velocity_x_ms", _gpsData.velocity_x},
                {"velocity_y_ms", _gpsData.velocity_y},
                {"velocity_z_ms", _gpsData.velocity_z},
                {"speed_ms", std::sqrt(_gpsData.velocity_x * _gpsData.velocity_x + _gpsData.velocity_y * _gpsData.velocity_y)}
            }},
            {"estimator_type", static_cast<int>(_gpsData.estimator_type)}
        };
    }
    
    // System status information
    if (_hasSystemStatus) {
        vehicleInfo["system_status"] = {
            {"sensors", {
                {"present_bitmap", _systemStatus.onboard_control_sensors_present},
                {"enabled_bitmap", _systemStatus.onboard_control_sensors_enabled},
                {"health_bitmap", _systemStatus.onboard_control_sensors_health}
            }},
            {"performance", {
                {"cpu_load_percent", _systemStatus.load / 10.0f},
                {"cpu_load_raw", _systemStatus.load}
            }},
            {"battery", {
                {"voltage_mv", _systemStatus.voltage_battery},
                {"voltage_v", _systemStatus.voltage_battery / 1000.0f},
                {"current_ca", _systemStatus.current_battery},
                {"current_a", _systemStatus.current_battery / 100.0f},
                {"remaining_percent", _systemStatus.battery_remaining}
            }},
            {"communication", {
                {"drop_rate_percent", _systemStatus.drop_rate_comm / 100.0f},
                {"drop_rate_raw", _systemStatus.drop_rate_comm},
                {"errors_count", _systemStatus.errors_comm}
            }},
            {"autopilot_errors", {
                {"errors_count1", _systemStatus.errors_count1},
                {"errors_count2", _systemStatus.errors_count2},
                {"errors_count3", _systemStatus.errors_count3},
                {"errors_count4", _systemStatus.errors_count4}
            }}
        };
    }
    
    // Overall status
    vehicleInfo["status"] = {
        {"has_device_info", _hasDeviceInfo},
        {"has_autopilot_version", _hasAutopilotVersion},
        {"has_gps_data", _hasGPSData},
        {"has_system_status", _hasSystemStatus},
        {"is_complete", _hasDeviceInfo && _hasAutopilotVersion && _hasGPSData && _hasSystemStatus}
    };
    
    return vehicleInfo.dump();
}
