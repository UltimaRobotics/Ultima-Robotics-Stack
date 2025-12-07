#ifndef NETWORK_COLLECTOR_H
#define NETWORK_COLLECTOR_H

#include <string>
#include <vector>
#include <memory>
#include <stdexcept>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

struct NetworkData {
    std::string type;
    std::string data;
    std::string timestamp;
};

class CommandExecutor {
public:
    static std::string executeCommand(const std::string& command);
    static std::vector<std::string> executeCommandLines(const std::string& command);
    static bool commandExists(const std::string& command);
};

class NetworkCollector {
protected:
    CommandExecutor executor;

public:
    NetworkCollector() = default;
    virtual ~NetworkCollector() = default;

    virtual json collectDataJson() = 0;
    virtual std::vector<NetworkData> collectData() = 0;
    virtual std::string getCollectorName() const = 0;
    
    std::string getCurrentTimestamp() const;
    bool hasRootPrivileges() const;
};

#endif // NETWORK_COLLECTOR_H
