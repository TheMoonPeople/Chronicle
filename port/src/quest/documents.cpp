// nlohmann/json before the game headers, whose one-letter macros it cannot stand.
#include "quest/documents.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <deque>
#include <format>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <ranges>
#include <stdexcept>
#include <string_view>

#include "data/data_read.hpp"
#include "fishing/quests.hpp"
#include "item/framework.hpp"
#include "item/host.hpp"
#include "localize/localize.hpp"
#include "platform/datapacks.hpp"
#include "platform/native_config.hpp"
#include "platform/vfs.hpp"
#include "quest/framework.hpp"

namespace dc::quest {

using namespace dc::platform;

namespace {

using Json = nlohmann::json;

constexpr std::string_view kFolder = "gameplay/quests/";
constexpr std::string_view kKind = "quest";

[[noreturn]] void Fail(const std::string &where, std::string_view message) {
    throw std::runtime_error(std::format("{}: {}", where, message));
}

// A document's text, held by every function of its quest that shows any: quest::Text only views it.
struct Texts {
    std::map<std::string, Text, std::less<>> by_key;
    std::deque<std::string>                  owned;
};

// A page a choice says. The scene's pages are a stack, the last pushed read first, so a choice's pages
// are kept until its steps have run and pushed then, last first: a document lists them as they are read.
struct Page {
    enum class Kind : uint8_t {
        Say,
        SayThenClose,
        Ask
    };
    Kind                            kind;
    Text                            text;
    std::vector<std::string>        args;
    std::vector<dialog::ChoiceItem> choices;
};

class Runner;
using Step = std::function<void(Runner &)>;
using Steps = std::vector<Step>;
using Arg = std::function<std::string(Scene &)>;

class Runner {
public:
    explicit Runner(Scene &scene) : scene(scene) {}

    void Run(const Steps &steps) {
        for (const Step &step : steps) {
            if (ended) {
                return;
            }
            step(*this);
        }
    }

    void Push(Page page) { pages.push_back(std::move(page)); }

    void Flush() {
        for (Page &page : std::views::reverse(pages)) {
            switch (page.kind) {
                case Page::Kind::Say:
                    scene.Say(page.text, std::move(page.args));
                    break;
                case Page::Kind::SayThenClose:
                    scene.SayThenClose(page.text, std::move(page.args));
                    break;
                case Page::Kind::Ask:
                    scene.Ask(page.text, std::move(page.choices), std::move(page.args));
                    break;
            }
        }
        pages.clear();
    }

    // back, close and open_shop end the choice: its pages first, then nothing after.
    void End(const std::function<void(Scene &)> &ending) {
        Flush();
        ending(scene);
        ended = true;
    }

    Scene &scene;

private:
    std::vector<Page> pages;
    bool              ended = false;
};

SceneFn Play(std::shared_ptr<const Texts> texts, std::shared_ptr<const Steps> steps) {
    return [texts = std::move(texts), steps = std::move(steps)](Scene &scene) {
        Runner runner(scene);
        runner.Run(*steps);
        runner.Flush();
    };
}

int Number(std::string_view id) {
    const item::Def *item = item::Find(id);
    return item != nullptr ? item->number : -1;
}

constexpr std::array<std::string_view, 5>                                 kTowns = {"norune", "matataki", "queens", "muska_lacka", "yellow_drops"};
constexpr std::array<std::string_view, 6>                                 kCharacters = {"toan", "xiao", "goro", "ruby", "ungaga", "osmond"};
constexpr std::array<std::pair<std::string_view, dialog::ChoiceColor>, 7> kColors = {
    {
     {"white", dialog::ChoiceColor::White},
     {"yellow", dialog::ChoiceColor::Yellow},
     {"cyan", dialog::ChoiceColor::Cyan},
     {"green", dialog::ChoiceColor::Green},
     {"gold", dialog::ChoiceColor::Gold},
     {"grey", dialog::ChoiceColor::Grey},
     {"highlight", dialog::ChoiceColor::Highlight},
     }
};
constexpr std::array<std::pair<std::string_view, ShopLine>, 8> kShopLines = {
    {
     {"greeting", ShopLine::Greeting},
     {"browsing", ShopLine::Browsing},
     {"selling", ShopLine::Selling},
     {"not_enough_money", ShopLine::NotEnoughMoney},
     {"no_room", ShopLine::NoRoom},
     {"confirm", ShopLine::Confirm},
     {"thanks", ShopLine::Thanks},
     {"goodbye", ShopLine::Goodbye},
     }
};

// Reads one document into a quest, checking everything it names as it goes.
class Parser {
public:
    Parser(const Json &document, std::optional<std::string_view> file) : document(document) {
        if (!document.is_object()) {
            Fail("quest", "not an object");
        }
        id = String(Member(document, "id", "quest"), "id");
        if (id.empty() || !std::ranges::all_of(id, [](char c) {
                return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == ':';
            })) {
            Fail("id", "lower-case letters, digits, _ and : only");
        }
        if (file) {
            std::string expected = id;
            std::ranges::replace(expected, ':', '.');
            if (vfs::FoldKey(*file) != vfs::FoldKey(expected + ".json")) {
                Fail("id", std::format("{} belongs in {}.json", id, expected));
            }
        }
        ReadTexts();
        for (const Json &stage : Array(document, "stages", "quest")) {
            stages.push_back(String(stage, "stages"));
        }
    }

