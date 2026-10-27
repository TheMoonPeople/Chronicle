#pragma once

#include "common.h"

#include "character.hpp"

namespace dc::game {

/**
 * Selects the movement or wait operation in an NPC sequence record.
 */
enum class NP_SEQUENCE_OPERATION : s32 {
    UNUSED = 0,
    MOVE = 1,
    WAIT = 2
};

/**
 * Stores the destination and timing of one queued NPC action.
 */
struct NP_SEQUENCE {
    NP_SEQUENCE_OPERATION operation;   /**< Operation performed by the record. */
    int                   wait_frames; /**< Frames remaining for a wait action. */
    u8                    unk_08[8];
    float                 destination[4]; /**< Destination position of a movement action. */
    float                 speed[3];       /**< Movement speed values set for each coordinate. */
    u8                    unk_2C[0x24];
};

STATIC_ASSERT(sizeof(NP_SEQUENCE) == 0x50);

/**
 * Controls a non-player character and its queued movement actions.
 */
class CNPCharacter : public CCharacter {
public:
    int         sequence_enabled; /**< Enables advancement of queued actions. */
    int         read_index;       /**< Index of the current action. */
    int         write_index;      /**< Index of the next record to fill. */
    u8          unk_11BC[4];
    NP_SEQUENCE sequences[8];        /**< Circular storage for movement and wait actions. */
    s32         map_parts_no;        /**< Edited-map part associated with the villager, or a negative value. */
    s32         villager_id;         /**< Villager-table identifier represented by this event NPC. */
    char        resource_name[0x20]; /**< Resource name of the model loaded for the villager. */
    s32         talk_target;         /**< Whether the player stands close enough and faces the villager to talk to it. */
    s32         initialized;         /**< Whether the villager model has been initialized for use. */
    s32         draw_enabled;        /**< Whether the villager model participates in drawing and movement. */
    s32         near_camera;         /**< Whether proximity to the camera requests this villager's full update. */
    int         step_hidden;         /**< Steps the character while it is not visible. */
    s32         event_status;        /**< Status value exposed to an event script for the talking NPC. */
    s32         unk_1480;
    int         alpha_step;           /**< Default alpha change per frame. */
    s32         alpha_step_override;  /**< Alpha change for the next step only, replacing alpha_step, or -1. */
    s32         texture_block;        /**< Texture block the model's textures and texture animations live in. */
    s32         recurring_talk_event; /**< Event number used for repeated conversations with this villager. */
    u8          unk_1494[0xC];

    /**
     * Advances the character, its fade, and its movement sequence.
     */
    virtual void Step();

    /**
     * Advances the shadow while the NPC is visible.
     */
    virtual void ShadowStep();

    /**
     * Advances the current movement or wait action.
     */
    void PlaySeq();

    /**
     * Clears the action ring and enables sequence playback.
     */
    void ClearSeq();

    /**
     * Queues movement to a destination at the specified speed.
     */
    int SetSeq(float *destination, float speed);

    /**
     * Queues a wait lasting the specified frame count.
     */
    int SetWait(int frames);

    /**
     * Reports whether the action ring contains pending entries.
     */
    int CheckSeq();

    /**
     * Returns the next writable action record and advances the write index.
     */
    NP_SEQUENCE *GetNextSeq();

    /**
     * Returns the current action record.
     */
    NP_SEQUENCE *GetNowSeq();

    /**
     * Releases the current action and advances the read index.
     */
    void NextSeq();

    /**
     * Draws the NPC with its ambient colour and alpha overrides.
     */
    virtual void Draw();

    /**
     * Draws the shadow while the NPC is visible.
     */
    virtual void DrawShadow();

    /**
     * Reports whether the enabled NPC has a model and positive alpha.
     */
    int CheckDraw();

    /**
     * Builds collision polygons for the NPC when enabled.
     */
    virtual int PickUpPoly(float *position, CCPoly *polygons);

    /**
     * Initializes the NPC state and clears queued actions.
     */
    virtual void Initialize();

    /**
     * Constructs a non-player character.
     */
    CNPCharacter();
};

STATIC_ASSERT(sizeof(CNPCharacter) == 0x14A0);

/**
 * Runtime character objects for the ten villagers selected for the current period.
 */
extern CNPCharacter EdVillager[10];

} // namespace dc::game
