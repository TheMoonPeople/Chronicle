#include "quest_framework.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstdio>
#include <stdexcept>
#include <utility>

#include "localize.hpp"

namespace Quest {
namespace {

using json = nlohmann::json;

constexpr int kSaveVersion = 1;
// Rules that keep moving each other on stop after this many passes rather than hang the game.
constexpr int kMaxEvaluatePasses = 32;

struct Entry {
    Definition quest;
    Progress   progress;
};

struct State {
    // Owned one by one, so the Definition and Progress a Scene holds stay where they are.
    std::vector<std::unique_ptr<Entry>> quests;
    // Progress a save had for quests that are not registered, kept to write back.
    std::map<std::string, json> orphans;
    IGame                      *game = nullptr;
};

State &Get() {
    static State state;
    return state;
}

Entry *FindEntry(std::string_view id) {
    for (const std::unique_ptr<Entry> &entry : Get().quests) {
        if (entry->quest.Id() == id) {
            return entry.get();
        }
    }
    return nullptr;
}

Progress Fresh(const Definition &quest) {
    Progress progress;
    progress.stage = quest.FirstStage();
    return progress;
}

std::string Localized(Text text) {
    return LocalizeText(text.key, text.english);
}

// Runs a quest's choice: a Scene of the quest as it is now. What the choice changed can change what
// the NPC's menu offers, so the menu is asked for again after it.
void RunScene(const std::string &quest_id, const SceneFn &on_select, Dialog::ISessionController &session) {
    Entry *entry = FindEntry(quest_id);
    IGame *game = Get().game;
    if (entry == nullptr || game == nullptr || !on_select) {
        session.Close();
        return;
    }
    Scene scene(entry->quest, entry->progress, *game, session);
    on_select(scene);
    session.RefreshHub();
}

Dialog::ChoiceItem MakeChoice(const std::string &quest_id, std::string id, Text label, SceneFn on_select,
                              Dialog::ChoiceColor color) {
    Dialog::ChoiceItem item;
    item.id = std::move(id);
    item.fallback_label = Localized(label);
    item.color = color;
    item.on_select = [quest_id, on_select = std::move(on_select)](Dialog::ISessionController &session) {
        RunScene(quest_id, on_select, session);
    };
    return item;
}

json ProgressJson(const Progress &progress) {
    json vars = json::object();
    for (const auto &[name, value] : progress.vars) {
        vars[name] = value;
    }
    return json{
        {"stage", progress.stage},
        {"vars",  vars          }
    };
}

Progress ProgressFrom(const Definition &quest, const json &saved) {
    Progress progress = Fresh(quest);
    if (!saved.is_object()) {
        return progress;
    }
    if (auto stage = saved.find("stage"); stage != saved.end() && stage->is_string()) {
        const std::string name = stage->get<std::string>();
        if (quest.HasStage(name)) {
            progress.stage = name;
        } else {
            std::fprintf(stderr, "quests: %s has no stage \"%s\"; starting it again\n", quest.Id().c_str(),
                         name.c_str());
        }
    }
    if (auto vars = saved.find("vars"); vars != saved.end() && vars->is_object()) {
        for (const auto &[name, value] : vars->items()) {
            if (value.is_number_integer()) {
                progress.vars[name] = value.get<int>();
            }
        }
    }
    return progress;
}

} // namespace

// ---- Context and Scene ----

Context::Context(const Definition &quest, Progress &progress, IGame &game)
    : quest(&quest), progress(&progress), game(&game) {}

const std::string &Context::QuestId() const { return quest->Id(); }

IGame &Context::Game() const { return *game; }

const std::string &Context::Stage() const { return progress->stage; }

bool Context::AtStage(std::string_view stage) const { return progress->stage == stage; }

void Context::SetStage(std::string_view stage) {
    if (!quest->HasStage(stage)) {
        std::fprintf(stderr, "quests: %s has no stage \"%.*s\"\n", quest->Id().c_str(),
                     static_cast<int>(stage.size()), stage.data());
        return;
    }
    progress->stage = std::string(stage);
}

int Context::Var(std::string_view name) const {
    const auto it = progress->vars.find(std::string(name));
    return it == progress->vars.end() ? 0 : it->second;
}

void Context::SetVar(std::string_view name, int value) { progress->vars[std::string(name)] = value; }

Scene::Scene(const Definition &quest, Progress &progress, IGame &game, Dialog::ISessionController &session)
    : Context(quest, progress, game), session(&session) {}

void Scene::Say(Text text, std::vector<std::string> args) {
    session->PushRawMessage(Localized(text), std::move(args));
}

void Scene::SayTalkLine(int message, Text fallback) {
    std::string line = Game().TalkLine(message);
    session->PushRawMessage(line.empty() ? Localized(fallback) : std::move(line));
}

void Scene::Ask(Text prompt, std::vector<Dialog::ChoiceItem> choices, std::vector<std::string> args) {
    session->PushRawChoicePage(Localized(prompt), std::move(choices), std::move(args));
}

Dialog::ChoiceItem Scene::Choice(Text label, SceneFn on_select, Dialog::ChoiceColor color) const {
    return MakeChoice(QuestId(), QuestId() + "." + std::string(label.key), label, std::move(on_select), color);
}

void Scene::SayThenClose(Text text, std::vector<std::string> args) {
    session->PushClosingMessage(Localized(text), std::move(args));
}

void Scene::Back() { session->PopPage(); }

void Scene::Close() { session->Close(); }

void Scene::OpenShop(ShopSpec shop) {
    Game().OpenShop(std::move(shop));
    session->Close();
}

// ---- Conditions ----

bool NpcMatch::Matches(const Dialog::NpcContext &npc) const {
    return (map_no == kAny || map_no == npc.map_no) && (npc_id == kAny || npc_id == npc.npc_id) &&
           (mes_file_key.empty() || mes_file_key == npc.mes_file_key);
}

Condition AtStage(std::string_view stage) {
    return [stage = std::string(stage)](const Context &ctx) { return ctx.Stage() == stage; };
}

Condition AtStages(std::initializer_list<std::string_view> stages) {
    std::vector<std::string> names(stages.begin(), stages.end());
    return [names = std::move(names)](const Context &ctx) {
        return std::find(names.begin(), names.end(), ctx.Stage()) != names.end();
    };
}

Condition HasItem(int item, int count) {
    return [item, count](const Context &ctx) { return ctx.Game().ItemCount(item) >= count; };
}

Condition HasMoney(int amount) {
    return [amount](const Context &ctx) { return ctx.Game().Money() >= amount; };
}

Condition FloorReached(int dungeon, int floor) {
    return [dungeon, floor](const Context &ctx) { return ctx.Game().DeepestFloor(dungeon) >= floor; };
}

Condition DungeonStarted(int dungeon) {
    return [dungeon](const Context &ctx) { return ctx.Game().DungeonCount(dungeon) > 0; };
}

Condition PartyMember(int chara) {
    return [chara](const Context &ctx) { return ctx.Game().PartySize() > chara; };
}

Condition EventDone(int town, int part) {
    return [town, part](const Context &ctx) { return ctx.Game().PartEventDone(town, part); };
}

Condition GameFlagSet(int flag) {
    return [flag](const Context &ctx) { return ctx.Game().GameFlag(flag); };
}

Condition GameNumberAtLeast(int slot, int value) {
    return [slot, value](const Context &ctx) { return ctx.Game().GameNumber(slot) >= value; };
}

Condition MapFlagSet(int map, int flag) {
    return [map, flag](const Context &ctx) { return ctx.Game().MapFlag(map, flag); };
}

Condition PartPlaced(int town, int part) {
    return [town, part](const Context &ctx) { return ctx.Game().PartPlaced(town, part) > 0; };
}

Condition PartComplete(int town, int part) {
    return [town, part](const Context &ctx) { return ctx.Game().PartComplete(town, part); };
}

Condition RequestMet(int town, int request) {
    return [town, request](const Context &ctx) { return ctx.Game().RequestMet(town, request); };
}

Condition VillagerHome(int town, int villager) {
    return [town, villager](const Context &ctx) { return ctx.Game().VillagerHome(town, villager); };
}

Condition VarAtLeast(std::string_view name, int value) {
    return [name = std::string(name), value](const Context &ctx) { return ctx.Var(name) >= value; };
}

Condition All(std::vector<Condition> conditions) {
    return [conditions = std::move(conditions)](const Context &ctx) {
        return std::all_of(conditions.begin(), conditions.end(), [&](const Condition &c) { return !c || c(ctx); });
    };
}

Condition Any(std::vector<Condition> conditions) {
    return [conditions = std::move(conditions)](const Context &ctx) {
        return std::any_of(conditions.begin(), conditions.end(), [&](const Condition &c) { return c && c(ctx); });
    };
}

Condition Not(Condition condition) {
    return [condition = std::move(condition)](const Context &ctx) { return !condition || !condition(ctx); };
}

// ---- Definition and Builder ----

bool Definition::HasStage(std::string_view stage) const {
    return std::find(stages.begin(), stages.end(), stage) != stages.end();
}

Builder::Builder(std::string id) { quest.id = std::move(id); }

Builder &Builder::Stages(std::initializer_list<std::string_view> stages) {
    quest.stages.assign(stages.begin(), stages.end());
    return *this;
}

Builder &Builder::Advance(std::string_view from, Condition when, std::string_view to) {
    quest.rules.push_back({std::string(from), std::move(when), std::string(to)});
    return *this;
}

Builder &Builder::Option(NpcMatch npc, Condition when, Text label, SceneFn on_select, Dialog::ChoiceColor color) {
    quest.options.push_back({std::move(npc), std::move(when), label, color, std::move(on_select)});
    return *this;
}

Builder &Builder::TakesOver(NpcMatch npc, Retail behaviour) {
    quest.takeovers.push_back({std::move(npc), behaviour});
    return *this;
}

Definition Builder::Build() {
    const std::string &id = quest.id;
    if (id.empty() || quest.stages.empty()) {
        throw std::logic_error("quest \"" + id + "\" needs an id and at least one stage");
    }
    for (size_t i = 0; i < quest.stages.size(); ++i) {
        if (std::find(quest.stages.begin() + i + 1, quest.stages.end(), quest.stages[i]) != quest.stages.end()) {
            throw std::logic_error("quest \"" + id + "\" names stage \"" + quest.stages[i] + "\" twice");
        }
    }
    for (const Definition::Rule &rule : quest.rules) {
        if (!quest.HasStage(rule.from) || !quest.HasStage(rule.to) || !rule.when) {
            throw std::logic_error("quest \"" + id + "\" has a rule from \"" + rule.from + "\" to \"" + rule.to +
                                   "\" that is not between two of its stages");
        }
    }
    for (const Definition::Option &option : quest.options) {
        if (!option.on_select) {
            throw std::logic_error("quest \"" + id + "\" has a choice that does nothing");
        }
    }
    return std::move(quest);
}

// ---- Registry ----

void Register(Definition quest) {
    State &state = Get();
    if (FindEntry(quest.Id()) != nullptr) {
        throw std::logic_error("quest \"" + quest.Id() + "\" is registered twice");
    }
    auto entry = std::make_unique<Entry>();
    entry->progress = Fresh(quest);
    // A save read before the quest was registered keeps what it had for it.
    if (auto orphan = state.orphans.find(quest.Id()); orphan != state.orphans.end()) {
        entry->progress = ProgressFrom(quest, orphan->second);
        state.orphans.erase(orphan);
    }
    entry->quest = std::move(quest);
    state.quests.push_back(std::move(entry));
}

void Clear() {
    State &state = Get();
    state.quests.clear();
    state.orphans.clear();
}

const Definition *Find(std::string_view id) {
    const Entry *entry = FindEntry(id);
    return entry != nullptr ? &entry->quest : nullptr;
}

const Progress *FindProgress(std::string_view id) {
    const Entry *entry = FindEntry(id);
    return entry != nullptr ? &entry->progress : nullptr;
}

void SetGame(IGame *game) { Get().game = game; }

IGame *Game() { return Get().game; }

void Evaluate() {
    State &state = Get();
    if (state.game == nullptr) {
        return;
    }
    for (int pass = 0; pass < kMaxEvaluatePasses; ++pass) {
        bool moved = false;
        for (const std::unique_ptr<Entry> &entry : state.quests) {
            Context ctx(entry->quest, entry->progress, *state.game);
            for (const Definition::Rule &rule : entry->quest.Rules()) {
                if (ctx.AtStage(rule.from) && rule.when(ctx)) {
                    ctx.SetStage(rule.to);
                    moved = true;
                    break;
                }
            }
        }
        if (!moved) {
            return;
        }
    }
    std::fprintf(stderr, "quests: rules still moving after %d passes; stopped\n", kMaxEvaluatePasses);
}

void CollectOptions(const Dialog::NpcContext &npc, std::vector<Dialog::ChoiceItem> &out) {
    State &state = Get();
    if (state.game == nullptr) {
        return;
    }
    Evaluate();
    for (const std::unique_ptr<Entry> &entry : state.quests) {
        Context                                ctx(entry->quest, entry->progress, *state.game);
        const std::vector<Definition::Option> &options = entry->quest.Options();
        for (size_t i = 0; i < options.size(); ++i) {
            const Definition::Option &option = options[i];
            if (option.npc.Matches(npc) && (!option.when || option.when(ctx))) {
                out.push_back(MakeChoice(entry->quest.Id(), entry->quest.Id() + ".option" + std::to_string(i),
                                         option.label, option.on_select, option.color));
            }
        }
    }
}

bool TakesOver(const Dialog::NpcContext &npc, Retail behaviour) {
    for (const std::unique_ptr<Entry> &entry : Get().quests) {
        for (const Definition::Takeover &takeover : entry->quest.Takeovers()) {
            if (takeover.behaviour == behaviour && takeover.npc.Matches(npc)) {
                return true;
            }
        }
    }
    return false;
}

// ---- Saving ----

void ResetProgress() {
    State &state = Get();
    for (const std::unique_ptr<Entry> &entry : state.quests) {
        entry->progress = Fresh(entry->quest);
    }
    state.orphans.clear();
}

std::string SaveJson() {
    const State &state = Get();
    json         quests = json::object();
    for (const auto &[id, saved] : state.orphans) {
        quests[id] = saved;
    }
    for (const std::unique_ptr<Entry> &entry : state.quests) {
        quests[entry->quest.Id()] = ProgressJson(entry->progress);
    }
    return json{
        {"version", kSaveVersion},
        {"quests",  quests      }
    }
        .dump(2);
}

bool LoadJson(std::string_view text) {
    ResetProgress();
    const json saved = json::parse(text, nullptr, false);
    if (saved.is_discarded() || !saved.is_object()) {
        return false;
    }
    const auto version = saved.find("version");
    const auto quests = saved.find("quests");
    if (version == saved.end() || !version->is_number_integer() || version->get<int>() > kSaveVersion ||
        quests == saved.end() || !quests->is_object()) {
        return false;
    }
    State &state = Get();
    for (const auto &[id, progress] : quests->items()) {
        if (Entry *entry = FindEntry(id)) {
            entry->progress = ProgressFrom(entry->quest, progress);
        } else {
            state.orphans[id] = progress;
        }
    }
    return true;
}

} // namespace Quest
