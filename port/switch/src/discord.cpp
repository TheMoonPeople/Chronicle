#include "platform/discord.hpp"

void DiscordStart(std::string) {}

void DiscordSetActivity(const DiscordActivity &) {}

void DiscordStop() {}

std::string DiscordActivityCommand(const DiscordActivity &, long long, int, int) { return {}; }
