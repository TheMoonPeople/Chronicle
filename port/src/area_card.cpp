// Parse JSON before including game headers, which define single-letter macros.
#include "area_card.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

#include "caption_text.hpp"
#include "gfx/gfx.hpp"
#include "localize.hpp"
#include "localize_texture.hpp"
#include "platform/png_image.hpp"
#include "texture_port.hpp"
#include "textwipe.hpp"

namespace areacard {

namespace {

struct Entry {
    const char *card;
    const char *english;
};

constexpr Entry kCards[] = {
    {"mt01",  "NORUNE VILLAGE"    },
    {"mt02",  "MATATAKI VILLAGE"  },
    {"mt03",  "QUEENS"            },
    {"mt04",  "MUSKA RACKA"       },
    {"mt05",  "MOON FACTORY"      },
    {"mt101", "DIVINE BEAST CAVE" },
    {"mt102", "WISE OWL FOREST"   },
    {"mt103", "SHIPWRECK"         },
    {"mt104", "SUN&MOON TEMPLE"   },
    {"mt105", "MOON SEA"          },
    {"mt106", "GALLERY OF TIME"   },
    {"mt107", "DEMON SHAFT"       },
    {"mt14",  "BROWNBOO VILLAGE"  },
    {"mt23",  "YELLOW DROPS"      },
    {"mt301", "DARK SHRINE"       },
    {"mt40",  "DARK HEAVEN CASTLE"},
};

struct Layout {
    std::string text;
    float       cap_top = 0.27f;
    float       cap_height = 0.42f;
    float       xsq = 0.56f;
    float       max_width = 0.9375f;
    bool        underline = true;
    float       underline_y = 0.7656f;
    float       underline_thick = 0.0547f;
    float       underline_pad_left = 0.0117f;
    float       underline_pad_right = 0.0169f;
    float       underline_bow = 0.0078f;
    float       underline_wave = 0.0420f; // height of the tilde in the ribbon, of the card's height
    bool        drips = false;

    // Measured ink bounds exclude the underline and use fractions of the card.
    struct Ink {
        float                             left, right, top, bottom;
        std::vector<std::array<float, 2>> slots;
        caption::FillField                field;            // the painted lettering's colours spread outward, for the texture of the fill
        bool                              has_ring = false; // the colours of the dark outline and the glow just outside the painted letters
        unsigned char                     outline[3] = {0, 0, 0}, glow[3] = {0, 0, 0};
        bool                              has_rule = false; // and the brush underneath it, where the card has one
        float                             rule_y = 0.0f, rule_thick = 0.0f, rule_left = 0.0f, rule_right = 0.0f;
    };

    std::optional<Ink> ink;
    unsigned           field_id = 0;
    float              underline_y0 = 0.7656f; // the plate json's, before the fit moves it
    caption::LineStyle style;
    float              edge_width = 0.0039f; // of the card's width
    float              glow_width = 0.013f;
};

std::optional<Layout> g_card;

std::optional<Layout::Ink> MeasureInk(const PortDecodedTexture                &card, const unsigned char (&fill)[3],
                                      const std::vector<caption::LetterWidth> &letters) {
    const int width = static_cast<int>(card.width);
    const int height = static_cast<int>(card.height);
    if (card.levels.empty() || width < 8 || height < 8 ||
        card.levels[0].size() < static_cast<size_t>(width) * height * (card.format == gfx::TextureFormat::Index8 ? 1 : 4)) {
        return std::nullopt;
    }
    std::vector<uint8_t> mask(static_cast<size_t>(width) * height, 0);
    std::vector<int>     rows(static_cast<size_t>(height), 0);
    std::vector<uint8_t> rule_row(static_cast<size_t>(height), 0);
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            const size_t i = static_cast<size_t>(y) * width + x;
            int          rgb[3];
            if (card.format == gfx::TextureFormat::Index8) {
                const uint32_t colour = card.palette[card.levels[0][i]];
                rgb[0] = static_cast<int>(colour & 0xFF);
                rgb[1] = static_cast<int>((colour >> 8) & 0xFF);
                rgb[2] = static_cast<int>((colour >> 16) & 0xFF);
            } else {
                rgb[0] = card.levels[0][i * 4];
                rgb[1] = card.levels[0][i * 4 + 1];
                rgb[2] = card.levels[0][i * 4 + 2];
            }

            const float lum = static_cast<float>(rgb[0] + rgb[1] + rgb[2]) / 3.0f;
            const float fill_lum = static_cast<float>(fill[0] + fill[1] + fill[2]) / 3.0f;
            const float tr = (static_cast<float>(rgb[0]) - lum) - (static_cast<float>(fill[0]) - fill_lum);
            const float tg = (static_cast<float>(rgb[1]) - lum) - (static_cast<float>(fill[1]) - fill_lum);
            const float tb = (static_cast<float>(rgb[2]) - lum) - (static_cast<float>(fill[2]) - fill_lum);
            if (lum >= fill_lum * 0.82f && tr * tr + tg * tg + tb * tb <= 60.0f * 60.0f) {
                mask[i] = 1;
                rows[static_cast<size_t>(y)]++;
            }
        }
    }
    // Long horizontal runs belong to the underline rather than the letters.
    for (int y = 0; y < height; y++) {
        int longest = 0, run = 0;
        for (int x = 0; x < width; x++) {
            run = mask[static_cast<size_t>(y) * width + x] ? run + 1 : 0;
            longest = std::max(longest, run);
        }
        if (longest >= std::max(16, width / 4)) {
            rule_row[static_cast<size_t>(y)] = 1;
            rows[static_cast<size_t>(y)] = 0;
        }
    }

