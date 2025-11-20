/*
 *  OpenVPN -- An application to securely tunnel IP networks
 *             over a single UDP port, with support for SSL/TLS-based
 *             session authentication and key exchange,
 *             packet encryption, packet authentication, and
 *             packet compression.
 *
 *  Tunnel Entry Points - Exported wrapper functions for external access
 *  
 *  This header exposes non-static entry points to the OpenVPN tunnel functions
 *  allowing external code (like C bridges or wrappers) to drive the event loop.
 */

#ifndef TUNNEL_ENTRY_H
#define TUNNEL_ENTRY_H

#include "openvpn.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Run a point-to-point VPN tunnel
 * This is a non-static wrapper around the internal tunnel_point_to_point function
 * 
 * @param c  OpenVPN context structure
 */
void openvpn_run_point_to_point(struct context *c);

/**
 * Run a VPN server tunnel
 * This is a non-static wrapper around the internal tunnel_server function
 * 
 * @param c  OpenVPN context structure
 */
void openvpn_run_server(struct context *c);

#ifdef __cplusplus
}
#endif

#endif /* TUNNEL_ENTRY_H */
