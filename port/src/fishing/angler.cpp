// The fishing session's game side: the angler, the line and the fish, the camera, the sounds and the
// messages, as EdMoveChara's fishing block drove them.

// nlohmann/json (through the framework) before the game headers, whose one-letter macros it cannot stand.
#include "fishing/session.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <format>
#include <optional>
#include <print>
#include <string>
#include <utility>
#include <vector>

#include "camerafollow.hpp"
#include "character.hpp"
#include "clsmes.hpp"
#include "dataalloc.hpp"
#include "dataread.hpp"
#include "ebattle.hpp"
#include "edit.hpp"
#include "editloop.hpp"
#include "effectmacro.hpp"
#include "fish.hpp"
#include "fishing.hpp"
#include "fishing/host.hpp"
#include "dialog/script_question.hpp"
#include "editloop3.hpp"
#include "editmapscript.hpp"
#include "gamepad.hpp"
#include "itemdata.hpp"
#include "main.hpp"
#include "menu_draw.hpp"
#include "menuetc.hpp"
#include "text/gametext.hpp"
#include "ui/framework/surfaces.hpp"
#include "ui/framework/text.hpp"
#include "ui/framework/widgets.hpp"
#include "mainselect.hpp"
#include "item/framework.hpp"
#include "item/host.hpp"
#include "localize/localize.hpp"
#include "mathutil.hpp"
#include "savedata.hpp"
#include "shop.hpp"
#include "snd.hpp"
#include "sound_effect.h"
#include "text/gametext.hpp"

