#include "wireguard_c_bridge.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <net/if.h>
#include "thread_manager.h"

/* Include WireGuard library headers */
#include "../ur-wg-provider/include/ipc.h"
#include "../ur-wg-provider/include/config.h"
#include "../ur-wg-provider/include/containers.h"
#include "../ur-wg-provider/include/encoding.h"
#include "../ur-wg-provider/include/routing.h"
#include "../ur-wg-provider/include/routing_api.h"

#define MAX_LINE_LENGTH 4096
#define MAX_CMD_LENGTH 1024

struct wireguard_bridge_ctx {
    wireguard_bridge_state_t state;
    wireguard_bridge_config_t config;
    wireguard_bridge_stats_t stats;
    wireguard_bridge_stats_t cached_stats;
    struct wgdevice *device;
    pthread_mutex_t mutex;
    pthread_mutex_t stats_mutex;
    char last_error[256];
    time_t connect_time;
    char routes[256][64];
    int route_count;

    /* Stats monitoring using thread_manager */
    thread_manager_t thread_manager;
    unsigned int stats_thread_id;
    volatile bool stats_running;
    wireguard_stats_callback_t stats_callback;
    void* stats_user_data;
    unsigned int stats_interval_ms;

    /* Bandwidth calculation */
    uint64_t last_bytes_sent;
    uint64_t last_bytes_received;
    time_t last_stats_update;
};

static int g_initialized = 0;
static pthread_mutex_t g_init_mutex = PTHREAD_MUTEX_INITIALIZER;

int wireguard_bridge_init_static(void) {
    pthread_mutex_lock(&g_init_mutex);
    if (g_initialized) {
        pthread_mutex_unlock(&g_init_mutex);
        return 0;
    }
    g_initialized = 1;
    pthread_mutex_unlock(&g_init_mutex);
    return 0;
}

void wireguard_bridge_uninit_static(void) {
    pthread_mutex_lock(&g_init_mutex);
    g_initialized = 0;
    pthread_mutex_unlock(&g_init_mutex);
}

wireguard_bridge_ctx_t* wireguard_bridge_create_context(void) {
    wireguard_bridge_ctx_t* ctx = (wireguard_bridge_ctx_t*)calloc(1, sizeof(wireguard_bridge_ctx_t));
    if (!ctx) {
        return NULL;
    }

    ctx->state = WG_STATE_INITIAL;
    ctx->device = NULL;
    pthread_mutex_init(&ctx->mutex, NULL);
    pthread_mutex_init(&ctx->stats_mutex, NULL);
    memset(&ctx->stats, 0, sizeof(wireguard_bridge_stats_t));
    memset(&ctx->cached_stats, 0, sizeof(wireguard_bridge_stats_t));
    memset(&ctx->config, 0, sizeof(wireguard_bridge_config_t));
    ctx->last_error[0] = '\0';
    ctx->connect_time = 0;
    ctx->route_count = 0;

    /* Initialize thread manager with capacity of 2 threads */
    if (thread_manager_init(&ctx->thread_manager, 2) < 0) {
        pthread_mutex_destroy(&ctx->mutex);
        pthread_mutex_destroy(&ctx->stats_mutex);
        free(ctx);
        return NULL;
    }

    ctx->stats_thread_id = 0;
    ctx->stats_running = false;
    ctx->stats_callback = NULL;
    ctx->stats_user_data = NULL;
    ctx->stats_interval_ms = 1000;
    ctx->last_bytes_sent = 0;
    ctx->last_bytes_received = 0;
    ctx->last_stats_update = 0;

    return ctx;
}

static void free_wg_device(struct wgdevice *device) {
    if (!device) return;

    struct wgpeer *peer = device->first_peer;
    while (peer) {
        struct wgpeer *next_peer = peer->next_peer;

        struct wgallowedip *allowedip = peer->first_allowedip;
        while (allowedip) {
            struct wgallowedip *next_allowedip = allowedip->next_allowedip;
            free(allowedip);
            allowedip = next_allowedip;
        }

        free(peer);
        peer = next_peer;
    }

    free(device);
}

void wireguard_bridge_destroy_context(wireguard_bridge_ctx_t* ctx) {
    if (!ctx) return;

    /* Stop stats monitoring first */
    wireguard_bridge_stop_stats_monitor(ctx);

    pthread_mutex_lock(&ctx->mutex);
    if (ctx->device) {
        free_wg_device(ctx->device);
        ctx->device = NULL;
    }
    pthread_mutex_unlock(&ctx->mutex);

    /* Destroy thread manager */
    thread_manager_destroy(&ctx->thread_manager);

    pthread_mutex_destroy(&ctx->mutex);
    pthread_mutex_destroy(&ctx->stats_mutex);
    free(ctx);
}

