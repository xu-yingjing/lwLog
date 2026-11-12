# Porting

The application supplies a configuration header and link-time port functions. There is no global handle, callback registration or OS dependency; `lwlog_init()` performs only the one-time initialisation of the optional port lock. Only the library source and the application's port need to be compiled.

## Integration order

1. Create `lwlog_config.h` in the application's configuration directory. Copying `src/lwlog_config_template.h` is optional; remove its outer `#if 0` and matching final `#endif` if using the template.
2. Add `src/` and that configuration directory to every translation unit that includes `lwlog.h` or `lwlog_port.h`, and compile `src/lwlog.c`.
3. Implement `lwlog_port_output()`; when timestamps are enabled, `lwlog_port_get_tick_ms()`; when locking is enabled, `lwlog_port_lock_init()`, `lwlog_port_lock()` and `lwlog_port_unlock()` in an application-owned source file that includes `lwlog_port.h`.
4. Initialise the output transport before logging. A port may deliberately discard output before transport readiness; the library does not track readiness.
5. Call `lwlog_init()` exactly once before the first log call. It invokes `lwlog_port_lock_init()` when `LWLOG_USE_LOCK != 0`; with locking disabled it does nothing.
6. Allocate enough task or startup stack, then call the log macros.

All translation units must see the same configuration. In particular, changing `LWLOG_USE_TIMESTAMP` only for the port does not remove the clock call from an already compiled library object. Do not use the example configuration directory in production.

## Configuration

`lwlog_config.h` is included before the `LWLOG_LEVEL_*` constants are defined. A definition such as `#define LWLOG_OUTPUT_LEVEL LWLOG_LEVEL_INFO` is valid because expansion happens later, but an immediate `#if` test of those constants inside the configuration header is not.

The library provides these defaults for omitted settings:

```c
#define LWLOG_OUTPUT_LEVEL LWLOG_LEVEL_INFO
#define LWLOG_USE_LOCK 1U
#define LWLOG_USE_TIMESTAMP 1U
#define LWLOG_LINE_MAX 128U
#define LWLOG_USE_COLOR 1U
```

The output threshold is 0 through 4 inclusive (`NONE`, `ERROR`, `WARN`, `INFO`, `DEBUG`). Values outside that range are compile errors. `LWLOG_LINE_MAX` must be at least 32 bytes. Zero disables timestamps; a nonzero value enables them. Zero disables locking and removes the three lock-hook dependencies; a nonzero value (the default) requires the hooks and a `lwlog_init()` call before the first log. A nonzero `LWLOG_USE_COLOR` (the default) adds ANSI SGR colour sequences to normal and assertion output, which the port must pass through unchanged and the receiving terminal must interpret; zero emits plain text.

The threshold applies only to the four level macros. A disabled macro expands to `((void)0)`, so do not put required side effects in its tag, format or argument expressions. `lwlog_write()` and `lwlog_assert()` are not filtered, even at `LWLOG_LEVEL_NONE`.

## Output contract

```c
void lwlog_port_output(const char *buf, uint32_t len);
```

Consume exactly `len` bytes synchronously, or copy them into storage owned by the port before returning. The bytes are not a NUL-terminated string. Do not save the pointer or start DMA directly from it and return: normal output uses a local stack buffer, and assertion digits are also stack objects. A port-owned queue is possible, but queue capacity, copying, backpressure and failure policy remain the port's responsibility.

A normal log call invokes the output hook once. Assertion output uses several calls: the error colour when enabled, a prefix, file name, colon, individual decimal digits and the reset plus CRLF. Do not append newlines in the hook. There is no return value and therefore no way to report a dropped or partially transmitted line to the library. Do not call the logger recursively from either port hook.

## Lock contract

```c
void lwlog_port_lock_init(void);
void lwlog_port_lock(void);
void lwlog_port_unlock(void);
```

Supply these functions only when `LWLOG_USE_LOCK != 0`. `lwlog_init()` calls `lwlog_port_lock_init()`; call `lwlog_init()` exactly once before the first log. The lock implementation is application-owned. A task mutex is typical; a critical section or scheduler lock is possible only if it can be taken from every context used for normal logging.

`lwlog_write()` builds the complete line first, then calls `lwlog_port_lock()`, `lwlog_port_output()` exactly once and `lwlog_port_unlock()`. The lock therefore serializes complete normal lines, while timestamp acquisition and libc formatting run outside it and may overlap across tasks. The clock hook must tolerate concurrent calls unless the application serializes logging externally. Do not make the lock recursive and do not call the logger from inside the hooks.

The lock covers the entire output hook call, which may block on the transport. Keep it short, but do not release it between fragments of a normal line because the line is already a single output call. `LWLOG_USE_LOCK == 0` removes the calls and the hook declarations, so a lock-free build does not link these functions; concurrent output is then entirely the application's responsibility.

