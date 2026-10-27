#include "common.h"

#include <libgraph.h>

#include <array>
#include <cstdlib>
#include <optional>
#include <string>
#include <string_view>

#include "dataread.hpp"
#include "mglib.hpp"
#include "texture.hpp"
#include "tim2.hpp"

namespace {

constexpr int kBlockCount = 72;
constexpr int kTextureCount = 196;

struct DummyTexture {
    std::string name;
    int         width = 32;
    int         height = 32;
    int         bpp = 4;
};

// Copies one '#'-separated field. A field ended by the terminator counts the terminator twice in
// the returned advance, so a caller stepping past it reads beyond the string.
int GetStr(const char *text, char *out) {
    if (text[0] == 0) {
        return 0;
    }

    int length = 0;

    while (text[length] != '#') {
        out[length] = text[length];
        length++;

        if (text[length - 1] == 0) {
            break;
        }
    }

    out[length] = 0;
    return length + 1;
}

// Parses "#name#width#height#bpp", where an empty field keeps its default.
std::optional<DummyTexture> GetDummyInfo(const char *text) {
    if (text[0] != '#') {
        return std::nullopt;
    }

    int                  cursor = 1;
    DummyTexture         dummy;
    std::array<char, 32> name{};
    std::array<char, 8>  field{};

    int length = GetStr(&text[cursor], name.data());

    if (length < 2) {
        name[0] = 0;
    }

    cursor += length;

    length = GetStr(&text[cursor], field.data());
    cursor += length;

    if (length > 1) {
        dummy.width = std::atoi(field.data());
    }

    length = GetStr(&text[cursor], field.data());
    cursor += length;

    if (length > 1) {
        dummy.height = std::atoi(field.data());
    }

    length = GetStr(&text[cursor], field.data());

    if (length > 1) {
        dummy.bpp = std::atoi(field.data());
    }

    dummy.name = name.data();
    return dummy;
}

// Builds every requested block from the table entries naming it, in table order. A table ends at
// the first entry whose name is null or empty.
template <typename Info, typename EnterDummy, typename EnterFile>
int LoadBlocks(CTextureManager &manager, int block, Info *table, EnterDummy enter_dummy, EnterFile enter_file) {
    for (int block_no = 0; block_no < kBlockCount; block_no++) {
        if (block >= 0 && block != block_no) {
            continue;
        }

        bool begun = false;

        for (int entry = 0; table[entry].name != nullptr && table[entry].name[0] != 0; entry++) {
            Info &info = table[entry];

            if (info.block_no != block_no) {
                continue;
            }

            if (!begun) {
                manager.BeginEnterTextureBlock(block_no);
            }

            begun = true;

            if (std::optional<DummyTexture> dummy = GetDummyInfo(info.name)) {
                enter_dummy(info, *dummy);
            } else {
                enter_file(info);
            }
        }

        if (begun) {
            manager.EndEnterTextureBlock(block_no);
        }
    }

    return 0;
}

} // namespace

CTexture::CTexture() {
    Initialize();
}

void CTexture::Initialize() {
    name[0] = 0;
    block = 0;
    image[3] = nullptr;
    image[2] = nullptr;
    image[1] = nullptr;
    image[0] = nullptr;
    clut = nullptr;
    tex1 = 0;
    tex0 = 0;
    bpp = 0;
    height = 0;
    width = 0;
    swizzled = false;
}

CTextureBlock::CTextureBlock() {
    Initialize();
}

void CTextureBlock::Initialize() {
    name[0] = 0;
    vram_end = 0;
    vram_top = 0;
    vram_dirty = 0;
    buffer_end = nullptr;
    buffer = nullptr;
    loaded = false;
    extend = false;
}

int CTextureManager::SearchTextureName(char *name, int block) {
    for (int index = texture_max; index >= 0; index--) {
        const CTexture &texture = textures[index];

        if (texture.name[0] == 0) {
            continue;
        }

        if (block >= 0 && block != texture.block) {
            continue;
        }

        if (std::string_view{name} == texture.name) {
            return index;
        }
    }

    return -1;
}

int CTextureManager::GetTextureHandle(char *name, int block) {
    int handle = SearchTextureName(name, block);

    if (handle < 0) {
        return -1;
    }

    return handle;
}

CTexture *CTextureManager::GetTexture(int handle) {
    if (handle < 0 || handle >= texture_max) {
        return &textures[0];
    }

    return &textures[handle];
}

CTexture *CTextureManager::GetTexture(char *name, int block) {
    int handle = GetTextureHandle(name, block);

    if (handle < 0) {
        return nullptr;
    }

    return GetTexture(handle);
}

CTexture *CTextureManager::SearchTexture(char *name) {
    int index = 1;

    for (; index < kTextureCount; index++) {
        if (textures[index].name[0] == 0) {
            break;
        }
    }

    SearchTextureName(name, -1);
    return textures + index;
}

