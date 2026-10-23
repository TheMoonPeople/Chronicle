#include "captionfont.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <map>
#include <set>
#include <string>
#include <vector>

#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "stb_truetype.h"

#include "platform/paths.hpp"

namespace capfont {

namespace {

constexpr int kAtlasSize = 2048;
// Transparent atlas padding prevents neighboring glyphs bleeding under linear sampling.
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
    std::map<std::string, Glyph>              painted;         // GetPaintedGlyph's and GetPaintedWord's
    std::set<std::string>                     painted_missing; // ones that could not be made
};

Font     g_fonts[4];
int      g_variant = 0;
Atlas    g_atlas;
unsigned g_generation = 0;

void ResetAtlas() {
    if (g_atlas.texture != gfx::kNullTexture) {
        gfx::DestroyTexture(g_atlas.texture);
    }
    g_atlas = {};
    g_generation++;
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

struct Bitmap {
    int                        x0 = 0;
    int                        y0 = 0;
    int                        w = 0;
    int                        h = 0;
    std::vector<unsigned char> alpha;
};

// Some font characters map to empty placeholders; treat those as missing.
int DrawnGlyph(char32_t ch) {
    const int index = stbtt_FindGlyphIndex(&g_fonts[g_variant].info, static_cast<int>(ch));
    return index != 0 && !stbtt_IsGlyphEmpty(&g_fonts[g_variant].info, index) ? index : 0;
}

Bitmap Raster(const stbtt_fontinfo &info, int index, float scale_x, float scale_y) {
    Bitmap bitmap;
    int    x1, y1;
    stbtt_GetGlyphBitmapBox(&info, index, scale_x, scale_y, &bitmap.x0, &bitmap.y0, &x1, &y1);
    bitmap.w = x1 - bitmap.x0;
    bitmap.h = y1 - bitmap.y0;
    if (bitmap.w > 0 && bitmap.h > 0) {
        bitmap.alpha.assign(static_cast<size_t>(bitmap.w) * bitmap.h, 0);
        stbtt_MakeGlyphBitmap(&info, bitmap.alpha.data(), bitmap.w, bitmap.h, bitmap.w, scale_x, scale_y, index);
    }
    return bitmap;
}

Bitmap Raster(int index, float scale_x, float scale_y) {
    return Raster(g_fonts[g_variant].info, index, scale_x, scale_y);
}

struct Part {
    const Bitmap *bitmap;
    int           dx;
    int           dy;
};

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

// Missing accent marks are composed from the font's punctuation glyphs.
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
            dy = (base.y0 + base.h - 2) - mark.y0;
        } else {
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

// Squared Euclidean distance transform (Felzenszwalb and Huttenlocher).
void DistanceLine(std::vector<float> &f, std::vector<int> &v, std::vector<float> &z, std::vector<float> &out) {
    const int n = static_cast<int>(f.size());
    int       k = 0;
    v[0] = 0;
    z[0] = -1e30f;
    z[1] = 1e30f;
    for (int q = 1; q < n; q++) {
        float s;
        for (;;) {
            const int p = v[static_cast<size_t>(k)];
            s = ((f[static_cast<size_t>(q)] + static_cast<float>(q * q)) - (f[static_cast<size_t>(p)] + static_cast<float>(p * p))) /
                (2.0f * static_cast<float>(q - p));
            if (s <= z[static_cast<size_t>(k)] && k > 0) {
                k--;
            } else {
                break;
            }
        }
        k++;
        v[static_cast<size_t>(k)] = q;
        z[static_cast<size_t>(k)] = s;
        z[static_cast<size_t>(k) + 1] = 1e30f;
    }
    k = 0;
    for (int q = 0; q < n; q++) {
        while (z[static_cast<size_t>(k) + 1] < static_cast<float>(q)) {
            k++;
        }
        const int p = v[static_cast<size_t>(k)];
        out[static_cast<size_t>(q)] = static_cast<float>((q - p) * (q - p)) + f[static_cast<size_t>(p)];
    }
}

Bitmap Dilate(const Bitmap &in, int radius) {
    if (radius <= 0 || in.w <= 0 || in.h <= 0) {
        return in;
    }
    Bitmap    out;
    const int pad = radius + 1;
    out.x0 = in.x0 - pad;
    out.y0 = in.y0 - pad;
    out.w = in.w + 2 * pad;
    out.h = in.h + 2 * pad;
    const int          width = out.w, height = out.h;
    const float        far_distance = 1e12f;
    std::vector<float> grid(static_cast<size_t>(width) * height, far_distance);
    for (int row = 0; row < in.h; row++) {
        for (int col = 0; col < in.w; col++) {
            if (in.alpha[static_cast<size_t>(row) * in.w + col] >= 128) {
                grid[static_cast<size_t>(row + pad) * width + col + pad] = 0.0f;
            }
        }
    }
    const int          longest = std::max(width, height);
    std::vector<float> line(static_cast<size_t>(longest)), result(static_cast<size_t>(longest)), z(static_cast<size_t>(longest) + 1);
    std::vector<int>   v(static_cast<size_t>(longest));
    for (int col = 0; col < width; col++) {
        line.resize(static_cast<size_t>(height));
        for (int row = 0; row < height; row++) {
            line[static_cast<size_t>(row)] = grid[static_cast<size_t>(row) * width + col];
        }
        DistanceLine(line, v, z, result);
        for (int row = 0; row < height; row++) {
            grid[static_cast<size_t>(row) * width + col] = result[static_cast<size_t>(row)];
        }
    }
    for (int row = 0; row < height; row++) {
        line.resize(static_cast<size_t>(width));
        for (int col = 0; col < width; col++) {
            line[static_cast<size_t>(col)] = grid[static_cast<size_t>(row) * width + col];
        }
        DistanceLine(line, v, z, result);
        for (int col = 0; col < width; col++) {
            grid[static_cast<size_t>(row) * width + col] = result[static_cast<size_t>(col)];
        }
    }
    out.alpha.assign(static_cast<size_t>(width) * height, 0);
    for (int row = 0; row < height; row++) {
        for (int col = 0; col < width; col++) {
            const float d = std::sqrt(grid[static_cast<size_t>(row) * width + col]);
            float       cover = std::clamp(static_cast<float>(radius) + 0.5f - d, 0.0f, 1.0f) * 255.0f;
            const int   src_row = row - pad, src_col = col - pad;
            if (src_row >= 0 && src_row < in.h && src_col >= 0 && src_col < in.w) {
                cover = std::max(cover, static_cast<float>(in.alpha[static_cast<size_t>(src_row) * in.w + src_col]));
            }
            out.alpha[static_cast<size_t>(row) * width + col] = static_cast<unsigned char>(cover + 0.5f);
        }
    }
    return out;
}

// A bitmap `factor` times finer brought back to pixels: each pixel the average of the finer ones it covers, the
// finer bitmap's origin rounded out to whole pixels first.
Bitmap Downsample(const Bitmap &in, int factor) {
    Bitmap out;
    auto   floor_div = [](int v, int d) { return v >= 0 ? v / d : -((-v + d - 1) / d); };
    out.x0 = floor_div(in.x0, factor);
    out.y0 = floor_div(in.y0, factor);
    const int shift_x = in.x0 - out.x0 * factor, shift_y = in.y0 - out.y0 * factor;
    out.w = (in.w + shift_x + factor - 1) / factor;
    out.h = (in.h + shift_y + factor - 1) / factor;
    out.alpha.assign(static_cast<size_t>(std::max(out.w, 0)) * std::max(out.h, 0), 0);
    for (int row = 0; row < out.h; row++) {
        for (int col = 0; col < out.w; col++) {
            int sum = 0;
            for (int dy = 0; dy < factor; dy++) {
                for (int dx = 0; dx < factor; dx++) {
                    const int r = row * factor + dy - shift_y, c = col * factor + dx - shift_x;
                    if (r >= 0 && r < in.h && c >= 0 && c < in.w) {
                        sum += in.alpha[static_cast<size_t>(r) * in.w + c];
                    }
                }
            }
            out.alpha[static_cast<size_t>(row) * out.w + col] = static_cast<unsigned char>(sum / (factor * factor));
        }
    }
    return out;
}

float AdvanceOf(char32_t ch, float scale) {
    char32_t drawn = ch;
    if (DrawnGlyph(ch) == 0) {
        for (const Accented &accented : kAccented) {
            if (accented.ch == ch) {
                drawn = accented.base;
            }
        }
        if (ch == U'\u00A1') {
            drawn = U'!';
        } else if (ch == U'\u00BF') {
            drawn = U'?';
        } else if (ch == U'\u0153' || ch == U'\u0152') {
            drawn = ch == U'\u0153' ? U'o' : U'O';
        }
    }
    int index = DrawnGlyph(drawn);
    if (index == 0) {
        index = stbtt_FindGlyphIndex(&g_fonts[g_variant].info, static_cast<int>(drawn));
    }
    int advance = 0, bearing = 0;
    stbtt_GetGlyphHMetrics(&g_fonts[g_variant].info, index, &advance, &bearing);
    return static_cast<float>(advance) * scale;
}

bool InitFontSlotFromBytes(int slot, std::vector<unsigned char> &&bytes) {
    if (slot < 0 || slot >= 4 || bytes.empty()) {
        return false;
    }
    Font font;
    font.data = std::move(bytes);
    const int offset = stbtt_GetFontOffsetForIndex(font.data.data(), 0);
    if (offset < 0 || !stbtt_InitFont(&font.info, font.data.data(), offset)) {
        return false;
    }
    font.loaded = true;
    g_fonts[slot] = std::move(font);
    stbtt_InitFont(&g_fonts[slot].info, g_fonts[slot].data.data(),
                   stbtt_GetFontOffsetForIndex(g_fonts[slot].data.data(), 0));
    return true;
}

bool LoadFontSlot(int slot, const std::filesystem::path &file) {
    if (slot < 0 || slot >= 4) {
        return false;
    }
    std::ifstream stream(file, std::ios::binary);
    if (!stream) {
        return false;
    }
    std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(stream)),
                                     std::istreambuf_iterator<char>());
    if (!InitFontSlotFromBytes(slot, std::move(bytes))) {
        std::fprintf(stderr, "font: %s is not a TrueType font\n", PathsDisplay(file).c_str());
        return false;
    }
    return true;
}

} // namespace

