#include "langset.hpp"

#include <libpkt.h>

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

Fader Fade;
int   Cursor;
int   Proc;

/** Opacity of each language entry. */
int Alpha[5];

void LangsetInit() {
    InitializeDataBuffer();
    SetDataBuffer(&VisualData, 200000);
    SetDataBuffer(&MotionData, 500000);
    SetDataBuffer(&TextureData, 300000);
    SetPacketReadBuffer(40000, 300000);
    MGSetBGColor(0.0f, 0.0f, 0.0f, 128.0f);
    LOADTEXTURE_INFO textures[] = {
        {"#frame_image_mes#640#" SCREEN_HEIGHT_STR "#4",    26, 0},
        {"#fukidashibase#640#" HALF_BUFFER_HEIGHT_STR "#4", 26, 0},
        {"#fontbase#512#256#1",                             26, 0},
        {"titledat/lang_set.img",                           0,  0},
        {"",                                                0,  0},
    };
    TexManager.Initialize(0x3FE0);
    TexManager.LoadTextureBlock(-1, textures, read_buffer);
    GamePad.SetAutoRepeat(PAD_UP | PAD_DOWN, 30, 9);
    GamePad.MenuModeOn(120);
    Fade.value = 0;
    Cursor = 0;
    Proc = LANGSET_FADE_IN;
}

int LangsetLoop();

int LangsetProc() {
    if (GamePad.Down(PAD_UP) != 0) {
        Cursor--;
    }

    if (GamePad.Down(PAD_DOWN) != 0) {
        Cursor++;
    }

    if (Cursor < 0) {
        Cursor = 4;
    }

    if (Cursor > 4) {
        Cursor = 0;
    }

    if (GamePad.Down(PAD_START) != 0 || GamePad.Down(PAD_CROSS) != 0) {
        return 1;
    }

    return 0;
}

/**
 * Fills in a rectangle's position and size.
 */
static inline void SetRect(RECT *rect, int x, int y, int width, int height) {
    rect->x = x;
    rect->y = y;
    rect->width = width;
    rect->height = height;
}

void LangsetDraw() {
    RECT rect;
    int  language;
    int  alpha;

    setbilinear(1);

    for (language = 0; language < 5; language++) {
        Alpha[language] = 0x40;
    }

    Alpha[Cursor] = 0x80;

    TexManager.ReloadTexture(GetVif1Packet(), 0);
    SetRect(&rect, 0x9E, 0, 0x144, 0x5A);
    alpha = Fade.Get(0x80);
    set2DSprite(GetVif1Packet(), TexManager.GetTexture("lang_set", -1), &rect, &rect, alpha);
    SetRect(&rect, 0x9E, 0x5F, 0x144, 0x3C);
    alpha = Fade.Get(Alpha[0]);
    set2DSprite(GetVif1Packet(), TexManager.GetTexture("lang_set", -1), &rect, &rect, alpha);
    SetRect(&rect, 0x9E, 0x9C, 0x144, 0x3C);
    alpha = Fade.Get(Alpha[1]);
    set2DSprite(GetVif1Packet(), TexManager.GetTexture("lang_set", -1), &rect, &rect, alpha);
    SetRect(&rect, 0x9E, 0xD9, 0x144, 0x3C);
    alpha = Fade.Get(Alpha[2]);
    set2DSprite(GetVif1Packet(), TexManager.GetTexture("lang_set", -1), &rect, &rect, alpha);
    SetRect(&rect, 0x9E, 0x116, 0x144, 0x3C);
    alpha = Fade.Get(Alpha[3]);
    set2DSprite(GetVif1Packet(), TexManager.GetTexture("lang_set", -1), &rect, &rect, alpha);
    SetRect(&rect, 0x9E, 0x153, 0x144, 0x3C);
    alpha = Fade.Get(Alpha[4]);
    set2DSprite(GetVif1Packet(), TexManager.GetTexture("lang_set", -1), &rect, &rect, alpha);
}
