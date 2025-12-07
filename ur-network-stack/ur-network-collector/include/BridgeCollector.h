#ifndef BRIDGE_COLLECTOR_H
#define BRIDGE_COLLECTOR_H

#include "NetworkCollector.h"
#include <vector>

struct BridgeInfo {
    std::string name;
    std::string id;
    std::string stpState;
    std::string stpForwardDelay;
    std::string stpHelloTime;
    std::string stpMaxAge;
    std::string ageingTime;
    std::string helloTimer;
    std::string tcnTimer;
    std::string topologyChangeTimer;
    std::string gcTimer;
    std::string interfaceCount;
    std::string status;
};

struct BridgePort {
    std::string bridgeName;
    std::string portName;
    std::string portId;
    std::string state;
    std::string priority;
    std::string pathCost;
    std::string designatedBridge;
    std::string designatedPort;
    std::string designatedRoot;
    std::string hairpinMode;
    std::string proxyarp;
    std::string proxyarpWiFi;
    std::string fastLeave;
    std::string learning;
    std::string flooding;
    std::string costMode;
    std::string mcastSnooping;
    std::string mcastQuerier;
    std::string mcastRouter;
    std::string mcastFastLeave;
    std::string mcastStartupQueryCount;
    std::string mcastStartupQueryInterval;
    std::string mcastQueryInterval;
    std::string mcastQueryResponseInterval;
    std::string mcastLastMemberCount;
    std::string mcastLastMemberInterval;
    std::string mcastMembershipInterval;
    std::string mcastQuerierInterval;
};

class BridgeCollector : public NetworkCollector {
public:
    BridgeCollector() = default;
    ~BridgeCollector() override = default;

    json collectDataJson() override;
    std::vector<NetworkData> collectData() override;
    std::string getCollectorName() const override { return "Bridges"; }

private:
    std::vector<BridgeInfo> parseBridgeShow();
    std::vector<BridgeInfo> parseIpLinkBridge();
    std::vector<BridgeInfo> parseProcNetBridge();
    std::vector<BridgePort> parseBridgePorts(const std::string& bridgeName);
    std::string formatBridgeData(const std::vector<BridgeInfo>& bridges, const std::vector<BridgePort>& ports);
    json formatBridgeDataJson(const std::vector<BridgeInfo>& bridges, const std::vector<BridgePort>& ports);
};

#endif // BRIDGE_COLLECTOR_H
