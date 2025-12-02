#pragma once

#include <cstdint>
#include <memory>
#include <functional>

// Standardized GPS data structures (similar to QGC)
struct sensor_gps_s
{
    uint64_t timestamp = 0;
    uint64_t timestamp_sample = 0;
    uint32_t device_id = 0;
    
    double latitude_deg = 0.0;
    double longitude_deg = 0.0;
    double altitude_msl_m = 0.0;
    double altitude_ellipsoid_m = 0.0;
    
    float s_variance_m_s = 0.0f;
    float c_variance_rad = 0.0f;
    
    // Fix type constants (matching QGC)
    static constexpr uint8_t FIX_TYPE_NONE = 1;
    static constexpr uint8_t FIX_TYPE_2D = 2;
    static constexpr uint8_t FIX_TYPE_3D = 3;
    static constexpr uint8_t FIX_TYPE_RTCM_CODE_DIFFERENTIAL = 4;
    static constexpr uint8_t FIX_TYPE_RTK_FLOAT = 5;
    static constexpr uint8_t FIX_TYPE_RTK_FIXED = 6;
    static constexpr uint8_t FIX_TYPE_EXTRAPOLATED = 8;
    
    uint8_t fix_type = FIX_TYPE_NONE;
    
    float eph = 0.0f;  // Horizontal position accuracy (m)
    float epv = 0.0f;  // Vertical position accuracy (m)
    
    float hdop = 0.0f; // Horizontal dilution of precision
    float vdop = 0.0f; // Vertical dilution of precision
    
    int32_t noise_per_ms = 0;
    uint16_t automatic_gain_control = 0;
    
    // Jamming detection
    static constexpr uint8_t JAMMING_STATE_UNKNOWN = 0;
    static constexpr uint8_t JAMMING_STATE_OK = 1;
    static constexpr uint8_t JAMMING_STATE_WARNING = 2;
    static constexpr uint8_t JAMMING_STATE_CRITICAL = 3;
    uint8_t jamming_state = JAMMING_STATE_UNKNOWN;
    int32_t jamming_indicator = 0;
    
    // Spoofing detection
    static constexpr uint8_t SPOOFING_STATE_UNKNOWN = 0;
    static constexpr uint8_t SPOOFING_STATE_NONE = 1;
    static constexpr uint8_t SPOOFING_STATE_INDICATED = 2;
    static constexpr uint8_t SPOOFING_STATE_MULTIPLE = 3;
    uint8_t spoofing_state = SPOOFING_STATE_UNKNOWN;
    
    // Velocity information
    float vel_m_s = 0.0f;
    float vel_n_m_s = 0.0f;
    float vel_e_m_s = 0.0f;
    float vel_d_m_s = 0.0f;
    float cog_rad = 0.0f;
    bool vel_ned_valid = false;
    
    // Timing
    int32_t timestamp_time_relative = 0;
    uint64_t time_utc_usec = 0;
    
    // Satellite information
    uint8_t satellites_used = 0;
    
    // Heading information
    float heading = 0.0f;
    float heading_offset = 0.0f;
    float heading_accuracy = 0.0f;
    
    // RTCM information
    float rtcm_injection_rate = 0.0f;
    uint8_t selected_rtcm_instance = 0;
    bool rtcm_crc_failed = false;
    
    static constexpr uint8_t RTCM_MSG_USED_UNKNOWN = 0;
    static constexpr uint8_t RTCM_MSG_USED_NOT_USED = 1;
    static constexpr uint8_t RTCM_MSG_USED_USED = 2;
    uint8_t rtcm_msg_used = RTCM_MSG_USED_UNKNOWN;
};

struct satellite_info_s
{
    uint64_t timestamp = 0;
    static constexpr uint8_t SAT_INFO_MAX_SATELLITES = 20;
    
    uint8_t count = 0;
    uint8_t svid[20] = {0};
    uint8_t used[20] = {0};
    uint8_t elevation[20] = {0};
    uint8_t azimuth[20] = {0};
    uint8_t snr[20] = {0};
    uint8_t prn[20] = {0};
};

