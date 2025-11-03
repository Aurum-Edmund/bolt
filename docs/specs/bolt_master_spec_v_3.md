# Bolt Language & Standard Library Master Specification v3.1 (Kernel Edition)

---

## Overview
This document is the canonical, unified specification of the **Bolt programming language**, its **kernel-safe standard library**, and its **compiler/runtime interface contract**. It consolidates all design threads, modules, and appendices into one self-contained master source.

### Included Sections
1. Language Overview & Syntax Contract
2. Memory & Safety Model
3. Blueprint & Capability Semantics
4. Concurrency & Error Model
5. I/O Interface Contract
6. Kernel Boot Subset
7. Full Standard Library (all modules inlined)
8. Simulation & Test Examples
9. Appendices (A–F)

---

## 1. Language Overview & Syntax Contract
- **C‑style signatures**: `ReturnType name(Type a, Type b) { ... }`.
- **Semicolons required** for statements; **K&R braces**.
- **Type‑first** parameters and fields: `Type name`.
- **No nested functions or closures**: use top‑level `static` helpers. Composition uses a **context pointer and function pointers**.
- **Generics**: angle brackets, for example `Map<K, V>`.
- **Modules**: every file begins with `package …; module …;`.
- **Errors** use `Result<T, E>`; **optionals** use `Optional<T>`.
- **C‑style casts**: `(T) expression` between pointers, integers, and `byte`. No implicit narrowing conversions.
- **Fixed‑size arrays** are allowed (for example, `byte buffer[32];` and `byte hello[] = { 'h','i' };`). Arrays decay to pointers when passed as parameters.
- **Type qualifiers**: `constant` may prefix a type once; repeating the qualifier is rejected by the frontend.
- **Kernel markers** annotate external bindings to platform or hardware using square brackets, for example `[kernel_allocation]`, `[kernel_serial]`, `[kernel_time]`, `[kernel_sync]`, `[kernel_vfs]`.

---

## 2. Memory & Safety Model
- No hidden garbage collection. All dynamic allocation flows through an **Allocator** interface built from a **context pointer** and **function pointers**.
- Intrinsics are declared with `intrinsic` and must be pure or precisely specified side‑effects.
- Failure is explicit (no exceptions). Undefined behavior is never relied upon by the standard library.

**Allocator interface**
```bolt
package std.memory; module allocator;
use std.core.option as opt;

public blueprint Allocator {
    void* context;
    opt.Optional<pointer<byte>> (*allocate)(void* context, unsigned64 bytes, unsigned64 alignment);
    void (*deallocate)(void* context, pointer<byte> p, unsigned64 bytes, unsigned64 alignment);
    opt.Optional<pointer<byte>> (*reallocate)(void* context, pointer<byte> p, unsigned64 old_size, unsigned64 new_size, unsigned64 alignment);
};

[kernel_allocation] external void* __alloc_context;
[kernel_allocation] external opt.Optional<pointer<byte>> __alloc_allocate(void* context, unsigned64 bytes, unsigned64 alignment);
[kernel_allocation] external void __alloc_deallocate(void* context, pointer<byte> p, unsigned64 bytes, unsigned64 alignment);
[kernel_allocation] external opt.Optional<pointer<byte>> __alloc_reallocate(void* context, pointer<byte> p, unsigned64 old_size, unsigned64 new_size, unsigned64 alignment);

public Allocator system_allocator() {
    Allocator a; a.context = __alloc_context; a.allocate = __alloc_allocate; a.deallocate = __alloc_deallocate; a.reallocate = __alloc_reallocate; return a;
};
```

**Bump allocator (boot‑friendly)**
```bolt
package std.memory; module bump;
use std.core as core;
use std.core.option as opt;
use std.memory.allocator as allocator;

public blueprint BumpState { pointer<byte> start; unsigned64 cursor; unsigned64 capacity; };

static unsigned64 align_up_u64(unsigned64 x, unsigned64 a) { unsigned64 m = (x % a); if (m == 0) { return x; } return x + (a - m); };

public void bump_init(BumpState* s, core.Span<byte> region) { s->start = region.data; s->cursor = 0; s->capacity = region.length; };

static opt.Optional<pointer<byte>> bump_allocate(void* context, unsigned64 bytes, unsigned64 alignment) {
    BumpState* s = (BumpState*)context;
    unsigned64 base = (unsigned64)s->start;
    unsigned64 ptr = base + s->cursor;
    unsigned64 aligned = align_up_u64(ptr, alignment);
    unsigned64 next = (aligned - base) + bytes;
    if (next > s->capacity) { return opt.none<pointer<byte>>(); }
    s->cursor = next;
    return opt.some<pointer<byte>>((pointer<byte>)aligned);
};

static void bump_deallocate(void* context, pointer<byte> p, unsigned64 bytes, unsigned64 alignment) { (void)context; (void)p; (void)bytes; (void)alignment; };

static opt.Optional<pointer<byte>> bump_reallocate(void* context, pointer<byte> p, unsigned64 old_size, unsigned64 new_size, unsigned64 alignment) {
    opt.Optional<pointer<byte>> np = bump_allocate(context, new_size, alignment);
    if (!np.present) { return opt.none<pointer<byte>>(); }
    intrinsic_memcpy(np.value, p, (old_size < new_size) ? old_size : new_size);
    return np;
};

public void bump_reset(BumpState* s) { s->cursor = 0; };

public allocator.Allocator to_allocator(BumpState* s) {
    allocator.Allocator a; a.context = (void*)s; a.allocate = bump_allocate; a.deallocate = bump_deallocate; a.reallocate = bump_reallocate; return a;
};

intrinsic void intrinsic_memcpy(pointer<byte> dst, pointer<byte> src, unsigned64 n);
```

---

## 3. Blueprints, Kernel Markers & Interfaces
- **Blueprint**: a nominal record type (similar to a C `struct`) with Bolt semantics.
- **Interface**: a blueprint that exposes behavior via **a context pointer and function pointers**.
- **Kernel markers**: every `external` that touches platform or hardware must be annotated with the appropriate square‑bracket marker, for example `[kernel_allocation]`, `[kernel_serial]`, `[kernel_time]`, `[kernel_sync]`, `[kernel_vfs]`.

**Writer interface (example)**
```bolt
package std.io; module writer;
use std.core as core; use std.core.result as res;

public enum WriteError { DeviceError, NoSpace };

public blueprint Writer {
    void* context;
    res.Result<unsigned64, WriteError> (*write)(void* context, core.View<byte> src);
    res.Result<void, WriteError>       (*flush)(void* context);
};
```

---

## 4. Concurrency Model & Error Conventions
- **Cooperative scheduling** across the system.
- **Surfaces**:
  - `Scheduler` (spawn, yield, sleep)
  - `TaskHandle` (join, detach)
  - `Channel<T>` (send, receive, close)
- **No exceptions**: use `Result<T, E>` for operations that can fail and `Optional<T>` for presence or absence.
- **Timeout helpers** return a single error enumeration (no union error types):
  - `RecvError { Timeout, Closed, Empty }`.

**Scheduler surface**
```bolt
package std.concurrency; module task;
use std.core.result as res;

public enum TaskError { JoinOnSelf, Detached, SchedulerMissing };

public blueprint TaskId { unsigned64 raw };

public blueprint TaskHandle {
    TaskId id;
    res.Result<void, TaskError> (*join)(void* context);
    void (*detach)(void* context);
    void* context;
};

public blueprint Scheduler {
    res.Result<TaskHandle, TaskError> (*spawn)(void* context, void (*entry)(void*), void* arg);
    void (*yield_now)(void* context);
    void (*sleep_ticks)(void* context, unsigned64 ticks);
    void* context;
};
```

---

## 5. I/O Interface Contract
All I/O is defined as **interfaces with a context pointer and function pointers** so implementations can be swapped or layered.

**Reader**
```bolt
package std.io; module reader;
use std.core as core; use std.core.result as res;

public enum ReadError { EndOfStream, DeviceError };

public blueprint Reader {
    void* context;
    res.Result<unsigned64, ReadError> (*read)(void* context, core.Span<byte> dst);
};

public res.Result<void, ReadError> read_exact(Reader* r, core.Span<byte> dst) {
    unsigned64 off = 0;
    while (off < dst.length) {
        core.Span<byte> chunk = core.Span<byte>{ data: dst.data + off, length: dst.length - off };
        res.Result<unsigned64, ReadError> n = r->read(r->context, chunk);
        if (!n.ok) { return res.error<void, ReadError>(n.error); }
        if (n.value == 0) { return res.error<void, ReadError>(ReadError.EndOfStream); }
        off += n.value;
    }
    return res.ok<void, ReadError>({});
};
```

