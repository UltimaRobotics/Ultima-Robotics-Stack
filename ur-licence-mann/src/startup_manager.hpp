#pragma once

#include <string>
#include <vector>
#include <filesystem>

struct StartupPaths {
    std::string keys_dir = "./keys";
    std::string config_dir = "./config";
    std::string licenses_dir = "./licenses";
    std::string definitions_file = "./config/license_definitions.json";
    std::string app_config_file = "./config/app_config.json";
};

class StartupManager {
public:
    explicit StartupManager(bool verbose = false);
    ~StartupManager();

    bool initialize(const StartupPaths& paths);
    
    bool check_and_create_paths();
    bool check_and_create_files();
    
    bool verify_directory(const std::string& dir_path, bool create_if_missing = true);
    bool verify_file(const std::string& file_path, bool create_if_missing = true,
                    const std::string& default_content = "");
    
    const StartupPaths& get_paths() const { return paths_; }
    
    std::vector<std::string> get_missing_paths() const;
    std::vector<std::string> get_missing_files() const;
    
    bool is_initialized() const { return initialized_; }
    
private:
    bool verbose_;
    bool initialized_;
    StartupPaths paths_;
    
    void log(const std::string& message) const;
    std::string get_default_license_definitions_json() const;
    std::string get_default_app_config_json() const;
};
