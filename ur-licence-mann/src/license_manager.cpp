#include "license_manager.hpp"
#include "crypto_utils.hpp"
#include "hardware_fingerprint.hpp"

#include <lcxx/lcxx.hpp>
#include <lcxx/identifiers/hardware.hpp>
#include <lcxx/identifiers/os.hpp>

#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>

#include <iostream>
#include <fstream>
#include <filesystem>
#include <ctime>
#include <iomanip>
#include <sstream>

LicenseManager::LicenseManager(bool verbose) : verbose_(verbose) {}

LicenseManager::~LicenseManager() = default;

bool LicenseManager::generate_license(
    const LicenseData& data,
    const std::string& private_key_path,
    const std::string& output_path,
    const std::string& encryption_key
) {
    try {
        log("Starting license generation...");

        // Load private key
        auto private_key = load_private_key(private_key_path);
        if (!private_key) {
            std::cerr << "Failed to load private key from: " << private_key_path << std::endl;
            return false;
        }

        // Create license object
        lcxx::license license;

        // Add basic license data
        if (!data.user_name.empty()) {
            license.push_content("user_name", data.user_name);
        }
        if (!data.user_email.empty()) {
            license.push_content("user_email", data.user_email);
        }
        if (!data.expiry_date.empty()) {
            license.push_content("expiry_date", data.expiry_date);
        } else {
            // Default to 1 year from now in ISO8601 format
            std::time_t now = std::time(nullptr);
            std::time_t future = now + (365 * 24 * 60 * 60); // 365 days in seconds
            std::tm* future_tm = std::gmtime(&future);
            std::stringstream ss;
            ss << std::put_time(future_tm, "%Y-%m-%dT%H:%M:%SZ");
            license.push_content("expiry_date", ss.str());
        }

        // Add generation timestamp
        license.push_content("generated_at", get_current_date());

        // Add hardware fingerprint if requested
        if (data.bind_hardware) {
            log("Collecting hardware fingerprint...");
            std::string hw_fingerprint = HardwareFingerprint::generate();
            license.push_content("hardware_fingerprint", hw_fingerprint);
            log("Hardware fingerprint: " + hw_fingerprint);
        }

        // Add custom fields
        for (const auto& [key, value] : data.custom_fields) {
            license.push_content(key, value);
        }

        // Generate signed license JSON
        auto license_json = lcxx::to_json(license, private_key);
        std::string license_string = license_json.dump(4);

        log("License generated, size: " + std::to_string(license_string.size()) + " bytes");

        // Encrypt if encryption key is provided
        if (!encryption_key.empty()) {
            log("Encrypting license with AES-256...");
            license_string = CryptoUtils::encrypt_aes256(license_string, encryption_key);
            if (license_string.empty()) {
                std::cerr << "Failed to encrypt license" << std::endl;
                return false;
            }
        }

        // Write to file
        if (!write_file(output_path, license_string)) {
            std::cerr << "Failed to write license to: " << output_path << std::endl;
            return false;
        }

        log("License written to: " + output_path);
        return true;

    } catch (const std::exception& e) {
        std::cerr << "Exception during license generation: " << e.what() << std::endl;
        return false;
    }
}

