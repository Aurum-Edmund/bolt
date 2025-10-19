# Bolt — Language and Compiler (Stage-0)

**Status:** Active development (kernel-first).  
**Specs:** v2.3 Language / v3.1 Master.  
**Targets:** x86-64 (freestanding, PE/COFF).

## What is Bolt?
Bolt is a full-word, kernel-safe systems language and toolchain designed for the Air Kernel. Stage-0 focuses on front-end bring-up: lexing, parsing, semantic binding, and a stub MIR layer that prepares for code generation. The toolchain enforces deterministic, allocation-free behaviour for freestanding builds and uses bracketed attributes for low-level control.

## Quick Start (Windows)
1. Install Visual Studio 2022 with the "Desktop development with C++" workload and CMake 3.26+.
2. Install LLVM 17+ (MC, Object, Support libraries) and add it to `PATH` if needed.
3. Configure the project:
   ```powershell
   cmake --preset windows-release
   ```
4. Build the compiler driver:
   ```powershell
   cmake --build --preset build-windows-release --target boltcc
   ```
5. Run a smoke test (lex + parse + bind + MIR lowering + verification + debug dump):
   ```powershell
   build/windows-release/compiler/driver/Debug/boltcc.exe tests/blueprint_sample.bolt --emit=obj --target=x64-freestanding
   ```

## Repository Layout
```
/bolt
  /compiler
    /frontend          # lexing, parsing, tokens
    /high_level_ir     # binder and HIR data structures
    /middle_ir         # MIR scaffolding, lowering, verification
    /low_level_ir      # forthcoming LIR modelling (machine-aware)
    /backend           # backend passes (selection, allocation)
    /linker            # bolt-ld wrapper and image scripts
    /logging           # shared logger integration
    /language_server   # boltlsp implementation
    /driver            # boltcc entry point
    /targets           # architecture-specific configuration
  /docs               # roadmaps, specs
  /tests              # sample sources and smoke cases
  /cmake              # toolchain files and presets
```

## Roadmap
- Stage-0: complete MIR lowering, introduce MIR verifier, and prepare LIR scaffolding.
- Stage-1: instruction selection, x64 ABI integration, and flat binary emission.
- Stage-2: linker wrapper, bootable images, and kernel runtime bindings.

## Contributing
Use short-lived feature branches. Adhere to the language glossary (full words, no underscores) and keep freestanding paths allocation-free. Run the smoke test before opening a pull request.

## License
MIT License. See [LICENSE](LICENSE).




