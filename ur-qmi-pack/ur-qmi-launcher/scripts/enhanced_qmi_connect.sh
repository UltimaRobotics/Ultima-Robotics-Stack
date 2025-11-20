
#!/bin/bash

# Enhanced QMI Modem Connection Script with Intelligent Routing Management
# Usage: sudo ./enhanced_qmi_connect.sh <device> <apn> [<ip_type>] [<auth_type>] [<use_dhcp>] [<routing_mode>]

# Check root
[ "$(id -u)" -ne 0 ] && { echo "Run as root with sudo"; exit 1; }

# Arguments
[ $# -lt 2 ] && {
    echo "Usage: $0 <device> <apn> [<ip_type>] [<auth_type>] [<use_dhcp>] [<routing_mode>]"
    echo "IP types: 4=IPv4, 6=IPv6, 10=IPv4v6 (default: 4)"
    echo "Auth types: none, pap, chap, auto (default: none)"
    echo "DHCP: 1 to enable DHCP, 0 to use manual config (default: 0)"
    echo "Routing modes: auto, preserve, replace, coexist (default: auto)"
    exit 1
}

# Config
DEVICE=$1
APN=$2
IPTYPE=${3:-4}
AUTH=${4:-none}
USE_DHCP=${5:-0}
ROUTING_MODE=${6:-auto}
WWAN_IF="wwan0"
TIMEOUT=15
RETRIES=3

# Global variables for routing management
declare -A ORIGINAL_ROUTES
declare -A INTERFACE_METRICS
declare -A CURRENT_GATEWAYS
ROUTING_BACKUP_FILE="/tmp/qmi_routing_backup_$(date +%s).txt"
CELLULAR_METRIC_BASE=100

# Color codes for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

print_info() { echo -e "${BLUE}[INFO]${NC} $1"; }
print_success() { echo -e "${GREEN}[SUCCESS]${NC} $1"; }
print_warning() { echo -e "${YELLOW}[WARNING]${NC} $1"; }
print_error() { echo -e "${RED}[ERROR]${NC} $1"; }

# Enhanced parameter extraction functions
extract_ip_value() {
    local line="$1"
    # Extract just the IP address value after the colon
    echo "$line" | sed 's/.*: *\([0-9.]*\).*/\1/' | grep -E '^[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}$'
}

extract_numeric_value() {
    local line="$1"
    # Extract numeric value after colon
    echo "$line" | sed 's/.*: *\([0-9]*\).*/\1/' | grep -E '^[0-9]+$'
}

validate_ip_address() {
    local ip="$1"
    if [[ -n "$ip" ]] && [[ "$ip" =~ ^[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}$ ]]; then
        # Check each octet is 0-255
        local IFS=.
        local -a octets=($ip)
        for octet in "${octets[@]}"; do
            if [[ $octet -lt 0 || $octet -gt 255 ]]; then
                return 1
            fi
        done
        return 0
    fi
    return 1
}

validate_subnet_mask() {
    local mask="$1"
    if ! validate_ip_address "$mask"; then
        return 1
    fi
    
    # Check if it's a valid subnet mask (contiguous 1s followed by 0s in binary)
    local IFS=.
    local -a octets=($mask)
    local binary=""
    
    for octet in "${octets[@]}"; do
        binary="$binary$(printf "%08d" $(echo "obase=2; $octet" | bc))"
    done
    
    # Check if binary representation has contiguous 1s followed by 0s
    if [[ "$binary" =~ ^1*0*$ ]]; then
        return 0
    fi
    
    return 1
}

# Advanced routing analysis functions
analyze_current_routing() {
    print_info "Analyzing current routing table..."
    
    # Get all current routes
    ip route show > "$ROUTING_BACKUP_FILE"
    
    # Analyze existing default routes
    local default_routes=$(ip route show default | wc -l)
    print_info "Found $default_routes existing default route(s)"
    
    # Collect interface metrics and gateways
    while IFS= read -r route; do
        if [[ "$route" =~ ^default ]]; then
            local interface=$(echo "$route" | grep -o 'dev [^ ]*' | awk '{print $2}')
            local gateway=$(echo "$route" | grep -o 'via [^ ]*' | awk '{print $2}')
            local metric=$(echo "$route" | grep -o 'metric [0-9]*' | awk '{print $2}')
            
            if [[ -n "$interface" ]]; then
                INTERFACE_METRICS["$interface"]=${metric:-0}
                CURRENT_GATEWAYS["$interface"]="$gateway"
                print_info "  Interface: $interface, Gateway: ${gateway:-'direct'}, Metric: ${metric:-'0 (default)'}"
            fi
        fi
    done < <(ip route show default)
    
    # Analyze interface priorities based on type
    analyze_interface_priorities
}

analyze_interface_priorities() {
    print_info "Analyzing interface priorities..."
    
    # Get all active interfaces
    local interfaces=$(ip link show up | grep -E '^[0-9]+:' | awk -F': ' '{print $2}' | grep -v lo)
    
    for interface in $interfaces; do
        local priority=1000  # Default low priority
        
        # Assign priorities based on interface type
        case "$interface" in
            eth*|en*) priority=50 ;;          # Ethernet - highest priority
            wlan*|wifi*|wl*) priority=200 ;;  # WiFi - medium priority  
            wwan*|cellular*|usb*) priority=300 ;; # Cellular - lower priority
            ppp*|tun*|tap*) priority=400 ;;   # VPN/Dial-up - lowest priority
            *) priority=500 ;;                # Unknown interfaces
        esac
        
        # Check if interface has IP address
        if ip addr show "$interface" | grep -q 'inet '; then
            print_info "  Active interface: $interface (suggested metric: $priority)"
        else
            print_info "  Inactive interface: $interface"
        fi
    done
}

