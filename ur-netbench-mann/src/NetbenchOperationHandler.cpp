#include "../include/NetbenchOperationHandler.hpp"
#include "../include/ConfigManager.hpp"
#include "../include/OperationWorker.hpp"
#include <iostream>
#include <sstream>
#include <fstream>
#include <cstdlib>
#include <memory>

int NetbenchOperationHandler::execute(const std::string& operationConfigJson, 
                                     const std::string& packageConfigJson, 
                                     bool verbose) {
    try {
        // Parse operation configuration
        json operationConfig = json::parse(operationConfigJson);
        json packageConfig = json::parse(packageConfigJson);
        
        // Extract operation method
        if (!operationConfig.contains("operation")) {
            std::cerr << "[NetbenchOperationHandler] Missing 'operation' field in request" << std::endl;
            return -1;
        }
        
        std::string operation = operationConfig["operation"].get<std::string>();
        
        // Extract parameters
        std::map<std::string, std::string> params;
        if (operationConfig.contains("params")) {
            params = parseParams(operationConfig["params"]);
        }
        
        // Log the operation
        logOperation(operation, params, verbose);
        
        // Route to appropriate handler
        if (operation == "servers-status") {
            return handleServersStatus(params, packageConfig, verbose);
        } else if (operation == "ping-test") {
            return handlePingTest(params, packageConfig, verbose);
        } else if (operation == "traceroute-test") {
            return handleTracerouteTest(params, packageConfig, verbose);
        } else if (operation == "iperf-test") {
            return handleIperfTest(params, packageConfig, verbose);
        } else if (operation == "dns-lookup") {
            return handleDnsLookup(params, packageConfig, verbose);
        } else {
            std::cerr << "[NetbenchOperationHandler] Unknown operation: " << operation << std::endl;
            return -1;
        }
        
    } catch (const json::parse_error& e) {
        std::cerr << "[NetbenchOperationHandler] JSON parse error: " << e.what() << std::endl;
        return -1;
    } catch (const std::exception& e) {
        std::cerr << "[NetbenchOperationHandler] Exception: " << e.what() << std::endl;
        return -1;
    }
}

int NetbenchOperationHandler::handleServersStatus(const std::map<std::string, std::string>& params, 
                                                  const json& packageConfig, bool verbose) {
    try {
        // Extract required parameters
        std::string serversListPath;
        std::string outputDir;
        
        if (params.find("servers_list_path") != params.end()) {
            serversListPath = params.at("servers_list_path");
        } else if (packageConfig.contains("servers_list_path")) {
            serversListPath = packageConfig["servers_list_path"].get<std::string>();
        } else {
            std::cerr << "[NetbenchOperationHandler] servers_list_path is required for servers-status operation" << std::endl;
            return -1;
        }
        
        if (params.find("output_dir") != params.end()) {
            outputDir = params.at("output_dir");
        } else if (packageConfig.contains("output_dir")) {
            outputDir = packageConfig["output_dir"].get<std::string>();
        } else {
            outputDir = "runtime-data/server-status"; // Default
        }
        
        // Build the command
        std::string command = "./netbench-cli -package_config " + serversListPath;
        
        // Execute the command
        std::string output = executeCommand(command, verbose);
        int exitCode = (output.find("ERROR") == std::string::npos) ? 0 : 1;
        
        if (verbose) {
            std::cout << "[NetbenchOperationHandler] Servers status command completed with exit code: " << exitCode << std::endl;
            std::cout << "[NetbenchOperationHandler] Output: " << output << std::endl;
        }
        
        return exitCode;
        
    } catch (const std::exception& e) {
        std::cerr << "[NetbenchOperationHandler] Exception in servers-status: " << e.what() << std::endl;
        return -1;
    }
}

int NetbenchOperationHandler::handlePingTest(const std::map<std::string, std::string>& params, 
                                            const json& packageConfig, bool verbose) {
    try {
        // Extract required parameters
        std::string target;
        int count = 4;
        
        if (params.find("target") != params.end()) {
            target = params.at("target");
        } else {
            std::cerr << "[NetbenchOperationHandler] target is required for ping-test operation" << std::endl;
            return -1;
        }
        
        if (params.find("count") != params.end()) {
            count = std::stoi(params.at("count"));
        }
        
        // Build the ping command
        std::string command = "ping -c " + std::to_string(count) + " " + target;
        
        // Execute the command
        std::string output = executeCommand(command, verbose);
        int exitCode = (output.find("0% packet loss") != std::string::npos) ? 0 : 1;
        
        if (verbose) {
            std::cout << "[NetbenchOperationHandler] Ping test to " << target << " completed with exit code: " << exitCode << std::endl;
        }
        
        return exitCode;
        
    } catch (const std::exception& e) {
        std::cerr << "[NetbenchOperationHandler] Exception in ping-test: " << e.what() << std::endl;
        return -1;
    }
}

