#ifndef MAVLINK_COLLECTOR_THREAD_H
#define MAVLINK_COLLECTOR_THREAD_H

#include <string>
#include <cstdint>
#include <atomic>
#include <memory>
#include "PackageConfig.h"
#include "Vehicle.h"

// Global variables
extern std::atomic<bool> g_running;
extern Vehicle vehicle;
extern bool verbose_mode;

/**
 * @brief Structure to hold MAVLink collector thread arguments
 */
struct MavlinkCollectorArgs {
    PackageConfig config;
    std::atomic<bool>* running;
    
    MavlinkCollectorArgs(const PackageConfig& cfg, std::atomic<bool>* run_flag) 
        : config(cfg), running(run_flag) {}
};

/**
 * @brief Main MAVLink collector thread function
 * 
 * This function contains the main workflow that was previously in main()
 * after argument parsing. It can be controlled by the ur-threadder-api.
 * 
 * @param arg Pointer to MavlinkCollectorArgs structure
 * @return void* Always returns nullptr
 */
void* mavlinkCollectorThreadFunction(void* arg);

/**
 * @brief Initialize the MAVLink collector with package configuration
 * 
 * @param config Package configuration
 * @return true if initialization successful, false otherwise
 */
bool initializeMavlinkCollector(const PackageConfig& config);

/**
 * @brief Cleanup the MAVLink collector resources
 */
void cleanupMavlinkCollector();

/**
 * @brief Clear all device data flags
 * 
 * This function resets all device data flags to false when a device is removed.
 */
void clearDeviceData();

/**
 * @brief Publish "no device" messages to all MQTT topics
 * 
 * This function publishes JSON messages indicating no device is being monitored
 * to all relevant MQTT topics when a device is removed.
 */
void publishNoDeviceMessages();

/**
 * @brief Check if the MAVLink collector is properly initialized
 * 
 * @return true if initialized, false otherwise
 */
bool isMavlinkCollectorInitialized();

/**
 * @brief Start the MAVLink collector thread
 * 
 * @param config Package configuration
 * @param running Pointer to running flag
 * @return Thread ID if successful, 0 otherwise
 */
unsigned int startMavlinkCollector(const PackageConfig& config, std::atomic<bool>* running);

/**
 * @brief Stop the MAVLink collector thread
 * 
 * @param threadId Thread ID to stop
 */
void stopMavlinkCollector(unsigned int threadId);

/**
 * @brief Start the device data publisher thread
 * 
 * This thread publishes collected device data to MQTT every 1 second
 * using a cron job mechanism.
 * 
 * @return Thread ID if successful, 0 otherwise
 */
unsigned int startDeviceDataPublisher();

/**
 * @brief Stop the device data publisher thread
 */
void stopDeviceDataPublisher();

#endif // MAVLINK_COLLECTOR_THREAD_H