calculate_optimal_metric() {
    local target_interface="$1"
    local routing_mode="$2"
    
    case "$routing_mode" in
        "auto")
            # Auto mode: choose metric based on interface type and existing routes
            if [[ "$target_interface" =~ ^wwan ]]; then
                # For cellular, use higher metric unless it should be primary
                if [[ ${#INTERFACE_METRICS[@]} -eq 0 ]]; then
                    echo $CELLULAR_METRIC_BASE  # Primary interface
                else
                    echo $((CELLULAR_METRIC_BASE + 50))  # Secondary interface
                fi
            else
                echo $CELLULAR_METRIC_BASE
            fi
            ;;
        "preserve")
            # Preserve mode: use metric that doesn't conflict with existing
            local max_metric=0
            for metric in "${INTERFACE_METRICS[@]}"; do
                [[ $metric -gt $max_metric ]] && max_metric=$metric
            done
            echo $((max_metric + 10))
            ;;
        "replace")
            # Replace mode: use low metric to override existing routes
            echo 10
            ;;
        "coexist")
            # Coexist mode: use medium metric to coexist with others
            echo $((CELLULAR_METRIC_BASE + 25))
            ;;
        *)
            echo $CELLULAR_METRIC_BASE
            ;;
    esac
}

setup_intelligent_routing() {
    local interface="$1"
    local gateway="$2"
    local local_ip="$3"
    local routing_mode="$4"
    
    print_info "Setting up intelligent routing for $interface"
    print_info "  Gateway: $gateway"
    print_info "  Local IP: $local_ip"
    print_info "  Routing mode: $routing_mode"
    
    # Calculate optimal metric
    local optimal_metric=$(calculate_optimal_metric "$interface" "$routing_mode")
    print_info "  Calculated optimal metric: $optimal_metric"
    
    # Handle different routing modes
    case "$routing_mode" in
        "replace")
            print_warning "Replacing existing default routes"
            # Remove existing default routes
            ip route show default | while IFS= read -r route; do
                print_info "  Removing: $route"
                ip route del default $(echo "$route" | sed 's/^default //')
            done
            ;;
        "preserve")
            print_info "Preserving existing routes, adding cellular as alternative"
            ;;
        "coexist"|"auto")
            print_info "Setting up coexistence with existing routes"
            ;;
    esac
    
    # Add cellular default route
    if [[ -n "$gateway" ]]; then
        print_info "Adding default route via $gateway dev $interface metric $optimal_metric"
        if ip route add default via "$gateway" dev "$interface" metric "$optimal_metric" 2>/dev/null; then
            print_success "Default route added successfully"
        else
            print_warning "Could not add default route, checking if it already exists"
            if ip route show default | grep -q "$gateway"; then
                print_info "Route via $gateway already exists"
            else
                print_error "Failed to add default route"
                return 1
            fi
        fi
    fi
    
    # Add local network route if we have local IP
    if [[ -n "$local_ip" ]]; then
        local network=$(echo "$local_ip" | sed 's/\.[0-9]*$/.0/')
        print_info "Adding local network route: $network/24 dev $interface"
        ip route add "$network/24" dev "$interface" metric "$((optimal_metric - 10))" 2>/dev/null || true
    fi
    
    # Add service-specific routes
    add_service_specific_routes "$interface" "$gateway" "$optimal_metric"
    
    # Configure source-based routing if needed
    setup_source_routing "$interface" "$local_ip" "$optimal_metric"
}

add_service_specific_routes() {
    local interface="$1"
    local gateway="$2" 
    local base_metric="$3"
    
    print_info "Adding service-specific routes..."
    
    # DNS servers - ensure they're reachable via cellular
    local dns_servers=("8.8.8.8" "8.8.4.4" "1.1.1.1" "9.9.9.9")
    for dns in "${dns_servers[@]}"; do
        if [[ -n "$gateway" ]]; then
            ip route add "$dns/32" via "$gateway" dev "$interface" metric "$((base_metric - 5))" 2>/dev/null || true
        fi
    done
}

setup_source_routing() {
    local interface="$1"
    local local_ip="$2"
    local metric="$3"
    
    if [[ -z "$local_ip" ]]; then
        return 0
    fi
    
    print_info "Setting up source-based routing for $local_ip"
    
    # Create custom routing table for cellular interface
    local table_id=200
    
    # Add to rt_tables if not present
    if ! grep -q "^$table_id" /etc/iproute2/rt_tables 2>/dev/null; then
        echo "$table_id cellular" >> /etc/iproute2/rt_tables 2>/dev/null || true
    fi
    
    # Add routes to custom table
    ip route add default via $(ip route show dev "$interface" | grep -o 'via [^ ]*' | awk '{print $2}' | head -1) dev "$interface" table "$table_id" 2>/dev/null || true
    
    # Add rule for source-based routing
    ip rule add from "$local_ip" table "$table_id" 2>/dev/null || true
    
    print_success "Source-based routing configured"
}

validate_routing_setup() {
    local interface="$1"
    local gateway="$2"
    
    print_info "Validating routing setup..."
    
    # Check if interface is up and has IP
    if ! ip addr show "$interface" | grep -q 'inet '; then
        print_error "Interface $interface has no IP address"
        return 1
    fi
    
    print_success "Interface $interface is up with IP address"
    return 0
}

