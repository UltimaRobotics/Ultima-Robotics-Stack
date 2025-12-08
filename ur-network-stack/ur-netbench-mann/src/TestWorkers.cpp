#include "../include/TestWorkers.hpp"
#include "../include/LoggingMacros.hpp"
#include "../include/FileWatchdog.hpp"
#include "../thirdparty/dns-lookup-api/include/DNSLookupAPI.hpp"
#include "../thirdparty/traceroute-api/include/traceroute.hpp"
#include "../thirdparty/ur-ping-api/include/PingAPI.hpp"
#include "../thirdparty/ur-iperf-api/cpp-wrapper/include/IperfWrapper.hpp"
#include "../ur-netbench-shared/include/DNSResultSerializer.hpp"
#include "../ur-netbench-shared/include/TracerouteResultSerializer.hpp"
#include "../ur-netbench-shared/include/PingResultSerializer.hpp"
#include "../ur-netbench-shared/include/IperfResultSerializer.hpp"
#include <iostream>
#include <fstream>
#include <iomanip>
#include <memory>
#include <thread>
#include <chrono>

using namespace NetBench::Shared;

void dns_test_worker(ThreadMgr::ThreadManager& thread_manager, const json& config_json, const std::string& output_file) {
    try {
        LOG_INFO("[DNS Worker] DNS test worker started" << std::endl);

        NetBench::Shared::DNSConfig dns_config = DNSResultSerializer::deserializeConfig(config_json);

        if (!output_file.empty()) {
            dns_config.export_file_path = output_file;
        }

        // Setup file watchdog for real-time monitoring
        std::unique_ptr<FileWatchdog> watchdog;
        if (!dns_config.export_file_path.empty()) {
            LOG_INFO("[DNS Worker] Starting FileWatchdog thread for: " << dns_config.export_file_path << std::endl);

            watchdog = std::make_unique<FileWatchdog>(
                thread_manager,
                dns_config.export_file_path,
                [](const json& result_json) {
                    try {
                        if (!result_json.contains("hostname")) {
                            return;
                        }

                        // Deserialize the DNS result
                        NetBench::Shared::DNSResult result = DNSResultSerializer::deserializeResult(result_json);

                        // Extract and format data into JSON structure
                        json extracted_data;
                        extracted_data["timestamp"] = std::time(nullptr);
                        extracted_data["hostname"] = result.hostname;
                        extracted_data["query_type"] = result.query_type;
                        extracted_data["success"] = result.success;
                        extracted_data["query_time_ms"] = result.query_time_ms;

                        if (!result.nameserver.empty()) {
                            extracted_data["nameserver"] = result.nameserver;
                        }

                        // Format records
                        if (!result.records.empty()) {
                            json records_array = json::array();
                            for (const auto& record : result.records) {
                                json record_obj;
                                record_obj["type"] = record.type;
                                record_obj["value"] = record.value;
                                record_obj["ttl"] = record.ttl;
                                records_array.push_back(record_obj);
                            }
                            extracted_data["records"] = records_array;
                            extracted_data["records_count"] = result.records.size();
                        } else {
                            extracted_data["records"] = json::array();
                            extracted_data["records_count"] = 0;
                        }

                        if (!result.success && !result.error_message.empty()) {
                            extracted_data["error_message"] = result.error_message;
                        }

                        // Print extracted data as JSON
                        LOG_INFO("[DNS Worker] JSON Data: " << extracted_data.dump(2) << std::endl);

                    } catch (const json::exception& e) {
                        LOG_ERROR("[DNS Worker] JSON error in real-time callback: " << e.what() << std::endl);
                    } catch (const std::exception& e) {
                        LOG_ERROR("[DNS Worker] Error parsing real-time data: " << e.what() << std::endl);
                    }
                },
                100
            );

            watchdog->start();
            LOG_INFO("[DNS Worker] FileWatchdog thread started successfully" << std::endl);
        }

        ::DNSConfig legacy_dns_config;
        legacy_dns_config.hostname = dns_config.hostname;
        legacy_dns_config.query_type = dns_config.query_type;
        legacy_dns_config.nameserver = dns_config.nameserver;
        legacy_dns_config.timeout_ms = dns_config.timeout_ms;
        legacy_dns_config.use_tcp = dns_config.use_tcp;
        legacy_dns_config.export_file_path = dns_config.export_file_path;

        DNSLookupAPI dns;
        dns.setConfig(legacy_dns_config);

        LOG_INFO("[DNS Worker] Starting DNS lookup for: " << dns_config.hostname << std::endl);
        ::DNSResult legacy_result = dns.execute();

        NetBench::Shared::DNSResult result;
        result.hostname = legacy_result.hostname;
        result.query_type = legacy_result.query_type;
        result.success = legacy_result.success;
        result.error_message = legacy_result.error_message;
        result.nameserver = legacy_result.nameserver;
        result.query_time_ms = legacy_result.query_time_ms;

        for (const auto& rec : legacy_result.records) {
            NetBench::Shared::DNSRecord record;
            record.type = rec.type;
            record.value = rec.value;
            record.ttl = rec.ttl;
            result.records.push_back(record);
        }

        if (result.success) {
            LOG_INFO("[DNS Worker] Lookup completed successfully!" << std::endl);
        } else {
            LOG_ERROR("[DNS Worker] Lookup failed: " << result.error_message << std::endl);
        }

        if (watchdog) {
            LOG_INFO("[DNS Worker] Waiting for FileWatchdog to process final data..." << std::endl);
            // Wait longer to ensure file write completes and watchdog processes the data
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            watchdog->stop();
            LOG_INFO("[DNS Worker] FileWatchdog thread stopped" << std::endl);
        }

        LOG_INFO("[DNS Worker] DNS test worker finished" << std::endl);
    } catch (const std::exception& e) {
        LOG_ERROR("[DNS Worker] Error: " << e.what() << std::endl);
    }
}

