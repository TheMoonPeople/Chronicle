// The log on a console. Sent by `nxlink -s darkcloud.nro`, the game sends stdout and stderr back to
// the computer that sent it; started any other way, they go to darkcloud.log in its own directory,
// unbuffered, so a run that stops short still leaves its last lines there. libnx calls
// userAppInit and userAppExit around main.

#include <arpa/inet.h>
#include <switch.h>

#include <cstdio>
#include <unistd.h>

namespace {
bool g_sockets = false;
}

extern "C" void userAppInit() {
    if (__nxlink_host.s_addr != 0 && R_SUCCEEDED(socketInitializeDefault())) {
        g_sockets = true;
        nxlinkStdio();
    }
}

extern "C" void userAppExit() {
    if (g_sockets) {
        socketExit();
    }
}

// After libnx has mounted the SD card and moved to the .nro's directory, before main.
__attribute__((constructor)) static void LogToFile() {
    if (g_sockets || std::freopen("darkcloud.log", "w", stdout) == nullptr) {
        return;
    }
    dup2(fileno(stdout), fileno(stderr));
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    std::setvbuf(stderr, nullptr, _IONBF, 0);
    char directory[512] = "";
    getcwd(directory, sizeof(directory));
    std::printf("darkcloud: log of a run in %s\n", directory);
}