bool Load(const std::filesystem::path &file) {
    g_variant = 0;
    std::ifstream stream(file, std::ios::binary);
    if (!stream) {
        std::fprintf(stderr, "font: cannot read %s\n", PathsDisplay(file).c_str());
        return false;
    }
    std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(stream)),
                                     std::istreambuf_iterator<char>());
    if (!InitFontSlotFromBytes(0, std::move(bytes))) {
        std::fprintf(stderr, "font: %s is not a TrueType font\n", PathsDisplay(file).c_str());
        return false;
    }
    ResetAtlas();
    return true;
}

bool Loaded() {
    return g_fonts[0].loaded;
}

bool LoadVariant(const std::filesystem::path &file) {
    return LoadFontSlot(1, file);
}

bool VariantLoaded() {
    return g_fonts[1].loaded;
}

bool LoadStatusFont(const std::filesystem::path &file) {
    return LoadFontSlot(2, file);
}

bool LoadStatusFontMemory(const unsigned char *data, std::size_t size) {
    if (data == nullptr || size == 0) {
        return false;
    }
    std::vector<unsigned char> bytes(data, data + size);
    return InitFontSlotFromBytes(2, std::move(bytes));
}

bool StatusFontLoaded() {
    return g_fonts[2].loaded;
}

bool LoadTitleFontMemory(const unsigned char *data, std::size_t size) {
    if (data == nullptr || size == 0) {
        return false;
    }
    std::vector<unsigned char> bytes(data, data + size);
    return InitFontSlotFromBytes(3, std::move(bytes));
}

