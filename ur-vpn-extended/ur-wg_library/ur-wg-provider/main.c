
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <arpa/inet.h>
#include <net/if.h>

#include "containers.h"
#include "config.h"
#include "encoding.h"
#include "ipc.h"

#define MAX_LINE_LENGTH 4096
#define MAX_ADDRESSES 16
#define MAX_DNS_SERVERS 8
#define MAX_ROUTES 256

static volatile sig_atomic_t should_exit = 0;
static char global_interface[IFNAMSIZ] = {0};

struct tunnel_config {
    char addresses[MAX_ADDRESSES][64];
    int address_count;
    char dns_servers[MAX_DNS_SERVERS][64];
    int dns_count;
    char routes[MAX_ROUTES][64];
    int route_count;
    int mtu;
};

static void signal_handler(int signum) {
    should_exit = 1;
}

static void setup_signal_handlers(void) {
    struct sigaction sa = {0};
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
}

static void print_usage(const char *prog_name) {
    fprintf(stderr, "Usage: %s <config-file> [interface-name] [options]\n", prog_name);
    fprintf(stderr, "\n");
    fprintf(stderr, "Start a WireGuard tunnel from a configuration file.\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Arguments:\n");
    fprintf(stderr, "  <config-file>      Path to WireGuard configuration file\n");
    fprintf(stderr, "  [interface-name]   Optional interface name (default: wg0)\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  -no-bypass         Don't add routing rules (only establish connection)\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Example:\n");
    fprintf(stderr, "  %s /etc/wireguard/wg0.conf\n", prog_name);
    fprintf(stderr, "  %s /etc/wireguard/wg0.conf wg0\n", prog_name);
    fprintf(stderr, "  %s /etc/wireguard/wg0.conf wg0 -no-bypass\n", prog_name);
}

static void parse_additional_config(const char *config_file, struct tunnel_config *tun_cfg) {
    FILE *fp;
    char line[MAX_LINE_LENGTH];
    int in_interface_section = 0;
    
    memset(tun_cfg, 0, sizeof(*tun_cfg));
    tun_cfg->mtu = 0;
    
    fp = fopen(config_file, "r");
    if (!fp) {
        return;
    }
    
    while (fgets(line, sizeof(line), fp)) {
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n')
            line[len - 1] = '\0';
        
        /* Remove leading/trailing whitespace */
        char *trimmed = line;
        while (*trimmed == ' ' || *trimmed == '\t') trimmed++;
        
        if (strncasecmp(trimmed, "[Interface]", 11) == 0) {
            in_interface_section = 1;
            continue;
        } else if (trimmed[0] == '[') {
            in_interface_section = 0;
            continue;
        }
        
        if (!in_interface_section)
            continue;
        
        /* Parse Address */
        if (strncasecmp(trimmed, "Address", 7) == 0) {
            char *value = strchr(trimmed, '=');
            if (value && tun_cfg->address_count < MAX_ADDRESSES) {
                value++;
                while (*value == ' ' || *value == '\t') value++;
                
                /* Handle comma-separated addresses */
                char *addr_copy = strdup(value);
                char *addr = strtok(addr_copy, ",");
                while (addr && tun_cfg->address_count < MAX_ADDRESSES) {
                    while (*addr == ' ' || *addr == '\t') addr++;
                    char *end = addr + strlen(addr) - 1;
                    while (end > addr && (*end == ' ' || *end == '\t')) *end-- = '\0';
                    
                    strncpy(tun_cfg->addresses[tun_cfg->address_count], addr, 63);
                    tun_cfg->addresses[tun_cfg->address_count][63] = '\0';
                    tun_cfg->address_count++;
                    
                    addr = strtok(NULL, ",");
                }
                free(addr_copy);
            }
        }
        /* Parse DNS */
        else if (strncasecmp(trimmed, "DNS", 3) == 0) {
            char *value = strchr(trimmed, '=');
            if (value && tun_cfg->dns_count < MAX_DNS_SERVERS) {
                value++;
                while (*value == ' ' || *value == '\t') value++;
                
                char *dns_copy = strdup(value);
                char *dns = strtok(dns_copy, ",");
                while (dns && tun_cfg->dns_count < MAX_DNS_SERVERS) {
                    while (*dns == ' ' || *dns == '\t') dns++;
                    char *end = dns + strlen(dns) - 1;
                    while (end > dns && (*end == ' ' || *end == '\t')) *end-- = '\0';
                    
                    strncpy(tun_cfg->dns_servers[tun_cfg->dns_count], dns, 63);
                    tun_cfg->dns_servers[tun_cfg->dns_count][63] = '\0';
                    tun_cfg->dns_count++;
                    
                    dns = strtok(NULL, ",");
                }
                free(dns_copy);
            }
        }
        /* Parse MTU */
        else if (strncasecmp(trimmed, "MTU", 3) == 0) {
            char *value = strchr(trimmed, '=');
            if (value) {
                value++;
                while (*value == ' ' || *value == '\t') value++;
                tun_cfg->mtu = atoi(value);
            }
        }
    }
    
    fclose(fp);
}

