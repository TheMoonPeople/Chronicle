#include "editarea.hpp"

#include <cstring>

#include "collision.hpp"
#include "editground.hpp"
#include "frame.hpp"
#include "framevu1.hpp"
#include "mapparts.hpp"
#include "mdt.hpp"
#include "mglib.hpp"
#include "rect.hpp"
#include "vector3.hpp"
#include "visualvu1.hpp"

/**
 * Identifies map-part attributes used by connection queries.
 */
// clang-format off
enum MapConnectionAttribute {
    MAP_CONNECTION_ROAD = 1,
    MAP_CONNECTION_RIVER = 2
};

// clang-format on

/**
 * Identifies the shape encoded by connected map-part neighbors.
 */
// clang-format off
enum MapConnectionShape {
    MAP_CONNECTION_CORNER = 1,
    MAP_CONNECTION_STRAIGHT = 2,
    MAP_CONNECTION_THREE_WAY = 3,
    MAP_CONNECTION_FOUR_WAY = 4,
    MAP_CONNECTION_ISOLATED = 5,
    MAP_CONNECTION_END = 6
};

// clang-format on

/**
 * Views the metadata of a grid collision triangle as the surface attributes it holds.
 */
union GridPolyInfo {
    CCPolyInfo info; /**< Metadata as the collision triangle stores it. */

    struct {
        s16 ground_kind; /**< What the surface is made of. */
        s16 foot_sound;  /**< Sound the character's feet play on it. */
    } attr;              /**< Surface attributes at the start of the metadata. */
};

void CEditArea::SetSize(s32 width, s32 height, float unit_size, float unit_alt) {
    this->width = width;
    this->height = height;
    this->unit_size = unit_size;
    this->unit_alt = unit_alt;
}

void CEditArea::GetPos(CVector3_i_ *position, float x, float y, float z) {
    x -= offset_x;
    y -= offset_y;
    z -= offset_z;
    x += 0.1f;
    z += 0.1f;
    position->x = (int) (x / unit_size);
    position->y = (int) (y / unit_alt);
    position->z = (int) (z / unit_size);
}

void CEditArea::GetPos(CVector3_f_ *position, s32 x, s32 y, s32 z) {
    position->x = this->offset_x + ((float) x * this->unit_size);
    position->y = this->offset_y + ((float) y * this->unit_alt);
    position->z = this->offset_z + ((float) z * this->unit_size);
}

void CEditArea::SetPartsNo(int x, int y, int parts_no) {
    if (x < 0 || x >= width) {
        return;
    }

    if (y < 0 || y >= height) {
        return;
    }

    grid[x][y].parts_no = parts_no;
}

void CEditArea::SetPartsID(int x, int y, int parts_id) {
    if (x < 0 || x >= width) {
        return;
    }

    if (y < 0 || y >= height) {
        return;
    }

    grid[x][y].parts_id = parts_id;
}

void CEditArea::SetPartsExtra(int x, int y, int parts_extra) {
    if (x < 0 || x >= width) {
        return;
    }

    if (y < 0 || y >= height) {
        return;
    }

    grid[x][y].parts_extra = parts_extra;
}

void CEditArea::SetCode(int x, int y, int code) {
    if (x < 0 || x >= width) {
        return;
    }

    if (y < 0 || y >= height) {
        return;
    }

    grid[x][y].code = code;
}

int CEditArea::GetPartsID(int x, int y) {
    if (x < 0 || x >= width) {
        return -1;
    }

    if (y < 0 || y >= height) {
        return -1;
    }

    return grid[x][y].parts_id;
}

int CEditArea::GetCode(int x, int y) {
    if (x < 0 || x >= width) {
        return -1;
    }

    if (y < 0 || y >= height) {
        return -1;
    }

    return grid[x][y].code;
}

