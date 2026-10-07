#pragma once

#include <functional>

class CBoxVu0;
class CCamera;
class CCameraFollow;
class CCPoly;
class CEditGround;

// The mouse turns the game's cameras where the right stick does, by the angle it moved
// (InputGetMouseLook) rather than through a stick's dead zone, steps and full deflection. A camera
// reads it in its stick's units, added to the stick's reading, so its own conditions (R1 and L1
// only while the stick rests) see both alike; the town's walk camera takes it apart (below).
//
// A follow camera eases towards its angle and its eye towards its place (CCameraFollow::Step,
// CCamera::Step), which suits a stick's small steady steps but trails the mouse and takes the
// shorter way round, backwards, once a step passes half a turn. So a turning reading also leaves
// the mouse's share with the camera, and the camera's next AddAngle within the same pad read, if
// it is exactly that reading's delta, turns the camera by the share at once
// (port/src/camerafollow.cpp); the stick's share and everything else the camera does still ease.
// Vertical mouse motion changes view pitch independently of the retail height controls.

// The stick's reading plus the mouse's turn this tick, for a camera that turns by
// AddAngle(radians * -reading): right turns right.
float MouseLookTurn(CCameraFollow *camera, float radians, float stick, bool pitch = true);

// Marks the camera currently under player control; lock-on disables manual pitch.
void MouseLookControlPitch(CCamera *camera, bool enabled);

// The view's pitch after this read's mouse motion, once per read, within 86 degrees of level.
// It changes the view matrix, not the eye, follow height, look target or pending camera positions.
float MouseLookViewPitch(CCamera *camera, float base);

// The mouse's share of an AddAngle delta on camera: nonzero only for the delta of the reading
// MouseLookTurn last gave for it in the current pad read. Any AddAngle on that camera ends the
// reading.
float MouseLookTakeTurn(CCameraFollow *camera, float delta);

// A first-person view's vertical angle (above zero looks down) once the mouse tilts it, within the mouse's
// 86-degree limits; the stick retains the retail limits.
float MouseLookEyeAngleV(float angle);

// A first-person view's heading once the mouse turns it, within half a turn either way.
float MouseLookEyeAngleH(float angle);

// The town's walk camera (EdMoveChara) decides its wall tests from where the eye is before the
// stick's turn, which allows for a stick's small step, not for the mouse's whole turn at once. So
// the town takes the mouse's turn apart from the stick's: EdGetRXf records it into a request
// EditLoop opens around the walk, and EditLoop applies it after the camera's step, as far round as
// the eye stays clear of walls, the rest dropped.

// The camera polygons in box (CEditGround::PickUpCameraPoly), as a count written to *polys.
using MouseLookPolys = std::function<int(CBoxVu0 &box, CCPoly **polys)>;

// The signed safe prefix of a continuous turn. Conservative bounds cover the swept look ray,
// eye clearance and floor query; uncertain intervals subdivide within a fixed work budget.
// An initially invalid pose rejects the turn. This helper has no pending camera state;
// TownMouseApply validates the full follow-camera corridor through MouseCameraClamp.
float MouseLookClampOrbit(const float *eye, const float *look, float turn, const MouseLookPolys &polys);

// Turns a follow camera's angle, the angle it is turning to, its eye and the eye it is easing to by
// turn radians at once.
void MouseLookTurnOrbit(CCameraFollow *camera, float turn);

// Opens the request for this pad read's walk, replacing any other.
void TownMouseBegin(CCameraFollow *camera, CEditGround *ground, int map, int mode);

// From EdGetRXf: the mouse's turn this read for camera, if a request for it is open.
void TownMouseRecord(CCameraFollow *camera, bool fishing);

// Applies and closes the request, if the camera shown, the ground, the map and the mode are still
// the ones it was opened for, in the same pad read.
void TownMouseApply(CCamera *shown, CEditGround *ground, int map, int mode);
