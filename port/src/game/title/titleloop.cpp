/* The title screen's own loop. Retail compiles it apart from the rest of the
   title unit: its constants are a run of their own, which is why the three
   names it shares with the code before it are spelled twice in the image. */

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

void MGStretchMoveImage(sceGsTex0 *src, const CRect<int> &src_rect, sceGsTex0 *dst, const CRect<int> &dst_rect);

/* The rectangle DrawObjectVibe takes by value. It is four ints and not a CRect: the two are the
   same fields and the name the call encodes is this one. */
struct RECT {
    int x; /**< Left edge. */
    int y; /**< Top edge. */
    int w; /**< Width. */
    int h; /**< Height. */
};

void InitOpeningBook(u_long128 *pack, int *param);
int  OpeningBookKey();
void OpeningBookDraw();

#include "editloop.hpp"
#include "gamemode.hpp"
#include "main.hpp"
#include "memcard.hpp"
#include "menu_save.hpp"
#include "mglib.hpp"
#include "object.hpp"
#include "texture.hpp"
#include "title/dispfade.hpp"
#include "title/op_a.hpp"
#include "title/op_b.hpp"
#include "title/op_c.hpp"
#include "title/rushmovi.hpp"
#include "title/script.hpp"
#include "title/title.hpp"
#include "title/titleloop.hpp"
#include "vutext.hpp"
#include "mainselect.hpp"
#include "wind.hpp"

/* The classes this movie places in the world, declared here rather than reached through headers of
   their own because each is another unit's to type. Only the members this file touches are named;
   the extents are the sizes the executable gives the objects below. */

/* The rippling water plane the outdoor scenes stand on. The frame is where the plane sits in the
   world. */
class CWater {
public:
    char      unk_00[176];
    CFrameVu1 frame; /**< Places and draws the water surface. */

    CWater();

    void SetVertex(float *corner0, float *corner1, float *corner2, float *corner3);
    void SetSize(int row_count, int column_count, CDataAlloc2<1> *arena);
    void SetParam(float speed, float damping_rate, float scale, float shift);
    void SetColor(u_char red, u_char green, u_char blue, u_char alpha);
    void Shake(int x, int y, float height_change);
    void Hamon();
    int  DrawVu1(RenderInfo *info, sceVif1Packet *packet, u_long128 *parent_info);
};

/* Named rather than included, because a unit's include list is a dial on the order a call's
   floating-point arguments are set up in and nothing here needs the definition: adding
   renderinfo.h alone takes RushInit's three-float SetFollow out of the order the image has. */

/* A frame parented to an object, which is what lets the world transform drive a model. */
class CObjectFrame : public CObject {
public:
    virtual void FrameObjectOnOff(char *name, int on);
    virtual void Draw();

    void SetFrame(CFrameVu1 *frame, int level);
};

/* One piece of scenery. The movie builds a table of them, hands each its model, and drives them
   through the object dispatch like anything else in the world. */
