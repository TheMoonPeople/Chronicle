#pragma helper_mask_gpr 0x30
#pragma helper_mask_fpr 0x1000

#include "mapparts.hpp"

#include <libvu0.h>

#include "collision.hpp"
#include "editpartsinfo.hpp"
#include "frame.hpp"
#include "framevu1.hpp"
#include "mathutil.hpp"
#include "mglib.hpp"
#include "objanime.hpp"

void CMapParts::GetPosition(float *out_position) {
    sceVu0CopyVector(out_position, this->pos);
}

void CMapParts::SetPosition(float *position) {
    sceVu0CopyVector(this->pos, position);
}

void CMapParts::SetPosition(float x, float y, float z) {
    this->pos[0] = x;
    this->pos[1] = y;
    this->pos[2] = z;
}

void CMapParts::SetRotation(float x, float y, float z) {
    this->rotation.x = x;
    this->rotation.y = y;
    this->rotation.z = z;
}

void CMapParts::SetRotY(int direction) {
    float angle;
    float zero = 0.0f;

    // A part faces one of four directions, so each step is a quarter turn.
    this->rot_y = direction;
    angle = 0.5f * (PI * direction);
    SetRotation(zero, angle, 0.0f);
}

int CMapParts::GetRotY() {
    return this->rot_y;
}

void CMapParts::Initialize() {
    int i;

    CMapObject::Initialize();
    this->info = NULL;
    this->preview_frame = 0;
    this->blocked_preview_frame = 0;
    this->ripple_frame = NULL;
    this->unk_1D4 = 0;
    this->unit_size = 0.0f;
    this->rot_y = 0;
    this->def_parts_no = 0;
    this->subtype = MAP_PARTS_SUBTYPE_NONE;
    this->parts_no = this->area = -1;
    this->lift = 0.0f;
    this->draw_distance = -1.0f;

    for (i = 0; i < MAP_PARTS_EFFECT_MAX; i++) {
        this->effect_on[i] = 0;
        this->effect[i] = NULL;
    }

    this->func_count = 0;
    this->func_data = 0;
}

CMapParts::CMapParts() {
    Initialize();
}

void CMapParts::FrameObjectOnOff(char *name, int on) {
    CFrame *found;

    CMapObject::FrameObjectOnOff(name, on);

    if (this->ripple_frame != NULL) {
        found = this->ripple_frame->SearchFrame(name);

        if (found != NULL) {
            found->attr.draw_on = on;
        }
    }
}

int CMapParts::GetWidth() {
    EDITPARTS_INFO *parts_info;

    parts_info = this->info;

    if (parts_info == NULL) {
        return 1;
    }

    // A part that faces east or west covers the grid the other way round.
    if (this->rot_y % 2) {
        return parts_info->height;
    }

    return parts_info->width;
}

int CMapParts::GetHeight() {
    EDITPARTS_INFO *parts_info;

    parts_info = this->info;

    if (parts_info == NULL) {
        return 1;
    }

    if (this->rot_y % 2) {
        return parts_info->width;
    }

    return parts_info->height;
}

int CMapParts::GetInfoData(int x, int y) {
    int width;
    int height;
    int value;

    if (this->info == NULL) {
        return 0;
    }

    value = 0;
    width = GetWidth();
    height = GetHeight();

    // Every direction reads the same grid, from the corner that the direction
    // puts first and along the axis that it puts first.
    switch (this->rot_y) {
        case -1:
            value = this->info->header->cell[y + height * (width - x - 1)];
            break;
        case 0:
            value = this->info->header->cell[x + y * width];
            break;
        case 1:
            value = this->info->header->cell[x * height + (height - y) - 1];
            break;
        case 2:
            value = this->info->header->cell[(width - x) + width * (height - y - 1) - 1];
            break;
    }

    return value;
}

