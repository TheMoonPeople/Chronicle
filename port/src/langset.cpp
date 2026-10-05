#include "langset.hpp"

#include "fader.hpp"
#include "mainselect.hpp"

// Retail opens with a VU1 program call; the renderer has no programs.
PC_OVERRIDE int LangsetLoop() {
    switch (Proc) {
        case LANGSET_FADE_IN:
            if (Fade.In() != 0) {
                Proc = LANGSET_SELECT;
            }

            break;
        case LANGSET_SELECT:
            if (LangsetProc() != 0) {
                Proc = LANGSET_FADE_OUT;
            }

            break;
        case LANGSET_FADE_OUT:
            if (Fade.Out() != 0) {
                // The first two codes are not offered here, so the cursor counts from the third.
                LanguageCode = Cursor + 2;
                return 1;
            }

            break;
    }

    LangsetDraw();
    return 0;
}
