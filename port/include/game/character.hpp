#pragma once

#include "common.h"

#include <libvu0.h>

#include "dataalloc_fwd.hpp"
#include "fakepointlight.hpp"
#include "gameutil.hpp"
#include "object.hpp"
#include "textureanime.hpp"

namespace dc::game {

/**
 * @file
 * Declares the character that a motion drives.
 */

/**
 * Kinds of character motion event, as CHARA_EVENT::kind holds them.
 */
enum class CharaEventKind : s32 {
    FootSound = 0,   /**< Footstep sound. */
    SoundEffect = 1, /**< Sound effect. */
};

class CCloth;

/**
 * Number of sets of motions that one character can hold.
 */
inline constexpr int CHARA_MOTION_MAX = 8;

/**
 * Number of point lights that one character can stand in.
 */
inline constexpr int CHARA_POINT_LIGHT_MAX = 2;

/**
 * Number of pieces of cloth that one character can wear.
 */
inline constexpr int CHARA_CLOTH_MAX = 4;

/**
 * Number of foot sounds that one motion can ask for.
 */
inline constexpr int CHARA_FOOT_SOUND_MAX = 6;

/**
 * Number of events that one motion can ask for.
 */
inline constexpr int CHARA_EVENT_MAX = 32;

/**
 * Names one sound that a motion plays as a foot reaches the ground.
 */
struct CHARA_FOOT_SOUND {
    s32 frame; /**< Frame of the motion that plays the sound; below zero where the slot is free. */
    s32 unk_04;
    s32 foot;      /**< Foot that reaches the ground. */
    s32 motion_no; /**< Motion that plays the sound. */
    s32 unk_10;
};

STATIC_ASSERT(sizeof(CHARA_FOOT_SOUND) == 0x14);

/**
 * Names one thing that a motion does as it reaches a frame.
 */
struct CHARA_EVENT {
    s32 frame;     /**< Frame of the motion that fires the event; below zero where the slot is free. */
    s32 kind;      /**< What the event plays. @see CharaEventKind. */
    s32 no;        /**< Sound that the event plays. */
    s32 motion_no; /**< Motion that fires the event. */
    s32 unk_10;
};

STATIC_ASSERT(sizeof(CHARA_EVENT) == 0x14);

/**
 * Names one thing that a motion does over a range of frames.
 */
struct CHARA_UNK_1068 {
    s32   unk_00;
    float unk_04;
    s32   unk_08;
    s32   unk_0C;
    s32   unk_10;
};

STATIC_ASSERT(sizeof(CHARA_UNK_1068) == 0x14);

/**
 * One motion set, as a character holds it while it plays.
 */
class MotionParam {
public:
    u8 storage[sizeof(tagMOTION_TYPE)]; /**< Room for the motions of one motion set beyond the first. */

