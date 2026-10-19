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
 * Material and noise synthesis parameters for a procedural window surface preset.
 */
struct SurfaceSpec {
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
    float       ridge_weight;    /**< Soft-ridged noise weight. */
    float       tooth_weight;    /**< Fine surface grain heightmap weight. */
    float       bump_strength;   /**< Directional bump shading multiplier. */
    float       dither_strength; /**< Ordered Bayer dither amplitude. */
    float       bevel_strength;  /**< Edge bevel lighting multiplier. */
};

/**
 * Cached 512x512 tileable base texture for a procedural window surface.
 */
struct BaseSurfaceData {
    std::vector<uint8_t> rgba;            /**< Tileable RGBA8 color buffer. */
    int                  size_mask = 511; /**< Coordinate wrap mask for power-of-two dimensions. */
};

/**
 * Cached VRAM texture handle and wrapper for a rendered window plate.
 */
struct CachedWindowTex {
    CTexture tex{};   /**< Engine texture descriptor. */
    unsigned tbp = 0; /**< Host texture key. */
};

/**
 * Surface specification for the blue-slate stone window preset.
 */
constexpr SurfaceSpec kBlueStoneSpec{
    .plate_path      = "textures/plates/ui_stone.png",
    .tex_name        = "ui_stone_win",
    .seed_offset     = 0,
    .base_r          = 78.0f,
    .base_g          = 92.0f,
    .base_b          = 116.0f,
    .scale_r         = 0.88f,
    .scale_g         = 1.00f,
    .scale_b         = 1.14f,
    .bevel_r         = 1.00f,
    .bevel_g         = 1.05f,
    .bevel_b         = 1.12f,
    .rim_r           = 22.0f,
    .rim_g           = 28.0f,
    .rim_b           = 40.0f,
    .macro_weight    = 34.0f,
    .med_weight      = 22.0f,
    .detail_weight   = 12.0f,
    .ridge_weight    = 14.0f,
    .tooth_weight    = 0.085f,
    .bump_strength   = 32.0f,
    .dither_strength = 2.2f,
    .bevel_strength  = 40.0f,
};

/**
 * Surface specification for the parchment paper window preset.
 */
constexpr SurfaceSpec kPaperSpec{
    .plate_path      = "textures/plates/ui_paper.png",
    .tex_name        = "ui_paper_win",
    .seed_offset     = 1000,
    .base_r          = 160.0f,
    .base_g          = 128.0f,
    .base_b          = 90.0f,
    .scale_r         = 0.95f,
    .scale_g         = 0.88f,
    .scale_b         = 0.72f,
    .bevel_r         = 1.05f,
    .bevel_g         = 0.95f,
    .bevel_b         = 0.80f,
    .rim_r           = 58.0f,
    .rim_g           = 44.0f,
    .rim_b           = 28.0f,
    .macro_weight    = 30.0f,
    .med_weight      = 20.0f,
    .detail_weight   = 22.0f,
    .ridge_weight    = 12.0f,
    .tooth_weight    = 0.075f,
    .bump_strength   = 40.0f,
    .dither_strength = 4.0f,
    .bevel_strength  = 64.0f,
};

/**
 * Returns the surface parameters for a window preset.
 */
const SurfaceSpec &GetSurfaceSpec(WindowPreset preset) {
    return (preset == WindowPreset::Paper) ? kPaperSpec : kBlueStoneSpec;
}

/**
 * Computes a deterministic 2D hash in [0, 1] for integer lattice coordinates.
 */
