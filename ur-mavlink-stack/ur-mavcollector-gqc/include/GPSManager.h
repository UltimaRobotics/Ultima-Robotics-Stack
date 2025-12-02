#pragma once

#include <memory>
#include <functional>
#include <atomic>

// Forward declarations
class GPSProvider;
struct sensor_gps_s;
struct satellite_info_s;
struct sensor_gnss_relative_s;

/// GPS Manager - Singleton pattern similar to QGroundControl
/// Manages all GPS data collection and processing operations
class GPSManager
{
public:
    /// GPS data update callbacks
    using GPSUpdateCallback = std::function<void(const sensor_gps_s&)>;
    using SatelliteUpdateCallback = std::function<void(const satellite_info_s&)>;
    using GNSSRelativeUpdateCallback = std::function<void(const sensor_gnss_relative_s&)>;
    
    /// Get singleton instance
    static GPSManager* instance();
    
    /// Initialize GPS manager
    bool initialize();
    
    /// Shutdown GPS manager
    void shutdown();
    
    /// Register GPS data update callbacks
    void setGPSUpdateCallback(GPSUpdateCallback callback);
    void setSatelliteUpdateCallback(SatelliteUpdateCallback callback);
    void setGNSSRelativeUpdateCallback(GNSSRelativeUpdateCallback callback);
    
    /// Get current GPS data
    const sensor_gps_s& getCurrentGPSData() const;
    const satellite_info_s& getCurrentSatelliteData() const;
    const sensor_gnss_relative_s& getCurrentGNSSRelativeData() const;
    
    /// GPS data availability
    bool isGPSDataAvailable() const;
    bool isSatelliteDataAvailable() const;
    bool isGNSSRelativeDataAvailable() const;
    
    /// Request GPS data streams (similar to QGC's stream requests)
    void requestGPSDataStreams(uint32_t rateHz = 10);
    
    /// Process MAVLink GPS messages (centralized handling)
    void handleMAVLinkMessage(const mavlink_message_t& message);
    
private:
    GPSManager();
    ~GPSManager();
    
    // Singleton pattern
    GPSManager(const GPSManager&) = delete;
    GPSManager& operator=(const GPSManager&) = delete;
    
    static GPSManager* _instance;
    static std::mutex _instanceMutex;
    
    // GPS provider
    std::unique_ptr<GPSProvider> _gpsProvider;
    
    // Data storage
    sensor_gps_s _currentGPSData;
    satellite_info_s _currentSatelliteData;
    sensor_gnss_relative_s _currentGNSSRelativeData;
    
    // Data availability flags
    std::atomic<bool> _gpsDataAvailable{false};
    std::atomic<bool> _satelliteDataAvailable{false};
    std::atomic<bool> _gnssRelativeDataAvailable{false};
    
    // Callbacks
    GPSUpdateCallback _gpsUpdateCallback;
    SatelliteUpdateCallback _satelliteUpdateCallback;
    GNSSRelativeUpdateCallback _gnssRelativeUpdateCallback;
    
    // Internal methods
    void _updateGPSData(const sensor_gps_s& data);
    void _updateSatelliteData(const satellite_info_s& data);
    void _updateGNSSRelativeData(const sensor_gnss_relative_s& data);
    
    // MAVLink message handlers
    void _handleGPSRawInt(const mavlink_message_t& message);
    void _handleGPS2Raw(const mavlink_message_t& message);
    void _handleGlobalPositionInt(const mavlink_message_t& message);
    void _handleHighLatency2(const mavlink_message_t& message);
    void _handleGPSStatus(const mavlink_message_t& message);
    void _handleGPS2Raw(const mavlink_message_t& message);
    void _handleGNSSInt(const mavlink_message_t& message);
};