int NetbenchOperationHandler::handleTracerouteTest(const std::map<std::string, std::string>& params, 
                                                  const json& packageConfig, bool verbose) {
    try {
        // Extract required parameters
        std::string target;
        int maxHops = 30;
        
        if (params.find("target") != params.end()) {
            target = params.at("target");
        } else {
            std::cerr << "[NetbenchOperationHandler] target is required for traceroute-test operation" << std::endl;
            return -1;
        }
        
        if (params.find("max_hops") != params.end()) {
            maxHops = std::stoi(params.at("max_hops"));
        }
        
        // Build the traceroute command
        std::string command = "traceroute -m " + std::to_string(maxHops) + " " + target;
        
        // Execute the command
        std::string output = executeCommand(command, verbose);
        int exitCode = (output.find(" 0.0% ") != std::string::npos || 
                       output.find(" 1.0% ") != std::string::npos) ? 0 : 1;
        
        if (verbose) {
            std::cout << "[NetbenchOperationHandler] Traceroute test to " << target << " completed with exit code: " << exitCode << std::endl;
        }
        
        return exitCode;
        
    } catch (const std::exception& e) {
        std::cerr << "[NetbenchOperationHandler] Exception in traceroute-test: " << e.what() << std::endl;
        return -1;
    }
}

int NetbenchOperationHandler::handleIperfTest(const std::map<std::string, std::string>& params, 
                                             const json& packageConfig, bool verbose) {
    try {
        // Extract required parameters
        std::string target;
        std::string mode = "tcp";
        int time = 10;
        
        if (params.find("target") != params.end()) {
            target = params.at("target");
        } else {
            std::cerr << "[NetbenchOperationHandler] target is required for iperf-test operation" << std::endl;
            return -1;
        }
        
        if (params.find("mode") != params.end()) {
            mode = params.at("mode");
        }
        
        if (params.find("time") != params.end()) {
            time = std::stoi(params.at("time"));
        }
        
        // Build the iperf command
        std::string command = "iperf3 -c " + target + " -t " + std::to_string(time);
        if (mode == "udp") {
            command += " -u";
        }
        
        // Execute the command
        std::string output = executeCommand(command, verbose);
        int exitCode = (output.find("ERROR") == std::string::npos) ? 0 : 1;
        
        if (verbose) {
            std::cout << "[NetbenchOperationHandler] Iperf test to " << target << " completed with exit code: " << exitCode << std::endl;
        }
        
        return exitCode;
        
    } catch (const std::exception& e) {
        std::cerr << "[NetbenchOperationHandler] Exception in iperf-test: " << e.what() << std::endl;
        return -1;
    }
}

int NetbenchOperationHandler::handleDnsLookup(const std::map<std::string, std::string>& params, 
                                             const json& packageConfig, bool verbose) {
    try {
        // Extract required parameters
        std::string domain;
        std::string dnsServer = "8.8.8.8";
        
        if (params.find("domain") != params.end()) {
            domain = params.at("domain");
        } else {
            std::cerr << "[NetbenchOperationHandler] domain is required for dns-lookup operation" << std::endl;
            return -1;
        }
        
        if (params.find("dns_server") != params.end()) {
            dnsServer = params.at("dns_server");
        }
        
        // Build the dig command
        std::string command = "dig @" + dnsServer + " " + domain;
        
        // Execute the command
        std::string output = executeCommand(command, verbose);
        int exitCode = (output.find("ANSWER SECTION") != std::string::npos) ? 0 : 1;
        
        if (verbose) {
            std::cout << "[NetbenchOperationHandler] DNS lookup for " << domain << " completed with exit code: " << exitCode << std::endl;
        }
        
        return exitCode;
        
    } catch (const std::exception& e) {
        std::cerr << "[NetbenchOperationHandler] Exception in dns-lookup: " << e.what() << std::endl;
        return -1;
    }
}

std::map<std::string, std::string> NetbenchOperationHandler::parseParams(const json& paramsObj) {
    std::map<std::string, std::string> params;
    
    for (auto& [key, value] : paramsObj.items()) {
        if (value.is_string()) {
            params[key] = value.get<std::string>();
        } else if (value.is_number()) {
            params[key] = std::to_string(value.get<int>());
        } else if (value.is_boolean()) {
            params[key] = value.get<bool>() ? "true" : "false";
        } else {
            params[key] = value.dump();
        }
    }
    
    return params;
}

std::string NetbenchOperationHandler::executeCommand(const std::string& command, bool verbose) {
    if (verbose) {
        std::cout << "[NetbenchOperationHandler] Executing command: " << command << std::endl;
    }
    
    // Execute command and capture output
    std::shared_ptr<FILE> pipe(popen(command.c_str(), "r"), pclose);
    if (!pipe) {
        return "ERROR: Failed to execute command";
    }
    
    char buffer[128];
    std::string result = "";
    
    while (fgets(buffer, sizeof(buffer), pipe.get()) != nullptr) {
        result += buffer;
    }
    
    return result;
}

std::string NetbenchOperationHandler::generateResultJson(int exitCode, const std::string& output, const std::string& operation) {
    json result;
    result["operation"] = operation;
    result["exit_code"] = exitCode;
    result["success"] = (exitCode == 0);
    result["output"] = output;
    result["timestamp"] = std::time(nullptr);
    
    return result.dump();
}

void NetbenchOperationHandler::logOperation(const std::string& operation, 
                                           const std::map<std::string, std::string>& params, 
                                           bool verbose) {
    if (verbose) {
        std::cout << "[NetbenchOperationHandler] Executing operation: " << operation << std::endl;
        std::cout << "[NetbenchOperationHandler] Parameters:" << std::endl;
        for (const auto& [key, value] : params) {
            std::cout << "  " << key << " = " << value << std::endl;
        }
    }
}
