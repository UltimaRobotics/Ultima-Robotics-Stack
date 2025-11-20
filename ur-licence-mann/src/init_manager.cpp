
#include "init_manager.hpp"
#include "license_manager.hpp"
#include "crypto_utils.hpp"
#include "device_config.h"
#include "hardware_fingerprint.hpp"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <random>
#include <sstream>
#include <iomanip>
#include <nlohmann/json.hpp>
#include <sys/stat.h>
#include <openssl/rand.h>
#include <lcxx/crypto.hpp>

bool InitManager::initialize(PackageConfig& config, bool verbose) {
    if (verbose) {
        std::cout << "Initializing ur-licence-mann system..." << std::endl;
    }

    // Hardware binding is always required (enforced in PackageConfig)

    if (!ensure_directories(config, verbose)) {
        return false;
    }

    if (!ensure_rsa_keys(config, verbose)) {
        return false;
    }

    if (!ensure_encryption_keys(config, verbose)) {
        return false;
    }

    // Ensure license definitions file is encrypted
    if (!ensure_encrypted_license_definitions(config, verbose)) {
        return false;
    }

    // Ensure single license file exists
    if (!ensure_single_license_file(config, verbose)) {
        return false;
    }

    if (verbose) {
        std::cout << "Initialization complete." << std::endl;
        std::cout << "Hardware binding: REQUIRED (enforced)" << std::endl;
    }

    return true;
}

bool InitManager::ensure_directories(const PackageConfig& config, bool verbose) {
    try {
        std::filesystem::create_directories(config.keys_directory);
        std::filesystem::create_directories(config.config_directory);
        std::filesystem::create_directories(config.licenses_directory);

        if (verbose) {
            std::cout << "Directories created/verified" << std::endl;
        }
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Failed to create directories: " << e.what() << std::endl;
        return false;
    }
}

bool InitManager::ensure_encryption_keys(PackageConfig& config, bool verbose) {
    // No longer needed - we use BUILTIN_ENCRYPTION_KEY only
    // This function is kept for compatibility but does nothing
    if (verbose) {
        std::cout << "Using built-in encryption key from device configuration" << std::endl;
    }
    return true;
}

bool InitManager::verify_key_pair_consistency(const std::string& private_key_path, const std::string& public_key_path, bool verbose) {
    try {
        // Try to load both keys
        std::ifstream priv_file(private_key_path);
        std::ifstream pub_file(public_key_path);
        
        if (!priv_file.good() || !pub_file.good()) {
            if (verbose) {
                std::cout << "Key files not accessible for verification" << std::endl;
            }
            return false;
        }
        
        std::stringstream priv_buffer, pub_buffer;
        priv_buffer << priv_file.rdbuf();
        pub_buffer << pub_file.rdbuf();
        priv_file.close();
        pub_file.close();
        
        std::string priv_content = priv_buffer.str();
        std::string pub_content = pub_buffer.str();
        
        // Check if files are empty
        if (priv_content.empty() || pub_content.empty()) {
            if (verbose) {
                std::cout << "Key files are empty" << std::endl;
            }
            return false;
        }
        
        // Verify the keys can be loaded
        auto private_key = lcxx::crypto::load_key(priv_content, lcxx::crypto::key_type::private_key);
        auto public_key = lcxx::crypto::load_key(pub_content, lcxx::crypto::key_type::public_key);
        
        if (!private_key || !public_key) {
            if (verbose) {
                std::cout << "Failed to load key pair - keys may be corrupted" << std::endl;
            }
            return false;
        }
        
        // Test the key pair by signing and verifying a test message
        std::string test_message = "key_consistency_test";
        auto signature = lcxx::crypto::sign(test_message, private_key);
        bool verified = lcxx::crypto::verify_signature(test_message, signature, public_key);
        
        if (!verified) {
            if (verbose) {
                std::cout << "Key pair consistency check failed - private and public keys don't match" << std::endl;
            }
            return false;
        }
        
        if (verbose) {
            std::cout << "RSA key pair consistency verified successfully" << std::endl;
        }
        return true;
        
    } catch (const std::exception& e) {
        if (verbose) {
            std::cout << "Exception during key verification: " << e.what() << std::endl;
        }
        return false;
    }
}

