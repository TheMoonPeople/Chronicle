#pragma argument_flag 0
#pragma argument_flag_ones 47, 54, 61, 64, 66, 159, 160, 162, 168, 1204
#pragma argument_flag_ones 1205, 1206, 1209, 1210, 1211, 1473, 1493, 1494, 1580, 1582
#pragma argument_flag_ones 1583, 1585, 1599

#include "common.h"

#include <cstdlib>
#include <cstring>

#include "camera.hpp"
#include "camerafollow.hpp"
#include "character.hpp"
#include "clsmes.hpp"
#include "dataalloc.hpp"
#include "dataread.hpp"
#include "frame.hpp"
#include "framevu1.hpp"
#include "gamepad.hpp"
#include "gameutil.hpp"
#include "main.hpp"
#include "mainselect.hpp"
#include "mds.hpp"
#include "mglib.hpp"
#include "snd.hpp"
#include "sound.hpp"
#include "texture.hpp"
#include "title/dispfade.hpp"
#include "title/op_a.hpp"
#include "title/opening.hpp"
#include "title/script.hpp"
#include "vector.hpp"
#include "vutext.hpp"

/**
 * Steps of the camera-pack read LoadSceneBG runs, as SceneFlg holds them.
 */
// clang-format off
enum SceneReadStep {
    SCENE_READ_START = 0, /**< Start reading the next camera pack. */
    SCENE_READ_WAIT  = 1, /**< Wait for the read, then unpack it. */
    SCENE_READ_IDLE  = 2, /**< Nothing to do until the next pack is requested. */
};

// clang-format on

/* The rectangle every 2D draw takes, declared here rather than reached through rect.h for the
   reason title.cpp declares its own: the rectangles this file builds are temporaries whose four
   stores come out ascending, and the constructor rect.h states assigns them in the other order. */
template <class T>
class CRect {
public:
    T x; /**< Left edge. */
    T y; /**< Top edge. */
    T w; /**< Width. */
    T h; /**< Height. */

    CRect() {}

    CRect(T left, T top, T width, T height) {
        x = left;
        y = top;
        w = width;
        h = height;
    }
} __attribute__((aligned(16)));

void MGSetViewMatrix(sceVu0FMATRIX view);
void MGFillBox(const CRect<int> &rect, u_char r, u_char g, u_char b, u_char a);

void wait_now_loading_vsync();
void InitializeDataBuffer();
void set2DSprite(sceVif1Packet *packet, CTexture *texture, const CRect<int> &dst, const CRect<int> &src, u_char alpha);
void set2DSprite(sceVif1Packet *packet, CTexture *texture, const CRect<int> &dst, const CRect<int> &src, u_char r, u_char g, u_char b, u_char a);

void OpB_InitProcess();
void OpB_InitProcess2();
void OpB_LoadDataBG();
void OpB_LoadDataBG2();
void OpB_MotionProcess();
void OpB_SoundProcess();
void OpB_DrawProcess();
void OpC_InitProcess();
void OpC_InitProcess2();
void OpC_InitProcess3();
void OpC_InitProcess4();
void OpC_InitProcess5();
void OpC_LoadDataBG();
void OpC_LoadDataBG2();
void OpC_LoadDataBG3();
void OpC_LoadDataBG4();
void OpC_LoadDataBG5();
void OpC_MotionProcess();
void OpC_SoundProcess();
void OpC_DrawProcess();
void OpD_InitProcess();
void OpD_InitProcess2();
void OpD_LoadDataBG();
void OpD_LoadDataBG2();
void OpD_MotionProcess();
void OpD_SoundProcess();
void OpD_DrawProcess();

static void LoadMessage();
static void LoadScene();
static void SceneChange();
static void PauseProcess();
static void SoundStop();
static void WaitKeyProcess();
static void MotionProcess();
static void SoundProcess();
static void DrawProcess();
static void DrawMess();

CDataAlloc2<1> CharaDataBuffer__2[7];
CDataAlloc2<1> DummyBuffer(-1);
CDataAlloc2<1> PassDataBuffer[3];
CDataAlloc2<1> MapDataBuffer(-1);
CDataAlloc2<1> WaterBuffer__2(-1);
CDataAlloc2<1> testBuffer(-1);

CCharacter Chara__3[23];
char       CharaTex__2[23];

/* Nothing calls this and nothing reads the table it fills: the link this file was built by removed
   both. They are here because the compiler carries state from one definition to the next, and the
   camera below is constructed with its arguments evaluated in an order no declaration that emits
   nothing reaches. */
static int OpeningWork[9];

static void OpeningWorkInit() {
    OpeningWork[0] = 1;
    OpeningWork[1] = 2;
    OpeningWork[2] = 3;
    OpeningWork[3] = 4;
    OpeningWork[4] = 5;
    OpeningWork[5] = 6;
    OpeningWork[6] = 7;
    OpeningWork[7] = 8;
    OpeningWork[8] = 9;
}

/* Nothing reads this pointer and the link removes it. Together with the exact folded constant
   below, it preserves the camera initializer's argument order without adding retained data. */

CCameraFollow OP_MainCamera(60.0f + 0.0f + 0.0f + 0.0f + 0.0f + 0.0f + 0.0f, 20.0f, 0.0f, 4.0f);

CCharacter       Cam__2[3];
MOTION_INFO      Op_MotionInfo;
static ClsMes    Mes1;
static CDispFade DispFade;

u_char              *PassReadBuffer;
u_char              *MesBuffer;
int                  OpBgmSqPort;
static tagFRAME_INF *frame_info_cam;
int                  SceneNp__2;
int                  Pause;
static int           CameraMode;
static int           SceneRp;
static int           SceneCnt;
static int           SceneFlg;
static int           SceneSw;
static float         PauseFrame;
static u_char        End;
static int           EndCnt;
static int           BgmOff;
static int           BgmVol;
static int           BgmNo;

void OpeningInit();

/**
 * Loads the opening movie's localized message resources.
 *
 * @mangled LoadMessage__Fv
 * @address 0x1DAF4C0
 * @size 0x1F4
 * @unknownret
 */
