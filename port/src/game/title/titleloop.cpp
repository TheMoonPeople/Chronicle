#include "common.h"

#include <libgraph.h>
#include <libpkt.h>
#include <libvu0.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdlib>
#include <cstring>

#include "app/gameloop.hpp"
#include "battle_globals.hpp"
#include "camera.hpp"
#include "camerafollow.hpp"
#include "character.hpp"
#include "dataalloc.hpp"
#include "dataread.hpp"
#include "dataset.hpp"
#include "editloop.hpp"
#include "frame.hpp"
#include "framevu1.hpp"
#include "gamemode.hpp"
#include "gamepad.hpp"
#include "main.hpp"
#include "mainselect.hpp"
#include "mathutil.hpp"
#include "mds.hpp"
#include "memcard.hpp"
#include "menu_save.hpp"
#include "mglib.hpp"
#include "object.hpp"
#include "rect.hpp"
#include "savedata.hpp"
#include "snd.hpp"
#include "sound.hpp"
#include "texture.hpp"
#include "title/cursol.hpp"
#include "title/dispfade.hpp"
#include "title/logo.hpp"
#include "title/op_a.hpp"
#include "title/op_b.hpp"
#include "title/op_c.hpp"
#include "title/rushmovi.hpp"
#include "title/scfader.hpp"
#include "title/script.hpp"
#include "title/sprite.hpp"
#include "title/title.hpp"
#include "title/title_draw.hpp"
#include "title/tisound.hpp"
#include "title/title_menu.hpp"
#include "title/titleloop.hpp"
#include "ui/menu_pointer.hpp"
#include "vutext.hpp"
#include "wind.hpp"

