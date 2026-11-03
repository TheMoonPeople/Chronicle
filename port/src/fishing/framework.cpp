#include "fishing/framework.hpp"

#include <algorithm>
#include <format>
#include <stdexcept>
#include <utility>

namespace dc::fishing {

namespace {

template <class Stage>
std::map<std::string, StageFactory<Stage>, std::less<>> &Stages() {
    static std::map<std::string, StageFactory<Stage>, std::less<>> stages;
    return stages;
}

template <class Stage>
std::unique_ptr<Stage> Make(const nlohmann::json &config, std::string_view kind) {
    if (!config.is_object() || !config.contains("stage") || !config["stage"].is_string()) {
        throw std::runtime_error(std::format("{}: a stage needs its name in \"stage\"", kind));
    }
    const std::string name = config["stage"].get<std::string>();
    const auto        found = Stages<Stage>().find(name);
    if (found == Stages<Stage>().end()) {
        throw std::runtime_error(std::format("no {} stage is called \"{}\"", kind, name));
    }
    return found->second(config);
}

std::map<int, Species> &ByKind() {
    static std::map<int, Species> species;
    return species;
}

std::map<int, Place> &BySpot() {
    static std::map<int, Place> places;
    return places;
}

} // namespace

RodEffect EffectOf(const item::Rod &rod) {
    const float balance = 0.8f + 0.2f * rod.balance / 100.0f;
    return {.size = 0.8f + 0.4f * rod.heft * balance / 100.0f,
            .control = 3.0f * rod.control * balance / 100.0f,
            .pull_divisor = 1.0f + 2.0f * std::max(rod.spring * balance - 10.0f, 0.0f) / 90.0f,
            .lure = std::clamp(rod.lure / 100.0f, 0.0f, 1.0f)};
}

bool Species::IsCustom() const {
    return kind >= kCustomKindFirst;
}

float Species::Affinity(std::string_view bait, float fallback) const {
    const auto found = food_affinity.find(std::string(bait));
    return found != food_affinity.end() ? found->second : fallback;
}

void RegisterSpawnStage(std::string_view name, StageFactory<SpawnStage> factory) {
    Stages<SpawnStage>().insert_or_assign(std::string(name), std::move(factory));
}

void RegisterBiteStage(std::string_view name, StageFactory<BiteStage> factory) {
    Stages<BiteStage>().insert_or_assign(std::string(name), std::move(factory));
}

void RegisterFightStage(std::string_view name, StageFactory<FightStage> factory) {
    Stages<FightStage>().insert_or_assign(std::string(name), std::move(factory));
}

void RegisterOutcomeStage(std::string_view name, StageFactory<OutcomeStage> factory) {
    Stages<OutcomeStage>().insert_or_assign(std::string(name), std::move(factory));
}

std::unique_ptr<SpawnStage> MakeSpawnStage(const nlohmann::json &config) {
    return Make<SpawnStage>(config, "spawn");
}

std::unique_ptr<BiteStage> MakeBiteStage(const nlohmann::json &config) {
    return Make<BiteStage>(config, "bite");
}

std::unique_ptr<FightStage> MakeFightStage(const nlohmann::json &config) {
    return Make<FightStage>(config, "fight");
}

std::unique_ptr<OutcomeStage> MakeOutcomeStage(const nlohmann::json &config) {
    return Make<OutcomeStage>(config, "outcome");
}

void Clear() {
    ByKind().clear();
    BySpot().clear();
}

void RegisterSpecies(Species species) {
    const int kind = species.kind;
    ByKind().insert_or_assign(kind, std::move(species));
}

void RegisterPlace(Place place) {
    const int spot = place.spot;
    BySpot().insert_or_assign(spot, std::move(place));
}

const Species *FindSpecies(std::string_view id) {
    const auto found = std::ranges::find(ByKind(), id, [](const auto &entry) { return std::string_view(entry.second.id); });
    return found != ByKind().end() ? &found->second : nullptr;
}

const Species *SpeciesOfKind(int kind) {
    const auto found = ByKind().find(kind);
    return found != ByKind().end() ? &found->second : nullptr;
}

const Place *PlaceAtSpot(int spot) {
    const auto found = BySpot().find(spot);
    return found != BySpot().end() ? &found->second : nullptr;
}

std::vector<const Species *> AllSpecies() {
    std::vector<const Species *> all;
    for (const auto &[kind, species] : ByKind()) {
        all.push_back(&species);
    }
    return all;
}

} // namespace dc::fishing
