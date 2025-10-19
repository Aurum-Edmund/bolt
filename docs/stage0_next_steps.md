# Stage-0 Compiler Build Priorities

This checklist captures the immediate follow-up tasks after establishing the Stage-0 scaffold. The items mirror sections 4–8 of the build instructions and should be completed in order so that every layer has a stable interface before back-end work begins.

_Status update:_ The Stage-0 lexer is in place; the next milestones focus on parsing and representation construction.

1. **Front-End Foundation (compiler/front_end)**
   - **Completed:** lexical scanner with attribute-aware tokens, plus baseline parser for modules, blueprints, and functions (attributes + modifiers handled).
   - **Completed:** semantic binder for duplicate-attribute validation, symbol tables, attribute placement checks, and LiveValue/kernel marker capture.
   - Next: fold in import resolution scaffolding and start enforcing kernel profile diagnostics.
2. **High-Level IR (compiler/hir)**
   - **Completed:** foundational HIR data structures with type references, attribute-derived alignment flags, packed/bitfield metadata, and LiveValue/kernel marker flags.
   - Next: expose lowering entry points to MIR and model module-level linkage metadata.
3. **Middle Representation (compiler/mir)**
   - Model basic blocks, instruction opcodes, and effect kinds per spec §5.
   - Add a verifier that enforces single entry, explicit terminators, and LiveValue barriers.
4. **Back-End Preparation (compiler/back_end + compiler/targets/x64)**
   - Describe calling convention records (argument registers, callee preserved sets).
   - Sketch stack frame builder and object writer interface.
5. **Driver Integration**
   - Replace the current placeholder pipeline in `boltcc` with staged passes.
   - Emit diagnostics via the standard logger bridge (`compiler/log`).
6. **Tooling and Tests**
   - Populate `/tests` with lexer and parser unit tests.
   - Add CI configuration using the provided `CMakePresets.json`.

Progress through these items should stay aligned with the language and compiler glossaries to preserve terminology and attribute semantics.


