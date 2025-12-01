#ifndef WATCHDOG_THREAD_ARGS_H
#define WATCHDOG_THREAD_ARGS_H

#include <string>

/**
 * @brief Arguments structure for the watchdog thread function
 */
struct WatchdogThreadArgs {
    std::string* config_path;  /**< Pointer to package configuration path */
    
    /**
     * @brief Constructor
     * @param config Pointer to configuration path string
     */
    explicit WatchdogThreadArgs(std::string* config) : config_path(config) {}
};

#endif // WATCHDOG_THREAD_ARGS_H
