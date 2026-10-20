#pragma once

// The Sharp text font's glyph sheet: the game's message glyph sheet (meswin gaiji.img) at kSheetScale times, its 88
// letters drawn from Chronicle Sharp (tools/font/ChronicleSharp-*.ttf: the bold font for the game's bold dialogue set,
// the regular one for its narrower set) and every other cell scaled up as it is. The fonts lay each letter out on the
// sheet's own cell, 50 units a cell pixel with the baseline at its row 15. The sheet is the disc's, so this runs when
// the data is extracted and nothing it makes is distributed. Header-only like the rest of dcdata.
//
// Written under <data>/sharp/:
//
//     <language>/gaiji.png        the language's glyph sheet
//     version.txt                 kVersion, so a changed recipe makes them again
//     version.txt                 kVersion, so a changed recipe makes them again

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "dcdata.hpp"
#include "textures.hpp"

namespace dcdata::sharp {

namespace fs = std::filesystem;

/// Bumped whenever what Build makes changes, so it is made again.
inline constexpr int kVersion = 4;

/// How many times the cells' resolution the glyph sheet is drawn at.
inline constexpr int kSheetScale = 6;

/// The 88 letters of gaiji.img, in its order: nine 14x20 cells a row from u = 128 (bold) and u = 256 (regular).
inline constexpr std::string_view kOrder =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz'=\"!?#&+-*/%()@|<>{}[]:,.$0123456789";
inline constexpr int kCellW = 14;
inline constexpr int kCellH = 20;

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

/// A letter as the sheet draws it: its contours in cell px.
struct Shaped {
    std::vector<Contour> contours;
};

/// The 88 letters of one set from its font, in cell px.
inline std::vector<Shaped> FontSet(const Ttf &font) {
    constexpr double    kUnits = 50, kBase = 15;
    std::vector<Shaped> out(kOrder.size());
    for (std::size_t i = 0; i < kOrder.size(); i++) {
        for (Contour c : font.Of(static_cast<unsigned char>(kOrder[i])).contours) {
            for (Pt &p : c.pts) {
                p = {p.x / kUnits, kBase - p.y / kUnits};
            }
            out[i].contours.push_back(std::move(c));
        }
    }
    return out;
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

/// Makes <data>/sharp from the extracted data and Chronicle Sharp in `fonts` (ChronicleSharp-Bold.ttf and
/// ChronicleSharp-Regular.ttf, or font.ttf and font_thin.ttf as the port's lang folder names them). False, with the
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
    fs::path bold_file = find("ChronicleSharp-Bold.ttf", "font.ttf");
    fs::path regular_file = find("ChronicleSharp-Regular.ttf", "font_thin.ttf");
    if (bold_file.empty() || regular_file.empty()) {
        Log(log, "sharp: no Chronicle Sharp fonts in {}; the Sharp glyph sheet is not made\n", fonts.string());
        return false;
    }
    // The letters are drawn into every language's sheet whose letters are the English one's.
    std::optional<Picture> reference;
    for (std::string_view pack : {"meswin/mes_tex_1.pak", "meswin/mes_tex_2.pak", "meswin/mes_tex.pak"}) {
        reference = GlyphSheet(data / std::string(pack));
        if (reference) {
            break;
        }
    }
    if (!reference) {
        Log(log, "sharp: no message pack with gaiji.img in {}; the Sharp glyph sheet is not made\n", data.string());
        return false;
    }
    const std::vector<Shaped> bold = FontSet(ReadTtf(ReadFile(bold_file)));
    const std::vector<Shaped> regular = FontSet(ReadTtf(ReadFile(regular_file)));
    const std::vector<double> coverage = SheetCoverage(*reference);

    fs::path out = data / "sharp";
    // The folder holds no fonts: the port ships its own.
    std::error_code error;
    fs::remove(out / "font.ttf", error);
    fs::remove(out / "font_regular.ttf", error);
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
    Log(log, "sharp: glyph sheets made in {}\n", out.string());
    return true;
}

} // namespace dcdata::sharp
