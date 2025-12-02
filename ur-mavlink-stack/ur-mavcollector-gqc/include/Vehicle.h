#pragma once

#include <string>
#include <memory>
#include <functional>

#include "FactGroup.h"
#include "MAVLinkUdpConnection.h"

// Forward declarations for MAVLink types
struct __mavlink_autopilot_version_t;
struct __mavlink_message_t;

// Forward declarations
class Vehicle;
class ParameterManager;
class BoardIdentifier;

/// Main Vehicle class that manages all vehicle data collection.
/// This is a Qt-free port of QGroundControl's Vehicle class.
class Vehicle : public FactGroup
{
public:
    explicit Vehicle(MAVLinkUdpConnection* connection);
    virtual ~Vehicle();

    /// System ID of this vehicle
    uint8_t systemId() const { return _systemId; }
    void setSystemId(uint8_t systemId) { _systemId = systemId; }

    /// Component ID of this vehicle
    uint8_t componentId() const { return _componentId; }
    void setComponentId(uint8_t componentId) { _componentId = componentId; }

    /// Vehicle type
    uint8_t vehicleType() const { return _vehicleType; }
    void setVehicleType(uint8_t vehicleType) { _vehicleType = vehicleType; }

    /// Autopilot type
    uint8_t autopilotType() const { return _autopilotType; }
    void setAutopilotType(uint8_t autopilotType) { _autopilotType = autopilotType; }

    /// Base mode
    uint8_t baseMode() const { return _baseMode; }
    void setBaseMode(uint8_t baseMode) { _baseMode = baseMode; }

    /// Custom mode
    uint32_t customMode() const { return _customMode; }
    void setCustomMode(uint32_t customMode) { _customMode = customMode; }

    /// System status
    uint8_t systemStatus() const { return _systemStatus; }
    void setSystemStatus(uint8_t systemStatus) { _systemStatus = systemStatus; }

    /// MAVLink version
    uint8_t mavlinkVersion() const { return _mavlinkVersion; }
    void setMavlinkVersion(uint8_t mavlinkVersion) { _mavlinkVersion = mavlinkVersion; }

    /// Get parameter manager
    std::shared_ptr<ParameterManager> parameterManager() { return _parameterManager; }

    /// Get UDP connection
    MAVLinkUdpConnection* connection() { return _connection; }

    /// Send MAVLink message to vehicle
    bool sendMessage(const mavlink_message_t &message);

    /// Send MAVLink command to vehicle
    bool sendCommand(uint16_t command, uint8_t confirmation, float param1 = 0.0f, float param2 = 0.0f, 
                     float param3 = 0.0f, float param4 = 0.0f, float param5 = 0.0f, float param6 = 0.0f, float param7 = 0.0f);

    /// Check if vehicle is armed
    bool armed() const;

    /// Check if vehicle is flying
    bool flying() const;

    /// Get vehicle mode as string
    std::string flightMode() const;

    /// Get vehicle type as string
    std::string vehicleTypeString() const;

    /// Get autopilot type as string
    std::string autopilotTypeString() const;

    /// Get system status as string
    std::string systemStatusString() const;

    /// Get autopilot version information
    uint64_t capabilities() const { return _capabilities; }
    uint64_t uid() const { return _uid; }
    uint32_t flightSwVersion() const { return _flightSwVersion; }
    uint32_t middlewareSwVersion() const { return _middlewareSwVersion; }
    uint32_t osSwVersion() const { return _osSwVersion; }
    uint32_t boardVersion() const { return _boardVersion; }
    uint16_t vendorId() const { return _vendorId; }
    uint16_t productId() const { return _productId; }
    const uint8_t* flightCustomVersion() const { return _flightCustomVersion; }
    std::string flightCustomVersionString() const;
    std::string flightSwVersionString() const;
    std::string middlewareSwVersionString() const;
    std::string osSwVersionString() const;
    std::string capabilitiesString() const;
    
