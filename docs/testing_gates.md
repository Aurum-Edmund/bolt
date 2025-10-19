# Testing Gates (Planned)

This document outlines the Stage-0 testing gates that must be in place before we progress to the backend milestones. Each gate is currently a roadmap item; they are not yet automated.

## 1. Golden Baselines
- Record `examples/*.bolt` sources.
- Snapshot outputs for each stage:
  - `.mir` (MIR canonical dump)
  - `.s` (assembly once available)
  - `.o` (object files)
- CI should compare current output against the golden baseline.

## 2. Negative Diagnostics
- Curate error cases for:
  - Borrow violations
  - Uninitialised usage
  - Non-exhaustive pattern matches
- Ensure each emits the documented diagnostic code and message.

## 3. Hash Stability\n- Compute a canonical hash for AST/MIR dumps.\n- Detect drift to guard against nondeterministic output.\n- Use the MIR canonical printer/hash (olt::mir::canonicalPrint, canonicalHash) to feed this gate.\n
## 4. Build & Target Matrix
- **Development build**: Windows + MSVC/LLVM toolchain, emitting COFF for quick `dumpbin` or `llvm-objdump` inspection.
- **Kernel build**: `x86_64-elf` freestanding, custom link script, no CRT.

## 5. Documentation Sync
- Language spec v2.3 semantics must match implemented behaviour.
- Compiler spec: keep pass descriptions, invariants, IR formatting, and error codes aligned with the code.
- README stays updated with Windows build steps, supported targets, examples, and known limitations.

