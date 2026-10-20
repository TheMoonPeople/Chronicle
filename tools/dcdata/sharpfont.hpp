#pragma once

// The Sharp text font: the community font (tools/font/DarkCloudCompendium*.ttf) reshaped, letter by letter, until
// drawn at the game's 14x20 size it matches the game's own letters in meswin gaiji.img, then drawn into a
// high-resolution copy of that glyph sheet. The fitting needs the disc's glyph sheet, so it runs when the data is
// extracted and nothing it makes is distributed. Header-only like the rest of dcdata.
//
// Each of the 88 letters gaiji.img has twice (the game's bold dialogue set and its narrower regular set) keeps the
// font's own outline points; a fit moves them:
//
//   warp   shift, scale and slant, a smooth displacement field (cubic B-spline, 4x5 nodes over the cell) and one stroke
//          weight
//   grids  the same with finer fields (8x6 displacement nodes) and a stroke weight that varies over the letter (7x5)
//   as is  the font's letter unchanged, its capital height matched to the cell's and centred on it
//
// kPicks chooses one per letter. The fit minimises the difference between the letter's exact-area coverage at 14x20
// and the cell's, with smoothness and size terms on the fields and the outline kept inside the cell. The stroke weight
// moves each control point along its corner's mitre, so both edges of a corner move by the weight.
//
// Written under <data>/sharp/:
//
//     font.ttf                    the bold set's letters, for text the port draws from a font (captions)
//     font_regular.ttf            the regular set's
//     <language>/gaiji.png        the language's glyph sheet at kSheetScale times, the 88 letters drawn from the fits
//                                 and every other cell scaled up as it is
//     version.txt                 kVersion, so a changed recipe makes them again

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <functional>
#include <map>
#include <mutex>
#include <numbers>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "dcdata.hpp"
#include "textures.hpp"

namespace dcdata::sharp {

namespace fs = std::filesystem;

/// Bumped whenever what Build makes changes, so it is made again.
inline constexpr int kVersion = 3;

/// How many times the cells' resolution the glyph sheet is drawn at.
inline constexpr int kSheetScale = 6;

/// The 88 letters of gaiji.img, in its order: nine 14x20 cells a row from u = 128 (bold) and u = 256 (regular).
inline constexpr std::string_view kOrder =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz'=\"!?#&+-*/%()@|<>{}[]:,.$0123456789";
inline constexpr int kCellW = 14;
inline constexpr int kCellH = 20;

enum class Fit {
    Warp,
    Grids,
    AsIs
};

/// The fit each letter uses, the same for both sets: chosen by eye, letter by letter.
inline Fit PickOf(char ch) {
    constexpr std::string_view grids = "!&*01245789?";
    constexpr std::string_view as_is = "=#-%|{}[]";
    if (as_is.find(ch) != std::string_view::npos) {
        return Fit::AsIs;
    }
    if (grids.find(ch) != std::string_view::npos) {
        return Fit::Grids;
    }
    return Fit::Warp;
}

struct Pt {
    double x = 0, y = 0;
};

inline Pt operator+(Pt a, Pt b) { return {a.x + b.x, a.y + b.y}; }

inline Pt operator-(Pt a, Pt b) { return {a.x - b.x, a.y - b.y}; }

inline Pt operator*(double k, Pt a) { return {k * a.x, k * a.y}; }

struct Contour {
    std::vector<Pt>   pts;
    std::vector<bool> on; // on-curve points; between two off-curve points one is implied halfway
};

// ---------------------------------------------------------------------------------------------- TrueType in

namespace ttf_detail {

inline std::uint16_t U16(const std::uint8_t *p) { return static_cast<std::uint16_t>(p[0] << 8 | p[1]); }

inline std::int16_t S16(const std::uint8_t *p) { return static_cast<std::int16_t>(U16(p)); }

inline std::uint32_t U32(const std::uint8_t *p) {
    return static_cast<std::uint32_t>(p[0]) << 24 | p[1] << 16 | p[2] << 8 | p[3];
}

inline void Put16(std::vector<std::uint8_t> &v, std::size_t at, int x) {
    v[at] = static_cast<std::uint8_t>(x >> 8);
    v[at + 1] = static_cast<std::uint8_t>(x);
}

inline void Add16(std::vector<std::uint8_t> &v, int x) {
    v.push_back(static_cast<std::uint8_t>(x >> 8));
    v.push_back(static_cast<std::uint8_t>(x));
}

inline void Add32(std::vector<std::uint8_t> &v, std::uint32_t x) {
    for (int s = 24; s >= 0; s -= 8) {
        v.push_back(static_cast<std::uint8_t>(x >> s));
    }
}

} // namespace ttf_detail

/// A glyph of a TrueType font: a simple one's contours in font units, or a composite one's bytes.
struct TtfGlyph {
    bool                      composite = false;
    std::vector<Contour>      contours;
    std::vector<std::uint8_t> raw; // composite: the glyph's bytes
    int                       advance = 0;
};

/// The TrueType font read whole: its tables, and every glyph decoded.
struct Ttf {
    std::vector<std::uint8_t>                            bytes;
    std::map<std::string, std::span<const std::uint8_t>> tables;
    int                                                  units_per_em = 1000;
    std::vector<TtfGlyph>                                glyphs;
    std::map<char32_t, int>                              cmap;

    std::span<const std::uint8_t> Table(std::string_view tag) const {
        auto found = tables.find(std::string(tag));
        if (found == tables.end()) {
            Fail("font has no {} table", tag);
        }
        return found->second;
    }

