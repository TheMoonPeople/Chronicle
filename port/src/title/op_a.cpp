#include "common.h"

#include <libgraph.h>
#include <libpkt.h>
#include <libvu0.h>

#include <cmath>
#include <cstdlib>

#include "camera.hpp"
#include "camerafollow.hpp"
#include "character.hpp"
#include "dataalloc.hpp"
#include "dataread.hpp"
#include "effect.hpp"
#include "effectgroup.hpp"
#include "fireomni.hpp"
#include "frame.hpp"
#include "framevu1.hpp"
#include "main.hpp"
#include "mainselect.hpp"
#include "map.hpp"
#include "mathutil.hpp"
#include "mds.hpp"
#include "mglib.hpp"
#include "objanime.hpp"
#include "renderinfo.hpp"
#include "sound.hpp"
#include "texture.hpp"
#include "title/op_a.hpp"
#include "title/opening.hpp"
#include "title/script.hpp"

// The bodies are retail's, which pass string literals as char *.
#pragma clang diagnostic ignored "-Wwritable-strings"

// Retail's dungeon-square scene: OpA_DrawProcess reaches the static setTexScroll and
// setCloudTexScroll, whose TEXFLUSH packets are gone (the copies are ordered on the renderer) and whose
// transfers are MoveImageTest's, and it shares its scenery state with the scene's set-up, sound and motion.

typedef MOTION_INFO tagMOTION_KEY;

/* The rectangle a texture transfer takes, declared here rather than reached through rect.h for the
   reason op_b.cpp declares its own: rect.h's four-argument constructor assigns h, w, y and x in
   that order and every rectangle this file builds assigns them the other way round. The default
   constructor keeps rect.h's order and is not dead code — the effect parameter block below carries
   one as a member, so it runs wherever a caller builds that block. */
template <class T>
class CRect {
public:
    T x; /**< Left edge in pixels. */
    T y; /**< Top edge in pixels. */
    T w; /**< Width in pixels. */
    T h; /**< Height in pixels. */

    CRect() {
        h = 0;
        w = 0;
        y = 0;
        x = 0;
    }

    CRect(T new_x, T new_y, T new_width, T new_height) {
        x = new_x;
        y = new_y;
        w = new_width;
        h = new_height;
    }
} __attribute__((aligned(16)));

/* One actor's face, as this scene animates it. The eyes and the mouth are two strips of frames
   stacked bottom-up in one 256-wide texture — the eyes down the left half and the mouth down the
   right — and a tick copies the current frame of each over the plate the model draws with. The two
   offsets are measured from the bottom edge of the plate; the blink state is kept here because this
   scene blinks the cast on a clock of its own rather than from the script. */
struct FACE_INFO {
    char *plate;        /**< Texture the model's face is drawn from. */
    char *strip;        /**< Texture holding the eye and mouth frames. */
    int   eye_bottom;   /**< Height of the eye region above the plate's bottom edge. */
    int   eye_height;   /**< Height of one eye frame. */
    int   mouth_bottom; /**< Height of the mouth region above the plate's bottom edge. */
    int   mouth_height; /**< Height of one mouth frame. */
    int   eye;          /**< Eye frame currently shown. */
    int   mouth;        /**< Mouth frame currently shown. */
    int   strip_bottom; /**< Row the frame strips count up from. */
    int   eye_max;      /**< Last eye frame of a blink. */
    int   blink;        /**< Blink phase: zero idle, one closing, two opening. */
};

void wait_now_loading_vsync();
void MoveImageTest(sceVif1Packet *packet, int sbp, int sbw, int spsm, const CRect<int> &rect, int dbp, int dbw, int dpsm, int dsax, int dsay, int dir);
void set2DSprite(sceVif1Packet *packet, CTexture *texture, const CRect<int> &src, const CRect<int> &dst, u_char alpha);
void DepthOfField(float *dist, int level, int alpha, int blur);

static void LoadTexture();
static void LoadData();
static void SetDanceMotion();
static void InitDancerPos();
static void DrawCloud();
static void SmokeProcess();
static void MoveDancers();
static void ReaderShadow();
static void DancerShadow();
static void ShogunShadow();
static void ShisaiShadow();
static void DrawShadow(float x, float y, float z);
static void setTexScroll();
static void setCloudTexScroll();
/* op_b.cpp owns these three; retail has this unit call across to them. */
void FaceChange(int actor_no);
void LoadCharaData(int buffer_no, int actor_no);
void LoadMotionData();

/* Each of the couple's motion files is one key range — the frame it starts at and the frame it ends
   at — written straight over the character's own first key, so that a file loaded in the background
   takes over without the motion driver being told anything. */
extern tagMOTION_KEY noroi[10];

extern tagMOTION_KEY dancer[10];

/* The scene's own world, and the objects the configuration file fills in. Both frame pointers are
   typed from the loader that writes them rather than from anything here: title/opdata assigns
   LoadCollisionFile's and LoadMDSFile's results to them, and nothing in this file reads either. */
CMap                 OP_BuildingMap;
CMap                 OP_BuildingMap2;
CMap                 OP_GroundMap;
OBJ_ANIME_SEQ        OP_AnimeSeq[32];
extern int           OP_AnimeSeqRot;
extern int           OP_FireList;
extern sceVu0FVECTOR OP_FirePosition[96];
extern float         OP_FireScale[96];
extern int           OP_FireFlg[96];
extern CFrameVu1    *OP_GroundCol;
extern CFrameVu1    *OP_SkyFrame;
extern CFrame       *OP_CharaFrame__2;
extern char          CloudFlag;

CFireOmni            CFire;
static sceVu0FVECTOR DancerPos[35];
static sceVu0FVECTOR DancerRot[35];
static CCharacter    Cloud;

static CFrameVu1   *Shadow;
static float        DancerAmb;
static CEffectGroup Smoke;
static CEffect     *EffectTable;
static float        DanceWait;
extern int                 DanceCnt;
extern int                 DanceStart;

/* The dungeon square's set-up. The two maps are cleared first because this scene is placed from a
   configuration file rather than from a table of its own, and the couple's second motion files are
   started in the background so that the first change of step does not wait for a read. */
