#pragma once

#include "common.h"

#include "character.hpp"
#include "dataalloc_fwd.hpp"

/**
 * Loads, draws and advances one Dran field model.
 */
class CDranMapField {
public:
    CCharacter character; /**< Character data for one Dran field model. */

    /**
     * Initializes this Dran field model's character data.
     */
    void Initialize() {
        character.Initialize();
    }

    /**
     * Loads model data into this Dran field model's character.
     */
    void LoadPackData(unsigned int *pack, char *name, CDataAlloc2<1> *model_alloc, CDataAlloc2<1> *texture_alloc) {
        character.LoadPackData(pack, name, model_alloc, texture_alloc);
    }

    /**
     * Sets this Dran field model's world position.
     */
    void SetPosition(float x, float y, float z) {
        character.SetPosition(x, y, z);
    }

    /**
     * Sets this Dran field model's world rotation.
     */
    void SetRotation(float x, float y, float z) {
        character.SetRotation(x, y, z);
    }

    /**
     * Loads one Dran field model into the next free slot.
     *
     * @mangled LoadModel__13CDranMapFieldFPUiP14CDataAlloc2_1_
     * @address 0x1CD3C0
     * @size 0x14C
     */
    void LoadModel(unsigned int *pack, CDataAlloc2<1> *arena);

    /**
     * Appends polygons near the active Dran field collision frames.
     *
     * @mangled AddCollision__13CDranMapFieldFP6CCPolyi7CBoxVu0
     * @address 0x1CD510
     * @size 0xF8
     */
    int AddCollision(CCPoly *poly, int count, CBoxVu0 box);

    /**
     * Loads one Dran field collision frame into the next free slot.
     *
     * @mangled LoadCollision__13CDranMapFieldFPUiP14CDataAlloc2_1_
     * @address 0x1CD610
     * @size 0x110
     */
    void LoadCollision(unsigned int *pack, CDataAlloc2<1> *arena);

    /**
     * Draws every active Dran field model that has finished loading.
     *
     * @mangled Draw__13CDranMapFieldFv
     * @address 0x1CD720
     * @size 0xAC
     */
    void Draw();

    /**
     * Advances the timed state of every loaded Dran field model.
     *
     * @mangled Step__13CDranMapFieldFv
     * @address 0x1CD7D0
     * @size 0x134
     */
    void Step();
};

STATIC_ASSERT(sizeof(CDranMapField) == 0x11B0);

/**
 * Holds the Dran field models, their collision frames and their runtime
 * states.
 */
struct DRAN_MAP_FIELD_SET {
    CDranMapField field[12];       /**< Models drawn for the Dran fields. */
    CFrame       *collision[12];   /**< Collision frame each field stands on. */
    s32           state[12];       /**< What each field is doing; counts down while its motion runs. */
    s32           field_count;     /**< Fields the floor laid out. */
    s32           collision_count; /**< Collision frames read for them. */
    u8            unk_D4A8[8];
};

STATIC_ASSERT(sizeof(DRAN_MAP_FIELD_SET) == 0xD4B0);

/**
 * Dran field models and runtime state for the current dungeon.
 */
extern "C" DRAN_MAP_FIELD_SET DranMapField;

/**
 * Message LoadModel prints when a field's model table overflows.
 */
extern char OverMessage[];
