#pragma once

#include "common.h"

#include <libvu0.h>

#include <array>

namespace dc::game {

/**
 *          Animates and draws the nine-letter title logo.
 */
class CLogo {
public:
    /**
     *          Number of letters in the logo.
     */
    static constexpr int kLetterCount = 9;

    std::array<int, kLetterCount> alpha;        /**< Opacity of each letter. */
    std::array<int, kLetterCount> light;        /**< Light level each letter is drawn at. */
    int                           count;        /**< Ticks the fade has run for. */
    int                           unused_count; /**< Cleared alongside count and never read. */
    sceVu0FVECTOR                 position;     /**< Where the letters hang in the world. */

    /**
     *          Constructs the logo in its initial animation state.
     */
    CLogo();

    /**
     *          Resets the logo letters and placement.
     */
    void Init();

    /**
     *          Reveals the logo letters in sequence.
     */
    void Fade();

    /**
     *          Fades all logo letters out.
     */
    void FadeOut();

    /**
     *          Draws the sparkles whose time windows contain the given logo motion time.
     */
    void Sparkdraw(float time);

    /**
     *          Draws the logo letters with their current opacity and lighting.
     */
    void Draw();

    /**
     *          Advances the title fade and fades the letters out once it passes halfway.
     */
    void Move();
};

} // namespace dc::game