    Definition Build() {
        Builder builder(id);
        builder.Stages(stages);
        if (const Json *rules = Optional(document, "rules")) {
            for (size_t index = 0; index < ArrayOf(*rules, "rules").size(); index++) {
                const std::string where = std::format("rules[{}]", index);
                const Json       &rule = Object((*rules)[index], where, {"from", "when", "to"});
                builder.Advance(Stage(Member(rule, "from", where), where + ".from"),
                                ParseCondition(Member(rule, "when", where), where + ".when"),
                                Stage(Member(rule, "to", where), where + ".to"));
            }
        }
        if (const Json *options = Optional(document, "options")) {
            for (size_t index = 0; index < ArrayOf(*options, "options").size(); index++) {
                const std::string where = std::format("options[{}]", index);
                const Json       &option = Object((*options)[index], where, {"npc", "when", "label", "color", "steps"});
                const Json       *when = Optional(option, "when");
                builder.Option(ParseNpc(Member(option, "npc", where), where + ".npc"),
                               when != nullptr ? ParseCondition(*when, where + ".when") : Condition{},
                               TextAt(Member(option, "label", where), where + ".label"),
                               Play(texts, std::make_shared<const Steps>(ParseSteps(Member(option, "steps", where), where + ".steps"))),
                               Color(option, where));
            }
        }
        if (const Json *counters = Optional(document, "counters")) {
            for (size_t index = 0; index < ArrayOf(*counters, "counters").size(); index++) {
                ParseCounter((*counters)[index], std::format("counters[{}]", index));
            }
        }
        try {
            return builder.Build();
        } catch (const std::logic_error &error) {
            Fail("quest", error.what());
        }
    }

    // What Build found that this version does not read, outside the steps (which must all be known).
    std::vector<std::string> Unknown() const {
        std::vector<std::string> unknown;
        for (const auto &[key, value] : document.items()) {
            if (!std::ranges::contains(std::array<std::string_view, 8>{"schema_version", "kind", "id", "stages", "rules",
                                                                       "options", "counters", "text"},
                                       key)) {
                unknown.push_back(key);
            }
        }
        return unknown;
    }

    std::vector<std::pair<std::string, std::string>> Strings() const {
        std::vector<std::pair<std::string, std::string>> out;
        for (const Text &text : std::views::values(texts->by_key)) {
            out.emplace_back(text.key, text.english);
        }
        return out;
    }

    const std::string &Id() const { return id; }

    std::vector<std::function<void()>> counters;

private:
    static const Json &Member(const Json &object, const char *key, const std::string &where) {
        const auto found = object.find(key);
        if (found == object.end()) {
            Fail(where, std::format("needs {}", key));
        }
        return *found;
    }

    static const Json *Optional(const Json &object, const char *key) {
        const auto found = object.find(key);
        return found != object.end() ? &*found : nullptr;
    }

