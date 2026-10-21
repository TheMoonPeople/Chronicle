
#include "common.h"

#include <cstdlib>
#include <cstring>
#include <iterator>

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

namespace {
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
} // namespace

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

namespace {
static ClsMes    Mes1;
static CDispFade DispFade;
} // namespace

extern u_char *PassReadBuffer;
extern u_char *MesBuffer;
extern int     OpBgmSqPort;

namespace {
static tagFRAME_INF *frame_info_cam;
} // namespace

extern int SceneNp__2;
extern int Pause;

namespace {
static int    CameraMode;
static int    SceneRp;
static int    SceneCnt;
static int    SceneFlg;
static int    SceneSw;
static float  PauseFrame;
static u_char End;
static int    EndCnt;
static int    BgmOff;
static int    BgmVol;
static int    BgmNo;
} // namespace

PORT_OVERRIDE void OpeningInit() {
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
    OP_MainCamera.SetRef(nullptr, 0.0f, 0.0f, 0.0f);
    OP_MainCamera.SetPos(nullptr, 0.0f, 0.0f, 0.0f);
    OP_MainCamera.SetDistance(80.0f);
    OP_MainCamera.SetHeight(0.0f);
    OP_MainCamera.SetFollow(0.0f, 0.0f, 0.0f);
    OP_MainCamera.Step(0);
    OP_MainCamera.SetSpeed(0.0f);
    MGSetRenderInfo(800.0f, 6.0f, 65535);
    wait_now_loading_vsync();
    CScript__2.Load("opdat/opening.scr");
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
namespace {
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
    }

    Mes1.buff = reinterpret_cast<short *>(MesBuffer);
    Mes1.text = reinterpret_cast<char *>(MesBuffer);
    Mes1.text += *reinterpret_cast<short *>(MesBuffer + 2);
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
        LoadFile(files[i][0], reinterpret_cast<void *>(read_buffer), nullptr);
        Cam__2[i].LoadPackData(read_buffer, files[i][1], &PassDataBuffer[i], nullptr);
        Cam__2[i].motion_type.state.time = 1.0f;
        Cam__2[i].motion_type.state.blend_step = 1.0f;
        Cam__2[i].motion_type.state.motion_no = 0;
        Cam__2[i].motion_type.state.playing_no = 0;
        Cam__2[i].motion_type.state.camera = &OP_MainCamera;
    }
}
} // namespace

