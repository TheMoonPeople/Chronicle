#include "editground.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <memory>
#include <string_view>
#include <vector>

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
#include "platform/config.hpp"
#include "rect.hpp"
#include "savedata.hpp"
#include "vector3.hpp"

namespace dc::game {

using namespace dc::platform;

namespace {

int CheckDelete(CEditArea *area, CMapParts *parts, float x, float y, float z);

} // namespace

int CEditGround::SetMapParts(int parts_no, float x, float y, float z, int rot_y) {
    CVector3_f_ grid;
    int         deleted_parts_no = 0;
    int         deleted_rot_y = 0;

    if (CheckEffect()) {
        return -1;
    }

    if (parts_no < 0 || parts_no >= 24) {
        return -1;
    }

    if (plot_parts == nullptr) {
        return -1;
    }

    CMapParts *source = &plot_parts[parts_no];

    if (source->handle < 0) {
        return -1;
    }

    int area_no = GetAreaCode(x, y, z);

    if (area_no < 0) {
        return -1;
    }

    CEditArea *area = areas[area_no];

    if (source->info == nullptr) {
        return -1;
    }

    int        placeable = area->CheckParts(source, x, y, z, rot_y);
    CMapParts *target = parts;
    int        parts_id = -1;

    switch (source->subtype) {
        case MAP_PARTS_SUBTYPE_BRIDGE:
        case MAP_PARTS_SUBTYPE_ON_RIVER:
            // A bridge or a crossing replaces the river or road piece beneath it.
            parts_id = area->SearchPartsID(x, y, z);

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

    if (parts_info != nullptr) {
        EDITPARTS_INFO *info = parts_info->GetPartsInfo(parts_no);

        if (info != nullptr) {
            if (info->placed == info->stock) {
                return -1;
            }

            info->placed++;
        }
    }

    if (parts_id < 0) {
        for (parts_id = 0; parts_id < 128; parts_id++) {
            if (parts[parts_id].handle < 0) {
                break;
            }
        }

        if (parts_id == 128) {
            return -1;
        }

        target = &parts[parts_id];
    }

    if (placeable != 0 && source->subtype != MAP_PARTS_SUBTYPE_ROAD) {
        int span_width = source->GetWidth();
        int span_height = source->GetHeight();

        for (int column = 0; column < span_width; column++) {
            for (int row = 0; row < span_height; row++) {
                float cell_x = x - static_cast<float>((span_width >> 1) - column) * area->GetUnitSize();
                float cell_z = z - static_cast<float>((span_height >> 1) - row) * area->GetUnitSize();

                if (area->SearchPartsExtra(cell_x, y, cell_z) == 1) {
                    DeleteMapParts(&deleted_parts_no, &deleted_rot_y, cell_x, y, cell_z);
                }
            }
        }
    }

    memcpy(target, source, sizeof(CMapParts));
    target->SetRotY(rot_y);
    area->GetGrid(&grid, x, y, z);
    int width = source->GetWidth();
    int height = source->GetHeight();

    if (width % 2 == 1) {
        grid.x += 0.5f * area->GetUnitSize();
    }

    if (height % 2 == 1) {
        grid.z += 0.5f * area->GetUnitSize();
    }

    grid.y = area->GetAlt(grid.x, grid.y, grid.z);
    target->unit_size = area->GetUnitSize();
    sceVu0FVECTOR position = {0.0f, 0.0f, 0.0f, 1.0f};
    position[0] = grid.x;
    position[1] = grid.y;
    position[2] = grid.z;
    target->SetPosition(position);
    target->SetRotY(rot_y);
    target->area = area_no;
    area->SetMapParts(parts_id, parts, x, y, z, rot_y);

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

namespace {

/**
 * Puts a map part's camera collision frame where the part is and gives it back.
 */
CFrame *GetCameraFrame(CMapParts *parts) {
    if (parts->camera_frame == nullptr) {
        return nullptr;
    }

    parts->camera_frame->SetPosition(parts->pos[0], parts->pos[1], parts->pos[2]);
    parts->camera_frame->SetRotation(parts->rotation.x, parts->rotation.y, parts->rotation.z);
    return parts->camera_frame;
}

/**
 * Takes one copy of a plot's part back off the map count, never going below zero.
 */
void ReleasePlot(CEditPartsInfo *parts_info, int plot) {
    if (parts_info == nullptr) {
        return;
    }

    EDITPARTS_INFO *info = parts_info->GetPartsInfo(plot);

    if (info != nullptr) {
        info->placed = std::max<s32>(info->placed - 1, 0);
    }
}

} // namespace

int CEditGround::SetRiverParts(float x, float y, float z, int column_step, int row_step) {
    CVector3_i_ cell;
    CVector3_f_ position;
    int         deleted_parts_no = 0;
    int         deleted_rot_y = 0;

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

    if ((subtype != MAP_PARTS_SUBTYPE_RIVER && subtype != MAP_PARTS_SUBTYPE_BRIDGE && subtype != MAP_PARTS_SUBTYPE_ON_RIVER) || river_parts == nullptr) {
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

    CMapParts &river = river_parts[piece - 1];

    for (int i = 0; i < 4; i++) {
        object->SetFrame(river.frame[i], i);
    }

    object->collision_frame = river.GetCollisionFrame();
    object->shadow_frame = river.shadow_frame;
    object->shade_frame = river.shade_frame;
    object->camera_frame = GetCameraFrame(&river);
    object->ripple_frame = river.ripple_frame;
    object->handle = river.handle;
    object->subtype = river.subtype;
    object->bound = river.bound;
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

    if (object->subtype != MAP_PARTS_SUBTYPE_ROAD || road_parts == nullptr) {
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

    CMapParts &road = road_parts[piece - 1];

    for (int i = 0; i < 4; i++) {
        object->SetFrame(road.frame[i], i);
    }

    object->collision_frame = road.GetCollisionFrame();
    object->SetRotY(rot_y);
    object->handle = road.handle;
    object->subtype = road.subtype;
    object->bound = road.bound;
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
        ReleasePlot(parts_info, plot);
        return parts_id;
    }

    ReleasePlot(parts_info, parts[parts_id].parts_no);
    sceVu0FVECTOR position;
    parts[parts_id].GetPosition(position);
    area->DeleteMapParts(parts_id, parts, 1.0f + position[0], position[1], 1.0f + position[2]);

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
        if (areas[i] == nullptr) {
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
    for (CMapParts &object : parts) {
        if (object.handle >= 0 && object.parts_no == parts_no) {
            return &object;
        }
    }

    return nullptr;
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
        return nullptr;
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

    if (effect_parts_id < 0 || effect_parts_id >= 128 || effect_count <= 0 || parts[effect_parts_id].handle < 0) {
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

namespace {

/**
 * Returns the absolute value of a floating-point number.
 */
float Magnitude(float value) {
    return value < 0.0f ? -value : value;
}

} // namespace

void CEditGround::EditAreaClip(CCamera *camera, float range) {
    CBoxVu0       box;
    sceVu0FVECTOR eye = {0.0f, 0.0f, 0.0f, 0.0f};
    sceVu0FVECTOR ref;
    sceVu0FVECTOR forward;
    float         dists[4];
    sceVu0FVECTOR corner_a;
    sceVu0FVECTOR corner_b;
    sceVu0FVECTOR corner_max;
    sceVu0FVECTOR corner_min;

    if (camera != nullptr) {
        camera->GetPos(eye);
        camera->GetRef(ref);

        if (GetAreaCode(eye[0], eye[1], eye[2]) < 0) {
            sceVu0SubVector(forward, ref, eye);
            forward[1] = 0.0f;
            sceVu0Normalize(forward, forward);
            sceVu0ScaleVector(forward, forward, 500.0f);
            sceVu0AddVector(ref, eye, forward);
        }
    } else {
        sceVu0CopyVector(eye, ref);
    }

    for (int i = 0; i < 4; i++) {
        CEditArea *area = areas[i];

        if (area == nullptr) {
            break;
        }

        area->GetOffset(box.min);
        box.min[1] -= 2.0f * area->GetUnitAlt();
        box.min[3] = 1.0f;
        sceVu0CopyVector(box.max, box.min);
        box.max[0] += area->GetUnitSize() * static_cast<float>(area->GetWidth());
        box.max[1] += 32.0f * area->GetUnitAlt();
        box.max[2] += area->GetUnitSize() * static_cast<float>(area->GetHeight());
        dists[i] = -1.0f;

        if (MGClipBox(&box)) {
            area_visible[i] = 0;
            continue;
        }

        area_visible[i] = 1;

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

        if (side_x < 0 && side_z < 0) {
            dists[i] = DistVector(ref, corner_min);
        } else if (side_x == 0 && side_z < 0) {
            dists[i] = Magnitude(ref[2] - corner_min[2]);
        } else if (side_x > 0 && side_z < 0) {
            dists[i] = DistVector(ref, corner_a);
        } else if (side_x < 0 && side_z == 0) {
            dists[i] = Magnitude(ref[0] - corner_min[0]);
        } else if (side_x == 0 && side_z == 0) {
            dists[i] = 0.0f;
        } else if (side_x > 0 && side_z == 0) {
            dists[i] = Magnitude(ref[0] - corner_max[0]);
        } else if (side_x < 0 && side_z > 0) {
            dists[i] = DistVector(ref, corner_b);
        } else if (side_x == 0 && side_z > 0) {
            dists[i] = Magnitude(ref[2] - corner_max[2]);
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
        if (areas[j] == nullptr) {
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
                if (areas[k] == nullptr) {
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
        default:
            break;
    }
}

int CEditGround::GetRandomPlanePos(sceVu0FVECTOR out_position, sceVu0FVECTOR avoid[], int avoid_count, sceVu0FVECTOR bounds) {
    std::vector<int> cells;
    sceVu0FVECTOR    position;
    CVector3_f_      grid;
    CVector3_f_      chosen;
    int              area_count = 0;

    for (int area_no = 0;; area_no++) {
        if (areas[area_no] == nullptr) {
            break;
        }

        area_count++;
    }

    if (area_count == 0) {
        return 0;
    }

    cells.reserve(2048);

    for (int area_no = 0; area_no < area_count; area_no++) {
        int width = areas[area_no]->GetWidth();
        int height = areas[area_no]->GetHeight();

        // The outer two cells of each edge are never chosen.
        for (int x = 2; x < width - 2; x++) {
            for (int z = 2; z < height - 2; z++) {
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
                    cells.push_back((area_no << 16) | (x | (z << 8)));
                }
            }
        }
    }

    if (cells.empty()) {
        return 0;
    }

    int cell = cells[rand() % static_cast<int>(cells.size())];
    int column = cell & 0xFF;
    int row = (cell >> 8) & 0xFF;
    int area_no = (cell >> 16) & 0xFF;
    areas[area_no]->GetPos(&chosen, column, 0, row);
    out_position[0] = chosen.x + 0.5f * areas[area_no]->GetUnitSize();
    out_position[1] = areas[0]->GetAlt(column, row);
    out_position[2] = chosen.z + 0.5f * areas[area_no]->GetUnitSize();
    return 1;
}

int CEditGround::GetNearParts(CMapParts **out_parts, int limit, CBoxVu0 *box, CBoxVu0 *fixed_box) {
    int count = 0;

    for (CMapParts &object : parts) {
        if (object.handle < 0) {
            continue;
        }

        if (!object.CheckBox(box)) {
            continue;
        }

        if (count >= limit) {
            return count;
        }

        out_parts[count] = &object;
        count++;
    }

    if (fixed_box == nullptr) {
        fixed_box = box;
    }

    for (CMapParts &fixed : fixed_parts) {
        if (fixed.handle < 0) {
            continue;
        }

        if (!fixed.CheckBox2(fixed_box)) {
            continue;
        }

        if (count >= limit) {
            return count;
        }

        out_parts[count] = &fixed;
        count++;
    }

    return count;
}

void CEditGround::MakePartsBox() {
    for (CEditArea *area : areas) {
        if (area != nullptr) {
            area->MakePartsBox();
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

    if (areas[area_no] != nullptr) {
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

        if (owner == nullptr) {
            return nullptr;
        }

        sceVu0CopyVector(out_position, marker->position);
        CFrameVu1 *frame = owner->frame[0];

        if (frame == nullptr) {
            return nullptr;
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

    return nullptr;
}

void CEditGround::DrawBaseGround() {
    for (CEditArea *area : areas) {
        if (area == nullptr) {
            break;
        }

        area->DrawGrid();
    }
}

// Parts step down a level of detail at 50, 300, 500 and 800 from the eye, each raised to at least
// video.detail_distance so a part within it draws the finest level the caller allows.
void CEditGround::Draw(float time, int pass, int lowest, int highest, int fixed_lowest, int fixed_highest) {
    sceVu0FVECTOR lod_distance = {50.0f, 300.0f, 500.0f, 800.0f};
    sceVu0FVECTOR position;
    sceVu0FVECTOR ambient;
    float         detail = ConfigDetailDistance();

    for (float &distance : lod_distance) {
        distance = std::max(distance, detail);
    }

    for (CMapParts &fixed : fixed_parts) {
        if (fixed.category_no == pass) {
            fixed.DrawParts(time, lod_distance, fixed_lowest, fixed_highest, nullptr);
        }
    }

    for (int i = 0; i < 128; i++) {
        CMapParts *object = &parts[i];

        if (object->category_no != pass) {
            continue;
        }

        if (object->handle < 0) {
            continue;
        }

        if (object->area >= 0 && (object->area >= 4 || area_visible[object->area] == 0)) {
            continue;
        }

        if (!(clip_plane[3] <= 0.0f)) {
            object->GetPosition(position);

            if (!(DistVector(position, clip_plane) <= clip_plane[3]) && !object->ChangeDigData()) {
                continue;
            }
        }

        sceVu0FVECTOR focus_ambient = {32.0f, 32.0f, 128.0f, 128.0f};
        MGGetAmbient(ambient);

        if (focus_parts_id == i) {
            MGSetAmbient(focus_ambient);
        }

        object->DrawParts(time, lod_distance, lowest, highest, nullptr);
        MGSetAmbient(ambient);
    }
}

void CEditGround::StepWater() {
    for (CGroundWater &surface : water_surfaces) {
        if (surface.draw == 0) {
            continue;
        }

        if (surface.water.CheckClip()) {
            continue;
        }

        for (const GROUND_WATER_RIPPLE &ripple : surface.ripples) {
            if (ripple.power == 0.0f && ripple.range == 0.0f) {
                break;
            }

            int row = ripple.row;
            int column = ripple.column;

            // Both picks scale by the row count, so a free column only covers a square surface.
            if (row < 0) {
                int rows = surface.water.rows;
                row = rows * static_cast<float>(rand()) / 2.1474836e9f;
            }

            if (column < 0) {
                int rows = surface.water.rows;
                column = rows * static_cast<float>(rand()) / 2.1474836e9f;
            }

            surface.water.Shake(row, column, ripple.power + ripple.range * static_cast<float>(rand()) / 2.1474836e9f);
        }

        surface.water.Hamon();
    }
}

void CEditGround::DrawWaterSurface(CCamera *camera) {
    sceVu0FVECTOR eye;
    sceVu0FVECTOR position;
    sceVu0FVECTOR dir;
    sceVu0FVECTOR parts_position;

    camera->GetPos(eye);
    camera->GetDir(dir);
    dir[1] = 0.0f;
    sceVu0Normalize(dir, dir);

    for (CGroundWater &water_surface : water_surfaces) {
        CGroundWater *surface = &water_surface;

        if (surface->draw == 0) {
            continue;
        }

        CWater *water = &surface->water;
        sceVu0CopyVector(position, surface->offset);

        if (surface->parts_no >= 0) {
            CMapParts *owner = GetPartsObject(surface->parts_no);

            if (owner == nullptr) {
                continue;
            }

            CFrame *frame = owner->ripple_frame;

            if (frame != nullptr && surface->name[0] != '\0') {
                frame = frame->SearchFrame(surface->name);

                if (frame != nullptr && !(frame->attr.draw_on & 1)) {
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

        DrawVu1__6CWaterFP10RenderInfoP13sceVif1PacketP1(water, &mgRenderInfo, GetVif1Packet(), nullptr);
    }
}

void CEditGround::DrawWater(int pass) {
    sceVu0FVECTOR position;
    sceVu0FVECTOR corner;

    for (CMapParts &placed : parts) {
        if (suppress_water) {
            break;
        }

        CMapParts *object = &placed;

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
            default:
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

    for (CMapParts &fixed : fixed_parts) {
        if (fixed.category_no == pass) {
            fixed.Draw();
        }
    }
}

void CEditGround::DrawRipple(int pass) {
    sceVu0FVECTOR transform;
    sceVu0FVECTOR position;

    for (CMapParts &placed : parts) {
        if (suppress_water) {
            break;
        }

        CMapParts *object = &placed;

        if (object->handle < 0) {
            continue;
        }

        if (object->ripple_frame == nullptr) {
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

        if (object->ripple_frame == nullptr) {
            continue;
        }

        object->GetPosition(transform);
        object->ripple_frame->SetPosition(transform);
        object->SetRotY(object->GetRotY());
        object->GetRotation(transform);
        object->ripple_frame->SetRotation(transform[0], transform[1], transform[2]);
        MGDraw(object->ripple_frame);
    }

    for (CMapParts &fixed : fixed_parts) {
        if (fixed.category_no != pass) {
            continue;
        }

        if (fixed.ripple_frame == nullptr) {
            continue;
        }

        if (fixed.draw_on == 0) {
            continue;
        }

        fixed.GetPosition(transform);
        fixed.ripple_frame->SetPosition(transform);
        fixed.GetRotation(transform);
        fixed.ripple_frame->SetRotation(transform[0], transform[1], transform[2]);
        MGDraw(fixed.ripple_frame);
    }
}

namespace {

using ShadowClock = std::chrono::steady_clock;

// What EdGroundDrawShadowBeyondNear drew, reported every 120 calls under DC_PERF_GAME_PHASES=1.
struct ShadowPartCounts {
    unsigned calls = 0;
    unsigned mutable_scanned = 0;
    unsigned mutable_selected = 0;
    unsigned fixed_scanned = 0;
    unsigned fixed_selected = 0;
    unsigned shades = 0;
    unsigned shadows = 0;
    unsigned precise = 0;
    unsigned fast = 0;
    double   shade_ms = 0.0;
    double   shade_max_ms = 0.0;
    double   shadow_ms = 0.0;
    double   shadow_max_ms = 0.0;
};

ShadowPartCounts far_shadow_counts;

bool ShadowPartCountsEnabled() {
    static const bool enabled = [] {
        const char *setting = std::getenv("DC_PERF_GAME_PHASES");
        return setting != nullptr && std::string_view(setting) == "1";
    }();
    return enabled;
}

void AddShadowSpan(ShadowClock::time_point start, double &total_ms, double &max_ms) {
    const double elapsed_ms = std::chrono::duration<double, std::milli>(ShadowClock::now() - start).count();
    total_ms += elapsed_ms;
    max_ms = std::max(max_ms, elapsed_ms);
}

// Draws part's shade and then its shadow, with the fast volume program for pass 1 or past
// fast_distance; counts both when counts is given.
void DrawPartShadow(CMapParts &part, int pass, float depth, float fast_distance, ShadowPartCounts *counts) {
    if (counts != nullptr) {
        ++counts->shades;
    }
    const auto shade_start = counts != nullptr ? ShadowClock::now() : ShadowClock::time_point{};
    part.DrawShade();
    if (counts != nullptr) {
        AddShadowSpan(shade_start, counts->shade_ms, counts->shade_max_ms);
    }

    if (part.shadow_frame == nullptr) {
        return;
    }

    int fast = pass != 0;

    if (depth > fast_distance) {
        fast = 1;
    }

    if (counts != nullptr) {
        ++counts->shadows;
        ++(fast != 0 ? counts->fast : counts->precise);
    }
    const auto shadow_start = counts != nullptr ? ShadowClock::now() : ShadowClock::time_point{};
    part.DrawShadow(fast);
    if (counts != nullptr) {
        AddShadowSpan(shadow_start, counts->shadow_ms, counts->shadow_max_ms);
    }
}

// The shadows of the parts whose depth from the eye lies between near_distance and far_distance.
// A part within full of the eye takes the precise volume program whatever its draw_distance; past
// it, one deeper than 300 takes the fast program and one with a draw_distance casts nothing past it.
void DrawShadowBetween(CEditGround &ground, int pass, float near_distance, float far_distance, float full,
                       ShadowPartCounts *counts = nullptr) {
    sceVu0FVECTOR position;
    sceVu0FVECTOR view;
    sceVu0FVECTOR clip_position;
    float         fast_distance = full > 300.0f ? full : 300.0f;

    if (counts != nullptr) {
        counts->mutable_scanned += static_cast<unsigned>(std::size(ground.parts));
        counts->fixed_scanned += static_cast<unsigned>(std::size(ground.fixed_parts));
    }

    for (CMapParts &object : ground.parts) {
        if (object.category_no < 0) {
            continue;
        }

        if (object.handle < 0) {
            continue;
        }

        if (object.area >= 0 && (object.area >= 4 || ground.area_visible[object.area] == 0)) {
            continue;
        }

        if (!(ground.clip_plane[3] <= 0.0f)) {
            object.GetPosition(clip_position);

            if (!(DistVector(clip_position, ground.clip_plane) <= ground.clip_plane[3]) && !object.ChangeDigData()) {
                continue;
            }
        }

        object.GetPosition(position);
        position[3] = 1.0f;
        sceVu0ApplyMatrix(view, mgRenderInfo.view_scaled, position);

        if (!(object.draw_distance <= 0.0f) && pass == 0) {
            float distance = DistVector(view);

            if (object.draw_distance < distance && !(distance < full)) {
                continue;
            }
        }

        if (view[2] > near_distance && view[2] < far_distance) {
            if (counts != nullptr) {
                ++counts->mutable_selected;
            }
            DrawPartShadow(object, pass, view[2], fast_distance, counts);
        }
    }

    for (CMapParts &fixed : ground.fixed_parts) {
        if (fixed.category_no < 0) {
            continue;
        }

        if (fixed.handle < 0) {
            continue;
        }

        sceVu0CopyVector(position, fixed.pos);
        position[3] = 1.0f;
        sceVu0ApplyMatrix(view, mgRenderInfo.view_scaled, position);

        if (view[2] > near_distance && view[2] < far_distance) {
            if (counts != nullptr) {
                ++counts->fixed_selected;
            }
            DrawPartShadow(fixed, pass, view[2], fast_distance, counts);
        }
    }
}

} // namespace

// Casts each part's shadow by its depth from the eye: EditLoop composites
// the parts nearer than the map's shadow_near (320 by default) at shadow_mode (0x34) and those out
// to shadow_far (740) at the fainter shadow_mode_2 (0x20), and past that a part casts none. A part
// deeper than 300 takes the fast volume program, and one with a draw_distance casts nothing past
// it. A part within video.shadow_distance of the eye belongs to the near band, with the precise
// program and whatever its draw_distance, so its shadow is as dark and as exact as up close; those
// of them deeper than shadow_near are left out of the near band here and cast by
// EdGroundDrawShadowBeyondNear. The overhead view of georama mode (pass 1) keeps its own.
void CEditGround::DrawShadow(int pass, float near_distance, float far_distance) {
    float full = pass == 0 ? ConfigShadowDistance() : 0.0f;

    if (near_distance < 1.0f) {
        near_distance = -10000000.0f;
    } else {
        near_distance = std::max(near_distance, full);
        far_distance = std::max(far_distance, near_distance);
    }

    DrawShadowBetween(*this, pass, near_distance, far_distance, full);
}

void EdGroundDrawShadowBeyondNear(CEditGround *ground, float shadow_near) {
    ShadowPartCounts *counts = ShadowPartCountsEnabled() ? &far_shadow_counts : nullptr;
    float             full = ConfigShadowDistance();

    if (full > shadow_near) {
        DrawShadowBetween(*ground, 0, shadow_near, full, full, counts);
    }
    if (counts != nullptr && ++counts->calls == 120) {
        std::fprintf(stderr,
                     "cpu farshadowparts %u calls: mutable %u/%u fixed %u/%u shade %u shadow %u precise %u fast %u\n",
                     counts->calls, counts->mutable_selected, counts->mutable_scanned, counts->fixed_selected,
                     counts->fixed_scanned, counts->shades, counts->shadows, counts->precise, counts->fast);
        std::fprintf(
            stderr,
            "cpu farshadowparts calls wall ms: shade total %.3f max %.3f shadow total %.3f max %.3f over %u frames\n",
            counts->shade_ms, counts->shade_max_ms, counts->shadow_ms, counts->shadow_max_ms, counts->calls);
        *counts = {};
    }
}

namespace {

// LoadObjectParts stores a preview frame pointer truncated to the s32 preview fields; the frame is
// recovered by matching those bits against the part's own level-of-detail frames.
CFrame *PreviewFrame(CMapParts *parts, s32 truncated) {
    if (truncated == 0) {
        return nullptr;
    }

    for (int lod = 3; lod >= 0; lod--) {
        CFrame        *frame = parts->frame[lod];
        std::uintptr_t bits = reinterpret_cast<std::uintptr_t>(frame) & 0xFFFFFFFFu;

        if (frame != nullptr && bits == static_cast<u32>(truncated)) {
            return frame;
        }
    }

    return nullptr;
}

} // namespace

void CEditGround::DrawPartsCursor(int plot, float *position, float *model_pos, int rot_y, float *rotation, int area_no) {
    if (plot < 0 || plot >= 24) {
        sceVu0CopyVector(model_pos, position);
        return;
    }

    int area_code = GetAreaCode(position[0], position[1], position[2]);

    if (area_code < 0) {
        sceVu0CopyVector(model_pos, position);
        return;
    }

    CEditArea *area = areas[area_code];
    CMapParts *source = &plot_parts[plot];
    int        width = source->GetWidth();
    int        height = source->GetHeight();

    if (area->CheckAreaRect(position[0], position[1], position[2], width, height) == 0) {
        sceVu0CopyVector(model_pos, position);
        return;
    }

    CVector3_i_ grid;
    area->GetPos(&grid, position[0], position[1], position[2]);
    int fits = area->CheckParts(source, position[0], position[1], position[2], rot_y);
    int occupant = area->SearchPartsID(position[0], position[1], position[2]);

    if (source->subtype == MAP_PARTS_SUBTYPE_ON_RIVER) {
        if (CheckDelete(area, source, position[0], position[1], position[2]) != 0) {
            fits = 0;
        }

        if (occupant >= 0 && occupant < 128 && parts[occupant].handle != 1) {
            fits = 0;
        }
    }

    if (parts_info != nullptr) {
        EDITPARTS_INFO *info = parts_info->GetPartsInfo(plot);

        if (info != nullptr) {
            fits &= info->placed < info->stock;
        }
    }

    CVector3_f_ cell;
    area->GetPos(&cell, grid.x, grid.y, grid.z);
    position[0] = cell.x;
    position[1] = 5.0f + cell.y;
    position[2] = cell.z;
    position[3] = 0.0f;
    CFrame *preview = fits != 0 ? PreviewFrame(source, source->preview_frame) : PreviewFrame(source, source->blocked_preview_frame);

    // The preview is drawn at the part's finest level of detail.
    if (preview != nullptr && source->frame[0] != nullptr) {
        preview = source->frame[0];
    }

    if (width % 2 == 1) {
        position[0] += 0.5f * area->GetUnitSize();
    }

    if (height % 2 == 1) {
        position[2] += 0.5f * area->GetUnitSize();
    }

    sceVu0FMATRIX light_direction;
    sceVu0FMATRIX light_colour;
    sceVu0FVECTOR ambient;

    if (fits == 0) {
        // Draw the cursor dim and without point lights where the part cannot go.
        MGGetAmbient(ambient);
        MGGetPLight(light_direction, light_colour);
        sceVu0FVECTOR dim = {60.0f, 60.0f, 60.0f, 128.0f};
        MGSetAmbient(dim);
        MGSetPLight(mgZeroMatrix, mgZeroMatrix);
    }

    cursor.unit_size = area->GetUnitSize();

    if (cursor.area == area_no) {
        cursor.Draw(position, width, height);
    }

    if (fits == 0) {
        MGSetAmbient(ambient);
        MGSetPLight(light_direction, light_colour);
    }

    position[1] += 50.0f;
    sceVu0FVECTOR goal;
    sceVu0CopyVector(goal, position);

    for (int axis = 0; axis < 3; axis++) {
        float step = goal[axis] - model_pos[axis];

        if (Magnitude(step) < 1.0f) {
            model_pos[axis] = goal[axis];
        } else {
            model_pos[axis] += step / 8.0f;
        }
    }

    if (source->category_no != area_no) {
        return;
    }

    int subtype = source->subtype;

    if (preview == nullptr || subtype == MAP_PARTS_SUBTYPE_RIVER || subtype == MAP_PARTS_SUBTYPE_BRIDGE) {
        return;
    }

    CFrame *model = nullptr;
    int     saved_draw = 0;

    if (subtype == MAP_PARTS_SUBTYPE_ON_RIVER) {
        char river_frame_name[] = "kawa";
        model = preview->SearchFrame(river_frame_name);

        if (model != nullptr) {
            saved_draw = model->attr.draw_on;
            model->attr.draw_on = 2;
        }

        if (occupant >= 0 && occupant < 128) {
            parts[occupant].GetRotation(rotation);
        }
    }

    preview->SetPosition(model_pos);
    preview->SetRotation(rotation[0], rotation[1], rotation[2]);
    MGDraw(preview);

    if (model != nullptr) {
        model->attr.draw_on = saved_draw;
    }
}

void CEditGround::DrawEffect(CCameraFollow *camera, float time, CEffectGroup *effects) {
    sceVu0FVECTOR position;

    for (CMapParts &object : parts) {
        if (object.area >= 0 && area_visible[object.area] == 0) {
            continue;
        }

        if (!(clip_plane[3] <= 0.0f)) {
            object.GetPosition(position);

            if (!(DistVector(position, clip_plane) <= clip_plane[3])) {
                continue;
            }
        }

        object.DrawEffect(camera, time, effects);
    }

    for (CMapParts &fixed : fixed_parts) {
        fixed.DrawEffect(camera, time, effects);
    }
}

void CEditGround::Save([[maybe_unused]] char *path) {
    auto          buffer = std::make_unique<char[]>(4000);
    sceVu0FVECTOR position;
    auto         *header = reinterpret_cast<GROUND_SAVE_HEADER *>(buffer.get());
    auto         *records = reinterpret_cast<SV_GRD_PART *>(buffer.get() + sizeof(GROUND_SAVE_HEADER));

    header->offset = sizeof(GROUND_SAVE_HEADER);
    header->count = 0;

    for (CMapParts &placed : parts) {
        CMapParts *object = &placed;
        int        parts_id = placed.handle;

        if (parts_id < 0) {
            continue;
        }

        int kind = object->subtype;

        if (kind > 0) {
            for (int j = 0; j < 24; j++) {
                if (kind == plot_parts[j].subtype) {
                    parts_id = plot_parts[j].handle;
                    break;
                }
            }
        }

        object->GetPosition(position);
        SV_GRD_PART &record = records[header->count];
        record.part_id = parts_id;
        record.rot_y = object->rot_y;
        record.pos_x = position[0];
        record.pos_y = position[1];
        record.pos_z = position[2];
        header->count++;
    }

    SV_GRD_PART &terminator = records[header->count];
    terminator.part_id = -1;
    terminator.rot_y = -1;
    terminator.pos_x = 0.0f;
    terminator.pos_y = 0.0f;
    terminator.pos_z = 0.0f;
    header->size = sizeof(GROUND_SAVE_HEADER) + (header->count + 1) * sizeof(SV_GRD_PART);
    WriteFile("host0:y:/ps2/dc_data/gdata0.edt", buffer.get(), header->size);
}

void CEditGround::Load(char *data) {
    std::unique_ptr<char[]> buffer;
    char                   *file = data;

    Clear();

    if (data == nullptr) {
        buffer = std::make_unique<char[]>(4000);
        file = buffer.get();

        if (!LoadFile2("gdata0.edt", file, nullptr, 0)) {
            return;
        }
    }

    const auto *header = reinterpret_cast<GROUND_SAVE_HEADER *>(file);
    const auto *records = reinterpret_cast<SV_GRD_PART *>(file + header->offset);
    // Parts that follow the ground's height go down first, then the rest, then the
    // parts that stand on others.
    for (int i = 0; i < header->count; i++) {
        const SV_GRD_PART *record = &records[i];
        int                parts_id = record->part_id;

        if (parts_id < 0 || parts_id >= 24) {
            break;
        }

        if (plot_parts[parts_id].ChangeAltData()) {
            float x = record->pos_x + 1.0f;
            float z = record->pos_z + 1.0f;
            SetMapParts(record->part_id, x, record->pos_y, z, record->rot_y);
        }
    }

    for (int i = 0; i < header->count; i++) {
        const SV_GRD_PART *record = &records[i];
        int                parts_id = record->part_id;

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

    for (int i = 0; i < header->count; i++) {
        const SV_GRD_PART *record = &records[i];
        int                parts_id = record->part_id;

        if (parts_id < 0 || parts_id >= 24) {
            break;
        }

        int kind = plot_parts[parts_id].subtype;

        if (kind == MAP_PARTS_SUBTYPE_BRIDGE || kind == MAP_PARTS_SUBTYPE_ON_RIVER) {
            SetMapParts(parts_id, record->pos_x + 1.0f, record->pos_y, record->pos_z + 1.0f, record->rot_y);
        }
    }

    for (CEditArea *area : areas) {
        if (area != nullptr) {
            area->RemakeGrid();
            area->RemakeGrid();
        }
    }
}

void CEditGround::Save(int town, CSaveData *save) {
    sceVu0FVECTOR position;
    int           count = 0;

    SV_GRD_PART *records = save->GetParts(town, &count);

    if (records == nullptr) {
        return;
    }

    int saved = 0;

    for (CMapParts &object : parts) {
        int parts_id = object.handle;

        if (parts_id < 0) {
            continue;
        }

        int kind = object.subtype;

        if (kind == MAP_PARTS_SUBTYPE_ROAD || kind == MAP_PARTS_SUBTYPE_RIVER || kind == MAP_PARTS_SUBTYPE_BRIDGE) {
            for (int j = 0; j < 24; j++) {
                if (kind == plot_parts[j].subtype) {
                    parts_id = plot_parts[j].handle;
                    break;
                }
            }
        }

        object.GetPosition(position);
        SV_GRD_PART &record = records[saved++];
        record.part_id = parts_id;
        record.rot_y = object.rot_y;
        record.pos_x = position[0];
        record.pos_y = position[1];
        record.pos_z = position[2];
    }

    SV_GRD_PART &terminator = records[saved];
    terminator.part_id = -1;
    terminator.rot_y = -1;
    terminator.pos_x = 0.0f;
    terminator.pos_y = 0.0f;
    terminator.pos_z = 0.0f;
}

void CEditGround::Load(int town, CSaveData *save) {
    int count = 0;

    SV_GRD_PART *saved = save->GetParts(town, &count);

    if (saved == nullptr) {
        return;
    }

    auto  buffer = std::make_unique<char[]>(0x5000);
    auto *header = reinterpret_cast<GROUND_SAVE_HEADER *>(buffer.get());
    auto *records = reinterpret_cast<SV_GRD_PART *>(buffer.get() + sizeof(GROUND_SAVE_HEADER));

    header->count = count;
    header->offset = sizeof(GROUND_SAVE_HEADER);

    for (int i = 0; i < count; i++) {
        records[i].part_id = saved[i].part_id;
        records[i].rot_y = saved[i].rot_y;
        records[i].pos_x = saved[i].pos_x;
        records[i].pos_y = saved[i].pos_y;
        records[i].pos_z = saved[i].pos_z;
    }

    records[count].part_id = -1;
    records[count].rot_y = -1;
    Load(buffer.get());
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
    int count = 0;

    for (CMapParts &object : parts) {
        if (object.handle < 0) {
            continue;
        }

        if (flags != 0 && object.subtype == MAP_PARTS_SUBTYPE_NONE) {
            continue;
        }

        CFrame *frame = object.GetCollisionFrame();

        if (frame == nullptr) {
            continue;
        }

        if (!object.CheckBox(&box)) {
            continue;
        }

        count += frame->PickUpNearPoly(&out_polygons[count], box);
    }

    for (CMapParts &fixed : fixed_parts) {
        if (fixed.handle < 0) {
            continue;
        }

        CFrame *frame = fixed.GetCollisionFrame();

        if (frame == nullptr) {
            continue;
        }

        if (!fixed.CheckBox2(&box)) {
            continue;
        }

        count += frame->PickUpNearPoly(&out_polygons[count], box);
    }

    for (CEditArea *area : areas) {
        if (area != nullptr) {
            count += area->PickUpPoly(&out_polygons[count], box);
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
    int           count = 0;
    sceVu0FVECTOR center;

    for (int i = 0; i < 128 && (flags & 2); i++) {
        CMapParts &object = parts[i];

        if (object.handle < 0) {
            continue;
        }

        CFrame *frame = GetCameraFrame(&object);

        if (frame == nullptr) {
            continue;
        }

        if (!object.CheckBox(&box)) {
            continue;
        }

        count += frame->PickUpNearPoly(&out_polygons[count], box);
    }

    for (int i = 0; i < 64 && (flags & 1); i++) {
        CMapParts &fixed = fixed_parts[i];

        if (fixed.handle < 0) {
            continue;
        }

        CFrame *frame = GetCameraFrame(&fixed);

        if (frame == nullptr) {
            continue;
        }

        if (!fixed.CheckBox(&box)) {
            continue;
        }

        count += frame->PickUpNearPoly(&out_polygons[count], box);
    }

    // Every area writes its ground polygons to the same place, so only the last one's survive.
    CCPoly *area_polygons = &out_polygons[count];

    for (CEditArea *area : areas) {
        if (area != nullptr) {
            sceVu0AddVector(center, box.max, box.min);
            sceVu0ScaleVector(center, center, 0.5f);
            count += area->PickUpPoly(area_polygons, center[0], center[1], center[2]);
        }
    }

    return count;
}

void CEditGround::Clear() {
    for (CMapParts &object : parts) {
        object.Initialize();
    }

    for (int i = 0; i < 4; i++) {
        if (areas[i] != nullptr) {
            areas[i]->Clear();
        }

        area_visible[i] = 1;
    }

    effect_count = -1;
    effect_parts_id = -1;
    effect_alt = 0.0f;
    effect_target_alt = 0.0f;
    effect_step = 0.0f;

    if (parts_info != nullptr) {
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
        info->placed = 0;
        info->stock -= 6;
        info->obtained = saved;
    }
}

void CEditGround::Initialize() {
    map_no = TOWN_NORUNE;
    plot_parts = nullptr;
    river_parts = nullptr;
    road_parts = nullptr;
    parts_info = nullptr;

    for (int area = 0; area < 4; area++) {
        areas[area] = nullptr;
        area_visible[area] = 1;
    }

    for (CMapParts &fixed : fixed_parts) {
        fixed.Initialize();
    }

    for (s32 &word : spare_words) {
        word = 0;
    }

    for (CGroundWater &surface : water_surfaces) {
        surface.draw = 0;
    }

    for (EPARTS_FUNC_DATA *&marker : people) {
        marker = nullptr;
    }

    people_count = 0;
    suppress_water = 0;
    Clear();
    clip_plane[3] = -1.0f;
}

void CEditGround::RemakeGrid() {
    for (CEditArea *area : areas) {
        if (area != nullptr) {
            area->grid_redraw = 1;
        }
    }
}

CEditGround::CEditGround() {
    cursor.area = 0;
    people[0] = nullptr;
    cursor.pieces[1] = nullptr;
    cursor.pieces[0] = nullptr;
    cursor.unit_size = 1.0f;
    Initialize();
}

namespace {

/**
 * Reports whether a part may be replaced by the one being placed over it.
 */
int CheckDelete(CEditArea *area, CMapParts *parts, float x, float y, float z) {
    CVector3_i_ position;

    area->GetPos(&position, x, y, z);
    int column = position.x;
    int row = position.z;
    int kind = parts->subtype;

    // The river ends Clear lays down in Matataki stay put; only a bridge may go over them.
    if (area->GetMapNo() != TOWN_MATATAKI || kind == MAP_PARTS_SUBTYPE_BRIDGE) {
        return 0;
    }

    switch (area->GetAreaID()) {
        case 0:
            return (column == 5 && row == 0) || (column == 2 && row == 7);
        case 1:
            return (column == 3 && row == 5) || (column == 11 && row == 3);
        case 2:
            return (column == 4 && row == 0) || (column == 3 && row == 7);
        default:
            return 0;
    }
}

} // namespace

void CPartsCursor::Draw(float *position, int width, int height) {
    sceVu0FVECTOR cell;
    sceVu0FVECTOR corner;
    int           columns = width * 2;
    int           rows = height * 2;

    sceVu0CopyVector(corner, position);
    corner[0] -= 0.5f * (unit_size * width);
    corner[2] -= 0.5f * (unit_size * height);
    corner[0] += 0.25f * unit_size;
    corner[2] += 0.25f * unit_size;
    sceVu0CopyVector(cell, corner);

    for (int i = 0; i < columns; i++) {
        cell[2] = corner[2];

        for (int j = 0; j < rows; j++) {
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
    CMapParts *placed[24][64] = {};
    int        plot_count[24] = {};

    for (int i = 0; i < 24; i++) {
        parts_info->request[i] = 0;
    }

    for (CMapParts &object : parts) {
        if (object.handle >= 0) {
            int plot = object.parts_no;

            if (plot >= 0 && plot < 24) {
                placed[plot][plot_count[plot]++] = &object;
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
        default:
            break;
    }

    for (int i = 0; i < 24; i++) {
        if (!parts_info->CheckComplete(i)) {
            parts_info->request[i] = 0;
        }
    }
}

int CEditGround::CheckPartsRect(int parts_no, int area_no, CRect_i_ &rect) {
    std::array<int, 256> parts_ids;

    if (area_no < 0 || area_no >= 4) {
        return 0;
    }

    if (areas[area_no] == nullptr) {
        return 0;
    }

    int count = areas[area_no]->GetPartsRect(rect, parts_ids.data(), static_cast<int>(parts_ids.size()));

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

    if (target == nullptr) {
        return;
    }

    target->GetPosition(position);
    int area_no = GetAreaCode(position[0], position[1], position[2]);

    if (area_no < 0 || area_no >= 4) {
        return;
    }

    if (areas[area_no] == nullptr) {
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

    *rect = CRect_i_(0, 0, 0, 0);

    if (target == nullptr) {
        return;
    }

    target->GetPosition(position);
    int area_no = GetAreaCode(position[0], position[1], position[2]);

    if (area_no < 0 || area_no >= 4) {
        return;
    }

    if (areas[area_no] == nullptr) {
        return;
    }

    areas[area_no]->GetPos(&grid, position[0], position[1], position[2]);
    int rot_y = target->GetRotY();
    target->SetRotY(0);
    int width = target->GetWidth();
    int height = target->GetHeight();
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

    int offset_x = 0;
    int offset_z = 0;

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
        default:
            break;
    }

    rect->x = grid.x + offset_x + adjust_x;
    rect->y = grid.z + offset_z + adjust_z;
    rect->width = 1;
    rect->height = 1;
}

namespace {

/**
 * Gives the first cell that a span covers when it is centred on a cell.
 */
int SpanStart(int centre, int size) {
    return centre - (size >> 1);
}

} // namespace

void CEditGround::GetRectDirParts(CRect_i_ *rect, CMapParts *target, int direction, int depth) {
    sceVu0FVECTOR position;
    CVector3_i_   grid;

    *rect = CRect_i_(0, 0, 0, 0);

    if (target == nullptr) {
        return;
    }

    target->GetPosition(position);
    int area_no = GetAreaCode(position[0], position[1], position[2]);

    if (area_no < 0 || area_no >= 4) {
        return;
    }

    if (areas[area_no] == nullptr) {
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

    if (areas[0] == nullptr) {
        return;
    }

    if (placed[0][0] != nullptr && placed[0][0]->GetRotY() == 1) {
        parts_info->request[0] = 1;
    }

    if (placed[1][0] != nullptr) {
        CRect_i_ zone(10, 0, 4, 5);

        if (CheckPartsRect(1, 0, zone)) {
            parts_info->request[1] = 1;
        }
    }

    if (placed[2][0] != nullptr) {
        CRect_i_ zone(2, 8, 9, 4);

        if (!CheckPartsRect(2, 0, zone)) {
            parts_info->request[2] = 1;
        }
    }

    if (placed[3][0] != nullptr) {
        CRect_i_ rect;
        rect.x = rect.y = rect.width = rect.height = 0;
        GetRectParts(&rect, placed[12][0], 2);

        if (CheckPartsRect(3, 0, rect)) {
            parts_info->request[3] = 1;
        }
    }

    if (placed[4][0] != nullptr) {
        CRect_i_ rect;
        rect.x = rect.y = rect.width = rect.height = 0;
        GetRectParts(&rect, placed[6][0], 4);

        if (CheckPartsRect(4, 0, rect)) {
            parts_info->request[4] = 1;
        }
    }

    if (placed[5][0] != nullptr) {
        CRect_i_ rect;
        rect.x = rect.y = rect.width = rect.height = 0;
        GetRectDirParts(&rect, placed[8][0], 2, 4);

        if (CheckPartsRect(5, 0, rect) && SaveData->GetGameIntFlag(0) >= 100) {
            parts_info->request[5] = 1;
        }
    }

    if (placed[6][0] != nullptr) {
        CRect_i_ rect;
        rect.x = rect.y = rect.width = rect.height = 0;
        GetRectParts(&rect, placed[1][0], 4);

        if (!CheckPartsRect(6, 0, rect)) {
            parts_info->request[6] = 1;
        }
    }

    if (placed[7][0] != nullptr) {
        std::array<int, 256> parts_ids;
        CRect_i_             rect;
        rect.x = rect.y = rect.width = rect.height = 0;
        GetRectParts(&rect, placed[7][0], 4);
        int count = areas[0]->GetPartsRect(rect, parts_ids.data(), static_cast<int>(parts_ids.size()));
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

    if (areas[0] == nullptr || areas[1] == nullptr || areas[2] == nullptr) {
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

    if (placed[0][0] != nullptr) {
        CRect_i_ zone(0, 0, 4, 1);

        if (CheckPartsRect(0, 0, zone)) {
            parts_info->request[0] = 1;
        }

        CRect_i_ second_zone(0, 5, 1, 1);

        if (CheckPartsRect(0, 1, second_zone)) {
            parts_info->request[0] = 1;
        }
    }

    if (placed[1][0] != nullptr) {
        std::array<int, 256> parts_ids;
        CRect_i_             rect;
        rect.x = rect.y = rect.width = rect.height = 0;
        GetRectParts(&rect, placed[1][0], 1);
        int count = areas[placed[1][0]->area]->GetPartsRect(rect, parts_ids.data(), static_cast<int>(parts_ids.size()));
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

    if (placed[2][0] != nullptr) {
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

    if (placed[3][0] != nullptr) {
        CRect_i_ rect;
        rect.x = rect.y = rect.width = rect.height = 0;
        GetRectParts(&rect, placed[3][0], 4);
        int area_no = placed[3][0]->area;

        if (CheckPartsRect(14, area_no, rect)) {
            parts_info->request[3] = 1;
        }
    }

    if (placed[4][0] != nullptr) {
        CRect_i_ zone(0, 5, 7, 3);

        if (CheckPartsRect(4, 2, zone)) {
            parts_info->request[4] = 1;
        }
    }

    if (placed[5][0] != nullptr) {
        sceVu0FVECTOR position;
        placed[5][0]->GetPosition(position);

        if (GetAlt_i(position[0], position[1], position[2]) > 0) {
            parts_info->request[5] = 1;
        }
    }

    if (placed[6][0] != nullptr && placed[3][0] != nullptr) {
        CRect_i_ rect;
        rect.x = rect.y = rect.width = rect.height = 0;
        GetRectParts(&rect, placed[3][0], 3);

        if (CheckPartsRect(6, placed[3][0]->area, rect)) {
            parts_info->request[6] = 1;
        }
    }

    if (placed[7][0] != nullptr) {
        sceVu0FVECTOR position;
        placed[7][0]->GetPosition(position);

        if (GetAlt_i(position[0], position[1], position[2]) > 0) {
            parts_info->request[7] = 1;
        }
    }

    if (placed[14][0] != nullptr) {
        std::array<int, 256> parts_ids;
        CRect_i_             rect;
        rect.x = rect.y = rect.width = rect.height = 0;
        GetRectParts(&rect, placed[14][0], 1);
        int count = areas[placed[14][0]->area]->GetPartsRect(rect, parts_ids.data(), static_cast<int>(parts_ids.size()));
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

    if (areas[0] == nullptr || areas[1] == nullptr || areas[2] == nullptr) {
        return;
    }

    if (placed[0][0] != nullptr) {
        CRect_i_ zone(0, 0, 1, 7);

        if (CheckPartsRect(0, 1, zone)) {
            parts_info->request[0] = 1;
        }

        CRect_i_ second_zone(0, 0, 1, 8);

        if (CheckPartsRect(0, 2, second_zone)) {
            parts_info->request[0] = 1;
        }
    }

    if (placed[1][0] != nullptr && placed[10][0] != nullptr) {
        CRect_i_ rect;
        rect.x = rect.y = rect.width = rect.height = 0;
        GetRectParts(&rect, placed[10][0], 3);

        if (CheckPartsRect(1, placed[10][0]->area, rect)) {
            parts_info->request[1] = 1;
        }
    }

    if (placed[2][0] != nullptr && placed[8][0] != nullptr) {
        CRect_i_ rect;
        rect.x = rect.y = rect.width = rect.height = 0;
        GetRectParts(&rect, placed[8][0], 2);

        if (CheckPartsRect(2, placed[8][0]->area, rect)) {
            parts_info->request[2] = 1;
        }
    }

    if (placed[3][0] != nullptr && placed[1][0] != nullptr) {
        CRect_i_ rect;
        rect.x = rect.y = rect.width = rect.height = 0;
        GetRectParts(&rect, placed[1][0], 2);

        if (CheckPartsRect(3, placed[1][0]->area, rect)) {
            parts_info->request[3] = 1;
        }
    }

    if (placed[4][0] != nullptr) {
        if (placed[9][0] == nullptr) {
            parts_info->request[4] = 1;
        } else if (placed[4][0]->area != placed[9][0]->area) {
            parts_info->request[4] = 1;
        }
    }

    if (placed[5][0] != nullptr && placed[5][0]->GetRotY() == 0) {
        parts_info->request[5] = 1;
    }

    if (placed[6][0] != nullptr && placed[11][0] != nullptr && placed[6][0]->GetRotY() == -1) {
        CRect_i_ rect;
        rect.x = rect.y = rect.width = rect.height = 0;
        GetRectParts(&rect, placed[11][0], 3);

        if (CheckPartsRect(6, placed[11][0]->area, rect)) {
            parts_info->request[6] = 1;
        }
    }

    if (placed[7][0] != nullptr && placed[7][0]->area == 0) {
        parts_info->request[7] = 1;
    }

    if (placed[8][0] != nullptr) {
        CRect_i_ rect;
        rect.x = rect.y = rect.width = rect.height = 0;
        GetRectParts(&rect, placed[8][0], 0, 3);

        if (CheckPartsRect(13, placed[8][0]->area, rect)) {
            parts_info->request[8] = 1;
        }
    }

    if (placed[9][0] != nullptr && placed[8][0] != nullptr) {
        CRect_i_ rect;
        rect.x = rect.y = rect.width = rect.height = 0;
        GetRectParts(&rect, placed[8][0], 1);

        if (CheckPartsRect(9, placed[8][0]->area, rect)) {
            parts_info->request[9] = 1;
        }
    }
}

namespace {

/**
 * Reports whether one part, turned by a quarter-turn offset, faces the same direction as another.
 */
int CheckRot(CMapParts *parts, CMapParts *other, int rotation_offset) {
    if (parts == nullptr || other == nullptr) {
        return 0;
    }

    int rotation = rotation_offset + parts->GetRotY();
    int other_rotation = other->GetRotY();

    if (rotation > 2) {
        rotation -= 4;
    }

    if (rotation < -1) {
        rotation += 4;
    }

    return rotation == other_rotation;
}

} // namespace

void CEditGround::MuskaRequest(CMapParts *(*placed)[64]) {
    parts_info->parts_max = 9;

    if (areas[0] == nullptr) {
        return;
    }

    if (placed[0][0] != nullptr && placed[8][0] != nullptr) {
        CRect_i_ zone(1, 0, 6, 3);

        if (CheckPartsRect(0, 0, zone)) {
            CRect_i_ rect;
            rect.x = rect.y = rect.width = rect.height = 0;
            GetRectDirParts(&rect, placed[0][0], 2, 14);

            if (CheckPartsRect(8, 0, rect) && CheckRot(placed[0][0], placed[8][0], 1) && parts_info->GetCompEvent(0)) {
                parts_info->request[0] = 1;
            }
        }
    }

    if (placed[1][0] != nullptr && placed[10][0] != nullptr) {
        CRect_i_ rect;
        rect.x = rect.y = rect.width = rect.height = 0;
        GetRectDirParts(&rect, placed[1][0], 2, 14);

        if (CheckPartsRect(10, 0, rect) && CheckRot(placed[1][0], placed[10][0], 1)) {
            parts_info->request[1] = 1;
        }
    }

    if (placed[2][0] != nullptr && placed[9][0] != nullptr) {
        CRect_i_ rect;
        rect.x = rect.y = rect.width = rect.height = 0;
        GetRectDirParts(&rect, placed[2][0], 2, 14);

        if (CheckPartsRect(9, 0, rect) && CheckRot(placed[2][0], placed[9][0], 0)) {
            parts_info->request[2] = 1;
        }
    }

    if (placed[3][0] != nullptr && placed[9][0] != nullptr) {
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

    if (placed[4][0] != nullptr && placed[9][0] != nullptr) {
        CRect_i_ rect;
        rect.x = rect.y = rect.width = rect.height = 0;
        GetRectDirParts(&rect, placed[4][0], 2, 14);

        if (CheckPartsRect(9, 0, rect) && CheckRot(placed[4][0], placed[9][0], 1)) {
            parts_info->request[4] = 1;
        }
    }

    if (placed[5][0] != nullptr && placed[10][0] != nullptr) {
        CRect_i_ rect;
        rect.x = rect.y = rect.width = rect.height = 0;
        GetRectDirParts(&rect, placed[5][0], 2, 14);

        if (CheckPartsRect(10, 0, rect) && CheckRot(placed[5][0], placed[10][0], 0)) {
            parts_info->request[5] = 1;
        }
    }

    if (placed[6][0] != nullptr && placed[8][0] != nullptr) {
        CRect_i_ rect;
        rect.x = rect.y = rect.width = rect.height = 0;
        GetRectDirParts(&rect, placed[6][0], 2, 14);

        if (CheckPartsRect(8, 0, rect) && CheckRot(placed[6][0], placed[8][0], 0) && placed[6][0]->GetRotY() == 1) {
            parts_info->request[6] = 1;
        }
    }

    if (placed[7][0] != nullptr && placed[8][0] != nullptr) {
        CRect_i_ rect;
        rect.x = rect.y = rect.width = rect.height = 0;
        GetRectDirParts(&rect, placed[7][0], 2, 14);

        if (CheckPartsRect(8, 0, rect) && CheckRot(placed[7][0], placed[8][0], 2)) {
            parts_info->request[7] = 1;
        }
    }

    if (placed[10][0] != nullptr) {
        CRect_i_ zone(2, 0, 3, 14);

        if (CheckPartsRect(10, 0, zone) && placed[10][0]->GetRotY() == 0) {
            parts_info->request[10] = 1;
        }
    }
}

void CEditGround::YellowRequest(CMapParts *(*placed)[64]) {
    parts_info->parts_max = 13;

    if (areas[0] == nullptr) {
        return;
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

    if (placed[7][0] != nullptr) {
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

    for (int i = 0; i < 24; i++) {
        int on = 2;
        plot_parts[i].FrameObjectOnOff("setuzoku", on);

        if (placed[i][0] != nullptr) {
            sceVu0FVECTOR position;
            placed[i][0]->GetPosition(position);

            if (parts_info->request[i] && position[1] < 1.0f) {
                on = 1;
            }

            plot_parts[i].FrameObjectOnOff("setuzoku", on);
        }
    }
}

} // namespace dc::game