void OpA_InitProcess() {
    OP_FireList = 0;
    OP_AnimeSeqRot = 0;
    OP_GroundMap.Initialize();
    OP_BuildingMap.Initialize();
    OPAnalyz("opdat/opinfo.cfg");
    LoadTexture();
    OPMdsLoad();
    LoadFile("opdat/dungeon/dungeon.pak", read_buffer, 0);
    wait_now_loading_vsync();
    LoadData();
    CSnd.SetReverb(0, 4, 80);
    CSnd.SetReverb(1, 4, 60);
    CSnd.LoadSoundFileFromPack("o01a.txt", read_buffer);
    CSnd.SetVol(MIDI_PORT_SE_TITLE, 256);
    CSnd.SetVol(MIDI_PORT_SE_DEFAULT, 256);
    CSnd.SetVol(MIDI_PORT_UNK_D, 256);
    CSnd.SetVol(MIDI_PORT_SE_SPECIAL, 256);
    CSnd.SE_Play(MIDI_PORT_SE_TITLE, 16, 20, 0, 0);
    wait_now_loading_vsync();
    SetDanceMotion();
    LoadFile("opdat/chara/01p19a1b.chr", (void *) ((char *) read_buffer + 0x10C900), 0);
    LoadFile("opdat/chara/01p17a1b.chr", (void *) read_buffer, 0);
    StartReadBG();
    DanceWait = 0.0f;
    DanceCnt = 1;
    DanceStart = 0;
    CloudFlag = 0;
}

/**
 * The scene's textures. Four rows name the fixed surfaces the registry keeps for every scene and
 * the twenty-five after them are filled in by name from the pack this function reads; the empty
 * name at the end is what states where the table stops.
 *
 * @mangled LoadTexture__Fv__3
 * @address 0x1DB5070
 * @size 0x3AC
 * @unknownret
 */
static void LoadTexture() {
    LOADTEXTURE_INFO2 texture_list[] = {
        {"#blender#640#" HALF_BUFFER_HEIGHT_STR "#4",       0,  0},
        {"#fontbase#512#256#1",                             26, 0},
        {"#fukidashibase#640#" HALF_BUFFER_HEIGHT_STR "#4", 26, 0},
        {"#frame_image#640#" HALF_BUFFER_HEIGHT_STR "#4",   22, 0},
        {0,                                                 26, 0},
        {0,                                                 26, 0},
        {0,                                                 26, 0},
        {0,                                                 1,  0},
        {0,                                                 1,  0},
        {0,                                                 6,  1},
        {0,                                                 2,  0},
        {0,                                                 4,  0},
        {0,                                                 4,  0},
        {0,                                                 3,  0},
        {0,                                                 3,  0},
        {0,                                                 5,  0},
        {0,                                                 5,  0},
        {0,                                                 7,  0},
        {0,                                                 8,  0},
        {0,                                                 8,  0},
        {0,                                                 8,  0},
        {0,                                                 10, 0},
        {0,                                                 10, 0},
        {0,                                                 11, 0},
        {0,                                                 0,  0},
        {0,                                                 19, 0},
        {0,                                                 19, 0},
        {0,                                                 6,  0},
        {"",                                                0,  0}
    };

    TexManager.Initialize(16352);
    LoadFile("opdat/dungeon/dungeon.pim", (void *) read_buffer, 0);

    texture_list[4].name = (char *) GetPackFile(read_buffer, "gaiji.img", 0);
    texture_list[5].name = (char *) GetPackFile(read_buffer, "fuki256.img", 0);
    texture_list[6].name = (char *) GetPackFile(read_buffer, "syst04.img", 0);
    texture_list[7].name = (char *) GetPackFile(read_buffer, "c07a01.img", 0);
    texture_list[8].name = (char *) GetPackFile(read_buffer, "c07a01an.img", 0);
    texture_list[9].name = (char *) GetPackFile(read_buffer, "p17a01.img", 0);
    texture_list[10].name = (char *) GetPackFile(read_buffer, "c08a01.img", 0);
    texture_list[11].name = (char *) GetPackFile(read_buffer, "c09a01.img", 0);
    texture_list[12].name = (char *) GetPackFile(read_buffer, "c09a01an.img", 0);
    texture_list[13].name = (char *) GetPackFile(read_buffer, "c11a01.img", 0);
    texture_list[14].name = (char *) GetPackFile(read_buffer, "c11a01an.img", 0);
    texture_list[15].name = (char *) GetPackFile(read_buffer, "p19a01.img", 0);
    texture_list[16].name = (char *) GetPackFile(read_buffer, "p19a01an.img", 0);
    texture_list[17].name = (char *) GetPackFile(read_buffer, "ex.img", 0);
    texture_list[18].name = (char *) GetPackFile(read_buffer, "cloud.img", 0);
    texture_list[19].name = (char *) GetPackFile(read_buffer, "cloudan.img", 0);
    texture_list[20].name = (char *) GetPackFile(read_buffer, "cloud2.img", 0);
    texture_list[21].name = (char *) GetPackFile(read_buffer, "i01t01.img", 0);
    texture_list[22].name = (char *) GetPackFile(read_buffer, "i01t02.img", 0);
    texture_list[23].name = (char *) GetPackFile(read_buffer, "e01s01.img", 0);
    texture_list[24].name = (char *) GetPackFile(read_buffer, "fire.img", 0);
    texture_list[25].name = (char *) GetPackFile(read_buffer, "pause.img", 0);

    switch (LanguageCode) {
        case LANG_JAPANESE:
            texture_list[26].name = (char *) GetPackFile(read_buffer, "pause_e.img", 0);
            break;
        case LANG_ENGLISH_US:
            texture_list[26].name = (char *) GetPackFile(read_buffer, "pause_e.img", 0);
            break;
        case LANG_ENGLISH_UK:
            texture_list[26].name = (char *) GetPackFile(read_buffer, "pause_e.img", 0);
            break;
        case LANG_FRENCH:
            texture_list[26].name = (char *) GetPackFile(read_buffer, "pause_f.img", 0);
            break;
        case LANG_GERMAN:
            texture_list[26].name = (char *) GetPackFile(read_buffer, "pause_g.img", 0);
            break;
        case LANG_ITALIAN:
            texture_list[26].name = (char *) GetPackFile(read_buffer, "pause_i.img", 0);
            break;
        case LANG_SPANISH:
            texture_list[26].name = (char *) GetPackFile(read_buffer, "pause_s.img", 0);
            break;
    }

    texture_list[27].name = (char *) GetPackFile(read_buffer, "ashikage.img", 0);

    TexManager.LoadTextureBlock(-1, texture_list);

    CharaTex__2[0] = 1;
    CharaTex__2[1] = 2;
    CharaTex__2[4] = 2;
    CharaTex__2[5] = 2;
    CharaTex__2[3] = 4;
    CharaTex__2[2] = 3;
    CharaTex__2[6] = 5;
    CharaTex__2[7] = 6;
}

/**
 * The scene's actors. The four in the table are the townspeople around the square, the two after
 * them are the couple the scene is about, and the last two are the sky the square is drawn under
 * and the one shadow every one of them is drawn onto.
 *
 * @mangled LoadData__Fv__2
 * @address 0x1DB5420
 * @size 0x32C
 * @unknownret
 */
