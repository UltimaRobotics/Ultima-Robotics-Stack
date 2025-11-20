#include "connection_registry.h"
#include "command_logger.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <signal.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ctime>
#include <iomanip>
#include <atomic>
#include <exception>

// Static member definitions
std::mutex ConnectionRegistry::registry_mutex_;
std::map<std::string, ConnectionReference> ConnectionRegistry::active_connections_;
std::string ConnectionRegistry::registry_file_path_ = "/tmp/qmi_connections.registry";

bool ConnectionRegistry::loadRegistryFromFile() {
    std::ifstream file(registry_file_path_);
    if (!file.is_open()) {
        return false; // File doesn't exist yet, that's okay
    }

    active_connections_.clear();
    std::string line;

    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;

        std::istringstream iss(line);
        ConnectionReference connection;
        std::string start_time_str;

        if (std::getline(iss, connection.connection_id, '|') &&
            std::getline(iss, connection.device_path, '|') &&
            std::getline(iss, connection.interface_name, '|') &&
            std::getline(iss, connection.apn, '|') &&
            iss >> connection.process_id >> connection.is_active >> 
                   connection.qmi_connection_id >> start_time_str) {

            // Parse timestamp
            std::tm tm = {};
            std::istringstream time_stream(start_time_str);
            time_stream >> std::get_time(&tm, "%Y-%m-%d_%H:%M:%S");
            connection.start_time = std::chrono::system_clock::from_time_t(std::mktime(&tm));

            active_connections_[connection.connection_id] = connection;
        }
    }

    file.close();
    cleanupStaleConnections();
    return true;
}

bool ConnectionRegistry::saveRegistryToFile() {
    std::ofstream file(registry_file_path_);
    if (!file.is_open()) {
        std::cerr << "Error: Cannot write to registry file " << registry_file_path_ << std::endl;
        return false;
    }

    file << "# QMI Connection Registry\n";
    file << "# Format: connection_id|device_path|interface_name|apn|process_id|is_active|qmi_connection_id|start_time\n";

    for (const auto& entry : active_connections_) {
        const ConnectionReference& connection = entry.second;

        // Format timestamp
        auto time_t = std::chrono::system_clock::to_time_t(connection.start_time);
        std::tm* tm = std::localtime(&time_t);

        file << connection.connection_id << "|"
             << connection.device_path << "|"
             << connection.interface_name << "|"
             << connection.apn << "|"
             << connection.process_id << "|"
             << (connection.is_active ? 1 : 0) << "|"
             << connection.qmi_connection_id << "|"
             << std::put_time(tm, "%Y-%m-%d_%H:%M:%S") << "\n";
    }

    file.close();
    return true;
}

void ConnectionRegistry::cleanupStaleConnections() {
    auto it = active_connections_.begin();
    while (it != active_connections_.end()) {
        if (!processExists(it->second.process_id)) {
            std::cout << "Removing stale connection: " << it->first 
                     << " (process " << it->second.process_id << " no longer exists)" << std::endl;
            it = active_connections_.erase(it);
        } else {
            ++it;
        }
    }
}

bool ConnectionRegistry::registerConnection(const ConnectionReference& connection) {
    std::lock_guard<std::mutex> lock(registry_mutex_);

    loadRegistryFromFile();

    ConnectionReference new_connection = connection;
    if (new_connection.connection_id.empty()) {
        new_connection.connection_id = new_connection.generateConnectionId();
    }
    new_connection.process_id = getpid();
    new_connection.start_time = std::chrono::system_clock::now();

    active_connections_[new_connection.connection_id] = new_connection;

    std::cout << "Registered connection: " << new_connection.connection_id 
              << " (PID: " << new_connection.process_id << ")" << std::endl;

    return saveRegistryToFile();
}

bool ConnectionRegistry::unregisterConnection(const std::string& connection_id) {
    std::lock_guard<std::mutex> lock(registry_mutex_);

    loadRegistryFromFile();

    auto it = active_connections_.find(connection_id);
    if (it != active_connections_.end()) {
        std::cout << "Unregistering connection: " << connection_id << std::endl;
        active_connections_.erase(it);
        return saveRegistryToFile();
    }

    return false;
}

