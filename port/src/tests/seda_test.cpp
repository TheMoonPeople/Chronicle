#include <gtest/gtest.h>

#include <cstring>
#include <string>
#include <vector>

#include "../../../tools/dcdata/models.hpp"
#include "character.hpp"
#include "data_fixture.hpp"
#include "dataread.hpp"
#include "dataread_port.hpp"
#include "dun/gameloop.hpp"
#include "gameutil.hpp"
#include "platform/paths.hpp"
#include "seda.hpp"
#include "sedamod.hpp"

// The Seda mod: dcdata's pack (tools/dcdata/dcdata.hpp, MakeSedaPack) and the port's use of it
// (port/src/dataread.cpp, port/src/seda.cpp).

using namespace datafix;

namespace {

// A scene of the named objects, the first holding model data at a made-up offset.
Bytes Scene(const std::vector<std::string> &names) {
    Bytes data(16 + names.size() * 112, 0);
    Put32(data, 8, static_cast<std::uint32_t>(names.size()));
    for (std::size_t i = 0; i < names.size(); i++) {
        std::size_t object = 16 + i * 112;
        std::memcpy(&data[object + 8], names[i].c_str(), names[i].size() + 1);
        Put32(data, object + 40, 0x400);
        Put32(data, object + 44, static_cast<std::uint32_t>(static_cast<std::int32_t>(i) - 1));
    }
    return data;
}

std::string ObjectName(const Bytes &scene, int index) {
    return reinterpret_cast<const char *>(&scene[16 + index * 112 + 8]);
}

std::uint32_t ObjectData(const Bytes &scene, int index) {
    return dcdata::Le32(&scene[16 + index * 112 + 40]);
}

Bytes Text(std::string_view text) {
    Bytes data(text.begin(), text.end());
    data.push_back(0);
    return data;
}

const dcdata::SedaVariant &King() {
    return *dcdata::FindSedaVariant("king");
}

const dcdata::SedaVariant &Seda() {
    return *dcdata::FindSedaVariant("seda");
}

// Seda's cutscene pack (c07a) as the disc has it, cut down.
Bytes C07Source() {
    return MakePack({
        {"c07a.cfg", Text("//Seda\nMODEL \"c07a.mds\"\nMOTION 0, \"c07a.mot\", \"c07a.bbp\", \"c07a.wgt\"\n"
                          "KEY_START 0\nKEY 10, 20, 0.1 //stand 0\nMOTION_END\nEVENT 43,0,0\n")},
        {"c07a.mds", Scene({"skel_1_1", "hip", "l_hand", "r_hand3", "r_hand", "r_hand4", "obj10"})},
        {"c07a.mot", Pattern(64, 3)},
    });
}

// King Seda's cutscene pack as the disc has it, cut down.
Bytes SedaSource() {
    return MakePack({
        {"e502p68b.cfg", Text("//King\nIMG 0,\"p68b01.img\"\nIMG_END\nMODEL \"e502p68b.mds\"\nSHADOW_MODEL "
                              "\"e502p68s.mds\"\nMOTION 0, \"e502p68b.mot\", \"e502p68b.bbp\", \"e502p68b.wgt\"\n"
                              "KEY_START 0\nKEY \t10, \t20, \t0.1\t//stand 0\nKEY \t40, \t60, \t0.2\t//walk 1\n"
                              "MOTION_END\nEVENT 42,0,0\n")},
        {"e502p68b.mds", Scene({"g_1", "c", "cube40_3_2", "cube40_3_3", "seiken__m"})},
        {"e502p68s.mds", Scene({"g_1", "seiken__m"})},
        {"e502p68b.mot", Pattern(64, 1)},
        {"p68b01.img", Pattern(32, 2)},
    });
}

} // namespace

TEST(Seda, PackPutsSedasMotionsInToansOrder) {
    std::vector<dcdata::PackMember> pack = dcdata::ReadPack(dcdata::MakeSedaPack(SedaSource(), King()));
    ASSERT_EQ(pack.size(), 5u);
    EXPECT_EQ(pack[0].name, "base.cfg");
    std::string config(pack[0].data.begin(), pack[0].data.end());
    // Text only: the game's reader takes a NUL for a letter of a word.
    EXPECT_EQ(config.find('\0'), std::string::npos);
    EXPECT_EQ(config.back(), '\n');
    EXPECT_EQ(dcdata::CountMotionKeys(config), 41);
    EXPECT_NE(config.find("KEY_START 0\n\tKEY\t10,\t20,\t0.1,\t//0 stand: stand\n"), std::string::npos) << config;
    EXPECT_NE(config.find("\tKEY\t420,\t445,\t0.3,\t//40 combo 3: fight the assassin\nMOTION_END\nFOOT 43,53\n"),
              std::string::npos);
    // His own model, textures and events stay.
    EXPECT_NE(config.find("MODEL \"e502p68b.mds\"\n"), std::string::npos);
    EXPECT_NE(config.find("IMG 0,\"p68b01.img\"\n"), std::string::npos);
    EXPECT_NE(config.find("EVENT 42,0,0\n"), std::string::npos);
    EXPECT_EQ(config.find("//walk 1"), std::string::npos);

    // The sword is the weapon's place and no longer drawn; the right hand holds items.
    EXPECT_EQ(pack[1].name, "e502p68b.mds");
    EXPECT_EQ(ObjectName(pack[1].data, 2), "item");
    EXPECT_EQ(ObjectData(pack[1].data, 2), 0x400u);
    EXPECT_EQ(ObjectName(pack[1].data, 4), "weapon");
    EXPECT_EQ(ObjectData(pack[1].data, 4), 0u);
    // The shadow model is left as it was.
    EXPECT_EQ(ObjectName(pack[2].data, 1), "seiken__m");
}

