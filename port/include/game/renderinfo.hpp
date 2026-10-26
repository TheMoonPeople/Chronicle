#pragma once

#include "common.h"

#include <libvu0.h>

/**
 * Everything a frame is drawn against, kept in one object so that a draw takes a single pointer:
   the camera, the screen it is projected onto, the lights and the fog. Most of it is derived
   rather than set — the two matrices a caller supplies are the view and the projection, and the
   rest is recomputed from them whenever either changes.

   The block from scale to far is one clip volume expressed per axis rather than four
   vectors: x, y and z each get a scale, an offset and the two bounds they are held between.
 */
struct RenderInfo {
    float         projection; /**< Projection scale the render info was last built from. */
    int           unk_04[3];
    sceVu0FMATRIX view_scaled;     /**< View matrix with the screen's aspect squeeze applied. */
    sceVu0FMATRIX screen;          /**< Camera-space to screen-space projection matrix. */
    sceVu0FMATRIX view;            /**< View matrix the camera supplied. */
    sceVu0FMATRIX view_screen;     /**< World to screen matrix: screen times view_scaled. */
    sceVu0FMATRIX light_direction; /**< Direction of each parallel light, one per row. */
    sceVu0FMATRIX light_color;     /**< Colour of each parallel light, one per row. */
    sceVu0FVECTOR ambient;         /**< Ambient light colour. */
    sceVu0FVECTOR scale;           /**< Screen scale on x and y and depth scale on z. */
    sceVu0FVECTOR offset;          /**< Screen centre on x and y and depth offset on z. */
    sceVu0FVECTOR near;            /**< Lower screen bounds on x and y and near clip depth on z. */
    sceVu0FVECTOR far;             /**< Upper screen bounds on x and y and far clip depth on z. */
    sceVu0FVECTOR position;        /**< Camera position. */
    sceVu0FVECTOR view_position;   /**< Eye position a caller supplied with the view matrix. */
    float         pitch;           /**< Camera pitch derived from the view matrix. */
    float         yaw;             /**< Camera yaw derived from the view matrix. */
    int           unk_208[2];
    sceVu0FVECTOR clip_max;    /**< Upper clip bounds sent to the microprograms, far depth in w. */
    sceVu0FVECTOR clip_min;    /**< Lower clip bounds sent to the microprograms, near depth in w. */
    sceVu0FMATRIX perspective; /**< Perspective matrix into clip space. */
    sceVu0FMATRIX viewport;    /**< Clip space to GS screen coordinates matrix. */
    /**
 * The plane a frame's shadow is cast onto, kept as a point and a normal, and the projection
       onto it that ShadowMatrix derives from the pair and the first light direction.
 */
    sceVu0FVECTOR shadow_point;  /**< Point on the plane shadows are cast onto. */
    sceVu0FVECTOR shadow_normal; /**< Normal of the plane shadows are cast onto. */
    sceVu0FMATRIX shadow;        /**< Projection onto the shadow plane along the first light. */
    /**
 * The block a draw fills in on its way through rather than one the camera setup owns: every
       field from here to the fog is written by CFrameVu1::DrawVu1 out of the frame's own
       attributes, so what a microprogram reads here belongs to the frame being drawn and not to
       the scene. unk_320 is the exception and is a switch the caller sets.
 */
    int   clip_flags;  /**< Microprogram flags: 1 clips, 2 scissors, 4 is the frame's own switch. */
    int   scissor;     /**< Whether the frame being drawn crosses the near plane and must be scissored. */
    int   scissoring;  /**< Forces scissoring of every frame that crosses the near plane. */
    float frame_far_z; /**< Far depth from the attributes of the frame last drawn. */
    int   shadow_pass; /**< Pass being drawn: 0 normal, 1 fast shadow, 2 shadow, 8 shade. */
    int   fog_enabled; /**< Whether the frame being drawn takes fog. */
    /**
 * Fog is applied as a + b/z, which is what makes both ends of it a division the caller never
       does: MGSetFogParm takes the two depths and the two densities and solves for the pair.
 */
    float  fog_a;     /**< Constant term of the fog factor. */
    float  fog_b;     /**< Coefficient of 1/z in the fog factor. */
    float  fog_far;   /**< Fog amount at the far fog depth. */
    float  fog_near;  /**< Fog amount at the near fog depth. */
    u_char fog_red;   /**< Red component of the fog colour. */
    u_char fog_green; /**< Green component of the fog colour. */
    u_char fog_blue;  /**< Blue component of the fog colour. */
    u_char unk_33B;
    int    eye_in_model; /**< Whether the frame being drawn is sent the eye position in model space for reflection. */
    int    unlit;        /**< Draws every frame without lighting while nonzero. */
    int    unk_344[3];

    RenderInfo() {
        scissor = false;
        clip_flags = 0;
        scissoring = 0;
        frame_far_z = 0.0f;
        shadow_pass = 0;
        fog_enabled = false;
        fog_b = 0.0f;
        fog_a = 0.0f;
        fog_far = 0.0f;
        fog_near = 255.0f;
        fog_blue = 255;
        fog_green = 255;
        fog_red = 255;
        unlit = 0;
    }
};
