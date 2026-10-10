#include "gametext.hpp"

#include "platform/config.hpp"
#include "platform/input.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <string_view>

#include "gameutil.hpp"
#include "main.hpp"
#include "menu_draw.hpp"

namespace {

// gaiji.img's letter grid in code order: nine 14x20 cells a row from u 128 (GaijiDataTbl).
constexpr std::u32string_view kGrid = U"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz'=\"!?#&+-*/%()@|<>{}[]:,.$0123456789";
constexpr s16                 kGridFirst = -0x2DF;

struct Extra {
    s16      code;
    char32_t ch;
};

// The rest of PAL's characters: a few cells of their own below the grid, and grid letters that
// DrawGaijiFont draws an accent over. -0x263 to -0x261, -0x253 and -0x252 draw a bare '?'.
// clang-format off
constexpr Extra kExtras[] = {
    {-0x287, U'œ'}, {-0x286, U'¡'}, {-0x285, U'¿'}, {-0x284, U'Ä'}, {-0x283, U'Ç'}, {-0x282, U'È'},
    {-0x281, U'É'}, {-0x280, U'Ö'}, {-0x27F, U'Ü'}, {-0x27E, U'ß'}, {-0x27D, U'à'}, {-0x27C, U'á'},
    {-0x27B, U'â'}, {-0x27A, U'ä'}, {-0x279, U'ç'}, {-0x278, U'è'}, {-0x277, U'é'}, {-0x276, U'ê'},
    {-0x275, U'ë'}, {-0x274, U'ì'}, {-0x273, U'í'}, {-0x272, U'î'}, {-0x271, U'ï'}, {-0x270, U'ñ'},
    {-0x26F, U'ò'}, {-0x26E, U'ó'}, {-0x26D, U'ô'}, {-0x26C, U'ö'}, {-0x26B, U'ù'}, {-0x26A, U'ú'},
    {-0x269, U'û'}, {-0x268, U'ü'}, {-0x267, U'Ú'}, {-0x266, U'Á'}, {-0x265, U'Œ'}, {-0x264, U'Ó'},
    {-0x260, U'À'}, {-0x25F, U'Â'}, {-0x25E, U'Ï'}, {-0x25D, U'Í'}, {-0x25C, U'Ì'}, {-0x25B, U'Î'},
    {-0x25A, U'Ù'}, {-0x259, U'Û'}, {-0x258, U'Ë'}, {-0x257, U'Ê'}, {-0x256, U'Ò'}, {-0x255, U'Ô'},
    {-0x254, U'Ñ'},
};
// clang-format on

// Layouts under way that must not touch the game's random numbers.
int g_keep_random = 0;

constexpr s16 kUnknown = kGridFirst + static_cast<s16>(kGrid.find(U'?'));

// The code point at utf8[at], advancing at past it; U+FFFD for a byte that does not start a
// well-formed sequence, advancing one byte.
char32_t NextChar(std::string_view utf8, size_t &at) {
    const auto lead = static_cast<unsigned char>(utf8[at]);
    int        length;
    char32_t   ch;
    char32_t   least;

    if (lead < 0x80) {
        at++;
        return lead;
    }
    if ((lead & 0xE0) == 0xC0) {
        length = 2;
        ch = lead & 0x1F;
        least = 0x80;
    } else if ((lead & 0xF0) == 0xE0) {
        length = 3;
        ch = lead & 0x0F;
        least = 0x800;
    } else if ((lead & 0xF8) == 0xF0) {
        length = 4;
        ch = lead & 0x07;
        least = 0x10000;
    } else {
        at++;
        return 0xFFFD;
    }
    if (at + length > utf8.size()) {
        at++;
        return 0xFFFD;
    }
    for (int i = 1; i < length; i++) {
        const auto next = static_cast<unsigned char>(utf8[at + i]);
        if ((next & 0xC0) != 0x80) {
            at++;
            return 0xFFFD;
        }
        ch = ch << 6 | (next & 0x3F);
    }
    if (ch < least || ch > 0x10FFFF || (ch >= 0xD800 && ch <= 0xDFFF)) {
        at++;
        return 0xFFFD;
    }
    at += length;
    return ch;
}

void AppendUtf8(std::string &out, char32_t ch) {
    if (ch < 0x80) {
        out += static_cast<char>(ch);
    } else if (ch < 0x800) {
        out += static_cast<char>(0xC0 | ch >> 6);
        out += static_cast<char>(0x80 | (ch & 0x3F));
    } else if (ch < 0x10000) {
        out += static_cast<char>(0xE0 | ch >> 12);
        out += static_cast<char>(0x80 | (ch >> 6 & 0x3F));
        out += static_cast<char>(0x80 | (ch & 0x3F));
    } else {
        out += static_cast<char>(0xF0 | ch >> 18);
        out += static_cast<char>(0x80 | (ch >> 12 & 0x3F));
        out += static_cast<char>(0x80 | (ch >> 6 & 0x3F));
        out += static_cast<char>(0x80 | (ch & 0x3F));
    }
}

// The control codes that are not characters, by the names the text is written with. Codes in a family
// (a colour, a value, a name) are written with their number: "{value3}", "{wait 12}".
struct Named {
    s16         code;
    const char *name;
};

constexpr Named kNamed[] = {
    {-0xFD,  "page"        },
    {-0x400, "/color"      },
    {-0x3FF, "white"       },
    {-0x3FE, "yellow"      },
    {-0x3FD, "cyan"        },
    {-0x3FC, "green"       },
    {-0x3FB, "dark"        },
    {-0x3FA, "gold"        },
    {-0x3F9, "grey"        },
    {-0x301, "highlight"   },
    {-0x401, "value"       },
    {-0x300, "select"      },
    {-0x2FF, "start"       },
    {-0x2FE, "L1"          },
    {-0x2FD, "R1"          },
    {-0x2FC, "L2"          },
    {-0x2FB, "R2"          },
    {-0x2FA, "circle"      },
    {-0x2F9, "triangle"    },
    {-0x2F8, "cross"       },
    {-0x2F7, "square"      },
    {-0x2F6, "dpad"        },
    {-0x2F5, "dpad-updown" },
    {-0x2F4, "dpad-sides"  },
    {-0x2F3, "red-slash"   },
    {-0x2F2, "heart"       },
    {-0x2F1, "note"        },
    {-0x2F0, "red-x"       },
    {-0x2EF, "yellow-arrow"},
    {-0x2EE, "icon-sword"  },
    {-0x2ED, "icon-orb"    },
    {-0x2EC, "icon-georama"},
    {-0x2EB, "icon-jar"    },
    {-0x2EA, "orange-arrow"},
    {-0x2E9, "bait"        },
    {-0x2E8, "word-button" },
    {-0x2E7, "monster"     },
    {-0x2E6, "alert"       },
    {-0x2E5, "up"          },
    {-0x2E4, "right"       },
    {-0x2E3, "down"        },
    {-0x2E2, "left"        },
    {-0x2E1, "hand"        },
};

// The families: a name, the lowest code, how many, and where the number starts. A code is written as
// the name and the number (code - first + origin), joined by a space where the name ends in a letter
// that a digit would be read as part of ("wait 12") or directly ("value3").
struct Family {
    const char *name;
    int         first;
    int         last;
    int         origin;
    bool        spaced;
};

constexpr Family kFamilies[] = {
    {"wait",    -0x200, -0x101, 0, true },
    {"color",   -0x3FF, -0x302, 2, true },
    {"gap",     -0x700, -0x601, 0, true },
    {"spacing", -0x800, -0x701, 0, true },
    {"justify", -0x900, -0x801, 0, true },
    {"bubble",  -0xA00, -0x901, 0, true },
    {"icon",    -0x300, -0x2E0, 0, true },
    {"name",    -0x506, -0x501, 1, false},
    {"value",   -0x40D, -0x406, 1, false},
};

// "insert" runs over two stretches of codes.
constexpr int kInsertFirst[] = {-0x402, -0x40E};
constexpr int kInsertCount[] = {4, 6};

std::string Lower(std::string_view text) {
    std::string out(text);
    for (char &ch : out) {
        if (ch >= 'A' && ch <= 'Z') {
            ch = static_cast<char>(ch - 'A' + 'a');
        }
    }
    return out;
}

// "12" as a number up to 4 digits, or -1.
int ParseNumber(std::string_view text) {
    if (text.empty() || text.size() > 4) {
        return -1;
    }
    int value = 0;
    for (char ch : text) {
        if (ch < '0' || ch > '9') {
            return -1;
        }
        value = value * 10 + (ch - '0');
    }
    return value;
}

// The name of a control code, or empty where it has none (a character, or a code the game does not use).
std::string ControlName(s16 code) {
    for (const Named &named : kNamed) {
        if (named.code == code) {
            return named.name;
        }
    }
    for (const Family &family : kFamilies) {
        if (code >= family.first && code <= family.last) {
            // values count upwards in number while their codes run downwards.
            const bool downwards = std::string_view(family.name) == "value";
            const int  number = downwards ? family.last - code + family.origin : code - family.first + family.origin;
            return std::string(family.name) + (family.spaced ? " " : "") + std::to_string(number);
        }
    }
    for (int stretch = 0; stretch < 2; stretch++) {
        const int first = kInsertFirst[stretch];
        if (code <= first && code > first - kInsertCount[stretch]) {
            return "insert" + std::to_string((stretch == 0 ? 0 : kInsertCount[0]) + (first - code) + 1);
        }
    }
    return {};
}

// The code a name stands for; false where it is none.
bool ControlFromName(std::string_view token, s16 &code) {
    const std::string name = Lower(token);
    for (const Named &named : kNamed) {
        if (name == Lower(named.name)) {
            code = named.code;
            return true;
        }
    }
    for (const Family &family : kFamilies) {
        const std::string_view prefix = family.name;
        if (name.size() <= prefix.size() || name.compare(0, prefix.size(), prefix) != 0) {
            continue;
        }
        std::string_view rest = std::string_view(name).substr(prefix.size());
        if (family.spaced != (rest.front() == ' ')) {
            continue;
        }
        const int number = ParseNumber(family.spaced ? rest.substr(1) : rest);
        const bool downwards = prefix == "value";
        const int  value = downwards ? family.last - (number - family.origin) : family.first + (number - family.origin);
        if (number >= 0 && value >= family.first && value <= family.last) {
            code = static_cast<s16>(value);
            return true;
        }
    }
    if (name.size() > 6 && name.compare(0, 6, "insert") == 0) {
        const int number = ParseNumber(std::string_view(name).substr(6));
        if (number >= 1 && number <= kInsertCount[0] + kInsertCount[1]) {
            const int index = number - 1;
            code = static_cast<s16>(index < kInsertCount[0] ? kInsertFirst[0] - index
                                                            : kInsertFirst[1] - (index - kInsertCount[0]));
            return true;
        }
    }
    return false;
}

// "{N}" or "{name}" at utf8[at]: the code into code and at past the '}'.
bool ReadEscape(std::string_view utf8, size_t &at, s16 &code) {
    if (const size_t close = utf8.find('}', at); close != std::string_view::npos && close - at <= 24 &&
                                                  ControlFromName(utf8.substr(at + 1, close - at - 1), code)) {
        at = close + 1;
        return true;
    }
    size_t i = at + 1;
    bool   negative = false;
    long   value = 0;
    size_t digits = 0;

    if (i < utf8.size() && utf8[i] == '-') {
        negative = true;
        i++;
    }
    while (i < utf8.size() && utf8[i] >= '0' && utf8[i] <= '9' && digits < 6) {
        value = value * 10 + (utf8[i] - '0');
        digits++;
        i++;
    }
    if (digits == 0 || i >= utf8.size() || utf8[i] != '}') {
        return false;
    }
    value = negative ? -value : value;
    if (value < INT16_MIN || value > INT16_MAX) {
        return false;
    }
    code = static_cast<s16>(value);
    at = i + 1;
    return true;
}

} // namespace

