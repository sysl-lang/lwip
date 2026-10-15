/*
 * The few things sysl cannot reach in lwIP without C: fields of `struct netif` and `struct tcp_pcb`,
 * whose layouts depend on `lwipopts.h`, macros that read them, and the statistics block. Each
 * function is a field read or write and nothing more, so the binding above it decides everything.
 *
 * This file is compiled with lwIP's include roots and whatever `lwipopts.h` the build uses, so every
 * offset here is the one lwIP's own objects were compiled with.
 */
#include "lwip/opt.h"
#include "lwip/netif.h"
#include "lwip/etharp.h"
#include "lwip/tcp.h"
#include "lwip/stats.h"
#include "lwip/ip4_addr.h"
#include "netif/ethernet.h"

#include <string.h>

/* The route-by-source hook the default `lwipopts.h` installs (`LWIP_HOOK_IP4_ROUTE_SRC`): the
 * interface that is up and holds `src`, or NULL so that lwIP routes by destination as usual. */
struct netif *lwip_sysl_route_src(const ip4_addr_t *src, const ip4_addr_t *dest) {
  struct netif *netif;
  LWIP_UNUSED_ARG(dest);
  if (src == NULL || ip4_addr_isany(src)) {
    return NULL;
  }
  NETIF_FOREACH(netif) {
    if (netif_is_up(netif) && ip4_addr_eq(src, netif_ip4_addr(netif))) {
      return netif;
    }
  }
  return NULL;
}

/* What an Ethernet interface's init function owes lwIP: its hardware address, its MTU, its flags,
 * a two-letter name, and the two output functions. IP goes out through ARP; ARP's frames go out
 * through `linkoutput`, which is the driver. */
void lwip_sysl_netif_setup(struct netif *netif, const u8_t *mac, u16_t mtu,
                           netif_linkoutput_fn linkoutput, char name0, char name1) {
  netif->hwaddr_len = ETH_HWADDR_LEN;
  memcpy(netif->hwaddr, mac, ETH_HWADDR_LEN);
  netif->mtu = mtu;
  netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_ETHERNET;
  netif->name[0] = name0;
  netif->name[1] = name1;
#if LWIP_IPV4 && LWIP_ARP
  netif->output = etharp_output;
#endif
  netif->linkoutput = linkoutput;
}

void *lwip_sysl_netif_state(const struct netif *netif) { return netif->state; }

u32_t lwip_sysl_netif_ip4(const struct netif *netif) { return ip4_addr_get_u32(netif_ip4_addr(netif)); }

const u8_t *lwip_sysl_netif_hwaddr(const struct netif *netif) { return netif->hwaddr; }

u16_t lwip_sysl_tcp_sndbuf(const struct tcp_pcb *pcb) { return tcp_sndbuf(pcb); }

u16_t lwip_sysl_tcp_sndqueuelen(const struct tcp_pcb *pcb) { return tcp_sndqueuelen(pcb); }

u16_t lwip_sysl_tcp_mss(const struct tcp_pcb *pcb) { return tcp_mss(pcb); }

int lwip_sysl_tcp_state(const struct tcp_pcb *pcb) { return (int)pcb->state; }

void lwip_sysl_tcp_nagle(struct tcp_pcb *pcb, int on) {
  if (on) {
    tcp_nagle_enable(pcb);
  } else {
    tcp_nagle_disable(pcb);
  }
}

/* One counter of one protocol's statistics. `proto` is 0 link, 1 ARP, 2 IP, 3 ICMP, 4 UDP, 5 TCP;
 * `field` is 0 sent, 1 received, 2 dropped, 3 checksum errors, 4 length errors, 5 out of memory,
 * 6 no route, 7 protocol errors, 8 other errors. Anything else answers 0. */
u32_t lwip_sysl_stat(int proto, int field) {
#if LWIP_STATS
  const struct stats_proto *s;
  switch (proto) {
#if LINK_STATS
    case 0: s = &lwip_stats.link; break;
#endif
#if ETHARP_STATS
    case 1: s = &lwip_stats.etharp; break;
#endif
#if IP_STATS
    case 2: s = &lwip_stats.ip; break;
#endif
#if ICMP_STATS
    case 3: s = &lwip_stats.icmp; break;
#endif
#if UDP_STATS
    case 4: s = &lwip_stats.udp; break;
#endif
#if TCP_STATS
    case 5: s = &lwip_stats.tcp; break;
#endif
    default: return 0;
  }
  switch (field) {
    case 0: return s->xmit;
    case 1: return s->recv;
    case 2: return s->drop;
    case 3: return s->chkerr;
    case 4: return s->lenerr;
    case 5: return s->memerr;
    case 6: return s->rterr;
    case 7: return s->proterr;
    case 8: return s->err;
    default: return 0;
  }
#else
  LWIP_UNUSED_ARG(proto);
  LWIP_UNUSED_ARG(field);
  return 0;
#endif
}

/* lwIP's MIB-II count of retransmitted TCP segments (`tcpRetransSegs`). lwIP 2.2.1 increments it in
 * `tcp_rexmit`, the fast-retransmit path; a retransmission after a timeout goes through
 * `tcp_rexmit_rto` and is not counted, though it shows in `tcp` sent. */
u32_t lwip_sysl_tcp_fast_retransmits(void) {
#if MIB2_STATS
  return lwip_stats.mib2.tcpretranssegs;
#else
  return 0;
#endif
}

/* Times lwIP's heap or one of its pools had nothing left to give. */
u32_t lwip_sysl_memory_errors(void) {
  u32_t n = 0;
#if LWIP_STATS && MEM_STATS
  n += lwip_stats.mem.err;
#endif
#if LWIP_STATS && MEMP_STATS
  for (int i = 0; i < MEMP_MAX; i++) {
    n += lwip_stats.memp[i]->err;
  }
#endif
  return n;
}
