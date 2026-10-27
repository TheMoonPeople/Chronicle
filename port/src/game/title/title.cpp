#include "common.h"

#include <libgraph.h>
#include <libpkt.h>
#include <libvu0.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <numbers>

#include "camera.hpp"
#include "camerafollow.hpp"
#include "character.hpp"
#include "dataalloc.hpp"
#include "dataread.hpp"
#include "frame.hpp"
#include "framevu1.hpp"
#include "gamepad.hpp"
#include "mathutil.hpp"
#include "mds.hpp"
#include "savedata.hpp"
#include "snd.hpp"
#include "sound.hpp"
#include "title/cursol.hpp"
#include "title/logo.hpp"
#include "title/scfader.hpp"
#include "title/sprite.hpp"

/**
 * Rectangle every 2D draw takes; declared here because rect.h's constructor takes its fields in another order.
 */
template <class T>
class CRect {
public:
    T x; /**< Left edge. */
    T y; /**< Top edge. */
    T w; /**< Width. */
    T h; /**< Height. */

    CRect() = default;

    CRect(T left, T top, T width, T height) : x(left), y(top), w(width), h(height) {}
};

/**
 * One piece of scenery a scene lays out: its model file, position and turn about the vertical axis.
 */
struct MAP_INFO {
    char *name;        /**< Name of the model file in the scene's pack. */
    float position[3]; /**< Where the piece stands in the world. */
    float rotation;    /**< Turn about the vertical axis, in degrees. */
};

/**
 * Rectangle DrawObjectVibe takes by value.
 */
struct RECT {
    int x; /**< Left edge. */
    int y; /**< Top edge. */
    int w; /**< Width. */
    int h; /**< Height. */
};

#include "gamemode.hpp"
#include "main.hpp"
#include "mainselect.hpp"
#include "memcard.hpp"
#include "mglib.hpp"
#include "object.hpp"
#include "texture.hpp"
#include "title/dispfade.hpp"
#include "title/op_a.hpp"
#include "title/op_b.hpp"
#include "title/opening.hpp"
#include "title/rushmovi.hpp"
#include "title/script.hpp"
#include "title/title.hpp"
#include "title/titleloop.hpp"
#include "vutext.hpp"
#include "water.hpp"
#include "wind.hpp"

class OBJ_ANIME_SEQ {
public:
    char          name[16];  /**< Name of the frame the animation drives. */
    int           anim_type; /**< What is animated: rotation, position, scale or colour. */
    int           play_mode; /**< How the value runs between its two ends. */
    char          unk_18[8];
    sceVu0FVECTOR start_value; /**< Value the animation starts from. */
    sceVu0FVECTOR end_value;   /**< Value the animation runs to. */
    float         step_x;      /**< Amount added to the first component each step. */
    float         step_y;      /**< Amount added to the second component each step. */
    float         step_z;      /**< Amount added to the third component each step. */
    char          unk_4C[60];

    void Initialize();
};

// The world classes below are other units' types; only the members this file touches are named,
// padded to the sizes the objects have.

/**
 * Frame parented to an object, which lets the world transform drive a model.
 */
class CObjectFrame : public CObject {
public:
    CFrameVu1 *frame[4];         /**< Frame of each level of detail; zero where the object has none. */
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
    CFrameVu1 *collision_frame; /**< Frame that collision reads; zero where the object has none. */
    CFrameVu1 *unk_D4;
    CFrameVu1 *shade_frame;  /**< Frame that the shade draws from; zero where the object takes none. */
    CFrameVu1 *camera_frame; /**< Collision frame the camera reads; zero where the object has none. */
    float      unk_E0;
    int        category_no; /**< Category the map filed the object under. */
    int        handle;      /**< Handle the map gave the object. */
    char       unk_EC[4];

    CMapObject();

    void Draw() override;

    void Initialize();
    void DrawShadow(int fast);
};

/**
 * Dust the running feet kick up.
 */
class CRunEffect {
public:
    char unk_00[208];

    CRunEffect();

    void Lighting(int enabled);
    void Set(float *origin);
    void Step();
    void Draw();
};

class CTexture;

/**
 * The movie's one fire, which is a light rather than a model.
 */
class CFireOmni {
public:
    char          unk_18[32];
    sceVu0FVECTOR position;    /**< World position the fire draws at. */
    s32           texture_set; /**< Indicates that the textures were supplied rather than looked up. */
    CTexture     *core;        /**< Bright inner texture of the flame. */
    CTexture     *glow;        /**< Soft outer texture of the flame. */
    s32           unk_3C;

    CFireOmni();

    void FireStep();
    void FireCreate();

    void SetPosition(float x, float y, float z) {
        position[0] = 10.0f * x;
        position[1] = 10.0f * y;
        position[2] = 10.0f * z;
        position[3] = 1.0f;
    }

    void DrawFire(int unused0, int unused1, CCamera *camera, float *eye, float scale, int layers, float camera_offset);
};

/**
 * Run of frames the world draws as one.
 */
class CMap {
public:
    char unk_00[2800];

    void        Initialize();
    CMapObject *SetObject(CFrameVu1 *frame, int category_no, int handle);
    CMapObject *SetObject(int index, CFrameVu1 *frame, int category_no, int handle);
    CMapObject *GetObject(int index);
    void        Draw();
};

void wait_now_loading_vsync();
void ObjAnimePlay(OBJ_ANIME_SEQ *sequence);
void set2DSprite(sceVif1Packet *packet, CTexture *texture, const CRect<int> &dst, const CRect<int> &src, u_char alpha);
void set2DSprite(sceVif1Packet *packet, CTexture *texture, const CRect<int> &dst, const CRect<int> &src, int width, int height, float angle);
void DepthOfField(float *dist, int level, int alpha, int blur);

CCamera       Camera(4.0f);
CCameraFollow FCamera(60.0f, 20.0f, 0.0f, 4.0f);
CCharacter    Cloud__2;
CCharacter    Logo;
CCharacter    Spark[9];

CFrame        *ObjectFrame3;
class CProcess CProcess;
CScFader       CFade;

class CSprite CSprite;
class CLogo   CLogo;
class CCursol CCursol;

