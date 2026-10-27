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
#include "frame.hpp"
#include "framevu1.hpp"
#include "main.hpp"
#include "mainselect.hpp"
#include "mapobject.hpp"
#include "mathutil.hpp"
#include "mds.hpp"
#include "mglib.hpp"
#include "renderinfo.hpp"
#include "sound.hpp"
#include "texture.hpp"
#include "title/op_a.hpp"
#include "title/opening.hpp"
#include "title/script.hpp"

using tagMOTION_KEY = MOTION_INFO;

/* The rectangle a texture transfer takes. Unlike rect.h's, the four-argument constructor assigns
   x, y, w and h in that order. */
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

/* One row of the table a map sorts its scenery by. */
class CategoryAttr {
public:
    char unk_00[24];

    CategoryAttr();
};

/* A scene map: the ground, the buildings standing on it, or a second set of buildings. */
class CMap {
public:
    CategoryAttr category[16]; /**< Category rows the scenery is sorted by. */
    CMapObject   object[10];   /**< Scenery pieces the map holds. */
    char         unk_AE0[16];

    CMap() { Initialize(); }

    void Initialize();
    void Draw();
};

class CTexture;

/* The scene's one fire, which is a light rather than a model. */
class CFireOmni {
public:
    char          unk_00[32];
    sceVu0FVECTOR pos;         /**< World position DrawFire draws the next fire at. */
    s32           texture_set; /**< Indicates that the textures were supplied rather than looked up. */
    CTexture     *core;        /**< Bright inner texture of the flame. */
    CTexture     *glow;        /**< Soft outer texture of the flame. */
    s32           unk_3C;

    CFireOmni();

    void FireStep();
    void FireCreate();
    void DrawFire(int unknown0, int unknown1, CCamera *camera, float *eye, float scale, int unknown2, float unknown3);
};

class CEffect;
class CEffectParam;

/* The pool the chimney smoke's particles are handed out of. */
class CEffectGroup {
public:
    CEffect *effect_table; /**< First effect in the caller-supplied pool. */
    int      capacity;     /**< Number of effects in the pool. */

    CEffectGroup() { Initialize(nullptr, 0); }

    void Initialize(CEffect *table, int max);
    void Clear();
    void EnterEffect(CEffectParam *param);
    void Step(int unknown0);
    void Draw();
};

/* What one particle is entered with. Only the fields the smoke sets are named. */
class CEffectParam {
public:
    int           lifetime;                   /**< Frames before the particle is retired. */
    int           position_oscillation_flags; /**< Bit zero sways the particle's position. */
    float         unk_08;                     /**< Unscaled sprite width. */
    float         height;                     /**< Unscaled sprite height. */
    char          unk_10[16];
    sceVu0FVECTOR position; /**< World position the particle starts at. */
    char          unk_30[4];
    float         velocity_y; /**< Height the particle rises each step. */
    char          unk_38[24];
    float         position_oscillation_x; /**< Horizontal sway amplitude. */
    char          unk_54[12];
    float         position_oscillation_rate_x; /**< Phase advance of the horizontal sway each step. */
    char          unk_64[28];
    float         scale_velocity_x; /**< Width scale added each step. */
    float         scale_velocity_y; /**< Height scale added each step. */
    char          unk_88[40];
    int           opacity_mode; /**< How the opacity changes over the particle's life. */
    int           render_flags; /**< Alpha and depth-buffer state the particle draws with. */
    float         opacity;      /**< Starting opacity from zero to one. */
    CTexture     *texture;      /**< Texture the particle draws. */
    CRect<int>    texel;        /**< Rectangle sampled from the texture. */
    char          unk_D0[4];
    int           texture_frame_period; /**< Modulus applied before a texture frame is chosen. */
    char          unk_D8[8];

    CEffectParam() {}

    void Initialize();
};

/* One actor's face. The eyes and the mouth are two strips of frames stacked bottom-up in one
   256-wide texture, the eyes down the left half and the mouth down the right, and a tick copies the
   current frame of each over the plate the model draws with. Offsets are measured from the bottom
   edge of the plate. */
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

