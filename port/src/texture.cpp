#include "texture.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "dataalloc.hpp"
#include "mglib.hpp"
#include "texture_port.hpp"
#include "tim2.hpp"

namespace {

constexpr int kPsmT8H = 27;

enum class EnterMode {
    Block,
    Extended,
    Fixed,
};

u_long Tex0(unsigned tbp, int tbw, int psm, int tw, int th, unsigned cbp, int cld) {
    return (u_long) tbp | ((u_long) tbw << 14) | ((u_long) psm << 20) | ((u_long) tw << 26) | ((u_long) th << 30) |
           ((u_long) 1 << 34) | ((u_long) cbp << 37) | ((u_long) cld << 61);
}

int CeilLog2(int value) {
    int log = 0;
    for (int power = value; power >= 2; power >>= 1) {
        log++;
    }
    return (1 << log) == value ? log : log + 1;
}

int Psm(int bpp) {
    switch (bpp) {
        case 0:
            return SCE_GS_PSMT4;
        case 1:
            return SCE_GS_PSMT8;
        case 2:
            return SCE_GS_PSMCT16;
        case 3:
            return SCE_GS_PSMCT24;
        case 4:
            return SCE_GS_PSMCT32;
    }
    return -1;
}

int ImageBytes(int width, int height, int bpp) {
    return bpp == 0 ? width * height / 2 : width * height * bpp;
}

[[noreturn]] void Stop(const char *what, const char *name, int value) {
    std::fprintf(stderr, "texture: %s (%s %d)\n", what, name, value);
    std::abort();
}

void ReleaseKeys(const CTexture &texture) {
    if (texture.name[0] == 0) {
        return;
    }
    const sceGsTex0 &tex0 = *reinterpret_cast<const sceGsTex0 *>(&texture.tex0);
    PortReleaseKey(tex0.TBP0);
    if (tex0.CBP != 0) {
        PortReleaseKey(tex0.CBP);
    }
}

// Retail's EnterTexture, EnterTextureEX and EnterFixTexture share everything but where pixels
// are staged and which VRAM end moves. The staging buffer and the VRAM arithmetic are kept as
// retail does them: the game sizes later allocations from buffer_used (editloop's LoadTexture),
// and the overflow checks stop the same loads retail stopped. The image goes to the renderer, and
// TBP0/CBP become registry keys, not VRAM addresses.
void Enter(CTextureManager &manager, EnterMode mode, int block, char *name, u_char *image, int width, int height,
           int bpp, u_char *clut, int clut_colors, int mipmap, u_char *mip1, u_char *mip2, u_long tex1,
           int swizzled) {
    if (name[0] == 0) {
        return;
    }
    CTexture *tex = manager.SearchTexture(name);
    if (image == 0 || mip1 == 0) {
        mipmap = false;
    }
    int psm = Psm(bpp);
    if (psm < 0) {
        return;
    }
    bool indexed = bpp < 2;
    int  tw = CeilLog2(width);
    int  th = CeilLog2(height);
    int  tbw = width >> 6;
    if (tbw <= 0) {
        tbw = 1;
    }
    int image_size = ImageBytes(width, height, bpp);
    int image_blocks = image_size >> 8;
    int clut_bytes = bpp == 0 ? 64 : 1024;

    if (mode == EnterMode::Fixed) {
        int fix_blocks = image_blocks;
        if (mipmap) {
            int mip_blocks = image_blocks >> 2;
            fix_blocks += mip_blocks + (mip_blocks >> 2);
        }
        if (fix_blocks % 32) {
            fix_blocks += 32 - fix_blocks % 32;
        }
        manager.vram_fix -= fix_blocks;
        if (image == 0 ? psm != SCE_GS_PSMCT32 : indexed) {
            manager.vram_fix -= 32;
        }
        if (image != 0 && !indexed) {
            tex->clut = 0;
        }
    } else {
        CTextureBlock &entry = manager.blocks[block];
        int            vram_top = entry.vram_top;
        int            vram_end = entry.vram_end;
        bool           staged = mode == EnterMode::Block;
        if (!staged) {
            entry.extend = true;
        }
        if (image == 0) {
            vram_top += image_blocks;
            if (indexed) {
                vram_top += 4;
            }
            vram_end = vram_top;
        } else {
            auto stage = [&](const u_char *source, int bytes, int blocks) -> u_int * {
                u_int *destination = (u_int *) (manager.buffer + manager.buffer_used);
                if (staged) {
                    // A picture with fewer levels than the block asks for leaves the last one null,
                    // which retail copies from all the same.
                    if (source != nullptr) {
                        std::memcpy(destination, source, bytes);
                    }
                    manager.buffer_used += blocks * 16;
                }
                vram_end += blocks;
                return staged ? destination : (u_int *) source;
            };
            tex->image[0] = stage(image, image_size, image_blocks);
            if (mipmap) {
                tex->image[1] = stage(mip1, image_size >> 2, image_blocks >> 2);
                tex->image[2] = stage(mip2, image_size >> 4, (image_blocks >> 2) >> 2);
            }
            if (indexed) {
                u_int *staged_clut = (u_int *) (manager.buffer + manager.buffer_used);
                if (staged) {
                    std::memcpy(staged_clut, clut, clut_bytes);
                    manager.buffer_used += 64;
                }
                tex->clut = staged ? staged_clut : (u_int *) clut;
                vram_end += 4;
            } else {
                tex->clut = 0;
            }
            tex->swizzled = staged ? indexed : swizzled;
        }
        if (vram_end % 32) {
            int pad = 32 - vram_end % 32;
            vram_end += pad;
            if (staged) {
                manager.buffer_used += pad * 16;
            }
        }
        entry.vram_top = vram_top;
        entry.vram_end = vram_end;
        if (staged && manager.buffer_used >= manager.buffer_size) {
            Stop("texture buffer over", name, manager.buffer_used);
        }
        if (entry.vram_end > manager.vram_max) {
            Stop("VRAM is not enough", name, entry.vram_end);
        }
    }

    unsigned tbp;
    unsigned cbp = 0;
    if (image == 0) {
        tbp = PortCreatePlaceholder(name, width, height, bpp, PortTextureOwner::Manager, &cbp);
    } else {
        const u_char      *levels[3] = {image, mipmap ? mip1 : nullptr, mipmap ? mip2 : nullptr};
        PortDecodedTexture decoded;
        if (!PortDecodeTexture(bpp, width, height, levels, mipmap ? 3 : 1, clut, clut_colors,
                               indexed && swizzled != 0, decoded)) {
            return;
        }
        tbp = PortCreateTexture(decoded, PortTextureOwner::Manager, &cbp);
    }
    tex->tex0 = Tex0(tbp, tbw, psm, tw, th, cbp, indexed ? 1 : 0);

    std::strcpy(tex->name, name);
    tex->width = width;
    tex->height = height;
    tex->bpp = bpp;
    tex->block = mode == EnterMode::Fixed ? -1 : block;

    if (bpp > 0 && mipmap) {
        tex->tex1 = SCE_GS_SET_TEX1(0, 2, 1, 5, 1, 0, -120);
        if (tex1 != 0) {
            sceGsTex1 *lod = (sceGsTex1 *) &tex1;
            tex->tex1 = SCE_GS_SET_TEX1(0, 2, 1, 5, 1, lod->L, lod->K);
        }
    }
    manager.texture_max = 195;
}

} // namespace

