#ifdef PAL
#pragma argument_flag 0
#pragma argument_flag_ones 47, 54, 61, 64, 66, 159, 160, 162, 168, 1204
#pragma argument_flag_ones 1205, 1206, 1209, 1210, 1211, 1473, 1493, 1494, 1580, 1582
#pragma argument_flag_ones 1583, 1585, 1599
#else
#pragma argument_flag 0
#pragma argument_flag_ones 47, 54, 61, 64, 66, 159, 160, 162, 168, 334
#pragma argument_flag_ones 339, 344, 349, 354, 359, 364, 428, 430, 1138, 1139
#pragma argument_flag_ones 1140, 1143, 1144, 1145, 1407, 1427, 1428, 1514, 1516, 1517
#pragma argument_flag_ones 1519, 1533
#endif

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

void OpeningInit() {
    wait_now_loading_vsync();
    InitializeDataBuffer();
    SetDataBuffer(&VisualData, 7500);
    MesBuffer = VisualData.Alloc(4312);
    SetDataBuffer(&CharaDataBuffer__2[0], 253000);
    SetDataBuffer(&CharaDataBuffer__2[4], 65000);
    SetDataBuffer(&CharaDataBuffer__2[6], 327000);
    SetDataBuffer(&PassDataBuffer[0], 15000);
    SetDataBuffer(&PassDataBuffer[1], 15000);
    SetDataBuffer(&PassDataBuffer[2], 15000);
    SetDataBuffer(&testBuffer, 15000);
    PassReadBuffer = testBuffer.Alloc(15000);
    SetDataBuffer(&MapDataBuffer, 159500);
    SetDataBuffer(&WaterBuffer__2, 30000);
#ifdef PAL
    SetDataBuffer(&TextureData, 365000);
#else
    SetDataBuffer(&TextureData, 355000);
#endif
    SetPacketReadBuffer(40000, 273000);
    OP_MainCamera.SetRef(0, 0.0f, 0.0f, 0.0f);
    OP_MainCamera.SetPos(0, 0.0f, 0.0f, 0.0f);
    OP_MainCamera.SetDistance(80.0f);
    OP_MainCamera.SetHeight(0.0f);
    OP_MainCamera.SetFollow(0.0f, 0.0f, 0.0f);
    OP_MainCamera.Step(0);
    OP_MainCamera.SetSpeed(0.0f);
    MGSetRenderInfo(800.0f, 6.0f, 65535);
    wait_now_loading_vsync();
#ifdef PAL
    CScript__2.Load("opdat/opening.pal");
#else
    CScript__2.Load("opdat/opening.scr");
#endif
    wait_now_loading_vsync();
    LoadMessage();
    wait_now_loading_vsync();
    LoadScene();
    wait_now_loading_vsync();
    OpA_InitProcess();
    CameraMode = 0;
    SceneNp__2 = 0;
    SceneRp = 0;
    SceneCnt = 3;
    SceneFlg = SCENE_READ_IDLE;
    SceneSw = 0;
    Pause = 0;
    End = 0;
    EndCnt = 0;
    BgmOff = 0;
    BgmNo = 0;
    BgmVol = 127;
    DispFade.FadeInit(128.0f);
    DispFade.FadeOutStart(128.0f, 0);
}

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

#ifdef PAL
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
#else
    switch (LanguageCode) {
        case LANG_JAPANESE:
            LoadFile("opdat/fconv.bin", MesBuffer, 0);
            break;
        case LANG_ENGLISH_US:
            LoadFile("opdat/usa/fconv.bin", MesBuffer, 0);
            break;
        case LANG_ENGLISH_UK:
            LoadFile("opdat/usa/fconv.bin", MesBuffer, 0);
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
            LoadFile("opdat/usa/fconv.bin", MesBuffer, 0);
            break;
    }
#endif

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

