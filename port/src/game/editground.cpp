#include "stubs/editground.hpp"
#pragma helper_mask_gpr 0x30
#pragma helper_mask_fpr 0x1000

#include "editground.hpp"

#include <cstdlib>
#include <cstring>

#include "camera.hpp"
#include "camerafollow.hpp"
#include "dataread.hpp"
#include "editarea.hpp"
#include "editpartsinfo.hpp"
#include "frame.hpp"
#include "framevu1.hpp"
#include "mapparts.hpp"
#include "mathutil.hpp"
#include "mglib.hpp"
#include "objanime.hpp"
#include "rect.hpp"
#include "savedata.hpp"
#include "vector3.hpp"

static int CheckDelete(CEditArea *area, CMapParts *parts, float x, float y, float z);

int CEditGround::SetMapParts(int parts_no, float x, float y, float z, int rot_y) {
    CVector3_f_     grid;
    int             area_no;
    int             span[2];
    int             deleted_parts_no;
    int             deleted_rot_y;
    int             width;
    int             height;
    int             i;
    int             j;
    CMapParts      *source;
    int             parts_id;
    CEditArea     **slot;
    CMapParts      *target;
    CEditArea      *area;
    int             placeable;
    EDITPARTS_INFO *info;

    if (CheckEffect()) {
        return -1;
    }

    if (parts_no < 0 || parts_no >= 24) {
        return -1;
    }

    if (plot_parts == NULL) {
        return -1;
    }

    source = &plot_parts[parts_no];

    if (source->handle < 0) {
        return -1;
    }

    area_no = GetAreaCode(x, y, z);

    if (area_no < 0) {
        return -1;
    }

    CEditArea **area_list = areas;
    slot = &area_list[area_no];
    area = *slot;

    if (source->info == NULL) {
        return -1;
    }

    placeable = area->CheckParts(source, x, y, z, rot_y);
    target = parts;
    parts_id = -1;

    switch (source->subtype) {
        case MAP_PARTS_SUBTYPE_BRIDGE:
        case MAP_PARTS_SUBTYPE_ON_RIVER:
            // A bridge or a crossing replaces the river or road piece beneath it.
            parts_id = (*slot)->SearchPartsID(x, y, z);

            if (parts_id >= 0 && CheckDelete(area, source, x, y, z) == 0) {
                if (parts[parts_id].subtype == MAP_PARTS_SUBTYPE_RIVER && parts[parts_id].handle == 1) {
                    target = &parts[parts_id];
                    rot_y = target->GetRotY();

                    if (source->subtype != MAP_PARTS_SUBTYPE_ON_RIVER) {
                        source = &river_parts[6];
                    }

                    parts_no = source->parts_no;
                } else {
                    return -1;
                }
            } else if (source->subtype != MAP_PARTS_SUBTYPE_ROAD) {
                return -1;
            }

            break;
        default:
            if (placeable == 0) {
                return parts_id;
            }

            break;
    }

    if (parts_info != NULL) {
        info = parts_info->GetPartsInfo(parts_no);

        if (info != NULL) {
            if (info->placed == info->stock) {
                return -1;
            }

            info->placed++;
        }
    }

    if (parts_id < 0) {
        for (parts_id = 0; parts_id < 128; target++, parts_id++) {
            if (target->handle < 0) {
                break;
            }
        }

        if (parts_id == 128) {
            return -1;
        }
    }

    if (placeable != 0 && source->subtype != MAP_PARTS_SUBTYPE_ROAD) {
        span[0] = source->GetWidth();
        span[1] = source->GetHeight();

        for (i = 0; i < span[0]; i++) {
            for (j = 0; j < span[1]; j++) {
                float cell_x = x - (float) ((span[0] >> 1) - i) * area->GetUnitSize();
                float cell_z = z - (float) ((span[1] >> 1) - j) * area->GetUnitSize();

                if (area->SearchPartsExtra(cell_x, y, cell_z) == 1) {
                    DeleteMapParts(&deleted_parts_no, &deleted_rot_y, cell_x, y, cell_z);
                }
            }
        }
    }

    memcpy(target, source, sizeof(CMapParts));
    target->SetRotY(rot_y);
    (*slot)->GetGrid(&grid, x, y, z);
    width = source->GetWidth();
    height = source->GetHeight();

    if (width % 2 == 1) {
        grid.x += 0.5f * (*slot)->GetUnitSize();
    }

    if (height % 2 == 1) {
        grid.z += 0.5f * (*slot)->GetUnitSize();
    }

    grid.y = (*slot)->GetAlt(grid.x, grid.y, grid.z);
    target->unit_size = (*slot)->GetUnitSize();
    sceVu0FVECTOR position = {0.0f, 0.0f, 0.0f, 1.0f};
    position[0] = grid.x;
    position[1] = grid.y;
    position[2] = grid.z;
    target->SetPosition(position);
    target->SetRotY(rot_y);
    target->area = area_no;
    (*slot)->SetMapParts(parts_id, parts, x, y, z, rot_y);

    if (target->subtype == MAP_PARTS_SUBTYPE_RIVER && target->handle != 6) {
        SetRiverParts(x, y, z, 0, 0);
        SetRiverParts(x, y, z, 1, 0);
        SetRiverParts(x, y, z, 0, 1);
        SetRiverParts(x, y, z, -1, 0);
        SetRiverParts(x, y, z, 0, -1);
    }

    if (target->subtype == MAP_PARTS_SUBTYPE_ROAD) {
        SetRoadParts(x, y, z, 0, 0);
        SetRoadParts(x, y, z, 1, 0);
        SetRoadParts(x, y, z, 0, 1);
        SetRoadParts(x, y, z, -1, 0);
        SetRoadParts(x, y, z, 0, -1);
    }

    return parts_id;
}

/**
 * Gives back the frame that a map part's shadow draws from.
 */
static inline CFrame *GetShadowFrame(CMapParts *parts) {
    return parts->shadow_frame;
}

/**
 * Gives back the frame that a map part's shade draws from.
 */
static inline CFrame *GetShadeFrame(CMapParts *parts) {
    return parts->shade_frame;
}

/**
 * Gives back the frame that a map part's ripple draws from.
 */
static inline CFrame *GetRippleFrame(CMapParts *parts) {
    return parts->ripple_frame;
}

/**
 * Puts a map part's camera collision frame where the part is and gives it back.
 */
static inline CFrame *GetCameraFrame(CMapParts *parts) {
    if (parts->camera_frame == NULL) {
        return NULL;
    }

    parts->camera_frame->SetPosition(parts->pos[0], parts->pos[1], parts->pos[2]);
    parts->camera_frame->SetRotation(parts->rotation.x, parts->rotation.y, parts->rotation.z);
    return parts->camera_frame;
}

int CEditGround::SetRiverParts(float x, float y, float z, int column_step, int row_step) {
    CVector3_i_ cell;
    CVector3_f_ position;
    int         deleted_parts_no;
    int         deleted_rot_y;

    int area_no = GetAreaCode(x, y, z);

    if (area_no < 0) {
        return 0;
    }

    CEditArea *area = areas[area_no];
    area->GetPos(&cell, x, y, z);
    cell.x += column_step;
    cell.z += row_step;
    int parts_id = area->GetPartsID(cell.x, cell.z);

    if (parts_id < 0) {
        return 0;
    }

    CMapParts *object = &parts[parts_id];
    int        subtype = object->subtype;

    if ((subtype != MAP_PARTS_SUBTYPE_RIVER && subtype != MAP_PARTS_SUBTYPE_BRIDGE && subtype != MAP_PARTS_SUBTYPE_ON_RIVER) || river_parts == NULL) {
        return 0;
    }

    if (subtype == MAP_PARTS_SUBTYPE_BRIDGE || subtype == MAP_PARTS_SUBTYPE_ON_RIVER) {
        area->GetPos(&position, cell.x, cell.y, cell.z);
        DeleteMapParts(&deleted_parts_no, &deleted_rot_y, position.x + 1.0f, position.y, position.z + 1.0f);
    }

    int code = area->SetRiverParts(cell.x, cell.z);

    if (code < 0) {
        return 0;
    }

    int piece = (code & 0xFF0) >> 4;
    int rot_y = code & 0xF;

    if (piece - 1 < 0 || piece - 1 >= 16) {
        return 0;
    }

    if (rot_y >= 3) {
        rot_y = -1;
    }

    for (int i = 0; i < 4; i++) {
        CMapParts *river = &river_parts[(u32) (piece - 1)];
        CFrameVu1 *frame = river->frame[i];
        object->SetFrame(frame, i);
    }

    object->collision_frame = river_parts[(u32) (piece - 1)].GetCollisionFrame();
    object->shadow_frame = GetShadowFrame(&river_parts[(u32) (piece - 1)]);
    object->shade_frame = GetShadeFrame(&river_parts[(u32) (piece - 1)]);
    object->camera_frame = GetCameraFrame(&river_parts[(u32) (piece - 1)]);
    object->ripple_frame = GetRippleFrame(&river_parts[(u32) (piece - 1)]);
    object->handle = river_parts[(u32) (piece - 1)].handle;
    object->subtype = river_parts[(u32) (piece - 1)].subtype;
    object->bound = river_parts[(u32) (piece - 1)].bound;
    object->SetRotY(rot_y);
    return 1;
}

