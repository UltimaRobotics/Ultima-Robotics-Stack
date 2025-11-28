#include "config.h"
#include "openvpn_c_bridge.h"
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdio.h>
#include <stdarg.h>

// Include logger for verbosity control
#include "../ur-rpc-template/deps/ur-logger-api/logger.h"

// Include OpenVPN lib-src headers (C only)
#include "source-port/lib-src/syshead.h"
#include "source-port/lib-src/openvpn.h"
#include "source-port/lib-src/init.h"
#include "source-port/lib-src/forward.h"
#include "source-port/lib-src/options.h"
#include "source-port/lib-src/sig.h"
#include "source-port/lib-src/ssl.h"
#include "source-port/lib-src/tunnel_entry.h"
#include "source-port/lib-src/socket.h"
#include "source-port/lib-src/misc.h"
#include "source-port/apis/openvpn_routing.h"
#include "source-port/apis/openvpn_routing_api.h"

// Helper function to measure ping/latency via simple socket timing
static int measure_connection_latency(const char *server_ip, int server_port) {
    if (!server_ip || server_port <= 0) {
        return 0;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);

    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        return 0;
    }

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        return 0;
    }

    // Set socket to non-blocking
    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);

    struct timeval start_time, end_time;
    gettimeofday(&start_time, NULL);

    // Try to connect (will return immediately with EINPROGRESS)
    connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr));

    // Wait for connection with timeout
    fd_set write_fds;
    FD_ZERO(&write_fds);
    FD_SET(sock, &write_fds);

    struct timeval timeout;
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;

    int result = select(sock + 1, NULL, &write_fds, NULL, &timeout);
    gettimeofday(&end_time, NULL);

    close(sock);

    if (result > 0) {
        // Calculate latency in milliseconds
        long latency_ms = (end_time.tv_sec - start_time.tv_sec) * 1000 +
                         (end_time.tv_usec - start_time.tv_usec) / 1000;
        return (int)latency_ms;
    }

    return 0;
}

// Opaque context implementation
struct openvpn_bridge_ctx {
    struct context c;
    struct env_set *es;
    bool initialized;
    bool init_complete;
};

int openvpn_bridge_init_static(void) {
    return init_static() ? 0 : -1;
}

void openvpn_bridge_uninit_static(void) {
    uninit_static();
}

openvpn_bridge_ctx_t *openvpn_bridge_create_context(void) {
    openvpn_bridge_ctx_t *ctx = (openvpn_bridge_ctx_t *)malloc(sizeof(openvpn_bridge_ctx_t));
    if (!ctx) {
        return NULL;
    }

    memset(ctx, 0, sizeof(openvpn_bridge_ctx_t));
    context_clear(&ctx->c);
    ctx->c.first_time = true;
    ctx->initialized = false;
    ctx->init_complete = false;
    ctx->es = NULL;

    return ctx;
}

int openvpn_bridge_parse_config(openvpn_bridge_ctx_t *ctx, const char *config_file) {
    if (!ctx || !config_file) {
        return -1;
    }

    // Allocate environment set
    ctx->es = env_set_create(NULL);
    if (!ctx->es) {
        return -1;
    }
    ctx->c.es = ctx->es;

    // Initialize options
    init_options(&ctx->c.options, true);

    // Parse config file
    const char *args[5];
    int argc = 3;
    
    args[0] = "openvpn";
    args[1] = "--config";
    args[2] = config_file;
    
    // Add --verb 0 if OpenVPN library logging is disabled
    bool logging_enabled = logger_is_source_enabled(LOG_SOURCE_OPENVPN_LIBRARY);
    
    if (!logging_enabled) {
        args[3] = "--verb";
        args[4] = "0";
        argc = 5;
    }
    
    parse_argv(&ctx->c.options, argc, (char **)args, M_USAGE, 
               OPT_P_DEFAULT, NULL, ctx->c.es);

    // Override verb level in options struct to take precedence over config file
    if (!logging_enabled) {
        ctx->c.options.verbosity = 0;
    }

    // Post-process options
    options_postprocess(&ctx->c.options, ctx->c.es);

    return 0;
}

int openvpn_bridge_context_init_1(openvpn_bridge_ctx_t *ctx) {
    if (!ctx) {
        return -1;
    }

    // Initialize the global signal handler first
    signal_reset(&siginfo_static, 0);
    
    // Assign signal handler to context
    ctx->c.sig = &siginfo_static;

    context_init_1(&ctx->c);
    ctx->initialized = true;

    return 0;
}

