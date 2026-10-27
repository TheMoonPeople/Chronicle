#include "ui/name_mouse_layout.hpp"

#include <array>

namespace dc::ui::name_mouse {

namespace {

// DrawNameTemplete is drawn at (0x42, 0x9E).
constexpr int kFrameX = 0x42;
constexpr int kFrameY = 0x9E;

constexpr int kLanguageCount = 7;

using TabTable = std::array<std::array<short, kTabCount>, kLanguageCount>;

// Where each language's tabs lie across the frame (Get_NameTemp_PutX) and how wide they are
// (DrawNameTemplete's tab_width). A width of 0 is a tab the language does not have.
constexpr TabTable kTabX = {
    {
     {40, 125, 209, 261, 311, 378, 40, 74, 111, 168, 223},
     {0, 0, 40, 223, 303, 384, 40, 74, 111, 168, 223},
     {0, 0, 40, 144, 303, 384, 40, 74, 111, 168, 223},
     {0, 0, 40, 140, 303, 384, 40, 74, 111, 168, 223},
     {0, 0, 40, 150, 303, 384, 40, 74, 111, 168, 223},
     {0, 0, 40, 142, 303, 384, 40, 74, 111, 168, 223},
     {0, 0, 40, 132, 283, 384, 40, 74, 111, 168, 223},
     }
};
constexpr TabTable kTabWidth = {
    {
     {78, 78, 44, 44, 60, 52, 24, 24, 48, 48, 112},
     {0, 0, 176, 75, 74, 70, 24, 24, 48, 48, 112},
     {0, 0, 96, 155, 74, 70, 24, 24, 48, 48, 112},
     {0, 0, 92, 159, 76, 70, 24, 24, 48, 48, 112},
     {0, 0, 103, 148, 74, 70, 24, 24, 48, 48, 112},
     {0, 0, 94, 157, 75, 70, 24, 24, 48, 48, 112},
     {0, 0, 86, 145, 95, 70, 24, 24, 48, 48, 112},
     }
};

// A key is drawn 22 by 22; the area that counts as on it reaches a little past, so the sliver between
// keys is not dead, but stays clear of the next key.
constexpr int kKeySize = 22;
constexpr int kKeyPadX = 4;
constexpr int kKeyPadY = 2;

constexpr int kAlphabetKeys = 52;
constexpr int kEuroKeys = 26;
constexpr int kSymbolKeys = 40;
constexpr int kBlankSymbol = 16;
// The first character code the accented keyboard draws a key for.
constexpr int kFirstEuroCode = 0x104;

constexpr int kNameX = 182;
constexpr int kNameY = 108;

bool ValidLanguage(int language) { return language >= 0 && language < kLanguageCount; }

Rect PaddedKey(int x, int y) {
    return Rect{.left = x - kKeyPadX, .top = y - kKeyPadY, .right = x + kKeySize + kKeyPadX, .bottom = y + kKeySize + kKeyPadY};
}

} // namespace

Rect TabRect(int language, int tab) {
    if (!ValidLanguage(language) || tab < 0 || tab >= kTabCount || kTabWidth[language][tab] == 0) {
        return Rect{};
    }
    int left = kFrameX + kTabX[language][tab];
    int top = kFrameY + 6 + (tab / 6) * 27;
    int height = 0x18;
    if (tab == kTabOk) {
        top += 4;
        height = 0x28;
    }
    return Rect{.left = left, .top = top, .right = left + kTabWidth[language][tab], .bottom = top + height};
}

Rect KeyRect(const Screen &screen, int key) {
    if (key < 0) {
        return Rect{};
    }
    int base_x = kFrameX + 0x38;
    int base_y = kFrameY + 0x54;
    switch (screen.input_mode) {
        case kInputAlphabet: {
            int x = base_x - 0xE + (key % 13) * 34;
            if (key < kAlphabetKeys) {
                // The European screens move up to make room for the accented rows.
                int y = base_y - (screen.language > 2 ? screen.euro_rows * 3 : 0);
                return PaddedKey(x, y + (key / 13) * 26);
            }
            int accented = key - kAlphabetKeys;
            if (screen.language <= 2 || screen.euro_codes == nullptr || accented >= kEuroKeys ||
                accented >= screen.euro_rows * 13 || screen.euro_codes[accented] < kFirstEuroCode) {
                return Rect{};
            }
            return PaddedKey(x, base_y - screen.euro_rows * 3 + (key / 13) * 26);
        }
        case kInputSymbol:
            if (key >= kSymbolKeys || (screen.symbol_cells != nullptr && screen.symbol_cells[key] == kBlankSymbol)) {
                return Rect{};
            }
            return PaddedKey(base_x + (key % 10) * 38, base_y + (key / 10) * 26);
        default:
            // The kana keyboards are Japanese, which the PAL release does not offer.
            return Rect{};
    }
}

Rect SlotRect(int slot) {
    if (slot < 0 || slot >= kNameSlots) {
        return Rect{};
    }
    int left = kNameX + slot * 22;
    return Rect{.left = left, .top = kNameY, .right = left + 22, .bottom = kNameY + 32};
}

Rect DialogRect(int area) {
    switch (area) {
        case kAreaHelp:
            return Rect{.left = 0xAE, .top = 0xAA, .right = 0xAE + 0x12C, .bottom = 0xAA + 0x60};
        case kAreaConfirm:
            return Rect{.left = 0xD8, .top = 0xB8, .right = 0xD8 + 0xDA, .bottom = 0xB8 + 0x60};
        case kAreaEmptyNotice:
            return Rect{.left = 0xBA, .top = 0xB8, .right = 0xBA + 0x116, .bottom = 0xB8 + 0x60};
        default:
            return Rect{};
    }
}

int ConfirmChoice(float x, float y) {
    // The message starts 16 across and 14 down the window; its lines are 20 high: "Accept?", Yes, No.
    constexpr int left = 0xD8 + 12;
    constexpr int right = 0xD8 + 0x68;
    constexpr int yes_top = 0xB8 + 14 + 20;
    if (x < left || x >= right) {
        return 0;
    }
    if (y >= yes_top && y < yes_top + 20) {
        return 1;
    }
    if (y >= yes_top + 20 && y < yes_top + 40) {
        return -1;
    }
    return 0;
}

Hit HitTest(const Screen &screen, float x, float y) {
    for (int tab = 0; tab < kTabCount; ++tab) {
        if (TabRect(screen.language, tab).Contains(x, y)) {
            return Hit{.kind = Kind::Tab, .index = tab};
        }
    }
    int keys = screen.input_mode == kInputSymbol ? kSymbolKeys : kAlphabetKeys + kEuroKeys;
    for (int key = 0; key < keys; ++key) {
        if (KeyRect(screen, key).Contains(x, y)) {
            return Hit{.kind = Kind::Key, .index = key};
        }
    }
    for (int slot = 0; slot < kNameSlots; ++slot) {
        if (SlotRect(slot).Contains(x, y)) {
            return Hit{.kind = Kind::Slot, .index = slot};
        }
    }
    return Hit{};
}

float PointerUnitsPerCount(float target_pixels_per_unit, float target_width, float window_width) {
    if (!(target_pixels_per_unit > 0.0f) || !(target_width > 0.0f) || !(window_width > 0.0f)) {
        return 0.0f;
    }
    return target_width / window_width / target_pixels_per_unit;
}

} // namespace dc::ui::name_mouse
