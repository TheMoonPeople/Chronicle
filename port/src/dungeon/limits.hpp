#pragma once

namespace dc::game {
class CDngStatusData;
} // namespace dc::game

namespace dc::dungeon {

/**
 * The limit a dungeon's floor puts on the party when the player arrives on it (a ResLimitZone, -1 for
 * none): one of the floors its script always limits, or one the game rolled into status's
 * res_limit_zone_id when the game began, on a floor the script lets a rolled limit take. dungeon is 0-5,
 * floor counted from 0.
 */
int FloorLimit(const game::CDngStatusData &status, int dungeon, int floor);

/**
 * The message the dungeon's own message file (dunmsd00) tells a limit with on arrival, and in insert the
 * message its {insert1} names (the one character who may fight), -1 for none.
 */
int FloorLimitMessage(int limit, int &insert);

} // namespace dc::dungeon
