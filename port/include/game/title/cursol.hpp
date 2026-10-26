#pragma once

#include "common.h"

/**
 * Rows of the title menu, as CCursol::select holds them.
 */
// clang-format off
enum TitleMenuRow {
    TITLE_MENU_NEW_GAME = 0, /**< New game. */
    TITLE_MENU_LOAD     = 1, /**< Load. */
    TITLE_MENU_OPTION   = 2, /**< Options. */
    TITLE_MENU_ATTRACT  = 3, /**< Attract movie, after the idle timeout. */
    TITLE_MENU_UNK_4    = 4, /**< Drawn but never selected. */
};

// clang-format on

/**
 *          Controls selection and easing for the title menu cursor.
 */
class CCursol {
public:
    float y;        /**< Height the cursor has eased to. */
    float target_y; /**< Height the cursor is easing towards. */
    int   alpha[5]; /**< Opacity of each menu row. */
    int   select;   /**< Row the cursor stands on. @see TitleMenuRow. */
    char  arrived;  /**< Whether the cursor has reached its target. */

    /**
     *          Constructs a cursor in its initial menu state.
     *
     * @mangled __ct__7CCursolFv
     * @address 0x1DD4CC0
     * @size 0x30
     */
    CCursol();

    /**
     *          Resets the cursor selection and animation state.
     *
     * @mangled Init__7CCursolFv
     * @address 0x1DD4CF0
     * @size 0x2C
     * @unknownret
     */
    void Init();

    /**
     *          Processes input and advances cursor movement.
     *
     * @mangled Move__7CCursolFv
     * @address 0x1DD4D20
     * @size 0x384
     * @unknownret
     */
    int Move();

    /**
     *          Sets the cursor's target vertical position.
     *
     * @mangled Set__7CCursolFf
     * @address 0x1DD50B0
     * @size 0xC
     * @unknownret
     */
    void Set(float new_target_y);

    /**
     *          Returns the currently selected menu row.
     *
     * @mangled GetSelect__7CCursolFv
     * @address 0x1DD50C0
     * @size 0xC
     * @unknownret
     */
    int GetSelect();

    /**
     *          Returns the cursor's current display position.
     *
     * @mangled GetPos__7CCursolFv
     * @address 0x1DD50D0
     * @size 0x24
     * @unknownret
     */
    int GetPos();
};
