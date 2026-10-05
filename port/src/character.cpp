#include "character.hpp"

#include <libvu0.h>

#include "arena.hpp"
#include "cloth.hpp"
#include "frame.hpp"

// CCharacter::wind is an s32 holding the address of a CWind, and every writer (title.cpp's
// InitProc*, op_b, op_c, op_d, rushmovi, editloop's MainEditMode, EdEventNPCStep's copies) stores
// an image global there. Retail's cast back only survives an image below 4 GiB, so the pointer is
// recovered from the image instead.
PC_OVERRIDE void CCharacter::ClothStep(int step) {
    sceVu0FVECTOR world_pos;
    CFrame       *root;
    int           i;

    if (MotionStopFlag != 0) {
        return;
    }

    sceVu0CopyVector(world_pos, this->pos);

    if (this->frame != NULL) {
        this->frame->SetPosition(this->pos[0], this->pos[1], this->pos[2]);
        this->frame->SetRotation(this->rotation.x, this->rotation.y, this->rotation.z);
        root = this->frame->parent;

        if (root != NULL) {
            this->pos[3] = 1.0f;
            root->GetWorldPosition(world_pos, this->pos);
        }
    }

    for (i = 0; i < CHARA_CLOTH_MAX; i++) {
        if (this->cloth[i] != NULL) {
            this->cloth[i]->wind = PortImagePointer(this->wind);
            this->cloth[i]->floor_y = world_pos[1];
            this->cloth[i]->Step(step);
        }
    }
}
