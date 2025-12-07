#ifndef ROUTE_COLLECTOR_H
#define ROUTE_COLLECTOR_H

#include "NetworkCollector.h"
#include <vector>

struct RouteInfo {
    std::string destination;
    std::string gateway;
    std::string netmask;
    std::string interface;
    std::string metric;
    std::string protocol;
    std::string scope;
    std::string type;
};

class RouteCollector : public NetworkCollector {
public:
    RouteCollector() = default;
    ~RouteCollector() override = default;

    json collectDataJson() override;
    std::vector<NetworkData> collectData() override;
    std::string getCollectorName() const override { return "Routes"; }

private:
    std::vector<RouteInfo> parseIpRoute();
    std::vector<RouteInfo> parseRouteCmd();
    std::vector<RouteInfo> parseProcRoutes();
    std::vector<RouteInfo> parseIpRouteShowTable(const std::string& table);
    std::string formatRouteData(const std::vector<RouteInfo>& routes);
    json formatRouteDataJson(const std::vector<RouteInfo>& routes);
};

#endif // ROUTE_COLLECTOR_H