/* One looping object animation the scene's configuration file registers: a frame is found by name
   and one of its properties is driven from a start value towards an end value by a step each
   tick. */
class OBJ_ANIME_SEQ {
public:
    char          name[16]; /**< Name of the frame the animation drives. */
    int           property; /**< Property animated: rotation, position, scale or colour. */
    int           mode;     /**< How the value moves between its two ends. */
    char          unk_18[8];
    sceVu0FVECTOR start_value; /**< Value the animation starts from. */
    sceVu0FVECTOR end_value;   /**< Value the animation runs to. */
    float         step_x;      /**< Amount added to the first component each tick. */
    float         step_y;      /**< Amount added to the second component each tick. */
    float         step_z;      /**< Amount added to the third component each tick. */
    float         step_w;
    sceVu0FVECTOR current;    /**< Current animated value applied to the attached frames. */
    CFrame       *frames[10]; /**< Frames driven by this animation. */
    int           completion_flag;
    int           unk_8C;

    OBJ_ANIME_SEQ();

    void Initialize();
};

void wait_now_loading_vsync();
void ObjAnimePlay(OBJ_ANIME_SEQ *sequence);
void MoveImageTest(sceVif1Packet *packet, int sbp, int sbw, int spsm, const CRect<int> &rect, int dbp, int dbw, int dpsm, int dsax, int dsay, int dir);
void set2DSprite(sceVif1Packet *packet, CTexture *texture, const CRect<int> &src, const CRect<int> &dst, u_char alpha);
void DepthOfField(float *dist, int level, int alpha, int blur);

namespace {
void LoadTexture();
void LoadData();
void SetDanceMotion();
void InitDancerPos();
void DrawCloud();
void SmokeProcess();
void MoveDancers();
void ReaderShadow();
void DancerShadow();
void ShogunShadow();
void ShisaiShadow();
void DrawShadow(float x, float y, float z);
void setTexScroll();
void setCloudTexScroll();
} // namespace

void FaceChange(int actor_no);
void LoadCharaData(int buffer_no, int actor_no);
void LoadMotionData();

/* Each of the couple's motion files is one key range, the frame it starts at and the frame it ends
   at, written straight over the character's own first key so that a file loaded in the background
   takes over without the motion driver being told anything. */
tagMOTION_KEY noroi[10] = {
    {100, 750, 0.5f, 0},
    {201, 528, 0.5f, 0},
    {103, 802, 0.5f, 0},
    {126, 781, 0.5f, 0},
    {1,   356, 0.5f, 0},
    {50,  629, 0.5f, 0},
    {185, 833, 0.5f, 0},
    {93,  665, 0.5f, 0},
    {124, 529, 0.5f, 0},
    {298, 747, 0.5f, 0}
};

tagMOTION_KEY dancer[10] = {
    {12,  536, 0.5f, 0},
    {209, 536, 0.5f, 0},
    {111, 810, 0.5f, 0},
    {116, 771, 0.5f, 0},
    {1,   356, 0.5f, 0},
    {1,   629, 0.5f, 0},
    {135, 783, 0.5f, 0},
    {82,  660, 0.5f, 0},
    {124, 529, 0.5f, 0},
    {327, 782, 0.5f, 0}
};

int           OP_AnimeSeqRot;
int           OP_FireList;
sceVu0FVECTOR OP_FirePosition[96];
float         OP_FireScale[96];
int           OP_FireFlg[96];
CFrameVu1    *OP_GroundCol;
CFrameVu1    *OP_SkyFrame;
CFrame       *OP_CharaFrame__2;
char          CloudFlag;

namespace {
sceVu0FVECTOR DancerPos[35];
sceVu0FVECTOR DancerRot[35];
CCharacter    Cloud;

CFrameVu1   *Shadow;
float        DancerAmb;
CEffectGroup Smoke;
CEffect     *EffectTable;
} // namespace

int DanceCnt;
int DanceStart;

