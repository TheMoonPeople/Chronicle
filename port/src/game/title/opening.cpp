#include "common.h"

#include <algorithm>
#include <bit>
#include <cstdlib>
#include <string_view>

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

/* The rectangle every 2D draw takes; its constructor assigns the edges in ascending order. */
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

CDataAlloc2<1> CharaDataBuffer__2[7];
CDataAlloc2<1> DummyBuffer(-1);
CDataAlloc2<1> PassDataBuffer[3];
CDataAlloc2<1> MapDataBuffer(-1);
CDataAlloc2<1> WaterBuffer__2(-1);
CDataAlloc2<1> testBuffer(-1);

CCharacter Chara__3[23];
char       CharaTex__2[23];

CCameraFollow OP_MainCamera(60.0f, 20.0f, 0.0f, 4.0f);

CCharacter  Cam__2[3];
MOTION_INFO Op_MotionInfo;

u_char *PassReadBuffer;
u_char *MesBuffer;
int     OpBgmSqPort;
int     SceneNp__2;
int     Pause;

namespace {

/**
 * Steps of the camera-pack read LoadSceneBG runs, as SceneFlg holds them.
 */
enum class SceneReadStep : int {
    Start = 0, /**< Start reading the next camera pack. */
    Wait = 1,  /**< Wait for the read, then unpack it. */
    Idle = 2,  /**< Nothing to do until the next pack is requested. */
};

ClsMes    Mes1;
CDispFade DispFade;

tagFRAME_INF *frame_info_cam;
int           CameraMode;
int           SceneRp;
int           SceneCnt;
SceneReadStep SceneFlg;
int           SceneSw;
float         PauseFrame;
u_char        End;
int           EndCnt;
int           BgmOff;
int           BgmVol;
int           BgmNo;

void SoundStop();
void DrawMess();

/**
 * Loads the opening movie's localized message resources.
 */
void LoadMessage() {
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
            LoadFile("opdat/optext_0.mes", MesBuffer, nullptr);
            break;
        case LANG_ENGLISH_US:
            LoadFile("opdat/optext_1.mes", MesBuffer, nullptr);
            break;
        case LANG_ENGLISH_UK:
            LoadFile("opdat/optext_2.mes", MesBuffer, nullptr);
            break;
        case LANG_FRENCH:
            LoadFile("opdat/optext_3.mes", MesBuffer, nullptr);
            break;
        case LANG_GERMAN:
            LoadFile("opdat/optext_4.mes", MesBuffer, nullptr);
            break;
        case LANG_ITALIAN:
            LoadFile("opdat/optext_5.mes", MesBuffer, nullptr);
            break;
        case LANG_SPANISH:
            LoadFile("opdat/optext_6.mes", MesBuffer, nullptr);
            break;
        default:
            break;
    }

    Mes1.buff = reinterpret_cast<short *>(MesBuffer);
    Mes1.text = reinterpret_cast<char *>(MesBuffer);
    Mes1.text += *reinterpret_cast<short *>(&MesBuffer[2]);
}

/**
 * Loads the assets required by the selected opening scene.
 */
void LoadScene() {
    char *files[3][2] = {
        {"opdat/scene/0101acp.sne", "0101acp.cfg"},
        {"opdat/scene/0101bcp.sne", "0101bcp.cfg"},
        {"opdat/scene/0104cp.sne",  "0104cp.cfg" },
    };

    for (int i = 0; i < 3; i++) {
        LoadFile(files[i][0], read_buffer, nullptr);
        Cam__2[i].LoadPackData(read_buffer, files[i][1], &PassDataBuffer[i], nullptr);
        Cam__2[i].motion_type.state.time = 1.0f;
        Cam__2[i].motion_type.state.blend_step = 1.0f;
        Cam__2[i].motion_type.state.motion_no = 0;
        Cam__2[i].motion_type.state.playing_no = 0;
        Cam__2[i].motion_type.state.camera = &OP_MainCamera;
    }
}

/**
 * Advances to the next opening scene or loading phase.
 */
void SceneChange() {
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
        default:
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
        default:
            break;
    }
}

/**
 * Processes pause input and pause-screen state.
 */
void PauseProcess() {
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
                while (ReadBGSync()) {
                }

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
 */
void SoundStop() {
    SndStopAllSe();
    CSnd.Stop(MIDI_PORT_BGM);
    CSnd.StopVoice(0);
}

/**
 * Processes input while the movie is waiting for a key.
 */
void WaitKeyProcess() {
    static int flg = 0;

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
        Mes1.text_rate = 1.0f;
        Mes1.text_rate_set = 1.0f;
    }
}

