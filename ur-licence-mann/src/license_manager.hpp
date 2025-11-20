#pragma once

#include <string>
#include <map>
#include <memory>
#include <vector>
#include "license_types.hpp"

// Forward declarations and includes for the licensecxx API
#include <lcxx/crypto.hpp>
namespace lcxx {
    class license;
}

struct LicenseData {
    std::string user_name;
    std::string user_email;
    std::string expiry_date;
    bool bind_hardware = false;
    std::map<std::string, std::string> custom_fields;
};

struct LicenseVerificationResult {
    bool valid = false;
    std::string error_message;
    LicenseData license_data;
};

struct EnhancedLicenseVerificationResult {
    bool valid = false;
    std::string error_message;
    EnhancedLicenseData license_data;
};

class LicenseManager {
public:
    explicit LicenseManager(bool verbose = false);
    ~LicenseManager();

    // Generate a new license with optional AES encryption
    bool generate_license(
        const LicenseData& data,
        const std::string& private_key_path,
        const std::string& output_path,
        const std::string& encryption_key = ""
    );

    // Verify an existing license
    LicenseVerificationResult verify_license(
        const std::string& license_path,
        const std::string& public_key_path,
        const std::string& encryption_key = "",
        bool check_hardware = false,
        bool check_expiry = true
    );

    // Update/renew an existing license
    bool update_license(
        const std::string& input_license_path,
        const std::string& output_path,
        const std::string& public_key_path,
        const std::string& private_key_path,
        const std::string& encryption_key = "",
        const std::string& new_expiry = "",
        const std::map<std::string, std::string>& updates = {}
    );

    // Generate RSA key pair
    bool generate_key_pair(const std::string& output_dir, int key_size = 2048);

    // Enhanced license generation with structured data
    bool generate_enhanced_license(
        const EnhancedLicenseData& data,
        const std::string& private_key_path,
        const std::string& output_path,
        const std::string& encryption_key = ""
    );

    // Enhanced license verification
    EnhancedLicenseVerificationResult verify_enhanced_license(
        const std::string& license_path,
        const std::string& public_key_path,
        const std::string& encryption_key = "",
        bool check_hardware = false,
        bool check_expiry = true
    );

    // Extract specific field from license
    std::string extract_license_field(
        const std::string& license_path,
        const std::string& field_name,
        const std::string& public_key_path = "",
        const std::string& encryption_key = ""
    );

    // Get all license fields as a map
    std::map<std::string, std::string> extract_all_license_fields(
        const std::string& license_path,
        const std::string& public_key_path = "",
        const std::string& encryption_key = ""
    );

private:
    bool verbose_;
    
    // Helper methods
    lcxx::crypto::rsa_key_t load_private_key(const std::string& key_path);
    lcxx::crypto::rsa_key_t load_public_key(const std::string& key_path);
    std::string read_file(const std::string& file_path);
    bool write_file(const std::string& file_path, const std::string& content);
    bool is_date_expired(const std::string& date_str);
    std::string get_current_date();
    void log(const std::string& message);
};
