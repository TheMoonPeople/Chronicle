#include "editground.hpp"

#include <algorithm>
#include <cstdint>

#include "camera.hpp"
#include "camerafollow.hpp"
#include "dataread.hpp"
#include "editarea.hpp"
#include "editground_port.hpp"
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

    if (area->GetMapNo() != TOWN_MATATAKI || kind == MAP_PARTS_SUBTYPE_BRIDGE) {
        return 0;
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
        default:
            break;
    }

    return 0;
}

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

namespace {

// The shadows of the parts whose depth from the eye lies between near_distance and far_distance.
// A part within full of the eye takes the precise volume program whatever its draw_distance; past
// it, one deeper than 300 takes the fast program and one with a draw_distance casts nothing past it.
void DrawShadowBetween(CEditGround &ground, int pass, float near_distance, float far_distance, float full) {
    sceVu0FVECTOR position;
    sceVu0FVECTOR view;
    sceVu0FVECTOR clip_position;
    float         fast_distance = full > 300.0f ? full : 300.0f;

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
            object.DrawShade();

            if (object.shadow_frame != nullptr) {
                int fast = pass != 0;

                if (view[2] > fast_distance) {
                    fast = 1;
                }

                object.DrawShadow(fast);
            }
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
            fixed.DrawShade();

            if (fixed.shadow_frame != nullptr) {
                int fast = pass != 0;

                if (view[2] > fast_distance) {
                    fast = 1;
                }

                fixed.DrawShadow(fast);
            }
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
    float full = ConfigShadowDistance();

    if (full > shadow_near) {
        DrawShadowBetween(*ground, 0, shadow_near, full, full);
    }
}