int CEditGround::SetRoadParts(float x, float y, float z, int column_step, int row_step) {
    CVector3_i_ cell;

    int area_no = GetAreaCode(x, y, z);

    if (area_no < 0) {
        return 0;
    }

    CEditArea *area = areas[area_no];
    area->GetPos(&cell, x, y, z);
    cell.x += column_step;
    cell.z += row_step;
    int parts_id = area->GetPartsID(cell.x, cell.z);

    if (parts_id < 0) {
        return 0;
    }

    CMapParts *object = &parts[parts_id];

    if (object->subtype != MAP_PARTS_SUBTYPE_ROAD || road_parts == NULL) {
        return 0;
    }

    int code = area->SetRoadParts(cell.x, cell.z);

    if (code < 0) {
        return 0;
    }

    int piece = (code & 0xFF0) >> 4;
    int rot_y = code & 0xF;

    if (piece - 1 < 0 || piece - 1 >= 6) {
        return 0;
    }

    if (rot_y >= 3) {
        rot_y = -1;
    }

    for (int i = 0; i < 4; i++) {
        CMapParts *road = &road_parts[(u32) (piece - 1)];
        CFrameVu1 *frame = road->frame[i];
        object->SetFrame(frame, i);
    }

    object->collision_frame = road_parts[(u32) (piece - 1)].GetCollisionFrame();
    object->SetRotY(rot_y);
    object->handle = road_parts[(u32) (piece - 1)].handle;
    object->subtype = road_parts[(u32) (piece - 1)].subtype;
    object->bound = road_parts[(u32) (piece - 1)].bound;
    return 1;
}

int CEditGround::DeleteMapParts(int *out_parts_no, int *out_rot_y, float x, float y, float z) {
    int area_code = GetAreaCode(x, y, z);

    if (area_code < 0) {
        return -1;
    }

    CEditArea *area = areas[area_code];
    int        parts_id = area->SearchPartsID(x, y, z);

    if (parts_id < 0) {
        return -1;
    }

    CMapParts *target = &parts[parts_id];

    if (CheckDelete(area, target, x, y, z) != 0) {
        return -1;
    }

    if (target->subtype == MAP_PARTS_SUBTYPE_BRIDGE || target->subtype == MAP_PARTS_SUBTYPE_ON_RIVER) {
        // A bridge or a crossing leaves the plain river piece behind.
        sceVu0FVECTOR position;
        int           rot_y = target->GetRotY();
        target->GetPosition(position);
        int plot = target->parts_no;
        *out_parts_no = plot;
        memcpy(target, &river_parts[1], sizeof(CMapParts));
        target->SetPosition(position);
        target->SetRotY(rot_y);
        area->SetMapParts(parts_id, parts, x, y, z, rot_y);

        if (parts_info != NULL) {
            EDITPARTS_INFO *info = parts_info->GetPartsInfo(plot);

            if (info != NULL) {
                info->placed--;

                if (info->placed < 0) {
                    info->placed = false;
                }
            }
        }

        return parts_id;
    }

    if (parts_info != NULL) {
        EDITPARTS_INFO *info = parts_info->GetPartsInfo(parts[parts_id].parts_no);

        if (info != NULL) {
            info->placed--;

            if (info->placed < 0) {
                info->placed = false;
            }
        }
    }

    sceVu0FVECTOR position;
    parts[parts_id].GetPosition(position);
    area->DeleteMapParts(parts_id, parts, (float) (1.0f + position[0]), position[1], (float) (1.0f + position[2]));

    if (parts[parts_id].subtype == MAP_PARTS_SUBTYPE_RIVER) {
        SetRiverParts(position[0], position[1], position[2], 1, 0);
        SetRiverParts(position[0], position[1], position[2], 0, 1);
        SetRiverParts(position[0], position[1], position[2], -1, 0);
        SetRiverParts(position[0], position[1], position[2], 0, -1);
    }

    if (parts[parts_id].subtype == MAP_PARTS_SUBTYPE_ROAD) {
        SetRoadParts(position[0], position[1], position[2], 1, 0);
        SetRoadParts(position[0], position[1], position[2], 0, 1);
        SetRoadParts(position[0], position[1], position[2], -1, 0);
        SetRoadParts(position[0], position[1], position[2], 0, -1);
    }

    *out_parts_no = parts[parts_id].parts_no;
    *out_rot_y = parts[parts_id].rot_y;
    parts[parts_id].Initialize();
    parts[parts_id].handle = -1;
    return parts_id;
}

int CEditGround::GetAreaCode(float x, float y, float z) {
    for (int i = 0; i < 4; i++) {
        if (areas[i] == NULL) {
            break;
        }

        if (areas[i]->CheckArea(x, y, z)) {
            return i;
        }
    }

    return -1;
}

float CEditGround::GetAlt(float x, float y, float z) {
    int area_no = GetAreaCode(x, y, z);

    if (area_no < 0) {
        return 0.0f;
    }

    return areas[area_no]->GetAlt(x, y, z);
}

int CEditGround::GetAlt_i(float x, float y, float z) {
    int area_no = GetAreaCode(x, y, z);

    if (area_no < 0) {
        return 0;
    }

    return areas[area_no]->GetAlt_i(x, y, z);
}

CMapParts *CEditGround::GetPartsObject(int parts_no) {
    CMapParts *object = parts;

    for (int i = 0; i < 128; i++, object++) {
        if (object->handle >= 0 && object->parts_no == parts_no) {
            return object;
        }
    }

    return NULL;
}

int CEditGround::GetPartsID(float x, float y, float z) {
    int area_no = GetAreaCode(x, y, z);

    if (area_no < 0) {
        return -1;
    }

    return areas[area_no]->SearchPartsID(x, y, z);
}

CMapParts *CEditGround::GetParts(float x, float y, float z) {
    int parts_id = GetPartsID(x, y, z);

    if (parts_id < 0) {
        return NULL;
    }

    return &parts[parts_id];
}

int CEditGround::CheckEffect() {
    return effect_count > 0;
}

void CEditGround::SetBuildEffect(int parts_id) {
    sceVu0FVECTOR position;

    if (parts_id < 0 || parts_id >= 128) {
        return;
    }

    if (parts[parts_id].subtype != MAP_PARTS_SUBTYPE_NONE) {
        return;
    }

    effect_parts_id = parts_id;
    effect_count = 15;
    parts[parts_id].GetPosition(position);
    effect_target_alt = position[1];
    position[1] += 50.0f;
    parts[parts_id].SetPosition(position);
    effect_alt = position[1];
    effect_step = (effect_target_alt - effect_alt) / effect_count;
}

void CEditGround::EffectTask() {
    sceVu0FVECTOR position;
    int           stop = effect_parts_id < 0 || effect_parts_id >= 128;
    stop = (bool) stop || effect_count <= 0;
    stop = (bool) stop || parts[effect_parts_id].handle < 0;

    if (stop) {
        effect_parts_id = -1;
        effect_count = -1;
        return;
    }

    effect_count--;
    parts[effect_parts_id].GetPosition(position);
    position[1] += effect_step;
    parts[effect_parts_id].SetPosition(position);

    if (effect_count <= 0) {
        position[1] = effect_target_alt;
        parts[effect_parts_id].SetPosition(position);
    }
}

int CEditGround::SetFocusParts(float x, float y, float z) {
    focus_parts_id = -1;
    int area_no = GetAreaCode(x, y, z);

    if (area_no < 0) {
        return -1;
    }

    CEditArea *area = areas[area_no];
    int        parts_id = area->SearchPartsID(x, y, z);

    if (parts_id < 0) {
        return -1;
    }

    CMapParts *focus = &parts[parts_id];

    if (CheckDelete(area, focus, x, y, z)) {
        return -1;
    }

    focus_parts_id = GetPartsID(x, y, z);
    return focus_parts_id;
}

/**
 * Returns the absolute value of a floating-point number.
 */
static inline float Magnitude(float value) {
    return value < 0.0f ? -value : value;
}

