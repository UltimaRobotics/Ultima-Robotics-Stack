
#include "IperfWrapper.hpp"
#include <fstream>
#include <stdexcept>
#include <iostream>
#include <ctime>

thread_local IperfWrapper* g_currentWrapper = nullptr;

IperfWrapper::IperfWrapper() : 
    test_(nullptr), 
    realtimeJsonOutput_(false),
    logToFile_(false),
    exportToFile_(false),
    streamingMode_(false),
    intervalCount_(0),
    onTestStart_(nullptr), 
    onTestFinish_(nullptr),
    onJsonOutput_(nullptr) {
    test_ = iperf_new_test();
    if (!test_) {
        throw std::runtime_error("Failed to create iperf test structure");
    }
    iperf_defaults(test_);
}

IperfWrapper::~IperfWrapper() {
    if (logFile_.is_open()) {
        logFile_.close();
    }
    if (exportFile_.is_open()) {
        exportFile_.close();
    }
    if (test_) {
        iperf_free_test(test_);
        test_ = nullptr;
    }
}

void IperfWrapper::setOnTestStart(std::function<void()> callback) {
    onTestStart_ = callback;
}

void IperfWrapper::setOnTestFinish(std::function<void()> callback) {
    onTestFinish_ = callback;
}

void IperfWrapper::setOnJsonOutput(std::function<void(const std::string&)> callback) {
    onJsonOutput_ = callback;
}

void IperfWrapper::enableRealtimeJsonOutput(bool enable) {
    realtimeJsonOutput_ = enable;
    if (enable && test_) {
        iperf_set_test_json_callback(test_, onJsonCallback);
    }
}

void IperfWrapper::enableLogToFile(const std::string& logFile) {
    logToFile_ = true;
    logFileName_ = logFile;
    if (test_) {
        iperf_set_test_json_callback(test_, onJsonCallback);
    }
}

void IperfWrapper::enableExportToFile(const std::string& exportFile) {
    exportToFile_ = true;
    exportFileName_ = exportFile;
    if (test_) {
        iperf_set_test_json_callback(test_, onJsonCallback);
    }
}

void IperfWrapper::enableStreamingMode(bool enable) {
    streamingMode_ = enable;
}

std::string IperfWrapper::getJsonOutput() const {
    if (!test_) {
        return "{}";
    }

    char* json_output = iperf_get_test_json_output_string(test_);
    if (json_output) {
        return std::string(json_output);
    }

    return "{}";
}

std::string IperfWrapper::getLastError() const {
    return lastError_;
}