    const TtfGlyph &Of(char32_t ch) const {
        auto found = cmap.find(ch);
        if (found == cmap.end()) {
            Fail("font has no glyph for U+{:04X}", static_cast<unsigned>(ch));
        }
        return glyphs[static_cast<std::size_t>(found->second)];
    }
};

inline Ttf ReadTtf(std::vector<std::uint8_t> bytes) {
    using namespace ttf_detail;
    Ttf font;
    font.bytes = std::move(bytes);
    const std::uint8_t *d = font.bytes.data();
    if (font.bytes.size() < 12) {
        Fail("not a TrueType font");
    }
    int count = U16(d + 4);
    for (int i = 0; i < count; i++) {
        const std::uint8_t *r = d + 12 + i * 16;
        std::uint32_t       off = U32(r + 8), len = U32(r + 12);
        if (off + std::uint64_t(len) > font.bytes.size()) {
            Fail("font table out of range");
        }
        font.tables[std::string(reinterpret_cast<const char *>(r), 4)] = {d + off, len};
    }
    auto head = font.Table("head");
    font.units_per_em = U16(head.data() + 18);
    int  long_loca = S16(head.data() + 50);
    int  glyph_count = U16(font.Table("maxp").data() + 4);
    auto loca = font.Table("loca");
    auto glyf = font.Table("glyf");
    auto hhea = font.Table("hhea");
    auto hmtx = font.Table("hmtx");
    int  metrics = U16(hhea.data() + 34);

    font.glyphs.resize(static_cast<std::size_t>(glyph_count));
    for (int g = 0; g < glyph_count; g++) {
        std::uint32_t a = long_loca ? U32(loca.data() + 4 * g) : 2u * U16(loca.data() + 2 * g);
        std::uint32_t b = long_loca ? U32(loca.data() + 4 * (g + 1)) : 2u * U16(loca.data() + 2 * (g + 1));
        TtfGlyph     &glyph = font.glyphs[static_cast<std::size_t>(g)];
        glyph.advance = U16(hmtx.data() + 4 * std::min(g, metrics - 1));
        if (b <= a) {
            continue;
        }
        const std::uint8_t *p = glyf.data() + a;
        int                 contours = S16(p);
        if (contours < 0) {
            glyph.composite = true;
            glyph.raw.assign(p, glyf.data() + b);
            continue;
        }
        std::vector<int> ends(static_cast<std::size_t>(contours));
        for (int c = 0; c < contours; c++) {
            ends[static_cast<std::size_t>(c)] = U16(p + 10 + 2 * c);
        }
        int                 points = contours ? ends.back() + 1 : 0;
        const std::uint8_t *q = p + 10 + 2 * contours;
        q += 2 + U16(q); // instructions
        std::vector<std::uint8_t> flags;
        while (static_cast<int>(flags.size()) < points) {
            std::uint8_t f = *q++;
            flags.push_back(f);
            if (f & 8) {
                for (int r = *q++; r > 0; r--) {
                    flags.push_back(f);
                }
            }
        }
        std::vector<int> xs(static_cast<std::size_t>(points)), ys(static_cast<std::size_t>(points));
        auto             coords = [&](std::vector<int> &out, int short_bit, int same_bit) {
            int value = 0;
            for (int i = 0; i < points; i++) {
                std::uint8_t f = flags[static_cast<std::size_t>(i)];
                if (f & short_bit) {
                    int delta = *q++;
                    value += (f & same_bit) ? delta : -delta;
                } else if (!(f & same_bit)) {
                    value += S16(q);
                    q += 2;
                }
                out[static_cast<std::size_t>(i)] = value;
            }
        };
        coords(xs, 2, 16);
        coords(ys, 4, 32);
        int start = 0;
        for (int end : ends) {
            Contour c;
            for (int i = start; i <= end; i++) {
                c.pts.push_back({static_cast<double>(xs[static_cast<std::size_t>(i)]), static_cast<double>(ys[static_cast<std::size_t>(i)])});
                c.on.push_back(flags[static_cast<std::size_t>(i)] & 1);
            }
            glyph.contours.push_back(std::move(c));
            start = end + 1;
        }
    }

    // cmap: the Windows Unicode BMP subtable (format 4).
    auto cmap = font.Table("cmap");
    int  subtables = U16(cmap.data() + 2);
    for (int i = 0; i < subtables; i++) {
        const std::uint8_t *r = cmap.data() + 4 + 8 * i;
        int                 platform = U16(r), encoding = U16(r + 2);
        const std::uint8_t *t = cmap.data() + U32(r + 4);
        if (!((platform == 3 && encoding == 1) || platform == 0) || U16(t) != 4) {
            continue;
        }
        int                 segments = U16(t + 6) / 2;
        const std::uint8_t *ends = t + 14, *starts = ends + 2 * segments + 2, *deltas = starts + 2 * segments,
                           *ranges = deltas + 2 * segments;
        for (int s = 0; s < segments; s++) {
            int first = U16(starts + 2 * s), last = U16(ends + 2 * s), delta = S16(deltas + 2 * s),
                range = U16(ranges + 2 * s);
            for (int c = first; c <= last && c != 0xFFFF; c++) {
                int g;
                if (range == 0) {
                    g = (c + delta) & 0xFFFF;
                } else {
                    const std::uint8_t *at = ranges + 2 * s + range + 2 * (c - first);
                    g = U16(at);
                    if (g != 0) {
                        g = (g + delta) & 0xFFFF;
                    }
                }
                if (g != 0 && g < glyph_count) {
                    font.cmap.emplace(static_cast<char32_t>(c), g);
                }
            }
        }
        break;
    }
    return font;
}

// ---------------------------------------------------------------------------------------------- TrueType out

/// The font with the glyphs replaced: hinting dropped, every glyph `advance` wide, the given ascent and descent.
/// `scale_rest` scales the glyphs `replaced` does not name (the accents and symbols the letters are composed with).
inline std::vector<std::uint8_t> WriteTtf(const Ttf &font, const std::map<int, std::vector<Contour>> &replaced,
                                          double scale_rest, int advance, int ascent, int descent) {
    using namespace ttf_detail;
    const int                 count = static_cast<int>(font.glyphs.size());
    std::vector<std::uint8_t> glyf, loca;
    std::vector<int>          lsb(static_cast<std::size_t>(count), 0), xmax(static_cast<std::size_t>(count), 0);
    int                       max_points = 0, max_contours = 0;
    int                       bx0 = 32767, by0 = 32767, bx1 = -32768, by1 = -32768;
    for (int g = 0; g < count; g++) {
        Add32(loca, static_cast<std::uint32_t>(glyf.size()));
        const TtfGlyph &glyph = font.glyphs[static_cast<std::size_t>(g)];
        if (glyph.composite) {
            // The components, their offsets scaled like the rest, without instructions.
            const std::uint8_t       *p = glyph.raw.data();
            std::vector<std::uint8_t> out(glyph.raw.begin(), glyph.raw.begin() + 10);
            for (int k = 0; k < 4; k++) {
                Put16(out, 2 + 2 * k, static_cast<int>(std::lround(S16(p + 2 + 2 * k) * scale_rest)));
            }
            const std::uint8_t *q = p + 10;
            for (bool more = true; more;) {
                int flags = U16(q), index = U16(q + 2);
                q += 4;
                int a1, a2;
                if (flags & 1) {
                    a1 = S16(q);
                    a2 = S16(q + 2);
                    q += 4;
                } else {
                    a1 = (flags & 2) ? static_cast<std::int8_t>(q[0]) : q[0];
                    a2 = (flags & 2) ? static_cast<std::int8_t>(q[1]) : q[1];
                    q += 2;
                }
                if (flags & 2) {
                    a1 = static_cast<int>(std::lround(a1 * scale_rest));
                    a2 = static_cast<int>(std::lround(a2 * scale_rest));
                }
                more = flags & 0x20;
                Add16(out, (flags | 1) & ~0x100);
                Add16(out, index);
                Add16(out, a1);
                Add16(out, a2);
                int extra = (flags & 8) ? 2 : (flags & 0x40) ? 4
                                          : (flags & 0x80)   ? 8
                                                             : 0;
                out.insert(out.end(), q, q + extra);
                q += extra;
            }
            glyf.insert(glyf.end(), out.begin(), out.end());
            lsb[static_cast<std::size_t>(g)] = static_cast<std::int16_t>(U16(out.data() + 2));
        } else {
            std::vector<Contour> contours;
            if (auto found = replaced.find(g); found != replaced.end()) {
                contours = found->second;
            } else {
                contours = glyph.contours;
                for (Contour &c : contours) {
                    for (Pt &p : c.pts) {
                        p = scale_rest * p;
                    }
                }
            }
            std::vector<int>  xs, ys, ends;
            std::vector<bool> on;
            for (const Contour &c : contours) {
                for (std::size_t i = 0; i < c.pts.size(); i++) {
                    xs.push_back(static_cast<int>(std::lround(c.pts[i].x)));
                    ys.push_back(static_cast<int>(std::lround(c.pts[i].y)));
                    on.push_back(c.on[i]);
                }
                ends.push_back(static_cast<int>(xs.size()) - 1);
            }
            if (!xs.empty()) {
                int x0 = *std::min_element(xs.begin(), xs.end()), x1 = *std::max_element(xs.begin(), xs.end());
                int y0 = *std::min_element(ys.begin(), ys.end()), y1 = *std::max_element(ys.begin(), ys.end());
                Add16(glyf, static_cast<int>(ends.size()));
                Add16(glyf, x0);
                Add16(glyf, y0);
                Add16(glyf, x1);
                Add16(glyf, y1);
                for (int e : ends) {
                    Add16(glyf, e);
                }
                Add16(glyf, 0); // no instructions
                std::vector<std::uint8_t> flags, xb, yb;
                int                       px = 0, py = 0;
                for (std::size_t i = 0; i < xs.size(); i++) {
                    int          dx = xs[i] - px, dy = ys[i] - py;
                    std::uint8_t f = on[i] ? 1 : 0;
                    if (dx == 0) {
                        f |= 16;
                    } else if (std::abs(dx) < 256) {
                        f |= 2 | (dx > 0 ? 16 : 0);
                        xb.push_back(static_cast<std::uint8_t>(std::abs(dx)));
                    } else {
                        Add16(xb, dx);
                    }
                    if (dy == 0) {
                        f |= 32;
                    } else if (std::abs(dy) < 256) {
                        f |= 4 | (dy > 0 ? 32 : 0);
                        yb.push_back(static_cast<std::uint8_t>(std::abs(dy)));
                    } else {
                        Add16(yb, dy);
                    }
                    flags.push_back(f);
                    px = xs[i];
                    py = ys[i];
                }
                glyf.insert(glyf.end(), flags.begin(), flags.end());
                glyf.insert(glyf.end(), xb.begin(), xb.end());
                glyf.insert(glyf.end(), yb.begin(), yb.end());
                lsb[static_cast<std::size_t>(g)] = x0;
                xmax[static_cast<std::size_t>(g)] = x1;
                max_points = std::max(max_points, static_cast<int>(xs.size()));
                max_contours = std::max(max_contours, static_cast<int>(ends.size()));
                bx0 = std::min(bx0, x0);
                by0 = std::min(by0, y0);
                bx1 = std::max(bx1, x1);
                by1 = std::max(by1, y1);
            }
        }
        while (glyf.size() % 4) {
            glyf.push_back(0);
        }
    }
    Add32(loca, static_cast<std::uint32_t>(glyf.size()));

    std::vector<std::uint8_t> hmtx;
    for (int g = 0; g < count; g++) {
        Add16(hmtx, advance);
        Add16(hmtx, lsb[static_cast<std::size_t>(g)]);
    }
    auto copy = [&](std::string_view tag) {
        auto t = font.Table(tag);
        return std::vector<std::uint8_t>(t.begin(), t.end());
    };
    std::vector<std::uint8_t> head = copy("head");
    Put16(head, 36, bx0);
    Put16(head, 38, by0);
    Put16(head, 40, bx1);
    Put16(head, 42, by1);
    Put16(head, 50, 1);
    std::fill(head.begin() + 8, head.begin() + 12, 0);
    std::vector<std::uint8_t> hhea = copy("hhea");
    Put16(hhea, 4, ascent);
    Put16(hhea, 6, -descent);
    Put16(hhea, 8, 0);
    Put16(hhea, 10, advance);
    Put16(hhea, 34, count);
    std::vector<std::uint8_t> maxp = copy("maxp");
    if (maxp.size() >= 32) {
        Put16(maxp, 6, max_points);
        Put16(maxp, 8, max_contours);
        Put16(maxp, 26, 0);
    }
    std::map<std::string, std::vector<std::uint8_t>> out;
    for (const auto &[tag, data] : font.tables) {
        static const std::array<std::string_view, 10> dropped = {"cvt ", "fpgm", "prep", "hdmx", "LTSH",
                                                                 "VDMX", "gasp", "kern", "GPOS", "DSIG"};
        if (std::find(dropped.begin(), dropped.end(), tag) == dropped.end()) {
            out[tag] = std::vector<std::uint8_t>(data.begin(), data.end());
        }
    }
    out["glyf"] = glyf;
    out["loca"] = loca;
    out["hmtx"] = hmtx;
    out["head"] = head;
    out["hhea"] = hhea;
    out["maxp"] = maxp;
    if (out.contains("OS/2") && out["OS/2"].size() >= 78) {
        std::vector<std::uint8_t> &os2 = out["OS/2"];
        Put16(os2, 68, ascent);
        Put16(os2, 70, -descent);
        Put16(os2, 72, 0);
        Put16(os2, 74, ascent);
        Put16(os2, 76, descent);
    }

    auto checksum = [](const std::vector<std::uint8_t> &v) {
        std::uint32_t sum = 0;
        for (std::size_t i = 0; i < v.size(); i += 4) {
            std::uint32_t word = 0;
            for (std::size_t k = 0; k < 4; k++) {
                word = word << 8 | (i + k < v.size() ? v[i + k] : 0);
            }
            sum += word;
        }
        return sum;
    };
    const int tables = static_cast<int>(out.size());
    int       power = 1, log2 = 0;
    while (power * 2 <= tables) {
        power *= 2;
        log2++;
    }
    std::vector<std::uint8_t> file;
    Add32(file, 0x00010000);
    Add16(file, tables);
    Add16(file, power * 16);
    Add16(file, log2);
    Add16(file, tables * 16 - power * 16);
    std::size_t offset = 12 + 16 * static_cast<std::size_t>(tables);
    std::size_t head_at = 0;
    for (const auto &[tag, data] : out) {
        file.insert(file.end(), tag.begin(), tag.end());
        Add32(file, checksum(data));
        Add32(file, static_cast<std::uint32_t>(offset));
        Add32(file, static_cast<std::uint32_t>(data.size()));
        if (tag == "head") {
            head_at = offset;
        }
        offset += (data.size() + 3) & ~std::size_t(3);
    }
    for (const auto &[tag, data] : out) {
        file.insert(file.end(), data.begin(), data.end());
        while (file.size() % 4) {
            file.push_back(0);
        }
    }
    std::uint32_t adjust = 0xB1B0AFBAu - checksum(file);
    for (int k = 0; k < 4; k++) {
        file[head_at + 8 + k] = static_cast<std::uint8_t>(adjust >> (24 - 8 * k));
    }
    return file;
}

// ---------------------------------------------------------------------------------------------- outlines

/// A contour as a closed polyline: each quadratic piece in n steps.
inline std::vector<Pt> Flatten(const Contour &c, int n = 6) {
    std::vector<Pt>   P = c.pts;
    std::vector<bool> on = c.on;
    std::vector<Pt>   out;
    if (P.size() < 2) {
        return out;
    }
    std::size_t m = P.size();
    std::size_t k0 = m;
    for (std::size_t i = 0; i < m; i++) {
        if (on[i]) {
            k0 = i;
            break;
        }
    }
    if (k0 == m) {
        P.push_back(0.5 * (P[0] + P[1]));
        on.push_back(true);
        k0 = m;
        m++;
    }
    auto curve = [&](Pt a, Pt ctrl, Pt b) {
        for (int s = 0; s < n; s++) {
            double t = static_cast<double>(s) / n;
            out.push_back((1 - t) * (1 - t) * a + 2 * (1 - t) * t * ctrl + t * t * b);
        }
    };
    Pt                cur = P[k0];
    std::optional<Pt> ctrl;
    for (std::size_t step = 1; step <= m; step++) {
        std::size_t i = (k0 + step) % m;
        Pt          p = P[i];
        if (on[i]) {
            if (!ctrl) {
                out.push_back(cur);
            } else {
                curve(cur, *ctrl, p);
            }
            cur = p;
            ctrl.reset();
        } else {
            if (ctrl) {
                Pt mid = 0.5 * (*ctrl + p);
                curve(cur, *ctrl, mid);
                cur = mid;
            }
            ctrl = p;
        }
    }
    if (ctrl) {
        curve(cur, *ctrl, P[k0]);
    }
    return out;
}

/// The exact coverage of each pixel of an h x w grid (pixels `size` cell px across, its corner at `origin`) by closed
/// polylines, non-zero (as font rasterisers do: the accumulated signed area, clamped).
inline std::vector<double> Coverage(const std::vector<std::vector<Pt>> &polys, int h, int w, double size = 1.0,
                                    Pt origin = {}) {
    constexpr int       M = 8;
    const int           wb = w + 2 * M;
    std::vector<double> acc(static_cast<std::size_t>((h + 1) * wb + 2), 0.0);
    for (const auto &L : polys) {
        if (L.size() < 3) {
            continue;
        }
        for (std::size_t k = 0; k < L.size(); k++) {
            Pt     a = L[k], b = L[(k + 1) % L.size()];
            double x0 = (a.x - origin.x) / size + M, y0 = (a.y - origin.y) / size;
            double x1 = (b.x - origin.x) / size + M, y1 = (b.y - origin.y) / size;
            if (y0 == y1) {
                continue;
            }
            double dir, ax, ay, bx, by;
            if (y0 < y1) {
                dir = 1;
                ax = x0, ay = y0, bx = x1, by = y1;
            } else {
                dir = -1;
                ax = x1, ay = y1, bx = x0, by = y0;
            }
            double dxdy = (bx - ax) / (by - ay);
            double x = ax;
            if (ay < 0) {
                x -= ay * dxdy;
            }
            int ys = static_cast<int>(std::floor(std::max(ay, 0.0)));
            int ye = std::min(h, static_cast<int>(std::ceil(by)));
            for (int y = ys; y < ye; y++) {
                std::size_t ls = static_cast<std::size_t>(y) * wb;
                double      dy = std::min(y + 1.0, by) - std::max(static_cast<double>(y), ay);
                double      xn = x + dxdy * dy;
                double      d = dy * dir;
                double      xa = std::clamp(std::min(x, xn), 0.0, wb - 2.0);
                double      xb = std::clamp(std::max(x, xn), 0.0, wb - 2.0);
                double      x0f = std::floor(xa);
                int         x0i = static_cast<int>(x0f);
                double      x1c = std::ceil(xb);
                int         x1i = static_cast<int>(x1c);
                if (x1i <= x0i + 1) {
                    double xmf = std::clamp(0.5 * (x + xn) - x0f, 0.0, 1.0);
                    acc[ls + x0i] += d - d * xmf;
                    acc[ls + x0i + 1] += d * xmf;
                } else {
                    double s = 1.0 / (xb - xa);
                    double f0 = xa - x0f;
                    double a0 = 0.5 * s * (1 - f0) * (1 - f0);
                    double f1 = xb - x1c + 1;
                    double am = 0.5 * s * f1 * f1;
                    acc[ls + x0i] += d * a0;
                    if (x1i == x0i + 2) {
                        acc[ls + x0i + 1] += d * (1 - a0 - am);
                    } else {
                        double a1 = s * (1.5 - f0);
                        acc[ls + x0i + 1] += d * (a1 - a0);
                        for (int xi = x0i + 2; xi < x1i - 1; xi++) {
                            acc[ls + xi] += d * s;
                        }
                        double a2 = a1 + (x1i - x0i - 3) * s;
                        acc[ls + x1i - 1] += d * (1 - a2 - am);
                    }
                    acc[ls + x1i] += d * am;
                }
                x = xn;
            }
        }
    }
    std::vector<double> out(static_cast<std::size_t>(h) * w, 0.0);
    for (int y = 0; y < h; y++) {
        double run = 0;
        for (int xi = 0; xi < wb; xi++) {
            run += acc[static_cast<std::size_t>(y) * wb + xi];
            if (xi >= M && xi < M + w) {
                out[static_cast<std::size_t>(y) * w + (xi - M)] = std::min(1.0, std::abs(run));
            }
        }
    }
    return out;
}

inline double SignedArea(const std::vector<Pt> &p) {
    double a = 0;
    for (std::size_t i = 0; i < p.size(); i++) {
        const Pt &u = p[i], &v = p[(i + 1) % p.size()];
        a += u.x * v.y - v.x * u.y;
    }
    return a / 2;
}

/// Moves a contour's control points w px off the ink (into it when negative), along each corner's mitre so both of
/// a corner's edges move by w (capped where a corner is very sharp).
inline std::vector<Pt> Offset(const std::vector<Pt> &q, const std::vector<double> &w, double outer) {
    const std::size_t n = q.size();
    std::vector<Pt>   out(n);
    auto              unit_normal = [](Pt d) {
        double len = std::hypot(d.x, d.y) + 1e-12;
        return Pt{d.y / len, -d.x / len};
    };
    for (std::size_t i = 0; i < n; i++) {
        Pt     n1 = unit_normal(q[i] - q[(i + n - 1) % n]);
        Pt     n2 = unit_normal(q[(i + 1) % n] - q[i]);
        Pt     m = n1 + n2;
        double len = std::hypot(m.x, m.y) + 1e-12;
        m = (1 / len) * m;
        double c = std::max(m.x * n1.x + m.y * n1.y, 0.35);
        out[i] = q[i] + (outer * w[i] / c) * m;
    }
    return out;
}

// Cubic B-spline weights of n nodes over u in [0, 1]. The 4x5 warp's own form: needs n >= 4, the end nodes stopping
// the spline.
inline std::array<double, 4> SplineWeights(double u, int n, int &first) {
    double t = u * (n - 3);
    first = std::clamp(static_cast<int>(std::floor(t)), 0, n - 4);
    double f = t - first;
    return {(1 - f) * (1 - f) * (1 - f) / 6, (3 * f * f * f - 6 * f * f + 4) / 6,
            (-3 * f * f * f + 3 * f * f + 3 * f + 1) / 6, f * f * f / 6};
}

// The finer fields' form: any n >= 1, the end nodes repeated.
inline void SplineWeightsAny(double u, int n, std::array<int, 4> &at, std::array<double, 4> &w) {
    if (n == 1) {
        at = {0, 0, 0, 0};
        w = {1, 0, 0, 0};
        return;
    }
    double t = u * (n - 1);
    int    i = static_cast<int>(std::floor(t));
    double f = t - i;
    w = {(1 - f) * (1 - f) * (1 - f) / 6, (3 * f * f * f - 6 * f * f + 4) / 6, (-3 * f * f * f + 3 * f * f + 3 * f + 1) / 6,
         f * f * f / 6};
    for (int k = 0; k < 4; k++) {
        at[static_cast<std::size_t>(k)] = std::clamp(i + k - 1, 0, n - 1);
    }
}

// ---------------------------------------------------------------------------------------------- least squares

/// Bounded nonlinear least squares (Levenberg-Marquardt, steps clipped to the bounds, forward-difference Jacobian).
/// `budget` counts evaluations of the residuals outside the Jacobian.
inline std::vector<double> LeastSquares(const std::function<std::vector<double>(const std::vector<double> &)> &f,
                                        std::vector<double> x, const std::vector<double> &lo,
                                        const std::vector<double> &hi, int budget, double *cost_out = nullptr) {
    const std::size_t n = x.size();
    for (std::size_t j = 0; j < n; j++) {
        x[j] = std::clamp(x[j], lo[j], hi[j]);
    }
    auto cost_of = [](const std::vector<double> &r) {
        double c = 0;
        for (double v : r) {
            c += v * v;
        }
        return c / 2;
    };
    std::vector<double> r = f(x);
    double              cost = cost_of(r);
    int                 evals = 1;
    double              lambda = 1e-3;
    const std::size_t   m = r.size();
    std::vector<double> J(m * n), normal(n * n), g(n), step(n), trial(n);
    int                 stalls = 0;
    while (evals < budget) {
        for (std::size_t j = 0; j < n; j++) {
            double h = 1.4901161193847656e-08 * std::max(1.0, std::abs(x[j]));
            if (x[j] + h > hi[j]) {
                h = -h;
            }
            std::vector<double> xj = x;
            xj[j] += h;
            std::vector<double> rj = f(xj);
            for (std::size_t i = 0; i < m; i++) {
                J[i * n + j] = (rj[i] - r[i]) / h;
            }
        }
        for (std::size_t a = 0; a < n; a++) {
            double s = 0;
            for (std::size_t i = 0; i < m; i++) {
                s += J[i * n + a] * r[i];
            }
            g[a] = s;
            for (std::size_t b = a; b < n; b++) {
                double t = 0;
                for (std::size_t i = 0; i < m; i++) {
                    t += J[i * n + a] * J[i * n + b];
                }
                normal[a * n + b] = normal[b * n + a] = t;
            }
        }
        bool improved = false;
        while (evals < budget) {
            // (A + lambda diag A) step = -g, by Cholesky.
            std::vector<double> L(normal);
            for (std::size_t a = 0; a < n; a++) {
                L[a * n + a] += lambda * std::max(normal[a * n + a], 1e-12) + 1e-12;
            }
            bool ok = true;
            for (std::size_t a = 0; a < n && ok; a++) {
                for (std::size_t b = 0; b <= a; b++) {
                    double s = L[a * n + b];
                    for (std::size_t k = 0; k < b; k++) {
                        s -= L[a * n + k] * L[b * n + k];
                    }
                    if (a == b) {
                        if (s <= 0) {
                            ok = false;
                            break;
                        }
                        L[a * n + a] = std::sqrt(s);
                    } else {
                        L[a * n + b] = s / L[b * n + b];
                    }
                }
            }
            if (!ok) {
                lambda *= 10;
                continue;
            }
            std::vector<double> z(n);
            for (std::size_t a = 0; a < n; a++) {
                double s = -g[a];
                for (std::size_t k = 0; k < a; k++) {
                    s -= L[a * n + k] * z[k];
                }
                z[a] = s / L[a * n + a];
            }
            for (std::size_t a = n; a-- > 0;) {
                double s = z[a];
                for (std::size_t k = a + 1; k < n; k++) {
                    s -= L[k * n + a] * step[k];
                }
                step[a] = s / L[a * n + a];
            }
            double moved = 0;
            for (std::size_t j = 0; j < n; j++) {
                trial[j] = std::clamp(x[j] + step[j], lo[j], hi[j]);
                moved = std::max(moved, std::abs(trial[j] - x[j]));
            }
            std::vector<double> rt = f(trial);
            evals++;
            double ct = cost_of(rt);
            if (ct < cost) {
                stalls = (cost - ct) < 1e-10 * cost ? stalls + 1 : 0;
                x = trial;
                r = std::move(rt);
                cost = ct;
                lambda = std::max(lambda / 3, 1e-9);
                improved = true;
                break;
            }
            lambda *= 4;
            if (lambda > 1e10 || moved < 1e-12) {
                break;
            }
        }
        if (!improved || stalls >= 3) {
            break;
        }
    }
    if (cost_out != nullptr) {
        *cost_out = cost;
    }
    return x;
}

// ---------------------------------------------------------------------------------------------- the fits

/// A letter being fitted: the font's contours (font units, y up) and the cell it is matched to (coverage, rows top
/// to bottom).
struct Letter {
    std::vector<Contour> contours;
    std::vector<double>  cell;      // kCellH x kCellW
    double               outer = 1; // which side of a contour is ink, once y points down
    Pt                   lo, hi;    // the outline's bounds
};

inline Letter MakeLetter(std::vector<Contour> contours, std::vector<double> cell) {
    Letter l{std::move(contours), std::move(cell)};
    double best = 0, sign = 1;
    bool   first = true;
    for (const Contour &c : l.contours) {
        double a = SignedArea(c.pts);
        if (std::abs(a) > best) {
            best = std::abs(a);
            sign = a < 0 ? -1 : 1;
        }
        for (const Pt &p : c.pts) {
            if (first) {
                l.lo = l.hi = p;
                first = false;
            }
            l.lo = {std::min(l.lo.x, p.x), std::min(l.lo.y, p.y)};
            l.hi = {std::max(l.hi.x, p.x), std::max(l.hi.y, p.y)};
        }
    }
    l.outer = -sign;
    return l;
}

/// The cell size and place a letter starts from: its height to the cell's ink height, centred on the ink.
inline double StartScale(const Letter &l, double &ox, double &oy) {
    int    top = kCellH, bottom = -1;
    double xsum = 0;
    int    count = 0;
    for (int y = 0; y < kCellH; y++) {
        for (int x = 0; x < kCellW; x++) {
            if (l.cell[static_cast<std::size_t>(y) * kCellW + x] > 0.35) {
                top = std::min(top, y);
                bottom = std::max(bottom, y);
                xsum += x;
                count++;
            }
        }
    }
    if (count == 0) {
        top = 0;
        bottom = kCellH - 1;
        xsum = kCellW / 2.0;
        count = 1;
    }
    double s0 = (bottom - top + 1) / std::max(l.hi.y - l.lo.y, 1.0);
    ox = xsum / count + 0.5 - s0 * (l.lo.x + l.hi.x) / 2;
    oy = top + s0 * l.hi.y;
    return s0;
}

/// The shapes both fits share: affine part v[0..4] (x and y scale relative to s0, slant, shift), stroke weight v[5],
/// a displacement field gy x gx (x then y), and a weight field wy x wx added to v[5].
struct Model {
    const Letter *letter;
    double        s0;
    int           gy, gx, wy, wx;
    bool          coarse_spline; // the 4x5 warp's spline form

