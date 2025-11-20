# MAVLink Collected Data Structures

## Table of Contents

1. [Overview](#overview)
2. [Core Data Structures](#core-data-structures)
3. [Message Types](#message-types)
4. [System Information](#system-information)
5. [GPS and Position Data](#gps-and-position-data)
6. [Attitude and Orientation](#attitude-and-orientation)
7. [Sensor Data](#sensor-data)
8. [Battery and Power](#battery-and-power)
9. [Radio and Communication](#radio-and-communication)
10. [Mission and Waypoint Data](#mission-and-waypoint-data)
11. [Parameter Management](#parameter-management)
12. [Command and Control](#command-and-control)
13. [System Status and Health](#system-status-and-health)
14. [Logging and Storage](#logging-and-storage)
15. [Extended Data Types](#extended-data-types)
16. [Data Collection Patterns](#data-collection-patterns)
17. [Storage Formats](#storage-formats)
18. [API Integration](#api-integration)
19. [Examples and Use Cases](#examples-and-use-cases)

## Overview

The MAVLink data collection system captures and processes telemetry data from MAVLink-compatible vehicles. This document describes the comprehensive data structures used to store, manage, and analyze this collected data.

### Data Collection Philosophy

The data structures are designed to:
- Preserve the original MAVLink message format
- Enable efficient querying and analysis
- Support real-time processing
- Maintain data integrity and consistency
- Provide extensibility for custom data types

### Architecture Principles

1. **Message Preservation**: Original MAVLink messages are stored in their raw form
2. **Semantic Enrichment**: Additional metadata provides context and meaning
3. **Temporal Organization**: Data is organized by timestamps for time-series analysis
4. **Hierarchical Structure**: Vehicle, component, and message hierarchies are maintained
5. **Cross-Reference Capability**: Links between related data points are preserved

## Core Data Structures

### VehicleInfo Structure

```cpp
struct VehicleInfo {
    // System identification
    uint8_t system_id;           // MAVLink system ID (1-255)
    uint8_t component_id;        // MAVLink component ID
    std::string vehicle_type;    // Quadcopter, Fixed-wing, Rover, etc.
    std::string autopilot_type;  // ArduPilot, PX4, etc.
    
    // System capabilities
    std::vector<std::string> supported_streams;
    std::map<std::string, bool> capabilities;
    
    // Connection information
    std::string connection_type; // Serial, UDP, TCP
    std::string connection_string;
    uint64_t connection_timestamp;
    
    // Metadata
    std::string vehicle_name;
    std::string description;
    std::map<std::string, std::string> custom_fields;
    
    // Data collection status
    bool is_collecting;
    uint64_t collection_start_time;
    uint64_t last_message_time;
    uint32_t total_messages_received;
};
```

### MessageContainer Structure

```cpp
struct MessageContainer {
    // Message identification
    uint32_t message_id;         // MAVLink message ID
    std::string message_name;    // Human-readable name
    uint8_t message_type;        // MAVLink message type enum
    
    // Timestamp information
    uint64_t timestamp_us;       // Microsecond timestamp
    uint64_t sequence_number;    // Monotonically increasing sequence
    
    // Raw message data
    std::vector<uint8_t> raw_data; // Complete MAVLink message bytes
    size_t message_length;       // Length of the message
    
    // Parsed data
    std::map<std::string, MessageField> fields;
    
    // System information
    uint8_t system_id;
    uint8_t component_id;
    
    // Quality indicators
    uint8_t rssi;               // Signal strength if available
    bool message_complete;      // Complete message received
    uint8_t checksum_status;    // Checksum validation result
    
    // Processing metadata
    uint64_t processing_time_us; // Time to process this message
    std::string parser_version;  // MAVLink parser version used
};
```

### MessageField Structure

```cpp
struct MessageField {
    std::string field_name;      // Field identifier
    std::string field_type;      // Data type (int32_t, float, etc.)
    
    // Value storage (variant type)
    std::variant<
        int8_t, uint8_t,
        int16_t, uint16_t,
        int32_t, uint32_t,
        int64_t, uint64_t,
        float, double,
        std::string, std::vector<uint8_t>
    > value;
    
    // Field metadata
    std::string description;
    std::string units;          // Physical units (meters, degrees, etc.)
    bool is_array;              // True for array fields
    size_t array_size;          // Number of elements in array
    
    // Value validation
    bool has_min_value;
    bool has_max_value;
    std::variant<int64_t, double> min_value;
    std::variant<int64_t, double> max_value;
    
    // Encoding information
    uint8_t mavlink_type;       // Original MAVLink type
    size_t byte_offset;         // Offset in raw message
    size_t byte_size;           // Size in bytes
};
```

## Message Types

### HEARTBEAT Message Structure

```cpp
struct HeartbeatData {
    // Core heartbeat fields
    uint32_t custom_mode;        // Custom mode for specific autopilot
    uint8_t mavtype;            // MAVLink vehicle type
    uint8_t autopilot;          // Autopilot type
    uint8_t base_mode;          // Base mode (armed, stabilized, etc.)
    uint8_t system_status;      // System status (active, standby, etc.)
    uint8_t mavlink_version;    // MAVLink protocol version
    
    // Extended data
    std::string mode_name;      // Human-readable mode name
    bool is_armed;              // Armed status derived from base_mode
    bool is_stabilized;         // Stabilization enabled
    bool is_guided;             // Guided mode active
    
    // Health indicators
    bool has_gps_fix;           // GPS fix status
    bool has_radio_link;        // Radio link status
    bool has_battery;           // Battery monitoring active
    
    // Timing information
    uint64_t heartbeat_interval_us; // Time since last heartbeat
    uint32_t missed_heartbeats;     // Count of missed heartbeats
    
    // Metadata
    uint64_t reception_time_us;
    uint8_t signal_quality;
};
```

### SYSTEM_TIME Message Structure

```cpp
struct SystemTimeData {
    // Time fields
    uint64_t time_unix_usec;    // Unix timestamp in microseconds
    uint32_t time_boot_ms;      // Time since system boot in milliseconds
    
    // Derived fields
    std::string iso_timestamp;  // ISO 8601 formatted timestamp
    uint64_t uptime_seconds;    // System uptime in seconds
    uint64_t uptime_days;       // System uptime in days
    
    // Time synchronization
    bool is_synchronized;       // Time sync with external source
    std::string sync_source;    // GPS, NTP, RTC, etc.
    uint64_t sync_accuracy_us;  // Estimated sync accuracy
    
    // Time drift tracking
    double drift_rate_ppm;      // Clock drift rate in parts per million
    uint64_t last_sync_time;    // Last synchronization timestamp
    
    // Metadata
    uint64_t reception_time_us;
    uint8_t time_quality;
};
```

## GPS and Position Data

### GPS_RAW_INT Message Structure

```cpp
struct GpsRawIntData {
    // Position information
    int32_t lat;                // Latitude in degrees * 1e7
    int32_t lon;                // Longitude in degrees * 1e7
    int32_t alt;                // Altitude in millimeters above sea level
    int32_t relative_alt;       // Altitude above ground in millimeters
    
    // GPS fix and status
    uint8_t fix_type;           // GPS fix type (0=No GPS, 3=3D Fix, etc.)
    uint8_t satellites_visible; // Number of visible satellites
    int32_t eph;                // GPS horizontal position accuracy in mm
    int32_t epv;                // GPS vertical position accuracy in mm
    
    // Velocity information
    uint16_t vel;               // GPS ground speed in cm/s
    int16_t cog;                // Course over ground in degrees * 100
    int32_t alt_ellipsoid;      // Altitude above ellipsoid in mm
    uint32_t h_acc;             // Horizontal accuracy in mm
    uint32_t v_acc;             // Vertical accuracy in mm
    uint32_t vel_acc;           // Velocity accuracy in mm/s
    uint32_t hdg_acc;           // Heading accuracy in deg * 100
    
    // Time information
    uint64_t time_usec;         // GPS timestamp in microseconds
    uint32_t time_week;         // GPS week number
    uint32_t time_week_ms;      // GPS time in milliseconds for current week
    
    // Derived fields
    double latitude_deg;        // Latitude in decimal degrees
    double longitude_deg;       // Longitude in decimal degrees
    double altitude_m;          // Altitude in meters
    double ground_speed_mps;    // Ground speed in m/s
    double course_over_ground_deg; // COG in degrees
    
    // Quality indicators
    double hdop;                // Horizontal dilution of precision
    double vdop;                // Vertical dilution of precision
    double pdop;                // Position dilution of precision
    
    // Metadata
    std::string fix_type_name;  // Human-readable fix type
    bool is_valid_fix;          // Valid position fix
    uint64_t reception_time_us;
};
```

### GLOBAL_POSITION_INT Message Structure

```cpp
struct GlobalPositionIntData {
    // Global position
    int32_t lat;                // Latitude in degrees * 1e7
    int32_t lon;                // Longitude in degrees * 1e7
    int32_t alt;                // Altitude in millimeters above sea level
    int32_t relative_alt;       // Altitude above home in millimeters
    
    // Velocity information
    int16_t vx;                 // Ground X speed in cm/s (positive = North)
    int16_t vy;                 // Ground Y speed in cm/s (positive = East)
    int16_t vz;                 // Ground Z speed in cm/s (positive = Down)
    uint16_t hdg;               // Vehicle heading in degrees * 100 (0-35999)
    
    // Derived fields
    double latitude_deg;        // Latitude in decimal degrees
    double longitude_deg;       // Longitude in decimal degrees
    double altitude_m;          // Altitude in meters above sea level
    double relative_altitude_m; // Altitude in meters above home
    double velocity_north_mps;  // North velocity in m/s
    double velocity_east_mps;   // East velocity in m/s
    double velocity_down_mps;   // Down velocity in m/s
    double heading_deg;         // Heading in degrees
    
    // Position quality
    double horizontal_accuracy_m; // Estimated horizontal accuracy
    double vertical_accuracy_m;   // Estimated vertical accuracy
    double speed_accuracy_mps;    // Estimated speed accuracy
    
    // Motion analysis
    double ground_speed_mps;    // Total ground speed magnitude
    double climb_rate_mps;      // Vertical climb rate
    double track_angle_deg;     // Track angle (direction of movement)
    
    // Home position reference
    bool has_home_position;     // Home position is set
    double distance_from_home_m; // Distance from home position
    double bearing_from_home_deg; // Bearing from home position
    
    // Metadata
    uint64_t time_boot_ms;      // Time since boot
    uint64_t reception_time_us;
    uint8_t position_source;    // GPS, visual, inertial navigation, etc.
};
```

### LOCAL_POSITION_NED Message Structure

```cpp
struct LocalPositionNedData {
    // Position in local NED frame
    float x;                    // X position in meters (North)
    float y;                    // Y position in meters (East)
    float z;                    // Z position in meters (Down)
    
    // Velocity in local NED frame
    float vx;                   // X velocity in m/s (North)
    float vy;                   // Y velocity in m/s (East)
    float vz;                   // Z velocity in m/s (Down)
    
    // Derived quantities
    double horizontal_position_m; // Horizontal distance from origin
    double total_position_m;      // Total 3D distance from origin
    double horizontal_speed_mps;  // Horizontal speed magnitude
    double total_speed_mps;       // Total 3D speed magnitude
    double vertical_speed_mps;    // Vertical speed (positive = climbing)
    
    // Position quality
    float position_accuracy_m;   // Estimated position accuracy
    float velocity_accuracy_mps;  // Estimated velocity accuracy
    
    // Coordinate system info
    std::string coordinate_frame; // NED, ENU, etc.
    bool is_global_reference;     // Referenced to global coordinates
    
    // Motion analysis
    double track_angle_deg;       // Direction of horizontal motion
    double climb_rate_mps;        // Vertical climb rate
    double acceleration_mps2;     // Estimated acceleration
    
    // Metadata
    uint64_t time_boot_ms;        // Time since boot
    uint64_t reception_time_us;
    uint8_t estimation_source;    // EKF, visual odometry, etc.
};
```

## Attitude and Orientation

### ATTITUDE Message Structure

```cpp
struct AttitudeData {
    // Attitude angles (Euler)
    float roll;                 // Roll angle in radians
    float pitch;                // Pitch angle in radians
    float yaw;                  // Yaw angle in radians
    
    // Angular rates
    float rollspeed;            // Roll angular speed in rad/s
    float pitchspeed;           // Pitch angular speed in rad/s
    float yawspeed;             // Yaw angular speed in rad/s
    
    // Derived quantities
    double roll_deg;            // Roll angle in degrees
    double pitch_deg;           // Pitch angle in degrees
    double yaw_deg;             // Yaw angle in degrees
    double roll_rate_dps;       // Roll rate in degrees per second
    double pitch_rate_dps;      // Pitch rate in degrees per second
    double yaw_rate_dps;        // Yaw rate in degrees per second
    
    // Quaternion representation
    double q_w;                 // Quaternion W component
    double q_x;                 // Quaternion X component
    double q_y;                 // Quaternion Y component
    double q_z;                 // Quaternion Z component
    
    // Attitude quality
    float attitude_accuracy_deg; // Estimated attitude accuracy
    float rate_accuracy_dps;    // Estimated angular rate accuracy
    
    // Motion analysis
    double total_rotation_rate_dps; // Total angular velocity magnitude
    double attitude_change_deg;     // Change in attitude since last message
    bool is_level;                   // Vehicle is approximately level
    bool is_turning;                 // Vehicle is actively turning
    
    // Orientation state
    std::string flight_phase;       // Takeoff, cruise, landing, etc.
    bool is_inverted;               // Vehicle is inverted
    double bank_angle_deg;          // Total bank angle magnitude
    
    // Metadata
    uint64_t time_boot_ms;          // Time since boot
    uint64_t reception_time_us;
    uint8_t estimation_source;      // IMU, EKF, visual, etc.
};
```

### ATTITUDE_QUATERNION Message Structure

```cpp
struct AttitudeQuaternionData {
    // Quaternion components
    float q1;                   // Quaternion component 1 (w)
    float q2;                   // Quaternion component 2 (x)
    float q3;                   // Quaternion component 3 (y)
    float q4;                   // Quaternion component 4 (z)
    
    // Angular rates
    float rollspeed;            // Roll angular speed in rad/s
    float pitchspeed;           // Pitch angular speed in rad/s
    float yawspeed;             // Yaw angular speed in rad/s
    
    // Quaternion validation
    double quaternion_norm;     // Should be 1.0 for valid quaternion
    bool is_valid_quaternion;   // Quaternion passes validation
    
    // Derived Euler angles
    double roll_deg;            // Roll angle in degrees
    double pitch_deg;           // Pitch angle in degrees
    double yaw_deg;             // Yaw angle in degrees
    double roll_rad;            // Roll angle in radians
    double pitch_rad;           // Pitch angle in radians
    double yaw_rad;             // Yaw angle in radians
    
    // Angular velocity magnitude
    double total_angular_rate_dps; // Total angular velocity in deg/s
    
    // Attitude change tracking
    double quaternion_distance; // Change from previous quaternion
    double angular_displacement_deg; // Angular displacement since last
    
    // Orientation analysis
    double tilt_angle_deg;      // Tilt from vertical
    double heading_change_rate_dps; // Rate of heading change
    bool is_rotating;           // Significant rotation detected
    
    // Quaternion components for analysis
    double rotation_axis_x;     // Rotation axis X component
    double rotation_axis_y;     // Rotation axis Y component
    double rotation_axis_z;     // Rotation axis Z component
    double rotation_angle_rad;  // Rotation angle around axis
    
    // Metadata
    uint64_t time_boot_ms;      // Time since boot
    uint64_t reception_time_us;
    uint8_t estimation_type;    // EKF1, EKF2, EKF3, etc.
};
```

## Sensor Data

### RAW_IMU Message Structure

```cpp
struct RawImuData {
    // Raw accelerometer data
    int16_t xacc;               // X acceleration in raw ADC units
    int16_t yacc;               // Y acceleration in raw ADC units
    int16_t zacc;               // Z acceleration in raw ADC units
    
    // Raw gyroscope data
    int16_t xgyro;              // X angular rate in raw ADC units
    int16_t ygyro;              // Y angular rate in raw ADC units
    int16_t zgyro;              // Z angular rate in raw ADC units
    
    // Raw magnetometer data
    int16_t xmag;               // X magnetic field in raw ADC units
    int16_t ymag;               // Y magnetic field in raw ADC units
    int16_t zmag;               // Z magnetic field in raw ADC units
    
    // Temperature data
    int16_t temperature;        // Temperature in raw ADC units
    
    // Calibrated values (if calibration data available)
    double accel_x_mps2;        // X acceleration in m/s²
    double accel_y_mps2;        // Y acceleration in m/s²
    double accel_z_mps2;        // Z acceleration in m/s²
    double gyro_x_dps;          // X angular rate in degrees/s
    double gyro_y_dps;          // Y angular rate in degrees/s
    double gyro_z_dps;          // Z angular rate in degrees/s
    double mag_x_gauss;         // X magnetic field in Gauss
    double mag_y_gauss;         // Y magnetic field in Gauss
    double mag_z_gauss;         // Z magnetic field in Gauss
    double temperature_c;       // Temperature in Celsius
    
    // Sensor analysis
    double accel_magnitude_mps2; // Total acceleration magnitude
    double gyro_magnitude_dps;   // Total angular rate magnitude
    double mag_magnitude_gauss;  // Total magnetic field magnitude
    double gravity_component;    // Gravity component in acceleration
    
    // Motion detection
    bool is_accelerating;       // Significant acceleration detected
    bool is_rotating;           // Significant rotation detected
    bool has_magnetic_disturbance; // Magnetic field disturbance
    
    // Orientation from sensors
    double roll_from_accel_deg; // Roll from accelerometer
    double pitch_from_accel_deg; // Pitch from accelerometer
    double heading_from_mag_deg; // Heading from magnetometer
    
    // Sensor quality
    uint8_t accel_quality;      // Accelerometer data quality
    uint8_t gyro_quality;       // Gyroscope data quality
    uint8_t mag_quality;        // Magnetometer data quality
    
    // Calibration status
    bool accel_calibrated;      // Accelerometer is calibrated
    bool gyro_calibrated;       // Gyroscope is calibrated
    bool mag_calibrated;        // Magnetometer is calibrated
    
    // Metadata
    uint64_t time_usec;         // Timestamp in microseconds
    uint64_t reception_time_us;
    uint8_t sensor_id;          // IMU sensor identifier
    std::string sensor_type;    // IMU sensor model/type
};
```

### SCALED_IMU Message Structure

```cpp
struct ScaledImuData {
    // Scaled accelerometer data
    int16_t xacc;               // X acceleration in mG (milli-g)
    int16_t yacc;               // Y acceleration in mG
    int16_t zacc;               // Z acceleration in mG
    
    // Scaled gyroscope data
    int16_t xgyro;              // X angular rate in milliradians/s
    int16_t ygyro;              // Y angular rate in milliradians/s
    int16_t zgyro;              // Z angular rate in milliradians/s
    
    // Scaled magnetometer data
    int16_t xmag;               // X magnetic field in millitesla
    int16_t ymag;               // Y magnetic field in millitesla
    int16_t zmag;               // Z magnetic field in millitesla
    
    // Temperature data
    int16_t temperature;        // Temperature in degrees Celsius * 100
    
    // Converted to standard units
    double accel_x_mps2;        // X acceleration in m/s²
    double accel_y_mps2;        // Y acceleration in m/s²
    double accel_z_mps2;        // Z acceleration in m/s²
    double gyro_x_dps;          // X angular rate in degrees/s
    double gyro_y_dps;          // Y angular rate in degrees/s
    double gyro_z_dps;          // Z angular rate in degrees/s
    double mag_x_tesla;         // X magnetic field in Tesla
    double mag_y_tesla;         // Y magnetic field in Tesla
    double mag_z_tesla;         // Z magnetic field in Tesla
    double temperature_c;       // Temperature in Celsius
    
    // Sensor analysis
    double accel_magnitude_mps2; // Total acceleration magnitude
    double gravity_magnitude_mps2; // Gravity magnitude component
    double linear_accel_magnitude_mps2; // Linear acceleration magnitude
    double gyro_magnitude_dps;   // Total angular rate magnitude
    double mag_magnitude_tesla;  // Total magnetic field magnitude
    
    // Motion state analysis
    double motion_intensity;    // Overall motion intensity metric
    bool is_static;             // Vehicle is approximately static
    bool is_dynamic;            // Vehicle is in dynamic motion
    double vibration_level;     // Vibration level indicator
    
    // Orientation estimation
    double roll_deg;            // Estimated roll angle
    double pitch_deg;           // Estimated pitch angle
    double tilt_angle_deg;      // Tilt from vertical
    
    // Sensor health
    uint8_t accel_health;       // Accelerometer health indicator
    uint8_t gyro_health;        // Gyroscope health indicator
    uint8_t mag_health;         // Magnetometer health indicator
    bool sensor_fusion_active;  // Sensor fusion is operational
    
    // Calibration metrics
    double accel_bias_x;        // Accelerometer X bias
    double accel_bias_y;        // Accelerometer Y bias
    double accel_bias_z;        // Accelerometer Z bias
    double gyro_bias_x;         // Gyroscope X bias
    double gyro_bias_y;         // Gyroscope Y bias
    double gyro_bias_z;         // Gyroscope Z bias
    
    // Metadata
    uint64_t time_boot_ms;      // Time since boot
    uint64_t reception_time_us;
    uint8_t sensor_id;          // IMU sensor identifier
};
```

### SCALED_PRESSURE Message Structure

```cpp
struct ScaledPressureData {
    // Pressure measurements
    float press_abs;            // Absolute pressure in hPa
    float press_diff;           // Differential pressure in hPa
    int16_t temperature;        // Temperature measurement in degrees Celsius
    
    // Derived altitude
    double altitude_m;          // Altitude from absolute pressure
    double indicated_airspeed_mps; // Indicated airspeed from differential pressure
    double true_airspeed_mps;   // True airspeed (corrected for altitude)
    
    // Pressure analysis
    double pressure_change_rate; // Rate of pressure change
    double pressure_altitude_rate; // Rate of altitude change from pressure
    bool pressure_stable;       // Pressure is stable
    double pressure_noise_level; // Pressure measurement noise
    
    // Airspeed analysis
    double airspeed_accuracy_mps; // Estimated airspeed accuracy
    bool is_flying;             // Vehicle is flying (based on airspeed)
    double stall_margin_mps;    // Margin to stall speed
    
    // Atmospheric conditions
    double sea_level_pressure_hpa; // Estimated sea level pressure
    double density_altitude_m;   // Density altitude
    double pressure_altitude_error_m; // Pressure altitude error
    
    // Sensor quality
    uint8_t pressure_abs_quality;   // Absolute pressure quality
    uint8_t pressure_diff_quality;  // Differential pressure quality
    uint8_t temperature_quality;    // Temperature quality
    
    // Calibration status
    bool pressure_calibrated;   // Pressure sensors calibrated
    bool temperature_calibrated; // Temperature sensor calibrated
    double calibration_factor;  // Applied calibration factor
    
    // Environmental conditions
    double humidity_percent;    // Relative humidity (if available)
    double wind_speed_mps;      // Estimated wind speed
    double wind_direction_deg;  // Estimated wind direction
    
    // Performance metrics
    double climb_rate_mps;      // Climb rate from pressure
    double glide_ratio;         // Glide ratio calculation
    double lift_coefficient;    // Estimated lift coefficient
    
    // Metadata
    uint64_t time_boot_ms;      // Time since boot
    uint64_t reception_time_us;
    uint8_t sensor_id;          // Pressure sensor identifier
    std::string sensor_type;    // Sensor model/type
};
```

## Battery and Power

### BATTERY_STATUS Message Structure

```cpp
struct BatteryStatusData {
    // Battery identification
    uint8_t id;                 // Battery ID
    uint8_t battery_function;   // Function of the battery
    uint8_t type;               // Type of the battery
    
    // Voltage measurements
    int16_t voltage_battery;    // Battery voltage in millivolts
    uint16_t voltages[10];      // Individual cell voltages in millivolts
    uint8_t voltages_len;       // Number of cell voltages
    
    // Current measurements
    int16_t current_battery;    // Battery current in milliamps
    int8_t current_consumed;    // Consumed current in 10*milliampere-hours
    
    // Energy measurements
    int32_t energy_consumed;    // Consumed energy in 10*millijoules
    int16_t battery_remaining;  // Remaining battery energy in percent
    
    // Temperature measurements
    int16_t temperature;        // Battery temperature in centi-degrees Celsius
    
    // Derived values
    double voltage_v;           // Battery voltage in volts
    double current_a;           // Battery current in amps
    double power_w;             // Power consumption in watts
    double energy_wh;           // Consumed energy in watt-hours
    double temperature_c;       // Battery temperature in Celsius
    
    // Cell analysis
    double cell_voltages_v[10]; // Individual cell voltages in volts
    double cell_voltage_avg_v;  // Average cell voltage
    double cell_voltage_min_v;  // Minimum cell voltage
    double cell_voltage_max_v;  // Maximum cell voltage
    double cell_voltage_balance; // Cell voltage balance metric
    uint8_t weakest_cell_id;    // Index of weakest cell
    
    // Battery health indicators
    double internal_resistance_ohm; // Estimated internal resistance
    double capacity_ah;         // Estimated battery capacity in Ah
    double capacity_percent;    // Capacity as percentage of design
    double health_score;        // Overall battery health score
    bool battery_healthy;       // Battery is in good health
    
    // Performance metrics
    double discharge_rate_c;    // Discharge rate in C units
    double time_remaining_minutes; // Estimated time remaining
    double energy_efficiency;   // Energy efficiency metric
    
    // State analysis
    bool is_charging;           // Battery is charging
    bool is_discharging;        // Battery is discharging
    bool is_balancing;          // Cell balancing active
    bool has_fault;             // Battery fault detected
    
    // Predictive analysis
    double voltage_trend_vps;   // Voltage trend in volts per second
    double temperature_trend_cps; // Temperature trend in Celsius per second
    double predicted_time_remaining_minutes; // Predicted time remaining
    
    // Safety metrics
    bool over_voltage;          // Over voltage condition
    bool under_voltage;         // Under voltage condition
    bool over_current;          // Over current condition
    bool over_temperature;      // Over temperature condition
    
    // Metadata
    uint64_t time_boot_ms;      // Time since boot
    uint64_t reception_time_us;
    std::string battery_model;  // Battery model information
    std::string battery_chemistry; // LiPo, LiFe, NiMH, etc.
};
```

### SYSTEM_POWER Message Structure

```cpp
struct SystemPowerData {
    // Power supply status
    uint16_t Vcc;               // 5V rail voltage in millivolts
    uint16_t Vservo;            // Servo rail voltage in millivolts
    uint16_t flags;             // Power supply status flags
    
    // Derived voltages
    double vcc_v;               // 5V rail voltage in volts
    double vservo_v;            // Servo rail voltage in volts
    
    // Power rail analysis
    bool vcc_healthy;           // 5V rail is within acceptable range
    bool vservo_healthy;        // Servo rail is within acceptable range
    double vcc_load_percent;    // 5V rail load percentage
    double vservo_load_percent; // Servo rail load percentage
    
    // Power consumption
    double vcc_current_a;       // Estimated 5V rail current
    double vservo_current_a;    // Estimated servo rail current
    double total_power_w;       // Total system power consumption
    
    // Power source information
    bool usb_power_connected;   // USB power is connected
    bool battery_power_connected; // Battery power is connected
    bool external_power_connected; // External power supply connected
    std::string primary_power_source; // Primary power source
    
    // Power management status
    bool power_saving_mode;     // Power saving mode active
    bool low_power_warning;     // Low power warning active
    bool critical_power_warning; // Critical power warning
    
    // System health indicators
    uint8_t power_system_health; // Overall power system health
    bool power_stable;          // Power supply is stable
    double power_quality_score; // Power quality metric
    
    // Environmental factors
    double ambient_temperature_c; // Ambient temperature
    double power_efficiency;    // Power conversion efficiency
    
    // Failover status
    bool power_failover_active; // Power failover is active
    bool redundant_power_active; // Redundant power systems active
    uint8_t active_power_rails; // Number of active power rails
    
    // Predictive metrics
    double power_trend_wps;     // Power consumption trend
    double estimated_runtime_minutes; // Estimated runtime at current load
    
    // Safety and protection
    bool over_current_protection; // Over-current protection active
    bool over_voltage_protection; // Over-voltage protection active
    bool thermal_protection;    // Thermal protection active
    
    // Metadata
    uint64_t time_boot_ms;      // Time since boot
    uint64_t reception_time_us;
    std::string power_controller; // Power controller type
};
```

## Radio and Communication

### RADIO_STATUS Message Structure

```cpp
struct RadioStatusData {
    // Radio link quality
    uint16_t rxerrors;          // Receive errors
    uint16_t fixed;             // Count of error corrected packets
    uint8_t rssi;               // Local signal strength
    uint8_t remrssi;            // Remote signal strength
    uint8_t txbuf;              // How full the tx buffer is as a percentage
    uint8_t noise;              // Background noise level
    uint8_t remnoise;           // Remote background noise level
    
    // Link quality metrics
    double link_quality_percent; // Overall link quality percentage
    double packet_loss_percent;  // Packet loss percentage
    double link_stability_score; // Link stability metric
    
    // Signal analysis
    double signal_to_noise_ratio_db; // SNR in dB
    double signal_strength_dbm;  // Signal strength in dBm
    double noise_floor_dbm;      // Noise floor in dBm
    
    // Performance metrics
    double throughput_bps;       // Estimated throughput
    double latency_ms;           // Estimated link latency
    uint32_t packets_received;   // Total packets received
    uint32_t packets_sent;       // Total packets sent
    
    // Link status
    bool link_established;       // Radio link is established
    bool link_stable;           // Link is stable
    bool high_quality;          // High quality link
    bool degraded_link;         // Link quality is degraded
    
    // Communication analysis
    double effective_data_rate_bps; // Effective data rate
    double retransmission_rate;     // Retransmission rate
    uint16_t consecutive_errors;    // Consecutive error count
    
    // Radio configuration
    uint8_t channel;            // Radio channel
    uint32_t frequency_hz;      // Radio frequency
    uint16_t bandwidth_hz;      // Channel bandwidth
    uint8_t modulation_type;    // Modulation type
    
    // Antenna information
    bool diversity_enabled;     // Antenna diversity enabled
    uint8_t active_antenna;     // Active antenna ID
    double antenna_gain_dbi;    // Antenna gain in dBi
    
    // Environmental factors
    double path_loss_db;        // Estimated path loss
    double multipath_interference; // Multipath interference level
    bool interference_detected; // Interference detected
    
    // Predictive metrics
    double link_trend;          // Link quality trend
    double predicted_link_quality; // Predicted link quality
    uint32_t time_to_link_failure_s; // Estimated time to link failure
    
    // Safety indicators
    bool critical_link_quality; // Critical link quality
    bool link_failure_imminent; // Link failure imminent
    uint8_t link_health_score;  // Overall link health score
    
    // Metadata
    uint64_t time_boot_ms;      // Time since boot
    uint64_t reception_time_us;
    std::string radio_model;    // Radio model information
    std::string protocol_version; // Radio protocol version
};
```

### LINK_NODE_STATUS Message Structure

```cpp
struct LinkNodeStatusData {
    // Node identification
    uint8_t tx_seq;             // Transmit sequence
    uint8_t rx_seq;             // Receive sequence
    uint8_t msg_type;           // Message type
    uint8_t addr;               // Node address
    
    // Node status
    bool is_online;             // Node is online
    bool is_active;             // Node is actively communicating
    uint32_t uptime_s;          // Node uptime in seconds
    
    // Communication statistics
    uint32_t messages_sent;     // Total messages sent
    uint32_t messages_received; // Total messages received
    uint32_t bytes_sent;        // Total bytes sent
    uint32_t bytes_received;    // Total bytes received
    
    // Performance metrics
    double message_rate_hz;     // Message rate in Hz
    double data_rate_bps;       // Data rate in bits per second
    double latency_ms;          // Average latency
    double jitter_ms;           // Latency jitter
    
    // Link quality
    double link_quality_percent; // Link quality percentage
    double packet_loss_percent;  // Packet loss rate
    double error_rate;          // Error rate
    
    // Node capabilities
    bool supports_routing;      // Node supports message routing
    bool supports_relay;        // Node can relay messages
    bool is_gateway;            // Node is a gateway
    bool is_end_node;           // Node is an end device
    
    // Network topology
    uint8_t hop_count;          // Number of hops to this node
    double signal_strength_dbm; // Signal strength
    double distance_m;          // Estimated distance
    
    // Resource utilization
    double cpu_usage_percent;   // CPU usage percentage
    double memory_usage_percent; // Memory usage percentage
    double buffer_usage_percent; // Buffer usage percentage
    
    // Health indicators
    uint8_t health_score;       // Node health score
    bool has_errors;            // Node has active errors
    uint16_t error_count;       // Total error count
    
    // Timing information
    uint64_t last_message_time_us; // Time of last message
    uint64_t last_heartbeat_time_us; // Time of last heartbeat
    uint32_t message_interval_ms;   // Message interval
    
    // Neighbor information
    std::vector<uint8_t> neighbor_nodes; // List of neighbor nodes
    uint8_t neighbor_count;     // Number of neighbors
    
    // Security status
    bool encryption_enabled;    // Encryption is enabled
    bool authenticated;         // Node is authenticated
    uint8_t security_level;     // Security level
    
    // Metadata
    uint64_t time_boot_ms;      // Time since boot
    uint64_t reception_time_us;
    std::string node_type;      // Node type (ground, air, relay, etc.)
    std::string firmware_version; // Node firmware version
};
```

## Mission and Waypoint Data

### MISSION_ITEM Message Structure

```cpp
struct MissionItemData {
    // Mission item identification
    uint16_t seq;               // Sequence number
    uint8_t frame;              // Coordinate frame
    uint16_t command;           // MAV_CMD command
    uint8_t current;            // Current waypoint
    uint8_t autocontinue;       // Autocontinue to next waypoint
    
    // Parameters (command-specific)
    float param1;               // Parameter 1
    float param2;               // Parameter 2
    float param3;               // Parameter 3
    float param4;               // Parameter 4
    
    // Position information
    float x;                    // X coordinate (latitude or local X)
    float y;                    // Y coordinate (longitude or local Y)
    float z;                    // Z coordinate (altitude or local Z)
    
    // Derived position
    double latitude_deg;        // Latitude in degrees (if global frame)
    double longitude_deg;       // Longitude in degrees (if global frame)
    double altitude_m;          // Altitude in meters
    
    // Command interpretation
    std::string command_name;   // Human-readable command name
    std::string frame_name;     // Human-readable frame name
    std::string waypoint_type;  // Waypoint type (normal, loiter, etc.)
    
    // Navigation data
    double distance_from_previous_m; // Distance from previous waypoint
    double bearing_from_previous_deg; // Bearing from previous waypoint
    double distance_to_next_m; // Distance to next waypoint
    double bearing_to_next_deg; // Bearing to next waypoint
    
    // Execution parameters
    double acceptance_radius_m; // Waypoint acceptance radius
    double loiter_radius_m;     // Loiter radius (if applicable)
    double loiter_direction;    // Loiter direction (clockwise/counterclockwise)
    double loiter_time_s;       // Loiter time in seconds
    
    // Speed and altitude constraints
    double speed_mps;           // Target speed in m/s
    double speed_percent;       // Speed as percentage of cruise speed
    double altitude_constraint_m; // Altitude constraint
    bool is_absolute_altitude;  // Altitude is absolute (not relative)
    
    // Timing information
    double arrival_time_s;      // Estimated arrival time
    double dwell_time_s;        // Dwell time at waypoint
    double time_since_start_s;  // Time since mission start
    
    // Waypoint status
    bool is_completed;          // Waypoint is completed
    bool is_active;             // Waypoint is currently active
    bool is_skipped;            // Waypoint was skipped
    uint8_t completion_status;  // Completion status code
    
    // Risk assessment
    double risk_score;          // Waypoint risk score
    bool has_obstacles;         // Obstacles detected near waypoint
    bool is_in_no_fly_zone;     // Waypoint is in no-fly zone
    
    // Environmental conditions
    double wind_speed_mps;      // Expected wind speed
    double wind_direction_deg;  // Expected wind direction
    double weather_score;       // Weather suitability score
    
    // Metadata
    uint64_t creation_time_us;  // Waypoint creation time
    uint64_t modification_time_us; // Last modification time
    std::string description;    // Waypoint description
    std::map<std::string, std::string> custom_fields; // Custom data
};
```

### MISSION_CURRENT Message Structure

```cpp
struct MissionCurrentData {
    // Current mission status
    uint16_t seq;               // Current mission sequence number
    
    // Mission progress
    uint16_t total_waypoints;   // Total number of waypoints
    uint16_t completed_waypoints; // Number of completed waypoints
    double progress_percent;    // Mission progress percentage
    
    // Current waypoint information
    std::string current_command_name; // Current waypoint command
    double distance_to_current_m; // Distance to current waypoint
    double bearing_to_current_deg; // Bearing to current waypoint
    
    // Timing information
    uint64_t mission_start_time_us; // Mission start time
    uint64_t current_waypoint_time_us; // Current waypoint start time
    double time_at_current_waypoint_s; // Time at current waypoint
    double estimated_mission_time_remaining_s; // Estimated time remaining
    
    // Navigation status
    bool is_on_course;          // Vehicle is on course to waypoint
    double cross_track_error_m; // Cross-track error in meters
    double along_track_progress_m; // Progress along track
    
    // Speed and performance
    double current_speed_mps;   // Current speed
    double average_speed_mps;   // Average mission speed
    double speed_vs_target_percent; // Speed vs target percentage
    
    // Altitude status
    double current_altitude_m;  // Current altitude
    double target_altitude_m;   // Target altitude
    double altitude_error_m;    // Altitude error
    
    // Mission state
    bool mission_active;        // Mission is currently active
    bool mission_paused;        // Mission is paused
    bool mission_completed;     // Mission is completed
    bool mission_aborted;       // Mission was aborted
    
    // Waypoint execution
    uint8_t waypoint_execution_state; // Current execution state
    bool waypoint_reached;      // Current waypoint reached
    bool waypoint_acceptance_met; // Waypoint acceptance criteria met
    
    // Mission constraints
    bool time_constraint_active; // Time constraint is active
    bool speed_constraint_active; // Speed constraint is active
    bool altitude_constraint_active; // Altitude constraint is active
    
    // Error handling
    uint8_t mission_error_code; // Mission error code
    bool has_mission_warnings;  // Mission has active warnings
    uint16_t warning_count;     // Number of active warnings
    
    // Mission statistics
    double total_distance_flown_m; // Total distance flown
    double total_flight_time_s; // Total flight time
    double fuel_consumed_percent; // Fuel consumed percentage
    
    // Environmental conditions
    double current_wind_speed_mps; // Current wind speed
    double current_wind_direction_deg; // Current wind direction
    double weather_impact_score; // Weather impact on mission
    
    // Predictive information
    double eta_to_next_waypoint_s; // ETA to next waypoint
    double eta_to_mission_completion_s; // ETA to mission completion
    double fuel_estimated_remaining_percent; // Estimated fuel remaining
    
    // Mission metadata
    std::string mission_name;   // Mission name
    std::string mission_type;   // Mission type (survey, patrol, etc.)
    uint8_t mission_priority;   // Mission priority level
    
    // Metadata
    uint64_t time_boot_ms;      // Time since boot
    uint64_t reception_time_us;
    std::string vehicle_mode;   // Current vehicle mode
};
```

## Parameter Management

### PARAM_VALUE Message Structure

```cpp
struct ParamValueData {
    // Parameter identification
    uint16_t param_id;          // Parameter ID (replaced by param_id_str)
    char param_id_str[16];      // Parameter ID as string
    float param_value;          // Parameter value
    uint8_t param_type;         // Parameter type
    uint16_t param_count;       // Total number of parameters
    uint16_t param_index;       // Index of this parameter
    
    // Parameter interpretation
    std::string parameter_name; // Full parameter name
    std::string parameter_type_str; // Parameter type as string
    std::string parameter_category; // Parameter category
    
    // Value analysis
    double value_numeric;       // Value as numeric type
    std::string value_string;   // Value as string representation
    bool is_default_value;      // Value is default
    bool is_modified;           // Value has been modified from default
    
    // Parameter constraints
    bool has_min_value;         // Parameter has minimum value
    bool has_max_value;         // Parameter has maximum value
    bool has_increment;         // Parameter has increment step
    double min_value;           // Minimum allowed value
    double max_value;           // Maximum allowed value
    double increment;           // Value increment step
    
    // Parameter validation
    bool value_valid;           // Current value is valid
    bool value_in_range;        // Value is within allowed range
    double range_violation_amount; // Amount of range violation
    
    // Parameter metadata
    std::string description;    // Parameter description
    std::string units;          // Parameter units
    std::string display_name;   // Display name for UI
    uint8_t decimal_places;     // Number of decimal places for display
    
    // Change tracking
    uint64_t last_change_time_us; // Time of last change
    double previous_value;      // Previous value
    double change_amount;       // Amount of last change
    double change_rate;         // Rate of change
    
    // Parameter groups
    std::string group_name;     // Parameter group name
    uint8_t group_id;           // Parameter group ID
    std::vector<std::string> related_parameters; // Related parameters
    
    // System impact
    bool requires_reboot;       // Parameter change requires reboot
    bool is_critical;           // Parameter is critical for safety
    bool affects_flight;        // Parameter affects flight characteristics
    uint8_t change_impact_level; // Impact level of changes
    
    // Configuration status
    bool is_readonly;           // Parameter is read-only
    bool is_volatile;           // Parameter is volatile (not saved)
    bool is_bitmask;            // Parameter is a bitmask
    uint32_t bitmask_value;     // Bitmask interpretation
    
    // Value ranges and recommendations
    double recommended_min;     // Recommended minimum value
    double recommended_max;     // Recommended maximum value
    double optimal_value;       // Optimal value for current conditions
    
    // Performance impact
    double performance_impact_score; // Impact on performance
    bool affects_power_consumption; // Affects power consumption
    bool affects_sensor_accuracy; // Affects sensor accuracy
    
    // Safety considerations
    bool safety_critical;       // Parameter is safety-critical
    bool has_safety_limits;     // Has safety limits enforced
    double safety_limit_min;    // Safety minimum limit
    double safety_limit_max;    // Safety maximum limit
    
    // Metadata
    uint64_t reception_time_us;
    std::string firmware_version; // Firmware version this parameter applies to
    std::string vehicle_type;   // Vehicle type this applies to
    std::map<std::string, std::string> custom_metadata; // Custom metadata
};
```

## Command and Control

### COMMAND_ACK Message Structure

```cpp
struct CommandAckData {
    // Command identification
    uint16_t command;           // Command ID
    uint8_t result;             // Result of command
    uint8_t progress;           // Progress of command (for long-running commands)
    int32_t result_param2;      // Additional result parameter
    uint8_t target_system;      // Target system ID
    uint8_t target_component;   // Target component ID
    
    // Command interpretation
    std::string command_name;   // Human-readable command name
    std::string result_str;     // Result as string
    bool command_successful;    // Command was successful
    bool command_failed;        // Command failed
    
    // Result analysis
    uint8_t result_code;        // Result code enumeration
    bool is_acceptance;         // Command was accepted
    bool is_rejection;          // Command was rejected
    bool is_in_progress;        // Command is in progress
    
    // Progress tracking
    double progress_percent;    // Progress as percentage
    uint64_t estimated_completion_time_us; // Estimated completion time
    uint64_t command_start_time_us; // Command start time
    double elapsed_time_s;      // Time elapsed since command start
    
    // Error information
    uint8_t error_code;         // Specific error code
    std::string error_message;  // Human-readable error message
    bool is_temporary_error;    // Error is temporary (retry may succeed)
    bool is_permanent_error;    // Error is permanent
    
    // Command context
    std::string command_source; // Source of command (GCS, onboard, etc.)
    uint8_t command_priority;   // Command priority level
    bool requires_confirmation; // Command requires confirmation
    
    // Impact analysis
    bool affects_flight_mode;   // Command affects flight mode
    bool affects_safety;        // Command affects safety systems
    bool affects_mission;       // Command affects mission execution
    
    // Timing information
    uint64_t command_send_time_us; // Time command was sent
    uint64_t response_time_us;   // Time response was received
    double round_trip_time_ms;   // Round trip time
    
    // System state at execution
    std::string vehicle_mode_at_execution; // Vehicle mode when command executed
    bool was_armed_at_execution; // Armed state when command executed
    double altitude_at_execution_m; // Altitude when command executed
    
    // Command validation
    bool command_valid;         // Command was valid
    bool parameters_valid;      // Command parameters were valid
    bool authorization_valid;   // Command authorization was valid
    
    // Retry information
    uint8_t retry_count;        // Number of retries attempted
    uint8_t max_retries_allowed; // Maximum retries allowed
    bool will_retry;            // Command will be retried
    
    // Dependencies
    std::vector<uint16_t> prerequisite_commands; // Commands that must complete first
    std::vector<uint16_t> dependent_commands; // Commands that depend on this one
    
    // Metadata
    uint64_t reception_time_us;
    std::string command_category; // Category of command (navigation, safety, etc.)
    std::map<std::string, std::string> execution_context; // Execution context data
};
```

## System Status and Health

### SYS_STATUS Message Structure

```cpp
struct SysStatusData {
    // Power supply status
    uint32_t voltage_battery;   // Battery voltage in millivolts
    int16_t current_battery;    // Battery current in milliamps
    int8_t current_consumed;    // Consumed current in 10*milliampere-hours
    int32_t energy_consumed;    // Consumed energy in 10*millijoules
    int16_t battery_remaining;  // Remaining battery energy in percent
    
    // Communication status
    uint16_t drop_rate_comm;    // Communication drop rate, dropped packets on all links
    uint16_t errors_comm;       // Communication errors, dropped packets on all links
    uint16_t errors_count1;     // Autopilot-specific errors
    uint16_t errors_count2;     // Autopilot-specific errors
    uint16_t errors_count3;     // Autopilot-specific errors
    uint16_t errors_count4;     // Autopilot-specific errors
    
    // Sensor status
    uint32_t onboard_control_sensors_present; // Bitmask showing which onboard controllers and sensors are present
    uint32_t onboard_control_sensors_enabled; // Bitmask showing which onboard controllers and sensors are enabled
    uint32_t onboard_control_sensors_health; // Bitmask showing which onboard controllers and sensors have an error
    
    // Derived power values
    double voltage_v;           // Battery voltage in volts
    double current_a;           // Battery current in amps
    double power_w;             // Power consumption in watts
    double energy_wh;           // Consumed energy in watt-hours
    
    // Power analysis
    double power_trend_wps;     // Power consumption trend
    double efficiency_score;    // Power efficiency score
    bool power_system_healthy;  // Power system is healthy
    double estimated_runtime_minutes; // Estimated runtime
    
    // Communication analysis
    double link_quality_percent; // Overall link quality
    double packet_loss_percent;  // Packet loss percentage
    double communication_health_score; // Communication health score
    bool communication_healthy; // Communication is healthy
    
    // Sensor presence analysis
    std::vector<std::string> present_sensors; // List of present sensors
    uint8_t sensor_count;       // Total number of sensors
    bool critical_sensors_present; // Critical sensors are present
    
    // Sensor enabled analysis
    std::vector<std::string> enabled_sensors; // List of enabled sensors
    uint8_t enabled_sensor_count; // Number of enabled sensors
    double sensor_enable_rate; // Percentage of sensors enabled
    
    // Sensor health analysis
    std::vector<std::string> healthy_sensors; // List of healthy sensors
    std::vector<std::string> failed_sensors; // List of failed sensors
    uint8_t healthy_sensor_count; // Number of healthy sensors
    double sensor_health_rate; // Percentage of sensors healthy
    
    // System health indicators
    double overall_health_score; // Overall system health score
    bool system_healthy;        // System is healthy
    uint8_t health_level;       // Health level (critical, warning, good)
    
    // Performance metrics
    double cpu_usage_percent;   // CPU usage percentage
    double memory_usage_percent; // Memory usage percentage
    double temperature_c;       // System temperature
    bool thermal_healthy;       // Thermal system is healthy
    
    // Error analysis
    uint32_t total_errors;      // Total error count
    double error_rate;          // Error rate per second
    uint8_t critical_error_count; // Critical error count
    bool has_critical_errors;   // Has critical errors
    
    // System state
    bool is_armed;              // System is armed
    bool is_flying;             // System is flying
    std::string flight_mode;    // Current flight mode
    uint8_t system_state;       // System state enumeration
    
    // Battery status analysis
    bool battery_healthy;       // Battery is healthy
    bool low_battery_warning;   // Low battery warning
    bool critical_battery_warning; // Critical battery warning
    double battery_health_score; // Battery health score
    
    // Communication link status
    bool primary_link_healthy;  // Primary communication link healthy
    bool has_backup_link;       // Has backup communication link
    uint8_t active_link_count;  // Number of active links
    
    // Sensor specific analysis
    bool gps_healthy;           // GPS is healthy
    bool imu_healthy;           // IMU is healthy
    bool magnetometer_healthy;  // Magnetometer is healthy
    bool barometer_healthy;     // Barometer is healthy
    bool pitot_healthy;         // Pitot tube is healthy
    
    // Navigation status
    bool navigation_healthy;    // Navigation system healthy
    bool position_good;         // Position estimate is good
    bool velocity_good;         // Velocity estimate is good
    double navigation_accuracy_score; // Navigation accuracy score
    
    // Control system status
    bool control_system_healthy; // Control system healthy
    bool actuators_responding;   // Actuators are responding
    double control_loop_frequency_hz; // Control loop frequency
    
    // Safety systems
    bool safety_systems_healthy; // Safety systems healthy
    bool geofence_active;       // Geofence is active
    bool geofence_breached;     // Geofence is breached
    bool failsafe_active;       // Failsafe is active
    
    // Metadata
    uint64_t time_boot_ms;      // Time since boot
    uint64_t reception_time_us;
    std::string autopilot_version; // Autopilot firmware version
    std::string hardware_version; // Hardware version
};
```

## Extended Data Types

### Custom Data Message Structure

```cpp
struct CustomDataMessage {
    // Message identification
    uint16_t message_type_id;   // Custom message type ID
    std::string message_type_name; // Human-readable type name
    uint8_t version;            // Message format version
    
    // Data payload
    std::vector<uint8_t> raw_payload; // Raw data payload
    std::map<std::string, CustomField> parsed_fields; // Parsed fields
    
    // Timestamp information
    uint64_t timestamp_us;      // Message timestamp
    uint64_t sequence_number;   // Sequence number
    
    // Source information
    uint8_t source_system_id;   // Source system ID
    uint8_t source_component_id; // Source component ID
    std::string source_name;    // Source component name
    
    // Data validation
    bool checksum_valid;        // Checksum is valid
    bool format_valid;          // Format is valid
    uint8_t validation_errors;  // Validation error count
    
    // Metadata
    std::string description;    // Message description
    std::string data_format;    // Data format specification
    std::map<std::string, std::string> metadata; // Additional metadata
};

struct CustomField {
    std::string field_name;     // Field name
    std::string field_type;     // Field data type
    std::variant<int32_t, uint32_t, float, double, std::string, std::vector<uint8_t>> value;
    std::string units;          // Field units
    std::string description;    // Field description
};
```

### Data Batch Structure

```cpp
struct DataBatch {
    // Batch identification
    uint64_t batch_id;          // Unique batch identifier
    uint64_t start_time_us;     // Batch start time
    uint64_t end_time_us;       // Batch end time
    uint32_t message_count;     // Number of messages in batch
    
    // Batch content
    std::vector<MessageContainer> messages; // Messages in batch
    std::map<uint16_t, uint32_t> message_type_counts; // Count by message type
    
    // Batch statistics
    double data_rate_bps;       // Average data rate
    double message_rate_hz;     // Average message rate
    uint64_t total_bytes;       // Total bytes in batch
    
    // Quality metrics
    double completeness_score;  // Batch completeness
    double quality_score;       // Data quality score
    uint32_t error_count;       // Number of errors
    
    // Compression information
    bool is_compressed;         // Batch is compressed
    std::string compression_type; // Compression algorithm used
    double compression_ratio;   // Compression ratio
    
    // Metadata
    std::string source_system;  // Source system name
    std::string collection_session; // Collection session ID
    std::map<std::string, std::string> batch_metadata; // Additional metadata
};
```

## Data Collection Patterns

### Collection Session Structure

```cpp
struct CollectionSession {
    // Session identification
    std::string session_id;     // Unique session identifier
    std::string session_name;   // Human-readable session name
    std::string description;    // Session description
    
    // Timing information
    uint64_t start_time_us;     // Session start time
    uint64_t end_time_us;       // Session end time
    uint64_t duration_us;       // Session duration
    
    // Vehicle information
    std::string vehicle_id;     // Vehicle identifier
    std::string vehicle_type;   // Vehicle type
    std::string autopilot_type; // Autopilot type
    
    // Collection statistics
    uint64_t total_messages;    // Total messages collected
    uint64_t total_bytes;       // Total bytes collected
    double average_message_rate_hz; // Average message rate
    
    // Data quality
    double overall_quality_score; // Overall data quality
    double completeness_percentage; // Data completeness
    uint32_t error_count;       // Total errors encountered
    
    // Configuration
    std::set<uint16_t> enabled_message_types; // Enabled message types
    std::map<std::string, std::string> collection_parameters; // Collection parameters
    
    // Storage information
    std::string storage_location; // Storage location
    uint64_t storage_size_bytes; // Storage size used
    std::vector<std::string> file_names; // Generated file names
    
    // Session status
    bool is_active;             // Session is active
    bool is_completed;          // Session is completed
    bool has_errors;            // Session has errors
    
    // Metadata
    std::map<std::string, std::string> custom_metadata; // Custom metadata
    std::vector<std::string> tags; // Session tags
    std::string operator_name;  // Operator name
};
```

### Data Stream Structure

```cpp
struct DataStream {
    // Stream identification
    std::string stream_id;      // Stream identifier
    std::string stream_name;    // Stream name
    std::string stream_type;    // Stream type (telemetry, command, etc.)
    
    // Stream configuration
    uint16_t message_type;      // MAVLink message type
    uint8_t system_id;          // System ID filter
    uint8_t component_id;       // Component ID filter
    double target_rate_hz;      // Target stream rate
    
    // Stream statistics
    uint64_t messages_received; // Messages received
    uint64_t bytes_received;    // Bytes received
    double actual_rate_hz;      // Actual stream rate
    double quality_score;       // Stream quality score
    
    // Timing information
    uint64_t first_message_time_us; // First message time
    uint64_t last_message_time_us;  // Last message time
    uint64_t total_duration_us;     // Total duration
    
    // Stream health
    bool is_active;             // Stream is active
    bool is_healthy;            // Stream is healthy
    uint32_t consecutive_errors;    // Consecutive errors
    double packet_loss_rate;    // Packet loss rate
    
    // Performance metrics
    double latency_ms;          // Average latency
    double jitter_ms;           // Latency jitter
    double throughput_bps;      // Throughput
    
    // Stream content
    std::vector<MessageContainer> recent_messages; // Recent messages
    std::map<std::string, double> field_statistics; // Field statistics
    
    // Metadata
    std::string source_description; // Source description
    std::map<std::string, std::string> stream_metadata; // Stream metadata
};
```

## Storage Formats

### Database Storage Schema

```cpp
struct DatabaseSchema {
    // Tables for different data types
    struct MessagesTable {
        uint64_t id;            // Primary key
        uint64_t timestamp_us;  // Timestamp
        uint8_t system_id;      // System ID
        uint8_t component_id;   // Component ID
        uint16_t message_type;  // Message type
        std::vector<uint8_t> raw_data; // Raw message data
        uint32_t sequence_num;  // Sequence number
        uint64_t reception_time_us; // Reception time
    };
    
    struct VehiclesTable {
        std::string vehicle_id; // Vehicle identifier
        uint8_t system_id;      // System ID
        std::string vehicle_type; // Vehicle type
        std::string autopilot_type; // Autopilot type
        uint64_t first_seen_us; // First seen time
        uint64_t last_seen_us;  // Last seen time
        bool is_active;         // Active status
        std::map<std::string, std::string> metadata; // Vehicle metadata
    };
    
    struct SessionsTable {
        std::string session_id; // Session identifier
        std::string session_name; // Session name
        uint64_t start_time_us; // Start time
        uint64_t end_time_us;   // End time
        std::string vehicle_id; // Vehicle ID
        uint64_t message_count; // Message count
        double quality_score;   // Quality score
        std::map<std::string, std::string> session_metadata; // Session metadata
    };
    
    struct MessageTypesTable {
        uint16_t message_id;    // Message ID
        std::string message_name; // Message name
        std::string description; // Description
        std::string schema_version; // Schema version
        bool is_active;         // Active status
    };
};
```

### File Storage Format

```cpp
struct FileStorageFormat {
    // File header
    struct FileHeader {
        char magic[4];          // Magic identifier ("MAVL")
        uint16_t version;       // File format version
        uint16_t header_size;   // Header size
        uint64_t creation_time_us; // File creation time
        std::string creator_info; // Creator information
        std::string session_id; // Session ID
        uint8_t system_id;      // System ID
        uint8_t component_id;   // Component ID
        uint64_t message_count; // Total messages
        uint64_t file_size;     // Total file size
        uint32_t checksum;      // Header checksum
    };
    
    // Message entry
    struct MessageEntry {
        uint64_t timestamp_us;  // Message timestamp
        uint32_t message_size;  // Message size
        uint16_t message_type;  // Message type
        uint8_t system_id;      // System ID
        uint8_t component_id;   // Component ID
        uint32_t sequence_num;  // Sequence number
        std::vector<uint8_t> message_data; // Message data
        uint32_t entry_checksum; // Entry checksum
    };
    
    // File index
    struct FileIndex {
        uint64_t index_offset;  // Index offset in file
        uint32_t entry_count;   // Number of entries
        std::vector<IndexEntry> entries; // Index entries
        uint32_t index_checksum; // Index checksum
    };
    
    struct IndexEntry {
        uint64_t file_offset;   // Offset in file
        uint64_t timestamp_us;  // Timestamp
        uint16_t message_type;  // Message type
        uint32_t message_size;  // Message size
    };
};
```

## API Integration

### REST API Data Structures

```cpp
struct ApiResponse {
    // Response metadata
    bool success;               // Request was successful
    std::string message;        // Response message
    uint32_t status_code;       // HTTP status code
    uint64_t timestamp_us;      // Response timestamp
    
    // Data payload
    std::variant<
        std::vector<MessageContainer>,
        CollectionSession,
        VehicleInfo,
        std::map<std::string, double>
    > data;
    
    // Pagination information
    bool has_pagination;        // Response includes pagination
    uint32_t total_items;       // Total number of items
    uint32_t page_size;         // Page size
    uint32_t current_page;      // Current page number
    bool has_next_page;         // Has next page
    bool has_previous_page;     // Has previous page
    
    // Error information
    bool has_error;             // Response has error
    std::string error_code;     // Error code
    std::string error_message;  // Error message
    std::map<std::string, std::string> error_details; // Error details
    
    // Request information
    std::string request_id;     // Request identifier
    std::string endpoint;       // API endpoint
    std::map<std::string, std::string> request_parameters; // Request parameters
    
    // Performance metrics
    uint64_t processing_time_us; // Processing time
    uint64_t database_query_time_us; // Database query time
    uint32_t result_count;      // Number of results
    
    // Cache information
    bool from_cache;            // Response from cache
    uint64_t cache_age_us;      // Cache age
    uint64_t cache_ttl_us;      // Cache time to live
};

struct QueryParameters {
    // Time range
    uint64_t start_time_us;     // Start time
    uint64_t end_time_us;       // End time
    
    // Filters
    std::set<uint16_t> message_types; // Message type filter
    std::set<uint8_t> system_ids; // System ID filter
    std::set<uint8_t> component_ids; // Component ID filter
    std::string session_id;     // Session ID filter
    
    // Pagination
    uint32_t page_size;         // Page size
    uint32_t page_number;       // Page number
    
    // Sorting
    std::string sort_field;     // Sort field
    bool sort_ascending;        // Sort direction
    
    // Field selection
    std::vector<std::string> fields; // Fields to return
    bool include_raw_data;      // Include raw message data
    
    // Aggregation
    bool aggregate;             // Aggregate results
    std::string aggregation_type; // Aggregation type
    std::string aggregation_field; // Aggregation field
    
    // Quality filter
    double min_quality_score;   // Minimum quality score
    bool exclude_errors;        // Exclude error messages
};
```

## Examples and Use Cases

### Data Analysis Example Structure

```cpp
struct FlightAnalysisReport {
    // Report metadata
    std::string report_id;      // Report identifier
    std::string session_id;     // Session ID analyzed
    uint64_t generation_time_us; // Report generation time
    std::string analysis_type;  // Type of analysis
    
    // Flight summary
    double total_flight_time_s; // Total flight time
    double total_distance_m;    // Total distance flown
    double max_altitude_m;      // Maximum altitude
    double max_speed_mps;       // Maximum speed
    double average_speed_mps;   // Average speed
    
    // Performance metrics
    double flight_efficiency_score; // Flight efficiency
    double navigation_accuracy_score; // Navigation accuracy
    double communication_quality_score; // Communication quality
    double system_health_score; // System health
    
    // Event analysis
    uint32_t takeoff_events;    // Number of takeoff events
    uint32_t landing_events;    // Number of landing events
    uint32_t mode_changes;      // Number of mode changes
    uint32_t error_events;      // Number of error events
    
    // Battery analysis
    double battery_consumed_wh; // Battery consumed
    double average_power_w;     // Average power consumption
    double battery_efficiency;  // Battery efficiency
    bool low_battery_events;    // Low battery events occurred
    
    // Communication analysis
    double average_link_quality_percent; // Average link quality
    uint32_t communication_dropouts; // Communication dropouts
    double packet_loss_percent; // Packet loss rate
    
    // Sensor analysis
    std::map<std::string, double> sensor_health_scores; // Sensor health scores
    std::vector<std::string> sensor_anomalies; // Sensor anomalies detected
    
    // Safety analysis
    uint32_t safety_events;     // Safety events count
    bool geofence_breaches;     // Geofence breaches occurred
    bool failsafe_activated;    // Failsafe was activated
    
    // Recommendations
    std::vector<std::string> maintenance_recommendations; // Maintenance recommendations
    std::vector<std::string> performance_improvements; // Performance improvement suggestions
    
    // Data quality
    double data_completeness_percent; // Data completeness
    double data_quality_score; // Overall data quality
    std::vector<std::string> data_gaps; // Data gaps identified
    
    // Metadata
    std::map<std::string, std::string> analysis_parameters; // Analysis parameters
    std::vector<std::string> analysis_warnings; // Analysis warnings
};
```

### Real-time Monitoring Structure

```cpp
struct RealTimeMonitoringData {
    // Current status
    bool is_connected;          // Vehicle is connected
    bool is_armed;              // Vehicle is armed
    bool is_flying;             // Vehicle is flying
    std::string current_mode;   // Current flight mode
    
    // Position and attitude
    double latitude_deg;        // Current latitude
    double longitude_deg;       // Current longitude
    double altitude_m;          // Current altitude
    double heading_deg;         // Current heading
    double roll_deg;            // Current roll
    double pitch_deg;           // Current pitch
    
    // Velocity
    double ground_speed_mps;    // Ground speed
    double vertical_speed_mps;  // Vertical speed
    double climb_rate_mps;      // Climb rate
    
    // System status
    double battery_remaining_percent; // Battery remaining
    double battery_voltage_v;   // Battery voltage
    double system_health_score; // System health
    
    // Communication
    double link_quality_percent; // Link quality
    uint32_t messages_per_second; // Message rate
    double latency_ms;          // Communication latency
    
    // Sensors
    bool gps_fix;               // GPS fix status
    uint8_t satellite_count;    // Satellite count
    double hdop;                // Horizontal dilution of precision
    
    // Warnings and errors
    std::vector<std::string> active_warnings; // Active warnings
    std::vector<std::string> active_errors;   // Active errors
    uint8_t warning_level;      // Warning level
    
    // Performance metrics
    double cpu_usage_percent;   // CPU usage
    double memory_usage_percent; // Memory usage
    double system_temperature_c; // System temperature
    
    // Mission status
    bool mission_active;        // Mission is active
    uint16_t current_waypoint;  // Current waypoint
    double mission_progress_percent; // Mission progress
    
    // Timing
    uint64_t last_update_us;    // Last update time
    uint64_t uptime_s;          // System uptime
    double data_age_ms;         // Age of data
    
    // Quality indicators
    double data_quality_score;  // Data quality score
    bool data_fresh;            // Data is fresh
    bool sensors_healthy;       // All sensors healthy
    
    // Metadata
    std::string vehicle_id;     // Vehicle ID
    std::string vehicle_type;   // Vehicle type
    uint64_t session_id;        // Session ID
};
```

This comprehensive documentation covers the extensive data structures used in MAVLink data collection, providing detailed specifications for each component, field relationships, derived calculations, and usage patterns. The structures are designed to support both real-time monitoring and post-flight analysis while maintaining data integrity and providing extensive metadata for context and analysis.
