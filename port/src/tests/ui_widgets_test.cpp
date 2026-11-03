#include <gtest/gtest.h>

#include "ui/framework/widgets.hpp"

namespace dc::test {

using namespace dc::ui;

namespace {

// The fish exchange's list: 28 prizes (entries 0 to 27), five rows of 0x22 in view.
ListCursor Exchange() {
    ListCursor list(ListLayout{.first_row = Rect(0x96, 0x7E, 0x12E, 0x22)});
    list.Reset(27, 0x7E);
    return list;
}

// An item board of 15 slots, five to a row, two rows in view.
SlotGrid Board() {
    SlotGrid grid(SlotGridLayout{.window = Rect(208, 286, 210, 80)});
    grid.Reset(15);
    return grid;
}

} // namespace

TEST(UiListCursor, UpAndDownStopAtTheEndsAndTheRowsFollow) {
    ListCursor list = Exchange();
    list.Up();
    EXPECT_EQ(list.Cursor(), 0);
    for (int i = 0; i < 5; i++) {
        list.Down();
    }
    EXPECT_EQ(list.Cursor(), 5);
    EXPECT_EQ(list.Top(), 1);
    for (int i = 0; i < 40; i++) {
        list.Down();
    }
    EXPECT_EQ(list.Cursor(), 27);
    EXPECT_EQ(list.Top(), 23);
    list.Up();
    list.Up();
    list.Up();
    list.Up();
    list.Up();
    EXPECT_EQ(list.Cursor(), 22);
    EXPECT_EQ(list.Top(), 22);
}

TEST(UiListCursor, APageDownMayLeaveTheViewOneRowPastTheEndAsRetailDoes) {
    ListCursor list = Exchange();
    for (int i = 0; i < 4; i++) {
        list.PageDown();
    }
    EXPECT_EQ(list.Top(), 20);
    EXPECT_EQ(list.Cursor(), 20);
    list.PageDown();
    // 25 is more than one past the last top (23): back to it; the cursor stops on the last.
    EXPECT_EQ(list.Top(), 23);
    EXPECT_EQ(list.Cursor(), 25);
    list.Set(19, 19);
    list.PageDown();
    EXPECT_EQ(list.Top(), 24); // one past the last top stays
    EXPECT_EQ(list.Cursor(), 24);
    list.PageUp();
    list.PageUp();
    list.PageUp();
    list.PageUp();
    list.PageUp();
    EXPECT_EQ(list.Top(), 0);
    EXPECT_EQ(list.Cursor(), 0);
}

TEST(UiListCursor, TheHandEasesAQuarterAndSnapsWithinTwo) {
    ListCursor list = Exchange();
    list.EaseHand(0x7E + 0x22);
    EXPECT_EQ(list.HandY(), 0x7E + 8); // a quarter of 0x22, whole pixels
    for (int i = 0; i < 20; i++) {
        list.EaseHand(0x7E + 0x22);
    }
    // A quarter of three is nothing and three is not within two: retail's hand rests three short.
    EXPECT_EQ(list.HandY(), 0x7E + 0x22 - 3);
    list.SetHandY(0x7E + 0x22 - 2);
    list.EaseHand(0x7E + 0x22);
    EXPECT_EQ(list.HandY(), 0x7E + 0x22 - 2); // two short stays too: a quarter of two is nothing
    list.SetHandY(0x7E + 0x22 - 1);
    list.EaseHand(0x7E + 0x22);
    EXPECT_EQ(list.HandY(), 0x7E + 0x22); // within two, at once
}

TEST(UiListCursor, APointerFindsTheRowItIsOver) {
    ListCursor list = Exchange();
    EXPECT_EQ(list.Hit(0x100, 0x7E + 1), 0);
    EXPECT_EQ(list.Hit(0x100, 0x7E + 0x22 * 2 + 5), 2);
    EXPECT_EQ(list.Hit(0x100, 0x7E + 0x22 * 5), -1);
    EXPECT_EQ(list.Hit(0x10, 0x7E + 1), -1);
    list.Set(26, 23);
    EXPECT_EQ(list.Hit(0x100, 0x7E + 0x22 * 4 + 1), 27);
    list.Set(27, 24);
    EXPECT_EQ(list.Hit(0x100, 0x7E + 0x22 * 4 + 1), -1); // past the last entry
}

TEST(UiSlotGrid, TheCursorMovesBySlotsAndRowsWithoutWrappingEmptySlotsIncluded) {
    SlotGrid grid = Board();
    EXPECT_FALSE(grid.Move(-1, 0));
    EXPECT_FALSE(grid.Move(0, -1));
    EXPECT_TRUE(grid.Move(1, 0));
    EXPECT_EQ(grid.Cursor(), 1);
    EXPECT_TRUE(grid.Move(0, 1));
    EXPECT_EQ(grid.Cursor(), 6);
    EXPECT_TRUE(grid.Move(0, 1));
    EXPECT_EQ(grid.Cursor(), 11);
    EXPECT_EQ(grid.TopRow(), 1); // the third row brings the view down
    EXPECT_FALSE(grid.Move(0, 1));
    grid.SetCursor(14);
    EXPECT_FALSE(grid.Move(1, 0));
}

TEST(UiSlotGrid, TheRowsEaseToTheViewInWholePixels) {
    SlotGrid grid = Board();
    grid.Move(0, 1);
    grid.Move(0, 1);
    grid.Ease();
    EXPECT_EQ(grid.RowsY(), 286 - 10); // a quarter of 40
    for (int i = 0; i < 10; i++) {
        grid.Ease();
    }
    EXPECT_EQ(grid.RowsY(), 286 - 40);
    EXPECT_EQ(grid.CursorRect().y, 286 + 40);
}

TEST(UiSlotGrid, APointerFindsTheFilledSlotWhereItIsDrawn) {
    SlotGrid grid = Board();
    EXPECT_EQ(grid.Hit(208 + 42 + 1, 286 + 1, 7), 1);
    EXPECT_EQ(grid.Hit(208 + 1, 286 + 40 + 1, 7), 5);
    EXPECT_EQ(grid.Hit(208 + 42 * 2 + 1, 286 + 40 + 1, 7), -1); // empty
    EXPECT_EQ(grid.Hit(208 + 1, 286 - 1, 7), -1);               // outside the window
}

} // namespace dc::test