    int Size() const { return 6 + 2 * gy * gx + wy * wx; }

    double Field(const double *grid, int ny, int nx, double x, double y) const {
        double u = std::clamp((x + 1) / 16, 0.0, 1.0), w = std::clamp((y + 1) / 22, 0.0, 1.0);
        if (coarse_spline) {
            int    iu, iw;
            auto   bu = SplineWeights(u, nx, iu);
            auto   bw = SplineWeights(w, ny, iw);
            double s = 0;
            for (int a = 0; a < 4; a++) {
                for (int b = 0; b < 4; b++) {
                    s += bw[static_cast<std::size_t>(a)] * bu[static_cast<std::size_t>(b)] * grid[(iw + a) * nx + iu + b];
                }
            }
            return s;
        }
        std::array<int, 4>    au, aw;
        std::array<double, 4> bu, bw;
        SplineWeightsAny(u, nx, au, bu);
        SplineWeightsAny(w, ny, aw, bw);
        double s = 0;
        for (int a = 0; a < 4; a++) {
            for (int b = 0; b < 4; b++) {
                s += bw[static_cast<std::size_t>(a)] * bu[static_cast<std::size_t>(b)] *
                     grid[aw[static_cast<std::size_t>(a)] * nx + au[static_cast<std::size_t>(b)]];
            }
        }
        return s;
    }

