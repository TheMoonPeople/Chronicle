#pragma once

#include "common.h"

#include <libvu0.h>

/**
 * Kinds of map part function marker, as EPARTS_FUNC_DATA::kind holds them.
 */
// clang-format off
enum EPartsFuncKind {
    EPARTS_FUNC_VILLAGER      = 1,  /**< Villager stand point. */
    EPARTS_FUNC_DOOR          = 2,  /**< Door. */
    EPARTS_FUNC_FIRE_LARGE    = 3,  /**< Three-layer fire. */
    EPARTS_FUNC_SMOKE         = 4,  /**< Smoke. */
    EPARTS_FUNC_FIRE_MEDIUM   = 5,  /**< Two-layer fire. */
    EPARTS_FUNC_OBJ_ANIME     = 6,  /**< Object animation. */
    EPARTS_FUNC_CAMERA        = 7,  /**< Interior camera zone. */
    EPARTS_FUNC_OBJ_TIMER     = 9,  /**< Object timer. */
    EPARTS_FUNC_DOOR_SIDE_A   = 10, /**< One side of an entrance. */
    EPARTS_FUNC_DOOR_SIDE_B   = 11, /**< Other side of an entrance. */
    EPARTS_FUNC_FIRE_SMALL    = 12, /**< One-layer fire. */
    EPARTS_FUNC_UNK_D         = 13, /**< Two-layer fire, like EPARTS_FUNC_FIRE_MEDIUM. */
    EPARTS_FUNC_UNK_E         = 14, /**< Effect that is not drawn. */
    EPARTS_FUNC_CANDLE        = 15, /**< Candle. */
    EPARTS_FUNC_DOOR_EXTENT   = 16, /**< Extent of an entrance. */
    EPARTS_FUNC_ITEM_BOX      = 17, /**< Item box. */
    EPARTS_FUNC_EVENT         = 18, /**< Event point. */
    EPARTS_FUNC_LADDER_BOTTOM = 19, /**< Bottom of a ladder. */
    EPARTS_FUNC_LADDER_TOP    = 20, /**< Top of a ladder. */
    EPARTS_FUNC_SOUND         = 21, /**< Sound source. */
};

// clang-format on

/**
 * What an object animation sequence drives, as OBJ_ANIME_SEQ::property holds it.
 */
// clang-format off
enum ObjAnimeProperty {
    OBJ_ANIME_PROPERTY_NONE     = -1, /**< Unused. */
    OBJ_ANIME_PROPERTY_ROTATION = 0,  /**< Rotation. */
    OBJ_ANIME_PROPERTY_POSITION = 1,  /**< Position. */
    OBJ_ANIME_PROPERTY_SCALE    = 2,  /**< Scale. */
    OBJ_ANIME_PROPERTY_COLOR    = 3,  /**< Colour. */
};

// clang-format on

/**
 * How an object animation sequence moves its value, as OBJ_ANIME_SEQ::mode holds it.
 */
// clang-format off
enum ObjAnimeMode {
    OBJ_ANIME_MODE_LINEAR              = 0, /**< Adds the step forever. */
    OBJ_ANIME_MODE_WRAP                = 1, /**< Restarts from the start value. */
    OBJ_ANIME_MODE_PING_PONG           = 2, /**< Turns back at each end. */
    OBJ_ANIME_MODE_ONCE                = 3, /**< Stops at the end value. */
    OBJ_ANIME_MODE_RANDOM              = 4, /**< Random value on each axis. */
    OBJ_ANIME_MODE_RANDOM_WALK         = 5, /**< Random step on each axis, held within the range. */
    OBJ_ANIME_MODE_RANDOM_UNIFORM      = 6, /**< One random value on every axis. */
    OBJ_ANIME_MODE_RANDOM_WALK_UNIFORM = 7, /**< One random step on every axis, held within the range. */
};

// clang-format on

/**
 * Kinds of town effect, as EDIT_EFFECT_INFO::kind holds them.
 */
// clang-format off
enum EditEffectKind {
    EDIT_EFFECT_NONE        = 0, /**< Free. */
    EDIT_EFFECT_FIRE_LARGE  = 1, /**< Three-layer fire. */
    EDIT_EFFECT_FIRE_SMALL  = 2, /**< One-layer fire. */
    EDIT_EFFECT_FIRE_MEDIUM = 3, /**< Two-layer fire. */
    EDIT_EFFECT_SMOKE       = 4, /**< Smoke. */
    EDIT_EFFECT_UNK_5       = 5, /**< Not drawn. */
    EDIT_EFFECT_CANDLE      = 6, /**< Candle. */
    EDIT_EFFECT_SOUND       = 7, /**< Sound source. */
};