    // An object with no members but these.
    static const Json &Object(const Json &value, const std::string &where, std::initializer_list<std::string_view> known) {
        if (!value.is_object()) {
            Fail(where, "not an object");
        }
        for (const auto &[key, member] : value.items()) {
            if (!std::ranges::contains(known, key)) {
                Fail(where, std::format("has {}, which this version does not read", key));
            }
        }
        return value;
    }

    static const Json &ArrayOf(const Json &value, const std::string &where) {
        if (!value.is_array()) {
            Fail(where, "not an array");
        }
        return value;
    }

    static const Json &Array(const Json &object, const char *key, const std::string &where) {
        return ArrayOf(Member(object, key, where), where + "." + key);
    }

    static std::string String(const Json &value, const std::string &where) {
        if (!value.is_string()) {
            Fail(where, "not a string");
        }
        return value.get<std::string>();
    }

    static int Int(const Json &value, const std::string &where) {
        if (!value.is_number_integer()) {
            Fail(where, "not a whole number");
        }
        return value.get<int>();
    }

    static int IntAt(const Json &object, const char *key, const std::string &where) {
        return Int(Member(object, key, where), where + "." + key);
    }

    // A number, or a name for one: a town or a party member.
    template <size_t N>
    static int Named(const Json &value, const std::array<std::string_view, N> &names, const std::string &where) {
        if (value.is_string()) {
            const auto found = std::ranges::find(names, value.get<std::string>());
            if (found == names.end()) {
                Fail(where, std::format("no such name as {}", value.get<std::string>()));
            }
            return static_cast<int>(found - names.begin());
        }
        const int number = Int(value, where);
        if (number < 0 || number >= static_cast<int>(N)) {
            Fail(where, std::format("{} is not 0 to {}", number, N - 1));
        }
        return number;
    }

    static int Town(const Json &object, const std::string &where) {
        return Named(Member(object, "town", where), kTowns, where + ".town");
    }

    void ReadTexts() {
        texts = std::make_shared<Texts>();
        const Json *text = Optional(document, "text");
        if (text == nullptr) {
            return;
        }
        if (!text->is_object()) {
            Fail("text", "not an object");
        }
        for (const auto &[key, english] : text->items()) {
            if (key.empty()) {
                Fail("text", "a text needs a name");
            }
            const std::string &full = texts->owned.emplace_back(std::format("quest.{}.{}", id, key));
            const std::string &words = texts->owned.emplace_back(String(english, "text." + key));
            texts->by_key.emplace(key, Text{.key = full, .english = words});
        }
    }

    std::string Stage(const Json &value, const std::string &where) const {
        std::string stage = String(value, where);
        if (!std::ranges::contains(stages, stage)) {
            Fail(where, std::format("{} is not one of the quest's stages", stage));
        }
        return stage;
    }

    Text TextAt(const Json &value, const std::string &where) const {
        const std::string key = String(value, where);
        const auto        found = texts->by_key.find(key);
        if (found == texts->by_key.end()) {
            Fail(where, std::format("no text called {}", key));
        }
        return found->second;
    }

    static std::string ItemId(const Json &value, const std::string &where) {
        std::string item = String(value, where);
        if (Number(item) < 0) {
            Fail(where, std::format("no item is called {}", item));
        }
        return item;
    }

    dialog::ChoiceColor Color(const Json &object, const std::string &where) const {
        const Json *color = Optional(object, "color");
        if (color == nullptr) {
            return dialog::ChoiceColor::White;
        }
        const std::string name = String(*color, where + ".color");
        const auto        found = std::ranges::find(kColors, name, &std::pair<std::string_view, dialog::ChoiceColor>::first);
        if (found == kColors.end()) {
            Fail(where + ".color", std::format("no such colour as {}", name));
        }
        return found->second;
    }

