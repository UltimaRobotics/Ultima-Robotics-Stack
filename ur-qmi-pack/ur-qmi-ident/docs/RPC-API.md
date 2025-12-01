# ur-qmi-ident RPC API Documentation

## Overview

The ur-qmi-ident application provides a JSON-RPC 2.0 API over MQTT for querying QMI device information and managing device discovery operations.

## Configuration

- **Request Topic**: `direct_messaging/ur-qmi-ident/requests`
- **Response Topic**: `direct_messaging/ur-qmi-ident/responses`
- **Broker**: Configured in `ur-rpc-config.json` (default: `0.0.0.0:1899`)

## RPC Methods

### 1. list_devices ⭐ (NEW)

Returns a comprehensive list of all QMI devices discovered in the system, with automatic fallback between advanced and basic device profiles.

**Request:**
```json
{
  "jsonrpc": "2.0",
  "method": "list_devices",
  "id": "unique_request_id",
  "params": {}
}
```

**Response:**
```json
{
  "jsonrpc": "2.0",
  "id": "unique_request_id",
  "success": true,
  "result": {
    "devices": [
      {
        "path": "/dev/cdc-wdm0",
        "imei": "869816054518831",
        "model": "NL668-EAU-10-01",
        "manufacturer": "Fibocom Wireless Inc.",
        "firmware": "19011.1000.00.02.73.13",
        "bands": [],
        "sim_present": true,
        "pin_locked": true,
        "gps_supported": true,
        "max_carriers": 252,
        "profile_type": "advanced",
        "msisdn": "",
        "power_state": "external-source",
        "hardware_revision": "V1.0.1",
        "operating_mode": "online",
        "prl_version": "",
        "activation_state": "",
        "user_lock_state": "Disabled",
        "band_capabilities": "",
        "factory_sku": "",
        "software_version": "",
        "iccid": "",
        "imsi": "",
        "uim_state": "Present",
        "pin_status": "Disabled",
        "time": "",
        "stored_images": [],
        "firmware_preference": "",
        "boot_image_download_mode": "",
        "usb_composition": "",
        "mac_address_wlan": "",
        "mac_address_bt": ""
      }
    ],
    "count": 1,
    "timestamp": "1764527632"
  }
}
```

**Features:**
- **Automatic Profile Detection**: Returns advanced device profiles when available, falls back to basic profiles
- **Comprehensive Data**: Includes all device information from SIM status to hardware details
- **Metadata**: Includes device count and timestamp
- **Error Handling**: Graceful fallback if advanced scanning fails

### 2. get_devices

Returns QMIDevice objects with full SIM status information (MANAGER mode only).

**Request:**
```json
{
  "jsonrpc": "2.0",
  "method": "get_devices",
  "id": "unique_request_id",
  "params": {}
}
```

### 3. get_device_details

Returns detailed information for a specific device.

**Request:**
```json
{
  "jsonrpc": "2.0",
  "method": "get_device_details",
  "id": "unique_request_id",
  "params": {
    "device_path": "/dev/cdc-wdm0"
  }
}
```

### 4. get_device_count

Returns the number of discovered devices.

**Request:**
```json
{
  "jsonrpc": "2.0",
  "method": "get_device_count",
  "id": "unique_request_id",
  "params": {}
}
```

### 5. scan_devices

Triggers a device rescan operation.

**Request:**
```json
{
  "jsonrpc": "2.0",
  "method": "scan_devices",
  "id": "unique_request_id",
  "params": {}
}
```

## Device Discovery Events

The application also publishes device discovery events to the watchdog topic:

**Topic**: `direct_messaging/ur-qmi-qmi-watchdog/requests`

**Event Format:**
```json
{
  "jsonrpc": "2.0",
  "method": "device_discovery_event",
  "id": "qmi_discovery_1764527632711",
  "params": {
    "event_type": "device_added|device_removed",
    "timestamp": "1764527632",
    "device_data": {
      // Complete device profile data (same as list_devices)
    }
  }
}
```

## Usage Examples

### Using mosquitto CLI

```bash
# List all devices
mosquitto_pub -h localhost -p 1899 -t "direct_messaging/ur-qmi-ident/requests" -m '{
  "jsonrpc": "2.0",
  "method": "list_devices",
  "id": "test_001",
  "params": {}
}'

# Listen for responses
mosquitto_sub -h localhost -p 1899 -t "direct_messaging/ur-qmi-ident/responses"
```

### Using the test script

```bash
# Run the provided test script
./scripts/test-list-devices.sh
```

## Error Responses

All methods return standardized error responses:

```json
{
  "jsonrpc": "2.0",
  "id": "request_id",
  "success": false,
  "error": "Error description"
}
```

## Device Profile Types

### Basic Profile
- Essential device information
- IMEI, model, firmware
- Basic SIM status
- GPS and carrier support

### Advanced Profile
- All basic profile data
- Manufacturer details
- Power and operating state
- Hardware/software versions
- Complete SIM/UIM status
- Network capabilities
- MAC addresses

## Troubleshooting

1. **No Response**: Check if ur-qmi-ident is running and MQTT broker is accessible
2. **Empty Device List**: Verify QMI device permissions (run with sudo)
3. **Scanner Not Available**: Ensure the scanner is properly initialized
4. **Permission Denied**: Check udev rules or run with appropriate privileges

## Integration with ur-qmi-watchdog

The application automatically publishes device discovery events to the watchdog system, enabling real-time monitoring and automated responses to device changes in the network.