static void LoadData() {
    char *config_names[4] = {"01c07a.cfg", "01c08a.cfg", "01c11a.cfg", "01c09a.cfg"};

    for (int i = 0; i < 4; i++) {
        Chara__3[i].LoadPackData(read_buffer, config_names[i], CharaDataBuffer__2, 0);

        CFrameAttr attr;

        attr.clip_enable = false;
        Chara__3[i].frame->SetAttr(attr, 1, 4);
        Chara__3[i].motion_type.state.time = 10.0f;
        Chara__3[i].motion_type.state.blend_step = 0.05f;
        Chara__3[i].motion_type.state.motion_no = 0;
        Chara__3[i].motion_type.state.playing_no = 0;
    }

    Chara__3[1].motion_type.state.time = 115.0f;

    wait_now_loading_vsync();

    Chara__3[6].Initialize();
    Chara__3[6].frame = LoadMDSFile(GetPackFile(read_buffer, "01p19a.mds", 0), &CharaDataBuffer__2[4], 6, 0, 0);

    CFrameAttr noroi_attr;

    noroi_attr.clip_enable = false;
    Chara__3[6].frame->SetAttr(noroi_attr, 1, 4);

    wait_now_loading_vsync();

    Chara__3[7].Initialize();
    Chara__3[7].frame = LoadMDSFile(GetPackFile(read_buffer, "01p17a.mds", 0), &CharaDataBuffer__2[4], 6, 0, 0);

    CFrameAttr dancer_attr;

    dancer_attr.clip_enable = false;
    Chara__3[7].frame->SetAttr(dancer_attr, 1, 4);

    float scale[4] = {4.5f, 1.0f, 4.5f, 0.0f};

    Chara__3[7].frame->SearchFrame("body")->ScaleBoundBox(scale);

    InitDancerPos();

    wait_now_loading_vsync();

    Cloud.frame = LoadMDSFile(GetPackFile(read_buffer, "01cloud.mds", 0), 2, 0);
    Cloud.SetPosition(0.0f, 50.0f, 20.0f);

    // Retail asked for 12800 bytes, fifty of the PS2's 256-byte CEffect.
    EffectTable = (CEffect *) MapDataBuffer.Alloc(50 * sizeof(CEffect));
    Smoke.Initialize(EffectTable, 50);
    Smoke.Clear();

    Shadow = LoadMDSFile(GetPackFile(read_buffer, "ashikage.mds", 0), 2, 0);
}

/**
 * The couple's first dance step. Each takes its motion file's own first key range, which is what
 * the background loader then advances one file at a time.
 *
 * @mangled SetDanceMotion__Fv
 * @address 0x1DB5750
 * @size 0x148
 * @unknownret
 */
static void SetDanceMotion() {
    LoadFile("opdat/chara/01p19a1a.chr", (void *) read_buffer, 0);
    Chara__3[6].LoadPackData(read_buffer, "01p19a1a.cfg", &CharaDataBuffer__2[4], &CharaDataBuffer__2[6], 0);
    Chara__3[6].motion_type.state.time = 120.0f;
    Chara__3[6].motion_type.state.blend_step = 0.1f;
    Chara__3[6].motion_type.state.motion_no = 0;
    Chara__3[6].motion_type.state.playing_no = 0;
    Chara__3[6].motion_type.motion_info->start = noroi[0].start;
    Chara__3[6].motion_type.motion_info->end = noroi[0].end;

    LoadFile("opdat/chara/01p17a1a.chr", (void *) read_buffer, 0);
    Chara__3[7].LoadPackData(read_buffer, "01p17a1a.cfg", &CharaDataBuffer__2[4], &CharaDataBuffer__2[6], 0);
    Chara__3[7].motion_type.state.time = 1.0f;
    Chara__3[7].motion_type.state.blend_step = 0.1f;
    Chara__3[7].motion_type.state.motion_no = 0;
    Chara__3[7].motion_type.state.playing_no = 0;
    Chara__3[7].motion_type.motion_info->start = dancer[0].start;
    Chara__3[7].motion_type.motion_info->end = dancer[0].end;
}

/**
 * Where the crowd stands, as the square was laid out: thirty-five places in tenths of a world unit
 * across seven rows, every one of them facing the couple.
 *
 * @mangled InitDancerPos__Fv
 * @address 0x1DB58A0
 * @size 0x124
 * @unknownret
 */
static void InitDancerPos() {
    float layout[35][4] = {
        {3.0f,  0.0f, 27.0f, 0.0f},
        {0.0f,  0.0f, 27.0f, 0.0f},
        {-3.0f, 0.0f, 27.0f, 0.0f},
        {1.5f,  0.0f, 29.0f, 0.0f},
        {-1.5f, 0.0f, 29.0f, 0.0f},
        {4.5f,  0.0f, 29.0f, 0.0f},
        {-4.5f, 0.0f, 29.0f, 0.0f},
        {3.0f,  0.0f, 31.0f, 0.0f},
        {0.0f,  0.0f, 31.0f, 0.0f},
        {-3.0f, 0.0f, 31.0f, 0.0f},
        {1.5f,  0.0f, 33.0f, 0.0f},
        {-1.5f, 0.0f, 33.0f, 0.0f},
        {4.5f,  0.0f, 33.0f, 0.0f},
        {-4.5f, 0.0f, 33.0f, 0.0f},
        {3.0f,  0.0f, 35.0f, 0.0f},
        {0.0f,  0.0f, 35.0f, 0.0f},
        {-3.0f, 0.0f, 35.0f, 0.0f},
        {1.5f,  0.0f, 37.0f, 0.0f},
        {-1.5f, 0.0f, 37.0f, 0.0f},
        {4.5f,  0.0f, 37.0f, 0.0f},
        {-4.5f, 0.0f, 37.0f, 0.0f},
        {3.0f,  0.0f, 39.0f, 0.0f},
        {0.0f,  0.0f, 39.0f, 0.0f},
        {-3.0f, 0.0f, 39.0f, 0.0f},
        {1.5f,  0.0f, 41.0f, 0.0f},
        {-1.5f, 0.0f, 41.0f, 0.0f},
        {4.5f,  0.0f, 41.0f, 0.0f},
        {-4.5f, 0.0f, 41.0f, 0.0f},
        {3.0f,  0.0f, 43.0f, 0.0f},
        {0.0f,  0.0f, 43.0f, 0.0f},
        {-3.0f, 0.0f, 43.0f, 0.0f},
        {1.5f,  0.0f, 45.0f, 0.0f},
        {-1.5f, 0.0f, 45.0f, 0.0f},
        {4.5f,  0.0f, 45.0f, 0.0f},
        {-4.5f, 0.0f, 45.0f, 0.0f}
    };

    for (int i = 0; i < 35; i++) {
        DancerPos[i][0] = 10.0f * layout[i][0];
        DancerPos[i][1] = 10.0f * layout[i][1];
        DancerPos[i][2] = 10.0f + 10.0f * layout[i][2];
        DancerRot[i][0] = 0.0f;
        DancerRot[i][1] = PI_D;
        /* The third store repeats the first rather than clearing the roll, which is deliberate here
           and not a slip left in place: every row of the table has a zero roll already, so the
           line has no effect either way, and the code the compiler emits is not the same without
           it. */
        DancerRot[i][0] = 0.0f;
    }

    Chara__3[6].SetPosition(0.0f, 0.0f, 250.0f);
    Chara__3[6].SetRotation(0.0f, PI_D, 0.0f);
}

