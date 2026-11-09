# Changelog

## 0.1.0

Initial standalone packaging of the in-tree embedded logger.

- `LWLOG_E`, `LWLOG_W`, `LWLOG_I` and `LWLOG_D` retain compile-time filtering with no argument evaluation at disabled levels. Direct `lwlog_write()` calls bypass that filter.
- Normal output uses a fixed-size stack buffer, optional millisecond timestamps and CRLF termination, including truncated lines.
- `lwlog_assert()` writes raw `ASSERT file:line` fragments without printf formatting, timestamps or log-level filtering. It returns; the application owns the halt policy.
- Application-owned `lwlog_config.h` and link-time `lwlog_port_output()` / optional `lwlog_port_get_tick_ms()` keep the library independent of board and OS code.
- New for the standalone repository: `README.md`, `LICENSE`, `doc/porting.md`, `examples/lwlog_demo.c`, `examples/lwlog_config.h` and `build.sh`. The four existing files under `src/` are unchanged.
- Known gaps: no internal locking, ISR-safety guarantee, asynchronous output, runtime filtering or transport implementation. The current source assumes ARM32-style `size_t` / `uint32_t` types; strict 64-bit-host warning cleanliness is not claimed. Hardware-port and C-runtime behaviour must be verified by the integrator.