static void LoadMessage() {
    Mes1.Preset(MES_PRESET_AUTO_PAGE);
    Mes1.text_x = 90;
    Mes1.text_y = 350;
    Mes1.text_rate = 1.0f;
    Mes1.text_rate_set = 1.0f;
    Mes1.end_mark = false;
    Mes1.tex_block = 26;
    Mes1.tex_buff = MesWinTexBuff_01;
    Mes1.tail_to_x = 320;
    Mes1.tail_to_y = 234;
    Mes1.tail_half_width = 8;
    Mes1.tail_length = 64;
    Mes1.grow_x = 310;
    Mes1.grow_y = 210;

    switch (LanguageCode) {
        case LANG_JAPANESE:
            LoadFile("opdat/optext_0.mes", MesBuffer, 0);
            break;
        case LANG_ENGLISH_US:
            LoadFile("opdat/optext_1.mes", MesBuffer, 0);
            break;
        case LANG_ENGLISH_UK:
            LoadFile("opdat/optext_2.mes", MesBuffer, 0);
            break;
        case LANG_FRENCH:
            LoadFile("opdat/optext_3.mes", MesBuffer, 0);
            break;
        case LANG_GERMAN:
            LoadFile("opdat/optext_4.mes", MesBuffer, 0);
            break;
        case LANG_ITALIAN:
            LoadFile("opdat/optext_5.mes", MesBuffer, 0);
            break;
        case LANG_SPANISH:
            LoadFile("opdat/optext_6.mes", MesBuffer, 0);
            break;
    }

    Mes1.buff = (short *) MesBuffer;
    Mes1.text = (char *) MesBuffer;
    Mes1.text += *(short *) (MesBuffer + 2);
}

/**
 * Loads the assets required by the selected opening scene.
 *
 * @mangled LoadScene__Fv
 * @address 0x1DAF6C0
 * @size 0x11C
 * @unknownret
 */
static void LoadScene() {
    char *files[3][2] = {
        {"opdat/scene/0101acp.sne", "0101acp.cfg"},
        {"opdat/scene/0101bcp.sne", "0101bcp.cfg"},
        {"opdat/scene/0104cp.sne",  "0104cp.cfg" },
    };
    int i;

    for (i = 0; i < 3; i++) {
        LoadFile(files[i][0], (void *) read_buffer, 0);
        Cam__2[i].LoadPackData(read_buffer, files[i][1], &PassDataBuffer[i], 0);
        Cam__2[i].motion_type.state.time = 1.0f;
        Cam__2[i].motion_type.state.blend_step = 1.0f;
        Cam__2[i].motion_type.state.motion_no = 0;
        Cam__2[i].motion_type.state.playing_no = 0;
        Cam__2[i].motion_type.state.camera = &OP_MainCamera;
    }
}

void LoadSceneBG();

int OpeningLoop();

/**
 * Advances to the next opening scene or loading phase.
 *
 * @mangled SceneChange__Fv
 * @address 0x1DAFAA0
 * @size 0x1B0
 * @unknownret
 */
static void SceneChange() {
    switch (CScript__2.load_no) {
        case OP_SCENE_NORUNE:
            OpB_LoadDataBG();
            break;
        case OP_SCENE_TOAN_HOUSE:
            OpB_LoadDataBG2();
            break;
        case OP_SCENE_NORUNE_NIGHT:
            OpC_LoadDataBG();
            break;
        case OP_SCENE_DANCE:
            OpC_LoadDataBG2();
            break;
        case OP_SCENE_MAJIN:
            OpC_LoadDataBG3();
            break;
        case OP_SCENE_NORUNE_RUINED:
            OpC_LoadDataBG4();
            break;
        case OP_SCENE_NORUNE_BURNING:
            OpC_LoadDataBG5();
            break;
        case OP_SCENE_SEIREI_KING:
            OpD_LoadDataBG();
            break;
        case OP_SCENE_MEADOW:
            OpD_LoadDataBG2();
            break;
    }

    switch (CScript__2.init_no) {
        case OP_SCENE_NORUNE:
            SoundStop();
            OpB_InitProcess();
            break;
        case OP_SCENE_TOAN_HOUSE:
            OpB_InitProcess2();
            break;
        case OP_SCENE_NORUNE_NIGHT:
            SoundStop();
            OpC_InitProcess();
            break;
        case OP_SCENE_DANCE:
            OpC_InitProcess2();
            break;
        case OP_SCENE_MAJIN:
            SoundStop();
            OpC_InitProcess3();
            break;
        case OP_SCENE_NORUNE_RUINED:
            OpC_InitProcess4();
            break;
        case OP_SCENE_NORUNE_BURNING:
            OpC_InitProcess5();
            break;
        case OP_SCENE_SEIREI_KING:
            SoundStop();
            OpD_InitProcess();
            break;
        case OP_SCENE_MEADOW:
            SoundStop();
            OpD_InitProcess2();
            break;
    }
}

/**
 * Processes pause input and pause-screen state.
 *
 * @mangled PauseProcess__Fv
 * @address 0x1DAFC50
 * @size 0x280
 * @unknownret
 */
static void PauseProcess() {
    static int endflg = 0;

    if (DispFade.GetRate() != 0.0) {
        return;
    }

    if (!endflg) {
        if (End) {
            return;
        }

        if (Pause == 0) {
            if (!GamePad.Down(PAD_START)) {
                return;
            }

            CSnd.Stop(MIDI_PORT_BGM);

            if (CScript__2.scene) {
                CSnd.Stop(MIDI_PORT_AMBIENT);
            }

            CSnd.SetVol(MIDI_PORT_SE_TITLE, 0);
            CSnd.SetVol(MIDI_PORT_SE_DEFAULT, 0);
            CSnd.SetVol(MIDI_PORT_UNK_D, 0);
            CSnd.SetVol(MIDI_PORT_SE_SPECIAL, 0);
            Pause = 1;
            endflg = 0;
            PauseFrame = Cam__2[SceneNp__2].motion_type.state.time;
        } else if (Pause == 1) {
            if (GamePad.Down(PAD_CIRCLE)) {
                endflg = 1;
                CSnd.SetVol(MIDI_PORT_SE_TITLE, 256);
                CSnd.SetVol(MIDI_PORT_SE_DEFAULT, 256);
                CSnd.SetVol(MIDI_PORT_UNK_D, 256);
                CSnd.SetVol(MIDI_PORT_SE_SPECIAL, 256);

                if (CScript__2.scene) {
                    CSnd.SQ_RePlay(MIDI_PORT_AMBIENT);
                }

                if (OpBgmSqPort != -1) {
                    CSnd.SQ_RePlay(MIDI_PORT_BGM);
                }
            } else if (GamePad.Down(PAD_CROSS)) {
                while (ReadBGSync())
                    ;

                End = 1;
                DispFade.FadeOutStart(1.0f, 0);
            }
        }
    } else {
        endflg = 0;
        Pause = 0;
    }
}