    // The control points in cell px before the stroke weight, contour by contour.
    std::vector<std::vector<Pt>> Placed(const std::vector<double> &v) const {
        const double                 sx = s0 * v[0], sy = s0 * v[1];
        const double                *gxs = &v[6], *gys = &v[6 + gy * gx];
        std::vector<std::vector<Pt>> out;
        for (const Contour &c : letter->contours) {
            std::vector<Pt> q;
            for (const Pt &p : c.pts) {
                double x = sx * p.x - v[2] * sy * p.y + v[3], y = -sy * p.y + v[4];
                q.push_back({x + Field(gxs, gy, gx, x, y), y + Field(gys, gy, gx, x, y)});
            }
            out.push_back(std::move(q));
        }
        return out;
    }

    // The control points with the stroke weight, in cell px.
    std::vector<std::vector<Pt>> Outline(const std::vector<double> &v) const {
        std::vector<std::vector<Pt>> placed = Placed(v), out;
        const double                *wg = &v[6 + 2 * gy * gx];
        for (const auto &q : placed) {
            std::vector<double> w(q.size(), v[5]);
            if (wy * wx > 1 || !coarse_spline) {
                for (std::size_t i = 0; i < q.size(); i++) {
                    w[i] = v[5] + Field(wg, wy, wx, q[i].x, q[i].y);
                }
            }
            out.push_back(Offset(q, w, letter->outer));
        }
        return out;
    }

