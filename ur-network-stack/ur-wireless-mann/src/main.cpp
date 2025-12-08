
#include "urwt/rpc/rpc_service.hpp"
#include <iostream>
#include <string>
#include <cstring>

void printUsage(const char* program_name) {
    std::cout << "UR Wireless Manager - RPC Service Mode\n\n";
    std::cout << "Usage:\n";
    std::cout << "  " << program_name << " -c <rpc-config-file> -wc <wireless-config-file>\n\n";
    std::cout << "Arguments:\n";
    std::cout << "  -c <file>     Path to ur-rpc-config.json (required)\n";
    std::cout << "  -wc <file>    Path to wireless-config.json (required)\n\n";
    std::cout << "Example:\n";
    std::cout << "  " << program_name << " -c config/ur-rpc-config.json -wc config/wireless-config.json\n";
}

int main(int argc, char* argv[]) {
    if (argc == 2) {
        std::string arg = argv[1];
        if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            return 0;
        }
    }

    if (argc != 5) {
        printUsage(argv[0]);
        return 1;
    }

    std::string rpcConfigPath;
    std::string wirelessConfigPath;

    for (int i = 1; i < argc; i++) {
        if (std::strcmp(argv[i], "-c") == 0 && i + 1 < argc) {
            rpcConfigPath = argv[i + 1];
            i++;
        } else if (std::strcmp(argv[i], "-wc") == 0 && i + 1 < argc) {
            wirelessConfigPath = argv[i + 1];
            i++;
        }
    }

    if (rpcConfigPath.empty() || wirelessConfigPath.empty()) {
        std::cerr << "Error: Both -c and -wc arguments are required\n\n";
        printUsage(argv[0]);
        return 1;
    }

    try {
        urwt::rpc::RPCService service;
        
        auto initResult = service.initialize(rpcConfigPath, wirelessConfigPath);
        if (initResult.has_value()) {
            std::cerr << "Failed to initialize RPC service: " << *initResult << std::endl;
            return 1;
        }

        std::cout << "UR Wireless Manager RPC Service started successfully" << std::endl;
        std::cout << "RPC Config: " << rpcConfigPath << std::endl;
        std::cout << "Wireless Config: " << wirelessConfigPath << std::endl;
        std::cout << "Listening for RPC requests..." << std::endl;

        auto runResult = service.run();
        if (runResult.has_value()) {
            std::cerr << "RPC service error: " << *runResult << std::endl;
            return 1;
        }

        std::cout << "UR Wireless Manager RPC Service stopped" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
