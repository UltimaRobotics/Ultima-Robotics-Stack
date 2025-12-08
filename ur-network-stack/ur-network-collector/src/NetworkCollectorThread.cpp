#include "NetworkCollectorThread.h"
#include "NetworkCollectorThreadGlobals.h"
#include "NetworkCollector.h"
#include "VlanCollector.h"
#include "NatCollector.h"
#include "FirewallCollector.h"
#include "RouteCollector.h"
#include "BridgeCollector.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <chrono>
#include <iomanip>
#include <thread>
#include <nlohmann/json.hpp>

// C includes for ur-rpc-template
extern "C" {
#include "../thirdparty/ur-rpc-template/deps/cJSON/cJSON.h"
#include "../thirdparty/ur-rpc-template/extensions/direct_template.h"
#include "../thirdparty/ur-rpc-template/ur-rpc-template.h"
}

using json = nlohmann::json;

NetworkCollectorThread::NetworkCollectorThread()
    : isRunning_(false)
    , shouldStop_(false)
    , threadId_(0)
    , publishingThreadRunning_(false)
    , publishingThreadId_(0) {
    
    // Initialize thread manager with capacity for 10 threads
    threadManager_ = std::make_unique<ThreadMgr::ThreadManager>(10);
}

NetworkCollectorThread::~NetworkCollectorThread() {
    stop();
    
    // Stop publishing thread
    if (publishingThreadRunning_.load()) {
        g_publishingThreadShouldStop.store(true);
        if (publishingThread_.joinable()) {
            publishingThread_.join();
        }
        publishingThreadRunning_.store(false);
    }
}

NetworkCollectorThread::NetworkCollectorThread(NetworkCollectorThread&& other) noexcept
    : threadManager_(std::move(other.threadManager_))
    , isRunning_(other.isRunning_.load())
    , shouldStop_(other.shouldStop_.load())
    , threadId_(other.threadId_)
    , config_(std::move(other.config_))
    , lastCollectedData_(std::move(other.lastCollectedData_)) {
    
    other.isRunning_ = false;
    other.shouldStop_ = false;
    other.threadId_ = 0;
}

NetworkCollectorThread& NetworkCollectorThread::operator=(NetworkCollectorThread&& other) noexcept {
    if (this != &other) {
        stop();
        
        threadManager_ = std::move(other.threadManager_);
        isRunning_ = other.isRunning_.load();
        shouldStop_ = other.shouldStop_.load();
        threadId_ = other.threadId_;
        config_ = std::move(other.config_);
        lastCollectedData_ = std::move(other.lastCollectedData_);
        
        other.isRunning_ = false;
        other.shouldStop_ = false;
        other.threadId_ = 0;
    }
    return *this;
}

void NetworkCollectorThread::resetPublishSequence() {
    // Set the reset flag to trigger sequence reset in publishToMqtt
    // This ensures each publish cycle starts with clean sequence information
    extern bool g_resetPublishSequence;
    g_resetPublishSequence = true;
}

