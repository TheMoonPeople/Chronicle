#include "objectframe.hpp"

#include <algorithm>

#include "framevu1.hpp"
#include "mathutil.hpp"
#include "mglib.hpp"

namespace dc::game {

void CObjectFrame::SetFrame(CFrameVu1 *frame, int level) {
    if (level < 0) {
        return;
    }

    if (level >= 4) {
        return;
    }

    this->frame[level] = frame;
}

void CObjectFrame::FrameObjectOnOff(char *name, int on) {
    if (name == nullptr) {
        return;
    }

    for (CFrameVu1 *level_frame : this->frame) {
        if (level_frame != nullptr) {
            CFrame *found = level_frame->SearchFrame(name);

            if (found != nullptr) {
                found->attr.draw_on = on;
            }
        }
    }
}

void CObjectFrame::Draw() {
    if (this->draw_on == 0) {
        return;
    }

    if (this->frame[0] == nullptr) {
        return;
    }

    this->frame[0]->SetPosition(this->pos);
    this->frame[0]->SetRotation(this->rotation.x, this->rotation.y, this->rotation.z);
    this->frame[0]->SetScale(this->scale[0], this->scale[1], this->scale[2]);
    MGDraw(this->frame[0]);
}

void CObjectFrame::DrawLOD(float *distance, int lowest, int highest, int *out_level) {
    if (this->draw_on == 0) {
        return;
    }

    if (this->frame[0] == nullptr) {
        return;
    }

    lowest = std::max(lowest, 0);
    highest = std::min(highest, 3);
    highest = std::max(highest, lowest);

    int lod = 0;

    if (lowest == highest) {
        lod = lowest;

        while (lod >= 0) {
            if (this->frame[lod] != nullptr) {
                break;
            }

            lod--;
        }
    } else {
        // Distance is measured in eye space, so the frame needs the object's
        // position and angle before its matrix is read.
        this->frame[0]->SetPosition(this->pos);
        this->frame[0]->SetRotation(this->rotation.x, this->rotation.y, this->rotation.z);
        float local_to_world[4][4];
        float local_to_eye[4][4];
        this->frame[0]->GetLWMatrix(local_to_world);
        MulMatrix(local_to_eye, mgRenderInfo.view_scaled, local_to_world);
        float eye_distance = local_to_eye[3][0] * local_to_eye[3][0] + local_to_eye[3][1] * local_to_eye[3][1] + local_to_eye[3][2] * local_to_eye[3][2];

        int level = 0;

        for (lod = 0; lod < 4; lod++) {
            if (eye_distance < distance[lod] * distance[lod]) {
                break;
            }

            level = lod;
        }

        level = std::max(level, lowest);
        level = std::min(level, highest);

        lod = level;

        while (lod >= 0) {
            if (this->frame[lod] != nullptr) {
                break;
            }

            lod--;
        }
    }

    if (this->frame[lod] != nullptr) {
        this->frame[lod]->SetPosition(this->pos);
        this->frame[lod]->SetRotation(this->rotation.x, this->rotation.y, this->rotation.z);
        this->frame[lod]->SetScale(this->scale[0], this->scale[1], this->scale[2]);
        MGDraw(this->frame[lod]);
    }

    if (out_level != nullptr) {
        *out_level = lod;
    }
}

void CObjectFrame::Initialize(CFrameVu1 *frame) {
    CObject::Initialize(1.0f);
    this->rotation_changed = false;
    this->draw_on = 1;
    this->frame[0] = frame;

    for (int i = 1; i < 4; i++) {
        this->frame[i] = nullptr;
    }

    this->scale[0] = 1.0f;
    this->scale[1] = 1.0f;
    this->scale[2] = 1.0f;
}

} // namespace dc::game
