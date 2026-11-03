#pragma once

#include <filesystem>
#include <map>

#include "item/framework.hpp"
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace dc::game {
struct FishInfo;
class CFish;
class CMainChara;
class CCameraFollow;
} // namespace dc::game

/**
 * The fishing framework's tie to the game: its documents, gameplay/fishing/fish.json, places.json and
 * prizes.json, read as the item documents are, and what retail's fishing code asks of them.
 */
namespace dc::fishing {

struct Place;
struct Species;
struct Catch;
enum class TimeOfDay;

/**
 * Fills the species, places and prizes from the highest-priority usable document, then the data's own,
 * or as compiled. item::Load runs first: species name baits by item ID.
 */
void Load();

/**
 * Writes the documents as compiled under directory/gameplay/fishing/. False when one cannot be
 * written.
 */
bool Export(const std::filesystem::path &directory);

/**
 * A fish kind's row as CFish keeps it, likings in esa_info's order; null for a kind no species is.
 */
const game::FishInfo *Info(int kind);

/**
 * The model archive of a fish kind (GetFishFileName), or null.
 */
char *ModelPath(int kind);

/**
 * The system message naming a fish kind (GetFishMsgNo), 0 for a kind no species is, as retail.
 */
int NameMessage(int kind);

/**
 * Whether a catch of the kind counts toward the Mardan Garayan.
 */
bool CountsAsMardanGarayan(int kind);

/**
 * What fishing at a spot starts with.
 */
struct Spawning {
    std::vector<int> kinds; /**< Each fish's kind; -1 leaves its place empty. */
    bool             draw_under_water = true;
};

/**
 * The fish a spot starts with at a time of day (ED_TIME_*), drawn by its place's spawn stage, which
 * becomes the current place. A spot no place is has five empty places, as retail.
 */
Spawning SpawnAt(int spot, int time_of_day);

/**
 * A custom species' catches, kept in chronicle.dat's FISH chunk: retail's kinds are in save.dat's
 * fish_data. Records of a species whose document is gone are kept and written back.
 */
struct SpeciesRecord {
    int                      caught = 0;
    int                      largest = 0; /**< In the records' units, tenths of a size unit. */
    std::vector<std::string> baits;       /**< Item IDs of the baits it has been caught with. */
};

/**
 * A custom species' record, or null for one never caught.
 */
const SpeciesRecord *RecordOf(std::string_view species);

/**
 * Every custom species' record, by ID.
 */
const std::map<std::string, SpeciesRecord, std::less<>> &AllRecords();

/**
 * Adds a catch of a custom species to its record.
 */
void RecordCatch(const Catch &fish);

/**
 * A custom species' name in a language file (fr_fr), else English, else the first it has.
 */
const std::string &SpeciesName(const Species &species, std::string_view language);

/**
 * The time of day now, as the spawn's intervals know it.
 */
TimeOfDay TimeOfDayNow();

/**
 * The place fishing last started at (SpawnAt), or for a spot no place is, retail's stages.
 */
const Place &CurrentPlace();

/**
 * A bait the hook takes: retail's thirteen in esa_info's order, then each custom item with the Bait
 * trait. esa_type and CFish's food_kind are its index.
 */
struct BaitInfo {
    int         item;
    std::string id;
    float       radius;
    bool        lure; /**< Back to the bag after a catch, never lost. */
    float       default_affinity;
};

/**
 * The bait at an index, or null.
 */
const BaitInfo *BaitAt(int index);

/**
 * The index of the bait an item (or a copy of it) is, or -1.
 */
int BaitIndex(int item);

/**
 * Every bait's item, for the bait picker.
 */
std::vector<int> BaitItems();


/**
 * One tick of the angler's session (fishing/angler.cpp), in place of retail's EdMoveChara block:
 * position is where the angler stands; float_weight and hook_weight are how hard the rod holds the
 * float and hook this tick, below 0 for not at all.
 */
void StepSession(game::CMainChara &chara, game::CCameraFollow &camera, float *position, float &float_weight, float &hook_weight);

/**
 * A fish's next action about the bait, when CFish::Step asks: the current place's bite stage's.
 */
void UpdateFishAction(game::CFish &fish);

/**
 * How fast a fish swims to the bait: retail's 0.1, or the speed the bite stage gave the fish it chose
 * for the cast (BiteSession::speed).
 */
float ApproachSpeed(const game::CFish &fish);

/**
 * The rod the angler fishes with: the one chosen from the fishing menu while it is in the bag, else
 * the Fishing Rod. The choice is kept with the save (chronicle.dat's FISH chunk).
 */
item::Rod RodInUse();

/**
 * Chooses the rod by item ID; empty for the Fishing Rod.
 */
void               ChooseRod(std::string id);
const std::string &ChosenRod();

/**
 * Chooses the rod by item number (a copy's handle, or the Fishing Rod's); nothing for an item that
 * is not a rod.
 */
void ChooseRodNumber(int number);

/**
 * Whether an item (or a copy's handle) is a rod.
 */
bool IsRod(int number);

/**
 * Every rod (the Rod trait) the bag holds, each once, by item number.
 */
std::vector<int> RodsCarried();

/**
 * Whether the fight holds the hooked fish where it says (FightView::steers), and the way it faces.
 */
bool  FightSteers();
float BattleHeading();

/**
 * Has the dialog framework ask fishing's questions (the sign's, and Circle's with Change rod in
 * Enhanced); once, however often it is called.
 */
void RegisterQuestions();

/**
 * Draws a fight's gauges in the town's 2D pass (a fight that steers: tension, the line still out,
 * the fish veering, HIT! as it starts); nothing otherwise.
 */
void DrawFight();

/**
 * The fishing framework's text keys and their English, for the language files.
 */
std::vector<std::pair<std::string, std::string>> Strings();

} // namespace dc::fishing
