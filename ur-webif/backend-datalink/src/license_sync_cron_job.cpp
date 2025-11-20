#include "license_sync_cron_job.hpp"
#include "../thirdparty/ur-rpc-template/extensions/direct_template.h"
#include "../thirdparty/controlled-log/include/controlled_log.h"
// Include C header first to help IDE resolve dependencies
#include "../thirdparty/ur-threadder-api/include/thread_manager.h"
// ThreadManager.hpp is used for ThreadMgr::ThreadManager in constructor
#include "../thirdparty/ur-threadder-api/cpp/include/ThreadManager.hpp"
#include <chrono>
#include <nlohmann/json.hpp>
#include <ctime>
#include <cstdlib>
#include <filesystem>

using json = nlohmann::json;

LicenseSyncCronJob::LicenseSyncCronJob(const std::string& configPath, bool verbose)
    : configPath_(configPath)
    , verbose_(verbose)
    , threadManager_(std::make_shared<ThreadMgr::ThreadManager>(10)) {
    
    LOG_LICENSE_CRON_INFO("LicenseSyncCronJob initialized with heartbeat-triggered verification");
    
    // Load configuration to get database path
    try {
        configLoader_.loadFromFile(configPath_);
        const auto& dbConfig = configLoader_.getDatabaseConfig();
        systemDatabasePath_ = dbConfig.system_db_path;
        LOG_LICENSE_CRON_INFO("Database path loaded from config: " + systemDatabasePath_);
    } catch (const std::exception& e) {
        LOG_LICENSE_CRON_ERROR("Failed to load config: " + std::string(e.what()));
        LOG_LICENSE_CRON_WARN("Using default database path: " + systemDatabasePath_);
    }
    
    // Initialize database
    if (!initializeDatabase()) {
        LOG_LICENSE_CRON_ERROR("Failed to initialize database");
    }
}

LicenseSyncCronJob::~LicenseSyncCronJob() {
    stop();
    
    // Close database connection
    if (dbConnection_) {
        sqlite3_close(dbConnection_);
        dbConnection_ = nullptr;
    }
}

bool LicenseSyncCronJob::start() {
    if (running_.load()) {
        return true;
    }

    running_.store(true);
    verificationInProgress_.store(false);
    
    // Subscribe to license response topic
    if (direct_client_subscribe_topic("direct_messaging/ur-licence-mann/responses") != 0) {
        LOG_LICENSE_CRON_ERROR("Failed to subscribe to ur-licence-mann response topic");
        running_.store(false);
        return false;
    }

    LOG_LICENSE_CRON_INFO("LicenseSyncCronJob started - waiting for heartbeat triggers");
    
    // Start periodic scheduling thread
    startPeriodicScheduling();
    
    return true;
}

void LicenseSyncCronJob::stop() {
    LOG_LICENSE_CRON_INFO("Stopping LicenseSyncCronJob...");
    running_.store(false);
    
    // Stop periodic scheduling
    stopPeriodicScheduling();
    
    // Notify any waiting threads
    responseCondition_.notify_all();
    
    LOG_LICENSE_CRON_INFO("LicenseSyncCronJob stopped");
}

bool LicenseSyncCronJob::isRunning() const {
    return running_.load();
}

void LicenseSyncCronJob::handleHeartbeatMessage(const std::string& /*topic*/, const std::string& /*payload*/) {
    if (!running_.load()) {
        return;
    }

    // Ignore heartbeats while verification is in progress
    if (verificationInProgress_.load()) {
        LOG_LICENSE_CRON_DEBUG("Ignoring heartbeat - verification in progress");
        return;
    }

    try {
        LOG_LICENSE_CRON_INFO("Heartbeat received from ur-licence-mann, starting license verification");

        // Start verification in a separate thread
        threadManager_->createThread([this]() {
            this->performLicenseVerification();
        });

    } catch (const std::exception& e) {
        LOG_LICENSE_CRON_ERROR("Error handling heartbeat: " + std::string(e.what()));
    }
}

