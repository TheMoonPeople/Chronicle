#pragma once

#include "common.h"

// clang-format off
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

// clang-format on

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
class CCharacter;

/**
 * Queues and advances character movement, rotation, and animation actions.
 */
class CActionSeq {
public:
    ACT_SEQ    *move_head; /**< First movement action. */
    ACT_SEQ    *move_tail; /**< Last movement action. */
    u8          unk_08[8];
    float       position[4];        /**< Current movement position. */
    float       start_position[4];  /**< Starting position for interpolation. */
    float       queued_position[4]; /**< Destination of the last queued movement. */
    int         move_frame;         /**< Elapsed movement frames. */
    int         anime_frame;        /**< Elapsed texture animation frames. */
    int         rotation_mode;      /**< Active automatic rotation: 0 none, 1 face the reference, 2 face the movement, 3 turn to an angle. */
    int         rotation_complete;  /**< Indicates that the rotation reached its target. */
    int         rotation_delay;     /**< Frames remaining before rotation advances. */
    int         motion_trigger;     /**< Enables queued motion playback. */
    int         motion_delay;       /**< Frames remaining before motion playback. */
    int         anime_trigger;      /**< Enables queued texture animation playback. */
    int         anime_delay;        /**< Frames remaining before texture animation playback. */
    u8          unk_64[12];
    float       rotation_target[4]; /**< Reference position or target angles for rotation; the last element holds the turn speed. */
    ACT_SEQ    *rotation_head;      /**< First rotation interpolation action. */
    ACT_SEQ    *rotation_tail;      /**< Last rotation interpolation action. */
    u8          unk_88[8];
    float       rotation[4];       /**< Current character rotation. */
    float       start_rotation[4]; /**< Starting rotation for interpolation. */
    int         rotation_frame;    /**< Elapsed rotation interpolation frames. */
    ACT_SEQ    *motion_head;       /**< First motion action. */
    ACT_SEQ    *motion_tail;       /**< Last motion action. */
    ACT_SEQ    *anime_head;        /**< First texture animation action. */
    ACT_SEQ    *anime_tail;        /**< Last texture animation action. */
    int         capacity;          /**< Number of records in the action pool. */
    ACT_SEQ    *pool;              /**< Storage for queued actions. */
    CCharacter *character;         /**< Character receiving the actions. */

    /**
     * Binds the action pool and clears playback state.
     *
     * @mangled Initialize__10CActionSeqFP7ACT_SEQi
     * @address 0x154B30
     * @size 0x4C
     */
    void Initialize(ACT_SEQ *records, int count);

    /**
     * Creates an empty action sequencer.
     *
     * @mangled __ct__10CActionSeqFv
     * @address 0x154B80
     * @size 0x38
     */
    CActionSeq();

    /**
     * Clears all queues and playback state.
     *
     * @mangled ClearSeq__10CActionSeqFv
     * @address 0x154BC0
     * @size 0x54
     */
    void ClearSeq();

    /**
     * Finds an unused action record in the pool.
     *
     * @mangled GetNextSeq__10CActionSeqFv
     * @address 0x154C20
     * @size 0x4C
     */
    ACT_SEQ *GetNextSeq();

    /**
     * Copies the character transform into the sequencer.
     *
     * @mangled SyncChara__10CActionSeqFP10CCharacter
     * @address 0x154C70
     * @size 0x8C
     */
    void SyncChara(CCharacter *target);

    /**
     * Appends an unused record to the movement queue.
     *
     * @mangled NextMoveSeq__10CActionSeqFv
     * @address 0x154D00
     * @size 0x68
     */
    ACT_SEQ *NextMoveSeq();

    /**
     * Appends an unused record to the motion queue.
     *
     * @mangled NextMotionSeq__10CActionSeqFv
     * @address 0x154D70
     * @size 0x68
     */
    ACT_SEQ *NextMotionSeq();