int CMapParts::ChangeAltData() {
    int   i;
    short cell;

    if (this->info == NULL) {
        return 0;
    }

    if (this->info->header == NULL) {
        return 0;
    }

    for (i = 0; i < this->info->width * this->info->height; i++) {
        cell = this->info->header->cell[i];

        if (cell != 0 && cell < 0x80) {
            return 1;
        }
    }

    return 0;
}

int CMapParts::ChangeDigData() {
    int   i;
    short cell;

    if (this->info == NULL) {
        return 0;
    }

    if (this->info->header == NULL) {
        return 0;
    }

    for (i = 0; i < this->info->width * this->info->height; i++) {
        cell = this->info->header->cell[i];

        if (cell != 0 && cell == 0x81) {
            return 1;
        }
    }

    return 0;
}

int CMapParts::CheckBox(CBoxVu0 *box) {
    float         world[4][4];
    sceVu0FVECTOR min;
    sceVu0FVECTOR max;

    // The extent of the part is held in its own axes, so it takes the
    // position and the angle of the part before it meets the box.
    RotMatrixY(world, this->rotation.y);
    sceVu0CopyVector(world[3], this->pos);
    world[3][3] = 1.0f;
    this->bound.min[3] = 1.0f;
    this->bound.max[3] = 1.0f;
    sceVu0ApplyMatrix(max, world, this->bound.max);
    sceVu0ApplyMatrix(min, world, this->bound.min);
    VectorMaxMin(max, min, max, min);

    if (max[0] < box->min[0]) {
        return 0;
    }

    if (max[2] < box->min[2]) {
        return 0;
    }

    if (min[0] > box->max[0]) {
        return 0;
    }

    if (min[2] > box->max[2]) {
        return 0;
    }

    return 1;
}

int CMapParts::CheckBox2(CBoxVu0 *box) {
    float         world[4][4];
    sceVu0FVECTOR corner[4];
    sceVu0FVECTOR max;
    sceVu0FVECTOR min;

    RotMatrixY(world, this->rotation.y);

    // All four corners take part, so that a part turned away from the axes
    // still meets the box on the ground it really covers.
    sceVu0CopyVector(corner[0], this->bound.min);
    sceVu0CopyVector(corner[1], this->bound.min);
    sceVu0CopyVector(corner[2], this->bound.max);
    sceVu0CopyVector(corner[3], this->bound.max);
    corner[1][2] = this->bound.max[2];
    corner[2][2] = this->bound.min[2];

    sceVu0CopyVector(world[3], this->pos);
    world[3][3] = 1.0f;
    this->bound.min[3] = 1.0f;
    this->bound.max[3] = 1.0f;
    sceVu0ApplyMatrix(corner[0], world, corner[0]);
    sceVu0ApplyMatrix(corner[1], world, corner[1]);
    sceVu0ApplyMatrix(corner[2], world, corner[2]);
    sceVu0ApplyMatrix(corner[3], world, corner[3]);
    VectorMaxMin(max, min, corner[0], corner[1], corner[2], corner[3]);

    if (max[0] < box->min[0]) {
        return 0;
    }

    if (max[2] < box->min[2]) {
        return 0;
    }

    if (min[0] > box->max[0]) {
        return 0;
    }

    if (min[2] > box->max[2]) {
        return 0;
    }

    return 1;
}

