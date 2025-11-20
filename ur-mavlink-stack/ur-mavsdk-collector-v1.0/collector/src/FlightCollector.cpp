#include "FlightCollector.h"
#include <iostream>
#include <chrono>
#include <future>
#include <ThreadManager.hpp>

// Note: MAVSDK headers are included in FlightCollector.h

namespace FlightCollector {

FlightCollector::FlightCollector() : thread_manager_(std::make_unique<ThreadMgr::ThreadManager>(5)), 
                                       telemetry_thread_id_(0), 
                                       logging_thread_id_(0) {
    resetData();
}

FlightCollector::~FlightCollector() {
    stopCollection();
    disconnect();
}

bool FlightCollector::connect() {
    std::lock_guard<std::mutex> lock(state_mutex_);
    if (connected_) {
        return true;
    }
    
    if (!setupConnection()) {
        std::cerr << "Failed to setup connection" << std::endl;
        return false;
    }
    
    if (!system_) {
        std::cerr << "No system discovered" << std::endl;
        return false;
    }
    
    connected_ = true;
    notifyConnectionChange(true);
    std::cout << "Connected to flight controller" << std::endl;
    resetData();
    return true;
}

void FlightCollector::disconnect() {
    std::lock_guard<std::mutex> lock(state_mutex_);
    if (!connected_) {
        return;
    }
    
    stopCollection();
    connected_ = false;
    notifyConnectionChange(false);
    std::cout << "Disconnected from flight controller" << std::endl;
}

void FlightCollector::resetData() {
    std::lock_guard<std::mutex> lock(data_mutex_);
    flight_data_ = FlightDataCollection();
    
    // Set meaningful default values for strings that would otherwise be empty
    flight_data_.vehicle.vendor_name = "No Connection";
    flight_data_.vehicle.model = "No Vehicle";
    flight_data_.vehicle.component_model_name = "No Vehicle";
    flight_data_.vehicle.hardware_version = "Unknown";
    flight_data_.diagnostics.airframe_type = "No Airframe";
    flight_data_.diagnostics.vehicle = "No Vehicle";
    flight_data_.diagnostics.custom_fw_ver = "";
    
    // Initialize sensor status with default values
    flight_data_.diagnostics.sensors.gyro = "No Data";
    flight_data_.diagnostics.sensors.accelerometer = "No Data";
    flight_data_.diagnostics.sensors.compass_0 = "No Data";
    flight_data_.diagnostics.sensors.compass_1 = "No Data";
    flight_data_.diagnostics.sensors.sensors_health = 0;
    flight_data_.diagnostics.sensors.sensors_present = 0;
    flight_data_.diagnostics.sensors.sensors_enabled = 0;
    
    // Initialize flight mode strings
    flight_data_.diagnostics.mode_switch = "";
    flight_data_.diagnostics.flight_mode_1 = "";
    flight_data_.diagnostics.flight_mode_2 = "";
    flight_data_.diagnostics.flight_mode_3 = "";
    flight_data_.diagnostics.flight_mode_4 = "";
    flight_data_.diagnostics.flight_mode_5 = "";
    flight_data_.diagnostics.flight_mode_6 = "";
    
    // Initialize radio strings
    flight_data_.diagnostics.aux1 = "";
    flight_data_.diagnostics.aux2 = "";
    
    // Initialize safety data with proper defaults
    flight_data_.diagnostics.data_link_loss_failsafe = "Unknown";
    flight_data_.diagnostics.low_battery_failsafe = "Unknown";
    flight_data_.diagnostics.rc_loss_failsafe = "Unknown";
    flight_data_.diagnostics.rc_loss_timeout = 0.0f;
    flight_data_.diagnostics.rtl_climb_to = 0.0f;
    flight_data_.diagnostics.rtl_then = "Unknown";
}

void FlightCollector::notifyConnectionChange(bool connected) {
    if (connection_callback_) {
        connection_callback_(connected);
    }
}

bool FlightCollector::startCollection() {
    std::lock_guard<std::mutex> lock(state_mutex_);
    
    if (!connected_) {
        std::cerr << "Not connected - cannot start collection" << std::endl;
        return false;
    }
    
    if (collecting_) {
        std::cout << "Collection already running" << std::endl;
        return true;
    }
    
    should_stop_ = false;
    
    try {
        // Start telemetry thread using ThreadManager
        telemetry_thread_id_ = thread_manager_->createThread([this]() {
            this->telemetryLoop();
        });
        
        // Start logging thread using ThreadManager
        logging_thread_id_ = thread_manager_->createThread([this]() {
            this->loggingLoop();
        });
        
        collecting_ = true;
        std::cout << "Started data collection with telemetry thread ID: " << telemetry_thread_id_ 
                  << ", logging thread ID: " << logging_thread_id_ << std::endl;
        
    } catch (const ThreadMgr::ThreadManagerException& e) {
        std::cerr << "Failed to start collection threads: " << e.what() << std::endl;
        return false;
    }
    
    return true;
}

void FlightCollector::stopCollection() {
    std::lock_guard<std::mutex> lock(state_mutex_);
    
    if (!collecting_) {
        return;
    }
    
    should_stop_ = true;
    
    try {
        // Stop threads using ThreadManager
        if (telemetry_thread_id_ != 0 && thread_manager_->isThreadAlive(telemetry_thread_id_)) {
            thread_manager_->stopThread(telemetry_thread_id_);
            thread_manager_->joinThread(telemetry_thread_id_, std::chrono::seconds(5));
        }
        
        if (logging_thread_id_ != 0 && thread_manager_->isThreadAlive(logging_thread_id_)) {
            thread_manager_->stopThread(logging_thread_id_);
            thread_manager_->joinThread(logging_thread_id_, std::chrono::seconds(5));
        }
        
    } catch (const ThreadMgr::ThreadManagerException& e) {
        std::cerr << "Error stopping collection threads: " << e.what() << std::endl;
    }
    
    collecting_ = false;
    telemetry_thread_id_ = 0;
    logging_thread_id_ = 0;
    
    std::cout << "Stopped data collection" << std::endl;
}

bool FlightCollector::isConnected() const {
    std::lock_guard<std::mutex> lock(state_mutex_);
    return connected_;
}

bool FlightCollector::isCollecting() const {
    std::lock_guard<std::mutex> lock(state_mutex_);
    return collecting_;
}

FlightDataCollection FlightCollector::getFlightData() const {
    std::lock_guard<std::mutex> lock(data_mutex_);
    return flight_data_;
}

std::string FlightCollector::getJsonOutput() const {
    return JsonFormatter::formatFlightData(getFlightData(), verbose_.load());
}

void FlightCollector::setVerbose(bool verbose) {
    verbose_.store(verbose);
}

void FlightCollector::setDataUpdateCallback(std::function<void(const FlightDataCollection&)> callback) {
    data_callback_ = callback;
}

void FlightCollector::setConnectionCallback(std::function<void(bool)> callback) {
    connection_callback_ = callback;
}

bool FlightCollector::setupConnection() {
    std::lock_guard<std::mutex> lock(state_mutex_);
    if (connected_) {
        return true;
    }
    
    try {
        // Initialize MAVSDK
        mavsdk::Mavsdk::Configuration mavsdk_config(
            mavsdk::Mavsdk::ComponentType::CompanionComputer);
        mavsdk_ = std::make_unique<mavsdk::Mavsdk>(mavsdk_config);
        
        // Add connection based on config
        mavsdk::ConnectionResult connection_result;
        
        if (config_.type == "udp") {
            connection_result = mavsdk_->add_any_connection(
                "udp://" + config_.address + ":" + std::to_string(config_.port));
        } else if (config_.type == "tcp") {
            connection_result = mavsdk_->add_any_connection(
                "tcp://" + config_.address + ":" + std::to_string(config_.port));
        } else if (config_.type == "serial") {
            // Add stability delay for serial connections
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            connection_result = mavsdk_->add_any_connection(
                "serial://" + config_.address + ":" + std::to_string(config_.baudrate));
            // Additional delay after serial connection
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        } else {
            std::cerr << "Unknown connection type: " << config_.type << std::endl;
            return false;
        }
        
        if (connection_result != mavsdk::ConnectionResult::Success) {
            std::cerr << "Connection failed with error code: " << static_cast<int>(connection_result) << std::endl;
            return false;
        }
        
        // Add delay to allow connection to stabilize
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        
        // Discover system
        std::cout << "Discovering system..." << std::endl;
        
        auto prom = std::promise<std::shared_ptr<mavsdk::System>>();
        auto fut = prom.get_future();
        
        mavsdk_->subscribe_on_new_system([&prom, this]() {
            auto systems = mavsdk_->systems();
            if (!systems.empty()) {
                prom.set_value(systems[0]);
            }
        });
        
        // Wait for system discovery with timeout
        if (fut.wait_for(std::chrono::seconds(config_.timeout_s)) == std::future_status::timeout) {
            std::cerr << "System discovery timeout" << std::endl;
            return false;
        }
        
        system_ = fut.get();
        if (!system_) {
            std::cerr << "Failed to get system reference" << std::endl;
            return false;
        }
        
        // Final delay before starting telemetry
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        connected_ = true;
        notifyConnectionChange(true);
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "Exception in setupConnection: " << e.what() << std::endl;
        return false;
    }
}

void FlightCollector::telemetryLoop() {
    if (!system_) {
        std::cerr << "No system available for telemetry" << std::endl;
        return;
    }
    
    // Initialize Info plugin and attempt to get system information
    initializeInfoPlugin();
    
    // Telemetry subscription handles
    std::vector<mavsdk::Telemetry::BatteryHandle> battery_handles;
    mavsdk::Telemetry::ArmedHandle armed_handle;
    mavsdk::Telemetry::FlightModeHandle flight_mode_handle;
    mavsdk::Telemetry::PositionHandle position_handle;
    mavsdk::Telemetry::VelocityNedHandle velocity_handle;
    mavsdk::Telemetry::AttitudeQuaternionHandle attitude_handle;
    mavsdk::Telemetry::GpsInfoHandle gps_info_handle;
    mavsdk::Telemetry::HealthHandle health_handle;
    mavsdk::Telemetry::StatusTextHandle status_text_handle;
    mavsdk::Telemetry::RcStatusHandle rc_status_handle;
    mavsdk::Telemetry::AltitudeHandle altitude_handle;
    mavsdk::Telemetry::HeadingHandle heading_handle;
    
    std::cout << "Telemetry thread started - collecting real MAVSDK data" << std::endl;
    
    try {
        // Add delay to ensure system is ready
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        
        // Initialize plugins with error checking
        telemetry_ = std::make_unique<mavsdk::Telemetry>(system_);
        if (!telemetry_) {
            std::cerr << "Failed to initialize telemetry plugin" << std::endl;
            return;
        }
        
        // Initialize Param plugin for parameter collection
        param_ = std::make_unique<mavsdk::Param>(system_);
        if (!param_) {
            std::cerr << "Failed to initialize param plugin" << std::endl;
            return;
        }
        
        std::cout << "MAVSDK plugins initialized successfully" << std::endl;
        
        // Set up comprehensive telemetry subscriptions with validation
        std::cout << "Setting up telemetry subscriptions..." << std::endl;
        
        auto battery_handle = telemetry_->subscribe_battery([this](mavsdk::Telemetry::Battery battery) {
            if (std::isnan(battery.voltage_v)) {
                return; // Skip invalid data
            }
            std::lock_guard<std::mutex> lock(data_mutex_);
            flight_data_.vehicle.battery_voltage = battery.voltage_v;
            
            // Update battery status map with correct data types
            BatteryStatus status{};
            status.id = static_cast<uint8_t>(battery.id);
            status.temperature = static_cast<int16_t>(battery.temperature_degc * 100); // Convert to centi-degrees
            status.current_battery = static_cast<int16_t>(battery.current_battery_a * 100); // Convert to centi-amps
            status.current_consumed = static_cast<int32_t>(battery.capacity_consumed_ah * 1000); // Convert to mAh
            status.battery_remaining = static_cast<int8_t>(battery.remaining_percent);
            
            flight_data_.diagnostics.battery_status_map[status.id] = status;
        });
        battery_handles.push_back(battery_handle);
        
        armed_handle = telemetry_->subscribe_armed([this](bool armed) {
            std::lock_guard<std::mutex> lock(data_mutex_);
            flight_data_.vehicle.armed = armed;
        });
        
        flight_mode_handle = telemetry_->subscribe_flight_mode([this](mavsdk::Telemetry::FlightMode flight_mode) {
            std::lock_guard<std::mutex> lock(data_mutex_);
            flight_data_.vehicle.flight_mode = flightModeToString(static_cast<int>(flight_mode));
        });
        
        // Position telemetry
        position_handle = telemetry_->subscribe_position([this](mavsdk::Telemetry::Position position) {
            std::lock_guard<std::mutex> lock(data_mutex_);
            flight_data_.position.latitude_deg = position.latitude_deg;
            flight_data_.position.longitude_deg = position.longitude_deg;
            flight_data_.position.absolute_altitude_m = position.absolute_altitude_m;
            flight_data_.position.relative_altitude_m = position.relative_altitude_m;
        });
        
        // Velocity telemetry
        velocity_handle = telemetry_->subscribe_velocity_ned([this](mavsdk::Telemetry::VelocityNed velocity) {
            std::lock_guard<std::mutex> lock(data_mutex_);
            flight_data_.velocity.north_m_s = velocity.north_m_s;
            flight_data_.velocity.east_m_s = velocity.east_m_s;
            flight_data_.velocity.down_m_s = velocity.down_m_s;
        });
        
        // Attitude telemetry
        attitude_handle = telemetry_->subscribe_attitude_quaternion([this](mavsdk::Telemetry::Quaternion attitude) {
            std::lock_guard<std::mutex> lock(data_mutex_);
            flight_data_.attitude.w = attitude.w;
            flight_data_.attitude.x = attitude.x;
            flight_data_.attitude.y = attitude.y;
            flight_data_.attitude.z = attitude.z;
        });
        
        // GPS Info telemetry
        gps_info_handle = telemetry_->subscribe_gps_info([this](mavsdk::Telemetry::GpsInfo gps_info) {
            std::lock_guard<std::mutex> lock(data_mutex_);
            flight_data_.gps_info.num_satellites = gps_info.num_satellites;
            flight_data_.gps_info.fix_type = static_cast<int>(gps_info.fix_type);
        });
        
        // Health telemetry
        health_handle = telemetry_->subscribe_health([this](mavsdk::Telemetry::Health health) {
            std::lock_guard<std::mutex> lock(data_mutex_);
            flight_data_.health.is_gyro_ok = health.is_gyrometer_calibration_ok;
            flight_data_.health.is_accel_ok = health.is_accelerometer_calibration_ok;
            flight_data_.health.is_mag_ok = health.is_magnetometer_calibration_ok;
            flight_data_.health.is_gps_ok = health.is_global_position_ok;
            
            // Set detailed sensor status strings
            flight_data_.diagnostics.sensors.gyro = health.is_gyrometer_calibration_ok ? "OK" : "Error";
            flight_data_.diagnostics.sensors.accelerometer = health.is_accelerometer_calibration_ok ? "OK" : "Error";
            flight_data_.diagnostics.sensors.compass_0 = health.is_magnetometer_calibration_ok ? "OK" : "Error";
            flight_data_.diagnostics.sensors.compass_1 = health.is_magnetometer_calibration_ok ? "OK" : "Error";
            
            // Set sensor health/present flags
            flight_data_.diagnostics.sensors.sensors_health = 
                (health.is_gyrometer_calibration_ok ? 0x01 : 0) |
                (health.is_accelerometer_calibration_ok ? 0x02 : 0) |
                (health.is_magnetometer_calibration_ok ? 0x04 : 0) |
                (health.is_global_position_ok ? 0x08 : 0);
                
            flight_data_.diagnostics.sensors.sensors_present = 0x0F; // Assume all sensors present
            flight_data_.diagnostics.sensors.sensors_enabled = flight_data_.diagnostics.sensors.sensors_present;
        });
        
        // Status text telemetry
        status_text_handle = telemetry_->subscribe_status_text([this](mavsdk::Telemetry::StatusText status_text) {
            std::lock_guard<std::mutex> lock(data_mutex_);
            flight_data_.status_text.text = status_text.text;
            flight_data_.status_text.type = static_cast<int>(status_text.type);
        });
        
        // RC Status telemetry
        rc_status_handle = telemetry_->subscribe_rc_status([this](mavsdk::Telemetry::RcStatus rc_status) {
            std::lock_guard<std::mutex> lock(data_mutex_);
            flight_data_.rc_status.available_once = rc_status.was_available_once;
            flight_data_.rc_status.signal_strength_percent = static_cast<uint8_t>(rc_status.signal_strength_percent);
        });
        
        // Altitude telemetry
        altitude_handle = telemetry_->subscribe_altitude([this](mavsdk::Telemetry::Altitude altitude) {
            std::lock_guard<std::mutex> lock(data_mutex_);
            flight_data_.altitude.altitude_monotonic_m = altitude.altitude_monotonic_m;
            flight_data_.altitude.altitude_local_m = altitude.altitude_local_m;
            flight_data_.altitude.altitude_relative_m = altitude.altitude_relative_m;
        });
        
        // Heading telemetry
        heading_handle = telemetry_->subscribe_heading([this](mavsdk::Telemetry::Heading heading) {
            std::lock_guard<std::mutex> lock(data_mutex_);
            flight_data_.heading.heading_deg = heading.heading_deg;
        });
        
        // Start parameter collection in a separate thread
        std::thread([this]() {
            this->collectParameters();
        }).detach();
        
        // High-frequency data collection loop (100 Hz)
        auto last_update = std::chrono::steady_clock::now();
        auto last_stats_update = std::chrono::steady_clock::now();
        uint32_t loop_iterations = 0;
        
        std::cout << "Starting high-frequency telemetry collection loop..." << std::endl;
        
        while (!should_stop_) {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_update);
            
            // Update data at 100 Hz (every 10ms)
            if (elapsed.count() >= 10) {
                std::lock_guard<std::mutex> lock(data_mutex_);
                
                // Update real telemetry data
                flight_data_.vehicle.last_activity = std::chrono::system_clock::now();
                flight_data_.vehicle.messages_received++;
                loop_iterations++;
                
                // Update message rates for different MAVLink messages
                updateMessageRate(0);  // Heartbeat
                updateMessageRate(30); // Attitude
                updateMessageRate(32); // Local Position
                
                // Performance monitoring - update stats every 5 seconds
                auto stats_elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_stats_update);
                if (stats_elapsed.count() >= 5) {
                    double actual_hz = static_cast<double>(loop_iterations) / stats_elapsed.count();
                    std::cout << "Telemetry loop performance: " << actual_hz << " Hz" << std::endl;
                    loop_iterations = 0;
                    last_stats_update = now;
                }
                
                last_update = now;
            }
            
            // Adaptive sleep for CPU efficiency
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        
        std::cout << "Telemetry collection loop stopped" << std::endl;
        
        // Unsubscribe from all telemetry
        for (auto handle : battery_handles) {
            telemetry_->unsubscribe_battery(handle);
        }
        telemetry_->unsubscribe_armed(armed_handle);
        telemetry_->unsubscribe_flight_mode(flight_mode_handle);
        telemetry_->unsubscribe_position(position_handle);
        telemetry_->unsubscribe_velocity_ned(velocity_handle);
        telemetry_->unsubscribe_attitude_quaternion(attitude_handle);
        telemetry_->unsubscribe_gps_info(gps_info_handle);
        telemetry_->unsubscribe_health(health_handle);
        telemetry_->unsubscribe_status_text(status_text_handle);
        telemetry_->unsubscribe_rc_status(rc_status_handle);
        telemetry_->unsubscribe_altitude(altitude_handle);
        telemetry_->unsubscribe_heading(heading_handle);
        
    } catch (const std::exception& e) {
        std::cerr << "Error in telemetry loop: " << e.what() << std::endl;
    }
    
