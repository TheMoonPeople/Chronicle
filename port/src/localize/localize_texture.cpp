#include "localize/localize_texture.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <format>
#include <optional>
#include <set>
#include <span>
#include <string>

#include "localize/localize.hpp"
#include "platform/config.hpp"
#include "platform/png_image.hpp"
#include "render/texture_registry.hpp"

namespace fs = std::filesystem;

namespace dc::localize {

using namespace dc::platform;
using namespace dc::render;

namespace {

// Pictures already complained about, so a wrong-shaped one says so once.
std::set<std::string> &Reported() {
    static std::set<std::string> reported;
    return reported;
}

// Whether a picture of this shape belongs to a texture of that one (a card is not stretched into a menu sheet).
bool SameShape(int source_width, int source_height, unsigned width, unsigned height) {
    double source_aspect = static_cast<double>(source_width) / std::max(1, source_height);
    double aspect = static_cast<double>(width) / std::max<unsigned>(1, height);
    return std::fabs(source_aspect - aspect) <= 0.02 * aspect;
}

// Which kind of text a picture is, so its shadow follows that kind's Options slider, and how far (in the texture's
// own pixels, at 50%) the shadow falls down and to the right: about a tenth of the letters' height.
struct Kind {
    int Config::*percent;
    float        scale;
};

std::optional<Kind> KindOf(std::string_view name) {
    auto digits = [&](size_t from) {
        return name.size() > from && std::ranges::all_of(name.substr(from), [](char c) { return c >= '0' && c <= '9'; });
    };
    if (name.starts_with("mt") && digits(2)) {
        return Kind{.percent = &Config::name_shadow, .scale = 5.6f};
    }
    if (name.starts_with("floor") && digits(5)) {
        return Kind{.percent = &Config::floor_shadow, .scale = 3.1f};
    }
    if (name.starts_with("boss_") && digits(5)) {
        return Kind{.percent = &Config::boss_shadow, .scale = 3.6f};
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
    for (size_t tap = 0; tap < kernel.size(); tap++) {
        int distance = static_cast<int>(tap) - radius;
        kernel[tap] = std::exp(-static_cast<float>(distance * distance) / (2.0f * sigma * sigma));
        total += kernel[tap];
    }
    for (float &tap : kernel) {
        tap /= total;
    }
    std::vector<float> pass(plane.size());
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            float sum = 0.0f;
            for (size_t tap = 0; tap < kernel.size(); tap++) {
                int sx = std::clamp(x + static_cast<int>(tap) - radius, 0, width - 1);
                sum += plane[static_cast<size_t>(y) * width + sx] * kernel[tap];
            }
            pass[static_cast<size_t>(y) * width + x] = sum;
        }
    }
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            float sum = 0.0f;
            for (size_t tap = 0; tap < kernel.size(); tap++) {
                int sy = std::clamp(y + static_cast<int>(tap) - radius, 0, height - 1);
                sum += pass[static_cast<size_t>(sy) * width + x] * kernel[tap];
            }
            plane[static_cast<size_t>(y) * width + x] = sum;
        }
    }
    for (size_t i = 0; i < plane.size(); i++) {
        float shadow = std::min(1.0f, plane[i] * opacity) * (1.0f - mask[i] / 255.0f); // not under the letters themselves
        if (shadow <= 0.0f) {
            continue;
        }
        std::span<uint8_t> pixel = std::span(rgba).subspan(i * 4, 4);
        float              alpha = pixel[3] / 255.0f;
        float              out_alpha = alpha + shadow * (1.0f - alpha);
        // Darken what is there, then lay the shadow beneath what is transparent.
        float darken = alpha * (1.0f - shadow * alpha) / out_alpha;
        for (int channel = 0; channel < 3; channel++) {
            pixel[channel] = static_cast<uint8_t>(std::lround(pixel[channel] * darken));
        }
        pixel[3] = static_cast<uint8_t>(std::lround(out_alpha * 255.0f));
    }
}

fs::path LocalizeTexturePath(std::string_view name, unsigned width, unsigned height) {
    std::string language = LocalizeLanguageFile();
    if (language.empty()) {
        return {};
    }
    for (const std::string &file : {std::format("{}_{}x{}.png", name, width, height), std::format("{}.png", name)}) {
        fs::path path = LocalizeFindFile(std::format("textures/{}/{}", language, file));
        if (!path.empty()) {
            return path;
        }
    }
    return {};
}

