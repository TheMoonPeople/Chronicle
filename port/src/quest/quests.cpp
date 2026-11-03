#include "quest/quests.hpp"

#include "quest/documents.hpp"

namespace dc::quest {

const NpcMatch &NoruneMayor() {
    static const NpcMatch mayor{.map_no = 0, .npc_id = 13, .mes_file_key = {}};
    return mayor;
}

void RegisterAllQuests() {
    Register(MayorSupplies());
    RegisterDocuments();
}

std::vector<std::pair<std::string, std::string>> QuestStrings() {
    std::vector<std::pair<std::string, std::string>> out;
    for (const Text &text : MayorSuppliesTexts()) {
        out.emplace_back(text.key, text.english);
    }
    for (auto &entry : DocumentStrings()) {
        out.push_back(std::move(entry));
    }
    return out;
}

} // namespace dc::quest