namespace dc::fishing {

using namespace dc::game;

namespace {

// The message a custom species' name is laid over the evaluation window as; the town's files stop
// far below it.
constexpr int kCustomNameMessage = 0x7F00;

int g_ticks = 0; // the sessions' ticks, for the bite stage's deadlines

// DC_FISHING_TRACE prints the session's steps and each fish's actions about the bait.
const bool kTrace = std::getenv("DC_FISHING_TRACE") != nullptr;

template <class... Args>
void Trace(std::format_string<Args...> format, Args &&...args) {
    if (kTrace) {
        std::println(stderr, "fishing: {:6} {}", g_ticks, std::format(format, std::forward<Args>(args)...));
    }
}

const char *ActionName(game::FishAction action) {
    switch (action) {
        case FISH_ACTION_APPROACH_FOOD:
            return "approach";
        case FISH_ACTION_EAT_FOOD:
            return "nibble";
        case FISH_ACTION_BITE_HOOK:
            return "bite";
        case FISH_ACTION_LEAVE:
            return "leave";
        case FISH_ACTION_NONE:
            return "swim";
        default:
            return "held";
    }
}

BiteSession g_bite;

// Fishing's two questions, which every fishing town's script asks in its window 1: the sign's "Fish /
// Exchange FP / Fishing log / Quit fishing" (message 20) and Circle's "Continue fishing / Quit fishing"
// (22, event 0x85). The dialog framework asks them; where the place's fight reads rods (Enhanced),
// Change rod goes between Circle's two: the rods' board, after which the script hears Continue.
constexpr std::array kFishingTowns = {std::string_view("e01"), std::string_view("e02"), std::string_view("e04"),
                                      std::string_view("s09")};
constexpr int        kQuestionWindow = 1;
constexpr int        kSignMessage = 20;
constexpr int        kStandingMessage = 22;
constexpr int        kContinueRow = 0;
constexpr int        kQuitRow = 1;

constexpr const char *kMenuRod = "fishing.menu.rod";
constexpr const char *kMenuRodText = "Change rod";

// Change rod: the rods in the bag on the item board, opened from Circle's event as the town's Square
// opens the bait's, so that fishing goes on as it was once it closes.
void OpenRodBoard() {
    std::vector<int> rods = RodsCarried();
    rods.push_back(-1);
    EdUseItemInit();
    EdSetUseItem(rods.data());
    EdEventMenu(ED_MENU_CHANGE_ROD);
}

// The town's own scripts ask them, a house's never, and Circle's only while fishing (where the place
// is the one being fished).
bool AskFishingQuestion(const dialog::QuestionContext &question, std::vector<dialog::QuestionRow> &rows) {
    if (question.window != kQuestionWindow || question.interior ||
        std::ranges::find(kFishingTowns, question.town) == kFishingTowns.end()) {
        return false;
    }
    if (question.message == kSignMessage) {
        return true;
    }
    if (question.message != kStandingMessage || EdMoveCharaInfo.fishing == 0) {
        return false;
    }
    if (CurrentPlace().fight->ReadsRods()) {
        rows.push_back({.label = localize::LocalizeText(kMenuRod, kMenuRodText),
                        .before = kQuitRow,
                        .answer = kContinueRow,
                        .then = OpenRodBoard});
    }
    return true;
}

// Brisk looks at the water again this often while no fish there likes the bait; longer gaps are a
// roll of nothing, which the trace says.
constexpr int BriskLookAgain = 30;

double Uniform() {
    return static_cast<double>(std::rand()) / (static_cast<double>(RAND_MAX) + 1.0);
}

// The bait on the hook, as FishingStepFish gives it to the fish.
BiteContext HookBait() {
    BiteContext context{.random = Uniform, .rod = RodInUse()};
    if (const BaitInfo *bait = BaitAt(esa_type); bait != nullptr && EsaFrame != nullptr) {
        context.bait = bait->id;
        context.radius = bait->radius;
        context.default_affinity = bait->default_affinity;
        // Debug builds let every fish notice the bait from anywhere.
        if (DebugMode) {
            context.radius = 100000000.0f;
        }
    }
    return context;
}

FishAction ActionOf(game::FishAction action) {
    switch (action) {
        case FISH_ACTION_APPROACH_FOOD:
            return FishAction::Approach;
        case FISH_ACTION_EAT_FOOD:
            return FishAction::Eat;
        case FISH_ACTION_BITE_HOOK:
            return FishAction::Bite;
        case FISH_ACTION_LEAVE:
            return FishAction::Leave;
        default:
            return FishAction::None;
    }
}

game::FishAction ToGame(FishAction action) {
    switch (action) {
        case FishAction::Approach:
            return FISH_ACTION_APPROACH_FOOD;
        case FishAction::Eat:
            return FISH_ACTION_EAT_FOOD;
        case FishAction::Bite:
            return FISH_ACTION_BITE_HOOK;
        case FishAction::Leave:
            return FISH_ACTION_LEAVE;
        case FishAction::None:
            break;
    }
    return FISH_ACTION_NONE;
}

class GameAngler final : public Angler {
public:
    GameAngler(CMainChara &chara, CCameraFollow &camera, float *position) : chara_(chara), camera_(camera), position_(position) {}

    void Motion(int motion, int flags) override { chara_.SetMotion(motion, flags); }

    int CurrentMotion() override { return chara_.motion_no; }

    bool MotionDone() override { return chara_.motion_state == 3; }

    void HoldStill() override { chara_mode = 1; }

    void Turn(float stick) override {
        sceVu0FVECTOR facing;
        chara_.GetRotation(facing);
        facing[1] = AngleInterpolate(facing[1], facing[1] - 0.1f * stick, 0.01f, INTERPOLATE_STEP);
        chara_.SetRotation(facing);
    }

    void Play(Sound sound) override {
        sceVu0FVECTOR float_pos;
        switch (sound) {
            case Sound::Cast:
                SndSePlay(SE_FISHING_CAST, position_, -1.0f, -1.0f);
                break;
            case Sound::Sink:
                FishLineGetUki(float_pos);
                SndSePlay(SE_FISHING_SINK, float_pos, -1.0f, -1.0f);
                break;
            case Sound::Bite:
                SndSePlay(SE_FISHING_BITE, -1, 0);
                break;
            case Sound::Landed:
                SndSePlay(SE_FISHING_LANDED, -1, 0);
                break;
            case Sound::Fanfare:
                SndSPSePlay(0x2F, -1);
                break;
            case Sound::Warning:
                SndSePlay(SE_WARNING, -1, 0);
                break;
            case Sound::Snap:
                SndSePlay(SE_WEAPON_BREAK, -1, 0);
                break;
        }
    }

