#pragma once

#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "platform/config.hpp"

namespace options {

struct Row {
    const char *key = nullptr;
    const char *label = nullptr;
    const char *help = nullptr;
    int         game_help = -1;
    int (*count)(const Config &) = nullptr;
    int (*get)(const Config &) = nullptr;
    void (*set)(Config &, int choice) = nullptr;
    std::string (*text)(const Config &) = nullptr;
    const char *names = nullptr;
    void (*restore)(Config &config, const Config &defaults) = nullptr;
    // A binding row: the input action it edits. Its value is the action's current keys, and
    // pressing confirm starts listening for the next one. nullptr for every other row.
    const char *action = nullptr;
    // An action row runs when the player confirms it instead of changing a setting.
    void (*activate)() = nullptr;
};

struct Page {
    const char          *name;
    const char          *help;
    std::span<const Row> rows;
};

std::span<const Page> Pages();

// The key of a page's name: options.page.<name in lower case>; its help is that key with .help.
std::string PageKey(const char *name);

// Every string the settings rows draw that is their own: key and English, as LocalizeText takes them.
std::vector<std::pair<std::string, std::string>> RowStrings();

// The value a setting row shows, with " *" when it applies at the next start.
std::string RowValue(const Row &row, const Config &config);

// Moves a setting one choice left or right, or round from the last to the first. Returns whether it moved.
bool StepRow(const Row &row, Config &config, int direction, bool wrap);

// Puts every setting of the page back to its default.
void ResetPage(const Page &page, Config &config);

// Points the action of a binding row at one key or button name ("F", "Mouse2", "Gamepad:paddle2"),
// replacing whatever config.json had for it.
void SetBinding(Config &config, std::string_view action, std::string_view key);

// Rebuilds the list of window sizes the Resolution row offers, for the window's monitor.
void ListResolutions();

} // namespace options
