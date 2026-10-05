
#include "common.h"

#include <libgraph.h>
#include <libpkt.h>
#include <libvu0.h>

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "camera.hpp"
#include "camerafollow.hpp"
#include "character.hpp"
#include "dataalloc.hpp"
#include "dataread.hpp"
#include "fireomni.hpp"
#include "frame.hpp"
#include "framevu1.hpp"
#include "gamepad.hpp"
#include "main.hpp"
#include "mathutil.hpp"
#include "mds.hpp"
#include "mglib.hpp"
#include "objanime.hpp"
#include "object.hpp"
#include "runeffect.hpp"
#include "snd.hpp"
#include "sound.hpp"
#include "texture.hpp"
#include "title/dispfade.hpp"
#include "title/op_a.hpp"
#include "title/op_b.hpp"
#include "title/opening.hpp"
#include "title/rushmovi.hpp"
#include "title/script.hpp"
#include "vutext.hpp"
#include "water.hpp"
#include "wind.hpp"

// The bodies are retail's, which pass string literals as char *.
#pragma clang diagnostic ignored "-Wwritable-strings"

// Retail's RushInit, RushLoop and the static processes they share state with. The VU1 program upload
// in DrawProcess is gone. The unit's FaceChange is FaceChangeMovie here, the name op_d calls it by
// and the PS2 build binds to it (ps2/config/pal/object_fixups.json).

/* The overlay's own rectangle. Its constructor assigns x, y, w, h in that order, where
   rect.h's assigns them in the other; the same split title.cpp and opening.cpp carry. */
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
};

/* One piece of scenery the third scene lays out: the model file, where it stands and how far it is
   turned about the vertical axis, in degrees. */
struct MAP_INFO {
    char *name;        /**< Model file of the piece. */
    float position[3]; /**< Where the piece stands. */
    float rotation;    /**< Turn about the vertical axis, in degrees. */
};

void wait_now_loading_vsync();
void InitializeDataBuffer();
void MGMoveImage(sceGsTex0 *src, const CRect<int> &rect, sceGsTex0 *dst, int dsax, int dsay, int dir);
void set2DSprite(sceVif1Packet *packet, CTexture *texture, const CRect<int> &dst, const CRect<int> &src, u_char alpha);
void set2DSprite(sceVif1Packet *packet, CTexture *texture, const CRect<int> &dst, const CRect<int> &src, u_char r, u_char g, u_char b, u_char a);

void MoveImageTest(sceVif1Packet *packet, int sbp, int sbw, int spsm, const CRect<int> &rect, int dbp, int dbw, int dpsm, int dsax, int dsay, int dir);

extern CCameraFollow   MainCamera__3;
static CDispFade      DispFade;
CFireOmni             CFire__4;
extern class CScript  CScript;
extern CWind          Wind__4;
CWater                Water__2;
extern char           CharaTex[9];
extern CDataAlloc2<1> CharaDataBuffer;
static tagFRAME_INF   frame_info_cam[300];
extern CCharacter     Cam[4];
static MOTION_INFO    MotionInfo;
extern CDataAlloc2<1> PathDataBuffer;
extern CDataAlloc2<1> WaterBuffer;
static CDataAlloc2<1> DummyDataBuffer(-1);
extern CTexAnimeData  TexAnimeDataMovie[30];
extern CRunEffect     CRunFx;
extern CFrame        *OP_CharaFrame;

static u_char bEnd;
static int    EndCnt;
static int    CameraMode;
extern int    SceneNp;
extern float  TitleAngle;
static int    StartDisp;
extern int    TitleFade;
extern int    TitleFadeCnt;
extern int    StartLightning;
extern float  atraGetStatusRate;

/* One actor's face, as this scene animates it. The eyes and the mouth are two strips of frames
   stacked bottom-up in one texture, and a tick copies the current frame of each over the model's
   face plate. This scene's eye strip is three columns of ten rather than one column, which is what
   the eye number is folded into two coordinates for below. */
struct FACE_INFO {
    char *plate;        /**< Texture of the face plate the frames are copied over. */
    char *strip;        /**< Texture holding the eye and mouth frame strips. */
    int   eye_bottom;   /**< Distance of the eye area above the plate's bottom edge. */
    int   eye_height;   /**< Height of one eye frame. */
    int   mouth_bottom; /**< Distance of the mouth area above the plate's bottom edge. */
    int   mouth_height; /**< Height of one mouth frame. */
    int   eye;          /**< Eye frame currently shown. */
    int   mouth;        /**< Mouth frame currently shown. */
    int   strip_bottom; /**< Bottom row of the frame strips in the strip texture. */
    int   unk_24;
    int   unk_28;
};

