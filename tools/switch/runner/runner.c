// The test runner (docs/SWITCH.md, "Testing from a computer"): a netloader that stays. It waits for
// `nxlink` as the Homebrew Menu's netloader does (Y there), saves the .nro it is sent under
// sdmc:/switch/, and starts it with the sent arguments, the nxlink host and `--then <this runner>`,
// so the game comes back here when it exits instead of to the menu. + quits to the menu.
//
// It draws nothing: a console of its own (consoleInit) left the GPU so that NVK, in the game it
// started next, faulted allocating its swapchain. The screen keeps what it showed. Started with the
// GPU untouched, NVK lost its device at the first submission; the Homebrew Menu, after which NVK
// starts cleanly, draws with deko3d, so the runner sets the GPU up through deko3d as the menu does
// (InitGpu) before it starts anything.

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <switch.h>
#include <unistd.h>
#include <zlib.h>

#include <deko3d.h>

#define PORT 28280
#define CLIENT_PORT 28771
#define CHUNK (16 * 1024)

static bool RecvAll(int sock, void *buffer, size_t size) {
    unsigned char *at = buffer;
    while (size > 0) {
        ssize_t got = recv(sock, at, size, 0);
        if (got > 0) {
            at += got;
            size -= (size_t)got;
        } else if (got == 0 || (errno != EWOULDBLOCK && errno != EAGAIN)) {
            return false;
        } else {
            svcSleepThread(1000000);
        }
    }
    return true;
}

static bool SendAll(int sock, const void *buffer, size_t size) {
    const unsigned char *at = buffer;
    while (size > 0) {
        ssize_t sent = send(sock, at, size, 0);
        if (sent > 0) {
            at += sent;
            size -= (size_t)sent;
        } else if (sent == 0 || (errno != EWOULDBLOCK && errno != EAGAIN)) {
            return false;
        } else {
            svcSleepThread(1000000);
        }
    }
    return true;
}

// The file arrives as one zlib stream in chunks, each led by its length.
static bool Receive(int sock, FILE *file, uint32_t length) {
    static unsigned char in[CHUNK];
    static unsigned char out[CHUNK];
    z_stream             stream = {0};
    if (inflateInit(&stream) != Z_OK) {
        return false;
    }
    uint32_t total = 0;
    int      status = Z_OK;
    while (status != Z_STREAM_END) {
        uint32_t chunk = 0;
        if (!RecvAll(sock, &chunk, 4) || chunk > CHUNK || !RecvAll(sock, in, chunk)) {
            break;
        }
        stream.avail_in = chunk;
        stream.next_in = in;
        do {
            stream.avail_out = CHUNK;
            stream.next_out = out;
            status = inflate(&stream, Z_NO_FLUSH);
            if (status != Z_OK && status != Z_STREAM_END && status != Z_BUF_ERROR) {
                inflateEnd(&stream);
                return false;
            }
            uint32_t have = CHUNK - stream.avail_out;
            if (fwrite(out, 1, have, file) != have) {
                inflateEnd(&stream);
                return false;
            }
            total += have;
        } while (stream.avail_out == 0);
    }
    inflateEnd(&stream);
    printf("received %u of %u bytes\n", (unsigned)total, (unsigned)length);
    return status == Z_STREAM_END && total == length;
}

// Receives one .nro and sets it as the next program; false when anything went wrong.
static bool Serve(int sock, struct in_addr host, const char *self) {
    uint32_t name_length = 0;
    char     name[256];
    if (!RecvAll(sock, &name_length, 4) || name_length >= sizeof(name) || !RecvAll(sock, name, name_length)) {
        printf("bad file name\n");
        return false;
    }
    name[name_length] = 0;
    uint32_t length = 0;
    if (!RecvAll(sock, &length, 4)) {
        return false;
    }
    static char path[512];
    snprintf(path, sizeof(path), "sdmc:/switch/%s", name);
    FILE *file = fopen(path, "wb");
    int   response = file != NULL ? 0 : -1;
    SendAll(sock, &response, sizeof(response));
    if (file == NULL) {
        printf("cannot write %s (%d)\n", path, errno);
        return false;
    }
    printf("receiving %s, %u bytes\n", path, (unsigned)length);
    bool ok = Receive(sock, file, length);
    fclose(file);
    if (!ok) {
        printf("transfer failed\n");
        unlink(path);
        return false;
    }
    SendAll(sock, &response, sizeof(response));

    // The arguments, each ended by a zero byte.
    uint32_t     args_length = 0;
    static char  args[4096];
    if (!RecvAll(sock, &args_length, 4) || args_length >= sizeof(args) || !RecvAll(sock, args, args_length)) {
        printf("bad arguments\n");
        return false;
    }
    args[args_length] = 0;

    static char command[8192];
    int         at = snprintf(command, sizeof(command), "\"%s\"", path);
    for (uint32_t i = 0; i < args_length; i += (uint32_t)strlen(args + i) + 1) {
        if (args[i] != 0) {
            at += snprintf(command + at, sizeof(command) - (size_t)at, " \"%s\"", args + i);
        }
    }
    at += snprintf(command + at, sizeof(command) - (size_t)at, " --then \"%s\" %08x_NXLINK_", self,
                   (unsigned)host.s_addr);
    printf("starting %s\n", command);
    envSetNextLoad(path, command);
    return true;
}