static void parse_additional_config_data(const char* config_file, wireguard_bridge_config_t* config) {
    FILE* f = fopen(config_file, "r");
    if (!f) return;

    char line[MAX_LINE_LENGTH];
    int in_interface_section = 0;

    while (fgets(line, sizeof(line), f)) {
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n')
            line[len - 1] = '\0';

        char* trimmed = line;
        while (*trimmed == ' ' || *trimmed == '\t') trimmed++;

        if (strncasecmp(trimmed, "[Interface]", 11) == 0) {
            in_interface_section = 1;
            continue;
        } else if (trimmed[0] == '[') {
            in_interface_section = 0;
            continue;
        }

        if (!in_interface_section) continue;

        /* Parse Address */
        if (strncasecmp(trimmed, "Address", 7) == 0) {
            char* value = strchr(trimmed, '=');
            if (value && config->address_count < 16) {
                value++;
                while (*value == ' ' || *value == '\t') value++;

                char* addr_copy = strdup(value);
                char* addr = strtok(addr_copy, ",");
                while (addr && config->address_count < 16) {
                    while (*addr == ' ' || *addr == '\t') addr++;
                    char* end = addr + strlen(addr) - 1;
                    while (end > addr && (*end == ' ' || *end == '\t')) *end-- = '\0';

                    strncpy(config->addresses[config->address_count], addr, 63);
                    config->addresses[config->address_count][63] = '\0';
                    config->address_count++;

                    addr = strtok(NULL, ",");
                }
                free(addr_copy);
            }
        }
        /* Parse DNS */
        else if (strncasecmp(trimmed, "DNS", 3) == 0) {
            char* value = strchr(trimmed, '=');
            if (value && config->dns_count < 8) {
                value++;
                while (*value == ' ' || *value == '\t') value++;

                char* dns_copy = strdup(value);
                char* dns = strtok(dns_copy, ",");
                while (dns && config->dns_count < 8) {
                    while (*dns == ' ' || *dns == '\t') dns++;
                    char* end = dns + strlen(dns) - 1;
                    while (end > dns && (*end == ' ' || *end == '\t')) *end-- = '\0';

                    strncpy(config->dns_servers[config->dns_count], dns, 63);
                    config->dns_servers[config->dns_count][63] = '\0';
                    config->dns_count++;

                    dns = strtok(NULL, ",");
                }
                free(dns_copy);
            }
        }
        /* Parse MTU */
        else if (strncasecmp(trimmed, "MTU", 3) == 0) {
            char* value = strchr(trimmed, '=');
            if (value) {
                value++;
                while (*value == ' ' || *value == '\t') value++;
                config->mtu = atoi(value);
            }
        }
    }

    fclose(f);
}

static void extract_routes_from_device(struct wgdevice* device, wireguard_bridge_ctx_t* ctx) {
    struct wgpeer *peer;
    struct wgallowedip* allowedip;

    for_each_wgpeer(device, peer) {
        for_each_wgallowedip(peer, allowedip) {
            if (ctx->route_count >= 256) break;

            char ip[INET6_ADDRSTRLEN];
            if (allowedip->family == AF_INET) {
                inet_ntop(AF_INET, &allowedip->ip4, ip, sizeof(ip));
            } else {
                inet_ntop(AF_INET6, &allowedip->ip6, ip, sizeof(ip));
            }

            snprintf(ctx->routes[ctx->route_count], 63, "%s/%u", ip, allowedip->cidr);
            ctx->route_count++;
        }
    }
}

int wireguard_bridge_parse_config(wireguard_bridge_ctx_t* ctx, const char* config_file) {
    if (!ctx || !config_file) return -1;

    pthread_mutex_lock(&ctx->mutex);
    ctx->state = WG_STATE_CONFIGURING;

    /* Parse WireGuard configuration */
    struct config_ctx config_ctx;
    if (!config_read_init(&config_ctx, false)) {
        snprintf(ctx->last_error, sizeof(ctx->last_error), "Failed to initialize config parser");
        ctx->state = WG_STATE_ERROR;
        pthread_mutex_unlock(&ctx->mutex);
        return -1;
    }

    FILE* f = fopen(config_file, "r");
    if (!f) {
        snprintf(ctx->last_error, sizeof(ctx->last_error), "Failed to open config file: %s", config_file);
        ctx->state = WG_STATE_ERROR;
        pthread_mutex_unlock(&ctx->mutex);
        return -1;
    }

    char line[MAX_LINE_LENGTH];
    while (fgets(line, sizeof(line), f)) {
        if (!config_read_line(&config_ctx, line)) {
            snprintf(ctx->last_error, sizeof(ctx->last_error), "Failed to parse config line");
            fclose(f);
            ctx->state = WG_STATE_ERROR;
            pthread_mutex_unlock(&ctx->mutex);
            return -1;
        }
    }
    fclose(f);

    ctx->device = config_read_finish(&config_ctx);
    if (!ctx->device) {
        snprintf(ctx->last_error, sizeof(ctx->last_error), "Failed to finalize config");
        ctx->state = WG_STATE_ERROR;
        pthread_mutex_unlock(&ctx->mutex);
        return -1;
    }

    /* Set default interface name if not already set */
    if (!ctx->config.interface_name[0]) {
        strncpy(ctx->config.interface_name, "wg0", sizeof(ctx->config.interface_name) - 1);
    }

    /* Set interface name in device structure */
    strncpy(ctx->device->name, ctx->config.interface_name, IFNAMSIZ - 1);
    ctx->device->name[IFNAMSIZ - 1] = '\0';

    /* Parse additional configuration (Address, DNS, MTU) */
    parse_additional_config_data(config_file, &ctx->config);

    /* Extract routes from AllowedIPs */
    extract_routes_from_device(ctx->device, ctx);

    pthread_mutex_unlock(&ctx->mutex);
    return 0;
}

int wireguard_bridge_set_config(wireguard_bridge_ctx_t* ctx, const wireguard_bridge_config_t* config) {
    if (!ctx || !config) return -1;

    pthread_mutex_lock(&ctx->mutex);
    memcpy(&ctx->config, config, sizeof(wireguard_bridge_config_t));
    ctx->state = WG_STATE_CONFIGURING;
    pthread_mutex_unlock(&ctx->mutex);

    return 0;
}

