#include "VehicleBatteryFactGroup.h"
#include "Vehicle.h"
#include "Fact.h"
#include "FactGroup.h"

// Include only the specific MAVLink headers needed to avoid conflicts
#include "../thirdparty/c_library_v2/common/mavlink.h"
#include "../thirdparty/c_library_v2/common/mavlink_msg_battery_status.h"

#include "../thirdparty/c_library_v2/common/mavlink_msg_battery_info.h"
#include "../thirdparty/c_library_v2/common/mavlink_msg_smart_battery_info.h"
#include "../thirdparty/c_library_v2/ardupilotmega/mavlink_msg_battery2.h"

#include <cstring>     // For memcpy in packed structure handling
#include <cstdint>     // For standard integer types
#include <memory>      // For std::shared_ptr used in fact creation and battery groups
#include <iostream>    // For debug output logging (std::cout)
#include <string>      // For string operations in battery info fields

VehicleBatteryFactGroup::VehicleBatteryFactGroup(bool ignoreCamelCase)
    : FactGroup(500, ignoreCamelCase) // Update every 500ms
{
    // Add all basic battery facts for primary battery
    _addFact(std::make_shared<Fact>(0, "voltage", FactMetaData::valueTypeFloat));
    _addFact(std::make_shared<Fact>(0, "current", FactMetaData::valueTypeFloat));
    _addFact(std::make_shared<Fact>(0, "consumed", FactMetaData::valueTypeFloat));
    _addFact(std::make_shared<Fact>(0, "remaining", FactMetaData::valueTypeFloat));
    _addFact(std::make_shared<Fact>(0, "percent", FactMetaData::valueTypeUint8));
    _addFact(std::make_shared<Fact>(0, "temperature", FactMetaData::valueTypeFloat));
    _addFact(std::make_shared<Fact>(0, "id", FactMetaData::valueTypeUint8));
    _addFact(std::make_shared<Fact>(0, "function", FactMetaData::valueTypeUint8));
    _addFact(std::make_shared<Fact>(0, "type", FactMetaData::valueTypeUint8));
    _addFact(std::make_shared<Fact>(0, "timeRemaining", FactMetaData::valueTypeUint32));
    _addFact(std::make_shared<Fact>(0, "chargeState", FactMetaData::valueTypeUint8));
    _addFact(std::make_shared<Fact>(0, "mode", FactMetaData::valueTypeUint8));
    _addFact(std::make_shared<Fact>(0, "faultBitmask", FactMetaData::valueTypeUint32));
    _addFact(std::make_shared<Fact>(0, "cellCount", FactMetaData::valueTypeUint16));
    
    // Add enhanced battery info facts (from BATTERY_INFO)
    _addFact(std::make_shared<Fact>(0, "dischargeMinVoltage", FactMetaData::valueTypeFloat));
    _addFact(std::make_shared<Fact>(0, "chargingMinVoltage", FactMetaData::valueTypeFloat));
    _addFact(std::make_shared<Fact>(0, "restingMinVoltage", FactMetaData::valueTypeFloat));
    _addFact(std::make_shared<Fact>(0, "chargingMaxVoltage", FactMetaData::valueTypeFloat));
    _addFact(std::make_shared<Fact>(0, "chargingMaxCurrent", FactMetaData::valueTypeFloat));
    _addFact(std::make_shared<Fact>(0, "nominalVoltage", FactMetaData::valueTypeFloat));
    _addFact(std::make_shared<Fact>(0, "dischargeMaxCurrent", FactMetaData::valueTypeFloat));
    _addFact(std::make_shared<Fact>(0, "dischargeMaxBurstCurrent", FactMetaData::valueTypeFloat));
    _addFact(std::make_shared<Fact>(0, "designCapacity", FactMetaData::valueTypeFloat));
    _addFact(std::make_shared<Fact>(0, "fullChargeCapacity", FactMetaData::valueTypeFloat));
    _addFact(std::make_shared<Fact>(0, "cycleCount", FactMetaData::valueTypeUint16));
    _addFact(std::make_shared<Fact>(0, "weight", FactMetaData::valueTypeUint16));
    _addFact(std::make_shared<Fact>(0, "stateOfHealth", FactMetaData::valueTypeUint8));
    _addFact(std::make_shared<Fact>(0, "cellsInSeries", FactMetaData::valueTypeUint8));
    _addFact(std::make_shared<Fact>(0, "manufactureDate", FactMetaData::valueTypeString));
    _addFact(std::make_shared<Fact>(0, "serialNumber", FactMetaData::valueTypeString));
    _addFact(std::make_shared<Fact>(0, "batteryName", FactMetaData::valueTypeString));
    
    // Add smart battery info facts (from SMART_BATTERY_INFO)
    _addFact(std::make_shared<Fact>(0, "capacityFullSpecification", FactMetaData::valueTypeInt32));
    _addFact(std::make_shared<Fact>(0, "capacityFull", FactMetaData::valueTypeInt32));
    _addFact(std::make_shared<Fact>(0, "smartSerialNumber", FactMetaData::valueTypeString));
    _addFact(std::make_shared<Fact>(0, "deviceName", FactMetaData::valueTypeString));
    _addFact(std::make_shared<Fact>(0, "smartManufactureDate", FactMetaData::valueTypeString));
    
    // Add cell voltage tracking facts
    _addFact(std::make_shared<Fact>(0, "cellCountDetected", FactMetaData::valueTypeUint8));
    
    // Add system-level battery facts for dynamic detection
    _addFact(std::make_shared<Fact>(0, "mavlinkVersion", FactMetaData::valueTypeUint8));
    _addFact(std::make_shared<Fact>(0, "batteryCount", FactMetaData::valueTypeUint8));
    _addFact(std::make_shared<Fact>(0, "batterySystemType", FactMetaData::valueTypeUint8));
    
    // Initialize system facts to unknown state (no fallback values)
    // Only set values when actual data is received from MAVLink messages
    // mavlinkVersion()->setRawValue(static_cast<uint8_t>(0));
    // batteryCount()->setRawValue(static_cast<uint8_t>(0));
    // batterySystemType()->setRawValue(static_cast<uint8_t>(0));
    // cellCountDetected()->setRawValue(static_cast<uint8_t>(0));
}