    void Rumble(int intensity, int ticks) override { GamePad.SetVibration(1, intensity, ticks); }

    void Ripple(float radius) override {
        sceVu0FVECTOR ripple;
        FishLineGetUki(ripple);
        ripple[1] = FishingGetWaterLevel();
        EffectHamon(&EdEffectGroup, ripple, radius);
    }

    void PullFloat(float tension) override { FishPullHook(tension); }

    // Retail let the bite pass with a tug and a rumble much like a nibble's.
    void BiteCue() override {
        sceVu0FVECTOR ripple;
        FishLineGetUki(ripple);
        ripple[1] = FishingGetWaterLevel();
        SndSePlay(SE_FISHING_SPLASH, ripple, -1.0f, -1.0f);
        EffectHamon(&EdEffectGroup, ripple, 10.0f);
        EffectHamon(&EdEffectGroup, ripple, 15.0f);
        EffectHamon(&EdEffectGroup, ripple, 20.0f);
        GamePad.SetVibration(1, 255, 20);
    }

    void Help(bool waiting) override {
        if (waiting) {
            EdFishingAngleHelpMEs(FishingGetEsaItemNo());
        } else {
            EdFishingWalkHelpMes(FishingGetEsaItemNo());
        }
    }

    void OpenMenu(int event) override {
        EdMoveCharaInfo.event_no = event;
        EdMoveCharaInfo.system_event_no = -1;
        EdMoveCharaInfo.acted = true;
    }

    // The camera watches the point halfway between the angler and the float.
    void FollowLine() override {
        sceVu0FVECTOR centre;
        FishLineGetUki(centre);
        sceVu0AddVector(centre, centre, position_);
        sceVu0ScaleVector(centre, centre, 0.5f);
        camera_.SetFollow(centre[0], centre[1], centre[2]);
    }

    void CatchCamera(float distance) override {
        sceVu0FVECTOR facing;
        chara_.GetRotation(facing);
        camera_.SetAngle(AngleLimit(facing[1]));
        camera_.SetHeight(10.0f);
        camera_.SetDistance(distance);
        camera_.Step(-1);
    }

    void FadeMusic(bool out) override {
        static int volume = 0;
        if (out) {
            volume = SndGetBgmVol();
            SndBgmFadeOut(60, 0);
        } else {
            SndBgmFadeIn(60, volume, 0);
        }
    }

    bool FloatCaught() override { return FishingCheckUkiHook() != 0; }

    bool HookLow() override {
        sceVu0FVECTOR hook;
        FishLineGetHook(hook);
        return hook[1] < 5.0f + FishingGetWaterLevel();
    }

    void LoseBait() override {
        if (const BaitInfo *bait = BaitAt(esa_type); bait != nullptr && bait->lure) {
            return;
        }
        EdFishingLostEsaMes();
        FishingDeleteEsa();
    }

    // Even a lure comes off the hook: the landed fish's model goes where the script built the bait's
    // (_CLEAR_EVENT_BUFF puts the event buffer at the villagers' buffer's free end, as LandFish does).
    void UseBait() override {
        if (const BaitInfo *bait = BaitAt(esa_type); bait != nullptr && bait->lure) {
            EdGetItem(bait->item, 1, -1);
        }
        FishingDeleteEsa();
    }

    void ResetFish() override { FishingInitFishStatus(); }

