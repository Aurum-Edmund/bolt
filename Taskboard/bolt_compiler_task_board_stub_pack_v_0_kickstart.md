# Bolt Compiler Task Board (Stage-0)

---

## Scope
- Complete the Stage-0 front-end and middle-IR pipeline with canonical module metadata.
- Carry import information from parsing through resolution into MIR and driver diagnostics.
- Stay aligned with Bolt Language v2.3 and Master Specification v3.1 while iterating.

---

## Kanban

### To Do
- High-level IR type system expansion (algebraic data types, generics, references).
- Middle-IR SSA construction and live enforcement passes.
- Backend scaffolding: instruction selection tables and linear scan register allocator.
- Runtime stubs plus Air ABI shims for freestanding x86-64 builds.
- Broaden golden and negative diagnostic suites.

### Doing
- Import resolver integration (canonical module paths, pending-resolution reporting).
- Middle-IR metadata expansion and canonical hashing updates.
- Parser diagnostic coverage and recovery polish.

### Done
- Lexer with diagnostics.
- Attribute-aware parser for modules, blueprints, and functions.
- High-level IR binder with attribute validation, live handling, and duplicate checks.
- Import capture and duplicate diagnostics in AST/HIR.
- Middle-IR lowering, printer, canonical hash, and verification skeleton.
- Driver CLI with MIR dump, canonical hash emission, and resolver wiring.
- Unit and golden tests covering front end, HIR binder, MIR lowering, and canonical output.
- MIR lowering and canonicalization emit type-first parameter and field descriptors with default scalar aliases (e.g., `integer value`).
- Parser accepts type-first source syntax with return types preceding `function`, keeping source and MIR canonical formats consistent.
- Parser sources normalized to consistent Unix newlines to keep builds warning-free across toolchains.
- Runtime panic helper and freestanding entry point now reuse the shared `BOLT_NORETURN` definition to avoid duplicate macros after conflict merges.
- Freestanding `_start` entry is now opt-in via `BOLT_RUNTIME_INCLUDE_FREESTANDING_START`, preventing conflicts when host tooling links against runtime helpers.
- Runtime memory copy/fill helpers now ship with unit coverage validating nominal and zero-length behavior.
- Driver CLI accepts `--import-root` entries and search roots flow into module locator (unit tested).
- Linker wrapper exposes structured CLI parsing (emit kind, target, sysroot, runtime roots) with dedicated unit coverage.
- Linker wrapper materialises Windows command plans (link.exe) and reports missing host linkers with actionable diagnostics.
- Linker wrapper now produces Windows static library plans via `lib.exe`, enabling archive packaging for Stage-0 builds.
- Linker wrapper now materialises Air (`ld.lld`) command plans, validating linker scripts and freestanding entry configuration while avoiding conflicts with `.air` kernel artifact naming.
- Linker documentation clarifies that Stage-0 resolves `ld.lld` directly and records guidance for SDKs that ship alternate wrapper names so environments stay compatible.
- Linker wrapper validates linker scripts, import bundles, runtime roots, and object inputs ahead of invocation, and stages import bundles to `<output>.imports` after successful links (dry runs report the planned destination).
- Linker wrapper now assembles Bolt archives with `llvm-ar`, rejects `-L`/`-l` flags for deterministic `.zap` creation, and includes unit coverage for the new planner and validation behaviour.
- Module locator scans import roots, caches discovered modules, and reports `BOLT-E2225`/`BOLT-E2226` diagnostics for missing roots or duplicate modules before import resolution, with unit coverage locking down discovery and error handling.
- Parser, binder, and MIR tests exercise `link` functions across modules with multiple blueprints and assert blueprint field metadata so the static replacement modifier stays regression-safe.
- Parser emits fix-it hints for missing semicolons on package/module/import declarations and the driver reports the suggested edits alongside parser diagnostics.
- Linker CLI auto-selects the Air triple for `--emit=air`/`--emit=zap` when no target is provided and rejects incompatible emit/target combinations with explicit diagnostics.
- Driver emits JSON import bundles via `--emit-import-bundle`, enforcing single-module usage and covering the manifest format in unit tests.
- Linker CLI exposes `--entry` overrides, passing custom entry symbols through Windows (`/ENTRY`) and Air (`-e`) command planners with documentation and unit tests updated accordingly.
- Linker CLI rejects entry overrides for static libraries or Bolt archives so the flag never silently applies to unsupported emit kinds.
- Repository housekeeping keeps the local `master` branch aligned with the active Stage-0 work tip to avoid divergence during follow-up sessions.
- Linker CLI honors explicit `--linker`/`--archiver` overrides, validates the overridden executables, and covers the behavior with planner and CLI unit tests.
- Linker wrapper requires runtime roots for Air images, auto-injects the Bolt runtime archive for Air/Windows executables, and reports missing runtime libraries up front with dedicated unit coverage.
- Runtime root detection searches `lib/` and triple-qualified folders so packaged SDK layouts resolve Bolt runtime archives without manual intervention, with docs/tests covering the expanded lookup.
- Linker CLI now honors `--no-runtime`, skipping runtime archive validation/injection for executables and Air images while planners, validators, and docs capture the override behavior.
- Runtime library now ships atomic load/store/exchange/compare-exchange helpers for 8-bit, 16-bit, 32-bit, and 64-bit values with cross-platform unit coverage, paving the way for MIR SSA and live passes that require atomic intrinsics.
- Runtime atomic helpers now include fetch-add and fetch-sub operations for all integer widths, with unit coverage keeping the expanded intrinsic surface stable ahead of MIR SSA work.
- Runtime atomic helpers now include fetch-and/fetch-or/fetch-xor operations across all integer widths with dedicated unit tests so the bitwise atomic surface stays stable for upcoming MIR lowering.
- Linker CLI now falls back to `BOLT_SYSROOT`/`BOLT_RUNTIME_ROOT` environment defaults when the corresponding flags are omitted, keeping scripted builds ergonomic without overriding explicit command-line configuration.
- Linker CLI and planner now honor `--map`, validating destination directories and forwarding map generation to Windows (`/MAP`) and Air (`--Map=`) toolchains with fresh documentation and unit coverage.
- Import resolver now threads canonical module paths through MIR resolved-import metadata, driver notices, and JSON import bundles with dedicated regression coverage guarding the canonical flow.
- MIR pass library exposes a control-flow graph builder that records block predecessors and successors from terminator metadata, with linear and branching unit tests priming SSA construction.
- MIR pass library now ships a dominator tree builder with immediate-dominator edges, dominance queries, and regression tests to unblock the SSA conversion pass stack.

