#include "quests/quests.hpp"

const Quest::NpcMatch &NoruneMayor() {
    static const Quest::NpcMatch mayor{0, 13, {}};
    return mayor;
}

// In the order their choices come in an NPC's menu, below "Talk".
void RegisterAllQuests() {
    Quest::Register(MayorSupplies());
    Quest::Register(CarlsSlingshot());
}
