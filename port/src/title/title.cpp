
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
#include "map.hpp"
#include "mapobject.hpp"
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

#include "gamemode.hpp"
#include "main.hpp"
#include "memcard.hpp"
#include "mglib.hpp"
#include "objanime.hpp"
#include "object.hpp"
#include "runeffect.hpp"
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
#include "title_port.hpp"

// The bodies are retail's, which pass string literals as char * and brace-elide their tables.
#pragma clang diagnostic ignored "-Wwritable-strings"
#pragma clang diagnostic ignored "-Wmissing-braces"

// Retail title.cpp's scene set-ups and drawing for the attract movie and the title screen. They
// place op_a's maps, op_b's scenery table, rushmovi's water and fire and op_a's object animations,
// which the port lays out with the host's classes where title.cpp declares PS2-sized ones of its
// own. The globals stay title.cpp's.

void wait_now_loading_vsync();
void set2DSprite(sceVif1Packet *packet, CTexture *texture, const CRect<int> &dst, const CRect<int> &src, u_char alpha);
void set2DSprite(sceVif1Packet *packet, CTexture *texture, const CRect<int> &dst, const CRect<int> &src, int width,
                 int height, float angle);
void DepthOfField(float *dist, int level, int alpha, int blur);
void WaterProcess();

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

// title.cpp's own CFireOmni had this inline; fireomni.hpp's has no counterpart.
static void SetFirePosition(CFireOmni &fire, float x, float y, float z) {
    fire.pos[0] = 10.0f * x;
    fire.pos[1] = 10.0f * y;
    fire.pos[2] = 10.0f * z;
    fire.pos[3] = 1.0f;
}

PC_OVERRIDE void DataLoad() {
    if (CScript.load_no != RUSH_SCENE_NONE) {
    load_wait:
        if (ReadBGSync()) {
            goto load_wait;
        }
    }

    switch (CScript.load_no) {
        case RUSH_SCENE_A: {
            void *buffer = read_buffer;
            LoadFile("rmdat/rmdat1.pak", buffer, 0);
            break;
        }
        case RUSH_SCENE_B:
            LoadFileBG("rmdat/rmdat2.pak", (u_long128 *) read_buffer, 0);
            break;
        case RUSH_SCENE_C:
            LoadFileBG("rmdat/rmdat3.pak", (u_long128 *) read_buffer, 0);
            break;
        case RUSH_SCENE_D:
            LoadFileBG("rmdat/rmdat4.pak", (u_long128 *) read_buffer, 0);
            break;
        case RUSH_SCENE_E:
            LoadFileBG("rmdat/rmdat5.pak", (u_long128 *) read_buffer, 0);
            break;
        case RUSH_SCENE_F:
            LoadFileBG("rmdat/rmdat6.pak", (u_long128 *) read_buffer, 0);
            break;
        case RUSH_SCENE_G:
            LoadFileBG("rmdat/rmdat7.pak", (u_long128 *) read_buffer, 0);
            break;
        case RUSH_SCENE_H:
            LoadFileBG("rmdat/rmdat8.pak", (u_long128 *) read_buffer, 0);
            break;
        case RUSH_SCENE_I:
            LoadFileBG("rmdat/rmdat9.pak", (u_long128 *) read_buffer, 0);
            break;
        case RUSH_SCENE_TITLE:
            LoadFileBG("rmdat/title.pak", (u_long128 *) read_buffer, 0);
            break;
    }

    CScript.load_no = RUSH_SCENE_NONE;

    if (CScript.init_no != RUSH_SCENE_NONE) {
        while (ReadBGSync())
            ;

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
    }

    CScript.init_no = RUSH_SCENE_NONE;
}

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

#ifdef PAL
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

#else
    textures[2].name = (char *) GetPackFile(read_buffer, "start.img", 0);
#endif
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
    Chara__3[0].wind = (int) (intptr_t) &Wind__4;
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