**Writer**
```bolt
package std.io; module writer;
use std.core as core; use std.core.result as res;

public enum WriteError { DeviceError, NoSpace };

public blueprint Writer {
    void* context;
    res.Result<unsigned64, WriteError> (*write)(void* context, core.View<byte> src);
    res.Result<void, WriteError>       (*flush)(void* context);
};

public res.Result<void, WriteError> write_all(Writer* w, core.View<byte> src) {
    unsigned64 off = 0;
    while (off < src.length) {
        core.View<byte> piece = core.View<byte>{ data: src.data + off, length: src.length - off };
        auto r = w->write(w->context, piece);
        if (!r.ok) { return res.error<void, WriteError>(r.error); }
        if (r.value == 0) { return res.error<void, WriteError>(WriteError.DeviceError); }
        off += r.value;
    }
    return w->flush(w->context);
};
```

**Serial writer binding (example of a kernel marker)**
```bolt
package std.io; module serial;
use std.core as core; use std.core.result as res; use std.io.writer as writer;

[kernel_serial] external res.Result<unsigned64, writer.WriteError> hw_write(core.View<byte> data);

static res.Result<unsigned64, writer.WriteError> serial_write(void* context, core.View<byte> src) { (void)context; return hw_write(src); };
static res.Result<void, writer.WriteError> serial_flush(void* context) { (void)context; return res.ok<void, writer.WriteError>({}); };

public writer.Writer writer() { writer.Writer w; w.context = (void*)0; w.write = serial_write; w.flush = serial_flush; return w; };
```

---

## 6. Kernel Boot‑Target Subset (Bolt‑Safe Core)
- **Allowed**: primitives, control flow, static storage, constants, basic blueprints, direct memory access, and `external` with a **kernel marker**.
- **Forbidden**: general allocators, file input or output, channels or tasks, recursion, and floating point.

**Boot‑legal example**
```bolt
package kernel.boot; module init;
use std.core as core;

public constant unsigned64 KERNEL_MAGIC = 0xA1R0B00T;

public blueprint BootInfo { unsigned64 total_memory; unsigned64 free_memory; };

public core.Result<void, unsigned32> initialize(BootInfo* info) {
    if (info->total_memory == 0) { return core.error<void, unsigned32>(1); }
    return core.ok<void, unsigned32>({});
};
```
---

## 7. Full Standard Library (inline)

### std/core/core.bolt

```bolt
package std.core; module core;

public blueprint Unit {};

public blueprint Pair<A, B> { A first; B second; };

public blueprint Slice<T> { pointer<T> data; unsigned64 length; };
public blueprint Span<T>  { pointer<T> data; unsigned64 length; };
public blueprint View<T>  { pointer<constant T> data; unsigned64 length; };

public blueprint RangeU64 { unsigned64 start; unsigned64 end_exclusive; };

public unsigned64 min_u64(unsigned64 a, unsigned64 b) { if (a < b) { return a; } return b; };
public unsigned64 max_u64(unsigned64 a, unsigned64 b) { if (a > b) { return a; } return b; };
```

### std/core/option.bolt

```bolt
package std.core; module option;

public blueprint Optional<T> { boolean present; T value; };

public Optional<T> some<T>(T v) { return Optional<T>{ present: true, value: v }; };
public Optional<T> none<T>()   { return Optional<T>{ present: false, value: (/* undefined */) }; };

public T unwrap_or<T>(Optional<T> o, T fallback) { if (o.present) { return o.value; } return fallback; };
```

### std/core/result.bolt
```bolt
package std.core; module result;

public blueprint Result<T, E> { boolean ok; T value; E error; };

public Result<T,E> ok<T,E>(T v) { return Result<T,E>{ ok: true, value: v, error: (/* unused */) }; };
public Result<T,E> error<T,E>(E e) { return Result<T,E>{ ok: false, value: (/* unused */), error: e }; };

public Result<U,E> map<T,U,E>(Result<T,E> r, U (*f)(T)) { if (r.ok) { return ok<U,E>(f(r.value)); } return Result<U,E>{ ok: false, value: (/* unused */), error: r.error }; };
public T unwrap_or<T,E>(Result<T,E> r, T fallback) { if (r.ok) { return r.value; } return fallback; };
```

### std/core/bytes.bolt
```bolt
package std.core; module bytes;
use std.core as core;

public core.View<T> view_from_ptr<T>(pointer<constant T> p, unsigned64 n) { return core.View<T>{ data: p, length: n }; };
public core.Span<T> span_from_ptr<T>(pointer<T> p, unsigned64 n)      { return core.Span<T>{ data: p, length: n }; };
public core.View<byte> view_from_cstring(pointer<constant byte> p) { unsigned64 n = 0; while (p[n] != 0) { n += 1; } return core.View<byte>{ data: p, length: n }; };
```

### std/core/assert.bolt

```bolt
package std.core; module assert;
use std.core as core; use std.core.result as res;

public void assert_true(boolean cond) { if (!cond) { intrinsic_trap(); } };
public void assert_eq_u64(unsigned64 a, unsigned64 b) { if (a != b) { intrinsic_trap(); } };

intrinsic void intrinsic_trap();
```

### std/core/traits.bolt

```bolt
package std.core; module traits;

public blueprint Equatable<T> { void* context; boolean (*equals)(void* context, T a, T b); };
public blueprint Hasher<T>    { void* context; unsigned64 (*hash)(void* context, T key); };
```

---

### std/memory/allocator.bolt

```bolt
package std.memory; module allocator;
use std.core.option as opt;

public blueprint Allocator {
    void* context;
    opt.Optional<pointer<byte>> (*allocate)(void* context, unsigned64 bytes, unsigned64 alignment);
    void (*deallocate)(void* context, pointer<byte> p, unsigned64 bytes, unsigned64 alignment);
    opt.Optional<pointer<byte>> (*reallocate)(void* context, pointer<byte> p, unsigned64 old_size, unsigned64 new_size, unsigned64 alignment);
};

[kernel_allocation] external void* __alloc_context;
[kernel_allocation] external opt.Optional<pointer<byte>> __alloc_allocate(void* context, unsigned64 bytes, unsigned64 alignment);
[kernel_allocation] external void __alloc_deallocate(void* context, pointer<byte> p, unsigned64 bytes, unsigned64 alignment);
[kernel_allocation] external opt.Optional<pointer<byte>> __alloc_reallocate(void* context, pointer<byte> p, unsigned64 old_size, unsigned64 new_size, unsigned64 alignment);

public Allocator system_allocator() {
    Allocator a; a.context = __alloc_context; a.allocate = __alloc_allocate; a.deallocate = __alloc_deallocate; a.reallocate = __alloc_reallocate; return a;
};
```

### std/memory/bump.bolt

```bolt
package std.memory; module bump;
use std.core as core; use std.core.option as opt; use std.memory.allocator as allocator;

public blueprint BumpState { pointer<byte> start; unsigned64 cursor; unsigned64 capacity; };

static unsigned64 align_up_u64(unsigned64 x, unsigned64 a) { unsigned64 m = (x % a); if (m == 0) { return x; } return x + (a - m); };

public void bump_init(BumpState* s, core.Span<byte> region) { s->start = region.data; s->cursor = 0; s->capacity = region.length; };

static opt.Optional<pointer<byte>> bump_allocate(void* context, unsigned64 bytes, unsigned64 alignment) {
    BumpState* s = (BumpState*)context;
    unsigned64 base = (unsigned64)s->start;
    unsigned64 ptr = base + s->cursor;
    unsigned64 aligned = align_up_u64(ptr, alignment);
    unsigned64 next = (aligned - base) + bytes;
    if (next > s->capacity) { return opt.none<pointer<byte>>(); }
    s->cursor = next;
    return opt.some<pointer<byte>>((pointer<byte>)aligned);
};

static void bump_deallocate(void* context, pointer<byte> p, unsigned64 bytes, unsigned64 alignment) { (void)context; (void)p; (void)bytes; (void)alignment; };

static opt.Optional<pointer<byte>> bump_reallocate(void* context, pointer<byte> p, unsigned64 old_size, unsigned64 new_size, unsigned64 alignment) {
    opt.Optional<pointer<byte>> np = bump_allocate(context, new_size, alignment);
    if (!np.present) { return opt.none<pointer<byte>>(); }
    intrinsic_memcpy(np.value, p, (old_size < new_size) ? old_size : new_size);
    return np;
};

public void bump_reset(BumpState* s) { s->cursor = 0; };

public allocator.Allocator to_allocator(BumpState* s) { allocator.Allocator a; a.context = (void*)s; a.allocate = bump_allocate; a.deallocate = bump_deallocate; a.reallocate = bump_reallocate; return a; };

intrinsic void intrinsic_memcpy(pointer<byte> dst, pointer<byte> src, unsigned64 n);
```