void CEditArea::SetAlt(int x, int y, int altitude) {
    if (x < 0 || x >= width) {
        return;
    }

    if (y < 0 || y >= height) {
        return;
    }

    if (altitude < 0) {
        altitude = 0;
    }

    if (altitude >= 128) {
        altitude = 127;
    }

    grid[x][y].altitude = altitude;
}

int CEditArea::GetAlt_i(int x, int y) {
    if (x < 0 || x >= width) {
        return 0;
    }

    if (y < 0 || y >= height) {
        return 0;
    }

    return grid[x][y].altitude;
}

float CEditArea::GetAlt(int x, int y) {
    if (x < 0 || x >= width) {
        return 0.0f;
    }

    if (y < 0 || y >= height) {
        return 0.0f;
    }

    return offset_y + static_cast<float>(grid[x][y].altitude) * unit_alt;
}

float CEditArea::GetAlt(float x, float y, float z) {
    CVector3_i_ position;
    GetPos(&position, x, y, z);
    return GetAlt(position.x, position.z);
}

int CEditArea::GetAlt_i(float x, float y, float z) {
    CVector3_i_ position;
    GetPos(&position, x, y, z);
    return GetAlt_i(position.x, position.z);
}

int CEditArea::GetPartsExtra(int x, int y) {
    switch (map_no) {
        case TOWN_MATATAKI:
            switch (area_id) {
                case 0:
                    if (x == 5 && y == -1) {
                        return MAP_CONNECTION_RIVER;
                    }

                    if (x == 2 && y == 8) {
                        return MAP_CONNECTION_RIVER;
                    }

                    break;
                case 1:
                    if (x == 3 && y == 6) {
                        return MAP_CONNECTION_RIVER;
                    }

                    if (x == 12 && y == 3) {
                        return MAP_CONNECTION_RIVER;
                    }

                    break;
                case 2:
                    if (x == 4 && y == -1) {
                        return MAP_CONNECTION_RIVER;
                    }

                    if (x == 3 && y == 8) {
                        return MAP_CONNECTION_RIVER;
                    }

                    break;
            }
    }

    if (x < 0 || x >= width) {
        return -1;
    }

    if (y < 0 || y >= height) {
        return -1;
    }

    return grid[x][y].parts_extra;
}

int CEditArea::SetMapParts(int parts_id, CMapParts *parts, float x, float y, float z, int rot_y) {
    CVector3_i_ position;
    int         i;
    int         j;
    CMapParts  *target = &parts[parts_id];
    target->unit_size = unit_size;
    int parts_width = target->GetWidth();
    int parts_height = target->GetHeight();
    GetPos(&position, x, y, z);

    for (i = 0; i < parts_width; i++) {
        for (j = 0; j < parts_height; j++) {
            int half_width = parts_width >> 1;
            int cell_x = i + (position.x - half_width);
            int half_height = parts_height >> 1;
            int cell_y = j + (position.z - half_height);
            int cell_code = target->GetInfoData(i, j);

            if (cell_code != 0) {
                SetCode(cell_x, cell_y, cell_code);

                if (cell_code < 0x80) {
                    AddAlt(cell_x, cell_y, cell_code);
                } else {
                    SetPartsNo(cell_x, cell_y, target->handle);
                    SetPartsID(cell_x, cell_y, parts_id);
                    SetPartsExtra(cell_x, cell_y, target->subtype);
                }
            }
        }
    }

    grid_redraw = 1;
    return 1;
}

