#include "save/save_state.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "platform/files.hpp"

namespace dc::save {

namespace {

using Json = nlohmann::ordered_json;

// Far more than the file ever holds; larger files are rejected.
constexpr std::size_t  kMaxSize = 0x10000;
constexpr std::int64_t kMaxSaveFolder = 999999999;

} // namespace

bool SaveStateRead(const std::filesystem::path &path, SaveState &state) {
    std::string text(kMaxSize + 1, '\0');
    text.resize(FilesRead(path, text.data(), text.size()));
    if (text.size() > kMaxSize) {
        return false;
    }
    Json json = Json::parse(text, nullptr, false);
    if (json.is_discarded() || !json.is_object()) {
        return false;
    }
    if (auto last = json.find("last_save"); last != json.end()) {
        if (last->is_number_unsigned()) {
            auto value = last->get<std::uint64_t>();
            if (value <= static_cast<std::uint64_t>(kMaxSaveFolder)) {
                state.last_save = static_cast<int>(value);
            }
        } else if (last->is_number_integer()) {
            auto value = last->get<std::int64_t>();
            if (value >= 0 && value <= kMaxSaveFolder) {
                state.last_save = static_cast<int>(value);
            }
        }
    }
    if (auto clear = json.find("game_clear"); clear != json.end() && clear->is_boolean()) {
        state.game_clear = clear->get<bool>();
    }
    return true;
}

bool SaveStateWrite(const std::filesystem::path &path, const SaveState &state) {
    if (FilesCreateDirectory(path.parent_path()) == FilesResult::kFailed) {
        return false;
    }
    Json json;
    json["last_save"] = state.last_save;
    json["game_clear"] = state.game_clear;
    std::string text = json.dump(4) + "\n";
    return FilesWrite(path, text.data(), text.size(), true) == FilesResult::kWritten;
}

bool SaveStateSeed(const std::filesystem::path &source, const std::filesystem::path &destination, std::string &error) {
    namespace fs = std::filesystem;
    std::error_code code;
    error.clear();
    auto fail = [&](const char *message) {
        error = message;
        if (code) {
            error += ": " + code.message();
        }
        return false;
    };
    if (source.empty() || destination.empty()) {
        return fail("source and destination directories are required");
    }
    const auto source_status = fs::symlink_status(source, code);
    if (!fs::is_directory(source_status) || code) {
        return fail("seed source is not a readable directory");
    }
    auto resolved = [&](const fs::path &path) {
#if defined(__SWITCH__)
        // Mounted devices are absolute to devoptab, but not to std::filesystem's POSIX parser.
        if (path.begin() != path.end() && path.begin()->string().ends_with(':')) {
            return path.lexically_normal();
        }
#endif
        return fs::weakly_canonical(path, code);
    };
    const fs::path from = resolved(source);
    if (code) {
        return fail("cannot resolve seed source");
    }
    const fs::path to = resolved(destination);
    if (code) {
        return fail("cannot resolve seed destination");
    }
    auto [from_part, to_part] = std::mismatch(from.begin(), from.end(), to.begin(), to.end());
    if (from_part == from.end() || to_part == to.end()) {
        return fail("seed source and destination must not overlap");
    }
    const bool exists = fs::exists(destination, code);
    if (code) {
        return fail("cannot inspect seed destination");
    }
    if (exists && (!fs::is_directory(destination, code) || code || !fs::is_empty(destination, code) || code)) {
        return fail("seed destination must be a new or empty directory");
    }
    std::vector<std::pair<fs::path, bool>> entries;
    fs::recursive_directory_iterator       entry(source, code), end;
    for (; !code && entry != end; entry.increment(code)) {
        const auto status = entry->symlink_status(code);
        if (code || (!fs::is_directory(status) && !fs::is_regular_file(status))) {
            return fail("seed source must contain only directories and regular files");
        }
        entries.emplace_back(entry->path().lexically_relative(source), fs::is_directory(status));
    }
    if (code) {
        return fail("cannot inspect seed source");
    }
    fs::create_directories(destination, code);
    if (code) {
        return fail("cannot create seed destination");
    }
    std::vector<char> buffer(64 * 1024);
    for (const auto &[relative, directory] : entries) {
        if (directory) {
            fs::create_directories(destination / relative, code);
            if (code) {
                return fail("cannot create seed subdirectory; destination may be partial");
            }
            continue;
        }
        std::ifstream input(source / relative, std::ios::binary);
        // Do not inherit RomFS permissions, and never truncate a file created since the empty check.
        std::ofstream output(destination / relative, std::ios::binary | std::ios::out | std::ios::noreplace);
        if (!input || !output) {
            return fail("cannot open seed file; destination may be partial");
        }
        while (input && output) {
            input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
            output.write(buffer.data(), input.gcount());
        }
        if (!input.eof() || !output) {
            return fail("cannot copy seed file; destination may be partial");
        }
        output.close();
        if (!output) {
            return fail("cannot finish seed file; destination may be partial");
        }
    }
    return true;
}

} // namespace dc::save
