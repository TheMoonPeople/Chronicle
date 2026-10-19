#include "texture_assets.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>
#include <string>

#include "../../tools/dcdata/sharpfont.hpp"
#include "editloop.hpp"
#include "mainselect.hpp"
#include "localize_texture.hpp"
#include "main.hpp"
#include "platform/config.hpp"
#include "platform/imagefile.hpp"
#include "platform/paths.hpp"
#include "texture_port.hpp"

extern s32 mode;

namespace fs = std::filesystem;

namespace {

std::vector<fs::path> g_roots;
bool                  g_roots_set;
bool                  g_sharp_gaiji; // the glyph sheet in use is the Sharp text font's

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

fs::path Find(const fs::path &directory, std::string_view name, unsigned width, unsigned height) {
    std::string sized = std::string(name) + "_" + std::to_string(width) + "x" + std::to_string(height) + ".png";
    for (const std::string &file : {sized, std::string(name) + ".png"}) {
        std::error_code error;
        fs::path        path = directory / file;
        if (fs::is_regular_file(path, error)) {
            return path;
        }
    }
    return {};
}

} // namespace

fs::path SharpTextFile(std::string_view name) {
    // Asked for every frame text is drawn: once made, the files stay; until then, looked for now and again (a worker
    // may be making them).
    static bool built = false;
    static int  until_next_look = 0;
    const fs::path &root = PathsDataRoot();
    if (!built) {
        if (until_next_look-- > 0) {
            return {};
        }
        until_next_look = 300;
        built = dcdata::sharp::IsBuilt(root);
        if (!built) {
            return {};
        }
    }
    std::error_code error;
    fs::path        file = root / "sharp" / std::string(name);
    return fs::is_regular_file(file, error) ? file : fs::path{};
}

fs::path SharpGaijiPath(std::string_view language) {
    if (language.empty() || language.find_first_of("/\\.") != std::string_view::npos) {
        return {};
    }
    return SharpTextFile(std::string(language) + "/gaiji.png");
}

std::string TextureAssetsArea() {
    switch (mode) {
        case GAME_MODE_DUNGEON:
        case GAME_MODE_LOADER:
            if (main_select_menu_no >= 0 && main_select_menu_no < 7) {
                return "d0" + std::to_string(main_select_menu_no + 1);
            }
            return {};
        case GAME_MODE_EDIT:
            return EditMapName;
        default:
            return {};
    }
}

std::string TextureAssetsLanguage() {
    static const char *const kDirs[7] = {"a_jpn", "a_usa", "a_eng", "a_fre", "a_ger", "a_ita", "a_spa"};
    return LanguageCode >= 0 && LanguageCode < 7 ? kDirs[LanguageCode] : "";
}

std::vector<fs::path> TextureAssetRoots() {
    if (g_roots_set) {
        return g_roots;
    }
    std::vector<fs::path> roots;
    roots.push_back(PathsSaveRoot() / "assets");
    fs::path exe = PathsExecutable();
    if (!exe.empty()) {
        roots.push_back(exe.parent_path() / "assets");
    }
    return roots;
}

void TextureAssetsSetRoots(const std::vector<fs::path> &roots) {
    g_roots = roots;
    g_roots_set = !roots.empty();
}

fs::path TextureAssetPath(std::string_view area, std::string_view language, std::string_view name, unsigned width,
                          unsigned height) {
    if (name.empty() || name.find_first_of("/\\") != std::string_view::npos || name == "." || name == "..") {
        return {};
    }
    auto safe = [](std::string_view folder) {
        return !folder.empty() && folder != "." && folder != ".." && folder.find_first_of("/\\") == std::string_view::npos;
    };
    for (const fs::path &root : TextureAssetRoots()) {
        for (std::string_view folder : {area, language}) {
            if (safe(folder)) {
                fs::path found = Find(root / std::string(folder) / "textures", name, width, height);
                if (!found.empty()) {
                    return found;
                }
            }
        }
        fs::path found = Find(root / "textures", name, width, height);
        if (!found.empty()) {
            return found;
        }
    }
    return {};
}

bool TextureAssetsSharpGaiji() {
    return g_sharp_gaiji;
}

bool TextureAssetApply(const char *name, PortDecodedTexture &texture) {
    if (name == nullptr || texture.width == 0 || texture.height == 0 || texture.levels.empty()) {
        return false;
    }
    const bool gaiji = std::string_view(name) == "gaiji";
    if (gaiji) {
        g_sharp_gaiji = false;
    }
    // A picture is looked for by the texture's own size, so a texture this function already replaced is left alone.
    if (texture.logical_width != 0) {
        return false;
    }
    fs::path path = TextureAssetPath(TextureAssetsArea(), TextureAssetsLanguage(), name, texture.width, texture.height);
    bool     sharp = false;
    if (path.empty() && gaiji && ConfigGet().font_sharp && !ConfigGet().font_community) {
        // The Sharp text font: the glyph sheet drawn from the game's letters when the data was extracted.
        path = SharpGaijiPath(TextureAssetsLanguage());
        sharp = !path.empty();
    }
    if (path.empty()) {
        return false;
    }
    int                  width = 0;
    int                  height = 0;
    std::vector<uint8_t> rgba;
    if (!imagefile::LoadPng(path, width, height, rgba)) {
        if (!Reported()[path.string()]) {
            Reported()[path.string()] = true;
            std::fprintf(stderr, "assets: cannot read picture %s\n", PathsDisplay(path).c_str());
        }
        return false;
    }
    if (!SameShape(width, height, texture.width, texture.height)) {
        if (!Reported()[path.string()]) {
            Reported()[path.string()] = true;
            std::fprintf(stderr, "assets: %s is %dx%d, the texture %s is %ux%u: not used\n",
                         PathsDisplay(path).c_str(), width, height, name, texture.width, texture.height);
        }
        return false;
    }

    // The size the picture is kept at: its own if it has the texture's pixels or more (up to the cap), else the
    // texture's.
    int target_width = width;
    int target_height = height;
    if (static_cast<unsigned>(width) < texture.width || static_cast<unsigned>(height) < texture.height) {
        target_width = static_cast<int>(texture.width);
        target_height = static_cast<int>(texture.height);
    } else if (std::max(width, height) > kTextureAssetMaxSide) {
        double k = static_cast<double>(kTextureAssetMaxSide) / std::max(width, height);
        target_width = std::max(1, static_cast<int>(std::lround(width * k)));
        target_height = std::max(1, static_cast<int>(std::lround(height * k)));
    }

    size_t                            level_count = texture.levels.size();
    std::vector<std::vector<uint8_t>> levels;
    bool                              translucent = false;
    for (size_t level = 0; level < level_count; level++) {
        int w = std::max(1, target_width >> level);
        int h = std::max(1, target_height >> level);
        levels.push_back(LocalizeResample(rgba.data(), width, height, w, h));
    }
    for (size_t i = 3; i < levels[0].size(); i += 4) {
        if (levels[0][i] != 255) {
            translucent = true;
            break;
        }
    }
    texture.logical_width = texture.width;
    texture.logical_height = texture.height;
    texture.width = static_cast<unsigned>(target_width);
    texture.height = static_cast<unsigned>(target_height);
    texture.levels = std::move(levels);
    texture.format = gfx::TextureFormat::Rgba8;
    texture.has_alpha = translucent || texture.has_alpha;
    texture.four_bit = false;
    g_sharp_gaiji = sharp;
    return true;
}
