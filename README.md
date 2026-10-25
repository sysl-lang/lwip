# lwIP for sysl

The TCP/IP stack most microcontroller networking runs on, bound to
[lwIP](https://savannah.nongnu.org/projects/lwip/) 2.2.1, vendored unchanged, and driven with no
operating system under it: the raw (callback) API, for a board's Ethernet MAC or Wi-Fi chip (the
Pico 2 W's CYW43439 hands its host Ethernet frames, and its SDK networks through lwIP).

*An independent sysl package for lwIP. It is not affiliated with or endorsed by lwIP's authors or trademark holders.*

- **Nothing comes from the program's heap.** Every pcb, packet buffer and queued segment comes from
  lwIP's own pools and its own heap, static arrays sized by `lwipopts.h` (`MEM_LIBC_MALLOC 0`). The
  binding's own files are `@no_alloc`, so the compiler holds them to it.
- **One thread calls in, lwIP calls back.** A program hands lwIP the frames its device received and
  runs lwIP's timers from its main loop; lwIP answers through the program's driver and through traits
  the program implements. Between those calls nothing runs, and nothing needs a lock.
- **lwIP's tree is upstream's, byte for byte.** What the package decides about it — which files are
  compiled, and the configuration lwIP takes from its integrator — is in `sysl.hocon`'s `c` block
  and in `config/`, and a consumer can replace the configuration with its own.

```hocon
dependencies {
  lwip { git = "github.com/sysl-lang/lwip", version = "0.1.1" }
}
```

The module is `sh.sysl.lwip`. It needs sysl 0.1.2 or later, which has the vendored-C
manifest block (`c { … }`, `c_include`) and target-only dependencies (`if = "!libc"`).

## Using it

A board gives lwIP a clock and an interface over its driver, then loops: frames in, timers run. This
is `examples/board.sysl`, which `sysl build-c examples/board.sysl -o board.a` compiles into an archive
for the board's C to link.

```sysl
import sh.sysl.lwip.{init, check_timeouts, interface, ip4, tcp, Clock, Connection, Driver, Error,
                     Interface, Listener, Packet, Tcp, Verdict}

struct Ticks
    ms: u32

impl Clock for Ticks
    now_ms(*self) -> u32 = self.ms

// The board's Ethernet MAC: `send` copies a frame into a transmit descriptor.
struct Mac
    sent: u32

impl Driver for Mac
    send(*self, frame: []const u8) -> Result[unit, Error]
        self.sent += 1
        Ok(())

// An echo server: whatever arrives goes straight back.
struct Echo
    open: bool

impl Connection for Echo
    received(*self, tcp: Tcp, data: Packet) -> Verdict
        var chunk: [512]u8 = [0; 512]
        var at = 0usize
        while at < data.len()
            val n = data.copy_to(chunk[..], at)
            tcp.write(chunk[0..<n])
            at += n
        Verdict.Continue

    failed(*self, error: Error)
        self.open = false

struct Server
    conn: Echo

impl Listener for Server
    accept(*self, tcp: Tcp) -> Verdict
        self.conn.open = true
        tcp.attach(&self.conn)
        Verdict.Continue

@export("net_run")
run(read_tick: *extern() -> u32, next_frame: *extern(*u8) -> usize) -> int
    var ticks = Ticks(0)
    var mac = Mac(0)
    var eth: Interface[Mac] = interface([0x02, 0, 0, 0, 0, 0x01])
    var server = Server(Echo(false))
    init(&ticks).unwrap()
    eth.add(&mac, ip4(192, 168, 1, 50), ip4(255, 255, 255, 0), ip4(192, 168, 1, 1)).unwrap()
    eth.set_default()
    val listener = tcp().unwrap()
    listener.bind(ip4(0, 0, 0, 0), 7).unwrap()
    listener.listen(&server).unwrap()
    var frame: [1514]u8 = [0; 1514]
    loop
        val n = next_frame(&frame[0])
        if n > 0 then eth.input(frame[0..<n])
        ticks.ms = read_tick()
        check_timeouts()
```

- **`Interface`** is lwIP's `struct netif` over a `Driver` (`send(frame)`, one Ethernet frame with no
  preamble or FCS). `input(frame)` hands it a received frame, `add`/`remove`, `up`/`down`,
  `link_up`/`link_down`, `set_default`, `set_address`, `resolve` and `arp_entry` (ARP),
  `start_dhcp`/`stop_dhcp`/`dhcp_bound`.
