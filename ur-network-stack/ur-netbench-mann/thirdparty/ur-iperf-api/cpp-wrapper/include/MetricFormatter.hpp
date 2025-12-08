
#pragma once

#include <string>
#include <cmath>
#include <cstdint>

class MetricFormatter {
public:
    // Format bits per second (rate) - uses 1000-based units
    static std::string formatBitsPerSecond(double bps);
    
    // Format bytes (size) - uses 1024-based units
    static std::string formatBytes(uint64_t bytes);
    
    // Format seconds with appropriate precision
    static std::string formatSeconds(double seconds);
    
    // Format percentage
    static std::string formatPercentage(double value);
    
private:
    // Helper to format with appropriate precision
    static std::string formatValue(double value, const std::string& unit);
};
