#include "discord.hpp"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#endif

#include <nlohmann/json.hpp>

#include <algorithm>
#include <bit>
#include <cerrno>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

namespace fs = std::filesystem;
using Json = nlohmann::json;
using Clock = std::chrono::steady_clock;

static_assert(std::endian::native == std::endian::little, "Discord's frame header is little-endian");

constexpr const char *kProjectUrl = "https://github.com/TheMoonPeople/Chronicle";
constexpr auto        kReconnectDelay = std::chrono::seconds(15);
// Discord's rate limit is five updates per 20 seconds.
constexpr auto kUpdateInterval = std::chrono::seconds(4);
constexpr auto kReplyTimeout = std::chrono::seconds(5);

enum Opcode : std::uint32_t {
    kOpHandshake = 0,
    kOpFrame = 1,
    kOpClose = 2,
    kOpPing = 3,
    kOpPong = 4,
};

struct Frame {
    std::uint32_t opcode = 0;
    Json          body;
};

struct State {
    std::mutex                     mutex;
    std::condition_variable        wake;
    std::optional<DiscordActivity> wanted;
    bool                           stop = false;
    std::thread                    thread;
} g_state;

#ifdef _WIN32

using Pipe = HANDLE;
const Pipe kNoPipe = INVALID_HANDLE_VALUE;

std::vector<std::string> PipePaths() {
    std::vector<std::string> paths;
    for (int i = 0; i < 10; ++i) {
        paths.push_back("\\\\.\\pipe\\discord-ipc-" + std::to_string(i));
    }
    return paths;
}

Pipe OpenPipe(const std::string &path) {
    return CreateFileA(path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
}

void ClosePipe(Pipe pipe) {
    CloseHandle(pipe);
}

bool WriteAll(Pipe pipe, const void *data, std::size_t size) {
    const char *bytes = static_cast<const char *>(data);
    while (size > 0) {
        DWORD written = 0;
        if (!WriteFile(pipe, bytes, static_cast<DWORD>(size), &written, nullptr) || written == 0) {
            return false;
        }
        bytes += written;
        size -= written;
    }
    return true;
}

bool ReadAll(Pipe pipe, void *data, std::size_t size, Clock::time_point deadline) {
    char *bytes = static_cast<char *>(data);
    while (size > 0) {
        DWORD available = 0;
        if (!PeekNamedPipe(pipe, nullptr, 0, nullptr, &available, nullptr)) {
            return false;
        }
        if (available == 0) {
            if (Clock::now() >= deadline) {
                return false;
            }
            Sleep(5);
            continue;
        }
        DWORD got = 0;
        DWORD want = static_cast<DWORD>(std::min<std::size_t>(size, available));
        if (!ReadFile(pipe, bytes, want, &got, nullptr) || got == 0) {
            return false;
        }
        bytes += got;
        size -= got;
    }
    return true;
}

int ProcessId() {
    return static_cast<int>(GetCurrentProcessId());
}

#else

using Pipe = int;
constexpr Pipe kNoPipe = -1;

#ifdef MSG_NOSIGNAL
constexpr int kSendFlags = MSG_NOSIGNAL;
#else
constexpr int kSendFlags = 0;
#endif

std::vector<std::string> PipePaths() {
    std::vector<fs::path> dirs;
    for (const char *name : {"XDG_RUNTIME_DIR", "TMPDIR", "TMP", "TEMP"}) {
        const char *value = std::getenv(name);
        if (value != nullptr && *value != '\0') {
            dirs.emplace_back(value);
        }
    }
    dirs.emplace_back("/tmp");
    std::vector<std::string> paths;
    for (const fs::path &dir : dirs) {
        for (const char *package :
             {"", "app/com.discordapp.Discord", "snap.discord", ".flatpak/dev.vencord.Vesktop/xdg-run"}) {
            for (int i = 0; i < 10; ++i) {
                paths.push_back((dir / package / ("discord-ipc-" + std::to_string(i))).string());
            }
        }
    }
    return paths;
}

Pipe OpenPipe(const std::string &path) {
    sockaddr_un address = {};
    address.sun_family = AF_UNIX;
    if (path.size() >= sizeof(address.sun_path)) {
        return kNoPipe;
    }
    std::memcpy(address.sun_path, path.c_str(), path.size() + 1);
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        return kNoPipe;
    }
#ifdef SO_NOSIGPIPE
    int on = 1;
    setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &on, sizeof(on));