static void MotionProcess();
static void DrawProcess();
static void SoundProcess();
// title.cpp defines these; retail's unit declared them static and MWCC bound the calls to title.cpp
// all the same, where clang leaves internal references nothing defines.
void DataLoad();
void DrawProcA();
void DrawProcB();
void DrawProcC();
void DrawProcD();
void DrawProcE();
void DrawProcF();
void DrawProcG();
void DrawProcH();
void DrawProcI();
void DrawProcTitle();

/* One actor's blinking and speaking. The eyes and the mouth are two strips of frames in one
   texture, and a tick copies the current frame of each over the actor's face plate. This scene's
   eye strip is three columns of ten frames rather than one column, so the eye number the script
   holds picks the column as well as the row. */
void FaceChangeMovie(int actor_no) {
    static FACE_INFO face = {"c01d01", "c01d01an_4", 27, 48, 78, 44, 0, 0, 512, 3, 0};
    CTexture        *plate;
    CTexture        *strip;
    int              sbp;
    int              dbp;
    int              sbw;
    int              dbw;

    if (face.plate == 0) {
        return;
    }

    plate = TexManager.GetTexture(face.plate, -1);
    strip = TexManager.GetTexture(face.strip, -1);

    if (plate == 0 || strip == 0) {
        return;
    }

    sbp = strip->tex0 & 0x3fff;
    dbp = plate->tex0 & 0x3fff;
    sbw = (strip->tex0 >> 14) & 0x3f;
    dbw = (plate->tex0 >> 14) & 0x3f;

    face.eye = CScript__2.obj[actor_no].eye;

    int eye = face.eye;
    int column;

    if (eye < 10) {
        column = 0;
    } else if (eye < 18) {
        column = 128;
        eye -= 10;
    } else {
        column = 256;
        eye -= 18;
    }

    CRect<int> eyes(column, face.strip_bottom - face.eye_height * (eye + 1), 128, face.eye_height);

    MoveImageTest(Vif1Packet, sbp, sbw, SCE_GS_PSMT8, eyes, dbp, dbw, SCE_GS_PSMT8, 0, 128 - face.eye_height - face.eye_bottom, 0);

    face.mouth = CScript__2.obj[actor_no].mouth;

    CRect<int> mouth(384, face.strip_bottom - face.mouth_height * (face.mouth + 1), 128, face.mouth_height);

    MoveImageTest(Vif1Packet, sbp, sbw, SCE_GS_PSMT8, mouth, dbp, dbw, SCE_GS_PSMT8, 0, 128 - face.mouth_height - face.mouth_bottom, 0);

}

void RushInit() {
    wait_now_loading_vsync();
    InitializeDataBuffer();
    SetDataBuffer(&CharaDataBuffer, 490000);
    SetDataBuffer(&PathDataBuffer, 30000);
    SetDataBuffer(&MapDataBuffer, 260000);
    SetDataBuffer(&WaterBuffer, 30000);
    SetDataBuffer(&DummyDataBuffer, 10000);
    SetDataBuffer(&TextureData, 250000);
    SetPacketReadBuffer(50000, 360000);

    MainCamera__3.SetDistance(80.0f);
    MainCamera__3.SetHeight(0.0f);
    MainCamera__3.SetFollow(0.0f, 0.0f, 0.0f);
    MainCamera__3.Step(0);
    MGSetRenderInfo(800.0f, 6.0f, 65535.0f);

    wait_now_loading_vsync();
    SndInitialize(4, 30, 4, 5);
    SndSetReadBuffer(read_buffer);
    SndSoundLoad(309);
    SndAmbientPlay(0);
    SndVoiceLoad(0);
    SndBgmInit();
    SndBgmLoad(25);
    wait_now_loading_vsync();

    StartReadBG();
    CScript.load_no = RUSH_SCENE_A;
    CScript.init_no = RUSH_SCENE_A;
    DataLoad();
    wait_now_loading_vsync();
    CScript.Load("rmdat/rmdat.scr");

    DispFade.FadeInit(128.0f);
    DispFade.FadeOutStart(128.0f, 0);
    bEnd = 0;
    EndCnt = 0;
    CameraMode = 1;
    TitleAngle = 1.57f;
    StartDisp = 1;
    TitleFade = 0;
    TitleFadeCnt = 0;
    atraGetStatusRate = 0;
    StartLightning = 0;
}