int wireguard_bridge_set_interface(wireguard_bridge_ctx_t* ctx, const char* interface_name) {
    if (!ctx || !interface_name) return -1;

    pthread_mutex_lock(&ctx->mutex);
    strncpy(ctx->config.interface_name, interface_name, sizeof(ctx->config.interface_name) - 1);
    pthread_mutex_unlock(&ctx->mutex);

    return 0;
}

int wireguard_bridge_add_peer(wireguard_bridge_ctx_t* ctx, const char* public_key, 
                               const char* endpoint, const char* allowed_ips) {
    if (!ctx || !public_key) return -1;

    pthread_mutex_lock(&ctx->mutex);
    strncpy(ctx->config.peer_public_key, public_key, sizeof(ctx->config.peer_public_key) - 1);
    if (endpoint) {
        strncpy(ctx->config.peer_endpoint, endpoint, sizeof(ctx->config.peer_endpoint) - 1);
    }
    if (allowed_ips) {
        strncpy(ctx->config.allowed_ips, allowed_ips, sizeof(ctx->config.allowed_ips) - 1);
    }
    pthread_mutex_unlock(&ctx->mutex);

    return 0;
}

int wireguard_bridge_connect(wireguard_bridge_ctx_t* ctx) {
    if (!ctx) return -1;

    pthread_mutex_lock(&ctx->mutex);

    if (!ctx->device) {
        snprintf(ctx->last_error, sizeof(ctx->last_error), "No device configuration loaded");
        ctx->state = WG_STATE_ERROR;
        pthread_mutex_unlock(&ctx->mutex);
        return -1;
    }

    ctx->state = WG_STATE_HANDSHAKING;

    if (ipc_set_device(ctx->device) < 0) {
        snprintf(ctx->last_error, sizeof(ctx->last_error), "Failed to set device configuration");
        ctx->state = WG_STATE_ERROR;
        pthread_mutex_unlock(&ctx->mutex);
        return -1;
    }

    ctx->state = WG_STATE_CONNECTED;
    ctx->connect_time = time(NULL);

    pthread_mutex_unlock(&ctx->mutex);
    return 0;
}

int wireguard_bridge_disconnect(wireguard_bridge_ctx_t* ctx) {
    if (!ctx) return -1;

    pthread_mutex_lock(&ctx->mutex);
    ctx->state = WG_STATE_DISCONNECTED;
    pthread_mutex_unlock(&ctx->mutex);

    return 0;
}

int wireguard_bridge_reconnect(wireguard_bridge_ctx_t* ctx) {
    if (!ctx) return -1;

    wireguard_bridge_disconnect(ctx);
    sleep(1);
    return wireguard_bridge_connect(ctx);
}

wireguard_bridge_state_t wireguard_bridge_get_state(wireguard_bridge_ctx_t* ctx) {
    if (!ctx) return WG_STATE_ERROR;

    pthread_mutex_lock(&ctx->mutex);
    wireguard_bridge_state_t state = ctx->state;
    pthread_mutex_unlock(&ctx->mutex);

    return state;
}