void CEditGround::EditAreaClip(CCamera *camera, float range) {
    CBoxVu0       box;
    sceVu0FVECTOR eye = {0.0f, 0.0f, 0.0f, 0.0f};
    sceVu0FVECTOR ref;
    sceVu0FVECTOR dir;
    sceVu0FVECTOR forward;
    float         dists[4];
    float         dots[4];
    sceVu0FVECTOR to_center;
    sceVu0FVECTOR center;
    sceVu0FVECTOR corner_a;
    sceVu0FVECTOR corner_b;
    sceVu0FVECTOR corner_max;
    sceVu0FVECTOR corner_min;
    float         facing;
    int           i;

    if (camera != NULL) {
        camera->GetPos(eye);
        camera->GetRef(ref);
        sceVu0SubVector(dir, ref, eye);

        if (GetAreaCode(eye[0], eye[1], eye[2]) < 0) {
            sceVu0SubVector(forward, ref, eye);
            forward[1] = 0.0f;
            sceVu0Normalize(forward, forward);
            sceVu0ScaleVector(forward, forward, 500.0f);
            sceVu0AddVector(ref, eye, forward);
        }
    } else {
        sceVu0CopyVector(eye, ref);
        sceVu0CopyVector(dir, ref);
    }

    facing = 0.0f;

    for (i = 0; i < 4; i++) {
        CEditArea *area = areas[i];

        if (area == NULL) {
            break;
        }

        area->GetOffset(box.min);
        box.min[1] -= 2.0f * area->GetUnitAlt();
        box.min[3] = 1.0f;
        sceVu0CopyVector(box.max, box.min);
        box.max[0] += area->GetUnitSize() * (float) area->GetWidth();
        box.max[1] += 32.0f * area->GetUnitAlt();
        box.max[2] += area->GetUnitSize() * (float) area->GetHeight();
        dists[i] = -1.0f;
        dots[i] = -1.0f;

        if (MGClipBox(&box)) {
            area_visible[i] = 0;
            continue;
        }

        area_visible[i] = 1;
        sceVu0AddVector(center, box.max, box.min);
        sceVu0ScaleVector(center, center, 0.5f);
        sceVu0SubVector(to_center, center, eye);
        dots[i] = sceVu0InnerProduct(to_center, dir);

        if (dots[i] > 0.0f) {
            facing += 1.0f;
        }

        // The corners of the area on the ground, for the distance from the target to its edge.
        sceVu0CopyVector(corner_max, box.max);
        corner_max[1] = 0.0f;
        sceVu0CopyVector(corner_min, box.min);
        corner_min[1] = 0.0f;
        corner_a[0] = box.max[0];
        corner_a[1] = 0.0f;
        corner_a[2] = box.min[2];
        corner_b[0] = box.min[0];
        corner_b[1] = 0.0f;
        corner_b[2] = box.max[2];
        int side_x = 0;
        int side_z = 0;

        if (ref[0] < box.min[0]) {
            side_x = -1;
        }

        if (ref[0] > box.max[0]) {
            side_x = 1;
        }

        if (ref[2] < box.min[2]) {
            side_z = -1;
        }

        if (ref[2] > box.max[2]) {
            side_z = 1;
        }

        float gap;

        if (side_x < 0 && side_z < 0) {
            dists[i] = DistVector(ref, corner_min);
        } else if (side_x == 0 && side_z < 0) {
            gap = ref[2] - corner_min[2];
            dists[i] = gap < 0.0f ? -gap : gap;
        } else if (side_x > 0 && side_z < 0) {
            dists[i] = DistVector(ref, corner_a);
        } else if (side_x < 0 && side_z == 0) {
            gap = ref[0] - corner_min[0];
            dists[i] = gap < 0.0f ? -gap : gap;
        } else if (side_x == 0 && side_z == 0) {
            dists[i] = 0.0f;
        } else if (side_x > 0 && side_z == 0) {
            gap = ref[0] - corner_max[0];
            dists[i] = gap < 0.0f ? -gap : gap;
        } else if (side_x < 0 && side_z > 0) {
            dists[i] = DistVector(ref, corner_b);
        } else if (side_x == 0 && side_z > 0) {
            gap = ref[2] - corner_max[2];
            dists[i] = gap < 0.0f ? -gap : gap;
        } else if (side_x > 0 && side_z > 0) {
            dists[i] = DistVector(ref, corner_max);
        }

        if (range > 0.0f && dists[i] > range) {
            area_visible[i] = 0;
        }
    }

    if (range < 0.0f) {
        clip_plane[3] = -1.0f;
        return;
    }

    int nearest = -1;
    int nearest_dist = -1;

    for (int j = 0; j < 4; j++) {
        if (areas[j] == NULL) {
            break;
        }

        if (area_visible[j] == 0) {
            continue;
        }

        if (dists[j] < 0.0f) {
            continue;
        }

        if (nearest < 0) {
            nearest = j;
            nearest_dist = dists[j];
        } else if (nearest_dist > dists[j]) {
            nearest = j;
            nearest_dist = dists[j];
        }
    }

    switch (map_no) {
        case TOWN_MATATAKI:
            if (dists[0] > dists[2]) {
                if (area_visible[2]) {
                    area_visible[0] = 0;
                    area_visible[2] = 1;
                }
            } else {
                if (area_visible[0]) {
                    area_visible[0] = 1;
                    area_visible[2] = 0;
                }
            }

            if (range > 0.0f) {
                if (clip_plane[3] < 0.0f || !(clip_plane[3] < 1200.0f)) {
                    clip_plane[0] = eye[0];
                    clip_plane[1] = eye[1];
                    clip_plane[2] = eye[2];
                    clip_plane[3] = 1200.0f;
                }
            } else {
                clip_plane[3] = -1.0f;
            }

            break;
        case TOWN_QUEENS:
            for (int k = 0; k < 4; k++) {
                if (areas[k] == NULL) {
                    break;
                }

                if (k == nearest) {
                    area_visible[k] = 1;
                } else {
                    area_visible[k] = 0;
                }

                if (dists[k] > 0.0f && dists[k] < 600.0f) {
                    area_visible[k] = 1;
                }
            }

            break;
    }
}

int CEditGround::GetRandomPlanePos(sceVu0FVECTOR out_position, sceVu0FVECTOR avoid[], int avoid_count, sceVu0FVECTOR bounds) {
    int           cells[2048];
    sceVu0FVECTOR position;
    CVector3_f_   grid;
    CVector3_f_   chosen;
    int           found = 0;
    int           area_count = 0;
    int           x;
    int           z;
    int           area_no;

    for (x = 0;; x++) {
        if (areas[x] == NULL) {
            break;
        }

        area_count++;
    }

    if (area_count == 0) {
        return 0;
    }

    for (area_no = 0; area_no < area_count; area_no++) {
        int width = areas[area_no]->GetWidth();
        int height = areas[area_no]->GetHeight();

        // The outer two cells of each edge are never chosen.
        for (x = 2; x < width - 2; x++) {
            for (z = 2; z < height - 2; z++) {
                if (areas[area_no]->GetPartsID(x, z) >= 0 && areas[area_no]->GetPartsExtra(x, z) != 1) {
                    continue;
                }

                int blocked = 0;
                areas[area_no]->GetPos(&grid, x, 0, z);
                position[0] = grid.x;
                position[1] = areas[area_no]->GetAlt(x, z);
                position[2] = grid.z;

                if (!(bounds[3] <= 0.0f) && !(DistVector(bounds, position) <= bounds[3])) {
                    blocked = 1;
                }

                for (int i = 0; i < avoid_count; i++) {
                    float distance = DistVector(avoid[i], position);

                    if (distance < avoid[i][3]) {
                        blocked = 1;
                        break;
                    }
                }

                if (areas[area_no]->GetAlt_i(x, z) > 0) {
                    blocked = 1;
                }

                if (!blocked) {
                    cells[found++] = (area_no << 16) | (x | (z << 8));
                }
            }
        }
    }

    if (found == 0) {
        return 0;
    }

    int cell = cells[rand() % found];
    int column = cell & 0xFF;
    int row = (cell >> 8) & 0xFF;
    area_no = (cell >> 16) & 0xFF;
    areas[area_no]->GetPos(&chosen, column, 0, row);
    out_position[0] = chosen.x + 0.5f * areas[area_no]->GetUnitSize();
    out_position[1] = areas[0]->GetAlt(column, row);
    out_position[2] = chosen.z + 0.5f * areas[area_no]->GetUnitSize();
    return 1;
}

int CEditGround::GetNearParts(CMapParts **out_parts, int limit, CBoxVu0 *box, CBoxVu0 *fixed_box) {
    int        i;
    int        count = 0;
    CMapParts *object = parts;

    for (i = 0; i < 128; i++, object++) {
        if (object->handle < 0) {
            continue;
        }

        if (!object->CheckBox(box)) {
            continue;
        }

        if (count >= limit) {
            return count;
        }

        *out_parts++ = object;
        count++;
    }

    if (fixed_box == NULL) {
        fixed_box = box;
    }

    for (i = 0; i < 64; i++) {
        if (fixed_parts[i].handle < 0) {
            continue;
        }

        CMapParts *fixed = &fixed_parts[i];

        if (!fixed->CheckBox2(fixed_box)) {
            continue;
        }

        if (count >= limit) {
            return count;
        }

        *out_parts++ = fixed;
        count++;
    }

    return count;
}

void CEditGround::MakePartsBox() {
    for (int i = 0; i < 4; i++) {
        if (areas[i] != NULL) {
            areas[i]->MakePartsBox();
        }
    }
}

void CEditGround::GetPartsBox(CBoxVu0 *out_box, float x, float y, float z) {
    int area_no = GetAreaCode(x, y, z);

    if (area_no < 0) {
        out_box->max[0] = out_box->max[1] = out_box->max[2] = 0.0f;
        out_box->max[3] = 1.0f;
        out_box->min[0] = out_box->min[1] = out_box->min[2] = 0.0f;
        out_box->min[3] = 1.0f;
        return;
    }

    if (areas[area_no] != NULL) {
        areas[area_no]->GetPartsBox(out_box);
    }
}

EPARTS_FUNC_DATA *CEditGround::GetPeoplePos(int villager, float *out_position) {
    sceVu0FVECTOR transform;
    sceVu0FVECTOR frame_rotation;

    for (int i = 0; i < people_count; i++) {
        EPARTS_FUNC_DATA *marker = people[i];

        if (marker->kind != EPARTS_FUNC_VILLAGER || marker->link_id != villager) {
            continue;
        }

        CMapParts *owner = marker->parts;

        if (owner->parts_no >= 0) {
            owner = GetPartsObject(owner->parts_no);
        }

        if (owner == NULL) {
            return NULL;
        }

        sceVu0CopyVector(out_position, marker->position);
        CFrameVu1 *frame = owner->frame[0];

        if (frame == NULL) {
            return NULL;
        }

        owner->GetPosition(transform);
        frame->SetPosition(transform);
        owner->GetRotation(transform);
        frame->SetRotation(transform[0], transform[1], transform[2]);
        out_position[3] = 1.0f;
        frame->GetWorldPosition(out_position, out_position);
        frame->GetRotation(frame_rotation);
        out_position[3] = AngleLimit(frame_rotation[1] + marker->rotation[1]);
        return marker;
    }

    return NULL;
}

void CEditGround::DrawBaseGround() {
    for (int i = 0; i < 4; i++) {
        if (areas[i] == NULL) {
            break;
        }

        areas[i]->DrawGrid();
    }
}

void CEditGround::StepWater() {
    int           i;
    int           j;
    CGroundWater *surface = water_surfaces;

    for (i = 0; i < 4; i++, surface++) {
        if (surface->draw == 0) {
            continue;
        }

        if (surface->water.CheckClip()) {
            continue;
        }

        for (j = 0; j < 4; j++) {
            if (surface->ripples[j].power == 0.0f && surface->ripples[j].range == 0.0f) {
                break;
            }

            int row = surface->ripples[j].row;
            int column = surface->ripples[j].column;

            if (row < 0) {
                int rows = surface->water.rows;
                row = rows * (float) rand() / 2.1474836e9f;
            }

            if (column < 0) {
                int rows = surface->water.rows;
                column = rows * (float) rand() / 2.1474836e9f;
            }

            surface->water.Shake(row, column, surface->ripples[j].power + surface->ripples[j].range * (float) rand() / 2.1474836e9f);
        }

        surface->water.Hamon();
    }
}

