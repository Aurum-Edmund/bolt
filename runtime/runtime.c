#include "runtime.h"

#include <stdbool.h>
#include <stdint.h>
#if defined(_MSC_VER) && !defined(__clang__)
#    include <intrin.h>
#else
#    include <stdatomic.h>
#endif

#if defined(_MSC_VER)
#    include <intrin.h>
#endif

BOLT_NORETURN void bolt_panic_abort(const char* message)
{
    (void)message;
#if defined(_MSC_VER)
    __fastfail(1);
#else
    __builtin_trap();
    for (;;)
    {
    }
#endif
}

void* bolt_memory_copy(void* destination, const void* source, size_t bytes)
{
    unsigned char* dst = (unsigned char*)destination;
    const unsigned char* src = (const unsigned char*)source;
    for (size_t index = 0; index < bytes; ++index)
    {
        dst[index] = src[index];
    }
    return destination;
}

void* bolt_memory_fill(void* destination, int value, size_t bytes)
{
    unsigned char* dst = (unsigned char*)destination;
    const unsigned char byteValue = (unsigned char)(value & 0xFF);
    for (size_t index = 0; index < bytes; ++index)
    {
        dst[index] = byteValue;
    }
    return destination;
}
#if defined(_MSC_VER) && !defined(__clang__)
static void bolt_atomic_apply_release(boltAtomicOrder order)
{
    (void)order;
    // Windows interlocked primitives provide sequential consistency. No
    // additional fencing is required beyond a compiler barrier.
    _ReadWriteBarrier();
}

static void bolt_atomic_apply_acquire(boltAtomicOrder order)
{
    (void)order;
    _ReadWriteBarrier();
}

uint8_t bolt_atomic_load_u8(const volatile uint8_t* object, boltAtomicOrder order)
{
    char value = _InterlockedCompareExchange8((volatile char*)object, 0, 0);
    bolt_atomic_apply_acquire(order);
    return (uint8_t)value;
}

void bolt_atomic_store_u8(volatile uint8_t* object, uint8_t value, boltAtomicOrder order)
{
    bolt_atomic_apply_release(order);
    _InterlockedExchange8((volatile char*)object, (char)value);
}

uint8_t bolt_atomic_exchange_u8(volatile uint8_t* object, uint8_t value, boltAtomicOrder order)
{
    bolt_atomic_apply_release(order);
    char previous = _InterlockedExchange8((volatile char*)object, (char)value);
    bolt_atomic_apply_acquire(order);
    return (uint8_t)previous;
}

uint8_t bolt_atomic_fetch_add_u8(volatile uint8_t* object, uint8_t value, boltAtomicOrder order)
{
    bolt_atomic_apply_release(order);
    char previous = _InterlockedExchangeAdd8((volatile char*)object, (char)value);
    bolt_atomic_apply_acquire(order);
    return (uint8_t)previous;
}

uint8_t bolt_atomic_fetch_sub_u8(volatile uint8_t* object, uint8_t value, boltAtomicOrder order)
{
    bolt_atomic_apply_release(order);
    unsigned char negated = (unsigned char)(0u - (unsigned int)value);
    char previous = _InterlockedExchangeAdd8((volatile char*)object, (char)negated);
    bolt_atomic_apply_acquire(order);
    return (uint8_t)previous;
}

uint8_t bolt_atomic_fetch_and_u8(volatile uint8_t* object, uint8_t value, boltAtomicOrder order)
{
    bolt_atomic_apply_release(order);
    char previous = _InterlockedAnd8((volatile char*)object, (char)value);
    bolt_atomic_apply_acquire(order);
    return (uint8_t)previous;
}

uint8_t bolt_atomic_fetch_or_u8(volatile uint8_t* object, uint8_t value, boltAtomicOrder order)
{
    bolt_atomic_apply_release(order);
    char previous = _InterlockedOr8((volatile char*)object, (char)value);
    bolt_atomic_apply_acquire(order);
    return (uint8_t)previous;
}

uint8_t bolt_atomic_fetch_xor_u8(volatile uint8_t* object, uint8_t value, boltAtomicOrder order)
{
    bolt_atomic_apply_release(order);
    char previous = _InterlockedXor8((volatile char*)object, (char)value);
    bolt_atomic_apply_acquire(order);
    return (uint8_t)previous;
}

