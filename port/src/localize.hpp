#pragma once

#include <cstddef>
#include <filesystem>
#include <initializer_list>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "gametext.hpp"

// Game text from JSON the port ships, laid over the retail message files (docs/LOCALIZATION.md). Each
// language is one flat file, as Minecraft's language files are, named for it (en_gb.json, fr_fr.json...) and
// read from <exe>/lang/, then <save>/lang/ over it:
//
//   {"dun.script.d01.d01.12": "There are three chests here.", ...}
//
// A key is the message file's key (LocalizeMessageKey), a dot, and the message id. The keys are the same in
// every language, so the files can be set side by side. A message the JSON does not have, and a file it
// does not have, stay as retail has them.

// One edit to a message the disc has (docs/LOCALIZATION.md, "Patches"): either text that takes its
// place whole, or pairs of a piece of the disc's text and what stands for it. The edit applies only
// where the disc's message still has what it expects: every "find" occurs in it, and "original",
// when given, is the LocalizeTextHash of the whole message.
struct LocalizePatch {
    int                                              id = 0;
    std::string                                      original;
    std::optional<std::string>                       text;
    std::vector<std::pair<std::string, std::string>> replace;
};

struct LocalizeTable {
    // Message file key > message id > text.
    std::map<std::string, std::map<int, std::string>> files;
    // Patches by message file key, in the order they were read; a message a "files" entry has is not patched.
    std::map<std::string, std::vector<LocalizePatch>> patches;
    // The port's own text, the Options screen's: key (a dotted name that does not end in a message id) > text.
    std::map<std::string, std::string> strings;
};

// The key of a message file, the same in every language: the path the game loads (or "<pack path>/<entry>"
// for a file in a pack) with the language taken out, the ".mes" taken off and the slashes made dots.
// dun/script/d01/d01_3.mes is "dun.script.d01.d01", and commenu/a_fre/dungeon/dunmenu5.pak/allmenu.mes is
// "commenu.dungeon.dunmenu5.pak.allmenu".
std::string LocalizeMessageKey(std::string_view path);

// A 64-bit FNV-1a of a message's text as 16 hex digits: what "original" in a patch is checked against,
// and what the export lists in hashes/<language>.json for the person writing one.
std::string LocalizeTextHash(std::string_view text);

// What a replaced file cost: texts that went in, characters the font lacks (shown as '?'), and
// texts left as retail because they did not fit.
struct LocalizeStats {
    int replaced = 0;
    int missing = 0;
    int rejected = 0;
};

// Reads one language's file: a flat object of "<file key>.<id>": text for the disc's messages, and
// "<name>": text for the port's own (a key whose last part is not a number). Gives back false with the
// reason in error for text that is not JSON, a number that is not an s16 id, or a value that is not a
// string; table is untouched then.
bool LocalizeParse(std::string_view json, LocalizeTable &table, std::string &error);

// Reads a patch file, {"patches": [...]}, in the same way.
bool LocalizeParsePatches(std::string_view json, LocalizeTable &table, std::string &error);

// Table entries of over replace those of base.
void LocalizeMerge(LocalizeTable &base, const LocalizeTable &over);

// The messages of a retail message file of words s16, as GameTextDecode reads them; the first of
// an id the file repeats, as the game's lookup finds it. False for a file that is not one.
bool LocalizeReadMessages(const s16 *file, size_t words, std::map<int, std::string> &out);

// retail with texts laid over it, as a message file in out. False, leaving out as it was, where
// retail is not a message file or nothing changed.
bool LocalizeMessages(const s16 *retail, size_t words, const std::map<int, std::string> &texts,
                      std::vector<s16> &out, LocalizeStats &stats);

// The text of a string the port draws itself (the Options screen's labels, help and choices): the current
// language's text for key if its JSON has one, else english. A "%1", "%2"... in a text stands for what
// LocalizeFormat is given.
std::string LocalizeText(std::string_view key, std::string_view english);
std::string LocalizeFormat(std::string text, std::initializer_list<std::string> arguments);

// Host hooks, called from the data loaders. LocalizeLoaded notes where a pack came from (only
// .pak and .pac are kept). LocalizeFile rewrites a message file just read into buffer, which holds
// capacity bytes, and sets *size; a file that would not fit stays as retail. LocalizePack gives
// back the data of pack entry name as the JSON has it (a buffer that lives on), or data itself.
void         LocalizeLoaded(std::string_view path, const void *buffer);
bool         LocalizeFile(std::string_view path, void *buffer, size_t capacity, int *size);
const u_int *LocalizePack(const u_int *pack, std::string_view name, const u_int *data, int *size);

// Writes every message file under the data root as one flat file per language into out (en_gb.json,
// fr_fr.json...; the files that name no language go in every one) and each message's hash into
// out/hashes/ the same way. Gives back how many message files it read, or -1 where the data cannot be
// read; verify counts, on stderr, texts that do not encode back to the retail codes.
int LocalizeExport(const std::filesystem::path &out, bool verify);

// The file called name in the language folders (the save folder's before the executable's), or an
// empty path where neither has one.
std::filesystem::path LocalizeFindFile(std::string_view name);

struct Config;

// A ConfigChangeHook: when game.language is set to a language other than the current one, the switch
// is held until the game can make it whole. A language is not one setting but everything an area loads
// (its message files, menu packs and images), so changing it under a loaded area leaves half of it in
// each; the game applies it between areas, and reloads the town or title it is in once the Options
// screen has closed.
void LocalizeConfigChanged(const Config &before, const Config &after);

// Whether a language is waiting to be switched to, and the switch: LanguageCode changes and the system
// messages (names, places) are read again. The game loop calls it where a new area is about to load.
bool LocalizeLanguagePending();
void LocalizeApplyPendingLanguage();

// For tests: where the language files are, and forgetting what was read.
void LocalizeSetDirectories(std::vector<std::filesystem::path> directories);
void LocalizeReset();