bool ConnectionRegistry::updateConnection(const std::string& connection_id, const ConnectionReference& connection) {
    std::lock_guard<std::mutex> lock(registry_mutex_);

    loadRegistryFromFile();

    auto it = active_connections_.find(connection_id);
    if (it != active_connections_.end()) {
        // Preserve original registration info
        ConnectionReference updated = connection;
        updated.connection_id = it->second.connection_id;
        updated.process_id = it->second.process_id;
        updated.start_time = it->second.start_time;

        active_connections_[connection_id] = updated;
        return saveRegistryToFile();
    }

    return false;
}

bool ConnectionRegistry::killConnection(const std::string& connection_ref) {
    std::lock_guard<std::mutex> lock(registry_mutex_);

    loadRegistryFromFile();

    // Try to find connection by exact ID first
    auto it = active_connections_.find(connection_ref);
    if (it == active_connections_.end()) {
        // Try to find by pattern (device name, interface, etc.)
        std::vector<std::string> matches = findConnectionsByPattern(connection_ref);
        if (matches.empty()) {
            std::cerr << "Error: Connection reference '" << connection_ref << "' not found" << std::endl;
            return false;
        }
        if (matches.size() > 1) {
            std::cerr << "Error: Ambiguous connection reference '" << connection_ref << "'. Matches:" << std::endl;
            for (const std::string& match : matches) {
                std::cerr << "  " << match << std::endl;
            }
            return false;
        }
        it = active_connections_.find(matches[0]);
    }

    if (it == active_connections_.end()) {
        return false;
    }

    const ConnectionReference& connection = it->second;

    std::cout << "Killing connection: " << connection.connection_id << std::endl;
    std::cout << "  Device: " << connection.device_path << std::endl;
    std::cout << "  Interface: " << connection.interface_name << std::endl;
    std::cout << "  APN: " << connection.apn << std::endl;
    std::cout << "  Process ID: " << connection.process_id << std::endl;

    // Try to gracefully terminate the process
    bool terminated = false;
    if (processExists(connection.process_id)) {
        std::cout << "Sending SIGTERM to process " << connection.process_id << "..." << std::endl;
        if (kill(connection.process_id, SIGTERM) == 0) {
            // Wait a few seconds for graceful shutdown
            for (int i = 0; i < 5; ++i) {
                sleep(1);
                if (!processExists(connection.process_id)) {
                    terminated = true;
                    break;
                }
            }
        }

        // If still running, force kill
        if (!terminated && processExists(connection.process_id)) {
            std::cout << "Sending SIGKILL to process " << connection.process_id << "..." << std::endl;
            kill(connection.process_id, SIGKILL);
            sleep(1);
        }
    }

    // Clean up QMI connection if we have the details
    if (connection.qmi_connection_id != 0) {
        std::ostringstream cleanup_cmd;
        cleanup_cmd << "qmicli -d " << connection.device_path 
                   << " --wds-stop-network=" << connection.qmi_connection_id
                   << " --client-no-release-cid";

        std::cout << "Cleaning up QMI connection..." << std::endl;
        system(cleanup_cmd.str().c_str());
    }

    // Clean up interface
    if (!connection.interface_name.empty()) {
        std::ostringstream interface_cmd;
        interface_cmd << "ip link set " << connection.interface_name << " down";
        system(interface_cmd.str().c_str());
    }

    // Remove from registry
    active_connections_.erase(it);
    saveRegistryToFile();

    std::cout << "Connection " << connection.connection_id << " killed successfully" << std::endl;
    return true;
}

bool ConnectionRegistry::killAllConnections() {
    std::lock_guard<std::mutex> lock(registry_mutex_);

    loadRegistryFromFile();

    if (active_connections_.empty()) {
        std::cout << "No active connections to kill" << std::endl;
        return true;
    }

    std::cout << "Killing " << active_connections_.size() << " active connections..." << std::endl;

    std::vector<std::string> connection_ids;
    for (const auto& entry : active_connections_) {
        connection_ids.push_back(entry.first);
    }

    bool all_successful = true;
    for (const std::string& connection_id : connection_ids) {
        if (!killConnection(connection_id)) {
            all_successful = false;
        }
    }

    return all_successful;
}