int wireguard_bridge_get_stats(wireguard_bridge_ctx_t* ctx, wireguard_bridge_stats_t* stats) {
    if (!ctx || !stats) return -1;

    pthread_mutex_lock(&ctx->mutex);

    memset(stats, 0, sizeof(wireguard_bridge_stats_t));

    if (!ctx->device || !ctx->device->name[0]) {
        pthread_mutex_unlock(&ctx->mutex);
        return -1;
    }

    // Query current device state from kernel
    char interface_name[IFNAMSIZ];
    strncpy(interface_name, ctx->device->name, IFNAMSIZ - 1);
    interface_name[IFNAMSIZ - 1] = '\0';

    pthread_mutex_unlock(&ctx->mutex);

    // Get device statistics (ipc_get_device allocates memory)
    struct wgdevice *current_device = NULL;
    if (ipc_get_device(&current_device, interface_name) != 0) {
        return -1;
    }

    if (!current_device) {
        return -1;
    }

    pthread_mutex_lock(&ctx->mutex);

    // Extract statistics from current device state
    if (current_device->first_peer) {
        struct wgpeer *peer = current_device->first_peer;

        // Transfer statistics
        stats->bytes_sent = peer->tx_bytes;
        stats->bytes_received = peer->rx_bytes;
        stats->tx_packets = 0; // Not available via standard WireGuard IPC
        stats->rx_packets = 0; // Not available via standard WireGuard IPC

        // Last handshake time
        stats->last_handshake = peer->last_handshake_time.tv_sec;

        // Calculate connected duration
        if (ctx->connect_time > 0) {
            stats->connected_duration = time(NULL) - ctx->connect_time;
        }

        // Endpoint
        if (peer->endpoint.addr.sa_family == AF_INET) {
            struct sockaddr_in *addr = (struct sockaddr_in *)&peer->endpoint.addr;
            char ip_str[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &addr->sin_addr, ip_str, sizeof(ip_str));
            snprintf(stats->endpoint, sizeof(stats->endpoint), "%s:%d", 
                     ip_str, ntohs(addr->sin_port));
        } else if (peer->endpoint.addr.sa_family == AF_INET6) {
            struct sockaddr_in6 *addr = (struct sockaddr_in6 *)&peer->endpoint.addr;
            char ip_str[INET6_ADDRSTRLEN];
            inet_ntop(AF_INET6, &addr->sin6_addr, ip_str, sizeof(ip_str));
            snprintf(stats->endpoint, sizeof(stats->endpoint), "[%s]:%d", 
                     ip_str, ntohs(addr->sin6_port));
        }

        // Public key (base64 encoded)
        char key_b64[WG_KEY_LEN_BASE64];
        key_to_base64(key_b64, peer->public_key);
        strncpy(stats->public_key, key_b64, sizeof(stats->public_key) - 1);
        stats->public_key[sizeof(stats->public_key) - 1] = '\0';

        // Allowed IPs
        char allowed_ips_str[512] = {0};
        struct wgallowedip *allowedip = peer->first_allowedip;
        while (allowedip) {
            char ip_str[INET6_ADDRSTRLEN];
            if (allowedip->family == AF_INET) {
                inet_ntop(AF_INET, &allowedip->ip4, ip_str, sizeof(ip_str));
            } else {
                inet_ntop(AF_INET6, &allowedip->ip6, ip_str, sizeof(ip_str));
            }

            char cidr[256];
            snprintf(cidr, sizeof(cidr), "%s%s/%d", 
                     strlen(allowed_ips_str) > 0 ? ", " : "",
                     ip_str, allowedip->cidr);
            strncat(allowed_ips_str, cidr, sizeof(allowed_ips_str) - strlen(allowed_ips_str) - 1);

            allowedip = allowedip->next_allowedip;
        }
        strncpy(stats->allowed_ips, allowed_ips_str, sizeof(stats->allowed_ips) - 1);
        stats->allowed_ips[sizeof(stats->allowed_ips) - 1] = '\0';
    }

    // Get local IP from configured addresses
    if (ctx->config.address_count > 0) {
        strncpy(stats->local_ip, ctx->config.addresses[0], sizeof(stats->local_ip) - 1);
        stats->local_ip[sizeof(stats->local_ip) - 1] = '\0';
    }

    // Estimate latency based on handshake recency (simplified heuristic)
    if (stats->last_handshake > 0) {
        time_t now = time(NULL);
        time_t handshake_age = now - stats->last_handshake;
        if (handshake_age < 300) { // Less than 5 minutes
            stats->latency_ms = 10 + (handshake_age / 10); // Rough estimate
        } else {
            stats->latency_ms = 999; // Stale connection
        }
    }

    // Get interface name
    if (current_device && current_device->name[0]) {
        strncpy(stats->interface_name, current_device->name, sizeof(stats->interface_name) - 1);
        stats->interface_name[sizeof(stats->interface_name) - 1] = '\0';
    }

    // Collect routes from AllowedIPs (convert to JSON format)
    if (current_device->first_peer && current_device->first_peer->first_allowedip) {
        char *route_ptr = stats->routes;
        int remaining = sizeof(stats->routes) - 1;
        int written = snprintf(route_ptr, remaining, "[");
        route_ptr += written;
        remaining -= written;
        
        struct wgallowedip *allowedip = current_device->first_peer->first_allowedip;
        int route_idx = 0;
        while (allowedip && remaining > 0) {
            char ip_str[INET6_ADDRSTRLEN];
            if (allowedip->family == AF_INET) {
                inet_ntop(AF_INET, &allowedip->ip4, ip_str, sizeof(ip_str));
            } else {
                inet_ntop(AF_INET6, &allowedip->ip6, ip_str, sizeof(ip_str));
            }
            
            written = snprintf(route_ptr, remaining,
                              "%s{\"destination\":\"%s/%d\",\"via\":\"interface\"}",
                              route_idx > 0 ? "," : "", ip_str, allowedip->cidr);
            if (written > 0 && written < remaining) {
                route_ptr += written;
                remaining -= written;
                route_idx++;
            } else {
                break;
            }
            
            allowedip = allowedip->next_allowedip;
        }
        
        if (remaining > 1) {
            snprintf(route_ptr, remaining, "]");
        }
    } else {
        strcpy(stats->routes, "[]");
    }

    pthread_mutex_unlock(&ctx->mutex);

    // Free the device structure
    free_wg_device(current_device);

    return 0;
}

bool wireguard_bridge_is_connected(wireguard_bridge_ctx_t* ctx) {
    if (!ctx) return false;

    pthread_mutex_lock(&ctx->mutex);
    bool connected = (ctx->state == WG_STATE_CONNECTED);
    pthread_mutex_unlock(&ctx->mutex);

    return connected;
}

const char* wireguard_bridge_get_last_error(wireguard_bridge_ctx_t* ctx) {
    if (!ctx) return "Invalid context";

    pthread_mutex_lock(&ctx->mutex);
    const char* error = ctx->last_error;
    pthread_mutex_unlock(&ctx->mutex);

    return error;
}

void wireguard_bridge_set_state(wireguard_bridge_ctx_t* ctx, wireguard_bridge_state_t state) {
    if (!ctx) return;

    pthread_mutex_lock(&ctx->mutex);
    ctx->state = state;
    pthread_mutex_unlock(&ctx->mutex);
}

/* Additional setup functions matching main.c functionality */

int wireguard_bridge_create_interface(wireguard_bridge_ctx_t* ctx, const char* interface_name) {
    if (!ctx || !interface_name) return -1;

    char cmd[MAX_CMD_LENGTH];
    snprintf(cmd, sizeof(cmd), "ip link add dev %s type wireguard 2>/dev/null", interface_name);

    if (system(cmd) != 0) {
        pthread_mutex_lock(&ctx->mutex);
        snprintf(ctx->last_error, sizeof(ctx->last_error), "Failed to create interface %s", interface_name);
        pthread_mutex_unlock(&ctx->mutex);
        return -1;
    }

    pthread_mutex_lock(&ctx->mutex);
    strncpy(ctx->config.interface_name, interface_name, sizeof(ctx->config.interface_name) - 1);
    if (ctx->device) {
        strncpy(ctx->device->name, interface_name, IFNAMSIZ - 1);
        ctx->device->name[IFNAMSIZ - 1] = '\0';
    }
    pthread_mutex_unlock(&ctx->mutex);

    return 0;
}

