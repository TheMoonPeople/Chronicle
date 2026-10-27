#pragma once

#include <vector>

#include "collision.hpp"

namespace dc::game {
class CEditGround;
class CCameraFollow;
class CDungeonMap;
} // namespace dc::game

/**
 * Collects town camera geometry into polys, proving an allocation bound before each retail collector.
 * mask bit 0 selects fixed parts and bit 1 movable parts; grid areas are always included.
 * Returns the polygon count, or a negative value if the geometry could not be validated within budget.
 */
int MouseCameraPolys(CEditGround &ground, CBoxVu0 &box, int mask, std::vector<CCPoly> &polys);

/**
 * Collects dungeon camera geometry into polys, proving an allocation bound before each retail collector.
 * Each part's collision frame is transformed to its placement before collection.
 * Returns the polygon count, or a negative value if the geometry could not be validated within budget.
 */
int MouseCameraPolys(CDungeonMap &map, CBoxVu0 &box, std::vector<CCPoly> &polys);

/**
 * Conservative, continuous clearance test for the eye, its look ray and floor clearance.
 * The radii enlarge the eye and look points into regions that must all be clear.
 * Returns false for invalid input or any polygon closer than the required clearance.
 */
bool MouseCameraClear(const float *eye, const float *look, float eye_radius, float look_radius,
                      CCPoly *polys, int count, float vertical_radius = 0.0f, float floor_clearance = 18.0f);

/**
 * Clamps an orbit turn so the current, pending and follow-easing camera corridor stays clear.
 * If retail left a blocked pending target after contracting a clear actual eye, this may shorten distance
 * and reset that pending target after proving the recovery corridor; the actual eye and reference never move.
 */
float MouseCameraClamp(CCameraFollow &camera, float turn, CCPoly *polys, int count);

/**
 * Clamps a new horizontal orbit distance against the entire current, pending and follow corridor.
 * Applies the same pending-target recovery as MouseCameraClamp.
 * Returns the accepted distance, or the current distance if no change can be proven clear.
 */
float MouseCameraClampDistance(CCameraFollow &camera, float distance, CCPoly *polys, int count,
                               float floor_clearance = 18.0f);
