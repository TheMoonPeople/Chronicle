#include "localize_texture.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>
#include <optional>
#include <string>

#include "localize.hpp"
#include "platform/config.hpp"
#include "platform/png_image.hpp"
#include "texture_port.hpp"

namespace fs = std::filesystem;

namespace {

// Pictures already complained about, so a wrong-shaped one says so once.
std::map<std::string, bool> &Reported() {
    static std::map<std::string, bool> reported;
    return reported;
}

// Whether a picture of this shape belongs to a texture of that one (a card is not stretched into a menu sheet).
bool SameShape(int source_width, int source_height, unsigned width, unsigned height) {
    double a = static_cast<double>(source_width) / std::max(1, source_height);
    double b = static_cast<double>(width) / std::max<unsigned>(1, height);
    return std::fabs(a - b) <= 0.02 * b;
}

// Which kind of text a picture is, so its shadow follows that kind's Options slider, and how far (in the texture's
// own pixels, at 50%) the shadow falls down and to the right: about a tenth of the letters' height.
struct Kind {
    int Config::*percent;
    float        scale;
};

std::optional<Kind> KindOf(std::string_view name) {
    auto digits = [&](size_t from) {
        return name.size() > from && std::all_of(name.begin() + from, name.end(), [](char c) { return c >= '0' && c <= '9'; });
    };
    if (name.starts_with("mt") && digits(2)) {
        return Kind{&Config::name_shadow, 5.6f};
    }
    if (name.starts_with("floor") && digits(5)) {
        return Kind{&Config::floor_shadow, 3.1f};
    }
    if (name.starts_with("boss_") && digits(5)) {
        return Kind{&Config::boss_shadow, 3.6f};
    }
    return std::nullopt;
}

float Sample(const std::vector<uint8_t> &plane, int width, int height, float x, float y) {
    x -= 0.5f;
    y -= 0.5f;
    int   x0 = static_cast<int>(std::floor(x));
    int   y0 = static_cast<int>(std::floor(y));
    float fx = x - static_cast<float>(x0);
    float fy = y - static_cast<float>(y0);
    auto  at = [&](int px, int py) {
        return px < 0 || py < 0 || px >= width || py >= height ? 0.0f : plane[static_cast<size_t>(py) * width + px] / 255.0f;
    };
    return (at(x0, y0) * (1 - fx) + at(x0 + 1, y0) * fx) * (1 - fy) + (at(x0, y0 + 1) * (1 - fx) + at(x0 + 1, y0 + 1) * fx) * fy;
}

} // namespace

void LocalizeCastShadow(std::vector<uint8_t> &rgba, int width, int height, const std::vector<uint8_t> &mask, int percent,
                        float offset) {
    if (percent <= 0 || mask.size() != static_cast<size_t>(width) * height) {
        return;
    }
    percent = std::min(percent, 100);
    // Soft but dark: the lettering's shape, moved down and right and blurred by about half the move, at an opacity that
    // rises quickly and levels off (50% is already a solid shadow; 100% is darker and a little longer).
    const float        opacity = 1.0f - std::exp(-static_cast<float>(percent) / 30.0f);
    const float        move = offset * (0.85f + 0.3f * static_cast<float>(percent) / 100.0f);
    const float        sigma = std::max(0.6f, 0.42f * move);
    std::vector<float> plane(static_cast<size_t>(width) * height);
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            plane[static_cast<size_t>(y) * width + x] =
                Sample(mask, width, height, static_cast<float>(x) + 0.5f - move, static_cast<float>(y) + 0.5f - move);
        }
    }
    // Gaussian blur, one direction then the other.
    const int          radius = std::max(1, static_cast<int>(std::ceil(sigma * 3.0f)));
    std::vector<float> kernel(static_cast<size_t>(radius) * 2 + 1);
    float              total = 0.0f;
    for (int i = -radius; i <= radius; i++) {
        kernel[static_cast<size_t>(i + radius)] = std::exp(-static_cast<float>(i * i) / (2.0f * sigma * sigma));
        total += kernel[static_cast<size_t>(i + radius)];
    }
    for (float &k : kernel) {
        k /= total;
    }
    std::vector<float> pass(plane.size());
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            float sum = 0.0f;
            for (int i = -radius; i <= radius; i++) {
                int sx = std::clamp(x + i, 0, width - 1);
                sum += plane[static_cast<size_t>(y) * width + sx] * kernel[static_cast<size_t>(i + radius)];
            }
            pass[static_cast<size_t>(y) * width + x] = sum;
        }
    }
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            float sum = 0.0f;
            for (int i = -radius; i <= radius; i++) {
                int sy = std::clamp(y + i, 0, height - 1);
                sum += pass[static_cast<size_t>(sy) * width + x] * kernel[static_cast<size_t>(i + radius)];
            }
            plane[static_cast<size_t>(y) * width + x] = sum;
        }
    }
    for (size_t i = 0; i < plane.size(); i++) {
        float s = std::min(1.0f, plane[i] * opacity) * (1.0f - mask[i] / 255.0f); // not under the letters themselves
        if (s <= 0.0f) {
            continue;
        }
        uint8_t *p = &rgba[i * 4];
        float    a = p[3] / 255.0f;
        float    out_a = a + s * (1.0f - a);
        float    k = a * (1.0f - s * a) / out_a; // darken what is there, then lay the shadow beneath what is transparent
        for (int c = 0; c < 3; c++) {
            p[c] = static_cast<uint8_t>(std::lround(p[c] * k));
        }
        p[3] = static_cast<uint8_t>(std::lround(out_a * 255.0f));
    }
}