s16 GameTextCode(char32_t ch) {
    switch (ch) {
        case U' ':
        case U' ':
            return MES_CODE_SPACE;
        case U'\n':
            return MES_CODE_NEWLINE;
        case U'‘':
        case U'’':
            ch = U'\'';
            break;
        case U'“':
        case U'”':
            ch = U'"';
            break;
        case U'–':
        case U'—':
            ch = U'-';
            break;
    }

    const size_t at = kGrid.find(ch);
    if (at != std::u32string_view::npos) {
        return static_cast<s16>(kGridFirst + at);
    }
    for (const Extra &extra : kExtras) {
        if (extra.ch == ch) {
            return extra.code;
        }
    }
    return 0;
}

char32_t GameTextChar(s16 code) {
    switch (code) {
        case MES_CODE_SPACE:
            return U' ';
        case MES_CODE_NEWLINE:
            return U'\n';
    }

    if (code >= kGridFirst && code < kGridFirst + static_cast<int>(kGrid.size())) {
        return kGrid[code - kGridFirst];
    }
    for (const Extra &extra : kExtras) {
        if (extra.code == code) {
            return extra.ch;
        }
    }
    return 0;
}

int GameTextEncode(std::string_view utf8, std::vector<s16> &out) {
    int    missing = 0;
    size_t at = 0;

    while (at < utf8.size()) {
        if (utf8[at] == '{') {
            if (at + 1 < utf8.size() && utf8[at + 1] == '{') {
                out.push_back(GameTextCode(U'{'));
                at += 2;
                continue;
            }

            s16 code;
            if (ReadEscape(utf8, at, code)) {
                out.push_back(code);
                continue;
            }
        }

        s16 code = GameTextCode(NextChar(utf8, at));
        if (code == 0) {
            code = kUnknown;
            missing++;
        }
        out.push_back(code);
    }

    out.push_back(MES_CODE_END);
    return missing;
}

