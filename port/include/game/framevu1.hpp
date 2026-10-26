#pragma once

#include "common.h"

#include "frame.hpp"

// Forward declarations for the types these declarations name. The skeleton
// headers are generated from the retail symbol table, which knows the type
// names but not where they live.
class CVisualVu1;

/**
 * Draws a model with the VU1 microprogram.
 */
class CFrameVu1 : public CFrame {
public:
    CVisualVu1 *visual; /**< Model data drawn for this frame. */

    /**
     * @mangled __as__9CFrameVu1FR9CFrameVu1
     * @address 0x12A2B0
     * @size 0x44
     * @unknownret
     */
    CFrameVu1 &operator=(CFrameVu1 &other);

    /**
     * @mangled DrawVu1__9CFrameVu1FPUiP10RenderInfo
     * @address 0x129400
     * @size 0xD10
     * @unknownret
     */
    virtual int DrawVu1(unsigned int *packet, RenderInfo *info);

    /**
     * @mangled DrawVu1__9CFrameVu1FP13sceVif1PacketP10RenderInfo
     * @address 0x12A110
     * @size 0x7C
     * @unknownret
     */
    virtual int DrawVu1(sceVif1Packet *packet, RenderInfo *info);

    /**
     * @mangled SetVisual__9CFrameVu1FP10CVisualVu1
     * @address 0x12A210
     * @size 0xC
     * @unknownret
     */
    void SetVisual(CVisualVu1 *visual);

    /**
     * @mangled GetVisual__9CFrameVu1Fv
     * @address 0x12A220
     * @size 0xC
     * @unknownret
     */
    CVisualVu1 *GetVisual();

    /**
     * @mangled Initialize__9CFrameVu1Fv
     * @address 0x12A230
     * @size 0x30
     * @unknownret
     */
    virtual void Initialize();

    /**
     * @mangled __ct__9CFrameVu1Fv
     * @address 0x12A260
     * @size 0x48
     */
    CFrameVu1();
};

STATIC_ASSERT(sizeof(CFrameVu1) == 0x270);
