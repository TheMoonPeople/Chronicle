#pragma helper_mask_gpr 0x30

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

/**
 * Phases of an actor's blink, as FACE_INFO::blink holds them.
 */
// clang-format off
enum FaceBlink {
    BLINK_IDLE    = 0, /**< Eyes open, waiting for the next blink. */
    BLINK_CLOSING = 1, /**< Eyes closing towards the last eye frame. */
    BLINK_OPENING = 2, /**< Eyes opening back to the first frame. */
};

// clang-format on

typedef MOTION_INFO tagMOTION_KEY;

/* The rectangle a texture transfer takes, declared here rather than reached through rect.h for
   the reason title.cpp declares its own: every rectangle this overlay builds is built by a
   constructor that assigns x, y, w and h in that order, and rect.h's assigns them in the other. */
template <class T>
class CRect {
public:
    T x; /**< Left edge in pixels. */
    T y; /**< Top edge in pixels. */
    T w; /**< Width in pixels. */
    T h; /**< Height in pixels. */

    CRect() {}

    CRect(T new_x, T new_y, T new_width, T new_height) {
        x = new_x;
        y = new_y;
        w = new_width;
        h = new_height;
    }
} __attribute__((aligned(16)));

/* The three classes this scene places in the world, declared here rather than reached through
   headers of their own because each is another unit's to type. Only the members this file touches
   are named; the extents are the sizes the executable gives the objects below. */

/* A frame parented to an object, which is what lets the world transform drive a model. */
class CObjectFrame : public CObject {
public:
    CFrameVu1 *frame[4];         /**< Frame of each level of detail; zero where the object has none. */
    s32        rotation_changed; /**< Set whenever the angle, its motion or the moment changes. */
    s32        draw_on;          /**< 1 while the object draws; 0 leaves it out of the scene. */

    virtual void FrameObjectOnOff(char *name, int on);
    virtual void Draw();

    void SetFrame(CFrameVu1 *frame, int unknown0);
};

/* One piece of scenery. The scene builds a table of them, hands each its model, and drives them
   through the object dispatch like anything else in the world. */
class CMapObject : public CObjectFrame {
public:
    CFrameVu1 *collision_frame; /**< Frame that collision reads; zero where the object has none. */
    CFrameVu1 *shadow_frame;    /**< Model the object's shadow is drawn from. */
    CFrameVu1 *shade_frame;     /**< Frame that the shade draws from; zero where the object takes none. */
    CFrameVu1 *camera_frame;    /**< Collision frame the camera reads; zero where the object has none. */
    float      shadow_offset;   /**< Height the shadow drops below the object. */
    int        unk_34;          /**< Map category the object is sorted under. */
    int        handle;          /**< Map handle; below zero leaves the object undrawn. */
    char       unk_EC[4];

    CMapObject();

