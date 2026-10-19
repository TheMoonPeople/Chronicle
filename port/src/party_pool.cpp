#include "party_pool.hpp"

#include <algorithm>

#include "dun/gameloop.hpp"
#include "platform/config.hpp"
#include "shot_freefuncs.hpp"
#include "userstatus.hpp"

/**
 * Updates the active character's healing and pool effects.
 */
void PortOriginalHealingWater();

/** Ticks until the next pool healing pulse. */
extern int healingSpeed;

void RestorePoolCompanions(CUserStatus &status) {
    if (!ConfigGet().party_pool_healing) {
        return;
    }
    for (int member = 0; member < std::clamp<int>(status.party_size, 0, 6); ++member) {
        if (member == status.cur_chara || status.hp[member] <= 0) {
            continue;
        }
        status.AddNowLife(member, status.max_hp[member], 0.0f);
        status.AddDrink(member, static_cast<s16>(status.water_max[member]), 0.0f);
    }
}

/**
 * Extends a water-pool healing pulse to living companions when enabled.
 */
void HealingWater() {
    const bool party_pulse = ConfigGet().party_pool_healing && healingSpeed <= 0 && CheckHealingWater();
    PortOriginalHealingWater();
    if (party_pulse) {
        RestorePoolCompanions(*UserStatus);
    }
}