bool TitleFontLoaded() {
    return g_fonts[3].loaded;
}

float KernPixels(char32_t left, char32_t right, int em_px) {
    const Font &font = g_fonts[g_variant];
    if (!font.loaded) {
        return 0.0f;
    }
    const int kern = stbtt_GetCodepointKernAdvance(&font.info, static_cast<int>(left), static_cast<int>(right));
    return static_cast<float>(kern) * stbtt_ScaleForMappingEmToPixels(&font.info, static_cast<float>(em_px));
}

void UseFontSlot(int slot) {
    if (slot == 3 && g_fonts[3].loaded) {
        g_variant = 3;
    } else if (slot == 2 && g_fonts[2].loaded) {
        g_variant = 2;
    } else if (slot == 1 && g_fonts[1].loaded) {
        g_variant = 1;
    } else {
        g_variant = 0;
    }
}

void UseVariant(bool variant) {
    UseFontSlot(variant ? 1 : 0);
}

bool GetGlyph(char32_t ch, int em_px, Glyph &out) {
    return GetOutlinedGlyph(ch, em_px, 0, out);
}

namespace {

// A glyph drawn `stretch` sixty-fourths as wide as the font has it, then outlined (the outline the same all round); with
// `smooth`, drawn and outlined four times finer and brought back, so the outline's edge is anti-aliased as finely as the
// letter's own instead of stepping a pixel at a time.
bool GetShapedGlyph(char32_t ch, int em_px, int outline_px, int stretch, bool smooth, Glyph &out) {
    if (!g_fonts[g_variant].loaded || em_px < 4 || em_px > 512 || outline_px < 0 || outline_px > 63 || stretch < 16 ||
        stretch > 255) {
        return false;
    }
    const auto key = std::make_pair(ch,
                                    em_px + outline_px * 1024 + g_variant * (1 << 20) + (stretch == 64 ? 0 : stretch << 22) +
                                        (smooth ? 1 << 30 : 0));
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

    const float scale = stbtt_ScaleForMappingEmToPixels(&g_fonts[g_variant].info, static_cast<float>(em_px));
    Bitmap      bitmap;
    const int   fine = smooth ? 4 : 1;
    if (const int index = DrawnGlyph(ch); index != 0) {
        bitmap = Raster(index, scale * static_cast<float>(stretch) / 64.0f * static_cast<float>(fine), scale * static_cast<float>(fine));
    } else if (stretch != 64 || smooth || !Compose(ch, scale, em_px, bitmap)) {
        g_atlas.missing[key] = true;
        return false;
    }
    const float advance = AdvanceOf(ch, scale) * static_cast<float>(stretch) / 64.0f;
    if (outline_px > 0) {
        // With `smooth` the outline is given in the finer pixels already (a quarter of a pixel each).
        bitmap = Dilate(bitmap, smooth ? outline_px : outline_px * fine);
    }
    if (fine > 1) {
        bitmap = Downsample(bitmap, fine);
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
            // Resetting a full atlas invalidates glyph coordinates already queued for this frame.
            std::fprintf(stderr, "font: atlas full, starting again\n");
            gfx::TextureHandle keep = g_atlas.texture;
            g_atlas = {};
            g_generation++;
            g_atlas.texture = keep;
            std::vector<unsigned char> blank(static_cast<size_t>(kAtlasSize) * kAtlasSize * 4, 0);
            gfx::UpdateTextureNow(keep, 0, 0, 0, kAtlasSize, kAtlasSize, blank.data());
        }
    }

