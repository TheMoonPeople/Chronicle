#include <algorithm>
#include <array>
#include <cmath>
#include <format>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "fishing/framework.hpp"

namespace dc::fishing {

namespace {

using Json = nlohmann::json;

struct Weighted {
    const Species *species;
    double         weight;
};

std::vector<Weighted> ParseWeights(const Json &list, std::string_view where) {
    if (!list.is_array() || list.empty()) {
        throw std::runtime_error(std::format("{} needs a list of fish", where));
    }
    std::vector<Weighted> weights;
    for (const Json &entry : list) {
        if (!entry.is_object() || !entry.contains("fish") || !entry["fish"].is_string() || !entry.contains("weight") ||
            !entry["weight"].is_number()) {
            throw std::runtime_error(std::format("{}: each fish needs \"fish\" and \"weight\"", where));
        }
        const std::string id = entry["fish"].get<std::string>();
        const Species    *species = FindSpecies(id);
        if (species == nullptr) {
            throw std::runtime_error(std::format("{}: no species is {}", where, id));
        }
        const double weight = entry["weight"].get<double>();
        if (!std::isfinite(weight) || weight <= 0.0) {
            throw std::runtime_error(std::format("{}: {}'s weight must be above 0", where, id));
        }
        weights.push_back({species, weight});
    }
    return weights;
}

const Species *Draw(const std::vector<Weighted> &weights, double roll) {
    double total = 0.0;
    for (const Weighted &entry : weights) {
        total += entry.weight;
    }
    roll *= total;
    for (const Weighted &entry : weights) {
        if (roll < entry.weight) {
            return entry.species;
        }
        roll -= entry.weight;
    }
    return weights.back().species;
}

/**
 * Retail's spawn: each fish drawn by weight from the place's table, or from its rare table one time in
 * the time of day's interval.
 */
class RetailSpawn final : public SpawnStage {
public:
    explicit RetailSpawn(const Json &settings) {
        fish_ = ParseWeights(settings.contains("fish") ? settings["fish"] : Json(), "spawn");
        if (!settings.contains("rare")) {
            return;
        }
        const Json &rare = settings["rare"];
        if (!rare.is_object() || !rare.contains("interval") || !rare["interval"].is_object()) {
            throw std::runtime_error("spawn: rare needs an interval for each time of day");
        }
        Rare parsed{.fish = ParseWeights(rare.contains("fish") ? rare["fish"] : Json(), "spawn rare")};
        constexpr std::array<const char *, 4> kTimes = {"day", "evening", "night", "morning"};
        for (std::size_t time = 0; time < kTimes.size(); time++) {
            const Json &interval = rare["interval"].contains(kTimes[time]) ? rare["interval"][kTimes[time]] : Json();
            if (!interval.is_number_integer() || interval.get<long long>() < 1 || interval.get<long long>() > 1000000) {
                throw std::runtime_error(std::format("spawn: rare interval {} must be a whole number from 1", kTimes[time]));
            }
            parsed.interval[time] = interval.get<int>();
        }
        rare_ = std::move(parsed);
    }

    std::vector<const Species *> Spawn(const Place &place, const SpawnContext &context) const override {
        std::vector<const Species *> spawned;
        for (int fish = 0; fish < place.fish_count; fish++) {
            const bool rare = rare_ && context.random() * rare_->interval[static_cast<std::size_t>(context.time)] < 1.0;
            spawned.push_back(Draw(rare ? rare_->fish : fish_, context.random()));
        }
        return spawned;
    }

private:
    struct Rare {
        std::array<int, 4>    interval{};
        std::vector<Weighted> fish;
    };