void CTextureManager::EnterIMGFile(u_char *buffer, int block, int mipmap, int extend) {
    if (buffer == nullptr) {
        return;
    }

    const bool swizzled = std::string_view{reinterpret_cast<const char *>(buffer), 3} == "IM2";
    auto      *header = reinterpret_cast<IMG_head *>(buffer);
    auto      *entries = reinterpret_cast<IMG_entry *>(header + 1);

    for (u_int index = 0; index < header->pictures; index++) {
        IMG_entry &entry = entries[index];
        auto      *head = reinterpret_cast<TM2_head *>(buffer + entry.offset);
        auto      *picture = reinterpret_cast<TM2_picture *>(reinterpret_cast<u_char *>(head) + 16);
        int        width = head->image_width;
        int        height = head->image_height;
        int        bpp = 0;

        switch (head->image_type) {
            case TIM2_RGB16:
                bpp = 2;
                break;
            case TIM2_RGB24:
                bpp = 3;
                break;
            case TIM2_RGB32:
                bpp = 4;
                break;
            case TIM2_IDTEX4:
                bpp = 0;
                break;
            case TIM2_IDTEX8:
                bpp = 1;
                break;
            default:
                continue;
        }

        u_char *image = reinterpret_cast<u_char *>(picture) + picture->header_size;
        u_char *clut = bpp < 2 ? image + picture->image_size : nullptr;

        std::array<u_char *, 4> mip{};

        if (picture->mipmap_count > 1) {
            u_char *mip_image = image + picture->mipmap_size[0];

            for (int level = 1; level < static_cast<int>(picture->mipmap_count); level++) {
                mip[level] = mip_image;
                mip_image += picture->mipmap_size[level];
            }
        }

        if (extend == 0) {
            if (block >= 0) {
                EnterTexture(block, entry.name, image, width, height, bpp, clut, picture->clut_colors, mipmap, mip[1], mip[2], mip[3], picture->tex1, swizzled);
            } else {
                EnterFixTexture(entry.name, image, width, height, bpp, clut, picture->clut_colors, mipmap, mip[1], mip[2], mip[3], picture->tex1, swizzled);
            }
        } else if (block >= 0) {
            EnterTextureEX(block, entry.name, image, width, height, bpp, clut, picture->clut_colors, mipmap, mip[1], mip[2], mip[3], picture->tex1, swizzled);
        }
    }
}

void CTextureManager::BeginEnterTextureBlock(int block) {
    if (block < 0 || block >= kBlockCount) {
        return;
    }

    CTextureBlock &entry = blocks[block];

    if (entry.vram_top == 0) {
        entry.vram_top = mgTopVRAM;
    }

    if (entry.vram_end == 0) {
        entry.vram_end = mgTopVRAM;
    }

    entry.buffer = buffer + buffer_used;
    entry.buffer_end = buffer + buffer_used;
    entry.loaded = false;
}

int CTextureManager::LoadTextureBlock(int block, u_int *buffer) {
    return LoadTextureBlock(block, file, buffer);
}

int CTextureManager::LoadTextureBlock(int block, LOADTEXTURE_INFO *table, u_int *buffer) {
    return LoadBlocks(
        *this, block, table,
        [](LOADTEXTURE_INFO &info, DummyTexture &dummy) {
            TexManager.EnterTexture(info.block_no, dummy.name.data(), nullptr, dummy.width, dummy.height, dummy.bpp, nullptr, 0, 0, nullptr, nullptr, nullptr, 0, 0);
        },
        [this, buffer](LOADTEXTURE_INFO &info) {
            if (LoadFile2(info.name, buffer, nullptr, 0) != 0) {
                EnterIMGFile(reinterpret_cast<u_char *>(buffer), info.block_no, info.mipmap, 0);
            }
        });
}

int CTextureManager::LoadTextureBlock(int block, LOADTEXTURE_INFO2 *table) {
    return LoadBlocks(
        *this, block, table,
        [](LOADTEXTURE_INFO2 &info, DummyTexture &dummy) {
            TexManager.EnterTexture(info.block_no, dummy.name.data(), nullptr, dummy.width, dummy.height, dummy.bpp, nullptr, 0, 0, nullptr, nullptr, nullptr, 0, 0);
        },
        [this](LOADTEXTURE_INFO2 &info) { EnterIMGFile(reinterpret_cast<u_char *>(info.name), info.block_no, info.mipmap, 0); });
}

int CTextureManager::LoadTextureBlockEX(int block, LOADTEXTURE_INFO2 *table) {
    return LoadBlocks(
        *this, block, table,
        [](LOADTEXTURE_INFO2 &info, DummyTexture &dummy) {
            TexManager.EnterTextureEX(info.block_no, dummy.name.data(), nullptr, dummy.width, dummy.height, dummy.bpp, nullptr, 0, 0, nullptr, nullptr, nullptr, 0, 0);
        },
        [this](LOADTEXTURE_INFO2 &info) { EnterIMGFile(reinterpret_cast<u_char *>(info.name), info.block_no, info.mipmap, 1); });
}

int CTextureManager::EnterTextureFile(LOADTEXTURE_INFO *table) {
    file = table;
    return 1;
}

void CTextureManager::print_buff_info() {
}