/* The tick's drawing, in the order the frame is built: the ground, the buildings standing on it,
   the townspeople, the couple, the crowd behind them, the fires, the sky and last the depth of
   field. The two ambients that fade are what makes the square go dark as the scene turns: one rides
   up over the buildings while the camera holds on them, the other rides the crowd down as the
   couple's motion runs out. */
void OpA_DrawProcess() {
    static sceVu0FVECTOR ambient = {0.0f, 0.0f, 0.0f, 0.0f};
    static int           wait;
    sceVu0FVECTOR        saved_ambient;

    TexManager.ReloadTexture(Vif1Packet, 10);
    OP_GroundMap.Draw();
    TexManager.ReloadTexture(Vif1Packet, 11);

    if (!Pause) {
        setTexScroll();
    }

    if (CScript__2.camera_start != 44) {
        if (CScript__2.camera_start < 15 || CScript__2.camera_start >= 39) {
            OP_BuildingMap.Draw();
        }

        ambient[3] = 0.0f;
        wait = 0;
    } else {
        MGGetAmbient(saved_ambient);

        if (wait > 700) {
            if (ambient[3] < 128.0f) {
                ambient[3] += 0.1f;
            }
        } else {
            wait++;
        }

        MGSetAmbient(ambient);
        OP_BuildingMap.Draw();
        MGSetAmbient(saved_ambient);
    }

    for (int i = 0; i < 6; i++) {
        if (i != 1 && CScript__2.obj[i].disp) {
            TexManager.ReloadTexture(Vif1Packet, CharaTex__2[i]);
            FaceChange(i);

            if (!Pause) {
                Chara__3[i].Step();
            }

            Chara__3[i].Draw();

            if (i == 2) {
                ShisaiShadow();
            }

            if (i == 3) {
                ShogunShadow();
            }
        }
    }

    if (CScript__2.obj[1].disp) {
        TexManager.ReloadTexture(Vif1Packet, CharaTex__2[1]);

        if (!Pause) {
            Chara__3[1].Step();
        }

        static float col = 0.0f;
        static float am = 0.0f;
        static int   wait = 0;

        if (CScript__2.camera_start == 17) {
            static sceVu0FMATRIX lcolor;

            for (int i = 0; i < 3; i++) {
                if (lightcolor[i][0] > col) {
                    lcolor[i][0] = col;
                }

                if (lightcolor[i][1] > col) {
                    lcolor[i][1] = col;
                }

                if (lightcolor[i][2] > col) {
                    lcolor[i][2] = col;
                }
            }

            if (wait > 180) {
                if (col < 128.0f) {
                    col += 0.5f;
                }
            } else {
                wait++;
            }

            MGSetPLight(light, lcolor);

            sceVu0FVECTOR lead_ambient = {22.0f, 22.0f, 12.0f, 0.0f};

            lead_ambient[3] = am;

            if (am < 128.0f) {
                am += 0.5f;
            }

            MGSetAmbient(lead_ambient);
        } else {
            am = 0.0f;
            col = 0.0f;
            wait = 0;
        }

        Chara__3[1].Draw();
    }

    MGSetAmbient(ambientlight);
    MGSetPLight(light, lightcolor);

    sceVu0FVECTOR crowd_ambient;

    sceVu0CopyVector(crowd_ambient, ambientlight);

    if (CScript__2.camera_start == 44) {
        if (Cam__2[SceneNp__2].motion_type.state.time > 249.0f) {
            DancerAmb -= 0.2f;

            if (DancerAmb < 0.0f) {
                DancerAmb = 0.0f;
            }

            crowd_ambient[3] = DancerAmb;
        }
    } else {
        DancerAmb = 127.0f;
    }

    if (CScript__2.obj[6].disp && !Pause) {
        if (DanceWait < 2.0f || DanceWait > 480.0f) {
            Chara__3[6].Step();
            Chara__3[7].Step();
        }

        DanceWait += 1.0f;

        if (DanceWait > 10000) {
            DanceWait = 10000;
        }
    }

    if (CScript__2.obj[6].disp) {
        TexManager.ReloadTexture(Vif1Packet, CharaTex__2[6]);
        FaceChange(6);
        MGSetAmbient(crowd_ambient);
        Chara__3[6].Draw();
        ReaderShadow();
    }

    if (CScript__2.obj[7].disp) {
        TexManager.ReloadTexture(Vif1Packet, 6);
        Chara__3[7].SetPosition(DancerPos[0][0], DancerPos[0][1], DancerPos[0][2]);
        Chara__3[7].SetRotation(DancerRot[0][0], DancerRot[0][1], DancerRot[0][2]);
        MGSetAmbient(crowd_ambient);
        Chara__3[7].Draw();
        DancerShadow();

        int crowd_count;

        switch (CScript__2.camera_start) {
            case 0:
                crowd_count = 28;
                break;
            case 3:
                crowd_count = 30;
                break;
            case 7:
                crowd_count = 30;
                break;
            case 8:
                crowd_count = 30;
                break;
            case 9:
                crowd_count = 25;
                break;
            case 10:
                crowd_count = 15;
                break;
            case 39:
                crowd_count = 30;
                break;
            case 43:
                crowd_count = 30;
                break;
            case 45:
                crowd_count = 14;
                break;
            default:
                crowd_count = 35;
                break;
        }

        for (int i = 1; i < crowd_count; i++) {
            CFrame *frame = Chara__3[7].frame;

            frame->SetPosition(DancerPos[i][0], DancerPos[i][1], DancerPos[i][2]);
            frame->SetRotation(DancerRot[i][0], DancerRot[i][1], DancerRot[i][2]);
            MGSetAmbient(crowd_ambient);
            MGDraw(frame);
            DancerShadow();
        }
    }

    MGSetAmbient(ambientlight);

    sceVu0FVECTOR eye;

    TexManager.ReloadTexture(GetVif1Packet(), 0);
    OP_CharaFrame__2 = Cam__2[SceneNp__2].frame;
    sceVu0CopyVector(eye, OP_CharaFrame__2->position);

    if (!Pause) {
        CFire.FireStep();
    }

    CFire.FireCreate();

    for (int i = 0; i < OP_FireList; i++) {
        float z = OP_FirePosition[i][2];
        float y = OP_FirePosition[i][1];
        float x = OP_FirePosition[i][0];

        CFire.pos[0] = 10.0f * x;
        CFire.pos[1] = 10.0f * y;
        CFire.pos[2] = 10.0f * z;
        CFire.pos[3] = 1.0f;
        CFire.DrawFire(1, 1, &OP_MainCamera, eye, OP_FireScale[i], 15, 15.0f);
    }

    DrawCloud();

    static float cnt = 1024.0f;
    static int   sw = 0;

    if (CScript__2.sprite == 1) {
        TexManager.ReloadTexture(Vif1Packet, 7);

        int alpha = (int) (128.0f - cnt / 4.0f);

        if (alpha < 0) {
            alpha = 0;
        }

        set2DSprite(GetVif1Packet(), TexManager.GetTexture("ex", -1), CRect<int>((int) (272.0f - cnt / 2.0f), (int) (10.0f - cnt / 2.0f), (int) (128.0f + cnt), (int) (128.0f + cnt)), CRect<int>(0, 0, 128, 128), (u_char) alpha);

        if (sw == 0) {
            cnt -= cnt / 2.0f;

            if (cnt < 2.0f) {
                sw = 1;
            }
        } else {
            cnt += cnt / 2.0f;

            if (cnt > 16.0f) {
                sw = 0;
            }
        }
    } else {
        cnt = 1024.0f;
        sw = 0;
    }

    TexManager.ReloadTexture(Vif1Packet, 22);

    switch (CScript__2.camera_start) {
        case 0:
        case 43:
        case 45:
        case 47:
            break;
        case 3:
        case 10:
        case 24:
        case 25: {
            float dof[] = {50.0f};

            DepthOfField(dof, 1, 56, 0);
        } break;
        default: {
            float dof[2] = {200.0f, 400.0f};

            DepthOfField(dof, 2, 40, 0);
        } break;
    }
}

