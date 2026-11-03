#pragma once

namespace dc::game {
class CDungeonEventData;
} // namespace dc::game

namespace dc::dungeon {

/**
 * Cross at an event: whether its script should run as Square starts it, asking for an item. True with
 * auto_open_locked_doors (rules.hpp) at a locked room door while the bag holds a key that opens the
 * dungeon's locked doors (item::DungeonKey), which the door's item window is then given without asking
 * (TakeAutoKey).
 */
bool AutoOpenLockedDoor(const game::CDungeonEventData &slot);

/**
 * The key an AutoOpenLockedDoor press set aside, for the item window a script opens: given when the
 * window takes that key and the bag still holds it, and taken out of the bag when the window uses up
 * what it takes (vanish), as choosing it would. Forgets the key either way.
 */
bool TakeAutoKey(const int *usable, bool vanish, int &item);

} // namespace dc::dungeon
