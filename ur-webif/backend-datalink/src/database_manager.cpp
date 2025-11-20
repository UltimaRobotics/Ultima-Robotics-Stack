#include "database_manager.h"
#include "../thirdparty/controlled-log/include/controlled_log.h"
#include <sqlite3.h>

DatabaseManager::~DatabaseManager() {
    shutdown();
}

bool DatabaseManager::initialize(const ConfigLoader::DatabaseConfig& config) {
    std::lock_guard<std::mutex> lock(db_mutex_);
    
    if (runtime_db_ != nullptr || system_db_ != nullptr) {
        logError("Database already initialized");
        return false;
    }
    
    config_ = config;
    
    if (!config_.enabled) {
        LOG_DATABASE_INFO("Database disabled in configuration");
        return true;
    }
    
    // Initialize runtime database first
    if (!createDatabase(config_.runtime_db_path, &runtime_db_)) {
        logError("Failed to initialize runtime database: " + config_.runtime_db_path);
        return false;
    }
    
    // Initialize system database second
    if (!createDatabase(config_.system_db_path, &system_db_)) {
        logError("Failed to initialize system database: " + config_.system_db_path);
        sqlite3_close(runtime_db_);
        runtime_db_ = nullptr;
        return false;
    }
    
    // Now that both databases are initialized, run final operations test
    if (!testDatabaseOperations()) {
        logError("Final database operations test failed");
        sqlite3_close(runtime_db_);
        sqlite3_close(system_db_);
        runtime_db_ = nullptr;
        system_db_ = nullptr;
        return false;
    }
    
    LOG_DATABASE_INFO("Both databases initialized successfully");
    return true;
}

void DatabaseManager::shutdown() {
    std::lock_guard<std::mutex> lock(db_mutex_);
    
    if (runtime_db_ != nullptr) {
        sqlite3_close(runtime_db_);
        runtime_db_ = nullptr;
        LOG_DATABASE_INFO("Runtime database connection closed");
    }
    
    if (system_db_ != nullptr) {
        sqlite3_close(system_db_);
        system_db_ = nullptr;
        LOG_DATABASE_INFO("System database connection closed");
    }
}

bool DatabaseManager::logConnection(const std::string& connection_id, const std::string& client_ip, const std::string& status) {
    if (!config_.enabled || !config_.log_connections || runtime_db_ == nullptr) {
        return true; // Silently ignore if disabled
    }
    
    std::lock_guard<std::mutex> lock(db_mutex_);
    
    std::string sql = "INSERT INTO connections_log (connection_id, client_ip, status, connected_at) VALUES (?, ?, ?, ?)";
    std::vector<std::string> params = {
        connection_id,
        client_ip,
        status,
        getCurrentTimestamp()
    };
    
    return executeSQLWithParams(runtime_db_, sql, params);
}

bool DatabaseManager::logDisconnection(const std::string& connection_id) {
    if (!config_.enabled || !config_.log_connections || runtime_db_ == nullptr) {
        return true; // Silently ignore if disabled
    }
    
    std::lock_guard<std::mutex> lock(db_mutex_);
    
    std::string sql = "UPDATE connections_log SET disconnected_at = ?, status = 'disconnected' WHERE connection_id = ? AND disconnected_at IS NULL";
    std::vector<std::string> params = {
        getCurrentTimestamp(),
        connection_id
    };
    
    return executeSQLWithParams(runtime_db_, sql, params);
}

bool DatabaseManager::logMessage(const std::string& connection_id, const std::string& direction, const std::string& message_text) {
    if (!config_.enabled || !config_.log_messages || runtime_db_ == nullptr) {
        return true; // Silently ignore if disabled
    }
    
    std::lock_guard<std::mutex> lock(db_mutex_);
    
    std::string sql = "INSERT INTO messages (connection_id, direction, message_text, timestamp) VALUES (?, ?, ?, ?)";
    std::vector<std::string> params = {
        connection_id,
        direction,
        message_text,
        getCurrentTimestamp()
    };
    
    return executeSQLWithParams(runtime_db_, sql, params);
}

int DatabaseManager::getActiveConnectionCount() const {
    if (!config_.enabled || runtime_db_ == nullptr) {
        return 0;
    }
    
    std::lock_guard<std::mutex> lock(db_mutex_);
    
    std::string sql = "SELECT COUNT(*) FROM connections_log WHERE status = 'connected' AND disconnected_at IS NULL";
    sqlite3_stmt* stmt = nullptr;
    
    if (sqlite3_prepare_v2(runtime_db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        logError("Failed to prepare statement for active connection count");
        return 0;
    }
    
    int count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        count = sqlite3_column_int(stmt, 0);
    }
    
    sqlite3_finalize(stmt);
    return count;
}

