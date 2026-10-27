#pragma once

#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "platform/config.hpp"

namespace dc::options {

/**
 * One line of the options screen: a setting, a key binding or an action, with its strings and handlers.
 */
struct Row {
    const char *key = nullptr;                                                             /**< config.json name; options.<key> prefixes its strings. */
    const char *label = nullptr;                                                           /**< English label. */
    const char *help = nullptr;                                                            /**< English help text, or nullptr when the game's is used. */
    int         game_help = -1;                                                            /**< Game help message id, or -1. */
    int (*count)(const platform::Config &) = nullptr;                                      /**< Number of choices. */
    int (*get)(const platform::Config &) = nullptr;                                        /**< Current choice. */
    void (*set)(platform::Config &, int choice) = nullptr;                                 /**< Applies a choice. */
    std::string (*text)(const platform::Config &) = nullptr;                               /**< Value text when not one of names. */
    const char *names = nullptr;                                                           /**< English choice names separated by '|'. */
    void (*restore)(platform::Config &config, const platform::Config &defaults) = nullptr; /**< Reset when set(get(defaults)) is not enough. */
    const char *action = nullptr;                                                          /**< Input action a binding row edits, else nullptr. */
    void (*activate)() = nullptr;                                                          /**< Runs on confirm for an action row, else nullptr. */
};

/**
 * One tab of the options screen and the rows it lists.
 */
struct Page {
    const char          *name; /**< English tab name. */
    const char          *help; /**< English help text. */
    std::span<const Row> rows; /**< Rows in display order. */
};

/**
 * Every page of the options screen, in tab order.
 */
std::span<const Page> Pages();

/**
 * Localisation key of a page's name: options.page.<name in lower case>; its help adds .help.
 */
std::string PageKey(const char *name);

/**
 * Every string the rows draw themselves, as the key and English pairs LocalizeText takes.
 */
std::vector<std::pair<std::string, std::string>> RowStrings();

/**
 * Value text a row shows, with " *" appended when the setting applies at the next start.
 */
std::string RowValue(const Row &row, const platform::Config &config);

/**
 * Moves a setting one choice left or right, wrapping from end to end when asked. Returns whether it moved.
 */
bool StepRow(const Row &row, platform::Config &config, int direction, bool wrap);

/**
 * Puts every setting of the page back to its default.
 */
void ResetPage(const Page &page, platform::Config &config);

/**
 * Binds an action to a single key or button name ("F", "Mouse2", "Gamepad:paddle2"), replacing its keys.
 */
void SetBinding(platform::Config &config, std::string_view action, std::string_view key);

/**
 * Rebuilds the window sizes the Resolution row offers for the window's monitor.
 */
void ListResolutions();

} // namespace dc::options