    std::vector<Weighted> fish_;
    std::optional<Rare>   rare_;
};

double Chance(const Json &settings, const char *key, std::string_view where) {
    const Json &value = settings.contains(key) ? settings[key] : Json();
    if (!value.is_number() || value.get<double>() < 0.0 || value.get<double>() > 1.0) {
        throw std::runtime_error(std::format("{}: {} must be a chance from 0 to 1", where, key));
    }
    return value.get<double>();
}

int Ticks(const Json &value, std::string_view what) {
    if (!value.is_number_integer() || value.get<long long>() < 1 || value.get<long long>() > 100000) {
        throw std::runtime_error(std::format("{} must be a whole number of ticks from 1", what));
    }
    return value.get<int>();
}

/**
 * Retail's bite (CFish's FindFood, EatFood, BiteHook, LeaveFood and LeaveHook): a fish within the
 * bait's radius notices it at its liking x 0.3 a check, takes it and then the hook at x 0.5, and
 * loses interest at 0.1, every 21 to 50 ticks. The port's own: it holds the hook for hook_ticks.
 */
class RetailBite final : public BiteStage {
public:
    explicit RetailBite(const Json &settings) {
        const Json &hold = settings.contains("hook_ticks") ? settings["hook_ticks"] : Json();
        if (!hold.is_array() || hold.size() != 2) {
            throw std::runtime_error("bite: hook_ticks needs the fewest and the most");
        }
        hold_first_ = Ticks(hold[0], "bite: hook_ticks");
        hold_last_ = Ticks(hold[1], "bite: hook_ticks");
        if (hold_last_ < hold_first_) {
            throw std::runtime_error("bite: hook_ticks must not end before it starts");
        }
        lose_early_ = Chance(settings, "lose_bait_early", "bite");
        lose_escaped_ = Chance(settings, "lose_bait_escaped", "bite");
    }

    ActionStep Update(const FishView &fish, const BiteContext &context) const override {
        // Retail's timer, 20 to 49, which runs a tick past its value.
        int        timer = 20 + static_cast<int>(context.random() * 30.0);
        const bool bait = !context.bait.empty() && context.radius > 0.0f;
        const auto chance = [&](double probability) { return context.random() < probability; };
        const double liking = bait && fish.species != nullptr ? fish.species->Affinity(context.bait, context.default_affinity) : 0.0;
        const bool   at_bait = bait && fish.distance <= 1.0f;

        switch (fish.action) {
            case FishAction::None:
                if (bait && fish.distance <= context.radius && chance(liking * 0.3)) {
                    return {FishAction::Approach, timer + 1};
                }
                return {FishAction::None, timer + 1};
            case FishAction::Approach: {
                FishAction next = FishAction::Approach;
                if (at_bait && chance(liking * 0.5)) {
                    next = FishAction::Eat;
                }
                if (!bait || chance(0.1)) {
                    next = FishAction::Leave;
                    timer >>= 1;
                }
                return {next, timer + 1};
            }
            case FishAction::Eat: {
                FishAction next = FishAction::Eat;
                if (at_bait && chance(liking * 0.5)) {
                    next = FishAction::Bite;
                }
                if (!bait || chance(0.1)) {
                    next = FishAction::Leave;
                }
                if (next == FishAction::Bite) {
                    return {next, hold_first_ + static_cast<int>(context.random() * (hold_last_ - hold_first_ + 1))};
                }
                return {next, timer + 1};
            }
            case FishAction::Bite:
                return {FishAction::Leave, timer * 2 + 1};
            case FishAction::Leave:
                return {FishAction::None, timer + 1};
        }
        return {FishAction::None, timer + 1};
    }

    bool LosesBait(LossCause cause, const BiteContext &context) const override {
        return context.random() < (cause == LossCause::PressedEarly ? lose_early_ : lose_escaped_);
    }

private:
    int    hold_first_ = 0;
    int    hold_last_ = 0;
    double lose_early_ = 0.0;
    double lose_escaped_ = 0.0;
};

/**
 * Retail's fight, which isn't one: the controller rumbles for ticks while the fish's model loads,
 * then it is landed.
 */
class RetailFight final : public FightStage {
public:
    explicit RetailFight(const Json &settings) {
        ticks_ = Ticks(settings.contains("ticks") ? settings["ticks"] : Json(), "fight: ticks");
    }

    std::unique_ptr<Fight> Begin(const Hooked & /*fish*/) const override { return std::make_unique<Run>(ticks_); }

private:
    class Run final : public Fight {
    public:
        explicit Run(int ticks) : ticks_(ticks) {}

