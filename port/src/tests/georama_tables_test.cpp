// nlohmann/json before the game headers, whose one-letter macros it cannot stand.
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <cstring>
#include <filesystem>
#include <format>
#include <fstream>
#include <string>
#include <vector>

#include "dngstatusdata.hpp"
#include "dun/gameloop.hpp"
#include "dungeonparts.hpp"
#include "editpartsdata.hpp"
#include "georama/tables.hpp"
#include "platform/datapacks.hpp"
#include "savedata.hpp"
#include "tests/platform_fixture.hpp"
#include "userstatus.hpp"

namespace dc::test {

using namespace dc::game;
using namespace dc::platform;

namespace {

namespace fs = std::filesystem;

const char *Name(const char *name) {
    return name != nullptr ? name : "(none)";
}

// Every value the game reads from the tables, the model names by their text.
std::string Snapshot() {
    std::string out;
    for (int town = 0; town < 6; town++) {
        for (const EDIT_PARTS_ATRA &part : EditPartsData[town].parts) {
            out += std::format("{} {} {} {} {};", part.max, part.stackable, part.tex_no, part.msg_no, part.kind);
            for (const EDIT_CHIP_ATTACH_DATA &element : part.elements) {
                out += std::format("{} {} {} [{}|{}|{}|{}];", element.id, element.required_element, element.npc_no, Name(element.names[0]), Name(element.names[1]),
                                   Name(element.names[2]), Name(element.names[3]));
            }
        }
        for (const EDIT_ELEMENT_ATRA &element : EditElementData[town]) {
            out += std::format("{} {} {};", element.tex_no, element.msg_no, element.unk_08);
        }
        for (const ATRA_APPEAR *row = AtraAppearData[town];; row++) {
            out += std::format("{} {} {};", row->id, row->floor, row->count);
            if (row->id == -1) {
                break;
            }
        }
    }
    return out;
}

// A data folder with a datapacks folder beside it, and the exported documents to start packs from.
class GeoramaTables : public ::testing::Test {
protected:
    fs::path    directory = fs::canonical(fs::temp_directory_path()) / std::format("dc_georama_{}", ProcessId());
    fs::path    data = directory / "data";
    std::string compiled;

    void SetUp() override {
        fs::remove_all(directory);
        fs::create_directories(data);
        georama::Load();
        compiled = Snapshot();
        ASSERT_TRUE(georama::Export(directory / "exported"));
    }

    void TearDown() override {
        datapacks::Clear();
        georama::Load();
        fs::remove_all(directory);
    }

    nlohmann::json Document(const std::string &name) const {
        return nlohmann::json::parse(std::ifstream(directory / "exported/gameplay/georama" / name));
    }

    void Pack(const std::string &name, const nlohmann::json &document) const {
        const fs::path file = directory / "datapacks/pack/gameplay/georama" / name;
        fs::create_directories(file.parent_path());
        std::ofstream(file) << document.dump();
    }