float Hash2D(int ix, int iy, int seed) {
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
float SmoothNoisePow2(float x, float y, int px_mask, int py_mask, int seed) {
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

    const float v00 = Hash2D(ix0, iy0, seed);
    const float v10 = Hash2D(ix1, iy0, seed);
    const float v01 = Hash2D(ix0, iy1, seed);
    const float v11 = Hash2D(ix1, iy1, seed);

    const float top = v00 + (v10 - v00) * ux;
    const float bot = v01 + (v11 - v01) * ux;
    return top + (bot - top) * uy;
}

/**
 * Evaluates tileable fractional Brownian motion over a 512x512 domain.
 */
float FbmPeriodic512(float x, float y, int cell_size, int octaves, int seed) {
    float val      = 0.0f;
    float amp      = 0.5f;
    float inv_cell = 1.0f / static_cast<float>(cell_size);
    int   period   = std::max(1, 512 / cell_size);

    for (int i = 0; i < octaves; ++i) {
        const int mask = period - 1;
        val += amp * SmoothNoisePow2(x * inv_cell, y * inv_cell, mask, mask, seed + i * 17);
        inv_cell *= 2.0f;
        period <<= 1;
        amp *= 0.5f;
    }
    return val;
}

/**
 * Evaluates tileable soft-ridged multifractal noise over a 512x512 domain.
 */
float SoftRidgedPeriodic512(float x, float y, int cell_size, int octaves, int seed) {
    float val      = 0.0f;
    float amp      = 0.5f;
    float inv_cell = 1.0f / static_cast<float>(cell_size);
    int   period   = std::max(1, 512 / cell_size);

    for (int i = 0; i < octaves; ++i) {
        const int   mask  = period - 1;
        const float n     = SmoothNoisePow2(x * inv_cell, y * inv_cell, mask, mask, seed + i * 29);
        const float ridge = 1.0f - std::sqrt((n * 2.0f - 1.0f) * (n * 2.0f - 1.0f) + 0.08f);
        val += ridge * amp;
        inv_cell *= 2.0f;
        period <<= 1;
        amp *= 0.5f;
    }
    return val;
}

/**
 * Applies a separable 5-tap periodic Gaussian-like blur to a square float buffer.
 */
void BlurPeriodic512(std::vector<float> &field, int size, int passes) {
    const int          mask = size - 1;
    std::vector<float> tmp(field.size(), 0.0f);

    for (int p = 0; p < passes; ++p) {
        for (int y = 0; y < size; ++y) {
            const size_t row = static_cast<size_t>(y) * size;
            for (int x = 0; x < size; ++x) {
                const float sum = field[row + ((x - 2) & mask)] * 1.0f +
                                  field[row + ((x - 1) & mask)] * 4.0f +
                                  field[row + (x & mask)] * 6.0f +
                                  field[row + ((x + 1) & mask)] * 4.0f +
                                  field[row + ((x + 2) & mask)] * 1.0f;
                tmp[row + x] = sum * (1.0f / 16.0f);
            }
        }
        for (int y = 0; y < size; ++y) {
            const size_t r_m2 = static_cast<size_t>((y - 2) & mask) * size;
            const size_t r_m1 = static_cast<size_t>((y - 1) & mask) * size;
            const size_t r_0  = static_cast<size_t>(y) * size;
            const size_t r_p1 = static_cast<size_t>((y + 1) & mask) * size;
            const size_t r_p2 = static_cast<size_t>((y + 2) & mask) * size;
            for (int x = 0; x < size; ++x) {
                const float sum = tmp[r_m2 + x] * 1.0f +
                                  tmp[r_m1 + x] * 4.0f +
                                  tmp[r_0 + x] * 6.0f +
                                  tmp[r_p1 + x] * 4.0f +
                                  tmp[r_p2 + x] * 1.0f;
                field[r_0 + x] = sum * (1.0f / 16.0f);
            }
        }
    }
}

/**
 * Computes the signed distance from a point to a rounded box centered at (cx, cy).
 */
float RoundedBoxSdf(float px, float py, float cx, float cy, float half_w, float half_h, float radius) {
    const float dx    = std::fabs(px - cx) - (half_w - radius);
    const float dy    = std::fabs(py - cy) - (half_h - radius);
    const float out_d = std::hypot(std::max(dx, 0.0f), std::max(dy, 0.0f));
    const float in_d  = std::min(std::max(dx, dy), 0.0f);
    return out_d + in_d - radius;
}

/**
 * Builds or returns the cached 512x512 tileable base surface for a window preset.
 */
const BaseSurfaceData &GetBaseSurface(WindowPreset preset) {
    static BaseSurfaceData surfaces[2];
    const int              idx = (preset == WindowPreset::Paper) ? 1 : 0;
    BaseSurfaceData       &out = surfaces[idx];

    if (!out.rgba.empty()) {
        return out;
    }

    const SurfaceSpec    &spec       = GetSurfaceSpec(preset);
    std::filesystem::path custom_png = LocalizeFindFile(spec.plate_path);
    int                   png_w      = 0;
    int                   png_h      = 0;
    std::vector<uint8_t>  loaded;
    if (!custom_png.empty() && PngLoadRgba(custom_png, png_w, png_h, loaded) &&
        png_w == png_h && (png_w == 256 || png_w == 512)) {
        out.size_mask = png_w - 1;
        out.rgba      = std::move(loaded);
        return out;
    }

    constexpr int kSize = 512;
    constexpr int kMask = kSize - 1;
    out.size_mask       = kMask;

    const int          s = spec.seed_offset;
    std::vector<float> height_map(static_cast<size_t>(kSize) * kSize, 0.0f);
    std::vector<float> tone_map(static_cast<size_t>(kSize) * kSize, 0.0f);

    for (int y = 0; y < kSize; ++y) {
        for (int x = 0; x < kSize; ++x) {
            const float fx = static_cast<float>(x);
            const float fy = static_cast<float>(y);

            const float wx = (FbmPeriodic512(fx, fy, 64, 3, s + 501) - 0.5f) * 14.0f;
            const float wy = (FbmPeriodic512(fx, fy, 64, 3, s + 601) - 0.5f) * 14.0f;

            const float macro  = FbmPeriodic512(fx + wx, fy + wy, 128, 4, s + 101);
            const float med    = FbmPeriodic512(fx - wy * 0.5f, fy + wx * 0.5f, 64, 4, s + 303);
            const float detail = FbmPeriodic512(fx, fy, 32, 3, s + 202);
            const float soft_r = SoftRidgedPeriodic512(fx + wx * 0.4f, fy - wy * 0.4f, 64, 2, s + 404);

            const size_t i = static_cast<size_t>(y) * kSize + x;
            height_map[i]  = macro * 0.35f + med * 0.38f + detail * 0.15f + soft_r * 0.12f;
            tone_map[i]    = (macro - 0.48f) * spec.macro_weight +
                             (med - 0.48f) * spec.med_weight +
                             (detail - 0.47f) * spec.detail_weight +
                             (soft_r - 0.32f) * spec.ridge_weight;
        }
    }

    BlurPeriodic512(height_map, kSize, 2);
    BlurPeriodic512(tone_map, kSize, 1);

    for (int y = 0; y < kSize; ++y) {
        for (int x = 0; x < kSize; ++x) {
            const float fx    = static_cast<float>(x);
            const float fy    = static_cast<float>(y);
            const float tooth = SmoothNoisePow2(fx * 0.5f, fy * 0.5f, 255, 255, s + 808) * 0.65f +
                                Hash2D(x, y, s + 909) * 0.35f;
            height_map[static_cast<size_t>(y) * kSize + x] += tooth * spec.tooth_weight;
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

            const float bump   = ((h_tl - h_br) * 0.7f + (h_t - h_b) * 0.3f) * spec.bump_strength;
            const float dither = kBayer4x4[y & 3][x & 3] * spec.dither_strength;
            const float delta  = tone_map[i] + bump + dither;

            const float r = spec.base_r + delta * spec.scale_r;
            const float g = spec.base_g + delta * spec.scale_g;
            const float b = spec.base_b + delta * spec.scale_b;

            const size_t out_idx  = i * 4;
            out.rgba[out_idx + 0] = static_cast<uint8_t>(std::lround(std::clamp(r, 0.0f, 255.0f) / 3.0f) * 3);
            out.rgba[out_idx + 1] = static_cast<uint8_t>(std::lround(std::clamp(g, 0.0f, 255.0f) / 3.0f) * 3);
            out.rgba[out_idx + 2] = static_cast<uint8_t>(std::lround(std::clamp(b, 0.0f, 255.0f) / 3.0f) * 3);
            out.rgba[out_idx + 3] = 255;
        }
    }

    return out;
}

/**
 * Generates or retrieves a cached beveled window texture of the requested dimensions.
 */
CTexture *GetProceduralWindowTex(int width_px, int height_px, WindowPreset preset) {
    using CacheKey = std::tuple<int, int, int>;
    static std::map<CacheKey, CachedWindowTex> cache;

    const CacheKey key{width_px, height_px, static_cast<int>(preset)};
    auto           it = cache.find(key);
    if (it != cache.end() && it->second.tbp != 0 && PortKeyLive(it->second.tbp)) {
        return &it->second.tex;
    }

    const SurfaceSpec     &spec        = GetSurfaceSpec(preset);
    const BaseSurfaceData &surface     = GetBaseSurface(preset);
    const int              size_mask   = surface.size_mask;
    const int              base_stride = size_mask + 1;

    PortDecodedTexture decoded{};
    decoded.width     = static_cast<unsigned>(width_px);
    decoded.height    = static_cast<unsigned>(height_px);
    decoded.format    = gfx::TextureFormat::Rgba8;
    decoded.has_alpha = true;

    std::vector<uint8_t> rgba(static_cast<size_t>(width_px) * height_px * 4, 0);

    const float     cx      = width_px * 0.5f;
    const float     cy      = height_px * 0.5f;
    const float     half_w  = width_px * 0.5f - 1.5f;
    const float     half_h  = height_px * 0.5f - 1.5f;
    constexpr float kRadius = 11.0f;
    constexpr float kBevelW = 12.0f;
    const float     inv_w   = 1.0f / static_cast<float>(std::max(1, width_px));
    const float     inv_h   = 1.0f / static_cast<float>(std::max(1, height_px));

    for (int y = 0; y < height_px; ++y) {
        const int   by          = y & size_mask;
        const bool  near_y_edge = (y < 16 || y >= height_px - 16);
        const float v_norm      = static_cast<float>(y) * inv_h;

        for (int x = 0; x < width_px; ++x) {
            const int    bx      = x & size_mask;
            const size_t src_idx = (static_cast<size_t>(by) * base_stride + bx) * 4;
            const size_t dst_idx = (static_cast<size_t>(y) * width_px + x) * 4;

            const float u_norm  = static_cast<float>(x) * inv_w;
            const float ambient = (0.45f - (u_norm * 0.45f + v_norm * 0.55f)) * 14.0f;

            if (!near_y_edge && x >= 16 && x < width_px - 16) {
                rgba[dst_idx + 0] = static_cast<uint8_t>(std::clamp(static_cast<int>(surface.rgba[src_idx + 0] + ambient * spec.scale_r), 0, 255));
                rgba[dst_idx + 1] = static_cast<uint8_t>(std::clamp(static_cast<int>(surface.rgba[src_idx + 1] + ambient * spec.scale_g), 0, 255));
                rgba[dst_idx + 2] = static_cast<uint8_t>(std::clamp(static_cast<int>(surface.rgba[src_idx + 2] + ambient * spec.scale_b), 0, 255));
                rgba[dst_idx + 3] = 255;
                continue;
            }

            const float px   = static_cast<float>(x) + 0.5f;
            const float py   = static_cast<float>(y) + 0.5f;
            const float dist = RoundedBoxSdf(px, py, cx, cy, half_w, half_h, kRadius);
            if (dist > 0.5f) {
                continue;
            }

            const float edge_alpha = std::clamp(0.5f - dist, 0.0f, 1.0f);
            const float inward     = -dist;

            float r = static_cast<float>(surface.rgba[src_idx + 0]) + ambient * spec.scale_r;
            float g = static_cast<float>(surface.rgba[src_idx + 1]) + ambient * spec.scale_g;
            float b = static_cast<float>(surface.rgba[src_idx + 2]) + ambient * spec.scale_b;

            if (inward < kBevelW) {
                constexpr float kEps = 0.5f;
                const float gx = RoundedBoxSdf(px + kEps, py, cx, cy, half_w, half_h, kRadius) -
                                 RoundedBoxSdf(px - kEps, py, cx, cy, half_w, half_h, kRadius);
                const float gy = RoundedBoxSdf(px, py + kEps, cx, cy, half_w, half_h, kRadius) -
                                 RoundedBoxSdf(px, py - kEps, cx, cy, half_w, half_h, kRadius);
                const float glen = std::max(1e-5f, std::hypot(gx, gy));
                const float nx   = gx / glen;
                const float ny   = gy / glen;

                const float light_dot    = -(nx * 0.65f + ny * 0.76f);
                const float profile      = std::sin(std::min(1.0f, inward / kBevelW) * 3.14159265f);
                const float inner_groove = std::exp(-((inward - 9.5f) * (inward - 9.5f)) / 3.5f) * 6.0f;
                const float bevel        = light_dot * profile * spec.bevel_strength - inner_groove;

                r += bevel * spec.bevel_r;
                g += bevel * spec.bevel_g;
                b += bevel * spec.bevel_b;

                if (inward < 1.8f) {
                    const float rim_t = std::clamp((1.8f - inward) / 1.8f, 0.0f, 1.0f);
                    r = r * (1.0f - rim_t * 0.62f) + spec.rim_r * rim_t;
                    g = g * (1.0f - rim_t * 0.62f) + spec.rim_g * rim_t;
                    b = b * (1.0f - rim_t * 0.62f) + spec.rim_b * rim_t;
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

    CachedWindowTex &entry = cache[key];
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

void TrackerWindow::Draw(int x, int y, int width_px, int height_px, int alpha, WindowPreset preset) {
    if (preset == WindowPreset::None || width_px <= 0 || height_px <= 0 || alpha <= 0) {
        return;
    }
    if (preset == WindowPreset::DarkTranslucent) {
        HelpBox::Draw(x, y, width_px, height_px, alpha);
        return;
    }
    if (preset == WindowPreset::Recessed) {
        DrawIndentation(Rect(x, y, width_px, height_px), true, alpha);
        return;
    }

    CTexture *surface = (width_px >= 48 && height_px >= 48)
                            ? GetProceduralWindowTex(width_px, height_px, preset)
                            : nullptr;
    if (surface == nullptr) {
        const float w = std::max(0.0f, (width_px - 28) / 16.0f);
        const float h = std::max(0.0f, (height_px - 76) / 16.0f);
        MenuHelpWinDraw(x, y, w, h, alpha);
        return;
    }

    setbilinear(0);
    DrawMenu2DSprite(surface,
                     CRect_i_(x, y, width_px, height_px),
                     CRect_i_(0, 0, width_px, height_px),
                     alpha);
}

void TrackerWindow::DrawIndentation(const Rect &box, bool dark_variant, int alpha) {
    if (box.Empty() || alpha <= 0) {
        return;
    }

    CTexture *tex = detail::GetWhitePixelTex();
    if (tex == nullptr) {
        return;
    }

    const int rim_light_a  = (alpha * (dark_variant ? 42 : 55)) >> 7;
    const int rim_shadow_a = (alpha * (dark_variant ? 95 : 70)) >> 7;
    const int mid_shadow_a = (alpha * (dark_variant ? 75 : 52)) >> 7;
    const int floor_a      = (alpha * (dark_variant ? 56 : 36)) >> 7;

    detail::DrawRoundedRect(tex, box.x + 1, box.y + 1, box.w, box.h, 255, 248, 230, rim_light_a, true);
    detail::DrawRoundedRect(tex, box.x, box.y, box.w - 1, box.h - 1, 0, 0, 0, rim_shadow_a, true);

    if (box.w > 4 && box.h > 4) {
        detail::DrawRoundedRect(tex, box.x + 1, box.y + 1, box.w - 2, box.h - 2, 0, 0, 0, mid_shadow_a, true);
    }

    if (box.w > 6 && box.h > 6) {
        detail::DrawRoundedRect(tex, box.x + 2, box.y + 2, box.w - 3, box.h - 3, 0, 0, 0, floor_a, false);
    }
}

void TrackerWindow::DrawStoneBanner(const Rect &box, int alpha, int brightness,
                                    std::string_view title, const TextStyle &title_style) {
    if (box.Empty() || alpha <= 0) {
        return;
    }

    CTexture *wep_tex = detail::GetWepStatusTex();
    if (wep_tex == nullptr) {
        DrawIndentation(box, false, alpha);
    } else {
        setbilinear(0);
        const int left_w  = std::min(40, box.w / 3);
        const int right_w = std::min(32, box.w / 3);
        const int mid_w   = std::max(0, box.w - left_w - right_w);

        DrawMenu2DSprite(wep_tex, CRect_i_(box.x, box.y, left_w, box.h),
                         CRect_i_(0xE0, 0x1A8, 0x28, 0x20), brightness, brightness, brightness, alpha);
        if (mid_w > 0) {
            DrawMenu2DSprite(wep_tex, CRect_i_(box.x + left_w, box.y, mid_w, box.h),
                             CRect_i_(0x108, 0x1A8, 0x78, 0x20), brightness, brightness, brightness, alpha);
        }
        if (right_w > 0) {
            DrawMenu2DSprite(wep_tex, CRect_i_(box.x + left_w + mid_w, box.y, right_w, box.h),
                             CRect_i_(0xD4, 0x1D4, 0x20, 0x20), brightness, brightness, brightness, alpha);
        }
    }

    if (!title.empty()) {
        MenuTextureReload(detail::kMessageTextureBlock);
        Label lbl(title, title_style);
        lbl.Draw(box.CenterX(), box.y + 1, alpha);
    }
}

void HelpBox::Draw(int x, int y, int width_px, int height_px, int alpha) {
    setbilinear(0);
    MenuTextureReload(BtlMenuReadBlock);
    const float w = std::max(0.0f, (width_px - 48) / 16.0f);
    const float h = std::max(0.0f, (height_px - 44) / 16.0f);
    MenuHelpWinDraw(x, y, w, h, alpha);
}

void HelpBox::DrawTitled(int box_x, int bottom_y, int box_w, int text_offset_x,
                         std::string_view title, std::string_view desc,
                         int alpha, int line_height) {
    if (title.empty() || desc.empty() || alpha <= 0) {
        return;
    }

    MenuTextureReload(detail::kMessageTextureBlock);
    const int                draw_x     = box_x + (text_offset_x > 0 ? text_offset_x : 22);
    const int                max_text_w = box_w - 46;
    std::vector<std::string> lines      = WrapText(desc, max_text_w);
    const int                num_lines  = std::max(1, static_cast<int>(lines.size()));
    const int                box_h      = 32 + (num_lines * line_height);
    const int                box_y      = bottom_y - box_h;

    Draw(box_x, box_y, box_w, box_h, alpha);

    MenuTextureReload(detail::kMessageTextureBlock);
    const int title_y = box_y + 12;
    const int desc_y  = title_y + 24;

    Label(title, ColorWhite).Draw(draw_x, title_y, alpha);
    for (size_t i = 0; i < lines.size(); ++i) {
        Label(lines[i], ColorWhite).Draw(draw_x, desc_y + static_cast<int>(i) * line_height, alpha);
    }
}

void Window::Draw(int x, int y, int width_px, int height_px, int alpha, WindowPreset preset) {
    TrackerWindow::Draw(x, y, width_px, height_px, alpha, preset);
}

void TabPlate::Draw(int left, int right, int y, int alpha) {
    constexpr int kPlateSide   = 24;
    constexpr int kPlateHeight = 44;
    const float   middle       = std::max(0.0f, static_cast<float>(right - left + 28 - 2 * kPlateSide) / 16.0f);
    MenuHelpWinDraw(left - 14, y - (kPlateHeight - 20) / 2, middle, 0.0f, alpha);
}

void Divider::DrawVertical(int x, int top_y, int bottom_y, int alpha) {
    CTexture *tex = detail::GetWhitePixelTex();
    if (tex == nullptr || bottom_y <= top_y) {
        return;
    }
    const CRect_i_ uv_white(0x6A, 0x6E, 1, 1);
    const int      h = bottom_y - top_y;

    DrawMenu2DSprite(tex, CRect_i_(x - 1, top_y, 1, h), uv_white, 0, 0, 0, (alpha * 50) >> 7);
    DrawMenu2DSprite(tex, CRect_i_(x, top_y, 1, h), uv_white, 0, 0, 0, (alpha * 95) >> 7);
    DrawMenu2DSprite(tex, CRect_i_(x + 1, top_y, 1, h), uv_white, 0, 0, 0, (alpha * 65) >> 7);
    DrawMenu2DSprite(tex, CRect_i_(x + 2, top_y + 1, 1, h - 1), uv_white, 255, 248, 230, (alpha * 48) >> 7);
}

void Divider::DrawHorizontal(int left_x, int right_x, int y, int alpha) {
    CTexture *tex = detail::GetWhitePixelTex();
    if (tex == nullptr || right_x <= left_x) {
        return;
    }
    const CRect_i_ uv_white(0x6A, 0x6E, 1, 1);
    const int      w = right_x - left_x;

    DrawMenu2DSprite(tex, CRect_i_(left_x, y - 1, w, 1), uv_white, 0, 0, 0, (alpha * 50) >> 7);
    DrawMenu2DSprite(tex, CRect_i_(left_x, y, w, 1), uv_white, 0, 0, 0, (alpha * 95) >> 7);
    DrawMenu2DSprite(tex, CRect_i_(left_x, y + 1, w, 1), uv_white, 0, 0, 0, (alpha * 65) >> 7);
    DrawMenu2DSprite(tex, CRect_i_(left_x + 1, y + 2, w - 1, 1), uv_white, 255, 248, 230, (alpha * 48) >> 7);
}

void SubWindow::DrawBackground(int alpha) const {
    if (m_preset != WindowPreset::None) {
        TrackerWindow::Draw(m_outer, alpha, m_preset);
    }
}

void SubWindow::DrawBanner(std::string_view title, int banner_w, int banner_h,
                           int y_offset, int alpha) const {
    const Rect banner_box = m_outer.Anchored(Anchor::TopCenter, banner_w, banner_h, 0, y_offset);
    TrackerWindow::DrawStoneBanner(banner_box, alpha, 0x80, title);
}

void SubWindow::DrawAnchoredLabel(std::string_view text, Anchor anchor, const TextStyle &style,
                                  int dx, int dy, int alpha) const {
    Label lbl(text, style);
    lbl.DrawAnchored(m_content, anchor, dx, dy, alpha);
}

std::pair<SubWindow, SubWindow> SubWindow::SplitVertical(int left_w, int gap,
                                                         bool draw_divider, int alpha) const {
    int divider_x          = 0;
    auto [left_r, right_r] = m_content.SplitVertical(left_w, gap, &divider_x);
    if (draw_divider) {
        Divider::DrawVertical(divider_x, m_content.Top(), m_content.Bottom(), alpha);
    }
    return {SubWindow(left_r), SubWindow(right_r)};
}

std::pair<SubWindow, SubWindow> SubWindow::SplitHorizontal(int top_h, int gap,
                                                           bool draw_divider, int alpha) const {
    int divider_y       = 0;
    auto [top_r, bot_r] = m_content.SplitHorizontal(top_h, gap, &divider_y);
    if (draw_divider) {
        Divider::DrawHorizontal(m_content.Left(), m_content.Right(), divider_y, alpha);
    }
    return {SubWindow(top_r), SubWindow(bot_r)};
}

} // namespace UI