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
- Live enforcement pass now enforces baseline invariants for Live-qualified functions, reports structured diagnostics (`BOLT-E4101`), surfaces them through the driver, and validates that Live-qualified basic blocks end with terminators.
- Front-end parser sources normalized to Unix newlines to eliminate stray include warnings during builds.

## Progress Metric
- **Estimated Stage-0 completion:** ~55?%

## Pending Tasks
- Execute runtime/linker implementation plan (stub APIs, helper implementation, bolt-ld integration, automation).
- Expand MIR Live enforcement beyond baseline structural checks and implement SSA conversion passes.

## Notes
- Use the task board (`../Taskboard/bolt_compiler_task_board_stub_pack_v_0_kickstart.md`) to log task state and checklist progress.
- Keep this context log updated when significant milestones or decisions occur to support future chat continuity.
- All identifiers must follow Bolt naming rules (full words, lowerCamelCase attributes). Tests should run via `cmake --preset build-windows-release` and `ctest -C Debug`.
- Milestone M1 is **not yet complete**; dependencies for module discovery/resolution remain outstanding.
- Suggestion tracked: add compiler fix-it hints for missing semicolons to ease developer workflow.
- Repository housekeeping: consolidated local branches so `master` remains the primary tracked branch.

Refer to the [Bolt Compiler Task Board (Stage-0)](../Taskboard/bolt_compiler_task_board_stub_pack_v_0_kickstart.md) for detailed Kanban status, milestones, and supporting notes.
