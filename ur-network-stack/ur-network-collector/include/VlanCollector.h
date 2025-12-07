#ifndef VLAN_COLLECTOR_H
#define VLAN_COLLECTOR_H

#include "NetworkCollector.h"
#include <vector>

struct VlanInfo {
    int vlanId;
    std::string name;
    std::string interface;
    std::string status;
    std::string ipAddress;
    std::string netmask;
};

class VlanCollector : public NetworkCollector {
public:
    VlanCollector() = default;
    ~VlanCollector() override = default;

    json collectDataJson() override;
    std::vector<NetworkData> collectData() override;
    std::string getCollectorName() const override { return "VLAN"; }

private:
    std::vector<VlanInfo> parseVlanConfig();
    std::vector<VlanInfo> parseVlanFromProc();
    std::vector<VlanInfo> parseVlanFromIpLink();
    std::string formatVlanData(const std::vector<VlanInfo>& vlans);
    json formatVlanDataJson(const std::vector<VlanInfo>& vlans);
};

#endif // VLAN_COLLECTOR_H