void CEditGround::DrawWaterSurface(CCamera *camera) {
    sceVu0FVECTOR eye;
    sceVu0FVECTOR position;
    sceVu0FVECTOR dir;
    sceVu0FVECTOR parts_position;
    int           i;
    CGroundWater *surface = water_surfaces;

    camera->GetPos(eye);
    camera->GetDir(dir);
    dir[1] = 0.0f;
    sceVu0Normalize(dir, dir);

    for (i = 0; i < 4; i++, surface++) {
        if (surface->draw == 0) {
            continue;
        }

        CWater *water = &surface->water;
        sceVu0CopyVector(position, surface->offset);

        if (surface->parts_no >= 0) {
            CMapParts *owner = GetPartsObject(surface->parts_no);

            if (owner == NULL) {
                continue;
            }

            CFrame *frame = owner->ripple_frame;

            if (frame != NULL && surface->name[0] != '\0') {
                frame = frame->SearchFrame(surface->name);

                if (frame != NULL && !(frame->attr.draw_on & 1)) {
                    continue;
                }
            }

            owner->GetPosition(parts_position);

            if (!(clip_plane[3] <= 0.0f || DistVector(parts_position, clip_plane) <= clip_plane[3] || owner->ChangeDigData())) {
                continue;
            }

            sceVu0AddVector(position, position, parts_position);
            water->frame.SetPosition(position);
            owner->GetRotation(parts_position);
            water->frame.SetRotation(parts_position[0], parts_position[1], parts_position[2]);
        } else {
            if (surface->follow[0]) {
                position[0] = eye[0] + 50.0f * dir[0];
            }

            if (surface->follow[1]) {
                position[1] = eye[1];
            }

            if (surface->follow[2]) {
                position[2] = eye[2] + 50.0f * dir[2];
            }

            CVector3_f_ rotation;
            rotation.z = rotation.y = rotation.x = 0.0f;
            water->frame.SetRotation(rotation.x, rotation.y, rotation.z);
            water->frame.SetPosition(position);
        }

        DrawVu1__6CWaterFP10RenderInfoP13sceVif1PacketP1(water, &mgRenderInfo, GetVif1Packet(), NULL);
    }
}

void CEditGround::DrawWater(int pass) {
    sceVu0FVECTOR position;
    sceVu0FVECTOR corner;
    CMapParts    *object = parts;
    int           i;

    for (i = 0; i < 128; i++, object++) {
        if (suppress_water) {
            break;
        }

        if (object->handle < 0) {
            continue;
        }

        if (object->area >= 0 && area_visible[object->area] == 0) {
            continue;
        }

        CMapParts *water = &river_parts[7];
        object->GetPosition(position);

        switch (object->subtype) {
            case MAP_PARTS_SUBTYPE_RIVER:
            case MAP_PARTS_SUBTYPE_ON_RIVER:
            case MAP_PARTS_SUBTYPE_BRIDGE:
                water->SetPosition(position);
                water->Draw();
                break;
            case MAP_PARTS_SUBTYPE_LAKE:
                if (object->area < 0) {
                    break;
                }

                if (object->GetWidth() != 2) {
                    break;
                }

                if (object->GetHeight() != 2) {
                    break;
                }

                float size = areas[object->area]->GetUnitSize();
                position[0] += 0.5f * size;
                position[2] += 0.5f * size;
                water->SetPosition(position);
                water->Draw();
                sceVu0CopyVector(corner, position);
                corner[0] = position[0] - size;
                water->SetPosition(corner);
                water->Draw();
                corner[2] = position[2] - size;
                water->SetPosition(corner);
                water->Draw();
                corner[0] = position[0];
                water->SetPosition(corner);
                water->Draw();
                break;
        }
    }

    for (int j = 0; j < 64; j++) {
        if (fixed_parts[j].category_no == pass) {
            fixed_parts[j].Draw();
        }
    }
}

void CEditGround::DrawRipple(int pass) {
    sceVu0FVECTOR transform;
    sceVu0FVECTOR position;
    CMapParts    *object = parts;
    int           i;

    for (i = 0; i < 128; i++, object++) {
        if (suppress_water) {
            break;
        }

        if (object->handle < 0) {
            continue;
        }

        if (object->ripple_frame == NULL) {
            continue;
        }

        if (object->draw_on == 0) {
            continue;
        }

        if (object->area >= 0 && area_visible[object->area] == 0) {
            continue;
        }

        if (!(clip_plane[3] <= 0.0f)) {
            object->GetPosition(position);

            if (!(DistVector(position, clip_plane) <= clip_plane[3]) && !object->ChangeDigData()) {
                continue;
            }
        }

        if (object->ripple_frame == NULL) {
            continue;
        }

        object->GetPosition(transform);
        object->ripple_frame->SetPosition(transform);
        object->SetRotY(object->GetRotY());
        object->GetRotation(transform);
        object->ripple_frame->SetRotation(transform[0], transform[1], transform[2]);
        MGDraw(object->ripple_frame);
    }

    for (int j = 0; j < 64; j++) {
        if (fixed_parts[j].category_no != pass) {
            continue;
        }

        if (fixed_parts[j].ripple_frame == NULL) {
            continue;
        }

        if (fixed_parts[j].draw_on == 0) {
            continue;
        }

        fixed_parts[j].GetPosition(transform);
        fixed_parts[j].ripple_frame->SetPosition(transform);
        fixed_parts[j].GetRotation(transform);
        fixed_parts[j].ripple_frame->SetRotation(transform[0], transform[1], transform[2]);
        MGDraw(fixed_parts[j].ripple_frame);
    }
}

void CEditGround::DrawEffect(CCameraFollow *camera, float time, CEffectGroup *effects) {
    sceVu0FVECTOR position;
    int           i;
    CMapParts    *object = parts;

    for (i = 0; i < 128; i++, object++) {
        if (object->area >= 0 && area_visible[object->area] == 0) {
            continue;
        }

        if (!(clip_plane[3] <= 0.0f)) {
            object->GetPosition(position);

            if (!(DistVector(position, clip_plane) <= clip_plane[3])) {
                continue;
            }
        }

        object->DrawEffect(camera, time, effects);
    }

    for (i = 0; i < 64; i++) {
        fixed_parts[i].DrawEffect(camera, time, effects);
    }
}

void CEditGround::Save(char *path) {
    char                buffer[4000];
    sceVu0FVECTOR       position;
    GROUND_SAVE_HEADER *header = (GROUND_SAVE_HEADER *) buffer;
    SV_GRD_PART        *record = (SV_GRD_PART *) (header + 1);

    header->offset = sizeof(GROUND_SAVE_HEADER);
    header->count = 0;

    for (int i = 0; i < 128; i++) {
        CMapParts *object = &parts[i];
        int        parts_id = parts[i].handle;

        if (parts_id < 0) {
            continue;
        }

        int j;
        int kind = object->subtype;

        if (kind > 0) {
            for (j = 0; j < 24; j++) {
                if (kind == plot_parts[j].subtype) {
                    parts_id = plot_parts[j].handle;
                    break;
                }
            }
        }

        object->GetPosition(position);
        record->part_id = parts_id;
        record->rot_y = object->rot_y;
        record->pos_x = position[0];
        record->pos_y = position[1];
        record->pos_z = position[2];
        record++;
        header->count++;
    }

    record->part_id = -1;
    record->rot_y = -1;
    record->pos_x = 0.0f;
    record->pos_y = 0.0f;
    record->pos_z = 0.0f;
    header->size = (char *) (record + 1) - buffer;
    WriteFile("host0:y:/ps2/dc_data/gdata0.edt", buffer, header->size);
}

void CEditGround::Load(char *data) {
    char                buffer[4000];
    GROUND_SAVE_HEADER *header;
    char               *file;
    SV_GRD_PART        *record;
    SV_GRD_PART        *records;
    int                 i;

    Clear();
    file = buffer;

    if (data == NULL) {
        if (!LoadFile2("gdata0.edt", file, NULL, 0)) {
            return;
        }
    } else {
        file = data;
    }

    header = (GROUND_SAVE_HEADER *) file;
    records = (SV_GRD_PART *) (file + header->offset);
    // Parts that follow the ground's height go down first, then the rest, then the
    // parts that stand on others.
    record = records;

    for (i = 0; i < header->count; i++, record++) {
        int parts_id = record->part_id;

        if (parts_id < 0 || parts_id >= 24) {
            break;
        }

        if (plot_parts[parts_id].ChangeAltData()) {
            float x = record->pos_x + 1.0f;
            float z = record->pos_z + 1.0f;
            SetMapParts(record->part_id, x, record->pos_y, z, record->rot_y);
        }
    }

    record = records;

    for (i = 0; i < header->count; i++, record++) {
        int parts_id = record->part_id;

        if (parts_id < 0 || parts_id >= 24) {
            break;
        }

        int kind = plot_parts[parts_id].subtype;

        if (kind == MAP_PARTS_SUBTYPE_BRIDGE || kind == MAP_PARTS_SUBTYPE_ON_RIVER) {
            for (int j = 0; j < 24; j++) {
                if (plot_parts[j].subtype == MAP_PARTS_SUBTYPE_RIVER) {
                    SetMapParts(j, record->pos_x + 1.0f, record->pos_y, record->pos_z + 1.0f, record->rot_y);
                    break;
                }
            }
        } else {
            SetMapParts(parts_id, record->pos_x + 1.0f, record->pos_y, record->pos_z + 1.0f, record->rot_y);
        }
    }

    record = records;

    for (i = 0; i < header->count; i++, record++) {
        int parts_id = record->part_id;

        if (parts_id < 0 || parts_id >= 24) {
            break;
        }

        switch (plot_parts[parts_id].subtype) {
            case MAP_PARTS_SUBTYPE_BRIDGE:
            case MAP_PARTS_SUBTYPE_ON_RIVER:
                SetMapParts(parts_id, record->pos_x + 1.0f, record->pos_y, record->pos_z + 1.0f, record->rot_y);
                break;
        }
    }

    for (i = 0; i < 4; i++) {
        if (areas[i] != NULL) {
            areas[i]->RemakeGrid();
            areas[i]->RemakeGrid();
        }
    }
}

