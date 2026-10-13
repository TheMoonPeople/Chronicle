#pragma once

// dcdata models: finds every character model on the disc and how its skeleton compares with one of
// the playable characters', for putting another character's model on that character's motions.
//
// A character is a pack (.chr, or a pack nested in another) that holds a text configuration
// (.cfg: MODEL, MOTION, KEY ...) and its scenes (.mds). A scene is a 16-byte header whose third word
// counts the objects, then one 112-byte record per object: two words, a 32-byte name, the offset of
// its model data, its parent's index (negative for none) and its matrix. The objects are the
// skeleton's bones, by name and parent, which is what a motion drives.

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "dcdata.hpp"

namespace dcdata {

struct Bone {
    std::string name;
    int         parent = -1;
};

struct Model {
    std::string       where;   // The file on the disc, then each pack member down to the scene.
    std::vector<Bone> bones;
    std::string       config;  // The configuration of the pack holding the scene, or empty.
    int               motions = 0; // KEY lines of that configuration.
};

inline constexpr std::size_t kMdsHeader = 16;
inline constexpr std::size_t kMdsObject = 112;

inline bool EndsWithFolded(std::string_view name, std::string_view suffix) {
    return name.size() >= suffix.size() && EqualsFolded(name.substr(name.size() - suffix.size()), suffix);
}

// The bones of a scene, or nothing when the data is not one.
inline std::optional<std::vector<Bone>> ParseMds(std::span<const unsigned char> data) {
    if (data.size() < kMdsHeader) {
        return std::nullopt;
    }
    std::uint32_t count = Le32(data.data() + 8);
    if (count == 0 || count > 4096 || kMdsHeader + count * kMdsObject > data.size()) {
        return std::nullopt;
    }
    std::vector<Bone> bones(count);
    for (std::uint32_t i = 0; i < count; i++) {
        const unsigned char *object = data.data() + kMdsHeader + i * kMdsObject;
        const char          *name = reinterpret_cast<const char *>(object + 8);
        const void          *end = std::memchr(name, 0, 32);
        if (!end) {
            return std::nullopt;
        }
        bones[i].name.assign(name, static_cast<const char *>(end));
        bones[i].parent = static_cast<std::int32_t>(Le32(object + 44));
        if (bones[i].parent >= static_cast<std::int32_t>(count)) {
            return std::nullopt;
        }
    }
    return bones;
}

// The members of a pack, or nothing when the data is not one.
inline std::optional<std::vector<PackMember>> TryPack(std::span<const unsigned char> data) {
    if (data.size() < 76 || !data[0]) {
        return std::nullopt;
    }
    std::vector<PackMember> members;
    try {
        members = ReadPack(data);
    } catch (const Error &) {
        return std::nullopt;
    }
    if (members.empty()) {
        return std::nullopt;
    }
    for (const PackMember &member : members) {
        for (unsigned char c : member.name) {
            if (c < 0x20 || c >= 0x7F) {
                return std::nullopt;
            }
        }
    }
    return members;
}

inline int CountMotionKeys(std::string_view config) {
    int count = 0;
    for (std::size_t start = 0; start < config.size();) {
        std::size_t end = config.find('\n', start);
        if (end == std::string_view::npos) {
            end = config.size();
        }
        std::string_view line = config.substr(start, end - start);
        std::size_t      first = line.find_first_not_of(" \t\r");
        if (first != std::string_view::npos && line.substr(first).starts_with("KEY") &&
            (line.size() == first + 3 || line[first + 3] == ' ' || line[first + 3] == '\t')) {
            count++;
        }
        start = end + 1;
    }
    return count;
}

// Adds every scene in a pack, and in the packs nested in it, to models.
inline void CollectModels(std::span<const unsigned char> data, const std::string &where, int depth,
                          std::vector<Model> &models) {
    std::optional<std::vector<PackMember>> members = TryPack(data);
    if (!members) {
        return;
    }
    std::string config;
    for (const PackMember &member : *members) {
        if (config.empty() && EndsWithFolded(member.name, ".cfg")) {
            const char *text = reinterpret_cast<const char *>(member.data.data());
            const void *end = std::memchr(text, 0, member.data.size());
            config.assign(text, end ? static_cast<const char *>(end) : text + member.data.size());
        }
    }
    for (const PackMember &member : *members) {
        std::string inner = where + " :: " + member.name;
        if (EndsWithFolded(member.name, ".mds")) {
            if (std::optional<std::vector<Bone>> bones = ParseMds(member.data)) {
                models.push_back({inner, std::move(*bones), config, CountMotionKeys(config)});
            }
        } else if (depth < 3) {
            CollectModels(member.data, inner, depth + 1, models);
        }
    }
}

// How many of the reference's bone names the model has too.
inline int SharedBones(const Model &model, const Model &reference) {
    std::unordered_set<std::string> names;
    for (const Bone &bone : model.bones) {
        names.insert(bone.name);
    }
    int shared = 0;
    for (const Bone &bone : reference.bones) {
        shared += names.contains(bone.name);
    }
    return shared;
}

inline void WriteSkeleton(std::FILE *out, const Model &model) {
    for (std::size_t i = 0; i < model.bones.size(); i++) {
        const Bone &bone = model.bones[i];
        Log(out, "    {:3} {:<32} parent {:3}{}\n", i, bone.name, bone.parent,
            bone.parent >= 0 && bone.parent < static_cast<int>(model.bones.size())
                ? std::format(" ({})", model.bones[bone.parent].name)
                : std::string());
    }
}

inline void WriteConfig(std::FILE *out, const std::string &config) {
    if (config.empty()) {
        Log(out, "    (no configuration)\n");
        return;
    }
    std::size_t start = 0;
    while (start < config.size()) {
        std::size_t end = config.find('\n', start);
        if (end == std::string::npos) {
            end = config.size();
        }
        std::string line = config.substr(start, end - start);
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        Log(out, "    | {}\n", line);
        start = end + 1;
    }
}

// Every model on the disc, the ones most like the reference's skeleton first, then the details of
// the closest ones and of every model in a pack named c<digits> (the story's characters).
inline int ModelReport(const Archive &archive, std::FILE *out, std::string_view reference_path,
                       std::size_t detailed = 40) {
    std::vector<unsigned char> hd2 = ReadExtent(archive.hd2);
    std::vector<Record>        records = ParseIndex(hd2);
    std::size_t                duplicates = 0;
    Reader                     dat(archive.dat.file);
    std::vector<Model>         models;
    std::vector<unsigned char> data;
    for (const Record *record : Reachable(records, nullptr, &duplicates)) {
        if (record->size < 76 || record->size > (64u << 20)) {
            continue;
        }
        data.resize(record->size);
        try {
            dat.Read(archive.dat.offset + record->sector * kSector, data.data(), data.size());
        } catch (const Error &) {
            continue; // a record past the end of DATA.DAT, which extract warns of
        }
        CollectModels(data, record->path, 0, models);
    }

    std::string reference_folded = FoldPath(reference_path);
    auto        reference = std::find_if(models.begin(), models.end(), [&](const Model &model) {
        return model.where.starts_with(reference_folded + " :: ");
    });
    if (reference == models.end()) {
        Fail("no model in {}", reference_path);
    }
    Model ref = *reference;

    struct Row {
        const Model *model;
        int          shared;
    };
    std::vector<Row> rows;
    for (const Model &model : models) {
        rows.push_back({&model, SharedBones(model, ref)});
    }
    std::stable_sort(rows.begin(), rows.end(), [](const Row &a, const Row &b) { return a.shared > b.shared; });

    Log(out, "# dcdata models\n\n");
    Log(out, "source: {}\n", archive.source.filename().string());
    Log(out, "reference: {} ({} bones, {} motion keys)\n", ref.where, ref.bones.size(), ref.motions);
    Log(out, "{} models found\n\n", models.size());

    Log(out, "## Every model, most like the reference first\n\n");
    Log(out, "shared/ref  bones  keys  model\n");
    for (const Row &row : rows) {
        Log(out, "{:4}/{:<4}  {:5}  {:4}  {}\n", row.shared, ref.bones.size(), row.model->bones.size(),
            row.model->motions, row.model->where);
    }

    Log(out, "\n## Reference\n\n{}\n  configuration:\n", ref.where);
    WriteConfig(out, ref.config);
    Log(out, "  skeleton:\n");
    WriteSkeleton(out, ref);

    Log(out, "\n## Details\n");
    std::size_t written = 0;
    for (const Row &row : rows) {
        const Model &model = *row.model;
        if (model.where == ref.where) {
            continue;
        }
        // The story characters' packs are named c<number><letter>, as dun/mainchara/c01d.chr.
        std::string_view file = std::string_view(model.where).substr(0, model.where.find(" :: "));
        std::string_view base = file.substr(file.rfind('/') + 1);
        bool character = base.size() > 2 && (base[0] == 'c' || base[0] == 'C') && base[1] >= '0' && base[1] <= '9';
        if (written >= detailed && !character) {
            continue;
        }
        written++;
        Log(out, "\n{}\n  {} bones, {}/{} of the reference's, {} motion keys\n", model.where, model.bones.size(),
            row.shared, ref.bones.size(), model.motions);
        std::unordered_set<std::string> names;
        for (const Bone &bone : model.bones) {
            names.insert(bone.name);
        }
        std::string missing;
        for (const Bone &bone : ref.bones) {
            if (!names.contains(bone.name)) {
                missing += (missing.empty() ? "" : " ") + bone.name;
            }
        }
        Log(out, "  missing from the reference: {}\n  configuration:\n", missing.empty() ? "none" : missing);
        WriteConfig(out, model.config);
        Log(out, "  skeleton:\n");
        WriteSkeleton(out, model);
    }
    return 0;
}

} // namespace dcdata
