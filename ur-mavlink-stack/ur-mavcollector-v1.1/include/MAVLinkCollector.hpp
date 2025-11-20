#pragma once

#include <string>
#include <memory>
#include <atomic>
#include <thread>
#include <chrono>
#include <fstream>
#include <netinet/in.h>
#include <fcntl.h>
#include <cerrno>
#include "../nholmann/json.hpp"
#include <mutex>
#include <map>
#include <deque>
#include <functional>
#include <set>

// MAVLink headers - include both v1 and v2
#include "../mavlink_c_library_v2/common/mavlink.h"
#include "../mavlink_c_library_v1/common/mavlink.h"
#include "UdpHandler.hpp"

// Define separate channels for v1 and v2
#define MAVLINK_COMM_V1 0
#define MAVLINK_COMM_V2 1

class MAVLinkCollector {
public:
    struct CollectorConfig {
        std::string udp_address;
        int udp_port;
        uint8_t request_sysid;
        uint8_t request_compid;
        std::string log_file;
        std::string diagnose_data_logfile;
        int request_interval_ms;
        int log_interval_ms;
        int diagnose_log_interval_ms;
        int socket_timeout_ms;
        int collection_loop_sleep_ms;
        bool verbose;
        bool mav_stats_counter;
        bool enableHTTP;
        std::set<uint8_t> filtered_system_ids;
        struct {
            std::string serverAddress;
            int serverPort;
            int timeoutMs;
            int fetch_status_interval_ms;
        } httpConfig;
    };

    struct VehicleData {
        std::string model;
        uint8_t system_id;
        uint8_t component_id;
        std::string flight_mode;
        bool armed;
        float battery_voltage;
        std::chrono::system_clock::time_point last_heartbeat;
        std::string firmware;
        std::chrono::system_clock::time_point last_activity;
        uint32_t messages_received;
        std::chrono::system_clock::time_point start_time;

        // Component information from COMPONENT_INFORMATION_BASIC
        std::string vendor_name;
        std::string component_model_name;
        std::string software_version;
        std::string hardware_version;
        std::string serial_number;
    };

    struct BatteryInfo {
        uint8_t id;
        uint8_t battery_function;
        uint8_t type;
        uint8_t state_of_health;
        uint8_t cells_in_series;
        uint16_t cycle_count;
        uint16_t weight;
        float discharge_minimum_voltage;
        float charging_minimum_voltage;
        float resting_minimum_voltage;
        float charging_maximum_voltage;
        float charging_maximum_current;
        float nominal_voltage;
        float discharge_maximum_current;
        float discharge_maximum_burst_current;
        float design_capacity;
        float full_charge_capacity;
        std::string manufacture_date;
        std::string serial_number;
        std::string name;  // v2 only: device_name field
    };

    struct BatteryStatus {
        uint8_t id;
        uint8_t battery_function;
        uint8_t type;
        int16_t temperature;
        std::vector<uint16_t> voltages;        // MAVLink v1: cells 1-10
        std::vector<uint16_t> voltages_ext;    // MAVLink v2 only: cells 11-14
        int16_t current_battery;
        int32_t current_consumed;
        int32_t energy_consumed;
        int8_t battery_remaining;
        uint8_t charge_state;                  // MAVLink v2 only
        uint8_t mode;                          // MAVLink v2 only
        uint32_t fault_bitmask;                // MAVLink v2 only
        int32_t time_remaining;                // MAVLink v2 only: remaining battery time in seconds
    };

    struct PowerStatus {
        uint16_t Vcc;           // 5V rail voltage in mV
        uint16_t Vservo;        // Servo rail voltage in mV
        uint16_t flags;         // Bitmap of power supply status flags
    };

    struct DiagnosticData {
        // Airframe
        std::string airframe_type;
        std::string vehicle;
        std::string firmware_version;
        std::string custom_fw_ver;

        // Sensors
        std::string compass_0;
        std::string compass_1;
        std::string gyro;
        std::string accelerometer;

        // Radio
        int roll_channel;
        int pitch_channel;
        int yaw_channel;
        int throttle_channel;
        std::string aux1;
        std::string aux2;

        // Flight modes
        std::string mode_switch;
        std::string flight_mode_1;
        std::string flight_mode_2;
        std::string flight_mode_3;
        std::string flight_mode_4;
        std::string flight_mode_5;
        std::string flight_mode_6;

        // Power - Battery data
        std::map<uint8_t, BatteryInfo> battery_info_map;
        std::map<uint8_t, BatteryStatus> battery_status_map;
        PowerStatus power_status;
        float battery_full_voltage;
        float battery_empty_voltage;
        int number_of_cells;