        FightResult Step(const FightContext &context) override {
            const bool landed = context.ticks > ticks_ && context.model_ready();
            context.rumble(80 + static_cast<int>(context.random() * 40.0), 10);
            return landed ? FightResult::Landed : FightResult::Fighting;
        }

    private:
        int ticks_;
    };

    int ticks_ = 0;
};

double Number(const Json &settings, const char *key, std::string_view where, double low, double high) {
    const Json &value = settings.contains(key) ? settings[key] : Json();
    if (!value.is_number() || value.get<double>() < low || value.get<double>() > high) {
        throw std::runtime_error(std::format("{}: {} must be a number from {} to {}", where, key, low, high));
    }
    return value.get<double>();
}

// Dark Cloud 2 runs at 30 ticks a second and the port at 60: its rates are halved and its durations
// doubled, so that its fight moves at its own pace.
constexpr float Dc2Rate(float per_dc2_tick) {
    return per_dc2_tick * 0.5f;
}

constexpr int Dc2Ticks(int dc2_ticks) {
    return dc2_ticks * 2;
}

/**
 * Dark Cloud 2's fight (BattleLoop, LineTensionStep), in its ticks of a 30th of a second, with the
 * port's answer to a veer: the fish pulls and the angler winds it in without letting the line's tension
 * reach the top. Holding Cross reels, raising tension 0.03 x pull a tick (26.5% more, the port's); more
 * than 5 presses and pushes in quick succession make tension climb, more than 20 make it jump. Every 60
 * to 139 ticks the fish veers to one side for veer_ticks: the stick held that way takes most of what
 * reeling adds and tires the fish; held the other way, or at all once the veer and a moment after it are
 * over, the rod is out of position and tension climbs sharply. Left alone, tension falls by ease a tick
 * (the port's), and can't fall below a floor rising 0.0001 a tick. The fish is landed once reeled within
 * land_distance after min_ticks; the line snaps at 1.
 *
 * Dark Cloud 2's tension rules have no units and come across as they are; its distances (casts of 160
 * to 400 landing at 90) are the settings, in Dark Cloud 1's.
 */
class TensionFight final : public FightStage {
public:
    explicit TensionFight(const Json &settings)
        : settings_{.land = static_cast<float>(Number(settings, "land_distance", "fight", 0.5, 1000.0)),
                    .min_ticks = Ticks(settings.contains("min_ticks") ? settings["min_ticks"] : Json(), "fight: min_ticks"),
                    .reel = static_cast<float>(Number(settings, "reel_speed", "fight", 0.001, 100.0)),
                    .lose_snapped = Chance(settings, "lose_bait_snapped", "fight"),
                    .veer_ticks = settings.contains("veer_ticks") ? Ticks(settings["veer_ticks"], "fight: veer_ticks") : kVeerTicks,
                    .ease = settings.contains("ease") ? static_cast<float>(Number(settings, "ease", "fight", 0.0, 1.0)) : kEase} {}

    std::unique_ptr<Fight> Begin(const Hooked &fish) const override { return std::make_unique<Run>(settings_, fish); }

    [[nodiscard]] bool BaitStaysOn() const override { return true; }
    [[nodiscard]] bool ReadsRods() const override { return true; }

private:
    // Its veer of 30 ticks, a second; and the port's easing, 2.5 times its 0.004, which made a fight
    // mostly waiting for the line to slacken.
    static constexpr int   kVeerTicks = Dc2Ticks(30);
    static constexpr float kEase = Dc2Rate(0.01f);

    struct Settings {
        float  land;
        int    min_ticks;
        float  reel;
        double lose_snapped;
        int    veer_ticks;
        float  ease; /**< How much tension falls a tick, left alone. */
    };

    class Run final : public Fight {
    public:
        // A copy of the stage's settings: a fight outlives no stage in the game, but needn't rely on it.
        Run(const Settings &stage, const Hooked &fish) : stage_(stage) {
            // Dark Cloud 2 scales its pull_rate by the size in 80ths of a metre; Dark Cloud 1's sizes
            // have no such scale, so a fish of its kind's usual size pulls its species' pull.
            const Species *species = fish.species;
            const float    usual = species != nullptr && species->min_size > 0.0f ? species->min_size : 1.0f;
            const RodEffect rod = EffectOf(fish.rod);
            pull_ = (species != nullptr ? species->pull : 1.0f) * std::max(fish.size, 0.0f) / usual / rod.pull_divisor;
            control_ = rod.control;
            lure_ = rod.lure;
            line_ = std::max(fish.distance, stage.land + 1.0f);
            start_ = line_;
        }

