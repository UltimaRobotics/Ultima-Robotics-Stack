#ifndef BOARD_IDENTIFIER_H
#define BOARD_IDENTIFIER_H

#include <string>
#include <vector>
#include <map>

struct BoardInfo {
    uint16_t vendorID;
    uint16_t productID;
    std::string boardClass;
    std::string name;
    std::string comment;
    
    BoardInfo() = default;
    BoardInfo(uint16_t vid, uint16_t pid, const std::string& cls, const std::string& n, const std::string& c = "")
        : vendorID(vid), productID(pid), boardClass(cls), name(n), comment(c) {}
};

class BoardIdentifier {
public:
    static BoardIdentifier& instance();
    
    std::string identifyBoard(uint16_t vendorID, uint16_t productID) const;
    std::string getBoardClass(uint16_t vendorID, uint16_t productID) const;
    std::string getBoardName(uint16_t vendorID, uint16_t productID) const;

private:
    BoardIdentifier();
    void initializeBoardDatabase();
    
    std::vector<BoardInfo> _boardDatabase;
    std::map<std::pair<uint16_t, uint16_t>, BoardInfo> _boardMap;
};

#endif // BOARD_IDENTIFIER_H