- **`Tcp`**: `tcp()`, `bind`, `listen(listener)`, `connect(address, port, handler)`, `attach`,
  `write` (answers how much the send buffer took), `output`, `send_buffer`, `mss`, `set_nagle`,
  `state`, `local`, `remote`, `close`, `shutdown`, `abort`. A **`Connection`** hears `connected`,
  `received`, `peer_closed`, `sent`, `poll` and `failed`; a **`Listener`** hears `accept`. A callback
  answers a `Verdict` — `Continue`, or `Abort` to reset the connection.
- **`Udp`**: `udp()`, `bind`, `connect`, `send_to`, `send`, `receive(handler)`, `remove`, with a
  **`Datagrams`** handler. **`Raw`**: `raw(protocol)`, `bind`, `send_to`, `receive`, `remove`, with a
  **`Packets`** handler — how a program sends an ICMP echo request, which lwIP has no API for.
- **`Packet`** is a received chain of lwIP buffers, valid during its callback: `len`, `copy_to`,
  `at`, and `buffers`/`buffer(i)` to read it in place.
- **`init(clock)`** once, **`check_timeouts()`** from the main loop, **`next_timeout_ms()`** for how
  long the board may sleep. **`counters(protocol)`**, **`tcp_retransmits()`** and
  **`memory_errors()`** read lwIP's statistics.

### What must stay where it is

lwIP keeps addresses, and a copy of what it points at is a copy lwIP knows nothing about:

- an `Interface` and the driver given to `add`, from `add` until `remove`;
- the clock given to `init`, for the life of the program;
- a `Connection`, `Listener`, `Datagrams` or `Packets` handler while it is attached — until `close`
  (which detaches it), `remove`, or its `failed`.

`Tcp`, `Udp` and `Raw` are handles — one word naming a pcb in lwIP's pool — and copy freely; lwIP frees
the pcb, so a handle kept after `close`, `remove`, `abort` or `failed` names nothing.

### Your own `lwipopts.h`

lwIP takes its options from `lwipopts.h`. The package's (`config/lwipopts.h`) is NO_SYS, IPv4 over
Ethernet with ARP, ICMP, raw, UDP, TCP and DHCP, software checksums, 16 KiB of heap, 24 pool buffers,
8 TCP pcbs and a 4-segment window. A consumer names a directory of its own on the dependency, and the
`lwipopts.h` there stands in for the package's everywhere lwIP is compiled — lwIP's objects, the
binding's C and every size the binding measures:

```hocon
dependencies {
  lwip { git = "github.com/sysl-lang/lwip", version = "0.1.1", c_include = ["config"] }
}
```

Every size in the package's file is a default (`#ifndef`), so changing one or two needs no copy:

```c
#ifndef MY_LWIPOPTS_H
#define MY_LWIPOPTS_H
#define MEMP_NUM_TCP_PCB 13
#define TCP_SND_BUF      (2 * 1460)
#include_next "lwipopts.h"
#endif
```

`#include_next` carries on the search past the consumer's directory into the package's. The first
block of the package's file (NO_SYS, the pools, IPv4, the statistics, and the route-by-source hook) is
what the binding relies on. `tests/override` is such a project, and its tests prove both halves.

## What is bound, and what is not

Bound: lwIP's IPv4 core and its Ethernet layer — interfaces, ARP, IPv4, ICMP (answering echo
requests), raw IP, UDP, TCP, the DHCP client, packet buffers, timers and statistics — through the raw
API with `NO_SYS = 1`.

Carried and not compiled, each for a reason:

- **The sequential (`netconn`) and socket APIs**, which need an operating system's threads and
  mailboxes. On a board the raw API is what is used.
- **IPv6**, 6LoWPAN, PPP, SLIP and the bridge: not what a board's Ethernet or Wi-Fi needs first, and
  each is a `sources` line and an `lwipopts.h` switch away when it is.
- **`apps/`** (HTTP server, MQTT, SNTP, mDNS and the rest): applications over the API, each its own
  binding when wanted.