void LoadSceneBG() {
    CDataAlloc2<1> *buffer;
    int             slot;
    char           *files[126][2] = {
        {"opdat/scene/0101acp.sne",  "0101acp.cfg" },
        {"opdat/scene/0101bcp.sne",  "0101bcp.cfg" },
        {"opdat/scene/0104cp.sne",   "0104cp.cfg"  },
        {"opdat/scene/0104bcp.sne",  "0104bcp.cfg" },
        {"opdat/scene/0105cp.sne",   "0105cp.cfg"  },
        {"opdat/scene/0107cp.sne",   "0107cp.cfg"  },
        {"opdat/scene/0108cp.sne",   "0108cp.cfg"  },
        {"opdat/scene/0109cp.sne",   "0109cp.cfg"  },
        {"opdat/scene/0110cp.sne",   "0110cp.cfg"  },
        {"opdat/scene/0111cp.sne",   "0111cp.cfg"  },
        {"opdat/scene/0112cp.sne",   "0112cp.cfg"  },
        {"opdat/scene/0113cp.sne",   "0113cp.cfg"  },
        {"opdat/scene/0114acp.sne",  "0114acp.cfg" },
        {"opdat/scene/0114bcp.sne",  "0114bcp.cfg" },
        {"opdat/scene/0114ccp.sne",  "0114ccp.cfg" },
        {"opdat/scene/0114dcp.sne",  "0114dcp.cfg" },
        {"opdat/scene/0114ecp.sne",  "0114ecp.cfg" },
        {"opdat/scene/0114gcp.sne",  "0114gcp.cfg" },
        {"opdat/scene/0114hcp.sne",  "0114hcp.cfg" },
        {"opdat/scene/0114icp.sne",  "0114icp.cfg" },
        {"opdat/scene/0114jcp.sne",  "0114jcp.cfg" },
        {"opdat/scene/0114kcp.sne",  "0114kcp.cfg" },
        {"opdat/scene/0114f1cp.sne", "0114f1cp.cfg"},
        {"opdat/scene/0114f2cp.sne", "0114f2cp.cfg"},
        {"opdat/scene/0115cp.sne",   "0115cp.cfg"  },
        {"opdat/scene/0116cp.sne",   "0116cp.cfg"  },
        {"opdat/scene/0117cp.sne",   "0117cp.cfg"  },
        {"opdat/scene/0118cp.sne",   "0118cp.cfg"  },
        {"opdat/scene/0119cp.sne",   "0119cp.cfg"  },
        {"opdat/scene/0119bcp.sne",  "0119bcp.cfg" },
        {"opdat/scene/0120cp.sne",   "0120cp.cfg"  },
        {"opdat/scene/0121cp.sne",   "0121cp.cfg"  },
        {"opdat/scene/0122cp.sne",   "0122cp.cfg"  },
        {"opdat/scene/0123cp.sne",   "0123cp.cfg"  },
        {"opdat/scene/0124cp.sne",   "0124cp.cfg"  },
        {"opdat/scene/0125cp.sne",   "0125cp.cfg"  },
        {"opdat/scene/0126cp.sne",   "0126cp.cfg"  },
        {"opdat/scene/0120cp.sne",   "0120cp.cfg"  },
        {"opdat/scene/0128cp.sne",   "0128cp.cfg"  },
        {"opdat/scene/0129cp.sne",   "0129cp.cfg"  },
        {"opdat/scene/0126cp.sne",   "0126cp.cfg"  },
        {"opdat/scene/0130cp.sne",   "0130cp.cfg"  },
        {"opdat/scene/0126cp.sne",   "0126cp.cfg"  },
        {"opdat/scene/0132cp.sne",   "0132cp.cfg"  },
        {"opdat/scene/0132bcp.sne",  "0132bcp.cfg" },
        {"opdat/scene/0132ccp.sne",  "0132ccp.cfg" },
        {"opdat/scene/0133cp.sne",   "0133cp.cfg"  },
        {"opdat/scene/0134cp.sne",   "0134cp.cfg"  },
        {"opdat/scene/0135cp.sne",   "0135cp.cfg"  },
        {"opdat/scene/0136cp.sne",   "0136cp.cfg"  },
        {"opdat/scene/0301cp.sne",   "0301cp.cfg"  },
        {"opdat/scene/0302cp.sne",   "0302cp.cfg"  },
        {"opdat/scene/0303cp.sne",   "0303cp.cfg"  },
        {"opdat/scene/0304cp.sne",   "0304cp.cfg"  },
        {"opdat/scene/0305cp.sne",   "0305cp.cfg"  },
        {"opdat/scene/0306cp.sne",   "0306cp.cfg"  },
        {"opdat/scene/0307cp.sne",   "0307cp.cfg"  },
        {"opdat/scene/0308cp.sne",   "0308cp.cfg"  },
        {"opdat/scene/0401cp.sne",   "0401cp.cfg"  },
        {"opdat/scene/0402cp.sne",   "0402cp.cfg"  },
        {"opdat/scene/0403cp.sne",   "0403cp.cfg"  },
        {"opdat/scene/0404cp.sne",   "0404cp.cfg"  },
        {"opdat/scene/0405cp.sne",   "0405cp.cfg"  },
        {"opdat/scene/0406cp.sne",   "0406cp.cfg"  },
        {"opdat/scene/0407cp.sne",   "0407cp.cfg"  },
        {"opdat/scene/0408cp.sne",   "0408cp.cfg"  },
        {"opdat/scene/0409cp.sne",   "0409cp.cfg"  },
        {"opdat/scene/0410cp.sne",   "0410cp.cfg"  },
        {"opdat/scene/0411cp.sne",   "0411cp.cfg"  },
        {"opdat/scene/0412cp.sne",   "0412cp.cfg"  },
        {"opdat/scene/0413cp.sne",   "0413cp.cfg"  },
        {"opdat/scene/0414cp.sne",   "0414cp.cfg"  },
        {"opdat/scene/0415cp.sne",   "0415cp.cfg"  },
        {"opdat/scene/0416cp.sne",   "0416cp.cfg"  },
        {"opdat/scene/0417cp.sne",   "0417cp.cfg"  },
        {"opdat/scene/0418cp.sne",   "0418cp.cfg"  },
        {"opdat/scene/0419cp.sne",   "0419cp.cfg"  },
        {"opdat/scene/0420cp.sne",   "0420cp.cfg"  },
        {"opdat/scene/0421cp.sne",   "0421cp.cfg"  },
        {"opdat/scene/0423cp.sne",   "0423cp.cfg"  },
        {"opdat/scene/0425cp.sne",   "0425cp.cfg"  },
        {"opdat/scene/0426cp.sne",   "0426cp.cfg"  },
        {"opdat/scene/0427cp.sne",   "0427cp.cfg"  },
        {"opdat/scene/0428cp.sne",   "0428cp.cfg"  },
        {"opdat/scene/0429cp.sne",   "0429cp.cfg"  },
        {"opdat/scene/0430cp.sne",   "0430cp.cfg"  },
        {"opdat/scene/0431cp.sne",   "0431cp.cfg"  },
        {"opdat/scene/0432cp.sne",   "0432cp.cfg"  },
        {"opdat/scene/0433cp.sne",   "0433cp.cfg"  },
        {"opdat/scene/0434cp.sne",   "0434cp.cfg"  },
        {"opdat/scene/0435cp.sne",   "0435cp.cfg"  },
        {"opdat/scene/0436cp.sne",   "0436cp.cfg"  },
        {"opdat/scene/0437cp.sne",   "0437cp.cfg"  },
        {"opdat/scene/0438cp.sne",   "0438cp.cfg"  },
        {"opdat/scene/0439cp.sne",   "0439cp.cfg"  },
        {"opdat/scene/0440cp.sne",   "0440cp.cfg"  },
        {"opdat/scene/0441cp.sne",   "0441cp.cfg"  },
        {"opdat/scene/0442cp.sne",   "0442cp.cfg"  },
        {"opdat/scene/0443cp.sne",   "0443cp.cfg"  },
        {"opdat/scene/0501cp.sne",   "0501cp.cfg"  },
        {"opdat/scene/0502cp.sne",   "0502cp.cfg"  },
        {"opdat/scene/0503cp.sne",   "0503cp.cfg"  },
        {"opdat/scene/0504cp.sne",   "0504cp.cfg"  },
        {"opdat/scene/0505cp.sne",   "0505cp.cfg"  },
        {"opdat/scene/0506cp.sne",   "0506cp.cfg"  },
        {"opdat/scene/0510cp.sne",   "0510cp.cfg"  },
        {"opdat/scene/0511cp.sne",   "0511cp.cfg"  },
        {"opdat/scene/0512cp.sne",   "0512cp.cfg"  },
        {"opdat/scene/0513cp.sne",   "0513cp.cfg"  },
        {"opdat/scene/0514cp.sne",   "0514cp.cfg"  },
        {"opdat/scene/0515cp.sne",   "0515cp.cfg"  },
        {"opdat/scene/0515bcp.sne",  "0515bcp.cfg" },
        {"opdat/scene/0516cp.sne",   "0516cp.cfg"  },
        {"opdat/scene/0517cp.sne",   "0517cp.cfg"  },
        {"opdat/scene/0518cp.sne",   "0518cp.cfg"  },
        {"opdat/scene/0520cp.sne",   "0520cp.cfg"  },
        {"opdat/scene/0521cp.sne",   "0521cp.cfg"  },
        {"opdat/scene/0522cp.sne",   "0522cp.cfg"  },
        {"opdat/scene/0523cp.sne",   "0523cp.cfg"  },
        {"opdat/scene/0524cp.sne",   "0524cp.cfg"  },
        {"opdat/scene/0525cp.sne",   "0525cp.cfg"  },
        {"opdat/scene/0526cp.sne",   "0526cp.cfg"  },
        {"opdat/scene/0527cp.sne",   "0527cp.cfg"  },
        {"opdat/scene/0528cp.sne",   "0528cp.cfg"  },
        {"opdat/scene/0529cp.sne",   "0529cp.cfg"  },
        {"-1",                       "-1"          },
    };

    if (files[SceneCnt][0] == "-1") {
        return;
    }

    switch (SceneFlg) {
        case SCENE_READ_START:
            while (ReadBGSync())
                ;

            if (SceneCnt % 5 == 0) {
                StartReadBG();
            }

            LoadFileBG(files[SceneCnt][0], (u_long128 *) PassReadBuffer, 0);
            SceneFlg = SCENE_READ_WAIT;
            break;
        case SCENE_READ_WAIT:
            if (ReadBGSync()) {
                break;
            }

            slot = SceneRp;
            buffer = &PassDataBuffer[slot];
            buffer->used = 0;
            Cam__2[slot].LoadPackData((u_int *) PassReadBuffer, files[SceneCnt][1], buffer, 0);
            SceneCnt++;
            SceneRp++;

            if (SceneRp > 2) {
                SceneRp = 0;
            }

            SceneFlg = SCENE_READ_IDLE;
            break;
        case SCENE_READ_IDLE:
            break;
    }
}