    const int least = std::max(6, height / 12);
    int       top = -1, bottom = -1;
    for (int y = 0; y < height && top < 0;) {
        if (rows[static_cast<size_t>(y)] < 2) {
            y++;
            continue;
        }
        int end = y;
        for (int gap = 0, k = y; k < height && gap <= 1; k++) {
            if (rows[static_cast<size_t>(k)] >= 2) {
                end = k;
                gap = 0;
            } else {
                gap++;
            }
        }
        if (end - y + 1 >= least) {
            top = y;
            bottom = end;
        }
        y = end + 1;
    }
    if (top < 0) {
        return std::nullopt;
    }
    int left = width, right = -1;
    for (int y = top; y <= bottom; y++) {
        for (int x = 0; x < width; x++) {
            if (mask[static_cast<size_t>(y) * width + x]) {
                left = std::min(left, x);
                right = std::max(right, x);
            }
        }
    }
    if (right < left || right - left + 1 < width / 8) {
        return std::nullopt;
    }
    Layout::Ink ink = {static_cast<float>(left) / width, static_cast<float>(right + 1) / width,
                       static_cast<float>(top) / height, static_cast<float>(bottom + 1) / height};
    // Touching painted letters require sheared-column cuts near the font's estimated boundaries.
    {
        constexpr float  kLean = -0.1f; // the painted lean, the same on every card (x = x' + kLean * (y - mid))
        const int        rows_n = bottom - top + 1;
        const int        pad = rows_n;
        std::vector<int> cols(static_cast<size_t>(width + 2 * pad), 0);
        for (int y = top; y <= bottom; y++) {
            for (int x = 0; x < width; x++) {
                if (mask[static_cast<size_t>(y) * width + x]) {
                    const int sheared = static_cast<int>(std::lround(static_cast<float>(x) - kLean * (static_cast<float>(y - top) - rows_n / 2.0f))) + pad;
                    if (sheared >= 0 && sheared < static_cast<int>(cols.size())) {
                        cols[static_cast<size_t>(sheared)]++;
                    }
                }
            }
        }
        int first = -1, last = -1;
        for (int x = 0; x < static_cast<int>(cols.size()); x++) {
            if (cols[static_cast<size_t>(x)] > 0) {
                first = first < 0 ? x : first;
                last = x;
            }
        }
        float  total = 0.0f;
        size_t drawn = 0;
        for (const caption::LetterWidth &item : letters) {
            total += item.width;
            drawn += item.blank ? 0 : 1;
        }
        if (first >= 0 && total > 0.0f && drawn >= 2) {
            const float      span = static_cast<float>(last - first + 1);
            std::vector<int> bound(letters.size() + 1, 0);
            bound[0] = first - 1;
            bound[letters.size()] = last + 1;
            float walked = 0.0f;
            for (size_t i = 0; i + 1 < letters.size(); i++) {
                walked += letters[i].width;
                const float expect = static_cast<float>(first) + span * walked / total;
                const float reach = 0.4f * std::min(letters[i].width, letters[i + 1].width) / total * span;
                int         best = static_cast<int>(std::lround(expect));
                int         best_count = 1 << 30;
                for (int x = static_cast<int>(std::floor(expect - reach)); x <= static_cast<int>(std::ceil(expect + reach)); x++) {
                    if (x < 0 || x >= static_cast<int>(cols.size())) {
                        continue;
                    }
                    const int count = cols[static_cast<size_t>(x)];
                    if (count < best_count || (count == best_count && std::fabs(static_cast<float>(x) - expect) < std::fabs(static_cast<float>(best) - expect))) {
                        best = x;
                        best_count = count;
                    }
                }
                bound[i + 1] = std::max(best, bound[i] + 1);
            }

            std::vector<int> lo(letters.size(), 1 << 30), hi(letters.size(), -1);
            for (int y = top; y <= bottom; y++) {
                for (int x = 0; x < width; x++) {
                    if (!mask[static_cast<size_t>(y) * width + x]) {
                        continue;
                    }
                    const int sheared = static_cast<int>(std::lround(static_cast<float>(x) - kLean * (static_cast<float>(y - top) - rows_n / 2.0f))) + pad;
                    size_t    item = 0;
                    while (item + 1 < letters.size() && sheared > bound[item + 1]) {
                        item++;
                    }
                    lo[item] = std::min(lo[item], x);
                    hi[item] = std::max(hi[item], x);
                }
            }
            std::vector<std::array<float, 2>> slots;
            for (size_t i = 0; i < letters.size(); i++) {
                if (letters[i].blank) {
                    continue;
                }
                if (hi[i] < 0) {
                    slots.clear();
                    break;
                }
                slots.push_back({static_cast<float>(lo[i]) / width, static_cast<float>(hi[i] + 1) / width});
            }
            if (slots.size() == drawn) {
                ink.slots = std::move(slots);
            }
        }
    }
    // Extend each painted ink color to its nearest uncovered texels before sampling runtime glyphs.
    {
        auto colour_at = [&](size_t i, int rgb[3]) {
            if (card.format == gfx::TextureFormat::Index8) {
                const uint32_t colour = card.palette[card.levels[0][i]];
                rgb[0] = static_cast<int>(colour & 0xFF);
                rgb[1] = static_cast<int>((colour >> 8) & 0xFF);
                rgb[2] = static_cast<int>((colour >> 16) & 0xFF);
            } else {
                rgb[0] = card.levels[0][i * 4];
                rgb[1] = card.levels[0][i * 4 + 1];
                rgb[2] = card.levels[0][i * 4 + 2];
            }
        };
        const size_t        count = static_cast<size_t>(width) * height;
        std::vector<int>    nearest(count, -1);
        std::vector<int>    distance(count, 0);
        std::vector<size_t> queue;
        for (int y = top; y <= bottom; y++) {
            for (int x = 0; x < width; x++) {
                const size_t i = static_cast<size_t>(y) * width + x;
                if (mask[i]) {
                    nearest[i] = static_cast<int>(i);
                    queue.push_back(i);
                }
            }
        }
        for (size_t head = 0; head < queue.size(); head++) {
            const size_t i = queue[head];
            const int    x = static_cast<int>(i % width), y = static_cast<int>(i / width);
            const int    dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
            for (int k = 0; k < 4; k++) {
                const int nx = x + dx[k], ny = y + dy[k];
                if (nx < 0 || ny < 0 || nx >= width || ny >= height || ny < top - 12 || ny > bottom + 12) {
                    continue;
                }
                const size_t n = static_cast<size_t>(ny) * width + nx;
                if (nearest[n] < 0) {
                    nearest[n] = nearest[i];
                    distance[n] = distance[i] + 1;
                    queue.push_back(n);
                }
            }
        }
        ink.field.width = width;
        ink.field.height = height;
        ink.field.rgb.assign(count * 3, 0);
        long ring[2][4] = {
            {0, 0, 0, 0},
            {0, 0, 0, 0}
        };
        for (size_t i = 0; i < count; i++) {
            int rgb[3] = {fill[0], fill[1], fill[2]};
            if (nearest[i] >= 0) {
                colour_at(static_cast<size_t>(nearest[i]), rgb);
            }
            for (int c = 0; c < 3; c++) {
                ink.field.rgb[i * 3 + c] = static_cast<unsigned char>(rgb[c]);
            }
            const int x = static_cast<int>(i % width), y = static_cast<int>(i / width);
            if (nearest[i] >= 0 && !mask[i] && y >= top && y <= bottom && x >= left && x <= right) {
                const int which = distance[i] >= 1 && distance[i] <= 2 ? 0 : distance[i] >= 3 && distance[i] <= 5 ? 1
                                                                                                                  : -1;
                if (which >= 0) {
                    int own[3];
                    colour_at(i, own);
                    for (int c = 0; c < 3; c++) {
                        ring[which][c] += own[c];
                    }
                    ring[which][3]++;
                }
            }
        }
        if (ring[0][3] > 8 && ring[1][3] > 8) {
            ink.has_ring = true;
            for (int c = 0; c < 3; c++) {
                ink.outline[c] = static_cast<unsigned char>(ring[0][c] / ring[0][3]);
                ink.glow[c] = static_cast<unsigned char>(ring[1][c] / ring[1][3]);
            }
        }
    }

