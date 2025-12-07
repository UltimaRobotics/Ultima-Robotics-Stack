#ifndef FIREWALL_COLLECTOR_H
#define FIREWALL_COLLECTOR_H

#include "NetworkCollector.h"
#include <vector>

struct FirewallRule {
    std::string type;
    std::string table;
    std::string chain;
    int lineNumber;
    std::string target;
    std::string protocol;
    std::string source;
    std::string destination;
    std::string sourcePort;
    std::string destPort;
    std::string inputInterface;
    std::string outputInterface;
    std::string state;
    std::string additional;
    long packets;
    long bytes;
};

class FirewallCollector : public NetworkCollector {
public:
    FirewallCollector() = default;
    ~FirewallCollector() override = default;

    json collectDataJson() override;
    std::vector<NetworkData> collectData() override;
    std::string getCollectorName() const override { return "Firewall"; }

private:
    std::vector<FirewallRule> parseIptablesRules(const std::string& table = "filter");
    std::vector<FirewallRule> parseNftablesRules();
    std::vector<FirewallRule> parseUfwRules();
    std::vector<FirewallRule> parseFirewalldRules();
    std::string formatFirewallData(const std::vector<FirewallRule>& rules);
    json formatFirewallDataJson(const std::vector<FirewallRule>& rules);
};

#endif // FIREWALL_COLLECTOR_H