int CEditArea::DeleteMapParts(int parts_no, CMapParts *parts, float x, float y, float z) {
    CVector3_i_ position;
    GetPos(&position, x, y, z);

    if (parts_no < 0) {
        return 0;
    }

    CMapParts *target = &parts[parts_no];
    int        parts_width = target->GetWidth();
    int        parts_height = target->GetHeight();

    for (int i = 0; i < parts_width; i++) {
        for (int j = 0; j < parts_height; j++) {
            int half_width = parts_width >> 1;
            int cell_x = i + (position.x - half_width);
            int half_height = parts_height >> 1;
            int cell_y = j + (position.z - half_height);
            s16 cell_code = target->GetInfoData(i, j);

            if (cell_code != 0) {
                if (cell_code < 0x80) {
                    int occupant = GetPartsID(cell_x, cell_y);
                    AddAlt(cell_x, cell_y, -cell_code);

                    if (occupant >= 0) {
                        CMapParts    *occupant_parts = &parts[occupant];
                        sceVu0FVECTOR occupant_position;
                        occupant_parts->GetPosition(occupant_position);
                        occupant_position[1] = GetAlt(cell_x, cell_y);
                        occupant_parts->SetPosition(occupant_position);
                    }
                } else {
                    SetPartsNo(cell_x, cell_y, -1);
                    SetPartsID(cell_x, cell_y, -1);
                    SetPartsExtra(cell_x, cell_y, -1);
                    SetCode(cell_x, cell_y, -1);
                }
            }
        }
    }

    grid_redraw = 1;
    return 1;
}

int CEditArea::SetRiverParts(int x, int y) {
    int neighbors[4];
    int negative_z, positive_x, positive_z, negative_x;
    int count, shape, direction, i;

    if ((x < 0) || (x >= this->width)) {
        return -1;
    }

    if ((y < 0) || (y >= this->height)) {
        return -1;
    }

    if (GetPartsExtra(x, y) != MAP_CONNECTION_RIVER) {
        return -1;
    }

    negative_z = GetPartsExtra(x, y - 1);
    positive_x = GetPartsExtra(x + 1, y);
    positive_z = GetPartsExtra(x, y + 1);
    negative_x = GetPartsExtra(x - 1, y);
    neighbors[0] = negative_z == MAP_CONNECTION_RIVER;
    neighbors[1] = positive_x == MAP_CONNECTION_RIVER;
    neighbors[2] = positive_z == MAP_CONNECTION_RIVER;
    neighbors[3] = negative_x == MAP_CONNECTION_RIVER;
    neighbors[0] = neighbors[0] || negative_z == MAP_PARTS_SUBTYPE_BRIDGE;
    neighbors[1] = neighbors[1] || positive_x == MAP_PARTS_SUBTYPE_BRIDGE;
    neighbors[2] = neighbors[2] || positive_z == MAP_PARTS_SUBTYPE_BRIDGE;
    neighbors[3] = neighbors[3] || negative_x == MAP_PARTS_SUBTYPE_BRIDGE;
    neighbors[0] = neighbors[0] || negative_z == MAP_PARTS_SUBTYPE_ON_RIVER;
    neighbors[1] = neighbors[1] || positive_x == MAP_PARTS_SUBTYPE_ON_RIVER;
    neighbors[2] = neighbors[2] || positive_z == MAP_PARTS_SUBTYPE_ON_RIVER;
    neighbors[3] = neighbors[3] || negative_x == MAP_PARTS_SUBTYPE_ON_RIVER;
    count = 0;

    for (i = 0; i < 4; i++) {
        count += neighbors[i];
    }

    shape = -1;
    direction = 0;

    if (count == 4) {
        shape = MAP_CONNECTION_FOUR_WAY;
    }

    if (count == 3) {
        shape = MAP_CONNECTION_THREE_WAY;

        if (neighbors[0] == 0) {
            direction = 3;
        }

        if (neighbors[1] == 0) {
            direction = 2;
        }

        if (neighbors[2] == 0) {
            direction = 1;
        }

        if (neighbors[3] == 0) {
            direction = 0;
        }
    }

    if (count == 2) {
        if (((neighbors[0] != 0) && (neighbors[2] != 0)) || ((neighbors[1] != 0) && (neighbors[3] != 0))) {
            shape = MAP_CONNECTION_STRAIGHT;

            if (neighbors[0] != 0) {
                direction = 0;
            }

            if (neighbors[1] != 0) {
                direction = 1;
            }
        } else {
            shape = MAP_CONNECTION_CORNER;

            if ((neighbors[1] != 0) && (neighbors[2] != 0)) {
                direction = 3;
            }

            if ((neighbors[2] != 0) && (neighbors[3] != 0)) {
                direction = 2;
            }

            if ((neighbors[3] != 0) && (neighbors[0] != 0)) {
                direction = 1;
            }

            if ((neighbors[0] != 0) && (neighbors[1] != 0)) {
                direction = 0;
            }
        }
    }

    if (count == 1) {
        shape = MAP_CONNECTION_END;

        if (neighbors[0] != 0) {
            direction = 2;
        }

        if (neighbors[1] != 0) {
            direction = 1;
        }

        if (neighbors[2] != 0) {
            direction = 0;
        }

        if (neighbors[3] != 0) {
            direction = 3;
        }
    }

    if (count == 0) {
        shape = MAP_CONNECTION_ISOLATED;
    }

    if (shape < 0) {
        return -1;
    }

    shape = (shape << 4) & 0xFF0;
    return shape | (direction & 0xF);
}