std::vector<ConnectionReference> ConnectionRegistry::listActiveConnections() {
    std::lock_guard<std::mutex> lock(registry_mutex_);

    loadRegistryFromFile();
    cleanupStaleConnections();

    std::vector<ConnectionReference> connections;
    for (const auto& entry : active_connections_) {
        connections.push_back(entry.second);
    }

    return connections;
}

ConnectionReference ConnectionRegistry::getConnectionStatus(const std::string& connection_ref) {
    std::lock_guard<std::mutex> lock(registry_mutex_);

    loadRegistryFromFile();
    cleanupStaleConnections();

    auto it = active_connections_.find(connection_ref);
    if (it != active_connections_.end()) {
        return it->second;
    }

    // Try pattern matching
    std::vector<std::string> matches = findConnectionsByPattern(connection_ref);
    if (matches.size() == 1) {
        it = active_connections_.find(matches[0]);
        if (it != active_connections_.end()) {
            return it->second;
        }
    }

    return ConnectionReference(); // Empty reference indicates not found
}

std::string ConnectionRegistry::createConnectionReference(const std::string& device_path, 
                                                        const std::string& interface_name,
                                                        const std::string& apn) {
    ConnectionReference connection;
    connection.device_path = device_path;
    connection.interface_name = interface_name;
    connection.apn = apn;
    return connection.generateConnectionId();
}

bool ConnectionRegistry::isConnectionActive(const std::string& connection_id) {
    std::lock_guard<std::mutex> lock(registry_mutex_);

    loadRegistryFromFile();

    auto it = active_connections_.find(connection_id);
    if (it != active_connections_.end()) {
        return processExists(it->second.process_id) && it->second.is_active;
    }

    return false;
}

bool ConnectionRegistry::processExists(pid_t pid) {
    if (pid <= 0) return false;
    return kill(pid, 0) == 0;
}

std::vector<std::string> ConnectionRegistry::findConnectionsByPattern(const std::string& pattern) {
    std::vector<std::string> matches;

    for (const auto& entry : active_connections_) {
        const ConnectionReference& connection = entry.second;

        // Check various fields for pattern match
        if (connection.connection_id.find(pattern) != std::string::npos ||
            connection.device_path.find(pattern) != std::string::npos ||
            connection.interface_name.find(pattern) != std::string::npos ||
            connection.apn.find(pattern) != std::string::npos) {
            matches.push_back(entry.first);
        }
    }

    return matches;
}

void ConnectionRegistry::initialize(const std::string& registry_file) {
    registry_file_path_ = registry_file;

    // Create registry directory if it doesn't exist
    std::string dir = registry_file.substr(0, registry_file.find_last_of('/'));
    mkdir(dir.c_str(), 0755);

    std::cout << "Connection registry initialized: " << registry_file_path_ << std::endl;
}

void ConnectionRegistry::cleanup() {
    std::lock_guard<std::mutex> lock(registry_mutex_);
    active_connections_.clear();
}

// Helper to get a copy of a connection entry
ConnectionReference ConnectionRegistry::getConnection(const std::string& connection_id) {
    std::lock_guard<std::mutex> lock(registry_mutex_);
    loadRegistryFromFile(); // Ensure we have the latest data from file
    auto it = active_connections_.find(connection_id);
    if (it != active_connections_.end()) {
        return it->second;
    }
    return ConnectionReference(); // Return empty if not found
}

// Helper to get all active connections
std::vector<ConnectionReference> ConnectionRegistry::getActiveConnections() {
    std::lock_guard<std::mutex> lock(registry_mutex_);
    
    // Always load from file to ensure we have the most current data
    if (!loadRegistryFromFile()) {
        std::cerr << "Warning: Could not load registry from file in getActiveConnections()" << std::endl;
    }
    
    // Clean up any stale entries
    cleanupStaleConnections();
    
    std::vector<ConnectionReference> connections;
    for (const auto& pair : active_connections_) {
        connections.push_back(pair.second);
    }
    return connections;
}

// Helper to remove a connection from the registry
void ConnectionRegistry::removeConnection(const std::string& connection_id) {
    std::lock_guard<std::mutex> lock(registry_mutex_);
    active_connections_.erase(connection_id);
    saveRegistryToFile();
}

