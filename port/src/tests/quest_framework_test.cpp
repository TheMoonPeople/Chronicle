// nlohmann/json before the game headers, whose one-letter macros it cannot stand.
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "dialog/framework.hpp"
#include "itemdata.hpp"
#include "mainselect.hpp"
#include "quest/framework.hpp"
#include "quest/quests.hpp"

namespace dc::test {

using namespace dc::game;
using namespace dc::quest;

namespace {

// The game as a quest sees it, kept in a few maps.
class FakeGame final : public quest::IGame {
public:
    std::map<int, int>                  items;
    std::map<int, int>                  stored;
    std::map<int, int>                  floors;
    std::map<int, int>                  dungeon_counts;
    std::map<std::pair<int, int>, bool> part_events;
    std::map<std::pair<int, int>, int>  parts_placed;
    std::map<std::pair<int, int>, bool> parts_complete;
    std::map<std::pair<int, int>, bool> requests;
    std::map<std::pair<int, int>, bool> villagers_home;
    std::map<int, bool>                 game_flags;
    std::map<int, int>                  game_numbers;
    std::map<std::pair<int, int>, bool> map_flags;
    std::map<int, std::string>          talk_lines;
    int                                 money = 0;
    int                                 party = 1;
    int                                 pack_room = 10;
    std::optional<quest::ShopSpec>      shop;
    bool                                can_show_items = true;
    std::vector<int>                    items_shown;

    [[nodiscard]] int ItemCount(int item) const override {
        const auto it = items.find(item);
        return it == items.end() ? 0 : it->second;
    }

    bool GiveItem(int item) override {
        if (pack_room <= 0) {
            return false;
        }
        --pack_room;
        ++items[item];
        return true;
    }

    bool TakeItem(int item) override {
        if (ItemCount(item) <= 0) {
            return false;
        }
        --items[item];
        ++pack_room;
        return true;
    }

    [[nodiscard]] int Money() const override { return money; }

    bool SpendMoney(int amount) override {
        if (amount > money) {
            return false;
        }
        money -= amount;
        return true;
    }

    void EarnMoney(int amount) override { money += amount; }

    [[nodiscard]] int DeepestFloor(int dungeon) const override {
        const auto it = floors.find(dungeon);
        return it == floors.end() ? -1 : it->second;
    }

    [[nodiscard]] bool HasItemAnywhere(int item) const override {
        const auto it = stored.find(item);
        return ItemCount(item) > 0 || (it != stored.end() && it->second > 0);
    }

    [[nodiscard]] int DungeonCount(int dungeon) const override {
        const auto it = dungeon_counts.find(dungeon);
        return it == dungeon_counts.end() ? 0 : it->second;
    }

    [[nodiscard]] int PartySize() const override { return party; }

    template <typename Map, typename Key>
    static auto Get(const Map &map, const Key &key) {
        const auto it = map.find(key);
        return it == map.end() ? typename Map::mapped_type{} : it->second;
    }

    [[nodiscard]] bool GameFlag(int flag) const override { return Get(game_flags, flag); }

    void SetGameFlag(int flag, bool on) override { game_flags[flag] = on; }

    [[nodiscard]] int GameNumber(int slot) const override { return Get(game_numbers, slot); }

    void SetGameNumber(int slot, int value) override { game_numbers[slot] = value; }

    [[nodiscard]] bool MapFlag(int map, int flag) const override { return Get(map_flags, std::make_pair(map, flag)); }

    void SetMapFlag(int map, int flag, bool on) override { map_flags[{map, flag}] = on; }

    [[nodiscard]] bool PartObtained(int town, int part) const override { return PartPlaced(town, part) > 0; }

    [[nodiscard]] int PartPlaced(int town, int part) const override { return Get(parts_placed, std::make_pair(town, part)); }

    [[nodiscard]] bool PartComplete(int town, int part) const override { return Get(parts_complete, std::make_pair(town, part)); }

    [[nodiscard]] bool PartEventDone(int town, int part) const override { return Get(part_events, std::make_pair(town, part)); }

    [[nodiscard]] bool RequestMet(int town, int request) const override { return Get(requests, std::make_pair(town, request)); }

    [[nodiscard]] bool VillagerHome(int town, int villager) const override {
        return Get(villagers_home, std::make_pair(town, villager));
    }