    /**
     * Constructs the motion parameters.
     */
    MotionParam();
};

STATIC_ASSERT(sizeof(MotionParam) == 0x80);

/**
 * Clips a triangle against the near plane, giving back the polygon that survives.
 */
int scissior(float out[][4], float first[][4], float second[][4], float near_z);

/**
 * Moves and animates one character in the world.
 */
class CCharacter : public CObject {
public:
    float            body_width;                              /**< Width of the character collision body. */
    float            body_height;                             /**< Height used for targeting and camera framing. */
    float            body_depth;                              /**< Depth of the character collision body. */
    CFrame          *frame;                                   /**< Frame that draws the character; null if it has no model. */
    CFrame          *shadow_frame;                            /**< Frame that draws the shadow; null if it casts none. */
    u_int           *images[4];                               /**< Texture images the character loaded; the first is its own, the rest come from extension packs. */
    char            *config;                                  /**< Configuration text the character was loaded from, kept to reload its texture animations. */
    s32              config_size;                             /**< Size in bytes of the configuration text. */
    CTextureAnime    tex_anime;                               /**< Animation of every texture that the character draws. */
    tagFRAME_INF    *frame_info;                              /**< Frame bindings every motion set of the model shares. */
    tagFRAME_INF    *shadow_frame_info;                       /**< Frame bindings every motion set of the shadow shares. */
    tagMOTION_TYPE   motion_type;                             /**< Motions of the model. */
    tagMOTION_TYPE   shadow_motion_type;                      /**< Motions of the shadow. */
    s32              motion_start[CHARA_MOTION_MAX];          /**< First motion number of each set; -1 where it holds none. */
    s32              motion_end[CHARA_MOTION_MAX];            /**< Motion number after the last one of each set. */
    MotionParam      motion_storage[CHARA_MOTION_MAX];        /**< Storage the motions of each set beyond the first are built in. */
    MotionParam      shadow_motion_storage[CHARA_MOTION_MAX]; /**< Storage the shadow motions of each set beyond the first are built in. */
    tagMOTION_TYPE  *motion[CHARA_MOTION_MAX];                /**< Motions of each set; null where it holds none. */
    tagMOTION_TYPE  *shadow_motion[CHARA_MOTION_MAX];         /**< Shadow motions of each set. */
    float            motion_speed;                            /**< Speed of the motion; -1.0 for the speed the motion gives. */
    s32              motion_flags;                            /**< Playback flags: 1 holds the motion still, 2 stops it on its last frame, 4 restarts it from its first frame without blending. */
    s32              motion_no;                               /**< Identifier of the motion that the character plays. */
    float            unk_C6C;
    s32              motion_state;                       /**< 0 while no motion plays, 2 while one plays, 3 as one ends. */
    CCloth         **cloth;                              /**< Pieces of cloth that the character wears. */
    CCloth          *cloth_buf[8];                       /**< Storage the cloth pointers start in. */
    s32              wind;                               /**< Wind the character's cloth sways in, or zero for none. */
    s32              fade_out;                           /**< 1 while the ground fades the character down to its lowest alpha. */
    s32              ground_ambient_no;                  /**< Which of the two ground ambient colours the character takes on, or -1 for neither. */
    sceVu0FVECTOR    ground_ambient[2];                  /**< Ambient colours the character takes on over the two kinds of tinted ground. */
    sceVu0FVECTOR    ambient_tint;                       /**< Ambient colour added while drawing, stepping towards the selected ground colour. */
    sceVu0FVECTOR    ambient_offset;                     /**< Ambient RGB added while drawing and an optional alpha override. */
    float            fade[4];                            /**< Alpha the character draws with, two spare values, and the alpha it fades down to. */
    CFakePointLight  point_light[CHARA_POINT_LIGHT_MAX]; /**< Lights that the character stands in. */
    CHARA_FOOT_SOUND foot_sound[CHARA_FOOT_SOUND_MAX];   /**< Sounds that the feet play. */
    s32              foot_sound_id;                      /**< Set of foot sounds that the ground asks for; below zero for none. */
    s32              foot_sound_enable;                  /**< 1 while the feet play a sound. */
    s32              foot_sound_wait;                    /**< Frames before the feet may play another sound. */
    s32              event_enable;                       /**< 1 while the motion fires its events. */
    CHARA_EVENT      event[CHARA_EVENT_MAX];             /**< Events that the motions fire. */
    CHARA_UNK_1068   unk_1068[16];

    /**
     * 1 while every character holds the motion where it stands.
     */
    static s32 MotionStopFlag;

    /**
     * Sets a character to the state it starts in.
     */
    CCharacter() { Initialize(); }

    /**
     * Selects the motion the character plays next, without advancing it.
     */
    void SetMotion(int no, int mode, float speed = -1.0f) {
        motion_no = no;
        motion_flags = mode;
        motion_speed = speed;
    }

    /**
     * Sets the speed of the motion the character plays.
     */
    void SetMotionSpeed(float speed) {
        motion_speed = speed;
    }

    /**
     * Clears every point light that affects the character.
     */
    void ClearPointLight();

    /**
     * Adds a point light to the first free light slot.
     */
    int SetPointLight(float *pos, float inner_range, float outer_range, float red, float green, float blue, float alpha);

    /**
     * Initializes the character texture animation from animation data.
     */
    void InitializeTexAnime(CTexAnimeData *data, int count);

    /**
     * Clears the character texture animation.
     */
    void ClearTexAnime();

    /**
     * Advances the character texture animation.
     */
    void TextureAnime(int step);

    /**
     * Enables one character texture animation.
     */
    void TexAnimeOn(int no);

    /**
     * Disables one character texture animation.
     */
    void TexAnimeOff(int no);

    /**
     * Adds footstep events across a motion frame range.
     */
    int SetFootSound(float left_frame, float right_frame, int motion_no);

    /**
     * Selects the footstep sound set for the current ground.
     */
    void SetFootSoundID(int id);

    /**
     * Enables or disables motion footstep sounds.
     */
    void FootSoundEnable(int on);

    /**
     * Enables or disables motion events.
     */
    void EventEnable(int on);

    /**
     * Adds an event to one motion frame.
     */
    void SetEvent(float frame, int kind, int no, int motion_no);

    /**
     * Clears events for one motion or every motion.
     */
    void ClearEvent(int motion_no);

