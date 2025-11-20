#pragma once

#include <string>
#include <nlohmann/json.hpp>

struct PackageConfig {
    std::string keys_directory = "./keys";
    std::string config_directory = "./config";
    std::string licenses_directory = "./licenses";
    std::string license_definitions_file = "../config/operation/license_definitions.json";
    std::string encrypted_license_definitions_file = "./config/license_definitions.enc";
    std::string encryption_keys_file = "./keys/encryption_keys.json";
    std::string private_key_file = "./keys/private_key.pem";
    std::string public_key_file = "./keys/public_key.pem";

    // Security features - always enabled (hardcoded, not read from JSON)
    bool auto_encrypt_definitions = true;
    bool auto_encrypt_licenses = true;
    bool require_hardware_binding = true;
    bool require_signature = true;

    static PackageConfig from_json(const nlohmann::json& j) {
        PackageConfig config;

        if (j.contains("keys_directory")) config.keys_directory = j["keys_directory"];
        if (j.contains("config_directory")) config.config_directory = j["config_directory"];
        if (j.contains("licenses_directory")) config.licenses_directory = j["licenses_directory"];
        if (j.contains("license_definitions_file")) config.license_definitions_file = j["license_definitions_file"];
        if (j.contains("encrypted_license_definitions_file")) config.encrypted_license_definitions_file = j["encrypted_license_definitions_file"];
        if (j.contains("encryption_keys_file")) config.encryption_keys_file = j["encryption_keys_file"];
        if (j.contains("private_key_file")) config.private_key_file = j["private_key_file"];
        if (j.contains("public_key_file")) config.public_key_file = j["public_key_file"];

        // Security flags are ignored from JSON - always enabled
        // master_encryption_key is no longer supported - only BUILTIN_ENCRYPTION_KEY is used

        return config;
    }

    nlohmann::json to_json() const {
        return nlohmann::json{
            {"keys_directory", keys_directory},
            {"config_directory", config_directory},
            {"licenses_directory", licenses_directory},
            {"license_definitions_file", license_definitions_file},
            {"encrypted_license_definitions_file", encrypted_license_definitions_file},
            {"encryption_keys_file", encryption_keys_file},
            {"private_key_file", private_key_file},
            {"public_key_file", public_key_file}
        };
    }
};