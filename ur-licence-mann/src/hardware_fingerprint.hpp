#pragma once

#include <string>
#include <map>

class HardwareFingerprint {
public:
    // Generate a hardware fingerprint based on system characteristics
    static std::string generate();
    
    // Generate fingerprint from specific components
    static std::string generate_from_components(const std::map<std::string, std::string>& components);
    
    // Get individual system components
    static std::string get_cpu_info();
    static std::string get_os_info();
    static std::string get_hostname();
    static std::string get_machine_id();
    static std::string read_first_mac_address();
    
private:
    static std::string hash_string(const std::string& input);
};