void traceroute_test_worker(ThreadMgr::ThreadManager& thread_manager, const json& config_json, const std::string& output_file) {
    try {
        LOG_INFO("[Traceroute Worker] Traceroute test worker started" << std::endl);

        traceroute::TracerouteConfig tr_config = traceroute::TracerouteConfig::from_json(config_json);

        if (!output_file.empty()) {
            tr_config.export_file_path = output_file;
        }

        // Setup file watchdog for real-time monitoring
        std::unique_ptr<FileWatchdog> watchdog;
        if (!tr_config.export_file_path.empty()) {
            watchdog = std::make_unique<FileWatchdog>(
                thread_manager,
                tr_config.export_file_path,
                [](const json& result_json) {

                        // Print the raw JSON for real-time monitoring
                    LOG_INFO("[Traceroute Worker] File updated with JSON:" << std::endl);
                    LOG_INFO(result_json.dump(2) << std::endl);

                    // Parse trace configuration if available
                    if (result_json.contains("trace") && result_json["trace"].is_object()) {
                        auto trace = result_json["trace"];
                        LOG_INFO("[Traceroute Worker] Trace Configuration:" << std::endl);
                        LOG_INFO("[Traceroute Worker]   Target: " << trace.value("target", "") << std::endl);
                        LOG_INFO("[Traceroute Worker]   Max Hops: " << trace.value("max_hops", 0) << std::endl);
                        LOG_INFO("[Traceroute Worker]   Timeout: " << trace.value("timeout_ms", 0) << " ms" << std::endl);
                    }

                    // Parse and display hops if available
                    if (result_json.contains("hops") && result_json["hops"].is_array()) {
                        int hop_count = result_json["hops"].size();
                        LOG_INFO("[Traceroute Worker] Current Hops: " << hop_count << std::endl);

                        for (const auto& hop_json : result_json["hops"]) {
                            int hop_num = hop_json.value("hop", 0);
                            std::string ip = hop_json.value("ip", "*");
                            std::string hostname = hop_json.value("hostname", "*");
                            double rtt = hop_json.value("rtt_ms", 0.0);
                            bool timeout = hop_json.value("timeout", false);

                            LOG_INFO("[Traceroute Worker]   Hop " << hop_num << ": ");
                            if (timeout) {
                                LOG_INFO("* * * (timeout)" << std::endl);
                            } else {
                                LOG_INFO(ip);
                                if (hostname != ip && hostname != "*") {
                                    LOG_INFO(" (" << hostname << ")");
                                }
                                LOG_INFO(" - " << rtt << " ms" << std::endl);
                            }
                        }
                    }

                    // Parse and display summary if available
                    if (result_json.contains("summary") && result_json["summary"].is_object()) {
                        auto summary = result_json["summary"];
                        LOG_INFO("[Traceroute Worker] Summary:" << std::endl);
                        LOG_INFO("[Traceroute Worker]   Resolved IP: " << summary.value("resolved_ip", "") << std::endl);
                        LOG_INFO("[Traceroute Worker]   Success: " << (summary.value("success", false) ? "true" : "false") << std::endl);
                        LOG_INFO("[Traceroute Worker]   Total Hops: " << summary.value("total_hops", 0) << std::endl);

                        if (summary.contains("error") && !summary["error"].get<std::string>().empty()) {
                            LOG_ERROR("[Traceroute Worker]   Error: " << summary["error"].get<std::string>() << std::endl);
                        }
                    }
                },
                100
            );
            watchdog->start();
        }

        traceroute::Traceroute tracer;

        auto hop_callback = [](const traceroute::HopInfo& hop) {
            LOG_INFO("[Traceroute Worker] Hop " << hop.hop_number << ": ");
            if (hop.timeout) {
                LOG_INFO("* * * (timeout)" << std::endl);
            } else {
                LOG_INFO(hop.ip_address);
                if (hop.hostname != hop.ip_address) {
                    LOG_INFO(" (" << hop.hostname << ")");
                }
                LOG_INFO(" - " << hop.rtt_ms << " ms" << std::endl);
            }
        };

        LOG_INFO("[Traceroute Worker] Starting traceroute to: " << tr_config.target << std::endl);
        traceroute::TracerouteResult legacy_result = tracer.execute(tr_config, hop_callback);

        TracerouteResult result;
        result.target = legacy_result.target;
        result.resolved_ip = legacy_result.resolved_ip;
        result.success = legacy_result.success;
        result.error_message = legacy_result.error_message;

        for (const auto& hop : legacy_result.hops) {
            HopInfo hop_info;
            hop_info.hop_number = hop.hop_number;
            hop_info.ip_address = hop.ip_address;
            hop_info.hostname = hop.hostname;
            hop_info.rtt_ms = hop.rtt_ms;
            hop_info.timeout = hop.timeout;
            result.hops.push_back(hop_info);
        }

        if (result.success) {
            LOG_INFO("[Traceroute Worker] Reached destination!" << std::endl);
        } else {
            LOG_ERROR("[Traceroute Worker] Failed: " << result.error_message << std::endl);
        }

        if (watchdog) {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            watchdog->stop();
        }

        LOG_INFO("[Traceroute Worker] Traceroute test worker finished" << std::endl);
    } catch (const std::exception& e) {
        LOG_ERROR("[Traceroute Worker] Error: " << e.what() << std::endl);
    }
}

