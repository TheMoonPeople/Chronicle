#pragma once

#include <string>

namespace dc::platform {

/**
 * Rich presence shown on the player's Discord profile; empty fields are omitted.
 */
struct DiscordActivity {
    std::string details;     /**< First line under the game name. */
    std::string state;       /**< Second line under the game name. */
    std::string large_image; /**< Asset key of the large image. */
    std::string large_text;  /**< Hover text of the large image. */

    bool operator==(const DiscordActivity &) const = default;
};

/**
 * Discord application id the port registers its presence under.
 */
inline constexpr const char *kDiscordClientId = "1556980864171245638";

/**
 * Starts the background thread that connects to the local Discord client over IPC.
 * Does nothing when client_id is empty or the thread is already running.
 */
void DiscordStart(std::string client_id);

/**
 * Sets the activity the background thread publishes, rate-limited to Discord's update budget.
 * The latest call wins; intermediate activities may never be sent.
 */
void DiscordSetActivity(const DiscordActivity &activity);

/**
 * Stops and joins the background thread, closing any open IPC connection.
 * Does nothing when the thread is not running.
 */
void DiscordStop();

/**
 * Builds the SET_ACTIVITY command JSON sent to Discord for an activity.
 * start_time is a Unix timestamp in seconds; nonce identifies the request.
 */
std::string DiscordActivityCommand(const DiscordActivity &activity, long long start_time, int pid, int nonce);

} // namespace dc::platform
