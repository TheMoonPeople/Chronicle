#pragma once

#include "common.h"

#include <libvu0.h>

// Forward declarations for the types these declarations name. The skeleton
// headers are generated from the retail symbol table, which knows the type
// names but not where they live.
class CCamera;
class CTexture;

/**
 * Draws a fire that gives off light in every direction, and the raster that it
 * casts on the floor.
 */
class CFireOmni {
public:
    s32           unk_00;
    float         raster_phase;   /**< Animation phase of the raster cast on the floor. */
    float         flame_phase;    /**< Animation phase of the flame, wrapping at 128. */
    float         cell_phase;     /**< Texture cell the flame draws, counting down to zero. */
    float         flicker_width;  /**< Random width scale held between flicker steps. */
    float         flicker_height; /**< Random height scale held between flicker steps. */
    s16           flicker_seed;   /**< Seed the flicker draw restarts the generator from. */
    s16           unk_1A;
    s32           flicker_count; /**< Frames drawn since the flicker scales were last chosen. */
    sceVu0FVECTOR pos;           /**< World position that the fire draws at. */
    s32           texture_set;   /**< Indicates that the textures were supplied rather than looked up. */
    CTexture     *core;          /**< Bright inner texture of the flame. */
    CTexture     *glow;          /**< Soft outer texture of the flame. */
    s32           unk_3C;

    /**
     * Clears the fire's animation state and gives its cell phase a random
     * start.
     *
     * @mangled __ct__9CFireOmniFv
     * @address 0x161650
     * @size 0x94
     */
    CFireOmni();

    /**
     * Advances the flame and cell animation phases and chooses the seed the
     * next flicker draws from.
     *
     * @mangled FireStep__9CFireOmniFv
     * @address 0x1616F0
     * @size 0xD0
     */
    void FireStep();

    /**
     * Composes the flame texture for the current cell phase out of the sheet
     * the texture manager holds.
     *
     * @mangled FireCreate__9CFireOmniFv
     * @address 0x1617C0
     * @size 0x2D8
     */
    void FireCreate();

    /**
     * Supplies the two flame textures, so that drawing does not look them up
     * for itself.
     *
     * @mangled SetTexture__9CFireOmniFP8CTextureP8CTexture
     * @address 0x161AA0
     * @size 0x18
     */
    void SetTexture(CTexture *core_texture, CTexture *glow_texture);

    /**
     * Draws the flame as camera-facing sprites, one layer per bit of the layer
     * mask.
     *
     * @mangled DrawFire__9CFireOmniFiiP7CCameraPffif
     * @address 0x161AC0
     * @size 0x7C0
     */
    void DrawFire(int unused0, int unused1, CCamera *camera, float *colour, float scale, int layers, float camera_offset);

    /**
     * Advances and wraps the fire raster animation phase.
     *
     * @mangled RasterStep__9CFireOmniFv
     * @address 0x162280
     * @size 0x84
     */
    void RasterStep();

    /**
     * Draws the light the fire casts on the floor, clipped to the screen.
     *
     * @mangled DrawRaster__9CFireOmniFv
     * @address 0x162310
     * @size 0x26C
     */
    void DrawRaster();
};

STATIC_ASSERT(sizeof(CFireOmni) == 0x40);