void CTextureManager::Initialize(int size) {
    PortReleaseOwner(PortTextureOwner::Manager);

    TextureData.used = 0;
    SetBuffer((u_long128 *) (TextureData.base + TextureData.used * 16), TextureData.limit);

    for (int i = 0; i < 72; i++) {
        blocks[i].Initialize();
        blocks[i].vram_top = mgTopVRAM;
        blocks[i].vram_end = mgTopVRAM;
    }
    for (int i = 0; i < 196; i++) {
        textures[i].Initialize();
    }

    texture_max = 1;
    vram_work = 8960;
    vram_size = size;
    vram_max = vram_size;
    vram_fix = size;

    // The frame-copy area water samples, a field of the frame on PS2: the water unit fills it.
    unsigned work = PortRegisterNamedTarget("work", 640, SCREEN_HALF_HEIGHT, true, PortTextureOwner::Manager);
    std::strcpy(textures[0].name, "work");
    textures[0].tex0 = SCE_GS_SET_TEX0(work, 10, 0, 10, 8, 1, 0, 0, 0, 0, 0, 0);
    textures[0].block = -1;
    last_block = -1;
}

void CTextureManager::EnterTexture(int block, char *name, u_char *image, int width, int height, int bpp,
                                   u_char *clut, int clut_colors, int mipmap, u_char *mip1, u_char *mip2,
                                   u_char *mip3, u_long tex1, int swizzled) {
    Enter(*this, EnterMode::Block, block, name, image, width, height, bpp, clut, clut_colors, mipmap, mip1, mip2,
          tex1, swizzled);
}

