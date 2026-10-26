#pragma once

#include "common.h"

/**
 * Draws the short muzzle-hit flashes produced by the machine gun.
 */
class CHIT_MACHINGUN_EFFECT {
public:
    float position[16][4]; /**< World positions of the active flashes. */
    s32   timer[16];       /**< Remaining frame counts of the flashes. */

    /**
     * Starts one flash at the specified world position.
     *
     * @mangled Set__21CHIT_MACHINGUN_EFFECTFPf
     * @address 0x1AE980
     * @size 0x98
     */
    void Set(float *position);

    /**
     * Draws each active machine-gun hit flash.
     *
     * @mangled Draw__21CHIT_MACHINGUN_EFFECTFv
     * @address 0x1AEA20
     * @size 0xB0
     */
    void Draw();

    /**
     * Advances the lifetime of each active flash.
     *
     * @mangled Step__21CHIT_MACHINGUN_EFFECTFv
     * @address 0x1AEAD0
     * @size 0x48
     */
    void Step();
};

STATIC_ASSERT(sizeof(CHIT_MACHINGUN_EFFECT) == 0x140);