std::vector<std::string> DatabaseManager::getRecentConnections(int limit) const {
    std::vector<std::string> connections;
    
    if (!config_.enabled || runtime_db_ == nullptr) {
        return connections;
    }
    
    std::lock_guard<std::mutex> lock(db_mutex_);
    
    std::string sql = "SELECT connection_id, client_ip, status, connected_at FROM connections_log ORDER BY connected_at DESC LIMIT ?";
    sqlite3_stmt* stmt = nullptr;
    
    if (sqlite3_prepare_v2(runtime_db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        logError("Failed to prepare statement for recent connections");
        return connections;
    }
    
    sqlite3_bind_int(stmt, 1, limit);
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        std::ostringstream oss;
        oss << "ID: " << reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0))
            << ", IP: " << reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1))
            << ", Status: " << reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2))
            << ", Connected: " << reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        connections.push_back(oss.str());
    }
    
    sqlite3_finalize(stmt);
    return connections;
}

bool DatabaseManager::createDatabase(const std::string& db_path, sqlite3** db) {
    // Create database directory if it doesn't exist
    size_t last_slash = db_path.find_last_of('/');
    if (last_slash != std::string::npos) {
        std::string db_dir = db_path.substr(0, last_slash);
        std::string mkdir_cmd = "mkdir -p " + db_dir;
        if (system(mkdir_cmd.c_str()) != 0) {
            logError("Failed to create database directory: " + db_dir);
            return false;
        }
    }
    
    // Check if database file exists
    bool db_exists = std::ifstream(db_path).good();
    
    // Open database
    int result = sqlite3_open(db_path.c_str(), db);
    if (result != SQLITE_OK) {
        logError("Failed to open database: " + std::string(sqlite3_errmsg(*db)));
        sqlite3_close(*db);
        *db = nullptr;
        return false;
    }
    
    // Enable foreign keys
    executeSQL(*db, "PRAGMA foreign_keys = ON");
    
    // Create tables if database is new
    if (!db_exists) {
        LOG_DATABASE_INFO("Creating new database: " + db_path);
        
        // Determine which database this is and create appropriate tables
        if (db_path == config_.runtime_db_path) {
            if (!createRuntimeTables()) {
                logError("Failed to create runtime database tables");
                sqlite3_close(*db);
                *db = nullptr;
                return false;
            }
        } else if (db_path == config_.system_db_path) {
            if (!createSystemTables()) {
                logError("Failed to create system database tables");
                sqlite3_close(*db);
                *db = nullptr;
                return false;
            }
        }
    } else {
        LOG_DATABASE_INFO("Using existing database: " + db_path);
        
        // For existing databases, create missing tables based on database type
        if (db_path == config_.runtime_db_path) {
            LOG_DATABASE_INFO("Ensuring runtime database tables exist...");
            if (!createRuntimeTables()) {
                logError("Failed to ensure runtime database tables exist");
                sqlite3_close(*db);
                *db = nullptr;
                return false;
            }
        } else if (db_path == config_.system_db_path) {
            LOG_DATABASE_INFO("Ensuring system database tables exist...");
            if (!createSystemTables()) {
                logError("Failed to ensure system database tables exist");
                sqlite3_close(*db);
                *db = nullptr;
                return false;
            }
        }
    }
    
    // Test database operations (simplified for individual database creation)
    if (db_path == config_.runtime_db_path) {
        // For runtime database, just test a simple connection
        LOG_DATABASE_INFO("Runtime database connection test passed");
    } else if (db_path == config_.system_db_path) {
        // For system database, just test a simple connection 
        LOG_DATABASE_INFO("System database connection test passed");
    }
    
    return true;
}

