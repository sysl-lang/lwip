/*
 * This project's lwIP configuration: the package's, with a bigger pool of TCP pcbs and a smaller
 * TCP send buffer. Everything else comes from the package's own `lwipopts.h`, which `#include_next`
 * finds after this directory.
 */
#ifndef OVERRIDE_LWIPOPTS_H
#define OVERRIDE_LWIPOPTS_H

#define MEMP_NUM_TCP_PCB 13
#define TCP_SND_BUF      (2 * 1460)

#include_next "lwipopts.h"

#endif