bool bolt_atomic_compare_exchange_u8(volatile uint8_t* object,
    uint8_t* expected,
    uint8_t desired,
    boltAtomicOrder successOrder,
    boltAtomicOrder failureOrder)
{
    bolt_atomic_apply_release(successOrder);
    char prior = _InterlockedCompareExchange8((volatile char*)object, (char)desired, (char)(*expected));
    if (prior == (char)(*expected))
    {
        bolt_atomic_apply_acquire(successOrder);
        return true;
    }

    bolt_atomic_apply_acquire(failureOrder);
    *expected = (uint8_t)prior;
    return false;
}

uint16_t bolt_atomic_load_u16(const volatile uint16_t* object, boltAtomicOrder order)
{
    short value = _InterlockedCompareExchange16((volatile short*)object, 0, 0);
    bolt_atomic_apply_acquire(order);
    return (uint16_t)value;
}

void bolt_atomic_store_u16(volatile uint16_t* object, uint16_t value, boltAtomicOrder order)
{
    bolt_atomic_apply_release(order);
    _InterlockedExchange16((volatile short*)object, (short)value);
}

uint16_t bolt_atomic_exchange_u16(volatile uint16_t* object, uint16_t value, boltAtomicOrder order)
{
    bolt_atomic_apply_release(order);
    short previous = _InterlockedExchange16((volatile short*)object, (short)value);
    bolt_atomic_apply_acquire(order);
    return (uint16_t)previous;
}

uint16_t bolt_atomic_fetch_add_u16(volatile uint16_t* object, uint16_t value, boltAtomicOrder order)
{
    bolt_atomic_apply_release(order);
    short previous = _InterlockedExchangeAdd16((volatile short*)object, (short)value);
    bolt_atomic_apply_acquire(order);
    return (uint16_t)previous;
}

uint16_t bolt_atomic_fetch_sub_u16(volatile uint16_t* object, uint16_t value, boltAtomicOrder order)
{
    bolt_atomic_apply_release(order);
    unsigned short negated = (unsigned short)(0u - (unsigned int)value);
    short previous = _InterlockedExchangeAdd16((volatile short*)object, (short)negated);
    bolt_atomic_apply_acquire(order);
    return (uint16_t)previous;
}

uint16_t bolt_atomic_fetch_and_u16(volatile uint16_t* object, uint16_t value, boltAtomicOrder order)
{
    bolt_atomic_apply_release(order);
    short previous = _InterlockedAnd16((volatile short*)object, (short)value);
    bolt_atomic_apply_acquire(order);
    return (uint16_t)previous;
}

uint16_t bolt_atomic_fetch_or_u16(volatile uint16_t* object, uint16_t value, boltAtomicOrder order)
{
    bolt_atomic_apply_release(order);
    short previous = _InterlockedOr16((volatile short*)object, (short)value);
    bolt_atomic_apply_acquire(order);
    return (uint16_t)previous;
}

uint16_t bolt_atomic_fetch_xor_u16(volatile uint16_t* object, uint16_t value, boltAtomicOrder order)
{
    bolt_atomic_apply_release(order);
    short previous = _InterlockedXor16((volatile short*)object, (short)value);
    bolt_atomic_apply_acquire(order);
    return (uint16_t)previous;
}

bool bolt_atomic_compare_exchange_u16(volatile uint16_t* object,
    uint16_t* expected,
    uint16_t desired,
    boltAtomicOrder successOrder,
    boltAtomicOrder failureOrder)
{
    bolt_atomic_apply_release(successOrder);
    short prior = _InterlockedCompareExchange16((volatile short*)object, (short)desired, (short)(*expected));
    if (prior == (short)(*expected))
    {
        bolt_atomic_apply_acquire(successOrder);
        return true;
    }

    bolt_atomic_apply_acquire(failureOrder);
    *expected = (uint16_t)prior;
    return false;
}

