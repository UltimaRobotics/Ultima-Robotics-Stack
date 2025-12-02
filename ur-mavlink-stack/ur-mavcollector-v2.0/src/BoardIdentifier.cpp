#include "BoardIdentifier.h"
#include <iostream>

BoardIdentifier& BoardIdentifier::instance() {
    static BoardIdentifier instance;
    return instance;
}

BoardIdentifier::BoardIdentifier() {
    initializeBoardDatabase();
}

void BoardIdentifier::initializeBoardDatabase() {
    // Pixhawk boards
    _boardDatabase.emplace_back(9900, 16, "Pixhawk", "PX4 FMU V1", "");
    _boardDatabase.emplace_back(9900, 17, "Pixhawk", "PX4 FMU V2", "");
    _boardDatabase.emplace_back(9900, 18, "Pixhawk", "PX4 FMU V4", "");
    _boardDatabase.emplace_back(9900, 19, "Pixhawk", "PX4 FMU V4 PRO", "");
    _boardDatabase.emplace_back(9900, 22, "Pixhawk", "PX4 FMU V2", "Bootloader on older Pixhawk V2 boards");
    _boardDatabase.emplace_back(9900, 4097, "Pixhawk", "AeroCore", "");
    _boardDatabase.emplace_back(9900, 33, "Pixhawk", "AUAV X2.1 FMU V2", "");
    _boardDatabase.emplace_back(9900, 48, "Pixhawk", "MindPX FMU V2", "");
    _boardDatabase.emplace_back(9900, 50, "Pixhawk", "PX4 FMU V5", "");
    _boardDatabase.emplace_back(12677, 51, "Pixhawk", "PX4 FMU V5X", "");
    _boardDatabase.emplace_back(7052, 54, "Pixhawk", "PX4 FMU V6U", "");
    _boardDatabase.emplace_back(12677, 53, "Pixhawk", "PX4 FMU V6X", "");
    _boardDatabase.emplace_back(12677, 56, "Pixhawk", "PX4 FMU V6C", "");
    _boardDatabase.emplace_back(13891, 29, "Pixhawk", "PX4 FMU V6X-RT", "");
    _boardDatabase.emplace_back(9900, 64, "Pixhawk", "TAP V1", "");
    _boardDatabase.emplace_back(9900, 65, "Pixhawk", "ASC V1", "");
    _boardDatabase.emplace_back(9900, 22, "Pixhawk", "Crazyflie 2", "");
    _boardDatabase.emplace_back(9900, 1, "Pixhawk", "Omnibus F4 SD", "");
    _boardDatabase.emplace_back(8137, 28, "Pixhawk", "PX4 FMUK66 v3.x", "");
    _boardDatabase.emplace_back(8137, 36, "Pixhawk", "Tropic-Community VMU", "");
    _boardDatabase.emplace_back(8137, 37, "Pixhawk", "MR-TROPIC", "");
    _boardDatabase.emplace_back(1155, 41775, "Pixhawk", "PX4 FMU ModalAI FCv1", "");
    _boardDatabase.emplace_back(1155, 41776, "Pixhawk", "PX4 FMU ModalAI FCv2", "");
    _boardDatabase.emplace_back(12642, 75, "Pixhawk", "PX4 DurandalV1", "");
    _boardDatabase.emplace_back(12642, 80, "Pixhawk", "Holybro Kakute Flight Controller", "");
    _boardDatabase.emplace_back(4104, 1, "Pixhawk", "PX4 FMU UVify Core", "");
    _boardDatabase.emplace_back(12643, 76, "Pixhawk", "CUAV Flight Controller", "");
    _boardDatabase.emplace_back(1155, 55, "Pixhawk", "PX4 FMU SmartAP AIRLink", "");
    _boardDatabase.emplace_back(12677, 57, "Pixhawk", "ARK FMU V6X", "");
    _boardDatabase.emplace_back(12677, 58, "Pixhawk", "ARK Pi6X", "");
    _boardDatabase.emplace_back(12677, 59, "Pixhawk", "ARK FPV", "");
    
    // ArduPilot boards
    _boardDatabase.emplace_back(1155, 22336, "Pixhawk", "ArduPilot ChibiOS", "");
    _boardDatabase.emplace_back(4617, 22336, "Pixhawk", "ArduPilot ChibiOS", "");
    _boardDatabase.emplace_back(4617, 22337, "Pixhawk", "ArduPilot ChibiOS", "");
    
    // Holybro boards
    _boardDatabase.emplace_back(12642, 0, "Pixhawk", "Holybro", "");
    
    // CubePilot boards
    _boardDatabase.emplace_back(11694, 0, "Pixhawk", "CubePilot", "");
    
    // Other boards
    _boardDatabase.emplace_back(2702, 110, "Pixhawk", "JFB JFB110", "");
    _boardDatabase.emplace_back(13735, 1, "Pixhawk", "ThePeach FCC-K1", "");
    _boardDatabase.emplace_back(13735, 2, "Pixhawk", "ThePeach FCC-R1", "");
    _boardDatabase.emplace_back(9900, 4119, "Pixhawk", "mRo Pixracer Pro", "");
    _boardDatabase.emplace_back(9900, 4130, "Pixhawk", "mRo Control Zero Classic", "");
    _boardDatabase.emplace_back(9900, 4131, "Pixhawk", "mRo Control Zero H7", "");
    _boardDatabase.emplace_back(9900, 4132, "Pixhawk", "mRo Control Zero H7 OEM", "");
    _boardDatabase.emplace_back(9900, 4388, "Pixhawk", "3DR Control Zero H7 OEM Rev G", "");
    _boardDatabase.emplace_back(2106, 7120, "Pixhawk", "PX4 Accton Godwit GA1", "");
    
    // SiK Radio boards
    _boardDatabase.emplace_back(1027, 24597, "SiK Radio", "SiK Radio", "3DR Radio");
    _boardDatabase.emplace_back(4292, 60000, "SiK Radio", "SiK Radio", "SILabs Radio");
    _boardDatabase.emplace_back(12346, 4097, "SiK Radio", "DroneBridge Radio", "ESP32-based telemetry radio");
    
    // RTK GPS boards
    _boardDatabase.emplace_back(5446, 424, "RTK GPS", "U-blox RTK GPS", "U-blox RTK GPS (M8P)");
    _boardDatabase.emplace_back(5446, 425, "RTK GPS", "U-blox RTK GPS", "U-blox RTK GPS (F9P)");
    _boardDatabase.emplace_back(1317, 42151, "RTK GPS", "Trimble RTK GPS", "");
    _boardDatabase.emplace_back(5418, 34240, "RTK GPS", "Septentrio RTK GPS", "");
    
    // OpenPilot boards
    _boardDatabase.emplace_back(8352, 16732, "OpenPilot", "OpenPilot OPLink", "");
    _boardDatabase.emplace_back(8352, 16733, "OpenPilot", "OpenPilot CC3D", "");
    _boardDatabase.emplace_back(8352, 16734, "OpenPilot", "OpenPilot Revolution", "");
    _boardDatabase.emplace_back(8352, 16848, "OpenPilot", "Taulabs Sparky2", "");
    
    // Additional boards
    _boardDatabase.emplace_back(13891, 5600, "Pixhawk", "ZeroOne X6", "");
    _boardDatabase.emplace_back(8355, 16888, "Pixhawk", "Svehicle e2", "");
    
    // Build the lookup map for efficient searching
    for (const auto& board : _boardDatabase) {
        _boardMap[std::make_pair(board.vendorID, board.productID)] = board;
    }
}

std::string BoardIdentifier::identifyBoard(uint16_t vendorID, uint16_t productID) const {
    auto it = _boardMap.find(std::make_pair(vendorID, productID));
    if (it != _boardMap.end()) {
        const BoardInfo& info = it->second;
        if (!info.comment.empty()) {
            return info.name + " (" + info.comment + ")";
        }
        return info.name;
    }
    
    // Fallback to generic identification
    return "Unknown Board (VID: " + std::to_string(vendorID) + ", PID: " + std::to_string(productID) + ")";
}

std::string BoardIdentifier::getBoardClass(uint16_t vendorID, uint16_t productID) const {
    auto it = _boardMap.find(std::make_pair(vendorID, productID));
    if (it != _boardMap.end()) {
        return it->second.boardClass;
    }
    return "Unknown";
}

std::string BoardIdentifier::getBoardName(uint16_t vendorID, uint16_t productID) const {
    auto it = _boardMap.find(std::make_pair(vendorID, productID));
    if (it != _boardMap.end()) {
        return it->second.name;
    }
    return "Unknown Board";
}
