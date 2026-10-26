#pragma once

struct MoveCheckInfo;
class CCPoly;

/**
 * Reports whether an editor input mode currently owns the pad.
 */
int PortEdCheckKeyMode(int mode);

/**
 * Town-walk collision step: adds keyboard first-person velocity, then runs the retail
 * floor, wall and event checks, without treating the town as an interior.
 */
int PortEdMoveCheck(float *pos, float *velocity, float *out_pos, MoveCheckInfo *out_info, CCPoly *polys, int poly_num, int mode);