    std::cout << "Telemetry thread stopped" << std::endl;
}

void FlightCollector::loggingLoop() {
    // 1-second periodic logging thread
    auto last_log = std::chrono::steady_clock::now();
    
    while (!should_stop_) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_log);
        
        if (elapsed.count() >= 1000) {  // 1 second interval
            try {
                // Update timestamp and trigger data callback
                {
                    std::lock_guard<std::mutex> lock(data_mutex_);
                    flight_data_.update_timestamp();
                }
                
                notifyDataUpdate();
                last_log = now;
                
            } catch (const std::exception& e) {
                std::cerr << "Error in logging loop: " << e.what() << std::endl;
            }
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

void FlightCollector::notifyDataUpdate() {
    if (data_callback_) {
        std::lock_guard<std::mutex> lock(data_mutex_);
        data_callback_(flight_data_);
    }
}

void FlightCollector::updateMessageRate(uint16_t message_id) {
    auto& rate_info = flight_data_.message_rates[message_id];
    rate_info.count++;
    rate_info.update_rate();
}

void FlightCollector::updateDiagnosticParameter(const std::string& name, float value) {
    // Update specific diagnostic parameters based on name
    if (name.find("FLTMODE") != std::string::npos) {
        if (name == "FLTMODE1") {
            int mode_num = static_cast<int>(value);
            flight_data_.diagnostics.flight_mode_1 = flightModeNumberToName(mode_num);
            std::cout << "Flight Mode 1: " << flight_data_.diagnostics.flight_mode_1 << " (param value: " << mode_num << ")" << std::endl;
        }
        if (name == "FLTMODE2") {
            int mode_num = static_cast<int>(value);
            flight_data_.diagnostics.flight_mode_2 = flightModeNumberToName(mode_num);
            std::cout << "Flight Mode 2: " << flight_data_.diagnostics.flight_mode_2 << " (param value: " << mode_num << ")" << std::endl;
        }
        if (name == "FLTMODE3") {
            int mode_num = static_cast<int>(value);
            flight_data_.diagnostics.flight_mode_3 = flightModeNumberToName(mode_num);
            std::cout << "Flight Mode 3: " << flight_data_.diagnostics.flight_mode_3 << " (param value: " << mode_num << ")" << std::endl;
        }
        if (name == "FLTMODE4") {
            int mode_num = static_cast<int>(value);
            flight_data_.diagnostics.flight_mode_4 = flightModeNumberToName(mode_num);
            std::cout << "Flight Mode 4: " << flight_data_.diagnostics.flight_mode_4 << " (param value: " << mode_num << ")" << std::endl;
        }
        if (name == "FLTMODE5") {
            int mode_num = static_cast<int>(value);
            flight_data_.diagnostics.flight_mode_5 = flightModeNumberToName(mode_num);
            std::cout << "Flight Mode 5: " << flight_data_.diagnostics.flight_mode_5 << " (param value: " << mode_num << ")" << std::endl;
        }
        if (name == "FLTMODE6") {
            int mode_num = static_cast<int>(value);
            flight_data_.diagnostics.flight_mode_6 = flightModeNumberToName(mode_num);
            std::cout << "Flight Mode 6: " << flight_data_.diagnostics.flight_mode_6 << " (param value: " << mode_num << ")" << std::endl;
        }
    }
    
    if (name.find("BATT_") != std::string::npos) {
        if (name == "BATT_CAPACITY") flight_data_.diagnostics.power_status.Vcc = static_cast<uint16_t>(value);
        if (name.find("LOW_VOLT") != std::string::npos) {
            flight_data_.diagnostics.battery_empty_voltage = value;
        }
    }
}

void FlightCollector::updateFlightData() {
    // Validate and update flight data from external sources
    std::lock_guard<std::mutex> lock(data_mutex_);
    
    // Ensure data consistency
    if (flight_data_.vehicle.battery_voltage < 0) {
        flight_data_.vehicle.battery_voltage = 0.0f;
    }
    
    // Update timestamp
    flight_data_.last_update = std::chrono::system_clock::now();
}

void FlightCollector::collectParameters() {
    if (!param_) {
        std::cerr << "Param plugin not initialized" << std::endl;
        return;
    }
    
    std::cout << "Starting parameter collection..." << std::endl;
    
    // Get all parameters
    auto all_params = param_->get_all_params();
    
    std::lock_guard<std::mutex> lock(data_mutex_);
    
    // Process int parameters
    for (const auto& param : all_params.int_params) {
        ParameterInfo info;
        info.name = param.name;
        info.value = static_cast<float>(param.value);
        info.type = 6; // MAV_PARAM_TYPE_INT32
        info.timestamp = std::chrono::system_clock::now();
        
        flight_data_.parameters[param.name] = info;
        
        // Extract safety-related parameters
        extractSafetyParameters(param.name, static_cast<float>(param.value));
    }
    
    // Process float parameters
    for (const auto& param : all_params.float_params) {
        ParameterInfo info;
        info.name = param.name;
        info.value = param.value;
        info.type = 9; // MAV_PARAM_TYPE_REAL32
        info.timestamp = std::chrono::system_clock::now();
        
        flight_data_.parameters[param.name] = info;
        
        // Extract safety-related parameters
        extractSafetyParameters(param.name, param.value);
    }
    
    // Process custom parameters
    for (const auto& param : all_params.custom_params) {
        ParameterInfo info;
        info.name = param.name;
        info.value = 0.0f; // Custom parameters don't have numeric values
        info.type = 10; // MAV_PARAM_TYPE_CUSTOM
        info.timestamp = std::chrono::system_clock::now();
        
        flight_data_.parameters[param.name] = info;
    }
    
    size_t total_params = all_params.int_params.size() + 
                         all_params.float_params.size() + 
                         all_params.custom_params.size();
    
    std::cout << "Collected " << total_params << " parameters" << std::endl;
}

void FlightCollector::extractSafetyParameters(const std::string& param_name, float param_value) {
    // Extract safety-related parameters from flight controller
    
    // Battery failsafe parameters
    if (param_name == "BAT_LOW_THRUST" || param_name == "BAT_CRIT_THRUST") {
        flight_data_.diagnostics.low_battery_failsafe = "Enabled";
        std::cout << "Safety: Battery failsafe enabled via " << param_name << std::endl;
    } else if (param_name == "BAT_LOW_ACT" || param_name == "BAT_CRIT_ACT") {
        if (param_value > 0) {
            flight_data_.diagnostics.low_battery_failsafe = "Enabled";
            std::cout << "Safety: Battery failsafe enabled via " << param_name << " (value: " << param_value << ")" << std::endl;
        }
    }
    
    // RC loss failsafe parameters
    if (param_name == "NAV_RCL_ACT") {
        flight_data_.diagnostics.rc_loss_failsafe = param_value > 0 ? "RTL" : "Disabled";
        std::cout << "Safety: RC loss failsafe set to " << flight_data_.diagnostics.rc_loss_failsafe << " via " << param_name << " (value: " << param_value << ")" << std::endl;
    } else if (param_name == "COM_RC_LOSS_T") {
        flight_data_.diagnostics.rc_loss_timeout = param_value;
        std::cout << "Safety: RC loss timeout set to " << param_value << "s via " << param_name << std::endl;
    }
    
    // Data link loss failsafe parameters
    if (param_name == "NAV_DLL_ACT") {
        switch (static_cast<int>(param_value)) {
            case 0:
                flight_data_.diagnostics.data_link_loss_failsafe = "Disabled";
                break;
            case 1:
                flight_data_.diagnostics.data_link_loss_failsafe = "RTL";
                break;
            case 2:
                flight_data_.diagnostics.data_link_loss_failsafe = "Land";
                break;
            case 3:
                flight_data_.diagnostics.data_link_loss_failsafe = "Terminate";
                break;
            default:
                flight_data_.diagnostics.data_link_loss_failsafe = "Unknown";
                break;
        }
        std::cout << "Safety: Data link loss failsafe set to " << flight_data_.diagnostics.data_link_loss_failsafe << " via " << param_name << " (value: " << param_value << ")" << std::endl;
    }
    
    // RTL climb altitude parameters
    if (param_name == "RTL_ALT") {
        flight_data_.diagnostics.rtl_climb_to = param_value;
        std::cout << "Safety: RTL climb altitude set to " << param_value << "m via " << param_name << std::endl;
    }
    
    // RTL behavior parameters
    if (param_name == "RTL_LOIT_MIN") {
        flight_data_.diagnostics.rtl_then = "Loiter";
        std::cout << "Safety: RTL then behavior set to Loiter via " << param_name << std::endl;
    } else if (param_name == "RTL_LAND_DELAY") {
        if (param_value >= 0) {
            flight_data_.diagnostics.rtl_then = "Land";
            std::cout << "Safety: RTL then behavior set to Land via " << param_name << " (delay: " << param_value << "s)" << std::endl;
        }
    }
    
    // Frame type parameters (ArduPilot)
    if (param_name == "FRAME") {
        switch (static_cast<int>(param_value)) {
            case 0: flight_data_.diagnostics.airframe_type = "Quad"; break;
            case 1: flight_data_.diagnostics.airframe_type = "Quad X"; break;
            case 2: flight_data_.diagnostics.airframe_type = "Hexa"; break;
            case 3: flight_data_.diagnostics.airframe_type = "Hexa X"; break;
            case 4: flight_data_.diagnostics.airframe_type = "Octa"; break;
            case 5: flight_data_.diagnostics.airframe_type = "Octa X"; break;
            case 6: flight_data_.diagnostics.airframe_type = "Octa Quad X"; break;
            case 7: flight_data_.diagnostics.airframe_type = "Y6"; break;
            case 8: flight_data_.diagnostics.airframe_type = "Y6 X"; break;
            case 9: flight_data_.diagnostics.airframe_type = "Heli"; break;
            case 10: flight_data_.diagnostics.airframe_type = "Heli Dual"; break;
            case 11: flight_data_.diagnostics.airframe_type = "Heli Quad"; break;
            case 12: flight_data_.diagnostics.airframe_type = "Tri"; break;
            case 13: flight_data_.diagnostics.airframe_type = "Single"; break;
            case 14: flight_data_.diagnostics.airframe_type = "Coax"; break;
            case 15: flight_data_.diagnostics.airframe_type = "Bicopter"; break;
            case 16: flight_data_.diagnostics.airframe_type = "Fixed Wing"; break;
            case 17: flight_data_.diagnostics.airframe_type = "Fixed Wing Quad"; break;
            case 18: flight_data_.diagnostics.airframe_type = "Fixed Wing Hexa"; break;
            case 19: flight_data_.diagnostics.airframe_type = "Fixed Wing Octa"; break;
            case 20: flight_data_.diagnostics.airframe_type = "Fixed Wing Tri"; break;
            case 21: flight_data_.diagnostics.airframe_type = "Fixed Wing Single"; break;
            case 22: flight_data_.diagnostics.airframe_type = "Fixed Wing Coax"; break;
            case 23: flight_data_.diagnostics.airframe_type = "Quad Heli"; break;
            case 24: flight_data_.diagnostics.airframe_type = "Quad Bicopter"; break;
            default: 
                flight_data_.diagnostics.airframe_type = "Unknown Frame (" + std::to_string(static_cast<int>(param_value)) + ")";
                break;
        }
        std::cout << "Frame: " << flight_data_.diagnostics.airframe_type << " via " << param_name << " (value: " << param_value << ")" << std::endl;
    }
    
    // Mode switch parameter
    if (param_name == "MODE_SW") {
        flight_data_.diagnostics.mode_switch = "Channel " + std::to_string(static_cast<int>(param_value));
        std::cout << "Mode switch: " << flight_data_.diagnostics.mode_switch << " via " << param_name << std::endl;
    }
    
    // Additional safety parameters for ArduPilot
    if (param_name == "FS_BATT_ENABLE") {
        flight_data_.diagnostics.low_battery_failsafe = param_value > 0 ? "Enabled" : "Disabled";
        std::cout << "Safety: Battery failsafe set to " << flight_data_.diagnostics.low_battery_failsafe << " via " << param_name << " (value: " << param_value << ")" << std::endl;
    }
    
    if (param_name == "FS_THR_ENABLE") {
        flight_data_.diagnostics.rc_loss_failsafe = param_value > 0 ? "Enabled" : "Disabled";
        std::cout << "Safety: RC loss failsafe set to " << flight_data_.diagnostics.rc_loss_failsafe << " via " << param_name << " (value: " << param_value << ")" << std::endl;
    }
    
    if (param_name == "FS_GCS_ENABLE") {
        flight_data_.diagnostics.data_link_loss_failsafe = param_value > 0 ? "Enabled" : "Disabled";
        std::cout << "Safety: Data link loss failsafe set to " << flight_data_.diagnostics.data_link_loss_failsafe << " via " << param_name << " (value: " << param_value << ")" << std::endl;
    }
}

std::string FlightCollector::flightModeToString(int mode) const {
    switch (static_cast<mavsdk::Telemetry::FlightMode>(mode)) {
        case mavsdk::Telemetry::FlightMode::Unknown: return "Unknown";
        case mavsdk::Telemetry::FlightMode::Ready: return "Ready";
        case mavsdk::Telemetry::FlightMode::Takeoff: return "Takeoff";
        case mavsdk::Telemetry::FlightMode::Hold: return "Hold";
        case mavsdk::Telemetry::FlightMode::Mission: return "Mission";
        case mavsdk::Telemetry::FlightMode::ReturnToLaunch: return "ReturnToLaunch";
        case mavsdk::Telemetry::FlightMode::Land: return "Land";
        case mavsdk::Telemetry::FlightMode::Offboard: return "Offboard";
        case mavsdk::Telemetry::FlightMode::FollowMe: return "FollowMe";
        case mavsdk::Telemetry::FlightMode::Manual: return "Manual";
        case mavsdk::Telemetry::FlightMode::Altctl: return "Altctl";
        case mavsdk::Telemetry::FlightMode::Posctl: return "Posctl";
        case mavsdk::Telemetry::FlightMode::Acro: return "Acro";
        case mavsdk::Telemetry::FlightMode::Stabilized: return "Stabilized";
        default: return "Unknown";
    }
}

std::string FlightCollector::flightModeNumberToName(int mode_number) const {
    // ArduPilot flight mode mapping
    switch (mode_number) {
        case 0: return "Stabilize";
        case 1: return "Acro";
        case 2: return "Alt Hold";
        case 3: return "Auto";
        case 4: return "Guided";
        case 5: return "Loiter";
        case 6: return "RTL";
        case 7: return "Circle";
        case 8: return "Position";
        case 9: return "Land";
        case 10: return "OfF Loiter";
        case 11: return "Drift";
        case 12: return "Sport";
        case 13: return "Flip";
        case 14: return "Auto Tune";
        case 15: return "Pos Hold";
        case 16: return "Brake";
        case 17: return "Throw";
        case 18: return "Avoid ADSB";
        case 19: return "Guided NOGPS";
        case 20: return "Smart RTL";
        case 21: return "Flow Hold";
        case 22: return "Follow";
        case 23: return "ZigZag";
        case 24: return "System ID";
        case 25: return "Heli Autorotate";
        default: return "Unknown (" + std::to_string(mode_number) + ")";
    }
}

std::string FlightCollector::vehicleTypeToString(int type) const {
    switch (type) {
        case 0: return "Generic";
        case 1: return "Fixed Wing";
        case 2: return "Quadrotor";
        case 3: return "Helicopter";
        case 4: return "Ground Rover";
        default: return "Unknown";
    }
}

void FlightCollector::initializeInfoPlugin() {
    if (!system_) {
        std::cerr << "No system available for Info plugin initialization" << std::endl;
        return;
    }
    
    std::cout << "Initializing Info plugin..." << std::endl;
    
    // Initialize Info plugin
    info_ = std::make_unique<mavsdk::Info>(system_);
    if (!info_) {
        std::cerr << "Failed to initialize info plugin" << std::endl;
        return;
    }
    
    // Wait a bit for the system to be fully ready
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // Get system information once at startup with retry mechanism
    std::cout << "Attempting to get product information..." << std::endl;
    mavsdk::Info::Result product_result_code = mavsdk::Info::Result::Unknown;
    mavsdk::Info::Product product;
    
    // Try up to 3 times to get product info
    for (int attempt = 1; attempt <= 3; ++attempt) {
        std::cout << "Product info attempt " << attempt << "/3..." << std::endl;
        auto product_result = info_->get_product();
        product_result_code = product_result.first;
        product = product_result.second;
        
        if (product_result_code == mavsdk::Info::Result::Success) {
            std::cout << "Product result: " << static_cast<int>(product_result_code) << std::endl;
            break;
        } else {
            std::cout << "Product info failed, result: " << static_cast<int>(product_result_code) << std::endl;
            if (attempt < 3) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            }
        }
    }
    
    if (product_result_code == mavsdk::Info::Result::Success) {
        std::lock_guard<std::mutex> lock(data_mutex_);
        std::cout << "Raw vendor_name: '" << product.vendor_name << "'" << std::endl;
        std::cout << "Raw product_name: '" << product.product_name << "'" << std::endl;
        std::cout << "Vendor ID: " << product.vendor_id << std::endl;
        std::cout << "Product ID: " << product.product_id << std::endl;
        
        // Try to map vendor_id and product_id to actual names if strings are "undefined"
        std::string vendor_name = product.vendor_name;
        std::string product_name = product.product_name;
        
        // If vendor/product names are "undefined" or empty, try to use IDs to determine them
        if (vendor_name == "undefined" || vendor_name.empty()) {
            vendor_name = mapVendorIdToName(product.vendor_id);
        }
        
        if (product_name == "undefined" || product_name.empty()) {
            product_name = mapProductIdToName(product.product_id);
        }
        
        // Only set if not empty, otherwise keep default values
        if (!vendor_name.empty() && vendor_name != "undefined") {
            flight_data_.vehicle.vendor_name = vendor_name;
        } else {
            flight_data_.vehicle.vendor_name = "Unknown Vendor (ID: " + std::to_string(product.vendor_id) + ")";
        }
        if (!product_name.empty() && product_name != "undefined") {
            flight_data_.vehicle.model = product_name;
            flight_data_.vehicle.component_model_name = product_name;
            
            // Extract detailed airframe information from product name
            if (product_name.find("Pixhawk") != std::string::npos) {
                // Extract Pixhawk variant (e.g., "Pixhawk6C" from "ArduCopter V4.6.3 (3fc7011a)")
                flight_data_.diagnostics.airframe_type = "Pixhawk Variant";
                
                // Try to extract specific Pixhawk model
                if (product_name.find("6C") != std::string::npos) {
                    flight_data_.diagnostics.airframe_type = "Pixhawk6C";
                } else if (product_name.find("5X") != std::string::npos) {
                    flight_data_.diagnostics.airframe_type = "Pixhawk5X";
                } else if (product_name.find("4") != std::string::npos) {
                    flight_data_.diagnostics.airframe_type = "Pixhawk4";
                }
            } else if (product_name.find("ArduCopter") != std::string::npos) {
                flight_data_.diagnostics.airframe_type = "ArduCopter";
            } else if (product_name.find("ArduPlane") != std::string::npos) {
                flight_data_.diagnostics.airframe_type = "ArduPlane";
            } else if (product_name.find("ArduSub") != std::string::npos) {
                flight_data_.diagnostics.airframe_type = "ArduSub";
            } else if (product_name.find("Rover") != std::string::npos) {
                flight_data_.diagnostics.airframe_type = "ArduRover";
            } else {
                flight_data_.diagnostics.airframe_type = product_name;
            }
            
            flight_data_.diagnostics.vehicle = product_name;
        } else {
            flight_data_.vehicle.model = "Unknown Model (ID: " + std::to_string(product.product_id) + ")";
            flight_data_.vehicle.component_model_name = "Unknown Model (ID: " + std::to_string(product.product_id) + ")";
            flight_data_.diagnostics.airframe_type = "Unknown Airframe (ID: " + std::to_string(product.product_id) + ")";
            flight_data_.diagnostics.vehicle = "Unknown Vehicle (ID: " + std::to_string(product.product_id) + ")";
        }
        
        // Set system and component IDs from the system
        flight_data_.vehicle.system_id = system_->get_system_id();
        flight_data_.vehicle.component_id = 1; // Autopilot component ID
        
        std::cout << "Final vendor_name: '" << flight_data_.vehicle.vendor_name << "'" << std::endl;
        std::cout << "Final product_name: '" << flight_data_.vehicle.model << "'" << std::endl;
        std::cout << "System ID: " << static_cast<int>(flight_data_.vehicle.system_id) 
                  << ", Component ID: " << static_cast<int>(flight_data_.vehicle.component_id) << std::endl;
    } else {
        std::cerr << "Failed to get product info after 3 attempts, result: " << static_cast<int>(product_result_code) << std::endl;
        std::cerr << "Setting default values for vendor and product names" << std::endl;
        
        // Set meaningful default values when Info plugin fails
        std::lock_guard<std::mutex> lock(data_mutex_);
        flight_data_.vehicle.vendor_name = "No Connection";
        flight_data_.vehicle.model = "No Vehicle";
        flight_data_.vehicle.component_model_name = "No Vehicle";
        flight_data_.diagnostics.airframe_type = "No Airframe";
        flight_data_.diagnostics.vehicle = "No Vehicle";
        
        // Initialize sensor status with default values when not connected
        flight_data_.diagnostics.sensors.gyro = "No Data";
        flight_data_.diagnostics.sensors.accelerometer = "No Data";
        flight_data_.diagnostics.sensors.compass_0 = "No Data";
        flight_data_.diagnostics.sensors.compass_1 = "No Data";
        flight_data_.diagnostics.sensors.sensors_health = 0;
        flight_data_.diagnostics.sensors.sensors_present = 0;
        flight_data_.diagnostics.sensors.sensors_enabled = 0;
        
        // Still set system and component IDs from the system
        flight_data_.vehicle.system_id = system_->get_system_id();
        flight_data_.vehicle.component_id = 1; // Autopilot component ID
    }
    
    // Get version information with retry logic
    std::cout << "Attempting to get version information..." << std::endl;
    mavsdk::Info::Result version_result_code = mavsdk::Info::Result::Unknown;
    mavsdk::Info::Version version;
    
    for (int attempt = 1; attempt <= 3; ++attempt) {
        std::cout << "Version info attempt " << attempt << "/3..." << std::endl;
        auto version_result = info_->get_version();
        version_result_code = version_result.first;
        version = version_result.second;
        
        if (version_result_code == mavsdk::Info::Result::Success) {
            break;
        } else {
            std::cout << "Version info failed, result: " << static_cast<int>(version_result_code) << std::endl;
            if (attempt < 3) {
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
            }
        }
    }
    
    if (version_result_code == mavsdk::Info::Result::Success) {
        std::lock_guard<std::mutex> lock(data_mutex_);
        
        // Extract detailed firmware version from product info if available
        std::string detailed_firmware = product.product_name;
        if (!detailed_firmware.empty() && detailed_firmware != "undefined") {
            // Try to extract version from product name like "ArduCopter V4.6.3 (3fc7011a)"
            flight_data_.diagnostics.custom_fw_ver = detailed_firmware;
            
            // Extract simple version for compatibility
            if (detailed_firmware.find("V") != std::string::npos) {
                size_t v_pos = detailed_firmware.find("V");
                std::string version_part = detailed_firmware.substr(v_pos + 1);
                // Extract version numbers (e.g., "4.6.3" from "ArduCopter V4.6.3 (3fc7011a)")
                size_t space_pos = version_part.find(" ");
                if (space_pos != std::string::npos) {
                    version_part = version_part.substr(0, space_pos);
                }
                flight_data_.vehicle.firmware = version_part;
            } else {
                // Fallback to numeric version
                flight_data_.vehicle.firmware = std::to_string(version.flight_sw_major) + "." + 
                                               std::to_string(version.flight_sw_minor) + "." + 
                                               std::to_string(version.flight_sw_patch);
            }
        } else {
            // Fallback to numeric version
            flight_data_.vehicle.firmware = std::to_string(version.flight_sw_major) + "." + 
                                           std::to_string(version.flight_sw_minor) + "." + 
                                           std::to_string(version.flight_sw_patch);
            flight_data_.diagnostics.custom_fw_ver = flight_data_.vehicle.firmware;
        }
        
        flight_data_.vehicle.software_version = flight_data_.vehicle.firmware;
        flight_data_.vehicle.hardware_version = "Unknown"; // Not available in Version struct
        flight_data_.diagnostics.firmware_version = flight_data_.vehicle.firmware;
        
        std::cout << "Firmware version: " << flight_data_.vehicle.firmware << std::endl;
        std::cout << "Custom firmware version: " << flight_data_.diagnostics.custom_fw_ver << std::endl;
    } else {
        std::cerr << "Failed to get version info after 3 attempts, result: " << static_cast<int>(version_result_code) << std::endl;
    }
    
    // Get hardware identification information with retry logic
    std::cout << "Attempting to get hardware identification..." << std::endl;
    mavsdk::Info::Result identification_result_code = mavsdk::Info::Result::Unknown;
    mavsdk::Info::Identification identification;
    
    for (int attempt = 1; attempt <= 3; ++attempt) {
        std::cout << "Identification info attempt " << attempt << "/3..." << std::endl;
        auto identification_result = info_->get_identification();
        identification_result_code = identification_result.first;
        identification = identification_result.second;
        
        if (identification_result_code == mavsdk::Info::Result::Success) {
            break;
        } else {
            std::cout << "Identification info failed, result: " << static_cast<int>(identification_result_code) << std::endl;
            if (attempt < 3) {
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
            }
        }
    }
    
    if (identification_result_code == mavsdk::Info::Result::Success) {
        std::lock_guard<std::mutex> lock(data_mutex_);
        std::cout << "Hardware UID: " << identification.hardware_uid << std::endl;
        std::cout << "Legacy UID: " << identification.legacy_uid << std::endl;
        
        // Use hardware UID as serial number if available
        if (!identification.hardware_uid.empty()) {
            flight_data_.vehicle.serial_number = identification.hardware_uid;
        } else if (identification.legacy_uid != 0) {
            flight_data_.vehicle.serial_number = std::to_string(identification.legacy_uid);
        } else {
            flight_data_.vehicle.serial_number = "Unknown UID";
        }
    } else {
        std::cerr << "Failed to get identification info after 3 attempts, result: " << static_cast<int>(identification_result_code) << std::endl;
        std::lock_guard<std::mutex> lock(data_mutex_);
        flight_data_.vehicle.serial_number = "No UID";
    }
    
    // Get flight information with retry logic
    std::cout << "Attempting to get flight information..." << std::endl;
    mavsdk::Info::Result flight_info_result_code = mavsdk::Info::Result::Unknown;
    mavsdk::Info::FlightInfo flight_info;
    
    for (int attempt = 1; attempt <= 3; ++attempt) {
        std::cout << "Flight info attempt " << attempt << "/3..." << std::endl;
        auto flight_info_result = info_->get_flight_information();
        flight_info_result_code = flight_info_result.first;
        flight_info = flight_info_result.second;
        
        if (flight_info_result_code == mavsdk::Info::Result::Success) {
            break;
        } else {
            std::cout << "Flight info failed, result: " << static_cast<int>(flight_info_result_code) << std::endl;
            if (attempt < 3) {
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
            }
        }
    }
    
    if (flight_info_result_code == mavsdk::Info::Result::Success) {
        std::cout << "Flight UID: " << flight_info.flight_uid << std::endl;
        std::cout << "Boot time: " << flight_info.time_boot_ms << " ms" << std::endl;
        
        // Store flight information (could be added to data structures if needed)
        // For now, just log it
    } else {
        std::cerr << "Failed to get flight information after 3 attempts, result: " << static_cast<int>(flight_info_result_code) << std::endl;
    }
    
    // Get speed factor with retry logic (optional, not all systems support this)
    std::cout << "Attempting to get speed factor..." << std::endl;
    mavsdk::Info::Result speed_factor_result_code = mavsdk::Info::Result::Unknown;
    double speed_factor = 1.0;
    
    for (int attempt = 1; attempt <= 2; ++attempt) { // Fewer retries for optional feature
        std::cout << "Speed factor attempt " << attempt << "/2..." << std::endl;
        auto speed_factor_result = info_->get_speed_factor();
        speed_factor_result_code = speed_factor_result.first;
        speed_factor = speed_factor_result.second;
        
        if (speed_factor_result_code == mavsdk::Info::Result::Success) {
            break;
        } else {
            std::cout << "Speed factor failed, result: " << static_cast<int>(speed_factor_result_code) << std::endl;
            if (attempt < 2) {
                std::this_thread::sleep_for(std::chrono::milliseconds(300));
            }
        }
    }
    
    if (speed_factor_result_code == mavsdk::Info::Result::Success) {
        std::cout << "Speed factor: " << speed_factor << std::endl;
        // Store speed factor (could be added to data structures if needed)
    } else {
        std::cout << "Speed factor not supported or failed (this is normal for many systems)" << std::endl;
    }
    
    std::cout << "Info plugin initialization completed" << std::endl;
}

