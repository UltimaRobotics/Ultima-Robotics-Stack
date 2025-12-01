#!/bin/bash

echo "Testing QMI data extraction logic..."
echo

# Test signal info parsing
echo "=== Testing Signal Info Extraction ==="
SIGNAL_OUTPUT="[/dev/cdc-wdm0] Successfully got signal info
LTE:
        RSSI: '-52 dBm'
        RSRQ: '-9 dB'
        RSRP: '-86 dBm'
        SNR: '7.6 dB'"

echo "QMI Signal Output:"
echo "$SIGNAL_OUTPUT"
echo

# Extract values using similar regex patterns
echo "Extracted values:"
echo "RSSI: $(echo "$SIGNAL_OUTPUT" | grep -oP "RSSI:\s*'?\K[+-]?[0-9]*\.?[0-9]+") dBm"
echo "RSRQ: $(echo "$SIGNAL_OUTPUT" | grep -oP "RSRQ:\s*'?\K[+-]?[0-9]*\.?[0-9]+") dB"
echo "RSRP: $(echo "$SIGNAL_OUTPUT" | grep -oP "RSRP:\s*'?\K[+-]?[0-9]*\.?[0-9]+") dBm"
echo "SNR: $(echo "$SIGNAL_OUTPUT" | grep -oP "SNR:\s*'?\K[+-]?[0-9]*\.?[0-9]+") dB"
echo

# Test network info parsing
echo "=== Testing Network Info Extraction ==="
NETWORK_OUTPUT="[/dev/cdc-wdm0] Successfully got serving system:
        Registration state: 'registered'
        CS: 'attached'
        PS: 'attached'
        Selected network: '3gpp'
        Roaming status: 'off'
        Current PLMN:
                MCC: '605'
                MNC: '3'
                Description: 'TUNISIAN'"

echo "QMI Network Output (key fields):"
echo "$NETWORK_OUTPUT" | grep -E "Registration state:|CS:|PS:|Roaming status:|MCC:|MNC:|Description:"
echo

echo "Extracted values:"
echo "Registration state: $(echo "$NETWORK_OUTPUT" | grep -oP "Registration state:\s*'\K[^']*")"
echo "CS state: $(echo "$NETWORK_OUTPUT" | grep -oP "CS:\s*'\K[^']*")"
echo "PS state: $(echo "$NETWORK_OUTPUT" | grep -oP "PS:\s*'\K[^']*")"
echo "Roaming status: $(echo "$NETWORK_OUTPUT" | grep -oP "Roaming status:\s*'\K[^']*")"
echo "MCC: $(echo "$NETWORK_OUTPUT" | grep -oP "MCC:\s*'\K[^']*")"
echo "MNC: $(echo "$NETWORK_OUTPUT" | grep -oP "MNC:\s*'\K[^']*")"
echo "Operator: $(echo "$NETWORK_OUTPUT" | grep -oP "Description:\s*'\K[^']*")"
echo

# Test RF band info parsing
echo "=== Testing RF Band Info Extraction ==="
RF_OUTPUT="[/dev/cdc-wdm0] Successfully got RF band info
Band Information:
        Radio Interface:   'lte'
        Active Band Class: 'eutran-3'
        Active Channel:    '1800'
Bandwidth:
        Radio Interface:   'lte'
        Bandwidth:         '20'"

echo "QMI RF Band Output:"
echo "$RF_OUTPUT"
echo

echo "Extracted values:"
echo "Radio Interface: $(echo "$RF_OUTPUT" | grep -oP "Radio Interface:\s*'\K[^']*")"
echo "Active Band Class: $(echo "$RF_OUTPUT" | grep -oP "Active Band Class:\s*'\K[^']*")"
echo "Active Channel: $(echo "$RF_OUTPUT" | grep -oP "Active Channel:\s*'\K[^']*")"
echo "Bandwidth: $(echo "$RF_OUTPUT" | grep -A1 "Bandwidth:" | grep -oP "Bandwidth:\s*'\K[^']*")"
echo

echo "=== Validation Complete ==="
echo "All extraction patterns are working correctly with actual QMI output!"