/**
 * The sky, which is one model turned inside out and scrolled rather than a backdrop. It grows from
 * nothing over the first few seconds of the scene it belongs to and its own ambient rides up with
 * it, so the sky arrives before the square does.
 *
 * @mangled DrawCloud__Fv
 * @address 0x1DB6710
 * @size 0x230
 * @unknownret
 */
static void DrawCloud() {
    static sceVu0FVECTOR ambient = {0.0f, 0.0f, 0.0f, 0.0f};
    static float         sc = 0.0f;

    if (CScript__2.sprite == 2) {
        CloudFlag = 1;
    }

    if (CloudFlag == 1) {
        TexManager.ReloadTexture(Vif1Packet, 8);

        if (!Pause) {
            setCloudTexScroll();
        }

        Cloud.frame->SetScale(sc, sc, sc);

        if (CScript__2.camera_start == 44 || CScript__2.camera_start == 16) {
            Cloud.SetPosition(0.0f, 30.0f, -10.0f);
            Cloud.SetRotation(0.0f, PI_D, 0.0f);
        } else {
            Cloud.SetPosition(0.0f, 50.0f, 15.0f);
            Cloud.SetRotation(0.0f, 0.0f, 0.0f);
        }

        sceVu0FVECTOR saved_ambient;

        MGGetAmbient(saved_ambient);
        MGSetAmbient(ambient);
        Cloud.Draw();
        SmokeProcess();
        MGSetAmbient(saved_ambient);

        if (!Pause) {
            if (sc < 1.0) {
                sc = 0.005f + sc;
            }

            if (ambient[3] < 80.0f) {
                ambient[3] += 1.0f;
            }
        }
    } else {
        sc = 0.0f;
        ambient[3] = 0.0f;
    }
}

/**
 * The chimney smoke. One particle is entered every eighth tick, with its speed, its spin and how
 * far it drifts taken from the random generator so that no two rise the same way, and a second one
 * at a fixed place beside it.
 *
 * @mangled SmokeProcess__Fv
 * @address 0x1DB6940
 * @size 0x2A4
 * @unknownret
 */
static void SmokeProcess() {
    static int cnt = 0;

    cnt++;

    if (cnt > 7) {
        cnt = 0;
    }

    CEffectParam param;

    param.Initialize();

    if (cnt == 0) {
        sceVu0FVECTOR chimney_position = {0.0f, 80.0f, 22.0f, 1.0f};
        sceVu0FVECTOR alternate_position = {-30.0f, 100.0f, 30.0f, 1.0f};

        if (CScript__2.camera_start != 19) {
            sceVu0CopyVector(param.position, chimney_position);
        } else {
            sceVu0CopyVector(param.position, alternate_position);
        }

        param.position_oscillation_flags = 1;
        param.position_oscillation_scale[0] = 0.5f * (float) rand() / 2147483648.0f;
        param.position_oscillation_rate[0] = (float) PI_D / (20.0f + (float) (rand() * 10) / 2147483648.0f);
        param.opacity_mode = 2;
        param.render_flags = 2;
        param.opacity = 0.07f;
        param.velocity[1] = 1.5f + 0.2f * (float) rand() / 2147483648.0f;
        param.scale_velocity[0] = 0.04f;
        param.scale_velocity[1] = 0.04f;
        param.lifetime = 120;
        param.texture = TexManager.GetTexture("cloud2", -1);
        param.texel = CRect_i_(0, 0, 128, 128);
        param.width = 30.0f;
        param.height = 13.0f;
        param.texture_frame_period = 60;
        Smoke.EnterEffect(&param);
    }

    Smoke.Step(1);
    Smoke.Draw();

    if (cnt == 0) {
        sceVu0FVECTOR second_position = {0.0f, 50.0f, -17.0f, 1.0f};

        sceVu0CopyVector(param.position, second_position);
        Smoke.EnterEffect(&param);
    }

    Smoke.Draw();
}

/* The tick's motion. The projection is re-set first because the camera passes through the scenery
   in some of the shots and the near plane has to move with it; the camera is then shaken by a
   random tenth of a unit on each axis for the shots that want it. After that every actor takes what
   the script last asked of it, and the ones the camera's own frame tree carries take their place
   from it instead. */

/**
 * Where the crowd stands once the scene is running: thirty-five frames in the camera's own model,
 * one per place, so the whole square was animated in the same file as the camera move.
 *
 * @mangled MoveDancers__Fv
 * @address 0x1DB75D0
 * @size 0x188
 * @unknownret
 */
