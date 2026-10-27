#pragma once

#include "common.h"

#include "collision.hpp"
#include "dataalloc_fwd.hpp"
#include "frame.hpp"
#include "texture.hpp"

namespace dc::game {

/**
 * Font colours of FontColorTbl.
 */
// clang-format off
enum FontColor {
    FONT_COLOR_WHITE        = 0x80BFBFBF, /**< White. */
    FONT_COLOR_BROWN        = 0x80304045, /**< Dark brown. */
    FONT_COLOR_YELLOW       = 0x8040BDBD, /**< Yellow. */
    FONT_COLOR_CYAN         = 0x80BDBD40, /**< Cyan. */
    FONT_COLOR_GREEN        = 0x8040BD40, /**< Green. */
    FONT_COLOR_BROWN_OPAQUE = 0xFF304045, /**< Opaque dark brown. */
    FONT_COLOR_GOLD         = 0x8066CEE7, /**< Gold. */
    FONT_COLOR_GREY         = 0x808F8F8F, /**< Grey. */
    FONT_COLOR_MAGENTA      = 0x80BF3FBF, /**< Magenta. */
};

// clang-format on

/**
 * What a motion key list sets, as Mot_List::type holds it.
 */
// clang-format off
enum MotionKeyType {
    MOTION_KEY_ROTATION        = 0,   /**< Rotation. */
    MOTION_KEY_SCALE           = 1,   /**< Scale. */
    MOTION_KEY_TRANSLATION     = 2,   /**< Translation. */
    MOTION_KEY_VERTEX          = 12,  /**< Vertices. */
    MOTION_KEY_CAMERA_POSITION = 30,  /**< Camera position. */
    MOTION_KEY_CAMERA_TARGET   = 31,  /**< Camera target. */
    MOTION_KEY_CAMERA_ROLL     = 32,  /**< Camera roll. */
    MOTION_KEY_CAMERA_FOV      = 33,  /**< Camera field of view. */
    MOTION_KEY_MATERIAL_ALPHA  = 40,  /**< Material alpha. */
    MOTION_KEY_MATERIAL_COLOR  = 41,  /**< Material colour. */
    MOTION_KEY_VISIBLE         = 50,  /**< Visibility. */
    MOTION_KEY_UNK_33          = 51,  /**< Visibility, with the other pair of draw modes. */
    MOTION_KEY_SKIP            = 200, /**< Skipped. */
};

// clang-format on

class CCharacter;

/**
 * @file
 * Declares the motion data that a character plays, and the calls that step it.
 */

/**
 * Palette the message window's font draws out of.
 */
extern u32 MesWinClut[256];

/**
 * Where each external character of the message window's font sits, one row of eight words each.
 */
extern s32 GaijiDataTbl[176][8];

/**
 * Colour each palette entry of the message window's font draws with.
 */
extern u32 FontColorTbl[16];

/**
 * Whether the first message window is still being built, one step per frame.
 */
extern int Mes1MakeFlg;

/**
 * Whether the second message window is still being built, one step per frame.
 */
extern int Mes2MakeFlg;

/**
 * Whether the message windows skip drawing themselves entirely.
 */
extern int MesAbsDrawOff;

class CCamera;
class CRect_i_;
struct RECT;
struct i;

/**
 * Connects the EE client to the EZMIDI RPC server.
 */
int ezMidiInit();

/**
 * Sends one command to the EZMIDI RPC server and returns its response word.
 */
int ezMidi(int command, int argument);

/**
 * Transfers a buffer from EE memory into IOP memory and waits for completion.
 */
int ezTransToIOP(void *iop_address, void *ee_address, int size);

/**
 * Turns a model frame gradually towards a world-space target position.
 */
int LookAt(CFrameVu1 *frame, float *target, _FRAMECONSTRAINT constraint);

/**
 * Turns a model frame gradually towards another frame's world position.
 */
int LookAt(CFrameVu1 *frame, CFrameVu1 *target, _FRAMECONSTRAINT constraint);

/**
 * Opens a batch of textured two-dimensional sprites in a VIF1 packet.
 */
void set2DSprite_Start(sceVif1Packet *packet, CTexture *texture);

/**
 * Closes the current two-dimensional sprite batch and fixes its tag lengths.
 */
void set2DSprite_End(sceVif1Packet *packet, CTexture *texture);

/**
 * Uploads a texture's 16-by-16 colour lookup table through a VIF1 packet.
 */
void SetClut(sceVif1Packet *packet, CTexture *texture, i *clut);

/**
 * Describes one motion of a model: the range of frames it covers and how fast
 * it plays.
 */
struct MOTION_INFO {
    s32   start; /**< First frame of the motion. */
    s32   end;   /**< Frame after the last one of the motion. */
    float speed; /**< Frames that the motion plays each step. */
    s32   unk_0C;
};

STATIC_ASSERT(sizeof(MOTION_INFO) == 0x10);

/**
 * Records how far one model has played through the motion it holds.
 */
struct MOTION_STATE {
    float            time;            /**< Frame that the motion stands on. */
    float            blend;           /**< How far the pose has blended toward the next frame or motion; 1 is all the way. */
    float            blend_step;      /**< Amount the blend advances each step while changing motion. */
    unsigned int     frame;           /**< Whole frame that the model is posed from. */
    unsigned int     next_frame;      /**< Frame that the pose interpolates toward. */
    s32              motion_no;       /**< Motion that the character asks for. */
    s32              playing_no;      /**< Motion that plays now. */
    s32              blending;        /**< Set while the model blends into a newly requested motion. */
    CCamera         *camera;          /**< Camera that the motion moves, or nullptr where it moves none. */
    s32              look_at;         /**< Set when a frame of the model turns to look at a target. */
    CFrameVu1       *look_frame;      /**< Frame that turns to look. */
    CFrameVu1       *look_target;     /**< Frame looked at, or nullptr to look at look_position. */
    sceVu0FVECTOR    look_position;   /**< Point looked at when there is no target frame. */
    _FRAMECONSTRAINT look_constraint; /**< Axes the looking frame may turn about. */
    u8               unk_44[0xC];
} __attribute__((aligned(16)));

STATIC_ASSERT(sizeof(MOTION_STATE) == 0x50);

/**
 * Describes the frames of one model that a motion drives.
 */
struct tagFRAME_INF {
    s32            parent_frame;  /**< Parent frame used to build the driven frame's transform. */
    u32            vertex_count;  /**< Number of visual vertices copied for vertex motion. */
    sceVu0FVECTOR *base_vertices; /**< Arena copy of the visual's undeformed vertices. */
    u8             unk_0C[4];
    sceVu0FMATRIX  matrix;           /**< Transform the driver interpolates into the frame. */
    sceVu0FMATRIX  bone_base_matrix; /**< Inverse bind pose of the bone that skins the frame. */
    sceVu0FMATRIX  bone_matrix;      /**< Accumulated transform of the bone that skins the frame. */
};

STATIC_ASSERT(sizeof(tagFRAME_INF) == 0xD0);

/**
 * Holds one key of a frame driver: a motion frame and the value the driver
 * takes there.
 */
struct Mot_Key {
    u32           frame; /**< Motion frame that the key stands on. */
    u8            unk_04[12];
    sceVu0FVECTOR value; /**< What the driver sets on that frame. */
};

STATIC_ASSERT(sizeof(Mot_Key) == 0x20);

/**
 * Drives one frame of a model from a run of motion keys.
 */
struct Mot_List {
    u32       frame;     /**< Frame of the model that the keys drive. */
    u32       target;    /**< Part of the frame that the keys drive: a vertex, a material or a bone. */
    s32       type;      /**< What the keys set. @see MotionKeyType. */
    u32       key_count; /**< Number of keys. */
    Mot_Key  *keys;      /**< The keys, in frame order. */
    Mot_List *next;      /**< The next driver, or nullptr after the last. */
    u8        unk_18[8];
};

STATIC_ASSERT(sizeof(Mot_List) == 0x20);

/**
 * Heads one driver's keys in a motion file; the keys follow it.
 */
struct Mot_File_List {
    u32 frame;  /**< Frame of the model that the keys drive. */
    u32 target; /**< Part of the frame that the keys drive. */
    s32 type;   /**< What the keys set, as Mot_List::type. */
    u8  unk_0C[4];
    u32 key_count; /**< Number of keys after the header. */
    u32 more;      /**< Nonzero when another driver follows this one's keys. */
    u8  unk_18[8];
};

STATIC_ASSERT(sizeof(Mot_File_List) == 0x20);

/**
 * Carries one set of motions, the state that plays them and the frames that
 * they drive.
 */
struct tagMOTION_TYPE {
    sceVu0FMATRIX *base_matrices; /**< Bind-pose matrices loaded from the motion archive. */
    Mot_List      *proc_list;     /**< Frame drivers applied from the motion state. */
    Mot_List      *proc_list2;    /**< Frame drivers applied from the frame table. */
    u8             unk_0C[4];
    MOTION_STATE   state;       /**< How far the set has played, and what it moves. */
    tagFRAME_INF  *frame_info;  /**< Frames that the motions drive. */
    MOTION_INFO   *motion_info; /**< One entry per motion of the set. */
    s32            unk_68;
    s32            unk_6C;
    s32            unk_70;
    s32            unk_74;
    float          unk_78;
    s32            unk_7C;
} __attribute__((aligned(16)));

STATIC_ASSERT(sizeof(tagMOTION_TYPE) == 0x80);

/**
 * Names one optional motion-data file found in a model archive.
 */
struct MOTION_FILE_INFO {
    char         *name; /**< Name used to find the file in the archive. */
    unsigned int *data; /**< Packed file data, or nullptr when the file is absent. */
    int           size; /**< Size of the packed file in bytes. */
};

STATIC_ASSERT(sizeof(MOTION_FILE_INFO) == 0xC);

/**
 * Takes a motion's animation data out of an arena and fills it from its files.
 */
int CreateAnimeDataEX(tagMOTION_TYPE *motion, CDataAlloc2<1> *arena, MOTION_FILE_INFO *files);

/**
 * Builds the per-frame animation table a model's motion needs.
 */
void AnimeDataInit(CFrame *frame, tagMOTION_TYPE *motion, CDataAlloc2<1> *arena, tagFRAME_INF **frame_info);

/**
 * Builds the per-frame animation table into storage already set aside.
 */
int AnimeDataInit(CFrame *frame, tagMOTION_TYPE *motion, CDataAlloc2<1> *arena, tagFRAME_INF *frame_info);

/**
 * Applies one motion's frame to a model's frame hierarchy, and gives back the
 * next driver in the list.
 */
Mot_List *MotionProc(CFrame *frame, MOTION_STATE *state, Mot_List *list);

/**
 * Skins one bone of a model: moves each weighted vertex toward the bone's
 * transform, and gives back the next driver in the list.
 */
Mot_List *MotionProc2(CFrame *frame, tagMOTION_TYPE *motion, tagFRAME_INF *frame_info, Mot_List *list);

/**
 * Puts a model on the frame that its motion state stands on.
 */
void SetMotionEX(CFrame *frame, tagMOTION_TYPE *motion, MOTION_INFO *info, MOTION_STATE *state, tagFRAME_INF *frame_info);

/**
 * Maps how far the playing motion has run onto the requested motion's frames.
 */
int NextMotionTime_GET_EX(MOTION_INFO *info, MOTION_STATE *state);

/**
 * Finds where a line from one point to another crosses a set of polygons, and
 * gives back how many crossings it found.
 */
int CheckHits(CCPoly *poly, int count, float *from, float *to, int max, int *hit_poly, float (*hit_point)[4], int sort, int mode);

/**
 * Copies the polygons of a set whose bounds meet a box, and gives back how
 * many it copied.
 */
int PickUpNearPoly(CCPoly *out, CBoxVu0 box, CCPoly *poly, int count);

/**
 * Finds the polygon straight below a point, and gives back which one it is.
 */
int CheckHitVertical(CCPoly *poly, int count, float *from, float depth, float *hit_point, int mode);

/**
 * Works out where a character stands on screen, in pixels.
 */
void GetScrPosFromChar(CCharacter *chara, int *out_pos);

/**
 * Maps one character of a registered name from its Japanese code to the
 * external-character code the English font draws it with.
 */
int NameRegistCodeJtoE(int code);

/**
 * Gives the font palette entry that holds one colour, or the first entry.
 */
unsigned int Color2Clut(unsigned int colour);

/**
 * Turns one point about another by an angle, and writes where it lands.
 */
void RollPos(float *centre, float *point, float angle, float *out);

/**
 * Draws one part of a texture into a rectangle of the screen, at one colour.
 */
void set2DSprite_Core(sceVif1Packet *packet, CTexture *texture, const CRect_i_ &screen, const CRect_i_ &texel, unsigned char red, unsigned char green, unsigned char blue, unsigned char alpha);

/**
 * Tells whether a point lies inside a rectangle.
 */
int CheckPosInOutForRect(RECT *rect, int x, int y);

/**
 * Gives back how far a point lies from a rectangle.
 */
float GetDisPosToRect(RECT *rect, int x, int y);

/**
 * Gives back the point that lies a fraction of the way from one to another.
 */
float LinerInterpolation(float from, float to, float at);

/**
 * Widens a rectangle so that it also covers a point.
 */
void AreaAddPos(int *area, int *pos, int *out);

/**
 * Records what a step of a character ran into.
 */
struct MoveCheckInfo {
    s32           landed; /**< 1 where the step landed on a polygon. */
    u8            unk_04[0xC];
    CCPoly        ground_poly;  /**< Polygon found below the step. */
    s32           ground_found; /**< 1 where the step found ground below it. */
    u8            unk_64[0xC];
    CCPoly        poly;         /**< Polygon that the step landed on. */
    sceVu0FVECTOR ground_point; /**< Point where the step found ground below it. */
};

STATIC_ASSERT(sizeof(MoveCheckInfo) == 0xD0);

/**
 * Slides a position along a velocity, against a list of collision polygons.
 */
int MoveCheck(float *pos, float *velocity, float *out_pos, MoveCheckInfo *out_info, CCPoly *polys, int poly_num, int mode);

/**
 * Finds the collision polygon below a point and combines its surface attributes.
 */
int GetFootPoly(float *position, float depth, CCPoly *found, float *ground, CCPoly *polys, int count, int mode);

/**
 * Pushes a position out of the polygons within a radius of it.
 */
int CheckWidth(CCPoly *polys, int count, float *position, float radius, float *out, int flags);

/**
 * Finds the event polygon that a step crosses, and gives back the kind of
 * ground it is.
 */
int GetEventPoly(float *position, float *velocity, CCPoly *found, int *found_no, float *hit, CCPoly *polys, int count, int mode);

/**
 * Pushes the camera out of the polygons within a radius of it, counting only
 * the polygons that face the way it is pushed.
 */
int CheckCameraWidth(CCPoly *polys, int count, float *position, float radius, float *out, int flags);

/**
 * Texture buffer the first common menu message window draws its glyphs from.
 */
extern u8 MesWinTexBuff_01[0x100];

/**
 * Texture buffer the second common menu message window draws its glyphs from.
 */
extern u8 MesWinTexBuff_02[0x100];

/**
 * Texture buffer the third common menu message window draws its glyphs from.
 */
extern u8 MesWinTexBuff_11[0x100];

/**
 * Texture buffer the name message window draws its glyphs from.
 */
extern u8 MesWinTexBuff_12[0x100];

} // namespace dc::game