/**
 * Stops the opening scene's active sounds.
 *
 * @mangled SoundStop__Fv
 * @address 0x1DAFED0
 * @size 0x40
 * @unknownret
 */
static void SoundStop() {
    SndStopAllSe();
    CSnd.Stop(MIDI_PORT_BGM);
    CSnd.StopVoice(0);
}

/**
 * Processes input while the movie is waiting for a key.
 *
 * @mangled WaitKeyProcess__Fv
 * @address 0x1DAFF10
 * @size 0x17C
 * @unknownret
 */
static void WaitKeyProcess() {
    static int flg = 0;
    static int cnt = 0;

    if (CScript__2.mes_wait) {
        if (!flg) {
            PauseFrame = Cam__2[SceneNp__2].motion_type.state.time;
            flg = 1;
        }

        if (Pause) {
            return;
        }

        if (GamePad.Down(PAD_CIRCLE) || GamePad.Down(PAD_CROSS)) {
            if (Mes1.State() == CLSMES_PAGE_WAIT) {
                Mes1.text_rate = 1.0f;
                Mes1.text_rate_set = 1.0f;
                Mes1.GoNextPage();
            } else if (Mes1.State() == CLSMES_SHOWN) {
                CScript__2.mes_wait = false;
                flg = 0;
            } else {
                Mes1.text_rate = 0;
                Mes1.text_rate_set = 0;
            }
        }
    } else {
        flg = 0;
        cnt = 0;
        Mes1.text_rate = 1.0f;
        Mes1.text_rate_set = 1.0f;
    }
}

/**
 * Dispatches motion processing for the active opening scene.
 *
 * @mangled MotionProcess__Fv
 * @address 0x1DB0090
 * @size 0x3C0
 * @unknownret
 */
static void MotionProcess() {
    // PAL runs at 50 frames a second, so its fade speeds are raised by a fifth.
    switch (CScript__2.fade) {
        case TSFADE_IN_BLACK:
            DispFade.FadeInStart(CScript__2.fade_speed, 0);
            CScript__2.fade = TSFADE_NONE;
            break;
        case TSFADE_OUT_BLACK:
            DispFade.FadeOutStart(CScript__2.fade_speed, 0);
            CScript__2.fade = TSFADE_NONE;
            break;
        case TSFADE_IN_WHITE:
            DispFade.FadeInStart(CScript__2.fade_speed, 1);
            CScript__2.fade = TSFADE_NONE;
            break;
        case TSFADE_OUT_WHITE:
            DispFade.FadeOutStart(CScript__2.fade_speed, 1);
            CScript__2.fade = TSFADE_NONE;
            break;
    }

    if (CScript__2.motion_req) {
        if (!ReadBGSync()) {
            Op_MotionInfo.start = CScript__2.motion_start;
            Op_MotionInfo.end = CScript__2.motion_end;
            Op_MotionInfo.speed = CScript__2.motion_step;
            CScript__2.motion_req = false;

            if (SceneSw == 1) {
                SceneNp__2++;

                if (SceneNp__2 > 2) {
                    SceneNp__2 = 0;
                }

                SceneFlg = SCENE_READ_START;
            }

            SceneSw = 1;
            Cam__2[SceneNp__2].motion_type.state.time = (float) CScript__2.motion_start;
        } else {
            while (ReadBGSync())
                ;
        }
    }

    if (Cam__2[SceneNp__2].motion_type.state.time > (float) (CScript__2.motion_end - 1)) {
        Cam__2[SceneNp__2].motion_type.state.time = (float) (CScript__2.motion_end - 1);
    }

    if (CameraMode == 0) {
        Cam__2[SceneNp__2].motion_type.state.camera = &OP_MainCamera;

        if (PauseFrame > (float) (CScript__2.motion_end - 1)) {
            PauseFrame = (float) (CScript__2.motion_end - 1);
        }

        if (Pause) {
            Cam__2[SceneNp__2].motion_type.state.time = PauseFrame - CScript__2.motion_step;
        }

        if (CScript__2.mes_wait == 1) {
            Cam__2[SceneNp__2].motion_type.state.time = PauseFrame - CScript__2.motion_step;
        }

        SetMotionEX(Cam__2[SceneNp__2].frame, &Cam__2[SceneNp__2].motion_type, &Op_MotionInfo, &Cam__2[SceneNp__2].motion_type.state, frame_info_cam);
        LoadSceneBG();
    }

    if (!Pause) {
        switch (CScript__2.scene) {
            case OP_SCENE_DUNGEON_SQUARE:
                OpA_MotionProcess();
                break;
            case OP_SCENE_NORUNE:
                OpB_MotionProcess();
                break;
            case OP_SCENE_TOAN_HOUSE:
                OpB_MotionProcess();
                break;
            case OP_SCENE_NORUNE_NIGHT:
                OpC_MotionProcess();
                break;
            case OP_SCENE_DANCE:
                OpC_MotionProcess();
                break;
            case OP_SCENE_MAJIN:
                OpC_MotionProcess();
                break;
            case OP_SCENE_NORUNE_RUINED:
                OpC_MotionProcess();
                break;
            case OP_SCENE_NORUNE_BURNING:
                OpC_MotionProcess();
                break;
            case OP_SCENE_SEIREI_KING:
                OpD_MotionProcess();
                break;
            case OP_SCENE_MEADOW:
                OpD_MotionProcess();
                break;
        }
    }
}

/**
 * Dispatches sound processing for the active opening scene.
 *
 * @mangled SoundProcess__Fv
 * @address 0x1DB0450
 * @size 0x330
 * @unknownret
 */
