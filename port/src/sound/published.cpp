// nlohmann/json before the game headers, whose one-letter macros it cannot stand.
#include "sound/published.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <set>
#include <string>
#include <vector>

#include "audio/trace.hpp"
#include "data/data_mount.hpp"
#include "data/data_read.hpp"
#include "platform/datapacks.hpp"

namespace dc::sound {

using Json = nlohmann::json;

namespace {

std::string Lower(std::string_view text) {
    std::string out(text);
    std::ranges::transform(out, out.begin(), [](unsigned char c) { return static_cast<char>(c >= 'A' && c <= 'Z' ? c + 0x20 : c); });
    return out;
}

// Where a published file is found: each active datapack's, highest priority first, then the data's.
std::vector<vfs::Ref> Candidates(std::string_view path) {
    std::vector<vfs::Ref> found = platform::datapacks::Find(path);
    if (vfs::Ref own = vfs::Find(data::DataMount(), path)) {
        found.push_back(std::move(own));
    }
    return found;
}

// A published document: the first candidate that is JSON of the kind asked for.
std::optional<Json> Document(std::string_view path, std::string_view kind) {
    for (const vfs::Ref &file : Candidates(path)) {
        std::vector<std::uint8_t> bytes;
        Json                      document = vfs::Read(file, bytes) ? Json::parse(bytes, nullptr, false) : Json(Json::value_t::discarded);
        if (!document.is_discarded() && document.is_object() && document.value("kind", "") == kind) {
            return document;
        }
        // Each broken file is reported once; the next candidate, or the disc's own, plays.
        static std::set<std::string> reported;
        if (reported.insert(file.Display()).second) {
            std::fprintf(stderr, "sound: %s is not a %.*s document\n", file.Display().c_str(), static_cast<int>(kind.size()), kind.data());
        }
    }
    return std::nullopt;
}

std::optional<std::string> Text(std::string_view path) {
    for (const vfs::Ref &file : Candidates(path)) {
        std::vector<std::uint8_t> bytes;
        if (vfs::Read(file, bytes)) {
            return std::string(bytes.begin(), bytes.end());
        }
    }
    return std::nullopt;
}

int Int(const Json &object, const char *key) {
    const auto found = object.find(key);
    return found != object.end() && found->is_number_integer() ? found->get<int>() : 0;
}

// The published table beside a text table: sound/tbl/sqtbl.txt has sound/tbl/sqtbl.json.
std::optional<Json> TableRows(std::string_view table, std::string_view kind) {
    std::filesystem::path path(table);
    path.replace_extension(".json");
    std::optional<Json> document = Document(path.generic_string(), kind);
    if (!document || !document->contains("rows") || !(*document)["rows"].is_array()) {
        return std::nullopt;
    }
    return (*document)["rows"];
}

} // namespace

std::optional<PublishedList> FindList(const void *member, std::size_t size, std::string_view list) {
    data::DataReadSource source;
    if (member == nullptr || size == 0 || !data::DataReadSourceRange(member, size, source)) {
        return std::nullopt;
    }
    std::filesystem::path path(source.path);
    path.replace_extension(".sound.json");
    const std::string         document_path = path.generic_string();
    const std::optional<Json> document = Document(document_path, "sound_pack");
    if (!document || !document->contains("lists") || !(*document)["lists"].is_array()) {
        return std::nullopt;
    }
    for (const Json &entry : (*document)["lists"]) {
        if (!entry.is_object() || Lower(entry.value("name", "")) != Lower(list)) {
            continue;
        }
        PublishedList published{.document = document_path};
        if (const auto config = entry.find("config"); config != entry.end() && config->is_object() && !config->empty()) {
            published.config = PublishedConfig{
                .reverb_mode = Int(*config, "reverb_mode"),
                .reverb_depth = Int(*config, "reverb_depth"),
                .se_table = Int(*config, "se_table"),
                .se_table_type = Int(*config, "se_table_type"),
            };
        }
        for (const Json &load : entry.value("load", Json::array())) {
            const std::string name = Lower(load.value("member", ""));
            if (load.contains("bank") && load["bank"].is_string()) {
                published.banks[name] = load["bank"];
            } else if (load.contains("sequence") && load["sequence"].is_string()) {
                published.sequences[name] = load["sequence"];
            }
        }
        audio::Trace("published: %s list %.*s, %zu banks, %zu sequences", document_path.c_str(), static_cast<int>(list.size()), list.data(),
                     published.banks.size(), published.sequences.size());
        return published;
    }
    return std::nullopt;
}

std::shared_ptr<audio::Bank> LoadBank(const std::string &document) {
    const std::optional<std::string> text = Text(document);
    if (!text) {
        return nullptr;
    }
    return audio::Bank::CreateFromJsonCandidates(*text, [](const std::filesystem::path &sample) { return Candidates(sample.generic_string()); });
}

std::shared_ptr<audio::SqFile> LoadSequence(const std::string &document) {
    const std::optional<std::string> text = Text(document);
    return text ? audio::SqFile::CreateFromJson(*text) : nullptr;
}

std::optional<std::vector<SequenceVolume>> SequenceVolumes(std::string_view table) {
    const std::optional<Json> rows = TableRows(table, "sequence_volumes");
    if (!rows) {
        return std::nullopt;
    }
    std::vector<SequenceVolume> volumes;
    for (const Json &row : *rows) {
        if (row.is_object() && row.contains("name") && row["name"].is_string()) {
            volumes.push_back({.name = row["name"], .volume = Int(row, "volume")});
        }
    }
    return volumes;
}

std::optional<std::vector<EffectVolume>> EffectVolumes(std::string_view table) {
    const std::optional<Json> rows = TableRows(table, "effect_volumes");
    if (!rows) {
        return std::nullopt;
    }
    std::vector<EffectVolume> volumes;
    for (const Json &row : *rows) {
        if (row.is_object()) {
            volumes.push_back({.bank = Int(row, "bank"), .program = Int(row, "program"), .volume = Int(row, "volume")});
        }
    }
    return volumes;
}

} // namespace dc::sound
