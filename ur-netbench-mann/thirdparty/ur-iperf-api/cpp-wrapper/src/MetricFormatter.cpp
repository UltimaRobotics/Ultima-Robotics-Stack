
#include "MetricFormatter.hpp"
#include <sstream>
#include <iomanip>
#include <cstdint>

std::string MetricFormatter::formatBitsPerSecond(double bps) {
    // Use 1000-based units for rates (as per SI standard)
    const double KILO = 1000.0;
    const double MEGA = 1000.0 * 1000.0;
    const double GIGA = 1000.0 * 1000.0 * 1000.0;
    const double TERA = 1000.0 * 1000.0 * 1000.0 * 1000.0;
    
    if (bps >= TERA) {
        return formatValue(bps / TERA, "Tbps");
    } else if (bps >= GIGA) {
        return formatValue(bps / GIGA, "Gbps");
    } else if (bps >= MEGA) {
        return formatValue(bps / MEGA, "Mbps");
    } else if (bps >= KILO) {
        return formatValue(bps / KILO, "Kbps");
    } else {
        return formatValue(bps, "bps");
    }
}

std::string MetricFormatter::formatBytes(uint64_t bytes) {
    // Use 1024-based units for data sizes
    const double KILO = 1024.0;
    const double MEGA = 1024.0 * 1024.0;
    const double GIGA = 1024.0 * 1024.0 * 1024.0;
    const double TERA = 1024.0 * 1024.0 * 1024.0 * 1024.0;
    
    double value = static_cast<double>(bytes);
    
    if (value >= TERA) {
        return formatValue(value / TERA, "TB");
    } else if (value >= GIGA) {
        return formatValue(value / GIGA, "GB");
    } else if (value >= MEGA) {
        return formatValue(value / MEGA, "MB");
    } else if (value >= KILO) {
        return formatValue(value / KILO, "KB");
    } else {
        return formatValue(value, "B");
    }
}

std::string MetricFormatter::formatSeconds(double seconds) {
    if (seconds < 0.001) {
        return formatValue(seconds * 1000000.0, "µs");
    } else if (seconds < 1.0) {
        return formatValue(seconds * 1000.0, "ms");
    } else if (seconds < 60.0) {
        return formatValue(seconds, "s");
    } else if (seconds < 3600.0) {
        int mins = static_cast<int>(seconds / 60.0);
        double secs = seconds - (mins * 60.0);
        std::ostringstream oss;
        oss << mins << "m " << std::fixed << std::setprecision(1) << secs << "s";
        return oss.str();
    } else {
        int hours = static_cast<int>(seconds / 3600.0);
        int mins = static_cast<int>((seconds - hours * 3600.0) / 60.0);
        std::ostringstream oss;
        oss << hours << "h " << mins << "m";
        return oss.str();
    }
}

std::string MetricFormatter::formatPercentage(double value) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << value << "%";
    return oss.str();
}

std::string MetricFormatter::formatValue(double value, const std::string& unit) {
    std::ostringstream oss;
    
    // Choose precision based on magnitude
    if (value < 10.0) {
        oss << std::fixed << std::setprecision(2);
    } else if (value < 100.0) {
        oss << std::fixed << std::setprecision(1);
    } else {
        oss << std::fixed << std::setprecision(0);
    }
    
    oss << value << " " << unit;
    return oss.str();
}
