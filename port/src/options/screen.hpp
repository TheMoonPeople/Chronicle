#pragma once

#include <cstdint>
#include <deque>
#include <string>
#include <vector>

#include "options/rows.hpp"
#include "platform/config.hpp"
#include "ui_framework.hpp"

class CTexture;

namespace options {

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

constexpr int kOnTabs = -1;

enum class Glyph {
    None,
    L1,
    R1,
    Up,
    Down,
};

struct Screen {
    bool             open = false;
    int              mode = 0;
    int              block_no = 0;
    int              step = 0;
    int              step_count = 0;
    bool             texture_ready = false;
    CTexture        *texture = nullptr;
    int              page = 0;
    int              row = 0;
    int              first_tab = 0;
    int              last_tab_dir = 1; // -1 = Left (<), +1 = Right (>)
    int              last_row_dir = 1; // -1 = Left (<), +1 = Right (>)
    std::vector<int> first_row;
    float            cursor_x = 0.0f;
    float            cursor_y = 0.0f;
    int              bar_frames = 0;
    Config           opened;
    bool             save_failed = false;
    std::string      display_now;
    short           *game_messages = nullptr;
    int              held[16] = {};
    bool             pointing = false;
    float            pointer_x = 0.0f;
    float            pointer_y = 0.0f;
    std::uint32_t    mouse_buttons = 0;
    Glyph            glyph = Glyph::None;
    int              binding_row = -1;
    std::string      binding_prompt;
    std::string      binding_wait;
};

extern Screen g_screen;

struct Texts {
    std::deque<UI::Label>             tabs;
    std::deque<std::deque<UI::Label>> labels;
    std::deque<std::deque<UI::Label>> values;
    UI::Label                         left;
    UI::Label                         right;
    UI::Label                         l1;
    UI::Label                         r1;
    UI::Label                         shortcuts;
    GameTextFile                      help;

    Texts();
};

Texts &GetTexts();

const Page &CurrentPage();
int         RowCount(int page);
int         FirstRow();
int         RowTop(int row);
bool        Scrolls(int page);
int         TabX(int tab);
int         LastVisibleTab();
int         L1X();
int         R1X();

int  Open(int mode, int block_no, u_long128 *buffer);
int  Run();
void Draw();

} // namespace options