/**
 * Dispatches motion processing for the active opening scene.
 */
void MotionProcess() {
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
        default:
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

                SceneFlg = SceneReadStep::Start;
            }

            SceneSw = 1;
            Cam__2[SceneNp__2].motion_type.state.time = static_cast<float>(CScript__2.motion_start);
        } else {
            while (ReadBGSync()) {
            }
        }
    }

    Cam__2[SceneNp__2].motion_type.state.time = std::min(Cam__2[SceneNp__2].motion_type.state.time, static_cast<float>(CScript__2.motion_end - 1));

    if (CameraMode == 0) {
        Cam__2[SceneNp__2].motion_type.state.camera = &OP_MainCamera;

        PauseFrame = std::min(PauseFrame, static_cast<float>(CScript__2.motion_end - 1));

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
            case OP_SCENE_TOAN_HOUSE:
                OpB_MotionProcess();
                break;
            case OP_SCENE_NORUNE_NIGHT:
            case OP_SCENE_DANCE:
            case OP_SCENE_MAJIN:
            case OP_SCENE_NORUNE_RUINED:
            case OP_SCENE_NORUNE_BURNING:
                OpC_MotionProcess();
                break;
            case OP_SCENE_SEIREI_KING:
            case OP_SCENE_MEADOW:
                OpD_MotionProcess();
                break;
            default:
                break;
        }
    }
}

/**
 * Dispatches sound processing for the active opening scene.
 */
void SoundProcess() {
    if (CScript__2.se_stop == 0) {
        if (CScript__2.se_voice != 0) {
            switch (CScript__2.se_kind) {
                case TSSE_EFFECT:
                    OpPlayVolSE(MIDI_PORT_SE_DEFAULT, CScript__2.se_no, CScript__2.se_voice, 1.0f);
                    break;
                case TSSE_SPECIAL:
                    OpPlayVolSE(MIDI_PORT_SE_SPECIAL, CScript__2.se_no, CScript__2.se_voice, 1.0f);
                    break;
                default:
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
            default:
                break;
        }

        CScript__2.se_stop = false;
    }

    if (CScript__2.bgm_fade != 0) {
        if (CScript__2.se_kind == TSSE_ALL) {
            CSnd.Fade(MIDI_PORT_BGM, static_cast<float>(CScript__2.bgm_fade) / 2.0f, CScript__2.se_fade_time);
            CSnd.Fade(MIDI_PORT_AMBIENT, static_cast<float>(CScript__2.bgm_fade), CScript__2.se_fade_time);
            CSnd.Fade(MIDI_PORT_UNK_2, static_cast<float>(CScript__2.bgm_fade), CScript__2.se_fade_time);
            CSnd.Fade(MIDI_PORT_SE_TITLE, static_cast<float>(CScript__2.bgm_fade), CScript__2.se_fade_time);
            CSnd.Fade(MIDI_PORT_SE_DEFAULT, static_cast<float>(CScript__2.bgm_fade), CScript__2.se_fade_time);
            CSnd.Fade(MIDI_PORT_UNK_D, static_cast<float>(CScript__2.bgm_fade), CScript__2.se_fade_time);
            CSnd.Fade(MIDI_PORT_SE_SPECIAL, static_cast<float>(CScript__2.bgm_fade), CScript__2.se_fade_time);
        }

        CScript__2.bgm_fade = 0;
    }

    switch (CScript__2.scene) {
        case OP_SCENE_DUNGEON_SQUARE:
            OpA_SoundProcess();
            break;
        case OP_SCENE_NORUNE:
        case OP_SCENE_TOAN_HOUSE:
            OpB_SoundProcess();
            break;
        case OP_SCENE_NORUNE_NIGHT:
        case OP_SCENE_DANCE:
        case OP_SCENE_MAJIN:
        case OP_SCENE_NORUNE_RUINED:
        case OP_SCENE_NORUNE_BURNING:
            OpC_SoundProcess();
            break;
        case OP_SCENE_SEIREI_KING:
        case OP_SCENE_MEADOW:
            OpD_SoundProcess();
            break;
        default:
            break;
    }
}

/**
 * Dispatches drawing for the active opening scene.
 */
void DrawProcess() {
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

    sceVif1PkCall(Vif1Packet, reinterpret_cast<u_long128 *>(Vu_prog0f), 0);
    sceVif1PkTerminate(Vif1Packet);

    switch (CScript__2.scene) {
        case OP_SCENE_DUNGEON_SQUARE:
            OpA_DrawProcess();
            break;
        case OP_SCENE_NORUNE:
        case OP_SCENE_TOAN_HOUSE:
            OpB_DrawProcess();
            break;
        case OP_SCENE_NORUNE_NIGHT:
        case OP_SCENE_DANCE:
        case OP_SCENE_MAJIN:
        case OP_SCENE_NORUNE_RUINED:
        case OP_SCENE_NORUNE_BURNING:
            OpC_DrawProcess();
            break;
        case OP_SCENE_SEIREI_KING:
        case OP_SCENE_MEADOW:
            OpD_DrawProcess();
            break;
        default:
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
            default:
                break;
        }

        setbilinear(1);
    }

    MGGetFBuffBackTex(&tex0);

    CTexture texture;

    texture.tex0 = std::bit_cast<u_long>(tex0);
    set2DSprite(Vif1Packet, &texture, CRect<int>(0, 0, 640, SCREEN_HEIGHT), CRect<int>(0, 0, 640, SCREEN_HALF_HEIGHT), 128, 128, 128, 40);
    DispFade.FadeIn(Vif1Packet);
    DispFade.FadeOut(Vif1Packet);
}

