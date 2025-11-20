#include <CLI11.hpp>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <signal.h>
#include <sstream>
#include <thread>

#include "config_manager.hpp"
#include "feature_manager.hpp"
#include "file_watchdog.hpp"
#include "init_manager.hpp"
#include "license_manager.hpp"
#include "operation_handler.hpp"
#include "package_config.hpp"
#include "rpc_client.hpp"
#include "rpc_operation_processor.hpp"
#include "startup_manager.hpp"

// Global flag for graceful shutdown
static std::atomic<bool> g_running(true);

// Global RPC client and operation processor for signal handling
std::shared_ptr<RpcClient> g_rpcClient;
std::unique_ptr<RpcOperationProcessor> g_operationProcessor;

void signalHandler(int signal) {
  std::cout << "\n[Main] Caught signal " << signal << ", shutting down..."
            << std::endl;
  if (g_rpcClient) {
    g_rpcClient->stop();
  }
  g_running.store(false);
  exit(0);
}

PackageConfig load_package_config(const std::string &config_path,
                                  bool verbose) {
  PackageConfig config;

  std::ifstream file(config_path);
  if (!file.good()) {
    if (verbose) {
      std::cout << "Package config file not found, using defaults" << std::endl;
    }
    return config;
  }

  try {
    nlohmann::json j;
    file >> j;
    config = PackageConfig::from_json(j);
    if (verbose) {
      std::cout << "Loaded package config from: " << config_path << std::endl;
    }
  } catch (const std::exception &e) {
    std::cerr << "Error loading package config: " << e.what() << std::endl;
  }

  return config;
}

int main(int argc, char **argv) {
  CLI::App app{"ur-licence-mann - Advanced License Management Tool"};

  bool verbose = false;
  std::string package_config_path;
  std::string rpc_config_path;

  app.add_flag("-v,--verbose", verbose, "Enable verbose output");
  app.add_option("--package-config", package_config_path,
                 "Path to package configuration JSON file")
      ->required();
  app.add_option("--rpc-config", rpc_config_path,
                 "Path to RPC configuration JSON file");

  try {
    app.parse(argc, argv);
  } catch (const CLI::ParseError &e) {
    return app.exit(e);
  }

  // Setup signal handlers for graceful shutdown
  signal(SIGINT, signalHandler);
  signal(SIGTERM, signalHandler);

  std::cout << "[Main] Starting ur-licence-mann..." << std::endl;
  std::cout << "[Main] Package config: " << package_config_path << std::endl;

  // Load package configuration
  PackageConfig pkg_config = load_package_config(package_config_path, verbose);

  // Initialize license manager components
  if (!InitManager::initialize(pkg_config, verbose)) {
    std::cerr << "[Main] Initialization failed" << std::endl;
    return 1;
  }

  // If RPC config is provided, launch RPC client thread
  if (!rpc_config_path.empty()) {
    std::cout << "[Main] RPC config: " << rpc_config_path << std::endl;

    // Initialize RPC client
    g_rpcClient = std::make_shared<RpcClient>(rpc_config_path, "ur-licence-mann");

    // Initialize operation processor
    g_operationProcessor =
        std::make_unique<RpcOperationProcessor>(pkg_config, verbose);

    // CRITICAL: Set message handler BEFORE starting the client
    std::cout << "[Main] Setting up message handler..." << std::endl;
    g_rpcClient->setMessageHandler(
        [&](const std::string &topic, const std::string &payload) {
          if (verbose) {
            std::cout << "[Main] Custom handler received message on topic: " << topic
                      << std::endl;
          }
          // Only process messages on the request topic
          if (topic.find("direct_messaging/ur-licence-mann/requests") ==
              std::string::npos) {
            return;
          }

          // Process the request using operation processor
          if (g_operationProcessor) {
            g_operationProcessor->processRequest(payload.c_str(), payload.size());
          }
        });
    std::cout << "[Main] Message handler configured successfully" << std::endl;

    // Start RPC client - handler must be set first
    std::cout << "[Main] Starting RPC client..." << std::endl;
    if (!g_rpcClient->start()) {
      std::cerr << "[Main] Failed to start RPC client" << std::endl;
      return 1;
    }

    // Wait for RPC client to initialize
    std::this_thread::sleep_for(std::chrono::seconds(2));

    if (!g_rpcClient->isRunning()) {
      std::cerr << "[Main] RPC client failed to start" << std::endl;
      return 1;
    }

    std::cout << "[Main] RPC client is running and ready to process requests"
              << std::endl;
    std::cout
        << "[Main] Listening on: direct_messaging/ur-licence-mann/requests"
        << std::endl;
    std::cout
        << "[Main] Responding on: direct_messaging/ur-licence-mann/responses"
        << std::endl;
    std::cout << "[Main] Press Ctrl+C to stop..." << std::endl;

    // Main loop - keep running until signal received
    while (g_running.load()) {
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    // Graceful shutdown
    std::cout << "[Main] Shutting down RPC client..." << std::endl;
    g_rpcClient->stop();

    // Wait for RPC thread to finish
    // The thread is started implicitly by g_rpcClient->start()
    // We need to join it if it was explicitly created, but in this case,
    // the RpcClient manages its own thread. Stopping it should be sufficient.
    // If rpcThread.joinable() were true, we'd join it here.

    std::cout << "[Main] Application stopped" << std::endl;
    return 0;
  }

  std::cout << "ur-licence-mann - Advanced License Management Tool\n\n";
  std::cout << "RPC-Based Interface:\n";
  std::cout << "  --package-config <file>  Package configuration JSON file "
               "(required)\n";
  std::cout << "  --rpc-config <file>      RPC configuration JSON file\n";
  std::cout << "  -v, --verbose            Enable verbose output\n\n";

  std::cout << "Usage:\n";
  std::cout << "  RPC Mode (recommended):\n";
  std::cout << "    ./ur-licence-mann --package-config config.json "
               "--rpc-config ur-rpc-config.json\n\n";

  std::cout << "  The application will:\n";
  std::cout << "    1. Initialize license management system\n";
  std::cout << "    2. Start RPC client and listen for operation requests\n";
  std::cout << "    3. Process operations (generate/verify/update/etc.) via "
               "RPC messages\n\n";

  std::cout << "RPC Request Format:\n";
  std::cout << "  Send JSON messages to the request topic specified in "
               "ur-rpc-config.json:\n";
  std::cout << "  {\n";
  std::cout << "    \"operation\": \"generate\" | \"verify\" | \"update\" | "
               "\"get_license_info\" | \"get_license_plan\" | "
               "\"get_license_definitions\",\n";
  std::cout << "    \"parameters\": {\n";
  std::cout << "      \"license_file\": \"path/to/license.lic\",\n";
  std::cout << "      \"output\": \"path/to/output.lic\",\n";
  std::cout << "      ...\n";
  std::cout << "    }\n";
  std::cout << "  }\n\n";

  std::cout << "Note: Encryption keys are automatically generated on first run "
               "and stored securely.\n";
  std::cout << "      License definitions are automatically encrypted when "
               "auto_encrypt_definitions=true.\n";
  std::cout << "      All licenses include hardware fingerprints and "
               "signatures when required.\n";

  return 0;
}