    NpcMatch ParseNpc(const Json &value, const std::string &where) const {
        const Json &npc = Object(value, where, {"town", "villager", "message_file"});
        NpcMatch    match;
        if (Optional(npc, "town") != nullptr) {
            match.map_no = Town(npc, where);
        }
        if (Optional(npc, "villager") != nullptr) {
            match.npc_id = IntAt(npc, "villager", where);
        }
        if (const Json *file = Optional(npc, "message_file")) {
            match.mes_file_key = String(*file, where + ".message_file");
        }
        return match;
    }

    Condition ParseCondition(const Json &value, const std::string &where) const {
        if (!value.is_object() || value.size() != 1) {
            Fail(where, "a condition is an object with one member");
        }
        const std::string name = value.begin().key();
        const Json       &arg = value.begin().value();
        const std::string at = where + "." + name;
        if (name == "at_stage") {
            return AtStage(Stage(arg, at));
        }
        if (name == "at_stages") {
            std::vector<Condition> any;
            for (const Json &stage : ArrayOf(arg, at)) {
                any.push_back(AtStage(Stage(stage, at)));
            }
            return Any(std::move(any));
        }
        if (name == "has_item") {
            if (arg.is_string()) {
                return HasItem(ItemId(arg, at));
            }
            const Json &item = Object(arg, at, {"item", "count"});
            return HasItem(ItemId(Member(item, "item", at), at + ".item"),
                           Optional(item, "count") != nullptr ? IntAt(item, "count", at) : 1);
        }
        if (name == "has_item_with") {
            const Json *trait = &arg;
            int         count = 1;
            if (arg.is_object()) {
                const Json &object = Object(arg, at, {"trait", "count"});
                trait = &Member(object, "trait", at);
                count = Optional(object, "count") != nullptr ? IntAt(object, "count", at) : 1;
            }
            const std::string trait_name = String(*trait, at);
            if (trait_name == "fish") {
                return HasItemWith<item::Fish>(count);
            }
            if (trait_name == "bait") {
                return HasItemWith<item::Bait>(count);
            }
            if (trait_name == "rod") {
                return HasItemWith<item::Rod>(count);
            }
            if (trait_name == "key_item") {
                return HasItemWith<item::KeyItem>(count);
            }
            if (trait_name == "quest_item") {
                return HasItemWith<item::QuestItem>(count);
            }
            if (trait_name == "dungeon_key") {
                return HasItemWith<item::DungeonKey>(count);
            }
            Fail(at, std::format("no such trait as {}", trait_name));
        }
        if (name == "has_money") {
            return HasMoney(Int(arg, at));
        }
        if (name == "floor_reached") {
            const Json &floor = Object(arg, at, {"dungeon", "floor"});
            return FloorReached(IntAt(floor, "dungeon", at), Optional(floor, "floor") != nullptr ? IntAt(floor, "floor", at) : 0);
        }
        if (name == "dungeon_started") {
            return DungeonStarted(Int(arg, at));
        }
        if (name == "game_flag") {
            return GameFlagSet(Int(arg, at));
        }
        if (name == "game_number_at_least") {
            const Json &number = Object(arg, at, {"slot", "value"});
            return GameNumberAtLeast(IntAt(number, "slot", at), IntAt(number, "value", at));
        }
        if (name == "map_flag") {
            const Json &flag = Object(arg, at, {"map", "flag"});
            return MapFlagSet(IntAt(flag, "map", at), IntAt(flag, "flag", at));
        }
        if (name == "part_placed" || name == "part_complete" || name == "event_done") {
            const Json &part = Object(arg, at, {"town", "part"});
            const int   town = Town(part, at);
            const int   number = IntAt(part, "part", at);
            return name == "part_placed" ? PartPlaced(town, number) : name == "part_complete" ? PartComplete(town, number)
                                                                                              : EventDone(town, number);
        }
        if (name == "request_met") {
            const Json &request = Object(arg, at, {"town", "request"});
            return RequestMet(Town(request, at), IntAt(request, "request", at));
        }
        if (name == "villager_home") {
            const Json &villager = Object(arg, at, {"town", "villager"});
            return VillagerHome(Town(villager, at), IntAt(villager, "villager", at));
        }
        if (name == "party_member") {
            return PartyMember(Named(arg, kCharacters, at));
        }
        if (name == "var_at_least") {
            const Json &var = Object(arg, at, {"var", "value"});
            return VarAtLeast(String(Member(var, "var", at), at + ".var"), IntAt(var, "value", at));
        }
        if (name == "caught_a_fish") {
            if (arg != true) {
                Fail(at, "is true or left out");
            }
            return [](const Context &) { return fishing::HasCaughtAFish(); };
        }
        if (name == "all" || name == "any") {
            std::vector<Condition> conditions;
            for (size_t index = 0; index < ArrayOf(arg, at).size(); index++) {
                conditions.push_back(ParseCondition(arg[index], std::format("{}[{}]", at, index)));
            }
            return name == "all" ? All(std::move(conditions)) : Any(std::move(conditions));
        }
        if (name == "not") {
            return Not(ParseCondition(arg, at));
        }
        Fail(where, std::format("no such condition as {}", name));
    }