### std/memory/arena.bolt

```bolt
package std.memory; module arena;
use std.core as core; use std.memory.allocator as allocator; use std.memory.bump as bump;

public blueprint Arena { bump.BumpState state };

public void create(Arena* a, core.Span<byte> region) { bump.bump_init(&a->state, region); };
public allocator.Allocator allocator(Arena* a) { return bump.to_allocator(&a->state); };
public void reset(Arena* a) { bump.bump_reset(&a->state); };
```

### std/memory/buffer.bolt

```bolt
package std.memory; module buffer;
use std.core as core; use std.core.result as res; use std.core.option as opt; use std.memory.allocator as allocator;

public enum BufferError { AllocationFailed };

public blueprint Buffer { pointer<byte> data; unsigned64 length; unsigned64 capacity; allocator.Allocator allocator; };

public res.Result<Buffer, BufferError> create(unsigned64 capacity, allocator.Allocator a) {
    opt.Optional<pointer<byte>> p = a.allocate(a.context, capacity, 8);
    if (!p.present) { return res.error<Buffer, BufferError>(BufferError.AllocationFailed); }
    Buffer b; b.data = p.value; b.length = 0; b.capacity = capacity; b.allocator = a; return res.ok<Buffer, BufferError>(b);
};

public res.Result<void, BufferError> ensure_capacity(Buffer* b, unsigned64 need) {
    if (need <= b->capacity) { return res.ok<void, BufferError>({}); }
    opt.Optional<pointer<byte>> np = b->allocator.reallocate(b->allocator.context, b->data, b->capacity, need, 8);
    if (!np.present) { return res.error<void, BufferError>(BufferError.AllocationFailed); }
    b->data = np.value; b->capacity = need; return res.ok<void, BufferError>({});
};

public res.Result<void, BufferError> write_bytes(Buffer* b, core.View<byte> src) {
    unsigned64 new_len = b->length + src.length;
    auto r = ensure_capacity(b, new_len);
    if (!r.ok) { return res.error<void, BufferError>(r.error); }
    intrinsic_memcpy(b->data + b->length, (pointer<byte>)src.data, src.length);
    b->length = new_len; return res.ok<void, BufferError>({});
};

intrinsic void intrinsic_memcpy(pointer<byte> dst, pointer<byte> src, unsigned64 n);
```

### std/memory/view_span.bolt

```bolt
package std.memory; module view_span;
use std.core as core;

public core.Span<byte> span_from_buffer(pointer<byte> p, unsigned64 capacity) { core.Span<byte> s; s.data = p; s.length = capacity; return s; };
public core.View<byte> view_from_buffer(pointer<constant byte> p, unsigned64 length) { core.View<byte> v; v.data = p; v.length = length; return v; };
public core.Span<T> span_slice<T>(core.Span<T> s, unsigned64 start, unsigned64 count) { core.Span<T> out; out.data = s.data + start; out.length = count; return out; };
public core.View<T> view_slice<T>(core.View<T> v, unsigned64 start, unsigned64 count) { core.View<T> out; out.data = v.data + start; out.length = count; return out; };
```

---

### std/io/reader.bolt

```bolt
package std.io; module reader;
use std.core as core; use std.core.result as res;

public enum ReadError { EndOfStream, DeviceError };

public blueprint Reader {
    void* context;
    res.Result<unsigned64, ReadError> (*read)(void* context, core.Span<byte> dst);
};

public res.Result<void, ReadError> read_exact(Reader* r, core.Span<byte> dst) {
    unsigned64 off = 0;
    while (off < dst.length) {
        core.Span<byte> chunk = core.Span<byte>{ data: dst.data + off, length: dst.length - off };
        res.Result<unsigned64, ReadError> n = r->read(r->context, chunk);
        if (!n.ok) { return res.error<void, ReadError>(n.error); }
        if (n.value == 0) { return res.error<void, ReadError>(ReadError.EndOfStream); }
        off += n.value;
    }
    return res.ok<void, ReadError>({});
};
```

### std/io/writer.bolt

```bolt
package std.io; module writer;
use std.core as core; use std.core.result as res;

public enum WriteError { DeviceError, NoSpace };

public blueprint Writer {
    void* context;
    res.Result<unsigned64, WriteError> (*write)(void* context, core.View<byte> src);
    res.Result<void, WriteError>       (*flush)(void* context);
};

public res.Result<void, WriteError> write_all(Writer* w, core.View<byte> src) {
    unsigned64 off = 0;
    while (off < src.length) {
        core.View<byte> piece = core.View<byte>{ data: src.data + off, length: src.length - off };
        auto r = w->write(w->context, piece);
        if (!r.ok) { return res.error<void, WriteError>(r.error); }
        if (r.value == 0) { return res.error<void, WriteError>(WriteError.DeviceError); }
        off += r.value;
    }
    return w->flush(w->context);
};
```

### std/io/serial.bolt

```bolt
package std.io; module serial;
use std.core as core; use std.core.result as res; use std.io.writer as writer;

[kernel_serial] external res.Result<unsigned64, writer.WriteError> hw_write(core.View<byte> data);

static res.Result<unsigned64, writer.WriteError> serial_write(void* context, core.View<byte> src) { (void)context; return hw_write(src); };
static res.Result<void, writer.WriteError> serial_flush(void* context) { (void)context; return res.ok<void, writer.WriteError>({}); };

public writer.Writer writer() { writer.Writer w; w.context = (void*)0; w.write = serial_write; w.flush = serial_flush; return w; };
```

### std/io/console.bolt

```bolt
package std.io; module console;
use std.io.writer as writer; use std.io.serial as serial;

public writer.Writer out() { return serial.writer(); };
```

### std/io/buffered_writer.bolt

```bolt
package std.io; module buffered_writer;
use std.core as core; use std.core.result as res; use std.io.writer as writer;

public enum BwError { FlushFailed };

public blueprint BufferedWriter { writer.Writer inner; pointer<byte> buf; unsigned64 length; unsigned64 capacity; };

public BufferedWriter with_buffer(writer.Writer inner, core.Span<byte> storage) { BufferedWriter b; b.inner = inner; b.buf = storage.data; b.length = 0; b.capacity = storage.length; return b; };

public res.Result<void, BwError> flush(BufferedWriter* b) {
    core.View<byte> v = core.View<byte>{ data: (pointer<constant byte>)b->buf, length: b->length };
    auto r = writer.write_all(&b->inner, v);
    if (!r.ok) { return res.error<void, BwError>(BwError.FlushFailed); }
    b->length = 0; return res.ok<void, BwError>({});
};

public res.Result<void, BwError> write(BufferedWriter* b, core.View<byte> src) {
    if (src.length > b->capacity) { (void)flush(b); auto r = writer.write_all(&b->inner, src); if (!r.ok) { return res.error<void, BwError>(BwError.FlushFailed); } return res.ok<void, BwError>({}); }
    if (b->length + src.length > b->capacity) { (void)flush(b); }
    intrinsic_memcpy(b->buf + b->length, (pointer<byte>)src.data, src.length);
    b->length += src.length; return res.ok<void, BwError>({});
};

intrinsic void intrinsic_memcpy(pointer<byte> dst, pointer<byte> src, unsigned64 n);
```

### std/io/console_format.bolt

```bolt
package std.io; module console_format;
use std.core as core; use std.core.result as res; use std.io.console as console; use std.io.writer as writer; use std.memory.view_span as vs; use std.text.format_int as fmti;

public void print_line_bytes(core.View<byte> bytes) { writer.Writer w = console.out(); (void)writer.write_all(&w, bytes); byte nl[1] = {'\n'}; core.View<byte> newline = vs.view_from_buffer((pointer<constant byte>)&nl[0], 1); (void)writer.write_all(&w, newline); };

public void print_u64(unsigned64 v) { writer.Writer w = console.out(); byte buf[32]; core.Span<byte> span = vs.span_from_buffer(&buf[0], 32); res.Result<unsigned64, fmti.FormatError> r = fmti.u64_to_ascii(v, span); if (r.ok) { core.View<byte> out = core.View<byte>{ data: (pointer<constant byte>)&buf[0], length: r.value }; (void)writer.write_all(&w, out); } byte nl[1] = {'\n'}; core.View<byte> newline = vs.view_from_buffer((pointer<constant byte>)&nl[0], 1); (void)writer.write_all(&w, newline); };
```