    std::vector<std::vector<Pt>> Polys(const std::vector<double> &v, int steps = 6) const {
        std::vector<std::vector<Pt>> outline = Outline(v), out;
        for (std::size_t c = 0; c < outline.size(); c++) {
            out.push_back(Flatten({outline[c], letter->contours[c].on}, steps));
        }
        return out;
    }

    // The residuals: the 14x20 coverage against the cell, then the fields' smoothness and size, a pull to the font's
    // own proportions and the outline kept in the cell.
    std::vector<double> Residuals(const std::vector<double> &v, double reg, double wsm) const {
        std::vector<double> r = Coverage(Polys(v), kCellH, kCellW);
        for (std::size_t i = 0; i < r.size(); i++) {
            r[i] -= letter->cell[i];
        }
        for (int k = 0; k < 2; k++) {
            const double *grid = &v[6 + k * gy * gx];
            for (int y = 0; y + 2 < gy; y++) {
                for (int x = 0; x < gx; x++) {
                    r.push_back(reg * (grid[(y + 2) * gx + x] - 2 * grid[(y + 1) * gx + x] + grid[y * gx + x]));
                }
            }
        }
        for (int k = 0; k < 2; k++) {
            const double *grid = &v[6 + k * gy * gx];
            for (int y = 0; y < gy; y++) {
                for (int x = 0; x + 2 < gx; x++) {
                    r.push_back(reg * (grid[y * gx + x + 2] - 2 * grid[y * gx + x + 1] + grid[y * gx + x]));
                }
            }
        }
        for (int k = 0; k < 2; k++) {
            const double *grid = &v[6 + k * gy * gx];
            for (int y = 0; y + 1 < gy; y++) {
                for (int x = 0; x < gx; x++) {
                    r.push_back(reg * 0.3 * (grid[(y + 1) * gx + x] - grid[y * gx + x]));
                }
            }
        }
        for (int k = 0; k < 2; k++) {
            const double *grid = &v[6 + k * gy * gx];
            for (int y = 0; y < gy; y++) {
                for (int x = 0; x + 1 < gx; x++) {
                    r.push_back(reg * 0.3 * (grid[y * gx + x + 1] - grid[y * gx + x]));
                }
            }
        }
        for (int i = 0; i < 2 * gy * gx; i++) {
            r.push_back(0.05 * v[6 + i]);
        }
        if (!coarse_spline) {
            const double *W = &v[6 + 2 * gy * gx];
            for (int y = 0; y + 1 < wy; y++) {
                for (int x = 0; x < wx; x++) {
                    r.push_back(wsm * (W[(y + 1) * wx + x] - W[y * wx + x]));
                }
            }
            for (int y = 0; y < wy; y++) {
                for (int x = 0; x + 1 < wx; x++) {
                    r.push_back(wsm * (W[y * wx + x + 1] - W[y * wx + x]));
                }
            }
            for (int i = 0; i < wy * wx; i++) {
                r.push_back(0.05 * W[i]);
            }
        }
        r.push_back(0.15 * (v[0] - 1));
        r.push_back(0.15 * (v[1] - 1));
        for (const auto &q : Placed(v)) {
            for (const Pt &p : q) {
                r.push_back(std::max(0.0, -p.x));
                r.push_back(std::max(0.0, p.x - kCellW));
                r.push_back(std::max(0.0, 0.3 - p.y));
                r.push_back(std::max(0.0, p.y - (kCellH - 0.3)));
            }
        }
        return r;
    }

