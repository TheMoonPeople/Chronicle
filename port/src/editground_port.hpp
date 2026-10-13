#pragma once

class CEditGround;

// The shadows of the parts within video.shadow_distance of the eye that lie deeper than the map's
// shadow_near, which retail cast only in its fainter far band or not at all; CEditGround::DrawShadow
// leaves them out of the near band. EditMainDraw casts them before the depth of field, so that it
// softens them with the ground they fall on.
void EdGroundDrawShadowBeyondNear(CEditGround *ground, float shadow_near);
