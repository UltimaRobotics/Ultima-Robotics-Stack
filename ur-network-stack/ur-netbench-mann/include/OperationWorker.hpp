
#ifndef OPERATION_WORKER_HPP
#define OPERATION_WORKER_HPP

#include "json.hpp"
#include <string>

using json = nlohmann::json;

namespace ThreadMgr {
    class ThreadManager;
}

/**
 * @brief Worker function that executes network benchmark operations
 * @param tm Reference to ThreadManager
 * @param package_config_file Path to the package configuration file
 */
void operation_worker(ThreadMgr::ThreadManager& tm, const std::string& package_config_file);

#endif // OPERATION_WORKER_HPP
