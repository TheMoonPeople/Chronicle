#include "langset.hpp"

#include <libpkt.h>

#include <array>

#include "dataalloc.hpp"
#include "dataread.hpp"
#include "dataset.hpp"
#include "fader.hpp"
#include "gamepad.hpp"
#include "mainselect.hpp"
#include "mglib.hpp"
#include "rect.hpp"
#include "snd.hpp"
#include "texture.hpp"
#include "vutext.hpp"

namespace dc::game {

Fader Fade;
int   Cursor;
int   Proc;

namespace {

constexpr int kLanguageCount = 5;

/**
 * Draws one band of the language plate texture at full width, faded towards
 * the given opacity.
 */
void DrawPlateBand(int y, int height, int opacity) {
    RECT      rect{.x = 0x9E, .y = y, .width = 0x144, .height = height};
    const int alpha = Fade.Get(opacity);
    set2DSprite(GetVif1Packet(), TexManager.GetTexture("lang_set", -1), &rect, &rect, alpha);
}

} // namespace

void LangsetInit() {
    InitializeDataBuffer();
    SetDataBuffer(&VisualData, 200000);
    SetDataBuffer(&MotionData, 500000);
    SetDataBuffer(&TextureData, 300000);
    SetPacketReadBuffer(40000, 300000);
    MGSetBGColor(0.0f, 0.0f, 0.0f, 128.0f);
    std::array<LOADTEXTURE_INFO, 5> textures{
        {
         {.name = "#frame_image_mes#640#" SCREEN_HEIGHT_STR "#4", .block_no = 26, .mipmap = 0},
         {.name = "#fukidashibase#640#" HALF_BUFFER_HEIGHT_STR "#4", .block_no = 26, .mipmap = 0},
         {.name = "#fontbase#512#256#1", .block_no = 26, .mipmap = 0},
         {.name = "titledat/lang_set.img", .block_no = 0, .mipmap = 0},
         {.name = "", .block_no = 0, .mipmap = 0},
         }
    };
    TexManager.Initialize(0x3FE0);
    TexManager.LoadTextureBlock(-1, textures.data(), read_buffer);
    GamePad.SetAutoRepeat(PAD_UP | PAD_DOWN, 30, 9);
    GamePad.MenuModeOn(120);
    Fade.value = 0;
    Cursor = 0;
    Proc = LANGSET_FADE_IN;
}

int LangsetLoop() {
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
        default:
            break;
    }

    LangsetDraw();
    return 0;
}

int LangsetProc() {
    if (GamePad.Down(PAD_UP) != 0) {
        Cursor--;
    }

    if (GamePad.Down(PAD_DOWN) != 0) {
        Cursor++;
    }

    if (Cursor < 0) {
        Cursor = kLanguageCount - 1;
    }

    if (Cursor > kLanguageCount - 1) {
        Cursor = 0;
    }

    if (GamePad.Down(PAD_START) != 0 || GamePad.Down(PAD_CROSS) != 0) {
        return 1;
    }

    return 0;
}

void LangsetDraw() {
    setbilinear(1);

    std::array<int, kLanguageCount> opacities{};
    opacities.fill(0x40);
    opacities[Cursor] = 0x80;

    TexManager.ReloadTexture(GetVif1Packet(), 0);
    DrawPlateBand(0, 0x5A, 0x80);

    constexpr std::array<int, kLanguageCount> entry_tops{0x5F, 0x9C, 0xD9, 0x116, 0x153};
    for (int language = 0; language < kLanguageCount; language++) {
        DrawPlateBand(entry_tops[language], 0x3C, opacities[language]);
    }
}

} // namespace dc::game
