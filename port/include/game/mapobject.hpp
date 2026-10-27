#pragma once

#include "common.h"

#include "objectframe.hpp"

namespace dc::game {

/**
 * @file
 * Declares the object that a map places, with the frames that give it a
 * shadow, a shade and a collision shape.
 */

class CFrame;

/**
 * Draws one object of a map, together with the shadow it casts, the shade
 * that lies on it and the shape that collision reads.
 */
class CMapObject : public CObjectFrame {
public:
    CFrame *collision_frame; /**< Frame that collision reads; null where the object has none. */
    CFrame *shadow_frame;    /**< Frame that the shadow draws from; null where the object casts none. */
    CFrame *shade_frame;     /**< Frame that the shade draws from; null where the object takes none. */
    CFrame *camera_frame;    /**< Collision frame the camera reads; null where the object has none. */
    float   shadow_offset;   /**< Height that the shadow drops below the object. */
    s32     category_no;     /**< Category whose level-of-detail range and draw pass the object uses; below zero for a free slot. */
    s32     handle;          /**< Handle the map gave the object; below zero where the slot is free. */
    s32     unk_0EC;

    /**
     * Puts the collision frame where the object is and gives it back.
     */
    CFrame *GetCollisionFrame();

    /**
     * Turns a named part of every frame the object holds on or off.
     */
    virtual void FrameObjectOnOff(char *name, int on);

    /**
     * Draws the object.
     */
    virtual void Draw();

    /**
     * Draws the object at the level of detail that suits its distance from
     * the eye, and gives back the level that it drew.
     */
    virtual void DrawLOD(float *distance, int lowest, int highest, int *out_level);

    /**
     * Drops the shadow of the object onto the ground below it.
     */
    void DrawShadow(int fast);

    /**
     * Draws the shade that lies on the object.
     */
    void DrawShade();

    /**
     * Puts the object at rest at the world origin with no frame of its own.
     */
    void Initialize();

    /**
     * Makes an object that a map has yet to place.
     */
    CMapObject();
};

STATIC_ASSERT(sizeof(CMapObject) == 0xF0);

} // namespace dc::game
