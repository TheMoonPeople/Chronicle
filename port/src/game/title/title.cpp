
#include "common.h"

#include <libgraph.h>
#include <libpkt.h>
#include <libvu0.h>

#include <cmath>
#include <cstdlib>
#include <cstring>

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

/* The rectangle every 2D draw takes, declared here rather than reached through rect.h because the
   two constructors that header states are not this file's: every rectangle here is built by one
   that assigns x, y, w and h in that order, and rect.h's assigns them in the other. */
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
    char *name;        /**< Name of the model file in the scene's pack. */
    float position[3]; /**< Where the piece stands in the world. */
    float rotation;    /**< Turn about the vertical axis, in degrees. */
};

/* The rectangle DrawObjectVibe takes by value. It is four ints and not a CRect: the two are the
   same fields and the name the call encodes is this one. */
struct RECT {
    int x; /**< Left edge. */
    int y; /**< Top edge. */
    int w; /**< Width. */
    int h; /**< Height. */
};

#include "gamemode.hpp"
#include "main.hpp"
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
#include "mainselect.hpp"
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

/* The classes this movie places in the world, declared here rather than reached through headers of
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

/* One piece of scenery. The movie builds a table of them, hands each its model, and drives them
   through the object dispatch like anything else in the world. */
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

    virtual void Draw();

    void Initialize();
    void DrawShadow(int fast);
};

/* The dust the running feet kick up, declared here for the same reason. */
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

/* The movie's one fire, which is a light rather than a model. */
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

/* A run of frames the world draws as one. */
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

static void InitProcA();
static void InitProcB();
static void AtraLight();
static void InitProcC();
static void InitProcD();
static void InitProcE();
static void InitProcF();
static void InitProcG();
static void InitProcH();
static void InitProcI();
static void InitProcTitle();

CCamera      Camera(4.0f);
CCameraFollow FCamera(60.0f, 20.0f, 0.0f, 4.0f);
CCharacter    Cloud__2;
CCharacter    Logo;
CCharacter    Spark[9];

/**
 * Initializes title cinematic scene A.
 *
 * @mangled InitProcA__Fv
 * @address 0x1DCB560
 * @size 0x82C
 * @unknownret
 */
