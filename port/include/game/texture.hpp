#pragma once

#include "common.h"

#include <libgraph.h>

class sceVif1Packet;
class CTexture;
struct TM2_head;

struct IMG_head {
    char  tag[4];   /**< IMG file signature. */
    u_int pictures; /**< Number of image entries in the file. */
    u_int unk_08[2];
};

struct IMG_entry {
    char  name[32]; /**< Null-terminated texture name. */
    u_int offset;   /**< Byte offset of the texture data. */
    u_int unk_24[3];
};

void SetTextureInfo(CTexture *texture, char *name, TM2_head *head);
void SetTextureInfo(CTexture *texture, char *name, u_char *buffer);

/* One texture file a block is built from, as the tables handed to EnterTextureFile hold them.
   LoadTextureBlock walks a table from the front for each block in turn and stops at the first
   entry whose name is null or empty, so a table states its own end rather than its length; the
   third field is carried into EnterIMGFile unread by anything reconstructed. */
struct LOADTEXTURE_INFO {
    char *name;     /**< Texture file or synthetic frame-buffer specification. */
    int   block_no; /**< Destination texture block. */
    int   mipmap;   /**< Whether the file's textures keep their mip levels. */
};

/* The same three fields for the block loaders that take an image already in memory. The first
   field is spelled as a name because that is what the loaders read it as — they compare its first
   character and hand it to the archive reader — and a caller with pixels rather than a file name
   casts on the way in. */
struct LOADTEXTURE_INFO2 {
    char *name;     /**< Texture file or synthetic frame-buffer specification. */
    int   block_no; /**< Destination texture block. */
    int   mipmap;   /**< Whether the image's textures keep their mip levels. */
};

/* One texture the registry hands out: the name it answers to, the block that owns it, the `TEX0`
   and `TEX1` already assembled for it, and where each of its four mip levels and its palette were
   copied. A block number of -1 marks a texture no block owns, which is what the registry's own
   entry at index zero and every fixed texture carry. */
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
    void Initialize();
};

/* One of the seventy-two texture blocks: the run of video memory its textures were given, the run
   of the registry's buffer their pixels were copied into, and how far another block has since
   written over the first. `vram_end` is where the next texture entered into the block goes and
   `vram_top` is where one entered without pixels goes, which is why the two move apart. */
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
    void Initialize();
};

/* The registry itself, of which the game keeps exactly one. Seventy-two blocks and a hundred and
   ninety-six textures, all of them constructed at start-up, over one buffer taken whole from
   `TextureData` at Initialize. Video memory is handed out from the bottom for the blocks and from
   `vram_fix` downwards for the fixed textures, and the two meeting is the error every entry
   point checks for. */
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

    void      Initialize(int size);
    void      SetBuffer(u_long128 *buffer, int size);
    int       SearchTextureName(char *name, int block);
    int       GetTextureHandle(char *name, int block);
    CTexture *GetTexture(int handle);
    /**
     * Finds a named texture in one block, or in every block when the block is negative.
     *
     * @mangled GetTexture__15CTextureManagerFPci
     * @address 0x1312D0
     * @size 0x50
     */
    CTexture *GetTexture(char *name, int block);
    CTexture *SearchTexture(char *name);
    void      EnterTexture(int block, char *name, u_char *image, int width, int height, int bpp, u_char *clut, int clut_colors, int mipmap, u_char *mip1, u_char *mip2, u_char *mip3, u_long tex1, int swizzled);
    void      EnterTextureEX(int block, char *name, u_char *image, int width, int height, int bpp, u_char *clut, int clut_colors, int mipmap, u_char *mip1, u_char *mip2, u_char *mip3, u_long tex1, int swizzled);
    void      EnterFixTexture(char *name, u_char *image, int width, int height, int bpp, u_char *clut, int clut_colors, int mipmap, u_char *mip1, u_char *mip2, u_char *mip3, u_long tex1, int swizzled);
    void      EnterFixTextureZ(u_char *buffer);
    void      EnterIMGFile(u_char *buffer, int block, int mipmap, int extend);
    void      ReloadTexture(sceVif1Packet *packet, int block);
    void      BeginEnterTextureBlock(int block);
    void      EndEnterTextureBlock(int block);
    int       DeleteTextureBlock(int block);
    int       CleanUpBuffer();
    int       CleanUpTextureList();
    int       LoadTextureBlock(int block, u_int *buffer);
    int       LoadTextureBlock(int block, LOADTEXTURE_INFO *table, u_int *buffer);
    int       LoadTextureBlock(int block, LOADTEXTURE_INFO2 *table);
    int       LoadTextureBlockEX(int block, LOADTEXTURE_INFO2 *table);
    int       EnterTextureFile(LOADTEXTURE_INFO *table);
    void      print_buff_info();
};

extern CTextureManager TexManager;