void LicenseSyncCronJob::handleLicenseResponse(const std::string& topic, const std::string& payload) {
    if (!running_.load()) {
        return;
    }

    try {
        LOG_LICENSE_CRON_DEBUG("JSON-RPC 2.0 license response received on topic: " + topic);

        if (payload.empty()) {
            LOG_LICENSE_CRON_WARN("Empty JSON-RPC 2.0 license response payload");
            return;
        }

        json response = json::parse(payload);

        // Check if this is a JSON-RPC 2.0 response from ur-licence-mann
        if (!response.contains("jsonrpc") || response["jsonrpc"].get<std::string>() != "2.0") {
            LOG_LICENSE_CRON_DEBUG("Ignoring non-JSON-RPC 2.0 response");
            return;
        }

        // Extract transaction ID (JSON-RPC 2.0 uses "id" field)
        std::string transactionId;
        if (response.contains("id")) {
            if (response["id"].is_string()) {
                transactionId = response["id"].get<std::string>();
            } else if (response["id"].is_number()) {
                transactionId = std::to_string(response["id"].get<int>());
            } else {
                LOG_LICENSE_CRON_WARN("Invalid transaction ID type in JSON-RPC 2.0 response");
                return;
            }
        } else {
            LOG_LICENSE_CRON_WARN("Missing transaction ID in JSON-RPC 2.0 response");
            return;
        }

        // Check if this is a license sync response (our transaction IDs start with "license_sync_")
        if (transactionId.find("license_sync_") != 0) {
            LOG_LICENSE_CRON_DEBUG("Ignoring non-license-sync JSON-RPC 2.0 response with ID: " + transactionId);
            return;
        }
        
        // Parse the JSON-RPC 2.0 response structure
        bool success = false;
        std::string resultData;
        std::string errorMessage;
        
        if (response.contains("success") && response["success"].is_boolean()) {
            success = response["success"].get<bool>();
        }
        
        if (response.contains("result")) {
            if (response["result"].is_string()) {
                resultData = response["result"].get<std::string>();
            } else if (response["result"].is_object()) {
                resultData = response["result"].dump();
            } else {
                resultData = response["result"].dump();
            }
        }
        
        if (response.contains("message") && response["message"].is_string()) {
            errorMessage = response["message"].get<std::string>();
        }
        
        // Store the structured JSON-RPC 2.0 response and notify waiting thread
        {
            std::lock_guard<std::mutex> lock(responseMutex_);
            pendingResponses_[transactionId] = response;
            // Also add convenience fields for easier access
            pendingResponses_[transactionId]["_extracted_success"] = success;
            pendingResponses_[transactionId]["_extracted_result"] = resultData;
            pendingResponses_[transactionId]["_extracted_error"] = errorMessage;
        }
        responseCondition_.notify_all();
        
        LOG_LICENSE_CRON_INFO("Stored JSON-RPC 2.0 response for transaction: " + transactionId);
        LOG_LICENSE_CRON_INFO("Response success: " + std::string(success ? "true" : "false"));
        if (!errorMessage.empty()) {
            LOG_LICENSE_CRON_INFO("Response message: " + errorMessage);
        }

    } catch (const std::exception& e) {
        LOG_LICENSE_CRON_ERROR("Error in JSON-RPC 2.0 license response handler: " + std::string(e.what()));
    }
}

