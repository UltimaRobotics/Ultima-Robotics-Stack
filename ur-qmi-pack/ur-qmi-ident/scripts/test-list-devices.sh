#!/bin/bash

# Test script for the list_devices RPC method
# Usage: ./test-list-devices.sh

echo "=== Testing ur-qmi-ident list_devices RPC method ==="
echo

# Configuration
REQUEST_TOPIC="direct_messaging/ur-qmi-ident/requests"
RESPONSE_TOPIC="direct_messaging/ur-qmi-ident/responses"
BROKER_HOST="0.0.0.0"
BROKER_PORT="1899"

echo "🔍 Publishing list_devices request to: $REQUEST_TOPIC"
echo "📥 Listening for response on: $RESPONSE_TOPIC"
echo

# Create the JSON-RPC 2.0 request
REQUEST_JSON='{
  "jsonrpc": "2.0",
  "method": "list_devices",
  "id": "test_list_devices_'$(date +%s)'",
  "params": {}
}'

echo "📤 Request JSON:"
echo "$REQUEST_JSON" | jq '.'
echo

# Publish the request and listen for response
echo "🔄 Sending request..."
mosquitto_pub -h "$BROKER_HOST" -p "$BROKER_PORT" -t "$REQUEST_TOPIC" -m "$REQUEST_JSON"

echo "⏳ Waiting for response..."
sleep 2

# Listen for response (timeout after 10 seconds)
echo "📥 Response received:"
mosquitto_sub -h "$BROKER_HOST" -p "$BROKER_PORT" -t "$RESPONSE_TOPIC" -C 1 -W 10 | jq '.' 2>/dev/null || echo "No response received or timeout"

echo
echo "=== Test Complete ==="
echo
echo "💡 Available RPC methods:"
echo "   - list_devices: Returns all devices in the system (new)"
echo "   - get_devices: Returns QMIDevice objects (MANAGER mode)"
echo "   - get_device_details: Get details for specific device"
echo "   - get_device_count: Returns number of devices"
echo "   - scan_devices: Trigger device rescan"
echo
echo "💡 list_devices response format:"
echo "   {"
echo "     \"devices\": ["
echo "       {"
echo "         \"path\": \"/dev/cdc-wdm0\","
echo "         \"imei\": \"123456789012345\","
echo "         \"model\": \"Model-Name\","
echo "         \"manufacturer\": \"Manufacturer\","
echo "         \"firmware\": \"Firmware-Version\","
echo "         \"profile_type\": \"advanced|basic\","
echo "         \"sim_present\": true,"
echo "         \"pin_locked\": false,"
echo "         \"gps_supported\": true,"
echo "         \"power_state\": \"external-source\","
echo "         \"operating_mode\": \"online\","
echo "         \"hardware_revision\": \"V1.0.1\","
echo "         \"iccid\": \"8988...\","
echo "         \"pin_status\": \"Disabled\","
echo "         \"...\": \"...\""
echo "       }"
echo "     ],"
echo "     \"count\": 1,"
echo "     \"timestamp\": \"1638360000\""
echo "   }"
