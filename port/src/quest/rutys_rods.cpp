// nlohmann/json (through the fishing framework) before the game headers, whose one-letter macros it cannot stand.
#include "fishing/quests.hpp"

#include <array>
#include <string>
#include <vector>

#include "item/framework.hpp"
#include "item/host.hpp"
#include "localize/localize.hpp"
#include "quest/quests.hpp"

// Ruty's rods: Enhanced fishing's quest (roadmap/RUTY_QUEST.md). Ruty keeps Queens' fish store and
// studies fish; she has seen every sea fish that comes through the port but none from inland, and has
// wanted to see a Mardan Garayan since she was a girl. Once her store is open and Toan has caught a
// fish, she asks him to catch every kind in Norune and Matataki, then a Mardan Garayan, then the Baron
// Garayan from her oldest fish book, giving him a better rod for each. The rods are the Enhanced
// fishing pack's, so the quest is there only with it.
//
//   hidden  -- her store open, a fish caught -->  offered  -- she asks -->  inland
//   inland  -- every kind caught, the Sea Rod -->  mardan  -- a Mardan Garayan, the Garayan Rod -->  baron
//   baron  -- a Baron Garayan, the Baron Rod -->  done

namespace dc::quest {

namespace {

constexpr std::string_view kQuest = "rutys_rods";

// Ruty, as the dialog framework numbers Queens' villagers (her lines start at 20 + 1 * 20, "Hey,
// brother. You're not from this town."); her store is the same villager's house.
constexpr int    kQueens = 2;
constexpr int    kRutyVillager = 1;
const NpcMatch   kRuty{.map_no = kQueens, .npc_id = kRutyVillager, .mes_file_key = {}};

// Every kind of Norune's pond and Matataki's river, the Mardan Garayan and the Baron aside.
const std::vector<std::string> kInland = {"vanilla:fish_1", "vanilla:fish_2", "vanilla:fish_4", "vanilla:fish_6",
                                          "vanilla:fish_7", "vanilla:fish_9", "vanilla:fish_10"};

// The towns' names in the system messages.
constexpr int kNoruneName = 10;
constexpr int kMatatakiName = 12;

constexpr std::string_view kSeaRod = "chronicle:sea_rod";
constexpr std::string_view kGarayanRod = "chronicle:garayan_rod";
constexpr std::string_view kBaronRod = "chronicle:baron_rod";

constexpr Text kAsk = {.key = "quest.rutys_rods.ruty.ask", .english = "I've been fishing."};
constexpr Text kAbout = {.key = "quest.rutys_rods.ruty.about", .english = "About the fish"};
constexpr Text kOffer = {.key = "quest.rutys_rods.ruty.offer",
                         .english = "Hey, brother! I hear you've been fishing. Mind if I ask you something?{page}"
                                    "I study fish, see. I've seen every sea fish that comes through this port... "
                                    "but never one from inland.{page}"
                                    "River fish, pond fish... I've only ever read about 'em. You'd tell me what you "
                                    "catch, wouldn't ya?{page}"
                                    "Catch every kind in %1 and %2, and I'll get you a real rod off the next boat in. "
                                    "Deal?"};
constexpr Text kInlandWaiting = {.key = "quest.rutys_rods.ruty.inland_waiting",
                                 .english = "%1 more kinds to go, brother?{page}"
                                            "I've got a page in my notebook for each one. Don't leave me hanging!{page}"
                                            "If one won't bite, try another bait. Fish are picky. Like my customers."};
constexpr Text kSeaRodGiven = {.key = "quest.rutys_rods.ruty.sea_rod",
                               .english = "Every kind in %1 and %2? You're amazing, brother! My notebook's never "
                                          "been so full.{page}"
                                          "Here. A sailor owed me a favor, and this is how he paid me back. It's yours now."};
constexpr Text kMardanAsk = {.key = "quest.rutys_rods.ruty.mardan_ask",
                             .english = "There's one page I still can't fill. The Mardan Garayan.{page}"
                                        "Folks say seeing one brings you happiness. I've wanted to see one since I was "
                                        "a little girl.{page}"
                                        "If you ever catch one, tell me first, okay? Not Suzy. Me."};
constexpr Text kMardanWaiting = {.key = "quest.rutys_rods.ruty.mardan_waiting",
                                 .english = "Any luck with the Mardan Garayan?{page}"
                                            "I hear it won't touch worms. Only fruit. Strange tastes for a fish.{page}"
                                            "Then again, Mr. King won't eat fish at all. Who am I to judge?"};
constexpr Text kGarayanRodGiven = {.key = "quest.rutys_rods.ruty.garayan_rod",
                                   .english = "You... you really caught one!? Tell me everything! What color was it? How "
                                              "big was it?{page}"
                                              "...I feel happy just hearing about it. Guess the stories were true.{page}"
                                              "You've earned better than that rod. I was saving this one for something "
                                              "special. This is it."};
constexpr Text kBaronTale = {.key = "quest.rutys_rods.ruty.baron_tale",
                             .english = "Hey, between you and me...{page}"
                                        "There's a drawing in my oldest fish book. A Garayan bigger than any net. The "
                                        "book calls it the Baron.{page}"
                                        "Nobody's seen one in a hundred years. Most folks say it never existed.{page}"
                                        "But a week ago, I'd have said the same about the Mardan Garayan."};
constexpr Text kBaronWaiting = {.key = "quest.rutys_rods.ruty.baron_waiting",
                                .english = "Still chasing the Baron?{page}"
                                           "If it's half as big as that drawing, hold on tight. I don't wanna be the "
                                           "one fishing YOU out of the water."};
constexpr Text kBaronRodGiven = {.key = "quest.rutys_rods.ruty.baron_rod",
                                 .english = "No way... The Baron Garayan!?{page}"
                                            "A hundred years, and you're the one who found it! My old book was right!{page}"
                                            "Here, brother. It's the best rod I've got. I was gonna sell it, but... "
                                            "nobody deserves it more than you."};
constexpr Text kSuzy = {.key = "quest.rutys_rods.ruty.suzy",
                        .english = "Just don't tell Suzy I gave it away for free. She'd never let me hear the end of it."};
constexpr Text kAfterwards = {.key = "quest.rutys_rods.ruty.afterwards",
                              .english = "My notebook's full, thanks to you.{page}"
                                         "Come buy some fish sometime, brother. I'll give ya the friend's price."};
constexpr Text kNoRoom = {.key = "quest.rutys_rods.ruty.no_room",
                          .english = "Your bag's full, brother. Make some room, and I'll give you what I owe ya."};
constexpr Text kReceived = {.key = "quest.rutys_rods.received", .english = "Received the %1."};

int RodNumber(std::string_view id) {
    const item::Def *rod = item::Find(id);
    return rod != nullptr ? rod->number : -1;
}

// The two towns as the player's data names them (Nolun on PAL, Norune on NTSC), and each language.
std::vector<std::string> Towns(Scene &scene) {
    std::string norune = scene.Game().SystemText(kNoruneName);
    std::string matataki = scene.Game().SystemText(kMatatakiName);
    return {norune.empty() ? "Norune Village" : norune, matataki.empty() ? "Matataki Village" : matataki};
}

// A rod for Toan, held up as the towns' events show an item received once Ruty has had her say; a town
// without that scene says so in a line. False, with Ruty saying so, where the bag has no room. A page
// read goes back to the one under it, so the pages are pushed last first.
bool GiveRod(Scene &scene, std::string_view rod, Text said, Text then, std::string_view next) {
    const int number = RodNumber(rod);
    if (number < 0 || !scene.Game().GiveItem(number)) {
        scene.Say(kNoRoom);
        return false;
    }
    scene.SetStage(next);
    if (scene.Game().ShowItemReceived(number)) {
        scene.SayThenClose(then);
    } else {
        const item::ItemText *text = item::CustomText(number, localize::LocalizeLanguageFile());
        scene.Say(then);
        scene.Say(kReceived, {text != nullptr ? text->name : std::string(rod)});
    }
    scene.Say(said, Towns(scene));
    return true;
}

void RutyAsks(Scene &scene) {
    scene.Say(kOffer, Towns(scene));
    scene.SetStage("inland");
}

void RutyOnTheFish(Scene &scene) {
    const int inland = scene.Var("inland");
    if (scene.AtStage("inland")) {
        if (inland >= static_cast<int>(kInland.size())) {
            GiveRod(scene, kSeaRod, kSeaRodGiven, kMardanAsk, "mardan");
        } else {
            scene.Say(kInlandWaiting, {std::to_string(kInland.size() - static_cast<std::size_t>(inland))});
        }
    } else if (scene.AtStage("mardan")) {
        if (scene.Var("mardan") > 0) {
            GiveRod(scene, kGarayanRod, kGarayanRodGiven, kBaronTale, "baron");
        } else {
            scene.Say(kMardanWaiting);
        }
    } else if (scene.AtStage("baron")) {
        if (scene.Var("baron") > 0) {
            GiveRod(scene, kBaronRod, kBaronRodGiven, kSuzy, "done");
        } else {
            scene.Say(kBaronWaiting);
        }
    } else {
        scene.Say(kAfterwards);
    }
}

} // namespace

std::span<const Text> RutysRodsTexts() {
    static constexpr std::array texts{kAsk,           kAbout,         kOffer,      kInlandWaiting, kSeaRodGiven,
                                      kMardanAsk,     kMardanWaiting, kGarayanRodGiven, kBaronTale, kBaronWaiting,
                                      kBaronRodGiven, kSuzy,          kAfterwards, kNoRoom,        kReceived};
    return texts;
}

bool RutysRodsAvailable() {
    return RodNumber(kSeaRod) >= 0 && RodNumber(kGarayanRod) >= 0 && RodNumber(kBaronRod) >= 0;
}

Definition RutysRods() {
    // The catches count only while she is waiting for them.
    fishing::CountSpecies(std::string(kQuest), "inland", kInland, {"inland"});
    fishing::CountCatches(std::string(kQuest), "mardan", {.species = "vanilla:fish_5"}, {"mardan"});
    fishing::CountCatches(std::string(kQuest), "baron", {.species = "vanilla:fish_17"}, {"baron"});
    const Condition caught_a_fish = [](const Context &) { return fishing::HasCaughtAFish(); };
    return Builder(std::string(kQuest))
        .Stages({"hidden", "offered", "inland", "mardan", "baron", "done"})
        .Advance("hidden", All({VillagerHome(kQueens, kRutyVillager), caught_a_fish}), "offered")
        .Option(kRuty, AtStage("offered"), kAsk, RutyAsks)
        .Option(kRuty, AtStages({"inland", "mardan", "baron", "done"}), kAbout, RutyOnTheFish)
        .Build();
}

} // namespace dc::quest