void VehicleBatteryFactGroup::handleMessage([[maybe_unused]] Vehicle *vehicle, const mavlink_message_t &message)
{
    // Comprehensive battery message handling
    switch (message.msgid) {
        // Standard battery status messages
        case MAVLINK_MSG_ID_BATTERY_STATUS:
            _handleBatteryStatus(message);
            break;
            
        // Battery info messages
        case MAVLINK_MSG_ID_BATTERY_INFO:
            _handleBatteryInfo(message);
            break;
            
        // Smart battery info messages
        case MAVLINK_MSG_ID_SMART_BATTERY_INFO:
            _handleSmartBatteryInfo(message);
            break;
            
        // ArduPilot battery2 messages
        case MAVLINK_MSG_ID_BATTERY2:
            _handleBattery2(message);
            break;
            
        // System status (for basic battery info fallback)
        case MAVLINK_MSG_ID_SYS_STATUS:
            _handleSysStatus(message);
            break;
            
        default:
            // Not a battery message, ignore
            break;
    }
}

// Enhanced battery status handler using QGC's proven approach
void VehicleBatteryFactGroup::_handleBatteryStatus(const mavlink_message_t &message)
{
    // Use standard MAVLink decode function
    mavlink_battery_status_t batteryStatus;
    mavlink_msg_battery_status_decode(&message, &batteryStatus);
    
    // QGC Approach: Simple voltage processing - sum all valid cells
    double totalVoltage = NAN;
    uint8_t validCellCount = 0;
    
    // Process main voltages (cells 1-10) - QGC style
    for (int i = 0; i < 10; i++) {
        if (batteryStatus.voltages[i] != UINT16_MAX) {
            double cellVoltage = static_cast<double>(batteryStatus.voltages[i]) / 1000.0;
            if (std::isnan(totalVoltage)) {
                totalVoltage = cellVoltage;  // First valid cell
            } else {
                totalVoltage += cellVoltage;  // Sum subsequent cells
            }
            validCellCount++;
        } else {
            break;  // Stop at first invalid cell (QGC approach)
        }
    }
    
    // Process extended voltages (cells 11-14) - QGC style
    for (int i = 0; i < 4; i++) {
        if (batteryStatus.voltages_ext[i] != 0) {
            double cellVoltage = static_cast<double>(batteryStatus.voltages_ext[i]) / 1000.0;
            if (std::isnan(totalVoltage)) {
                totalVoltage = cellVoltage;
            } else {
                totalVoltage += cellVoltage;
            }
            validCellCount++;
        } else {
            break;
        }
    }
    
    // Extract current using QGC's NaN approach
    double batteryCurrent = NAN;
    if (batteryStatus.current_battery != -1) {
        batteryCurrent = static_cast<double>(batteryStatus.current_battery) / 100.0;
    }
    
    // Extract consumed charge
    double consumed = NAN;
    if (batteryStatus.current_consumed != -1) {
        consumed = static_cast<double>(batteryStatus.current_consumed);
    }
    
    // Extract remaining percentage
    double remaining = NAN;
    uint8_t percent = 0;
    if (batteryStatus.battery_remaining != -1) {
        percent = static_cast<uint8_t>(batteryStatus.battery_remaining);
        remaining = static_cast<double>(batteryStatus.battery_remaining);
    }
    
    // Extract temperature using QGC's NaN approach
    double temperature = NAN;
    if (batteryStatus.temperature != INT16_MAX) {
        temperature = static_cast<double>(batteryStatus.temperature) / 100.0;
    }
    
    // Extract additional MAVLink v2 fields
    uint32_t timeRemaining = batteryStatus.time_remaining;
    uint8_t chargeState = batteryStatus.charge_state;
    uint8_t mode = batteryStatus.mode;
    uint32_t faultBitmask = batteryStatus.fault_bitmask;
    
    // Count actual cells for cell count detection
    uint8_t detectedCellCount = validCellCount;
    
    // Update all battery facts with QGC-style data
    _updateBatteryFacts(batteryStatus.id, static_cast<float>(totalVoltage), static_cast<float>(batteryCurrent), 
                       static_cast<float>(consumed), static_cast<float>(remaining), percent,
                       static_cast<float>(temperature), batteryStatus.battery_function, batteryStatus.type,
                       timeRemaining, chargeState, mode, faultBitmask, detectedCellCount);
    
    // Update detected cell count only if we have valid data
    if (detectedCellCount > 0) {
        cellCountDetected()->setRawValue(detectedCellCount);
    }
    
    // Set battery count only if we have valid battery data
    if (!std::isnan(totalVoltage) && totalVoltage > 0.0) {
        batteryCount()->setRawValue(static_cast<uint8_t>(1));
    }
    
    // Store cell voltages for JSON output using QGC approach
    nlohmann::json cellVoltages = nlohmann::json::array();
    
    // Only add individual cell voltages if they are actually available
    if (validCellCount > 0) {
        // Add individual cell voltages if available
        for (int i = 0; i < 10; i++) {
            if (batteryStatus.voltages[i] != UINT16_MAX) {
                cellVoltages.push_back(static_cast<double>(batteryStatus.voltages[i]) / 1000.0);
            } else {
                break;  // Stop at first invalid cell
            }
        }
        
        // Add extended cell voltages if available
        for (int i = 0; i < 4; i++) {
            if (batteryStatus.voltages_ext[i] != 0) {
                cellVoltages.push_back(static_cast<double>(batteryStatus.voltages_ext[i]) / 1000.0);
            } else {
                break;
            }
        }
    }
    
    _storeCellVoltages(cellVoltages);
}

