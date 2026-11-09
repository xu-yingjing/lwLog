#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "lwlog_port.h"

#if (LWLOG_USE_TIMESTAMP != 0)
#define DEMO_TIME "[123] "
static unsigned int tick_calls;

uint32_t lwlog_port_get_tick_ms(void)
{
    tick_calls++;
    return 123U;
}
#else
#define DEMO_TIME ""
#endif

static char output[LWLOG_LINE_MAX + 64U];
static size_t output_len;
static unsigned int output_calls;
static int output_overflow;

void lwlog_port_output(const char *buf, uint32_t len)
{
    output_calls++;
    if ((size_t)len > sizeof(output) - 1U - output_len) {
        output_overflow = 1;
        return;
    }
    memcpy(&output[output_len], buf, (size_t)len);
    output_len += (size_t)len;
    output[output_len] = '\0';
}

static void reset_output(void)
{
    output_len = 0U;
    output_calls = 0U;
    output_overflow = 0;
    output[0] = '\0';
#if (LWLOG_USE_TIMESTAMP != 0)
    tick_calls = 0U;
#endif
}

static int check_output(const char *name, const char *want)
{
    if (output_overflow || output_len != strlen(want) || strcmp(output, want) != 0) {
        printf("FAIL %s\n", name);
        return 1;
    }
    printf("ok   %s\n", name);
    return 0;
}

static int check_levels(void)
{
    static const char want[] =
#if (LWLOG_OUTPUT_LEVEL >= LWLOG_LEVEL_ERROR)
        DEMO_TIME "E DEMO: 1\r\n"
#endif
#if (LWLOG_OUTPUT_LEVEL >= LWLOG_LEVEL_WARN)
        DEMO_TIME "W DEMO: 2\r\n"
#endif
#if (LWLOG_OUTPUT_LEVEL >= LWLOG_LEVEL_INFO)
        DEMO_TIME "I DEMO: 3\r\n"
#endif
#if (LWLOG_OUTPUT_LEVEL >= LWLOG_LEVEL_DEBUG)
        DEMO_TIME "D DEMO: 4\r\n"
#endif
        "";
    int evaluated = 0;
    int failures;

    reset_output();
    LWLOG_E("DEMO", "%d", ++evaluated);
    LWLOG_W("DEMO", "%d", ++evaluated);
    LWLOG_I("DEMO", "%d", ++evaluated);
    LWLOG_D("DEMO", "%d", ++evaluated);
    failures = check_output("level filtering", want);
    if (evaluated != (int)LWLOG_OUTPUT_LEVEL || output_calls != LWLOG_OUTPUT_LEVEL) {
        printf("FAIL disabled macro argument evaluation\n");
        failures++;
    }
    return failures;
}

static int check_truncation(const char *name, const char *prefix)
{
    size_t i;
    size_t prefix_len = strlen(prefix);

    if (output_overflow || output_calls != 1U || output_len != LWLOG_LINE_MAX - 1U ||
        memcmp(output, prefix, prefix_len) != 0 ||
        output[output_len - 2U] != '\r' || output[output_len - 1U] != '\n') {
        printf("FAIL %s\n", name);
        return 1;
    }
    for (i = prefix_len; i < output_len - 2U; i++) {
        if (output[i] != 'x') {
            printf("FAIL %s content\n", name);
            return 1;
        }
    }
    printf("ok   %s\n", name);
    return 0;
}

static int check_assertions(void)
{
    int failures;

    reset_output();
    lwlog_assert("demo.c", 12345);
    failures = check_output("assert decimal digits", "ASSERT demo.c:12345\r\n");
#if (LWLOG_USE_TIMESTAMP != 0)
    if (tick_calls != 0U) {
        printf("FAIL assertion clock dependency\n");
        failures++;
    }
#endif
    reset_output();
    lwlog_assert("demo.c", 0);
    failures += check_output("assert zero", "ASSERT demo.c:0\r\n");
    reset_output();
    lwlog_assert(NULL, 7);
    failures += check_output("assert NULL file", "ASSERT ?:7\r\n");
    return failures;
}

int main(void)
{
    char long_text[LWLOG_LINE_MAX + 16U];
    int failures;

    reset_output();
    lwlog_write('I', "DEMO", "value=%d", 42);
    failures = check_output("direct formatting", DEMO_TIME "I DEMO: value=42\r\n");
    if (output_calls != 1U) {
        printf("FAIL normal output call count\n");
        failures++;
    }
#if (LWLOG_USE_TIMESTAMP != 0)
    if (tick_calls != 1U) {
        printf("FAIL normal clock call count\n");
        failures++;
    }
#endif
    failures += check_levels();

    memset(long_text, 'x', sizeof(long_text) - 1U);
    long_text[sizeof(long_text) - 1U] = '\0';
    reset_output();
    lwlog_write('I', "DEMO", "%s", long_text);
    failures += check_truncation("long message", DEMO_TIME "I DEMO: ");
    reset_output();
    lwlog_write('D', long_text, "%s", "discarded");
    failures += check_truncation("long tag", DEMO_TIME "D ");
    failures += check_assertions();

    if (failures != 0) {
        printf("%d check(s) failed\n", failures);
        return 1;
    }
    printf("all checks passed\n");
    return 0;
}
