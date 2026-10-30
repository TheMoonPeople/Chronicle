#pragma once

#include <nlohmann/json.hpp>

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "item/framework.hpp"

/**
 * The fishing framework (docs/FISHING_FRAMEWORK.md): the species and places fishing knows, and the
 * stages that decide what happens at each, chosen by name in the places' documents. No game headers;
 * fishing/host.cpp fills retail's tables from it.
 */
namespace dc::fishing {

/**
 * A kind of fish.
 */
struct Species {
    std::string                  id;   /**< vanilla:fish_<kind> for retail's. */
    int                          kind; /**< The number CFish and the save's records know it by. */
    std::string                  model;
    int                          name_msg = 0; /**< The system message naming it. */
    float                        model_size = 1.0f; /**< The size the angling model is drawn at unscaled. */
    float                        min_size = 1.0f;   /**< The usual size, the centre of the roll. */
    float                        max_size = 2.0f;
    int                          min_fp = 0; /**< Fish points at min_size. */
    int                          max_fp = 0;
    std::map<std::string, float> food_affinity; /**< Liking by bait item ID; a bait left out is not liked. */
    bool                         mardan_garayan = false; /**< Counted toward the Mardan Garayan. */
    std::map<std::string, std::string> names; /**< A custom species' name by language file; retail's are name_msg. */
    std::string                        item;  /**< The item a kept fish becomes (the keep outcome); empty for none. */
    float                              pull = 0.5f; /**< How hard it pulls in a fight that reads it (tension), at its usual size: below 0.5, Cross can be held through the fight. */

    [[nodiscard]] bool IsCustom() const;

    /**
     * The liking for a bait, or fallback (the bait's default_affinity) for one it doesn't name.
     */
    [[nodiscard]] float Affinity(std::string_view bait, float fallback = 0.0f) const;
};

/**
 * Custom species' kinds start here, in ID order; retail's are 0 to 17, which the save's records keep.
 */
inline constexpr int kCustomKindFirst = 256;

enum class TimeOfDay {
    Day,
    Evening,
    Night,
    Morning,
};

/**
 * What a spawn stage is given besides the place.
 */
struct SpawnContext {
    TimeOfDay               time = TimeOfDay::Day;
    std::function<double()> random; /**< Uniform in [0, 1). */
};

struct Place;

/**
 * Chooses the fish in the water when fishing starts at a place.
 */
class SpawnStage {
public:
    virtual ~SpawnStage() = default;

    /**
     * The species of each fish, place.fish_count of them.
     */
    virtual std::vector<const Species *> Spawn(const Place &place, const SpawnContext &context) const = 0;
};

/**
 * What a rod does in the stages that read it, its Balance applied (Dark Cloud 2's rod numbers): 1 for
 * each with retail's rod.
 */
struct RodEffect {
    float size = 1.0f;         /**< Heft: the hooked fish's size, 0.8 to 1.2 times. */
    float control = 1.0f;      /**< Control: how much the stick held the fish's way tires it. */
    float pull_divisor = 1.0f; /**< Spring: the fish's pull is divided by it, 1 + 2 x (spring - 10) / 90. */
    float lure = 0.0f;         /**< Lure: from 0 to 1, halving at 1 the rolls of nothing and the bait lost to a fish that gets away. */
};

RodEffect EffectOf(const item::Rod &rod);

/**
 * What a fish is doing about the bait, as CFish's action holds it.
 */
enum class FishAction {
    None,     /**< Swimming; may notice the bait. */
    Approach, /**< Swimming to the bait. */
    Eat,      /**< Nibbling: the float bobs. */
    Bite,     /**< On the hook: the float goes under. */
    Leave,    /**< Swimming away. */
};

/**
 * A fish in the water as a bite stage sees it.
 */
struct FishView {
    const Species *species = nullptr;
    FishAction     action = FishAction::None;
    float          distance = 0.0f; /**< From the bait. */
};

/**
 * What a bite stage keeps across one cast, shared by every fish: brisk chooses one for the cast. The
 * game resets it as the float settles, and when the angler turns or reels in.
 */
struct BiteSession {
    bool rolled = false; /**< A fish (or none) has been chosen. */
    int  chosen = -1;    /**< The chosen fish's place in the water, or -1 for none. */
    int  until = 0;      /**< A stage's own deadline, in BiteContext::now's ticks. */
    int   step = 0;       /**< A stage's own step: brisk's chosen fish has started its wait. */
    float speed = 0.0f;   /**< How fast the chosen fish swims to the bait; 0 for retail's. */
};

/**
 * What a bite stage is given besides the fish.
 */
struct BiteContext {
    std::string_view        bait;                    /**< The bait's item ID; empty for a bare hook. */
    float                   radius = 0.0f;           /**< How far fish notice it. */
    float                   default_affinity = 0.0f; /**< The liking of a species that doesn't name it. */
    std::function<double()> random;                  /**< Uniform in [0, 1). */
    int                     fish_index = -1;         /**< The asking fish's place in the water. */
    std::vector<const Species *> fish;               /**< Every fish in the water by place; null where none swims. */
    std::vector<float>           distances;          /**< Each fish's distance from the bait, by place. */
    bool                         waiting = true;     /**< The float is out, waiting for a bite. */
    BiteSession            *session = nullptr;       /**< Null where nothing is shared (a bait loss's roll). */
    int                     now = 0;                 /**< Ticks, counting up through the session. */
    item::Rod               rod;                     /**< The rod in use. */
};

/**
 * A fish's next action and the ticks until it is asked again.
 */
struct ActionStep {
    FishAction action;
    int        ticks;
};

/**
 * Why the bait may be lost, and a fish got away.
 */
enum class LossCause {
    PressedEarly, /**< Cross while the fish only nibbled. */
    LetGo,        /**< The fish let go of the hook before Cross. */
    Snapped,      /**< The line broke in the fight. */
};

/**
 * Decides what each fish does about the bait, and when the bait is lost.
 */
class BiteStage {
public:
    virtual ~BiteStage() = default;