/**
 * Draws opening subtitles and pause messages.
 */
void DrawMess() {
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

                Mes1.end_mark = cnt < 16;

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

    const int center = Mes1.win_x + (Mes1.win_width >> 1);
    const int offset = (center - Mes1.tail_to_x) >> 2;
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

} // namespace

void OpPlayVolPanSE(float *position, float near_dist, float far_dist, int group, int no, int voice) {
    float volume = 0.0f;
    float pan = 0.0f;

    SndGetVolPan(&volume, &pan, position, near_dist, far_dist);
    pan = std::clamp(pan, -1.0f, 1.0f);

    const int    pan_level = static_cast<int>(63.0f * pan) + 64;
    const int    se_index = CSnd.GetSeNo(no, voice);
    const short *se_info = CSnd.GetSeInfTbl();
    const short  base_volume = se_info[(se_index * 2) + 1];
    const int    level = std::clamp(static_cast<int>(static_cast<float>(base_volume) * volume), 0, 127);

    CSnd.SE_Play(group, no, voice, pan_level, 127, level, 0);
}

void OpSetVolPanSE(float *position, float near_dist, float far_dist, int group, int no, int voice) {
    float volume = 0.0f;
    float pan = 0.0f;

    SndGetVolPan(&volume, &pan, position, near_dist, far_dist);
    pan = std::clamp(pan, -1.0f, 1.0f);

    const int pan_level = static_cast<int>(63.0f * pan) + 64;
    CSnd.SE_SetPan(group, no, voice, pan_level, 0);

    const int    se_index = CSnd.GetSeNo(no, voice);
    const short *se_info = CSnd.GetSeInfTbl();
    const short  base_volume = se_info[(se_index * 2) + 1];
    const int    level = std::clamp(static_cast<int>(static_cast<float>(base_volume) * volume), 0, 127);

    CSnd.SE_SetVol(group, no, voice, level, 0);
}

void OpPlayVolSE(int group, int no, int voice, float volume) {
    const int    se_index = CSnd.GetSeNo(no, voice);
    const short *se_info = CSnd.GetSeInfTbl();
    const short  base_volume = se_info[(se_index * 2) + 1];
    const int    level = std::clamp(static_cast<int>(static_cast<float>(base_volume) * volume), 0, 127);

    CSnd.SE_Play(group, no, voice, 64, 127, level, 0);
}

int OpGetVolSQ(int no) {
    switch (no) {
        case 0:
        case 1:
            return CSnd.GetMidiState()->port[0].sequence[0]->volume;
        default:
            break;
    }
    return 0;
}

