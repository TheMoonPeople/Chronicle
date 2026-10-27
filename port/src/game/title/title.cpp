#include "common.h"

#include <libgraph.h>
#include <libpkt.h>
#include <libvu0.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>

#include "camera.hpp"
#include "camerafollow.hpp"
#include "character.hpp"
#include "dataalloc.hpp"
#include "dataread.hpp"
#include "effectmacro.hpp"
#include "fireomni.hpp"
#include "frame.hpp"
#include "framevu1.hpp"
#include "gamepad.hpp"
#include "map.hpp"
#include "mapobject.hpp"
#include "mathutil.hpp"
#include "mds.hpp"
#include "nowload.hpp"
#include "savedata.hpp"
#include "snd.hpp"
#include "sound.hpp"
#include "title/cursol.hpp"
#include "title/logo.hpp"
#include "title/scfader.hpp"
#include "title/sprite.hpp"

namespace dc::game {

/**
 * Screen or texel rectangle taken by the title overlay's 2D draws, built as left, top, width, height.
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
 * One piece of scenery the third scene lays out: model file, position and turn about the vertical
 * axis in degrees.
 */
struct MAP_INFO {
    char *name;        /**< Name of the model file in the scene's pack. */
    float position[3]; /**< Where the piece stands in the world. */
    float rotation;    /**< Turn about the vertical axis, in degrees. */
};

} // namespace dc::game

#include "gamemode.hpp"
#include "main.hpp"
#include "mainselect.hpp"
#include "memcard.hpp"
#include "mglib.hpp"
#include "objanime.hpp"
#include "object.hpp"
#include "rect.hpp"
#include "runeffect.hpp"
#include "texture.hpp"
#include "title/dispfade.hpp"
#include "title/op_a.hpp"
#include "title/op_b.hpp"
#include "title/op_c.hpp"
#include "title/opening.hpp"
#include "title/rushmovi.hpp"
#include "title/script.hpp"
#include "title/title.hpp"
#include "title/title_draw.hpp"
#include "title/titleloop.hpp"
#include "vutext.hpp"
#include "water.hpp"
#include "wind.hpp"