static void InitProcA() {
    wait_now_loading_vsync();

    LOADTEXTURE_INFO2 textures[] = {
        {"#blender#640#" HALF_BUFFER_HEIGHT_STR "#4",     0,  0},
        {"#frame_image#640#" HALF_BUFFER_HEIGHT_STR "#4", 22, 0},
        {0,                                               20, 0},
        {0,                                               0,  0},
        {0,                                               1,  0},
        {0,                                               2,  0},
        {0,                                               3,  0},
        {0,                                               4,  0},
        {0,                                               5,  0},
        {0,                                               10, 0},
        {0,                                               10, 0},
        {"",                                              0,  0}
    };

    switch (LanguageCode) {
        case LANG_JAPANESE:
            textures[2].name = (char *) GetPackFile(read_buffer, "start.img", 0);
            break;
        case LANG_ENGLISH_US:
            textures[2].name = (char *) GetPackFile(read_buffer, "start.img", 0);
            break;
        case LANG_ENGLISH_UK:
            textures[2].name = (char *) GetPackFile(read_buffer, "start.img", 0);
            break;
        case LANG_FRENCH:
            textures[2].name = (char *) GetPackFile(read_buffer, "start_f.img", 0);
            break;
        case LANG_GERMAN:
            textures[2].name = (char *) GetPackFile(read_buffer, "start_g.img", 0);
            break;
        case LANG_ITALIAN:
            textures[2].name = (char *) GetPackFile(read_buffer, "start_i.img", 0);
            break;
        case LANG_SPANISH:
            textures[2].name = (char *) GetPackFile(read_buffer, "start_s.img", 0);
            break;
    }

    textures[3].name = (char *) GetPackFile(read_buffer, "effect.img", 0);
    textures[4].name = (char *) GetPackFile(read_buffer, "c01d01.img", 0);
    textures[5].name = (char *) GetPackFile(read_buffer, "c12a01.img", 0);
    textures[6].name = (char *) GetPackFile(read_buffer, "c08a01.img", 0);
    textures[7].name = (char *) GetPackFile(read_buffer, "c09a01.img", 0);
    textures[8].name = (char *) GetPackFile(read_buffer, "e04a01.img", 0);
    textures[9].name = (char *) GetPackFile(read_buffer, "s1401.img", 0);
    textures[10].name = (char *) GetPackFile(read_buffer, "e01s01.img", 0);
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
        Chara__3[i].LoadPackData(read_buffer, chara[i], &CharaDataBuffer, 0);

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
    Chara__3[0].wind = (int) &Wind__4;
    Chara__3[1].FootSoundEnable(0);

    for (int j = 4; j < 9; j++) {
        Chara__3[j].LoadPackData(read_buffer, chara[4], &CharaDataBuffer, 0);

        CFrameAttr attr;

        attr.clip_enable = false;
        Chara__3[j].frame->SetAttr(attr, 1, 4);
        Chara__3[j].motion_type.state.blend_step = 0.05f;
        Chara__3[j].motion_type.state.motion_no = 0;
        Chara__3[j].motion_type.state.playing_no = 0;
        Chara__3[j].SetScale(5.0f, (float) (j - j + 5), 5.0f);
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

    CFrameVu1 *map = LoadMDSFile(GetPackFile(read_buffer, "s1402.mds", 0), &MapDataBuffer, 2, 0, 0);

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

    map = LoadMDSFile(GetPackFile(read_buffer, "s1401.mds", 0), &MapDataBuffer, 2, 0, 0);
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
        Cam[k].LoadPackData(read_buffer, campath[k], &PathDataBuffer, 0);
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

void DrawProcA();

CFrame        *ObjectFrame3;
class CProcess CProcess;
CScFader       CFade;

class CSprite CSprite;
class CLogo   CLogo;
class CCursol CCursol;

static float TitleCameraWork[4];

/**
 *
 *
 * @mangled InitProcB__Fv
 * @address 0x1DCC570
 * @size 0x7B0
 * @unknownret
 */
static void InitProcB() {
    LOADTEXTURE_INFO2 textures[] = {
        {"#blender#640#" HALF_BUFFER_HEIGHT_STR "#4",     0,  0},
        {"#frame_image#640#" HALF_BUFFER_HEIGHT_STR "#4", 22, 0},
        {"#shadow_buff#640#" HALF_BUFFER_HEIGHT_STR "#4", 23, 0},
        {"#water_buff#640#" HALF_BUFFER_HEIGHT_STR "#4",  21, 0},
        {0,                                               20, 0},
        {0,                                               0,  0},
        {0,                                               10, 0},
        {0,                                               1,  0},
        {0,                                               2,  0},
        {0,                                               3,  0},
        {0,                                               4,  0},
        {0,                                               8,  0},
        {0,                                               9,  0},
        {"",                                              0,  0}
    };

    switch (LanguageCode) {
        case LANG_JAPANESE:
            textures[4].name = (char *) GetPackFile(read_buffer, "start.img", 0);
            break;
        case LANG_ENGLISH_US:
            textures[4].name = (char *) GetPackFile(read_buffer, "start.img", 0);
            break;
        case LANG_ENGLISH_UK:
            textures[4].name = (char *) GetPackFile(read_buffer, "start.img", 0);
            break;
        case LANG_FRENCH:
            textures[4].name = (char *) GetPackFile(read_buffer, "start_f.img", 0);
            break;
        case LANG_GERMAN:
            textures[4].name = (char *) GetPackFile(read_buffer, "start_g.img", 0);
            break;
        case LANG_ITALIAN:
            textures[4].name = (char *) GetPackFile(read_buffer, "start_i.img", 0);
            break;
        case LANG_SPANISH:
            textures[4].name = (char *) GetPackFile(read_buffer, "start_s.img", 0);
            break;
    }

    textures[5].name = (char *) GetPackFile(read_buffer, "effect.img", 0);
    textures[6].name = (char *) GetPackFile(read_buffer, "d01m01.img", 0);
    textures[7].name = (char *) GetPackFile(read_buffer, "c01d01.img", 0);
    textures[8].name = (char *) GetPackFile(read_buffer, "e01a01.img", 0);
    textures[9].name = (char *) GetPackFile(read_buffer, "saget.img", 0);
    textures[10].name = (char *) GetPackFile(read_buffer, "d01etc.img", 0);
    textures[11].name = (char *) GetPackFile(read_buffer, "rm04ex.img", 0);
    textures[12].name = (char *) GetPackFile(read_buffer, "c01w03.img", 0);
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
    Chara__3[0].LoadPackData(read_buffer, "c01d.cfg", &CharaDataBuffer, 0);
    Chara__3[0].LoadPackData(read_buffer, "rm04c01d.cfg", &CharaDataBuffer, 0);

    for (int j = 0; j < 4; j++) {
        Chara__3[j].LoadPackData(read_buffer, chara[j], &CharaDataBuffer, 0);

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
    Chara__3[0].wind = (int) &Wind__4;
    Chara__3[7].LoadPackData(read_buffer, "rm04ex.cfg", &CharaDataBuffer, 0);
    Chara__3[8].LoadPackData(read_buffer, "c01w03.cfg", &CharaDataBuffer, 0);

    OP_FireList = 0;
    OP_AnimeSeqRot = 0;
    MapDataBuffer.Reset();
    OP_GroundMap.Initialize();
    OP_BuildingMap.Initialize();

    CFrameAttr map_attr;

    CFrameVu1 *map = LoadMDSFile(GetPackFile(read_buffer, "s4201.mds", 0), &MapDataBuffer, 2, 0, 0);

    map_attr.fog_enable = true;
    map->SetAttr(map_attr, 1, 64);
    SetFrameAttr(map, 1);

    CMapObject *object = OP_GroundMap.SetObject(map, 0, 0);

    object->SetPosition(CVector3_f_(0.0f, 0.0f, 0.0f));
    object->SetRotation(CVector3_f_(0.0f, 0.0f, 0.0f));

    map = LoadMDSFile(GetPackFile(read_buffer, "s4202.mds", 0), &MapDataBuffer, 2, 0, 0);
    map->SetAttr(map_attr, 1, 64);
    SetFrameAttr(map, 1);
    object = OP_BuildingMap.SetObject(map, 0, 0);
    object->SetPosition(CVector3_f_(0.0f, 0.0f, 0.0f));
    object->SetRotation(CVector3_f_(0.0f, 0.0f, 0.0f));

    PathDataBuffer.Reset();
    SceneNp = -1;

    char *campath[4] = {"rm03cam.cfg", "rm04cam.cfg", "rm05cam.cfg"};

    for (int k = 0; k < 3; k++) {
        Cam[k].LoadPackData(read_buffer, campath[k], &PathDataBuffer, 0);
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
    float zero = 0.0f;
    Water__2.frame.SetPosition(zero, -4.0f, zero);
    Water__2.SetSize(24, 24, &WaterBuffer);
    Water__2.SetParam(0.1f, 0.015f, 0.0f, 2.0f);
    Water__2.SetColor(100, 110, 120, 128);
}

void DrawProcB();

/**
 *
 *
 * @mangled AtraLight__Fv
 * @address 0x1DCD1D0
 * @size 0x28C
 * @unknownret
 */
static void AtraLight() {
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

            if (len <= 10.0f) {
                len = 10.0f;
            }

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

/**
 *
 *
 * @mangled InitProcC__Fv
 * @address 0x1DCD460
 * @size 0x67C
 * @unknownret
 */
static void InitProcC() {
    LOADTEXTURE_INFO2 textures[] = {
        {"#blender#640#" HALF_BUFFER_HEIGHT_STR "#4",     0,  0},
        {"#frame_image#640#" HALF_BUFFER_HEIGHT_STR "#4", 22, 0},
        {"#shadow_buff#640#" HALF_BUFFER_HEIGHT_STR "#4", 23, 0},
        {"#water_buff#640#" HALF_BUFFER_HEIGHT_STR "#4",  21, 0},
        {0,                                               20, 0},
        {0,                                               0,  0},
        {0,                                               10, 0},
        {0,                                               10, 0},
        {0,                                               10, 0},
        {0,                                               1,  0},
        {0,                                               9,  0},
        {"",                                              0,  0}
    };

    switch (LanguageCode) {
        case LANG_JAPANESE:
            textures[4].name = (char *) GetPackFile(read_buffer, "start.img", 0);
            break;
        case LANG_ENGLISH_US:
            textures[4].name = (char *) GetPackFile(read_buffer, "start.img", 0);
            break;
        case LANG_ENGLISH_UK:
            textures[4].name = (char *) GetPackFile(read_buffer, "start.img", 0);
            break;
        case LANG_FRENCH:
            textures[4].name = (char *) GetPackFile(read_buffer, "start_f.img", 0);
            break;
        case LANG_GERMAN:
            textures[4].name = (char *) GetPackFile(read_buffer, "start_g.img", 0);
            break;
        case LANG_ITALIAN:
            textures[4].name = (char *) GetPackFile(read_buffer, "start_i.img", 0);
            break;
        case LANG_SPANISH:
            textures[4].name = (char *) GetPackFile(read_buffer, "start_s.img", 0);
            break;
    }

    textures[5].name = (char *) GetPackFile(read_buffer, "effect.img", 0);
    textures[6].name = (char *) GetPackFile(read_buffer, "s04b01.img", 0);
    textures[7].name = (char *) GetPackFile(read_buffer, "s04b02.img", 0);
    textures[8].name = (char *) GetPackFile(read_buffer, "s04w01.img", 0);
    textures[9].name = (char *) GetPackFile(read_buffer, "c01d01.img", 0);
    textures[10].name = (char *) GetPackFile(read_buffer, "pat.img", 0);
    TexManager.DeleteTextureBlock(0);
    TexManager.CleanUpBuffer();
    TexManager.Initialize(16352);
    TexManager.LoadTextureBlock(-1, textures);

    CharaTex[0] = 1;
    CharaTex[8] = 9;

    CharaDataBuffer.Reset();
    Chara__3[0].LoadPackData(read_buffer, "c01d.cfg", &CharaDataBuffer, 0);

    CFrameAttr attr;

    attr.clip_enable = false;
    Chara__3[0].frame->SetAttr(attr, 1, 4);
    Chara__3[0].motion_type.state.time = 10.0f;
    Chara__3[0].motion_type.state.blend_step = 0.05f;
    Chara__3[0].motion_type.state.motion_no = 0;
    Chara__3[0].motion_type.state.playing_no = 0;
    Chara__3[0].FootSoundEnable(0);
    Chara__3[0].wind = (int) &Wind__4;

    Chara__3[8].LoadPackData(read_buffer, "pat.cfg", &CharaDataBuffer, 0);
    attr.clip_enable = false;
    Chara__3[8].frame->SetAttr(attr, 1, 4);
    Chara__3[8].motion_type.state.time = 1.0f;
    Chara__3[8].motion_type.state.blend_step = 0.05f;
    Chara__3[8].motion_type.state.motion_no = 0;
    Chara__3[8].motion_type.state.playing_no = 0;

    MAP_INFO norn[] = {
        {"s04g01_0.mds", 0.0f,    0.0f,   0.0f,    0.0f   },
        {"s04g02_0.mds", 0.0f,    0.0f,   0.0f,    0.0f   },
        {"s04g04_0.mds", 0.0f,    0.0f,   0.0f,    0.0f   },
        {"s04g06_0.mds", 0.0f,    0.0f,   0.0f,    0.0f   },
        {"s04g03_0.mds", 0.0f,    0.0f,   0.0f,    0.0f   },
        {"s04g05_0.mds", 0.0f,    0.0f,   0.0f,    0.0f   },
        {"s04r01_0.mds", 0.0f,    0.0f,   74.0f,   0.0f   },
        {"s04r02_0.mds", -56.68f, 0.0f,   47.566f, -50.0f },
        {"s04r03_0.mds", 32.439f, 0.0f,   -66.51f, 154.0f },
        {"s04r05_0.mds", 82.0f,   -10.0f, 109.0f,  -90.0f },
        {"s04r06_0.mds", 61.832f, 10.0f,  -127.0f, -28.0f },
        {"s04r07_0.mds", -54.64f, 10.0f,  -115.6f, 25.0f  },
        {"s04r08_0.mds", -90.85f, 10.0f,  75.585f, 125.0f },
        {"s04w01_0.mds", 0.0f,    0.0f,   0.0f,    0.0f   },
        {"s04w02_0.mds", 0.0f,    0.0f,   0.0f,    0.0f   },
        {"s04h01_0.mds", 0.0f,    -10.0f, 0.0f,    0.0f   },
        {"s04h02_0.mds", 127.0f,  -10.0f, 109.0f,  0.0f   },
        {"s04h03_0.mds", -140.0f, -10.0f, 110.0f,  125.0f },
        {"s04h03_0.mds", 90.0f,   -10.0f, -180.0f, -28.0f },
        {"s04h03_0.mds", -80.0f,  -10.0f, -170.0f, 25.0f  },
        {"s04a01_0.mds", -63.15f, -15.0f, 128.0f,  50.0f  },
        {"s04a01_0.mds", -126.0f, -15.0f, -110.0f, -20.0f },
        {"s04a01_0.mds", -178.0f, -15.0f, 46.0f,   10.0f  },
        {"s04a01_0.mds", 12.0f,   -15.0f, -218.0f, -70.0f },
        {"s04a01_0.mds", 204.0f,  -15.0f, -152.0f, -140.0f},
        {"s04a01_0.mds", 244.0f,  -15.0f, 14.0f,   190.0f }
    };

    OP_FireList = 0;
    OP_AnimeSeqRot = 0;
    MapDataBuffer.Reset();

    CFrameAttr map_attr;
    CFrameVu1 *map;

    for (int i = 0; i < 26; i++) {
        map = LoadMDSFile(GetPackFile(read_buffer, norn[i].name, 0), &MapDataBuffer, 2, 0, 0);

        map_attr.fog_enable = true;
        map->SetAttr(map_attr, 1, 64);
        SetFrameAttr(map, 1);

        CMapObject &object = OP_NornMapObj[i];

        object.Initialize();
        object.SetFrame(map, 0);
        OP_NornMapObj[i].handle = 0;
        OP_NornMapObj[i].category_no = 0;
        object.SetPosition(CVector3_f_(norn[i].position[0], norn[i].position[1], norn[i].position[2]));
        object.SetRotation(CVector3_f_(0.0f, (float) (PI_D * norn[i].rotation / 180), 0.0f));
    }

    PathDataBuffer.Reset();
    SceneNp = -1;

    char *campath[4] = {"rm06cam.cfg"};

    for (int j = 0; j < 1; j++) {
        Cam[j].LoadPackData(read_buffer, campath[j], &PathDataBuffer, 0);
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
    Water__2.frame.SetPosition((float) (OP_FireList & 0), 0.0f, 0.0f);
    Water__2.SetSize(32, 32, &WaterBuffer);
    Water__2.SetParam(0.1f, 0.015f, 0.0f, 2.0f);
    Water__2.SetColor(128, 128, 128, 128);
}

void DrawProcC();

/**
 *
 *
 * @mangled InitProcD__Fv
 * @address 0x1DCDE60
 * @size 0x614
 * @unknownret
 */
static void InitProcD() {
    LOADTEXTURE_INFO2 textures[] = {
        {"#blender#640#" HALF_BUFFER_HEIGHT_STR "#4",     0,  0},
        {"#frame_image#640#" HALF_BUFFER_HEIGHT_STR "#4", 22, 0},
        {"#shadow_buff#640#" HALF_BUFFER_HEIGHT_STR "#4", 23, 0},
        {0,                                               20, 0},
        {0,                                               10, 0},
        {0,                                               1,  0},
        {0,                                               1,  0},
        {0,                                               2,  0},
        {0,                                               9,  0},
        {"",                                              0,  0}
    };

    switch (LanguageCode) {
        case LANG_JAPANESE:
            textures[3].name = (char *) GetPackFile(read_buffer, "start.img", 0);
            break;
        case LANG_ENGLISH_US:
            textures[3].name = (char *) GetPackFile(read_buffer, "start.img", 0);
            break;
        case LANG_ENGLISH_UK:
            textures[3].name = (char *) GetPackFile(read_buffer, "start.img", 0);
            break;
        case LANG_FRENCH:
            textures[3].name = (char *) GetPackFile(read_buffer, "start_f.img", 0);
            break;
        case LANG_GERMAN:
            textures[3].name = (char *) GetPackFile(read_buffer, "start_g.img", 0);
            break;
        case LANG_ITALIAN:
            textures[3].name = (char *) GetPackFile(read_buffer, "start_i.img", 0);
            break;
        case LANG_SPANISH:
            textures[3].name = (char *) GetPackFile(read_buffer, "start_s.img", 0);
            break;
    }

    textures[4].name = (char *) GetPackFile(read_buffer, "d02i01.img", 0);
    textures[5].name = (char *) GetPackFile(read_buffer, "c01d01.img", 0);
    textures[6].name = (char *) GetPackFile(read_buffer, "c01d01an.img", 0);
    textures[7].name = (char *) GetPackFile(read_buffer, "e54a01.img", 0);
    textures[8].name = (char *) GetPackFile(read_buffer, "c01w11.img", 0);
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
    Chara__3[0].LoadPackData(read_buffer, "c01d.cfg", &CharaDataBuffer, 0);

    for (int j = 0; j < 3; j++) {
        Chara__3[j].LoadPackData(read_buffer, chara[j], &CharaDataBuffer, 0);

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
    Chara__3[8].LoadPackData(read_buffer, "c01w11.cfg", &CharaDataBuffer, 0);
    Chara__3[0].TexAnimeOn(2);
    Chara__3[0].wind = (int) &Wind__4;

    OP_FireList = 0;
    OP_AnimeSeqRot = 0;
    MapDataBuffer.Reset();
    OP_GroundMap.Initialize();
    OP_BuildingMap.Initialize();
    OP_BuildingMap2.Initialize();

    CFrameAttr map_attr;

    CFrameVu1 *map = LoadMDSFile(GetPackFile(read_buffer, "s44g01_0.mds", 0), &MapDataBuffer, 2, 0, 0);

    map_attr.fog_enable = true;
    map->SetAttr(map_attr, 1, 64);
    SetFrameAttr(map, 1);

    CMapObject *object = OP_GroundMap.SetObject(map, 0, 0);

    object->SetPosition(CVector3_f_(0.0f, 0.0f, 0.0f));
    object->SetRotation(CVector3_f_(0.0f, 0.0f, 0.0f));

    map = LoadMDSFile(GetPackFile(read_buffer, "s44g02_0.mds", 0), &MapDataBuffer, 2, 0, 0);
    map->SetAttr(map_attr, 1, 64);
    SetFrameAttr(map, 1);
    object = OP_BuildingMap.SetObject(map, 0, 0);
    object->SetPosition(CVector3_f_(0.0f, 0.0f, 0.0f));
    object->SetRotation(CVector3_f_(0.0f, 0.0f, 0.0f));

    PathDataBuffer.Reset();
    SceneNp = -1;

    char *campath[1] = {"rm07cam.cfg"};

    for (int k = 0; k < 1; k++) {
        Cam[k].LoadPackData(read_buffer, campath[k], &PathDataBuffer, 0);
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

void DrawProcD();

/**
 *
 *
 * @mangled InitProcE__Fv
 * @address 0x1DCE6A0
 * @size 0x538
 * @unknownret
 */
static void InitProcE() {
    LOADTEXTURE_INFO2 textures[] = {
        {"#blender#640#" HALF_BUFFER_HEIGHT_STR "#4",     0,  0},
        {"#frame_image#640#" HALF_BUFFER_HEIGHT_STR "#4", 22, 0},
        {"#shadow_buff#640#" HALF_BUFFER_HEIGHT_STR "#4", 23, 0},
        {0,                                               20, 0},
        {0,                                               10, 0},
        {0,                                               1,  0},
        {0,                                               1,  0},
        {0,                                               2,  0},
        {0,                                               2,  0},
        {"",                                              0,  0}
    };

    switch (LanguageCode) {
        case LANG_JAPANESE:
            textures[3].name = (char *) GetPackFile(read_buffer, "start.img", 0);
            break;
        case LANG_ENGLISH_US:
            textures[3].name = (char *) GetPackFile(read_buffer, "start.img", 0);
            break;
        case LANG_ENGLISH_UK:
            textures[3].name = (char *) GetPackFile(read_buffer, "start.img", 0);
            break;
        case LANG_FRENCH:
            textures[3].name = (char *) GetPackFile(read_buffer, "start_f.img", 0);
            break;
        case LANG_GERMAN:
            textures[3].name = (char *) GetPackFile(read_buffer, "start_g.img", 0);
            break;
        case LANG_ITALIAN:
            textures[3].name = (char *) GetPackFile(read_buffer, "start_i.img", 0);
            break;
        case LANG_SPANISH:
            textures[3].name = (char *) GetPackFile(read_buffer, "start_s.img", 0);
            break;
    }

    textures[4].name = (char *) GetPackFile(read_buffer, "s4501.img", 0);
    textures[5].name = (char *) GetPackFile(read_buffer, "c01d01.img", 0);
    textures[6].name = (char *) GetPackFile(read_buffer, "c01d01an.img", 0);
    textures[7].name = (char *) GetPackFile(read_buffer, "c04b01.img", 0);
    textures[8].name = (char *) GetPackFile(read_buffer, "c04b01an.img", 0);
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
    Chara__3[0].LoadPackData(read_buffer, "c01d.cfg", &CharaDataBuffer, 0);

    for (int j = 0; j < 2; j++) {
        Chara__3[j].LoadPackData(read_buffer, chara[j], &CharaDataBuffer, 0);

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
    Chara__3[0].wind = (int) &Wind__4;

    OP_FireList = 0;
    OP_AnimeSeqRot = 0;
    MapDataBuffer.Reset();
    OP_GroundMap.Initialize();
    OP_BuildingMap.Initialize();
    OP_BuildingMap2.Initialize();

    CFrameAttr map_attr;

    CFrameVu1 *map = LoadMDSFile(GetPackFile(read_buffer, "s4501.mds", 0), &MapDataBuffer, 2, 0, 0);

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
        Cam[k].LoadPackData(read_buffer, campath[k], &PathDataBuffer, 0);
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

void DrawProcE();

/**
 *
 *
 * @mangled InitProcF__Fv
 * @address 0x1DCEE10
 * @size 0x690
 * @unknownret
 */
static void InitProcF() {
    LOADTEXTURE_INFO2 textures[] = {
        {"#blender#640#" HALF_BUFFER_HEIGHT_STR "#4",     0,  0},
        {"#frame_image#640#" HALF_BUFFER_HEIGHT_STR "#4", 22, 0},
        {"#shadow_buff#640#" HALF_BUFFER_HEIGHT_STR "#4", 23, 0},
        {0,                                               20, 0},
        {0,                                               10, 0},
        {0,                                               1,  0},
        {0,                                               1,  0},
        {0,                                               2,  0},
        {0,                                               3,  0},
        {0,                                               9,  0},
        {"",                                              0,  0}
    };

    switch (LanguageCode) {
        case LANG_JAPANESE:
            textures[3].name = (char *) GetPackFile(read_buffer, "start.img", 0);
            break;
        case LANG_ENGLISH_US:
            textures[3].name = (char *) GetPackFile(read_buffer, "start.img", 0);
            break;
        case LANG_ENGLISH_UK:
            textures[3].name = (char *) GetPackFile(read_buffer, "start.img", 0);
            break;
        case LANG_FRENCH:
            textures[3].name = (char *) GetPackFile(read_buffer, "start_f.img", 0);
            break;
        case LANG_GERMAN:
            textures[3].name = (char *) GetPackFile(read_buffer, "start_g.img", 0);
            break;
        case LANG_ITALIAN:
            textures[3].name = (char *) GetPackFile(read_buffer, "start_i.img", 0);
            break;
        case LANG_SPANISH:
            textures[3].name = (char *) GetPackFile(read_buffer, "start_s.img", 0);
            break;
    }

    textures[4].name = (char *) GetPackFile(read_buffer, "d02b01.img", 0);
    textures[5].name = (char *) GetPackFile(read_buffer, "c01d01.img", 0);
    textures[6].name = (char *) GetPackFile(read_buffer, "c01d01an.img", 0);
    textures[7].name = (char *) GetPackFile(read_buffer, "c14a01.img", 0);
    textures[8].name = (char *) GetPackFile(read_buffer, "rm11rock.img", 0);
    textures[9].name = (char *) GetPackFile(read_buffer, "c01w01.img", 0);
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
    Chara__3[0].LoadPackData(read_buffer, "c01d.cfg", &CharaDataBuffer, 0);

    for (int j = 0; j < 6; j++) {
        Chara__3[j].LoadPackData(read_buffer, chara[j], &CharaDataBuffer, 0);

        CFrameAttr attr;

        attr.clip_enable = false;
        Chara__3[j].frame->SetAttr(attr, 1, 4);
        Chara__3[j].motion_type.state.time = 1.0f;
        Chara__3[j].motion_type.state.blend_step = 0.05f;
        Chara__3[j].motion_type.state.motion_no = 0;
        Chara__3[j].motion_type.state.playing_no = 0;
    }

    Chara__3[0].TexAnimeOn(4);
    Chara__3[0].wind = (int) &Wind__4;
    Chara__3[8].LoadPackData(read_buffer, "c01w01.cfg", &CharaDataBuffer, 0);
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

    CFrameVu1 *map = LoadMDSFile(GetPackFile(read_buffer, "s4601.mds", 0), &MapDataBuffer, 2, 0, 0);

    map_attr.fog_enable = true;
    map->SetAttr(map_attr, 1, 64);
    SetFrameAttr(map, 1);

    CMapObject *object = OP_GroundMap.SetObject(map, 0, 0);

    object->SetPosition(CVector3_f_(0.0f, 0.0f, 0.0f));
    object->SetRotation(CVector3_f_(0.0f, 0.0f, 0.0f));

    map = LoadMDSFile(GetPackFile(read_buffer, "s4602.mds", 0), &MapDataBuffer, 2, 0, 0);
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
        Cam[k].LoadPackData(read_buffer, campath[k], &PathDataBuffer, 0);
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

void DrawProcF();

/**
 *
 *
 * @mangled InitProcG__Fv
 * @address 0x1DCF720
 * @size 0x5D0
 * @unknownret
 */
static void InitProcG() {
    LOADTEXTURE_INFO2 textures[] = {
        {"#blender#640#" HALF_BUFFER_HEIGHT_STR "#4",     0,  0},
        {"#frame_image#640#" HALF_BUFFER_HEIGHT_STR "#4", 22, 0},
        {"#shadow_buff#640#" HALF_BUFFER_HEIGHT_STR "#4", 23, 0},
        {0,                                               20, 0},
        {0,                                               10, 0},
        {0,                                               10, 0},
        {0,                                               10, 0},
        {0,                                               1,  0},
        {0,                                               2,  0},
        {0,                                               2,  0},
        {0,                                               2,  0},
        {"",                                              0,  0}
    };

    switch (LanguageCode) {
        case LANG_JAPANESE:
            textures[3].name = (char *) GetPackFile(read_buffer, "start.img", 0);
            break;
        case LANG_ENGLISH_US:
            textures[3].name = (char *) GetPackFile(read_buffer, "start.img", 0);
            break;
        case LANG_ENGLISH_UK:
            textures[3].name = (char *) GetPackFile(read_buffer, "start.img", 0);
            break;
        case LANG_FRENCH:
            textures[3].name = (char *) GetPackFile(read_buffer, "start_f.img", 0);
            break;
        case LANG_GERMAN:
            textures[3].name = (char *) GetPackFile(read_buffer, "start_g.img", 0);
            break;
        case LANG_ITALIAN:
            textures[3].name = (char *) GetPackFile(read_buffer, "start_i.img", 0);
            break;
        case LANG_SPANISH:
            textures[3].name = (char *) GetPackFile(read_buffer, "start_s.img", 0);
            break;
    }

    textures[4].name = (char *) GetPackFile(read_buffer, "s4701.img", 0);
    textures[5].name = (char *) GetPackFile(read_buffer, "e02s01.img", 0);
    textures[6].name = (char *) GetPackFile(read_buffer, "e02s06.img", 0);
    textures[7].name = (char *) GetPackFile(read_buffer, "c01d01.img", 0);
    textures[8].name = (char *) GetPackFile(read_buffer, "c06a01.img", 0);
    textures[9].name = (char *) GetPackFile(read_buffer, "c06a01an.img", 0);
    textures[10].name = (char *) GetPackFile(read_buffer, "c06w01.img", 0);
    TexManager.Initialize(16352);
    TexManager.LoadTextureBlock(-1, textures);

    CharaTex[0] = 1;
    CharaTex[1] = 2;
    CharaTex[2] = 2;

    char *chara[3] = {"rm14ebc01d.cfg", "rm14ebc06a.cfg", "rm13c06a.cfg"};

    CharaDataBuffer.Reset();
    Chara__3[0].LoadPackData(read_buffer, "c01d.cfg", &CharaDataBuffer, 0);

    for (int i = 0; i < 3; i++) {
        Chara__3[i].LoadPackData(read_buffer, chara[i], &CharaDataBuffer, 0);

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
    Chara__3[0].wind = (int) &Wind__4;
    Chara__3[1].wind = (int) &Wind__4;
    Chara__3[2].wind = (int) &Wind__4;

    OP_FireList = 0;
    OP_AnimeSeqRot = 0;
    MapDataBuffer.Reset();
    OP_GroundMap.Initialize();
    OP_BuildingMap.Initialize();
    OP_BuildingMap2.Initialize();

    CFrameAttr map_attr;

    CFrameVu1 *map = LoadMDSFile(GetPackFile(read_buffer, "s4701.mds", 0), &MapDataBuffer, 2, 0, 0);

    map_attr.fog_enable = true;
    map->SetAttr(map_attr, 1, 64);
    SetFrameAttr(map, 1);

    CMapObject *object = OP_GroundMap.SetObject(map, 0, 0);

    object->SetPosition(CVector3_f_(0.0f, 0.0f, 0.0f));
    object->SetRotation(CVector3_f_(0.0f, 0.0f, 0.0f));

    map = LoadMDSFile(GetPackFile(read_buffer, "e02s01_0.mds", 0), &MapDataBuffer, 2, 0, 0);
    map->SetAttr(map_attr, 1, 64);
    SetFrameAttr(map, 1);
    object = OP_BuildingMap.SetObject(map, 0, 0);
    object->SetPosition(CVector3_f_(0.0f, 0.0f, 0.0f));
    object->SetRotation(CVector3_f_(0.0f, 0.0f, 0.0f));

    PathDataBuffer.Reset();
    SceneNp = -1;

    char *campath[3] = {"rm12cam.cfg", "rm13cam.cfg", "rm14cam.cfg"};

    for (int j = 0; j < 3; j++) {
        Cam[j].LoadPackData(read_buffer, campath[j], &PathDataBuffer, 0);
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

void DrawProcG();

/**
 *
 *
 * @mangled InitProcH__Fv
 * @address 0x1DCFEF0
 * @size 0x9E8
 * @unknownret
 */
static void InitProcH() {
    LOADTEXTURE_INFO2 textures[] = {
        {"#blender#640#" HALF_BUFFER_HEIGHT_STR "#4",     0,  0},
        {"#frame_image#640#" HALF_BUFFER_HEIGHT_STR "#4", 22, 0},
        {"#shadow_buff#640#" HALF_BUFFER_HEIGHT_STR "#4", 23, 0},
        {0,                                               20, 0},
        {0,                                               0,  0},
        {0,                                               10, 0},
        {0,                                               1,  0},
        {0,                                               2,  0},
        {0,                                               3,  0},
        {0,                                               4,  0},
        {0,                                               9,  0},
        {"",                                              0,  0}
    };

    switch (LanguageCode) {
        case LANG_JAPANESE:
            textures[3].name = (char *) GetPackFile(read_buffer, "start.img", 0);
            break;
        case LANG_ENGLISH_US:
            textures[3].name = (char *) GetPackFile(read_buffer, "start.img", 0);
            break;
        case LANG_ENGLISH_UK:
            textures[3].name = (char *) GetPackFile(read_buffer, "start.img", 0);
            break;
        case LANG_FRENCH:
            textures[3].name = (char *) GetPackFile(read_buffer, "start_f.img", 0);
            break;
        case LANG_GERMAN:
            textures[3].name = (char *) GetPackFile(read_buffer, "start_g.img", 0);
            break;
        case LANG_ITALIAN:
            textures[3].name = (char *) GetPackFile(read_buffer, "start_i.img", 0);
            break;
        case LANG_SPANISH:
            textures[3].name = (char *) GetPackFile(read_buffer, "start_s.img", 0);
            break;
    }

    textures[4].name = (char *) GetPackFile(read_buffer, "fire.img", 0);
    textures[5].name = (char *) GetPackFile(read_buffer, "d01b01.img", 0);
    textures[6].name = (char *) GetPackFile(read_buffer, "c12a01.img", 0);
    textures[7].name = (char *) GetPackFile(read_buffer, "c01d01.img", 0);
    textures[8].name = (char *) GetPackFile(read_buffer, "f_boll_2.img", 0);
    textures[9].name = (char *) GetPackFile(read_buffer, "rm16yuka.img", 0);
    textures[10].name = (char *) GetPackFile(read_buffer, "c01w01.img", 0);
    TexManager.Initialize(16352);
    TexManager.LoadTextureBlock(-1, textures);

    CharaTex[0] = 1;
    CharaTex[1] = 2;
    CharaTex[2] = 3;
    CharaTex[3] = 4;
    CharaTex[8] = 9;

    Chara__3[0].LoadPackData(read_buffer, "rm15c12a.cfg", &CharaDataBuffer, 0);

    CFrameAttr attr;

    attr.clip_enable = false;
    Chara__3[0].frame->SetAttr(attr, 1, 4);
    Chara__3[0].motion_type.state.time = 10.0f;
    Chara__3[0].motion_type.state.blend_step = 0.05f;
    Chara__3[0].motion_type.state.motion_no = 0;
    Chara__3[0].motion_type.state.playing_no = 0;
    Chara__3[0].FootSoundEnable(0);

    Chara__3[1].LoadPackData(read_buffer, "c01d.cfg", &CharaDataBuffer, 0);
    attr.clip_enable = false;
    Chara__3[1].frame->SetAttr(attr, 1, 4);
    Chara__3[1].motion_type.state.time = 70.0f;
    Chara__3[1].motion_type.state.blend_step = 0.05f;
    Chara__3[1].motion_type.state.motion_no = 0;
    Chara__3[1].motion_type.state.playing_no = 0;

    Chara__3[2].LoadPackData(read_buffer, "f_boll_2.cfg", &CharaDataBuffer, 0);
    attr.clip_enable = false;
    Chara__3[2].frame->SetAttr(attr, 1, 4);
    Chara__3[2].motion_type.state.time = 20.0f;
    Chara__3[2].motion_type.state.blend_step = 0.05f;
    Chara__3[2].motion_type.state.motion_no = 0;
    Chara__3[2].motion_type.state.playing_no = 0;

    Chara__3[3].LoadPackData(read_buffer, "rm16yuka.cfg", &CharaDataBuffer, 0);
    attr.clip_enable = false;
    Chara__3[3].frame->SetAttr(attr, 1, 4);
    Chara__3[3].motion_type.state.time = 2.0f;
    Chara__3[3].motion_type.state.blend_step = 0.05f;
    Chara__3[3].motion_type.state.motion_no = 0;
    Chara__3[3].motion_type.state.playing_no = 0;

    Chara__3[8].LoadPackData(read_buffer, "c01w01.cfg", &CharaDataBuffer, 0);
    Chara__3[1].wind = (int) &Wind__4;

    OP_FireList = 0;
    OP_AnimeSeqRot = 0;
    MapDataBuffer.Reset();
    OP_GroundMap.Initialize();
    OP_BuildingMap.Initialize();
    OP_BuildingMap2.Initialize();

    CFrameAttr map_attr;

    CFrameVu1 *map = LoadMDSFile(GetPackFile(read_buffer, "s4801.mds", 0), &MapDataBuffer, 2, 0, 0);

    map_attr.fog_enable = true;
    map->SetAttr(map_attr, 1, 64);
    SetFrameAttr(map, 1);

    CMapObject *object = OP_GroundMap.SetObject(map, 0, 0);

    object->SetPosition(CVector3_f_(0.0f, 0.0f, 0.0f));
    object->SetRotation(CVector3_f_(0.0f, 0.0f, 0.0f));

    map = LoadMDSFile(GetPackFile(read_buffer, "s4802.mds", 0), &MapDataBuffer, 2, 0, 0);
    map_attr.fog_enable = true;
    map->SetAttr(map_attr, 1, 64);
    SetFrameAttr(map, 1);
    object = OP_BuildingMap.SetObject(0, map, 0, 0);
    object->SetPosition(CVector3_f_(0.0f, 0.0f, 0.0f));
    object->SetRotation(CVector3_f_(0.0f, 0.0f, 0.0f));

    map = LoadMDSFile(GetPackFile(read_buffer, "s4803.mds", 0), &MapDataBuffer, 2, 0, 0);
    map_attr.fog_enable = true;
    map->SetAttr(map_attr, 1, 64);
    SetFrameAttr(map, 1);
    object = OP_BuildingMap.SetObject(1, map, 0, 0);
    object->SetPosition(CVector3_f_(0.0f, 0.0f, 0.0f));
    object->SetRotation(CVector3_f_(0.0f, 0.0f, 0.0f));

    map = LoadMDSFile(GetPackFile(read_buffer, "s4804.mds", 0), &MapDataBuffer, 2, 0, 0);
    map_attr.fog_enable = true;
    map->SetAttr(map_attr, 1, 64);
    SetFrameAttr(map, 1);
    object = OP_BuildingMap.SetObject(2, map, 0, 0);
    object->SetPosition(CVector3_f_(0.0f, 0.0f, 0.0f));
    object->SetRotation(CVector3_f_(0.0f, 0.0f, 0.0f));

    map = LoadMDSFile(GetPackFile(read_buffer, "s4805.mds", 0), &MapDataBuffer, 2, 0, 0);
    map_attr.fog_enable = true;
    map->SetAttr(map_attr, 1, 64);
    SetFrameAttr(map, 1);
    object = OP_BuildingMap.SetObject(3, map, 0, 0);
    object->SetPosition(CVector3_f_(0.0f, 0.0f, 0.0f));
    object->SetRotation(CVector3_f_(0.0f, 0.0f, 0.0f));

    map = LoadMDSFile(GetPackFile(read_buffer, "s4806.mds", 0), &MapDataBuffer, 2, 0, 0);
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
        Cam[i].LoadPackData(read_buffer, campath[i], &PathDataBuffer, 0);
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

void DrawProcH();

/**
 *
 *
 * @mangled InitProcI__Fv
 * @address 0x1DD0CA0
 * @size 0x814
 * @unknownret
 */
static void InitProcI() {
    LOADTEXTURE_INFO2 textures[] = {
        {"#blender#640#" HALF_BUFFER_HEIGHT_STR "#4",     0,  0},
        {"#frame_image#640#" HALF_BUFFER_HEIGHT_STR "#4", 22, 0},
        {"#shadow_buff#640#" HALF_BUFFER_HEIGHT_STR "#4", 23, 0},
        {0,                                               20, 0},
        {0,                                               3,  0},
        {0,                                               10, 0},
        {0,                                               10, 0},
        {0,                                               1,  0},
        {0,                                               2,  0},
        {"",                                              0,  0}
    };

    switch (LanguageCode) {
        case LANG_JAPANESE:
            textures[3].name = (char *) GetPackFile(read_buffer, "start.img", 0);
            break;
        case LANG_ENGLISH_US:
            textures[3].name = (char *) GetPackFile(read_buffer, "start.img", 0);
            break;
        case LANG_ENGLISH_UK:
            textures[3].name = (char *) GetPackFile(read_buffer, "start.img", 0);
            break;
        case LANG_FRENCH:
            textures[3].name = (char *) GetPackFile(read_buffer, "start_f.img", 0);
            break;
        case LANG_GERMAN:
            textures[3].name = (char *) GetPackFile(read_buffer, "start_g.img", 0);
            break;
        case LANG_ITALIAN:
            textures[3].name = (char *) GetPackFile(read_buffer, "start_i.img", 0);
            break;
        case LANG_SPANISH:
            textures[3].name = (char *) GetPackFile(read_buffer, "start_s.img", 0);
            break;
    }

    textures[4].name = (char *) GetPackFile(read_buffer, "e305ex2.img", 0);
    textures[5].name = (char *) GetPackFile(read_buffer, "s1202.img", 0);
    textures[6].name = (char *) GetPackFile(read_buffer, "s2401.img", 0);
    textures[7].name = (char *) GetPackFile(read_buffer, "c01d01.img", 0);
    textures[8].name = (char *) GetPackFile(read_buffer, "m18ashiba.img", 0);
    TexManager.Initialize(16352);
    TexManager.LoadTextureBlock(-1, textures);

    CharaTex[0] = 1;
    CharaTex[1] = 2;
    CharaTex[2] = 3;

    char *chara[3] = {"rm18c01d.cfg", "rm18ashiba.cfg", "info2.cfg"};

    CharaDataBuffer.Reset();
    Chara__3[0].LoadPackData(read_buffer, "c01d.cfg", &CharaDataBuffer, 0);

    for (int i = 0; i < 3; i++) {
        Chara__3[i].LoadPackData(read_buffer, chara[i], &CharaDataBuffer, 0);

        CFrameAttr attr;

        attr.clip_enable = false;
        Chara__3[i].frame->SetAttr(attr, 1, 4);
        Chara__3[i].motion_type.state.time = 1.0f;
        Chara__3[i].motion_type.state.blend_step = 0.05f;
        Chara__3[i].motion_type.state.motion_no = 0;
        Chara__3[i].motion_type.state.playing_no = 0;
    }

    Chara__3[1].motion_type.state.time = 23.0f;
    Chara__3[0].wind = (int) &Wind__4;
    Chara__3[2].SetScale(20.0f, 20.0f, 20.0f);

    OP_FireList = 0;
    OP_AnimeSeqRot = 0;
    MapDataBuffer.Reset();
    OP_GroundMap.Initialize();
    OP_BuildingMap.Initialize();
    OP_BuildingMap2.Initialize();

    CFrameAttr map_attr;

    CFrameVu1 *map = LoadMDSFile(GetPackFile(read_buffer, "s24g01_0.mds", 0), &MapDataBuffer, 2, 0, 0);

    map_attr.fog_enable = true;
    map->SetAttr(map_attr, 1, 64);
    SetFrameAttr(map, 1);

    CMapObject *object = OP_GroundMap.SetObject(map, 0, 0);

    object->SetPosition(CVector3_f_(0.0f, 0.0f, 0.0f));
    object->SetRotation(CVector3_f_(0.0f, 0.0f, 0.0f));

    map = LoadMDSFile(GetPackFile(read_buffer, "s24g02_0.mds", 0), &MapDataBuffer, 2, 0, 0);
    map_attr.fog_enable = true;
    map->SetAttr(map_attr, 1, 64);
    SetFrameAttr(map, 1);
    object = OP_BuildingMap.SetObject(0, map, 0, 0);
    object->SetPosition(CVector3_f_(0.0f, 0.0f, 0.0f));
    object->SetRotation(CVector3_f_(0.0f, 0.0f, 0.0f));

    map = LoadMDSFile(GetPackFile(read_buffer, "s24g03_0.mds", 0), &MapDataBuffer, 2, 0, 0);
    map_attr.fog_enable = true;
    map->SetAttr(map_attr, 1, 64);
    SetFrameAttr(map, 1);
    object = OP_BuildingMap.SetObject(1, map, 0, 0);
    object->SetPosition(CVector3_f_(0.0f, 0.0f, 0.0f));
    object->SetRotation(CVector3_f_(0.0f, 0.0f, 0.0f));

    map = LoadMDSFile(GetPackFile(read_buffer, "ship.mds", 0), &MapDataBuffer, 2, 0, 0);
    map_attr.fog_enable = true;
    map->SetAttr(map_attr, 1, 64);
    SetFrameAttr(map, 1);
    object = OP_BuildingMap.SetObject(2, map, 0, 0);
    object->SetPosition(CVector3_f_(0.0f, 0.0f, 0.0f));
    object->SetRotation(CVector3_f_(0.0f, 0.0f, 0.0f));

    map = LoadMDSFile(GetPackFile(read_buffer, "s24g04_0.mds", 0), &MapDataBuffer, 2, 0, 0);
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
        Cam[j].LoadPackData(read_buffer, campath[j], &PathDataBuffer, 0);
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

void DrawProcI();

/**
 *
 *
 * @mangled InitProcTitle__Fv
 * @address 0x1DD1760
 * @size 0x9C
 * @unknownret
 */
static void InitProcTitle() {
    LOADTEXTURE_INFO2 textures[] = {
        {"#blender#640#" HALF_BUFFER_HEIGHT_STR "#4",     0,  0},
        {"#frame_image#640#" HALF_BUFFER_HEIGHT_STR "#4", 22, 0},
        {"#shadow_buff#640#" HALF_BUFFER_HEIGHT_STR "#4", 23, 0},
        {0,                                               1,  0},
        {"",                                              0,  0}
    };

    textures[3].name = (char *) GetPackFile(read_buffer, "title.img", 0);
    TexManager.Initialize(16352);
    TexManager.LoadTextureBlock(-1, textures);
}

void DrawProcTitle();

static void TitleSetCamera(float x, float y, float z, float dist, float height) {
    int i = 1;
    int j = 2;
    int k = 3;
    int l = 4;

    TitleCameraWork[0] = 0.0f;
}