struct sensor_gnss_relative_s
{
    uint64_t timestamp = 0;
    uint64_t timestamp_sample = 0;
    
    uint32_t device_id = 0;
    uint64_t time_utc_usec = 0;
    
    uint16_t reference_station_id = 0;
    
    float position[3] = {0.0f, 0.0f, 0.0f};
    float position_accuracy[3] = {0.0f, 0.0f, 0.0f};
    
    float heading = 0.0f;
    float heading_accuracy = 0.0f;
    
    float position_length = 0.0f;
    float accuracy_length = 0.0f;
    
    bool gnss_fix_ok = false;
    bool differential_solution = false;
    bool relative_position_valid = false;
    bool carrier_solution_floating = false;
    bool carrier_solution_fixed = false;
    bool moving_base_mode = false;
    bool reference_position_miss = false;
    bool reference_observations_miss = false;
    bool heading_valid = false;
    bool relative_position_normalized = false;
};

/// GPS Provider - Abstraction layer for GPS data processing
/// Similar to QGroundControl's GPSProvider but adapted for MAVLink
class GPSProvider
{
public:
    /// GPS data update callbacks
    using GPSDataCallback = std::function<void(const sensor_gps_s&)>;
    using SatelliteDataCallback = std::function<void(const satellite_info_s&)>;
    using GNSSRelativeCallback = std::function<void(const sensor_gnss_relative_s&)>;
    
    GPSProvider();
    ~GPSProvider();
    
    /// Set data update callbacks
    void setGPSDataCallback(GPSDataCallback callback);
    void setSatelliteDataCallback(SatelliteDataCallback callback);
    void setGNSSRelativeCallback(GNSSRelativeCallback callback);
    
    /// Process MAVLink messages and convert to standardized format
    void processMAVLinkMessage(const mavlink_message_t& message);
    
    /// Get current data
    const sensor_gps_s& getCurrentGPSData() const { return _currentGPSData; }
    const satellite_info_s& getCurrentSatelliteData() const { return _currentSatelliteData; }
    const sensor_gnss_relative_s& getCurrentGNSSRelativeData() const { return _currentGNSSRelativeData; }
    
    /// Data availability
    bool isGPSDataAvailable() const { return _gpsDataAvailable; }
    bool isSatelliteDataAvailable() const { return _satelliteDataAvailable; }
    bool isGNSSRelativeDataAvailable() const { return _gnssRelativeDataAvailable; }
    
    /// Request specific GPS data streams
    void requestGPSStreams(uint32_t rateHz = 10);
    
private:
    // Current data
    sensor_gps_s _currentGPSData;
    satellite_info_s _currentSatelliteData;
    sensor_gnss_relative_s _currentGNSSRelativeData;
    
    // Data availability flags
    bool _gpsDataAvailable = false;
    bool _satelliteDataAvailable = false;
    bool _gnssRelativeDataAvailable = false;
    
    // Callbacks
    GPSDataCallback _gpsDataCallback;
    SatelliteDataCallback _satelliteDataCallback;
    GNSSRelativeCallback _gnssRelativeCallback;
    
    // MAVLink message processing
    void _processGPSRawInt(const mavlink_message_t& message);
    void _processGPS2Raw(const mavlink_message_t& message);
    void _processGlobalPositionInt(const mavlink_message_t& message);
    void _processHighLatency2(const mavlink_message_t& message);
    void _processGPSStatus(const mavlink_message_t& message);
    void _processGNSSInt(const mavlink_message_t& message);
    
    // Data validation and conversion
    bool _validateGPSData(const sensor_gps_s& data) const;
    bool _validateSatelliteData(const satellite_info_s& data) const;
    bool _validateGNSSRelativeData(const sensor_gnss_relative_s& data) const;
    
    void _updateGPSData(const sensor_gps_s& data);
    void _updateSatelliteData(const satellite_info_s& data);
    void _updateGNSSRelativeData(const sensor_gnss_relative_s& data);
    
    // Utility methods
    uint64_t _getCurrentTimestamp() const;
    void _initializeDataStructures();
};
