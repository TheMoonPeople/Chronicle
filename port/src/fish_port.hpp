#pragma once

// A fix to retail's size roll (CFish::SetScale). Retail clamps every roll past a kind's largest size
// to the largest, so the top display centimetre holds the whole tail of the roll (8.5 fish in a
// million) and stands out against the centimetre below it, where a bigger fish should be the
// rarer: 7.9 times as many for Baron Garayan, 2.1 for Mardan Garayan. A fish rolled above its
// kind's usual size is moved up the range by a share t^kFishSizeLiftExponent * (1 - t) of it, t
// being how far up it rolled, so that the centimetres approaching the largest fill in to about the
// largest's own share and the sizes ramp into it. The largest keeps exactly its retail rarity: only
// a roll that retail clamps lands on it.
inline constexpr float kFishSizeLiftExponent = 1.2f;

// The lift's strength for a kind whose sizes span range (largest less usual, in size units): the
// strength that leaves the centimetre below the largest holding as many fish as the largest. Wide
// ranges are lifted more, since there the clamp stands out more; a range of 4 cm or less, none.
float FishSizeLift(float range);

float SmoothFishSize(float size, float base_size, float max_size);

struct RS_STACKDATA;

// The towns' bait fix. Each town's bait-changing event (program 134 of gedit/e01, e02, e04 and s09 in PAL
// retail) opens the bait picker (_GOTO_CHANGE_ESA), which offers all thirteen baits and takes the chosen one
// out of the bag, then looks the choice up in a list of eight baits the script keeps in the eight variables
// after the one the picker writes to. Only a bait on that list is attached (_LOAD_ITEM_FILE, _LOAD_ITEM,
// _SET_FISHING_ESA): Carrot, Potato Cake, Minon, Battan and Petite Fish are not on it, so they were eaten and
// the hook left empty. When the picker closes, the port puts such a choice in the list's first place, and the
// script attaches it as it attaches the others.
//
// Puts bait into the list after choice, the variable the picker writes to, if the script would not attach it
// otherwise, and returns whether it did. Nothing is changed when bait is not one of the thirteen (a cancelled
// pick), when it is on the list already, or when the eight variables do not all hold baits (a script laid out
// otherwise).
bool ListBaitForScript(RS_STACKDATA *choice, int bait);
