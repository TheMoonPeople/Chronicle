#include <cstdio>
#include <cstdlib>
#include <print>

#include "app/exitcodes.hpp"
#include "app/gameloop.hpp"

// Retail halted on a failed assertion. The game prints its own context on stdout first (LoadFile
// names the file), so stdout is flushed ahead of the message. _Exit, not exit: static destructors
// would tear down the renderer under a frame the game may have open.
#ifdef __ANDROID__
extern "C" [[noreturn]] void __assert(const char *file, int line, const char *expression) {
#else
[[noreturn]] void __assert(const char *file, int line, const char *expression) {
#endif
    std::fflush(stdout);
    std::println(stderr, "{}:{}: assertion failed: {}", file, line, expression);
    std::fflush(nullptr);
    std::_Exit(dc::app::kExitGameAssert);
}

// Retail's libc generator, in place of the host's. The game leans on its exact sequence: a fire
// reseeds it every tick from a seed it drew the tick before (CFireOmni), so what follows in the
// tick -- the title's lightning, the light a fire throws on a character -- repeats in a pattern
// the generator decides. All 32 bits of the state come back but the sign.
//
// The first draws happen during static construction: each CFireOmni and CWind constructor takes one
// and the CSaveData constructors take more. Which value each static gets, and where the sequence
// stands when main starts, depends on the order statics are built in, which otherwise follows the
// link order. The game statics that draw carry init_priority (65001 upwards) so that the order
// stays the one the port has always had: the title winds and the game units' spare fires and winds,
// objanime's fire, main's save_data, EdWind, edit_in's fire, the dungeon maps and kaze, then the
// title fires and everything without a priority.
namespace {
unsigned int g_rand_state = 1;
}

unsigned int dc::app::GameRandomStateForTrace() {
    return g_rand_state;
}

// glibc declares these noexcept; Apple's libc and the Windows CRT do not.
#if defined(__APPLE__) || defined(_WIN32) || defined(__ANDROID__)
#define LIBC_NOEXCEPT
#else
#define LIBC_NOEXCEPT noexcept
#endif

extern "C" void srand(unsigned int seed) LIBC_NOEXCEPT {
    g_rand_state = seed;
}

extern "C" int rand() LIBC_NOEXCEPT {
    g_rand_state = g_rand_state * 1103515245u + 12345u;
    return static_cast<int>(g_rand_state & 0x7FFFFFFFu);
}

extern "C" [[noreturn]] void exit__2(int status) {
    std::exit(status);
}
