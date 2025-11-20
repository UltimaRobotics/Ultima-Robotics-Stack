
#pragma once

#include <string>
#include <filesystem>
#include <stdexcept>

class PathValidator {
public:
    // Validate and sanitize file paths to prevent directory traversal
    static std::string validate_path(const std::string& base_dir, const std::string& user_path) {
        std::filesystem::path base = std::filesystem::absolute(base_dir);
        std::filesystem::path full = std::filesystem::absolute(base / user_path);
        
        // Ensure the resolved path is within the base directory
        auto [base_end, full_end] = std::mismatch(base.begin(), base.end(), full.begin());
        if (base_end != base.end()) {
            throw std::runtime_error("Path traversal attempt detected");
        }
        
        return full.string();
    }
    
    // Check if path contains suspicious patterns
    static bool is_safe_path(const std::string& path) {
        return path.find("..") == std::string::npos && 
               path.find("~") == std::string::npos &&
               !path.empty() &&
               path[0] != '/';  // Reject absolute paths from user input
    }
};