    std::pair<std::vector<double>, std::vector<double>> Bounds() const {
        std::vector<double> lo(static_cast<std::size_t>(Size()), -2.0), hi(static_cast<std::size_t>(Size()), 2.0);
        const double        l6[6] = {0.6, 0.6, -0.4, -20, -20, -0.7}, h6[6] = {1.6, 1.6, 0.6, 40, 40, 0.7};
        for (int i = 0; i < 6; i++) {
            lo[static_cast<std::size_t>(i)] = l6[i];
            hi[static_cast<std::size_t>(i)] = h6[i];
        }
        return {lo, hi};
    }
};

/// The warp: the affine part and weight from a few shifted starts, then everything.
inline std::vector<double> FitWarp(const Model &m) {
    double ox, oy;
    StartScale(*m.letter, ox, oy);
    std::vector<double> v0(static_cast<std::size_t>(m.Size()), 0.0);
    v0[0] = v0[1] = 1;
    v0[2] = 0.12;
    v0[3] = ox;
    v0[4] = oy;
    auto [lo, hi] = m.Bounds();
    std::vector<double> best;
    double              best_cost = 0;
    const double        starts[5][2] = {
        {0,    0   },
        {0.5,  0   },
        {-0.5, 0   },
        {0,    0.5 },
        {0,    -0.5}
    };
    for (const auto &s : starts) {
        std::vector<double> a(v0.begin(), v0.begin() + 6);
        a[3] += s[0];
        a[4] += s[1];
        double cost;
        auto   affine = [&](const std::vector<double> &x) {
            std::vector<double> full = v0;
            std::copy(x.begin(), x.end(), full.begin());
            return m.Residuals(full, 0.06, 0.15);
        };
        std::vector<double> x = LeastSquares(affine, a, {lo.begin(), lo.begin() + 6}, {hi.begin(), hi.begin() + 6}, 300, &cost);
        if (best.empty() || cost < best_cost) {
            best = x;
            best_cost = cost;
        }
    }
    std::copy(best.begin(), best.end(), v0.begin());
    return LeastSquares([&](const std::vector<double> &v) { return m.Residuals(v, 0.06, 0.15); }, v0, lo, hi, 400);
}

/// Carries a fit into a model with other grid sizes: the grids resampled linearly, node to node.
inline std::vector<double> Lift(const Model &to, const std::vector<double> &v, const Model *from) {
    std::vector<double> out(static_cast<std::size_t>(to.Size()), 0.0);
    std::copy(v.begin(), v.begin() + 6, out.begin());
    if (from == nullptr) {
        return out;
    }
    auto resample = [](const double *src, int sy, int sx, double *dst, int dy, int dx) {
        for (int y = 0; y < dy; y++) {
            for (int x = 0; x < dx; x++) {
                double fy = dy > 1 ? static_cast<double>(y) * (sy - 1) / (dy - 1) : 0;
                double fx = dx > 1 ? static_cast<double>(x) * (sx - 1) / (dx - 1) : 0;
                int    y0 = std::min(static_cast<int>(fy), sy - 1), x0 = std::min(static_cast<int>(fx), sx - 1);
                int    y1 = std::min(y0 + 1, sy - 1), x1 = std::min(x0 + 1, sx - 1);
                double ty = fy - y0, tx = fx - x0;
                dst[y * dx + x] = (1 - ty) * ((1 - tx) * src[y0 * sx + x0] + tx * src[y0 * sx + x1]) +
                                  ty * ((1 - tx) * src[y1 * sx + x0] + tx * src[y1 * sx + x1]);
            }
        }
    };
    for (int k = 0; k < 2; k++) {
        resample(&v[6 + k * from->gy * from->gx], from->gy, from->gx, &out[6 + k * to.gy * to.gx], to.gy, to.gx);
    }
    resample(&v[6 + 2 * from->gy * from->gx], from->wy, from->wx, &out[6 + 2 * to.gy * to.gx], to.wy, to.wx);
    return out;
}

/// The finer grids, grown in steps from the warp's affine part.
inline std::pair<Model, std::vector<double>> FitGrids(const Letter &l, double s0, const std::vector<double> &warp) {
    auto fit = [](const Model &m, const std::vector<double> &v0, double reg, double wsm) {
        auto [lo, hi] = m.Bounds();
        std::vector<double> start = v0;
        for (std::size_t j = 0; j < start.size(); j++) {
            start[j] = std::clamp(start[j], lo[j] + 1e-9, hi[j] - 1e-9);
        }
        return LeastSquares([&](const std::vector<double> &v) { return m.Residuals(v, reg, wsm); }, start, lo, hi, 400);
    };
    Model base{&l, s0, 5, 4, 1, 1, false};
    auto  vb = fit(base, Lift(base, warp, nullptr), 0.06, 0.15);
    Model mid{&l, s0, 5, 4, 4, 3, false};
    auto  vm = fit(mid, Lift(mid, vb, &base), 0.06, 0.15);
    vm = fit(mid, vm, 0.06, 0.15);
    Model fine{&l, s0, 8, 6, 7, 5, false};
    auto  vf = fit(fine, Lift(fine, vm, &mid), 0.03, 0.08);
    return {fine, vf};
}

// ---------------------------------------------------------------------------------------------- source edits

/// Cuts a contour from point a to point b and closes the stroke with a half-round end bulging the way it was going.
inline Contour RoundEnd(const Contour &c, std::size_t a, std::size_t b, int steps = 9) {
    const std::vector<Pt> &P = c.pts;
    Pt                     pa = P[a], pb = P[b], mid = 0.5 * (pa + pb);
    double                 r = std::hypot(pa.x - pb.x, pa.y - pb.y) / 2;
    Pt                     away = mid - P[(a + P.size() - 20) % P.size()];
    double                 t0 = std::atan2(pa.y - mid.y, pa.x - mid.x), t1 = std::atan2(pb.y - mid.y, pb.x - mid.x);
    double                 heading = std::atan2(away.y, away.x);
    constexpr double       turn = 2 * std::numbers::pi;
    auto                   wrap = [](double t) { return std::fmod(std::fmod(t, turn) + turn, turn); };
    double                 sweep = wrap(t1 - t0);
    double                 h = wrap(heading - t0);
    if (!(0 < h && h < sweep)) {
        sweep -= turn;
    }
    Contour out;
    for (std::size_t i = 0; i <= a; i++) {
        out.pts.push_back(P[i]);
        out.on.push_back(c.on[i]);
    }
    for (int k = 1; k <= steps; k++) {
        double t = t0 + sweep * k / (steps + 1);
        out.pts.push_back({mid.x + r * std::cos(t), mid.y + r * std::sin(t)});
        out.on.push_back(true);
    }
    for (std::size_t i = b; i < P.size(); i++) {
        out.pts.push_back(P[i]);
        out.on.push_back(c.on[i]);
    }
    return out;
}

/// The font's contours for a letter, with the edits made where the font's design differs from the game's.
inline std::vector<Contour> SourceContours(const Ttf &font, char ch, bool bold) {
    std::vector<Contour> contours = font.Of(static_cast<unsigned char>(ch)).contours;
    // The game's bold 5 ends its tail going straight down, without the font's curl back to the left.
    if (bold && ch == '5' && contours.size() == 1 && contours[0].pts.size() == 589) {
        contours[0] = RoundEnd(contours[0], 196, 350);
    }
    return contours;
}

// ---------------------------------------------------------------------------------------------- the glyph sheet

/// How much of each texel of the glyph sheet a letter covers: alpha times brightness.
inline std::vector<double> SheetCoverage(const Picture &sheet) {
    std::vector<double> out(static_cast<std::size_t>(sheet.width) * sheet.height);
    for (std::size_t i = 0; i < out.size(); i++) {
        const std::uint8_t *c = &sheet.rgba[i * 4];
        out[i] = c[3] / 255.0 * (0.299 * c[0] + 0.587 * c[1] + 0.114 * c[2]) / 255.0;
    }
    return out;
}

inline std::vector<double> CellOf(const std::vector<double> &coverage, const Picture &sheet, std::size_t index, bool regular) {
    const int           u = 128 + kCellW * static_cast<int>(index % 9) + (regular ? 128 : 0);
    const int           v = kCellH * static_cast<int>(index / 9);
    std::vector<double> cell(kCellW * kCellH, 0.0);
    for (int y = 0; y < kCellH; y++) {
        for (int x = 0; x < kCellW; x++) {
            if (u + x < static_cast<int>(sheet.width) && v + y < static_cast<int>(sheet.height)) {
                cell[static_cast<std::size_t>(y) * kCellW + x] = coverage[static_cast<std::size_t>(v + y) * sheet.width + u + x];
            }
        }
    }
    return cell;
}

/// gaiji.img's picture in a language's message pack, if it has one.
inline std::optional<Picture> GlyphSheet(const fs::path &pack_file) {
    std::error_code error;
    if (!fs::is_regular_file(pack_file, error)) {
        return std::nullopt;
    }
    std::vector<unsigned char> pack = ReadFile(pack_file);
    for (const PackMember &member : ReadPack(pack)) {
        if (member.name != "gaiji.img") {
            continue;
        }
        std::optional<Picture> found;
        ScanBanks(member.data, [&](const BankEntry &entry) {
            if (!found) {
                std::vector<Picture> pictures = DecodeTim2(member.data, entry.tim2, entry.swizzled);
                if (!pictures.empty()) {
                    found = std::move(pictures[0]);
                }
            }
        });
        return found;
    }
    return std::nullopt;
}

/// A letter's final outline in cell px (contours with their on-curve flags), for both the font and the sheet.
struct Shaped {
    std::vector<Contour> contours;
    double               px_per_unit = 0; // the fit's vertical scale; 0 for a letter drawn as it is
};

/// Shapes the 88 letters of one set from the font and the sheet's cells.
inline std::vector<Shaped> ShapeSet(const Ttf &font, const std::vector<double> &coverage, const Picture &sheet, bool regular,
                                    std::FILE *log) {
    const bool          bold = !regular;
    std::vector<Shaped> out(kOrder.size());
    // The capital height the "as is" letters are drawn at: the font's H to the cell's H.
    const TtfGlyph &H = font.Of('H');
    double          h_lo = 1e9, h_hi = -1e9;
    for (const Contour &c : H.contours) {
        for (const Pt &p : c.pts) {
            h_lo = std::min(h_lo, p.y);
            h_hi = std::max(h_hi, p.y);
        }
    }
    const double top = bold ? 0.6 : 1.4, bottom = bold ? 18.5 : 15.4;
    const double per_px = (h_hi - h_lo) / (bottom - top), base = top + h_hi / per_px;

    std::mutex  lock;
    std::size_t next = 0;
    auto        work = [&] {
        for (;;) {
            std::size_t i;
            {
                std::lock_guard guard(lock);
                if (next >= kOrder.size()) {
                    return;
                }
                i = next++;
            }
            const char           ch = kOrder[i];
            std::vector<Contour> source = SourceContours(font, ch, bold);
            Shaped               shaped;
            if (PickOf(ch) == Fit::AsIs) {
                double x0 = 1e9, x1 = -1e9;
                for (const Contour &c : source) {
                    for (const Pt &p : c.pts) {
                        x0 = std::min(x0, p.x);
                        x1 = std::max(x1, p.x);
                    }
                }
                for (Contour c : source) {
                    for (Pt &p : c.pts) {
                        p = {(p.x - x0) / per_px + (kCellW - (x1 - x0) / per_px) / 2, base - p.y / per_px};
                    }
                    shaped.contours.push_back(std::move(c));
                }
            } else {
                Letter              letter = MakeLetter(source, CellOf(coverage, sheet, i, regular));
                double              ox, oy;
                double              s0 = StartScale(letter, ox, oy);
                Model               warp{&letter, s0, 5, 4, 1, 1, true};
                std::vector<double> vw = FitWarp(warp);
                Model               model = warp;
                std::vector<double> v = vw;
                if (PickOf(ch) == Fit::Grids) {
                    std::tie(model, v) = FitGrids(letter, s0, vw);
                }
                auto outline = model.Outline(v);
                for (std::size_t c = 0; c < outline.size(); c++) {
                    shaped.contours.push_back({outline[c], source[c].on});
                }
                shaped.px_per_unit = s0 * vw[1];
            }
            out[i] = std::move(shaped);
        }
    };
    std::vector<std::thread> threads;
    unsigned                 count = std::max(1u, std::min(16u, std::thread::hardware_concurrency()));
    for (unsigned t = 0; t < count; t++) {
        threads.emplace_back(work);
    }
    for (std::thread &t : threads) {
        t.join();
    }
    Log(log, "sharp: {} letters of the {} set shaped\n", out.size(), regular ? "regular" : "bold");
    return out;
}

/// The font made from a set: the letters at 50 units a cell pixel, the baseline at the cell's row 15.
inline std::vector<std::uint8_t> SetFont(const Ttf &font, const std::vector<Shaped> &set) {
    constexpr double                    kUnits = 50, kBase = 15;
    std::map<int, std::vector<Contour>> replaced;
    for (std::size_t i = 0; i < kOrder.size(); i++) {
        std::vector<Contour> units;
        for (Contour c : set[i].contours) {
            for (Pt &p : c.pts) {
                p = {p.x * kUnits, (kBase - p.y) * kUnits};
            }
            units.push_back(std::move(c));
        }
        replaced[font.cmap.at(static_cast<unsigned char>(kOrder[i]))] = std::move(units);
    }
    // Everything else (accents, other symbols) at the size the fitted letters usually came to.
    std::vector<double> scales;
    for (const Shaped &s : set) {
        if (s.px_per_unit > 0) {
            scales.push_back(s.px_per_unit);
        }
    }
    std::nth_element(scales.begin(), scales.begin() + static_cast<long>(scales.size() / 2), scales.end());
    const double rest = (scales.empty() ? 0.02 : scales[scales.size() / 2]) * kUnits;
    return WriteTtf(font, replaced, rest, 650, static_cast<int>(kBase * kUnits), static_cast<int>((kCellH - kBase) * kUnits));
}

/// The drop shadow the game's narrower letters carry in their own texels (the bold ones have none): the letter's
/// coverage moved down and right, blurred a little and thickened, at most half dark. Its shape was measured on the
/// game's 88 cells; `scale` is texels per game pixel.
inline std::vector<double> BakedShadow(const std::vector<double> &cov, int w, int h, int scale) {
    constexpr double kShiftX = 0.866, kShiftY = 0.841, kBlur = 0.741, kGain = 9.21, kMost = 0.512;
    const double     sigma = kBlur * scale;
    const int        reach = static_cast<int>(std::ceil(3 * sigma + std::max(kShiftX, kShiftY) * scale)) + 1;
    auto             kernel = [&](double shift) {
        std::vector<double> k(static_cast<std::size_t>(2 * reach + 1));
        double              sum = 0;
        for (int t = -reach; t <= reach; t++) {
            double v = std::exp(-(t - shift) * (t - shift) / (2 * sigma * sigma));
            k[static_cast<std::size_t>(t + reach)] = v;
            sum += v;
        }
        for (double &v : k) {
            v /= sum;
        }
        return k;
    };
    // A texel's shadow comes from the letter up and to the left of it: the kernels are centred that far back.
    const std::vector<double> kx = kernel(kShiftX * scale), ky = kernel(kShiftY * scale);
    std::vector<double>       across(cov.size(), 0.0), out(cov.size(), 0.0);
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            double s = 0;
            for (int t = -reach; t <= reach; t++) {
                int sx = x - t;
                if (sx >= 0 && sx < w) {
                    s += kx[static_cast<std::size_t>(t + reach)] * cov[static_cast<std::size_t>(y) * w + sx];
                }
            }
            across[static_cast<std::size_t>(y) * w + x] = s;
        }
    }
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            double s = 0;
            for (int t = -reach; t <= reach; t++) {
                int sy = y - t;
                if (sy >= 0 && sy < h) {
                    s += ky[static_cast<std::size_t>(t + reach)] * across[static_cast<std::size_t>(sy) * w + x];
                }
            }
            out[static_cast<std::size_t>(y) * w + x] = kMost * std::min(1.0, kGain * s);
        }
    }
    return out;
}