uint32_t bolt_atomic_load_u32(const volatile uint32_t* object, boltAtomicOrder order)
{
    uint32_t value = (uint32_t)_InterlockedCompareExchange((volatile long*)object, 0L, 0L);
    bolt_atomic_apply_acquire(order);
    return value;
}

void bolt_atomic_store_u32(volatile uint32_t* object, uint32_t value, boltAtomicOrder order)
{
    bolt_atomic_apply_release(order);
    _InterlockedExchange((volatile long*)object, (long)value);
}

uint32_t bolt_atomic_exchange_u32(volatile uint32_t* object, uint32_t value, boltAtomicOrder order)
{
    bolt_atomic_apply_release(order);
    long previous = _InterlockedExchange((volatile long*)object, (long)value);
    bolt_atomic_apply_acquire(order);
    return (uint32_t)previous;
}

uint32_t bolt_atomic_fetch_add_u32(volatile uint32_t* object, uint32_t value, boltAtomicOrder order)
{
    bolt_atomic_apply_release(order);
    long previous = _InterlockedExchangeAdd((volatile long*)object, (long)value);
    bolt_atomic_apply_acquire(order);
    return (uint32_t)previous;
}

uint32_t bolt_atomic_fetch_sub_u32(volatile uint32_t* object, uint32_t value, boltAtomicOrder order)
{
    bolt_atomic_apply_release(order);
    unsigned long negated = (unsigned long)(0u - value);
    long previous = _InterlockedExchangeAdd((volatile long*)object, (long)negated);
    bolt_atomic_apply_acquire(order);
    return (uint32_t)previous;
}

uint32_t bolt_atomic_fetch_and_u32(volatile uint32_t* object, uint32_t value, boltAtomicOrder order)
{
    bolt_atomic_apply_release(order);
    long previous = _InterlockedAnd((volatile long*)object, (long)value);
    bolt_atomic_apply_acquire(order);
    return (uint32_t)previous;
}

uint32_t bolt_atomic_fetch_or_u32(volatile uint32_t* object, uint32_t value, boltAtomicOrder order)
{
    bolt_atomic_apply_release(order);
    long previous = _InterlockedOr((volatile long*)object, (long)value);
    bolt_atomic_apply_acquire(order);
    return (uint32_t)previous;
}

uint32_t bolt_atomic_fetch_xor_u32(volatile uint32_t* object, uint32_t value, boltAtomicOrder order)
{
    bolt_atomic_apply_release(order);
    long previous = _InterlockedXor((volatile long*)object, (long)value);
    bolt_atomic_apply_acquire(order);
    return (uint32_t)previous;
}

bool bolt_atomic_compare_exchange_u32(volatile uint32_t* object,
    uint32_t* expected,
    uint32_t desired,
    boltAtomicOrder successOrder,
    boltAtomicOrder failureOrder)
{
    bolt_atomic_apply_release(successOrder);
    long prior = _InterlockedCompareExchange((volatile long*)object, (long)desired, (long)(*expected));
    if (prior == (long)(*expected))
    {
        bolt_atomic_apply_acquire(successOrder);
        return true;
    }

    bolt_atomic_apply_acquire(failureOrder);
    *expected = (uint32_t)prior;
    return false;
}

uint64_t bolt_atomic_load_u64(const volatile uint64_t* object, boltAtomicOrder order)
{
    uint64_t value = (uint64_t)_InterlockedCompareExchange64((volatile long long*)object, 0LL, 0LL);
    bolt_atomic_apply_acquire(order);
    return value;
}

void bolt_atomic_store_u64(volatile uint64_t* object, uint64_t value, boltAtomicOrder order)
{
    bolt_atomic_apply_release(order);
    _InterlockedExchange64((volatile long long*)object, (long long)value);
}

uint64_t bolt_atomic_exchange_u64(volatile uint64_t* object, uint64_t value, boltAtomicOrder order)
{
    bolt_atomic_apply_release(order);
    long long previous = _InterlockedExchange64((volatile long long*)object, (long long)value);
    bolt_atomic_apply_acquire(order);
    return (uint64_t)previous;
}

