/*
 * The configuration lwIP is compiled with, unless a consumer supplies its own.
 *
 * A consumer that wants other pool sizes, another window or more protocols writes its own
 * `lwipopts.h` in a directory of its project and names that directory on the dependency:
 *
 *     lwip { git = "github.com/sysl-lang/lwip", version = "...", c_include = ["config"] }
 *
 * Its file then stands in for this one everywhere lwIP is compiled, in the package's sysl too, so
 * every size the binding measures is the one the objects were built with. `arch/cc.h` still comes
 * from the package. The simplest way to write one is to copy this file and change what differs:
 * the first block below is what the binding relies on and should be kept as it is.
 */
#ifndef LWIPOPTS_H
#define LWIPOPTS_H

/* ---- What the binding relies on ---------------------------------------------------------- */

/* No operating system: lwIP is driven from one thread by calls into it, the raw (callback) API,
 * and `sys_check_timeouts`. Nothing preempts it, so it needs no locking. */
#define NO_SYS                      1
#define SYS_LIGHTWEIGHT_PROT        0
#define LWIP_SOCKET                 0
#define LWIP_NETCONN                0

/* Every byte lwIP uses comes from its own pools and its own heap, both static arrays inside the
 * library. Nothing is taken from the program's allocator. */
#define MEM_LIBC_MALLOC             0
#define MEMP_MEM_MALLOC             0

/* A pointer's alignment, so a pool element holding one is aligned on a 64-bit host too. */
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 8
#define MEM_ALIGNMENT               8
#else
#define MEM_ALIGNMENT               4
#endif

/* IPv4 over Ethernet (or Wi-Fi, which hands lwIP Ethernet frames), with ARP. */
#define LWIP_IPV4                   1
#define LWIP_IPV6                   0
#define LWIP_ARP                    1
#define LWIP_ETHERNET               1

/* The counters the binding reads, among them TCP's retransmissions. */
#define LWIP_STATS                  1
#define LWIP_STATS_LARGE            1
#define MIB2_STATS                  1

/* A packet whose source address is set leaves by the interface holding that address, rather than
 * by the first whose subnet holds the destination. With one interface this changes nothing; with
 * two on one subnet (a Wi-Fi station and a soft access point, say) it is what keeps each reply on
 * the link its request came in on. */
#define LWIP_HOOK_FILENAME          "lwip_sysl_hooks.h"
#define LWIP_HOOK_IP4_ROUTE_SRC(src, dest) lwip_sysl_route_src((src), (dest))

/* ---- Protocols --------------------------------------------------------------------------- */

#define LWIP_ICMP                   1
#define LWIP_RAW                    1
#define LWIP_UDP                    1
#define LWIP_TCP                    1
#define LWIP_DHCP                   1

/* Checksums are generated and checked in software. A MAC that computes them in hardware turns
 * the generation off in its own copy of this file. */
#define CHECKSUM_GEN_IP             1
#define CHECKSUM_GEN_UDP            1
#define CHECKSUM_GEN_TCP            1
#define CHECKSUM_GEN_ICMP           1
#define CHECKSUM_CHECK_IP           1
#define CHECKSUM_CHECK_UDP          1
#define CHECKSUM_CHECK_TCP          1
#define CHECKSUM_CHECK_ICMP         1

/* ---- Sizes ------------------------------------------------------------------------------- */

/* Each size is only a default, so a consumer that wants to change one or two need not copy this
 * file: its own `lwipopts.h` defines those and then includes this one with clang's
 * `#include_next "lwipopts.h"`, which carries on the search past the consumer's directory. */

/* lwIP's heap, for outgoing data a write copies and for the DHCP client's state. */
#ifndef MEM_SIZE
#define MEM_SIZE                    (16 * 1024)
#endif

/* Received frames land in pool buffers, one frame per buffer. */
#ifndef PBUF_POOL_SIZE
#define PBUF_POOL_SIZE              24
#endif
#ifndef PBUF_POOL_BUFSIZE
#define PBUF_POOL_BUFSIZE           1536
#endif

#ifndef MEMP_NUM_PBUF
#define MEMP_NUM_PBUF               16
#endif
#ifndef MEMP_NUM_RAW_PCB
#define MEMP_NUM_RAW_PCB            4
#endif
#ifndef MEMP_NUM_UDP_PCB
#define MEMP_NUM_UDP_PCB            6
#endif
#ifndef MEMP_NUM_TCP_PCB
#define MEMP_NUM_TCP_PCB            8
#endif
#ifndef MEMP_NUM_TCP_PCB_LISTEN
#define MEMP_NUM_TCP_PCB_LISTEN     4
#endif
#ifndef MEMP_NUM_TCP_SEG
#define MEMP_NUM_TCP_SEG            32
#endif
#ifndef ARP_TABLE_SIZE
#define ARP_TABLE_SIZE              8
#endif

#ifndef TCP_MSS
#define TCP_MSS                     1460
#endif
#ifndef TCP_WND
#define TCP_WND                     (4 * TCP_MSS)
#endif
#ifndef TCP_SND_BUF
#define TCP_SND_BUF                 (4 * TCP_MSS)
#endif
#ifndef TCP_SND_QUEUELEN
#define TCP_SND_QUEUELEN            ((4 * TCP_SND_BUF) / TCP_MSS)
#endif

#endif
