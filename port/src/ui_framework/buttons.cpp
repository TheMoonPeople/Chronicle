#include "ui_framework/internal.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <map>
#include <tuple>
#include <utility>
#include <vector>

#include "gfx/gfx.hpp"
#include "localize.hpp"
#include "platform/png_image.hpp"
#include "texture_port.hpp"

namespace UI {

namespace {

/**
 * Material and noise synthesis parameters for a procedural button preset.
 */
struct ButtonSurfaceSpec {
    const char *plate_path;      /**< Optional PNG override path relative to lang/. */
    const char *tex_name;        /**< Internal texture identifier. */
    int         seed_offset;     /**< Base seed offset for procedural noise generation. */
    float       base_r;          /**< Base red channel value. */
    float       base_g;          /**< Base green channel value. */
    float       base_b;          /**< Base blue channel value. */
    float       scale_r;         /**< Red channel modulation scale. */
    float       scale_g;         /**< Green channel modulation scale. */
    float       scale_b;         /**< Blue channel modulation scale. */
    float       bevel_r;         /**< Red channel bevel highlight scale. */
    float       bevel_g;         /**< Green channel bevel highlight scale. */
    float       bevel_b;         /**< Blue channel bevel highlight scale. */
    float       rim_r;           /**< Outer rim red channel value. */
    float       rim_g;           /**< Outer rim green channel value. */
    float       rim_b;           /**< Outer rim blue channel value. */
    float       macro_weight;    /**< Low-frequency FBM noise weight. */
    float       med_weight;      /**< Medium-frequency FBM noise weight. */
    float       detail_weight;   /**< High-frequency FBM noise weight. */
    float       ridge_weight;    /**< Ridged noise weight. */
    float       pitting_weight;  /**< Surface pore depression weight. */
    float       tooth_weight;    /**< Fine grain heightmap weight. */
    float       bump_strength;   /**< Directional bump shading multiplier. */
    float       dither_strength; /**< Ordered Bayer dither amplitude. */
    float       bevel_strength;  /**< Edge bevel lighting multiplier. */
};

/**
 * Cached 256x256 tileable base texture and heightmap for a button material.
 */
struct ButtonBaseSurface {
    std::vector<uint8_t> rgba;            /**< Tileable RGBA8 color buffer. */
    std::vector<float>   height;          /**< Tileable normalized heightfield. */
    int                  size_mask = 255; /**< Coordinate wrap mask for power-of-two dimensions. */
};

/**
 * Cached VRAM texture handle and wrapper for a rendered button plate.
 */
struct CachedButtonTex {
    CTexture tex{};   /**< Engine texture descriptor. */
    unsigned tbp = 0; /**< Host texture key. */
};

/**
 * Surface specification for the terracotta clay button preset.
 */
constexpr ButtonSurfaceSpec kTerracottaBtnSpec{
    .plate_path      = "textures/plates/ui_btn_terracotta.png",
    .tex_name        = "ui_btn_terracotta",
    .seed_offset     = 2100,
    .base_r          = 214.0f,
    .base_g          = 114.0f,
    .base_b          = 60.0f,
    .scale_r         = 1.08f,
    .scale_g         = 0.86f,
    .scale_b         = 0.68f,
    .bevel_r         = 1.06f,
    .bevel_g         = 0.92f,
    .bevel_b         = 0.76f,
    .rim_r           = 74.0f,
    .rim_g           = 34.0f,
    .rim_b           = 16.0f,
    .macro_weight    = 26.0f,
    .med_weight      = 22.0f,
    .detail_weight   = 24.0f,
    .ridge_weight    = 18.0f,
    .pitting_weight  = 14.0f,
    .tooth_weight    = 0.115f,
    .bump_strength   = 46.0f,
    .dither_strength = 3.2f,
    .bevel_strength  = 58.0f,
};

/**
 * Surface specification for the amber-gold sandstone button preset.
 */
constexpr ButtonSurfaceSpec kAmberGoldBtnSpec{
    .plate_path      = "textures/plates/ui_btn_ambergold.png",
    .tex_name        = "ui_btn_amber",
    .seed_offset     = 3200,
    .base_r          = 218.0f,
    .base_g          = 158.0f,
    .base_b          = 74.0f,
    .scale_r         = 1.05f,
    .scale_g         = 0.94f,
    .scale_b         = 0.72f,
    .bevel_r         = 1.06f,
    .bevel_g         = 0.98f,
    .bevel_b         = 0.82f,
    .rim_r           = 78.0f,
    .rim_g           = 52.0f,
    .rim_b           = 20.0f,
    .macro_weight    = 24.0f,
    .med_weight      = 20.0f,
    .detail_weight   = 22.0f,
    .ridge_weight    = 18.0f,
    .pitting_weight  = 12.0f,
    .tooth_weight    = 0.105f,
    .bump_strength   = 44.0f,
    .dither_strength = 3.0f,
    .bevel_strength  = 58.0f,
};

/**
 * Surface specification for the stone-green jade button preset.
 */
constexpr ButtonSurfaceSpec kStoneGreenBtnSpec{
    .plate_path      = "textures/plates/ui_btn_stonegreen.png",
    .tex_name        = "ui_btn_green",
    .seed_offset     = 4300,
    .base_r          = 92.0f,
    .base_g          = 176.0f,
    .base_b          = 148.0f,
    .scale_r         = 0.82f,
    .scale_g         = 1.04f,
    .scale_b         = 0.96f,
    .bevel_r         = 0.90f,
    .bevel_g         = 1.06f,
    .bevel_b         = 1.02f,
    .rim_r           = 28.0f,
    .rim_g           = 68.0f,
    .rim_b           = 56.0f,
    .macro_weight    = 25.0f,
    .med_weight      = 21.0f,
    .detail_weight   = 22.0f,
    .ridge_weight    = 19.0f,
    .pitting_weight  = 13.0f,
    .tooth_weight    = 0.110f,
    .bump_strength   = 45.0f,
    .dither_strength = 3.0f,
    .bevel_strength  = 58.0f,
};

/**
 * Returns the surface parameters for a button preset.
 */
const ButtonSurfaceSpec &GetButtonSpec(ButtonPreset preset) {
    switch (preset) {
        case ButtonPreset::AmberGold:
            return kAmberGoldBtnSpec;
        case ButtonPreset::StoneGreen:
            return kStoneGreenBtnSpec;
        case ButtonPreset::Terracotta:
        default:
            return kTerracottaBtnSpec;
    }
}

/**
 * Computes a deterministic 2D hash in [0, 1] for integer lattice coordinates.
 */
float BtnHash2D(int ix, int iy, int seed) {
    uint32_t n = static_cast<uint32_t>(ix) * 374761393u +
                 static_cast<uint32_t>(iy) * 668265263u +
                 static_cast<uint32_t>(seed) * 1442695041u;
    n = (n ^ (n >> 13)) * 1274126177u;
    n = n ^ (n >> 16);
    return static_cast<float>(n) / 4294967295.0f;
}

/**
 * Evaluates smooth periodic 2D value noise over a power-of-two domain.
 */
float BtnSmoothNoisePow2(float x, float y, int px_mask, int py_mask, int seed) {
    const int   x0 = static_cast<int>(std::floor(x));
    const int   y0 = static_cast<int>(std::floor(y));
    const float fx = x - static_cast<float>(x0);
    const float fy = y - static_cast<float>(y0);
    const float ux = fx * fx * (3.0f - 2.0f * fx);
    const float uy = fy * fy * (3.0f - 2.0f * fy);

    const int ix0 = x0 & px_mask;
    const int ix1 = (x0 + 1) & px_mask;
    const int iy0 = y0 & py_mask;
    const int iy1 = (y0 + 1) & py_mask;

    const float v00 = BtnHash2D(ix0, iy0, seed);
    const float v10 = BtnHash2D(ix1, iy0, seed);
    const float v01 = BtnHash2D(ix0, iy1, seed);
    const float v11 = BtnHash2D(ix1, iy1, seed);

    const float top = v00 + (v10 - v00) * ux;
    const float bot = v01 + (v11 - v01) * ux;
    return top + (bot - top) * uy;
}

/**
 * Evaluates tileable fractional Brownian motion over a 256x256 domain.
 */
float BtnFbmPeriodic256(float x, float y, int cell_size, int octaves, int seed) {
    float val      = 0.0f;
    float amp      = 0.5f;
    float inv_cell = 1.0f / static_cast<float>(cell_size);
    int   period   = std::max(1, 256 / cell_size);

    for (int i = 0; i < octaves; ++i) {
        const int mask = period - 1;
        val += amp * BtnSmoothNoisePow2(x * inv_cell, y * inv_cell, mask, mask, seed + i * 19);
        inv_cell *= 2.0f;
        period <<= 1;
        amp *= 0.5f;
    }
    return val;
}

/**
 * Evaluates tileable ridged multifractal noise over a 256x256 domain.
 */
float BtnRidgedPeriodic256(float x, float y, int cell_size, int octaves, int seed) {
    float val      = 0.0f;
    float amp      = 0.5f;
    float inv_cell = 1.0f / static_cast<float>(cell_size);
    int   period   = std::max(1, 256 / cell_size);

    for (int i = 0; i < octaves; ++i) {
        const int   mask  = period - 1;
        const float n     = BtnSmoothNoisePow2(x * inv_cell, y * inv_cell, mask, mask, seed + i * 31);
        const float ridge = 1.0f - std::sqrt((n * 2.0f - 1.0f) * (n * 2.0f - 1.0f) + 0.05f);
        val += ridge * amp;
        inv_cell *= 2.0f;
        period <<= 1;
        amp *= 0.5f;
    }
    return val;
}

/**
 * Applies a separable 3-tap periodic box blur to a square float buffer.
 */
void BlurPeriodic256(std::vector<float> &field, int size, int passes) {
    const int          mask = size - 1;
    std::vector<float> tmp(field.size(), 0.0f);

    for (int p = 0; p < passes; ++p) {
        for (int y = 0; y < size; ++y) {
            const size_t row = static_cast<size_t>(y) * size;
            for (int x = 0; x < size; ++x) {
                const float sum = field[row + ((x - 1) & mask)] * 3.0f +
                                  field[row + (x & mask)] * 10.0f +
                                  field[row + ((x + 1) & mask)] * 3.0f;
                tmp[row + x] = sum * (1.0f / 16.0f);
            }
        }
        for (int y = 0; y < size; ++y) {
            const size_t r_m1 = static_cast<size_t>((y - 1) & mask) * size;
            const size_t r_0  = static_cast<size_t>(y) * size;
            const size_t r_p1 = static_cast<size_t>((y + 1) & mask) * size;
            for (int x = 0; x < size; ++x) {
                const float sum = tmp[r_m1 + x] * 3.0f +
                                  tmp[r_0 + x] * 10.0f +
                                  tmp[r_p1 + x] * 3.0f;
                field[r_0 + x] = sum * (1.0f / 16.0f);
            }
        }
    }
}

/**
 * Computes the signed distance from a point to a rounded rectangle centered at (cx, cy).
 */
float BtnRoundedSdf(float px, float py, float cx, float cy, float half_w, float half_h, float radius) {
    const float dx    = std::fabs(px - cx) - (half_w - radius);
    const float dy    = std::fabs(py - cy) - (half_h - radius);
    const float out_d = std::hypot(std::max(dx, 0.0f), std::max(dy, 0.0f));
    const float in_d  = std::min(std::max(dx, dy), 0.0f);
    return out_d + in_d - radius;
}

/**
 * Builds or returns the cached 256x256 base surface for the given button preset.
 */
const ButtonBaseSurface &GetButtonBaseSurface(ButtonPreset preset) {
    static ButtonBaseSurface surfaces[3];
    const int                idx = (preset == ButtonPreset::AmberGold)    ? 1
                                 : (preset == ButtonPreset::StoneGreen) ? 2
                                                                        : 0;
    ButtonBaseSurface       &out = surfaces[idx];

    if (!out.rgba.empty()) {
        return out;
    }

    const ButtonSurfaceSpec &spec       = GetButtonSpec(preset);
    std::filesystem::path    custom_png = LocalizeFindFile(spec.plate_path);
    int                      png_w      = 0;
    int                      png_h      = 0;
    std::vector<uint8_t>     loaded;
    if (!custom_png.empty() && PngLoadRgba(custom_png, png_w, png_h, loaded) &&
        png_w == png_h && (png_w == 128 || png_w == 256 || png_w == 512)) {
        out.size_mask = png_w - 1;
        out.rgba      = std::move(loaded);
        out.height.assign(static_cast<size_t>(png_w) * png_h, 0.5f);
        return out;
    }

    constexpr int kSize = 256;
    constexpr int kMask = kSize - 1;
    out.size_mask       = kMask;

    const int          s = spec.seed_offset;
    std::vector<float> height_map(static_cast<size_t>(kSize) * kSize, 0.0f);
    std::vector<float> tone_map(static_cast<size_t>(kSize) * kSize, 0.0f);

    for (int y = 0; y < kSize; ++y) {
        for (int x = 0; x < kSize; ++x) {
            const float fx = static_cast<float>(x);
            const float fy = static_cast<float>(y);

            const float wx = (BtnFbmPeriodic256(fx, fy, 32, 3, s + 501) - 0.5f) * 9.0f;
            const float wy = (BtnFbmPeriodic256(fx, fy, 32, 3, s + 601) - 0.5f) * 9.0f;

            const float macro  = BtnFbmPeriodic256(fx + wx, fy + wy, 64, 4, s + 101);
            const float med    = BtnFbmPeriodic256(fx - wy * 0.6f, fy + wx * 0.6f, 24, 4, s + 303);
            const float detail = BtnFbmPeriodic256(fx, fy, 12, 3, s + 202);
            const float ridge  = BtnRidgedPeriodic256(fx + wx * 0.5f, fy - wy * 0.5f, 28, 3, s + 404);

            const float raw_pit = BtnSmoothNoisePow2(fx * 0.35f, fy * 0.35f, 63, 63, s + 707);
            const float pit     = (raw_pit < 0.28f) ? ((raw_pit - 0.28f) * 2.2f) : 0.0f;

            const size_t i = static_cast<size_t>(y) * kSize + x;
            height_map[i]  = macro * 0.28f + med * 0.34f + detail * 0.22f + ridge * 0.16f + pit * 0.08f;
            tone_map[i]    = (macro - 0.48f) * spec.macro_weight +
                             (med - 0.48f) * spec.med_weight +
                             (detail - 0.48f) * spec.detail_weight +
                             (ridge - 0.34f) * spec.ridge_weight +
                             pit * spec.pitting_weight;
        }
    }

    BlurPeriodic256(height_map, kSize, 1);

    for (int y = 0; y < kSize; ++y) {
        for (int x = 0; x < kSize; ++x) {
            const float fx    = static_cast<float>(x);
            const float fy    = static_cast<float>(y);
            const float tooth = BtnSmoothNoisePow2(fx * 0.65f, fy * 0.65f, 127, 127, s + 808) * 0.55f +
                                BtnHash2D(x, y, s + 909) * 0.45f;
            height_map[static_cast<size_t>(y) * kSize + x] += (tooth - 0.5f) * spec.tooth_weight;
        }
    }

    static constexpr float kBayer4x4[4][4] = {
        {-0.46875f,  0.03125f, -0.34375f,  0.15625f},
        { 0.28125f, -0.21875f,  0.40625f, -0.09375f},
        {-0.28125f,  0.21875f, -0.40625f,  0.09375f},
        { 0.46875f, -0.03125f,  0.34375f, -0.15625f}
    };

    out.rgba.assign(static_cast<size_t>(kSize) * kSize * 4, 255);

    for (int y = 0; y < kSize; ++y) {
        const int ym1 = (y - 1) & kMask;
        const int yp1 = (y + 1) & kMask;
        for (int x = 0; x < kSize; ++x) {
            const int xm1 = (x - 1) & kMask;
            const int xp1 = (x + 1) & kMask;

            const size_t i    = static_cast<size_t>(y) * kSize + x;
            const float  h_tl = height_map[static_cast<size_t>(ym1) * kSize + xm1];
            const float  h_br = height_map[static_cast<size_t>(yp1) * kSize + xp1];
            const float  h_t  = height_map[static_cast<size_t>(ym1) * kSize + x];
            const float  h_b  = height_map[static_cast<size_t>(yp1) * kSize + x];

            const float bump       = ((h_tl - h_br) * 0.68f + (h_t - h_b) * 0.32f) * spec.bump_strength;
            const float dither     = kBayer4x4[y & 3][x & 3] * spec.dither_strength;
            const float delta      = tone_map[i] + bump + dither;
            const float clay_shift = (BtnHash2D(x >> 1, y >> 1, s + 111) - 0.5f) * 6.0f;

            const float r = spec.base_r + delta * spec.scale_r + clay_shift;
            const float g = spec.base_g + delta * spec.scale_g;
            const float b = spec.base_b + delta * spec.scale_b - clay_shift * 0.5f;

            const size_t out_idx  = i * 4;
            out.rgba[out_idx + 0] = static_cast<uint8_t>(std::lround(std::clamp(r, 0.0f, 255.0f)));
            out.rgba[out_idx + 1] = static_cast<uint8_t>(std::lround(std::clamp(g, 0.0f, 255.0f)));
            out.rgba[out_idx + 2] = static_cast<uint8_t>(std::lround(std::clamp(b, 0.0f, 255.0f)));
            out.rgba[out_idx + 3] = 255;
        }
    }

    out.height = std::move(height_map);
    return out;
}

/**
 * Generates or retrieves a cached beveled button texture of the requested dimensions.
 */
CTexture *GetProceduralButtonTex(int width_px, int height_px, ButtonPreset preset,
                                 bool rounded, int seed_variant) {
    using CacheKey = std::tuple<int, int, int, int, int>;
    static std::map<CacheKey, CachedButtonTex> cache;

    const int      variant_bucket = seed_variant & 15;
    const CacheKey key{width_px, height_px, static_cast<int>(preset), rounded ? 1 : 0, variant_bucket};
    auto           it = cache.find(key);
    if (it != cache.end() && it->second.tbp != 0 && PortKeyLive(it->second.tbp)) {
        return &it->second.tex;
    }

    const ButtonSurfaceSpec &spec        = GetButtonSpec(preset);
    const ButtonBaseSurface &surface     = GetButtonBaseSurface(preset);
    const int                size_mask   = surface.size_mask;
    const int                base_stride = size_mask + 1;

    const int u_off = (variant_bucket * 53) & size_mask;
    const int v_off = (variant_bucket * 89) & size_mask;

    PortDecodedTexture decoded{};
    decoded.width     = static_cast<unsigned>(width_px);
    decoded.height    = static_cast<unsigned>(height_px);
    decoded.format    = gfx::TextureFormat::Rgba8;
    decoded.has_alpha = true;

    std::vector<uint8_t> rgba(static_cast<size_t>(width_px) * height_px * 4, 0);

    const float cx      = width_px * 0.5f;
    const float cy      = height_px * 0.5f;
    const float half_w  = width_px * 0.5f - 0.75f;
    const float half_h  = height_px * 0.5f - 0.75f;
    const float radius  = rounded ? 3.5f : 1.0f;
    const float bevel_w = 2.8f;
    const float inv_w   = 1.0f / static_cast<float>(std::max(1, width_px));
    const float inv_h   = 1.0f / static_cast<float>(std::max(1, height_px));

    for (int y = 0; y < height_px; ++y) {
        const int   by     = (y + v_off) & size_mask;
        const float v_norm = static_cast<float>(y) * inv_h;

        for (int x = 0; x < width_px; ++x) {
            const int    bx      = (x + u_off) & size_mask;
            const size_t src_idx = (static_cast<size_t>(by) * base_stride + bx) * 4;
            const size_t dst_idx = (static_cast<size_t>(y) * width_px + x) * 4;

            const float px   = static_cast<float>(x) + 0.5f;
            const float py   = static_cast<float>(y) + 0.5f;
            const float dist = BtnRoundedSdf(px, py, cx, cy, half_w, half_h, radius);
            if (dist > 0.5f) {
                continue;
            }

            const float edge_alpha = std::clamp(0.5f - dist, 0.0f, 1.0f);
            const float inward     = -dist;
            const float u_norm     = static_cast<float>(x) * inv_w;
            const float dome       = std::sin(v_norm * 3.14159265f) * 6.0f +
                                     (0.45f - (u_norm * 0.35f + v_norm * 0.65f)) * 16.0f;

            float r = static_cast<float>(surface.rgba[src_idx + 0]) + dome * spec.scale_r;
            float g = static_cast<float>(surface.rgba[src_idx + 1]) + dome * spec.scale_g;
            float b = static_cast<float>(surface.rgba[src_idx + 2]) + dome * spec.scale_b;

            if (inward < bevel_w + 1.2f) {
                constexpr float kEps = 0.5f;
                const float gx = BtnRoundedSdf(px + kEps, py, cx, cy, half_w, half_h, radius) -
                                 BtnRoundedSdf(px - kEps, py, cx, cy, half_w, half_h, radius);
                const float gy = BtnRoundedSdf(px, py + kEps, cx, cy, half_w, half_h, radius) -
                                 BtnRoundedSdf(px, py - kEps, cx, cy, half_w, half_h, radius);
                const float glen = std::max(1e-5f, std::hypot(gx, gy));
                const float nx   = gx / glen;
                const float ny   = gy / glen;

                const float h_val     = surface.height[static_cast<size_t>(by) * base_stride + bx];
                const float light_dot = -(nx * 0.58f + ny * 0.81f) + (h_val - 0.5f) * 0.35f;

                const float profile      = (inward < bevel_w)
                                               ? std::sin(std::clamp(inward / bevel_w, 0.0f, 1.0f) * 3.14159265f)
                                               : 0.0f;
                const float inner_groove = std::exp(-((inward - 2.2f) * (inward - 2.2f)) / 0.65f) * 11.0f;
                const float bevel        = light_dot * profile * spec.bevel_strength - inner_groove;

                r += bevel * spec.bevel_r;
                g += bevel * spec.bevel_g;
                b += bevel * spec.bevel_b;

                if (inward < 1.15f) {
                    const float rim_t = std::clamp((1.15f - inward) / 1.15f, 0.0f, 1.0f);
                    r = r * (1.0f - rim_t * 0.72f) + spec.rim_r * rim_t;
                    g = g * (1.0f - rim_t * 0.72f) + spec.rim_g * rim_t;
                    b = b * (1.0f - rim_t * 0.72f) + spec.rim_b * rim_t;
                }
            }

            rgba[dst_idx + 0] = static_cast<uint8_t>(std::lround(std::clamp(r, 0.0f, 255.0f)));
            rgba[dst_idx + 1] = static_cast<uint8_t>(std::lround(std::clamp(g, 0.0f, 255.0f)));
            rgba[dst_idx + 2] = static_cast<uint8_t>(std::lround(std::clamp(b, 0.0f, 255.0f)));
            rgba[dst_idx + 3] = static_cast<uint8_t>(std::lround(edge_alpha * 255.0f));
        }
    }

    decoded.levels.push_back(std::move(rgba));
    const unsigned tbp = PortCreateTexture(decoded, PortTextureOwner::Other, nullptr);
    if (tbp == 0) {
        return nullptr;
    }

    CachedButtonTex &entry = cache[key];
    entry.tbp              = tbp;
    entry.tex.Initialize();
    std::strcpy(entry.tex.name, spec.tex_name);
    entry.tex.width  = width_px;
    entry.tex.height = height_px;
    entry.tex.bpp    = 4;
    entry.tex.block  = -1;
    entry.tex.tex0   = static_cast<u_long>(tbp) | (1ull << 34);
    entry.tex.tex1   = 0;
    return &entry.tex;
}

} // namespace

void BevelButton::Draw(const Rect &box, std::string_view text,
                       const ButtonStyle &style, int alpha) {
    if (box.Empty() || alpha <= 0) {
        return;
    }

    setbilinear(0);

    if (CTexture *gaiji_tex = detail::GetWhitePixelTex()) {
        detail::DrawRoundedRect(gaiji_tex, box.x + 2, box.y + 3, box.w, box.h,
                                0, 0, 0, (alpha * 75) >> 7, style.rounded);
    }

    const int seed_variant = (box.y / 13) + (box.x / 17) * 3;
    CTexture *btn_tex      = (box.w >= 16 && box.h >= 12)
                                 ? GetProceduralButtonTex(box.w, box.h, style.preset, style.rounded, seed_variant)
                                 : nullptr;

    if (btn_tex != nullptr) {
        const unsigned char mod = static_cast<unsigned char>(std::clamp(style.brightness, 0, 255));
        DrawMenu2DSprite(btn_tex,
                         CRect_i_(box.x, box.y, box.w, box.h),
                         CRect_i_(0, 0, box.w, box.h),
                         mod, mod, mod, alpha);
    } else if (CTexture *gaiji_tex = detail::GetWhitePixelTex()) {
        auto scale_c = [&](int c) -> unsigned char {
            const int modulated = (c * style.brightness) >> 7;
            const int gs_val    = (modulated * 128 + 127) / 255;
            return static_cast<unsigned char>(std::clamp(gs_val, 0, 255));
        };
        detail::DrawRoundedRect(gaiji_tex, box.x, box.y, box.w, box.h,
                                scale_c(214), scale_c(114), scale_c(60), alpha, style.rounded);
    }

    if (!text.empty()) {
        float     fitted_xscale = style.text_xscale;
        const int max_text_w    = std::max(16, box.w - 16);
        Label     measure_lbl(text, style.text_color, style.align, style.drop_shadow,
                              style.text_scale, style.text_weight, style.text_xscale);
        const int natural_w = measure_lbl.GetWidth();
        if (natural_w > max_text_w) {
            fitted_xscale = style.text_xscale * (static_cast<float>(max_text_w) / static_cast<float>(natural_w));
        }

        u8 dark_r = 76;
        u8 dark_g = 32;
        u8 dark_b = 14;
        u8 lite_r = 254;
        u8 lite_g = 184;
        u8 lite_b = 136;
        if (style.preset == ButtonPreset::AmberGold) {
            dark_r = 82;
            dark_g = 52;
            dark_b = 16;
            lite_r = 255;
            lite_g = 226;
            lite_b = 156;
        } else if (style.preset == ButtonPreset::StoneGreen) {
            dark_r = 24;
            dark_g = 68;
            dark_b = 54;
            lite_r = 172;
            lite_g = 240;
            lite_b = 216;
        }

        const Rect inner_face(box.x + 2, box.y + 2, std::max(1, box.w - 5), std::max(1, box.h - 4));
        TextStyle  txt_style{
            .color          = style.text_color,
            .drop_shadow    = style.drop_shadow && !style.recessed_text,
            .align          = style.align,
            .scale          = style.text_scale,
            .xscale         = fitted_xscale,
            .weight         = style.text_weight,
            .text_dx        = style.text_dx - 1,
            .text_dy        = style.text_dy,
            .use_custom_rgb = style.recessed_text,
            .fill_r         = 236,
            .fill_g         = 230,
            .fill_b         = 218,
            .recessed       = style.recessed_text,
            .recess_dark_r  = dark_r,
            .recess_dark_g  = dark_g,
            .recess_dark_b  = dark_b,
            .recess_light_r = lite_r,
            .recess_light_g = lite_g,
            .recess_light_b = lite_b,
        };
        const int text_alpha = (style.brightness < 0x80)
                                   ? ((alpha * std::max(0x48, style.brightness)) >> 7)
                                   : alpha;
        DrawTextBox(text, inner_face, txt_style, text_alpha);
    }
}

void BevelButton::Draw(int x, int y, int width, int height, std::string_view text,
                       u32 text_color, int alpha, ButtonPreset preset,
                       Alignment align, bool drop_shadow, FontWeight weight) {
    ButtonStyle style = (weight == FontWeight::Thin)
                            ? ButtonStyle::Thin(preset)
                            : ButtonStyle::Thick(preset);
    style.text_color  = text_color;
    style.drop_shadow = drop_shadow;
    style.align       = align;
    Draw(Rect(x, y, width, height), text, style, alpha);
}

void TextureButton::Draw(const Rect &box, CTexture *texture, const Rect &uv,
                         int alpha, std::string_view text,
                         const TextStyle &text_style) {
    if (texture != nullptr && !uv.Empty()) {
        setbilinear(0);
        DrawMenu2DSprite(texture, CRect_i_(box.x, box.y, box.w, box.h),
                         CRect_i_(uv.x, uv.y, uv.w, uv.h), alpha);
    }

    if (!text.empty()) {
        DrawTextBox(text, box, text_style, alpha);
    }
}

void TextureButton::Draw(int x, int y, int width, int height,
                         CTexture *texture, int u, int v, int uv_w, int uv_h,
                         int alpha, std::string_view text,
                         const TextStyle &text_style) {
    Draw(Rect(x, y, width, height), texture, Rect(u, v, uv_w, uv_h), alpha, text, text_style);
}

} // namespace UI