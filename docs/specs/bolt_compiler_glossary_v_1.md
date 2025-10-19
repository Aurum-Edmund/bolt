# 🧠 Bolt Compiler Glossary and Terminology (v1.3 — Aligned with Bolt Language v2.3)

This glossary aligns the compiler’s terminology with **Bolt Language v2.3**. All identifiers are full words (no underscores or hyphens). “IR” is spelled out as **Bolt Intermediate Representation**. System‑call wording follows language spec (User System Request Entry; Kernel System Request Identifier). Attribute names mirror the language’s **bracketed attributes** (lowerCamelCase).

---

## 0) Legacy → Modern Compiler Jargon (Alignment Table)
| Legacy / Classic Term | Modern Term (Full Words) | Definition |
|---|---|---|
| IR | **Bolt Intermediate Representation** | The architecture‑neutral program form used for analysis and optimization. |
| SSA | **Static Single Assignment Form** | Each variable is assigned exactly once; enables precise analysis. |
| CFG | **Control Flow Graph** | Graph of basic blocks and their control transfers. |
| DFG | **Data Flow Graph** | Graph of value production and consumption. |
| Front end | **Front End** | Lexing, parsing, and semantic analysis stages. |
| Mid end / Middle end | **Middle End** | Optimization stages operating on Bolt Intermediate Representation. |
| Back end | **Back End** | Instruction selection, register allocation, and emission. |
| Assembler | **Assembly Emitter** | Optional human‑readable assembly output generator. |
| Obj / .o | **Object File** | Compiled unit before linking. |
| Mangling | **Name Mangling** | Encoding full symbol identity into a linkable name. |
| Demangling | **Name Demangling** | Restoring human‑readable names from mangled symbols. |
| ABI | **Application Binary Interface** | Calling convention and binary format contract for a target. |
| CSR set | **Callee‑Saved Registers** | Registers preserved across calls by the callee. |
| Caller‑saved | **Caller‑Saved Registers** | Registers the caller must preserve across a call. |
| Peephole | **Local Instruction Simplification** | Tiny, local pattern improvements after selection. |
| LTO | **Whole Program Optimization** | Optimization across module boundaries at link time. |
| PGO | **Profile‑Guided Optimization** | Uses profile data collected from executions to guide optimization. |
| DCE | **Dead Code Elimination** | Removes computations whose results are never used. |
| DSE | **Dead Store Elimination** | Removes memory writes whose values are never read. |
| CSE | **Common Subexpression Elimination** | Reuses results of identical computations. |
| LICM | **Loop Invariant Code Motion** | Hoists loop‑constant work outside loops. |
| GVN | **Global Value Numbering** | Identifies equivalences to eliminate redundant work. |
| SROA | **Scalar Replacement of Aggregates** | Breaks aggregates into scalars for optimization. |
| Mem2Reg | **Promote Memory to Registers** | Converts stack spills to registers (often builds SSA). |
| RA | **Register Allocation** | Assigns program values to hardware registers. |
| Prologue/Epilogue | **Function Prologue and Epilogue** | Generated entry and exit sequences for a function. |
| Thunk | **Shim Function** | Small adapter that changes calling convention or arguments. |
| Stub (syscall) | **User System Request Entry** | User‑side entry that prepares registers and enters the System Request Gateway. |
| TBAA | **Type‑Based Alias Analysis** | Uses type info to infer which pointers may alias. |
| AA | **Alias Analysis** | Determines which memory references can point to the same location. |
| SCCP | **Sparse Conditional Constant Propagation** | Propagates constants and folds conditionals using sparse analysis. |
| PIC / PIE | **Position‑Independent Code / Position‑Independent Executable** | Code or executable that can run at any address. |
| MC layer | **Machine Code Layer** | The compiler layer that models target instructions and encodings. |
| JIT | **Just‑In‑Time Compiler** | Compiles at runtime just before execution. |
| AOT | **Ahead‑Of‑Time Compiler** | Compiles before runtime into static artifacts. |

## 1) Compiler Overview
- **Bolt Compiler:** The translator that converts Bolt source into machine code or **Bolt Intermediate Representation** and then into an artifact.
- **Compilation Unit:** A single Bolt module or source file compiled prior to linking.
- **Source Tree:** The directory structure containing all modules participating in a build.
- **Artifact:** The final product of compilation (executable or library).
- **Build Target:** A configuration describing architecture, format, calling convention, and optimization options.
- **Toolchain:** The complete set of tools used for compilation (compiler, assembly emitter, linker, debugger, profiler).

