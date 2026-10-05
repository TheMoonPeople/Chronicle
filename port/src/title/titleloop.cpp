#include "common.h"

#include <libgraph.h>
#include <libvu0.h>

#include <bit>

#include "camerafollow.hpp"
#include "character.hpp"
#include "dataread.hpp"
#include "dataset.hpp"
#include "frame.hpp"
#include "gamemode.hpp"
#include "gamepad.hpp"
#include "main.hpp"
#include "memcard.hpp"
#include "menu_save.hpp"
#include "mglib.hpp"
#include "rect.hpp"
#include "savedata.hpp"
#include "snd.hpp"
#include "sound.hpp"
#include "texture.hpp"
#include "title/cursol.hpp"
#include "title/logo.hpp"
#include "title/scfader.hpp"
#include "title/sprite.hpp"
#include "title/title.hpp"
#include "title/titleloop.hpp"
#include "title_port.hpp"

// The bodies are retail's, which pass string literals as char *.
#pragma clang diagnostic ignored "-Wwritable-strings"

// Retail's TitleLoop and TitleDraw. The VU1 program upload that opened TitleLoop is gone (the
// renderer has no microprogram to load) and the previous-frame feedback covers the whole logical
// frame: PAL retail kept NTSC's 640x448 rect here, leaving the bottom 32 rows without the trail
// every other title scene draws over the full frame.

void InitOpeningBook(u_long128 *pack, int *param);
int  OpeningBookKey();
void OpeningBookDraw();
void TiPlayVolSE(int group, int no, int voice, float volume);

