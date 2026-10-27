// The log on a console. Sent by `nxlink -s darkcloud.nro`, the game sends stdout and stderr back to
// the computer that sent it; started any other way, they go to darkcloud.log in its own directory,
// unbuffered, so a run that stops short still leaves its last lines there. libnx calls
// userAppInit and userAppExit around main.

#include <arpa/inet.h>
#include <switch.h>
#include <sys/iosupport.h>
#include <sys/reent.h>
#include <unistd.h>

#include <cstdio>
#include <string_view>

#include "switch_main.hpp"
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

namespace dc::platform {

void SwitchDisplaySize(int &width, int &height) {
    const bool handheld = appletGetOperationMode() == AppletOperationMode_Handheld;
    width = handheld ? 1280 : 1920;
    height = handheld ? 720 : 1080;
}

void SwitchShowMissingData() {
    consoleInit(nullptr);
    constexpr std::string_view message =
        "Missing game data. Place data.zip in the switch/chronicle/ folder in your SD card.\n"
        "Press + to exit.\n";
    // stdout may already point to the log file or nxlink socket.
    devoptab_list[STD_OUT]->write_r(_REENT, nullptr, message.data(), message.size());
    consoleUpdate(nullptr);

    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    PadState pad;
    padInitializeDefault(&pad);
    while (appletMainLoop()) {
        padUpdate(&pad);
        if ((padGetButtonsDown(&pad) & HidNpadButton_Plus) != 0) {
            break;
        }
        consoleUpdate(nullptr);
    }
    consoleExit(nullptr);
}

} // namespace dc::platform

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
    dc::platform::SwitchRestoreMemoryGuards();
    if (g_socket >= 0) {
        close(g_socket);
    }
    if (g_sockets) {
        socketExit();
    }
}
