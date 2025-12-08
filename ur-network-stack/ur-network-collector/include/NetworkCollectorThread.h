#ifndef NETWORK_COLLECTOR_THREAD_H
#define NETWORK_COLLECTOR_THREAD_H

#include <string>
#include <vector>
#include <memory>
#include <atomic>
#include <mutex>
#include <thread>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

// Include the complete ThreadManager header for ThreadInfo struct
#include "ThreadManager.hpp"

// Forward declarations for C threadder API
extern "C" {
    #include "thread_manager.h"
}

struct CollectionConfig {
    bool collectVlan = false;
    bool collectNat = false;
    bool collectFirewall = false;
    bool collectRoutes = false;
    bool collectBridges = false;
    bool collectAll = true;
    bool outputText = false;
    bool quietMode = false;
    bool continuousMode = false;
    std::string outputFile;
    int collectionInterval = 5; // seconds between collections
    int publishingInterval = 10; // seconds between MQTT publishing
    bool enableMqttPublishing = false;
    std::string mqttRuntimeTopic = "ur-shared-bus/ur-network-stack/ur-net-collector/runtime";
};

// Global data variables for publishing thread
extern json g_vlanData;
extern json g_natData;
extern json g_firewallData;
extern json g_routesData;
extern json g_bridgesData;
extern std::mutex g_dataMutex;
extern std::atomic<bool> g_dataUpdated;
extern std::atomic<bool> g_publishingThreadShouldStop;

class NetworkCollectorThread {
private:
    std::unique_ptr<ThreadMgr::ThreadManager> threadManager_;
    std::atomic<bool> isRunning_;
    std::atomic<bool> shouldStop_;
    unsigned int threadId_;
    mutable std::mutex configMutex_;
    CollectionConfig config_;
    std::string lastCollectedData_;
    mutable std::mutex dataMutex_;
    
    // Publishing thread management
    std::atomic<bool> publishingThreadRunning_;
    unsigned int publishingThreadId_;
    std::thread publishingThread_;

    /**
     * @brief Publish data to MQTT topic
     * @param data JSON data to publish
     * @param dataType Type of network data (vlan, nat, firewall, routes, bridges)
     */
    void publishToMqtt(const std::string& data, const std::string& dataType);
    
    /**
     * @brief Publish all network data split by type
     * @param data Complete JSON data containing all network types
     */
    void publishSplitDataToMqtt(const std::string& data);
    
    /**
     * @brief Publishing thread function - runs every second
     */
    void publishingThreadFunc();
    
    /**
     * @brief Update global data variables with collected data
     * @param data Complete JSON data containing all network types
     */
    void updateGlobalData(const std::string& data);
    
    /**
     * @brief Reset publish sequence tracking for new cycle
     */
    void resetPublishSequence();

public:
    NetworkCollectorThread();
    ~NetworkCollectorThread();

    // Disable copy construction and assignment
    NetworkCollectorThread(const NetworkCollectorThread&) = delete;
    NetworkCollectorThread& operator=(const NetworkCollectorThread&) = delete;

    // Enable move construction and assignment
    NetworkCollectorThread(NetworkCollectorThread&& other) noexcept;
    NetworkCollectorThread& operator=(NetworkCollectorThread&& other) noexcept;

    /**
     * @brief Start the network collection thread
     * @param config Collection configuration
     * @return true if started successfully
     */
    bool start(const CollectionConfig& config);

    /**
     * @brief Stop the network collection thread
     */
    void stop();

    /**
     * @brief Check if the thread is currently running
     * @return true if running
     */
    bool isRunning() const;

    /**
     * @brief Get the last collected data
     * @return JSON string of last collected data
     */
    std::string getLastCollectedData() const;

    /**
     * @brief Update collection configuration
     * @param config New configuration
     */
    void updateConfig(const CollectionConfig& config);

    /**
     * @brief Get current configuration
     * @return Current configuration
     */
    CollectionConfig getConfig() const;

    /**
     * @brief Get thread information
     * @return Thread information structure
     */
    ThreadMgr::ThreadInfo getThreadInfo() const;

private:
    /**
     * @brief Thread function that performs data collection
     */
    void networkCollectorThreadFunc();

    /**
     * @brief Static thread function wrapper
     * @param instance Pointer to NetworkCollectorThread instance
     */
    static void* staticThreadFunc(void* instance);

    /**
     * @brief Collect network data based on current configuration
     * @return JSON string containing collected data
     */
    std::string collectNetworkData();

    /**
     * @brief Get current timestamp
     * @return Timestamp string
     */
    std::string getCurrentTimestamp() const;
};

#endif // NETWORK_COLLECTOR_THREAD_H