LicenseVerificationResult LicenseManager::verify_license(
    const std::string& license_path,
    const std::string& public_key_path,
    const std::string& encryption_key,
    bool check_hardware,
    bool check_expiry
) {
    LicenseVerificationResult result;
    
    try {
        log("Starting license verification...");

        // Load public key
        auto public_key = load_public_key(public_key_path);
        if (!public_key) {
            result.error_message = "Failed to load public key from: " + public_key_path;
            return result;
        }

        // Read license file
        std::string license_content = read_file(license_path);
        if (license_content.empty()) {
            result.error_message = "Failed to read license file: " + license_path;
            return result;
        }

        // Auto-detect and decrypt if content is encrypted
        if (CryptoUtils::is_content_encrypted(license_content)) {
            if (encryption_key.empty()) {
                result.error_message = "License is encrypted but no encryption key provided";
                return result;
            }
            log("Decrypting license with AES-256...");
            license_content = CryptoUtils::decrypt_aes256(license_content, encryption_key);
            if (license_content.empty()) {
                result.error_message = "Failed to decrypt license - invalid encryption key";
                return result;
            }
        }

        // Parse JSON and verify signature
        auto [license, signature] = lcxx::from_json(license_content);
        
        if (!lcxx::verify_license(license, signature, public_key)) {
            result.error_message = "License signature verification failed";
            return result;
        }

        log("License signature verified successfully");

        // Extract license data
        auto user_name_opt = license.get("user_name");
        if (user_name_opt.has_value()) {
            result.license_data.user_name = user_name_opt.value();
        }
        auto user_email_opt = license.get("user_email");
        if (user_email_opt.has_value()) {
            result.license_data.user_email = user_email_opt.value();
        }
        auto expiry_date_opt = license.get("expiry_date");
        if (expiry_date_opt.has_value()) {
            result.license_data.expiry_date = expiry_date_opt.value();
        }

        // Check expiry date if requested
        if (check_expiry) {
            auto expiry_opt = license.get("expiry_date");
            if (expiry_opt.has_value()) {
                std::string expiry = expiry_opt.value();
                if (is_date_expired(expiry)) {
                    result.error_message = "License has expired on: " + expiry;
                    return result;
                }
                log("License expiry check passed: " + expiry);
            }
        }

        // Check hardware fingerprint if requested
        if (check_hardware) {
            auto hw_fingerprint_opt = license.get("hardware_fingerprint");
            if (hw_fingerprint_opt.has_value()) {
                std::string stored_fingerprint = hw_fingerprint_opt.value();
                std::string current_fingerprint = HardwareFingerprint::generate();
                
                if (stored_fingerprint != current_fingerprint) {
                    result.error_message = "Hardware fingerprint mismatch";
                    log("Stored fingerprint: " + stored_fingerprint);
                    log("Current fingerprint: " + current_fingerprint);
                    return result;
                }
                log("Hardware fingerprint verification passed");
            }
        }

        result.valid = true;
        log("License verification completed successfully");
        return result;

    } catch (const std::exception& e) {
        result.error_message = "Exception during verification: " + std::string(e.what());
        return result;
    }
}

bool LicenseManager::update_license(
    const std::string& input_license_path,
    const std::string& output_path,
    const std::string& public_key_path,
    const std::string& private_key_path,
    const std::string& encryption_key,
    const std::string& new_expiry,
    const std::map<std::string, std::string>& updates
) {
    try {
        log("Starting license update...");

        // First verify the existing license
        auto verification_result = verify_license(
            input_license_path, 
            public_key_path, 
            encryption_key, 
            false, // Don't check hardware for update
            false  // Don't check expiry for update
        );

        if (!verification_result.valid) {
            std::cerr << "Cannot update invalid license: " << verification_result.error_message << std::endl;
            return false;
        }

        // Load keys
        auto private_key = load_private_key(private_key_path);
        auto public_key = load_public_key(public_key_path);
        if (!private_key || !public_key) {
            std::cerr << "Failed to load cryptographic keys" << std::endl;
            return false;
        }

        // Read and decrypt original license
        std::string license_content = read_file(input_license_path);
        if (CryptoUtils::is_content_encrypted(license_content)) {
            if (encryption_key.empty()) {
                std::cerr << "License is encrypted but no encryption key provided" << std::endl;
                return false;
            }
            license_content = CryptoUtils::decrypt_aes256(license_content, encryption_key);
        }

        auto [original_license, original_signature] = lcxx::from_json(license_content);

        // Create updated license
        lcxx::license updated_license;

        // Copy existing content from the original license
        auto original_content = original_license.get_content();
        for (const auto& [key, value] : original_content) {
            updated_license.push_content(key, value);
        }

        // Update expiry date if provided
        if (!new_expiry.empty()) {
            updated_license.push_content("expiry_date", new_expiry);
        }

        // Apply field updates
        for (const auto& [key, value] : updates) {
            updated_license.push_content(key, value);
        }

        // Add update timestamp
        updated_license.push_content("updated_at", get_current_date());

        // Generate new signed license
        auto updated_json = lcxx::to_json(updated_license, private_key);
        std::string updated_content = updated_json.dump(4);

        // Encrypt if necessary
        if (!encryption_key.empty()) {
            updated_content = CryptoUtils::encrypt_aes256(updated_content, encryption_key);
        }

        // Write updated license
        if (!write_file(output_path, updated_content)) {
            std::cerr << "Failed to write updated license" << std::endl;
            return false;
        }

        log("License updated successfully");
        return true;

    } catch (const std::exception& e) {
        std::cerr << "Exception during license update: " << e.what() << std::endl;
        return false;
    }
}