namespace {

float TitleCameraWork[4];

char *StartImageName() {
    switch (LanguageCode) {
        case LANG_JAPANESE:
        case LANG_ENGLISH_US:
        case LANG_ENGLISH_UK:
            return "start.img";
        case LANG_FRENCH:
            return "start_f.img";
        case LANG_GERMAN:
            return "start_g.img";
        case LANG_ITALIAN:
            return "start_i.img";
        case LANG_SPANISH:
            return "start_s.img";
        default:
            return nullptr;
    }
}

// Leaves the entry untouched for a language with no start image.
void LoadStartImage(LOADTEXTURE_INFO2 &texture) {
    if (char *name = StartImageName()) {
        texture.name = reinterpret_cast<char *>(GetPackFile(read_buffer, name, nullptr));
    }
}

void InitProcA() {
    wait_now_loading_vsync();

    LOADTEXTURE_INFO2 textures[] = {
        {.name = "#blender#640#" HALF_BUFFER_HEIGHT_STR "#4",     .block_no = 0,  .mipmap = 0},
        {.name = "#frame_image#640#" HALF_BUFFER_HEIGHT_STR "#4", .block_no = 22, .mipmap = 0},
        {.name = nullptr,                                         .block_no = 20, .mipmap = 0},
        {.name = nullptr,                                         .block_no = 0,  .mipmap = 0},
        {.name = nullptr,                                         .block_no = 1,  .mipmap = 0},
        {.name = nullptr,                                         .block_no = 2,  .mipmap = 0},
        {.name = nullptr,                                         .block_no = 3,  .mipmap = 0},
        {.name = nullptr,                                         .block_no = 4,  .mipmap = 0},
        {.name = nullptr,                                         .block_no = 5,  .mipmap = 0},
        {.name = nullptr,                                         .block_no = 10, .mipmap = 0},
        {.name = nullptr,                                         .block_no = 10, .mipmap = 0},
        {.name = "",                                              .block_no = 0,  .mipmap = 0}
    };

    LoadStartImage(textures[2]);

    textures[3].name = reinterpret_cast<char *>(GetPackFile(read_buffer, "effect.img", nullptr));
    textures[4].name = reinterpret_cast<char *>(GetPackFile(read_buffer, "c01d01.img", nullptr));
    textures[5].name = reinterpret_cast<char *>(GetPackFile(read_buffer, "c12a01.img", nullptr));
    textures[6].name = reinterpret_cast<char *>(GetPackFile(read_buffer, "c08a01.img", nullptr));
    textures[7].name = reinterpret_cast<char *>(GetPackFile(read_buffer, "c09a01.img", nullptr));
    textures[8].name = reinterpret_cast<char *>(GetPackFile(read_buffer, "e04a01.img", nullptr));
    textures[9].name = reinterpret_cast<char *>(GetPackFile(read_buffer, "s1401.img", nullptr));
    textures[10].name = reinterpret_cast<char *>(GetPackFile(read_buffer, "e01s01.img", nullptr));
    TexManager.Initialize(16352);
    TexManager.LoadTextureBlock(-1, textures);

    CharaTex[0] = 1;
    CharaTex[1] = 2;
    CharaTex[2] = 3;
    CharaTex[3] = 4;
    CharaTex[4] = 5;
    CharaTex[5] = 5;
    CharaTex[6] = 5;
    CharaTex[7] = 5;
    CharaTex[8] = 5;

    wait_now_loading_vsync();

    char *chara[5] = {"02c01d.cfg", "02c12a.cfg", "02c08a.cfg", "02c09a.cfg", "02e04a.cfg"};

    CharaDataBuffer.Reset();

    for (int i = 0; i < 4; i++) {
        Chara__3[i].LoadPackData(read_buffer, chara[i], &CharaDataBuffer, nullptr);

        CFrameAttr attr;

        attr.clip_enable = false;
        Chara__3[i].frame->SetAttr(attr, 1, 4);
        Chara__3[i].motion_type.state.time = 1.0f;
        Chara__3[i].motion_type.state.blend_step = 0.05f;
        Chara__3[i].motion_type.state.motion_no = 0;
        Chara__3[i].motion_type.state.playing_no = 0;
    }

    Chara__3[2].motion_type.state.time = 10.0f;
    Chara__3[3].motion_type.state.time = 10.0f;
    Chara__3[0].wind = static_cast<s32>(reinterpret_cast<intptr_t>(&Wind__4));
    Chara__3[1].FootSoundEnable(0);

    for (int j = 4; j < 9; j++) {
        Chara__3[j].LoadPackData(read_buffer, chara[4], &CharaDataBuffer, nullptr);

        CFrameAttr attr;

        attr.clip_enable = false;
        Chara__3[j].frame->SetAttr(attr, 1, 4);
        Chara__3[j].motion_type.state.blend_step = 0.05f;
        Chara__3[j].motion_type.state.motion_no = 0;
        Chara__3[j].motion_type.state.playing_no = 0;
        Chara__3[j].SetScale(5.0f, 5.0f, 5.0f);
    }

    Chara__3[4].motion_type.state.time = 1.0f;
    Chara__3[5].motion_type.state.time = 4.0f;
    Chara__3[6].motion_type.state.time = 8.0f;
    Chara__3[7].motion_type.state.time = 12.0f;
    Chara__3[8].motion_type.state.time = 16.0f;
    OP_CharaFrame = Chara__3[0].frame;

    wait_now_loading_vsync();

    OP_FireList = 0;
    OP_AnimeSeqRot = 0;
    MapDataBuffer.Reset();
    OP_GroundMap.Initialize();
    OP_BuildingMap.Initialize();
    OP_BuildingMap2.Initialize();

    CFrameAttr map_attr;

    CFrameVu1 *map = LoadMDSFile(GetPackFile(read_buffer, "s1402.mds", nullptr), &MapDataBuffer, 2, nullptr, nullptr);

    map_attr.fog_enable = true;
    map->SetAttr(map_attr, 1, 64);
    SetFrameAttr(map, 1);

    CMapObject *object = OP_GroundMap.SetObject(map, 0, 0);

    object->SetPosition(CVector3_f_(0.0f, 0.0f, 0.0f));
    object->SetRotation(CVector3_f_(0.0f, 0.0f, 0.0f));

    sceVu0FVECTOR anime_start;
    sceVu0FVECTOR anime_step;

    anime_start[0] = 0.0f;
    anime_start[1] = 0.0f;
    anime_start[2] = 0.0f;
    anime_step[0] = 0.0f;
    anime_step[1] = 0.015f;
    anime_step[2] = 0.0f;
    SetObjAnime("tenkyu", map, anime_start, anime_step);

    anime_start[0] = 0.0f;
    anime_start[1] = 0.0f;
    anime_start[2] = 0.0f;
    anime_step[0] = 0.015f;
    anime_step[1] = 0.0f;
    anime_step[2] = 0.0f;
    SetObjAnime("tenkyu2", map, anime_start, anime_step);

    anime_start[0] = 0.0f;
    anime_start[1] = 0.0f;
    anime_start[2] = 0.0f;
    anime_step[0] = 0.05f;
    anime_step[1] = 0.0f;
    anime_step[2] = 0.0f;
    SetObjAnime("tenkyu3", map, anime_start, anime_step);

    anime_start[0] = 0.0f;
    anime_start[1] = 0.0f;
    anime_start[2] = 0.0f;
    anime_step[0] = 1.0f;
    anime_step[1] = 0.015f;
    anime_step[2] = 0.0f;
    SetObjAnime("inazuma", map, anime_start, anime_step);

    map = LoadMDSFile(GetPackFile(read_buffer, "s1401.mds", nullptr), &MapDataBuffer, 2, nullptr, nullptr);
    map->SetAttr(map_attr, 1, 64);
    SetFrameAttr(map, 1);
    object = OP_BuildingMap.SetObject(map, 0, 0);
    object->SetPosition(CVector3_f_(0.0f, 0.0f, 0.0f));
    object->SetRotation(CVector3_f_(0.0f, 0.0f, 0.0f));

    wait_now_loading_vsync();

    PathDataBuffer.Reset();
    SceneNp = -1;

    char *campath[4] = {"0201cp.cfg", "0202cp.cfg", "0203cp.cfg", "0204cp.cfg"};

    for (int k = 0; k < 4; k++) {
        Cam[k].LoadPackData(read_buffer, campath[k], &PathDataBuffer, nullptr);
        Cam[k].motion_type.state.time = 1.0f;
        Cam[k].motion_type.state.blend_step = 1.0f;
        Cam[k].motion_type.state.motion_no = 0;
        Cam[k].motion_type.state.playing_no = 0;
        Cam[k].motion_type.state.camera = &MainCamera__3;
    }

    wait_now_loading_vsync();
    OPAnalyz("sim:rmdat/rmdat1.cfg");
    OPMdsLoad();
}

void InitProcB() {
    LOADTEXTURE_INFO2 textures[] = {
        {.name = "#blender#640#" HALF_BUFFER_HEIGHT_STR "#4",     .block_no = 0,  .mipmap = 0},
        {.name = "#frame_image#640#" HALF_BUFFER_HEIGHT_STR "#4", .block_no = 22, .mipmap = 0},
        {.name = "#shadow_buff#640#" HALF_BUFFER_HEIGHT_STR "#4", .block_no = 23, .mipmap = 0},
        {.name = "#water_buff#640#" HALF_BUFFER_HEIGHT_STR "#4",  .block_no = 21, .mipmap = 0},
        {.name = nullptr,                                         .block_no = 20, .mipmap = 0},
        {.name = nullptr,                                         .block_no = 0,  .mipmap = 0},
        {.name = nullptr,                                         .block_no = 10, .mipmap = 0},
        {.name = nullptr,                                         .block_no = 1,  .mipmap = 0},
        {.name = nullptr,                                         .block_no = 2,  .mipmap = 0},
        {.name = nullptr,                                         .block_no = 3,  .mipmap = 0},
        {.name = nullptr,                                         .block_no = 4,  .mipmap = 0},
        {.name = nullptr,                                         .block_no = 8,  .mipmap = 0},
        {.name = nullptr,                                         .block_no = 9,  .mipmap = 0},
        {.name = "",                                              .block_no = 0,  .mipmap = 0}
    };

    LoadStartImage(textures[4]);

    textures[5].name = reinterpret_cast<char *>(GetPackFile(read_buffer, "effect.img", nullptr));
    textures[6].name = reinterpret_cast<char *>(GetPackFile(read_buffer, "d01m01.img", nullptr));
    textures[7].name = reinterpret_cast<char *>(GetPackFile(read_buffer, "c01d01.img", nullptr));
    textures[8].name = reinterpret_cast<char *>(GetPackFile(read_buffer, "e01a01.img", nullptr));
    textures[9].name = reinterpret_cast<char *>(GetPackFile(read_buffer, "saget.img", nullptr));
    textures[10].name = reinterpret_cast<char *>(GetPackFile(read_buffer, "d01etc.img", nullptr));
    textures[11].name = reinterpret_cast<char *>(GetPackFile(read_buffer, "rm04ex.img", nullptr));
    textures[12].name = reinterpret_cast<char *>(GetPackFile(read_buffer, "c01w03.img", nullptr));
    TexManager.Initialize(16352);
    TexManager.LoadTextureBlock(-1, textures);

    CharaTex[0] = 1;
    CharaTex[1] = 2;
    CharaTex[2] = 3;
    CharaTex[3] = 4;
    CharaTex[7] = 8;
    CharaTex[8] = 9;

    char *chara[4] = {"rm05c01d.cfg", "rm04e01a.cfg", "rm05saget.cfg", "d01o03_m.cfg"};

    CharaDataBuffer.Reset();

    for (int i = 0; i < 30; i++) {
        TexAnimeDataMovie[i].Initialize();
    }

    Chara__3[3].InitializeTexAnime(TexAnimeDataMovie, 30);
    Chara__3[3].TexAnimeOn(0);
    Chara__3[0].LoadPackData(read_buffer, "c01d.cfg", &CharaDataBuffer, nullptr);
    Chara__3[0].LoadPackData(read_buffer, "rm04c01d.cfg", &CharaDataBuffer, nullptr);

    for (int j = 0; j < 4; j++) {
        Chara__3[j].LoadPackData(read_buffer, chara[j], &CharaDataBuffer, nullptr);

        CFrameAttr attr;

        attr.clip_enable = false;
        Chara__3[j].frame->SetAttr(attr, 1, 4);
        Chara__3[j].motion_type.state.time = 1.0f;
        Chara__3[j].motion_type.state.blend_step = 0.05f;
        Chara__3[j].motion_type.state.motion_no = 0;
        Chara__3[j].motion_type.state.playing_no = 0;
    }

    Chara__3[1].motion_type.state.time = 10.0f;
    Chara__3[2].motion_type.state.time = 30.0f;
    Chara__3[0].wind = static_cast<s32>(reinterpret_cast<intptr_t>(&Wind__4));
    Chara__3[7].LoadPackData(read_buffer, "rm04ex.cfg", &CharaDataBuffer, nullptr);
    Chara__3[8].LoadPackData(read_buffer, "c01w03.cfg", &CharaDataBuffer, nullptr);

    OP_FireList = 0;
    OP_AnimeSeqRot = 0;
    MapDataBuffer.Reset();
    OP_GroundMap.Initialize();
    OP_BuildingMap.Initialize();

    CFrameAttr map_attr;

    CFrameVu1 *map = LoadMDSFile(GetPackFile(read_buffer, "s4201.mds", nullptr), &MapDataBuffer, 2, nullptr, nullptr);

    map_attr.fog_enable = true;
    map->SetAttr(map_attr, 1, 64);
    SetFrameAttr(map, 1);

    CMapObject *object = OP_GroundMap.SetObject(map, 0, 0);

    object->SetPosition(CVector3_f_(0.0f, 0.0f, 0.0f));
    object->SetRotation(CVector3_f_(0.0f, 0.0f, 0.0f));

    map = LoadMDSFile(GetPackFile(read_buffer, "s4202.mds", nullptr), &MapDataBuffer, 2, nullptr, nullptr);
    map->SetAttr(map_attr, 1, 64);
    SetFrameAttr(map, 1);
    object = OP_BuildingMap.SetObject(map, 0, 0);
    object->SetPosition(CVector3_f_(0.0f, 0.0f, 0.0f));
    object->SetRotation(CVector3_f_(0.0f, 0.0f, 0.0f));

    PathDataBuffer.Reset();
    SceneNp = -1;

    char *campath[4] = {"rm03cam.cfg", "rm04cam.cfg", "rm05cam.cfg"};

    for (int k = 0; k < 3; k++) {
        Cam[k].LoadPackData(read_buffer, campath[k], &PathDataBuffer, nullptr);
        Cam[k].motion_type.state.time = 1.0f;
        Cam[k].motion_type.state.blend_step = 1.0f;
        Cam[k].motion_type.state.motion_no = 0;
        Cam[k].motion_type.state.playing_no = 0;
        Cam[k].motion_type.state.camera = &MainCamera__3;
    }

    OP_FireList = 0;
    OP_AnimeSeqRot = 0;
    OPAnalyz("sim:rmdat/rmdat2.cfg");
    OPMdsLoad();

    sceVu0FVECTOR corner0 = {260.0f, 0.0f, -400.0f, 1.0f};
    sceVu0FVECTOR corner1 = {380.0f, 0.0f, -400.0f, 1.0f};
    sceVu0FVECTOR corner2 = {260.0f, 0.0f, -250.0f, 1.0f};
    sceVu0FVECTOR corner3 = {380.0f, 0.0f, -250.0f, 1.0f};

    Water__2.SetVertex(corner0, corner1, corner2, corner3);
    Water__2.frame.SetPosition(0.0f, -4.0f, 0.0f);
    Water__2.SetSize(24, 24, &WaterBuffer);
    Water__2.SetParam(0.1f, 0.015f, 0.0f, 2.0f);
    Water__2.SetColor(100, 110, 120, 128);
}

void AtraLight() {
    if (Chara__3[2].motion_type.state.time >= 40.0f) {
        CFrame *frame = Chara__3[2].frame->SearchFrame("light01");

        if (frame) {
            sceVu0FVECTOR position;
            sceVu0FVECTOR eye;
            sceVu0FVECTOR dir;
            sceVu0FVECTOR world;
            sceVu0FMATRIX matrix;

            sceVu0CopyMatrix(matrix, frame->local);
            position[0] = matrix[3][0];
            position[1] = matrix[3][1];
            position[2] = matrix[3][2];
            frame->GetWorldPosition(world, position);

            OP_CharaFrame = Chara__3[0].frame;
            sceVu0CopyVector(eye, OP_CharaFrame->position);

            dir[0] = world[0] - eye[0];
            dir[1] = world[1] - eye[1] - 12.0f;
            dir[2] = world[2] - eye[2];

            float len = sqrt(dir[0] * dir[0] + dir[1] * dir[1] + dir[2] * dir[2]);

            len = std::max(len, 10.0f);

            float power = 5.0f * (100.0f / (len * len));

            sceVu0Normalize(dir, dir);
            dir[0] *= power;
            dir[1] *= power;
            dir[2] *= power;
            dir[3] = 0.0f;

            light[0][3] = dir[0];
            light[1][3] = dir[1];
            light[2][3] = dir[2];
            light[3][3] = 0.0f;

            lightcolor[3][0] = 0.8359375f * atraGetStatusRate;
            lightcolor[3][1] = 0.9765625f * atraGetStatusRate;
            lightcolor[3][2] = 0.66015625f * atraGetStatusRate;
            lightcolor[3][3] = 0.5f * atraGetStatusRate;
            atraGetStatusRate += 2.0f;

            MGSetPLight(light, lightcolor);
        }
    }
}

void InitProcC() {
    LOADTEXTURE_INFO2 textures[] = {
        {.name = "#blender#640#" HALF_BUFFER_HEIGHT_STR "#4",     .block_no = 0,  .mipmap = 0},
        {.name = "#frame_image#640#" HALF_BUFFER_HEIGHT_STR "#4", .block_no = 22, .mipmap = 0},
        {.name = "#shadow_buff#640#" HALF_BUFFER_HEIGHT_STR "#4", .block_no = 23, .mipmap = 0},
        {.name = "#water_buff#640#" HALF_BUFFER_HEIGHT_STR "#4",  .block_no = 21, .mipmap = 0},
        {.name = nullptr,                                         .block_no = 20, .mipmap = 0},
        {.name = nullptr,                                         .block_no = 0,  .mipmap = 0},
        {.name = nullptr,                                         .block_no = 10, .mipmap = 0},
        {.name = nullptr,                                         .block_no = 10, .mipmap = 0},
        {.name = nullptr,                                         .block_no = 10, .mipmap = 0},
        {.name = nullptr,                                         .block_no = 1,  .mipmap = 0},
        {.name = nullptr,                                         .block_no = 9,  .mipmap = 0},
        {.name = "",                                              .block_no = 0,  .mipmap = 0}
    };

    LoadStartImage(textures[4]);

    textures[5].name = reinterpret_cast<char *>(GetPackFile(read_buffer, "effect.img", nullptr));
    textures[6].name = reinterpret_cast<char *>(GetPackFile(read_buffer, "s04b01.img", nullptr));
    textures[7].name = reinterpret_cast<char *>(GetPackFile(read_buffer, "s04b02.img", nullptr));
    textures[8].name = reinterpret_cast<char *>(GetPackFile(read_buffer, "s04w01.img", nullptr));
    textures[9].name = reinterpret_cast<char *>(GetPackFile(read_buffer, "c01d01.img", nullptr));
    textures[10].name = reinterpret_cast<char *>(GetPackFile(read_buffer, "pat.img", nullptr));
    TexManager.DeleteTextureBlock(0);
    TexManager.CleanUpBuffer();
    TexManager.Initialize(16352);
    TexManager.LoadTextureBlock(-1, textures);

    CharaTex[0] = 1;
    CharaTex[8] = 9;

    CharaDataBuffer.Reset();
    Chara__3[0].LoadPackData(read_buffer, "c01d.cfg", &CharaDataBuffer, nullptr);

    CFrameAttr attr;

    attr.clip_enable = false;
    Chara__3[0].frame->SetAttr(attr, 1, 4);
    Chara__3[0].motion_type.state.time = 10.0f;
    Chara__3[0].motion_type.state.blend_step = 0.05f;
    Chara__3[0].motion_type.state.motion_no = 0;
    Chara__3[0].motion_type.state.playing_no = 0;
    Chara__3[0].FootSoundEnable(0);
    Chara__3[0].wind = static_cast<s32>(reinterpret_cast<intptr_t>(&Wind__4));

    Chara__3[8].LoadPackData(read_buffer, "pat.cfg", &CharaDataBuffer, nullptr);
    attr.clip_enable = false;
    Chara__3[8].frame->SetAttr(attr, 1, 4);
    Chara__3[8].motion_type.state.time = 1.0f;
    Chara__3[8].motion_type.state.blend_step = 0.05f;
    Chara__3[8].motion_type.state.motion_no = 0;
    Chara__3[8].motion_type.state.playing_no = 0;

    MAP_INFO norn[] = {
        {.name = "s04g01_0.mds", .position = {0.0f, 0.0f, 0.0f},         .rotation = 0.0f   },
        {.name = "s04g02_0.mds", .position = {0.0f, 0.0f, 0.0f},         .rotation = 0.0f   },
        {.name = "s04g04_0.mds", .position = {0.0f, 0.0f, 0.0f},         .rotation = 0.0f   },
        {.name = "s04g06_0.mds", .position = {0.0f, 0.0f, 0.0f},         .rotation = 0.0f   },
        {.name = "s04g03_0.mds", .position = {0.0f, 0.0f, 0.0f},         .rotation = 0.0f   },
        {.name = "s04g05_0.mds", .position = {0.0f, 0.0f, 0.0f},         .rotation = 0.0f   },
        {.name = "s04r01_0.mds", .position = {0.0f, 0.0f, 74.0f},        .rotation = 0.0f   },
        {.name = "s04r02_0.mds", .position = {-56.68f, 0.0f, 47.566f},   .rotation = -50.0f },
        {.name = "s04r03_0.mds", .position = {32.439f, 0.0f, -66.51f},   .rotation = 154.0f },
        {.name = "s04r05_0.mds", .position = {82.0f, -10.0f, 109.0f},    .rotation = -90.0f },
        {.name = "s04r06_0.mds", .position = {61.832f, 10.0f, -127.0f},  .rotation = -28.0f },
        {.name = "s04r07_0.mds", .position = {-54.64f, 10.0f, -115.6f},  .rotation = 25.0f  },
        {.name = "s04r08_0.mds", .position = {-90.85f, 10.0f, 75.585f},  .rotation = 125.0f },
        {.name = "s04w01_0.mds", .position = {0.0f, 0.0f, 0.0f},         .rotation = 0.0f   },
        {.name = "s04w02_0.mds", .position = {0.0f, 0.0f, 0.0f},         .rotation = 0.0f   },
        {.name = "s04h01_0.mds", .position = {0.0f, -10.0f, 0.0f},       .rotation = 0.0f   },
        {.name = "s04h02_0.mds", .position = {127.0f, -10.0f, 109.0f},   .rotation = 0.0f   },
        {.name = "s04h03_0.mds", .position = {-140.0f, -10.0f, 110.0f},  .rotation = 125.0f },
        {.name = "s04h03_0.mds", .position = {90.0f, -10.0f, -180.0f},   .rotation = -28.0f },
        {.name = "s04h03_0.mds", .position = {-80.0f, -10.0f, -170.0f},  .rotation = 25.0f  },
        {.name = "s04a01_0.mds", .position = {-63.15f, -15.0f, 128.0f},  .rotation = 50.0f  },
        {.name = "s04a01_0.mds", .position = {-126.0f, -15.0f, -110.0f}, .rotation = -20.0f },
        {.name = "s04a01_0.mds", .position = {-178.0f, -15.0f, 46.0f},   .rotation = 10.0f  },
        {.name = "s04a01_0.mds", .position = {12.0f, -15.0f, -218.0f},   .rotation = -70.0f },
        {.name = "s04a01_0.mds", .position = {204.0f, -15.0f, -152.0f},  .rotation = -140.0f},
        {.name = "s04a01_0.mds", .position = {244.0f, -15.0f, 14.0f},    .rotation = 190.0f }
    };

    OP_FireList = 0;
    OP_AnimeSeqRot = 0;
    MapDataBuffer.Reset();

    CFrameAttr map_attr;

    for (int i = 0; i < 26; i++) {
        CFrameVu1 *map = LoadMDSFile(GetPackFile(read_buffer, norn[i].name, nullptr), &MapDataBuffer, 2, nullptr, nullptr);

        map_attr.fog_enable = true;
        map->SetAttr(map_attr, 1, 64);
        SetFrameAttr(map, 1);

        CMapObject &object = OP_NornMapObj[i];

        object.Initialize();
        object.SetFrame(map, 0);
        OP_NornMapObj[i].handle = 0;
        OP_NornMapObj[i].category_no = 0;
        object.SetPosition(CVector3_f_(norn[i].position[0], norn[i].position[1], norn[i].position[2]));
        object.SetRotation(CVector3_f_(0.0f, static_cast<float>(std::numbers::pi * norn[i].rotation / 180), 0.0f));
    }

    PathDataBuffer.Reset();
    SceneNp = -1;

    char *campath[4] = {"rm06cam.cfg"};

    for (int j = 0; j < 1; j++) {
        Cam[j].LoadPackData(read_buffer, campath[j], &PathDataBuffer, nullptr);
        Cam[j].motion_type.state.time = 1.0f;
        Cam[j].motion_type.state.blend_step = 1.0f;
        Cam[j].motion_type.state.motion_no = 0;
        Cam[j].motion_type.state.playing_no = 0;
        Cam[j].motion_type.state.camera = &MainCamera__3;
    }

    OP_FireList = 0;
    OP_AnimeSeqRot = 0;
    OPAnalyz("sim:rmdat/rmdat3.cfg");
    OPMdsLoad();

    sceVu0FVECTOR corner0 = {-120.0f, 0.0f, -120.0f, 1.0f};
    sceVu0FVECTOR corner1 = {120.0f, 0.0f, -120.0f, 1.0f};
    sceVu0FVECTOR corner2 = {-120.0f, 0.0f, 120.0f, 1.0f};
    sceVu0FVECTOR corner3 = {-120.0f, 0.0f, 120.0f, 1.0f};

    Water__2.SetVertex(corner0, corner1, corner2, corner3);
    Water__2.frame.SetPosition(0.0f, 0.0f, 0.0f);
    Water__2.SetSize(32, 32, &WaterBuffer);
    Water__2.SetParam(0.1f, 0.015f, 0.0f, 2.0f);
    Water__2.SetColor(128, 128, 128, 128);
}

void InitProcD() {
    LOADTEXTURE_INFO2 textures[] = {
        {.name = "#blender#640#" HALF_BUFFER_HEIGHT_STR "#4",     .block_no = 0,  .mipmap = 0},
        {.name = "#frame_image#640#" HALF_BUFFER_HEIGHT_STR "#4", .block_no = 22, .mipmap = 0},
        {.name = "#shadow_buff#640#" HALF_BUFFER_HEIGHT_STR "#4", .block_no = 23, .mipmap = 0},
        {.name = nullptr,                                         .block_no = 20, .mipmap = 0},
        {.name = nullptr,                                         .block_no = 10, .mipmap = 0},
        {.name = nullptr,                                         .block_no = 1,  .mipmap = 0},
        {.name = nullptr,                                         .block_no = 1,  .mipmap = 0},
        {.name = nullptr,                                         .block_no = 2,  .mipmap = 0},
        {.name = nullptr,                                         .block_no = 9,  .mipmap = 0},
        {.name = "",                                              .block_no = 0,  .mipmap = 0}
    };

    LoadStartImage(textures[3]);

    textures[4].name = reinterpret_cast<char *>(GetPackFile(read_buffer, "d02i01.img", nullptr));
    textures[5].name = reinterpret_cast<char *>(GetPackFile(read_buffer, "c01d01.img", nullptr));
    textures[6].name = reinterpret_cast<char *>(GetPackFile(read_buffer, "c01d01an.img", nullptr));
    textures[7].name = reinterpret_cast<char *>(GetPackFile(read_buffer, "e54a01.img", nullptr));
    textures[8].name = reinterpret_cast<char *>(GetPackFile(read_buffer, "c01w11.img", nullptr));
    TexManager.Initialize(16352);
    TexManager.LoadTextureBlock(-1, textures);

    CharaTex[0] = 1;
    CharaTex[1] = 2;
    CharaTex[2] = 2;
    CharaTex[8] = 9;

    char *chara[3] = {"rm07c01d.cfg", "rm07e54b.cfg", "rm07e54a.cfg"};

    CharaDataBuffer.Reset();

    for (int i = 0; i < 30; i++) {
        TexAnimeDataMovie[i].Initialize();
    }

    Chara__3[0].InitializeTexAnime(TexAnimeDataMovie, 30);
    Chara__3[0].LoadPackData(read_buffer, "c01d.cfg", &CharaDataBuffer, nullptr);

    for (int j = 0; j < 3; j++) {
        Chara__3[j].LoadPackData(read_buffer, chara[j], &CharaDataBuffer, nullptr);

        CFrameAttr attr;

        attr.clip_enable = false;
        Chara__3[j].frame->SetAttr(attr, 1, 4);
        Chara__3[j].motion_type.state.time = 1.0f;
        Chara__3[j].motion_type.state.blend_step = 0.05f;
        Chara__3[j].motion_type.state.motion_no = 0;
        Chara__3[j].motion_type.state.playing_no = 0;
    }

    Chara__3[0].motion_type.state.time = 10.0f;
    Chara__3[1].motion_type.state.time = 10.0f;
    Chara__3[2].motion_type.state.time = 10.0f;
    Chara__3[8].LoadPackData(read_buffer, "c01w11.cfg", &CharaDataBuffer, nullptr);
    Chara__3[0].TexAnimeOn(2);
    Chara__3[0].wind = static_cast<s32>(reinterpret_cast<intptr_t>(&Wind__4));

    OP_FireList = 0;
    OP_AnimeSeqRot = 0;
    MapDataBuffer.Reset();
    OP_GroundMap.Initialize();
    OP_BuildingMap.Initialize();
    OP_BuildingMap2.Initialize();

    CFrameAttr map_attr;

    CFrameVu1 *map = LoadMDSFile(GetPackFile(read_buffer, "s44g01_0.mds", nullptr), &MapDataBuffer, 2, nullptr, nullptr);

    map_attr.fog_enable = true;
    map->SetAttr(map_attr, 1, 64);
    SetFrameAttr(map, 1);

    CMapObject *object = OP_GroundMap.SetObject(map, 0, 0);

    object->SetPosition(CVector3_f_(0.0f, 0.0f, 0.0f));
    object->SetRotation(CVector3_f_(0.0f, 0.0f, 0.0f));

    map = LoadMDSFile(GetPackFile(read_buffer, "s44g02_0.mds", nullptr), &MapDataBuffer, 2, nullptr, nullptr);
    map->SetAttr(map_attr, 1, 64);
    SetFrameAttr(map, 1);
    object = OP_BuildingMap.SetObject(map, 0, 0);
    object->SetPosition(CVector3_f_(0.0f, 0.0f, 0.0f));
    object->SetRotation(CVector3_f_(0.0f, 0.0f, 0.0f));

    PathDataBuffer.Reset();
    SceneNp = -1;

    char *campath[1] = {"rm07cam.cfg"};

    for (int k = 0; k < 1; k++) {
        Cam[k].LoadPackData(read_buffer, campath[k], &PathDataBuffer, nullptr);
        Cam[k].motion_type.state.time = 10.0f;
        Cam[k].motion_type.state.blend_step = 1.0f;
        Cam[k].motion_type.state.motion_no = 0;
        Cam[k].motion_type.state.playing_no = 0;
        Cam[k].motion_type.state.camera = &MainCamera__3;
    }

    OP_FireList = 0;
    OP_AnimeSeqRot = 0;
    OPAnalyz("sim:rmdat/rmdat4.cfg");
    OPMdsLoad();
}

void InitProcE() {
    LOADTEXTURE_INFO2 textures[] = {
        {.name = "#blender#640#" HALF_BUFFER_HEIGHT_STR "#4",     .block_no = 0,  .mipmap = 0},
        {.name = "#frame_image#640#" HALF_BUFFER_HEIGHT_STR "#4", .block_no = 22, .mipmap = 0},
        {.name = "#shadow_buff#640#" HALF_BUFFER_HEIGHT_STR "#4", .block_no = 23, .mipmap = 0},
        {.name = nullptr,                                         .block_no = 20, .mipmap = 0},
        {.name = nullptr,                                         .block_no = 10, .mipmap = 0},
        {.name = nullptr,                                         .block_no = 1,  .mipmap = 0},
        {.name = nullptr,                                         .block_no = 1,  .mipmap = 0},
        {.name = nullptr,                                         .block_no = 2,  .mipmap = 0},
        {.name = nullptr,                                         .block_no = 2,  .mipmap = 0},
        {.name = "",                                              .block_no = 0,  .mipmap = 0}
    };

    LoadStartImage(textures[3]);

    textures[4].name = reinterpret_cast<char *>(GetPackFile(read_buffer, "s4501.img", nullptr));
    textures[5].name = reinterpret_cast<char *>(GetPackFile(read_buffer, "c01d01.img", nullptr));
    textures[6].name = reinterpret_cast<char *>(GetPackFile(read_buffer, "c01d01an.img", nullptr));
    textures[7].name = reinterpret_cast<char *>(GetPackFile(read_buffer, "c04b01.img", nullptr));
    textures[8].name = reinterpret_cast<char *>(GetPackFile(read_buffer, "c04b01an.img", nullptr));
    TexManager.Initialize(16352);
    TexManager.LoadTextureBlock(-1, textures);

    CharaTex[0] = 1;
    CharaTex[1] = 2;

    char *chara[2] = {"rm09c01d.cfg", "rm09c04b.cfg"};

    CharaDataBuffer.Reset();

    for (int i = 0; i < 30; i++) {
        TexAnimeDataMovie[i].Initialize();
    }

    Chara__3[0].InitializeTexAnime(TexAnimeDataMovie, 30);
    Chara__3[1].InitializeTexAnime(TexAnimeDataMovie, 30);
    Chara__3[0].LoadPackData(read_buffer, "c01d.cfg", &CharaDataBuffer, nullptr);

    for (int j = 0; j < 2; j++) {
        Chara__3[j].LoadPackData(read_buffer, chara[j], &CharaDataBuffer, nullptr);

        CFrameAttr attr;

        attr.clip_enable = false;
        Chara__3[j].frame->SetAttr(attr, 1, 4);
        Chara__3[j].motion_type.state.time = 1.0f;
        Chara__3[j].motion_type.state.blend_step = 0.05f;
        Chara__3[j].motion_type.state.motion_no = 0;
        Chara__3[j].motion_type.state.playing_no = 0;
    }

    Chara__3[0].TexAnimeOn(2);
    Chara__3[1].TexAnimeOn(1);
    Chara__3[0].wind = static_cast<s32>(reinterpret_cast<intptr_t>(&Wind__4));

    OP_FireList = 0;
    OP_AnimeSeqRot = 0;
    MapDataBuffer.Reset();
    OP_GroundMap.Initialize();
    OP_BuildingMap.Initialize();
    OP_BuildingMap2.Initialize();

    CFrameAttr map_attr;

    CFrameVu1 *map = LoadMDSFile(GetPackFile(read_buffer, "s4501.mds", nullptr), &MapDataBuffer, 2, nullptr, nullptr);

    map_attr.fog_enable = true;
    map->SetAttr(map_attr, 1, 64);
    SetFrameAttr(map, 1);

    CMapObject *object = OP_GroundMap.SetObject(map, 0, 0);

    object->SetPosition(CVector3_f_(0.0f, 0.0f, 0.0f));
    object->SetRotation(CVector3_f_(0.0f, 0.0f, 0.0f));
    object->FrameObjectOnOff("door2", 0);

    PathDataBuffer.Reset();
    SceneNp = -1;

    char *campath[4] = {"rm08cam.cfg", "rm09cam.cfg"};

    for (int k = 0; k < 2; k++) {
        Cam[k].LoadPackData(read_buffer, campath[k], &PathDataBuffer, nullptr);
        Cam[k].motion_type.state.time = 1.0f;
        Cam[k].motion_type.state.blend_step = 1.0f;
        Cam[k].motion_type.state.motion_no = 0;
        Cam[k].motion_type.state.playing_no = 0;
        Cam[k].motion_type.state.camera = &MainCamera__3;
    }

    OP_FireList = 0;
    OP_AnimeSeqRot = 0;
    OPAnalyz("sim:rmdat/rmdat5.cfg");
    OPMdsLoad();
}

void InitProcF() {
    LOADTEXTURE_INFO2 textures[] = {
        {.name = "#blender#640#" HALF_BUFFER_HEIGHT_STR "#4",     .block_no = 0,  .mipmap = 0},
        {.name = "#frame_image#640#" HALF_BUFFER_HEIGHT_STR "#4", .block_no = 22, .mipmap = 0},
        {.name = "#shadow_buff#640#" HALF_BUFFER_HEIGHT_STR "#4", .block_no = 23, .mipmap = 0},
        {.name = nullptr,                                         .block_no = 20, .mipmap = 0},
        {.name = nullptr,                                         .block_no = 10, .mipmap = 0},
        {.name = nullptr,                                         .block_no = 1,  .mipmap = 0},
        {.name = nullptr,                                         .block_no = 1,  .mipmap = 0},
        {.name = nullptr,                                         .block_no = 2,  .mipmap = 0},
        {.name = nullptr,                                         .block_no = 3,  .mipmap = 0},
        {.name = nullptr,                                         .block_no = 9,  .mipmap = 0},
        {.name = "",                                              .block_no = 0,  .mipmap = 0}
    };

    LoadStartImage(textures[3]);

    textures[4].name = reinterpret_cast<char *>(GetPackFile(read_buffer, "d02b01.img", nullptr));
    textures[5].name = reinterpret_cast<char *>(GetPackFile(read_buffer, "c01d01.img", nullptr));
    textures[6].name = reinterpret_cast<char *>(GetPackFile(read_buffer, "c01d01an.img", nullptr));
    textures[7].name = reinterpret_cast<char *>(GetPackFile(read_buffer, "c14a01.img", nullptr));
    textures[8].name = reinterpret_cast<char *>(GetPackFile(read_buffer, "rm11rock.img", nullptr));
    textures[9].name = reinterpret_cast<char *>(GetPackFile(read_buffer, "c01w01.img", nullptr));
    TexManager.Initialize(16352);
    TexManager.LoadTextureBlock(-1, textures);

    CharaTex[0] = 1;
    CharaTex[1] = 2;
    CharaTex[2] = 2;
    CharaTex[3] = 2;
    CharaTex[4] = 2;
    CharaTex[5] = 3;
    CharaTex[8] = 9;

    char *chara[6] = {
        "rm10c01d.cfg", "rm10c14a.cfg", "rm10c14b.cfg",
        "rm10c14c.cfg", "rm10c14d.cfg", "rm11rock.cfg"};

    CharaDataBuffer.Reset();

    for (int i = 0; i < 30; i++) {
        TexAnimeDataMovie[i].Initialize();
    }

    Chara__3[0].InitializeTexAnime(TexAnimeDataMovie, 30);
    Chara__3[0].LoadPackData(read_buffer, "c01d.cfg", &CharaDataBuffer, nullptr);

    for (int j = 0; j < 6; j++) {
        Chara__3[j].LoadPackData(read_buffer, chara[j], &CharaDataBuffer, nullptr);

        CFrameAttr attr;

        attr.clip_enable = false;
        Chara__3[j].frame->SetAttr(attr, 1, 4);
        Chara__3[j].motion_type.state.time = 1.0f;
        Chara__3[j].motion_type.state.blend_step = 0.05f;
        Chara__3[j].motion_type.state.motion_no = 0;
        Chara__3[j].motion_type.state.playing_no = 0;
    }

    Chara__3[0].TexAnimeOn(4);
    Chara__3[0].wind = static_cast<s32>(reinterpret_cast<intptr_t>(&Wind__4));
    Chara__3[8].LoadPackData(read_buffer, "c01w01.cfg", &CharaDataBuffer, nullptr);
    Chara__3[0].motion_type.state.time = 150.0f;
    Chara__3[1].motion_type.state.time = 135.0f;
    Chara__3[2].motion_type.state.time = 135.0f;
    Chara__3[3].motion_type.state.time = 135.0f;
    Chara__3[4].motion_type.state.time = 135.0f;

    OP_FireList = 0;
    OP_AnimeSeqRot = 0;
    MapDataBuffer.Reset();
    OP_GroundMap.Initialize();
    OP_BuildingMap.Initialize();
    OP_BuildingMap2.Initialize();

    CFrameAttr map_attr;

    CFrameVu1 *map = LoadMDSFile(GetPackFile(read_buffer, "s4601.mds", nullptr), &MapDataBuffer, 2, nullptr, nullptr);

    map_attr.fog_enable = true;
    map->SetAttr(map_attr, 1, 64);
    SetFrameAttr(map, 1);

    CMapObject *object = OP_GroundMap.SetObject(map, 0, 0);

    object->SetPosition(CVector3_f_(0.0f, 0.0f, 0.0f));
    object->SetRotation(CVector3_f_(0.0f, 0.0f, 0.0f));

    map = LoadMDSFile(GetPackFile(read_buffer, "s4602.mds", nullptr), &MapDataBuffer, 2, nullptr, nullptr);
    map->SetAttr(map_attr, 1, 64);
    SetFrameAttr(map, 1);
    object = OP_BuildingMap.SetObject(map, 0, 0);
    object->SetPosition(CVector3_f_(0.0f, 0.0f, 0.0f));
    object->SetRotation(CVector3_f_(0.0f, 0.0f, 0.0f));
    object->FrameObjectOnOff("rock1", 0);

    PathDataBuffer.Reset();
    SceneNp = -1;

    char *campath[4] = {"rm10cam.cfg", "rm11cam.cfg"};

    for (int k = 0; k < 2; k++) {
        Cam[k].LoadPackData(read_buffer, campath[k], &PathDataBuffer, nullptr);
        Cam[k].motion_type.state.time = 1.0f;
        Cam[k].motion_type.state.blend_step = 1.0f;
        Cam[k].motion_type.state.motion_no = 0;
        Cam[k].motion_type.state.playing_no = 0;
        Cam[k].motion_type.state.camera = &MainCamera__3;
    }

    OP_FireList = 0;
    OP_AnimeSeqRot = 0;
    OPAnalyz("sim:rmdat/rmdat6.cfg");
    OPMdsLoad();
}

void InitProcG() {
    LOADTEXTURE_INFO2 textures[] = {
        {.name = "#blender#640#" HALF_BUFFER_HEIGHT_STR "#4",     .block_no = 0,  .mipmap = 0},
        {.name = "#frame_image#640#" HALF_BUFFER_HEIGHT_STR "#4", .block_no = 22, .mipmap = 0},
        {.name = "#shadow_buff#640#" HALF_BUFFER_HEIGHT_STR "#4", .block_no = 23, .mipmap = 0},
        {.name = nullptr,                                         .block_no = 20, .mipmap = 0},
        {.name = nullptr,                                         .block_no = 10, .mipmap = 0},
        {.name = nullptr,                                         .block_no = 10, .mipmap = 0},
        {.name = nullptr,                                         .block_no = 10, .mipmap = 0},
        {.name = nullptr,                                         .block_no = 1,  .mipmap = 0},
        {.name = nullptr,                                         .block_no = 2,  .mipmap = 0},
        {.name = nullptr,                                         .block_no = 2,  .mipmap = 0},
        {.name = nullptr,                                         .block_no = 2,  .mipmap = 0},
        {.name = "",                                              .block_no = 0,  .mipmap = 0}
    };

    LoadStartImage(textures[3]);

    textures[4].name = reinterpret_cast<char *>(GetPackFile(read_buffer, "s4701.img", nullptr));
    textures[5].name = reinterpret_cast<char *>(GetPackFile(read_buffer, "e02s01.img", nullptr));
    textures[6].name = reinterpret_cast<char *>(GetPackFile(read_buffer, "e02s06.img", nullptr));
    textures[7].name = reinterpret_cast<char *>(GetPackFile(read_buffer, "c01d01.img", nullptr));
    textures[8].name = reinterpret_cast<char *>(GetPackFile(read_buffer, "c06a01.img", nullptr));
    textures[9].name = reinterpret_cast<char *>(GetPackFile(read_buffer, "c06a01an.img", nullptr));
    textures[10].name = reinterpret_cast<char *>(GetPackFile(read_buffer, "c06w01.img", nullptr));
    TexManager.Initialize(16352);
    TexManager.LoadTextureBlock(-1, textures);

    CharaTex[0] = 1;
    CharaTex[1] = 2;
    CharaTex[2] = 2;

    char *chara[3] = {"rm14ebc01d.cfg", "rm14ebc06a.cfg", "rm13c06a.cfg"};

    CharaDataBuffer.Reset();
    Chara__3[0].LoadPackData(read_buffer, "c01d.cfg", &CharaDataBuffer, nullptr);

    for (int i = 0; i < 3; i++) {
        Chara__3[i].LoadPackData(read_buffer, chara[i], &CharaDataBuffer, nullptr);

        CFrameAttr attr;

        attr.clip_enable = false;
        Chara__3[i].frame->SetAttr(attr, 1, 4);
        Chara__3[i].motion_type.state.time = 1.0f;
        Chara__3[i].motion_type.state.blend_step = 0.05f;
        Chara__3[i].motion_type.state.motion_no = 0;
        Chara__3[i].motion_type.state.playing_no = 0;
    }

    Chara__3[0].motion_type.state.time = 10.0f;
    Chara__3[1].motion_type.state.time = 10.0f;
    Chara__3[2].motion_type.state.time = 82.0f;
    Chara__3[0].wind = static_cast<s32>(reinterpret_cast<intptr_t>(&Wind__4));
    Chara__3[1].wind = static_cast<s32>(reinterpret_cast<intptr_t>(&Wind__4));
    Chara__3[2].wind = static_cast<s32>(reinterpret_cast<intptr_t>(&Wind__4));

    OP_FireList = 0;
    OP_AnimeSeqRot = 0;
    MapDataBuffer.Reset();
    OP_GroundMap.Initialize();
    OP_BuildingMap.Initialize();
    OP_BuildingMap2.Initialize();

    CFrameAttr map_attr;

    CFrameVu1 *map = LoadMDSFile(GetPackFile(read_buffer, "s4701.mds", nullptr), &MapDataBuffer, 2, nullptr, nullptr);

    map_attr.fog_enable = true;
    map->SetAttr(map_attr, 1, 64);
    SetFrameAttr(map, 1);

    CMapObject *object = OP_GroundMap.SetObject(map, 0, 0);

    object->SetPosition(CVector3_f_(0.0f, 0.0f, 0.0f));
    object->SetRotation(CVector3_f_(0.0f, 0.0f, 0.0f));

    map = LoadMDSFile(GetPackFile(read_buffer, "e02s01_0.mds", nullptr), &MapDataBuffer, 2, nullptr, nullptr);
    map->SetAttr(map_attr, 1, 64);
    SetFrameAttr(map, 1);
    object = OP_BuildingMap.SetObject(map, 0, 0);
    object->SetPosition(CVector3_f_(0.0f, 0.0f, 0.0f));
    object->SetRotation(CVector3_f_(0.0f, 0.0f, 0.0f));

    PathDataBuffer.Reset();
    SceneNp = -1;

    char *campath[3] = {"rm12cam.cfg", "rm13cam.cfg", "rm14cam.cfg"};

    for (int j = 0; j < 3; j++) {
        Cam[j].LoadPackData(read_buffer, campath[j], &PathDataBuffer, nullptr);
        Cam[j].motion_type.state.time = 1.0f;
        Cam[j].motion_type.state.blend_step = 1.0f;
        Cam[j].motion_type.state.motion_no = 0;
        Cam[j].motion_type.state.playing_no = 0;
        Cam[j].motion_type.state.camera = &MainCamera__3;
    }

    OP_FireList = 0;
    OP_AnimeSeqRot = 0;
    OPAnalyz("sim:rmdat/rmdat7.cfg");
    OPMdsLoad();
}

void InitProcH() {
    LOADTEXTURE_INFO2 textures[] = {
        {.name = "#blender#640#" HALF_BUFFER_HEIGHT_STR "#4",     .block_no = 0,  .mipmap = 0},
        {.name = "#frame_image#640#" HALF_BUFFER_HEIGHT_STR "#4", .block_no = 22, .mipmap = 0},
        {.name = "#shadow_buff#640#" HALF_BUFFER_HEIGHT_STR "#4", .block_no = 23, .mipmap = 0},
        {.name = nullptr,                                         .block_no = 20, .mipmap = 0},
        {.name = nullptr,                                         .block_no = 0,  .mipmap = 0},
        {.name = nullptr,                                         .block_no = 10, .mipmap = 0},
        {.name = nullptr,                                         .block_no = 1,  .mipmap = 0},
        {.name = nullptr,                                         .block_no = 2,  .mipmap = 0},
        {.name = nullptr,                                         .block_no = 3,  .mipmap = 0},
        {.name = nullptr,                                         .block_no = 4,  .mipmap = 0},
        {.name = nullptr,                                         .block_no = 9,  .mipmap = 0},
        {.name = "",                                              .block_no = 0,  .mipmap = 0}
    };

    LoadStartImage(textures[3]);

    textures[4].name = reinterpret_cast<char *>(GetPackFile(read_buffer, "fire.img", nullptr));
    textures[5].name = reinterpret_cast<char *>(GetPackFile(read_buffer, "d01b01.img", nullptr));
    textures[6].name = reinterpret_cast<char *>(GetPackFile(read_buffer, "c12a01.img", nullptr));
    textures[7].name = reinterpret_cast<char *>(GetPackFile(read_buffer, "c01d01.img", nullptr));
    textures[8].name = reinterpret_cast<char *>(GetPackFile(read_buffer, "f_boll_2.img", nullptr));
    textures[9].name = reinterpret_cast<char *>(GetPackFile(read_buffer, "rm16yuka.img", nullptr));
    textures[10].name = reinterpret_cast<char *>(GetPackFile(read_buffer, "c01w01.img", nullptr));
    TexManager.Initialize(16352);
    TexManager.LoadTextureBlock(-1, textures);

    CharaTex[0] = 1;
    CharaTex[1] = 2;
    CharaTex[2] = 3;
    CharaTex[3] = 4;
    CharaTex[8] = 9;

    Chara__3[0].LoadPackData(read_buffer, "rm15c12a.cfg", &CharaDataBuffer, nullptr);

    CFrameAttr attr;

    attr.clip_enable = false;
    Chara__3[0].frame->SetAttr(attr, 1, 4);
    Chara__3[0].motion_type.state.time = 10.0f;
    Chara__3[0].motion_type.state.blend_step = 0.05f;
    Chara__3[0].motion_type.state.motion_no = 0;
    Chara__3[0].motion_type.state.playing_no = 0;
    Chara__3[0].FootSoundEnable(0);

    Chara__3[1].LoadPackData(read_buffer, "c01d.cfg", &CharaDataBuffer, nullptr);
    attr.clip_enable = false;
    Chara__3[1].frame->SetAttr(attr, 1, 4);
    Chara__3[1].motion_type.state.time = 70.0f;
    Chara__3[1].motion_type.state.blend_step = 0.05f;
    Chara__3[1].motion_type.state.motion_no = 0;
    Chara__3[1].motion_type.state.playing_no = 0;

    Chara__3[2].LoadPackData(read_buffer, "f_boll_2.cfg", &CharaDataBuffer, nullptr);
    attr.clip_enable = false;
    Chara__3[2].frame->SetAttr(attr, 1, 4);
    Chara__3[2].motion_type.state.time = 20.0f;
    Chara__3[2].motion_type.state.blend_step = 0.05f;
    Chara__3[2].motion_type.state.motion_no = 0;
    Chara__3[2].motion_type.state.playing_no = 0;

    Chara__3[3].LoadPackData(read_buffer, "rm16yuka.cfg", &CharaDataBuffer, nullptr);
    attr.clip_enable = false;
    Chara__3[3].frame->SetAttr(attr, 1, 4);
    Chara__3[3].motion_type.state.time = 2.0f;
    Chara__3[3].motion_type.state.blend_step = 0.05f;
    Chara__3[3].motion_type.state.motion_no = 0;
    Chara__3[3].motion_type.state.playing_no = 0;

    Chara__3[8].LoadPackData(read_buffer, "c01w01.cfg", &CharaDataBuffer, nullptr);
    Chara__3[1].wind = static_cast<s32>(reinterpret_cast<intptr_t>(&Wind__4));

    OP_FireList = 0;
    OP_AnimeSeqRot = 0;
    MapDataBuffer.Reset();
    OP_GroundMap.Initialize();
    OP_BuildingMap.Initialize();
    OP_BuildingMap2.Initialize();

    CFrameAttr map_attr;

    CFrameVu1 *map = LoadMDSFile(GetPackFile(read_buffer, "s4801.mds", nullptr), &MapDataBuffer, 2, nullptr, nullptr);

    map_attr.fog_enable = true;
    map->SetAttr(map_attr, 1, 64);
    SetFrameAttr(map, 1);

    CMapObject *object = OP_GroundMap.SetObject(map, 0, 0);

    object->SetPosition(CVector3_f_(0.0f, 0.0f, 0.0f));
    object->SetRotation(CVector3_f_(0.0f, 0.0f, 0.0f));

    map = LoadMDSFile(GetPackFile(read_buffer, "s4802.mds", nullptr), &MapDataBuffer, 2, nullptr, nullptr);
    map_attr.fog_enable = true;
    map->SetAttr(map_attr, 1, 64);
    SetFrameAttr(map, 1);
    object = OP_BuildingMap.SetObject(0, map, 0, 0);
    object->SetPosition(CVector3_f_(0.0f, 0.0f, 0.0f));
    object->SetRotation(CVector3_f_(0.0f, 0.0f, 0.0f));

    map = LoadMDSFile(GetPackFile(read_buffer, "s4803.mds", nullptr), &MapDataBuffer, 2, nullptr, nullptr);
    map_attr.fog_enable = true;
    map->SetAttr(map_attr, 1, 64);
    SetFrameAttr(map, 1);
    object = OP_BuildingMap.SetObject(1, map, 0, 0);
    object->SetPosition(CVector3_f_(0.0f, 0.0f, 0.0f));
    object->SetRotation(CVector3_f_(0.0f, 0.0f, 0.0f));

    map = LoadMDSFile(GetPackFile(read_buffer, "s4804.mds", nullptr), &MapDataBuffer, 2, nullptr, nullptr);
    map_attr.fog_enable = true;
    map->SetAttr(map_attr, 1, 64);
    SetFrameAttr(map, 1);
    object = OP_BuildingMap.SetObject(2, map, 0, 0);
    object->SetPosition(CVector3_f_(0.0f, 0.0f, 0.0f));
    object->SetRotation(CVector3_f_(0.0f, 0.0f, 0.0f));

    map = LoadMDSFile(GetPackFile(read_buffer, "s4805.mds", nullptr), &MapDataBuffer, 2, nullptr, nullptr);
    map_attr.fog_enable = true;
    map->SetAttr(map_attr, 1, 64);
    SetFrameAttr(map, 1);
    object = OP_BuildingMap.SetObject(3, map, 0, 0);
    object->SetPosition(CVector3_f_(0.0f, 0.0f, 0.0f));
    object->SetRotation(CVector3_f_(0.0f, 0.0f, 0.0f));

    map = LoadMDSFile(GetPackFile(read_buffer, "s4806.mds", nullptr), &MapDataBuffer, 2, nullptr, nullptr);
    map_attr.fog_enable = true;
    map->SetAttr(map_attr, 1, 64);
    SetFrameAttr(map, 1);
    object = OP_BuildingMap.SetObject(4, map, 0, 0);
    object->SetPosition(CVector3_f_(0.0f, 0.0f, 0.0f));
    object->SetRotation(CVector3_f_(0.0f, 0.0f, 0.0f));

    PathDataBuffer.Reset();
    SceneNp = -1;

    char *campath[2] = {"rm15cam.cfg", "rm16cam.cfg"};

    for (int i = 0; i < 2; i++) {
        Cam[i].LoadPackData(read_buffer, campath[i], &PathDataBuffer, nullptr);
        Cam[i].motion_type.state.time = 1.0f;
        Cam[i].motion_type.state.blend_step = 1.0f;
        Cam[i].motion_type.state.motion_no = 0;
        Cam[i].motion_type.state.playing_no = 0;
        Cam[i].motion_type.state.camera = &MainCamera__3;
    }

    OP_FireList = 0;
    OP_AnimeSeqRot = 0;
    OPAnalyz("sim:rmdat/rmdat8.cfg");
    OPMdsLoad();
}

void InitProcI() {
    LOADTEXTURE_INFO2 textures[] = {
        {.name = "#blender#640#" HALF_BUFFER_HEIGHT_STR "#4",     .block_no = 0,  .mipmap = 0},
        {.name = "#frame_image#640#" HALF_BUFFER_HEIGHT_STR "#4", .block_no = 22, .mipmap = 0},
        {.name = "#shadow_buff#640#" HALF_BUFFER_HEIGHT_STR "#4", .block_no = 23, .mipmap = 0},
        {.name = nullptr,                                         .block_no = 20, .mipmap = 0},
        {.name = nullptr,                                         .block_no = 3,  .mipmap = 0},
        {.name = nullptr,                                         .block_no = 10, .mipmap = 0},
        {.name = nullptr,                                         .block_no = 10, .mipmap = 0},
        {.name = nullptr,                                         .block_no = 1,  .mipmap = 0},
        {.name = nullptr,                                         .block_no = 2,  .mipmap = 0},
        {.name = "",                                              .block_no = 0,  .mipmap = 0}
    };

    LoadStartImage(textures[3]);

    textures[4].name = reinterpret_cast<char *>(GetPackFile(read_buffer, "e305ex2.img", nullptr));
    textures[5].name = reinterpret_cast<char *>(GetPackFile(read_buffer, "s1202.img", nullptr));
    textures[6].name = reinterpret_cast<char *>(GetPackFile(read_buffer, "s2401.img", nullptr));
    textures[7].name = reinterpret_cast<char *>(GetPackFile(read_buffer, "c01d01.img", nullptr));
    textures[8].name = reinterpret_cast<char *>(GetPackFile(read_buffer, "m18ashiba.img", nullptr));
    TexManager.Initialize(16352);
    TexManager.LoadTextureBlock(-1, textures);

    CharaTex[0] = 1;
    CharaTex[1] = 2;
    CharaTex[2] = 3;

    char *chara[3] = {"rm18c01d.cfg", "rm18ashiba.cfg", "info2.cfg"};

    CharaDataBuffer.Reset();
    Chara__3[0].LoadPackData(read_buffer, "c01d.cfg", &CharaDataBuffer, nullptr);

    for (int i = 0; i < 3; i++) {
        Chara__3[i].LoadPackData(read_buffer, chara[i], &CharaDataBuffer, nullptr);

        CFrameAttr attr;

        attr.clip_enable = false;
        Chara__3[i].frame->SetAttr(attr, 1, 4);
        Chara__3[i].motion_type.state.time = 1.0f;
        Chara__3[i].motion_type.state.blend_step = 0.05f;
        Chara__3[i].motion_type.state.motion_no = 0;
        Chara__3[i].motion_type.state.playing_no = 0;
    }

    Chara__3[1].motion_type.state.time = 23.0f;
    Chara__3[0].wind = static_cast<s32>(reinterpret_cast<intptr_t>(&Wind__4));
    Chara__3[2].SetScale(20.0f, 20.0f, 20.0f);

    OP_FireList = 0;
    OP_AnimeSeqRot = 0;
    MapDataBuffer.Reset();
    OP_GroundMap.Initialize();
    OP_BuildingMap.Initialize();
    OP_BuildingMap2.Initialize();

    CFrameAttr map_attr;

    CFrameVu1 *map = LoadMDSFile(GetPackFile(read_buffer, "s24g01_0.mds", nullptr), &MapDataBuffer, 2, nullptr, nullptr);

    map_attr.fog_enable = true;
    map->SetAttr(map_attr, 1, 64);
    SetFrameAttr(map, 1);

    CMapObject *object = OP_GroundMap.SetObject(map, 0, 0);

    object->SetPosition(CVector3_f_(0.0f, 0.0f, 0.0f));
    object->SetRotation(CVector3_f_(0.0f, 0.0f, 0.0f));

    map = LoadMDSFile(GetPackFile(read_buffer, "s24g02_0.mds", nullptr), &MapDataBuffer, 2, nullptr, nullptr);
    map_attr.fog_enable = true;
    map->SetAttr(map_attr, 1, 64);
    SetFrameAttr(map, 1);
    object = OP_BuildingMap.SetObject(0, map, 0, 0);
    object->SetPosition(CVector3_f_(0.0f, 0.0f, 0.0f));
    object->SetRotation(CVector3_f_(0.0f, 0.0f, 0.0f));

    map = LoadMDSFile(GetPackFile(read_buffer, "s24g03_0.mds", nullptr), &MapDataBuffer, 2, nullptr, nullptr);
    map_attr.fog_enable = true;
    map->SetAttr(map_attr, 1, 64);
    SetFrameAttr(map, 1);
    object = OP_BuildingMap.SetObject(1, map, 0, 0);
    object->SetPosition(CVector3_f_(0.0f, 0.0f, 0.0f));
    object->SetRotation(CVector3_f_(0.0f, 0.0f, 0.0f));

    map = LoadMDSFile(GetPackFile(read_buffer, "ship.mds", nullptr), &MapDataBuffer, 2, nullptr, nullptr);
    map_attr.fog_enable = true;
    map->SetAttr(map_attr, 1, 64);
    SetFrameAttr(map, 1);
    object = OP_BuildingMap.SetObject(2, map, 0, 0);
    object->SetPosition(CVector3_f_(0.0f, 0.0f, 0.0f));
    object->SetRotation(CVector3_f_(0.0f, 0.0f, 0.0f));

    map = LoadMDSFile(GetPackFile(read_buffer, "s24g04_0.mds", nullptr), &MapDataBuffer, 2, nullptr, nullptr);
    map_attr.fog_enable = true;
    map->SetAttr(map_attr, 1, 64);
    SetFrameAttr(map, 1);
    object = OP_BuildingMap2.SetObject(0, map, 0, 0);
    object->SetPosition(CVector3_f_(0.0f, 0.0f, 0.0f));
    object->SetRotation(CVector3_f_(0.0f, 0.0f, 0.0f));

    PathDataBuffer.Reset();
    SceneNp = -1;

    char *campath[4] = {"rm17cam.cfg", "rm18cam.cfg"};

    for (int j = 0; j < 2; j++) {
        Cam[j].LoadPackData(read_buffer, campath[j], &PathDataBuffer, nullptr);
        Cam[j].motion_type.state.time = 1.0f;
        Cam[j].motion_type.state.blend_step = 1.0f;
        Cam[j].motion_type.state.motion_no = 0;
        Cam[j].motion_type.state.playing_no = 0;
        Cam[j].motion_type.state.camera = &MainCamera__3;
    }

    OP_FireList = 0;
    OP_AnimeSeqRot = 0;
    OPAnalyz("sim:rmdat/rmdat9.cfg");
    OPMdsLoad();
}

void InitProcTitle() {
    LOADTEXTURE_INFO2 textures[] = {
        {.name = "#blender#640#" HALF_BUFFER_HEIGHT_STR "#4",     .block_no = 0,  .mipmap = 0},
        {.name = "#frame_image#640#" HALF_BUFFER_HEIGHT_STR "#4", .block_no = 22, .mipmap = 0},
        {.name = "#shadow_buff#640#" HALF_BUFFER_HEIGHT_STR "#4", .block_no = 23, .mipmap = 0},
        {.name = nullptr,                                         .block_no = 1,  .mipmap = 0},
        {.name = "",                                              .block_no = 0,  .mipmap = 0}
    };

    textures[3].name = reinterpret_cast<char *>(GetPackFile(read_buffer, "title.img", nullptr));
    TexManager.Initialize(16352);
    TexManager.LoadTextureBlock(-1, textures);
}

void TitleSetCamera([[maybe_unused]] float x, [[maybe_unused]] float y, [[maybe_unused]] float z,
                    [[maybe_unused]] float dist, [[maybe_unused]] float height) {
    TitleCameraWork[0] = 0.0f;
}

} // namespace
