#include "urwt/monitor/scan_result_cache.hpp"
#include <algorithm>

namespace urwt {
namespace monitor {

ScanResultCache::ScanResultCache(std::chrono::seconds default_ttl)
    : default_ttl_(default_ttl) {
}

ScanResultCache::~ScanResultCache() {
    clear();
}

void ScanResultCache::store(const ScanResult& result) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    CachedScanResult cached;
    cached.result = result;
    cached.cached_at = std::chrono::system_clock::now();
    
    cache_[result.interface().name()] = cached;
}

std::optional<ScanResult> ScanResultCache::get(const std::string& interface) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = cache_.find(interface);
    if (it == cache_.end()) {
        return std::nullopt;
    }
    
    if (it->second.isExpired(default_ttl_)) {
        cache_.erase(it);
        return std::nullopt;
    }
    
    return it->second.result;
}

void ScanResultCache::invalidate(const std::string& interface) {
    std::lock_guard<std::mutex> lock(mutex_);
    cache_.erase(interface);
}

void ScanResultCache::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    cache_.clear();
}

std::optional<NetworkInfo> ScanResultCache::findNetwork(
    const std::string& interface,
    const std::string& ssid) {
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = cache_.find(interface);
    if (it == cache_.end()) {
        return std::nullopt;
    }
    
    if (it->second.isExpired(default_ttl_)) {
        cache_.erase(it);
        return std::nullopt;
    }
    
    return it->second.result.findBySSID(ssid);
}

std::vector<NetworkInfo> ScanResultCache::getAvailableNetworks(
    const std::string& interface) {
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = cache_.find(interface);
    if (it == cache_.end()) {
        return {};
    }
    
    if (it->second.isExpired(default_ttl_)) {
        cache_.erase(it);
        return {};
    }
    
    return it->second.result.networks();
}

void ScanResultCache::setTTL(std::chrono::seconds ttl) {
    std::lock_guard<std::mutex> lock(mutex_);
    default_ttl_ = ttl;
}

size_t ScanResultCache::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return cache_.size();
}

void ScanResultCache::cleanup() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = cache_.begin();
    while (it != cache_.end()) {
        if (it->second.isExpired(default_ttl_)) {
            it = cache_.erase(it);
        } else {
            ++it;
        }
    }
}

} // namespace monitor
} // namespace urwt