    // What fills a text's %1, %2...
    std::vector<Arg> ParseArgs(const Json &step, const std::string &where) const {
        std::vector<Arg> args;
        const Json      *list = Optional(step, "args");
        if (list == nullptr) {
            return args;
        }
        for (size_t index = 0; index < ArrayOf(*list, where + ".args").size(); index++) {
            const std::string at = std::format("{}.args[{}]", where, index);
            const Json       &arg = (*list)[index];
            if (!arg.is_object() || arg.empty()) {
                Fail(at, "not an object");
            }
            if (arg.contains("var")) {
                const std::string var = String(Object(arg, at, {"var"})["var"], at + ".var");
                args.push_back([var](Scene &scene) { return std::to_string(scene.Var(var)); });
            } else if (arg.contains("var_left")) {
                const Json       &left = Object(arg, at, {"var_left", "of"});
                const std::string var = String(left["var_left"], at + ".var_left");
                const int         of = IntAt(left, "of", at);
                args.push_back([var, of](Scene &scene) { return std::to_string(of - scene.Var(var)); });
            } else if (arg.contains("system_text")) {
                const Json       &system = Object(arg, at, {"system_text", "or"});
                const int         message = IntAt(system, "system_text", at);
                const std::string fallback = Optional(system, "or") != nullptr ? String(system["or"], at + ".or") : std::string();
                args.push_back([message, fallback](Scene &scene) {
                    std::string text = scene.Game().SystemText(message);
                    return text.empty() ? fallback : text;
                });
            } else if (arg.contains("item_name")) {
                const std::string item = ItemId(Object(arg, at, {"item_name"})["item_name"], at + ".item_name");
                args.push_back([item](Scene &scene) {
                    const int number = Number(item);
                    if (const item::ItemText *text = item::CustomText(number, localize::LocalizeLanguageFile())) {
                        return text->name;
                    }
                    std::string name = number >= 0 ? scene.Game().SystemText(number + 100) : std::string();
                    return name.empty() ? item : name;
                });
            } else if (arg.contains("item_count")) {
                const std::string item = ItemId(Object(arg, at, {"item_count"})["item_count"], at + ".item_count");
                args.push_back([item](Scene &scene) { return std::to_string(scene.Game().ItemCount(Number(item))); });
            } else {
                Fail(at, std::format("no such argument as {}", arg.begin().key()));
            }
        }
        return args;
    }

    static std::vector<std::string> Fill(const std::vector<Arg> &args, Scene &scene) {
        std::vector<std::string> out;
        for (const Arg &arg : args) {
            out.push_back(arg(scene));
        }
        return out;
    }

    // A step that can fail runs its then steps when it does what it says, its else steps when not.
    std::pair<std::shared_ptr<const Steps>, std::shared_ptr<const Steps>> Branches(const Json        &step,
                                                                                   const std::string &where) const {
        const Json *then = Optional(step, "then");
        const Json *otherwise = Optional(step, "else");
        return {std::make_shared<const Steps>(then != nullptr ? ParseSteps(*then, where + ".then") : Steps{}),
                std::make_shared<const Steps>(otherwise != nullptr ? ParseSteps(*otherwise, where + ".else") : Steps{})};
    }

