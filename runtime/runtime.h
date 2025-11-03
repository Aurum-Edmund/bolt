#pragma once

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_MSC_VER)
#    define BOLT_NORETURN __declspec(noreturn)
#else
#    define BOLT_NORETURN __attribute__((noreturn))
#endif

/*
 * Host-agnostic panic routine. Terminates the program without unwinding.
 * Implementations may emit diagnostics when available but must never return.
 */
BOLT_NORETURN void bolt_panic_abort(const char* message);

/*
 * Memory copy helper (see glossary "Memory Copy").
 * Copies `bytes` from source to destination and returns destination.
 * Overlapping regions are supported and copied safely.
 */
void* bolt_memory_copy(void* destination, const void* source, size_t bytes);

/*
 * Memory fill helper (see glossary "Memory Fill").
 * Fills `bytes` bytes at destination with the provided byte value.
 * Returns destination.
 */
void* bolt_memory_fill(void* destination, int value, size_t bytes);

/*
 * Allocates zero-initialized storage of `size` bytes. Returns NULL when the
 * host cannot satisfy the allocation request. Callers must release the
 * allocation with `bolt_delete` when the storage is no longer required.
 */
void* bolt_new(size_t size);

/* Releases memory obtained via `bolt_new`. The pointer may be NULL. */
void bolt_delete(void* memory);

/* Smart pointer support --------------------------------------------------- */

typedef void (*boltSharedPointerDestructor)(void* payload);

typedef struct boltSharedPointerControlBlock boltSharedPointerControlBlock;

typedef struct boltSharedPointer
{
    void* payload;
    boltSharedPointerControlBlock* control;
} boltSharedPointer;

/*
 * Creates a managed pointer around `payload`. The payload is expected to be a
 * valid allocation obtained through `bolt_new` or a compatible allocator.
 * When the final reference is released the optional destructor is invoked; if
 * NULL, `bolt_delete` is used. Returns an invalid pointer when payload is NULL
 * or the control block cannot be allocated.
 */
boltSharedPointer bolt_shared_pointer_make(void* payload, boltSharedPointerDestructor destructor);

/* Returns true when the pointer owns a payload. */
bool bolt_shared_pointer_is_valid(const boltSharedPointer* pointer);

/* Returns the managed payload or NULL when the pointer is invalid. */
void* bolt_shared_pointer_get(const boltSharedPointer* pointer);

/*
 * Creates an additional reference to the payload. Copying is explicit—callers
 * receive a fresh `boltSharedPointer` instance with its own lifetime.
 */
boltSharedPointer bolt_shared_pointer_copy(const boltSharedPointer* pointer);

/*
 * Transfers ownership from `pointer` into the returned instance. The source is
 * invalidated, providing an explicit move semantic.
 */
boltSharedPointer bolt_shared_pointer_move(boltSharedPointer* pointer);

/* Releases the reference held by `pointer` and invalidates it. */
void bolt_shared_pointer_release(boltSharedPointer* pointer);

/*
 * Memory ordering used by the Bolt atomic helper APIs. Mirrors the
 * specification terminology while mapping onto the host's atomic
 * primitives.
 */
typedef enum boltAtomicOrder
{
    boltAtomicOrderRelaxed = 0,
    boltAtomicOrderAcquire,
    boltAtomicOrderRelease,
    boltAtomicOrderAcquireRelease,
    boltAtomicOrderSequentiallyConsistent
} boltAtomicOrder;

/* Atomic 8-bit helpers */
uint8_t bolt_atomic_load_u8(const volatile uint8_t* object, boltAtomicOrder order);
void bolt_atomic_store_u8(volatile uint8_t* object, uint8_t value, boltAtomicOrder order);
uint8_t bolt_atomic_exchange_u8(volatile uint8_t* object, uint8_t value, boltAtomicOrder order);
uint8_t bolt_atomic_fetch_add_u8(volatile uint8_t* object, uint8_t value, boltAtomicOrder order);
uint8_t bolt_atomic_fetch_sub_u8(volatile uint8_t* object, uint8_t value, boltAtomicOrder order);
uint8_t bolt_atomic_fetch_and_u8(volatile uint8_t* object, uint8_t value, boltAtomicOrder order);
uint8_t bolt_atomic_fetch_or_u8(volatile uint8_t* object, uint8_t value, boltAtomicOrder order);
uint8_t bolt_atomic_fetch_xor_u8(volatile uint8_t* object, uint8_t value, boltAtomicOrder order);
bool bolt_atomic_compare_exchange_u8(volatile uint8_t* object,
    uint8_t* expected,
    uint8_t desired,
    boltAtomicOrder successOrder,
    boltAtomicOrder failureOrder);

