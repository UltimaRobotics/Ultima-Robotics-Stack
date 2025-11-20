#pragma once

#include <string>
#include <map>
#include <vector>
#include <nlohmann/json.hpp>
#include "crypto_utils.hpp"

using json = nlohmann::json;

enum class FeatureStatus {
    UNLIMITED_ACCESS,
    LIMITED_ACCESS,
    LIMITED_ACCESS_V2,
    DISABLED
};

struct FeatureDefinition {
    std::string feature_name;
    FeatureStatus status;
    
    FeatureDefinition() : status(FeatureStatus::DISABLED) {}
    FeatureDefinition(const std::string& name, FeatureStatus s)
        : feature_name(name), status(s) {}
};

struct LicenseFeature {
    std::string license_type;
    std::vector<FeatureDefinition> features;
    
    LicenseFeature() = default;
    LicenseFeature(const std::string& type) : license_type(type) {}
};

class FeatureManager {
public:
    explicit FeatureManager(bool verbose = false);
    ~FeatureManager();

    bool load_definitions(const std::string& file_path);
    bool save_definitions(const std::string& file_path);
    
    bool load_encrypted_definitions(const std::string& encrypted_file_path, 
                                     const std::string& encryption_key);
    bool save_encrypted_definitions(const std::string& encrypted_file_path,
                                     const std::string& encryption_key);
    
    bool add_license_type(const std::string& license_type, 
                         const std::vector<FeatureDefinition>& features);
    bool update_license_type(const std::string& license_type, 
                            const std::vector<FeatureDefinition>& features);
    bool delete_license_type(const std::string& license_type);
    
    bool add_feature_to_license(const std::string& license_type, 
                               const FeatureDefinition& feature);
    bool remove_feature_from_license(const std::string& license_type, 
                                     const std::string& feature_name);
    bool update_feature_in_license(const std::string& license_type,
                                   const std::string& feature_name,
                                   FeatureStatus new_status);
    
    bool has_license_type(const std::string& license_type) const;
    std::vector<std::string> get_license_types() const;
    std::vector<FeatureDefinition> get_features_for_license(const std::string& license_type) const;
    
    void print_license_definitions() const;
    
    json to_json() const;
    bool from_json(const json& j);
    
    static std::string feature_status_to_string(FeatureStatus status);
    static FeatureStatus string_to_feature_status(const std::string& status_str);
    
private:
    bool verbose_;
    std::map<std::string, LicenseFeature> license_definitions_;
    
    void log(const std::string& message) const;
};
