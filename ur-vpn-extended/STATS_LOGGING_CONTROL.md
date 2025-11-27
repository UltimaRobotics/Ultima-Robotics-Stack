# Stats Logging Control

## Overview

The ur-vpn-manager now supports controllable stats logging for OpenVPN and WireGuard instances through configuration flags in the master config file. This allows you to enable or disable statistics logging independently for each VPN type.

## Configuration

### Master Config File Structure

Add the `stats_logging` section to your `master-config.json`:

```json
{
  "config_file_path": "config/config.json",
  "cached_data_path": "config/cached-data.json",
  "custom_routing_rules": "config/routing-rules.json",
  "http_server": {
    "enabled": true,
    "host": "0.0.0.0",
    "port": 5000
  },
  "verbose": true,
  "stats_logging": {
    "enabled": true,
    "openvpn": true,
    "wireguard": true
  }
}
```

### Configuration Options

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `enabled` | boolean | `true` | Global stats logging enable/disable flag |
| `openvpn` | boolean | `true` | Enable/disable OpenVPN stats logging |
| `wireguard` | boolean | `true` | Enable/disable WireGuard stats logging |

## Usage Examples

### Enable All Stats Logging (Default)
```json
"stats_logging": {
  "enabled": true,
  "openvpn": true,
  "wireguard": true
}
```

### Disable All Stats Logging
```json
"stats_logging": {
  "enabled": false,
  "openvpn": false,
  "wireguard": false
}
```

### Enable Only OpenVPN Stats
```json
"stats_logging": {
  "enabled": true,
  "openvpn": true,
  "wireguard": false
}
```

### Enable Only WireGuard Stats
```json
"stats_logging": {
  "enabled": true,
  "openvpn": false,
  "wireguard": true
}
```

## Behavior

### When Stats Logging is Disabled

- **No stats events emitted**: The system will not generate or emit statistics update events
- **Reduced log volume**: Fewer JSON messages will be generated and logged
- **Performance improvement**: Slight performance improvement due to reduced event processing
- **Internal tracking continues**: Internal statistics tracking for management operations continues to work

### When Stats Logging is Enabled

- **Regular stats events**: Statistics update events are emitted as JSON messages
- **Real-time monitoring**: Enables real-time monitoring of VPN connection statistics
- **Full functionality**: All stats-related features work normally

## Sample Output

### Stats Event Format

When enabled, stats events are emitted in this format:

```json
{
  "data": {
    "connection_time": "0h 0m 0s",
    "download_bytes": 0,
    "download_formatted": "0.00 B",
    "download_rate_bps": 0,
    "download_rate_formatted": "0.00 B/s",
    "ping_ms": 0,
    "total_session_mb": 0.000102996826171875,
    "upload_bytes": 108,
    "upload_formatted": "108.00 B",
    "upload_rate_bps": 0,
    "upload_rate_formatted": "0.00 B/s"
  },
  "instance": "ovpn0",
  "message": "Statistics update",
  "timestamp": 1764248704,
  "type": "stats"
}
```

## Runtime Control

The stats logging configuration is read at startup from the master config file. To change the settings:

1. Stop the ur-vpn-manager service
2. Update the `stats_logging` section in `master-config.json`
3. Restart the ur-vpn-manager service

## Predefined Configuration Files

The following predefined configuration files are available for testing:

- `master-config.json` - All stats logging enabled (default)
- `master-config-no-stats.json` - All stats logging disabled
- `master-config-openvpn-only.json` - Only OpenVPN stats enabled
- `master-config-wireguard-only.json` - Only WireGuard stats enabled

### Using Predefined Configs

```bash
# Use config with no stats logging
./ur-vpn-manager -pkg_config config/master-config-no-stats.json -rpc_config config/ur-rpc-config.json

# Use config with only OpenVPN stats
./ur-vpn-manager -pkg_config config/master-config-openvpn-only.json -rpc_config config/ur-rpc-config.json

# Use config with only WireGuard stats
./ur-vpn-manager -pkg_config config/master-config-wireguard-only.json -rpc_config config/ur-rpc-config.json
```

## Implementation Details

### Code Changes

The stats logging control is implemented through:

1. **Configuration parsing** in `main.cpp` - Reads stats_logging section from master config
2. **Control methods** in `VPNInstanceManager` - Provides getter/setter methods for stats flags
3. **Callback checks** in `vpn_manager_lifecycle.cpp` - Stats callbacks check flags before emitting events
4. **Member variables** in `VPNInstanceManager` - Atomic boolean flags for thread-safe control

### Thread Safety

All stats logging control flags are implemented as `std::atomic<bool>` to ensure thread-safe operation across multiple VPN instances and callback threads.

### Performance Impact

- **When disabled**: Minimal CPU overhead for stats processing
- **When enabled**: Normal stats processing overhead
- **Memory usage**: Negligible impact (3 atomic boolean variables)

## Troubleshooting

### Stats Not Appearing

1. Check that `stats_logging.enabled` is `true` in master config
2. Verify the specific VPN type flag (`openvpn` or `wireguard`) is `true`
3. Ensure the VPN instance is running and connected
4. Check that verbose mode is enabled to see configuration loading messages

### Configuration Not Applied

1. Verify the master config file path is correct
2. Check JSON syntax validity of the config file
3. Look for configuration loading error messages on startup
4. Restart the service after making changes

### Unexpected Behavior

1. Check the verbose output for stats logging configuration messages
2. Verify the VPN instance type matches the expected flag
3. Ensure all three flags are properly set (enabled, openvpn, wireguard)
