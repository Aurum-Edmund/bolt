# Runtime Stub Plan (Stage-0)

Stage‑0 requires a minimal freestanding runtime so compiled Bolt programs can boot under the Air kernel. The runtime lives in `runtime/` and is bundled by the linker wrapper.

## Core Stubs

| Stub | Description | Notes |
|------|-------------|-------|
| `_start` | Assembly entry point invoked by the loader. Sets stack, zeroes `.bss`, calls `start`, and halts or loops on return. | Implement in NASM/YASM syntax to align with existing build toolchain. |
| `bolt_panic_abort` | Non-returning panic routine that halts/hangs the CPU. | Must respect Live ordering; optionally print diagnostics when debug I/O is available. |
| `bolt_memory_copy` | Memory copy helper (maps to the **Memory Copy** glossary term). | Thin wrapper around `intrinsic_memcpy`; enforces non-overlapping behaviour and Live fences. |
| `bolt_memory_fill` | Memory fill helper (maps to the **Memory Fill** glossary term). | Wraps `intrinsic_memset`; used by allocator and runtime zeroing. |
| `bolt_atomic_compare_exchange` (and related) | Minimal atomic intrinsics required by the compiler IR. | Coordinate with upcoming MIR SSA/Live enforcement. |

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