bool LicenseManager::generate_key_pair(const std::string& output_dir, int key_size) {
    try {
        log("Generating RSA key pair...");

        // Create output directory if it doesn't exist
        std::filesystem::create_directories(output_dir);

        std::string private_key_path = output_dir + "/private_key.pem";
        std::string public_key_path = output_dir + "/public_key.pem";

        // Generate RSA key pair using OpenSSL C API
        EVP_PKEY* pkey = nullptr;
        EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr);
        if (!ctx) {
            std::cerr << "Failed to create EVP_PKEY_CTX" << std::endl;
            return false;
        }

        // Initialize key generation
        if (EVP_PKEY_keygen_init(ctx) <= 0) {
            std::cerr << "Failed to initialize key generation" << std::endl;
            EVP_PKEY_CTX_free(ctx);
            return false;
        }

        // Set RSA key size
        if (EVP_PKEY_CTX_set_rsa_keygen_bits(ctx, key_size) <= 0) {
            std::cerr << "Failed to set RSA key size" << std::endl;
            EVP_PKEY_CTX_free(ctx);
            return false;
        }

        // Generate the key
        if (EVP_PKEY_keygen(ctx, &pkey) <= 0) {
            std::cerr << "Failed to generate RSA key" << std::endl;
            EVP_PKEY_CTX_free(ctx);
            return false;
        }

        EVP_PKEY_CTX_free(ctx);

        // Write private key to file
        FILE* private_fp = fopen(private_key_path.c_str(), "wb");
        if (!private_fp) {
            std::cerr << "Failed to open private key file for writing" << std::endl;
            EVP_PKEY_free(pkey);
            return false;
        }

        if (!PEM_write_PrivateKey(private_fp, pkey, nullptr, nullptr, 0, nullptr, nullptr)) {
            std::cerr << "Failed to write private key" << std::endl;
            fclose(private_fp);
            EVP_PKEY_free(pkey);
            return false;
        }
        fclose(private_fp);

        // Write public key to file
        FILE* public_fp = fopen(public_key_path.c_str(), "wb");
        if (!public_fp) {
            std::cerr << "Failed to open public key file for writing" << std::endl;
            EVP_PKEY_free(pkey);
            return false;
        }

        if (!PEM_write_PUBKEY(public_fp, pkey)) {
            std::cerr << "Failed to write public key" << std::endl;
            fclose(public_fp);
            EVP_PKEY_free(pkey);
            return false;
        }
        fclose(public_fp);

        EVP_PKEY_free(pkey);

        log("Key pair generated successfully");
        log("Private key: " + private_key_path);
        log("Public key: " + public_key_path);

        return true;

    } catch (const std::exception& e) {
        std::cerr << "Exception during key generation: " << e.what() << std::endl;
        return false;
    }
}

// Private helper methods
lcxx::crypto::rsa_key_t LicenseManager::load_private_key(const std::string& key_path) {
    try {
        std::string key_content = read_file(key_path);
        if (key_content.empty()) {
            return nullptr;
        }
        return lcxx::crypto::load_key(key_content, lcxx::crypto::key_type::private_key);
    } catch (const std::exception& e) {
        log("Error loading private key: " + std::string(e.what()));
        return nullptr;
    }
}