void NetworkCollectorThread::publishToMqtt(const std::string& data, const std::string& dataType) {
    if (!config_.enableMqttPublishing || config_.mqttRuntimeTopic.empty()) {
        return;
    }
    
    // Add rate limiting - don't publish if data is too large or empty
    const size_t MAX_MESSAGE_SIZE = 8192; // 3KB limit per message
    if (data.empty()) {
        if (!config_.quietMode) {
            std::cout << "[PublishingThread] Skipping empty " << dataType << " data" << std::endl;
        }
        return;
    }
    
    if (data.length() > MAX_MESSAGE_SIZE) {
        static int skippedPublishes = 0;
        skippedPublishes++;
        
        if (!config_.quietMode) {
            std::cout << "[PublishingThread] Skipping " << dataType << " data - message too large:" << std::endl;
            std::cout << "  ├─ Size: " << data.length() << " bytes" << std::endl;
            std::cout << "  ├─ Limit: " << MAX_MESSAGE_SIZE << " bytes (8KB)" << std::endl;
            std::cout << "  └─ Total skipped: " << skippedPublishes << " messages" << std::endl;
        }
        
        if (!config_.quietMode) {
            std::cerr << "[PublishingThread] Skipped " << dataType 
                     << " data - message size " << data.length() 
                     << " bytes exceeds 8KB limit" << std::endl;
        }
        return;
    }
    
    static int successfulPublishes = 0;
    static int failedPublishes = 0;
    static std::string lastDataType = "";
    
    // Check if we need to reset the sequence (called from resetPublishSequence)
    if (g_resetPublishSequence) {
        lastDataType = "";
        g_resetPublishSequence = false;
    }
    
    try {
        // Create topic name based on data type with special handling for split firewall data
        std::string topic;
        if (dataType == "firewall-top") {
            topic = "ur-shared-bus/ur-network-stack/ur-net-collector/runtime-firewall-top";
        } else if (dataType == "firewall-bot") {
            topic = "ur-shared-bus/ur-network-stack/ur-net-collector/runtime-firewall-bot";
        } else {
            topic = config_.mqttRuntimeTopic + "-" + dataType;
        }
        
        // Log publish action metadata (only in verbose mode)
        if (!config_.quietMode) {
            auto now = std::chrono::system_clock::now();
            auto time_t = std::chrono::system_clock::to_time_t(now);
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch()) % 1000;
            
            std::cout << "[PublishingThread] Publishing " << dataType << " data:" << std::endl;
            std::cout << "  ├─ Topic: " << topic << std::endl;
            std::cout << "  ├─ Size: " << data.length() << " bytes" << std::endl;
            std::cout << "  ├─ Timestamp: " << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
            std::cout << "." << std::setfill('0') << std::setw(3) << ms.count() << std::endl;
            
            // Show sequence information
            if (!lastDataType.empty()) {
                std::cout << "  ├─ Sequence: " << lastDataType << " → " << dataType << " (+200ms delay)" << std::endl;
            } else {
                std::cout << "  ├─ Sequence: Start of publish cycle" << std::endl;
            }
        }
        
        // Use ur-rpc-template to publish to MQTT topic
        auto publish_start = std::chrono::high_resolution_clock::now();
        int result = direct_client_publish_raw_message(topic.c_str(), 
                                                        data.c_str(), 
                                                        data.length());
        auto publish_end = std::chrono::high_resolution_clock::now();
        auto publish_duration = std::chrono::duration_cast<std::chrono::microseconds>(publish_end - publish_start);
        
        if (result != 0) {
            failedPublishes++;
            if (!config_.quietMode) {
                std::cout << "  ├─ Status: FAILED (error code: " << result << ")" << std::endl;
                std::cout << "  └─ Publish time: " << publish_duration.count() << " μs" << std::endl;
            }
            if (!config_.quietMode) {
                std::cerr << "[PublishingThread] Failed to publish to MQTT topic: " 
                         << topic << " (error: " << result << ")" << std::endl;
            }
        } else {
            successfulPublishes++;
            if (!config_.quietMode) {
                std::cout << "  ├─ Status: SUCCESS" << std::endl;
                std::cout << "  └─ Publish time: " << publish_duration.count() << " μs" << std::endl;
            }
            if (!config_.quietMode) {
                std::cout << "[PublishingThread] Successfully published " << dataType 
                         << " data to topic: " << topic << std::endl;
            }
        }
        
        // Update last data type for sequence tracking
        lastDataType = dataType;
        
    } catch (const std::exception& e) {
        failedPublishes++;
        std::cout << "  ├─ Status: EXCEPTION" << std::endl;
        std::cout << "  └─ Error: " << e.what() << std::endl;
        if (!config_.quietMode) {
            std::cerr << "[PublishingThread] MQTT publishing error for " 
                     << dataType << ": " << e.what() << std::endl;
        }
    } catch (...) {
        failedPublishes++;
        std::cout << "  ├─ Status: UNKNOWN ERROR" << std::endl;
        std::cout << "  └─ Error: Unknown exception occurred" << std::endl;
        if (!config_.quietMode) {
            std::cerr << "[PublishingThread] Unknown MQTT publishing error for " 
                     << dataType << std::endl;
        }
    }
}