void ConnectionRegistry::handleTerminationSignal(const std::string& connection_id) {
    std::cout << "Connection registry handling termination for: " << connection_id << std::endl;

    // Ensure registry is loaded from file to get current connection details
    std::lock_guard<std::mutex> lock(registry_mutex_);
    if (!loadRegistryFromFile()) {
        std::cerr << "Warning: Could not load registry from file during termination" << std::endl;
        // Continue with cleanup anyway, in case connection exists in memory
    }

    // Get connection details before removal
    auto it = active_connections_.find(connection_id);
    if (it == active_connections_.end()) {
        std::cerr << "Warning: Connection " << connection_id << " not found in registry" << std::endl;
        // Still attempt cleanup of basic interface/routing based on connection_id pattern
        attemptGenericCleanup(connection_id);
        return;
    }

    ConnectionReference connection = it->second;
    
    std::cout << "Performing connection-specific cleanup for: " << connection_id << std::endl;
    std::cout << "  Device: " << connection.device_path << std::endl;
    std::cout << "  Interface: " << connection.interface_name << std::endl;
    std::cout << "  APN: " << connection.apn << std::endl;
    std::cout << "  QMI Connection ID: " << connection.qmi_connection_id << std::endl;

    // Clean up QMI connection if we have the details
    if (connection.qmi_connection_id != 0 && !connection.device_path.empty()) {
        std::ostringstream cleanup_cmd;
        cleanup_cmd << "qmicli -d " << connection.device_path 
                   << " --wds-stop-network=" << connection.qmi_connection_id
                   << " --client-no-release-cid 2>/dev/null";

        std::cout << "Cleaning up QMI connection ID: " << connection.qmi_connection_id << std::endl;
        system(cleanup_cmd.str().c_str());
    }

    // Clean up interface with comprehensive cleanup
    if (!connection.interface_name.empty()) {
        std::cout << "Cleaning up interface: " << connection.interface_name << std::endl;
        
        // Stop any DHCP clients
        std::ostringstream dhcp_kill_cmd;
        dhcp_kill_cmd << "pkill -f 'dhclient.*" << connection.interface_name << "' 2>/dev/null";
        system(dhcp_kill_cmd.str().c_str());
        
        // Flush routes for this interface
        std::ostringstream route_flush_cmd;
        route_flush_cmd << "ip route flush dev " << connection.interface_name << " 2>/dev/null";
        std::cout << "Flushing routes for interface: " << connection.interface_name << std::endl;
        system(route_flush_cmd.str().c_str());

        // Flush interface addresses
        std::ostringstream flush_cmd;
        flush_cmd << "ip addr flush dev " << connection.interface_name << " 2>/dev/null";
        std::cout << "Flushing addresses for interface: " << connection.interface_name << std::endl;
        system(flush_cmd.str().c_str());

        // Bring interface down
        std::ostringstream interface_cmd;
        interface_cmd << "ip link set dev " << connection.interface_name << " down 2>/dev/null";
        std::cout << "Bringing down interface: " << connection.interface_name << std::endl;
        system(interface_cmd.str().c_str());
    }

    // Remove from registry
    active_connections_.erase(it);
    saveRegistryToFile();
    std::cout << "Connection " << connection_id << " unregistered from registry" << std::endl;
}