        FightResult Step(const FightContext &context) override {
            const FightInput &input = context.input;
            const int         stick = input.stick_x > 0.8f ? 1 : (input.stick_x < -0.8f ? -1 : 0);
            const bool        pushed = stick != 0 && stick != stick_;
            stick_ = stick;

            if (input.reel) {
                line_ = std::max(line_ - (Aligned(stick) ? stage_.reel * kAlignedReel : stage_.reel), 0.0f);
            }
            if (pushed || input.tapped) {
                actions_++;
                action_hold_ = Dc2Ticks(10);
            }
            if (--action_hold_ <= 0) {
                actions_ = std::max(actions_ - 1, 0);
            }
            Swim(context);
            Tension(context, input.reel, stick);

            if (tension_ >= 1.0f) {
                return FightResult::Snapped;
            }
            const bool close = std::min(context.distance, line_) < stage_.land;
            return close && context.ticks >= stage_.min_ticks && context.model_ready() ? FightResult::Landed : FightResult::Fighting;
        }

        [[nodiscard]] FightView View() const override {
            return {.steers = true, .tension = tension_, .line = line_, .heading = heading_, .chance = chance_ > 0 ? direction_ : 0, .landing = stage_.land, .start = start_};
        }

        bool LosesBait(const FightContext &context) const override { return context.random() < stage_.lose_snapped * (1.0 - 0.5 * lure_); }

    private:
        // FishBattle: the fish swims outward within 72 degrees of the angler's facing, turning a tenth
        // of the way to its heading a tick, and veers to one side in an action chance.
        void Swim(const FightContext &context) {
            float target = static_cast<float>((context.random() - 0.5) * 2.5132742);
            if (chance_ <= 0) {
                chance_ = 0;
                if (--next_chance_ <= 0) {
                    chance_ = stage_.veer_ticks;
                    direction_ = context.random() < 0.5 ? 1 : -1;
                }
            }
            if (chance_ > 0) {
                const float side = static_cast<float>(context.random() * 1.2566371);
                target = direction_ > 0 ? -side : side;
                if (--chance_ <= 0) {
                    chance_ = 0;
                    grace_ = kGrace + 1; // counted down from the next tick, the first without the arrow
                    next_chance_ = Dc2Ticks(60 + static_cast<int>(context.random() * 80.0));
                }
            } else if (grace_ > 0) {
                grace_--;
            }
            heading_ += 0.1f * (target - heading_);
        }

        // The stick held the way the fish veers, while the arrow shows or in the grace after it.
        [[nodiscard]] bool Aligned(int stick) const { return (chance_ > 0 || grace_ > 0) && stick == direction_; }

        // LineTensionStep, with the port's answer to a veer: the stick held the fish's way while the arrow
        // shows (and for kGrace ticks after, for a person's reaction) takes most of what reeling adds and
        // tires the fish; held any other way, or once the veer is over, the rod is out of position and
        // tension climbs sharply, reeling or not.
        void Tension(const FightContext &context, bool winding, int stick) {
            const float pull = pull_ * (1.0f + 0.5f * vigour_);
            float       rate = -stage_.ease;
            if (actions_ > 5) {
                rate = -stage_.ease + Dc2Rate(0.01f) * static_cast<float>(actions_ - 5) / 15.0f;
            }
            if (actions_ > 20) {
                rate = Dc2Rate(0.2f);
            }
            if (rate > 0.0f) {
                rate *= pull;
            }
            const bool aligned = Aligned(stick);
            const bool out_of_position = stick != 0 && !aligned;
            const float reeling = Dc2Rate(0.03f) * pull * kBuild;
            int         strength = 0;
            if (winding) {
                rate += aligned ? reeling * kAligned : reeling;
                strength = 150;
            }
            if (aligned) {
                vigour_ -= Dc2Rate(0.02f) * control_;
                strength = std::max(strength, 100);
            } else {
                vigour_ += kRecovery;
            }
            if (out_of_position) {
                rate += reeling * kOutOfPosition;
                strength = 220;
            }
            vigour_ = std::clamp(vigour_, -1.0f, 1.0f);
            floor_ = std::min(floor_ + Dc2Rate(0.0001f), 1.0f);
            tension_ = std::clamp(tension_ + rate, floor_, 1.0f);
            if (strength > 0) {
                context.rumble(strength, Dc2Ticks(4));
            }
            // A warning above 0.7, now and then.
            if (tension_ > 0.7f && warned_ == 0) {
                context.warn();
                warned_ = Dc2Ticks(20);
            }
            warned_ = std::max(warned_ - 1, 0);
        }

