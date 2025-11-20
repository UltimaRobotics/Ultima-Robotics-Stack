#pragma once

#include <string>
#include <vector>
#include <map>
#include <chrono>
#include <deque>

namespace FlightCollector {

/**
 * @brief Primary flight controller state structure
 */
struct VehicleData {
    std::string model;                                   // Vehicle model from custom version
    uint8_t system_id;                                   // MAVLink system ID
    uint8_t component_id;                                // MAVLink component ID  
    std::string flight_mode;                             // Current flight mode
    bool armed;                                          // Armed status
    float battery_voltage;                               // Main battery voltage
    std::chrono::system_clock::time_point last_heartbeat;
    std::string firmware;                                // Firmware type + version
    std::chrono::system_clock::time_point last_activity;
    uint32_t messages_received;                          // Message statistics
    std::chrono::system_clock::time_point start_time;
    
    // Component information
    std::string vendor_name;                             // Component vendor
    std::string component_model_name;                    // Component model
    std::string software_version;                        // Software version
    std::string hardware_version;                        // Hardware version
    std::string serial_number;                           // Serial number
};

/**
 * @brief Battery status structure
 */
struct BatteryStatus {
    uint8_t id;                                          // Battery ID
    uint8_t battery_function;                            // Function type
    uint8_t type;                                        // Battery chemistry
    int16_t temperature;                                 // Temperature (cdegC)
    std::vector<uint16_t> voltages;                      // Cell voltages 1-10 (mV)
    std::vector<uint16_t> voltages_ext;                  // Cell voltages 11-14 (v2 only)
    int16_t current_battery;                             // Current (cA)
    int32_t current_consumed;                            // Consumed current (mAh)
    int32_t energy_consumed;                             // Consumed energy (hJ)
    int8_t battery_remaining;                            // Remaining percentage
    uint8_t charge_state;                                // Charge state (v2)
    uint8_t mode;                                        // Battery mode (v2)
    uint32_t fault_bitmask;                              // Fault indicators (v2)
    int32_t time_remaining;                              // Time remaining (s, v2)
};

/**
 * @brief Battery information structure
 */
struct BatteryInfo {
    uint8_t id;
    uint8_t battery_function;
    uint8_t type;
    uint8_t state_of_health;                             // Health percentage
    uint8_t cells_in_series;                             // Cell count
    uint16_t cycle_count;                                // Charge cycles
    uint16_t weight;                                     // Weight (mg)
    float discharge_minimum_voltage;                     // Voltage thresholds
    float charging_minimum_voltage;
    float resting_minimum_voltage;
    float charging_maximum_voltage;
    float charging_maximum_current;                      // Current limits
    float nominal_voltage;
    float discharge_maximum_current;
    float discharge_maximum_burst_current;
    float design_capacity;                               // Capacity specifications
    float full_charge_capacity;
    std::string manufacture_date;
    std::string serial_number;
    std::string name;                                    // Device name
};

/**
 * @brief Power system monitoring structure
 */
struct PowerStatus {
    uint16_t Vcc;                                        // 5V rail voltage (mV)
    uint16_t Vservo;                                     // Servo rail voltage (mV)  
    uint16_t flags;                                      // Power supply status flags
};

/**
 * @brief Parameter information structure
 */
struct ParameterInfo {
    std::string name;                                    // Parameter name (16 chars max)
    float value;                                         // Parameter value
    uint8_t type;                                        // MAVLink parameter type
    std::chrono::system_clock::time_point timestamp;
};

/**
 * @brief Sensor status structure
 */
struct SensorStatus {
    std::string gyro;                                    // "OK", "Disabled", "Unhealthy", "Not Present"
    std::string accelerometer;
    std::string compass_0;
    std::string compass_1;
    uint32_t sensors_present;                            // Raw sensor bitmap
    uint32_t sensors_enabled;
    uint32_t sensors_health;
};

/**
 * @brief Diagnostic data structure
 */
struct DiagnosticData {
    // Airframe information
    std::string airframe_type;
    std::string vehicle;
    std::string firmware_version;
    std::string custom_fw_ver;
    
    // Sensor status (derived from SYS_STATUS)
    SensorStatus sensors;
    
