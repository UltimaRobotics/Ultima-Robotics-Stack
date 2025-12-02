#pragma once

#include "Fact.h"
#include "FactGroup.h"
#include <nlohmann/json.hpp>
#include "../thirdparty/c_library_v1/common/mavlink.h"
#include "../thirdparty/c_library_v2/common/mavlink.h"
#include <map>
#include <memory>
#include <cstdint>

// Forward declarations to avoid circular dependencies and redefinition conflicts
class Vehicle;

// Forward declare MAVLink message types to avoid including conflicting headers
struct __mavlink_battery_info_t;
struct __mavlink_smart_battery_info_t;
struct __mavlink_battery2_t;

/// Battery FactGroup containing battery telemetry data.
/// This is a Qt-free port of QGroundControl's BatteryFactGroup.
/// Enhanced with comprehensive MAVLink battery message support.
class VehicleBatteryFactGroup : public FactGroup
{
public:
    explicit VehicleBatteryFactGroup(bool ignoreCamelCase = false);
    virtual ~VehicleBatteryFactGroup() = default;

    // Basic battery facts
    std::shared_ptr<Fact> voltage() { return getFact("voltage"); }
    std::shared_ptr<Fact> current() { return getFact("current"); }
    std::shared_ptr<Fact> consumed() { return getFact("consumed"); }
    std::shared_ptr<Fact> remaining() { return getFact("remaining"); }
    std::shared_ptr<Fact> percent() { return getFact("percent"); }
    std::shared_ptr<Fact> temperature() { return getFact("temperature"); }
    std::shared_ptr<Fact> id() { return getFact("id"); }
    std::shared_ptr<Fact> function() { return getFact("function"); }
    std::shared_ptr<Fact> type() { return getFact("type"); }
    std::shared_ptr<Fact> timeRemaining() { return getFact("timeRemaining"); }
    std::shared_ptr<Fact> chargeState() { return getFact("chargeState"); }
    std::shared_ptr<Fact> mode() { return getFact("mode"); }
    std::shared_ptr<Fact> faultBitmask() { return getFact("faultBitmask"); }
    std::shared_ptr<Fact> cellCount() { return getFact("cellCount"); }
    
    // Enhanced battery info facts (from BATTERY_INFO)
    std::shared_ptr<Fact> dischargeMinVoltage() { return getFact("dischargeMinVoltage"); }
    std::shared_ptr<Fact> chargingMinVoltage() { return getFact("chargingMinVoltage"); }
    std::shared_ptr<Fact> restingMinVoltage() { return getFact("restingMinVoltage"); }
    std::shared_ptr<Fact> chargingMaxVoltage() { return getFact("chargingMaxVoltage"); }
    std::shared_ptr<Fact> chargingMaxCurrent() { return getFact("chargingMaxCurrent"); }
    std::shared_ptr<Fact> nominalVoltage() { return getFact("nominalVoltage"); }
    std::shared_ptr<Fact> dischargeMaxCurrent() { return getFact("dischargeMaxCurrent"); }
    std::shared_ptr<Fact> dischargeMaxBurstCurrent() { return getFact("dischargeMaxBurstCurrent"); }
    std::shared_ptr<Fact> designCapacity() { return getFact("designCapacity"); }
    std::shared_ptr<Fact> fullChargeCapacity() { return getFact("fullChargeCapacity"); }
    std::shared_ptr<Fact> cycleCount() { return getFact("cycleCount"); }
    std::shared_ptr<Fact> weight() { return getFact("weight"); }
    std::shared_ptr<Fact> stateOfHealth() { return getFact("stateOfHealth"); }
    std::shared_ptr<Fact> cellsInSeries() { return getFact("cellsInSeries"); }
    std::shared_ptr<Fact> manufactureDate() { return getFact("manufactureDate"); }
    std::shared_ptr<Fact> serialNumber() { return getFact("serialNumber"); }
    std::shared_ptr<Fact> batteryName() { return getFact("batteryName"); }
    
    // Smart battery info facts (from SMART_BATTERY_INFO)
    std::shared_ptr<Fact> capacityFullSpecification() { return getFact("capacityFullSpecification"); }
    std::shared_ptr<Fact> capacityFull() { return getFact("capacityFull"); }
    std::shared_ptr<Fact> smartSerialNumber() { return getFact("smartSerialNumber"); }
    std::shared_ptr<Fact> deviceName() { return getFact("deviceName"); }
    std::shared_ptr<Fact> smartManufactureDate() { return getFact("smartManufactureDate"); }
    
