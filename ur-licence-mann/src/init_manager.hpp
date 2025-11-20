#pragma once

#include "package_config.hpp"
#include <string>

class InitManager {
public:
    static bool initialize(PackageConfig& config, bool verbose = false);
    
private:
    static bool ensure_directories(const PackageConfig& config, bool verbose);
    static bool ensure_encryption_keys(PackageConfig& config, bool verbose);
    static bool ensure_rsa_keys(const PackageConfig& config, bool verbose);
    static bool verify_key_pair_consistency(const std::string& private_key_path, const std::string& public_key_path, bool verbose);
    static bool encrypt_license_definitions(const PackageConfig& config, bool verbose);
    static bool ensure_encrypted_license_definitions(const PackageConfig& config, bool verbose);
    static bool ensure_single_license_file(const PackageConfig& config, bool verbose);
    static bool validate_and_reset_license(const std::filesystem::path& license_path, const PackageConfig& config, bool verbose);
    static bool create_valid_license(const std::filesystem::path& license_path, const PackageConfig& config, bool verbose);
    static std::string generate_random_key(size_t length);
};
