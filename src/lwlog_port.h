#ifndef LWLOG_PORT_H
#define LWLOG_PORT_H

#include "lwlog.h"
#include <stdint.h>

#if (LWLOG_USE_LOCK != 0)
void lwlog_port_lock_init(void);
void lwlog_port_lock(void);
void lwlog_port_unlock(void);
#endif

#if (LWLOG_USE_TIMESTAMP != 0)
uint32_t lwlog_port_get_tick_ms(void);
#endif

void lwlog_port_output(const char *buf, uint32_t len);

#endif