int RushLoop() {
    ReadBG();
    CScript.Step();
    DataLoad();
    MotionProcess();
    DrawProcess();
    DispFade.FadeIn(Vif1Packet);
    DispFade.FadeOut(Vif1Packet);
    SoundProcess();

    if (!bEnd) {
        if (GamePad.Down(PAD_START)) {
            while (ReadBGSync())
                ;

            DispFade.FadeOutStart(8.0f, 0);
            bEnd = 1;
            SndBgmFadeOut(32, 0);
        }
    }

    if (bEnd) {
        if (EndCnt < 128) {
            EndCnt += 8;
        } else {
            while (ReadBGSync())
                ;

            float black = 0.0f;
            MGSetBGColor(black, black, black, 128.0f);
            SndStopAllSe();
            SndBgmStop();
            SndAmbientStop();
            SndStep();
            return 1;
        }
    }

    if (CScript.end) {
        while (ReadBGSync())
            ;

        float black = 0.0f;
        MGSetBGColor(black, black, black, 128.0f);
        SndStopAllSe();
        SndBgmStop();
        SndAmbientStop();
        SndStep();
        return 1;
    }

    SndStep();
    return 0;
}

/**
 *
 *
 * @mangled MotionProcess__Fv__2
 * @address 0x1DC90A0
 * @size 0xDD0
 * @unknownret
 */