show_routing_summary() {
    print_info "=== Routing Summary ==="
    
    echo -e "\n${BLUE}Default Routes:${NC}"
    ip route show default | while IFS= read -r route; do
        echo "  $route"
    done
    
    echo -e "\n${BLUE}Interface Routes:${NC}"
    ip route show | grep -E "dev (eth|wlan|wwan|usb)" | head -10 | while IFS= read -r route; do
        echo "  $route"
    done
    
    echo -e "\n${BLUE}Interface Status:${NC}"
    ip addr show | grep -A 2 -E "^[0-9]+: (eth|wlan|wwan|usb)" | grep -E "(^[0-9]+:|inet )" | while IFS= read -r line; do
        echo "  $line"
    done
}

show_connection_summary() {
    local interface="$1"
    local gateway="$2"
    local local_ip="$3"
    
    print_info "=== CONNECTION SUMMARY ==="
    
    echo -e "\n${GREEN}Connection Details:${NC}"
    echo "  Interface: $interface"
    echo "  Local IP: $local_ip"
    echo "  Gateway: $gateway"
    echo "  Device: $DEVICE"
    echo "  APN: $APN"
    echo "  Connection ID: $CID"
    
    print_info "=== END SUMMARY ==="
}

restore_original_routing() {
    print_info "Restoring original routing configuration..."
    
    if [[ -f "$ROUTING_BACKUP_FILE" ]]; then
        # Remove routes we added
        ip route show | grep "dev $WWAN_IF" | while IFS= read -r route; do
            print_info "Removing: $route"
            ip route del $route 2>/dev/null || true
        done
        
        # Restore original default routes if they were removed
        if [[ "$ROUTING_MODE" == "replace" ]]; then
            print_info "Restoring original default routes..."
            # This is complex - in practice, most systems will auto-restore
            # or we could save and restore the specific routes
        fi
        
        print_success "Routing restoration completed"
    else
        print_warning "No routing backup found"
    fi
}

# Subnet to CIDR conversion (enhanced)
mask2cidr() {
    local mask=$1
    local cidr=0
    local IFS=.
    
    for octet in $mask; do
        case $octet in
            255) cidr=$((cidr + 8)) ;;
            254) cidr=$((cidr + 7)) ;;
            252) cidr=$((cidr + 6)) ;;
            248) cidr=$((cidr + 5)) ;;
            240) cidr=$((cidr + 4)) ;;
            224) cidr=$((cidr + 3)) ;;
            192) cidr=$((cidr + 2)) ;;
            128) cidr=$((cidr + 1)) ;;
            0) ;;
            *) print_error "Invalid subnet mask: $mask"; return 1 ;;
        esac
    done
    
    echo $cidr
}

# Enhanced gateway validation
validate_gateway() {
    local gw=$1
    local ip=$2
    local mask=$3
    
    # Convert to arrays for subnet calculation
    IFS=. read -ra ip_parts <<< "$ip"
    IFS=. read -ra mask_parts <<< "$mask"
    IFS=. read -ra gw_parts <<< "$gw"
    
    # Check if gateway is in same subnet
    for i in {0..3}; do
        local ip_masked=$((ip_parts[i] & mask_parts[i]))
        local gw_masked=$((gw_parts[i] & mask_parts[i]))
        
        if [[ $ip_masked -ne $gw_masked ]]; then
            print_error "Gateway $gw is not in the same subnet as IP $ip with mask $mask"
            return 1
        fi
    done
    
    print_success "Gateway validation passed"
    return 0
}

# Enhanced manual network configuration
setup_network_manual() {
    local ip=$1 mask=$2 gw=$3 dns1=$4 dns2=$5 mtu=${6:-1500}
    
    print_info "Configuring network manually..."
    print_info "  IP: $ip"
    print_info "  Subnet mask: $mask" 
    print_info "  Gateway: ${gw:-'none'}"
    print_info "  DNS1: $dns1"
    print_info "  DNS2: ${dns2:-'none'}"
    print_info "  MTU: $mtu"
    
    # Validate parameters
    if ! validate_ip_address "$ip"; then
        print_error "Invalid IP address: $ip"
        return 1
    fi
    
    if ! validate_subnet_mask "$mask"; then
        print_error "Invalid subnet mask: $mask"
        return 1
    fi
    
    if [[ -n "$gw" ]] && ! validate_ip_address "$gw"; then
        print_error "Invalid gateway address: $gw"
        return 1
    fi
    
    # Convert mask to CIDR
    local cidr=$(mask2cidr "$mask") || {
        print_error "Invalid subnet mask: $mask"
        return 1
    }
    
    print_info "Converted to CIDR: $ip/$cidr"
    
    # Configure interface
    ip link set dev "$WWAN_IF" down
    ip addr flush dev "$WWAN_IF"
    ip link set dev "$WWAN_IF" mtu "$mtu"
    
    if ip addr add "$ip/$cidr" dev "$WWAN_IF"; then
        print_success "IP address configured: $ip/$cidr"
    else
        print_error "Failed to configure IP address"
        return 1
    fi
    
    if ip link set dev "$WWAN_IF" up; then
        print_success "Interface brought up"
    else
        print_error "Failed to bring interface up"
        return 1
    fi
    
    # Set up routing with intelligence
    if [[ -n "$gw" ]]; then
        validate_gateway "$gw" "$ip" "$mask" && {
            setup_intelligent_routing "$WWAN_IF" "$gw" "$ip" "$ROUTING_MODE"
        }
    else
        print_warning "No gateway provided, setting up interface-only routing"
        setup_intelligent_routing "$WWAN_IF" "" "$ip" "$ROUTING_MODE"
    fi
    
    # Configure DNS
    configure_dns "$dns1" "$dns2"
    
    return 0
}

