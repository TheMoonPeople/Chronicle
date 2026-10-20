#include "battlemenu/internal.hpp"

#include <string>

#include "battle_globals.hpp"
#include "caption_text.hpp"

extern s8 AlphabetEtcOffset[][2];

namespace {

// A name's letter in the name alphabet's codes: from 0xA2, A to Z then a to z; 0 for one the font is not asked for.
char32_t NameLetter(int code) {
    const int index = code - 0xA2;
    if (index >= 0 && index < 26) {
        return static_cast<char32_t>(U'A' + index);
    }
    if (index >= 26 && index < 52) {
        return static_cast<char32_t>(U'a' + index - 26);
    }
    return 0;
}

// The name alphabet's spacing, the condensing a long name gets letter against letter: the game's GetFontLRTumeW
// (static in ps2/src/battle_globals.cpp).
int Kerning(int slot, const short *name) {
    const int code = name[slot];
    int       kerning = 0;
    if (code >= 0xA2 && code < 0x100) {
        kerning += AlphabetEtcOffset[code - 0xA2][0];
        if (slot - 1 >= 0) {
            kerning += AlphabetEtcOffset[code - 0xA2][1];
        }
    }
    return kerning;
}

} // namespace

// The ally card's name. With a font to draw text from, it is drawn from it in the painted alphabet's look, centred on
// the span the game lays the alphabet's letters over; a name with a letter the font is not given keeps the alphabet.
PORT_OVERRIDE void CharaSelectNameDraw2(int x, int y, short *name, CTexture **textures, int blend_mode) {
    if (name == nullptr) {
        return;
    }

    int last = 9;
    while (name[last] == 0 && last > 0) {
        last--;
    }
    int  width = (last + 1) * 0x16;
    bool condensed = width >= 0xB0;
    int  kerning = 0;
    for (int slot = last; slot >= 0; slot--) {
        kerning += Kerning(slot, name);
    }
    if (condensed) {
        width -= kerning;
    }
    const int first_x = x + 0x44 + (width >> 1) - ((width / (last + 1)) >> 1);

    // Where the game puts each letter, right to left.
    std::u32string text;
    bool           letters = true;
    int            draw_x = first_x, left = first_x;
    for (int slot = last; slot >= 0; slot--) {
        const char32_t ch = NameLetter(name[slot]);
        letters = letters && ch != 0;
        text.insert(text.begin(), ch);
        left = draw_x;
        draw_x -= 0x14 - (condensed ? Kerning(slot, name) : 0);
    }
    if (letters) {
        const float right = static_cast<float>(first_x + 0x16);
        if (caption::DrawName(text, (static_cast<float>(left) + right) / 2, static_cast<float>(y), right - static_cast<float>(left), blend_mode)) {
            return;
        }
    }

    draw_x = first_x;
    for (; last >= 0; last--) {
        int       cell_x;
        int       cell_y;
        CTexture *texture = GetNameTextureInfo(textures, name[last], cell_x, cell_y);
        CRect_i_  cell_rect(cell_x, cell_y, 0x16, 0x17);
        if (texture != nullptr) {
            DrawMenu2DSprite(texture, CRect_i_(draw_x + 2, y + 2, 0x16, 0x15), cell_rect, 10, 10, 10, blend_mode);
            DrawMenu2DSprite(texture, CRect_i_(draw_x, y, 0x16, 0x15), cell_rect, blend_mode);
        }
        draw_x -= 0x14 - (last >= 0 && condensed ? Kerning(last, name) : 0);
    }
}