    static Step Branch(std::function<bool(Scene &)> act, std::pair<std::shared_ptr<const Steps>, std::shared_ptr<const Steps>> branches) {
        return [act = std::move(act), branches = std::move(branches)](Runner &runner) {
            runner.Run(act(runner.scene) ? *branches.first : *branches.second);
        };
    }

    Steps ParseSteps(const Json &value, const std::string &where) const {
        Steps steps;
        for (size_t index = 0; index < ArrayOf(value, where).size(); index++) {
            steps.push_back(ParseStep(value[index], std::format("{}[{}]", where, index)));
        }
        return steps;
    }

    Step ParseStep(const Json &step, const std::string &where) const {
        if (step == "back") {
            return [](Runner &runner) { runner.End([](Scene &scene) { scene.Back(); }); };
        }
        if (step == "close") {
            return [](Runner &runner) { runner.End([](Scene &scene) { scene.Close(); }); };
        }
        if (!step.is_object() || step.empty()) {
            Fail(where, "a step is \"back\", \"close\" or an object");
        }
        if (step.contains("say") || step.contains("say_then_close")) {
            const bool       closes = step.contains("say_then_close");
            const char      *verb = closes ? "say_then_close" : "say";
            const Json      &say = Object(step, where, {verb, "args"});
            const Text       text = TextAt(say[verb], where + "." + verb);
            std::vector<Arg> args = ParseArgs(say, where);
            const Page::Kind kind = closes ? Page::Kind::SayThenClose : Page::Kind::Say;
            return [text, args = std::move(args), kind](Runner &runner) {
                runner.Push({.kind = kind, .text = text, .args = Fill(args, runner.scene), .choices = {}});
            };
        }
        if (step.contains("ask")) {
            const Json      &ask = Object(step, where, {"ask", "args", "choices"});
            const Text       prompt = TextAt(ask["ask"], where + ".ask");
            std::vector<Arg> args = ParseArgs(ask, where);

            struct Choice {
                Text                label;
                dialog::ChoiceColor color;
                SceneFn             on_select;
            };

            std::vector<Choice> choices;
            const Json         &list = Array(ask, "choices", where);
            for (size_t index = 0; index < list.size(); index++) {
                const std::string at = std::format("{}.choices[{}]", where, index);
                const Json       &choice = Object(list[index], at, {"label", "color", "steps"});
                choices.push_back({.label = TextAt(Member(choice, "label", at), at + ".label"),
                                   .color = Color(choice, at),
                                   .on_select = Play(texts, std::make_shared<const Steps>(ParseSteps(Member(choice, "steps", at), at + ".steps")))});
            }
            return [prompt, args = std::move(args), choices = std::move(choices)](Runner &runner) {
                std::vector<dialog::ChoiceItem> items;
                for (const Choice &choice : choices) {
                    items.push_back(runner.scene.Choice(choice.label, choice.on_select, choice.color));
                }
                runner.Push({.kind = Page::Kind::Ask, .text = prompt, .args = Fill(args, runner.scene), .choices = std::move(items)});
            };
        }
        if (step.contains("stage")) {
            const std::string stage = Stage(Object(step, where, {"stage"})["stage"], where + ".stage");
            return [stage](Runner &runner) { runner.scene.SetStage(stage); };
        }
        if (step.contains("set_var") || step.contains("add_var")) {
            const bool        adds = step.contains("add_var");
            const char       *verb = adds ? "add_var" : "set_var";
            const Json       &var = Object(step, where, {verb, "value"});
            const std::string name = String(var[verb], where + "." + verb);
            const int         value = IntAt(var, "value", where);
            return [name, value, adds](Runner &runner) { runner.scene.SetVar(name, (adds ? runner.scene.Var(name) : 0) + value); };
        }
        if (step.contains("give_item") || step.contains("take_item") || step.contains("show_item_received")) {
            const char            *verb = step.contains("give_item") ? "give_item" : step.contains("take_item") ? "take_item"
                                                                                                                : "show_item_received";
            const Json            &act = Object(step, where, {verb, "then", "else"});
            const std::string      item = ItemId(act[verb], where + "." + verb);
            const std::string_view kind = verb;
            return Branch(
                [item, kind](Scene &scene) {
                    const int number = Number(item);
                    if (number < 0) {
                        return false;
                    }
                    return kind == "give_item"   ? scene.Game().GiveItem(number)
                           : kind == "take_item" ? scene.Game().TakeItem(number)
                                                 : scene.Game().ShowItemReceived(number);
                },
                Branches(act, where));
        }
        if (step.contains("give_money")) {
            const int amount = IntAt(Object(step, where, {"give_money"}), "give_money", where);
            return [amount](Runner &runner) { runner.scene.Game().EarnMoney(amount); };
        }
        if (step.contains("take_money")) {
            const Json &take = Object(step, where, {"take_money", "then", "else"});
            const int   amount = IntAt(take, "take_money", where);
            return Branch([amount](Scene &scene) { return scene.Game().SpendMoney(amount); }, Branches(take, where));
        }
        if (step.contains("if")) {
            const Json     &branch = Object(step, where, {"if", "then", "else"});
            const Condition when = ParseCondition(branch["if"], where + ".if");
            return Branch([when](Scene &scene) { return when(scene); }, Branches(branch, where));
        }
        if (step.contains("open_shop")) {
            const ShopSpec shop = ParseShop(Object(step, where, {"open_shop"})["open_shop"], where + ".open_shop");
            return [shop](Runner &runner) { runner.End([&shop](Scene &scene) { scene.OpenShop(shop); }); };
        }
        Fail(where, std::format("no such step as {}", step.begin().key()));
    }

