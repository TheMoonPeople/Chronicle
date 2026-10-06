#include "overlay.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace {

struct Glyph {
    char         c;
    std::uint8_t rows[kOverlayGlyphHeight];
};

// A glyph of the counter's font (tools/font/gen_overlay_font.py), as 8-bit coverage: the w x h bitmap's
// top-left corner lies x font pixels right of the pen and y below the line's top.
struct FontGlyph {
    char                 c;
    int                  advance;
    int                  x;
    int                  y;
    int                  w;
    int                  h;
    const std::uint8_t *bits;
};

// clang-format off
constexpr Glyph kPixelFont[] = {
    {'A', {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}}, {'B', {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E}},
    {'C', {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E}}, {'D', {0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E}},
    {'E', {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F}}, {'F', {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10}},
    {'G', {0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0F}}, {'H', {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}},
    {'I', {0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E}}, {'J', {0x07, 0x02, 0x02, 0x02, 0x02, 0x12, 0x0C}},
    {'K', {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11}}, {'L', {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F}},
    {'M', {0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11}}, {'N', {0x11, 0x11, 0x19, 0x15, 0x13, 0x11, 0x11}},
    {'O', {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}}, {'P', {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10}},
    {'Q', {0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D}}, {'R', {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11}},
    {'S', {0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E}}, {'T', {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04}},
    {'U', {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}}, {'V', {0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04}},
    {'W', {0x11, 0x11, 0x11, 0x15, 0x15, 0x15, 0x0A}}, {'X', {0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11}},
    {'Y', {0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04}}, {'Z', {0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F}},
    {'0', {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E}}, {'1', {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E}},
    {'2', {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F}}, {'3', {0x1F, 0x02, 0x04, 0x02, 0x01, 0x11, 0x0E}},
    {'4', {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02}}, {'5', {0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E}},
    {'6', {0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E}}, {'7', {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08}},
    {'8', {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E}}, {'9', {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C}},
    {'.', {0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C}}, {',', {0x00, 0x00, 0x00, 0x00, 0x0C, 0x04, 0x08}},
    {':', {0x00, 0x0C, 0x0C, 0x00, 0x0C, 0x0C, 0x00}}, {';', {0x00, 0x0C, 0x0C, 0x00, 0x0C, 0x04, 0x08}},
    {'/', {0x00, 0x01, 0x02, 0x04, 0x08, 0x10, 0x00}}, {'(', {0x02, 0x04, 0x08, 0x08, 0x08, 0x04, 0x02}},
    {')', {0x08, 0x04, 0x02, 0x02, 0x02, 0x04, 0x08}}, {'+', {0x00, 0x04, 0x04, 0x1F, 0x04, 0x04, 0x00}},
    {'-', {0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00}}, {'_', {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1F}},
    {'%', {0x18, 0x19, 0x02, 0x04, 0x08, 0x13, 0x03}}, {'\'', {0x04, 0x04, 0x08, 0x00, 0x00, 0x00, 0x00}},
    {'[', {0x0E, 0x08, 0x08, 0x08, 0x08, 0x08, 0x0E}}, {']', {0x0E, 0x02, 0x02, 0x02, 0x02, 0x02, 0x0E}},
    {'?', {0x0E, 0x11, 0x01, 0x02, 0x04, 0x00, 0x04}}, {'~', {0x00, 0x00, 0x08, 0x15, 0x02, 0x00, 0x00}},
};
// clang-format on

#include "overlay_font.inc"

const FontGlyph *FindFontGlyph(char c) {
    if (c >= 'a' && c <= 'z') {
        c = static_cast<char>(c - 'a' + 'A');
    }
    for (const FontGlyph &glyph : kFont) {
        if (glyph.c == c) {
            return &glyph;
        }
    }
    return c == '?' ? nullptr : FindFontGlyph('?');
}

constexpr std::uint8_t kBackdropAlpha = 0x60;

gfx::Vertex2D Vertex(float x, float y, std::uint8_t grey, std::uint8_t alpha) {
    return gfx::Vertex2D{
        x, y, 0.0f, 0.0f, 0.0f, {grey, grey, grey, alpha}
    };
}

} // namespace

const std::uint8_t *OverlayGlyph(char c) {
    if (c >= 'a' && c <= 'z') {
        c = static_cast<char>(c - 'a' + 'A');
    }
    if (c == ' ') {
        return nullptr;
    }
    for (const Glyph &glyph : kPixelFont) {
        if (glyph.c == c) {
            return glyph.rows;
        }
    }
    return OverlayGlyph('?');
}

