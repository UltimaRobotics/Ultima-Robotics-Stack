
#include "IperfWrapper.hpp"
#include <fstream>
#include <stdexcept>
#include <iostream>
#include <ctime>

extern thread_local IperfWrapper* g_currentWrapper;

void IperfWrapper::openLogFile() {
    if (!logToFile_ || logFileName_.empty()) {
        return;
    }

    logFile_.open(logFileName_, std::ios::out | std::ios::trunc);
    if (!logFile_.is_open()) {
        throw std::runtime_error("Failed to open log file: " + logFileName_);
    }
}

void IperfWrapper::openExportFile() {
    if (!exportToFile_ || exportFileName_.empty()) {
        return;
    }

    exportFile_.open(exportFileName_, std::ios::out | std::ios::trunc);
    if (!exportFile_.is_open()) {
        throw std::runtime_error("Failed to open export file: " + exportFileName_);
    }
}

void IperfWrapper::initializeResults() {
    intervalCount_ = 0;
    time_t startTime = std::time(nullptr);
    
    // Initialize log results
    logResults_ = TestResults();
    logResults_.test_start_time = startTime;
    
    // Initialize export results
    exportResults_ = TestResults();
    exportResults_.test_start_time = startTime;
}

bool IperfWrapper::isIntervalData(const std::string& jsonData) {
    // Interval data: has "start", "end", and "sum" but NOT "sum_sent"/"sum_received"
    // Summary data: has "sum_sent" and "sum_received" (final test results)
    bool hasInterval = (jsonData.find("\"start\"") != std::string::npos &&
                        jsonData.find("\"end\"") != std::string::npos);
    bool hasSummary = (jsonData.find("\"sum_sent\"") != std::string::npos ||
                       jsonData.find("\"sum_received\"") != std::string::npos);
    
    // It's interval data if it has start/end timing but NOT final summary fields
    return hasInterval && !hasSummary;
}

void IperfWrapper::writeResultsToFile(std::ofstream& file, const TestResults& results) {
    if (!file.is_open()) {
        return;
    }
    
    // Clear file and seek to beginning
    file.seekp(0);
    file.clear();
    
    // Write formatted JSON with proper indentation
    json resultJson = results.to_json();
    file << resultJson.dump(2) << std::endl;
    
    // Flush to ensure data is written
    file.flush();
}

void IperfWrapper::updateAndWriteResults(const std::string& jsonData, bool isInterval) {
    try {
        json parsedData = json::parse(jsonData);
        
        if (isInterval) {
            intervalCount_++;

            // Update log results - store complete interval data with formatted metrics
            if (logToFile_) {
                logResults_.intervals.emplace_back("interval", parsedData);
                if (logFile_.is_open()) {
                    writeResultsToFile(logFile_, logResults_);
                }
            }

            // Update export results - store complete interval data with formatted metrics
            if (exportToFile_) {
                exportResults_.intervals.emplace_back("interval", parsedData);
                if (exportFile_.is_open()) {
                    writeResultsToFile(exportFile_, exportResults_);
                }
            }
        } else {
            // Handle summary/final results
            if (logToFile_) {
                logResults_.summary = parsedData;
                logResults_.has_summary = true;
                if (logFile_.is_open()) {
                    writeResultsToFile(logFile_, logResults_);
                }
            }

            if (exportToFile_) {
                exportResults_.summary = parsedData;
                exportResults_.has_summary = true;
                if (exportFile_.is_open()) {
                    writeResultsToFile(exportFile_, exportResults_);
                }
            }
        }
    } catch (const json::exception& e) {
        // If JSON parsing fails, log the error
        std::cerr << "JSON parse error: " << e.what() << std::endl;
    }
}

void IperfWrapper::writeIntervalResult(const std::string& jsonData, bool isInterval) {
    updateAndWriteResults(jsonData, isInterval);
}
