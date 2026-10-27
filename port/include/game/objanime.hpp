#pragma once

#include "common.h"

#include <libvu0.h>

namespace dc::game {

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
     */
    void Initialize();

    /**
     * Constructs an object-animation sequence.
     */
    OBJ_ANIME_SEQ();
};

STATIC_ASSERT(sizeof(OBJ_ANIME_SEQ) == 0x90);

/**
 * Frame animations the map parts declare, filled by the item-definition parser.
 */
extern OBJ_ANIME_SEQ FrameObjAnim[48];

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
 */
void ObjAnimeAllStop();

/**
 * Starts every object animation.
 */
void ObjAnimeAllStart();

/**
 * Attaches an object animation to one frame.
 */
int InitObjAnime(CFrame *frame, OBJ_ANIME_SEQ *sequence);

/**
 * Attaches an object animation to a list of frames.
 */
int InitObjAnime(CFrame **frames, OBJ_ANIME_SEQ *sequence);

/**
 * Attaches an object animation to a counted list of frames.
 */
int InitObjAnime(CFrame **frames, int count, OBJ_ANIME_SEQ *sequence);

/**
 * Attaches an object animation to the frames one function point names.
 */
int InitObjAnime(CFrame **frames, int count, EPARTS_FUNC_DATA *func, OBJ_ANIME_SEQ *sequence);

/**
 * Advances one object animation by a frame.
 */
void ObjAnimePlay(OBJ_ANIME_SEQ *sequence);

/**
 * Selects the frame that carries an edited map effect.
 */
void InitEditEffect(CFrame *frame, EDIT_EFFECT_INFO *effect);

/**
 * Attaches an editor effect to the frame a function point names.
 */
int InitEditEffect(CFrame *frame, EPARTS_FUNC_DATA *func, EDIT_EFFECT_INFO *effect);

/**
 * Gives back 1 while the time of day, the map flags and its frames let an effect draw.
 */
int CheckEditEffect(EDIT_EFFECT_INFO *effect, float time);

/**
 * Advances the editor's shared fire, candle and flame effects.
 */
void EditEffectStep();

/**
 * Rebuilds the editor's fire texture for the frame.
 */
void EditEffectStep2();

/**
 * Draws one editor effect, choosing the kind from its record.
 */
void DrawEditEffect(EDIT_EFFECT_INFO *effect, CCamera *camera, CEffectGroup *group);

} // namespace dc::game