// Access cell voltage by index
std::shared_ptr<Fact> VehicleBatteryFactGroup::cellVoltage(uint8_t cellIndex)
{
    std::string factName = "cell" + std::to_string(cellIndex + 1) + "Voltage";
    return getFact(factName);
}

// Store cell voltages for JSON output
void VehicleBatteryFactGroup::_storeCellVoltages(const nlohmann::json& cellVoltages)
{
    // Store cell voltages in individual facts for JSON output
    for (size_t i = 0; i < cellVoltages.size(); i++) {
        std::string factName = "cell" + std::to_string(i + 1) + "Voltage";
        
        // Add fact if it doesn't exist
        if (!getFact(factName)) {
            _addCellVoltageFacts(static_cast<uint8_t>(i + 1));
        }
        
        // Set the value
        auto fact = getFact(factName);
        if (fact && cellVoltages[i].is_number()) {
            fact->setRawValue(cellVoltages[i].get<float>());
        }
    }
}

// Battery info handler using proper MAVLink decode
void VehicleBatteryFactGroup::_handleBatteryInfo(const mavlink_message_t &message)
{
    // Use standard MAVLink decode function
    mavlink_battery_info_t batteryInfo;
    mavlink_msg_battery_info_decode(&message, &batteryInfo);
       
    // Update cell count from BATTERY_INFO if available
    if (batteryInfo.cells_in_series != 0) {
        cellCountDetected()->setRawValue(batteryInfo.cells_in_series);
        cellCount()->setRawValue(batteryInfo.cells_in_series);
    }
    
    // Update battery info facts using MAVLink data
    _updateBatteryInfoFacts(batteryInfo.id, batteryInfo);
}

