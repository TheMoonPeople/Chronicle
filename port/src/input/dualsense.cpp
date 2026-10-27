#include "input/dualsense.hpp"

#include <algorithm>
#include <iterator>
#include <utility>

#include "dun/gameloop.hpp"
#include "main.hpp"
#include "platform/input.hpp"
#include "userstatus.hpp"

namespace dc::input {

using namespace dc::game;

LightbarColour LightbarForLife(int life, int max_life) {
    if (max_life <= 0) {
        return LightbarIdle();
    }
    float fraction = std::clamp(static_cast<float>(life) / static_cast<float>(max_life), 0.0f, 1.0f);
    // Red to yellow over the lower half, yellow to green over the upper.
    float red = fraction < 0.5f ? 255.0f : 255.0f * (1.0f - fraction) * 2.0f;
    float green = fraction < 0.5f ? 255.0f * fraction * 2.0f : 255.0f;
    return {.red = static_cast<std::uint8_t>(red), .green = static_cast<std::uint8_t>(green), .blue = 0};
}

LightbarColour LightbarIdle() { return {.red = 0, .green = 48, .blue = 160}; }

void DualSenseUpdate() {
    LightbarColour colour = LightbarIdle();
    if (mode == GAME_MODE_DUNGEON && UserStatus != nullptr) {
        int chara = UserStatus->cur_chara;
        if (chara >= 0 && std::cmp_less(chara, std::size(UserStatus->hp))) {
            colour = LightbarForLife(UserStatus->hp[chara], UserStatus->max_hp[chara]);
        }
    }
    InputSetLightbar(0, colour.red, colour.green, colour.blue);
}

} // namespace dc::input
