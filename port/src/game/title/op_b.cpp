#include "common.h"

#include <libgraph.h>
#include <libpkt.h>
#include <libvu0.h>

#include <array>
#include <bit>

#include "camera.hpp"
#include "camerafollow.hpp"
#include "character.hpp"
#include "dataalloc.hpp"
#include "dataread.hpp"
#include "frame.hpp"
#include "framevu1.hpp"
#include "mainselect.hpp"
#include "mathutil.hpp"
#include "mds.hpp"
#include "mglib.hpp"
#include "renderinfo.hpp"
#include "sound.hpp"
#include "texture.hpp"
#include "title/op_a.hpp"
#include "title/op_b.hpp"
#include "title/opening.hpp"
#include "title/script.hpp"
#include "vector3.hpp"
#include "wind.hpp"

/* A texture-transfer rectangle, declared locally as title.cpp does rather than taken from rect.h. */
template <class T>
class CRect {
public:
    T x; /**< Left edge in pixels. */
    T y; /**< Top edge in pixels. */
    T w; /**< Width in pixels. */
    T h; /**< Height in pixels. */

    CRect() = default;

    CRect(T new_x, T new_y, T new_width, T new_height) : x(new_x), y(new_y), w(new_width), h(new_height) {}
} __attribute__((aligned(16)));

/* The classes below belong to other units; only the members this file touches are named. */

/* A frame parented to an object, which is what lets the world transform drive a model. */
class CObjectFrame : public CObject {
public:
    CFrameVu1 *frame[4];         /**< Frame of each level of detail; null where the object has none. */
    s32        rotation_changed; /**< Set whenever the angle, its motion or the moment changes. */
    s32        draw_on;          /**< 1 while the object draws; 0 leaves it out of the scene. */

    virtual void FrameObjectOnOff(char *name, int on);
    virtual void Draw();

    void SetFrame(CFrameVu1 *frame, int unknown0);
};

/* One piece of scenery. */
class CMapObject : public CObjectFrame {
public:
    CFrameVu1 *collision_frame; /**< Frame that collision reads; null where the object has none. */
    CFrameVu1 *shadow_frame;    /**< Model the object's shadow is drawn from. */
    CFrameVu1 *shade_frame;     /**< Frame that the shade draws from; null where the object takes none. */
    CFrameVu1 *camera_frame;    /**< Collision frame the camera reads; null where the object has none. */
    float      shadow_offset;   /**< Height the shadow drops below the object. */
    int        unk_34;          /**< Map category the object is sorted under. */
    int        handle;          /**< Map handle; below zero leaves the object undrawn. */
    char       unk_EC[4];

    CMapObject();

    void Draw() override;

