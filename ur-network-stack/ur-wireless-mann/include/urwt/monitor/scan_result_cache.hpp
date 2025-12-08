#ifndef URWT_MONITOR_SCAN_RESULT_CACHE_HPP
#define URWT_MONITOR_SCAN_RESULT_CACHE_HPP

#include <map>
#include <mutex>
#include <chrono>
#include <optional>
#include "urwt/models/scan_result.hpp"
#include "urwt/models/network_info.hpp"

namespace urwt {
namespace monitor {

struct CachedScanResult {
    ScanResult result;
    std::chrono::system_clock::time_point cached_at;

    bool isExpired(std::chrono::seconds max_age) const {
        auto now = std::chrono::system_clock::now();
        auto age = std::chrono::duration_cast<std::chrono::seconds>(
            now - cached_at);
        return age > max_age;
    }
};

class ScanResultCache {
public:
    explicit ScanResultCache(std::chrono::seconds default_ttl = std::chrono::seconds(120));
    ~ScanResultCache();

    // Cache operations
    void store(const ScanResult& result);
    std::optional<ScanResult> get(const std::string& interface);
    void invalidate(const std::string& interface);
    void clear();

    // Network lookup
    std::optional<NetworkInfo> findNetwork(const std::string& interface,
                                          const std::string& ssid);
    std::vector<NetworkInfo> getAvailableNetworks(const std::string& interface);

    // Cache management
    void setTTL(std::chrono::seconds ttl);
    size_t size() const;
    void cleanup();  // Remove expired entries

private:
    mutable std::mutex mutex_;
    std::map<std::string, CachedScanResult> cache_;
    std::chrono::seconds default_ttl_;
};

} // namespace monitor
} // namespace urwt

#endif // URWT_MONITOR_SCAN_RESULT_CACHE_HPP