/// The glyph sheet at kSheetScale times: the letters of both sets drawn from their outlines, every other cell's
/// pixels scaled up as they are.
inline std::vector<std::uint8_t> SharpSheet(const Picture &sheet, const std::vector<Shaped> &bold,
                                            const std::vector<Shaped> &regular) {
    const int                 S = kSheetScale;
    const std::uint32_t       W = sheet.width * S, H = sheet.height * S;
    std::vector<std::uint8_t> rgba(static_cast<std::size_t>(W) * H * 4);
    for (std::uint32_t y = 0; y < H; y++) {
        for (std::uint32_t x = 0; x < W; x++) {
            std::memcpy(&rgba[(static_cast<std::size_t>(y) * W + x) * 4], &sheet.rgba[(static_cast<std::size_t>(y / S) * sheet.width + x / S) * 4], 4);
        }
    }
    for (int set = 0; set < 2; set++) {
        const std::vector<Shaped> &letters = set == 0 ? bold : regular;
        for (std::size_t i = 0; i < kOrder.size(); i++) {
            const int u = 128 + kCellW * static_cast<int>(i % 9) + (set == 1 ? 128 : 0), v = kCellH * static_cast<int>(i / 9);
            if (u + kCellW > static_cast<int>(sheet.width) || v + kCellH > static_cast<int>(sheet.height)) {
                continue;
            }
            // The cell's ink colour: its brightest-covering texel's, so drawn texels keep the game's colour and the
            // coverage is carried by alpha.
            std::array<std::uint8_t, 3> ink = {255, 255, 255};
            double                      best = -1;
            for (int y = 0; y < kCellH; y++) {
                for (int x = 0; x < kCellW; x++) {
                    const std::uint8_t *c = &sheet.rgba[(static_cast<std::size_t>(v + y) * sheet.width + u + x) * 4];
                    double              k = c[3] * (0.299 * c[0] + 0.587 * c[1] + 0.114 * c[2]);
                    if (k > best) {
                        best = k;
                        ink = {c[0], c[1], c[2]};
                    }
                }
            }
            const double                 lum = std::max((0.299 * ink[0] + 0.587 * ink[1] + 0.114 * ink[2]) / 255.0, 1e-3);
            std::vector<std::vector<Pt>> polys;
            for (const Contour &c : letters[i].contours) {
                polys.push_back(Flatten(c, 24));
            }
            std::vector<double> cov = Coverage(polys, kCellH * S, kCellW * S, 1.0 / S);
            std::vector<double> shadow = set == 1 ? BakedShadow(cov, kCellW * S, kCellH * S, S) : std::vector<double>(cov.size(), 0.0);
            for (int y = 0; y < kCellH * S; y++) {
                for (int x = 0; x < kCellW * S; x++) {
                    std::uint8_t *c = &rgba[(static_cast<std::size_t>(v * S + y) * W + u * S + x) * 4];
                    const std::size_t k = static_cast<std::size_t>(y) * kCellW * S + x;
                    // The letter over its shadow: the letter's ink colour, the shadow black.
                    const double letter = std::min(1.0, cov[k] / lum);
                    const double a = letter + (1 - letter) * shadow[k];
                    for (int ch = 0; ch < 3; ch++) {
                        c[ch] = static_cast<std::uint8_t>(a > 0 ? std::lround(ink[static_cast<std::size_t>(ch)] * letter / a) : ink[static_cast<std::size_t>(ch)]);
                    }
                    c[3] = static_cast<std::uint8_t>(std::lround(a * 255));
                }
            }
        }
    }
    return EncodePng(rgba.data(), W, H);
}