    void RestartBite() override {
        g_bite = {};
        Trace("the cast starts over");
        // A stage that chose a fish for the cast sends it away now, not when its nibble runs out, so
        // that it never nibbles on beside a fish the new roll chose.
        if (CurrentPlace().bite->ChoosesForTheCast()) {
            for (int place = 0; Fish != nullptr && place < kMaxFish; place++) {
                if (Fish[place].action == FISH_ACTION_APPROACH_FOOD || Fish[place].action == FISH_ACTION_EAT_FOOD ||
                    Fish[place].action == FISH_ACTION_BITE_HOOK) {
                    Fish[place].action_timer = -1;
                }
            }
        }
    }

    void WatchFish(int fish_no) override { FishingAngleFish(fish_no); }

    void HookFish(int fish_no) override {
        FishingBattleFish(fish_no);
        char *file = GetFishFileName(FishingFishKind(fish_no));
        if (file != nullptr) {
            StartReadBG();
            LoadFileBG(file, reinterpret_cast<u_long128 *>(read_buffer), nullptr);
        }
    }

    bool ModelReady() override { return ReadBGSync() == 0; }

    float HookedSize() override {
        const CFish *fish = FishingGetBattleFish();
        return fish != nullptr ? fish->size : 0.0f;
    }

    float FishDistance() override {
        sceVu0FVECTOR at;
        if (CFish *fish = FishingGetBattleFish()) {
            fish->GetPosition(at);
        } else {
            FishLineGetUki(at);
        }
        return std::hypot(at[0] - position_[0], at[2] - position_[2]);
    }

    void ReleaseFish() override {
        FishingInitFishStatus();
        FishingBattleFish(-1);
    }

    // As CFish::SetScale scales the models from the size.
    void GrowHooked(float factor) override {
        if (CFish *fish = FishingGetBattleFish()) {
            fish->size *= factor;
            fish->angle_model_scale = fish->size / fish->info.model_size;
            fish->model_scale = fish->size / 25.0f;
        }
    }

    item::Rod RodInUse() override { return fishing::RodInUse(); }

    // The angling model goes in the free end of the villagers' buffer.
    void LandFish() override {
        BG_READ_INFO  *read_info = GetReadBGFile(0);
        u_int         *fish_data = nullptr;
        CDataAlloc2<1> arena(-1);
        if (read_info != nullptr) {
            fish_data = reinterpret_cast<u_int *>(read_info->buffer);
            int free_blocks = EdVillagerBuffer.limit - EdVillagerBuffer.used;
            arena.base = EdVillagerBuffer.base + static_cast<std::ptrdiff_t>(EdVillagerBuffer.used) * 16;
            arena.limit = free_blocks;
            arena.used = 0;
        }
        FishingBattleToAngleFish(fish_data, &arena);
    }

    void PutAwayFish() override { FishingDeleteAngleFish(); }

    std::optional<Catch> Caught() override {
        int       size = 0;
        int       points = 0;
        const int kind = FishingGetAngleFishSize(&size, &points);
        if (kind < 0) {
            return std::nullopt;
        }
        return Catch{.species = SpeciesOfKind(kind), .size = size, .points = points};
    }

    bool MessageDone() override {
        if (!message_shown) {
            return true;
        }
        if (EdMenuLoop(&EditMes1) == 0) {
            return false;
        }
        EditMes1.auto_pos = MES_POS_TOP_LEFT;
        ClsMesSetMessageOverlay(nullptr, nullptr);
        message_shown = false;
        return true;
    }

    const Species *FishSpecies(int fish_no) override {
        return Fish != nullptr && fish_no >= 0 && fish_no < kMaxFish ? SpeciesOfKind(Fish[fish_no].fish_kind) : nullptr;
    }

    const Species *NibblingSpecies() override {
        for (int fish_no = 0; Fish != nullptr && fish_no < kMaxFish; fish_no++) {
            if (Fish[fish_no].action == FISH_ACTION_EAT_FOOD) {
                return SpeciesOfKind(Fish[fish_no].fish_kind);
            }
        }
        return nullptr;
    }