namespace dc::game {

using namespace dc::title;

// String literals are passed to the game APIs as char *.
#pragma clang diagnostic ignored "-Wwritable-strings"

void set2DSprite(sceVif1Packet *packet, CTexture *texture, const CRect<int> &dst, const CRect<int> &src, int width,
                 int height, float angle);

// The title units declare their own rectangle template where every other unit takes CRect_i_.
// MWCC mangled both the same, so retail linked the title's calls to the shared definitions; clang
// mangles CRect<int> apart, so these give the title's spelling a body (op_a.cpp has MoveImageTest's).
namespace {

CRect_i_ Rect(const CRect<int> &rect) { return CRect_i_(rect.x, rect.y, rect.w, rect.h); }

} // namespace

void set2DSprite(sceVif1Packet *packet, CTexture *texture, const CRect<int> &screen, const CRect<int> &texel,
                 u_char alpha) {
    set2DSprite(packet, texture, Rect(screen), Rect(texel), alpha);
}

void set2DSprite(sceVif1Packet *packet, CTexture *texture, const CRect<int> &screen, const CRect<int> &texel,
                 u_char red, u_char green, u_char blue, u_char alpha) {
    set2DSprite(packet, texture, Rect(screen), Rect(texel), red, green, blue, alpha);
}

void set2DSprite(sceVif1Packet *packet, CTexture *texture, const CRect<int> &screen, const CRect<int> &texel,
                 int pivot_x, int pivot_y, float angle) {
    set2DSprite(packet, texture, Rect(screen), Rect(texel), pivot_x, pivot_y, angle);
}

void MGMoveImage(sceGsTex0 *src, const CRect<int> &rect, sceGsTex0 *dst, int dst_x, int dst_y, int direction) {
    MGMoveImage(src, Rect(rect), dst, dst_x, dst_y, direction);
}

void MGFillBox(const CRect<int> &rect, u_char r, u_char g, u_char b, u_char a) { MGFillBox(Rect(rect), r, g, b, a); }

namespace {
void InitProcA();
void InitProcB();
void AtraLight();
void InitProcC();
void InitProcD();
void InitProcE();
void InitProcF();
void InitProcG();
void InitProcH();
void InitProcI();
void InitProcTitle();

void SetFirePosition(CFireOmni &fire, float x, float y, float z) {
    fire.pos[0] = 10.0f * x;
    fire.pos[1] = 10.0f * y;
    fire.pos[2] = 10.0f * z;
    fire.pos[3] = 1.0f;
}

void LoadStartImage(LOADTEXTURE_INFO2 &entry) {
#ifdef PAL
    switch (LanguageCode) {
        case LANG_JAPANESE:
        case LANG_ENGLISH_US:
        case LANG_ENGLISH_UK:
            entry.name = reinterpret_cast<char *>(GetPackFile(read_buffer, "start.img", nullptr));
            break;
        case LANG_FRENCH:
            entry.name = reinterpret_cast<char *>(GetPackFile(read_buffer, "start_f.img", nullptr));
            break;
        case LANG_GERMAN:
            entry.name = reinterpret_cast<char *>(GetPackFile(read_buffer, "start_g.img", nullptr));
            break;
        case LANG_ITALIAN:
            entry.name = reinterpret_cast<char *>(GetPackFile(read_buffer, "start_i.img", nullptr));
            break;
        case LANG_SPANISH:
            entry.name = reinterpret_cast<char *>(GetPackFile(read_buffer, "start_s.img", nullptr));
            break;
        default:
            break;
    }
#else
    entry.name = reinterpret_cast<char *>(GetPackFile(read_buffer, "start.img", nullptr));
#endif
}
} // namespace

CCamera       Camera(4.0f);
CCameraFollow FCamera(60.0f, 20.0f, 0.0f, 4.0f);
CCharacter    Cloud__2;
CCharacter    Logo;
CCharacter    Spark[9];

void DataLoad() {
    if (CScript.load_no != RUSH_SCENE_NONE) {
        while (ReadBGSync()) {
        }
    }

    switch (CScript.load_no) {
        case RUSH_SCENE_A: {
            void *buffer = read_buffer;
            LoadFile("rmdat/rmdat1.pak", buffer, nullptr);
            break;
        }
        case RUSH_SCENE_B:
            LoadFileBG("rmdat/rmdat2.pak", reinterpret_cast<u_long128 *>(read_buffer), nullptr);
            break;
        case RUSH_SCENE_C:
            LoadFileBG("rmdat/rmdat3.pak", reinterpret_cast<u_long128 *>(read_buffer), nullptr);
            break;
        case RUSH_SCENE_D:
            LoadFileBG("rmdat/rmdat4.pak", reinterpret_cast<u_long128 *>(read_buffer), nullptr);
            break;
        case RUSH_SCENE_E:
            LoadFileBG("rmdat/rmdat5.pak", reinterpret_cast<u_long128 *>(read_buffer), nullptr);
            break;
        case RUSH_SCENE_F:
            LoadFileBG("rmdat/rmdat6.pak", reinterpret_cast<u_long128 *>(read_buffer), nullptr);
            break;
        case RUSH_SCENE_G:
            LoadFileBG("rmdat/rmdat7.pak", reinterpret_cast<u_long128 *>(read_buffer), nullptr);
            break;
        case RUSH_SCENE_H:
            LoadFileBG("rmdat/rmdat8.pak", reinterpret_cast<u_long128 *>(read_buffer), nullptr);
            break;
        case RUSH_SCENE_I:
            LoadFileBG("rmdat/rmdat9.pak", reinterpret_cast<u_long128 *>(read_buffer), nullptr);
            break;
        case RUSH_SCENE_TITLE:
            LoadFileBG("rmdat/title.pak", reinterpret_cast<u_long128 *>(read_buffer), nullptr);
            break;
        default:
            break;
    }

    CScript.load_no = RUSH_SCENE_NONE;

    if (CScript.init_no != RUSH_SCENE_NONE) {
        while (ReadBGSync()) {
        }

        StartReadBG();
    }

    switch (CScript.init_no) {
        case RUSH_SCENE_A:
            InitProcA();
            break;
        case RUSH_SCENE_B:
            InitProcB();
            break;
        case RUSH_SCENE_C:
            InitProcC();
            break;
        case RUSH_SCENE_D:
            InitProcD();
            break;
        case RUSH_SCENE_E:
            InitProcE();
            break;
        case RUSH_SCENE_F:
            InitProcF();
            break;
        case RUSH_SCENE_G:
            InitProcG();
            break;
        case RUSH_SCENE_H:
            InitProcH();
            break;
        case RUSH_SCENE_I:
            InitProcI();
            break;
        case RUSH_SCENE_TITLE:
            InitProcTitle();
            break;
        default:
            break;
    }

    CScript.init_no = RUSH_SCENE_NONE;
}

namespace {
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
    Chara__3[0].wind = static_cast<int>(reinterpret_cast<std::intptr_t>(&Wind__4));
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

} // namespace

void DrawProcA() {
    sceVu0FMATRIX flash = {
        {100.0f, 80.0f, 60.0f, 0.0f},
        {90.0f,  90.0f, 50.0f, 0.0f},
        {0.0f,   0.0f,  0.0f,  0.0f},
        {0.0f,   0.0f,  0.0f,  0.0f}
    };
    sceVu0FMATRIX scene_color;
    sceVu0FMATRIX chara_color;

    sceVu0CopyMatrix(scene_color, lightcolor);
    sceVu0CopyMatrix(chara_color, flash);

    static int lightning = 0;
    static int flash_level = 0;

    if (rand() % 127 == 0) {
        flash_level = 254;
        lightning = 4;

        int thunder_se = rand() % 6;

        if (thunder_se < 6) {
            SndSePlay(thunder_se + 67, -1, 0);
        }
    }

    if (CScript.camera_start == 0 && Cam[SceneNp].motion_type.state.time > 20.0f && !StartLightning) {
        StartLightning = 1;
        flash_level = 254;
        lightning = 4;
        SndSePlay(SE_THUNDER_CLAP, -1, 0);
    }

    for (int i = 0; i < 4; i++) {
        if (lightcolor[i][0] < static_cast<float>(flash_level)) {
            scene_color[i][0] = static_cast<float>(flash_level);
        }

        if (lightcolor[i][1] < static_cast<float>(flash_level)) {
            scene_color[i][1] = static_cast<float>(flash_level);
        }

        if (lightcolor[i][2] < static_cast<float>(flash_level)) {
            scene_color[i][2] = static_cast<float>(flash_level);
        }

        if (flash[i][0] < static_cast<float>(flash_level)) {
            chara_color[i][0] = static_cast<float>(flash_level);
        }

        if (flash[i][1] < static_cast<float>(flash_level)) {
            chara_color[i][1] = static_cast<float>(flash_level);
        }

        if (flash[i][2] < static_cast<float>(flash_level)) {
            chara_color[i][2] = static_cast<float>(flash_level);
        }

        if (flash_level > 2) {
            flash_level -= 2;
        }
    }

    if (CScript.camera_start == 2) {
        Chara__3[6].SetScale(2.0f, 2.0f, 2.0f);
        Chara__3[8].SetScale(2.0f, 2.0f, 2.0f);
    }

    for (int i = 0; i < 9; i++) {
        if (CScript.obj[i].disp) {
            TexManager.ReloadTexture(Vif1Packet, CharaTex[i]);
            Chara__3[i].Step();
            Chara__3[i].ClothStep(0);
            MGSetPLight(light, chara_color);
            Chara__3[i].Draw();
        }
    }

    MGSetPLight(light, scene_color);
    TexManager.ReloadTexture(Vif1Packet, 10);

    CMapObject   *object = OP_GroundMap.GetObject(0);
    sceVu0FVECTOR camera_position;

    MainCamera__3.GetPos(camera_position);
    object->SetPosition(camera_position);

    if (lightning) {
        object->FrameObjectOnOff("inazuma", 1);
    } else {
        object->FrameObjectOnOff("inazuma", 0);
        OP_AnimeSeq[OP_AnimeSeqRot - 1].step[0] = static_cast<float>(rand() % 10) / 10.0f;
        ObjAnimePlay(&OP_AnimeSeq[OP_AnimeSeqRot - 1]);
    }

    if (lightning > 0) {
        lightning--;
    }

    sceVu0FVECTOR ambient = {90.0f, 90.0f, 90.0f, 128.0f};

    MGSetAmbient(ambient);

    sceGsAlpha alpha = mgAlpha;

    alpha.bits.a = 0;
    alpha.bits.b = 2;
    alpha.bits.c = 0;
    alpha.bits.d = 1;
    setAlphaFlag(Vif1Packet, &alpha);
    object->Draw();
    MGSetAmbient(ambientlight);
    setAlphaFlag(Vif1Packet, &mgAlpha);
    TexManager.ReloadTexture(Vif1Packet, 10);
    OP_BuildingMap.Draw();
    TexManager.ReloadTexture(GetVif1Packet(), 0);

    sceVu0FVECTOR eye;

    OP_CharaFrame = Cam[SceneNp].frame;
    sceVu0CopyVector(eye, OP_CharaFrame->position);

    CFire__4.FireStep();
    CFire__4.FireCreate();

    for (int i = 0; i < OP_FireList; i++) {
        float z = OP_FirePosition[i][2];
        float y = OP_FirePosition[i][1];
        float x = OP_FirePosition[i][0];

        CFire__4.pos[0] = 10.0f * x;
        CFire__4.pos[1] = 10.0f * y;
        CFire__4.pos[2] = 10.0f * z;
        CFire__4.pos[3] = 1.0f;

        CFire__4.DrawFire(1, 1, &MainCamera__3, eye, OP_FireScale[i], 15, 15.0f);
    }

    TexManager.ReloadTexture(Vif1Packet, 22);

    float dof[3] = {1000.0f, 2000.0f, 3000.0f};

    DepthOfField(dof, 3, 32, 0);
}

CFrame        *ObjectFrame3;
class CProcess CProcess;
CScFader       CFade;

class CSprite CSprite;
class CLogo   CLogo;
class CCursol CCursol;

namespace {
float TitleCameraWork[4];
} // namespace

namespace {
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
    Chara__3[0].wind = static_cast<int>(reinterpret_cast<std::intptr_t>(&Wind__4));
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

} // namespace

void DrawProcB() {
    TexManager.ReloadTexture(Vif1Packet, 10);
    OP_GroundMap.Draw();
    OP_BuildingMap.Draw();

    WaterProcess();

    TexManager.ReloadTexture(Vif1Packet, 23);
    CTexture *texture = TexManager.GetTexture("shadow_buff", -1);

    MGBeginDrawShadow(*reinterpret_cast<sceGsTex0 *>(&texture->tex0));

    for (int i = 0; i < 9; i++) {
        if (CScript.obj[i].disp) {
            Chara__3[i].ShadowStep();
            Chara__3[i].DrawShadow();
        }
    }

    MGEndDrawShadow(52);

    for (int i = 0; i < 9; i++) {
        if (CScript.obj[i].disp) {
            sceVu0FMATRIX save_light;
            sceVu0FMATRIX save_lightcolor;

            sceVu0CopyMatrix(save_light, light);
            sceVu0CopyMatrix(save_lightcolor, lightcolor);

            if (i == 0 && CScript.camera_start == 6) {
                AtraLight();
            }

            TexManager.ReloadTexture(Vif1Packet, CharaTex[i]);
            Chara__3[i].TextureAnime(CharaTex[i]);
            Chara__3[i].Step();
            Chara__3[i].ClothStep(0);
            Chara__3[i].Draw();

            sceVu0CopyMatrix(light, save_light);
            sceVu0CopyMatrix(lightcolor, save_lightcolor);
            MGSetPLight(light, lightcolor);
        }
    }

    TexManager.ReloadTexture(GetVif1Packet(), 0);

    if (CScript.camera_start == 4) {
        sceVu0FVECTOR position;

        sceVu0CopyVector(position, Chara__3[0].frame->position);
        CRunFx.Lighting(1);

        int frame = static_cast<int>(Chara__3[0].motion_type.state.time);

        if ((frame >= 73 && frame < 74) || (frame >= 83 && frame < 84)) {
            CRunFx.Set(position);
        }

        CRunFx.Step();
        CRunFx.Draw();
    }

    sceVu0FVECTOR eye;

    OP_CharaFrame = Cam[SceneNp].frame;
    sceVu0CopyVector(eye, OP_CharaFrame->position);

    CFire__4.FireStep();
    CFire__4.FireCreate();

    for (int i = 0; i < OP_FireList; i++) {
        float z = OP_FirePosition[i][2] / 10.0f;
        float y = OP_FirePosition[i][1] / 10.0f;
        float x = OP_FirePosition[i][0] / 10.0f;

        SetFirePosition(CFire__4, x, y, z);

        CFire__4.DrawFire(1, 1, &MainCamera__3, eye, OP_FireScale[i], 3, 15.0f);
    }

    TexManager.ReloadTexture(Vif1Packet, 22);

    float dof[2] = {400.0f, 1000.0f};

    DepthOfField(dof, 2, 32, 0);
}

namespace {
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