void LicenseSyncCronJob::performLicenseVerification() {
    if (!running_.load()) {
        return;
    }

    // Set verification in progress flag
    verificationInProgress_.store(true);
    
    try {
        LOG_LICENSE_CRON_INFO("Starting JSON-RPC 2.0 license verification process");
        
        // Send license info request
        std::string infoTransactionId = sendLicenseInfoRequest();
        if (infoTransactionId.empty()) {
            LOG_LICENSE_CRON_ERROR("Failed to send JSON-RPC 2.0 license info request");
            verificationInProgress_.store(false);
            return;
        }
        
        // Wait for response with 1 second timeout
        if (!waitForResponse(infoTransactionId, std::chrono::milliseconds(1000))) {
            LOG_LICENSE_CRON_ERROR("Timeout waiting for JSON-RPC 2.0 license info response (transaction: " + infoTransactionId + ")");
            verificationInProgress_.store(false);
            return;
        }
        
        // Send license plan request
        std::string planTransactionId = sendLicensePlanRequest();
        if (planTransactionId.empty()) {
            LOG_LICENSE_CRON_ERROR("Failed to send JSON-RPC 2.0 license plan request");
            verificationInProgress_.store(false);
            return;
        }
        
        // Wait for response with 1 second timeout
        if (!waitForResponse(planTransactionId, std::chrono::milliseconds(1000))) {
            LOG_LICENSE_CRON_ERROR("Timeout waiting for JSON-RPC 2.0 license plan response (transaction: " + planTransactionId + ")");
            verificationInProgress_.store(false);
            return;
        }
        
        // Process collected JSON-RPC 2.0 responses
        json combinedData = json::object();
        {
            std::lock_guard<std::mutex> lock(responseMutex_);
            
            // Process license info response
            if (pendingResponses_.find(infoTransactionId) != pendingResponses_.end()) {
                const auto& infoResponse = pendingResponses_[infoTransactionId];
                if (infoResponse.contains("_extracted_success") && infoResponse["_extracted_success"].get<bool>()) {
                    std::string resultStr = infoResponse["_extracted_result"].get<std::string>();
                    try {
                        combinedData["license_info"] = json::parse(resultStr);
                    } catch (const json::parse_error&) {
                        combinedData["license_info"] = resultStr;
                    }
                } else {
                    std::string errorMsg = infoResponse.value("_extracted_error", "Unknown error");
                    LOG_LICENSE_CRON_ERROR("License info request failed (transaction: " + infoTransactionId + "): " + errorMsg);
                    LOG_LICENSE_CRON_ERROR("Full error response: " + infoResponse.dump());
                }
            }
            
            // Process license plan response
            if (pendingResponses_.find(planTransactionId) != pendingResponses_.end()) {
                const auto& planResponse = pendingResponses_[planTransactionId];
                if (planResponse.contains("_extracted_success") && planResponse["_extracted_success"].get<bool>()) {
                    std::string resultStr = planResponse["_extracted_result"].get<std::string>();
                    try {
                        combinedData["license_plan"] = json::parse(resultStr);
                    } catch (const json::parse_error&) {
                        combinedData["license_plan"] = resultStr;
                    }
                } else {
                    std::string errorMsg = planResponse.value("_extracted_error", "Unknown error");
                    LOG_LICENSE_CRON_ERROR("License plan request failed (transaction: " + planTransactionId + "): " + errorMsg);
                    LOG_LICENSE_CRON_ERROR("Full error response: " + planResponse.dump());
                }
            }
        }
        
        // Both requests processed successfully - no need to call processResponse
        // since individual responses were already processed above
        if (combinedData.contains("license_info") && combinedData.contains("license_plan")) {
            LOG_LICENSE_CRON_INFO("Both license info and plan data received successfully");
            
            // Compare with system database if needed
            if (compareWithSystemDatabase(combinedData)) {
                updateSystemDatabase(combinedData);
            } else {
                LOG_LICENSE_CRON_INFO("License data is consistent with system database, no update needed");
            }
        } else {
            LOG_LICENSE_CRON_WARN("Incomplete license data received - skipping database comparison");
        }
        
        LOG_LICENSE_CRON_INFO("JSON-RPC 2.0 license verification completed successfully");
        
        // Mark first job as completed and start periodic scheduling
        if (!firstJobCompleted_.exchange(true)) {
            LOG_LICENSE_CRON_INFO("First license verification job completed successfully");
            LOG_LICENSE_CRON_INFO("Switching to periodic scheduling (every 10 minutes)");
        }
        
    } catch (const std::exception& e) {
        LOG_LICENSE_CRON_ERROR("Error during JSON-RPC 2.0 license verification: " + std::string(e.what()));
    }
    
    // Clear verification flag and exit
    verificationInProgress_.store(false);
    
    // Clear pending responses
    {
        std::lock_guard<std::mutex> lock(responseMutex_);
        pendingResponses_.clear();
    }
    
    LOG_LICENSE_CRON_INFO("JSON-RPC 2.0 license verification thread exiting");
}

