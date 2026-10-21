#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <set>
#include <string>
#include <string_view>

#include "../glyph_draw.hpp"
#include "platform/config.hpp"
#include "platform/input.hpp"

// The button symbols (platform/glyphs.hpp): their two settings, the key names the keyboard style looks
// glyphs up by, and the shipped glyphs.json the names must be found in.

namespace {

nlohmann::json ReadShippedIndex() {
    std::filesystem::path file = std::filesystem::path(__FILE__).parent_path() / ".." / ".." / "glyphs" / "glyphs.json";
    std::ifstream         stream(file);
    return nlohmann::json::parse(stream, nullptr, false);
}

} // namespace

TEST(Glyphs, SettingsDefaultToNewSymbolsOnTheDeviceInUse) {
    Config config = ConfigParse("");
    ASSERT_TRUE(config.glyphs_new);
    ASSERT_EQ(config.glyph_device, ConfigGlyphDevice::Auto);
}

TEST(Glyphs, SettingsParseAndRoundTrip) {
    Config config = ConfigParse(R"({"input":{"glyphs":"original","glyph_device":"switch"}})");
    ASSERT_FALSE(config.glyphs_new);
    ASSERT_EQ(config.glyph_device, ConfigGlyphDevice::Switch);
    ASSERT_EQ(ConfigParse(ConfigSerialize(config)), config);
    for (std::string_view name : {"auto", "ps3", "ps4", "ps5", "xbox", "switch", "steamdeck", "steamcontroller", "keyboard"}) {
        Config c = ConfigParse(std::string(R"({"input":{"glyph_device":")") + std::string(name) + R"("}})");
        ASSERT_EQ(ConfigParse(ConfigSerialize(c)).glyph_device, c.glyph_device) << name;
    }
    ASSERT_EQ(ConfigParse(R"({"input":{"glyph_device":"steamdeck"}})").glyph_device, ConfigGlyphDevice::SteamDeck);
    ASSERT_EQ(ConfigParse(R"({"input":{"glyph_device":"steamcontroller"}})").glyph_device,
              ConfigGlyphDevice::SteamController);
}

TEST(Glyphs, BadSettingsAreIgnored) {
    Config config = ConfigParse(R"({"input":{"glyphs":"fancy","glyph_device":"atari"}})");
    ASSERT_TRUE(config.glyphs_new);
    ASSERT_EQ(config.glyph_device, ConfigGlyphDevice::Auto);
}

TEST(Glyphs, ShippedIndexHasEveryPadButtonInEveryPadStyle) {
    nlohmann::json index = ReadShippedIndex();
    ASSERT_TRUE(index.is_object());
    const std::set<std::string> buttons = {"cross", "circle", "square", "triangle", "l1", "r1", "l2", "r2", "start",
                                           "select", "dpad", "dpad_ud", "dpad_lr", "up", "down", "left", "right",
                                           "l3", "r3", "lstick", "rstick"};
    for (const char *style : {"ps3", "ps4", "ps5", "xbox", "switch", "steamdeck", "steamcontroller", "keyboard"}) {
        ASSERT_TRUE(index["styles"].contains(style)) << style;
        const auto &glyphs = index["styles"][style]["glyphs"];
        for (const std::string &button : buttons) {
            // The keyboard style shows the key bound to a button (the next test); only the directional
            // symbols, which are no one key, are drawn for it.
            bool pad_only = std::string_view(style) == "keyboard" && button != "dpad" && button != "dpad_ud" &&
                            button != "dpad_lr" && button != "lstick" && button != "rstick";
            if (!pad_only) {
                ASSERT_TRUE(glyphs.contains(button)) << style << " lacks " << button;
            }
        }
    }
}

TEST(Glyphs, ShippedKeyboardIndexHasTheDefaultBindings) {
    nlohmann::json index = ReadShippedIndex();
    ASSERT_TRUE(index.is_object());
    const auto &glyphs = index["styles"]["keyboard"]["glyphs"];
    InputResetBindings();
    for (const char *action : {"cross", "circle", "square", "triangle", "l1", "r1", "l2", "r2", "l3", "r3", "start",
                               "select", "up", "down", "left", "right"}) {
        std::string name = InputPrimaryBindingName(action);
        ASSERT_FALSE(name.empty()) << action;
        ASSERT_TRUE(glyphs.contains(name)) << action << " is bound to " << name << ", which has no symbol";
    }
}

TEST(Glyphs, EveryAtlasRectIsInsideItsAtlasFile) {
    nlohmann::json index = ReadShippedIndex();
    ASSERT_TRUE(index.is_object());
    for (const auto &[style, value] : index["styles"].items()) {
        std::filesystem::path png = std::filesystem::path(__FILE__).parent_path() / ".." / ".." / "glyphs" /
                                    value["atlas"].get<std::string>();
        ASSERT_TRUE(std::filesystem::exists(png)) << png;
        // PNG: the width and height are the two big-endian words after the signature and "IHDR".
        std::ifstream stream(png, std::ios::binary);
        unsigned char header[24] = {};
        stream.read(reinterpret_cast<char *>(header), sizeof(header));
        auto word = [&](int at) {
            return (header[at] << 24) | (header[at + 1] << 16) | (header[at + 2] << 8) | header[at + 3];
        };
        const int width = word(16);
        const int height = word(20);
        for (const auto &[name, rect] : value["glyphs"].items()) {
            ASSERT_GE(rect[0].get<int>(), 0) << style << " " << name;
            ASSERT_GE(rect[1].get<int>(), 0) << style << " " << name;
            ASSERT_LE(rect[0].get<int>() + rect[2].get<int>(), width) << style << " " << name;
            ASSERT_LE(rect[1].get<int>() + rect[3].get<int>(), height) << style << " " << name;
        }
    }
}

TEST(Glyphs, TheSkipPromptButtonsAreListedInEveryLanguage) {
    for (const char *name : {"pause_e", "pause_f", "pause_g", "pause_i", "pause_s"}) {
        EXPECT_EQ(GlyphBakedCount(name), 2) << name;
    }
}