fs::path LocalizeTexturePath(std::string_view name, unsigned width, unsigned height) {
    std::string language = LocalizeLanguageFile();
    if (language.empty()) {
        return {};
    }
    fs::path    result;
    std::string sized = std::string(name) + "_" + std::to_string(width) + "x" + std::to_string(height) + ".png";
    for (const std::string &file : {sized, std::string(name) + ".png"}) {
        fs::path path = LocalizeFindFile("textures/" + language + "/" + file);
        if (!path.empty()) {
            result = path;
            break;
        }
    }
    return result;
}

std::vector<uint8_t> LocalizeResample(const uint8_t *rgba, int source_width, int source_height, int width, int height) {
    std::vector<uint8_t> out(static_cast<size_t>(width) * height * 4);
    for (int y = 0; y < height; y++) {
        double y0 = static_cast<double>(y) * source_height / height;
        double y1 = static_cast<double>(y + 1) * source_height / height;
        for (int x = 0; x < width; x++) {
            double x0 = static_cast<double>(x) * source_width / width;
            double x1 = static_cast<double>(x + 1) * source_width / width;
            double r = 0, g = 0, b = 0, a = 0, weight = 0;
            for (int sy = static_cast<int>(y0); sy < std::min(source_height, static_cast<int>(std::ceil(y1))); sy++) {
                double wy = std::min<double>(sy + 1, y1) - std::max<double>(sy, y0);
                for (int sx = static_cast<int>(x0); sx < std::min(source_width, static_cast<int>(std::ceil(x1))); sx++) {
                    double         w = wy * (std::min<double>(sx + 1, x1) - std::max<double>(sx, x0));
                    const uint8_t *p = rgba + (static_cast<size_t>(sy) * source_width + sx) * 4;
                    double         pa = p[3] / 255.0;
                    r += p[0] * pa * w; // averaged premultiplied, so a transparent pixel's colour does not bleed in
                    g += p[1] * pa * w;
                    b += p[2] * pa * w;
                    a += pa * w;
                    weight += w;
                }
            }
            uint8_t *o = &out[(static_cast<size_t>(y) * width + x) * 4];
            if (weight <= 0 || a <= 0) {
                o[0] = o[1] = o[2] = o[3] = 0;
                continue;
            }
            o[0] = static_cast<uint8_t>(std::lround(r / a));
            o[1] = static_cast<uint8_t>(std::lround(g / a));
            o[2] = static_cast<uint8_t>(std::lround(b / a));
            o[3] = static_cast<uint8_t>(std::lround(255.0 * a / weight));
        }
    }
    return out;
}

bool LocalizeTexture(const char *name, PortDecodedTexture &texture) {
    if (name == nullptr || texture.width == 0 || texture.height == 0 || texture.levels.empty()) {
        return false;
    }
    fs::path path = LocalizeTexturePath(name, texture.width, texture.height);
    if (path.empty()) {
        return false;
    }
    int                  width = 0;
    int                  height = 0;
    std::vector<uint8_t> rgba;
    if (!PngLoadRgba(path, width, height, rgba)) {
        std::fprintf(stderr, "localize: cannot read picture %s\n", path.string().c_str());
        return false;
    }
    if (!SameShape(width, height, texture.width, texture.height)) {
        if (!Reported()[path.string()]) {
            Reported()[path.string()] = true;
            std::fprintf(stderr, "localize: %s is %dx%d, the texture %s is %ux%u: not used\n", path.string().c_str(),
                         width, height, name, texture.width, texture.height);
        }
        return false;
    }
    if (std::optional<Kind> kind = KindOf(name)) {
        int percent = ConfigGet().*kind->percent;
        if (percent > 0) {
            // What casts the shadow: the picture's `<name>_text.png` where it has one (a card's backdrop is opaque), else
            // its own opacity.
            std::vector<uint8_t> mask(static_cast<size_t>(width) * height);
            for (size_t i = 0; i < mask.size(); i++) {
                mask[i] = rgba[i * 4 + 3];
            }
            fs::path             mask_path = LocalizeFindFile(std::string("textures/") + LocalizeLanguageFile() + "/" + name + "_text.png");
            int                  mask_width = 0;
            int                  mask_height = 0;
            std::vector<uint8_t> mask_rgba;
            if (!mask_path.empty() && PngLoadRgba(mask_path, mask_width, mask_height, mask_rgba)) {
                std::vector<uint8_t> fitted = mask_width == width && mask_height == height
                                                  ? mask_rgba
                                                  : LocalizeResample(mask_rgba.data(), mask_width, mask_height, width, height);
                for (size_t i = 0; i < mask.size(); i++) {
                    mask[i] = fitted[i * 4];
                }
            }
            // The offset is in the texture's own pixels, so a picture drawn at more pixels than the texture gets a longer one.
            LocalizeCastShadow(rgba, width, height, mask, percent,
                               kind->scale * static_cast<float>(width) / static_cast<float>(texture.width));
        }
    }
    size_t                            level_count = texture.levels.size();
    std::vector<std::vector<uint8_t>> levels;
    bool                              translucent = false;
    for (size_t level = 0; level < level_count; level++) {
        int w = std::max<int>(1, static_cast<int>(texture.width >> level));
        int h = std::max<int>(1, static_cast<int>(texture.height >> level));
        levels.push_back(LocalizeResample(rgba.data(), width, height, w, h));
    }
    for (size_t i = 3; i < levels[0].size(); i += 4) {
        if (levels[0][i] != 255) {
            translucent = true;
            break;
        }
    }
    texture.levels = std::move(levels);
    texture.format = gfx::TextureFormat::Rgba8;
    texture.has_alpha = translucent || texture.has_alpha;
    texture.four_bit = false;
    return true;
}
