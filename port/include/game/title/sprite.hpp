#pragma once

#include "common.h"

class CSprite {
public:
    float x[12];   /**< Horizontal position of each trail point, newest first. */
    float y[12];   /**< Vertical position of each trail point, newest first. */
    float angle;   /**< Heading the head of the trail is turning towards. */
    int   started; /**< Whether the trail has been let go. */

    /**
     * @mangled __ct__7CSpriteFv
     * @address 0x1DD4420
     * @size 0x48
     */
    CSprite();

    /**
     * @mangled Init__7CSpriteFv
     * @address 0x1DD4470
     * @size 0x44
     * @unknownret
     */
    void Init();

    /**
     * @mangled Move__7CSpriteFv
     * @address 0x1DD44C0
     * @size 0x2A0
     * @unknownret
     */
    void Move();

    /**
     * @mangled Draw__7CSpriteFv
     * @address 0x1DD4760
     * @size 0x544
     * @unknownret
     */
    void Draw();

    /**
     * @mangled Se__7CSpriteFv
     * @address 0x1DD4CB0
     * @size 0xC
     * @unknownret
     */
    int Se();
};
