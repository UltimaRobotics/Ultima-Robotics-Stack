#include "../include/routing.h"
#include "../include/routing_api.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <net/if.h>
#include <arpa/inet.h>

#define NETLINK_BUFFER_SIZE 8192

typedef struct {
    int fd;
    unsigned int if_index;
} netlink_ctx_t;

static int netlink_open_socket(void) {
    int fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
    if (fd < 0) {
        return -1;
    }
    
    struct sockaddr_nl sa;
    memset(&sa, 0, sizeof(sa));
    sa.nl_family = AF_NETLINK;
    sa.nl_pid = getpid();
    
    if (bind(fd, (struct sockaddr*)&sa, sizeof(sa)) < 0) {
        close(fd);
        return -1;
    }
    
    return fd;
}

static int netlink_send_request(int fd, struct nlmsghdr *nlh) {
    struct sockaddr_nl sa;
    memset(&sa, 0, sizeof(sa));
    sa.nl_family = AF_NETLINK;
    
    struct iovec iov = { nlh, nlh->nlmsg_len };
    struct msghdr msg = { &sa, sizeof(sa), &iov, 1, NULL, 0, 0 };
    
    return sendmsg(fd, &msg, 0);
}

static int netlink_receive_ack(int fd) {
    char buf[NETLINK_BUFFER_SIZE];
    struct iovec iov = { buf, sizeof(buf) };
    struct sockaddr_nl sa;
    struct msghdr msg = { &sa, sizeof(sa), &iov, 1, NULL, 0, 0 };
    
    int len = recvmsg(fd, &msg, 0);
    if (len < 0) {
        return -1;
    }
    
    struct nlmsghdr *nlh = (struct nlmsghdr*)buf;
    
    if (nlh->nlmsg_type == NLMSG_ERROR) {
        struct nlmsgerr *err = (struct nlmsgerr*)NLMSG_DATA(nlh);
        return err->error;
    }
    
    return 0;
}

int netlink_add_route(const char *interface_name,
                     const char *dest_addr,
                     uint8_t prefix_len,
                     const char *gateway,
                     uint32_t metric,
                     bool is_ipv6) {
    int fd = netlink_open_socket();
    if (fd < 0) {
        return -1;
    }
    
    unsigned int if_index = if_nametoindex(interface_name);
    if (if_index == 0) {
        close(fd);
        return -1;
    }
    
    char buf[NETLINK_BUFFER_SIZE];
    memset(buf, 0, sizeof(buf));
    
    struct nlmsghdr *nlh = (struct nlmsghdr*)buf;
    struct rtmsg *rtm = (struct rtmsg*)NLMSG_DATA(nlh);
    
    nlh->nlmsg_len = NLMSG_LENGTH(sizeof(struct rtmsg));
    nlh->nlmsg_type = RTM_NEWROUTE;
    nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_CREATE | NLM_F_EXCL | NLM_F_ACK;
    nlh->nlmsg_seq = 1;
    nlh->nlmsg_pid = getpid();
    
    rtm->rtm_family = is_ipv6 ? AF_INET6 : AF_INET;
    rtm->rtm_dst_len = prefix_len;
    rtm->rtm_src_len = 0;
    rtm->rtm_tos = 0;
    rtm->rtm_table = RT_TABLE_MAIN;
    rtm->rtm_protocol = RTPROT_BOOT;
    rtm->rtm_scope = RT_SCOPE_UNIVERSE;
    rtm->rtm_type = RTN_UNICAST;
    rtm->rtm_flags = 0;
    
    struct rtattr *rta = (struct rtattr*)((char*)nlh + NLMSG_ALIGN(nlh->nlmsg_len));
    
    if (is_ipv6) {
        struct in6_addr addr;
        inet_pton(AF_INET6, dest_addr, &addr);
        
        rta->rta_type = RTA_DST;
        rta->rta_len = RTA_LENGTH(sizeof(struct in6_addr));
        memcpy(RTA_DATA(rta), &addr, sizeof(struct in6_addr));
        nlh->nlmsg_len = NLMSG_ALIGN(nlh->nlmsg_len) + RTA_LENGTH(sizeof(struct in6_addr));
    } else {
        struct in_addr addr;
        inet_pton(AF_INET, dest_addr, &addr);
        
        rta->rta_type = RTA_DST;
        rta->rta_len = RTA_LENGTH(sizeof(struct in_addr));
        memcpy(RTA_DATA(rta), &addr, sizeof(struct in_addr));
        nlh->nlmsg_len = NLMSG_ALIGN(nlh->nlmsg_len) + RTA_LENGTH(sizeof(struct in_addr));
    }
    
    rta = (struct rtattr*)((char*)nlh + NLMSG_ALIGN(nlh->nlmsg_len));
    rta->rta_type = RTA_OIF;
    rta->rta_len = RTA_LENGTH(sizeof(int));
    memcpy(RTA_DATA(rta), &if_index, sizeof(int));
    nlh->nlmsg_len = NLMSG_ALIGN(nlh->nlmsg_len) + RTA_LENGTH(sizeof(int));
    
    if (gateway) {
        rta = (struct rtattr*)((char*)nlh + NLMSG_ALIGN(nlh->nlmsg_len));
        
        if (is_ipv6) {
            struct in6_addr gw;
            inet_pton(AF_INET6, gateway, &gw);
            
            rta->rta_type = RTA_GATEWAY;
            rta->rta_len = RTA_LENGTH(sizeof(struct in6_addr));
            memcpy(RTA_DATA(rta), &gw, sizeof(struct in6_addr));
            nlh->nlmsg_len = NLMSG_ALIGN(nlh->nlmsg_len) + RTA_LENGTH(sizeof(struct in6_addr));
        } else {
            struct in_addr gw;
            inet_pton(AF_INET, gateway, &gw);
            
            rta->rta_type = RTA_GATEWAY;
            rta->rta_len = RTA_LENGTH(sizeof(struct in_addr));
            memcpy(RTA_DATA(rta), &gw, sizeof(struct in_addr));
            nlh->nlmsg_len = NLMSG_ALIGN(nlh->nlmsg_len) + RTA_LENGTH(sizeof(struct in_addr));
        }
    }
    
    if (metric > 0) {
        rta = (struct rtattr*)((char*)nlh + NLMSG_ALIGN(nlh->nlmsg_len));
        rta->rta_type = RTA_PRIORITY;
        rta->rta_len = RTA_LENGTH(sizeof(uint32_t));
        memcpy(RTA_DATA(rta), &metric, sizeof(uint32_t));
        nlh->nlmsg_len = NLMSG_ALIGN(nlh->nlmsg_len) + RTA_LENGTH(sizeof(uint32_t));
    }
    
    int result = -1;
    if (netlink_send_request(fd, nlh) > 0) {
        result = netlink_receive_ack(fd);
    }
    
    close(fd);
    return result;
}

