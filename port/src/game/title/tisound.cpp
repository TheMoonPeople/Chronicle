#include "common.h"

#include <algorithm>

#include "snd.hpp"
#include "sound.hpp"
#include "title/tisound.hpp"

namespace dc::game {

void TiPlayVolSE(int group, int no, int voice, float volume) {
    const int    se_index = CSnd.GetSeNo(no, voice);
    const short *se_info = CSnd.GetSeInfTbl();
    // Each SE table entry is a pair of shorts; the second is the base volume.
    const short base_volume = se_info[(se_index * 2) + 1];
    const int   level = std::clamp(static_cast<int>(static_cast<float>(base_volume) * volume), 0, 127);

    CSnd.SE_Play(group, no, voice, 64, 127, level, 0);
}

} // namespace dc::game