static void MoveDancers() {
    char         *frame_names[35] = {"p17a1", "p17a2", "p17a3", "p17a4", "p17a5", "p17a6", "p17a7", "p17a8",
                                     "p17a9", "p17a10", "p17a11", "p17a12", "p17a13", "p17a14", "p17a15",
                                     "p17a16", "p17a17", "p17a18", "p17a19", "p17a20", "p17a21", "p17a22",
                                     "p17a23", "p17a24", "p17a25", "p17a26", "p17a27", "p17a28", "p17a29",
                                     "p17a30", "p17a31", "p17a32", "p17a33", "p17a34", "p17a35"};
    sceVu0FMATRIX matrix;

    if (CScript__2.obj[7].move_req) {
        for (int i = 0; i < 35; i++) {
            CFrame *frame = Cam__2[SceneNp__2].frame->SearchFrame(frame_names[i]);

            if (frame) {
                frame->GetLWMatrix(matrix);
                DancerPos[i][0] = matrix[3][0];
                DancerPos[i][1] = matrix[3][1];
                DancerPos[i][2] = matrix[3][2];
                DancerRot[i][0] = 0.0f;
                DancerRot[i][1] = (float) (atan2f(matrix[2][0], matrix[2][2]) + PI_D);
                DancerRot[i][2] = 0.0f;
            }
        }
    }
}

/**
 * The couple's own shadows, which are a pair each because a dancer's two feet move apart: the
 * effect frames the model carries are found by name and one shadow is drawn under each.
 *
 * @mangled ReaderShadow__Fv
 * @address 0x1DB7760
 * @size 0xB8
 * @unknownret
 */
static void ReaderShadow() {
    sceVu0FMATRIX matrix;

    TexManager.ReloadTexture(Vif1Packet, 6);

    CFrame *frame = Chara__3[6].frame->SearchFrame("eff20");

    if (frame) {
        frame->GetLWMatrix(matrix);
        DrawShadow(matrix[3][0], matrix[3][1], matrix[3][2]);
    }

    frame = Chara__3[6].frame->SearchFrame("eff14");

    if (frame) {
        frame->GetLWMatrix(matrix);
        DrawShadow(matrix[3][0], matrix[3][1], matrix[3][2]);
    }
}

/**
 * Draws the dancer's paired foot shadows.
 *
 * @mangled DancerShadow__Fv
 * @address 0x1DB7820
 * @size 0xB4
 * @unknownret
 */
static void DancerShadow() {
    sceVu0FMATRIX matrix;
    float        *z_address;
    float        *y_address;
    float        *x_address;
    float         x;
    float         y;
    float         z;

    // The addresses stay valid when the second lookup replaces the matrix, so only the values
    // need to be loaded again.
    Chara__3[7].frame->SearchFrame("r_foot")->GetLWMatrix(matrix);
    x_address = &matrix[3][0];
    x = *x_address;
    y_address = &matrix[3][1];
    y = *y_address;
    z_address = &matrix[3][2];
    z = *z_address;
    DrawShadow(x, y, z);
    Chara__3[7].frame->SearchFrame("l_foot")->GetLWMatrix(matrix);
    x = *x_address;
    y = *y_address;
    z = *z_address;
    DrawShadow(x, y, z);
}

/**
 * Draws the shogun's scene shadow.
 *
 * @mangled ShogunShadow__Fv
 * @address 0x1DB78E0
 * @size 0xCC
 * @unknownret
 */
static void ShogunShadow() {
    sceVu0FMATRIX matrix;
    float        *z_address;
    float        *y_address;
    float        *x_address;
    float         x;
    float         y;
    float         z;

    // Both effect frames write the same matrix storage, so the coordinate addresses are shared.
    TexManager.ReloadTexture(Vif1Packet, 6);
    Chara__3[3].frame->SearchFrame("eff66")->GetLWMatrix(matrix);
    x_address = &matrix[3][0];
    x = *x_address;
    y_address = &matrix[3][1];
    y = *y_address;
    z_address = &matrix[3][2];
    z = *z_address;
    DrawShadow(x, y, z);
    Chara__3[3].frame->SearchFrame("eff62")->GetLWMatrix(matrix);
    x = *x_address;
    y = *y_address;
    z = *z_address;
    DrawShadow(x, y, z);
}

/**
 * Draws the priest's scene shadow.
 *
 * @mangled ShisaiShadow__Fv
 * @address 0x1DB79B0
 * @size 0xCC
 * @unknownret
 */
static void ShisaiShadow() {
    sceVu0FMATRIX matrix;
    float        *z_address;
    float        *y_address;
    float        *x_address;
    float         x;
    float         y;
    float         z;

    // Both effect frames write the same matrix storage, so the coordinate addresses are shared.
    TexManager.ReloadTexture(Vif1Packet, 6);
    Chara__3[2].frame->SearchFrame("eff93")->GetLWMatrix(matrix);
    x_address = &matrix[3][0];
    x = *x_address;
    y_address = &matrix[3][1];
    y = *y_address;
    z_address = &matrix[3][2];
    z = *z_address;
    DrawShadow(x, y, z);
    Chara__3[2].frame->SearchFrame("eff99")->GetLWMatrix(matrix);
    x = *x_address;
    y = *y_address;
    z = *z_address;
    DrawShadow(x, y, z);
}

/**
 * One shadow, laid on the ground under the point it is given. It fades with height rather than
 * being clipped, which is what lets a foot lift without the shadow following it up, and it is never
 * brighter than the ambient the crowd is drawn at.
 *
 * @mangled DrawShadow__Ffff
 * @address 0x1DB7A80
 * @size 0x108
 * @unknownret
 */
static void DrawShadow(float x, float y, float z) {
    sceVu0FVECTOR ambient = {0.0f, 0.0f, 0.0f, 64.0f};

    ambient[3] -= 10.0f * y;

    if (ambient[3] > 0.0f) {
        if (ambient[3] > DancerAmb) {
            ambient[3] = DancerAmb;
        }

        MGSetAmbient(ambient);

        float heading = atan2f(x, z);

        Shadow->SetPosition(x, 0.05f, z);
        Shadow->SetRotation(89.0f * PI_D / 180.0f, heading, 0.0f);
        MGDraw(Shadow);
        MGSetAmbient(ambientlight);
    }
}

/* The tick's sound. The three footfalls are windows on an actor's own motion frame with a wait
   behind each, because a frame number is only inside its window for one tick at the motion's rate
   and the wait keeps a motion that stalls there from playing the step twice. The rest is the
   square's own ambience: the fountain from a fixed point, the wind while the sky is up, and the
   change of music the camera makes when it turns away. */
