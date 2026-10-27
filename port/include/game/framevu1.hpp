#pragma once

#include "common.h"

#include "frame.hpp"

namespace dc::game {

class CVisualVu1;

/**
 * Draws a model with the VU1 microprogram.
 */
class CFrameVu1 : public CFrame {
public:
    CVisualVu1 *visual; /**< Model data drawn for this frame. */

    /**
     * Copies the frame and the model it draws.
     */
    CFrameVu1 &operator=(CFrameVu1 &other);

    /**
     * Draws the frame's model; always returns 0.
     */
    virtual int DrawVu1(unsigned int *packet, RenderInfo *info);

    /**
     * Draws the frame's model, ignoring the packet; always returns 0.
     */
    virtual int DrawVu1(sceVif1Packet *packet, RenderInfo *info);

    /**
     * Sets the model the frame draws.
     */
    void SetVisual(CVisualVu1 *visual);

    /**
     * Returns the model the frame draws.
     */
    CVisualVu1 *GetVisual();

    /**
     * Resets the frame and detaches its model.
     */
    virtual void Initialize();

    /**
     * Constructs a frame with no model.
     */
    CFrameVu1();
};

STATIC_ASSERT(sizeof(CFrameVu1) == 0x270);

} // namespace dc::game
