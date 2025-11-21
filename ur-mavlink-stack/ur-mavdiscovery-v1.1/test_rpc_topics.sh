#!/bin/bash

echo "Testing RPC topic routing..."
echo "Starting ur-mavdiscovery in background..."

# Clear the log
> mavdiscovery.log

# Start the application in background
./build/ur-mavdiscovery -rpc_config config/rpc-config.json -package_config config/config.json &
APP_PID=$!

# Wait for it to initialize
sleep 3

echo "Application started with PID: $APP_PID"

# Let it run for a bit to collect logs
sleep 5

# Stop the application
kill $APP_PID
wait $APP_PID 2>/dev/null

echo "Application stopped. Checking logs for RPC requests..."

# Check for the correct fixed topics
echo ""
echo "=== Checking for correct direct_messaging topics ==="
grep -E "direct_messaging/ur-mavrouter/requests|direct_messaging/ur-mavcollector/requests" mavdiscovery.log

echo ""
echo "=== Checking for any transaction ID topics (should be empty) ==="
grep -E "ur_rpc/.*/request/[a-f0-9-]+" mavdiscovery.log

echo ""
echo "=== Checking RPC debug logs ==="
grep -E "\[RPC\].*topic" mavdiscovery.log | head -10

echo ""
echo "=== Test completed ==="