int CEditArea::SetRoadParts(int x, int y) {
    int neighbors[4];
    int count;
    int shape;
    int direction;
    int i;

    if ((x < 0) || (x >= this->width)) {
        return -1;
    }

    if ((y < 0) || (y >= this->height)) {
        return -1;
    }

    if (GetPartsExtra(x, y) != MAP_CONNECTION_ROAD) {
        return -1;
    }

    neighbors[0] = GetPartsExtra(x, y - 1) == MAP_CONNECTION_ROAD;
    neighbors[1] = GetPartsExtra(x + 1, y) == MAP_CONNECTION_ROAD;
    neighbors[2] = GetPartsExtra(x, y + 1) == MAP_CONNECTION_ROAD;
    neighbors[3] = GetPartsExtra(x - 1, y) == MAP_CONNECTION_ROAD;
    count = 0;

    for (i = 0; i < 4; i++) {
        count += neighbors[i];
    }

    shape = -1;
    direction = 0;

    if (count == 4) {
        shape = MAP_CONNECTION_FOUR_WAY;
    }

    if (count == 3) {
        shape = MAP_CONNECTION_THREE_WAY;

        if (neighbors[0] == 0) {
            direction = 0;
        }

        if (neighbors[1] == 0) {
            direction = 3;
        }

        if (neighbors[2] == 0) {
            direction = 2;
        }

        if (neighbors[3] == 0) {
            direction = 1;
        }
    }

    if (count == 2) {
        if (((neighbors[0] != 0) && (neighbors[2] != 0)) || ((neighbors[1] != 0) && (neighbors[3] != 0))) {
            shape = MAP_CONNECTION_STRAIGHT;

            if (neighbors[0] != 0) {
                direction = 0;
            }

            if (neighbors[1] != 0) {
                direction = 1;
            }
        } else {
            shape = MAP_CONNECTION_CORNER;

            if ((neighbors[1] != 0) && (neighbors[2] != 0)) {
                direction = 0;
            }

            if ((neighbors[2] != 0) && (neighbors[3] != 0)) {
                direction = 3;
            }

            if ((neighbors[3] != 0) && (neighbors[0] != 0)) {
                direction = 2;
            }

            if ((neighbors[0] != 0) && (neighbors[1] != 0)) {
                direction = 1;
            }
        }
    }

    if (count == 1) {
        shape = MAP_CONNECTION_END;

        if (neighbors[0] != 0) {
            direction = 2;
        }

        if (neighbors[1] != 0) {
            direction = 1;
        }

        if (neighbors[2] != 0) {
            direction = 0;
        }

        if (neighbors[3] != 0) {
            direction = 3;
        }
    }

    if (count == 0) {
        shape = MAP_CONNECTION_ISOLATED;
    }

    if (shape < 0) {
        return -1;
    }

    shape = (shape << 4) & 0xFF0;
    return shape | (direction & 0xF);
}