namespace {
/* Four rows name the fixed surfaces the registry keeps for every scene and the ones after them are
   filled in by name from the pack; the empty name terminates the table. */
void LoadTexture() {
    LOADTEXTURE_INFO2 texture_list[] = {
        {"#blender#640#" HALF_BUFFER_HEIGHT_STR "#4",       0,  0},
        {"#fontbase#512#256#1",                             26, 0},
        {"#fukidashibase#640#" HALF_BUFFER_HEIGHT_STR "#4", 26, 0},
        {"#frame_image#640#" HALF_BUFFER_HEIGHT_STR "#4",   22, 0},
        {nullptr,                                           26, 0},
        {nullptr,                                           26, 0},
        {nullptr,                                           26, 0},
        {nullptr,                                           1,  0},
        {nullptr,                                           1,  0},
        {nullptr,                                           6,  1},
        {nullptr,                                           2,  0},
        {nullptr,                                           4,  0},
        {nullptr,                                           4,  0},
        {nullptr,                                           3,  0},
        {nullptr,                                           3,  0},
        {nullptr,                                           5,  0},
        {nullptr,                                           5,  0},
        {nullptr,                                           7,  0},
        {nullptr,                                           8,  0},
        {nullptr,                                           8,  0},
        {nullptr,                                           8,  0},
        {nullptr,                                           10, 0},
        {nullptr,                                           10, 0},
        {nullptr,                                           11, 0},
        {nullptr,                                           0,  0},
        {nullptr,                                           19, 0},
        {nullptr,                                           19, 0},
        {nullptr,                                           6,  0},
        {"",                                                0,  0}
    };

    TexManager.Initialize(16352);
    LoadFile("opdat/dungeon/dungeon.pim", read_buffer, nullptr);

    texture_list[4].name = reinterpret_cast<char *>(GetPackFile(read_buffer, "gaiji.img", nullptr));
    texture_list[5].name = reinterpret_cast<char *>(GetPackFile(read_buffer, "fuki256.img", nullptr));
    texture_list[6].name = reinterpret_cast<char *>(GetPackFile(read_buffer, "syst04.img", nullptr));
    texture_list[7].name = reinterpret_cast<char *>(GetPackFile(read_buffer, "c07a01.img", nullptr));
    texture_list[8].name = reinterpret_cast<char *>(GetPackFile(read_buffer, "c07a01an.img", nullptr));
    texture_list[9].name = reinterpret_cast<char *>(GetPackFile(read_buffer, "p17a01.img", nullptr));
    texture_list[10].name = reinterpret_cast<char *>(GetPackFile(read_buffer, "c08a01.img", nullptr));
    texture_list[11].name = reinterpret_cast<char *>(GetPackFile(read_buffer, "c09a01.img", nullptr));
    texture_list[12].name = reinterpret_cast<char *>(GetPackFile(read_buffer, "c09a01an.img", nullptr));
    texture_list[13].name = reinterpret_cast<char *>(GetPackFile(read_buffer, "c11a01.img", nullptr));
    texture_list[14].name = reinterpret_cast<char *>(GetPackFile(read_buffer, "c11a01an.img", nullptr));
    texture_list[15].name = reinterpret_cast<char *>(GetPackFile(read_buffer, "p19a01.img", nullptr));
    texture_list[16].name = reinterpret_cast<char *>(GetPackFile(read_buffer, "p19a01an.img", nullptr));
    texture_list[17].name = reinterpret_cast<char *>(GetPackFile(read_buffer, "ex.img", nullptr));
    texture_list[18].name = reinterpret_cast<char *>(GetPackFile(read_buffer, "cloud.img", nullptr));
    texture_list[19].name = reinterpret_cast<char *>(GetPackFile(read_buffer, "cloudan.img", nullptr));
    texture_list[20].name = reinterpret_cast<char *>(GetPackFile(read_buffer, "cloud2.img", nullptr));
    texture_list[21].name = reinterpret_cast<char *>(GetPackFile(read_buffer, "i01t01.img", nullptr));
    texture_list[22].name = reinterpret_cast<char *>(GetPackFile(read_buffer, "i01t02.img", nullptr));
    texture_list[23].name = reinterpret_cast<char *>(GetPackFile(read_buffer, "e01s01.img", nullptr));
    texture_list[24].name = reinterpret_cast<char *>(GetPackFile(read_buffer, "fire.img", nullptr));
    texture_list[25].name = reinterpret_cast<char *>(GetPackFile(read_buffer, "pause.img", nullptr));

    switch (LanguageCode) {
        case LANG_JAPANESE:
        case LANG_ENGLISH_US:
        case LANG_ENGLISH_UK:
            texture_list[26].name = reinterpret_cast<char *>(GetPackFile(read_buffer, "pause_e.img", nullptr));
            break;
        case LANG_FRENCH:
            texture_list[26].name = reinterpret_cast<char *>(GetPackFile(read_buffer, "pause_f.img", nullptr));
            break;
        case LANG_GERMAN:
            texture_list[26].name = reinterpret_cast<char *>(GetPackFile(read_buffer, "pause_g.img", nullptr));
            break;
        case LANG_ITALIAN:
            texture_list[26].name = reinterpret_cast<char *>(GetPackFile(read_buffer, "pause_i.img", nullptr));
            break;
        case LANG_SPANISH:
            texture_list[26].name = reinterpret_cast<char *>(GetPackFile(read_buffer, "pause_s.img", nullptr));
            break;
        default:
            break;
    }

    texture_list[27].name = reinterpret_cast<char *>(GetPackFile(read_buffer, "ashikage.img", nullptr));

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
 */
void LoadData() {
    char *config_names[4] = {"01c07a.cfg", "01c08a.cfg", "01c11a.cfg", "01c09a.cfg"};

    for (int i = 0; i < 4; i++) {
        Chara__3[i].LoadPackData(read_buffer, config_names[i], CharaDataBuffer__2, nullptr);

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
    Chara__3[6].frame = LoadMDSFile(GetPackFile(read_buffer, "01p19a.mds", nullptr), &CharaDataBuffer__2[4], 6, nullptr, nullptr);

    CFrameAttr noroi_attr;

    noroi_attr.clip_enable = false;
    Chara__3[6].frame->SetAttr(noroi_attr, 1, 4);

    wait_now_loading_vsync();

    Chara__3[7].Initialize();
    Chara__3[7].frame = LoadMDSFile(GetPackFile(read_buffer, "01p17a.mds", nullptr), &CharaDataBuffer__2[4], 6, nullptr, nullptr);

    CFrameAttr dancer_attr;

    dancer_attr.clip_enable = false;
    Chara__3[7].frame->SetAttr(dancer_attr, 1, 4);

    float scale[4] = {4.5f, 1.0f, 4.5f, 0.0f};

    Chara__3[7].frame->SearchFrame("body")->ScaleBoundBox(scale);

    InitDancerPos();

    wait_now_loading_vsync();

    Cloud.frame = LoadMDSFile(GetPackFile(read_buffer, "01cloud.mds", nullptr), 2, 0);
    Cloud.SetPosition(0.0f, 50.0f, 20.0f);

    EffectTable = reinterpret_cast<CEffect *>(MapDataBuffer.Alloc(12800));
    Smoke.Initialize(EffectTable, 50);
    Smoke.Clear();

    Shadow = LoadMDSFile(GetPackFile(read_buffer, "ashikage.mds", nullptr), 2, 0);
}

/**
 * The couple's first dance step. Each takes its motion file's own first key range, which is what
 * the background loader then advances one file at a time.
 */
void SetDanceMotion() {
    LoadFile("opdat/chara/01p19a1a.chr", read_buffer, nullptr);
    Chara__3[6].LoadPackData(read_buffer, "01p19a1a.cfg", &CharaDataBuffer__2[4], &CharaDataBuffer__2[6], nullptr);
    Chara__3[6].motion_type.state.time = 120.0f;
    Chara__3[6].motion_type.state.blend_step = 0.1f;
    Chara__3[6].motion_type.state.motion_no = 0;
    Chara__3[6].motion_type.state.playing_no = 0;
    Chara__3[6].motion_type.motion_info->start = noroi[0].start;
    Chara__3[6].motion_type.motion_info->end = noroi[0].end;

    LoadFile("opdat/chara/01p17a1a.chr", read_buffer, nullptr);
    Chara__3[7].LoadPackData(read_buffer, "01p17a1a.cfg", &CharaDataBuffer__2[4], &CharaDataBuffer__2[6], nullptr);
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
 */
void InitDancerPos() {
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
    }

    Chara__3[6].SetPosition(0.0f, 0.0f, 250.0f);
    Chara__3[6].SetRotation(0.0f, PI_D, 0.0f);
}

/**
 * The sky, which is one model turned inside out and scrolled rather than a backdrop. It grows from
 * nothing over the first few seconds of the scene it belongs to and its own ambient rides up with
 * it, so the sky arrives before the square does.
 */
void DrawCloud() {
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
 */
void SmokeProcess() {
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
        param.position_oscillation_x = 0.5f * static_cast<float>(rand()) / 2147483648.0f;
        param.position_oscillation_rate_x = static_cast<float>(PI_D) / (20.0f + static_cast<float>(rand() * 10) / 2147483648.0f);
        param.opacity_mode = 2;
        param.render_flags = 2;
        param.opacity = 0.07f;
        param.velocity_y = 1.5f + 0.2f * static_cast<float>(rand()) / 2147483648.0f;
        param.scale_velocity_x = 0.04f;
        param.scale_velocity_y = 0.04f;
        param.lifetime = 120;
        param.texture = TexManager.GetTexture("cloud2", -1);
        param.texel = CRect<int>(0, 0, 128, 128);
        param.unk_08 = 30.0f;
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

/**
 * Where the crowd stands once the scene is running: thirty-five frames in the camera's own model,
 * one per place, so the whole square was animated in the same file as the camera move.
 */
void MoveDancers() {
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
                DancerRot[i][1] = static_cast<float>(atan2f(matrix[2][0], matrix[2][2]) + PI_D);
                DancerRot[i][2] = 0.0f;
            }
        }
    }
}

/**
 * The couple's own shadows, which are a pair each because a dancer's two feet move apart: the
 * effect frames the model carries are found by name and one shadow is drawn under each.
 */
void ReaderShadow() {
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

void DancerShadow() {
    sceVu0FMATRIX matrix;

    Chara__3[7].frame->SearchFrame("r_foot")->GetLWMatrix(matrix);
    DrawShadow(matrix[3][0], matrix[3][1], matrix[3][2]);
    Chara__3[7].frame->SearchFrame("l_foot")->GetLWMatrix(matrix);
    DrawShadow(matrix[3][0], matrix[3][1], matrix[3][2]);
}

void ShogunShadow() {
    sceVu0FMATRIX matrix;

    TexManager.ReloadTexture(Vif1Packet, 6);
    Chara__3[3].frame->SearchFrame("eff66")->GetLWMatrix(matrix);
    DrawShadow(matrix[3][0], matrix[3][1], matrix[3][2]);
    Chara__3[3].frame->SearchFrame("eff62")->GetLWMatrix(matrix);
    DrawShadow(matrix[3][0], matrix[3][1], matrix[3][2]);
}

void ShisaiShadow() {
    sceVu0FMATRIX matrix;

    TexManager.ReloadTexture(Vif1Packet, 6);
    Chara__3[2].frame->SearchFrame("eff93")->GetLWMatrix(matrix);
    DrawShadow(matrix[3][0], matrix[3][1], matrix[3][2]);
    Chara__3[2].frame->SearchFrame("eff99")->GetLWMatrix(matrix);
    DrawShadow(matrix[3][0], matrix[3][1], matrix[3][2]);
}

/**
 * One shadow, laid on the ground under the point it is given. It fades with height rather than
 * being clipped, which is what lets a foot lift without the shadow following it up, and it is never
 * brighter than the ambient the crowd is drawn at.
 */
void DrawShadow(float x, float y, float z) {
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

/**
 * Scrolls the waterfall: two local-to-local transfers put one frame's worth of the strip texture
 * over the plate with the seam moving down it. The texture cache is flushed first because the plate
 * about to be overwritten is the one the previous tick drew from.
 */
void setTexScroll() {
    static int   setTexScrollCnt = 0;
    static float setTexScrollCntf = 0.0f;

    sceGifTag giftag = {0, 1, 0, 0, 0, 0, 1, SCE_GIF_PACKED_AD};

    sceVif1PkCnt(Vif1Packet, 0);
    sceVif1PkOpenDirectCode(Vif1Packet, 0);
    sceVif1PkOpenGifTag(Vif1Packet, *reinterpret_cast<u_long128 *>(&giftag));
    sceVif1PkAddGsAD(Vif1Packet, SCE_GS_TEXFLUSH, 0);
    sceVif1PkCloseGifTag(Vif1Packet);
    sceVif1PkCloseDirectCode(Vif1Packet);
    sceVif1PkTerminate(Vif1Packet);

    if (setTexScrollCntf <= 0.0f) {
        setTexScrollCntf = 126.0f;
    } else {
        setTexScrollCntf -= 0.5f;
    }

    setTexScrollCnt = static_cast<int>(setTexScrollCntf);

    CTexture *plate = TexManager.GetTexture("i01t06", -1);
    CTexture *strip = TexManager.GetTexture("i01t06a", -1);

    if (plate == nullptr || strip == nullptr) {
        return;
    }

    int dbp = plate->tex0 & 0x3fff;
    int sbp = strip->tex0 & 0x3fff;
    int dbw = (plate->tex0 >> 14) & 0x3f;
    int sbw = (strip->tex0 >> 14) & 0x3f;

    if (setTexScrollCnt != 128) {
        MoveImageTest(Vif1Packet, sbp, sbw, 0, CRect<int>(0, setTexScrollCnt, 128, 128 - setTexScrollCnt), dbp, dbw, 0, 0, 0, 0);
    }

    if (setTexScrollCnt != 0) {
        MoveImageTest(Vif1Packet, sbp, sbw, 0, CRect<int>(0, 0, 128, setTexScrollCnt), dbp, dbw, 0, 0, 128 - setTexScrollCnt, 0);
    }
}

/**
 * The sky's own scroll, which is the same two transfers over the cloud plate.
 */
void setCloudTexScroll() {
    static int   setTexScrollCnt = 0;
    static float setTexScrollCntf = 0.0f;

    sceGifTag giftag = {0, 1, 0, 0, 0, 0, 1, SCE_GIF_PACKED_AD};

    sceVif1PkCnt(Vif1Packet, 0);
    sceVif1PkOpenDirectCode(Vif1Packet, 0);
    sceVif1PkOpenGifTag(Vif1Packet, *reinterpret_cast<u_long128 *>(&giftag));
    sceVif1PkAddGsAD(Vif1Packet, SCE_GS_TEXFLUSH, 0);
    sceVif1PkCloseGifTag(Vif1Packet);
    sceVif1PkCloseDirectCode(Vif1Packet);
    sceVif1PkTerminate(Vif1Packet);

    if (setTexScrollCntf <= 0.0f) {
        setTexScrollCntf = 126.0f;
    } else {
        setTexScrollCntf -= 0.5f;
    }

    setTexScrollCnt = static_cast<int>(setTexScrollCntf);

    CTexture *plate = TexManager.GetTexture("cloud", -1);
    CTexture *strip = TexManager.GetTexture("cloudan", -1);

    if (plate == nullptr || strip == nullptr) {
        return;
    }

    int dbp = plate->tex0 & 0x3fff;
    int sbp = strip->tex0 & 0x3fff;
    int dbw = (plate->tex0 >> 14) & 0x3f;
    int sbw = (strip->tex0 >> 14) & 0x3f;

    if (setTexScrollCnt != 128) {
        MoveImageTest(Vif1Packet, sbp, sbw, 0, CRect<int>(0, setTexScrollCnt, 128, 128 - setTexScrollCnt), dbp, dbw, 0, 0, 0, 0);
    }

    if (setTexScrollCnt != 0) {
        MoveImageTest(Vif1Packet, sbp, sbw, 0, CRect<int>(0, 0, 128, setTexScrollCnt), dbp, dbw, 0, 0, 128 - setTexScrollCnt, 0);
    }
}
} // namespace
