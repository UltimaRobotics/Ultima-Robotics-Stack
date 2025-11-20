
# Configuration Operations Documentation

## Overview

This document covers all configuration aspects of the ur-openvpn-library, including client configuration, server setup, and runtime operations.

## Client Configuration

### JSON Configuration Format

The client API accepts configuration in JSON format:

```json
{
  "profile_name": "MyVPN",
  "ovpn_config": "# OpenVPN configuration content here",
  "auth": {
    "username": "user@example.com",
    "password": "secure_password"
  },
  "certificates": {
    "cert_path": "/path/to/client.crt",
    "key_path": "/path/to/client.key",
    "ca_path": "/path/to/ca.crt"
  },
  "connection": {
    "auto_reconnect": true,
    "reconnect_interval": 30,
    "ping_interval": 10,
    "mtu_size": 1500
  },
  "proxy": {
    "host": "proxy.example.com",
    "port": 8080,
    "username": "proxy_user",
    "password": "proxy_pass"
  },
  "settings": {
    "enable_compression": true,
    "log_verbose": true,
    "stats_interval": 5
  }
}
```

### OpenVPN Configuration (.ovpn)

Example client configuration embedded in JSON:

```
client
proto udp
explicit-exit-notify
remote vpn.example.com 1194
dev tun
resolv-retry infinite
nobind
persist-key
persist-tun
remote-cert-tls server
cipher AES-256-GCM
auth SHA256
verb 3

# Inline certificates
<ca>
-----BEGIN CERTIFICATE-----
...
-----END CERTIFICATE-----
</ca>

<cert>
-----BEGIN CERTIFICATE-----
...
-----END CERTIFICATE-----
</cert>

<key>
-----BEGIN PRIVATE KEY-----
...
-----END PRIVATE KEY-----
</key>
```

### Configuration Parameters

#### Connection Settings

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `proto` | string | udp | Protocol: udp, tcp, udp6, tcp6 |
| `remote` | string | - | Server address and port |
| `dev` | string | tun | Device type: tun or tap |
| `nobind` | flag | - | Don't bind to local port |
| `persist-key` | flag | - | Keep keys across restart |
| `persist-tun` | flag | - | Keep TUN device across restart |

#### Security Settings

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `cipher` | string | AES-256-GCM | Encryption cipher |
| `auth` | string | SHA256 | Authentication algorithm |
| `tls-cipher` | string | - | TLS cipher suite |
| `tls-version-min` | string | 1.2 | Minimum TLS version |
| `remote-cert-tls` | string | server | Verify server certificate |

#### Network Settings

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `mtu-size` | int | 1500 | Maximum transmission unit |
| `fragment` | int | - | Fragment size (UDP only) |
| `mssfix` | int | - | MSS clamping value |
| `redirect-gateway` | flag | - | Route all traffic through VPN |

### API Configuration Functions

#### Parse JSON Configuration

```c
#include "openvpn_client_api.h"

const char *json_config = "{ ... }";
ovpn_client_config_t config;

int result = ovpn_client_parse_config_json(json_config, &config);
if (result == 0) {
    // Configuration parsed successfully
}
```

#### Create Client Session

```c
void my_event_callback(const ovpn_client_event_t *event, void *user_data) {
    printf("Event: %s\n", ovpn_client_event_type_to_string(event->type));
}

uint32_t session_id = ovpn_client_create_session(
    &config,
    my_event_callback,
    NULL  // user_data
);
```

#### Connect to VPN

```c
int result = ovpn_client_connect(session_id);
if (result == OVPN_ERROR_SUCCESS) {
    // Connection initiated
}
```

#### Update Configuration

```c
// Modify configuration
config.auto_reconnect = true;
config.reconnect_interval = 60;

// Apply changes
int result = ovpn_client_update_config(session_id, &config);
```

## Server Configuration

### JSON Configuration Format

