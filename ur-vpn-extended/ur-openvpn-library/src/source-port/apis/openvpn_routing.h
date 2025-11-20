#ifndef OPENVPN_ROUTING_H
#define OPENVPN_ROUTING_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include <netinet/in.h>

#define OVPN_MAX_ROUTE_RULES 256
#define OVPN_MAX_ROUTE_DESC 512

typedef enum {
    OVPN_ROUTE_TYPE_AUTOMATIC = 0,
    OVPN_ROUTE_TYPE_CUSTOM_TUNNEL,
    OVPN_ROUTE_TYPE_CUSTOM_EXCLUDE,
    OVPN_ROUTE_TYPE_CUSTOM_GATEWAY
} ovpn_route_type_t;

typedef enum {
    OVPN_ROUTE_SRC_ANY = 0,
    OVPN_ROUTE_SRC_IP_ADDRESS,
    OVPN_ROUTE_SRC_IP_RANGE,
    OVPN_ROUTE_SRC_SUBNET,
    OVPN_ROUTE_SRC_INTERFACE
} ovpn_route_src_type_t;

typedef enum {
    OVPN_ROUTE_PROTO_BOTH = 0,
    OVPN_ROUTE_PROTO_TCP,
    OVPN_ROUTE_PROTO_UDP,
    OVPN_ROUTE_PROTO_ICMP
} ovpn_route_protocol_t;

typedef enum {
    OVPN_ROUTE_STATE_PENDING = 0,
    OVPN_ROUTE_STATE_APPLIED,
    OVPN_ROUTE_STATE_FAILED,
    OVPN_ROUTE_STATE_REMOVED
} ovpn_route_state_t;

typedef union {
    struct in_addr ipv4;
    struct in6_addr ipv6;
} ovpn_ip_addr_t;

typedef struct {
    char id[64];
    char name[256];
    
    ovpn_route_type_t type;
    bool is_automatic;
    bool user_modified;
    
    ovpn_route_src_type_t src_type;
    ovpn_ip_addr_t src_addr;
    uint8_t src_prefix_len;
    char src_interface[32];
    
    ovpn_ip_addr_t dest_addr;
    uint8_t dest_prefix_len;
    bool is_ipv6;
    
    ovpn_ip_addr_t gateway;
    bool has_gateway;
    uint32_t metric;
    uint32_t table_id;
    
    ovpn_route_protocol_t protocol;
    uint16_t port_start;
    uint16_t port_end;
    
    ovpn_route_state_t state;
    bool enabled;
    bool log_traffic;
    
    char description[OVPN_MAX_ROUTE_DESC];
    time_t created_time;
    time_t modified_time;
    time_t applied_time;
    
    uint64_t packets_routed;
    uint64_t bytes_routed;
    time_t last_used;
} ovpn_route_rule_t;

typedef enum {
    OVPN_ROUTE_EVENT_ADDED = 0,
    OVPN_ROUTE_EVENT_REMOVED,
    OVPN_ROUTE_EVENT_MODIFIED,
    OVPN_ROUTE_EVENT_DETECTED,
    OVPN_ROUTE_EVENT_FAILED,
    OVPN_ROUTE_EVENT_STATS_UPDATE
} ovpn_route_event_type_t;

typedef void (*ovpn_route_event_callback_t)(
    ovpn_route_event_type_t event_type,
    const ovpn_route_rule_t *rule,
    const char *error_message,
    void *user_data
);

typedef struct ovpn_routing_ctx ovpn_routing_ctx_t;

#endif
