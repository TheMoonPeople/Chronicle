#pragma once

class CEditGround;

/**
 * Casts the shadows of the parts within video.shadow_distance of the eye that lie deeper than the
 * map's shadow_near, which CEditGround::DrawShadow leaves out of the near band. EditMainDraw casts
 * them before the depth of field, so that it softens them with the ground they fall on.
 */
void EdGroundDrawShadowBeyondNear(CEditGround *ground, float shadow_near);