    TimeOfDay Time() override { return TimeOfDayNow(); }

    const Place &CurrentPlace() override { return fishing::CurrentPlace(); }

    BiteContext Bait() override { return HookBait(); }

    OutcomeContext Outcome() override {
        return {.add_points = [](int points) { SaveData->AddFishingPoint(points); },
                .record =
                    [](const Catch &fish) {
                        // fish_data names a retail kind; a custom species' records are chronicle.dat's.
                        if (fish.species != nullptr && fish.species->IsCustom()) {
                            RecordCatch(fish);
                        } else if (fish.species != nullptr) {
                            SaveData->SetFishingRank(fish.species->kind, static_cast<float>(fish.size));
                        }
                    },
                .count_mardan_garayan =
                    [] {
                        SaveData->CountMardanGarayan();
                        SetFishMardanGarayanNum(1);
                    },
                .show =
                    [](const Catch &fish) {
                        EditMes1.mes_no[0] = GetFishMsgNo(fish.species != nullptr ? fish.species->kind : -1);
                        // A custom species' name has no message of the town's: it is laid over the window.
                        if (fish.species != nullptr && fish.species->IsCustom()) {
                            static text::GameTextFile names;
                            names.Set(kCustomNameMessage, SpeciesName(*fish.species, localize::LocalizeLanguageFile()));
                            ClsMesSetMessageOverlay(&EditMes1, names.Data());
                            EditMes1.mes_no[0] = kCustomNameMessage;
                        }
                        EditMes1.values[0] = fish.size;
                        EditMes1.values[1] = fish.points;
                        EditMes1.values[2] = SaveData->GetFishingPoint();
                        EditMes1.tail_on = false;
                        EditMes1.auto_pos = MES_POS_TOP_LEFT;
                        EditMes1.MakeMesWin(2000);
                        EditMes1.auto_pos = MES_POS_TOP_LEFT;
                        int window[4];
                        EditMes1.AutoSet(window);
                        message_shown = true;
                    },
                .keep = Keep};
    }

    double Random() override { return Uniform(); }

    // A kept fish is a copy of its species' item, its size in the copy's state where the item keeps one,
    // given as an event's gift is: a full bag overflows, as retail's does.
    static bool Keep(const Catch &fish) {
        const item::Def *def = fish.species != nullptr && !fish.species->item.empty() ? item::Find(fish.species->item) : nullptr;
        if (def == nullptr) {
            return false;
        }
        int number = def->number;
        if (!def->state.empty()) {
            number = item::NewCopy(*def);
            if (number < 0) {
                return false;
            }
            if (item::Copy *copy = item::FindCopy(number); copy != nullptr && copy->state.contains("size")) {
                copy->state["size"] = fish.size / 10.0;
            }
        }
        EdGetItem(number, 1, -1);
        return true;
    }

private:
    CMainChara    &chara_;
    CCameraFollow &camera_;
    float         *position_;

