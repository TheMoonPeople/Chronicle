
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

// The bodies are retail's, which pass string literals as char *.
#pragma clang diagnostic ignored "-Wwritable-strings"

// Retail's opening loop: OpeningLoop reaches the static DrawProcess, which uploaded the VU1 program,
// and DrawProcess shares the fade, the message window and the scene state with OpeningInit and the
// other static processes, so they move together.

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

extern CDataAlloc2<1> CharaDataBuffer__2[7];
extern CDataAlloc2<1> DummyBuffer;
extern CDataAlloc2<1> PassDataBuffer[3];
extern CDataAlloc2<1> MapDataBuffer;
extern CDataAlloc2<1> WaterBuffer__2;
extern CDataAlloc2<1> testBuffer;

extern CCharacter Chara__3[23];
extern char       CharaTex__2[23];

extern CCameraFollow OP_MainCamera;

extern CCharacter  Cam__2[3];
extern MOTION_INFO Op_MotionInfo;
static ClsMes      Mes1;
static CDispFade   DispFade;

extern u_char              *PassReadBuffer;
extern u_char              *MesBuffer;
extern int                  OpBgmSqPort;
static tagFRAME_INF *frame_info_cam;
extern int                  SceneNp__2;
extern int                  Pause;
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

PC_OVERRIDE void OpeningInit() {
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
    SetDataBuffer(&TextureData, 365000);
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
    CScript__2.Load("opdat/opening.pal");
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

PC_OVERRIDE void LoadSceneBG() {
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

    // Retail compares the pointer with the table's own merged "-1" literal.
    if (strcmp(files[SceneCnt][0], "-1") == 0) {
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

PC_OVERRIDE int OpeningLoop() {
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
    [[maybe_unused]] static int cnt = 0;

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

PC_OVERRIDE void OpBgmPlay() {
    int volumes[8] = {82, 106, 64, 69, 91, 92, 95, 108};

    if (BgmOff == 0) {
        CSnd.SQ_Play(MIDI_PORT_BGM, 0);
    } else {
        CSnd.SQ_Play(MIDI_PORT_BGM, 0, 0);
    }

    BgmVol = volumes[BgmNo];
    BgmNo++;
}