---

### std/text/string.bolt

```bolt
package std.text; module string;
use std.core as core; use std.core.result as res; use std.memory.buffer as buffer; use std.memory.allocator as allocator;

public enum StringError { AllocationFailed, OutOfRange };

public blueprint String { buffer.Buffer buffer };

public res.Result<String, StringError> create(unsigned64 capacity, allocator.Allocator a) { auto b = buffer.create(capacity, a); if (!b.ok) { return res.error<String, StringError>(StringError.AllocationFailed); } String s; s.buffer = b.value; return res.ok<String, StringError>(s); };

public unsigned64 length(String* s) { return s->buffer.length; };
public unsigned64 capacity(String* s) { return s->buffer.capacity; };
public void clear(String* s) { s->buffer.length = 0; };

public res.Result<void, StringError> reserve(String* s, unsigned64 add_bytes) { unsigned64 need = s->buffer.length + add_bytes; auto r = buffer.ensure_capacity(&s->buffer, need); if (!r.ok) { return res.error<void, StringError>(StringError.AllocationFailed); } return res.ok<void, StringError>({}); };

public res.Result<void, StringError> append_byte(String* s, byte b) { auto rr = reserve(s, 1); if (!rr.ok) { return rr; } s->buffer.data[s->buffer.length] = b; s->buffer.length += 1; return res.ok<void, StringError>({}); };

public res.Result<void, StringError> append_view(String* s, core.View<byte> v) { auto r = buffer.write_bytes(&s->buffer, v); if (!r.ok) { return res.error<void, StringError>(StringError.AllocationFailed); } return res.ok<void, StringError>({}); };

public core.View<byte> as_view(String* s) { core.View<byte> v; v.data = (pointer<constant byte>)s->buffer.data; v.length = s->buffer.length; return v; };

public res.Result<core.View<byte>, StringError> slice(String* s, unsigned64 start, unsigned64 end_excl) { if ((start > end_excl) || (end_excl > s->buffer.length)) { return res.error<core.View<byte>, StringError>(StringError.OutOfRange); } core.View<byte> v; v.data = (pointer<constant byte>)(s->buffer.data + start); v.length = (end_excl - start); return res.ok<core.View<byte>, StringError>(v); };

public res.Result<void, StringError> insert(String* s, unsigned64 index, core.View<byte> v) { if (index > s->buffer.length) { return res.error<void, StringError>(StringError.OutOfRange); } auto rr = reserve(s, v.length); if (!rr.ok) { return rr; } unsigned64 i = s->buffer.length; while (i > index) { s->buffer.data[i + v.length - 1] = s->buffer.data[i - 1]; i -= 1; } intrinsic_memcpy(s->buffer.data + index, (pointer<byte>)v.data, v.length); s->buffer.length += v.length; return res.ok<void, StringError>({}); };

intrinsic void intrinsic_memcpy(pointer<byte> dst, pointer<byte> src, unsigned64 n);
```

### std/text/format.bolt

```bolt
package std.text; module format;
use std.core as core; use std.core.result as res; use std.text.string as str; use std.memory.buffer as buffer;

public enum FormatError { AllocationFailed };

public res.Result<void, FormatError> append_bytes(str.String* s, core.View<byte> v) { auto r = buffer.write_bytes(&s->buffer, v); if (!r.ok) { return res.error<void, FormatError>(FormatError.AllocationFailed); } return res.ok<void, FormatError>({}); };
```

### std/text/format_int.bolt

```bolt
package std.text; module format_int;
use std.core as core; use std.core.result as res;

public enum FormatError { BufferTooSmall };

public res.Result<unsigned64, FormatError> u64_to_ascii(unsigned64 value, core.Span<byte> dst) {
    if (value == 0) {
        if (dst.length == 0) { return res.error<unsigned64, FormatError>(FormatError.BufferTooSmall); }
        dst.data[0] = '0'; return res.ok<unsigned64, FormatError>(1);
    }
    unsigned64 i = 0;
    while (value > 0) {
        if (i >= dst.length) { return res.error<unsigned64, FormatError>(FormatError.BufferTooSmall); }
        unsigned64 digit = value % 10; dst.data[i] = (byte)('0' + (byte)digit); value /= 10; i += 1;
    }
    unsigned64 a = 0; unsigned64 b = i - 1;
    while (a < b) { byte t = dst.data[a]; dst.data[a] = dst.data[b]; dst.data[b] = t; a += 1; b -= 1; }
    return res.ok<unsigned64, FormatError>(i);
};
```

### std/text/utf8.bolt

```bolt
package std.text; module utf8;
use std.core as core;

public enum Utf8Result { Ok, Invalid };

public Utf8Result validate(core.View<byte> v) {
    unsigned64 i = 0;
    while (i < v.length) {
        byte c = v.data[i];
        if ((c & 0x80) == 0) { i += 1; continue; }
        if ((c & 0xE0) == 0xC0) {
            if (i + 1 >= v.length) { return Utf8Result.Invalid; }
            byte c1 = v.data[i + 1]; if ((c1 & 0xC0) != 0x80) { return Utf8Result.Invalid; }
            unsigned16 lead = (unsigned16)(c & 0x1F); if (lead == 0) { return Utf8Result.Invalid; }
            i += 2; continue;
        }
        if ((c & 0xF0) == 0xE0) {
            if (i + 2 >= v.length) { return Utf8Result.Invalid; }
            byte c1 = v.data[i + 1]; byte c2 = v.data[i + 2];
            if (((c1 & 0xC0) != 0x80) || ((c2 & 0xC0) != 0x80)) { return Utf8Result.Invalid; }
            i += 3; continue;
        }
        if ((c & 0xF8) == 0xF0) {
            if (i + 3 >= v.length) { return Utf8Result.Invalid; }
            byte c1 = v.data[i + 1]; byte c2 = v.data[i + 2]; byte c3 = v.data[i + 3];
            if (((c1 & 0xC0) != 0x80) || ((c2 & 0xC0) != 0x80) || ((c3 & 0xC0) != 0x80)) { return Utf8Result.Invalid; }
            i += 4; continue;
        }
        return Utf8Result.Invalid;
    }
    return Utf8Result.Ok;
};
```
---
### std/time/time.bolt
```bolt
package std.time; module time;
use std.core as core; use std.core.result as res;

public constant unsigned64 TICKS_PER_SECOND = 1_000_000; // 1 tick = 1 microsecond

public blueprint Instant { unsigned64 ticks; };
public blueprint Duration { unsigned64 ticks; };

public Duration duration_between(Instant start, Instant end_) {
    if (end_.ticks <= start.ticks) { return Duration{ ticks: 0 }; }
    return Duration{ ticks: end_.ticks - start.ticks };
};

public unsigned64 as_milliseconds(Duration d) { return d.ticks / (TICKS_PER_SECOND / 1000); };
public Duration from_milliseconds(unsigned64 ms) { return Duration{ ticks: ms * (TICKS_PER_SECOND / 1000) }; };
public Duration add(Duration a, Duration b) { return Duration{ ticks: a.ticks + b.ticks }; };
public Instant add_to_instant(Instant t, Duration d) { return Instant{ ticks: t.ticks + d.ticks }; };

[kernel_time] external res.Result<Instant, unsigned32> monotonic();
```

### std/time/units.bolt
```bolt
package std.time; module units; use std.time.time as t;

public t.Duration seconds(unsigned64 s) { return t.Duration{ ticks: s * t.TICKS_PER_SECOND }; };
public t.Duration milliseconds(unsigned64 ms) { return t.from_milliseconds(ms); };
public t.Duration microseconds(unsigned64 us) { return t.Duration{ ticks: us * (t.TICKS_PER_SECOND / 1_000_000) }; };
```

---