int CEditArea::SearchPartsID(float x, float y, float z) {
    CVector3_i_ position;
    GetPos(&position, x, y, z);
    return grid[position.x][position.z].parts_id;
}

int CEditArea::SearchPartsExtra(float x, float y, float z) {
    CVector3_i_ position;
    GetPos(&position, x, y, z);
    return GetPartsExtra(position.x, position.z);
}

void CEditArea::GetGrid(CVector3_f_ *snapped, float x, float y, float z) {
    CVector3_i_ cell;
    GetPos(&cell, x, y, z);
    GetPos(snapped, cell.x, cell.y, cell.z);
}

void CEditArea::RemakeGrid() {
    if (grid_frame == NULL) {
        return;
    }

    CVisualVu1 *visual = grid_frame->GetVisual();

    if (visual == NULL) {
        return;
    }

    MDT_HEADER *model = (MDT_HEADER *) visual->GetMDTDataAddress();

    if (model == NULL) {
        return;
    }

    sceVu0FVECTOR *vertices = (sceVu0FVECTOR *) ((char *) model + model->vertex_ofs);
    int            y;
    int            x;

    for (x = 0; x < width; x++) {
        for (y = 0; y < height; y++) {
            int index = x + y * width;
            index *= 4;

            if (GetCode(x, y) == 0x81) {
                sceVu0CopyVector(vertices[index + 1], vertices[index]);
                sceVu0CopyVector(vertices[index + 2], vertices[index]);
                sceVu0CopyVector(vertices[index + 3], vertices[index]);
            } else {
                vertices[index + 1][0] = 1.002f * unit_size + vertices[index][0];
                vertices[index + 1][2] = vertices[index][2];
                vertices[index + 2][0] = vertices[index][0];
                vertices[index + 2][2] = 1.002f * unit_size + vertices[index][2];
                vertices[index + 3][0] = 1.002f * unit_size + vertices[index][0];
                vertices[index + 3][2] = 1.002f * unit_size + vertices[index][2];
            }
        }
    }

    grid_frame->attr.remake_pending = 1;
}

void CEditArea::GetPartsBox(CBoxVu0 *box) {
    memcpy(box, &this->parts_box, sizeof(CBoxVu0));
}

void CEditArea::MakePartsBox() {
    CVector3_f_ position;
    int         min_x = 0;
    int         max_x = 0;
    int         max_z = 0;
    int         min_z = 0;

    for (int x = 0; x < width; x++) {
        for (int z = 0; z < height; z++) {
            if (grid[x][z].parts_no >= 0 && grid[x][z].parts_id >= 0) {
                if (max_x < x) {
                    max_x = x;
                }

                if (max_z < z) {
                    max_z = z;
                }

                if (min_x > x) {
                    max_x = x;
                }

                if (z < min_z) {
                    min_z = z;
                }
            }
        }
    }

    GetPos(&position, max_x, 0, max_z);
    parts_box.max[0] = position.x + unit_size;
    parts_box.max[1] = position.y;
    parts_box.max[2] = position.z + unit_size;
    parts_box.max[3] = 1.0f;
    GetPos(&position, 0, 0, min_z);
    parts_box.min[0] = position.x;
    parts_box.min[1] = position.y;
    parts_box.min[2] = position.z;
    parts_box.min[3] = 1.0f;
}

int CEditArea::CheckArea(float x, float y, float z) {
    if (x < offset_x) {
        return 0;
    }

    if (z < offset_z) {
        return 0;
    }

    if (x > offset_x + unit_size * static_cast<float>(width)) {
        return 0;
    }

    if (z > offset_z + unit_size * static_cast<float>(height)) {
        return 0;
    }

    return 1;
}

