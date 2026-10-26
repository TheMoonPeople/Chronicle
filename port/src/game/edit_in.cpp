#include "common.h"
#include "edit_in.hpp"

#include <libvu0.h>

#include <array>
#include <iterator>
#include <string_view>

#include "dataalloc.hpp"
#include "editloop3.hpp"
#include "editpartsinfo.hpp"
#include "fireomni.hpp"
#include "frame.hpp"
#include "framevu1.hpp"
#include "mapparts.hpp"
#include "mds.hpp"

namespace {

// Never drawn, but its constructor draws from rand() during static initialisation, which the
// game's random sequence depends on.
CFireOmni Fire;

} // namespace

EDIT_IN_INFO *EdInInfo;

/**
 * Splits one element's space-separated object names into the caller's name buffers.
 */
void GetElementObjName(EDITPARTS_INFO *info, char **names, int element, int mode) {
    names[0][0] = '\0';

    if (info == nullptr) {
        return;
    }

    if (element < 0 || element >= std::ssize(info->elements)) {
        return;
    }

    if (mode < 0 || mode >= std::ssize(info->elements[element].names)) {
        return;
    }

    const char *text = info->elements[element].names[mode];

    if (text == nullptr) {
        return;
    }

    int word = 0;
    int length = 0;

    for (const char c : std::string_view(text)) {
        if (c == ' ') {
            names[word][length] = '\0';
            length = 0;
            word++;
        } else {
            names[word][length] = c;
            length++;
        }
    }

    names[word][length] = '\0';
    names[word + 1][0] = '\0';
}

void LoadMapObject(CMapParts *parts, u_int **data, CDataAlloc2<1> *alloc) {
    std::array<CFrameVu1 *, 4> frames{};

    LoadMDSFileLOD(frames.data(), data, alloc, 1);
    CFrameAttr attr;
    attr.fog_enable = true;
    attr.clip_enable = false;
    attr.alpha_ref = 0x40;

    for (int lod = 1; lod < std::ssize(frames); lod++) {
        if (frames[lod] != nullptr) {
            frames[lod]->SetAttr(attr, 1, 0x44);
            SetFrameAttr(frames[lod], 1);
        }
    }

    attr.clip_depth = 100.0f;
    attr.clip_enable = true;
    frames[0]->SetAttr(attr, 1, 0);
    SetFrameAttr(frames[0], 1);

    for (int lod = 0; lod < std::ssize(frames); lod++) {
        parts->SetFrame(frames[lod], lod);
    }

    if (data[4] != nullptr) {
        parts->shadow_frame = LoadMDSFile(data[4], alloc, 14, nullptr, nullptr);
    }

    if (data[5] != nullptr) {
        parts->collision_frame = LoadCollisionFile(data[5], alloc);
    } else {
        parts->collision_frame = nullptr;
    }

    if (data[6] != nullptr) {
        parts->shade_frame = LoadMDSFile(data[6], alloc, 0, nullptr, nullptr);
    }

    if (data[7] != nullptr) {
        parts->ripple_frame = LoadMDSFile(data[7], alloc, 0, nullptr, nullptr);
    }

    if (data[8] != nullptr) {
        parts->camera_frame = LoadCollisionFile(data[8], alloc);
    }
}

/**
 * Builds one interior part from its archive entry.
 */
int LoadPTS(CMapParts *parts, u_int *archive) {
    const auto *pack = reinterpret_cast<const EPARTS_ARCHIVE *>(archive);
    auto       *bytes = reinterpret_cast<u8 *>(archive);
    const auto  chunk = [bytes](s32 offset) { return reinterpret_cast<u_int *>(bytes + offset); };
    const auto *header = reinterpret_cast<const EPARTS_INFO_HEADER *>(bytes + pack->info_offset);

    if (pack->lod0_size <= 0) {
        return 0;
    }

    u_int *collision = pack->collision_size > 0 ? chunk(pack->collision_offset) : nullptr;
    u_int *camera = pack->camera_size > 0 ? chunk(pack->camera_offset) : nullptr;

    parts->SetFrame(LoadMDSFile(chunk(pack->lod0_offset), &EdNPCBuffer, 0, nullptr, nullptr), 0);

    if (collision != nullptr) {
        parts->collision_frame = LoadCollisionFile(collision, &EdNPCBuffer);
    }

    if (camera != nullptr) {
        parts->camera_frame = LoadCollisionFile(camera, &EdNPCBuffer);
    } else {
        parts->camera_frame = nullptr;
    }

    sceVu0FVECTOR position = {header->position[0], header->position[1], header->position[2], 1.0f};
    parts->SetPosition(position);
    parts->SetRotation(header->rotation[0], header->rotation[1], header->rotation[2]);
    return 0;
}
