#pragma once

#include <cstddef>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "audio/hdbank.hpp"
#include "audio/sq.hpp"

/**
 * The sound dcdata publishes beside the disc's packs (docs/AUDIO_FRAMEWORK.md), which the game plays in
 * place of the packs' own banks and sequences. Every file is looked for in the active datapacks first,
 * then in the game's data, so a pack can replace a bank, a sequence or a single sample.
 */
namespace dc::sound {

/**
 * A pack's driver settings, as its .cfg script sets them (REVERBE, TABLE).
 */
struct PublishedConfig {
    int reverb_mode = 0;
    int reverb_depth = 0;
    int se_table = 0;
    int se_table_type = 0;
};

/**
 * One load list of a published pack: the document each member it loads was published as.
 */
struct PublishedList {
    std::string                        document;  /**< The pack's document, for diagnostics. */
    std::map<std::string, std::string> banks;     /**< A bank member (lower case) and its bank document. */
    std::map<std::string, std::string> sequences; /**< A sequence member (lower case) and its document. */
    std::optional<PublishedConfig>     config;    /**< The list's .cfg settings, when the pack has them. */
};

/**
 * The published load list named list of the pack that member, a member read out of a loaded .snd,
 * belongs to; nothing when the pack has no published document or it has no such list.
 */
std::optional<PublishedList> FindList(const void *member, std::size_t size, std::string_view list);

/**
 * The bank a bank document describes, its samples read as WAV; null when it cannot be read.
 */
std::shared_ptr<audio::Bank> LoadBank(const std::string &document);

/**
 * The sequence a sequence document describes; null when it cannot be read.
 */
std::shared_ptr<audio::SqFile> LoadSequence(const std::string &document);

/**
 * A row of the published sequence volume table (sound/tbl/sqtbl.json).
 */
struct SequenceVolume {
    std::string name;
    int         volume = 0;
};

/**
 * A row of the published effect volume table (sound/tbl/setbl.json).
 */
struct EffectVolume {
    int bank = 0;
    int program = 0;
    int volume = 0;
};

/**
 * The published rows of a volume table the game reads as text (sound/tbl/sqtbl.txt and setbl.txt);
 * nothing when it has no published table.
 */
std::optional<std::vector<SequenceVolume>> SequenceVolumes(std::string_view table);
std::optional<std::vector<EffectVolume>>   EffectVolumes(std::string_view table);

} // namespace dc::sound
