#include "hardware_fingerprint.hpp"
#include <lcxx/identifiers/hardware.hpp>
#include <lcxx/identifiers/os.hpp>
#include <openssl/sha.h>
#include <openssl/evp.h>
#include <iostream>
#include <sstream>
#include <fstream>
#include <stdexcept>
#include <iomanip>
#include <vector>
#include <filesystem>
#include <sys/utsname.h>
#include <unistd.h>
#include <limits.h>

std::string HardwareFingerprint::generate() {
    std::map<std::string, std::string> components;
    
    // Use licensecxx identifiers where possible
    try {
        components["cpu"] = get_cpu_info();
        components["os"] = get_os_info();
        components["hostname"] = get_hostname();
        components["machine_id"] = get_machine_id();
    } catch (const std::exception& e) {
        std::cerr << "Warning: Error collecting hardware info: " << e.what() << std::endl;
    }
    
    return generate_from_components(components);
}

std::string HardwareFingerprint::generate_from_components(const std::map<std::string, std::string>& components) {
    std::stringstream ss;
    
    // Create a deterministic string from components
    for (const auto& [key, value] : components) {
        if (!value.empty()) {
            ss << key << ":" << value << ";";
        }
    }
    
    std::string combined = ss.str();
    return hash_string(combined);
}

std::string HardwareFingerprint::get_cpu_info() {
    try {
        // Try to use licensecxx hardware identifier
        auto hw_identifier = lcxx::experimental::identifiers::hardware(
            lcxx::experimental::identifiers::hw_ident_strat::cpu_model_name | 
            lcxx::experimental::identifiers::hw_ident_strat::cpu_max_frequency
        );
        return hw_identifier.source_text;
    } catch (const std::exception&) {
        // Fallback methods
    }
    
    // Fallback: read from /proc/cpuinfo on Linux
    std::ifstream cpuinfo("/proc/cpuinfo");
    if (cpuinfo.is_open()) {
        std::string line;
        while (std::getline(cpuinfo, line)) {
            if (line.find("model name") != std::string::npos) {
                size_t colon_pos = line.find(':');
                if (colon_pos != std::string::npos && colon_pos + 2 < line.length()) {
                    std::string model = line.substr(colon_pos + 2);
                    // Trim leading/trailing whitespace
                    size_t start = model.find_first_not_of(" \t");
                    size_t end = model.find_last_not_of(" \t\r\n");
                    if (start != std::string::npos && end != std::string::npos) {
                        return model.substr(start, end - start + 1);
                    }
                }
            }
        }
    }
    
    return "unknown-cpu";
}

std::string HardwareFingerprint::get_os_info() {
    try {
        // Use licensecxx OS identifier
        auto os_identifier = lcxx::experimental::identifiers::os(
            lcxx::experimental::identifiers::os_ident_strat::os_name |
            lcxx::experimental::identifiers::os_ident_strat::os_architecture
        );
        return os_identifier.source_text;
    } catch (const std::exception&) {
        // Fallback: use POSIX uname
        struct utsname uts;
        if (uname(&uts) == 0) {
            std::stringstream ss;
            ss << uts.sysname << " " << uts.release << " " 
               << uts.version << " " << uts.machine;
            return ss.str();
        }
    }
    
    return "unknown-os";
}

std::string HardwareFingerprint::get_hostname() {
    try {
        auto hostname_identifier = lcxx::experimental::identifiers::os(
            lcxx::experimental::identifiers::os_ident_strat::os_pc_name
        );
        return hostname_identifier.source_text;
    } catch (const std::exception&) {
        // Fallback: use POSIX gethostname
        char hostname[HOST_NAME_MAX + 1];
        if (gethostname(hostname, sizeof(hostname)) == 0) {
            hostname[HOST_NAME_MAX] = '\0';  // Ensure null termination
            return std::string(hostname);
        }
    }
    
    return "unknown-host";
}

std::string HardwareFingerprint::get_machine_id() {
    // Try different sources for machine ID
    std::vector<std::string> id_files = {
        "/etc/machine-id",
        "/var/lib/dbus/machine-id",
        "/sys/class/dmi/id/product_uuid"
    };
    
    for (const auto& file : id_files) {
        std::ifstream f(file);
        if (f.is_open()) {
            std::string id;
            std::getline(f, id);
            if (!id.empty()) {
                // Trim whitespace
                size_t start = id.find_first_not_of(" \t\r\n");
                size_t end = id.find_last_not_of(" \t\r\n");
                if (start != std::string::npos && end != std::string::npos) {
                    return id.substr(start, end - start + 1);
                }
            }
        }
    }
    
    // Fallback: read MAC address from network interfaces
    std::string mac = read_first_mac_address();
    if (!mac.empty()) {
        return mac;
    }
    
    return "unknown";
}

std::string HardwareFingerprint::read_first_mac_address() {
    try {
        // Read from /sys/class/net/ directory
        std::filesystem::path net_path = "/sys/class/net";
        if (std::filesystem::exists(net_path)) {
            for (const auto& entry : std::filesystem::directory_iterator(net_path)) {
                if (entry.is_directory()) {
                    // Skip loopback interface
                    std::string iface_name = entry.path().filename().string();
                    if (iface_name == "lo") continue;
                    
                    // Try to read MAC address
                    std::filesystem::path addr_file = entry.path() / "address";
                    if (std::filesystem::exists(addr_file)) {
                        std::ifstream addr_stream(addr_file);
                        if (addr_stream.is_open()) {
                            std::string mac;
                            std::getline(addr_stream, mac);
                            if (!mac.empty() && mac != "00:00:00:00:00:00") {
                                // Trim whitespace
                                size_t start = mac.find_first_not_of(" \t\r\n");
                                size_t end = mac.find_last_not_of(" \t\r\n");
                                if (start != std::string::npos && end != std::string::npos) {
                                    return mac.substr(start, end - start + 1);
                                }
                            }
                        }
                    }
                }
            }
        }
    } catch (const std::exception& e) {
        // Ignore errors, return empty string
    }
    
    return "";
}

std::string HardwareFingerprint::hash_string(const std::string& input) {
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hash_len;
    
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) {
        return "hash_error";
    }
    
    if (EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) != 1 ||
        EVP_DigestUpdate(ctx, input.c_str(), input.length()) != 1 ||
        EVP_DigestFinal_ex(ctx, hash, &hash_len) != 1) {
        EVP_MD_CTX_free(ctx);
        return "hash_error";
    }
    
    EVP_MD_CTX_free(ctx);
    
    std::stringstream ss;
    for (unsigned int i = 0; i < hash_len; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
    }
    
    return ss.str();
}