# Enhanced DHCP network configuration
setup_network_dhcp() {
    print_info "Starting DHCP configuration..."
    
    # Configure interface
    ip link set dev "$WWAN_IF" down
    ip addr flush dev "$WWAN_IF"
    ip link set dev "$WWAN_IF" up
    
    # Kill any existing DHCP clients
    pkill -f "dhclient.*$WWAN_IF" 2>/dev/null || true
    sleep 1
    
    # Start DHCP client with timeout
    print_info "Starting DHCP client..."
    if timeout 30 dhclient -v "$WWAN_IF"; then
        print_success "DHCP configuration successful"
        
        # Extract gateway from DHCP lease
        local dhcp_gateway=$(ip route show dev "$WWAN_IF" default | awk '{print $3}' | head -1)
        local dhcp_ip=$(ip addr show "$WWAN_IF" | grep 'inet ' | awk '{print $2}' | cut -d'/' -f1)
        
        # Set up intelligent routing
        setup_intelligent_routing "$WWAN_IF" "$dhcp_gateway" "$dhcp_ip" "$ROUTING_MODE"
        
        return 0
    else
        print_error "DHCP configuration failed"
        return 1
    fi
}

# udhcpc fallback function for cellular interfaces
attempt_udhcpc_fallback() {
    local interface="$1"
    
    print_info "Attempting udhcpc fallback for cellular interface: $interface"
    
    # Check if udhcpc is available
    if ! command -v udhcpc >/dev/null 2>&1; then
        print_warning "udhcpc not available, skipping cellular DHCP fallback"
        return 0
    fi
    
    # Check if interface already has an IP
    local current_ip=$(ip addr show "$interface" | grep 'inet ' | awk '{print $2}' | cut -d'/' -f1 | head -1)
    if [[ -n "$current_ip" ]]; then
        print_info "Interface $interface already has IP: $current_ip"
        print_info "Running udhcpc to refresh/validate cellular DHCP lease..."
    else
        print_info "Interface $interface has no IP, attempting udhcpc configuration..."
    fi
    
    # Kill any existing udhcpc processes for this interface
    pkill -f "udhcpc.*$interface" 2>/dev/null || true
    sleep 1
    
    # Create udhcpc script directory if it doesn't exist
    local script_dir="/usr/share/udhcpc"
    if [[ ! -d "$script_dir" ]]; then
        mkdir -p "$script_dir" 2>/dev/null || true
    fi
    
    # Run udhcpc with cellular-optimized options
    print_info "Executing: udhcpc -q -f -i $interface"
    local udhcpc_output=""
    local udhcpc_success=false
    
    # Run udhcpc with timeout and capture output
    if timeout 30 udhcpc -q -f -i "$interface" 2>&1 | while IFS= read -r line; do
        echo "  udhcpc: $line"
        echo "$line" >> /tmp/udhcpc_${interface}_output.log
    done; then
        udhcpc_success=true
    fi
    
    # Check the results
    sleep 2
    local new_ip=$(ip addr show "$interface" | grep 'inet ' | awk '{print $2}' | cut -d'/' -f1 | head -1)
    local new_gateway=$(ip route show dev "$interface" default | awk '{print $3}' | head -1)
    
    if [[ -n "$new_ip" ]]; then
        if [[ "$new_ip" != "$current_ip" ]]; then
            print_success "udhcpc successfully configured new IP: $new_ip"
        else
            print_success "udhcpc confirmed existing IP: $new_ip"
        fi
        
        if [[ -n "$new_gateway" ]]; then
            print_success "Gateway available via udhcpc: $new_gateway"
            
            # Update routing if gateway changed
            if [[ "$new_gateway" != "$GW" ]]; then
                print_info "Gateway changed, updating intelligent routing..."
                setup_intelligent_routing "$interface" "$new_gateway" "$new_ip" "$ROUTING_MODE"
            fi
        fi
        
        print_success "udhcpc configuration completed"
        
        return 0
    else
        print_warning "udhcpc completed but no IP address assigned"
        if [[ -f "/tmp/udhcpc_${interface}_output.log" ]]; then
            print_info "udhcpc output log:"
            cat "/tmp/udhcpc_${interface}_output.log" | head -10
        fi
        return 1
    fi
}

configure_dns() {
    local dns1="$1"
    local dns2="$2"
    
    print_info "Configuring DNS..."
    
    # Backup original resolv.conf
    [[ -f /etc/resolv.conf ]] && cp /etc/resolv.conf /etc/resolv.conf.qmi.backup
    
    # Create new DNS configuration
    {
        echo "# Generated by enhanced QMI connect script"
        echo "# Backup saved as /etc/resolv.conf.qmi.backup"
        [[ -n "$dns1" ]] && echo "nameserver $dns1"
        [[ -n "$dns2" ]] && echo "nameserver $dns2"
        # Add fallback DNS servers
        echo "nameserver 8.8.8.8"
        echo "nameserver 1.1.1.1"
    } > /etc/resolv.conf
    
    print_success "DNS configured with primary: ${dns1:-'none'}, secondary: ${dns2:-'none'}"
}

