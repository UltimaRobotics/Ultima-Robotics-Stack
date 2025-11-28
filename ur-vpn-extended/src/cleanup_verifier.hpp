#ifndef CLEANUP_VERIFIER_HPP
#define CLEANUP_VERIFIER_HPP

#include <string>
#include <vector>
#include <fstream>
#include <filesystem>
#include <nlohmann/json.hpp>
#include "cleanup_tracker.hpp"

using json = nlohmann::json;

namespace vpn_manager {

struct VerificationResult {
    bool exists;
    bool is_running;
    bool is_configured;
    std::string details;
    json raw_data;
    
    json to_json() const {
        json j;
        j["exists"] = exists;
        j["is_running"] = is_running;
        j["is_configured"] = is_configured;
        j["details"] = details;
        j["raw_data"] = raw_data;
        return j;
    }
};

class CleanupVerifier {
private:
    std::string config_file_path_;
    std::string routing_rules_path_;
    
public:
    CleanupVerifier(const std::string& config_path, const std::string& routing_path)
        : config_file_path_(config_path), routing_rules_path_(routing_path) {}
    
    VerificationResult verifyThreadExists(const std::string& instance_name) {
        VerificationResult result;
        result.exists = false;
        result.is_running = false;
        result.is_configured = false;
        
        // Check if thread exists by looking for it in /proc
        std::string cmd = "ps aux | grep -i '" + instance_name + "' | grep -v grep | wc -l";
        FILE* pipe = popen(cmd.c_str(), "r");
        if (pipe) {
            char buffer[128];
            if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                int count = std::stoi(std::string(buffer));
                result.exists = (count > 0);
                result.is_running = (count > 0);
            }
            pclose(pipe);
        }
        
        result.details = result.exists ? 
            "Thread process found for instance " + instance_name :
            "No thread process found for instance " + instance_name;
            
        result.raw_data["instance_name"] = instance_name;
        result.raw_data["process_count"] = result.exists ? 1 : 0;
        
        return result;
    }
    
    VerificationResult verifyRoutingRulesExist(const std::string& instance_name) {
        VerificationResult result;
        result.exists = false;
        result.is_running = false;
        result.is_configured = false;
        result.raw_data["rule_count"] = 0;
        result.raw_data["rules"] = json::array();
        
        // Check routing rules in the JSON config file
        if (std::filesystem::exists(routing_rules_path_)) {
            try {
                std::ifstream file(routing_rules_path_);
                json routing_config;
                file >> routing_config;
                
                if (routing_config.contains("routing_rules")) {
                    for (const auto& rule : routing_config["routing_rules"]) {
                        if (rule.contains("vpn_instance") && rule["vpn_instance"] == instance_name) {
                            result.exists = true;
                            result.is_configured = rule.value("enabled", false);
                            result.raw_data["rule_count"] = result.raw_data.value("rule_count", 0) + 1;
                            
                            if (rule.contains("destination") && rule.contains("gateway")) {
                                result.raw_data["rules"].push_back({
                                    {"destination", rule["destination"]},
                                    {"gateway", rule["gateway"]},
                                    {"enabled", rule["enabled"]}
                                });
                            }
                        }
                    }
                }
            } catch (const std::exception& e) {
                result.details = "Error reading routing config: " + std::string(e.what());
                return result;
            }
        }
        
        // Also check system routing table
        std::string cmd = "ip route show | grep -v '^default' | wc -l";
        FILE* pipe = popen(cmd.c_str(), "r");
        if (pipe) {
            char buffer[128];
            if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                result.raw_data["system_route_count"] = std::stoi(std::string(buffer));
            }
            pclose(pipe);
        }
        
        result.details = result.exists ?
            "Found " + std::to_string(result.raw_data["rule_count"].get<int>()) + " routing rules for " + instance_name :
            "No routing rules found for " + instance_name;
            