// clang-format on

// Forward declarations for the types these declarations name. The skeleton
// headers are generated from the retail symbol table, which knows the type
// names but not where they live.
class CCamera;
class CEffectGroup;
class CFrame;
class CMapParts;

/**
 * Stores one function marker extracted from a map-part resource.
 *
 * Each record is copied verbatim from the resource before its owning part index is installed.
 */
struct EPARTS_FUNC_DATA {
    u8            unk_00[0x10];
    int           kind;            /**< Selects how the marker is interpreted by map setup. @see EPartsFuncKind. */
    CMapParts    *parts;           /**< Map part the marker was extracted from. */
    float         start_time;      /**< Beginning of the marker's active time interval. */
    float         end_time;        /**< End of the marker's active time interval. */
    int           link_id;         /**< Associates related markers belonging to one event. */
    int           completion_flag; /**< Map flag which suppresses the resulting event after completion. */
    u8            unk_28[0x8];
    char          frame_name[0x10]; /**< Optional frame whose visibility gates the resulting event. */
    sceVu0FVECTOR position;         /**< Primary position carried by the marker. */
    sceVu0FVECTOR rotation;         /**< Primary rotation carried by the marker. */
    sceVu0FVECTOR parameters;       /**< Secondary vector whose meaning depends on the marker kind. */
    sceVu0FVECTOR values;           /**< Scalar parameters whose meaning depends on the marker kind. */
    sceVu0FMATRIX matrix;           /**< Placement of the marker's frame relative to its owning frame. */
};

STATIC_ASSERT(sizeof(EPARTS_FUNC_DATA) == 0xC0);

/**
 * Plays one animation of a map object.
 *
 * 0x1B00 bytes hold 48 of them at `FrameObjAnim`.
 */
struct OBJ_ANIME_SEQ {
    char          frame_name[0x10]; /**< Frame the animation drives; empty for the frame it is given. */
    int           property;         /**< What the animation drives. @see ObjAnimeProperty. */
    int           mode;             /**< How the value moves. @see ObjAnimeMode. */
    u8            unk_18[0x8];
    sceVu0FVECTOR from;            /**< Value the animation starts at, and the lower bound of a random one. */
    sceVu0FVECTOR to;              /**< Value the animation ends at, and the upper bound of a random one. */
    sceVu0FVECTOR step;            /**< Amount the value changes by each frame, or the spread of a random walk. */
    sceVu0FVECTOR current;         /**< Current animated value applied to the attached frames. */
    CFrame       *frames[10];      /**< Frames driven by this animation. */
    int           completion_flag; /**< Map flag associated with the source function marker. */
    int           unk_8C;

    /**
     * Clears one object-animation sequence.
     *
     * @mangled Initialize__13OBJ_ANIME_SEQFv
     * @address 0x165C90
     * @size 0x14
     */
    void Initialize();

    /**
     * Constructs an object-animation sequence.
     *
     * @mangled __ct__13OBJ_ANIME_SEQFv
     * @address 0x165CB0
     * @size 0x30
     */
    OBJ_ANIME_SEQ();
};

STATIC_ASSERT(sizeof(OBJ_ANIME_SEQ) == 0x90);

/** Frame animations the map parts declare, defined with the item-definition parser. */
extern "C" OBJ_ANIME_SEQ FrameObjAnim[48];

/**
 * Describes one effect that an edited map places on a part, and the frame
 * that carries it.
 */
struct EDIT_EFFECT_INFO {
    char          frame_name[16]; /**< Names the child frame that carries the effect. */
    s32           kind;           /**< Effect in the slot; zero or below where the slot is free. @see EditEffectKind. */
    s32           map_flag;       /**< Map flag that stops the effect while it is set; zero or below where none does. */
    float         start;          /**< Time of day that the effect starts at. */
    float         end;            /**< Time of day that the effect stops at. */
    CFrame       *frame;          /**< Frame that the effect stands on. */
    s32           unk_24;
    u8            unk_28[8];
    sceVu0FVECTOR offset; /**< Distance from the frame to the effect; also the first end of a line sound source. */
    u8            unk_40[16];
    sceVu0FVECTOR colour;        /**< Colour of the light that the effect gives; also the second end of a line sound source, whose fourth component says whether that end is set. */
    float         sound_no;      /**< Sound effect emitted by this effect. */
    float         near_distance; /**< Distance at which the sound has full volume. */
    float         far_distance;  /**< Distance beyond which the sound is inaudible. */
    u8            unk_6C[4];
};

