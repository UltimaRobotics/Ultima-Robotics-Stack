#pragma once

#include <atomic>
#include <chrono>
#include <string>
#include <mutex>
#include <condition_variable>
#include <unordered_map>
#include <thread>
#include <nlohmann/json.hpp>
#include "../thirdparty/ur-threadder-api/cpp/include/ThreadManager.hpp"
#include <sqlite3.h>
#include "../include/config_loader.h"

// Note: direct_template.h functions are used but included in .cpp
// to avoid unused header warnings in header file

class LicenseSyncCronJob {
public:
    LicenseSyncCronJob(const std::string& configPath, bool verbose = false);
    ~LicenseSyncCronJob();

    // Start the cron job after RPC client connection is established
    bool start();
    void stop();

    // Check if cron job is running
    bool isRunning() const;

    // Handle heartbeat messages from message router
    void handleHeartbeatMessage(const std::string& topic, const std::string& payload);
    
    // Handle license response messages from message router
    void handleLicenseResponse(const std::string& topic, const std::string& payload);

private:
    // Main verification function triggered by heartbeat
    void performLicenseVerification();
    
    // Send license info request with transaction ID
    std::string sendLicenseInfoRequest();
    
    // Send license plan request with transaction ID
    std::string sendLicensePlanRequest();
    
    // Wait for response with timeout
    bool waitForResponse(const std::string& transactionId, std::chrono::milliseconds timeout);
    
    // Process received response
    void processResponse(const nlohmann::json& response);

    // Request license info from ur-licence-mann
    void requestLicenseInfo();

    // Request license plan from ur-licence-mann
    void requestLicensePlan();

    // Compare received data with system database
    bool compareWithSystemDatabase(const nlohmann::json& receivedData);

    // Update system database with received data
    void updateSystemDatabase(const nlohmann::json& receivedData);

    // Initialize SQLite database with license table
    bool initializeDatabase();

    // Load license data from SQLite database
    nlohmann::json loadLicenseData();

    // Save license data to SQLite database
    void saveLicenseData(const nlohmann::json& licenseData);

    // Start periodic scheduling thread
    void startPeriodicScheduling();

    // Stop periodic scheduling thread
    void stopPeriodicScheduling();

    // Periodic scheduling thread function
    void periodicSchedulingThread();

    // Load system database
    nlohmann::json loadSystemDatabase();

    // Save system database
    void saveSystemDatabase(const nlohmann::json& data);

    // Send RPC request to ur-licence-mann
    void sendRpcRequest(const std::string& operation, const nlohmann::json& parameters);

    // Process received license response
    void processLicenseResponse(const nlohmann::json& responseData);

    std::string configPath_;
    bool verbose_;
    std::atomic<bool> running_{false};
    
    // Thread management
    std::shared_ptr<ThreadMgr::ThreadManager> threadManager_;
    
    // Request/response state management
    std::mutex responseMutex_;
    std::condition_variable responseCondition_;
    std::unordered_map<std::string, nlohmann::json> pendingResponses_;
    std::atomic<bool> verificationInProgress_{false};
    
    // Database paths and connection
    std::string systemDatabasePath_ = "./data/system-data.db";
    sqlite3* dbConnection_ = nullptr;
    
    // Configuration loader
    ConfigLoader configLoader_;
    
    // Periodic scheduling
    std::atomic<bool> firstJobCompleted_{false};
    std::atomic<bool> periodicSchedulingRunning_{false};
    std::thread periodicSchedulingThread_;
    std::mutex periodicSchedulingMutex_;
    std::condition_variable periodicSchedulingCondition_;
    
    // Constants
    static constexpr int PERIODIC_INTERVAL_MINUTES = 10;
};