// ---------------------------------------------------------------------------------------------- build

/// The language folders and their message packs, in LanguageCode order.
inline constexpr std::array<std::pair<std::string_view, std::string_view>, 7> kLanguages = {
    {
     {"a_jpn", "meswin/mes_tex.pak"},
     {"a_usa", "meswin/mes_tex_1.pak"},
     {"a_eng", "meswin/mes_tex_2.pak"},
     {"a_fre", "meswin/mes_tex_3.pak"},
     {"a_ger", "meswin/mes_tex_4.pak"},
     {"a_ita", "meswin/mes_tex_5.pak"},
     {"a_spa", "meswin/mes_tex_6.pak"},
     }
};

/// Whether <data>/sharp is current.
inline bool IsBuilt(const fs::path &data) {
    std::error_code error;
    fs::path        version = data / "sharp" / "version.txt";
    if (!fs::is_regular_file(version, error)) {
        return false;
    }
    std::vector<unsigned char> text = ReadFile(version);
    return std::string(text.begin(), text.end()) == std::to_string(kVersion) + "\n";
}

/// Makes <data>/sharp from the extracted data and the community fonts in `fonts` (DarkCloudCompendium.ttf and
/// DarkCloudCompendiumThin.ttf, or font.ttf and font_thin.ttf as the port's lang folder names them). False, with the
/// reason logged, when something it needs is missing.
inline bool Build(const fs::path &data, const fs::path &fonts, std::FILE *log) {
    auto find = [&](std::string_view a, std::string_view b) {
        std::error_code error;
        for (std::string_view name : {a, b}) {
            if (fs::is_regular_file(fonts / std::string(name), error)) {
                return fonts / std::string(name);
            }
        }
        return fs::path{};
    };
    fs::path bold_file = find("DarkCloudCompendium.ttf", "font.ttf");
    fs::path thin_file = find("DarkCloudCompendiumThin.ttf", "font_thin.ttf");
    if (bold_file.empty() || thin_file.empty()) {
        Log(log, "sharp: no community fonts in {}; the Sharp text font is not made\n", fonts.string());
        return false;
    }
    // The letters are fitted to the first English sheet there is, then drawn into every language's.
    std::optional<Picture> reference;
    for (std::string_view pack : {"meswin/mes_tex_1.pak", "meswin/mes_tex_2.pak", "meswin/mes_tex.pak"}) {
        reference = GlyphSheet(data / std::string(pack));
        if (reference) {
            break;
        }
    }
    if (!reference) {
        Log(log, "sharp: no message pack with gaiji.img in {}; the Sharp text font is not made\n", data.string());
        return false;
    }
    Ttf                 bold_font = ReadTtf(ReadFile(bold_file));
    Ttf                 thin_font = ReadTtf(ReadFile(thin_file));
    std::vector<double> coverage = SheetCoverage(*reference);
    std::vector<Shaped> bold = ShapeSet(bold_font, coverage, *reference, false, log);
    std::vector<Shaped> regular = ShapeSet(thin_font, coverage, *reference, true, log);

    fs::path out = data / "sharp";
    WriteFile(out / "font.ttf", SetFont(bold_font, bold));
    WriteFile(out / "font_regular.ttf", SetFont(thin_font, regular));
    for (const auto &[folder, pack] : kLanguages) {
        std::optional<Picture> sheet = GlyphSheet(data / std::string(pack));
        if (!sheet || sheet->width != reference->width || sheet->height != reference->height) {
            continue;
        }
        // A sheet whose letters are not the reference's (the Japanese one has other cells there) keeps its own.
        std::vector<double> cov = SheetCoverage(*sheet);
        double              diff = 0;
        for (std::size_t i = 0; i < kOrder.size(); i++) {
            for (bool reg : {false, true}) {
                std::vector<double> a = CellOf(cov, *sheet, i, reg), b = CellOf(coverage, *reference, i, reg);
                for (std::size_t k = 0; k < a.size(); k++) {
                    diff += std::abs(a[k] - b[k]);
                }
            }
        }
        if (diff / (kOrder.size() * 2 * kCellW * kCellH) > 0.01) {
            Log(log, "sharp: {} has other letters; not redrawn\n", folder);
            continue;
        }
        WriteFile(out / std::string(folder) / "gaiji.png", SharpSheet(*sheet, bold, regular));
    }
    std::string version = std::to_string(kVersion) + "\n";
    WriteFile(out / "version.txt", std::span(reinterpret_cast<const unsigned char *>(version.data()), version.size()));
    Log(log, "sharp: text font made in {}\n", out.string());
    return true;
}

} // namespace dcdata::sharp
