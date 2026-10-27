#pragma once

#include <cstdint>
#include <functional>
#include <initializer_list>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "dialog/framework.hpp"
#include "item/framework.hpp"

/**
 * Quests (docs/QUEST_FRAMEWORK.md). A quest is a state machine: named stages, its own numbers, rules
 * that move it on when a condition holds, and choices it adds to NPCs' conversations. A quest reaches
 * the game only through quest::IGame, so it runs the same against the save data in play and against a
 * fake in a test. Like dialog_framework.hpp, this header includes no PS2 headers.
 */
namespace dc::quest {

/**
 * Text a quest shows: its key in lang/<language>.json, and the English for a language without it.
 */
struct Text {
    std::string_view key;
    std::string_view english;
};

/**
 * One item a quest's shop sells, at the quest's price.
 */
struct ShopGood {
    int item = -1;
    int price = 0;
};

/**
 * Who stands behind a quest's shop counter.
 */
enum class Shopkeeper : uint8_t {
    Speaker, /**< The NPC the conversation was with, by their town model; look_of_shop's where it cannot be. */
    Retail,  /**< look_of_shop's own shopkeeper. */
};

/**
 * The shopkeeper's lines a quest's shop can give, by when the shop screen shows them.
 */
enum class ShopLine : uint8_t {
    Greeting,       /**< Opening the shop. */
    Browsing,       /**< The cursor on the shop's goods. */
    Selling,        /**< The cursor on the player's own items. */
    NotEnoughMoney, /**< A purchase the player cannot pay for. */
    NoRoom,         /**< A purchase the player has no room for. */
    Confirm,        /**< Asking to settle up. */
    Thanks,         /**< A purchase made. */
    Goodbye,        /**< Leaving. */
};

/**
 * A shop screen a quest opens: retail's item shop screen, with the quest's goods, prices, shopkeeper
 * and lines, laid out as retail shop look_of_shop is (where its shopkeeper stands, and the lines the
 * quest does not give). Retail's shops keep their own.
 */
struct ShopSpec {
    int                                    look_of_shop = 0;
    std::vector<ShopGood>                  goods;
    Shopkeeper                             keeper = Shopkeeper::Speaker;
    std::vector<std::pair<ShopLine, Text>> lines;
};

/**
 * What a quest can ask of the game and do to it.
 */
class IGame {
public:
    virtual ~IGame() = default;

    /**
     * Items the player carries (the pack and the stored items).
     */
    virtual int ItemCount(int item) const = 0;
    /**
     * Gives the player one item, as a pickup does: a dungeon item into the pack, a weapon to the
     * character who wields it. False, changing nothing, where there is no room for it.
     */
    virtual bool GiveItem(int item) = 0;
    /**
     * Takes one item from the player: false, changing nothing, where they have none.
     */
    virtual bool TakeItem(int item) = 0;

    /**
     * The money the player has.
     */
    virtual int Money() const = 0;
    /**
     * Takes amount from the player: false, changing nothing, where they have less than amount.
     */
    virtual bool SpendMoney(int amount) = 0;
    /**
     * Gives the player amount, up to the most the game holds.
     */
    virtual void EarnMoney(int amount) = 0;

    /**
     * Whether the player has the item anywhere: the pack or the stored items.
     */
    virtual bool HasItemAnywhere(int item) const = 0;

    /**
     * The deepest floor reached in a dungeon (0 is the Divine Beast Cave), or -1 if never entered.
     */
    virtual int DeepestFloor(int dungeon) const = 0;
    /**
     * A dungeon's count as the town scripts read it (CSaveData::QuestDungeon): 0 until the player has
     * been in.
     */
    virtual int DungeonCount(int dungeon) const = 0;
    /**
     * Game flags, 0 to 8191 (_SGET_GAME_FLAG, _SSET_GAME_FLAG), as the town scripts read and set them.
     * Setting one changes what retail does: a quest sets only flags it knows the meaning of.
     */
    virtual bool GameFlag(int flag) const = 0;
    virtual void SetGameFlag(int flag, bool on) = 0;
    /**
     * The 32 game numbers (_SGET_GAME_INT_FLAG, _SSET_GAME_INT_FLAG).
     */
    virtual int  GameNumber(int slot) const = 0;
    virtual void SetGameNumber(int slot, int value) = 0;
    /**
     * A map's flags, as the scripts read them (_SGET_MAP_FLAG): set where the save holds 0.
     */
    virtual bool MapFlag(int map, int flag) const = 0;
    virtual void SetMapFlag(int map, int flag, bool on) = 0;