#endif
    if (connect(fd, reinterpret_cast<const sockaddr *>(&address), sizeof(address)) != 0) {
        close(fd);
        return kNoPipe;
    }
    return fd;
}

void ClosePipe(Pipe fd) {
    close(fd);
}

bool WriteAll(Pipe fd, const void *data, std::size_t size) {
    const char *bytes = static_cast<const char *>(data);
    while (size > 0) {
        ssize_t written = send(fd, bytes, size, kSendFlags);
        if (written < 0 && errno == EINTR) {
            continue;
        }
        if (written <= 0) {
            return false;
        }
        bytes += written;
        size -= static_cast<std::size_t>(written);
    }
    return true;
}

bool ReadAll(Pipe fd, void *data, std::size_t size, Clock::time_point deadline) {
    char *bytes = static_cast<char *>(data);
    while (size > 0) {
        auto left = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - Clock::now()).count();
        if (left <= 0) {
            return false;
        }
        pollfd poll_fd = {fd, POLLIN, 0};
        int    ready = poll(&poll_fd, 1, static_cast<int>(left));
        if (ready < 0 && errno == EINTR) {
            continue;
        }
        if (ready <= 0) {
            return false;
        }
        ssize_t got = recv(fd, bytes, size, 0);
        if (got < 0 && errno == EINTR) {
            continue;
        }
        if (got <= 0) {
            return false;
        }
        bytes += got;
        size -= static_cast<std::size_t>(got);
    }
    return true;
}

int ProcessId() {
    return static_cast<int>(getpid());
}

#endif

bool WriteFrame(Pipe pipe, std::uint32_t opcode, const std::string &payload) {
    std::uint32_t header[2] = {opcode, static_cast<std::uint32_t>(payload.size())};
    return WriteAll(pipe, header, sizeof(header)) && WriteAll(pipe, payload.data(), payload.size());
}

std::optional<Frame> ReadFrame(Pipe pipe) {
    Clock::time_point deadline = Clock::now() + kReplyTimeout;
    std::uint32_t     header[2];
    if (!ReadAll(pipe, header, sizeof(header), deadline) || header[1] > (1u << 20)) {
        return std::nullopt;
    }
    std::string payload(header[1], '\0');
    if (!ReadAll(pipe, payload.data(), payload.size(), deadline)) {
        return std::nullopt;
    }
    return Frame{header[0], Json::parse(payload, nullptr, false)};
}

void Report(const Json &body) {
    std::string message = body.value("message", std::string());
    if (body.contains("data") && body["data"].is_object()) {
        message = body["data"].value("message", message);
    }
    if (message.empty()) {
        message = body.dump();
    }
    static std::string reported;
    if (message != reported) {
        std::fprintf(stderr, "discord: %s\n", message.c_str());
        reported = message;
    }
}

bool ReadReply(Pipe pipe) {
    for (;;) {
        std::optional<Frame> frame = ReadFrame(pipe);
        if (!frame) {
            return false;
        }
        if (frame->opcode == kOpPing) {
            if (!WriteFrame(pipe, kOpPong, frame->body.dump())) {
                return false;
            }
            continue;
        }
        if (frame->opcode != kOpFrame || frame->body.value("evt", Json()) == "ERROR") {
            Report(frame->body);
            return false;
        }
        return true;
    }
}