    virtual void Draw();

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

/* One actor's face, as this scene animates it. The eyes and the mouth are two strips of frames
   stacked bottom-up in one 256-wide texture — the eyes down the left half and the mouth down the
   right — and a tick copies the current frame of each over the plate the model draws with. The two
   offsets are measured from the bottom edge of the plate; the blink state is kept here because this
   scene blinks the cast on a clock of its own rather than from the script. */
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

/* One piece of scenery as the scene was laid out: the model, the model its shadow is drawn
   from, where it stands in tenths of a world unit, and its heading in degrees. */
struct MAPOBJ_INFO {
    char *name;        /**< Model file, or null to reuse the previous row's model. */
    char *shadow_name; /**< Shadow model file, or null for none. */
    float position[3]; /**< Position in tenths of a world unit. */
    float rotation[3]; /**< Heading about each axis in degrees. */
};

void InitObjAnime(CFrame *frame, OBJ_ANIME_SEQ *sequence);
void ObjAnimePlay(OBJ_ANIME_SEQ *sequence);
void MoveImageTest(sceVif1Packet *packet, int sbp, int sbw, int spsm, const CRect<int> &rect, int dbp, int dbw, int dpsm, int dsax, int dsay, int dir);
void DepthOfField(float *dist, int level, int alpha, int blur);

static CFrameVu1 *ToansHouse;
static CFrameVu1 *DoransFuusya[2];
static int        VolFade;

static void setTexAnime();
void        FaceChangeC(int no);

extern CMapObject OP_NornMapObj[76];
extern CMapObject OP_NornMapObj2[87];

static CFireOmni     CFire;
static CCharacter    Komono;
static OBJ_ANIME_SEQ Door;
static OBJ_ANIME_SEQ Fuusya[2];
CWind                Wind;
static CFrame       *TaimatsuFrame[12];
static OBJ_ANIME_SEQ Taimatsu[12];
static CMapObject    OP_ToanMapObj;

/* One actor's blinking and speaking. Four of the eight rows are never blinked because their actors
   are too far from the camera for it to read, and the two the script talks through carry a hand-off
   of their own: an eye number the script set is stepped on by one here once the line it belongs to
   has been on screen long enough, which is how a raised eyebrow outlasts the word that raised it.
   The mouth is driven from the script's own clock, a new frame picked at random every sixth
   hundredth of a second left on the line's timer while the actor is talking. */
void FaceChange(int actor_no);

void LoadCharaData(int buffer_no, int actor_no) {
    char *name[6][2] = {
        {"opdat/chara/01c07a.chr", "01c07a.cfg"},
        {"opdat/chara/01c08a.chr", "01c08a.cfg"},
        {"opdat/chara/01c11a.chr", "01c11a.cfg"},
        {"opdat/chara/01c09a.chr", "01c09a.cfg"},
        {"opdat/chara/01c08b.chr", "01c08b.cfg"},
        {"opdat/chara/01c08c.chr", "01c08c.cfg"}
    };

    switch (CScript__2.obj[actor_no].load_step) {
        case TSLOAD_START:
            LoadFileBG(name[actor_no][0], (u_long128 *) read_buffer, 0);
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
            Chara__3[actor_no].LoadPackData(read_buffer, name[actor_no][1], &CharaDataBuffer__2[buffer_no], 0);
            Chara__3[actor_no].motion_type.state.time = 10.0f;
            Chara__3[actor_no].motion_type.state.blend_step = 0.05f;
            Chara__3[actor_no].motion_type.state.motion_no = 0;
            Chara__3[actor_no].motion_type.state.playing_no = 0;
            CScript__2.obj[actor_no].load = -1;
            CScript__2.obj[actor_no].load_step = TSLOAD_NONE;
            break;
    }
}

/* The couple's dance, which is ten motion files played end to end. A file is swapped in when the
   one running reaches its last key and the next pair is started in the background straight after,
   so the dance runs continuously off a buffer that only ever holds two steps. */
void LoadMotionData();

void OpB_LoadDataBG() {
    while (ReadBGSync())
        ;

    LoadFileBG("opdat/norn/norn.pak", (u_long128 *) read_buffer, 0);
    CScript__2.load_no = OP_SCENE_NONE;
}

void OpB_LoadDataBG2() {
    while (ReadBGSync())
        ;

    LoadFileBG("opdat/toan/toan.pim", (u_long128 *) read_buffer, 0);
    CScript__2.load_no = OP_SCENE_NONE;
}

/* The village scene's set-up, and the shape every scene file's is a variation of. The textures come
   out of the pack the background load left in memory, so the manifest is built with its five fixed
   surfaces named and its eighteen scene images filled in by name afterwards. The two tables below
   are the scene as it was laid out: a model, the model its shadow is drawn from, a position in
   tenths of a world unit and a heading in degrees, one row per piece of scenery. The rows with no
   model of their own are further copies of the row above them, which is why the frame is only
   reloaded where a name is given. */
void OpB_InitProcess();

/* The second half of the scene, which starts when the camera cuts to Toan's house. Everything the
   first half loaded is thrown away and the pack the background read left in memory is unpacked
   again: the block numbers say which of the registry's blocks each image belongs to, and the five
   named entries are the fixed surfaces the registry keeps for every scene. The three actors are
   loaded one at a time rather than through a table because each is followed by set-up of its own —
   Toan's mother has a frame turned off, Toan himself carries the cloth the wind drives, and the
   second Toan is the one the door animation is timed against. */
void OpB_InitProcess2();

/* The scene's per-tick motion. Four of the actors are told what the script last asked of them —
   the loop is over the four the scene animates rather than over all of them — and a motion that
   has run past its last key falls through to whatever motion was queued behind it. Then the three
   models the camera's own frame tree carries drive three more actors: each is found by name in the
   camera model, and its world transform becomes that actor's position and heading. */
void OpB_MotionProcess();

/* The scene's per-tick sound. Two of the actors' footfalls are played from their own positions
   rather than from a track, which is why each is a window on the actor's motion frame with a wait
   behind it: a frame number is only inside its window for one tick at the motion's own rate, and
   the wait keeps a stutter in the motion from playing the step twice. Which of the two samples a
   footfall takes is decided by how far the camera's motion has run, because the ground changes
   under the actor part way through the scene. */
void OpB_SoundProcess() {
    if (CScript__2.scene == OP_SCENE_NORUNE) {
        float camera_time = Cam__2[SceneNp__2].motion_type.state.time;

        if (CScript__2.obj[9].motion == 0) {
            static int    wait = 0;
            sceVu0FVECTOR position;
            int           motion_frame;

            sceVu0CopyVector(position, Chara__3[9].pos);
            motion_frame = (int) Chara__3[9].motion_type.state.time;

            if (wait == 0) {
                if (motion_frame > 28 && motion_frame < 30) {
                    if (camera_time < 387.0f) {
                        OpPlayVolPanSE(position, 10.0f, 400.0f, MIDI_PORT_SE_DEFAULT, 21, 20);
                    } else {
                        OpPlayVolPanSE(position, 10.0f, 400.0f, MIDI_PORT_SE_DEFAULT, 21, 32);
                    }

                    wait = 4;
                } else if (motion_frame > 38 && motion_frame < 40) {
                    if (camera_time < 387.0f) {
                        OpPlayVolPanSE(&position[0], (float) (10 + (motion_frame & 0)), (float) (wait - wait + 400), MIDI_PORT_SE_DEFAULT, 21, 21);
                    } else {
                        OpPlayVolPanSE(position, 10.0f, 400.0f, MIDI_PORT_SE_DEFAULT, 21, 33);
                    }

                    wait = 4;
                }
            } else {
                wait = wait - 1;
            }
        }

        sceVu0FVECTOR river = {50.0f, 50.0f, -100.0f, 0.0f};

        OpSetVolPanSE(river, 100.0f, 400.0f, 15, 16, 21);
    } else if (CScript__2.scene == OP_SCENE_TOAN_HOUSE) {
        if (CScript__2.sprite == 1) {
            CSnd.SetVol(MIDI_PORT_BGM, (float) (OpGetVolSQ(0) * 1.0));
            CSnd.SetVol(MIDI_PORT_AMBIENT, (float) (OpGetVolSQ(1) * 0.7));
        }

        if (CScript__2.camera_start == 55) {
            CSnd.SetVol(MIDI_PORT_BGM, (float) (OpGetVolSQ(0) * 0.5));
            CSnd.SetVol(MIDI_PORT_AMBIENT, (float) (OpGetVolSQ(1) * 0.1));
        }

        if (CScript__2.camera_start == 55 && CScript__2.obj[10].motion == 6) {
            static int    wait = 0;
            sceVu0FVECTOR position;
            int           motion_frame;

            sceVu0CopyVector(position, Chara__3[9].pos);
            motion_frame = (int) Chara__3[10].motion_type.state.time;

            if (wait == 0) {
                if (motion_frame > 134 && motion_frame < 136) {
                    OpPlayVolPanSE(position, 50.0f, 300.0f, MIDI_PORT_SE_DEFAULT, 21, 32);
                    wait = 5;
                } else if (motion_frame > 144 && motion_frame < 146) {
                    OpPlayVolPanSE(position, 50.0f, 300.0f, MIDI_PORT_SE_DEFAULT, 21, 33);
                    wait = 5;
                }
            } else {
                wait = wait - 1;
            }
        }

        sceVu0FVECTOR position;

        sceVu0CopyVector(position, Chara__3[10].pos);
        OpSetVolPanSE(position, 100.0f, 200.0f, 14, 40, 39);
    }
}

/* The scene's per-tick drawing, in the order the frame is built: the world, then the fires that
   are lights rather than models, then the actors' shadows onto the one buffer that holds them all,
   then the actors themselves, and last the depth of field the two outdoor scenes take. The near
   plane is pulled in to half a unit because the camera passes through the scenery, and the far one
   is the largest the Z buffer holds. */
void OpB_DrawProcess();

/**
 * The village sign's one animated texture. The animation is a strip of eight 64-pixel frames in a
 * texture of its own and the plate the world draws is another, so a tick is a local-to-local
 * transfer of one frame over the plate; the counter advances by a half so that a frame stands for
 * two ticks, and the texture cache is flushed first because the plate about to be overwritten is
 * the one the previous tick drew from.
 *
 * @mangled setTexAnime__Fv
 * @address 0x1DBB8A0
 * @size 0x1EC
 * @unknownret
 */
static void setTexAnime() {
    sceGifTag giftag = {0, 1, 0, 0, 0, 0, 1, SCE_GIF_PACKED_AD};
    CTexture *plate;
    CTexture *strip;
    int       dbp;
    int       dbw;
    int       sbp;
    int       sbw;

    sceVif1PkCnt(Vif1Packet, 0);
    sceVif1PkOpenDirectCode(Vif1Packet, 0);
    sceVif1PkOpenGifTag(Vif1Packet, *(u_long128 *) &giftag);
    sceVif1PkAddGsAD(Vif1Packet, SCE_GS_TEXFLUSH, 0);
    sceVif1PkCloseGifTag(Vif1Packet);
    sceVif1PkCloseDirectCode(Vif1Packet);
    sceVif1PkTerminate(Vif1Packet);

    plate = TexManager.GetTexture("i01e01", -1);
    strip = TexManager.GetTexture("i01e01_a", -1);

    if (plate == 0 || strip == 0) {
        return;
    }

    dbp = plate->tex0 & 0x3fff;
    dbw = (plate->tex0 >> 14) & 0x3f;
    sbp = strip->tex0 & 0x3fff;
    sbw = (strip->tex0 >> 14) & 0x3f;

    static float cnt = 0.0f;

    MoveImageTest(Vif1Packet, sbp, sbw, 0, CRect<int>(0, (int) cnt * 64, 64, 64), dbp, dbw, 0, 0, 0, 0);

    cnt += 0.5f;

    if (cnt > 7.0f) {
        cnt = 0.0f;
    }
}

/* One actor's blinking and speaking. The mouth is driven from the script's own clock rather than
   from a motion: while the actor is talking, a new mouth frame is picked at random every sixth
   hundredth of a second left on the timer, and the timer running out closes the mouth and ends the
   line. The eyes are whatever the script last asked for. The cache is flushed on both sides of the
   two transfers because the plate is a texture the previous tick drew from and the next one will. */