static void SoundProcess() {
    if (CScript__2.se_stop == 0) {
        if (CScript__2.se_voice != 0) {
            switch (CScript__2.se_kind) {
                case TSSE_EFFECT:
                    OpPlayVolSE(MIDI_PORT_SE_DEFAULT, CScript__2.se_no, CScript__2.se_voice, 1.0f);
                    break;
                case TSSE_SPECIAL:
                    OpPlayVolSE(MIDI_PORT_SE_SPECIAL, CScript__2.se_no, CScript__2.se_voice, 1.0f);
                    break;
            }

            CScript__2.se_voice = 0;
        }
    } else {
        switch (CScript__2.se_kind) {
            case TSSE_EFFECT:
                CSnd.SE_Stop(MIDI_PORT_SE_DEFAULT, CScript__2.se_no, CScript__2.se_voice, 0);
                break;
            case TSSE_SPECIAL:
                CSnd.SE_Stop(MIDI_PORT_SE_SPECIAL, CScript__2.se_no, CScript__2.se_voice, 0);
                break;
        }

        CScript__2.se_stop = false;
    }

    if (CScript__2.bgm_fade != 0) {
        switch (CScript__2.se_kind) {
            case TSSE_ALL:
                CSnd.Fade(MIDI_PORT_BGM, (float) CScript__2.bgm_fade / 2.0f, CScript__2.se_fade_time);
                CSnd.Fade(MIDI_PORT_AMBIENT, (float) CScript__2.bgm_fade, CScript__2.se_fade_time);
                CSnd.Fade(MIDI_PORT_UNK_2, (float) CScript__2.bgm_fade, CScript__2.se_fade_time);
                CSnd.Fade(MIDI_PORT_SE_TITLE, (float) CScript__2.bgm_fade, CScript__2.se_fade_time);
                CSnd.Fade(MIDI_PORT_SE_DEFAULT, (float) CScript__2.bgm_fade, CScript__2.se_fade_time);
                CSnd.Fade(MIDI_PORT_UNK_D, (float) CScript__2.bgm_fade, CScript__2.se_fade_time);
                CSnd.Fade(MIDI_PORT_SE_SPECIAL, (float) CScript__2.bgm_fade, CScript__2.se_fade_time);
                break;
        }

        CScript__2.bgm_fade = 0;
    }

    switch (CScript__2.scene) {
        case OP_SCENE_DUNGEON_SQUARE:
            OpA_SoundProcess();
            break;
        case OP_SCENE_NORUNE:
            OpB_SoundProcess();
            break;
        case OP_SCENE_TOAN_HOUSE:
            OpB_SoundProcess();
            break;
        case OP_SCENE_NORUNE_NIGHT:
            OpC_SoundProcess();
            break;
        case OP_SCENE_DANCE:
            OpC_SoundProcess();
            break;
        case OP_SCENE_MAJIN:
            OpC_SoundProcess();
            break;
        case OP_SCENE_NORUNE_RUINED:
            OpC_SoundProcess();
            break;
        case OP_SCENE_NORUNE_BURNING:
            OpC_SoundProcess();
            break;
        case OP_SCENE_SEIREI_KING:
            OpD_SoundProcess();
            break;
        case OP_SCENE_MEADOW:
            OpD_SoundProcess();
            break;
    }
}

/**
 * Dispatches drawing for the active opening scene.
 *
 * @mangled DrawProcess__Fv
 * @address 0x1DB0780
 * @size 0x35C
 * @unknownret
 */
static void DrawProcess() {
    sceVu0FVECTOR position;
    sceVu0FMATRIX camera;
    sceVu0FMATRIX view;
    sceVu0FMATRIX unit;
    sceGsTex0     tex0;

    OP_MainCamera.GetPos(position);
    SndSetCamera(&OP_MainCamera);
    OP_MainCamera.GetCameraMatrix(camera);

    if (CScript__2.scene == OP_SCENE_MAJIN || CScript__2.scene == OP_SCENE_SEIREI_KING) {
        OP_MainCamera.Step(1);
    }

    sceVu0UnitMatrix(unit);
    sceVu0MulMatrix(view, unit, camera);

    if (CScript__2.scene != OP_SCENE_NORUNE) {
        MGSetViewMatrix(view, position);
    } else {
        MGSetViewMatrix(view);
    }

    sceVif1PkCall(Vif1Packet, (u_long128 *) Vu_prog0f, 0);
    sceVif1PkTerminate(Vif1Packet);

    switch (CScript__2.scene) {
        case OP_SCENE_DUNGEON_SQUARE:
            OpA_DrawProcess();
            break;
        case OP_SCENE_NORUNE:
            OpB_DrawProcess();
            break;
        case OP_SCENE_TOAN_HOUSE:
            OpB_DrawProcess();
            break;
        case OP_SCENE_NORUNE_NIGHT:
            OpC_DrawProcess();
            break;
        case OP_SCENE_DANCE:
            OpC_DrawProcess();
            break;
        case OP_SCENE_MAJIN:
            OpC_DrawProcess();
            break;
        case OP_SCENE_NORUNE_RUINED:
            OpC_DrawProcess();
            break;
        case OP_SCENE_NORUNE_BURNING:
            OpC_DrawProcess();
            break;
        case OP_SCENE_SEIREI_KING:
            OpD_DrawProcess();
            break;
        case OP_SCENE_MEADOW:
            OpD_DrawProcess();
            break;
    }

    DrawMess();

    if (Pause == 1) {
        setbilinear(0);
        MGFillBox(CRect<int>(0, 0, 10240, SCREEN_HEIGHT * 8), 0, 0, 0, 64);
        TexManager.ReloadTexture(Vif1Packet, 19);

        switch (LanguageCode) {
            case LANG_JAPANESE:
                set2DSprite(GetVif1Packet(), TexManager.GetTexture("pause", -1), CRect<int>(256, 160, 128, 128), CRect<int>(0, 0, 128, 128), 128);
                break;
            case LANG_ENGLISH_US:
                set2DSprite(GetVif1Packet(), TexManager.GetTexture("pause_e", -1), CRect<int>(256, 160, 128, 128), CRect<int>(0, 0, 128, 128), 128);
                break;
            case LANG_ENGLISH_UK:
                set2DSprite(GetVif1Packet(), TexManager.GetTexture("pause_e", -1), CRect<int>(256, 160, 128, 128), CRect<int>(0, 0, 128, 128), 128);
                break;
            case LANG_FRENCH:
                set2DSprite(GetVif1Packet(), TexManager.GetTexture("pause_f", -1), CRect<int>(256, 160, 128, 128), CRect<int>(0, 0, 128, 128), 128);
                break;
            case LANG_GERMAN:
                set2DSprite(GetVif1Packet(), TexManager.GetTexture("pause_g", -1), CRect<int>(256, 160, 128, 128), CRect<int>(0, 0, 128, 128), 128);
                break;
            case LANG_ITALIAN:
                set2DSprite(GetVif1Packet(), TexManager.GetTexture("pause_i", -1), CRect<int>(256, 160, 128, 128), CRect<int>(0, 0, 128, 128), 128);
                break;
            case LANG_SPANISH:
                set2DSprite(GetVif1Packet(), TexManager.GetTexture("pause_s", -1), CRect<int>(256, 160, 128, 128), CRect<int>(0, 0, 128, 128), 128);
                break;
        }

        setbilinear(1);
    }

    MGGetFBuffBackTex(&tex0);

    CTexture texture;

    texture.tex0 = *(u_long *) &tex0;
    set2DSprite(Vif1Packet, &texture, CRect<int>(0, 0, 640, SCREEN_HEIGHT), CRect<int>(0, 0, 640, SCREEN_HALF_HEIGHT), 128, 128, 128, 40);
    DispFade.FadeIn(Vif1Packet);
    DispFade.FadeOut(Vif1Packet);
}