void OpA_SoundProcess() {
    /* These type-only names preserve the first footfall's argument-selection state. */
    static int    mus = 0;

    if (DanceWait > 5.0f) {
        if (mus == 0) {
            OpBgmSqPort = 0;
            OpBgmPlay();
            mus = 1;
        }
    } else {
        mus = 0;
    }

    sceVu0FVECTOR camera_position;
    sceVu0FVECTOR shogun_position;

    sceVu0CopyVector(shogun_position, Chara__3[3].pos);
    OP_MainCamera.GetPos(camera_position);

    if (CScript__2.obj[3].motion == 1) {
        int        motion_frame = (int) Chara__3[3].motion_type.state.time;
        static int wait = 0;

        if (wait == 0) {
            if (motion_frame > 38 && motion_frame < 40) {
                OpPlayVolPanSE(shogun_position, 100.0f, 500.0f, MIDI_PORT_SE_DEFAULT, 21, 28);
                wait = 10;
            } else if (motion_frame > 48 && motion_frame < 50) {
                OpPlayVolPanSE(shogun_position, 100.0f, 500.0f, MIDI_PORT_SE_DEFAULT, 21, 29);
                wait = 10;
            }
        } else {
            wait--;
        }
    }

    if (CScript__2.obj[3].motion == 4) {
        int        motion_frame = (int) Chara__3[3].motion_type.state.time;
        static int wait = 0;

        if (wait == 0) {
            if (motion_frame > 98 && motion_frame < 100) {
                OpPlayVolPanSE(shogun_position, 100.0f, 500.0f, MIDI_PORT_SE_DEFAULT, 21, 28);
                wait = 10;
            } else if (motion_frame > 108 && motion_frame < 110) {
                OpPlayVolPanSE(shogun_position, 100.0f, 500.0f, MIDI_PORT_SE_DEFAULT, 21, 29);
                wait = 10;
            }
        } else {
            wait--;
        }
    }

    if (CScript__2.obj[3].motion == 10) {
        int        motion_frame = (int) Chara__3[3].motion_type.state.time;
        static int wait = 0;

        if (wait == 0) {
            if (motion_frame > 228 && motion_frame < 230) {
                OpPlayVolPanSE(shogun_position, 100.0f, 500.0f, MIDI_PORT_SE_DEFAULT, 21, 28);
                wait = 10;
            } else if (motion_frame > 238 && motion_frame < 240) {
                OpPlayVolPanSE(shogun_position, 100.0f, 500.0f, MIDI_PORT_SE_DEFAULT, 21, 29);
                wait = 10;
            }
        } else {
            wait--;
        }
    }

    if (CScript__2.obj[2].motion == 14) {
        int        motion_frame = (int) Chara__3[2].motion_type.state.time;
        static int wait = 0;

        if (wait == 0) {
            if (motion_frame > 295 && motion_frame < 297) {
                OpPlayVolPanSE(shogun_position, 100.0f, 500.0f, MIDI_PORT_SE_DEFAULT, 21, 28);
                wait = 5;
            } else if (motion_frame > 298 && motion_frame < 300) {
                OpPlayVolPanSE(shogun_position, 100.0f, 500.0f, MIDI_PORT_SE_DEFAULT, 21, 29);
                wait = 5;
            } else if (motion_frame > 301 && motion_frame < 303) {
                OpPlayVolPanSE(shogun_position, 100.0f, 500.0f, MIDI_PORT_SE_DEFAULT, 21, 28);
                wait = 5;
            }
        } else {
            wait--;
        }
    }

    sceVu0FVECTOR fountain = {0.0f, 0.0f, 0.0f, 0.0f};

    OpSetVolPanSE(fountain, 200.0f, 500.0f, 15, 16, 20);

    static int   seflg = 0;
    [[maybe_unused]] static int   secnt = 0;
    [[maybe_unused]] static float vol = 128.0f;

    if (CloudFlag == 1) {
        if (seflg == 0) {
            OpPlayVolSE(MIDI_PORT_SE_TITLE, 16, 28, 0.8f);
            seflg = 1;
        } else if (CScript__2.camera_start != 44) {
            sceVu0FVECTOR wind = {0.0f, 50.0f, 0.0f, 0.0f};

            OpSetVolPanSE(wind, 100.0f, 400.0f, 15, 16, 28);
        }
    } else {
        seflg = 0;
        secnt = 0;
        vol = 128.0f;
    }

    {
        static int flg = 0;

        if (CScript__2.camera_start == 44 && Cam__2[SceneNp__2].motion_type.state.time > 249.0) {
            if (flg == 0) {
                while (ReadBGSync())
                    ;

                LoadFileBG("opdat/dungeon/o1bbgm.snd", (u_long128 *) read_buffer, 0);
                flg = 1;
            }
        } else {
            flg = 0;
        }
    }

    {
        static int flg = 0;

        if (CScript__2.camera_start == 14) {
            if (flg == 0) {
                CSnd.Stop(MIDI_PORT_BGM);
                CSnd.SetReverb(0, 4, 50);
                CSnd.LoadSoundFileFromPack("o01b.txt", read_buffer);
                OpBgmSqPort = 0;
                OpBgmPlay();
                flg = 1;
            }
        } else {
            flg = 0;
        }
    }
}

/**
 * The waterfall behind the square. Its animation is a strip of frames in a texture of its own and
 * the plate the world draws is another, so a tick is two local-to-local transfers that between them
 * put one frame's worth of the strip over the plate with the seam moving down it; the texture cache
 * is flushed first because the plate about to be overwritten is the one the previous tick drew
 * from.
 *
 * @mangled setTexScroll__Fv
 * @address 0x1DB82D0
 * @size 0x274
 * @unknownret
 */
static void setTexScroll() {
    static int   setTexScrollCnt = 0;
    static float setTexScrollCntf = 0.0f;

    CTexture *plate;
    CTexture *strip;
    int       dbp;
    int       sbp;
    int       dbw;
    int       sbw;

    if (setTexScrollCntf <= 0.0f) {
        setTexScrollCntf = 126.0f;
    } else {
        setTexScrollCntf -= 0.5f;
    }

    setTexScrollCnt = (int) setTexScrollCntf;

    plate = TexManager.GetTexture("i01t06", -1);
    strip = TexManager.GetTexture("i01t06a", -1);

    if (plate == 0 || strip == 0) {
        return;
    }

    dbp = plate->tex0 & 0x3fff;
    sbp = strip->tex0 & 0x3fff;
    dbw = (plate->tex0 >> 14) & 0x3f;
    sbw = (strip->tex0 >> 14) & 0x3f;

    if (setTexScrollCnt != 128) {
        MoveImageTest(Vif1Packet, sbp, sbw, 0, CRect<int>(0, setTexScrollCnt, 128, 128 - setTexScrollCnt), dbp, dbw, 0, 0, 0, 0);
    }

    if (setTexScrollCnt != 0) {
        MoveImageTest(Vif1Packet, sbp, sbw, 0, CRect<int>(0, 0, 128, setTexScrollCnt), dbp, dbw, 0, 0, 128 - setTexScrollCnt, 0);
    }
}

/**
 * The sky's own scroll, which is the same two transfers over the cloud plate.
 *
 * @mangled setCloudTexScroll__Fv
 * @address 0x1DB8550
 * @size 0x274
 * @unknownret
 */
