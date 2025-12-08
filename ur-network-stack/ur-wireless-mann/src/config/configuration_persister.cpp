
#include "urwt/config/configuration_persister.hpp"
#include <fstream>
#include <filesystem>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <iostream>

namespace urwt {
namespace config {

using json = nlohmann::json;
namespace fs = std::filesystem;

ConfigurationPersister::ConfigurationPersister() = default;
ConfigurationPersister::~ConfigurationPersister() = default;

Result<bool, std::string> ConfigurationPersister::saveConfig(
    const WirelessConfig& config,
    const std::string& path) {
    
    if (fs::exists(path)) {
        auto backupResult = createBackup(path);
        if (backupResult.isError()) {
            std::cerr << "Warning: Failed to create backup: " 
                      << backupResult.error() << std::endl;
        }
    }
    
    json j = config;
    
    j["version"] = "1.0";
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t_now), "%Y-%m-%dT%H:%M:%SZ");
    j["last_updated"] = ss.str();
    
    std::string content = j.dump(2);
    
    return atomicWrite(path, content);
}

Result<WirelessConfig, std::string> ConfigurationPersister::loadConfig(
    const std::string& path) {
    
    if (!fs::exists(path)) {
        return Result<WirelessConfig, std::string>::error(
            "Configuration file does not exist: " + path);
    }
    
    std::ifstream file(path);
    if (!file.is_open()) {
        return Result<WirelessConfig, std::string>::error(
            "Cannot open configuration file: " + path);
    }
    
    json j;
    try {
        file >> j;
    } catch (const json::exception& e) {
        return Result<WirelessConfig, std::string>::error(
            "JSON parse error: " + std::string(e.what()));
    }
    
    if (!j.contains("version")) {
        return Result<WirelessConfig, std::string>::error(
            "Missing version field in configuration");
    }
    
    std::string version = j["version"].get<std::string>();
    if (version != "1.0") {
        return Result<WirelessConfig, std::string>::error(
            "Unsupported configuration version: " + version);
    }
    
    try {
        WirelessConfig config = j.get<WirelessConfig>();
        return Result<WirelessConfig, std::string>::ok(config);
    } catch (const json::exception& e) {
        return Result<WirelessConfig, std::string>::error(
            "Failed to deserialize configuration: " + std::string(e.what()));
    }
}

Result<bool, std::string> ConfigurationPersister::createBackup(
    const std::string& path) {
    
    if (!fs::exists(path)) {
        return Result<bool, std::string>::error(
            "Source file does not exist: " + path);
    }
    
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << path << ".backup." 
       << std::put_time(std::localtime(&time_t_now), "%Y%m%d_%H%M%S");
    std::string backup_path = ss.str();
    
    try {
        fs::copy_file(path, backup_path, 
                      fs::copy_options::overwrite_existing);
    } catch (const fs::filesystem_error& e) {
        return Result<bool, std::string>::error(
            "Failed to create backup: " + std::string(e.what()));
    }
    
    rotateBackups(path, 5);
    
    return Result<bool, std::string>::ok(true);
}

Result<bool, std::string> ConfigurationPersister::restoreBackup(
    const std::string& path, 
    int backupIndex) {
    
    auto backups = listBackups(path);
    
    if (backupIndex < 0 || backupIndex >= static_cast<int>(backups.size())) {
        return Result<bool, std::string>::error(
            "Invalid backup index: " + std::to_string(backupIndex));
    }
    
    std::string backup_path = backups[backupIndex];
    
    try {
        fs::copy_file(backup_path, path, 
                      fs::copy_options::overwrite_existing);
    } catch (const fs::filesystem_error& e) {
        return Result<bool, std::string>::error(
            "Failed to restore backup: " + std::string(e.what()));
    }
    
    return Result<bool, std::string>::ok(true);
}

Result<bool, std::string> ConfigurationPersister::atomicWrite(
    const std::string& path,
    const std::string& content) {
    
    fs::path file_path(path);
    fs::path parent_dir = file_path.parent_path();
    
    if (!parent_dir.empty() && !fs::exists(parent_dir)) {
        try {
            fs::create_directories(parent_dir);
        } catch (const fs::filesystem_error& e) {
            return Result<bool, std::string>::error(
                "Failed to create directory: " + std::string(e.what()));
        }
    }
    
    std::string tmp_path = path + ".tmp";
    std::ofstream tmp_file(tmp_path, std::ios::binary);
    
    if (!tmp_file.is_open()) {
        return Result<bool, std::string>::error(
            "Cannot open temporary file for writing: " + tmp_path);
    }
    
    tmp_file << content;
    tmp_file.close();
    
    if (tmp_file.fail()) {
        fs::remove(tmp_path);
        return Result<bool, std::string>::error(
            "Failed to write to temporary file: " + tmp_path);
    }
    
    try {
        fs::rename(tmp_path, path);
    } catch (const fs::filesystem_error& e) {
        fs::remove(tmp_path);
        return Result<bool, std::string>::error(
            "Failed to rename temporary file: " + std::string(e.what()));
    }
    
    return Result<bool, std::string>::ok(true);
}

std::vector<std::string> ConfigurationPersister::listBackups(
    const std::string& path) {
    
    std::vector<std::string> backups;
    fs::path file_path(path);
    fs::path parent_dir = file_path.parent_path();
    std::string base_name = file_path.filename().string() + ".backup.";
    
    if (!fs::exists(parent_dir)) {
        return backups;
    }
    
    for (const auto& entry : fs::directory_iterator(parent_dir)) {
        if (entry.is_regular_file()) {
            std::string filename = entry.path().filename().string();
            if (filename.find(base_name) == 0) {
                backups.push_back(entry.path().string());
            }
        }
    }
    
    std::sort(backups.begin(), backups.end(), 
              [](const std::string& a, const std::string& b) {
                  return fs::last_write_time(a) > fs::last_write_time(b);
              });
    
    return backups;
}

void ConfigurationPersister::rotateBackups(const std::string& path, 
                                           int max_backups) {
    auto backups = listBackups(path);
    
    if (static_cast<int>(backups.size()) > max_backups) {
        for (size_t i = max_backups; i < backups.size(); ++i) {
            fs::remove(backups[i]);
        }
    }
}

}
}
