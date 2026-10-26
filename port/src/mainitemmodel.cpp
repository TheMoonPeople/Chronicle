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

int CMainItemModel::SetCashModel(int item_no, unsigned int *model_data, unsigned int *texture_data,
                                               int texture_size) {
    int slot = GetFreeCashNo();
    if (slot == -1) {
        return -1;
    }

    DataReadSource source;
    const bool     has_source = texture_data != nullptr && texture_size > 0 &&
                                DataReadSourceRange(texture_data, static_cast<std::size_t>(texture_size), source);
    BtItemCashArea[slot].Reset();
    u_char         *texture_copy = BtItemCashArea[slot].base + BtItemCashArea[slot].used * 16;
    CDataAlloc2<1> *area = &BtItemCashArea[slot];
    area->Alloc((texture_size >> 4) + 1);
    std::memcpy(texture_copy, texture_data, static_cast<std::size_t>(texture_size));
    if (has_source) {
        DataReadRegisterCopiedSource(texture_copy, static_cast<std::size_t>(texture_size), area, source);
    }
    TexManager.DeleteTextureBlock(slot + 0x38);
    TexManager.CleanUpBuffer();
    LOADTEXTURE_INFO2 textures[2] = {};
    textures[0].name = reinterpret_cast<char *>(texture_copy);
    textures[0].block_no = slot + 0x38;
    TexManager.LoadTextureBlockEX(slot + 0x38, textures);
    cash[slot] = reinterpret_cast<u_int *>(LoadMDSFile(model_data, area, 0, nullptr, nullptr));
    cash_lock[slot] = 1;
    cash_item[slot] = item_no;
    int model_no = GetFreeModelNo();
    model[model_no] = ITEM_MODEL_LOADED;
    model_cash[model_no] = slot;
    return model_no;
}
