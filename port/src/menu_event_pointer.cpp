#include "menu_event_pointer.hpp"

#include "menu_pointer.hpp"
#include "menu_save.hpp"
#include "platform/input.hpp"

/**
 * Runs the original item picker selection and drawing logic.
 */
int PortEventItemSelectLoop(int *result);

/** The item picker's selection and transition state. */
extern MINI_MENU_INFO MiniMenu;
/** The item board's logical screen origin. */
extern float EventBoardPos[2];
/** The animated vertical origin of the first item row. */
extern int EventItemMoveY;
/** The item picker's animated hand position. */
extern float MiniCur[2];
/** Whether the item picker textures are ready. */
extern int MiniEventTexReadFlag;

namespace {

/** The pointer owned by the door/object item picker. */
MenuPointer event_pointer;
/** Whether the item picker remained active after its last update. */
bool event_active = false;

/**
 * Finds an occupied item cell within the animated board's visible clip.
 */
int ItemHit(float x, float y) {
    const float left = EventBoardPos[0] + 28.0f;
    const float top = EventBoardPos[1] + 56.0f;
    if (x < left || x >= left + 210.0f || y < top || y >= top + 80.0f) {
        return -1;
    }
    for (int index = 0; index < MiniMenu.event_item_num; ++index) {
        const float cell_left = left + (index % 5) * 42.0f;
        const float cell_top = static_cast<float>(EventItemMoveY + (index / 5) * 40);
        if (x >= cell_left && x < cell_left + 42.0f && y >= cell_top && y < cell_top + 40.0f) {
            return index;
        }
    }
    return -1;
}

} // namespace

/**
 * Tracks the item picker's lifetime while preserving its original item handling.
 */
int EventItemSelectLoop(int *result) {
    const int done = PortEventItemSelectLoop(result);
    event_active = done == 0;
    return done;
}

bool EventItemPointerUpdate() {
    const bool active = event_active;
    event_active = false;
    if (!active) {
        return !event_pointer.Close();
    }
    if (!event_pointer.IsOpen()) {
        event_pointer.Open();
    }
    const MenuPointerTake take = event_pointer.Take(MiniCur[0], MiniCur[1]);
    MenuPointerShow(&event_pointer);
    if (!MiniEventTexReadFlag) {
        return true;
    }
    if (MiniMenu.state == MINI_MENU_REFUSED) {
        if ((take.clicked & 3) != 0) {
            MenuPointerPress(kInputCircle);
        }
        return true;
    }
    if (MiniMenu.state != MINI_MENU_CHOOSING) {
        return true;
    }
    if ((take.clicked & 2) != 0) {
        MenuPointerPress(kInputCircle);
        return true;
    }
    const int hit = take.pointing ? ItemHit(event_pointer.x, event_pointer.y) : -1;
    if (hit >= 0 && take.moved && take.wheel == 0.0f) {
        MiniMenu.cursor = hit;
        if ((take.clicked & 1) != 0) {
            MenuPointerPress(kInputCross);
        }
    }
    if (take.pointing && take.wheel != 0.0f) {
        MenuPointerPress(take.wheel > 0.0f ? kInputUp : kInputDown);
    }
    return true;
}
