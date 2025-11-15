# Stage-1 Context Log

**Last updated:** 2025-11-10
**Session lead:** Codex (GPT-5)

---

## Current Milestone
- **S1-M0 (Planning):** Expand the high-level IR type system and propagate metadata into MIR to unblock backend work.

## Progress Summary
- Stage-0 deliverables archived; SSA conversion, linker automation, and runtime helpers are available as a foundation for Stage-1.
- Task board for Stage-1 created to capture type-system, backend scaffolding, runtime, and diagnostic expansion workstreams.
- Extended HIR type parsing to preserve nested array metadata and verified MIR lowering keeps the structure intact via new unit coverage.
- Added qualifier-aware type parsing so constant-qualified pointer and blueprint metadata survive binder capture and MIR lowering.
- Renamed the Bolt qualifier keyword to the full word `constant`, updating parsing, metadata propagation, and unit coverage.
- Hardened frontend coverage for the `constant` keyword with lexer and parser regression tests that pin the qualifier spelling in type-first syntax.
- Cleared remaining documentation references to the abbreviated qualifier so the language glossary now presents the canonical spelling exclusively.
- Expanded frontend, binder, and MIR regression suites to cover `constant` qualifiers on fixed-length arrays and pointer-to-array signatures.
- Hardened the binder by rejecting duplicate `constant` qualifiers and added regression coverage to guard the new validation.
- Added a dedicated binder diagnostic for repeated qualifiers so duplicate `constant` usage surfaces as BOLT-E2301 with a targeted message.
- Introduced an explicit binder diagnostic (BOLT-E2302) for unknown type qualifiers to enforce the canonical `constant` spelling.
- Narrowed the legacy qualifier guard so standalone `const` tokens trigger BOLT-E2302 while allowing type names that begin with the prefix, and added binder coverage for the acceptance case.
- Enhanced the legacy qualifier diagnostic to recommend the canonical `constant` keyword so developers receive a guided fix.
- Added a binder regression for postfix qualifier mistakes and introduced diagnostic BOLT-E2303 so trailing `constant` usage points developers to prefix the keyword.
- Extended qualifier validation to nested generic arguments so postfix `constant` tokens inside angle brackets also raise BOLT-E2303 with guided messaging.
- Hardened `live` qualifier parsing so duplicate prefixes emit diagnostic BOLT-E2218 while preserving binder recovery for the underlying type metadata.
- Added binder enforcement for misplaced `live` tokens so trailing or nested occurrences raise BOLT-E2219 and are stripped before type metadata is captured.

## Progress Metric
- **Estimated Stage-1 completion:** 19%

## Pending Tasks
- Prioritise high-level IR type-system expansion items for implementation order.
- Outline backend scaffolding spikes (instruction selection tables, allocator prototype) and resource owners.
- Refine runtime/linker milestones for freestanding image production, leveraging Stage-0 automation.
- Identify diagnostic gaps to grow parser/binder negative suites and MIR goldens.

## Notes
- Stage-0 context retained under `Context/archive/stage_0_context_log.md` for historical reference.
- Task tracking for Stage-1 lives in `../Taskboard/bolt_compiler_task_board_stage1.md`.
- Continue enforcing Bolt naming rules (full words, lowerCamelCase attributes) and run tests with `cmake --preset build-windows-release` / `ctest -C Debug` when applicable.