    [[nodiscard]] std::string TalkLine(int message) const override {
        const auto it = talk_lines.find(message);
        return it == talk_lines.end() ? std::string() : it->second;
    }

    void OpenShop(quest::ShopSpec spec) override { shop = std::move(spec); }

    bool ShowItemReceived(int item) override {
        if (!can_show_items) {
            return false;
        }
        items_shown.push_back(item);
        return true;
    }
};

float TenPerByte(std::string_view text) {
    return 10.0f * static_cast<float>(text.size());
}

class QuestFramework : public ::testing::Test {
protected:
    void SetUp() override {
        LanguageCode = 2; // en_gb: the built-in English.
        quest::Clear();
        item::Clear();
        quest::SetGame(&game);
        dialog::Shutdown();
        dialog::Init();
        dialog::SetTextMeasurer(TenPerByte);
        dialog::SetLayoutConfig(dialog::LayoutConfig{});
        dialog::RegisterOptionProvider(quest::CollectOptions);
    }

    void TearDown() override {
        dialog::Shutdown();
        quest::SetGame(nullptr);
        quest::Clear();
        item::Clear();
    }

    static dialog::NpcContext Npc(int map_no, int npc_id) {
        dialog::NpcContext context;
        context.map_no = map_no;
        context.npc_id = npc_id;
        context.host_plays_original = true;
        return context;
    }

    static const dialog::ActiveView &View() {
        const dialog::ActiveView *view = dialog::GetActiveView();
        EXPECT_NE(view, nullptr);
        static const dialog::ActiveView none;
        return view != nullptr ? *view : none;
    }

    static std::vector<std::string> Labels() { return View().formatted_choice_labels; }

    static std::string Stage(std::string_view quest) {
        const quest::Progress *progress = quest::FindProgress(quest);
        return progress != nullptr ? progress->stage : "<none>";
    }

    // Moves the hand to the choice labelled label and takes it.
    static void Pick(std::string_view label) {
        const std::vector<std::string> labels = Labels();
        int                            target = -1;
        for (size_t i = 0; i < labels.size(); ++i) {
            if (labels[i] == label) {
                target = static_cast<int>(i);
            }
        }
        ASSERT_GE(target, 0) << "no choice \"" << label << "\"";
        while (View().selected_choice != target) {
            ASSERT_TRUE(dialog::InputMoveDown());
        }
        ASSERT_TRUE(dialog::InputConfirm());
    }

    // Confirms through a page of text, every slice of it.
    static void Read() {
        const int slices = View().slice_count;
        for (int i = 0; i < slices; ++i) {
            ASSERT_TRUE(dialog::InputConfirm());
        }
    }