    /**
     * Assigns a camera to every loaded motion set.
     */
    void SetMotionCamera(CCamera *camera);

    /**
     * Finds the motion set and local index for a motion number.
     */
    tagMOTION_TYPE *GetMotionParam(int motion_no, int *out_index, int *out_start, int *out_end, int *out_set);

    /**
     * Gets the motion information for one motion number.
     */
    MOTION_INFO *GetMotionInfo(int motion_no);

    /**
     * Gets the current frame time of the active motion.
     */
    float GetNowTime();

    /**
     * Advances the model motion and fires frame events.
     */
    void Step() override;

    /**
     * Synchronizes and advances the shadow motion.
     */
    virtual void ShadowStep();

    /**
     * Advances every cloth simulation worn by the character.
     */
    virtual void ClothStep(int step);

    /**
     * Sets the floor height for every cloth simulation.
     */
    virtual void ClothFloor(int floor);

    /**
     * Stops every cloth simulation worn by the character.
     */
    virtual void StopCloth(int unused);

    /**
     * Copies the character position to an output vector.
     */
    virtual void GetPosition(float *out_position);

    /**
     * Sets the character, model, and shadow positions.
     */
    void SetPosition(float x, float y, float z) override;

    /**
     * Gets the character position in world coordinates.
     */
    virtual void GetWorldPosition(float *out_position);

    /**
     * Moves the character onto a reachable collision polygon.
     */
    virtual int PickUpPoly(float *position, CCPoly *polygons);

    /**
     * Sets the character position from a vector.
     */
    void SetPosition(float *position) override;

    /**
     * Sets the character position from a three-component vector.
     */
    void SetPosition(CVector3_f_ position) override;

    /**
     * Sets the character, model, and shadow rotations.
     */
    void SetRotation(float x, float y, float z) override;

    /**
     * Sets the character rotation from a vector.
     */
    void SetRotation(float *rotation) override;

    /**
     * Sets the character rotation from a three-component vector.
     */
    void SetRotation(CVector3_f_ rotation) override;

    /**
     * Sets the character, model, and shadow scales.
     */
    void SetScale(float x, float y, float z) override;

    /**
     * Sets the character scale from a vector.
     */
    void SetScale(float *scale) override;

    /**
     * Draws the model and every cloth simulation.
     */
    virtual void Draw();

    /**
     * Draws the character shadow below the model.
     */
    virtual void DrawShadow();

    /**
     * Loads model and texture data from a character pack.
     */
    virtual void LoadPackData(unsigned int *pack, char *name, CDataAlloc2<1> *model_alloc, CDataAlloc2<1> *texture_alloc);

    /**
     * Loads model, motion, and texture data from a character pack.
     */
    virtual void LoadPackData(unsigned int *pack, char *name, CDataAlloc2<1> *model_alloc, CDataAlloc2<1> *motion_alloc, CDataAlloc2<1> *texture_alloc);

    /**
     * Loads a character pack from one arena, putting its textures in the given
     * texture block.
     */
    virtual void LoadPackData2(unsigned int *pack, char *name, CDataAlloc2<1> *alloc, int texture_block_no, CDataAlloc2<1> *image_alloc, int visual_type);

    /**
     * Loads a character pack from one arena into a texture block, optionally
     * keeping the textures the block already holds.
     */
    virtual void LoadPackData3(unsigned int *pack, char *name, CDataAlloc2<1> *alloc, int texture_block_no, CDataAlloc2<1> *image_alloc, int keep_textures, int visual_type);

    /**
     * Restores a texture block after extended character textures are deleted.
     */
    void DeleteExtendTexture(int block_no);

    /**
     * Releases every extended motion set.
     */
    void DeleteExtendMotion();

    /**
     * Resets the character and all animation state.
     */
    virtual void Initialize();
};

STATIC_ASSERT(sizeof(CCharacter) == 0x11B0);

/**
 * Moves and animates the character that the player leads.
 */
class CMainChara : public CCharacter {
public:
    s32           unk_11B0;
    s32           unk_11B4;
    s32           unk_11B8;
    s32           unk_11BC;
    s32           unk_11C0;
    s32           unk_11C4;
    s32           unk_11C8;
    s32           unk_11CC;
    MoveCheckInfo move_info; /**< What the character's last step ran into. */

    /**
     * Puts a new player character at rest.
     */
    CMainChara() { Initialize(); }

    /**
     * Draws the player character.
     */
    void Draw() override;

    /**
     * Puts the character at rest with no motion and nothing in hand.
     */
    void Initialize() override;
};

STATIC_ASSERT(sizeof(CMainChara) == 0x12A0);

} // namespace dc::game
