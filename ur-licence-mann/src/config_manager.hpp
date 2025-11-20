#pragma once

#include <string>
#include <map>
#include <vector>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

class ConfigManager {
public:
    explicit ConfigManager(bool verbose = false);
    ~ConfigManager();

    bool load_config(const std::string& config_path);
    bool save_config(const std::string& config_path);
    bool is_loaded() const { return config_loaded_; }

    std::string get_value(const std::string& key, const std::string& default_value = "") const;
    void set_value(const std::string& key, const std::string& value);
    
    bool has_key(const std::string& key) const;
    
    std::map<std::string, std::string> get_all_values() const;
    
    json get_json() const { return config_; }
    void set_json(const json& j) { config_ = j; config_loaded_ = true; }
    
private:
    bool verbose_;
    bool config_loaded_;
    json config_;
    std::string config_path_;
    
    void log(const std::string& message) const;
};
