#include "ui/dungeon_plates.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

#include "localize/localize.hpp"
#include "localize/localize_texture.hpp"
#include "main.hpp"
#include "menuetc.hpp"
#include "platform/png_image.hpp"
#include "render/texture_registry.hpp"
#include "text/gametext.hpp"
#include "ui/framework.hpp"

namespace dc::ui::dungeon_plates {

using namespace dc::game;

namespace {

constexpr int kPlateWidth = 256;
constexpr int kPlateHeight = 40;

// Where the sheet keeps each dungeon's plate, as DunEnterBoardWaku cuts them: the first six whole, the Demon
// Shaft's in two halves of its middle 36 rows.
struct Piece {
    int sheet_x, sheet_y; /**< Where in "dunenter". */
    int plate_x, plate_y; /**< Where in the blank plate. */
    int width, height;
};

constexpr std::array kPieces = {
    Piece{0x000, 0x100, 0,   0, 256, 40},
    Piece{0x000, 0x128, 0,   0, 256, 40},
    Piece{0x000, 0x150, 0,   0, 256, 40},
    Piece{0x100, 0x100, 0,   0, 256, 40},
    Piece{0x100, 0x128, 0,   0, 256, 40},
    Piece{0x100, 0x150, 0,   0, 256, 40},
    Piece{0x020, 0x080, 0,   2, 128, 36},
    Piece{0x020, 0x0A4, 128, 2, 128, 36},
};

// Each dungeon's name in the system messages, as the world map has it.
constexpr std::array kNameMessages = {11, 13, 16, 23, 21, 22, 25};

bool g_blank = false;

// The painted names: capitals 19 pixels tall from row 10, at most 227 wide, centred; yellow, outlined green.
// Label puts the capitals 5 below its y, and the font's capitals are narrower than the painting's by this much.
constexpr int   kNameCentre = 128;
constexpr int   kNameWidest = 227;
constexpr int   kNameTop = 5;
constexpr float kNameScale = 1.31f;
constexpr float kNameWiden = 1.12f;

std::string Upper(std::string_view utf8) {
    std::string out;
    for (std::size_t at = 0; at < utf8.size(); at++) {
        const auto byte = static_cast<unsigned char>(utf8[at]);
        if (byte >= 'a' && byte <= 'z') {
            out += static_cast<char>(byte - 0x20);
        } else if (byte == 0xC3 && at + 1 < utf8.size()) {
            // Latin-1's lower case letters (but the division sign and the y with diaeresis) are their capitals plus 0x20.
            auto next = static_cast<unsigned char>(utf8[++at]);
            if (next >= 0xA0 && next <= 0xBE && next != 0xB7) {
                next -= 0x20;
            }
            out += static_cast<char>(byte);
            out += static_cast<char>(next);
        } else {
            out += static_cast<char>(byte);
        }
    }
    while (!out.empty() && out.back() == ' ') {
        out.pop_back();
    }
    return out;
}

std::string NameOf(int dungeon) {
    if (SystemMes == nullptr || dungeon < 0 || dungeon >= static_cast<int>(kNameMessages.size())) {
        return {};
    }
    // The system messages are read through a window, as the game reads them; its own messages are put back.
    short *const saved = CommonMenuMes3.buff;
    CommonMenuMes3.buff = SystemMes;
    const short *codes = CommonMenuMes3.GetTextLineDataTop(kNameMessages[dungeon]);
    CommonMenuMes3.buff = saved;
    if (codes == nullptr) {
        return {};
    }
    const std::string text = text::GameTextDecode(codes);
    return Upper(text.substr(0, text.find('\n')));
}

} // namespace

bool Prepare(const char *name, render::PortDecodedTexture &texture) {
    if (name == nullptr || std::string_view(name) != "dunenter") {
        return false;
    }
    g_blank = false;
    const std::filesystem::path path = localize::LocalizeFindFile("textures/plates/dungeon_name.png");
    int                         width = 0;
    int                         height = 0;
    std::vector<uint8_t>        plate;
    if (path.empty() || texture.levels.empty() || texture.width < 0x200 || texture.height < 0x178 ||
        !platform::PngLoadRgba(path, width, height, plate)) {
        return false;
    }
    if (width != kPlateWidth || height != kPlateHeight) {
        plate = localize::LocalizeResample(plate.data(), width, height, kPlateWidth, kPlateHeight);
    }

    // The sheet as straight RGBA, as a language's picture leaves it (localize_texture.cpp).
    const auto           sheet_width = static_cast<int>(texture.width);
    const auto           sheet_height = static_cast<int>(texture.height);
    std::vector<uint8_t> rgba(static_cast<std::size_t>(sheet_width) * sheet_height * 4);
    for (std::size_t pixel = 0; pixel < static_cast<std::size_t>(sheet_width) * sheet_height; pixel++) {
        if (texture.format == gfx::TextureFormat::Index8) {
            const uint32_t colour = texture.palette[texture.levels[0][pixel]];
            std::memcpy(&rgba[pixel * 4], &colour, 4);
        } else {
            std::memcpy(&rgba[pixel * 4], &texture.levels[0][pixel * 4], 4);
        }
    }
    for (const Piece &piece : kPieces) {
        for (int y = 0; y < piece.height; y++) {
            std::memcpy(&rgba[((static_cast<std::size_t>(piece.sheet_y) + y) * sheet_width + piece.sheet_x) * 4],
                        &plate[((static_cast<std::size_t>(piece.plate_y) + y) * kPlateWidth + piece.plate_x) * 4], static_cast<std::size_t>(piece.width) * 4);
        }
    }
    std::vector<std::vector<uint8_t>> levels;
    for (std::size_t level = 0; level < texture.levels.size(); level++) {
        const int level_width = std::max(1, sheet_width >> level);
        const int level_height = std::max(1, sheet_height >> level);
        levels.push_back(level == 0 ? rgba : localize::LocalizeResample(rgba.data(), sheet_width, sheet_height, level_width, level_height));
    }
    texture.levels = std::move(levels);
    texture.format = gfx::TextureFormat::Rgba8;
    texture.has_alpha = true;
    texture.four_bit = false;
    g_blank = true;
    return true;
}

bool Blank() {
    return g_blank;
}

void DrawName(int dungeon, int x, int y, int alpha) {
    const std::string name = NameOf(dungeon);
    if (name.empty()) {
        return;
    }
    TextStyle style;
    style.weight = FontWeight::Normal;
    style.align = Alignment::Center;
    style.scale = kNameScale;
    style.xscale = kNameWiden;
    style.drop_shadow = false;
    style.use_custom_rgb = true;
    style.fill_r = 254;
    style.fill_g = 212;
    style.fill_b = 84;
    style.outline_width = 2.0f;
    style.outline_r = 57;
    style.outline_g = 84;
    style.outline_b = 59;
    Label label(name, style);
    // A name wider than the painted ones ever were is drawn narrower, never smaller.
    if (const int width = label.GetWidth(); width > kNameWidest) {
        style.xscale = kNameWiden * static_cast<float>(kNameWidest) / static_cast<float>(width);
        label.SetStyle(style);
    }
    // The Demon Shaft's plate is cut two rows lower in the plate, so drawn two rows higher.
    const int top = dungeon == 6 ? kNameTop - 2 : kNameTop;
    label.Draw(x + kNameCentre, y + top, alpha);
}

} // namespace dc::ui::dungeon_plates