### std/math/checked.bolt
```bolt
package std.math; module checked; use std.core.result as res;

public enum Overflow { Overflowed };

public res.Result<unsigned64, Overflow> add_u64(unsigned64 a, unsigned64 b) {
    unsigned64 c = a + b; if (c < a) { return res.error<unsigned64, Overflow>(Overflow.Overflowed); } return res.ok<unsigned64, Overflow>(c);
};

public res.Result<unsigned64, Overflow> sub_u64(unsigned64 a, unsigned64 b) {
    if (b > a) { return res.error<unsigned64, Overflow>(Overflow.Overflowed); } return res.ok<unsigned64, Overflow>(a - b);
};

public res.Result<unsigned64, Overflow> mul_u64(unsigned64 a, unsigned64 b) {
    if (a == 0 || b == 0) { return res.ok<unsigned64, Overflow>(0); }
    unsigned64 c = a * b; if ((c / a) != b) { return res.error<unsigned64, Overflow>(Overflow.Overflowed); }
    return res.ok<unsigned64, Overflow>(c);
};
```

### std/math/saturated.bolt
```bolt
package std.math; module saturated;

public unsigned64 add_u64(unsigned64 a, unsigned64 b) {
    unsigned64 c = a + b; if (c < a) { return 0xFFFF_FFFF_FFFF_FFFF; } return c;
};

public unsigned64 sub_u64(unsigned64 a, unsigned64 b) { return (a < b) ? 0 : (a - b); };
```

---

### std/concurrency/task.bolt
```bolt
package std.concurrency; module task; use std.core.result as res;

public enum TaskError { JoinOnSelf, Detached, SchedulerMissing };

public blueprint TaskId { unsigned64 raw; };

public blueprint TaskHandle {
    TaskId id;
    res.Result<void, TaskError> (*join)(void* context);
    void (*detach)(void* context);
    void* context;
};

public blueprint Scheduler {
    res.Result<TaskHandle, TaskError> (*spawn)(void* context, void (*entry)(void*), void* arg);
    void (*yield_now)(void* context);
    void (*sleep_ticks)(void* context, unsigned64 ticks);
    void* context;
};

static Scheduler GLOBAL_SCHEDULER;

public void install(Scheduler s) { GLOBAL_SCHEDULER = s; };

public res.Result<TaskHandle, TaskError> spawn(void (*entry)(void*), void* arg) {
    if (GLOBAL_SCHEDULER.spawn == (void*)0) { return res.error<TaskHandle, TaskError>(TaskError.SchedulerMissing); }
    return GLOBAL_SCHEDULER.spawn(GLOBAL_SCHEDULER.context, entry, arg);
};

public void yield_now() { if (GLOBAL_SCHEDULER.yield_now != (void*)0) { GLOBAL_SCHEDULER.yield_now(GLOBAL_SCHEDULER.context); } };
public void sleep_ticks(unsigned64 t) { if (GLOBAL_SCHEDULER.sleep_ticks != (void*)0) { GLOBAL_SCHEDULER.sleep_ticks(GLOBAL_SCHEDULER.context, t); } };
```

### std/concurrency/mutex.bolt
```bolt
package std.concurrency; module mutex;

public blueprint Mutex { void* context; void (*lock)(void* context); void (*unlock)(void* context); };
```

### std/concurrency/channel.bolt
```bolt
package std.concurrency; module channel; use std.core.result as res;

public enum ChanError { Closed, Full, Empty };

public blueprint Channel<T> {
    void* context;
    res.Result<void, ChanError> (*send)(void* context, T v);
    res.Result<T, ChanError> (*receive)(void* context);
    void (*close)(void* context);
};

[kernel_sync] external res.Result<Channel<T>, ChanError> sys_make_bounded<T>(unsigned64 capacity);

public res.Result<Channel<T>, ChanError> bounded<T>(unsigned64 capacity) { return sys_make_bounded<T>(capacity); };
```

### std/concurrency/channel_blocking.bolt
```bolt
package std.concurrency; module channel_blocking; use std.concurrency.channel as ch; use std.concurrency.task as task; use std.core.result as res;

public res.Result<void, ch.ChanError> send_blocking<T>(ch.Channel<T>* c, T v) {
    while (true) {
        auto r = c->send(c->context, v);
        if (r.ok) { return r; }
        if (r.error == ch.ChanError.Closed) { return r; }
        task.yield_now();
    }
};

public res.Result<T, ch.ChanError> receive_blocking<T>(ch.Channel<T>* c) {
    while (true) {
        auto r = c->receive(c->context);
        if (r.ok) { return r; }
        if (r.error == ch.ChanError.Closed) { return r; }
        task.yield_now();
    }
};
```

### std/concurrency/channel_timeout.bolt
```bolt
package std.concurrency; module channel_timeout; use std.concurrency.channel as ch; use std.concurrency.task as task; use std.core.result as res; use std.time.time as time;

public enum RecvError { Timeout, Closed, Empty };

public res.Result<T, RecvError> receive_until<T>(ch.Channel<T>* c, time.Instant deadline) {
    while (true) {
        auto r = c->receive(c->context);
        if (r.ok) { return res.ok<T, RecvError>(r.value); }
        if (r.error == ch.ChanError.Closed) { return res.error<T, RecvError>(RecvError.Closed); }
        auto now = time.monotonic();
        if (!now.ok) { task.yield_now(); continue; }
        if (now.value.ticks >= deadline.ticks) { return res.error<T, RecvError>(RecvError.Timeout); }
        task.yield_now();
    }
};
```

### std/concurrency/channel_timeout_for.bolt
```bolt
package std.concurrency; module channel_timeout_for; use std.concurrency.channel as ch; use std.core.result as res; use std.time.time as time;

public enum RecvError { Timeout, Closed, Empty };

public res.Result<T, RecvError> receive_for<T>(ch.Channel<T>* c, time.Duration d) {
    auto start = time.monotonic();
    if (!start.ok) { return res.error<T, RecvError>(RecvError.Timeout); }
    time.Instant deadline = time.Instant{ ticks: start.value.ticks + d.ticks };
    return std.concurrency.channel_timeout.receive_until<T>(c, deadline);
};
```

---

### std/fs/path.bolt
```bolt
package std.fs; module path; use std.core as core; use std.core.result as res;

public enum PathError { Invalid, TooLong };

public blueprint Path { core.View<byte> bytes; };

public res.Result<Path, PathError> from_bytes(core.View<byte> b) {
    if (b.length == 0) { return res.error<Path, PathError>(PathError.Invalid); }
    if (b.length > 4096) { return res.error<Path, PathError>(PathError.TooLong); }
    Path p; p.bytes = b; return res.ok<Path, PathError>(p);
};
```

### std/fs/file.bolt
```bolt
package std.fs; module file; use std.core as core; use std.core.result as res; use std.fs.path as p;

public enum OpenMode { ReadOnly, WriteOnly, ReadWrite, Create, Truncate, Append };
public enum FileError { NotFound, NoPermission, Busy, Invalid, DeviceError };

public blueprint File { void* context; res.Result<unsigned64, FileError> (*read)(void* context, core.Span<byte> dst); res.Result<unsigned64, FileError> (*write)(void* context, core.View<byte> src); res.Result<void, FileError> (*flush)(void* context); void (*close)(void* context); };

public res.Result<File, FileError> open(p.Path path, OpenMode mode) { return sys_open(path, mode); };

[kernel_vfs] external res.Result<File, FileError> sys_open(p.Path path, OpenMode mode);
```