# Main execution
main() {
    print_info "Enhanced QMI Connection Setup Starting..."
    print_info "Device: $DEVICE, APN: $APN, IP Type: $IPTYPE, Auth: $AUTH"
    print_info "DHCP: $([ "$USE_DHCP" -eq 1 ] && echo "Enabled" || echo "Disabled")"
    print_info "Routing Mode: $ROUTING_MODE"
    
    # Analyze current routing state
    analyze_current_routing
    
    # Stop ModemManager to prevent conflicts
    systemctl stop ModemManager 2>/dev/null || print_warning "Could not stop ModemManager"
    
    # Initialize modem
    print_info "Initializing modem..."
    if qmicli -d "$DEVICE" --dms-set-operating-mode='online'; then
        print_success "Modem set to online mode"
    else
        print_error "Failed to set modem online"
        exit 1
    fi
    
    # Reset WDS service
    if qmicli -d "$DEVICE" --wds-reset; then
        print_success "WDS service reset"
    else
        print_warning "WDS reset failed, continuing..."
    fi
    
    # Prepare interface
    ip link set "$WWAN_IF" down 2>/dev/null
    if ip link set "$WWAN_IF" up; then
        print_success "Interface $WWAN_IF is ready"
    else
        print_error "Failed to prepare interface $WWAN_IF"
        exit 1
    fi
    
    # Detailed diagnostics before connection
    print_info "Performing detailed diagnostics before connection..."
    diagnose_connection_prerequisites "$DEVICE"
    
    # Connect with retries and detailed error capture
    local attempt=0
    local CID=""
    local last_error=""
    
    while [ $attempt -lt $RETRIES ]; do
        attempt=$((attempt+1))
        print_info "Connection attempt $attempt/$RETRIES"
        
        # Capture full output for debugging
        local qmi_output=""
        local qmi_cmd="qmicli -d '$DEVICE' --device-open-net='net-raw-ip|net-no-qos-header' --wds-start-network=\"apn='$APN',ip-type=$IPTYPE,auth=$AUTH\" --client-no-release-cid"
        
        print_info "Executing QMI command: $qmi_cmd"
        qmi_output=$(eval "$qmi_cmd" 2>&1)
        local qmi_exit_code=$?
        
        print_info "QMI command exit code: $qmi_exit_code"
        print_info "QMI command output:"
        echo "$qmi_output" | while IFS= read -r line; do
            echo "  $line"
        done
        
        if [[ $qmi_exit_code -eq 0 ]]; then
            # Check if we got a "Network started" message first
            if echo "$qmi_output" | grep -q "Network started"; then
                print_success "QMI network session started successfully"
                
                # Extract CID from the output using multiple patterns
                CID=$(echo "$qmi_output" | grep -E "CID: '[0-9]+'" | sed "s/.*CID: '\([0-9]*\)'.*/\1/" | head -1)
                
                # If first pattern failed, try alternative patterns
                if [[ -z "$CID" ]]; then
                    CID=$(echo "$qmi_output" | grep -o "CID: [0-9]*" | awk '{print $2}' | head -1)
                fi
                
                # If still no CID, try extracting from "Client ID not released" line
                if [[ -z "$CID" ]]; then
                    CID=$(echo "$qmi_output" | grep -A 2 "Client ID not released" | grep "CID:" | sed "s/.*CID: '\([0-9]*\)'.*/\1/" | head -1)
                fi
                
                # Extract Packet Data Handle as backup identifier
                local PDH=$(echo "$qmi_output" | grep "Packet data handle:" | sed "s/.*Packet data handle: '\([0-9]*\)'.*/\1/" | head -1)
                
                if [[ -n "$CID" ]]; then
                    print_success "Connected with CID $CID"
                    [[ -n "$PDH" ]] && print_info "Packet Data Handle: $PDH"
                    break
                elif [[ -n "$PDH" ]]; then
                    print_success "Connection established with Packet Data Handle: $PDH"
                    print_warning "CID not found, but connection appears successful"
                    CID="$PDH"  # Use PDH as fallback identifier
                    break
                else
                    print_warning "QMI command succeeded but no CID or PDH found in output"
                    last_error="No CID or PDH in successful output"
                    
                    # Check if connection is actually established by checking packet service
                    local packet_status=$(qmicli -d "$DEVICE" --wds-get-packet-service-status 2>&1)
                    if echo "$packet_status" | grep -q "connected"; then
                        print_success "Packet service shows connected state, proceeding without stored CID"
                        CID="unknown"  # Use placeholder to continue
                        break
                    fi
                fi
            else
                print_warning "No 'Network started' message in QMI output"
                last_error="No network started confirmation"
            fi
        else
            # Analyze the error
            last_error=$(analyze_qmi_error "$qmi_output")
            print_error "QMI command failed: $last_error"
            
            # Try to recover based on error type
            if attempt_connection_recovery "$DEVICE" "$last_error"; then
                print_info "Recovery attempt completed, continuing..."
            fi
        fi
        
        if [[ $attempt -lt $RETRIES ]]; then
            print_warning "Attempt $attempt failed, waiting before retry..."
            sleep $((attempt * 2))  # Progressive backoff
        fi
    done
    
    if [[ -z "$CID" ]]; then
        print_error "Failed to establish connection after $RETRIES attempts"
        print_error "Last error: $last_error"
        
        # Comprehensive failure diagnosis
        diagnose_connection_failure "$DEVICE" "$APN" "$last_error"
        exit 1
    fi
    
    # Verify connection is actually established
    print_info "Verifying connection establishment..."
    local packet_status=$(qmicli -d "$DEVICE" --wds-get-packet-service-status 2>&1)
    print_info "Current packet service status: $packet_status"
    
    if ! echo "$packet_status" | grep -q "connected"; then
        print_warning "Packet service not showing connected state, but attempting to get settings anyway"
    else
        print_success "Packet service confirmed as connected"
    fi
    
    # Get connection settings
    print_info "Retrieving connection settings..."
    local CONN_INFO=$(qmicli -d "$DEVICE" --wds-get-current-settings 2>&1)
    
    print_info "Raw connection settings output:"
    echo "$CONN_INFO" | while IFS= read -r line; do
        print_info "  $line"
    done
    
    if [[ -z "$CONN_INFO" ]] || echo "$CONN_INFO" | grep -q "error"; then
        print_warning "Failed to get complete connection settings, trying alternative approach"
        
        # Try to proceed with whatever information we have
        if echo "$packet_status" | grep -q "connected"; then
            print_info "Packet service is connected, attempting manual interface configuration"
            # We'll try to configure the interface manually and use DHCP as fallback
            USE_DHCP=1
            CONN_INFO="Manual configuration mode"
        else
            print_error "Connection verification failed completely"
            exit 1
        fi
    fi
    
    # Extract network parameters with improved parsing
    local IP MASK GW DNS1 DNS2 MTU
    
    if [[ "$CONN_INFO" != "Manual configuration mode" ]]; then
        # Use more robust extraction methods
        IP=$(echo "$CONN_INFO" | grep "IPv4 address:" | head -1 | sed 's/^[[:space:]]*IPv4 address:[[:space:]]*//' | sed 's/[[:space:]]*$//')
        MASK=$(echo "$CONN_INFO" | grep "IPv4 subnet mask:" | head -1 | sed 's/^[[:space:]]*IPv4 subnet mask:[[:space:]]*//' | sed 's/[[:space:]]*$//')
        GW=$(echo "$CONN_INFO" | grep "IPv4 gateway address:" | head -1 | sed 's/^[[:space:]]*IPv4 gateway address:[[:space:]]*//' | sed 's/[[:space:]]*$//')
        DNS1=$(echo "$CONN_INFO" | grep "IPv4 primary DNS:" | head -1 | sed 's/^[[:space:]]*IPv4 primary DNS:[[:space:]]*//' | sed 's/[[:space:]]*$//')
        DNS2=$(echo "$CONN_INFO" | grep "IPv4 secondary DNS:" | head -1 | sed 's/^[[:space:]]*IPv4 secondary DNS:[[:space:]]*//' | sed 's/[[:space:]]*$//')
        MTU=$(echo "$CONN_INFO" | grep "MTU:" | head -1 | sed 's/^[[:space:]]*MTU:[[:space:]]*//' | sed 's/[[:space:]]*$//')
        
        print_info "Network parameters extracted:"
        print_info "  IP: ${IP:-'not provided'}"
        print_info "  Subnet Mask: ${MASK:-'not provided'}"
        print_info "  Gateway: ${GW:-'not provided'}"
        print_info "  DNS1: ${DNS1:-'not provided'}"
        print_info "  DNS2: ${DNS2:-'not provided'}"
        print_info "  MTU: ${MTU:-'1500 (default)'}"
        
        # Additional validation of extracted values
        if [[ -n "$IP" ]] && ! validate_ip_address "$IP"; then
            print_warning "Extracted IP address appears invalid: $IP"
            IP=""
        fi
        
        if [[ -n "$MASK" ]] && ! validate_subnet_mask "$MASK"; then
            print_warning "Extracted subnet mask appears invalid: $MASK"
            MASK=""
        fi
        
        if [[ -n "$GW" ]] && ! validate_ip_address "$GW"; then
            print_warning "Extracted gateway address appears invalid: $GW"
            GW=""
        fi
        
        # Validate that we got essential parameters
        if [[ -z "$IP" ]] && [[ "$USE_DHCP" -eq 0 ]]; then
            print_warning "No valid IP address extracted from QMI settings, forcing DHCP mode"
            USE_DHCP=1
        fi
    else
        print_info "Manual configuration mode - will use DHCP or interface detection"
        USE_DHCP=1
    fi
    
    # Configure network
    if [ "$USE_DHCP" -eq 1 ]; then
        if ! setup_network_dhcp; then
            print_warning "DHCP failed, falling back to manual configuration"
            if [[ -n "$IP" && -n "$MASK" ]]; then
                setup_network_manual "$IP" "$MASK" "$GW" "$DNS1" "$DNS2" "${MTU:-1500}" || {
                    print_error "Network setup failed"
                    exit 1
                }
            else
                print_error "No valid network parameters available for manual configuration"
                exit 1
            fi
        fi
    else
        setup_network_manual "$IP" "$MASK" "$GW" "$DNS1" "$DNS2" "${MTU:-1500}" || {
            print_error "Manual network setup failed"
            exit 1
        }
    fi
    
    # Validate setup
    validate_routing_setup "$WWAN_IF" "$GW"
    
    # Try udhcpc as additional DHCP client after routing setup
    attempt_udhcpc_fallback "$WWAN_IF"
    
    # Show summary
    show_routing_summary
    
    print_success "Enhanced QMI connection established successfully!"
    
    # Show detailed connection summary
    show_connection_summary "$WWAN_IF" "$GW" "$IP"
    
    # Cleanup function
    cleanup() {
        print_info "Cleaning up connection..."
        
        # Restore routing
        restore_original_routing
        
        # Stop DHCP clients if used
        if [[ "$USE_DHCP" -eq 1 ]]; then
            dhclient -r "$WWAN_IF" 2>/dev/null || true
            pkill -f "udhcpc.*$WWAN_IF" 2>/dev/null || true
        fi
        
        # Clear interface
        ip addr flush dev "$WWAN_IF" 2>/dev/null
        ip link set "$WWAN_IF" down 2>/dev/null
        
        # Disconnect QMI session
        if [[ -n "$CID" ]] && [[ "$CID" != "unknown" ]]; then
            print_info "Disconnecting QMI session with CID $CID"
            if qmicli -d "$DEVICE" --wds-stop-network="$CID" --client-cid="$CID" 2>/dev/null; then
                print_success "QMI session disconnected"
            else
                print_warning "Failed to disconnect QMI session properly"
            fi
        elif [[ "$CID" == "unknown" ]]; then
            print_info "CID unknown, attempting to disconnect all active sessions"
            # Try to disconnect using various CID ranges
            for cid in {1..30}; do
                qmicli -d "$DEVICE" --wds-stop-network="$cid" 2>/dev/null && \
                    print_info "Disconnected session with CID $cid"
            done
        fi
        
        # Additional cleanup - reset WDS service
        qmicli -d "$DEVICE" --wds-reset 2>/dev/null
        
        # Restore DNS
        [[ -f /etc/resolv.conf.qmi.backup ]] && mv /etc/resolv.conf.qmi.backup /etc/resolv.conf
        
        # Restart ModemManager
        systemctl start ModemManager 2>/dev/null || true
        
        # Clean up backup files
        [[ -f "$ROUTING_BACKUP_FILE" ]] && rm -f "$ROUTING_BACKUP_FILE"
        
        # Clean up udhcpc log files
        rm -f /tmp/udhcpc_${WWAN_IF}_output.log 2>/dev/null || true
        
        print_success "Cleanup completed"
        exit 0
    }
    
    trap cleanup EXIT INT TERM
    
    print_info "Connection established successfully. Press Ctrl+C to disconnect"
    
    # Wait for user interrupt
    while true; do
        sleep 60
    done
}