// A deko3d device and graphics queue, one submission waited for, then all of it destroyed.
static void InitGpu(void) {
    DkDeviceMaker device_maker;
    dkDeviceMakerDefaults(&device_maker);
    DkDevice device = dkDeviceCreate(&device_maker);

    DkMemBlockMaker block_maker;
    dkMemBlockMakerDefaults(&block_maker, device, DK_MEMBLOCK_ALIGNMENT);
    block_maker.flags = DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached;
    DkMemBlock block = dkMemBlockCreate(&block_maker);

    DkCmdBufMaker buffer_maker;
    dkCmdBufMakerDefaults(&buffer_maker, device);
    DkCmdBuf buffer = dkCmdBufCreate(&buffer_maker);
    dkCmdBufAddMemory(buffer, block, 0, DK_MEMBLOCK_ALIGNMENT);

    DkQueueMaker queue_maker;
    dkQueueMakerDefaults(&queue_maker, device);
    queue_maker.flags = DkQueueFlags_Graphics;
    DkQueue queue = dkQueueCreate(&queue_maker);

    dkCmdBufBarrier(buffer, DkBarrier_Full, DkInvalidateFlags_Image | DkInvalidateFlags_Shader);
    DkCmdList list = dkCmdBufFinishList(buffer);
    dkQueueSubmitCommands(queue, list);
    dkQueueWaitIdle(queue);

    dkQueueDestroy(queue);
    dkCmdBufDestroy(buffer);
    dkMemBlockDestroy(block);
    dkDeviceDestroy(device);
}

int main(int argc, char **argv) {
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    PadState pad;
    padInitializeDefault(&pad);
    const char *self = argc > 0 && argv[0] != NULL ? argv[0] : "sdmc:/switch/chronicle/runner.nro";

    printf("Chronicle test runner: nxlink to this Switch; + returns to the menu.\n");
    if (!envHasNextLoad()) {
        printf("Start it from the Homebrew Menu; it cannot start programs from here.\n");
    }
    if (R_FAILED(socketInitializeDefault())) {
        printf("no network\n");
        return 1;
    }
    struct in_addr own = {(in_addr_t)gethostid()};
    printf("listening on %s port %d\n", inet_ntoa(own), PORT);

    struct sockaddr_in address = {0};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(PORT);
    int listener = socket(AF_INET, SOCK_STREAM, 0);
    int ping = socket(AF_INET, SOCK_DGRAM, 0);
    int yes = 1;
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    if (listener < 0 || bind(listener, (struct sockaddr *)&address, sizeof(address)) != 0 || listen(listener, 2) != 0) {
        printf("cannot listen (%d)\n", errno);
    }
    if (ping >= 0) {
        bind(ping, (struct sockaddr *)&address, sizeof(address));
        fcntl(ping, F_SETFL, fcntl(ping, F_GETFL, 0) | O_NONBLOCK);
    }
    fcntl(listener, F_SETFL, fcntl(listener, F_GETFL, 0) | O_NONBLOCK);

    InitGpu();

    bool started = false;
    while (appletMainLoop() && !started) {
        padUpdate(&pad);
        if (padGetButtonsDown(&pad) & HidNpadButton_Plus) {
            break;
        }
        // nxlink's broadcast ping: answer it so `nxlink` finds the Switch without an address.
        char               message[64];
        struct sockaddr_in from;
        socklen_t          from_length = sizeof(from);
        ssize_t got = ping >= 0 ? recvfrom(ping, message, sizeof(message), 0, (struct sockaddr *)&from, &from_length) : -1;
        if (got >= 6 && strncmp(message, "nxboot", 6) == 0) {
            from.sin_port = htons(CLIENT_PORT);
            sendto(ping, "bootnx", 6, 0, (struct sockaddr *)&from, sizeof(from));
        }
        struct sockaddr_in remote;
        socklen_t          remote_length = sizeof(remote);
        int                sock = accept(listener, (struct sockaddr *)&remote, &remote_length);
        if (sock >= 0) {
            fcntl(sock, F_SETFL, fcntl(sock, F_GETFL, 0) & ~O_NONBLOCK);
            started = Serve(sock, remote.sin_addr, self);
            close(sock);
        }
        svcSleepThread(10000000);
    }
    close(listener);
    if (ping >= 0) {
        close(ping);
    }
    socketExit();
    return 0;
}