static void setCloudTexScroll() {
    static int   setTexScrollCnt = 0;
    static float setTexScrollCntf = 0.0f;

    CTexture *plate;
    CTexture *strip;
    int       dbp;
    int       sbp;
    int       dbw;
    int       sbw;

    if (setTexScrollCntf <= 0.0f) {
        setTexScrollCntf = 126.0f;
    } else {
        setTexScrollCntf -= 0.5f;
    }

    setTexScrollCnt = (int) setTexScrollCntf;

    plate = TexManager.GetTexture("cloud", -1);
    strip = TexManager.GetTexture("cloudan", -1);

    if (plate == 0 || strip == 0) {
        return;
    }

    dbp = plate->tex0 & 0x3fff;
    sbp = strip->tex0 & 0x3fff;
    dbw = (plate->tex0 >> 14) & 0x3f;
    sbw = (strip->tex0 >> 14) & 0x3f;

    if (setTexScrollCnt != 128) {
        MoveImageTest(Vif1Packet, sbp, sbw, 0, CRect<int>(0, setTexScrollCnt, 128, 128 - setTexScrollCnt), dbp, dbw, 0, 0, 0, 0);
    }

    if (setTexScrollCnt != 0) {
        MoveImageTest(Vif1Packet, sbp, sbw, 0, CRect<int>(0, 0, 128, setTexScrollCnt), dbp, dbw, 0, 0, 128 - setTexScrollCnt, 0);
    }
}

void OpA_MotionProcess() {
    switch (CScript__2.camera_start) {
        case 2:
        case 3:
        case 15:
            MGSetRenderInfo(mgRenderInfo.scale[0], 10.0f, 0xffff);
            break;
        case 37: {
            /* The scoped middle arms and folded constants preserve their shared materialisation order. */
            MGSetRenderInfo(mgRenderInfo.scale[0], (float) ((1 << 3) + 2), (0x10000 - 1));
            break;
        }
        case 0:
        case 39: {
            MGSetRenderInfo(mgRenderInfo.scale[0], 16.0f, 0xffff);
            break;
        }
        case 40:
        case 41:
        case 42:
        case 48: {
            MGSetRenderInfo(mgRenderInfo.scale[0], 18.0f, 0xffff);
            break;
        }
        default:
            MGSetRenderInfo(mgRenderInfo.scale[0], 6.0f, 0xffff);
            break;
    }

    bool       shake = false;
    static int d;

    switch (CScript__2.camera_start) {
        case 15:
            shake = true;
            d = 3;
            break;
        case 14:
        case 16:
        case 17:
            shake = true;
            d = 10;
            break;
        case 18:
            if (Cam__2[SceneNp__2].motion_type.state.time < 56.0f) {
                shake = true;
                d = 15;
            }

            break;
        case 44:
            if (CloudFlag == 1) {
                shake = true;
                d = 5;
            }

            break;
    }

    if (shake) {
        sceVu0FVECTOR reference;

        OP_MainCamera.GetRef(reference);
        reference[0] += (float) (rand() % d) / 10.0f;
        reference[1] += (float) (rand() % d) / 10.0f;
        reference[2] += (float) (rand() % d) / 10.0f;
        OP_MainCamera.SetRef(reference);
    }

    for (int i = 0; i < OP_AnimeSeqRot; i++) {
        ObjAnimePlay(&OP_AnimeSeq[i]);
    }

    for (int i = 0; i < 6; i++) {
        if (CScript__2.obj[i].disp) {
            if (CScript__2.obj[i].motion_end != -1) {
                if (Chara__3[i].motion_type.state.time > (float) (Chara__3[i].motion_type.motion_info[CScript__2.obj[i].motion].end - 1)) {
                    CScript__2.obj[i].motion = CScript__2.obj[i].motion_end;
                    CScript__2.obj[i].motion_end = -1;
                }
            }

            Chara__3[i].motion_type.state.blend_step = CScript__2.obj[i].step;

            if (CScript__2.obj[i].step == 1.0f) {
                if (CScript__2.obj[i].motion != Chara__3[i].motion_no) {
                    Chara__3[i].motion_type.state.time = (float) Chara__3[i].motion_type.motion_info[CScript__2.obj[i].motion].start;
                    Chara__3[i].motion_no = CScript__2.obj[i].motion;
                    Chara__3[i].motion_flags = 4;
                    Chara__3[i].motion_speed = -1.0f;
                } else {
                    Chara__3[i].motion_no = CScript__2.obj[i].motion;
                    Chara__3[i].motion_flags = 0;
                    Chara__3[i].motion_speed = -1.0f;
                }
            } else {
                Chara__3[i].motion_no = CScript__2.obj[i].motion;
                Chara__3[i].motion_flags = 0;
                Chara__3[i].motion_speed = -1.0f;
            }
        }
    }

    if (CScript__2.obj[6].disp) {
        LoadMotionData();
    }

    char         *frame_names[23] = {"c07a", "c08a", "c11a", "c09a", "c08a", "c08c", "p19a", "p17a", 0, 0, 0, 0,
                                     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    sceVu0FMATRIX matrix;

    for (int i = 0; i < 8; i++) {
        if (CScript__2.obj[i].disp && CScript__2.obj[i].move_req) {
            CFrame *frame = Cam__2[SceneNp__2].frame->SearchFrame(frame_names[i]);

            if (frame) {
                frame->GetLWMatrix(matrix);

                if (i == 6) {
                    Chara__3[i].SetRotation(0.0f, (float) (atan2f(matrix[2][0], matrix[2][2]) + PI_D), 0.0f);
                } else if (i == 5) {
                    float tilt = atan2f(-matrix[2][1], matrix[2][2]);

                    if (tilt > 3.14f) {
                        tilt -= 6.28f;
                    }

                    if (tilt < -3.14f) {
                        tilt += 6.28f;
                    }

                    Chara__3[i].SetRotation(tilt, atan2f(matrix[2][0], matrix[2][2]), 0.0f);
                } else {
                    Chara__3[i].SetRotation(0.0f, atan2f(matrix[2][0], matrix[2][2]), 0.0f);
                }

                if (i == 2 && CScript__2.camera_start == 36) {
                    static float f;

                    if (Chara__3[i].motion_type.state.time > 345 && Chara__3[i].motion_type.state.time < 350.0f) {
                        f = 1.5f;
                    } else if (Chara__3[i].motion_type.state.time >= 350.0f) {
                        if (f > 0.0f) {
                            f -= 0.1f;
                        }
                    } else {
                        f = 0.0f;
                    }

                    float x = matrix[3][0];
                    float y = matrix[3][1] - f;
                    float z = matrix[3][2];

                    Chara__3[i].SetPosition(x, y, z);
                } else {
                    float x = matrix[3][0];
                    float y = matrix[3][1];
                    float z = matrix[3][2];

                    Chara__3[i].SetPosition(x, y, z);
                }
            }
        }
    }

    if (CScript__2.obj[6].disp) {
        MoveDancers();
    }

    for (int i = 0; i < 23; i++) {
        if (CScript__2.obj[i].load != -1) {
            LoadCharaData(CScript__2.obj[i].load, i);
        }
    }
}