std::string LicenseSyncCronJob::sendLicenseInfoRequest() {
    try {
        LOG_LICENSE_CRON_INFO("Sending GET_LICENSE_INFO request...");

        // Generate unique transaction ID
        std::string transactionId = "license_sync_info_" + std::to_string(std::time(nullptr)) + "_" + std::to_string(std::rand());
        
        // Create JSON-RPC 2.0 request as ur-licence-mann expects
        json request = json::object();
        request["jsonrpc"] = "2.0";
        request["id"] = transactionId;
        request["method"] = "get_license_info";
        
        // Parameters object - license_file parameter is ignored by ur-licence-mann
        // ur-licence-mann uses predefined path: licenses_directory/system.lic
        json params = json::object();
        request["params"] = params;

        std::string requestStr = request.dump();
        
        // Send to ur-licence-mann request topic
        std::string requestTopic = "direct_messaging/ur-licence-mann/requests";
        
        if (direct_client_publish_raw_message(requestTopic.c_str(), 
                                              requestStr.c_str(), 
                                              requestStr.length()) == 0) {
            LOG_LICENSE_CRON_INFO("Sent JSON-RPC 2.0 license info request with transaction: " + transactionId);
            return transactionId;
        } else {
            LOG_LICENSE_CRON_ERROR("Failed to send JSON-RPC 2.0 license info request");
            return "";
        }

    } catch (const std::exception& e) {
        LOG_LICENSE_CRON_ERROR("Error sending JSON-RPC 2.0 license info request: " + std::string(e.what()));
        return "";
    }
}

std::string LicenseSyncCronJob::sendLicensePlanRequest() {
    try {
        LOG_LICENSE_CRON_INFO("Sending GET_LICENSE_PLAN request...");

        // Generate unique transaction ID
        std::string transactionId = "license_sync_plan_" + std::to_string(std::time(nullptr)) + "_" + std::to_string(std::rand());
        
        // Create JSON-RPC 2.0 request as ur-licence-mann expects
        json request = json::object();
        request["jsonrpc"] = "2.0";
        request["id"] = transactionId;
        request["method"] = "get_license_plan";
        
        // Parameters object - license_file parameter is ignored by ur-licence-mann
        // ur-licence-mann uses predefined path: licenses_directory/system.lic
        json params = json::object();
        request["params"] = params;

        std::string requestStr = request.dump();
        
        // Send to ur-licence-mann request topic
        std::string requestTopic = "direct_messaging/ur-licence-mann/requests";
        
        if (direct_client_publish_raw_message(requestTopic.c_str(), 
                                              requestStr.c_str(), 
                                              requestStr.length()) == 0) {
            LOG_LICENSE_CRON_INFO("Sent JSON-RPC 2.0 license plan request with transaction: " + transactionId);
            return transactionId;
        } else {
            LOG_LICENSE_CRON_ERROR("Failed to send JSON-RPC 2.0 license plan request");
            return "";
        }

    } catch (const std::exception& e) {
        LOG_LICENSE_CRON_ERROR("Error sending JSON-RPC 2.0 license plan request: " + std::string(e.what()));
        return "";
    }
}

bool LicenseSyncCronJob::waitForResponse(const std::string& transactionId, std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lock(responseMutex_);
    
    // Wait for response with timeout
    auto deadline = std::chrono::steady_clock::now() + timeout;
    
    if (responseCondition_.wait_until(lock, deadline, [&]() {
        return pendingResponses_.find(transactionId) != pendingResponses_.end() || !running_.load();
    })) {
        // Response received or job stopped
        if (!running_.load()) {
            LOG_LICENSE_CRON_INFO("Job stopped while waiting for response");
            return false;
        }
        
        LOG_LICENSE_CRON_INFO("Response received for transaction: " + transactionId);
        return true;
    } else {
        // Timeout occurred
        LOG_LICENSE_CRON_ERROR("Timeout waiting for response to transaction: " + transactionId);
        return false;
    }
}