---

## Milestones
- **M0 (Completed)** Stage-0 lexical analyser, parser, and binder pipeline online.
- **M1 (Active)** Import resolution framework feeding canonical MIR metadata.
- **M2 (Planned)** Middle-IR SSA, live enforcement, and diagnostic passes.
- **M3 (Planned)** Backend scaffolding with instruction selection and register allocation seeds.
- **M4 (Planned)** Runtime stubs, Air ABI shims, and freestanding image linkage.

---

## Repository Layout (trimmed)
```
bolt/
  CMakeLists.txt
  CMakePresets.json
  Taskboard/
  build/                 # generated artifacts (out of tree recommended)
  cmake/
  compiler/
    backend/
    driver/
    frontend/
    high_level_ir/
    language_server/
    linker/
    logging/
    low_level_ir/
    middle_ir/
    targets/
  docs/
  examples/
  runtime/
  stdlib/
  tests/
    golden/
    unit/
  third_party/
  tools/
```

---

## Testing Gates
- Unit tests via `bolt_unit_tests` (lexer, parser, binder, MIR lowering, canonical output).
- MIR golden comparisons (`tests/golden`, driven by `RunMirGolden.cmake`).
- Canonical MIR hash emission for determinism checks (`--show-mir-hash` / `--emit-mir-canonical`).

---

## Today's Focus

Runtime/Linker Acceleration Plan: finalize stub APIs, implement helpers, integrate bolt-ld wrapper, add automation.
1. Outline MIR SSA and live enforcement plan for Stage-0.
2. Define initial backend scaffolding milestones (instruction selection, register allocation).
3. Map runtime stub requirements to upcoming build steps.
4. Execute runtime/linker implementation plan (stub APIs, helper implementation, bolt-ld integration, automation).

**Notes:**
- MIR SSA plan: start with pruned IDOM construction, insert Phi nodes, enforce live barriers before lowering to back end.
- live enforcement: dedicate MIR pass to ensure side-effect ordering and fence handling prior to SSA conversion.
- Backend scaffolding: define instruction selection tables, linear scan register allocator, and object emission stub.
- Runtime stubs: `_start` entry, panic abort routine, memory copy and memory fill helpers, atomic intrinsics aligned with Air ABI shims.
- Linker integration: emit `.exe` (Windows), `.air` (Air executable), and link archives (`.lib` for Windows, `.zap` for Air/Bolt), then bundle runtime stubs plus resolved import metadata.
- Runtime/linker execution plan: finalize stub APIs, implement helpers, integrate bolt-ld wrapper, add automation, and document `link` keyword usage.
- Suggestion: add compiler fix-it hints for missing semicolons to reduce developer friction when exhaustion strikes.

## Task Log
- All Stage-0 task log items completed; future tasks tracked in the Kanban lists above.