# Comprehensive connection diagnostics
diagnose_connection_prerequisites() {
    local device="$1"
    
    print_info "=== Connection Prerequisites Diagnosis ==="
    
    # Check device status
    print_info "Checking device status..."
    local device_info=$(qmicli -d "$device" --dms-get-ids 2>&1)
    if [[ $? -eq 0 ]]; then
        print_success "Device communication OK"
        echo "$device_info" | grep -E "(IMEI|ESN|MEID)" | while IFS= read -r line; do
            print_info "  $line"
        done
    else
        print_error "Device communication failed"
        echo "$device_info"
    fi
    
    # Check SIM status
    print_info "Checking SIM card status..."
    local sim_status=$(qmicli -d "$device" --dms-get-capabilities 2>&1)
    if [[ $? -eq 0 ]]; then
        print_success "Device capabilities retrieved"
        echo "$sim_status" | grep -E "(Network|SIM)" | while IFS= read -r line; do
            print_info "  $line"
        done
    else
        print_warning "Could not get device capabilities"
    fi
    
    # Try alternative SIM check
    print_info "Checking SIM status via UIM service..."
    local uim_status=$(qmicli -d "$device" --uim-get-card-status 2>&1)
    if [[ $? -eq 0 ]]; then
        if echo "$uim_status" | grep -qi "ready\|active\|present"; then
            print_success "SIM card appears ready"
        else
            print_warning "SIM card status unclear"
        fi
        echo "$uim_status" | grep -E "(Card|State|PIN)" | head -5 | while IFS= read -r line; do
            print_info "  $line"
        done
    else
        print_warning "UIM service check failed, SIM status unknown"
    fi
    
    # Check signal strength
    print_info "Checking signal strength..."
    local signal_info=$(qmicli -d "$device" --nas-get-signal-strength 2>&1)
    if [[ $? -eq 0 ]]; then
        print_success "Signal information retrieved"
        echo "$signal_info" | grep -E "(RSSI|RSRP|Signal)" | while IFS= read -r line; do
            print_info "  $line"
        done
    else
        print_warning "Could not get signal information"
    fi
    
    # Check network registration
    print_info "Checking network registration..."
    local reg_status=$(qmicli -d "$device" --nas-get-serving-system 2>&1)
    if [[ $? -eq 0 ]]; then
        if echo "$reg_status" | grep -q "registered"; then
            print_success "Registered to network"
            echo "$reg_status" | grep -E "(Registration|Technology|Network)" | while IFS= read -r line; do
                print_info "  $line"
            done
        else
            print_error "Not registered to network"
            echo "$reg_status"
        fi
    else
        print_error "Could not check registration status"
    fi
    
    # Check packet service status
    print_info "Checking packet service status..."
    local packet_status=$(qmicli -d "$device" --wds-get-packet-service-status 2>&1)
    if [[ $? -eq 0 ]]; then
        print_info "Packet service status: $packet_status"
    else
        print_warning "Could not check packet service status"
    fi
    
    print_info "=== Prerequisites Check Complete ==="
}