    /**
     * Georama, by town (0 Norune, 1 Matataki, 2 Queens, 3 Muska Lacka, 4 Yellow Drops) and part (0 to
     * 23, as the town's parts are numbered): the town the player is in as it stands, any other as its
     * save keeps it.
     */
    virtual bool PartObtained(int town, int part) const = 0;
    /**
     * Copies of the part standing in the town.
     */
    virtual int PartPlaced(int town, int part) const = 0;
    /**
     * Every element the part wants is in place (CEditPartsInfo::CheckComplete).
     */
    virtual bool PartComplete(int town, int part) const = 0;
    /**
     * The part's completion event has run (CEditPartsInfo::GetCompEvent): Norune's part 7 is Gaffer's Buggy.
     */
    virtual bool PartEventDone(int town, int part) const = 0;
    /**
     * A villager's georama request is met (_SCHECK_REQUEST): request n of the town, 0 to 23.
     */
    virtual bool RequestMet(int town, int request) const = 0;
    /**
     * A villager, by the number the dialog framework gives them, is back in the town: their house
     * stands and they are in it (_CHECK_VILLAGER). Known only for the town the player is in; false
     * for any other.
     */
    virtual bool VillagerHome(int town, int villager) const = 0;
    /**
     * How many have joined the party, in the order they join: Toan (1), Xiao (2), Goro, Ruby,
     * Ungaga, Osmond.
     */
    virtual int PartySize() const = 0;

    /**
     * Line message of the town's villager message file, the one the NPCs' own conversations come
     * from, as GameTextDecode writes it; empty where the file has no such line.
     */
    virtual std::string TalkLine(int message) const = 0;

    /**
     * Opens the shop once the conversation it was chosen in has ended.
     */
    virtual void OpenShop(ShopSpec shop) = 0;
    /**
     * Shows the player receiving an item, as the towns' events do (Toan holding it up, the jingle and
     * the message naming it), once the conversation has ended. Shows only: GiveItem gives. False
     * where the town cannot show it, and the quest says so itself.
     */
    virtual bool ShowItemReceived(int item) = 0;
};

/**
 * One quest's progress, as a save keeps it.
 */
struct Progress {
    std::string                stage;
    std::map<std::string, int> vars;
};

class Definition;

/**
 * A quest as its rules and choices see it: its progress and the game.
 */
class Context {
public:
    Context(const Definition &quest, Progress &progress, IGame &game);

    const std::string &QuestId() const;
    IGame             &Game() const;
    const std::string &Stage() const;
    bool               AtStage(std::string_view stage) const;
    /**
     * Moves the quest to stage, which must be one of its stages.
     */
    void SetStage(std::string_view stage);
    /**
     * A number the quest keeps, 0 until set.
     */
    int  Var(std::string_view name) const;
    void SetVar(std::string_view name, int value);

private:
    const Definition *quest;
    Progress         *progress;
    IGame            *game;
};

class Scene;
using SceneFn = std::function<void(Scene &)>;

/**
 * A conversation choice of a quest's, being acted on: the quest's context and the conversation.
 */
class Scene : public Context {
public:
    Scene(const Definition &quest, Progress &progress, IGame &game, dialog::ISessionController &session);

    /**
     * A page of text, then back to the page under it. "{page}" splits it; %1... are args.
     */
    void Say(Text text, std::vector<std::string> args = {});
    /**
     * A line of the NPC's own (IGame::TalkLine), said as Say says text; fallback where the town's
     * message file has none.
     */
    void SayTalkLine(int message, Text fallback);
    /**
     * A question with choices of the quest's own (Choice); cancelling goes back.
     */
    void Ask(Text prompt, std::vector<dialog::ChoiceItem> choices, std::vector<std::string> args = {});
    /**
     * A choice for Ask: on_select runs as a Scene of the same quest.
     */
    dialog::ChoiceItem Choice(Text label, SceneFn on_select,
                              dialog::ChoiceColor color = dialog::ChoiceColor::White) const;
    /**
     * Back to the page under this one, or out of the conversation from the NPC's menu.
     */
    void Back();
    /**
     * Ends the conversation.
     */
    void Close();
    /**
     * Ends the conversation and opens the shop.
     */
    void OpenShop(ShopSpec shop);
    /**
     * A page of text, as Say says it, that ends the conversation once read.
     */
    void SayThenClose(Text text, std::vector<std::string> args = {});

    dialog::ISessionController &Session() const { return *session; }

private:
    dialog::ISessionController *session;
};

using Condition = std::function<bool(const Context &)>;

/**
 * Matches any map or NPC in an NpcMatch.
 */
inline constexpr int kAny = -1;

/**
 * Which NPCs a choice is for: kAny, or an empty key, matches any.
 */
struct NpcMatch {
    int         map_no = kAny;
    int         npc_id = kAny;
    std::string mes_file_key;

