#include "dispctrl.hpp"

#include <algorithm>
#include <string_view>

#include "bound.hpp"
#include "mglib.hpp"
#include "rect.hpp"

namespace {

/**
 * Stores the fixed header at the front of a PTS archive.
 */
struct PTS_HEADER {
    int unk_00;
    int count; /**< Number of file records in the archive. */
    int unk_08[2];
};

STATIC_ASSERT(sizeof(PTS_HEADER) == 0x10);

/**
 * Stores one named file record in a PTS archive.
 */
struct PTS_ENTRY {
    char name[16]; /**< Null-terminated file name. */
    int  offset;   /**< Byte offset of the file from the archive start. */
    int  unk_14[7];
};

STATIC_ASSERT(sizeof(PTS_ENTRY) == 0x30);

const PTS_ENTRY *PtsEntries(const PTS_HEADER *header) {
    return reinterpret_cast<const PTS_ENTRY *>(header + 1);
}

u_int *PtsFile(u_int *archive, const PTS_ENTRY &entry) {
    return reinterpret_cast<u_int *>(reinterpret_cast<char *>(archive) + entry.offset);
}

void DrawCurtain(float rate) {
    for (s32 x = 0; x < 640; x += 64) {
        for (s32 y = 0; y < SCREEN_HALF_HEIGHT; y += 32) {
            // Coordinates and sizes are in 12.4 fixed point.
            CRect_i_ rect;
            rect.x = x * 16;
            rect.y = y * 16;
            rect.width = 1024;
            rect.height = 512;
            MGFillBox(rect, 0, 0, 0, static_cast<u8>(rate));
        }
    }
}

} // namespace

int CBound::Sizeof() {
    return sizeof(CBound);
}

u_int *SearchPTS(u_int *archive, char *name) {
    const auto *header = reinterpret_cast<const PTS_HEADER *>(archive);

    if (header == nullptr) {
        return nullptr;
    }

    std::string_view base_name{name};
    if (const auto slash = base_name.rfind('/'); slash != std::string_view::npos) {
        base_name.remove_prefix(slash + 1);
    }

    const PTS_ENTRY *entries = PtsEntries(header);
    for (int i = 0; i < header->count; i++) {
        if (base_name == entries[i].name) {
            return PtsFile(archive, entries[i]);
        }
    }

    return nullptr;
}

u_int *SearchPTS(u_int *archive, int index) {
    const auto *header = reinterpret_cast<const PTS_HEADER *>(archive);

    if (header == nullptr) {
        return nullptr;
    }

    if (index >= header->count) {
        return nullptr;
    }

    return PtsFile(archive, PtsEntries(header)[index]);
}

void CDispCtrl::FadeOutStart(float fade_speed) {
    speed = fade_speed;
    rate = 0.0f;
    mode = DispFadeMode::FadeOut;
}

void CDispCtrl::FadeInStart(float fade_speed) {
    speed = fade_speed;
    rate = 128.0f;
    mode = DispFadeMode::FadeIn;
}

void CDispCtrl::FadeInit(float start_rate) {
    rate = start_rate;
    mode = DispFadeMode::Fixed;
}

float CDispCtrl::GetRate() {
    return rate;
}

float CDispCtrl::FadeOut(sceVif1Packet * /*packet*/) {
    if (mode != DispFadeMode::FadeOut) {
        return rate;
    }

    if (rate == 0.0f && speed == 0.0f) {
        return rate;
    }

    rate += speed;

    rate = std::min(rate, 128.0f);

    DrawCurtain(rate);

    if (rate >= 128.0f) {
        rate = 128.0f;
        speed = 0.0f;
    }

    return rate;
}

float CDispCtrl::FadeIn(sceVif1Packet * /*packet*/) {
    if (mode != DispFadeMode::FadeIn) {
        return rate;
    }

    if (rate == 0.0f && speed == 0.0f) {
        return rate;
    }

    rate -= speed;

    rate = std::max(rate, 0.0f);

    DrawCurtain(rate);

    if (rate <= 0.0f) {
        rate = 0.0f;
        speed = 0.0f;
    }

    return rate;
}
