#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "item/framework.hpp"
#include "itemdata.hpp"
#include "shop.hpp"

/**
 * Retail's item tables as gameplay documents (docs/ITEM_FRAMEWORK.md): ComItemInfo with each item's
 * model name, ITEM_LIST, AttachList, WeaponList, the baits and PriceList.
 */
namespace dc::item {

/**
 * Fills every table from the highest-priority usable datapack document, then the data's own, or as
 * compiled where there is none, then registers every vanilla item as the tables now hold it, and the
 * items' chunk of the save. datapacks::Load runs first.
 */
void Load();

/**
 * Registers the items' chunk of the save's chronicle.dat: the custom items and copies in the bag, its
 * quick slots and storage, taken out of save.dat as it is written and put back as it loads. Load does.
 */
void RegisterSaveChunk();

/**
 * Writes the tables' documents as compiled under directory/gameplay/, pretty-printed, as a starting
 * point for a datapack. Returns false when one cannot be written.
 */
bool Export(const std::filesystem::path &directory);

// The game finds an item's name at its number + 100 in the system messages, which use ids up to 2080,
// so custom numbers start past them.
inline constexpr int kCustomFirst = 0x800;      /**< Custom items' numbers start here... */
inline constexpr int kCustomEnd = kHandleFirst; /**< ...and stop before the copies' handles. */

/**
 * What retail's number ranges make an item.
 */
enum class Kind {
    None,       /**< Not an item: below the attachments, or an unused custom number. */
    Attachment, /**< ITEM_ATTACH_START up to ITEM_DUNGEON_START. */
    Item,       /**< ITEM_DUNGEON_START up to ITEM_WEAPON_START, and every custom item. */
    Weapon,     /**< ITEM_WEAPON_START up to the custom numbers. */
};

/**
 * The kind of an item number. Retail tells kinds apart by ranges, often taking anything from
 * ITEM_WEAPON_START up for a weapon; the code that would take a custom item for one asks this instead.
 */
Kind KindOf(int number);

/**
 * Frees the handles of copies no longer in the bag, its quick slots or storage. Only where no menu
 * holds an item: between areas, and when saving.
 */
void ReclaimHeldCopies();

/**
 * The number standing for an item a loaded save holds whose definition is missing, an orphan: kept
 * with its ID, so it is itself again once its definition returns, and inert meanwhile. The same ID
 * gives the same number; -1 once the custom numbers are used up.
 */
int OrphanNumber(std::string_view id);

/**
 * Whether a number, or a handle's item, is an orphan.
 */
bool IsOrphan(int number);

/**
 * Forgets the orphans, as another save loads.
 */
void DropOrphans();

/**
 * Lists the items' localization keys and their English text.
 */
std::vector<std::pair<std::string, std::string>> Strings();

/**
 * A custom item's name and description in one language.
 */
struct ItemText {
    std::string name;
    std::string description;
};

/**
 * A custom item's row of ComItemInfo: a bag item with no row in retail's tables, its icon, and its
 * own number as its message number. Null for a number that is not a custom item.
 */
game::COM_ITEM_INFO *CustomCommonInfo(int number);

/**
 * A custom item's row of ITEM_LIST, or null.
 */
game::ITEM_DATA *CustomItemData(int number);

/**
 * A custom item's prices, or null.
 */
const game::ITEM_MONEY_ENTRY *CustomPrice(int number);

/**
 * A vanilla item's ID from its number, whether or not the registry is loaded: vanilla:item_<number>
 * for a number the decomp has not named.
 */
std::string VanillaItemId(int number);

/**
 * A custom item's model name, as ITEM_NAME_TBL_NEW holds retail's; null for none or not a custom item.
 */
const char *CustomModel(int number);

/**
 * The model in dun/item/main_data of a bag item or attachment, retail's (ITEM_NAME_TBL_NEW) or a custom
 * item's; null where it has none.
 */
const char *ModelName(int number);

/**
 * A custom item's text in a language (its language file, such as fr_fr), else in English, else the
 * first it has; null for a number that is not a custom item.
 */
const ItemText *CustomText(int number, std::string_view language);

/**
 * A message the game looks up that a custom item answers, for when the message file lacks it: the
 * item's name at its number + 100 in the system messages (system true), else its description at its
 * number + 500, in the current language. The codes end with MES_CODE_END and last until the next Load;
 * null for any other id.
 */
short *CustomMessage(int id, bool system);

} // namespace dc::item