    int rule_top = -1, rule_bottom = -1;
    for (int y = 0; y < height; y++) {
        if (rule_row[static_cast<size_t>(y)]) {
            if (rule_top < 0) {
                rule_top = y;
            }
            rule_bottom = y;
        }
    }
    if (rule_top >= 0) {
        int rule_left = width, rule_right = -1;
        for (int y = rule_top; y <= rule_bottom; y++) {
            for (int x = 0; x < width; x++) {
                if (mask[static_cast<size_t>(y) * width + x]) {
                    rule_left = std::min(rule_left, x);
                    rule_right = std::max(rule_right, x);
                }
            }
        }
        if (rule_right - rule_left + 1 >= width / 4) {
            ink.has_rule = true;
            ink.rule_y = (static_cast<float>(rule_top + rule_bottom + 1) / 2) / height;
            ink.rule_thick = static_cast<float>(rule_bottom - rule_top + 1) / height;
            ink.rule_left = static_cast<float>(rule_left) / width;
            ink.rule_right = static_cast<float>(rule_right + 1) / width;
        }
    }
    return ink;
}

bool Colour(const nlohmann::json &json, const char *key, unsigned char (&out)[3]) {
    if (!json.contains(key) || !json[key].is_array() || json[key].size() != 3) {
        return false;
    }
    for (int i = 0; i < 3; i++) {
        out[i] = static_cast<unsigned char>(std::clamp(json[key][i].get<int>(), 0, 255));
    }
    return true;
}

