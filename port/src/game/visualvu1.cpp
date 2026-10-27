#include "stubs/visualvu1.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <span>
#include <string_view>
#include <utility>

#include "dataalloc.hpp"
#include "dataset.hpp"
#include "mathutil.hpp"
#include "mdt.hpp"
#include "mglib.hpp"
#include "renderinfo.hpp"
#include "texture.hpp"
#include "tim2.hpp"
#include "visual.hpp"

namespace {

// Rounds a texture dimension up to the next power of two and gives its exponent.
int CeilLog2(int size) {
    int log2 = 0;

    for (int rest = size; rest >= 2; rest >>= 1) {
        log2++;
    }

    if (size != (1 << log2)) {
        log2++;
    }

    return log2;
}

int SetShadowData(u_int *packet, float (*matrix)[4]) {
    packet[0] = 0;
    packet[1] = 0;
    packet[2] = 0;
    packet[3] = 0x6C080029;
    packet[4] = 0x8001;
    packet[5] = 0x102E8000;
    packet[6] = 0xE;
    packet[7] = 0;
    packet[8] = 0x68;
    packet[9] = 0x80;
    packet[10] = 0x42;
    packet[11] = 0;
    packet[12] = 0x8001;
    packet[13] = 0x102E8000;
    packet[14] = 0xE;
    packet[15] = 0;
    packet[16] = 0x62;
    packet[17] = 0x80;
    packet[18] = 0x42;
    packet[19] = 0;

    auto       *quads = reinterpret_cast<u_long128 *>(packet);
    const auto *rows = reinterpret_cast<const u_long128 *>(matrix);
    quads[5] = rows[0];
    quads[6] = rows[1];
    quads[7] = rows[2];
    quads[8] = rows[3];
    return 0x24;
}

int SetMaterial(u_int *packet, MDT_MATERIAL *material) {
    if (material == nullptr) {
        return 0x14;
    }

    packet[0] = 0;
    packet[1] = 0;
    packet[2] = 0;
    packet[3] = 0x6C040025;
    u_int last_row[4] = {3, 0, 0, 0};
    auto *quads = reinterpret_cast<u_long128 *>(packet);
    quads[1] = *reinterpret_cast<u_long128 *>(material->diffuse);
    quads[2] = *reinterpret_cast<u_long128 *>(material->ambient);
    quads[3] = *reinterpret_cast<u_long128 *>(material->specular);
    quads[4] = *reinterpret_cast<u_long128 *>(last_row);
    return 0x14;
}

int SetTEX0(u_int *packet, u_long tex0, u_long tex1) {
    packet[0] = 0;
    packet[1] = 0;
    packet[2] = 0;

    if (tex1 == 0) {
        tex1 = std::bit_cast<u_long>(mgTEX1Env);
    }

    if (tex1 != 0) {
        packet[3] = 0x50000003;
        packet[4] = 0x8002;
    } else {
        packet[3] = 0x50000002;
        packet[4] = 0x8001;
    }

    packet[5] = 0x10000000;
    packet[6] = 0xE;
    packet[7] = 0;

    packet[8] = static_cast<u_int>(tex0);
    packet[9] = static_cast<u_int>((tex0 >> 32) & 0xFFFFFFFF);
    packet[10] = SCE_GS_TEX0_1;
    packet[11] = 0;

    if (tex1 != 0) {
        packet[12] = static_cast<u_int>(tex1);
        packet[13] = static_cast<u_int>((tex1 >> 32) & 0xFFFFFFFF);
        packet[14] = SCE_GS_TEX1_1;
        packet[15] = 0;
        return 0x10;
    }

    return 0xC;
}

// Writes one vector-unit upload header followed by the gathered vertex streams and gives back
// its length in quadwords.
int SetVuData(int count, u_long128 *block, u_int *index, u_long128 *vertex, u_long128 *normal, u_long128 *uv, u_long128 *colour, int prim) {
    auto *words = reinterpret_cast<u_int *>(block);

    words[0] = count | 0x8000;
    words[1] = prim != 4 ? 0x302DC000 : 0x302E4000;
    words[2] = 0x412;
    words[3] = 0;
    words[4] = count;
    words[5] = prim;
    words[6] = colour != nullptr ? 0x100 : 0;

    const int vertex_start = 2;
    const int normal_start = vertex_start + count;
    const int uv_start = normal_start + count;
    const int colour_start = uv_start + count;
    int       next_index = 0;

    for (int out = 0; out < count; out++) {
        block[vertex_start + out] = vertex[index[next_index]];
        block[normal_start + out] = normal[index[next_index + 1]];
        block[uv_start + out] = uv[index[next_index + 2]];
        next_index += 3;

        if (colour != nullptr) {
            block[colour_start + out] = colour[index[next_index]];
            next_index++;
        }
    }

    if (colour != nullptr) {
        return count * 4 + 2;
    }

    return count * 3 + 2;
}

} // namespace