void ping_test_worker(ThreadMgr::ThreadManager& thread_manager, const json& config_json, const std::string& output_file) {
    try {
        LOG_INFO("[Ping Worker] Ping test worker started" << std::endl);

        NetBench::Shared::PingConfig ping_config = PingResultSerializer::deserializeConfig(config_json);

        if (!output_file.empty()) {
            ping_config.export_file_path = output_file;
        }

        // Setup file watchdog for real-time monitoring
        std::unique_ptr<FileWatchdog> watchdog;
        if (!ping_config.export_file_path.empty()) {
            watchdog = std::make_unique<FileWatchdog>(
                thread_manager,
                ping_config.export_file_path,
                [](const json& result_json) {

                        // Print the raw JSON for real-time monitoring
                    LOG_INFO("[Ping Worker] File updated with JSON:" << std::endl);
                    LOG_INFO(result_json.dump(2) << std::endl);

                    // If we have a summary, also print parsed summary
                    if (result_json.contains("summary") && result_json["summary"].is_object()) {
                        auto summary = result_json["summary"];
                        LOG_INFO("[Ping Worker] Test Summary:" << std::endl);
                        LOG_INFO("[Ping Worker]   Packets Sent: " << summary.value("packets_sent", 0) << std::endl);
                        LOG_INFO("[Ping Worker]   Packets Received: " << summary.value("packets_received", 0) << std::endl);
                        LOG_INFO("[Ping Worker]   Packets Lost: " << summary.value("packets_lost", 0) << std::endl);
                        LOG_INFO("[Ping Worker]   Loss: " << summary.value("loss_percentage", 0.0) << "%" << std::endl);
                        LOG_INFO("[Ping Worker]   RTT: min=" << summary.value("rtt_min_ms", 0.0) 
                                  << "ms avg=" << summary.value("rtt_avg_ms", 0.0)
                                  << "ms max=" << summary.value("rtt_max_ms", 0.0) << "ms" << std::endl);
                    }
                },
                100
            );
            watchdog->start();
        }

        ::PingConfig legacy_ping_config;
        legacy_ping_config.destination = ping_config.destination;
        legacy_ping_config.count = ping_config.count;
        legacy_ping_config.timeout_ms = ping_config.timeout_ms;
        legacy_ping_config.interval_ms = ping_config.interval_ms;
        legacy_ping_config.packet_size = ping_config.packet_size;
        legacy_ping_config.ttl = ping_config.ttl;
        legacy_ping_config.resolve_hostname = ping_config.resolve_hostname;
        legacy_ping_config.export_file_path = ping_config.export_file_path;

        PingAPI ping;
        ping.setConfig(legacy_ping_config);

        ping.setRealtimeCallback([](const ::PingRealtimeResult& rt_result) {
            NetBench::Shared::PingRealtimeResult realtime_result;
            realtime_result.sequence = rt_result.sequence;
            realtime_result.rtt_ms = rt_result.rtt_ms;
            realtime_result.ttl = rt_result.ttl;
            realtime_result.success = rt_result.success;
            realtime_result.error_message = rt_result.error_message;

            if (realtime_result.success) {
                LOG_INFO("[Ping Worker] Seq " << realtime_result.sequence 
                          << ": RTT=" << realtime_result.rtt_ms << "ms "
                          << "TTL=" << realtime_result.ttl << std::endl);
            } else {
                LOG_ERROR("[Ping Worker] Seq " << realtime_result.sequence 
                          << ": " << realtime_result.error_message << std::endl);
            }
        });

        LOG_INFO("[Ping Worker] Starting ping test to: " << ping_config.destination << std::endl);
        ::PingResult legacy_result = ping.execute();

        NetBench::Shared::PingResult result;
        result.destination = legacy_result.destination;
        result.ip_address = legacy_result.ip_address;
        result.packets_sent = legacy_result.packets_sent;
        result.packets_received = legacy_result.packets_received;
        result.packets_lost = legacy_result.packets_lost;
        result.loss_percentage = legacy_result.loss_percentage;
        result.min_rtt_ms = legacy_result.min_rtt_ms;
        result.max_rtt_ms = legacy_result.max_rtt_ms;
        result.avg_rtt_ms = legacy_result.avg_rtt_ms;
        result.stddev_rtt_ms = legacy_result.stddev_rtt_ms;
        result.rtt_times = legacy_result.rtt_times;
        result.sequence_numbers = legacy_result.sequence_numbers;
        result.ttl_values = legacy_result.ttl_values;
        result.success = legacy_result.success;
        result.error_message = legacy_result.error_message;

        if (result.success) {
            LOG_INFO("[Ping Worker] Test completed successfully!" << std::endl);
        } else {
            LOG_ERROR("[Ping Worker] Failed: " << result.error_message << std::endl);
        }

        if (watchdog) {
            // Wait longer to ensure final file write completes
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            watchdog->stop();
        }

        // Print final summary directly from result
        if (result.success) {
            LOG_INFO("[Ping Worker] Final Results:" << std::endl);
            LOG_INFO("[Ping Worker]   Destination: " << result.destination << std::endl);
            LOG_INFO("[Ping Worker]   IP Address: " << result.ip_address << std::endl);
            LOG_INFO("[Ping Worker]   Packets Sent: " << result.packets_sent << std::endl);
            LOG_INFO("[Ping Worker]   Packets Received: " << result.packets_received << std::endl);
            LOG_INFO("[Ping Worker]   Loss: " << result.loss_percentage << "%" << std::endl);
            LOG_INFO("[Ping Worker]   RTT: min=" << result.min_rtt_ms 
                      << "ms avg=" << result.avg_rtt_ms 
                      << "ms max=" << result.max_rtt_ms << "ms" << std::endl);
        }

        LOG_INFO("[Ping Worker] Ping test worker finished" << std::endl);
    } catch (const std::exception& e) {
        LOG_ERROR("[Ping Worker] Error: " << e.what() << std::endl);
    }
}