int openvpn_bridge_run_tunnel(openvpn_bridge_ctx_t *ctx) {
    if (!ctx || !ctx->initialized) {
        return -1;
    }

    // Run tunnel based on mode using exported entry points
    switch (ctx->c.options.mode) {
        case MODE_POINT_TO_POINT:
            openvpn_run_point_to_point(&ctx->c);
            break;

        case MODE_SERVER:
            openvpn_run_server(&ctx->c);
            break;

        default:
            return -1; // Invalid mode
    }

    ctx->init_complete = true;
    return 0;
}

openvpn_bridge_state_t openvpn_bridge_get_state(openvpn_bridge_ctx_t *ctx) {
    if (!ctx || !ctx->initialized) {
        return BRIDGE_STATE_INITIAL;
    }

    // Detect state from TLS multi-state if available
    if (ctx->c.c2.tls_multi) {
        if (ctx->c.c2.tls_multi->multi_state == CAS_CONNECT_DONE) {
            return BRIDGE_STATE_CONNECTED;
        } else if (ctx->c.c2.tls_multi->multi_state == CAS_FAILED) {
            return BRIDGE_STATE_ERROR;
        } else if (ctx->c.c2.tls_multi->multi_state >= CAS_WAITING_OPTIONS_IMPORT) {
            return BRIDGE_STATE_GET_CONFIG;
        } else if (ctx->c.c2.tls_multi->multi_state >= CAS_WAITING_AUTH) {
            return BRIDGE_STATE_AUTH;
        }
    }

    // Check if TUN device is configured
    if (ctx->c.c1.tuntap && ctx->c.c1.tuntap->did_ifconfig_setup) {
        return BRIDGE_STATE_ASSIGN_IP;
    }

    return BRIDGE_STATE_CONNECTING;
}

bool openvpn_bridge_is_connected(openvpn_bridge_ctx_t *ctx) {
    if (!ctx || !ctx->initialized) {
        return false;
    }

    if (ctx->c.c2.tls_multi) {
        return ctx->c.c2.tls_multi->multi_state == CAS_CONNECT_DONE;
    }

    return false;
}

