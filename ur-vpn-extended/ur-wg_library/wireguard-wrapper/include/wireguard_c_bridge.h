
#ifndef WIREGUARD_C_BRIDGE_H
#define WIREGUARD_C_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declaration */
typedef struct wireguard_bridge_ctx wireguard_bridge_ctx_t;

/* Connection states */
typedef enum {
    WG_STATE_INITIAL = 0,
    WG_STATE_CONFIGURING = 1,
    WG_STATE_HANDSHAKING = 2,
    WG_STATE_CONNECTED = 3,
    WG_STATE_RECONNECTING = 4,
    WG_STATE_DISCONNECTED = 5,
    WG_STATE_ERROR = 6
} wireguard_bridge_state_t;

/* Statistics structure */
typedef struct {
    uint64_t bytes_sent;
    uint64_t bytes_received;
    uint64_t tx_packets;
    uint64_t rx_packets;
    time_t last_handshake;
    uint32_t latency_ms;
    char endpoint[128];
    char allowed_ips[512];
    char public_key[45];  /* Base64 encoded */
    char local_ip[64];
    int connected_duration;
    uint64_t upload_rate_bps;    /* Calculated bandwidth */
    uint64_t download_rate_bps;  /* Calculated bandwidth */
    char interface_name[32];
    char routes[2048];  /* JSON array of route objects */
} wireguard_bridge_stats_t;

/* Statistics callback function type */
typedef void (*wireguard_stats_callback_t)(const wireguard_bridge_stats_t* stats, void* user_data);

/* Configuration structure */
typedef struct {
    char interface_name[16];
    char private_key[45];
    char listen_port[6];
    char peer_public_key[45];
    char peer_endpoint[128];
    char allowed_ips[512];
    char preshared_key[45];
    int persistent_keepalive;
    char addresses[16][64];  /* IP addresses to assign */
    int address_count;
    char dns_servers[8][64]; /* DNS servers */
    int dns_count;
    int mtu;                 /* MTU value */
} wireguard_bridge_config_t;

/* Core API functions */
int wireguard_bridge_init_static(void);
void wireguard_bridge_uninit_static(void);

wireguard_bridge_ctx_t* wireguard_bridge_create_context(void);
void wireguard_bridge_destroy_context(wireguard_bridge_ctx_t* ctx);

int wireguard_bridge_parse_config(wireguard_bridge_ctx_t* ctx, const char* config_file);
int wireguard_bridge_set_config(wireguard_bridge_ctx_t* ctx, const wireguard_bridge_config_t* config);
int wireguard_bridge_set_interface(wireguard_bridge_ctx_t* ctx, const char* interface_name);
int wireguard_bridge_add_peer(wireguard_bridge_ctx_t* ctx, const char* public_key, 
                               const char* endpoint, const char* allowed_ips);

int wireguard_bridge_connect(wireguard_bridge_ctx_t* ctx);
int wireguard_bridge_connect_full(wireguard_bridge_ctx_t* ctx, bool setup_routing, bool setup_dns);
int wireguard_bridge_disconnect(wireguard_bridge_ctx_t* ctx);
int wireguard_bridge_reconnect(wireguard_bridge_ctx_t* ctx);

/* Additional setup functions */
int wireguard_bridge_create_interface(wireguard_bridge_ctx_t* ctx, const char* interface_name);
int wireguard_bridge_assign_addresses(wireguard_bridge_ctx_t* ctx);
int wireguard_bridge_set_mtu(wireguard_bridge_ctx_t* ctx, int mtu);
int wireguard_bridge_bring_up_interface(wireguard_bridge_ctx_t* ctx);
int wireguard_bridge_setup_routes(wireguard_bridge_ctx_t* ctx);
int wireguard_bridge_setup_dns(wireguard_bridge_ctx_t* ctx);
int wireguard_bridge_cleanup_interface(wireguard_bridge_ctx_t* ctx);

wireguard_bridge_state_t wireguard_bridge_get_state(wireguard_bridge_ctx_t* ctx);
int wireguard_bridge_get_stats(wireguard_bridge_ctx_t* ctx, wireguard_bridge_stats_t* stats);
bool wireguard_bridge_is_connected(wireguard_bridge_ctx_t* ctx);

const char* wireguard_bridge_get_last_error(wireguard_bridge_ctx_t* ctx);
void wireguard_bridge_set_state(wireguard_bridge_ctx_t* ctx, wireguard_bridge_state_t state);

/* Non-blocking stats monitoring */
int wireguard_bridge_start_stats_monitor(wireguard_bridge_ctx_t* ctx, 
                                          wireguard_stats_callback_t callback,
                                          void* user_data,
                                          unsigned int interval_ms);
void wireguard_bridge_stop_stats_monitor(wireguard_bridge_ctx_t* ctx);
int wireguard_bridge_get_cached_stats(wireguard_bridge_ctx_t* ctx, wireguard_bridge_stats_t* stats);

/* Routing context handle */
typedef void* wireguard_routing_ctx_t;

/* Routing event callback */
typedef void (*wireguard_bridge_route_callback_t)(
    const char *event_type,
    const char *rule_json,
    const char *error_msg,
    void *user_data
);

/* Initialize routing for this bridge context */
wireguard_routing_ctx_t wireguard_bridge_routing_init(wireguard_bridge_ctx_t *ctx);

/* Cleanup routing */
void wireguard_bridge_routing_cleanup(wireguard_routing_ctx_t routing_ctx);

/* Add custom route rule (JSON format) */
int wireguard_bridge_routing_add_rule_json(wireguard_routing_ctx_t routing_ctx,
                                           const char *rule_json);

/* Remove route rule */
int wireguard_bridge_routing_remove_rule(wireguard_routing_ctx_t routing_ctx,
                                         const char *rule_id);

/* Get all routes as JSON array */
char* wireguard_bridge_routing_get_all_json(wireguard_routing_ctx_t routing_ctx);

/* Apply routes before connection */
int wireguard_bridge_routing_apply_pre_connect(wireguard_routing_ctx_t routing_ctx);

/* Detect routes after connection */
int wireguard_bridge_routing_detect_post_connect(wireguard_routing_ctx_t routing_ctx);

/* Set routing event callback */
void wireguard_bridge_routing_set_callback(wireguard_routing_ctx_t routing_ctx,
                                           wireguard_bridge_route_callback_t callback,
                                           void *user_data);

#ifdef __cplusplus
}
#endif

#endif /* WIREGUARD_C_BRIDGE_H */
