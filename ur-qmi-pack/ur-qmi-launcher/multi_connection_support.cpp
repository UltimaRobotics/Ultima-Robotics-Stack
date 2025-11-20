// Multi-connection support methods implementation for QMISessionHandler

#include "qmi_session_handler.h"
#include <iostream>
#include <sstream>
#include <algorithm>

std::string QMISessionHandler::getAssignedInterfaceName() const {
    std::lock_guard<std::mutex> lock(m_session_mutex);
    return m_interface_name;
}

void QMISessionHandler::setInterfaceName(const std::string& interface_name) {
    std::lock_guard<std::mutex> lock(m_session_mutex);
    if (interface_name != m_interface_name) {
        std::cout << "Interface name changed from " << m_interface_name << " to " << interface_name 
                  << " for device " << m_device_path << std::endl;
        m_interface_name = interface_name;
        m_interface_auto_detected = false;
    }
}

std::string QMISessionHandler::autoDetectInterfaceName() const {
    // Try to find interface names associated with this device
    std::vector<std::string> candidates = {"wwan0", "wwan1", "wwan2", "wwan3", "wwan4", "wwan5"};
    
    // First, try to find an available interface
    for (const std::string& candidate : candidates) {
        if (isInterfaceAvailable(candidate)) {
            // Check if this interface corresponds to our device by checking device path association
            std::ostringstream cmd;
            cmd << "readlink -f /sys/class/net/" << candidate << "/device 2>/dev/null | grep -q " << m_device_instance_id;
            if (const_cast<QMISessionHandler*>(this)->executeCommand(cmd.str()).empty()) {
                std::cout << "Auto-detected interface: " << candidate << " for device " << m_device_path << std::endl;
                return candidate;
            }
        }
    }
    
    // Fallback: use the next available wwan interface
    std::string next_available = findNextAvailableInterface("wwan");
    if (!next_available.empty()) {
        std::cout << "Using next available interface: " << next_available << " for device " << m_device_path << std::endl;
        return next_available;
    }
    
    // Last fallback: use wwan0
    std::cout << "Fallback to wwan0 for device " << m_device_path << std::endl;
    return "wwan0";
}

bool QMISessionHandler::isInterfaceAvailable(const std::string& interface_name) const {
    std::ostringstream cmd;
    cmd << "ip link show " << interface_name << " 2>/dev/null";
    std::string output = const_cast<QMISessionHandler*>(this)->executeCommand(cmd.str());
    return !output.empty() && output.find("does not exist") == std::string::npos;
}

std::string QMISessionHandler::getDeviceInstanceId() const {
    return m_device_instance_id;
}

std::vector<std::string> QMISessionHandler::getAvailableInterfaces() {
    std::vector<std::string> interfaces;
    
    // Get all network interfaces (use system command for static method)
    std::string output;
    FILE* pipe = popen("ip link show | grep -E '^[0-9]+: ' | awk -F': ' '{print $2}' | awk '{print $1}'", "r");
    if (pipe) {
        char buffer[128];
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            output += buffer;
        }
        pclose(pipe);
    }
    
    std::istringstream iss(output);
    std::string interface;
    while (std::getline(iss, interface)) {
        if (interface.find("wwan") == 0 || interface.find("usb") == 0 || interface.find("qmi") == 0) {
            interfaces.push_back(interface);
        }
    }
    
    return interfaces;
}

std::string QMISessionHandler::findNextAvailableInterface(const std::string& base_name) {
    for (int i = 0; i < 10; ++i) {
        std::string interface_name = base_name + std::to_string(i);
        
        // Check if interface exists but is not in use by another connection
        std::ostringstream cmd;
        cmd << "ip link show " << interface_name << " 2>/dev/null";
        
        // Use system command for static method
        std::string output;
        FILE* pipe = popen(cmd.str().c_str(), "r");
        if (pipe) {
            char buffer[1024];
            while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                output += buffer;
            }
            pclose(pipe);
        }
        
        if (!output.empty() && output.find("does not exist") == std::string::npos) {
            // Interface exists, check if it's free (not UP with an IP address)
            std::ostringstream status_cmd;
            status_cmd << "ip addr show " << interface_name << " | grep 'inet ' | wc -l";
            
            std::string status_output;
            FILE* status_pipe = popen(status_cmd.str().c_str(), "r");
            if (status_pipe) {
                char buffer[128];
                while (fgets(buffer, sizeof(buffer), status_pipe) != nullptr) {
                    status_output += buffer;
                }
                pclose(status_pipe);
            }
            
            if (status_output.find("0") != std::string::npos) {
                // Interface exists but has no IP - it's available
                return interface_name;
            }
        }
    }
    
    return ""; // No available interface found
}