## 2) Front End (Language Analysis)
- **Lexer (Lexical Analyzer):** Converts source text into tokens.
- **Token:** The smallest syntactic unit (identifier, keyword, literal, operator).
- **Parser:** Builds an **Abstract Syntax Tree** from tokens.
- **Abstract Syntax Tree:** A hierarchical representation of the program’s grammatical structure.
- **Syntax Node:** An individual element of the tree (expression, statement, type).
- **Semantic Analyzer:** Validates types, scopes, trait/interface conformance, and attributes.
- **Symbol Table:** Repository storing declared identifiers, their types, and visibility.
- **Scope Frame:** The context in which names are defined (block, function, module).
- **Type Resolver:** Matches type names and infers omitted types.
- **Constant Folder:** Evaluates constant expressions during compilation.
- **Annotation Processor:** Interprets **bracketed attributes** such as `interruptHandler`, `bareFunction`, `inSection`, `aligned`, `pageAligned`, `packed`, `bits`, `systemRequest`, and `intrinsic`.
- **Diagnostic Reporter:** Collects and formats syntax or semantic errors and warnings.

## 3) Middle End (Optimization and Bolt Intermediate Representation)
- **Bolt Intermediate Representation (program graph):** Architecture‑neutral form used for analysis and optimization.
- **Control Flow Graph:** A graph describing all possible execution paths.
- **Data Flow Graph:** A graph describing how values are produced and consumed.
- **Static Single Assignment Form:** A property where each variable is written exactly once; enables strong analysis (uses Phi nodes to merge values).
- **Optimization Pass:** A transformation that improves performance or size without changing behavior.
- **Validation Pass:** Verifies representation integrity after each transformation.
- **Pass Manager:** Coordinates the order and conditions for optimization passes.

### Core Optimization Passes (compiler default set)
- **Constant Folding** and **Constant Propagation**
- **Algebraic Simplification**
- **Common Subexpression Elimination**
- **Copy Propagation**
- **Dead Code Elimination** and **Dead Store Elimination**
- **Loop Invariant Code Motion** and **Loop Unrolling** (where profitable)
- **Strength Reduction**
- **Inline Expansion** (guided by a heuristic)
- **Escape Analysis** (stack promotion when safe)

## 4) Back End (Emission, Linking, and Layout)
- **Code Generator:** Converts Bolt Intermediate Representation into machine instructions.
- **Instruction Selector:** Chooses concrete target instructions for representation operations.
- **Register Allocator:** Assigns program values to physical registers.
- **Memory Layout Manager:** Determines stack frames, global variables, and data segment layout.
- **Object Emitter:** Writes compiled code and metadata into an object file.
- **Assembly Emitter (optional):** Emits human‑readable assembly for inspection.
- **Relocation Entry:** Placeholder requiring address fix‑ups at link or load time.
- **Linker:** Combines object files and libraries into a single artifact.
- **Symbol Resolver:** Matches unresolved references between modules.
- **Section Table:** A map of code and data regions within the final artifact.
- **Relocation Resolver:** Computes final addresses and offsets for the artifact image.

## 5) System Request Integration (Kernel Interoperation)
- **User System Request Entry:** A user‑side function that prepares registers and arguments and enters the System Request Gateway.
- **Kernel System Request Identifier:** The numeric identifier selecting a kernel System Request handler.
- **System Request Gateway:** The transition point that validates and dispatches a system request into the kernel.
- **Calling Convention Manager:** Centralizes target calling conventions for default functions, `interruptHandler`, `bareFunction`, and System Request entries.

## 6) Concurrency and Memory Model (Compiler Semantics)
- **Atomic Ordering:** Supported orders are `relaxed`, `acquire`, `release`, `acquireRelease`, and `sequentiallyConsistent`.
- **Memory Fence:** The intrinsic `memoryFence(order)` creates an ordering boundary that may not be removed or crossed by the optimizer.
- **Lock‑Free Widths:** One, two, four, and eight byte atomics are lock‑free on x86‑64.
- **Live Value Semantics:** Loads and stores to Live Value memory are side‑effecting and must not be elided or merged across fences.
- **Packed Layout Rules:** Fields in `packed` blueprints may not be reordered; program order must be preserved for memory‑mapped input/output.

## 7) Profiles, Policies, and Build Integration
- **Freestanding Profile:** Enables `--freestanding --no-runtime --no-stdlib`, disables exceptions, uses `panic abort`, and enforces the **no hidden allocation** rule.
- **Bounds Check Policy:** Enabled in debug builds and typically disabled in release builds.
- **No Hidden Allocation Verifier:** A compile‑time pass that fails the build if any kernel path would allocate memory implicitly.
- **Deterministic Mode:** Ensures reproducible builds with stable ordering and timestamps.