void CEditGround::Save(int town, CSaveData *save) {
    sceVu0FVECTOR position;
    int           count;
    int           i;

    SV_GRD_PART *record = save->GetParts(town, &count);

    if (record == NULL) {
        return;
    }

    for (i = 0; i < 128; i++) {
        CMapParts *object = &parts[i];
        int        parts_id = parts[i].handle;

        if (parts_id < 0) {
            continue;
        }

        int j;
        int kind = object->subtype;

        switch (kind) {
            case MAP_PARTS_SUBTYPE_ROAD:
            case MAP_PARTS_SUBTYPE_RIVER:
            case MAP_PARTS_SUBTYPE_BRIDGE:
                for (j = 0; j < 24; j++) {
                    if (kind == plot_parts[j].subtype) {
                        parts_id = plot_parts[j].handle;
                        break;
                    }
                }

                break;
        }

        object->GetPosition(position);
        record->part_id = parts_id;
        record->rot_y = object->rot_y;
        record->pos_x = position[0];
        record->pos_y = position[1];
        record->pos_z = position[2];
        record++;
    }

    record->part_id = -1;
    record->rot_y = -1;
    record->pos_x = 0.0f;
    record->pos_y = 0.0f;
    record->pos_z = 0.0f;
}

void CEditGround::Load(int town, CSaveData *save) {
    char                buffer[0x5000];
    int                 count;
    GROUND_SAVE_HEADER *header = (GROUND_SAVE_HEADER *) buffer;
    SV_GRD_PART        *record = (SV_GRD_PART *) &buffer[sizeof(GROUND_SAVE_HEADER)];

    SV_GRD_PART *saved = save->GetParts(town, &count);

    if (saved == NULL) {
        return;
    }

    header->count = count;
    header->offset = sizeof(GROUND_SAVE_HEADER);

    for (int i = 0; i < count; saved++, record++, i++) {
        record->part_id = saved->part_id;
        record->rot_y = saved->rot_y;
        record->pos_x = saved->pos_x;
        record->pos_y = saved->pos_y;
        record->pos_z = saved->pos_z;
    }

    record->part_id = -1;
    record->rot_y = -1;
    Load(buffer);
}

int CEditGround::PickUpPoly(CCPoly *polygons, float x, float y, float z) {
    CBoxVu0 box;

    box.max[0] = x + 30.0f;
    box.min[0] = x - 30.0f;
    box.max[2] = z + 30.0f;
    box.min[2] = z - 30.0f;
    box.max[1] = y + 100.0f;
    box.min[1] = y - 100.0f;
    return PickUpPoly(polygons, box, 0);
}

int CEditGround::PickUpPoly(CCPoly *out_polygons, CBoxVu0 box, int flags) {
    int        i;
    int        count = 0;
    CCPoly    *polygons = out_polygons;
    CMapParts *object = parts;
    int        found;
    CFrame    *frame;

    for (i = 0; i < 128; i++, object++) {
        if (object->handle < 0) {
            continue;
        }

        if (flags != 0 && object->subtype == MAP_PARTS_SUBTYPE_NONE) {
            continue;
        }

        frame = object->GetCollisionFrame();

        if (frame == NULL) {
            continue;
        }

        if (!object->CheckBox(&box)) {
            continue;
        }

        found = frame->PickUpNearPoly(polygons, box);
        polygons += found;
        count += found;
    }

    for (i = 0; i < 64; i++) {
        if (fixed_parts[i].handle < 0) {
            continue;
        }

        CMapParts *fixed = &fixed_parts[i];
        frame = fixed->GetCollisionFrame();

        if (frame == NULL) {
            continue;
        }

        if (!fixed->CheckBox2(&box)) {
            continue;
        }

        found = frame->PickUpNearPoly(polygons, box);
        polygons += found;
        count += found;
    }

    for (i = 0; i < 4; i++) {
        if (areas[i] != NULL) {
            found = areas[i]->PickUpPoly(polygons, box);
            polygons += found;
            count += found;
        }
    }

    return count;
}

int CEditGround::PickUpEditAreaPoly(CCPoly *polygons, float x, float y, float z) {
    int area_no = GetAreaCode(x, y, z);

    if (area_no < 0) {
        return 0;
    }

    return areas[area_no]->PickUpPoly(polygons, x, y, z);
}

int CEditGround::PickUpCameraPoly(CCPoly *out_polygons, CBoxVu0 &box, int flags) {
    int           i;
    int           count = 0;
    CCPoly       *polygons = out_polygons;
    CMapParts    *object = parts;
    CFrame       *frame;
    int           found;
    sceVu0FVECTOR center;

    for (i = 0; i < 128 && (flags & 2); i++, object++) {
        if (object->handle < 0) {
            continue;
        }

        frame = GetCameraFrame(object);

        if (frame == NULL) {
            continue;
        }

        if (!object->CheckBox(&box)) {
            continue;
        }

        found = frame->PickUpNearPoly(polygons, box);
        polygons += found;
        count += found;
    }

    for (i = 0; i < 64 && (flags & 1); i++) {
        if (fixed_parts[i].handle < 0) {
            continue;
        }

        frame = GetCameraFrame(&fixed_parts[i]);

        if (frame == NULL) {
            continue;
        }

        if (!fixed_parts[i].CheckBox(&box)) {
            continue;
        }

        found = frame->PickUpNearPoly(polygons, box);
        polygons += found;
        count += found;
    }

    for (i = 0; i < 4; i++) {
        if (areas[i] != NULL) {
            sceVu0AddVector(center, box.max, box.min);
            sceVu0ScaleVector(center, center, 0.5f);
            count += areas[i]->PickUpPoly(polygons, center[0], center[1], center[2]);
        }
    }

    return count;
}

void CEditGround::Clear() {
    int i;

    for (i = 0; i < 128; i++) {
        parts[i].Initialize();
    }

    for (i = 0; i < 4; i++) {
        if (areas[i] != NULL) {
            areas[i]->Clear();
        }

        area_visible[i] = 1;
    }

    effect_count = -1;
    effect_parts_id = -1;
    effect_alt = 0.0f;
    effect_target_alt = 0.0f;
    effect_step = 0.0f;

    if (parts_info != NULL) {
        parts_info->Clear();
    }

    if (map_no == TOWN_MATATAKI) {
        CVector3_f_     position;
        EDITPARTS_INFO *info = parts_info->GetPartsInfo(16);
        int             saved = info->obtained;

        info->stock += 6;
        info->obtained = 0;
        areas[0]->GetPos(&position, 5, 0, 0);
        SetMapParts(16, position.x, position.y, position.z, 0);
        areas[0]->GetPos(&position, 2, 0, 7);
        SetMapParts(16, position.x, position.y, position.z, 0);
        areas[1]->GetPos(&position, 3, 0, 5);
        SetMapParts(16, position.x, position.y, position.z, 0);
        areas[1]->GetPos(&position, 11, 0, 3);
        SetMapParts(16, position.x, position.y, position.z, 0);
        areas[2]->GetPos(&position, 4, 0, 0);
        SetMapParts(16, position.x, position.y, position.z, 0);
        areas[2]->GetPos(&position, 3, 0, 7);
        SetMapParts(16, position.x, position.y, position.z, 0);
        info->placed = false;
        info->stock -= 6;
        info->obtained = saved;
    }
}

void CEditGround::Initialize() {
    map_no = TOWN_NORUNE;
    plot_parts = NULL;
    river_parts = NULL;
    road_parts = NULL;
    parts_info = NULL;

    for (int area = 0; area < 4; area++) {
        areas[area] = NULL;
        area_visible[area] = 1;
    }

    int i;

    for (i = 0; i < 64; i++) {
        fixed_parts[i].Initialize();
    }

    for (i = 0; i < 1; i++) {
        spare_words[i] = 0;
    }

    for (i = 0; i < 4; i++) {
        water_surfaces[i].draw = false;
    }

    for (i = 0; i < 128; i++) {
        people[i] = NULL;
    }

    people_count = 0;
    suppress_water = false;
    Clear();
    clip_plane[3] = -1.0f;
}

void CEditGround::RemakeGrid() {
    for (int i = 0; i < 4; i++) {
        if (areas[i] != NULL) {
            areas[i]->grid_redraw = 1;
        }
    }
}

CEditGround::CEditGround() {
    cursor.area = 0;
    people[0] = NULL;
    cursor.pieces[1] = NULL;
    cursor.pieces[0] = NULL;
    cursor.unit_size = 1.0f;
    Initialize();
}

/**
 * Reports whether a part may be replaced by the one being placed over it.
 *
 * @mangled CheckDelete__FP9CEditAreaP9CMapPartsfff
 * @address 0x1A5B20
 * @size 0x18C
 */