lcxx::crypto::rsa_key_t LicenseManager::load_public_key(const std::string& key_path) {
    try {
        std::string key_content = read_file(key_path);
        if (key_content.empty()) {
            return nullptr;
        }
        return lcxx::crypto::load_key(key_content, lcxx::crypto::key_type::public_key);
    } catch (const std::exception& e) {
        log("Error loading public key: " + std::string(e.what()));
        return nullptr;
    }
}

std::string LicenseManager::read_file(const std::string& file_path) {
    std::ifstream file(file_path);
    if (!file.is_open()) {
        return "";
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

bool LicenseManager::write_file(const std::string& file_path, const std::string& content) {
    // Create directory if it doesn't exist
    std::filesystem::path path(file_path);
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path());
    }

    std::ofstream file(file_path);
    if (!file.is_open()) {
        return false;
    }
    
    file << content;
    return file.good();
}

bool LicenseManager::is_date_expired(const std::string& date_str) {
    // Parse date in YYYY-MM-DD format
    std::istringstream ss(date_str);
    std::tm tm = {};
    ss >> std::get_time(&tm, "%Y-%m-%d");
    
    if (ss.fail()) {
        return true; // Invalid date is considered expired
    }
    
    auto expiry_time = std::mktime(&tm);
    auto current_time = std::time(nullptr);
    
    return expiry_time < current_time;
}

std::string LicenseManager::get_current_date() {
    std::time_t now = std::time(nullptr);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&now), "%Y-%m-%d %H:%M:%S UTC");
    return ss.str();
}

void LicenseManager::log(const std::string& message) {
    if (verbose_) {
        std::cout << "[LOG] " << message << std::endl;
    }
}

bool LicenseManager::generate_enhanced_license(
    const EnhancedLicenseData& data,
    const std::string& private_key_path,
    const std::string& output_path,
    const std::string& encryption_key
) {
    try {
        log("Starting enhanced license generation...");

        // Load private key
        auto private_key = load_private_key(private_key_path);
        if (!private_key) {
            std::cerr << "Failed to load private key from: " << private_key_path << std::endl;
            return false;
        }

        // Create license object
        lcxx::license license;

        // Add structured license data
        std::string license_id = data.license_id.empty() ? 
            LicenseTypeUtils::generate_license_id() : data.license_id;
        
        license.push_content("license_id", license_id);
        license.push_content("product", data.product);
        license.push_content("version", data.version);
        
        // Device information
        license.push_content("device_hardware_id", data.device.hardware_id);
        license.push_content("device_model", data.device.model);
        license.push_content("device_mac", data.device.mac);
        
        // Customer information
        license.push_content("customer_name", data.customer.name);
        license.push_content("customer_email", data.customer.email);
        
        // Timestamps
        std::string issued_at = data.issued_at.empty() ? 
            LicenseTypeUtils::get_current_iso8601_timestamp() : data.issued_at;
        license.push_content("issued_at", issued_at);
        
        std::string valid_until = data.valid_until.empty() ?
            LicenseTypeUtils::parse_date_to_iso8601_end_of_day("2026-06-30") : 
            LicenseTypeUtils::parse_date_to_iso8601_end_of_day(data.valid_until);
        license.push_content("valid_until", valid_until);
        
        // License tier and type
        license.push_content("license_tier", LicenseTypeUtils::license_tier_to_string(data.license_tier));
        license.push_content("licence_type", LicenseTypeUtils::license_type_to_string(data.licence_type));
        
        // Signature algorithm
        license.push_content("signature_algorithm", LicenseTypeUtils::signature_algorithm_to_string(data.signature_algorithm));

        // Generate signed license JSON
        auto license_json = lcxx::to_json(license, private_key);
        
        // Add the actual signature to the JSON structure (for JSON output)
        if (license_json.contains("signature")) {
            // The signature is already included by lcxx::to_json
            log("Digital signature added to license");
        }
        
        std::string license_string = license_json.dump(4);
        log("Enhanced license generated, size: " + std::to_string(license_string.size()) + " bytes");
        log("License ID: " + license_id);

        // Encrypt if encryption key is provided
        if (!encryption_key.empty()) {
            log("Encrypting license with AES-256...");
            license_string = CryptoUtils::encrypt_aes256(license_string, encryption_key);
            if (license_string.empty()) {
                std::cerr << "Failed to encrypt license" << std::endl;
                return false;
            }
        }

        // Write to file
        if (!write_file(output_path, license_string)) {
            std::cerr << "Failed to write license to: " << output_path << std::endl;
            return false;
        }

        log("Enhanced license written to: " + output_path);
        return true;

    } catch (const std::exception& e) {
        std::cerr << "Exception during enhanced license generation: " << e.what() << std::endl;
        return false;
    }
}

