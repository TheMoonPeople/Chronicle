#pragma once

#include <span>
#include <string>
#include <utility>
#include <vector>

#include "platform/config.hpp"
#include "platform/window.hpp"

namespace dc::game {
class CSaveData;
} // namespace dc::game

namespace dc::options {

/**
 * Returns whether the Options screen is open, from InitMenuOption until MenuOptionKey reports it
 * closed. The host keeps the interface at 100% meanwhile; video.ui_scale applies once it closes.
 */
bool MenuOptionOpen();

/**
 * Returns every string the Options screen draws that is its own, not the game's, as key and English
 * pairs in the form LocalizeText takes them (options.<setting>.label, .help, .choice.<n>,
 * options.page.<name>...). The export lists them in every language's file.
 */
std::vector<std::pair<std::string, std::string>> OptionStrings();

/**
 * Puts the game's options from config.json into the save, where the game reads them (its
 * configuration words, the dungeon map's status and the menu cursors' reset flag), and sets the
 * sound's stereo mode. Runs before every mode starts and whenever config.json's options change.
 */
void GameOptionsApply(const platform::ConfigGameOptions &options);

/**
 * Config change hook that calls GameOptionsApply
 * when the game options differ between the two configs.
 */
void GameOptionsChanged(const platform::Config &before, const platform::Config &after);

/**
 * Zeros the option fields reserved in a serialized save's fixed layout. Only the copy about to be
 * written is cleared; the running game keeps config.json's options.
 */
void GameOptionsClear(game::CSaveData &save);

/**
 * A window size the Resolution row can choose;
 * 0 by 0 is the monitor's own (Desktop).
 */
struct OptionResolution {
    int width;  /**< Width in pixels. */
    int height; /**< Height in pixels. */
};

/**
 * Returns the sizes the Resolution row steps through: Desktop first, then the curated sizes, the
 * display's own modes (none smaller than 800 by 600) and the configured size, from the smallest area
 * up and each once. With the display known, only sizes that fit it are listed, plus the configured one.
 */
std::vector<OptionResolution> OptionResolutionList(std::span<const DisplayModeSize> modes, int configured_width,
                                                   int configured_height, bool display_known, int display_width,
                                                   int display_height);

/**
 * The Controls row's zoom reset presets: Middle Mouse, Mouse4, Mouse5, Home and Disabled, followed
 * by the configured custom binding when present. Selecting a preset changes only zoom_reset.
 */
int         OptionZoomResetChoice(const platform::Config &config);
int         OptionZoomResetCount(const platform::Config &config);
std::string OptionZoomResetText(const platform::Config &config);
void        OptionSetZoomReset(platform::Config &config, int choice);
void        OptionRestoreZoomReset(platform::Config &config, const platform::Config &defaults);

/**
 * The Controls row's presets for the camera's vertical return rate:
 * current choice, choice count, display text, selection and restore.
 */
int         OptionCameraReturnChoice(const platform::Config &config);
int         OptionCameraReturnCount(const platform::Config &config);
std::string OptionCameraReturnText(const platform::Config &config);
void        OptionSetCameraReturn(platform::Config &config, int choice);
void        OptionRestoreCameraReturn(platform::Config &config, const platform::Config &defaults);

} // namespace dc::options
