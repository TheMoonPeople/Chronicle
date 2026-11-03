#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <vector>

#include "data_fixture.hpp"
#include "localize/localize.hpp"
#include "platform/png_image.hpp"
#include "render/texture_registry.hpp"
#include "ui/dungeon_plates.hpp"

namespace dc::test {

namespace fs = std::filesystem;

namespace {

// port/lang, which ships the plate.
fs::path PortLang() {
    return fs::path(__FILE__).parent_path().parent_path().parent_path() / "lang";
}

// The select's sheet as the disc has it: 512x384, indexed, every texel one opaque colour.
constexpr uint32_t kPainted = 0xFFFF00FFu;

render::PortDecodedTexture PaintedSheet() {
    render::PortDecodedTexture sheet;
    sheet.width = 0x200;
    sheet.height = 0x180;
    sheet.format = gfx::TextureFormat::Index8;
    sheet.palette[1] = kPainted;
    sheet.levels.push_back(std::vector<uint8_t>(static_cast<size_t>(sheet.width) * sheet.height, 1));
    return sheet;
}

uint32_t Texel(const render::PortDecodedTexture &sheet, int x, int y) {
    uint32_t colour = 0;
    std::memcpy(&colour, &sheet.levels[0][(static_cast<size_t>(y) * sheet.width + x) * 4], 4);
    return colour;
}

} // namespace

TEST(DungeonPlates, TheBlankPlateCoversEveryPaintedName) {
    localize::LocalizeSetDirectories({PortLang()});
    int                  width = 0;
    int                  height = 0;
    std::vector<uint8_t> plate;
    ASSERT_TRUE(platform::PngLoadRgba(PortLang() / "textures/plates/dungeon_name.png", width, height, plate));
    ASSERT_EQ(width, 256);
    ASSERT_EQ(height, 40);
    auto plate_texel = [&](int x, int y) {
        uint32_t colour = 0;
        std::memcpy(&colour, &plate[(static_cast<size_t>(y) * width + x) * 4], 4);
        return colour;
    };

    render::PortDecodedTexture sheet = PaintedSheet();
    ASSERT_TRUE(ui::dungeon_plates::Prepare("dunenter", sheet));
    EXPECT_TRUE(ui::dungeon_plates::Blank());
    EXPECT_EQ(sheet.format, gfx::TextureFormat::Rgba8);

    // Each of the six whole plates, the middle of its name.
    for (int icon = 0; icon < 6; icon++) {
        const int x = (icon / 3) << 8;
        const int y = (icon % 3) * 40 + 0x100;
        EXPECT_EQ(Texel(sheet, x + 128, y + 20), plate_texel(128, 20)) << "plate " << icon;
        EXPECT_EQ(Texel(sheet, x, y), plate_texel(0, 0)) << "plate " << icon << " corner";
    }
    // The Demon Shaft's halves: the plate's middle 36 rows, its left half then its right.
    EXPECT_EQ(Texel(sheet, 0x20 + 10, 0x80 + 10), plate_texel(10, 12));
    EXPECT_EQ(Texel(sheet, 0x20 + 10, 0xA4 + 10), plate_texel(138, 12));
    // The rest of the sheet as it was.
    EXPECT_EQ(Texel(sheet, 0x180, 0x10), kPainted);
    EXPECT_EQ(Texel(sheet, 0x10, 0x178), kPainted);

    // Another texture is left alone, and says nothing of the sheet.
    render::PortDecodedTexture other = PaintedSheet();
    EXPECT_FALSE(ui::dungeon_plates::Prepare("kaisou", other));
    EXPECT_EQ(other.format, gfx::TextureFormat::Index8);
    EXPECT_TRUE(ui::dungeon_plates::Blank());
    localize::LocalizeSetDirectories({});
}

TEST(DungeonPlates, WithoutThePlateTheNamesStayPainted) {
    localize::LocalizeSetDirectories({datafix::TempDir("dungeon_plates")});
    render::PortDecodedTexture sheet = PaintedSheet();
    EXPECT_FALSE(ui::dungeon_plates::Prepare("dunenter", sheet));
    EXPECT_FALSE(ui::dungeon_plates::Blank());
    EXPECT_EQ(sheet.format, gfx::TextureFormat::Index8);
    localize::LocalizeSetDirectories({});
}

} // namespace dc::test
