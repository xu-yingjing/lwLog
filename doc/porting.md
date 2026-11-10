# Porting

The application supplies a configuration header and link-time port functions. There is no library initialisation, global handle, callback registration or OS dependency. Only the library source and the application's port need to be compiled.

## Integration order

1. Create `lwlog_config.h` in the application's configuration directory. Copying `src/lwlog_config_template.h` is optional; remove its outer `#if 0` and matching final `#endif` if using the template.
2. Add `src/` and that configuration directory to every translation unit that includes `lwlog.h` or `lwlog_port.h`, and compile `src/lwlog.c`.
3. Implement `lwlog_port_output()` and, when timestamps are enabled, `lwlog_port_get_tick_ms()` in an application-owned source file that includes `lwlog_port.h`.
4. Initialise the output transport before logging. A port may deliberately discard output before transport readiness; the library does not track readiness.
5. Allocate enough task or startup stack, then call the log macros. No library initialisation call is needed.

All translation units must see the same configuration. In particular, changing `LWLOG_USE_TIMESTAMP` only for the port does not remove the clock call from an already compiled library object. Do not use the example configuration directory in production.

## Configuration

`lwlog_config.h` is included before the `LWLOG_LEVEL_*` constants are defined. A definition such as `#define LWLOG_OUTPUT_LEVEL LWLOG_LEVEL_INFO` is valid because expansion happens later, but an immediate `#if` test of those constants inside the configuration header is not.

The library provides these defaults for omitted settings:

```c
#define LWLOG_OUTPUT_LEVEL LWLOG_LEVEL_INFO
#define LWLOG_USE_TIMESTAMP 1U
#define LWLOG_LINE_MAX 128U
#define LWLOG_USE_COLOR 1U
```

The output threshold is 0 through 4 inclusive (`NONE`, `ERROR`, `WARN`, `INFO`, `DEBUG`). Values outside that range are compile errors. `LWLOG_LINE_MAX` must be at least 32 bytes. Zero disables timestamps; a nonzero value enables them. A nonzero value (the default) adds ANSI SGR colour sequences to normal and assertion output, which the port must pass through unchanged and the receiving terminal must interpret; zero emits plain text.

The threshold applies only to the four level macros. A disabled macro expands to `((void)0)`, so do not put required side effects in its tag, format or argument expressions. `lwlog_write()` and `lwlog_assert()` are not filtered, even at `LWLOG_LEVEL_NONE`.

## Output contract

```c
void lwlog_port_output(const char *buf, uint32_t len);
```

Consume exactly `len` bytes synchronously, or copy them into storage owned by the port before returning. The bytes are not a NUL-terminated string. Do not save the pointer or start DMA directly from it and return: normal output uses a local stack buffer, and assertion digits are also stack objects. A port-owned queue is possible, but queue capacity, copying, backpressure and failure policy remain the port's responsibility.

A normal log call invokes the output hook once. Assertion output uses several calls: the error colour when enabled, a prefix, file name, colon, individual decimal digits and the reset plus CRLF. Do not append newlines in the hook. There is no return value and therefore no way to report a dropped or partially transmitted line to the library. Do not call the logger recursively from either port hook.

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

The current source uses `%u` for the timestamp and passes `strlen(file)` to the `uint32_t` output length. The original ARM32 ABI makes `uint32_t` and `size_t` both `unsigned int`. On a different ABI, verify the timestamp format type; 64-bit hosts can emit a `-Wconversion` diagnostic for the file-name length. The standalone packaging does not change these source-level assumptions.

## Assertions

```c
void lwlog_assert(const char *file, int line);
```

Output is `ASSERT file:line\r\n`, wrapped in the error colour and reset when `LWLOG_USE_COLOR` is enabled. A NULL file pointer becomes `?`; other file pointers must name valid NUL-terminated strings. The line is converted to `uint32_t` and printed as decimal, so callers normally pass a nonnegative value such as `__LINE__`.

This path uses literal fragments, `strlen()` and a small integer-to-decimal helper. It does not call `snprintf` / `vsnprintf`, allocate the normal line buffer, read the clock or check the log threshold. The file name is not truncated: the total assertion length is not bounded by `LWLOG_LINE_MAX`.

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

There is no built-in lock. Local formatting buffers do not serialize a shared UART, file or test capture buffer. Serialize complete log and assertion operations when multiple producers share a port; locking each output-hook call separately does not prevent assertion fragments from interleaving. The C runtime and transport must also support the chosen calling contexts.

Task or main-loop logging is the intended integration. Interrupt-context safety is not guaranteed, including for assertions: avoiding printf alone does not make the output hook safe. Do not take an ordinary task mutex from an ISR or a fault path.

## Porting checklist

- The library, port and callers use one consistent `lwlog_config.h`.
- Output consumes or copies bytes before returning and never assumes NUL termination.
- The transport is ready, and unavailable-output behaviour is intentional.
- Timestamps are milliseconds; a timestamp-disabled build links without the clock hook.
- Disabled log macros do not hide required application side effects.
- The normal stack budget includes the buffer, libc formatting and port.
- When colour is enabled, the viewer interprets ANSI sequences and the reduced message budget still holds the longest prefix and message.
- Shared output is serialized across whole operations, including assertion fragments.
- The assertion caller owns halt/reset policy, and the chosen transport can progress in that context.
- Target ABI and format-specifier support have been checked against the linked C runtime.
