#!/bin/bash

# Modem Diagnostics Script for QMI Devices
# Usage: sudo ./modem-diagnostics.sh [device]

DEVICE=${1:-"/dev/cdc-wdm0"}

echo "=== QMI Modem Diagnostics for $DEVICE ==="
echo

# Check if device exists
if [ ! -e "$DEVICE" ]; then
    echo "❌ ERROR: Device $DEVICE not found"
    exit 1
fi

echo "🔍 1. Basic Device Identification"
echo "-----------------------------------"
sudo qmicli -d "$DEVICE" --dms-get-ids
echo

echo "🔍 2. Device Capabilities"
echo "------------------------"
sudo qmicli -d "$DEVICE" --dms-get-capabilities
echo

echo "🔍 3. Manufacturer & Model Info"
echo "------------------------------"
sudo qmicli -d "$DEVICE" --dms-get-manufacturer
sudo qmicli -d "$DEVICE" --dms-get-model
echo

echo "🔍 4. Power & Operating State"
echo "----------------------------"
sudo qmicli -d "$DEVICE" --dms-get-power-state
sudo qmicli -d "$DEVICE" --dms-get-operating-mode
echo

echo "🔍 5. SIM Card Status (Comprehensive)"
echo "------------------------------------"
sudo qmicli -d "$DEVICE" --uim-get-card-status
echo

echo "🔍 6. Network Registration Status"
echo "---------------------------------"
sudo qmicli -d "$DEVICE" --nas-get-registration-state
echo

echo "🔍 7. Signal Strength"
echo "-------------------"
sudo qmicli -d "$DEVICE" --nas-get-signal-info
echo

echo "🔍 8. Serving System Info"
echo "------------------------"
sudo qmicli -d "$DEVICE" --nas-get-serving-system
echo

echo "🔍 9. Firmware & Hardware Info"
echo "-----------------------------"
sudo qmicli -d "$DEVICE" --dms-get-revision
sudo qmicli -d "$DEVICE" --dms-get-hardware-revision
sudo qmicli -d "$DEVICE" --dms-get-software-version
echo

echo "🔍 10. Band Capabilities"
echo "-----------------------"
sudo qmicli -d "$DEVICE" --dms-get-band-capabilities
echo

echo "🔍 11. Time Information"
echo "----------------------"
sudo qmicli -d "$DEVICE" --dms-get-time
echo

echo "🔍 12. Factory Information"
echo "------------------------"
sudo qmicli -d "$DEVICE" --dms-get-factory-sku
echo

echo "🔍 13. User Lock State"
echo "---------------------"
sudo qmicli -d "$DEVICE" --dms-get-user-lock-state
echo

echo "=== Diagnostics Complete ==="
echo
echo "💡 PIN Status Summary:"
echo "   - PIN1 disabled: SIM is unlocked, no PIN required"
echo "   - PIN1 enabled-not-verified: SIM is locked, PIN required"
echo "   - PIN1 enabled-verified: PIN was entered successfully"
echo "   - Check retries remaining to avoid SIM lockout"
echo
echo "💡 Troubleshooting Tips:"
echo "   - If 'error: couldn't open the QmiDevice': Check permissions (use sudo)"
echo "   - If 'DeviceUnsupported': Command not supported by this modem"
echo "   - If 'Card state: absent': No SIM card inserted"
echo "   - If PIN retries are low: Avoid incorrect PIN attempts"