    ShopSpec ParseShop(const Json &value, const std::string &where) const {
        const Json &shop = Object(value, where, {"look_of_shop", "keeper", "goods", "lines"});
        ShopSpec    spec;
        spec.look_of_shop = IntAt(shop, "look_of_shop", where);
        if (const Json *keeper = Optional(shop, "keeper")) {
            const std::string name = String(*keeper, where + ".keeper");
            if (name != "speaker" && name != "retail") {
                Fail(where + ".keeper", "speaker or retail");
            }
            spec.keeper = name == "speaker" ? Shopkeeper::Speaker : Shopkeeper::Retail;
        }
        const Json &goods = Array(shop, "goods", where);
        for (size_t index = 0; index < goods.size(); index++) {
            const std::string at = std::format("{}.goods[{}]", where, index);
            const Json       &good = Object(goods[index], at, {"item", "price"});
            spec.goods.push_back({.item = Number(ItemId(Member(good, "item", at), at + ".item")), .price = IntAt(good, "price", at)});
        }
        if (const Json *lines = Optional(shop, "lines")) {
            if (!lines->is_object()) {
                Fail(where + ".lines", "not an object");
            }
            for (const auto &[name, key] : lines->items()) {
                const auto line = std::ranges::find(kShopLines, name, &std::pair<std::string_view, ShopLine>::first);
                if (line == kShopLines.end()) {
                    Fail(where + ".lines", std::format("no such line as {}", name));
                }
                spec.lines.emplace_back(line->second, TextAt(key, where + ".lines." + name));
            }
        }
        return spec;
    }