void FadeCansel() {
    MIDI_STATE *state = CSnd.GetMidiState();
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

/* The parts of other units' classes the definition loader places in the world, sized as the
   executable allocates them. */

/* A frame parented to an object, so the world transform drives a model. */
class CObjectFrame : public CObject {
public:
    CFrameVu1 *frame[4];         /**< Frame of each level of detail; zero where the object has none. */
    s32        rotation_changed; /**< Set whenever the angle, its motion or the moment changes. */
    s32        draw_on;          /**< 1 while the object draws; 0 leaves it out of the scene. */

    virtual void FrameObjectOnOff(char *name, int on);
    virtual void Draw();

    void SetFrame(CFrameVu1 *frame, int level);
};

/* One piece of scenery a map holds and drives through the object dispatch. */
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

/* One row of the level-of-detail table a map sorts its scenery by. */
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

/* One looping object animation a definition file registers against a named frame. */
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

/* Near and far fog planes and the two densities the renderer takes. */
float  op_fogRate[4] = {1000.0f, 3500.0f, 0.0f, 255.0f};
u_char op_fogColor[3] = {96, 160, 239};

namespace {

int   debugModeFlag = 1;
float run_speed = 1.0f;

/* Water plane a definition file may place, centred on WaterPos and axis-aligned. */
sceVu0FVECTOR WaterV1;
sceVu0FVECTOR WaterV2;
sceVu0FVECTOR WaterV3;
sceVu0FVECTOR WaterV4;
sceVu0FVECTOR WaterPos;
u_char        WaterR;
u_char        WaterG;
u_char        WaterB;
int           WaterFlag;
int           WaterMeshW;
int           WaterMeshH;
float         WaterShake;
float         WaterCourant;
float         WaterDecline;
float         WaterAmplitude;
float         WaterRefraction;

CFrameVu1 *ColModel[64];
int        ColModelCount;
CFrameVu1 *ShadowModel[64];
int        shadowModelCount;

int animeSpeed1;
int animeSpeed2;

/* Commands of the definition language: the command number, the argument count, then one
   TSARG_* type per argument. */
int TEIGI_GRD_IMG[] = {0, 2, TSARG_STRING, TSARG_COMMA_VALUE};
int TEIGI_BLD_IMG[] = {1, 2, TSARG_STRING, TSARG_COMMA_VALUE};
int TEIGI_SKY_IMG[] = {2, 2, TSARG_STRING, TSARG_COMMA_VALUE};
int TEIGI_FIRE_IMG[] = {31, 1, TSARG_STRING};
int TEIGI_GRD[] = {3, 7, TSARG_STRING, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE};
int TEIGI_BLD[] = {4, 8, TSARG_STRING, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE};
int TEIGI_LOD[] = {5, 1, TSARG_STRING};
int TEIGI_CRD[] = {6, 1, TSARG_STRING};
int TEIGI_SKY[] = {7, 2, TSARG_STRING, TSARG_COMMA_VALUE};
int TEIGI_FOG[] = {8, 7, TSARG_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE};
int TEIGI_AMBIENT[] = {9, 3, TSARG_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE};
int TEIGI_LIGHT_COL[] = {10, 7, TSARG_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE};
int TEIGI_FARCLIP[] = {11, 1, TSARG_VALUE};
int TEIGI_BG_COL2[] = {12, 3, TSARG_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE};
int TEIGI_BG_COL[] = {12, 3, TSARG_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE};
int TEIGI_NORMALCLIP_OFF[] = {13, 1, TSARG_VALUE};
int TEIGI_RUN_SPEED[] = {14, 1, TSARG_VALUE};
int TEIGI_EDIT_FOG[] = {16, 7, TSARG_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE};
int TEIGI_WATER_SET[] = {17, 5, TSARG_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE};
int TEIGI_WATER_RGB[] = {18, 3, TSARG_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE};
int TEIGI_WATER_PARAM[] = {19, 7, TSARG_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE};
int TEIGI_LEVEL_FAR[] = {21, 4, TSARG_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE};
int TEIGI_DebugFlag[] = {22, 1, TSARG_VALUE};
int TEIGI_AnimeSpeed[] = {23, 2, TSARG_VALUE, TSARG_COMMA_VALUE};
int TEIGI_UPER[] = {24, 8, TSARG_STRING, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE};
int TEIGI_UPR_IMG[] = {25, 2, TSARG_STRING, TSARG_COMMA_VALUE};
int TEIGI_PLIGHT[] = {26, 9, TSARG_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE};
int TEIGI_ADD_CRD[] = {27, 7, TSARG_STRING, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE};
int TEIGI_DEF_PATS[] = {50, 0};
int TEIGI_DEF_ENDS[] = {51, 0};
int TEIGI_S_VOLUME[] = {28, 7, TSARG_STRING, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE};
int TEIGI_PROJECTION[] = {29, 1, TSARG_VALUE};
int TEIGI_OBJ_ROT[] = {30, 7, TSARG_STRING, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE};
int TEIGI_FIRE[] = {32, 5, TSARG_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE};
int TEIGI_MAPINFO[] = {80, 1, TSARG_STRING};
int TEIGI_PT_BASE[] = {52, 2, TSARG_STRING, TSARG_COMMA_VALUE};
int TEIGI_PT_COLS[] = {54, 1, TSARG_STRING};
int TEIGI_MAPD[] = {53, 32, TSARG_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE};
int TEIGI_PT_FIRE[] = {55, 3, TSARG_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE};
int TEIGI_PT_WATER[] = {56, 9, TSARG_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE};

float  Projection = 800.0f;
u_char editFogColor[3] = {96, 160, 239};
float  editFogRate[4] = {1000.0f, 3500.0f, 0.0f, 255.0f};

/* Distances at which every level-of-detail object in a scene changes model. */
float levelOfDitialZ[4] = {200.0f, 400.0f, 800.0f, 1600.0f};

/* One of the point lights a definition file may place. */
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

POINT_LIGHT pointLight[96];
int         pointLightStack;

/* Row of the argument buffers the parser is filling; one row per parsed command. */
int argLevel;
int teigiFileSize;

/* The disc drive transfers by DMA into whole cache lines. */
char  teigiBuff[4096] __attribute__((aligned(64)));
char  argStrBuff[128][64];
float argValBuff[128][64];

/* Model names of one level-of-detail object; null where the file leaves a level out. */
char *LODNameBuff[4];

int nowObjCnt;
int nowObjCnt2;
int nowPartsCnt;

bool MatchesAt(const char *buffer, int position, std::string_view token) {
    return std::string_view(&buffer[position], token.size()) == token;
}

/**
 * Skips the separators before the next token. Line endings are already NULs, so a NUL separates
 * and a `//` comment runs to the next NUL; the ideographic space is a two-byte separator.
 */
int skipSpace(char *buffer, int position) {
    while (position < teigiFileSize) {
        bool skipped = false;

        if (MatchesAt(buffer, position, "\x81\x40")) {
            position++;
            skipped = true;
        }

        if (buffer[position] == ' ') {
            skipped = true;
        }

        if (buffer[position] == '\t') {
            skipped = true;
        }

        if (buffer[position] == '\0') {
            position++;
            skipped = true;
        }

        if (MatchesAt(buffer, position, "//")) {
            while (buffer[position] != '\0') {
                position++;
            }

            position++;
            skipped = true;
        }

        if (!skipped) {
            return position;
        }

        position++;
    }

    return teigiFileSize;
}

/**
 * Reads one numeric argument (ON, OFF or a decimal number) into the value buffer slot.
 * Returns the position after it, or -1 when the text is not a number.
 */
int ReadValueArg(char *buffer, int cursor, float &value) {
    if (MatchesAt(buffer, cursor, "ON")) {
        value = 1.0f;
        return cursor + 2;
    }

    if (MatchesAt(buffer, cursor, "OFF")) {
        value = 0;
        return cursor + 3;
    }

    if (buffer[cursor] != '-' && (buffer[cursor] < '0' || buffer[cursor] > '9')) {
        return -1;
    }

    value = static_cast<float>(std::atof(&buffer[cursor]));

    for (int char_count = 0; char_count < 32; char_count++) {
        bool accepted = false;

        if (buffer[cursor] == '-') {
            cursor++;
            accepted = true;
        }

        if (buffer[cursor] >= '0' && buffer[cursor] <= '9') {
            cursor++;
            accepted = true;
        }

        if (buffer[cursor] == '.') {
            cursor++;
            accepted = true;
        }

        if (!accepted) {
            return cursor;
        }
    }

    return -1;
}

/**
 * Reads one command's arguments as its row describes them into the argument buffers at argLevel.
 * Returns the position after them, or -1 when the text does not match the row.
 */
int checkArg(char *buffer, int position, int *command) {
    int cursor = position;

    if (command[1] == 0) {
        return position;
    }

    for (int i = 0; i < command[1]; i++) {
        argValBuff[argLevel][0] = static_cast<float>(command[0]);

        switch (command[2 + i]) {
            case TSARG_STRING: {
                if (buffer[cursor] != '"') {
                    return -1;
                }

                cursor++;

                int char_count = 0;

                for (; char_count < 64; char_count++) {
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
            }

            case TSARG_COMMA_VALUE:
                if (buffer[cursor] != ',') {
                    return -1;
                }

                cursor = skipSpace(buffer, cursor + 1);
                [[fallthrough]];

            case TSARG_VALUE:
                cursor = ReadValueArg(buffer, cursor, argValBuff[argLevel][1 + i]);

                if (cursor < 0) {
                    return -1;
                }

                cursor = skipSpace(buffer, cursor);
                break;

            default:
                break;
        }
    }

    return cursor;
}

} // namespace
