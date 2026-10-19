#pragma once

#include "audio/hdbank.hpp"
#include "audio/sq.hpp"

namespace chronicle_audio {
// Convert the retail data Chronicle already loads into official Kazarov's semantic inputs.
// The returned bank/sequence owns its data; no filesystem writes or private renderer are needed.
std::shared_ptr<audio::Bank>   CreateBank(std::span<const std::uint8_t> hd, std::span<const std::uint8_t> bd);
std::shared_ptr<audio::SqFile> CreateSequence(std::span<const std::uint8_t> sq);
} // namespace chronicle_audio