int openvpn_bridge_get_stats(openvpn_bridge_ctx_t *ctx, openvpn_bridge_stats_t *stats) {
    if (!ctx || !stats) {
        return -1;
    }

    memset(stats, 0, sizeof(openvpn_bridge_stats_t));

    if (ctx->initialized) {
        // Collect byte transfer statistics from OpenVPN context
        stats->bytes_sent = ctx->c.c2.link_write_bytes;
        stats->bytes_received = ctx->c.c2.link_read_bytes;
        stats->tun_read_bytes = ctx->c.c2.tun_read_bytes;
        stats->tun_write_bytes = ctx->c.c2.tun_write_bytes;

        // Track connection start time - set once when first connected, never reset
        static time_t connection_start_time = 0;
        static bool was_connected_before = false;
        bool is_currently_connected = (ctx->c.c2.tls_multi && 
                                       ctx->c.c2.tls_multi->multi_state == CAS_CONNECT_DONE);
        
        if (is_currently_connected && !was_connected_before) {
            // First time connected - record the start time
            connection_start_time = time(NULL);
            was_connected_before = true;
        } else if (!is_currently_connected) {
            // Connection lost - reset for next connection
            was_connected_before = false;
            connection_start_time = 0;
        }
        
        stats->connected_since = connection_start_time;

        // Extract local and remote IP addresses if available
        if (ctx->c.c1.tuntap && ctx->c.c1.tuntap->did_ifconfig_setup) {
            // Get interface name
            if (ctx->c.c1.tuntap->actual_name) {
                snprintf(stats->interface_name, sizeof(stats->interface_name), "%s", 
                         ctx->c.c1.tuntap->actual_name);
            }
            
            // Get local IP from tun/tap configuration
            if (ctx->c.c1.tuntap->local) {
                snprintf(stats->local_ip, sizeof(stats->local_ip), "%s", 
                         print_in_addr_t(ctx->c.c1.tuntap->local, 0, &ctx->c.c2.gc));
            }
            
            // Get remote/gateway IP
            if (ctx->c.c1.tuntap->remote_netmask) {
                snprintf(stats->remote_ip, sizeof(stats->remote_ip), "%s",
                         print_in_addr_t(ctx->c.c1.tuntap->remote_netmask, 0, &ctx->c.c2.gc));
            }
        }

        // Get server IP and port from connection endpoint
        int server_port = 0;
        if (ctx->c.c2.link_sockets && ctx->c.c1.link_sockets_num > 0 && ctx->c.c2.link_sockets[0]) {
            struct link_socket *ls = ctx->c.c2.link_sockets[0];
            if (ls->info.lsa && ls->info.lsa->actual.dest.addr.sa.sa_family == AF_INET) {
                struct sockaddr_in *sa = (struct sockaddr_in *)&ls->info.lsa->actual.dest.addr.sa;
                inet_ntop(AF_INET, &sa->sin_addr, stats->server_ip, sizeof(stats->server_ip));
                server_port = ntohs(sa->sin_port);
            }
        }

        // Measure latency/ping to server if connected and we have server IP
        // Use single shared static cache for ping measurements
        static int cached_ping_ms = 0;
        static time_t last_ping_time = 0;
        
        if (stats->server_ip[0] != '\0' && server_port > 0 && 
            ctx->c.c2.tls_multi && ctx->c.c2.tls_multi->multi_state == CAS_CONNECT_DONE) {
            time_t current_time = time(NULL);
            
            // Measure latency every 5 seconds to avoid overhead
            if (current_time - last_ping_time >= 5) {
                int measured_ping = measure_connection_latency(stats->server_ip, server_port);
                if (measured_ping > 0) {
                    cached_ping_ms = measured_ping;
                }
                last_ping_time = current_time;
            }
            
            stats->ping_ms = cached_ping_ms;
        } else {
            stats->ping_ms = 0;
        }

        // Collect routes pushed by server (linked list traversal)
        if (ctx->c.c1.route_list && ctx->c.c1.route_list->routes) {
            char *route_ptr = stats->routes;
            int remaining = sizeof(stats->routes) - 1;
            int written = snprintf(route_ptr, remaining, "[");
            route_ptr += written;
            remaining -= written;
            
            int route_idx = 0;
            for (struct route_ipv4 *r = ctx->c.c1.route_list->routes; r && remaining > 0; r = r->next) {
                if (r->flags & RT_DEFINED) {
                    const char *network = print_in_addr_t(r->network, 0, &ctx->c.c2.gc);
                    const char *netmask = print_in_addr_t(r->netmask, 0, &ctx->c.c2.gc);
                    const char *gateway = print_in_addr_t(r->gateway, 0, &ctx->c.c2.gc);
                    
                    written = snprintf(route_ptr, remaining, 
                                      "%s{\"destination\":\"%s\",\"netmask\":\"%s\",\"gateway\":\"%s\"}",
                                      route_idx > 0 ? "," : "", network, netmask, gateway);
                    if (written > 0 && written < remaining) {
                        route_ptr += written;
                        remaining -= written;
                        route_idx++;
                    } else {
                        break;
                    }
                }
            }
            
            if (remaining > 1) {
                snprintf(route_ptr, remaining, "]");
            }
        } else {
            strcpy(stats->routes, "[]");
        }
    }

    return 0;
}

void openvpn_bridge_signal(openvpn_bridge_ctx_t *ctx, int signal) {
    if (!ctx) {
        return;
    }

    if (ctx->c.sig) {
        ctx->c.sig->signal_received = signal;
    }
}

void openvpn_bridge_destroy_context(openvpn_bridge_ctx_t *ctx) {
    if (!ctx) {
        return;
    }

    if (ctx->initialized) {
        context_gc_free(&ctx->c);
    }

    if (ctx->es) {
        env_set_destroy(ctx->es);
        ctx->es = NULL;
    }

    free(ctx);
}

/* Routing API Implementation */

typedef struct {
    ovpn_routing_ctx_t *ovpn_routing;
    openvpn_bridge_ctx_t *bridge_ctx;  // Add reference to bridge context
    openvpn_bridge_route_callback_t callback;
    void *user_data;
} openvpn_routing_bridge_ctx_t;