/**
 * Draws opening subtitles and pause messages.
 *
 * @mangled DrawMess__Fv
 * @address 0x1DB0AE0
 * @size 0x3CC
 * @unknownret
 */
static void DrawMess() {
    int offset;
    int center;

    if (CScript__2.scene > OP_SCENE_NORUNE_RUINED) {
        Mes1.auto_page = false;
    }

    if (CScript__2.mes_no == 0) {
        return;
    }

    TexManager.ReloadTexture(Vif1Packet, Mes1.tex_block);

    static int no = 0;

    if (CScript__2.mes_no != no) {
        no = CScript__2.mes_no;
        Mes1MakeFlg = 1;
    } else {
        if (!Pause) {
            if (CScript__2.mes_timer > CScript__2.motion_step) {
                CScript__2.mes_timer -= CScript__2.motion_step;
            } else {
                CScript__2.mes_no = 0;
            }
        }

        Mes1.end_mark = false;

        if (CScript__2.mes_wait) {
            CScript__2.mes_timer = 2.0f * CScript__2.motion_step;

            if (Mes1.State() == CLSMES_SHOWN || (Mes1.auto_page == 0 && Mes1.State() == CLSMES_PAGE_WAIT)) {
                static int cnt = 0;

                if (cnt < 16) {
                    Mes1.end_mark = true;
                } else {
                    Mes1.end_mark = false;
                }

                cnt++;

                if (cnt > 31) {
                    cnt = 0;
                }
            } else {
                Mes1.end_mark = false;
            }
        }
    }

    Mes1.auto_page_wait = CScript__2.mes_page_wait;
    Mes1.text_x = CScript__2.mes_x;
    Mes1.text_y = CScript__2.mes_y;

    if (CScript__2.mes_tail_x == 0) {
        Mes1.tail_length = 0;
        Mes1.grow_x = CScript__2.mes_x;
        Mes1.grow_y = CScript__2.mes_y;
    } else {
        Mes1.tail_length = 64;
        Mes1.tail_to_x = CScript__2.mes_tail_x;
        Mes1.tail_to_y = CScript__2.mes_tail_y;
        Mes1.grow_x = CScript__2.mes_x;
        Mes1.grow_y = CScript__2.mes_y;
    }

    center = Mes1.win_x + (Mes1.win_width >> 1);
    offset = (center - Mes1.tail_to_x) >> 2;
    Mes1.tail_x = center - offset;

    if (CScript__2.mes_tail_x != 0) {
        if (offset > 0) {
            Mes1.tail_length += offset / 3;
        } else {
            Mes1.tail_length -= offset / 3;
        }
    }

    if (Mes1.grow_y > Mes1.tail_to_y) {
        Mes1.tail_y = Mes1.win_y + 20;
    } else {
        Mes1.tail_y = Mes1.win_y + Mes1.win_height - 20;
    }

    Mes1.fade_speed = 0.1f;

    if (Mes1MakeFlg) {
        Mes1MakeFlg = Mes1.MakeMesWin(CScript__2.mes_no);
    }

    Mes1.Step();
    Mes1.AbsFukidashiIn();
    Mes1.DrawMesWin();
}

void OpBgmPlay();

void OpPlayVolPanSE(float *position, float near_dist, float far_dist, int group, int no, int voice) {
    float  volume;
    float  pan;
    short *se_info;
    short  base_volume;
    int    level;
    int    se_index;
    int    pan_level;

    SndGetVolPan(&volume, &pan, position, near_dist, far_dist);

    if (pan < -1.0f) {
        pan = -1.0f;
    }

    if (pan > 1.0f) {
        pan = 1.0f;
    }

    pan_level = (int) (63.0f * pan) + 64;
    se_index = CSnd.GetSeNo(no, voice);
    se_info = CSnd.GetSeInfTbl();
    base_volume = se_info[se_index * 2 + 1];
    level = (int) ((float) base_volume * volume);

    if (level < 0) {
        level = 0;
    }

    if (level > 127) {
        level = 127;
    }

    CSnd.SE_Play(group, no, voice, pan_level, 127, level, 0);
}

void OpSetVolPanSE(float *position, float near_dist, float far_dist, int group, int no, int voice) {
    float  volume;
    float  pan;
    short *se_info;
    short  base_volume;
    int    level;
    int    se_index;
    int    pan_level;

    SndGetVolPan(&volume, &pan, position, near_dist, far_dist);

    if (pan < -1.0f) {
        pan = -1.0f;
    }

    if (pan > 1.0f) {
        pan = 1.0f;
    }

    pan_level = (int) (63.0f * pan) + 64;
    CSnd.SE_SetPan(group, no, voice, pan_level, 0);
    se_index = CSnd.GetSeNo(no, voice);
    se_info = CSnd.GetSeInfTbl();
    base_volume = se_info[se_index * 2 + 1];
    level = (int) ((float) base_volume * volume);

    if (level < 0) {
        level = 0;
    }

    if (level > 127) {
        level = 127;
    }

    CSnd.SE_SetVol(group, no, voice, level, 0);
}