### std/fs/path_ops.bolt
```bolt
package std.fs; module path_ops; use std.core as core; use std.core.result as res; use std.fs.path as p;

public enum PathOpError { Invalid, TooLong };

public res.Result<p.Path, PathOpError> join(p.Path a, p.Path b, core.Span<byte> outbuf) {
    if (a.bytes.length == 0 || b.bytes.length == 0) { return res.error<p.Path, PathOpError>(PathOpError.Invalid); }
    unsigned64 len = 0;
    if (a.bytes.length > outbuf.length) { return res.error<p.Path, PathOpError>(PathOpError.TooLong); }
    intrinsic_memcpy(outbuf.data + len, (pointer<byte>)a.bytes.data, a.bytes.length); len += a.bytes.length;
    if (len == 0 || outbuf.data[len - 1] != '/') { if (len + 1 > outbuf.length) { return res.error<p.Path, PathOpError>(PathOpError.TooLong); } outbuf.data[len] = '/'; len += 1; }
    if (len + b.bytes.length > outbuf.length) { return res.error<p.Path, PathOpError>(PathOpError.TooLong); }
    intrinsic_memcpy(outbuf.data + len, (pointer<byte>)b.bytes.data, b.bytes.length); len += b.bytes.length;
    p.Path out; out.bytes = core.View<byte>{ data: (pointer<constant byte>)outbuf.data, length: len }; return res.ok<p.Path, PathOpError>(out);
};

public res.Result<p.Path, PathOpError> normalize(p.Path inpath, core.Span<byte> outbuf) {
    unsigned64 o = 0; unsigned64 i = 0; while (i < inpath.bytes.length) {
        byte c = inpath.bytes.data[i];
        if (c == '/') { if (o == 0 || outbuf.data[o - 1] != '/') { if (o + 1 > outbuf.length) { return res.error<p.Path, PathOpError>(PathOpError.TooLong); } outbuf.data[o++] = '/'; } i += 1; continue; }
        if (c == '.') { if (i + 1 < inpath.bytes.length && inpath.bytes.data[i + 1] == '/') { i += 2; continue; } }
        if (o + 1 > outbuf.length) { return res.error<p.Path, PathOpError>(PathOpError.TooLong); }
        outbuf.data[o++] = c; i += 1;
    }
    p.Path out; out.bytes = core.View<byte>{ data: (pointer<constant byte>)outbuf.data, length: o }; return res.ok<p.Path, PathOpError>(out);
};

intrinsic void intrinsic_memcpy(pointer<byte> dst, pointer<byte> src, unsigned64 n);
```

### std/collections/array_list.bolt
```bolt
package std.collections; module array_list;

use std.core as core;
use std.core.result as res;
use std.memory.allocator as allocator;

public enum ListError { AllocationFailed, OutOfRange };

public blueprint ArrayList<T> {
    pointer<T> data;
    unsigned64 length;
    unsigned64 capacity;
    allocator.Allocator allocator;
};

public res.Result<ArrayList<T>, ListError> create<T>(unsigned64 capacity, allocator.Allocator a) {
    unsigned64 bytes = capacity * sizeof(T);
    auto p = a.allocate(a.context, bytes, alignof(T));
    if (!p.present) { return res.error<ArrayList<T>, ListError>(ListError.AllocationFailed); }
    ArrayList<T> l; l.data = (pointer<T>)p.value; l.length = 0; l.capacity = capacity; l.allocator = a; return res.ok<ArrayList<T>, ListError>(l);
};

public res.Result<void, ListError> reserve<T>(ArrayList<T>* l, unsigned64 need_capacity) {
    if (need_capacity <= l->capacity) { return res.ok<void, ListError>({}); }
    unsigned64 new_cap = need_capacity;
    unsigned64 old_bytes = l->capacity * sizeof(T);
    unsigned64 new_bytes = new_cap * sizeof(T);
    auto np = l->allocator.reallocate(l->allocator.context, (pointer<byte>)l->data, old_bytes, new_bytes, alignof(T));
    if (!np.present) { return res.error<void, ListError>(ListError.AllocationFailed); }
    l->data = (pointer<T>)np.value; l->capacity = new_cap; return res.ok<void, ListError>({});
};

public res.Result<void, ListError> push<T>(ArrayList<T>* l, T value) {
    if (l->length == l->capacity) { unsigned64 target = (l->capacity == 0) ? 1 : (l->capacity * 2); auto rr = reserve<T>(l, target); if (!rr.ok) { return rr; } }
    l->data[l->length] = value; l->length += 1; return res.ok<void, ListError>({});
};

public res.Result<pointer<T>, ListError> get_ref<T>(ArrayList<T>* l, unsigned64 index) {
    if (index >= l->length) { return res.error<pointer<T>, ListError>(ListError.OutOfRange); }
    return res.ok<pointer<T>, ListError>(l->data + index);
};

public res.Result<T, ListError> pop_back<T>(ArrayList<T>* l) {
    if (l->length == 0) { return res.error<T, ListError>(ListError.OutOfRange); }
    l->length -= 1; return res.ok<T, ListError>(l->data[l->length]);
};
```

---

### std/collections/ring_buffer.bolt
```bolt
package std.collections;
module ring_buffer;

use std.core as core;
use std.core.result as res;
use std.memory.allocator as alloc;

public enum RingError { AllocationFailed, Empty, Full };

public blueprint RingBuffer<T> {
    pointer<T> data;
    unsigned64 capacity;
    unsigned64 head;  // pop from head
    unsigned64 tail;  // push at tail
    unsigned64 count;
    alloc.Allocator allocator;
};

public res.Result<RingBuffer<T>, RingError> create<T>(unsigned64 capacity, alloc.Allocator a) {
    auto p = a.allocate(a.context, capacity * sizeof(T), alignof(T));
    if (!p.present) { return res.error<RingBuffer<T>, RingError>(RingError.AllocationFailed); }
    RingBuffer<T> r;
    r.data = (pointer<T>)p.value;
    r.capacity = capacity;
    r.head = 0; r.tail = 0; r.count = 0; r.allocator = a;
    return res.ok<RingBuffer<T>, RingError>(r);
}

public boolean is_empty<T>(RingBuffer<T>* r) { return r->count == 0; }
public boolean is_full<T>(RingBuffer<T>* r)  { return r->count == r->capacity; }

public res.Result<void, RingError> push<T>(RingBuffer<T>* r, T v) {
    if (is_full<T>(r)) { return res.error<void, RingError>(RingError.Full); }
    r->data[r->tail] = v;
    r->tail = (r->tail + 1) % r->capacity;
    r->count += 1;
    return res.ok<void, RingError>({});
}

public res.Result<T, RingError> pop<T>(RingBuffer<T>* r) {
    if (is_empty<T>(r)) { return res.error<T, RingError>(RingError.Empty); }
    T v = r->data[r->head];
    r->head = (r->head + 1) % r->capacity;
    r->count -= 1;
    return res.ok<T, RingError>(v);
}
```

---

### std/collections/hash_map.bolt
```bolt
package std.collections;
module hash_map;

use std.core as core;
use std.core.result as res;
use std.core.option as opt;
use std.memory.allocator as alloc;

public enum MapError { AllocationFailed, Full, NotFound };

public blueprint Entry<K, V> {
    byte state; // 0=Empty,1=Occupied,2=Tombstone
    K key;
    V value;
};

public blueprint HashMap<K, V> {
    pointer<Entry<K,V>> entries;
    unsigned64 capacity;   // power of two
    unsigned64 length;     // occupied
    unsigned64 tombstones; // tombstone count
    alloc.Allocator allocator;
    void* context;
    unsigned64 (*hash)(void* context, K key);
    boolean (*equals)(void* context, K a, K b);
};

static unsigned64 next_pow2(unsigned64 x) {
    if (x < 2) { return 2; }
    x -= 1;
    x |= x >> 1; x |= x >> 2; x |= x >> 4; x |= x >> 8; x |= x >> 16; x |= x >> 32;
    return x + 1;
}

static void zero_entries(pointer<Entry<K,V>> e, unsigned64 n) {
    unsigned64 i = 0; while (i < n) { e[i].state = 0; i += 1; }
}

public res.Result<HashMap<K,V>, MapError> create<K,V>(unsigned64 min_capacity,
                                                      alloc.Allocator a,
                                                      void* context,
                                                      unsigned64 (*hash)(void* context, K),
                                                      boolean (*equals)(void* context, K, K))
{
    unsigned64 cap = next_pow2(min_capacity < 8 ? 8 : min_capacity);
    unsigned64 bytes = cap * sizeof(Entry<K,V>);
    auto p = a.allocate(a.context, bytes, alignof(Entry<K,V>));
    if (!p.present) { return res.error<HashMap<K,V>, MapError>(MapError.AllocationFailed); }

    HashMap<K,V> m;
    m.entries = (pointer<Entry<K,V>>)p.value;
    m.capacity = cap;
    m.length = 0;
    m.tombstones = 0;
    m.allocator = a;
    m.context = context;
    m.hash = hash;
    m.equals = equals;
    zero_entries(m.entries, m.capacity);
    return res.ok<HashMap<K,V>, MapError>(m);
}

public blueprint Slot { boolean found; unsigned64 index; };

static Slot find_slot_insert<K,V>(HashMap<K,V>* m, K key) {
    unsigned64 mask = m->capacity - 1;
    unsigned64 h = m->hash(m->context, key) & mask;
    unsigned64 i = h;
    unsigned64 first_tomb = (unsigned64)(~0ULL);
    while (true) {
        byte st = m->entries[i].state;
        if (st == 0) { // Empty
            Slot s; s.found=false; s.index=(first_tomb!=(unsigned64)(~0ULL)?first_tomb:i); return s;
        }
        if (st == 1) { if (m->equals(m->context, m->entries[i].key, key)) { Slot s; s.found=true; s.index=i; return s; } }
        else { if (first_tomb == (unsigned64)(~0ULL)) { first_tomb = i; } }
        i = (i + 1) & mask;
    }
}

static Slot find_slot_lookup<K,V>(HashMap<K,V>* m, K key) {
    unsigned64 mask = m->capacity - 1;
    unsigned64 i = m->hash(m->context, key) & mask;
    while (true) {
        byte st = m->entries[i].state;
        if (st == 0) { Slot s; s.found=false; s.index=i; return s; }
        if (st == 1 && m->equals(m->context, m->entries[i].key, key)) { Slot s; s.found=true; s.index=i; return s; }
        i = (i + 1) & mask;
    }
}

public res.Result<void, MapError> put<K,V>(HashMap<K,V>* m, K key, V value) {
    unsigned64 used = m->length + m->tombstones;
    if ((used * 100) >= (m->capacity * 70)) {
        auto rr = std.collections.hash_map_rehash.rehash<K,V>(m, m->capacity * 2);
        if (!rr.ok) { return rr; }
    }
    Slot s = find_slot_insert<K,V>(m, key);
    if (s.found) { m->entries[s.index].value = value; return res.ok<void, MapError>({}); }
    if (m->entries[s.index].state == 2) { m->tombstones -= 1; }
    m->entries[s.index].key = key;
    m->entries[s.index].value = value;
    m->entries[s.index].state = 1;
    m->length += 1;
    return res.ok<void, MapError>({});
}

public opt.Optional<pointer<V>> get_ref<K,V>(HashMap<K,V>* m, K key) {
    Slot s = find_slot_lookup<K,V>(m, key);
    if (!s.found) { return opt.none<pointer<V>>(); }
    return opt.some<pointer<V>>(&m->entries[s.index].value);
}

public opt.Optional<V> get<K,V>(HashMap<K,V>* m, K key) {
    Slot s = find_slot_lookup<K,V>(m, key);
    if (!s.found) { return opt.none<V>(); }
    return opt.some<V>(m->entries[s.index].value);
}
```