        static constexpr float kRecovery = Dc2Rate(0.01f);

        // The port's, from the user's play: tension builds 26.5% faster than Dark Cloud 2's for reeling
        // (15%, then 10% more); the stick held the fish's way leaves a quarter of it and winds 5% faster,
        // held out of position adds half again more.
        static constexpr float kBuild = 1.15f * 1.1f;
        static constexpr float kAligned = 0.25f;
        static constexpr float kAlignedReel = 1.05f;
        static constexpr float kOutOfPosition = 1.5f;
        static constexpr int   kGrace = 10;

        Settings            stage_;
        float               pull_ = 1.0f;
        float               control_ = 1.0f; /**< The rod's Control. */
        float               lure_ = 0.0f;    /**< The rod's Lure. */
        float               line_ = 0.0f;
        float               start_ = 0.0f;
        float               tension_ = 0.0f;
        float               floor_ = 0.0f;
        float               vigour_ = 0.0f;
        float               heading_ = 0.0f;
        int                 actions_ = 0;
        int                 action_hold_ = 0;
        int                 chance_ = 0;
        int                 next_chance_ = Dc2Ticks(60);
        int                 direction_ = 1;
        int                 warned_ = 0;
        int                 grace_ = 0;
        int                 stick_ = 0;
    };

    Settings settings_;
};

/**
 * Retail's evaluation: the Mardan Garayan count, the points, the records, the message.
 */
class RetailOutcome final : public OutcomeStage {
public:
    void Apply(Catch &fish, const OutcomeContext &context) const override {
        if (fish.species != nullptr && fish.species->mardan_garayan) {
            context.count_mardan_garayan();
        }
        context.add_points(fish.points);
        context.record(fish);
        context.show(fish);
    }
};

// A whole number of ticks from the first to the last of a two-number range.
struct Span {
    int first = 0;
    int last = 0;

    int Draw(const std::function<double()> &random) const { return first + static_cast<int>(random() * (last - first + 1)); }
};

Span ParseSpan(const Json &value, std::string_view what) {
    if (!value.is_array() || value.size() != 2) {
        throw std::runtime_error(std::format("{} needs the fewest ticks and the most", what));
    }
    const Span span{Ticks(value[0], what), Ticks(value[1], what)};
    if (span.last < span.first) {
        throw std::runtime_error(std::format("{} must not end before it starts", what));
    }
    return span;
}

/**
 * Dark Cloud 2's liking levels (F:3815), which weigh a fish's chance to come 0, 0.5, 1 and 1.5; Dark
 * Cloud 1's likings are 0, 0.2, 0.5 and 1.
 */
enum class Level {
    None,
    Low,
    Normal,
    High,
};

Level LevelOf(double liking) {
    if (liking <= 0.0) {
        return Level::None;
    }
    if (liking < 0.35) {
        return Level::Low;
    }
    return liking < 0.75 ? Level::Normal : Level::High;
}

/**
 * Dark Cloud 2's bite (GetUkiWaitTime, F:3783): as the float settles one roll chooses a fish in the
 * water by its liking for the bait, or nothing (then another roll after retry ticks). The chosen fish
 * swims over in at least wait ticks, nibbles for its liking's nibble, then holds the hook for its window;
 * missed, it leaves and the roll starts again. The other fish leave the bait alone.
 */
class BriskBite final : public BiteStage {
public:
    static constexpr int   kLookAgain = 30;        // ticks, while no fish in the water likes the bait
    static constexpr float kRetailSpeed = 0.1f;    // CFish's approach
    static constexpr float kFastestSpeed = 0.6f;   // about retail's quickest swimming

