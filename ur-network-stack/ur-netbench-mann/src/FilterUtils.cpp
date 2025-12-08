
#include "../include/FilterUtils.hpp"
#include <algorithm>
#include <cctype>

std::string to_lower(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(), 
                   [](unsigned char c){ return std::tolower(c); });
    return result;
}

bool port_in_range(int port, const std::string& port_field) {
    if (port_field.empty()) {
        return false;
    }
    
    size_t dash_pos = port_field.find('-');
    if (dash_pos != std::string::npos) {
        try {
            int start = std::stoi(port_field.substr(0, dash_pos));
            int end = std::stoi(port_field.substr(dash_pos + 1));
            return port >= start && port <= end;
        } catch (...) {
            return false;
        }
    }
    
    try {
        int port_value = std::stoi(port_field);
        return port == port_value;
    } catch (...) {
        return false;
    }
}

bool matches_filter(const json& server, const json& filters) {
    if (filters.contains("keyword") && !filters["keyword"].get<std::string>().empty()) {
        std::string keyword = to_lower(filters["keyword"].get<std::string>());
        bool match = false;
        
        std::vector<std::string> fields = {"IP/HOST", "CONTINENT", "COUNTRY", "SITE", "PROVIDER"};
        for (const auto& field : fields) {
            if (server.contains(field)) {
                std::string value = to_lower(server[field].get<std::string>());
                if (value.find(keyword) != std::string::npos) {
                    match = true;
                    break;
                }
            }
        }
        
        if (!match) return false;
    }
    
    if (filters.contains("continent") && !filters["continent"].get<std::string>().empty()) {
        std::string continent_filter = to_lower(filters["continent"].get<std::string>());
        if (server.contains("CONTINENT")) {
            std::string continent = to_lower(server["CONTINENT"].get<std::string>());
            if (continent.find(continent_filter) == std::string::npos) {
                return false;
            }
        } else {
            return false;
        }
    }
    
    if (filters.contains("country") && !filters["country"].get<std::string>().empty()) {
        std::string country_filter = to_lower(filters["country"].get<std::string>());
        if (server.contains("COUNTRY")) {
            std::string country = to_lower(server["COUNTRY"].get<std::string>());
            if (country.find(country_filter) == std::string::npos) {
                return false;
            }
        } else {
            return false;
        }
    }
    
    if (filters.contains("site") && !filters["site"].get<std::string>().empty()) {
        std::string site_filter = to_lower(filters["site"].get<std::string>());
        if (server.contains("SITE")) {
            std::string site = to_lower(server["SITE"].get<std::string>());
            if (site.find(site_filter) == std::string::npos) {
                return false;
            }
        } else {
            return false;
        }
    }
    
    if (filters.contains("provider") && !filters["provider"].get<std::string>().empty()) {
        std::string provider_filter = to_lower(filters["provider"].get<std::string>());
        if (server.contains("PROVIDER")) {
            std::string provider = to_lower(server["PROVIDER"].get<std::string>());
            if (provider.find(provider_filter) == std::string::npos) {
                return false;
            }
        } else {
            return false;
        }
    }
    
    if (filters.contains("host") && !filters["host"].get<std::string>().empty()) {
        std::string host_filter = to_lower(filters["host"].get<std::string>());
        if (server.contains("IP/HOST")) {
            std::string host = to_lower(server["IP/HOST"].get<std::string>());
            if (host.find(host_filter) == std::string::npos) {
                return false;
            }
        } else {
            return false;
        }
    }
    
    if (filters.contains("port")) {
        int port_filter = filters["port"].get<int>();
        if (server.contains("PORT")) {
            std::string port_field = server["PORT"].is_string() ? 
                                    server["PORT"].get<std::string>() : 
                                    std::to_string(server["PORT"].get<int>());
            if (!port_in_range(port_filter, port_field)) {
                return false;
            }
        } else {
            return false;
        }
    }
    
    if (filters.contains("min_speed") && !filters["min_speed"].get<std::string>().empty()) {
        std::string min_speed = filters["min_speed"].get<std::string>();
        if (server.contains("GB/S") && !server["GB/S"].get<std::string>().empty()) {
            std::string speed = server["GB/S"].get<std::string>();
            if (speed < min_speed) {
                return false;
            }
        } else {
            return false;
        }
    }
    
    if (filters.contains("options") && !filters["options"].get<std::string>().empty()) {
        std::string options_filter = to_lower(filters["options"].get<std::string>());
        if (server.contains("OPTIONS")) {
            std::string options = to_lower(server["OPTIONS"].get<std::string>());
            if (options.find(options_filter) == std::string::npos) {
                return false;
            }
        } else {
            return false;
        }
    }
    
    return true;
}

json filter_servers(const json& servers, const json& filters) {
    json filtered = json::array();
    
    for (const auto& server : servers) {
        if (matches_filter(server, filters)) {
            filtered.push_back(server);
        }
    }
    
    return filtered;
}
