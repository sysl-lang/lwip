/*
 * lwIP's port header for sysl, with no operating system under it (NO_SYS = 1).
 *
 * lwIP asks its port for four things here: how to print a diagnostic, what to do when one of its
 * assertions fails, how to make a random number, and anything about the compiler that its own
 * defaults get wrong. clang gets nothing wrong, so only the first two are said.
 *
 * - A diagnostic is dropped. lwIP's default prints with `printf`, which a board without a console
 *   has nowhere to send, and which would drag `<stdio.h>` into a build that has no C library.
 * - A failed assertion stops the program through `lwip_sysl_assert`, which the sysl side of the
 *   package defines: lwIP asserts on misuse of its API (a pcb used after it was freed, a pbuf
 *   chain whose lengths disagree), and carrying on past one corrupts its pools.
 *
 * `LWIP_RAND` is left undefined, so lwIP picks its first local port and DHCP transaction ID the
 * same way every run. A consumer that wants them randomized defines it in its own copy of this
 * file, beside its own `lwipopts.h`.
 */
#ifndef LWIP_ARCH_CC_H
#define LWIP_ARCH_CC_H

void lwip_sysl_assert(const char *message, const char *file, int line);

#define LWIP_PLATFORM_DIAG(x)   do { } while (0)
#define LWIP_PLATFORM_ASSERT(x) lwip_sysl_assert((x), __FILE__, __LINE__)

#endif
