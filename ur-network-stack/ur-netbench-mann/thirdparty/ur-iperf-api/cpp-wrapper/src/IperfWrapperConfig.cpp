
#include "IperfWrapper.hpp"
#include <fstream>
#include <stdexcept>
#include <ctime>
#include <iomanip>
#include <sstream>

void IperfWrapper::validateConfig(const json& config) {
    if (!config.contains("role")) {
        throw std::runtime_error("Missing required field: 'role' (must be 'client' or 'server')");
    }

    std::string role = config["role"];
    if (role != "client" && role != "server") {
        throw std::runtime_error("Invalid role: must be 'client' or 'server'");
    }

    if (role == "client" && !config.contains("server_hostname")) {
        throw std::runtime_error("Client mode requires 'server_hostname' field");
    }
}

void IperfWrapper::loadConfig(const json& config) {
    validateConfig(config);
    applyOutputOptions(config);
    applyRequiredFields(config);
    applyOptionalFields(config);
    
    // Register callback immediately after config for real-time output
    if (realtimeJsonOutput_ || logToFile_ || exportToFile_) {
        iperf_set_test_json_callback(test_, onJsonCallback);
    }
}

void IperfWrapper::applyOutputOptions(const json& config) {
    // Handle realtime output
    if (config.contains("realtime") && config["realtime"].is_boolean()) {
        enableRealtimeJsonOutput(config["realtime"]);
    }
    
    // Handle streaming mode
    if (config.contains("streaming") && config["streaming"].is_boolean()) {
        enableStreamingMode(config["streaming"]);
    }
    
    // Handle log results
    if (config.contains("log_results")) {
        if (config["log_results"].is_boolean() && config["log_results"]) {
            // Generate timestamped filename
            auto now = std::time(nullptr);
            auto tm = *std::localtime(&now);
            std::ostringstream oss;
            oss << "iperf_results_" 
                << std::put_time(&tm, "%Y%m%d_%H%M%S") 
                << ".json";
            enableLogToFile(oss.str());
        } else if (config["log_results"].is_string()) {
            enableLogToFile(config["log_results"]);
        }
    }
    
    // Handle export results
    if (config.contains("export_results") && config["export_results"].is_string()) {
        enableExportToFile(config["export_results"]);
    }
}

void IperfWrapper::loadConfigFromFile(const std::string& configFile) {
    std::ifstream file(configFile);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open config file: " + configFile);
    }

    json config;
    try {
        file >> config;
    } catch (const json::exception& e) {
        throw std::runtime_error("Failed to parse JSON config: " + std::string(e.what()));
    }

    loadConfig(config);
}

void IperfWrapper::applyRequiredFields(const json& config) {
    std::string role = config["role"];
    if (role == "client") {
        iperf_set_test_role(test_, 'c');
        std::string hostname = config["server_hostname"];
        iperf_set_test_server_hostname(test_, hostname.c_str());
    } else {
        iperf_set_test_role(test_, 's');
    }
}