int netlink_del_route(const char *interface_name,
                     const char *dest_addr,
                     uint8_t prefix_len,
                     bool is_ipv6) {
    int fd = netlink_open_socket();
    if (fd < 0) {
        return -1;
    }
    
    unsigned int if_index = if_nametoindex(interface_name);
    if (if_index == 0) {
        close(fd);
        return -1;
    }
    
    char buf[NETLINK_BUFFER_SIZE];
    memset(buf, 0, sizeof(buf));
    
    struct nlmsghdr *nlh = (struct nlmsghdr*)buf;
    struct rtmsg *rtm = (struct rtmsg*)NLMSG_DATA(nlh);
    
    nlh->nlmsg_len = NLMSG_LENGTH(sizeof(struct rtmsg));
    nlh->nlmsg_type = RTM_DELROUTE;
    nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK;
    nlh->nlmsg_seq = 1;
    nlh->nlmsg_pid = getpid();
    
    rtm->rtm_family = is_ipv6 ? AF_INET6 : AF_INET;
    rtm->rtm_dst_len = prefix_len;
    rtm->rtm_table = RT_TABLE_MAIN;
    
    struct rtattr *rta = (struct rtattr*)((char*)nlh + NLMSG_ALIGN(nlh->nlmsg_len));
    
    if (is_ipv6) {
        struct in6_addr addr;
        inet_pton(AF_INET6, dest_addr, &addr);
        
        rta->rta_type = RTA_DST;
        rta->rta_len = RTA_LENGTH(sizeof(struct in6_addr));
        memcpy(RTA_DATA(rta), &addr, sizeof(struct in6_addr));
        nlh->nlmsg_len = NLMSG_ALIGN(nlh->nlmsg_len) + RTA_LENGTH(sizeof(struct in6_addr));
    } else {
        struct in_addr addr;
        inet_pton(AF_INET, dest_addr, &addr);
        
        rta->rta_type = RTA_DST;
        rta->rta_len = RTA_LENGTH(sizeof(struct in_addr));
        memcpy(RTA_DATA(rta), &addr, sizeof(struct in_addr));
        nlh->nlmsg_len = NLMSG_ALIGN(nlh->nlmsg_len) + RTA_LENGTH(sizeof(struct in_addr));
    }
    
    int result = -1;
    if (netlink_send_request(fd, nlh) > 0) {
        result = netlink_receive_ack(fd);
    }
    
    close(fd);
    return result;
}