int CEditArea::CheckAreaRect(float x, float y, float z, int rect_width, int rect_height) {
    CVector3_i_ position;
    GetPos(&position, x, y, z);
    int bottom;
    int right;
    int left;
    int top;
    int half_width = rect_width >> 1;
    left = position.x - half_width;
    right = rect_width + left - 1;
    int half_height = rect_height >> 1;
    top = position.z - half_height;
    bottom = rect_height + top - 1;

    if (left < 0 || top < 0) {
        return 0;
    }

    if (right >= this->width || bottom >= this->height) {
        return 0;
    }

    return 1;
}

int CEditArea::CheckParts(CMapParts *parts, float x, float y, float z, int rotation) {
    if (parts->info == NULL) {
        return 0;
    }

    CVector3_i_ position;
    GetPos(&position, x, y, z);

    if (parts->subtype == MAP_PARTS_SUBTYPE_ON_RIVER) {
        if (GetPartsExtra(position.x, position.z) != MAP_PARTS_SUBTYPE_RIVER) {
            return 0;
        }

        return 1;
    }

    parts->SetRotY(rotation);
    int i;
    int j;
    int part_width = parts->GetWidth();
    int part_height = parts->GetHeight();

    for (i = 0; i < part_width; i++) {
        for (j = 0; j < part_height; j++) {
            int half_width = part_width >> 1;
            int cell_x = i + (position.x - half_width);
            int half_height = part_height >> 1;
            int cell_y = j + (position.z - half_height);

            if (cell_x < 0 || cell_x >= this->width) {
                return 0;
            }

            if (cell_y < 0 || cell_y >= this->height) {
                return 0;
            }

            s16 cell_code = parts->GetInfoData(i, j);

            if (cell_code != 0) {
                int occupant = grid[cell_x][cell_y].parts_no;
                int extra = GetPartsExtra(cell_x, cell_y);

                if ((parts->subtype == MAP_PARTS_SUBTYPE_ROAD || extra != MAP_PARTS_SUBTYPE_ROAD) && occupant >= 0) {
                    return 0;
                }
            }
        }
    }

    if (parts->subtype == MAP_PARTS_SUBTYPE_RIVER) {
        if (GetAlt_i(position.x, position.z) > 0) {
            return 0;
        }

        int neighbors[4];
        neighbors[0] = GetPartsExtra(position.x + 1, position.z);
        neighbors[1] = GetPartsExtra(position.x - 1, position.z);
        neighbors[2] = GetPartsExtra(position.x, position.z - 1);
        neighbors[3] = GetPartsExtra(position.x, position.z + 1);

        for (int i = 0; i < 4; i++) {
            int extra = neighbors[i];

            if (extra == MAP_PARTS_SUBTYPE_BRIDGE) {
                return 0;
            }

            if (extra == MAP_PARTS_SUBTYPE_LAKE) {
                return 0;
            }

            if (extra == MAP_PARTS_SUBTYPE_ON_RIVER) {
                return 0;
            }

            if (extra == MAP_PARTS_SUBTYPE_ON_RIVER) {
                return 0;
            }
        }
    }

    if (parts->subtype == MAP_PARTS_SUBTYPE_LAKE) {
        if (GetAlt_i(position.x, position.z) > 0) {
            return 0;
        }

        for (int i = -1; i < part_width + 1; i++) {
            for (int j = -1; j < part_height + 1; j++) {
                int half_width = part_width >> 1;
                int cell_x = i + (position.x - half_width);
                int extra = GetPartsExtra(cell_x, j + (position.z - (part_height >> 1)));

                if (extra == MAP_PARTS_SUBTYPE_BRIDGE) {
                    return 0;
                }

                if (extra == MAP_PARTS_SUBTYPE_RIVER) {
                    return 0;
                }

                if (extra == MAP_PARTS_SUBTYPE_ON_RIVER) {
                    return 0;
                }
            }
        }
    }

    return 1;
}

int CEditArea::PickUpPoly(CCPoly *polygons, float x, float y, float z) {
    CVector3_i_ position;

    if (CheckArea(x, y, z) == 0) {
        return 0;
    }

    GetPos(&position, x, y, z);
    CRect_i_ rect(position.x - 1, position.z - 1, 2, 2);
    return PickUpPoly(polygons, rect);
}

