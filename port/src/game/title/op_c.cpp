#include "common.h"

#include <libgraph.h>
#include <libpkt.h>
#include <libvu0.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <iterator>
#include <numbers>

#include "camera.hpp"
#include "camerafollow.hpp"
#include "character.hpp"
#include "dataalloc.hpp"
#include "dataread.hpp"
#include "frame.hpp"
#include "framevu1.hpp"
#include "main.hpp"
#include "mainselect.hpp"
#include "mathutil.hpp"
#include "mds.hpp"
#include "mglib.hpp"
#include "renderinfo.hpp"
#include "sound.hpp"
#include "stubs/title/op_c.hpp"
#include "texture.hpp"
#include "title/bombeffect.hpp"
#include "title/majinbeem.hpp"
#include "title/op_a.hpp"
#include "title/op_b.hpp"
#include "title/op_c.hpp"
#include "title/opening.hpp"
#include "title/script.hpp"
#include "vector.hpp"
#include "visualvu1.hpp"
#include "wind.hpp"

// Declared locally: rect.h's four-argument constructor assigns the fields in the opposite order.
template <class T>
class CRect {
public:
    T x; /**< Left edge. */
    T y; /**< Top edge. */
    T w; /**< Width. */
    T h; /**< Height. */

    CRect() {
        h = 0;
        w = 0;
        y = 0;
        x = 0;
    }

    CRect(T left, T top, T width, T height) {
        x = left;
        y = top;
        w = width;
        h = height;
    }
} __attribute__((aligned(16)));

/**
 * A frame parented to an object, which lets the world transform drive a model.
 */
class CObjectFrame : public CObject {
public:
    CFrameVu1 *frame[4];         /**< Frame of each level of detail; null where the object has none. */
    s32        rotation_changed; /**< Set whenever the angle, its motion or the moment changes. */
    s32        draw_on;          /**< 1 while the object draws; 0 leaves it out of the scene. */

    virtual void FrameObjectOnOff(char *name, int on);
    virtual void Draw();

    void SetFrame(CFrameVu1 *frame, int level);
};

/**
 * One piece of scenery, driven through the object dispatch like anything else in the world.
 */
class CMapObject : public CObjectFrame {
public:
    CFrameVu1 *collision_frame; /**< Frame that collision reads; null where the object has none. */
    CFrameVu1 *shadow_frame;    /**< Model the object's shadow is drawn from; null where it casts none. */
    CFrameVu1 *shade_frame;     /**< Frame that the shade draws from; null where the object takes none. */
    CFrameVu1 *camera_frame;    /**< Collision frame the camera reads; null where the object has none. */
    float      shadow_offset;   /**< Height the shadow drops below the object. */
    int        unk_34;
    int        handle; /**< Handle the map gave the object. */
    char       unk_3C[4];

    CMapObject();

    void Draw() override;

    void Initialize();
    void DrawShadow(int fast);
};

class CTexture;

/**
 * The scene's one fire, which is a light rather than a model.
 */
class CFireOmni {
public:
    char          unk_00[32];
    sceVu0FVECTOR position;    /**< World position the fire draws at. */
    s32           texture_set; /**< Indicates that the textures were supplied rather than looked up. */
    CTexture     *core;        /**< Bright inner texture of the flame. */
    CTexture     *glow;        /**< Soft outer texture of the flame. */
    s32           unk_3C;

    CFireOmni();

    void FireStep();
    void FireCreate();
    void DrawFire(int unused0, int unused1, CCamera *camera, float *eye, float scale, int layers, float camera_offset);
};

/**
 * The rippling water the scene stands on, laid out as the shared CWater.
 */
class CWater {
public:
    s32            rows;
    s32            columns;
    float         *height;
    float         *height_a;
    float         *height_b;
    sceVu0FVECTOR  vertex[4];
    u_int         *packet[3];
    CVisualPolyVu1 visual;
    u8             color[4];
    float          wave_speed;
    float          damping;
    float          height_scale;
    float          distortion;
    s32            tags_built;
    CFrameVu1      frame; /**< Places and draws the water surface. */

    CWater();

    void SetVertex(float *corner0, float *corner1, float *corner2, float *corner3);
    void SetSize(int rows, int columns, CDataAlloc2<1> *arena);
    void SetParam(float wave_speed, float damping, float height_scale, float distortion);
    void SetColor(u_char red, u_char green, u_char blue, u_char alpha);
    void Shake(int x, int y, float height_change);
    void Hamon();
    void DrawVu1(RenderInfo *info, sceVif1Packet *packet, u_long128 *buffer);
};

/**
 * One looping object animation: a frame found by name, driven between two values at a rate.
 */
class OBJ_ANIME_SEQ {
public:
    char          name[16];  /**< Name of the frame the animation drives. */
    int           anim_type; /**< What is animated: 0 rotation, 1 position, 2 scale, 3 colour. */
    int           play_mode; /**< How the value runs between its two ends. */
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

class CEffect;
class CEffectParam;

/**
 * A pool of particles the scene's smoke is entered into.
 */
class CEffectGroup {
public:
    CEffect *effect_table; /**< First effect in the caller-supplied pool. */
    int      capacity;     /**< Number of effects in the pool. */

    CEffectGroup() { Initialize(nullptr, 0); }

    void Initialize(CEffect *table, int count);
    void Clear();
    void EnterEffect(CEffectParam *param);
    void Step(int unused);
    void Draw();
};

/**
 * What one particle is entered with; only the fields the smoke sets are named.
 */
class CEffectParam {
public:
    int           lifetime;                   /**< Number of frames before the particle is retired. */
    int           position_oscillation_flags; /**< Bit zero enables sinusoidal position offsets. */
    float         unk_08;                     /**< Initial unscaled sprite width. */
    float         unk_0C;                     /**< Initial unscaled sprite height. */
    char          unk_10[16];
    sceVu0FVECTOR position; /**< Initial world-space position. */
    char          unk_30[4];
    float         rise_speed; /**< Upward velocity added to the position every step. */
    char          unk_38[24];
    float         wobble_scale; /**< Amplitude of the sideways position oscillation. */
    char          unk_54[12];
    float         wobble_rate; /**< Phase rate of the sideways position oscillation. */
    char          unk_64[28];
    float         width_growth;  /**< Width scale added every step. */
    float         height_growth; /**< Height scale added every step. */
    char          unk_88[40];
    int           opacity_mode; /**< Selects constant, increasing, or decreasing opacity. */
    int           render_flags; /**< Selects temporary alpha and depth-buffer state. */
    float         opacity;      /**< Initial opacity. */
    CTexture     *texture;      /**< Texture the particle is drawn from. */
    CRect<int>    texel;        /**< Rectangle sampled from the texture. */
    char          unk_D0[4];
    int           texture_frame_period; /**< Modulus applied before selecting a texture frame. */
    char          unk_D8[8];

    CEffectParam() {}

