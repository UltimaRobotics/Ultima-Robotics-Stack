#pragma once

#include <atomic>
#include <chrono>
#include <string>
#include <mutex>
#include <condition_variable>
#include <unordered_map>
#include <nlohmann/json.hpp>
#include "../modules/ur-threadder-api/cpp/include/ThreadManager.hpp"
#include "../ur-mavdiscovery-shared/include/MavlinkDeviceStructs.hpp"

class DeviceDiscoveryCronJob {
public:
    DeviceDiscoveryCronJob(const std::string& configPath, bool verbose = false);
    ~DeviceDiscoveryCronJob();

    // Start the device discovery job after RPC client connection is established
    bool start();
    void stop();

    // Check if discovery job is running
    bool isRunning() const;

    // Set discovery interval (default: 5 minutes)
    void setDiscoveryInterval(std::chrono::seconds interval);

    // Trigger immediate discovery (for testing)
    void triggerImmediateDiscovery();
    
    // Handle heartbeat messages from ur-mavdiscovery
    void handleHeartbeatMessage(const std::string& topic, const std::string& payload);
    
    // Handle device discovery responses from ur-mavdiscovery
    void handleDiscoveryResponse(const std::string& topic, const std::string& payload);

    // Get discovered devices
    std::vector<MavlinkShared::DeviceInfo> getDiscoveredDevices() const;

    // Set RPC request callback function
    void setRpcRequestCallback(std::function<std::string(const std::string&, const std::string&, const std::string&)> callback);

    // Check if heartbeat has been received
    bool hasReceivedHeartbeat() const;

private:
    // Main discovery thread function
    void performDeviceDiscovery();

    // Send device discovery request to ur-mavdiscovery
    std::string sendDiscoveryRequest();
    
    // Send device discovery request with specific local transaction ID
    std::string sendDiscoveryRequestWithLocalId(const std::string& localTransactionId);

    // Wait for response with timeout
    bool waitForResponse(const std::string& transactionId, std::chrono::milliseconds timeout);

    // Process received discovery response
    void processDiscoveryResponse(const nlohmann::json& responseData);

    // Start periodic scheduling
    void startPeriodicScheduling();

    // Stop periodic scheduling
    void stopPeriodicScheduling();

    // Periodic scheduling thread function
    void periodicSchedulingThread();

    // Process discovered devices and trigger events
    void processDiscoveredDevices(const std::vector<MavlinkShared::DeviceInfo>& devices);

    std::string configPath_;
    bool verbose_;
    std::atomic<bool> running_{false};
    std::atomic<bool> discoveryInProgress_{false};
    std::atomic<bool> firstJobCompleted_{false};
    std::atomic<bool> heartbeatReceived_{false}; // Track if heartbeat has been received
    std::chrono::seconds discoveryInterval_{300}; // 5 minutes default

    // Thread management
    std::shared_ptr<ThreadMgr::ThreadManager> threadManager_;
    std::thread periodicSchedulingThread_;
    std::atomic<bool> periodicSchedulingRunning_{false};

    // Response waiting mechanism
    mutable std::mutex responseMutex_;
    std::condition_variable responseCondition_;
    std::unordered_map<std::string, nlohmann::json> pendingResponses_;

    // Periodic scheduling
    mutable std::mutex periodicSchedulingMutex_;
    std::condition_variable periodicSchedulingCondition_;

    // Discovered devices storage
    mutable std::mutex devicesMutex_;
    std::vector<MavlinkShared::DeviceInfo> discoveredDevices_;

    // RPC request callback
    std::function<std::string(const std::string&, const std::string&, const std::string&)> rpcRequestCallback_;

    // Timeout constants
    const std::chrono::seconds heartbeatTimeout_{10}; // 10 seconds
    const std::chrono::seconds responseTimeout_{1};   // 1 second as requested
    const int PERIODIC_INTERVAL_MINUTES = 5;          // 5 minutes for periodic discovery
};
