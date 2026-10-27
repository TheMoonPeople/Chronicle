#include <array>

#include "itemdata.hpp"
#include "quest/quests.hpp"

// Carl's Slingshot: a sidequest between two of Norune's children, and the port's example quest
// (docs/QUEST_FRAMEWORK.md). Gina borrowed Carl's slingshot and never gave it back. She makes up a
// story, Carl doesn't buy it, and she owns up: she broke it, and made him a new one out of bones.
// Carl wants nothing to do with the crooked thing and tells Toan to keep it: it is the Bone
// Slingshot, Xiao's weapon, so the quest opens once Xiao is in the party. Carl and Gina come back to
// the village with two different georama pieces, so where the player places them decides when the
// quest can be done.
//
//   hidden  -- Xiao joins -->  offered  -- Carl asks for help -->  asked
//   asked  -- Gina's excuse -->  excuse  -- Carl doesn't buy it -->  doubted
//   doubted  -- Gina owns up -->  confessed  -- Carl gives up the new slingshot -->  done

namespace dc::quest {

using namespace dc::game;

namespace {

// Carl and Gina, as the dialog framework numbers Norune's villagers (their lines in the town's
// message file start at 20 + villager * 20: Carl's 160, "Gina calls me shorty", and Gina's 200).
const NpcMatch kCarl{.map_no = 0, .npc_id = 7, .mes_file_key = {}};
const NpcMatch kGina{.map_no = 0, .npc_id = 9, .mes_file_key = {}};

constexpr int kXiao = 1;

// Carl's choices.
constexpr Text kCarlAsk = {.key = "quest.carls_slingshot.carl.ask", .english = "You look upset."};
constexpr Text kCarlAbout = {.key = "quest.carls_slingshot.carl.about", .english = "Your slingshot"};
constexpr Text kCarlStory = {.key = "quest.carls_slingshot.carl.story",
                             .english = "{name1}, bro... Gina borrowed my slingshot.{page}"
                                        "She said she'd give it back the next day. That was ages ago!{page}"
                                        "Every time I ask her, she runs off. It's not fair.{page}"
                                        "Can you get it back for me bro? Please?"};
constexpr Text kCarlWaiting = {.key = "quest.carls_slingshot.carl.waiting",
                               .english = "Did Gina give it back yet? Go ask her!"};
constexpr Text kCarlBird = {.key = "quest.carls_slingshot.carl.bird",
                            .english = "A bird?! Birds don't take slingshots!{page}"
                                       "She's fibbing. She always makes stuff up.{page}"
                                       "Go talk to her again. I know she's lying!"};
constexpr Text kCarlAgain = {.key = "quest.carls_slingshot.carl.again", .english = "Go ask Gina again. And no birds this time!"};
constexpr Text kCarlJunk = {.key = "quest.carls_slingshot.carl.junk",
                            .english = "She broke it?{page}"
                                       "And she made this...? It's all crooked. It's junk.{page}"
                                       "...You keep it. I don't want it.{page}"
                                       "Give it to {name2}.."};
constexpr Text kCarlNoRoom = {.key = "quest.carls_slingshot.carl.no_room",
                              .english = "{name2}'s got too many weapons to carry it. Make some room first."};
constexpr Text kReceived = {.key = "quest.carls_slingshot.received", .english = "Received the Bone Slingshot for {name2}."};

// Gina's.
constexpr Text kGinaAbout = {.key = "quest.carls_slingshot.gina.about", .english = "Carl's slingshot"};
constexpr Text kGinaExcuse = {.key = "quest.carls_slingshot.gina.excuse",
                              .english = "Carl's slingshot?{page}"
                                         "Umm... a, a big bird took it! It was *THIS* big!{page}"
                                         "It went whoosh, and flew away with it!"};
constexpr Text kGinaBird = {.key = "quest.carls_slingshot.gina.bird", .english = "The bird took it! Honest!"};
constexpr Text kGinaConfess = {.key = "quest.carls_slingshot.gina.confess",
                               .english = "...Carl doesn't believe the bird?{page}"
                                          "...Okay. I was shooting at bottles, and it went SNAP.{page}"
                                          "I didn't mean to! So I made him a new one!.{page}"
                                          "Can you give it to him? I'm scared he'll be mad.."};
constexpr Text kGinaWaiting = {.key = "quest.carls_slingshot.gina.waiting",
                               .english = "Did you give Carl the new slingshot? Is he mad?"};

void CarlAsksForHelp(Scene &scene) {
    scene.Say(kCarlStory);
    scene.SetStage("asked");
}

void CarlOnTheSlingshot(Scene &scene) {
    if (scene.AtStage("excuse")) {
        scene.Say(kCarlBird);
        scene.SetStage("doubted");
    } else if (scene.AtStage("confessed")) {
        if (!scene.Game().GiveItem(ITEM_WEAPON_BONE_SLINGSHOT)) {
            scene.Say(kCarlNoRoom);
            return;
        }
        scene.SetStage("done");
        // Toan holds it up as the town's own events show an item received, once Carl has had his say;
        // a town without that scene says so in a line instead.
        if (scene.Game().ShowItemReceived(ITEM_WEAPON_BONE_SLINGSHOT)) {
            scene.SayThenClose(kCarlJunk);
        } else {
            scene.Say(kReceived);
            scene.Say(kCarlJunk);
        }
    } else if (scene.AtStage("doubted")) {
        scene.Say(kCarlAgain);
    } else {
        scene.Say(kCarlWaiting);
    }
}

void GinaOnTheSlingshot(Scene &scene) {
    if (scene.AtStage("asked")) {
        scene.Say(kGinaExcuse);
        scene.SetStage("excuse");
    } else if (scene.AtStage("doubted")) {
        scene.Say(kGinaConfess);
        scene.SetStage("confessed");
    } else if (scene.AtStage("confessed")) {
        scene.Say(kGinaWaiting);
    } else {
        scene.Say(kGinaBird);
    }
}

} // namespace

std::span<const Text> CarlsSlingshotTexts() {
    static constexpr std::array texts{kCarlAsk, kCarlAbout, kCarlStory, kCarlWaiting, kCarlBird,
                                      kCarlAgain, kCarlJunk, kCarlNoRoom, kReceived, kGinaAbout,
                                      kGinaExcuse, kGinaBird, kGinaConfess, kGinaWaiting};
    return texts;
}

Definition CarlsSlingshot() {
    return Builder("carls_slingshot")
        .Stages({"hidden", "offered", "asked", "excuse", "doubted", "confessed", "done"})
        .Advance("hidden", PartyMember(kXiao), "offered")
        .Option(kCarl, AtStage("offered"), kCarlAsk, CarlAsksForHelp)
        .Option(kCarl, AtStages({"asked", "excuse", "doubted", "confessed"}), kCarlAbout, CarlOnTheSlingshot)
        .Option(kGina, AtStages({"asked", "excuse", "doubted", "confessed"}), kGinaAbout, GinaOnTheSlingshot)
        .Build();
}

} // namespace dc::quest
