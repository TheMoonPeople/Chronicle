#pragma once

#include <strings.h>

#if defined(__ANDROID__) && !defined(__ANDROID_ASSERT_RENAME)
#define __ANDROID_ASSERT_RENAME
#define __assert __android_libc_assert
#endif
#include <cassert>
#include <cstddef>
#include <new>
#ifdef __ANDROID__
#undef __assert
#undef __ANDROID_ASSERT_RENAME
#endif
#ifdef __APPLE__
#undef __assert
#endif
#ifdef __APPLE__
#define section(name)
#endif

#define PORT_STRINGIFY2(x) #x
#define PORT_STRINGIFY(x) PORT_STRINGIFY2(x)
#define PORT_ASM_NAME(name) PORT_STRINGIFY(__USER_LABEL_PREFIX__) #name

#include "kazarov/port.h"

/**
 * The Metrowerks runtime's assertion failure.
 */
#if defined(__ANDROID__)
extern "C" [[noreturn]] void __assert(const char *file, int line, const char *expression);
#else
[[noreturn]] void __assert(const char *file, int line, const char *expression);
#endif

/**
 * The Metrowerks runtime's exit.
 */
extern "C" [[noreturn]] void exit__2(int status);

/**
 * Makes a temporary an lvalue, for the non-const reference parameters MWCC binds temporaries to.
 */
template <class T> T &Ps2Lvalue(T &&value) {
    return static_cast<T &>(value);
}

// types.h gives the PS2's size_t and NULL. Included here under another name,
// the host's stay in force, and #pragma once keeps the game from including it again.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmacro-redefined"
#pragma push_macro("NULL")
#define size_t ps2_size_t
#ifdef _WIN32
// Windows uses LLP64; the GS register bitfields require the PS2's 64-bit u_long.
#define u_long ps2_native_u_long
#endif
#include "types.h"
#ifdef _WIN32
#undef u_long
#define u_long u64
#endif
#undef size_t
#pragma pop_macro("NULL")
#pragma clang diagnostic pop

// The layouts STATIC_ASSERT checks are the PS2's: 32-bit pointers and longs as
// MWCC lays them out. Once common.h is in, it checks nothing.
#include "common.h"
#undef STATIC_ASSERT
#define STATIC_ASSERT(expr)