    // Whether the evaluation opened its window, which the session waits on.
    static inline bool message_shown = false;
};

Session g_session;

float g_heading = 0.0f; // where the steered fish faces

// Retail's line link, which a fight that reels the line in shortens.
constexpr float kRetailLink = 1.6666666f;

// A fight that steers holds the hooked fish at the line's end, along its heading from the angler's
// facing, and the line's links shorten with it as it is reeled in.
void Steer(CMainChara &chara, const float *position) {
    const Fight    *fight = g_session.CurrentFight();
    const FightView view = fight != nullptr ? fight->View() : FightView{};
    CFish          *fish = FishingGetBattleFish();
    if (!view.steers || fish == nullptr) {
        distp = kRetailLink;
        return;
    }
    sceVu0FVECTOR facing;
    sceVu0FVECTOR at;
    chara.GetRotation(facing);
    fish->GetPosition(at);
    g_heading = AngleLimit(facing[1] + view.heading);
    // Eased there, so the fish doesn't jump as the fight takes it.
    at[0] += 0.2f * (position[0] + view.line * sinf(g_heading) - at[0]);
    at[2] += 0.2f * (position[2] + view.line * cosf(g_heading) - at[2]);
    fish->SetPosition(at);
    distp = std::max(view.line, 1.0f) / 23.0f;
}

} // namespace

void StepSession(CMainChara &chara, CCameraFollow &camera, float *position, float &float_weight, float &hook_weight) {
    // The game starts a session over by setting chara_fishing (EdMoveCharaInit, leaving fishing).
    if (chara_fishing == ED_FISHING_INIT && g_session.CurrentStep() != Step::Init) {
        g_session.Reset();
    }
    GameAngler angler(chara, camera, position);
    int          fish_no = -1;
    const Input  input{.cross = EdPadDown(0x40, 0xFFFF) != 0,
                       .cross_held = EdPadOn(0x40, 0xFFFF) != 0,
                       .circle = EdPadDown(0x20, 0xFFFF) != 0,
                       .square = EdPadDown(0x80, 0xFFFF) != 0,
                       .stick_x = GamePad.GetLXf(),
                       .stick_y = -GamePad.GetLYf(),
                       .fish_status = FishingFishStatus(&fish_no),
                       .fish_no = fish_no,
                       .debug = DebugMode != 0};
    g_ticks++;
    const Step before = g_session.CurrentStep();
    g_session.Tick(angler, input);
    if (g_session.CurrentStep() != before) {
        Trace("step {} -> {} (status {}, cross {})", static_cast<int>(before), static_cast<int>(g_session.CurrentStep()), input.fish_status,
              input.cross);
    }
    chara_fishing = static_cast<int>(g_session.CurrentStep());
    Steer(chara, position);
    float_weight = g_session.FloatWeight();
    hook_weight = g_session.HookWeight();
}

// Retail's ANGLE and BATTLE actions are the session's; the rest are the bite stage's.
void UpdateFishAction(CFish &fish) {
    if (fish.action != FISH_ACTION_NONE && fish.action != FISH_ACTION_APPROACH_FOOD && fish.action != FISH_ACTION_EAT_FOOD &&
        fish.action != FISH_ACTION_BITE_HOOK && fish.action != FISH_ACTION_LEAVE) {
        fish.action_timer = 20 + std::rand() % 30;
        return;
    }
    sceVu0FVECTOR position;
    fish.GetPosition(position);
    BiteContext context{.random = Uniform, .fish_index = static_cast<int>(&fish - Fish), .session = &g_bite, .now = g_ticks, .rod = RodInUse()};
    if (const BaitInfo *bait = BaitAt(fish.food_kind)) {
        context.bait = bait->id;
        context.radius = fish.food_radius;
        context.default_affinity = bait->default_affinity;
    }
    for (int place = 0; Fish != nullptr && place < kMaxFish; place++) {
        sceVu0FVECTOR at;
        Fish[place].GetPosition(at);
        context.fish.push_back(Fish[place].fish_kind >= 0 ? SpeciesOfKind(Fish[place].fish_kind) : nullptr);
        context.distances.push_back(DistVector(Fish[place].food_position, at));
    }
    const Step step = g_session.CurrentStep();
    context.waiting = step == Step::WaitBite || step == Step::BiteCheck || step == Step::Hooked;
    const FishView   view{.species = SpeciesOfKind(fish.fish_kind),
                          .action = ActionOf(fish.action),
                          .distance = DistVector(fish.food_position, position)};
    const int        chosen = g_bite.chosen;
    const int        until = g_bite.until;
    const ActionStep next = CurrentPlace().bite->Update(view, context);
    if (g_bite.chosen != chosen) {
        Trace("fish {} chosen", g_bite.chosen);
    } else if (g_bite.chosen < 0 && g_bite.until != until && g_bite.until - g_ticks > BriskLookAgain) {
        Trace("no fish comes; the next roll at {}", g_bite.until);
    }
    if (ToGame(next.action) != fish.action) {
        Trace("fish {} ({}) {} -> {} for {} ticks, {:.1f} from the bait", context.fish_index, view.species != nullptr ? view.species->id : "none",
              ActionName(fish.action), ActionName(ToGame(next.action)), next.ticks, view.distance);
    }
    fish.action = ToGame(next.action);
    // Retail counts the timer down past 0 before asking again, and the tick asked counts too.
    fish.action_timer = next.ticks - 1;
}

bool FightSteers() {
    const Fight *fight = g_session.CurrentFight();
    return fight != nullptr && fight->View().steers;
}

float BattleHeading() {
    return g_heading;
}

namespace {

constexpr const char *kTension = "fishing.fight.tension";
constexpr const char *kTensionText = "Tension";
constexpr const char *kLine = "fishing.fight.line";
constexpr const char *kLineText = "Line";
constexpr const char *kHit = "fishing.fight.hit";
constexpr const char *kHitText = "HIT!";

} // namespace

std::vector<std::pair<std::string, std::string>> Strings() {
    return {
        {kTension,     kTensionText    },
        {kLine,        kLineText       },
        {kHit,         kHitText        },
        {kMenuRod,     kMenuRodText    },
    };
}

void RegisterQuestions() {
    static bool registered = false;
    if (!std::exchange(registered, true)) {
        dialog::RegisterQuestionProvider(AskFishingQuestion);
    }
}

// Dark Cloud 1 has no art for a fight, so the gauges are the port's: a paper plate at the foot of the
// screen with the tension, shading toward red, and the line still to reel in.
void DrawFight() {
    const Fight *fight = g_session.CurrentFight();
    if (fight == nullptr || g_session.CurrentStep() != Step::Battle) {
        return;
    }
    const FightView view = fight->View();
    if (!view.steers) {
        return;
    }
    ui::TrackerWindow::DrawPaper(ui::Rect(190, 372, 260, 76));
    ui::ProgressBarStyle tension;
    const float          hot = std::clamp((view.tension - 0.4f) / 0.5f, 0.0f, 1.0f);
    tension.fill_r = static_cast<u8>(28 + hot * 150);
    tension.fill_g = static_cast<u8>(68 - hot * 50);
    tension.fill_b = static_cast<u8>(45 - hot * 25);
    tension.hi_r = static_cast<u8>(45 + hot * 170);
    tension.hi_g = static_cast<u8>(95 - hot * 60);
    tension.hi_b = static_cast<u8>(65 - hot * 35);
    tension.done_r = 200;
    tension.done_g = 30;
    tension.done_b = 20;
    ui::ProgressBar::Draw(ui::Rect(205, 384, 230, 22), view.tension, localize::LocalizeText(kTension, kTensionText), tension);
    const float span = std::max(view.start - view.landing, 1.0f);
    ui::ProgressBar::Draw(ui::Rect(205, 414, 230, 22), std::clamp((view.line - view.landing) / span, 0.0f, 1.0f),
                          localize::LocalizeText(kLine, kLineText));
    ui::TextStyle big{.scale = 2.0f, .weight = ui::FontWeight::Bold};
    if (g_session.FightTicks() < 60) {
        ui::DrawTextBox(localize::LocalizeText(kHit, kHitText), ui::Rect(220, 150, 200, 60), big);
    }
    if (view.chance != 0) {
        // The side the fish veers to, where the stick pays.
        ui::DrawTextBox(view.chance > 0 ? ">>>" : "<<<", ui::Rect(view.chance > 0 ? 470 : 70, 220, 100, 60), big);
    }
}

float ApproachSpeed(const CFish &fish) {
    const bool chosen = Fish != nullptr && &fish - Fish == g_bite.chosen && g_bite.speed > 0.0f;
    return chosen ? g_bite.speed : 0.1f;
}

} // namespace dc::fishing
