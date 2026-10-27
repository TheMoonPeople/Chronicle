#pragma once

namespace dc::game {

/**
 * Buffers and draws the map editor's debug-text overlay.
 */
class CDebugFont {
public:
    int   x;            /**< Distance of the overlay from the left of the screen. */
    int   y;            /**< Distance of the overlay from the top of the screen. */
    int   width;        /**< Width of the overlay. */
    int   height;       /**< Height of the overlay. */
    char *texture_name; /**< Name of the texture the overlay draws its glyphs from. */
    int   alpha;        /**< Alpha the overlay composites its texture with. */
    int   length;       /**< Number of occupied bytes in the text buffer. */
    char  text[512];    /**< Buffered text awaiting display. */

    /**
     * Draws the buffered debug text.
     */
    void Draw();
};

} // namespace dc::game
