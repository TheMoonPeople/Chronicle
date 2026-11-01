#include "journal/heard.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <print>

namespace dc::journal {

namespace {

constexpr std::uint32_t kVersion = 1;
constexpr int           kTowns = 5;
constexpr int           kVillagers = 16;

// A bit per villager of each town, set once their request is heard.
std::array<std::uint16_t, kTowns> g_requests_heard{};

bool Valid(int town, int villager) {
    return town >= 0 && town < kTowns && villager >= 0 && villager < kVillagers;
}

} // namespace

void NoteTalkLine(int town, int villager, int message) {
    if (Valid(town, villager) && message == RequestMessage(villager)) {
        g_requests_heard[town] |= static_cast<std::uint16_t>(1U << villager);
    }
}

bool RequestHeard(int town, int villager) {
    return Valid(town, villager) && (g_requests_heard[town] & (1U << villager)) != 0;
}

std::optional<save::ExtensionChunk> SaveChunk() {
    if (std::ranges::all_of(g_requests_heard, [](std::uint16_t bits) { return bits == 0; })) {
        return std::nullopt;
    }
    save::ExtensionChunk chunk{
        .tag = {'J', 'R', 'N', 'L'},
          .version = kVersion
    };
    for (std::uint16_t bits : g_requests_heard) {
        chunk.payload.push_back(static_cast<std::uint8_t>(bits & 0xFF));
        chunk.payload.push_back(static_cast<std::uint8_t>(bits >> 8));
    }
    return chunk;
}

void LoadChunk(const save::ExtensionChunk *chunk) {
    g_requests_heard = {};
    if (chunk == nullptr) {
        return;
    }
    if (chunk->version != kVersion || chunk->payload.size() != g_requests_heard.size() * 2) {
        std::println(stderr, "journal: the save's heard lines cannot be read (version {}); nothing is heard", chunk->version);
        return;
    }
    for (std::size_t town = 0; town < g_requests_heard.size(); town++) {
        g_requests_heard[town] = static_cast<std::uint16_t>(chunk->payload[town * 2] | (chunk->payload[(town * 2) + 1] << 8));
    }
}

void RegisterSaveChunk() {
    save::ExtensionRegister("JRNL", [](game::CSaveData &) { return SaveChunk(); }, LoadChunk);
}

} // namespace dc::journal
