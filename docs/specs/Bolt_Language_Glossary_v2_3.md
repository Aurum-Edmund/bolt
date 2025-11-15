
# ⚡ Bolt Language Glossary and Syntax Reference (v2.3 — Air Format, Kernel-Ready)

This edition defines **bracketed attributes** (square-bracket attributes placed above declarations), locks the **Bolt Intermediate Representation** term, **bans underscores and hyphens in language identifiers**, and uses **full words** everywhere. It is the canonical reference for freestanding or kernel builds.

---

## Attribute Syntax (Formal)
**Style rule:** Language identifiers MUST avoid underscores `_` and hyphens `-`. Use single words or **lowerCamelCase** for identifiers and attributes. No brand-specific terminology is used.

**Placement:** Place attributes on the line immediately **above** a declaration (function, blueprint or field, variable). Multiple lines of attributes are allowed; order is preserved.

**Grammar (EBNF):**
```
Attributes      := { AttributeLine }
AttributeLine   := '[' Attribute { ',' Attribute } ']'
Attribute       := Identifier [ '(' ArgumentList ')' ]
ArgumentList    := Argument { ',' Argument }
Argument        := Identifier '=' Value | Value
Value           := Integer | Text | Identifier
Identifier      := letter { letter | digit }
```
**Rules:**
- Duplicate attributes are an error unless the attribute is explicitly **repeatable** (none are repeatable in v2.3).
- Unknown attributes are an error in the **freestanding** profile and a warning in the **hosted** profile.

**Supported attributes (v2.3, lowerCamelCase):**
- `interruptHandler`
- `bareFunction`
- `inSection(name: text)`
- `aligned(bytes: integer)`
- `pageAligned`
- `packed`
- `bits(width: integer)` (field only)
- `systemRequest(identifier: integer)`
- `intrinsic(name: text)`

**Attribute placement (Stage‑0 enforcement)**
| Attribute | Allowed on | Notes |
|-----------|------------|-------|
| `interruptHandler` | `function` | Kernel profile only; emits interrupt-friendly prologue/epilogue. |
| `bareFunction` | `function` | Removes generated prologue/epilogue; mutually exclusive with `interruptHandler`. |
| `inSection` | `function` | Places the symbol into a specific linker section (for example `.text.boot`). |
| `aligned` | `function` | Enforces minimum alignment (power of two). |
| `pageAligned` | `function` | Convenience shorthand for current architecture page size. |
| `systemRequest` | `function` | Marks user-to-kernel gateway entry; requires kernel profile. |
| `intrinsic` | `function` | Requests a named backend-defined intrinsic expansion. |
| `packed` | `blueprint` | Removes padding from the layout (required for MMIO device blocks). |
| `aligned` | `blueprint` | Overrides layout alignment when not using `packed`. |
| `bits` | `blueprint` field | Requires containing blueprint to be `packed`; sets exact bit width. |
| `aligned` | `blueprint` field | Overrides field-level alignment within the packed structure. |

> Stage‑0 tooling must treat any attribute outside its allowed context as an error in freestanding builds.

**Examples**
```bolt
[interruptHandler]
public integer function timerInterrupt() { /* ... */ }

[bareFunction]
[inSection(".text.boot"), aligned(4096)]
public integer function earlyEntry() { assembly { "cli"; "hlt"; } }

[systemRequest(identifier=1)]
public live integer function systemWrite(integer fileDescriptor, pointer<byte> buffer, unsigned64 length);

public live integer function example(integer value) {
    integer next = value + 1;
    return next;
}

[packed]
public blueprint UartControl {
    [bits(1)] unsignedInteger enable;
    [bits(2)] unsignedInteger parity;
    [bits(2)] unsignedInteger stopBits;
    [bits(27)] unsignedInteger reserved;
}
```

## Numeric Type Aliases
- integer is the default signed thirty-two bit scalar (alias for integer32).
## Memory Model and live value
**live value (volatile):**
- Front-end analysis records live qualifiers alongside type metadata so MIR/LIR passes can honour ordering and visibility guarantees.
- The qualifier may appear at most once before a type; repeating `live` triggers binder diagnostic `BOLT-E2218`.
- Valid surfaces include function return types, parameters, and blueprint fields; the binder enforces the same diagnostics (`BOLT-E2217`, `BOLT-E2218`, `BOLT-E2219`) across each surface and strips misplaced tokens before type metadata is captured.
loat64.
- Use explicit width forms (for example, integer16, integer64, loat32) when exact sizing or interop requirements demand it.
- Compiler diagnostics and canonical MIR output prefer the alias form unless a width-specific type is declared.
- Documentation snippets may present the shorthand public integer function sample(integer value) { ... } to illustrate the alias while the Stage-0 parser continues to accept unction sample(...) -> integer.

---