void NetworkCollectorThread::publishSplitDataToMqtt(const std::string& data) {
    if (!config_.enableMqttPublishing) {
        return;
    }
    
    try {
        // Parse the complete JSON data
        json completeData = json::parse(data);
        
        // Check if data contains the expected structure
        if (!completeData.contains("data") || !completeData["data"].is_array()) {
            if (!config_.quietMode) {
                std::cerr << "[NetworkCollectorThread] Invalid data structure for split publishing" << std::endl;
            }
            return;
        }
        
        // Iterate through each data item and publish to its respective topic
        for (const auto& dataItem : completeData["data"]) {
            if (!dataItem.contains("type") || !dataItem.contains("data")) {
                continue;
            }
            
            std::string dataType = dataItem["type"];
            json itemData = dataItem["data"];
            
            // Create individual JSON object for this data type
            json individualData;
            individualData["data"] = itemData;
            individualData["success"] = dataItem.value("success", true);
            individualData["timestamp"] = dataItem.value("timestamp", "");
            individualData["type"] = dataType;
            
            // Add special fields for split firewall data if applicable
            if (dataType == "firewall-top" || dataType == "firewall-bot") {
                individualData["total_rules"] = dataItem.value("total_rules", 0);
                individualData["part"] = dataItem.value("part", "");
            }
            
            // Determine the actual topic name for publishing
            std::string publishTopic = dataType;
            if (dataType == "firewall-top") {
                publishTopic = "firewall-top";
            } else if (dataType == "firewall-bot") {
                publishTopic = "firewall-bot";
            }
            
            // Publish the individual data item
            publishToMqtt(individualData.dump(), publishTopic);
        }
        
    } catch (const std::exception& e) {
        if (!config_.quietMode) {
            std::cerr << "[NetworkCollectorThread] Error parsing data for split publishing: " 
                     << e.what() << std::endl;
        }
    }
}

void NetworkCollectorThread::updateGlobalData(const std::string& data) {
    try {
        json completeData = json::parse(data);
        
        if (!completeData.contains("data") || !completeData["data"].is_array()) {
            return;
        }
        
        std::lock_guard<std::mutex> lock(g_dataMutex);
        
        // Clear previous data
        g_vlanData = json::array();
        g_natData = json::array();
        g_firewallData = json::array();
        g_firewallTopData = json::array();
        g_firewallBotData = json::array();
        g_routesData = json::array();
        g_bridgesData = json::array();
        
        // Update global variables with new data
        for (const auto& dataItem : completeData["data"]) {
            if (!dataItem.contains("type")) {
                continue;
            }
            
            std::string dataType = dataItem["type"];
            
            if (dataType == "vlan") {
                g_vlanData = dataItem;
            } else if (dataType == "nat") {
                g_natData = dataItem;
            } else if (dataType == "firewall") {
                g_firewallData = dataItem;
            } else if (dataType == "firewall-top") {
                g_firewallTopData = dataItem;
            } else if (dataType == "firewall-bot") {
                g_firewallBotData = dataItem;
            } else if (dataType == "routes") {
                g_routesData = dataItem;
            } else if (dataType == "bridges") {
                g_bridgesData = dataItem;
            }
        }
        
        // Mark data as updated
        g_dataUpdated.store(true);
        
    } catch (const std::exception& e) {
        if (!config_.quietMode) {
            std::cerr << "[NetworkCollectorThread] Error updating global data: " << e.what() << std::endl;
        }
    }
}

