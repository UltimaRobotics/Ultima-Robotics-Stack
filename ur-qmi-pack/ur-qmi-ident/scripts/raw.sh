
#!/bin/bash

# QMI Device Scanner Command Test Script
# Tests all commands used in basic and advanced modes

echo -e "\033[0;32mQMI Device Scanner Command Test Script\033[0m"
echo -e "\033[0;32m======================================\033[0m"

# Find QMI devices
echo -e "\033[0;34mSearching for QMI devices...\033[0m"
QMI_DEVICES=$(ls /dev/cdc-wdm* 2>/dev/null)

if [ -z "$QMI_DEVICES" ]; then
    echo -e "\033[0;31mNo QMI devices found!\033[0m"
    echo "Make sure your QMI device is connected and recognized by the system."
    exit 1
fi

for device in $QMI_DEVICES; do
    echo -e "\033[0;32mFound QMI device: $device\033[0m"
done

# System diagnostic commands
echo -e "\n\033[0;34m========================================\033[0m"
echo -e "\033[0;34m SYSTEM DIAGNOSTIC COMMANDS\033[0m"
echo -e "\033[0;34m========================================\033[0m"

run_command() {
    local cmd="$1"
    local desc="$2"
    echo -e "\n\033[1;33mCommand:\033[0m $cmd"
    echo -e "\033[1;33mDescription:\033[0m $desc"
    echo -e "\033[0;32mResponse:\033[0m"
    echo "----------------------------------------"
    if eval "$cmd" 2>/dev/null; then
        :
    else
        echo -e "\033[0;31mCommand failed with exit code $?\033[0m"
        eval "$cmd"
    fi
    echo "----------------------------------------"
}

run_command "qmicli --help-all | head -20" "QMI CLI help (first 20 lines)"
run_command "ls -la /dev/cdc-wdm*" "List QMI device files"
run_command "lsmod | grep qmi" "Show loaded QMI kernel modules"
run_command "dmesg | grep -i qmi | tail -10" "Recent QMI-related kernel messages"

# Network interface commands
echo -e "\n\033[0;34m========================================\033[0m"
echo -e "\033[0;34m NETWORK INTERFACE COMMANDS\033[0m"
echo -e "\033[0;34m========================================\033[0m"

run_command "ip link show" "Show all network interfaces"
run_command "cat /sys/class/net/wlan0/address 2>/dev/null || echo 'wlan0 not found'" "Get WLAN MAC address"
run_command "cat /sys/class/net/hci0/address 2>/dev/null || echo 'hci0 not found'" "Get Bluetooth MAC address"
run_command "lsusb | grep -i 'modem\|cellular\|qualcomm\|sierra\|huawei\|zte'" "List USB cellular modems"

# Test each QMI device
for device in $QMI_DEVICES; do
    echo -e "\n\033[0;34mTesting device: $device\033[0m"
    
    # Basic mode commands (these should work on most devices)
    echo -e "\n\033[0;34m========================================\033[0m"
    echo -e "\033[0;34m BASIC MODE COMMANDS for $device\033[0m"
    echo -e "\033[0;34m========================================\033[0m"
    
    run_command "qmicli -d $device --dms-get-ids" "Get device IDs (IMEI, ESN, MEID)"
    run_command "qmicli -d $device --dms-get-model" "Get device model information"
    run_command "qmicli -d $device --dms-get-revision" "Get firmware revision/version"
    run_command "qmicli -d $device --dms-get-capabilities" "Get device capabilities (bands, features)"
    run_command "qmicli -d $device --uim-get-card-status" "Get SIM/UIM card status and PIN state"
    
    # Advanced mode commands (may fail on some devices)
    echo -e "\n\033[0;34m========================================\033[0m"
    echo -e "\033[0;34m ADVANCED MODE COMMANDS for $device\033[0m"
    echo -e "\033[0;34m========================================\033[0m"
    
    run_command "qmicli -d $device --dms-get-manufacturer" "Get device manufacturer"
    run_command "qmicli -d $device --dms-get-msisdn" "Get MSISDN (phone number)"
    run_command "qmicli -d $device --dms-get-power-state" "Get device power state"
    run_command "qmicli -d $device --dms-get-hardware-revision" "Get hardware revision"
    run_command "qmicli -d $device --dms-get-operating-mode" "Get operating mode (online/offline)"
    run_command "qmicli -d $device --dms-get-time" "Get system time"
    run_command "qmicli -d $device --dms-get-band-capabilities" "Get detailed band capabilities"
    run_command "qmicli -d $device --dms-get-software-version" "Get software version information"
    run_command "qmicli -d $device --dms-get-factory-sku" "Get factory SKU"
    
    # Optional commands that may fail
    echo -e "\n\033[1;33mTesting optional commands (may fail):\033[0m"
    run_command "qmicli -d $device --dms-list-stored-images" "List stored firmware images"
    run_command "qmicli -d $device --dms-swi-get-usb-composition" "Get USB composition (Sierra Wireless specific)"
    run_command "qmicli -d $device --dms-get-prl-version" "Get PRL (Preferred Roaming List) version"
    run_command "qmicli -d $device --dms-get-activation-state" "Get service activation state"
    run_command "qmicli -d $device --dms-get-user-lock-state" "Get user lock state"
    run_command "qmicli -d $device --dms-get-boot-image-download-mode" "Get boot image download mode"
    
    echo -e "\n\033[0;34m========================================\033[0m"
    echo -e "\033[0;34m DEVICE $device TESTING COMPLETE\033[0m"
    echo -e "\033[0;34m========================================\033[0m"
done

echo -e "\n\033[0;32mAll QMI command tests completed!\033[0m"
echo -e "\033[1;33mNote: Some commands may fail depending on device capabilities and support.\033[0m"