## Calling Conventions (x86-64, long-pointer sixty-four data model, kernel profile)
**Default `function` (System V AMD64):**
- **Arguments (left to right):** RDI, RSI, RDX, RCX, R8, R9, then stack.
- **Return:** RAX (and RDX for one hundred twenty-eight bit values where applicable).
- **Callee saved:** RBX, RBP, R12 to R15.
- **Caller saved:** RAX, RCX, RDX, RSI, RDI, R8 to R11.
- **Stack:** sixteen-byte aligned at call; the red zone is **disabled** in the kernel.

**`[interruptHandler]` functions:**
- Prologue and epilogue save and restore the full interrupt frame; returns with `iretq`.
- The direction flag is cleared.
- Segment state and stack alignment are restored by the epilogue.

**`[bareFunction]`:**
- No prologue or epilogue is emitted; the caller must establish alignment and save registers.

**`[systemRequest(identifier = N)]` (User System Request Entry):**
- Emits the `syscall` instruction with the **Kernel System Request Identifier** in RAX; other registers follow the kernel Application Binary Interface contract.

> Note: Register names (RAX, RDI, and so on) are hardware names and may retain uppercase letters.

---

## Memory Model and Live Value
**Atomic ordering (per operation):** `relaxed`, `acquire`, `release`, `acquireRelease`, `sequentiallyConsistent`.
- Fences: `memoryFence(order)`; the optimizer may not remove a fence or move instructions across it.
- Lock-free widths: one, two, four, and eight bytes are lock-free on x86-64.

**Live Value (volatile):**
- Loads and stores are side-effecting; they are never removed or merged across fences.
- Reordering across **packed** memory-mapped input/output blueprint fields is forbidden; the compiler must respect program order.
- Front-end analysis records Live qualifiers alongside type metadata so MIR/LIR passes can honour ordering and visibility guarantees.

---

## Link Placement and Alignment Rules
- `inSection(".name")` sets the symbol’s **primary** output section.
- `aligned(N)` enforces a minimum alignment; if `pageAligned` is also present, the **greater** alignment wins.
- Conflicts (for example, multiple `inSection` or multiple `aligned`) are errors.
- Module-wide defaults (via a build script or a profile) apply **only** when no per-symbol attribute is present.

---

## Error Handling and Runtime Policy (Kernel)
- **Exceptions:** disabled. Any `try` or `catch` is a compile-time error in the freestanding profile.
- **Panic policy:** `panic abort` — immediate trap; no unwinding.
- **Assertions:**
  - Debug kernel: trap with file, line, and message.
  - Release kernel: removed unless explicitly compiled with `--assert=keep`.
- **No hidden allocation:** Formatting, `match`, and standard helpers are allocation-free in freestanding builds. Any implicit allocation is a compile-time error.

---

## Build Profiles and Required Flags
Freestanding or kernel builds **must** enable the following flags:
```
--freestanding --no-runtime --no-stdlib \
--panic=abort --exceptions=off \
--rtti=off --reflection=off \
--bounds=off   # enable in debug with --bounds=on
```

### Smart pointers and references
- Use `Type*` to declare a managed pointer. The `*` suffix is equivalent to `pointer<Type>` and maps to a shared-ownership smart pointer implemented by the Bolt runtime. All managed pointers participate in deterministic reference counting and never perform hidden allocations—copying requires an explicit call to the runtime smart pointer helpers.
- Use `Type&` to declare a reference alias. The syntax mirrors `reference<Type>` in existing code and binds to the underlying value without taking ownership.
- Chained suffixes associate from right to left. `integer*&` denotes a `reference<pointer<integer>>`, enabling references to managed pointers without exposing raw addresses.
- Pointer validity is determined by ownership: `if (object)` checks that the managed pointer owns a payload, and `if (!object)` enters when the pointer is empty.

### Object lifecycle
- Blueprint construction and teardown use dedicated function names: the blueprint name itself (`BlueprintName`) represents the constructor and `~BlueprintName` names the destructor. Stage‑0 tooling records the identifiers alongside other function metadata.
- `new` allocates zero-initialised storage and returns a managed pointer-ready address. `delete` releases storage obtained from `new`. Both keywords are reserved in the lexer so they cannot be repurposed for identifiers.
- All automatic variables receive sane defaults; uninitialised storage is zero-filled by default so deterministic state is available before constructors run.
- Constructor parameters receive deterministic defaults when omitted: integers resolve to `0`, floating-point values resolve to `0.0`, and managed pointers resolve to `null`. Reference parameters cannot be defaulted—Stage‑0 emits `BOLT-W2210` to require an explicit argument.
- Destructors must not declare parameters. Stage‑0 emits `BOLT-E2230` if a destructor signature contains arguments.
- The runtime exposes explicit helpers for smart pointer construction (`bolt_shared_pointer_make`), copying (`bolt_shared_pointer_copy`), moving (`bolt_shared_pointer_move`), validation (`bolt_shared_pointer_is_valid`), and teardown (`bolt_shared_pointer_release`). Hidden allocations are forbidden—callers decide when to allocate and destroy.

