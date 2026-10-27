#pragma once

#include <array>
#include <cstdint>
#include <deque>
#include <string>
#include <vector>

#include "options/rows.hpp"
#include "platform/config.hpp"
#include "ui/framework.hpp"

namespace dc::game {
class CTexture;
} // namespace dc::game

namespace dc::options {

constexpr int kTabY = 96;
constexpr int kTabGap = 24;
constexpr int kTabsRight = 532;
constexpr int kTabArrow = 14;
constexpr int kRowY = 138;
constexpr int kRowStep = 25;
constexpr int kVisibleRows = 8;
constexpr int kLabelX = 138;
constexpr int kValueX = 374;
constexpr int kValueRight = 554;
constexpr int kBarX = 580;
constexpr int kExitX = 136;
constexpr int kExitY = 352;
constexpr int kExitWidth = 60;
constexpr int kExitHeight = 32;
constexpr int kHelpY = kExitY + 44;

/**
 * Row index meaning the cursor is on the page tabs rather than on a row.
 */
constexpr int kOnTabs = -1;

/**
 * Clickable glyph the mouse pointer is over, drawn highlighted.
 */
enum class Glyph {
    None,
    L1,
    R1,
    Up,
    Down,
};

/**
 * State of the open options screen: its step, cursor, page, scroll and input tracking.
 */
struct Screen {
    bool                open = false;
    int                 mode = 0;
    int                 block_no = 0;
    int                 step = 0;
    int                 step_count = 0;
    bool                texture_ready = false;
    game::CTexture     *texture = nullptr;
    int                 page = 0;
    int                 row = 0;
    int                 first_tab = 0;
    int                 last_tab_dir = 1; /**< -1 after moving to a tab on the left, +1 to the right. */
    int                 last_row_dir = 1; /**< -1 after stepping a value left, +1 right. */
    std::vector<int>    first_row;
    float               cursor_x = 0.0f;
    float               cursor_y = 0.0f;
    int                 bar_frames = 0;
    platform::Config    opened;
    bool                save_failed = false;
    std::string         display_now;
    short              *game_messages = nullptr;
    std::array<int, 16> held = {}; /**< Frames each pad button bit has been held. */
    bool                pointing = false;
    float               pointer_x = 0.0f;
    float               pointer_y = 0.0f;
    std::uint32_t       mouse_buttons = 0;
    Glyph               glyph = Glyph::None;
    int                 binding_row = -1;
    std::string         binding_prompt;
    std::string         binding_wait;
};

extern Screen g_screen;

/**
 * Localised labels and help messages the screen draws, built for the current language.
 */
struct Texts {
    std::deque<ui::Label>             tabs;
    std::deque<std::deque<ui::Label>> labels;
    std::deque<std::deque<ui::Label>> values;
    ui::Label                         left;
    ui::Label                         right;
    ui::Label                         l1;
    ui::Label                         r1;
    ui::Label                         shortcuts;
    text::GameTextFile                help;

    Texts();
};

/**
 * Returns the screen's texts, building them on first use after a language change.
 */
Texts &GetTexts();

/**
 * Returns the page the screen shows.
 */
const Page &CurrentPage();

/**
 * Returns the number of setting rows on a page.
 */
int RowCount(int page);

/**
 * Returns the first visible row of the current page.
 */
int FirstRow();

/**
 * Returns the screen y of a row of the current page.
 */
int RowTop(int row);

/**
 * Returns whether a page has more rows than fit and so shows a scroll bar.
 */
bool Scrolls(int page);

/**
 * Returns the screen x of a page tab, counting from the first visible tab.
 */
int TabX(int tab);

/**
 * Returns the last page tab that fits on screen.
 */
int LastVisibleTab();

/**
 * Returns the screen x of the L1 glyph left of the tabs.
 */
int L1X();

/**
 * Returns the screen x of the R1 glyph right of the tabs.
 */
int R1X();

/**
 * Opens the screen, loading its files into buffer (the read buffer when null). Returns 0 on failure.
 */
int Open(int mode, int block_no, u_long128 *buffer);

/**
 * Runs one frame of the screen. Returns 1 once it has faded out and closed.
 */
int Run();

/**
 * Draws the screen.
 */
void Draw();

} // namespace dc::options
