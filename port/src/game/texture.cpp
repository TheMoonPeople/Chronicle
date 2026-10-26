#include "common.h"

#include <eekernel.h>
#include <libdma.h>
#include <libgraph.h>
#include <libpkt.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "dataalloc.hpp"
#include "dataread.hpp"
#include "mglib.hpp"
#include "texture.hpp"
#include "tim2.hpp"

/* The IMG archive's directory as the registry walks it: a header at the start of the buffer the
   file was read into and one fixed-size entry per picture behind it. */

CTexture::CTexture() {
    Initialize();
}

void CTexture::Initialize() {
    name[0] = 0;
    block = 0;
    image[3] = 0;
    image[2] = 0;
    image[1] = 0;
    image[0] = 0;
    clut = 0;
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
    buffer_end = 0;
    buffer = 0;
    loaded = false;
    extend = false;
}

/* The buffer is taken as it comes and walked forward to the next 128-byte boundary, because every
   transfer out of it is a DMA read; the quadword count has to lose what the alignment ate. */

int CTextureManager::SearchTextureName(char *name, int block) {
    int       i = texture_max;
    CTexture *tex = &textures[i];

    for (; i >= 0; i--, tex--) {
        char *name_cursor;
        char *entry_cursor;
        char  name_char;
        char  entry_char;

        if (tex->name[0] == 0) {
            continue;
        }

        if (block >= 0 && block != tex->block) {
            continue;
        }

        name_cursor = name;
        entry_cursor = tex->name;

        while ((name_char = *name_cursor) != 0 && (entry_char = *entry_cursor) != 0) {
            if (name_char != entry_char) {
                break;
            }

            name_cursor++;
            entry_cursor++;
        }

        if (name_char == 0 && *entry_cursor == 0) {
            return i;
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
        return 0;
    }

    return GetTexture(handle);
}

CTexture *CTextureManager::SearchTexture(char *name) {
    int       i;
    CTexture *tex = &textures[1];

    for (i = 1; i < 196; i++, tex++) {
        if (i == 196) {
            printf("Texture Over!!\n");

            while (1)
                ;
        }

        if (tex->name[0] == 0) {
            break;
        }
    }

    SearchTextureName(name, -1);
    return tex;
}

/* The extended entry point keeps the caller's own pixels rather than copying them into the block
   buffer, which is what lets a character carry textures the block was not built with. */

/* A fixed texture is uploaded on the spot down the GIF channel rather than kept in the block
   buffer for the frame packet to send, and its video memory is taken off the top of the region
   the blocks grow into from below. */

/* The Z-buffer scratch texture, which is the one fixed texture whose size and format the loader
   insists on rather than reads. */

void CTextureManager::EnterIMGFile(u_char *buffer, int block, int mipmap, int extend) {
    u_int   i;
    int     swizzled;
    u_char *entry;

    if (buffer == 0) {
        return;
    }

    swizzled = false;

    if (memcmp(buffer, "IM2", 3) == 0) {
        swizzled = true;
    }

    entry = buffer + 16;

    for (i = 0; i < *(u_int *) (buffer + 4); i++) {
        TM2_head    *head;
        TM2_picture *picture;
        u_char      *image;
        u_char      *clut;
        int          width;
        int          height;
        int          bpp;

        head = (TM2_head *) (buffer + *(int *) (entry + 32));
        picture = (TM2_picture *) ((u_char *) head + 16);
        width = head->image_width;
        height = head->image_height;

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
                entry += 48;
                continue;
        }

        clut = 0;
        image = (u_char *) picture + picture->header_size;

        if (bpp < 2) {
            clut = image + picture->image_size;
        }

        u_char *mip[4] = {0, 0, 0, 0};

        if (picture->mipmap_count > 1) {
            u_char *mip_image = image + picture->mipmap_size[0];

            for (int level = 1; level < picture->mipmap_count; level++) {
                mip[level] = mip_image;
                mip_image += picture->mipmap_size[level];
            }
        }

        if (extend == 0) {
            if (block >= 0) {
                EnterTexture(block, (char *) entry, image, width, height, bpp, clut, picture->clut_colors, mipmap, mip[1], mip[2], mip[3], picture->tex1, swizzled);
            } else {
                EnterFixTexture((char *) entry, image, width, height, bpp, clut, picture->clut_colors, mipmap, mip[1], mip[2], mip[3], picture->tex1, swizzled);
            }
        } else if (block >= 0) {
            EnterTextureEX(block, (char *) entry, image, width, height, bpp, clut, picture->clut_colors, mipmap, mip[1], mip[2], mip[3], picture->tex1, swizzled);
        }

        entry += 48;
    }
}

void CTextureManager::BeginEnterTextureBlock(int block) {
    if (block < 0 || block >= 72) {
        return;
    }

    if (blocks[block].vram_top == 0) {
        blocks[block].vram_top = mgTopVRAM;
    }

    if (blocks[block].vram_end == 0) {
        blocks[block].vram_end = mgTopVRAM;
    }

    blocks[block].buffer = buffer + buffer_used;
    blocks[block].buffer_end = buffer + buffer_used;
    blocks[block].loaded = false;
}

/* The blocks are packed back down to the front of the buffer in address order, and every texture
   that points into one is moved by the same distance its block was: the pointers are what the
   frame packet transfers from, so they cannot be left behind. */

