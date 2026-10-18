// The log on a console: started by `nxlink -s darkcloud.nro`, the game sends stdout and stderr back
// to the computer that sent it. Started from the Homebrew Menu, they go nowhere, as before. libnx
// calls these two around main.

#include <arpa/inet.h>
#include <switch.h>

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