std::vector<uint8_t> LocalizeResample(const uint8_t *rgba, int source_width, int source_height, int width, int height) {
    std::span<const uint8_t> source(rgba, static_cast<size_t>(source_width) * source_height * 4);
    std::vector<uint8_t>     out(static_cast<size_t>(width) * height * 4);
    for (int y = 0; y < height; y++) {
        double y0 = static_cast<double>(y) * source_height / height;
        double y1 = static_cast<double>(y + 1) * source_height / height;
        for (int x = 0; x < width; x++) {
            double x0 = static_cast<double>(x) * source_width / width;
            double x1 = static_cast<double>(x + 1) * source_width / width;
            double red = 0;
            double green = 0;
            double blue = 0;
            double alpha = 0;
            double weight = 0;
            for (int sy = static_cast<int>(y0); sy < std::min(source_height, static_cast<int>(std::ceil(y1))); sy++) {
                double row_overlap = std::min<double>(sy + 1, y1) - std::max<double>(sy, y0);
                for (int sx = static_cast<int>(x0); sx < std::min(source_width, static_cast<int>(std::ceil(x1))); sx++) {
                    double                   area = row_overlap * (std::min<double>(sx + 1, x1) - std::max<double>(sx, x0));
                    std::span<const uint8_t> pixel = source.subspan((static_cast<size_t>(sy) * source_width + sx) * 4, 4);
                    double                   pixel_alpha = pixel[3] / 255.0;
                    // Averaged premultiplied, so a transparent pixel's colour does not bleed in.
                    red += pixel[0] * pixel_alpha * area;
                    green += pixel[1] * pixel_alpha * area;
                    blue += pixel[2] * pixel_alpha * area;
                    alpha += pixel_alpha * area;
                    weight += area;
                }
            }
            std::span<uint8_t> target = std::span(out).subspan((static_cast<size_t>(y) * width + x) * 4, 4);
            if (weight <= 0 || alpha <= 0) {
                std::ranges::fill(target, uint8_t{0});
                continue;
            }
            target[0] = static_cast<uint8_t>(std::lround(red / alpha));
            target[1] = static_cast<uint8_t>(std::lround(green / alpha));
            target[2] = static_cast<uint8_t>(std::lround(blue / alpha));
            target[3] = static_cast<uint8_t>(std::lround(255.0 * alpha / weight));
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
    int                  picture_width = 0;
    int                  picture_height = 0;
    std::vector<uint8_t> rgba;
    if (!PngLoadRgba(path, picture_width, picture_height, rgba)) {
        std::fprintf(stderr, "localize: cannot read picture %s\n", path.string().c_str());
        return false;
    }
    if (!SameShape(picture_width, picture_height, texture.width, texture.height)) {
        if (Reported().insert(path.string()).second) {
            std::fprintf(stderr, "localize: %s is %dx%d, the texture %s is %ux%u: not used\n", path.string().c_str(),
                         picture_width, picture_height, name, texture.width, texture.height);
        }
        return false;
    }
    if (std::optional<Kind> kind = KindOf(name)) {
        int percent = ConfigGet().*kind->percent;
        if (percent > 0) {
            // What casts the shadow: the picture's `<name>_text.png` where it has one (a card's backdrop is opaque), else
            // its own opacity.
            std::vector<uint8_t> mask(static_cast<size_t>(picture_width) * picture_height);
            for (size_t i = 0; i < mask.size(); i++) {
                mask[i] = rgba[i * 4 + 3];
            }
            fs::path             mask_path = LocalizeFindFile(std::format("textures/{}/{}_text.png", LocalizeLanguageFile(), name));
            int                  mask_width = 0;
            int                  mask_height = 0;
            std::vector<uint8_t> mask_rgba;
            if (!mask_path.empty() && PngLoadRgba(mask_path, mask_width, mask_height, mask_rgba)) {
                std::vector<uint8_t> fitted = mask_width == picture_width && mask_height == picture_height
                                                  ? mask_rgba
                                                  : LocalizeResample(mask_rgba.data(), mask_width, mask_height, picture_width, picture_height);
                for (size_t i = 0; i < mask.size(); i++) {
                    mask[i] = fitted[i * 4];
                }
            }
            // The offset is in the texture's own pixels, so a picture drawn at more pixels than the texture gets a longer one.
            LocalizeCastShadow(rgba, picture_width, picture_height, mask, percent,
                               kind->scale * static_cast<float>(picture_width) / static_cast<float>(texture.width));
        }
    }
    size_t                            level_count = texture.levels.size();
    std::vector<std::vector<uint8_t>> levels;
    bool                              translucent = false;
    for (size_t level = 0; level < level_count; level++) {
        int level_width = std::max<int>(1, static_cast<int>(texture.width >> level));
        int level_height = std::max<int>(1, static_cast<int>(texture.height >> level));
        levels.push_back(LocalizeResample(rgba.data(), picture_width, picture_height, level_width, level_height));
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

} // namespace dc::localize
