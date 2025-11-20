
#!/bin/bash

echo "=== Clean Restart QMI Connection ==="

QMI_DEVICE="/dev/cdc-wdm3"

# Step 1: Kill existing connection manager
echo "1. Stopping existing connection manager..."
sudo pkill -f "qmi_connection_manager"
sleep 2

# Step 2: Clean up QMI sessions
echo "2. Cleaning up QMI sessions..."
for cid in {1..30}; do
    qmicli -d "$QMI_DEVICE" --wds-stop-network="$cid" --client-cid="$cid" 2>/dev/null && echo "Stopped session CID $cid"
done

# Step 3: Reset interfaces
echo "3. Resetting network interfaces..."
sudo ip addr flush dev wwan0 2>/dev/null
sudo ip addr flush dev wwan1 2>/dev/null
sudo ip link set wwan0 down 2>/dev/null
sudo ip link set wwan1 down 2>/dev/null

# Step 4: Restart connection
echo "4. Restarting connection manager..."
cd /home/runner/workspace/connection-manager
sudo ./build/qmi_connection_manager config/config.json config/dev-lte.json