void ConnectionRegistry::attemptGenericCleanup(const std::string& connection_id) {
    std::cout << "Attempting generic cleanup for connection: " << connection_id << std::endl;
    
    // Try to extract device and interface info from connection ID pattern
    // Format is typically: device_timestamp (e.g., cdc-wdm3_1755447267)
    std::string device_name, interface_name;
    size_t underscore_pos = connection_id.find_last_of('_');
    if (underscore_pos != std::string::npos) {
        device_name = connection_id.substr(0, underscore_pos);
        interface_name = "wwan"; // Default WWAN interface pattern
        
        // Try to find actual interface by device pattern
        std::ostringstream find_interface_cmd;
        find_interface_cmd << "ip link show | grep -o 'wwan[0-9]*' | head -1";
        
        FILE* pipe = popen(find_interface_cmd.str().c_str(), "r");
        if (pipe) {
            char buffer[128];
            if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                std::string found_interface(buffer);
                // Remove trailing newline
                if (!found_interface.empty() && found_interface.back() == '\n') {
                    found_interface.pop_back();
                }
                if (!found_interface.empty()) {
                    interface_name = found_interface;
                }
            }
            pclose(pipe);
        }
    }
    
    // Generic cleanup operations
    if (!interface_name.empty() && interface_name != "wwan") {
        std::cout << "Performing generic cleanup for interface: " << interface_name << std::endl;
        
        // Kill any DHCP clients
        std::ostringstream dhcp_kill_cmd;
        dhcp_kill_cmd << "pkill -f 'dhclient.*" << interface_name << "' 2>/dev/null";
        system(dhcp_kill_cmd.str().c_str());
        
        // Flush routes
        std::ostringstream route_flush_cmd;
        route_flush_cmd << "ip route flush dev " << interface_name << " 2>/dev/null";
        system(route_flush_cmd.str().c_str());

        // Flush addresses
        std::ostringstream addr_flush_cmd;
        addr_flush_cmd << "ip addr flush dev " << interface_name << " 2>/dev/null";
        system(addr_flush_cmd.str().c_str());

        // Bring interface down
        std::ostringstream interface_down_cmd;
        interface_down_cmd << "ip link set dev " << interface_name << " down 2>/dev/null";
        system(interface_down_cmd.str().c_str());
        
        std::cout << "Generic cleanup completed for interface: " << interface_name << std::endl;
    }
    
    // Try to clean up any QMI connections for the device
    if (!device_name.empty()) {
        std::string device_path = "/dev/" + device_name;
        std::cout << "Attempting QMI cleanup for device: " << device_path << std::endl;
        
        // Try to stop any active network sessions (generic)
        std::ostringstream qmi_cleanup_cmd;
        qmi_cleanup_cmd << "timeout 5 qmicli -d " << device_path << " --wds-stop-network --autoconnect 2>/dev/null";
        system(qmi_cleanup_cmd.str().c_str());
    }
}

