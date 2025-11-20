
# VPN Instance Manager HTTP API Test Scripts

This directory contains Python test scripts for testing each HTTP operation of the VPN Instance Manager.

## Prerequisites

- Python 3.6+
- `requests` library: `pip install requests`
- VPN Instance Manager running with HTTP server enabled on port 8080

## Starting the VPN Instance Manager

```bash
cd ur-vpn-mann
./build/vpn-instance-manager config/master-config.json
```

Make sure `config/master-config.json` has HTTP server enabled:
```json
{
  "config_file_path": "config/example_config.json",
  "cached_data_path": "config/cached-data.json",
  "http_server": {
    "enabled": true,
    "host": "0.0.0.0",
    "port": 8080
  }
}
```

## Test Scripts Usage

### 1. Parse Configuration
```bash
python tests/test_parse.py <config_file_path>
```
Example:
```bash
python tests/test_parse.py config/example.ovpn
```

### 2. Add VPN Instance
```bash
python tests/test_add.py <instance_name> <config_file_path> [vpn_type] [auto_start]
```
Examples:
```bash
python tests/test_add.py my_vpn config/example.ovpn
python tests/test_add.py my_vpn config/example.ovpn openvpn true
python tests/test_add.py wg_vpn config/peer1.conf wireguard false
```

### 3. Start VPN Instance
```bash
python tests/test_start.py <instance_name>
```
Example:
```bash
python tests/test_start.py my_vpn
```

### 4. Stop VPN Instance
```bash
python tests/test_stop.py <instance_name>
```
Example:
```bash
python tests/test_stop.py my_vpn
```

### 5. Restart VPN Instance
```bash
python tests/test_restart.py <instance_name>
```
Example:
```bash
python tests/test_restart.py my_vpn
```

### 6. Get Status
```bash
# Get status of specific instance
python tests/test_status.py <instance_name>

# Get status of all instances
python tests/test_status.py
```
Examples:
```bash
python tests/test_status.py my_vpn
python tests/test_status.py
```

### 7. List Instances
```bash
# List all instances
python tests/test_list.py

# List instances by type
python tests/test_list.py <vpn_type>
```
Examples:
```bash
python tests/test_list.py
python tests/test_list.py openvpn
python tests/test_list.py wireguard
```

### 8. Update VPN Instance
```bash
python tests/test_update.py <instance_name> <config_file_path>
```
Example:
```bash
python tests/test_update.py my_vpn config/updated_config.ovpn
```

### 9. Delete VPN Instance
```bash
python tests/test_delete.py <instance_name> [vpn_type]
```
Examples:
```bash
python tests/test_delete.py my_vpn
python tests/test_delete.py my_vpn openvpn
```

### 10. Get Aggregated Statistics
```bash
python tests/test_stats.py
```

## Notes

- All scripts accept command-line arguments (no hardcoded data)
- Scripts return exit code 0 on success, 1 on failure
- Response is printed in JSON format for easy parsing
- Make sure the HTTP server is running before executing tests
