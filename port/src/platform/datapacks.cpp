#include "platform/datapacks.hpp"

#include <nlohmann/json.hpp>

#include <fstream>
#include <iterator>
#include <memory>
#include <print>
#include <ranges>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>

#include "platform/paths.hpp"

namespace dc::platform::datapacks {

namespace {

constexpr int kSchemaVersion = 1;

/**
 * Enabled packs' mounts in increasing priority.
 */
std::vector<std::shared_ptr<const vfs::Mount>> packs;

void Diagnostic(const std::filesystem::path &path, std::string_view message) {
    std::println(stderr, "datapacks: {}: {}", PathsDisplay(path), message);
}

std::vector<std::shared_ptr<const vfs::Mount>> ReadConfig(const std::filesystem::path &directory,
                                                          const std::filesystem::path &config) {
    std::ifstream stream(config, std::ios::binary);
    const auto    root = nlohmann::json::parse(stream, nullptr, false);
    const auto    version = root.is_object() ? root.find("schema_version") : root.end();
    if (!root.is_object() || version == root.end() || !version->is_number_integer() || *version != kSchemaVersion ||
        !root.contains("load_order") || !root["load_order"].is_array()) {
        throw std::runtime_error("expected schema_version 1 and a load_order array");
    }
    std::vector<std::shared_ptr<const vfs::Mount>> configured;
    for (const auto &entry : root["load_order"]) {
        if (!entry.is_string()) {
            throw std::runtime_error("load_order entries must be pack names");
        }
        const auto                 &folder = entry.get_ref<const std::string &>();
        const std::filesystem::path name = PathsFromUtf8(folder);
        if (!vfs::SafeRelative(folder) || std::distance(name.begin(), name.end()) != 1) {
            throw std::runtime_error("load_order contains an unsafe pack name");
        }
        std::error_code error;
        if (!std::filesystem::is_directory(directory / name, error)) {
            Diagnostic(directory / name, "pack is missing or not a folder; skipped");
            continue;
        }
        configured.push_back(vfs::OpenDirectory(directory / name, true));
    }
    return configured;
}

} // namespace

void Load(const std::filesystem::path &data_root) {
    Clear();
    std::error_code error;
    const auto      data = std::filesystem::weakly_canonical(data_root, error);
    if (error) {
        Diagnostic(data_root, "cannot establish the data location; packs disabled");
        return;
    }
    const auto directory = data.parent_path() / "datapacks";
    const auto config = directory / "datapacks.json";
    if (!std::filesystem::exists(config, error)) {
        return;
    }
    try {
        packs = ReadConfig(directory, config);
    } catch (const std::exception &exception) {
        Diagnostic(config, std::string(exception.what()) + "; packs disabled");
    }
}

void Clear() {
    packs.clear();
}

std::vector<vfs::Ref> Find(std::string_view relative) {
    std::vector<vfs::Ref> found;
    if (!vfs::SafeRelative(relative)) {
        return found;
    }
    for (const auto &pack : std::views::reverse(packs)) {
        if (auto file = vfs::Find(pack, relative)) {
            found.push_back(std::move(file));
        }
    }
    return found;
}

} // namespace dc::platform::datapacks
