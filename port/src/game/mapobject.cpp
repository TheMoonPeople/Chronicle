#pragma helper_mask_gpr 0x30
#pragma helper_mask_fpr 0x1000

#include "mapobject.hpp"

#include <libvu0.h>

#include "frame.hpp"
#include "framevu1.hpp"
#include "mglib.hpp"

CFrame *CMapObject::GetCollisionFrame() {
    if (this->draw_on == 0) {
        return NULL;
    }

    if (this->collision_frame == NULL) {
        return NULL;
    }

    this->collision_frame->SetPosition(this->pos);
    this->collision_frame->SetRotation(this->rotation.x, this->rotation.y, this->rotation.z);
    this->collision_frame->SetScale(this->scale[0], this->scale[1], this->scale[2]);
    return this->collision_frame;
}

void CMapObject::FrameObjectOnOff(char *name, int on) {
    int     i;
    CFrame *found;

    if (name == NULL) {
        return;
    }

    for (i = 0; i < 4; i++) {
        if (this->frame[i] != NULL) {
            found = this->frame[i]->SearchFrame(name);

            if (found != NULL) {
                found->attr.draw_on = on;
            }
        }
    }

    if (this->shadow_frame != NULL) {
        found = this->shadow_frame->SearchFrame(name);

        if (found != NULL) {
            found->attr.draw_on = on;
        }
    }

    if (this->shade_frame != NULL) {
        found = this->shade_frame->SearchFrame(name);

        if (found != NULL) {
            found->attr.draw_on = on;
        }
    }

    if (this->collision_frame != NULL) {
        found = this->collision_frame->SearchFrame(name);

        if (found != NULL) {
            found->flags = on;
        }
    }

    if (this->camera_frame != NULL) {
        found = this->camera_frame->SearchFrame(name);

        if (found != NULL) {
            found->flags = on;
        }
    }
}

void CMapObject::Draw() {
    if (this->draw_on == 0) {
        return;
    }

    if (this->handle < 0) {
        return;
    }

    CObjectFrame::Draw();
}

void CMapObject::DrawLOD(float *distance, int lowest, int highest, int *out_level) {
    int level;

    if (this->draw_on == 0) {
        return;
    }

    if (this->handle < 0) {
        return;
    }

    CObjectFrame::DrawLOD(distance, lowest, highest, &level);

    if (out_level != NULL) {
        *out_level = level;
    }
}

void CMapObject::DrawShadow(int fast) {
    sceVu0FVECTOR transform;

    if (this->draw_on == 0) {
        return;
    }

    if (this->shadow_frame == NULL) {
        return;
    }

    // The shadow takes the angle and the position of the object, then drops by
    // the offset so that it lies on the ground rather than inside the model.
    sceVu0FVECTOR light = {0.0f, 1.0f, 0.0f, 0.0f};

    GetRotation(transform);

    CFrame *shadow = this->shadow_frame;
    shadow->SetRotation(transform[0], transform[1], transform[2]);
    sceVu0CopyVector(transform, this->pos);
    this->shadow_frame->SetPosition(transform);

    if (fast) {
        transform[1] += this->shadow_offset;
        MGDrawShadowFast2(this->shadow_frame, transform, light);
    } else {
        transform[1] += this->shadow_offset;
        MGDrawShadow(this->shadow_frame, transform, light);
    }
}

void CMapObject::DrawShade() {
    sceVu0FVECTOR transform;

    if (this->draw_on == 0) {
        return;
    }

    if (this->shade_frame == NULL) {
        return;
    }

    GetRotation(transform);
    this->shade_frame->SetRotation(transform[0], transform[1], transform[2]);
    sceVu0CopyVector(transform, this->pos);
    this->shade_frame->SetPosition(transform);
    MGDrawShade(this->shade_frame);
}

void CMapObject::Initialize() {
    CObjectFrame::Initialize(NULL);
    this->category_no = this->handle = -1;
    this->shadow_frame = NULL;
    this->shade_frame = NULL;
    this->collision_frame = NULL;
    this->camera_frame = NULL;
    this->shadow_offset = -8.0f;
}

CMapObject::CMapObject() {
    Initialize();
}