namespace dc::game {

using namespace dc::title;
using namespace dc::ui;

// TexManager.GetTexture takes the texture name as char *.
#pragma clang diagnostic ignored "-Wwritable-strings"

int    Fade1;
int    Fade2;
int    Fade3;
int    Fade4;
int    Wait;
int    opcnt;
int    keywait;
u_char brink;
int    brinkcnt;
int    EffCnt;

void TitleInit(int mode) {
    float scale = 750.0f;
    float far_z = 65535.0f;

    InitializeDataBuffer();
    SetDataBuffer(&VisualData, 200000);
    SetDataBuffer(&MotionData, 500000);
    SetDataBuffer(&TextureData, 300000);
    SetPacketReadBuffer(40000, 300000);

    MGSetRenderInfo(scale, 4.0f, far_z);
    Camera.SetRef(nullptr, 0.0f, 0.0f, 0.0f);
    Camera.SetPos(nullptr, 0.0f, 0.0f, 0.0f);
    FCamera.SetFollow(0.0f, 0.0f, 0.0f);
    FCamera.SetAngle(0.0f);
    FCamera.SetDistance(180.0f);
    FCamera.SetHeight(2.0f);
    MGSetBGColor(0.0f, 0.0f, 0.0f, 128.0f);
    setbilinear(1);

    LOADTEXTURE_INFO menu_textures[] = {
        {"#fukidashibase#640#" HALF_BUFFER_HEIGHT_STR "#4", 26, 0},
        {"#fontbase#512#256#1",                             26, 0},
        {"meswin/gaiji.img",                                26, 0},
        {"meswin/fuki256.img",                              26, 0},
        {"meswin/syst04.img",                               26, 0},
        {"",                                                0,  0}
    };

    TexManager.Initialize(16352);
    TexManager.LoadTextureBlock(-1, menu_textures, read_buffer);
    LoadFileMenuData("stayframe.img", read_buffer);
    TexManager.EnterFixTextureZ(reinterpret_cast<u_char *>(read_buffer));

    switch (LanguageCode) {
        case LANG_JAPANESE:
        case LANG_ENGLISH_US:
            LoadFile("titledat/title.pak", read_buffer, nullptr);
            break;
        case LANG_ENGLISH_UK:
            LoadFile("titledat/title_eu.pak", read_buffer, nullptr);
            break;
        case LANG_FRENCH:
            LoadFile("titledat/title_f.pak", read_buffer, nullptr);
            break;
        case LANG_GERMAN:
            LoadFile("titledat/title_g.pak", read_buffer, nullptr);
            break;
        case LANG_ITALIAN:
            LoadFile("titledat/title_i.pak", read_buffer, nullptr);
            break;
        case LANG_SPANISH:
            LoadFile("titledat/title_s.pak", read_buffer, nullptr);
            break;
        default:
            break;
    }

    LOADTEXTURE_INFO2 title_textures[] = {
        {"#frame_image#640#" HALF_BUFFER_HEIGHT_STR "#4", 0, 0},
        {"#frame_image#640#" HALF_BUFFER_HEIGHT_STR "#4", 1, 0},
        {nullptr,                                         0, 0},
        {nullptr,                                         1, 0},
        {nullptr,                                         1, 0},
        {nullptr,                                         1, 0},
        {nullptr,                                         1, 0},
        {nullptr,                                         1, 0},
        {nullptr,                                         1, 0},
        {nullptr,                                         1, 0},
        {nullptr,                                         0, 0}
    };

    title_textures[2].name = reinterpret_cast<char *>(GetPackFile(read_buffer, "bg.img", nullptr));
    title_textures[3].name = reinterpret_cast<char *>(GetPackFile(read_buffer, "title.img", nullptr));
    title_textures[4].name = reinterpret_cast<char *>(GetPackFile(read_buffer, "main.img", nullptr));
    title_textures[5].name = reinterpret_cast<char *>(GetPackFile(read_buffer, "pat01.img", nullptr));
    title_textures[6].name = reinterpret_cast<char *>(GetPackFile(read_buffer, "pat02.img", nullptr));
    title_textures[7].name = reinterpret_cast<char *>(GetPackFile(read_buffer, "start.img", nullptr));
    title_textures[8].name = reinterpret_cast<char *>(GetPackFile(read_buffer, "start3.img", nullptr));
    title_textures[9].name = reinterpret_cast<char *>(GetPackFile(read_buffer, "icon01.img", nullptr));
    TexManager.LoadTextureBlock(-1, title_textures);

    sceVu0FVECTOR pos;

    ObjectFrame3 = LoadMDSFile(GetPackFile(read_buffer, "sky.mds", nullptr), 2, 0);
    pos[0] = 0.0f;
    pos[1] = 0.0f;
    pos[2] = -28.0f;
    ObjectFrame3->SetPosition(pos);

    Cloud__2.LoadPackData(read_buffer, "cloud.cfg", &MotionData, nullptr);
    Cloud__2.motion_type.state.time = 100.0f;
    Cloud__2.motion_type.state.blend_step = 0.1f;
    Cloud__2.motion_type.state.motion_no = 0;
    Cloud__2.motion_type.state.playing_no = 0;
    Cloud__2.motion_type.state.blending = false;
    Cloud__2.motion_no = 0;
    Cloud__2.SetPosition(0.0f, 0.0f, -5.0f);

    Logo.LoadPackData(read_buffer, "logo.cfg", &MotionData, nullptr);
    Logo.motion_type.state.time = 1.0f;
    Logo.motion_type.state.blend_step = 0.1f;
    Logo.motion_type.state.motion_no = 0;
    Logo.motion_type.state.playing_no = 0;
    Logo.motion_type.state.blending = false;
    Logo.motion_no = 0;

    char *spark_names[9] = {
        "logo_p1.cfg", "logo_p2.cfg", "logo_p3.cfg", "logo_p4.cfg", "logo_p5.cfg",
        "logo_p6.cfg", "logo_p7.cfg", "logo_p8.cfg", "logo_p9.cfg"};

    for (int i = 0; i < 9; i++) {
        Spark[i].LoadPackData(read_buffer, spark_names[i], &MotionData, nullptr);
        Spark[i].motion_type.state.time = 1.0f;
        Spark[i].motion_type.state.blend_step = 0.1f;
        Spark[i].motion_type.state.motion_no = 0;
        Spark[i].motion_type.state.playing_no = 0;
        Spark[i].motion_type.state.blending = false;
        Spark[i].motion_no = 0;
        Spark[i].SetPosition(0.0f, 11.599f, -19.099f);
    }

    CSnd.SetReverb(0, 4, 5);
    CSnd.SetReverb(1, 0, 0);
    CSnd.LoadSoundFileFromPack("title.txt", read_buffer);
    CSnd.SetVol(MIDI_PORT_SE_TITLE, 256);
    CSnd.SetVol(MIDI_PORT_SE_DEFAULT, 256);
    CSnd.SetVol(MIDI_PORT_UNK_D, 256);
    CSnd.SetVol(MIDI_PORT_SE_SPECIAL, 256);
    CSnd.SQ_Play(MIDI_PORT_AMBIENT, 0);

    Fade1 = Fade2 = Fade3 = Fade4 = Wait = 0;
    opcnt = 0;
    keywait = 0;
    brinkcnt = 0;
    brink = 0;
    EffCnt = 0;
    CProcess.no = TITLE_STEP_FADE_IN;
    CFade.value = 0;

    CSprite.Init();
    CLogo.Init();
    CCursol.Init();

    if (mode == 1) {
        CCursol.select = TITLE_MENU_LOAD;
        CCursol.Set(316.0f);

        for (int i = 0; i < 10; i++) {
            CCursol.Move();
        }
    }

    GamePad.SetAutoRepeat(PAD_UP | PAD_DOWN, 30, 9);
    GamePad.MenuModeOn(120);
}

namespace {
// A title menu row's screen y: laid out for NTSC's 448-line picture, lowered to stay centred in PAL.
constexpr int MenuY(int y) {
    return y + (SCREEN_HEIGHT - 448) / 2;
}

MenuPointer title_pointer;

struct TitlePointerAction {
    bool start = false;
    bool confirm = false;
    bool back = false;
    bool moved = false;
    int  confirm_hover = -1; // the exit confirmation's button under the pointer
};

TitlePointerAction TakeTitlePointer() {
    TitlePointerAction action;
    if (CProcess.no == TITLE_STEP_OPTION_INIT || CProcess.no == TITLE_STEP_OPTION ||
        CProcess.no == TITLE_STEP_SAVE_INIT || CProcess.no == TITLE_STEP_SAVE) {
        // The child screen owns pointer capture, including a button held through the fade.
        title_pointer.Abandon();
        return action;
    }
    if (CProcess.no != TITLE_STEP_LOGO && CProcess.no != TITLE_STEP_MENU) {
        title_pointer.Close();
        return action;
    }
    if (!title_pointer.IsOpen()) {
        title_pointer.Open();
    }
    auto take = title_pointer.Take(225.0f, CProcess.no == TITLE_STEP_MENU ? CCursol.GetPos() : 312.0f);
    MenuPointerShow(&title_pointer);
    if (!take.pointing || !take.moved) {
        return action;
    }
    const float x = title_pointer.x;
    const float y = title_pointer.y - static_cast<float>(MenuY(0));
    if (CProcess.no == TITLE_STEP_LOGO) {
        action.start = (take.clicked & 1) && x >= 64 && x < 576 && y >= 296 && y < 360;
    } else if (title::menu::ConfirmOpen()) {
        action.confirm_hover = title::menu::ConfirmHit(title_pointer.x, title_pointer.y);
        action.confirm = (take.clicked & 1) != 0 && action.confirm_hover >= 0;
        action.back = (take.clicked & 2) != 0;
    } else {
        action.back = (take.clicked & 2) != 0;
        const int row = title::menu::RowAt(x, title_pointer.y);
        if (row >= 0) {
            if (CCursol.select != row) {
                CCursol.select = row;
                TiPlayVolSE(MIDI_PORT_UNK_D, 122, 24, 1.0f);
            }
            action.moved = true;
            action.confirm = (take.clicked & 1) != 0;
            opcnt = 0;
        }
    }
    return action;
}
} // namespace

int TitleLoop() {
    sceVu0FVECTOR pos;
    sceVu0FMATRIX matrix;

    FCamera.GetPos(pos);
    FCamera.Step(1);
    FCamera.GetCameraMatrix(matrix);
    MGSetViewMatrix(matrix, pos);
    Cloud__2.Step();
    const auto pointer = TakeTitlePointer();

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

            if (GamePad.Down(PAD_START) || pointer.start) {
                CProcess.no = TITLE_STEP_LOGO;
            }

            break;

        case TITLE_STEP_LOGO:
            if (CSprite.Se() == 0) {
                TiPlayVolSE(MIDI_PORT_SE_DEFAULT, 38, 21, 1.0f);
            }

            CSprite.Move();

            if (GamePad.Down(PAD_START) || pointer.start) {
                Logo.motion_type.state.time = 116.0f;
                Fade1 = 128;
                CSprite.x[0] = 700.0f;
            }

            if (CSprite.x[0] > 100.0f) {
                if (Logo.motion_type.state.time < 116.0f) {
                    for (int i = 0; i < 9; i++) {
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
                        keywait = std::min(keywait, 500);
                    }

                    CLogo.Move();
                }

                if (Fade1 > 127 && (GamePad.Down(PAD_START) || pointer.start)) {
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

            if (title::menu::ConfirmOpen()) {
                title::menu::StepConfirm(pointer.confirm_hover, pointer.confirm, pointer.back);
                break;
            }

            if (CCursol.Move() || pointer.moved || pointer.back) {
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
                    CCursol.select = title::menu::kExitRow;
                }

                if (CCursol.select > title::menu::kExitRow) {
                    CCursol.select = TITLE_MENU_NEW_GAME;
                }

                CCursol.Set(title::menu::HandY(CCursol.select));

                if (GamePad.Down(PAD_START) || GamePad.Down(PAD_CROSS) || pointer.confirm) {
                    if (CCursol.select == title::menu::kExitRow) {
                        title::menu::OpenConfirm();
                    } else {
                        TiPlayVolSE(MIDI_PORT_SE_DEFAULT, 38, 20, 1.0f);
                        CProcess.no = TITLE_STEP_MENU_BLINK;
                    }
                    opcnt = 0;
                }

                if (!title::menu::ConfirmOpen() && (GamePad.Down(PAD_CIRCLE) || pointer.back)) {
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
                    default:
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

                InitOpeningBook(reinterpret_cast<u_long128 *>(read_buffer), book);
            }
            CProcess.no = TITLE_STEP_OPENING_BOOK;
            break;

        case TITLE_STEP_OPENING_BOOK:
            if (OpeningBookKey()) {
                CProcess.no = TITLE_STEP_EXIT;
            }

            break;

        case TITLE_STEP_SAVE_INIT:
            InitMenuSave(SAVE_MENU_MODE_LOAD, 2, nullptr);
            CProcess.no = TITLE_STEP_SAVE;
            break;

        case TITLE_STEP_SAVE:
            switch (MenuSaveKey()) {
                case MENU_SAVE_RUNNING:
                    CFade.In();
                    break;
                case MENU_SAVE_LOADED:
                    MapJump(SaveData->GetMapNo(), -1);
                    app::GameResumeLoaded();
                    CProcess.no = TITLE_STEP_EXIT;
                    break;
                case MENU_SAVE_CLOSED:
                    brink = 0;
                    CFade.value = 0;
                    CProcess.no = TITLE_STEP_RETURN_FADE_IN;
                    break;
                default:
                    break;
            }

            break;

        case TITLE_STEP_OPTION_INIT:
            InitMenuOption(OPTION_OPEN_TITLE, 2, nullptr);
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

        default:
            break;
    }

    TitleDraw();
    CSnd.Step();
    return 0;
}

void TitleDraw() {
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
        ambient[3] = static_cast<float>(CFade.Get(128));
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
        static std::array<float, 4> rot = {0.0f, 0.0f, 0.0f, 0.0f};

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
            ambient[3] = static_cast<float>(CFade.Get(100));
        } else {
            ambient[3] = 100.0f;
        }

        MGSetAmbient(ambient);
        Cloud__2.SetScale(TitlePortBackdropScale(), TitlePortBackdropHeightScale(), 1.0f);
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
            {CRect_i_(0, 0,   640, 85),  CRect_i_(1, 0,   639, 45), 112},
            {CRect_i_(0, 85,  640, 84),  CRect_i_(1, 42,  639, 45), 114},
            {CRect_i_(0, 169, 640, 84),  CRect_i_(1, 84,  639, 45), 114},
            {CRect_i_(0, 253, 640, 84),  CRect_i_(1, 126, 639, 45), 114},
            {CRect_i_(0, 336, 640, 105), CRect_i_(1, 167, 639, 57), 114},
            {CRect_i_(0, 441, 640, 39),  CRect_i_(1, 220, 639, 19), 114},
        };
        TitlePortFog(bands);

