#include "common.h"

#include <cstring>

#include "character.hpp"
#include "dataalloc.hpp"
#include "dataread_port.hpp"
#include "dun/gameloop.hpp"
#include "mainitemmodel.hpp"
#include "mds.hpp"
#include "mglib.hpp"
#include "texture.hpp"

namespace {

/** Texture block of the first model cache slot. */
constexpr int kCashTextureBlock = 0x38;

/** Allocation granule of a cache area, in bytes. */
constexpr int kCashAllocUnit = 16;

} // namespace

int CMainItemModel::SetCashModel(int item_no, unsigned int *model_data, unsigned int *texture_data,
                                 int texture_size) {
    const int slot = GetFreeCashNo();
    if (slot == -1) {
        return -1;
    }

    const auto      texture_bytes = static_cast<std::size_t>(texture_size);
    DataReadSource  source;
    const bool      has_source = texture_data != nullptr && texture_size > 0 &&
                                 DataReadSourceRange(texture_data, texture_bytes, source);
    CDataAlloc2<1> &area = BtItemCashArea[slot];
    area.Reset();
    u_char *texture_copy = &area.base[static_cast<std::ptrdiff_t>(area.used) * kCashAllocUnit];
    area.Alloc((texture_size >> 4) + 1);
    std::memcpy(texture_copy, texture_data, texture_bytes);
    if (has_source) {
        DataReadRegisterCopiedSource(texture_copy, texture_bytes, &area, source);
    }
    TexManager.DeleteTextureBlock(slot + kCashTextureBlock);
    TexManager.CleanUpBuffer();
    LOADTEXTURE_INFO2 textures[2] = {};
    textures[0].name = reinterpret_cast<char *>(texture_copy);
    textures[0].block_no = slot + kCashTextureBlock;
    TexManager.LoadTextureBlockEX(slot + kCashTextureBlock, textures);
    cash[slot] = reinterpret_cast<u_int *>(LoadMDSFile(model_data, &area, 0, nullptr, nullptr));
    cash_lock[slot] = 1;
    cash_item[slot] = item_no;
    const int model_no = GetFreeModelNo();
    model[model_no] = ITEM_MODEL_LOADED;
    model_cash[model_no] = slot;
    return model_no;
}
