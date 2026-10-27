#pragma once

#include "common.h"

#include <libgraph.h>

class sceVif1Packet;

namespace dc::game {

class CTexture;
struct TM2_head;

/**
 * Header at the start of an IMG texture archive, followed by one IMG_entry per picture.
 */
struct IMG_head {
    char  tag[4];   /**< IMG file signature. */
    u_int pictures; /**< Number of image entries in the file. */
    u_int unk_08[2];
};

/**
 * One picture in an IMG texture archive's directory.
 */
struct IMG_entry {
    char  name[32]; /**< Null-terminated texture name. */
    u_int offset;   /**< Byte offset of the texture data. */
    u_int unk_24[3];
};

/**
 * Fills a texture's size, format and GS registers from a TIM2 header.
 */
void SetTextureInfo(CTexture *texture, char *name, TM2_head *head);

/**
 * Fills a texture from the IMG archive picture with the given name, or every picture when empty.
 */
void SetTextureInfo(CTexture *texture, char *name, u_char *buffer);

/**
 * One texture file a block is built from. A table of these ends at the first entry whose name is
 * null or empty.
 */
struct LOADTEXTURE_INFO {
    char *name;     /**< Texture file or "#name#width#height#bpp" placeholder specification. */
    int   block_no; /**< Destination texture block. */
    int   mipmap;   /**< Whether the file's textures keep their mip levels. */
};

/**
 * One image already in memory a block is built from. The first field holds either a placeholder
 * specification or the IMG archive itself, which callers cast to a name.
 */
struct LOADTEXTURE_INFO2 {
    char *name;     /**< IMG archive in memory or "#name#width#height#bpp" placeholder specification. */
    int   block_no; /**< Destination texture block. */
    int   mipmap;   /**< Whether the image's textures keep their mip levels. */
};

/**
 * One registered texture: its name, owning block, assembled TEX0/TEX1 and staged pixels. A block
 * of -1 marks a texture no block owns, such as entry zero and every fixed texture.
 */
class CTexture {
public:
    short  block;    /**< Texture block containing this texture. */
    short  width;    /**< Texture width in pixels. */
    short  height;   /**< Texture height in pixels. */
    short  bpp;      /**< Bits per pixel. */
    char   name[32]; /**< Null-terminated texture name. */
    u_long tex0;     /**< GS TEX0 register value. */
    u_long tex1;     /**< GS TEX1 register value. */
    u_int *image[4]; /**< Staged pixels of each mip level, or null past the last one. */
    u_int *clut;     /**< Staged colour lookup table, or null for a true-colour texture. */
    int    swizzled; /**< Whether the 8-bit pixels are stored in GS 32-bit page order. */

    CTexture();

    /**
     * Clears the texture back to an unused entry.
     */
    void Initialize();
};

/**
 * One of the seventy-two texture blocks: its run of video memory, its run of the staging buffer,
 * and how far another block has since written over its VRAM.
 */
class CTextureBlock {
public:
    char       name[32];   /**< Null-terminated block name. */
    int        vram_top;   /**< End of the VRAM reserved for textures entered without pixels. */
    int        vram_end;   /**< VRAM address the next texture entered into the block is placed at. */
    int        loaded;     /**< Whether the block's textures have been uploaded to VRAM. */
    int        extend;     /**< Whether the block holds textures whose pixels the caller owns. */
    int        vram_dirty; /**< Highest VRAM address another block's upload has overwritten since this block was loaded. */
    u_long128 *buffer;     /**< Start of the block's pixels in the staging buffer. */
    u_long128 *buffer_end; /**< End of the block's pixels in the staging buffer. */

    CTextureBlock();

    /**
     * Clears the block back to an empty, unloaded state.
     */
    void Initialize();
};

/**
 * The texture registry. Blocks take VRAM from the bottom and fixed textures from `vram_fix`
 * downwards; the two meeting is the error every entry point checks for.
 */
class CTextureManager {
public:
    int               texture_max;   /**< Upper bound of the texture indices searched. */
    int               vram_work;     /**< VRAM address of the working area. */
    int               vram_size;     /**< Top of the VRAM the registry manages. */
    int               last_block;    /**< Block most recently uploaded by ReloadTexture, or -1. */
    int               vram_max;      /**< Highest VRAM address a block may reach. */
    int               vram_fix;      /**< Bottom of the fixed textures' VRAM, which grows downwards. */
    CTextureBlock     blocks[72];    /**< Managed texture blocks. */
    CTexture          textures[196]; /**< Registered textures. */
    u_long128        *buffer;        /**< Staging buffer the blocks' pixels are copied into. */
    int               buffer_used;   /**< Quadwords of the staging buffer in use. */
    int               buffer_size;   /**< Quadwords the staging buffer holds. */
    LOADTEXTURE_INFO *file;          /**< Texture table the two-argument LoadTextureBlock loads from. */

