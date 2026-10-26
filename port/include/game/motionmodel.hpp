#pragma once

#include "common.h"

#include "dataalloc_fwd.hpp"
#include "gameutil.hpp"

// Forward declarations for the types these declarations name. The skeleton
// headers are generated from the retail symbol table, which knows the type
// names but not where they live.
class CMotionModel {
public:
    CFrame        *frame; /**< Frame that draws the model. */
    u8             unk_04[0xC];
    tagMOTION_TYPE motion;         /**< Motion data and playback state for the model. */
    int            current_motion; /**< Motion selected for the next playback step. */
    u8             unk_94[0xC];

    /**
     * Reads one model and its motions out of an archive into two arenas.
     *
     * @mangled LoadPack__12CMotionModelFPUiPcP14CDataAlloc2_1_P14CDataAlloc2_1_P11MOTION_INFOi
     * @address 0x1B6A30
     * @size 0x2A0
     */
    void LoadPack(unsigned int *pack, char *base_name, CDataAlloc2<1> *model_arena, CDataAlloc2<1> *motion_arena, MOTION_INFO *motion_info, int initialize_frames);

    /**
     * Advances the model's motion by a frame.
     *
     * @mangled Step__12CMotionModelFv
     * @address 0x1B6CD0
     * @size 0x68
     */
    void Step();

    /**
     * Draws the model.
     *
     * @mangled Draw__12CMotionModelFv
     * @address 0x1B6D40
     * @size 0x2C
     */
    void Draw();
};

STATIC_ASSERT(sizeof(CMotionModel) == 0xA0);

/**
 * Gives the attribute that one weapon element hits with.
 *
 * @mangled GetWeaponElementAttr__Fi
 * @address 0x1B69F0
 * @size 0x34
 */
int GetWeaponElementAttr(int element);