int OverlayPixelSize(const gfx::LogicalMapping &mapping) {
    return std::max(1, static_cast<int>(std::lround(std::min(mapping.scale_x, mapping.scale_y))));
}

void OverlayDrawText(std::string_view text, const gfx::LogicalMapping &mapping, int pixel, int x, int y) {
    if (text.empty() || mapping.scale_x <= 0.0f || mapping.scale_y <= 0.0f) {
        return;
    }
    // Quads on whole target pixels, given in the logical space Draw2D maps back onto them.
    auto quad = [&](std::vector<gfx::Vertex2D> &out, int left, int top, int right, int bottom,
                    std::uint8_t grey, std::uint8_t alpha) {
        float x0 = (static_cast<float>(left) - mapping.offset_x) / mapping.scale_x;
        float x1 = (static_cast<float>(right) - mapping.offset_x) / mapping.scale_x;
        float y0 = (static_cast<float>(top) - mapping.offset_y) / mapping.scale_y;
        float y1 = (static_cast<float>(bottom) - mapping.offset_y) / mapping.scale_y;
        out.push_back(Vertex(x0, y0, grey, alpha));
        out.push_back(Vertex(x1, y0, grey, alpha));
        out.push_back(Vertex(x1, y1, grey, alpha));
        out.push_back(Vertex(x0, y1, grey, alpha));
    };

    int columns = 0;
    for (char c : text) {
        if (const FontGlyph *glyph = FindFontGlyph(c)) {
            columns += glyph->advance;
        }
    }
    std::vector<gfx::Vertex2D> backdrop;
    quad(backdrop, x, y, x + (columns + 2 * kOverlayPadding) * pixel,
         y + (kFontHeight + 2 * kOverlayPadding) * pixel, 0, kBackdropAlpha);

    // One quad per run of equal coverage in a glyph row; coverage becomes alpha (0x80 is opaque).
    std::vector<gfx::Vertex2D> glyphs;
    int                        pen = x + kOverlayPadding * pixel;
    int                        top = y + kOverlayPadding * pixel;
    for (char c : text) {
        const FontGlyph *glyph = FindFontGlyph(c);
        if (glyph == nullptr) {
            continue;
        }
        for (int row = 0; row < glyph->h; row++) {
            const std::uint8_t *line = glyph->bits + row * glyph->w;
            for (int column = 0; column < glyph->w;) {
                int end = column;
                while (end < glyph->w && line[end] == line[column]) {
                    end++;
                }
                if (line[column] != 0) {
                    int left = pen + (glyph->x + column) * pixel;
                    int y0 = top + (glyph->y + row) * pixel;
                    quad(glyphs, left, y0, pen + (glyph->x + end) * pixel, y0 + pixel, 0xFF,
                         static_cast<std::uint8_t>((line[column] * 0x80 + 127) / 255));
                }
                column = end;
            }
        }
        pen += glyph->advance * pixel;
    }

    gfx::DrawState blended;
    blended.blend = true;
    gfx::Draw2D(gfx::Primitive::Quads, backdrop, {}, blended);
    if (!glyphs.empty()) {
        gfx::Draw2D(gfx::Primitive::Quads, glyphs, {}, blended);
    }
}

gfx::DisplayListRef OverlayRecord(std::string_view text) {
    gfx::BeginRecording();
    if (!gfx::Recording()) {
        return nullptr;
    }
    // Draw2D places depthless 2D by the UI mapping; the counter's size follows the window alone.
    int pixel = OverlayPixelSize(gfx::GetLogicalMapping(gfx::kMainTarget));
    OverlayDrawText(text, gfx::GetUiMapping(gfx::kMainTarget), pixel, pixel * kOverlayPadding,
                    pixel * kOverlayPadding);
    return gfx::EndRecording();
}

bool OverlayRate::Count(Clock::time_point now) {
    if (!started_) {
        started_ = true;
        start_ = now;
        events_ = 0;
        return false;
    }
    events_++;
    Clock::duration elapsed = now - start_;
    if (elapsed < window_) {
        return false;
    }
    rate_ = static_cast<double>(events_) / std::chrono::duration<double>(elapsed).count();
    start_ = now;
    events_ = 0;
    return true;
}
