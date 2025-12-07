#include "RouteCollector.h"
#include <sstream>
#include <regex>
#include <fstream>
#include <iomanip>

json RouteCollector::collectDataJson() {
    json result;
    result["type"] = "routes";
    result["timestamp"] = getCurrentTimestamp();
    
    try {
        std::vector<RouteInfo> routes;
        
        auto ipRoutes = parseIpRoute();
        routes.insert(routes.end(), ipRoutes.begin(), ipRoutes.end());
        
        auto procRoutes = parseProcRoutes();
        routes.insert(routes.end(), procRoutes.begin(), procRoutes.end());
        
        if (CommandExecutor::commandExists("route")) {
            auto cmdRoutes = parseRouteCmd();
            routes.insert(routes.end(), cmdRoutes.begin(), cmdRoutes.end());
        }
        
        auto mainTableRoutes = parseIpRouteShowTable("main");
        routes.insert(routes.end(), mainTableRoutes.begin(), mainTableRoutes.end());
        
        result["data"] = formatRouteDataJson(routes);
        result["success"] = true;
        
    } catch (const std::exception& e) {
        result["success"] = false;
        result["error"] = "Error collecting route data: " + std::string(e.what());
        result["data"] = json::array();
    }
    
    return result;
}

std::vector<NetworkData> RouteCollector::collectData() {
    std::vector<NetworkData> results;
    
    try {
        std::vector<RouteInfo> routes;
        
        auto ipRoutes = parseIpRoute();
        routes.insert(routes.end(), ipRoutes.begin(), ipRoutes.end());
        
        auto procRoutes = parseProcRoutes();
        routes.insert(routes.end(), procRoutes.begin(), procRoutes.end());
        
        if (CommandExecutor::commandExists("route")) {
            auto cmdRoutes = parseRouteCmd();
            routes.insert(routes.end(), cmdRoutes.begin(), cmdRoutes.end());
        }
        
        auto mainTableRoutes = parseIpRouteShowTable("main");
        routes.insert(routes.end(), mainTableRoutes.begin(), mainTableRoutes.end());
        
        NetworkData data;
        data.type = "routes";
        data.data = formatRouteData(routes);
        data.timestamp = getCurrentTimestamp();
        results.push_back(data);
        
    } catch (const std::exception& e) {
        NetworkData errorData;
        errorData.type = "routes_error";
        errorData.data = "Error collecting route data: " + std::string(e.what());
        errorData.timestamp = getCurrentTimestamp();
        results.push_back(errorData);
    }
    
    return results;
}

std::vector<RouteInfo> RouteCollector::parseIpRoute() {
    std::vector<RouteInfo> routes;
    
    try {
        std::string output = CommandExecutor::executeCommand("ip route show");
        std::istringstream iss(output);
        std::string line;
        
        while (std::getline(iss, line)) {
            if (line.empty()) continue;
            
            RouteInfo route;
            route.protocol = "kernel";
            
            std::istringstream lineStream(line);
            std::string token;
            std::vector<std::string> tokens;
            
            while (lineStream >> token) {
                tokens.push_back(token);
            }
            
            for (size_t i = 0; i < tokens.size(); ++i) {
                if (tokens[i] == "via" && i + 1 < tokens.size()) {
                    route.gateway = tokens[i + 1];
                } else if (tokens[i] == "dev" && i + 1 < tokens.size()) {
                    route.interface = tokens[i + 1];
                } else if (tokens[i] == "metric" && i + 1 < tokens.size()) {
                    route.metric = tokens[i + 1];
                } else if (tokens[i] == "scope" && i + 1 < tokens.size()) {
                    route.scope = tokens[i + 1];
                } else if (tokens[i] == "proto" && i + 1 < tokens.size()) {
                    route.protocol = tokens[i + 1];
                } else if (tokens[i] == "src" && i + 1 < tokens.size()) {
                    // Source route, skip for now
                } else if (tokens[i] == "table" && i + 1 < tokens.size()) {
                    // Table specification, skip for now
                }
            }
            
            if (!tokens.empty()) {
                if (tokens[0].find('/') != std::string::npos) {
                    route.destination = tokens[0];
                    std::istringstream destStream(tokens[0]);
                    std::string ip;
                    if (std::getline(destStream, ip, '/')) {
                        route.netmask = tokens[0].substr(tokens[0].find('/') + 1);
                    }
                } else {
                    route.destination = tokens[0];
                    route.netmask = "32";
                }
            }
            
            if (route.gateway.empty()) {
                route.gateway = "0.0.0.0";
            }
            
            if (route.interface.empty()) {
                route.interface = "N/A";
            }
            
            if (route.metric.empty()) {
                route.metric = "0";
            }
            
            if (route.scope.empty()) {
                route.scope = "global";
            }
            
            if (route.protocol.empty()) {
                route.protocol = "kernel";
            }
            
            route.type = "unicast";
            routes.push_back(route);
        }
    } catch (...) {
        // ip route failed
    }
    
    return routes;
}

std::vector<RouteInfo> RouteCollector::parseRouteCmd() {
    std::vector<RouteInfo> routes;
    
    try {
        std::string output = CommandExecutor::executeCommand("route -n");
        std::istringstream iss(output);
        std::string line;
        
        while (std::getline(iss, line)) {
            if (line.find("Destination") != std::string::npos || line.empty()) {
                continue;
            }
            
            std::istringstream lineStream(line);
            RouteInfo route;
            
            lineStream >> route.destination >> route.gateway >> route.netmask;
            lineStream >> route.metric >> route.metric >> route.interface;
            
            route.protocol = "kernel";
            route.scope = "global";
            route.type = "unicast";
            
            routes.push_back(route);
        }
    } catch (...) {
        // route command failed
    }
    
    return routes;
}

