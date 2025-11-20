#pragma once

#include <string>
#include <random>
#include <sstream>
#include <iomanip>
#include <ctime>

// License Tier Enumeration
enum class LicenseTier {
    OpenUser,
    Professional,
    Enterprise,
    Developer
};

// License Type Enumeration  
enum class LicenseType {
    UltimaOpenLicence,
    UltimaProfessionalLicence,
    UltimaEnterpriseLicence,
    UltimaDeveloperLicence,
    CustomLicence
};

// Signature Algorithm Enumeration
enum class SignatureAlgorithm {
    RSA_SHA256,
    ECDSA_SHA256,
    RSA_SHA512
};

// Device Information Structure
struct DeviceInfo {
    std::string hardware_id;
    std::string model;
    std::string mac;
    
    // Default constructor
    DeviceInfo() = default;
    
    // Constructor with parameters
    DeviceInfo(const std::string& hw_id, const std::string& device_model, const std::string& mac_addr)
        : hardware_id(hw_id), model(device_model), mac(mac_addr) {}
};

// Customer Information Structure
struct CustomerInfo {
    std::string name;
    std::string email;
    
    // Default constructor
    CustomerInfo() = default;
    
    // Constructor with parameters
    CustomerInfo(const std::string& customer_name, const std::string& customer_email)
        : name(customer_name), email(customer_email) {}
};

// Enhanced License Data Structure
struct EnhancedLicenseData {
    std::string license_id;
    std::string product;
    std::string version;
    DeviceInfo device;
    CustomerInfo customer;
    std::string issued_at;
    std::string valid_until;
    LicenseTier license_tier;
    LicenseType licence_type;
    std::string signature;
    SignatureAlgorithm signature_algorithm;
    
    // Default constructor
    EnhancedLicenseData() 
        : license_tier(LicenseTier::OpenUser)
        , licence_type(LicenseType::UltimaOpenLicence)
        , signature_algorithm(SignatureAlgorithm::RSA_SHA256) {}
};

// Utility Functions for License Types
class LicenseTypeUtils {
public:
    // Generate a UUID-like license ID
    static std::string generate_license_id() {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 15);
        std::uniform_int_distribution<> dis2(8, 11);
        
        std::stringstream ss;
        int i;
        ss << std::hex;
        for (i = 0; i < 8; i++) {
            ss << dis(gen);
        }
        ss << "-";
        for (i = 0; i < 4; i++) {
            ss << dis(gen);
        }
        ss << "-4";
        for (i = 0; i < 3; i++) {
            ss << dis(gen);
        }
        ss << "-";
        ss << dis2(gen);
        for (i = 0; i < 3; i++) {
            ss << dis(gen);
        }
        ss << "-";
        for (i = 0; i < 12; i++) {
            ss << dis(gen);
        }
        return ss.str();
    }
    
    // Convert LicenseTier to string
    static std::string license_tier_to_string(LicenseTier tier) {
        switch (tier) {
            case LicenseTier::OpenUser: return "OpenUser";
            case LicenseTier::Professional: return "Professional";  
            case LicenseTier::Enterprise: return "Enterprise";
            case LicenseTier::Developer: return "Developer";
            default: return "OpenUser";
        }
    }
    
    // Convert string to LicenseTier
    static LicenseTier string_to_license_tier(const std::string& tier_str) {
        if (tier_str == "Professional") return LicenseTier::Professional;
        if (tier_str == "Enterprise") return LicenseTier::Enterprise;
        if (tier_str == "Developer") return LicenseTier::Developer;
        return LicenseTier::OpenUser; // Default
    }
    
    // Convert LicenseType to string
    static std::string license_type_to_string(LicenseType type) {
        switch (type) {
            case LicenseType::UltimaOpenLicence: return "UltimaOpenLicence";
            case LicenseType::UltimaProfessionalLicence: return "UltimaProfessionalLicence";
            case LicenseType::UltimaEnterpriseLicence: return "UltimaEnterpriseLicence";
            case LicenseType::UltimaDeveloperLicence: return "UltimaDeveloperLicence";
            case LicenseType::CustomLicence: return "CustomLicence";
            default: return "UltimaOpenLicence";
        }
    }
    
    // Convert string to LicenseType
    static LicenseType string_to_license_type(const std::string& type_str) {
        if (type_str == "UltimaProfessionalLicence") return LicenseType::UltimaProfessionalLicence;
        if (type_str == "UltimaEnterpriseLicence") return LicenseType::UltimaEnterpriseLicence;
        if (type_str == "UltimaDeveloperLicence") return LicenseType::UltimaDeveloperLicence;
        if (type_str == "CustomLicence") return LicenseType::CustomLicence;
        return LicenseType::UltimaOpenLicence; // Default
    }
    
    // Convert SignatureAlgorithm to string
    static std::string signature_algorithm_to_string(SignatureAlgorithm algo) {
        switch (algo) {
            case SignatureAlgorithm::RSA_SHA256: return "RSA_SHA256";
            case SignatureAlgorithm::ECDSA_SHA256: return "ECDSA_SHA256";
            case SignatureAlgorithm::RSA_SHA512: return "RSA_SHA512";
            default: return "RSA_SHA256";
        }
    }
    
    // Convert string to SignatureAlgorithm
    static SignatureAlgorithm string_to_signature_algorithm(const std::string& algo_str) {
        if (algo_str == "ECDSA_SHA256") return SignatureAlgorithm::ECDSA_SHA256;
        if (algo_str == "RSA_SHA512") return SignatureAlgorithm::RSA_SHA512;
        return SignatureAlgorithm::RSA_SHA256; // Default
    }
    
    // Get current ISO8601 timestamp
    static std::string get_current_iso8601_timestamp() {
        std::time_t now = std::time(nullptr);
        std::stringstream ss;
        ss << std::put_time(std::gmtime(&now), "%Y-%m-%dT%H:%M:%SZ");
        return ss.str();
    }
    
    // Parse date from YYYY-MM-DD to ISO8601 format with end of day
    static std::string parse_date_to_iso8601_end_of_day(const std::string& date_str) {
        // If already in ISO format, return as is
        if (date_str.find('T') != std::string::npos) {
            return date_str;
        }
        // Convert YYYY-MM-DD to YYYY-MM-DDTHH:MM:SSZ (end of day)
        return date_str + "T23:59:59Z";
    }
    
    // Get expiry timestamp N days from now
    static std::string get_expiry_timestamp(int days_from_now) {
        std::time_t now = std::time(nullptr);
        std::time_t expiry = now + (days_from_now * 24 * 60 * 60);
        std::stringstream ss;
        ss << std::put_time(std::gmtime(&expiry), "%Y-%m-%dT%H:%M:%SZ");
        return ss.str();
    }
};