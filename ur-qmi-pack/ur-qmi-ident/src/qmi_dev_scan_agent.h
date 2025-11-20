
#ifndef QMI_DEV_SCAN_AGENT_H
#define QMI_DEV_SCAN_AGENT_H

#include <string>
#include <vector>
#include <memory>
#include <json/json.h>
#include "qmi_scanner.h"

class QMIDevScanAgent {
public:
    QMIDevScanAgent();
    ~QMIDevScanAgent();
    
    // JSON formatting and validation
    std::string formatJsonPretty(const Json::Value& json);
    std::string formatJsonCompact(const Json::Value& json);
    bool validateJsonString(const std::string& json_str);
    Json::Value parseJsonString(const std::string& json_str);
    
    // Device profile JSON operations
    Json::Value deviceProfileToJson(const DeviceProfile& profile);
    Json::Value advancedDeviceProfileToJson(const AdvancedDeviceProfile& profile);
    Json::Value qmiDeviceToJson(const QMIDevice& device);
    Json::Value simStatusToJson(const SIMStatus& sim_status);
    
    // JSON to struct extraction
    bool jsonToDeviceProfile(const Json::Value& json, DeviceProfile& profile);
    bool jsonToAdvancedDeviceProfile(const Json::Value& json, AdvancedDeviceProfile& profile);
    bool jsonToQMIDevice(const Json::Value& json, QMIDevice& device);
    bool jsonToSIMStatus(const Json::Value& json, SIMStatus& sim_status);
    
    // Batch operations
    Json::Value deviceProfilesArrayToJson(const std::vector<DeviceProfile>& profiles);
    Json::Value advancedDeviceProfilesArrayToJson(const std::vector<AdvancedDeviceProfile>& profiles);
    Json::Value qmiDevicesArrayToJson(const std::vector<QMIDevice>& devices);
    
    std::vector<DeviceProfile> jsonToDeviceProfilesArray(const Json::Value& json_array);
    std::vector<AdvancedDeviceProfile> jsonToAdvancedDeviceProfilesArray(const Json::Value& json_array);
    std::vector<QMIDevice> jsonToQMIDevicesArray(const Json::Value& json_array);
    
    // Event JSON generation
    Json::Value createDeviceEvent(const std::string& event_type, const DeviceProfile& profile);
    Json::Value createAdvancedDeviceEvent(const std::string& event_type, const AdvancedDeviceProfile& profile);
    Json::Value createQMIDeviceEvent(const std::string& event_type, const QMIDevice& device);
    
    // Configuration and metadata
    Json::Value createScanConfiguration(ProfileMode mode, const std::vector<std::string>& options);
    Json::Value createScanReport(const std::string& scan_id, int64_t timestamp, 
                                const std::vector<DeviceProfile>& basic_profiles,
                                const std::vector<AdvancedDeviceProfile>& advanced_profiles);
    
    // Utility functions
    std::string generateScanId();
    int64_t getCurrentTimestamp();
    bool saveJsonToFile(const Json::Value& json, const std::string& filename);
    Json::Value loadJsonFromFile(const std::string& filename);
    
    // Error handling
    struct JsonError {
        bool has_error;
        std::string error_message;
        int error_line;
        int error_column;
    };
    
    JsonError getLastError() const;
    void clearLastError();

private:
    JsonError m_last_error;
    std::unique_ptr<Json::StreamWriterBuilder> m_pretty_writer_builder;
    std::unique_ptr<Json::StreamWriterBuilder> m_compact_writer_builder;
    std::unique_ptr<Json::CharReaderBuilder> m_reader_builder;
    
    void setError(const std::string& message, int line = -1, int column = -1);
    Json::Value stringVectorToJsonArray(const std::vector<std::string>& vec);
    std::vector<std::string> jsonArrayToStringVector(const Json::Value& json_array);
};

#endif // QMI_DEV_SCAN_AGENT_H