    // Cell voltage facts (up to 14 cells)
    std::shared_ptr<Fact> cellVoltage(uint8_t cellIndex);
    std::shared_ptr<Fact> cellCountDetected() { return getFact("cellCountDetected"); }
    
    // System-level battery facts
    std::shared_ptr<Fact> mavlinkVersion() { return getFact("mavlinkVersion"); }
    std::shared_ptr<Fact> batteryCount() { return getFact("batteryCount"); }
    std::shared_ptr<Fact> batterySystemType() { return getFact("batterySystemType"); }
    
    // Access specific battery data by ID
    std::shared_ptr<Fact> voltage(uint8_t batteryId);
    std::shared_ptr<Fact> current(uint8_t batteryId);
    std::shared_ptr<Fact> percent(uint8_t batteryId);

    void handleMessage(Vehicle *vehicle, const mavlink_message_t &message) override;

protected:
    // Comprehensive battery message handlers
    void _handleBatteryStatus(const mavlink_message_t &message);
    void _handleBatteryInfo(const mavlink_message_t &message);
    void _handleSmartBatteryInfo(const mavlink_message_t &message);
    void _handleBattery2(const mavlink_message_t &message);
    void _handleSysStatus(const mavlink_message_t &message);
    
    // Version-specific handlers
    void _handleBatteryStatusV1(const mavlink_message_t &message);
    void _handleBatteryStatusV2(const mavlink_message_t &message);
    void _handleBatteryStatusV2Dev(const mavlink_message_t &message);
    
    // Enhanced helper methods
    bool _detectMavlinkVersion(const mavlink_message_t &message);
    uint8_t _detectBatteryCount();
    void _configureForMavlinkVersion(uint8_t version);
    void _configureForBatteryCount(uint8_t count);
    
    // Battery count-specific handlers
    void _handleNoBattery();
    void _handleSingleBattery(const mavlink_message_t &message);
    void _handleDualBattery(const mavlink_message_t &message);
    
    // Cell voltage processing
    void _processCellVoltages(const uint16_t* voltages, uint8_t voltageCount, const uint16_t* voltagesExt, uint8_t extCount, bool hasMultipleCells = false);
    void _addCellVoltageFacts(uint8_t cellCount);
    void _storeCellVoltages(const nlohmann::json& cellVoltages);
    
    // Legacy helper methods
    bool _isV2BatteryStatus(const mavlink_message_t &message);
    void _updateBatteryFacts(uint8_t batteryId, float voltage, float current, 
                           float consumed, float remaining, uint8_t percent, 
                           float temperature, uint8_t function, uint8_t type,
                           uint32_t timeRemaining, uint8_t chargeState,
                           uint8_t mode, uint32_t faultBitmask, uint16_t cellCount);
    void _updateBatteryInfoFacts(uint8_t batteryId, const __mavlink_battery_info_t& info);
    void _updateSmartBatteryInfoFacts(uint8_t batteryId, const __mavlink_smart_battery_info_t& info);
    void _updateBattery2Facts(uint8_t batteryId, const __mavlink_battery2_t& battery2);
    void _addBatterySpecificFacts(uint8_t batteryId);
    
private:
    // Multi-battery support: map battery ID to fact group
    std::map<uint8_t, std::shared_ptr<FactGroup>> _batteryFactGroups;
    std::shared_ptr<FactGroup> _getOrCreateBatteryGroup(uint8_t batteryId);
    
    // Dynamic configuration state
    uint8_t _detectedMavlinkVersion = 0;  // 0=unknown, 1=v1, 2=v2, 3=v2dev
    uint8_t _detectedBatteryCount = 0;    // 0=unknown, 1=1 battery, 2=2 batteries, 255=no battery
    bool _configurationComplete = false;
    
    // Battery detection tracking
    std::map<uint8_t, bool> _detectedBatteryIds;
    uint32_t _lastDetectionTime = 0;
    static constexpr uint32_t DETECTION_TIMEOUT_MS = 5000; // 5 seconds timeout
    
    // Cell voltage tracking
    uint8_t _maxCellCount = 0;
    std::map<uint8_t, std::vector<std::shared_ptr<Fact>>> _cellVoltageFacts;
};