int OpeningLoop() {
    ReadBG();
    PauseProcess();

    if (Pause == 0) {
        CScript__2.Step();
        WaitKeyProcess();
        SceneChange();
    }

    MotionProcess();
    SoundProcess();
    DrawProcess();

    if (End) {
        if (EndCnt < 128) {
            EndCnt++;
        } else {
            SoundStop();
            MGSetBGColor(0.0f, 0.0f, 0.0f, 128.0f);
            FadeCansel();
            return 1;
        }
    }

    /* These type-only names preserve the second exit's background-colour argument state. */
    typedef float ExitState0, ExitState1;

    if (CScript__2.end) {
        MGSetBGColor(0.0f, 0.0f, 0.0f, 128.0f);
        FadeCansel();
        return 1;
    }

    CSnd.Step();
    return 0;
}

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
#ifdef PAL_TIMING
            DispFade.FadeInStart(1.2f * CScript__2.fade_speed, 0);
#else
            DispFade.FadeInStart(CScript__2.fade_speed, 0);
#endif
            CScript__2.fade = TSFADE_NONE;
            break;
        case TSFADE_OUT_BLACK:
#ifdef PAL_TIMING
            DispFade.FadeOutStart(1.2f * CScript__2.fade_speed, 0);
#else
            DispFade.FadeOutStart(CScript__2.fade_speed, 0);
#endif
            CScript__2.fade = TSFADE_NONE;
            break;
        case TSFADE_IN_WHITE:
#ifdef PAL_TIMING
            DispFade.FadeInStart(1.2f * CScript__2.fade_speed, 1);
#else
            DispFade.FadeInStart(CScript__2.fade_speed, 1);
#endif
            CScript__2.fade = TSFADE_NONE;
            break;
        case TSFADE_OUT_WHITE:
#ifdef PAL_TIMING
            DispFade.FadeOutStart(1.2f * CScript__2.fade_speed, 1);
#else
            DispFade.FadeOutStart(CScript__2.fade_speed, 1);
#endif
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
#ifdef PAL_TIMING
                CScript__2.bgm_fade = 1.2f * CScript__2.bgm_fade;
#endif
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

#ifdef PAL
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

#else
        set2DSprite(GetVif1Packet(), TexManager.GetTexture("pause_e", -1), CRect<int>(256, 160, 128, 128), CRect<int>(0, 0, 128, 128), 128);
#endif
        setbilinear(1);
    }

    MGGetFBuffBackTex(&tex0);

    CTexture texture;

    texture.tex0 = *(u_long *) &tex0;
#ifdef PAL
    set2DSprite(Vif1Packet, &texture, CRect<int>(0, 0, 640, SCREEN_HEIGHT), CRect<int>(0, 0, 640, SCREEN_HALF_HEIGHT), 128, 128, 128, 40);
#else
    set2DSprite(Vif1Packet, &texture, CRect<int>(0, 0, 640, 448), CRect<int>(0, 0, 640, 224), 128, 128, 128, 35);
#endif
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

void OpBgmPlay() {
    int volumes[8] = {82, 106, 64, 69, 91, 92, 95, 108};

    if (BgmOff == 0) {
        CSnd.SQ_Play(MIDI_PORT_BGM, 0);
    } else {
        CSnd.SQ_Play(MIDI_PORT_BGM, 0, 0);
    }

    BgmVol = volumes[BgmNo];
    BgmNo++;
}

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
    virtual void FrameObjectOnOff(char *name, int on);
    virtual void Draw();

    void SetFrame(CFrameVu1 *frame, int level);
};

/* One piece of scenery. A map holds a table of them, hands each its model, and drives them through
   the object dispatch like anything else in the world. */