    FakeGame game;
};

quest::Definition Errand() {
    return quest::Builder("errand")
        .Stages({"start", "middle", "end"})
        .Advance("start", quest::HasItem(ITEM_BREAD), "middle")
        .Advance("middle", quest::HasMoney(100), "end")
        .Option({.map_no = 0, .npc_id = 7}, quest::AtStage("middle"), {.key = "quest.errand.ask", .english = "The errand"},
                [](quest::Scene &scene) { scene.SetVar("asked", scene.Var("asked") + 1); })
        .Build();
}

TEST_F(QuestFramework, BuilderRejectsQuestsThatCannotWork) {
    EXPECT_THROW(quest::Builder("none").Build(), std::logic_error);
    EXPECT_THROW(quest::Builder("twice").Stages({"a", "a"}).Build(), std::logic_error);
    EXPECT_THROW(quest::Builder("lost").Stages({"a"}).Advance("a", quest::HasMoney(1), "b").Build(), std::logic_error);
    EXPECT_THROW(quest::Builder("idle").Stages({"a"}).Option({}, nullptr, {"k", "Idle"}, nullptr).Build(),
                 std::logic_error);

    quest::Register(Errand());
    EXPECT_THROW(quest::Register(Errand()), std::logic_error);
}

TEST_F(QuestFramework, RulesMoveQuestsOnAsFarAsTheyHold) {
    quest::Register(Errand());
    quest::Evaluate();
    EXPECT_EQ(Stage("errand"), "start");

    // Both rules hold: one evaluation goes all the way.
    game.items[ITEM_BREAD] = 1;
    game.money = 100;
    quest::Evaluate();
    EXPECT_EQ(Stage("errand"), "end");
}

TEST_F(QuestFramework, ChoicesShowForTheirNpcWhileTheirConditionHolds) {
    quest::Register(Errand());
    std::vector<dialog::ChoiceItem> out;
    game.items[ITEM_BREAD] = 1;

    quest::CollectOptions(Npc(0, 8), out);
    EXPECT_TRUE(out.empty());
    quest::CollectOptions(Npc(0, 7), out);
    ASSERT_EQ(out.size(), 1u);
    EXPECT_EQ(out[0].fallback_label, "The errand");

    // Choosing it acts on the quest's numbers.
    ASSERT_TRUE(dialog::TryInterceptOpen(Npc(0, 7), {}));
    Pick("The errand");
    EXPECT_EQ(quest::FindProgress("errand")->vars.at("asked"), 1);
}

TEST_F(QuestFramework, ProgressRoundTripsAndKeepsQuestsItDoesNotKnow) {
    quest::Register(Errand());
    game.items[ITEM_BREAD] = 1;
    quest::Evaluate();
    ASSERT_TRUE(quest::LoadJson(quest::SaveJson()));
    EXPECT_EQ(Stage("errand"), "middle");

    const std::string saved =
        R"({"version": 1, "quests": {"errand": {"stage": "end", "vars": {"asked": 3}},
                                     "modded": {"stage": "x", "vars": {}}}})";
    ASSERT_TRUE(quest::LoadJson(saved));
    EXPECT_EQ(Stage("errand"), "end");
    EXPECT_EQ(quest::FindProgress("errand")->vars.at("asked"), 3);
    // A quest no longer registered is written back as it was.
    const nlohmann::json written = nlohmann::json::parse(quest::SaveJson());
    EXPECT_EQ(written["quests"]["modded"]["stage"], "x");

    // A stage the quest does not have, or text that is not a quest file, starts it again.
    ASSERT_TRUE(quest::LoadJson(R"({"version": 1, "quests": {"errand": {"stage": "gone"}}})"));
    EXPECT_EQ(Stage("errand"), "start");
    quest::LoadJson(R"({"version": 1, "quests": {"errand": {"stage": "end"}}})");
    EXPECT_FALSE(quest::LoadJson("not json"));
    EXPECT_EQ(Stage("errand"), "start");
}

// A quest of every kind of step, as a shipped one would be: a rule, a story, a question, an item
// handed over, and a shop at the end. It lives only in this test.
quest::Definition FetchQuest() {
    using quest::Scene;
    constexpr quest::Text ask = {.key = "quest.test_fetch.ask", .english = "The errand"};
    constexpr quest::Text story = {.key = "quest.test_fetch.story", .english = "One.{page}Two.{page}Three."};
    constexpr quest::Text reminder = {.key = "quest.test_fetch.reminder", .english = "Any luck with the bread?"};
    constexpr quest::Text found = {.key = "quest.test_fetch.found", .english = "Is that bread?"};
    constexpr quest::Text give = {.key = "quest.test_fetch.give", .english = "Hand it over"};
    constexpr quest::Text not_yet = {.key = "quest.test_fetch.not_yet", .english = "Not yet"};
    constexpr quest::Text thanks = {.key = "quest.test_fetch.thanks", .english = "Thank you!{page}I sell it for %1 Gilda now."};
    constexpr quest::Text buy = {.key = "quest.test_fetch.buy", .english = "Buy bread"};
    constexpr quest::Text greeting = {.key = "quest.test_fetch.greeting", .english = "Fresh bread!"};

    auto hand_over = [=](Scene &scene) {
        if (!scene.Game().TakeItem(ITEM_BREAD)) {
            scene.Back();
            return;
        }
        scene.SetStage("done");
        scene.Back();
        scene.Say(thanks, {"50"});
    };
    auto talk = [=](Scene &scene) {
        if (scene.AtStage("asked") && scene.Game().ItemCount(ITEM_BREAD) > 0) {
            scene.Ask(found, {scene.Choice(give, hand_over), scene.Choice(not_yet, [](Scene &reply) { reply.Back(); })});
            return;
        }
        scene.Say(scene.AtStage("asked") ? reminder : story);
        scene.SetStage("asked");
    };
    auto sell = [=](Scene &scene) {
        quest::ShopSpec shop;
        shop.goods = {
            {.item = ITEM_BREAD, .price = 50}
        };
        shop.lines = {
            {quest::ShopLine::Greeting, greeting}
        };
        scene.OpenShop(std::move(shop));
    };
    return quest::Builder("test_fetch")
        .Stages({"hidden", "offered", "asked", "done"})
        .Advance("hidden", quest::FloorReached(0), "offered")
        .Option({.map_no = 0, .npc_id = 4}, quest::AtStages({"offered", "asked"}), ask, talk)
        .Option({.map_no = 0, .npc_id = 4}, quest::AtStage("done"), buy, sell)
        .Build();
}