static void extract_routes_from_device(struct wgdevice *device, struct tunnel_config *tun_cfg) {
    struct wgpeer *peer;
    struct wgallowedip *allowedip;
    
    for_each_wgpeer(device, peer) {
        for_each_wgallowedip(peer, allowedip) {
            if (tun_cfg->route_count >= MAX_ROUTES)
                break;
            
            char ip[INET6_ADDRSTRLEN];
            if (allowedip->family == AF_INET) {
                inet_ntop(AF_INET, &allowedip->ip4, ip, sizeof(ip));
            } else {
                inet_ntop(AF_INET6, &allowedip->ip6, ip, sizeof(ip));
            }
            
            snprintf(tun_cfg->routes[tun_cfg->route_count], 63, "%s/%u", ip, allowedip->cidr);
            tun_cfg->route_count++;
        }
    }
}

static void cleanup_interface(const char *interface_name) {
    char cmd[512];
    
    printf("\n[#] Cleaning up interface: %s\n", interface_name);
    
    /* Remove DNS configuration */
    snprintf(cmd, sizeof(cmd), "resolvconf -d %s 2>/dev/null || true", interface_name);
    system(cmd);
    
    /* Bring down and delete interface */
    snprintf(cmd, sizeof(cmd), "ip link set dev %s down 2>/dev/null || true", interface_name);
    system(cmd);
    
    snprintf(cmd, sizeof(cmd), "ip link del dev %s 2>/dev/null || true", interface_name);
    system(cmd);
    
    printf("[#] Tunnel stopped\n");
}

static struct wgdevice *read_config_file(const char *config_file) {
    FILE *fp;
    char line[MAX_LINE_LENGTH];
    struct config_ctx ctx = {0};
    struct wgdevice *device = NULL;
    
    fp = fopen(config_file, "r");
    if (!fp) {
        fprintf(stderr, "Error: Cannot open config file '%s': %s\n", 
                config_file, strerror(errno));
        return NULL;
    }
    
    if (!config_read_init(&ctx, false)) {
        fprintf(stderr, "Error: Failed to initialize config reader\n");
        fclose(fp);
        return NULL;
    }
    
    while (fgets(line, sizeof(line), fp)) {
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n')
            line[len - 1] = '\0';
        
        if (!config_read_line(&ctx, line)) {
            fprintf(stderr, "Error: Failed to parse line: %s\n", line);
            fclose(fp);
            free_wgdevice(ctx.device);
            return NULL;
        }
    }
    
    fclose(fp);
    
    device = config_read_finish(&ctx);
    if (!device) {
        fprintf(stderr, "Error: Failed to finalize configuration\n");
        return NULL;
    }
    
    return device;
}