STATIC_ASSERT(sizeof(EDIT_EFFECT_INFO) == 0x70);

/**
 * Stops every object animation.
 *
 * @mangled ObjAnimeAllStop__Fv
 * @address 0x165CE0
 * @size 0x10
 */
void ObjAnimeAllStop();

/**
 * Starts every object animation.
 *
 * @mangled ObjAnimeAllStart__Fv
 * @address 0x165CF0
 * @size 0xC
 */
void ObjAnimeAllStart();

/**
 * Attaches an object animation to one frame.
 *
 * @mangled InitObjAnime__FP6CFrameP13OBJ_ANIME_SEQ
 * @address 0x165D00
 * @size 0xC4
 */
int InitObjAnime(CFrame *frame, OBJ_ANIME_SEQ *sequence);

/**
 * Attaches an object animation to a list of frames.
 *
 * @mangled InitObjAnime__FPP6CFrameP13OBJ_ANIME_SEQ
 * @address 0x165DD0
 * @size 0xFC
 */
int InitObjAnime(CFrame **frames, OBJ_ANIME_SEQ *sequence);

/**
 * Attaches an object animation to a counted list of frames.
 *
 * @mangled InitObjAnime__FPP6CFrameiP13OBJ_ANIME_SEQ
 * @address 0x165ED0
 * @size 0x138
 */
int InitObjAnime(CFrame **frames, int count, OBJ_ANIME_SEQ *sequence);

/**
 * Attaches an object animation to the frames one function point names.
 *
 * @mangled InitObjAnime__FPP6CFrameiP16EPARTS_FUNC_DATAP13OBJ_ANIME_SEQ
 * @address 0x166010
 * @size 0x160
 */
int InitObjAnime(CFrame **frames, int count, EPARTS_FUNC_DATA *func, OBJ_ANIME_SEQ *sequence);

/**
 * Advances one object animation by a frame.
 *
 * @mangled ObjAnimePlay__FP13OBJ_ANIME_SEQ
 * @address 0x1661E0
 * @size 0x78C
 */
void ObjAnimePlay(OBJ_ANIME_SEQ *sequence);

/**
 * Selects the frame that carries an edited map effect.
 *
 * @mangled InitEditEffect__FP6CFrameP16EDIT_EFFECT_INFO
 * @address 0x166970
 * @size 0x60
 */
void InitEditEffect(CFrame *frame, EDIT_EFFECT_INFO *effect);

/**
 * Attaches an editor effect to the frame a function point names.
 *
 * @mangled InitEditEffect__FP6CFrameP16EPARTS_FUNC_DATAP16EDIT_EFFECT_INFO
 * @address 0x1669D0
 * @size 0x1E0
 */
int InitEditEffect(CFrame *frame, EPARTS_FUNC_DATA *func, EDIT_EFFECT_INFO *effect);

/**
 * Gives back 1 while an effect is one that the time of day and the map flags
 * let draw.
 *
 * @mangled CheckEditEffect__FP16EDIT_EFFECT_INFOf
 * @address 0x166BB0
 * @size 0x160
 */
int CheckEditEffect(EDIT_EFFECT_INFO *effect, float time);

/**
 * Advances the editor's shared fire, candle and flame effects.
 *
 * @mangled EditEffectStep__Fv
 * @address 0x166D10
 * @size 0xC4
 */
void EditEffectStep();

/**
 * Rebuilds the editor's fire texture for the frame.
 *
 * @mangled EditEffectStep2__Fv
 * @address 0x166DE0
 * @size 0x28
 */
void EditEffectStep2();

/**
 * Draws one editor effect, choosing the kind from its record.
 *
 * @mangled DrawEditEffect__FP16EDIT_EFFECT_INFOP7CCameraP12CEffectGroup
 * @address 0x166E10
 * @size 0x24C
 */
void DrawEditEffect(EDIT_EFFECT_INFO *effect, CCamera *camera, CEffectGroup *group);

/**
 * Whether all object animations are stopped.
 */
extern int all_stop;
