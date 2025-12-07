#ifndef NAT_COLLECTOR_H
#define NAT_COLLECTOR_H

#include "NetworkCollector.h"
#include <vector>

struct NatRule {
    std::string type;
    std::string chain;
    std::string source;
    std::string destination;
    std::string protocol;
    std::string sourcePort;
    std::string destPort;
    std::string target;
    std::string additional;
};

class NatCollector : public NetworkCollector {
public:
    NatCollector() = default;
    ~NatCollector() override = default;

    json collectDataJson() override;
    std::vector<NetworkData> collectData() override;
    std::string getCollectorName() const override { return "NAT"; }

private:
    std::vector<NatRule> parseIptablesNat();
    std::vector<NatRule> parseNftablesNat();
    std::vector<NatRule> parseConntrack();
    std::string formatNatData(const std::vector<NatRule>& rules);
    json formatNatDataJson(const std::vector<NatRule>& rules);
};

#endif // NAT_COLLECTOR_H
