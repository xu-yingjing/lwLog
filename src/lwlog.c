#include "lwlog.h"
#include "lwlog_port.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#if (LWLOG_USE_COLOR != 0)
#define LWLOG_COLOR_ERROR "\x1b[31m"
#define LWLOG_COLOR_WARN  "\x1b[33m"
#define LWLOG_COLOR_INFO  "\x1b[32m"
#define LWLOG_COLOR_DEBUG "\x1b[90m"
#define LWLOG_COLOR_RESET "\x1b[0m"

typedef struct {
    const char *sequence;
    uint32_t length;
} lwlog_color_t;

static lwlog_color_t lwlog_level_color(char level)
{
    switch (level) {
    case 'E':
        return (lwlog_color_t){LWLOG_COLOR_ERROR, (uint32_t)(sizeof(LWLOG_COLOR_ERROR) - 1U)};
    case 'W':
        return (lwlog_color_t){LWLOG_COLOR_WARN, (uint32_t)(sizeof(LWLOG_COLOR_WARN) - 1U)};
    case 'I':
        return (lwlog_color_t){LWLOG_COLOR_INFO, (uint32_t)(sizeof(LWLOG_COLOR_INFO) - 1U)};
    case 'D':
        return (lwlog_color_t){LWLOG_COLOR_DEBUG, (uint32_t)(sizeof(LWLOG_COLOR_DEBUG) - 1U)};
    default:
        return (lwlog_color_t){"", 0U};
    }
}
#endif

static void lwlog_put_u32(uint32_t value)
{
    char digit;
    uint32_t div = 1U;

    while ((value / div) >= 10U) {
        div *= 10U;
    }

    while (div != 0U) {
        digit = (char)('0' + (value / div) % 10U);
        lwlog_port_output(&digit, 1U);
        div /= 10U;
    }
}

void lwlog_assert(const char *file, int line)
{
    static const char prefix[] = "ASSERT ";
    static const char unknown[] = "?";
    static const char colon[] = ":";
#if (LWLOG_USE_COLOR != 0)
    static const char suffix[] = LWLOG_COLOR_RESET "\r\n";
#else
    static const char suffix[] = "\r\n";
#endif

#if (LWLOG_USE_COLOR != 0)
    lwlog_port_output(LWLOG_COLOR_ERROR, sizeof(LWLOG_COLOR_ERROR) - 1U);
#endif
    lwlog_port_output(prefix, sizeof(prefix) - 1U);
    if (file == NULL) {
        file = unknown;
    }
    lwlog_port_output(file, strlen(file));
    lwlog_port_output(colon, sizeof(colon) - 1U);
    lwlog_put_u32((uint32_t)line);
    lwlog_port_output(suffix, sizeof(suffix) - 1U);
}

void lwlog_write(char level, const char *tag, const char *fmt, ...)
{
    char buf[LWLOG_LINE_MAX];
#if (LWLOG_USE_COLOR != 0)
    lwlog_color_t color = lwlog_level_color(level);
    uint32_t prefix_len = color.length;
    uint32_t suffix_len = (color.length != 0U) ? ((uint32_t)(sizeof(LWLOG_COLOR_RESET) - 1U) + 2U) : 2U;
#else
    uint32_t prefix_len = 0U;
    uint32_t suffix_len = 2U;
#endif
    uint32_t max_text = (uint32_t)sizeof(buf) - prefix_len - suffix_len - 1U;
    uint32_t pos;
    int n;
    int m;
    va_list ap;

#if (LWLOG_USE_COLOR != 0)
    if (prefix_len != 0U) {
        memcpy(buf, color.sequence, (size_t)prefix_len);
    }
#endif

#if (LWLOG_USE_TIMESTAMP != 0)
    n = snprintf(&buf[prefix_len], (size_t)sizeof(buf) - (size_t)prefix_len,
                 "[%u] %c %s: ", lwlog_port_get_tick_ms(), level, tag);
#else
    n = snprintf(&buf[prefix_len], (size_t)sizeof(buf) - (size_t)prefix_len,
                 "%c %s: ", level, tag);
#endif
    if (n < 0) {
        return;
    }

    if ((size_t)n > (size_t)max_text) {
        n = (int)max_text;
    }

    va_start(ap, fmt);
    m = vsnprintf(&buf[prefix_len + (uint32_t)n], (size_t)max_text - (size_t)n + 1U, fmt, ap);
    va_end(ap);

    if (m > 0) {
        n += m;
        if ((size_t)n > (size_t)max_text) {
            n = (int)max_text;
        }
    }

    pos = prefix_len + (uint32_t)n;

#if (LWLOG_USE_COLOR != 0)
    if (color.length != 0U) {
        memcpy(&buf[pos], LWLOG_COLOR_RESET, sizeof(LWLOG_COLOR_RESET) - 1U);
        pos += (uint32_t)(sizeof(LWLOG_COLOR_RESET) - 1U);
    }
#endif
    buf[pos++] = '\r';
    buf[pos++] = '\n';

    lwlog_port_output(buf, pos);
}