// Smart battery info handler for smart battery data
void VehicleBatteryFactGroup::_handleSmartBatteryInfo(const mavlink_message_t &message)
{
    mavlink_smart_battery_info_t smartBatteryInfo;
    mavlink_msg_smart_battery_info_decode(&message, &smartBatteryInfo);
        
    // Update smart battery info facts
    _updateSmartBatteryInfoFacts(smartBatteryInfo.id, smartBatteryInfo);
}

// Battery2 handler for ArduPilot battery2 messages
void VehicleBatteryFactGroup::_handleBattery2(const mavlink_message_t &message)
{
    mavlink_battery2_t battery2;
    mavlink_msg_battery2_decode(&message, &battery2);
        
    // Update battery2 facts (for battery ID 1 typically)
    _updateBattery2Facts(1, battery2);
}

// Process cell voltages from battery status messages
void VehicleBatteryFactGroup::_processCellVoltages(const uint16_t* voltages, uint8_t voltageCount, 
                                                  const uint16_t* voltagesExt, uint8_t extCount, bool hasMultipleCells)
{
    uint8_t totalCells = 0;
    
    if (hasMultipleCells) {
        // We have individual cell voltages
        for (uint8_t i = 0; i < voltageCount; i++) {
            if (voltages[i] != UINT16_MAX && voltages[i] != 0) {
                totalCells++;
            }
        }
        
        // Update max cell count and add facts if needed
        if (totalCells > _maxCellCount) {
            _maxCellCount = totalCells;
            _addCellVoltageFacts(totalCells);
        }
        
        // Update individual cell voltages
        for (uint8_t i = 0; i < voltageCount && i < _maxCellCount; i++) {
            if (voltages[i] != UINT16_MAX && voltages[i] != 0) {
                std::string factName = "cell" + std::to_string(i + 1) + "Voltage";
                auto fact = getFact(factName);
                if (fact) {
                    fact->setRawValue(static_cast<float>(voltages[i]) / 1000.0f);
                }
            }
        }
    } else {
        // We only have total battery voltage, no individual cell voltages
        // Set cell count to 1 and voltage as total battery voltage
        totalCells = 1;
        
        if (totalCells > _maxCellCount) {
            _maxCellCount = totalCells;
            _addCellVoltageFacts(totalCells);
        }
        
        // Store total voltage as "cell1" for compatibility
        if (voltages[0] != UINT16_MAX && voltages[0] != 0) {
            auto fact = getFact("cell1Voltage");
            if (fact) {
                fact->setRawValue(static_cast<float>(voltages[0]) / 1000.0f);
            }
        }
    }
    
    // Update detected cell count
    cellCountDetected()->setRawValue(totalCells);
}