EnhancedLicenseVerificationResult LicenseManager::verify_enhanced_license(
    const std::string& license_path,
    const std::string& public_key_path,
    const std::string& encryption_key,
    bool check_hardware,
    bool check_expiry
) {
    EnhancedLicenseVerificationResult result;
    
    try {
        log("Starting enhanced license verification...");

        // Read license file
        std::string license_content = read_file(license_path);
        if (license_content.empty()) {
            result.error_message = "Failed to read license file: " + license_path;
            return result;
        }

        // Auto-detect and decrypt if content is encrypted
        if (CryptoUtils::is_content_encrypted(license_content)) {
            if (encryption_key.empty()) {
                result.error_message = "License is encrypted but no encryption key provided";
                return result;
            }
            log("Decrypting license...");
            license_content = CryptoUtils::decrypt_aes256(license_content, encryption_key);
            if (license_content.empty()) {
                result.error_message = "Failed to decrypt license file";
                return result;
            }
        }

        // Parse license JSON
        auto [license, signature] = lcxx::from_json(license_content);

        // Load public key for verification
        auto public_key = load_public_key(public_key_path);
        if (!public_key) {
            result.error_message = "Failed to load public key";
            return result;
        }

        // Verify signature
        if (!lcxx::verify_license(license, signature, public_key)) {
            result.error_message = "Invalid license signature";
            return result;
        }

        log("License signature verified successfully");

        // Extract license data
        auto content = license.get_content();
        
        result.license_data.license_id = content.count("license_id") ? content.at("license_id") : "";
        result.license_data.product = content.count("product") ? content.at("product") : "";
        result.license_data.version = content.count("version") ? content.at("version") : "";
        
        // Device info
        result.license_data.device.hardware_id = content.count("device_hardware_id") ? content.at("device_hardware_id") : "";
        result.license_data.device.model = content.count("device_model") ? content.at("device_model") : "";
        result.license_data.device.mac = content.count("device_mac") ? content.at("device_mac") : "";
        
        // Customer info
        result.license_data.customer.name = content.count("customer_name") ? content.at("customer_name") : "";
        result.license_data.customer.email = content.count("customer_email") ? content.at("customer_email") : "";
        
        // Timestamps
        result.license_data.issued_at = content.count("issued_at") ? content.at("issued_at") : "";
        result.license_data.valid_until = content.count("valid_until") ? content.at("valid_until") : "";
        
        // License tier and type
        if (content.count("license_tier")) {
            result.license_data.license_tier = LicenseTypeUtils::string_to_license_tier(content.at("license_tier"));
        }
        if (content.count("licence_type")) {
            result.license_data.licence_type = LicenseTypeUtils::string_to_license_type(content.at("licence_type"));
        }
        if (content.count("signature_algorithm")) {
            result.license_data.signature_algorithm = LicenseTypeUtils::string_to_signature_algorithm(content.at("signature_algorithm"));
        }

        // Check expiry if requested
        if (check_expiry && !result.license_data.valid_until.empty()) {
            // Parse ISO8601 format
            std::tm tm = {};
            std::istringstream ss(result.license_data.valid_until);
            ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
            
            if (!ss.fail()) {
                auto expiry_time = std::mktime(&tm);
                auto current_time = std::time(nullptr);
                
                if (expiry_time < current_time) {
                    result.error_message = "License has expired";
                    return result;
                }
                log("License expiry check passed");
            }
        }

        // Check hardware binding if requested
        if (check_hardware && !result.license_data.device.hardware_id.empty()) {
            std::string current_hw = HardwareFingerprint::generate();
            if (current_hw != result.license_data.device.hardware_id) {
                result.error_message = "Hardware mismatch - license is bound to different hardware";
                return result;
            }
            log("Hardware binding check passed");
        }

        result.valid = true;
        log("Enhanced license verification completed successfully");
        return result;

    } catch (const std::exception& e) {
        result.error_message = "Exception during verification: " + std::string(e.what());
        return result;
    }
}

