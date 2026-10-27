#pragma once

#include "common.h"

#include "dataalloc_fwd.hpp"
#include "gameutil.hpp"

namespace dc::game {

/**
 * A model with a skeleton and a set of motions, loaded together from one pack.
 */
class CMotionModel {
public:
    CFrame        *frame; /**< Frame that draws the model. */
    u8             unk_04[0xC];
    tagMOTION_TYPE motion;         /**< Motion data and playback state for the model. */
    int            current_motion; /**< Motion selected for the next playback step. */
    u8             unk_94[0xC];

    /**
     * Reads one model and its motions out of an archive into two arenas.
     */
    void LoadPack(unsigned int *pack, const char *base_name, CDataAlloc2<1> *model_arena, CDataAlloc2<1> *motion_arena, MOTION_INFO *motion_info, int initialize_frames);

    /**
     * Advances the model's motion by a frame.
     */
    void Step();

    /**
     * Draws the model.
     */
    void Draw();
};

STATIC_ASSERT(sizeof(CMotionModel) == 0xA0);

/**
 * Gives the attribute that one weapon element hits with.
 */
int GetWeaponElementAttr(int element);

} // namespace dc::game
