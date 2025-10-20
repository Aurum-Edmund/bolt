# ⚡ Bolt Specification v2.3 — Kernel and Compiler Unification

> Canonical unified specification for the Bolt language, kernel runtime, and compiler toolchain. Fully aligned with **Bolt Language Glossary v2.3** and **Bolt Compiler Glossary v1.3**.

───────────────────────────────
## Part I Language Definition (v2.3)
───────────────────────────────

### 1. Core Philosophy
- Every Bolt construct mirrors a cognitive operation: definition (Blueprint), data (mutable/fixed/constant), and behavior (function).
- Full-word syntax only; clarity and determinism outweigh brevity.
- No underscores or hyphens in identifiers. lowerCamelCase for attributes.

### 2. Core Constructs
| Concept | Keyword | Purpose |
|----------|----------|----------|
| Data Container | **Blueprint** | Declares structured data with fields and metadata. |
| Behavior | **function** | Defines executable logic. |
| Constant | **constant** | Declares compile-time immutable value. |
| Mutable Variable | **mutable** | Declares runtime variable subject to change. |
| Fixed Variable | **fixed** | Variable that becomes immutable after first assignment. |
| Interface | **interface** | Method-only contract. |
| Enumeration | **enumeration** | Finite set of variants. |
| Alias | **alias** | Alternate name for an existing type. |

### Numeric Type Aliases
- `integer` is the default signed thirty-two bit scalar (alias for `integer32`).
- `float` maps to `float32` and `double` maps to `float64`.
- Explicit width forms (`integer16`, `integer64`, `float32`, `float64`, and so on) remain available when precise sizing matters.
- Tooling canonicalizes parameters and fields using the alias form and prints them as `<type> <name>` unless an explicit width is requested, matching the source declaration order (`integer value`).

### 3. Attributes
Attributes precede declarations using bracketed syntax. Multiple lines allowed.
```bolt
[interruptHandler]
function timerInterrupt() { /* ... */ }

[bareFunction]
[inSection(".text.boot"), aligned(4096)]
function earlyEntry() { /* startup */ }

[packed]
blueprint RegisterBlock {
    [bits(1)] unsignedInteger enable
    [bits(31)] unsignedInteger reserved
}
```
**Supported attributes:** interruptHandler, bareFunction, inSection(name), aligned(bytes), pageAligned, packed, bits(width), systemRequest(identifier), intrinsic(name)

### 4. Memory and Concurrency
- **Live Value:** loads and stores are side-effecting.
- **Atomic Ordering:** relaxed, acquire, release, acquireRelease, sequentiallyConsistent.
- **Mutual Lock**, **Access Counter**, **Wake Signal**, **Sleep List** define synchronization primitives.

### 5. Error and Runtime Policy
- No exceptions; panic abort only.
- Assertions trap in debug; removed in release.
- No hidden allocations allowed in kernel or freestanding builds.

### 6. Profiles
- **freestanding:** --no-runtime --no-stdlib --panic=abort
- **hosted:** allows runtime, reflection, exceptions.

───────────────────────────────
## Part II Runtime / Kernel Core
───────────────────────────────

### 1. Boot Chain
1. Firmware Seed → minimal AIR interpreter.
2. Perplexity Loader → verifies and loads **Bolt Core Seed Pack**.
3. Core Seed Pack → brings up memory, scheduler, console, relay.
4. Kernel Base → file system, Atlas Interlink, diagnostics.

### 2. AIR Bytecode (Full-Word Instructions)
All operations use full names.
```
Move, Load, Store, Add, Subtract, Multiply, Divide,
BitwiseAnd, BitwiseOr, BitwiseXor, BitwiseNot,
Compare, TransferControl, BranchIfZero, BranchIfNotZero,
BranchIfGreater, BranchIfLess, BranchIfGreaterOrEqual,
BranchIfLessOrEqual, Call, Return, SystemCall.
```

### 3. Memory Model
- Dimensions represent logical memory spaces.
- Region-based allocation; no garbage collection.
- Handles use capability tokens for access.

### 4. Scheduler
- Cooperative microthreads with run-to-yield semantics.
- Stable round-robin ordering.
- Saves ArgumentRegisters, ScratchRegister, LinkRegister.

### 5. System Request Integration
- **User System Request Entry**: prepares registers and executes transition.
- **Kernel System Request Identifier**: numeric code selecting kernel handler.
- **System Request Gateway**: verifies and dispatches request.

───────────────────────────────
## Part III Compiler and Toolchain
───────────────────────────────

### 1. Pipeline Overview
1. Lexing → Parsing → Abstract Syntax Tree → Semantic Analysis.
2. Bolt Intermediate Representation (Bolt IR) generation.
3. Optimization passes (deterministic).
4. Register allocation and ABI enforcement.
5. AIR emission with full-word opcodes.
6. Package build (.bpkg).

### 2. Front End
- Lexer: converts text to tokens.
- Parser: constructs AST.
- Semantic Analyzer: validates names, types, and attributes.
- Annotation Processor: interprets bracketed attributes.

### 3. Middle End
- Bolt Intermediate Representation: architecture-neutral graph.
- Control Flow Graph and Data Flow Graph models.
- Optimizations: Constant Folding, Copy Propagation, Dead Code/Store Elimination, Strength Reduction, Inline Expansion.

### 4. Back End
- Code Generator → Instruction Selector → Register Allocator → Object Emitter.
- Memory Layout Manager controls stack and globals.
- AIR emission obeys calling convention and determinism policy.

### 5. Diagnostics
- Stable error codes (BOLT-E0001 etc.).
- Primary and secondary source spans.
- Fix-It Hints suggest minimal changes.

### 6. Self-Hosting Path
Stage A: host bootstrap.
Stage B: partial reimplementation in Bolt.
Stage C: full Bolt compiler compiles itself.
Stage D: native optimizer and linker.

───────────────────────────────
## Part IV Conformance and Examples
───────────────────────────────

### Example Blueprint
```bolt
Blueprint Engine {
    mutable Integer rpm = 0
    constant Integer maxRpm = 9000

    function start() {
        Console.printLine("Engine started.")
        rpm = 800
    }

    function rev(Integer increase) {
        rpm = Math.clamp(rpm + increase, 0, maxRpm)
    }
}
```

### Example System Request
```bolt
[systemRequest(identifier=1)]
integer function systemWrite(integer fd, &byte buffer, unsignedInteger length)
```

───────────────────────────────
## Part V Reference Glossary (Extract)
───────────────────────────────

**Blueprint** — Structured definition of data and associated functions.
**function** — Executable behavior unit.
**constant / mutable / fixed** — Compile-time or runtime variable categories.
**attribute** — Declarative modifier applied via brackets.
**Bolt Intermediate Representation** — Core architecture-neutral program form.
**System Request Gateway** — Transition boundary between user and kernel.
**Live Value** — Memory qualifier ensuring side-effect visibility.
**freestanding profile** — Build mode without runtime or implicit allocation.
**packed** — Layout directive removing padding between fields.

───────────────────────────────
**Status:** Canonical, kernel-ready specification. All terms, syntax, and compiler semantics align with Bolt Language v2.3 and Compiler Glossary v1.3.




