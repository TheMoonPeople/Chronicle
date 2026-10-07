// nlohmann/json before the game headers, whose one-letter macros it cannot stand.
#include "localize.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <functional>
#include <memory>
#include <set>
#include <unordered_map>

#include "../../tools/dcdata/dcdata.hpp"
#include "dataread.hpp"
#include "mainselect.hpp"
#include "menu_option.hpp"
#include "platform/config.hpp"
#include "platform/paths.hpp"

namespace fs = std::filesystem;

namespace {

using Json = nlohmann::ordered_json;

// The 76-byte entry of dataread.cpp's pack format, for walking a pack to its message files.
constexpr size_t kPackName = 64;
constexpr size_t kPackEntry = kPackName + 12;

struct State {
    std::vector<fs::path>                         directories;
    bool                                          directories_set = false;
    int                                           language = -1;
    LocalizeTable                                 table;
    std::set<std::string>                         reported;
    std::unordered_map<const void *, std::string> packs;
    // Packs hand out pointers that callers keep, so what was built is never freed.
    std::map<std::string, std::unique_ptr<std::vector<s16>>> built;
};

State &Get() {
    static State state;
    return state;
}

// Each problem is told once, so a file loaded every frame does not fill the log.
void Report(const std::string &what) {
    if (Get().reported.insert(what).second) {
        std::fprintf(stderr, "localize: %s\n", what.c_str());
    }
}

// The file each LanguageCode reads and the export writes, named as Minecraft's language files are
// (language_COUNTRY.json), and the folder its menu packs are in.
constexpr const char *kLanguageFiles[7] = {"ja_jp", "en_us", "en_gb", "fr_fr", "de_de", "it_it", "es_es"};
constexpr const char *kLanguageDirs[7] = {"a_jpn", "a_usa", "a_eng", "a_fre", "a_ger", "a_ita", "a_spa"};

// The file suffix of a language code, or null for a code that is none.
const char *LanguageFile(int code) {
    return code >= 0 && code < 7 ? kLanguageFiles[code] : nullptr;
}

std::vector<fs::path> Directories() {
    State &state = Get();
    if (state.directories_set) {
        return state.directories;
    }
    std::vector<fs::path> directories;
    fs::path              exe = PathsExecutable();
    if (!exe.empty()) {
        directories.push_back(exe.parent_path() / "lang");
    }
    directories.push_back(PathsSaveRoot() / "lang");
    return directories;
}

bool ReadText(const fs::path &file, std::string &text) {
    std::ifstream stream(file, std::ios::binary);
    if (!stream) {
        return false;
    }
    text.assign(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
    return true;
}

// One language's files (<language>.json, the save folder's over the executable's), without patches.
LocalizeTable LoadLanguage(int code) {
    LocalizeTable table;
    const char   *name = LanguageFile(code);
    if (name == nullptr) {
        return table;
    }
    for (const fs::path &directory : Directories()) {
        fs::path        file = directory / (std::string(name) + ".json");
        std::string     text;
        std::error_code error;
        if (!fs::is_regular_file(file, error) || !ReadText(file, text)) {
            continue;
        }
        LocalizeTable read;
        std::string   why;
        if (LocalizeParse(text, read, why)) {
            LocalizeMerge(table, read);
        } else {
            Report(PathsDisplay(file) + ": " + why + "; using retail text");
        }
    }
    return table;
}

// The table for the current LanguageCode, read when the code first or newly is asked for.
const LocalizeTable &Table() {
    State &state = Get();
    if (state.language == LanguageCode) {
        return state.table;
    }
    state.language = LanguageCode;
    state.table = LoadLanguage(LanguageCode);
    if (LanguageFile(LanguageCode) == nullptr) {
        return state.table;
    }
    // Patches: every .json in <dir>/patches, by name, whatever the language.
    for (const fs::path &directory : Directories()) {
        std::error_code       error;
        std::vector<fs::path> names;
        for (fs::directory_iterator it(directory / "patches", error), end; !error && it != end; it.increment(error)) {
            if (it->path().extension() == ".json") {
                names.push_back(it->path());
            }
        }
        std::sort(names.begin(), names.end());
        for (const fs::path &file : names) {
            std::string text;
            if (!ReadText(file, text)) {
                continue;
            }
            LocalizeTable read;
            std::string   why;
            if (LocalizeParsePatches(text, read, why)) {
                LocalizeMerge(state.table, read);
            } else {
                Report(PathsDisplay(file) + ": " + why + "; patches in it ignored");
            }
        }
    }
    return state.table;
}

// DC_LOCALIZE_TRACE=1 names every message file the game loads, as the JSON would key it, with
// whether the table has an entry for it: how to see what a play-through touches.
void Trace(const std::string &key, bool has_entry) {
    static const bool on = std::getenv("DC_LOCALIZE_TRACE") != nullptr;
    if (on && Get().reported.insert("trace " + key).second) {
        std::fprintf(stderr, "localize-trace: %s %s\n", key.c_str(), has_entry ? "json" : "retail");
    }
}

bool IsPackName(std::string_view path) {
    auto ends = [&](std::string_view suffix) {
        return path.size() >= suffix.size() &&
               std::equal(suffix.begin(), suffix.end(), path.end() - suffix.size(), [](char a, char b) {
                   return a == dcdata::FoldChar(b);
               });
    };
    return ends(".pak") || ends(".pac");
}

bool IsMessageName(std::string_view path) {
    return path.size() >= 4 && std::equal(path.end() - 4, path.end(), ".mes", [](char a, char b) {
               return dcdata::FoldChar(a) == b;
           });
}

std::string FileKey(std::string_view path) {
    if (size_t colon = path.find(':'); colon != std::string_view::npos) {
        path = path.substr(colon + 1);
    }
    return dcdata::FoldPath(path);
}

// A retail message file as the game reads it: s16 words, whose bytes the loader has swapped for
// the host's order already.
struct Message {
    int              id;
    std::vector<s16> codes;
};

bool ReadRetail(const s16 *file, size_t words, std::vector<Message> &out) {
    if (words < 2 || file[0] < 0) {
        return false;
    }
    const size_t count = static_cast<size_t>(file[0]);
    if (2 + count * 2 > words) {
        return false;
    }
    const size_t base = 1 + count;
    for (size_t i = 0; i < count; i++) {
        const int  id = file[2 + i * 2];
        const long start = static_cast<long>(base) + file[3 + i * 2];
        if (start < 0 || static_cast<size_t>(start) >= words) {
            continue;
        }
        Message message{id, {}};
        size_t  at = static_cast<size_t>(start);
        while (at < words && file[at] != MES_CODE_END) {
            message.codes.push_back(file[at++]);
        }
        if (at >= words) {
            continue;
        }
        message.codes.push_back(MES_CODE_END);
        out.push_back(std::move(message));
    }
    return true;
}

} // namespace

std::string LocalizeTextHash(std::string_view text) {
    std::uint64_t hash = 0xCBF29CE484222325ULL;
    for (unsigned char c : text) {
        hash ^= c;
        hash *= 0x100000001B3ULL;
    }
    char digits[17];
    std::snprintf(digits, sizeof digits, "%016llx", static_cast<unsigned long long>(hash));
    return digits;
}

std::string LocalizeMessageKey(std::string_view path) {
    std::string key = FileKey(path);
    // The folder a language's menu packs are in, a_fre/ and the like, is not part of the key.
    for (const char *dir : kLanguageDirs) {
        const std::string folder = std::string(dir) + "/";
        if (key.compare(0, folder.size(), folder) == 0) {
            key.erase(0, folder.size());
        } else if (size_t at = key.find("/" + folder); at != std::string::npos) {
            key.erase(at + 1, folder.size());
        }
    }
    if (key.size() >= 4 && key.compare(key.size() - 4, 4, ".mes") == 0) {
        key.erase(key.size() - 4);
        // The language is the last number of the name: d01_3, steve07_1_4.
        if (key.size() >= 2 && key[key.size() - 2] == '_' && key.back() >= '0' && key.back() <= '6') {
            key.erase(key.size() - 2);
        }
    }
    std::replace(key.begin(), key.end(), '/', '.');
    return key;
}

namespace {

int ParseId(const std::string &key, bool &ok) {
    char *end = nullptr;
    long  id = std::strtol(key.c_str(), &end, 10);
    ok = !key.empty() && *end == '\0' && id >= INT16_MIN && id <= INT16_MAX;
    return static_cast<int>(id);
}

bool ParsePatches(const Json &patches, LocalizeTable &read, std::string &error) {
    if (!patches.is_array()) {
        error = "\"patches\" is not an array";
        return false;
    }
    int number = 0;
    for (const Json &item : patches) {
        number++;
        const std::string where = "patch " + std::to_string(number);
        if (!item.is_object() || !item.contains("file") || !item["file"].is_string() || !item.contains("id")) {
            error = where + " needs a \"file\" and an \"id\"";
            return false;
        }
        LocalizePatch patch;
        bool          ok = false;
        if (item["id"].is_number_integer()) {
            patch.id = ParseId(std::to_string(item["id"].get<long long>()), ok);
        }
        if (!ok) {
            error = where + ": \"id\" is not a message id (an s16)";
            return false;
        }
        if (item.contains("original")) {
            if (!item["original"].is_string()) {
                error = where + ": \"original\" is not a string";
                return false;
            }
            patch.original = item["original"].get<std::string>();
        }
        const bool has_text = item.contains("text");
        const bool has_replace = item.contains("replace");
        if (has_text == has_replace) {
            error = where + " needs exactly one of \"text\" and \"replace\"";
            return false;
        }
        if (has_text) {
            if (!item["text"].is_string()) {
                error = where + ": \"text\" is not a string";
                return false;
            }
            patch.text = item["text"].get<std::string>();
        } else {
            if (!item["replace"].is_array() || item["replace"].empty()) {
                error = where + ": \"replace\" is not a list of [find, with] pairs";
                return false;
            }
            for (const Json &pair : item["replace"]) {
                if (!pair.is_array() || pair.size() != 2 || !pair[0].is_string() || !pair[1].is_string() ||
                    pair[0].get<std::string>().empty()) {
                    error = where + ": \"replace\" is not a list of [find, with] pairs";
                    return false;
                }
                patch.replace.emplace_back(pair[0].get<std::string>(), pair[1].get<std::string>());
            }
        }
        // A patch names its file by the path the game loads or by its message key.
        const std::string file = item["file"].get<std::string>();
        read.patches[IsMessageName(file) || file.find_first_of("/\\") != std::string::npos ? LocalizeMessageKey(file)
                                                                                           : file]
            .push_back(std::move(patch));
    }
    return true;
}

} // namespace

bool LocalizeParse(std::string_view json, LocalizeTable &table, std::string &error) {
    LocalizeTable read;
    try {
        Json root = Json::parse(json);
        if (!root.is_object()) {
            error = "the top level is not an object";
            return false;
        }
        for (const auto &[key, text] : root.items()) {
            if (!text.is_string()) {
                error = "\"" + key + "\" is not a string";
                return false;
            }
            // "dun.script.d01.d01.12" is message 12 of a message file; any other key is a string of the port's.
            const size_t      dot = key.rfind('.');
            const std::string last = dot == std::string::npos ? key : key.substr(dot + 1);
            const bool        number = !last.empty() && last.find_first_not_of("-0123456789") == std::string::npos &&
                                       last != "-";
            if (!number) {
                read.strings[key] = text.get<std::string>();
                continue;
            }
            bool      ok = false;
            const int id = ParseId(last, ok);
            if (!ok || dot == 0 || dot == std::string::npos) {
                error = "\"" + key + "\" is not a message key (a file key, a dot and an s16 id)";
                return false;
            }
            read.files[key.substr(0, dot)][id] = text.get<std::string>();
        }
    } catch (const Json::exception &failure) {
        error = failure.what();
        return false;
    }
    table = std::move(read);
    return true;
}

bool LocalizeParsePatches(std::string_view json, LocalizeTable &table, std::string &error) {
    LocalizeTable read;
    try {
        Json root = Json::parse(json);
        auto patches = root.is_object() ? root.find("patches") : root.end();
        if (!root.is_object() || patches == root.end()) {
            error = "no \"patches\" list";
            return false;
        }
        if (!ParsePatches(*patches, read, error)) {
            return false;
        }
    } catch (const Json::exception &failure) {
        error = failure.what();
        return false;
    }
    table = std::move(read);
    return true;
}

void LocalizeMerge(LocalizeTable &base, const LocalizeTable &over) {
    for (const auto &[file, messages] : over.files) {
        for (const auto &[id, text] : messages) {
            base.files[file][id] = text;
        }
    }
    for (const auto &[file, patches] : over.patches) {
        auto &target = base.patches[file];
        target.insert(target.end(), patches.begin(), patches.end());
    }
    for (const auto &[key, text] : over.strings) {
        base.strings[key] = text;
    }
}

bool LocalizeReadMessages(const s16 *file, size_t words, std::map<int, std::string> &out) {
    std::vector<Message> messages;
    if (!ReadRetail(file, words, messages)) {
        return false;
    }
    for (const Message &message : messages) {
        out.emplace(message.id, GameTextDecode(message.codes.data()));
    }
    return true;
}

bool LocalizeMessages(const s16 *retail, size_t words, const std::map<int, std::string> &texts,
                      std::vector<s16> &out, LocalizeStats &stats) {
    std::vector<Message> messages;
    if (texts.empty() || !ReadRetail(retail, words, messages)) {
        return false;
    }
    // Retail's own texts go in as codes, so none is changed by being read back as text.
    std::map<int, const Message *> first;
    for (const Message &message : messages) {
        first.emplace(message.id, &message);
    }
    GameTextFile file;
    for (const auto &[id, message] : first) {
        std::string text = GameTextDecode(message->codes.data());
        if (file.Set(id, text) < 0) {
            return false;
        }
    }
    LocalizeStats made;
    for (const auto &[id, text] : texts) {
        const int missing = file.Set(id, text);
        if (missing < 0) {
            made.rejected++;
        } else {
            made.replaced++;
            made.missing += missing;
        }
    }
    stats = made;
    if (made.replaced == 0) {
        return false;
    }
    // GameTextFile does not say how long the file is; the last message by offset ends it.
    const s16   *data = file.Data();
    const size_t base = 1 + static_cast<size_t>(data[0]);
    size_t       end = 2 + static_cast<size_t>(data[0]) * 2;
    for (int i = 0; i < data[0]; i++) {
        size_t at = base + static_cast<size_t>(data[3 + i * 2]);
        while (data[at] != MES_CODE_END) {
            at++;
        }
        end = std::max(end, at + 1);
    }
    out.assign(data, data + end);
    return true;
}

namespace {

// What the JSON says for one message file: the patches applied to the disc's messages, then the
// "files" texts, which win. A patch that does not apply is told once and the message stays as it was.
std::map<int, std::string> Wanted(const LocalizeTable &table, const std::string &key, const s16 *retail,
                                  size_t words) {
    std::map<int, std::string> texts;
    if (auto found = table.files.find(key); found != table.files.end()) {
        texts = found->second;
    }
    auto patches = table.patches.find(key);
    if (patches == table.patches.end()) {
        return texts;
    }
    std::map<int, std::string> current;
    if (!LocalizeReadMessages(retail, words, current)) {
        return texts;
    }
    for (const LocalizePatch &patch : patches->second) {
        const std::string name = key + " message " + std::to_string(patch.id);
        if (table.files.count(key) != 0 && table.files.at(key).count(patch.id) != 0) {
            continue;
        }
        auto message = current.find(patch.id);
        if (message == current.end()) {
            Report(name + ": not in the disc's file; patch skipped");
            continue;
        }
        if (!patch.original.empty() && patch.original != LocalizeTextHash(message->second)) {
            Report(name + ": the disc's text is not what the patch was written for; patch skipped");
            continue;
        }
        std::string text;
        if (patch.text) {
            text = *patch.text;
        } else {
            text = message->second;
            int applied = 0;
            for (const auto &[find, with] : patch.replace) {
                size_t at = text.find(find);
                if (at == std::string::npos) {
                    Report(name + ": \"" + find + "\" is not in the disc's text; that replacement skipped");
                    continue;
                }
                applied++;
                for (; at != std::string::npos; at = text.find(find, at + with.size())) {
                    text.replace(at, find.size(), with);
                }
            }
            if (applied == 0) {
                continue;
            }
        }
        message->second = text;
        texts[patch.id] = text;
    }
    return texts;
}

} // namespace

void LocalizeLoaded(std::string_view path, const void *buffer) {
    if (IsPackName(path)) {
        Get().packs[buffer] = FileKey(path);
    } else {
        Get().packs.erase(buffer);
    }
}

bool LocalizeFile(std::string_view path, void *buffer, size_t capacity, int *size) {
    if (!IsMessageName(path)) {
        return false;
    }
    const LocalizeTable &table = Table();
    const std::string    key = LocalizeMessageKey(path);
    const bool           known = table.files.count(key) != 0 || table.patches.count(key) != 0;
    Trace(key, known);
    if (!known || size == nullptr || *size < 4) {
        return false;
    }
    std::vector<s16> out;
    LocalizeStats    stats;
    const size_t     words = static_cast<size_t>(*size) / 2;
    const auto       texts = Wanted(table, key, static_cast<const s16 *>(buffer), words);
    if (!LocalizeMessages(static_cast<const s16 *>(buffer), words, texts, out, stats)) {
        if (stats.rejected > 0) {
            Report(key + ": " + std::to_string(stats.rejected) + " texts did not fit; retail text kept");
        }
        return false;
    }
    if (out.size() * 2 > capacity) {
        Report(key + ": the translated file is " + std::to_string(out.size() * 2) + " bytes, past the " +
               std::to_string(capacity) + " the game reads it into; retail text kept");
        return false;
    }
    if (stats.rejected > 0) {
        Report(key + ": " + std::to_string(stats.rejected) + " texts did not fit; retail text kept for them");
    }
    if (stats.missing > 0) {
        Report(key + ": " + std::to_string(stats.missing) + " characters the game font lacks became '?'");
    }
    std::memcpy(buffer, out.data(), out.size() * 2);
    *size = static_cast<int>(out.size() * 2);
    return true;
}

const u_int *LocalizePack(const u_int *pack, std::string_view name, const u_int *data, int *size) {
    State &state = Get();
    if (data == nullptr || size == nullptr || !IsMessageName(name)) {
        return data;
    }
    auto from = state.packs.find(pack);
    if (from == state.packs.end()) {
        return data;
    }
    const LocalizeTable &table = Table();
    const std::string    key = LocalizeMessageKey(from->second + "/" + std::string(name));
    const bool           known = table.files.count(key) != 0 || table.patches.count(key) != 0;
    Trace(key, known);
    if (!known || *size < 4) {
        return data;
    }
    // The same pack read again hands out the same text again, as long as the retail bytes and the
    // language are the same.
    const size_t bytes = static_cast<size_t>(*size);
    std::string  tag = key + "#" + std::to_string(LanguageCode) + "#" +
                       std::to_string(std::hash<std::string_view>{}(
                           std::string_view(reinterpret_cast<const char *>(data), bytes)));
    if (auto cached = state.built.find(tag); cached != state.built.end()) {
        *size = static_cast<int>(cached->second->size() * 2);
        return reinterpret_cast<const u_int *>(cached->second->data());
    }
    std::vector<s16> out;
    LocalizeStats    stats;
    const auto       texts = Wanted(table, key, reinterpret_cast<const s16 *>(data), bytes / 2);
    if (!LocalizeMessages(reinterpret_cast<const s16 *>(data), bytes / 2, texts, out, stats)) {
        if (stats.rejected > 0) {
            Report(key + ": " + std::to_string(stats.rejected) + " texts did not fit; retail text kept");
        }
        return data;
    }
    if (stats.rejected > 0) {
        Report(key + ": " + std::to_string(stats.rejected) + " texts did not fit; retail text kept for them");
    }
    if (stats.missing > 0) {
        Report(key + ": " + std::to_string(stats.missing) + " characters the game font lacks became '?'");
    }
    // The game reads whole quadwords off a message file's end, as it does off the disc's.
    out.resize((out.size() + 7) & ~size_t{7}, MES_CODE_END);
    auto  kept = std::make_unique<std::vector<s16>>(std::move(out));
    auto *result = reinterpret_cast<const u_int *>(kept->data());
    *size = static_cast<int>(kept->size() * 2);
    state.built[tag] = std::move(kept);
    return result;
}

std::string LocalizeText(std::string_view english_key, std::string_view english) {
    const LocalizeTable &table = Table();
    const std::string    key(english_key);
    if (auto found = table.strings.find(key); found != table.strings.end()) {
        return found->second;
    }
    // A key that ends in a number is read as a message id (options.game.map.choice.0); it is the same entry.
    if (size_t dot = key.rfind('.'); dot != std::string::npos) {
        bool      ok = false;
        const int id = ParseId(key.substr(dot + 1), ok);
        if (auto file = ok ? table.files.find(key.substr(0, dot)) : table.files.end(); file != table.files.end()) {
            if (auto text = file->second.find(id); text != file->second.end()) {
                return text->second;
            }
        }
    }
    return std::string(english);
}

std::string LocalizeFormat(std::string text, std::initializer_list<std::string> arguments) {
    int number = 1;
    for (const std::string &argument : arguments) {
        const std::string placeholder = "%" + std::to_string(number++);
        for (size_t at = text.find(placeholder); at != std::string::npos; at = text.find(placeholder, at + argument.size())) {
            text.replace(at, placeholder.size(), argument);
        }
    }
    return text;
}

std::filesystem::path LocalizeFindFile(std::string_view name) {
    std::vector<fs::path> directories = Directories();
    for (auto directory = directories.rbegin(); directory != directories.rend(); ++directory) {
        fs::path        file = *directory / std::string(name);
        std::error_code error;
        if (fs::is_regular_file(file, error)) {
            return file;
        }
    }
    return {};
}

namespace {

// 0: none waiting.
int g_pending_language = 0;

} // namespace

void LocalizeConfigChanged(const Config &before, const Config &after) {
    if (after.language == before.language) {
        return;
    }
    // "ask" and the language the game is already in leave nothing to switch to.
    g_pending_language = after.language >= 2 && after.language <= 6 && after.language != LanguageCode ? after.language : 0;
}

bool LocalizeLanguagePending() {
    return g_pending_language != 0;
}

void LocalizeApplyPendingLanguage() {
    if (g_pending_language == 0) {
        return;
    }
    LanguageCode = g_pending_language;
    g_pending_language = 0;
    if (SystemMes != nullptr) {
        LoadSystemMessage();
    }
}

void LocalizeSetDirectories(std::vector<fs::path> directories) {
    State &state = Get();
    state.directories = std::move(directories);
    state.directories_set = true;
    state.language = -1;
}

void LocalizeReset() {
    State &state = Get();
    state.language = -1;
    state.table = {};
    state.reported.clear();
    state.packs.clear();
}

namespace {

// Which language a message file is for, from where the game keeps it: "_N.mes", "img_N" or an
// "a_xxx" directory. -1 where its path says none.
int FileLanguage(const std::string &key) {
    static const std::pair<const char *, int> dirs[] = {
        {"a_jpn/", 0},
        {"a_usa/", 1},
        {"a_eng/", 2},
        {"a_fre/", 3},
        {"a_ger/", 4},
        {"a_ita/", 5},
        {"a_spa/", 6}
    };
    for (const auto &[dir, code] : dirs) {
        if (key.find(dir) != std::string::npos) {
            return code;
        }
    }
    std::string_view name = key;
    if (size_t slash = name.rfind('/'); slash != std::string_view::npos) {
        name = name.substr(slash + 1);
    }
    if (name.size() >= 6 && name[name.size() - 6] == '_' && name[name.size() - 5] >= '0' &&
        name[name.size() - 5] <= '6' && name.substr(name.size() - 4) == ".mes") {
        return name[name.size() - 5] - '0';
    }
    return -1;
}

struct Exported {
    std::string                key;
    std::map<int, std::string> messages;
    int                        language;
    // The language came from what the text is, not from the path.
    bool derived = false;
};

// Japanese text is not text but codes from 0 up, which GameTextDecode writes as "{N}"; every other
// language's codes are negative.
bool IsJapanese(const std::map<int, std::string> &messages) {
    for (const auto &[id, text] : messages) {
        for (size_t at = text.find('{'); at != std::string::npos; at = text.find('{', at + 1)) {
            if (at + 1 < text.size() && text[at + 1] >= '0' && text[at + 1] <= '9') {
                return true;
            }
        }
    }
    return false;
}

} // namespace

int LocalizeExport(const fs::path &out, bool verify) {
    const fs::path &root = PathsDataRoot();
    std::error_code error;
    if (!fs::is_directory(root, error)) {
        std::fprintf(stderr, "export: %s is not a directory\n", PathsDisplay(root).c_str());
        return -1;
    }
    std::vector<Exported> found;
    int                   mismatched = 0;
    auto                  add = [&](const std::string &key, const s16 *words, size_t count) {
        Exported item;
        item.key = key;
        item.language = FileLanguage(key);
        if (!LocalizeReadMessages(words, count, item.messages) || item.messages.empty()) {
            return;
        }
        // A file that names no language but is Japanese (the menu packs left in commenu/) is for that one.
        if (item.language == -1 && IsJapanese(item.messages)) {
            item.language = 0;
            item.derived = true;
        }
        if (verify) {
            std::map<int, std::string> again;
            LocalizeStats              stats;
            std::vector<s16>           rebuilt;
            std::map<int, std::string> same = item.messages;
            if (LocalizeMessages(words, count, same, rebuilt, stats)) {
                std::map<int, std::string> back;
                if (!LocalizeReadMessages(rebuilt.data(), rebuilt.size(), back) || back != item.messages ||
                    stats.missing != 0 || stats.rejected != 0) {
                    mismatched++;
                    std::fprintf(stderr, "export: %s does not read back as it was (missing %d, rejected %d)\n",
                                 key.c_str(), stats.missing, stats.rejected);
                }
            } else {
                mismatched++;
                std::fprintf(stderr, "export: %s could not be rebuilt\n", key.c_str());
            }
        }
        found.push_back(std::move(item));
    };

    auto options = fs::directory_options::follow_directory_symlink | fs::directory_options::skip_permission_denied;
    for (fs::recursive_directory_iterator it(root, options, error), end; !error && it != end; it.increment(error)) {
        std::error_code entry_error;
        if (!it->is_regular_file(entry_error)) {
            continue;
        }
        const std::string key = dcdata::FoldPath(it->path().lexically_relative(root).generic_string());
        const bool        mes = IsMessageName(key);
        if (!mes && !IsPackName(key)) {
            continue;
        }
        std::string bytes;
        if (!ReadText(it->path(), bytes)) {
            continue;
        }
        if (mes) {
            std::vector<s16> words(bytes.size() / 2);
            std::memcpy(words.data(), bytes.data(), words.size() * 2);
            add(key, words.data(), words.size());
            continue;
        }
        for (size_t at = 0; at + kPackEntry <= bytes.size();) {
            const char *entry = bytes.data() + at;
            if (entry[0] == 0) {
                break;
            }
            int offset, size, next;
            std::memcpy(&offset, entry + kPackName, 4);
            std::memcpy(&size, entry + kPackName + 4, 4);
            std::memcpy(&next, entry + kPackName + 8, 4);
            std::string name(entry, std::find(entry, entry + kPackName, '\0') - entry);
            if (offset < 0 || size < 0 || next <= 0 || at + static_cast<size_t>(offset) + size > bytes.size()) {
                break;
            }
            if (IsMessageName(name)) {
                std::vector<s16> words(static_cast<size_t>(size) / 2);
                std::memcpy(words.data(), bytes.data() + at + offset, words.size() * 2);
                add(key + "/" + dcdata::FoldPath(name), words.data(), words.size());
            }
            at += static_cast<size_t>(next);
        }
    }

    // One flat file per language, as Minecraft's are ("<file key>.<id>": text), with the files that name
    // no language in every one. Where a file's path names its language it wins over one that does not.
    // Two files of one language that come to the same key would be one file to the loader, so that is told.
    std::error_code made;
    fs::create_directories(out / "hashes", made);
    int collisions = 0;
    for (int code = 0; code < 7; code++) {
        std::map<std::string, const Exported *>           chosen;
        std::map<std::string, std::map<int, std::string>> keyed;
        auto                                              rank = [](const Exported &item) { return item.language == -1 ? 0 : item.derived ? 1
                                                                                                                                          : 2; };
        for (const Exported &item : found) {
            if (item.language != code && item.language != -1) {
                continue;
            }
            const std::string key = LocalizeMessageKey(item.key);
            auto              at = chosen.find(key);
            if (at == chosen.end() || rank(item) > rank(*at->second)) {
                chosen[key] = &item;
            } else if (rank(item) == rank(*at->second) && at->second->key != item.key) {
                collisions++;
                std::fprintf(stderr, "export: %s and %s are both \"%s\"\n", at->second->key.c_str(),
                             item.key.c_str(), key.c_str());
            }
        }
        for (const auto &[key, item] : chosen) {
            keyed[key] = item->messages;
        }
        Json texts = Json::object();
        Json hashes = Json::object();
        for (const auto &[key, messages] : keyed) {
            for (const auto &[id, text] : messages) {
                texts[key + "." + std::to_string(id)] = text;
                hashes[key + "." + std::to_string(id)] = LocalizeTextHash(text);
            }
        }
        // The port's own strings (the Options screen's), as this language shows them now: the shipped
        // translation if there is one, else the English the code has.
        const LocalizeTable language = LoadLanguage(code);
        for (const auto &[key, english] : OptionStrings()) {
            auto text = language.strings.find(key);
            texts[key] = text != language.strings.end() ? text->second : english;
        }
        std::ofstream(out / (std::string(kLanguageFiles[code]) + ".json"), std::ios::binary | std::ios::trunc)
            << texts.dump(4, ' ', false, Json::error_handler_t::replace) << '\n';
        std::ofstream(out / "hashes" / (std::string(kLanguageFiles[code]) + ".json"), std::ios::binary | std::ios::trunc)
            << hashes.dump(4, ' ', false, Json::error_handler_t::replace) << '\n';
    }
    if (collisions != 0) {
        std::fprintf(stderr, "export: %d message files share a key with another\n", collisions);
    }
    if (verify) {
        std::fprintf(stderr, "export: %d of %zu message files do not read back as they were\n", mismatched,
                     found.size());
    }
    return static_cast<int>(found.size());
}
