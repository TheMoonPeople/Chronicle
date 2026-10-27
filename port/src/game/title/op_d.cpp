#include "common.h"

#include <libgraph.h>
#include <libpkt.h>
#include <libvu0.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <span>

#include "camera.hpp"
#include "camerafollow.hpp"
#include "character.hpp"
#include "dataalloc.hpp"
#include "dataread.hpp"
#include "effectmacro.hpp"
#include "frame.hpp"
#include "framevu1.hpp"
#include "main.hpp"
#include "mapobject.hpp"
#include "mathutil.hpp"
#include "mds.hpp"
#include "mdt.hpp"
#include "mglib.hpp"
#include "renderinfo.hpp"
#include "snd.hpp"
#include "sound.hpp"
#include "texture.hpp"
#include "title/op_a.hpp"
#include "title/op_b.hpp"
#include "title/op_d.hpp"
#include "title/opening.hpp"
#include "title/rushmovi.hpp"
#include "title/script.hpp"
#include "title/seireiking.hpp"
#include "visual.hpp"
#include "wind.hpp"

namespace dc::game {

// rect.h's four-argument constructor assigns the fields in the opposite order to this one.
template <class T>
class CRect {
public:
    T x; /**< Left edge. */
    T y; /**< Top edge. */
    T w; /**< Width. */
    T h; /**< Height. */