int wireguard_bridge_assign_addresses(wireguard_bridge_ctx_t* ctx) {
    if (!ctx) return -1;

    pthread_mutex_lock(&ctx->mutex);
    const char* interface_name = ctx->config.interface_name;
    int address_count = ctx->config.address_count;
    pthread_mutex_unlock(&ctx->mutex);

    if (!interface_name[0]) {
        return -1;
    }

    char cmd[MAX_CMD_LENGTH];
    for (int i = 0; i < address_count; i++) {
        snprintf(cmd, sizeof(cmd), "ip address add %s dev %s", 
                 ctx->config.addresses[i], interface_name);
        if (system(cmd) != 0) {
            pthread_mutex_lock(&ctx->mutex);
            snprintf(ctx->last_error, sizeof(ctx->last_error), 
                    "Failed to add address %s", ctx->config.addresses[i]);
            pthread_mutex_unlock(&ctx->mutex);
        }
    }

    return 0;
}

int wireguard_bridge_set_mtu(wireguard_bridge_ctx_t* ctx, int mtu) {
    if (!ctx || mtu <= 0) return -1;

    pthread_mutex_lock(&ctx->mutex);
    const char* interface_name = ctx->config.interface_name;
    ctx->config.mtu = mtu;
    pthread_mutex_unlock(&ctx->mutex);

    if (!interface_name[0]) {
        return -1;
    }

    char cmd[MAX_CMD_LENGTH];
    snprintf(cmd, sizeof(cmd), "ip link set mtu %d dev %s", mtu, interface_name);

    if (system(cmd) != 0) {
        pthread_mutex_lock(&ctx->mutex);
        snprintf(ctx->last_error, sizeof(ctx->last_error), "Failed to set MTU");
        pthread_mutex_unlock(&ctx->mutex);
        return -1;
    }

    return 0;
}

int wireguard_bridge_bring_up_interface(wireguard_bridge_ctx_t* ctx) {
    if (!ctx) return -1;

    pthread_mutex_lock(&ctx->mutex);
    const char* interface_name = ctx->config.interface_name;
    pthread_mutex_unlock(&ctx->mutex);

    if (!interface_name[0]) {
        return -1;
    }

    char cmd[MAX_CMD_LENGTH];
    snprintf(cmd, sizeof(cmd), "ip link set up dev %s", interface_name);

    if (system(cmd) != 0) {
        pthread_mutex_lock(&ctx->mutex);
        snprintf(ctx->last_error, sizeof(ctx->last_error), "Failed to bring up interface");
        pthread_mutex_unlock(&ctx->mutex);
        return -1;
    }

    return 0;
}

int wireguard_bridge_setup_routes(wireguard_bridge_ctx_t* ctx) {
    if (!ctx) return -1;

    pthread_mutex_lock(&ctx->mutex);
    const char* interface_name = ctx->config.interface_name;
    int route_count = ctx->route_count;
    pthread_mutex_unlock(&ctx->mutex);

    if (!interface_name[0]) {
        return -1;
    }

    char cmd[MAX_CMD_LENGTH];
    for (int i = 0; i < route_count; i++) {
        snprintf(cmd, sizeof(cmd), "ip route add %s dev %s 2>/dev/null || true", 
                 ctx->routes[i], interface_name);
        system(cmd);
    }

    return 0;
}

int wireguard_bridge_setup_dns(wireguard_bridge_ctx_t* ctx) {
    if (!ctx) return -1;

    pthread_mutex_lock(&ctx->mutex);
    const char* interface_name = ctx->config.interface_name;
    int dns_count = ctx->config.dns_count;

    if (!interface_name[0] || dns_count == 0) {
        pthread_mutex_unlock(&ctx->mutex);
        return 0;
    }

    char resolvconf_cmd[256];
    snprintf(resolvconf_cmd, sizeof(resolvconf_cmd), 
             "resolvconf -a %s -m 0 -x 2>/dev/null", interface_name);

    FILE* resolvconf = popen(resolvconf_cmd, "w");
    if (resolvconf) {
        for (int i = 0; i < dns_count; i++) {
            fprintf(resolvconf, "nameserver %s\n", ctx->config.dns_servers[i]);
        }
        pclose(resolvconf);
    }

    pthread_mutex_unlock(&ctx->mutex);
    return 0;
}

int wireguard_bridge_cleanup_interface(wireguard_bridge_ctx_t* ctx) {
    if (!ctx) return -1;

    pthread_mutex_lock(&ctx->mutex);
    const char* interface_name = ctx->config.interface_name;
    pthread_mutex_unlock(&ctx->mutex);

    if (!interface_name[0]) {
        return 0;
    }

    char cmd[MAX_CMD_LENGTH];
    int result = 0;

    /* Remove DNS configuration */
    snprintf(cmd, sizeof(cmd), "resolvconf -d %s 2>/dev/null || true", interface_name);
    system(cmd);

    /* Remove routes */
    snprintf(cmd, sizeof(cmd), "ip route flush dev %s 2>/dev/null || true", interface_name);
    system(cmd);

    /* Bring down interface */
    snprintf(cmd, sizeof(cmd), "ip link set dev %s down 2>/dev/null", interface_name);
    result = system(cmd);
    if (result != 0) {
        pthread_mutex_lock(&ctx->mutex);
        snprintf(ctx->last_error, sizeof(ctx->last_error), 
                 "Failed to bring down interface %s", interface_name);
        pthread_mutex_unlock(&ctx->mutex);
    }

    /* Delete interface */
    snprintf(cmd, sizeof(cmd), "ip link del dev %s 2>/dev/null", interface_name);
    result = system(cmd);
    if (result != 0) {
        pthread_mutex_lock(&ctx->mutex);
        snprintf(ctx->last_error, sizeof(ctx->last_error), 
                 "Failed to delete interface %s", interface_name);
        pthread_mutex_unlock(&ctx->mutex);
        return -1;
    }

    return 0;
}