TEST_F(QuestFramework, AQuestRunsFromItsRuleToItsShop) {
    quest::Register(FetchQuest());
    const dialog::NpcContext ctx = Npc(0, 4);

    // Before the rule holds, the villager has nothing new to say.
    EXPECT_FALSE(dialog::TryInterceptOpen(ctx, {}));
    EXPECT_EQ(Stage("test_fetch"), "hidden");

    game.floors[0] = 0;
    ASSERT_TRUE(dialog::TryInterceptOpen(ctx, {}));
    EXPECT_EQ(Labels(), (std::vector<std::string>{"Talk", "The errand", "Never mind"}));
    EXPECT_EQ(Stage("test_fetch"), "offered");

    Pick("The errand");
    EXPECT_EQ(View().slice_count, 3);
    EXPECT_EQ(Stage("test_fetch"), "asked");
    Read();

    // Without the item, a reminder.
    Pick("The errand");
    EXPECT_EQ(View().slice_count, 1);
    EXPECT_NE(View().current_slice.raw_text.find("Any luck"), std::string::npos);
    Read();

    // With it, a question; handing it over finishes the quest, and the menu changes at once.
    game.items[ITEM_BREAD] = 1;
    Pick("The errand");
    EXPECT_EQ(Labels(), (std::vector<std::string>{"Hand it over", "Not yet"}));
    Pick("Hand it over");
    EXPECT_EQ(game.ItemCount(ITEM_BREAD), 0);
    EXPECT_EQ(Stage("test_fetch"), "done");
    EXPECT_NE(View().current_slice.raw_text.find("Thank you"), std::string::npos);
    ASSERT_TRUE(dialog::InputConfirm());
    EXPECT_NE(View().current_slice.raw_text.find("50 Gilda"), std::string::npos);
    ASSERT_TRUE(dialog::InputConfirm());

    EXPECT_EQ(Labels(), (std::vector<std::string>{"Talk", "Buy bread", "Never mind"}));
    Pick("Buy bread");
    EXPECT_FALSE(dialog::IsSessionActive());
    ASSERT_TRUE(game.shop.has_value());
    ASSERT_EQ(game.shop->goods.size(), 1u);
    EXPECT_EQ(game.shop->goods[0].item, ITEM_BREAD);
    EXPECT_EQ(game.shop->goods[0].price, 50);
    // The villager stands behind the counter by default, saying the quest's lines.
    EXPECT_EQ(game.shop->keeper, quest::Shopkeeper::Speaker);
    EXPECT_EQ(game.shop->lines.size(), 1u);
}

TEST_F(QuestFramework, MayorSuppliesSitBelowTalkOnceTheCaveIsOpen) {
    quest::Register(MayorSupplies());
    // A quest registered after it comes after it in his menu.
    quest::Register(quest::Builder("test_after")
                        .Stages({"open"})
                        .Option(NoruneMayor(), nullptr, {.key = "quest.test_after.ask", .english = "Something else"},
                                [](quest::Scene &scene) { scene.Back(); })
                        .Build());
    const dialog::NpcContext mayor = Npc(NoruneMayor().map_no, NoruneMayor().npc_id);

    ASSERT_TRUE(dialog::TryInterceptOpen(mayor, {}));
    EXPECT_EQ(Labels(), (std::vector<std::string>{"Talk", "Something else", "Never mind"}));
    dialog::ForceCloseSession();

    game.dungeon_counts[0] = 1;
    ASSERT_TRUE(dialog::TryInterceptOpen(mayor, {}));
    EXPECT_EQ(Labels(), (std::vector<std::string>{"Talk", "Do you have any supplies?", "Something else", "Never mind"}));
}