        MGSetGsTEST(nullptr);
    }

    {
        sceGsTex0 tex0;
        sceGsTex0 image;

        TitlePortCaptureFog();
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

            set2DSprite(GetVif1Packet(), TexManager.GetTexture("main", -1), CRect_i_(0, 62, 288, 170), CRect_i_(0, 0, 288, 170), static_cast<u_char>(CFade.Get(Fade4)));
            set2DSprite(GetVif1Packet(), TexManager.GetTexture("main", -1), CRect_i_(288, 112, 72, 190), CRect_i_(288, 50, 72, 190), static_cast<u_char>(CFade.Get(Fade4)));
            set2DSprite(GetVif1Packet(), TexManager.GetTexture("main", -1), CRect_i_(360, 142, 412, 160), CRect_i_(360, 80, 412, 160), static_cast<u_char>(CFade.Get(Fade4)));

            ambient[3] = static_cast<float>(CFade.Get(128));
            MGSetAmbient(ambient);

            if (CFade.Get(128) == 128) {
                CSprite.Draw();
            }

            set2DSprite(GetVif1Packet(), TexManager.GetTexture("start", -1), CRect_i_(64, MenuY(362), 512, 64), CRect_i_(0, 64, 512, 64), static_cast<u_char>(CFade.Get(Fade1)));

            if (CProcess.no == TITLE_STEP_LOGO) {
                set2DSprite(GetVif1Packet(), TexManager.GetTexture("start", -1), CRect_i_(64, MenuY(296), 512, 64), CRect_i_(0, 0, 512, 64), static_cast<u_char>(CFade.Get(Fade2)));
            } else {
                const int brightness = (brink != 0 && brinkcnt % 3 == 0) ? 32 : 128;

                // The painted rows (start3) have no Exit, so all four come from the font. None are
                // drawn on the way to the attract movie, whose number Exit's row shares.
                if (CProcess.no != TITLE_STEP_ATTRACT_FADE_OUT) {
                    title::menu::Draw(CCursol.GetSelect(), CFade.Get(brightness), CFade.Get(32));
                }

                if (CProcess.no != TITLE_STEP_ATTRACT_FADE_OUT && !title::menu::ConfirmOpen()) {
                    setbilinear(0);
                    CursorVibeCnt++;

                    RECT rect = {.x = 0, .y = 0, .width = 32, .height = 32};

                    float hand_x = 225.0f;
                    float hand_y = CCursol.GetPos();
                    MenuPointerHand(hand_x, hand_y);
                    DrawObjectVibe(hand_x, hand_y, TexManager.GetTexture("icon01", -1), rect, 128, CFade.Get(128));
                    setbilinear(1);
                }

                title::menu::DrawConfirm(CFade.Get(128));
                setbilinear(1);
            }

            break;

        case TITLE_STEP_OPENING_BOOK:
            TexManager.ReloadTexture(GetVif1Packet(), 2);
            OpeningBookDraw();
            break;

        case TITLE_STEP_SAVE:
            TexManager.ReloadTexture(GetVif1Packet(), 2);
            DrawMenuSave(nullptr);
            break;

        case TITLE_STEP_OPTION:
            TexManager.ReloadTexture(GetVif1Packet(), 2);
            DrawMenuOption();
            break;

        default:
            break;
    }

    if (CProcess.no == TITLE_STEP_LOGO && title_pointer.pointing) {
        float hand_x = 0.0f;
        float hand_y = 0.0f;
        if (MenuPointerHand(hand_x, hand_y)) {
            RECT rect = {.x = 0, .y = 0, .width = 32, .height = 32};
            setbilinear(0);
            DrawObjectVibe(hand_x, hand_y, TexManager.GetTexture("icon01", -1), rect, 128, 128);
            setbilinear(1);
        }
    }
    TitlePortFeedback(35);
}

} // namespace dc::game