void LicenseSyncCronJob::processResponse(const nlohmann::json& response) {
    try {
        LOG_LICENSE_CRON_INFO("Processing JSON-RPC 2.0 license verification response");
        
        // Extract structured data from JSON-RPC 2.0 response
        json combinedData = json::object();
        
        // Check if response has the expected JSON-RPC 2.0 structure
        if (response.contains("_extracted_success") && response["_extracted_success"].get<bool>()) {
            // Successful response - parse the result data
            if (response.contains("_extracted_result")) {
                std::string resultStr = response["_extracted_result"].get<std::string>();
                
                try {
                    // Try to parse the result as JSON (ur-licence-mann returns structured JSON for data operations)
                    json resultJson = json::parse(resultStr);
                    
                    // Determine the operation type based on transaction ID
                    std::string transactionId = response.value("id", "");
                    if (transactionId.find("license_sync_info_") == 0) {
                        combinedData["license_info"] = resultJson;
                        LOG_LICENSE_CRON_INFO("Received structured license info data");
                    } else if (transactionId.find("license_sync_plan_") == 0) {
                        combinedData["license_plan"] = resultJson;
                        LOG_LICENSE_CRON_INFO("Received structured license plan data");
                    }
                    
                } catch (const json::parse_error&) {
                    // Result is not valid JSON, treat as plain text
                    LOG_LICENSE_CRON_WARN("License response result is not valid JSON, treating as plain text");
                    
                    std::string transactionId = response.value("id", "");
                    if (transactionId.find("license_sync_info_") == 0) {
                        combinedData["license_info"] = resultStr;
                        LOG_LICENSE_CRON_INFO("Received license info as plain text");
                    } else if (transactionId.find("license_sync_plan_") == 0) {
                        combinedData["license_plan"] = resultStr;
                        LOG_LICENSE_CRON_INFO("Received license plan as plain text");
                    }
                }
            }
        } else {
            // Error response - extract detailed error information
            std::string transactionId = response.value("id", "unknown");
            std::string errorMsg = response.value("_extracted_error", "Unknown JSON-RPC 2.0 error");
            
            // Determine operation type for better error reporting
            std::string operationType = "unknown operation";
            if (transactionId.find("license_sync_info_") == 0) {
                operationType = "get_license_info";
            } else if (transactionId.find("license_sync_plan_") == 0) {
                operationType = "get_license_plan";
            }
            
            LOG_LICENSE_CRON_ERROR("JSON-RPC 2.0 operation failed for " + operationType + " (transaction: " + transactionId + "): " + errorMsg);
            return;
        }
        
        // Compare with system database if needed
        if (compareWithSystemDatabase(combinedData)) {
            updateSystemDatabase(combinedData);
        } else {
            LOG_LICENSE_CRON_INFO("License data is consistent with system database, no update needed");
        }
        
    } catch (const std::exception& e) {
        LOG_LICENSE_CRON_ERROR("Error processing JSON-RPC 2.0 license response: " + std::string(e.what()));
    }
}


bool LicenseSyncCronJob::compareWithSystemDatabase(const json& receivedData) {
    try {
        json licenseData = loadLicenseData();
        
        bool hasChanges = false;
        
        // Compare license info
        if (receivedData.contains("license_info")) {
            if (!licenseData.contains("license_info") || 
                licenseData["license_info"] != receivedData["license_info"]) {
                hasChanges = true;
                LOG_LICENSE_CRON_INFO("License info changes detected");
            }
        }
        
        // Compare license plan
        if (receivedData.contains("license_plan")) {
            if (!licenseData.contains("license_plan") || 
                licenseData["license_plan"] != receivedData["license_plan"]) {
                hasChanges = true;
                LOG_LICENSE_CRON_INFO("License plan changes detected");
            }
        }
        
        return hasChanges;

    } catch (const std::exception& e) {
        LOG_LICENSE_CRON_ERROR("Error comparing license data: " + std::string(e.what()));
        return false;
    }
}

void LicenseSyncCronJob::updateSystemDatabase(const json& receivedData) {
    try {
        // Save license data to SQLite database
        saveLicenseData(receivedData);
        
        LOG_LICENSE_CRON_INFO("License database updated successfully");

    } catch (const std::exception& e) {
        LOG_LICENSE_CRON_ERROR("Error updating license database: " + std::string(e.what()));
    }
}