    /// Board identification
    std::string boardName() const;
    std::string boardClass() const;
    std::string boardIdentification() const;

    // Fact accessors for main vehicle facts
    std::shared_ptr<Fact> roll() { return getFact("roll"); }
    std::shared_ptr<Fact> pitch() { return getFact("pitch"); }
    std::shared_ptr<Fact> heading() { return getFact("heading"); }
    std::shared_ptr<Fact> rollRate() { return getFact("rollRate"); }
    std::shared_ptr<Fact> pitchRate() { return getFact("pitchRate"); }
    std::shared_ptr<Fact> yawRate() { return getFact("yawRate"); }
    std::shared_ptr<Fact> groundSpeed() { return getFact("groundSpeed"); }
    std::shared_ptr<Fact> airSpeed() { return getFact("airSpeed"); }
    std::shared_ptr<Fact> climbRate() { return getFact("climbRate"); }
    std::shared_ptr<Fact> altitudeRelative() { return getFact("altitudeRelative"); }
    std::shared_ptr<Fact> altitudeAMSL() { return getFact("altitudeAMSL"); }
    std::shared_ptr<Fact> altitudeAboveTerr() { return getFact("altitudeAboveTerr"); }
    std::shared_ptr<Fact> flightDistance() { return getFact("flightDistance"); }
    std::shared_ptr<Fact> distanceToHome() { return getFact("distanceToHome"); }
    std::shared_ptr<Fact> timeToHome() { return getFact("timeToHome"); }
    std::shared_ptr<Fact> missionItemIndex() { return getFact("missionItemIndex"); }
    std::shared_ptr<Fact> headingToNextWP() { return getFact("headingToNextWP"); }
    std::shared_ptr<Fact> distanceToNextWP() { return getFact("distanceToNextWP"); }
    std::shared_ptr<Fact> headingToHome() { return getFact("headingToHome"); }
    std::shared_ptr<Fact> headingFromHome() { return getFact("headingFromHome"); }
    std::shared_ptr<Fact> headingFromGCS() { return getFact("headingFromGCS"); }
    std::shared_ptr<Fact> distanceToGCS() { return getFact("distanceToGCS"); }
    std::shared_ptr<Fact> hobbs() { return getFact("hobbs"); }
    std::shared_ptr<Fact> throttlePct() { return getFact("throttlePct"); }
    std::shared_ptr<Fact> imuTemp() { return getFact("imuTemp"); }

    // Const versions of the above methods
    std::shared_ptr<Fact> roll() const { return getFact("roll"); }
    std::shared_ptr<Fact> pitch() const { return getFact("pitch"); }
    std::shared_ptr<Fact> heading() const { return getFact("heading"); }
    std::shared_ptr<Fact> rollRate() const { return getFact("rollRate"); }
    std::shared_ptr<Fact> pitchRate() const { return getFact("pitchRate"); }
    std::shared_ptr<Fact> yawRate() const { return getFact("yawRate"); }
    std::shared_ptr<Fact> groundSpeed() const { return getFact("groundSpeed"); }
    std::shared_ptr<Fact> airSpeed() const { return getFact("airSpeed"); }
    std::shared_ptr<Fact> climbRate() const { return getFact("climbRate"); }
    std::shared_ptr<Fact> altitudeRelative() const { return getFact("altitudeRelative"); }
    std::shared_ptr<Fact> altitudeAMSL() const { return getFact("altitudeAMSL"); }
    std::shared_ptr<Fact> altitudeAboveTerr() const { return getFact("altitudeAboveTerr"); }
    std::shared_ptr<Fact> flightDistance() const { return getFact("flightDistance"); }
    std::shared_ptr<Fact> distanceToHome() const { return getFact("distanceToHome"); }
    std::shared_ptr<Fact> timeToHome() const { return getFact("timeToHome"); }
    std::shared_ptr<Fact> missionItemIndex() const { return getFact("missionItemIndex"); }
    std::shared_ptr<Fact> headingToNextWP() const { return getFact("headingToNextWP"); }
    std::shared_ptr<Fact> distanceToNextWP() const { return getFact("distanceToNextWP"); }
    std::shared_ptr<Fact> headingToHome() const { return getFact("headingToHome"); }
    std::shared_ptr<Fact> headingFromHome() const { return getFact("headingFromHome"); }
    std::shared_ptr<Fact> headingFromGCS() const { return getFact("headingFromGCS"); }
    std::shared_ptr<Fact> distanceToGCS() const { return getFact("distanceToGCS"); }
    std::shared_ptr<Fact> hobbs() const { return getFact("hobbs"); }
    std::shared_ptr<Fact> throttlePct() const { return getFact("throttlePct"); }
    std::shared_ptr<Fact> imuTemp() const { return getFact("imuTemp"); }