---

### std/collections/hash_map_delete.bolt
```bolt
package std.collections;
module hash_map_delete;

use std.collections.hash_map as hm;
use std.core as core;
use std.core.result as res;

public res.Result<boolean, hm.MapError> remove<K,V>(hm.HashMap<K,V>* m, K key) {
    hm.Slot s = hm.find_slot_lookup<K,V>(m, key);
    if (!s.found) { return res.ok<boolean, hm.MapError>(false); }
    if (m->entries[s.index].state == 1) {
        m->entries[s.index].state = 2; // Tombstone
        m->length -= 1;
        m->tombstones += 1;
        return res.ok<boolean, hm.MapError>(true);
    }
    return res.ok<boolean, hm.MapError>(false);
}
```

---

### std/collections/hash_map_rehash.bolt
```bolt
package std.collections;
module hash_map_rehash;

use std.collections.hash_map as hm;
use std.core as core;
use std.core.result as res;
use std.memory.allocator as alloc;

public res.Result<void, hm.MapError> rehash<K,V>(hm.HashMap<K,V>* m, unsigned64 new_capacity) {
    unsigned64 cap = new_capacity;
    if (cap < (m->length * 2)) { cap = m->length * 2; }
    cap -= 1; cap |= cap >> 1; cap |= cap >> 2; cap |= cap >> 4; cap |= cap >> 8; cap |= cap >> 16; cap |= cap >> 32; cap += 1;

    unsigned64 bytes = cap * sizeof(hm.Entry<K,V>);
    auto p = m->allocator.allocate(m->allocator.context, bytes, alignof(hm.Entry<K,V>));
    if (!p.present) { return res.error<void, hm.MapError>(hm.MapError.AllocationFailed); }

    pointer<hm.Entry<K,V>> new_entries = (pointer<hm.Entry<K,V>>)p.value;
    unsigned64 i = 0; while (i < cap) { new_entries[i].state = 0; i += 1; }

    unsigned64 mask = cap - 1;
    i = 0; while (i < m->capacity) {
        if (m->entries[i].state == 1) {
            K key = m->entries[i].key; V val = m->entries[i].value;
            unsigned64 j = m->hash(m->context, key) & mask;
            while (true) {
                if (new_entries[j].state == 0) { new_entries[j].key = key; new_entries[j].value = val; new_entries[j].state = 1; break; }
                j = (j + 1) & mask;
            }
        }
        i += 1;
    }

    m->allocator.deallocate(m->allocator.context, (pointer<byte>)m->entries, m->capacity * sizeof(hm.Entry<K,V>), alignof(hm.Entry<K,V>));
    m->entries = new_entries;
    m->capacity = cap;
    m->tombstones = 0;
    return res.ok<void, hm.MapError>({});
}
```

---

### std/logger.bolt
```bolt
package std; module logger;

use std.core as core;
use std.core.result as res;
use std.io.writer as writer;
use std.io.console as console;

public enum Level { trace, debug, info, warn, error };

public blueprint Output { writer.Writer sink; Level min_level; };

public blueprint Router { core.Slice<Output> outputs; Level level; };

// Global router. Implementations should install a Router at startup via configure().
static Router GLOBAL_LOGGER;

public void configure(core.Slice<Output> outputs, Level level) {
    GLOBAL_LOGGER.outputs = outputs;
    GLOBAL_LOGGER.level = level;
};

static boolean enabled(Level current, Level msg) {
    // Lower ordinal is more verbose; allow if msg >= current
    return ((unsigned64)msg) >= ((unsigned64)current);
};

static void write_line(writer.Writer* w, core.View<byte> v) {
    (void)writer.write_all(w, v);
    byte nl[1] = { '\n' };
    core.View<byte> newline = core.View<byte>{ data: (pointer<constant byte>)&nl[0], length: 1 };
    (void)writer.write_all(w, newline);
};

public void emit(Level level, core.View<byte> message) {
    Router* r = &GLOBAL_LOGGER;
    unsigned64 i = 0;
    while (i < r->outputs.length) {
        Output* out = r->outputs.data + i;
        if (enabled((out->min_level > r->level) ? out->min_level : r->level, level)) {
            writer.Writer w = out->sink;
            write_line(&w, message);
        }
        i += 1;
    }
};

public void trace(core.View<byte> message) { emit(Level.trace, message); };
public void debug(core.View<byte> message) { emit(Level.debug, message); };
public void info(core.View<byte> message)  { emit(Level.info,  message); };
public void warn(core.View<byte> message)  { emit(Level.warn,  message); };
public void error(core.View<byte> message) { emit(Level.error, message); };

// Convenience: route everything to the default console writer
public Output serial_output_full() {
    Output o; o.sink = console.out(); o.min_level = Level.trace; return o;
};
```
---

## 8. Simulation Backends (`std/sim`)
These modules provide user-mode simulation for the stdlib (time, scheduler, channels, console). They are **not** part of the kernel but let you test behavior without hardware.

### std/sim/time.bolt
```bolt
package std.sim;
module time;

use std.time.time as t;
use std.core.result as res;

static t.Instant NOW = t.Instant{ ticks: 0 };

public void advance_ticks(unsigned64 delta) { NOW.ticks += delta; }

public res.Result<t.Instant, unsigned32> monotonic() { return res.ok<t.Instant, unsigned32>(NOW); }
```

---

### std/sim/scheduler.bolt (cooperative)
```bolt
package std.sim;
module scheduler;

use std.concurrency.task as task;
use std.core.result as res;

public blueprint SimScheduler { unsigned64 ticks; };

static res.Result<task.TaskHandle, task.TaskError> spawn_stub(void* context, void (*entry)(void*), void* arg) {
    // In sim, we call immediately (single-threaded). Real schedulers would queue this.
    entry(arg);
    task.TaskHandle h; h.id = task.TaskId{ raw: 0 }; h.context = (void*)0; h.join = 0; h.detach = 0; return res.ok<task.TaskHandle, task.TaskError>(h);
}

static void yield_now_stub(void* context) { (void)context; }
static void sleep_ticks_stub(void* context, unsigned64 ticks) { (void)context; (void)ticks; }

public void install() {
    task.Scheduler s; s.context=(void*)0; s.spawn=spawn_stub; s.yield_now=yield_now_stub; s.sleep_ticks=sleep_ticks_stub; task.install(s);
}
```