bool LicenseSyncCronJob::initializeDatabase() {
    try {
        // Create data directory if it doesn't exist
        std::filesystem::path dbPath(systemDatabasePath_);
        std::filesystem::path dbDir = dbPath.parent_path();
        
        if (!std::filesystem::exists(dbDir)) {
            std::filesystem::create_directories(dbDir);
            LOG_LICENSE_CRON_INFO("Created database directory: " + dbDir.string());
        }
        
        // Open database connection
        int rc = sqlite3_open(systemDatabasePath_.c_str(), &dbConnection_);
        if (rc != SQLITE_OK) {
            LOG_LICENSE_CRON_ERROR("Cannot open database: " + std::string(sqlite3_errmsg(dbConnection_)));
            return false;
        }
        
        // Create license table if it doesn't exist
        const char* createTableSQL = R"(
            CREATE TABLE IF NOT EXISTS license (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                license_info TEXT NOT NULL DEFAULT '{}',
                license_plan TEXT NOT NULL DEFAULT '{}',
                last_updated INTEGER NOT NULL,
                sync_source TEXT NOT NULL DEFAULT 'ur-licence-mann'
            )
        )";
        
        char* errMsg = nullptr;
        rc = sqlite3_exec(dbConnection_, createTableSQL, nullptr, nullptr, &errMsg);
        if (rc != SQLITE_OK) {
            LOG_LICENSE_CRON_ERROR("SQL error: " + std::string(errMsg));
            sqlite3_free(errMsg);
            return false;
        }
        
        // Check if license table has data and insert default if empty
        json existingData = loadLicenseData();
        if (!existingData.contains("license_info") || existingData["license_info"].get<std::string>().empty() ||
            !existingData.contains("license_plan") || existingData["license_plan"].get<std::string>().empty()) {
            
            LOG_LICENSE_CRON_INFO("License table is empty, will be populated by first successful license sync");
        }
        
        LOG_LICENSE_CRON_INFO("Database initialized successfully");
        return true;
        
    } catch (const std::exception& e) {
        LOG_LICENSE_CRON_ERROR("Error initializing database: " + std::string(e.what()));
        return false;
    }
}

json LicenseSyncCronJob::loadLicenseData() {
    try {
        if (!dbConnection_) {
            LOG_LICENSE_CRON_ERROR("Database connection not initialized");
            return json::object();
        }
        
        const char* selectSQL = "SELECT license_info, license_plan FROM license ORDER BY id DESC LIMIT 1";
        sqlite3_stmt* stmt;
        
        int rc = sqlite3_prepare_v2(dbConnection_, selectSQL, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            LOG_LICENSE_CRON_ERROR("Failed to prepare statement: " + std::string(sqlite3_errmsg(dbConnection_)));
            return json::object();
        }
        
        json result = json::object();
        
        rc = sqlite3_step(stmt);
        if (rc == SQLITE_ROW) {
            const char* licenseInfo = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            const char* licensePlan = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            
            if (licenseInfo && strlen(licenseInfo) > 0) {
                try {
                    result["license_info"] = json::parse(licenseInfo);
                } catch (const json::parse_error&) {
                    result["license_info"] = licenseInfo;
                }
            }
            
            if (licensePlan && strlen(licensePlan) > 0) {
                try {
                    result["license_plan"] = json::parse(licensePlan);
                } catch (const json::parse_error&) {
                    result["license_plan"] = licensePlan;
                }
            }
        }
        
        sqlite3_finalize(stmt);
        return result;
        
    } catch (const std::exception& e) {
        LOG_LICENSE_CRON_ERROR("Error loading license data: " + std::string(e.what()));
        return json::object();
    }
}

void LicenseSyncCronJob::saveLicenseData(const json& licenseData) {
    try {
        if (!dbConnection_) {
            LOG_LICENSE_CRON_ERROR("Database connection not initialized");
            return;
        }
        
        std::string licenseInfoStr = "{}";
        std::string licensePlanStr = "{}";
        
        if (licenseData.contains("license_info")) {
            licenseInfoStr = licenseData["license_info"].dump();
        }
        
        if (licenseData.contains("license_plan")) {
            licensePlanStr = licenseData["license_plan"].dump();
        }
        
        // Insert or replace license data
        const char* insertSQL = R"(
            INSERT OR REPLACE INTO license (id, license_info, license_plan, last_updated, sync_source)
            VALUES (1, ?, ?, ?, 'ur-licence-mann')
        )";
        
        sqlite3_stmt* stmt;
        int rc = sqlite3_prepare_v2(dbConnection_, insertSQL, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            LOG_LICENSE_CRON_ERROR("Failed to prepare insert statement: " + std::string(sqlite3_errmsg(dbConnection_)));
            return;
        }
        
        sqlite3_bind_text(stmt, 1, licenseInfoStr.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, licensePlanStr.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int64(stmt, 3, std::time(nullptr));
        
        rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE) {
            LOG_LICENSE_CRON_ERROR("Failed to save license data: " + std::string(sqlite3_errmsg(dbConnection_)));
        } else {
            LOG_LICENSE_CRON_INFO("License data saved to database successfully");
        }
        
        sqlite3_finalize(stmt);
        
    } catch (const std::exception& e) {
        LOG_LICENSE_CRON_ERROR("Error saving license data: " + std::string(e.what()));
    }
}