void CMapParts::DrawParts(float time, float *distance, int lowest, int highest, int *out_level) {
    sceVu0FVECTOR     ambient;
    sceVu0FVECTOR     saved_ambient;
    float             light_direction[4][4];
    float             light_colour[4][4];
    float             saved_light_colour[4][4];
    float             saved_light_direction[4][4];
    sceVu0FVECTOR     part_pos;
    sceVu0FVECTOR     part_rotation;
    float             parts_distance[4];
    sceVu0FVECTOR     effect_pos;
    sceVu0FVECTOR     direction;
    int               lit;
    int               light;
    int               i;
    EDIT_EFFECT_INFO *effect_info;
    float             extent;

    if (this->draw_on == 0) {
        return;
    }

    lit = 0;
    light = 3;

    // A part that covers more ground goes to a coarser level of detail later,
    // so that its size rather than its distance decides.
    extent = this->bound.max[0] - this->bound.min[0];
    extent += this->bound.max[2] - this->bound.min[2];
    extent *= 0.25f;

    for (i = 0; i < 4; i++) {
        parts_distance[i] = extent + distance[i];
    }

    for (i = 0; i < MAP_PARTS_EFFECT_MAX; i++) {
        if (this->effect_on[i] < 0) {
            break;
        }

        if (this->effect_on[i] == 0) {
            continue;
        }

        effect_info = this->effect[i];

        if (effect_info == NULL) {
            continue;
        }

        if (effect_info->kind != EDIT_EFFECT_UNK_5) {
            continue;
        }

        if (!CheckEditEffect(effect_info, time)) {
            continue;
        }

        if (effect_info->frame == NULL) {
            continue;
        }

        // The first effect that lights the part takes the lights that the
        // scene had, so that the last one can put them back.
        if (!lit) {
            MGGetAmbient(ambient);
            MGGetPLight(light_direction, light_colour);
            sceVu0CopyMatrix(saved_light_colour, light_colour);
            sceVu0CopyMatrix(saved_light_direction, light_direction);
            sceVu0CopyVector(saved_ambient, ambient);
            sceVu0CopyVector(part_pos, this->pos);
            sceVu0CopyVector(part_rotation, (float *) &this->rotation);

            if (this->frame[0] != NULL) {
                this->frame[0]->SetPosition(part_pos);
                this->frame[0]->SetRotation(part_rotation[0], part_rotation[1], part_rotation[2]);
            }

            lit = 1;
        }

        effect_info->frame->GetWorldPosition(effect_pos, effect_info->offset);
        sceVu0SubVector(direction, effect_pos, part_pos);
        sceVu0Normalize(direction, direction);
        light_direction[0][light] = direction[0];
        light_direction[1][light] = direction[1];
        light_direction[2][light] = direction[2];
        light_colour[light][0] = effect_info->colour[0];
        light_colour[light][1] = effect_info->colour[1];
        light_colour[light][2] = effect_info->colour[2];
        light_colour[light][3] = 128.0f;

        light--;

        if (light < 0) {
            break;
        }
    }

    if (lit) {
        MGSetPLight(light_direction, light_colour);
    }

    CMapParts::DrawLOD(parts_distance, lowest, highest, out_level);

    if (lit) {
        MGSetAmbient(saved_ambient);
        MGSetPLight(saved_light_direction, saved_light_colour);
    }
}

void CMapParts::DrawEffect(CCamera *camera, float time, CEffectGroup *group) {
    sceVu0FVECTOR     part_pos;
    sceVu0FVECTOR     part_rotation;
    int               i;
    EDIT_EFFECT_INFO *effect_info;

    if (this->handle < 0) {
        return;
    }

    if (this->draw_on == 0) {
        return;
    }

    for (i = 0; i < MAP_PARTS_EFFECT_MAX; i++) {
        if (this->effect_on[i] < 0) {
            break;
        }

        if (this->effect_on[i] == 0) {
            continue;
        }

        effect_info = this->effect[i];

        if (effect_info == NULL) {
            continue;
        }

        if (!CheckEditEffect(effect_info, time)) {
            continue;
        }

        sceVu0CopyVector(part_pos, this->pos);
        sceVu0CopyVector(part_rotation, (float *) &this->rotation);

        if (this->frame[0] != NULL) {
            this->frame[0]->SetPosition(part_pos);
            this->frame[0]->SetRotation(part_rotation[0], part_rotation[1], part_rotation[2]);
        }

        DrawEditEffect(effect_info, camera, group);
    }
}