    void Initialize();
};

void InitObjAnime(CFrame *frame, OBJ_ANIME_SEQ *sequence);
void ObjAnimePlay(OBJ_ANIME_SEQ *sequence);
void MoveImageTest(sceVif1Packet *packet, int sbp, int sbw, int spsm, const CRect<int> &rect, int dbp, int dbw, int dpsm, int dsax, int dsay, int dir);
void set2DSprite(sceVif1Packet *packet, CTexture *texture, const CRect<int> &src, const CRect<int> &dst, u_char r, u_char g, u_char b, u_char a);
void MGMoveImage(sceGsTex0 *dst, const CRect<int> &rect, sceGsTex0 *src, int dsax, int dsay, int dir);
void DepthOfField(float *dist, int level, int alpha, int blur);

namespace {

/**
 * One piece of scenery as laid out: model, shadow model, position in tenths of a world unit and
 * rotation in degrees. A row with a null model reuses the previous row's model.
 */
struct MAPOBJ_INFO {
    char *name;        /**< Model file, or null to reuse the previous row's model. */
    char *shadow_name; /**< Model file the shadow is drawn from, or null for none. */
    float position[3]; /**< Position in tenths of a world unit. */
    float rotation[3]; /**< Rotation about each axis in degrees. */
};

void MapLoad();
void MapLoad2();
void LoadCharaMajin();
void DrawDancer();
void InchikiShadow();
void DrawShadow(int shadow_no, float x, float y, float z);
void MajinBeemProcess();
void SmokeProcess();
void WaterProcess();
int  KageCheck(int actor);
void LightSet(float x, float y, float z, int actor);
void setTexScroll();
void setTexAnim();

CFireOmni     CFire;
OBJ_ANIME_SEQ Fuusya[2];
CWind         Wind;
CFrame       *TaimatsuFrame[12];
OBJ_ANIME_SEQ Taimatsu[12];
CMapObject    OP_NornMapObj3[4];
CBombEffect   CBomb[3];
CFrameVu1    *DoransFuusya[2];
CEffectGroup  Smoke;
CMajinBeem    CBeem[3];
CFrameVu1    *Shadow[16];
CFrame       *CharaF[3];

int MBeemCnt;

// Places `object` at the row's position (tenths of a world unit) and rotation (degrees).
void PlaceMapObject(CMapObject &object, const MAPOBJ_INFO &row) {
    object.SetPosition(CVector3_f_(10.0f * row.position[0], 10.0f * row.position[1], 10.0f * row.position[2]));
    object.SetRotation(CVector3_f_(static_cast<float>(std::numbers::pi * row.rotation[0] / 180), static_cast<float>(std::numbers::pi * row.rotation[1] / 180), static_cast<float>(std::numbers::pi * row.rotation[2] / 180)));
}

// Waits for the previous background read, starts reading `path` and marks the script's load done.
void LoadSceneDataBG(char *path) {
    while (ReadBGSync()) {
    }

    LoadFileBG(path, reinterpret_cast<u_long128 *>(read_buffer), nullptr);
    CScript__2.load_no = OP_SCENE_NONE;
}

} // namespace

void OpC_LoadDataBG() {
    LoadSceneDataBG("opdat/norn2/norn2.pim");
}

void OpC_LoadDataBG2() {
    LoadSceneDataBG("opdat/norn2/ndance.pak");
}

void OpC_LoadDataBG3() {
    LoadSceneDataBG("opdat/norn2/majin.pak");
}

void OpC_LoadDataBG4() {
    LoadSceneDataBG("opdat/norn2/norn2.pim");
}

void OpC_LoadDataBG5() {
    LoadSceneDataBG("opdat/norn3/norn3.pak");
}

void OpC_InitProcess4() {
    while (ReadBGSync()) {
    }

    TexManager.DeleteTextureBlock(0);
    TexManager.DeleteTextureBlock(10);
    TexManager.DeleteTextureBlock(11);
    TexManager.CleanUpBuffer();

    struct PackImage {
        const char *name;
        int         block_no;
    };

    static constexpr std::array<PackImage, 13> kImages = {
        {{"fire.img", 0},
         {"t0203.img", 10},
         {"e01b01.img", 10},
         {"e01b02.img", 10},
         {"e01b03.img", 10},
         {"e01t01.img", 13},
         {"t0205.img", 10},
         {"t0003.img", 11},
         {"e01s03.img", 11},
         {"e01s06.img", 11},
         {"t0001.img", 11},
         {"bakuhatu.img", 18},
         {"sunakemuri.img", 18}}
    };

    // The blender surface, the pack's images, then a null-named terminator.
    std::array<LOADTEXTURE_INFO2, kImages.size() + 2> tex{};

    tex[0] = {"#blender#640#" HALF_BUFFER_HEIGHT_STR "#4", 0, 0};

    for (std::size_t i = 0; i < kImages.size(); i++) {
        tex[i + 1].name = reinterpret_cast<char *>(GetPackFile(read_buffer, const_cast<char *>(kImages[i].name), nullptr));
        tex[i + 1].block_no = kImages[i].block_no;
        tex[i + 1].mipmap = false;
    }

    TexManager.LoadTextureBlock(-1, tex.data());
    CScript__2.init_no = 0;
}

namespace {

/**
 * Where the village stands, as the scene was laid out: a model, the model its shadow is drawn
 * from, a position in tenths of a world unit and a heading in degrees, one row per piece of scenery.
 * A row with no model of its own is another copy of the row above it, which is how a terrace of
 * identical houses costs one model load. The two windmills and the eight torches are kept aside as
 * they pass because the object animations are registered against those frames by name.
 */
void MapLoad() {
    MAPOBJ_INFO norn[] = {
        {"opdat/norn2/t0207.mds",    nullptr,                    {0.0f, 0.0f, 0.0f},           {0.0f, 0.0f, 0.0f}     },
        {"opdat/norn2/e01h01_0.mds", "opdat/norn2/e01h01_s.mds", {-35.0f, 0.0f, -20.0f},       {0.0f, 0.0f, 0.0f}     },
        {"opdat/norn2/e01h05_2.mds", nullptr,                    {50.0f, 0.0f, 20.0f},         {0.0f, -90.0f, 0.0f}   },
        {"opdat/norn2/e01h11_2.mds", nullptr,                    {50.0f, 0.0f, -10.0f},        {0.0f, -90.0f, 0.0f}   },
        {"opdat/norn2/e01h07_2.mds", nullptr,                    {5.0f, 0.0f, -20.0f},         {0.0f, 0.0f, 0.0f}     },
        {"opdat/norn2/e01h08_2.mds", nullptr,                    {-45.0f, 0.0f, -45.0f},       {0.0f, 0.0f, 0.0f}     },
        {"opdat/norn2/e01h02_2.mds", nullptr,                    {-60.0f, 0.0f, 30.0f},        {0.0f, 90.0f, 0.0f}    },
        {"opdat/norn2/e01h06_2.mds", nullptr,                    {0.0f, 4.7424f, 79.274f},     {0.0f, 0.0f, 0.0f}     },
        {"opdat/norn2/e01a02_1.mds", nullptr,                    {0.0f, 0.0f, 59.965f},        {0.0f, 0.0f, 0.0f}     },
        {"opdat/norn2/t0002.mds",    nullptr,                    {-23.7809f, 0.0f, 19.8331f},  {0.0f, -4.4f, 0.0f}    },
        {nullptr,                    nullptr,                    {23.1908f, 0.0f, 38.0943f},   {0.0f, -170.971f, 0.0f}},
        {"opdat/norn2/e01t01_0.mds", nullptr,                    {-45.0f, 0.1f, -5.0f},        {0.0f, 0.0f, 0.0f}     },
        {"opdat/norn2/e01t01_1.mds", nullptr,                    {65.0f, 0.1f, 45.0f},         {0.0f, -90.0f, 0.0f}   },
        {nullptr,                    nullptr,                    {55.0f, 0.1f, 55.0f},         {0.0f, -90.0f, 0.0f}   },
        {nullptr,                    nullptr,                    {-45.0f, 0.1f, 55.0f},        {0.0f, -90.0f, 0.0f}   },
        {nullptr,                    nullptr,                    {55.0f, 0.1f, 5.0f},          {0.0f, -90.0f, 0.0f}   },
        {"opdat/norn2/e01c01_0.mds", nullptr,                    {25.0f, 0.0f, 5.0f},          {0.0f, 90.0f, 0.0f}    },
        {nullptr,                    nullptr,                    {-45.0f, 0.0f, 5.0f},         {0.0f, -90.0f, 0.0f}   },
        {"opdat/norn2/e01c02_0.mds", nullptr,                    {-45.0f, 0.0f, 25.0f},        {0.0f, 0.0f, 0.0f}     },
        {nullptr,                    nullptr,                    {-45.0f, 0.0f, 15.0f},        {0.0f, 0.0f, 0.0f}     },
        {nullptr,                    nullptr,                    {-25.0f, 0.0f, 5.0f},         {0.0f, 90.0f, 0.0f}    },
        {nullptr,                    nullptr,                    {-5.0f, 0.0f, 5.0f},          {0.0f, 90.0f, 0.0f}    },
        {nullptr,                    nullptr,                    {-15.0f, 0.0f, 5.0f},         {0.0f, 90.0f, 0.0f}    },
        {nullptr,                    nullptr,                    {15.0f, 0.0f, 5.0f},          {0.0f, 90.0f, 0.0f}    },
        {nullptr,                    nullptr,                    {25.0f, 0.0f, -5.0f},         {0.0f, 0.0f, 0.0f}     },
        {nullptr,                    nullptr,                    {25.0f, 0.0f, -15.0f},        {0.0f, 0.0f, 0.0f}     },
        {nullptr,                    nullptr,                    {25.0f, 0.0f, -25.0f},        {0.0f, 0.0f, 0.0f}     },
        {"opdat/norn2/e01c06_0.mds", nullptr,                    {-45.0f, 0.0f, 35.0f},        {0.0f, 180.0f, 0.0f}   },
        {"opdat/norn2/e01c07_0.mds", nullptr,                    {-35.0f, 0.0f, 5.0f},         {0.0f, 90.0f, 0.0f}    },
        {nullptr,                    nullptr,                    {5.0f, 0.0f, 5.0f},           {0.0f, 90.0f, 0.0f}    },
        {"opdat/norn2/e01c08_0.mds", nullptr,                    {25.0f, -0.01f, 5.0f},        {0.0f, 90.0f, 0.0f}    },
        {nullptr,                    nullptr,                    {-45.0f, -0.01f, 5.0f},       {0.0f, -90.0f, 0.0f}   },
        {nullptr,                    nullptr,                    {-45.0f, -0.01f, 25.0f},      {0.0f, 0.0f, 0.0f}     },
        {nullptr,                    nullptr,                    {-45.0f, -0.01f, 15.0f},      {0.0f, 0.0f, 0.0f}     },
        {nullptr,                    nullptr,                    {-25.0f, -0.01f, 5.0f},       {0.0f, 90.0f, 0.0f}    },
        {nullptr,                    nullptr,                    {-5.0f, -0.01f, 5.0f},        {0.0f, 90.0f, 0.0f}    },
        {nullptr,                    nullptr,                    {-15.0f, -0.01f, 5.0f},       {0.0f, 90.0f, 0.0f}    },
        {nullptr,                    nullptr,                    {15.0f, -0.01f, 5.0f},        {0.0f, 90.0f, 0.0f}    },
        {nullptr,                    nullptr,                    {25.0f, -0.01f, -5.0f},       {0.0f, 0.0f, 0.0f}     },
        {nullptr,                    nullptr,                    {25.0f, -0.01f, -15.0f},      {0.0f, 0.0f, 0.0f}     },
        {nullptr,                    nullptr,                    {25.0f, -0.01f, -25.0f},      {0.0f, 0.0f, 0.0f}     },
        {nullptr,                    nullptr,                    {-45.0f, -0.01f, 35.0f},      {0.0f, 180.0f, 0.0f}   },
        {nullptr,                    nullptr,                    {-35.0f, -0.01f, 5.0f},       {0.0f, 90.0f, 0.0f}    },
        {nullptr,                    nullptr,                    {5.0f, -0.01f, 5.0f},         {0.0f, 90.0f, 0.0f}    },
        {"opdat/norn2/e01w01_0.mds", nullptr,                    {25.0f, 0.0f, 5.0f},          {0.0f, 90.0f, 0.0f}    },
        {nullptr,                    nullptr,                    {-45.0f, 0.0f, 5.0f},         {0.0f, -90.0f, 0.0f}   },
        {"opdat/norn2/e01w02_0.mds", nullptr,                    {-45.0f, 0.0f, 25.0f},        {0.0f, 0.0f, 0.0f}     },
        {nullptr,                    nullptr,                    {-45.0f, 0.0f, 15.0f},        {0.0f, 0.0f, 0.0f}     },
        {nullptr,                    nullptr,                    {-25.0f, 0.0f, 5.0f},         {0.0f, 90.0f, 0.0f}    },
        {nullptr,                    nullptr,                    {-5.0f, 0.0f, 5.0f},          {0.0f, 90.0f, 0.0f}    },
        {nullptr,                    nullptr,                    {-15.0f, 0.0f, 5.0f},         {0.0f, 90.0f, 0.0f}    },
        {nullptr,                    nullptr,                    {15.0f, 0.0f, 5.0f},          {0.0f, 90.0f, 0.0f}    },
        {nullptr,                    nullptr,                    {25.0f, 0.0f, -5.0f},         {0.0f, 0.0f, 0.0f}     },
        {nullptr,                    nullptr,                    {25.0f, 0.0f, -15.0f},        {0.0f, 0.0f, 0.0f}     },
        {nullptr,                    nullptr,                    {25.0f, 0.0f, -25.0f},        {0.0f, 0.0f, 0.0f}     },
        {"opdat/norn2/e01w06_0.mds", nullptr,                    {-45.0f, 0.0f, 35.0f},        {0.0f, 180.0f, 0.0f}   },
        {"opdat/norn2/e01r02_0.mds", nullptr,                    {-35.0f, 0.05f, -35.0f},      {0.0f, 90.0f, 0.0f}    },
        {nullptr,                    nullptr,                    {-25.0f, 0.05f, -35.0f},      {0.0f, 90.0f, 0.0f}    },
        {"opdat/norn2/e01r02_0.mds", nullptr,                    {-15.0f, 0.05f, -25.0f},      {0.0f, 0.0f, 0.0f}     },
        {nullptr,                    nullptr,                    {-5.0f, 0.05f, -5.0f},        {0.0f, -90.0f, 0.0f}   },
        {nullptr,                    nullptr,                    {-15.0f, 0.05f, -15.0f},      {0.0f, 0.0f, 0.0f}     },
        {nullptr,                    nullptr,                    {-25.0f, 0.05f, -5.0f},       {0.0f, 90.0f, 0.0f}    },
        {"opdat/norn2/e01r03_0.mds", nullptr,                    {-15.0f, 0.05f, -35.0f},      {0.0f, 0.0f, 0.0f}     },
        {nullptr,                    nullptr,                    {-15.0f, 0.05f, -5.0f},       {0.0f, 180.0f, 0.0f}   },
        {"opdat/norn2/e01r06_0.mds", nullptr,                    {-35.0f, 0.05f, -5.0f},       {0.0f, 90.0f, 0.0f}    },
        {nullptr,                    nullptr,                    {5.0f, 0.05f, -5.0f},         {0.0f, -90.0f, 0.0f}   },
        {"opdat/norn2/t0004.mds",    nullptr,                    {-32.9331f, 0.06f, 10.3033f}, {0.0f, 0.0f, 0.0f}     },
        {nullptr,                    nullptr,                    {-36.9302f, 0.06f, 10.3033f}, {0.0f, 0.0f, 0.0f}     },
        {nullptr,                    nullptr,                    {-36.9302f, 0.06f, -0.0916f}, {0.0f, 0.0f, 0.0f}     },
        {nullptr,                    nullptr,                    {-32.9331f, 0.06f, -0.0916f}, {0.0f, 0.0f, 0.0f}     },
        {nullptr,                    nullptr,                    {3.0797f, 0.06f, -0.0916f},   {0.0f, 0.0f, 0.0f}     },
        {nullptr,                    nullptr,                    {7.0849f, 0.06f, -0.0916f},   {0.0f, 0.0f, 0.0f}     },
        {nullptr,                    nullptr,                    {7.0849f, 0.06f, 10.3125f},   {0.0f, 0.0f, 0.0f}     },
        {nullptr,                    nullptr,                    {3.0797f, 0.06f, 10.3125f},   {0.0f, 0.0f, 0.0f}     },
        {"opdat/norn2/t0213.mds",    nullptr,                    {0.0f, 0.0f, 0.0f},           {0.0f, 0.0f, 0.0f}     }
    };
    CFrameAttr attr;
    CFrameVu1 *frame = nullptr;

    attr.fog_enable = true;
    MapDataBuffer.used = 0;

    for (int i = 0; i < std::ssize(norn); i++) {
        if (norn[i].name) {
            LoadFile(norn[i].name, read_buffer, nullptr);
            frame = LoadMDSFile(read_buffer, &MapDataBuffer, 2, nullptr, nullptr);
        }

        if (i == 4) {
            DoransFuusya[0] = frame;
        }

        if (i == 5) {
            DoransFuusya[1] = frame;
        }

        if (i >= 67 && i < 75) {
            TaimatsuFrame[i - 67] = frame;
        }

        SetFrameAttr(frame, 1);
        frame->SetAttr(attr, 1, 64);

        CMapObject &object = OP_NornMapObj[i];

        object.Initialize();
        object.SetFrame(frame, 0);
        object.handle = 0;
        object.unk_34 = 0;
        PlaceMapObject(object, norn[i]);

        object.FrameObjectOnOff("win1", 0);
        object.FrameObjectOnOff("light1", 0);

        if (norn[i].shadow_name) {
            LoadFile(norn[i].shadow_name, read_buffer, nullptr);
            object.shadow_frame = LoadMDSFile(read_buffer, &MapDataBuffer, 14, nullptr, nullptr);
            object.shadow_offset = -20.0f;
        }
    }

    MAPOBJ_INFO ground[] = {
        {"opdat/norn/t0006.mds",  nullptr, {0.0f, 0.0f, 0.0f},             {0.0f, 0.0f, 0.0f}     },
        {"opdat/norn2/t0001.mds", nullptr, {-33.25f, 0.03f, 13.0707f},     {0.0f, 0.0f, 0.0f}     },
        {nullptr,                 nullptr, {-23.25f, 0.03f, 13.0707f},     {0.0f, 0.0f, 0.0f}     },
        {nullptr,                 nullptr, {-13.25f, 0.03f, 13.0707f},     {0.0f, 0.0f, 0.0f}     },
        {nullptr,                 nullptr, {-3.25f, 0.03f, 13.0707f},      {0.0f, 0.0f, 0.0f}     },
        {nullptr,                 nullptr, {14.9146f, 0.03f, 13.0707f},    {0.0f, 0.0f, 0.0f}     },
        {nullptr,                 nullptr, {24.9146f, 0.03f, 13.0707f},    {0.0f, 0.0f, 0.0f}     },
        {nullptr,                 nullptr, {34.9146f, 0.03f, 13.0707f},    {0.0f, 0.0f, 0.0f}     },
        {nullptr,                 nullptr, {36.4833f, 0.03f, 17.4747f},    {0.0f, -90.0f, 0.0f}   },
        {nullptr,                 nullptr, {36.4833f, 0.03f, 27.4747f},    {0.0f, -90.0f, 0.0f}   },
        {nullptr,                 nullptr, {36.4833f, 0.03f, 37.4747f},    {0.0f, -90.0f, 0.0f}   },
        {nullptr,                 nullptr, {36.4833f, 0.03f, 47.4747f},    {0.0f, -90.0f, 0.0f}   },
        {nullptr,                 nullptr, {34.9598f, 0.03f, 52.2721f},    {0.0f, -180.0f, 0.0f}  },
        {nullptr,                 nullptr, {25.8411f, 0.03f, 52.2721f},    {0.0f, -180.0f, 0.0f}  },
        {nullptr,                 nullptr, {16.0658f, 0.03f, 52.2721f},    {0.0f, -180.0f, 0.0f}  },
        {nullptr,                 nullptr, {6.657f, 0.03f, 52.2721f},      {0.0f, -180.0f, 0.0f}  },
        {nullptr,                 nullptr, {-6.7301f, 0.03f, 52.2721f},    {0.0f, -180.0f, 0.0f}  },
        {nullptr,                 nullptr, {-15.4243f, 0.03f, 52.2721f},   {0.0f, -180.0f, 0.0f}  },
        {nullptr,                 nullptr, {-24.5027f, 0.03f, 52.2721f},   {0.0f, -180.0f, 0.0f}  },
        {nullptr,                 nullptr, {-33.5025f, 0.03f, 52.2721f},   {0.0f, -180.0f, 0.0f}  },
        {nullptr,                 nullptr, {-35.0227f, 0.03f, 47.4712f},   {0.0f, 90.0f, 0.0f}    },
        {nullptr,                 nullptr, {-35.0227f, 0.03f, 37.4712f},   {0.0f, 90.0f, 0.0f}    },
        {nullptr,                 nullptr, {-35.0227f, 0.03f, 27.4712f},   {0.0f, 90.0f, 0.0f}    },
        {nullptr,                 nullptr, {-35.0227f, 0.03f, 17.4712f},   {0.0f, 90.0f, 0.0f}    },
        {"opdat/norn2/t0003.mds", nullptr, {-28.23f, 0.03f, 13.1174f},     {0.0f, 0.0f, 0.0f}     },
        {nullptr,                 nullptr, {-18.23f, 0.03f, 13.1174f},     {0.0f, 0.0f, 0.0f}     },
        {nullptr,                 nullptr, {-8.23f, 0.03f, 13.1174f},      {0.0f, 0.0f, 0.0f}     },
        {nullptr,                 nullptr, {19.937f, 0.03f, 13.1174f},     {0.0f, 0.0f, 0.0f}     },
        {nullptr,                 nullptr, {29.937f, 0.03f, 13.1174f},     {0.0f, 0.0f, 0.0f}     },
        {nullptr,                 nullptr, {36.4365f, 0.03f, 22.4967f},    {0.0f, -90.0f, 0.0f}   },
        {nullptr,                 nullptr, {36.4365f, 0.03f, 32.4967f},    {0.0f, -90.0f, 0.0f}   },
        {nullptr,                 nullptr, {36.4365f, 0.03f, 42.4967f},    {0.0f, -90.0f, 0.0f}   },
        {nullptr,                 nullptr, {31.6374f, 0.03f, 52.3176f},    {0.0f, -180.0f, 0.0f}  },
        {nullptr,                 nullptr, {21.6374f, 0.03f, 52.3176f},    {0.0f, -180.0f, 0.0f}  },
        {nullptr,                 nullptr, {11.6374f, 0.03f, 52.3176f},    {0.0f, -180.0f, 0.0f}  },
        {nullptr,                 nullptr, {-10.1013f, 0.03f, 52.3176f},   {0.0f, -180.0f, 0.0f}  },
        {nullptr,                 nullptr, {-20.1013f, 0.03f, 52.3176f},   {0.0f, -180.0f, 0.0f}  },
        {nullptr,                 nullptr, {-30.1013f, 0.03f, 52.3176f},   {0.0f, -180.0f, 0.0f}  },
        {nullptr,                 nullptr, {-34.9775f, 0.03f, 42.4508f},   {0.0f, 90.0f, 0.0f}    },
        {nullptr,                 nullptr, {-34.9775f, 0.03f, 32.4508f},   {0.0f, 90.0f, 0.0f}    },
        {nullptr,                 nullptr, {-34.9775f, 0.03f, 22.4508f},   {0.0f, 90.0f, 0.0f}    },
        {"opdat/norn2/t0212.mds", nullptr, {0.0f, 0.02f, 30.0f},           {0.0f, 0.0f, 0.0f}     },
        {"opdat/norn2/t0208.mds", nullptr, {0.0f, 0.0f, 29.999f},          {0.0f, 0.0f, 0.0f}     },
        {"opdat/norn2/t0201.mds", nullptr, {8.1246f, 0.0f, 21.2374f},      {0.0f, -190.452f, 0.0f}},
        {nullptr,                 nullptr, {9.3114f, 0.0f, 21.4458f},      {0.0f, -243.621f, 0.0f}},
        {nullptr,                 nullptr, {9.8278f, 0.0f, 22.3789f},      {0.0f, -237.204f, 0.0f}},
        {nullptr,                 nullptr, {9.6149f, 1.2748f, 21.9196f},   {0.0f, -237.777f, 0.0f}},
        {nullptr,                 nullptr, {-10.8524f, 0.0f, 38.0058f},    {0.0f, 279.8325f, 0.0f}},
        {nullptr,                 nullptr, {-11.6807f, 0.0f, 36.7247f},    {0.0f, 281.7807f, 0.0f}},
        {nullptr,                 nullptr, {-11.9794f, 0.0f, 37.8902f},    {0.0f, 287.5107f, 0.0f}},
        {nullptr,                 nullptr, {-12.2587f, 0.0f, 39.046f},     {0.0f, 288.9999f, 0.0f}},
        {nullptr,                 nullptr, {-10.9969f, 0.0f, 39.1231f},    {0.0f, 280.291f, 0.0f} },
        {nullptr,                 nullptr, {24.4449f, 0.0f, 28.8081f},     {0.0f, -84.5684f, 0.0f}},
        {nullptr,                 nullptr, {24.5097f, 1.2748f, 29.317f},   {0.0f, -84.5684f, 0.0f}},
        {nullptr,                 nullptr, {24.7114f, 0.0f, 29.8031f},     {0.0f, -84.5684f, 0.0f}},
        {nullptr,                 nullptr, {24.9802f, 1.2748f, 30.2274f},  {0.0f, -84.5684f, 0.0f}},
        {nullptr,                 nullptr, {25.2412f, 0.0f, 30.6682f},     {0.0f, -84.5684f, 0.0f}},
        {"opdat/norn2/t0202.mds", nullptr, {6.9352f, 0.0f, 22.8016f},      {0.0f, -66.4631f, 0.0f}},
        {nullptr,                 nullptr, {-14.5591f, 0.0f, 35.5869f},    {0.0f, 112.4143f, 0.0f}},
        {"opdat/norn2/t0203.mds", nullptr, {8.5959f, 0.0f, 23.4822f},      {0.0f, -49.7327f, 0.0f}},
        {nullptr,                 nullptr, {-13.0716f, 0.0f, 36.4929f},    {0.0f, 129.2592f, 0.0f}},
        {nullptr,                 nullptr, {-14.4103f, 0.0f, 37.4364f},    {0.0f, 112.5289f, 0.0f}},
        {"opdat/norn2/t0204.mds", nullptr, {-9.0122f, 0.0f, 20.8749f},     {0.0f, -234.912f, 0.0f}},
        {nullptr,                 nullptr, {-9.9647f, 0.0f, 21.4127f},     {0.0f, -227.234f, 0.0f}},
        {nullptr,                 nullptr, {-10.7374f, 0.0f, 22.1576f},    {0.0f, -223.797f, 0.0f}},
        {nullptr,                 nullptr, {-9.4268f, 0.9971f, 21.1822f},  {0.0f, -241.444f, 0.0f}},
        {nullptr,                 nullptr, {-10.3487f, 0.9971f, 21.8464f}, {0.0f, -225.287f, 0.0f}},
        {nullptr,                 nullptr, {-9.8716f, 1.9904f, 21.4498f},  {0.0f, -228.61f, 0.0f} },
        {nullptr,                 nullptr, {12.8223f, 0.0f, 26.3336f},     {0.0f, -343.545f, 0.0f}},
        {nullptr,                 nullptr, {11.8478f, 0.0f, 26.9861f},     {0.0f, -348.702f, 0.0f}},
        {nullptr,                 nullptr, {11.9701f, 0.0f, 28.1885f},     {0.0f, -354.661f, 0.0f}},
        {nullptr,                 nullptr, {13.085f, 0.0f, 27.3741f},      {0.0f, -346.983f, 0.0f}},
        {nullptr,                 nullptr, {11.7334f, 0.9971f, 27.8052f},  {0.0f, -348.358f, 0.0f}},
        {nullptr,                 nullptr, {12.58f, 0.9971f, 26.905f},     {0.0f, -337.013f, 0.0f}},
        {nullptr,                 nullptr, {12.8088f, 0.0f, 38.6433f},     {0.0f, -382.965f, 0.0f}},
        {nullptr,                 nullptr, {11.6416f, 0.0f, 38.5285f},     {0.0f, -388.121f, 0.0f}},
        {nullptr,                 nullptr, {10.9726f, 0.0f, 39.535f},      {0.0f, -394.08f, 0.0f} },
        {nullptr,                 nullptr, {12.3514f, 0.0f, 39.6143f},     {0.0f, -386.402f, 0.0f}},
        {nullptr,                 nullptr, {11.0331f, 0.9971f, 39.0886f},  {0.0f, -387.777f, 0.0f}},
        {nullptr,                 nullptr, {12.2587f, 0.9971f, 38.9308f},  {0.0f, -376.433f, 0.0f}},
        {"opdat/norn2/t0205.mds", nullptr, {-9.8702f, 2.9808f, 21.5102f},  {0.0f, -140.374f, 0.0f}},
        {nullptr,                 nullptr, {-11.6688f, 1.2789f, 36.7089f}, {0.0f, -61.7649f, 0.0f}},
        {nullptr,                 nullptr, {12.4831f, 1.9258f, 26.9556f},  {0.0f, -84.3394f, 0.0f}},
        {"opdat/norn2/t0206.mds", nullptr, {-4.3913f, 0.0f, 18.9443f},     {0.0f, 18.3347f, 0.0f} },
        {nullptr,                 nullptr, {-13.2586f, 0.0f, 27.5378f},    {0.0f, 77.6931f, 0.0f} },
        {nullptr,                 nullptr, {4.648f, 0.0f, 19.1829f},       {0.0f, -21.6578f, 0.0f}},
        {nullptr,                 nullptr, {13.7675f, 0.0f, 33.1962f},     {0.0f, -110.58f, 0.0f} }
    };

    CFrameVu1 *ground_frame = nullptr;

    for (int j = 0; j < std::ssize(ground); j++) {
        if (ground[j].name) {
            LoadFile(ground[j].name, read_buffer, nullptr);
            ground_frame = LoadMDSFile(read_buffer, &MapDataBuffer, 2, nullptr, nullptr);
        }

        if (j > 0) {
            ground_frame->SetAttr(attr, 1, 64);
            SetFrameAttr(ground_frame, 1);
        }

        if (j == 41) {
            TaimatsuFrame[8] = ground_frame;
        }

        if (j == 0) {
            CFrame *sun = ground_frame->SearchFrame("sun3");

            sun->SetPosition(420.0f, 400.0f, -30.0f);
        }

        CMapObject &object = OP_NornMapObj2[j];

        object.Initialize();
        object.SetFrame(ground_frame, 0);
        object.handle = 0;
        object.unk_34 = 0;
        PlaceMapObject(object, ground[j]);
    }
}

/**
 * The same village after the demon has burned it, which is a second layout rather than a change to
 * the first: the models come out of the pack the background read left in memory rather than off the
 * disc, so every row is a name inside it.
 */
void MapLoad2() {
    MAPOBJ_INFO norn[] = {
        {"b0301.mds",    nullptr, {0.0f, 0.0f, 0.0f},           {0.0f, 0.0f, 0.0f}     },
        {"e01h01_1.mds", nullptr, {-35.0f, 0.0f, -20.0f},       {0.0f, 0.0f, 0.0f}     },
        {"e01h05_2.mds", nullptr, {50.0f, 0.0f, 20.0f},         {0.0f, -90.0f, 0.0f}   },
        {"e01h11_2.mds", nullptr, {55.0f, 0.0f, -10.0f},        {0.0f, -90.0f, 0.0f}   },
        {"e01h07_2.mds", nullptr, {5.0f, 0.0f, -20.0f},         {0.0f, 0.0f, 0.0f}     },
        {"e01h02_2.mds", nullptr, {-60.0f, 0.0f, 30.0f},        {0.0f, 90.0f, 0.0f}    },
        {"e01h06_2.mds", nullptr, {0.0f, 4.7424f, 79.274f},     {0.0f, 0.0f, 0.0f}     },
        {"e01a02_1.mds", nullptr, {0.0f, 0.0f, 59.965f},        {0.0f, 0.0f, 0.0f}     },
        {"t0304.mds",    nullptr, {-5.0f, 0.0f, -45.0f},        {0.0f, 0.0f, 0.0f}     },
        {nullptr,        nullptr, {-25.0f, 0.0f, -45.0f},       {0.0f, 0.0f, 0.0f}     },
        {"t0305.mds",    nullptr, {45.0f, 0.0f, -50.0f},        {0.0f, 0.0f, 0.0f}     },
        {"t0002.mds",    nullptr, {-23.7809f, 0.0f, 19.8331f},  {0.0f, -4.4f, 0.0f}    },
        {nullptr,        nullptr, {23.1908f, 0.0f, 38.0943f},   {0.0f, -170.971f, 0.0f}},
        {"e01t01_0.mds", nullptr, {-45.0f, 0.1f, -5.0f},        {0.0f, 0.0f, 0.0f}     },
        {nullptr,        nullptr, {65.0f, 0.1f, 45.0f},         {0.0f, 0.0f, 0.0f}     },
        {"e01t01_1.mds", nullptr, {55.0f, 0.1f, -35.0f},        {0.0f, -90.0f, 0.0f}   },
        {nullptr,        nullptr, {-45.0f, 0.1f, 55.0f},        {0.0f, -90.0f, 0.0f}   },
        {nullptr,        nullptr, {-55.0f, 0.1f, 5.0f},         {0.0f, -90.0f, 0.0f}   },
        {"e01c01_0.mds", nullptr, {25.0f, 0.0f, 5.0f},          {0.0f, 90.0f, 0.0f}    },
        {nullptr,        nullptr, {-45.0f, 0.0f, 5.0f},         {0.0f, -90.0f, 0.0f}   },
        {"e01c02_0.mds", nullptr, {-45.0f, 0.0f, 25.0f},        {0.0f, 0.0f, 0.0f}     },
        {nullptr,        nullptr, {-45.0f, 0.0f, 15.0f},        {0.0f, 0.0f, 0.0f}     },
        {nullptr,        nullptr, {-25.0f, 0.0f, 5.0f},         {0.0f, 90.0f, 0.0f}    },
        {nullptr,        nullptr, {-5.0f, 0.0f, 5.0f},          {0.0f, 90.0f, 0.0f}    },
        {nullptr,        nullptr, {-15.0f, 0.0f, 5.0f},         {0.0f, 90.0f, 0.0f}    },
        {nullptr,        nullptr, {15.0f, 0.0f, 5.0f},          {0.0f, 90.0f, 0.0f}    },
        {nullptr,        nullptr, {25.0f, 0.0f, -5.0f},         {0.0f, 0.0f, 0.0f}     },
        {nullptr,        nullptr, {25.0f, 0.0f, -15.0f},        {0.0f, 0.0f, 0.0f}     },
        {nullptr,        nullptr, {25.0f, 0.0f, -25.0f},        {0.0f, 0.0f, 0.0f}     },
        {"e01c06_0.mds", nullptr, {-45.0f, 0.0f, 35.0f},        {0.0f, 180.0f, 0.0f}   },
        {"e01c07_0.mds", nullptr, {-35.0f, 0.0f, 5.0f},         {0.0f, 90.0f, 0.0f}    },
        {nullptr,        nullptr, {5.0f, 0.0f, 5.0f},           {0.0f, 90.0f, 0.0f}    },
        {"e01c08_0.mds", nullptr, {25.0f, -0.01f, 5.0f},        {0.0f, 90.0f, 0.0f}    },
        {nullptr,        nullptr, {-45.0f, -0.01f, 5.0f},       {0.0f, -90.0f, 0.0f}   },
        {nullptr,        nullptr, {-45.0f, -0.01f, 25.0f},      {0.0f, 0.0f, 0.0f}     },
        {nullptr,        nullptr, {-45.0f, -0.01f, 15.0f},      {0.0f, 0.0f, 0.0f}     },
        {nullptr,        nullptr, {-25.0f, -0.01f, 5.0f},       {0.0f, 90.0f, 0.0f}    },
        {nullptr,        nullptr, {-5.0f, -0.01f, 5.0f},        {0.0f, 90.0f, 0.0f}    },
        {nullptr,        nullptr, {-15.0f, -0.01f, 5.0f},       {0.0f, 90.0f, 0.0f}    },
        {nullptr,        nullptr, {15.0f, -0.01f, 5.0f},        {0.0f, 90.0f, 0.0f}    },
        {nullptr,        nullptr, {25.0f, -0.01f, -5.0f},       {0.0f, 0.0f, 0.0f}     },
        {nullptr,        nullptr, {25.0f, -0.01f, -15.0f},      {0.0f, 0.0f, 0.0f}     },
        {nullptr,        nullptr, {25.0f, -0.01f, -25.0f},      {0.0f, 0.0f, 0.0f}     },
        {nullptr,        nullptr, {-45.0f, -0.01f, 35.0f},      {0.0f, 180.0f, 0.0f}   },
        {nullptr,        nullptr, {-35.0f, -0.01f, 5.0f},       {0.0f, 90.0f, 0.0f}    },
        {nullptr,        nullptr, {5.0f, -0.01f, 5.0f},         {0.0f, 90.0f, 0.0f}    },
        {"e01w01_0.mds", nullptr, {25.0f, 0.0f, 5.0f},          {0.0f, 90.0f, 0.0f}    },
        {nullptr,        nullptr, {-45.0f, 0.0f, 5.0f},         {0.0f, -90.0f, 0.0f}   },
        {"e01w02_0.mds", nullptr, {-45.0f, 0.0f, 25.0f},        {0.0f, 0.0f, 0.0f}     },
        {nullptr,        nullptr, {-45.0f, 0.0f, 15.0f},        {0.0f, 0.0f, 0.0f}     },
        {nullptr,        nullptr, {-25.0f, 0.0f, 5.0f},         {0.0f, 90.0f, 0.0f}    },
        {nullptr,        nullptr, {-5.0f, 0.0f, 5.0f},          {0.0f, 90.0f, 0.0f}    },
        {nullptr,        nullptr, {-15.0f, 0.0f, 5.0f},         {0.0f, 90.0f, 0.0f}    },
        {nullptr,        nullptr, {15.0f, 0.0f, 5.0f},          {0.0f, 90.0f, 0.0f}    },
        {nullptr,        nullptr, {25.0f, 0.0f, -5.0f},         {0.0f, 0.0f, 0.0f}     },
        {nullptr,        nullptr, {25.0f, 0.0f, -15.0f},        {0.0f, 0.0f, 0.0f}     },
        {nullptr,        nullptr, {25.0f, 0.0f, -25.0f},        {0.0f, 0.0f, 0.0f}     },
        {"e01w06_0.mds", nullptr, {-45.0f, 0.0f, 35.0f},        {0.0f, 180.0f, 0.0f}   },
        {"e01r02_0.mds", nullptr, {-35.0f, 0.05f, -35.0f},      {0.0f, 90.0f, 0.0f}    },
        {nullptr,        nullptr, {-25.0f, 0.05f, -35.0f},      {0.0f, 90.0f, 0.0f}    },
        {"e01r02_0.mds", nullptr, {-15.0f, 0.05f, -25.0f},      {0.0f, 0.0f, 0.0f}     },
        {nullptr,        nullptr, {-5.0f, 0.05f, -5.0f},        {0.0f, -90.0f, 0.0f}   },
        {nullptr,        nullptr, {-15.0f, 0.05f, -15.0f},      {0.0f, 0.0f, 0.0f}     },
        {nullptr,        nullptr, {-25.0f, 0.05f, -5.0f},       {0.0f, 90.0f, 0.0f}    },
        {"e01r03_0.mds", nullptr, {-15.0f, 0.05f, -35.0f},      {0.0f, 0.0f, 0.0f}     },
        {nullptr,        nullptr, {-15.0f, 0.05f, -5.0f},       {0.0f, 180.0f, 0.0f}   },
        {"e01r06_0.mds", nullptr, {-35.0f, 0.05f, -5.0f},       {0.0f, 90.0f, 0.0f}    },
        {nullptr,        nullptr, {5.0f, 0.05f, -5.0f},         {0.0f, -90.0f, 0.0f}   },
        {"t0302.mds",    nullptr, {2.9721f, 0.06f, -0.6331f},   {0.0f, 36.2109f, 0.0f} },
        {nullptr,        nullptr, {7.2685f, 0.06f, 11.4402f},   {0.0f, -116.883f, 0.0f}},
        {nullptr,        nullptr, {2.9715f, 0.06f, 12.3798f},   {0.0f, 135.9057f, 0.0f}},
        {nullptr,        nullptr, {7.6022f, 0.06f, -3.0402f},   {0.0f, 75.5158f, 0.0f} },
        {nullptr,        nullptr, {-35.9097f, 0.06f, 12.6724f}, {0.0f, 145.6459f, 0.0f}},
        {"t0213.mds",    nullptr, {0.0f, 0.0f, 0.0f},           {0.0f, 0.0f, 0.0f}     },
        {"t0306.mds",    nullptr, {45.0f, 0.0f, 50.0f},         {0.0f, 90.0f, 0.0f}    },
        {"t0307.mds",    nullptr, {45.0f, 0.0f, 50.0f},         {0.0f, 180.0f, 0.0f}   }
    };
    CFrameAttr attr;
    CFrameVu1 *frame = nullptr;

    attr.fog_enable = true;
    CharaDataBuffer__2[0].used = 0;

    for (int i = 0; i < std::ssize(norn); i++) {
        if (norn[i].name) {
            frame = LoadMDSFile(GetPackFile(read_buffer, norn[i].name, nullptr), &CharaDataBuffer__2[0], 2, nullptr, nullptr);
        }

        if (i == 4) {
            DoransFuusya[0] = frame;
        }

        if (i == 74) {
            DoransFuusya[1] = frame;
        }

        if (i >= 69 && i < 74) {
            TaimatsuFrame[i - 69] = frame;
        }

        frame->SetAttr(attr, 1, 64);
        SetFrameAttr(frame, 1);

        CMapObject &object = OP_NornMapObj[i];

        object.Initialize();
        object.SetFrame(frame, 0);
        object.handle = 0;
        object.unk_34 = 0;
        PlaceMapObject(object, norn[i]);

        object.FrameObjectOnOff("win1", 0);
        object.FrameObjectOnOff("light1", 0);

        if (norn[i].shadow_name) {
            LoadFile(norn[i].shadow_name, read_buffer, nullptr);
            object.shadow_frame = LoadMDSFile(read_buffer, &MapDataBuffer, 14, nullptr, nullptr);
            object.shadow_offset = -20.0f;
        }
    }

    MAPOBJ_INFO ground[] = {
        {"t0006.mds", nullptr, {0.0f, 0.0f, 0.0f},             {0.0f, 0.0f, 0.0f}     },
        {"t0001.mds", nullptr, {-33.25f, 0.03f, 13.0707f},     {0.0f, 0.0f, 0.0f}     },
        {nullptr,     nullptr, {-23.25f, 0.03f, 13.0707f},     {0.0f, 0.0f, 0.0f}     },
        {nullptr,     nullptr, {-3.25f, 0.03f, 13.0707f},      {0.0f, 0.0f, 0.0f}     },
        {nullptr,     nullptr, {14.9146f, 0.03f, 13.0707f},    {0.0f, 0.0f, 0.0f}     },
        {nullptr,     nullptr, {24.9146f, 0.03f, 13.0707f},    {0.0f, 0.0f, 0.0f}     },
        {nullptr,     nullptr, {34.9146f, 0.03f, 13.0707f},    {0.0f, 0.0f, 0.0f}     },
        {nullptr,     nullptr, {36.4833f, 0.03f, 17.4747f},    {0.0f, -90.0f, 0.0f}   },
        {nullptr,     nullptr, {6.657f, 0.03f, 52.2721f},      {0.0f, -180.0f, 0.0f}  },
        {nullptr,     nullptr, {-24.5027f, 0.03f, 52.2721f},   {0.0f, -180.0f, 0.0f}  },
        {nullptr,     nullptr, {-33.5025f, 0.03f, 52.2721f},   {0.0f, -180.0f, 0.0f}  },
        {nullptr,     nullptr, {-35.0227f, 0.03f, 37.4712f},   {0.0f, 90.0f, 0.0f}    },
        {nullptr,     nullptr, {-35.0227f, 0.03f, 27.4712f},   {0.0f, 90.0f, 0.0f}    },
        {"t0301.mds", nullptr, {-35.6934f, 0.0f, 20.4342f},    {0.0f, 130.0f, 0.0f}   },
        {nullptr,     nullptr, {-32.5089f, 0.0f, 46.7067f},    {0.0f, -74.3699f, 0.0f}},
        {nullptr,     nullptr, {-10.4729f, 0.0f, 13.2973f},    {0.0f, 79.7557f, 0.0f} },
        {nullptr,     nullptr, {15.1651f, 0.0f, 54.5186f},     {0.0f, 207.0f, 0.0f}   },
        {nullptr,     nullptr, {35.441f, 0.0f, 28.889f},       {0.0f, 156.3029f, 0.0f}},
        {nullptr,     nullptr, {-6.7649f, 0.0f, 49.889f},      {0.0f, 17.6471f, 0.0f} },
        {nullptr,     nullptr, {-17.6795f, 0.0f, 54.369f},     {0.0f, 105.768f, 0.0f} },
        {nullptr,     nullptr, {34.153f, 0.0f, 39.193f},       {0.0f, 42.628f, 0.0f}  },
        {nullptr,     nullptr, {28.736f, 0.0f, 55.7777f},      {0.0f, 121.467f, 0.0f} },
        {"t0003.mds", nullptr, {-28.23f, 0.03f, 13.1174f},     {0.0f, 0.0f, 0.0f}     },
        {nullptr,     nullptr, {19.937f, 0.03f, 13.1174f},     {0.0f, 0.0f, 0.0f}     },
        {nullptr,     nullptr, {29.937f, 0.03f, 13.1174f},     {0.0f, 0.0f, 0.0f}     },
        {nullptr,     nullptr, {-30.1013f, 0.03f, 52.3176f},   {0.0f, -180.0f, 0.0f}  },
        {"t0212.mds", nullptr, {0.0f, 0.02f, 30.0f},           {0.0f, 0.0f, 0.0f}     },
        {"t0208.mds", nullptr, {0.0f, 0.0f, 29.999f},          {0.0f, 0.0f, 0.0f}     },
        {"t0201.mds", nullptr, {9.8278f, 0.0f, 22.3789f},      {0.0f, -237.204f, 0.0f}},
        {nullptr,     nullptr, {-11.6807f, 0.0f, 36.7247f},    {0.0f, 281.7807f, 0.0f}},
        {"t0303.mds", nullptr, {5.6932f, 0.0f, 21.7183f},      {0.0f, -40.0f, 0.0f}   },
        {nullptr,     nullptr, {8.0326f, 0.0f, 25.0416f},      {0.0f, 57.0f, 0.0f}    },
        {nullptr,     nullptr, {8.1469f, 0.0f, 18.4168f},      {0.0f, 77.0f, 0.0f}    },
        {nullptr,     nullptr, {-11.9749f, 0.0f, 34.2407f},    {0.0f, 157.0f, 0.0f}   },
        {nullptr,     nullptr, {-10.5469f, 0.0f, 36.1527f},    {0.0f, 217.0f, 0.0f}   },
        {nullptr,     nullptr, {-18.8727f, 0.0f, 32.604f},     {0.0f, 40.0f, 0.0f}    },
        {nullptr,     nullptr, {12.1088f, 0.0f, 43.2429f},     {0.0f, -55.0f, 0.0f}   },
        {nullptr,     nullptr, {-25.5735f, 0.0f, 30.4597f},    {0.0f, -52.0f, 0.0f}   },
        {nullptr,     nullptr, {-27.1088f, 0.0f, 29.2536f},    {0.0f, 22.0f, 0.0f}    },
        {nullptr,     nullptr, {-28.1088f, 0.0f, 33.4081f},    {0.0f, 78.0f, 0.0f}    },
        {"t0202.mds", nullptr, {6.9352f, 0.0f, 22.8016f},      {0.0f, -66.4631f, 0.0f}},
        {nullptr,     nullptr, {-14.5591f, 0.0f, 35.5869f},    {0.0f, 112.4143f, 0.0f}},
        {"t0203.mds", nullptr, {8.5959f, 0.0f, 23.4822f},      {0.0f, -49.7327f, 0.0f}},
        {nullptr,     nullptr, {-13.0716f, 0.0f, 36.4929f},    {0.0f, 129.2592f, 0.0f}},
        {nullptr,     nullptr, {-14.4103f, 0.0f, 37.4364f},    {0.0f, 112.5289f, 0.0f}},
        {"t0204.mds", nullptr, {19.0043f, 0.0f, 20.5515f},     {0.0f, 18.2201f, 0.0f} },
        {nullptr,     nullptr, {23.1962f, 0.0f, 27.52f},       {0.0f, 71.7343f, 0.0f} },
        {nullptr,     nullptr, {19.0335f, 0.0f, 22.3668f},     {0.0f, 39.5341f, 0.0f} },
        {nullptr,     nullptr, {15.9599f, 0.0f, 27.5015f},     {0.0f, -11.6883f, 0.0f}},
        {nullptr,     nullptr, {-11.7777f, 0.0f, 28.0256f},    {0.0f, -16.9596f, 0.0f}},
        {nullptr,     nullptr, {-8.2194f, 0.0f, 22.6732f},     {0.0f, -23.3767f, 0.0f}},
        {nullptr,     nullptr, {-8.9274f, 0.0f, 17.7011f},     {0.0f, 28.9917f, 0.0f} },
        {nullptr,     nullptr, {-15.2388f, 0.0f, 22.5856f},    {0.0f, -15.5854f, 0.0f}},
        {nullptr,     nullptr, {-12.4433f, 0.0f, 23.0674f},    {0.0f, 15.2407f, 0.0f} },
        {nullptr,     nullptr, {-11.3862f, 0.0f, 21.697f},     {0.0f, -10.657f, 0.0f} },
        {nullptr,     nullptr, {11.704f, 0.0f, 38.1355f},      {0.0f, -45.7425f, 0.0f}},
        {nullptr,     nullptr, {14.5128f, 0.0f, 38.3406f},     {0.0f, -16.272f, 0.0f} },
        {nullptr,     nullptr, {10.6253f, 0.0f, 40.4613f},     {0.0f, 25.3247f, 0.0f} },
        {nullptr,     nullptr, {-21.0844f, 0.0f, 38.2517f},    {0.0f, 50.0f, 0.0f}    },
        {"t0205.mds", nullptr, {-11.6688f, 1.2789f, 36.7089f}, {0.0f, -61.7649f, 0.0f}},
        {"t0206.mds", nullptr, {-4.3913f, 0.0f, 18.9443f},     {0.0f, 18.3347f, 0.0f} },
        {nullptr,     nullptr, {13.2586f, 0.0f, 33.1962f},     {0.0f, 110.58f, 0.0f}  }
    };

    CFrameVu1 *ground_frame = nullptr;

    for (int j = 0; j < std::ssize(ground); j++) {
        if (ground[j].name) {
            ground_frame = LoadMDSFile(GetPackFile(read_buffer, ground[j].name, nullptr), &CharaDataBuffer__2[6], 2, nullptr, nullptr);
        }

        if (j > 0) {
            ground_frame->SetAttr(attr, 1, 64);
            SetFrameAttr(ground_frame, 1);
        }

        if (j == 26) {
            TaimatsuFrame[8] = ground_frame;
        }

        CMapObject &object = OP_NornMapObj2[j];

        object.Initialize();
        object.SetFrame(ground_frame, 0);
        object.handle = 0;
        object.unk_34 = 0;
        PlaceMapObject(object, ground[j]);

        if (j == 0) {
            object.FrameObjectOnOff("sun3", 0);
        }
    }
}

/**
 * The demon's second half, which is loaded while the scene is already running: the script asks for
 * it by putting a load number on the actor, and this is the three ticks that answer — start the
 * read, wait for it, unpack it — after which both numbers go back to none.
 */
void LoadCharaMajin() {
    switch (CScript__2.obj[4].load_step) {
        case TSLOAD_START:
            LoadFileBG("opdat/norn2/majinb.pak", reinterpret_cast<u_long128 *>(read_buffer), nullptr);
            CScript__2.obj[4].load_step = TSLOAD_WAIT;
            break;

        case TSLOAD_WAIT:
            if (!ReadBGSync()) {
                CScript__2.obj[4].load_step = TSLOAD_UNPACK;
            }

            break;

        case TSLOAD_UNPACK:
            Chara__3[4].Initialize();
            Chara__3[4].LoadPackData(read_buffer, "04c08b.cfg", &CharaDataBuffer__2[6], nullptr);
            Chara__3[4].motion_type.state.time = 5.0f;
            Chara__3[4].motion_type.state.blend_step = 1.0f;
            Chara__3[4].motion_type.state.motion_no = 0;
            Chara__3[4].motion_type.state.playing_no = 0;
            CScript__2.obj[4].load = -1;
            CScript__2.obj[4].load_step = TSLOAD_NONE;
            break;

        default:
            break;
    }
}

// Draws shadow `shadow_no` under the frame named `name` within `root`, if it has one.
void DrawFrameShadow(CFrame *root, char *name, int shadow_no) {
    CFrame *frame = root->SearchFrame(name);

    if (frame) {
        sceVu0FMATRIX matrix;

        frame->GetLWMatrix(matrix);
        DrawShadow(shadow_no, matrix[3][0], matrix[3][1], matrix[3][2]);
    }
}

// Draws `actor`'s model into dancer slot `slot` at the camera frame named `anchor`, with foot
// shadows `shadow_no` and `shadow_no + 1`.
void DrawDancerAt(int slot, int actor, char *anchor, int shadow_no) {
    TexManager.ReloadTexture(Vif1Packet, CharaTex__2[actor]);
    CharaF[slot] = Chara__3[actor].frame;

    CFrame *anchor_frame = Cam__2[SceneNp__2].frame->SearchFrame(anchor);

    if (!anchor_frame) {
        return;
    }

    sceVu0FMATRIX matrix;

    anchor_frame->GetLWMatrix(matrix);
    CharaF[slot]->SetRotation(0.0f, atan2f(matrix[2][0], matrix[2][2]), 0.0f);
    CharaF[slot]->SetPosition(matrix[3][0], matrix[3][1], matrix[3][2]);
    LightSet(matrix[3][0], matrix[3][1], matrix[3][2], 0);
    MGDraw(CharaF[slot]);

    DrawFrameShadow(CharaF[slot], "r_foot", shadow_no);
    DrawFrameShadow(CharaF[slot], "l_foot", shadow_no + 1);
}

/**
 * The two dancers the crowd is watching, which are drawn from the camera's own frame tree rather
 * than from the scene: each is placed where a named frame of the camera's model stands, lit from
 * there, and given a pair of foot shadows of its own.
 */
void DrawDancer() {
    if (CScript__2.obj[19].disp) {
        DrawDancerAt(0, 19, "p15a1", 10);
        DrawDancerAt(1, 19, "p15a2", 12);
    }

    if (CScript__2.obj[20].disp) {
        DrawDancerAt(2, 20, "p16a1", 14);
    }
}

/**
 * Every dancer's feet, drawn as flat models placed under the frames they belong to rather than as
 * shadows the renderer casts: the scene has more actors than the shadow buffer would hold.
 */
void InchikiShadow() {
    struct FootShadows {
        int         actor;
        const char *right;
        const char *left;
        int         shadow_no;
    };

    static constexpr std::array<FootShadows, 7> kFeet = {
        {{14, "r_foot", "l_foot", 0},
         {15, "r_foot", "l_foot", 2},
         {16, "r_foot", "l_foot", 4},
         {17, "eff19", "eff7", 4},
         {18, "r_foot", "l_foot", 4},
         {19, "r_foot", "l_foot", 6},
         {20, "r_foot", "l_foot", 6}}
    };

    for (const FootShadows &feet : kFeet) {
        if (CScript__2.obj[feet.actor].disp) {
            DrawFrameShadow(Chara__3[feet.actor].frame, const_cast<char *>(feet.right), feet.shadow_no);
            DrawFrameShadow(Chara__3[feet.actor].frame, const_cast<char *>(feet.left), feet.shadow_no + 1);
        }
    }
}

/**
 * One of those flat models, placed and turned to face the light rather than the camera. The ambient
 * colour it is drawn under falls with the height it is drawn at, so a foot lifted far enough off
 * the ground has no shadow at all.
 */
void DrawShadow(int shadow_no, float x, float y, float z) {
    sceVu0FVECTOR ambient = {0.0f, 0.0f, 0.0f, 54.0f};

    TexManager.ReloadTexture(Vif1Packet, 18);
    ambient[3] = ambient[3] - 10.0f * y;

    if (ambient[3] > 0.0f) {
        MGSetAmbient(ambient);

        float heading = atan2f(x, z - 300.0f);

        Shadow[shadow_no]->SetPosition(x, 0.05f, z);
        Shadow[shadow_no]->SetRotation(1.5533431f, heading, 0.0f);
        MGDraw(Shadow[shadow_no]);
        MGSetAmbient(ambientlight);
    }
}

// Fires `beam` from `from` towards `to`, filling its trail with copies of the head.
void FireBeam(CMajinBeem &beam, float *from, float *to, float speed, float head_size) {
    sceVu0CopyVector(beam.target, to);
    beam.speed = speed;

    for (int i = 0; i < 60; i++) {
        sceVu0CopyVector(beam.positions[i], from);
        beam.state = MAJIN_BEEM_AIM;
        beam.counters[i] = -i;
        beam.alphas[i] = 128.0f - static_cast<float>(2 * i);
        beam.sizes[i] = 20.0f - static_cast<float>(i / 3);

        beam.sizes[i] = std::max(beam.sizes[i], 8.0f);

        beam.active = true;
    }

    for (int i = 0; i < 10; i++) {
        beam.sizes[i] = 20.0f;
    }

    beam.alphas[59] = 128.0f;
    beam.sizes[59] = head_size;
}

/**
 * The beam the demon fires, which is started by the script and then runs on its own. A beam the
 * script marks as ending is aimed from a frame of the demon's own model at the frame the camera
 * carries; one it does not is aimed between the two points the script names.
 */
void MajinBeemProcess() {
    static constexpr std::array<const char *, 10> kFrameNames = {"yubi", "heso", "l_asi", "r_asi", "kuti",
                                                                 "toutyou", "yubi", "yubi", "yubi", "heso"};

    if (CScript__2.beem_req) {
        float speed = CScript__2.scene == OP_SCENE_MAJIN ? 4.5f : 8.0f;
        int   beam = CScript__2.beem_no;

        if (CScript__2.beem_end) {
            static sceVu0FVECTOR from = {0.0f, 0.0f, 0.0f, 0.0f};
            static sceVu0FVECTOR to = {0.0f, 0.0f, 0.0f, 0.0f};
            sceVu0FMATRIX        matrix;

            int     part = MBeemCnt;
            CFrame *frame = Chara__3[1].frame->SearchFrame(const_cast<char *>(kFrameNames[part]));

            MBeemCnt = MBeemCnt + 1;

            if (frame) {
                frame->GetLWMatrix(matrix);
                from[0] = matrix[3][0];
                from[1] = matrix[3][1];
                from[2] = matrix[3][2];
            }

            CFrame *end = Cam__2[SceneNp__2].frame->SearchFrame("end");

            if (end) {
                end->GetLWMatrix(matrix);
                to[0] = matrix[3][0];
                to[1] = matrix[3][1];
                to[2] = matrix[3][2];
            }

            FireBeam(CBeem[beam], from, to, speed, part == 2 || part == 3 ? 48.0f : 32.0f);
        } else {
            FireBeam(CBeem[beam], CScript__2.beem_from[beam], CScript__2.beem_to[beam], speed, 0.0f);
        }

        CScript__2.beem_req = false;
    }

    TexManager.ReloadTexture(Vif1Packet, 18);

    for (CMajinBeem &beam : CBeem) {
        if (!Pause) {
            beam.Step();
        }

        beam.Draw(&OP_MainCamera);
    }

    if (CScript__2.obj[1].disp && CScript__2.scene == OP_SCENE_MAJIN) {
        int     part = MBeemCnt == 0 ? 0 : MBeemCnt - 1;
        CFrame *frame = Chara__3[1].frame->SearchFrame(const_cast<char *>(kFrameNames[part]));

        if (frame) {
            sceVu0FMATRIX matrix;
            sceVu0FVECTOR head;

            frame->GetLWMatrix(matrix);
            head[0] = matrix[3][0];
            head[1] = matrix[3][1];
            head[2] = matrix[3][2];
            OP_CharaFrame__2 = Cam__2[SceneNp__2].frame;

            sceVu0FVECTOR eye;

            sceVu0CopyVector(eye, OP_CharaFrame__2->position);
            CBeem[CScript__2.beem_no].Draw2(&OP_MainCamera, head, eye);
        }
    }
}

// Enters one smoke particle rising from `position`.
void EnterSmoke(float *position) {
    CEffectParam param;

    param.Initialize();
    sceVu0CopyVector(param.position, position);
    param.position_oscillation_flags = 1;
    param.wobble_scale = 0.5f * rand() / 2147483648.0f;
    param.wobble_rate = std::numbers::pi_v<float> / (20.0f + 10 * rand() / 2147483648.0f);
    param.opacity_mode = 2;
    param.render_flags = 1;
    param.opacity = 0.15f;
    param.rise_speed = 1.5f + 0.2f * rand() / 2147483648.0f;
    param.width_growth = 0.04f;
    param.height_growth = 0.04f;
    param.lifetime = 120;
    param.texture = TexManager.GetTexture("gray smoke", -1);

    // One of the four 64-pixel quarters of the smoke texture.
    int quarter = rand() % 4;

    param.texel = CRect<int>(64 * (quarter % 2), 64 * (quarter / 2), 64, 64);
    param.unk_08 = 20.0f;
    param.unk_0C = 20.0f;
    param.texture_frame_period = 60;
    Smoke.EnterEffect(&param);
}

/**
 * The two columns of smoke the burning village gives off, entered into the effect pool one particle
 * apiece every eighth tick. Everything about a particle but where it starts is random.
 */
void SmokeProcess() {
    static int tick = 0;

    tick = tick + 1;

    if (tick > 7) {
        tick = 0;
    }

    if (tick == 0) {
        sceVu0FVECTOR position = {400.0f, 10.0f, -450.0f, 1.0f};

        EnterSmoke(position);

        position[0] = -550.0f;
        position[1] = 0.0f;
        position[2] = 300.0f;
        EnterSmoke(position);
    }

    Smoke.Step(1);
    Smoke.Draw();
}

/**
 * The river, which is drawn as a grid that refracts what is already in the frame buffer: the frame
 * is copied into the surface the grid samples, the grid is placed under the camera, and the depth
 * write is turned off so the water does not occlude what is drawn after it.
 */
void WaterProcess() {
    setTexScroll();
    setTexAnim();

    sceGsTex0 frame_tex;

    MGGetFBuffTex(&frame_tex);

    CRect<int> rect(0, 0, 640, SCREEN_HALF_HEIGHT);
    sceGsTex0  surface = *reinterpret_cast<sceGsTex0 *>(&TexManager.GetTexture("water_buff", -1)->tex0);

    MGMoveImage(&frame_tex, rect, &surface, 0, 0, 0);

    sceVu0FVECTOR ref;

    OP_MainCamera.GetRef(ref);
    ref[1] = -13.0f;
    Water.frame.SetPosition(ref);

    sceGsZbuf zbuf = mgZBuffer;

    zbuf.bits.zmsk = 1;
    MGSetGsZBUF(&zbuf);
    Water.Shake(static_cast<int>(32.0f * rand() / 2147483648.0f), static_cast<int>(32.0f * rand() / 2147483648.0f), -0.5f);
    Water.Hamon();
    Water.DrawVu1(&mgRenderInfo, GetVif1Packet(), nullptr);
    MGSetGsZBUF(&mgZBuffer);
}

/**
 * Which of the twenty-three actors gets a shadow drawn into the shadow buffer. The dancers do not,
 * because they carry the flat models InchikiShadow places instead; the demon does not, because it
 * is in the air.
 */
int KageCheck(int actor) {
    switch (actor) {
        case 1:
        case 3:
        case 4:
        case 5:
        case 14:
        case 15:
        case 16:
        case 17:
        case 19:
        case 20:
            return 0;

        case 18:
            return CScript__2.scene < OP_SCENE_NORUNE_BURNING ? 0 : 1;

        default:
            return 1;
    }
}

/**
 * The light one object is drawn under, aimed at it from two points beside the camera rather than
 * from one direction for the whole frame, which is what gives the village its lamplight. The light
 * colour falls with distance and jitters by a few levels a tick, and the ambient colour falls with
 * distance too — except in the burning village, where it is raised instead.
 */
void LightSet(float x, float y, float z, int actor) {
    sceVu0FVECTOR dir;

    dir[0] = -x;

    if (actor == 0) {
        dir[1] = 50.1f;
    } else {
        dir[1] = 250.1f;
    }

    dir[2] = 299.99f - z;
    sceVu0Normalize(dir, dir);
    light[0][0] = dir[0];
    light[1][0] = dir[1];
    light[2][0] = dir[2];
    dir[0] = x;
    dir[1] = 70.1f;
    dir[2] = z - 299.99f;
    sceVu0Normalize(dir, dir);
    light[0][1] = dir[0];
    light[1][1] = dir[1];
    light[2][1] = dir[2];

    sceVu0FMATRIX color;

    sceVu0CopyMatrix(color, lightcolor);

    sceVu0FVECTOR from;
    sceVu0FVECTOR to;

    from[0] = x;
    from[1] = y;
    from[2] = z;
    from[3] = 0.0f;
    to[0] = 0.0f;
    to[1] = 0.0f;
    to[2] = 299.99f;
    to[3] = 0.0f;

    int falloff = std::min(static_cast<int>(DistVector(from, to)) / 20, 60);

    color[0][0] = color[0][0] - static_cast<float>(falloff);
    color[0][0] = color[0][0] + static_cast<float>(rand() % 20 - 10);
    MGSetPLight(light, color);

    sceVu0FVECTOR ambient;

    sceVu0CopyVector(ambient, ambientlight);

    if (CScript__2.scene != OP_SCENE_NORUNE_BURNING) {
        falloff = std::min(static_cast<int>(DistVector(from, to)) / 10, 20);

        ambient[0] += 20.0f;
        ambient[0] = ambient[0] - static_cast<float>(falloff);
    } else {
        ambient[0] += 25.0f;
        ambient[1] += 10.0f;
    }

    MGSetAmbient(ambient);
}

int   setTexScrollCnt;
float setTexScrollCntf;
int   setTexAnimCnt;
float setTexAnimCntf;

/**
 * The waterfall, which is one 128-pixel texture scrolled sideways over another at half a pixel a
 * tick and wrapped by copying the two halves separately.
 */
void setTexScroll() {
    if (setTexScrollCntf >= 126.0f) {
        setTexScrollCntf = 0.0f;
    } else {
        setTexScrollCntf += 0.5f;
    }

    setTexScrollCnt = static_cast<int>(setTexScrollCntf);

    CTexture *plate = TexManager.GetTexture("e01b20", -1);
    CTexture *strip = TexManager.GetTexture("e01b21", -1);

    if (plate == nullptr || strip == nullptr) {
        return;
    }

    int dbp = plate->tex0 & 0x3fff;
    int sbp = strip->tex0 & 0x3fff;
    int dbw = (plate->tex0 >> 14) & 0x3f;
    int sbw = (strip->tex0 >> 14) & 0x3f;

    if (setTexScrollCnt != 128) {
        MoveImageTest(Vif1Packet, sbp, sbw, SCE_GS_PSMT8, CRect<int>(setTexScrollCnt, 0, 128 - setTexScrollCnt, 128), dbp, dbw, SCE_GS_PSMT8, 0, 0, 0);
    }

    if (setTexScrollCnt != 0) {
        MoveImageTest(Vif1Packet, sbp, sbw, SCE_GS_PSMT8, CRect<int>(0, 0, setTexScrollCnt, 128), dbp, dbw, SCE_GS_PSMT8, 128 - setTexScrollCnt, 0, 0);
    }
}

void FlushTextureCache() {
    sceVif1PkCnt(Vif1Packet, 0);
    sceVif1PkOpenDirectCode(Vif1Packet, 0);
    sceVif1PkOpenGifTag(Vif1Packet, *reinterpret_cast<u_long128 *>(&GiftagAD));
    sceVif1PkAddGsAD(Vif1Packet, SCE_GS_TEXFLUSH, 0);
    sceVif1PkCloseGifTag(Vif1Packet);
    sceVif1PkCloseDirectCode(Vif1Packet);
}

/**
 * The river's surface, which is eight 64-pixel frames stacked in a strip of their own and copied
 * one at a time over the plate the grid samples, at a fifth of a frame per tick. The cache is
 * flushed on both sides because the plate is a texture the previous tick drew from.
 */
void setTexAnim() {
    FlushTextureCache();

    if (setTexAnimCntf >= 7.0f) {
        setTexAnimCntf = 0.0f;
    } else {
        setTexAnimCntf += 0.2f;
    }

    setTexAnimCnt = static_cast<int>(setTexAnimCntf);

    CTexture *strip = TexManager.GetTexture("e01b23", -1);
    CTexture *plate = TexManager.GetTexture("e01b22", -1);

    if (strip == nullptr || plate == nullptr) {
        return;
    }

    MGMoveImage(reinterpret_cast<sceGsTex0 *>(&strip->tex0), CRect<int>(0, setTexAnimCnt * 64, 64, 64), reinterpret_cast<sceGsTex0 *>(&plate->tex0), 0, 0, 0);
    FlushTextureCache();
}

} // namespace