```json
{
  "server_name": "OpenVPN Server",
  "listen_address": "0.0.0.0",
  "listen_port": 1194,
  "protocol": "udp",
  "device_type": "tun",
  "server_subnet": "10.8.0.0/24",
  
  "certificates": {
    "ca_cert_path": "/etc/openvpn/ca.crt",
    "server_cert_path": "/etc/openvpn/server.crt",
    "server_key_path": "/etc/openvpn/server.key",
    "dh_params_path": "/etc/openvpn/dh2048.pem"
  },
  
  "security": {
    "cipher": "AES-256-GCM",
    "auth_digest": "SHA256",
    "compression_enabled": false,
    "duplicate_cn_allowed": false
  },
  
  "client_config": {
    "max_clients": 100,
    "client_to_client": true,
    "push_routes": true,
    "dns_servers": ["8.8.8.8", "8.8.4.4"]
  },
  
  "management": {
    "address": "127.0.0.1",
    "port": 7505
  },
  
  "logging": {
    "log_file": "/var/log/openvpn.log",
    "verbosity": 3,
    "append": true
  }
}
```

### Server Configuration Functions

#### Initialize Server

```c
#include "openvpn_server_api.h"

ovpn_server_context_t *ctx = ovpn_server_init();
```

#### Load Configuration

```c
const char *json_config = "{ ... }";
int result = ovpn_server_load_config_json(ctx, json_config);
```

#### Start Server

```c
int result = ovpn_server_start(ctx);
if (result == 0) {
    // Server started successfully
}
```

#### Create Client Certificate

```c
uint32_t client_id = ovpn_server_create_client(
    ctx,
    "client1@example.com",  // common_name
    "client@example.com",   // email
    "VPN User"             // description
);
```

#### Generate Client Configuration

```c
ovpn_client_config_options_t options = {
    .include_ca_cert = true,
    .include_client_cert = true,
    .include_client_key = true,
    .use_inline_certs = true,
    .remote_host = "vpn.example.com",
    .remote_port = 1194,
    .protocol = "udp",
    .redirect_gateway = true
};

char *client_config = ovpn_server_generate_client_config(
    ctx,
    client_id,
    &options
);
```

## Configuration File Examples

### Example 1: Basic Client Configuration

**File**: `Ultima-Robotics-FehmiYousfi.ovpn`

```
client
proto udp
explicit-exit-notify
remote 164.132.53.65 1194
dev tun
resolv-retry infinite
nobind
persist-key
persist-tun
remote-cert-tls server
verify-x509-name server_TdRVRdZMQXooJ8YK name
auth SHA256
auth-nocache
cipher AES-128-GCM
tls-client
tls-version-min 1.2
```

### Example 2: Server Configuration with TLS-Auth

```
port 1194
proto udp
dev tun

ca /etc/openvpn/ca.crt
cert /etc/openvpn/server.crt
key /etc/openvpn/server.key
dh /etc/openvpn/dh2048.pem

server 10.8.0.0 255.255.255.0
ifconfig-pool-persist /var/log/openvpn/ipp.txt

push "redirect-gateway def1 bypass-dhcp"
push "dhcp-option DNS 8.8.8.8"
push "dhcp-option DNS 8.8.4.4"

keepalive 10 120
cipher AES-256-GCM
auth SHA256
user nobody
group nogroup
persist-key
persist-tun

status /var/log/openvpn/openvpn-status.log
log-append /var/log/openvpn/openvpn.log
verb 3
```

### Example 3: Client with Proxy

```
client
dev tun
proto tcp
remote vpn.example.com 443

http-proxy proxy.corporate.com 8080
http-proxy-retry

resolv-retry infinite
nobind
persist-key
persist-tun

cipher AES-256-GCM
auth SHA256

verb 3
```

## Runtime Configuration

### Session Management

#### Get Session State

```c
ovpn_client_state_t state = ovpn_client_get_state(session_id);
const char *state_str = ovpn_client_state_to_string(state);
printf("Connection state: %s\n", state_str);
```

#### Get Statistics

```c
ovpn_client_stats_t stats;
ovpn_client_get_stats(session_id, &stats);

printf("Bytes sent: %llu\n", stats.bytes_sent);
printf("Bytes received: %llu\n", stats.bytes_received);
printf("Connected since: %s", ctime(&stats.connected_since));
```

#### Get Quality Metrics

```c
ovpn_quality_metrics_t quality;
ovpn_client_get_quality(session_id, &quality);

printf("Ping: %u ms\n", quality.ping_ms);
printf("Packet loss: %u%%\n", quality.packet_loss_pct);
printf("Bandwidth: %u/%u kbps\n", 
    quality.bandwidth_up_kbps,
    quality.bandwidth_down_kbps);
```

### Connection Control

#### Disconnect

```c
int result = ovpn_client_disconnect(session_id);
```

#### Pause/Resume

