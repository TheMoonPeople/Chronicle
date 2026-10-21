#include "itemdata.hpp"
#include "quests/quests.hpp"

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

namespace {

using Quest::Scene;
using Quest::Text;

// Carl and Gina, as the dialog framework numbers Norune's villagers (their lines in the town's
// message file start at 20 + villager * 20: Carl's 160, "Gina calls me shorty", and Gina's 200).
const Quest::NpcMatch kCarl{0, 7, {}};
const Quest::NpcMatch kGina{0, 9, {}};

constexpr int kXiao = 1;

// Carl's choices.
constexpr Text kCarlAsk = {"quest.carls_slingshot.carl.ask", "You look upset."};
constexpr Text kCarlAbout = {"quest.carls_slingshot.carl.about", "Your slingshot"};
constexpr Text kCarlStory = {"quest.carls_slingshot.carl.story",
                             "{name1}, bro... Gina borrowed my slingshot.{page}"
                             "She said she'd give it back the next day. That was ages ago!{page}"
                             "Every time I ask her, she runs off. It's not fair.{page}"
                             "Can you get it back for me bro? Please?"};
constexpr Text kCarlWaiting = {"quest.carls_slingshot.carl.waiting",
                               "Did Gina give it back yet? Go ask her!"};
constexpr Text kCarlBird = {"quest.carls_slingshot.carl.bird",
                            "A bird?! Birds don't take slingshots!{page}"
                            "She's fibbing. She always makes stuff up.{page}"
                            "Go talk to her again. I know she's lying!"};
constexpr Text kCarlAgain = {"quest.carls_slingshot.carl.again", "Go ask Gina again. And no birds this time!"};
constexpr Text kCarlJunk = {"quest.carls_slingshot.carl.junk",
                            "She broke it?{page}"
                            "And she made this...? It's all crooked. It's junk.{page}"
                            "...You keep it. I don't want it.{page}"
                            "Give it to {name2}.."};
constexpr Text kCarlNoRoom = {"quest.carls_slingshot.carl.no_room",
                              "{name2}'s got too many weapons to carry it. Make some room first."};
constexpr Text kReceived = {"quest.carls_slingshot.received", "Received the Bone Slingshot for {name2}."};

// Gina's.
constexpr Text kGinaAbout = {"quest.carls_slingshot.gina.about", "Carl's slingshot"};
constexpr Text kGinaExcuse = {"quest.carls_slingshot.gina.excuse",
                              "Carl's slingshot?{page}"
                              "Umm... a, a big bird took it! It was *THIS* big!{page}"
                              "It went whoosh, and flew away with it!"};
constexpr Text kGinaBird = {"quest.carls_slingshot.gina.bird", "The bird took it! Honest!"};
constexpr Text kGinaConfess = {"quest.carls_slingshot.gina.confess",
                               "...Carl doesn't believe the bird?{page}"
                               "...Okay. I was shooting at bottles, and it went SNAP.{page}"
                               "I didn't mean to! So I made him a new one!.{page}"
                               "Can you give it to him? I'm scared he'll be mad.."};
constexpr Text kGinaWaiting = {"quest.carls_slingshot.gina.waiting",
                               "Did you give Carl the new slingshot? Is he mad?"};

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

std::span<const Quest::Text> CarlsSlingshotTexts() {
    static constexpr Text texts[] = {kCarlAsk, kCarlAbout, kCarlStory, kCarlWaiting, kCarlBird,
                                     kCarlAgain, kCarlJunk, kCarlNoRoom, kReceived, kGinaAbout,
                                     kGinaExcuse, kGinaBird, kGinaConfess, kGinaWaiting};
    return texts;
}

Quest::Definition CarlsSlingshot() {
    return Quest::Builder("carls_slingshot")
        .Stages({"hidden", "offered", "asked", "excuse", "doubted", "confessed", "done"})
        .Advance("hidden", Quest::PartyMember(kXiao), "offered")
        .Option(kCarl, Quest::AtStage("offered"), kCarlAsk, CarlAsksForHelp)
        .Option(kCarl, Quest::AtStages({"asked", "excuse", "doubted", "confessed"}), kCarlAbout, CarlOnTheSlingshot)
        .Option(kGina, Quest::AtStages({"asked", "excuse", "doubted", "confessed"}), kGinaAbout, GinaOnTheSlingshot)
        .Build();
}