static int CheckDelete(CEditArea *area, CMapParts *parts, float x, float y, float z) {
    CVector3_i_ position;

    area->GetPos(&position, x, y, z);
    int column = position.x;
    int row = position.z;
    int kind = parts->subtype;

    switch (area->GetMapNo()) {
        case TOWN_MATATAKI:
            if (kind == MAP_PARTS_SUBTYPE_BRIDGE) {
                break;
            }

            switch (area->GetAreaID()) {
                case 0:
                    if (column == 5 && row == 0) {
                        return 1;
                    }

                    if (column == 2 && row == 7) {
                        return 1;
                    }

                    break;
                case 1:
                    if (column == 3 && row == 5) {
                        return 1;
                    }

                    if (column == 11 && row == 3) {
                        return 1;
                    }

                    break;
                case 2:
                    if (column == 4 && row == 0) {
                        return 1;
                    }

                    if (column == 3 && row == 7) {
                        return 1;
                    }

                    break;
            }

            break;
    }

    return 0;
}

void CPartsCursor::Draw(float *position, int width, int height) {
    sceVu0FVECTOR cell;
    sceVu0FVECTOR corner;
    int           i;
    int           j;
    int           columns = width * 2;
    int           rows = height * 2;

    sceVu0CopyVector(corner, position);
    corner[0] -= 0.5f * (unit_size * width);
    corner[2] -= 0.5f * (unit_size * height);
    corner[0] += 0.25f * unit_size;
    corner[2] += 0.25f * unit_size;
    sceVu0CopyVector(cell, corner);

    for (i = 0; i < columns; i++) {
        cell[2] = corner[2];

        for (j = 0; j < rows; j++) {
            int piece = 2;
            int turn = 0;

            if (i == 0 && j == 0) {
                piece = 0;
            }

            if (i > 0 && i < columns - 1 && j == 0) {
                piece = 1;
                turn = 2;
            }

            if (i == columns - 1 && j == 0) {
                piece = 0;
                turn = -1;
            }

            if (i == columns - 1 && j > 0 && j < rows - 1) {
                piece = 1;
                turn = 1;
            }

            if (i == columns - 1 && j == rows - 1) {
                piece = 0;
                turn = 2;
            }

            if (i > 0 && i < columns - 1 && j == rows - 1) {
                piece = 1;
                turn = 0;
            }

            if (i == 0 && j == rows - 1) {
                piece = 0;
                turn = 1;
            }

            if (i == 0 && j > 0 && j < rows - 1) {
                piece = 1;
                turn = -1;
            }

            pieces[piece]->SetPosition(cell);
            pieces[piece]->SetRotation(0.0f, HALF_PI * turn, 0.0f);
            float scale = unit_size / 100.0f;
            pieces[piece]->SetScale(scale, scale, scale);
            MGDraw(pieces[piece]);
            cell[2] += 0.5f * unit_size;
        }

        cell[0] += 0.5f * unit_size;
    }
}

void CEditGround::RequestCheck() {
    CMapParts *placed[24][64];
    int        plot_count[24];
    int        i;

    memset(placed, 0, sizeof(placed));
    CMapParts *object = parts;

    for (i = 0; i < 24; i++) {
        plot_count[i] = 0;
        parts_info->request[i] = 0;
    }

    for (i = 0; i < 128; i++, object++) {
        if (object->handle >= 0) {
            int plot = object->parts_no;

            if (plot >= 0 && plot < 24) {
                placed[plot][plot_count[plot]++] = object;
            }
        }
    }

    switch (map_no) {
        case TOWN_NORUNE:
            NornRequest(placed);
            break;
        case TOWN_MATATAKI:
            MatatagiRequest(placed);
            break;
        case TOWN_QUEENS:
            QueensRequest(placed);
            break;
        case TOWN_MUSKA_LACKA:
            MuskaRequest(placed);
            break;
        case TOWN_YELLOW_DROPS:
            YellowRequest(placed);
            break;
    }

    for (i = 0; i < 24; i++) {
        if (!parts_info->CheckComplete(i)) {
            parts_info->request[i] = 0;
        }
    }
}

int CEditGround::CheckPartsRect(int parts_no, int area_no, CRect_i_ &rect) {
    int parts_ids[256];

    if (area_no < 0 || area_no >= 4) {
        return 0;
    }

    if (areas[area_no] == NULL) {
        return 0;
    }

    int count = areas[area_no]->GetPartsRect(rect, parts_ids, 256);

    for (int i = 0; i < count; i++) {
        if (parts[parts_ids[i]].parts_no == parts_no) {
            return 1;
        }
    }

    return 0;
}

void CEditGround::GetRectParts(CRect_i_ *rect, CMapParts *target, int margin) {
    sceVu0FVECTOR position;
    CVector3_i_   grid;

    *rect = CRect_i_(0, 0, 0, 0);

    if (target == NULL) {
        return;
    }

    target->GetPosition(position);
    int area_no = GetAreaCode(position[0], position[1], position[2]);

    if (area_no < 0 || area_no >= 4) {
        return;
    }

    if (areas[area_no] == NULL) {
        return;
    }

    areas[area_no]->GetPos(&grid, position[0], position[1], position[2]);
    int width = target->GetWidth();
    int height = target->GetHeight();
    int half_width = width >> 1;
    rect->x = grid.x - half_width - margin;
    int half_height = height >> 1;
    rect->y = grid.z - half_height - margin;
    rect->width = width + margin * 2;
    rect->height = height + margin * 2;
}

void CEditGround::GetRectParts(CRect_i_ *rect, CMapParts *target, int column, int row) {
    sceVu0FVECTOR position;
    CVector3_i_   grid;
    int           width;
    int           height;
    int           rot_y;
    int           offset_x;
    int           offset_z;

    *rect = CRect_i_(0, 0, 0, 0);

    if (target == NULL) {
        return;
    }

    target->GetPosition(position);
    int area_no = GetAreaCode(position[0], position[1], position[2]);

    if (area_no < 0 || area_no >= 4) {
        return;
    }

    if (areas[area_no] == NULL) {
        return;
    }

    areas[area_no]->GetPos(&grid, position[0], position[1], position[2]);
    rot_y = target->GetRotY();
    target->SetRotY(0);
    width = target->GetWidth();
    height = target->GetHeight();
    target->SetRotY(rot_y);
    column -= width >> 1;
    row -= height >> 1;
    int adjust_x = 0;
    int adjust_z = 0;

    // A part with an even side has no centre cell, so turning it shifts the cells by one.
    if (width % 2 == 1 && height % 2 == 0) {
        if (rot_y == -1) {
            adjust_x = -1;
        }

        if (rot_y == 2) {
            adjust_z = -1;
        }
    }

    if (width % 2 == 0 && height % 2 == 1) {
        if (rot_y == 1) {
            adjust_z = -1;
        }

        if (rot_y == 2) {
            adjust_x = -1;
        }
    }

    if (width % 2 == 0 && height % 2 == 0) {
        if (rot_y == -1) {
            adjust_x = -1;
        }

        if (rot_y == 1) {
            adjust_z = -1;
        }

        if (rot_y == 2) {
            adjust_x = -1;
            adjust_z = -1;
        }
    }

    switch (target->GetRotY()) {
        case 0:
            offset_x = column;
            offset_z = row;
            break;
        case 1:
            offset_x = row;
            offset_z = -column;
            break;
        case 2:
            offset_x = -column;
            offset_z = -row;
            break;
        case -1:
            offset_x = -row;
            offset_z = column;
            break;
    }

    rect->x = grid.x + offset_x + adjust_x;
    rect->y = grid.z + offset_z + adjust_z;
    rect->width = 1;
    rect->height = 1;
}

/**
 * Gives the first cell that a span covers when it is centred on a cell.
 */
static inline int SpanStart(int &centre, int size) {
    int half = size >> 1;
    return centre - half;
}

void CEditGround::GetRectDirParts(CRect_i_ *rect, CMapParts *target, int direction, int depth) {
    sceVu0FVECTOR position;
    CVector3_i_   grid;

    *rect = CRect_i_(0, 0, 0, 0);

    if (target == NULL) {
        return;
    }

    target->GetPosition(position);
    int area_no = GetAreaCode(position[0], position[1], position[2]);

    if (area_no < 0 || area_no >= 4) {
        return;
    }

    if (areas[area_no] == NULL) {
        return;
    }

    areas[area_no]->GetPos(&grid, position[0], position[1], position[2]);
    int width = target->GetWidth();
    int height = target->GetHeight();
    direction += target->GetRotY();

    if (direction > 2) {
        direction -= 4;
    }

    if (direction < -1) {
        direction += 4;
    }

    if (direction < -1 || direction > 2) {
        return;
    }

    if (direction % 2 != 0) {
        rect->width = depth;
        rect->height = height;

        if (direction == 1) {
            rect->x = SpanStart(grid.x, width) - rect->width;
            rect->y = SpanStart(grid.z, height);
        } else {
            rect->x = width + SpanStart(grid.x, width);
            rect->y = SpanStart(grid.z, height);
        }
    } else {
        rect->width = width;
        rect->height = depth;

        if (direction == 0) {
            rect->x = SpanStart(grid.x, width);
            rect->y = SpanStart(grid.z, height) - rect->height;
        } else {
            rect->x = SpanStart(grid.x, width);
            rect->y = height + SpanStart(grid.z, height);
        }
    }
}