class CMapObject : public CObjectFrame {
public:
    char       unk_00[36];
    CFrameVu1 *unk_D4;
    char       unk_28[8];
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
void OPAnalyz(char *name) {
    char *buffer;
    int   i;
    int   position;
    int   matched;

    argLevel = 0;
    buffer = teigiBuff;

    if (LoadFile(name, teigiBuff, &teigiFileSize) == 0) {
        return;
    }

    for (i = 0; i < teigiFileSize; i++) {
        if (buffer[i] == 13 && buffer[i + 1] == 10) {
            buffer[i + 1] = 0;
            buffer[i] = 0;
        }
    }

    for (i = 0; i < 96; i++) {
        pointLight[i].used = 0;
    }

    pointLightStack = 0;

    position = 0;

    while (position < teigiFileSize) {
        matched = 0;

        position = skipSpace(buffer, position);

        if (memcmp(&buffer[position], "GRD_IMG", 7) == 0) {
            position = skipSpace(buffer, position + 7);
            position = checkArg(buffer, position, TEIGI_GRD_IMG);

            if (position != -1) {
                matched = 1;
            }

            position = skipSpace(buffer, position);
            argLevel++;
        }

        if (memcmp(&buffer[position], "BLD_IMG", 7) == 0) {
            position = skipSpace(buffer, position + 7);
            position = checkArg(buffer, position, TEIGI_BLD_IMG);

            if (position != -1) {
                matched = 1;
            }

            position = skipSpace(buffer, position);
            argLevel++;
        }

        if (memcmp(&buffer[position], "SKY_IMG", 7) == 0) {
            position = skipSpace(buffer, position + 7);
            position = checkArg(buffer, position, TEIGI_SKY_IMG);

            if (position != -1) {
                matched = 1;
            }

            position = skipSpace(buffer, position);
            argLevel++;
        }

        if (memcmp(&buffer[position], "GND", 3) == 0) {
            position = skipSpace(buffer, position + 3);
            position = checkArg(buffer, position, TEIGI_GRD);

            if (position != -1) {
                matched = 1;
            }

            position = skipSpace(buffer, position);
            argLevel++;
        }

        if (memcmp(&buffer[position], "BLD", 3) == 0 && memcmp(&buffer[position], "BLD_IMG", 7) != 0) {
            position = skipSpace(buffer, position + 3);
            position = checkArg(buffer, position, TEIGI_BLD);

            if (position != -1) {
                matched = 1;
            }

            position = skipSpace(buffer, position);
            argLevel++;
        }

        if (memcmp(&buffer[position], "LOD", 3) == 0) {
            position = skipSpace(buffer, position + 3);
            position = checkArg(buffer, position, TEIGI_LOD);

            if (position != -1) {
                matched = 1;
            }

            position = skipSpace(buffer, position);
            argLevel++;
        }

        if (memcmp(&buffer[position], "SKY", 3) == 0) {
            position = skipSpace(buffer, position + 3);
            position = checkArg(buffer, position, TEIGI_SKY);

            if (position != -1) {
                matched = 1;
            }

            position = skipSpace(buffer, position);
            argLevel++;
        }

        if (memcmp(&buffer[position], "FOG", 3) == 0) {
            position = skipSpace(buffer, position + 3);
            position = checkArg(buffer, position, TEIGI_FOG);

            if (position != -1) {
                matched = 1;
            }

            position = skipSpace(buffer, position);
            argLevel++;
        }

        if (memcmp(&buffer[position], "CRD", 3) == 0) {
            position = skipSpace(buffer, position + 3);
            position = checkArg(buffer, position, TEIGI_CRD);

            if (position != -1) {
                matched = 1;
            }

            position = skipSpace(buffer, position);
            argLevel++;
        }

        if (memcmp(&buffer[position], "AMBIENT", 7) == 0) {
            position = skipSpace(buffer, position + 7);
            position = checkArg(buffer, position, TEIGI_AMBIENT);

            if (position != -1) {
                matched = 1;
            }

            position = skipSpace(buffer, position);
            argLevel++;
        }

        if (memcmp(&buffer[position], "LIGHT_C", 7) == 0) {
            position = skipSpace(buffer, position + 7);
            position = checkArg(buffer, position, TEIGI_LIGHT_COL);

            if (position != -1) {
                matched = 1;
            }

            position = skipSpace(buffer, position);
            argLevel++;
        }

        if (memcmp(&buffer[position], "FARCLIP", 7) == 0) {
            position = skipSpace(buffer, position + 7);
            position = checkArg(buffer, position, TEIGI_FARCLIP);

            if (position != -1) {
                matched = 1;
            }

            position = skipSpace(buffer, position);
            argLevel++;
        }

        /* Six where the keyword is seven, which is the original's own step: the `2` is left
           standing, and a digit is not a separator, so it is the argument reader that meets it. */
        if (memcmp(&buffer[position], "BG_COL2", 7) == 0) {
            position = skipSpace(buffer, position + 6);
            position = checkArg(buffer, position, TEIGI_BG_COL2);

            if (position != -1) {
                matched = 1;
            }

            position = skipSpace(buffer, position);
            argLevel++;
        }

        if (memcmp(&buffer[position], "BG_COL", 6) == 0) {
            position = skipSpace(buffer, position + 6);
            position = checkArg(buffer, position, TEIGI_BG_COL);

            if (position != -1) {
                matched = 1;
            }

            position = skipSpace(buffer, position);
            argLevel++;
        }

        if (memcmp(&buffer[position], "NORMALCLIP_OFF", 14) == 0) {
            position = skipSpace(buffer, position + 14);
            position = checkArg(buffer, position, TEIGI_NORMALCLIP_OFF);

            if (position != -1) {
                matched = 1;
            }

            position = skipSpace(buffer, position);
            argLevel++;
        }

        if (memcmp(&buffer[position], "RUN_SPEED", 9) == 0) {
            position = skipSpace(buffer, position + 9);
            position = checkArg(buffer, position, TEIGI_RUN_SPEED);

            if (position != -1) {
                matched = 1;
            }

            position = skipSpace(buffer, position);
            argLevel++;
        }

        if (memcmp(&buffer[position], "EDIT_FOG", 8) == 0) {
            position = skipSpace(buffer, position + 8);
            position = checkArg(buffer, position, TEIGI_EDIT_FOG);

            if (position != -1) {
                matched = 1;
            }

            position = skipSpace(buffer, position);
            argLevel++;
        }

        if (memcmp(&buffer[position], "WATER_SET", 9) == 0) {
            position = skipSpace(buffer, position + 9);
            position = checkArg(buffer, position, TEIGI_WATER_SET);

            if (position != -1) {
                matched = 1;
            }

            position = skipSpace(buffer, position);
            argLevel++;
        }

        if (memcmp(&buffer[position], "WATER_RGB", 9) == 0) {
            position = skipSpace(buffer, position + 9);
            position = checkArg(buffer, position, TEIGI_WATER_RGB);

            if (position != -1) {
                matched = 1;
            }

            position = skipSpace(buffer, position);
            argLevel++;
        }

        if (memcmp(&buffer[position], "WATER_PARAM", 11) == 0) {
            position = skipSpace(buffer, position + 11);
            position = checkArg(buffer, position, TEIGI_WATER_PARAM);

            if (position != -1) {
                matched = 1;
            }

            position = skipSpace(buffer, position);
            argLevel++;
        }

        if (memcmp(&buffer[position], "LEVEL_FAR", 9) == 0) {
            position = skipSpace(buffer, position + 9);
            position = checkArg(buffer, position, TEIGI_LEVEL_FAR);

            if (position != -1) {
                matched = 1;
            }

            position = skipSpace(buffer, position);
            argLevel++;
        }

        if (memcmp(&buffer[position], "DebugFlag", 9) == 0) {
            position = skipSpace(buffer, position + 9);
            position = checkArg(buffer, position, TEIGI_DebugFlag);

            if (position != -1) {
                matched = 1;
            }

            position = skipSpace(buffer, position);
            argLevel++;
        }

        if (memcmp(&buffer[position], "AnimeSpeed", 10) == 0) {
            position = skipSpace(buffer, position + 10);
            position = checkArg(buffer, position, TEIGI_AnimeSpeed);

            if (position != -1) {
                matched = 1;
            }

            position = skipSpace(buffer, position);
            argLevel++;
        }

        if (memcmp(&buffer[position], "UPER", 4) == 0) {
            position = skipSpace(buffer, position + 4);
            position = checkArg(buffer, position, TEIGI_UPER);

            if (position != -1) {
                matched = 1;
            }

            position = skipSpace(buffer, position);
            argLevel++;
        }

        if (memcmp(&buffer[position], "UPR_IMG", 7) == 0) {
            position = skipSpace(buffer, position + 7);
            position = checkArg(buffer, position, TEIGI_UPR_IMG);

            if (position != -1) {
                matched = 1;
            }

            position = skipSpace(buffer, position);
            argLevel++;
        }

        if (memcmp(&buffer[position], "PLIGHT", 6) == 0) {
            position = skipSpace(buffer, position + 6);
            position = checkArg(buffer, position, TEIGI_PLIGHT);

            if (position != -1) {
                matched = 1;
            }

            position = skipSpace(buffer, position);
            argLevel++;
        }

        if (memcmp(&buffer[position], "ADD_CRD", 7) == 0) {
            position = skipSpace(buffer, position + 7);
            position = checkArg(buffer, position, TEIGI_ADD_CRD);

            if (position != -1) {
                matched = 1;
            }

            position = skipSpace(buffer, position);
            argLevel++;
        }

        if (memcmp(&buffer[position], "DEF_PATS", 8) == 0) {
            position = skipSpace(buffer, position + 8);
            argValBuff[argLevel][0] = (float) TEIGI_DEF_PATS[0];
            matched = 1;
            argLevel++;
        }

        if (memcmp(&buffer[position], "DEF_ENDS", 8) == 0) {
            position = skipSpace(buffer, position + 8);
            argValBuff[argLevel][0] = (float) TEIGI_DEF_ENDS[0];
            matched = 1;
            argLevel++;
        }

        if (memcmp(&buffer[position], "PT_BASE", 7) == 0) {
            position = skipSpace(buffer, position + 7);
            position = checkArg(buffer, position, TEIGI_PT_BASE);

            if (position != -1) {
                matched = 1;
            }

            position = skipSpace(buffer, position);
            argLevel++;
        }

        if (memcmp(&buffer[position], "MAPD", 4) == 0) {
            position = skipSpace(buffer, position + 4);
            position = checkArg(buffer, position, TEIGI_MAPD);

            if (position != -1) {
                matched = 1;
            }

            position = skipSpace(buffer, position);
            argLevel++;
        }

        if (memcmp(&buffer[position], "PT_COLS", 7) == 0) {
            position = skipSpace(buffer, position + 7);
            position = checkArg(buffer, position, TEIGI_PT_COLS);

            if (position != -1) {
                matched = 1;
            }

            position = skipSpace(buffer, position);
            argLevel++;
        }

        if (memcmp(&buffer[position], "PT_FIRE", 7) == 0) {
            position = skipSpace(buffer, position + 7);
            position = checkArg(buffer, position, TEIGI_PT_FIRE);

            if (position != -1) {
                matched = 1;
            }

            position = skipSpace(buffer, position);
            argLevel++;
        }

        if (memcmp(&buffer[position], "PT_WATER", 8) == 0) {
            position = skipSpace(buffer, position + 8);
            position = checkArg(buffer, position, TEIGI_PT_WATER);

            if (position != -1) {
                matched = 1;
            }

            position = skipSpace(buffer, position);
            argLevel++;
        }

        if (memcmp(&buffer[position], "S_VOLUME", 8) == 0) {
            position = skipSpace(buffer, position + 8);
            position = checkArg(buffer, position, TEIGI_S_VOLUME);

            if (position != -1) {
                matched = 1;
            }

            position = skipSpace(buffer, position);
            argLevel++;
        }

        if (memcmp(&buffer[position], "PROJECTION", 10) == 0) {
            position = skipSpace(buffer, position + 10);
            position = checkArg(buffer, position, TEIGI_PROJECTION);

            if (position != -1) {
                matched = 1;
            }

            position = skipSpace(buffer, position);
            argLevel++;
        }

        if (memcmp(&buffer[position], "OBJ_ROT", 7) == 0) {
            position = skipSpace(buffer, position + 7);
            position = checkArg(buffer, position, TEIGI_OBJ_ROT);

            if (position != -1) {
                matched = 1;
            }

            position = skipSpace(buffer, position);
            argLevel++;
        }

        if (memcmp(&buffer[position], "MAPINFO", 7) == 0) {
            position = skipSpace(buffer, position + 7);
            position = checkArg(buffer, position, TEIGI_MAPINFO);

            if (position != -1) {
                matched = 1;
            }

            position = skipSpace(buffer, position);
            argLevel++;
        }

        if (memcmp(&buffer[position], "FIRE_IMG", 8) == 0) {
            position = skipSpace(buffer, position + 8);
            position = checkArg(buffer, position, TEIGI_FIRE_IMG);

            if (position != -1) {
                matched = 1;
            }

            position = skipSpace(buffer, position);
            argLevel++;
        }

        if (memcmp(&buffer[position], "FIRE", 4) == 0) {
            position = skipSpace(buffer, position + 4);
            position = checkArg(buffer, position, TEIGI_FIRE);

            if (position != -1) {
                matched = 1;
            }

            position = skipSpace(buffer, position);
            argLevel++;
        }

        if (!matched) {
            exit__2(-1);
        }

        position = skipSpace(buffer, position);
    }
}

/* Walk the levels the reader left behind and do what each one asks. A level's number stands in
   slot 0 of its row and every command is tested against every level, so a file may repeat a
   command as often as it likes and the last one to run wins. The frame attribute the loader builds
   as it goes is what every model it places is given, which is why the two commands that place a
   whole building save it and put it back: what they set is theirs alone. */
void OPMdsLoad() {
    CFrameAttr    saved_attr;
    CFrameAttr    attr;
    char          path[4][128];
    CFrameVu1    *lod_frames[4];
    sceVu0FVECTOR light_dir;
    int           i;
    int           j;
    int           k;
    int           light_no;
    int           light_slot;
    CFrameVu1    *frame;
    CFrameVu1    *rot_frame;
    CMapObject   *object;
    CFrame       *shadow;
    float         rx;
    float         ry;
    float         rz;
    float         light_x;

    attr.clip_depth = 20.0f;
    attr.clip_enable = true;
    attr.program_option = 0;

    nowObjCnt = 0;
    nowObjCnt2 = 0;

    for (i = 0; i < argLevel; i++) {
        if (TEIGI_AnimeSpeed[0] == (int) argValBuff[i][0]) {
            animeSpeed1 = (int) argValBuff[i][1];
            animeSpeed2 = (int) argValBuff[i][2];
        }

        if (TEIGI_DebugFlag[0] == (int) argValBuff[i][0]) {
            debugModeFlag = (int) argValBuff[i][1];
        }

        if (TEIGI_LEVEL_FAR[0] == (int) argValBuff[i][0]) {
            levelOfDitialZ[0] = argValBuff[i][1];
            levelOfDitialZ[1] = argValBuff[i][2];
            levelOfDitialZ[2] = argValBuff[i][3];
            levelOfDitialZ[3] = argValBuff[i][4];
        }

        if (TEIGI_WATER_PARAM[0] == (int) argValBuff[i][0]) {
            WaterMeshW = (int) argValBuff[i][1];
            WaterMeshH = (int) argValBuff[i][2];
            WaterShake = argValBuff[i][3];
            WaterCourant = argValBuff[i][4];
            WaterDecline = argValBuff[i][5];
            WaterAmplitude = argValBuff[i][6];
            WaterRefraction = argValBuff[i][7];
        }

        if (TEIGI_WATER_SET[0] == (int) argValBuff[i][0]) {
            float half_width = argValBuff[i][1] / 2.0f;
            float half_depth = argValBuff[i][2] / 2.0f;

            WaterV1[0] = -half_width;
            WaterV1[2] = -half_depth;
            WaterV2[0] = half_width;
            WaterV2[2] = -half_depth;
            WaterV3[0] = -half_width;
            WaterV3[2] = half_depth;
            WaterV4[0] = half_width;
            WaterV4[2] = half_depth;
            WaterPos[0] = argValBuff[i][3];
            WaterPos[1] = argValBuff[i][4];
            WaterPos[2] = argValBuff[i][5];
            WaterFlag = 1;
        }

        if (TEIGI_WATER_RGB[0] == (int) argValBuff[i][0]) {
            WaterR = argValBuff[i][1];
            WaterG = argValBuff[i][2];
            WaterB = argValBuff[i][3];
        }

        if (TEIGI_RUN_SPEED[0] == (int) argValBuff[i][0]) {
            run_speed = argValBuff[i][1];
        }

        if (TEIGI_NORMALCLIP_OFF[0] == (int) argValBuff[i][0]) {
            if (argValBuff[i][1] == 0.0f) {
                attr.program_option = 0;
            } else {
                attr.program_option = 1;
            }
        }

        if (TEIGI_BG_COL2[0] == (int) argValBuff[i][0]) {
            MGSetBGColor((float) (u_int) argValBuff[i][1], (float) (u_int) argValBuff[i][2], (float) (u_int) argValBuff[i][3], 128.0f);
        }

        if (TEIGI_BG_COL[0] == (int) argValBuff[i][0]) {
            MGSetBGColor((float) (u_int) argValBuff[i][1], (float) (u_int) argValBuff[i][2], (float) (u_int) argValBuff[i][3], 128.0f);
        }

        if (TEIGI_FARCLIP[0] == (int) argValBuff[i][0]) {
            attr.far_clip_enable = true;
            attr.far_clip = argValBuff[i][1];
        }

        if (TEIGI_LIGHT_COL[0] == (int) argValBuff[i][0]) {
            light_no = (int) argValBuff[i][7];
            light_dir[0] = argValBuff[i][1];
            light_dir[1] = argValBuff[i][2];
            light_dir[2] = argValBuff[i][3];
            sceVu0Normalize(light_dir, light_dir);
            /* The row's index is materialised between the first component's load and its store,
               and an assignment's right-hand side is emitted before the subscript it is stored
               through, so the component has to already be in a register when the statement that
               computes the index runs. */
            light_x = light_dir[0];
            light_slot = light_no - 1;
            light[0][light_slot] = light_x;
            light[1][light_slot] = light_dir[1];
            light[2][light_slot] = light_dir[2];
            lightcolor[light_slot][0] = argValBuff[i][4];
            lightcolor[light_slot][1] = argValBuff[i][5];
            lightcolor[light_slot][2] = argValBuff[i][6];
            MGSetPLight(light, lightcolor);
        }

        if (TEIGI_AMBIENT[0] == (int) argValBuff[i][0]) {
            ambientlight[0] = argValBuff[i][1];
            ambientlight[1] = argValBuff[i][2];
            ambientlight[2] = argValBuff[i][3];
            MGSetAmbient(ambientlight);
        }

        if (TEIGI_CRD[0] == (int) argValBuff[i][0]) {
            strcpy(path[0], "sim:");
            strcat(path[0], argStrBuff[i]);
            LoadFile(path[0], (void *) read_buffer, 0);
            OP_GroundCol = LoadCollisionFile(read_buffer);
        }

        if (TEIGI_FOG[0] == (int) argValBuff[i][0]) {
            op_fogRate[0] = argValBuff[i][1];
            op_fogRate[1] = argValBuff[i][2];
            op_fogRate[2] = argValBuff[i][6];
            op_fogRate[3] = argValBuff[i][7];
            op_fogColor[0] = argValBuff[i][3];
            op_fogColor[1] = argValBuff[i][4];
            op_fogColor[2] = argValBuff[i][5];
            MGSetFogParm(op_fogRate[0], op_fogRate[1], op_fogColor[0], op_fogColor[1], op_fogColor[2], op_fogRate[2], op_fogRate[3]);
            attr.fog_enable = true;
        }

        if (TEIGI_EDIT_FOG[0] == (int) argValBuff[i][0]) {
            editFogRate[0] = argValBuff[i][1];
            editFogRate[1] = argValBuff[i][2];
            editFogRate[2] = argValBuff[i][6];
            editFogRate[3] = argValBuff[i][7];
            editFogColor[0] = argValBuff[i][3];
            editFogColor[1] = argValBuff[i][4];
            editFogColor[2] = argValBuff[i][5];
            attr.fog_enable = true;
        }

        if (TEIGI_PLIGHT[0] == (int) argValBuff[i][0]) {
            pointLight[pointLightStack].used = 1;
            pointLight[pointLightStack].x = 10.0f * argValBuff[i][1];
            pointLight[pointLightStack].y = 10.0f * argValBuff[i][2];
            pointLight[pointLightStack].z = 10.0f * argValBuff[i][3];
            pointLight[pointLightStack].r = argValBuff[i][4];
            pointLight[pointLightStack].g = argValBuff[i][5];
            pointLight[pointLightStack].b = argValBuff[i][6];
            pointLight[pointLightStack].arg7 = argValBuff[i][7];
            pointLight[pointLightStack].arg8 = argValBuff[i][8];
            pointLight[pointLightStack].arg9 = argValBuff[i][9];
            pointLightStack++;
        }

        if (TEIGI_GRD[0] == (int) argValBuff[i][0]) {
            strcpy(path[0], "sim:");
            strcat(path[0], argStrBuff[i]);
            LoadFile(path[0], (void *) read_buffer, 0);
            frame = LoadMDSFile(read_buffer, &MapDataBuffer, 2, 0, 0);
            rot_frame = frame;
            frame->SetAttr(attr, 1, 64);
            SetFrameAttr(frame, 1);
            object = OP_GroundMap.SetObject(frame, 0, 0);
            CVector3_f_ position(10.0f * argValBuff[i][2], 10.0f * argValBuff[i][3], 10.0f * argValBuff[i][4]);
            object->SetPosition(position);
            rx = PI * argValBuff[i][5] / 180.0f;
            ry = PI * argValBuff[i][6] / 180.0f;
            rz = PI * argValBuff[i][7] / 180.0f;
            CVector3_f_ rotation(rx, ry, rz);
            object->SetRotation(rotation);
        }

        if (TEIGI_SKY[0] == (int) argValBuff[i][0]) {
            strcpy(path[0], "sim:");
            strcat(path[0], argStrBuff[i]);
            LoadFile(path[0], (void *) read_buffer, 0);
            OP_SkyFrame = LoadMDSFile(read_buffer, &MapDataBuffer, 2, 0, 0);

            if (argValBuff[i][2] == 1.0f) {
                OP_SkyFrame->SetAttr(attr, 1, 64);
            }
        }

        if (TEIGI_BLD[0] == (int) argValBuff[i][0]) {
            saved_attr = attr;

            if (argValBuff[i][8] == 0.0f) {
                strcpy(path[0], "sim:");
                strcat(path[0], argStrBuff[i]);
                LoadFile(path[0], (void *) read_buffer, 0);
                frame = LoadMDSFile(read_buffer, &MapDataBuffer, 2, 0, 0);
                rot_frame = frame;
                frame->SetAttr(attr, 1, 64);
                SetFrameAttr(frame, 1);
                object = OP_BuildingMap.SetObject(nowObjCnt++, frame, 0, 0);
                object->handle = 1;
                object->unk_34 = 5;
                CVector3_f_ position(10.0f * argValBuff[i][2], 10.0f * argValBuff[i][3], 10.0f * argValBuff[i][4]);
                object->SetPosition(position);
                rx = PI * argValBuff[i][5] / 180.0f;
                ry = PI * argValBuff[i][6] / 180.0f;
                rz = PI * argValBuff[i][7] / 180.0f;
                CVector3_f_ rotation(rx, ry, rz);
                object->SetRotation(rotation);
            } else {
                for (j = 0; j < 4; j++) {
                    if (argStrBuff[i + j][0] != 0) {
                        strcpy(path[j], "sim:");
                        strcat(path[j], argStrBuff[i + j]);
                        LODNameBuff[j] = path[j];
                    } else {
                        LODNameBuff[j] = 0;
                    }
                }

                LoadLODData(lod_frames, LODNameBuff, read_buffer, 0);

                object = OP_BuildingMap.GetObject(nowObjCnt++);
                object->handle = 1;
                object->unk_34 = 1;

                CategoryAttr *category = &OP_BuildingMap.category[1];

                for (k = 0; k < 4; k++) {
                    category->lod[k] = levelOfDitialZ[k];
                }

                category->lowest = 0;
                category->highest = 3;

                for (j = 0; j < 4; j++) {
                    object->SetFrame(lod_frames[j], j);

                    if (lod_frames[j] != 0) {
                        lod_frames[j]->SetAttr(attr, 1, 64);
                        SetFrameAttr(lod_frames[j], 1);
                        shadow = frame->SearchFrame("shadow");

                        if (shadow != 0) {
                            attr.alpha_ref = 1;
                            shadow->SetAttr(attr, 1, 64);
                            SetFrameAttr(shadow, 1);
                        }
                    }
                }

                CVector3_f_ position(10.0f * argValBuff[i][2], 10.0f * argValBuff[i][3], 10.0f * argValBuff[i][4]);
                object->SetPosition(position);
                rx = PI * argValBuff[i][5] / 180.0f;
                ry = PI * argValBuff[i][6] / 180.0f;
                rz = PI * argValBuff[i][7] / 180.0f;
                CVector3_f_ rotation(rx, ry, rz);
                object->SetRotation(rotation);
            }

            attr = saved_attr;
        }

        if (TEIGI_UPER[0] == (int) argValBuff[i][0]) {
            saved_attr = attr;

            if (argValBuff[i][8] == 0.0f) {
                strcpy(path[0], "sim:");
                strcat(path[0], argStrBuff[i]);
                LoadFile(path[0], (void *) read_buffer, 0);
                frame = LoadMDSFile(read_buffer, &MapDataBuffer, 2, 0, 0);
                frame->SetAttr(attr, 1, 64);
                SetFrameAttr(frame, 1);
                object = OP_BuildingMap2.SetObject(nowObjCnt2++, frame, 0, 0);
                object->handle = 1;
                object->unk_34 = 5;
                CVector3_f_ position(10.0f * argValBuff[i][2], 10.0f * argValBuff[i][3], 10.0f * argValBuff[i][4]);
                object->SetPosition(position);
                rx = PI * argValBuff[i][5] / 180.0f;
                ry = PI * argValBuff[i][6] / 180.0f;
                rz = PI * argValBuff[i][7] / 180.0f;
                CVector3_f_ rotation(rx, ry, rz);
                object->SetRotation(rotation);
            } else {
                for (j = 0; j < 4; j++) {
                    if (argStrBuff[i + j][0] != 0) {
                        strcpy(path[j], "sim:");
                        strcat(path[j], argStrBuff[i + j]);
                        LODNameBuff[j] = path[j];
                    } else {
                        LODNameBuff[j] = 0;
                    }
                }

                LoadLODData(lod_frames, LODNameBuff, read_buffer, 0);

                object = OP_BuildingMap2.GetObject(nowObjCnt++);
                object->handle = 1;
                object->unk_34 = 1;

                CategoryAttr *category = &OP_BuildingMap2.category[1];

                for (k = 0; k < 4; k++) {
                    category->lod[k] = levelOfDitialZ[k];
                }

                category->lowest = 0;
                category->highest = 3;

                for (j = 0; j < 4; j++) {
                    object->SetFrame(lod_frames[j], j);

                    if (lod_frames[j] != 0) {
                        lod_frames[j]->SetAttr(attr, 1, 64);
                        SetFrameAttr(lod_frames[j], 1);
                        shadow = frame->SearchFrame("shadow");

                        if (shadow != 0) {
                            attr.alpha_ref = 1;
                            shadow->SetAttr(attr, 1, 64);
                            SetFrameAttr(shadow, 1);
                        }
                    }
                }

                CVector3_f_ position(10.0f * argValBuff[i][2], 10.0f * argValBuff[i][3], 10.0f * argValBuff[i][4]);
                object->SetPosition(position);
                rx = PI * argValBuff[i][5] / 180.0f;
                ry = PI * argValBuff[i][6] / 180.0f;
                rz = PI * argValBuff[i][7] / 180.0f;
                CVector3_f_ rotation(rx, ry, rz);
                object->SetRotation(rotation);
            }

            attr = saved_attr;
        }

        if (TEIGI_S_VOLUME[0] == (int) argValBuff[i][0]) {
            strcpy(path[0], "sim:");
            strcat(path[0], argStrBuff[i]);
            LoadFile(path[0], (void *) read_buffer, 0);
            ShadowModel[shadowModelCount] = LoadMDSFile(read_buffer, 14, 0);
            ShadowModel[shadowModelCount]->SetPosition(10.0f * argValBuff[i][2], 10.0f * argValBuff[i][3], 10.0f * argValBuff[i][4]);
            rx = PI * argValBuff[i][5] / 180.0f;
            ry = PI * argValBuff[i][6] / 180.0f;
            rz = PI * argValBuff[i][7] / 180.0f;
            ShadowModel[shadowModelCount]->SetRotation(rx, ry, rz);
            shadowModelCount++;
        }

        if (TEIGI_ADD_CRD[0] == (int) argValBuff[i][0]) {
            strcpy(path[0], "sim:");
            strcat(path[0], argStrBuff[i]);
            LoadFile(path[0], (void *) read_buffer, 0);
            ColModel[ColModelCount] = LoadCollisionFile(read_buffer);
            ColModel[ColModelCount]->SetPosition(10.0f * argValBuff[i][2], 10.0f * argValBuff[i][3], 10.0f * argValBuff[i][4]);
            rx = PI * argValBuff[i][5] / 180.0f;
            ry = PI * argValBuff[i][6] / 180.0f;
            rz = PI * argValBuff[i][7] / 180.0f;
            ColModel[ColModelCount]->SetRotation(rx, ry, rz);
            ColModelCount++;
        }

        if (TEIGI_PROJECTION[0] == (int) argValBuff[i][0]) {
            Projection = argValBuff[i][1];
        }

        if (TEIGI_OBJ_ROT[0] == (int) argValBuff[i][0]) {
            OP_AnimeSeq[OP_AnimeSeqRot].Initialize();
            OP_AnimeSeq[OP_AnimeSeqRot].type = 0;
            OP_AnimeSeq[OP_AnimeSeqRot].number = 0;
            OP_AnimeSeq[OP_AnimeSeqRot].start[0] = argValBuff[i][2];
            OP_AnimeSeq[OP_AnimeSeqRot].start[1] = argValBuff[i][3];
            OP_AnimeSeq[OP_AnimeSeqRot].start[2] = argValBuff[i][4];
            OP_AnimeSeq[OP_AnimeSeqRot].step_x = argValBuff[i][5];
            OP_AnimeSeq[OP_AnimeSeqRot].step_y = argValBuff[i][6];
            OP_AnimeSeq[OP_AnimeSeqRot].step_z = argValBuff[i][7];
            strcpy(OP_AnimeSeq[OP_AnimeSeqRot].name, argStrBuff[i]);
            InitObjAnime(rot_frame, &OP_AnimeSeq[OP_AnimeSeqRot]);
            OP_AnimeSeqRot++;
        }

        if (TEIGI_FIRE[0] == (int) argValBuff[i][0]) {
            OP_FirePosition[OP_FireList][0] = argValBuff[i][1];
            OP_FirePosition[OP_FireList][1] = argValBuff[i][2];
            OP_FirePosition[OP_FireList][2] = argValBuff[i][3];
            OP_FireScale[OP_FireList] = argValBuff[i][4];
            OP_FireFlg[OP_FireList] = (int) argValBuff[i][5];
            OP_FireList++;
        }

        if (TEIGI_DEF_ENDS[0] == (int) argValBuff[i][0]) {
            nowPartsCnt++;
        }
    }
}

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