int main(int argc, char *argv[]) {
    struct wgdevice *device = NULL;
    struct tunnel_config tun_cfg;
    char cmd[512];
    int ret = 1;
    int no_bypass = 0;
    
    if (argc < 2 || argc > 4) {
        print_usage(argv[0]);
        return 1;
    }
    
    const char *config_file = argv[1];
    
    /* Parse interface name and flags */
    int arg_idx = 2;
    if (argc >= 3) {
        if (strcmp(argv[2], "-no-bypass") == 0) {
            no_bypass = 1;
            strncpy(global_interface, "wg0", IFNAMSIZ - 1);
        } else {
            strncpy(global_interface, argv[2], IFNAMSIZ - 1);
            global_interface[IFNAMSIZ - 1] = '\0';
            arg_idx = 3;
        }
    } else {
        strncpy(global_interface, "wg0", IFNAMSIZ - 1);
    }
    
    /* Check for -no-bypass flag */
    if (argc == 4 && strcmp(argv[3], "-no-bypass") == 0) {
        no_bypass = 1;
    }
    
    /* Setup signal handlers for clean shutdown */
    setup_signal_handlers();
    
    printf("[#] Starting WireGuard tunnel: %s\n", global_interface);
    printf("[#] Reading configuration from: %s\n", config_file);
    
    /* Parse wg-quick specific configuration */
    parse_additional_config(config_file, &tun_cfg);
    
    /* Read WireGuard device configuration */
    device = read_config_file(config_file);
    if (!device) {
        fprintf(stderr, "Error: Failed to read configuration file\n");
        return 1;
    }
    
    strncpy(device->name, global_interface, IFNAMSIZ - 1);
    device->name[IFNAMSIZ - 1] = '\0';
    
    /* Extract routes from AllowedIPs */
    extract_routes_from_device(device, &tun_cfg);
    
    /* Create the WireGuard interface */
    printf("[#] ip link add dev %s type wireguard\n", global_interface);
    snprintf(cmd, sizeof(cmd), "ip link add dev %s type wireguard 2>/dev/null", global_interface);
    if (system(cmd) != 0) {
        fprintf(stderr, "Error: Failed to create interface\n");
        goto cleanup;
    }
    
    /* Configure WireGuard */
    printf("[#] Applying WireGuard configuration\n");
    if (ipc_set_device(device) < 0) {
        fprintf(stderr, "Error: Failed to configure WireGuard interface: %s\n", strerror(errno));
        goto cleanup;
    }
    
    /* Assign IP addresses */
    for (int i = 0; i < tun_cfg.address_count; i++) {
        printf("[#] ip address add %s dev %s\n", tun_cfg.addresses[i], global_interface);
        snprintf(cmd, sizeof(cmd), "ip address add %s dev %s", tun_cfg.addresses[i], global_interface);
        if (system(cmd) != 0) {
            fprintf(stderr, "Warning: Failed to add address %s\n", tun_cfg.addresses[i]);
        }
    }
    
    /* Set MTU if specified, otherwise auto-calculate */
    if (tun_cfg.mtu > 0) {
        printf("[#] ip link set mtu %d dev %s\n", tun_cfg.mtu, global_interface);
        snprintf(cmd, sizeof(cmd), "ip link set mtu %d dev %s", tun_cfg.mtu, global_interface);
        system(cmd);
    }
    
    /* Bring interface up */
    printf("[#] ip link set up dev %s\n", global_interface);
    snprintf(cmd, sizeof(cmd), "ip link set up dev %s", global_interface);
    if (system(cmd) != 0) {
        fprintf(stderr, "Error: Failed to bring up interface\n");
        goto cleanup;
    }
    
    /* Add routes for AllowedIPs (skip if -no-bypass flag is set) */
    if (!no_bypass) {
        for (int i = 0; i < tun_cfg.route_count; i++) {
            printf("[#] ip route add %s dev %s\n", tun_cfg.routes[i], global_interface);
            snprintf(cmd, sizeof(cmd), "ip route add %s dev %s 2>/dev/null || true", tun_cfg.routes[i], global_interface);
            system(cmd);
        }
    } else {
        printf("[#] Skipping routing rules (no-bypass mode)\n");
    }
    
    /* Configure DNS if specified */
    if (tun_cfg.dns_count > 0) {
        char resolvconf_cmd[256];
        snprintf(resolvconf_cmd, sizeof(resolvconf_cmd), "resolvconf -a %s -m 0 -x", global_interface);
        FILE *resolvconf = popen(resolvconf_cmd, "w");
        if (resolvconf) {
            for (int i = 0; i < tun_cfg.dns_count; i++) {
                fprintf(resolvconf, "nameserver %s\n", tun_cfg.dns_servers[i]);
            }
            pclose(resolvconf);
            printf("[#] DNS configured: %d server(s)\n", tun_cfg.dns_count);
        }
    }
    
    printf("\n[#] WireGuard tunnel '%s' is now active\n", global_interface);
    if (no_bypass) {
        printf("[#] Running in no-bypass mode (no routing rules added)\n");
    }
    printf("[#] Press Ctrl+C to stop the tunnel\n\n");
    
    /* Keep running until signal */
    while (!should_exit) {
        sleep(1);
    }
    
    ret = 0;
    
cleanup:
    cleanup_interface(global_interface);
    
    if (device) {
        free_wgdevice(device);
        device = NULL;
    }
    
    return ret;
}
