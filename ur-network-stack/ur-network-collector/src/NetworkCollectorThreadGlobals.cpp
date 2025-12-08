#include "NetworkCollectorThreadGlobals.h"
#include <nlohmann/json.hpp>
#include <mutex>
#include <atomic>

// Note: This file requires CMake build system to properly resolve include paths
// The linter may show errors for missing headers, but compilation succeeds

// Global data variables for publishing thread
json g_vlanData = json::array();
json g_natData = json::array();
json g_firewallData = json::array();
json g_firewallTopData = json::array();
json g_firewallBotData = json::array();
json g_routesData = json::array();
json g_bridgesData = json::array();
std::mutex g_dataMutex;
std::atomic<bool> g_dataUpdated{false};
std::atomic<bool> g_publishingThreadShouldStop{false};
bool g_resetPublishSequence = false;
