#pragma once

#include "gfx/gfx.hpp"

#include <libgraph.h>

#include <array>
#include <cstdint>
#include <string_view>
#include <vector>

class CTexture;

// What the renderer needs to sample one of the game's textures. The game passes textures
// around as TEX0/TEX1 register values and as registry handles; the texture manager keeps the
// TBP0 of every live texture unique (and CBP of every palette), so a TEX0 resolves to exactly
// one image. TBP0 kMGPortFrameTbp0 and kMGPortPreviousFrameTbp0 (mglib_port.hpp) resolve to
// gfx::kMainTarget and gfx::kPreviousFrame.
struct PortTextureRef {
    gfx::TextureBinding binding;
    unsigned            width = 0;  // logical texels
    unsigned            height = 0; // logical texels
    bool                valid = false;
};

PortTextureRef PortTextureFromTex0(u_long tex0, u_long tex1);
PortTextureRef PortTextureFromTex0(const sceGsTex0 &tex0, u_long tex1 = 0);
PortTextureRef PortTextureFromHandle(int handle);
PortTextureRef PortTextureFromCTexture(const CTexture *texture);

// ---- The registry behind the resolvers -------------------------------------------------------
//
// TBP0 and CBP values are keys into one 14-bit space shared by images and palettes, because the
// game moves CLUTs around as 16x16 PSMCT32 images addressed by their CBP. A key is unique among
// live entries and is handed out round-robin, so a released key is not reused until the rest of
// the space has been. 0 and 0xFFF are the frame keys of mglib_port.hpp and are never handed out.

// Who releases an entry: the texture manager drops its own at Initialize and DeleteTextureBlock,
// the loading screen its own at the next init_now_loading.
enum class PortTextureOwner : uint8_t {
    Manager,
    Loading,
    Other,
};

// A TIM2 picture decoded into what the renderer takes. RGBA levels are r,g,b,a bytes with alpha
// already in renderer units; the palette is in index order (the CSM1 order the GS reads applied).
struct PortDecodedTexture {
    unsigned                          width = 0;
    unsigned                          height = 0;
    gfx::TextureFormat                format = gfx::TextureFormat::Rgba8;
    bool                              has_alpha = true;
    std::vector<std::vector<uint8_t>> levels;
    std::array<uint32_t, 256>         palette = {};
    bool                              four_bit = false;
};

// bpp as CTexture keeps it: 0 IDTEX4, 1 IDTEX8, 2 RGB16, 3 RGB24, 4 RGB32. levels[0] is the base,
// each further one half the size of the one before. swizzled: 8-bit pixels in the "IM2" layout
// (Conv8to32's output). Returns false for a format or size it cannot take.
bool PortDecodeTexture(int bpp, int width, int height, const u_char *const *levels, int level_count,
                       const u_char *clut, int clut_colors, bool swizzled, PortDecodedTexture &out);

// The inverse of retail's Conv8to32: GS PSMCT32-page order back to linear 8-bit rows.
void PortUnswizzle8(int width, int height, const u_char *swizzled, u_char *linear);

// The palette entry a CSM1 CLUT image position holds (bits 3 and 4 swapped), and back.
constexpr unsigned PortClutCsm1(unsigned position) {
    return (position & ~0x18u) | ((position & 0x08u) << 1) | ((position & 0x10u) >> 1);
}

// Creates the renderer texture (and palette, for an index texture) and registers both. Returns
// the TBP0 key, 0 when the renderer is not up or refused it; *palette_key gets the CBP key or 0.
unsigned PortCreateTexture(const PortDecodedTexture &decoded, PortTextureOwner owner, unsigned *palette_key);

// A texture the game draws into or uploads into without pixels of its own: a named render
// target (kept alive across releases, so its contents survive a mode's re-entry) for 16, 24 and
// 32 bits, an index texture with a zeroed palette for 4 and 8 bits.
unsigned PortCreatePlaceholder(std::string_view name, unsigned width, unsigned height, int bpp,
                               PortTextureOwner owner, unsigned *palette_key);

// Registers a key for a named render target that already has a fixed role (the manager's "work").
unsigned PortRegisterNamedTarget(std::string_view name, unsigned width, unsigned height, bool has_alpha,
                                 PortTextureOwner owner);

void PortReleaseKey(unsigned key);
void PortReleaseOwner(PortTextureOwner owner);
bool PortKeyLive(unsigned key);

// The GS local-to-local transfer MGMoveImage describes, in texels of the two images: TBP0/PSM of
// each side. Images copy on the renderer; palettes copy their CPU shadow in CLUT image positions
// and re-upload. False if either side is unknown or the two cannot be copied between.
bool PortMoveImage(unsigned src_tbp, unsigned src_psm, int x, int y, int width, int height, unsigned dst_tbp,
                   unsigned dst_psm, int dst_x, int dst_y);
bool PortMoveImage(const sceGsTex0 &src, int x, int y, int width, int height, const sceGsTex0 &dst, int dst_x,
                   int dst_y);

// A 16x16 PSMCT32 CLUT image (GS alpha, CSM1 order) uploaded at cbp, as SetClut sends it.
bool PortLoadClut(unsigned cbp, const u_int *clut);
// The CPU shadow of a palette, index order, renderer alpha.
bool PortPaletteEntries(unsigned cbp, uint32_t *rgba);
