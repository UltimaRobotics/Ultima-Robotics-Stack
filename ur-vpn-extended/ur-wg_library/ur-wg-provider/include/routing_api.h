#ifndef WIREGUARD_ROUTING_API_H
#define WIREGUARD_ROUTING_API_H

#include "routing.h"

wg_routing_ctx_t* wg_routing_init(const char *interface_name);

void wg_routing_cleanup(wg_routing_ctx_t *ctx);

void wg_routing_set_callback(wg_routing_ctx_t *ctx, 
                             wg_route_event_callback_t callback,
                             void *user_data);

int wg_routing_add_rule(wg_routing_ctx_t *ctx, const wg_route_rule_t *rule);

int wg_routing_remove_rule(wg_routing_ctx_t *ctx, const char *rule_id);

int wg_routing_update_rule(wg_routing_ctx_t *ctx, 
                           const char *rule_id,
                           const wg_route_rule_t *updated_rule);

int wg_routing_get_rule(wg_routing_ctx_t *ctx, 
                        const char *rule_id,
                        wg_route_rule_t *out_rule);

int wg_routing_get_all_rules(wg_routing_ctx_t *ctx,
                             wg_route_rule_t **out_rules,
                             size_t *out_count);

int wg_routing_apply_rules(wg_routing_ctx_t *ctx);

int wg_routing_clear_routes(wg_routing_ctx_t *ctx);

int wg_routing_detect_routes(wg_routing_ctx_t *ctx);

int wg_routing_start_monitoring(wg_routing_ctx_t *ctx, int interval_ms);

void wg_routing_stop_monitoring(wg_routing_ctx_t *ctx);

char* wg_routing_export_json(wg_routing_ctx_t *ctx);

int wg_routing_import_json(wg_routing_ctx_t *ctx, const char *json_str);

int wg_routing_get_rule_stats(wg_routing_ctx_t *ctx,
                              const char *rule_id,
                              uint64_t *packets,
                              uint64_t *bytes);

#endif
