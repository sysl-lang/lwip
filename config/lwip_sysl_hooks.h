/*
 * The hooks the package's `lwipopts.h` installs. lwIP includes this file wherever a hook can be
 * called, by the name `LWIP_HOOK_FILENAME` gives it.
 */
#ifndef LWIP_SYSL_HOOKS_H
#define LWIP_SYSL_HOOKS_H

#include "lwip/ip4_addr.h"

struct netif;

/* The interface that holds `src` as its address and is up, or NULL to leave the choice to lwIP's
 * ordinary routing. Defined in the package's C, beside its sysl. */
struct netif *lwip_sysl_route_src(const ip4_addr_t *src, const ip4_addr_t *dest);

#endif