- **`altcp`** (the TLS-capable TCP abstraction) and **DNS**: compiled out by the default options.
- **IPv4 fragmentation and reassembly** are on (lwIP's default); **IGMP** and **AutoIP** are off.

## Tests

`sysl test .` runs 15; `sysl test tests/override` runs 2 more.

**Two endpoints of one lwIP, joined by a wire made of memory.** lwIP's state is global — one set of
pools, pcb lists, timers and counters per program — so two lwIP stacks cannot share a process. One
lwIP holds any number of interfaces, though, and two of them, `a` at 10.0.0.1 and `b` at 10.0.0.2,
joined by a wire, are two endpoints sharing nothing but code: their ARP entries are keyed by interface,
their pcbs are bound to their own addresses, and every byte between them is an Ethernet frame lwIP
built on one side and parsed on the other. lwIP would otherwise send everything out of the first
interface on the subnet, and the package's `lwipopts.h` routes by source address
(`LWIP_HOOK_IP4_ROUTE_SRC`), so each end's packets leave by its own interface. A hand-written peer
would have been a second TCP the tests trusted; two processes over a pipe, the same lwIP with a
scheduler between. The wire queues and never delivers from inside a send, since lwIP is not
reentrant, and the tests step it: deliver everything queued, then move the clock 10 ms and run the
timers.

They cover ARP (each end learns the other's address), an ICMP echo answered with id, sequence and
payload intact, UDP both ways with the sender's address and port, a TCP connect with each end on its
own address, **300 KiB each way at once checked byte for byte** followed by a close, a connection
refused with a reset, a DHCP DISCOVER broadcast, the TCP pool's bound, and the binding's refusals.
**Loss**: with every seventh frame dropped (86 of 607), 200 KiB still cross each way intact; lwIP's
MIB-II counter shows 12 fast retransmits, and its timeout retransmissions recover the rest.

**The oracle is tcpdump.** The wire writes every frame it delivers to a pcap capture, and the host's
`/usr/sbin/tcpdump -r <file> -vv -nn` decodes it. tcpdump knows nothing of lwIP; at `-vv` it checks
every IP, TCP and UDP checksum and says "incorrect" or "bad" of one that is wrong. Over the whole
exchange — ARP, ping, UDP and the 300 KiB transfer, 751 frames — it reads every frame, says neither
word of any, marks every one of the 745 TCP segments' checksums `(correct)` and both datagrams
`[udp sum ok]`, and its text shows the handshake (`[S]`, `[S.]`, then the client acknowledging the
SYN) and the close as RFC 793 draws it: the client's `[F.]` after all 300 KiB, the server's `[F.]`
after that, and the client's last ACK. The lossy run is read the same way.

**`c_include`.** `tests/override` depends on the package with its own `lwipopts.h` (13 TCP pcbs, a
2920-byte send buffer). Its tests see 13 in the binding's `c const` probe and 13 pcbs in lwIP's pool
at run time, with 2920 bytes of send buffer each; without `c_include` both go red (8 and 5840).

**Mutations**, each of which turned at least one test red: the driver dropping a frame's last byte
(9 tests); the clock not advancing (both transfers and the oracle); the connection handler's `arg`
never set, so lwIP hands callbacks the listener's or none (6); TCP checksum generation off (6), and
with lwIP's own checking off too, so only tcpdump can tell (the oracle: `cksum 0x0000 (incorrect ->
0x788d)`); IP header checksum generation and checking off (the oracle: `bad cksum 0`); the interface
given the wrong hardware address (the ARP test and the oracle); an address's octets reversed (the
address test, and the oracle); the route-by-source hook removed (8).

**AddressSanitizer**: `SYSL_EXTRA_CFLAGS="-fsanitize=address -g" sysl test .` passes 18/18; every
clang command (lwIP's 32 files, the shim, the sysl code units) and the link carried the flag, and the
test binary imports 36 `__asan_*` symbols.

**Bare metal.** The package builds for boards with `sysl-lang/cstd` as its C library: the dependency is
`if = "!libc"`, so on a target with no libc lwIP's C compiles against cstd's headers and the archive
carries cstd's `memcpy`, `memset`, `strlen` and the rest, and on the host the real libc answers. The
suite builds `examples/board.sysl` with `build-c` for `thumbv7em-freestanding`, `thumbv6m-freestanding`
and `riscv32-freestanding`, and checks with `nm` that the archive defines lwIP's entry points and
cstd's definitions of what lwIP calls. It needs sysl 0.1.2 or later. A board keeps lwIP's state in
module storage, so `interface` is a `const` function and a `var eth: Interface[Mac] = interface(…)` is
constant data, which a target with no loader requires.

## Vendored version

lwIP 2.2.1, from `lwip-2.2.1.zip` (SHA-256
`7b622662dba2383d71f874f2e494b54ae948531559c17acbe75797966d646878`), whose `src/` is `c/lwip/src`
unchanged (`diff -r` empty). An upgrade is a copy of the new release's `src/` over it, and the diff
of that commit shows every file the release added.

## License

lwIP is BSD-3-Clause; its terms are carried verbatim in `LICENSE`, and in each of its files. Everything
else here is ISC.
