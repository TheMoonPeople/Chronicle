#pragma once

#include <cstddef>
#include <filesystem>
#include <vector>

#include "gfx/gfx.hpp"

namespace capfont {

bool Load(const std::filesystem::path &file);
bool Loaded();

struct Glyph {
    float u0, v0, u1, v1;
    float x_offset, y_offset;
    float width, height;
    float advance = 0;
};

bool GetGlyph(char32_t ch, int em_px, Glyph &out);
bool GetOutlinedGlyph(char32_t ch, int em_px, int outline_px, Glyph &out);
// The glyph drawn stretch times as wide (to a sixty-fourth) before it is outlined, so its edges stay the font's own
// and the outline is as thick at the sides as at the top; with smooth, drawn and outlined four times finer so the
// outline's edge is anti-aliased as finely as the letter's. Letters the font composes have none stretched.
bool GetStretchedGlyph(char32_t ch, int em_px, int outline_px, float stretch, bool smooth, Glyph &out);
bool RenderGlyph(char32_t ch, int em_px, std::vector<unsigned char> &alpha, int &w, int &h, int &x0, int &y0);

// Slot 1: Optional thin variant font
bool LoadVariant(const std::filesystem::path &file);
bool VariantLoaded();
void UseVariant(bool variant);

// Slot 2: Weapon status font (SourceSansProSemibold.ttf)
bool LoadStatusFont(const std::filesystem::path &file);
bool LoadStatusFontMemory(const unsigned char *data, std::size_t size);
bool StatusFontLoaded();

// Selects active font slot: 0 = Main, 1 = Thin, 2 = Status (SourceSansProSemibold)
void UseFontSlot(int slot);

bool AddImage(const std::vector<unsigned char> &rgba, int w, int h, Glyph &out);
unsigned Generation();
gfx::TextureBinding Binding();

struct Metrics {
    float ascent;
    float descent;
    float advance;
    float cap;
};

Metrics GetMetrics();

} // namespace capfont