void ConnectionRegistry::handleGlobalTermination() {
    std::cout << "Connection registry handling global termination..." << std::endl;

    // Get all active connections (now with proper file loading)
    std::vector<ConnectionReference> active_connections = getActiveConnections();

    if (active_connections.empty()) {
        std::cout << "No active connections found in registry" << std::endl;
        
        // Even if registry is empty, perform emergency cleanup of any WWAN interfaces
        std::cout << "Performing emergency cleanup of all WWAN interfaces..." << std::endl;
        performEmergencyWWANCleanup();
        return;
    }

    std::cout << "Found " << active_connections.size() << " active connections to clean up" << std::endl;

    // Clean up each connection gracefully
    for (const auto& connection : active_connections) {
        std::cout << "Cleaning up connection: " << connection.connection_id << std::endl;
        
        // Create a separate thread-safe cleanup to avoid deadlock issues
        try {
            // Manual cleanup to avoid double-locking registry_mutex_
            std::cout << "Performing connection-specific cleanup for: " << connection.connection_id << std::endl;
            std::cout << "  Device: " << connection.device_path << std::endl;
            std::cout << "  Interface: " << connection.interface_name << std::endl;
            std::cout << "  APN: " << connection.apn << std::endl;
            std::cout << "  QMI Connection ID: " << connection.qmi_connection_id << std::endl;

            // Clean up QMI connection if we have the details
            if (connection.qmi_connection_id != 0 && !connection.device_path.empty()) {
                std::ostringstream cleanup_cmd;
                cleanup_cmd << "qmicli -d " << connection.device_path 
                           << " --wds-stop-network=" << connection.qmi_connection_id
                           << " --client-no-release-cid 2>/dev/null";

                std::cout << "Cleaning up QMI connection ID: " << connection.qmi_connection_id << std::endl;
                system(cleanup_cmd.str().c_str());
            }

            // Clean up interface with comprehensive cleanup
            if (!connection.interface_name.empty()) {
                std::cout << "Cleaning up interface: " << connection.interface_name << std::endl;
                
                // Stop any DHCP clients
                std::ostringstream dhcp_kill_cmd;
                dhcp_kill_cmd << "pkill -f 'dhclient.*" << connection.interface_name << "' 2>/dev/null";
                system(dhcp_kill_cmd.str().c_str());
                
                // Flush routes for this interface
                std::ostringstream route_flush_cmd;
                route_flush_cmd << "ip route flush dev " << connection.interface_name << " 2>/dev/null";
                system(route_flush_cmd.str().c_str());

                // Flush interface addresses
                std::ostringstream flush_cmd;
                flush_cmd << "ip addr flush dev " << connection.interface_name << " 2>/dev/null";
                system(flush_cmd.str().c_str());

                // Remove any default routes that may use this interface
                std::ostringstream route_del_cmd;
                route_del_cmd << "ip route del default dev " << connection.interface_name << " 2>/dev/null";
                system(route_del_cmd.str().c_str());
                
                // Check for and remove any specific gateway routes
                std::ostringstream show_routes_cmd;
                show_routes_cmd << "ip route show dev " << connection.interface_name << " | grep '^default\\|via' | head -5";
                
                FILE* routes_pipe = popen(show_routes_cmd.str().c_str(), "r");
                if (routes_pipe) {
                    char route_buffer[256];
                    while (fgets(route_buffer, sizeof(route_buffer), routes_pipe) != nullptr) {
                        std::string route_line(route_buffer);
                        if (!route_line.empty() && route_line.back() == '\n') {
                            route_line.pop_back();
                        }
                        
                        if (!route_line.empty()) {
                            std::ostringstream del_specific_route;
                            del_specific_route << "ip route del " << route_line << " 2>/dev/null";
                            system(del_specific_route.str().c_str());
                        }
                    }
                    pclose(routes_pipe);
                }

                // Bring interface down
                std::ostringstream interface_cmd;
                interface_cmd << "ip link set dev " << connection.interface_name << " down 2>/dev/null";
                system(interface_cmd.str().c_str());
                
                std::cout << "Interface " << connection.interface_name << " cleanup completed" << std::endl;
            }

            std::cout << "Connection " << connection.connection_id << " cleanup completed" << std::endl;
            
        } catch (const std::exception& e) {
            std::cerr << "Error during cleanup of connection " << connection.connection_id << ": " << e.what() << std::endl;
        } catch (...) {
            std::cerr << "Unknown error during cleanup of connection " << connection.connection_id << std::endl;
        }
    }
    
    // After all cleanups are done, clear the registry
    {
        std::lock_guard<std::mutex> lock(registry_mutex_);
        active_connections_.clear();
        saveRegistryToFile();
    }

    std::cout << "Global termination cleanup completed" << std::endl;
}

void ConnectionRegistry::performEmergencyWWANCleanup() {
    std::cout << "Performing emergency WWAN cleanup..." << std::endl;
    
    // Find all WWAN interfaces
    std::string find_interfaces_cmd = "ip link show | grep -E ': (wwan|wwp|rmnet)' | awk '{print $2}' | sed 's/:$//'";
    FILE* pipe = popen(find_interfaces_cmd.c_str(), "r");
    if (pipe) {
        char buffer[128];
        std::vector<std::string> wwan_interfaces;
        
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            std::string interface(buffer);
            if (!interface.empty() && interface.back() == '\n') {
                interface.pop_back();
            }
            if (!interface.empty()) {
                wwan_interfaces.push_back(interface);
            }
        }
        pclose(pipe);
        
        std::cout << "Found " << wwan_interfaces.size() << " WWAN interfaces for emergency cleanup" << std::endl;
        
        for (const auto& interface : wwan_interfaces) {
            std::cout << "Emergency cleanup for interface: " << interface << std::endl;
            
            // Stop DHCP clients
            std::ostringstream dhcp_kill_cmd;
            dhcp_kill_cmd << "pkill -f 'dhclient.*" << interface << "' 2>/dev/null";
            system(dhcp_kill_cmd.str().c_str());
            
            // Flush routes
            std::ostringstream route_flush_cmd;
            route_flush_cmd << "ip route flush dev " << interface << " 2>/dev/null";
            system(route_flush_cmd.str().c_str());
            
            // Flush addresses
            std::ostringstream addr_flush_cmd;
            addr_flush_cmd << "ip addr flush dev " << interface << " 2>/dev/null";
            system(addr_flush_cmd.str().c_str());
            
            // Bring interface down
            std::ostringstream interface_down_cmd;
            interface_down_cmd << "ip link set dev " << interface << " down 2>/dev/null";
            system(interface_down_cmd.str().c_str());
            
            std::cout << "Emergency cleanup completed for: " << interface << std::endl;
        }
        
        // Also try to stop any QMI connections
        std::string find_qmi_cmd = "ls /dev/cdc-wdm* 2>/dev/null";
        FILE* qmi_pipe = popen(find_qmi_cmd.c_str(), "r");
        if (qmi_pipe) {
            char qmi_buffer[128];
            while (fgets(qmi_buffer, sizeof(qmi_buffer), qmi_pipe) != nullptr) {
                std::string device(qmi_buffer);
                if (!device.empty() && device.back() == '\n') {
                    device.pop_back();
                }
                if (!device.empty()) {
                    std::cout << "Emergency QMI cleanup for device: " << device << std::endl;
                    std::ostringstream qmi_cleanup_cmd;
                    qmi_cleanup_cmd << "timeout 5 qmicli -d " << device << " --wds-stop-network --autoconnect 2>/dev/null";
                    system(qmi_cleanup_cmd.str().c_str());
                }
            }
            pclose(qmi_pipe);
        }
    }
    
    std::cout << "Emergency WWAN cleanup completed" << std::endl;
}