PC_OVERRIDE void DrawProcA() {
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
    static int col = 0;

    if (rand() % 127 == 0) {
        col = 254;
        lightning = 4;

        int se = rand() % 6;

        if (se < 6) {
            SndSePlay(se + 67, -1, 0);
        }
    }

    if (CScript.camera_start == 0) {
        if (Cam[SceneNp].motion_type.state.time > 20.0f && !StartLightning) {
            StartLightning = 1;
            col = 254;
            lightning = 4;
            SndSePlay(SE_THUNDER_CLAP, -1, 0);
        }
    }

    for (int i = 0; i < 4; i++) {
        if (lightcolor[i][0] < (float) col) {
            scene_color[i][0] = (float) col;
        }

        if (lightcolor[i][1] < (float) col) {
            scene_color[i][1] = (float) col;
        }

        if (lightcolor[i][2] < (float) col) {
            scene_color[i][2] = (float) col;
        }

        if (flash[i][0] < (float) col) {
            chara_color[i][0] = (float) col;
        }

        if (flash[i][1] < (float) col) {
            chara_color[i][1] = (float) col;
        }

        if (flash[i][2] < (float) col) {
            chara_color[i][2] = (float) col;
        }

        if (col > 2) {
            col -= 2;
        }
    }

    if (CScript.camera_start == 2) {
        Chara__3[6].SetScale((float) (col - col + 2), 2.0f, 2.0f);
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
        OP_AnimeSeq[OP_AnimeSeqRot - 1].step[0] = (float) (rand() % 10) / 10.0f;
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

        float *fire_scale = &OP_FireScale[i];
        CFire__4.DrawFire(1, 1, &MainCamera__3, eye, *fire_scale, 15, 15.0f);
    }

    TexManager.ReloadTexture(Vif1Packet, 22);

    float dof[3] = {1000.0f, 2000.0f, 3000.0f};

    DepthOfField(dof, 3, 32, 0);
}

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

#ifdef PAL
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

#else
    textures[4].name = (char *) GetPackFile(read_buffer, "start.img", 0);
#endif
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
    Chara__3[0].wind = (int) (intptr_t) &Wind__4;
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
#ifdef PAL
    float zero = 0.0f;
    Water__2.frame.SetPosition(zero, -4.0f, zero);
#else
    typedef float bp0, bp1, bp2;
    Water__2.frame.SetPosition(0.0f, -4.0f, 0.0f);
#endif
    Water__2.SetSize(24, 24, &WaterBuffer);
    Water__2.SetParam(0.1f, 0.015f, 0.0f, 2.0f);
    Water__2.SetColor(100, 110, 120, 128);
}

