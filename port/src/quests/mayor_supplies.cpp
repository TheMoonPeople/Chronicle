#include "itemdata.hpp"
#include "quests/quests.hpp"

// The Mayor of Norune's supplies. Retail's talk script (gedit/e01/event.stb, the talk event) hands
// the player one of each of these they have none of, in the pack or stored, each time they talk to
// him, once they have been into the Divine Beast Cave and until Gaffer's Buggy is built; after that
// he talks about the buggy. Here it is a choice of his menu instead, "Do you have any supplies?",
// and the script's own handing out is held back (Quest::Retail::MayorSupplies, quest_host.cpp).

namespace {

using Quest::Scene;
using Quest::Text;

constexpr int kDivineBeastCave = 0;
// Gaffer's Buggy: part 7 of Norune.
constexpr int kNorune = 0;
constexpr int kGaffersBuggy = 7;

// The script's lines for them: what he says handing supplies over, and about the buggy once it is built.
constexpr int kSuppliesLine = kNoruneMayorLines + 7;
constexpr int kBuggyLine = kNoruneMayorLines + 9;

constexpr Text kAsk = {"quest.mayor_supplies.ask", "Do you have any supplies?"};
constexpr Text kHere = {"quest.mayor_supplies.here", "Here, take these with you."};
constexpr Text kBuggy = {"quest.mayor_supplies.buggy", "Gaffer's Buggy has all you need now."};
constexpr Text kStocked = {"quest.mayor_supplies.stocked",
                           "You seem to have everything you need. Come back if you run short."};
constexpr Text kNoRoom = {"quest.mayor_supplies.no_room",
                          "You've no room to carry any more. Make some space and come back."};

void HandOutSupplies(Scene &scene) {
    if (scene.Game().PartEventDone(kNorune, kGaffersBuggy)) {
        scene.SayTalkLine(kBuggyLine, kBuggy);
        return;
    }

    bool given = false;
    bool short_of_room = false;
    for (int item : kNoruneMayorSupplies) {
        if (scene.Game().HasItemAnywhere(item)) {
            continue;
        }
        if (scene.Game().GiveItem(item)) {
            given = true;
        } else {
            short_of_room = true;
        }
    }

    if (given) {
        scene.SayTalkLine(kSuppliesLine, kHere);
    } else if (short_of_room) {
        scene.Say(kNoRoom);
    } else {
        scene.Say(kStocked);
    }
}

} // namespace

const int kNoruneMayorSupplies[5] = {ITEM_TASTY_WATER, ITEM_BREAD, ITEM_REPAIR_POWDER, ITEM_ESCAPE_POWDER,
                                     ITEM_ANTIDOTE_DRINK};

std::span<const Quest::Text> MayorSuppliesTexts() {
    static constexpr Text texts[] = {kAsk, kHere, kBuggy, kStocked, kNoRoom};
    return texts;
}

Quest::Definition MayorSupplies() {
    return Quest::Builder("mayor_supplies")
        .Stages({"open"})
        .TakesOver(NoruneMayor(), Quest::Retail::MayorSupplies)
        .Option(NoruneMayor(), Quest::DungeonStarted(kDivineBeastCave), kAsk, HandOutSupplies)
        .Build();
}
