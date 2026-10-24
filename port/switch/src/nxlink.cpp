// The log on a console. Sent by `nxlink -s darkcloud.nro`, the game sends stdout and stderr back to
// the computer that sent it; started any other way, they go to darkcloud.log in its own directory,
// unbuffered, so a run that stops short still leaves its last lines there. libnx calls
// userAppInit and userAppExit around main.

#include <arpa/inet.h>
#include <switch.h>
#include <unistd.h>

#include <cstdio>

#include "switch_memory.hpp"

namespace {
int  g_socket = -1;
bool g_sockets = false;

void LogToFile() {
    if (std::freopen("darkcloud.log", "w", stdout) == nullptr) {
        return;
    }
    dup2(fileno(stdout), fileno(stderr));
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    std::setvbuf(stderr, nullptr, _IONBF, 0);
}
} // namespace

extern "C" void userAppInit() {
    if (__nxlink_host.s_addr != 0 && R_SUCCEEDED(socketInitializeDefault())) {
        g_sockets = true;
        g_socket = nxlinkStdio();
        if (g_socket >= 0) {
            return;
        }
    }
    LogToFile();
}

extern "C" void userAppExit() {
    SwitchRestoreMemoryGuards();
    if (g_socket >= 0) {
        close(g_socket);
    }
    if (g_sockets) {
        socketExit();
    }
}
