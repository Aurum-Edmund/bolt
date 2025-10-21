# Bolt Compiler Task Board (Stage-1)

---

## Scope
- Extend the high-level IR type system (algebraic data types, generics, references) and carry metadata into MIR.
- Stand up backend scaffolding including instruction selection tables and a linear-scan register allocator prototype.
- Deliver freestanding runtime stubs and Air ABI shims capable of booting Stage-1 images through `bolt-ld`.
- Expand the diagnostic surface (negative parser/binder suites, MIR/golden deltas) to harden the pipeline.

---

## Kanban

### To Do
- High-level IR type system expansion (algebraic data types, generics, references).
- Backend scaffolding: instruction selection tables and linear scan register allocator.
- Runtime stubs plus Air ABI shims for freestanding x86-64 builds.
- Broaden golden and negative diagnostic suites.

### Doing
- None yet; Stage-1 execution not started.

### Done
- Stage-0 deliverables archived; SSA conversion, linker automation, and runtime helpers form the foundation for Stage-1.

---

## Milestones
- **S1-M0 (Planned)** Extend type system coverage and propagate metadata into MIR.
- **S1-M1 (Planned)** Establish backend scaffolding (instruction selection tables, allocator prototype).
- **S1-M2 (Planned)** Ship freestanding runtime/linker capable of producing bootable Air images.

---

## References
- Stage-1 context log: `../Context/stage_1_context_log.md`.
- Stage-0 archive: `../Context/archive/stage_0_context_log.md`.

---

## Today's Focus

Stage-1 kick-off pending. Prioritise defining type-system design tasks and backend scaffolding spikes before deep implementation.

**Notes:**
- Carry over outstanding suggestions (additional fix-it hints, parser recovery improvements) from Stage-0 once prioritised.
- Review runtime plan to scope remaining freestanding requirements before scheduling implementation work.