    // Vehicle Fact Groups
    std::shared_ptr<FactGroup> gpsFactGroup() { return getFactGroup("gps"); }
    std::shared_ptr<FactGroup> gps2FactGroup() { return getFactGroup("gps2"); }
    std::shared_ptr<FactGroup> batteryFactGroup() { return getFactGroup("battery"); }
    std::shared_ptr<FactGroup> systemStatusFactGroup() { return getFactGroup("systemStatus"); }
    std::shared_ptr<FactGroup> rcFactGroup() { return getFactGroup("rc"); }
    std::shared_ptr<FactGroup> vibrationFactGroup() { return getFactGroup("vibration"); }
    std::shared_ptr<FactGroup> temperatureFactGroup() { return getFactGroup("temperature"); }
    std::shared_ptr<FactGroup> estimatorStatusFactGroup() { return getFactGroup("estimatorStatus"); }
    std::shared_ptr<FactGroup> windFactGroup() { return getFactGroup("wind"); }

    // Callback support for Qt-free implementation
    typedef std::function<void(const Vehicle*)> VehicleChangedCallback;
    void setVehicleChangedCallback(VehicleChangedCallback callback) { _vehicleChangedCallback = callback; }

    typedef std::function<void(const Vehicle*, std::string, std::string)> VehicleTextMessageCallback;
    void setVehicleTextMessageCallback(VehicleTextMessageCallback callback) { _vehicleTextMessageCallback = callback; }

    /// Handle incoming MAVLink message
    void handleMessage(const mavlink_message_t &message);

private:
    void _initializeFactGroups();
    void _handleHeartbeat(const mavlink_message_t &message);
    void _handleStatustext(const mavlink_message_t &message);
    void _handleCommandAck(const mavlink_message_t &message);
    void _handleAutopilotVersion(const mavlink_message_t &message);
    void _requestAutopilotVersion();
    void _requestTelemetryStreams();
    void _notifyVehicleChanged();

    // Vehicle identification
    uint8_t _systemId = 0;
    uint8_t _componentId = 0;
    uint8_t _vehicleType = 0;
    uint8_t _autopilotType = 0;
    uint8_t _baseMode = 0;
    uint32_t _customMode = 0;
    uint8_t _systemStatus = 0;
    uint8_t _mavlinkVersion = 0;

    // Autopilot version information
    uint64_t _capabilities = 0;
    uint64_t _uid = 0;
    uint32_t _flightSwVersion = 0;
    uint32_t _middlewareSwVersion = 0;
    uint32_t _osSwVersion = 0;
    uint32_t _boardVersion = 0;
    uint16_t _vendorId = 0;
    uint16_t _productId = 0;
    uint8_t _flightCustomVersion[8] = {0};

    // Communication
    MAVLinkUdpConnection* _connection = nullptr;

    // Managers
    std::shared_ptr<ParameterManager> _parameterManager;

    // Callbacks
    VehicleChangedCallback _vehicleChangedCallback;
    VehicleTextMessageCallback _vehicleTextMessageCallback;

    // Timing
    uint64_t _lastHeartbeatTime = 0;
    static constexpr uint64_t HEARTBEAT_TIMEOUT_MS = 5000; // 5 seconds
};
