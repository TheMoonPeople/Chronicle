#include "title/title_menu.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <string>

#include "gamepad.hpp"
#include "localize/localize.hpp"
#include "snd.hpp"
#include "sound.hpp"
#include "text/captionfont.hpp"
#include "title/tisound.hpp"
#include "ui/framework.hpp"

namespace dc::title::menu {

using namespace dc::game;
using namespace dc::localize;
using namespace dc::text;

namespace {

struct Row {
    const char *key;
    const char *english;
};

// Retail's words for its painted rows (start.img and its French, German, Italian and Spanish
// copies), then Exit.
constexpr std::array kRows = {
    Row{.key = "title.menu.start",    .english = "START"   },
    Row{.key = "title.menu.continue", .english = "CONTINUE"},
    Row{.key = "title.menu.option",   .english = "OPTION"  },
    Row{.key = "title.menu.exit",     .english = "EXIT"    },
};
static_assert(kRows.size() == kRowCount);

// Where the letters stand on the PAL screen. Retail's painted capitals are 16 tall, from 263 across,
// with their tops about 29 apart from 306, and the trademark lines start at about 405: a fourth row
// fits above them with the rows 28 apart from 284, which also keeps them clear of the logo.
constexpr int   kTextLeft = 263;
constexpr int   kFirstTextTop = 284;
constexpr int   kRowStride = 28;
constexpr float kCapHeight = 16.7f;

// The capitals are the painting's own, fitted to it in the font; their spacing is the font's kerning, each
// pair's gap beyond 1.3 (tools/font/README.md). The letters are set by their outlines, so the spacing is the
// same at any window size, with the 1.25 the renderer leaves between them made 1.3 by the gap below.
constexpr float kStretch = 1.0f;
constexpr float kLetterGap = 0.05f;
constexpr float kKernStrength = 1.0f;

// capfont's slot for the title font (ui::FontWeight::Title).
constexpr int kTitleFontSlot = 3;

// Retail's hand stands about 2 above its row's capitals.
constexpr float kHandAboveText = 2.0f;

// The rows' hit area: the 256 pixels retail's painted rows span, and each row's height around its
// letters.
constexpr int kRowLeft = 193;
constexpr int kRowWidth = 256;
constexpr int kRowAboveText = 6;

// The painting's letters (start.img): cream texels at alpha 0xFF, twice the GS's opaque 0x80. Retail
// draws the selected row at 128, which shows the cream itself (226, 253, 210, measured on a real GPU),
// and the others at 32, which the doubled texels make half opaque. The font's rows take the same look.
constexpr u8 kCreamR = 226;
constexpr u8 kCreamG = 253;
constexpr u8 kCreamB = 210;

// The painting's unselected letters read darker than half its cream: its strokes are softer than the
// font's, so less of each letter is at full strength. Three quarters of half, judged on a real GPU.
constexpr int kOtherRowsShare = 3;
constexpr int kOtherRowsOf = 4;

// Retail's letters sit in a soft black halo that fades as it reaches out from them.
constexpr float kHaloReach = 3.75f;
constexpr float kHaloStrength = 0.55f;

enum class Choice : int {
    Yes = 0,
    No = 1
};

bool g_confirm_open = false;
int  g_confirm_choice = static_cast<int>(Choice::No);
bool g_confirm_chosen = false;

ui::Cursor &ConfirmCursor() {
    static ui::Cursor cursor;
    return cursor;
}

std::string ConfirmTitle() {
    return LocalizeText("title.exit.confirm_title", "\"Exit\"");
}

std::string ConfirmDescription() {
    return LocalizeText("title.exit.confirm_desc", "Return to desktop?");
}

void PlayCursorSound() {
    TiPlayVolSE(MIDI_PORT_UNK_D, 122, 24, 1.0f);
}

void PlayConfirmSound() {
    TiPlayVolSE(MIDI_PORT_SE_DEFAULT, 38, 20, 1.0f);
}

void CloseConfirm() {
    g_confirm_open = false;
    PlayCursorSound();
}

} // namespace

float HandY(int row) {
    return static_cast<float>(kFirstTextTop + row * kRowStride) - kHandAboveText;
}

int RowAt(float x, float y) {
    const auto top = static_cast<float>(kFirstTextTop - kRowAboveText);
    if (x < kRowLeft || x >= kRowLeft + kRowWidth || y < top || y >= top + kRowCount * kRowStride) {
        return -1;
    }
    return static_cast<int>((y - top) / kRowStride);
}

namespace {

// The caption renderer sets a line's first letter with its ink at the line's left, but the painting was
// typeset: each word starts at the same pen position, so a flat letter (E) stands a little right of a
// round one (C, O). This is how far the first letter of text sits right of where START's S does, in
// logical pixels, from the two letters' left side bearings in the title font.
float BearingFromS(const std::string &text) {
    float      h_bounds[4];
    float      first_bounds[4];
    float      s_bounds[4];
    const auto ch = static_cast<unsigned char>(text.empty() ? 'S' : text[0]);
    if (ch >= 0x80 || !capfont::GetGlyphBounds(U'H', h_bounds, kTitleFontSlot) ||
        !capfont::GetGlyphBounds(ch, first_bounds, kTitleFontSlot) ||
        !capfont::GetGlyphBounds(U'S', s_bounds, kTitleFontSlot)) {
        return 0.0f;
    }
    return (first_bounds[0] - s_bounds[0]) * kCapHeight / (h_bounds[3] - h_bounds[1]) * kStretch;
}

} // namespace

void Draw(int selected, int selected_alpha, int other_alpha) {
    ui::TextStyle style;
    style.align = ui::Alignment::Left;
    style.weight = ui::FontWeight::Title;
    style.scale = kCapHeight / 14.5f;
    style.xscale = kStretch;
    style.letter = kLetterGap;
    style.kern = kKernStrength;
    style.ink_spacing = true;
    style.use_custom_rgb = true;
    style.see_through = true;
    // Label puts the capitals' top 3 below its y, less half what the scale adds to their height.
    const int cap_offset = static_cast<int>(std::lround(3.0f - ((kCapHeight - 14.5f) * 0.5f)));

    for (int row = 0; row < kRowCount; ++row) {
        const bool chosen = row == selected;
        style.fill_r = kCreamR;
        style.fill_g = kCreamG;
        style.fill_b = kCreamB;
        style.drop_shadow = false;
        style.halo = kHaloReach;
        style.halo_strength = kHaloStrength;
        const std::string text = LocalizeText(kRows[row].key, kRows[row].english);
        ui::Label         label(text, style);
        label.Draw(kTextLeft + static_cast<int>(std::lround(BearingFromS(text))), kFirstTextTop + row * kRowStride - cap_offset,
                   // Retail's alphas on the painting's doubled texels (above).
                   chosen ? selected_alpha : std::min(other_alpha * 2 * kOtherRowsShare / kOtherRowsOf, 0x80));
    }
}

bool ConfirmOpen() {
    return g_confirm_open;
}

void OpenConfirm() {
    g_confirm_open = true;
    g_confirm_chosen = false;
    g_confirm_choice = static_cast<int>(Choice::No);
    const ui::Rect no = ui::ConfirmModal::ButtonRect(ConfirmTitle(), ConfirmDescription(), static_cast<int>(Choice::No));
    ConfirmCursor().SnapTo(static_cast<float>(no.x - 34), static_cast<float>(no.y + 3));
    PlayConfirmSound();
}

void StepConfirm(int pointer_hover, bool pointer_click, bool pointer_back) {
    if (g_confirm_chosen) {
        return;
    }
    if (pointer_hover >= 0 && pointer_hover != g_confirm_choice) {
        g_confirm_choice = pointer_hover;
        PlayCursorSound();
    }
    if (GamePad.Down(PAD_UP | PAD_DOWN)) {
        g_confirm_choice = 1 - g_confirm_choice;
        PlayCursorSound();
    } else if (GamePad.Down(PAD_CROSS) || GamePad.Down(PAD_START) || pointer_click) {
        if (g_confirm_choice == static_cast<int>(Choice::Yes)) {
            g_confirm_chosen = true;
            PlayConfirmSound();
            SDL_Event event{};
            event.type = SDL_EVENT_QUIT;
            SDL_PushEvent(&event);
        } else {
            CloseConfirm();
        }
    } else if (GamePad.Down(PAD_CIRCLE) || pointer_back) {
        CloseConfirm();
    }
}

int ConfirmHit(float x, float y) {
    return ui::ConfirmModal::Hit(ConfirmTitle(), ConfirmDescription(), x, y);
}

void DrawConfirm(int alpha) {
    if (!g_confirm_open) {
        return;
    }
    ui::ConfirmModal::Draw(ConfirmTitle(), ConfirmDescription(), LocalizeText("battlemenu.quit.yes", "YES"),
                           LocalizeText("battlemenu.quit.no", "NO"), g_confirm_choice, ConfirmCursor(), alpha,
                           ui::WindowPreset::DarkTranslucent, g_confirm_chosen ? static_cast<int>(Choice::Yes) : -1, g_confirm_chosen);
}

std::vector<std::pair<std::string, std::string>> Strings() {
    std::vector<std::pair<std::string, std::string>> out;
    out.reserve(kRows.size() + 2);
    for (const Row &row : kRows) {
        out.emplace_back(row.key, row.english);
    }
    out.emplace_back("title.exit.confirm_title", "\"Exit\"");
    out.emplace_back("title.exit.confirm_desc", "Return to desktop?");
    return out;
}

} // namespace dc::title::menu
