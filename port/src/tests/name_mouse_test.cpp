#include <gtest/gtest.h>

#include "battle_globals.hpp"
#include "ui/name_mouse_layout.hpp"

namespace dc::test {

using namespace dc::game;

namespace {

using namespace ui::name_mouse;

constexpr int kEnglish = 1;
constexpr int kGerman = 4;

Screen EnglishScreen(int input_mode) {
    Screen screen;
    screen.language = kEnglish;
    screen.input_mode = input_mode;
    screen.symbol_cells = menu_kigoutbl;
    return screen;
}

Screen EuropeanScreen(int language) {
    Screen screen = EnglishScreen(kInputAlphabet);
    screen.language = language;
    screen.euro_codes = &menu_euro_codetbl[language - 2][0][0];
    screen.euro_rows = euro_code_linelimmit[language - 2];
    return screen;
}

Hit Centre(const Screen &screen, int key) {
    Rect rect = KeyRect(screen, key);
    return HitTest(screen, (rect.left + rect.right) * 0.5f, (rect.top + rect.bottom) * 0.5f);
}

} // namespace

// The alphabet keyboard is 4 rows of 13 keys 34 across and 26 down; the 22-pixel key sits inside its rect.
TEST(NameMouse, AlphabetKeysAreWhereTheyAreDrawn) {
    Screen screen = EnglishScreen(kInputAlphabet);
    for (int key = 0; key < 52; ++key) {
        Rect rect = KeyRect(screen, key);
        EXPECT_EQ(rect.left + 4, 108 + (key % 13) * 34) << key;
        EXPECT_EQ(rect.top + 2, 242 + (key / 13) * 26) << key;
        EXPECT_EQ(Centre(screen, key), (Hit{Kind::Key, key})) << key;
    }
    // Nothing beyond the 52 letters in English.
    EXPECT_FALSE(KeyRect(screen, 52).Contains(0, 0));
    EXPECT_EQ(KeyRect(screen, 52).right, 0);
}

// The symbol keyboard is 4 rows of 10 keys 38 across; its blank keys are not keys.
TEST(NameMouse, SymbolKeysSkipBlanks) {
    Screen screen = EnglishScreen(kInputSymbol);
    for (int key = 0; key < 40; ++key) {
        if (menu_kigoutbl[key] == 16) {
            EXPECT_EQ(KeyRect(screen, key).right, 0) << key;
            continue;
        }
        Rect rect = KeyRect(screen, key);
        EXPECT_EQ(rect.left + 4, 122 + (key % 10) * 38) << key;
        EXPECT_EQ(rect.top + 2, 242 + (key / 10) * 26) << key;
        EXPECT_EQ(Centre(screen, key), (Hit{Kind::Key, key})) << key;
    }
}

// The accented rows sit under the letters, shift the whole keyboard up 3 pixels a row, and offer
// only the keys the game draws.
TEST(NameMouse, AccentedKeysFollowTheLanguage) {
    Screen screen = EuropeanScreen(kGerman);
    ASSERT_EQ(screen.euro_rows, euro_code_linelimmit[2]);
    int shift = screen.euro_rows * 3;
    EXPECT_EQ(KeyRect(screen, 0).top + 2, 242 - shift);
    int offered = 0;
    for (int accented = 0; accented < 26; ++accented) {
        Rect rect = KeyRect(screen, 52 + accented);
        bool drawn = accented < screen.euro_rows * 13 && menu_euro_codetbl[2][0][accented] >= 0x104;
        EXPECT_EQ(rect.right != 0, drawn) << accented;
        if (!drawn) {
            continue;
        }
        ++offered;
        EXPECT_EQ(rect.left + 4, 108 + (accented % 13) * 34) << accented;
        EXPECT_EQ(rect.top + 2, 242 - shift + (4 + accented / 13) * 26) << accented;
        EXPECT_EQ(Centre(screen, 52 + accented), (Hit{Kind::Key, 52 + accented})) << accented;
    }
    EXPECT_GT(offered, 0);
}

// British and American English have no accented rows.
TEST(NameMouse, EnglishHasNoAccentedKeys) {
    Screen screen = EnglishScreen(kInputAlphabet);
    screen.language = 2;
    for (int key = 52; key < 78; ++key) {
        EXPECT_EQ(KeyRect(screen, key).right, 0) << key;
    }
}

// American English has ALPHABET, SYMBOL/NUMBERS, DEFAULT and DECIDE on the first row and the arrows,
// DEL., INS. and PROFILE on the second.
TEST(NameMouse, EnglishTabs) {
    Screen screen = EnglishScreen(kInputAlphabet);
    EXPECT_EQ(TabRect(kEnglish, 0).right, 0);
    EXPECT_EQ(TabRect(kEnglish, 1).right, 0);
    EXPECT_EQ(HitTest(screen, 66 + 40 + 10, 170), (Hit{Kind::Tab, 2}));
    EXPECT_EQ(HitTest(screen, 66 + 223 + 10, 170), (Hit{Kind::Tab, 3}));
    EXPECT_EQ(HitTest(screen, 66 + 303 + 10, 170), (Hit{Kind::Tab, kTabDefaultName}));
    EXPECT_EQ(HitTest(screen, 66 + 384 + 10, 170), (Hit{Kind::Tab, kTabOk}));
    EXPECT_EQ(HitTest(screen, 66 + 40 + 5, 195), (Hit{Kind::Tab, kTabLeft}));
    EXPECT_EQ(HitTest(screen, 66 + 74 + 5, 195), (Hit{Kind::Tab, kTabRight}));
    EXPECT_EQ(HitTest(screen, 66 + 111 + 5, 195), (Hit{Kind::Tab, kTabDelete}));
    EXPECT_EQ(HitTest(screen, 66 + 168 + 5, 195), (Hit{Kind::Tab, kTabInsert}));
    EXPECT_EQ(HitTest(screen, 66 + 223 + 5, 195), (Hit{Kind::Tab, kTabHelp}));
    // DECIDE is the tall tab.
    EXPECT_EQ(TabRect(kEnglish, kTabOk).bottom - TabRect(kEnglish, kTabOk).top, 40);
    // A gap between tabs is nothing.
    EXPECT_EQ(HitTest(screen, 66 + 40 + 177, 170), (Hit{}));
}

// Every language's tabs stay on the 640x480 screen and clear of each other and of the keys.
TEST(NameMouse, TabsDoNotOverlap) {
    for (int language = 0; language < 7; ++language) {
        for (int a = 0; a < kTabCount; ++a) {
            Rect first = TabRect(language, a);
            if (first.right == 0) {
                continue;
            }
            EXPECT_GE(first.left, 0);
            EXPECT_LE(first.right, 640);
            EXPECT_LT(first.bottom, 242 - 9 - 2);
            for (int b = a + 1; b < kTabCount; ++b) {
                Rect second = TabRect(language, b);
                if (second.right == 0) {
                    continue;
                }
                bool apart = first.right <= second.left || second.right <= first.left || first.bottom <= second.top ||
                             second.bottom <= first.top;
                EXPECT_TRUE(apart) << language << ": " << a << " and " << b;
            }
        }
    }
}

// The name is ten 22-pixel slots from x=182.
TEST(NameMouse, NameSlots) {
    Screen screen = EnglishScreen(kInputAlphabet);
    for (int slot = 0; slot < 10; ++slot) {
        EXPECT_EQ(HitTest(screen, 182 + slot * 22 + 11, 122), (Hit{Kind::Slot, slot}));
    }
    EXPECT_EQ(HitTest(screen, 181, 122), (Hit{}));
    EXPECT_EQ(HitTest(screen, 182 + 220, 122), (Hit{}));
    EXPECT_EQ(SlotRect(10).right, 0);
}

// Outside every control is nothing, including the frame around the keyboard and the corners.
TEST(NameMouse, OutsideIsNothing) {
    Screen screen = EnglishScreen(kInputAlphabet);
    EXPECT_EQ(HitTest(screen, 0, 0), (Hit{}));
    EXPECT_EQ(HitTest(screen, 639, 479), (Hit{}));
    EXPECT_EQ(HitTest(screen, 500, 100), (Hit{}));
    // The key-help panel on the right.
    EXPECT_EQ(HitTest(screen, 450, 100), (Hit{}));
    // Left of the first key and below the last row.
    EXPECT_EQ(HitTest(screen, 100, 250), (Hit{}));
    EXPECT_EQ(HitTest(screen, 150, 242 + 3 * 26 + 30), (Hit{}));
}

// The kana keyboards are not offered in the PAL release's languages.
TEST(NameMouse, KanaKeysAreNotOffered) {
    Screen screen = EnglishScreen(kInputKatakana);
    screen.language = 0;
    EXPECT_EQ(KeyRect(screen, 0).right, 0);
}

TEST(NameMouse, DialogWindows) {
    EXPECT_TRUE(DialogRect(kAreaHelp).Contains(0xAE + 10, 0xAA + 10));
    EXPECT_TRUE(DialogRect(kAreaConfirm).Contains(0xD8 + 10, 0xB8 + 10));
    EXPECT_FALSE(DialogRect(kAreaConfirm).Contains(0xAE + 10, 0xB8 + 10));
    EXPECT_TRUE(DialogRect(kAreaEmptyNotice).Contains(0xBA + 10, 0xB8 + 10));
    EXPECT_EQ(DialogRect(kAreaKeyboard).right, 0);
}

// "Accept?" has a Yes and a No line; the rest of the window is neither.
TEST(NameMouse, ConfirmChoices) {
    EXPECT_EQ(ConfirmChoice(0xD8 + 40, 0xB8 + 14 + 30), 1);
    EXPECT_EQ(ConfirmChoice(0xD8 + 40, 0xB8 + 14 + 50), -1);
    EXPECT_EQ(ConfirmChoice(0xD8 + 40, 0xB8 + 14 + 10), 0);
    EXPECT_EQ(ConfirmChoice(0xD8 + 200, 0xB8 + 14 + 30), 0);
    EXPECT_EQ(ConfirmChoice(0xD8 + 40, 0xB8 + 14 + 70), 0);
    EXPECT_TRUE(DialogRect(kAreaConfirm).Contains(0xD8 + 40, 0xB8 + 14 + 30));
}

// Counts of motion become 640x480 units through the window and the mapping the 2D is drawn with.
TEST(NameMouse, PointerScale) {
    // A 1280x960 target shown in a window of the same size: 2 pixels a unit.
    EXPECT_FLOAT_EQ(PointerUnitsPerCount(2.0f, 1280.0f, 1280.0f), 0.5f);
    // The same target in a window half as wide (a high-DPI display): a count is 2 pixels, a unit.
    EXPECT_FLOAT_EQ(PointerUnitsPerCount(2.0f, 1280.0f, 640.0f), 1.0f);
    // Interface size 50% halves the scale: a count covers twice the units.
    EXPECT_FLOAT_EQ(PointerUnitsPerCount(1.0f, 1280.0f, 1280.0f), 1.0f);
    // 16:9 window: the frame is letterboxed at 2.25 pixels a unit.
    EXPECT_NEAR(PointerUnitsPerCount(2.25f, 1920.0f, 1920.0f), 1.0f / 2.25f, 1e-6f);
    EXPECT_EQ(PointerUnitsPerCount(0.0f, 1280.0f, 1280.0f), 0.0f);
    EXPECT_EQ(PointerUnitsPerCount(2.0f, 0.0f, 1280.0f), 0.0f);
    EXPECT_EQ(PointerUnitsPerCount(2.0f, 1280.0f, 0.0f), 0.0f);
}

} // namespace dc::test