    void Initialize();
    void DrawShadow(int unknown0);
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

/* One looping object animation: a frame is found by name and one of its properties is driven from
   a start value towards an end value by a step each tick. */
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

void MoveImageTest(sceVif1Packet *packet, int sbp, int sbw, int spsm, const CRect<int> &rect, int dbp, int dbw, int dpsm, int dsax, int dsay, int dir);

CWind Wind;

namespace {

/* Character pack and the configuration inside it, per village actor. */
struct CharaFile {
    const char *pack;
    const char *config;
};

constexpr std::array CharaFiles = {
    CharaFile{.pack = "opdat/chara/01c07a.chr", .config = "01c07a.cfg"},
    CharaFile{.pack = "opdat/chara/01c08a.chr", .config = "01c08a.cfg"},
    CharaFile{.pack = "opdat/chara/01c11a.chr", .config = "01c11a.cfg"},
    CharaFile{.pack = "opdat/chara/01c09a.chr", .config = "01c09a.cfg"},
    CharaFile{.pack = "opdat/chara/01c08b.chr", .config = "01c08b.cfg"},
    CharaFile{.pack = "opdat/chara/01c08c.chr", .config = "01c08c.cfg"},
};

CFrameVu1    *ToansHouse;
CFrameVu1    *DoransFuusya[2];
int           VolFade;
CFireOmni     CFire;
CCharacter    Komono;
OBJ_ANIME_SEQ Door;
OBJ_ANIME_SEQ Fuusya[2];
CFrame       *TaimatsuFrame[12];
OBJ_ANIME_SEQ Taimatsu[12];
CMapObject    OP_ToanMapObj;

/* The village sign's animated texture: a strip of eight 64-pixel frames in its own texture is
   copied one frame at a time over the plate the world draws. The counter advances by a half so a
   frame stands for two ticks; the texture cache is flushed first because the plate about to be
   overwritten is the one the previous tick drew from. */
void setTexAnime() {
    sceGifTag giftag = {0, 1, 0, 0, 0, 0, 1, SCE_GIF_PACKED_AD};

    sceVif1PkCnt(Vif1Packet, 0);
    sceVif1PkOpenDirectCode(Vif1Packet, 0);
    sceVif1PkOpenGifTag(Vif1Packet, std::bit_cast<u_long128>(giftag));
    sceVif1PkAddGsAD(Vif1Packet, SCE_GS_TEXFLUSH, 0);
    sceVif1PkCloseGifTag(Vif1Packet);
    sceVif1PkCloseDirectCode(Vif1Packet);
    sceVif1PkTerminate(Vif1Packet);

    CTexture *plate = TexManager.GetTexture("i01e01", -1);
    CTexture *strip = TexManager.GetTexture("i01e01_a", -1);

    if (plate == nullptr || strip == nullptr) {
        return;
    }

    int dbp = plate->tex0 & 0x3fff;
    int dbw = (plate->tex0 >> 14) & 0x3f;
    int sbp = strip->tex0 & 0x3fff;
    int sbw = (strip->tex0 >> 14) & 0x3f;

    static float frame_counter = 0.0f;

    MoveImageTest(Vif1Packet, sbp, sbw, 0, CRect<int>(0, static_cast<int>(frame_counter) * 64, 64, 64), dbp, dbw, 0, 0, 0, 0);

    frame_counter += 0.5f;

    if (frame_counter > 7.0f) {
        frame_counter = 0.0f;
    }
}

} // namespace

void LoadCharaData(int buffer_no, int actor_no) {
    const CharaFile &file = CharaFiles[actor_no];

    switch (CScript__2.obj[actor_no].load_step) {
        case TSLOAD_START:
            LoadFileBG(const_cast<char *>(file.pack), reinterpret_cast<u_long128 *>(read_buffer), nullptr);
            CScript__2.obj[actor_no].load_step = TSLOAD_WAIT;
            break;
        case TSLOAD_WAIT:
            if (!ReadBGSync()) {
                CScript__2.obj[actor_no].load_step = TSLOAD_UNPACK;
            }

            break;
        case TSLOAD_UNPACK:
            Chara__3[actor_no].Initialize();
            CharaDataBuffer__2[buffer_no].used = 0;
            Chara__3[actor_no].LoadPackData(read_buffer, const_cast<char *>(file.config), &CharaDataBuffer__2[buffer_no], nullptr);
            Chara__3[actor_no].motion_type.state.time = 10.0f;
            Chara__3[actor_no].motion_type.state.blend_step = 0.05f;
            Chara__3[actor_no].motion_type.state.motion_no = 0;
            Chara__3[actor_no].motion_type.state.playing_no = 0;
            CScript__2.obj[actor_no].load = -1;
            CScript__2.obj[actor_no].load_step = TSLOAD_NONE;
            break;
        default:
            break;
    }
}

void OpB_LoadDataBG() {
    while (ReadBGSync()) {
    }

    LoadFileBG("opdat/norn/norn.pak", reinterpret_cast<u_long128 *>(read_buffer), nullptr);
    CScript__2.load_no = OP_SCENE_NONE;
}

void OpB_LoadDataBG2() {
    while (ReadBGSync()) {
    }

    LoadFileBG("opdat/toan/toan.pim", reinterpret_cast<u_long128 *>(read_buffer), nullptr);
    CScript__2.load_no = OP_SCENE_NONE;
}

/* A footfall's frame window is only open for one tick at the motion's rate; the wait keeps a
   stutter in the motion from playing the step twice. The sample changes part way through the
   scene, judged by how far the camera's motion has run, because the ground changes under the actor. */
void OpB_SoundProcess() {
    if (CScript__2.scene == OP_SCENE_NORUNE) {
        float camera_time = Cam__2[SceneNp__2].motion_type.state.time;

        if (CScript__2.obj[9].motion == 0) {
            static int    footstep_wait = 0;
            sceVu0FVECTOR position;

            sceVu0CopyVector(position, Chara__3[9].pos);
            int motion_frame = static_cast<int>(Chara__3[9].motion_type.state.time);

            if (footstep_wait == 0) {
                if (motion_frame > 28 && motion_frame < 30) {
                    if (camera_time < 387.0f) {
                        OpPlayVolPanSE(position, 10.0f, 400.0f, MIDI_PORT_SE_DEFAULT, 21, 20);
                    } else {
                        OpPlayVolPanSE(position, 10.0f, 400.0f, MIDI_PORT_SE_DEFAULT, 21, 32);
                    }

                    footstep_wait = 4;
                } else if (motion_frame > 38 && motion_frame < 40) {
                    if (camera_time < 387.0f) {
                        OpPlayVolPanSE(position, 10.0f, 400.0f, MIDI_PORT_SE_DEFAULT, 21, 21);
                    } else {
                        OpPlayVolPanSE(position, 10.0f, 400.0f, MIDI_PORT_SE_DEFAULT, 21, 33);
                    }

                    footstep_wait = 4;
                }
            } else {
                --footstep_wait;
            }
        }

        sceVu0FVECTOR river = {50.0f, 50.0f, -100.0f, 0.0f};

        OpSetVolPanSE(river, 100.0f, 400.0f, 15, 16, 21);
    } else if (CScript__2.scene == OP_SCENE_TOAN_HOUSE) {
        if (CScript__2.sprite == 1) {
            CSnd.SetVol(MIDI_PORT_BGM, static_cast<float>(OpGetVolSQ(0) * 1.0));
            CSnd.SetVol(MIDI_PORT_AMBIENT, static_cast<float>(OpGetVolSQ(1) * 0.7));
        }

        if (CScript__2.camera_start == 55) {
            CSnd.SetVol(MIDI_PORT_BGM, static_cast<float>(OpGetVolSQ(0) * 0.5));
            CSnd.SetVol(MIDI_PORT_AMBIENT, static_cast<float>(OpGetVolSQ(1) * 0.1));
        }

        if (CScript__2.camera_start == 55 && CScript__2.obj[10].motion == 6) {
            static int    footstep_wait = 0;
            sceVu0FVECTOR position;

            sceVu0CopyVector(position, Chara__3[9].pos);
            int motion_frame = static_cast<int>(Chara__3[10].motion_type.state.time);

            if (footstep_wait == 0) {
                if (motion_frame > 134 && motion_frame < 136) {
                    OpPlayVolPanSE(position, 50.0f, 300.0f, MIDI_PORT_SE_DEFAULT, 21, 32);
                    footstep_wait = 5;
                } else if (motion_frame > 144 && motion_frame < 146) {
                    OpPlayVolPanSE(position, 50.0f, 300.0f, MIDI_PORT_SE_DEFAULT, 21, 33);
                    footstep_wait = 5;
                }
            } else {
                --footstep_wait;
            }
        }

        sceVu0FVECTOR position;

        sceVu0CopyVector(position, Chara__3[10].pos);
        OpSetVolPanSE(position, 100.0f, 200.0f, 14, 40, 39);
    }
}