`lwlog_assert()` never takes this lock. See [Assertions](#assertions) for the resulting trade-off.

A blocking task mutex is suitable only for normal task or main-loop logging. Do not call `lwlog_write()` from an ISR, fault handler, critical section or while the scheduler is suspended: taking the mutex there can deadlock or violate the RTOS calling rules. The assertion path does not take the lock, but its output hook and transport must still support the assertion's calling context.

## Clock contract

```c
uint32_t lwlog_port_get_tick_ms(void);
```

Supply this function only when `LWLOG_USE_TIMESTAMP != 0`. Return uptime in milliseconds, not raw RTOS ticks, microseconds or cycles. A zero value before the scheduler or timer starts is acceptable. The counter is printed as an unsigned decimal number without padding and wraps with `uint32_t`; no timeout logic depends on it.

Normal output reads the clock once per call when enabled. The assertion path never reads it. Define `LWLOG_USE_TIMESTAMP` as zero and rebuild the library to remove the clock dependency entirely.

## Normal lines

```c
void lwlog_write(char level, const char *tag, const char *fmt, ...);
```

Supply non-NULL, NUL-terminated `tag` and `fmt` strings and arguments matching the format. The function does not validate them or interpret the level character. Use trusted format strings, for example `LWLOG_I("NET", "%s", received_text)`, rather than passing external text as a format string.

The grammar is `[milliseconds] level tag: message\r\n`, or `level tag: message\r\n` without timestamps. With colour enabled, a five-byte SGR sequence precedes the line and a four-byte reset sequence precedes the CRLF. There is no escaping or splitting of embedded newlines. Formatting capabilities, including floating point, wide strings and 64-bit integers, are properties of the target's libc, not guarantees of lwlog.

`LWLOG_LINE_MAX` is the buffer size, not the maximum message length. Without colour, prefix and message together occupy at most `LWLOG_LINE_MAX - 3` bytes; CRLF adds two bytes, so at most `LWLOG_LINE_MAX - 1` bytes reach the port. Enabling colour consumes the same buffer, reducing the prefix-plus-message budget to `LWLOG_LINE_MAX - 12`; the single output call per normal line is unchanged. A long tag can consume the entire prefix budget, leaving no room for the colon or message. Truncation does not add an ellipsis and can split multibyte text.

If prefix formatting returns a negative result, nothing is output. If message formatting returns a negative result, the prefix and CRLF are still output; no error status is returned. The library itself does not allocate or keep mutable static state, but it reserves `LWLOG_LINE_MAX` bytes on each normal call's stack in addition to formatter and port stack use. Check the linked call graph and worst-case formats when sizing a task stack.

With locking enabled, the lock covers the single output-hook call; prefix formatting, the clock read and `snprintf` / `vsnprintf` happen before it. A normal line is never split across lock acquisitions. With locking disabled, concurrent tasks can interleave their complete output calls, and the port must handle that or the application must serialize logging.

The current source uses `%u` for the timestamp and passes `strlen(file)` to the `uint32_t` output length. The original ARM32 ABI makes `uint32_t` and `size_t` both `unsigned int`. On a different ABI, verify the timestamp format type; 64-bit hosts can emit a `-Wconversion` diagnostic for the file-name length. The standalone packaging does not change these source-level assumptions.

## Assertions

```c
void lwlog_assert(const char *file, int line);
```

Output is `ASSERT file:line\r\n`, wrapped in the error colour and reset when `LWLOG_USE_COLOR` is enabled. A NULL file pointer becomes `?`; other file pointers must name valid NUL-terminated strings. The line is converted to `uint32_t` and printed as decimal, so callers normally pass a nonnegative value such as `__LINE__`.

This path uses literal fragments, `strlen()` and a small integer-to-decimal helper. It does not call `snprintf` / `vsnprintf`, allocate the normal line buffer, read the clock or check the log threshold. The file name is not truncated: the total assertion length is not bounded by `LWLOG_LINE_MAX`.

The assertion path bypasses `LWLOG_USE_LOCK` entirely: it never calls the lock hooks, even when they are enabled. It therefore remains usable when another task holds the normal lock, in a fault handler or in interrupt context, but its fragments can interleave with a normal line or another assertion. The output hook must be able to make progress without the normal lock in that context, and the application remains responsible for making the transport usable there.

`lwlog_assert()` returns. It does not disable interrupts, halt, reset, flush a queued transport or guarantee delivery after a fault. A bare-metal caller that wants to stop can use:

```c
if (invariant_failed) {
    lwlog_assert(__FILE__, __LINE__);
    for (;;) {
    }
}
```

Interrupt masking and reset policy belong in the caller. If interrupts are masked or the scheduler is stopped, the output hook must still be able to make progress; a port that waits on an interrupt, mutex or logging task may deadlock. Use a synchronous polling transport when those contexts need assertion output.

## Concurrency

With `LWLOG_USE_LOCK` and a port lock, complete normal log lines are serialized across tasks; formatting and timestamp work still run concurrently and must be safe in the chosen contexts. Assertions are always unlocked, so they may interleave at fragment granularity with normal output or with another assertion. If the application needs atomic assertion output, it must provide a separate mechanism that is safe in the assertion's context; the library does not.

Task or main-loop logging is the intended integration for normal lines. Blocking task mutexes must not be taken from an ISR, fault handler, critical section or suspended scheduler. `lwlog_assert()` avoids the normal lock but still calls the output hook, so interrupt and fault safety depend entirely on the port and transport. Do not call the logger recursively from a port hook.

## Porting checklist

- The library, port and callers use one consistent `lwlog_config.h`.
- Output consumes or copies bytes before returning and never assumes NUL termination.
- The transport is ready, and unavailable-output behaviour is intentional.
- Timestamps are milliseconds; a timestamp-disabled build links without the clock hook.
- A lock-enabled build calls `lwlog_init()` exactly once before the first log and links a lock that is ready at that point; a lock-disabled build links without the lock hooks.
- The normal stack budget includes the buffer, libc formatting and port.
- When colour is enabled, the viewer interprets ANSI sequences and the reduced message budget still holds the longest prefix and message.
- The lock covers the complete normal output call and is released on every path.
- The clock hook and libc formatting run outside the lock and tolerate concurrent calls.
- Assertions bypass the normal lock, and the output hook can make progress in the intended fault or interrupt context.
- Shared normal output is serialized when locking is enabled; assertion fragments may interleave, and any extra serialization is application-owned.
- The assertion caller owns halt/reset policy, and the chosen transport can progress in that context.
- Target ABI and format-specifier support have been checked against the linked C runtime.