void iperf_test_worker(ThreadMgr::ThreadManager& thread_manager, const json& config_json, const std::string& output_file) {
    try {
        LOG_INFO("[Iperf Worker] Iperf test worker started" << std::endl);

        IperfWrapper iperf;

        json iperf_config = config_json;

        if (!iperf_config.contains("role")) {
            iperf_config["role"] = "client";
        }

        bool need_port = !iperf_config.contains("port") || iperf_config["port"].get<int>() == 0;
        bool need_options = !iperf_config.contains("options") || iperf_config["options"].get<std::string>().empty();
        bool has_servers_list = iperf_config.contains("use_servers_list") && iperf_config["use_servers_list"].get<bool>();
        bool has_hostname = iperf_config.contains("server_hostname") && !iperf_config["server_hostname"].get<std::string>().empty();

        if ((need_port || need_options) && (has_servers_list || (has_hostname && need_port))) {
            if (!iperf_config.contains("servers_list_path") || iperf_config["servers_list_path"].get<std::string>().empty()) {
                LOG_ERROR("[Iperf Worker] ERROR: servers_list_path is required but not provided in config" << std::endl);
                return;
            }

            std::string servers_list_path = iperf_config["servers_list_path"].get<std::string>();
            std::ifstream servers_file(servers_list_path);

            if (servers_file.is_open()) {
                json servers_list;
                try {
                    servers_file >> servers_list;
                    servers_file.close();

                    if (servers_list.is_array() && !servers_list.empty()) {
                        json selected_server;
                        bool found_by_hostname = false;

                        if (has_hostname && need_port) {
                            std::string target_hostname = iperf_config["server_hostname"].get<std::string>();

                            for (const auto& server : servers_list) {
                                if (server.contains("IP/HOST") && server["IP/HOST"].get<std::string>() == target_hostname) {
                                    selected_server = server;
                                    found_by_hostname = true;
                                    break;
                                }
                            }
                        }

                        if (!found_by_hostname) {
                            selected_server = servers_list[0];
                        }

                        if (need_port && selected_server.contains("PORT")) {
                            std::string port_field = selected_server["PORT"].is_string() ? 
                                                    selected_server["PORT"].get<std::string>() : 
                                                    std::to_string(selected_server["PORT"].get<int>());

                            size_t dash_pos = port_field.find('-');
                            int port = 5201;
                            try {
                                if (dash_pos != std::string::npos) {
                                    port = std::stoi(port_field.substr(0, dash_pos));
                                } else {
                                    port = std::stoi(port_field);
                                }
                            } catch (const std::exception& e) {
                                port = 5201;
                            }

                            iperf_config["port"] = port;
                            LOG_INFO("[Iperf Worker] Auto-selected port: " << port << std::endl);
                        }

                        if (need_options && selected_server.contains("OPTIONS") && !selected_server["OPTIONS"].get<std::string>().empty()) {
                            iperf_config["options"] = selected_server["OPTIONS"].get<std::string>();
                        }
                    }
                } catch (const std::exception& e) {
                    LOG_ERROR("[Iperf Worker] Failed to parse servers list: " << e.what() << std::endl);
                }
            }
        }

        if (!output_file.empty()) {
            iperf_config["export_results"] = output_file;
        }

        iperf_config["realtime"] = true;

        // Setup thread-based file watchdog for real-time monitoring
        std::unique_ptr<FileWatchdog> watchdog;
        std::string export_file = iperf_config.value("export_results", "");

        if (!export_file.empty()) {
            LOG_INFO("[Iperf Worker] Starting FileWatchdog thread for: " << export_file << std::endl);

            watchdog = std::make_unique<FileWatchdog>(
                thread_manager,
                export_file,
                [](const json& result_json) {
                    try {
                        json extracted_data;
                        extracted_data["timestamp"] = std::time(nullptr);

                        // Handle intervals array
                        if (result_json.contains("intervals") && result_json["intervals"].is_array()) {
                            int total_intervals = result_json["intervals"].size();
                            extracted_data["total_intervals"] = total_intervals;

                            if (total_intervals > 0) {
                                // Get the latest interval
                                const auto& last_interval = result_json["intervals"][total_intervals - 1];
                                json interval_info;
                                interval_info["interval_number"] = total_intervals;

                                // Extract from formatted data if available
                                if (last_interval.contains("formatted") && last_interval["formatted"].is_object()) {
                                    const auto& formatted = last_interval["formatted"];

                                    if (formatted.contains("sum") && formatted["sum"].is_object()) {
                                        const auto& sum = formatted["sum"];
                                        interval_info["bitrate"] = sum.value("bits_per_second", "N/A");
                                        interval_info["transfer"] = sum.value("bytes", "N/A");
                                        interval_info["duration"] = sum.value("duration", "N/A");
                                        interval_info["retransmits"] = sum.value("retransmits", "N/A");
                                    }

                                    // Extract per-stream data if available
                                    if (formatted.contains("streams") && formatted["streams"].is_array()) {
                                        json streams = json::array();
                                        for (const auto& stream : formatted["streams"]) {
                                            json stream_data;
                                            if (stream.contains("bits_per_second")) {
                                                stream_data["bitrate"] = stream["bits_per_second"];
                                            }
                                            if (stream.contains("rtt")) {
                                                stream_data["rtt"] = stream["rtt"];
                                            }
                                            if (stream.contains("snd_cwnd")) {
                                                stream_data["cwnd"] = stream["snd_cwnd"];
                                            }
                                            if (!stream_data.empty()) {
                                                streams.push_back(stream_data);
                                            }
                                        }
                                        if (!streams.empty()) {
                                            interval_info["streams"] = streams;
                                        }
                                    }
                                }
                                // Fallback to raw data
                                else if (last_interval.contains("data") && last_interval["data"].is_object()) {
                                    const auto& interval_data = last_interval["data"];

                                    // Handle nested data.data structure
                                    const json* actual_data = &interval_data;
                                    if (interval_data.contains("data") && interval_data["data"].is_object()) {
                                        actual_data = &interval_data["data"];
                                    }

                                    if (actual_data->contains("sum") && (*actual_data)["sum"].is_object()) {
                                        const auto& sum = (*actual_data)["sum"];

                                        if (sum.contains("bits_per_second")) {
                                            interval_info["bitrate_bps"] = sum["bits_per_second"].get<double>();
                                            interval_info["bitrate_mbps"] = sum["bits_per_second"].get<double>() / 1000000.0;
                                        }
                                        if (sum.contains("bytes")) {
                                            interval_info["bytes"] = sum["bytes"].get<uint64_t>();
                                            interval_info["megabytes"] = sum["bytes"].get<uint64_t>() / 1000000.0;
                                        }
                                        if (sum.contains("retransmits")) {
                                            interval_info["retransmits"] = sum["retransmits"];
                                        }
                                        if (sum.contains("start") && sum.contains("end")) {
                                            interval_info["start_time"] = sum["start"].get<double>();
                                            interval_info["end_time"] = sum["end"].get<double>();
                                        }
                                    }
                                }

                                extracted_data["latest_interval"] = interval_info;
                            }
                        }

                        // Handle summary data (test completion)
                        if (result_json.contains("summary") && result_json["summary"].is_object()) {
                            const auto& summary = result_json["summary"];

                            if (summary.contains("data") && summary["data"].is_object()) {
                                const auto& summary_data = summary["data"];

                                // Check if this is the final summary
                                if (summary_data.contains("sum_sent") || summary_data.contains("sum_received")) {
                                    json summary_info;
                                    summary_info["test_complete"] = true;

                                    if (summary_data.contains("sum_sent") && summary_data["sum_sent"].is_object()) {
                                        const auto& sum_sent = summary_data["sum_sent"];
                                        json sent_info;

                                        if (sum_sent.contains("bits_per_second")) {
                                            sent_info["bitrate_bps"] = sum_sent["bits_per_second"].get<double>();
                                            sent_info["bitrate_mbps"] = sum_sent["bits_per_second"].get<double>() / 1000000.0;
                                        }
                                        if (sum_sent.contains("bytes")) {
                                            sent_info["bytes"] = sum_sent["bytes"].get<uint64_t>();
                                            sent_info["megabytes"] = sum_sent["bytes"].get<uint64_t>() / 1000000.0;
                                        }
                                        if (sum_sent.contains("retransmits")) {
                                            sent_info["retransmits"] = sum_sent["retransmits"];
                                        }

                                        summary_info["sent"] = sent_info;
                                    }

                                    if (summary_data.contains("sum_received") && summary_data["sum_received"].is_object()) {
                                        const auto& sum_received = summary_data["sum_received"];
                                        json received_info;

                                        if (sum_received.contains("bits_per_second")) {
                                            received_info["bitrate_bps"] = sum_received["bits_per_second"].get<double>();
                                            received_info["bitrate_mbps"] = sum_received["bits_per_second"].get<double>() / 1000000.0;
                                        }
                                        if (sum_received.contains("bytes")) {
                                            received_info["bytes"] = sum_received["bytes"].get<uint64_t>();
                                            received_info["megabytes"] = sum_received["bytes"].get<uint64_t>() / 1000000.0;
                                        }

                                        summary_info["received"] = received_info;
                                    }

                                    extracted_data["summary"] = summary_info;
                                }
                            }
                        }

                        // Print extracted data as JSON
                        LOG_INFO("[Iperf Worker] JSON Data: " << extracted_data.dump(2) << std::endl);

                    } catch (const json::exception& e) {
                        LOG_ERROR("[Iperf Worker] JSON error in real-time callback: " << e.what() << std::endl);
                    } catch (const std::exception& e) {
                        LOG_ERROR("[Iperf Worker] Error parsing real-time data: " << e.what() << std::endl);
                    }
                },
                100  // Check every 100ms
            );

            watchdog->start();
            LOG_INFO("[Iperf Worker] FileWatchdog thread started successfully" << std::endl);
        }

        // Load configuration and run the test
        iperf.loadConfig(iperf_config);

        std::string role = config_json.value("role", "client");
        LOG_INFO("[Iperf Worker] Starting iperf in " << role << " mode" << std::endl);

        int result = iperf.run();

        if (result == 0) {
            LOG_INFO("[Iperf Worker] Test completed successfully!" << std::endl);
        } else {
            std::string error = iperf.getLastError();
            LOG_ERROR("[Iperf Worker] Test failed with code " << result);
            if (!error.empty()) {
                LOG_ERROR(": " << error);
            }
            LOG_ERROR(std::endl);
        }

        // Stop the watchdog thread
        if (watchdog) {
            LOG_INFO("[Iperf Worker] Stopping FileWatchdog thread..." << std::endl);
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            watchdog->stop();
            LOG_INFO("[Iperf Worker] FileWatchdog thread stopped" << std::endl);
        }

        LOG_INFO("[Iperf Worker] Iperf test worker finished" << std::endl);
    } catch (const std::exception& e) {
        LOG_ERROR("[Iperf Worker] Error: " << e.what() << std::endl);
    }
}