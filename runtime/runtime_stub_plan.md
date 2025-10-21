# Runtime Stub Plan (Stage-0)

Stage‑0 requires a minimal freestanding runtime so compiled Bolt programs can boot under the Air kernel. The runtime lives in `runtime/` and is bundled by the linker wrapper.

## Core Stubs

| Stub | Description | Notes |
|------|-------------|-------|
| `_start` | Assembly entry point invoked by the loader. Sets stack, zeroes `.bss`, calls `start`, and halts or loops on return. | Implement in NASM/YASM syntax to align with existing build toolchain. |
| `bolt_panic_abort` | Non-returning panic routine that halts/hangs the CPU. | Must respect live ordering; optionally print diagnostics when debug I/O is available. |
| `bolt_memory_copy` | Memory copy helper (maps to the **Memory Copy** glossary term). | Thin wrapper around `intrinsic_memcpy`; enforces non-overlapping behaviour and live fences. |
| `bolt_memory_fill` | Memory fill helper (maps to the **Memory Fill** glossary term). | Wraps `intrinsic_memset`; used by allocator and runtime zeroing. |
| `bolt_atomic_compare_exchange` (and related) | Minimal atomic intrinsics required by the compiler IR. | Coordinate with upcoming MIR SSA/live enforcement. |
| `bolt_new` / `bolt_delete` | Deterministic allocation primitives. | Zero-initialise storage; callers must release explicitly. |
| `bolt_shared_pointer_*` | Shared-ownership smart pointer helpers. | Provide make/copy/move/isValid/release without mutexes. |

## Integration Notes

1. **Calling Convention**: follow the Air kernel ABI (System V AMD64 without red zone). All stubs must preserve callee-saved registers.
2. **Linker Symbols**: expose `_start`, `_bolt_runtime_panic`, `_bolt_runtime_memcpy`, `_bolt_runtime_memset`, etc., so linker scripts can place them explicitly.
3. **Configuration Flags**: allow the driver to toggle optional diagnostics (panic message emission) via linker or runtime metadata.
4. **Testing**: unit-level tests can call the C versions (`bolt_panic_abort` etc.) under a hosted environment; integration tests will boot the image once the linker wrapper is ready.

## Immediate Work Items

1. Author header (`runtime/runtime.h`) declaring the stub APIs for the compiler back end.
2. Implement `_start` entry (currently provided as a C stub) and C implementations for panic/memory helpers.
3. Wire runtime objects into the build (`CMakeLists.txt`) and ensure driver/linker include them automatically.
4. Add smoke tests that link `examples/add.bolt` against the runtime and verify return code/panic behaviour.

## Current Progress

- Implemented portable atomic helper APIs (`bolt_atomic_load/store/exchange/compare_exchange`) for 8-bit, 16-bit, 32-bit, and
  64-bit values, using C11 atomics on hosted builds and Windows interlocked fallbacks when compiling with MSVC. Unit tests in
  `tests/unit/runtime/RuntimeHelpersTest.cpp` cover load/store, exchange, and compare-exchange behaviour for each width.
- Added `bolt_atomic_fetch_add`/`bolt_atomic_fetch_sub` helpers for all supported widths so MIR can lower arithmetic atomics;
  unit tests cover both operations to guard future regressions.
- Added `bolt_atomic_fetch_and`/`bolt_atomic_fetch_or`/`bolt_atomic_fetch_xor` helpers for all supported widths so bitwise atomics
  required by Stage-0 lowering are available with matching unit coverage.
- Implemented deterministic allocation helpers (`bolt_new`, `bolt_delete`) and a shared-pointer runtime (`bolt_shared_pointer_make`/`copy`/`move`/`is_valid`/`release`) to back the new pointer syntax and object validity checks. Unit coverage exercises zero-initialisation, copy semantics, move semantics, and destructor invocation.


