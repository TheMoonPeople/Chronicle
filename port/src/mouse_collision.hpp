#pragma once

#include <vector>

#include "collision.hpp"

class CEditGround;
class CCameraFollow;

// Collects camera geometry with a proven allocation bound before calling retail collectors.
// A negative result means the geometry could not be validated within the work budget.
int MouseCameraPolys(CEditGround &ground, CBoxVu0 &box, int mask, std::vector<CCPoly> &polys);

// A continuous, conservative clearance test for the eye, its look ray and floor clearance.
bool MouseCameraClear(const float *eye, const float *look, float eye_radius, float look_radius,
                      CCPoly *polys, int count, float vertical_radius = 0.0f);

// Includes pending positions and the remaining follow-camera easing corridor.
float MouseCameraClamp(CCameraFollow &camera, float turn, CCPoly *polys, int count);
