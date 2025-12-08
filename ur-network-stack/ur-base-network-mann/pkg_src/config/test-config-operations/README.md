# OpenWrt Network CLI - Operation Configuration Files

This directory contains JSON operation configuration files for all supported operations of the `openwrt-network-cli` binary.

## **Complete List of Supported Operations**

### **📋 Basic Network Operations**
| File | Operation | Description |
|------|-----------|-------------|
| `01-network-connection-mode.json` | Set Connection Mode | Configure DHCP or static connection |
| `02-network-static-ip.json` | Static IP Configuration | Set static IP, subnet, and gateway |
| `03-network-dns-servers.json` | DNS Servers | Configure DNS server addresses |
| `04-network-mtu-size.json` | MTU Size | Set interface MTU (576-9000) |

### **🛣️ Static Routes Management**
| File | Operation | Description |
|------|-----------|-------------|
| `05-static-routes-add.json` | Add Static Routes | Add multiple static routes with metrics |
| `06-static-routes-remove.json` | Remove Static Routes | Remove specific static routes |

### **🏷️ VLAN Configuration**
| File | Operation | Description |
|------|-----------|-------------|
| `07-vlans-add.json` | Add VLANs | Create VLAN interfaces (802.1q/802.1ad) |
| `08-vlans-remove.json` | Remove VLANs | Delete VLAN configurations |

### **🔥 Firewall Rules Management**
| File | Operation | Description |
|------|-----------|-------------|
| `09-firewall-rules-add.json` | Add Firewall Rules | Create comprehensive firewall rules |
| `10-firewall-rules-remove.json` | Remove Firewall Rules | Delete specific firewall rules |

### **🌉 Network Bridge Configuration**
| File | Operation | Description |
|------|-----------|-------------|
| `11-bridges-add.json` | Add Bridges | Create network bridges with STP |
| `12-bridges-remove.json` | Remove Bridges | Delete bridge configurations |

### **🔄 NAT Rules Management**
| File | Operation | Description |
|------|-----------|-------------|
| `13-nat-rules-add.json` | Add NAT Rules | Port forwarding and masquerading |
| `14-nat-rules-remove.json` | Remove NAT Rules | Delete NAT configurations |

### **⚙️ System Operations**
| File | Operation | Description |
|------|-----------|-------------|
| `15-system-restart-interface.json` | Restart Interface | Restart network interface |
| `16-system-restore-defaults.json` | Restore Defaults | Factory reset network settings |

### **📊 Data Collection and Listing Operations**
| File | Operation | Description |
|------|-----------|-------------|
| `20-listing-network-status.json` | Network Status | Show comprehensive network status and interface info |
| `21-listing-static-routes.json` | List Static Routes | Display all configured static routes |
| `22-listing-vlans.json` | List VLANs | Display all VLAN configurations |
| `23-listing-firewall-rules.json` | List Firewall Rules | Display all firewall rules |
| `24-listing-bridges.json` | List Bridges | Display all network bridges |
| `25-listing-nat-rules.json` | List NAT Rules | Display all NAT rules |
| `26-listing-profiles.json` | List Profiles | Display all network profiles |
| `27-listing-monitoring.json` | Monitoring Control | Start and stop network monitoring |
| `28-listing-all-configurations.json` | List All | Display all configurations and status |

### **🎯 Combined & Specialized Operations**
| File | Operation | Description |
|------|-----------|-------------|
| `17-comprehensive-setup.json` | Complete Setup | Full network configuration |
| `18-security-hardening.json` | Security Hardening | Restrictive firewall setup |
| `19-guest-network-setup.json` | Guest Network | Isolated guest network setup |
| `29-add-and-list-comprehensive.json` | Add & List Demo | Add sample configs and list them |

## **🚀 Usage Examples**

### **Execute Individual Operations**
```bash
# Test mode (recommended for testing)
./openwrt-network-cli -p config/package-config.json -o config/test-config-operations/01-network-connection-mode.json -t

# Production mode
sudo ./openwrt-network-cli -p config/package-config.json -o config/test-config-operations/01-network-connection-mode.json
```

### **Execute Data Collection Operations**
```bash
# Show network status and interface information
./openwrt-network-cli -p config/package-config.json -o config/test-config-operations/20-listing-network-status.json -t

# List all static routes
./openwrt-network-cli -p config/package-config.json -o config/test-config-operations/21-listing-static-routes.json -t

# List all configurations
./openwrt-network-cli -p config/package-config.json -o config/test-config-operations/28-listing-all-configurations.json -t
```