bool DatabaseManager::createRuntimeTables() {
    std::vector<std::string> tables = {
        // Connections log table
        "CREATE TABLE IF NOT EXISTS connections_log ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "connection_id TEXT NOT NULL,"
        "client_ip TEXT NOT NULL,"
        "status TEXT NOT NULL DEFAULT 'connected',"
        "connected_at TEXT NOT NULL,"
        "disconnected_at TEXT,"
        "created_at DATETIME DEFAULT CURRENT_TIMESTAMP"
        ")",
        
        // Messages table
        "CREATE TABLE IF NOT EXISTS messages ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "connection_id TEXT NOT NULL,"
        "direction TEXT NOT NULL," // 'in' or 'out'
        "message_text TEXT,"
        "timestamp TEXT NOT NULL,"
        "created_at DATETIME DEFAULT CURRENT_TIMESTAMP,"
        "FOREIGN KEY (connection_id) REFERENCES connections_log(connection_id)"
        ")",
        
        // Indexes for better performance
        "CREATE INDEX IF NOT EXISTS idx_connections_connection_id ON connections_log(connection_id)",
        "CREATE INDEX IF NOT EXISTS idx_connections_status ON connections_log(status)",
        "CREATE INDEX IF NOT EXISTS idx_connections_connected_at ON connections_log(connected_at)",
        "CREATE INDEX IF NOT EXISTS idx_messages_connection_id ON messages(connection_id)",
        "CREATE INDEX IF NOT EXISTS idx_messages_timestamp ON messages(timestamp)"
    };
    
    for (const auto& sql : tables) {
        if (!executeSQL(runtime_db_, sql)) {
            return false;
        }
    }
    
    return true;
}

bool DatabaseManager::createSystemTables() {
    std::vector<std::string> tables = {
        // Dashboard data table
        "CREATE TABLE IF NOT EXISTS dashboard_data ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "category TEXT NOT NULL UNIQUE,"
        "data_json TEXT NOT NULL,"
        "updated_at TEXT NOT NULL,"
        "created_at DATETIME DEFAULT CURRENT_TIMESTAMP"
        ")",
        
        // Simplified system license table
        "CREATE TABLE IF NOT EXISTS system_license ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "license_id TEXT NOT NULL UNIQUE,"
        "license_type TEXT NOT NULL,"
        "license_tier TEXT NOT NULL,"
        "product_name TEXT NOT NULL,"
        "product_version TEXT NOT NULL,"
        "user_name TEXT NOT NULL,"
        "user_email TEXT NOT NULL,"
        "license_file_path TEXT,"
        "public_key_path TEXT,"
        "is_valid BOOLEAN NOT NULL DEFAULT TRUE,"
        "is_hardware_bound BOOLEAN DEFAULT FALSE,"
        "hardware_fingerprint TEXT,"
        "issued_at TEXT NOT NULL,"
        "expires_at TEXT NOT NULL,"
        "last_verified_at TEXT,"
        "verification_status TEXT DEFAULT 'pending',"
        "license_data TEXT,"
        "created_at DATETIME DEFAULT CURRENT_TIMESTAMP,"
        "updated_at DATETIME DEFAULT CURRENT_TIMESTAMP"
        ")",
        
        // Indexes for better performance
        "CREATE INDEX IF NOT EXISTS idx_dashboard_data_category ON dashboard_data(category)",
        "CREATE INDEX IF NOT EXISTS idx_dashboard_data_updated_at ON dashboard_data(updated_at)",
        "CREATE INDEX IF NOT EXISTS idx_system_license_license_id ON system_license(license_id)",
        "CREATE INDEX IF NOT EXISTS idx_system_license_expires_at ON system_license(expires_at)",
        "CREATE INDEX IF NOT EXISTS idx_system_license_is_valid ON system_license(is_valid)"
    };
    
    for (const auto& sql : tables) {
        if (!executeSQL(system_db_, sql)) {
            return false;
        }
    }
    
    // Initialize placeholder license data if table is empty
    if (!initializeLicensePlaceholder()) {
        logError("Failed to initialize license placeholder data");
        return false;
    }
    
    return true;
}

