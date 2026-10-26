#include "mapparts.hpp"

#include <libvu0.h>

// Road tiles must clear the uneven ground at the same height. A small lift lets the ground win the
// depth test in places as the camera moves, exposing a different, darker surface beneath the road.
// The constant road lift is within retail's distance-dependent range and keeps adjacent tiles level.
constexpr float kPartsLiftDepth = 0.1f;
constexpr float kRoadLiftDepth = 0.5f;

void CMapParts::DrawLOD(float *distance, int lowest, int highest, int *out_level) {
    sceVu0FVECTOR saved_pos;
    sceVu0FVECTOR lifted_pos;

    if (this->handle < 0) {
        return;
    }

    if (this->draw_on == 0) {
        return;
    }

    sceVu0CopyVector(saved_pos, this->pos);
    sceVu0CopyVector(lifted_pos, saved_pos);

    if (this->subtype == MAP_PARTS_SUBTYPE_ROAD && this->lift > 0.0f) {
        lifted_pos[1] += kRoadLiftDepth;
    } else if (this->lift > 0.0f) {
        if (this->lift > 1.0f) {
            lifted_pos[1] += 0.1f;
        } else {
            lifted_pos[1] += kPartsLiftDepth * this->lift;
        }
    }

    sceVu0CopyVector(this->pos, lifted_pos);
    CObjectFrame::DrawLOD(distance, lowest, highest, out_level);
    sceVu0CopyVector(this->pos, saved_pos);
}