void OpPlayVolSE(int group, int no, int voice, float volume) {
    short *se_info;
    short  base_volume;
    int    se_index;
    int    level;

    se_index = CSnd.GetSeNo(no, voice);
    se_info = CSnd.GetSeInfTbl();
    base_volume = se_info[se_index * 2 + 1];
    level = (int) ((float) base_volume * volume);

    if (level < 0) {
        level = 0;
    }

    if (level > 127) {
        level = 127;
    }

    CSnd.SE_Play(group, no, voice, 64, 127, level, 0);
}

int OpGetVolSQ(int no) {
    switch (no) {
        case 0:
            return CSnd.GetMidiState()->port[0].sequence[0]->volume;
        case 1:
            return CSnd.GetMidiState()->port[0].sequence[0]->volume;
    }
    return 0;
}

void FadeCansel() {
    MIDI_STATE *state;

    state = CSnd.GetMidiState();
    state->port[0].fade[0].active = false;
    state->port[2].fade[0].active = false;
    state->port[1].fade[1].active = false;
    state->port[4].fade[1].active = false;
    state->port[5].fade[1].active = false;
    state->port[6].fade[1].active = false;
    CSnd.SetVol(MIDI_PORT_SE_TITLE, 256);
    CSnd.SetVol(MIDI_PORT_SE_DEFAULT, 256);
    CSnd.SetVol(MIDI_PORT_UNK_D, 256);
    CSnd.SetVol(MIDI_PORT_SE_SPECIAL, 256);
    CSnd.Step();
}

/* The classes the loader places in the world, declared here rather than reached through headers of
   their own because each is another unit's to type. Only the members this file touches are named;
   the extents are the sizes the executable gives the objects below. */

/* A frame parented to an object, which is what lets the world transform drive a model. */
class CObjectFrame : public CObject {
public:
    CFrameVu1 *frame[4];         /**< Frame of each level of detail; zero where the object has none. */
    s32        rotation_changed; /**< Set whenever the angle, its motion or the moment changes. */
    s32        draw_on;          /**< 1 while the object draws; 0 leaves it out of the scene. */

    virtual void FrameObjectOnOff(char *name, int on);
    virtual void Draw();

    void SetFrame(CFrameVu1 *frame, int level);
};

/* One piece of scenery. A map holds a table of them, hands each its model, and drives them through
   the object dispatch like anything else in the world. */
class CMapObject : public CObjectFrame {
public:
    CFrameVu1 *collision_frame; /**< Frame that collision reads; zero where the object has none. */
    CFrameVu1 *unk_D4;
    CFrameVu1 *shade_frame;  /**< Frame that the shade draws from; zero where the object takes none. */
    CFrameVu1 *camera_frame; /**< Collision frame the camera reads; zero where the object has none. */
    float      unk_E0;
    int        unk_34; /**< Category row of the map the object draws with. */
    int        handle; /**< Handle the map gave the object; below zero where the slot is free. */
    char       unk_EC[4];

    virtual void Draw();

    void Initialize();
    void DrawShadow(int fast);
};

/* One row of the table a map sorts its scenery by. The loader writes row one and no other, and
   what it writes there are the four distances a level-of-detail object changes model at. */
class CategoryAttr {
public:
    float lod[4];  /**< Distances at which the category changes level of detail. */
    int   lowest;  /**< Lowest level of detail that the category may draw. */
    int   highest; /**< Highest level of detail that the category may draw. */
};

/* A run of frames the world draws as one. */
class CMap {
public:
    CategoryAttr category[16]; /**< Level-of-detail ranges for the map's object categories. */
    char         unk_180[2416];

    CMapObject *SetObject(CFrameVu1 *frame, int category_no, int handle);
    CMapObject *SetObject(int no, CFrameVu1 *frame, int category_no, int handle);
    CMapObject *GetObject(int no);
};

/* One looping object animation a definition file registers: a frame is found by name and then
   driven between two motion numbers at a rate, with a scale of its own. */
class OBJ_ANIME_SEQ {
public:
    char          name[16]; /**< Name of the frame the animation drives. */
    int           type;     /**< Kind of animation the sequence plays. */
    int           number;   /**< Animation number selected within that kind. */
    char          unk_18[8];
    sceVu0FVECTOR start; /**< Value the animation starts from. */
    sceVu0FVECTOR unk_30;
    float         step_x; /**< Amount the first component advances each step. */
    float         step_y; /**< Amount the second component advances each step. */
    float         step_z; /**< Amount the third component advances each step. */
    char          unk_4C[60];

    void Initialize();
};

void InitObjAnime(CFrame *frame, OBJ_ANIME_SEQ *sequence);

/* How far the scene's fog reaches and what colour it is. The four rates are the near and far
   planes and the two densities the renderer takes; the editor's own pair below shadows them and is
   never handed to the renderer here. */
float  op_fogRate[4] = {1000.0f, 3500.0f, 0.0f, 255.0f};
u_char op_fogColor[3] = {96, 160, 239};

static int   debugModeFlag = 1;
static float run_speed = 1.0f;

/* The water plane a definition file may place. The four corners are built from one width and one
   depth, so the quad is always centred on the position and always axis-aligned; everything after
   them is what the simulation reads. */
static sceVu0FVECTOR WaterV1;
static sceVu0FVECTOR WaterV2;
static sceVu0FVECTOR WaterV3;
static sceVu0FVECTOR WaterV4;
static sceVu0FVECTOR WaterPos;
static u_char        WaterR;
static u_char        WaterG;
static u_char        WaterB;
static int           WaterFlag;
static int           WaterMeshW;
static int           WaterMeshH;
static float         WaterShake;
static float         WaterCourant;
static float         WaterDecline;
static float         WaterAmplitude;
static float         WaterRefraction;

/* The two lists the loader appends to as it goes: the shapes a scene is asked collision questions
   against, and the models that cast its shadows. */