    /**
     * A fish's next action, asked when its last one has run its ticks.
     */
    virtual ActionStep Update(const FishView &fish, const BiteContext &context) const = 0;

    /**
     * Whether the bait comes off for this cause.
     */
    virtual bool LosesBait(LossCause cause, const BiteContext &context) const = 0;

    /**
     * Whether the stage chooses fish for the whole cast (BiteSession), so that a cast starting over
     * sends the fish already at the bait away at once rather than when their actions run out.
     */
    [[nodiscard]] virtual bool ChoosesForTheCast() const { return false; }
};

/**
 * What the angler does in a fight, each tick.
 */
struct FightInput {
    bool  reel = false;    /**< Cross held: winds the line in. */
    bool  tapped = false;  /**< Cross pressed this tick. */
    float stick_x = 0.0f;  /**< The left stick, -1 (left) to 1. */
    float stick_y = 0.0f;  /**< -1 (down) to 1 (up). */
};

/**
 * What a fight is given each tick.
 */
struct FightContext {
    int                                           ticks = 0;   /**< Since the hook was set. */
    FightInput                                    input;
    float                                         distance = 0.0f; /**< The fish from the angler, across the water. */
    std::function<bool()>                         model_ready; /**< Whether the fish's angling model has loaded. */
    std::function<double()>                       random;
    std::function<void(int intensity, int ticks)> rumble;
    std::function<void()>                         warn; /**< The line close to snapping. */
};

enum class FightResult {
    Fighting,
    Landed,
    Snapped, /**< The line broke: the fish is gone. */
};

/**
 * What a fight shows, and how the game moves the fish and the line.
 */
struct FightView {
    bool  steers = false;  /**< The fight moves the fish and the line; otherwise retail's fish swims. */
    float tension = 0.0f;  /**< 0 to 1: at 1 the line snaps. */
    float line = 0.0f;     /**< Line out: the fish is held at most this far from the angler. */
    float heading = 0.0f;  /**< Where the fish swims, from the angler's facing, in radians. */
    int   chance = 0;      /**< The fish veering: -1 left, 1 right, 0 not; the stick held its way pays. */
    float landing = 0.0f;  /**< How close the fish lands. */
    float start = 0.0f;    /**< The line out as the hook was set. */
};

/**
 * One hooked fish's fight, from its stage's Begin.
 */
class Fight {
public:
    virtual ~Fight() = default;

    virtual FightResult Step(const FightContext &context) = 0;
    [[nodiscard]] virtual FightView View() const { return {}; }

    /**
     * Whether the bait goes with a snapped line.
     */
    virtual bool LosesBait(const FightContext &context) const { return false; }
};

/**
 * The fish being fought, as its stage is told it.
 */
struct Hooked {
    const Species *species = nullptr;
    float          size = 0.0f;     /**< In size units, as CFish keeps it. */
    float          distance = 0.0f; /**< From the angler as the hook is set: the line out. */
    item::Rod      rod;             /**< The rod in use. */
};

/**
 * From the hook being set to the fish landed.
 */
class FightStage {
public:
    virtual ~FightStage() = default;

