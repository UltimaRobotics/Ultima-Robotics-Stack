#include <iostream>
#include <memory>
#include <vector>
#include <string>
#include <fstream>
#include <chrono>
#include <getopt.h>
#include <iomanip>

#include "NetworkCollector.h"
#include "VlanCollector.h"
#include "NatCollector.h"
#include "FirewallCollector.h"
#include "RouteCollector.h"
#include "BridgeCollector.h"

void printUsage(const char* programName) {
    std::cout << "Network Data Collector Utility\n";
    std::cout << "Usage: " << programName << " [options]\n\n";
    std::cout << "Options:\n";
    std::cout << "  -h, --help          Show this help message\n";
    std::cout << "  -v, --vlan          Collect VLAN information\n";
    std::cout << "  -n, --nat           Collect NAT rules\n";
    std::cout << "  -f, --firewall      Collect firewall rules\n";
    std::cout << "  -r, --routes        Collect static routes\n";
    std::cout << "  -b, --bridges       Collect bridge information\n";
    std::cout << "  -a, --all           Collect all network data (default)\n";
    std::cout << "  -t, --text          Output in text format (default is JSON)\n";
    std::cout << "  -o, --output FILE   Save output to file\n";
    std::cout << "  -q, --quiet         Minimal output (errors only)\n";
    std::cout << "\nExamples:\n";
    std::cout << "  " << programName << " --all                    # Collect all data in JSON format\n";
    std::cout << "  " << programName << " --vlan --nat            # Collect VLAN and NAT data\n";
    std::cout << "  " << programName << " --text --output net.txt # Collect all data to text file\n";
    std::cout << "\nNote: Some operations may require root privileges.\n";
}

void printSeparator() {
    std::cout << std::string(80, '=') << std::endl;
}

std::string getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    ss << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return ss.str();
}

int main(int argc, char* argv[]) {
    bool collectVlan = false;
    bool collectNat = false;
    bool collectFirewall = false;
    bool collectRoutes = false;
    bool collectBridges = false;
    bool collectAll = true;
    bool outputText = false;
    bool quietMode = false;
    std::string outputFile;
    
    static struct option long_options[] = {
        {"help", no_argument, 0, 'h'},
        {"vlan", no_argument, 0, 'v'},
        {"nat", no_argument, 0, 'n'},
        {"firewall", no_argument, 0, 'f'},
        {"routes", no_argument, 0, 'r'},
        {"bridges", no_argument, 0, 'b'},
        {"all", no_argument, 0, 'a'},
        {"text", no_argument, 0, 't'},
        {"output", required_argument, 0, 'o'},
        {"quiet", no_argument, 0, 'q'},
        {0, 0, 0, 0}
    };
    
    int option_index = 0;
    int c;
    
    while ((c = getopt_long(argc, argv, "hvnfrbato:q", long_options, &option_index)) != -1) {
        switch (c) {
            case 'h':
                printUsage(argv[0]);
                return 0;
            case 'v':
                collectVlan = true;
                collectAll = false;
                break;
            case 'n':
                collectNat = true;
                collectAll = false;
                break;
            case 'f':
                collectFirewall = true;
                collectAll = false;
                break;
            case 'r':
                collectRoutes = true;
                collectAll = false;
                break;
            case 'b':
                collectBridges = true;
                collectAll = false;
                break;
            case 'a':
                collectAll = true;
                break;
            case 't':
                outputText = true;
                break;
            case 'o':
                outputFile = optarg;
                break;
            case 'q':
                quietMode = true;
                break;
            case '?':
                std::cerr << "Unknown option. Use -h for help.\n";
                return 1;
            default:
                break;
        }
    }
    
    if (collectAll) {
        collectVlan = collectNat = collectFirewall = collectRoutes = collectBridges = true;
    }
    
    json allData = json::array();
    
    try {
        if (collectVlan) {
            if (!quietMode) std::cout << "Collecting VLAN information...\n";
            auto vlanCollector = std::make_unique<VlanCollector>();
            auto vlanData = vlanCollector->collectDataJson();
            allData.push_back(vlanData);
        }
        
        if (collectNat) {
            if (!quietMode) std::cout << "Collecting NAT rules...\n";
            auto natCollector = std::make_unique<NatCollector>();
            auto natData = natCollector->collectDataJson();
            allData.push_back(natData);
        }
        
        if (collectFirewall) {
            if (!quietMode) std::cout << "Collecting firewall rules...\n";
            auto firewallCollector = std::make_unique<FirewallCollector>();
            auto firewallData = firewallCollector->collectDataJson();
            allData.push_back(firewallData);
        }
        
        if (collectRoutes) {
            if (!quietMode) std::cout << "Collecting static routes...\n";
            auto routeCollector = std::make_unique<RouteCollector>();
            auto routeData = routeCollector->collectDataJson();
            allData.push_back(routeData);
        }
        
        if (collectBridges) {
            if (!quietMode) std::cout << "Collecting bridge information...\n";
            auto bridgeCollector = std::make_unique<BridgeCollector>();
            auto bridgeData = bridgeCollector->collectDataJson();
            allData.push_back(bridgeData);
        }
        
        if (allData.empty()) {
            std::cerr << "No data collected. Check permissions and network configuration.\n";
            return 1;
        }
        
        json finalResult;
        finalResult["timestamp"] = getCurrentTimestamp();
        finalResult["data"] = allData;
        
        std::string output;
        if (outputText) {
            // Fallback to text format for backward compatibility
            std::ostringstream oss;
            for (const auto& item : allData) {
                if (!quietMode) printSeparator();
                // Convert JSON data back to text format
                if (item.contains("data") && item.contains("type")) {
                    oss << item["type"].get<std::string>() << " Configuration:\n";
                    oss << std::string(item["type"].get<std::string>().length() + 14, '=') << "\n";
                    if (item["success"].get<bool>()) {
                        if (item["type"] == "vlan") {
                            for (const auto& vlan : item["data"]) {
                                oss << "VLAN " << vlan["vlanId"] << ": " << vlan["name"] 
                                    << " (" << vlan["interface"] << ") - " << vlan["status"] << "\n";
                            }
                        } else if (item["type"] == "routes") {
                            for (const auto& route : item["data"]) {
                                oss << route["destination"] << " via " << route["gateway"] 
                                    << " dev " << route["interface"] << "\n";
                            }
                        } else {
                            oss << "Data collected successfully. Use JSON format for detailed view.\n";
                        }
                    } else {
                        oss << "Error: " << item["error"] << "\n";
                    }
                    if (!quietMode) printSeparator();
                }
            }
            output = oss.str();
        } else {
            output = finalResult.dump(4);
        }
        
        if (outputFile.empty()) {
            std::cout << output;
        } else {
            std::ofstream file(outputFile);
            if (file.is_open()) {
                file << output;
                file.close();
                if (!quietMode) std::cout << "Data saved to " << outputFile << std::endl;
            } else {
                std::cerr << "Failed to open output file: " << outputFile << std::endl;
                return 1;
            }
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
