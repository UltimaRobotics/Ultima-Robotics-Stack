#ifndef OPENVPN_C_BRIDGE_H
#define OPENVPN_C_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

// Opaque handle for OpenVPN context
typedef struct openvpn_bridge_ctx openvpn_bridge_ctx_t;

// Connection state enum (mirrors lib-src states)
typedef enum {
    BRIDGE_STATE_INITIAL = 0,
    BRIDGE_STATE_CONNECTING = 1,
    BRIDGE_STATE_WAIT = 2,
    BRIDGE_STATE_AUTH = 3,
    BRIDGE_STATE_GET_CONFIG = 4,
    BRIDGE_STATE_ASSIGN_IP = 5,
    BRIDGE_STATE_ADD_ROUTES = 6,
    BRIDGE_STATE_CONNECTED = 7,
    BRIDGE_STATE_RECONNECTING = 8,
    BRIDGE_STATE_EXITING = 9,
    BRIDGE_STATE_DISCONNECTED = 10,
    BRIDGE_STATE_ERROR = 11
} openvpn_bridge_state_t;

// Statistics structure (POD - safe for C++)
typedef struct {
    uint64_t bytes_sent;
    uint64_t bytes_received;
    uint64_t tun_read_bytes;
    uint64_t tun_write_bytes;
    time_t connected_since;
    uint32_t ping_ms;
    char local_ip[64];
    char remote_ip[64];
    char server_ip[64];
    char interface_name[32];
    char routes[2048];  /* JSON array of route objects */
} openvpn_bridge_stats_t;

// Bridge API functions

/**
 * Initialize OpenVPN static components
 * @return 0 on success, -1 on failure
 */
int openvpn_bridge_init_static(void);

/**
 * Cleanup OpenVPN static components
 */
void openvpn_bridge_uninit_static(void);

/**
 * Create a new OpenVPN context
 * @return Opaque context handle, NULL on failure
 */
openvpn_bridge_ctx_t *openvpn_bridge_create_context(void);

/**
 * Parse configuration file
 * @param ctx Context handle
 * @param config_file Path to configuration file
 * @return 0 on success, negative on failure
 */
int openvpn_bridge_parse_config(openvpn_bridge_ctx_t *ctx, const char *config_file);

/**
 * Initialize context level 1
 * @param ctx Context handle
 * @return 0 on success, negative on failure
 */
int openvpn_bridge_context_init_1(openvpn_bridge_ctx_t *ctx);

/**
 * Run the VPN tunnel (blocks until tunnel exits)
 * This function drives the main OpenVPN event loop based on the configured mode
 * @param ctx Context handle
 * @return 0 on success, negative on failure
 */
int openvpn_bridge_run_tunnel(openvpn_bridge_ctx_t *ctx);

/**
 * Get current connection state
 * @param ctx Context handle
 * @return Current state
 */
openvpn_bridge_state_t openvpn_bridge_get_state(openvpn_bridge_ctx_t *ctx);

/**
 * Check if connected
 * @param ctx Context handle
 * @return true if connected, false otherwise
 */
bool openvpn_bridge_is_connected(openvpn_bridge_ctx_t *ctx);

/**
 * Get statistics snapshot
 * @param ctx Context handle
 * @param stats Output statistics structure
 * @return 0 on success, negative on failure
 */
int openvpn_bridge_get_stats(openvpn_bridge_ctx_t *ctx, openvpn_bridge_stats_t *stats);

/**
 * Signal context to stop
 * @param ctx Context handle
 * @param signal Signal number (e.g., SIGTERM)
 */
void openvpn_bridge_signal(openvpn_bridge_ctx_t *ctx, int signal);

/**
 * Destroy context and free resources
 * @param ctx Context handle
 */
void openvpn_bridge_destroy_context(openvpn_bridge_ctx_t *ctx);

/* Routing context handle */
typedef void* openvpn_routing_ctx_t;

/* Routing event callback */
typedef void (*openvpn_bridge_route_callback_t)(
    const char *event_type,
    const char *rule_json,
    const char *error_msg,
    void *user_data
);

/* Initialize routing for this bridge context */
openvpn_routing_ctx_t openvpn_bridge_routing_init(openvpn_bridge_ctx_t *ctx);

/* Cleanup routing */
void openvpn_bridge_routing_cleanup(openvpn_routing_ctx_t routing_ctx);

/* Add custom route rule (JSON format) */
int openvpn_bridge_routing_add_rule_json(openvpn_routing_ctx_t routing_ctx,
                                         const char *rule_json);

/* Remove route rule */
int openvpn_bridge_routing_remove_rule(openvpn_routing_ctx_t routing_ctx,
                                       const char *rule_id);

/* Get all routes as JSON array */
char* openvpn_bridge_routing_get_all_json(openvpn_routing_ctx_t routing_ctx);

/* Apply routes before connection */
int openvpn_bridge_routing_apply_pre_connect(openvpn_routing_ctx_t routing_ctx);

/* Detect routes after connection */
int openvpn_bridge_routing_detect_post_connect(openvpn_routing_ctx_t routing_ctx);

/* Set routing event callback */
void openvpn_bridge_routing_set_callback(openvpn_routing_ctx_t routing_ctx,
                                         openvpn_bridge_route_callback_t callback,
                                         void *user_data);

/* Route Control System API */
int openvpn_bridge_routing_set_control_mode(openvpn_routing_ctx_t routing_ctx,
                                           bool prevent_default_routes,
                                           bool selective_routing);

int openvpn_bridge_routing_set_prevent_defaults(openvpn_routing_ctx_t routing_ctx,
                                               bool prevent);

int openvpn_bridge_routing_set_selective_mode(openvpn_routing_ctx_t routing_ctx,
                                             bool selective);

int openvpn_bridge_routing_add_custom_rule(openvpn_routing_ctx_t routing_ctx,
                                          const char *rule_json);

char* openvpn_bridge_routing_get_statistics(openvpn_routing_ctx_t routing_ctx);

#ifdef __cplusplus
}
#endif

#endif // OPENVPN_C_BRIDGE_H