        // Safety
        std::string low_battery_failsafe;
        std::string rc_loss_failsafe;
        float rc_loss_timeout;
        std::string data_link_loss_failsafe;
        float rtl_climb_to;
        std::string rtl_then;
    };

    struct MessageRateInfo {
        uint32_t count;
        std::deque<std::chrono::steady_clock::time_point> timestamps;
        double current_rate_hz;
        double expected_rate_hz;
        std::chrono::steady_clock::time_point last_update;
    };

    using MessageCallback = std::function<void(const mavlink_message_t&)>;

    explicit MAVLinkCollector(const std::string& config_file);
    ~MAVLinkCollector();

    bool start();
    void stop();
    bool isRunning() const { return running_; }
    void printMessageStats();

    // HTTP control
    void waitForHttpStart();
    void registerWithHttpServer();

    // Callback registration
    void registerMessageCallback(uint32_t msg_id, MessageCallback callback);
    void unregisterMessageCallback(uint32_t msg_id);

    // Rate tracking
    void setExpectedRate(uint32_t msg_id, double rate_hz);
    MessageRateInfo getMessageRateInfo(uint32_t msg_id);
    std::map<uint32_t, MessageRateInfo> getAllMessageRates();

private:
    void collectionLoop();
    void requestData();
    void requestHighFrequencyStreamsV1();
    void requestHighFrequencyStreamsV2();
    void requestStreamRateV1(uint8_t stream_id, uint16_t rate_hz);
    void requestMessageIntervalV2(uint32_t message_id, int32_t interval_us);
    void logData();
    void loadExistingLog();
    void saveLogToFile();

    void requestHeartbeat();
    void requestDataStream();
    void requestSpecificMessages();
    void requestAllParameters();

    void handleHeartbeat(const mavlink_message_t& msg);
    void handleAutopilotVersion(const mavlink_message_t& msg);
    void handleSysStatus(const mavlink_message_t& msg);
    void handleBatteryStatus(const mavlink_message_t& msg);
    void handleBatteryInfo(const mavlink_message_t& msg);
    void handleComponentInformationBasic(const mavlink_message_t& msg);
    void handlePowerStatus(const mavlink_message_t& msg);
    
    // Component-based battery requests
    void requestBatteryComponentInfo(uint8_t component_id);
    void requestBatteryComponents();

    // UDP packet handling callback
    void onCollectorPacket(const UdpHandler::UdpPacket& packet);

    std::string formatDuration(const std::chrono::system_clock::time_point& time_point);
    double calculateMessagesPerSecond();

    // Callback and rate tracking
    void invokeCallbacks(const mavlink_message_t& msg);
    void updateMessageRate(uint32_t msg_id);
    void calculateMessageRates();

    // UDP message sending
    bool sendMavlinkMessage(const mavlink_message_t& msg);

    CollectorConfig config_;
    VehicleData vehicle_data_;

    std::unique_ptr<UdpHandler> udp_handler_;

    std::atomic<bool> running_;
    std::unique_ptr<std::thread> collection_thread_;

    // Callback mechanism
    std::map<uint32_t, MessageCallback> message_callbacks_;
    std::mutex callbacks_mutex_;

    // Rate tracking
    std::map<uint32_t, MessageRateInfo> message_rates_;
    std::mutex rates_mutex_;
    std::chrono::steady_clock::time_point last_rate_calculation_;

    // Stats tracking
    std::map<uint32_t, uint32_t> message_stats_;
    std::mutex stats_mutex_;
    uint32_t total_messages_received_;

    nlohmann::json log_json_;
    std::chrono::system_clock::time_point last_log_time_;

    // Diagnostic data
    DiagnosticData diagnostic_data_;
    nlohmann::json diagnose_log_json_;
    std::chrono::system_clock::time_point last_diagnose_log_time_;
    std::mutex diagnose_mutex_;

    void requestDiagnosticData();
    void handleParamValue(const mavlink_message_t& msg);
    void logDiagnosticData();
    void loadExistingDiagnoseLog();
    void saveDiagnoseLogToFile();
    std::string getSensorStatus(uint32_t sensor_present, uint32_t sensor_enabled, uint32_t sensor_health, uint32_t bit);

    // Parameter logging
    struct ParameterInfo {
        std::string name;
        float value;
        uint8_t type;
        std::chrono::system_clock::time_point timestamp;
    };
    
    std::map<std::string, ParameterInfo> collected_parameters_;
    std::mutex params_mutex_;
    std::string params_log_file_;
    
    void saveParametersToFile();
};