---

### std/sim/channel.bolt
```bolt
package std.sim;
module channel;

use std.concurrency.channel as ch;
use std.core.result as res;
use std.memory.allocator as alloc;
use std.collections.ring_buffer as rb;

public blueprint ChannelContext<T> {
    rb.RingBuffer<T> ring;
    boolean closed;
};

static res.Result<void, ch.ChanError> send_impl<T>(void* vcontext, T value) {
    ChannelContext<T>* context = (ChannelContext<T>*)vcontext;
    if (context->closed) { return res.error<void, ch.ChanError>(ch.ChanError.Closed); }
    auto r = rb.push<T>(&context->ring, value);
    if (!r.ok) { return res.error<void, ch.ChanError>(ch.ChanError.Full); }
    return res.ok<void, ch.ChanError>({});
}

static res.Result<T, ch.ChanError> recv_impl<T>(void* vcontext) {
    ChannelContext<T>* context = (ChannelContext<T>*)vcontext;
    auto r = rb.pop<T>(&context->ring);
    if (!r.ok) { return res.error<T, ch.ChanError>(ch.ChanError.Empty); }
    return r;
}

static void close_impl<T>(void* vcontext) { ChannelContext<T>* context = (ChannelContext<T>*)vcontext; context->closed = true; }

public res.Result<ch.Channel<T>, ch.ChanError> make_bounded<T>(unsigned64 capacity, alloc.Allocator a) {
    auto ring = rb.create<T>(capacity, a);
    if (!ring.ok) { return res.error<ch.Channel<T>, ch.ChanError>(ch.ChanError.Full); }
    // allocate context
    auto mem = a.allocate(a.context, sizeof(ChannelContext<T>), alignof(ChannelContext<T>));
    if (!mem.present) { return res.error<ch.Channel<T>, ch.ChanError>(ch.ChanError.Full); }
    ChannelContext<T>* context = (ChannelContext<T>*)mem.value;
    context->ring = ring.value;
    context->closed = false;

    ch.Channel<T> c; c.context=(void*)context; c.send=send_impl<T>; c.receive=recv_impl<T>; c.close=close_impl<T>;
    return res.ok<ch.Channel<T>, ch.ChanError>(c);
}
```

---

### std/sim/fs_console.bolt
```bolt
package std.sim;
module fs_console;

use std.core as core;
use std.io.writer as wr;
use std.memory.view_span as vs;

// A memory-backed writer that collects bytes (for tests & demos)
public blueprint MemConsole {
    pointer<byte> buffer;
    unsigned64 capacity;
    unsigned64 cursor;
};

static res.Result<unsigned64, wr.WriteError> write_impl(void* vcontext, core.View<byte> src) {
    MemConsole* c = (MemConsole*)vcontext;
    unsigned64 space = c->capacity - c->cursor;
    unsigned64 n = (src.length < space) ? src.length : space;
    intrinsic_memcpy(c->buffer + c->cursor, (pointer<byte>)src.data, n);
    c->cursor += n;
    return res.ok<unsigned64, wr.WriteError>(n);
}

static res.Result<void, wr.WriteError> flush_impl(void* vcontext) { (void)vcontext; return res.ok<void, wr.WriteError>({}); }

public wr.Writer writer(MemConsole* c) {
    wr.Writer w; w.context=(void*)c; w.write=write_impl; w.flush=flush_impl; return w;
}

intrinsic void intrinsic_memcpy(pointer<byte> dst, pointer<byte> src, unsigned64 n);
```

---

## 9. Tests & Examples (reference)

### tests/logger_serial_demo.bolt
```bolt
package tests;
module logger_serial_demo;

use std.core as core;
use std.io.console as con;
use std.io.writer as wr;
use std.io.console_format as cf;
use std.logger as log;

public void main() {
    wr.Writer w = con.out();
    byte hello[] = { 'b','o','l','t',' ','o','n','!','\n' };
    core.View<byte> v = core.View<byte>{ data: (pointer<constant byte>)&hello[0], length: 9 };
    (void)wr.write_all(&w, v);

    // logger
    std.logger.Output outs[1];
    outs[0] = std.logger.serial_output_full();
    std.logger.Router r; r.level = std.logger.Level.info; r.outputs = core.Slice<std.logger.Output>{ data: &outs[0], length: 1 };
    std.logger.configure(r.outputs, r.level);

    byte msg[] = { 'h','e','l','l','o' };
    log.info(core.View<byte>{ data: (pointer<constant byte>)&msg[0], length: 5 });
}
```

---

### tests/hash_map_basic.bolt
```bolt
package tests;
module hash_map_basic;

use std.core as core;
use std.core.result as res;
use std.memory.allocator as alloc;
use std.collections.hash_map as hm;
use std.collections.hash_map_delete as hmdel;
use std.collections.hash_map_rehash as hmreh;

static unsigned64 hash_u64(void* context, unsigned64 k) { (void)context; return k * 11400714819323198485ULL; }
static boolean eq_u64(void* context, unsigned64 a, unsigned64 b) { (void)context; return a == b; }

public void demo() {
    alloc.Allocator a = alloc.system_allocator();
    auto m = hm.create<unsigned64, unsigned64>(16, a, (void*)0, hash_u64, eq_u64);
    if (!m.ok) { return; }
    unsigned64 i = 0; while (i < 100) { (void)hm.put<unsigned64,unsigned64>(&m.value, i, i+1); i += 1; }
    auto v = hm.get<unsigned64,unsigned64>(&m.value, 10);
    if (v.present) { /* ok */ }
    (void)hmdel.remove<unsigned64,unsigned64>(&m.value, 5);
    (void)hmreh.rehash<unsigned64,unsigned64>(&m.value, 256);
}
```

---

# Appendices (A–H)

## Appendix A: Naming, Formatting, and Conventions
- **Semicolons** mandatory; **K&R braces**.
- **Types first** for parameters and fields: `Type name`.
- **Module banner** at the top of every file: `package …; module …;`.
- **Interfaces** use a **context pointer and function pointers**.
- **No nested functions or closures**; use top‑level `static` helpers and pass context explicitly.
- **No abbreviations** in public surfaces (`context`, not `ctx`).

## Appendix B: Reserved Keywords
`package, module, public, blueprint, enum, constant, use, external, intrinsic, return, if, else, while, switch, case, break, continue, true, false, null, static, sizeof, alignof`.

## Appendix C: Required Intrinsics (Minimum Set)
- `intrinsic void intrinsic_memcpy(pointer<byte> dst, pointer<byte> src, unsigned64 n);`
- `intrinsic void intrinsic_memset(pointer<byte> dst, byte value, unsigned64 n);`
- Targets may extend with `intrinsic_memmove`, `intrinsic_cmpxchg`, etc., but the above two are mandatory.

## Appendix D: Result & Optional Interoperability
- `Result<void, E>`: treat `.value` as an empty object; never read from it.
- `Optional<T>`: read `.value` only when `present == true`.
- In stdlib code paths, never silently discard non‑`ok` results.

## Appendix E: Conformance Checklist (Compiler/Runtime)
- [ ] Enforce context‑and‑function‑pointer surfaces for `Allocator`, `Reader`, `Writer`, `Output`.
- [ ] Reject nested function definitions.
- [ ] Require square‑bracket **kernel markers** on all externals that touch platform or hardware.
- [ ] Toolchain flags enable freestanding builds (equivalent to `-ffreestanding`).
- [ ] Boot subset compiles with no allocator references.
- [ ] Stdlib compiles with no implicit dynamic allocation in hot paths.

## Appendix F: Compile‑Time Operators & Casts
- `sizeof(T)`: returns byte size of type `T`. Allowed in constant expressions.
- `alignof(T)`: returns ABI alignment of `T` in bytes. Allowed in constant expressions.
- **C‑style casts:** `(T) expression` allowed between pointers, integers, and `byte`. No implicit narrowing.

## Appendix G: Arrays & Initializers
- Local and static arrays: `byte buf[32];` and initializer lists `byte hello[] = { 'h','i' };` are allowed.
- Arrays decay to pointers when passed as parameters.
- Prefer `View<byte>` and `Span<byte>` in APIs.

## Appendix H: Option & Result Naming
- Import options via `use std.core.option as opt;` when constructing or returning optionals.
- Use `opt.Optional<T>`, `opt.some`, and `opt.none` consistently across the standard library.
