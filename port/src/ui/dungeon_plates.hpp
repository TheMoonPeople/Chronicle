#pragma once

namespace dc::render {
struct PortDecodedTexture;
} // namespace dc::render

/**
 * The floor select's dungeon name plates as a blank plate and the dungeon's name in the font: the plate
 * (lang/textures/plates/dungeon_name.png) is laid over the names painted in the select's sheet "dunenter"
 * as it decodes, and the select writes each name on it from the game's own messages, so every language
 * has it.
 */
namespace dc::ui::dungeon_plates {

/**
 * Lays the blank plate over every dungeon's painted name when texture name is the select's sheet and the
 * plate is there. Says whether it did.
 */
bool Prepare(const char *name, render::PortDecodedTexture &texture);

/**
 * Whether the select's sheet has its plates blank, for the select to write the names on.
 */
bool Blank();

/**
 * Writes dungeon's name (0-6) on its plate, the plate's top-left at (x, y) as the select draws it.
 */
void DrawName(int dungeon, int x, int y, int alpha);

} // namespace dc::ui::dungeon_plates
