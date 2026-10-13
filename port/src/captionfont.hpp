#pragma once

#include <filesystem>
#include <vector>

#include "gfx/gfx.hpp"

namespace capfont {

/**
 * Replaces the caption font; returns false for an unreadable or invalid font.
 */
bool Load(const std::filesystem::path &file);

bool Loaded();

/**
 * Atlas texel coordinates and pixel metrics relative to the glyph baseline.
 */
struct Glyph {
    float u0, v0, u1, v1;
    float x_offset, y_offset;
    float width, height;
    float advance = 0;
};

/**
 * Caches an em-sized glyph; false indicates a missing glyph or atlas failure.
 */
bool GetGlyph(char32_t ch, int em_px, Glyph &out);

/**
 * Caches glyph coverage dilated by outline_px pixels, from zero to 64.
 */
bool GetOutlinedGlyph(char32_t ch, int em_px, int outline_px, Glyph &out);

/**
 * Rasterizes glyph alpha coverage; x0 and y0 are pixel offsets from the baseline.
 */
bool RenderGlyph(char32_t ch, int em_px, std::vector<unsigned char> &alpha, int &w, int &h, int &x0, int &y0);

/**
 * Loads a lighter font used by UseVariant when available.
 */
bool LoadVariant(const std::filesystem::path &file);
bool VariantLoaded();
void UseVariant(bool variant);

/**
 * Uploads a width-by-height RGBA image into the glyph atlas.
 */
bool AddImage(const std::vector<unsigned char> &rgba, int w, int h, Glyph &out);

/**
 * Identifies atlas resets so external cached coordinates can be invalidated.
 */
unsigned Generation();

gfx::TextureBinding Binding();

/**
 * Font proportions as fractions of an em; descent is positive.
 */
struct Metrics {
    float ascent;
    float descent;
    float advance;
    float cap; /**< Capital ink height. */
};

Metrics GetMetrics();

} // namespace capfont
