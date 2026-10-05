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
    virtual void FrameObjectOnOff(char *name, int on);
    virtual void Draw();

    void SetFrame(CFrameVu1 *frame, int unknown0);
};

/* One piece of scenery. The scene builds a table of them, hands each its model, and drives them
   through the object dispatch like anything else in the world. */
class CMapObject : public CObjectFrame {
public:
    char       unk_00[36];
    CFrameVu1 *shadow_frame; /**< Model the object's shadow is drawn from. */
    char       unk_28[8];
    float      shadow_offset; /**< Height the shadow drops below the object. */
    int        unk_34;        /**< Map category the object is sorted under. */
    int        handle;        /**< Map handle; below zero leaves the object undrawn. */
    char       unk_EC[4];

    CMapObject();

    virtual void Draw();

    void Initialize();
    void DrawShadow(int unknown0);
};

/* The scene's one fire, which is a light rather than a model. */
class CFireOmni {
public:
    char          unk_00[32];
    sceVu0FVECTOR pos; /**< World position DrawFire draws the next fire at. */
    char          unk_30[16];

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
    char          unk_4C[60];

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

CMapObject OP_NornMapObj[76];
CMapObject OP_NornMapObj2[87];

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
void FaceChange(int actor_no) {
    sceGifTag        giftag = {0, 1, 0, 0, 0, 0, 1, SCE_GIF_PACKED_AD};
    static FACE_INFO face[8] = {
        {"c07a01",  "c07a01an",  42, 40, 87, 35, 0, 0, 256, 2, 0},
        {"c08a01",  "c08a01an",  42, 40, 87, 35, 0, 0, 320, 2, 0},
        {"c11a01",  "c11a01an",  32, 40, 84, 35, 0, 0, 448, 3, 0},
        {"c09a01",  "c09a01an",  10, 40, 73, 35, 0, 0, 448, 2, 0},
        {"c08a01",  "c08a01an",  42, 40, 87, 35, 0, 0, 320, 2, 0},
        {"c08a01",  "c08a01an",  42, 40, 87, 35, 0, 0, 320, 2, 0},
        {"p19a_03", "p19a_03an", 8,  40, 76, 35, 0, 0, 256, 2, 0},
        {"p17a01",  "p17a01an",  42, 40, 87, 35, 0, 0, 320, 2, 0}
    };
    CTexture *plate;
    CTexture *strip;
    int       sbp;
    int       dbp;
    int       sbw;
    int       dbw;

    sceVif1PkCnt(Vif1Packet, 0);
    sceVif1PkOpenDirectCode(Vif1Packet, 0);
    sceVif1PkOpenGifTag(Vif1Packet, *(u_long128 *) &giftag);
    sceVif1PkAddGsAD(Vif1Packet, SCE_GS_TEXFLUSH, 0);
    sceVif1PkCloseGifTag(Vif1Packet);
    sceVif1PkCloseDirectCode(Vif1Packet);
    sceVif1PkTerminate(Vif1Packet);

    plate = TexManager.GetTexture(face[actor_no].plate, -1);
    strip = TexManager.GetTexture(face[actor_no].strip, -1);

    if (plate == 0 || strip == 0) {
        return;
    }

    sbp = strip->tex0 & 0x3fff;
    dbp = plate->tex0 & 0x3fff;
    sbw = (strip->tex0 >> 14) & 0x3f;
    dbw = (plate->tex0 >> 14) & 0x3f;

    if (actor_no == 1) {
        return;
    }

    if (actor_no == 4) {
        return;
    }

    if (actor_no == 5) {
        return;
    }

    if (actor_no == 7) {
        return;
    }

    if (!Pause) {
        if (CScript__2.obj[actor_no].eye > face[actor_no].eye_max) {
            face[actor_no].blink = BLINK_IDLE;
        }

        if (CScript__2.obj[actor_no].eye_time >= CScript__2.motion_step) {
            CScript__2.obj[actor_no].eye_time -= CScript__2.motion_step;

            switch (face[actor_no].blink) {
                case BLINK_CLOSING:
                    if (CScript__2.obj[actor_no].eye < face[actor_no].eye_max) {
                        CScript__2.obj[actor_no].eye++;

                        if (CScript__2.obj[actor_no].eye == face[actor_no].eye_max) {
                            face[actor_no].blink = BLINK_OPENING;
                        }
                    }

                    break;
                case BLINK_OPENING:
                    if (CScript__2.obj[actor_no].eye > 0) {
                        CScript__2.obj[actor_no].eye--;
                    }

                    break;
            }
        } else if (rand() % 200 == 0) {
            if (CScript__2.obj[actor_no].eye == 0) {
                face[actor_no].blink = BLINK_CLOSING;
                CScript__2.obj[actor_no].eye = 1;
                CScript__2.obj[actor_no].eye_time = 15.0f * CScript__2.motion_step;
            }
        } else if (actor_no == 2) {
            if (CScript__2.obj[2].eye == 5) {
                CScript__2.obj[2].eye = 4;
                CScript__2.obj[2].eye_time = CScript__2.motion_step;
            } else {
                CScript__2.obj[actor_no].eye = 0;
                face[actor_no].blink = BLINK_IDLE;
            }
        } else {
            CScript__2.obj[actor_no].eye = 0;
            face[actor_no].blink = BLINK_IDLE;
        }
    }

    face[actor_no].eye = CScript__2.obj[actor_no].eye;

    MoveImageTest(Vif1Packet, sbp, sbw, SCE_GS_PSMT8, CRect<int>(0, face[actor_no].strip_bottom - face[actor_no].eye_height * (face[actor_no].eye + 1), 128, face[actor_no].eye_height), dbp, dbw, SCE_GS_PSMT8, 0, 88 - face[actor_no].eye_bottom, 0);

    if (!Pause) {
        if (CScript__2.obj[2].eye_time > 1.0f) {
            if (CScript__2.obj[2].eye == 4) {
                CScript__2.obj[2].eye = 5;
            }

            if (CScript__2.obj[2].eye == 6) {
                CScript__2.obj[2].eye = 7;
            }
        }

        if (CScript__2.obj[3].eye_time > 1.0f) {
            if (CScript__2.obj[3].eye == 3) {
                CScript__2.obj[3].eye = 4;
            }
        }
    }

    if (!Pause) {
        if (CScript__2.obj[actor_no].mouth_time >= CScript__2.motion_step) {
            CScript__2.obj[actor_no].mouth_time -= CScript__2.motion_step;

            if (CScript__2.obj[actor_no].talk) {
#ifdef PAL_TIMING
                if (rand() % 5 == 0) {
#else
                if ((int) (100.0f * CScript__2.obj[actor_no].mouth_time) % 6 == 0) {
#endif
                    CScript__2.obj[actor_no].mouth = rand() % 4;
                }
            }
        } else {
            CScript__2.obj[actor_no].mouth = 0;
            CScript__2.obj[actor_no].talk = false;
        }
    }

    face[actor_no].mouth = CScript__2.obj[actor_no].mouth;

    MoveImageTest(Vif1Packet, sbp, sbw, SCE_GS_PSMT8, CRect<int>(128, face[actor_no].strip_bottom - face[actor_no].mouth_height * (face[actor_no].mouth + 1), 128, face[actor_no].mouth_height), dbp, dbw, SCE_GS_PSMT8, 0, 88 - face[actor_no].mouth_bottom, 0);

    if (!Pause) {
        if (CScript__2.obj[2].mouth == 4) {
            CScript__2.obj[2].mouth = 5;
        }
    }

    sceVif1PkCnt(Vif1Packet, 0);
    sceVif1PkOpenDirectCode(Vif1Packet, 0);
    sceVif1PkOpenGifTag(Vif1Packet, *(u_long128 *) &GiftagAD);
    sceVif1PkAddGsAD(Vif1Packet, SCE_GS_TEXFLUSH, 0);
    sceVif1PkCloseGifTag(Vif1Packet);
    sceVif1PkCloseDirectCode(Vif1Packet);
}

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
void LoadMotionData() {
    char *motion_files[20][2] = {
        {"opdat/chara/01p19a1a.chr", "01p19a1a.cfg"},
        {"opdat/chara/01p19a1b.chr", "01p19a1b.cfg"},
        {"opdat/chara/01p19a2a.chr", "01p19a2a.cfg"},
        {"opdat/chara/01p19a2a.chr", "01p19a2a.cfg"},
        {"opdat/chara/01p19a2b.chr", "01p19a2b.cfg"},
        {"opdat/chara/01p19a3.chr",  "01p19a3.cfg" },
        {"opdat/chara/01p19a3b.chr", "01p19a3b.cfg"},
        {"opdat/chara/01p19a4a.chr", "01p19a4a.cfg"},
        {"opdat/chara/01p19a4b.chr", "01p19a4b.cfg"},
        {"opdat/chara/01p19a5.chr",  "01p19a5.cfg" },
        {"opdat/chara/01p17a1a.chr", "01p17a1a.cfg"},
        {"opdat/chara/01p17a1b.chr", "01p17a1b.cfg"},
        {"opdat/chara/01p17a2a.chr", "01p17a2a.cfg"},
        {"opdat/chara/01p17a2a.chr", "01p17a2a.cfg"},
        {"opdat/chara/01p17a2b.chr", "01p17a2b.cfg"},
        {"opdat/chara/01p17a3.chr",  "01p17a3.cfg" },
        {"opdat/chara/01p17a3b.chr", "01p17a3b.cfg"},
        {"opdat/chara/01p17a4a.chr", "01p17a4a.cfg"},
        {"opdat/chara/01p17a4b.chr", "01p17a4b.cfg"},
        {"opdat/chara/01p17a5.chr",  "01p17a5.cfg" }
    };

    if (DanceCnt < 10) {
        if (Chara__3[6].motion_type.state.time > (float) (Chara__3[6].motion_type.motion_info->end - 1)) {
            if (DanceCnt != 2) {
                CharaDataBuffer__2[6].used = 0;
                Chara__3[6].LoadPackData((u_int *) ((char *) read_buffer + 0x10C900), motion_files[DanceCnt][1], &CharaDataBuffer__2[4], &CharaDataBuffer__2[6], 0);
                Chara__3[7].LoadPackData(read_buffer, motion_files[DanceCnt + 10][1], &CharaDataBuffer__2[4], &CharaDataBuffer__2[6], 0);
            }

            Chara__3[6].motion_type.motion_info->start = noroi[DanceCnt].start;
            Chara__3[6].motion_type.motion_info->end = noroi[DanceCnt].end;
            Chara__3[6].motion_type.state.time = (float) Chara__3[6].motion_type.motion_info->start;
            Chara__3[7].motion_type.motion_info->start = dancer[DanceCnt].start;
            Chara__3[7].motion_type.motion_info->end = dancer[DanceCnt].end;
            Chara__3[7].motion_type.state.time = (float) Chara__3[7].motion_type.motion_info->start;
            DanceCnt++;

            if (DanceCnt != 3 && DanceCnt < 10) {
                LoadFileBG(motion_files[DanceCnt][0], (u_long128 *) ((char *) read_buffer + 0x10C900), 0);
                LoadFileBG(motion_files[DanceCnt + 10][0], (u_long128 *) read_buffer, 0);
            }
        }

        Chara__3[6].motion_no = 0;
        Chara__3[6].motion_flags = 0;
        Chara__3[6].motion_speed = -1.0f;

        if (Chara__3[6].motion_type.state.time > 225.0f) {
            DanceStart = 1;
        }

        if (DanceStart == 1) {
            Chara__3[7].motion_no = 0;
            Chara__3[7].motion_flags = 0;
            Chara__3[7].motion_speed = -1.0f;
        } else {
            Chara__3[7].motion_no = 1;
            Chara__3[7].motion_flags = 0;
            Chara__3[7].motion_speed = -1.0f;
        }
    }
}

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
void OpB_InitProcess() {
    LOADTEXTURE_INFO2 texture_list[] = {
        {"#blender#640#" HALF_BUFFER_HEIGHT_STR "#4",       0,  0},
        {"#fontbase#512#256#1",                             26, 0},
        {"#fukidashibase#640#" HALF_BUFFER_HEIGHT_STR "#4", 26, 0},
        {"#shadow_buff#640#" HALF_BUFFER_HEIGHT_STR "#4",   23, 0},
        {"#frame_image#640#" HALF_BUFFER_HEIGHT_STR "#4",   21, 0},
        {0,                                                 2,  0},
        {0,                                                 10, 0},
        {0,                                                 10, 0},
        {0,                                                 10, 0},
        {0,                                                 10, 0},
        {0,                                                 11, 0},
        {0,                                                 11, 0},
        {0,                                                 11, 0},
        {0,                                                 11, 0},
        {0,                                                 11, 0},
        {0,                                                 26, 0},
        {0,                                                 26, 0},
        {0,                                                 26, 0},
        {0,                                                 0,  0},
        {0,                                                 19, 0},
        {0,                                                 19, 0},
#ifndef PAL
        {0,                                                 19, 0},
#endif
        {0,                                                 2,  0},
        {"",                                                0,  0}
    };

    while (ReadBGSync())
        ;

    texture_list[5].name = (char *) GetPackFile(read_buffer, "p09a01.img", 0);
    texture_list[6].name = (char *) GetPackFile(read_buffer, "e01b01.img", 0);
    texture_list[7].name = (char *) GetPackFile(read_buffer, "e01b02.img", 0);
    texture_list[8].name = (char *) GetPackFile(read_buffer, "e01b03.img", 0);
    texture_list[9].name = (char *) GetPackFile(read_buffer, "e01t01.img", 0);
    texture_list[10].name = (char *) GetPackFile(read_buffer, "t0003.img", 0);
    texture_list[11].name = (char *) GetPackFile(read_buffer, "e01s03.img", 0);
    texture_list[12].name = (char *) GetPackFile(read_buffer, "e01s06.img", 0);
    texture_list[13].name = (char *) GetPackFile(read_buffer, "t0001.img", 0);
    texture_list[14].name = (char *) GetPackFile(read_buffer, "t0002.img", 0);
    texture_list[15].name = (char *) GetPackFile(read_buffer, "gaiji.img", 0);
    texture_list[16].name = (char *) GetPackFile(read_buffer, "fuki256.img", 0);
    texture_list[17].name = (char *) GetPackFile(read_buffer, "syst04.img", 0);
    texture_list[18].name = (char *) GetPackFile(read_buffer, "fire.img", 0);
    texture_list[19].name = (char *) GetPackFile(read_buffer, "pause.img", 0);

#ifdef PAL
    switch (LanguageCode) {
        case LANG_JAPANESE:
            texture_list[20].name = (char *) GetPackFile(read_buffer, "pause_e.img", 0);
            break;
        case LANG_ENGLISH_US:
            texture_list[20].name = (char *) GetPackFile(read_buffer, "pause_e.img", 0);
            break;
        case LANG_ENGLISH_UK:
            texture_list[20].name = (char *) GetPackFile(read_buffer, "pause_e.img", 0);
            break;
        case LANG_FRENCH:
            texture_list[20].name = (char *) GetPackFile(read_buffer, "pause_f.img", 0);
            break;
        case LANG_GERMAN:
            texture_list[20].name = (char *) GetPackFile(read_buffer, "pause_g.img", 0);
            break;
        case LANG_ITALIAN:
            texture_list[20].name = (char *) GetPackFile(read_buffer, "pause_i.img", 0);
            break;
        case LANG_SPANISH:
            texture_list[20].name = (char *) GetPackFile(read_buffer, "pause_s.img", 0);
            break;
    }

    texture_list[21].name = (char *) GetPackFile(read_buffer, "p09a01an.img", 0);
#else
    texture_list[20].name = (char *) GetPackFile(read_buffer, "pause_e.img", 0);
    texture_list[21].name = (char *) GetPackFile(read_buffer, "start2.img", 0);
    texture_list[22].name = (char *) GetPackFile(read_buffer, "p09a01an.img", 0);
#endif

    TexManager.Initialize(16352);
    TexManager.LoadTextureBlock(-1, texture_list);

    CharaTex__2[9] = 2;
    CharaTex__2[11] = 22;
    CharaTex__2[8] = 22;
    CharaTex__2[10] = 22;

    CSnd.SetReverb(0, 4, 30);
    CSnd.SetReverb(1, 4, 5);
    CSnd.LoadSoundFileFromPack("o02a.txt", read_buffer);
    CSnd.SetVol(MIDI_PORT_SE_TITLE, 256);
    CSnd.SetVol(MIDI_PORT_SE_DEFAULT, 256);
    CSnd.SetVol(MIDI_PORT_UNK_D, 256);
    CSnd.SetVol(MIDI_PORT_SE_SPECIAL, 256);
    OpBgmSqPort = 0;
    OpBgmPlay();
    CSnd.SQ_Play(MIDI_PORT_AMBIENT, 0);
    OpPlayVolSE(MIDI_PORT_SE_TITLE, 16, 22, 0.6f);
    CSnd.Step();
    CSnd.SE_Play(MIDI_PORT_SE_TITLE, 16, 21, 0);
    OP_FireList = 0;
    OPAnalyz("opdat/norn.cfg");
    OPMdsLoad();

    MAPOBJ_INFO norn[] = {
        {"opdat/norn/t0005.mds",    0,                         {0.0f, 0.0f, 0.0f},            {0.0f, 0.0f, 0.0f}  },
        {"opdat/norn/e01h01_0.mds", "opdat/norn/e01h01_s.mds", {-35.0f, 0.0f, -10.0f},        {0.0f, 0.0f, 0.0f}  },
        {"opdat/norn/e01h03_2.mds", "opdat/norn/e01h03_s.mds", {-15.0f, 0.0f, -50.0f},        {0.0f, 0.0f, 0.0f}  },
        {"opdat/norn/e01h10_2.mds", "opdat/norn/e01h10_s.mds", {15.0f, 0.0f, -50.0f},         {0.0f, 0.0f, 0.0f}  },
        {"opdat/norn/e01h07_0.mds", "opdat/norn/e01h07_s.mds", {5.0f, 0.0f, -10.0f},          {0.0f, 0.0f, 0.0f}  },
        {"opdat/norn/e01h08_2.mds", "opdat/norn/e01h08_s.mds", {-45.0f, 0.0f, -35.0f},        {0.0f, 0.0f, 0.0f}  },
        {"opdat/norn/t0002.mds",    0,                         {-35.0f, 0.0f, -55.0f},        {0.0f, 0.0f, 0.0f}  },
        {0,                         0,                         {-55.0f, 0.0f, -45.0f},        {0.0f, 0.0f, 0.0f}  },
        {"opdat/norn/e01t01_0.mds", 0,                         {-5.0f, 0.1f, 5.0f},           {0.0f, 0.0f, 0.0f}  },
        {"opdat/norn/e01t01_1.mds", 0,                         {15.0f, 0.1f, 5.0f},           {0.0f, 0.0f, 0.0f}  },
        {0,                         0,                         {-55.0f, 0.1f, 15.0f},         {0.0f, 0.0f, 0.0f}  },
        {"opdat/norn/e01t01_2.mds", 0,                         {-45.0f, 0.1f, -55.0f},        {0.0f, 0.0f, 0.0f}  },
        {0,                         0,                         {-55.0f, 0.1f, -65.0f},        {0.0f, 0.0f, 0.0f}  },
        {0,                         0,                         {-45.0f, 0.1f, -65.0f},        {0.0f, 0.0f, 0.0f}  },
        {0,                         0,                         {-35.0f, 0.1f, -65.0f},        {0.0f, 0.0f, 0.0f}  },
        {0,                         0,                         {-5.0f, 0.1f, -75.0f},         {0.0f, 0.0f, 0.0f}  },
        {0,                         0,                         {5.0f, 0.1f, -75.0f},          {0.0f, 0.0f, 0.0f}  },
        {0,                         0,                         {15.0f, 0.1f, -65.0f},         {0.0f, 0.0f, 0.0f}  },
        {0,                         0,                         {25.0f, 0.1f, -65.0f},         {0.0f, 0.0f, 0.0f}  },
        {0,                         0,                         {35.0f, 0.1f, -55.0f},         {0.0f, 0.0f, 0.0f}  },
        {"opdat/norn/e01c01_0.mds", 0,                         {25.0f, 0.0f, -35.0f},         {0.0f, 180.0f, 0.0f}},
        {0,                         0,                         {-55.0f, 0.0f, -25.0f},        {0.0f, 90.0f, 0.0f} },
        {0,                         0,                         {-55.0f, 0.0f, -25.0f},        {0.0f, -90.0f, 0.0f}},
        {"opdat/norn/e01c02_0.mds", 0,                         {-25.0f, 0.0f, -35.0f},        {0.0f, 90.0f, 0.0f} },
        {0,                         0,                         {-5.0f, 0.0f, -35.0f},         {0.0f, 90.0f, 0.0f} },
        {0,                         0,                         {5.0f, 0.0f, -35.0f},          {0.0f, 90.0f, 0.0f} },
        {0,                         0,                         {15.0f, 0.0f, -35.0f},         {0.0f, 90.0f, 0.0f} },
        {0,                         0,                         {25.0f, 0.0f, -15.0f},         {0.0f, 0.0f, 0.0f}  },
        {0,                         0,                         {25.0f, 0.0f, -5.0f},          {0.0f, 0.0f, 0.0f}  },
        {0,                         0,                         {25.0f, 0.0f, 5.0f},           {0.0f, 0.0f, 0.0f}  },
        {0,                         0,                         {15.0f, 0.0f, 15.0f},          {0.0f, 90.0f, 0.0f} },
        {0,                         0,                         {-5.0f, 0.0f, 15.0f},          {0.0f, 90.0f, 0.0f} },
        {0,                         0,                         {-15.0f, 0.0f, 15.0f},         {0.0f, 90.0f, 0.0f} },
        {0,                         0,                         {-35.0f, 0.0f, 15.0f},         {0.0f, 90.0f, 0.0f} },
        {0,                         0,                         {-45.0f, 0.0f, 25.0f},         {0.0f, 0.0f, 0.0f}  },
        {0,                         0,                         {-45.0f, 0.0f, 35.0f},         {0.0f, 0.0f, 0.0f}  },
        {0,                         0,                         {-45.0f, 0.0f, 45.0f},         {0.0f, 0.0f, 0.0f}  },
        {"opdat/norn/e01c06_0.mds", 0,                         {-35.0f, 0.0f, -35.0f},        {0.0f, 90.0f, 0.0f} },
        {"opdat/norn/e01c07_0.mds", 0,                         {-15.0f, 0.0f, -35.0f},        {0.0f, -90.0f, 0.0f}},
        {0,                         0,                         {25.0f, 0.0f, -25.0f},         {0.0f, 0.0f, 0.0f}  },
        {0,                         0,                         {5.0f, 0.0f, 15.0f},           {0.0f, 90.0f, 0.0f} },
        {0,                         0,                         {0.0f, 0.0f, 15.0f},           {0.0f, 90.0f, 0.0f} },
        {"opdat/norn/e01r01_0.mds", 0,                         {-55.0f, 0.05f, -35.0f},       {0.0f, -90.0f, 0.0f}},
        {0,                         0,                         {-55.0f, 0.05f, -25.0f},       {0.0f, 90.0f, 0.0f} },
        {"opdat/norn/e01r02_0.mds", 0,                         {-45.0f, 0.05f, -25.0f},       {0.0f, 90.0f, 0.0f} },
        {0,                         0,                         {-35.0f, 0.05f, -25.0f},       {0.0f, 90.0f, 0.0f} },
        {0,                         0,                         {-25.0f, 0.05f, -25.0f},       {0.0f, 90.0f, 0.0f} },
        {0,                         0,                         {-5.0f, 0.05f, -25.0f},        {0.0f, 90.0f, 0.0f} },
        {0,                         0,                         {5.0f, 0.05f, -25.0f},         {0.0f, 90.0f, 0.0f} },
        {0,                         0,                         {15.0f, 0.05f, -25.0f},        {0.0f, 90.0f, 0.0f} },
        {0,                         0,                         {-15.0f, 0.05f, -15.0f},       {0.0f, 0.0f, 0.0f}  },
        {"opdat/norn/e01r03_0.mds", 0,                         {-15.0f, 0.05f, -25.0f},       {0.0f, 0.0f, 0.0f}  },
        {"opdat/norn/e01r06_0.mds", 0,                         {-65.0f, 0.05f, -35.0f},       {0.0f, 90.0f, 0.0f} },
        {0,                         0,                         {-15.0f, 0.05f, -5.0f},        {0.0f, 180.0f, 0.0f}},
        {"opdat/norn/t0004.mds",    0,                         {-22.9331f, 0.05f, 9.8816f},   {0.0f, 0.0f, 0.0f}  },
        {0,                         0,                         {-26.9302f, 0.05f, 9.8816f},   {0.0f, 0.0f, 0.0f}  },
        {0,                         0,                         {-26.9302f, 0.05f, 20.2919f},  {0.0f, 0.0f, 0.0f}  },
        {0,                         0,                         {-22.9331f, 0.05f, 20.2919f},  {0.0f, 0.0f, 0.0f}  },
        {0,                         0,                         {3.0797f, 0.05f, 20.2919f},    {0.0f, 0.0f, 0.0f}  },
        {0,                         0,                         {7.0849f, 0.05f, 20.2919f},    {0.0f, 0.0f, 0.0f}  },
        {0,                         0,                         {7.0849f, 0.05f, 9.8816f},     {0.0f, 0.0f, 0.0f}  },
        {0,                         0,                         {3.0797f, 0.05f, 9.8816f},     {0.0f, 0.0f, 0.0f}  },
        {0,                         0,                         {19.8785f, 0.05f, -22.8436f},  {0.0f, 0.0f, 0.0f}  },
        {0,                         0,                         {19.8785f, 0.05f, -27.0007f},  {0.0f, 0.0f, 0.0f}  },
        {0,                         0,                         {-12.8131f, 0.05f, -28.8014f}, {0.0f, 0.0f, 0.0f}  },
        {0,                         0,                         {-16.9416f, 0.05f, -28.8014f}, {0.0f, 0.0f, 0.0f}  },
        {"opdat/norn/e01g02_0.mds", 0,                         {0.0f, 0.0f, 0.0f},            {0.0f, 0.0f, 0.0f}  },
        {"opdat/norn/e01g03_0.mds", 0,                         {0.0f, 0.0f, 0.0f},            {0.0f, 0.0f, 0.0f}  }
    };
    CFrameAttr  attr;
    CFrameVu1  *frame;
    CMapObject *object;

    attr.fog_enable = true;
    MapDataBuffer.used = 0;

    for (int i = 0; i < 68; i++) {
        if (norn[i].name) {
            LoadFile(norn[i].name, (void *) read_buffer, 0);
            frame = LoadMDSFile(read_buffer, &MapDataBuffer, 2, 0, 0);
        }

        if (i == 4) {
            DoransFuusya[0] = frame;
        }

        if (i == 5) {
            DoransFuusya[1] = frame;
        }

        if (i >= 54 && i < 66) {
            TaimatsuFrame[i - 54] = frame;
        }

        frame->SetAttr(attr, 1, 64);
        SetFrameAttr(frame, 1);

        object = &OP_NornMapObj[i];

        object->Initialize();
        object->SetFrame(frame, 0);
        OP_NornMapObj[i].handle = 0;
        OP_NornMapObj[i].unk_34 = 0;

        object->SetPosition(CVector3_f_(10.0f * norn[i].position[0], 10.0f * norn[i].position[1], 10.0f * norn[i].position[2]));
        object->SetRotation(CVector3_f_((float) (PI_D * norn[i].rotation[0] / 180), (float) (PI_D * norn[i].rotation[1] / 180), (float) (PI_D * norn[i].rotation[2] / 180)));

        object->FrameObjectOnOff("win1", 0);
        object->FrameObjectOnOff("light1", 0);

        if (norn[i].shadow_name) {
            LoadFile(norn[i].shadow_name, (void *) read_buffer, 0);
            object->shadow_frame = LoadMDSFile(read_buffer, &MapDataBuffer, 14, 0, 0);
            object->shadow_offset = -20.0f;
        }
    }

    MAPOBJ_INFO ground[] = {
        {"opdat/norn/t0006.mds", 0, {0.0f, 0.0f, 0.0f},            {0.0f, 0.0f, 0.0f}},
        {"opdat/norn/t0001.mds", 0, {-25.0203f, 0.03f, -21.3403f}, {0.0f, 0.0f, 0.0f}},
        {0,                      0, {-35.0203f, 0.03f, -21.3403f}, {0.0f, 0.0f, 0.0f}},
        {0,                      0, {-45.0203f, 0.03f, -21.3403f}, {0.0f, 0.0f, 0.0f}},
        {0,                      0, {-55.0203f, 0.03f, -21.3403f}, {0.0f, 0.0f, 0.0f}},
        {0,                      0, {-45.0203f, 0.03f, -28.3403f}, {0.0f, 0.0f, 0.0f}},
        {0,                      0, {-35.0203f, 0.03f, -28.3403f}, {0.0f, 0.0f, 0.0f}},
        {0,                      0, {-25.0203f, 0.03f, -28.3403f}, {0.0f, 0.0f, 0.0f}},
        {0,                      0, {-5.0203f, 0.03f, -28.3403f},  {0.0f, 0.0f, 0.0f}},
        {0,                      0, {4.9797f, 0.03f, -28.3403f},   {0.0f, 0.0f, 0.0f}},
        {0,                      0, {14.9797f, 0.03f, -21.3403f},  {0.0f, 0.0f, 0.0f}},
        {0,                      0, {14.9797f, 0.03f, -28.3403f},  {0.0f, 0.0f, 0.0f}},
        {0,                      0, {4.9797f, 0.03f, -21.3403f},   {0.0f, 0.0f, 0.0f}},
        {0,                      0, {-5.0203f, 0.03f, -21.3403f},  {0.0f, 0.0f, 0.0f}},
        {"opdat/norn/t0003.mds", 0, {-50.0f, 0.03f, -21.2932f},    {0.0f, 0.0f, 0.0f}},
        {0,                      0, {-40.0f, 0.03f, -21.2932f},    {0.0f, 0.0f, 0.0f}},
        {0,                      0, {-30.0f, 0.03f, -21.2932f},    {0.0f, 0.0f, 0.0f}},
        {0,                      0, {0.0f, 0.03f, -21.2932f},      {0.0f, 0.0f, 0.0f}},
        {0,                      0, {10.0f, 0.03f, -21.2932f},     {0.0f, 0.0f, 0.0f}},
        {0,                      0, {10.0f, 0.03f, -28.2932f},     {0.0f, 0.0f, 0.0f}},
        {0,                      0, {-30.0f, 0.03f, -28.2932f},    {0.0f, 0.0f, 0.0f}},
        {0,                      0, {0.0f, 0.03f, -28.2932f},      {0.0f, 0.0f, 0.0f}},
        {0,                      0, {-40.0f, 0.03f, -28.2932f},    {0.0f, 0.0f, 0.0f}}
    };

    for (int i = 0; i < 23; i++) {
        if (ground[i].name) {
            LoadFile(ground[i].name, (void *) read_buffer, 0);
            frame = LoadMDSFile(read_buffer, &MapDataBuffer, 2, 0, 0);
        }

        if (i > 0) {
            frame->SetAttr(attr, 1, 64);
            SetFrameAttr(frame, 1);
        }

        if (i == 0) {
            CFrame       *sun = frame->SearchFrame("sun3");
            sceVu0FVECTOR position;

            sceVu0CopyVector(position, sun->position);
            position[1] += 30.0f;
            position[2] -= 300.0f;
            sun->SetPosition(position);
        }

        object = &OP_NornMapObj2[i];

        object->Initialize();
        object->SetFrame(frame, 0);
        OP_NornMapObj2[i].handle = 0;
        OP_NornMapObj2[i].unk_34 = 0;

        object->SetPosition(CVector3_f_(10.0f * ground[i].position[0], 10.0f * ground[i].position[1], 10.0f * ground[i].position[2]));
        ((CMapObject &) OP_NornMapObj2[i]).SetRotation(CVector3_f_((float) (PI_D * ground[i].rotation[0] / 180), (float) (PI_D * ground[i].rotation[1] / 180), (float) (PI_D * ground[i].rotation[2] / 180)));
    }

    LoadFile("opdat/chara/03p09a.chr", (void *) read_buffer, 0);
    CharaDataBuffer__2[6].used = 0;
    Chara__3[9].LoadPackData(read_buffer, "03p09a.cfg", &CharaDataBuffer__2[6], 0);

    CFrameAttr chara_attr;

    chara_attr.clip_enable = false;
    Chara__3[9].frame->SetAttr(chara_attr, 1, 4);
    Chara__3[9].motion_type.state.time = 10.0f;
    Chara__3[9].motion_type.state.blend_step = 0.05f;
    Chara__3[9].motion_type.state.motion_no = 0;
    Chara__3[9].motion_type.state.playing_no = 0;

    Fuusya[0].Initialize();
    Fuusya[0].property = 0;
    Fuusya[0].mode = 0;
    Fuusya[0].start_value[2] = 0.0f;
    Fuusya[0].start_value[1] = 0.0f;
    Fuusya[0].start_value[0] = 0.0f;
    Fuusya[0].end_value[2] = 0.0f;
    Fuusya[0].end_value[1] = 0.0f;
    Fuusya[0].end_value[0] = 0.0f;
    Fuusya[0].step_y = 0.0f;
    Fuusya[0].step_x = 0.0f;
    Fuusya[0].step_z = -0.5f;
    strcpy(Fuusya[0].name, "hane");
    InitObjAnime(DoransFuusya[0], &Fuusya[0]);

    Fuusya[1].Initialize();
    Fuusya[1].property = 0;
    Fuusya[1].mode = 0;
    Fuusya[1].start_value[2] = 0.0f;
    Fuusya[1].start_value[1] = 0.0f;
    Fuusya[1].start_value[0] = 0.0f;
    Fuusya[1].end_value[2] = 0.0f;
    Fuusya[1].end_value[1] = 0.0f;
    Fuusya[1].end_value[0] = 0.0f;
    Fuusya[1].step_y = 0.0f;
    Fuusya[1].step_x = 0.0f;
    Fuusya[1].step_z = -0.5f;
    strcpy(Fuusya[1].name, "hane");
    InitObjAnime(DoransFuusya[1], &Fuusya[1]);

    for (int i = 0; i < 12; i++) {
        Taimatsu[i].Initialize();
        Taimatsu[i].property = 3;
        Taimatsu[i].mode = 4;
        Taimatsu[i].start_value[2] = 80.0f;
        Taimatsu[i].start_value[1] = 80.0f;
        Taimatsu[i].start_value[0] = 80.0f;
        Taimatsu[i].end_value[2] = 128.0f;
        Taimatsu[i].end_value[1] = 128.0f;
        Taimatsu[i].end_value[0] = 128.0f;
        strcpy(Taimatsu[i].name, "effect");
        InitObjAnime(TaimatsuFrame[i], &Taimatsu[i]);
    }

    VolFade = 0;
    CScript__2.init_no = 0;
}

/* The second half of the scene, which starts when the camera cuts to Toan's house. Everything the
   first half loaded is thrown away and the pack the background read left in memory is unpacked
   again: the block numbers say which of the registry's blocks each image belongs to, and the five
   named entries are the fixed surfaces the registry keeps for every scene. The three actors are
   loaded one at a time rather than through a table because each is followed by set-up of its own —
   Toan's mother has a frame turned off, Toan himself carries the cloth the wind drives, and the
   second Toan is the one the door animation is timed against. */
void OpB_InitProcess2() {
    LOADTEXTURE_INFO2 texture_list[] = {
        {"#blender#640#" HALF_BUFFER_HEIGHT_STR "#4",       0,  0},
        {"#fontbase#512#256#1",                             26, 0},
        {"#fukidashibase#640#" HALF_BUFFER_HEIGHT_STR "#4", 26, 0},
        {"#shadow_buff#640#" HALF_BUFFER_HEIGHT_STR "#4",   23, 0},
        {"#frame_image#640#" HALF_BUFFER_HEIGHT_STR "#4",   21, 0},
        {0,                                                 26, 0},
        {0,                                                 26, 0},
        {0,                                                 26, 0},
        {0,                                                 10, 0},
        {0,                                                 2,  0},
        {0,                                                 22, 0},
        {0,                                                 22, 0},
        {0,                                                 22, 0},
        {0,                                                 22, 0},
        {0,                                                 0,  0},
        {0,                                                 19, 0},
        {0,                                                 19, 0},
#ifndef PAL
        {0,                                                 19, 0},
#endif
        {0,                                                 2,  0},
        {"",                                                0,  0}
    };

    while (ReadBGSync())
        ;

    texture_list[5].name = (char *) GetPackFile(read_buffer, "gaiji.img", 0);
    texture_list[6].name = (char *) GetPackFile(read_buffer, "fuki256.img", 0);
    texture_list[7].name = (char *) GetPackFile(read_buffer, "syst04.img", 0);
    texture_list[8].name = (char *) GetPackFile(read_buffer, "i01h01n.img", 0);
    texture_list[9].name = (char *) GetPackFile(read_buffer, "p09a01.img", 0);
    texture_list[10].name = (char *) GetPackFile(read_buffer, "p10a01.img", 0);
    texture_list[11].name = (char *) GetPackFile(read_buffer, "c01d01.img", 0);
    texture_list[12].name = (char *) GetPackFile(read_buffer, "03c01d.img", 0);
    texture_list[13].name = (char *) GetPackFile(read_buffer, "03komono.img", 0);
    texture_list[14].name = (char *) GetPackFile(read_buffer, "fire.img", 0);
    texture_list[15].name = (char *) GetPackFile(read_buffer, "pause.img", 0);

#ifdef PAL
    switch (LanguageCode) {
        case LANG_JAPANESE:
            texture_list[16].name = (char *) GetPackFile(read_buffer, "pause_e.img", 0);
            break;
        case LANG_ENGLISH_US:
            texture_list[16].name = (char *) GetPackFile(read_buffer, "pause_e.img", 0);
            break;
        case LANG_ENGLISH_UK:
            texture_list[16].name = (char *) GetPackFile(read_buffer, "pause_e.img", 0);
            break;
        case LANG_FRENCH:
            texture_list[16].name = (char *) GetPackFile(read_buffer, "pause_f.img", 0);
            break;
        case LANG_GERMAN:
            texture_list[16].name = (char *) GetPackFile(read_buffer, "pause_g.img", 0);
            break;
        case LANG_ITALIAN:
            texture_list[16].name = (char *) GetPackFile(read_buffer, "pause_i.img", 0);
            break;
        case LANG_SPANISH:
            texture_list[16].name = (char *) GetPackFile(read_buffer, "pause_s.img", 0);
            break;
    }

    texture_list[17].name = (char *) GetPackFile(read_buffer, "p09a01an.img", 0);
#else
    texture_list[16].name = (char *) GetPackFile(read_buffer, "pause_e.img", 0);
    texture_list[17].name = (char *) GetPackFile(read_buffer, "start2.img", 0);
    texture_list[18].name = (char *) GetPackFile(read_buffer, "p09a01an.img", 0);
#endif

    TexManager.Initialize(16352);
    TexManager.LoadTextureBlock(-1, texture_list);

    OP_FireList = 0;
    OPAnalyz("opdat/toan.cfg");
    OPMdsLoad();

    LoadFile("opdat/toan/t0101.mds", (void *) read_buffer, 0);
    MapDataBuffer.used = 0;
    ToansHouse = LoadMDSFile(read_buffer, &MapDataBuffer, 2, 0, 0);
    OP_ToanMapObj.Initialize();
    OP_ToanMapObj.SetFrame(ToansHouse, 0);
    OP_ToanMapObj.handle = 0;
    OP_ToanMapObj.unk_34 = 0;

    LoadFile("opdat/toan/03komono.chr", (void *) read_buffer, 0);
    Komono.LoadPackData(read_buffer, "03komono.cfg", &MapDataBuffer, 0);

    CFrameAttr komono_attr;

    komono_attr.clip_enable = false;
    Komono.frame->SetAttr(komono_attr, 1, 4);
    Komono.motion_type.state.time = 10.0f;
    Komono.motion_type.state.blend_step = 0.05f;
    Komono.motion_type.state.motion_no = 0;
    Komono.motion_type.state.playing_no = 0;

    LoadFile("opdat/chara/03p10a.chr", (void *) read_buffer, 0);
    Chara__3[10].LoadPackData(read_buffer, "03p10a.cfg", &CharaDataBuffer__2[6], 0);

    CFrameAttr mother_attr;

    mother_attr.clip_enable = false;
    Chara__3[10].frame->SetAttr(mother_attr, 1, 4);
    Chara__3[10].motion_type.state.time = 10.0f;
    Chara__3[10].motion_type.state.blend_step = 0.05f;
    Chara__3[10].motion_type.state.motion_no = 0;
    Chara__3[10].motion_type.state.playing_no = 0;

    CFrame *soup = Chara__3[10].frame->SearchFrame("soup_nakami2");

    if (soup) {
        soup->attr.draw_on = 0;
    }

    LoadFile("opdat/chara/03c01d.chr", (void *) read_buffer, 0);
    Chara__3[8].LoadPackData(read_buffer, "03c01d.cfg", &CharaDataBuffer__2[6], 0);

    CFrameAttr toan_attr;

    toan_attr.clip_enable = false;
    Chara__3[8].frame->SetAttr(toan_attr, 1, 4);
    Chara__3[8].motion_type.state.time = 10.0f;
    Chara__3[8].motion_type.state.blend_step = 0.05f;
    Chara__3[8].motion_type.state.motion_no = 0;
    Chara__3[8].motion_type.state.playing_no = 0;

    sceVu0FVECTOR wind_dir;

    wind_dir[3] = 0.0f;
    wind_dir[1] = 0.0f;
    wind_dir[2] = 0.0f;
    wind_dir[0] = 0.0f;
    Wind.SetDir(wind_dir);
    Wind.SetVelocity(0.0f);
    Chara__3[8].wind = (int) &Wind;
    Chara__3[8].ClothStep(-1);

    LoadFile("opdat/chara/03c01d2.chr", (void *) read_buffer, 0);
    Chara__3[11].LoadPackData(read_buffer, "03c01d2.cfg", &CharaDataBuffer__2[6], 0);

    toan_attr.clip_enable = false;
    Chara__3[11].frame->SetAttr(toan_attr, 1, 4);
    Chara__3[11].motion_type.state.time = 160.0f;
    Chara__3[11].motion_type.state.blend_step = 0.1f;
    Chara__3[11].motion_type.state.motion_no = 0;
    Chara__3[11].motion_type.state.playing_no = 0;

    Door.Initialize();
    Door.property = 0;
    Door.mode = 3;
    Door.start_value[0] = 0.0f;
    Door.start_value[1] = 0.0f;
    Door.start_value[2] = 0.0f;
    Door.end_value[0] = 0.0f;
    Door.end_value[1] = 0.0f;
    Door.end_value[2] = 0.0f;
    Door.step_x = 0.0f;
    Door.step_y = 0.0f;
    Door.step_z = 0.0f;
    strcpy(Door.name, "door_1");
    InitObjAnime(ToansHouse, &Door);

    CSnd.SE_Stop(MIDI_PORT_SE_TITLE, 16, 22, 0);
    CSnd.SE_Stop(MIDI_PORT_SE_TITLE, 16, 21, 0);
    CSnd.SetVol(MIDI_PORT_BGM, (float) (OpGetVolSQ(0) * 0.5));
    CSnd.SetVol(MIDI_PORT_AMBIENT, (float) (OpGetVolSQ(1) * 0.1));
    CScript__2.init_no = 0;
}

/* The scene's per-tick motion. Four of the actors are told what the script last asked of them —
   the loop is over the four the scene animates rather than over all of them — and a motion that
   has run past its last key falls through to whatever motion was queued behind it. Then the three
   models the camera's own frame tree carries drive three more actors: each is found by name in the
   camera model, and its world transform becomes that actor's position and heading. */
void OpB_MotionProcess() {
    for (int i = 8; i < 12; i++) {
        if (CScript__2.obj[i].disp) {
            if (CScript__2.obj[i].motion_end != -1) {
                if (Chara__3[i].motion_type.state.time > (float) (Chara__3[i].motion_type.motion_info[CScript__2.obj[i].motion].end - 1)) {
                    CScript__2.obj[i].motion = CScript__2.obj[i].motion_end;
                    CScript__2.obj[i].motion_end = -1;
                }
            }

            Chara__3[i].motion_type.state.blend_step = CScript__2.obj[i].step;
            Chara__3[i].motion_no = CScript__2.obj[i].motion;
            Chara__3[i].motion_flags = 0;
            Chara__3[i].motion_speed = -1.0f;
        }
    }

    char         *frame_names[4] = {"c01d", "p09a", "p10a", "c01d"};
    sceVu0FMATRIX matrix;
#ifndef PAL
    float zero = 0.0f;
#endif
    sceVu0FVECTOR wind_dir;

    for (int i = 0; i < 4; i++) {
        CFrame *frame = Cam__2[SceneNp__2].frame->SearchFrame(frame_names[i]);

        if (frame) {
            frame->GetLWMatrix(matrix);
#ifdef PAL
            float rot_y = atan2f(matrix[2][0], matrix[2][2]);
            float rot_x = 0.0f, rot_z = 0.0f;

            Chara__3[i + 8].SetRotation(rot_x, rot_y, rot_z);
#else
            Chara__3[i + 8].SetRotation(zero, atan2f(matrix[2][0], matrix[2][2]), zero);
#endif
            float x = matrix[3][0];
            float y = matrix[3][1];
            float z = matrix[3][2];

            Chara__3[i + 8].SetPosition(x, y, z);
        }
    }

    wind_dir[0] = 0.0f;
    wind_dir[2] = 0.0f;
    wind_dir[1] = 0.0f;
    wind_dir[3] = 0.0f;
    Wind.SetDir(wind_dir);
    Wind.SetVelocity(0.0f);

    static int camera = 0;

    if (camera != CScript__2.camera_start) {
        camera = CScript__2.camera_start;

        if (camera == 51) {
            Chara__3[8].ClothStep(-1);

            for (int i = 0; i < 10; i++) {
                Chara__3[8].ClothStep(0);
            }
        }

        if (camera == 57) {
            Chara__3[11].ClothStep(-1);

            for (int i = 0; i < 10; i++) {
                Chara__3[11].ClothStep(0);
            }
        }
    }

    if (CScript__2.scene == OP_SCENE_NORUNE && !Pause) {
        ObjAnimePlay(&Fuusya[0]);
        ObjAnimePlay(&Fuusya[1]);

        for (int i = 0; i < 12; i++) {
            ObjAnimePlay(&Taimatsu[i]);
        }
    }

    if (CScript__2.scene == OP_SCENE_TOAN_HOUSE) {
        if (CScript__2.sprite == 1) {
            Door.end_value[1] = -85.0f;
            Door.step_y = -2.8f;
        }

        if (CScript__2.sprite == 2) {
            Door.end_value[1] = 0.0f;
            Door.step_y = 1.2f;
        }

        if (!Pause) {
            ObjAnimePlay(&Door);
        }
    }
}

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
void OpB_DrawProcess() {
    RenderInfo *info = &mgRenderInfo;

    MGSetRenderInfo(info->scale[0], 0.5f, 0xffff);
    TexManager.ReloadTexture(Vif1Packet, 10);

    switch (CScript__2.scene) {
        case OP_SCENE_NORUNE:
            for (int i = 0; i < 68; i++) {
                CMapObject *obj = &OP_NornMapObj[i];

                obj->Draw();
            }

            TexManager.ReloadTexture(Vif1Packet, 11);

            for (int i = 0; i < 23; i++) {
                CMapObject *obj = &OP_NornMapObj2[i];

                obj->Draw();
            }

            break;

        case OP_SCENE_TOAN_HOUSE:
            setTexAnime();
            OP_ToanMapObj.Draw();
            break;
    }

    if (OP_FireList > 0) {
        sceVu0FVECTOR eye;

        TexManager.ReloadTexture(GetVif1Packet(), 0);
        OP_CharaFrame__2 = Cam__2[SceneNp__2].frame;
        sceVu0CopyVector(eye, OP_CharaFrame__2->position);

        if (!Pause) {
            CFire.FireStep();
        }

        CFire.FireCreate();

        for (int i = 0; i < OP_FireList; i++) {
            float z = OP_FirePosition[i][2];
            float y = OP_FirePosition[i][1];
            float x = OP_FirePosition[i][0];

            CFire.pos[0] = 10.0f * x;
            CFire.pos[1] = 10.0f * y;
            CFire.pos[2] = 10.0f * z;
            CFire.pos[3] = 1.0f;

            int fire_flag = OP_FireFlg[i];

            if (fire_flag == 1) {
                CFire.DrawFire(1, 1, &OP_MainCamera, eye, OP_FireScale[i], 3, 15.0f);
            } else {
                CFire.DrawFire(1, 1, &OP_MainCamera, eye, OP_FireScale[i], 2, 15.0f);
            }
        }
    }

    if (CScript__2.sprite == 3) {
        CFrame *frame = Chara__3[8].frame->SearchFrame("soup_nakami");

        if (frame) {
            frame->attr.draw_on = 0;
        }
    }

    TexManager.ReloadTexture(Vif1Packet, 23);
    MGBeginDrawShadow(*(sceGsTex0 *) &TexManager.GetTexture("shadow_buff", -1)->tex0);

    for (int i = 8; i < 12; i++) {
        if (CScript__2.obj[i].disp) {
            if (!Pause) {
                Chara__3[i].ShadowStep();
            }

            Chara__3[i].DrawShadow();
        }
    }

    MGEndDrawShadow(52);

    for (int i = 8; i < 12; i++) {
        if (CScript__2.obj[i].disp) {
            TexManager.ReloadTexture(Vif1Packet, CharaTex__2[i]);

            if (!Pause) {
                Chara__3[i].Step();
                Chara__3[i].ClothStep(0);
            }

            if (i == 9) {
                FaceChangeC(i);
            }

            Chara__3[i].Draw();
        }
    }

    if (CScript__2.scene == OP_SCENE_TOAN_HOUSE) {
        TexManager.ReloadTexture(Vif1Packet, 22);

        if (!Pause) {
            Komono.Step();
        }

        Komono.Draw();
    }

    if (CScript__2.scene == OP_SCENE_NORUNE) {
        TexManager.ReloadTexture(Vif1Packet, 23);
        MGBeginDrawShadow(*(sceGsTex0 *) &TexManager.GetTexture("shadow_buff", -1)->tex0);

        switch (CScript__2.scene) {
            case OP_SCENE_NORUNE:
                for (int i = 0; i < 68; i++) {
                    OP_NornMapObj[i].DrawShadow(0);
                }

                break;

            case OP_SCENE_TOAN_HOUSE:
                OP_ToanMapObj.DrawShadow(0);
                break;
        }

        MGEndDrawShadow(52);
    }

    if (CScript__2.scene != OP_SCENE_TOAN_HOUSE) {
        TexManager.ReloadTexture(Vif1Packet, 21);

        float dof[2] = {300.0f, 1000.0f};

        DepthOfField(dof, 2, 64, 0);
    }
}

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
