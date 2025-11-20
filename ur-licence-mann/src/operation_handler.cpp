#include "operation_handler.hpp"
#include "license_manager.hpp"
#include "feature_manager.hpp"
#include "hardware_fingerprint.hpp"
#include "crypto_utils.hpp"
#include "../build/device_config.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <nlohmann/json.hpp>
#include <filesystem>

// Placeholder for the build-time encryption key
// In a real scenario, this would be defined in device_config.h or passed via CMake
#ifndef BUILTIN_ENCRYPTION_KEY
#define BUILTIN_ENCRYPTION_KEY "your_default_build_time_encryption_key_here"
#endif

int OperationHandler::execute(const OperationConfig& op_config, const PackageConfig& pkg_config, bool verbose) {
    switch (op_config.operation) {
        case urlic::OperationType::VERIFY:
            return handle_verify(op_config.parameters, pkg_config, verbose);
        case urlic::OperationType::UPDATE:
            return handle_update(op_config.parameters, pkg_config, verbose);
        case urlic::OperationType::GET_LICENSE_INFO:
            return handle_get_license_info(op_config.parameters, pkg_config, verbose);
        case urlic::OperationType::GET_LICENSE_PLAN:
            return handle_get_license_plan(op_config.parameters, pkg_config, verbose);
        case urlic::OperationType::GET_LICENSE_DEFINITIONS:
            return handle_get_license_definitions(op_config.parameters, pkg_config, verbose);
        case urlic::OperationType::UPDATE_LICENSE_DEFINITIONS:
            return handle_update_license_definitions(op_config.parameters, pkg_config, verbose);
        default:
            std::cerr << "Unknown operation type" << std::endl;
            return 1;
    }
}

