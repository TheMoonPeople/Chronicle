#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#ifndef _WIN32
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#endif

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>

#include "app/presence.hpp"
#include "editmapscript.hpp"
#include "main.hpp"
#include "platform/discord.hpp"

namespace dc::test {

using namespace dc::game;
using namespace dc::app;
using namespace dc::platform;

using Json = nlohmann::json;

#ifndef _WIN32

namespace fs = std::filesystem;

namespace {

bool ReadExactly(int fd, std::span<std::byte> bytes) {
    while (!bytes.empty()) {
        ssize_t got = recv(fd, bytes.data(), bytes.size(), 0);
        if (got <= 0) {
            return false;
        }
        bytes = bytes.subspan(static_cast<std::size_t>(got));
    }
    return true;
}

bool ReadFrame(int fd, std::uint32_t &opcode, Json &body) {
    std::array<std::uint32_t, 2> header{};
    if (!ReadExactly(fd, std::as_writable_bytes(std::span(header)))) {
        return false;
    }
    std::string payload(header[1], '\0');
    if (!ReadExactly(fd, std::as_writable_bytes(std::span(payload)))) {
        return false;
    }
    opcode = header[0];
    body = Json::parse(payload);
    return true;
}

void WriteFrame(int fd, std::uint32_t opcode, const Json &body) {
    std::string                        payload = body.dump();
    const std::array<std::uint32_t, 2> header = {opcode, static_cast<std::uint32_t>(payload.size())};
    ASSERT_EQ(send(fd, header.data(), sizeof(header), 0), static_cast<ssize_t>(sizeof(header)));
    ASSERT_EQ(send(fd, payload.data(), payload.size(), 0), static_cast<ssize_t>(payload.size()));
}

int ServeActivity(int server, const std::string &state) {
    int client = accept(server, nullptr, nullptr);
    EXPECT_GE(client, 0);
    std::uint32_t opcode = 0;
    Json          body;
    EXPECT_TRUE(ReadFrame(client, opcode, body));
    EXPECT_EQ(opcode, 0u);
    EXPECT_EQ(body["client_id"], "1234");
    Json ready;
    ready["cmd"] = "DISPATCH";
    ready["evt"] = "READY";
    WriteFrame(client, 1, ready);

    EXPECT_TRUE(ReadFrame(client, opcode, body));
    EXPECT_EQ(opcode, 1u);
    EXPECT_EQ(body["cmd"], "SET_ACTIVITY");
    EXPECT_EQ(body["args"]["activity"]["state"], state);
    Json reply;
    reply["cmd"] = "SET_ACTIVITY";
    reply["evt"] = nullptr;
    reply["nonce"] = body["nonce"];
    WriteFrame(client, 1, reply);
    return client;
}

} // namespace

#endif

TEST(PlatformDiscord, ActivityCommand) {
    Json command = Json::parse(DiscordActivityCommand(
        {.details = "Playing as Xiao", .state = "Shipwreck · Floor 4", .large_image = "xiao", .large_text = "Xiao"}, 1000, 42, 7));
    ASSERT_EQ(command["cmd"], "SET_ACTIVITY");
    ASSERT_EQ(command["nonce"], "7");
    ASSERT_EQ(command["args"]["pid"], 42);
    const Json &activity = command["args"]["activity"];
    ASSERT_EQ(activity["details"], "Playing as Xiao");
    ASSERT_EQ(activity["state"], "Shipwreck · Floor 4");
    ASSERT_EQ(activity["assets"]["large_image"], "xiao");
    ASSERT_EQ(activity["timestamps"]["start"], 1000);
    ASSERT_EQ(activity["buttons"][0]["url"], "https://github.com/TheMoonPeople/Chronicle");
}

#ifndef _WIN32

TEST(PlatformDiscord, SendsTheActivity) {
    std::string dir_template = "/tmp/dcdiscordXXXXXX";
    ASSERT_NE(mkdtemp(dir_template.data()), nullptr);
    fs::path dir = dir_template;
    fs::path path = dir / "discord-ipc-0";
    setenv("XDG_RUNTIME_DIR", dir.c_str(), 1);

    int server = socket(AF_UNIX, SOCK_STREAM, 0);
    ASSERT_GE(server, 0);
    sockaddr_un address = {};
    address.sun_family = AF_UNIX;
    path.native().copy(address.sun_path, sizeof(address.sun_path) - 1);
    ASSERT_EQ(bind(server, reinterpret_cast<const sockaddr *>(&address), sizeof(address)), 0);
    ASSERT_EQ(listen(server, 1), 0);

    DiscordStart("1234");
    DiscordSetActivity({.details = "Playing as Toan", .state = "Norune Village", .large_image = "toan", .large_text = "Toan"});
    int client = ServeActivity(server, "Norune Village");
    DiscordStop();
    close(client);

    DiscordStart("1234");
    client = ServeActivity(server, "Norune Village");
    DiscordStop();
    close(client);

    close(server);
    fs::remove_all(dir);
}

#endif

TEST(PlatformDiscord, Presence) {
    PresenceState   dungeon = {.mode = GAME_MODE_DUNGEON, .dungeon = DUNGEON_MOON_SEA, .floor = 2, .chara = 5};
    DiscordActivity activity = PresenceDescribe(dungeon);
    ASSERT_EQ(activity.details, "Playing as Osmond");
    ASSERT_EQ(activity.state, "Moon Sea · Floor 3");
    ASSERT_EQ(activity.large_image, "osmond");

    dungeon.back_floor = true;
    ASSERT_EQ(PresenceDescribe(dungeon).state, "Moon Sea · Back Floor 3");

    dungeon.choosing_floor = true;
    ASSERT_EQ(PresenceDescribe(dungeon).state, "Moon Sea · Choosing a floor");

    PresenceState gallery = {.mode = GAME_MODE_DUNGEON, .dungeon = DUNGEON_GALLERY_OF_TIME, .floor = 6};
    ASSERT_EQ(PresenceDescribe(gallery).state, "Gallery of Time · 102 Years Ago");

    PresenceState town = {.mode = GAME_MODE_EDIT, .map_no = 1, .edit_mode = ED_MODE_GEORAMA, .chara = 0};
    activity = PresenceDescribe(town);
    ASSERT_EQ(activity.details, "Playing as Toan");
    ASSERT_EQ(activity.state, "Rebuilding Matataki Village");

    town.edit_mode = ED_MODE_WALK;
    ASSERT_EQ(PresenceDescribe(town).state, "Matataki Village");

    ASSERT_EQ(PresenceDescribe({.mode = GAME_MODE_TITLE}).details, "At the title screen");
}

} // namespace dc::test