bool InitManager::ensure_rsa_keys(const PackageConfig& config, bool verbose) {
    // Convert to absolute paths
    std::filesystem::path private_key_path = std::filesystem::absolute(config.private_key_file);
    std::filesystem::path public_key_path = std::filesystem::absolute(config.public_key_file);
    
    std::ifstream private_key_file(private_key_path);
    std::ifstream public_key_file(public_key_path);

    bool private_exists = private_key_file.good();
    bool public_exists = public_key_file.good();
    
    private_key_file.close();
    public_key_file.close();

    // Check if both keys exist
    if (private_exists && public_exists) {
        if (verbose) {
            std::cout << "Existing RSA key pair found, verifying consistency..." << std::endl;
        }
        
        // Verify key pair consistency
        if (verify_key_pair_consistency(private_key_path.string(), public_key_path.string(), verbose)) {
            if (verbose) {
                std::cout << "RSA key pair is valid and consistent" << std::endl;
            }
            return true;
        } else {
            std::cerr << "WARNING: Existing RSA key pair is inconsistent or corrupted" << std::endl;
            std::cerr << "Regenerating RSA key pair..." << std::endl;
            // Remove old keys
            try {
                std::filesystem::remove(private_key_path);
                std::filesystem::remove(public_key_path);
            } catch (const std::exception& e) {
                std::cerr << "Warning: Could not remove old keys: " << e.what() << std::endl;
            }
        }
    } else if (private_exists || public_exists) {
        std::cerr << "WARNING: Incomplete RSA key pair found (missing " 
                  << (private_exists ? "public" : "private") << " key)" << std::endl;
        std::cerr << "Regenerating RSA key pair..." << std::endl;
        // Remove incomplete keys
        try {
            if (private_exists) std::filesystem::remove(private_key_path);
            if (public_exists) std::filesystem::remove(public_key_path);
        } catch (const std::exception& e) {
            std::cerr << "Warning: Could not remove incomplete keys: " << e.what() << std::endl;
        }
    }

    if (verbose) {
        std::cout << "Generating new RSA key pair..." << std::endl;
    }

    // Use absolute path for keys directory
    std::filesystem::path abs_keys_dir = std::filesystem::absolute(config.keys_directory);
    
    LicenseManager manager(verbose);
    if (manager.generate_key_pair(abs_keys_dir.string(), 2048)) {
        // Set restrictive permissions on private key
        #ifdef __linux__
        chmod(private_key_path.c_str(), S_IRUSR | S_IWUSR);
        #endif

        if (verbose) {
            std::cout << "RSA key pair generated successfully" << std::endl;
            std::cout << "Private key permissions set to 0600 (owner read/write only)" << std::endl;
        }
        
        // Verify the newly generated keys
        if (!verify_key_pair_consistency(private_key_path.string(), public_key_path.string(), verbose)) {
            std::cerr << "ERROR: Newly generated key pair failed consistency check" << std::endl;
            return false;
        }
        
        return true;
    } else {
        std::cerr << "Failed to generate RSA key pair" << std::endl;
        return false;
    }
}

bool InitManager::encrypt_license_definitions(const PackageConfig& config, bool verbose) {
    std::ifstream defs_file(config.license_definitions_file);
    if (!defs_file.good()) {
        return false;
    }

    try {
        std::stringstream buffer;
        buffer << defs_file.rdbuf();
        defs_file.close();
        std::string plain_content = buffer.str();

        std::string encrypted = CryptoUtils::encrypt_aes256(plain_content, BUILTIN_ENCRYPTION_KEY);

        if (encrypted.empty()) {
            std::cerr << "Failed to encrypt license definitions" << std::endl;
            return false;
        }

        std::ofstream out(config.encrypted_license_definitions_file);
        out << encrypted;
        out.close();

        if (verbose) {
            std::cout << "License definitions encrypted: " << config.encrypted_license_definitions_file << std::endl;
        }
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error encrypting license definitions: " << e.what() << std::endl;
        return false;
    }
}