    CTextureManager() { Initialize(16352); }

    /**
     * Resets every block and texture and takes the whole staging buffer, managing VRAM up to size.
     */
    void Initialize(int size);

    /**
     * Uses a staging buffer of size quadwords, aligned forward for DMA.
     */
    void SetBuffer(u_long128 *buffer, int size);

    /**
     * Returns the index of a named texture in one block, or any block when negative, or -1.
     */
    int SearchTextureName(char *name, int block);

    /**
     * Returns the handle of a named texture in one block, or any block when negative, or -1.
     */
    int GetTextureHandle(char *name, int block);

    /**
     * Returns the texture for a handle, or entry zero when the handle is out of range.
     */
    CTexture *GetTexture(int handle);

    /**
     * Finds a named texture in one block, or in every block when the block is negative.
     */
    CTexture *GetTexture(char *name, int block);

    /**
     * Returns the first unused texture entry.
     */
    CTexture *SearchTexture(char *name);

    /**
     * Registers a texture in a block, copying its pixels into the staging buffer.
     */
    void EnterTexture(int block, char *name, u_char *image, int width, int height, int bpp, u_char *clut, int clut_colors, int mipmap, u_char *mip1, u_char *mip2, u_char *mip3, u_long tex1, int swizzled);

    /**
     * Registers a texture in a block, keeping the caller's pixels rather than copying them.
     */
    void EnterTextureEX(int block, char *name, u_char *image, int width, int height, int bpp, u_char *clut, int clut_colors, int mipmap, u_char *mip1, u_char *mip2, u_char *mip3, u_long tex1, int swizzled);

    /**
     * Registers a fixed texture and uploads it at once to the top of the managed VRAM.
     */
    void EnterFixTexture(char *name, u_char *image, int width, int height, int bpp, u_char *clut, int clut_colors, int mipmap, u_char *mip1, u_char *mip2, u_char *mip3, u_long tex1, int swizzled);

    /**
     * Registers the Z-buffer scratch texture from a TIM2 image.
     */
    void EnterFixTextureZ(u_char *buffer);

    /**
     * Registers every picture of an IMG archive in a block, or as fixed textures when negative.
     */
    void EnterIMGFile(u_char *buffer, int block, int mipmap, int extend);

    /**
     * Makes a block's textures resident and records it as the last block reloaded.
     */
    void ReloadTexture(sceVif1Packet *packet, int block);

    /**
     * Starts entering textures into a block at the end of the staging buffer.
     */
    void BeginEnterTextureBlock(int block);

    /**
     * Finishes entering textures into a block.
     */
    void EndEnterTextureBlock(int block);

    /**
     * Clears a block, or every block when negative, and unregisters its textures.
     */
    int DeleteTextureBlock(int block);

    /**
     * Packs the blocks' pixels to the front of the staging buffer.
     */
    int CleanUpBuffer();

    /**
     * Tidies the texture table after blocks have been deleted.
     */
    int CleanUpTextureList();

    /**
     * Builds a block, or every block when negative, from the table set by EnterTextureFile.
     */
    int LoadTextureBlock(int block, u_int *buffer);

    /**
     * Builds a block, or every block when negative, reading each file of the table into buffer.
     */
    int LoadTextureBlock(int block, LOADTEXTURE_INFO *table, u_int *buffer);

    /**
     * Builds a block, or every block when negative, from IMG archives already in memory.
     */
    int LoadTextureBlock(int block, LOADTEXTURE_INFO2 *table);

    /**
     * Builds a block like LoadTextureBlock, keeping the archives' pixels rather than copying them.
     */
    int LoadTextureBlockEX(int block, LOADTEXTURE_INFO2 *table);

    /**
     * Sets the table the two-argument LoadTextureBlock loads from.
     */
    int EnterTextureFile(LOADTEXTURE_INFO *table);

    /**
     * Debug hook that prints nothing.
     */
    void print_buff_info();
};

/**
 * The game's one texture registry.
 */
extern CTextureManager TexManager;

} // namespace dc::game
