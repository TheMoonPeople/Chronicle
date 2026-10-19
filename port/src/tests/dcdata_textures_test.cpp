#include <gtest/gtest.h>

#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "../../../tools/dcdata/textures.hpp"
#include "platform/imagefile.hpp"
#include "platform_fixture.hpp"

namespace {

namespace fs = std::filesystem;

// A TIM2 with one 32-bit picture of w x h, every pixel (r, g, b, 0x80), inside an IMG container named `name`.
std::vector<unsigned char> Image(const std::string &name, unsigned w, unsigned h, unsigned char red) {
    std::vector<unsigned char> out(0x10, 0);
    std::memcpy(out.data(), "IMG", 3);
    out[4] = 1; // one entry: a 32-byte name, then the offset of its TIM2
    std::vector<unsigned char> label(16, 0);
    std::memcpy(label.data(), name.data(), name.size());
    out.insert(out.end(), label.begin(), label.end());
    out.resize(out.size() + 0x20, 0);
    out[0x10 + 32] = 0x40;
    std::vector<unsigned char> tim(0x10 + 0x30, 0);
    std::memcpy(tim.data(), "TIM2", 4);
    tim[4] = 4;
    tim[6] = 1;
    dcdata::Put32(&tim[0x10 + 8], w * h * 4);
    tim[0x10 + 12] = 0x30;
    tim[0x10 + 0x13] = 3;
    tim[0x10 + 0x14] = static_cast<unsigned char>(w);
    tim[0x10 + 0x16] = static_cast<unsigned char>(h);
    for (unsigned i = 0; i < w * h; i++) {
        unsigned char px[4] = {red, static_cast<unsigned char>(i), 7, 0x80};
        tim.insert(tim.end(), px, px + 4);
    }
    out.insert(out.end(), tim.begin(), tim.end());
    return out;
}

void Put(const fs::path &path, const std::vector<unsigned char> &bytes) {
    fs::create_directories(path.parent_path());
    std::ofstream(path, std::ios::binary).write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
}

struct Folders {
    fs::path root = fs::temp_directory_path() / ("dc_dcdata_textures_" + std::to_string(dc::test::ProcessId()));
    fs::path data = root / "data";
    fs::path assets = root / "assets";

    Folders() {
        std::error_code error;
        fs::remove_all(root, error);
        fs::create_directories(data);
    }
    ~Folders() {
        std::error_code error;
        fs::remove_all(root, error);
    }
};

TEST(DcdataTextures, AFileBelongsToTheAreaItsPathNames) {
    EXPECT_EQ(dcdata::AreaOf("dun/d03/map.img"), "d03");
    EXPECT_EQ(dcdata::AreaOf("dun/pack/d02b05.img"), "d02");
    EXPECT_EQ(dcdata::AreaOf("dun/pack/d021.img"), "");
    EXPECT_EQ(dcdata::AreaOf("dun/d04x.img"), "d04");
    EXPECT_EQ(dcdata::AreaOf("gedit/e03/map.img"), "e03");
    EXPECT_EQ(dcdata::AreaOf("gedit/system/esys.pak"), "");
    EXPECT_EQ(dcdata::AreaOf("chara/hero.chr"), "");
}

TEST(DcdataTextures, ALanguagesFolderIsNamedByItsDirectory) {
    EXPECT_EQ(dcdata::LanguageOf("commenu/a_eng/menu.img"), "a_eng");
    EXPECT_EQ(dcdata::LanguageOf("commenu/a_fre/eastk/x.img"), "a_fre");
    EXPECT_EQ(dcdata::LanguageOf("commenu/menu.img"), "");
}

TEST(DcdataTextures, APictureDecodesWithPs2AlphaDoubled) {
    std::vector<unsigned char> file = Image("tex", 2, 2, 9);
    auto pictures = dcdata::DecodeTim2(file, 0x40, false);
    ASSERT_EQ(pictures.size(), 1u);
    EXPECT_EQ(pictures[0].width, 2u);
    EXPECT_EQ(pictures[0].rgba[0], 9);
    EXPECT_EQ(pictures[0].rgba[3], 255);
}

TEST(DcdataTextures, PngsRoundTripThroughThePortsReader) {
    std::vector<uint8_t> rgba(64 * 32 * 4);
    for (size_t i = 0; i < rgba.size(); i++) {
        rgba[i] = static_cast<uint8_t>((i / 8) * 5);
    }
    Folders f;
    auto png = dcdata::EncodePng(rgba.data(), 64, 32);
    Put(f.root / "p.png", png);
    int w = 0;
    int h = 0;
    std::vector<uint8_t> back;
    ASSERT_TRUE(imagefile::LoadPng(f.root / "p.png", w, h, back));
    EXPECT_EQ(w, 64);
    EXPECT_EQ(h, 32);
    EXPECT_EQ(back, rgba);
}

TEST(DcdataTextures, APictureTheAreasShareIsWrittenOnceAndADifferentOneInItsArea) {
    Folders f;
    Put(f.data / "dun/d01/a.img", Image("same", 4, 4, 1));
    Put(f.data / "dun/d02/a.img", Image("same", 4, 4, 1));
    Put(f.data / "dun/d03/a.img", Image("same", 4, 4, 2)); // different here
    Put(f.data / "dun/d01/b.img", Image("only", 4, 4, 3));
    auto summary = dcdata::ConvertTextures(f.data, f.assets);
    EXPECT_EQ(summary.scanned, 4u);
    EXPECT_EQ(summary.written, 3u);
    EXPECT_TRUE(fs::exists(f.assets / "textures" / "same.png"));
    EXPECT_TRUE(fs::exists(f.assets / "d03" / "textures" / "same.png"));
    EXPECT_FALSE(fs::exists(f.assets / "d01" / "textures" / "same.png"));
    EXPECT_FALSE(fs::exists(f.assets / "d02" / "textures" / "same.png"));
    EXPECT_TRUE(fs::exists(f.assets / "textures" / "only.png"));
    int w = 0;
    int h = 0;
    std::vector<uint8_t> px;
    ASSERT_TRUE(imagefile::LoadPng(f.assets / "d03" / "textures" / "same.png", w, h, px));
    EXPECT_EQ(px[0], 2);
}

TEST(DcdataTextures, ANameInTwoSizesIsWrittenWithTheSize) {
    Folders f;
    Put(f.data / "dun/d01/a.img", Image("two", 4, 4, 1));
    Put(f.data / "dun/d01/b.img", Image("two", 8, 8, 1));
    dcdata::ConvertTextures(f.data, f.assets);
    EXPECT_TRUE(fs::exists(f.assets / "textures" / "two_4x4.png"));
    EXPECT_TRUE(fs::exists(f.assets / "textures" / "two_8x8.png"));
}

TEST(DcdataTextures, ARerunKeepsAPictureAnArtistChanged) {
    Folders f;
    Put(f.data / "dun/d01/a.img", Image("keep", 4, 4, 1));
    dcdata::ConvertTextures(f.data, f.assets);
    Put(f.assets / "textures" / "keep.png", {1, 2, 3});
    auto again = dcdata::ConvertTextures(f.data, f.assets);
    EXPECT_EQ(again.written, 0u);
    EXPECT_EQ(again.kept, 1u);
    EXPECT_EQ(fs::file_size(f.assets / "textures" / "keep.png"), 3u);
    dcdata::ConvertTextures(f.data, f.assets, nullptr, true);
    EXPECT_GT(fs::file_size(f.assets / "textures" / "keep.png"), 3u);
}

} // namespace
