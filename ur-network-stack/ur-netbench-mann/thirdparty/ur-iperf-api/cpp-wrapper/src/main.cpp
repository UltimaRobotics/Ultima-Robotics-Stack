#include "IperfWrapper.hpp"
#include <iostream>
#include <fstream>
#include <string>
#include <cstdlib>
#include <ctime>
#include <iomanip>
#include <sstream>

std::string getTimestampedLogFilename() {
    auto now = std::time(nullptr);
    auto tm = *std::localtime(&now);
    std::ostringstream oss;
    oss << "iperf_results_" 
        << std::put_time(&tm, "%Y%m%d_%H%M%S") 
        << ".json";
    return oss.str();
}

void printUsage(const char* programName) {
    std::cout << "Usage: " << programName << " [OPTIONS]\n\n"
              << "Options:\n"
              << "  -c, --config <file>     Load configuration from JSON file\n"
              << "  -j, --json <string>     Load configuration from JSON string\n"
              << "  -r, --realtime          Display real-time JSON results during test\n"
              << "  -s, --stream            Enable streaming mode (JSONL output, real-time)\n"
              << "  -l, --log-results [file] Log results to timestamped JSON file\n"
              << "                          (default: iperf_results_YYYYMMDD_HHMMSS.json)\n"
              << "  -x, --export-results <file> Export real-time results to a JSON file\n"
              << "  -h, --help              Show this help message\n"
              << "  -e, --example           Show example JSON configurations\n\n"
              << "Examples:\n"
              << "  " << programName << " --config client.json\n"
              << "  " << programName << " --json '{\"role\":\"server\",\"port\":5201}'\n"
              << "  " << programName << " --config client.json --realtime\n"
              << "  " << programName << " --config client.json --log-results\n"
              << "  " << programName << " --config client.json --log-results mytest.json --realtime\n"
              << "  " << programName << " --config client.json --export-results results.json\n";
}

