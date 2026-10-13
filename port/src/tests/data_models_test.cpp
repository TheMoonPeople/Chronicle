#include <gtest/gtest.h>

#include <cstdio>
#include <string>
#include <vector>

#include "../../../tools/dcdata/models.hpp"
#include "data_fixture.hpp"

// dcdata models (tools/dcdata/models.hpp) on a synthetic disc.

using namespace datafix;

namespace {

// A scene whose objects are the bones given, each the child of the one before it.
Bytes MakeMds(const std::vector<std::string> &bones) {
    Bytes data(dcdata::kMdsHeader + bones.size() * dcdata::kMdsObject, 0);
    Put32(data, 8, static_cast<std::uint32_t>(bones.size()));
    for (std::size_t i = 0; i < bones.size(); i++) {
        std::size_t object = dcdata::kMdsHeader + i * dcdata::kMdsObject;
        std::memcpy(&data[object + 8], bones[i].c_str(), bones[i].size() + 1);
        Put32(data, object + 44, static_cast<std::uint32_t>(static_cast<std::int32_t>(i) - 1));
    }
    return data;
}

Bytes Text(std::string_view text) {
    return Bytes(text.begin(), text.end());
}

std::string Report(const fs::path &iso) {
    fs::path    path = iso.parent_path() / "report.txt";
    std::FILE  *out = std::fopen(path.string().c_str(), "wb");
    int         result = dcdata::ModelReport(dcdata::OpenArchive(iso), out, "dun/mainchara/c01d.chr");
    std::fclose(out);
    EXPECT_EQ(result, 0);
    Bytes bytes = ReadBytes(path);
    return std::string(bytes.begin(), bytes.end());
}

} // namespace

TEST(DataModels, ParsesAScene) {
    std::optional<std::vector<dcdata::Bone>> bones = dcdata::ParseMds(MakeMds({"root", "spine", "head"}));
    ASSERT_TRUE(bones.has_value());
    ASSERT_EQ(bones->size(), 3u);
    EXPECT_EQ((*bones)[2].name, "head");
    EXPECT_EQ((*bones)[2].parent, 1);
    EXPECT_EQ((*bones)[0].parent, -1);
    EXPECT_FALSE(dcdata::ParseMds(Pattern(40, 1)).has_value());
}

TEST(DataModels, CountsMotionKeys) {
    EXPECT_EQ(dcdata::CountMotionKeys("MODEL \"a.mds\"\nKEY 0 10 1.0\n  KEY 11 20 1.0\r\nKEY_START 0\n"), 2);
}

// The report finds a character nested in an event pack and ranks it by the bones it shares.
TEST(DataModels, ReportRanksBySharedBones) {
    fs::path dir = TempDir("models");
    Bytes    toan = MakePack({
        {"c01d.cfg", Text("MODEL \"c01d.mds\"\nKEY 0 10 1.0\nKEY 11 20 1.0\n")},
        {"c01d.mds", MakeMds({"c01d", "kosi", "mune", "atama", "r_te"})        },
    });
    Bytes    seda = MakePack({
        {"c03c.cfg", Text("MODEL \"c03c.mds\"\nKEY 0 5 1.0\n")},
        {"c03c.mds", MakeMds({"c03c", "kosi", "mune", "atama"})},
    });
    Bytes    event = MakePack({
        {"script.stb", Pattern(64, 2)},
        {"c03c.chr",   seda          },
    });
    Disc     disc = MakeDisc({
        {"DUN\\MAINCHARA\\C01D.CHR", toan             },
        {"event\\ev01.pak",          event            },
        {"map\\m01.mds",             MakeMds({"floor"})},
        {"noise.bin",                Pattern(500, 3)  },
    });
    fs::path iso = dir / "disc.iso";
    WriteBytes(iso, MakeIso("DATA.DAT;1", disc.dat, "DATA.HD2;1", disc.hd2));

    std::string report = Report(iso);
    EXPECT_NE(report.find("reference: dun/mainchara/c01d.chr :: c01d.mds (5 bones, 2 motion keys)"), std::string::npos);
    EXPECT_NE(report.find("   3/5         4     1  event/ev01.pak :: c03c.chr :: c03c.mds"), std::string::npos) << report;
    EXPECT_NE(report.find("missing from the reference: c01d r_te"), std::string::npos);
    EXPECT_NE(report.find("parent   1 (kosi)"), std::string::npos);
    // A scene outside a pack is no character.
    EXPECT_EQ(report.find("map/m01.mds"), std::string::npos);
}
