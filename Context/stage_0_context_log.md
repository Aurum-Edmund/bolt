# Stage-0 Context Log

**Last updated:** 2025-10-20  
**Session lead:** Codex (GPT-5)

---

## Current Milestone
- **M1 (In Progress):** Import resolution framework feeding canonical MIR metadata.

## Progress Summary
- Front end captures imports in AST/HIR with duplicate diagnostics (`BOLT-E2218`).
- Import resolver now supports self-import detection, optional module locator lookup, and missing-import diagnostics (`BOLT-E2219`, `BOLT-E2220`).
- Module locator interface added with search roots and manual registration for driver configuration.
- MIR modules carry package names, canonical module paths, import lists, and resolved import metadata; printer and canonical hash updated accordingly.
- Driver integrates the locator and resolver, reporting resolved imports and canonical module paths during MIR verification.
- Tests updated: importer unit tests (including locator coverage), MIR canonical expectations, golden MIR snapshot, full unit + golden suite passing.
- Task board refreshed with Stage-0 focus, Kanban columns, and task checklist.
- Runtime stub and linker integration requirements captured in task board notes (entry, panic, memory helpers, atomic intrinsics, linker script workflow).
- Example entry functions now use `start`, and language keywords now include `link` for module-scoped declarations along with updated artifact terminology.
- Drafted detailed documentation for linker wrapper responsibilities and runtime stub APIs (`compiler/linker/linker.md`, `runtime/runtime_stub_plan.md`) to accelerate implementation.
- Runtime library skeleton (`runtime/runtime.h`, `runtime/runtime.c`) and linker wrapper stub (`compiler/linker/bolt_ld_main.cpp`) added and wired into the build system.
- MIR lowering and canonical output now emit type-first parameter and field descriptors (`integer value`) to align with spec naming rules.
- Scalar type aliases normalized: default integer/float/double names now map to integer32/float32/float64 under the hood, and canonical output reflects the shorthand.
- Language specification and glossary updated to document numeric aliases and canonical parameter ordering in MIR output.
- Parser updated to require type-first parameters/fields and return types ahead of `function`, aligning source syntax with canonical MIR output.
- Added parser regression tests that reject legacy colon-style parameters and blueprint fields to prevent syntax regressions.
- Driver CLI now accepts repeatable `--import-root` flags; module locator uses them and coverage added in unit tests.
- Runtime stub plan expanded with `_start` flow, testing strategy, and linker integration notes to guide upcoming implementation.
- live enforcement pass now enforces baseline invariants for live-qualified functions, reports structured diagnostics (`BOLT-E4101`), surfaces them through the driver, and validates that live-qualified basic blocks end with terminators.
- Front-end parser sources normalized to Unix newlines to eliminate stray include warnings during builds.
- Runtime panic/entry helpers share the common `BOLT_NORETURN` macro from `runtime.h`, cleaning up duplicate definitions after merge resolution.
- Freestanding `_start` entry point is now gated behind `BOLT_RUNTIME_INCLUDE_FREESTANDING_START` so host-linked tools can reuse runtime helpers without conflicting CRT entry symbols.
- Runtime memory helpers now have unit tests covering byte copy, fill, and zero-length behavior to guard the freestanding runtime contract.
- Linker wrapper now parses structured CLI options (emit kind, targets, sysroot/runtime roots) and ships with unit coverage to prepare for real platform invocations.
- Linker wrapper now plans Windows invocations (`link.exe`), surfaces the computed command line when verbose/dry-run is enabled, and reports missing host linkers with diagnostics.
- Linker wrapper now plans freestanding Air images through `ld.lld`, enforcing linker-script requirements and populating entry/runtime library parameters for the Stage-0 flow while matching the specification’s kernel artifact naming rules.
- Documented that Stage-0 now resolves `ld.lld` directly to avoid collisions with `.air` kernel artifacts, and captured guidance for SDKs that still ship an alternate wrapper name.
- Linker wrapper now synthesizes Windows static library invocations via `lib.exe`, allowing Stage-0 builds to package runtime or module archives alongside executable outputs.
- Linker wrapper now validates linker inputs (scripts, import bundles, runtime roots, search paths, objects) ahead of invocation and copies successful import bundles to `<output>.imports`, with dry runs reporting the staging path.
- Linker wrapper now assembles Bolt archives with `llvm-ar`, rejects `-L`/`-l` flags for deterministic `.zap` creation, and ships unit coverage for the new planner and validation paths.
- Parser, binder, and MIR lowering suites now cover `link` functions alongside multiple blueprints, ensuring the static replacement modifier remains stable across stages.
- Blueprint regression coverage now asserts blueprint field metadata in parser, binder, and MIR lowering tests to guard shared `link` helpers across aggregates.
- Linker CLI now defaults to `x86_64-air-bolt` when `--emit=air` or `--emit=zap` are selected without an explicit target and rejects incompatible target/emit combinations with direct diagnostics.
- Linker CLI now accepts `--entry` overrides, threading custom entry symbols through Windows (`/ENTRY`) and Air (`-e`) command plans with dedicated unit coverage and documentation updates.
- Linker CLI now rejects entry overrides for static libraries or Bolt archives, with validation and unit coverage preventing the flag from acting as a silent no-op.
- Driver now emits canonical JSON import bundles via `--emit-import-bundle`, guards single-module usage, and ships unit tests covering manifest structure and status serialization.
- Confirmed the local `master` branch points at the Stage-0 work tip so future syncs start from an aligned history without stale divergence.
- Linker CLI now accepts explicit `--linker`/`--archiver` overrides, threads them through command planning, and validates the overridden executables before launch for both Windows and Air toolchains.
- Linker wrapper now requires a runtime root for Air images, auto-detects the Bolt runtime archive, and injects it for Air and Windows executables with unit coverage guarding missing-runtime diagnostics.
- Runtime root discovery now probes `lib/` and triple-qualified subdirectories so packaged SDK layouts resolve Bolt runtime archives without extra flags, with updated unit coverage and documentation.
- Linker CLI now exposes `--no-runtime`, allowing executables and Air images to bypass Bolt runtime injection while keeping planners and validators aware of the override, with fresh unit coverage and documentation updates.
- Runtime library now provides atomic load/store/exchange/compare-exchange helpers for 8-bit, 16-bit, 32-bit, and 64-bit values with MSVC interlocked fallbacks and cross-platform unit coverage, preparing Stage-0 for future SSA and live passes that require atomic intrinsics.
- Runtime atomic helper suite now includes fetch-add and fetch-sub operations for 8-bit, 16-bit, 32-bit, and 64-bit values with unit coverage, covering the next wave of MIR atomic requirements.
- Runtime atomic helper suite now covers fetch-and, fetch-or, and fetch-xor operations for all supported widths with cross-platform unit coverage so the Stage-0 runtime exposes the full bitwise atomic set expected by upcoming MIR work.
- Module locator now scans import roots, caches discovered modules, and surfaces `BOLT-E2225`/`BOLT-E2226` diagnostics for missing roots or duplicate modules; the driver reports discoveries ahead of import resolution and new unit tests cover discovery, duplicates, and invalid roots.
- Parser now attaches fix-it hints to missing semicolon diagnostics on package/module/import declarations, and the driver surfaces the suggestions alongside parser errors.
- Linker CLI now honours `BOLT_SYSROOT` and `BOLT_RUNTIME_ROOT` defaults when explicit command-line values are absent, keeping scripted builds ergonomic without sacrificing deterministic flag handling.
- Linker CLI and planner now support `--map`, validating destinations and threading map file requests through Windows (`/MAP`) and Air (`--Map=`) invocations with expanded regression coverage.
- Import resolver now records canonical module paths for resolved imports, threads them through MIR metadata, driver notices, and JSON import bundles, and ships regression coverage to guard the canonical wiring.
- MIR pass library now includes a control-flow graph builder that captures block predecessors/successors from branch terminators, with unit tests covering linear and branching shapes to prepare SSA analysis.

## Progress Metric
- **Estimated Stage-0 completion:** ~83?%

## Pending Tasks
- Execute runtime/linker implementation plan (stub APIs, helper implementation, bolt-ld integration, automation).
- Expand MIR live enforcement beyond baseline structural checks and implement SSA conversion passes.

## Notes
- Use the task board (`../Taskboard/bolt_compiler_task_board_stub_pack_v_0_kickstart.md`) to log task state and checklist progress.
- Keep this context log updated when significant milestones or decisions occur to support future chat continuity.
- All identifiers must follow Bolt naming rules (full words, lowerCamelCase attributes). Tests should run via `cmake --preset build-windows-release` and `ctest -C Debug`.
- Milestone M1 is **not yet complete**; dependencies for module discovery/resolution remain outstanding.
- Suggestion tracked: add compiler fix-it hints for missing semicolons to ease developer workflow.
- Repository housekeeping: consolidated local branches so `master` remains the primary tracked branch.

Refer to the [Bolt Compiler Task Board (Stage-0)](../Taskboard/bolt_compiler_task_board_stub_pack_v_0_kickstart.md) for detailed Kanban status, milestones, and supporting notes.
