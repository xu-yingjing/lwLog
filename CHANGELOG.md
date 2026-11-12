# Changelog

## 0.1.2

- Add optional output serialization controlled by `LWLOG_USE_LOCK`, enabled by default. A lock-enabled build calls `lwlog_init()` once and links `lwlog_port_lock_init()`, `lwlog_port_lock()` and `lwlog_port_unlock()`; setting `LWLOG_USE_LOCK` to zero removes the lock calls and hook dependencies.
- `lwlog_write()` formats the complete line first and then holds the lock for the single output-hook call, so normal lines are serialized without keeping the clock or libc formatter inside the critical section.
- `lwlog_assert()` never takes the normal lock. It remains available in fault, interrupt and critical contexts where blocking on a task mutex could deadlock, at the cost of allowing assertion fragments to interleave with other output. Port hooks must not call the logger recursively.
- Fix a potential signed-integer overflow when `vsnprintf()` reports more characters than the remaining line capacity; the formatter now clamps before adding.
- Update the configuration template, example and documentation for the lock contract, and exercise lock initialisation, normal lock pairing and the assertion lock bypass in the demo.

## 0.1.1

- Add ANSI SGR colours controlled by `LWLOG_USE_COLOR`, enabled by default. Normal lines are coloured by level (error red, warning yellow, info green, debug grey) with a reset before the CRLF; `lwlog_assert()` output is red. Define `LWLOG_USE_COLOR` as zero for plain text.
- The colour and reset sequences share the existing `LWLOG_LINE_MAX` stack buffer, so the prefix-plus-message budget shrinks by 9 bytes when colour is enabled. The single output call per normal line and the plain-text output are unchanged.

## 0.1.0

Initial standalone packaging of the in-tree embedded logger.

- `LWLOG_E`, `LWLOG_W`, `LWLOG_I` and `LWLOG_D` retain compile-time filtering with no argument evaluation at disabled levels. Direct `lwlog_write()` calls bypass that filter.
- Normal output uses a fixed-size stack buffer, optional millisecond timestamps and CRLF termination, including truncated lines.
- `lwlog_assert()` writes raw `ASSERT file:line` fragments without printf formatting, timestamps or log-level filtering. It returns; the application owns the halt policy.
- Application-owned `lwlog_config.h` and link-time `lwlog_port_output()` / optional `lwlog_port_get_tick_ms()` keep the library independent of board and OS code.
- New for the standalone repository: `README.md`, `LICENSE`, `doc/porting.md`, `examples/lwlog_demo.c`, `examples/lwlog_config.h` and `build.sh`. The four existing files under `src/` are unchanged.
- Known gaps: no internal locking, ISR-safety guarantee, asynchronous output, runtime filtering or transport implementation. The current source assumes ARM32-style `size_t` / `uint32_t` types; strict 64-bit-host warning cleanliness is not claimed. Hardware-port and C-runtime behaviour must be verified by the integrator.
