#include "vpn_instance_manager.hpp"
#include "internal/vpn_manager_utils.hpp"
#include <fstream>
#include <iostream>
#include <sstream>

namespace vpn_manager {

bool VPNInstanceManager::loadConfiguration(const std::string& json_config) {
    try {
        json config = json::parse(json_config);

        if (!config.contains("vpn_profiles") || !config["vpn_profiles"].is_array()) {
            std::cerr << "Invalid configuration: missing 'vpn_profiles' array" << std::endl;
            return false;
        }

        {
            std::lock_guard<std::mutex> lock(instances_mutex_);

            for (const auto& profile : config["vpn_profiles"]) {
                VPNInstance instance;

                instance.id = profile.value("id", "");
                instance.name = profile.value("name", "");
                instance.protocol = profile.value("protocol", "");
                instance.type = VPNManagerUtils::parseVPNType(instance.protocol);
                instance.server = profile.value("server", "");
                instance.port = profile.value("port", 0);
                instance.encryption = profile.value("encryption", "");
                instance.auth_method = profile.value("auth_method", "");
                instance.username = profile.value("username", "");
                instance.password = profile.value("password", "");
                instance.created_date = profile.value("created_date", "");
                instance.parsed_config = profile.value("parsed_config", json::object());
                instance.connection_stats = profile.value("connection_stats", json::object());

                if (profile.contains("data_transfer")) {
                    instance.data_transfer.upload_bytes = profile["data_transfer"].value("upload_bytes", 0);
                    instance.data_transfer.download_bytes = profile["data_transfer"].value("download_bytes", 0);
                }

                if (profile.contains("total_data_transferred")) {
                    instance.total_data_transferred.total_bytes = profile["total_data_transferred"].value("total_bytes", 0);
                }

                if (profile.contains("connection_time")) {
                    instance.connection_time.total_seconds = profile["connection_time"].value("total_seconds", 0);
                }

                if (profile.contains("config_file") && profile["config_file"].contains("content")) {
                    instance.config_content = profile["config_file"]["content"].get<std::string>();
                }

                instance.enabled = profile.value("auto_connect", false);
                instance.auto_connect = profile.value("auto_connect", false);
                instance.status = profile.value("status", "Ready");
                instance.last_used = profile.value("last_used", "Never");
                instance.current_state = ConnectionState::INITIAL;
                instance.start_time = 0;
                instance.thread_id = 0;
                instance.should_stop = false;

                if (instance.id.empty() || instance.type == VPNType::UNKNOWN) {
                    std::cerr << "Skipping invalid profile: " << instance.name << std::endl;
                    continue;
                }

                instances_[instance.id] = instance;
            }
        }

        emitEvent("manager", "config_loaded", "Configuration loaded successfully");
        return true;

    } catch (const std::exception& e) {
        std::cerr << "Failed to load configuration: " << e.what() << std::endl;
        return false;
    }
}

bool VPNInstanceManager::loadConfigurationFromFile(const std::string& config_file, const std::string& cache_file) {
    try {
        if (verbose_) {
            json verbose_log;
            verbose_log["type"] = "verbose";
            verbose_log["message"] = "VPNInstanceManager::loadConfigurationFromFile - starting";
            verbose_log["config_file"] = config_file;
            verbose_log["cache_file"] = cache_file;
            std::cout << verbose_log.dump() << std::endl;
        }

        config_file_path_ = config_file;
        cache_file_path_ = cache_file;

        std::ifstream file(config_file);
        if (!file.good()) {
            std::cerr << "Cannot open config file: " << config_file << std::endl;
            return false;
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string config_content = buffer.str();
        file.close();

        if (verbose_) {
            json verbose_log;
            verbose_log["type"] = "verbose";
            verbose_log["message"] = "Config file loaded, parsing JSON";
            verbose_log["content_size"] = config_content.size();
            std::cout << verbose_log.dump() << std::endl;
        }

        if (!loadConfiguration(config_content)) {
            return false;
        }

        if (verbose_) {
            json verbose_log;
            verbose_log["type"] = "verbose";
            verbose_log["message"] = "Configuration parsed successfully";
            std::cout << verbose_log.dump() << std::endl;
        }

        if (!cache_file.empty()) {
            if (verbose_) {
                json verbose_log;
                verbose_log["type"] = "verbose";
                verbose_log["message"] = "Loading cached data";
                verbose_log["cache_file"] = cache_file;
                std::cout << verbose_log.dump() << std::endl;
            }

            std::ifstream cache(cache_file);
            if (cache.good()) {
                json cached_data;
                cache >> cached_data;
                cache.close();

                if (cached_data.contains("instances") && cached_data["instances"].is_array()) {
                    std::lock_guard<std::mutex> lock(instances_mutex_);

                    for (const auto& cached : cached_data["instances"]) {
                        std::string id = cached.value("id", "");
                        auto it = instances_.find(id);

                        if (it != instances_.end()) {
                            it->second.enabled = cached.value("enabled", false);
                            it->second.auto_connect = cached.value("auto_connect", false);
                            it->second.status = cached.value("status", "Ready");
                            it->second.last_used = cached.value("last_used", "Never");

                            if (verbose_) {
                                json verbose_log;
                                verbose_log["type"] = "verbose";
                                verbose_log["message"] = "Applied cached state to instance";
                                verbose_log["instance_id"] = id;
                                verbose_log["enabled"] = it->second.enabled;
                                std::cout << verbose_log.dump() << std::endl;
                            }
                        }
                    }
                }
            }
        }

        if (verbose_) {
            json verbose_log;
            verbose_log["type"] = "verbose";
            verbose_log["message"] = "VPNInstanceManager::loadConfigurationFromFile - complete";
            std::cout << verbose_log.dump() << std::endl;
        }

        return true;

    } catch (const std::exception& e) {
        std::cerr << "Failed to load configuration from file: " << e.what() << std::endl;
        return false;
    }
}

bool VPNInstanceManager::saveCachedData(const std::string& cache_file) {
    try {
        json cached_data;
        json instances_array = json::array();

        {
            std::lock_guard<std::mutex> lock(instances_mutex_);

            for (const auto& [id, inst] : instances_) {
                json cache_entry;
                cache_entry["id"] = inst.id;
                cache_entry["enabled"] = inst.enabled;
                cache_entry["auto_connect"] = inst.auto_connect;
                cache_entry["status"] = inst.status;
                cache_entry["last_used"] = inst.last_used;
                instances_array.push_back(cache_entry);
            }

            cached_data["instances"] = instances_array;
            cached_data["last_saved"] = time(nullptr);
        }

        std::ofstream file(cache_file);
        file << cached_data.dump(2);
        file.close();

        return true;
    } catch (const std::exception& e) {
        std::cerr << "Failed to save cached data: " << e.what() << std::endl;
        return false;
    }
}

bool VPNInstanceManager::saveConfiguration(const std::string& filepath) {
    try {
        json config;
        json profiles_array = json::array();

        {
            std::lock_guard<std::mutex> lock(instances_mutex_);

            for (const auto& [id, inst] : instances_) {
                json profile;
                profile["id"] = inst.id;
                profile["name"] = inst.name;
                profile["protocol"] = inst.protocol;
                profile["server"] = inst.server;
                profile["port"] = inst.port;
                profile["encryption"] = inst.encryption;
                profile["auth_method"] = inst.auth_method;
                profile["username"] = inst.username;
                profile["password"] = inst.password;
                profile["auto_connect"] = inst.auto_connect;
                profile["created_date"] = inst.created_date;
                profile["status"] = inst.status;
                profile["last_used"] = inst.last_used;
                profile["parsed_config"] = inst.parsed_config;
                profile["connection_stats"] = inst.connection_stats;

                json data_transfer;
                data_transfer["upload_bytes"] = inst.data_transfer.upload_bytes;
                data_transfer["download_bytes"] = inst.data_transfer.download_bytes;
                data_transfer["upload_formatted"] = VPNManagerUtils::formatBytes(inst.data_transfer.upload_bytes);
                data_transfer["download_formatted"] = VPNManagerUtils::formatBytes(inst.data_transfer.download_bytes);
                profile["data_transfer"] = data_transfer;

                json total_data;
                total_data["current_session_bytes"] = inst.total_data_transferred.current_session_bytes;
                total_data["total_bytes"] = inst.total_data_transferred.total_bytes;
                total_data["current_session_mb"] = inst.total_data_transferred.current_session_bytes / (1024.0 * 1024.0);
                total_data["total_mb"] = inst.total_data_transferred.total_bytes / (1024.0 * 1024.0);
                profile["total_data_transferred"] = total_data;

                json conn_time;
                conn_time["current_session_seconds"] = inst.connection_time.current_session_seconds;
                conn_time["total_seconds"] = inst.connection_time.total_seconds;
                conn_time["current_session_formatted"] = VPNManagerUtils::formatTime(inst.connection_time.current_session_seconds);
                conn_time["total_formatted"] = VPNManagerUtils::formatTime(inst.connection_time.total_seconds);
                profile["connection_time"] = conn_time;

                json config_file;
                config_file["content"] = inst.config_content;
                profile["config_file"] = config_file;

                profiles_array.push_back(profile);
            }

            config["vpn_profiles"] = profiles_array;
        }

        std::ofstream file(filepath);
        file << config.dump(2);
        file.close();

        return true;
    } catch (const std::exception& e) {
        std::cerr << "Failed to save configuration: " << e.what() << std::endl;
        return false;
    }
}

} // namespace vpn_manager