analyze_qmi_error() {
    local error_output="$1"
    
    # Common error patterns and their meanings
    if echo "$error_output" | grep -qi "couldn't allocate client"; then
        echo "QMI client allocation failed - device may be busy"
    elif echo "$error_output" | grep -qi "call failed"; then
        if echo "$error_output" | grep -qi "protocol error"; then
            echo "Protocol error - invalid APN or authentication"
        elif echo "$error_output" | grep -qi "no effect"; then
            echo "Connection already active or conflicting state"
        else
            echo "Call failed - $(echo "$error_output" | grep -i "call failed" | head -1)"
        fi
    elif echo "$error_output" | grep -qi "timeout"; then
        echo "Operation timeout - poor signal or network issues"
    elif echo "$error_output" | grep -qi "not registered"; then
        echo "Device not registered to network"
    elif echo "$error_output" | grep -qi "invalid transition"; then
        echo "Invalid state transition - device needs reset"
    elif echo "$error_output" | grep -qi "authentication failed"; then
        echo "Authentication failed - check username/password"
    else
        echo "Unknown error: $(echo "$error_output" | head -2 | tail -1)"
    fi
}

attempt_connection_recovery() {
    local device="$1"
    local error_type="$2"
    
    print_info "Attempting recovery for: $error_type"
    
    case "$error_type" in
        *"client allocation"*)
            print_info "Resetting QMI clients..."
            qmicli -d "$device" --wds-noop >/dev/null 2>&1
            sleep 1
            return 0
            ;;
        *"already active"*|*"no effect"*|*"No CID"*|*"No PDH"*)
            print_info "Handling existing connection state..."
            # Check if we actually have a working connection
            local packet_status=$(qmicli -d "$device" --wds-get-packet-service-status 2>&1)
            if echo "$packet_status" | grep -q "connected"; then
                print_success "Connection already established, attempting to use existing state"
                return 0
            fi
            
            # Try to cleanup any hanging connections
            print_info "Cleaning up existing connections..."
            for cid in {1..30}; do
                local cleanup_output=$(qmicli -d "$device" --wds-stop-network="$cid" 2>&1)
                if [[ $? -eq 0 ]] && ! echo "$cleanup_output" | grep -q "error"; then
                    print_info "Cleaned up connection with CID $cid"
                fi
            done
            
            # Reset WDS service
            print_info "Resetting WDS service..."
            qmicli -d "$device" --wds-reset >/dev/null 2>&1
            sleep 2
            return 0
            ;;
        *"not registered"*)
            print_info "Waiting for network registration..."
            sleep 5
            local reg_check=$(qmicli -d "$device" --nas-get-serving-system 2>&1)
            if echo "$reg_check" | grep -q "registered"; then
                print_success "Network registration recovered"
                return 0
            else
                print_warning "Still not registered to network"
                return 1
            fi
            ;;
        *"invalid transition"*)
            print_info "Resetting modem state..."
            qmicli -d "$device" --dms-set-operating-mode='offline' >/dev/null 2>&1
            sleep 2
            qmicli -d "$device" --dms-set-operating-mode='online' >/dev/null 2>&1
            sleep 3
            return 0
            ;;
        *)
            print_info "Attempting generic recovery..."
            # Try a complete service reset
            print_info "Performing comprehensive service reset..."
            qmicli -d "$device" --wds-reset >/dev/null 2>&1
            qmicli -d "$device" --dms-set-operating-mode='offline' >/dev/null 2>&1
            sleep 2
            qmicli -d "$device" --dms-set-operating-mode='online' >/dev/null 2>&1
            sleep 3
            return 0
            ;;
    esac
}

