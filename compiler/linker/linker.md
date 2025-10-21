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
   - Stage-0 now copies the supplied import bundle alongside the artifact as `<output>.imports` once the platform linker succeeds.

4. **Artifact Production**
   - Produce developer-friendly COFF/PE executables (`.exe`) and static libraries (`.lib`) for Windows iteration.
   - Produce Air kernel executables (`.air`) using the freestanding linker script.
   - Produce Bolt library archives (`.zap`) for reuse across modules and import resolution.
   - Optionally emit map files and symbol tables for debugging.

5. **Automation Hooks**
   - Provide a CMake entry point (`bolt_link`) used by tests and examples.
   - Surface deterministic hashes or timestamps so golden tests can compare outputs.
   - Ship a dry-run smoke test (`bolt_ld_dry_run`) that exercises runtime discovery, linker planning, and map reporting without requiring external platform linkers.

## Command Line Interface

The Stage-0 wrapper now exposes a deterministic set of options that mirror the pieces the final linker will need to assemble:

| Option | Purpose |
| --- | --- |
| `--emit=<kind>` | Selects the artifact kind: `exe`, `lib` (alias `link`), `air`, or `zap`. |
| `--target=<triple>` | Chooses the platform triple. Supported values: `x86_64-pc-windows-msvc`, `x86_64-air-bolt`. |
| `--sysroot=<path>` | Overrides the sysroot forwarded to the platform linker. |
| `--runtime-root=<path>` | Points to the directory that contains runtime stubs and helper archives. Required for `--emit=air` unless `--no-runtime` is provided. |
| `--no-runtime` | Skips runtime archive validation/injection for scenarios that supply their own entry and helper implementations (only valid with `--emit=exe` or `--emit=air`). |
| `--linker-script=<path>` | Supplies the freestanding linker script for Air images. |
| `--import-bundle=<path>` | Provides the resolved import metadata bundle to embed. |
| `--map=<path>` | Writes a linker map file alongside executables or Air images. |
| `--linker=<path>` | Overrides the detected platform linker executable (`link.exe`/`ld.lld`). |
| `--archiver=<path>` | Overrides the detected archive tool (`lib.exe`/`llvm-ar`). |
| `--entry=<symbol>` | Overrides the entry point symbol forwarded to the platform linker. Only valid with `--emit=exe` or `--emit=air`; defaults to `_start` for Air images. |
| `-L<path>` / `-l<name>` | Adds library search paths and static libraries. Both short and separated forms are accepted. |
| `-o <path>` | Sets the output artifact path (required unless `--help`/`--version` is provided). |
| `--verbose` | Emits the constructed linker command line resolved by the wrapper before launching the platform linker. |
| `--dry-run` | Resolves inputs without launching the platform linker. |

If callers omit `--sysroot` or `--runtime-root`, the wrapper now checks the `BOLT_SYSROOT` and `BOLT_RUNTIME_ROOT` environment
variables, respectively. Non-empty values supply the default paths while keeping explicit command-line arguments authoritative
when present. This keeps scripted builds ergonomic without sacrificing reproducibility for invocations that pass the paths
directly.

Basic validation ensures required arguments are present, targets are recognised, and unsupported artifact kinds are rejected. When `--emit=air` or `--emit=zap` is selected without an explicit `--target`, the wrapper now defaults to the Air triple (`x86_64-air-bolt`) so freestanding builds stay ergonomic. Explicit but incompatible target/emit pairs (for example `--emit=air --target=x86_64-pc-windows-msvc` or `--target=x86_64-air-bolt --emit=exe`) produce immediate diagnostics. The wrapper now materialises command plans for the Windows toolchain—`link.exe` for executables and `lib.exe` for static libraries (both optionally resolved from `${sysroot}/bin/`)—the freestanding Air image flow (`ld.lld`, optionally from `${sysroot}/bin/ld.lld`), and Bolt archives assembled with `llvm-ar`. When toolchains install linkers or archivers under non-standard paths, callers can supply `--linker` or `--archiver` to point the wrapper at the exact executable to launch. When callers specify `--entry`, the requested symbol is threaded through to the command plan (`/ENTRY:<symbol>` for Windows, `-e <symbol>` for Air); otherwise the freestanding pipeline defaults to `_start` while host builds defer to the platform linker’s default entry point. Entry overrides for static libraries or Bolt archives are rejected up front so the flag never becomes a silent no-op. The same guardrails now apply to `--map`: the wrapper refuses map requests for archives, ensures the destination directory exists, and forwards `/MAP:<path>` or `--Map=<path>` to the underlying tool so map files are generated deterministically when supported.

Runtime roots now participate directly in command synthesis. Windows executables automatically append the resolved Bolt runtime archive from `--runtime-root` (detecting either `bolt_runtime.lib` or `libbolt_runtime.a`), while Air images require a runtime root and include the same archive at link time to supply `_start`, panic, and memory helpers. The wrapper now probes conventional layouts—`<runtime-root>/`, `<runtime-root>/lib/`, `<runtime-root>/<triple>/`, and `<runtime-root>/lib/<triple>/`—so SDKs that stage libraries under a `lib/` directory or triple-qualified folder resolve without extra configuration. When callers pass `--no-runtime`, the wrapper skips the validation/injection step entirely so bespoke runtimes or bootstraps can link without packaging the Bolt runtime archive. Missing runtime archives still produce immediate diagnostics so misconfigured toolchains fail before invoking the platform linker.

Before spawning the platform linker, the wrapper validates that every referenced file or directory exists (explicit linker/archiver overrides, linker scripts, import bundles, runtime roots, library search directories, and—outside of `--dry-run` runs—each input object). Missing paths produce actionable diagnostics rather than letting the host linker fail later.

When an import bundle is provided and the platform linker or archiver succeeds, Stage-0 copies the metadata to `<output>.imports`. Dry runs report the destination path instead of touching the filesystem. Bolt archives disallow `-L`/`-l` flags—the wrapper expects every object or archive destined for the `.zap` to be listed explicitly—so the generated `llvm-ar` command remains deterministic across environments.

> **Note:** Upstream LLVM distributes the Air-capable linker as `ld.lld`. Earlier drafts referenced `link.air`, but that filename collides with the `.air` kernel artifacts described in the specification. Stage‑0 therefore resolves `ld.lld` directly; if your Air SDK exposes a renamed wrapper (for example `link.air`), create an `ld.lld` copy or symlink alongside it so the planner discovers the executable without ambiguity. Planned commands are printed when `--verbose` or `--dry-run` is provided. Stage‑0 still requires the host linker to be present on the PATH; if it is missing the wrapper reports an actionable diagnostic rather than silently succeeding.

## Next Steps

1. Expand automation to produce real executables once code generation is available (Stage-1+).
2. Auto-detect supporting runtime/object bundles for `.zap` creation (for example prebuilt runtime shards) instead of requiring callers to enumerate them manually.
3. Add integration tests that execute linked artifacts under QEMU or equivalent once backend code generation lands.


