#pragma once

#include "common.h"

#include "character.hpp"

// clang-format off
/**
 * Selects the movement or wait operation in an NPC sequence record.
 */
enum NP_SEQUENCE_OPERATION {
    NP_SEQUENCE_UNUSED = 0,
    NP_SEQUENCE_MOVE = 1,
    NP_SEQUENCE_WAIT = 2
};

// clang-format on

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
     *
     * @mangled Step__12CNPCharacterFv
     * @address 0x155FB0
     * @size 0x134
     */
    virtual void Step();

    /**
     * Advances the shadow while the NPC is visible.
     *
     * @mangled ShadowStep__12CNPCharacterFv
     * @address 0x1560F0
     * @size 0x4C
     */
    virtual void ShadowStep();

    /**
     * Advances the current movement or wait action.
     *
     * @mangled PlaySeq__12CNPCharacterFv
     * @address 0x156140
     * @size 0x210
     */
    void PlaySeq();

    /**
     * Clears the action ring and enables sequence playback.
     *
     * @mangled ClearSeq__12CNPCharacterFv
     * @address 0x156350
     * @size 0x48
     */
    void ClearSeq();

    /**
     * Queues movement to a destination at the specified speed.
     *
     * @mangled SetSeq__12CNPCharacterFPff
     * @address 0x1563A0
     * @size 0x6C
     */
    int SetSeq(float *destination, float speed);

    /**
     * Queues a wait lasting the specified frame count.
     *
     * @mangled SetWait__12CNPCharacterFi
     * @address 0x156410
     * @size 0x3C
     */
    int SetWait(int frames);

    /**
     * Reports whether the action ring contains pending entries.
     *
     * @mangled CheckSeq__12CNPCharacterFv
     * @address 0x156450
     * @size 0x18
     */
    int CheckSeq();

    /**
     * Returns the next writable action record and advances the write index.
     *
     * @mangled GetNextSeq__12CNPCharacterFv
     * @address 0x156470
     * @size 0x3C
     */
    NP_SEQUENCE *GetNextSeq();

    /**
     * Returns the current action record.
     *
     * @mangled GetNowSeq__12CNPCharacterFv
     * @address 0x1564B0
     * @size 0x20
     */
    NP_SEQUENCE *GetNowSeq();

    /**
     * Releases the current action and advances the read index.
     *
     * @mangled NextSeq__12CNPCharacterFv
     * @address 0x1564D0
     * @size 0x64
     */
    void NextSeq();

    /**
     * Draws the NPC with its ambient colour and alpha overrides.
     *
     * @mangled Draw__12CNPCharacterFv
     * @address 0x156540
     * @size 0xD8
     */
    virtual void Draw();

    /**
     * Draws the shadow while the NPC is visible.
     *
     * @mangled DrawShadow__12CNPCharacterFv
     * @address 0x156620
     * @size 0x4C
     */
    virtual void DrawShadow();

    /**
     * Reports whether the enabled NPC has a model and positive alpha.
     *
     * @mangled CheckDraw__12CNPCharacterFv
     * @address 0x156670
     * @size 0x58
     */
    int CheckDraw();

    /**
     * Builds collision polygons for the NPC when enabled.
     *
     * @mangled PickUpPoly__12CNPCharacterFPfP6CCPoly
     * @address 0x1566D0
     * @size 0x38
     */
    virtual int PickUpPoly(float *position, CCPoly *polygons);

    /**
     * Initializes the NPC state and clears queued actions.
     *
     * @mangled Initialize__12CNPCharacterFv
     * @address 0x1569E0
     * @size 0x8C
     */
    virtual void Initialize();

    /**
     * Constructs a non-player character.
     *
     * @mangled __ct__12CNPCharacterFv
     * @address 0x156A70
     * @size 0xEC
     */
    CNPCharacter();
};

STATIC_ASSERT(sizeof(CNPCharacter) == 0x14A0);

/** Runtime character objects for the ten villagers selected for the current period. */
extern CNPCharacter EdVillager[10];
