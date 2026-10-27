#include "editarea.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>

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

namespace dc::game {

namespace {

/**
 * Identifies the shape encoded by connected map-part neighbors.
 */
enum class MapConnectionShape : int {
    CORNER = 1,
    STRAIGHT = 2,
    THREE_WAY = 3,
    FOUR_WAY = 4,
    ISOLATED = 5,
    END = 6,
};

/**
 * Lays out the metadata of a grid collision triangle as the surface attributes it holds.
 */
struct GridSurfaceAttributes {
    s16                                              ground_kind; /**< What the surface is made of. */
    s16                                              foot_sound;  /**< Sound the character's feet play on it. */
    std::array<std::uint8_t, sizeof(CCPolyInfo) - 4> unk_04;
};

static_assert(sizeof(GridSurfaceAttributes) == sizeof(CCPolyInfo));

/** Foot sound of the grid floor, indexed by map number. */
constexpr std::array<s16, 5> kGridFootSounds = {0, 1, 0, 14, 2};

constexpr int EncodeConnection(MapConnectionShape shape, int direction) {
    return ((static_cast<int>(shape) << 4) & 0xFF0) | (direction & 0xF);
}

/**
 * Encodes the shape and direction of a connected cell from its four neighbors.
 *
 * Neighbors are ordered -Z, +X, +Z, -X. three_way_directions is indexed by the
 * missing neighbor; corner_directions by the first neighbor of the connected
 * pair (k, k + 1 mod 4).
 */
int EncodeNeighbors(const std::array<bool, 4> &neighbors, const std::array<int, 4> &three_way_directions,
                    const std::array<int, 4> &corner_directions) {
    constexpr std::array<int, 4> end_directions = {2, 1, 0, 3};

    switch (std::ranges::count(neighbors, true)) {
        case 4:
            return EncodeConnection(MapConnectionShape::FOUR_WAY, 0);
        case 3: {
            const auto missing = std::ranges::find(neighbors, false) - neighbors.begin();
            return EncodeConnection(MapConnectionShape::THREE_WAY, three_way_directions[missing]);
        }
        case 2: {
            if ((neighbors[0] && neighbors[2]) || (neighbors[1] && neighbors[3])) {
                return EncodeConnection(MapConnectionShape::STRAIGHT, neighbors[1] ? 1 : 0);
            }

            std::size_t first = 3;

            for (std::size_t side = 0; side < 3; side++) {
                if (neighbors[side] && neighbors[side + 1]) {
                    first = side;
                    break;
                }
            }

            return EncodeConnection(MapConnectionShape::CORNER, corner_directions[first]);
        }
        case 1: {
            const auto connected = std::ranges::find(neighbors, true) - neighbors.begin();
            return EncodeConnection(MapConnectionShape::END, end_directions[connected]);
        }
        default:
            return EncodeConnection(MapConnectionShape::ISOLATED, 0);
    }
}

} // namespace

void CEditArea::SetSize(int width, int height, float unit_size, float unit_alt) {
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
    position->x = static_cast<int>(x / unit_size);
    position->y = static_cast<int>(y / unit_alt);
    position->z = static_cast<int>(z / unit_size);
}

void CEditArea::GetPos(CVector3_f_ *position, int x, int y, int z) {
    position->x = offset_x + (static_cast<float>(x) * unit_size);
    position->y = offset_y + (static_cast<float>(y) * unit_alt);
    position->z = offset_z + (static_cast<float>(z) * unit_size);
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

    grid[x][y].altitude = std::clamp(altitude, 0, 127);
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
    CVector3_i_ position{};
    GetPos(&position, x, y, z);
    return GetAlt(position.x, position.z);
}

int CEditArea::GetAlt_i(float x, float y, float z) {
    CVector3_i_ position{};
    GetPos(&position, x, y, z);
    return GetAlt_i(position.x, position.z);
}

int CEditArea::GetPartsExtra(int x, int y) {
    if (map_no == TOWN_MATATAKI) {
        switch (area_id) {
            case 0:
                if ((x == 5 && y == -1) || (x == 2 && y == 8)) {
                    return MAP_PARTS_SUBTYPE_RIVER;
                }

                break;
            case 1:
                if ((x == 3 && y == 6) || (x == 12 && y == 3)) {
                    return MAP_PARTS_SUBTYPE_RIVER;
                }

                break;
            case 2:
                if ((x == 4 && y == -1) || (x == 3 && y == 8)) {
                    return MAP_PARTS_SUBTYPE_RIVER;
                }

                break;
            default:
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
    CVector3_i_ position{};
    CMapParts  *target = &parts[parts_id];
    target->unit_size = unit_size;
    const int parts_width = target->GetWidth();
    const int parts_height = target->GetHeight();
    GetPos(&position, x, y, z);

    for (int i = 0; i < parts_width; i++) {
        for (int j = 0; j < parts_height; j++) {
            const int cell_x = i + (position.x - (parts_width >> 1));
            const int cell_y = j + (position.z - (parts_height >> 1));
            const int cell_code = target->GetInfoData(i, j);

            if (cell_code == 0) {
                continue;
            }

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

    grid_redraw = 1;
    return 1;
}

int CEditArea::DeleteMapParts(int parts_no, CMapParts *parts, float x, float y, float z) {
    CVector3_i_ position{};
    GetPos(&position, x, y, z);

    if (parts_no < 0) {
        return 0;
    }

    CMapParts *target = &parts[parts_no];
    const int  parts_width = target->GetWidth();
    const int  parts_height = target->GetHeight();

    for (int i = 0; i < parts_width; i++) {
        for (int j = 0; j < parts_height; j++) {
            const int cell_x = i + (position.x - (parts_width >> 1));
            const int cell_y = j + (position.z - (parts_height >> 1));
            const s16 cell_code = static_cast<s16>(target->GetInfoData(i, j));

            if (cell_code == 0) {
                continue;
            }

            if (cell_code < 0x80) {
                const int occupant = GetPartsID(cell_x, cell_y);
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

    grid_redraw = 1;
    return 1;
}

int CEditArea::SetRiverParts(int x, int y) {
    if (x < 0 || x >= width) {
        return -1;
    }

    if (y < 0 || y >= height) {
        return -1;
    }

    if (GetPartsExtra(x, y) != MAP_PARTS_SUBTYPE_RIVER) {
        return -1;
    }

    const auto connects = [](int extra) {
        return extra == MAP_PARTS_SUBTYPE_RIVER || extra == MAP_PARTS_SUBTYPE_BRIDGE ||
               extra == MAP_PARTS_SUBTYPE_ON_RIVER;
    };

    const int negative_z = GetPartsExtra(x, y - 1);
    const int positive_x = GetPartsExtra(x + 1, y);
    const int positive_z = GetPartsExtra(x, y + 1);
    const int negative_x = GetPartsExtra(x - 1, y);
    return EncodeNeighbors({connects(negative_z), connects(positive_x), connects(positive_z), connects(negative_x)},
                           {3, 2, 1, 0}, {0, 3, 2, 1});
}

int CEditArea::SetRoadParts(int x, int y) {
    if (x < 0 || x >= width) {
        return -1;
    }

    if (y < 0 || y >= height) {
        return -1;
    }

    if (GetPartsExtra(x, y) != MAP_PARTS_SUBTYPE_ROAD) {
        return -1;
    }

    const bool negative_z = GetPartsExtra(x, y - 1) == MAP_PARTS_SUBTYPE_ROAD;
    const bool positive_x = GetPartsExtra(x + 1, y) == MAP_PARTS_SUBTYPE_ROAD;
    const bool positive_z = GetPartsExtra(x, y + 1) == MAP_PARTS_SUBTYPE_ROAD;
    const bool negative_x = GetPartsExtra(x - 1, y) == MAP_PARTS_SUBTYPE_ROAD;
    return EncodeNeighbors({negative_z, positive_x, positive_z, negative_x}, {0, 3, 2, 1}, {1, 0, 3, 2});
}

int CEditArea::SearchPartsID(float x, float y, float z) {
    CVector3_i_ position{};
    GetPos(&position, x, y, z);
    return grid[position.x][position.z].parts_id;
}

int CEditArea::SearchPartsExtra(float x, float y, float z) {
    CVector3_i_ position{};
    GetPos(&position, x, y, z);
    return GetPartsExtra(position.x, position.z);
}

void CEditArea::GetGrid(CVector3_f_ *snapped, float x, float y, float z) {
    CVector3_i_ cell{};
    GetPos(&cell, x, y, z);
    GetPos(snapped, cell.x, cell.y, cell.z);
}

void CEditArea::RemakeGrid() {
    if (grid_frame == nullptr) {
        return;
    }

    CVisualVu1 *visual = grid_frame->GetVisual();

    if (visual == nullptr) {
        return;
    }

    auto *model = reinterpret_cast<MDT_HEADER *>(visual->GetMDTDataAddress());

    if (model == nullptr) {
        return;
    }

    // Each cell owns four consecutive overlay vertices, cells laid out row by row.
    auto                          *first_vertex = reinterpret_cast<sceVu0FVECTOR *>(reinterpret_cast<std::byte *>(model) + model->vertex_ofs);
    const std::span<sceVu0FVECTOR> vertices(first_vertex,
                                            static_cast<std::size_t>(std::max(width * height, 0)) * 4);

    for (int x = 0; x < width; x++) {
        for (int y = 0; y < height; y++) {
            const auto     index = static_cast<std::size_t>(x + y * width) * 4;
            sceVu0FVECTOR &origin = vertices[index];

            if (GetCode(x, y) == 0x81) {
                sceVu0CopyVector(vertices[index + 1], origin);
                sceVu0CopyVector(vertices[index + 2], origin);
                sceVu0CopyVector(vertices[index + 3], origin);
            } else {
                vertices[index + 1][0] = 1.002f * unit_size + origin[0];
                vertices[index + 1][2] = origin[2];
                vertices[index + 2][0] = origin[0];
                vertices[index + 2][2] = 1.002f * unit_size + origin[2];
                vertices[index + 3][0] = 1.002f * unit_size + origin[0];
                vertices[index + 3][2] = 1.002f * unit_size + origin[2];
            }
        }
    }

    grid_frame->attr.remake_pending = 1;
}

void CEditArea::GetPartsBox(CBoxVu0 *box) {
    *box = parts_box;
}

void CEditArea::MakePartsBox() {
    CVector3_f_ position{};
    int         max_x = 0;
    int         max_z = 0;

    for (int x = 0; x < width; x++) {
        for (int z = 0; z < height; z++) {
            if (grid[x][z].parts_no >= 0 && grid[x][z].parts_id >= 0) {
                max_x = std::max(max_x, x);
                max_z = std::max(max_z, z);
            }
        }
    }

    GetPos(&position, max_x, 0, max_z);
    parts_box.max[0] = position.x + unit_size;
    parts_box.max[1] = position.y;
    parts_box.max[2] = position.z + unit_size;
    parts_box.max[3] = 1.0f;
    // The minimum corner never moves off the grid origin cell.
    GetPos(&position, 0, 0, 0);
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
    CVector3_i_ position{};
    GetPos(&position, x, y, z);
    const int left = position.x - (rect_width >> 1);
    const int right = rect_width + left - 1;
    const int top = position.z - (rect_height >> 1);
    const int bottom = rect_height + top - 1;

    if (left < 0 || top < 0) {
        return 0;
    }

    if (right >= width || bottom >= height) {
        return 0;
    }

    return 1;
}

int CEditArea::CheckParts(CMapParts *parts, float x, float y, float z, int rotation) {
    if (parts->info == nullptr) {
        return 0;
    }

    CVector3_i_ position{};
    GetPos(&position, x, y, z);

    if (parts->subtype == MAP_PARTS_SUBTYPE_ON_RIVER) {
        if (GetPartsExtra(position.x, position.z) != MAP_PARTS_SUBTYPE_RIVER) {
            return 0;
        }

        return 1;
    }

    parts->SetRotY(rotation);
    const int part_width = parts->GetWidth();
    const int part_height = parts->GetHeight();
    const int left = position.x - (part_width >> 1);
    const int top = position.z - (part_height >> 1);

    for (int i = 0; i < part_width; i++) {
        for (int j = 0; j < part_height; j++) {
            const int cell_x = i + left;
            const int cell_y = j + top;

            if (cell_x < 0 || cell_x >= width) {
                return 0;
            }

            if (cell_y < 0 || cell_y >= height) {
                return 0;
            }

            const s16 cell_code = static_cast<s16>(parts->GetInfoData(i, j));

            if (cell_code != 0) {
                const int occupant = grid[cell_x][cell_y].parts_no;
                const int extra = GetPartsExtra(cell_x, cell_y);

                // A non-road part may overlap an occupied road cell.
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

        const std::array neighbors = {
            GetPartsExtra(position.x + 1, position.z),
            GetPartsExtra(position.x - 1, position.z),
            GetPartsExtra(position.x, position.z - 1),
            GetPartsExtra(position.x, position.z + 1),
        };

        const bool blocked = std::ranges::any_of(neighbors, [](int extra) {
            return extra == MAP_PARTS_SUBTYPE_BRIDGE || extra == MAP_PARTS_SUBTYPE_LAKE ||
                   extra == MAP_PARTS_SUBTYPE_ON_RIVER;
        });

        if (blocked) {
            return 0;
        }
    }

    if (parts->subtype == MAP_PARTS_SUBTYPE_LAKE) {
        if (GetAlt_i(position.x, position.z) > 0) {
            return 0;
        }

        for (int i = -1; i < part_width + 1; i++) {
            for (int j = -1; j < part_height + 1; j++) {
                const int extra = GetPartsExtra(i + left, j + top);

                if (extra == MAP_PARTS_SUBTYPE_BRIDGE || extra == MAP_PARTS_SUBTYPE_RIVER ||
                    extra == MAP_PARTS_SUBTYPE_ON_RIVER) {
                    return 0;
                }
            }
        }
    }

    return 1;
}

int CEditArea::PickUpPoly(CCPoly *polygons, float x, float y, float z) {
    if (CheckArea(x, y, z) == 0) {
        return 0;
    }

    CVector3_i_ position{};
    GetPos(&position, x, y, z);
    const CRect_i_ rect(position.x - 1, position.z - 1, 2, 2);
    return PickUpPoly(polygons, rect);
}

int CEditArea::PickUpPoly(CCPoly *polygons, CRect_i_ rect) {
    int count = 0;

    for (int x = rect.x; x < rect.x + rect.width; x++) {
        for (int y = rect.y; y < rect.y + rect.height; y++) {
            if (x < 0 || x >= width || y < 0 || y >= height || GetCode(x, y) == 0x81) {
                continue;
            }

            CVector3_f_           position{};
            sceVu0FVECTOR         corner[4];
            sceVu0FVECTOR         normal;
            GridSurfaceAttributes surface{};
            GetPos(&position, x, 0, y);
            surface.foot_sound = kGridFootSounds[map_no];
            const auto info = std::bit_cast<CCPolyInfo>(surface);
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

            CCPoly &first = polygons[count];
            sceVu0CopyVector(first.vertex[0], corner[0]);
            sceVu0CopyVector(first.vertex[1], corner[1]);
            sceVu0CopyVector(first.vertex[2], corner[2]);
            sceVu0CopyVector(first.normal, normal);
            first.info = info;

            CCPoly &second = polygons[count + 1];
            sceVu0CopyVector(second.vertex[0], corner[2]);
            sceVu0CopyVector(second.vertex[1], corner[1]);
            sceVu0CopyVector(second.vertex[2], corner[3]);
            sceVu0CopyVector(second.normal, normal);
            second.info = info;
            count += 2;
        }
    }

    return count;
}

int CEditArea::PickUpPoly(CCPoly *polygons, CBoxVu0 box) {
    CVector3_i_ position{};
    GetPos(&position, box.min[0], box.min[1], box.min[2]);
    const int left = position.x;
    const int top = position.z;
    GetPos(&position, box.max[0], box.max[1], box.max[2]);
    const int      right = position.x;
    const int      bottom = position.z;
    const CRect_i_ rect(left - 1, top - 1, right - left + 2, bottom - top + 2);
    return PickUpPoly(polygons, rect);
}

int CEditArea::GetPartsRect(CRect_i_ &rect, int *parts_ids, int capacity) {
    int count = 0;

    for (int x = rect.x; x < rect.x + rect.width; x++) {
        for (int y = rect.y; y < rect.y + rect.height; y++) {
            if (count >= capacity) {
                return count;
            }

            const int parts_id = GetPartsID(x, y);

            if (parts_id >= 0 && !std::ranges::contains(std::span(parts_ids, static_cast<std::size_t>(count)), parts_id)) {
                parts_ids[count++] = parts_id;
            }
        }
    }

    return count;
}

void CEditArea::ChainWorkClear() {
    for (auto &column : chain_work) {
        std::ranges::fill(column, 0);
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
    const int extra = GetPartsExtra(x, y);

    if (extra != MAP_PARTS_SUBTYPE_RIVER && extra != MAP_PARTS_SUBTYPE_BRIDGE && extra != MAP_PARTS_SUBTYPE_ON_RIVER) {
        return 0;
    }

    if (x == target_x && y == target_y) {
        return 1;
    }

    const bool connected =
        CheckRiverChain(x - 1, y, target_x, target_y) != 0 || CheckRiverChain(x + 1, y, target_x, target_y) != 0 ||
        CheckRiverChain(x, y - 1, target_x, target_y) != 0 || CheckRiverChain(x, y + 1, target_x, target_y) != 0;
    return connected ? 1 : 0;
}

void CEditArea::DrawGrid() {
    sceVu0FVECTOR position;

    grid_redraw_count--;

    if (grid_redraw != 0) {
        grid_redraw_count = 2;
    }

    if (grid_redraw_count > 0) {
        RemakeGrid();

        if (grid_frame != nullptr) {
            grid_frame->attr.cull_enable = false;
        }
    } else if (grid_frame != nullptr) {
        grid_frame->attr.cull_enable = true;
    }

    grid_redraw = 0;

    grid_redraw_count = std::max(grid_redraw_count, 0);

    position[0] = offset_x;
    position[1] = offset_y;
    position[2] = offset_z;
    grid_frame->SetPosition(position);
    MGDraw(grid_frame);
}

void CEditArea::Clear() {
    for (int x = 0; x < width; x++) {
        for (int y = 0; y < height; y++) {
            CEditAreaCell &cell = grid[x][y];
            cell.parts_no = -1;
            cell.altitude = 0;
            cell.spare = 0;
            cell.parts_id = -1;
            cell.code = -1;
            cell.parts_extra = -1;
            cell.spare_index = -1;
        }
    }

    grid_redraw = 1;
    grid_redraw_count = 4;
}

void CEditArea::Initialize() {
    SetOffset(0.0f, 0.0f, 0.0f);
    SetSize(16, 16, 1.0f, 1.0f);
    grid_frame = nullptr;
    Clear();
    map_no = 0;
    area_id = 0;
}

void CEditArea::SetMapInfo(int map_no, int area_id) {
    this->map_no = map_no;
    this->area_id = area_id;
}

int CEditArea::GetMapNo() {
    return map_no;
}

int CEditArea::GetAreaID() {
    return area_id;
}

void CEditArea::SetGridFrame(CFrameVu1 *frame) {
    grid_frame = frame;
}

CFrameVu1 *CEditArea::GetGridFrame() {
    return grid_frame;
}

void CEditArea::SetOffset(float x, float y, float z) {
    offset_x = x;
    offset_y = y;
    offset_z = z;
}

void CEditArea::GetOffset(float *offset) {
    offset[0] = offset_x;
    offset[1] = offset_y;
    offset[2] = offset_z;
}

int CEditArea::GetWidth() {
    return width;
}

int CEditArea::GetHeight() {
    return height;
}

void CEditArea::AddAlt(int x, int y, int altitude_delta) {
    SetAlt(x, y, GetAlt_i(x, y) + altitude_delta);
}

float CEditArea::GetUnitSize() {
    return unit_size;
}

float CEditArea::GetUnitAlt() {
    return unit_alt;
}

CEditArea::CEditArea() {
    Initialize();
}

} // namespace dc::game