    Glyph glyph = {};
    glyph.x_offset = static_cast<float>(bitmap.x0);
    glyph.y_offset = static_cast<float>(bitmap.y0);
    glyph.width = static_cast<float>(width);
    glyph.height = static_cast<float>(height);
    glyph.advance = advance;

    if (width > 0 && height > 0) {
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
        gfx::UpdateTextureNow(g_atlas.texture, 0, static_cast<uint32_t>(g_atlas.x), static_cast<uint32_t>(g_atlas.y),
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

} // namespace

bool GetOutlinedGlyph(char32_t ch, int em_px, int outline_px, Glyph &out) {
    return GetShapedGlyph(ch, em_px, outline_px, 64, false, out);
}

bool GetStretchedGlyph(char32_t ch, int em_px, float outline_px, float stretch, bool smooth, Glyph &out) {
    const int outline = static_cast<int>(std::lround(outline_px * (smooth ? 4.0f : 1.0f)));
    return GetShapedGlyph(ch, em_px, outline, std::clamp(static_cast<int>(std::lround(stretch * 64.0f)), 16, 255), smooth, out);
}

bool AddImage(const std::vector<unsigned char> &rgba, int w, int h, Glyph &out) {
    if (w <= 0 || h <= 0 || w > 1024 || h > 1024 || rgba.size() < static_cast<size_t>(w) * h * 4 || !EnsureAtlas()) {
        return false;
    }
    if (g_atlas.x + w + 2 * kPad > kAtlasSize) {
        g_atlas.x = 0;
        g_atlas.y += g_atlas.row_height;
        g_atlas.row_height = 0;
    }
    if (g_atlas.y + h + 2 * kPad > kAtlasSize) {
        std::fprintf(stderr, "font: atlas full, starting again\n");
        gfx::TextureHandle keep = g_atlas.texture;
        g_atlas = {};
        g_generation++;
        g_atlas.texture = keep;
        std::vector<unsigned char> blank(static_cast<size_t>(kAtlasSize) * kAtlasSize * 4, 0);
        gfx::UpdateTextureNow(keep, 0, 0, 0, kAtlasSize, kAtlasSize, blank.data());
    }
    const int                  pw = w + 2 * kPad;
    const int                  ph = h + 2 * kPad;
    std::vector<unsigned char> pixels(static_cast<size_t>(pw) * ph * 4, 0);
    for (int row = 0; row < h; row++) {
        std::copy_n(&rgba[static_cast<size_t>(row) * w * 4], static_cast<size_t>(w) * 4,
                    &pixels[(static_cast<size_t>(row + kPad) * pw + kPad) * 4]);
    }
    gfx::UpdateTextureNow(g_atlas.texture, 0, static_cast<uint32_t>(g_atlas.x), static_cast<uint32_t>(g_atlas.y),
                          static_cast<uint32_t>(pw), static_cast<uint32_t>(ph), pixels.data());
    out = {};
    out.u0 = static_cast<float>(g_atlas.x + kPad);
    out.v0 = static_cast<float>(g_atlas.y + kPad);
    out.u1 = static_cast<float>(g_atlas.x + kPad + w);
    out.v1 = static_cast<float>(g_atlas.y + kPad + h);
    out.width = static_cast<float>(w);
    out.height = static_cast<float>(h);
    g_atlas.x += pw;
    g_atlas.row_height = std::max(g_atlas.row_height, ph);
    return true;
}

namespace {

struct Point {
    float x, y;
};

// A glyph's contours in font units, its curves in short straight pieces.
std::vector<std::vector<Point>> Contours(const stbtt_fontinfo &info, int index) {
    stbtt_vertex                   *vertices = nullptr;
    const int                       count = stbtt_GetGlyphShape(&info, index, &vertices);
    std::vector<std::vector<Point>> out;
    Point                           at = {};
    for (int i = 0; i < count; i++) {
        const stbtt_vertex &v = vertices[i];
        const Point         to = {static_cast<float>(v.x), static_cast<float>(v.y)};
        if (v.type == STBTT_vmove) {
            out.push_back({to});
        } else if (!out.empty() && v.type == STBTT_vline) {
            out.back().push_back(to);
        } else if (!out.empty() && v.type == STBTT_vcurve) {
            const Point c = {static_cast<float>(v.cx), static_cast<float>(v.cy)};
            for (int step = 1; step <= 16; step++) {
                const float t = static_cast<float>(step) / 16, s = 1 - t;
                out.back().push_back({s * s * at.x + 2 * s * t * c.x + t * t * to.x, s * s * at.y + 2 * s * t * c.y + t * t * to.y});
            }
        } else if (!out.empty() && v.type == STBTT_vcubic) {
            const Point c0 = {static_cast<float>(v.cx), static_cast<float>(v.cy)};
            const Point c1 = {static_cast<float>(v.cx1), static_cast<float>(v.cy1)};
            for (int step = 1; step <= 16; step++) {
                const float t = static_cast<float>(step) / 16, s = 1 - t;
                out.back().push_back({s * s * s * at.x + 3 * s * s * t * c0.x + 3 * s * t * t * c1.x + t * t * t * to.x,
                                      s * s * s * at.y + 3 * s * s * t * c0.y + 3 * s * t * t * c1.y + t * t * t * to.y});
            }
        }
        at = to;
    }
    stbtt_FreeShape(&info, vertices);
    std::erase_if(out, [](const std::vector<Point> &c) { return c.size() < 3; });
    return out;
}

float Area(const std::vector<Point> &c) {
    float sum = 0;
    for (size_t i = 0, j = c.size() - 1; i < c.size(); j = i++) {
        sum += c[j].x * c[i].y - c[i].x * c[j].y;
    }
    return sum / 2;
}

bool Inside(const std::vector<Point> &c, Point p) {
    bool in = false;
    for (size_t i = 0, j = c.size() - 1; i < c.size(); j = i++) {
        if ((c[i].y > p.y) != (c[j].y > p.y) && p.x < (c[j].x - c[i].x) * (p.y - c[i].y) / (c[j].y - c[i].y) + c[i].x) {
            in = !in;
        }
    }
    return in;
}

// One piece of a glyph: an outer contour with the holes inside it. Pieces run left to right, a column top first.
std::vector<std::vector<Point>> Piece(std::vector<std::vector<Point>> contours, int part) {
    if (part < 0 || contours.empty()) {
        return contours;
    }
    float largest = 0;
    for (const auto &c : contours) {
        largest = std::abs(Area(c)) > std::abs(largest) ? Area(c) : largest;
    }
    std::vector<std::vector<std::vector<Point>>> pieces;
    std::vector<const std::vector<Point> *>      holes;
    for (const auto &c : contours) {
        if ((Area(c) > 0) == (largest > 0)) {
            pieces.push_back({c});
        } else {
            holes.push_back(&c);
        }
    }
    for (const auto *hole : holes) {
        for (auto &piece : pieces) {
            if (Inside(piece[0], hole->front())) {
                piece.push_back(*hole);
                break;
            }
        }
    }
    auto left_top = [](const std::vector<std::vector<Point>> &piece) {
        float x = 1e9f, y = -1e9f;
        for (const Point &p : piece[0]) {
            x = std::min(x, p.x);
            y = std::max(y, p.y);
        }
        return std::make_pair(x, -y);
    };
    std::sort(pieces.begin(), pieces.end(), [&](const auto &a, const auto &b) { return left_top(a) < left_top(b); });
    return part < static_cast<int>(pieces.size()) ? pieces[static_cast<size_t>(part)] : std::vector<std::vector<Point>>{};
}

} // namespace

namespace {

// Every field of a Paint, for the painted glyphs' cache keys.
std::string PaintKey(const Paint &paint) {
    std::string key;
    for (float v : {paint.depth_x, paint.depth_y, paint.stroke, static_cast<float>(paint.strokes), paint.outline, paint.shadow_x,
                    paint.shadow_y, paint.shadow_grow, paint.shadow_soft, paint.shadow_alpha, paint.band_alpha, paint.band_y,
                    paint.band_core, paint.band_fade, paint.band_left, paint.band_right, static_cast<float>(paint.slot),
                    paint.shadow_only ? 1.0f : 0.0f}) {
        key += std::to_string(std::lround(v * 64)) + ",";
    }
    for (const unsigned char *colour : {paint.fill, paint.body, paint.ring, paint.shade}) {
        key += std::to_string(colour[0] << 16 | colour[1] << 8 | colour[2]) + ",";
    }
    return key;
}

// Contours in a box's pixels (y down) painted into the atlas; the glyph's offsets are from the box's top left.
bool PaintContours(const std::vector<std::vector<Point>> &contours, float box_w, float box_h, const Paint &paint, Glyph &out) {
    float span_left = 1e9f, span_right = -1e9f;
    for (const auto &c : contours) {
        for (const Point &p : c) {
            span_left = std::min(span_left, p.x);
            span_right = std::max(span_right, p.x);
        }
    }
    if (paint.band_right > paint.band_left) {
        span_left = paint.band_left;
        span_right = paint.band_right;
    }
    // The bitmap: the box, the depth, the outline and the shadow, and a pixel spare all round.
    const float shadow_reach = std::max(paint.shadow_alpha > 0 ? paint.shadow_grow + paint.shadow_soft : 0.0f,
                                        paint.band_alpha > 0 ? paint.band_fade : 0.0f);
    const float reach = std::max(paint.outline, shadow_reach) + 1.5f;
    const int   left = static_cast<int>(std::floor(-reach + std::min(0.0f, paint.shadow_x)));
    const int   top = static_cast<int>(std::floor(-reach + std::min(0.0f, paint.shadow_y)));
    const int   w = static_cast<int>(std::ceil(box_w + std::max({paint.depth_x, paint.shadow_x, 0.0f}) + reach)) - left;
    const int   h = static_cast<int>(std::ceil(box_h + std::max({paint.depth_y, paint.shadow_y, 0.0f}) + reach)) - top;

    // The signed distance from each pixel's centre to the outline (negative inside), as far as any layer looks.
    const float band = std::max({paint.outline + std::hypot(paint.depth_x, paint.depth_y), paint.stroke * static_cast<float>(paint.strokes),
                                 shadow_reach + std::hypot(paint.shadow_x, paint.shadow_y)}) +
                       2.0f;

    struct Segment {
        Point a, b;
    };

    std::vector<Segment> segments;
    for (const auto &c : contours) {
        for (size_t i = 0, j = c.size() - 1; i < c.size(); j = i++) {
            segments.push_back({c[j], c[i]});
        }
    }
    std::vector<float> distance(static_cast<size_t>(w) * h, band);
    std::vector<int>   winding(static_cast<size_t>(w) * h, 0);
    for (const Segment &s : segments) {
        // Every pixel within the band of the segment learns how near it is.
        const int   gx0 = std::max(0, static_cast<int>(std::floor(std::min(s.a.x, s.b.x) - band)) - left);
        const int   gx1 = std::min(w - 1, static_cast<int>(std::ceil(std::max(s.a.x, s.b.x) + band)) - left);
        const int   gy0 = std::max(0, static_cast<int>(std::floor(std::min(s.a.y, s.b.y) - band)) - top);
        const int   gy1 = std::min(h - 1, static_cast<int>(std::ceil(std::max(s.a.y, s.b.y) + band)) - top);
        const float dx = s.b.x - s.a.x, dy = s.b.y - s.a.y, length = dx * dx + dy * dy;
        for (int gy = gy0; gy <= gy1; gy++) {
            for (int gx = gx0; gx <= gx1; gx++) {
                const float px = static_cast<float>(gx + left) + 0.5f, py = static_cast<float>(gy + top) + 0.5f;
                const float t = length > 0 ? std::clamp(((px - s.a.x) * dx + (py - s.a.y) * dy) / length, 0.0f, 1.0f) : 0.0f;
                float      &d = distance[static_cast<size_t>(gy) * w + gx];
                d = std::min(d, std::hypot(px - (s.a.x + t * dx), py - (s.a.y + t * dy)));
            }
        }
        // Which pixels it winds round: a crossing of each row's centre line counts for the pixels right of it.
        if (s.a.y == s.b.y) {
            continue;
        }
        const int dir = s.b.y > s.a.y ? 1 : -1;
        for (int gy = 0; gy < h; gy++) {
            const float py = static_cast<float>(gy + top) + 0.5f;
            if ((py >= s.a.y) == (py >= s.b.y)) {
                continue;
            }
            const float cross = s.a.x + (py - s.a.y) / dy * dx;
            const int   from = std::max(0, static_cast<int>(std::ceil(cross - 0.5f)) - left);
            for (int gx = from; gx < w; gx++) {
                winding[static_cast<size_t>(gy) * w + gx] += dir;
            }
        }
    }
    for (size_t i = 0; i < distance.size(); i++) {
        if (winding[i] != 0) {
            distance[i] = -distance[i];
        }
    }
    auto sample = [&](const std::vector<float> &field, float far, float px, float py) {
        // Bilinear between pixel centres; beyond the bitmap, outside.
        const float fx = px - static_cast<float>(left) - 0.5f, fy = py - static_cast<float>(top) - 0.5f;
        const int   ix = static_cast<int>(std::floor(fx)), iy = static_cast<int>(std::floor(fy));
        const float tx = fx - static_cast<float>(ix), ty = fy - static_cast<float>(iy);
        auto        get = [&](int gx, int gy) {
            return gx < 0 || gy < 0 || gx >= w || gy >= h ? far : field[static_cast<size_t>(gy) * w + gx];
        };
        return (get(ix, iy) * (1 - tx) + get(ix + 1, iy) * tx) * (1 - ty) + (get(ix, iy + 1) * (1 - tx) + get(ix + 1, iy + 1) * tx) * ty;
    };
    auto at = [&](float px, float py) { return sample(distance, band, px, py); };
    auto cover = [](float signed_distance) { return std::clamp(0.5f - signed_distance, 0.0f, 1.0f); };

    const int                  steps = std::max(2, static_cast<int>(std::ceil(std::hypot(paint.depth_x, paint.depth_y) * 4)));
    std::vector<unsigned char> rgba(static_cast<size_t>(w) * h * 4);
    for (int gy = 0; gy < h; gy++) {
        for (int gx = 0; gx < w; gx++) {
            const float px = static_cast<float>(gx + left) + 0.5f, py = static_cast<float>(gy + top) + 0.5f;
            const float d = distance[static_cast<size_t>(gy) * w + gx];
            // The letter swept along its depth: the nearest of its copies back along the sweep.
            float wall = d;
            for (int step = 1; step <= steps; step++) {
                const float t = static_cast<float>(step) / static_cast<float>(steps);
                wall = std::min(wall, at(px - t * paint.depth_x, py - t * paint.depth_y));
            }
            float rgb[3] = {paint.ring[0], paint.ring[1], paint.ring[2]};
            auto  over = [&](const unsigned char (&colour)[3], float k, float amount) {
                for (int c = 0; c < 3; c++) {
                    rgb[c] += (static_cast<float>(colour[c]) * (1 - k) + static_cast<float>(paint.fill[c]) * k - rgb[c]) * amount;
                }
            };
            over(paint.body, 0, cover(wall));
            for (int i = 0; i <= paint.strokes; i++) {
                over(paint.body, paint.strokes > 0 ? static_cast<float>(i) / static_cast<float>(paint.strokes) : 1.0f,
                     cover(d + static_cast<float>(i) * paint.stroke));
            }
            unsigned char *texel = &rgba[(static_cast<size_t>(gy) * w + gx) * 4];
            float          alpha = paint.shadow_only ? 0.0f : cover(wall - paint.outline);
            if (paint.shadow_alpha > 0) {
                // The letter over its soft shadow: darkest against the moved letter (grown by shadow_grow), fading
                // to nothing shadow_soft beyond it.
                const float beyond = std::max(0.0f, at(px - paint.shadow_x, py - paint.shadow_y) - paint.shadow_grow);
                const float t = std::max(0.0f, 1.0f - beyond / std::max(paint.shadow_soft, 0.01f));
                float       glow = paint.shadow_alpha * t * t;
                if (paint.band_alpha > 0) {
                    // The band, as one light with the glow: the stronger of the two at each pixel, not both.
                    const float fade = std::max(paint.band_fade, 0.01f);
                    const float across = std::clamp(1.0f - std::max(0.0f, std::abs(py - paint.band_y) - paint.band_core) / fade, 0.0f, 1.0f);
                    const float along = std::clamp(std::min(px - span_left, span_right - px) / fade + 1.0f, 0.0f, 1.0f);
                    glow = std::max(glow, paint.band_alpha * across * across * (3 - 2 * across) * along);
                }
                const float shadow = glow * (1 - alpha);
                const float total = alpha + shadow;
                for (int c = 0; c < 3; c++) {
                    rgb[c] = total > 0 ? (rgb[c] * alpha + static_cast<float>(paint.shade[c]) * shadow) / total : rgb[c];
                }
                alpha = total;
            }
            for (int c = 0; c < 3; c++) {
                texel[c] = static_cast<unsigned char>(std::lround(std::clamp(rgb[c], 0.0f, 255.0f)));
            }
            texel[3] = static_cast<unsigned char>(std::lround(alpha * 255));
        }
    }
    Glyph glyph;
    if (!AddImage(rgba, w, h, glyph)) {
        return false;
    }
    glyph.x_offset = static_cast<float>(left);
    glyph.y_offset = static_cast<float>(top);
    out = glyph;
    return true;
}

} // namespace

bool GetPaintedGlyph(char32_t ch, int part, float box_w, float box_h, const Paint &paint, Glyph &out) {
    const Font &font = g_fonts[std::clamp(paint.slot, 0, 3)];
    if (!font.loaded || box_w < 1 || box_h < 1 || box_w > 512 || box_h > 512) {
        return false;
    }
    const std::string key = "g" + std::to_string(static_cast<unsigned>(ch)) + ":" + std::to_string(part) + ":" +
                            std::to_string(std::lround(box_w * 8)) + "," + std::to_string(std::lround(box_h * 8)) + ":" +
                            PaintKey(paint);
    if (auto found = g_atlas.painted.find(key); found != g_atlas.painted.end()) {
        out = found->second;
        return true;
    }
    if (g_atlas.painted_missing.count(key) != 0) {
        return false;
    }
    const int                       index = stbtt_FindGlyphIndex(&font.info, static_cast<int>(ch));
    std::vector<std::vector<Point>> contours = Piece(index != 0 ? Contours(font.info, index) : std::vector<std::vector<Point>>{}, part);
    if (contours.empty()) {
        g_atlas.painted_missing.insert(key);
        return false;
    }

    // The outline in the box's pixels, y down.
    float x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f;
    for (const auto &c : contours) {
        for (const Point &p : c) {
            x0 = std::min(x0, p.x);
            x1 = std::max(x1, p.x);
            y0 = std::min(y0, p.y);
            y1 = std::max(y1, p.y);
        }
    }
    const float sx = box_w / std::max(x1 - x0, 1.0f), sy = box_h / std::max(y1 - y0, 1.0f);
    for (auto &c : contours) {
        for (Point &p : c) {
            p = {(p.x - x0) * sx, (y1 - p.y) * sy};
        }
    }

    Glyph glyph;
    if (!PaintContours(contours, box_w, box_h, paint, glyph)) {
        g_atlas.painted_missing.insert(key);
        return false;
    }
    g_atlas.painted[key] = glyph;
    out = glyph;
    return true;
}

bool GetPaintedWord(const std::vector<PlacedLetter> &letters, const Paint &paint, Glyph &out) {
    const Font &font = g_fonts[std::clamp(paint.slot, 0, 3)];
    if (!font.loaded || letters.empty()) {
        return false;
    }
    // The key: every letter and its box, and the look.
    std::string key = "w";
    for (const PlacedLetter &letter : letters) {
        key += std::to_string(static_cast<unsigned>(letter.ch)) + ":";
        for (float v : {letter.x, letter.y, letter.w, letter.h}) {
            key += std::to_string(std::lround(v * 64)) + ",";
        }
    }
    key += PaintKey(paint);
    if (auto found = g_atlas.painted.find(key); found != g_atlas.painted.end()) {
        out = found->second;
        return true;
    }
    if (g_atlas.painted_missing.count(key) != 0) {
        return false;
    }
    // Every letter's outline stretched to its box, together in the word's pixels.
    std::vector<std::vector<Point>> contours;
    float                           box_w = 1.0f, box_h = 1.0f;
    for (const PlacedLetter &letter : letters) {
        const int                       index = stbtt_FindGlyphIndex(&font.info, static_cast<int>(letter.ch));
        std::vector<std::vector<Point>> own = index != 0 ? Contours(font.info, index) : std::vector<std::vector<Point>>{};
        if (own.empty()) {
            continue;
        }
        float x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f;
        for (const auto &c : own) {
            for (const Point &p : c) {
                x0 = std::min(x0, p.x);
                x1 = std::max(x1, p.x);
                y0 = std::min(y0, p.y);
                y1 = std::max(y1, p.y);
            }
        }
        const float sx = letter.w / std::max(x1 - x0, 1.0f), sy = letter.h / std::max(y1 - y0, 1.0f);
        for (auto &c : own) {
            for (Point &p : c) {
                p = {letter.x + (p.x - x0) * sx, letter.y + (y1 - p.y) * sy};
            }
            contours.push_back(std::move(c));
        }
        box_w = std::max(box_w, letter.x + letter.w);
        box_h = std::max(box_h, letter.y + letter.h);
    }
    Glyph glyph;
    if (contours.empty() || box_w > 1024 || box_h > 512 || !PaintContours(contours, box_w, box_h, paint, glyph)) {
        g_atlas.painted_missing.insert(key);
        return false;
    }
    g_atlas.painted[key] = glyph;
    out = glyph;
    return true;
}

bool GetGlyphBounds(char32_t ch, float (&bounds)[4], int slot) {
    const Font &font = g_fonts[std::clamp(slot, 0, 3)];
    const int   index = font.loaded ? stbtt_FindGlyphIndex(&font.info, static_cast<int>(ch)) : 0;
    if (index == 0) {
        return false;
    }
    bounds[0] = bounds[1] = 1e9f;
    bounds[2] = bounds[3] = -1e9f;
    for (const auto &c : Contours(font.info, index)) {
        for (const Point &p : c) {
            bounds[0] = std::min(bounds[0], p.x);
            bounds[1] = std::min(bounds[1], p.y);
            bounds[2] = std::max(bounds[2], p.x);
            bounds[3] = std::max(bounds[3], p.y);
        }
    }
    return bounds[0] < bounds[2] && bounds[1] < bounds[3];
}

bool GetInkExtent(char32_t ch, int em_px, float &left, float &right) {
    const Font &font = g_fonts[g_variant];
    const int   index = font.loaded ? DrawnGlyph(ch) : 0;
    if (index == 0) {
        return false;
    }
    float x0 = 1e9f, x1 = -1e9f;
    for (const auto &c : Contours(font.info, index)) {
        for (const Point &p : c) {
            x0 = std::min(x0, p.x);
            x1 = std::max(x1, p.x);
        }
    }
    if (x0 >= x1) {
        return false;
    }
    const float scale = stbtt_ScaleForMappingEmToPixels(&font.info, static_cast<float>(em_px));
    left = x0 * scale;
    right = x1 * scale;
    return true;
}

unsigned Generation() {
    return g_generation;
}

bool RenderGlyph(char32_t ch, int em_px, std::vector<unsigned char> &alpha, int &w, int &h, int &x0, int &y0) {
    if (!g_fonts[g_variant].loaded || em_px < 4 || em_px > 512) {
        return false;
    }
    const float scale = stbtt_ScaleForMappingEmToPixels(&g_fonts[g_variant].info, static_cast<float>(em_px));
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

bool RenderGlyphScaled(char32_t ch, int slot, float scale, std::vector<unsigned char> &alpha, int &w, int &h, int &x0,
                       int &y0) {
    if (slot < 0 || slot > 3 || !g_fonts[slot].loaded) {
        return false;
    }
    const int index = stbtt_FindGlyphIndex(&g_fonts[slot].info, static_cast<int>(ch));
    if (index == 0) {
        return false;
    }
    Bitmap bitmap = Raster(g_fonts[slot].info, index, scale, scale);
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
    Metrics metrics = {0.8f, 0.2f, 0.6f, 0.7f};
    if (!g_fonts[g_variant].loaded) {
        return metrics;
    }
    int ascent, descent, gap;
    stbtt_GetFontVMetrics(&g_fonts[g_variant].info, &ascent, &descent, &gap);
    const float em = 1.0f / stbtt_ScaleForMappingEmToPixels(&g_fonts[g_variant].info, 1.0f);
    metrics.ascent = static_cast<float>(ascent) / em;
    metrics.descent = static_cast<float>(-descent) / em;
    int advance, bearing;
    stbtt_GetCodepointHMetrics(&g_fonts[g_variant].info, '0', &advance, &bearing);
    metrics.advance = static_cast<float>(advance) / em;
    int x0, y0, x1, y1;
    if (stbtt_GetCodepointBox(&g_fonts[g_variant].info, 'H', &x0, &y0, &x1, &y1) != 0 && y1 > y0) {
        metrics.cap = static_cast<float>(y1 - y0) / em;
        metrics.cap_drop = static_cast<float>(-y0) / em;
    }
    return metrics;
}

} // namespace capfont