// Add cell voltage facts for the specified number of cells
void VehicleBatteryFactGroup::_addCellVoltageFacts(uint8_t cellCount)
{
    for (uint8_t i = 0; i < cellCount; i++) {
        std::string factName = "cell" + std::to_string(i + 1) + "Voltage";
        
        // Only add if it doesn't already exist
        if (!getFact(factName)) {
            _addFact(std::make_shared<Fact>(0, factName, FactMetaData::valueTypeFloat));
        }
    }
}

// Update battery info facts from BATTERY_INFO message
void VehicleBatteryFactGroup::_updateBatteryInfoFacts(uint8_t batteryId, const __mavlink_battery_info_t& info)
{
    // Create a typed copy for easier access
    mavlink_battery_info_t batteryInfo;
    std::memcpy(&batteryInfo, &info, sizeof(batteryInfo));
    
    // Update basic info facts
    dischargeMinVoltage()->setRawValue(batteryInfo.discharge_minimum_voltage);
    chargingMinVoltage()->setRawValue(batteryInfo.charging_minimum_voltage);
    restingMinVoltage()->setRawValue(batteryInfo.resting_minimum_voltage);
    chargingMaxVoltage()->setRawValue(batteryInfo.charging_maximum_voltage);
    chargingMaxCurrent()->setRawValue(batteryInfo.charging_maximum_current);
    nominalVoltage()->setRawValue(batteryInfo.nominal_voltage);
    dischargeMaxCurrent()->setRawValue(batteryInfo.discharge_maximum_current);
    dischargeMaxBurstCurrent()->setRawValue(batteryInfo.discharge_maximum_burst_current);
    designCapacity()->setRawValue(batteryInfo.design_capacity);
    fullChargeCapacity()->setRawValue(batteryInfo.full_charge_capacity);
    cycleCount()->setRawValue(batteryInfo.cycle_count);
    weight()->setRawValue(batteryInfo.weight);
    stateOfHealth()->setRawValue(batteryInfo.state_of_health);
    cellsInSeries()->setRawValue(batteryInfo.cells_in_series);
    
    // Update string fields
    manufactureDate()->setRawValue(std::string(batteryInfo.manufacture_date));
    serialNumber()->setRawValue(std::string(batteryInfo.serial_number));
    batteryName()->setRawValue(std::string(batteryInfo.name));
    
    // Update battery ID
    id()->setRawValue(batteryId);
}

// Update smart battery info facts from SMART_BATTERY_INFO message
void VehicleBatteryFactGroup::_updateSmartBatteryInfoFacts(uint8_t batteryId, const __mavlink_smart_battery_info_t& info)
{
    // Create a typed copy for easier access (avoid packed field issues)
    mavlink_smart_battery_info_t smartBatteryInfo;
    std::memcpy(&smartBatteryInfo, &info, sizeof(smartBatteryInfo));
    
    // Update smart battery specific facts (use local copies to avoid packed field issues)
    int32_t capacityFullSpec = smartBatteryInfo.capacity_full_specification;
    int32_t capacityFullVal = smartBatteryInfo.capacity_full;
    uint16_t cycleCount = smartBatteryInfo.cycle_count;
    uint16_t weight = smartBatteryInfo.weight;
    
    capacityFullSpecification()->setRawValue(capacityFullSpec);
    capacityFull()->setRawValue(capacityFullVal);
    
    // Update string fields
    smartSerialNumber()->setRawValue(std::string(smartBatteryInfo.serial_number));
    deviceName()->setRawValue(std::string(smartBatteryInfo.device_name));
    smartManufactureDate()->setRawValue(std::string(smartBatteryInfo.manufacture_date));
    
    // Update additional info
    this->cycleCount()->setRawValue(cycleCount);
    this->weight()->setRawValue(weight);
    
    // Update battery ID
    id()->setRawValue(batteryId);
}

