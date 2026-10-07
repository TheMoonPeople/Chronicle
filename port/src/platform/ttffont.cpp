#include "platform/ttffont.hpp"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <map>
#include <vector>

#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "stb_truetype.h"

#include "platform/paths.hpp"

namespace ttffont {

namespace {

constexpr int kAtlasSize = 2048;
// Space round each bitmap, so linear sampling at a quad's edge reads transparent texels.
constexpr int kPad = 1;

struct Font {
    std::vector<unsigned char> data;
    stbtt_fontinfo             info = {};
    bool                       loaded = false;
};

struct Atlas {
    gfx::TextureHandle                        texture = gfx::kNullTexture;
    int                                       x = 0;
    int                                       y = 0;
    int                                       row_height = 0;
    std::map<std::pair<char32_t, int>, Glyph> glyphs;
    std::map<std::pair<char32_t, int>, bool>  missing;
};

Font  g_font;
Atlas g_atlas;

void ResetAtlas() {
    if (g_atlas.texture != gfx::kNullTexture) {
        gfx::DestroyTexture(g_atlas.texture);
    }
    g_atlas = {};
}

bool EnsureAtlas() {
    if (g_atlas.texture == gfx::kNullTexture) {
        gfx::TextureDesc desc;
        desc.width = kAtlasSize;
        desc.height = kAtlasSize;
        desc.format = gfx::TextureFormat::Rgba8;
        g_atlas.texture = gfx::CreateTexture(desc);
    }
    return g_atlas.texture != gfx::kNullTexture;
}

// ---- Glyph bitmaps, and letters the font lacks built from ones it has ----

// Coverage, with its top-left corner (x0, y0) in pixels from the pen on the baseline (y down).
struct Bitmap {
    int                        x0 = 0;
    int                        y0 = 0;
    int                        w = 0;
    int                        h = 0;
    std::vector<unsigned char> alpha;
};

// The glyph index of ch when the font draws something for it. A blank placeholder (a font that maps
// a letter it does not have to an empty outline) counts as missing.
int DrawnGlyph(char32_t ch) {
    const int index = stbtt_FindGlyphIndex(&g_font.info, static_cast<int>(ch));
    return index != 0 && !stbtt_IsGlyphEmpty(&g_font.info, index) ? index : 0;
}

Bitmap Raster(int index, float scale_x, float scale_y) {
    Bitmap bitmap;
    int    x1, y1;
    stbtt_GetGlyphBitmapBox(&g_font.info, index, scale_x, scale_y, &bitmap.x0, &bitmap.y0, &x1, &y1);
    bitmap.w = x1 - bitmap.x0;
    bitmap.h = y1 - bitmap.y0;
    if (bitmap.w > 0 && bitmap.h > 0) {
        bitmap.alpha.assign(static_cast<size_t>(bitmap.w) * bitmap.h, 0);
        stbtt_MakeGlyphBitmap(&g_font.info, bitmap.alpha.data(), bitmap.w, bitmap.h, bitmap.w, scale_x, scale_y, index);
    }
    return bitmap;
}

struct Part {
    const Bitmap *bitmap;
    int           dx;
    int           dy;
};

// The parts laid over each other, each moved by its own offset; the strongest coverage wins.
Bitmap Combine(const std::vector<Part> &parts) {
    int x0 = INT32_MAX, y0 = INT32_MAX, x1 = INT32_MIN, y1 = INT32_MIN;
    for (const Part &part : parts) {
        if (part.bitmap->w <= 0 || part.bitmap->h <= 0) {
            continue;
        }
        x0 = std::min(x0, part.bitmap->x0 + part.dx);
        y0 = std::min(y0, part.bitmap->y0 + part.dy);
        x1 = std::max(x1, part.bitmap->x0 + part.dx + part.bitmap->w);
        y1 = std::max(y1, part.bitmap->y0 + part.dy + part.bitmap->h);
    }
    Bitmap out;
    if (x0 >= x1 || y0 >= y1) {
        return out;
    }
    out.x0 = x0;
    out.y0 = y0;
    out.w = x1 - x0;
    out.h = y1 - y0;
    out.alpha.assign(static_cast<size_t>(out.w) * out.h, 0);
    for (const Part &part : parts) {
        for (int row = 0; row < part.bitmap->h; row++) {
            for (int col = 0; col < part.bitmap->w; col++) {
                unsigned char &to = out.alpha[static_cast<size_t>(part.bitmap->y0 + part.dy + row - y0) * out.w +
                                              (part.bitmap->x0 + part.dx + col - x0)];
                to = std::max(to, part.bitmap->alpha[static_cast<size_t>(row) * part.bitmap->w + col]);
            }
        }
    }
    return out;
}

enum class Mark {
    Grave,
    Acute,
    Circumflex,
    Diaeresis,
    Tilde,
    Cedilla
};

// A character the PAL font draws as a letter with a mark on it, or under it.
struct Accented {
    char32_t ch;
    char32_t base;
    Mark     mark;
};

constexpr Accented kAccented[] = {
    {U'À', U'A', Mark::Grave     },
    {U'Á', U'A', Mark::Acute     },
    {U'Â', U'A', Mark::Circumflex},
    {U'Ä', U'A', Mark::Diaeresis },
    {U'È', U'E', Mark::Grave     },
    {U'É', U'E', Mark::Acute     },
    {U'Ê', U'E', Mark::Circumflex},
    {U'Ë', U'E', Mark::Diaeresis },
    {U'Ì', U'I', Mark::Grave     },
    {U'Í', U'I', Mark::Acute     },
    {U'Î', U'I', Mark::Circumflex},
    {U'Ï', U'I', Mark::Diaeresis },
    {U'Ñ', U'N', Mark::Tilde     },
    {U'Ò', U'O', Mark::Grave     },
    {U'Ó', U'O', Mark::Acute     },
    {U'Ô', U'O', Mark::Circumflex},
    {U'Ö', U'O', Mark::Diaeresis },
    {U'Ù', U'U', Mark::Grave     },
    {U'Ú', U'U', Mark::Acute     },
    {U'Û', U'U', Mark::Circumflex},
    {U'Ü', U'U', Mark::Diaeresis },
    {U'Ç', U'C', Mark::Cedilla   },
    {U'à', U'a', Mark::Grave     },
    {U'á', U'a', Mark::Acute     },
    {U'â', U'a', Mark::Circumflex},
    {U'ä', U'a', Mark::Diaeresis },
    {U'è', U'e', Mark::Grave     },
    {U'é', U'e', Mark::Acute     },
    {U'ê', U'e', Mark::Circumflex},
    {U'ë', U'e', Mark::Diaeresis },
    {U'ì', U'i', Mark::Grave     },
    {U'í', U'i', Mark::Acute     },
    {U'î', U'i', Mark::Circumflex},
    {U'ï', U'i', Mark::Diaeresis },
    {U'ñ', U'n', Mark::Tilde     },
    {U'ò', U'o', Mark::Grave     },
    {U'ó', U'o', Mark::Acute     },
    {U'ô', U'o', Mark::Circumflex},
    {U'ö', U'o', Mark::Diaeresis },
    {U'ù', U'u', Mark::Grave     },
    {U'ú', U'u', Mark::Acute     },
    {U'û', U'u', Mark::Circumflex},
    {U'ü', U'u', Mark::Diaeresis },
    {U'ç', U'c', Mark::Cedilla   },
};

// The plain character whose glyph stands for a mark, for the fonts that have no accent characters of
// their own: a tick for the acute, a pair of them for the diaeresis, a comma for the cedilla.
char32_t MarkGlyph(Mark mark) {
    switch (mark) {
        case Mark::Grave:
            return U'`';
        case Mark::Acute:
            return U'\'';
        case Mark::Circumflex:
            return U'^';
        case Mark::Diaeresis:
            return U'"';
        case Mark::Tilde:
            return U'~';
        case Mark::Cedilla:
            return U',';
    }
    return 0;
}

// Builds ch from the font's own glyphs when the font draws nothing for it; false for a character this
// does not know how to build (the caller then draws the game's bitmap). scale is pixels per font unit.
bool Compose(char32_t ch, float scale, int em_px, Bitmap &out) {
    const int gap = std::max(1, em_px / 25);

    for (const Accented &accented : kAccented) {
        if (accented.ch != ch) {
            continue;
        }
        const int base_index = DrawnGlyph(accented.base);
        const int mark_index = DrawnGlyph(MarkGlyph(accented.mark));
        if (base_index == 0 || mark_index == 0) {
            return false;
        }
        const bool   upper = accented.base >= U'A' && accented.base <= U'Z';
        const float  k = accented.mark == Mark::Cedilla ? 0.9f : upper ? 0.7f
                                                                       : 0.85f;
        const Bitmap base = Raster(base_index, scale, scale);
        const Bitmap mark = Raster(mark_index, scale * k, scale * k);
        if (base.w <= 0 || mark.w <= 0) {
            return false;
        }
        const int dx = (base.x0 + base.w / 2) - (mark.x0 + mark.w / 2);
        int       dy;
        if (accented.mark == Mark::Cedilla) {
            // The comma's top just inside the letter's bottom, its tail below the baseline.
            dy = (base.y0 + base.h - 2) - mark.y0;
        } else {
            // The mark's bottom a little above the letter's top (a lower-case i's dot included).
            dy = base.y0 - gap - (mark.y0 + mark.h);
        }
        out = Combine({
            {&base, 0,  0 },
            {&mark, dx, dy}
        });
        return out.w > 0;
    }

    if (ch == U'¡' || ch == U'¿') {
        const int index = DrawnGlyph(ch == U'¡' ? U'!' : U'?');
        if (index == 0) {
            return false;
        }
        out = Raster(index, scale, scale);
        // Turned over inside its own box, so it keeps the height and the depth the original had.
        for (int row = 0; row < out.h / 2; row++) {
            std::swap_ranges(out.alpha.begin() + static_cast<size_t>(row) * out.w,
                             out.alpha.begin() + static_cast<size_t>(row + 1) * out.w,
                             out.alpha.begin() + static_cast<size_t>(out.h - 1 - row) * out.w);
        }
        return out.w > 0;
    }

    if (ch == U'œ' || ch == U'Œ') {
        const int first = DrawnGlyph(ch == U'œ' ? U'o' : U'O');
        const int second = DrawnGlyph(ch == U'œ' ? U'e' : U'E');
        if (first == 0 || second == 0) {
            return false;
        }
        // Two letters, each squeezed to 65% of its width, the second tucked against the first.
        const Bitmap left = Raster(first, scale * 0.65f, scale);
        const Bitmap right = Raster(second, scale * 0.65f, scale);
        out = Combine({
            {&left,  0,                               0},
            {&right, left.x0 + left.w - right.x0 - 1, 0}
        });
        return out.w > 0;
    }
    return false;
}

} // namespace

bool Load(const std::filesystem::path &file) {
    std::ifstream stream(file, std::ios::binary);
    if (!stream) {
        std::fprintf(stderr, "font: cannot read %s\n", PathsDisplay(file).c_str());
        return false;
    }
    Font font;
    font.data.assign(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
    const unsigned char *bytes = font.data.data();
    int                  offset = stbtt_GetFontOffsetForIndex(bytes, 0);
    if (offset < 0 || !stbtt_InitFont(&font.info, bytes, offset)) {
        std::fprintf(stderr, "font: %s is not a TrueType font\n", PathsDisplay(file).c_str());
        return false;
    }
    font.loaded = true;
    // The font's data moves with the vector it is in, so the info is built on the final place.
    g_font = std::move(font);
    stbtt_InitFont(&g_font.info, g_font.data.data(), stbtt_GetFontOffsetForIndex(g_font.data.data(), 0));
    ResetAtlas();
    return true;
}

bool Loaded() {
    return g_font.loaded;
}

bool GetGlyph(char32_t ch, int em_px, Glyph &out) {
    if (!g_font.loaded || em_px < 4 || em_px > 512) {
        return false;
    }
    const auto key = std::make_pair(ch, em_px);
    if (auto found = g_atlas.glyphs.find(key); found != g_atlas.glyphs.end()) {
        out = found->second;
        return true;
    }
    if (g_atlas.missing.count(key) != 0) {
        return false;
    }
    if (!EnsureAtlas()) {
        g_atlas.missing[key] = true;
        return false;
    }

    // An em of em_px pixels: stb scales by the font's units per em.
    const float scale = stbtt_ScaleForMappingEmToPixels(&g_font.info, static_cast<float>(em_px));
    Bitmap      bitmap;
    if (const int index = DrawnGlyph(ch); index != 0) {
        bitmap = Raster(index, scale, scale);
    } else if (!Compose(ch, scale, em_px, bitmap)) {
        g_atlas.missing[key] = true;
        return false;
    }
    const int width = bitmap.w;
    const int height = bitmap.h;

    if (width > 0 && height > 0) {
        if (g_atlas.x + width + 2 * kPad > kAtlasSize) {
            g_atlas.x = 0;
            g_atlas.y += g_atlas.row_height;
            g_atlas.row_height = 0;
        }
        if (g_atlas.y + height + 2 * kPad > kAtlasSize) {
            // Full: start again. Glyphs queued this frame keep their old coordinates, which now
            // show whatever lands there; one frame of it, and the next fills the atlas afresh.
            std::fprintf(stderr, "font: atlas full, starting again\n");
            gfx::TextureHandle keep = g_atlas.texture;
            g_atlas = {};
            g_atlas.texture = keep;
            std::vector<unsigned char> blank(static_cast<size_t>(kAtlasSize) * kAtlasSize * 4, 0);
            gfx::UpdateTexture(keep, 0, 0, 0, kAtlasSize, kAtlasSize, blank.data());
        }
    }

    Glyph glyph = {};
    glyph.x_offset = static_cast<float>(bitmap.x0);
    glyph.y_offset = static_cast<float>(bitmap.y0);
    glyph.width = static_cast<float>(width);
    glyph.height = static_cast<float>(height);

    if (width > 0 && height > 0) {
        // White with the coverage as alpha, so the draw's vertex colour tints it.
        const int                  w = width + 2 * kPad;
        const int                  h = height + 2 * kPad;
        std::vector<unsigned char> pixels(static_cast<size_t>(w) * h * 4, 0);
        for (int row = 0; row < height; row++) {
            for (int col = 0; col < width; col++) {
                unsigned char *texel = &pixels[(static_cast<size_t>(row + kPad) * w + col + kPad) * 4];
                texel[0] = texel[1] = texel[2] = 255;
                texel[3] = bitmap.alpha[static_cast<size_t>(row) * width + col];
            }
        }
        gfx::UpdateTexture(g_atlas.texture, 0, static_cast<uint32_t>(g_atlas.x), static_cast<uint32_t>(g_atlas.y),
                           static_cast<uint32_t>(w), static_cast<uint32_t>(h), pixels.data());
        glyph.u0 = static_cast<float>(g_atlas.x + kPad);
        glyph.v0 = static_cast<float>(g_atlas.y + kPad);
        glyph.u1 = static_cast<float>(g_atlas.x + kPad + width);
        glyph.v1 = static_cast<float>(g_atlas.y + kPad + height);
        g_atlas.x += w;
        g_atlas.row_height = std::max(g_atlas.row_height, h);
    }

    g_atlas.glyphs[key] = glyph;
    out = glyph;
    return true;
}

bool RenderGlyph(char32_t ch, int em_px, std::vector<unsigned char> &alpha, int &w, int &h, int &x0, int &y0) {
    if (!g_font.loaded || em_px < 4 || em_px > 512) {
        return false;
    }
    const float scale = stbtt_ScaleForMappingEmToPixels(&g_font.info, static_cast<float>(em_px));
    Bitmap      bitmap;
    if (const int index = DrawnGlyph(ch); index != 0) {
        bitmap = Raster(index, scale, scale);
    } else if (!Compose(ch, scale, em_px, bitmap)) {
        return false;
    }
    alpha = std::move(bitmap.alpha);
    w = bitmap.w;
    h = bitmap.h;
    x0 = bitmap.x0;
    y0 = bitmap.y0;
    return true;
}

gfx::TextureBinding Binding() {
    gfx::TextureBinding binding;
    binding.texture = g_atlas.texture;
    binding.filter = gfx::Filter::Linear;
    return binding;
}

Metrics GetMetrics() {
    Metrics metrics = {0.8f, 0.2f, 0.6f};
    if (!g_font.loaded) {
        return metrics;
    }
    int ascent, descent, gap;
    stbtt_GetFontVMetrics(&g_font.info, &ascent, &descent, &gap);
    const float em = 1.0f / stbtt_ScaleForMappingEmToPixels(&g_font.info, 1.0f);
    metrics.ascent = static_cast<float>(ascent) / em;
    metrics.descent = static_cast<float>(-descent) / em;
    int advance, bearing;
    stbtt_GetCodepointHMetrics(&g_font.info, '0', &advance, &bearing);
    metrics.advance = static_cast<float>(advance) / em;
    return metrics;
}

} // namespace ttffont