    void Load() const {
        std::ofstream(directory / "datapacks/datapacks.json") << R"({"schema_version":1,"load_order":["pack"]})";
        datapacks::Load(data);
        georama::Load();
    }
};

} // namespace

TEST_F(GeoramaTables, TheExportedDocumentsGiveTheCompiledTablesBack) {
    Pack("parts.json", Document("parts.json"));
    Pack("atla.json", Document("atla.json"));
    Load();
    EXPECT_EQ(Snapshot(), compiled);
}

TEST_F(GeoramaTables, APackChangesATownsPartsAndADungeonsAtla) {
    nlohmann::json parts = Document("parts.json");
    for (nlohmann::json &part : parts["towns"][0]["parts"]) {
        if (part["part"] == 14) {
            part["max"] = 35;
        }
    }
    nlohmann::json atla = Document("atla.json");
    atla["dungeons"][0]["atla"].push_back({
        {"part",  14          },
        {"floor", "first_half"},
        {"count", 2           }
    });
    Pack("parts.json", parts);
    Pack("atla.json", atla);
    Load();

    EXPECT_EQ(EditPartsData[0].parts[14].max, 35);
    EXPECT_STREQ(EditPartsData[1].parts[0].elements[0].names[0], "taimatu") << "another town as compiled";
    int rows = 0;
    while (AtraAppearData[0][rows].id != -1) {
        rows++;
    }
    EXPECT_EQ(rows, 79);
    EXPECT_EQ(AtraAppearData[0][78].id, 14);
    EXPECT_EQ(AtraAppearData[0][78].floor, -1);
    EXPECT_EQ(AtraAppearData[0][78].count, 2);
}

TEST_F(GeoramaTables, ADungeonDealsOutTheDocumentsAtlaOnItsFirstVisit) {
    alignas(64) static CSaveData save;
    CUserStatus *const           saved = UserStatus;
    // Every Atla the cave's floors ask for, as BtAtraListMake leaves them in a new game.
    const auto dealt = [] {
        save.Initialize();
        UserStatus = save.GetDngStatus();
        BtAtraListMake(0);
        int atla = 0;
        for (int floor = 0; floor < 40; floor++) {
            atla += save.GetDngStatus()->GetMaxAtraNum(0, floor);
        }
        return atla;
    };
    const int compiled_atla = dealt();

    nlohmann::json atla = Document("atla.json");
    atla["dungeons"][0]["atla"].push_back({
        {"part",  14          },
        {"floor", "first_half"}
    });
    atla["dungeons"][0]["atla"].push_back({
        {"part",  14},
        {"floor", 2 }
    });
    Pack("atla.json", atla);
    Load();
    EXPECT_EQ(dealt(), compiled_atla + 2);
    UserStatus = saved;
}

TEST_F(GeoramaTables, ADocumentPartsLeaveOutGiveTheCompiledTables) {
    nlohmann::json atla = Document("atla.json");
    atla["dungeons"] = nlohmann::json::array({atla["dungeons"][2]});
    atla["dungeons"][0]["atla"] = nlohmann::json::array({
        {{"element", 5}, {"floor", 3}}
    });
    Pack("atla.json", atla);
    Load();
    EXPECT_EQ(AtraAppearData[2][0].id, 45);
    EXPECT_EQ(AtraAppearData[2][0].floor, 3);
    EXPECT_EQ(AtraAppearData[2][0].count, 1);
    EXPECT_EQ(AtraAppearData[2][1].id, -1);
    EXPECT_EQ(AtraAppearData[0][0].id, 0) << "Divine Beast Cave as compiled";
}

TEST_F(GeoramaTables, ADocumentTheGameCouldNotUseIsRefused) {
    const auto refused = [this](const std::string &name, const nlohmann::json &document) {
        Pack(name, document);
        Load();
        EXPECT_EQ(Snapshot(), compiled) << document.dump().substr(0, 200);
    };

    nlohmann::json atla = Document("atla.json");
    for (int i = 0; i < 30; i++) {
        atla["dungeons"][0]["atla"].push_back({
            {"part",  1},
            {"floor", 1}
        });
    }
    refused("atla.json", atla); // more than the save's 100 Atla

    atla = Document("atla.json");
    atla["dungeons"][0]["atla"].push_back({
        {"part",  1 },
        {"floor", 16}
    });
    refused("atla.json", atla); // Divine Beast Cave has 15 floors

    atla = Document("atla.json");
    atla["dungeons"][0]["atla"].push_back({
        {"part",  1            },
        {"floor", "second_half"},
        {"count", 60           }
    });
    refused("atla.json", atla); // more than its second half's floors hold

    atla = Document("atla.json");
    atla["dungeons"][0]["atla"].push_back({
        {"part",    1},
        {"element", 2},
        {"floor",   1}
    });
    refused("atla.json", atla);

    nlohmann::json parts = Document("parts.json");
    parts["towns"][0]["town"] = "brownboo";
    refused("parts.json", parts);

    parts = Document("parts.json");
    parts["towns"][0]["parts"].push_back({
        {"part",         30},
        {"max",          1 },
        {"stackable",    0 },
        {"picture",      0 },
        {"name_message", 0 },
        {"kind",         0 }
    });
    refused("parts.json", parts);
}

} // namespace dc::test
