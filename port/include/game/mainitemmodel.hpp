#pragma once

#include "common.h"

#include "frame.hpp"

/**
 * State of a held or thrown item model slot.
 */
// clang-format off
enum MainItemModelState {
    ITEM_MODEL_FREE   = -1, /**< Free. */
    ITEM_MODEL_LOADED = 0,  /**< Loaded. */
    ITEM_MODEL_HAND   = 1,  /**< In the hand. */
    ITEM_MODEL_THROWN = 2,  /**< Thrown. */
    ITEM_MODEL_UNK_3  = 3,  /**< Drawn on its own frame like a thrown item; never set. */
};

// clang-format on

/**
 * How an active item is used, as CActiveItemPack::CheckStatusType reports it.
 */
// clang-format off
enum ActiveItemUseType {
    ACTIVE_ITEM_NONE    = 0, /**< Not usable. */
    ACTIVE_ITEM_ACTION  = 1, /**< Used as an action. */
    ACTIVE_ITEM_DRINK   = 2, /**< Drunk. */
    ACTIVE_ITEM_FEATHER = 3, /**< Feather. */
    ACTIVE_ITEM_EAT     = 4, /**< Eaten. */
};

// clang-format on

class CMainItemModel {
public:
    u_int *cash[6];        /**< Model data each cache slot holds, or zero where the slot is free. */
    s32    cash_item[6];   /**< Item whose model each cache slot holds. */
    s32    cash_lock[6];   /**< Number of model slots that draw each cache slot's model. */
    s32    model[16];      /**< What each model slot holds; -1 where the slot is free. */
    s32    model_cash[16]; /**< Cache slot whose model each model slot draws. */
    u8     unk_0C8[8];
    CFrame frame[16];       /**< Frame that places each model slot's model. */
    float  velocity[16][4]; /**< Distance each thrown model moves in a frame. */
    s32    throw_time[16];  /**< Frames each thrown model has flown. */

    /**
     * Gives a free cache slot, or -1 where none is free.
     *
     * @mangled GetFreeCashNo__14CMainItemModelFv
     * @address 0x1D4540
     * @size 0x44
     */
    int GetFreeCashNo();

    /**
     * Gives a free model slot, or -1 where none is free.
     *
     * @mangled GetFreeModelNo__14CMainItemModelFv
     * @address 0x1D4590
     * @size 0x48
     */
    int GetFreeModelNo();

    /**
     * Reads one item model into a cache slot.
     *
     * @mangled SetCashModel__14CMainItemModelFiPUiPUii
     * @address 0x1D45E0
     * @size 0x190
     */
    int SetCashModel(int item_no, unsigned int *model_data, unsigned int *texture_data, int texture_size);

    /**
     * Releases one model slot.
     *
     * @mangled DeleteModel__14CMainItemModelFi
     * @address 0x1D4770
     * @size 0xA4
     */
    void DeleteModel(int model_no);

    /**
     * Puts one item model in the player's hand.
     *
     * @mangled SetHandModel__14CMainItemModelFi
     * @address 0x1D4820
     * @size 0x120
     */
    int SetHandModel(int source_no);

    /**
     * Releases every item model.
     *
     * @mangled AllReleasItem__14CMainItemModelFv
     * @address 0x1D4940
     * @size 0x78
     */
    void AllReleasItem();

    /**
     * Starts an item model flying from a position along a heading and returns its slot.
     *
     * @mangled SetThrowModel__14CMainItemModelFiPfPf
     * @address 0x1D49C0
     * @size 0x108
     */
    int SetThrowModel(int source_no, float *position, float *heading);

    /**
     * Draws every item model the player is carrying or has thrown.
     *
     * @mangled Draw__14CMainItemModelFv
     * @address 0x1D4AD0
     * @size 0x350
     */
    void Draw();

    /**
     * Advances every item model by a frame.
     *
     * @mangled Step__14CMainItemModelFv
     * @address 0x1D4E20
     * @size 0x520
     */
    void Step();

    /**
     * Clears every item model and cache slot.
     *
     * @mangled Initialize__14CMainItemModelFv
     * @address 0x1D5340
     * @size 0xE4
     */
    void Initialize();
};

/**
 * Names the model each item the player is running draws with.
 */
class CActiveItemPack {
public:
    s32             now;      /**< Slot the player is using now. */
    s32             item[4];  /**< Item each slot runs. */
    s32             model[9]; /**< Model each slot draws with, or -1 for none. */
    CMainItemModel *models;   /**< The pool the models come out of. */

    /**
     * Gives back how the item the player is using now is run.
     *
     * @mangled CheckStatusType__15CActiveItemPackFv
     * @address 0x1D5430
     * @size 0x144
     */
    int CheckStatusType();
};

STATIC_ASSERT(sizeof(CActiveItemPack) == 0x3C);

STATIC_ASSERT(sizeof(CMainItemModel) == 0x2810);

/** Pool that owns the active item models used during dungeon play. */
extern "C" CMainItemModel mainItemModel;

/**
 * Message logged when a cached model's last user lets it go.
 */
extern char MainItemRemoveMessage[];

/**
 * Message logged when a model is put in a hand.
 */
extern char MainItemHandMessage[];
