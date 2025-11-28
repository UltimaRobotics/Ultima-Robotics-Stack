#include "vpn_instance_manager.hpp"
#include "internal/vpn_manager_utils.hpp"
#include "../ur-vpn-parser/vpn_parser.hpp"
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
                    
                    // Validate config format using VPNParser during startup
                    if (!instance.config_content.empty()) {
                        try {
                            vpn_parser::VPNParser parser;
                            vpn_parser::ParseResult parse_result = parser.parse(instance.config_content);
                            
                            if (!parse_result.success) {
                                std::cerr << "Config validation failed for instance '" << instance.name 
                                         << "': " << parse_result.error_message << std::endl;
                                std::cerr << "Skipping this instance due to invalid config format" << std::endl;
                                continue;
                            }
                            
                            // Use parser-detected protocol instead of stored protocol if they differ
                            if (!parse_result.protocol_detected.empty() && 
                                parse_result.protocol_detected != instance.protocol) {
                                std::cout << "Protocol mismatch detected for instance '" << instance.name 
                                         << "': stored='" << instance.protocol 
                                         << "', detected='" << parse_result.protocol_detected << "'"
                                         << " - using detected protocol" << std::endl;
                                instance.protocol = parse_result.protocol_detected;
                                instance.type = VPNManagerUtils::parseVPNType(instance.protocol);
                            }
                            
                        } catch (const std::exception& e) {
                            std::cerr << "Parser exception for instance '" << instance.name 
                                     << "': " << e.what() << std::endl;
                            std::cerr << "Skipping this instance due to parser error" << std::endl;
                            continue;
                        }
                    }
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

