# lwlog

Small formatted logger for embedded C99 targets, with no OS dependency, no HAL and no dynamic allocation in the library.

The application supplies a configuration header, an output function and, when enabled, port lock hooks. `lwlog_init()` initialises the optional lock and must be called once before the first log call. Normal messages use a stack buffer and the C library's `snprintf` / `vsnprintf`; the assertion path writes raw fragments without printf formatting and deliberately bypasses the normal lock.

## Scope

Supported:

- Error, warning, info and debug macros with compile-time filtering. Disabled macros do not evaluate their arguments.
- An optional millisecond timestamp and a caller-supplied tag.
- ANSI SGR colours, enabled by default: normal lines are coloured by level and assertions are red. Define `LWLOG_USE_COLOR` as zero for plain text.
- Bounded normal log lines, including a trailing CRLF even when the message or tag is truncated.
- Optional serialization of each complete normal line through `LWLOG_USE_LOCK` and application-supplied port locks.
- Raw `ASSERT file:line` output, independent of the configured log level, timestamp, normal line buffer and normal lock.

Not supplied, by design:

- Runtime filtering, multiple destinations, file storage or an asynchronous queue.
- Lock implementation, transport initialisation, interrupt-context safety or a panic/halt policy.
- A formatter independent of libc. Available format specifiers and additional stack use depend on the linked C runtime.

## Quick start

Create an application-owned `lwlog_config.h` on the include path:

```c
#ifndef LWLOG_CONFIG_H
#define LWLOG_CONFIG_H

#define LWLOG_OUTPUT_LEVEL LWLOG_LEVEL_INFO
#define LWLOG_USE_LOCK 1U
#define LWLOG_USE_TIMESTAMP 1U
#define LWLOG_LINE_MAX 128U
#define LWLOG_USE_COLOR 1U

#endif
```

Compile `src/lwlog.c`, add `src/` and the application's configuration directory to the include paths, and link these port functions:

```c
#include "lwlog_port.h"

void lwlog_port_output(const char *buf, uint32_t len)
{
    while (len != 0U) {
        board_console_putc(*buf++);
        len--;
    }
}

#if (LWLOG_USE_LOCK != 0)
void lwlog_port_lock_init(void)
{
    board_log_lock_init();
}

void lwlog_port_lock(void)
{
    board_log_lock();
}

void lwlog_port_unlock(void)
{
    board_log_unlock();
}
#endif

#if (LWLOG_USE_TIMESTAMP != 0)
uint32_t lwlog_port_get_tick_ms(void)
{
    return board_get_tick_ms();
}
#endif
```

`board_console_putc()`, `board_log_lock_*()` and `board_get_tick_ms()` stand for your board functions; include their declarations in the port translation unit. The lock is typically a task-level mutex that serializes the complete output call. `lwlog_assert()` never takes it, and normal logging must not run from an ISR, fault handler, critical section or suspended scheduler. Set `LWLOG_USE_LOCK` to zero when the application has no suitable lock. The complete self-contained test port is in [examples/lwlog_demo.c](examples/lwlog_demo.c).

```c
#include "lwlog.h"

void app_start(void)
{
    lwlog_init();
    LWLOG_I("APP", "boot ok");
    LWLOG_W("SD", "retry %u", 2U);
    LWLOG_D("APP", "not evaluated at the INFO threshold");
}
```

Call `lwlog_init()` exactly once before the first log call. It only initialises the port lock; the output transport must already be usable. With `LWLOG_USE_LOCK` set to zero, `lwlog_init()` does nothing and the lock hooks are not required.

With a clock reading of 123 ms, the first line is `[123] I APP: boot ok\r\n`, wrapped in `\x1b[32m` and `\x1b[0m` with the reset preceding the CRLF. With timestamps disabled it is `I APP: boot ok\r\n`. Set `LWLOG_USE_COLOR` to zero for plain text. No extra newline is needed in the format string.

## Configuration

The header is required even if it only contains an include guard: omitted settings take the defaults below. [src/lwlog_config_template.h](src/lwlog_config_template.h) can be copied into the application's configuration directory and renamed to `lwlog_config.h`; remove its outer `#if 0` and matching final `#endif` to activate the settings.

