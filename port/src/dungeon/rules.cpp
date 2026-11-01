#include "dungeon/rules.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <format>
#include <fstream>
#include <print>
#include <set>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>

#include "platform/native_config.hpp"
#include "platform/paths.hpp"

namespace dc::dungeon {

using Json = nlohmann::json;
namespace native_config = platform::native_config;

namespace {

constexpr const char *kPath = "dungeon/rules.json";
constexpr const char *kKind = "dungeon_rules";

Rules g_rules;

// The document's switches, each a member of Rules by its name.
constexpr std::pair<const char *, bool Rules::*> kSwitches[] = {
    {"key_item_filter",        &Rules::key_item_filter       },
    {"auto_open_locked_doors", &Rules::auto_open_locked_doors},
};

Rules Parse(const Json &document, std::set<std::string> &unknown) {
    Rules rules;
    for (const auto &[name, member] : kSwitches) {
        if (const auto found = document.find(name); found != document.end()) {
            if (!found->is_boolean()) {
                throw std::runtime_error(std::format("{} must be true or false", name));
            }
            rules.*member = found->get<bool>();
        }
    }
    for (const auto &[key, value] : document.items()) {
        const bool known = key == "schema_version" || key == "kind" ||
                           std::ranges::any_of(kSwitches, [&key](const auto &entry) { return key == entry.first; });
        if (!known) {
            unknown.insert(key);
        }
    }
    return rules;
}

} // namespace

void LoadRules() {
    g_rules = {};
    std::set<std::string> unknown;
    native_config::Read(kPath, kKind, [&](const Json &document) {
        unknown.clear();
        g_rules = Parse(document, unknown);
    });
    if (!unknown.empty()) {
        std::string names;
        for (const std::string &name : unknown) {
            names += names.empty() ? name : ", " + name;
        }
        native_config::Report(std::format("gameplay/{}", kPath), "fields this version does not read: " + names);
    }
}

const Rules &GetRules() {
    return g_rules;
}

bool ExportRules(const std::filesystem::path &directory) {
    Json document = {
        {"schema_version", native_config::kSchemaVersion},
        {"kind",           kKind                        },
    };
    const Rules off;
    for (const auto &[name, member] : kSwitches) {
        document[name] = off.*member;
    }
    const std::filesystem::path file = directory / "gameplay" / kPath;
    std::error_code             error;
    std::filesystem::create_directories(file.parent_path(), error);
    std::ofstream stream(file, std::ios::binary | std::ios::trunc);
    stream << document.dump(2) << '\n';
    if (error || !stream) {
        std::println(stderr, "cannot write {}", PathsDisplay(file));
        return false;
    }
    return true;
}

} // namespace dc::dungeon
