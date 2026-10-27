#pragma once

#include <libgraph.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "gfx/gfx.hpp"

namespace dc::game {
class CTexture;
} // namespace dc::game

namespace dc::data {
struct DataReadSource;
} // namespace dc::data

namespace dc::render {

/**
 * What the renderer needs to sample one of the game's textures. The game passes textures
 * around as TEX0/TEX1 register values and as registry handles; the texture manager keeps the
 * TBP0 of every live texture unique (and CBP of every palette), so a TEX0 resolves to exactly
 * one image. TBP0 kMGPortFrameTbp0 and kMGPortPreviousFrameTbp0 (render/gs_state.hpp) resolve to
 * gfx::kMainTarget and gfx::kPreviousFrame.
 */
struct PortTextureRef {
    gfx::TextureBinding binding;
    unsigned            width = 0;  /**< Logical texels. */
    unsigned            height = 0; /**< Logical texels. */
    bool                valid = false;
};

/**
 * Resolves TEX0/TEX1 register values to the registered image (and palette, picked by CBP).
 * The result is invalid when TBP0 names no live image.
 */
PortTextureRef PortTextureFromTex0(u_long tex0, u_long tex1);
PortTextureRef PortTextureFromTex0(const sceGsTex0 &tex0, u_long tex1 = 0);
PortTextureRef PortTextureFromHandle(int handle);
PortTextureRef PortTextureFromCTexture(const game::CTexture *texture);

/*
 * The registry behind the resolvers.
 *
 * TBP0 and CBP values are keys into one 14-bit space shared by images and palettes, because the
 * game moves CLUTs around as 16x16 PSMCT32 images addressed by their CBP. A key is unique among
 * live entries and is handed out round-robin, so a released key is not reused until the rest of
 * the space has been. 0 and 0xFFF are the frame keys of render/gs_state.hpp and are never handed out.
 */

/**
 * Who releases an entry: the texture manager drops its own at Initialize and DeleteTextureBlock,
 * the loading screen its own at the next init_now_loading.
 */
enum class PortTextureOwner : uint8_t {
    Manager,
    Loading,
    Other,
};

/**
 * A TIM2 picture decoded into what the renderer takes. RGBA levels are r,g,b,a bytes with alpha
 * already in renderer units; the palette is in index order (the CSM1 order the GS reads applied).
 */
struct PortDecodedTexture {
    unsigned                          width = 0;
    unsigned                          height = 0;
    gfx::TextureFormat                format = gfx::TextureFormat::Rgba8;
    bool                              has_alpha = true;
    std::vector<std::vector<uint8_t>> levels;
    std::array<uint32_t, 256>         palette = {};
    bool                              four_bit = false;
};

/**
 * True when the data has no baked texture documents at all (a fresh extraction), so every texture
 * decodes from its raw source bytes instead. Decided once, from the data root, at the first load.
 */
bool PortLegacyTexturePath();

/**
 * Maps a data source (or its alias) to the baked texture document describing it, and to the
 * ordinal of its entry in that document when the source pins one.
 */
std::optional<std::string> PortTextureDocumentFromSource(const data::DataReadSource &source);
std::string                PortTextureDocumentFromAlias(std::string_view source_alias);
std::optional<unsigned>    PortTextureEntryIndexFromSource(const data::DataReadSource &source);

/**
 * Reads one schema-3 document and its RGBA levels through the active data VFS. A CLUT matching
 * one remembered with PortRememberTextureClut selects that palette variant's levels.
 */
bool PortLoadBakedTexture(std::string_view document, std::string_view name, unsigned picture_index,
                          unsigned width, unsigned height, unsigned level_count,
                          const u_char *clut, std::size_t clut_bytes,
                          PortDecodedTexture &out, std::string &error,
                          std::optional<unsigned> entry_index = std::nullopt,
                          int                     source_bpp = -1);

/**
 * Remembers the CLUT bytes of a named texture so baked palette variants can be matched against
 * the CLUT a later texture is loaded with.
 */
void PortRememberTextureClut(std::string_view name, const u_char *clut, std::size_t clut_bytes);

/**
 * bpp as CTexture keeps it: 0 IDTEX4, 1 IDTEX8, 2 RGB16, 3 RGB24, 4 RGB32. levels[0] is the base,
 * each further one half the size of the one before. swizzled: 8-bit pixels in the "IM2" layout
 * (Conv8to32's output). Returns false for a format or size it cannot take.
 */
bool PortDecodeTexture(int bpp, int width, int height, const u_char *const *levels, int level_count,
                       const u_char *clut, int clut_colors, bool swizzled, PortDecodedTexture &out);

/**
 * The inverse of retail's Conv8to32: GS PSMCT32-page order back to linear 8-bit rows.
 * Texels the conversion drops (images narrower than a block) come back as 0.
 */
void PortUnswizzle8(int width, int height, const u_char *swizzled, u_char *linear);

/**
 * The palette entry a CSM1 CLUT image position holds (bits 3 and 4 swapped), and back.
 * The mapping is its own inverse.
 */
constexpr unsigned PortClutCsm1(unsigned position) {
    return (position & ~0x18u) | ((position & 0x08u) << 1) | ((position & 0x10u) >> 1);
}

/**
 * Creates the renderer texture (and palette, for an index texture) and registers both. Returns
 * the TBP0 key, 0 when the renderer is not up or refused it; *palette_key gets the CBP key or 0.
 */
unsigned PortCreateTexture(const PortDecodedTexture &decoded, PortTextureOwner owner, unsigned *palette_key);

/**
 * A texture the game draws into or uploads into without pixels of its own: a named render
 * target (kept alive across releases, so its contents survive a mode's re-entry) for 16, 24 and
 * 32 bits, an index texture with a zeroed palette for 4 and 8 bits.
 */
unsigned PortCreatePlaceholder(std::string_view name, unsigned width, unsigned height, int bpp,
                               PortTextureOwner owner, unsigned *palette_key);

/**
 * Registers a key for a named render target that already has a fixed role (the manager's "work").
 * The target is looked up by name on every use, so gfx may recreate it at another size.
 */
unsigned PortRegisterNamedTarget(std::string_view name, unsigned width, unsigned height, bool has_alpha,
                                 PortTextureOwner owner);

/**
 * Releases one key, every key an owner holds, or reports whether a key is registered.
 * Releasing destroys the renderer texture the entry created; named targets live on in gfx.
 */
void PortReleaseKey(unsigned key);
void PortReleaseOwner(PortTextureOwner owner);
bool PortKeyLive(unsigned key);

/**
 * The GS local-to-local transfer MGMoveImage describes, in texels of the two images: TBP0/PSM of
 * each side. Images copy on the renderer; palettes copy their CPU shadow in CLUT image positions
 * and re-upload. False if either side is unknown or the two cannot be copied between.
 */
bool PortMoveImage(unsigned src_tbp, unsigned src_psm, int x, int y, int width, int height, unsigned dst_tbp,
                   unsigned dst_psm, int dst_x, int dst_y);
bool PortMoveImage(const sceGsTex0 &src, int x, int y, int width, int height, const sceGsTex0 &dst, int dst_x,
                   int dst_y);

/**
 * Uploads a 16x16 PSMCT32 CLUT image (GS alpha, CSM1 order) at cbp, as SetClut sends it.
 * False when cbp names no live palette.
 */
bool PortLoadClut(unsigned cbp, const u_int *clut);

/**
 * Copies the CPU shadow of a palette into rgba (256 entries, index order, renderer alpha).
 * False when cbp names no live palette.
 */
bool PortPaletteEntries(unsigned cbp, uint32_t *rgba);

} // namespace dc::render
