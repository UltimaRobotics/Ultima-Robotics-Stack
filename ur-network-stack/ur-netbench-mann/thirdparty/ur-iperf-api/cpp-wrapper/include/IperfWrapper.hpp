#pragma once

#include <string>
#include <memory>
#include <functional>
#include <fstream>
#include <vector>
#include "json.hpp"

extern "C" {
    #include "iperf_api.h"
}

using json = nlohmann::json;

// Forward declaration
class MetricFormatter;

// Structure to hold interval data
struct IntervalData {
    std::string event;
    json data;
    json formatted_metrics;  // Human-readable metrics
    
    IntervalData(const std::string& evt, const json& d);
    IntervalData(const std::string& evt, const json& d, const json& metrics) 
        : event(evt), data(d), formatted_metrics(metrics) {}
    
private:
    void generateFormattedMetrics();
};

// Structure to hold complete test results
struct TestResults {
    time_t test_start_time;
    std::vector<IntervalData> intervals;
    time_t test_end_time;
    json summary;
    bool has_summary;
    
    TestResults() : test_start_time(0), test_end_time(0), has_summary(false) {}
    
    // Convert to JSON
    json to_json() const {
        json result;
        result["test_start_time"] = test_start_time;
        
        // Build intervals array with complete data and formatted metrics
        json intervals_array = json::array();
        for (const auto& interval : intervals) {
            json interval_obj;
            interval_obj["event"] = interval.event;
            interval_obj["data"] = interval.data;
            if (!interval.formatted_metrics.is_null()) {
                interval_obj["formatted"] = interval.formatted_metrics;
            }
            intervals_array.push_back(interval_obj);
        }
        result["intervals"] = intervals_array;
        
        // Build streams array by extracting stream data from all intervals
        json streams_array = json::array();
        for (const auto& interval : intervals) {
            if (interval.data.contains("streams") && interval.data["streams"].is_array()) {
                for (const auto& stream : interval.data["streams"]) {
                    streams_array.push_back(stream);
                }
            }
        }
        result["streams"] = streams_array;
        
        if (test_end_time > 0) {
            result["test_end_time"] = test_end_time;
        }
        
        if (has_summary) {
            result["summary"] = summary;
        }
        
        return result;
    }
};

class IperfWrapper {
public:
    enum class Role {
        CLIENT = 'c',
        SERVER = 's'
    };

    enum class Protocol {
        TCP = Ptcp,
        UDP = Pudp,
        SCTP = Psctp
    };

    IperfWrapper();
    ~IperfWrapper();

    void loadConfig(const json& config);
    void loadConfigFromFile(const std::string& configFile);
    
    void enableRealtimeJsonOutput(bool enable);
    void enableLogToFile(const std::string& logFile);
    void enableExportToFile(const std::string& exportFile);
    void enableStreamingMode(bool enable);
    
    void applyOutputOptions(const json& config);
    
    int run();
    
    std::string getJsonOutput() const;
    std::string getLastError() const;
    
    void setOnTestStart(std::function<void()> callback);
    void setOnTestFinish(std::function<void()> callback);
    void setOnJsonOutput(std::function<void(const std::string&)> callback);

private:
    struct iperf_test* test_;
    std::string lastError_;
    std::string logFileName_;
    std::ofstream logFile_;
    std::string exportFileName_;
    std::ofstream exportFile_;
    bool realtimeJsonOutput_;
    bool logToFile_;
    bool exportToFile_;
    bool streamingMode_;
    int intervalCount_;
    
    // Test results structures
    TestResults logResults_;
    TestResults exportResults_;
    
    std::function<void()> onTestStart_;
    std::function<void()> onTestFinish_;
    std::function<void(const std::string&)> onJsonOutput_;
    
    void applyRequiredFields(const json& config);
    void applyOptionalFields(const json& config);
    void validateConfig(const json& config);
    
    void openLogFile();
    void openExportFile();
    
    void initializeResults();
    void writeIntervalResult(const std::string& jsonData, bool isInterval);
    bool isIntervalData(const std::string& jsonData);
    void writeResultsToFile(std::ofstream& file, const TestResults& results);
    void updateAndWriteResults(const std::string& jsonData, bool isInterval);
    
    static void onTestStartCallback(struct iperf_test* t);
    static void onTestFinishCallback(struct iperf_test* t);
    static void onJsonCallback(struct iperf_test* t, char* jsonData);
};