    bool Matches(const dialog::NpcContext &npc) const;
};

/**
 * How many items with trait T the player carries, each counted as IGame::ItemCount counts it.
 */
template <class T>
int ItemCountWith(const IGame &game) {
    int count = 0;
    for (const item::Def *item : item::With<T>()) {
        count += game.ItemCount(item->number);
    }
    return count;
}

/**
 * Conditions, for rules and choices.
 */
Condition AtStage(std::string_view stage);
Condition AtStages(std::initializer_list<std::string_view> stages);
Condition HasItem(int item, int count = 1);
/**
 * At least count of the item with an ID, looked up as the condition is checked so a quest can be
 * built before the items are registered. Never holds for an ID no item has.
 */
Condition HasItem(std::string_view id, int count = 1);

/**
 * At least count items with trait T between them, of any kind: HasItemWith<item::Fish>() for any fish.
 */
template <class T>
Condition HasItemWith(int count = 1) {
    return [count](const Context &ctx) { return ItemCountWith<T>(ctx.Game()) >= count; };
}

Condition HasMoney(int amount);
Condition FloorReached(int dungeon, int floor = 0);
Condition DungeonStarted(int dungeon);
Condition GameFlagSet(int flag);
Condition GameNumberAtLeast(int slot, int value);
Condition MapFlagSet(int map, int flag);
Condition PartPlaced(int town, int part);
Condition PartComplete(int town, int part);
Condition RequestMet(int town, int request);
Condition VillagerHome(int town, int villager);
/**
 * Character chara (0 Toan, 1 Xiao, 2 Goro...) has joined the party.
 */
Condition PartyMember(int chara);
Condition EventDone(int town, int part);
Condition VarAtLeast(std::string_view name, int value);
Condition All(std::vector<Condition> conditions);
Condition Any(std::vector<Condition> conditions);
Condition Not(Condition condition);

/**
 * Something retail does by itself that a quest takes over, doing it in its own way: while the quest
 * is registered, the host holds retail's version back for the NPCs the quest names.
 */
enum class Retail : uint8_t {
    MayorSupplies, /**< The Mayor of Norune's talk script hands out supplies the player has none of, unasked, until Gaffer's Buggy is built. */
};

/**
 * A quest, as Builder makes it.
 */
class Definition {
public:
    struct Rule {
        std::string from;
        Condition   when;
        std::string to;
    };

    struct Option {
        NpcMatch            npc;
        Condition           when;
        Text                label;
        dialog::ChoiceColor color = dialog::ChoiceColor::White;
        SceneFn             on_select;
    };

    struct Takeover {
        NpcMatch npc;
        Retail   behaviour;
    };

    const std::string &Id() const { return id; }

    const std::vector<std::string> &Stages() const { return stages; }

    const std::string &FirstStage() const { return stages.front(); }

    bool HasStage(std::string_view stage) const;

    const std::vector<Rule> &Rules() const { return rules; }

    const std::vector<Option> &Options() const { return options; }

    const std::vector<Takeover> &Takeovers() const { return takeovers; }

private:
    friend class Builder;

    std::string              id;
    std::vector<std::string> stages;
    std::vector<Rule>        rules;
    std::vector<Option>      options;
    std::vector<Takeover>    takeovers;
};

/**
 * Puts a quest together:
 *
 *   quest::Builder("lost_ring")
 *       .Stages({"hidden", "offered", "asked", "done"})
 *       .Advance("hidden", quest::FloorReached(0), "offered")
 *       .Option(villager, quest::AtStage("offered"), {"quest.lost_ring.ask", "Lost ring"}, Tell)
 *       .Build();
 */
class Builder {
public:
    explicit Builder(std::string id);

    /**
     * The quest's stages; it starts at the first.
     */
    Builder &Stages(std::initializer_list<std::string_view> stages);
    /**
     * A rule: at stage from, once when holds, the quest moves to stage to.
     */
    Builder &Advance(std::string_view from, Condition when, std::string_view to);
    /**
     * A choice the quest adds to the menu of the NPCs npc matches, while when holds.
     */
    Builder &Option(NpcMatch npc, Condition when, Text label, SceneFn on_select,
                    dialog::ChoiceColor color = dialog::ChoiceColor::White);
    /**
     * Holds retail's own behaviour back for the NPCs npc matches: the quest does it instead.
     */
    Builder &TakesOver(NpcMatch npc, Retail behaviour);

    /**
     * The quest; throws std::logic_error for one with no stages, or a rule or stage name that is not
     * one of its stages.
     */
    Definition Build();

private:
    Definition quest;
};

/**
 * Adds a quest to the ones the game knows, at its first stage or as a save already read keeps it.
 */
void Register(Definition quest);
/**
 * Forgets every quest and its progress (for tests).
 */
void              Clear();
const Definition *Find(std::string_view id);
const Progress   *FindProgress(std::string_view id);

/**
 * The game quests act on; until it is set, rules and choices do nothing.
 */
void   SetGame(IGame *game);
IGame *Game();

/**
 * Applies the rules until none moves a quest on.
 */
void Evaluate();
/**
 * The choices quests add for an NPC, after Evaluate: a Dialog option provider.
 */
void CollectOptions(const dialog::NpcContext &npc, std::vector<dialog::ChoiceItem> &out);
/**
 * Whether a registered quest takes behaviour over from retail for an NPC.
 */
bool TakesOver(const dialog::NpcContext &npc, Retail behaviour);

/**
 * A new game: every quest at its first stage, with no numbers.
 */
void ResetProgress();
/**
 * The progress of every quest, as JSON for the save's folder. A quest the save had that is no
 * longer registered (a mod taken out) is kept and written back as it was.
 */
std::string SaveJson();
/**
 * Progress from SaveJson's text; false, with every quest reset, for text that is not that.
 */
bool LoadJson(std::string_view json);

} // namespace dc::quest