## 8) Diagnostics and Developer Tools
- **Diagnostic Engine:** Central service for warnings, errors, and notes.
- **Diagnostic Code:** Stable identifier for each diagnostic (for example, BOLT‑E0001).
- **Primary and Secondary Spans:** Source ranges that highlight the main and related locations.
- **Fix‑It Hint:** A machine‑suggested source edit to resolve an issue.
- **Debug Information:** Metadata describing how source constructs map to machine code for debugging tools.
- **Profile Data:** Runtime feedback stored for later optimization passes.
- **Performance Analyzer:** Tooling that measures compilation time and optimization effectiveness.

---

# Complete Definition List (every compiler term and what it does)

### Program Representation
- **Bolt Intermediate Representation:** The architecture‑neutral program form used for analysis and optimization; flows through the middle end to the back end.
- **Control Flow Graph:** The directed graph that connects basic blocks to represent all possible execution paths.
- **Data Flow Graph:** The relation between producing and consuming operations for values within a function.
- **Static Single Assignment Form:** A representation constraint where each variable is assigned exactly once, enabling precise optimizations; merges use Phi nodes.
- **Phi Node:** An operation that selects a value based on the predecessor block in Static Single Assignment form.

### Optimization and Analysis
- **Optimization Pass:** A transformation that modifies Bolt Intermediate Representation to improve performance or size without changing behavior.
- **Validation Pass:** A check ensuring that the representation remains well‑formed after transformations.
- **Pass Manager:** The coordinator that schedules and configures optimization passes.

### Code Generation and Linking
- **Code Generator:** The stage that maps Bolt Intermediate Representation to concrete machine instructions.
- **Instruction Selector:** The component that chooses a target instruction sequence for each representation operation.
- **Register Allocator:** The algorithm that maps program values to a limited number of hardware registers.
- **Memory Layout Manager:** The component responsible for stack and data segment layout.
- **Object Emitter:** The writer that produces object files containing code, data, and metadata.
- **Assembly Emitter:** A human‑readable output generator for the compiled program.
- **Relocation Entry:** A placeholder in code or data that requires address fix‑up by the linker or loader.
- **Linker:** The tool that combines object files and libraries into the final artifact.
- **Relocation Resolver:** The step that fixes addresses and offsets after linking.

### System Request Integration
- **User System Request Entry:** The user‑side function that loads the Kernel System Request Identifier and arguments into registers and performs the system request transition.
- **Kernel System Request Identifier:** The number that identifies a specific kernel service in the System Request Gateway.
- **System Request Gateway:** The boundary where the operating system validates and dispatches a system request into the kernel.
- **Calling Convention Manager:** The policy keeper that defines registers, stack alignment, callee and caller saved sets, and special entry forms (interrupt service routine and bare function).

### Concurrency and Memory Semantics
- **Atomic Ordering:** The contract that constrains how memory operations may appear to interleave across threads; supported orders are relaxed, acquire, release, acquireRelease, and sequentiallyConsistent.
- **Memory Fence:** A compiler intrinsic that prevents reordering of memory operations across the fence in the chosen ordering.
- **Live Value Semantics:** The rule that reads and writes marked as Live Value are side‑effecting and must not be removed or combined.
- **Packed Layout Rules:** The rule that field order and exact bit‑width in a packed blueprint are preserved; required for correct memory‑mapped input/output.

### Profiles, Policies, and Tooling
- **Freestanding Profile:** A build configuration that removes hosted assumptions, disables exceptions, uses panic abort, and forbids implicit allocation.
- **No Hidden Allocation Verifier:** A compiler pass that guarantees no implicit allocation occurs in kernel‑profiled code.
- **Bounds Check Policy:** The configuration that controls runtime bounds checks (typically on in debug, off in release).
- **Deterministic Mode:** A build mode that ensures reproducible outputs for identical inputs.
- **Diagnostic Engine:** The system that standardizes error and warning reporting with codes, spans, and fix‑it hints.
- **Debug Information:** Symbol and location metadata that allows source‑level debugging.
- **Profile Data:** Measurements from running programs that guide later optimization.
- **Performance Analyzer:** A tool that measures and reports compilation cost and pass impact.

---

**Status:** Glossary aligned with **Bolt Language v2.3**. All terms use full words. The compiler glossary is ready to hand to Codex alongside the language spec.

