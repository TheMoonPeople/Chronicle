#include <gtest/gtest.h>

#include <algorithm>
#include <cstdlib>

#include "editloop3.hpp"
#include "fish.hpp"
#include "fishing/host.hpp"
#include "item/host.hpp"

namespace dc::test {

using namespace dc::game;

TEST(FishHook, AFishHoldsTheHookHalfASecondToASecond) {
    item::Load();
    fishing::Load();
    fishing::SpawnAt(1, ED_TIME_DAY); // Matataki, whose bite holds the hook 30 to 60 ticks
    int shortest = 1000;
    int longest = 0;
    // One seed, the numbers running on from fish to fish as the game's do: Windows' rand() gives nearly the
    // same first numbers for neighbouring seeds, so a seed for each fish would draw near enough the same fish.
    std::srand(1);
    for (int fish_no = 0; fish_no < 1000; fish_no++) {
        CFish fish;
        fish.fish_kind = 9;
        fish.info = *fishing::Info(9);
        float food[4] = {0.0f, 0.0f, 0.0f, 0.0f};
        fish.SetFoodPos(8, food, 50.0f); // the Carrot, which kind 9 likes most
        fish.action = FISH_ACTION_EAT_FOOD;
        fish.action_timer = -1;
        // Step until the fish takes the hook, then count the ticks it holds it.
        int tick = 0;
        while (fish.action == FISH_ACTION_EAT_FOOD && tick++ < 10000) {
            fish.action_timer = std::min(fish.action_timer, 0) - 1;
            fish.Step();
        }
        if (fish.action != FISH_ACTION_BITE_HOOK) {
            continue;
        }
        int held = 0;
        while (fish.action == FISH_ACTION_BITE_HOOK) {
            fish.Step();
            held++;
        }
        shortest = std::min(shortest, held);
        longest = std::max(longest, held);
    }
    EXPECT_EQ(shortest, 30);
    EXPECT_EQ(longest, 60);
}

} // namespace dc::test