### **Execute Complete Setup**
```bash
# Comprehensive network setup
./openwrt-network-cli -p config/package-config.json -o config/test-config-operations/17-comprehensive-setup.json -t

# Security hardening
./openwrt-network-cli -p config/package-config.json -o config/test-config-operations/18-security-hardening.json -t

# Guest network setup
./openwrt-network-cli -p config/package-config.json -o config/test-config-operations/19-guest-network-setup.json -t

# Add configurations and list them (demo)
./openwrt-network-cli -p config/package-config.json -o config/test-config-operations/29-add-and-list-comprehensive.json -t
```

## **📊 Operation Categories Summary**

### **Network Configuration (4 operations)**
- Connection mode (DHCP/Static)
- Static IP assignment
- DNS server configuration
- MTU size optimization

### **Routing (2 operations)**
- Add static routes with metrics
- Remove static routes

### **VLAN Management (2 operations)**
- Create VLAN interfaces
- Remove VLAN configurations

### **Firewall Management (2 operations)**
- Add comprehensive firewall rules
- Remove specific rules

### **Bridge Management (2 operations)**
- Create network bridges with STP
- Remove bridge configurations

### **NAT Management (2 operations)**
- Port forwarding and masquerading
- Remove NAT rules

### **System Operations (2 operations)**
- Interface restart
- Factory reset

### **Data Collection & Listing (9 operations)**
- Network status and interface information
- Static routes listing
- VLAN configurations listing
- Firewall rules listing
- Bridge configurations listing
- NAT rules listing
- Profile management listing
- Network monitoring control
- Comprehensive all-configurations listing

### **Specialized Setups (4 operations)**
- Complete network deployment
- Security hardening
- Guest network isolation
- Add and list demonstration

## **🔧 CLI Commands Correspondence**

Each JSON operation corresponds to CLI commands:

| JSON Operation | CLI Command Equivalent |
|----------------|------------------------|
| network.connection_mode | `set-mode <dhcp|static>` |
| network.static_ip | `set-static <ip> <mask> <gw>` |
| network.dns_servers | `set-dns <dns1> [dns2]` |
| network.mtu_size | `set-mtu <size>` |
| static_routes.add | `add-static-route <target> <gw> <iface> <metric>` |
| static_routes.remove | `remove-static-route <target>` |
| vlans.add | `add-vlan <name> <id> <base> <protocol>` |
| vlans.remove | `remove-vlan <name>` |
| firewall_rules.add | `add-firewall-rule <name> <src> <dest> <target> <proto>` |
| firewall_rules.remove | `remove-firewall-rule <name>` |
| bridges.add | `add-bridge <name> <stp> [interfaces...]` |
| bridges.remove | `remove-bridge <name>` |
| nat_rules.add | `add-nat-rule <name> <type> <src> <dest> <target>` |
| nat_rules.remove | `remove-nat-rule <name>` |
| system.restart_interface | `restart-interface` |
| system.restore_defaults | `restore-defaults` |
| listing.show_status | `status` |
| listing.show_interface_info | `interface` |
| listing.list_profiles | `profile-list` |
| listing.list_static_routes | `list-static-routes` |
| listing.list_vlans | `list-vlans` |
| listing.list_firewall_rules | `list-firewall-rules` |
| listing.list_bridges | `list-bridges` |
| listing.list_nat_rules | `list-nat-rules` |
| listing.start_monitoring | `monitor` |
| listing.stop_monitoring | `stop-monitor` |

## **📋 Total Operations: 29 Configuration Files**

**All 29 operations are fully supported and tested with the operation configuration system, including:**
- **16 configuration operations** (add/remove/modify network settings)
- **9 data collection operations** (list/view network information)
- **4 specialized combined operations** (complete setups and demonstrations)

## **🎯 JSON Structure for Listing Operations**

Listing operations use the `"listing"` section in JSON:

```json
{
  "description": "Data collection operations",
  "listing": {
    "show_status": true,
    "show_interface_info": true,
    "list_profiles": true,
    "list_static_routes": true,
    "list_vlans": true,
    "list_firewall_rules": true,
    "list_bridges": true,
    "list_nat_rules": true,
    "start_monitoring": true,
    "stop_monitoring": true
  }
}
```

Each listing operation can be combined with configuration operations in a single JSON file for comprehensive network management workflows.
