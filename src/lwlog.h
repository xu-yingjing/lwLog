#ifndef LWLOG_H
#define LWLOG_H

#include "lwlog_config.h"

#define LWLOG_LEVEL_NONE  (0U)
#define LWLOG_LEVEL_ERROR (1U)
#define LWLOG_LEVEL_WARN  (2U)
#define LWLOG_LEVEL_INFO  (3U)
#define LWLOG_LEVEL_DEBUG (4U)

#ifndef LWLOG_OUTPUT_LEVEL
#define LWLOG_OUTPUT_LEVEL LWLOG_LEVEL_INFO
#endif

#if (LWLOG_OUTPUT_LEVEL < LWLOG_LEVEL_NONE) || (LWLOG_OUTPUT_LEVEL > LWLOG_LEVEL_DEBUG)
#error "LWLOG_OUTPUT_LEVEL must be one of LWLOG_LEVEL_*"
#endif

#ifndef LWLOG_USE_LOCK
#define LWLOG_USE_LOCK (1U)
#endif

#ifndef LWLOG_USE_TIMESTAMP
#define LWLOG_USE_TIMESTAMP (1U)
#endif

#ifndef LWLOG_LINE_MAX
#define LWLOG_LINE_MAX (128U)
#endif

#if (LWLOG_LINE_MAX < 32U)
#error "LWLOG_LINE_MAX must be at least 32"
#endif

#ifndef LWLOG_USE_COLOR
#define LWLOG_USE_COLOR (1U)
#endif

#if (LWLOG_OUTPUT_LEVEL >= LWLOG_LEVEL_ERROR)
#define LWLOG_E(tag, ...) lwlog_write('E', (tag), __VA_ARGS__)
#else
#define LWLOG_E(tag, ...) ((void)0)
#endif

#if (LWLOG_OUTPUT_LEVEL >= LWLOG_LEVEL_WARN)
#define LWLOG_W(tag, ...) lwlog_write('W', (tag), __VA_ARGS__)
#else
#define LWLOG_W(tag, ...) ((void)0)
#endif

#if (LWLOG_OUTPUT_LEVEL >= LWLOG_LEVEL_INFO)
#define LWLOG_I(tag, ...) lwlog_write('I', (tag), __VA_ARGS__)
#else
#define LWLOG_I(tag, ...) ((void)0)
#endif

#if (LWLOG_OUTPUT_LEVEL >= LWLOG_LEVEL_DEBUG)
#define LWLOG_D(tag, ...) lwlog_write('D', (tag), __VA_ARGS__)
#else
#define LWLOG_D(tag, ...) ((void)0)
#endif

void lwlog_init(void);

void lwlog_assert(const char *file, int line);

void lwlog_write(char level, const char *tag, const char *fmt, ...);

#endif