    // Radio channel mapping
    int roll_channel = 0;
    int pitch_channel = 0;
    int yaw_channel = 0;
    int throttle_channel = 0;
    std::string aux1;
    std::string aux2;
    
    // Flight mode configuration
    std::string mode_switch;
    std::string flight_mode_1;
    std::string flight_mode_2;
    std::string flight_mode_3;
    std::string flight_mode_4;
    std::string flight_mode_5;
    std::string flight_mode_6;
    
    // Power system
    std::map<uint8_t, BatteryInfo> battery_info_map;     // Component-aware
    std::map<uint8_t, BatteryStatus> battery_status_map;
    PowerStatus power_status;
    float battery_full_voltage = 0.0f;
    float battery_empty_voltage = 0.0f;
    int number_of_cells = 0;
    
    // Safety configuration
    std::string low_battery_failsafe;
    std::string rc_loss_failsafe;
    float rc_loss_timeout = 0.0f;
    std::string data_link_loss_failsafe;
    float rtl_climb_to = 0.0f;
    std::string rtl_then;
};

/**
 * @brief Message rate tracking structure
 */
struct MessageRateInfo {
    uint32_t count = 0;                                  // Message count
    std::deque<std::chrono::steady_clock::time_point> timestamps;
    double current_rate_hz = 0.0;                        // Current frequency
    double expected_rate_hz = 0.0;                       // Target frequency
    std::chrono::system_clock::time_point last_update;
    
    // Rate calculation window (default 10 seconds)
    std::chrono::seconds rate_window{10};
    
    void update_rate() {
        auto now = std::chrono::steady_clock::now();
        timestamps.push_back(now);
        
        // Remove old timestamps outside the rate window
        auto cutoff = now - rate_window;
        while (!timestamps.empty() && timestamps.front() < cutoff) {
            timestamps.pop_front();
        }
        
        current_rate_hz = static_cast<double>(timestamps.size()) / rate_window.count();
        last_update = std::chrono::system_clock::now();
    }
};

/**
 * @brief Complete flight data collection
 */
struct FlightDataCollection {
    VehicleData vehicle;
    DiagnosticData diagnostics;
    std::map<std::string, ParameterInfo> parameters;
    std::map<uint16_t, MessageRateInfo> message_rates;
    std::chrono::system_clock::time_point last_update;
    
    // Telemetry data structures
    struct PositionData {
        double latitude_deg{0.0};
        double longitude_deg{0.0};
        float absolute_altitude_m{0.0f};
        float relative_altitude_m{0.0f};
    } position;
    
    struct VelocityData {
        float north_m_s{0.0f};
        float east_m_s{0.0f};
        float down_m_s{0.0f};
    } velocity;
    
    struct AttitudeData {
        float w{1.0f};
        float x{0.0f};
        float y{0.0f};
        float z{0.0f};
    } attitude;
    
    struct GpsInfoData {
        uint8_t num_satellites{0};
        int fix_type{0};
    } gps_info;
    
    struct HealthData {
        bool is_gyro_ok{false};
        bool is_accel_ok{false};
        bool is_mag_ok{false};
        bool is_gps_ok{false};
    } health;
    
    struct StatusTextData {
        std::string text;
        int type{0};
    } status_text;
    
    struct RcStatusData {
        bool available_once{false};
        uint8_t signal_strength_percent{0};
    } rc_status;
    
    struct AltitudeData {
        float altitude_monotonic_m{0.0f};
        float altitude_local_m{0.0f};
        float altitude_relative_m{0.0f};
    } altitude;
    
    struct HeadingData {
        float heading_deg{0.0f};
    } heading;
    
    void update_timestamp() {
        last_update = std::chrono::system_clock::now();
    }
};

/**
 * @brief Connection configuration structure
 */
struct ConnectionConfig {
    std::string type;                                    // "udp", "tcp", "serial"
    std::string address;                                 // IP address or device path
    int port = 0;                                        // Port for UDP/TCP
    int baudrate = 57600;                                // Baudrate for serial
    std::string system_id = "1";                         // Target system ID
    std::string component_id = "1";                      // Target component ID
    int timeout_s = 10;                                  // Connection timeout
};

} // namespace FlightCollector
