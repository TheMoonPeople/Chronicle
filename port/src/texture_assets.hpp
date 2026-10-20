#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

struct PortDecodedTexture;

// Replacement pictures for the game's textures, by area. A texture the game enters is looked up by its name:
//
//     <root>/<area>/textures/<name>.png      the picture for that dungeon, town or event map only
//     <root>/a_eng/textures/<name>.png       the picture for one language (a_jpn a_usa a_eng a_fre a_ger a_ita a_spa)
//     <root>/textures/<name>.png             the picture for every area
//
// `<name>_<w>x<h>.png` is tried first at each step, for a name two textures of different sizes share. The roots are
// the save folder's `assets` and then the executable's, so the save folder wins. The area is the dungeon
// (`d01`..`d07`) or the town or event map (`e01`, `s04`..) the game is in; the title, menus and loading screens have
// none and use the shared folder only. A language's own picture (docs/LOCALIZATION.md) comes before these.
//
// A picture has the texture's shape (its sides in the same ratio) but may have more pixels: one with at least as many
// pixels as the texture keeps them all, up to kTextureAssetMaxSide a side, and the game still sees the texture at its
// own size. One with fewer is scaled up to the texture's size. Only colour textures are replaced whole; the picture
// is RGBA, so a texture whose palette the game changes while it runs loses that effect. See docs/TEXTURE_ASSETS.md.

/// The most pixels a side of a replacement picture keeps; a larger one is scaled down to it.
inline constexpr int kTextureAssetMaxSide = 8192;

/// The area the game is in now: "d01".."d07", a town or event map's folder name, or "" when it is in none.
std::string TextureAssetsArea();

/// The language folder of the language the game is set to: "a_jpn", "a_usa", ... or "".
std::string TextureAssetsLanguage();

/// The folders holding `assets`: the save folder's first, then the executable's. Tests replace them.
std::vector<std::filesystem::path> TextureAssetRoots();
void                               TextureAssetsSetRoots(const std::vector<std::filesystem::path> &roots);

/// The picture for texture `name` of this size: the area's folder first, then the language's, then the shared one; or
/// an empty path.
std::filesystem::path TextureAssetPath(std::string_view area, std::string_view language, std::string_view name,
                                       unsigned width, unsigned height);

/// Called for each texture the game decodes, after the language's own picture had its chance: swaps in the
/// area's picture when there is one. True if it did. With the Sharp text font, the glyph sheet "gaiji" that no
/// picture replaces is swapped for the one redrawn in Chronicle Sharp when the data was extracted.
bool TextureAssetApply(const char *name, PortDecodedTexture &texture);

/// A file the extraction makes for the Sharp text font (dcdata/sharpfont.hpp: "<language>/gaiji.png"), or an empty
/// path while it is not made.
std::filesystem::path SharpTextFile(std::string_view name);
std::filesystem::path SharpGaijiPath(std::string_view language);

/// Whether the glyph sheet last decoded is the Sharp text font's: message text is then drawn from it, as the game
/// draws its letters, instead of from a font.
bool TextureAssetsSharpGaiji();