TEST_F(QuestFramework, MayorReplacesOnlyWhatThePlayerHasNoneOf) {
    quest::Register(MayorSupplies());
    const dialog::NpcContext mayor = Npc(NoruneMayor().map_no, NoruneMayor().npc_id);
    game.dungeon_counts[0] = 1;
    game.talk_lines[kNoruneMayorLines + 7] = "Take these.";
    // Bread in the pack, Escape Powder stored: only the other three come from him.
    game.items[ITEM_BREAD] = 1;
    game.stored[ITEM_ESCAPE_POWDER] = 1;

    ASSERT_TRUE(dialog::TryInterceptOpen(mayor, {}));
    Pick("Do you have any supplies?");
    EXPECT_EQ(View().current_slice.raw_text, "Take these.");
    EXPECT_EQ(game.ItemCount(ITEM_TASTY_WATER), 1);
    EXPECT_EQ(game.ItemCount(ITEM_REPAIR_POWDER), 1);
    EXPECT_EQ(game.ItemCount(ITEM_ANTIDOTE_DRINK), 1);
    EXPECT_EQ(game.ItemCount(ITEM_BREAD), 1);
    EXPECT_EQ(game.ItemCount(ITEM_ESCAPE_POWDER), 0);
    Read();

    // Asked again with everything had, nothing more.
    Pick("Do you have any supplies?");
    EXPECT_NE(View().current_slice.raw_text.find("everything you need"), std::string::npos);
    EXPECT_EQ(game.ItemCount(ITEM_TASTY_WATER), 1);
}

TEST_F(QuestFramework, MayorTalksOfTheBuggyOnceItIsBuilt) {
    quest::Register(MayorSupplies());
    const dialog::NpcContext mayor = Npc(NoruneMayor().map_no, NoruneMayor().npc_id);
    game.dungeon_counts[0] = 1;
    game.part_events[{0, 7}] = true;
    game.talk_lines[kNoruneMayorLines + 9] = "Gaffer's Buggy line.";

    ASSERT_TRUE(dialog::TryInterceptOpen(mayor, {}));
    Pick("Do you have any supplies?");
    EXPECT_EQ(View().current_slice.raw_text, "Gaffer's Buggy line.");
    EXPECT_EQ(game.ItemCount(ITEM_BREAD), 0);
}

TEST_F(QuestFramework, MayorSaysSoWhenThePackIsFull) {
    quest::Register(MayorSupplies());
    const dialog::NpcContext mayor = Npc(NoruneMayor().map_no, NoruneMayor().npc_id);
    game.dungeon_counts[0] = 1;
    game.pack_room = 0;

    ASSERT_TRUE(dialog::TryInterceptOpen(mayor, {}));
    Pick("Do you have any supplies?");
    EXPECT_NE(View().current_slice.raw_text.find("no room"), std::string::npos);
}

