#include "runtime.h"

#include <stdint.h>

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
#if !defined(_MSC_VER) && defined(BOLT_RUNTIME_INCLUDE_FREESTANDING_START)
extern int start(void);

BOLT_NORETURN void _start(void)
{
    int code = start();
    (void)code;
    bolt_panic_abort("start returned");
}
#endif

#undef BOLT_NORETURN