```c
// Pause connection
ovpn_client_pause(session_id);

// Resume connection
ovpn_client_resume(session_id);
```

#### Auto-Reconnect

```c
// Enable auto-reconnect
ovpn_client_set_auto_reconnect(session_id, true);

// Disable auto-reconnect
ovpn_client_set_auto_reconnect(session_id, false);
```

### Event Handling

#### Event Loop

```c
while (running) {
    ovpn_client_event_t event;
    if (ovpn_client_get_next_event(session_id, &event)) {
        handle_event(&event);
        
        // Free event data if needed
        if (event.data) {
            free(event.data);
        }
        if (event.message) {
            free(event.message);
        }
    }
    usleep(100000);  // 100ms
}
```

#### Event Types

```c
void handle_event(const ovpn_client_event_t *event) {
    switch (event->type) {
        case CLIENT_EVENT_STATE_CHANGE:
            printf("State changed to: %s\n", 
                ovpn_client_state_to_string(event->state));
            break;
            
        case CLIENT_EVENT_STATS_UPDATE:
            update_ui_statistics(event->data);
            break;
            
        case CLIENT_EVENT_ERROR:
            log_error(event->message);
            break;
            
        case CLIENT_EVENT_RECONNECT:
            show_notification("Reconnecting to VPN...");
            break;
    }
}
```

## Advanced Configuration

### Custom Routes

```c
// Add custom route for client
ovpn_server_add_client_route(
    ctx,
    client_id,
    "192.168.100.0/24",  // network
    "10.8.0.1",         // gateway
    true                // push to client
);
```

### Static IP Assignment

```c
// Assign static IP to client
ovpn_server_set_client_static_ip(
    ctx,
    client_id,
    "10.8.0.100"
);
```

### Certificate Management

```c
// Generate new certificate
ovpn_server_generate_client_certificate(
    ctx,
    client_id,
    365  // validity days
);

// Renew certificate
ovpn_server_renew_client_certificate(
    ctx,
    client_id,
    730  // validity days (2 years)
);

// Revoke certificate
ovpn_server_revoke_client(
    ctx,
    client_id,
    "Certificate compromised"
);
```

### Configuration Validation

```c
// Validate server configuration
int valid = ovpn_server_validate_config(&config);
if (!valid) {
    fprintf(stderr, "Invalid configuration\n");
}
```

## Configuration Best Practices

1. **Security**
   - Use strong ciphers (AES-256-GCM)
   - Enable TLS 1.2 minimum
   - Use certificate-based auth
   - Enable tls-auth or tls-crypt

2. **Reliability**
   - Enable persist-key and persist-tun
   - Configure keepalive settings
   - Use auto-reconnect for clients

3. **Performance**
   - Set appropriate MTU
   - Use UDP for better performance
   - Enable compression if beneficial
   - Configure fragment/mssfix for problematic networks

4. **Logging**
   - Set appropriate verbosity (3-4 recommended)
   - Use log rotation
   - Monitor connection logs

5. **Client Management**
   - Use unique common names
   - Set certificate expiry appropriately
   - Maintain revocation list
   - Regular certificate rotation

## Troubleshooting Configuration Issues

### Connection Failures

```c
// Check connection state
ovpn_client_state_t state = ovpn_client_get_state(session_id);
if (state == CLIENT_STATE_ERROR) {
    // Check event queue for error details
    ovpn_client_event_t event;
    while (ovpn_client_get_next_event(session_id, &event)) {
        if (event.type == CLIENT_EVENT_ERROR) {
            fprintf(stderr, "Error: %s\n", event.message);
        }
    }
}
```

### Authentication Issues

```c
// Update credentials
ovpn_client_send_auth(
    session_id,
    "new_username",
    "new_password"
);
```

### Network Issues

```c
// Test latency
int latency = ovpn_client_test_latency(session_id);
if (latency < 0) {
    fprintf(stderr, "Latency test failed\n");
} else {
    printf("Latency: %d ms\n", latency);
}
```

## Memory Management

### Free Configuration

```c
// Free configuration structure
ovpn_client_free_config(&config);

// Free generated config string
ovpn_server_free_config_string(client_config);

// Free client list
ovpn_server_free_client_list(clients, count);
```

### Cleanup

```c
// Destroy session
ovpn_client_destroy_session(session_id);

// Cleanup API
ovpn_client_api_cleanup();

// Cleanup server
ovpn_server_cleanup(ctx);
```