/* Stats monitoring thread function */
static void* stats_monitor_thread(void* arg) {
    wireguard_bridge_ctx_t* ctx = (wireguard_bridge_ctx_t*)arg;

    uint64_t last_sent = 0;
    uint64_t last_received = 0;
    struct timespec last_time;
    clock_gettime(CLOCK_MONOTONIC, &last_time);

    while (ctx->stats_running) {
        struct timespec current_time;
        clock_gettime(CLOCK_MONOTONIC, &current_time);

        /* Calculate time elapsed in milliseconds */
        long elapsed_ms = (current_time.tv_sec - last_time.tv_sec) * 1000 +
                         (current_time.tv_nsec - last_time.tv_nsec) / 1000000;

        if (elapsed_ms >= ctx->stats_interval_ms) {
            wireguard_bridge_stats_t current_stats;
            memset(&current_stats, 0, sizeof(current_stats));

            /* Get fresh stats from WireGuard kernel interface */
            pthread_mutex_lock(&ctx->mutex);
            if (ctx->device && ctx->device->name[0]) {
                struct wgdevice *fresh_device = NULL;
                if (ipc_get_device(&fresh_device, ctx->device->name) == 0 && fresh_device) {
                    struct wgpeer *peer;
                    for_each_wgpeer(fresh_device, peer) {
                        /* Collect byte transfer statistics */
                        current_stats.bytes_sent = peer->tx_bytes;
                        current_stats.bytes_received = peer->rx_bytes;
                        current_stats.last_handshake = peer->last_handshake_time.tv_sec;

                        /* Extract endpoint address (peer server) */
                        if (peer->endpoint.addr.sa_family == AF_INET) {
                            struct sockaddr_in *addr = (struct sockaddr_in *)&peer->endpoint.addr4;
                            char ip_str[INET_ADDRSTRLEN];
                            inet_ntop(AF_INET, &addr->sin_addr, ip_str, sizeof(ip_str));
                            snprintf(current_stats.endpoint, sizeof(current_stats.endpoint), 
                                     "%s:%d", ip_str, ntohs(addr->sin_port));
                        } else if (peer->endpoint.addr.sa_family == AF_INET6) {
                            struct sockaddr_in6 *addr = (struct sockaddr_in6 *)&peer->endpoint.addr6;
                            char ip_str[INET6_ADDRSTRLEN];
                            inet_ntop(AF_INET6, &addr->sin6_addr, ip_str, sizeof(ip_str));
                            snprintf(current_stats.endpoint, sizeof(current_stats.endpoint), 
                                     "[%s]:%d", ip_str, ntohs(addr->sin6_port));
                        }

                        /* Encode peer public key to base64 */
                        key_to_base64(current_stats.public_key, peer->public_key);

                        /* Extract allowed IPs */
                        struct wgallowedip *allowedip;
                        char *ptr = current_stats.allowed_ips;
                        size_t remaining = sizeof(current_stats.allowed_ips);
                        for_each_wgallowedip(peer, allowedip) {
                            char cidr_buf[128];
                            if (allowedip->family == AF_INET) {
                                char ip_str[INET_ADDRSTRLEN];
                                inet_ntop(AF_INET, &allowedip->ip4, ip_str, sizeof(ip_str));
                                snprintf(cidr_buf, sizeof(cidr_buf), "%s/%d", ip_str, allowedip->cidr);
                            } else if (allowedip->family == AF_INET6) {
                                char ip_str[INET6_ADDRSTRLEN];
                                inet_ntop(AF_INET6, &allowedip->ip6, ip_str, sizeof(ip_str));
                                snprintf(cidr_buf, sizeof(cidr_buf), "%s/%d", ip_str, allowedip->cidr);
                            }
                            
                            int written = snprintf(ptr, remaining, "%s%s", 
                                                  (ptr == current_stats.allowed_ips) ? "" : ", ", 
                                                  cidr_buf);
                            if (written > 0 && (size_t)written < remaining) {
                                ptr += written;
                                remaining -= written;
                            }
                        }

                        /* Calculate latency estimate from handshake freshness */
                        time_t now = time(NULL);
                        if (peer->last_handshake_time.tv_sec > 0) {
                            time_t handshake_age = now - peer->last_handshake_time.tv_sec;
                            /* If recent handshake, estimate low latency */
                            if (handshake_age < 30) {
                                current_stats.latency_ms = 10 + (handshake_age * 2);
                            } else {
                                current_stats.latency_ms = 100; /* Older handshake */
                            }
                        }

                        break; /* Only process first peer */
                    }
                    free_wgdevice(fresh_device);
                }
            }

            /* Calculate connection duration */
            if (ctx->connect_time > 0) {
                current_stats.connected_duration = (int)(time(NULL) - ctx->connect_time);
            }
            pthread_mutex_unlock(&ctx->mutex);

            /* Calculate bandwidth rates (bytes per second) */
            if (last_sent > 0 && last_received > 0 && elapsed_ms > 0) {
                uint64_t sent_diff = current_stats.bytes_sent - last_sent;
                uint64_t recv_diff = current_stats.bytes_received - last_received;

                current_stats.upload_rate_bps = (sent_diff * 1000) / elapsed_ms;
                current_stats.download_rate_bps = (recv_diff * 1000) / elapsed_ms;
            }

            /* Update cached stats */
            pthread_mutex_lock(&ctx->stats_mutex);
            memcpy(&ctx->cached_stats, &current_stats, sizeof(wireguard_bridge_stats_t));
            pthread_mutex_unlock(&ctx->stats_mutex);

            /* Invoke callback if set */
            if (ctx->stats_callback) {
                ctx->stats_callback(&current_stats, ctx->stats_user_data);
            }

            /* Update tracking variables */
            last_sent = current_stats.bytes_sent;
            last_received = current_stats.bytes_received;
            last_time = current_time;
        }

        /* Sleep for a short interval to avoid busy-waiting */
        struct timespec sleep_time;
        sleep_time.tv_sec = 0;
        sleep_time.tv_nsec = 50000000; /* 50ms */
        nanosleep(&sleep_time, NULL);
    }

    return NULL;
}