bool InitManager::ensure_encrypted_license_definitions(const PackageConfig& config, bool verbose) {
    // Use absolute paths
    std::filesystem::path abs_encrypted = std::filesystem::absolute(config.encrypted_license_definitions_file);
    std::filesystem::path abs_plain = std::filesystem::absolute(config.license_definitions_file);
    
    // Check if encrypted file exists
    std::ifstream encrypted_file(abs_encrypted);
    bool encrypted_exists = encrypted_file.good();
    encrypted_file.close();

    // Check if plain file exists
    std::ifstream plain_file(abs_plain);
    bool plain_exists = plain_file.good();
    plain_file.close();

    if (!plain_exists && !encrypted_exists) {
        if (verbose) {
            std::cout << "No license definitions file found, will be created on first use" << std::endl;
        }
        return true;
    }

    // If encrypted file doesn't exist but plain file does, encrypt it
    if (plain_exists && !encrypted_exists) {
        if (verbose) {
            std::cout << "Encrypting license definitions file..." << std::endl;
        }
        return encrypt_license_definitions(config, verbose);
    }

    // If encrypted file exists, verify it can be decrypted
    if (encrypted_exists) {
        if (verbose) {
            std::cout << "Verifying encrypted license definitions file..." << std::endl;
        }
        
        try {
            std::ifstream enc_file(abs_encrypted);
            std::stringstream buffer;
            buffer << enc_file.rdbuf();
            enc_file.close();
            std::string encrypted_content = buffer.str();

            std::string decrypted = CryptoUtils::decrypt_aes256(encrypted_content, BUILTIN_ENCRYPTION_KEY);
            
            if (decrypted.empty()) {
                std::cerr << "Failed to decrypt license definitions, may need re-encryption" << std::endl;
                
                // If plain file exists, re-encrypt it
                if (plain_exists) {
                    if (verbose) {
                        std::cout << "Re-encrypting from plain file..." << std::endl;
                    }
                    return encrypt_license_definitions(config, verbose);
                }
                return false;
            }

            if (verbose) {
                std::cout << "Encrypted license definitions verified successfully" << std::endl;
            }
            
            // Update plain file with decrypted content for consistency
            std::ofstream plain_out(abs_plain);
            plain_out << decrypted;
            plain_out.close();
            
            return true;
        } catch (const std::exception& e) {
            std::cerr << "Error verifying encrypted definitions: " << e.what() << std::endl;
            return false;
        }
    }

    return true;
}

bool InitManager::ensure_single_license_file(const PackageConfig& config, bool verbose) {
    try {
        std::filesystem::path licenses_dir = std::filesystem::absolute(config.licenses_directory);
        std::filesystem::path license_path = licenses_dir / LICENSE_FILE;
        
        if (verbose) {
            std::cout << "Checking for license file: " << license_path << std::endl;
        }
        
        // Find all .lic files in the licenses directory
        std::vector<std::filesystem::path> lic_files;
        for (const auto& entry : std::filesystem::directory_iterator(licenses_dir)) {
            if (entry.path().extension() == ".lic") {
                lic_files.push_back(entry.path());
            }
        }
        
        // If multiple license files exist, clean up
        if (lic_files.size() > 1) {
            if (verbose) {
                std::cout << "Found " << lic_files.size() << " license files, cleaning up..." << std::endl;
            }
            
            // Remove all license files
            for (const auto& lic_file : lic_files) {
                try {
                    std::filesystem::remove(lic_file);
                    if (verbose) {
                        std::cout << "Removed: " << lic_file << std::endl;
                    }
                } catch (const std::exception& e) {
                    std::cerr << "Warning: Could not remove " << lic_file << ": " << e.what() << std::endl;
                }
            }
            
            // Also remove any .enc files
            for (const auto& entry : std::filesystem::directory_iterator(licenses_dir)) {
                if (entry.path().extension() == ".enc") {
                    try {
                        std::filesystem::remove(entry.path());
                        if (verbose) {
                            std::cout << "Removed encrypted file: " << entry.path() << std::endl;
                        }
                    } catch (const std::exception& e) {
                        std::cerr << "Warning: Could not remove " << entry.path() << ": " << e.what() << std::endl;
                    }
                }
            }
        }
        
        // Check if the designated license file exists
        if (!std::filesystem::exists(license_path)) {
            if (verbose) {
                std::cout << "License file not found, creating new license: " << license_path << std::endl;
            }
            
            // Create a new license file
            LicenseManager manager(verbose);
            
            // Prepare license data
            EnhancedLicenseData license_data;
            license_data.product = PRODUCT_NAME;
            license_data.version = PRODUCT_VERSION;
            license_data.device.hardware_id = HardwareFingerprint::generate();
            license_data.device.model = DEVICE_MODEL;
            license_data.device.mac = HardwareFingerprint::read_first_mac_address();
            license_data.customer.name = "System";
            license_data.customer.email = "system@localhost";
            license_data.licence_type = LicenseType::UltimaOpenLicence;
            license_data.license_tier = LicenseTier::OpenUser;
            
            // Generate the license
            if (!manager.generate_enhanced_license(
                license_data,
                config.private_key_file,
                license_path.string(),
                BUILTIN_ENCRYPTION_KEY
            )) {
                std::cerr << "Failed to create license file" << std::endl;
                return false;
            }
            
            if (verbose) {
                std::cout << "License file created successfully: " << license_path << std::endl;
            }
        } else {
            if (verbose) {
                std::cout << "License file exists: " << license_path << std::endl;
            }
            
            // Validate existing license file and reset if needed
            if (!validate_and_reset_license(license_path, config, verbose)) {
                return false;
            }
        }
        
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "Error ensuring single license file: " << e.what() << std::endl;
        return false;
    }
}