void CTextureManager::EnterTextureEX(int block, char *name, u_char *image, int width, int height, int bpp,
                                     u_char *clut, int clut_colors, int mipmap, u_char *mip1, u_char *mip2,
                                     u_char *mip3, u_long tex1, int swizzled) {
    Enter(*this, EnterMode::Extended, block, name, image, width, height, bpp, clut, clut_colors, mipmap, mip1,
          mip2, tex1, swizzled);
}

void CTextureManager::EnterFixTexture(char *name, u_char *image, int width, int height, int bpp, u_char *clut,
                                      int clut_colors, int mipmap, u_char *mip1, u_char *mip2, u_char *mip3,
                                      u_long tex1, int swizzled) {
    Enter(*this, EnterMode::Fixed, -1, name, image, width, height, bpp, clut, clut_colors, mipmap, mip1, mip2,
          tex1, swizzled);
}

// Retail parks "stayframe", the menus' 640-wide 8-bit sheet of frames and digits, in the upper
// bytes of the Z buffer as PSMT8H. Here it is an index texture like any other; the menus reach it
// by name and draw pieces of it with set2DSprite.
void CTextureManager::EnterFixTextureZ(u_char *buffer) {
    char        *name = (char *) (buffer + 16);
    TM2_head    *head = (TM2_head *) (buffer + *(int *) (buffer + 48));
    TM2_picture *picture = (TM2_picture *) ((u_char *) head + 16);
    int          width = head->image_width;
    int          height = head->image_height;

    if (width != 640 || height > SCREEN_HALF_HEIGHT || picture->image_type != TIM2_IDTEX8) {
        return;
    }

    u_char   *image = (u_char *) picture + picture->header_size;
    u_char   *clut = image + picture->image_size;
    CTexture *tex = SearchTexture(name);
    std::strcpy(tex->name, name);
    tex->width = width;
    tex->height = height;
    tex->bpp = 1;
    tex->block = 73;

    const u_char      *levels[1] = {image};
    PortDecodedTexture decoded;
    unsigned           tbp = 0;
    unsigned           cbp = 0;
    if (PortDecodeTexture(1, width, height, levels, 1, clut, 256, false, decoded)) {
        tbp = PortCreateTexture(decoded, PortTextureOwner::Manager, &cbp);
    }
    tex->tex0 = SCE_GS_SET_TEX0(tbp, 10, kPsmT8H, 10, 8, 1, 0, cbp, 0, 0, 0, 1);
}

// Every texture is resident on the renderer from the moment it is entered, so a block never needs
// uploading; only the bookkeeping retail keeps for it remains.
void CTextureManager::ReloadTexture(sceVif1Packet *packet, int block) {
    if (block < 0 || block >= 72) {
        last_block = -1;
        return;
    }
    last_block = block;
    blocks[block].loaded = true;
}

int CTextureManager::DeleteTextureBlock(int block) {
    if (block >= 72) {
        return 0;
    }
    if (block >= 0) {
        blocks[block].Initialize();
    }
    if (block < 0) {
        block = -1;
    }
    for (int i = 0; i < 196; i++) {
        if (block < 0 || textures[i].block == block) {
            ReleaseKeys(textures[i]);
            textures[i].Initialize();
        }
    }
    return 1;
}

int LoadImage(u_int *packet, int dbp, int dpsm, int dbw, u_long128 *source, int qwc, int dsax, int dsay, int rrw,
              int rrh) {
    return 0;
}

int CTextureManager::CleanUpTextureList() {
    // Retail closes the table's holes by moving later entries down. A native visual keeps a table
    // index (Draw3DStrip::texture) where retail's packet holds a baked TEX0, so a move would
    // retarget it. Entries stay where they are: SearchTexture already fills empty ones, so a
    // reloaded block fills the first available holes rather than being appended after compaction.
    return 1;
}
