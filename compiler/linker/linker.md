# Linker Plan (Stage-0)

The linker component (`bolt-ld`) is responsible for turning Stage‑0 object files into a freestanding image that can boot under the Air kernel environment. The wrapper orchestrates platform linkers (LLD/`link.exe`) and injects configuration that matches the Bolt runtime.

## Responsibilities

1. **Object Intake**
   - Consume COFF objects on Windows for developer builds and ELF objects for the freestanding image.
   - Aggregate runtime stubs (`_start`, panic abort, memory copy/fill helpers, atomic intrinsics) alongside application objects.

2. **Linker Script Management**
   - Generate or select the appropriate linker script that places `.text`, `.data`, `.bss`, and bootstrap sections according to the Air ABI.
   - Expose configuration knobs (output format, entry symbol, section layout) via CLI flags that mirror the build instructions.

3. **Import Metadata Integration**
   - Bundle resolved import metadata emitted during MIR lowering so that the runtime can validate module boundaries at load time.
   - Emit diagnostics if referenced modules lack corresponding objects or archives.

4. **Artifact Production**
   - Produce developer-friendly COFF/PE executables (`.exe`) and static libraries (`.lib`) for Windows iteration.
   - Produce Air kernel executables (`.air`) using the freestanding linker script.
   - Produce Bolt library archives (`.zap`) for reuse across modules and import resolution.
   - Optionally emit map files and symbol tables for debugging.

5. **Automation Hooks**
   - Provide a CMake entry point (`bolt_link`) used by tests and examples.
   - Surface deterministic hashes or timestamps so golden tests can compare outputs.

## Next Steps

1. Define the CLI for `bolt-ld` (target selection, linker script path overrides, import metadata bundle).
2. Draft the base linker script template (Freestanding x86-64, entry `_start`).
3. Integrate runtime stubs once they land in `runtime/`.
4. Add unit/regression tests that build `examples/add.bolt` into a COFF/ELF artifact and smoke-boot under QEMU scripts (future milestone).


