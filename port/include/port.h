#pragma once

#include <strings.h>

#include <cassert>
#include <cstddef>
#include <new>

#ifdef __APPLE__
#undef __assert
#define section(name)
#endif

#define PORT_STRINGIFY2(x) #x
#define PORT_STRINGIFY(x) PORT_STRINGIFY2(x)
#define PORT_ASM_NAME(name) PORT_STRINGIFY(__USER_LABEL_PREFIX__) #name

[[noreturn]] void Ps2Unimplemented(const char *function, const char *file, int line);

#define PS2_UNIMPLEMENTED() Ps2Unimplemented(__func__, __FILE__, __LINE__)

// Marks a definition in port/src that replaces the one ps2/src gives the same name. The port
// compiles ps2/src without that one (scripts/port/pc_override.py, docs/PC.md).
#define PC_OVERRIDE

/**
 * The Metrowerks runtime's assertion failure.
 */
[[noreturn]] void __assert(const char *file, int line, const char *expression);

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
#include "types.h"
#undef size_t
#pragma pop_macro("NULL")
#pragma clang diagnostic pop

// The layouts STATIC_ASSERT checks are the PS2's: 32-bit pointers and longs as
// MWCC lays them out. Once common.h is in, it checks nothing.
#include "common.h"
#undef STATIC_ASSERT
#define STATIC_ASSERT(expr)