            float len = std::max(std::sqrt(dir[0] * dir[0] + dir[1] * dir[1] + dir[2] * dir[2]), 10.0f);

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
    Chara__3[0].wind = static_cast<int>(reinterpret_cast<std::intptr_t>(&Wind__4));

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
        object.SetRotation(CVector3_f_(0.0f, static_cast<float>(PI_D * norn[i].rotation / 180), 0.0f));
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

} // namespace

void DrawProcC() {
    TexManager.ReloadTexture(Vif1Packet, 10);

    for (int i = 0; i < 26; i++) {
        CMapObject *object = &OP_NornMapObj[i];

        object->Draw();
    }

    WaterProcess();

    TexManager.ReloadTexture(Vif1Packet, 23);
    CTexture *texture = TexManager.GetTexture("shadow_buff", -1);

    MGBeginDrawShadow(*reinterpret_cast<sceGsTex0 *>(&texture->tex0));

    for (int i = 0; i < 9; i++) {
        if (CScript.obj[i].disp) {
            Chara__3[i].ShadowStep();
            Chara__3[i].DrawShadow();
        }
    }

    MGEndDrawShadow(52);

    for (int i = 0; i < 9; i++) {
        if (CScript.obj[i].disp) {
            TexManager.ReloadTexture(Vif1Packet, CharaTex[i]);
            Chara__3[i].Step();
            Chara__3[i].ClothStep(0);
            Chara__3[i].Draw();
        }
    }

    sceVu0FVECTOR eye;

    TexManager.ReloadTexture(GetVif1Packet(), 0);
    OP_CharaFrame = Cam[SceneNp].frame;
    sceVu0CopyVector(eye, OP_CharaFrame->position);

    CFire__4.FireStep();
    CFire__4.FireCreate();

    for (int i = 0; i < OP_FireList; i++) {
        float z = OP_FirePosition[i][2] / 10.0f;
        float y = OP_FirePosition[i][1] / 10.0f;
        float x = OP_FirePosition[i][0] / 10.0f;

        CFire__4.pos[0] = 10.0f * x;
        CFire__4.pos[1] = 10.0f * y;
        CFire__4.pos[2] = 10.0f * z;
        CFire__4.pos[3] = 1.0f;

        CFire__4.DrawFire(1, 1, &MainCamera__3, eye, OP_FireScale[i], 2, 15.0f);
    }

    TexManager.ReloadTexture(Vif1Packet, 22);

    float dof[2] = {400.0f, 1000.0f};

    DepthOfField(dof, 2, 32, 0);
}

namespace {
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
    Chara__3[0].wind = static_cast<int>(reinterpret_cast<std::intptr_t>(&Wind__4));

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

} // namespace

void DrawProcD() {
    TexManager.ReloadTexture(Vif1Packet, 10);
    OP_GroundMap.Draw();
    OP_BuildingMap.Draw();

    TexManager.ReloadTexture(Vif1Packet, 23);
    CTexture *texture = TexManager.GetTexture("shadow_buff", -1);

    MGBeginDrawShadow(*reinterpret_cast<sceGsTex0 *>(&texture->tex0));

    for (int i = 0; i < 9; i++) {
        if (CScript.obj[i].disp) {
            Chara__3[i].ShadowStep();
            Chara__3[i].DrawShadow();
        }
    }

    MGEndDrawShadow(52);

    for (int i = 0; i < 9; i++) {
        if (CScript.obj[i].disp) {
            TexManager.ReloadTexture(Vif1Packet, CharaTex[i]);
            Chara__3[i].TextureAnime(CharaTex[i]);
            Chara__3[i].Step();
            Chara__3[i].ClothStep(0);
            Chara__3[i].Draw();
        }
    }

    TexManager.ReloadTexture(Vif1Packet, 22);

    float dof[2] = {400.0f, 1000.0f};

    DepthOfField(dof, 2, 32, 0);
}

namespace {
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
    Chara__3[0].wind = static_cast<int>(reinterpret_cast<std::intptr_t>(&Wind__4));

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

} // namespace

void DrawProcE() {
    TexManager.ReloadTexture(Vif1Packet, 10);
    OP_GroundMap.Draw();

    if (CScript.camera_start == 10 && Cam[SceneNp].motion_type.state.time >= 70.0f) {
        Chara__3[0].TexAnimeOn(3);
    }

    TexManager.ReloadTexture(Vif1Packet, 23);
    CTexture *texture = TexManager.GetTexture("shadow_buff", -1);

    MGBeginDrawShadow(*reinterpret_cast<sceGsTex0 *>(&texture->tex0));

    for (int i = 0; i < 9; i++) {
        if (CScript.obj[i].disp) {
            Chara__3[i].ShadowStep();
            Chara__3[i].DrawShadow();
        }
    }

    MGEndDrawShadow(52);

    for (int i = 0; i < 9; i++) {
        if (CScript.obj[i].disp) {
            TexManager.ReloadTexture(Vif1Packet, CharaTex[i]);
            Chara__3[i].TextureAnime(CharaTex[i]);
            Chara__3[i].Step();
            Chara__3[i].ClothStep(0);
            Chara__3[i].Draw();
        }
    }
}

namespace {
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
    Chara__3[0].wind = static_cast<int>(reinterpret_cast<std::intptr_t>(&Wind__4));
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

} // namespace

void DrawProcF() {
    TexManager.ReloadTexture(Vif1Packet, 10);
    OP_GroundMap.Draw();
    OP_BuildingMap.Draw();

    if (CScript.obj[5].disp) {
        TexManager.ReloadTexture(Vif1Packet, CharaTex[5]);
        Chara__3[5].Step();
        Chara__3[5].Draw();
    }

    TexManager.ReloadTexture(Vif1Packet, 23);
    CTexture *texture = TexManager.GetTexture("shadow_buff", -1);

    MGBeginDrawShadow(*reinterpret_cast<sceGsTex0 *>(&texture->tex0));

    for (int i = 0; i < 9; i++) {
        if (i != 5 && CScript.obj[i].disp) {
            Chara__3[i].ShadowStep();
            Chara__3[i].DrawShadow();
        }
    }

    MGEndDrawShadow(52);

    for (int i = 0; i < 9; i++) {
        if (CScript.obj[i].disp && i != 5) {
            TexManager.ReloadTexture(Vif1Packet, CharaTex[i]);
            Chara__3[i].TextureAnime(CharaTex[i]);
            Chara__3[i].Step();
            Chara__3[i].ClothStep(0);
            Chara__3[i].Draw();
        }
    }

    TexManager.ReloadTexture(Vif1Packet, 22);

    float dof[2] = {400.0f, 1000.0f};

    DepthOfField(dof, 2, 32, 0);
}

namespace {
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
    Chara__3[0].wind = static_cast<int>(reinterpret_cast<std::intptr_t>(&Wind__4));
    Chara__3[1].wind = static_cast<int>(reinterpret_cast<std::intptr_t>(&Wind__4));
    Chara__3[2].wind = static_cast<int>(reinterpret_cast<std::intptr_t>(&Wind__4));

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

} // namespace

void DrawProcG() {
    TexManager.ReloadTexture(Vif1Packet, 10);
    OP_GroundMap.Draw();
    OP_BuildingMap.Draw();

    TexManager.ReloadTexture(Vif1Packet, 23);
    CTexture *texture = TexManager.GetTexture("shadow_buff", -1);

    MGBeginDrawShadow(*reinterpret_cast<sceGsTex0 *>(&texture->tex0));

    for (int i = 0; i < 9; i++) {
        if (CScript.obj[i].disp) {
            Chara__3[i].ShadowStep();
            Chara__3[i].DrawShadow();
        }
    }

    MGEndDrawShadow(52);

    for (int i = 0; i < 9; i++) {
        if (CScript.obj[i].disp) {
            TexManager.ReloadTexture(Vif1Packet, CharaTex[i]);
            Chara__3[i].Step();
            Chara__3[i].ClothStep(0);
            Chara__3[i].Draw();
        }
    }

    TexManager.ReloadTexture(Vif1Packet, 22);

    float dof[2] = {400.0f, 1000.0f};

    DepthOfField(dof, 2, 32, 0);
}

namespace {
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
    Chara__3[1].wind = static_cast<int>(reinterpret_cast<std::intptr_t>(&Wind__4));

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

} // namespace

void DrawProcH() {
    TexManager.ReloadTexture(Vif1Packet, 10);
    OP_GroundMap.Draw();
    OP_BuildingMap.Draw();

    if (CScript.obj[3].disp) {
        TexManager.ReloadTexture(Vif1Packet, CharaTex[3]);
        Chara__3[3].Step();
        Chara__3[3].ClothStep(0);
        Chara__3[3].Draw();
    }

    TexManager.ReloadTexture(Vif1Packet, 23);
    CTexture *texture = TexManager.GetTexture("shadow_buff", -1);

    MGBeginDrawShadow(*reinterpret_cast<sceGsTex0 *>(&texture->tex0));

    for (int i = 0; i < 9; i++) {
        if (CScript.obj[i].disp) {
            Chara__3[i].ShadowStep();
            Chara__3[i].DrawShadow();
        }
    }

    MGEndDrawShadow(52);

    for (int i = 0; i < 9; i++) {
        if (i != 3 && CScript.obj[i].disp) {
            TexManager.ReloadTexture(Vif1Packet, CharaTex[i]);
            Chara__3[i].Step();
            Chara__3[i].ClothStep(0);
            Chara__3[i].Draw();
        }
    }

    sceVu0FVECTOR eye;

    TexManager.ReloadTexture(GetVif1Packet(), 0);
    OP_CharaFrame = Cam[SceneNp].frame;
    sceVu0CopyVector(eye, OP_CharaFrame->position);

    CFire__4.FireStep();
    CFire__4.FireCreate();

    for (int i = 0; i < OP_FireList; i++) {
        float z = OP_FirePosition[i][2] / 10.0f;
        float y = OP_FirePosition[i][1] / 10.0f;
        float x = OP_FirePosition[i][0] / 10.0f;

        CFire__4.pos[0] = 10.0f * x;
        CFire__4.pos[1] = 10.0f * y;
        CFire__4.pos[2] = 10.0f * z;
        CFire__4.pos[3] = 1.0f;

        CFire__4.DrawFire(1, 1, &MainCamera__3, eye, OP_FireScale[i], 3, 15.0f);
    }

    TexManager.ReloadTexture(Vif1Packet, 22);

    float dof[2] = {400.0f, 1000.0f};

    DepthOfField(dof, 2, 32, 0);
}

namespace {
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
    Chara__3[0].wind = static_cast<int>(reinterpret_cast<std::intptr_t>(&Wind__4));
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

} // namespace

void DrawProcI() {
    TexManager.ReloadTexture(Vif1Packet, 10);
    OP_GroundMap.Draw();
    OP_BuildingMap.Draw();

    if (CScript.camera_start == 18) {
        OP_BuildingMap2.Draw();
    }

    if (CScript.obj[1].disp) {
        TexManager.ReloadTexture(Vif1Packet, CharaTex[1]);
        Chara__3[1].Step();
        Chara__3[1].Draw();
    }

    if (CScript.obj[2].disp) {
        TexManager.ReloadTexture(Vif1Packet, CharaTex[2]);
        Chara__3[2].Step();
        Chara__3[2].Draw();
    }

    if (CScript.obj[0].disp) {
        sceVu0FMATRIX save_light;
        sceVu0FMATRIX save_lightcolor;

        sceVu0CopyMatrix(save_light, light);
        sceVu0CopyMatrix(save_lightcolor, lightcolor);
        light[1][0] = 1.3f;
        MGSetPLight(light, lightcolor);

        TexManager.ReloadTexture(Vif1Packet, 23);
        CTexture *texture = TexManager.GetTexture("shadow_buff", -1);

        MGBeginDrawShadow(*reinterpret_cast<sceGsTex0 *>(&texture->tex0));
        Chara__3[0].ShadowStep();
        Chara__3[0].DrawShadow();
        MGEndDrawShadow(52);

        sceVu0CopyMatrix(light, save_light);
        sceVu0CopyMatrix(lightcolor, save_lightcolor);
        MGSetPLight(light, lightcolor);

        TexManager.ReloadTexture(Vif1Packet, CharaTex[0]);
        Chara__3[0].Step();
        Chara__3[0].ClothStep(0);
        Chara__3[0].Draw();
    }

    TexManager.ReloadTexture(Vif1Packet, 22);

    float dof[2] = {400.0f, 1000.0f};

    DepthOfField(dof, 2, 32, 0);
}

namespace {
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

} // namespace

void DrawProcTitle() {
    TexManager.ReloadTexture(Vif1Packet, 1);

    TitlePortCard(TexManager.GetTexture("bg01", -1), TitleAngle);
    TitleAngle -= 0.0005f;

    set2DSprite(GetVif1Packet(), TexManager.GetTexture("dc01", -1), CRect<int>(0, 80, 288, 160), CRect<int>(0, 0, 288, 160), 128);
    set2DSprite(GetVif1Packet(), TexManager.GetTexture("dc01", -1), CRect<int>(288, 129, 352, 160), CRect<int>(288, 49, 352, 160), 128);
    set2DSprite(GetVif1Packet(), TexManager.GetTexture("dc01", -1), CRect<int>(0, 366, 640, 48), CRect<int>(0, 208, 640, 48), TitleFade);

    TitleFadeCnt++;

    if (TitleFadeCnt >= 60) {
        TitleFade = std::min(TitleFade + 1, 128);
    }

    TexManager.ReloadTexture(Vif1Packet, 22);

    float dof[3] = {1000.0f, 2000.0f, 3000.0f};
    DepthOfField(dof, 3, 32, 0);
}

namespace {

void TitleSetCamera([[maybe_unused]] float x, [[maybe_unused]] float y, [[maybe_unused]] float z,
                    [[maybe_unused]] float dist, [[maybe_unused]] float height) {
    TitleCameraWork[0] = 0.0f;
}

} // namespace

// The title overlay's objects use the host-sized layouts declared here rather than the title units'
// own. TitleOverlayConstruct rebuilds them the way the overlay loader does: zeroed, then constructed.

namespace {

template <class T>
void Construct(T &object) {
    std::memset(static_cast<void *>(&object), 0, sizeof(T));
    std::construct_at(&object);
}

} // namespace

// op_a.cpp's fire; op_a.hpp cannot declare it, since op_b.cpp and op_c.cpp include that header
// and keep file-local fires of the same name.
extern CFireOmni CFire;

void TitleOverlayConstruct() {
    for (CMap *map : {&OP_GroundMap, &OP_BuildingMap, &OP_BuildingMap2}) {
        Construct(*map);
        // map.hpp's CMap constructor leaves initialisation to the caller.
        map->Initialize();
    }
    for (OBJ_ANIME_SEQ &sequence : OP_AnimeSeq) {
        Construct(sequence);
    }
    for (CMapObject &object : OP_NornMapObj) {
        Construct(object);
    }
    for (CMapObject &object : OP_NornMapObj2) {
        Construct(object);
    }
    Construct(CFire);
    Construct(CFire__4);
    Construct(Water);
    Construct(Water__2);
}

} // namespace dc::game