    virtual std::unique_ptr<Fight> Begin(const Hooked &fish) const = 0;

    /**
     * Whether the bait stays on until the fish is landed (a fight that can be lost), rather than
     * being taken as the hook is set (retail's).
     */
    [[nodiscard]] virtual bool BaitStaysOn() const { return false; }

    /**
     * Whether the fight reads the rod: then its Heft grows the hooked fish as the hook is set.
     */
    [[nodiscard]] virtual bool ReadsRods() const { return false; }
};

/**
 * A landed fish.
 */
struct Catch {
    const Species   *species = nullptr;
    int              size = 0; /**< In tenths of a size unit, as the records keep it. */
    int              points = 0;
    const Place     *place = nullptr;
    std::string_view bait; /**< The bait's item ID; empty for a bare hook. */
    TimeOfDay        time = TimeOfDay::Day;
    bool             kept = false; /**< Given as an item by the outcome. */
};

/**
 * A fish that got away.
 */
struct Lost {
    const Species   *species = nullptr; /**< Null when it can't be told which fish it was. */
    const Place     *place = nullptr;
    std::string_view bait;
    LossCause        why = LossCause::LetGo;
    bool             bait_lost = false;
};

/**
 * Follows every landed or lost fish, after its outcome; a listener can't change it. Returns an id
 * for Unsubscribe.
 */
int  OnCatch(std::function<void(const Catch &)> listener);
int  OnLost(std::function<void(const Lost &)> listener);
void Unsubscribe(int id);
void Publish(const Catch &fish);
void Publish(const Lost &fish);

/**
 * What an outcome stage can give the player; the host's.
 */
struct OutcomeContext {
    std::function<void(int points)>              add_points;
    std::function<void(const Catch &)>           record; /**< Into the records: the save's for retail's kinds, chronicle.dat's for custom ones. */
    std::function<void()>                        count_mardan_garayan;
    std::function<void(const Catch &)>           show; /**< The evaluation message. */
    std::function<bool(const Catch &)>           keep; /**< Into the bag as its species' item; false where it has none. */
};

/**
 * What a landed fish gives the player.
 */
class OutcomeStage {
public:
    virtual ~OutcomeStage() = default;

    /**
     * What the catch gives; the stage marks it kept when it gives it as an item.
     */
    virtual void Apply(Catch &fish, const OutcomeContext &context) const = 0;
};

/**
 * Makes a stage from its settings in a place's document; throws std::runtime_error when they are not
 * ones it takes.
 */
template <class Stage>
using StageFactory = std::function<std::unique_ptr<Stage>(const nlohmann::json &settings)>;

/**
 * Where fishing happens: one of the spots the town scripts open.
 */
struct Place {
    std::string                       id;
    int                               spot = 0; /**< The number the town script passes. */
    int                               fish_count = 0;
    bool                              draw_under_water = true; /**< The sea hides its fish. */
    std::shared_ptr<const SpawnStage>   spawn;
    std::shared_ptr<const BiteStage>    bite;
    std::shared_ptr<const FightStage>   fight;
    std::shared_ptr<const OutcomeStage> outcome;
};

/**
 * The most fish one place holds: retail's water has room for six.
 */
inline constexpr int kMaxFish = 6;

void RegisterSpawnStage(std::string_view name, StageFactory<SpawnStage> factory);
void RegisterBiteStage(std::string_view name, StageFactory<BiteStage> factory);
void RegisterFightStage(std::string_view name, StageFactory<FightStage> factory);
void RegisterOutcomeStage(std::string_view name, StageFactory<OutcomeStage> factory);

/**
 * The stage a place names, made from its settings; throws std::runtime_error for a name no stage of
 * the kind has or settings it refuses.
 */
std::unique_ptr<SpawnStage>   MakeSpawnStage(const nlohmann::json &config);
std::unique_ptr<BiteStage>    MakeBiteStage(const nlohmann::json &config);
std::unique_ptr<FightStage>   MakeFightStage(const nlohmann::json &config);
std::unique_ptr<OutcomeStage> MakeOutcomeStage(const nlohmann::json &config);

/**
 * Forgets every species and place.
 */
void Clear();

void RegisterSpecies(Species species);
void RegisterPlace(Place place);

const Species *FindSpecies(std::string_view id);
const Species *SpeciesOfKind(int kind);
const Place   *PlaceAtSpot(int spot);

/**
 * Every species, by kind.
 */
std::vector<const Species *> AllSpecies();

/**
 * Registers the stages the port ships.
 */
void RegisterBuiltinStages();

} // namespace dc::fishing