TEST_F(QuestFramework, CarlsSlingshotGoesBackAndForthToXiaosNewWeapon) {
    quest::Register(CarlsSlingshot());
    const dialog::NpcContext carl = Npc(0, 7);
    const dialog::NpcContext gina = Npc(0, 9);

    // Before Xiao joins, neither child has anything new to say.
    EXPECT_FALSE(dialog::TryInterceptOpen(carl, {}));
    EXPECT_FALSE(dialog::TryInterceptOpen(gina, {}));
    game.party = 2;

    // Carl asks for help; only then does Gina have something to answer.
    EXPECT_FALSE(dialog::TryInterceptOpen(gina, {}));
    ASSERT_TRUE(dialog::TryInterceptOpen(carl, {}));
    EXPECT_EQ(Labels(), (std::vector<std::string>{"Talk", "You look upset.", "Never mind"}));
    Pick("You look upset.");
    EXPECT_EQ(View().slice_count, 4);
    Read();
    EXPECT_EQ(Stage("carls_slingshot"), "asked");
    EXPECT_EQ(Labels(), (std::vector<std::string>{"Talk", "Your slingshot", "Never mind"}));
    dialog::ForceCloseSession();

    // Gina's excuse.
    ASSERT_TRUE(dialog::TryInterceptOpen(gina, {}));
    Pick("Carl's slingshot");
    EXPECT_NE(View().current_slice.raw_text.find("Carl's slingshot?"), std::string::npos);
    Read();
    EXPECT_EQ(Stage("carls_slingshot"), "excuse");
    dialog::ForceCloseSession();

    // Carl doesn't buy it.
    ASSERT_TRUE(dialog::TryInterceptOpen(carl, {}));
    Pick("Your slingshot");
    EXPECT_NE(View().current_slice.raw_text.find("A bird?!"), std::string::npos);
    Read();
    EXPECT_EQ(Stage("carls_slingshot"), "doubted");
    dialog::ForceCloseSession();

    // Gina owns up.
    ASSERT_TRUE(dialog::TryInterceptOpen(gina, {}));
    Pick("Carl's slingshot");
    Read();
    EXPECT_EQ(Stage("carls_slingshot"), "confessed");
    dialog::ForceCloseSession();

    // Carl gives the new one up: Xiao's, unless she has no room for another weapon.
    ASSERT_TRUE(dialog::TryInterceptOpen(carl, {}));
    game.pack_room = 0;
    Pick("Your slingshot");
    EXPECT_NE(View().current_slice.raw_text.find("Make some room"), std::string::npos);
    EXPECT_EQ(Stage("carls_slingshot"), "confessed");
    Read();
    game.pack_room = 1;
    Pick("Your slingshot");
    EXPECT_EQ(game.ItemCount(ITEM_WEAPON_BONE_SLINGSHOT), 1);
    EXPECT_EQ(Stage("carls_slingshot"), "done");
    EXPECT_NE(View().current_slice.raw_text.find("She broke it?"), std::string::npos);
    // His say read, the conversation ends, and Toan holds the slingshot up.
    Read();
    EXPECT_FALSE(dialog::IsSessionActive());
    EXPECT_EQ(game.items_shown, (std::vector<int>{ITEM_WEAPON_BONE_SLINGSHOT}));

    // Done: neither child brings it up again.
    EXPECT_FALSE(dialog::TryInterceptOpen(carl, {}));
    EXPECT_FALSE(dialog::TryInterceptOpen(gina, {}));
}

TEST_F(QuestFramework, CarlSaysTheSlingshotIsReceivedWhereTheTownCannotShowIt) {
    quest::Register(CarlsSlingshot());
    game.party = 2;
    game.can_show_items = false;
    const dialog::NpcContext carl = Npc(0, 7);
    const dialog::NpcContext gina = Npc(0, 9);
    for (const dialog::NpcContext *npc : {&carl, &gina, &carl, &gina}) {
        ASSERT_TRUE(dialog::TryInterceptOpen(*npc, {}));
        Pick(View().formatted_choice_labels[1]);
        Read();
        dialog::ForceCloseSession();
    }
    ASSERT_EQ(Stage("carls_slingshot"), "confessed");

    ASSERT_TRUE(dialog::TryInterceptOpen(carl, {}));
    Pick("Your slingshot");
    Read();
    EXPECT_NE(View().current_slice.raw_text.find("Received the Bone Slingshot"), std::string::npos);
    Read();
    EXPECT_TRUE(dialog::IsSessionActive());
}

TEST_F(QuestFramework, ConditionsReadTheGamesFlagsAndGeorama) {
    // One stage a condition: the rules move the quest on as far as the game allows.
    quest::Register(quest::Builder("test_georama")
                        .Stages({"a", "b", "c", "d", "e", "f", "g", "h"})
                        .Advance("a", quest::GameFlagSet(200), "b")
                        .Advance("b", quest::GameNumberAtLeast(3, 100), "c")
                        .Advance("c", quest::MapFlagSet(0, 12), "d")
                        .Advance("d", quest::PartPlaced(1, 4), "e")
                        .Advance("e", quest::PartComplete(1, 4), "f")
                        .Advance("f", quest::RequestMet(1, 2), "g")
                        .Advance("g", quest::All({quest::VillagerHome(0, 9), quest::EventDone(0, 7)}), "h")
                        .Build());
    const auto stage_after = [&] {
        quest::Evaluate();
        return Stage("test_georama");
    };

    EXPECT_EQ(stage_after(), "a");
    game.game_flags[200] = true;
    game.game_numbers[3] = 99;
    EXPECT_EQ(stage_after(), "b");
    game.game_numbers[3] = 100;
    game.map_flags[{0, 12}] = true;
    // The same part in another town does not count.
    game.parts_placed[{0, 4}] = 1;
    EXPECT_EQ(stage_after(), "d");
    game.parts_placed[{1, 4}] = 1;
    game.parts_complete[{1, 4}] = true;
    game.requests[{1, 2}] = true;
    game.villagers_home[{0, 9}] = true;
    EXPECT_EQ(stage_after(), "g");
    game.part_events[{0, 7}] = true;
    EXPECT_EQ(stage_after(), "h");
}

