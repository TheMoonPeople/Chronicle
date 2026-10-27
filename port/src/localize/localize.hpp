#pragma once

#include <cstddef>
#include <filesystem>
#include <initializer_list>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "text/gametext.hpp"

namespace dc::platform {
struct Config;
} // namespace dc::platform

namespace dc::localize {

// Game text from JSON the port ships, laid over the retail message files (docs/LOCALIZATION.md). Each
// language is one flat file named for it (en_gb.json, fr_fr.json...), read from <exe>/lang/, then
// <save>/lang/ over it:
//
//   {"dun.script.d01.d01.12": "There are three chests here.", ...}
//
// A key is the message file's key (LocalizeMessageKey), a dot, and the message id. The keys are the same in
// every language. A message or file the JSON does not have stays as retail has it.

/**
 * One edit to a message the disc has (docs/LOCALIZATION.md, "Patches"). It applies only where every
 * "find" occurs in the disc's message and, when given, "original" is the LocalizeTextHash of it.
 */
struct LocalizePatch {
    int                                              id = 0;   /**< Message id within its file. */
    std::string                                      original; /**< Expected text hash, or empty for any. */
    std::optional<std::string>                       text;     /**< Text that replaces the message whole. */
    std::vector<std::pair<std::string, std::string>> replace;  /**< Find/with pairs applied in order. */
};

/**
 * Everything one language's JSON files say: replacement message texts, patches and the port's own
 * strings. A message a "files" entry has is not patched.
 */
struct LocalizeTable {
    std::map<std::string, std::map<int, std::string>> files;   /**< Message file key > message id > text. */
    std::map<std::string, std::vector<LocalizePatch>> patches; /**< Patches by file key, in read order. */
    std::map<std::string, std::string>                strings; /**< The port's own text by dotted key. */
};

/**
 * The language-independent key of a message file: the loaded path (or "<pack path>/<entry>") with the
 * language removed, ".mes" dropped and slashes made dots (dun/script/d01/d01_3.mes is "dun.script.d01.d01").
 */
std::string LocalizeMessageKey(std::string_view path);

/**
 * A 64-bit FNV-1a of a message's text as 16 hex digits, which a patch's "original" is checked against
 * and the export lists in hashes/<language>.json.
 */
std::string LocalizeTextHash(std::string_view text);

/**
 * What replacing one message file cost.
 */
struct LocalizeStats {
    int replaced = 0; /**< Texts that went in. */
    int missing = 0;  /**< Characters the font lacks, shown as '?'. */
    int rejected = 0; /**< Texts left as retail because they did not fit. */
};

/**
 * Reads one language's flat JSON object into table. Returns false with the reason in error, leaving
 * table untouched, for invalid JSON, a non-s16 message id or a non-string value.
 */
bool LocalizeParse(std::string_view json, LocalizeTable &table, std::string &error);

/**
 * Reads a patch file, {"patches": [...]}, in the same way as LocalizeParse.
 */
bool LocalizeParsePatches(std::string_view json, LocalizeTable &table, std::string &error);

/**
 * Merges over into base; entries of over replace those of base.
 */
void LocalizeMerge(LocalizeTable &base, const LocalizeTable &over);

/**
 * Decodes the messages of a retail message file of words s16, keeping the first of a repeated id as the
 * game's lookup does. Returns false for data that is not a message file.
 */
bool LocalizeReadMessages(const s16 *file, size_t words, std::map<int, std::string> &out);

/**
 * Builds retail with texts laid over it as a message file in out. Returns false, leaving out as it was,
 * where retail is not a message file or nothing changed.
 */
bool LocalizeMessages(const s16 *retail, size_t words, const std::map<int, std::string> &texts,
                      std::vector<s16> &out, LocalizeStats &stats);

/**
 * The current language's text for a string the port draws itself, or english where its JSON has none.
 * "%1", "%2"... in a text stand for LocalizeFormat's arguments.
 */
std::string LocalizeText(std::string_view key, std::string_view english);

/**
 * The file stem of the current language (`fr_fr`), or an empty string for one with no language file.
 */
std::string LocalizeLanguageFile();

/**
 * Replaces each "%N" in text with the Nth argument, counting from one.
 */
std::string LocalizeFormat(std::string text, std::initializer_list<std::string> arguments);

/**
 * Data-loader hook: notes where a pack buffer came from (only .pak and .pac are kept).
 */
void LocalizeLoaded(std::string_view path, const void *buffer);

/**
 * Data-loader hook: rewrites a message file just read into buffer, which holds capacity bytes, and
 * sets *size. A file that would not fit stays as retail.
 */
bool LocalizeFile(std::string_view path, void *buffer, size_t capacity, int *size);

/**
 * Data-loader hook: the data of pack entry name as the JSON has it (a buffer that lives on), or data
 * itself.
 */
const u_int *LocalizePack(const u_int *pack, std::string_view name, const u_int *data, int *size);

/**
 * Writes every message file under the data root as one flat file per language into out, and each
 * message's hash into out/hashes/. Returns how many message files it read; verify reports, on stderr,
 * texts that do not encode back to the retail codes.
 */
int LocalizeExport(const std::filesystem::path &out, bool verify);

/**
 * Sets the font file named on the command line (--font), which LocalizeFindFile("font.ttf") prefers
 * over the language folders' own.
 */
void LocalizeSetFontPath(const std::filesystem::path &file);

/**
 * The file called name in the language folders (the save folder's before the executable's), or an
 * empty path where neither has one.
 */
std::filesystem::path LocalizeFindFile(std::string_view name);

/**
 * ConfigChangeHook that holds a game.language change until the game can apply it between areas, since
 * an area's message files, menu packs and images all depend on it.
 */
void LocalizeConfigChanged(const platform::Config &before, const platform::Config &after);

/**
 * Whether a language is waiting to be switched to.
 */
bool LocalizeLanguagePending();

/**
 * Switches LanguageCode to the pending language and rereads the system messages; called where a new
 * area is about to load.
 */
void LocalizeApplyPendingLanguage();

/**
 * Test hook: sets where the language files are.
 */
void LocalizeSetDirectories(std::vector<std::filesystem::path> directories);

/**
 * Test hook: forgets what was read.
 */
void LocalizeReset();

} // namespace dc::localize