PC_OVERRIDE void DrawProcB() {
    TexManager.ReloadTexture(Vif1Packet, 10);
    OP_GroundMap.Draw();
    OP_BuildingMap.Draw();

    WaterProcess();

    TexManager.ReloadTexture(Vif1Packet, 23);
    CTexture *texture = TexManager.GetTexture("shadow_buff", -1);

    MGBeginDrawShadow(*(sceGsTex0 *) &texture->tex0);

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

        int frame = (int) Chara__3[0].motion_type.state.time;

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

#ifdef PAL
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

#else
    textures[4].name = (char *) GetPackFile(read_buffer, "start.img", 0);
#endif
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
    Chara__3[0].wind = (int) (intptr_t) &Wind__4;

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

PC_OVERRIDE void DrawProcC() {
    TexManager.ReloadTexture(Vif1Packet, 10);

    for (int i = 0; i < 26; i++) {
        CMapObject *object = &OP_NornMapObj[i];

        object->Draw();
    }

    WaterProcess();

    TexManager.ReloadTexture(Vif1Packet, 23);
    CTexture *texture = TexManager.GetTexture("shadow_buff", -1);

    MGBeginDrawShadow(*(sceGsTex0 *) &texture->tex0);

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

#ifdef PAL
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

#else
    textures[3].name = (char *) GetPackFile(read_buffer, "start.img", 0);
#endif
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
    Chara__3[0].wind = (int) (intptr_t) &Wind__4;

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

PC_OVERRIDE void DrawProcD() {
    TexManager.ReloadTexture(Vif1Packet, 10);
    OP_GroundMap.Draw();
    OP_BuildingMap.Draw();

    TexManager.ReloadTexture(Vif1Packet, 23);
    CTexture *texture = TexManager.GetTexture("shadow_buff", -1);

    MGBeginDrawShadow(*(sceGsTex0 *) &texture->tex0);

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

#ifdef PAL
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

#else
    textures[3].name = (char *) GetPackFile(read_buffer, "start.img", 0);
#endif
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
    Chara__3[0].wind = (int) (intptr_t) &Wind__4;

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

PC_OVERRIDE void DrawProcE() {
    TexManager.ReloadTexture(Vif1Packet, 10);
    OP_GroundMap.Draw();

    if (CScript.camera_start == 10 && Cam[SceneNp].motion_type.state.time >= 70.0f) {
        Chara__3[0].TexAnimeOn(3);
    }

    TexManager.ReloadTexture(Vif1Packet, 23);
    CTexture *texture = TexManager.GetTexture("shadow_buff", -1);

    MGBeginDrawShadow(*(sceGsTex0 *) &texture->tex0);

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

#ifdef PAL
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

#else
    textures[3].name = (char *) GetPackFile(read_buffer, "start.img", 0);
#endif
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
    Chara__3[0].wind = (int) (intptr_t) &Wind__4;
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

PC_OVERRIDE void DrawProcF() {
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

    MGBeginDrawShadow(*(sceGsTex0 *) &texture->tex0);

    for (int i = 0; i < 9; i++) {
        if (i != 5) {
            if (CScript.obj[i].disp) {
                Chara__3[i].ShadowStep();
                Chara__3[i].DrawShadow();
            }
        }
    }

    MGEndDrawShadow(52);

    for (int i = 0; i < 9; i++) {
        if (CScript.obj[i].disp) {
            if (i != 5) {
                TexManager.ReloadTexture(Vif1Packet, CharaTex[i]);
                Chara__3[i].TextureAnime(CharaTex[i]);
                Chara__3[i].Step();
                Chara__3[i].ClothStep(0);
                Chara__3[i].Draw();
            }
        }
    }

    TexManager.ReloadTexture(Vif1Packet, 22);

    float dof[2] = {400.0f, 1000.0f};

    DepthOfField(dof, 2, 32, 0);
}

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

#ifdef PAL
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

#else
    textures[3].name = (char *) GetPackFile(read_buffer, "start.img", 0);
#endif
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
    Chara__3[0].wind = (int) (intptr_t) &Wind__4;
    Chara__3[1].wind = (int) (intptr_t) &Wind__4;
    Chara__3[2].wind = (int) (intptr_t) &Wind__4;

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

PC_OVERRIDE void DrawProcG() {
    TexManager.ReloadTexture(Vif1Packet, 10);
    OP_GroundMap.Draw();
    OP_BuildingMap.Draw();

    TexManager.ReloadTexture(Vif1Packet, 23);
    CTexture *texture = TexManager.GetTexture("shadow_buff", -1);

    MGBeginDrawShadow(*(sceGsTex0 *) &texture->tex0);

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

#ifdef PAL
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

#else
    textures[3].name = (char *) GetPackFile(read_buffer, "start.img", 0);
#endif
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
    Chara__3[1].wind = (int) (intptr_t) &Wind__4;

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

PC_OVERRIDE void DrawProcH() {
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

    MGBeginDrawShadow(*(sceGsTex0 *) &texture->tex0);

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

#ifdef PAL
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

#else
    textures[3].name = (char *) GetPackFile(read_buffer, "start.img", 0);
#endif
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
    Chara__3[0].wind = (int) (intptr_t) &Wind__4;
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

PC_OVERRIDE void DrawProcI() {
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

        MGBeginDrawShadow(*(sceGsTex0 *) &texture->tex0);
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

PC_OVERRIDE void DrawProcTitle() {
    TexManager.ReloadTexture(Vif1Packet, 1);

    TitlePortCard(TexManager.GetTexture("bg01", -1), TitleAngle);
    TitleAngle -= 0.0005f;

    set2DSprite(GetVif1Packet(), TexManager.GetTexture("dc01", -1), CRect<int>(0, 80, 288, 160), CRect<int>(0, 0, 288, 160), 128);
    set2DSprite(GetVif1Packet(), TexManager.GetTexture("dc01", -1), CRect<int>(288, 129, 352, 160), CRect<int>(288, 49, 352, 160), 128);
    set2DSprite(GetVif1Packet(), TexManager.GetTexture("dc01", -1), CRect<int>(0, 366, 640, 48), CRect<int>(0, 208, 640, 48), TitleFade);

    TitleFadeCnt++;

    if (TitleFadeCnt >= 60) {
        TitleFade++;

        if (TitleFade >= 128) {
            TitleFade = 128;
        }
    }

    TexManager.ReloadTexture(Vif1Packet, 22);

    float dof[3] = {1000.0f, 2000.0f, 3000.0f};
    DepthOfField(dof, 3, 32, 0);
}