int OperationHandler::handle_generate(const std::map<std::string, std::string>& params,
                                      const PackageConfig& pkg_config, bool verbose) {
    try {
        LicenseManager manager(verbose);

        std::string output_file = params.count("output") ? params.at("output") : "./license.lic";

        EnhancedLicenseData license_data;
        license_data.license_id = params.count("license_id") ? params.at("license_id") : "";

        // Build-time constants - CANNOT be overwritten
        license_data.product = PRODUCT_NAME;
        license_data.version = PRODUCT_VERSION;

        // Hardware binding is always required
        license_data.device.hardware_id = params.count("device_hardware_id") ? params.at("device_hardware_id") : HardwareFingerprint::generate();
        license_data.device.model = DEVICE_MODEL; // Build-time constant - CANNOT be overwritten
        license_data.device.mac = params.count("device_mac") ? params.at("device_mac") : HardwareFingerprint::read_first_mac_address();

        // Customer information
        license_data.customer.name = params.count("customer_name") ? params.at("customer_name") : "";
        license_data.customer.email = params.count("customer_email") ? params.at("customer_email") : "";

        // Timestamps
        license_data.issued_at = params.count("issued_at") ? params.at("issued_at") : LicenseTypeUtils::get_current_iso8601_timestamp();

        // Default expiry: 1 year from now if not specified
        if (params.count("valid_until")) {
            license_data.valid_until = params.at("valid_until");
        } else {
            // Calculate 1 year from now
            std::time_t now = std::time(nullptr);
            std::time_t future = now + (DEFAULT_EXPIRY_YEARS * 365 * 24 * 60 * 60);
            std::tm* future_tm = std::gmtime(&future);
            std::stringstream ss;
            ss << std::put_time(future_tm, "%Y-%m-%dT%H:%M:%SZ");
            license_data.valid_until = ss.str();
        }

        // License tier - Default to OpenUser
        if (params.count("license_tier")) {
            license_data.license_tier = LicenseTypeUtils::string_to_license_tier(params.at("license_tier"));
        } else {
            license_data.license_tier = LicenseTier::OpenUser;
        }

        // License type is ALWAYS UltimaOpenLicence for generation
        // User-provided licence_type parameter is ignored during generation
        // Can only be changed via update operation
        license_data.licence_type = LicenseTypeUtils::string_to_license_type(DEFAULT_LICENSE_TYPE);

        if (params.count("signature_algorithm")) {
            license_data.signature_algorithm = LicenseTypeUtils::string_to_signature_algorithm(params.at("signature_algorithm"));
        }

        // Always use build-time encryption key
        std::string encryption_key = BUILTIN_ENCRYPTION_KEY;

        if (manager.generate_enhanced_license(license_data, pkg_config.private_key_file,
                                              output_file, encryption_key)) {
            std::cout << "License generated successfully: " << output_file << std::endl;

            // Always encrypt the generated license when auto_encrypt_licenses is enabled
            if (pkg_config.auto_encrypt_licenses) {
                std::string encrypted_output_file = output_file + ".enc";
                if (CryptoUtils::encrypt_file_aes256(output_file, encrypted_output_file, encryption_key)) {
                    std::cout << "License encrypted successfully: " << encrypted_output_file << std::endl;
                } else {
                    std::cerr << "Failed to encrypt license." << std::endl;
                    return 1;
                }
            }

            return 0;
        } else {
            std::cerr << "Failed to generate license" << std::endl;
            return 1;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error generating license: " << e.what() << std::endl;
        return 1;
    }
}

int OperationHandler::handle_verify(const std::map<std::string, std::string>& params,
                                    const PackageConfig& pkg_config, bool verbose) {
    try {
        LicenseManager manager(verbose);

        // Use predefined license file path from package config and device config
        std::filesystem::path licenses_dir = std::filesystem::absolute(pkg_config.licenses_directory);
        std::filesystem::path license_path = licenses_dir / LICENSE_FILE;
        std::string license_file = license_path.string();
        
        if (verbose) {
            std::cerr << "Using predefined license file: " << license_file << std::endl;
        }

        // Always use build-time encryption key for verification if auto_encrypt_licenses is true
        std::string encryption_key = pkg_config.auto_encrypt_licenses ? BUILTIN_ENCRYPTION_KEY : "";
        bool check_hardware = pkg_config.require_hardware_binding; // Hardware binding is always required
        bool check_expiry = params.count("check_expiry") ? (params.at("check_expiry") == "true") : true;

        auto result = manager.verify_license(license_file, pkg_config.public_key_file,
                                             encryption_key, check_hardware, check_expiry);

        if (result.valid) {
            std::cout << "License is VALID" << std::endl;
            std::cout << "User: " << result.license_data.user_name << std::endl;
            std::cout << "Email: " << result.license_data.user_email << std::endl;
            std::cout << "Expires: " << result.license_data.expiry_date << std::endl;
            return 0;
        } else {
            std::cerr << "License is INVALID: " << result.error_message << std::endl;
            return 1;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error verifying license: " << e.what() << std::endl;
        return 1;
    }
}

int OperationHandler::handle_update(const std::map<std::string, std::string>& params,
                                    const PackageConfig& pkg_config, bool verbose) {
    try {
        LicenseManager manager(verbose);

        std::string input_file = params.count("input_file") ? params.at("input_file") : "";
        std::string output_file = params.count("output_file") ? params.at("output_file") : input_file;
        std::string new_expiry = params.count("new_expiry") ? params.at("new_expiry") : "";

        if (input_file.empty()) {
            std::cerr << "Input license file not specified" << std::endl;
            return 1;
        }

        std::map<std::string, std::string> updates;
        for (const auto& [key, value] : params) {
            if (key != "input_file" && key != "output_file" && key != "new_expiry") {
                // Update operation allows changing licence_type
                updates[key] = value;
            }
        }

        // Always use build-time encryption key for update
        std::string encryption_key = BUILTIN_ENCRYPTION_KEY;

        if (manager.update_license(input_file, output_file, pkg_config.public_key_file,
                                  pkg_config.private_key_file, encryption_key, new_expiry, updates)) {
            std::cout << "License updated successfully: " << output_file << std::endl;

            // Log if licence_type was changed
            if (updates.count("licence_type") && verbose) {
                std::cout << "License type changed to: " << updates.at("licence_type") << std::endl;
            }

            // Ensure the updated license is encrypted if auto_encrypt_licenses is enabled
            if (pkg_config.auto_encrypt_licenses) {
                std::string encrypted_output_file = output_file + ".enc";
                if (CryptoUtils::encrypt_file_aes256(output_file, encrypted_output_file, encryption_key)) {
                    std::cout << "License encrypted successfully: " << encrypted_output_file << std::endl;
                    // Optionally remove the unencrypted file
                    // std::remove(output_file.c_str());
                } else {
                    std::cerr << "Failed to encrypt updated license." << std::endl;
                    return 1;
                }
            }

            return 0;
        } else {
            std::cerr << "Failed to update license" << std::endl;
            return 1;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error updating license: " << e.what() << std::endl;
        return 1;
    }
}

int OperationHandler::handle_get_license_info(const std::map<std::string, std::string>& params,
                                              const PackageConfig& pkg_config, bool verbose) {
    try {
        LicenseManager manager(verbose);

        // Use predefined license file path from package config and device config
        std::filesystem::path licenses_dir = std::filesystem::absolute(pkg_config.licenses_directory);
        std::filesystem::path license_path = licenses_dir / LICENSE_FILE;
        std::string license_file = license_path.string();
        
        if (verbose) {
            std::cerr << "Using predefined license file: " << license_file << std::endl;
        }

        // Always use build-time encryption key for extracting license info
        std::string encryption_key = BUILTIN_ENCRYPTION_KEY;

        auto fields = manager.extract_all_license_fields(license_file, pkg_config.public_key_file, encryption_key);

        if (fields.empty()) {
            std::cerr << "Failed to extract license information or license is invalid" << std::endl;
            return 1;
        }

        nlohmann::json output;
        for (const auto& [key, value] : fields) {
            output[key] = value;
        }

        std::cout << output.dump(2) << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error getting license info: " << e.what() << std::endl;
        return 1;
    }
}

int OperationHandler::handle_get_license_plan(const std::map<std::string, std::string>& params,
                                              const PackageConfig& pkg_config, bool verbose) {
    try {
        // Use predefined license file path from package config and device config
        std::filesystem::path licenses_dir = std::filesystem::absolute(pkg_config.licenses_directory);
        std::filesystem::path license_path = licenses_dir / LICENSE_FILE;
        std::string license_file = license_path.string();
        
        if (verbose) {
            std::cerr << "Using predefined license file: " << license_file << std::endl;
        }

        LicenseManager manager(verbose);
        // Always use build-time encryption key for extracting license plan
        std::string encryption_key = BUILTIN_ENCRYPTION_KEY;

        auto fields = manager.extract_all_license_fields(license_file, pkg_config.public_key_file, encryption_key);

        if (fields.empty()) {
            std::cerr << "Failed to extract license plan information" << std::endl;
            return 1;
        }

        nlohmann::json plan_info;
        plan_info["license_type"] = fields.count("licence_type") ? fields.at("licence_type") : "Unknown";
        plan_info["license_tier"] = fields.count("license_tier") ? fields.at("license_tier") : "Unknown";
        plan_info["product"] = fields.count("product") ? fields.at("product") : "Unknown";
        plan_info["version"] = fields.count("version") ? fields.at("version") : "Unknown";
        plan_info["expiry"] = fields.count("valid_until") ? fields.at("valid_until") : "Unknown";

        std::cout << plan_info.dump(2) << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error getting license plan: " << e.what() << std::endl;
        return 1;
    }
}

int OperationHandler::handle_get_license_definitions(const std::map<std::string, std::string>& /* params */,
                                                     const PackageConfig& pkg_config, bool verbose) {
    try {
        FeatureManager feature_mgr(verbose);

        // Use absolute paths
        std::filesystem::path abs_defs_file = std::filesystem::absolute(pkg_config.license_definitions_file);
        std::filesystem::path abs_encrypted_file = std::filesystem::absolute(pkg_config.encrypted_license_definitions_file);

        // Always attempt to use the encrypted file if auto_encrypt_definitions is true
        if (pkg_config.auto_encrypt_definitions) {
            std::ifstream encrypted_file_stream(abs_encrypted_file);
            if (encrypted_file_stream.good()) {
                encrypted_file_stream.close();
                std::string encrypted_content;
                std::ifstream enc_in(abs_encrypted_file);
                std::stringstream buffer;
                buffer << enc_in.rdbuf();
                encrypted_content = buffer.str();
                enc_in.close();

                // Always use build-time encryption key for definitions
                std::string encryption_key = BUILTIN_ENCRYPTION_KEY;
                std::string decrypted = CryptoUtils::decrypt_aes256(encrypted_content, encryption_key);

                if (!decrypted.empty()) {
                    auto json_data = nlohmann::json::parse(decrypted);
                    std::cout << json_data.dump(2) << std::endl;
                    return 0;
                } else {
                    std::cerr << "Failed to decrypt license definitions." << std::endl;
                    return 1;
                }
            } else {
                std::cerr << "Encrypted license definitions file not found: " << abs_encrypted_file << std::endl;
                // If encrypted file is missing but auto_encrypt_definitions is true, this is an error state.
                return 1;
            }
        }

        // If auto_encrypt_definitions is false, read from the plain definitions file
        std::cout << "\n=== License Definitions ===\n";

        std::ifstream def_file(abs_defs_file);

        if (!def_file.is_open()) {
             std::cerr << "Failed to open license definitions file: " << abs_defs_file << std::endl;
             return 1;
        }
        def_file.close(); // Close after checking if it's open

        if (!feature_mgr.load_definitions(abs_defs_file.string())) {
            std::cerr << "Failed to load license definitions" << std::endl;
            return 1;
        }

        auto json_data = feature_mgr.to_json();
        std::cout << json_data.dump(2) << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error getting license definitions: " << e.what() << std::endl;
        return 1;
    }
}

int OperationHandler::handle_update_license_definitions(const std::map<std::string, std::string>& params,
                                                        const PackageConfig& pkg_config, bool verbose) {
    try {
        FeatureManager feature_mgr(verbose);

        // Use absolute paths
        std::filesystem::path abs_defs_file = std::filesystem::absolute(pkg_config.license_definitions_file);
        std::filesystem::path abs_encrypted_file = std::filesystem::absolute(pkg_config.encrypted_license_definitions_file);

        std::string definitions_content;

        if (params.count("definitions_json")) {
            definitions_content = params.at("definitions_json");
        } else if (params.count("definitions_file")) {
            std::string input_defs_file = params.at("definitions_file");
            std::ifstream input_file_stream(input_defs_file);
            if (!input_file_stream.is_open()) {
                std::cerr << "Failed to open definitions file for update: " << input_defs_file << std::endl;
                return 1;
            }
            std::stringstream buffer;
            buffer << input_file_stream.rdbuf();
            definitions_content = buffer.str();
            input_file_stream.close();
        } else {
            std::cerr << "No definitions content or file provided for update." << std::endl;
            return 1;
        }

        // Parse the provided definitions to ensure it's valid JSON
        nlohmann::json json_data;
        try {
            json_data = nlohmann::json::parse(definitions_content);
        } catch (const nlohmann::json::parse_error& e) {
            std::cerr << "Invalid JSON provided for definitions update: " << e.what() << std::endl;
            return 1;
        }

        // Always use build-time encryption key for definitions
        std::string encryption_key = BUILTIN_ENCRYPTION_KEY;

        if (pkg_config.auto_encrypt_definitions) {
            std::string encrypted_content = CryptoUtils::encrypt_aes256(json_data.dump(), encryption_key);
            if (encrypted_content.empty()) {
                std::cerr << "Failed to encrypt license definitions." << std::endl;
                return 1;
            }

            std::ofstream encrypted_output_file(abs_encrypted_file);
            if (!encrypted_output_file.is_open()) {
                std::cerr << "Failed to open encrypted definitions file for writing: " << abs_encrypted_file << std::endl;
                return 1;
            }
            encrypted_output_file << encrypted_content;
            encrypted_output_file.close();
            std::cout << "License definitions updated and encrypted successfully: " << abs_encrypted_file << std::endl;
        } else {
            std::ofstream output_file_stream(abs_defs_file);
            if (!output_file_stream.is_open()) {
                std::cerr << "Failed to open definitions file for writing: " << abs_defs_file << std::endl;
                return 1;
            }
            output_file_stream << json_data.dump(2); // Write pretty-printed JSON
            output_file_stream.close();
            std::cout << "License definitions updated successfully: " << abs_defs_file << std::endl;
        }

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Error updating license definitions: " << e.what() << std::endl;
        return 1;
    }
}