void ConnectionRegistry::printConnectionsList() {
    std::lock_guard<std::mutex> lock(registry_mutex_);
    
    loadRegistryFromFile();
    cleanupStaleConnections();
    
    if (active_connections_.empty()) {
        std::cout << "No active connections found" << std::endl;
        return;
    }
    
    std::cout << "\n=== Active QMI Connections ===" << std::endl;
    std::cout << "Total connections: " << active_connections_.size() << std::endl;
    std::cout << std::endl;
    
    for (const auto& entry : active_connections_) {
        const ConnectionReference& connection = entry.second;
        std::cout << formatConnectionInfo(connection) << std::endl;
    }
}

void ConnectionRegistry::printConnectionStatus(const std::string& connection_ref) {
    ConnectionReference connection = getConnectionStatus(connection_ref);
    
    if (connection.connection_id.empty()) {
        std::cerr << "Error: Connection '" << connection_ref << "' not found" << std::endl;
        
        // Try to find similar connections
        std::lock_guard<std::mutex> lock(registry_mutex_);
        loadRegistryFromFile();
        std::vector<std::string> matches = findConnectionsByPattern(connection_ref);
        
        if (!matches.empty()) {
            std::cout << "\nSimilar connections found:" << std::endl;
            for (const std::string& match : matches) {
                std::cout << "  " << match << std::endl;
            }
        }
        return;
    }
    
    std::cout << "\n=== Connection Status ===" << std::endl;
    std::cout << formatConnectionInfo(connection) << std::endl;
    
    // Additional status checks
    bool process_running = processExists(connection.process_id);
    std::cout << "Process Status: " << (process_running ? "Running" : "Not Running") << std::endl;
    
    // Check interface status if available
    if (!connection.interface_name.empty()) {
        std::string check_cmd = "ip link show " + connection.interface_name + " 2>/dev/null | grep -q UP";
        bool interface_up = (system(check_cmd.c_str()) == 0);
        std::cout << "Interface Status: " << (interface_up ? "UP" : "DOWN") << std::endl;
    }
}

std::string ConnectionRegistry::formatConnectionInfo(const ConnectionReference& connection) {
    std::ostringstream info;
    
    // Format start time
    auto time_t = std::chrono::system_clock::to_time_t(connection.start_time);
    std::tm* tm = std::localtime(&time_t);
    
    // Calculate duration
    auto now = std::chrono::system_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::minutes>(now - connection.start_time);
    
    info << "Connection ID: " << connection.connection_id << std::endl;
    info << "  Device: " << connection.device_path << std::endl;
    info << "  Interface: " << (connection.interface_name.empty() ? "auto" : connection.interface_name) << std::endl;
    info << "  APN: " << connection.apn << std::endl;
    info << "  Process ID: " << connection.process_id << std::endl;
    info << "  QMI Connection ID: " << connection.qmi_connection_id << std::endl;
    info << "  Status: " << (connection.is_active ? "ACTIVE" : "INACTIVE") << std::endl;
    info << "  Start Time: " << std::put_time(tm, "%Y-%m-%d %H:%M:%S") << std::endl;
    info << "  Duration: " << duration.count() << " minutes" << std::endl;
    
    return info.str();
}