float Number(const nlohmann::json &json, const char *key, float fallback) {
    return json.contains(key) && json[key].is_number() ? json[key].get<float>() : fallback;
}

// Card fractions are transformed by the actual loading-sprite rectangle.
struct Frame {
    float x0, y0, w, h;

    float X(float fx) const { return x0 + fx * w; }

    float Y(float fy) const { return y0 + fy * h; }
};

// A tapered sine-wave ribbon preserves the painted underline's curved silhouette.
void Ribbon(std::vector<gfx::Vertex2D> &out, const Frame &frame, const Layout &layout, float left, float right, float grow,
            float feather, const unsigned char (&top_rgb)[3], const unsigned char (&mid_rgb)[3], const unsigned char (&low_rgb)[3],
            float fade) {
    constexpr int   kSteps = 96;
    constexpr float kPi = 3.14159265f;
    const float     k = std::clamp(fade, 0.0f, 1.0f);
    const float     aspect = frame.w / frame.h;
    const float     amplitude = std::max(layout.underline_wave, 0.0f);
    auto            smooth = [](float x) { x = std::clamp(x, 0.0f, 1.0f); return x * x * (3 - 2 * x); };
    auto            shade = [&](const unsigned char (&rgb)[3], float alpha) {
        return std::array<uint8_t, 4>{rgb[0], rgb[1], rgb[2], static_cast<uint8_t>(std::clamp(alpha * k, 0.0f, 1.0f) * 0x80 + 0.5f)};
    };
    auto point = [&](int i, float at, const unsigned char (&rgb)[3], float alpha, float extra) {
        const float t = static_cast<float>(i) / kSteps;
        const float x = left - grow + (right - left + 2 * grow) * t;
        const float taper = smooth(t / 0.16f) * smooth((1 - t) / 0.16f);
        const float body = 0.62f + 0.38f * std::sin(kPi * t);
        const float half = layout.underline_thick * 0.5f * body * taper;
        const float cy = layout.underline_y - amplitude * std::sin(2 * kPi * t - 0.35f);
        const float y = cy + at * (half + (grow + extra) * aspect);
        const auto  c = shade(rgb, alpha);
        return gfx::Vertex2D{
            frame.X(x), frame.Y(y), 0.0f, 0.0f, 0.0f, {c[0], c[1], c[2], c[3]}
        };
    };
    auto band = [&](float at0, const unsigned char (&rgb0)[3], float a0, float e0, float at1, const unsigned char (&rgb1)[3], float a1,
                    float e1) {
        for (int i = 0; i < kSteps; i++) {
            out.push_back(point(i, at0, rgb0, a0, e0));
            out.push_back(point(i + 1, at0, rgb0, a0, e0));
            out.push_back(point(i + 1, at1, rgb1, a1, e1));
            out.push_back(point(i, at1, rgb1, a1, e1));
        }
    };
    if (feather > 0.0f) {

        band(-1.0f, top_rgb, 0.0f, feather, -1.0f, top_rgb, 1.0f, 0.0f);
        band(1.0f, low_rgb, 1.0f, 0.0f, 1.0f, low_rgb, 0.0f, feather);
    }

    band(-1.0f, top_rgb, 1.0f, 0.0f, -0.15f, mid_rgb, 1.0f, 0.0f);
    band(-0.15f, mid_rgb, 1.0f, 0.0f, 1.0f, low_rgb, 1.0f, 0.0f);
}

