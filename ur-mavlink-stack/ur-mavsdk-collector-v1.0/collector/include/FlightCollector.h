#pragma once

#include "DataStructures.h"
#include "ConfigParser.h"
#include "JsonFormatter.h"
#include "RpcClientThread.h"
#include <mavsdk/mavsdk.h>
#include <plugins/telemetry/telemetry.h>
#include <plugins/info/info.h>
#include <plugins/param/param.h>
#include <functional>
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <ThreadManager.hpp>
#include <nlohmann/json.hpp>

// Forward declarations to avoid circular dependencies
namespace mavsdk {
class System;
class Telemetry;
class Info;
}

namespace FlightCollector {

/**
 * @brief Main flight data collector class using MAVSDK
 */
class FlightCollector {
public:
    /**
     * @brief Constructor
     */
    FlightCollector();
    
    /**
     * @brief Destructor
     */
    ~FlightCollector();
    
    /**
     * @brief Initialize collector with configuration
     * @param config Connection configuration
     * @return true if initialization successful, false otherwise
     */
    bool initialize(const ConnectionConfig& config);
    
    /**
     * @brief Initialize RPC client thread
     * @param rpcConfigPath Path to RPC configuration file
     * @return true if RPC initialization successful, false otherwise
     */
    bool initializeRpc(const std::string& rpcConfigPath);
    
    /**
     * @brief Connect to flight controller
     * @return true if connection successful, false otherwise
     */
    bool connect();
    
    /**
     * @brief Disconnect from flight controller
     */
    void disconnect();
    
    /**
     * @brief Start data collection
     * @return true if collection started successfully, false otherwise
     */
    bool startCollection();
    
    /**
     * @brief Stop data collection
     */
    void stopCollection();
    
    /**
     * @brief Check if collector is connected
     * @return true if connected, false otherwise
     */
    bool isConnected() const;
    
    /**
     * @brief Check if collection is active
     * @return true if collecting, false otherwise
     */
    bool isCollecting() const;
    
    /**
     * @brief Get current flight data collection
     * @return Copy of current flight data
     */
    FlightDataCollection getFlightData() const;
    
    /**
     * @brief Get vehicle data
     * @return Copy of vehicle data
     */
    VehicleData getVehicleData() const;
    
    /**
     * @brief Get diagnostic data
     * @return Copy of diagnostic data
     */
    DiagnosticData getDiagnosticData() const;
    
    /**
     * @brief Get all parameters
     * @return Copy of parameters map
     */
    std::map<std::string, ParameterInfo> getParameters() const;
    
    /**
     * @brief Get JSON output of current flight data
     * @return JSON string
     */
    std::string getJsonOutput() const;
    
    /**
     * @brief Set verbose mode for JSON output
     * @param verbose Whether to include verbose data (parameters, message_rates)
     */
    void setVerbose(bool verbose);
    
    /**
     * @brief Set data update callback
     * @param callback Function to call when data is updated
     */
    void setDataUpdateCallback(std::function<void(const FlightDataCollection&)> callback);
    
    /**
     * @brief Set connection status callback
     * @param callback Function to call when connection status changes
     */
    void setConnectionCallback(std::function<void(bool)> callback);
    
    /**
     * @brief Get connection statistics
     * @return Connection statistics string
     */
    std::string getConnectionStats() const;
    
    /**
     * @brief Check if RPC client is running
     * @return true if RPC client is running, false otherwise
     */
    bool isRpcRunning() const;
    
    /**
     * @brief Get RPC client instance
     * @return Pointer to RPC client thread
     */
    RpcClientThread* getRpcClient() const;

private:
    // MAVSDK components
    std::unique_ptr<mavsdk::Mavsdk> mavsdk_;
    std::shared_ptr<mavsdk::System> system_;
    std::unique_ptr<mavsdk::Telemetry> telemetry_;
    std::unique_ptr<mavsdk::Info> info_;
    std::unique_ptr<mavsdk::Param> param_;
    
    // Configuration and state
    ConnectionConfig config_;
    FlightDataCollection flight_data_;
    std::atomic<bool> connected_{false};
    std::atomic<bool> collecting_{false};
    std::atomic<bool> verbose_{false};
    
    // Thread safety
    mutable std::mutex data_mutex_;
    mutable std::mutex state_mutex_;
    
    // Callbacks
    std::function<void(const FlightDataCollection&)> data_callback_;
    std::function<void(bool)> connection_callback_;
    
    // Thread management
    std::unique_ptr<ThreadMgr::ThreadManager> thread_manager_;
    unsigned int telemetry_thread_id_;
    unsigned int logging_thread_id_;
    std::atomic<bool> should_stop_{false};
    
    // RPC client
    std::unique_ptr<RpcClientThread> rpc_client_;
    std::atomic<bool> rpc_running_{false};
    
    // Private methods
    bool setupConnection();
    void initializeInfoPlugin(); // Initialize Info plugin and get system information
    void telemetryLoop();      // High-frequency telemetry collection
    void loggingLoop();        // 1-second periodic logging
    void updateFlightData();
    void collectParameters();  // Parameter collection
    void extractSafetyParameters(const std::string& param_name, float param_value);  // Extract safety-related parameters
    void notifyDataUpdate();
    void notifyConnectionChange(bool connected);
    void updateMessageRate(uint16_t message_id);
    void updateDiagnosticParameter(const std::string& name, float value);
    void resetData();
    void setupRpcMessageHandler(); // Setup RPC message handler
    void onRpcMessage(const std::string& topic, const std::string& payload); // Handle RPC messages
    void handleGetFlightData(const nlohmann::json& request);
    void handleGetVehicleInfo(const nlohmann::json& request);
    void handleStartCollection(const nlohmann::json& request);
    void handleStopCollection(const nlohmann::json& request);
    void sendErrorResponse(const nlohmann::json& requestId, const std::string& errorMessage);
    std::string mapVendorIdToName(int32_t vendor_id);
    std::string mapProductIdToName(int32_t product_id);
    
    // Helper methods
    std::string flightModeToString(int mode) const;
    std::string flightModeNumberToName(int mode_number) const;  // Convert ArduPilot flight mode numbers to names
    std::string vehicleTypeToString(int type) const;
};

} // namespace FlightCollector