static CFrameVu1 *ColModel[64];
static int        ColModelCount;
static CFrameVu1 *ShadowModel[64];
static int        shadowModelCount;

static int animeSpeed1;
static int animeSpeed2;

/* One command of the definition language: the number the loader knows it by, how many arguments it
   takes, and one type per argument. A type-0 argument is a quoted string, a type-1 one a number
   behind a comma and a type-2 one a number standing on its own. The rows are read as plain `int`
   because that is how the reader takes them. */
static int TEIGI_GRD_IMG[] = {0, 2, TSARG_STRING, TSARG_COMMA_VALUE};
static int TEIGI_BLD_IMG[] = {1, 2, TSARG_STRING, TSARG_COMMA_VALUE};
static int TEIGI_SKY_IMG[] = {2, 2, TSARG_STRING, TSARG_COMMA_VALUE};
static int TEIGI_FIRE_IMG[] = {31, 1, TSARG_STRING};
static int TEIGI_GRD[] = {3, 7, TSARG_STRING, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE};
static int TEIGI_BLD[] = {4, 8, TSARG_STRING, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE};
static int TEIGI_LOD[] = {5, 1, TSARG_STRING};
static int TEIGI_CRD[] = {6, 1, TSARG_STRING};
static int TEIGI_SKY[] = {7, 2, TSARG_STRING, TSARG_COMMA_VALUE};
static int TEIGI_FOG[] = {8, 7, TSARG_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE};
static int TEIGI_AMBIENT[] = {9, 3, TSARG_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE};
static int TEIGI_LIGHT_COL[] = {10, 7, TSARG_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE};
static int TEIGI_FARCLIP[] = {11, 1, TSARG_VALUE};
static int TEIGI_BG_COL2[] = {12, 3, TSARG_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE};
static int TEIGI_BG_COL[] = {12, 3, TSARG_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE};
static int TEIGI_NORMALCLIP_OFF[] = {13, 1, TSARG_VALUE};
static int TEIGI_RUN_SPEED[] = {14, 1, TSARG_VALUE};
static int TEIGI_EDIT_FOG[] = {16, 7, TSARG_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE};
static int TEIGI_WATER_SET[] = {17, 5, TSARG_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE};
static int TEIGI_WATER_RGB[] = {18, 3, TSARG_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE};
static int TEIGI_WATER_PARAM[] = {19, 7, TSARG_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE};
static int TEIGI_LEVEL_FAR[] = {21, 4, TSARG_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE};
static int TEIGI_DebugFlag[] = {22, 1, TSARG_VALUE};
static int TEIGI_AnimeSpeed[] = {23, 2, TSARG_VALUE, TSARG_COMMA_VALUE};
static int TEIGI_UPER[] = {24, 8, TSARG_STRING, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE};
static int TEIGI_UPR_IMG[] = {25, 2, TSARG_STRING, TSARG_COMMA_VALUE};
static int TEIGI_PLIGHT[] = {26, 9, TSARG_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE};
static int TEIGI_ADD_CRD[] = {27, 7, TSARG_STRING, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE};
static int TEIGI_DEF_PATS[] = {50, 0};
static int TEIGI_DEF_ENDS[] = {51, 0};
static int TEIGI_S_VOLUME[] = {28, 7, TSARG_STRING, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE};
static int TEIGI_PROJECTION[] = {29, 1, TSARG_VALUE};
static int TEIGI_OBJ_ROT[] = {30, 7, TSARG_STRING, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE};
static int TEIGI_FIRE[] = {32, 5, TSARG_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE};
static int TEIGI_MAPINFO[] = {80, 1, TSARG_STRING};
static int TEIGI_PT_BASE[] = {52, 2, TSARG_STRING, TSARG_COMMA_VALUE};
static int TEIGI_PT_COLS[] = {54, 1, TSARG_STRING};
static int TEIGI_MAPD[] = {53, 32, TSARG_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE};
static int TEIGI_PT_FIRE[] = {55, 3, TSARG_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE};
static int TEIGI_PT_WATER[] = {56, 9, TSARG_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE};

/* The projection the scene is drawn with, and the editor's own fog beside the renderer's. */
static float  Projection = 800.0f;
static u_char editFogColor[3] = {96, 160, 239};
static float  editFogRate[4] = {1000.0f, 3500.0f, 0.0f, 255.0f};

/* The four distances a level-of-detail object changes model at, which every such object in a scene
   shares because the loader copies them into the map's own row rather than into the object. */
static float levelOfDitialZ[4] = {200.0f, 400.0f, 800.0f, 1600.0f};

/* One of the ninety-six lights a definition file may place. The first word is what the file has
   asked for and everything after it is what it asked for it to be, which is why the parser clears
   only that word and the loader writes the rest. */
struct POINT_LIGHT {
    int    used; /**< Whether the definition file placed this light. */
    float  x;    /**< Horizontal world position. */
    float  y;    /**< Vertical world position. */
    float  z;    /**< Depth world position. */
    u_char r;    /**< Red component of the light's colour. */
    u_char g;    /**< Green component of the light's colour. */
    u_char b;    /**< Blue component of the light's colour. */
    char   unk_13[1];
    u_int  arg7; /**< Seventh argument of the PLIGHT command, truncated to an integer. */
    u_int  arg8; /**< Eighth argument of the PLIGHT command, truncated to an integer. */
    float  arg9; /**< Ninth argument of the PLIGHT command. */
};

static POINT_LIGHT pointLight[96];
static int         pointLightStack;

/* The parse state the two readers share. `argLevel` is the nesting the caller has reached, which
   is what gives each level of a definition its own row of the two argument buffers; the file size
   is read once by the loader and then bounds every walk over the text. */
static int argLevel;
static int teigiFileSize;

/* The definition file is read straight into this one off the disc, and the drive transfers by
   DMA into whole cache lines. */
static char  teigiBuff[4096] __attribute__((aligned(64)));
static char  argStrBuff[128][64];
static float argValBuff[128][64];

/* The four model names one level-of-detail object is built from, as the loader hands them to the
   loader below it: a null entry is a level the file left out. */
static char *LODNameBuff[4];

static int nowObjCnt;
static int nowObjCnt2;
static int nowPartsCnt;