/* A texture table entry may spell a placeholder rather than name a file, and the fields of one
   are separated by hashes. */
static int GetStr(char *text, char *out) {
    int length = 0;

    if (*text == 0) {
        return 0;
    }

    while (*text != '#') {
        out[length] = *text;
        length++;

        if (*text == 0) {
            break;
        }

        text++;
    }

    out[length] = 0;
    return length + 1;
}

static int GetDummyInfo(char *text, char *name, int *width, int *height, int *bpp) {
    char width_text[8];
    char height_text[8];
    char bpp_text[8];
    int  length;

    if (*text != '#') {
        return 0;
    }

    text++;

    int  parsed_width = 32;
    int  parsed_height = 32;
    int  parsed_bpp = 4;
    char parsed_name[32] = "";

    length = GetStr(text, parsed_name);

    if (length < 2) {
        parsed_name[0] = 0;
    }

    text += length;

    length = GetStr(text, width_text);
    text += length;

    if (length > 1) {
        parsed_width = atoi(width_text);
    }

    length = GetStr(text, height_text);
    text += length;

    if (length > 1) {
        parsed_height = atoi(height_text);
    }

    length = GetStr(text, bpp_text);

    if (length > 1) {
        parsed_bpp = atoi(bpp_text);
    }

    strcpy(name, parsed_name);
    *width = parsed_width;
    *height = parsed_height;
    *bpp = parsed_bpp;
    return 1;
}

int CTextureManager::LoadTextureBlock(int block, u_int *buffer) {
    return LoadTextureBlock(block, file, buffer);
}

int CTextureManager::LoadTextureBlock(int block, LOADTEXTURE_INFO *table, u_int *buffer) {
    LOADTEXTURE_INFO *info;
    int               i;
    int               begun;

    for (i = 0; i < 72; i++) {
        if (block >= 0 && block != i) {
            continue;
        }

        info = table;
        begun = 0;

        while (1) {
            char name[32];
            int  width;
            int  height;
            int  bpp;

            if (info->name == 0) {
                break;
            }

            if (info->name[0] == 0) {
                break;
            }

            if (info->block_no == i) {
                if (begun == 0) {
                    BeginEnterTextureBlock(i);
                }

                begun = 1;

                width = 32;
                height = 32;
                bpp = 4;

                if (GetDummyInfo(info->name, name, &width, &height, &bpp) != 0) {
                    TexManager.EnterTexture(info->block_no, name, 0, width, height, bpp, 0, 0, 0, 0, 0, 0, 0, 0);
                } else if (LoadFile2(info->name, buffer, 0, 0) != 0) {
                    EnterIMGFile((u_char *) buffer, info->block_no, info->mipmap, 0);
                }
            }

            info++;
        }

        if (begun != 0) {
            EndEnterTextureBlock(i);
        }
    }

    return 0;
}

int CTextureManager::LoadTextureBlock(int block, LOADTEXTURE_INFO2 *table) {
    LOADTEXTURE_INFO2 *info;
    int                i;
    int                begun;

    for (i = 0; i < 72; i++) {
        if (block >= 0 && block != i) {
            continue;
        }

        info = table;
        begun = 0;

        while (1) {
            char name[32];
            int  width;
            int  height;
            int  bpp;

            if (info->name == 0) {
                break;
            }

            if (info->name[0] == 0) {
                break;
            }

            if (info->block_no == i) {
                if (begun == 0) {
                    BeginEnterTextureBlock(i);
                }

                begun = 1;

                width = 32;
                height = 32;
                bpp = 4;

                if (GetDummyInfo(info->name, name, &width, &height, &bpp) != 0) {
                    TexManager.EnterTexture(info->block_no, name, 0, width, height, bpp, 0, 0, 0, 0, 0, 0, 0, 0);
                } else {
                    EnterIMGFile((u_char *) info->name, info->block_no, info->mipmap, 0);
                }
            }

            info++;
        }

        if (begun != 0) {
            EndEnterTextureBlock(i);
        }
    }

    return 0;
}

int CTextureManager::LoadTextureBlockEX(int block, LOADTEXTURE_INFO2 *table) {
    LOADTEXTURE_INFO2 *info;
    int                i;
    int                begun;

    for (i = 0; i < 72; i++) {
        if (block >= 0 && block != i) {
            continue;
        }

        info = table;
        begun = 0;

        while (1) {
            char name[32];
            int  width;
            int  height;
            int  bpp;

            if (info->name == 0) {
                break;
            }

            if (info->name[0] == 0) {
                break;
            }

            if (info->block_no == i) {
                if (begun == 0) {
                    BeginEnterTextureBlock(i);
                }

                begun = 1;

                width = 32;
                height = 32;
                bpp = 4;

                if (GetDummyInfo(info->name, name, &width, &height, &bpp) != 0) {
                    TexManager.EnterTextureEX(info->block_no, name, 0, width, height, bpp, 0, 0, 0, 0, 0, 0, 0, 0);
                } else {
                    int mipmap = info->mipmap;
                    int entry_block = info->block_no;

                    EnterIMGFile((u_char *) info->name, entry_block, mipmap, begun);
                }
            }

            info++;
        }

        if (begun != 0) {
            EndEnterTextureBlock(i);
        }
    }

    return 0;
}

int CTextureManager::EnterTextureFile(LOADTEXTURE_INFO *table) {
    file = table;
    return 1;
}

void CTextureManager::print_buff_info() {
}
