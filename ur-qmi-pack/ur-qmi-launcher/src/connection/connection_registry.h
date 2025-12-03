#ifndef CONNECTION_REGISTRY_H
#define CONNECTION_REGISTRY_H

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <mutex>
#include <atomic>
#include <chrono>
#include <fstream>
#include <sstream>

struct ConnectionReference {
    std::string connection_id;
    std::string device_path;
    std::string interface_name;
    std::string apn;
    pid_t process_id;
    std::chrono::system_clock::time_point start_time;
    bool is_active;
    uint32_t qmi_connection_id;
    std::string packet_data_handle;

    ConnectionReference() : process_id(0), is_active(false), qmi_connection_id(0) {
        start_time = std::chrono::system_clock::now();
    }

    std::string generateConnectionId() const {
        // Generate unique connection ID based on device path and timestamp
        auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(
            start_time.time_since_epoch()).count();
        std::string device_name = device_path;
        size_t pos = device_name.find_last_of('/');
        if (pos != std::string::npos) {
            device_name = device_name.substr(pos + 1);
        }
        return device_name + "_" + std::to_string(timestamp);
    }
};

class ConnectionRegistry {
private:
    static std::mutex registry_mutex_;
    static std::map<std::string, ConnectionReference> active_connections_;
    static std::string registry_file_path_;

    static bool loadRegistryFromFile();
    static bool saveRegistryToFile();
    static void cleanupStaleConnections();

public:
    // Registry management
    static bool registerConnection(const ConnectionReference& connection);
    static bool unregisterConnection(const std::string& connection_id);
    static bool updateConnection(const std::string& connection_id, const ConnectionReference& connection);

    // Connection operations
    static bool killConnection(const std::string& connection_ref);
    static bool killAllConnections();
    static std::vector<ConnectionReference> listActiveConnections();
    static ConnectionReference getConnectionStatus(const std::string& connection_ref);

    // Utility functions
    static std::string createConnectionReference(const std::string& device_path,
                                               const std::string& interface_name,
                                               const std::string& apn);
    static bool isConnectionActive(const std::string& connection_id);
    static bool processExists(pid_t pid);
    static std::vector<std::string> findConnectionsByPattern(const std::string& pattern);

    // Registry initialization and cleanup
    static void initialize(const std::string& registry_file = "/tmp/qmi_connections.registry");
    static void cleanup();

    // Signal handling
    static void handleTerminationSignal(const std::string& connection_id);
    static void handleGlobalTermination(); // Added for global signal handling
    static void performEmergencyWWANCleanup(); // Emergency cleanup when registry is empty

    // Display functions
    static void printConnectionsList();
    static void printConnectionStatus(const std::string& connection_ref);
    static std::string formatConnectionInfo(const ConnectionReference& connection);

    // Helper methods for internal operations
    static ConnectionReference getConnection(const std::string& connection_id);
    static std::vector<ConnectionReference> getActiveConnections();
    static void removeConnection(const std::string& connection_id);
    static void attemptGenericCleanup(const std::string& connection_id);
};

// Connection lifecycle management
class ConnectionLifecycleManager {
private:
    std::string connection_id_;
    bool registered_;

public:
    ConnectionLifecycleManager(const std::string& device_path,
                              const std::string& interface_name,
                              const std::string& apn);
    ~ConnectionLifecycleManager();

    bool registerConnection(uint32_t qmi_connection_id, const std::string& packet_data_handle);
    bool updateStatus(bool is_active);
    bool deregisterConnection(const std::string& interface_name = "");
    void deregisterAllConnections();
    std::string getConnectionId() const { return connection_id_; }
    bool isRegistered() const { return registered_; }

    // Static helper for current process
    static void setupSignalHandlers(const std::string& connection_id);
};

#endif // CONNECTION_REGISTRY_H