/* Atomic 16-bit helpers */
uint16_t bolt_atomic_load_u16(const volatile uint16_t* object, boltAtomicOrder order);
void bolt_atomic_store_u16(volatile uint16_t* object, uint16_t value, boltAtomicOrder order);
uint16_t bolt_atomic_exchange_u16(volatile uint16_t* object, uint16_t value, boltAtomicOrder order);
uint16_t bolt_atomic_fetch_add_u16(volatile uint16_t* object, uint16_t value, boltAtomicOrder order);
uint16_t bolt_atomic_fetch_sub_u16(volatile uint16_t* object, uint16_t value, boltAtomicOrder order);
uint16_t bolt_atomic_fetch_and_u16(volatile uint16_t* object, uint16_t value, boltAtomicOrder order);
uint16_t bolt_atomic_fetch_or_u16(volatile uint16_t* object, uint16_t value, boltAtomicOrder order);
uint16_t bolt_atomic_fetch_xor_u16(volatile uint16_t* object, uint16_t value, boltAtomicOrder order);
bool bolt_atomic_compare_exchange_u16(volatile uint16_t* object,
    uint16_t* expected,
    uint16_t desired,
    boltAtomicOrder successOrder,
    boltAtomicOrder failureOrder);

/* Atomic 32-bit helpers */
uint32_t bolt_atomic_load_u32(const volatile uint32_t* object, boltAtomicOrder order);
void bolt_atomic_store_u32(volatile uint32_t* object, uint32_t value, boltAtomicOrder order);
uint32_t bolt_atomic_exchange_u32(volatile uint32_t* object, uint32_t value, boltAtomicOrder order);
uint32_t bolt_atomic_fetch_add_u32(volatile uint32_t* object, uint32_t value, boltAtomicOrder order);
uint32_t bolt_atomic_fetch_sub_u32(volatile uint32_t* object, uint32_t value, boltAtomicOrder order);
uint32_t bolt_atomic_fetch_and_u32(volatile uint32_t* object, uint32_t value, boltAtomicOrder order);
uint32_t bolt_atomic_fetch_or_u32(volatile uint32_t* object, uint32_t value, boltAtomicOrder order);
uint32_t bolt_atomic_fetch_xor_u32(volatile uint32_t* object, uint32_t value, boltAtomicOrder order);
bool bolt_atomic_compare_exchange_u32(volatile uint32_t* object,
    uint32_t* expected,
    uint32_t desired,
    boltAtomicOrder successOrder,
    boltAtomicOrder failureOrder);

/* Atomic 64-bit helpers */
uint64_t bolt_atomic_load_u64(const volatile uint64_t* object, boltAtomicOrder order);
void bolt_atomic_store_u64(volatile uint64_t* object, uint64_t value, boltAtomicOrder order);
uint64_t bolt_atomic_exchange_u64(volatile uint64_t* object, uint64_t value, boltAtomicOrder order);
uint64_t bolt_atomic_fetch_add_u64(volatile uint64_t* object, uint64_t value, boltAtomicOrder order);
uint64_t bolt_atomic_fetch_sub_u64(volatile uint64_t* object, uint64_t value, boltAtomicOrder order);
uint64_t bolt_atomic_fetch_and_u64(volatile uint64_t* object, uint64_t value, boltAtomicOrder order);
uint64_t bolt_atomic_fetch_or_u64(volatile uint64_t* object, uint64_t value, boltAtomicOrder order);
uint64_t bolt_atomic_fetch_xor_u64(volatile uint64_t* object, uint64_t value, boltAtomicOrder order);
bool bolt_atomic_compare_exchange_u64(volatile uint64_t* object,
    uint64_t* expected,
    uint64_t desired,
    boltAtomicOrder successOrder,
    boltAtomicOrder failureOrder);

#ifdef __cplusplus
}
#endif

