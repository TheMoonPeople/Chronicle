#include "mapobject.hpp"

#include <libvu0.h>

#include "frame.hpp"
#include "framevu1.hpp"
#include "mglib.hpp"

namespace dc::game {

CFrame *CMapObject::GetCollisionFrame() {
    if (this->draw_on == 0) {
        return nullptr;
    }

    if (this->collision_frame == nullptr) {
        return nullptr;
    }

    this->collision_frame->SetPosition(this->pos);
    this->collision_frame->SetRotation(this->rotation.x, this->rotation.y, this->rotation.z);
    this->collision_frame->SetScale(this->scale[0], this->scale[1], this->scale[2]);
    return this->collision_frame;
}

void CMapObject::FrameObjectOnOff(char *name, int on) {
    if (name == nullptr) {
        return;
    }

    for (CFrameVu1 *lod_frame : this->frame) {
        if (lod_frame != nullptr) {
            CFrame *found = lod_frame->SearchFrame(name);

            if (found != nullptr) {
                found->attr.draw_on = on;
            }
        }
    }

    if (this->shadow_frame != nullptr) {
        CFrame *found = this->shadow_frame->SearchFrame(name);

        if (found != nullptr) {
            found->attr.draw_on = on;
        }
    }

    if (this->shade_frame != nullptr) {
        CFrame *found = this->shade_frame->SearchFrame(name);

        if (found != nullptr) {
            found->attr.draw_on = on;
        }
    }

    if (this->collision_frame != nullptr) {
        CFrame *found = this->collision_frame->SearchFrame(name);

        if (found != nullptr) {
            found->flags = on;
        }
    }

    if (this->camera_frame != nullptr) {
        CFrame *found = this->camera_frame->SearchFrame(name);

        if (found != nullptr) {
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

    if (out_level != nullptr) {
        *out_level = level;
    }
}

void CMapObject::DrawShadow(int fast) {
    sceVu0FVECTOR transform;

    if (this->draw_on == 0) {
        return;
    }

    if (this->shadow_frame == nullptr) {
        return;
    }

    sceVu0FVECTOR light = {0.0f, 1.0f, 0.0f, 0.0f};

    GetRotation(transform);
    this->shadow_frame->SetRotation(transform[0], transform[1], transform[2]);
    sceVu0CopyVector(transform, this->pos);
    this->shadow_frame->SetPosition(transform);

    // The shadow drops by the offset so that it lies on the ground rather than inside the model.
    transform[1] += this->shadow_offset;

    if (fast) {
        MGDrawShadowFast2(this->shadow_frame, transform, light);
    } else {
        MGDrawShadow(this->shadow_frame, transform, light);
    }
}

void CMapObject::DrawShade() {
    sceVu0FVECTOR transform;

    if (this->draw_on == 0) {
        return;
    }

    if (this->shade_frame == nullptr) {
        return;
    }

    GetRotation(transform);
    this->shade_frame->SetRotation(transform[0], transform[1], transform[2]);
    sceVu0CopyVector(transform, this->pos);
    this->shade_frame->SetPosition(transform);
    MGDrawShade(this->shade_frame);
}

void CMapObject::Initialize() {
    CObjectFrame::Initialize(nullptr);
    this->category_no = this->handle = -1;
    this->shadow_frame = nullptr;
    this->shade_frame = nullptr;
    this->collision_frame = nullptr;
    this->camera_frame = nullptr;
    this->shadow_offset = -8.0f;
}

CMapObject::CMapObject() {
    Initialize();
}

} // namespace dc::game