std::string GameTextDecode(const s16 *codes) {
    std::string text;

    for (; *codes != MES_CODE_END; codes++) {
        const char32_t ch = GameTextChar(*codes);

        if (ch == 0) {
            const std::string name = ControlName(*codes);
            text += '{' + (name.empty() ? std::to_string(*codes) : name) + '}';
        } else if (ch == U'{') {
            text += "{{";
        } else {
            AppendUtf8(text, ch);
        }
    }

    return text;
}

int GameTextFile::Set(int id, std::string_view utf8) {
    if (id < INT16_MIN || id > INT16_MAX) {
        return -1;
    }

    std::vector<s16> codes;
    const int        missing = GameTextEncode(utf8, codes);

    const auto   found = messages_.find(id);
    const bool   added = found == messages_.end();
    const size_t count = messages_.size() + (added ? 1 : 0);
    const size_t total = codes_ - (added ? 0 : found->second.size()) + codes.size();
    size_t       last = codes.size();

    if (!messages_.empty() && messages_.rbegin()->first > id) {
        last = messages_.rbegin()->second.size();
    }
    if (1 + count + total - last > INT16_MAX) {
        return -1;
    }

    codes_ = total;
    messages_[id] = std::move(codes);
    dirty_ = true;
    return missing;
}

short *GameTextFile::Data() {
    if (dirty_) {
        const size_t count = messages_.size();

        data_.assign(2 + count * 2, 0);
        data_.reserve(data_.size() + codes_);
        data_[0] = static_cast<s16>(count);

        size_t entry = 0;
        for (const auto &[id, codes] : messages_) {
            data_[2 + entry * 2] = static_cast<s16>(id);
            data_[3 + entry * 2] = static_cast<s16>(data_.size() - (1 + count));
            data_.insert(data_.end(), codes.begin(), codes.end());
            entry++;
        }

        dirty_ = false;
    }

    return data_.data();
}

