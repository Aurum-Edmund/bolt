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

## Command Line Interface

The Stage-0 wrapper now exposes a deterministic set of options that mirror the pieces the final linker will need to assemble:

| Option | Purpose |
| --- | --- |
| `--emit=<kind>` | Selects the artifact kind: `exe`, `lib` (alias `link`), `air`, or `zap`. |
| `--target=<triple>` | Chooses the platform triple. Supported values: `x86_64-pc-windows-msvc`, `x86_64-air-bolt`. |
| `--sysroot=<path>` | Overrides the sysroot forwarded to the platform linker. |
| `--runtime-root=<path>` | Points to the directory that contains runtime stubs and helper archives. |
| `--linker-script=<path>` | Supplies the freestanding linker script for Air images. |
| `--import-bundle=<path>` | Provides the resolved import metadata bundle to embed. |
| `-L<path>` / `-l<name>` | Adds library search paths and static libraries. Both short and separated forms are accepted. |
| `-o <path>` | Sets the output artifact path (required unless `--help`/`--version` is provided). |
| `--verbose` | Emits the constructed linker command line once the driver grows invocation support. |
| `--dry-run` | Resolves inputs without launching the platform linker (current default behaviour). |

Basic validation ensures required arguments are present, targets are recognised, and unsupported artifact kinds are rejected. The wrapper still performs a dry run today, but the parsed configuration is now structured for consumption by the upcoming linker back ends.

## Next Steps

1. Draft the base linker script template (Freestanding x86-64, entry `_start`).
2. Integrate runtime stubs once they land in `runtime/`.
3. Add unit/regression tests that build `examples/add.bolt` into a COFF/ELF artifact and smoke-boot under QEMU scripts (future milestone).


