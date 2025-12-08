
#include "IperfWrapper.hpp"
#include <iostream>

extern thread_local IperfWrapper* g_currentWrapper;

void IperfWrapper::onJsonCallback(struct iperf_test* t, char* jsonData) {
    (void)t;

    if (!jsonData || !g_currentWrapper) {
        return;
    }

    std::string jsonStr(jsonData);

    // Detect if this is interval or summary data
    bool isInterval = g_currentWrapper->isIntervalData(jsonStr);

    // Display in terminal first for immediate feedback
    if (g_currentWrapper->realtimeJsonOutput_ && g_currentWrapper->onJsonOutput_) {
        if (isInterval) {
            g_currentWrapper->onJsonOutput_("[INTERVAL] " + jsonStr);
        } else {
            g_currentWrapper->onJsonOutput_("[SUMMARY] " + jsonStr);
        }
        std::cout.flush(); // Force terminal output immediately
    }

    // Update results and write to files
    g_currentWrapper->updateAndWriteResults(jsonStr, isInterval);
}