void LicenseSyncCronJob::startPeriodicScheduling() {
    try {
        if (periodicSchedulingRunning_.exchange(true)) {
            LOG_LICENSE_CRON_INFO("Periodic scheduling already running");
            return;
        }
        
        LOG_LICENSE_CRON_INFO("Starting periodic scheduling thread");
        periodicSchedulingThread_ = std::thread(&LicenseSyncCronJob::periodicSchedulingThread, this);
        
    } catch (const std::exception& e) {
        LOG_LICENSE_CRON_ERROR("Error starting periodic scheduling: " + std::string(e.what()));
        periodicSchedulingRunning_.store(false);
    }
}

void LicenseSyncCronJob::stopPeriodicScheduling() {
    try {
        if (!periodicSchedulingRunning_.exchange(false)) {
            return; // Already stopped
        }
        
        LOG_LICENSE_CRON_INFO("Stopping periodic scheduling thread");
        periodicSchedulingCondition_.notify_all();
        
        if (periodicSchedulingThread_.joinable()) {
            periodicSchedulingThread_.join();
        }
        
        LOG_LICENSE_CRON_INFO("Periodic scheduling thread stopped");
        
    } catch (const std::exception& e) {
        LOG_LICENSE_CRON_ERROR("Error stopping periodic scheduling: " + std::string(e.what()));
    }
}

void LicenseSyncCronJob::periodicSchedulingThread() {
    LOG_LICENSE_CRON_INFO("Periodic scheduling thread started");
    
    while (periodicSchedulingRunning_.load() && running_.load()) {
        // Wait for first job to complete or timeout
        std::unique_lock<std::mutex> lock(periodicSchedulingMutex_);
        
        if (firstJobCompleted_.load()) {
            // First job completed, run every 10 minutes
            if (periodicSchedulingCondition_.wait_for(lock, std::chrono::minutes(PERIODIC_INTERVAL_MINUTES), [this]() {
                return !periodicSchedulingRunning_.load() || !running_.load();
            })) {
                break; // Stop requested
            }
            
            if (periodicSchedulingRunning_.load() && running_.load()) {
                LOG_LICENSE_CRON_INFO("Periodic license verification triggered");
                
                // Run license verification in separate thread
                threadManager_->createThread([this]() {
                    this->performLicenseVerification();
                });
            }
        } else {
            // Wait for first job to complete
            if (periodicSchedulingCondition_.wait_for(lock, std::chrono::seconds(30), [this]() {
                return !periodicSchedulingRunning_.load() || !running_.load() || firstJobCompleted_.load();
            })) {
                break; // Stop requested
            }
        }
    }
    
    LOG_LICENSE_CRON_INFO("Periodic scheduling thread exiting");
}

json LicenseSyncCronJob::loadSystemDatabase() {
    try {
        std::ifstream dbFile(systemDatabasePath_);
        if (!dbFile.is_open()) {
            // Create empty database if it doesn't exist
            json emptyDb = json::object();
            emptyDb["created"] = std::time(nullptr);
            saveSystemDatabase(emptyDb);
            return emptyDb;
        }
        
        json data = json::parse(dbFile);
        dbFile.close();
        return data;

    } catch (const std::exception& e) {
        LOG_LICENSE_CRON_ERROR("Error loading system database: " + std::string(e.what()));
        return json::object();
    }
}

void LicenseSyncCronJob::saveSystemDatabase(const json& data) {
    try {
        std::ofstream dbFile(systemDatabasePath_);
        if (dbFile.is_open()) {
            dbFile << data.dump(4);
            dbFile.close();
        } else {
            LOG_LICENSE_CRON_ERROR("Failed to open system database for writing");
        }

    } catch (const std::exception& e) {
        LOG_LICENSE_CRON_ERROR("Error saving system database: " + std::string(e.what()));
    }
}
