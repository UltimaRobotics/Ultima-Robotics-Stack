#ifndef MAVLINK_UDP_CONNECTION_H
#define MAVLINK_UDP_CONNECTION_H

#include <string>
#include <functional>
#include <memory>
#include <cstdint>
#include <chrono>

#ifdef __cplusplus
extern "C" {
#endif

#include <mavlink.h>

#ifdef __cplusplus
}
#endif

// GPS State enumeration (matching gps-collector-api)
enum class GPSState {
    UNKNOWN = 0,
    NO_FIX = 1,
    GPS_FIX = 2,
    DGPS_FIX = 3,
    RTK_FIX = 4,
    RTK_FLOAT = 5
};

// GPS Fix Type enumeration (matching gps-collector-api)
enum class GPSFixType {
    NO_GPS = 0,
    NO_FIX = 1,
    GPS_2D_FIX = 2,
    GPS_3D_FIX = 3,
    DGPS = 4,
    RTK_FLOAT = 5,
    RTK_FIXED = 6,
    STATIC = 7
};

struct MavlinkHeartbeatInfo {
    uint8_t system_id;
    uint8_t component_id;
    uint8_t type;
    uint8_t autopilot;
    uint8_t base_mode;
    uint32_t custom_mode;
    uint8_t system_status;
};

struct MavlinkAutopilotVersionInfo {
    uint64_t capabilities;
    uint64_t uid;
    uint32_t flight_sw_version;
    uint32_t middleware_sw_version;
    uint32_t os_sw_version;
    uint32_t board_version;
    uint16_t vendor_id;
    uint16_t product_id;
    uint8_t flight_custom_version[8];
    uint8_t middleware_custom_version[8];
    uint8_t os_custom_version[8];
    uint8_t uid2[18];
    
    std::string flightSwVersionString() const;
    std::string flightCustomVersionString() const;
    std::string middlewareSwVersionString() const;
    std::string middlewareCustomVersionString() const;
    std::string osSwVersionString() const;
    std::string osCustomVersionString() const;
    std::string boardVersionString() const;
    std::string uid2String() const;
    std::string capabilitiesString() const;
};

struct MavlinkBatteryInfo {
    uint8_t id;
    uint8_t battery_function;
    uint8_t type;
    uint8_t state_of_health;
    uint8_t cells_in_series;
    uint16_t cycle_count;
    uint16_t weight;
    float discharge_minimum_voltage;
    float charging_minimum_voltage;
    float resting_minimum_voltage;
    float charging_maximum_voltage;
    float charging_maximum_current;
    float nominal_voltage;
    float discharge_maximum_current;
    float discharge_maximum_burst_current;
    float design_capacity;
    float full_charge_capacity;
    char manufacture_date[9];
    char serial_number[32];
    char name[50];
};

struct MavlinkBatteryStatus {
    uint8_t id;
    uint8_t battery_function;
    uint8_t type;
    int16_t temperature;
    uint16_t voltages[10];
    int16_t current_battery;
    int32_t current_consumed;
    int32_t energy_consumed;
    int8_t battery_remaining;
    int32_t time_remaining;
    uint8_t charge_state;
    uint16_t voltages_ext[4];
    uint8_t mode;
    uint32_t fault_bitmask;
};

// Comprehensive GPS data structure (matching gps-collector-api)
struct MavlinkGPSData {
    // GPS Status
    GPSState state;
    GPSFixType fix_type;
    uint8_t satellites_visible;
    uint8_t satellites_used;
    
    // Satellite information (up to 20 satellites)
    struct SatelliteInfo {
        uint8_t prn;           // Satellite PRN number
        uint8_t used;          // 0: not used, 1: used for localization
        uint8_t elevation;     // Elevation in degrees (0-90)
        uint8_t azimuth;       // Azimuth in degrees (0-360)
        uint8_t snr;           // Signal to noise ratio in dB
    } satellites[20];
    
    // Global Position (from GPS_GLOBAL_ORIGIN)
    bool has_origin;
    int32_t origin_latitude;    // Latitude in degrees * 1E7
    int32_t origin_longitude;   // Longitude in degrees * 1E7
    int32_t origin_altitude;    // Altitude in millimeters above MSL
    uint64_t origin_time_usec;  // Timestamp in microseconds
    
    // Current Position (from GLOBAL_POSITION_INT_COV)
    bool has_position;
    uint64_t position_time_usec; // Timestamp in microseconds
    int32_t latitude;           // Latitude in degrees * 1E7
    int32_t longitude;          // Longitude in degrees * 1E7
    int32_t altitude;           // Altitude in millimeters above MSL
    int32_t relative_altitude;  // Altitude above ground in millimeters
    float velocity_x;           // Ground X Speed (Latitude) in m/s
    float velocity_y;           // Ground Y Speed (Longitude) in m/s
    float velocity_z;           // Ground Z Speed (Altitude) in m/s
    float covariance[36];       // Position and velocity covariance matrix
    uint8_t estimator_type;     // Estimator type
    