    void ParseCounter(const Json &value, const std::string &where) {
        const Json       &counter = Object(value, where, {"count", "var", "species", "place", "bait", "min_size", "while_at"});
        const std::string count = String(Member(counter, "count", where), where + ".count");
        const std::string var = String(Member(counter, "var", where), where + ".var");
        fishing::Stages   while_at;
        if (const Json *stages_at = Optional(counter, "while_at")) {
            for (const Json &stage : ArrayOf(*stages_at, where + ".while_at")) {
                while_at.push_back(Stage(stage, where + ".while_at"));
            }
        }
        if (count == "fish_species") {
            if (Optional(counter, "place") != nullptr || Optional(counter, "bait") != nullptr || Optional(counter, "min_size") != nullptr) {
                Fail(where, "fish_species counts by species alone");
            }
            std::vector<std::string> species;
            for (const Json &kind : Array(counter, "species", where)) {
                species.push_back(String(kind, where + ".species"));
            }
            counters.push_back([quest = id, var, species = std::move(species), while_at = std::move(while_at)] {
                fishing::CountSpecies(quest, var, species, while_at);
            });
            return;
        }
        fishing::CatchFilter filter;
        if (const Json *species = Optional(counter, "species")) {
            filter.species = String(*species, where + ".species");
        }
        if (const Json *place = Optional(counter, "place")) {
            filter.place = String(*place, where + ".place");
        }
        if (const Json *bait = Optional(counter, "bait")) {
            filter.bait = String(*bait, where + ".bait");
        }
        if (Optional(counter, "min_size") != nullptr) {
            filter.min_size = IntAt(counter, "min_size", where);
        }
        if (count == "fish_caught") {
            counters.push_back([quest = id, var, filter, while_at] { fishing::CountCatches(quest, var, filter, while_at); });
        } else if (count == "largest_fish" && while_at.empty()) {
            counters.push_back([quest = id, var, filter] { fishing::KeepLargest(quest, var, filter); });
        } else if (count == "fish_points" && while_at.empty() && filter.species.empty() && filter.place.empty() &&
                   filter.bait.empty() && filter.min_size == 0) {
            counters.push_back([quest = id, var] { fishing::CountFishPoints(quest, var); });
        } else {
            Fail(where, std::format("cannot count {} so", count));
        }
    }

    const Json              &document;
    std::string              id;
    std::vector<std::string> stages;
    std::shared_ptr<Texts>   texts;
};

struct Loaded {
    Definition                                       quest;
    std::vector<std::function<void()>>               counters;
    std::vector<std::pair<std::string, std::string>> strings;
};

std::map<std::string, Loaded> loaded;

} // namespace

void LoadDocuments() {
    loaded.clear();
    std::map<std::string, std::string> files;
    for (const std::string &name : datapacks::Members(kFolder.substr(0, kFolder.size() - 1))) {
        files.try_emplace(vfs::FoldKey(name), name);
    }
    if (data::DataReadIndexed()) {
        for (const std::string &path : data::DataReadListFiles()) {
            if (path.size() <= kFolder.size() || vfs::FoldKey(path.substr(0, kFolder.size())) != vfs::FoldKey(kFolder)) {
                continue;
            }
            const std::string name = path.substr(kFolder.size());
            if (name.find('/') == std::string::npos) {
                files.try_emplace(vfs::FoldKey(name), name);
            }
        }
    }
    for (const std::string &name : std::views::values(files)) {
        native_config::Read(std::format("quests/{}", name), kKind, [&](const Json &document) {
            Parser parser(document, name);
            Loaded quest{.quest = parser.Build(), .counters = std::move(parser.counters), .strings = parser.Strings()};
            if (const std::vector<std::string> unknown = parser.Unknown(); !unknown.empty()) {
                std::string names;
                for (const std::string &field : unknown) {
                    names += names.empty() ? field : ", " + field;
                }
                native_config::Report(std::format("{}{}", kFolder, name), "fields this version does not read: " + names);
            }
            loaded.insert_or_assign(parser.Id(), std::move(quest));
        });
    }
}

void RegisterDocuments() {
    for (const auto &[id, quest] : loaded) {
        try {
            Register(quest.quest);
        } catch (const std::logic_error &error) {
            native_config::Report(std::format("{}{}", kFolder, id), error.what());
            continue;
        }
        for (const std::function<void()> &start : quest.counters) {
            start();
        }
    }
}

void RegisterDocument(const Json &document) {
    Parser     parser(document, std::nullopt);
    Definition quest = parser.Build();
    Register(std::move(quest));
    for (const std::function<void()> &start : parser.counters) {
        start();
    }
}

std::vector<std::pair<std::string, std::string>> DocumentStrings(const Json &document) {
    return Parser(document, std::nullopt).Strings();
}

std::vector<std::pair<std::string, std::string>> DocumentStrings() {
    std::vector<std::pair<std::string, std::string>> out;
    for (const Loaded &quest : std::views::values(loaded)) {
        out.insert(out.end(), quest.strings.begin(), quest.strings.end());
    }
    return out;
}

} // namespace dc::quest