        return result;
    }
    
    VerificationResult verifyVPNConnectionExists(const std::string& instance_name) {
        VerificationResult result;
        result.exists = false;
        result.is_running = false;
        result.is_configured = false;
        result.raw_data["vpn_interface_count"] = 0;
        result.raw_data["vpn_process_count"] = 0;
        result.raw_data["interfaces"] = "";
        
        // Check for VPN interfaces
        std::string cmd = "ip link show | grep -E '(tun|tap|wg)' | wc -l";
        FILE* pipe = popen(cmd.c_str(), "r");
        if (pipe) {
            char buffer[128];
            if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                int interface_count = std::stoi(std::string(buffer));
                result.exists = (interface_count > 0);
                result.raw_data["vpn_interface_count"] = interface_count;
            }
            pclose(pipe);
        }
        
        // Check for active VPN processes
        cmd = "pgrep -f '(openvpn|wireguard)' | wc -l";
        pipe = popen(cmd.c_str(), "r");
        if (pipe) {
            char buffer[128];
            if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                int vpn_processes = std::stoi(std::string(buffer));
                result.is_running = (vpn_processes > 0);
                result.raw_data["vpn_process_count"] = vpn_processes;
            }
            pclose(pipe);
        }
        
        // Check for specific instance interface
        cmd = "ip link show | grep -E '" + instance_name + "|tun[0-9]|wg[0-9]' | head -5";
        pipe = popen(cmd.c_str(), "r");
        if (pipe) {
            char buffer[512];
            std::string interfaces;
            while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                interfaces += buffer;
            }
            pclose(pipe);
            
            if (!interfaces.empty()) {
                result.is_configured = true;
                result.raw_data["interfaces"] = interfaces;
            }
        }
        
        result.details = result.exists ?
            "VPN infrastructure active with " + std::to_string(result.raw_data["vpn_interface_count"].get<int>()) + " interfaces" :
            "No VPN infrastructure found";
            
        return result;
    }
    
    VerificationResult verifyConfigurationExists(const std::string& instance_name) {
        VerificationResult result;
        result.exists = false;
        result.is_running = false;
        result.is_configured = false;
        result.raw_data["config_files"] = json::array();
        
        // Check main configuration file
        if (std::filesystem::exists(config_file_path_)) {
            try {
                std::ifstream file(config_file_path_);
                json config;
                file >> config;
                
                if (config.contains("vpn_instances")) {
                    for (const auto& instance : config["vpn_instances"]) {
                        if (instance.contains("name") && instance["name"] == instance_name) {
                            result.exists = true;
                            result.is_configured = instance.value("enabled", false);
                            result.raw_data["instance_config"] = instance;
                            break;
                        }
                    }
                }
            } catch (const std::exception& e) {
                result.details = "Error reading main config: " + std::string(e.what());
                return result;
            }
        }
        
        // Check for instance-specific config files
        std::string instance_config_dir = "/etc/ur-vpn/instances/";
        if (std::filesystem::exists(instance_config_dir)) {
            for (const auto& entry : std::filesystem::directory_iterator(instance_config_dir)) {
                if (entry.path().filename().string().find(instance_name) != std::string::npos) {
                    result.is_running = true;
                    result.raw_data["config_files"].push_back(entry.path().string());
                }
            }
        }
        
        result.details = result.exists ?
            "Configuration found for " + instance_name + " in main config" :
            "No configuration found for " + instance_name;
            
        return result;
    }
    
    json generateVerificationReport(const std::string& instance_name) {
        json report;
        report["instance_name"] = instance_name;
        report["verification_timestamp"] = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        
        report["thread_verification"] = verifyThreadExists(instance_name).to_json();
        report["routing_verification"] = verifyRoutingRulesExist(instance_name).to_json();
        report["vpn_verification"] = verifyVPNConnectionExists(instance_name).to_json();
        report["configuration_verification"] = verifyConfigurationExists(instance_name).to_json();
        
        // Summary
        bool any_exist = report["thread_verification"]["exists"].get<bool>() ||
                        report["routing_verification"]["exists"].get<bool>() ||
                        report["vpn_verification"]["exists"].get<bool>() ||
                        report["configuration_verification"]["exists"].get<bool>();
        
        report["summary"] = {
            {"any_resources_exist", any_exist},
            {"cleanup_needed", any_exist},
            {"verification_complete", true}
        };
        
        return report;
    }
};

}

#endif