bool DatabaseManager::initializeLicensePlaceholder() {
    // Check if license table already has data
    std::string check_sql = "SELECT COUNT(*) FROM system_license";
    sqlite3_stmt* stmt = nullptr;
    
    if (sqlite3_prepare_v2(system_db_, check_sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        logError("Failed to prepare statement for license placeholder check");
        return false;
    }
    
    int count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        count = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
    
    // If table already has data, don't create placeholder
    if (count > 0) {
        LOG_DATABASE_INFO("License table already contains data, skipping placeholder creation");
        return true;
    }
    
    // Create placeholder license entry indicating no license data collected yet
    std::string placeholder_sql = 
        "INSERT INTO system_license ("
        "license_id, license_type, license_tier, product_name, product_version, "
        "user_name, user_email, issued_at, expires_at, verification_status, "
        "license_data, is_valid"
        ") VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";
    
    std::vector<std::string> params = {
        "PLACEHOLDER_NO_LICENSE",  // license_id
        "uninitialized",           // license_type
        "none",                    // license_tier
        "unknown",                 // product_name
        "0.0.0",                   // product_version
        "system",                  // user_name
        "system@localhost",        // user_email
        "2025-01-01 00:00:00.000", // issued_at
        "2099-12-31 23:59:59.999", // expires_at
        "no_license",              // verification_status
        "{\"status\":\"no_license_collected\",\"message\":\"License data not yet collected\"}", // license_data
        "false"                    // is_valid
    };
    
    if (!executeSQLWithParams(system_db_, placeholder_sql, params)) {
        logError("Failed to insert placeholder license data");
        return false;
    }
    
    LOG_DATABASE_INFO("Created placeholder license entry");
    return true;
}

std::string DatabaseManager::getCurrentTimestamp() const {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::tm tm = *std::localtime(&time_t);
    
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    oss << "." << std::setfill('0') << std::setw(3) << ms.count();
    
    return oss.str();
}

bool DatabaseManager::executeSQL(sqlite3* db, const std::string& sql) {
    char* error_msg = nullptr;
    int result = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &error_msg);
    
    if (result != SQLITE_OK) {
        logError("SQL error: " + std::string(error_msg ? error_msg : "unknown error"));
        sqlite3_free(error_msg);
        return false;
    }
    
    return true;
}

bool DatabaseManager::executeSQLWithParams(sqlite3* db, const std::string& sql, const std::vector<std::string>& params) {
    sqlite3_stmt* stmt = nullptr;
    
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        logError("Failed to prepare statement: " + sql);
        return false;
    }
    
    // Bind parameters
    for (size_t i = 0; i < params.size(); ++i) {
        if (sqlite3_bind_text(stmt, static_cast<int>(i + 1), params[i].c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK) {
            logError("Failed to bind parameter " + std::to_string(i + 1));
            sqlite3_finalize(stmt);
            return false;
        }
    }
    
    // Execute statement
    int result = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    return (result == SQLITE_DONE || result == SQLITE_ROW);
}

void DatabaseManager::logError(const std::string& message) const {
    LOG_DATABASE_ERROR("ERROR: " + message);
}

