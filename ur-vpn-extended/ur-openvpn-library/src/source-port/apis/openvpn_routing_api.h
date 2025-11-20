#ifndef OPENVPN_ROUTING_API_H
#define OPENVPN_ROUTING_API_H

#include "openvpn_routing.h"

ovpn_routing_ctx_t* ovpn_routing_init(const char *interface_name);

void ovpn_routing_cleanup(ovpn_routing_ctx_t *ctx);

void ovpn_routing_set_callback(ovpn_routing_ctx_t *ctx,
                               ovpn_route_event_callback_t callback,
                               void *user_data);

int ovpn_routing_add_rule(ovpn_routing_ctx_t *ctx, const ovpn_route_rule_t *rule);

int ovpn_routing_remove_rule(ovpn_routing_ctx_t *ctx, const char *rule_id);

int ovpn_routing_update_rule(ovpn_routing_ctx_t *ctx,
                             const char *rule_id,
                             const ovpn_route_rule_t *updated_rule);

int ovpn_routing_get_rule(ovpn_routing_ctx_t *ctx,
                          const char *rule_id,
                          ovpn_route_rule_t *out_rule);

int ovpn_routing_get_all_rules(ovpn_routing_ctx_t *ctx,
                               ovpn_route_rule_t **out_rules,
                               size_t *out_count);

int ovpn_routing_apply_rules(ovpn_routing_ctx_t *ctx);

int ovpn_routing_clear_routes(ovpn_routing_ctx_t *ctx);

int ovpn_routing_detect_routes(ovpn_routing_ctx_t *ctx);

int ovpn_routing_start_monitoring(ovpn_routing_ctx_t *ctx, int interval_ms);

void ovpn_routing_stop_monitoring(ovpn_routing_ctx_t *ctx);

char* ovpn_routing_export_json(ovpn_routing_ctx_t *ctx);

int ovpn_routing_import_json(ovpn_routing_ctx_t *ctx, const char *json_str);

int ovpn_routing_get_rule_stats(ovpn_routing_ctx_t *ctx,
                                const char *rule_id,
                                uint64_t *packets,
                                uint64_t *bytes);

int ovpn_routing_hook_openvpn(ovpn_routing_ctx_t *ctx, void *openvpn_ctx);

#endif