TEST_F(QuestFramework, AQuestCanSetTheGamesFlags) {
    quest::Register(quest::Builder("test_flags")
                        .Stages({"open"})
                        .Option({.map_no = 0, .npc_id = 4}, nullptr, {.key = "quest.test_flags.ask", .english = "Flags"},
                                [](quest::Scene &scene) {
                                    scene.Game().SetGameFlag(300, true);
                                    scene.Game().SetGameNumber(5, 7);
                                    scene.Game().SetMapFlag(2, 9, true);
                                })
                        .Build());
    ASSERT_TRUE(dialog::TryInterceptOpen(Npc(0, 4), {}));
    Pick("Flags");
    EXPECT_TRUE(game.GameFlag(300));
    EXPECT_EQ(game.GameNumber(5), 7);
    EXPECT_TRUE(game.MapFlag(2, 9));
}

TEST_F(QuestFramework, TheSuppliesQuestTakesOverOnlyTheMayors) {
    EXPECT_FALSE(quest::TakesOver(Npc(0, 13), quest::Retail::MayorSupplies));
    quest::Register(MayorSupplies());
    EXPECT_TRUE(quest::TakesOver(Npc(0, 13), quest::Retail::MayorSupplies));
    EXPECT_FALSE(quest::TakesOver(Npc(0, 12), quest::Retail::MayorSupplies));
    EXPECT_FALSE(quest::TakesOver(Npc(1, 13), quest::Retail::MayorSupplies));
}

TEST_F(QuestFramework, ItemsCanBeNamedByTheirIds) {
    item::Register({.id = "test:bread", .number = ITEM_BREAD});
    quest::Register(quest::Builder("by_id").Stages({"start", "end"}).Advance("start", quest::HasItem("test:bread", 2), "end").Build());
    game.items[ITEM_BREAD] = 1;
    quest::Evaluate();
    EXPECT_EQ(Stage("by_id"), "start");
    game.items[ITEM_BREAD] = 2;
    quest::Evaluate();
    EXPECT_EQ(Stage("by_id"), "end");
}

TEST_F(QuestFramework, AnIdIsLookedUpAsTheConditionIsChecked) {
    quest::Register(quest::Builder("later").Stages({"start", "end"}).Advance("start", quest::HasItem("test:cake"), "end").Build());
    game.items[-1] = 5;
    quest::Evaluate();
    EXPECT_EQ(Stage("later"), "start");

    item::Register({.id = "test:cake", .number = ITEM_CHEESE});
    game.items[ITEM_CHEESE] = 1;
    quest::Evaluate();
    EXPECT_EQ(Stage("later"), "end");
}

TEST_F(QuestFramework, ItemsCanBeCountedByTrait) {
    item::Register({.id = "test:bread", .number = ITEM_BREAD});
    item::Register(item::Def{.id = "test:flapping_fish", .number = ITEM_FLAPPING_FISH}.With(item::Fish{}));
    item::Register(item::Def{.id = "test:rotten_fish", .number = ITEM_ROTTEN_FISH}.With(item::Fish{}));
    quest::Register(quest::Builder("fishmonger").Stages({"start", "end"}).Advance("start", quest::HasItemWith<item::Fish>(3), "end").Build());
    game.items[ITEM_BREAD] = 5;
    game.items[ITEM_FLAPPING_FISH] = 2;
    quest::Evaluate();
    EXPECT_EQ(Stage("fishmonger"), "start");
    EXPECT_EQ(quest::ItemCountWith<item::Fish>(game), 2);
    game.items[ITEM_ROTTEN_FISH] = 1;
    quest::Evaluate();
    EXPECT_EQ(Stage("fishmonger"), "end");
}

} // namespace

} // namespace dc::test