    explicit BriskBite(const Json &settings) {
        const auto at = [&](const char *key) { return settings.contains(key) ? settings[key] : Json(); };
        wait_ = ParseSpan(at("wait"), "bite: wait");
        retry_ = Ticks(at("retry"), "bite: retry");
        const Json nothing = at("nothing");
        if (!nothing.is_number() || nothing.get<double>() < 0.0) {
            throw std::runtime_error("bite: nothing must be a weight from 0");
        }
        nothing_ = nothing.get<double>();
        const Json nibble = at("nibble");
        if (!nibble.is_object()) {
            throw std::runtime_error("bite: nibble needs low, normal and high");
        }
        nibble_low_ = ParseSpan(nibble.contains("low") ? nibble["low"] : Json(), "bite: nibble low");
        nibble_normal_ = ParseSpan(nibble.contains("normal") ? nibble["normal"] : Json(), "bite: nibble normal");
        nibble_high_ = ParseSpan(nibble.contains("high") ? nibble["high"] : Json(), "bite: nibble high");
        window_ = ParseSpan(at("window"), "bite: window");
        lose_early_ = Chance(settings, "lose_bait_early", "bite");
        lose_escaped_ = settings.contains("lose_bait_escaped") ? Chance(settings, "lose_bait_escaped", "bite") : 0.0;
    }

    ActionStep Update(const FishView &fish, const BiteContext &context) const override {
        BiteSession *session = context.session;
        const bool   bait = !context.bait.empty() && context.radius > 0.0f;
        const auto   away = [&](int ticks) {
            return ActionStep{fish.action == FishAction::None || fish.action == FishAction::Leave ? FishAction::None : FishAction::Leave, ticks};
        };
        if (session == nullptr || !bait || !context.waiting) {
            return away(30);
        }
        if (!session->rolled) {
            if (context.now < session->until) {
                return away(1);
            }
            Roll(*session, context);
        }
        if (context.fish_index != session->chosen) {
            return away(1);
        }
        // A fish the roll has just chosen starts its own wait, whatever it was doing, and swims fast
        // enough to reach the bait in it: Dark Cloud 2's fish are on the bait when the wait ends.
        if (session->step == 0 || fish.action == FishAction::None || fish.action == FishAction::Leave) {
            const int wait = wait_.Draw(context.random);
            session->step = 1;
            session->until = context.now + wait * 2;
            session->speed = std::clamp(fish.distance / static_cast<float>(wait), kRetailSpeed, kFastestSpeed);
            return {FishAction::Approach, wait};
        }
        switch (fish.action) {
            case FishAction::Approach:
                // Dark Cloud 1's fish can be seen: it nibbles once it is at the bait. One the water
                // keeps from it for twice its wait swims off, and another roll comes.
                if (fish.distance > 2.0f) {
                    if (context.now < session->until) {
                        return {FishAction::Approach, 10};
                    }
                    *session = {};
                    return {FishAction::Leave, 60};
                }
                return {FishAction::Eat, NibbleFor(fish, context).Draw(context.random)};
            case FishAction::Eat:
                return {FishAction::Bite, window_.Draw(context.random)};
            case FishAction::Bite:
                // Missed: it swims off, and the next roll comes as it goes.
                *session = {};
                return {FishAction::Leave, 60};
            default:
                break;
        }
        return away(1);
    }

    // A rod's Lure halves, at its most, the bait lost to a fish that gets away (Dark Cloud 2, F:3285).
    bool LosesBait(LossCause cause, const BiteContext &context) const override {
        if (cause == LossCause::PressedEarly) {
            return context.random() < lose_early_;
        }
        return context.random() < lose_escaped_ * (1.0 - 0.5 * EffectOf(context.rod).lure);
    }

