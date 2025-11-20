#include "openvpn_routing.h"
#include "openvpn_routing_api.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>

#define MAX_RULES 256

struct ovpn_routing_ctx {
    char interface_name[32];
    ovpn_route_rule_t rules[MAX_RULES];
    size_t rule_count;
    
    ovpn_route_event_callback_t callback;
    void *callback_user_data;
    
    pthread_t monitor_thread;
    bool monitor_running;
    int monitor_interval_ms;
    
    void *openvpn_ctx;
    
    pthread_mutex_t lock;
};

static int execute_command(const char *cmd, char *output, size_t output_size) {
    FILE *fp = popen(cmd, "r");
    if (!fp) {
        return -1;
    }
    
    if (output && output_size > 0) {
        size_t total_read = 0;
        while (total_read < output_size - 1 && 
               fgets(output + total_read, output_size - total_read, fp) != NULL) {
            total_read = strlen(output);
        }
        output[total_read] = '\0';
    }
    
    int status = pclose(fp);
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

static void fire_event(ovpn_routing_ctx_t *ctx, ovpn_route_event_type_t event_type,
                      const ovpn_route_rule_t *rule, const char *error_msg) {
    if (ctx->callback) {
        ctx->callback(event_type, rule, error_msg, ctx->callback_user_data);
    }
}

static int find_rule_index(ovpn_routing_ctx_t *ctx, const char *rule_id) {
    for (size_t i = 0; i < ctx->rule_count; i++) {
        if (strcmp(ctx->rules[i].id, rule_id) == 0) {
            return (int)i;
        }
    }
    return -1;
}

static int apply_route_rule(ovpn_routing_ctx_t *ctx, ovpn_route_rule_t *rule) {
    char cmd[512];
    char dest_str[64];
    
    if (rule->is_ipv6) {
        char addr_buf[INET6_ADDRSTRLEN];
        inet_ntop(AF_INET6, &rule->dest_addr.ipv6, addr_buf, sizeof(addr_buf));
        snprintf(dest_str, sizeof(dest_str), "%s/%u", addr_buf, rule->dest_prefix_len);
    } else {
        char addr_buf[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &rule->dest_addr.ipv4, addr_buf, sizeof(addr_buf));
        snprintf(dest_str, sizeof(dest_str), "%s/%u", addr_buf, rule->dest_prefix_len);
    }
    
    const char *ip_cmd = rule->is_ipv6 ? "ip -6" : "ip";
    
    switch (rule->type) {
        case OVPN_ROUTE_TYPE_AUTOMATIC:
        case OVPN_ROUTE_TYPE_CUSTOM_TUNNEL:
            snprintf(cmd, sizeof(cmd), "%s route add %s dev %s metric %u 2>/dev/null",
                    ip_cmd, dest_str, ctx->interface_name, rule->metric);
            break;
            
        case OVPN_ROUTE_TYPE_CUSTOM_GATEWAY:
            if (rule->has_gateway) {
                char gw_str[INET6_ADDRSTRLEN];
                if (rule->is_ipv6) {
                    inet_ntop(AF_INET6, &rule->gateway.ipv6, gw_str, sizeof(gw_str));
                } else {
                    inet_ntop(AF_INET, &rule->gateway.ipv4, gw_str, sizeof(gw_str));
                }
                snprintf(cmd, sizeof(cmd), "%s route add %s via %s dev %s metric %u 2>/dev/null",
                        ip_cmd, dest_str, gw_str, ctx->interface_name, rule->metric);
            } else {
                return -1;
            }
            break;
            
        case OVPN_ROUTE_TYPE_CUSTOM_EXCLUDE:
            snprintf(cmd, sizeof(cmd), "%s route add %s metric %u 2>/dev/null",
                    ip_cmd, dest_str, rule->metric);
            break;
            
        default:
            return -1;
    }
    
    int result = execute_command(cmd, NULL, 0);
    
    if (result == 0) {
        rule->state = OVPN_ROUTE_STATE_APPLIED;
        rule->applied_time = time(NULL);
        fire_event(ctx, OVPN_ROUTE_EVENT_ADDED, rule, NULL);
    } else {
        rule->state = OVPN_ROUTE_STATE_FAILED;
        fire_event(ctx, OVPN_ROUTE_EVENT_FAILED, rule, "Failed to apply route");
    }
    
    return result;
}

static int remove_route_rule(ovpn_routing_ctx_t *ctx, ovpn_route_rule_t *rule) {
    char cmd[512];
    char dest_str[64];
    
    if (rule->is_ipv6) {
        char addr_buf[INET6_ADDRSTRLEN];
        inet_ntop(AF_INET6, &rule->dest_addr.ipv6, addr_buf, sizeof(addr_buf));
        snprintf(dest_str, sizeof(dest_str), "%s/%u", addr_buf, rule->dest_prefix_len);
    } else {
        char addr_buf[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &rule->dest_addr.ipv4, addr_buf, sizeof(addr_buf));
        snprintf(dest_str, sizeof(dest_str), "%s/%u", addr_buf, rule->dest_prefix_len);
    }
    
    const char *ip_cmd = rule->is_ipv6 ? "ip -6" : "ip";
    
    snprintf(cmd, sizeof(cmd), "%s route del %s 2>/dev/null", ip_cmd, dest_str);
    
    int result = execute_command(cmd, NULL, 0);
    
    if (result == 0) {
        rule->state = OVPN_ROUTE_STATE_REMOVED;
        fire_event(ctx, OVPN_ROUTE_EVENT_REMOVED, rule, NULL);
    }
    
    return result;
}

ovpn_routing_ctx_t* ovpn_routing_init(const char *interface_name) {
    if (!interface_name) {
        return NULL;
    }
    
    ovpn_routing_ctx_t *ctx = malloc(sizeof(ovpn_routing_ctx_t));
    if (!ctx) {
        return NULL;
    }
    
    memset(ctx, 0, sizeof(ovpn_routing_ctx_t));
    strncpy(ctx->interface_name, interface_name, sizeof(ctx->interface_name) - 1);
    
    pthread_mutex_init(&ctx->lock, NULL);
    
    return ctx;
}

void ovpn_routing_cleanup(ovpn_routing_ctx_t *ctx) {
    if (!ctx) {
        return;
    }
    
    ovpn_routing_stop_monitoring(ctx);
    
    pthread_mutex_destroy(&ctx->lock);
    free(ctx);
}

void ovpn_routing_set_callback(ovpn_routing_ctx_t *ctx,
                               ovpn_route_event_callback_t callback,
                               void *user_data) {
    if (!ctx) {
        return;
    }
    
    pthread_mutex_lock(&ctx->lock);
    ctx->callback = callback;
    ctx->callback_user_data = user_data;
    pthread_mutex_unlock(&ctx->lock);
}

int ovpn_routing_add_rule(ovpn_routing_ctx_t *ctx, const ovpn_route_rule_t *rule) {
    if (!ctx || !rule) {
        return -1;
    }
    
    pthread_mutex_lock(&ctx->lock);
    
    if (ctx->rule_count >= MAX_RULES) {
        pthread_mutex_unlock(&ctx->lock);
        return -1;
    }
    
    if (find_rule_index(ctx, rule->id) >= 0) {
        pthread_mutex_unlock(&ctx->lock);
        return -1;
    }
    
    memcpy(&ctx->rules[ctx->rule_count], rule, sizeof(ovpn_route_rule_t));
    ctx->rules[ctx->rule_count].state = OVPN_ROUTE_STATE_PENDING;
    ctx->rule_count++;
    
    pthread_mutex_unlock(&ctx->lock);
    
    return 0;
}

int ovpn_routing_remove_rule(ovpn_routing_ctx_t *ctx, const char *rule_id) {
    if (!ctx || !rule_id) {
        return -1;
    }
    
    pthread_mutex_lock(&ctx->lock);
    
    int idx = find_rule_index(ctx, rule_id);
    if (idx < 0) {
        pthread_mutex_unlock(&ctx->lock);
        return -1;
    }
    
    if (ctx->rules[idx].state == OVPN_ROUTE_STATE_APPLIED) {
        remove_route_rule(ctx, &ctx->rules[idx]);
    }
    
    for (size_t i = idx; i < ctx->rule_count - 1; i++) {
        memcpy(&ctx->rules[i], &ctx->rules[i + 1], sizeof(ovpn_route_rule_t));
    }
    ctx->rule_count--;
    
    pthread_mutex_unlock(&ctx->lock);
    
    return 0;
}

int ovpn_routing_update_rule(ovpn_routing_ctx_t *ctx,
                             const char *rule_id,
                             const ovpn_route_rule_t *updated_rule) {
    if (!ctx || !rule_id || !updated_rule) {
        return -1;
    }
    
    pthread_mutex_lock(&ctx->lock);
    
    int idx = find_rule_index(ctx, rule_id);
    if (idx < 0) {
        pthread_mutex_unlock(&ctx->lock);
        return -1;
    }
    
    if (ctx->rules[idx].state == OVPN_ROUTE_STATE_APPLIED) {
        remove_route_rule(ctx, &ctx->rules[idx]);
    }
    
    memcpy(&ctx->rules[idx], updated_rule, sizeof(ovpn_route_rule_t));
    ctx->rules[idx].state = OVPN_ROUTE_STATE_PENDING;
    ctx->rules[idx].modified_time = time(NULL);
    
    pthread_mutex_unlock(&ctx->lock);
    
    return 0;
}

int ovpn_routing_get_rule(ovpn_routing_ctx_t *ctx,
                          const char *rule_id,
                          ovpn_route_rule_t *out_rule) {
    if (!ctx || !rule_id || !out_rule) {
        return -1;
    }
    
    pthread_mutex_lock(&ctx->lock);
    
    int idx = find_rule_index(ctx, rule_id);
    if (idx < 0) {
        pthread_mutex_unlock(&ctx->lock);
        return -1;
    }
    
    memcpy(out_rule, &ctx->rules[idx], sizeof(ovpn_route_rule_t));
    
    pthread_mutex_unlock(&ctx->lock);
    
    return 0;
}

int ovpn_routing_get_all_rules(ovpn_routing_ctx_t *ctx,
                               ovpn_route_rule_t **out_rules,
                               size_t *out_count) {
    if (!ctx || !out_rules || !out_count) {
        return -1;
    }
    
    pthread_mutex_lock(&ctx->lock);
    
    if (ctx->rule_count == 0) {
        *out_rules = NULL;
        *out_count = 0;
        pthread_mutex_unlock(&ctx->lock);
        return 0;
    }
    
    *out_rules = malloc(sizeof(ovpn_route_rule_t) * ctx->rule_count);
    if (!*out_rules) {
        pthread_mutex_unlock(&ctx->lock);
        return -1;
    }
    
    memcpy(*out_rules, ctx->rules, sizeof(ovpn_route_rule_t) * ctx->rule_count);
    *out_count = ctx->rule_count;
    
    pthread_mutex_unlock(&ctx->lock);
    
    return 0;
}

int ovpn_routing_apply_rules(ovpn_routing_ctx_t *ctx) {
    if (!ctx) {
        return -1;
    }
    
    pthread_mutex_lock(&ctx->lock);
    
    int applied = 0;
    for (size_t i = 0; i < ctx->rule_count; i++) {
        if (ctx->rules[i].enabled && ctx->rules[i].state != OVPN_ROUTE_STATE_APPLIED) {
            if (apply_route_rule(ctx, &ctx->rules[i]) == 0) {
                applied++;
            }
        }
    }
    
    pthread_mutex_unlock(&ctx->lock);
    
    return applied;
}

int ovpn_routing_clear_routes(ovpn_routing_ctx_t *ctx) {
    if (!ctx) {
        return -1;
    }
    
    pthread_mutex_lock(&ctx->lock);
    
    int removed = 0;
    for (size_t i = 0; i < ctx->rule_count; i++) {
        if (ctx->rules[i].state == OVPN_ROUTE_STATE_APPLIED) {
            if (remove_route_rule(ctx, &ctx->rules[i]) == 0) {
                removed++;
            }
        }
    }
    
    pthread_mutex_unlock(&ctx->lock);
    
    return removed;
}

int ovpn_routing_detect_routes(ovpn_routing_ctx_t *ctx) {
    if (!ctx) {
        return -1;
    }
    
    char cmd[256];
    char output[4096];
    
    snprintf(cmd, sizeof(cmd), "ip route show dev %s 2>/dev/null", ctx->interface_name);
    
    if (execute_command(cmd, output, sizeof(output)) != 0) {
        return -1;
    }
    
    pthread_mutex_lock(&ctx->lock);
    
    char *line = strtok(output, "\n");
    int detected = 0;
    
    while (line != NULL) {
        ovpn_route_rule_t rule = {0};
        
        char dest[64];
        unsigned int prefix_len;
        unsigned int metric = 0;
        
        if (sscanf(line, "%63s metric %u", dest, &metric) >= 1 ||
            sscanf(line, "%63s", dest) >= 1) {
            
            char *slash = strchr(dest, '/');
            if (slash) {
                *slash = '\0';
                prefix_len = atoi(slash + 1);
            } else {
                prefix_len = 32;
            }
            
            if (inet_pton(AF_INET, dest, &rule.dest_addr.ipv4) == 1) {
                rule.is_ipv6 = false;
                rule.dest_prefix_len = prefix_len;
                
                snprintf(rule.id, sizeof(rule.id), "auto_%s_%u", dest, detected);
                snprintf(rule.name, sizeof(rule.name), "Auto: %s/%u", dest, prefix_len);
                rule.type = OVPN_ROUTE_TYPE_AUTOMATIC;
                rule.is_automatic = true;
                rule.metric = metric;
                rule.state = OVPN_ROUTE_STATE_APPLIED;
                rule.enabled = true;
                rule.created_time = time(NULL);
                
                if (ctx->rule_count < MAX_RULES) {
                    if (find_rule_index(ctx, rule.id) < 0) {
                        memcpy(&ctx->rules[ctx->rule_count], &rule, sizeof(ovpn_route_rule_t));
                        ctx->rule_count++;
                        fire_event(ctx, OVPN_ROUTE_EVENT_DETECTED, &rule, NULL);
                        detected++;
                    }
                }
            }
        }
        
        line = strtok(NULL, "\n");
    }
    
    pthread_mutex_unlock(&ctx->lock);
    
    return detected;
}

static void* monitor_thread_func(void *arg) {
    ovpn_routing_ctx_t *ctx = (ovpn_routing_ctx_t*)arg;
    
    while (ctx->monitor_running) {
        ovpn_routing_detect_routes(ctx);
        
        usleep(ctx->monitor_interval_ms * 1000);
    }
    
    return NULL;
}

int ovpn_routing_start_monitoring(ovpn_routing_ctx_t *ctx, int interval_ms) {
    if (!ctx || ctx->monitor_running) {
        return -1;
    }
    
    ctx->monitor_interval_ms = interval_ms;
    ctx->monitor_running = true;
    
    if (pthread_create(&ctx->monitor_thread, NULL, monitor_thread_func, ctx) != 0) {
        ctx->monitor_running = false;
        return -1;
    }
    
    return 0;
}

void ovpn_routing_stop_monitoring(ovpn_routing_ctx_t *ctx) {
    if (!ctx || !ctx->monitor_running) {
        return;
    }
    
    ctx->monitor_running = false;
    pthread_join(ctx->monitor_thread, NULL);
}

char* ovpn_routing_export_json(ovpn_routing_ctx_t *ctx) {
    if (!ctx) {
        return NULL;
    }
    
    pthread_mutex_lock(&ctx->lock);
    
    char *json = malloc(65536);
    if (!json) {
        pthread_mutex_unlock(&ctx->lock);
        return NULL;
    }
    
    strcpy(json, "{\"rules\":[");
    
    for (size_t i = 0; i < ctx->rule_count; i++) {
        char rule_json[2048];
        char dest_str[INET6_ADDRSTRLEN];
        
        if (ctx->rules[i].is_ipv6) {
            inet_ntop(AF_INET6, &ctx->rules[i].dest_addr.ipv6, dest_str, sizeof(dest_str));
        } else {
            inet_ntop(AF_INET, &ctx->rules[i].dest_addr.ipv4, dest_str, sizeof(dest_str));
        }
        
        snprintf(rule_json, sizeof(rule_json),
                "%s{\"id\":\"%s\",\"name\":\"%s\",\"destination\":\"%s/%u\","
                "\"type\":%d,\"metric\":%u,\"enabled\":%s,\"is_automatic\":%s}",
                i > 0 ? "," : "",
                ctx->rules[i].id,
                ctx->rules[i].name,
                dest_str,
                ctx->rules[i].dest_prefix_len,
                ctx->rules[i].type,
                ctx->rules[i].metric,
                ctx->rules[i].enabled ? "true" : "false",
                ctx->rules[i].is_automatic ? "true" : "false");
        
        strcat(json, rule_json);
    }
    
    strcat(json, "]}");
    
    pthread_mutex_unlock(&ctx->lock);
    
    return json;
}

int ovpn_routing_import_json(ovpn_routing_ctx_t *ctx, const char *json_str) {
    if (!ctx || !json_str) {
        return -1;
    }
    
    return -1;
}

int ovpn_routing_get_rule_stats(ovpn_routing_ctx_t *ctx,
                                const char *rule_id,
                                uint64_t *packets,
                                uint64_t *bytes) {
    if (!ctx || !rule_id) {
        return -1;
    }
    
    pthread_mutex_lock(&ctx->lock);
    
    int idx = find_rule_index(ctx, rule_id);
    if (idx < 0) {
        pthread_mutex_unlock(&ctx->lock);
        return -1;
    }
    
    if (packets) {
        *packets = ctx->rules[idx].packets_routed;
    }
    if (bytes) {
        *bytes = ctx->rules[idx].bytes_routed;
    }
    
    pthread_mutex_unlock(&ctx->lock);
    
    return 0;
}

int ovpn_routing_hook_openvpn(ovpn_routing_ctx_t *ctx, void *openvpn_ctx) {
    if (!ctx) {
        return -1;
    }
    
    pthread_mutex_lock(&ctx->lock);
    ctx->openvpn_ctx = openvpn_ctx;
    pthread_mutex_unlock(&ctx->lock);
    
    return 0;
}