bool InitManager::validate_and_reset_license(const std::filesystem::path& license_path, const PackageConfig& config, bool verbose) {
    try {
        if (verbose) {
            std::cout << "Validating license file: " << license_path << std::endl;
        }
        
        // Read and verify the license
        LicenseManager manager(verbose);
        auto verification_result = manager.verify_enhanced_license(license_path.string(), config.private_key_file, BUILTIN_ENCRYPTION_KEY);
        
        if (!verification_result.valid) {
            if (verbose) {
                std::cout << "License verification failed: " << verification_result.error_message << std::endl;
                std::cout << "Resetting license with correct data..." << std::endl;
            }
            return create_valid_license(license_path, config, verbose);
        }
        
        // Extract and validate all license fields
        auto license_data = verification_result.license_data;
        bool needs_reset = false;
        std::vector<std::string> invalid_fields;
        
        // Validate product information
        if (license_data.product.empty() || license_data.product != PRODUCT_NAME) {
            invalid_fields.push_back("product");
            needs_reset = true;
        }
        
        if (license_data.version.empty() || license_data.version != PRODUCT_VERSION) {
            invalid_fields.push_back("version");
            needs_reset = true;
        }
        
        // Validate device information
        if (license_data.device.hardware_id.empty()) {
            invalid_fields.push_back("device_hardware_id");
            needs_reset = true;
        }
        
        if (license_data.device.model.empty() || license_data.device.model != DEVICE_MODEL) {
            invalid_fields.push_back("device_model");
            needs_reset = true;
        }
        
        if (license_data.device.mac.empty()) {
            invalid_fields.push_back("device_mac");
            needs_reset = true;
        }
        
        // Validate customer information
        if (license_data.customer.name.empty()) {
            invalid_fields.push_back("customer_name");
            needs_reset = true;
        }
        
        if (license_data.customer.email.empty()) {
            invalid_fields.push_back("customer_email");
            needs_reset = true;
        }
        
        // Validate license information
        if (license_data.license_id.empty()) {
            invalid_fields.push_back("license_id");
            needs_reset = true;
        }
        
        if (license_data.issued_at.empty()) {
            invalid_fields.push_back("issued_at");
            needs_reset = true;
        }
        
        if (license_data.valid_until.empty()) {
            invalid_fields.push_back("valid_until");
            needs_reset = true;
        }
        
        // Check if hardware binding matches current system
        std::string current_hw_id = HardwareFingerprint::generate();
        std::string current_mac = HardwareFingerprint::read_first_mac_address();
        
        if (license_data.device.hardware_id != current_hw_id) {
            invalid_fields.push_back("device_hardware_id (mismatch)");
            needs_reset = true;
        }
        
        if (license_data.device.mac != current_mac) {
            invalid_fields.push_back("device_mac (mismatch)");
            needs_reset = true;
        }
        
        if (needs_reset) {
            if (verbose) {
                std::cout << "Invalid license fields detected: ";
                for (size_t i = 0; i < invalid_fields.size(); ++i) {
                    if (i > 0) std::cout << ", ";
                    std::cout << invalid_fields[i];
                }
                std::cout << std::endl;
                std::cout << "Resetting license with correct data..." << std::endl;
            }
            return create_valid_license(license_path, config, verbose);
        }
        
        if (verbose) {
            std::cout << "License validation passed - all fields are correct" << std::endl;
        }
        
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "Error validating license: " << e.what() << std::endl;
        if (verbose) {
            std::cout << "Exception occurred, resetting license..." << std::endl;
        }
        return create_valid_license(license_path, config, verbose);
    }
}