int CEditArea::PickUpPoly(CCPoly *polygons, CRect_i_ rect) {
    static int sound[] = {0, 1, 0, 14, 2};
    int        count = 0;

    for (int x = rect.x; x < rect.x + rect.width; x++) {
        for (int y = rect.y; y < rect.y + rect.height; y++) {
            if (x >= 0 && x < width && y >= 0 && y < height && GetCode(x, y) != 0x81) {
                CVector3_f_   position;
                sceVu0FVECTOR corner[4];
                sceVu0FVECTOR normal;
                GridPolyInfo  surface;
                GetPos(&position, x, 0, y);
                memset(&surface, 0, sizeof(surface));
                surface.attr.foot_sound = sound[map_no];
                corner[0][0] = position.x - 0.01f * unit_size;
                corner[0][1] = position.y;
                corner[0][2] = position.z - 0.01f * unit_size;
                corner[0][3] = 1.0f;
                sceVu0CopyVector(corner[1], corner[0]);
                sceVu0CopyVector(corner[2], corner[0]);
                sceVu0CopyVector(corner[3], corner[0]);
                corner[1][0] = 0.02f * unit_size + (corner[0][0] + unit_size);
                corner[2][2] = 0.02f * unit_size + (corner[0][2] + unit_size);
                corner[3][0] = 0.02f * unit_size + (corner[0][0] + unit_size);
                corner[3][2] = 0.02f * unit_size + (corner[0][2] + unit_size);
                normal[0] = 0.0f;
                normal[1] = 1.0f;
                normal[2] = 0.0f;
                normal[3] = 0.0f;
                sceVu0CopyVector(polygons->vertex[0], corner[0]);
                sceVu0CopyVector(polygons->vertex[1], corner[1]);
                sceVu0CopyVector(polygons->vertex[2], corner[2]);
                sceVu0CopyVector(polygons->normal, normal);
                polygons->info = surface.info;
                polygons++;
                sceVu0CopyVector(polygons->vertex[0], corner[2]);
                sceVu0CopyVector(polygons->vertex[1], corner[1]);
                sceVu0CopyVector(polygons->vertex[2], corner[3]);
                sceVu0CopyVector(polygons->normal, normal);
                polygons->info = surface.info;
                polygons++;
                count += 2;
            }
        }
    }

    return count;
}

int CEditArea::PickUpPoly(CCPoly *polygons, CBoxVu0 box) {
    int         left, top;
    CVector3_i_ position;
    GetPos(&position, box.min[0], box.min[1], box.min[2]);
    left = position.x;
    top = position.z;
    GetPos(&position, box.max[0], box.max[1], box.max[2]);
    int      right = position.x;
    int      bottom = position.z;
    CRect_i_ rect;
    rect.x = left - 1;
    rect.y = top - 1;
    rect.width = right - left + 2;
    rect.height = bottom - top + 2;
    return PickUpPoly(polygons, rect);
}

int CEditArea::GetPartsRect(CRect_i_ &rect, int *parts_ids, int capacity) {
    int x, y;
    int count = 0;

    for (x = rect.x; x < rect.x + rect.width; x++) {
        for (y = rect.y; y < rect.y + rect.height; y++) {
            if (count >= capacity) {
                break;
            }

            int parts_id = GetPartsID(x, y);

            if (parts_id >= 0) {
                if (count == 0) {
                    parts_ids[count++] = parts_id;
                } else {
                    int found = 0;

                    for (int i = 0; i < count; i++) {
                        if (parts_id == parts_ids[i]) {
                            found = 1;
                        }
                    }

                    if (!found) {
                        parts_ids[count++] = parts_id;
                    }
                }
            }
        }
    }

    return count;
}

void CEditArea::ChainWorkClear() {
    for (int x = 0; x < 16; x++) {
        for (int y = 0; y < 16; y++) {
            chain_work[x][y] = 0;
        }
    }
}

