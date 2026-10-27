#pragma once

#include "platform/discord.hpp"

namespace dc::app {

/**
 * Snapshot of the game state that Discord rich presence is derived from.
 * Negative values mean "unknown" for the corresponding field.
 */
struct PresenceState {
    int  mode = -1;              /**< Top-level game mode (GAME_MODE_*). */
    int  map_no = -1;            /**< Town map index while in edit mode. */
    int  edit_mode = -1;         /**< Town sub-mode (ED_MODE_*). */
    int  dungeon = -1;           /**< Selected dungeon index. */
    int  floor = -1;             /**< Zero-based current dungeon floor. */
    bool back_floor = false;     /**< Whether the player is in the back floors. */
    bool choosing_floor = false; /**< Whether the dungeon entrance floor menu is open. */
    int  chara = 0;              /**< Active character (CHARA_*). */
};

/**
 * Builds the Discord activity that describes the given game state.
 * Out-of-range dungeon, town and character indices fall back to generic text.
 */
platform::DiscordActivity PresenceDescribe(const PresenceState &state);

/**
 * Samples the live game state once per call and publishes the resulting activity
 * after it has stayed unchanged for a short settle period; loading and saving modes are ignored.
 */
void PresenceTick();

} // namespace dc::app