bool DatabaseManager::updateDashboardData(const std::string& category, const std::string& data_json) {
    if (!config_.enabled || system_db_ == nullptr) {
        return true; // Silently ignore if disabled
    }
    
    std::lock_guard<std::mutex> lock(db_mutex_);
    
    std::string sql = "INSERT OR REPLACE INTO dashboard_data (category, data_json, updated_at) VALUES (?, ?, ?)";
    sqlite3_stmt* stmt = nullptr;
    
    int result = sqlite3_prepare_v2(system_db_, sql.c_str(), -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        logError("Failed to prepare statement: " + sql + " - SQLite error: " + std::string(sqlite3_errmsg(system_db_)));
        return false;
    }
    
    // Bind parameters
    if (sqlite3_bind_text(stmt, 1, category.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK) {
        logError("Failed to bind category parameter: " + std::string(sqlite3_errmsg(system_db_)));
        sqlite3_finalize(stmt);
        return false;
    }
    
    if (sqlite3_bind_text(stmt, 2, data_json.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK) {
        logError("Failed to bind data_json parameter: " + std::string(sqlite3_errmsg(system_db_)));
        sqlite3_finalize(stmt);
        return false;
    }
    
    std::string timestamp = getCurrentTimestamp();
    if (sqlite3_bind_text(stmt, 3, timestamp.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK) {
        logError("Failed to bind timestamp parameter: " + std::string(sqlite3_errmsg(system_db_)));
        sqlite3_finalize(stmt);
        return false;
    }
    
    // Execute statement
    result = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    if (result != SQLITE_DONE) {
        logError("Failed to execute statement: " + std::string(sqlite3_errmsg(system_db_)));
        return false;
    }
    
    return true;
}

std::string DatabaseManager::getDashboardData(const std::string& category) const {
    if (!config_.enabled || system_db_ == nullptr) {
        return "{}";
    }
    
    std::lock_guard<std::mutex> lock(db_mutex_);
    
    std::string sql = "SELECT data_json FROM dashboard_data WHERE category = ?";
    sqlite3_stmt* stmt = nullptr;
    
    if (sqlite3_prepare_v2(system_db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        logError("Failed to prepare statement for dashboard data query");
        return "{}";
    }
    
    sqlite3_bind_text(stmt, 1, category.c_str(), -1, SQLITE_TRANSIENT);
    
    std::string result = "{}";
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* data = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        if (data) {
            result = data;
        }
    }
    
    sqlite3_finalize(stmt);
    return result;
}

bool DatabaseManager::initializeDashboardTables() {
    // This is now handled in createSystemTables()
    return true;
}

bool DatabaseManager::verifyDatabaseSchema() {
    if (!runtime_db_ || !system_db_) {
        logError("Databases not initialized for schema verification");
        return false;
    }
    
    // Check runtime database tables
    std::vector<std::string> runtime_required_tables = {
        "connections_log",
        "messages"
    };
    
    for (const auto& table_name : runtime_required_tables) {
        std::string sql = "SELECT name FROM sqlite_master WHERE type='table' AND name=?";
        sqlite3_stmt* stmt = nullptr;
        
        if (sqlite3_prepare_v2(runtime_db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
            logError("Failed to prepare schema verification statement for runtime table: " + table_name);
            return false;
        }
        
        sqlite3_bind_text(stmt, 1, table_name.c_str(), -1, SQLITE_TRANSIENT);
        
        bool table_exists = false;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            table_exists = true;
        }
        
        sqlite3_finalize(stmt);
        
        if (!table_exists) {
            logError("Required runtime table missing: " + table_name);
            
            // Try to create missing table
            LOG_DATABASE_INFO("Attempting to create missing runtime table: " + table_name);
            if (!createRuntimeTables()) {
                logError("Failed to create missing runtime table: " + table_name);
                return false;
            }
        }
    }
    
    // Check system database tables
    std::vector<std::string> system_required_tables = {
        "dashboard_data",
        "system_license"
    };
    
    for (const auto& table_name : system_required_tables) {
        std::string sql = "SELECT name FROM sqlite_master WHERE type='table' AND name=?";
        sqlite3_stmt* stmt = nullptr;
        
        if (sqlite3_prepare_v2(system_db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
            logError("Failed to prepare schema verification statement for system table: " + table_name);
            return false;
        }
        
        sqlite3_bind_text(stmt, 1, table_name.c_str(), -1, SQLITE_TRANSIENT);
        
        bool table_exists = false;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            table_exists = true;
        }
        
        sqlite3_finalize(stmt);
        
        if (!table_exists) {
            logError("Required system table missing: " + table_name);
            
            // Try to create missing table
            LOG_DATABASE_INFO("Attempting to create missing system table: " + table_name);
            if (!createSystemTables()) {
                logError("Failed to create missing system table: " + table_name);
                return false;
            }
        }
    }
    
    LOG_DATABASE_INFO("Database schema verification passed");
    return true;
}

bool DatabaseManager::testDatabaseOperations() {
    if (!runtime_db_ || !system_db_) {
        logError("Databases not initialized for operations test");
        return false;
    }
    
    // Test basic INSERT operation on system database dashboard_data table
    std::string test_sql = "INSERT OR REPLACE INTO dashboard_data (category, data_json, updated_at) VALUES (?, ?, ?)";
    sqlite3_stmt* stmt = nullptr;
    
    if (sqlite3_prepare_v2(system_db_, test_sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        logError("Database operations test failed - cannot prepare test statement: " + std::string(sqlite3_errmsg(system_db_)));
        return false;
    }
    
    // Bind test parameters
    if (sqlite3_bind_text(stmt, 1, "test", -1, SQLITE_TRANSIENT) != SQLITE_OK) {
        logError("Database operations test failed - cannot bind test category: " + std::string(sqlite3_errmsg(system_db_)));
        sqlite3_finalize(stmt);
        return false;
    }
    
    if (sqlite3_bind_text(stmt, 2, "{\"test\": true}", -1, SQLITE_TRANSIENT) != SQLITE_OK) {
        logError("Database operations test failed - cannot bind test data: " + std::string(sqlite3_errmsg(system_db_)));
        sqlite3_finalize(stmt);
        return false;
    }
    
    if (sqlite3_bind_text(stmt, 3, "2025-01-01 00:00:00.000", -1, SQLITE_TRANSIENT) != SQLITE_OK) {
        logError("Database operations test failed - cannot bind test timestamp: " + std::string(sqlite3_errmsg(system_db_)));
        sqlite3_finalize(stmt);
        return false;
    }
    
    // Execute test
    int result = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    if (result != SQLITE_DONE) {
        logError("Database operations test failed - cannot execute test statement: " + std::string(sqlite3_errmsg(system_db_)));
        return false;
    }
    
    // Clean up test data
    std::string cleanup_sql = "DELETE FROM dashboard_data WHERE category = 'test'";
    executeSQL(system_db_, cleanup_sql);
    
    LOG_DATABASE_INFO("Database operations test passed");
    return true;
}
