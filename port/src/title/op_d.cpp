#include "common.h"

#include <libgraph.h>
#include <libvu0.h>

#include <array>
#include <bit>
#include <cstdlib>

#include "camera.hpp"
#include "camerafollow.hpp"
#include "character.hpp"
#include "dataalloc.hpp"
#include "dataread.hpp"
#include "frame.hpp"
#include "framevu1.hpp"
#include "main.hpp"
#include "mapobject.hpp"
#include "mathutil.hpp"
#include "mds.hpp"
#include "mglib.hpp"
#include "renderinfo.hpp"
#include "snd.hpp"
#include "sound.hpp"
#include "texture.hpp"
#include "title/op_a.hpp"
#include "title/op_b.hpp"
#include "title/op_d.hpp"
#include "title/opening.hpp"
#include "title/script.hpp"
#include "title/seireiking.hpp"

// The texture and pack-file APIs take char * names, which this unit passes as string literals.
#pragma clang diagnostic ignored "-Wwritable-strings"

template <class T>
class CRect {
public:
    T x;
    T y;
    T w;
    T h;

    CRect(T left, T top, T width, T height) {
        x = left;
        y = top;
        w = width;
        h = height;
    }
};

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

} // namespace

void MoveImageTest(sceVif1Packet *packet, int sbp, int sbw, int spsm, const CRect<int> &rect, int dbp, int dbw,
                   int dpsm, int dsax, int dsay, int dir);

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

namespace {

// Values of CScript__2.sprite in this scene.
enum class OpDSprite : int {
    None = 0,
    HallLight = 1,
    RuinStill1 = 2,
    RuinStill2 = 3,
    RuinStill3 = 4,
    Caption1 = 5,
    Caption2 = 6,
};

OpDSprite CurrentSprite() {
    return static_cast<OpDSprite>(CScript__2.sprite);
}

struct MAPOBJ_INFO {
    char *name;
    char *shadow_name;
    float position[3];
    float rotation[3];
};

} // namespace

void set2DSprite(sceVif1Packet *packet, CTexture *texture, const CRect<int> &src, const CRect<int> &dst, u_char alpha);
void DepthOfField(float *dist, int level, int alpha, int blur);
void FaceChangeMovie(int actor);

// State and helpers defined by the rest of op_d under OpD_-prefixed labels.
extern CFrameVu1  *Hamon[4] asm("OpD_Hamon");
extern float       HScale[5] asm("OpD_HScale");
extern CCharacter  Effect asm("OpD_Effect");
extern CSeireiKing SeireiKing asm("OpD_SeireiKing");
extern int         amb3 asm("OpD_amb3");
extern CFrameVu1  *SkyFrame asm("OpD_SkyFrame");

void SkyColor(CFrameVu1 *frame) asm("OpD_SkyColor");
void EffectAtraPrizum() asm("OpD_EffectAtraPrizum");
void RollLight(float *target) asm("OpD_RollLight");
void EffectSeireiKing(float size) asm("OpD_EffectSeireiKing");
void LensFreaProcess() asm("OpD_LensFreaProcess");
void Setsumei() asm("OpD_Setsumei");
void HamonProcess() asm("OpD_HamonProcess");

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
