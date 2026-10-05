#include <cstdio>
#include <cstdlib>
#include <iterator>

#include "dataread.hpp"
#include "exitcodes.hpp"
#include "mathutil.hpp"
#include "title/title_port.hpp"

[[noreturn]] void Ps2Unimplemented(const char *function, const char *file, int line) {
    std::fprintf(stderr, "%s:%d: %s: PlayStation 2 code is not implemented on PC\n", file, line, function);
    std::abort();
}

// Retail halted on a failed assertion. The game prints its own context on stdout first (LoadFile
// names the file), so stdout is flushed ahead of the message. _Exit, not exit: static destructors
// would tear down the renderer under a frame the game may have open.
[[noreturn]] void __assert(const char *file, int line, const char *expression) {
    std::fflush(stdout);
    std::fprintf(stderr, "%s:%d: assertion failed: %s\n", file, line, expression);
    std::fflush(nullptr);
    std::_Exit(kExitGameAssert);
}

// Retail's libc generator, in place of the host's. The game leans on its exact sequence: a fire
// reseeds it every tick from a seed it drew the tick before (CFireOmni), so what follows in the
// tick -- the title's lightning, the light a fire throws on a character -- repeats in a pattern
// the generator decides. All 32 bits of the state come back but the sign.
namespace {
unsigned int g_rand_state = 1;
}

// glibc declares these noexcept; Apple's libc does not.
#ifdef __APPLE__
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

// The host runs the static constructors mwInit would have run.
PC_OVERRIDE extern "C" void mwInit(int argc, const char **argv, const char **envp) {}

extern "C" [[noreturn]] void exit__2(int status) {
    std::exit(status);
}

// TITLE.BIN and DUN.BIN are linked into the executable, so there is no file to load. Retail's
// loader re-ran an overlay's static constructors whenever a mode needed the other overlay; the
// port re-runs the title overlay's for the objects it lays out with host classes, whose PS2-sized
// constructors in the title units also run at start-up, over them.
PC_OVERRIDE void LoadOverlay(int mode) {
    enum Overlay { kNone, kTitle, kDungeon };
    constexpr Overlay kOverlay[] = {kTitle, kTitle, kNone, kDungeon, kDungeon, kTitle, kNone, kNone,
                                    kDungeon, kDungeon, kNone, kNone, kNone, kNone, kNone};
    static Overlay loaded = kNone;
    if (mode < 0 || mode >= static_cast<int>(std::size(kOverlay)) || kOverlay[mode] == kNone ||
        kOverlay[mode] == loaded) {
        return;
    }
    loaded = kOverlay[mode];
    if (loaded == kTitle) {
        TitleOverlayConstruct();
    }
}

PC_OVERRIDE extern "C" int mwLoadOverlay(char *path, void *address) {
    return 1;
}