void ConnectionLifecycleManager::setupSignalHandlers(const std::string& connection_id) {
    // Store connection ID for cleanup (thread-safe static)
    static std::mutex signal_mutex;
    static std::string static_connection_id;
    
    {
        std::lock_guard<std::mutex> lock(signal_mutex);
        static_connection_id = connection_id;
    }

    // Set up signal handlers only once to avoid conflicts
    static std::atomic<bool> handlers_installed{false};
    if (!handlers_installed.exchange(true)) {
        std::cout << "Installing enhanced signal handlers for connection cleanup" << std::endl;
        
        // Set up signal handlers to call the global termination handler
        signal(SIGINT, [](int signal_num){
            std::cout << "\nCaught signal: " << signal_num << std::endl;
            try {
                ConnectionRegistry::handleGlobalTermination();
            } catch (const std::exception& e) {
                std::cerr << "Error during signal handling: " << e.what() << std::endl;
            } catch (...) {
                std::cerr << "Unknown error during signal handling" << std::endl;
            }
            exit(signal_num);
        });
        
        signal(SIGTERM, [](int signal_num){
            std::cout << "\nCaught signal: " << signal_num << std::endl;
            try {
                ConnectionRegistry::handleGlobalTermination();
            } catch (const std::exception& e) {
                std::cerr << "Error during signal handling: " << e.what() << std::endl;
            } catch (...) {
                std::cerr << "Unknown error during signal handling" << std::endl;
            }
            exit(signal_num);
        });
    } else {
        std::cout << "Signal handlers already installed, reusing for connection: " << connection_id << std::endl;
    }
}


// ConnectionLifecycleManager implementation
ConnectionLifecycleManager::ConnectionLifecycleManager(const std::string& device_path, 
                                                      const std::string& interface_name,
                                                      const std::string& apn) 
    : registered_(false) {

    ConnectionReference connection;
    connection.device_path = device_path;
    connection.interface_name = interface_name;
    connection.apn = apn;
    connection.is_active = false;

    connection_id_ = connection.generateConnectionId();

    if (ConnectionRegistry::registerConnection(connection)) {
        registered_ = true;
        setupSignalHandlers(connection_id_);
        std::cout << "Connection lifecycle manager initialized for: " << connection_id_ << std::endl;
    }
}

ConnectionLifecycleManager::~ConnectionLifecycleManager() {
    if (registered_) {
        ConnectionRegistry::unregisterConnection(connection_id_);
        std::cout << "Connection lifecycle manager destroyed for: " << connection_id_ << std::endl;
    }
}

bool ConnectionLifecycleManager::registerConnection(uint32_t qmi_connection_id, 
                                                  const std::string& packet_data_handle) {
    if (!registered_) return false;

    ConnectionReference connection = ConnectionRegistry::getConnectionStatus(connection_id_);
    if (connection.connection_id.empty()) return false;

    connection.qmi_connection_id = qmi_connection_id;
    connection.packet_data_handle = packet_data_handle;
    connection.is_active = true;

    return ConnectionRegistry::updateConnection(connection_id_, connection);
}

bool ConnectionLifecycleManager::updateStatus(bool is_active) {
    if (!registered_) return false;

    ConnectionReference connection = ConnectionRegistry::getConnectionStatus(connection_id_);
    if (connection.connection_id.empty()) return false;

    connection.is_active = is_active;

    return ConnectionRegistry::updateConnection(connection_id_, connection);
}

bool ConnectionLifecycleManager::deregisterConnection(const std::string& interface_name) {
    if (!registered_) return false;
    
    bool result = ConnectionRegistry::unregisterConnection(connection_id_);
    if (result) {
        registered_ = false;
        std::cout << "Connection deregistered: " << connection_id_ << std::endl;
    }
    
    return result;
}

void ConnectionLifecycleManager::deregisterAllConnections() {
    if (registered_) {
        deregisterConnection();
    }
}