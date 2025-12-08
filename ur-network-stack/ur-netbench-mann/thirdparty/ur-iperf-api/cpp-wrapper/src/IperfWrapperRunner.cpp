
#include "IperfWrapper.hpp"
#include <stdexcept>
#include <ctime>

extern thread_local IperfWrapper* g_currentWrapper;

int IperfWrapper::run() {
    if (!test_) {
        lastError_ = "Test structure is not initialized";
        return -1;
    }

    g_currentWrapper = this;

    if (onTestStart_) {
        iperf_set_on_test_start_callback(test_, onTestStartCallback);
    }

    if (onTestFinish_) {
        iperf_set_on_test_finish_callback(test_, onTestFinishCallback);
    }

    // Initialize results structures
    initializeResults();
    
    if (logToFile_) {
        openLogFile();
    }

    if (exportToFile_) {
        openExportFile();
    }

    int result;
    char role = iperf_get_test_role(test_);

    if (role == 'c') {
        result = iperf_run_client(test_);
    } else if (role == 's') {
        result = iperf_run_server(test_);
    } else {
        lastError_ = "Invalid test role";
        return -1;
    }

    // Finalize with end time
    time_t endTime = std::time(nullptr);
    
    if (exportToFile_) {
        exportResults_.test_end_time = endTime;
        if (exportFile_.is_open()) {
            writeResultsToFile(exportFile_, exportResults_);
            exportFile_.close();
        }
    }
    
    if (logToFile_) {
        logResults_.test_end_time = endTime;
        if (logFile_.is_open()) {
            writeResultsToFile(logFile_, logResults_);
            logFile_.close();
        }
    }

    if (result < 0) {
        char* error = iperf_strerror(i_errno);
        if (error) {
            lastError_ = std::string(error);
        } else {
            lastError_ = "Unknown error occurred";
        }
    }

    g_currentWrapper = nullptr;

    return result;
}

void IperfWrapper::onTestStartCallback(struct iperf_test* t) {
    (void)t;
}

void IperfWrapper::onTestFinishCallback(struct iperf_test* t) {
    (void)t;
}
