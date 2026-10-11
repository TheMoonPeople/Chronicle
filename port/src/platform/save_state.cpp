#include "save_state.hpp"

#include <nlohmann/json.hpp>

#include <cstdint>
#include <string>

#include "platform/files.hpp"

namespace {

using Json = nlohmann::ordered_json;

// Far more than the file ever holds.
constexpr std::size_t kMaxSize = 0x10000;

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
            if (value <= 999999999) {
                state.last_save = static_cast<int>(value);
            }
        } else if (last->is_number_integer()) {
            auto value = last->get<std::int64_t>();
            if (value >= 0 && value <= 999999999) {
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