PORT_OVERRIDE void LoadSceneBG() {
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

            LoadFileBG(files[SceneCnt][0], reinterpret_cast<u_long128 *>(PassReadBuffer), nullptr);
            SceneFlg = SCENE_READ_WAIT;
            break;
        case SCENE_READ_WAIT:
            if (ReadBGSync()) {
                break;
            }

            slot = SceneRp;
            buffer = &PassDataBuffer[slot];
            buffer->used = 0;
            Cam__2[slot].LoadPackData(reinterpret_cast<u_int *>(PassReadBuffer), files[SceneCnt][1], buffer, nullptr);
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

PORT_OVERRIDE int OpeningLoop() {
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
namespace {
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
    static int                  flg = 0;
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
            Cam__2[SceneNp__2].motion_type.state.time = static_cast<float>(CScript__2.motion_start);
        } else {
            while (ReadBGSync())
                ;
        }
    }

    if (Cam__2[SceneNp__2].motion_type.state.time > static_cast<float>(CScript__2.motion_end - 1)) {
        Cam__2[SceneNp__2].motion_type.state.time = static_cast<float>(CScript__2.motion_end - 1);
    }

    if (CameraMode == 0) {
        Cam__2[SceneNp__2].motion_type.state.camera = &OP_MainCamera;

        if (PauseFrame > static_cast<float>(CScript__2.motion_end - 1)) {
            PauseFrame = static_cast<float>(CScript__2.motion_end - 1);
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
                CSnd.Fade(MIDI_PORT_BGM, static_cast<float>(CScript__2.bgm_fade) / 2.0f, CScript__2.se_fade_time);
                CSnd.Fade(MIDI_PORT_AMBIENT, static_cast<float>(CScript__2.bgm_fade), CScript__2.se_fade_time);
                CSnd.Fade(MIDI_PORT_UNK_2, static_cast<float>(CScript__2.bgm_fade), CScript__2.se_fade_time);
                CSnd.Fade(MIDI_PORT_SE_TITLE, static_cast<float>(CScript__2.bgm_fade), CScript__2.se_fade_time);
                CSnd.Fade(MIDI_PORT_SE_DEFAULT, static_cast<float>(CScript__2.bgm_fade), CScript__2.se_fade_time);
                CSnd.Fade(MIDI_PORT_UNK_D, static_cast<float>(CScript__2.bgm_fade), CScript__2.se_fade_time);
                CSnd.Fade(MIDI_PORT_SE_SPECIAL, static_cast<float>(CScript__2.bgm_fade), CScript__2.se_fade_time);
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

        // Not every opening scene loads a pause texture (the last scenes load none), so the
        // overlay is only drawn when the language's texture is actually present (#113).
        CTexture *pause_sprite = nullptr;

        switch (LanguageCode) {
            case LANG_JAPANESE:
                pause_sprite = TexManager.GetTexture("pause", -1);
                break;
            case LANG_ENGLISH_US:
                pause_sprite = TexManager.GetTexture("pause_e", -1);
                break;
            case LANG_ENGLISH_UK:
                pause_sprite = TexManager.GetTexture("pause_e", -1);
                break;
            case LANG_FRENCH:
                pause_sprite = TexManager.GetTexture("pause_f", -1);
                break;
            case LANG_GERMAN:
                pause_sprite = TexManager.GetTexture("pause_g", -1);
                break;
            case LANG_ITALIAN:
                pause_sprite = TexManager.GetTexture("pause_i", -1);
                break;
            case LANG_SPANISH:
                pause_sprite = TexManager.GetTexture("pause_s", -1);
                break;
        }

        if (pause_sprite != nullptr) {
            set2DSprite(GetVif1Packet(), pause_sprite, CRect<int>(256, 160, 128, 128), CRect<int>(0, 0, 128, 128), 128);
        }

        setbilinear(1);
    }

    MGGetFBuffBackTex(&tex0);

    CTexture texture;

    texture.tex0 = *reinterpret_cast<u_long *>(&tex0);
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
} // namespace

PORT_OVERRIDE void OpBgmPlay() {
    int volumes[8] = {82, 106, 64, 69, 91, 92, 95, 108};

    if (BgmOff == 0) {
        CSnd.SQ_Play(MIDI_PORT_BGM, 0);
    } else {
        CSnd.SQ_Play(MIDI_PORT_BGM, 0, 0);
    }

    BgmVol = volumes[BgmNo];
    BgmNo++;
}

#include "common.h"

#include <libvu0.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <memory>
#include <new>
#include <string>

#include "camera.hpp"
#include "camerafollow.hpp"
#include "character.hpp"
#include "dataalloc.hpp"
#include "dataread.hpp"
#include "dataread_port.hpp"
#include "frame.hpp"
#include "framevu1.hpp"
#include "main.hpp"
#include "map.hpp"
#include "mapobject.hpp"
#include "mds.hpp"
#include "mglib.hpp"
#include "objanime.hpp"
#include "platform/arena.hpp"
#include "title/op_a.hpp"
#include "title/opening.hpp"
#include "title/script.hpp"
#include "vector.hpp"

// The bodies are retail's, which pass string literals as char *.
#pragma clang diagnostic ignored "-Wwritable-strings"

// Retail opening.cpp's scene-definition reader: OPMdsLoad places the scenery in op_a's maps and
// registers op_a's object animations, which the port lays out with the host's CMap and
// OBJ_ANIME_SEQ where opening.cpp declares PS2-sized ones of its own. OPAnalyz and the parse
// state move with it.

namespace {
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
static int         argLevel;
static int         teigiFileSize;
static std::size_t argCapacity;

/* The definition file is read straight into this one off the disc, and the drive transfers by
   DMA into whole cache lines. */
static std::unique_ptr<std::array<char, 64>[]>  argStrBuff;
static std::unique_ptr<std::array<float, 64>[]> argValBuff;

/* The four model names one level-of-detail object is built from, as the loader hands them to the
   loader below it: a null entry is a level the file left out. */
static char *LODNameBuff[4];

static int nowObjCnt;
static int nowObjCnt2;
static int nowPartsCnt;

static int skipSpace(char *buf, int pos);
static int checkArg(char *buf, int pos, int *command);
} // namespace

namespace {

struct AlignedBufferDeleter {
    void operator()(char *buffer) const noexcept {
        ::operator delete[](buffer, std::align_val_t{64});
    }
};

} // namespace

/* Read one definition file and turn it into rows of arguments the loader can walk. Every line
   ending becomes a pair of NULs first, so the rest of the file is one long run of tokens with no
   line structure left in it; each command the text names is then matched by its keyword, its
   arguments are checked into the buffers at the next level, and the level moves on. A line naming
   no command at all is fatal, because a definition file the loader half-understands would place
   half a scene. */
PORT_OVERRIDE void OPAnalyz(char *name) {
    std::unique_ptr<char[], AlignedBufferDeleter> definition_buffer;
    char                                         *buffer;
    int                                           i;
    int                                           position;
    int                                           matched;

    argLevel = 0;
    const std::uintmax_t definition_size = DataFileSize(name);

    if (definition_size > std::numeric_limits<int>::max()) {
        ArenaOverflow("title definition file", std::numeric_limits<int>::max(), std::numeric_limits<int>::max());
    }

    const std::size_t buffer_capacity = static_cast<std::size_t>((definition_size + 0x7FF) & ~std::uintmax_t{0x7FF}) + 128;
    definition_buffer.reset(static_cast<char *>(::operator new[](buffer_capacity, std::align_val_t{64})));
    buffer = definition_buffer.get();

    if (LoadFile(name, buffer, &teigiFileSize) == 0) {
        return;
    }

    if (teigiFileSize < 0 || static_cast<std::size_t>(teigiFileSize) > buffer_capacity) {
        exit__2(-1);
    }

    argCapacity = std::min<std::size_t>(128, static_cast<std::size_t>(std::max(teigiFileSize, 1)));
    argStrBuff = std::make_unique<std::array<char, 64>[]>(argCapacity);
    argValBuff = std::make_unique<std::array<float, 64>[]>(argCapacity);

    std::memset(buffer + teigiFileSize, 0, buffer_capacity - static_cast<std::size_t>(teigiFileSize));

    for (i = 0; i < teigiFileSize; i++) {
        if (i + 1 < teigiFileSize && buffer[i] == 13 && buffer[i + 1] == 10) {
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
        if (static_cast<std::size_t>(argLevel) >= argCapacity) {
            exit__2(-1);
        }

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

            if (static_cast<std::size_t>(argLevel) >= argCapacity) {
                exit__2(-1);
            }

            argValBuff[argLevel][0] = static_cast<float>(TEIGI_DEF_PATS[0]);
            matched = 1;
            argLevel++;
        }

        if (memcmp(&buffer[position], "DEF_ENDS", 8) == 0) {
            position = skipSpace(buffer, position + 8);

            if (static_cast<std::size_t>(argLevel) >= argCapacity) {
                exit__2(-1);
            }

            argValBuff[argLevel][0] = static_cast<float>(TEIGI_DEF_ENDS[0]);
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
PORT_OVERRIDE void OPMdsLoad() {
    CFrameAttr                 saved_attr;
    CFrameAttr                 attr;
    std::array<std::string, 4> path;
    CFrameVu1                 *lod_frames[4];
    sceVu0FVECTOR              light_dir;
    int                        i;
    int                        j;
    int                        k;
    int                        light_no;
    int                        light_slot;
    CFrameVu1                 *frame;
    CFrameVu1                 *rot_frame;
    CMapObject                *object;
    CFrame                    *shadow;
    float                      rx;
    float                      ry;
    float                      rz;
    float                      light_x;

    attr.clip_depth = 20.0f;
    attr.clip_enable = true;
    attr.program_option = 0;

    nowObjCnt = 0;
    nowObjCnt2 = 0;

    for (i = 0; i < argLevel; i++) {
        if (TEIGI_AnimeSpeed[0] == static_cast<int>(argValBuff[i][0])) {
            animeSpeed1 = static_cast<int>(argValBuff[i][1]);
            animeSpeed2 = static_cast<int>(argValBuff[i][2]);
        }

        if (TEIGI_DebugFlag[0] == static_cast<int>(argValBuff[i][0])) {
            debugModeFlag = static_cast<int>(argValBuff[i][1]);
        }

        if (TEIGI_LEVEL_FAR[0] == static_cast<int>(argValBuff[i][0])) {
            levelOfDitialZ[0] = argValBuff[i][1];
            levelOfDitialZ[1] = argValBuff[i][2];
            levelOfDitialZ[2] = argValBuff[i][3];
            levelOfDitialZ[3] = argValBuff[i][4];
        }

        if (TEIGI_WATER_PARAM[0] == static_cast<int>(argValBuff[i][0])) {
            WaterMeshW = static_cast<int>(argValBuff[i][1]);
            WaterMeshH = static_cast<int>(argValBuff[i][2]);
            WaterShake = argValBuff[i][3];
            WaterCourant = argValBuff[i][4];
            WaterDecline = argValBuff[i][5];
            WaterAmplitude = argValBuff[i][6];
            WaterRefraction = argValBuff[i][7];
        }

        if (TEIGI_WATER_SET[0] == static_cast<int>(argValBuff[i][0])) {
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

        if (TEIGI_WATER_RGB[0] == static_cast<int>(argValBuff[i][0])) {
            WaterR = argValBuff[i][1];
            WaterG = argValBuff[i][2];
            WaterB = argValBuff[i][3];
        }

        if (TEIGI_RUN_SPEED[0] == static_cast<int>(argValBuff[i][0])) {
            run_speed = argValBuff[i][1];
        }

        if (TEIGI_NORMALCLIP_OFF[0] == static_cast<int>(argValBuff[i][0])) {
            if (argValBuff[i][1] == 0.0f) {
                attr.program_option = 0;
            } else {
                attr.program_option = 1;
            }
        }

        if (TEIGI_BG_COL2[0] == static_cast<int>(argValBuff[i][0])) {
            MGSetBGColor(static_cast<float>(static_cast<u_int>(argValBuff[i][1])), static_cast<float>(static_cast<u_int>(argValBuff[i][2])), static_cast<float>(static_cast<u_int>(argValBuff[i][3])), 128.0f);
        }

        if (TEIGI_BG_COL[0] == static_cast<int>(argValBuff[i][0])) {
            MGSetBGColor(static_cast<float>(static_cast<u_int>(argValBuff[i][1])), static_cast<float>(static_cast<u_int>(argValBuff[i][2])), static_cast<float>(static_cast<u_int>(argValBuff[i][3])), 128.0f);
        }

        if (TEIGI_FARCLIP[0] == static_cast<int>(argValBuff[i][0])) {
            attr.far_clip_enable = true;
            attr.far_clip = argValBuff[i][1];
        }

        if (TEIGI_LIGHT_COL[0] == static_cast<int>(argValBuff[i][0])) {
            light_no = static_cast<int>(argValBuff[i][7]);
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

        if (TEIGI_AMBIENT[0] == static_cast<int>(argValBuff[i][0])) {
            ambientlight[0] = argValBuff[i][1];
            ambientlight[1] = argValBuff[i][2];
            ambientlight[2] = argValBuff[i][3];
            MGSetAmbient(ambientlight);
        }

        if (TEIGI_CRD[0] == static_cast<int>(argValBuff[i][0])) {
            path[0] = std::string("sim:") + argStrBuff[i].data();
            LoadFile(path[0].data(), reinterpret_cast<void *>(read_buffer), nullptr);
            OP_GroundCol = LoadCollisionFile(read_buffer);
        }

        if (TEIGI_FOG[0] == static_cast<int>(argValBuff[i][0])) {
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

        if (TEIGI_EDIT_FOG[0] == static_cast<int>(argValBuff[i][0])) {
            editFogRate[0] = argValBuff[i][1];
            editFogRate[1] = argValBuff[i][2];
            editFogRate[2] = argValBuff[i][6];
            editFogRate[3] = argValBuff[i][7];
            editFogColor[0] = argValBuff[i][3];
            editFogColor[1] = argValBuff[i][4];
            editFogColor[2] = argValBuff[i][5];
            attr.fog_enable = true;
        }

        if (TEIGI_PLIGHT[0] == static_cast<int>(argValBuff[i][0])) {
            if (pointLightStack >= 96) {
                exit__2(-1);
            }

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

        if (TEIGI_GRD[0] == static_cast<int>(argValBuff[i][0])) {
            path[0] = std::string("sim:") + argStrBuff[i].data();
            LoadFile(path[0].data(), reinterpret_cast<void *>(read_buffer), nullptr);
            frame = LoadMDSFile(read_buffer, &MapDataBuffer, 2, nullptr, nullptr);
            rot_frame = frame;
            frame->SetAttr(attr, 1, 64);
            SetFrameAttr(frame, 1);
            object = OP_GroundMap.SetObject(frame, 0, 0);
            if (object == nullptr) {
                exit__2(-1);
            }
            CVector3_f_ position(10.0f * argValBuff[i][2], 10.0f * argValBuff[i][3], 10.0f * argValBuff[i][4]);
            object->SetPosition(position);
            rx = PI * argValBuff[i][5] / 180.0f;
            ry = PI * argValBuff[i][6] / 180.0f;
            rz = PI * argValBuff[i][7] / 180.0f;
            CVector3_f_ rotation(rx, ry, rz);
            object->SetRotation(rotation);
        }

        if (TEIGI_SKY[0] == static_cast<int>(argValBuff[i][0])) {
            path[0] = std::string("sim:") + argStrBuff[i].data();
            LoadFile(path[0].data(), reinterpret_cast<void *>(read_buffer), nullptr);
            OP_SkyFrame = LoadMDSFile(read_buffer, &MapDataBuffer, 2, nullptr, nullptr);

            if (argValBuff[i][2] == 1.0f) {
                OP_SkyFrame->SetAttr(attr, 1, 64);
            }
        }

        if (TEIGI_BLD[0] == static_cast<int>(argValBuff[i][0])) {
            saved_attr = attr;

            if (argValBuff[i][8] == 0.0f) {
                if (nowObjCnt >= static_cast<int>(std::size(OP_BuildingMap.object))) {
                    exit__2(-1);
                }

                path[0] = std::string("sim:") + argStrBuff[i].data();
                LoadFile(path[0].data(), reinterpret_cast<void *>(read_buffer), nullptr);
                frame = LoadMDSFile(read_buffer, &MapDataBuffer, 2, nullptr, nullptr);
                rot_frame = frame;
                frame->SetAttr(attr, 1, 64);
                SetFrameAttr(frame, 1);
                object = OP_BuildingMap.SetObject(nowObjCnt++, frame, 0, 0);
                object->handle = 1;
                object->category_no = 5;
                CVector3_f_ position(10.0f * argValBuff[i][2], 10.0f * argValBuff[i][3], 10.0f * argValBuff[i][4]);
                object->SetPosition(position);
                rx = PI * argValBuff[i][5] / 180.0f;
                ry = PI * argValBuff[i][6] / 180.0f;
                rz = PI * argValBuff[i][7] / 180.0f;
                CVector3_f_ rotation(rx, ry, rz);
                object->SetRotation(rotation);
            } else {
                if (nowObjCnt >= static_cast<int>(std::size(OP_BuildingMap.object))) {
                    exit__2(-1);
                }

                for (j = 0; j < 4; j++) {
                    if (static_cast<std::size_t>(i + j) < argCapacity && i + j < argLevel &&
                        argStrBuff[i + j][0] != 0) {
                        path[j] = std::string("sim:") + argStrBuff[i + j].data();
                        LODNameBuff[j] = path[j].data();
                    } else {
                        LODNameBuff[j] = nullptr;
                    }
                }

                LoadLODData(lod_frames, LODNameBuff, read_buffer, 0);

                object = OP_BuildingMap.GetObject(nowObjCnt++);
                if (object == nullptr) {
                    exit__2(-1);
                }
                object->handle = 1;
                object->category_no = 1;

                CMapCategoryAttr *category = &OP_BuildingMap.category[1];

                for (k = 0; k < 4; k++) {
                    category->lod[k] = levelOfDitialZ[k];
                }

                category->lowest = 0;
                category->highest = 3;

                for (j = 0; j < 4; j++) {
                    object->SetFrame(lod_frames[j], j);

                    if (lod_frames[j] != nullptr) {
                        lod_frames[j]->SetAttr(attr, 1, 64);
                        SetFrameAttr(lod_frames[j], 1);
                        shadow = lod_frames[j]->SearchFrame("shadow");

                        if (shadow != nullptr) {
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

        if (TEIGI_UPER[0] == static_cast<int>(argValBuff[i][0])) {
            saved_attr = attr;

            if (argValBuff[i][8] == 0.0f) {
                if (nowObjCnt2 >= static_cast<int>(std::size(OP_BuildingMap2.object))) {
                    exit__2(-1);
                }

                path[0] = std::string("sim:") + argStrBuff[i].data();
                LoadFile(path[0].data(), reinterpret_cast<void *>(read_buffer), nullptr);
                frame = LoadMDSFile(read_buffer, &MapDataBuffer, 2, nullptr, nullptr);
                frame->SetAttr(attr, 1, 64);
                SetFrameAttr(frame, 1);
                object = OP_BuildingMap2.SetObject(nowObjCnt2++, frame, 0, 0);
                object->handle = 1;
                object->category_no = 5;
                CVector3_f_ position(10.0f * argValBuff[i][2], 10.0f * argValBuff[i][3], 10.0f * argValBuff[i][4]);
                object->SetPosition(position);
                rx = PI * argValBuff[i][5] / 180.0f;
                ry = PI * argValBuff[i][6] / 180.0f;
                rz = PI * argValBuff[i][7] / 180.0f;
                CVector3_f_ rotation(rx, ry, rz);
                object->SetRotation(rotation);
            } else {
                if (nowObjCnt2 >= static_cast<int>(std::size(OP_BuildingMap2.object))) {
                    exit__2(-1);
                }

                for (j = 0; j < 4; j++) {
                    if (static_cast<std::size_t>(i + j) < argCapacity && i + j < argLevel &&
                        argStrBuff[i + j][0] != 0) {
                        path[j] = std::string("sim:") + argStrBuff[i + j].data();
                        LODNameBuff[j] = path[j].data();
                    } else {
                        LODNameBuff[j] = nullptr;
                    }
                }

                LoadLODData(lod_frames, LODNameBuff, read_buffer, 0);

                object = OP_BuildingMap2.GetObject(nowObjCnt2++);
                if (object == nullptr) {
                    exit__2(-1);
                }
                object->handle = 1;
                object->category_no = 1;

                CMapCategoryAttr *category = &OP_BuildingMap2.category[1];

                for (k = 0; k < 4; k++) {
                    category->lod[k] = levelOfDitialZ[k];
                }

                category->lowest = 0;
                category->highest = 3;

                for (j = 0; j < 4; j++) {
                    object->SetFrame(lod_frames[j], j);

                    if (lod_frames[j] != nullptr) {
                        lod_frames[j]->SetAttr(attr, 1, 64);
                        SetFrameAttr(lod_frames[j], 1);
                        shadow = lod_frames[j]->SearchFrame("shadow");

                        if (shadow != nullptr) {
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

        if (TEIGI_S_VOLUME[0] == static_cast<int>(argValBuff[i][0])) {
            if (shadowModelCount >= static_cast<int>(std::size(ShadowModel))) {
                exit__2(-1);
            }

            path[0] = std::string("sim:") + argStrBuff[i].data();
            LoadFile(path[0].data(), reinterpret_cast<void *>(read_buffer), nullptr);
            ShadowModel[shadowModelCount] = LoadMDSFile(read_buffer, 14, 0);
            ShadowModel[shadowModelCount]->SetPosition(10.0f * argValBuff[i][2], 10.0f * argValBuff[i][3], 10.0f * argValBuff[i][4]);
            rx = PI * argValBuff[i][5] / 180.0f;
            ry = PI * argValBuff[i][6] / 180.0f;
            rz = PI * argValBuff[i][7] / 180.0f;
            ShadowModel[shadowModelCount]->SetRotation(rx, ry, rz);
            shadowModelCount++;
        }

        if (TEIGI_ADD_CRD[0] == static_cast<int>(argValBuff[i][0])) {
            if (ColModelCount >= static_cast<int>(std::size(ColModel))) {
                exit__2(-1);
            }

            path[0] = std::string("sim:") + argStrBuff[i].data();
            LoadFile(path[0].data(), reinterpret_cast<void *>(read_buffer), nullptr);
            ColModel[ColModelCount] = LoadCollisionFile(read_buffer);
            ColModel[ColModelCount]->SetPosition(10.0f * argValBuff[i][2], 10.0f * argValBuff[i][3], 10.0f * argValBuff[i][4]);
            rx = PI * argValBuff[i][5] / 180.0f;
            ry = PI * argValBuff[i][6] / 180.0f;
            rz = PI * argValBuff[i][7] / 180.0f;
            ColModel[ColModelCount]->SetRotation(rx, ry, rz);
            ColModelCount++;
        }

        if (TEIGI_PROJECTION[0] == static_cast<int>(argValBuff[i][0])) {
            Projection = argValBuff[i][1];
        }

        if (TEIGI_OBJ_ROT[0] == static_cast<int>(argValBuff[i][0])) {
            if (OP_AnimeSeqRot < 0 || OP_AnimeSeqRot >= 32) {
                exit__2(-1);
            }

            OP_AnimeSeq[OP_AnimeSeqRot].Initialize();
            OP_AnimeSeq[OP_AnimeSeqRot].property = 0;
            OP_AnimeSeq[OP_AnimeSeqRot].mode = 0;
            OP_AnimeSeq[OP_AnimeSeqRot].from[0] = argValBuff[i][2];
            OP_AnimeSeq[OP_AnimeSeqRot].from[1] = argValBuff[i][3];
            OP_AnimeSeq[OP_AnimeSeqRot].from[2] = argValBuff[i][4];
            OP_AnimeSeq[OP_AnimeSeqRot].step[0] = argValBuff[i][5];
            OP_AnimeSeq[OP_AnimeSeqRot].step[1] = argValBuff[i][6];
            OP_AnimeSeq[OP_AnimeSeqRot].step[2] = argValBuff[i][7];
            const std::size_t frame_name_length = std::strlen(argStrBuff[i].data());
            if (frame_name_length >= sizeof(OP_AnimeSeq[OP_AnimeSeqRot].frame_name)) {
                exit__2(-1);
            }
            std::memcpy(OP_AnimeSeq[OP_AnimeSeqRot].frame_name, argStrBuff[i].data(), frame_name_length + 1);
            InitObjAnime(rot_frame, &OP_AnimeSeq[OP_AnimeSeqRot]);
            OP_AnimeSeqRot++;
        }

        if (TEIGI_FIRE[0] == static_cast<int>(argValBuff[i][0])) {
            if (OP_FireList < 0 || OP_FireList >= 96) {
                exit__2(-1);
            }

            OP_FirePosition[OP_FireList][0] = argValBuff[i][1];
            OP_FirePosition[OP_FireList][1] = argValBuff[i][2];
            OP_FirePosition[OP_FireList][2] = argValBuff[i][3];
            OP_FireScale[OP_FireList] = argValBuff[i][4];
            OP_FireFlg[OP_FireList] = static_cast<int>(argValBuff[i][5]);
            OP_FireList++;
        }

        if (TEIGI_DEF_ENDS[0] == static_cast<int>(argValBuff[i][0])) {
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
namespace {
static int skipSpace(char *buffer, int position) {
    int skipped;

    if (position < 0) {
        return teigiFileSize;
    }

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

    if (argLevel < 0 || static_cast<std::size_t>(argLevel) >= argCapacity || command[1] < 0 || command[1] > 63) {
        return -1;
    }

    cursor = position;

    if (command[1] == 0) {
        return position;
    }

    for (i = 0; i < command[1]; i++) {
        argValBuff[argLevel][0] = static_cast<float>(command[0]);

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

                    argValBuff[argLevel][1 + i] = static_cast<float>(atof(&buffer[cursor]));

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

                    argValBuff[argLevel][1 + i] = static_cast<float>(atof(&buffer[cursor]));

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
} // namespace