// Update battery2 facts from BATTERY2 message
void VehicleBatteryFactGroup::_updateBattery2Facts(uint8_t batteryId, const __mavlink_battery2_t& battery2)
{
    // Create a typed copy for easier access (avoid packed field issues)
    mavlink_battery2_t battery2Typed;
    std::memcpy(&battery2Typed, &battery2, sizeof(battery2Typed));
    
    // Update voltage and current
    float batteryVoltage = static_cast<float>(battery2Typed.voltage) / 1000.0f;
    float batteryCurrent = (battery2Typed.current_battery != -1) ? 
                   static_cast<float>(battery2Typed.current_battery) / 100.0f : NAN;
    
    voltage()->setRawValue(batteryVoltage);
    current()->setRawValue(batteryCurrent);
    
    // Update battery ID
    id()->setRawValue(batteryId);
}

// System status handler for basic battery fallback
void VehicleBatteryFactGroup::_handleSysStatus(const mavlink_message_t &message)
{
    mavlink_sys_status_t sysStatus;
    mavlink_msg_sys_status_decode(&message, &sysStatus);
    
    // Update basic battery facts from SYS_STATUS
    voltage()->setRawValue(static_cast<float>(sysStatus.voltage_battery / 1000.0f));
    current()->setRawValue(static_cast<float>(sysStatus.current_battery / 100.0f));
    percent()->setRawValue(static_cast<uint8_t>(sysStatus.battery_remaining));
    
    _setTelemetryAvailable(true);
}

// Helper method to update battery facts for any battery ID
void VehicleBatteryFactGroup::_updateBatteryFacts(uint8_t batteryId, float voltageVal, float currentVal, 
                                                 float consumedVal, float remainingVal, uint8_t percentVal, 
                                                 float temperatureVal, uint8_t functionVal, uint8_t typeVal,
                                                 uint32_t timeRemainingVal, uint8_t chargeStateVal,
                                                 uint8_t modeVal, uint32_t faultBitmaskVal, uint16_t cellCountVal)
{
    // Update primary battery facts
    voltage()->setRawValue(voltageVal);
    current()->setRawValue(currentVal);
    consumed()->setRawValue(consumedVal);
    remaining()->setRawValue(remainingVal);
    percent()->setRawValue(percentVal);
    temperature()->setRawValue(temperatureVal);
    id()->setRawValue(batteryId);
    function()->setRawValue(functionVal);
    type()->setRawValue(typeVal);
    timeRemaining()->setRawValue(timeRemainingVal);
    chargeState()->setRawValue(chargeStateVal);
    mode()->setRawValue(modeVal);
    faultBitmask()->setRawValue(faultBitmaskVal);
    cellCount()->setRawValue(cellCountVal);
    
    _setTelemetryAvailable(true);
}

// Get or create fact group for specific battery ID
std::shared_ptr<FactGroup> VehicleBatteryFactGroup::_getOrCreateBatteryGroup(uint8_t batteryId)
{
    if (_batteryFactGroups.find(batteryId) == _batteryFactGroups.end()) {
        auto newGroup = std::make_shared<FactGroup>(500);
        _addBatterySpecificFacts(batteryId);
        _batteryFactGroups[batteryId] = newGroup;
    }
    return _batteryFactGroups[batteryId];
}

// Add battery-specific facts for multi-battery support
void VehicleBatteryFactGroup::_addBatterySpecificFacts([[maybe_unused]] uint8_t batteryId)
{
    // Implementation for multi-battery support
    // For now, we rely on the primary facts
}

// Public methods to access specific battery data
std::shared_ptr<Fact> VehicleBatteryFactGroup::voltage(uint8_t batteryId)
{
    if (batteryId == 0) {
        return voltage();
    }
    auto group = _getOrCreateBatteryGroup(batteryId);
    return group ? group->getFact("voltage") : nullptr;
}

std::shared_ptr<Fact> VehicleBatteryFactGroup::current(uint8_t batteryId)
{
    if (batteryId == 0) {
        return current();
    }
    auto group = _getOrCreateBatteryGroup(batteryId);
    return group ? group->getFact("current") : nullptr;
}

std::shared_ptr<Fact> VehicleBatteryFactGroup::percent(uint8_t batteryId)
{
    if (batteryId == 0) {
        return percent();
    }
    auto group = _getOrCreateBatteryGroup(batteryId);
    return group ? group->getFact("percent") : nullptr;
}
