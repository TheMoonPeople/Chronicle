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

std::vector<std::pair<std::string, std::string>> QuestStrings() {
    std::vector<std::pair<std::string, std::string>> out;
    for (std::span<const Quest::Text> texts : {MayorSuppliesTexts(), CarlsSlingshotTexts()}) {
        for (const Quest::Text &text : texts) {
            out.emplace_back(text.key, text.english);
        }
    }
    return out;
}