### Operators
- Arithmetic: `+`, `-`, `*`, `/`, `%`, with compound assignment `+=` and `-=`.
- Increment and decrement: `++` and `--` (prefix and postfix are tokenised for future semantic passes).
- Comparison: `>`, `<`, `>=`, `<=`, `==`, `!=`.
- Logical: `&&` (logical AND) and `||` (reserved for future stages).

| `volatile` | **live value** | Side-effecting read or write; not optimized away. |
```bolt
profile kernelFreestanding {
    panic abort
    exceptions off
    bounds off
    rtti off
| `constant` | **constant** | Immutable compile-time value. |
}
```

---

## Core Language (Legacy → Modern → Definition)

### Declarations and Namespaces
| Legacy Term | Modern Term | Definition |
|---|---|---|
| `volatile` | **live value** | Side-effecting read or write; not optimized away. |
| `include` | **import** | Brings external symbols into scope. |
| `namespace` | **Code Namespace (Domain)** | Groups related symbols and prevents collisions. |
| `typedef` | **alias** | Alternate name for an existing type. |

### Types and Data
| Legacy Term | Modern Term | Definition |
|---|---|---|
| `struct` | **blueprint** | Data layout with named fields. |
| `enum` | **enumeration** | Discrete set of named variants. |
| interface | **interface** | Method-only type contract; no data. |

### Functions and Flow
| Legacy Term | Modern Term | Definition |
|---|---|---|
| `fn` | **function** | Executable routine with parameters and an optional return value. |
| `const` | **constant** | Immutable compile-time value. |
| `var` | **mutable** | Mutable runtime variable. |
| *(not applicable)* | **fixed** | Variable becomes immutable after the first assignment. |
| `switch` | **match** | Exhaustive pattern selection. |
| *(not applicable)* | **guard** | Early-exit precondition. |
| `goto` | **forbidden** | Replace with guards or state machines. |

### Memory and Access
| Legacy Term | Modern Term | Definition |
|---|---|---|
| pointer | **pointer** | Address of a value or a function. |
| reference | **reference** | Bound alias to an existing value. |
| `volatile` | **Live Value** | Side-effecting read or write; not optimized away. |
| page or frame | **Memory Block** | Page-sized unit for kernel-adjacent code. |

### Concurrency (Language Level)
| Legacy Term | Modern Term | Definition |
|---|---|---|
| mutex | **Mutual Lock** | Exclusive access to a critical section. |
| semaphore | **Access Counter** | Up to a fixed number of concurrent entrants. |
| spinlock | **Busy Lock** | Spin-wait lock for very small critical sections. |
| atomic operations | **Atomic Operations** | Indivisible read-modify-write with ordering. |
| condition variable | **Wake Signal** | Pairs with **Sleep List** to resume waiters. |
| wait queue | **Sleep List** | Threads waiting on a condition. |

### Kernel Interoperation (System Requests)
| Legacy Term | Modern Term | Definition |
|---|---|---|
| syscall | **system request** | User to kernel boundary call. |
| syscall number | **Kernel System Request Identifier** | Numeric selector in the kernel table. |
| syscall stub | **User System Request Entry** | A small user-side function that prepares registers and arguments and enters the System Request Gateway. |

### Build and Compilation (Documentation)
| Legacy Term | Modern Term | Definition |
|---|---|---|
| compiler | **compiler** | Translates source to machine code or **Bolt Intermediate Representation**. |
| linker | **linker** | Combines objects into the final artifact. |
| object file | **object file** | Compiled unit before linking. |
| build script | **build script** | Steps and flags to produce artifacts. |
| manifest | **run descriptor** | Entry point, permissions, and dependencies. |

public live integer function systemWrite(integer fileDescriptor, pointer<byte> buffer, unsigned64 length)
- **live value** — A qualifier that forces loads and stores to be side-effecting and visible to hardware or other agents.
|---|---|---|
| Lexical Analysis | **Lexical Analysis** | Converts source to tokens. |
| Parsing | **Parsing** | Builds the Abstract Syntax Tree. |
| Semantic Analysis | **Semantic Analysis** | Validates types, names, and scopes. |
| Intermediate Representation | **Bolt Intermediate Representation** | Architecture-neutral program form for analysis and optimization. |
| Optimization | **Optimization** | Improves Bolt Intermediate Representation without changing semantics. |
| Code Generation | **Code Generation** | Emits assembly or machine code from Bolt Intermediate Representation. |
| Linking | **Linking** | Produces the final executable or library. |

---