static void MotionProcess() {
    switch (CScript.fade) {
        case TSFADE_IN_BLACK:
            DispFade.FadeInStart(CScript.fade_speed, 0);
            CScript.fade = TSFADE_NONE;
            break;
        case TSFADE_OUT_BLACK:
            DispFade.FadeOutStart(CScript.fade_speed, 0);
            CScript.fade = TSFADE_NONE;
            break;
        case TSFADE_IN_WHITE:
            DispFade.FadeInStart(CScript.fade_speed, 1);
            CScript.fade = TSFADE_NONE;
            break;
        case TSFADE_OUT_WHITE:
            DispFade.FadeOutStart(CScript.fade_speed, 1);
            CScript.fade = TSFADE_NONE;
            break;
    }

    if (DispFade.GetRate() == 128.0) {
        return;
    }

    if (CScript.scene == RUSH_SCENE_TITLE) {
        return;
    }

    if (CScript.motion_req) {
        MotionInfo.start = CScript.motion_start;
        MotionInfo.end = CScript.motion_end;
        MotionInfo.speed = CScript.motion_step;

        if (CScript.camera_no != CScript.camera_start) {
            SceneNp++;
        }

        Cam[SceneNp].motion_type.state.time = (float) CScript.motion_start;
        Cam[SceneNp].motion_type.state.camera = &MainCamera__3;
        CScript.motion_req = false;
    }

    if (Cam[SceneNp].motion_type.state.time > (float) (CScript.motion_end - 1)) {
        Cam[SceneNp].motion_type.state.time = (float) (CScript.motion_end - 1);
    }

    for (int i = 0; i < OP_AnimeSeqRot - 1; i++) {
        ObjAnimePlay(&OP_AnimeSeq[i]);
    }

    for (int i = 0; i < 9; i++) {
        if (CScript.obj[i].disp) {
            if (CScript.obj[i].motion_end != -1) {
                if (Chara__3[i].motion_type.state.time > (float) (Chara__3[i].motion_type.motion_info[CScript.obj[i].motion].end - 1)) {
                    CScript.obj[i].motion = CScript.obj[i].motion_end;
                    CScript.obj[i].motion_end = -1;
                }
            }

            Chara__3[i].motion_type.state.blend_step = CScript.obj[i].step;

            if (CScript.obj[i].step == 1.0f) {
                if (CScript.obj[i].motion != Chara__3[i].motion_no) {
                    Chara__3[i].motion_type.state.time = (float) Chara__3[i].motion_type.motion_info[CScript.obj[i].motion].start;
                    Chara__3[i].motion_no = CScript.obj[i].motion;
                    Chara__3[i].motion_flags = 4;
                    Chara__3[i].motion_speed = -1.0f;
                } else {
                    Chara__3[i].motion_no = CScript.obj[i].motion;
                    Chara__3[i].motion_flags = 0;
                    Chara__3[i].motion_speed = -1.0f;
                }
            } else {
                Chara__3[i].motion_no = CScript.obj[i].motion;
                Chara__3[i].motion_flags = 0;
                Chara__3[i].motion_speed = -1.0f;
            }
        }
    }

    SetMotionEX(Cam[SceneNp].frame, &Cam[SceneNp].motion_type, &MotionInfo, &Cam[SceneNp].motion_type.state, frame_info_cam);

    sceVu0FVECTOR wind_dir;

    if (CScript.scene == RUSH_SCENE_A) {
        wind_dir[0] = 0.2f;
        wind_dir[2] = -0.2f;
        wind_dir[1] = 0.0f;
        wind_dir[3] = 0.0f;
    } else {
        wind_dir[0] = -0.2f;
        wind_dir[2] = 0.2f;
        wind_dir[1] = 0.0f;
        wind_dir[3] = 0.0f;
    }

    Wind__4.SetDir(wind_dir);

    switch (CScript.scene) {
        case RUSH_SCENE_A:
            Wind__4.SetVelocity(1.3f);
            break;
        case RUSH_SCENE_B:
            Wind__4.SetVelocity(0.4f);
            break;
        case RUSH_SCENE_C:
            Wind__4.SetVelocity(0.4f);
            break;
        case RUSH_SCENE_D:
            Wind__4.SetVelocity(0.4f);
            break;
        case RUSH_SCENE_E:
            Wind__4.SetVelocity(0.0f);
            break;
        case RUSH_SCENE_F:
            Wind__4.SetVelocity(0.4f);
            break;
        case RUSH_SCENE_G:
            Wind__4.SetVelocity(0.4f);
            break;
        case RUSH_SCENE_H:
            Wind__4.SetVelocity(0.4f);
            break;
        case RUSH_SCENE_I:
            Wind__4.SetVelocity(0.4f);
            break;
        case RUSH_SCENE_TITLE:
            Wind__4.SetVelocity(0.3f);
            break;
    }

    if (CScript.scene == RUSH_SCENE_H) {
        Chara__3[1].wind = (int) (intptr_t) &Wind__4;
    } else {
        Chara__3[0].wind = (int) (intptr_t) &Wind__4;
    }

    Wind__4.Step();

    char         *opening_frames[9] = {
        "c12a", "c12a", "c08a", "c08a", "e04a1", "e04a2", "e04a3", "e04a4", "e04a5"};
    char *scene_frames[9] = {
        "chr_a", "chr_b", "chr_c", "chr_d", "chr_e", "chr_f", "chr_g", "chr_h", "chr_i"};
    sceVu0FMATRIX matrix;
    CFrame       *frame;

    for (int i = 0; i < 9; i++) {
        if (CScript.obj[i].disp) {
            if (CScript.scene == RUSH_SCENE_A) {
                frame = Cam[SceneNp].frame->SearchFrame(opening_frames[i]);
            } else {
                frame = Cam[SceneNp].frame->SearchFrame(scene_frames[i]);
            }

            if (frame) {
                if (CScript.scene == RUSH_SCENE_A || CScript.camera_start == 16 || (CScript.camera_start == 17 && i == 0)) {
                    Chara__3[i].SetPosition(0.0f, 0.0f, 0.0f);
                    Chara__3[i].SetRotation(0.0f, 0.0f, 0.0f);
                    Chara__3[i].frame->SetReference(frame);
                } else {
                    frame->GetLWMatrix(matrix);

                    Chara__3[i].SetRotation(0.0f, atan2f(matrix[2][0], matrix[2][2]), 0.0f);
                    float x = matrix[3][0];
                    float y = matrix[3][1];
                    float z = matrix[3][2];
                    Chara__3[i].SetPosition(x, y, z);
                }
            }
        }
    }

    int scene = CScript.scene;

    if (scene == RUSH_SCENE_B || scene == RUSH_SCENE_D || scene == RUSH_SCENE_F || scene == RUSH_SCENE_H) {
        if (scene != RUSH_SCENE_H) {
            frame = Chara__3[0].frame->SearchFrame("weapon");
        } else {
            frame = Chara__3[1].frame->SearchFrame("weapon");
        }

        if (frame) {
            frame->GetLWMatrix(matrix);
            sceVu0Normalize(matrix[0], matrix[0]);
            sceVu0Normalize(matrix[1], matrix[1]);
            sceVu0Normalize(matrix[2], matrix[2]);
            Chara__3[8].SetPosition((float) (scene & 0), 0.0f, 0.0f);
            Chara__3[8].SetRotation(0.0f, 0.0f, 0.0f);
            Chara__3[8].frame->SetTransMatrix(matrix);
        }
    }

    if (scene == RUSH_SCENE_B) {
        if (CScript.camera_start == 5) {
            CScript.obj[7].disp = true;
        }

        if (CScript.obj[7].disp) {
            frame = Chara__3[0].frame->SearchFrame("dcol");

            if (frame) {
                frame->GetLWMatrix(matrix);
                sceVu0Normalize(matrix[0], matrix[0]);
                sceVu0Normalize(matrix[1], matrix[1]);
                sceVu0Normalize(matrix[2], matrix[2]);
                Chara__3[7].SetPosition(0.0f, 0.0f, 0.0f);
                Chara__3[7].SetRotation(0.0f, 0.0f, 0.0f);
                Chara__3[7].frame->SetTransMatrix(matrix);
            }

            static int old = 0;

            int cam_frame = (int) Cam[SceneNp].motion_type.state.time;

            if (old != cam_frame && (cam_frame == 12 || cam_frame == 17 || cam_frame == 52 || cam_frame == 111)) {
                CScript.obj[7].motion = 0;
                Chara__3[7].motion_no = 0;
                Chara__3[7].motion_flags = 4;
                Chara__3[7].motion_speed = -1.0f;
                old = cam_frame;
            } else if (old != cam_frame && cam_frame == 119) {
                CScript.obj[7].motion = 1;
                Chara__3[7].motion_no = 1;
                Chara__3[7].motion_flags = 4;
                Chara__3[7].motion_speed = -1.0f;
                old = cam_frame;
            } else {
                Chara__3[7].motion_no = CScript.obj[7].motion;
                Chara__3[7].motion_flags = 2;
                Chara__3[7].motion_speed = -1.0f;
            }
        }
    }

    static int iwacnt = 0;

    if (CScript.camera_start == 19) {
        if (iwacnt == 0) {
            Chara__3[2].SetPosition(260.26f, 507.76f, 520.5f);
            Chara__3[2].SetRotation(0.0f, 1.92f, 0.0f);
            Chara__3[2].motion_no = 0;
            Chara__3[2].motion_flags = 0;
            Chara__3[2].motion_speed = -1.0f;
            Chara__3[2].motion_type.state.time = 2.0f;
        }

        if (iwacnt == 330) {
            Chara__3[2].SetPosition(260.26f, 507.76f, 520.5f);
            Chara__3[2].SetRotation((float) (iwacnt & 0), 1.92f, 0.0f);
            Chara__3[2].motion_no = 1;
            Chara__3[2].motion_flags = 4;
            Chara__3[2].motion_speed = -1.0f;
            Chara__3[2].motion_type.state.time = 2.0f;
        }

        if (iwacnt == 331) {
            Chara__3[2].motion_no = 1;
            Chara__3[2].motion_flags = 0;
            Chara__3[2].motion_speed = -1.0f;
        }

        iwacnt++;
    } else {
        iwacnt = 0;
    }
}