uint64_t bolt_atomic_fetch_add_u64(volatile uint64_t* object, uint64_t value, boltAtomicOrder order)
{
    bolt_atomic_apply_release(order);
    long long previous = _InterlockedExchangeAdd64((volatile long long*)object, (long long)value);
    bolt_atomic_apply_acquire(order);
    return (uint64_t)previous;
}

uint64_t bolt_atomic_fetch_sub_u64(volatile uint64_t* object, uint64_t value, boltAtomicOrder order)
{
    bolt_atomic_apply_release(order);
    unsigned long long negated = 0ull - value;
    long long previous = _InterlockedExchangeAdd64((volatile long long*)object, (long long)negated);
    bolt_atomic_apply_acquire(order);
    return (uint64_t)previous;
}

uint64_t bolt_atomic_fetch_and_u64(volatile uint64_t* object, uint64_t value, boltAtomicOrder order)
{
    bolt_atomic_apply_release(order);
    long long previous = _InterlockedAnd64((volatile long long*)object, (long long)value);
    bolt_atomic_apply_acquire(order);
    return (uint64_t)previous;
}

uint64_t bolt_atomic_fetch_or_u64(volatile uint64_t* object, uint64_t value, boltAtomicOrder order)
{
    bolt_atomic_apply_release(order);
    long long previous = _InterlockedOr64((volatile long long*)object, (long long)value);
    bolt_atomic_apply_acquire(order);
    return (uint64_t)previous;
}

uint64_t bolt_atomic_fetch_xor_u64(volatile uint64_t* object, uint64_t value, boltAtomicOrder order)
{
    bolt_atomic_apply_release(order);
    long long previous = _InterlockedXor64((volatile long long*)object, (long long)value);
    bolt_atomic_apply_acquire(order);
    return (uint64_t)previous;
}

bool bolt_atomic_compare_exchange_u64(volatile uint64_t* object,
    uint64_t* expected,
    uint64_t desired,
    boltAtomicOrder successOrder,
    boltAtomicOrder failureOrder)
{
    bolt_atomic_apply_release(successOrder);
    long long prior = _InterlockedCompareExchange64((volatile long long*)object, (long long)desired, (long long)(*expected));
    if (prior == (long long)(*expected))
    {
        bolt_atomic_apply_acquire(successOrder);
        return true;
    }

    bolt_atomic_apply_acquire(failureOrder);
    *expected = (uint64_t)prior;
    return false;
}

#else
static memory_order bolt_atomic_to_memory_order(boltAtomicOrder order)
{
    switch (order)
    {
    case boltAtomicOrderRelaxed:
        return memory_order_relaxed;
    case boltAtomicOrderAcquire:
        return memory_order_acquire;
    case boltAtomicOrderRelease:
        return memory_order_release;
    case boltAtomicOrderAcquireRelease:
        return memory_order_acq_rel;
    case boltAtomicOrderSequentiallyConsistent:
    default:
        return memory_order_seq_cst;
    }
}

static memory_order bolt_atomic_failure_order(boltAtomicOrder order)
{
    switch (order)
    {
    case boltAtomicOrderRelaxed:
        return memory_order_relaxed;
    case boltAtomicOrderAcquire:
        return memory_order_acquire;
    case boltAtomicOrderRelease:
        return memory_order_relaxed;
    case boltAtomicOrderAcquireRelease:
        return memory_order_acquire;
    case boltAtomicOrderSequentiallyConsistent:
    default:
        return memory_order_seq_cst;
    }
}

uint8_t bolt_atomic_load_u8(const volatile uint8_t* object, boltAtomicOrder order)
{
    const volatile _Atomic uint8_t* atomicObject = (const volatile _Atomic uint8_t*)object;
    return atomic_load_explicit(atomicObject, bolt_atomic_to_memory_order(order));
}

void bolt_atomic_store_u8(volatile uint8_t* object, uint8_t value, boltAtomicOrder order)
{
    volatile _Atomic uint8_t* atomicObject = (volatile _Atomic uint8_t*)object;
    atomic_store_explicit(atomicObject, value, bolt_atomic_to_memory_order(order));
}

uint8_t bolt_atomic_exchange_u8(volatile uint8_t* object, uint8_t value, boltAtomicOrder order)
{
    volatile _Atomic uint8_t* atomicObject = (volatile _Atomic uint8_t*)object;
    return atomic_exchange_explicit(atomicObject, value, bolt_atomic_to_memory_order(order));
}

