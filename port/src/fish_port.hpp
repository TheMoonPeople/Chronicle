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
