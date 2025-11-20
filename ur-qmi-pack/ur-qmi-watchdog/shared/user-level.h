
#ifndef USER_LEVEL_H
#define USER_LEVEL_H

#include <string>
#include <json/json.h>

/**
 * Structure to hold parsed targeted request data
 */
struct TargetedRequestData {
    std::string type;
    std::string sender;
    std::string target;
    std::string method;
    std::string transaction_id;
    std::string response_topic;
    long long timestamp;
    int request_number;
    std::string data_payload;  // The extracted JSON data string
    std::string priority;
    bool is_valid;
    
    TargetedRequestData() : timestamp(0), request_number(0), is_valid(false) {}
};

/**
 * Structure to hold parsed QMI device data
 */
struct QMIDeviceData {
    std::string action;
    std::string device_path;
    std::string firmware_version;
    std::string imei;
    bool is_available;
    std::string manufacturer;
    std::string model;
    
    // SIM Status structure
    struct SIMStatus {
        std::string application_id;
        std::string application_state;
        std::string application_type;
        std::string card_state;
        std::string personalization_state;
        int pin1_retries;
        std::string pin1_state;
        int pin2_retries;
        std::string pin2_state;
        std::string primary_1x_status;
        std::string primary_gw_application;
        std::string primary_gw_slot;
        int puk1_retries;
        int puk2_retries;
        std::string secondary_1x_status;
        std::string secondary_gw_status;
        bool upin_replaces_pin1;
        int upin_retries;
        std::string upin_state;
        int upuk_retries;
        
        SIMStatus() : pin1_retries(0), pin2_retries(0), puk1_retries(0), 
                     puk2_retries(0), upin_replaces_pin1(false), 
                     upin_retries(0), upuk_retries(0) {}
    } sim_status;
    
    std::vector<std::string> supported_bands;
    bool is_valid;
    
    QMIDeviceData() : is_available(false), is_valid(false) {}
};

class TargetedRequestParser {
public:
    /**
     * Verify that a JSON string matches the expected targeted request format
     * @param json_string The JSON string to verify
     * @return true if format is valid, false otherwise
     */
    static bool verifyTargetedRequestFormat(const std::string& json_string);
    
    /**
     * Parse a targeted request JSON string and extract all components
     * @param json_string The JSON string to parse
     * @return TargetedRequestData structure with parsed data
     */
    static TargetedRequestData parseTargetedRequest(const std::string& json_string);
    
    /**
     * Extract and return just the data payload from a targeted request
     * @param json_string The JSON string containing the targeted request
     * @return The extracted data JSON string, empty if invalid
     */
    static std::string extractDataPayload(const std::string& json_string);
    
    /**
     * Parse the QMI device data from the extracted data payload
     * @param data_payload The JSON string containing QMI device data
     * @return QMIDeviceData structure with parsed device information
     */
    static QMIDeviceData parseQMIDeviceData(const std::string& data_payload);
    
    /**
     * Verify that a JSON string matches the expected QMI device data format
     * @param json_string The JSON string to verify
     * @return true if format is valid, false otherwise
     */
    static bool verifyQMIDeviceFormat(const std::string& json_string);
    
    /**
     * Get the last error message
     * @return Error message string
     */
    static std::string getLastError();

private:
    static std::string last_error_;
    
    /**
     * Set the last error message
     * @param error The error message to set
     */
    static void setError(const std::string& error);
    
    /**
     * Clear the last error message
     */
    static void clearError();
};


#endif // USER_LEVEL_H