/**
 *
 *
 * @mangled DrawProcess__Fv__2
 * @address 0x1DC9E70
 * @size 0x344
 * @unknownret
 */
static void DrawProcess() {
    if (DispFade.GetRate() == 128.0) {
        return;
    }

    sceVu0FVECTOR position;
    sceVu0FMATRIX camera;
    sceVu0FMATRIX view;
    sceVu0FMATRIX unit;

    MainCamera__3.GetPos(position);
    SndSetCamera(&MainCamera__3);
    MainCamera__3.GetCameraMatrix(camera);
    MainCamera__3.Step(1);

    sceVu0UnitMatrix(unit);
    sceVu0MulMatrix(view, unit, camera);
    MGSetViewMatrix(view, position);

    switch (CScript.scene) {
        case RUSH_SCENE_A:
            DrawProcA();
            break;
        case RUSH_SCENE_B:
            DrawProcB();
            break;
        case RUSH_SCENE_C:
            DrawProcC();
            break;
        case RUSH_SCENE_D:
            DrawProcD();
            break;
        case RUSH_SCENE_E:
            DrawProcE();
            break;
        case RUSH_SCENE_F:
            DrawProcF();
            break;
        case RUSH_SCENE_G:
            DrawProcG();
            break;
        case RUSH_SCENE_H:
            DrawProcH();
            break;
        case RUSH_SCENE_I:
            DrawProcI();
            break;
        case RUSH_SCENE_TITLE:
            DrawProcTitle();
            break;
    }

    if (CScript.scene != RUSH_SCENE_TITLE) {
        static int fade = 0;

        if (StartDisp) {
            TexManager.ReloadTexture(Vif1Packet, 20);
            set2DSprite(GetVif1Packet(), TexManager.GetTexture("start2", -1), CRect<int>(192, 392, 256, 32), CRect<int>(0, 0, 256, 32), fade);
            fade = (fade + 2) & 127;
        }

        if (GamePad.Down(PAD_TRIANGLE | PAD_CIRCLE | PAD_CROSS | PAD_SQUARE)) {
            StartDisp ^= 1;
        }

        if (!StartDisp) {
            fade = 0;
        }
    }

    sceGsTex0 back_tex;

    MGGetFBuffBackTex(&back_tex);

    CTexture texture;

    texture.tex0 = *(u_long *) &back_tex;
    set2DSprite(Vif1Packet, &texture, CRect<int>(0, 0, 640, SCREEN_HEIGHT), CRect<int>(0, 0, 640, SCREEN_HALF_HEIGHT), 128, 128, 128, 35);
}