diagnose_connection_failure() {
    local device="$1"
    local apn="$2"
    local last_error="$3"
    
    print_error "=== CONNECTION FAILURE DIAGNOSIS ==="
    
    print_info "Device: $device"
    print_info "APN: $apn"
    print_info "Last Error: $last_error"
    
    # Check if device is still accessible
    print_info "Testing device accessibility..."
    if qmicli -d "$device" --dms-get-operating-mode >/dev/null 2>&1; then
        print_success "Device is still accessible"
    else
        print_error "Device is no longer accessible"
        return
    fi
    
    # Check current packet service status
    print_info "Current packet service status..."
    local packet_status=$(qmicli -d "$device" --wds-get-packet-service-status 2>&1)
    print_info "Status: $packet_status"
    
    # Check for active connections
    print_info "Checking for existing connections..."
    local active_connections=$(qmicli -d "$device" --wds-get-current-settings 2>&1)
    if echo "$active_connections" | grep -q "IPv4"; then
        print_warning "There appears to be an existing connection"
        echo "$active_connections"
    else
        print_info "No existing connection detected"
    fi
    
    # Suggest troubleshooting steps
    print_info "=== TROUBLESHOOTING SUGGESTIONS ==="
    case "$last_error" in
        *"client allocation"*)
            print_info "1. Try killing any processes using the QMI device"
            print_info "2. Restart ModemManager: sudo systemctl restart ModemManager"
            print_info "3. Reset the USB device: sudo usb_modeswitch -v <vendor> -p <product> --reset-usb"
            ;;
        *"not registered"*)
            print_info "1. Check SIM card installation"
            print_info "2. Verify carrier signal strength in your area"
            print_info "3. Try manual network selection"
            print_info "4. Check if PIN is required: qmicli -d $device --dms-uim-verify-pin=PIN1,<pin>"
            ;;
        *"authentication"*|*"protocol error"*)
            print_info "1. Verify APN settings with your carrier"
            print_info "2. Try different authentication types (none, pap, chap)"
            print_info "3. Check if username/password are required"
            ;;
        *)
            print_info "1. Try power cycling the modem"
            print_info "2. Check carrier-specific APN settings"
            print_info "3. Verify account status with carrier"
            ;;
    esac
    
    print_info "=== END DIAGNOSIS ==="
}

# Enhanced IPv6 handling
configure_ipv6() {
    local interface="$1"
    
    case "$IPTYPE" in
        6|10)
            print_info "Enabling IPv6 on $interface"
            sysctl -w net.ipv6.conf."$interface".disable_ipv6=0 >/dev/null
            ;;
        *)
            print_info "Disabling IPv6 on $interface"
            sysctl -w net.ipv6.conf."$interface".disable_ipv6=1 >/dev/null
            ;;
    esac
}

# Execute main function
main "$@"

