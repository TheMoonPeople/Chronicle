#include <gtest/gtest.h>

#include "clsmes.hpp"
#include "memcard.hpp"
#include "menu_pointer.hpp"
#include "menu_save.hpp"
#include "platform/input.hpp"

namespace {
void Open(int state) {
    InputSetScriptedDevices({});
    SaveMenu.texture_ready = 1;
    SaveMenu.key_no = state;
    MenuMouseUpdate();
}

int Click(unsigned button) {
    InputKeyboardMouse mouse;
    mouse.mouse_buttons = button;
    InputSetScriptedDevices(mouse);
    MenuMouseUpdate();
    int result = MenuPointerSyntheticDown();
    InputSetScriptedDevices({});
    SaveMenu.texture_ready = 0;
    MenuMouseUpdate();
    return result;
}

void Prompt(int left) {
    // The initial pointer fingertip is (346, 254). Put the inline choices on
    // the second message row, well away from the old hard-coded vertical plate.
    auto &mes = CommonMenuMes2;
    mes.text_x = left;
    mes.text_y = 220;
    mes.text_width = 200;
    mes.char_height = 24;
    mes.fukidashi = 0;
    mes.centre_rows = 0;
    mes.win_line_num = 2;
    mes.win_line[0] = {-0x2F8, 0, 24, 0, 0};
    mes.win_line[1] = {-0x2FA, 80, 24, 0, 0};
}
} // namespace

TEST(MenuSavePointer, LoadYesFollowsCentredInlinePrompt) {
    Prompt(300);
    Open(SAVE_KEY_LOAD_DECIDE);
    EXPECT_EQ(Click(1), kInputCross);
}

TEST(MenuSavePointer, SaveNoFollowsCentredInlinePrompt) {
    Prompt(200);
    Open(SAVE_KEY_SAVE_DECIDE);
    EXPECT_EQ(Click(1), kInputCircle);
}

TEST(MenuSavePointer, CompletedMessageCanBeDismissedAnywhere) {
    // Deliberately place the message away from the initial pointer.
    CommonMenuMes2.text_x = CommonMenuMes2.text_y = 0;
    CommonMenuMes2.text_columns = 1;
    CommonMenuMes2.text_rows = 1;
    Open(SAVE_KEY_END_SAVE);
    EXPECT_EQ(Click(1), kInputCross);
}

TEST(MenuSavePointer, BackDoesNotRequireHoveringAChoice) {
    Prompt(0);
    Open(SAVE_KEY_LOAD_DECIDE);
    EXPECT_EQ(Click(2), kInputCircle);
}

TEST(MenuSavePointer, LoadYesIncludesRenderedRowCentring) {
    Prompt(300);
    CommonMenuMes2.text_y = 200;
    CommonMenuMes2.char_height = 20;
    CommonMenuMes2.rows = 4;
    CommonMenuMes2.text_rows = 2;
    CommonMenuMes2.centre_rows = 1;
    Open(SAVE_KEY_LOAD_DECIDE);
    EXPECT_EQ(Click(1), kInputCross);
}

TEST(MenuSavePointer, EndingCompletionBackAcknowledgesWithCross) {
    Open(SAVE_KEY_END_SAVE_ENDING);
    EXPECT_EQ(Click(2), kInputCross);
}
