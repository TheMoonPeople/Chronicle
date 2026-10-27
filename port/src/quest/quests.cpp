#include "quest/quests.hpp"

const Quest::NpcMatch &NoruneMayor() {
    static const Quest::NpcMatch mayor{.map_no = 0, .npc_id = 13, .mes_file_key = {}};
    return mayor;
}

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
