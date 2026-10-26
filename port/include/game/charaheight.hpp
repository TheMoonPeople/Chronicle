#pragma once

#include "common.h"

#include "userstatus.hpp"

/**
 * @file
 * Defines the height of the character the player controls, where hits on that character are placed.
 */

/**
 * How tall the current character stands.
 */
static inline float CharaHeight(CUserStatus *status) {
    float chara_height[6] = {16.0f, 14.0f, 16.0f, 16.0f, 18.0f, 15.0f};

    return chara_height[status->cur_chara];
}