uint8_t bolt_atomic_fetch_add_u8(volatile uint8_t* object, uint8_t value, boltAtomicOrder order)
{
    volatile _Atomic uint8_t* atomicObject = (volatile _Atomic uint8_t*)object;
    return atomic_fetch_add_explicit(atomicObject, value, bolt_atomic_to_memory_order(order));
}

uint8_t bolt_atomic_fetch_sub_u8(volatile uint8_t* object, uint8_t value, boltAtomicOrder order)
{
    volatile _Atomic uint8_t* atomicObject = (volatile _Atomic uint8_t*)object;
    return atomic_fetch_sub_explicit(atomicObject, value, bolt_atomic_to_memory_order(order));
}

uint8_t bolt_atomic_fetch_and_u8(volatile uint8_t* object, uint8_t value, boltAtomicOrder order)
{
    volatile _Atomic uint8_t* atomicObject = (volatile _Atomic uint8_t*)object;
    return atomic_fetch_and_explicit(atomicObject, value, bolt_atomic_to_memory_order(order));
}

uint8_t bolt_atomic_fetch_or_u8(volatile uint8_t* object, uint8_t value, boltAtomicOrder order)
{
    volatile _Atomic uint8_t* atomicObject = (volatile _Atomic uint8_t*)object;
    return atomic_fetch_or_explicit(atomicObject, value, bolt_atomic_to_memory_order(order));
}

uint8_t bolt_atomic_fetch_xor_u8(volatile uint8_t* object, uint8_t value, boltAtomicOrder order)
{
    volatile _Atomic uint8_t* atomicObject = (volatile _Atomic uint8_t*)object;
    return atomic_fetch_xor_explicit(atomicObject, value, bolt_atomic_to_memory_order(order));
}

bool bolt_atomic_compare_exchange_u8(volatile uint8_t* object,
    uint8_t* expected,
    uint8_t desired,
    boltAtomicOrder successOrder,
    boltAtomicOrder failureOrder)
{
    volatile _Atomic uint8_t* atomicObject = (volatile _Atomic uint8_t*)object;
    return atomic_compare_exchange_strong_explicit(atomicObject,
        expected,
        desired,
        bolt_atomic_to_memory_order(successOrder),
        bolt_atomic_failure_order(failureOrder));
}

uint16_t bolt_atomic_load_u16(const volatile uint16_t* object, boltAtomicOrder order)
{
    const volatile _Atomic uint16_t* atomicObject = (const volatile _Atomic uint16_t*)object;
    return atomic_load_explicit(atomicObject, bolt_atomic_to_memory_order(order));
}

void bolt_atomic_store_u16(volatile uint16_t* object, uint16_t value, boltAtomicOrder order)
{
    volatile _Atomic uint16_t* atomicObject = (volatile _Atomic uint16_t*)object;
    atomic_store_explicit(atomicObject, value, bolt_atomic_to_memory_order(order));
}

uint16_t bolt_atomic_exchange_u16(volatile uint16_t* object, uint16_t value, boltAtomicOrder order)
{
    volatile _Atomic uint16_t* atomicObject = (volatile _Atomic uint16_t*)object;
    return atomic_exchange_explicit(atomicObject, value, bolt_atomic_to_memory_order(order));
}

uint16_t bolt_atomic_fetch_add_u16(volatile uint16_t* object, uint16_t value, boltAtomicOrder order)
{
    volatile _Atomic uint16_t* atomicObject = (volatile _Atomic uint16_t*)object;
    return atomic_fetch_add_explicit(atomicObject, value, bolt_atomic_to_memory_order(order));
}

uint16_t bolt_atomic_fetch_sub_u16(volatile uint16_t* object, uint16_t value, boltAtomicOrder order)
{
    volatile _Atomic uint16_t* atomicObject = (volatile _Atomic uint16_t*)object;
    return atomic_fetch_sub_explicit(atomicObject, value, bolt_atomic_to_memory_order(order));
}