    [[nodiscard]] bool ChoosesForTheCast() const override { return true; }

private:
    static double Weight(Level level) {
        switch (level) {
            case Level::Low:
                return 0.5;
            case Level::Normal:
                return 1.0;
            case Level::High:
                return 1.5;
            case Level::None:
                break;
        }
        return 0.0;
    }

    static double Liking(const Species *species, const BiteContext &context) {
        return species != nullptr ? species->Affinity(context.bait, context.default_affinity) : 0.0;
    }

    const Span &NibbleFor(const FishView &fish, const BiteContext &context) const {
        switch (LevelOf(Liking(fish.species, context))) {
            case Level::High:
                return nibble_high_;
            case Level::Normal:
                return nibble_normal_;
            default:
                return nibble_low_;
        }
    }

    // Any fish in the water can come, as in Dark Cloud 2: the chosen one swims over in its wait. A bait
    // no fish here likes never brings one; the fish are looked at again now and then.
    void Roll(BiteSession &session, const BiteContext &context) const {
        std::vector<double> weights;
        double              liked = 0.0;
        for (const Species *species : context.fish) {
            weights.push_back(Weight(LevelOf(Liking(species, context))));
            liked += weights.back();
        }
        session = {};
        if (liked <= 0.0) {
            session.until = context.now + kLookAgain;
            return;
        }
        // A rod's Lure halves, at its most, the weight of nothing coming (Dark Cloud 2, F:3852).
        double roll = context.random() * (liked + nothing_ * (1.0 - 0.5 * EffectOf(context.rod).lure));
        for (std::size_t index = 0; index < weights.size(); index++) {
            if (roll < weights[index]) {
                session.chosen = static_cast<int>(index);
                break;
            }
            roll -= weights[index];
        }
        if (session.chosen < 0) {
            session.until = context.now + retry_;
            return;
        }
        session.rolled = true;
    }

    Span   wait_;
    int    retry_ = 0;
    double nothing_ = 0.0;
    Span   nibble_low_;
    Span   nibble_normal_;
    Span   nibble_high_;
    Span   window_;
    double lose_early_ = 0.0;
    double lose_escaped_ = 0.0;
};

bool Flag(const Json &settings, const char *key, std::string_view where) {
    if (!settings.contains(key)) {
        return false;
    }
    if (!settings[key].is_boolean()) {
        throw std::runtime_error(std::format("{}: {} must be true or false", where, key));
    }
    return settings[key].get<bool>();
}

/**
 * The fish into the bag as its species' item, with its size; the points and the records only where
 * the place asks for them.
 */
class KeepOutcome final : public OutcomeStage {
public:
    explicit KeepOutcome(const Json &settings)
        : points_(Flag(settings, "points", "outcome")), records_(Flag(settings, "records", "outcome")) {}

    void Apply(Catch &fish, const OutcomeContext &context) const override {
        fish.kept = context.keep(fish);
        if (records_ && fish.species != nullptr && fish.species->mardan_garayan) {
            context.count_mardan_garayan();
        }
        if (points_) {
            context.add_points(fish.points);
        }
        if (records_) {
            context.record(fish);
        }
        // The window says what was given: no points unless the place gives them.
        Catch shown = fish;
        shown.points = points_ ? fish.points : 0;
        context.show(shown);
    }

private:
    bool points_;
    bool records_;
};

} // namespace

void RegisterBuiltinStages() {
    RegisterSpawnStage("retail", [](const Json &settings) { return std::make_unique<RetailSpawn>(settings); });
    RegisterBiteStage("retail", [](const Json &settings) { return std::make_unique<RetailBite>(settings); });
    RegisterBiteStage("brisk", [](const Json &settings) { return std::make_unique<BriskBite>(settings); });
    RegisterFightStage("retail", [](const Json &settings) { return std::make_unique<RetailFight>(settings); });
    RegisterFightStage("tension", [](const Json &settings) { return std::make_unique<TensionFight>(settings); });
    RegisterOutcomeStage("retail", [](const Json &) { return std::make_unique<RetailOutcome>(); });
    RegisterOutcomeStage("keep", [](const Json &settings) { return std::make_unique<KeepOutcome>(settings); });
}

} // namespace dc::fishing