static int skipSpace(char *buf, int pos);
static int checkArg(char *buf, int pos, int *command);

/* Read one definition file and turn it into rows of arguments the loader can walk. Every line
   ending becomes a pair of NULs first, so the rest of the file is one long run of tokens with no
   line structure left in it; each command the text names is then matched by its keyword, its
   arguments are checked into the buffers at the next level, and the level moves on. A line naming
   no command at all is fatal, because a definition file the loader half-understands would place
   half a scene. */
void OPAnalyz(char *name);

/* Walk the levels the reader left behind and do what each one asks. A level's number stands in
   slot 0 of its row and every command is tested against every level, so a file may repeat a
   command as often as it likes and the last one to run wins. The frame attribute the loader builds
   as it goes is what every model it places is given, which is why the two commands that place a
   whole building save it and put it back: what they set is theirs alone. */
void OPMdsLoad();

/**
 * The run of separators standing before a token. The loader turns every line ending into a pair of
 * NULs before anything reads the text, so a NUL is a separator here and a comment is the run up to
 * the next one; the ideographic space is two bytes, which is why the skip is a loop over the file
 * rather than a walk over one kind of byte.
 *
 * @mangled skipSpace__FPci
 * @address 0x1DB4830
 * @size 0x110
 * @unknownret
 */
static int skipSpace(char *buffer, int position) {
    int skipped;

    while (position < teigiFileSize) {
        skipped = 0;

        if (memcmp(&buffer[position], "\x81\x40", 2) == 0) {
            position++;
            skipped = 1;
        }

        if (buffer[position] == ' ') {
            skipped = 1;
        }

        if (buffer[position] == '\t') {
            skipped = 1;
        }

        if (buffer[position] == '\0') {
            position++;
            skipped = 1;
        }

        if (memcmp(&buffer[position], "//", 2) == 0) {
            while (buffer[position] != '\0') {
                position++;
            }

            position++;
            skipped = 1;
        }

        if (!skipped) {
            return position;
        }

        position++;
    }

    return teigiFileSize;
}

/**
 * One command's arguments, described by the row the caller passes: its own number, how many
 * arguments it takes, and one type per argument. A type-0 argument is a quoted string and lands in
 * the string buffer; the other two are numbers and land in the value buffer beside the command
 * number, and differ only in whether a comma has to stand in front. Anything the forms do not
 * cover hands back -1.
 *
 * @mangled checkArg__FPciPi
 * @address 0x1DB4940
 * @size 0x574
 * @unknownret
 */
static int checkArg(char *buffer, int position, int *command) {
    int i;
    int cursor;
    int char_count;
    int accepted;

    cursor = position;

    if (command[1] == 0) {
        return position;
    }

    for (i = 0; i < command[1]; i++) {
        argValBuff[argLevel][0] = (float) command[0];

        switch (command[2 + i]) {
            case TSARG_STRING:
                if (buffer[cursor] != '"') {
                    return -1;
                }

                cursor++;

                for (char_count = 0; char_count < 64; char_count++) {
                    if (buffer[cursor] == '"') {
                        argStrBuff[argLevel][char_count] = '\0';
                        cursor++;
                        break;
                    }

                    argStrBuff[argLevel][char_count] = buffer[cursor];
                    cursor++;
                }

                if (char_count == 64) {
                    return -1;
                }

                cursor = skipSpace(buffer, cursor);
                break;

            case TSARG_COMMA_VALUE:
                if (buffer[cursor] != ',') {
                    return -1;
                }

                cursor = skipSpace(buffer, cursor + 1);

                if (memcmp(&buffer[cursor], "ON", 2) == 0) {
                    argValBuff[argLevel][1 + i] = 1.0f;
                    cursor += 2;
                } else if (memcmp(&buffer[cursor], "OFF", 3) == 0) {
                    argValBuff[argLevel][1 + i] = 0;
                    cursor += 3;
                } else {
                    accepted = 0;

                    if (buffer[cursor] == '-') {
                        accepted = 1;
                    }

                    if (buffer[cursor] >= '0' && buffer[cursor] <= '9') {
                        accepted = 1;
                    }

                    if (!accepted) {
                        return -1;
                    }

                    argValBuff[argLevel][1 + i] = (float) atof(&buffer[cursor]);

                    for (char_count = 0; char_count < 32; char_count++) {
                        accepted = 0;

                        if (buffer[cursor] == '-') {
                            cursor++;
                            accepted = 1;
                        }

                        if (buffer[cursor] >= '0' && buffer[cursor] <= '9') {
                            cursor++;
                            accepted = 1;
                        }

                        if (buffer[cursor] == '.') {
                            cursor++;
                            accepted = 1;
                        }

                        if (!accepted) {
                            break;
                        }
                    }

                    if (char_count == 32) {
                        return -1;
                    }
                }

                cursor = skipSpace(buffer, cursor);
                break;

            case TSARG_VALUE:
                if (memcmp(&buffer[cursor], "ON", 2) == 0) {
                    argValBuff[argLevel][1 + i] = 1.0f;
                    cursor += 2;
                } else if (memcmp(&buffer[cursor], "OFF", 3) == 0) {
                    argValBuff[argLevel][1 + i] = 0;
                    cursor += 3;
                } else {
                    accepted = 0;

                    if (buffer[cursor] == '-') {
                        accepted = 1;
                    }

                    if (buffer[cursor] >= '0' && buffer[cursor] <= '9') {
                        accepted = 1;
                    }

                    if (!accepted) {
                        return -1;
                    }

                    argValBuff[argLevel][1 + i] = (float) atof(&buffer[cursor]);

                    for (char_count = 0; char_count < 32; char_count++) {
                        accepted = 0;

                        if (buffer[cursor] == '-') {
                            cursor++;
                            accepted = 1;
                        }

                        if (buffer[cursor] >= '0' && buffer[cursor] <= '9') {
                            cursor++;
                            accepted = 1;
                        }

                        if (buffer[cursor] == '.') {
                            cursor++;
                            accepted = 1;
                        }

                        if (!accepted) {
                            break;
                        }
                    }

                    if (char_count == 32) {
                        return -1;
                    }
                }

                cursor = skipSpace(buffer, cursor);
                break;
        }
    }

    return cursor;
}
