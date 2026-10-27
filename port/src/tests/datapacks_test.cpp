#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "platform/datapacks.hpp"
#include "platform/native_config.hpp"
#include "tests/platform_fixture.hpp"

namespace dc::test {

using namespace dc::platform;

namespace {

namespace fs = std::filesystem;

std::string Text(const vfs::Ref &file) {
    std::vector<std::uint8_t> bytes;
    EXPECT_TRUE(vfs::Read(file, bytes));
    return {bytes.begin(), bytes.end()};
}

// A data folder with a datapacks folder beside it.
class Datapacks : public ::testing::Test {
protected:
    fs::path directory = fs::canonical(fs::temp_directory_path()) / std::format("dc_datapacks_{}", ProcessId());
    fs::path data = directory / "data";

    void SetUp() override {
        fs::remove_all(directory);
        fs::create_directories(data);
    }

    void TearDown() override {
        datapacks::Clear();
        fs::remove_all(directory);
    }

    void Write(const std::string &pack, const std::string &relative, std::string_view text) const {
        const fs::path path = directory / "datapacks" / fs::path(std::u8string(pack.begin(), pack.end())) / relative;
        fs::create_directories(path.parent_path());
        std::ofstream(path, std::ios::binary) << text;
    }

    void Config(std::string_view text) const {
        fs::create_directories(directory / "datapacks");
        std::ofstream(directory / "datapacks/datapacks.json") << text;
        datapacks::Load(data);
    }

    static std::vector<std::string> Texts(std::string_view relative) {
        std::vector<std::string> texts;
        for (const vfs::Ref &file : datapacks::Find(relative)) {
            texts.push_back(Text(file));
        }
        return texts;
    }
};

TEST_F(Datapacks, WithoutAConfigurationNoPackServes) {
    Write("unlisted", "Chara/Hero.dat", "unlisted");
    datapacks::Load(data);
    EXPECT_TRUE(datapacks::Find("Chara/Hero.dat").empty());
}

TEST_F(Datapacks, LaterPacksWinAndReorderingChangesTheWinner) {
    Write("First pack", "gameplay/items/items.json", "first");
    Write("日本語 pack", "gameplay/items/items.json", "second");
    Config(R"({"schema_version":1,"load_order":["First pack","日本語 pack"]})");
    EXPECT_EQ(Texts("gameplay/items/items.json"), (std::vector<std::string>{"second", "first"}));

    Config(R"({"schema_version":1,"load_order":["日本語 pack","First pack"]})");
    EXPECT_EQ(Texts("gameplay/items/items.json"), (std::vector<std::string>{"first", "second"}));
}

TEST_F(Datapacks, AnInvalidConfigurationDisablesEveryPack) {
    Write("valid", "Chara/Hero.dat", "replacement");
    for (const char *config : {R"({"schema_version":2,"load_order":["valid"]})",
                               R"({"schema_version":1.0,"load_order":["valid"]})",
                               R"({"schema_version":1,"load_order":["valid","../escape"]})",
                               R"({"schema_version":1,"load_order":["valid","a/b"]})",
                               R"({"schema_version":1,"load_order":["valid","C:escape"]})",
                               R"({"schema_version":1,"load_order":["valid",""]})",
                               R"({"schema_version":1,"load_order":["valid",false]})", "invalid JSON"}) {
        Config(R"({"schema_version":1,"load_order":["valid"]})");
        EXPECT_EQ(Texts("chara/hero.dat"), std::vector<std::string>{"replacement"});
        Config(config);
        EXPECT_TRUE(datapacks::Find("chara/hero.dat").empty()) << config;
    }
}

TEST_F(Datapacks, FilesAreFoundIgnoringCaseButNotByTraversal) {
    Write("files", "Chara/Hero.dat", "replacement");
    Config(R"({"schema_version":1,"load_order":["files"]})");
    EXPECT_EQ(Texts("chara/hero.DAT"), std::vector<std::string>{"replacement"});
    EXPECT_TRUE(datapacks::Find("../Chara/Hero.dat").empty());
    EXPECT_TRUE(datapacks::Find("/Chara/Hero.dat").empty());
    EXPECT_TRUE(datapacks::Find("C:Chara/Hero.dat").empty());
    datapacks::Clear();
    EXPECT_TRUE(datapacks::Find("chara/hero.dat").empty());
}

TEST_F(Datapacks, UnlistedMissingAndZipPacksAreSkipped) {
    Write("unlisted", "Chara/Hero.dat", "unlisted");
    Write("listed", "Chara/Hero.dat", "listed");
    std::ofstream(directory / "datapacks/zipped.zip") << "PK";
    Config(R"({"schema_version":1,"load_order":["missing","zipped","listed"]})");
    EXPECT_EQ(Texts("chara/hero.dat"), std::vector<std::string>{"listed"});
}

// A document is taken from the highest-priority pack whose schema, kind and contents are valid.
TEST_F(Datapacks, AGameplayDocumentFallsBackPastBadOnes) {
    Write("good", "gameplay/items/prices.json", R"({"schema_version":2,"kind":"prices","value":2})");
    Write("newer", "gameplay/items/prices.json", R"({"schema_version":3,"kind":"prices","value":5})");
    Write("other kind", "gameplay/items/prices.json", R"({"schema_version":2,"kind":"items","value":4})");
    Write("bad field", "gameplay/items/prices.json", R"({"schema_version":2,"kind":"prices","value":"3"})");
    Write("out of range", "gameplay/items/prices.json", R"({"schema_version":2,"kind":"prices","value":10})");
    Write("not json", "gameplay/items/prices.json", "{");
    int        value = 0;
    const auto read = [&value] {
        return native_config::Read("items/prices.json", "prices", [&value](const native_config::Json &document) {
            const int number = native_config::Int(document, "value");
            if (number > 9) {
                throw std::runtime_error("value is past 9");
            }
            value = number;
        });
    };
    Config(R"({"schema_version":1,"load_order":["good","newer","other kind","bad field","out of range","not json"]})");
    EXPECT_TRUE(read());
    EXPECT_EQ(value, 2);
    Config(R"({"schema_version":1,"load_order":["newer","other kind","bad field","out of range","not json"]})");
    EXPECT_FALSE(read());
    EXPECT_FALSE(native_config::Read("items/missing.json", "prices", [](const native_config::Json &) {}));
}

} // namespace

} // namespace dc::test
