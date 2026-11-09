#include "lwlog.h"
#include "lwlog_port.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

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
    static const char crlf[] = "\r\n";

    lwlog_port_output(prefix, sizeof(prefix) - 1U);
    if (file == NULL) {
        file = unknown;
    }
    lwlog_port_output(file, strlen(file));
    lwlog_port_output(colon, sizeof(colon) - 1U);
    lwlog_put_u32((uint32_t)line);
    lwlog_port_output(crlf, sizeof(crlf) - 1U);
}

void lwlog_write(char level, const char *tag, const char *fmt, ...)
{
    char buf[LWLOG_LINE_MAX];
    int n;
    int m;
    va_list ap;

#if (LWLOG_USE_TIMESTAMP != 0)
    n = snprintf(buf, sizeof(buf), "[%u] %c %s: ", lwlog_port_get_tick_ms(), level, tag);
#else
    n = snprintf(buf, sizeof(buf), "%c %s: ", level, tag);
#endif
    if (n < 0) {
        return;
    }

    if ((size_t)n > (sizeof(buf) - 3U)) {
        n = (int)sizeof(buf) - 3;
    }

    va_start(ap, fmt);
    m = vsnprintf(&buf[n], sizeof(buf) - (size_t)n - 2U, fmt, ap);
    va_end(ap);

    if (m > 0) {
        n += m;
        if ((size_t)n > (sizeof(buf) - 3U)) {
            n = (int)sizeof(buf) - 3;
        }
    }

    buf[n++] = '\r';
    buf[n++] = '\n';

    lwlog_port_output(buf, (uint32_t)n);
}
