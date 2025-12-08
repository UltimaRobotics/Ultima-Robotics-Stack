
#ifndef FILTER_UTILS_HPP
#define FILTER_UTILS_HPP

#include <string>
#include "json.hpp"

using json = nlohmann::json;

// String utilities
std::string to_lower(const std::string& str);

// Port range checking
bool port_in_range(int port, const std::string& port_field);

// Server filtering
bool matches_filter(const json& server, const json& filters);
json filter_servers(const json& servers, const json& filters);

#endif // FILTER_UTILS_HPP
