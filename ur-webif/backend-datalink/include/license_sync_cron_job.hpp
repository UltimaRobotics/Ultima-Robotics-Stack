#pragma once

#include <atomic>
#include <chrono>
#include <string>
#include <mutex>
#include <nlohmann/json.hpp>
#include "../thirdparty/ur-threadder-api/cpp/include/ThreadManager.hpp"

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

    // Set sync interval (default: 5 minutes)
    void setSyncInterval(std::chrono::seconds interval);

    // Trigger immediate sync (for testing)
    void triggerImmediateSync();
    
    // Handle heartbeat messages from message router
    void handleHeartbeatMessage(const std::string& topic, const std::string& payload);
    
    // Handle license response messages from message router
    void handleLicenseResponse(const std::string& topic, const std::string& payload);

private:
    // Main cron thread function
    void cronThreadFunc();

    // Request license info from ur-licence-mann
    void requestLicenseInfo();

    // Request license plan from ur-licence-mann
    void requestLicensePlan();

    // Compare received data with system database
    bool compareWithSystemDatabase(const nlohmann::json& receivedData);

    // Update system database with received data
    void updateSystemDatabase(const nlohmann::json& receivedData);

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
    std::atomic<bool> heartbeatActive_{false};
    std::chrono::seconds syncInterval_{300}; // 5 minutes default

    // Thread management
    std::shared_ptr<ThreadMgr::ThreadManager> threadManager_;
    unsigned int cronThreadId_{0};

    // Sync state management
    std::mutex syncMutex_;
    std::chrono::steady_clock::time_point lastSyncTime_;
    std::chrono::steady_clock::time_point lastHeartbeatTime_;
    bool waitingForLicenseInfo_;
    bool waitingForLicensePlan_;
    nlohmann::json pendingLicenseInfo_;
    nlohmann::json pendingLicensePlan_;

    // Database paths
    std::string systemDatabasePath_ = "./system_database.json";

    // Timeout constants
    const std::chrono::seconds heartbeatTimeout_{10}; // 10 seconds (reasonable for 1s heartbeat)
    const std::chrono::seconds responseTimeout_{30};  // 30 seconds
};
