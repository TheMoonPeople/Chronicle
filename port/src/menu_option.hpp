#pragma once

#include <span>
#include <string>
#include <utility>
#include <vector>

#include "platform/config.hpp"
#include "platform/window.hpp"

class CSaveData;

// Whether the Options screen is open, from InitMenuOption until MenuOptionKey reports it closed.
// The host keeps the interface at 100% meanwhile: video.ui_scale applies from when it closes.
bool MenuOptionOpen();

// Every string the Options screen draws that is its own, not the game's: key and English, as
// localize.hpp's LocalizeText takes them (options.<setting>.label, .help, .choice.<n>, options.page.<name>...).
// The export lists them in every language's file; port/lang/ ships their translations.
std::vector<std::pair<std::string, std::string>> OptionStrings();

// Puts the game's options from config.json into the save, where the game reads them (its
// configuration words, the dungeon map's status and the menu cursors' reset flag), and sets the
// sound's stereo mode. A loaded save brings its own copy, so this runs before every mode starts
// and whenever config.json's options change.
void GameOptionsApply(const ConfigGameOptions &options);

// The change hook that calls GameOptionsApply.
void GameOptionsChanged(const Config &before, const Config &after);

// Zeros the option fields reserved in a serialized save's fixed layout. Only the copy about to
// be written is cleared; the running game keeps config.json's options.
void GameOptionsClear(CSaveData &save);

// A window size the Resolution row can choose; 0 by 0 is the monitor's own (Desktop).
struct OptionResolution {
    int width;
    int height;
};

// The sizes the Resolution row steps through: Desktop first, then the curated sizes, the display's
// own modes (none smaller than 800 by 600) and the configured size, from the smallest area up and
// each once. With the display known (its width and height given), only sizes that fit it are
// listed, and the configured one always is, so opening the row never moves the window off it.
std::vector<OptionResolution> OptionResolutionList(std::span<const DisplayModeSize> modes, int configured_width,
                                                   int configured_height, bool display_known, int display_width,
                                                   int display_height);

// The Controls row's reset presets: Middle Mouse, Mouse4, Mouse5, Home and Disabled, followed by
// the configured custom binding when present. Selecting a preset changes only zoom_reset.
int         OptionZoomResetChoice(const Config &config);
int         OptionZoomResetCount(const Config &config);
std::string OptionZoomResetText(const Config &config);
void        OptionSetZoomReset(Config &config, int choice);
void        OptionRestoreZoomReset(Config &config, const Config &defaults);