int CEditArea::CheckRiverChain(int x, int y, int target_x, int target_y) {
    if (x < 0 || y < 0 || x >= 16 || y >= 16) {
        return 0;
    }

    if (chain_work[x][y] != 0) {
        return 0;
    }

    chain_work[x][y] = 1;
    static int ext = 0;
    ext = GetPartsExtra(x, y);

    if (ext != MAP_CONNECTION_RIVER && ext != MAP_PARTS_SUBTYPE_BRIDGE && ext != MAP_PARTS_SUBTYPE_ON_RIVER) {
        return 0;
    }

    if (x == target_x && y == target_y) {
        return 1;
    }

    if (CheckRiverChain(x - 1, y, target_x, target_y)) {
        return 1;
    }

    if (CheckRiverChain(x + 1, y, target_x, target_y)) {
        return 1;
    }

    if (CheckRiverChain(x, y - 1, target_x, target_y)) {
        return 1;
    }

    return CheckRiverChain(x, y + 1, target_x, target_y) ? 1 : 0;
}

void CEditArea::DrawGrid() {
    sceVu0FVECTOR position;

    grid_redraw_count--;

    if (grid_redraw != 0) {
        grid_redraw_count = 2;
    }

    if (grid_redraw_count > 0) {
        RemakeGrid();

        if (grid_frame != NULL) {
            grid_frame->attr.cull_enable = false;
        }
    } else {
        if (grid_frame != NULL) {
            grid_frame->attr.cull_enable = true;
        }
    }

    grid_redraw = 0;

    if (grid_redraw_count < 0) {
        grid_redraw_count = 0;
    }

    position[0] = offset_x;
    position[1] = offset_y;
    position[2] = offset_z;
    grid_frame->SetPosition(position);
    MGDraw(grid_frame);
}

void CEditArea::Clear() {
    for (int x = 0; x < width; x++) {
        for (int y = 0; y < height; y++) {
            grid[x][y].parts_no = -1;
            grid[x][y].altitude = 0;
            grid[x][y].spare = 0;
            grid[x][y].parts_id = -1;
            grid[x][y].code = -1;
            grid[x][y].parts_extra = -1;
            grid[x][y].spare_index = -1;
        }
    }

    grid_redraw = 1;
    grid_redraw_count = 4;
}

void CEditArea::Initialize() {
    float zero = 0.0f;
    SetOffset(zero, zero, zero);
    SetSize(16, 16, 1.0f, 1.0f);
    grid_frame = NULL;
    Clear();
    map_no = 0;
    area_id = 0;
}

void CEditArea::SetMapInfo(s32 map_no, s32 area_id) {
    this->map_no = map_no;
    this->area_id = area_id;
}

int CEditArea::GetMapNo() {
    return this->map_no;
}

int CEditArea::GetAreaID() {
    return this->area_id;
}

void CEditArea::SetGridFrame(CFrameVu1 *frame) {
    this->grid_frame = frame;
}

CFrameVu1 *CEditArea::GetGridFrame() {
    return this->grid_frame;
}

void CEditArea::SetOffset(float x, float y, float z) {
    this->offset_x = x;
    this->offset_y = y;
    this->offset_z = z;
}

void CEditArea::GetOffset(float *offset) {
    offset[0] = offset_x;
    offset[1] = offset_y;
    offset[2] = offset_z;
}

int CEditArea::GetWidth() {
    return this->width;
}

int CEditArea::GetHeight() {
    return this->height;
}

void CEditArea::AddAlt(int x, int y, int altitude_delta) {
    SetAlt(x, y, GetAlt_i(x, y) + altitude_delta);
}

float CEditArea::GetUnitSize() {
    return this->unit_size;
}

float CEditArea::GetUnitAlt() {
    return this->unit_alt;
}

CEditArea::CEditArea() {
    Initialize();
}