std::vector<RouteInfo> RouteCollector::parseProcRoutes() {
    std::vector<RouteInfo> routes;
    
    std::ifstream procFile("/proc/net/route");
    if (!procFile.is_open()) {
        return routes;
    }
    
    std::string line;
    while (std::getline(procFile, line)) {
        if (line.find("Iface") == 0 || line.empty()) {
            continue;
        }
        
        std::istringstream iss(line);
        RouteInfo route;
        std::string destination, gateway, flags, refcnt, use, metric, mask, mtu, window, irtt;
        
        iss >> route.interface >> destination >> gateway >> flags >> refcnt >> use >> metric >> mask >> mtu >> window >> irtt;
        
        // Convert hex addresses to decimal
        if (!destination.empty() && destination != "00000000") {
            unsigned long dest;
            std::stringstream ss;
            ss << std::hex << destination;
            ss >> dest;
            route.destination = std::to_string(dest & 0xFF) + "." +
                               std::to_string((dest >> 8) & 0xFF) + "." +
                               std::to_string((dest >> 16) & 0xFF) + "." +
                               std::to_string((dest >> 24) & 0xFF);
        } else {
            route.destination = "0.0.0.0";
        }
        
        if (!gateway.empty() && gateway != "00000000") {
            unsigned long gw;
            std::stringstream ss;
            ss << std::hex << gateway;
            ss >> gw;
            route.gateway = std::to_string(gw & 0xFF) + "." +
                            std::to_string((gw >> 8) & 0xFF) + "." +
                            std::to_string((gw >> 16) & 0xFF) + "." +
                            std::to_string((gw >> 24) & 0xFF);
        } else {
            route.gateway = "0.0.0.0";
        }
        
        if (!mask.empty()) {
            unsigned long mask_val;
            std::stringstream ss;
            ss << std::hex << mask;
            ss >> mask_val;
            
            // Count bits in netmask
            int bits = 0;
            for (int i = 0; i < 32; i++) {
                if (mask_val & (1 << i)) bits++;
            }
            route.netmask = std::to_string(bits);
        } else {
            route.netmask = "0";
        }
        
        route.metric = metric;
        route.protocol = "kernel";
        route.scope = "global";
        route.type = "unicast";
        
        routes.push_back(route);
    }
    
    return routes;
}

std::vector<RouteInfo> RouteCollector::parseIpRouteShowTable(const std::string& table) {
    std::vector<RouteInfo> routes;
    
    try {
        std::string output = CommandExecutor::executeCommand("ip route show table " + table);
        std::istringstream iss(output);
        std::string line;
        
        while (std::getline(iss, line)) {
            if (line.empty()) continue;
            
            RouteInfo route;
            route.protocol = "kernel";
            route.type = "table_" + table;
            
            std::istringstream lineStream(line);
            std::string token;
            std::vector<std::string> tokens;
            
            while (lineStream >> token) {
                tokens.push_back(token);
            }
            
            for (size_t i = 0; i < tokens.size(); ++i) {
                if (tokens[i] == "via" && i + 1 < tokens.size()) {
                    route.gateway = tokens[i + 1];
                } else if (tokens[i] == "dev" && i + 1 < tokens.size()) {
                    route.interface = tokens[i + 1];
                } else if (tokens[i] == "metric" && i + 1 < tokens.size()) {
                    route.metric = tokens[i + 1];
                } else if (tokens[i] == "scope" && i + 1 < tokens.size()) {
                    route.scope = tokens[i + 1];
                }
            }
            
            if (!tokens.empty()) {
                if (tokens[0].find('/') != std::string::npos) {
                    route.destination = tokens[0];
                    route.netmask = tokens[0].substr(tokens[0].find('/') + 1);
                } else {
                    route.destination = tokens[0];
                    route.netmask = "32";
                }
            }
            
            if (route.gateway.empty()) route.gateway = "0.0.0.0";
            if (route.interface.empty()) route.interface = "N/A";
            if (route.metric.empty()) route.metric = "0";
            if (route.scope.empty()) route.scope = "global";
            
            routes.push_back(route);
        }
    } catch (...) {
        // ip route show table failed
    }
    
    return routes;
}

std::string RouteCollector::formatRouteData(const std::vector<RouteInfo>& routes) {
    std::ostringstream oss;
    oss << "Static Routes Configuration:\n";
    oss << "===========================\n";
    
    if (routes.empty()) {
        oss << "No routes configured or unable to access route information.\n";
        return oss.str();
    }
    
    oss << "Destination    | Gateway        | Netmask | Interface | Metric | Protocol | Scope  | Type\n";
    oss << "---------------|----------------|---------|-----------|--------|----------|--------|------\n";
    
    for (const auto& route : routes) {
        oss << std::setw(14) << route.destination << " | "
            << std::setw(14) << route.gateway << " | "
            << std::setw(7) << route.netmask << " | "
            << std::setw(9) << route.interface << " | "
            << std::setw(6) << route.metric << " | "
            << std::setw(8) << route.protocol << " | "
            << std::setw(6) << route.scope << " | "
            << std::setw(4) << route.type << "\n";
    }
    
    return oss.str();
}

json RouteCollector::formatRouteDataJson(const std::vector<RouteInfo>& routes) {
    json result = json::array();
    
    for (const auto& route : routes) {
        json routeObj;
        routeObj["destination"] = route.destination;
        routeObj["gateway"] = route.gateway;
        routeObj["netmask"] = route.netmask;
        routeObj["interface"] = route.interface;
        routeObj["metric"] = route.metric;
        routeObj["protocol"] = route.protocol;
        routeObj["scope"] = route.scope;
        routeObj["type"] = route.type;
        result.push_back(routeObj);
    }
    
    return result;
}
