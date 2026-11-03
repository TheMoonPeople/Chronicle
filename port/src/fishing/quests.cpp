#include "fishing/quests.hpp"

#include <algorithm>
#include <utility>

#include "fishing/host.hpp"
#include "quest/framework.hpp"
#include "savedata.hpp"

namespace dc::fishing {

bool CatchFilter::Matches(const Catch &fish) const {
    return (species.empty() || (fish.species != nullptr && fish.species->id == species)) &&
           (place.empty() || (fish.place != nullptr && fish.place->id == place)) && (bait.empty() || fish.bait == bait) &&
           fish.size >= min_size;
}

namespace {

bool AtStage(std::string_view quest, const Stages &while_at) {
    if (while_at.empty()) {
        return true;
    }
    const quest::Progress *progress = quest::FindProgress(quest);
    return progress != nullptr && std::ranges::contains(while_at, progress->stage);
}

} // namespace

void CountCatches(std::string quest, std::string var, CatchFilter filter, Stages while_at) {
    OnCatch([quest = std::move(quest), var = std::move(var), filter = std::move(filter), while_at = std::move(while_at)](const Catch &fish) {
        if (AtStage(quest, while_at) && filter.Matches(fish)) {
            quest::ChangeVar(quest, var, [](int count) { return count + 1; });
        }
    });
}

void CountFishPoints(std::string quest, std::string var) {
    OnCatch([quest = std::move(quest), var = std::move(var)](const Catch &fish) {
        quest::ChangeVar(quest, var, [&fish](int points) { return points + fish.points; });
    });
}

void KeepLargest(std::string quest, std::string var, CatchFilter filter) {
    OnCatch([quest = std::move(quest), var = std::move(var), filter = std::move(filter)](const Catch &fish) {
        if (filter.Matches(fish)) {
            quest::ChangeVar(quest, var, [&fish](int largest) { return std::max(largest, fish.size); });
        }
    });
}

void CountSpecies(std::string quest, std::string var, std::vector<std::string> species, Stages while_at) {
    OnCatch([quest = std::move(quest), var = std::move(var), species = std::move(species), while_at = std::move(while_at)](const Catch &fish) {
        if (!AtStage(quest, while_at) || fish.species == nullptr || std::ranges::find(species, fish.species->id) == species.end()) {
            return;
        }
        bool first = false;
        quest::ChangeVar(quest, var + "." + fish.species->id, [&first](int caught) {
            first = caught == 0;
            return 1;
        });
        if (first) {
            quest::ChangeVar(quest, var, [](int count) { return count + 1; });
        }
    });
}

bool HasCaughtAFish() {
    if (game::SaveData == nullptr) {
        return false;
    }
    for (int rank = 0; rank < 64; rank++) {
        if (const game::SV_FISH_DATA *fish = game::SaveData->GetFishingRank(rank); fish != nullptr && fish->rank > 0.0f) {
            return true;
        }
    }
    return game::SaveData->GetFishingPoint() > 0 || !AllRecords().empty();
}

} // namespace dc::fishing
