#include "character.hpp"

#include <libvu0.h>

#include <span>

#include "cloth.hpp"
#include "frame.hpp"
#include "platform/arena.hpp"

// CCharacter::wind holds a 32-bit image address of a CWind, so the pointer is recovered through
// the image rather than cast back directly.
void CCharacter::ClothStep(int step) {
    if (MotionStopFlag != 0) {
        return;
    }

    sceVu0FVECTOR world_pos;
    sceVu0CopyVector(world_pos, this->pos);

    if (this->frame != nullptr) {
        this->frame->SetPosition(this->pos[0], this->pos[1], this->pos[2]);
        this->frame->SetRotation(this->rotation.x, this->rotation.y, this->rotation.z);
        CFrame *root = this->frame->parent;

        if (root != nullptr) {
            this->pos[3] = 1.0f;
            root->GetWorldPosition(world_pos, this->pos);
        }
    }

    for (CCloth *piece : std::span(this->cloth, CHARA_CLOTH_MAX)) {
        if (piece != nullptr) {
            piece->wind = PortImagePointer(this->wind);
            piece->floor_y = world_pos[1];
            piece->Step(step);
        }
    }
}