static void openvpn_routing_event_wrapper(ovpn_route_event_type_t event_type,
                                          const ovpn_route_rule_t *rule,
                                          const char *error_message,
                                          void *user_data) {
    openvpn_routing_bridge_ctx_t *bridge_ctx = (openvpn_routing_bridge_ctx_t*)user_data;
    
    if (!bridge_ctx || !bridge_ctx->callback) {
        return;
    }
    
    const char *event_str = "unknown";
    switch (event_type) {
        case OVPN_ROUTE_EVENT_ADDED: event_str = "added"; break;
        case OVPN_ROUTE_EVENT_REMOVED: event_str = "removed"; break;
        case OVPN_ROUTE_EVENT_MODIFIED: event_str = "modified"; break;
        case OVPN_ROUTE_EVENT_DETECTED: event_str = "detected"; break;
        case OVPN_ROUTE_EVENT_FAILED: event_str = "failed"; break;
        case OVPN_ROUTE_EVENT_STATS_UPDATE: event_str = "stats_update"; break;
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

openvpn_routing_ctx_t openvpn_bridge_routing_init(openvpn_bridge_ctx_t *ctx) {
    if (!ctx) {
        return NULL;
    }
    
    const char *interface_name = "tun0";
    if (ctx->c.c1.tuntap) {
        interface_name = ctx->c.c1.tuntap->actual_name;
    }
    
    openvpn_routing_bridge_ctx_t *routing_ctx = malloc(sizeof(openvpn_routing_bridge_ctx_t));
    if (!routing_ctx) {
        return NULL;
    }
    
    routing_ctx->bridge_ctx = ctx;  // Store reference to bridge context
    routing_ctx->ovpn_routing = ovpn_routing_init(interface_name);
    if (!routing_ctx->ovpn_routing) {
        free(routing_ctx);
        return NULL;
    }
    
    routing_ctx->callback = NULL;
    routing_ctx->user_data = NULL;
    
    return (openvpn_routing_ctx_t)routing_ctx;
}

void openvpn_bridge_routing_cleanup(openvpn_routing_ctx_t routing_ctx) {
    if (!routing_ctx) {
        return;
    }
    
    openvpn_routing_bridge_ctx_t *bridge_ctx = (openvpn_routing_bridge_ctx_t*)routing_ctx;
    
    if (bridge_ctx->ovpn_routing) {
        ovpn_routing_cleanup(bridge_ctx->ovpn_routing);
    }
    
    free(bridge_ctx);
}

int openvpn_bridge_routing_add_rule_json(openvpn_routing_ctx_t routing_ctx,
                                         const char *rule_json) {
    if (!routing_ctx || !rule_json) {
        return -1;
    }
    
    openvpn_routing_bridge_ctx_t *bridge_ctx = (openvpn_routing_bridge_ctx_t*)routing_ctx;
    
    return -1;
}

int openvpn_bridge_routing_remove_rule(openvpn_routing_ctx_t routing_ctx,
                                       const char *rule_id) {
    if (!routing_ctx || !rule_id) {
        return -1;
    }
    
    openvpn_routing_bridge_ctx_t *bridge_ctx = (openvpn_routing_bridge_ctx_t*)routing_ctx;
    
    return ovpn_routing_remove_rule(bridge_ctx->ovpn_routing, rule_id);
}

char* openvpn_bridge_routing_get_all_json(openvpn_routing_ctx_t routing_ctx) {
    if (!routing_ctx) {
        return NULL;
    }
    
    openvpn_routing_bridge_ctx_t *bridge_ctx = (openvpn_routing_bridge_ctx_t*)routing_ctx;
    
    return ovpn_routing_export_json(bridge_ctx->ovpn_routing);
}

int openvpn_bridge_routing_apply_pre_connect(openvpn_routing_ctx_t routing_ctx) {
    if (!routing_ctx) {
        return -1;
    }
    
    openvpn_routing_bridge_ctx_t *bridge_ctx = (openvpn_routing_bridge_ctx_t*)routing_ctx;
    
    return ovpn_routing_apply_rules(bridge_ctx->ovpn_routing);
}

int openvpn_bridge_routing_detect_post_connect(openvpn_routing_ctx_t routing_ctx) {
    if (!routing_ctx) {
        return -1;
    }
    
    openvpn_routing_bridge_ctx_t *bridge_ctx = (openvpn_routing_bridge_ctx_t*)routing_ctx;
    
    return ovpn_routing_detect_routes(bridge_ctx->ovpn_routing);
}

void openvpn_bridge_routing_set_callback(openvpn_routing_ctx_t routing_ctx,
                                         openvpn_bridge_route_callback_t callback,
                                         void *user_data) {
    if (!routing_ctx) {
        return;
    }
    
    openvpn_routing_bridge_ctx_t *bridge_ctx = (openvpn_routing_bridge_ctx_t*)routing_ctx;
    
    bridge_ctx->callback = callback;
    bridge_ctx->user_data = user_data;
    
    ovpn_routing_set_callback(bridge_ctx->ovpn_routing, openvpn_routing_event_wrapper, bridge_ctx);
}

/* Route Control System API Implementation */

int openvpn_bridge_routing_set_control_mode(openvpn_routing_ctx_t routing_ctx,
                                           bool prevent_default_routes,
                                           bool selective_routing) {
    if (!routing_ctx) {
        return -1;
    }
    
    openvpn_routing_bridge_ctx_t *bridge_ctx = (openvpn_routing_bridge_ctx_t*)routing_ctx;
    
    if (!bridge_ctx->bridge_ctx || !bridge_ctx->bridge_ctx->c.c1.route_list) {
        return -1;
    }
    
    // Access the route control system from the OpenVPN context
    struct route_control_system *ctrl = &bridge_ctx->bridge_ctx->c.c1.route_list->route_control;
    
    route_control_set_prevent_defaults(ctrl, prevent_default_routes);
    route_control_set_selective_mode(ctrl, selective_routing);
    
    return 0;
}

int openvpn_bridge_routing_set_prevent_defaults(openvpn_routing_ctx_t routing_ctx,
                                               bool prevent) {
    if (!routing_ctx) {
        return -1;
    }
    
    openvpn_routing_bridge_ctx_t *bridge_ctx = (openvpn_routing_bridge_ctx_t*)routing_ctx;
    
    if (!bridge_ctx->bridge_ctx || !bridge_ctx->bridge_ctx->c.c1.route_list) {
        return -1;
    }
    
    // Access the route control system from the OpenVPN context
    struct route_control_system *ctrl = &bridge_ctx->bridge_ctx->c.c1.route_list->route_control;
    
    route_control_set_prevent_defaults(ctrl, prevent);
    
    return 0;
}

int openvpn_bridge_routing_set_selective_mode(openvpn_routing_ctx_t routing_ctx,
                                             bool selective) {
    if (!routing_ctx) {
        return -1;
    }
    
    openvpn_routing_bridge_ctx_t *bridge_ctx = (openvpn_routing_bridge_ctx_t*)routing_ctx;
    
    if (!bridge_ctx->bridge_ctx || !bridge_ctx->bridge_ctx->c.c1.route_list) {
        return -1;
    }
    
    // Access the route control system from the OpenVPN context
    struct route_control_system *ctrl = &bridge_ctx->bridge_ctx->c.c1.route_list->route_control;
    
    route_control_set_selective_mode(ctrl, selective);
    
    return 0;
}

int openvpn_bridge_routing_add_custom_rule(openvpn_routing_ctx_t routing_ctx,
                                          const char *rule_json) {
    if (!routing_ctx || !rule_json) {
        return -1;
    }
    
    openvpn_routing_bridge_ctx_t *bridge_ctx = (openvpn_routing_bridge_ctx_t*)routing_ctx;
    
    // Import the rule from JSON and apply it
    int result = ovpn_routing_import_json(bridge_ctx->ovpn_routing, rule_json);
    if (result == 0) {
        return ovpn_routing_apply_rules(bridge_ctx->ovpn_routing);
    }
    
    return result;
}

char* openvpn_bridge_routing_get_statistics(openvpn_routing_ctx_t routing_ctx) {
    if (!routing_ctx) {
        return NULL;
    }
    
    openvpn_routing_bridge_ctx_t *bridge_ctx = (openvpn_routing_bridge_ctx_t*)routing_ctx;
    
    // Try to get statistics from the route control system first
    if (bridge_ctx->bridge_ctx && bridge_ctx->bridge_ctx->c.c1.route_list) {
        struct route_control_system *ctrl = &bridge_ctx->bridge_ctx->c.c1.route_list->route_control;
        
        if (ctrl->initialized) {
            // Create JSON with real statistics
            char *stats = malloc(512);
            if (stats) {
                snprintf(stats, 512, 
                    "{\"routes_prevented\":%u,\"routes_allowed\":%u,\"default_routes_prevented\":%u,\"last_decision_time\":%ld}",
                    ctrl->routes_prevented,
                    ctrl->routes_allowed, 
                    ctrl->default_routes_prevented,
                    ctrl->last_decision_time);
                return stats;
            }
        }
    }
    
    // Fallback to routing system export or default
    char *stats = ovpn_routing_export_json(bridge_ctx->ovpn_routing);
    if (!stats) {
        // Return a default JSON object if no statistics available
        stats = strdup("{\"routes_prevented\":0,\"routes_allowed\":0,\"default_routes_prevented\":0}");
    }
    
    return stats;
}
