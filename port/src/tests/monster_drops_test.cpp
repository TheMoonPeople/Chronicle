#include <gtest/gtest.h>

#include <cstring>

#include "../monstorunit_port.hpp"
#include "monstorunit.hpp"

namespace {

const MONSTOR_MODEL *Species(const char *model) {
    for (const MONSTOR_MODEL &species : MonstorTable) {
        if (std::strcmp(species.model_name[0], model) == 0) {
            return &species;
        }
    }
    return nullptr;
}

} // namespace

TEST(MonsterDrops, RegularMonstersDropButNotTheUnlockable) {
    FixMonsterDrops();
    FixMonsterDrops(); // idempotent
    const char *dropping[] = {"e09a", "e21a", "e22a", "e42a", "e51a", "e58a", "e60a", "e61a",
                              "e90a", "e91a", "e127a", "e136a", "e140a", "e141a", "e158a", "e162a"};
    for (const char *model : dropping) {
        const MONSTOR_MODEL *species = Species(model);
        ASSERT_NE(species, nullptr) << model;
        EXPECT_NE(species->drops_items, 0) << model;
    }
    // Wine Keg and Gacious: no lock-on, left as retail.
    for (const char *model : {"e85a", "e124a"}) {
        const MONSTOR_MODEL *species = Species(model);
        ASSERT_NE(species, nullptr) << model;
        EXPECT_EQ(species->kind, MONSTER_KIND_NO_LOCK_ON) << model;
        EXPECT_EQ(species->drops_items, 0) << model;
    }
    // Every other species keeps its retail flag: only regular monsters change.
    int regular_without = 0;
    for (const MONSTOR_MODEL &species : MonstorTable) {
        if (species.model_name[0][0] == 'e' && species.drops_items == 0) {
            regular_without++;
        }
    }
    EXPECT_EQ(regular_without, 2);
}