/**
 *
 *
 * @mangled SoundProcess__Fv__2
 * @address 0x1DCA1C0
 * @size 0xCF4
 * @unknownret
 */
static void SoundProcess() {
    if (CScript.bgm_fade) {
        SndBgmFadeOut(64, 0);
        CScript.bgm_fade = 0;
    }

    if (CScript.scene == RUSH_SCENE_A) {
        static int mus = 0;

        if (CScript.camera_start == 1) {
            if (!mus) {
                SndBgmPlay(0);
                mus = 1;
            }
        } else {
            mus = 0;
        }
    }

    static int ambi = 0;

    if (CScript.scene == RUSH_SCENE_TITLE) {
        if (!ambi) {
            SndAmbientStop();
            ambi = 1;
        }
    } else {
        ambi = 0;
    }

    static int bat = 0;

    if (CScript.scene == RUSH_SCENE_A && Cam[SceneNp].motion_type.state.time > 10.0f) {
        if (!bat) {
            CFrame *frame = Cam[SceneNp].frame->SearchFrame("e04a5");

            if (frame) {
                sceVu0FMATRIX matrix;

                frame->GetLWMatrix(matrix);

                sceVu0FVECTOR position;

                position[0] = matrix[3][0];
                position[1] = matrix[3][1];
                position[2] = matrix[3][2];
                SndSePlay(SE_RUSH_IMPACT, position, 100.0f, 1000.0f);
            }
        }

        bat++;

        if (bat > 32) {
            bat = 0;
        }
    } else {
        bat = 0;
    }

    if ((CScript.scene == RUSH_SCENE_A && Cam[SceneNp].motion_type.state.time > 10.0f) || (CScript.scene == RUSH_SCENE_H && Cam[SceneNp].motion_type.state.time > 10.0f)) {
        static int wait = 0;

        if (CScript.scene == RUSH_SCENE_A) {
            CFrame *frame = Cam[SceneNp].frame->SearchFrame("c12a");
            int     chara_frame = (int) Chara__3[1].motion_type.state.time;

            if (wait == 0) {
                if (chara_frame == 10) {
                    if (frame) {
                        sceVu0FMATRIX matrix;

                        frame->GetLWMatrix(matrix);

                        sceVu0FVECTOR position;

                        position[0] = matrix[3][0];
                        position[1] = matrix[3][1];
                        position[2] = matrix[3][2];
                        SndSePlay(SE_RUSH_LOW_BUMP, position, 500.0f, 3000);
                    }

                    wait = 5;
                }
            } else if (wait > 0) {
                wait--;
            }
        } else {
            CFrame *frame = Cam[SceneNp].frame->SearchFrame("chr_a");
            int     chara_frame = (int) Chara__3[0].motion_type.state.time;

            if (wait == 0) {
                if (chara_frame == 20) {
                    if (frame) {
                        sceVu0FMATRIX matrix;

                        frame->GetLWMatrix(matrix);

                        sceVu0FVECTOR position;

                        position[0] = matrix[3][0];
                        position[1] = matrix[3][1];
                        position[2] = matrix[3][2];
                        SndSePlay(SE_RUSH_LOW_BUMP, position, 500.0f, 3000);
                    }

                    wait = 5;
                }
            } else if (wait > 0) {
                wait--;
            }
        }
    }

    static int wait = 0;

    if (wait == 0) {
        int cam_frame = (int) Cam[SceneNp].motion_type.state.time;

        switch (CScript.camera_start) {
            case 4:
                if (cam_frame == 30) {
                    SndSePlay(SE_AMBIENT_TOWN_PATTER, -1, 0);
                    SndSetSeVolf(SE_AMBIENT_TOWN_PATTER, 0.75f, 0);
                    wait = 5;
                }

                break;
            case 5:
                if (cam_frame == 10) {
                    SndSetSeVolf(SE_AMBIENT_TOWN_PATTER, 0.65f, 0);
                    SndSePlay(SE_RUSH_CRACK, -1, 0);
                    wait = 5;
                }

                if (cam_frame == 17) {
                    SndSePlay(SE_RUSH_CRACK_2, -1, 0);
                    wait = 5;
                }

                if (cam_frame == 23) {
                    SndSePlay(SE_RUSH_TAP, -1, 0);
                    wait = 5;
                }

                if (cam_frame == 31) {
                    SndSePlay(SE_RUSH_TAP_2, -1, 0);
                    wait = 5;
                }

                if (cam_frame == 46) {
                    SndSePlay(SE_WORLD_MAP_OPEN, -1, 0);
                    wait = 5;
                }

                if (cam_frame == 50) {
                    SndSePlay(SE_CHARA_SHOUT_3, -1, 0);
                    SndSePlay(SE_CHARA_ACTION_4, -1, 0);
                    wait = 5;
                }

                if (cam_frame == 53) {
                    SndSePlay(SE_RUSH_HISS, -1, 0);
                    wait = 5;
                }

                if (cam_frame == 62) {
                    SndSePlay(SE_RUSH_TAP_2, -1, 0);
                    wait = 5;
                }

                if (cam_frame == 65) {
                    SndSePlay(SE_BOX_OPEN, -1, 0);
                    wait = 5;
                }

                if (cam_frame == 97) {
                    SndSePlay(SE_RUSH_TAP, -1, 0);
                    wait = 5;
                }

                if (cam_frame == 113) {
                    SndSePlay(SE_CHARA_ACTION, -1, 0);
                    SndSePlay(SE_RUSH_CRACK_2, -1, 0);
                    wait = 5;
                }

                if (cam_frame == 119) {
                    SndSePlay(SE_CHARA_ACTION_2, -1, 0);
                    SndSePlay(SE_CHARA_SHOUT, -1, 0);
                    SndSePlay(SE_MONSTER_HIT, -1, 0);
                    wait = 5;
                }

                if (cam_frame == 124) {
                    SndSePlay(SE_RUSH_RUSTLE, -1, 0);
                    wait = 5;
                }

                if (cam_frame == 130) {
                    SndSePlay(SE_RUSH_CRACK_3, -1, 0);
                    wait = 5;
                }

                break;
            case 6:
                if (cam_frame == 47) {
                    SndSePlay(SE_RUSH_BRIGHT_SWELL, -1, 0);
                    wait = 10;
                }

                if (cam_frame == 124) {
                    SndSeStop(SE_AMBIENT_TOWN_PATTER, 0);
                    wait = 5;
                }

                break;
            case 8:
                if (cam_frame == 20) {
                    SndSePlay(SE_RUSH_WHOOSH, -1, 0);
                    wait = 5;
                }

                if (cam_frame == 37) {
                    SndSePlay(SE_WORLD_MAP_OPEN, -1, 0);
                    wait = 5;
                }

                if (cam_frame == 57) {
                    SndSePlay(SE_CHARA_ACTION, -1, 0);
                    wait = 4;
                }

                if (cam_frame == 60) {
                    SndSePlay(SE_MONSTER_HIT, -1, 0);
                    wait = 5;
                }

                if (cam_frame == 71) {
                    SndSePlay(SE_WORLD_MAP_OPEN, -1, 0);
                    wait = 5;
                }

                if (cam_frame == 86) {
                    SndSePlay(SE_RUSH_LONG_SWELL, -1, 0);
                    wait = 5;
                }

                break;
            case 11:
                if (cam_frame == 140) {
                    SndSePlay(SE_RUSH_DULL_KNOCK, -1, 0);
                    wait = 10;
                }

                if (cam_frame == 145) {
                    SndSePlay(SE_RUSH_DULL_KNOCK, -1, 0);
                    wait = 10;
                }

                if (cam_frame == 158) {
                    SndSePlay(SE_RUSH_SWELLING_TONE, -1, 0);
                    wait = 10;
                }

                break;
            case 12:
                if (cam_frame == 117) {
                    SndSePlay(SE_RUSH_DULL_KNOCK_2, -1, 0);
                    wait = 10;
                }

                if (cam_frame == 160) {
                    SndSePlay(SE_RUSH_DEEP_TONE, -1, 0);
                    wait = 10;
                }

                break;
            case 14:
                if (cam_frame == 87) {
                    SndSePlay(SE_RUSH_LOW_THUMP, -1, 0);
                    wait = 3;
                }

                break;
            case 15:
                if (cam_frame == 10) {
                    SndSePlay(SE_RUSH_CLACK, -1, 0);
                    SndSePlay(SE_RUSH_RISING_CHIRP, -1, 0);
                    wait = 5;
                }

                if (cam_frame == 18) {
                    SndSePlay(SE_RUSH_KNOCK, -1, 0);
                    wait = 5;
                }

                if (cam_frame == 40) {
                    SndSePlay(SE_RUSH_CLACK_2, -1, 0);
                    wait = 5;
                }

                if (cam_frame == 52) {
                    SndSePlay(SE_RUSH_KNOCK, -1, 0);
                    wait = 5;
                }

                if (cam_frame == 72) {
                    SndSePlay(SE_RUSH_FALLING_CHIRP, -1, 0);
                    wait = 5;
                }

                if (cam_frame == 88) {
                    SndSePlay(SE_RUSH_SCRAPE, -1, 0);
                    SndSePlay(SE_RUSH_BRIGHT_HIT, -1, 0);
                    wait = 5;
                }

                if (cam_frame == 94) {
                    SndSePlay(SE_RUSH_RISING_SWELL, -1, 0);
                    wait = 5;
                }

                break;
            case 17:
                if (cam_frame == 2) {
                    SndSePlay(SE_DRAN_FIELD_START, -1, 0);
                    wait = 3;
                }

                if (cam_frame == 40) {
                    SndSePlay(SE_RUSH_LONG_NOISE, -1, 0);
                    wait = 3;
                }

                if (cam_frame == 70) {
                    SndSePlay(SE_DRAN_FIELD_START, -1, 0);
                    SndSePlay(SE_RUSH_LONG_RUMBLE, -1, 0);
                    wait = 3;
                }

                break;
            case 18:
                if (cam_frame == 2) {
                    SndSePlay(SE_RUSH_LOW_HUM, -1, 0);
                    SndSePlay(SE_RUSH_LOW_SWELL, -1, 0);
                    wait = 5;
                }

                break;
            case 19:
                if (cam_frame == 26) {
                    SndSePlay(SE_RUSH_LOW_TONE, -1, 0);
                    wait = 5;
                }

                if (cam_frame == 56) {
                    SndSePlay(SE_RUSH_LOW_TONE, -1, 0);
                    wait = 5;
                }

                if (cam_frame == 86) {
                    SndSePlay(SE_RUSH_LOW_TONE, -1, 0);
                    wait = 5;
                }

                if (cam_frame == 120) {
                    SndSeStop(SE_RUSH_LOW_HUM, 0);
                    SndSeStop(SE_RUSH_LOW_SWELL, 0);
                    wait = 5;
                }

                break;
        }
    } else {
        wait--;
    }
}


