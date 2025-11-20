#ifndef WIREGUARD_ROUTING_H
#define WIREGUARD_ROUTING_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include <netinet/in.h>

#define MAX_ROUTE_RULES 256
#define MAX_ROUTE_DESCRIPTION 512

typedef enum {
    WG_ROUTE_TYPE_AUTOMATIC = 0,
    WG_ROUTE_TYPE_CUSTOM_TUNNEL,
    WG_ROUTE_TYPE_CUSTOM_EXCLUDE,
    WG_ROUTE_TYPE_CUSTOM_GATEWAY
} wg_route_type_t;

typedef enum {
    WG_ROUTE_SRC_ANY = 0,
    WG_ROUTE_SRC_IP_ADDRESS,
    WG_ROUTE_SRC_IP_RANGE,
    WG_ROUTE_SRC_SUBNET,
    WG_ROUTE_SRC_INTERFACE
} wg_route_src_type_t;

typedef enum {
    WG_ROUTE_PROTO_BOTH = 0,
    WG_ROUTE_PROTO_TCP,
    WG_ROUTE_PROTO_UDP,
    WG_ROUTE_PROTO_ICMP
} wg_route_protocol_t;

typedef enum {
    WG_ROUTE_STATE_PENDING = 0,
    WG_ROUTE_STATE_APPLIED,
    WG_ROUTE_STATE_FAILED,
    WG_ROUTE_STATE_REMOVED
} wg_route_state_t;

typedef union {
    struct in_addr ipv4;
    struct in6_addr ipv6;
} wg_ip_addr_t;

typedef struct {
    char id[64];
    char name[256];
    
    wg_route_type_t type;
    bool is_automatic;
    bool user_modified;
    
    wg_route_src_type_t src_type;
    wg_ip_addr_t src_addr;
    uint8_t src_prefix_len;
    char src_interface[32];
    
    wg_ip_addr_t dest_addr;
    uint8_t dest_prefix_len;
    bool is_ipv6;
    
    wg_ip_addr_t gateway;
    bool has_gateway;
    uint32_t metric;
    uint32_t table_id;
    
    wg_route_protocol_t protocol;
    uint16_t port_start;
    uint16_t port_end;
    
    wg_route_state_t state;
    bool enabled;
    bool log_traffic;
    
    char description[MAX_ROUTE_DESCRIPTION];
    time_t created_time;
    time_t modified_time;
    time_t applied_time;
    
    uint64_t packets_routed;
    uint64_t bytes_routed;
    time_t last_used;
} wg_route_rule_t;

typedef enum {
    WG_ROUTE_EVENT_ADDED = 0,
    WG_ROUTE_EVENT_REMOVED,
    WG_ROUTE_EVENT_MODIFIED,
    WG_ROUTE_EVENT_DETECTED,
    WG_ROUTE_EVENT_FAILED,
    WG_ROUTE_EVENT_STATS_UPDATE
} wg_route_event_type_t;

typedef void (*wg_route_event_callback_t)(
    wg_route_event_type_t event_type,
    const wg_route_rule_t *rule,
    const char *error_message,
    void *user_data
);

typedef struct wg_routing_ctx wg_routing_ctx_t;

#endif