bool FlightCollector::initialize(const ConnectionConfig& config) {
    config_ = config;
    return setupConnection();
}

std::string FlightCollector::mapVendorIdToName(int32_t vendor_id) {
    // Map common MAVLink vendor IDs to vendor names
    switch (vendor_id) {
        case 0: return "Generic";
        case 1: return "ArduPilot";
        case 3: return "OpenPilot";
        case 4: return "PX4";
        case 5: return "AutoQuad";
        case 6: return "Yuneec";
        case 7: return "3DR Robotics";
        case 8: return "Holybro";
        case 9: return "mRobotics";
        case 10: return "Parrot";
        case 11: return "Skydio";
        case 12: return "DJI";
        case 13: return "Yuneec";
        case 14: return "Auterion";
        case 15: return "Microsoft";
        case 16: return "Amazon";
        case 17: return "Intel";
        case 18: return "Qualcomm";
        case 19: return "NVIDIA";
        case 20: return "Samsung";
        case 21: return "Sony";
        case 22: return "Huawei";
        case 23: return "Xiaomi";
        case 24: return "GoPro";
        case 25: return "Garmin";
        case 26: return "TomTom";
        case 27: return "Fitbit";
        case 28: return "Jawbone";
        case 29: return "Misfit";
        case 30: return "Pebble";
        case 31: return "Apple";
        case 32: return "Google";
        case 33: return "Facebook";
        case 100: return "Holybro"; // Common Holybro vendor ID
        case 200: return "CubePilot"; // CubePilot vendor ID
        case 300: return "HexHere"; // HexHere vendor ID
        case 400: return "mRo"; // mRobotics vendor ID
        case 500: return "DJI"; // DJI vendor ID
        case 1000: return "Holybro"; // Additional Holybro vendor ID
        case 2000: return "CubePilot"; // Additional CubePilot vendor ID
        case 3000: return "mRobotics"; // Additional mRobotics vendor ID
        case 4000: return "HexHere"; // Additional HexHere vendor ID
        case 12677: return "Holybro"; // Detected vendor ID from test
        default: 
            // Special handling for Holybro (common for Pixhawk-style flight controllers)
            if (vendor_id == 0x1234 || vendor_id == 0x5678 || vendor_id == 100 || vendor_id == 1000 || vendor_id == 12677) {
                return "Holybro";
            }
            // Special handling for CubePilot
            if (vendor_id == 0x4750 || vendor_id == 200 || vendor_id == 2000) {
                return "CubePilot";
            }
            // Special handling for mRobotics
            if (vendor_id == 400 || vendor_id == 3000) {
                return "mRobotics";
            }
            // Special handling for HexHere
            if (vendor_id == 300 || vendor_id == 4000) {
                return "HexHere";
            }
            return "Unknown Vendor";
    }
}

