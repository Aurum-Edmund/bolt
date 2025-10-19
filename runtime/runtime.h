#pragma once

#include <stddef.h>

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
 * Copies ytes from source to destination and returns destination.
 * The regions must not overlap.
 */
void* bolt_memory_copy(void* destination, const void* source, size_t bytes);

/*
 * Memory fill helper (see glossary "Memory Fill").
 * Fills ytes bytes at destination with the byte value alue.
 * Returns destination.
 */
void* bolt_memory_fill(void* destination, int value, size_t bytes);

#ifdef __cplusplus
}
#endif

#undef BOLT_NORETURN
