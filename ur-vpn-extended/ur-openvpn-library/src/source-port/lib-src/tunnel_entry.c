/*
 *  OpenVPN -- An application to securely tunnel IP networks
 *             over a single UDP port, with support for SSL/TLS-based
 *             session authentication and key exchange,
 *             packet encryption, packet authentication, and
 *             packet compression.
 *
 *  Tunnel Entry Points - Exported wrapper functions for external access
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "syshead.h"
#include "tunnel_entry.h"
#include "openvpn.h"
#include "multi.h"

/* Forward declarations of tunnel functions (made non-static in openvpn.c) */
void tunnel_point_to_point(struct context *c);

void
openvpn_run_point_to_point(struct context *c)
{
    tunnel_point_to_point(c);
}

void
openvpn_run_server(struct context *c)
{
    tunnel_server(c);
}