std::string LicenseManager::extract_license_field(
    const std::string& license_path,
    const std::string& field_name,
    const std::string& public_key_path,
    const std::string& encryption_key
) {
    try {
        log("Extracting field '" + field_name + "' from license...");

        // Read license file
        std::string license_content = read_file(license_path);
        if (license_content.empty()) {
            log("Failed to read license file");
            return "";
        }

        // Auto-detect and decrypt if needed
        if (CryptoUtils::is_content_encrypted(license_content)) {
            if (encryption_key.empty()) {
                log("License is encrypted but no encryption key provided");
                return "";
            }
            license_content = CryptoUtils::decrypt_aes256(license_content, encryption_key);
            if (license_content.empty()) {
                log("Failed to decrypt license file");
                return "";
            }
        }

        // Parse license JSON
        auto [license, signature] = lcxx::from_json(license_content);

        // Verify signature if public key is provided
        if (!public_key_path.empty()) {
            auto public_key = load_public_key(public_key_path);
            if (!public_key || !lcxx::verify_license(license, signature, public_key)) {
                log("License signature verification failed");
                return "";
            }
        }

        // Extract the requested field
        auto content = license.get_content();
        if (content.count(field_name)) {
            log("Field '" + field_name + "' extracted successfully");
            return content.at(field_name);
        }

        log("Field '" + field_name + "' not found in license");
        return "";

    } catch (const std::exception& e) {
        log("Exception during field extraction: " + std::string(e.what()));
        return "";
    }
}

std::map<std::string, std::string> LicenseManager::extract_all_license_fields(
    const std::string& license_path,
    const std::string& public_key_path,
    const std::string& encryption_key
) {
    std::map<std::string, std::string> fields;
    
    try {
        log("Extracting all fields from license...");

        // Read license file
        std::string license_content = read_file(license_path);
        if (license_content.empty()) {
            log("Failed to read license file");
            return fields;
        }

        // Auto-detect and decrypt if needed
        if (CryptoUtils::is_content_encrypted(license_content)) {
            if (encryption_key.empty()) {
                log("License is encrypted but no encryption key provided");
                return fields;
            }
            license_content = CryptoUtils::decrypt_aes256(license_content, encryption_key);
            if (license_content.empty()) {
                log("Failed to decrypt license file");
                return fields;
            }
        }

        // Parse license JSON
        auto [license, signature] = lcxx::from_json(license_content);

        // Verify signature if public key is provided
        if (!public_key_path.empty()) {
            auto public_key = load_public_key(public_key_path);
            if (!public_key || !lcxx::verify_license(license, signature, public_key)) {
                log("License signature verification failed");
                return fields;
            }
        }

        // Extract all fields
        auto content = license.get_content();
        for (const auto& [key, value] : content) {
            fields[key] = value;
        }
        log("Successfully extracted " + std::to_string(fields.size()) + " fields from license");
        return fields;

    } catch (const std::exception& e) {
        log("Exception during field extraction: " + std::string(e.what()));
        return fields;
    }
}
