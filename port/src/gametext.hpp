#pragma once

#include <cstddef>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "clsmes.hpp"

// Port text in the game's own message font. The PAL font is the 88 characters of gaiji.img's
// letter grid (codes -0x2DF to -0x288) and the accented letters DrawGaijiFont builds from them
// with an accent drawn over (to -0x254); every PAL language's mes_tex_N.pak draws the same
// characters at the same codes, so one table serves them all.

// The code the font draws ch with, or 0 where it has none. ' ' is MES_CODE_SPACE and '\n'
// MES_CODE_NEWLINE.
s16 GameTextCode(char32_t ch);

// The character code draws, or 0 where code is not one (a control code, an icon).
char32_t GameTextChar(s16 code);

// Appends utf8's codes and MES_CODE_END to out. A control code, one that is not a character, is
// written in braces by its name ("{page}", "{cyan}...{/color}", "{L1}", "{value3}", "{name1}", "{wait 12}",
// "{icon 32}"; docs/LOCALIZATION.md lists them) or by its number, "{-253}", the s16 the game stores. "{{" is a '{'. Typographic quotes and dashes
// become the font's own. A character the font lacks, or a byte that is not UTF-8, becomes '?';
// gives back how many did.
int GameTextEncode(std::string_view utf8, std::vector<s16> &out);

// The text of codes up to MES_CODE_END, as GameTextEncode reads it back: a control code is written by
// its name where it has one, else as "{N}", and '{' as "{{".
std::string GameTextDecode(const s16 *codes);

// A message file of port text, laid out as ClsMes::SetBuff reads one: the count, then each
// message's number and where its text starts, then the texts, all s16. So an id is -0x8000 to
// 0x7FFF. Every message must start within 0x7FFF codes of &buff[1 + count]; the last message by
// id may extend beyond that range.
class GameTextFile {
public:
    // Sets message id's text; gives back how many characters GameTextEncode replaced, or -1,
    // leaving the file as it was, where the id or the file would not fit.
    int Set(int id, std::string_view utf8);

    // The file, for ClsMes::SetBuff; valid until the next Set.
    short *Data();

private:
    std::map<int, std::vector<s16>> messages_;
    std::vector<s16>                data_;
    size_t                          codes_ = 0;
    bool                            dirty_ = true;
};

// One piece of port text drawn as the menus draw their help line (CommonMenuMes2 under
// InitMenuMesSet(MENU_MES_SET_ALLMENU)): the menu font, white with a black edge, shown whole.
// Each holds its own ClsMes, so labels, values and a help line are one each.
class GameText {
public:
    GameText();
    GameText(const GameText &) = delete;
    GameText &operator=(const GameText &) = delete;

    // Lays the text out again when it changed; gives back how many characters became '?', or -1
    // where it does not fit the window's MES_WIN_LINE_MAX laid-out characters (names and values
    // that "{N}" codes name count as theirs), and then nothing draws and Width is 0.
    int Set(std::string_view utf8);

    // A FontColor, as FontColorTbl holds them; FONT_COLOR_WHITE until set.
    void SetColour(u32 colour);

    // Re-layout when controller glyphs change: pad symbols are baked into the text texture.
    void RefreshGlyphs();
    // Limit this text to a fixed horizontal cell, shrinking its character width as needed.
    void FitWidth(int width);

    // Draws the text with its first line's top-left corner at (x, y) on the game's 640-wide
    // screen, at alpha (0x80 solid), as DrawMenuClsMes does. Call it from a menu's draw function
    // once the message textures are loaded, as for CommonMenuMes2.
    void Draw(int x, int y, int alpha = 0x80);

    // With this on, laying the text out leaves the game's random numbers and the message windows'
    // shake tables as they were. MakeMesWin fills the tables with 64 rand() calls, so a text laid
    // out where the game does not (the FPS counter, between two ticks) would otherwise change what
    // the game does next, and by when it landed.
    void KeepGameRandom(bool keep) { keep_random_ = keep; }

    // How wide the widest line draws, in screen pixels; for right-aligning or centring a value.
    int Width() const { return mes_.text_width; }

    // The window itself, for settings the presets above do not cover.
    ClsMes &Mes() { return mes_; }

private:
    bool Layout();

    ClsMes       mes_;
    GameTextFile file_;
    std::string  text_;
    u32          colour_;
    int          missing_ = 0;
    bool         set_ = false;
    bool         keep_random_ = false;
    int          glyph_style_ = -1;
    int          cell_width_ = 0;
    int          base_char_width_ = 11;
};