void Mix(unsigned char (&out)[3], const unsigned char (&a)[3], const unsigned char (&b)[3], float t) {
    for (int i = 0; i < 3; i++) {
        out[i] = static_cast<unsigned char>(a[i] + (b[i] - a[i]) * t + 0.5f);
    }
}

} // namespace

std::vector<std::pair<std::string, std::string>> Strings() {
    std::vector<std::pair<std::string, std::string>> out;
    for (const Entry &entry : kCards) {
        out.emplace_back(std::string("area.") + entry.card, entry.english);
    }
    return out;
}

void Clear() {
    g_card.reset();
}

bool Prepare(const std::string &name, PortDecodedTexture &texture) {
    g_card.reset();
    const Entry *entry = nullptr;
    for (const Entry &candidate : kCards) {
        if (name == candidate.card) {
            entry = &candidate;
        }
    }
    if (entry == nullptr || !caption::Ready() || texture.width == 0 || texture.height == 0) {
        return false;
    }
    const std::filesystem::path plate = LocalizeFindFile("textures/plates/" + name + ".png");
    const std::filesystem::path sheet = LocalizeFindFile("textures/plates/" + name + ".json");
    if (plate.empty() || sheet.empty()) {
        return false;
    }
    Layout layout;
    try {
        std::ifstream         stream(sheet);
        const nlohmann::json  json = nlohmann::json::parse(stream);
        const nlohmann::json &lay = json.contains("layout") ? json["layout"] : json;
        layout.cap_top = Number(lay, "cap_top", layout.cap_top);
        layout.cap_height = Number(lay, "cap_height", layout.cap_height);
        layout.xsq = Number(lay, "xsq", layout.xsq);
        layout.max_width = Number(lay, "max_width", layout.max_width);
        layout.underline = Number(lay, "underline", 1.0f) != 0.0f;
        layout.underline_y = Number(lay, "underline_y", layout.underline_y);
        layout.underline_thick = Number(lay, "underline_thick", layout.underline_thick);
        layout.underline_pad_left = Number(lay, "underline_pad_left", layout.underline_pad_left);
        layout.underline_pad_right = Number(lay, "underline_pad_right", layout.underline_pad_right);
        layout.underline_bow = Number(lay, "underline_bow", layout.underline_bow);
        layout.underline_wave = Number(lay, "underline_wave", layout.underline_wave);
        const nlohmann::json &look = json.contains("style") ? json["style"] : json;
        Colour(look, "fill", layout.style.fill);
        Colour(look, "edge", layout.style.edge);
        Colour(look, "glow", layout.style.glow);
        layout.style.glow_amount = Number(look, "glow_amount", 0.0f);
        layout.edge_width = Number(look, "edge_width", layout.edge_width);
        layout.glow_width = Number(look, "glow_width", layout.glow_width);
        layout.drips = look.contains("drips") && look["drips"].is_number() && look["drips"].get<int>() != 0;
    } catch (const std::exception &error) {
        std::fprintf(stderr, "areacard: cannot read %s: %s\n", sheet.string().c_str(), error.what());
        return false;
    }

    int                  width = 0;
    int                  height = 0;
    std::vector<uint8_t> rgba;
    if (!PngLoadRgba(plate, width, height, rgba)) {
        std::fprintf(stderr, "areacard: cannot read picture %s\n", plate.string().c_str());
        return false;
    }
    layout.underline_y0 = layout.underline_y;
    const std::string text = LocalizeText(std::string("area.") + name, entry->english);
    // Per-letter slots are valid only when runtime and painted strings correspond.
    const bool match_letters = LocalizeLanguageFile() == "en_us" && text == entry->english && name != "mt14";
    layout.ink = MeasureInk(texture, layout.style.fill,
                            match_letters ? caption::LetterWidths(text) : std::vector<caption::LetterWidth>{});
    if (layout.ink) {
        std::fprintf(stderr, "areacard: %s measured lettering %.3f..%.3f x %.3f..%.3f, rule %d at %.3f, %zu letter slots\n", name.c_str(), layout.ink->left, layout.ink->right, layout.ink->top, layout.ink->bottom, layout.ink->has_rule ? 1 : 0, layout.ink->rule_y, layout.ink->slots.size());
    } else {
        std::fprintf(stderr, "areacard: %s: no lettering measured\n", name.c_str());
    }
    std::vector<std::vector<uint8_t>> levels;
    for (size_t level = 0; level < std::max<size_t>(1, texture.levels.size()); level++) {
        const int w = std::max<int>(1, static_cast<int>(texture.width >> level));
        const int h = std::max<int>(1, static_cast<int>(texture.height >> level));
        levels.push_back(LocalizeResample(rgba.data(), width, height, w, h));
    }
    texture.levels = std::move(levels);
    texture.format = gfx::TextureFormat::Rgba8;
    texture.has_alpha = false;
    texture.four_bit = false;
    layout.text = text;
    g_card = std::move(layout);
    return true;
}