uint16_t bolt_atomic_fetch_and_u16(volatile uint16_t* object, uint16_t value, boltAtomicOrder order)
{
    volatile _Atomic uint16_t* atomicObject = (volatile _Atomic uint16_t*)object;
    return atomic_fetch_and_explicit(atomicObject, value, bolt_atomic_to_memory_order(order));
}

uint16_t bolt_atomic_fetch_or_u16(volatile uint16_t* object, uint16_t value, boltAtomicOrder order)
{
    volatile _Atomic uint16_t* atomicObject = (volatile _Atomic uint16_t*)object;
    return atomic_fetch_or_explicit(atomicObject, value, bolt_atomic_to_memory_order(order));
}

uint16_t bolt_atomic_fetch_xor_u16(volatile uint16_t* object, uint16_t value, boltAtomicOrder order)
{
    volatile _Atomic uint16_t* atomicObject = (volatile _Atomic uint16_t*)object;
    return atomic_fetch_xor_explicit(atomicObject, value, bolt_atomic_to_memory_order(order));
}

bool bolt_atomic_compare_exchange_u16(volatile uint16_t* object,
    uint16_t* expected,
    uint16_t desired,
    boltAtomicOrder successOrder,
    boltAtomicOrder failureOrder)
{
    volatile _Atomic uint16_t* atomicObject = (volatile _Atomic uint16_t*)object;
    return atomic_compare_exchange_strong_explicit(atomicObject,
        expected,
        desired,
        bolt_atomic_to_memory_order(successOrder),
        bolt_atomic_failure_order(failureOrder));
}

uint32_t bolt_atomic_load_u32(const volatile uint32_t* object, boltAtomicOrder order)
{
    const volatile _Atomic uint32_t* atomicObject = (const volatile _Atomic uint32_t*)object;
    return atomic_load_explicit(atomicObject, bolt_atomic_to_memory_order(order));
}

void bolt_atomic_store_u32(volatile uint32_t* object, uint32_t value, boltAtomicOrder order)
{
    volatile _Atomic uint32_t* atomicObject = (volatile _Atomic uint32_t*)object;
    atomic_store_explicit(atomicObject, value, bolt_atomic_to_memory_order(order));
}

uint32_t bolt_atomic_exchange_u32(volatile uint32_t* object, uint32_t value, boltAtomicOrder order)
{
    volatile _Atomic uint32_t* atomicObject = (volatile _Atomic uint32_t*)object;
    return atomic_exchange_explicit(atomicObject, value, bolt_atomic_to_memory_order(order));
}

uint32_t bolt_atomic_fetch_add_u32(volatile uint32_t* object, uint32_t value, boltAtomicOrder order)
{
    volatile _Atomic uint32_t* atomicObject = (volatile _Atomic uint32_t*)object;
    return atomic_fetch_add_explicit(atomicObject, value, bolt_atomic_to_memory_order(order));
}

uint32_t bolt_atomic_fetch_sub_u32(volatile uint32_t* object, uint32_t value, boltAtomicOrder order)
{
    volatile _Atomic uint32_t* atomicObject = (volatile _Atomic uint32_t*)object;
    return atomic_fetch_sub_explicit(atomicObject, value, bolt_atomic_to_memory_order(order));
}

uint32_t bolt_atomic_fetch_and_u32(volatile uint32_t* object, uint32_t value, boltAtomicOrder order)
{
    volatile _Atomic uint32_t* atomicObject = (volatile _Atomic uint32_t*)object;
    return atomic_fetch_and_explicit(atomicObject, value, bolt_atomic_to_memory_order(order));
}

uint32_t bolt_atomic_fetch_or_u32(volatile uint32_t* object, uint32_t value, boltAtomicOrder order)
{
    volatile _Atomic uint32_t* atomicObject = (volatile _Atomic uint32_t*)object;
    return atomic_fetch_or_explicit(atomicObject, value, bolt_atomic_to_memory_order(order));
}

uint32_t bolt_atomic_fetch_xor_u32(volatile uint32_t* object, uint32_t value, boltAtomicOrder order)
{
    volatile _Atomic uint32_t* atomicObject = (volatile _Atomic uint32_t*)object;
    return atomic_fetch_xor_explicit(atomicObject, value, bolt_atomic_to_memory_order(order));
}