## Appendix — Syntax Mini-Guide (Attribute Style)
```bolt
[module("core.memory")]
import system.io

[packed]
blueprint Page { data: [byte; 4096] }

[interruptHandler]
integer function timerInterrupt() { /* ... */ }

[bareFunction]
[inSection(".text.boot")]
[aligned(4096)]
function earlyEntry() { /* ... */ }

[systemRequest(identifier=1)]
integer function systemWrite(integer fileDescriptor, &byte buffer, unsignedInteger length)
- **live value** — A qualifier that forces loads and stores to be side-effecting and visible to hardware or other agents.

**Reserved words (additions):** `interruptHandler`, `bareFunction`, `inSection`, `aligned`, `pageAligned`, `packed`, `bits`, `systemRequest`, `intrinsic`, `profile`.

---

## Complete Definition List (every word and what it does)

> This list defines **all language keywords, attributes, and core glossary nouns** used in this document, using full words and unambiguous phrasing.

### Keywords (core syntax)
- **module** — Declares a compilation and namespace unit for code.
- **import** — Brings public symbols from another module into scope.
- **blueprint** — Declares a data structure with named fields and explicit types.
- **enumeration** — Declares a finite set of named variants, optionally with payloads.
- **interface** — Declares a method-only type contract without stored data.
- **function** — Declares an executable routine with parameters and an optional return value.
- **constant** — Declares an immutable compile-time value.
- **mutable** — Declares a mutable runtime variable.
- **fixed** — Converts a variable into an immutable value after its first assignment.
- **alias** — Declares an alternate name for an existing type.
- **match** — Performs exhaustive pattern selection over an enumeration or tagged value.
- **guard** — Performs an early-exit check that must be satisfied to continue execution.
- **return** — Exits the current function, optionally returning a value.
- **break** — Exits the nearest loop immediately.
- **continue** — Skips to the next iteration of the nearest loop.

### Memory and concurrency helpers
- **pointer** — A value that holds the address of another value or a function.
- **reference** — A bound alias to an existing value without pointer arithmetic.
- **Live Value** — A qualifier that forces loads and stores to be side-effecting and visible to hardware or other agents.
- **Mutual Lock** — A synchronization primitive that grants exclusive access to a critical section.
- **Access Counter** — A synchronization primitive that allows up to a fixed number of concurrent entrants.
- **Busy Lock** — A synchronization primitive that spins while attempting to acquire a lock for a very short section.
- **Atomic Operations** — Indivisible read-modify-write operations with explicit memory ordering semantics.
- **Wake Signal** — A notification used to resume threads waiting in a **Sleep List**.
- **Sleep List** — A queue of threads that are waiting for a condition to be satisfied.

### Attributes (declaration modifiers)
- **interruptHandler** — Marks a function as an interrupt service routine with the correct prologue, epilogue, and return sequence.
- **bareFunction** — Suppresses the automatic prologue and epilogue; the function body must manage the stack and registers.
- **inSection(name)** — Places a symbol into a specific linker output section with the given name.
- **aligned(bytes)** — Enforces a minimum alignment in bytes for a symbol.
- **pageAligned** — Convenience attribute that enforces alignment to the current architecture page size.
- **packed** — Removes padding from a blueprint so that field layout is byte-tight; required for many memory-mapped device registers.
- **bits(width)** — Applied to a field inside a `packed` blueprint to set an exact bit width for that field.
- **systemRequest(identifier)** — Marks a function as a user-side entry to the kernel System Request Gateway and assigns the **Kernel System Request Identifier**.
- **intrinsic(name)** — Requests emission of a specific processor instruction sequence by name.

### Compiler and build nouns
- **Bolt Intermediate Representation** — The architecture-neutral program form used for analysis and optimization before code generation.
- **compiler** — The tool that translates Bolt source code into machine code or Bolt Intermediate Representation.
- **linker** — The tool that combines object files and libraries into a single artifact.
- **object file** — The compiled unit produced by the compiler prior to linking.
- **build script** — A script that declares targets, steps, and flags to build artifacts.
- **run descriptor** — A manifest that declares the entry point, permissions, and dependencies for an artifact.
- **Kernel System Request Identifier** — The numeric identifier that selects a kernel System Request handler.
- **User System Request Entry** — The user-side function that prepares registers and arguments and enters the System Request Gateway.

### Profiles, policies, and guarantees
- **freestanding** — A build profile with no operating system or runtime assumptions; exceptions are disabled and panic aborts.
- **panic abort** — A internal policy that traps immediately on a fatal condition without stack unwinding.
- **no hidden allocation** — A guarantee that the language and standard helpers do not allocate memory implicitly in freestanding builds.
- **bounds checks** — Optional runtime checks for memory accesses that can be enabled in debug builds and disabled in release builds.

End of v2.3 — Kernel-Ready (no underscores, full words, with complete definitions).