int wireguard_bridge_start_stats_monitor(wireguard_bridge_ctx_t* ctx, 
                                          wireguard_stats_callback_t callback,
                                          void* user_data,
                                          unsigned int interval_ms) {
    if (!ctx) return -1;

    /* Stop any existing monitor */
    wireguard_bridge_stop_stats_monitor(ctx);

    pthread_mutex_lock(&ctx->mutex);
    ctx->stats_callback = callback;
    ctx->stats_user_data = user_data;
    ctx->stats_interval_ms = interval_ms > 0 ? interval_ms : 1000;
    ctx->stats_running = true;
    pthread_mutex_unlock(&ctx->mutex);

    /* Start monitoring thread using thread_manager */
    int result = thread_create(&ctx->thread_manager, stats_monitor_thread, ctx, &ctx->stats_thread_id);
    if (result < 0) {
        ctx->stats_running = false;
        return -1;
    }

    return 0;
}

void wireguard_bridge_stop_stats_monitor(wireguard_bridge_ctx_t* ctx) {
    if (!ctx) return;

    pthread_mutex_lock(&ctx->mutex);
    if (!ctx->stats_running) {
        pthread_mutex_unlock(&ctx->mutex);
        return;
    }

    ctx->stats_running = false;
    unsigned int thread_id = ctx->stats_thread_id;
    pthread_mutex_unlock(&ctx->mutex);

    /* Wait for thread to finish using thread_manager */
    if (thread_id > 0) {
        thread_stop(&ctx->thread_manager, thread_id);
        thread_join(&ctx->thread_manager, thread_id, NULL);
    }
}

int wireguard_bridge_get_cached_stats(wireguard_bridge_ctx_t* ctx, wireguard_bridge_stats_t* stats) {
    if (!ctx || !stats) return -1;

    pthread_mutex_lock(&ctx->stats_mutex);
    memcpy(stats, &ctx->cached_stats, sizeof(wireguard_bridge_stats_t));
    pthread_mutex_unlock(&ctx->stats_mutex);

    return 0;
}

int wireguard_bridge_connect_full(wireguard_bridge_ctx_t* ctx, bool setup_routing, bool setup_dns) {
    if (!ctx) return -1;

    pthread_mutex_lock(&ctx->mutex);
    if (!ctx->device) {
        snprintf(ctx->last_error, sizeof(ctx->last_error), "No device configuration loaded");
        ctx->state = WG_STATE_ERROR;
        pthread_mutex_unlock(&ctx->mutex);
        return -1;
    }

    const char* interface_name = ctx->config.interface_name[0] ? 
                                 ctx->config.interface_name : "wg0";
    pthread_mutex_unlock(&ctx->mutex);

    /* Step 1: Create the WireGuard interface */
    if (wireguard_bridge_create_interface(ctx, interface_name) < 0) {
        return -1;
    }

    /* Step 2: Configure WireGuard device */
    pthread_mutex_lock(&ctx->mutex);
    ctx->state = WG_STATE_HANDSHAKING;

    if (ipc_set_device(ctx->device) < 0) {
        snprintf(ctx->last_error, sizeof(ctx->last_error), "Failed to configure WireGuard");
        ctx->state = WG_STATE_ERROR;
        pthread_mutex_unlock(&ctx->mutex);
        wireguard_bridge_cleanup_interface(ctx);
        return -1;
    }
    pthread_mutex_unlock(&ctx->mutex);

    /* Step 3: Assign IP addresses */
    wireguard_bridge_assign_addresses(ctx);

    /* Step 4: Set MTU if specified */
    pthread_mutex_lock(&ctx->mutex);
    int mtu = ctx->config.mtu;
    pthread_mutex_unlock(&ctx->mutex);

    if (mtu > 0) {
        wireguard_bridge_set_mtu(ctx, mtu);
    }

    /* Step 5: Bring interface up */
    if (wireguard_bridge_bring_up_interface(ctx) < 0) {
        wireguard_bridge_cleanup_interface(ctx);
        return -1;
    }

    /* Step 6: Setup routes if requested */
    if (setup_routing) {
        wireguard_bridge_setup_routes(ctx);
    }

    /* Step 7: Setup DNS if requested */
    if (setup_dns) {
        wireguard_bridge_setup_dns(ctx);
    }

    pthread_mutex_lock(&ctx->mutex);
    ctx->state = WG_STATE_CONNECTED;
    ctx->connect_time = time(NULL);
    pthread_mutex_unlock(&ctx->mutex);

    return 0;
}