GameText::GameText() : colour_(FONT_COLOR_WHITE) {
    mes_.Preset(MES_PRESET_SYSTEM);
    mes_.char_width = 11;
    mes_.char_height = 0x14;
    mes_.narrow_gaiji_set = 2;
    mes_.tex_block = 0x1A;
    mes_.auto_pos = MES_POS_NONE;
    mes_.tail_on = false;
    mes_.value_show = false;
    mes_.centre_rows = false;
}

// Retail's, but for the tables of a layout that keeps the game's random numbers: those stay as
// they were, and no rand() is called.
extern float RandTbl[64];
extern float RandTbl2[64];

PC_OVERRIDE void MakeRandTbl(float lo, float hi) {
    if (g_keep_random > 0) {
        return;
    }
    for (int i = 0; i < 64; i++) {
        RandTbl[i] = ((hi - lo) * (float) rand()) / 2147483648.0f;
        RandTbl[i] += lo;
    }
}

PC_OVERRIDE void MakeRandTbl2(float lo, float hi) {
    if (g_keep_random > 0) {
        return;
    }
    for (int i = 0; i < 64; i++) {
        RandTbl2[i] = ((hi - lo) * (float) rand()) / 2147483648.0f;
        RandTbl2[i] += lo;
    }
}

int GameText::Set(std::string_view utf8) {
    if (!set_ || utf8 != text_ || mes_.mes_made < 0) {
        set_ = true;
        text_ = utf8;
        missing_ = file_.Set(0, utf8);
        if (missing_ >= 0 && !Layout()) {
            missing_ = -1;
        }
    }

    if (cell_width_ > 0) {
        FitWidth(cell_width_);
    }

    return missing_;
}

