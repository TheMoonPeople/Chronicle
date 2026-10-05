#include "editground.hpp"

#include <cstdint>
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

#include "platform/config.hpp"

#pragma clang diagnostic ignored "-Wwritable-strings"

// Retail's DrawPartsCursor. LoadObjectParts keeps a part's preview frame in CMapParts' s32
// preview_frame, cast from one of the part's own level-of-detail frames, and nothing else stores
// there; the frame is recovered by matching those bits against the part's frames.

namespace {

float Magnitude(float value) {
    return value < 0.0f ? -value : value;
}

int CheckDelete(CEditArea *area, CMapParts *parts, float x, float y, float z) {
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

CFrame *PreviewFrame(CMapParts *parts, s32 truncated) {
    if (truncated == 0) {
        return NULL;
    }

    for (int k = 3; k >= 0; k--) {
        CFrame        *frame = parts->frame[k];
        std::uintptr_t bits = reinterpret_cast<std::uintptr_t>(frame) & 0xFFFFFFFFu;

        if (frame != NULL && bits == static_cast<u32>(truncated)) {
            return frame;
        }
    }

    return NULL;
}

} // namespace

PC_OVERRIDE void CEditGround::DrawPartsCursor(int plot, float *position, float *model_pos, int rot_y, float *rotation, int area_no) {
    [[maybe_unused]] static int old_parts = -1;
    CVector3_f_     cell;
    CVector3_i_     grid;
    sceVu0FMATRIX   light_direction;
    sceVu0FMATRIX   light_colour;
    sceVu0FVECTOR   ambient;
    int             area_code;
    CEditArea      *area;
    CMapParts      *source;
    int             width;
    int             height;
    int             fits;
    int             occupant;
    EDITPARTS_INFO *info;
    CFrame         *preview;
    CFrame         *model;
    int             saved_draw;
    int             subtype;
    float           step;

    if (plot < 0 || plot >= 24) {
        sceVu0CopyVector(model_pos, position);
        return;
    }

    area_code = GetAreaCode(position[0], position[1], position[2]);

    if (area_code < 0) {
        sceVu0CopyVector(model_pos, position);
        return;
    }

    area = areas[area_code];
    source = &plot_parts[plot];
    width = source->GetWidth();
    height = source->GetHeight();

    if (area->CheckAreaRect(position[0], position[1], position[2], width, height) == 0) {
        sceVu0CopyVector(model_pos, position);
        return;
    }

    area->GetPos(&grid, position[0], position[1], position[2]);
    fits = area->CheckParts(source, position[0], position[1], position[2], rot_y);
    occupant = area->SearchPartsID(position[0], position[1], position[2]);

    if (source->subtype == MAP_PARTS_SUBTYPE_ON_RIVER) {
        if (CheckDelete(area, source, position[0], position[1], position[2]) != 0) {
            fits = 0;
        }

        if (occupant >= 0 && parts[occupant].handle != 1) {
            fits = 0;
        }
    }

    if (parts_info != NULL) {
        info = parts_info->GetPartsInfo(plot);

        if (info != NULL) {
            fits &= info->placed < info->stock;
        }
    }

    area->GetPos(&cell, grid.x, grid.y, grid.z);
    position[0] = cell.x;
    position[1] = 5.0f + cell.y;
    position[2] = cell.z;
    position[3] = 0.0f;
    preview = fits != 0 ? PreviewFrame(source, source->preview_frame) : PreviewFrame(source, source->blocked_preview_frame);

    if (width % 2 == 1) {
        position[0] += 0.5f * area->GetUnitSize();
    }

    if (height % 2 == 1) {
        position[2] += 0.5f * area->GetUnitSize();
    }

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
    step = goal[0] - model_pos[0];

    if (Magnitude(step) < 1.0f) {
        model_pos[0] = goal[0];
    } else {
        model_pos[0] += step / 8.0f;
    }

    step = goal[1] - model_pos[1];

    if (Magnitude(step) < 1.0f) {
        model_pos[1] = goal[1];
    } else {
        model_pos[1] += step / 8.0f;
    }

    step = goal[2] - model_pos[2];

    if (Magnitude(step) < 1.0f) {
        model_pos[2] = goal[2];
    } else {
        model_pos[2] += step / 8.0f;
    }

    if (source->category_no == area_no) {
        subtype = source->subtype;

        if (preview != NULL && subtype != MAP_PARTS_SUBTYPE_RIVER && subtype != MAP_PARTS_SUBTYPE_BRIDGE) {
            model = NULL;

            if (subtype == MAP_PARTS_SUBTYPE_ON_RIVER) {
                model = preview->SearchFrame("kawa");

                if (model != NULL) {
                    saved_draw = model->attr.draw_on;
                    model->attr.draw_on = 2;
                }

                if (occupant >= 0) {
                    CMapParts *target = &parts[occupant];
                    target->GetRotation(rotation);
                }
            }

            float *angle = rotation;
            preview->SetPosition(model_pos);
            preview->SetRotation(angle[0], angle[1], angle[2]);
            MGDraw(preview);

            if (model != NULL) {
                model->attr.draw_on = saved_draw;
            }
        }
    }
}

// Retail's Draw, whose parts step down a level of detail at 50, 300, 500 and 800 from the eye.
// None of those distances is nearer than video.detail_distance, so a part within it draws the
// finest level the caller allows.
PC_OVERRIDE void CEditGround::Draw(float time, int pass, int lowest, int highest, int fixed_lowest, int fixed_highest) {
    sceVu0FVECTOR lod_distance = {50.0f, 300.0f, 500.0f, 800.0f};
    sceVu0FVECTOR position;
    sceVu0FVECTOR ambient;
    float         detail = ConfigDetailDistance();
    int           i;

    for (i = 0; i < 4; i++) {
        if (lod_distance[i] < detail) {
            lod_distance[i] = detail;
        }
    }

    for (i = 0; i < 64; i++) {
        if (fixed_parts[i].category_no == pass) {
            fixed_parts[i].DrawParts(time, lod_distance, fixed_lowest, fixed_highest, NULL);
        }
    }

    CMapParts *object = parts;

    for (i = 0; i < 128; i++, object++) {
        if (object->category_no != pass) {
            continue;
        }

        if (object->handle < 0) {
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

        sceVu0FVECTOR focus_ambient = {32.0f, 32.0f, 128.0f, 128.0f};
        MGGetAmbient(ambient);

        if (focus_parts_id == i) {
            MGSetAmbient(focus_ambient);
        }

        object->DrawParts(time, lod_distance, lowest, highest, NULL);
        MGSetAmbient(ambient);
    }
}

// Retail's DrawShadow, which casts a part's shadow by its depth from the eye: EditLoop composites
// the parts nearer than the map's shadow_near (320 by default) at shadow_mode (0x34) and those out
// to shadow_far (740) at the fainter shadow_mode_2 (0x20), and past that a part casts none. A part
// deeper than 300 takes the fast volume program, and one with a draw_distance casts nothing past
// it. A part within video.shadow_distance of the eye belongs to the near band, with the precise
// program and whatever its draw_distance, so its shadow is as dark and as exact as up close. The
// overhead view of georama mode (pass 1) keeps its own.
PC_OVERRIDE void CEditGround::DrawShadow(int pass, float near_distance, float far_distance) {
    sceVu0FVECTOR position;
    sceVu0FVECTOR view;
    sceVu0FVECTOR clip_position;
    int           i;
    CMapParts    *object;
    int           fast;
    float         full = pass == 0 ? ConfigShadowDistance() : 0.0f;
    float         fast_distance = full > 300.0f ? full : 300.0f;

    if (near_distance < 1.0f) {
        near_distance = -10000000;

        if (far_distance < full) {
            far_distance = full;
        }
    } else {
        if (near_distance < full) {
            near_distance = full;
        }

        if (far_distance < near_distance) {
            far_distance = near_distance;
        }
    }

    object = parts;

    for (i = 0; i < 128; i++, object++) {
        if (object->category_no < 0) {
            continue;
        }

        if (object->handle < 0) {
            continue;
        }

        if (object->area >= 0 && area_visible[object->area] == 0) {
            continue;
        }

        if (!(clip_plane[3] <= 0.0f)) {
            object->GetPosition(clip_position);

            if (!(DistVector(clip_position, clip_plane) <= clip_plane[3]) && !object->ChangeDigData()) {
                continue;
            }
        }

        object->GetPosition(position);
        position[3] = 1.0f;
        sceVu0ApplyMatrix(view, mgRenderInfo.view_scaled, position);

        if (!(object->draw_distance <= 0.0f) && pass == 0) {
            float distance = DistVector(view);

            if (object->draw_distance < distance && !(distance < full)) {
                continue;
            }
        }

        if (view[2] > near_distance && view[2] < far_distance) {
            object->DrawShade();

            if (object->shadow_frame != NULL) {
                fast = pass != 0;

                if (view[2] > fast_distance) {
                    fast = 1;
                }

                object->DrawShadow(fast);
            }
        }
    }

    for (int j = 0; j < 64; j++) {
        CMapParts *fixed = &fixed_parts[j];

        if (fixed->category_no < 0) {
            continue;
        }

        if (fixed->handle < 0) {
            continue;
        }

        sceVu0CopyVector(position, fixed->pos);
        position[3] = 1.0f;
        sceVu0ApplyMatrix(view, mgRenderInfo.view_scaled, position);

        if (view[2] > near_distance && view[2] < far_distance) {
            fixed->DrawShade();

            if (fixed->shadow_frame != NULL) {
                fast = pass != 0;

                if (view[2] > fast_distance) {
                    fast = 1;
                }

                fixed->DrawShadow(fast);
            }
        }
    }
}
