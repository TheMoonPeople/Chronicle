#pragma once

#include "common.h"

#include <libvu0.h>

#include "vu1.hpp"

// Forward declarations for the types these declarations name. The skeleton
// headers are generated from the retail symbol table, which knows the type
// names but not where they live.
struct RenderInfo;
struct sceVif1Packet;

class CVisualVu1 {
public:
    s32 flags; /**< Draw flags of the base visual, cleared when it is initialised. */
    s32 unk_04;

    CVisualVu1 &operator=(const CVisualVu1 &src);

    /**
     * Clears the vector-unit visual's packet pointers and sizes.
     *
     * @mangled Initialize__10CVisualVu1Fv
     * @address 0x134EC0
     * @size 0x3C
     */
    virtual void Initialize();

    /**
     * Takes the address that the model data of the object is read from. The
     * base object keeps none.
     *
     * @mangled SetMDTDataAddress__10CVisualVu1FPUi
     * @address 0x137E80
     * @size 0x8
     */
    virtual void SetMDTDataAddress(unsigned int *data);

    /**
     * Gets the address that the model data of the object is read from. The
     * base object keeps none.
     *
     * @mangled GetMDTDataAddress__10CVisualVu1Fv
     * @address 0x137E90
     * @size 0xC
     */
    virtual unsigned int *GetMDTDataAddress();

    /**
     * Returns zero because this visual has no retained model data to rebuild.
     *
     * @mangled RemakeData__10CVisualVu1FPUi
     * @address 0x134BB0
     * @size 0xC
     */
    virtual int RemakeData(unsigned int *data);

    /**
     * Draws the visual into a packet through the vector unit.
     *
     * @mangled DrawVu1__10CVisualVu1FPUiPA4_fP10RenderInfo11VU1_PROGRAMP1ii
     * @address 0x135000
     * @size 0x964
     */
    virtual int DrawVu1(unsigned int *packet, float (*matrix)[4], RenderInfo *info, VU1_PROGRAM program, u_long128 *draw_state, int unknown1, int unknown2);

    /**
     * Draws the visual into a VIF packet.
     *
     * @mangled DrawVu1__10CVisualVu1FP13sceVif1PacketPA4_fP10RenderInfo11VU1_PROGRAMP1ii
     * @address 0x134BC0
     * @size 0xC4
     */
    virtual int DrawVu1(sceVif1Packet *packet, float (*matrix)[4], RenderInfo *info, VU1_PROGRAM program, u_long128 *draw_state, int unknown1, int unknown2);

    s32 unk_0C;

    /**
     * Constructs a vector-unit visual and clears it.
     *
     * @mangled __ct__10CVisualVu1Fv
     * @address 0x134F00
     * @size 0x50
     */
    CVisualVu1();

    /**
     * Builds a model's VU data block and returns its size in quadwords.
     *
     * @mangled CreateVUdataFromMDT__10CVisualVu1FPUiPUiii
     * @address 0x135AA0
     * @size 0x3A8
     */
    int CreateVUdataFromMDT(unsigned int *block, unsigned int *data, int unknown0, int unknown1);

    /**
     * Rebuilds a VU data block from retained model data and returns its size in quadwords.
     *
     * @mangled CreateVUdataFromMDTRemake__10CVisualVu1FPUiPUii
     * @address 0x135E50
     * @size 0x288
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
     *
     * @mangled __as__7CVisualFRC7CVisual
     * @address 0x1433F0
     * @size 0x1C
     */
    CVisual &operator=(const CVisual &other);

    /**
     * Constructs a visual and clears it.
     *
     * @mangled __ct__7CVisualFv
     * @address 0x134B60
     * @size 0x44
     */
    CVisual();

    virtual void Initialize();

    int unk_0C;
};
