#ifndef NETWORK_COLLECTOR_THREAD_GLOBALS_H
#define NETWORK_COLLECTOR_THREAD_GLOBALS_H

#include <nlohmann/json.hpp>
#include <mutex>
#include <atomic>

using json = nlohmann::json;

// Global data variables for publishing thread
extern json g_vlanData;
extern json g_natData;
extern json g_firewallData;
extern json g_firewallTopData;
extern json g_firewallBotData;
extern json g_routesData;
extern json g_bridgesData;
extern std::mutex g_dataMutex;
extern std::atomic<bool> g_dataUpdated;
extern std::atomic<bool> g_publishingThreadShouldStop;
extern bool g_resetPublishSequence;

#endif // NETWORK_COLLECTOR_THREAD_GLOBALS_H
