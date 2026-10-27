#pragma once

#include "common.h"

#include <libvu0.h>

#include "dataalloc_fwd.hpp"
#include "frame.hpp"
#include "framevu1.hpp"
#include "visualvu1.hpp"

namespace dc::game {

/**
 * Draws the water surface of one map part.
 */
class CWater {
public:
    s32            rows;         /**< Grid rows the surface is divided into. */
    s32            columns;      /**< Grid columns the surface is divided into. */
    float         *height;       /**< Wave heights the surface currently draws from. */
    float         *height_a;     /**< First of the two wave-height buffers. */
    float         *height_b;     /**< Second of the two wave-height buffers. */
    sceVu0FVECTOR  vertex[4];    /**< The four corners of the surface. */
    u_int         *packet[3];    /**< VU packets, including one for each display buffer. */
    CVisualPolyVu1 visual;       /**< Draws the polygons of the surface. */
    u8             color[4];     /**< Red, green, blue and alpha channels of the surface. */
    float          wave_speed;   /**< Speed the ripples travel across the grid at. */
    float          damping;      /**< Rate the ripples lose height at. */
    float          height_scale; /**< Scale from a wave height to the vertical displacement of its vertex. */
    float          distortion;   /**< Scale from a height difference to the texture-coordinate shift it causes. */
    s32            tags_built;   /**< Set once both packets hold their tags, so rebuilds write only vertex data. */
    CFrameVu1      frame;        /**< Places and draws the water surface. */

    /**
     * Copies one surface over another, field by field.
     */
    CWater &operator=(CWater &src);

    /**
     * Stores the ripple speed and damping and the two scales the surface is
     * drawn with.
     */
    void SetParam(float speed, float damping_rate, float scale, float shift);

    /**
     * Sets the red, green, blue and alpha channels of the surface.
     */
    void SetColor(unsigned char red, unsigned char green, unsigned char blue, unsigned char alpha);

    /**
     * Builds the vector-unit packet that draws the surface, one quad per grid
     * cell.
     */
    int CreateVUData(unsigned int *output, RenderInfo *info);

    /**
     * Draws the surface through the vector unit.
     */
    int DrawVu1(RenderInfo *info, sceVif1Packet *packet, RenderInfo *parent_info);

    /**
     * Clips the surface against the view using the world-space box its four
     * corners span.
     */
    int CheckClip();

    /**
     * Advances the ripples one step, writing into whichever of the two height
     * buffers is not being drawn.
     */
    void Hamon();

    /**
     * Copies four corner positions into the water surface.
     */
    void SetVertex(float *corner0, float *corner1, float *corner2, float *corner3);

    /**
     * Raises one interior grid cell, starting a ripple from it.
     */
    void Shake(int row, int column, float height_change);

    /**
     * Gives the surface its grid size and takes the two height buffers out of
     * an arena.
     */
    void SetSize(int row_count, int column_count, CDataAlloc2<1> *arena);

    /**
     * Clears the grid, gives the surface a half-bright white and sets the
     * ripple constants.
     */
    CWater();
};

STATIC_ASSERT(sizeof(CWater) == 0x320);

/**
 * Draws a water surface through the vector unit under retail's symbol name,
 * which the CWater::DrawVu1 member cannot carry.
 */
extern "C" int DrawVu1__6CWaterFP10RenderInfoP13sceVif1PacketP1(CWater *water, RenderInfo *info, sceVif1Packet *draw_packet, void *parent_info);

} // namespace dc::game
