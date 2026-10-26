#pragma helper_mask_gpr 0x30
#pragma helper_mask_fpr 0x1000

#include "objectframe.hpp"

#include "framevu1.hpp"
#include "mathutil.hpp"
#include "mglib.hpp"

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
}

void CObjectFrame::Draw() {
    if (this->draw_on == 0) {
        return;
    }

    if (this->frame[0] == NULL) {
        return;
    }

    this->frame[0]->SetPosition(this->pos);
    this->frame[0]->SetRotation(this->rotation.x, this->rotation.y, this->rotation.z);
    this->frame[0]->SetScale(this->scale[0], this->scale[1], this->scale[2]);
    MGDraw(this->frame[0]);
}

void CObjectFrame::DrawLOD(float *distance, int lowest, int highest, int *out_level) {
    float local_to_world[4][4];
    float local_to_eye[4][4];
    int   lod;
    int   level;
    float eye_distance;

    if (this->draw_on == 0) {
        return;
    }

    if (this->frame[0] == NULL) {
        return;
    }

    // Keep the range of levels within the four the object holds.
    if (lowest < 0) {
        lowest = 0;
    }

    if (highest >= 4) {
        highest = 3;
    }

    if (highest < lowest) {
        highest = lowest;
    }

    if (lowest == highest) {
        lod = lowest;

        while (lod >= 0) {
            if (this->frame[lod] != NULL) {
                break;
            }

            lod--;
        }
    } else {
        // The level comes from how far the object is from the eye. The frame
        // carries the position and the angle so that its matrix gives the
        // distance the eye sees, not the distance along the world axes.
        this->frame[0]->SetPosition(this->pos);
        this->frame[0]->SetRotation(this->rotation.x, this->rotation.y, this->rotation.z);
        this->frame[0]->GetLWMatrix(local_to_world);
        MulMatrix(local_to_eye, mgRenderInfo.view_scaled, local_to_world);
        eye_distance = local_to_eye[3][0] * local_to_eye[3][0] + local_to_eye[3][1] * local_to_eye[3][1] + local_to_eye[3][2] * local_to_eye[3][2];

        level = 0;

        for (lod = 0; lod < 4; lod++) {
            if (eye_distance < distance[lod] * distance[lod]) {
                break;
            }

            level = lod;
        }

        if (level <= lowest) {
            level = lowest;
        }

        if (level >= highest) {
            level = highest;
        }

        lod = level;

        while (lod >= 0) {
            if (this->frame[lod] != NULL) {
                break;
            }

            lod--;
        }
    }

    if (this->frame[lod] != NULL) {
        this->frame[lod]->SetPosition(this->pos);
        this->frame[lod]->SetRotation(this->rotation.x, this->rotation.y, this->rotation.z);
        this->frame[lod]->SetScale(this->scale[0], this->scale[1], this->scale[2]);
        MGDraw(this->frame[lod]);
    }

    if (out_level != NULL) {
        *out_level = lod;
    }
}

void CObjectFrame::Initialize(CFrameVu1 *frame) {
    int i;

    CObject::Initialize(1.0f);
    this->rotation_changed = false;
    this->draw_on = 1;
    this->frame[0] = frame;

    for (i = 1; i < 4; i++) {
        this->frame[i] = NULL;
    }

    this->scale[0] = 1.0f;
    this->scale[1] = 1.0f;
    this->scale[2] = 1.0f;
}