void CEditGround::NornRequest(CMapParts *(*placed)[64]) {
    parts_info->parts_max = 8;

    if (areas[0] == NULL) {
        return;
    }

    if (placed[0][0] != NULL && placed[0][0]->GetRotY() == 1) {
        parts_info->request[0] = 1;
    }

    if (placed[1][0] != NULL) {
        if (CheckPartsRect(1, 0, CRect_i_(10, 0, 4, 5))) {
            parts_info->request[1] = 1;
        }
    }

    if (placed[2][0] != NULL) {
        if (!CheckPartsRect(2, 0, CRect_i_(2, 8, 9, 4))) {
            parts_info->request[2] = 1;
        }
    }

    if (placed[3][0] != NULL) {
        CRect_i_ rect;
        rect.x = rect.y = rect.width = rect.height = 0;
        GetRectParts(&rect, placed[12][0], 2);

        if (CheckPartsRect(3, 0, rect)) {
            parts_info->request[3] = 1;
        }
    }

    if (placed[4][0] != NULL) {
        CRect_i_ rect;
        rect.x = rect.y = rect.width = rect.height = 0;
        GetRectParts(&rect, placed[6][0], 4);

        if (CheckPartsRect(4, 0, rect)) {
            parts_info->request[4] = 1;
        }
    }

    if (placed[5][0] != NULL) {
        CRect_i_ rect;
        rect.x = rect.y = rect.width = rect.height = 0;
        GetRectDirParts(&rect, placed[8][0], 2, 4);

        if (CheckPartsRect(5, 0, rect) && SaveData->GetGameIntFlag(0) >= 100) {
            parts_info->request[5] = 1;
        }
    }

    if (placed[6][0] != NULL) {
        CRect_i_ rect;
        rect.x = rect.y = rect.width = rect.height = 0;
        GetRectParts(&rect, placed[1][0], 4);

        if (!CheckPartsRect(6, 0, rect)) {
            parts_info->request[6] = 1;
        }
    }

    if (placed[7][0] != NULL) {
        int      parts_ids[256];
        CRect_i_ rect;
        rect.x = rect.y = rect.width = rect.height = 0;
        GetRectParts(&rect, placed[7][0], 4);
        int count = areas[0]->GetPartsRect(rect, parts_ids, 256);
        int houses = 0;

        for (int i = 0; i < count; i++) {
            if (parts[parts_ids[i]].parts_no < 7) {
                houses++;
            }

            if (houses >= 2) {
                parts_info->request[7] = 1;
                break;
            }
        }
    }
}

void CEditGround::MatatagiRequest(CMapParts *(*placed)[64]) {
    parts_info->parts_max = 10;

    if (areas[0] == NULL || areas[1] == NULL || areas[2] == NULL) {
        return;
    }

    areas[0]->ChainWorkClear();
    int chained = areas[0]->CheckRiverChain(5, 0, 2, 7);
    areas[1]->ChainWorkClear();
    chained &= areas[1]->CheckRiverChain(3, 5, 11, 3);
    areas[2]->ChainWorkClear();
    chained &= areas[2]->CheckRiverChain(4, 0, 3, 7);

    if (chained) {
        parts_info->request[16] = 1;
    }

    if (placed[0][0] != NULL) {
        if (CheckPartsRect(0, 0, CRect_i_(0, 0, 4, 1))) {
            parts_info->request[0] = 1;
        }

        if (CheckPartsRect(0, 1, CRect_i_(0, 5, 1, 1))) {
            parts_info->request[0] = 1;
        }
    }

    if (placed[1][0] != NULL) {
        int      parts_ids[256];
        CRect_i_ rect;
        rect.x = rect.y = rect.width = rect.height = 0;
        GetRectParts(&rect, placed[1][0], 1);
        int count = areas[placed[1][0]->area]->GetPartsRect(rect, parts_ids, 256);
        int found = 0;

        for (int i = 0; i < count; i++) {
            if (parts[parts_ids[i]].parts_no == 15) {
                found++;
            }

            if (found >= 4) {
                parts_info->request[1] = 1;
                break;
            }
        }
    }

    if (placed[2][0] != NULL) {
        CRect_i_ rect;
        rect.x = rect.y = rect.width = rect.height = 0;
        GetRectParts(&rect, placed[2][0], 3);
        int area_no = placed[2][0]->area;

        if (CheckPartsRect(11, area_no, rect)) {
            parts_info->request[2] = 1;
        }

        if (CheckPartsRect(12, area_no, rect)) {
            parts_info->request[2] = 1;
        }

        if (CheckPartsRect(13, area_no, rect)) {
            parts_info->request[2] = 1;
        }
    }

    if (placed[3][0] != NULL) {
        CRect_i_ rect;
        rect.x = rect.y = rect.width = rect.height = 0;
        GetRectParts(&rect, placed[3][0], 4);
        int area_no = placed[3][0]->area;

        if (CheckPartsRect(14, area_no, rect)) {
            parts_info->request[3] = 1;
        }
    }

    if (placed[4][0] != NULL) {
        if (CheckPartsRect(4, 2, CRect_i_(0, 5, 7, 3))) {
            parts_info->request[4] = 1;
        }
    }

    if (placed[5][0] != NULL) {
        sceVu0FVECTOR position;
        placed[5][0]->GetPosition(position);

        if (GetAlt_i(position[0], position[1], position[2]) > 0) {
            parts_info->request[5] = 1;
        }
    }

    if (placed[6][0] != NULL && placed[3][0] != NULL) {
        CRect_i_ rect;
        rect.x = rect.y = rect.width = rect.height = 0;
        GetRectParts(&rect, placed[3][0], 3);

        if (CheckPartsRect(6, placed[3][0]->area, rect)) {
            parts_info->request[6] = 1;
        }
    }

    if (placed[7][0] != NULL) {
        sceVu0FVECTOR position;
        placed[7][0]->GetPosition(position);

        if (GetAlt_i(position[0], position[1], position[2]) > 0) {
            parts_info->request[7] = 1;
        }
    }

    if (placed[14][0] != NULL) {
        int      parts_ids[256];
        CRect_i_ rect;
        rect.x = rect.y = rect.width = rect.height = 0;
        GetRectParts(&rect, placed[14][0], 1);
        int count = areas[placed[14][0]->area]->GetPartsRect(rect, parts_ids, 256);
        int found = 0;

        for (int i = 0; i < count; i++) {
            int parts_no = parts[parts_ids[i]].parts_no;

            if (parts_no == 16) {
                found++;
            }

            if (parts_no == 17) {
                found++;
            }

            if (parts_no == 11) {
                found++;
            }

            if (parts_no == 12) {
                found++;
            }

            if (parts_no == 13) {
                found++;
            }

            if (found >= 14) {
                parts_info->request[14] = 1;
                break;
            }
        }
    }
}

void CEditGround::QueensRequest(CMapParts *(*placed)[64]) {
    parts_info->parts_max = 10;

    if (areas[0] == NULL || areas[1] == NULL || areas[2] == NULL) {
        return;
    }

    if (placed[0][0] != NULL) {
        if (CheckPartsRect(0, 1, CRect_i_(0, 0, 1, 7))) {
            parts_info->request[0] = 1;
        }

        if (CheckPartsRect(0, 2, CRect_i_(0, 0, 1, 8))) {
            parts_info->request[0] = 1;
        }
    }

    if (placed[1][0] != NULL && placed[10][0] != NULL) {
        CRect_i_ rect;
        rect.x = rect.y = rect.width = rect.height = 0;
        GetRectParts(&rect, placed[10][0], 3);

        if (CheckPartsRect(1, placed[10][0]->area, rect)) {
            parts_info->request[1] = 1;
        }
    }

    if (placed[2][0] != NULL && placed[8][0] != NULL) {
        CRect_i_ rect;
        rect.x = rect.y = rect.width = rect.height = 0;
        GetRectParts(&rect, placed[8][0], 2);

        if (CheckPartsRect(2, placed[8][0]->area, rect)) {
            parts_info->request[2] = 1;
        }
    }

    if (placed[3][0] != NULL && placed[1][0] != NULL) {
        CRect_i_ rect;
        rect.x = rect.y = rect.width = rect.height = 0;
        GetRectParts(&rect, placed[1][0], 2);

        if (CheckPartsRect(3, placed[1][0]->area, rect)) {
            parts_info->request[3] = 1;
        }
    }

    if (placed[4][0] != NULL) {
        if (placed[9][0] == NULL) {
            parts_info->request[4] = 1;
        } else if (placed[4][0]->area != placed[9][0]->area) {
            parts_info->request[4] = 1;
        }
    }

    if (placed[5][0] != NULL && placed[5][0]->GetRotY() == 0) {
        parts_info->request[5] = 1;
    }

    if (placed[6][0] != NULL && placed[11][0] != NULL && placed[6][0]->GetRotY() == -1) {
        CRect_i_ rect;
        rect.x = rect.y = rect.width = rect.height = 0;
        GetRectParts(&rect, placed[11][0], 3);

        if (CheckPartsRect(6, placed[11][0]->area, rect)) {
            parts_info->request[6] = 1;
        }
    }

    if (placed[7][0] != NULL && placed[7][0]->area == 0) {
        parts_info->request[7] = 1;
    }

    if (placed[8][0] != NULL) {
        CRect_i_ rect;
        rect.x = rect.y = rect.width = rect.height = 0;
        GetRectParts(&rect, placed[8][0], 0, 3);

        if (CheckPartsRect(13, placed[8][0]->area, rect)) {
            parts_info->request[8] = 1;
        }
    }

    if (placed[9][0] != NULL && placed[8][0] != NULL) {
        CRect_i_ rect;
        rect.x = rect.y = rect.width = rect.height = 0;
        GetRectParts(&rect, placed[8][0], 1);

        if (CheckPartsRect(9, placed[8][0]->area, rect)) {
            parts_info->request[9] = 1;
        }
    }
}

/**
 * Reports whether one part, turned by a quarter-turn offset, faces the same direction as another.
 *
 * @mangled CheckRot__FP9CMapPartsP9CMapPartsi
 * @address 0x1A7890
 * @size 0x8C
 */
static int CheckRot(CMapParts *parts, CMapParts *other, int rotation_offset) {
    int other_rotation;
    int rotation;

    if (parts == NULL || other == NULL) {
        return 0;
    }

    rotation = rotation_offset + parts->GetRotY();
    other_rotation = other->GetRotY();

    if (rotation > 2) {
        rotation -= 4;
    }

    if (rotation < -1) {
        rotation += 4;
    }

    return rotation == other_rotation;
}