bool VPNInstanceManager::loadConfigurationFromFile(const std::string& config_file, const std::string& cache_file, const std::string& cleanup_config_file) {
    try {
        if (verbose_) {
            json verbose_log;
            verbose_log["type"] = "verbose";
            verbose_log["message"] = "VPNInstanceManager::loadConfigurationFromFile - starting";
            verbose_log["config_file"] = config_file;
            verbose_log["cache_file"] = cache_file;
            verbose_log["cleanup_config_file"] = cleanup_config_file;
            std::cout << verbose_log.dump() << std::endl;
        }

        config_file_path_ = config_file;
        cache_file_path_ = cache_file;
        cleanup_config_path_ = cleanup_config_file;

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

void VPNInstanceManager::initializeCleanupSystem() {
    // Initialize CleanupCronJob now that config paths are set
    if (!cleanup_cron_job_) {
        cleanup_cron_job_ = std::make_unique<CleanupCronJob>(this, cleanup_tracker_.get(), 
                                                            config_file_path_, routing_rules_file_path_,
                                                            cleanup_config_path_);
        cleanup_cron_job_->start();
        
        if (verbose_) {
            json verbose_log;
            verbose_log["type"] = "verbose";
            verbose_log["message"] = "VPNInstanceManager::initializeCleanupSystem - cleanup cron job initialized";
            verbose_log["cleanup_config_path"] = cleanup_config_path_;
            std::cout << verbose_log.dump() << std::endl;
        }
    }
}

json VPNInstanceManager::purgeCleanup(bool confirm) {
    json result;
    result["type"] = "purge-cleanup";
    
    if (!confirm) {
        result["success"] = false;
        result["error"] = "Confirmation required. Set 'confirm': true to proceed with destructive purge cleanup.";
        return result;
    }
    
    json cleanup_log;
    cleanup_log["type"] = "purge-cleanup";
    cleanup_log["message"] = "Starting comprehensive purge cleanup - this will remove all VPN data";
    std::cout << cleanup_log.dump() << std::endl;
    
    try {
        // Step 1: Stop cleanup cron job temporarily to avoid interference
        bool cron_job_stopped = false;
        if (cleanup_cron_job_) {
            cleanup_cron_job_->stop();
            cron_job_stopped = true;
        }
        
        // Step 2: Stop all running VPN instances
        bool stop_success = stopAll();
        result["instances_stopped"] = stop_success;
        
        // Step 3: Clear all instances from memory
        {
            std::lock_guard<std::mutex> lock(instances_mutex_);
            int instance_count = instances_.size();
            instances_.clear();
            result["instances_cleared"] = instance_count;
        }
        
        // Step 4: Clear all routing rules
        {
            std::lock_guard<std::mutex> lock(routing_mutex_);
            int route_count = routing_rules_.size();
            routing_rules_.clear();
            result["routing_rules_cleared"] = route_count;
        }
        
        // Step 5: Save empty configuration and cache files
        bool config_saved = saveConfiguration(config_file_path_);
        bool cache_saved = saveCachedData(cache_file_path_);
        result["config_saved"] = config_saved;
        result["cache_saved"] = cache_saved;
        
        // Step 6: Clear custom routing rules file if it exists
        bool routing_rules_cleared = true;
        if (!routing_rules_file_path_.empty() && std::filesystem::exists(routing_rules_file_path_)) {
            try {
                std::ofstream file(routing_rules_file_path_);
                file << "{}";  // Write empty JSON object
                routing_rules_cleared = file.good();
            } catch (const std::exception& e) {
                routing_rules_cleared = false;
                std::cerr << "Failed to clear routing rules file: " << e.what() << std::endl;
            }
        }
        result["routing_rules_file_cleared"] = routing_rules_cleared;
        
        // Step 7: Perform aggressive interface cleanup using VPNCleanup
        bool interface_cleanup = VPNCleanup::cleanupAll(true);  // verbose = true
        result["interface_cleanup"] = interface_cleanup;
        
        // Step 8: Restart cleanup cron job if it was running
        if (cron_job_stopped && cleanup_cron_job_) {
            cleanup_cron_job_->start();
        }
        
        // Determine overall success
        bool overall_success = stop_success && config_saved && cache_saved && 
                              routing_rules_cleared && interface_cleanup;
        
        result["success"] = overall_success;
        result["message"] = overall_success ? "Purge cleanup completed successfully" : "Purge cleanup completed with some errors";
        
        json complete_log;
        complete_log["type"] = "purge-cleanup";
        complete_log["message"] = result["message"];
        complete_log["success"] = overall_success;
        std::cout << complete_log.dump() << std::endl;
        
        return result;
        
    } catch (const std::exception& e) {
        result["success"] = false;
        result["error"] = std::string("Purge cleanup failed: ") + e.what();
        
        json error_log;
        error_log["type"] = "purge-cleanup";
        error_log["message"] = "Purge cleanup failed with exception";
        error_log["error"] = e.what();
        std::cerr << error_log.dump() << std::endl;
        
        return result;
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

bool VPNInstanceManager::saveOriginalConfigToCache(const std::string& cache_file, const std::string& instance_name, const std::string& original_config) {
    try {
        json cached_data;
        
        // Try to load existing cache data
        std::ifstream file(cache_file);
        if (file.good()) {
            try {
                file >> cached_data;
            } catch (...) {
                // If file is corrupted or empty, start fresh
                cached_data = json::object();
            }
        }
        file.close();
        
        // Initialize original_configs array if it doesn't exist
        if (!cached_data.contains("original_configs")) {
            cached_data["original_configs"] = json::object();
        }
        
        // Save the original config for this instance
        cached_data["original_configs"][instance_name] = original_config;
        cached_data["last_saved"] = time(nullptr);
        
        // Write back to file
        std::ofstream out_file(cache_file);
        out_file << cached_data.dump(2);
        out_file.close();
        
        if (verbose_) {
            std::cout << json({
                {"type", "verbose"},
                {"message", "VPNInstanceManager::saveOriginalConfigToCache - Original config cached"},
                {"instance_name", instance_name}
            }).dump() << std::endl;
        }
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Failed to save original config to cache: " << e.what() << std::endl;
        return false;
    }
}

std::string VPNInstanceManager::loadOriginalConfigFromCache(const std::string& cache_file, const std::string& instance_name) {
    try {
        json cached_data;
        
        // Load existing cache data
        std::ifstream file(cache_file);
        if (!file.good()) {
            if (verbose_) {
                std::cout << json({
                    {"type", "verbose"},
                    {"message", "VPNInstanceManager::loadOriginalConfigFromCache - Cache file not found"},
                    {"cache_file", cache_file},
                    {"instance_name", instance_name}
                }).dump() << std::endl;
            }
            return "";
        }
        
        try {
            file >> cached_data;
        } catch (const std::exception& e) {
            if (verbose_) {
                std::cout << json({
                    {"type", "verbose"},
                    {"message", "VPNInstanceManager::loadOriginalConfigFromCache - Failed to parse cache file"},
                    {"cache_file", cache_file},
                    {"instance_name", instance_name},
                    {"error", e.what()}
                }).dump() << std::endl;
            }
            return "";
        }
        file.close();
        
        // Check if original_configs exists and contains the instance
        if (!cached_data.contains("original_configs") || !cached_data["original_configs"].contains(instance_name)) {
            if (verbose_) {
                std::cout << json({
                    {"type", "verbose"},
                    {"message", "VPNInstanceManager::loadOriginalConfigFromCache - No original config found for instance"},
                    {"cache_file", cache_file},
                    {"instance_name", instance_name}
                }).dump() << std::endl;
            }
            return "";
        }
        
        std::string original_config = cached_data["original_configs"][instance_name].get<std::string>();
        
        if (verbose_) {
            std::cout << json({
                {"type", "verbose"},
                {"message", "VPNInstanceManager::loadOriginalConfigFromCache - Original config loaded successfully"},
                {"instance_name", instance_name},
                {"config_length", original_config.length()}
            }).dump() << std::endl;
        }
        
        return original_config;
    } catch (const std::exception& e) {
        std::cerr << "Failed to load original config from cache: " << e.what() << std::endl;
        return "";
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