PC_OVERRIDE int TitleLoop() {
    sceVu0FVECTOR pos;
    sceVu0FMATRIX matrix;
    int           i;

    FCamera.GetPos(pos);
    FCamera.Step(1);
    FCamera.GetCameraMatrix(matrix);
    MGSetViewMatrix(matrix, pos);
    Cloud__2.Step();

    switch (CProcess.no) {
        case TITLE_STEP_FADE_IN:
            if (CFade.In()) {
                CProcess.no = TITLE_STEP_WAIT;
            }

            if (GamePad.Down(PAD_START) && EffCnt > 16) {
                CFade.Skip();
                CProcess.no = TITLE_STEP_LOGO;
            }

            if (EffCnt < 100) {
                EffCnt++;
            }

            break;

        case TITLE_STEP_RETURN_FADE_IN:
            if (CFade.In2()) {
                CProcess.no = TITLE_STEP_MENU;
            }

            break;

        case TITLE_STEP_WAIT:
            if (Wait < 120) {
                Wait++;
            } else {
                CProcess.no = TITLE_STEP_LOGO;
            }

            if (GamePad.Down(PAD_START)) {
                CProcess.no = TITLE_STEP_LOGO;
            }

            break;

        case TITLE_STEP_LOGO:
            if (CSprite.Se() == 0) {
                TiPlayVolSE(MIDI_PORT_SE_DEFAULT, 38, 21, 1.0f);
            }

            CSprite.Move();

            if (GamePad.Down(PAD_START)) {
                Logo.motion_type.state.time = 116.0f;
                Fade1 = 128;
                CSprite.x[0] = 700.0f;
            }

            if (CSprite.x[0] > 100.0f) {
                if (Logo.motion_type.state.time < 116.0f) {
                    for (i = 0; i < 9; i++) {
                        Spark[i].Step();
                    }

                    Logo.Step();
                    CLogo.Fade();
                } else {
                    if (Fade1 < 128) {
                        Fade1++;
                    } else {
                        Fade2 = (Fade2 + 2) & 0x7f;
                        keywait++;

                        if (keywait >= 500) {
                            keywait = 500;
                        }
                    }

                    CLogo.Move();
                }

                if (Fade1 > 127 && GamePad.Down(PAD_START)) {
                    TiPlayVolSE(MIDI_PORT_UNK_D, 122, 25, 1.0f);
                    CProcess.no = TITLE_STEP_MENU;
                    opcnt = 0;
                }

                if (Fade1 > 127) {
                    if (opcnt > 1800) {
                        CCursol.select = TITLE_MENU_ATTRACT;
                        CProcess.no = TITLE_STEP_ATTRACT_FADE_OUT;
                    } else {
                        opcnt++;
                    }
                }
            }

            break;

        case TITLE_STEP_MENU:
            CLogo.Move();

            if (CCursol.Move()) {
                if (GamePad.Down(PAD_UP)) {
                    CCursol.select--;
                    opcnt = 0;
                    TiPlayVolSE(MIDI_PORT_UNK_D, 122, 24, 1.0f);
                }

                if (GamePad.Down(PAD_DOWN)) {
                    CCursol.select++;
                    opcnt = 0;
                    TiPlayVolSE(MIDI_PORT_UNK_D, 122, 24, 1.0f);
                }

                if (CCursol.select < 0) {
                    CCursol.select = TITLE_MENU_OPTION;
                }

                if (CCursol.select > 2) {
                    CCursol.select = TITLE_MENU_NEW_GAME;
                }

                switch (CCursol.select) {
                    case TITLE_MENU_NEW_GAME:
                        CCursol.Set(304.0f);
                        break;
                    case TITLE_MENU_LOAD:
                        CCursol.Set(332.0f);
                        break;
                    case TITLE_MENU_OPTION:
                        CCursol.Set(364.0f);
                        break;
                }

                if (GamePad.Down(PAD_START) || GamePad.Down(PAD_CROSS)) {
                    TiPlayVolSE(MIDI_PORT_SE_DEFAULT, 38, 20, 1.0f);
                    CProcess.no = TITLE_STEP_MENU_BLINK;
                    opcnt = 0;
                }

                if (GamePad.Down(PAD_CIRCLE)) {
                    Fade1 = Fade2 = Fade3 = Fade4 = 0;
                    CSprite.Init();
                    CLogo.Init();
                    CCursol.Init();
                    CProcess.no = TITLE_STEP_LOGO;
                    opcnt = 0;
                }

                if (opcnt > 1800) {
                    CCursol.select = TITLE_MENU_ATTRACT;
                    CProcess.no = TITLE_STEP_ATTRACT_FADE_OUT;
                } else {
                    opcnt++;
                }
            }

            break;

        case TITLE_STEP_MENU_BLINK:
            CLogo.Move();
            brink = 1;
            brinkcnt++;

            if (brinkcnt > 60) {
                CProcess.no = TITLE_STEP_MENU_FADE_OUT;
            }

            break;

        case TITLE_STEP_MENU_FADE_OUT:
            brinkcnt++;

            if (CFade.Out()) {
                switch (CCursol.GetSelect()) {
                    case TITLE_MENU_NEW_GAME:
                        CProcess.no = TITLE_STEP_OPENING_BOOK_INIT;
                        break;
                    case TITLE_MENU_LOAD:
                        CProcess.no = TITLE_STEP_SAVE_INIT;
                        break;
                    case TITLE_MENU_OPTION:
                        CProcess.no = TITLE_STEP_OPTION_INIT;
                        break;
                }
            }

            break;

        case TITLE_STEP_ATTRACT_FADE_OUT:
            if (CFade.Out()) {
                SndStopAllSe();
                CSnd.Stop(MIDI_PORT_BGM);
                CSnd.StopVoice(0);
                CProcess.no = TITLE_STEP_EXIT;
            }

            break;

        case TITLE_STEP_OPENING_BOOK_INIT:
            SndStopAllSe();
            CSnd.Stop(MIDI_PORT_BGM);
            CSnd.StopVoice(0);
            CSnd.SetReverb(0, 4, 40);
            CSnd.SetVol(MIDI_PORT_SE_TITLE, 256);
            CSnd.SetVol(MIDI_PORT_SE_DEFAULT, 256);
            CSnd.SetVol(MIDI_PORT_UNK_D, 256);
            CSnd.SetVol(MIDI_PORT_SE_SPECIAL, 256);
            CSnd.SQ_Play(MIDI_PORT_BGM, 0);
            TiPlayVolSE(MIDI_PORT_SE_TITLE, 16, 24, 0.5f);
            {
                int book[2] = {2, 60};

                InitOpeningBook((u_long128 *) read_buffer, book);
            }
            CProcess.no = TITLE_STEP_OPENING_BOOK;
            break;

        case TITLE_STEP_OPENING_BOOK:
            if (OpeningBookKey()) {
                CProcess.no = TITLE_STEP_EXIT;
            }

            break;

        case TITLE_STEP_SAVE_INIT:
            InitMenuSave(SAVE_MENU_MODE_LOAD, 2, 0);
            CProcess.no = TITLE_STEP_SAVE;
            break;

        case TITLE_STEP_SAVE:
            switch (MenuSaveKey()) {
                case MENU_SAVE_RUNNING:
                    CFade.In();
                    break;
                case MENU_SAVE_LOADED:
                    /* CSaveData::map_no is private and retail reaches it from another
                       translation unit, so this is the offset main.cpp uses rather than
                       a getter the class does not have. */
                    MapJump(*(s32 *) ((char *) SaveData + 0x1C8), -1);
                    CProcess.no = TITLE_STEP_EXIT;
                    break;
                case MENU_SAVE_CLOSED:
                    brink = 0;
                    CFade.value = 0;
                    CProcess.no = TITLE_STEP_RETURN_FADE_IN;
                    break;
            }

            break;

        case TITLE_STEP_OPTION_INIT:
            InitMenuOption(OPTION_OPEN_TITLE, 2, 0);
            CProcess.no = TITLE_STEP_OPTION;
            break;

        case TITLE_STEP_OPTION:
            CFade.In();

            if (MenuOptionKey()) {
                brink = 0;
                CFade.value = 0;
                CProcess.no = TITLE_STEP_RETURN_FADE_IN;
            }

            break;

        case TITLE_STEP_EXIT:
            SndStopAllSe();
            CSnd.Stop(MIDI_PORT_BGM);
            CSnd.StopVoice(0);
            return CCursol.GetSelect() + 1;
    }

    TitleDraw();
    CSnd.Step();
    return 0;
}