| Setting | Default | Contract |
| --- | --- | --- |
| `LWLOG_OUTPUT_LEVEL` | `LWLOG_LEVEL_INFO` | `NONE` (0), `ERROR` (1), `WARN` (2), `INFO` (3) or `DEBUG` (4); includes every level at or below the threshold. |
| `LWLOG_USE_LOCK` | `1U` | Any nonzero value serializes each complete normal line through `lwlog_port_lock_init()`, `lwlog_port_lock()` and `lwlog_port_unlock()`; zero removes the lock calls and hook declarations. `lwlog_assert()` never takes this lock. Call `lwlog_init()` once before logging when enabled. |
| `LWLOG_USE_TIMESTAMP` | `1U` | Zero removes the timestamp and the clock-hook dependency; any nonzero value enables it. |
| `LWLOG_LINE_MAX` | `128U` | Buffer bytes per normal call; minimum 32, including space reserved for formatting, CRLF and, when enabled, colour sequences. |
| `LWLOG_USE_COLOR` | `1U` | Any nonzero value prefixes normal lines with the level's ANSI SGR colour (error red, warning yellow, info green, debug grey), inserts a reset before the CRLF, and writes assertions in red. Zero emits plain text. The receiving terminal must interpret ANSI escape sequences. |

Use a deferred macro definition such as `#define LWLOG_OUTPUT_LEVEL LWLOG_LEVEL_DEBUG` in the configuration header. The level constants are defined after that header is included, so do not test those constants with `#if` inside it.

Filtering belongs to `LWLOG_E`, `LWLOG_W`, `LWLOG_I` and `LWLOG_D`. Calling `lwlog_write()` directly bypasses it; `lwlog_assert()` is also always available, including at `LWLOG_LEVEL_NONE`.

## Porting

See [doc/porting.md](doc/porting.md) for output-buffer lifetime, configuration consistency, lock and clock contracts, concurrency, stack requirements and assertion handling. A normal line is formatted outside the lock and then written through one locked output call. `lwlog_assert()` bypasses the lock, so its fragments can interleave with other output. The normal line limit does not bound assertion output, and `lwlog_assert()` returns: the caller decides whether to halt, reset or continue.

The current source targets the ARM32 ABI where `size_t` and `uint32_t` are `unsigned int`. On a 64-bit host, the strict warning set can diagnose the `strlen()` to `uint32_t` conversion in the assertion path. The timestamp format also assumes that `uint32_t` matches `%u`; targets with a different typedef need a source adjustment.

## Verification

`sh build.sh` compiles the library and demo with `-std=c99 -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -Wundef -Wwrite-strings -Werror`. It neither links nor runs the demo. `CC` selects the compiler; `CFLAGS` adds target or configuration options, for example:

```sh
CC=arm-none-eabi-gcc CFLAGS="-mcpu=cortex-m4 -mthumb" sh build.sh
CC=armclang CFLAGS="--target=arm-arm-none-eabi -mcpu=cortex-m4 -mthumb" sh build.sh
CC=armclang CFLAGS="--target=arm-arm-none-eabi -mcpu=cortex-m4 -mthumb -DLWLOG_USE_LOCK=0 -DLWLOG_USE_TIMESTAMP=0" sh build.sh
CC=armclang CFLAGS="--target=arm-arm-none-eabi -mcpu=cortex-m4 -mthumb -DLWLOG_USE_TIMESTAMP=0 -DLWLOG_OUTPUT_LEVEL=0 -DLWLOG_LINE_MAX=32" sh build.sh
```

On a host with a compatible C runtime, build and run the self-checking demo separately:

```sh
cc -std=c99 -Wall -Wextra -I src -I examples src/lwlog.c examples/lwlog_demo.c -o lwlog_demo && ./lwlog_demo
```

The demo captures output with a fixed test clock instead of using hardware. It checks formatting, macro argument filtering, long-message and long-tag truncation, raw assertions, lock initialisation, normal lock pairing and the assertion lock bypass. It uses [examples/lwlog_config.h](examples/lwlog_config.h); do not put that directory on a production include path.

## Layout

```text
src/        lwlog.c, lwlog.h, lwlog_port.h, lwlog_config_template.h
examples/   lwlog_demo.c, lwlog_config.h
doc/        porting.md
build.sh    strict-warning compile check, no hardware involved
```

The standalone packaging starts at 0.1.0; release history is in [CHANGELOG.md](CHANGELOG.md).

## License

BSD 3-Clause, see [LICENSE](LICENSE).
