// `--then PATH` (main.cpp): when the game exits, the Homebrew Menu's loader starts PATH instead of
// the menu. tools/switch/runner sets it, so a test run returns to the runner, which waits for the
// next build over the network (docs/SWITCH.md, "Testing from a computer").

#include <switch.h>

#include <cstdio>
#include <cstdlib>
#include <string>

namespace {
std::string g_next;
}

void SwitchThen(const char *path) {
    if (!envHasNextLoad()) {
        std::fprintf(stderr, "--then: the loader cannot start another program\n");
        return;
    }
    g_next = path;
    std::atexit([] {
        std::string argv = "\"" + g_next + "\"";
        envSetNextLoad(g_next.c_str(), argv.c_str());
    });
}
