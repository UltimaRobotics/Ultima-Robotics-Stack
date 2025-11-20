#include "feature_manager.hpp"
#include <fstream>
#include <iostream>
#include <filesystem>

FeatureManager::FeatureManager(bool verbose) : verbose_(verbose) {
}

FeatureManager::~FeatureManager() = default;

bool FeatureManager::load_definitions(const std::string& file_path) {
    try {
        log("Loading feature definitions from: " + file_path);

        if (!std::filesystem::exists(file_path)) {
            std::cerr << "Definitions file does not exist: " << file_path << std::endl;
            return false;
        }

        std::ifstream file(file_path);
        if (!file.is_open()) {
            std::cerr << "Failed to open definitions file: " << file_path << std::endl;
            return false;
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        file.close();
        std::string content = buffer.str();

        json j;
        
        // Try to parse as JSON first (plain text)
        try {
            j = json::parse(content);
        } catch (const json::exception&) {
            // If parsing fails, it might be encrypted - this is handled by caller
            log("File might be encrypted or invalid JSON");
            return false;
        }

        return from_json(j);

    } catch (const std::exception& e) {
        std::cerr << "Error loading definitions: " << e.what() << std::endl;
        return false;
    }
}

bool FeatureManager::save_definitions(const std::string& file_path) {
    try {
        log("Saving feature definitions to: " + file_path);

        std::filesystem::path path(file_path);
        if (path.has_parent_path()) {
            std::filesystem::create_directories(path.parent_path());
        }

        std::ofstream file(file_path);
        if (!file.is_open()) {
            std::cerr << "Failed to open definitions file for writing: " << file_path << std::endl;
            return false;
        }

        json j = to_json();
        file << j.dump(4);

        log("Definitions saved successfully");
        return true;

    } catch (const std::exception& e) {
        std::cerr << "Error saving definitions: " << e.what() << std::endl;
        return false;
    }
}

bool FeatureManager::add_license_type(const std::string& license_type, 
                                      const std::vector<FeatureDefinition>& features) {
    if (has_license_type(license_type)) {
        log("License type already exists: " + license_type);
        return false;
    }

    LicenseFeature license_feature(license_type);
    license_feature.features = features;
    license_definitions_[license_type] = license_feature;

    log("Added license type: " + license_type);
    return true;
}

bool FeatureManager::update_license_type(const std::string& license_type, 
                                         const std::vector<FeatureDefinition>& features) {
    if (!has_license_type(license_type)) {
        log("License type not found: " + license_type);
        return false;
    }

    license_definitions_[license_type].features = features;
    log("Updated license type: " + license_type);
    return true;
}

bool FeatureManager::delete_license_type(const std::string& license_type) {
    if (!has_license_type(license_type)) {
        log("License type not found: " + license_type);
        return false;
    }

    license_definitions_.erase(license_type);
    log("Deleted license type: " + license_type);
    return true;
}

bool FeatureManager::add_feature_to_license(const std::string& license_type, 
                                            const FeatureDefinition& feature) {
    if (!has_license_type(license_type)) {
        log("License type not found: " + license_type);
        return false;
    }

    auto& features = license_definitions_[license_type].features;

    for (const auto& f : features) {
        if (f.feature_name == feature.feature_name) {
            log("Feature already exists in license type: " + feature.feature_name);
            return false;
        }
    }

    features.push_back(feature);
    log("Added feature '" + feature.feature_name + "' to license type: " + license_type);
    return true;
}

bool FeatureManager::remove_feature_from_license(const std::string& license_type, 
                                                 const std::string& feature_name) {
    if (!has_license_type(license_type)) {
        log("License type not found: " + license_type);
        return false;
    }

    auto& features = license_definitions_[license_type].features;
    auto it = std::find_if(features.begin(), features.end(),
                          [&feature_name](const FeatureDefinition& f) {
                              return f.feature_name == feature_name;
                          });

    if (it == features.end()) {
        log("Feature not found: " + feature_name);
        return false;
    }

    features.erase(it);
    log("Removed feature '" + feature_name + "' from license type: " + license_type);
    return true;
}

bool FeatureManager::update_feature_in_license(const std::string& license_type,
                                               const std::string& feature_name,
                                               FeatureStatus new_status) {
    if (!has_license_type(license_type)) {
        log("License type not found: " + license_type);
        return false;
    }

    auto& features = license_definitions_[license_type].features;
    for (auto& f : features) {
        if (f.feature_name == feature_name) {
            f.status = new_status;
            log("Updated feature '" + feature_name + "' status in license type: " + license_type);
            return true;
        }
    }

    log("Feature not found: " + feature_name);
    return false;
}

bool FeatureManager::has_license_type(const std::string& license_type) const {
    return license_definitions_.find(license_type) != license_definitions_.end();
}

std::vector<std::string> FeatureManager::get_license_types() const {
    std::vector<std::string> types;
    for (const auto& pair : license_definitions_) {
        types.push_back(pair.first);
    }
    return types;
}

std::vector<FeatureDefinition> FeatureManager::get_features_for_license(const std::string& license_type) const {
    if (!has_license_type(license_type)) {
        return {};
    }
    return license_definitions_.at(license_type).features;
}

void FeatureManager::print_license_definitions() const {
    std::cout << "\n=== License Definitions ===" << std::endl;

    for (const auto& [license_type, license_feature] : license_definitions_) {
        std::cout << "\nLicense Type: " << license_type << std::endl;
        std::cout << "Features (" << license_feature.features.size() << "):" << std::endl;

        for (const auto& feature : license_feature.features) {
            std::cout << "  - " << feature.feature_name 
                     << " [" << feature_status_to_string(feature.status) << "]" << std::endl;
        }
    }

    std::cout << "==========================\n" << std::endl;
}

json FeatureManager::to_json() const {
    json j = json::array();

    for (const auto& [license_type, license_feature] : license_definitions_) {
        json license_obj;
        license_obj["license_type"] = license_type;

        json features_array = json::array();
        for (const auto& feature : license_feature.features) {
            json feature_obj;
            feature_obj["feature_name"] = feature.feature_name;
            feature_obj["feature_status"] = feature_status_to_string(feature.status);
            features_array.push_back(feature_obj);
        }

        license_obj["features"] = features_array;
        j.push_back(license_obj);
    }

    return j;
}

bool FeatureManager::from_json(const json& j) {
    try {
        license_definitions_.clear();

        if (!j.is_array()) {
            std::cerr << "JSON root must be an array" << std::endl;
            return false;
        }

        for (const auto& license_obj : j) {
            if (!license_obj.contains("license_type") || !license_obj.contains("features")) {
                std::cerr << "Invalid license definition format" << std::endl;
                continue;
            }

            std::string license_type = license_obj["license_type"].get<std::string>();
            LicenseFeature license_feature(license_type);

            const auto& features_array = license_obj["features"];
            if (features_array.is_array()) {
                for (const auto& feature_obj : features_array) {
                    if (!feature_obj.contains("feature_name") || !feature_obj.contains("feature_status")) {
                        continue;
                    }

                    FeatureDefinition feature_def;
                    feature_def.feature_name = feature_obj["feature_name"].get<std::string>();
                    feature_def.status = string_to_feature_status(
                        feature_obj["feature_status"].get<std::string>()
                    );

                    license_feature.features.push_back(feature_def);
                }
            }

            license_definitions_[license_type] = license_feature;
        }

        log("Loaded " + std::to_string(license_definitions_.size()) + " license definitions");
        return true;

    } catch (const json::exception& e) {
        std::cerr << "JSON parsing error: " << e.what() << std::endl;
        return false;
    }
}

std::string FeatureManager::feature_status_to_string(FeatureStatus status) {
    switch (status) {
        case FeatureStatus::UNLIMITED_ACCESS: return "UNLIMITED_ACCESS";
        case FeatureStatus::LIMITED_ACCESS: return "LIMITED_ACCESS";
        case FeatureStatus::LIMITED_ACCESS_V2: return "LIMITED_ACCESS_V2";
        case FeatureStatus::DISABLED: return "DISABLED";
        default: return "DISABLED";
    }
}

FeatureStatus FeatureManager::string_to_feature_status(const std::string& status_str) {
    if (status_str == "UNLIMITED_ACCESS") return FeatureStatus::UNLIMITED_ACCESS;
    if (status_str == "LIMITED_ACCESS") return FeatureStatus::LIMITED_ACCESS;
    if (status_str == "LIMITED_ACCESS_V2") return FeatureStatus::LIMITED_ACCESS_V2;
    return FeatureStatus::DISABLED;
}

bool FeatureManager::load_encrypted_definitions(const std::string& encrypted_file_path, 
                                                        const std::string& encryption_key) {
    try {
        log("Loading encrypted feature definitions from: " + encrypted_file_path);

        if (!std::filesystem::exists(encrypted_file_path)) {
            std::cerr << "Encrypted definitions file does not exist: " << encrypted_file_path << std::endl;
            return false;
        }

        std::ifstream file(encrypted_file_path);
        if (!file.is_open()) {
            std::cerr << "Failed to open encrypted definitions file: " << encrypted_file_path << std::endl;
            return false;
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        file.close();
        std::string encrypted_content = buffer.str();

        // Decrypt the content
        std::string decrypted = CryptoUtils::decrypt_aes256(encrypted_content, encryption_key);
        if (decrypted.empty()) {
            std::cerr << "Failed to decrypt definitions file" << std::endl;
            return false;
        }

        // Parse the decrypted JSON
        json j = json::parse(decrypted);
        return from_json(j);

    } catch (const std::exception& e) {
        std::cerr << "Error loading encrypted definitions: " << e.what() << std::endl;
        return false;
    }
}

bool FeatureManager::save_encrypted_definitions(const std::string& encrypted_file_path,
                                                 const std::string& encryption_key) {
    try {
        log("Saving encrypted feature definitions to: " + encrypted_file_path);

        std::filesystem::path path(encrypted_file_path);
        if (path.has_parent_path()) {
            std::filesystem::create_directories(path.parent_path());
        }

        // Convert to JSON
        json j = to_json();
        std::string json_str = j.dump(4);

        // Encrypt the content
        std::string encrypted = CryptoUtils::encrypt_aes256(json_str, encryption_key);
        if (encrypted.empty()) {
            std::cerr << "Failed to encrypt definitions" << std::endl;
            return false;
        }

        // Save encrypted content
        std::ofstream file(encrypted_file_path);
        if (!file.is_open()) {
            std::cerr << "Failed to open encrypted definitions file for writing: " << encrypted_file_path << std::endl;
            return false;
        }

        file << encrypted;
        log("Encrypted definitions saved successfully");
        return true;

    } catch (const std::exception& e) {
        std::cerr << "Error saving encrypted definitions: " << e.what() << std::endl;
        return false;
    }
}

void FeatureManager::log(const std::string& message) const {
    if (verbose_) {
        std::cout << "[FeatureManager] " << message << std::endl;
    }
}