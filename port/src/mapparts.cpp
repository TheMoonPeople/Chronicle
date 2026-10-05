#include "mapparts.hpp"

#include <libvu0.h>

// Retail's DrawLOD raises a part that asks for a lift by its own distance from the eye: 0.02 of the
// lift within 100 units, then a thousandth of the distance up to 2. Neighbouring road tiles are
// separate parts, so each stands at a height of its own and the ground shows through the step
// between two of them: a fraction of a pixel on the PS2's frame, a line of grass along every tile
// edge on a larger one. The lift only keeps the part clear of the ground in the depth buffer, which
// the port's float depth does at any distance, so every part takes the one height retail gives a
// part 100 units away and neighbours meet edge to edge.
constexpr float kPartsLiftDepth = 0.1f;

PC_OVERRIDE void CMapParts::DrawLOD(float *distance, int lowest, int highest, int *out_level) {
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

    if (this->lift > 0.0f) {
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