bool InitManager::create_valid_license(const std::filesystem::path& license_path, const PackageConfig& config, bool verbose) {
    try {
        if (verbose) {
            std::cout << "Creating valid license file: " << license_path << std::endl;
        }
        
        // Remove existing license file
        if (std::filesystem::exists(license_path)) {
            std::filesystem::remove(license_path);
            if (verbose) {
                std::cout << "Removed invalid license file" << std::endl;
            }
        }
        
        // Create a new license file with correct data
        LicenseManager manager(verbose);
        
        // Prepare license data with all required fields
        EnhancedLicenseData license_data;
        license_data.product = PRODUCT_NAME;
        license_data.version = PRODUCT_VERSION;
        license_data.device.hardware_id = HardwareFingerprint::generate();
        license_data.device.model = DEVICE_MODEL;
        license_data.device.mac = HardwareFingerprint::read_first_mac_address();
        license_data.customer.name = "System";
        license_data.customer.email = "system@localhost";
        license_data.licence_type = LicenseType::UltimaOpenLicence;
        license_data.license_tier = LicenseTier::OpenUser;
        license_data.license_id = LicenseTypeUtils::generate_license_id();
        license_data.issued_at = LicenseTypeUtils::get_current_iso8601_timestamp();
        license_data.valid_until = LicenseTypeUtils::get_expiry_timestamp(365); // 1 year from now
        
        // Generate the license
        if (!manager.generate_enhanced_license(
            license_data,
            config.private_key_file,
            license_path.string(),
            BUILTIN_ENCRYPTION_KEY
        )) {
            std::cerr << "Failed to create valid license file" << std::endl;
            return false;
        }
        
        if (verbose) {
            std::cout << "Valid license file created successfully with all required fields:" << std::endl;
            std::cout << "  Product: " << license_data.product << std::endl;
            std::cout << "  Version: " << license_data.version << std::endl;
            std::cout << "  Device Model: " << license_data.device.model << std::endl;
            std::cout << "  Hardware ID: " << license_data.device.hardware_id << std::endl;
            std::cout << "  MAC Address: " << license_data.device.mac << std::endl;
            std::cout << "  Customer: " << license_data.customer.name << std::endl;
            std::cout << "  Email: " << license_data.customer.email << std::endl;
            std::cout << "  License ID: " << license_data.license_id << std::endl;
            std::cout << "  Issued At: " << license_data.issued_at << std::endl;
            std::cout << "  Valid Until: " << license_data.valid_until << std::endl;
        }
        
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "Error creating valid license: " << e.what() << std::endl;
        return false;
    }
}

std::string InitManager::generate_random_key(size_t length) {
    // Use OpenSSL's cryptographically secure random generator
    std::vector<unsigned char> buffer(length / 2);
    if (RAND_bytes(buffer.data(), buffer.size()) != 1) {
        throw std::runtime_error("Failed to generate secure random key");
    }

    std::stringstream ss;
    for (unsigned char byte : buffer) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
    }

    return ss.str();
}
