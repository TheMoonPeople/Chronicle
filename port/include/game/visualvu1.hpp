#pragma once

#include "common.h"

#include <libvu0.h>

#include "vu1.hpp"

struct sceVif1Packet;

namespace dc::game {

struct RenderInfo;

class CVisualVu1 {
public:
    s32 flags; /**< Draw flags of the base visual, cleared when it is initialised. */
    s32 unk_04;

    CVisualVu1 &operator=(const CVisualVu1 &src);

    /**
     * Clears the vector-unit visual's packet pointers and sizes.
     */
    virtual void Initialize();

    /**
     * Takes the address that the model data of the object is read from. The
     * base object keeps none.
     */
    virtual void SetMDTDataAddress(unsigned int *data);

    /**
     * Gets the address that the model data of the object is read from. The
     * base object keeps none.
     */
    virtual unsigned int *GetMDTDataAddress();

    /**
     * Returns zero because this visual has no retained model data to rebuild.
     */
    virtual int RemakeData(unsigned int *data);

    /**
     * Draws the visual into a packet through the vector unit.
     */
    virtual int DrawVu1(unsigned int *packet, float (*matrix)[4], RenderInfo *info, VU1_PROGRAM program, u_long128 *draw_state, int unknown1, int unknown2);

    /**
     * Draws the visual into a VIF packet.
     */
    virtual int DrawVu1(sceVif1Packet *packet, float (*matrix)[4], RenderInfo *info, VU1_PROGRAM program, u_long128 *draw_state, int unknown1, int unknown2);

    s32 unk_0C;

    /**
     * Constructs a vector-unit visual and clears it.
     */
    CVisualVu1();

    /**
     * Builds a model's VU data block and returns its size in quadwords.
     */
    int CreateVUdataFromMDT(unsigned int *block, unsigned int *data, int unknown0, int unknown1);

    /**
     * Rebuilds a VU data block from retained model data and returns its size in quadwords.
     */
    int CreateVUdataFromMDTRemake(unsigned int *block, unsigned int *data, int unknown0);
};

class CVisualPolyVu1 : public CVisualVu1 {
public:
    s32    unk_10;
    s32    unk_14;
    u_int *vu_data; /**< Vector-unit packet the polygons are drawn from. */
    u_int  vu_size; /**< Size of the vector-unit packet in quadwords. */

    CVisualPolyVu1 &operator=(const CVisualPolyVu1 &src);
} __attribute__((aligned(16)));

STATIC_ASSERT(sizeof(CVisualPolyVu1) == 0x20);

class CVisual {
public:
    int unk_00;
    int unk_04;

    /**
     * Copies the visual's scalar state and returns this visual.
     */
    CVisual &operator=(const CVisual &other);

    /**
     * Constructs a visual and clears it.
     */
    CVisual();

    virtual void Initialize();

    int unk_0C;
};

} // namespace dc::game