    // Timing
    std::chrono::steady_clock::time_point last_update;
    std::chrono::steady_clock::time_point last_heartbeat;
    
    // Constructor
    MavlinkGPSData() : 
        state(GPSState::UNKNOWN),
        fix_type(GPSFixType::NO_GPS),
        satellites_visible(0),
        satellites_used(0),
        has_origin(false),
        has_position(false),
        position_time_usec(0),
        latitude(0),
        longitude(0),
        altitude(0),
        relative_altitude(0),
        velocity_x(0.0f),
        velocity_y(0.0f),
        velocity_z(0.0f),
        estimator_type(0),
        origin_latitude(0),
        origin_longitude(0),
        origin_altitude(0),
        origin_time_usec(0) {
        
        // Initialize satellite data
        for (int i = 0; i < 20; ++i) {
            satellites[i] = {0, 0, 0, 0, 0};
        }
        
        // Initialize covariance matrix
        for (int i = 0; i < 36; ++i) {
            covariance[i] = 0.0f;
        }
        
        last_update = std::chrono::steady_clock::now();
        last_heartbeat = std::chrono::steady_clock::now();
    }
};

// System Status Data Structure
struct MavlinkSystemStatus {
    uint32_t onboard_control_sensors_present;   // Bitmap of present sensors/controllers
    uint32_t onboard_control_sensors_enabled;   // Bitmap of enabled sensors/controllers  
    uint32_t onboard_control_sensors_health;    // Bitmap of healthy sensors/controllers
    uint16_t load;                              // Mainloop time usage [0-1000] (d%)
    uint16_t voltage_battery;                   // Battery voltage [mV]
    int16_t current_battery;                    // Battery current [cA]
    int8_t battery_remaining;                   // Battery energy remaining [%]
    uint16_t drop_rate_comm;                    // Communication drop rate [c%]
    uint16_t errors_comm;                       // Communication errors count
    uint32_t errors_count1;                     // Autopilot-specific errors
    uint32_t errors_count2;                     // Autopilot-specific errors
    uint32_t errors_count3;                     // Autopilot-specific errors
    uint32_t errors_count4;                     // Autopilot-specific errors
    std::chrono::steady_clock::time_point last_update;
    
    MavlinkSystemStatus() :
        onboard_control_sensors_present(0),
        onboard_control_sensors_enabled(0),
        onboard_control_sensors_health(0),
        load(0),
        voltage_battery(0),
        current_battery(-1),
        battery_remaining(-1),
        drop_rate_comm(0),
        errors_comm(0),
        errors_count1(0),
        errors_count2(0),
        errors_count3(0),
        errors_count4(0) {
        last_update = std::chrono::steady_clock::now();
    }
};

class MavlinkUdpConnection {
public:
    using HeartbeatCallback = std::function<void(const MavlinkHeartbeatInfo&)>;
    using AutopilotVersionCallback = std::function<void(const MavlinkAutopilotVersionInfo&)>;
    using BatteryInfoCallback = std::function<void(const MavlinkBatteryInfo&)>;
    using BatteryStatusCallback = std::function<void(const MavlinkBatteryStatus&)>;
    using GPSDataCallback = std::function<void(const MavlinkGPSData&)>;
    using SystemStatusCallback = std::function<void(const MavlinkSystemStatus&)>;

    MavlinkUdpConnection(uint8_t system_id = 255, uint8_t component_id = MAV_COMP_ID_MISSIONPLANNER);
    ~MavlinkUdpConnection();

    bool connect(const std::string& address, uint16_t port);
    void disconnect();
    bool isConnected() const;

    void startReceiving();
    void stopReceiving();

    void setHeartbeatCallback(HeartbeatCallback callback);
    void setAutopilotVersionCallback(AutopilotVersionCallback callback);
    void setBatteryInfoCallback(BatteryInfoCallback callback);
    void setBatteryStatusCallback(BatteryStatusCallback callback);
    void setGPSDataCallback(GPSDataCallback callback);
    void setSystemStatusCallback(SystemStatusCallback callback);
    
    void sendHeartbeat();
    void requestAutopilotVersion();
    void requestBatteryInfo();
    void requestBatteryStatus();
    void requestGPSData(uint8_t target_system = 1, uint8_t target_component = 1, uint16_t message_rate_hz = 4);
    void stopGPSData(uint8_t target_system = 1, uint8_t target_component = 1);
    void requestSystemStatus(uint8_t target_system = 1, uint8_t target_component = 1, uint16_t message_rate_hz = 1);
    
    uint8_t getMavlinkVersion() const;

private:
    struct Impl;
    std::unique_ptr<Impl> pImpl;
    uint8_t m_system_id;
    uint8_t m_component_id;
};

#endif // MAVLINK_UDP_CONNECTION_H