    /**
     * Appends an unused record to the texture animation queue.
     *
     * @mangled NextAnimeSeq__10CActionSeqFv
     * @address 0x154DE0
     * @size 0x68
     */
    ACT_SEQ *NextAnimeSeq();

    /**
     * Queues movement to a destination over the specified number of frames.
     *
     * @mangled MoveSeq__10CActionSeqFPfi
     * @address 0x154E50
     * @size 0x84
     */
    void MoveSeq(float *destination, int frames);

    /**
     * Queues movement to a destination at the specified speed.
     *
     * @mangled MoveSeq__10CActionSeqFPff
     * @address 0x154EE0
     * @size 0xB0
     */
    void MoveSeq(float *destination, float speed);

    /**
     * Queues a wait at the last queued destination for the specified number of frames.
     *
     * @mangled MoveSeq__10CActionSeqFi
     * @address 0x154F90
     * @size 0x28
     */
    void MoveSeq(int frames);

    /**
     * Queues an immediate position change.
     *
     * @mangled SetPos__10CActionSeqFPf
     * @address 0x154FC0
     * @size 0x78
     */
    void SetPos(float *destination);

    /**
     * Queues rotation toward a reference position.
     *
     * @mangled RotRefSeq__10CActionSeqFPff
     * @address 0x155040
     * @size 0x6C
     */
    void RotRefSeq(float *reference, float speed);

    /**
     * Queues rotation toward a specified angle.
     *
     * @mangled RotAngleSeq__10CActionSeqFff
     * @address 0x1550B0
     * @size 0x5C
     */
    void RotAngleSeq(float angle, float speed);

    /**
     * Queues rotation along the movement direction.
     *
     * @mangled RotMoveSeq__10CActionSeqFf
     * @address 0x155110
     * @size 0x44
     */
    void RotMoveSeq(float speed);

    /**
     * Queues cancellation of automatic rotation.
     *
     * @mangled ClearRotSeq__10CActionSeqFv
     * @address 0x155160
     * @size 0x34
     */
    void ClearRotSeq();

    /**
     * Queues a wait for automatic rotation to finish.
     *
     * @mangled WaitRotSeq__10CActionSeqFv
     * @address 0x1551A0
     * @size 0x34
     */
    void WaitRotSeq();

    /**
     * Queues an immediate rotation change.
     *
     * @mangled SetRot__10CActionSeqFPf
     * @address 0x1551E0
     * @size 0x5C
     */
    void SetRot(float *angles);

    /**
     * Queues a delay before automatic rotation.
     *
     * @mangled SetDelayRot__10CActionSeqFi
     * @address 0x155240
     * @size 0x40
     */
    void SetDelayRot(int delay);

    /**
     * Queues a delayed motion trigger.
     *
     * @mangled MotionTrg__10CActionSeqFi
     * @address 0x155280
     * @size 0x40
     */
    void MotionTrg(int delay);

    /**
     * Queues a delayed texture animation trigger.
     *
     * @mangled AnimeTrg__10CActionSeqFi
     * @address 0x1552C0
     * @size 0x40
     */
    void AnimeTrg(int delay);

    /**
     * Queues a character motion with playback parameters.
     *
     * @mangled MotionSeq__10CActionSeqFiifi
     * @address 0x155300
     * @size 0x78
     */
    void MotionSeq(int motion, int mode, float speed, int flags);

    /**
     * Queues a texture animation change.
     *
     * @mangled AnimeSeq__10CActionSeqFiiii
     * @address 0x155380
     * @size 0x7C
     */
    void AnimeSeq(int animation, int enabled, int frames, int disable_after);

    /**
     * Reports whether movement and animation queues have finished.
     *
     * @mangled CheckEnd__10CActionSeqFv
     * @address 0x155400
     * @size 0x68
     */
    int CheckEnd();

    /**
     * Advances queued actions and updates the character transform.
     *
     * @mangled Play__10CActionSeqFv
     * @address 0x1554A0
     * @size 0xB04
     */
    void Play();
};

STATIC_ASSERT(sizeof(CActionSeq) == 0xD0);