// Seda keeps his sword and holds the equipped weapon at his right hand.
TEST(Seda, C07PackKeepsHisSword) {
    std::vector<dcdata::PackMember> pack = dcdata::ReadPack(dcdata::MakeSedaPack(C07Source(), Seda()));
    ASSERT_EQ(pack.size(), 3u);
    std::string config(pack[0].data.begin(), pack[0].data.end());
    EXPECT_EQ(dcdata::CountMotionKeys(config), 41);
    EXPECT_NE(config.find("// seda in Toan's place, made by dcdata from gedit/s99/chara/c07a.chr\n"), std::string::npos);
    EXPECT_NE(config.find("\tKEY\t269,\t285,\t0.4,\t//11 attack 1: sweep\n"), std::string::npos) << config;
    EXPECT_NE(config.find("MOTION_END\nFOOT 43,53\nEVENT 43,0,0\n"), std::string::npos);
    EXPECT_EQ(ObjectName(pack[1].data, 5), "weapon");
    EXPECT_EQ(ObjectData(pack[1].data, 5), 0x400u);
    EXPECT_EQ(ObjectName(pack[1].data, 3), "item");
    EXPECT_EQ(ObjectName(pack[1].data, 6), "obj10");
}

TEST(Seda, EveryVariantHasToansKeys) {
    for (const dcdata::SedaVariant &variant : dcdata::kSedaVariants) {
        EXPECT_EQ(variant.keys.size(), 41u) << variant.name;
    }
}

TEST(Seda, PackNeedsSedasSword) {
    Bytes source = MakePack({
        {"e502p68b.cfg", Text("MODEL \"e502p68b.mds\"\nKEY_START 0\nMOTION_END\n")},
        {"e502p68b.mds", Scene({"g_1", "cube40_3_2"})                             },
    });
    EXPECT_THROW(dcdata::MakeSedaPack(source, King()), dcdata::Error);
}

// Extraction builds the pack, and while Seda stands in Toan's dungeon pack is his.
TEST(Seda, ToansDungeonPackIsSedasWhileActive) {
    fs::path dir = TempDir("seda");
    Disc     disc = MakeDisc({
        {"dun\\mainchara\\c01d.chr",    Text("toan")},
        {"gedit\\s99\\chara\\c07a.chr", C07Source() },
    });
    WriteBytes(dir / "disc.iso", MakeIso("DATA.DAT;1", disc.dat, "DATA.HD2;1", disc.hd2));
    dcdata::Extract(dcdata::OpenArchive(dir / "disc.iso"), dir / "data", nullptr);
    ASSERT_TRUE(fs::is_regular_file(dir / "data" / "normalized" / dcdata::kSedaPack));
    PathsSetDataRoot(dir / "data");
    PathsSetSaveRoot(dir / "save");
    InitCDFile();

    alignas(64) static char buffer[8192];
    int                     size = 0;
    char                    path[] = "dun/mainchara/c01d.chr";
    SedaSetForced(false);
    ASSERT_EQ(LoadFile2(path, buffer, &size, 0), 1);
    EXPECT_EQ(std::string(buffer), "toan");

    SedaSetForced(true);
    ASSERT_EQ(LoadFile2(path, buffer, &size, 0), 1);
    EXPECT_EQ(std::string(buffer), "base.cfg");
    SedaSetForced(false);
}

// Toan's attack code times things by frames of his motion; Seda's time is turned back into those.
TEST(Seda, TimeStandsForToansFrame) {
    MOTION_INFO  keys[41] = {};
    MOTION_INFO *saved_info = CharaMain.motion_type.motion_info;
    int          saved_motion = CharaMain.motion_no;
    keys[15] = {269, 280, 0.4f, 0}; // Seda's charged attack start; Toan's is 180-191, the leap at 187
    keys[16] = {280, 280, 0.0f, 0};
    keys[36] = {269, 285, 0.4f, 0}; // Seda's first combo swing; Toan's is 820-830, the hit at 825-828
    CharaMain.motion_type.motion_info = keys;

    SedaSetForced(false);
    CharaMain.motion_no = 15;
    EXPECT_FLOAT_EQ(TOAN_TIME(276.0f), 276.0f);

    SedaSetForced(true);
    EXPECT_FLOAT_EQ(TOAN_TIME(269.0f), 180.0f);
    EXPECT_FLOAT_EQ(TOAN_TIME(276.0f), 187.0f); // the leap
    EXPECT_FLOAT_EQ(TOAN_TIME(280.0f), 191.0f);
    EXPECT_LT(TOAN_TIME(250.0f), 180.0f); // outside the key, outside Toan's
    CharaMain.motion_no = 36;
    EXPECT_FLOAT_EQ(TOAN_TIME(277.0f), 825.0f); // the hit
    CharaMain.motion_no = 16;
    EXPECT_FLOAT_EQ(TOAN_TIME(280.0f), 191.0f); // a key of no length stands for Toan's start

    SedaSetForced(false);
    CharaMain.motion_type.motion_info = saved_info;
    CharaMain.motion_no = saved_motion;
}