void IperfWrapper::applyOptionalFields(const json& config) {
    if (config.contains("port")) {
        int port = config["port"];
        iperf_set_test_server_port(test_, port);
    }

    if (config.contains("bind_port")) {
        int bind_port = config["bind_port"];
        iperf_set_test_bind_port(test_, bind_port);
    }

    if (config.contains("protocol")) {
        std::string protocol = config["protocol"];
        if (protocol == "tcp") {
            iperf_set_test_role(test_, iperf_get_test_role(test_));
        } else if (protocol == "udp") {
            iperf_set_test_role(test_, iperf_get_test_role(test_));
            iperf_set_test_blksize(test_, DEFAULT_UDP_BLKSIZE);
        } else if (protocol == "sctp") {
            iperf_set_test_role(test_, iperf_get_test_role(test_));
        }
    }

    if (config.contains("duration")) {
        int duration = config["duration"];
        iperf_set_test_duration(test_, duration);
    }
    
    // Enable JSON output mode - required for callback to work
    iperf_set_test_json_output(test_, 1);
    
    // Enable JSON streaming for real-time interval output
    if (realtimeJsonOutput_ || logToFile_ || exportToFile_) {
        iperf_set_test_json_stream(test_, 1);
    }

    if (config.contains("omit")) {
        int omit = config["omit"];
        iperf_set_test_omit(test_, omit);
    }

    if (config.contains("bandwidth")) {
        uint64_t bandwidth = config["bandwidth"];
        iperf_set_test_rate(test_, bandwidth);
    }

    if (config.contains("num_streams")) {
        int num_streams = config["num_streams"];
        iperf_set_test_num_streams(test_, num_streams);
    }

    if (config.contains("parallel")) {
        int parallel = config["parallel"];
        iperf_set_test_num_streams(test_, parallel);
    }

    if (config.contains("reverse")) {
        bool reverse = config["reverse"];
        iperf_set_test_reverse(test_, reverse ? 1 : 0);
    }

    if (config.contains("bidirectional")) {
        bool bidirectional = config["bidirectional"];
        iperf_set_test_bidirectional(test_, bidirectional ? 1 : 0);
    }

    if (config.contains("blksize")) {
        int blksize = config["blksize"];
        iperf_set_test_blksize(test_, blksize);
    }

    if (config.contains("buffer_size")) {
        int buffer_size = config["buffer_size"];
        iperf_set_test_socket_bufsize(test_, buffer_size);
    }

    if (config.contains("bytes")) {
        uint64_t bytes = config["bytes"];
        iperf_set_test_bytes(test_, bytes);
    }

    if (config.contains("blocks")) {
        uint64_t blocks = config["blocks"];
        iperf_set_test_blocks(test_, blocks);
    }

    if (config.contains("burst")) {
        int burst = config["burst"];
        iperf_set_test_burst(test_, burst);
    }

    if (config.contains("interval")) {
        double interval = config["interval"];
        iperf_set_test_reporter_interval(test_, interval);
    }

    if (config.contains("bind_address")) {
        std::string bind_address = config["bind_address"];
        iperf_set_test_bind_address(test_, bind_address.c_str());
    }

    if (config.contains("bind_dev")) {
        std::string bind_dev = config["bind_dev"];
        iperf_set_test_bind_dev(test_, bind_dev.c_str());
    }

    if (config.contains("zerocopy")) {
        bool zerocopy = config["zerocopy"];
        iperf_set_test_zerocopy(test_, zerocopy ? 1 : 0);
    }

    if (config.contains("verbose")) {
        bool verbose = config["verbose"];
        iperf_set_verbose(test_, verbose ? 1 : 0);
    }

    if (config.contains("tos")) {
        int tos = config["tos"];
        iperf_set_test_tos(test_, tos);
    }

    if (config.contains("no_delay")) {
        bool no_delay = config["no_delay"];
        iperf_set_test_no_delay(test_, no_delay ? 1 : 0);
    }

    if (config.contains("congestion_control")) {
        std::string cc = config["congestion_control"];
        iperf_set_test_congestion_control(test_, const_cast<char*>(cc.c_str()));
    }

    if (config.contains("mss")) {
        int mss = config["mss"];
        iperf_set_test_mss(test_, mss);
    }

    if (config.contains("timestamps")) {
        bool timestamps = config["timestamps"];
        iperf_set_test_timestamps(test_, timestamps ? 1 : 0);
    }

    if (config.contains("one_off")) {
        bool one_off = config["one_off"];
        iperf_set_test_one_off(test_, one_off ? 1 : 0);
    }

    if (config.contains("get_server_output")) {
        bool get_server_output = config["get_server_output"];
        iperf_set_test_get_server_output(test_, get_server_output ? 1 : 0);
    }

    if (config.contains("udp_counters_64bit")) {
        bool udp_64bit = config["udp_counters_64bit"];
        iperf_set_test_udp_counters_64bit(test_, udp_64bit ? 1 : 0);
    }

    if (config.contains("repeating_payload")) {
        bool repeating = config["repeating_payload"];
        iperf_set_test_repeating_payload(test_, repeating ? 1 : 0);
    }

    if (config.contains("dont_fragment")) {
        bool dont_fragment = config["dont_fragment"];
        iperf_set_dont_fragment(test_, dont_fragment ? 1 : 0);
    }

    if (config.contains("logfile")) {
        std::string logfile = config["logfile"];
        iperf_set_test_logfile(test_, logfile.c_str());
    }
}
