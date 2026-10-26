#include "common.h"

#include "snd.hpp"
#include "sound.hpp"

void TiPlayVolSE(int group, int no, int voice, float volume) {
    short *se_info;
    short  base_volume;
    int    se_index;
    int    level;

    se_index = CSnd.GetSeNo(no, voice);
    se_info = CSnd.GetSeInfTbl();
    base_volume = se_info[se_index * 2 + 1];
    level = (int) ((float) base_volume * volume);

    if (level < 0) {
        level = 0;
    }

    if (level > 127) {
        level = 127;
    }

    CSnd.SE_Play(group, no, voice, 64, 127, level, 0);
}
