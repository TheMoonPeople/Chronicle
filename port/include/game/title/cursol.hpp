#pragma once

#include "common.h"

#include <array>

namespace dc::game {

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
 * Controls selection and easing for the title menu cursor.
 */
class CCursol {
public:
    float              y;        /**< Height the cursor has eased to. */
    float              target_y; /**< Height the cursor is easing towards. */
    std::array<int, 5> alpha;    /**< Opacity of each menu row. */
    int                select;   /**< Row the cursor stands on. @see TitleMenuRow. */
    char               arrived;  /**< Whether the cursor has reached its target. */

    /**
     * Constructs a cursor in its initial menu state.
     */
    CCursol();

    /**
     * Resets the cursor selection and animation state.
     */
    void Init();

    /**
     * Eases the cursor towards its target and fades the row opacities; returns 1 once it is there.
     */
    int Move();

    /**
     * Sets the cursor's target vertical position.
     */
    void Set(float new_target_y);

    /**
     * Returns the currently selected menu row.
     */
    int GetSelect();

    /**
     * Returns the cursor's current height, truncated to whole pixels.
     */
    int GetPos();
};

} // namespace dc::game