void printExamples() {
    std::cout << "\n=== Example JSON Configurations ===\n\n";

    std::cout << "Server Configuration (basic):\n"
              << "{\n"
              << "  \"role\": \"server\",\n"
              << "  \"port\": 5201,\n"
              << "  \"json\": true\n"
              << "}\n\n";

    std::cout << "Client Configuration (basic):\n"
              << "{\n"
              << "  \"role\": \"client\",\n"
              << "  \"server_hostname\": \"127.0.0.1\",\n"
              << "  \"port\": 5201,\n"
              << "  \"duration\": 10,\n"
              << "  \"json\": true\n"
              << "}\n\n";

    std::cout << "Client Configuration (advanced):\n"
              << "{\n"
              << "  \"role\": \"client\",\n"
              << "  \"server_hostname\": \"192.168.1.100\",\n"
              << "  \"port\": 5201,\n"
              << "  \"protocol\": \"tcp\",\n"
              << "  \"duration\": 30,\n"
              << "  \"omit\": 3,\n"
              << "  \"bandwidth\": 10000000,\n"
              << "  \"num_streams\": 4,\n"
              << "  \"parallel\": 4,\n"
              << "  \"blksize\": 131072,\n"
              << "  \"buffer_size\": 262144,\n"
              << "  \"interval\": 1.0,\n"
              << "  \"reverse\": false,\n"
              << "  \"bidirectional\": false,\n"
              << "  \"no_delay\": true,\n"
              << "  \"congestion_control\": \"cubic\",\n"
              << "  \"json\": true,\n"
              << "  \"verbose\": true,\n"
              << "  \"zerocopy\": false\n"
              << "}\n\n";

    std::cout << "UDP Client Configuration:\n"
              << "{\n"
              << "  \"role\": \"client\",\n"
              << "  \"server_hostname\": \"192.168.1.100\",\n"
              << "  \"port\": 5201,\n"
              << "  \"protocol\": \"udp\",\n"
              << "  \"bandwidth\": 1000000,\n"
              << "  \"duration\": 10,\n"
              << "  \"blksize\": 1460,\n"
              << "  \"json\": true\n"
              << "}\n\n";

    std::cout << "=== Field Descriptions ===\n\n";
    std::cout << "REQUIRED FIELDS:\n"
              << "  role              : \"client\" or \"server\"\n"
              << "  server_hostname   : Server IP/hostname (required for client)\n\n";

    std::cout << "OPTIONAL FIELDS:\n"
              << "  port              : Server port (default: 5201)\n"
              << "  bind_port         : Local port to bind\n"
              << "  protocol          : \"tcp\", \"udp\", or \"sctp\" (default: tcp)\n"
              << "  duration          : Test duration in seconds (default: 10)\n"
              << "  omit              : Omit initial seconds from results (default: 0)\n"
              << "  bandwidth         : Target bandwidth in bits/sec (0 = unlimited)\n"
              << "  num_streams       : Number of parallel streams\n"
              << "  parallel          : Same as num_streams\n"
              << "  blksize           : Block size for read/write\n"
              << "  buffer_size       : Socket buffer size\n"
              << "  bytes             : Number of bytes to transmit\n"
              << "  blocks            : Number of blocks to transmit\n"
              << "  burst             : Number of packets to burst\n"
              << "  interval          : Reporting interval in seconds\n"
              << "  bind_address      : Local address to bind\n"
              << "  bind_dev          : Device to bind to\n"
              << "  reverse           : Run in reverse mode (server sends)\n"
              << "  bidirectional     : Run bidirectional test\n"
              << "  json              : Output in JSON format\n"
              << "  verbose           : Verbose output\n"
              << "  zerocopy          : Use zero-copy mode\n"
              << "  tos               : Type of Service (TOS) value\n"
              << "  no_delay          : Disable Nagle's algorithm (TCP_NODELAY)\n"
              << "  congestion_control: TCP congestion control algorithm\n"
              << "  mss               : Maximum Segment Size (MSS)\n"
              << "  timestamps        : Include timestamps in output\n"
              << "  one_off           : Accept only one connection then exit\n"
              << "  get_server_output : Get server output (client mode)\n"
              << "  udp_counters_64bit: Use 64-bit counters for UDP\n"
              << "  repeating_payload : Use repeating payload\n"
              << "  dont_fragment     : Set Don't Fragment bit\n"
              << "  username          : Username for authentication\n"
              << "  password          : Password for authentication\n"
              << "  logfile           : Write output to logfile\n\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }

    std::string configFile;
    std::string jsonString;
    std::string logFile;
    std::string exportFile;
    bool useFile = false;
    bool useString = false;
    bool enableRealtime = false;
    bool enableLogging = false;
    bool enableExport = false;
    bool enableStreaming = false;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];

        if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            return 0;
        } else if (arg == "-e" || arg == "--example") {
            printExamples();
            return 0;
        } else if (arg == "-c" || arg == "--config") {
            if (i + 1 < argc) {
                configFile = argv[++i];
                useFile = true;
            } else {
                std::cerr << "Error: --config requires a file path\n";
                return 1;
            }
        } else if (arg == "-j" || arg == "--json") {
            if (i + 1 < argc) {
                jsonString = argv[++i];
                useString = true;
            } else {
                std::cerr << "Error: --json requires a JSON string\n";
                return 1;
            }
        } else if (arg == "-r" || arg == "--realtime") {
            enableRealtime = true;
        } else if (arg == "-s" || arg == "--stream") {
            enableStreaming = true;
        } else if (arg == "-l" || arg == "--log-results") {
            enableLogging = true;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                logFile = argv[++i];
            } else {
                logFile = getTimestampedLogFilename();
            }
        } else if (arg == "-x" || arg == "--export-results") {
            enableExport = true;
            if (i + 1 < argc) {
                exportFile = argv[++i];
            } else {
                std::cerr << "Error: --export-results requires a file path\n";
                return 1;
            }
        } else {
            std::cerr << "Unknown option: " << arg << "\n";
            printUsage(argv[0]);
            return 1;
        }
    }

    if (!useFile && !useString) {
        std::cerr << "Error: Either --config or --json must be specified\n";
        printUsage(argv[0]);
        return 1;
    }

    try {
        IperfWrapper iperf;

        iperf.setOnTestStart([]() {
            std::cout << "Test starting...\n";
        });

        iperf.setOnTestFinish([]() {
            std::cout << "Test finished.\n";
        });

        // Load config first (which may set output options from config file)
        if (useFile) {
            std::cout << "Loading configuration from file: " << configFile << "\n";
            iperf.loadConfigFromFile(configFile);
        } else {
            std::cout << "Loading configuration from JSON string\n";
            json config = json::parse(jsonString);
            iperf.loadConfig(config);
        }
        
        // Command-line args override config file settings
        if (enableRealtime) {
            iperf.enableRealtimeJsonOutput(true);
        }

        if (enableStreaming) {
            iperf.enableStreamingMode(true);
        }
        
        if (enableLogging) {
            iperf.enableLogToFile(logFile);
        }
        
        if (enableExport) {
            iperf.enableExportToFile(exportFile);
        }
        
        // Set callback for realtime output
        iperf.setOnJsonOutput([](const std::string& jsonData) {
            std::cout << "[REALTIME] " << jsonData << std::endl;
            std::cout.flush();
        });

        std::cout << "Running iperf3 test...\n";
        int result = iperf.run();

        if (result < 0) {
            std::cerr << "Error running test: " << iperf.getLastError() << "\n";
            return 1;
        }

        std::string jsonOutput = iperf.getJsonOutput();
        if (!jsonOutput.empty() && jsonOutput != "{}") {
            std::cout << "\n=== Final JSON Output ===\n";
            std::cout << jsonOutput << "\n";
        }

        std::cout << "\nTest completed successfully.\n";
        if (enableLogging) {
            std::cout << "Results saved to: " << logFile << "\n";
        }
        if (enableExport) {
            std::cout << "Results exported to: " << exportFile << "\n";
        }
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}