bool bolt_atomic_compare_exchange_u32(volatile uint32_t* object,
    uint32_t* expected,
    uint32_t desired,
    boltAtomicOrder successOrder,
    boltAtomicOrder failureOrder)
{
    volatile _Atomic uint32_t* atomicObject = (volatile _Atomic uint32_t*)object;
    return atomic_compare_exchange_strong_explicit(atomicObject,
        expected,
        desired,
        bolt_atomic_to_memory_order(successOrder),
        bolt_atomic_failure_order(failureOrder));
}

uint64_t bolt_atomic_load_u64(const volatile uint64_t* object, boltAtomicOrder order)
{
    const volatile _Atomic uint64_t* atomicObject = (const volatile _Atomic uint64_t*)object;
    return atomic_load_explicit(atomicObject, bolt_atomic_to_memory_order(order));
}

void bolt_atomic_store_u64(volatile uint64_t* object, uint64_t value, boltAtomicOrder order)
{
    volatile _Atomic uint64_t* atomicObject = (volatile _Atomic uint64_t*)object;
    atomic_store_explicit(atomicObject, value, bolt_atomic_to_memory_order(order));
}

uint64_t bolt_atomic_exchange_u64(volatile uint64_t* object, uint64_t value, boltAtomicOrder order)
{
    volatile _Atomic uint64_t* atomicObject = (volatile _Atomic uint64_t*)object;
    return atomic_exchange_explicit(atomicObject, value, bolt_atomic_to_memory_order(order));
}

uint64_t bolt_atomic_fetch_add_u64(volatile uint64_t* object, uint64_t value, boltAtomicOrder order)
{
    volatile _Atomic uint64_t* atomicObject = (volatile _Atomic uint64_t*)object;
    return atomic_fetch_add_explicit(atomicObject, value, bolt_atomic_to_memory_order(order));
}

uint64_t bolt_atomic_fetch_sub_u64(volatile uint64_t* object, uint64_t value, boltAtomicOrder order)
{
    volatile _Atomic uint64_t* atomicObject = (volatile _Atomic uint64_t*)object;
    return atomic_fetch_sub_explicit(atomicObject, value, bolt_atomic_to_memory_order(order));
}

uint64_t bolt_atomic_fetch_and_u64(volatile uint64_t* object, uint64_t value, boltAtomicOrder order)
{
    volatile _Atomic uint64_t* atomicObject = (volatile _Atomic uint64_t*)object;
    return atomic_fetch_and_explicit(atomicObject, value, bolt_atomic_to_memory_order(order));
}

uint64_t bolt_atomic_fetch_or_u64(volatile uint64_t* object, uint64_t value, boltAtomicOrder order)
{
    volatile _Atomic uint64_t* atomicObject = (volatile _Atomic uint64_t*)object;
    return atomic_fetch_or_explicit(atomicObject, value, bolt_atomic_to_memory_order(order));
}

uint64_t bolt_atomic_fetch_xor_u64(volatile uint64_t* object, uint64_t value, boltAtomicOrder order)
{
    volatile _Atomic uint64_t* atomicObject = (volatile _Atomic uint64_t*)object;
    return atomic_fetch_xor_explicit(atomicObject, value, bolt_atomic_to_memory_order(order));
}

bool bolt_atomic_compare_exchange_u64(volatile uint64_t* object,
    uint64_t* expected,
    uint64_t desired,
    boltAtomicOrder successOrder,
    boltAtomicOrder failureOrder)
{
    volatile _Atomic uint64_t* atomicObject = (volatile _Atomic uint64_t*)object;
    return atomic_compare_exchange_strong_explicit(atomicObject,
        expected,
        desired,
        bolt_atomic_to_memory_order(successOrder),
        bolt_atomic_failure_order(failureOrder));
}
#endif
#if !defined(_MSC_VER) && defined(BOLT_RUNTIME_INCLUDE_FREESTANDING_START)
extern int start(void);

BOLT_NORETURN void _start(void)
{
    int code = start();
    (void)code;
    bolt_panic_abort("start returned");
}
#endif