/* Routing API Implementation */

typedef struct {
    wg_routing_ctx_t *wg_routing;
    wireguard_bridge_route_callback_t callback;
    void *user_data;
} routing_bridge_ctx_t;

static void routing_event_wrapper(wg_route_event_type_t event_type,
                                  const wg_route_rule_t *rule,
                                  const char *error_message,
                                  void *user_data) {
    routing_bridge_ctx_t *bridge_ctx = (routing_bridge_ctx_t*)user_data;
    
    if (!bridge_ctx || !bridge_ctx->callback) {
        return;
    }
    
    const char *event_str = "unknown";
    switch (event_type) {
        case WG_ROUTE_EVENT_ADDED: event_str = "added"; break;
        case WG_ROUTE_EVENT_REMOVED: event_str = "removed"; break;
        case WG_ROUTE_EVENT_MODIFIED: event_str = "modified"; break;
        case WG_ROUTE_EVENT_DETECTED: event_str = "detected"; break;
        case WG_ROUTE_EVENT_FAILED: event_str = "failed"; break;
        case WG_ROUTE_EVENT_STATS_UPDATE: event_str = "stats_update"; break;
    }
    
    char rule_json[512];
    if (rule) {
        snprintf(rule_json, sizeof(rule_json),
                "{\"id\":\"%s\",\"name\":\"%s\",\"type\":%d,\"metric\":%u}",
                rule->id, rule->name, rule->type, rule->metric);
    } else {
        strcpy(rule_json, "{}");
    }
    
    bridge_ctx->callback(event_str, rule_json, error_message, bridge_ctx->user_data);
}

wireguard_routing_ctx_t wireguard_bridge_routing_init(wireguard_bridge_ctx_t *ctx) {
    if (!ctx) {
        return NULL;
    }
    
    pthread_mutex_lock(&ctx->mutex);
    const char *interface_name = ctx->config.interface_name[0] ? 
                                 ctx->config.interface_name : "wg0";
    pthread_mutex_unlock(&ctx->mutex);
    
    routing_bridge_ctx_t *routing_ctx = malloc(sizeof(routing_bridge_ctx_t));
    if (!routing_ctx) {
        return NULL;
    }
    
    routing_ctx->wg_routing = wg_routing_init(interface_name);
    if (!routing_ctx->wg_routing) {
        free(routing_ctx);
        return NULL;
    }
    
    routing_ctx->callback = NULL;
    routing_ctx->user_data = NULL;
    
    return (wireguard_routing_ctx_t)routing_ctx;
}

void wireguard_bridge_routing_cleanup(wireguard_routing_ctx_t routing_ctx) {
    if (!routing_ctx) {
        return;
    }
    
    routing_bridge_ctx_t *bridge_ctx = (routing_bridge_ctx_t*)routing_ctx;
    
    if (bridge_ctx->wg_routing) {
        wg_routing_cleanup(bridge_ctx->wg_routing);
    }
    
    free(bridge_ctx);
}

int wireguard_bridge_routing_add_rule_json(wireguard_routing_ctx_t routing_ctx,
                                           const char *rule_json) {
    if (!routing_ctx || !rule_json) {
        return -1;
    }
    
    routing_bridge_ctx_t *bridge_ctx = (routing_bridge_ctx_t*)routing_ctx;
    
    return -1;
}

int wireguard_bridge_routing_remove_rule(wireguard_routing_ctx_t routing_ctx,
                                         const char *rule_id) {
    if (!routing_ctx || !rule_id) {
        return -1;
    }
    
    routing_bridge_ctx_t *bridge_ctx = (routing_bridge_ctx_t*)routing_ctx;
    
    return wg_routing_remove_rule(bridge_ctx->wg_routing, rule_id);
}

char* wireguard_bridge_routing_get_all_json(wireguard_routing_ctx_t routing_ctx) {
    if (!routing_ctx) {
        return NULL;
    }
    
    routing_bridge_ctx_t *bridge_ctx = (routing_bridge_ctx_t*)routing_ctx;
    
    return wg_routing_export_json(bridge_ctx->wg_routing);
}

int wireguard_bridge_routing_apply_pre_connect(wireguard_routing_ctx_t routing_ctx) {
    if (!routing_ctx) {
        return -1;
    }
    
    routing_bridge_ctx_t *bridge_ctx = (routing_bridge_ctx_t*)routing_ctx;
    
    return wg_routing_apply_rules(bridge_ctx->wg_routing);
}

int wireguard_bridge_routing_detect_post_connect(wireguard_routing_ctx_t routing_ctx) {
    if (!routing_ctx) {
        return -1;
    }
    
    routing_bridge_ctx_t *bridge_ctx = (routing_bridge_ctx_t*)routing_ctx;
    
    return wg_routing_detect_routes(bridge_ctx->wg_routing);
}

void wireguard_bridge_routing_set_callback(wireguard_routing_ctx_t routing_ctx,
                                           wireguard_bridge_route_callback_t callback,
                                           void *user_data) {
    if (!routing_ctx) {
        return;
    }
    
    routing_bridge_ctx_t *bridge_ctx = (routing_bridge_ctx_t*)routing_ctx;
    
    bridge_ctx->callback = callback;
    bridge_ctx->user_data = user_data;
    
    wg_routing_set_callback(bridge_ctx->wg_routing, routing_event_wrapper, bridge_ctx);
}