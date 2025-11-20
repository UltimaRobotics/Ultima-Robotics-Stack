
# VPN Statistics Collection Mechanisms

## Overview

The VPN Instance Manager implements real-time statistics collection for both OpenVPN and WireGuard connections. This document explains how statistics are collected from VPN threads and saved to JSON configuration files.

## Architecture

### Component Hierarchy

```
VPNInstanceManager
    ├── VPNInstance (per connection)
    │   ├── Wrapper Instance (OpenVPNWrapper or WireGuardWrapper)
    │   │   ├── Stats Thread (calculates bandwidth rates)
    │   │   └── Bridge Context (communicates with VPN kernel/userspace)
    │   └── Data Transfer Metrics (stored in instance)
    └── Periodic Save Thread (saves to JSON every 5 seconds)
```

## OpenVPN Statistics Collection

### 1. Stats Thread Implementation

Located in [`ur-vpn-mann/ur-openvpn-library/src/openvpn_wrapper.cpp`](rag://rag_source_1):

```cpp
void OpenVPNWrapper::statsLoop() {
    uint64_t last_bytes_sent = 0;
    uint64_t last_bytes_received = 0;
    auto last_update_time = std::chrono::steady_clock::now();
    
    while (running_) {
        auto current_time = std::chrono::steady_clock::now();
        auto elapsed_seconds = std::chrono::duration_cast<std::chrono::seconds>(
            current_time - last_update_time).count();
        
        if (elapsed_seconds > 0) {
            updateStats();
            
            VPNStats stats_copy;
            // Calculate bandwidth rates (bytes per second)
            {
                std::lock_guard<std::mutex> lock(stats_mutex_);
                
                uint64_t bytes_sent_diff = current_stats_.bytes_sent - last_bytes_sent;
                uint64_t bytes_received_diff = current_stats_.bytes_received - last_bytes_received;
                
                // Store instantaneous bandwidth (bytes/sec)
                current_stats_.upload_rate_bps = bytes_sent_diff / elapsed_seconds;
                current_stats_.download_rate_bps = bytes_received_diff / elapsed_seconds;
                
                last_bytes_sent = current_stats_.bytes_sent;
                last_bytes_received = current_stats_.bytes_received;
                
                stats_copy = current_stats_;
            }
            
            last_update_time = current_time;

            if (stats_callback_) {
                stats_callback_(stats_copy);
            }
        }

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}
```

**Key Implementation Details:**

- **Update Interval**: Stats are updated every 1 second
- **Rate Calculation**: Bandwidth rates are calculated by measuring the difference in cumulative bytes between updates
- **Thread Safety**: Uses `stats_mutex_` to protect shared data
- **Callback Mechanism**: Invokes `stats_callback_` to notify the instance manager

### 2. Bridge Communication

The `updateStats()` function retrieves cumulative byte counters from the OpenVPN bridge:

```cpp
void OpenVPNWrapper::updateStats() {
    if (!bridge_ctx_) {
        return;
    }

    std::lock_guard<std::mutex> lock(stats_mutex_);

    openvpn_bridge_stats_t bridge_stats;
    if (openvpn_bridge_get_stats(bridge_ctx_, &bridge_stats) == 0) {
        current_stats_.bytes_sent = bridge_stats.bytes_sent;
        current_stats_.bytes_received = bridge_stats.bytes_received;
        current_stats_.tun_read_bytes = bridge_stats.tun_read_bytes;
        current_stats_.tun_write_bytes = bridge_stats.tun_write_bytes;
        current_stats_.ping_ms = bridge_stats.ping_ms;
        // ... other stats
    }
}
```

**Data Flow:**
1. OpenVPN kernel/userspace → C Bridge → C++ Wrapper
2. Bridge provides **cumulative** byte counters
3. Wrapper calculates **rates** from cumulative values

## WireGuard Statistics Collection

### 1. Stats Thread Implementation

Located in [`ur-vpn-mann/ur-wg_library/wireguard-wrapper/src/wireguard_wrapper.cpp`](rag://rag_source_0):

```cpp
void WireGuardWrapper::statsLoop() {
    uint64_t last_bytes_sent = 0;
    uint64_t last_bytes_received = 0;
    auto last_update_time = std::chrono::steady_clock::now();
    
    while (running_) {
        auto current_time = std::chrono::steady_clock::now();
        auto elapsed_seconds = std::chrono::duration_cast<std::chrono::seconds>(
            current_time - last_update_time).count();
        
        if (elapsed_seconds > 0) {
            updateStats();
            
            VPNStats stats_copy;
            // Calculate bandwidth rates (bytes per second)
            {
                std::lock_guard<std::mutex> lock(stats_mutex_);
                
                uint64_t bytes_sent_diff = current_stats_.bytes_sent - last_bytes_sent;
                uint64_t bytes_received_diff = current_stats_.bytes_received - last_bytes_received;
                
                // Store instantaneous bandwidth (bytes/sec)
                current_stats_.upload_rate_bps = bytes_sent_diff / elapsed_seconds;
                current_stats_.download_rate_bps = bytes_received_diff / elapsed_seconds;
                
                last_bytes_sent = current_stats_.bytes_sent;
                last_bytes_received = current_stats_.bytes_received;
                
                stats_copy = current_stats_;
            }
            
            last_update_time = current_time;
            
            if (stats_callback_) {
                stats_callback_(stats_copy);
            }
        }
        
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}
```

**Implementation matches OpenVPN:**
- Same 1-second update interval
- Same rate calculation algorithm
- Same thread-safety pattern

### 2. WireGuard Bridge Communication

The `updateStats()` retrieves data from WireGuard kernel interface via IPC:

```cpp
void WireGuardWrapper::updateStats() {
    if (!bridge_ctx_) return;
    
    wireguard_bridge_stats_t stats;
    if (wireguard_bridge_get_stats(bridge_ctx_, &stats) == 0) {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        
        current_stats_.bytes_sent = stats.bytes_sent;
        current_stats_.bytes_received = stats.bytes_received;
        current_stats_.tx_packets = stats.tx_packets;
        current_stats_.rx_packets = stats.rx_packets;
        current_stats_.last_handshake = stats.last_handshake;
        // ... other stats
    }
}
```

**WireGuard-Specific Data Sources** ([`wireguard_c_bridge.c`](rag://rag_source_4)):
- Uses `ipc_get_device()` to query WireGuard kernel module
- Retrieves peer statistics including handshake times
- Reads endpoint information directly from kernel

## Instance Manager Integration

### 1. Stats Callback Registration

Located in [`ur-vpn-mann/src/vpn_instance_manager.cpp`](rag://rag_source_1):

```cpp
// For OpenVPN
wrapper->setStatsCallback([this, &instance](const openvpn::VPNStats& stats) {
    // Update real-time bandwidth rates (bytes per second)
    instance.data_transfer.upload_bytes = stats.upload_rate_bps;
    instance.data_transfer.download_bytes = stats.download_rate_bps;

    // Update session totals (cumulative bytes)
    instance.total_data_transferred.current_session_bytes = 
        stats.bytes_sent + stats.bytes_received;

    // Update connection time
    if (instance.connection_time.current_session_start > 0) {
        instance.connection_time.current_session_seconds = 
            time(nullptr) - instance.connection_time.current_session_start;
    }

    // Emit formatted event
    json data;
    data["upload_rate_bps"] = stats.upload_rate_bps;
    data["download_rate_bps"] = stats.download_rate_bps;
    data["upload_rate_formatted"] = formatBytes(stats.upload_rate_bps) + "/s";
    data["download_rate_formatted"] = formatBytes(stats.download_rate_bps) + "/s";
    // ... more fields
    
    emitEvent(instance.name, "stats", "Statistics update", data);
});
```

**Key Mappings:**
- `instance.data_transfer.upload_bytes` ← `stats.upload_rate_bps` (current rate)
- `instance.data_transfer.download_bytes` ← `stats.download_rate_bps` (current rate)
- `instance.total_data_transferred.current_session_bytes` ← cumulative total

### 2. Data Structure in VPNInstance

Defined in [`ur-vpn-mann/src/vpn_instance_manager.hpp`](rag://file_context_0):

```cpp
struct VPNInstance {
    // Real-time bandwidth rates (updated every second)
    struct {
        uint64_t upload_bytes;   // Actually stores upload_rate_bps
        uint64_t download_bytes; // Actually stores download_rate_bps
    } data_transfer;

    // Cumulative session data
    struct {
        uint64_t current_session_bytes; // Bytes in current session
        uint64_t total_bytes;           // Total bytes since creation
    } total_data_transferred;

    // Connection time tracking
    struct {
        time_t current_session_start;
        uint64_t current_session_seconds;
        uint64_t total_seconds;
    } connection_time;
};
```

## JSON Persistence Mechanism

### 1. Periodic Save Thread

The instance manager runs a background thread that saves configuration every 5 seconds:

```cpp
VPNInstanceManager::VPNInstanceManager() {
    // ... initialization
    
    // Start periodic config save thread
    config_save_thread_ = std::thread([this]() {
        while (running_) {
            std::this_thread::sleep_for(std::chrono::seconds(5));
            
            if (config_save_pending_.exchange(false)) {
                if (!config_file_path_.empty()) {
                    saveConfiguration(config_file_path_);
                }
            }
        }
    });
}
```

### 2. Triggering Saves

Statistics updates are marked for save when significant events occur:

```cpp
bool VPNInstanceManager::stopInstance(const std::string& instance_id) {
    // ... disconnect logic
    
    {
        std::lock_guard<std::mutex> lock(instances_mutex_);
        auto it = instances_.find(instance_id);
        if (it != instances_.end()) {
            // Update total metrics before disconnecting
            it->second.total_data_transferred.total_bytes += 
                it->second.total_data_transferred.current_session_bytes;
            it->second.connection_time.total_seconds += 
                it->second.connection_time.current_session_seconds;

            // Mark for save (deferred to background thread)
            config_save_pending_.store(true);
        }
    }
    
    return true;
}
```

### 3. JSON Structure

The `saveConfiguration()` function writes stats to JSON:

```cpp
bool VPNInstanceManager::saveConfiguration(const std::string& filepath) {
    json config;
    json profiles_array = json::array();

    for (const auto& [id, inst] : instances_) {
        json profile;
        // ... basic config
        
        // Data transfer metrics (real-time rates)
        json data_transfer;
        data_transfer["upload_bytes"] = inst.data_transfer.upload_bytes;
        data_transfer["download_bytes"] = inst.data_transfer.download_bytes;
        data_transfer["upload_formatted"] = formatBytes(inst.data_transfer.upload_bytes);
        data_transfer["download_formatted"] = formatBytes(inst.data_transfer.download_bytes);
        profile["data_transfer"] = data_transfer;

        // Total data transferred (cumulative)
        json total_data;
        total_data["current_session_bytes"] = inst.total_data_transferred.current_session_bytes;
        total_data["total_bytes"] = inst.total_data_transferred.total_bytes;
        total_data["total_mb"] = inst.total_data_transferred.total_bytes / (1024.0 * 1024.0);
        profile["total_data_transferred"] = total_data;

        // Connection time
        json conn_time;
        conn_time["total_seconds"] = inst.connection_time.total_seconds;
        conn_time["total_formatted"] = formatTime(inst.connection_time.total_seconds);
        profile["connection_time"] = conn_time;
        
        profiles_array.push_back(profile);
    }

    config["vpn_profiles"] = profiles_array;
    
    std::ofstream file(filepath);
    file << config.dump(2);
    file.close();
    
    return true;
}
```

## Real-Time Stats Flow Diagram

```
┌─────────────────────────────────────────────────────────────┐
│                    VPN Kernel/Userspace                      │
│  (cumulative bytes_sent, bytes_received, packets, etc.)     │
└────────────────────────────┬────────────────────────────────┘
                             │
                             ▼
┌─────────────────────────────────────────────────────────────┐
│                      C Bridge Layer                          │
│  openvpn_bridge_get_stats() / wireguard_bridge_get_stats()  │
│  Returns cumulative counters                                 │
└────────────────────────────┬────────────────────────────────┘
                             │
                             ▼
┌─────────────────────────────────────────────────────────────┐
│                   C++ Wrapper Stats Thread                   │
│  • Runs every 1 second                                       │
│  • Calculates: rate = (current - previous) / elapsed_time   │
│  • Stores rates in upload_rate_bps, download_rate_bps       │
└────────────────────────────┬────────────────────────────────┘
                             │
                             ▼ stats_callback_()
┌─────────────────────────────────────────────────────────────┐
│                  VPNInstanceManager                          │
│  • Receives VPNStats with rates (bps)                       │
│  • Updates instance.data_transfer (rates for display)       │
│  • Updates instance.total_data_transferred (cumulative)     │
│  • Emits JSON event with formatted stats                    │
└────────────────────────────┬────────────────────────────────┘
                             │
                             ▼
┌─────────────────────────────────────────────────────────────┐
│                  Periodic Save Thread                        │
│  • Runs every 5 seconds                                      │
│  • Checks config_save_pending_ flag                         │
│  • Writes all instance metrics to config.json               │
└─────────────────────────────────────────────────────────────┘
```

## Summary

### Statistics Collection Flow

1. **Every 1 Second (Stats Thread)**:
   - Wrapper retrieves cumulative counters from bridge
   - Calculates bandwidth rates from delta values
   - Invokes stats callback with calculated rates

2. **On Stats Callback (Instance Manager)**:
   - Updates instance bandwidth rates (for display)
   - Updates cumulative session totals
   - Updates connection time
   - Emits JSON event to subscribers

3. **Every 5 Seconds (Save Thread)**:
   - Checks if save is pending
   - Writes all metrics to JSON file
   - Preserves both real-time and historical data

4. **On Disconnect**:
   - Accumulates session totals into lifetime totals
   - Triggers immediate save flag
   - Background thread saves within 5 seconds

### Key Design Principles

- **Separation of Concerns**: Rate calculation in wrappers, aggregation in manager
- **Thread Safety**: Mutexes protect all shared statistics data
- **Performance**: Deferred saves prevent blocking VPN threads
- **Consistency**: Same implementation pattern for OpenVPN and WireGuard
- **Real-time Updates**: 1-second granularity for live monitoring
- **Persistence**: 5-second saves ensure data durability without overhead
