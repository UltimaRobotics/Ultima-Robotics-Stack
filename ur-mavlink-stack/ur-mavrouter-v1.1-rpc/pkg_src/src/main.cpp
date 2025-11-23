/*
 * This file is part of the MAVLink Router project
 *
 * Copyright (C) 2016  Intel Corporation. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <assert.h>
#include <climits>
#include <dirent.h>
#include <getopt.h>
#include <iostream>
#include <signal.h>
#include <string>
#include <sys/stat.h>
#include <thread>
#include <chrono>
#include <vector>
#include <map>
#include <atomic>

#include "common/conf_file.h"
#include "common/dbg.h"
#include "common/log.h"
#include "common/util.h"
#include <fstream>
#include <nlohmann/json.hpp>

#include "endpoint.h"
#include "logendpoint.h"
#include "mainloop.h"
#include "common/json_config.h"
#include "ThreadManager.hpp"
#include "mavlink-extensions/ExtensionManager.h"
#include "rpc-mechanisms/RpcControllerNew.hpp"

#ifdef _BUILD_HTTP
#include "HttpServer.hpp"
#endif

#define DEFAULT_CONFFILE "/etc/mavlink-router/main.conf"
#define DEFAULT_CONF_DIR "/etc/mavlink-router/config.d"
#define EXTENSION_CONFIG_DIR "pkg_src/config" // Default directory for extension configs

extern const char *BUILD_VERSION;

// Global shutdown flag for graceful termination
static std::atomic<bool> shutdown_requested{false};

static const struct option long_options[] = {{"package-config", required_argument, nullptr, 'P'},
                                             {"rpc-config", required_argument, nullptr, 'R'},
                                             {"help", no_argument, nullptr, 'h'},
                                             {"version", no_argument, nullptr, 'V'},
                                             {}};

static const char *short_options = "hP:R:V";

static void help(FILE *fp)
{
    fprintf(
        fp,
        "%s --package-config <file> --rpc-config <file> [OPTIONS...]\n\n"
        "  -P --package-config <file>   REQUIRED: JSON package configuration file containing paths to all config files\n"
        "  -R --rpc-config <file>       REQUIRED: JSON file with RPC client configurations\n"
        "  -h --help                    Print this message\n"
        "  -V --version                 Show version\n",
        program_invocation_short_name);
}

static uint32_t find_next_udp_port(const std::string &ip, const Configuration &config)
{
    uint32_t port = 14550U;

    for (const auto &c : config.udp_configs) {
        if (ip == c.address && port == c.port) {
            port++;
            break;
        }
    }

    return port;
}

static int split_on_last_colon(const char *str, char **base, unsigned long *number)
{
    char *colonstr;

    *base = strdup(str);
    colonstr = strrchr(*base, ':');
    *number = ULONG_MAX;

    if (colonstr != nullptr) {
        *colonstr = '\0';
        if (safe_atoul(colonstr + 1, number) < 0) {
            free(*base);
            return -EINVAL;
        }
    }

    return 0;
}

static Log::Level log_level_from_str(const char *str)
{
    if (strcaseeq(str, "error")) {
        return Log::Level::ERROR;
    }
    if (strcaseeq(str, "warning")) {
        return Log::Level::WARNING;
    }
    if (strcaseeq(str, "info")) {
        return Log::Level::INFO;
    }
    if (strcaseeq(str, "debug")) {
        return Log::Level::DEBUG;
    }
    if (strcaseeq(str, "trace")) {
        return Log::Level::TRACE;
    }

    throw std::invalid_argument("log_level_from_str: unkown string value");
}

static bool validate_file_exists(const std::string& filepath, const std::string& description, bool is_optional = false) {
    struct stat buffer;
    bool exists = (stat(filepath.c_str(), &buffer) == 0);
    
    if (exists) {
        log_info("Package config validation - %s: EXISTS (%s)", description.c_str(), filepath.c_str());
        log_debug("Package config validation - %s: File size: %ld bytes, Last modified: %ld", 
                  description.c_str(), buffer.st_size, buffer.st_mtime);
        return true;
    } else {
        if (is_optional) {
            log_info("Package config validation - %s: NOT FOUND (optional, will be created if needed) (%s)", 
                     description.c_str(), filepath.c_str());
            return true; // Optional files don't cause failure
        } else {
            log_error("Package config validation - %s: MISSING (%s)", description.c_str(), filepath.c_str());
            return false;
        }
    }
}

static int parse_package_config(const std::string& package_config_path, Configuration& config) {
    log_info("Package config parsing - Starting to parse package configuration file: %s", package_config_path.c_str());
    
    // Validate package config file exists
    if (!validate_file_exists(package_config_path, "Package config file")) {
        log_error("Package config parsing - Failed: Package config file does not exist");
        return -1;
    }
    
    try {
        // Read and parse the package config JSON file
        std::ifstream package_file(package_config_path);
        if (!package_file.is_open()) {
            log_error("Package config parsing - Failed: Cannot open package config file for reading");
            return -1;
        }
        
        nlohmann::json package_json;
        package_file >> package_json;
        package_file.close();
        
        log_info("Package config parsing - Successfully parsed JSON structure");
        std::string version = package_json.value("version", "unknown");
        std::string description = package_json.value("description", "no description");
        log_debug("Package config parsing - Version: %s, Description: %s", 
                  version.c_str(), description.c_str());
        
        // Process config_files section
        if (package_json.contains("config_files")) {
            log_info("Package config parsing - Processing config_files section");
            auto config_files = package_json["config_files"];
            
            // Router config
            if (config_files.contains("router_config")) {
                std::string router_config_path = config_files["router_config"];
                log_info("Package config parsing - Found router_config: %s", router_config_path.c_str());
                
                if (validate_file_exists(router_config_path, "Router config file")) {
                    config.json_conf_file = router_config_path;
                    log_info("Package config parsing - Router config assigned to configuration");
                } else {
                    log_warning("Package config parsing - Router config file not found, skipping");
                }
            }
            
            // HTTP server config
            if (config_files.contains("http_server_config")) {
                std::string http_config_path = config_files["http_server_config"];
                log_info("Package config parsing - Found http_server_config: %s", http_config_path.c_str());
                
                if (validate_file_exists(http_config_path, "HTTP server config file")) {
                    config.http_conf_file = http_config_path;
                    log_info("Package config parsing - HTTP server config assigned to configuration");
                } else {
                    log_warning("Package config parsing - HTTP server config file not found, HTTP server will be disabled");
                }
            }
            
            // Statistics config
            if (config_files.contains("statistics_config")) {
                std::string stats_config_path = config_files["statistics_config"];
                log_info("Package config parsing - Found statistics_config: %s", stats_config_path.c_str());
                
                if (validate_file_exists(stats_config_path, "Statistics config file")) {
                    config.stats_conf_file = stats_config_path;
                    log_info("Package config parsing - Statistics config assigned to configuration");
                } else {
                    log_warning("Package config parsing - Statistics config file not found, statistics will be disabled");
                }
            }
            
            log_info("Package config parsing - Completed config_files section processing");
        } else {
            log_warning("Package config parsing - No config_files section found in package config");
        }
        
        // Process log_files section (for logging purposes only)
        if (package_json.contains("log_files")) {
            log_info("Package config parsing - Processing log_files section");
            auto log_files = package_json["log_files"];
            
            for (auto& [key, path] : log_files.items()) {
                std::string log_desc = "Log file (" + key + ")";
                validate_file_exists(path.get<std::string>(), log_desc, true); // Mark as optional
            }
            
            log_info("Package config parsing - Completed log_files section processing");
        } else {
            log_warning("Package config parsing - No log_files section found in package config");
        }
        
        // Process runtime section (for logging purposes only)
        if (package_json.contains("runtime")) {
            log_info("Package config parsing - Processing runtime section");
            auto runtime = package_json["runtime"];
            
            if (runtime.contains("pid_file")) {
                std::string pid_file_path = runtime["pid_file"];
                validate_file_exists(pid_file_path, "PID file directory", true); // Mark as optional
            }
            
            std::string user = runtime.value("user", "default");
            std::string group = runtime.value("group", "default");
            log_info("Package config parsing - Runtime configuration: user=%s, group=%s", 
                     user.c_str(), group.c_str());
            log_info("Package config parsing - Completed runtime section processing");
        } else {
            log_warning("Package config parsing - No runtime section found in package config");
        }
        
        log_info("Package config parsing - Successfully completed package configuration processing");
        return 0;
        
    } catch (const nlohmann::json::parse_error& e) {
        log_error("Package config parsing - JSON parse error: %s", e.what());
        return -1;
    } catch (const std::exception& e) {
        log_error("Package config parsing - Unexpected error: %s", e.what());
        return -1;
    }
}

static bool handle_early_args(int argc, char *argv[])
{
    int c;
    
    // Only check for help and version, don't consume other arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            help(stdout);
            return false;
        }
        if (strcmp(argv[i], "-V") == 0 || strcmp(argv[i], "--version") == 0) {
            printf(PACKAGE " version %s\n", BUILD_VERSION);
            return false;
        }
    }
    
    return true;
}

static bool pre_parse_argv(int argc, char *argv[], Configuration &config)
{
    // This function parses package config and RPC config from
    // command line, so we can read the configuration files.
    // parse_argv will then parse remaining options.

    int c;

    while ((c = getopt_long(argc, argv, short_options, long_options, nullptr)) >= 0) {
        switch (c) {
        case 'P': {
            config.package_config_file = optarg;
            break;
        }
        case 'R': {
            config.rpc_config_file = optarg;
            break;
        }
        }
    }

    // Reset optind to 1, to allow argv to be parsed again
    optind = 1;

    if (config.package_config_file.empty()) {
        log_error("Package configuration file is required. Use -P --package-config <file>");
        return false;
    }

    if (config.rpc_config_file.empty()) {
        log_error("RPC configuration file is required. Use -R --rpc-config <file>");
        return false;
    }

    return true;
}

static int parse_argv(int argc, char *argv[], Configuration &config)
{
    int c;

    assert(argc >= 0);
    assert(argv);

    while ((c = getopt_long(argc, argv, short_options, long_options, nullptr)) >= 0) {
        switch (c) {
        case 'P': {
            config.package_config_file = optarg;
            break;
        }
        case 'R': {
            config.rpc_config_file = optarg;
            break;
        }
        default:
            help(stderr);
            return -EINVAL;
        }
    }

    return 1;
}

static std::string get_conf_file_name(const Configuration &config)
{
    char *s;

    if (!config.conf_file_name.empty()) {
        return config.conf_file_name;
    }

    s = getenv("MAVLINK_ROUTERD_CONF_FILE");
    if (s != nullptr) {
        return std::string(s);
    }

    return DEFAULT_CONFFILE;
}

static std::string get_conf_dir(const Configuration &config)
{
    char *s;

    if (!config.conf_dir.empty()) {
        return config.conf_dir;
    }

    s = getenv("MAVLINK_ROUTERD_CONF_DIR");
    if (s != nullptr) {
        return std::string(s);
    }

    return DEFAULT_CONF_DIR;
}

#define MAX_LOG_LEVEL_SIZE 10
static int parse_log_level(const char *val, size_t val_len, void *storage, size_t storage_len)
{
    assert(val);
    assert(storage);
    assert(val_len);

    if (storage_len < sizeof(Configuration::debug_log_level)) {
        return -ENOBUFS;
    }
    if (val_len > MAX_LOG_LEVEL_SIZE) {
        return -EINVAL;
    }

    const char *log_level = strndupa(val, val_len);
    try {
        auto *level = (Log::Level *)storage;
        *level = log_level_from_str(log_level);
    } catch (const std::exception &e) {
        log_error("Invalid argument for DebugLogLevel = %s", log_level);
        return -EINVAL;
    }

    return 0;
}
#undef MAX_LOG_LEVEL_SIZE

static int parse_confs(ConfFile &conffile, Configuration &config)
{
    int ret;
    size_t offset;
    struct ConfFile::section_iter iter;
    // clang-format off
    static const ConfFile::OptionsTable global_option_table[] = {
        {"TcpServerPort",       false, ConfFile::parse_ul,      OPTIONS_TABLE_STRUCT_FIELD(Configuration, tcp_port)},
        {"ReportStats",         false, ConfFile::parse_bool,    OPTIONS_TABLE_STRUCT_FIELD(Configuration, report_msg_statistics)},
        {"DebugLogLevel",       false, parse_log_level,         OPTIONS_TABLE_STRUCT_FIELD(Configuration, debug_log_level)},
        {"DeduplicationPeriod", false, ConfFile::parse_ul,      OPTIONS_TABLE_STRUCT_FIELD(Configuration, dedup_period_ms)},
        {"SnifferSysid",    false, ConfFile::parse_ul,      OPTIONS_TABLE_STRUCT_FIELD(Configuration, sniffer_sysid)},
        {"JsonConfFile",        false, ConfFile::parse_stdstring,  OPTIONS_TABLE_STRUCT_FIELD(Configuration, json_conf_file)},
        {"StatsConfFile",       false, ConfFile::parse_stdstring,  OPTIONS_TABLE_STRUCT_FIELD(Configuration, stats_conf_file)},
        {"HttpConfFile",        false, ConfFile::parse_stdstring,  OPTIONS_TABLE_STRUCT_FIELD(Configuration, http_conf_file)},
        {"ExtensionConfDir",    false, ConfFile::parse_stdstring,  OPTIONS_TABLE_STRUCT_FIELD(Configuration, extension_conf_dir)},
        {}
    };
    // clang-format on

    ret = conffile.extract_options("General", global_option_table, &config);
    if (ret < 0) {
        return ret;
    }

    ret = conffile.extract_options("General", LogEndpoint::option_table, &config.log_config);
    if (ret < 0) {
        return ret;
    }

    iter = {};
    offset = strlen(UartEndpoint::section_pattern) - 1;
    while (conffile.get_sections(UartEndpoint::section_pattern, &iter) == 0) {
        UartEndpointConfig opt_uart{};
        opt_uart.name = std::string(iter.name + offset, iter.name_len - offset);

        ret = conffile.extract_options(&iter, UartEndpoint::option_table, &opt_uart);
        if (ret != 0) {
            return ret;
        }

        if (opt_uart.baudrates.empty()) {
            opt_uart.baudrates.push_back(DEFAULT_BAUDRATE);
        }

        if (!UartEndpoint::validate_config(opt_uart)) {
            return -EINVAL; // error message was already logged by validate_config()
        }
        config.uart_configs.push_back(opt_uart);
    }

    iter = {};
    offset = strlen(UdpEndpoint::section_pattern) - 1;
    while (conffile.get_sections(UdpEndpoint::section_pattern, &iter) == 0) {
        UdpEndpointConfig opt_udp{};
        opt_udp.name = std::string(iter.name + offset, iter.name_len - offset);
        opt_udp.port = ULONG_MAX; // unset port value to be checked later on

        ret = conffile.extract_options(&iter, UdpEndpoint::option_table, &opt_udp);
        if (ret != 0) {
            return ret;
        }

        if (UdpEndpointConfig::Mode::Client == opt_udp.mode && ULONG_MAX == opt_udp.port) {
            opt_udp.port = find_next_udp_port(opt_udp.address, config);
        }

        if (!UdpEndpoint::validate_config(opt_udp)) {
            return -EINVAL; // error message was already logged by validate_config()
        }
        config.udp_configs.push_back(opt_udp);
    }

    iter = {};
    offset = strlen(TcpEndpoint::section_pattern) - 1;
    while (conffile.get_sections(TcpEndpoint::section_pattern, &iter) == 0) {
        TcpEndpointConfig opt_tcp{};
        opt_tcp.name = std::string(iter.name + offset, iter.name_len - offset);
        opt_tcp.port = ULONG_MAX; // unset port value to be checked later on
        ret = conffile.extract_options(&iter, TcpEndpoint::option_table, &opt_tcp);
        if (ret != 0) {
            return ret;
        }

        if (!TcpEndpoint::validate_config(opt_tcp)) {
            return -EINVAL; // error message was already logged by validate_config()
        }
        config.tcp_configs.push_back(opt_tcp);
    }

    return 0;
}

static int cmpstr(const void *s1, const void *s2)
{
    return strcmp(*(const char **)s1, *(const char **)s2);
}

void signalHandler(int signal) {
    std::cout << "Shutting down system..." << std::endl;
    exit(0);
    shutdown_requested.store(true);
    // Signal will be handled in main loop
}

static int parse_conf_files(Configuration &config)
{
    DIR *dir;
    struct dirent *ent;
    int ret = 0;
    char *files[128] = {};
    int i = 0, j = 0;

    ConfFile conffile;

    // First, open default conf file
    auto filename = get_conf_file_name(config);
    ret = conffile.parse(filename);

    // If there's no default conf file, everything is good
    if (ret < 0 && ret != -ENOENT) {
        return ret;
    }

    // Then, parse all files on configuration directory
    auto dirname = get_conf_dir(config);
    dir = opendir(dirname.c_str());
    if (dir != nullptr) {
        while ((ent = readdir(dir)) != nullptr) {
            char path[PATH_MAX];
            struct stat st;

            ret = snprintf(path, sizeof(path), "%s/%s", dirname.c_str(), ent->d_name);
            if (ret >= (int)sizeof(path)) {
                log_error("Couldn't open directory %s", dirname.c_str());
                ret = -EINVAL;
                goto fail;
            }
            if (stat(path, &st) < 0 || !S_ISREG(st.st_mode)) {
                continue;
            }
            files[i] = strdup(path);
            if (files[i] == nullptr) {
                ret = -ENOMEM;
                goto fail;
            }
            i++;

            if ((size_t)i > sizeof(files) / sizeof(*files)) {
                log_warning("Too many files on %s. Not all of them will be considered",
                            dirname.c_str());
                break;
            }
        }

        qsort(files, (size_t)i, sizeof(char *), cmpstr);

        for (j = 0; j < i; j++) {
            ret = conffile.parse(files[j]);
            if (ret < 0) {
                goto fail;
            }
            free(files[j]);
        }

        closedir(dir);
    }

    // Finally get configuration values from all config files
    return parse_confs(conffile, config);

fail:
    while (j < i) {
        free(files[j++]);
    }

    closedir(dir);

    return ret;
}

int main(int argc, char *argv[])
{
    Mainloop &mainloop = Mainloop::init();
    int retcode = EXIT_FAILURE; // Initialize to failure by default
    Configuration config{}; // Use brace initialization for proper C++ object initialization

    // Debug: Verify initial configuration state
    log_debug("main() - Initial configuration state: %zu UART, %zu UDP, %zu TCP endpoints",
             config.uart_configs.size(), config.udp_configs.size(), config.tcp_configs.size());

    // Handle help and version arguments before logging initialization
    if (!handle_early_args(argc, argv)) {
        return EXIT_SUCCESS; // Help or version was printed
    }

    // Initialize logging early to show argument validation errors
    Log::open(config.log_backend);

    if (!pre_parse_argv(argc, argv, config)) {
        Log::close();
        return EXIT_FAILURE; // Required arguments missing
    }
    log_info(PACKAGE " version %s", BUILD_VERSION);

    // Register signal handlers
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    // Build remaining config from config files and CLI parameters
    // Parse package configuration when provided
    if (!config.package_config_file.empty()) {
        log_info("main() - Using package configuration, parsing package config file");
        
        if (parse_package_config(config.package_config_file, config) < 0) {
            log_error("main() - Failed to parse package configuration file: %s", config.package_config_file.c_str());
            goto close_log;
        }
        
        log_info("main() - Package configuration loaded successfully");
        log_info("main() - Configuration summary from package config:");
        log_info("main() -   Router config: %s", config.json_conf_file.empty() ? "NOT SET" : config.json_conf_file.c_str());
        log_info("main() -   HTTP config: %s", config.http_conf_file.empty() ? "NOT SET" : config.http_conf_file.c_str());
        log_info("main() -   Statistics config: %s", config.stats_conf_file.empty() ? "NOT SET" : config.stats_conf_file.c_str());
        log_info("main() -   RPC config: %s", config.rpc_config_file.c_str());
        
    } else {
        log_info("main() - No package configuration provided, using legacy config files");
        if (parse_conf_files(config) < 0) {
            goto close_log;
        }
    }

    // Parse command line arguments, overriding conf files
    if (parse_argv(argc, argv, config) != 1) {
        goto close_log;
    }
    dbg("Cmd line and options parsed");

    // Debug: Verify configuration after command line parsing
    log_debug("main() - Post-argv configuration state: %zu UART, %zu UDP, %zu TCP endpoints",
             config.uart_configs.size(), config.udp_configs.size(), config.tcp_configs.size());

    // Load JSON configuration files if specified
    if (!config.json_conf_file.empty()) {
        log_info("main() - JSON configuration file specified: '%s'", config.json_conf_file.c_str());
        log_info("main() - Starting JSON configuration loading process");

        // Helper function to debug configuration state
        auto debug_config_state = [&](const std::string& stage) {
            log_debug("main() - Config state at %s:", stage.c_str());
            log_debug("main() - UART configs: size=%zu, capacity=%zu, data=%p",
                     config.uart_configs.size(), config.uart_configs.capacity(),
                     config.uart_configs.data());
            log_debug("main() - UDP configs: size=%zu, capacity=%zu, data=%p",
                     config.udp_configs.size(), config.udp_configs.capacity(),
                     config.udp_configs.data());
            log_debug("main() - TCP configs: size=%zu, capacity=%zu, data=%p",
                     config.tcp_configs.size(), config.tcp_configs.capacity(),
                     config.tcp_configs.data());
        };

        debug_config_state("before JSON parsing");

        // Check if file exists and is readable
        struct stat st;
        if (stat(config.json_conf_file.c_str(), &st) != 0) {
            log_error("main() - JSON configuration file does not exist or is not accessible: '%s' (%m)",
                     config.json_conf_file.c_str());
            goto close_log;
        }
        log_debug("main() - JSON file exists and is accessible, size: %ld bytes", st.st_size);

        JsonConfig json_config;
        log_debug("main() - Created JsonConfig instance, starting parse...");

        int ret = json_config.parse(config.json_conf_file);
        if (ret < 0) {
            log_error("main() - Failed to parse JSON configuration file: %s (error code: %d)",
                     config.json_conf_file.c_str(), ret);
            log_error("main() - JSON parsing failed, aborting configuration loading");
            goto close_log;
        }
        log_info("main() - JSON file parsed successfully, extracting configuration...");

        ret = json_config.extract_configuration(config);
        if (ret < 0) {
            log_error("main() - Failed to extract configuration from JSON file: %s (error code: %d)",
                     config.json_conf_file.c_str(), ret);
            log_error("main() - Configuration extraction failed, aborting");
            goto close_log;
        }

        log_info("main() - Successfully loaded JSON configuration from: %s", config.json_conf_file.c_str());

        debug_config_state("after JSON extraction");

        // Debug: Verify configuration immediately after extraction
        log_info("main() - Post-extraction verification: %zu UART, %zu UDP, %zu TCP endpoints",
                config.uart_configs.size(), config.udp_configs.size(), config.tcp_configs.size());
        for (size_t i = 0; i < config.uart_configs.size(); i++) {
            log_debug("main() - Post-extraction UART[%zu]: name='%s', device='%s'",
                     i, config.uart_configs[i].name.c_str(), config.uart_configs[i].device.c_str());
        }
        for (size_t i = 0; i < config.udp_configs.size(); i++) {
            log_debug("main() - Post-extraction UDP[%zu]: name='%s', address='%s', port=%lu",
                     i, config.udp_configs[i].name.c_str(), config.udp_configs[i].address.c_str(), config.udp_configs[i].port);
        }
        for (size_t i = 0; i < config.tcp_configs.size(); i++) {
            log_debug("main() - Post-extraction TCP[%zu]: name='%s', address='%s', port=%lu",
                     i, config.tcp_configs[i].name.c_str(), config.tcp_configs[i].address.c_str(), config.tcp_configs[i].port);
        }

        // Log detailed configuration status
        log_debug("main() - Log configuration: logs_dir='%s', log_mode=%d, fcu_id=%d",
                 config.log_config.logs_dir.c_str(), (int)config.log_config.log_mode, config.log_config.fcu_id);
    } else {
        log_debug("main() - No JSON configuration file specified");
    }

    // Load statistics configuration file if specified
    if (!config.stats_conf_file.empty()) {
        log_info("Loading statistics configuration from: %s", config.stats_conf_file.c_str());
        JsonConfig stats_config;
        int ret = stats_config.parse(config.stats_conf_file);
        if (ret < 0) {
            log_error("Failed to parse statistics configuration file: %s (error code: %d)",
                     config.stats_conf_file.c_str(), ret);
            goto close_log;
        }

        ret = stats_config.extract_stats_config(config.stats_config);
        if (ret < 0) {
            log_error("Failed to extract statistics configuration from JSON file: %s (error code: %d)",
                     config.stats_conf_file.c_str(), ret);
            goto close_log;
        }
        log_info("Successfully loaded statistics configuration from: %s", config.stats_conf_file.c_str());
    }

    Log::set_max_level(config.debug_log_level);

    // Log final configuration summary after all configuration loading
    log_info("main() - Final configuration loaded - %zu UART endpoints, %zu UDP endpoints, %zu TCP endpoints",
            config.uart_configs.size(), config.udp_configs.size(), config.tcp_configs.size());

    // Log individual endpoint details for debugging
    for (const auto &uart_cfg : config.uart_configs) {
        log_debug("main() - UART endpoint: %s on %s", uart_cfg.name.c_str(), uart_cfg.device.c_str());
    }
    for (const auto &udp_cfg : config.udp_configs) {
        log_debug("main() - UDP endpoint: %s at %s:%lu", udp_cfg.name.c_str(), udp_cfg.address.c_str(), udp_cfg.port);
    }
    for (const auto &tcp_cfg : config.tcp_configs) {
        log_debug("main() - TCP endpoint: %s at %s:%lu", tcp_cfg.name.c_str(), tcp_cfg.address.c_str(), tcp_cfg.port);
    }

    // Mainloop initialization and endpoint setup removed - will be done on HTTP start request
    log_info("main() - Mainloop will be initialized when start is requested via POST /api/threads/mainloop/start");

    // Initialize ThreadManager for tracked thread execution
    log_info("main() - Initializing ThreadManager for mavlink router mainloop");

    try {
        ThreadMgr::ThreadManager threadManager(10); // Initial capacity of 10 threads

        // Initialize ExtensionManager with ThreadManager
        MavlinkExtensions::ExtensionManager extensionManager(threadManager);

        // Set extension configuration directory from router config
        if (!config.extension_conf_dir.empty()) {
            extensionManager.setExtensionConfDir(config.extension_conf_dir);
        }

        // Set global configuration reference for auto-assignment of extension points
        extensionManager.setGlobalConfig(&config);

        // Create RPC controller for thread management (independent of HTTP)
        std::string routerConfigPath = config.json_conf_file.empty() ? "" : config.json_conf_file;
        auto rpcController = std::make_shared<RpcMechanisms::RpcController>(threadManager, routerConfigPath);
        log_info("main() - RPC controller created for thread management");
        
        if (!routerConfigPath.empty()) {
            log_info("main() - RPC controller configured with router config: %s", routerConfigPath.c_str());
        } else {
            log_warning("main() - RPC controller created without router configuration path");
        }

        // Initialize UR-RPC integration
        std::string rpcConfigPath = config.rpc_config_file.empty() ? "config/ur-mavrouter-rpc.json" : config.rpc_config_file;
        log_info("main() - Using RPC config file: %s", rpcConfigPath.c_str());
        
        // Set extension manager for RPC controller before initialization
        rpcController->setExtensionManager(&extensionManager);
        log_info("main() - Extension manager attached to RPC controller");
        
        // Also set RPC controller reference in extension manager
        extensionManager.setRpcController(rpcController.get());
        log_info("main() - RPC controller attached to extension manager");
        
        if (rpcController->initializeRpcIntegration(rpcConfigPath, "ur-mavrouter")) {
            log_info("main() - UR-RPC integration initialized successfully");
            
            // Start RPC client
            if (rpcController->startRpcClient()) {
                log_info("main() - UR-RPC client started successfully");
            } else {
                log_warning("main() - Failed to start UR-RPC client, continuing without RPC functionality");
            }
        } else {
            log_warning("main() - Failed to initialize UR-RPC integration, continuing without RPC functionality");
        }

        #ifdef _BUILD_HTTP
        std::shared_ptr<HttpModule::HttpServer> httpServer;
        HttpModule::HttpServerConfig httpConfig;
        unsigned int httpServerThreadId = 0;

        if (!config.http_conf_file.empty()) {
            log_info("main() - HTTP configuration file specified: '%s'", config.http_conf_file.c_str());

            try {
                httpConfig = HttpModule::parseHttpConfig(config.http_conf_file);
                httpServer = std::make_shared<HttpModule::HttpServer>(httpConfig);

                // Add default routes
                httpServer->addRoute(HttpModule::HttpMethod::GET, "/",
                    [](const HttpModule::HttpRequest& req) {
                        HttpModule::HttpResponse resp;
                        resp.statusCode = 200;
                        resp.contentType = "text/html";
                        resp.content = "<html><body><h1>MAVLink Router HTTP Server</h1><p>Server is running</p></body></html>";
                        return resp;
                    });

                httpServer->addRoute(HttpModule::HttpMethod::GET, "/status",
                    [](const HttpModule::HttpRequest& req) {
                        HttpModule::HttpResponse resp;
                        resp.statusCode = 200;
                        resp.contentType = "application/json";
                        resp.content = "{\"status\":\"running\",\"service\":\"mavlink-router\"}";
                        return resp;
                    });

                // Add extension management HTTP endpoints

                // POST /api/extensions/add - Add new extension
                httpServer->addRoute(HttpModule::HttpMethod::POST, "/api/extensions/add",
                    [&extensionManager](const HttpModule::HttpRequest& req) -> HttpModule::HttpResponse {
                        std::cout << "\n[HTTP] Client request: POST /api/extensions/add" << std::endl;
                        std::cout << "[HTTP] Request body: " << req.body << std::endl;

                        HttpModule::HttpResponse resp;
                        resp.contentType = "application/json";

                        try {
                            // Parse extension config from JSON
                            MavlinkExtensions::ExtensionConfig extConfig = extensionManager.parseExtensionConfigFromJson(req.body);

                            // Extension point assignment is now handled automatically by createExtension
                            // if assigned_extension_point is empty

                            // Create the extension
                            std::string result = extensionManager.createExtension(extConfig);

                            if (result == "Success") {
                                resp.statusCode = 201; // Created
                                auto info = extensionManager.getExtensionInfo(extConfig.name);
                                resp.content = "{\"status\":\"success\",\"message\":\"Extension created successfully\",\"extension\":" +
                                             extensionManager.extensionInfoToJson(info) + "}";
                            } else if (result == "No available extension points") {
                                resp.statusCode = 503; // Service Unavailable
                                resp.content = "{\"error\":\"" + result + "\"}";
                            } else {
                                resp.statusCode = 400; // Bad Request
                                resp.content = "{\"error\":\"" + result + "\"}";
                            }
                        } catch (const std::exception& e) {
                            std::cout << "[HTTP] Error: " << e.what() << std::endl;
                            resp.statusCode = 400;
                            resp.content = "{\"error\":\"Invalid request: " + std::string(e.what()) + "\"}";
                        }

                        return resp;
                    });

                // DELETE /api/extensions/delete/:name - Delete extension
                httpServer->addRoute(HttpModule::HttpMethod::DELETE, "/api/extensions/delete/:name",
                    [&extensionManager](const HttpModule::HttpRequest& req) -> HttpModule::HttpResponse {
                        std::string extensionName;

                        // Extract name from URL path
                        size_t pos = req.url.find("/api/extensions/delete/");
                        if (pos != std::string::npos) {
                            extensionName = req.url.substr(pos + 24); // Length of "/api/extensions/delete/"
                        }

                        std::cout << "\n[HTTP] Client request: DELETE /api/extensions/delete/" << extensionName << std::endl;

                        HttpModule::HttpResponse resp;
                        resp.contentType = "application/json";

                        if (extensionName.empty()) {
                            resp.statusCode = 400;
                            resp.content = "{\"error\":\"Extension name is required\"}";
                            return resp;
                        }

                        bool success = extensionManager.deleteExtension(extensionName);

                        if (success) {
                            resp.statusCode = 200;
                            resp.content = "{\"status\":\"success\",\"message\":\"Extension deleted successfully\"}";
                        } else {
                            resp.statusCode = 404;
                            resp.content = "{\"error\":\"Extension not found\"}";
                        }

                        return resp;
                    });

                // POST /api/extensions/stop/:name - Stop extension
                httpServer->addRoute(HttpModule::HttpMethod::POST, "/api/extensions/stop/:name",
                    [&extensionManager](const HttpModule::HttpRequest& req) -> HttpModule::HttpResponse {
                        std::string extensionName;

                        // Extract name from URL path
                        size_t pos = req.url.find("/api/extensions/stop/");
                        if (pos != std::string::npos) {
                            extensionName = req.url.substr(pos + 22); // Length of "/api/extensions/stop/"
                        }

                        std::cout << "\n[HTTP] Client request: POST /api/extensions/stop/" << extensionName << std::endl;

                        HttpModule::HttpResponse resp;
                        resp.contentType = "application/json";

                        if (extensionName.empty()) {
                            resp.statusCode = 400;
                            resp.content = "{\"error\":\"Extension name is required\"}";
                            return resp;
                        }

                        bool success = extensionManager.stopExtension(extensionName);

                        if (success) {
                            resp.statusCode = 200;
                            auto info = extensionManager.getExtensionInfo(extensionName);
                            resp.content = "{\"status\":\"success\",\"message\":\"Extension stopped\",\"extension\":" +
                                         extensionManager.extensionInfoToJson(info) + "}";
                        } else {
                            resp.statusCode = 404;
                            resp.content = "{\"error\":\"Extension not found\"}";
                        }

                        return resp;
                    });

                // POST /api/extensions/start/:name - Start extension
                httpServer->addRoute(HttpModule::HttpMethod::POST, "/api/extensions/start/:name",
                    [&extensionManager](const HttpModule::HttpRequest& req) -> HttpModule::HttpResponse {
                        std::string extensionName;

                        // Extract name from URL path
                        size_t pos = req.url.find("/api/extensions/start/");
                        if (pos != std::string::npos) {
                            extensionName = req.url.substr(pos + 23); // Length of "/api/extensions/start/"
                        }

                        std::cout << "\n[HTTP] Client request: POST /api/extensions/start/" << extensionName << std::endl;

                        HttpModule::HttpResponse resp;
                        resp.contentType = "application/json";

                        if (extensionName.empty()) {
                            resp.statusCode = 400;
                            resp.content = "{\"error\":\"Extension name is required\"}";
                            return resp;
                        }

                        bool success = extensionManager.startExtension(extensionName);

                        if (success) {
                            resp.statusCode = 200;
                            auto info = extensionManager.getExtensionInfo(extensionName);
                            resp.content = "{\"status\":\"success\",\"message\":\"Extension started\",\"extension\":" +
                                         extensionManager.extensionInfoToJson(info) + "}";
                        } else {
                            resp.statusCode = 404;
                            resp.content = "{\"error\":\"Extension not found\"}";
                        }

                        return resp;
                    });

                // GET /api/extensions/status/:name - Get specific extension status
                httpServer->addRoute(HttpModule::HttpMethod::GET, "/api/extensions/status/:name",
                    [&extensionManager](const HttpModule::HttpRequest& req) -> HttpModule::HttpResponse {
                        std::string extensionName;

                        // Extract name from URL path
                        size_t pos = req.url.find("/api/extensions/status/");
                        if (pos != std::string::npos) {
                            extensionName = req.url.substr(pos + 24); // Length of "/api/extensions/status/"
                        }

                        std::cout << "\n[HTTP] Client request: GET /api/extensions/status/" << extensionName << std::endl;

                        HttpModule::HttpResponse resp;
                        resp.contentType = "application/json";

                        if (extensionName.empty()) {
                            resp.statusCode = 400;
                            resp.content = "{\"error\":\"Extension name is required\"}";
                            return resp;
                        }

                        auto info = extensionManager.getExtensionInfo(extensionName);

                        if (!info.name.empty()) {
                            resp.statusCode = 200;
                            resp.content = extensionManager.extensionInfoToJson(info);
                        } else {
                            resp.statusCode = 404;
                            resp.content = "{\"error\":\"Extension not found\"}";
                        }

                        return resp;
                    });

                // GET /api/extensions/status - Get all extensions status
                httpServer->addRoute(HttpModule::HttpMethod::GET, "/api/extensions/status",
                    [&extensionManager](const HttpModule::HttpRequest& req) -> HttpModule::HttpResponse {
                        std::cout << "\n[HTTP] Client request: GET /api/extensions/status" << std::endl;

                        HttpModule::HttpResponse resp;
                        resp.contentType = "application/json";
                        resp.statusCode = 200;
                        resp.content = "{\"extensions\":" + extensionManager.allExtensionsToJson() + "}";

                        return resp;
                    });

                // GET /api/startup/status - Get startup mechanism status
                httpServer->addRoute(HttpModule::HttpMethod::GET, "/api/startup/status",
                    [&rpcController](const HttpModule::HttpRequest& req) -> HttpModule::HttpResponse {
                        std::cout << "\n[HTTP] Client request: GET /api/startup/status" << std::endl;
                        std::cout << "[HTTP] Action: Get startup mechanism status" << std::endl;

                        HttpModule::HttpResponse resp;
                        resp.contentType = "application/json";
                        resp.statusCode = 200;
                        
                        try {
                            auto startupStatus = rpcController->getStartupStatus();
                            resp.content = startupStatus.dump();
                            
                            std::cout << "[HTTP] Startup status: " << startupStatus["overall_status"] << std::endl;
                            
                        } catch (const std::exception& e) {
                            resp.statusCode = 500;
                            resp.content = "{\"error\": \"Failed to get startup status: " + std::string(e.what()) + "\"}";
                            std::cerr << "[HTTP] Error getting startup status: " << e.what() << std::endl;
                        }

                        return resp;
                    });

                // POST /api/startup/trigger - Manually trigger device discovery
                httpServer->addRoute(HttpModule::HttpMethod::POST, "/api/startup/trigger",
                    [&rpcController](const HttpModule::HttpRequest& req) -> HttpModule::HttpResponse {
                        std::cout << "\n[HTTP] Client request: POST /api/startup/trigger" << std::endl;
                        std::cout << "[HTTP] Action: Manually trigger device discovery" << std::endl;

                        HttpModule::HttpResponse resp;
                        resp.contentType = "application/json";
                        
                        try {
                            // Create a device discovery request
                            nlohmann::json triggerRequest;
                            triggerRequest["jsonrpc"] = "2.0";
                            triggerRequest["method"] = "device-list";
                            triggerRequest["params"] = nlohmann::json::object();
                            triggerRequest["id"] = "manual_trigger";
                            
                            // Send the request via RPC
                            std::string payload = triggerRequest.dump();
                            rpcController->handleRpcMessage("manual_trigger", payload);
                            
                            resp.statusCode = 200;
                            resp.content = "{\"status\": \"success\", \"message\": \"Device discovery triggered manually\"}";
                            std::cout << "[HTTP] Device discovery triggered successfully" << std::endl;
                            
                        } catch (const std::exception& e) {
                            resp.statusCode = 500;
                            resp.content = "{\"error\": \"Failed to trigger device discovery: " + std::string(e.what()) + "\"}";
                            std::cerr << "[HTTP] Error triggering device discovery: " << e.what() << std::endl;
                        }

                        return resp;
                    });


                // Set RPC controller for HTTP server
                httpServer->setRpcController(rpcController);
                log_info("main() - RPC controller attached to HTTP server");

                // Set router configuration path for HTTP server
                if (!routerConfigPath.empty()) {
                    httpServer->setRouterConfigPath(routerConfigPath);
                    log_info("main() - Router configuration path attached to HTTP server: %s", routerConfigPath.c_str());
                } else {
                    log_warning("main() - No router configuration path available for HTTP server");
                }

                // Set extension manager for HTTP server
                auto extensionManagerPtr = std::shared_ptr<MavlinkExtensions::ExtensionManager>(&extensionManager, [](MavlinkExtensions::ExtensionManager*){});
                httpServer->setExtensionManager(extensionManagerPtr);
                log_info("main() - Extension manager attached to HTTP server");

                // Start HTTP server in a separate thread if configured
                std::cout << "Starting HTTP server thread..." << std::endl;

                auto httpServerFunc = [&httpServer, &threadManager, &httpServerThreadId, rpcController, &extensionManager]() {
                    try {
                        httpServer->start();
                        log_info("HTTP server thread started, listening on %s:%u", httpServer->getConfig().address.c_str(), httpServer->getConfig().port);

                        // Register HTTP server with RPC controller
                        // The HTTP server thread will be registered by the main thread after creation
                        std::string httpAttachment = "http_server";
                        rpcController->registerThread(httpAttachment, httpServerThreadId, httpAttachment);
                        log_info("Registered HTTP server thread with RPC: ID=%u, Name='%s'", httpServerThreadId, httpAttachment.c_str());


                        // Keep thread alive while server is running
                        while (httpServer->isRunning()) {
                            std::this_thread::sleep_for(std::chrono::milliseconds(100));
                        }
                        log_info("HTTP server is no longer running, exiting thread function.");

                    } catch (const HttpModule::HttpServerException& e) {
                        log_error("HTTP server exception: %s", e.what());
                    }
                };

                log_info("main() - Creating HTTP server thread");
                httpServerThreadId = threadManager.createThread(httpServerFunc);
                log_info("main() - HTTP server thread created with ID: %u", httpServerThreadId);


            } catch (const HttpModule::HttpServerException& e) {
                log_error("main() - Failed to initialize HTTP server: %s", e.what());
                log_error("main() - Continuing without HTTP server");
            }
        } else {
            log_debug("main() - No HTTP configuration file specified, HTTP server disabled");
        }
        #else
        log_debug("main() - HTTP server support not compiled (build with -D_BUILD_HTTP=ON to enable)");
        #endif

        // Register restart callback for mainloop (will initialize and start on HTTP request)
        log_info("main() - Registering mainloop restart callback (mainloop will not start automatically)");
        rpcController->registerRestartCallback("mainloop", [&config, &threadManager, &rpcController, &extensionManager]() -> unsigned int {
            log_info("Restart callback: Creating new mainloop thread");

            // Reload configuration before starting mainloop to pick up any device path updates
            try {
                if (config.json_conf_file.empty()) {
                    log_error("Restart callback: No router configuration file available in package config");
                    log_warning("Restart callback: Using existing configuration");
                } else {
                    log_info("Restart callback: Reloading configuration from %s", config.json_conf_file.c_str());
                    
                    // Use the same JSON configuration loading mechanism as initial startup
                    JsonConfig json_config;
                    int ret = json_config.parse(config.json_conf_file);
                    if (ret >= 0) {
                        ret = json_config.extract_configuration(config);
                        if (ret >= 0) {
                            log_info("Restart callback: Configuration reloaded successfully");
                            log_info("Restart callback: Reloaded config has %zu UART, %zu UDP, %zu TCP endpoints",
                                    config.uart_configs.size(), config.udp_configs.size(), config.tcp_configs.size());
                        } else {
                            log_error("Restart callback: Failed to extract configuration from JSON (error code: %d)", ret);
                            log_warning("Restart callback: Using existing configuration");
                        }
                    } else {
                        log_error("Restart callback: Failed to parse JSON configuration file (error code: %d)", ret);
                        log_warning("Restart callback: Using existing configuration");
                    }
                }
            } catch (const std::exception& e) {
                log_error("Restart callback: Failed to reload configuration: %s", e.what());
                log_warning("Restart callback: Using existing configuration");
            }

            // Create new mainloop thread
            auto mainloopFunc = [&config]() {
                try {
                    log_info("Mainloop_thread - Thread function starting");

                    // Ensure clean state by tearing down first (safe even if not initialized)
                    log_info("Mainloop_thread - Ensuring clean state with teardown");
                    Mainloop::teardown();

                    // Initialize a fresh mainloop instance
                    log_info("Mainloop_thread - Calling init");
                    Mainloop& mainloop = Mainloop::init();
                    log_info("Mainloop_thread - Init complete");

                    // Open the mainloop (initialize epoll)
                    log_info("Mainloop_thread - Calling open");
                    int open_result = mainloop.open();
                    log_info("Mainloop_thread - Open returned %d, epollfd=%d", open_result, mainloop.epollfd);

                    if (open_result < 0) {
                        log_error("Failed to open mainloop (error=%d)", open_result);
                        Mainloop::teardown();
                        return;
                    }

                    // Add endpoints from config
                    log_info("Mainloop_thread - Calling add_endpoints");
                    if (!mainloop.add_endpoints(config)) {
                        log_error("Failed to add endpoints in mainloop");
                        Mainloop::teardown();
                        return;
                    }
                    log_info("Mainloop_thread - Endpoints added successfully");

                    // Start the mainloop
                    log_info("Mainloop_thread - Entering event loop");
                    int ret = mainloop.loop();
                    log_info("Mainloop_thread - Exited event loop with return code %d", ret);

                    // Clean up after loop exits
                    Mainloop::teardown();
                } catch (const std::exception& e) {
                    log_error("Exception in mainloop: %s", e.what());
                    Mainloop::teardown();
                } catch (...) {
                    log_error("Unknown exception in mainloop");
                    Mainloop::teardown();
                }
            };

            unsigned int newThreadId = threadManager.createThread(mainloopFunc);
            std::string attachment = "mainloop";

            // Register with RPC controller
            rpcController->registerThread(attachment, newThreadId, attachment);

            log_info("Restart callback: New mainloop thread created with ID %u", newThreadId);
            return newThreadId;
        });

        std::cout << "Registered mainloop restart callback" << std::endl;

        // Extension loading moved to HTTP start request handler
        log_info("main() - Extensions will be loaded when mainloop start is requested via POST /api/threads/mainloop/start");


        log_info("main() - Entering main wait loop - press Ctrl+C or send SIGTERM to exit");
        #ifdef _BUILD_HTTP
        log_info("main() - Mainloop will only start when requested via POST /api/threads/mainloop/start");
        #else
        log_info("main() - Mainloop will only start when requested via RPC device added event");
        #endif

        // Monitor HTTP server thread - keep running as long as it's alive
        while (true) {
        #ifdef _BUILD_HTTP
            bool httpServerAlive = httpServerThreadId != 0 && threadManager.isThreadAlive(httpServerThreadId);

            // If HTTP server stopped unexpectedly, that's an error condition - exit
            if (httpServerThreadId != 0 && !httpServerAlive) {
                ThreadMgr::ThreadState state = threadManager.getThreadState(httpServerThreadId);
                if (state == ThreadMgr::ThreadState::Error) {
                    log_error("main() - HTTP server thread %u encountered an error", httpServerThreadId);
                    retcode = EXIT_FAILURE;
                    break;
                }
                // HTTP server stopped gracefully (likely due to stop command)
                log_info("main() - HTTP server stopped, exiting application");
                retcode = EXIT_SUCCESS;
                break;
            }
        #else
            // When HTTP is disabled, keep running indefinitely or until shutdown signal
            // RPC client should stay alive and handle reconnections automatically
            if (shutdown_requested.load()) {
                log_info("main() - Shutdown requested, exiting application");
                retcode = EXIT_SUCCESS;
                break;
            }
            
            // Check RPC client status but don't exit on temporary disconnections
            if (rpcController) {
                bool rpcRunning = rpcController->isRpcClientRunning();
                if (!rpcRunning) {
                    static int rpcWarningCount = 0;
                    if (rpcWarningCount % 60 == 0) { // Log every 60 seconds (10 * 6)
                        log_warning("main() - RPC client not running, but keeping application alive for reconnection");
                        rpcWarningCount++;
                    } else {
                        rpcWarningCount++;
                    }
                }
            }
        #endif

            // Sleep for a short interval before checking again
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        // Cleanup - only stop HTTP server if it's still running
        #ifdef _BUILD_HTTP
        if (httpServerThreadId != 0 && threadManager.isThreadAlive(httpServerThreadId)) {
            std::cout << "Stopping HTTP server..." << std::endl;
            if (httpServer) { // Ensure httpServer is valid
                httpServer->stop();
            }
            try {
                threadManager.stopThread(httpServerThreadId);
                threadManager.joinThread(httpServerThreadId, std::chrono::seconds(5));
            } catch (const ThreadMgr::ThreadManagerException& e) {
                std::cerr << "Error stopping HTTP server thread: " << e.what() << std::endl;
            }
            std::cout << "HTTP server stopped" << std::endl;
        }
        #endif

        log_info("main() - ThreadManager operations completed");

        // Cleanup UR-RPC integration
        if (rpcController) {
            log_info("main() - Stopping UR-RPC client");
            rpcController->stopRpcClient();
            log_info("main() - UR-RPC client stopped");
        }

    } catch (const ThreadMgr::ThreadManagerException& e) {
        log_error("main() - ThreadManager exception: %s", e.what());
        retcode = EXIT_FAILURE;
    } catch (const std::exception& e) {
        log_error("main() - Standard exception in thread management: %s", e.what());
        retcode = EXIT_FAILURE;
    } catch (...) {
        log_error("main() - Unknown exception in thread management");
        retcode = EXIT_FAILURE;
    }

    Log::close();
    return retcode;

close_log:
    Log::close();
    return EXIT_FAILURE;
}

#ifdef _BUILD_HTTP
// HTTP server thread function - currently not used as HTTP server is managed by ThreadManager
// and started directly within main(). It's kept here for historical reference
// or potential future use if direct pthread manipulation is needed again.
static void* http_server_thread_func(void* arg) {
    log_info("http_thread - Thread function started");
    HttpModule::HttpServer* server = static_cast<HttpModule::HttpServer*>(arg);

    if (!server) {
        log_error("http_thread - Server pointer is NULL!");
        return nullptr;
    }

    log_info("http_thread - Server pointer valid, attempting to start server...");

    try {
        server->start();
        log_info("http_thread - Server started successfully, entering keep-alive loop");

        // Keep thread alive while server is running
        while (server->isRunning()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        log_info("http_thread - Server stopped running, exiting thread");
    } catch (const HttpModule::HttpServerException& e) {
        log_error("http_thread - HTTP server exception: %s", e.what());
        log_error("http_thread - Check if port is already in use: lsof -i :5000");
        log_error("http_thread - Check permissions and system resources");
    } catch (const std::exception& e) {
        log_error("http_thread - Unexpected exception: %s", e.what());
    } catch (...) {
        log_error("http_thread - Unknown exception occurred");
    }

    log_info("http_thread - Thread function completed");
    return nullptr;
}
#endif