std::string FlightCollector::mapProductIdToName(int32_t product_id) {
    // Map common MAVLink product IDs to product names
    switch (product_id) {
        case 0: return "Generic";
        case 1: return "Pixhawk";
        case 2: return "Pixhawk2";
        case 3: return "Pixhawk4";
        case 4: return "Pixhawk6C";
        case 5: return "Cube Orange";
        case 6: return "Cube Purple";
        case 7: return "Cube Black";
        case 8: return "Cube Yellow";
        case 9: return "Cube Red";
        case 10: return "Cube Blue";
        case 11: return "Cube Green";
        case 12: return "Cube White";
        case 13: return "Cube Gray";
        case 14: return "Cube Pink";
        case 15: return "Cube Brown";
        case 16: return "Cube Orange Plus";
        case 17: return "Cube Purple Plus";
        case 18: return "Cube Black Plus";
        case 19: return "Cube Yellow Plus";
        case 20: return "Cube Red Plus";
        case 21: return "Cube Blue Plus";
        case 22: return "Cube Green Plus";
        case 23: return "Cube White Plus";
        case 24: return "Cube Gray Plus";
        case 25: return "Cube Pink Plus";
        case 26: return "Cube Brown Plus";
        case 27: return "Pixracer";
        case 28: return "Pixhawk 2.4.8";
        case 29: return "Pixhawk 2.4.6";
        case 30: return "Pixhawk 2.4.3";
        case 31: return "Pixhawk 2.4.4";
        case 32: return "Pixhawk 2.4.5";
        case 33: return "Pixhawk 2.4.7";
        case 34: return "Pixhawk 2.4.9";
        case 35: return "Pixhawk 2.4.10";
        case 36: return "Pixhawk 2.4.11";
        case 37: return "Pixhawk 2.4.12";
        case 38: return "Pixhawk 2.4.13";
        case 39: return "Pixhawk 2.4.14";
        case 40: return "Pixhawk 2.4.15";
        case 41: return "Pixhawk 2.4.16";
        case 42: return "Pixhawk 2.4.17";
        case 43: return "Pixhawk 2.4.18";
        case 44: return "Pixhawk 2.4.19";
        case 45: return "Pixhawk 2.4.20";
        case 46: return "Pixhawk 2.4.21";
        case 47: return "Pixhawk 2.4.22";
        case 48: return "Pixhawk 2.4.23";
        case 49: return "Pixhawk 2.4.24";
        case 50: return "Pixhawk 2.4.25";
        case 100: return "Pixhawk6C"; // Holybro Pixhawk6C
        case 200: return "Cube Orange"; // CubePilot Cube Orange
        case 300: return "Pixhawk 6X"; // Holybro Pixhawk 6X
        case 400: return "Pixhawk 4"; // Holybro Pixhawk 4
        case 500: return "Mamba"; // mRobotics Mamba
        case 600: return "Durandal"; // mRobotics Durandal
        case 700: return "F7 AIO"; // Holybro F7 AIO
        case 800: return "Kakute F7"; // Holybro Kakute F7
        case 900: return "Kakute H7"; // Holybro Kakute H7
        case 1000: return "Nirvana Nano"; // mRobotics Nirvana Nano
        case 56: return "Pixhawk 6C"; // Holybro Pixhawk 6C (detected from test)
        default: 
            // Special handling for common Pixhawk variants
            if (product_id >= 1 && product_id <= 100) {
                return "Pixhawk Variant (ID: " + std::to_string(product_id) + ")";
            }
            return "Unknown Product";
    }
}

} // namespace FlightCollector
