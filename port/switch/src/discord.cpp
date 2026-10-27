#include "platform/discord.hpp"

namespace dc::platform {

void DiscordStart(std::string) {}

void DiscordSetActivity(const DiscordActivity &) {}

void DiscordStop() {}

std::string DiscordActivityCommand(const DiscordActivity &, long long, int, int) { return {}; }

} // namespace dc::platform
