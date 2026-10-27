#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <format>
#include <string_view>

#include "app/gameloop.hpp"
#include "dataread.hpp"
#include "dataset.hpp"
#include "ebattle.hpp"
#include "editpartsdata.hpp"
#include "gamemode.hpp"
#include "gameutil.hpp"
#include "main.hpp"
#include "mainselect.hpp"
#include "menu_save.hpp"

namespace dc::test {

using namespace dc::game;
using namespace dc::app;

namespace {

void Reset(int current_mode) {
    mode = current_mode;
    MapNo = 7;
    OldMapNo = 3;
    LocalMapNo = 9;
    NextMapNo = -1;
    main_select_menu_no = 5;
    constexpr std::string_view none = "none";
    std::ranges::copy(none, main_select_param);
    main_select_param[none.size()] = '\0';
    mc_mode = 0;
}

void Jump(int next) {
    Reset(GAME_MODE_MENU);
    NextMapNo = next;
    GameFollowMapJump();
}

} // namespace

TEST(IntegrationGameloop, MapJumpTransitions) {
    Jump(-1);
    ASSERT_TRUE(mode == GAME_MODE_MENU && MapNo == 7 && OldMapNo == 3);

    Jump(150);
    ASSERT_TRUE(mode == GAME_MODE_EDIT && MapNo == 150 && OldMapNo == 7 && LocalMapNo == 9);

    Jump(205);
    ASSERT_TRUE(mode == GAME_MODE_DUNGEON && MapNo == 205 && LocalMapNo == 5 && main_select_menu_no == 5);

    Jump(400);
    ASSERT_TRUE(mode == GAME_MODE_OPENING && MapNo == 400 && LocalMapNo == 0 && main_select_menu_no == 0);

    Jump(800);
    ASSERT_TRUE(mode == GAME_MODE_TITLE && MapNo == 800 && LocalMapNo == 0);
    ASSERT_TRUE(std::string_view(main_select_param) == "title" && main_select_menu_no == 0);

    Jump(801);
    ASSERT_TRUE(mode == GAME_MODE_RUSH_MOVIE && MapNo == 801);

    // Neither range: only OldMapNo moves.
    Jump(350);
    ASSERT_TRUE(mode == GAME_MODE_MENU && MapNo == 7 && OldMapNo == 7);

    // 1000 passes the >= 800 branch first, then becomes the ending's save.
    Jump(1000);
    ASSERT_TRUE(mode == GAME_MODE_SAVE && MapNo == -1 && LocalMapNo == 0 && mc_mode == SAVE_MENU_MODE_ENDING);
    ASSERT_TRUE(std::string_view(main_select_param) == "title");
}

TEST(IntegrationGameloop, LoopResults) {
    struct Case {
        int mode;
        int result;
        int next_mode;
        int next_map_no;
        int map_jump;
    };

    const auto cases = std::to_array<Case>({
        {GAME_MODE_LANGUAGE,     0,  GAME_MODE_LANGUAGE,   7,   -1 },
        {GAME_MODE_LANGUAGE,     1,  GAME_MODE_RUSH_MOVIE, 801, -1 },
        {GAME_MODE_MEMORY_CHECK, 1,  GAME_MODE_RUSH_MOVIE, 801, -1 },
        {GAME_MODE_RUSH_MOVIE,   1,  GAME_MODE_RUSH_MOVIE, 7,   800},
        {GAME_MODE_TITLE,        3,  GAME_MODE_DUNGEON,    7,   -1 },
        {GAME_MODE_TITLE,        4,  GAME_MODE_TITLE,      7,   801},
        {GAME_MODE_TITLE,        5,  GAME_MODE_OPENING,    7,   -1 },
        {GAME_MODE_EDIT,         1,  GAME_MODE_MENU,       7,   -1 },
        {GAME_MODE_EDIT,         2,  GAME_MODE_MENU,       7,   -1 },
        {GAME_MODE_EDIT,         3,  GAME_MODE_DUNGEON,    7,   -1 },
        {GAME_MODE_EDIT,         4,  GAME_MODE_MENU,       7,   -1 },
        {GAME_MODE_SAVE,         1,  GAME_MODE_MENU,       7,   -1 },
        {GAME_MODE_TRIAL_END,    1,  GAME_MODE_TITLE,      800, -1 },
        {GAME_MODE_OPENING,      1,  GAME_MODE_TITLE,      7,   0  },
        {GAME_MODE_LOADER,       1,  GAME_MODE_DUNGEON,    7,   -1 },
        {GAME_MODE_LOADER,       2,  GAME_MODE_MENU,       7,   -1 },
        {GAME_MODE_DUNGEON,      1,  GAME_MODE_MENU,       7,   -1 },
        {GAME_MODE_UNUSED_4,     1,  GAME_MODE_MENU,       7,   -1 },
        {GAME_MODE_MENU,         1,  GAME_MODE_MENU,       7,   -1 },
        {GAME_MODE_MENU,         -1, GAME_MODE_TITLE,      7,   800},
        {GAME_MODE_UNUSED_12,    0,  GAME_MODE_UNUSED_12,  7,   -1 },
    });
    for (const Case &test : cases) {
        Reset(test.mode);
        GameApplyLoopResult(test.mode, test.result);
        if (mode != test.next_mode || MapNo != test.next_map_no || NextMapNo != test.map_jump) {
            FAIL() << std::format("mode {} result {}: mode {} MapNo {} NextMapNo {}", test.mode, test.result, mode,
                                  MapNo, NextMapNo);
        }
    }
}

TEST(IntegrationGameloop, TitleNewGameJumpsToTheOpeningTown) {
    Reset(GAME_MODE_TITLE);
    GameApplyLoopResult(GAME_MODE_TITLE, 1);
    ASSERT_TRUE(mode == GAME_MODE_EDIT && NextMapNo == 400 && main_select_menu_no == 0);
    ASSERT_TRUE(std::string_view(main_select_param) == "e01");
}

// MenuLoop picks the next mode itself and returns 1; the result is the menu's, which leaves it.
TEST(IntegrationGameloop, DeveloperMenuKeepsTheModeItPicked) {
    Reset(GAME_MODE_EDIT);
    GameApplyLoopResult(GAME_MODE_MENU, 1);
    ASSERT_TRUE(mode == GAME_MODE_EDIT);
}

TEST(IntegrationGameloop, FrameBudgetAndStop) {
    ASSERT_TRUE(GameFrameCount() == 0 && !GameStopRequested());
    GameSetFrameBudget(3);
    GameRequestStop();
    ASSERT_TRUE(GameStopRequested());
}

// GetGaijiW reads EditGaijiTbl[code] for codes from -0x300: on the PS2 that is the cell count, the
// last word, of GaijiDataTbl's entry code + 0x300.
TEST(IntegrationGameloop, LinkAliases) {
    for (int code = -0x300; code < -0x251; code++) {
        ASSERT_TRUE(*reinterpret_cast<s32 *>(&EditGaijiTbl[code]) == GaijiDataTbl[code + 0x300][7]);
    }
}

TEST(IntegrationGameloop, GiveListParses) {
    ASSERT_TRUE(GameSetGive("210,211"));
    ASSERT_TRUE(GameSetGive("268"));
    ASSERT_TRUE(GameSetGive("376"));
    ASSERT_TRUE(!GameSetGive("377"));
    ASSERT_TRUE(!GameSetGive(""));
    ASSERT_TRUE(!GameSetGive("210,"));
    ASSERT_TRUE(!GameSetGive("sun"));
    ASSERT_TRUE(!GameSetGive("0"));
    ASSERT_TRUE(!GameSetGive("9999"));
}

} // namespace dc::test
