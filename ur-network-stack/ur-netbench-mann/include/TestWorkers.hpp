#ifndef TEST_WORKERS_HPP
#define TEST_WORKERS_HPP

#include "../thirdparty/nlohmann/json.hpp"
#include "../thirdparty/ur-threadder-api/cpp/include/ThreadManager.hpp"
#include <string>

using json = nlohmann::json;

void dns_test_worker(ThreadMgr::ThreadManager& thread_manager, const json& config_json, const std::string& output_file);
void traceroute_test_worker(ThreadMgr::ThreadManager& thread_manager, const json& config_json, const std::string& output_file);
void ping_test_worker(ThreadMgr::ThreadManager& thread_manager, const json& config_json, const std::string& output_file);
void iperf_test_worker(ThreadMgr::ThreadManager& thread_manager, const json& config_json, const std::string& output_file);

#endif // TEST_WORKERS_HPP