Pipe Connect(const std::string &client_id) {
    for (const std::string &path : PipePaths()) {
        Pipe pipe = OpenPipe(path);
        if (pipe == kNoPipe) {
            continue;
        }
        Json handshake;
        handshake["v"] = 1;
        handshake["client_id"] = client_id;
        if (WriteFrame(pipe, kOpHandshake, handshake.dump()) && ReadReply(pipe)) {
            std::fprintf(stderr, "discord: connected to %s\n", path.c_str());
            return pipe;
        }
        ClosePipe(pipe);
        return kNoPipe;
    }
    return kNoPipe;
}

void Run(std::string client_id) {
    Pipe                           pipe = kNoPipe;
    std::optional<DiscordActivity> sent;
    Clock::time_point              next_connect = Clock::now();
    Clock::time_point              next_send = Clock::now();
    long long                      start_time = static_cast<long long>(std::time(nullptr));
    int                            nonce = 0;

    std::unique_lock lock(g_state.mutex);
    while (!g_state.stop) {
        if (pipe == kNoPipe && Clock::now() >= next_connect) {
            lock.unlock();
            pipe = Connect(client_id);
            lock.lock();
            sent.reset();
            next_connect = Clock::now() + kReconnectDelay;
            continue;
        }
        if (pipe != kNoPipe && g_state.wanted && g_state.wanted != sent && Clock::now() >= next_send) {
            DiscordActivity activity = *g_state.wanted;
            lock.unlock();
            bool ok = WriteFrame(pipe, kOpFrame, DiscordActivityCommand(activity, start_time, ProcessId(), ++nonce)) &&
                      ReadReply(pipe);
            lock.lock();
            next_send = Clock::now() + kUpdateInterval;
            if (ok) {
                sent = std::move(activity);
            } else {
                ClosePipe(pipe);
                pipe = kNoPipe;
                next_connect = Clock::now() + kReconnectDelay;
            }
            continue;
        }
        if (pipe == kNoPipe) {
            g_state.wake.wait_until(lock, next_connect);
        } else if (g_state.wanted && g_state.wanted != sent) {
            g_state.wake.wait_until(lock, next_send);
        } else {
            g_state.wake.wait(lock);
        }
    }
    if (pipe != kNoPipe) {
        ClosePipe(pipe);
    }
}

} // namespace

std::string DiscordActivityCommand(const DiscordActivity &activity, long long start_time, int pid, int nonce) {
    Json presence;
    presence["timestamps"]["start"] = start_time;
    presence["assets"]["small_image"] = "chronicle";
    presence["assets"]["small_text"] = "Chronicle, the Dark Cloud decompilation";
    if (!activity.large_image.empty()) {
        presence["assets"]["large_image"] = activity.large_image;
        presence["assets"]["large_text"] = activity.large_text;
    }
    Json button;
    button["label"] = "Chronicle on GitHub";
    button["url"] = kProjectUrl;
    presence["buttons"].push_back(std::move(button));
    if (!activity.details.empty()) {
        presence["details"] = activity.details;
    }
    if (!activity.state.empty()) {
        presence["state"] = activity.state;
    }
    Json command;
    command["cmd"] = "SET_ACTIVITY";
    command["args"]["pid"] = pid;
    command["args"]["activity"] = std::move(presence);
    command["nonce"] = std::to_string(nonce);
    return command.dump();
}

void DiscordStart(std::string client_id) {
    if (client_id.empty() || g_state.thread.joinable()) {
        return;
    }
    g_state.stop = false;
    g_state.thread = std::thread(Run, std::move(client_id));
}

void DiscordSetActivity(const DiscordActivity &activity) {
    {
        std::lock_guard lock(g_state.mutex);
        g_state.wanted = activity;
    }
    g_state.wake.notify_one();
}

void DiscordStop() {
    if (!g_state.thread.joinable()) {
        return;
    }
    {
        std::lock_guard lock(g_state.mutex);
        g_state.stop = true;
    }
    g_state.wake.notify_one();
    g_state.thread.join();
}