void SetObjAnime(char *name, CFrameVu1 *frame, float *start, float *step) {
    OBJ_ANIME_SEQ &sequence = OP_AnimeSeq[OP_AnimeSeqRot];
    sequence.Initialize();
    sequence.property = OBJ_ANIME_PROPERTY_ROTATION;
    sequence.mode = OBJ_ANIME_MODE_LINEAR;
    sequence.from[0] = start[0];
    sequence.from[1] = start[1];
    sequence.from[2] = start[2];
    sequence.step[0] = step[0];
    sequence.step[1] = step[1];
    sequence.step[2] = step[2];
    strcpy(sequence.frame_name, name);
    InitObjAnime(frame, &sequence);
    OP_AnimeSeqRot++;
}

void WaterProcess() {
    sceGsTex0 frame_tex;
    sceGsTex0 water_tex;
    sceGsZbuf zbuf;

    MGGetFBuffTex(&frame_tex);

    CRect<int> rect(0, 0, 640, SCREEN_HALF_HEIGHT);

    water_tex = *(sceGsTex0 *) &TexManager.GetTexture("water_buff", -1)->tex0;
    MGMoveImage(&frame_tex, rect, &water_tex, 0, 0, 0);

    zbuf = mgZBuffer;
    zbuf.bits.zmsk = 1;
    MGSetGsZBUF(&zbuf);

    if (CScript.scene == RUSH_SCENE_B) {
        sceVu0FVECTOR position = {0.0f, -0.4f, 0.0f, 0.0f};

        Water__2.frame.SetPosition(position);
        Water__2.Shake(12, 4, (float) rand() * 4.5 / 2147483647.0 + 0.5);
    } else {
        sceVu0FVECTOR ref;

        MainCamera__3.GetRef(ref);
        ref[1] = 0.0f;
        Water__2.frame.SetPosition(ref);
        Water__2.Shake((int) (rand() * 32.0f / 2147483648.0f), (int) (rand() * 32.0f / 2147483648.0f), -0.5f);
    }

    Water__2.Hamon();
    DrawVu1__6CWaterFP10RenderInfoP13sceVif1PacketP1(&Water__2, &mgRenderInfo, GetVif1Packet(), nullptr);
    MGSetGsZBUF(&mgZBuffer);
}
