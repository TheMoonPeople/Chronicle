#pragma once

#include "common.h"

namespace dc::game {

/**
 * Identifies the operation performed by a queued action.
 */
enum ACT_SEQ_OPERATION {
    ACT_SEQ_UNUSED = 0,
    ACT_SEQ_SET_POSITION = 1,
    ACT_SEQ_MOVE = 2,
    ACT_SEQ_ROTATE_REFERENCE = 4,
    ACT_SEQ_ROTATE_MOVEMENT = 5,
    ACT_SEQ_ROTATE_ANGLE = 6,
    ACT_SEQ_CLEAR_ROTATION = 7,
    ACT_SEQ_WAIT_ROTATION = 8,
    ACT_SEQ_SET_ROTATION = 10,
    ACT_SEQ_DELAY_ROTATION = 11,
    ACT_SEQ_MOTION = 12,
    ACT_SEQ_TRIGGER_MOTION = 14,
    ACT_SEQ_ANIMATION = 15,
    ACT_SEQ_TRIGGER_ANIMATION = 18
};

/**
 * Stores a queued action and its operation-specific arguments.
 */
struct ACT_SEQ {
    ACT_SEQ_OPERATION operation; /**< Selects the queued action. */
    int               duration;  /**< Frame count or trigger delay. */
    int               unk_08;
    ACT_SEQ          *next; /**< Next action in the same queue. */

    union {
        float vector[4]; /**< Position, rotation, or reference vector. */

        struct {
            int id;   /**< Motion or texture animation identifier. */
            int mode; /**< Motion mode or animation enable state. */

            union {
                float speed;         /**< Motion playback speed. */
                int   disable_after; /**< Disables texture animation after its duration. */
            } playback;              /**< Playback parameters for the selected operation. */

            int flags; /**< Motion transition flags. */
        } animation;   /**< Motion and texture animation arguments. */
    } arguments;       /**< Operation-specific parameters. */
};

STATIC_ASSERT(sizeof(ACT_SEQ) == 0x20);

/**
 * Selects how automatic rotation turns the character each frame.
 */
enum class ActSeqRotationMode : int {
    None = 0,          /**< No automatic rotation. */
    FaceReference = 1, /**< Turn to face the reference position. */
    FaceMovement = 2,  /**< Turn to face the movement direction. */
    TurnToAngle = 3,   /**< Turn to the target yaw. */
    FaceDirection = 4, /**< Turn to face the target vector as a direction. */
};

class CCharacter;

/**
 * Queues and advances character movement, rotation, and animation actions.
 */
class CActionSeq {
public:
    ACT_SEQ           *move_head; /**< First movement action. */
    ACT_SEQ           *move_tail; /**< Last movement action. */
    u8                 unk_08[8];
    float              position[4];        /**< Current movement position. */
    float              start_position[4];  /**< Starting position for interpolation. */
    float              queued_position[4]; /**< Destination of the last queued movement. */
    int                move_frame;         /**< Elapsed movement frames. */
    int                anime_frame;        /**< Elapsed texture animation frames. */
    ActSeqRotationMode rotation_mode;      /**< Active automatic rotation. */
    int                rotation_complete;  /**< Indicates that the rotation reached its target. */
    int                rotation_delay;     /**< Frames remaining before rotation advances. */
    int                motion_trigger;     /**< Enables queued motion playback. */
    int                motion_delay;       /**< Frames remaining before motion playback. */
    int                anime_trigger;      /**< Enables queued texture animation playback. */
    int                anime_delay;        /**< Frames remaining before texture animation playback. */
    u8                 unk_64[12];
    float              rotation_target[4]; /**< Reference position or target angles for rotation; the last element holds the turn speed. */
    ACT_SEQ           *rotation_head;      /**< First rotation interpolation action. */
    ACT_SEQ           *rotation_tail;      /**< Last rotation interpolation action. */
    u8                 unk_88[8];
    float              rotation[4];       /**< Current character rotation. */
    float              start_rotation[4]; /**< Starting rotation for interpolation. */
    int                rotation_frame;    /**< Elapsed rotation interpolation frames. */
    ACT_SEQ           *motion_head;       /**< First motion action. */
    ACT_SEQ           *motion_tail;       /**< Last motion action. */
    ACT_SEQ           *anime_head;        /**< First texture animation action. */
    ACT_SEQ           *anime_tail;        /**< Last texture animation action. */
    int                capacity;          /**< Number of records in the action pool. */
    ACT_SEQ           *pool;              /**< Storage for queued actions. */
    CCharacter        *character;         /**< Character receiving the actions. */

    /**
     * Binds the action pool and clears playback state.
     */
    void Initialize(ACT_SEQ *records, int count);

    /**
     * Creates an empty action sequencer.
     */
    CActionSeq();

    /**
     * Clears all queues and playback state.
     */
    void ClearSeq();

    /**
     * Finds an unused action record in the pool.
     */
    ACT_SEQ *GetNextSeq();

    /**
     * Copies the character transform into the sequencer.
     */
    void SyncChara(CCharacter *target);

    /**
     * Appends an unused record to the movement queue.
     */
    ACT_SEQ *NextMoveSeq();

    /**
     * Appends an unused record to the motion queue.
     */
    ACT_SEQ *NextMotionSeq();

    /**
     * Appends an unused record to the texture animation queue.
     */
    ACT_SEQ *NextAnimeSeq();

    /**
     * Queues movement to a destination over the specified number of frames.
     */
    void MoveSeq(float *destination, int frames);

    /**
     * Queues movement to a destination at the specified speed.
     */
    void MoveSeq(float *destination, float speed);

    /**
     * Queues a wait at the last queued destination for the specified number of frames.
     */
    void MoveSeq(int frames);

    /**
     * Queues an immediate position change.
     */
    void SetPos(float *destination);

    /**
     * Queues rotation toward a reference position.
     */
    void RotRefSeq(float *reference, float speed);

    /**
     * Queues rotation toward a specified angle.
     */
    void RotAngleSeq(float angle, float speed);

    /**
     * Queues rotation along the movement direction.
     */
    void RotMoveSeq(float speed);

    /**
     * Queues cancellation of automatic rotation.
     */
    void ClearRotSeq();

    /**
     * Queues a wait for automatic rotation to finish.
     */
    void WaitRotSeq();

    /**
     * Queues an immediate rotation change.
     */
    void SetRot(float *angles);

    /**
     * Queues a delay before automatic rotation.
     */
    void SetDelayRot(int delay);

    /**
     * Queues a delayed motion trigger.
     */
    void MotionTrg(int delay);

    /**
     * Queues a delayed texture animation trigger.
     */
    void AnimeTrg(int delay);

    /**
     * Queues a character motion with playback parameters.
     */
    void MotionSeq(int motion, int mode, float speed, int flags);

    /**
     * Queues a texture animation change.
     */
    void AnimeSeq(int animation, int enabled, int frames, int disable_after);

    /**
     * Reports whether movement and animation queues have finished.
     */
    int CheckEnd();

    /**
     * Advances queued actions and updates the character transform.
     */
    void Play();
};

STATIC_ASSERT(sizeof(CActionSeq) == 0xD0);

} // namespace dc::game