void GameText::SetColour(u32 colour) {
    if (colour != colour_) {
        colour_ = colour;
        if (mes_.mes_made >= 0 && !Layout()) {
            missing_ = -1;
        }
    }
}

void GameText::FitWidth(int width) {
    cell_width_ = width;
    if (mes_.mes_made >= 0 && width > 0 && mes_.text_width > width) {
        Layout();
    } else if (mes_.mes_made >= 0 && width > 0 && mes_.char_width != base_char_width_) {
        Layout();
    }
}

void GameText::RefreshGlyphs() {
    const Config &config = ConfigGet();
    int style = static_cast<int>(config.glyphs_new) * 32 + static_cast<int>(config.glyph_device) * 2 +
                static_cast<int>(InputActiveGlyphFamily());
    // Original glyph mode still uses the PS2 font atlas, so the selected device does not affect it.
    if (!config.glyphs_new) {
        style = 0;
    }
    if (style != glyph_style_) {
        glyph_style_ = style;
        if (mes_.mes_made >= 0) {
            Layout();
        }
    }
}

bool GameText::Layout() {
    struct Keep {
        explicit Keep(bool on) : on_(on) { g_keep_random += on ? 1 : 0; }

        ~Keep() { g_keep_random -= on_ ? 1 : 0; }

        bool on_;
    } keep(keep_random_);

    mes_.char_width = base_char_width_;
    mes_.SetBuff(file_.Data());
    if (SystemMes != nullptr) {
        mes_.SetBuff_system(SystemMes);
    }
    mes_.clut_default = Color2Clut(colour_) & 0xFF;
    mes_.clut_now = mes_.clut_default;
    mes_.mes_made = -1;
    mes_.MakeMesWin(0);

    if (cell_width_ > 0 && mes_.text_width > cell_width_) {
        int fitted = std::max(1, static_cast<int>(base_char_width_ * cell_width_ /
                                                 static_cast<float>(mes_.text_width)));
        if (fitted < mes_.char_width) {
            mes_.char_width = fitted;
            mes_.mes_made = -1;
            mes_.MakeMesWin(0);
        }
    }

    if (mes_.win_line_num > 0 && mes_.win_line[mes_.win_line_num - 1].code == MES_CODE_END) {
        return true;
    }

    mes_.mes_made = -1;
    mes_.text_len = 0;
    mes_.text_width = 0;
    return false;
}

void GameText::Draw(int x, int y, int alpha) {
    if (mes_.mes_made < 0) {
        return;
    }

    mes_.edge_alpha = alpha;
    DrawMenuClsMes(&mes_, x, y);
}