void SetTextureInfo(CTexture *tex, char *name, TM2_head *head) {
    auto     *picture = reinterpret_cast<TM2_picture *>(reinterpret_cast<u_char *>(head) + 16);
    const int width = head->image_width;
    const int height = head->image_height;
    int       bpp;
    int       psm = SCE_GS_PSMCT32;

    switch (head->image_type) {
        case TIM2_RGB16:
            psm = SCE_GS_PSMCT16;
            bpp = 2;
            break;
        case TIM2_RGB24:
            psm = SCE_GS_PSMCT24;
            bpp = 3;
            break;
        case TIM2_RGB32:
            bpp = 4;
            break;
        case TIM2_IDTEX4:
            psm = SCE_GS_PSMT4;
            bpp = 0;
            break;
        case TIM2_IDTEX8:
            psm = SCE_GS_PSMT8;
            bpp = 1;
            break;
    }

    u_char *image = reinterpret_cast<u_char *>(picture) + picture->header_size;
    u_char *clut = bpp < 2 ? image + picture->image_size : nullptr;

    std::array<u_char *, 4> mip{};

    if (picture->mipmap_count > 1) {
        u_char *next = image + picture->mipmap_size[0];

        for (int level = 1; std::cmp_less(level, picture->mipmap_count); level++) {
            mip[level] = next;
            next += picture->mipmap_size[level];
        }
    }

    const std::string_view source_name{name};
    std::ranges::copy(source_name, tex->name);
    tex->name[source_name.size()] = '\0';
    tex->width = width;
    tex->height = height;
    tex->bpp = bpp;
    tex->image[0] = reinterpret_cast<u_int *>(image);

    for (int level = 1; level < 4; level++) {
        tex->image[level] = reinterpret_cast<u_int *>(mip[level]);
    }

    tex->clut = reinterpret_cast<u_int *>(clut);

    const int width_log2 = CeilLog2(width);
    const int height_log2 = CeilLog2(height);
    const int buffer_width = std::max(width >> 6, 1);

    if (tex->bpp > 1) {
        tex->tex0 = SCE_GS_SET_TEX0(0, buffer_width, psm, width_log2, height_log2, 1, 0, 0, 0, 0, 0, 0);
    } else {
        tex->tex0 = SCE_GS_SET_TEX0(0, buffer_width, psm, width_log2, height_log2, 1, 0, 0, 0, 0, 0, 1);
    }
}

void SetTextureInfo(CTexture *tex, char *name, u_char *buffer) {
    auto                *head = reinterpret_cast<IMG_head *>(buffer);
    std::span<IMG_entry> entries{reinterpret_cast<IMG_entry *>(head + 1), head->pictures};

    for (IMG_entry &entry : entries) {
        if (name != nullptr && name[0] != '\0' && std::string_view{entry.name} != std::string_view{name}) {
            continue;
        }

        SetTextureInfo(tex, entry.name, reinterpret_cast<TM2_head *>(buffer + entry.offset));
    }
}

void CVisual::Initialize() {
    unk_00 = 0;
    unk_04 = 0;
}

CVisual::CVisual() {
    Initialize();
}

int CVisualVu1::RemakeData(unsigned int * /*data*/) {
    return 0;
}

void CVisualVu1::Initialize() {
    CVisual::Initialize();
    vu_data = nullptr;
    vu_size = 0;
    unk_00 = 0;
    unk_04 = 0;
}

CVisualVu1::CVisualVu1() {
    Initialize();
}

void CVisualMDTVu1::SetMDTDataAddress(u_int *data) {
    this->data = data;
}

u_int *CVisualMDTVu1::GetMDTDataAddress() {
    return data;
}

void CVisualMDTVu1::Initialize() {
    CVisualVu1::Initialize();
    copy_on_draw = false;
    data = nullptr;
    vu_data_buffer[1] = nullptr;
    vu_data_buffer[0] = nullptr;
}

CVisualMDTVu1::CVisualMDTVu1() {
    CVisualMDTVu1::Initialize();
}