void CEditGround::MuskaRequest(CMapParts *(*placed)[64]) {
    parts_info->parts_max = 9;

    if (areas[0] == NULL) {
        return;
    }

    if (placed[0][0] != NULL && placed[8][0] != NULL) {
        if (CheckPartsRect(0, 0, CRect_i_(1, 0, 6, 3))) {
            CRect_i_ rect;
            rect.x = rect.y = rect.width = rect.height = 0;
            GetRectDirParts(&rect, placed[0][0], 2, 14);

            if (CheckPartsRect(8, 0, rect) && CheckRot(placed[0][0], placed[8][0], 1) && parts_info->GetCompEvent(0)) {
                parts_info->request[0] = 1;
            }
        }
    }

    if (placed[1][0] != NULL && placed[10][0] != NULL) {
        CRect_i_ rect;
        rect.x = rect.y = rect.width = rect.height = 0;
        GetRectDirParts(&rect, placed[1][0], 2, 14);

        if (CheckPartsRect(10, 0, rect) && CheckRot(placed[1][0], placed[10][0], 1)) {
            parts_info->request[1] = 1;
        }
    }

    if (placed[2][0] != NULL && placed[9][0] != NULL) {
        CRect_i_ rect;
        rect.x = rect.y = rect.width = rect.height = 0;
        GetRectDirParts(&rect, placed[2][0], 2, 14);

        if (CheckPartsRect(9, 0, rect) && CheckRot(placed[2][0], placed[9][0], 0)) {
            parts_info->request[2] = 1;
        }
    }

    if (placed[3][0] != NULL && placed[9][0] != NULL) {
        CRect_i_ rect;
        rect.x = rect.y = rect.width = rect.height = 0;
        GetRectParts(&rect, placed[11][0], 2);

        if (CheckPartsRect(3, 0, rect)) {
            CRect_i_ dir_rect;
            dir_rect.x = dir_rect.y = dir_rect.width = dir_rect.height = 0;
            GetRectDirParts(&dir_rect, placed[3][0], 2, 14);

            if (CheckPartsRect(9, 0, dir_rect) && CheckRot(placed[3][0], placed[9][0], 2)) {
                parts_info->request[3] = 1;
            }
        }
    }

    if (placed[4][0] != NULL && placed[9][0] != NULL) {
        CRect_i_ rect;
        rect.x = rect.y = rect.width = rect.height = 0;
        GetRectDirParts(&rect, placed[4][0], 2, 14);

        if (CheckPartsRect(9, 0, rect) && CheckRot(placed[4][0], placed[9][0], 1)) {
            parts_info->request[4] = 1;
        }
    }

    if (placed[5][0] != NULL && placed[10][0] != NULL) {
        CRect_i_ rect;
        rect.x = rect.y = rect.width = rect.height = 0;
        GetRectDirParts(&rect, placed[5][0], 2, 14);

        if (CheckPartsRect(10, 0, rect) && CheckRot(placed[5][0], placed[10][0], 0)) {
            parts_info->request[5] = 1;
        }
    }

    if (placed[6][0] != NULL && placed[8][0] != NULL) {
        CRect_i_ rect;
        rect.x = rect.y = rect.width = rect.height = 0;
        GetRectDirParts(&rect, placed[6][0], 2, 14);

        if (CheckPartsRect(8, 0, rect) && CheckRot(placed[6][0], placed[8][0], 0) && placed[6][0]->GetRotY() == 1) {
            parts_info->request[6] = 1;
        }
    }

    if (placed[7][0] != NULL && placed[8][0] != NULL) {
        CRect_i_ rect;
        rect.x = rect.y = rect.width = rect.height = 0;
        GetRectDirParts(&rect, placed[7][0], 2, 14);

        if (CheckPartsRect(8, 0, rect) && CheckRot(placed[7][0], placed[8][0], 2)) {
            parts_info->request[7] = 1;
        }
    }

    if (placed[10][0] != NULL) {
        if (CheckPartsRect(10, 0, CRect_i_(2, 0, 3, 14)) && placed[10][0]->GetRotY() == 0) {
            parts_info->request[10] = 1;
        }
    }
}

void CEditGround::YellowRequest(CMapParts *(*placed)[64]) {
    int unused[24];
    int i;

    parts_info->parts_max = 13;

    if (areas[0] == NULL) {
        return;
    }

    for (int j = 0; j < 24; j++) {
        unused[j] = 0;
    }

    if (CheckRot(placed[0][0], placed[7][0], 0)) {
        CRect_i_ rect;
        CRect_i_ other;
        rect.x = rect.y = rect.width = rect.height = 0;
        other.x = other.y = other.width = other.height = 0;
        GetRectParts(&rect, placed[0][0], 0, 2);
        GetRectParts(&other, placed[0][0], 1, 2);

        if (CheckPartsRect(7, 0, rect) && CheckPartsRect(7, 0, other)) {
            parts_info->request[0] = 1;
        }
    }

    if (CheckRot(placed[1][0], placed[5][0], 0)) {
        CRect_i_ rect;
        rect.x = rect.y = rect.width = rect.height = 0;
        GetRectParts(&rect, placed[1][0], 0, -1);

        if (CheckPartsRect(5, 0, rect)) {
            parts_info->request[1] = 1;
        }
    }

    if (CheckRot(placed[2][0], placed[6][0], 0)) {
        CRect_i_ rect;
        rect.x = rect.y = rect.width = rect.height = 0;
        GetRectParts(&rect, placed[2][0], 0, -1);

        if (CheckPartsRect(6, 0, rect)) {
            parts_info->request[2] = 1;
        }
    }

    if (CheckRot(placed[3][0], placed[7][0], 0)) {
        CRect_i_ rect;
        CRect_i_ other;
        rect.x = rect.y = rect.width = rect.height = 0;
        other.x = other.y = other.width = other.height = 0;
        GetRectParts(&rect, placed[3][0], 2, 1);
        GetRectParts(&other, placed[3][0], 2, 0);

        if (CheckPartsRect(7, 0, rect) && !CheckPartsRect(7, 0, other)) {
            parts_info->request[3] = 1;
        }
    }

    if (CheckRot(placed[4][0], placed[7][0], 0)) {
        CRect_i_ rect;
        CRect_i_ other;
        rect.x = rect.y = rect.width = rect.height = 0;
        other.x = other.y = other.width = other.height = 0;
        GetRectParts(&rect, placed[4][0], -1, 1);
        GetRectParts(&other, placed[4][0], -1, 0);

        if (CheckPartsRect(7, 0, rect) && !CheckPartsRect(7, 0, other)) {
            parts_info->request[4] = 1;
        }
    }

    if (CheckRot(placed[5][0], placed[3][0], 0)) {
        CRect_i_ rect;
        rect.x = rect.y = rect.width = rect.height = 0;
        GetRectParts(&rect, placed[3][0], 1, 2);

        if (CheckPartsRect(5, 0, rect)) {
            parts_info->request[5] = 1;
        }
    }

    if (CheckRot(placed[6][0], placed[4][0], 0)) {
        CRect_i_ rect;
        rect.x = rect.y = rect.width = rect.height = 0;
        GetRectParts(&rect, placed[4][0], 0, 2);

        if (CheckPartsRect(6, 0, rect)) {
            parts_info->request[6] = 1;
        }
    }

    if (placed[7][0] != NULL) {
        parts_info->request[7] = 1;
    }

    if (CheckRot(placed[8][0], placed[7][0], 0)) {
        CRect_i_ rect;
        CRect_i_ other;
        rect.x = rect.y = rect.width = rect.height = 0;
        other.x = other.y = other.width = other.height = 0;
        GetRectParts(&rect, placed[8][0], 0, -1);
        GetRectParts(&other, placed[8][0], 1, -1);

        if (CheckPartsRect(7, 0, rect) && CheckPartsRect(7, 0, other)) {
            parts_info->request[8] = 1;
        }
    }

    if (CheckRot(placed[9][0], placed[8][0], 0)) {
        CRect_i_ rect;
        rect.x = rect.y = rect.width = rect.height = 0;
        GetRectParts(&rect, placed[8][0], 0, 1);

        if (CheckPartsRect(9, 0, rect)) {
            parts_info->request[9] = 1;
        }
    }

    if (CheckRot(placed[10][0], placed[8][0], 0)) {
        CRect_i_ rect;
        rect.x = rect.y = rect.width = rect.height = 0;
        GetRectParts(&rect, placed[8][0], 1, 1);

        if (CheckPartsRect(10, 0, rect)) {
            parts_info->request[10] = 1;
        }
    }

    if (CheckRot(placed[11][0], placed[9][0], 0)) {
        CRect_i_ rect;
        rect.x = rect.y = rect.width = rect.height = 0;
        GetRectParts(&rect, placed[11][0], 0, -1);

        if (CheckPartsRect(9, 0, rect)) {
            parts_info->request[11] = 1;
        }
    }

    if (CheckRot(placed[12][0], placed[10][0], 0)) {
        CRect_i_ rect;
        rect.x = rect.y = rect.width = rect.height = 0;
        GetRectParts(&rect, placed[12][0], 0, -1);

        if (CheckPartsRect(10, 0, rect)) {
            parts_info->request[12] = 1;
        }
    }

    for (i = 0; i < 24; i++) {
        int on = 2;
        this->plot_parts[i].FrameObjectOnOff("setuzoku", on);

        if (placed[i][0] != NULL) {
            sceVu0FVECTOR position;
            placed[i][0]->GetPosition(position);

            if (parts_info->request[i] && position[1] < 1.0f) {
                on = 1;
            }

            this->plot_parts[i].FrameObjectOnOff("setuzoku", on);
        }
    }
}
