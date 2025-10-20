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
 * The regions must not overlap.
 */
void* bolt_memory_copy(void* destination, const void* source, size_t bytes);

/*
 * Memory fill helper (see glossary "Memory Fill").
 * Fills `bytes` bytes at destination with the provided byte value.
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
bool bolt_atomic_compare_exchange_u8(volatile uint8_t* object,
    uint8_t* expected,
    uint8_t desired,
    boltAtomicOrder successOrder,
    boltAtomicOrder failureOrder);

/* Atomic 16-bit helpers */
uint16_t bolt_atomic_load_u16(const volatile uint16_t* object, boltAtomicOrder order);
void bolt_atomic_store_u16(volatile uint16_t* object, uint16_t value, boltAtomicOrder order);
uint16_t bolt_atomic_exchange_u16(volatile uint16_t* object, uint16_t value, boltAtomicOrder order);
bool bolt_atomic_compare_exchange_u16(volatile uint16_t* object,
    uint16_t* expected,
    uint16_t desired,
    boltAtomicOrder successOrder,
    boltAtomicOrder failureOrder);

/* Atomic 32-bit helpers */
uint32_t bolt_atomic_load_u32(const volatile uint32_t* object, boltAtomicOrder order);
void bolt_atomic_store_u32(volatile uint32_t* object, uint32_t value, boltAtomicOrder order);
uint32_t bolt_atomic_exchange_u32(volatile uint32_t* object, uint32_t value, boltAtomicOrder order);
bool bolt_atomic_compare_exchange_u32(volatile uint32_t* object,
    uint32_t* expected,
    uint32_t desired,
    boltAtomicOrder successOrder,
    boltAtomicOrder failureOrder);

/* Atomic 64-bit helpers */
uint64_t bolt_atomic_load_u64(const volatile uint64_t* object, boltAtomicOrder order);
void bolt_atomic_store_u64(volatile uint64_t* object, uint64_t value, boltAtomicOrder order);
uint64_t bolt_atomic_exchange_u64(volatile uint64_t* object, uint64_t value, boltAtomicOrder order);
bool bolt_atomic_compare_exchange_u64(volatile uint64_t* object,
    uint64_t* expected,
    uint64_t desired,
    boltAtomicOrder successOrder,
    boltAtomicOrder failureOrder);

alue.
 * Returns destination.
 */
void* bolt_memory_fill(void* destination, int value, size_t bytes);

#ifdef __cplusplus
}
#endif