/* A title menu row's height on screen, laid out for NTSC's picture and lowered to stay centred in
   the taller PAL one. */
#define MENU_Y(y) ((y) + (SCREEN_HEIGHT - 448) / 2)

PC_OVERRIDE void TitleDraw() {
    sceVu0FVECTOR light0 = {2.4578f, 9.9294f, -2.8074f, 0.0f};
    sceVu0FVECTOR light1 = {4.6086f, -10.4028f, -0.8286f, 0.0f};
    sceVu0FVECTOR light2 = {0.0f, 0.0f, -10.0f, 0.0f};
    sceVu0FMATRIX light;
    sceVu0FMATRIX color = {
        {191.0f, 105.0f, 76.0f,  128.0f},
        {63.0f,  51.0f,  127.0f, 128.0f},
        {40.0f,  30.0f,  30.0f,  128.0f},
        {0.0f,   0.0f,   0.0f,   0.0f  }
    };
    sceVu0FVECTOR light0b = {0.0f, 6.0f, -8.0f, 0.0f};
    sceVu0FVECTOR light1b = {0.0f, 0.0f, 0.0f, 0.0f};
    sceVu0FVECTOR light2b = {0.0f, 0.0f, 0.0f, 0.0f};
    sceVu0FMATRIX lightb;
    sceVu0FMATRIX colorb = {
        {128.0f, 128.0f, 112.0f, 128.0f},
        {0.0f,   0.0f,   0.0f,   0.0f  },
        {0.0f,   0.0f,   0.0f,   0.0f  },
        {0.0f,   0.0f,   0.0f,   0.0f  }
    };
    sceVu0FVECTOR ambient = {0.0f, 0.0f, 0.0f, 100.0f};

    if (CProcess.no == TITLE_STEP_FADE_IN || (CProcess.no == TITLE_STEP_MENU_FADE_OUT && CCursol.GetSelect() == TITLE_MENU_NEW_GAME) || CProcess.no == TITLE_STEP_ATTRACT_FADE_OUT) {
        ambient[3] = (float) CFade.Get(128);
    } else {
        ambient[3] = 128.0f;
    }

    MGSetAmbient(ambient);

    sceVu0Normalize(light0, light0);
    sceVu0Normalize(light1, light1);
    sceVu0Normalize(light2, light2);
    sceVu0NormalLightMatrix(light, light0, light1, light2);
    MGSetPLight(light, color);

    if (CProcess.no != TITLE_STEP_OPENING_BOOK && CProcess.no != TITLE_STEP_EXIT) {
        static float rot[4] = {0.0f, 0.0f, 0.0f, 0.0f};

        TexManager.ReloadTexture(GetVif1Packet(), 0);
        rot[1] += 0.001f;

        if (rot[1] > 3.14f) {
            rot[1] -= 6.28f;
        }

        ObjectFrame3->SetRotation(rot[0], rot[1], rot[2]);
        ObjectFrame3->SetScale(TitlePortBackdropScale(), 1.0f, TitlePortBackdropScale());
        MGDraw(ObjectFrame3);
    }

    if (CProcess.no != TITLE_STEP_OPENING_BOOK && CProcess.no != TITLE_STEP_EXIT) {
        TexManager.ReloadTexture(GetVif1Packet(), 1);

        if (CProcess.no == TITLE_STEP_FADE_IN || (CProcess.no == TITLE_STEP_MENU_FADE_OUT && CCursol.GetSelect() == TITLE_MENU_NEW_GAME) || CProcess.no == TITLE_STEP_ATTRACT_FADE_OUT) {
            ambient[3] = (float) CFade.Get(100);
        } else {
            ambient[3] = 100.0f;
        }

        MGSetAmbient(ambient);
        Cloud__2.SetScale(TitlePortBackdropScale(), 1.0f, 1.0f);
        Cloud__2.Draw();
    }

    if (CProcess.no != TITLE_STEP_FADE_IN || CFade.Get(128) >= 4) {
        sceGsTest test = {};

        MGSetGsTEST(&test);
        test.bits.date = 0;
        test.bits.ate = 0;
        MGSetGsTEST(&test);
        setbilinear(1);

        const TitleFogBand bands[] = {
            {CRect_i_(0, 0, 640, 85),    CRect_i_(1, 0, 639, 45),   112},
            {CRect_i_(0, 85, 640, 84),   CRect_i_(1, 42, 639, 45),  114},
            {CRect_i_(0, 169, 640, 84),  CRect_i_(1, 84, 639, 45),  114},
            {CRect_i_(0, 253, 640, 84),  CRect_i_(1, 126, 639, 45), 114},
            {CRect_i_(0, 336, 640, 105), CRect_i_(1, 167, 639, 57), 114},
            {CRect_i_(0, 441, 640, 39),  CRect_i_(1, 220, 639, 19), 114},
        };
        TitlePortFog(bands);

        MGSetGsTEST(0);
    }

    {
        sceGsTex0 tex0;
        sceGsTex0 image;

        MGGetFBuffTex(&tex0);
        image = std::bit_cast<sceGsTex0>(TexManager.GetTexture("frame_image", -1)->tex0);
        tex0.PSM = 1;
        MGStretchMoveImage(&tex0, CRect_i_(0, 0, 10240, SCREEN_HALF_HEIGHT * 16), &image, CRect_i_(0, 0, 10240, SCREEN_HALF_HEIGHT * 16));
    }
    MGClearZBuffer(0);

    switch (CProcess.no) {
        case TITLE_STEP_LOGO:
        case TITLE_STEP_RETURN_FADE_IN:
        case TITLE_STEP_MENU:
        case TITLE_STEP_MENU_BLINK:
        case TITLE_STEP_MENU_FADE_OUT:
        case TITLE_STEP_ATTRACT_FADE_OUT:
            sceVu0Normalize(light0b, light0b);
            sceVu0Normalize(light1b, light1b);
            sceVu0Normalize(light2b, light2b);
            sceVu0NormalLightMatrix(lightb, light0b, light1b, light2b);
            MGSetPLight(lightb, colorb);
            CLogo.Sparkdraw(Logo.motion_type.state.time);
            CLogo.Draw();

            set2DSprite(GetVif1Packet(), TexManager.GetTexture("main", -1), CRect_i_(0, 62, 288, 170), CRect_i_(0, 0, 288, 170), (u_char) CFade.Get(Fade4));
            set2DSprite(GetVif1Packet(), TexManager.GetTexture("main", -1), CRect_i_(288, 112, 72, 190), CRect_i_(288, 50, 72, 190), (u_char) CFade.Get(Fade4));
            set2DSprite(GetVif1Packet(), TexManager.GetTexture("main", -1), CRect_i_(360, 142, 412, 160), CRect_i_(360, 80, 412, 160), (u_char) CFade.Get(Fade4));

            ambient[3] = (float) CFade.Get(128);
            MGSetAmbient(ambient);

            if (CFade.Get(128) == 128) {
                CSprite.Draw();
            }

            set2DSprite(GetVif1Packet(), TexManager.GetTexture("start", -1), CRect_i_(64, MENU_Y(362), 512, 64), CRect_i_(0, 64, 512, 64), (u_char) CFade.Get(Fade1));

            if (CProcess.no == TITLE_STEP_LOGO) {
                set2DSprite(GetVif1Packet(), TexManager.GetTexture("start", -1), CRect_i_(64, MENU_Y(296), 512, 64), CRect_i_(0, 0, 512, 64), (u_char) CFade.Get(Fade2));
            } else {
                static int br = 128;

                if (brink) {
                    if (brinkcnt % 3 == 0) {
                        br = 32;
                    } else {
                        br = 128;
                    }
                } else {
                    br = 128;
                }

                switch (CCursol.GetSelect()) {
                    case TITLE_MENU_NEW_GAME:
                        set2DSprite(GetVif1Packet(), TexManager.GetTexture("start3", -1), CRect_i_(193, MENU_Y(280), 256, 32), CRect_i_(0, 0, 256, 32), (u_char) CFade.Get(br));
                        set2DSprite(GetVif1Packet(), TexManager.GetTexture("start3", -1), CRect_i_(193, MENU_Y(312), 256, 32), CRect_i_(0, 32, 256, 32), (u_char) CFade.Get(32));
                        set2DSprite(GetVif1Packet(), TexManager.GetTexture("start3", -1), CRect_i_(193, MENU_Y(344), 256, 32), CRect_i_(0, 64, 256, 32), (u_char) CFade.Get(32));
                        break;

                    case TITLE_MENU_LOAD:
                        set2DSprite(GetVif1Packet(), TexManager.GetTexture("start3", -1), CRect_i_(193, MENU_Y(280), 256, 32), CRect_i_(0, 0, 256, 32), (u_char) CFade.Get(32));
                        set2DSprite(GetVif1Packet(), TexManager.GetTexture("start3", -1), CRect_i_(193, MENU_Y(312), 256, 32), CRect_i_(0, 32, 256, 32), (u_char) CFade.Get(br));
                        set2DSprite(GetVif1Packet(), TexManager.GetTexture("start3", -1), CRect_i_(193, MENU_Y(344), 256, 32), CRect_i_(0, 64, 256, 32), (u_char) CFade.Get(32));
                        break;

                    case TITLE_MENU_OPTION:
                        set2DSprite(GetVif1Packet(), TexManager.GetTexture("start3", -1), CRect_i_(193, MENU_Y(280), 256, 32), CRect_i_(0, 0, 256, 32), (u_char) CFade.Get(32));
                        set2DSprite(GetVif1Packet(), TexManager.GetTexture("start3", -1), CRect_i_(193, MENU_Y(312), 256, 32), CRect_i_(0, 32, 256, 32), (u_char) CFade.Get(32));
                        set2DSprite(GetVif1Packet(), TexManager.GetTexture("start3", -1), CRect_i_(193, MENU_Y(344), 256, 32), CRect_i_(0, 64, 256, 32), (u_char) CFade.Get(br));
                        break;
                }

                if (CProcess.no != TITLE_STEP_ATTRACT_FADE_OUT) {
                    setbilinear(0);
                    CursorVibeCnt++;

                    RECT rect = {0, 0, 32, 32};

                    DrawObjectVibe(225, CCursol.GetPos(), TexManager.GetTexture("icon01", -1), rect, 128, CFade.Get(128));
                    setbilinear(1);
                }
            }

            break;

        case TITLE_STEP_OPENING_BOOK:
            TexManager.ReloadTexture(GetVif1Packet(), 2);
            OpeningBookDraw();
            break;

        case TITLE_STEP_SAVE:
            TexManager.ReloadTexture(GetVif1Packet(), 2);
            DrawMenuSave(0);
            break;

        case TITLE_STEP_OPTION:
            TexManager.ReloadTexture(GetVif1Packet(), 2);
            DrawMenuOption();
            break;
    }

    TitlePortFeedback(35);
}
