#pragma once

#include <cstdint>
#include <filesystem>
#include <string_view>
#include <vector>

namespace dc::render {
struct PortDecodedTexture;
} // namespace dc::render

namespace dc::localize {

// Pictures of text. Some of the game's words are not messages but textures (an area's name card, the floor
// labels, a boss's name), one picture per language. The localization takes these from
// `textures/<language>/<name>.png` in the language folders (the save folder's before the executable's; the
// language is the same file stem as the language's JSON, `en_gb`, `fr_fr`...), so they follow game.language
// like the messages do. `<name>_<w>x<h>.png` is tried first, for a name two textures of different sizes share.
// A picture is scaled to the size of the texture it replaces; one whose shape differs from the texture's is
// left unused. tools/textpack writes these from the language's own words, without a shadow: the port casts it,
// from `<name>_text.png` (what is lettering) if the language has one, else from the picture's own opacity, at the
// strength of the Options slider for that kind of text (area names, floor labels, bosses' names).

/**
 * The picture for texture `name` of this size in the current language, or an empty path.
 */
std::filesystem::path LocalizeTexturePath(std::string_view name, unsigned width, unsigned height);

/**
 * Area-averaged scale of straight-alpha RGBA, the way the replacement fits the texture it stands for.
 */
std::vector<uint8_t> LocalizeResample(const uint8_t *rgba, int source_width, int source_height, int width, int height);

/**
 * Lays a soft shadow under whatever `mask` (0 to 255, one byte a pixel) covers in straight-alpha RGBA: the mask's shape
 * moved `offset` pixels down and right (at 50%) and blurred, as dark as `percent` (0 casts none, 50 is solid, 100 is darker
 * and a little longer). Where the picture is transparent it becomes the shadow; where it is opaque it is darkened.
 */
void LocalizeCastShadow(std::vector<uint8_t> &rgba, int width, int height, const std::vector<uint8_t> &mask, int percent,
                        float offset);

/**
 * Called for each texture the game decodes: swaps in the language's picture when there is one. True if it did.
 */
bool LocalizeTexture(const char *name, render::PortDecodedTexture &texture);

} // namespace dc::localize