void NetworkCollectorThread::publishingThreadFunc() {
    if (!config_.quietMode) {
        std::cout << "[PublishingThread] Started publishing thread - publishing every " 
                  << config_.publishingInterval << " seconds" << std::endl;
    }
    
    int publishCounter = 0;
    int totalPublishes = 0;
    int successfulPublishes = 0;
    int failedPublishes = 0;
    auto lastLogTime = std::chrono::steady_clock::now();
    
    while (!g_publishingThreadShouldStop.load()) {
        try {
            // Check if data has been updated and it's time to publish
            if (g_dataUpdated.load() && (publishCounter % config_.publishingInterval == 0)) {
                std::lock_guard<std::mutex> lock(g_dataMutex);
                
                // Log publishing cycle start
                auto now = std::chrono::steady_clock::now();
                auto cycleTime = std::chrono::duration_cast<std::chrono::seconds>(now - lastLogTime);
                
                if (!config_.quietMode) {
                    std::cout << "[PublishingThread] Starting publish cycle #" << (totalPublishes + 1) 
                              << " (after " << cycleTime.count() << "s)" << std::endl;
                    std::cout << "[PublishingThread] Publishing sequence: vlan → nat → firewall → routes → bridges" << std::endl;
                    std::cout << "[PublishingThread] Inter-publish delay: 200ms between each data type" << std::endl;
                }
                
                // Reset sequence tracking for new cycle
                resetPublishSequence();
                
                // Publish each data type if MQTT publishing is enabled
                if (config_.enableMqttPublishing) {
                    if (!g_vlanData.empty()) {
                        publishToMqtt(g_vlanData.dump(), "vlan");
                        totalPublishes++;
                    }
                    
                    // Add 200ms delay between data types
                    std::this_thread::sleep_for(std::chrono::milliseconds(200));
                    
                    if (!g_natData.empty()) {
                        publishToMqtt(g_natData.dump(), "nat");
                        totalPublishes++;
                    }
                    
                    // Add 200ms delay between data types
                    std::this_thread::sleep_for(std::chrono::milliseconds(200));
                    
                    // Publish split firewall data if available, otherwise regular firewall data
                    if (!g_firewallTopData.empty()) {
                        publishToMqtt(g_firewallTopData.dump(), "firewall-top");
                        totalPublishes++;
                    }
                    
                    // Add small delay between split parts
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    
                    if (!g_firewallBotData.empty()) {
                        publishToMqtt(g_firewallBotData.dump(), "firewall-bot");
                        totalPublishes++;
                    } else if (!g_firewallData.empty()) {
                        // Fallback to regular firewall data if no split data
                        publishToMqtt(g_firewallData.dump(), "firewall");
                        totalPublishes++;
                    }
                    
                    // Add 200ms delay between data types
                    std::this_thread::sleep_for(std::chrono::milliseconds(200));
                    
                    if (!g_routesData.empty()) {
                        publishToMqtt(g_routesData.dump(), "routes");
                        totalPublishes++;
                    }
                    
                    // Add 200ms delay between data types
                    std::this_thread::sleep_for(std::chrono::milliseconds(200));
                    
                    if (!g_bridgesData.empty()) {
                        publishToMqtt(g_bridgesData.dump(), "bridges");
                        totalPublishes++;
                    }
                }
                
                // Reset the update flag after publishing
                g_dataUpdated.store(false);
                
                // Log publishing cycle summary
                if (!config_.quietMode) {
                    std::cout << "[PublishingThread] Publish cycle completed - " 
                              << "Total: " << totalPublishes 
                              << ", Running: " << successfulPublishes 
                              << ", Failed: " << failedPublishes << std::endl;
                }
                
                lastLogTime = now;
            }
            
        } catch (const std::exception& e) {
            failedPublishes++;
            if (!config_.quietMode) {
                std::cerr << "[PublishingThread] Error in publishing loop: " << e.what() << std::endl;
            }
        }
        
        // Increment counter and sleep for 1 second
        publishCounter++;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    // Final summary
    if (!config_.quietMode) {
        std::cout << "[PublishingThread] Publishing thread stopped" << std::endl;
        std::cout << "[PublishingThread] Final statistics - Total publishes: " << totalPublishes 
                  << ", Successful: " << successfulPublishes 
                  << ", Failed: " << failedPublishes << std::endl;
    }
}

void NetworkCollectorThread::stop() {
    if (!isRunning_.load()) {
        return;
    }
    
    shouldStop_.store(true);
    
    // Stop publishing thread
    if (publishingThreadRunning_.load()) {
        g_publishingThreadShouldStop.store(true);
        if (publishingThread_.joinable()) {
            publishingThread_.join();
        }
        publishingThreadRunning_.store(false);
    }
    
    // Wait for collection thread to finish
    while (isRunning_.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    if (!config_.quietMode) {
        std::cout << "[NetworkCollectorThread] Network collector thread stopped" << std::endl;
    }
}

bool NetworkCollectorThread::start(const CollectionConfig& config) {
    std::lock_guard<std::mutex> lock(configMutex_);
    
    if (isRunning_.load()) {
        std::cerr << "[NetworkCollectorThread] Thread is already running" << std::endl;
        return false;
    }
    
    config_ = config;
    shouldStop_.store(false);
    g_publishingThreadShouldStop.store(false);
    
    try {
        // Start publishing thread first
        if (config_.enableMqttPublishing) {
            publishingThread_ = std::thread([this]() {
                publishingThreadFunc();
            });
            publishingThreadRunning_.store(true);
            
            if (!config_.quietMode) {
                std::cout << "[NetworkCollectorThread] Started publishing thread" << std::endl;
            }
        }
        
        // Create collection thread using the ThreadManager
        threadId_ = threadManager_->createThread([this]() {
            networkCollectorThreadFunc();
        });
        
        isRunning_.store(true);
        
        if (!config_.quietMode) {
            std::cout << "[NetworkCollectorThread] Started network collection thread with ID: " << threadId_ << std::endl;
        }
        
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "[NetworkCollectorThread] Failed to start thread: " << e.what() << std::endl;
        
        // Clean up publishing thread if it was started
        if (publishingThreadRunning_.load()) {
            g_publishingThreadShouldStop.store(true);
            if (publishingThread_.joinable()) {
                publishingThread_.join();
            }
            publishingThreadRunning_.store(false);
        }
        
        return false;
    }
}

bool NetworkCollectorThread::isRunning() const {
    return isRunning_.load();
}

std::string NetworkCollectorThread::getLastCollectedData() const {
    std::lock_guard<std::mutex> lock(dataMutex_);
    return lastCollectedData_;
}

void NetworkCollectorThread::updateConfig(const CollectionConfig& config) {
    std::lock_guard<std::mutex> lock(configMutex_);
    config_ = config;
}

CollectionConfig NetworkCollectorThread::getConfig() const {
    std::lock_guard<std::mutex> lock(configMutex_);
    return config_;
}

ThreadMgr::ThreadInfo NetworkCollectorThread::getThreadInfo() const {
    if (threadManager_ && isRunning_.load()) {
        return threadManager_->getThreadInfo(threadId_);
    }
    return ThreadMgr::ThreadInfo{};
}

void NetworkCollectorThread::networkCollectorThreadFunc() {
    if (!config_.quietMode) {
        std::cout << "[NetworkCollectorThread] Thread started, beginning data collection" << std::endl;
    }
    
    do {
        try {
            // Collect network data
            std::string data = collectNetworkData();
            
            // Store the collected data
            {
                std::lock_guard<std::mutex> lock(dataMutex_);
                lastCollectedData_ = data;
            }
            
            // Update global data variables for publishing thread
            updateGlobalData(data);
            
            // Note: Publishing is now handled by the dedicated publishing thread
            // that runs every second and checks g_dataUpdated flag
            
            // Output data if configured
            {
                std::lock_guard<std::mutex> lock(configMutex_);
                if (!config_.outputFile.empty()) {
                    std::ofstream file(config_.outputFile);
                    if (file.is_open()) {
                        file << data;
                        file.close();
                        if (!config_.quietMode) {
                            std::cout << "[NetworkCollectorThread] Data saved to " << config_.outputFile << std::endl;
                        }
                    } else {
                        std::cerr << "[NetworkCollectorThread] Failed to open output file: " << config_.outputFile << std::endl;
                    }
                } else if (!config_.quietMode) {
                    std::cout << "[NetworkCollectorThread] Collected data:" << std::endl;
                    std::cout << data << std::endl;
                }
            }
            
        } catch (const std::exception& e) {
            std::cerr << "[NetworkCollectorThread] Error during data collection: " << e.what() << std::endl;
        }
        
        // Check if we should continue (continuous mode) or stop (single mode)
        if (!config_.continuousMode) {
            break; // Single collection mode - exit after one collection
        }
        
        // Sleep for the configured interval, checking for stop signal periodically
        int sleepTime = config_.collectionInterval;
        for (int i = 0; i < sleepTime && !shouldStop_.load(); ++i) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        
    } while (!shouldStop_.load() && config_.continuousMode);
    
    if (!config_.quietMode) {
        std::cout << "[NetworkCollectorThread] Thread exiting" << std::endl;
    }
}

void* NetworkCollectorThread::staticThreadFunc(void* instance) {
    if (instance) {
        static_cast<NetworkCollectorThread*>(instance)->networkCollectorThreadFunc();
    }
    return nullptr;
}

std::string NetworkCollectorThread::collectNetworkData() {
    CollectionConfig localConfig;
    {
        std::lock_guard<std::mutex> lock(configMutex_);
        localConfig = config_;
    }
    
    json allData = json::array();
    
    try {
        if (localConfig.collectAll || localConfig.collectVlan) {
            if (!localConfig.quietMode) std::cout << "[NetworkCollectorThread] Collecting VLAN information..." << std::endl;
            auto vlanCollector = std::make_unique<VlanCollector>();
            auto vlanData = vlanCollector->collectDataJson();
            allData.push_back(vlanData);
        }
        
        if (localConfig.collectAll || localConfig.collectNat) {
            if (!localConfig.quietMode) std::cout << "[NetworkCollectorThread] Collecting NAT rules..." << std::endl;
            auto natCollector = std::make_unique<NatCollector>();
            auto natData = natCollector->collectDataJson();
            allData.push_back(natData);
        }
        
        if (localConfig.collectAll || localConfig.collectFirewall) {
            if (!localConfig.quietMode) std::cout << "[NetworkCollectorThread] Collecting firewall rules..." << std::endl;
            auto firewallCollector = std::make_unique<FirewallCollector>();
            auto firewallData = firewallCollector->collectDataJson();
            
            // Check firewall data size before adding to collection
            const size_t MAX_FIREWALL_SIZE = 8192; // 8KB limit
            std::string firewallDataString = firewallData.dump();
            if (firewallDataString.length() > MAX_FIREWALL_SIZE) {
                std::cout << "[NetworkCollectorThread] Firewall data too large (" 
                         << firewallDataString.length() << " bytes), splitting into two parts..." << std::endl;
                
                // Parse the firewall data to split it
                try {
                    json firewallJson = json::parse(firewallDataString);
                    if (firewallJson.contains("data") && firewallJson["data"].is_array()) {
                        auto firewallRules = firewallJson["data"];
                        size_t totalRules = firewallRules.size();
                        size_t midPoint = totalRules / 2;
                        
                        // Create top half
                        json topFirewallData;
                        topFirewallData["timestamp"] = firewallJson["timestamp"];
                        topFirewallData["success"] = firewallJson["success"];
                        topFirewallData["type"] = "firewall-top";
                        topFirewallData["total_rules"] = totalRules;
                        topFirewallData["part"] = "top";
                        topFirewallData["data"] = json::array();
                        
                        // Create bottom half
                        json bottomFirewallData;
                        bottomFirewallData["timestamp"] = firewallJson["timestamp"];
                        bottomFirewallData["success"] = firewallJson["success"];
                        bottomFirewallData["type"] = "firewall-bot";
                        bottomFirewallData["total_rules"] = totalRules;
                        bottomFirewallData["part"] = "bottom";
                        bottomFirewallData["data"] = json::array();
                        
                        // Split the rules
                        for (size_t i = 0; i < totalRules; i++) {
                            if (i < midPoint) {
                                topFirewallData["data"].push_back(firewallRules[i]);
                            } else {
                                bottomFirewallData["data"].push_back(firewallRules[i]);
                            }
                        }
                        
                        // Add both parts to the collection with special type markers
                        allData.push_back(topFirewallData);
                        allData.push_back(bottomFirewallData);
                        
                        std::cout << "[NetworkCollectorThread] Split firewall data: " 
                                 << topFirewallData["data"].size() << " rules in top part, "
                                 << bottomFirewallData["data"].size() << " rules in bottom part" << std::endl;
                    } else {
                        // Fallback: create error if we can't parse/split
                        json skippedFirewallData;
                        skippedFirewallData["timestamp"] = getCurrentTimestamp();
                        skippedFirewallData["success"] = false;
                        skippedFirewallData["error"] = "Firewall data exceeds 8KB size limit and cannot be split";
                        skippedFirewallData["type"] = "firewall";
                        skippedFirewallData["data"] = json::array();
                        allData.push_back(skippedFirewallData);
                    }
                } catch (const std::exception& e) {
                    // Fallback: create error if parsing fails
                    json skippedFirewallData;
                    skippedFirewallData["timestamp"] = getCurrentTimestamp();
                    skippedFirewallData["success"] = false;
                    skippedFirewallData["error"] = "Firewall data exceeds 8KB size limit and parsing failed";
                    skippedFirewallData["type"] = "firewall";
                    skippedFirewallData["data"] = json::array();
                    allData.push_back(skippedFirewallData);
                }
            } else {
                allData.push_back(firewallData);
            }
        }
        
        if (localConfig.collectAll || localConfig.collectRoutes) {
            if (!localConfig.quietMode) std::cout << "[NetworkCollectorThread] Collecting static routes..." << std::endl;
            auto routeCollector = std::make_unique<RouteCollector>();
            auto routeData = routeCollector->collectDataJson();
            allData.push_back(routeData);
        }
        
        if (localConfig.collectAll || localConfig.collectBridges) {
            if (!localConfig.quietMode) std::cout << "[NetworkCollectorThread] Collecting bridge information..." << std::endl;
            auto bridgeCollector = std::make_unique<BridgeCollector>();
            auto bridgeData = bridgeCollector->collectDataJson();
            allData.push_back(bridgeData);
        }
        
        if (allData.empty()) {
            json errorResult;
            errorResult["timestamp"] = getCurrentTimestamp();
            errorResult["success"] = false;
            errorResult["error"] = "No data collected. Check permissions and network configuration.";
            return errorResult.dump(4);
        }
        
        json finalResult;
        finalResult["timestamp"] = getCurrentTimestamp();
        finalResult["success"] = true;
        finalResult["data"] = allData;
        
        // Check for permission warnings and add summary
        std::vector<std::string> warnings;
        for (const auto& item : allData) {
            if (item.contains("warning")) {
                warnings.push_back(item["warning"]);
            }
        }
        
        if (!warnings.empty()) {
            finalResult["warnings"] = warnings;
            if (!localConfig.quietMode) {
                std::cout << "[NetworkCollectorThread] Permission warnings detected:" << std::endl;
                for (const auto& warning : warnings) {
                    std::cout << "  - " << warning << std::endl;
                }
                std::cout << "[NetworkCollectorThread] Some data may be incomplete. Run with sudo for full access." << std::endl;
            }
        }
        
        if (localConfig.outputText) {
            // Convert to text format for backward compatibility
            std::ostringstream oss;
            for (const auto& item : allData) {
                if (!localConfig.quietMode) {
                    oss << std::string(80, '=') << std::endl;
                }
                if (item.contains("data") && item.contains("type")) {
                    oss << item["type"].get<std::string>() << " Configuration:" << std::endl;
                    oss << std::string(item["type"].get<std::string>().length() + 14, '=') << std::endl;
                    if (item["success"].get<bool>()) {
                        if (item["type"] == "vlan") {
                            for (const auto& vlan : item["data"]) {
                                oss << "VLAN " << vlan["vlanId"] << ": " << vlan["name"] 
                                    << " (" << vlan["interface"] << ") - " << vlan["status"] << std::endl;
                            }
                        } else if (item["type"] == "routes") {
                            for (const auto& route : item["data"]) {
                                oss << route["destination"] << " via " << route["gateway"] 
                                    << " dev " << route["interface"] << std::endl;
                            }
                        } else {
                            oss << "Data collected successfully. Use JSON format for detailed view." << std::endl;
                        }
                    } else {
                        oss << "Error: " << item["error"] << std::endl;
                    }
                    if (!localConfig.quietMode) {
                        oss << std::string(80, '=') << std::endl;
                    }
                }
            }
            return oss.str();
        } else {
            return finalResult.dump(4);
        }
        
    } catch (const std::exception& e) {
        json errorResult;
        errorResult["timestamp"] = getCurrentTimestamp();
        errorResult["success"] = false;
        errorResult["error"] = std::string("Collection error: ") + e.what();
        return errorResult.dump(4);
    }
}

std::string NetworkCollectorThread::getCurrentTimestamp() const {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    ss << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return ss.str();
}
