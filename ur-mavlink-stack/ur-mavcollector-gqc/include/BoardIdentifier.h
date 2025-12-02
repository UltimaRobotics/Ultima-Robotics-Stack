#pragma once

#include <string>
#include <vector>
#include <map>

/// Board information structure for flight controller identification
struct BoardInfo {
    uint16_t vendorID;
    uint16_t productID;
    std::string boardClass;
    std::string name;
    std::string comment;
    
    BoardInfo() : vendorID(0), productID(0), boardClass(""), name(""), comment("") {}
    
    BoardInfo(uint16_t vid, uint16_t pid, const std::string& cls, const std::string& n, const std::string& c = "")
        : vendorID(vid), productID(pid), boardClass(cls), name(n), comment(c) {}
};

/// Board identification system for flight controllers
class BoardIdentifier {
public:
    static BoardIdentifier& instance();
    
    /// Identify board by vendor and product ID
    std::string identifyBoard(uint16_t vendorID, uint16_t productID) const;
    
    /// Get board class by vendor and product ID
    std::string getBoardClass(uint16_t vendorID, uint16_t productID) const;
    
    /// Get board name by vendor and product ID
    std::string getBoardName(uint16_t vendorID, uint16_t productID) const;
    
private:
    BoardIdentifier();
    void initializeBoardDatabase();
    
    std::vector<BoardInfo> _boardDatabase;
    std::map<std::pair<uint16_t, uint16_t>, BoardInfo> _boardMap;
};