    CRect(T left, T top, T width, T height) : x(left), y(top), w(width), h(height) {}
} __attribute__((aligned(16)));

void set2DSprite(sceVif1Packet *packet, CTexture *texture, const CRect<int> &src, const CRect<int> &dst, u_char alpha);
void MoveImageTest(sceVif1Packet *packet, int sbp, int sbw, int spsm, const CRect<int> &rect, int dbp, int dbw,
                   int dpsm, int dsax, int dsay, int dir);

namespace {

struct FACE_INFO {
    const char *plate;
    const char *strip;
    int         eye_bottom;
    int         eye_height;
    int         mouth_bottom;
    int         mouth_height;
    int         eye;
    int         mouth;
    int         strip_bottom;
    int         unk_24;
    int         unk_28;
};

struct MAPOBJ_INFO {
    char *name;
    char *shadow_name;
    float position[3];
    float rotation[3];
};

/**
 * Sprite cues the script raises during the spirit king and meadow scenes, as CScript::sprite holds them.
 */
enum class OpDSprite : int {
    None = 0,       /**< No cue. */
    HallLight = 1,  /**< Raise the hall's ambient light. */
    RuinStill1 = 2, /**< First still of the ruined village. */
    RuinStill2 = 3, /**< Second still of the ruined village. */
    RuinStill3 = 4, /**< Third still of the ruined village. */
    Caption1 = 5,   /**< First explanation caption. */
    Caption2 = 6,   /**< Second explanation caption. */
};

OpDSprite CurrentSprite() {
    return static_cast<OpDSprite>(CScript__2.sprite);
}

// init_priority keeps this static's place in the order rand() is drawn from; see app/runtime.cpp.
CWind Wind __attribute__((init_priority(65002)));

// A rising star turns towards its target above the screen a step at a time and restarts from the
// bottom once it leaves the top.
void DrawRisingStar(float &x, float &y, float &rot, float target_x, float start_x) {
    const float heading = atan2f(target_x - x, -107.0f - y);

    if (heading < rot) {
        rot = rot - 0.05f;
    }

    if (heading > rot) {
        rot += 0.05f;
    }

    x = x + static_cast<float>(sin(rot) * 4.0);
    y = y + static_cast<float>(cos(rot) * 4.0);

    const int alpha = y < 32.0f ? std::max(static_cast<int>(128.0f - 4.0f * (32.0f - y)), 0) : 128;

    if (y <= 0.0f) {
        x = start_x;
        y = 245.0f;
    }

    const int cut = static_cast<int>((245.0f - y) / 2.0f);

    set2DSprite(GetVif1Packet(), TexManager.GetTexture("0519p", -1), CRect<int>(static_cast<int>(x), static_cast<int>(y), 178 - cut, 191 - cut),
                CRect<int>(210, 0, 178, 178), static_cast<u_char>(alpha));
}

// A falling star heads straight for its target and restarts from its start once it reaches it.
void DrawFallingStar(float &x, float &y, float target_x, float start_x, float start_y) {
    const float heading = atan2f(target_x - x, 193.0f - y);

    x = x + static_cast<float>(sin(heading) * 3.0);
    y = y + static_cast<float>(cos(heading) * 3.0);

    const int alpha = y > 148.0f ? std::max(static_cast<int>(128.0f - 4.0f * (y - 159.0f)), 0) : 128;

    if (y >= 191.0f) {
        x = start_x;
        y = start_y;
    }

    set2DSprite(GetVif1Packet(), TexManager.GetTexture("0519p", -1), CRect<int>(static_cast<int>(x), static_cast<int>(y), 210, 221),
                CRect<int>(0, 0, 210, 206), static_cast<u_char>(alpha));
}

} // namespace

static void SkyColor(CFrameVu1 *frame);
static void EffectAtraPrizum();
static void RollLight(float *target);
static void EffectSeireiKing(float size);
static void LensFreaProcess();
static void Setsumei();
static void HamonProcess();

static CFrameVu1  *Hamon[4];
static float       HScale[5];
static CCharacter  Effect;
static CSeireiKing SeireiKing;

static int        amb3;
static CFrameVu1 *SkyFrame;

// Copies the actor's current eye and mouth rows from its strip texture onto its face plate.
void FaceChangeD(int actor_no) {
    static std::array<FACE_INFO, 21> face = {
        {{nullptr, nullptr, 42, 40, 87, 35, 0, 0, 256, 2, 0},
         {nullptr, nullptr, 42, 40, 87, 35, 0, 0, 320, 2, 0},
         {nullptr, nullptr, 32, 40, 84, 35, 0, 0, 448, 3, 0},
         {"c09a01", "c09a01an", 10, 40, 73, 35, 0, 0, 448, 2, 0},
         {nullptr, nullptr, 42, 40, 87, 35, 0, 0, 320, 2, 0},
         {nullptr, nullptr, 42, 40, 87, 35, 0, 0, 320, 2, 0},
         {nullptr, nullptr, 8, 40, 76, 35, 0, 0, 256, 2, 0},
         {nullptr, nullptr, 42, 40, 87, 35, 0, 0, 320, 2, 0},
         {nullptr, nullptr, 42, 40, 87, 35, 0, 0, 320, 2, 0},
         {"p09a01", "p09a01an_3", 44, 40, 92, 32, 0, 0, 192, 2, 0},
         {nullptr, nullptr, 42, 40, 87, 35, 0, 0, 320, 2, 0},
         {"c01d01", "c01d01an_3", 27, 48, 78, 44, 0, 0, 512, 3, 0},
         {nullptr, nullptr, 42, 40, 87, 35, 0, 0, 320, 2, 0},
         {nullptr, nullptr, 42, 40, 87, 35, 0, 0, 320, 2, 0},
         {nullptr, nullptr, 42, 40, 87, 35, 0, 0, 320, 2, 0},
         {nullptr, nullptr, 42, 40, 87, 35, 0, 0, 320, 2, 0},
         {nullptr, nullptr, 42, 40, 87, 35, 0, 0, 320, 2, 0},
         {nullptr, nullptr, 42, 40, 87, 35, 0, 0, 320, 2, 0},
         {nullptr, nullptr, 42, 40, 87, 35, 0, 0, 320, 2, 0},
         {nullptr, nullptr, 42, 40, 87, 35, 0, 0, 320, 2, 0},
         {nullptr, nullptr, 42, 40, 87, 35, 0, 0, 320, 2, 0}}
    };

    if (actor_no < 0 || actor_no >= std::ssize(face)) {
        return;
    }

    FACE_INFO &info = face[actor_no];

    if (info.plate == nullptr) {
        return;
    }

    CTexture *plate = TexManager.GetTexture(const_cast<char *>(info.plate), -1);
    CTexture *strip = TexManager.GetTexture(const_cast<char *>(info.strip), -1);

    if (plate == nullptr || strip == nullptr) {
        return;
    }

    // TEX0 TBP0 (bits 0-13) and TBW (bits 14-19).
    int sbp = strip->tex0 & 0x3fff;
    int dbp = plate->tex0 & 0x3fff;
    int sbw = (strip->tex0 >> 14) & 0x3f;
    int dbw = (plate->tex0 >> 14) & 0x3f;

    info.eye = CScript__2.obj[actor_no].eye;

    CRect<int> eye(0, info.strip_bottom - info.eye_height * (info.eye + 1), 128, info.eye_height);

    MoveImageTest(Vif1Packet, sbp, sbw, SCE_GS_PSMT8, eye, dbp, dbw, SCE_GS_PSMT8, 0, 128 - info.eye_height - info.eye_bottom, 0);

    if (!Pause) {
        if (CScript__2.obj[actor_no].mouth_time >= CScript__2.motion_step) {
            CScript__2.obj[actor_no].mouth_time -= CScript__2.motion_step;

            if (CScript__2.obj[actor_no].talk && static_cast<int>(100.0f * CScript__2.obj[actor_no].mouth_time) % 6 == 0) {
                CScript__2.obj[actor_no].mouth = rand() % 4;
            }
        } else {
            CScript__2.obj[actor_no].mouth = 0;
            CScript__2.obj[actor_no].talk = false;
        }
    }

    info.mouth = CScript__2.obj[actor_no].mouth;

    CRect<int> mouth(128, info.strip_bottom - info.mouth_height * (info.mouth + 1), 128, info.mouth_height);

    MoveImageTest(Vif1Packet, sbp, sbw, SCE_GS_PSMT8, mouth, dbp, dbw, SCE_GS_PSMT8, 0, 128 - info.mouth_height - info.mouth_bottom, 0);
}

void OpD_LoadDataBG() {
    while (ReadBGSync()) {
    }

    LoadFileBG("opdat/norn4/seirei.pak", reinterpret_cast<u_long128 *>(read_buffer), nullptr);
    CScript__2.load_no = OP_SCENE_NONE;
}

// The music bank is packed behind the scene in the same file.
void OpD_LoadDataBG2() {
    int size = 0;

    while (ReadBGSync()) {
    }

    LoadFileBG("opdat/norn4/seirei2.pak", reinterpret_cast<u_long128 *>(read_buffer), &size);
    SndBgmLoadBG(0, reinterpret_cast<u_int *>(reinterpret_cast<u_long128 *>(read_buffer) + (size >> 4) + 1), nullptr);
    CScript__2.load_no = OP_SCENE_NONE;
}

void OpD_InitProcess() {
    while (ReadBGSync()) {
    }

    for (int i = 0; i < 17; i++) {
        TexManager.DeleteTextureBlock(i);
    }

    TexManager.CleanUpBuffer();

    LOADTEXTURE_INFO2 tex[] = {
        {"#blender#640#" HALF_BUFFER_HEIGHT_STR "#4", 0,  0},
        {nullptr,                                     11, 0},
        {nullptr,                                     11, 0},
        {nullptr,                                     4,  0},
        {nullptr,                                     4,  0},
        {nullptr,                                     4,  0},
        {nullptr,                                     4,  0},
        {nullptr,                                     4,  0},
        {nullptr,                                     0,  0}
    };

    tex[1].name = reinterpret_cast<char *>(GetPackFile(read_buffer, "b0401.img", nullptr));
    tex[2].name = reinterpret_cast<char *>(GetPackFile(read_buffer, "b0402.img", nullptr));
    tex[3].name = reinterpret_cast<char *>(GetPackFile(read_buffer, "eef01.img", nullptr));
    tex[4].name = reinterpret_cast<char *>(GetPackFile(read_buffer, "eef02.img", nullptr));
    tex[5].name = reinterpret_cast<char *>(GetPackFile(read_buffer, "eef03.img", nullptr));
    tex[6].name = reinterpret_cast<char *>(GetPackFile(read_buffer, "eef04.img", nullptr));
    tex[7].name = reinterpret_cast<char *>(GetPackFile(read_buffer, "eef05.img", nullptr));

    TexManager.LoadTextureBlock(-1, tex);

    tex[0].name = reinterpret_cast<char *>(GetPackFile(read_buffer, "c01d01.img", nullptr));
    tex[0].block_no = 1;
    tex[0].mipmap = false;
    tex[1].name = reinterpret_cast<char *>(GetPackFile(read_buffer, "c01d01an.img", nullptr));
    tex[1].block_no = 1;
    tex[1].mipmap = false;
    tex[2].name = nullptr;

    TexManager.LoadTextureBlock(1, tex);

    tex[0].name = reinterpret_cast<char *>(GetPackFile(read_buffer, "c03c01.img", nullptr));
    tex[0].block_no = 2;
    tex[0].mipmap = false;
    tex[1].name = nullptr;

    TexManager.LoadTextureBlock(2, tex);
    MapDataBuffer.used = 0;

    CFrameVu1  *frame = LoadMDSFile(GetPackFile(read_buffer, "b0401.mds", nullptr), &MapDataBuffer, 2, nullptr, nullptr);
    CMapObject &hall = OP_NornMapObj[0];

    hall.Initialize();
    hall.SetFrame(frame, 0);
    OP_NornMapObj[0].handle = 0;
    OP_NornMapObj[0].category_no = 0;
    hall.SetPosition(CVector3_f_(0.0f, 0.0f, 0.0f));
    hall.SetRotation(CVector3_f_(0.0f, 0.0f, 0.0f));
    Hamon[0] = LoadMDSFile(GetPackFile(read_buffer, "b0402.mds", nullptr), &CharaDataBuffer__2[6], 2, nullptr, nullptr);
    Hamon[1] = Hamon[2] = Hamon[3] = Hamon[0];
    HScale[0] = 0.0f;
    HScale[1] = -0.8f;
    HScale[2] = -1.6f;
    HScale[3] = -2.4f;
    HScale[4] = -3.2f;
    OP_FireList = 0;
    OPAnalyz("opdat/seirei.cfg");
    OPMdsLoad();
    CharaDataBuffer__2[6].used = 0;
    Chara__3[8].LoadPackData(read_buffer, "05c01d.cfg", &CharaDataBuffer__2[6], nullptr);

    CFrameAttr attr8;

    attr8.clip_enable = false;
    Chara__3[8].frame->SetAttr(attr8, 1, 4);
    Chara__3[8].motion_type.state.time = 60.0f;
    Chara__3[8].motion_type.state.blend_step = 0.05f;
    Chara__3[8].motion_type.state.motion_no = 0;
    Chara__3[8].motion_type.state.playing_no = 0;
    CharaTex__2[8] = 1;
    Chara__3[11].LoadPackData(read_buffer, "05c01e.cfg", &CharaDataBuffer__2[6], nullptr);
    attr8.clip_enable = false;
    Chara__3[11].frame->SetAttr(attr8, 1, 4);
    Chara__3[11].motion_type.state.time = 75.0f;
    Chara__3[11].motion_type.state.blend_step = 0.05f;
    Chara__3[11].motion_type.state.motion_no = 1;
    Chara__3[11].motion_type.state.playing_no = 1;
    CharaTex__2[11] = 1;
    Chara__3[21].LoadPackData(read_buffer, "05c03c.cfg", &CharaDataBuffer__2[6], nullptr);

    CFrameAttr attr21;

    attr21.clip_enable = false;
    Chara__3[21].frame->SetAttr(attr21, 1, 4);
    Chara__3[21].motion_type.state.time = 1.0f;
    Chara__3[21].motion_type.state.blend_step = 0.05f;
    Chara__3[21].motion_type.state.motion_no = 0;
    Chara__3[21].motion_type.state.playing_no = 0;
    CharaTex__2[21] = 2;
    Effect.LoadPackData(read_buffer, "atrpeff1.cfg", &CharaDataBuffer__2[6], nullptr);

    CFrameAttr effect_attr;

    effect_attr.clip_enable = false;
    Effect.frame->SetAttr(effect_attr, 1, 4);
    Effect.motion_type.state.time = 1.0f;
    Effect.motion_type.state.blend_step = 1.0f;
    Effect.motion_type.state.motion_no = 0;
    Effect.motion_type.state.playing_no = 0;
    CSnd.SetReverb(0, 4, 50);
    CSnd.SetReverb(1, 2, 5);
    CSnd.LoadSoundFileFromPack("o04a.txt", read_buffer);
    CSnd.SetVol(MIDI_PORT_SE_TITLE, 256);
    CSnd.SetVol(MIDI_PORT_SE_DEFAULT, 256);
    CSnd.SetVol(MIDI_PORT_UNK_D, 256);
    CSnd.SetVol(MIDI_PORT_SE_SPECIAL, 256);

    LOADTEXTURE_INFO img[] = {
        {"opdat/norn4/i00002.img", 5, 0},
        {"opdat/norn4/i00006.img", 5, 0},
        {"opdat/norn4/i00022.img", 6, 0},
        {"opdat/norn4/0519.img",   7, 0},
        {"opdat/norn4/0519p.img",  7, 0},
        {nullptr,                  0, 0},
        {nullptr,                  0, 0},
        {nullptr,                  0, 0},
        {nullptr,                  0, 0},
        {nullptr,                  0, 0},
        {nullptr,                  0, 0},
        {nullptr,                  0, 0},
        {nullptr,                  0, 0},
        {nullptr,                  0, 0},
        {nullptr,                  0, 0}
    };

    TexManager.LoadTextureBlock(-1, img, read_buffer);
    SeireiKing.Initialize();
    amb3 = 0;
    CScript__2.init_no = 0;
}

void OpD_InitProcess2() {
    while (ReadBGSync()) {
    }

    LOADTEXTURE_INFO2 tex[] = {
        {nullptr, 10, 0},
        {nullptr, 10, 0},
        {nullptr, 10, 0},
        {nullptr, 10, 0},
        {nullptr, 10, 0},
        {nullptr, 10, 0},
        {nullptr, 3,  0},
        {nullptr, 10, 0},
        {nullptr, 0,  0}
    };

    tex[0].name = reinterpret_cast<char *>(GetPackFile(read_buffer, "b0403.img", nullptr));
    tex[1].name = reinterpret_cast<char *>(GetPackFile(read_buffer, "e01b01.img", nullptr));
    tex[2].name = reinterpret_cast<char *>(GetPackFile(read_buffer, "t0401.img", nullptr));
    tex[3].name = reinterpret_cast<char *>(GetPackFile(read_buffer, "e01s01.img", nullptr));
    tex[4].name = reinterpret_cast<char *>(GetPackFile(read_buffer, "e01s06.img", nullptr));
    tex[5].name = reinterpret_cast<char *>(GetPackFile(read_buffer, "t0402.img", nullptr));
    tex[6].name = reinterpret_cast<char *>(GetPackFile(read_buffer, "buterfly.img", nullptr));
    tex[7].name = reinterpret_cast<char *>(GetPackFile(read_buffer, "lensfler.img", nullptr));

    TexManager.LoadTextureBlock(-1, tex);

    MAPOBJ_INFO map[] = {
        {"b0403.mds",    nullptr, {0.0f, 0.0f, 0.0f},            {0.0f, 0.0f, 0.0f}},
        {"e01g02_0.mds", nullptr, {0.0f, 0.0f, 0.0f},            {0.0f, 0.0f, 0.0f}},
        {"e01g03_0.mds", nullptr, {0.0f, 0.0f, 0.0f},            {0.0f, 0.0f, 0.0f}},
        {"e01g04_0.mds", nullptr, {0.0f, 0.0f, 0.0f},            {0.0f, 0.0f, 0.0f}},
        {"e01h06_2.mds", nullptr, {0.0f, 4.7424f, 79.274f},      {0.0f, 0.0f, 0.0f}},
        {"e01a02_2.mds", nullptr, {0.0f, 0.0f, 59.965f},         {0.0f, 0.0f, 0.0f}},
        {"flower.mds",   nullptr, {0.0f, 0.0f, 0.0f},            {0.0f, 0.0f, 0.0f}},
        {"e01s05_0.mds", nullptr, {0.0f, 0.0f, 0.0f},            {0.0f, 0.0f, 0.0f}},
        {"e01s01_0.mds", nullptr, {0.0f, 0.0f, 0.0f},            {0.0f, 0.0f, 0.0f}},
        {"e01s01_1.mds", nullptr, {104.195f, 3.399f, -150.395f}, {0.0f, 0.0f, 0.0f}}
    };
    CFrameAttr attr;

    attr.fog_enable = true;

    for (int i = 1; i < 11; i++) {
        CFrameVu1 *frame = LoadMDSFile(GetPackFile(read_buffer, map[i - 1].name, nullptr), &MapDataBuffer, 2, nullptr, nullptr);

        if (i == 7) {
            attr.use_color = true;
            frame->SetAttr(attr, 512, 0);
        }

        if (i < 8) {
            frame->SetAttr(attr, 1, 64);
            SetFrameAttr(frame, 1);
        }

        if (i == 8) {
            SkyColor(frame);
        }

        if (i == 10) {
            SkyFrame = frame;
        }

        CMapObject &object = OP_NornMapObj[i];

        object.Initialize();
        object.SetFrame(frame, 0);
        OP_NornMapObj[i].handle = 0;
        OP_NornMapObj[i].category_no = 0;

        if (i == 7) {
            object.FrameObjectOnOff("flo_S", 0);
        }

        object.SetPosition(CVector3_f_(10.0f * map[i - 1].position[0], 10.0f * map[i - 1].position[1], 10.0f * map[i - 1].position[2]));
        object.SetRotation(CVector3_f_(static_cast<float>(PI_D * map[i - 1].rotation[0] / 180), static_cast<float>(PI_D * map[i - 1].rotation[1] / 180), static_cast<float>(PI_D * map[i - 1].rotation[2] / 180)));
    }

    Chara__3[22].LoadPackData(read_buffer, "buterfly.cfg", &CharaDataBuffer__2[6], nullptr);

    CFrameAttr attr22;

    attr22.clip_enable = false;
    Chara__3[22].frame->SetAttr(attr22, 1, 4);
    Chara__3[22].motion_type.state.time = 1.0f;
    Chara__3[22].motion_type.state.blend_step = 0.05f;
    Chara__3[22].motion_type.state.motion_no = 0;
    Chara__3[22].motion_type.state.playing_no = 0;
    CharaTex__2[22] = 3;
    Chara__3[5].LoadPackData(read_buffer, "buterfly.cfg", &CharaDataBuffer__2[6], nullptr);
    attr22.clip_enable = false;
    Chara__3[5].frame->SetAttr(attr22, 1, 4);
    Chara__3[5].motion_type.state.time = 1.0f;
    Chara__3[5].motion_type.state.blend_step = 0.05f;
    Chara__3[5].motion_type.state.motion_no = 0;
    Chara__3[5].motion_type.state.playing_no = 0;
    CharaTex__2[5] = 3;
    CSnd.SetReverb(0, 4, 30);
    CSnd.SetReverb(1, 2, 5);
    CSnd.LoadSoundFileFromPack("o04b.txt", read_buffer);
    CSnd.SetVol(MIDI_PORT_SE_TITLE, 256);
    CSnd.SetVol(MIDI_PORT_SE_DEFAULT, 256);
    CSnd.SetVol(MIDI_PORT_UNK_D, 256);
    CSnd.SetVol(MIDI_PORT_SE_SPECIAL, 256);
    CScript__2.init_no = 0;
}

// The dome is thirty-two vertices around; the two colours alternate two vertices at a time.
static void SkyColor(CFrameVu1 *frame) {
    if (frame == nullptr) {
        return;
    }

    CVisualVu1 *visual = frame->GetVisual();

    if (visual == nullptr) {
        return;
    }

    auto *model = reinterpret_cast<MDT_HEADER *>(visual->GetMDTDataAddress());

    if (model == nullptr || model->colour_count == 0) {
        return;
    }

    constexpr int vertex_count = 32;

    std::span     colors{reinterpret_cast<sceVu0FVECTOR *>(reinterpret_cast<char *>(model) + model->colour_ofs), vertex_count};
    sceVu0FVECTOR sky_color = {50.0f, 100.0f, 255.0f, 0.0f};
    sceVu0FVECTOR band_color = {255.0f, 255.0f, 255.0f, 0.0f};

    sky_color[0] = sky_color[0] / 128.0f;
    sky_color[1] = sky_color[1] / 128.0f;
    sky_color[2] = sky_color[2] / 128.0f;
    band_color[0] = band_color[0] / 128.0f;
    band_color[1] = band_color[1] / 128.0f;
    band_color[2] = band_color[2] / 128.0f;

    for (int i = 0; i < vertex_count; i++) {
        const sceVu0FVECTOR &source = i % 4 < 2 ? sky_color : band_color;

        colors[i][0] = source[0];
        colors[i][1] = source[1];
        colors[i][2] = source[2];
        colors[i][3] = 1.0f;
    }

    frame->attr.remake_pending = 1;
}

void OpD_MotionProcess() {
    constexpr int actor_count = 23;

    for (int i = 0; i < actor_count; i++) {
        if (CScript__2.obj[i].disp) {
            if (CScript__2.obj[i].motion_end != -1 &&
                Chara__3[i].motion_type.state.time > static_cast<float>(Chara__3[i].motion_type.motion_info[CScript__2.obj[i].motion].end - 1)) {
                CScript__2.obj[i].motion = CScript__2.obj[i].motion_end;
                CScript__2.obj[i].motion_end = -1;
            }

            Chara__3[i].motion_type.state.blend_step = CScript__2.obj[i].step;

            if (CScript__2.obj[i].step == 1.0f && CScript__2.obj[i].motion != Chara__3[i].motion_no) {
                Chara__3[i].motion_type.state.time = static_cast<float>(Chara__3[i].motion_type.motion_info[CScript__2.obj[i].motion].start);
            }

            Chara__3[i].motion_no = CScript__2.obj[i].motion;
            Chara__3[i].motion_flags = 0;
            Chara__3[i].motion_speed = -1.0f;
        }
    }

    // Frame of the camera's model each actor is placed on.
    constexpr std::array<const char *, actor_count> frame_names = {
        "", "", "", "", "", "tyou2", "", "", "c01d", "", "", "c01d", "", "", "", "", "", "", "", "", "", "c03c", "tyou"};
    sceVu0FMATRIX matrix;

    for (int i = 0; i < actor_count; i++) {
        if (CScript__2.obj[i].disp) {
            CFrame *frame = Cam__2[SceneNp__2].frame->SearchFrame(const_cast<char *>(frame_names[i]));

            if (frame) {
                frame->GetLWMatrix(matrix);
                Chara__3[i].SetRotation(0.0f, atan2f(matrix[2][0], matrix[2][2]), 0.0f);
                Chara__3[i].SetPosition(matrix[3][0], matrix[3][1], matrix[3][2]);
            }
        }
    }

    sceVu0FVECTOR dir;

    if (CScript__2.camera_start < 115) {
        dir[0] = 0.0f;
        dir[2] = 0.0f;
        dir[1] = 0.0f;
        dir[3] = 0.0f;
        Wind.SetDir(dir);
        Wind.SetVelocity(0.0f);
    } else {
        dir[0] = -0.1f;
        dir[2] = 0.1f;
        dir[1] = 0.0f;
        dir[3] = 0.0f;
        Wind.SetDir(dir);
        Wind.SetVelocity(0.2f);
    }

    const int cloth_actor = CScript__2.camera_start > 111 ? 11 : 8;

    Chara__3[cloth_actor].wind = static_cast<int>(reinterpret_cast<intptr_t>(&Wind));
    Wind.Step();

    static int camera = 0;

    if (camera != CScript__2.camera_start) {
        camera = CScript__2.camera_start;

        switch (camera) {
            case 101:
            case 104:
            case 123:
                Chara__3[cloth_actor].ClothStep(-1);

                for (int i = 0; i < 20; i++) {
                    Chara__3[cloth_actor].ClothStep(0);
                }

                break;

            case 106:
            case 109:
            case 112:
            case 115:
            case 125:
                Chara__3[cloth_actor].ClothStep(-1);

                for (int i = 0; i < 10; i++) {
                    Chara__3[cloth_actor].ClothStep(0);
                }

                break;

            default:
                break;
        }
    }
}

// Each camera cue fires once and re-arms when the camera moves off it.
void OpD_SoundProcess() {
    {
        static int fired = 0;

        if (CScript__2.camera_start == 111) {
            if (!fired) {
                OpPlayVolSE(MIDI_PORT_SE_DEFAULT, 40, 42, 1.0f);
                OpPlayVolSE(MIDI_PORT_SE_DEFAULT, 40, 44, 1.0f);
                OpPlayVolSE(MIDI_PORT_SE_DEFAULT, 40, 49, 1.0f);
                OpPlayVolSE(MIDI_PORT_SE_DEFAULT, 40, 51, 1.0f);
                OpPlayVolSE(MIDI_PORT_SE_DEFAULT, 40, 45, 1.0f);
                OpPlayVolSE(MIDI_PORT_SE_DEFAULT, 40, 47, 1.0f);
                OpPlayVolSE(MIDI_PORT_SE_DEFAULT, 40, 48, 1.0f);
                fired = 1;
            }
        } else {
            fired = 0;
        }
    }

    {
        static int fired = 0;

        if (CScript__2.camera_start == 101) {
            if (CScript__2.mes_no == 101 && !fired) {
                OpBgmPlay();
                CSnd.SQ_Play(MIDI_PORT_AMBIENT, 0);
                fired = 1;
            }
        } else {
            fired = 0;
        }
    }

    {
        static int fired = 0;

        if (CScript__2.camera_start == 110) {
            if (!fired) {
                CSnd.Fade(MIDI_PORT_BGM, -1.0f, 0);
                fired = 1;
            }
        } else {
            fired = 0;
        }
    }

    {
        static int fired = 0;

        if (CScript__2.camera_start == 115) {
            if (!fired) {
                OpBgmPlay();
                CSnd.SQ_Play(MIDI_PORT_AMBIENT, 0);
                fired = 1;
            }
        } else {
            fired = 0;
        }
    }

    {
        static int fired = 0;

        if (CScript__2.camera_start == 118) {
            if (!fired) {
                CSnd.Fade(MIDI_PORT_BGM, -1.0f, 0);
                fired = 1;
            }
        } else {
            fired = 0;
        }
    }

    {
        static int fired = 0;

        if (CScript__2.camera_start == 123) {
            if (!fired) {
                SndStopAllSe();
                CSnd.Stop(MIDI_PORT_BGM);
                CSnd.StopVoice(0);
                CSnd.SetReverb(0, 4, 30);
                CSnd.SetReverb(1, 2, 5);
                CSnd.LoadSoundFileFromPack("o04c.txt", read_buffer);
                CSnd.SetVol(MIDI_PORT_SE_TITLE, 256);
                CSnd.SetVol(MIDI_PORT_SE_DEFAULT, 256);
                CSnd.SetVol(MIDI_PORT_UNK_D, 256);
                CSnd.SetVol(MIDI_PORT_SE_SPECIAL, 256);

                while (SndSyncBG()) {
                }

                SndBgmPlay(0);
                CSnd.SQ_Play(MIDI_PORT_AMBIENT, 0);
                fired = 1;
            }
        } else {
            fired = 0;
        }
    }

    if (CScript__2.obj[21].disp) {
        static int    wait = 0;
        sceVu0FVECTOR position;

        sceVu0CopyVector(position, Chara__3[21].pos);
        const auto motion_frame = static_cast<float>(static_cast<int>(Chara__3[21].motion_type.state.time));

        // The wait keeps a motion that stalls inside a footstep window from repeating the step.
        if (wait == 0) {
            if (motion_frame > 43.0f && motion_frame < 45.0f) {
                OpPlayVolPanSE(position, 50.0f, 300.0f, MIDI_PORT_SE_DEFAULT, 21, 20);
                wait = 4;
            } else if (motion_frame > 53.0f && motion_frame < 55.0f) {
                OpPlayVolPanSE(position, 50.0f, 300.0f, MIDI_PORT_SE_DEFAULT, 21, 21);
                wait = 4;
            }
        } else {
            wait = wait - 1;
        }
    }
}

void OpD_DrawProcess() {
    OP_CharaFrame__2 = Cam__2[SceneNp__2].frame;

    switch (CScript__2.camera_start) {
        case 106:
        case 107:
        case 108:
        case 112:
        case 113:
        case 121:
        case 123:
        case 125:
            MGSetRenderInfo(mgRenderInfo.scale[0], 1.0f, 0xffff);
            break;

        case 115:
        case 118:
        case 126:
            MGSetRenderInfo(mgRenderInfo.scale[0], 20.0f, 0xffff);
            break;

        default:
            MGSetRenderInfo(mgRenderInfo.scale[0], 8.0f, 0xffff);
            break;
    }

    if (CScript__2.camera_start < 115) {
        TexManager.ReloadTexture(Vif1Packet, 11);

        if (CurrentSprite() == OpDSprite::HallLight && amb3 < 127) {
            ++amb3;
        }

        sceVu0FVECTOR ambient;

        sceVu0CopyVector(ambient, ambientlight);
        ambient[3] = static_cast<float>(amb3);
        MGSetAmbient(ambient);

        CMapObject &hall = OP_NornMapObj[0];

        hall.Draw();
        MGSetAmbient(ambientlight);
    } else {
        float green = 100.0f;
        float blue = 255.0f;
        MGSetBGColor(50.0f, green, blue, 128.0f);
        TexManager.ReloadTexture(Vif1Packet, 10);

        for (int i = 1; i < 11; i++) {
            if (i == 8) {
                mgRenderInfo.unlit = 1;
            } else {
                mgRenderInfo.unlit = 0;
            }

            if (i != 7) {
                CMapObject &object = OP_NornMapObj[i];

                object.Draw();
            }
        }

        if (CScript__2.camera_start > 119) {
            CMapObject &flower = OP_NornMapObj[7];

            flower.FrameObjectOnOff("flo_S", 0);
            flower.Draw();
            flower.FrameObjectOnOff("flo_S", 1);

            CFrame       *frame = flower.frame[0]->SearchFrame("flo_S");
            sceVu0FVECTOR ambient;

            sceVu0CopyVector(ambient, ambientlight);
            ambient[3] = 70.0f;
            MGSetAmbient(ambient);
            MGDraw(frame);
            MGSetAmbient(ambientlight);
        }

        if (CScript__2.camera_start == 124) {
            LensFreaProcess();
        }
    }

    if (CScript__2.camera_start < 106) {
        HamonProcess();
    }

    TexManager.ReloadTexture(Vif1Packet, 22);

    float dof[2] = {400.0f, 1000.0f};

    DepthOfField(dof, 2, 64, 0);

    for (int i = 0; i < 23; i++) {
        if (i != 22 && i != 5 && CScript__2.obj[i].disp) {
            TexManager.ReloadTexture(Vif1Packet, 23);
            MGBeginDrawShadow(std::bit_cast<sceGsTex0>(TexManager.GetTexture("shadow_buff", -1)->tex0));

            if (!Pause) {
                Chara__3[i].ShadowStep();
            }

            Chara__3[i].DrawShadow();
            MGEndDrawShadow(52);
        }
    }

    for (int i = 0; i < 23; i++) {
        if (CScript__2.obj[i].disp) {
            TexManager.ReloadTexture(Vif1Packet, CharaTex__2[i]);

            if (!Pause) {
                Chara__3[i].Step();
                Chara__3[i].ClothStep(0);
            }

            if (i == 8 || i == 11) {
                FaceChangeMovie(i);
            }

            if (CScript__2.camera_start >= 111 && CScript__2.camera_start < 114) {
                sceVu0FVECTOR position;

                sceVu0CopyVector(position, Chara__3[i].frame->position);
                RollLight(position);
            }

            Chara__3[i].Draw();
            MGSetPLight(light, lightcolor);
        }
    }

    switch (CScript__2.camera_start) {
        case 111:
            EffectSeireiKing(2.0f);
            break;

        case 118:
        case 119:
            if (CScript__2.mes_wait == 0) {
                EffectSeireiKing(4.0f);
            }

            break;

        case 112:
        case 113:
            EffectAtraPrizum();
            break;

        default:
            break;
    }

    static int fade1 = 0;
    static int fade2 = 0;
    static int fade3 = 0;

    if (CurrentSprite() == OpDSprite::RuinStill1) {
        fade1 = 128;
    } else if (fade1 > 0) {
        fade1 -= 2;
    }

    if (CurrentSprite() == OpDSprite::RuinStill2) {
        if (fade2 < 128) {
            fade2 += 2;
        }
    } else if (fade2 > 0) {
        fade2 -= 2;
    }

    if (CurrentSprite() == OpDSprite::RuinStill3) {
        if (fade3 < 128) {
            fade3 += 2;
        }
    } else if (fade3 > 0) {
        fade3 -= 1;
    }

    if (fade1) {
        TexManager.ReloadTexture(Vif1Packet, 5);
        set2DSprite(GetVif1Packet(), TexManager.GetTexture("i00002", -1), CRect<int>(0, 0, 640, SCREEN_HEIGHT), CRect<int>(0, 0, 640, 448), static_cast<u_char>(fade1));
    }

    if (fade2) {
        TexManager.ReloadTexture(Vif1Packet, 5);
        set2DSprite(GetVif1Packet(), TexManager.GetTexture("i00006", -1), CRect<int>(0, 0, 640, SCREEN_HEIGHT), CRect<int>(0, 0, 640, 448), static_cast<u_char>(fade2));
    }

    if (fade3) {
        TexManager.ReloadTexture(Vif1Packet, 6);
        set2DSprite(GetVif1Packet(), TexManager.GetTexture("i00022", -1), CRect<int>(0, 0, 640, SCREEN_HEIGHT), CRect<int>(0, 0, 640, 448), static_cast<u_char>(fade3));
    }

    switch (CurrentSprite()) {
        case OpDSprite::Caption1:
        case OpDSprite::Caption2:
            TexManager.ReloadTexture(Vif1Packet, 7);
            Setsumei();
            break;

        default:
            break;
    }
}

// One model drawn twice, spun opposite ways about the frame the actor carries it on.
static void EffectAtraPrizum() {
    if (CScript__2.obj[11].disp) {
        static float rot1 = 0.0f;
        static float rot2 = 0.0f;

        if (!Pause) {
            rot1 += 0.01f;

            if (rot1 > 3.14f) {
                rot1 = rot1 - 6.28f;
            }

            rot2 = rot2 - 0.01f;

            if (rot2 < -3.14f) {
                rot2 += 6.28f;
            }
        }

        sceVu0FMATRIX matrix;
        sceVu0FVECTOR ambient;

        sceVu0CopyVector(ambient, ambientlight);
        ambient[3] = 32.0f;
        MGSetAmbient(ambient);

        sceGsAlpha alpha = mgAlpha;

        alpha.bits.a = 0;
        alpha.bits.b = 2;
        alpha.bits.c = 0;
        alpha.bits.d = 1;
        setAlphaFlag(Vif1Packet, &alpha);
        TexManager.ReloadTexture(Vif1Packet, 4);

        CFrame *frame = Chara__3[11].frame->SearchFrame("atrp");

        if (frame) {
            frame->GetLWMatrix(matrix);

            if (CScript__2.camera_start == 112) {
                sceVu0FVECTOR position;
                sceVu0FMATRIX spin;

                sceVu0CopyVector(position, matrix[3]);
                sceVu0UnitMatrix(spin);
                sceVu0RotMatrixY(spin, spin, rot1);
                sceVu0MulMatrix(matrix, matrix, spin);
                sceVu0CopyVector(matrix[3], position);
                Effect.frame->SetTransMatrix(matrix);
                Effect.motion_no = 0;
                Effect.motion_flags = 0;
                Effect.motion_speed = -1.0f;

                if (!Pause) {
                    Effect.Step();
                }

                Effect.Draw();
                frame->GetLWMatrix(matrix);
                sceVu0CopyVector(position, matrix[3]);
                sceVu0UnitMatrix(spin);
                sceVu0RotMatrixY(spin, spin, rot2);
                sceVu0MulMatrix(matrix, matrix, spin);
                sceVu0CopyVector(matrix[3], position);
                Effect.frame->SetTransMatrix(matrix);
                MGDraw(Effect.frame);
            }
        }

        if (!Pause) {
            SeireiKing.Step();
        }

        SeireiKing.Draw(matrix[3], 0.2f, 1);

        sceVu0FVECTOR eye;

        sceVu0CopyVector(eye, OP_CharaFrame__2->position);
        SeireiKing.Draw2(&OP_MainCamera, matrix[3], eye);
        setAlphaFlag(Vif1Packet, &mgAlpha);
        MGSetAmbient(ambientlight);
    }
}

// On camera 113 the source circles the effect, so the light sweeps while nothing else moves.
static void RollLight(float *target) {
    if (CScript__2.camera_start >= 111 && CScript__2.camera_start < 114) {
        CFrame *frame = CScript__2.camera_start == 111 ? Cam__2[SceneNp__2].frame->SearchFrame("hikari")
                                                       : Chara__3[11].frame->SearchFrame("atrp");

        sceVu0FMATRIX matrix;
        sceVu0FMATRIX dirs;
        sceVu0FMATRIX colors;
        sceVu0FVECTOR dir;

        frame->GetLWMatrix(matrix);

        switch (CScript__2.camera_start) {
            case 111:
            case 112:
                dir[0] = matrix[3][0] - target[0];
                dir[1] = matrix[3][1];
                dir[2] = matrix[3][2] - target[2];
                sceVu0Normalize(dir, dir);
                sceVu0CopyMatrix(dirs, light);
                sceVu0CopyMatrix(colors, lightcolor);
                dirs[0][1] = dir[0];
                dirs[1][1] = dir[1];
                dirs[2][1] = dir[2];
                colors[1][0] = 255.0f;
                colors[1][1] = 255.0f;
                colors[1][2] = 255.0f;
                break;

            case 113: {
                static float angle = 0.0f;

                angle += 0.03f;

                if (angle > 3.14f) {
                    angle = angle - 6.28f;
                }

                dir[0] = static_cast<float>(matrix[3][0] + sin(angle) * 25.0 - target[0]);
                dir[1] = static_cast<float>(matrix[3][1] + 5.0);
                dir[2] = static_cast<float>(matrix[3][2] + cos(angle) * 25.0 - target[2]);
                sceVu0Normalize(dir, dir);
                sceVu0CopyMatrix(dirs, light);
                sceVu0CopyMatrix(colors, lightcolor);
                dirs[0][1] = dir[0];
                dirs[1][1] = dir[1];
                dirs[2][1] = dir[2];
                colors[1][0] = 128.0f;
                colors[1][1] = 128.0f;
                colors[1][2] = 128.0f;
            } break;

            default:
                break;
        }

        MGSetPLight(dirs, colors);
    }
}

// The trail is stepped every other tick so it drifts rather than darts.
static void EffectSeireiKing(float size) {
    CFrame *frame = Cam__2[SceneNp__2].frame->SearchFrame("hikari");

    if (frame) {
        sceVu0FMATRIX matrix;

        frame->GetLWMatrix(matrix);
        TexManager.ReloadTexture(Vif1Packet, 4);

        sceVu0FVECTOR head;

        head[0] = matrix[3][0];
        head[1] = matrix[3][1];
        head[2] = matrix[3][2];

        if (!Pause) {
            static int tick = 0;

            tick = tick + 1;

            if (tick > 1) {
                tick = 0;
            }

            if (tick == 0) {
                SeireiKing.Step();
            }
        }

        SeireiKing.Draw(head, size, 2);
    }
}

static void LensFreaProcess() {
    sceVu0FVECTOR      world;
    std::array<int, 4> screen{};
    sceVu0FVECTOR      local = {0.0f, 0.0f, 0.0f, 1.0f};
    CFrame            *frame = SkyFrame->SearchFrame("sun1");

    if (frame) {
        frame->GetWorldPosition(world, local);

        if (MGRotTransPers2D(screen.data(), world, 0) && TexManager.GetTexture("lensfler", -1)) {
            LensFlare(TexManager.GetTexture("lensfler", -1), world, 255, 255, 255);
        }
    }
}

// Two counters cross-fade groups of caption pieces; each star trail steers towards a point off the
// screen and restarts when it leaves.
static void Setsumei() {
    static int tick_a = 0;
    static int phase_a = 0;

    tick_a = tick_a + 1;

    if (tick_a > 30) {
        tick_a = 0;
        phase_a = phase_a + 1;

        if (phase_a > 1) {
            phase_a = 0;
        }
    }

    static int tick_b = 0;
    static int phase_b = 0;

    tick_b = tick_b + 1;

    if (tick_b > 30) {
        tick_b = 0;
        phase_b = phase_b + 1;

        if (phase_b > 2) {
            phase_b = 0;
        }
    }

    static int fade_a1 = 128;
    static int fade_a2 = 0;

    if (phase_a == 0) {
        fade_a1 = 128;

        if (fade_a2 > 0) {
            fade_a2 = fade_a2 - 8;
        }
    } else {
        fade_a2 = 128;

        if (fade_a1 > 0) {
            fade_a1 = fade_a1 - 8;
        }
    }

    static int fade_b1 = 128;
    static int fade_b2 = 0;
    static int fade_b3 = 0;

    switch (phase_b) {
        case 0:
            fade_b1 = 128;

            if (fade_b2 > 0) {
                fade_b2 = fade_b2 - 8;
            }

            if (fade_b3 > 0) {
                fade_b3 = fade_b3 - 8;
            }

            break;

        case 1:
            fade_b2 = 128;

            if (fade_b1 > 0) {
                fade_b1 = fade_b1 - 8;
            }

            if (fade_b3 > 0) {
                fade_b3 = fade_b3 - 8;
            }

            break;

        case 2:
            fade_b3 = 128;

            if (fade_b1 > 0) {
                fade_b1 = fade_b1 - 8;
            }

            if (fade_b2 > 0) {
                fade_b2 = fade_b2 - 8;
            }

            break;

        default:
            break;
    }

    set2DSprite(GetVif1Packet(), TexManager.GetTexture("0519", -1), CRect<int>(0, 0, 640, SCREEN_HEIGHT), CRect<int>(0, 0, 640, 448), 128);
    set2DSprite(GetVif1Packet(), TexManager.GetTexture("0519p", -1), CRect<int>(540, 254, 88, 128), CRect<int>(89, 206, 88, 119), static_cast<u_char>(fade_a1));
    set2DSprite(GetVif1Packet(), TexManager.GetTexture("0519p", -1), CRect<int>(540, 254, 88, 128), CRect<int>(0, 206, 88, 119), static_cast<u_char>(fade_a2));
    set2DSprite(GetVif1Packet(), TexManager.GetTexture("0519p", -1), CRect<int>(450, 27, 105, 113), CRect<int>(117, 448, 105, 105), static_cast<u_char>(fade_b1));
    set2DSprite(GetVif1Packet(), TexManager.GetTexture("0519p", -1), CRect<int>(450, 27, 105, 113), CRect<int>(222, 448, 105, 105), static_cast<u_char>(fade_b2));
    set2DSprite(GetVif1Packet(), TexManager.GetTexture("0519p", -1), CRect<int>(450, 27, 105, 113), CRect<int>(327, 448, 105, 105), static_cast<u_char>(fade_b3));
    set2DSprite(GetVif1Packet(), TexManager.GetTexture("0519p", -1), CRect<int>(350, 139, 208, 62), CRect<int>(432, 448, 208, 58), static_cast<u_char>(fade_b1));
    set2DSprite(GetVif1Packet(), TexManager.GetTexture("0519p", -1), CRect<int>(350, 139, 208, 62), CRect<int>(432, 506, 208, 58), static_cast<u_char>(fade_b2));
    set2DSprite(GetVif1Packet(), TexManager.GetTexture("0519p", -1), CRect<int>(350, 139, 208, 62), CRect<int>(432, 564, 208, 58), static_cast<u_char>(fade_b3));
    set2DSprite(GetVif1Packet(), TexManager.GetTexture("0519p", -1), CRect<int>(100, 188, 117, 60), CRect<int>(0, 325, 117, 56), static_cast<u_char>(fade_a1));
    set2DSprite(GetVif1Packet(), TexManager.GetTexture("0519p", -1), CRect<int>(100, 188, 117, 60), CRect<int>(0, 381, 117, 56), static_cast<u_char>(fade_a2));

    if (static_cast<OpDSprite>(CScript__2.sprite) == OpDSprite::Caption1) {
        set2DSprite(GetVif1Packet(), TexManager.GetTexture("0519p", -1), CRect<int>(6, 245, 178, 191), CRect<int>(388, 0, 178, 178), static_cast<u_char>(fade_a1));
        set2DSprite(GetVif1Packet(), TexManager.GetTexture("0519p", -1), CRect<int>(184, 245, 178, 191), CRect<int>(388, 0, 178, 178), static_cast<u_char>(fade_a1));
        set2DSprite(GetVif1Packet(), TexManager.GetTexture("0519p", -1), CRect<int>(362, 245, 178, 191), CRect<int>(388, 0, 178, 178), static_cast<u_char>(fade_a1));
        set2DSprite(GetVif1Packet(), TexManager.GetTexture("0519p", -1), CRect<int>(6, 245, 178, 191), CRect<int>(210, 0, 178, 178), static_cast<u_char>(fade_a2));
        set2DSprite(GetVif1Packet(), TexManager.GetTexture("0519p", -1), CRect<int>(184, 245, 178, 191), CRect<int>(210, 0, 178, 178), static_cast<u_char>(fade_a2));
        set2DSprite(GetVif1Packet(), TexManager.GetTexture("0519p", -1), CRect<int>(362, 245, 178, 191), CRect<int>(210, 0, 178, 178), static_cast<u_char>(fade_a2));
    } else {
        static float x1 = 6.0f;
        static float y1 = 245.0f;
        static float rot1 = -1.57f;
        static float x2 = 184.0f;
        static float y2 = 245.0f;
        static float rot2 = 1.57f;
        static float x3 = 362.0f;
        static float y3 = 245.0f;
        static float rot3 = 1.57f;

        DrawRisingStar(x1, y1, rot1, -100.0f, 6.0f);
        DrawRisingStar(x2, y2, rot2, 500.0f, 184.0f);
        DrawRisingStar(x3, y3, rot3, 900.0f, 362.0f);
    }

    static float x1 = 426.0f;
    static float y1 = -221.0f;
    static float x2 = 726.0f;
    static float y2 = -328.0f;

    DrawFallingStar(x1, y1, 40.0f, 426.0f, -221.0f);
    DrawFallingStar(x2, y2, 240.0f, 726.0f, -328.0f);

    if (static_cast<OpDSprite>(CScript__2.sprite) == OpDSprite::Caption2) {
        set2DSprite(GetVif1Packet(), TexManager.GetTexture("0519p", -1), CRect<int>(40, 193, 463, 221), CRect<int>(177, 206, 463, 242), static_cast<u_char>(fade_a1));
    }
}

// Each ripple plays its sound the first time its scale grows past zero; only the first four draw.
static void HamonProcess() {
    sceVu0FVECTOR ambient;

    sceVu0CopyVector(ambient, ambientlight);

    constexpr std::array<float, 5> volumes = {1.0f, 0.85f, 0.6f, 0.45f, 0.3f};

    for (int i = 0; i < 5; i++) {
        static std::array<int, 5> sound_played = {0, 0, 0, 0, 0};

        if (!Pause && HScale[i] < 3.2f) {
            HScale[i] += 0.005f;
        }

        if (HScale[i] > 0.0f) {
            if (!sound_played[i]) {
                OpPlayVolSE(MIDI_PORT_SE_DEFAULT, 40, 46, volumes[i]);
                sound_played[i] = 1;
            }

            if (i < 4) {
                Hamon[i]->SetScale(HScale[i], HScale[i], HScale[i]);
                Hamon[i]->SetPosition(0.0f, 0.01f + static_cast<float>(i) / 100.0f, 0.0f);
                ambient[3] = std::max(128.0f - 40.0f * HScale[i], 0.0f);

                MGSetAmbient(ambient);

                sceGsAlpha alpha = mgAlpha;

                alpha.bits.a = 0;
                alpha.bits.b = 2;
                alpha.bits.c = 0;
                alpha.bits.d = 1;
                setAlphaFlag(Vif1Packet, &alpha);
                MGDraw(Hamon[i]);
                setAlphaFlag(Vif1Packet, &mgAlpha);
            }
        }
    }

    MGSetAmbient(ambientlight);
}

} // namespace dc::game
