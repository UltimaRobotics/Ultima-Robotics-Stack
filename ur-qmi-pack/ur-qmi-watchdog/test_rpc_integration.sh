#!/bin/bash

echo "=== QMI Watchdog RPC Integration Test ==="
echo

# Test 1: Help message
echo "Test 1: Help message with RPC option"
./build/ur-qmi-watchdog -h
echo

# Test 2: Invalid arguments
echo "Test 2: Invalid arguments"
./build/ur-qmi-watchdog -invalid_option 2>&1 | head -5
echo

# Test 3: Missing package config
echo "Test 3: Missing package config"
./build/ur-qmi-watchdog -rpc_config config/ur-rpc-config.json 2>&1 | head -5
echo

# Test 4: RPC config validation (check if file exists)
echo "Test 4: RPC config file validation"
if [ -f "config/ur-rpc-config.json" ]; then
    echo "✅ RPC config file exists"
    echo "RPC config content:"
    cat config/ur-rpc-config.json | head -10
    echo "..."
else
    echo "❌ RPC config file not found"
fi
echo

# Test 5: Package config validation
echo "Test 5: Package config file validation"
if [ -f "config/advanced-config-lte.json" ]; then
    echo "✅ Package config file exists"
else
    echo "❌ Package config file not found"
fi
echo

echo "=== RPC Integration Test Complete ==="