bool Prepared() {
    return g_card.has_value();
}

void Draw(float x0, float y0, float x1, float y1, float fade) {
    if (!g_card || fade <= 0.0f) {
        return;
    }
    Layout            &layout = *g_card;
    const Frame        frame = {x0, y0, x1 - x0, y1 - y0};
    caption::LineStyle style = layout.style;
    style.edge_width = layout.edge_width * frame.w;
    style.glow_width = layout.glow_width * frame.w;

    const textwipe::Tuning &tune = textwipe::Get(textwipe::Kind::Card);
    style.letter = tune.letter;
    style.word = tune.word;
    style.xscale = tune.xscale;
    style.yscale = tune.yscale;
    style.dx = tune.dx;
    style.dy = tune.dy;
    style.weight = tune.weight;
    style.opacity = tune.opacity;

    float center_x = frame.X(0.5f);
    float cap_top = frame.Y(layout.cap_top);
    float cap_height = layout.cap_height * frame.h * tune.size;
    float xsq = layout.xsq;
    float max_width = layout.max_width * frame.w;
    if (layout.ink && tune.fit >= 0.5f) {
        const Layout::Ink &ink = *layout.ink;
        center_x = frame.X((ink.left + ink.right) / 2);
        cap_top = frame.Y(ink.top);
        cap_height = (ink.bottom - ink.top) * frame.h * tune.size;
        xsq = 10.0f; // wider than any name, so the box's width is what shapes it
        style.ink_fit = true;
        style.thin = caption::ThinReady();

        static unsigned next_field = 0;
        if (layout.field_id == 0) {
            layout.field_id = ++next_field;
            layout.ink->field.id = layout.field_id;
        }
        style.field = &layout.ink->field;
        style.field_x = frame.x0;
        style.field_y = frame.y0;
        style.field_w = frame.w;
        style.field_h = frame.h;
        if (layout.ink->has_ring) {
            for (int c = 0; c < 3; c++) {
                style.edge[c] = layout.ink->outline[c];
                style.glow[c] = layout.ink->glow[c];
            }
        }
        if (!ink.slots.empty()) {

            const float middle = (ink.left + ink.right) / 2;
            for (const std::array<float, 2> &slot : ink.slots) {
                style.slots.push_back({frame.X(middle + (slot[0] - middle) * tune.size) + tune.dx,
                                       frame.X(middle + (slot[1] - middle) * tune.size) + tune.dx});
            }
        }
        max_width = (ink.right - ink.left) * frame.w * tune.size;
    }

    layout.underline_y = layout.underline_y0 + tune.dy / frame.h;
    if (layout.ink && layout.ink->has_rule && tune.fit >= 0.5f) {
        layout.underline_y = layout.ink->rule_y + tune.dy / frame.h;
        layout.underline_thick = layout.ink->rule_thick;
    }
    caption::LineBounds span;
    if (!caption::DrawLine(layout.text, center_x, cap_top, cap_height, xsq, max_width, style,
                           fade, 1.0f, &span)) {
        return;
    }
    float left = (span.left - frame.x0) / frame.w - layout.underline_pad_left;
    float right = (span.right - frame.x0) / frame.w + layout.underline_pad_right;
    if (layout.ink && layout.ink->has_rule && tune.fit >= 0.5f) {
        left = layout.ink->rule_left;
        right = layout.ink->rule_right;
    }
    const float                grow = layout.edge_width;
    std::vector<gfx::Vertex2D> outline, body;
    if (layout.underline) {

        const float   feather = std::max(frame.w * 0.0022f, 0.9f) / frame.w;
        unsigned char light[3], dark[3], white[3] = {255, 255, 255}, black[3] = {0, 0, 0};
        Mix(light, style.fill, white, 0.45f);
        Mix(dark, style.fill, black, 0.30f);
        Ribbon(outline, frame, layout, left, right, grow, feather, style.edge, style.edge, style.edge, fade);
        Ribbon(body, frame, layout, left, right, 0.0f, 0.0f, light, style.fill, dark, fade);
    }
    if (layout.drips) {

        const float k = std::clamp(fade, 0.0f, 1.0f);
        for (int pass = 0; pass < 2; pass++) {
            const unsigned char (&rgb)[3] = pass == 0 ? style.edge : style.fill;
            const float                 fat = pass == 0 ? grow : 0.0f;
            const u_char                r = static_cast<u_char>(rgb[0] * k), g = static_cast<u_char>(rgb[1] * k), b = static_cast<u_char>(rgb[2] * k);
            std::vector<gfx::Vertex2D> &out = pass == 0 ? outline : body;
            for (int drop = 0; drop < 7; drop++) {
                const float x = (span.left - frame.x0) / frame.w + (static_cast<float>(drop) + 0.5f) * (span.right - span.left) / frame.w / 7;
                const float y = layout.underline_y + 0.0195f;
                const float length = 0.0898f * (0.7f + 0.3f * static_cast<float>((drop * 7) % 3) / 2);
                const float half = 0.0059f + fat;
                auto        vertex = [&](float px, float py) { return gfx::Vertex2D{
                                                                   frame.X(px), frame.Y(py), 0.0f, 0.0f, 0.0f, {r, g, b, 0x80}
 }; };
                out.push_back(vertex(x - half, y - fat));
                out.push_back(vertex(x + half, y - fat));
                out.push_back(vertex(x, y + length + fat));
                out.push_back(vertex(x, y + length + fat));
            }
        }
    }
    gfx::DrawState blended;
    blended.blend = true;
    textwipe::Draw2D(gfx::Primitive::Quads, outline, {}, blended);
    textwipe::Draw2D(gfx::Primitive::Quads, body, {}, blended);
}

} // namespace areacard