class CMapObject : public CObjectFrame {
public:
    char       unk_18[36];
    CFrameVu1 *unk_D4;
    char       unk_4C[8];
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

void InitializeDataBuffer();
void set2DSprite(sceVif1Packet *packet, CTexture *texture, const CRect<int> &dst, const CRect<int> &src, u_char alpha);
void set2DSprite(sceVif1Packet *packet, CTexture *texture, const CRect<int> &dst, const CRect<int> &src, u_char r, u_char g, u_char b, u_char a);

void TiPlayVolSE(int group, int no, int voice, float volume);

int    Fade1;
int    Fade2;
int    Fade3;
int    Fade4;
int    Wait;
int    opcnt;
int    keywait;
u_char brink;
int    brinkcnt;
int    EffCnt;

void TitleInit(int mode) {
    int   i;
    float scale = 750.0f;
    float far_z = 65535.0f;

    InitializeDataBuffer();
    SetDataBuffer(&VisualData, 200000);
    SetDataBuffer(&MotionData, 500000);
    SetDataBuffer(&TextureData, 300000);
    SetPacketReadBuffer(40000, 300000);

    MGSetRenderInfo(scale, 4.0f, far_z);
    Camera.SetRef(0, 0.0f, 0.0f, 0.0f);
    /* One of these five calls is written from a named zero and the rest are not, and where
       the name is declared is what it is for: this compiler evaluates a call's constant
       arguments in an order set by the declarations standing above the call. */
    float zero = 0.0f;
    Camera.SetPos(0, zero, zero, zero);
    FCamera.SetFollow(0.0f, 0.0f, 0.0f);
    FCamera.SetAngle(0.0f);
    FCamera.SetDistance(180.0f);
    FCamera.SetHeight(2.0f);
    MGSetBGColor(0.0f, 0.0f, 0.0f, 128.0f);
    setbilinear(1);

    LOADTEXTURE_INFO tex[] = {
        {"#fukidashibase#640#" HALF_BUFFER_HEIGHT_STR "#4", 26, 0},
        {"#fontbase#512#256#1",                             26, 0},
        {"meswin/gaiji.img",                                26, 0},
        {"meswin/fuki256.img",                              26, 0},
        {"meswin/syst04.img",                               26, 0},
        {"",                                                0,  0}
    };

    TexManager.Initialize(16352);
    TexManager.LoadTextureBlock(-1, tex, read_buffer);
    LoadFileMenuData("stayframe.img", (u_int *) read_buffer);
    TexManager.EnterFixTextureZ((u_char *) read_buffer);

    switch (LanguageCode) {
        case LANG_JAPANESE:
            LoadFile("titledat/title.pak", (void *) read_buffer, 0);
            break;
        case LANG_ENGLISH_US:
            LoadFile("titledat/title.pak", (void *) read_buffer, 0);
            break;
        case LANG_ENGLISH_UK:
            LoadFile("titledat/title_eu.pak", (void *) read_buffer, 0);
            break;
        case LANG_FRENCH:
            LoadFile("titledat/title_f.pak", (void *) read_buffer, 0);
            break;
        case LANG_GERMAN:
            LoadFile("titledat/title_g.pak", (void *) read_buffer, 0);
            break;
        case LANG_ITALIAN:
            LoadFile("titledat/title_i.pak", (void *) read_buffer, 0);
            break;
        case LANG_SPANISH:
            LoadFile("titledat/title_s.pak", (void *) read_buffer, 0);
            break;
    }

    LOADTEXTURE_INFO2 tex2[] = {
        {(char *) "#frame_image#640#" HALF_BUFFER_HEIGHT_STR "#4", 0, 0},
        {(char *) "#frame_image#640#" HALF_BUFFER_HEIGHT_STR "#4", 1, 0},
        {0,                                                        0, 0},
        {0,                                                        1, 0},
        {0,                                                        1, 0},
        {0,                                                        1, 0},
        {0,                                                        1, 0},
        {0,                                                        1, 0},
        {0,                                                        1, 0},
        {0,                                                        1, 0},
        {0,                                                        0, 0}
    };

    tex2[2].name = (char *) GetPackFile(read_buffer, "bg.img", 0);
    tex2[3].name = (char *) GetPackFile(read_buffer, "title.img", 0);
    tex2[4].name = (char *) GetPackFile(read_buffer, "main.img", 0);
    tex2[5].name = (char *) GetPackFile(read_buffer, "pat01.img", 0);
    tex2[6].name = (char *) GetPackFile(read_buffer, "pat02.img", 0);
    tex2[7].name = (char *) GetPackFile(read_buffer, "start.img", 0);
    tex2[8].name = (char *) GetPackFile(read_buffer, "start3.img", 0);
    tex2[9].name = (char *) GetPackFile(read_buffer, "icon01.img", 0);
    TexManager.LoadTextureBlock(-1, tex2);

    sceVu0FVECTOR pos;

    ObjectFrame3 = LoadMDSFile(GetPackFile(read_buffer, "sky.mds", 0), 2, 0);
    pos[0] = 0.0f;
    pos[1] = 0.0f;
    pos[2] = -28.0f;
    ObjectFrame3->SetPosition(pos);

    Cloud__2.LoadPackData(read_buffer, "cloud.cfg", &MotionData, 0);
    Cloud__2.motion_type.state.time = 100.0f;
    Cloud__2.motion_type.state.blend_step = 0.1f;
    Cloud__2.motion_type.state.motion_no = 0;
    Cloud__2.motion_type.state.playing_no = 0;
    Cloud__2.motion_type.state.blending = false;
    Cloud__2.motion_no = 0;
    Cloud__2.SetPosition(0.0f, 0.0f, -5.0f);

    Logo.LoadPackData(read_buffer, "logo.cfg", &MotionData, 0);
    Logo.motion_type.state.time = 1.0f;
    Logo.motion_type.state.blend_step = 0.1f;
    Logo.motion_type.state.motion_no = 0;
    Logo.motion_type.state.playing_no = 0;
    Logo.motion_type.state.blending = false;
    Logo.motion_no = 0;

    char *name[9] = {
        "logo_p1.cfg", "logo_p2.cfg", "logo_p3.cfg", "logo_p4.cfg", "logo_p5.cfg",
        "logo_p6.cfg", "logo_p7.cfg", "logo_p8.cfg", "logo_p9.cfg"};

    for (i = 0; i < 9; i++) {
        Spark[i].LoadPackData(read_buffer, name[i], &MotionData, 0);
        Spark[i].motion_type.state.time = 1.0f;
        Spark[i].motion_type.state.blend_step = 0.1f;
        Spark[i].motion_type.state.motion_no = 0;
        Spark[i].motion_type.state.playing_no = 0;
        Spark[i].motion_type.state.blending = false;
        Spark[i].motion_no = 0;
        Spark[i].SetPosition(0.0f, 11.599f, -19.099f);
    }

    CSnd.SetReverb(0, 4, 5);
    CSnd.SetReverb(1, 0, 0);
    CSnd.LoadSoundFileFromPack("title.txt", read_buffer);
    CSnd.SetVol(MIDI_PORT_SE_TITLE, 256);
    CSnd.SetVol(MIDI_PORT_SE_DEFAULT, 256);
    CSnd.SetVol(MIDI_PORT_UNK_D, 256);
    CSnd.SetVol(MIDI_PORT_SE_SPECIAL, 256);
    CSnd.SQ_Play(MIDI_PORT_AMBIENT, 0);

    Fade1 = Fade2 = Fade3 = Fade4 = Wait = 0;
    opcnt = 0;
    keywait = 0;
    brinkcnt = 0;
    brink = 0;
    EffCnt = 0;
    CProcess.no = TITLE_STEP_FADE_IN;
    CFade.value = 0;

    CSprite.Init();
    CLogo.Init();
    CCursol.Init();

    if (mode == 1) {
        CCursol.select = TITLE_MENU_LOAD;
        CCursol.Set(316.0f);

        for (i = 0; i < 10; i++) {
            CCursol.Move();
        }
    }

    GamePad.SetAutoRepeat(PAD_UP | PAD_DOWN, 30, 9);
    GamePad.MenuModeOn(120);
}

int TitleLoop();

/* A title menu row's height on screen, laid out for NTSC's picture and lowered to stay centred in
   the taller PAL one. */
#define MENU_Y(y) ((y) + (SCREEN_HEIGHT - 448) / 2)

void TitleDraw();

