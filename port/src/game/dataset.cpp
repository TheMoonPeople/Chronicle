#include "dataset.hpp"

#include <libvu0.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <print>
#include <ranges>
#include <string_view>

#include "boxvu0.hpp"
#include "collisionmdt.hpp"
#include "data/data_read.hpp"
#include "dataalloc.hpp"
#include "dataread.hpp"
#include "frame.hpp"
#include "framevu1.hpp"
#include "mathutil.hpp"
#include "mds.hpp"
#include "mdt.hpp"
#include "mglib.hpp"
#include "platform/arena.hpp"
#include "visual.hpp"

namespace dc::game {

using namespace dc::data;

void CCollisionMDT::Initialize() {
    model = nullptr;
    mesh = nullptr;
    mesh_count = 0;
}

u_int          *read_buffer;
CDataAlloc2<1> *WorkBuffer;
CDataAlloc2<1> *ActiveData;

CDataAlloc2<1>         VisualData(-1);
CDataAlloc2<1>         MotionData(-1);
CDataAlloc2<1>         TextureData(-1);
CDataAlloc2<1>         WaterData(-1);
CDataAlloc2<1>         VariousData(-1);
CDataAlloc2<1>         ActiveData0(-1);
CDataAlloc2<1>         ActiveData1(-1);
CDataAlloc<1, 1690000> GlobalDataBuffer;
CDataAlloc2<1>         workbuffer(-1);

int dset_mds_packet;
int dset_mds_objnum;

namespace {

// InitWorkBuffer (editloop.cpp, edit_in.cpp) starts EdMenuBuffer this far
// before read_buffer, in what retail's carving leaves of the preceding arena.
constexpr std::size_t kReadBufferLead = 0x180000;

constexpr int kMaxQuads = std::numeric_limits<int>::max() / kArenaHeadroom;

void Carve(CDataAlloc2<1> *arena, int quads) {
    if (arena == nullptr || quads <= 0 || quads > kMaxQuads) {
        ArenaOverflow(arena, quads, kMaxQuads);
    }
    DataReadForgetPackCapacities(arena);
    arena->base = ArenaBlock(arena, static_cast<std::size_t>(quads) * 16 * kArenaHeadroom);
    arena->limit = quads * kArenaHeadroom;
    arena->used = 0;
}

void CarveWorkBuffer(int quads) {
    Carve(&workbuffer, quads);
    WorkBuffer = &workbuffer;
}

u_long128 *CarvePackets(int quads, int which) {
    static constexpr std::array<char, 2> kPacketOwners{};
    if (which < 0 || which >= 2 || quads <= 0 || quads > kMaxQuads) {
        ArenaOverflow("VIF packet buffer", quads, kMaxQuads);
    }
    return reinterpret_cast<u_long128 *>(ArenaBlock(&kPacketOwners[which], static_cast<std::size_t>(quads) * 16 * kArenaHeadroom));
}

void CarveReadBuffer(int quads) {
    if (quads <= 0 || quads > kMaxQuads) {
        ArenaOverflow("file read buffer", quads, kMaxQuads);
    }
    DataReadForgetPackCapacities(read_buffer);
    read_buffer = reinterpret_cast<u_int *>(ArenaBlock(static_cast<const void *>(&read_buffer), static_cast<std::size_t>(quads) * 16 * kArenaHeadroom, kReadBufferLead));
}

} // namespace

void InitializeDataBuffer() {
    ArenaClearAll();
    DataReadClearPackCapacities();
    GlobalDataBuffer.used = 0;
    Carve(&WaterData, 10);
    Carve(&ActiveData0, 25000);
    Carve(&ActiveData1, 25000);
}

void SetDataBuffer(CDataAlloc2<1> *arena, int quads) {
    Carve(arena, quads);
}

void SetPacketReadBuffer(int packet_quads, int read_quads) {
    CarveReadBuffer(read_quads);
    MGInitVif1Packet(CarvePackets(packet_quads, 0), CarvePackets(packet_quads, 1));
    CarveWorkBuffer(2048);
}

void BufferAllClear() {
    ArenaClearAll();
    DataReadClearPackCapacities();
    GlobalDataBuffer.used = 0;
    Carve(&VisualData, 600000);
    Carve(&MotionData, 200000);
    Carve(&TextureData, 300000);
    Carve(&WaterData, 90000);
    Carve(&ActiveData0, 25000);
    Carve(&ActiveData1, 25000);
    CarveReadBuffer(100000);
    CarveWorkBuffer(4096);
    MGInitVif1Packet(CarvePackets(50000, 0), CarvePackets(50000, 1));
}

namespace {

// Bits of the attribute word LoadMDSFile and CreateVisual take.
constexpr int kAttrKeepModel = 2;
constexpr int kAttrDoubleBuffer = 4;
constexpr int kAttrShadow = 8;
constexpr int kAttrCopyOnDraw = 0x10;

constexpr int kMaxShadowEdges = 1024;

/**
 * One edge of a shadow model while ArrangeShadowMDT pairs edges up. A paired
 * edge is skipped rather than removed.
 */
struct SHADOW_EDGE {
    short         v0;        /**< Index of the vertex the edge starts at. */
    short         v1;        /**< Index of the vertex the edge ends at. */
    short         done;      /**< Whether the edge has already been paired. */
    int          *edge_flag; /**< Model's flag for the edge, set when the edge is interior. */
    sceVu0FVECTOR normal;    /**< Normal of the triangle the edge belongs to. */
};

template <typename T>
T *AtOffset(void *base, std::size_t offset) {
    return reinterpret_cast<T *>(static_cast<char *>(base) + offset);
}

// Where the allocator will hand out its next block.
u_int *NextBlock(CDataAlloc2<1> *alloc) {
    return reinterpret_cast<u_int *>(&alloc->base[static_cast<std::ptrdiff_t>(alloc->used) * 16]);
}

int htoi(std::string_view text) {
    int value = 0;
    int place = 1;

    for (const char ch : text | std::views::reverse) {
        int digit = 0;

        if (ch >= '0' && ch <= '9') {
            digit = ch - '0';
        }

        if (ch >= 'a' && ch <= 'f') {
            digit = ch - 'a' + 10;
        }

        if (ch >= 'A' && ch <= 'F') {
            digit = ch - 'A' + 10;
        }

        value += digit * place;
        place <<= 4;
    }

    return value;
}

void CopyName(char *dest, const char *src) {
    const std::string_view name(src);
    std::ranges::copy(name, dest);
    dest[name.size()] = '\0';
}

// An absent vertex array yields an empty box.
void CreateBBox(CBox3<float> *box, sceVu0FVECTOR *vertex, int num) {
    if (vertex == nullptr) {
        box->max[0] = box->min[0] = 0.0f;
        box->max[1] = box->min[1] = 0.0f;
        box->max[2] = box->min[2] = 0.0f;
        return;
    }

    box->max[0] = vertex[0][0];
    box->max[1] = vertex[0][1];
    box->max[2] = vertex[0][2];
    box->min[0] = vertex[0][0];
    box->min[1] = vertex[0][1];
    box->min[2] = vertex[0][2];

    for (int i = 0; i < num; i++) {
        box->max[0] = std::max(box->max[0], vertex[i][0]);
        box->max[1] = std::max(box->max[1], vertex[i][1]);
        box->max[2] = std::max(box->max[2], vertex[i][2]);
        box->min[0] = std::min(box->min[0], vertex[i][0]);
        box->min[1] = std::min(box->min[1], vertex[i][1]);
        box->min[2] = std::min(box->min[2], vertex[i][2]);
    }
}

// Fills a frame's bound and its eight corners from the model's vertices.
void SetFrameBounds(CFrameVu1 *node, MDT_HEADER *model) {
    auto     *vertex = reinterpret_cast<sceVu0FVECTOR *>(reinterpret_cast<char *>(model) + model->vertex_ofs);
    const int vertex_num = model->vertex_num;

    CreateBBox(reinterpret_cast<CBox3<float> *>(node->max), vertex, vertex_num);

    const std::array<float *, 2> extremes = {node->min, node->max};

    for (int corner = 0; corner < 8; corner++) {
        node->corner[corner][3] = 1.0f;
        node->corner[corner][0] = extremes[(corner & 1) != 0][0];
        node->corner[corner][1] = extremes[(corner & 2) != 0][1];
        node->corner[corner][2] = extremes[(corner & 4) != 0][2];
    }
}

// Two edges with the same corners in opposite order are shared by two
// triangles; if those face nearly the same way the edge is interior and its
// flag is set in the model, leaving the silhouette edges clear.
void ArrangeShadowMDT(u_int *data) {
    auto          edges = std::make_unique<SHADOW_EDGE[]>(kMaxShadowEdges);
    sceVu0FVECTOR side0;
    sceVu0FVECTOR side1;
    sceVu0FVECTOR face;

    auto *model = reinterpret_cast<MDT_HEADER *>(data);
    auto *mesh = AtOffset<MDT_SHADOW>(model, model->mesh_ofs);
    auto *vertex = reinterpret_cast<sceVu0FVECTOR *>(reinterpret_cast<char *>(model) + model->vertex_ofs);

    const int   shape_num = mesh->shape_num;
    MDT_SSHAPE *shape = mesh->shape;
    int         edge_count = 0;

    for (int shape_index = 0; shape_index < shape_num; shape_index++) {
        const int    triangle_count = shape->index_num / 3;
        MDT_SVERTEX *corner = shape->vertex;

        for (int triangle = 0; triangle < triangle_count; triangle++) {
            sceVu0SubVector(side0, vertex[corner[1].index], vertex[corner[0].index]);
            sceVu0SubVector(side1, vertex[corner[2].index], vertex[corner[0].index]);
            sceVu0OuterProduct(face, side0, side1);
            sceVu0Normalize(edges[edge_count].normal, face);
            sceVu0CopyVector(edges[edge_count + 1].normal, edges[edge_count].normal);
            sceVu0CopyVector(edges[edge_count + 2].normal, edges[edge_count].normal);

            for (int side = 0; side < 3; side++) {
                SHADOW_EDGE &edge = edges[edge_count];
                edge.v0 = static_cast<short>(corner[side].index);
                edge.v1 = static_cast<short>(corner[(side + 1) % 3].index);
                edge.done = 0;
                edge.edge_flag = &corner[side].edge;
                corner[side].edge = 0;
                edge_count++;
            }

            if (edge_count > 1020) {
                std::println("shadow initialize failed");

                for (;;) {
                }
            }

            corner += 3;
        }

        // The next shape starts directly after this one's last corner.
        shape = reinterpret_cast<MDT_SSHAPE *>(corner);
    }

    for (int k = 0; k < edge_count - 1; k++) {
        if (edges[k].done != 0) {
            continue;
        }

        for (int m = k + 1; m < edge_count; m++) {
            if (edges[m].done != 0) {
                continue;
            }

            if (edges[k].v0 != edges[m].v1) {
                continue;
            }

            if (edges[k].v1 != edges[m].v0) {
                continue;
            }

            if (DistVector(edges[k].normal, edges[m].normal) < 0.0008f) {
                edges[k].done = 1;
                *edges[k].edge_flag = 1;
                edges[m].done = 1;
                *edges[m].edge_flag = 1;
                break;
            }
        }
    }
}

} // namespace

void SetFrameAttr(CFrame *frame, int recurse) {
    char *name = frame->name;
    bool  has_codes = true;

    frame->attr.clip_enable = true;
    frame->attr.clip_depth = 150.0f;

    if (name[0] == '\0') {
        has_codes = false;
    }

    // Attribute codes follow the first "__" in the name.
    int pos = 1;

    for (;;) {
        if (name[pos] == '\0') {
            has_codes = false;
            break;
        }

        if (name[pos] == '_' && name[pos - 1] == '_') {
            break;
        }

        pos++;
    }

    while (has_codes) {
        if (name[pos] == '\0') {
            break;
        }

        switch (name[pos]) {
            case 'c':
            case 'C':
                frame->attr.use_color = true;
                frame->attr.color[0] = 128.0f;
                frame->attr.color[1] = 128.0f;
                frame->attr.color[2] = 128.0f;
                frame->attr.color[3] = 128.0f;
                break;
            case 'n':
            case 'N':
                frame->attr.clip_enable = false;
                break;
            case 'a':
            case 'A': {
                std::array<char, 3> blend_code{};
                blend_code[0] = name[pos + 1];
                pos += 2;
                blend_code[1] = name[pos];
                blend_code[2] = '\0';

                if (blend_code[0] >= 'a' && blend_code[0] <= 'z') {
                    blend_code[0] -= 32;
                }

                if (blend_code[1] >= 'a' && blend_code[1] <= 'z') {
                    blend_code[1] -= 32;
                }

                if (blend_code[0] == 'P' && blend_code[1] == 'P') {
                    frame->attr.blend_mode = 1;
                } else if (blend_code[0] == 'N' && blend_code[1] == 'N') {
                    frame->attr.blend_mode = -1;
                } else {
                    frame->attr.alpha_ref = htoi(blend_code.data());
                }

                break;
            }
            case 'z':
            case 'Z':
                frame->attr.depth_write = false;
                break;
            case 'f':
            case 'F':
                frame->attr.fog_enable = false;
                break;
            case 's':
            case 'S':
                frame->attr.program_option = 1;
                break;
            case 'm':
            case 'M':
                frame->attr.eye_relative = true;
                break;
            case 'b':
            case 'B':
                pos++;

                if (name[pos] == '\0') {
                    pos--;
                    break;
                }

                if (name[pos] == 'Y' || name[pos] == 'y') {
                    frame->attr.billboard = 2;
                }

                if (name[pos] == 'A' || name[pos] == 'a') {
                    frame->attr.billboard = 3;
                }

                break;
            case 't':
            case 'T':
                frame->attr.ambient_boost = true;
                break;
            case 'o':
            case 'O':
                frame->attr.ignore_depth = true;
                break;
            case 'v':
            case 'V':
                frame->attr.draw_on = 2;
                frame->flags = 2;
                break;
            default:
                break;
        }

        pos++;
    }

    if (recurse != 0) {
        for (CFrame *child = frame->child; child != nullptr; child = child->brother) {
            SetFrameAttr(child, recurse);
        }
    }
}

// Every object gets a frame in file order, so a parent index indexes the
// returned array; objects named "func_..." are left uninitialised.
CFrameVu1 *LoadMDSFile(u_int *data, CDataAlloc2<1> *alloc, int attr, char **double_names, char **retain_names) {
    if (data == nullptr) {
        return nullptr;
    }

    // Everything inside the file is addressed in quadwords from its start.
    if (reinterpret_cast<std::uintptr_t>(data) % 16 != 0) {
        std::println("address error!! {} ", static_cast<int>(reinterpret_cast<std::uintptr_t>(data)));
    }

    auto *header = reinterpret_cast<MDS_HEADER *>(data);

    dset_mds_packet = 0;
    dset_mds_objnum = header->object_num;

    u_char *block = alloc->Alloc(static_cast<int>(header->object_num * sizeof(CFrameVu1) / 16));
    auto   *frames = new (block) CFrameVu1[header->object_num];

    for (u_int i = 0; i < header->object_num; i++) {
        auto *object = AtOffset<MDS_OBJECT>(header, sizeof(MDS_HEADER) + (i * sizeof(MDS_OBJECT)));

        if (std::string_view(object->name).starts_with("func_")) {
            continue;
        }

        CFrameVu1 *node = &frames[i];
        node->Initialize();
        node->SetScale(1.0f, 1.0f, 1.0f);
        node->SetRotation(0.0f, 0.0f, 0.0f);
        node->SetPosition(0.0f, 0.0f, 0.0f);

        sceVu0FMATRIX matrix;

        for (int col = 0; col < 4; col++) {
            for (int row = 0; row < 4; row++) {
                matrix[row][col] = object->matrix[row][col];
            }
        }

        CopyName(node->name, object->name);
        node->SetTransMatrix(matrix);
        SetFrameAttr(node, 0);

        if (object->parent < 0) {
            node->SetParent(nullptr);
        } else {
            node->SetParent(&frames[object->parent]);
        }

        if (object->data_ofs != 0) {
            auto *model = AtOffset<MDT_HEADER>(header, object->data_ofs);
            SetFrameBounds(node, model);

            int object_attr = attr;

            if (double_names != nullptr) {
                for (char **entry = double_names; *entry != nullptr; entry++) {
                    if (FrameNameComp(node->name, *entry) != 0) {
                        object_attr |= kAttrKeepModel | kAttrDoubleBuffer;
                        break;
                    }
                }
            }

            if (retain_names != nullptr) {
                for (char **entry = retain_names; *entry != nullptr; entry++) {
                    if (FrameNameComp(node->name, *entry) != 0) {
                        object_attr |= kAttrKeepModel;
                        break;
                    }
                }
            }

            node->SetVisual(CreateVisual(reinterpret_cast<u_int *>(model), alloc, object_attr));
        }
    }

    return frames;
}

// The attribute picks the visual's kind, whether the model is copied beside
// the built block, and whether a second block is built to alternate with.
CVisualMDTVu1 *CreateVisual(u_int *data, CDataAlloc2<1> *alloc, int attr) {
    if (data == nullptr) {
        return nullptr;
    }

    CVisualMDTVu1 *visual = nullptr;

    if ((attr & kAttrKeepModel) != 0) {
        if ((attr & kAttrShadow) != 0) {
            visual = new (reinterpret_cast<u_long128 *>(alloc->Alloc(3))) CVisualShadow;
        } else {
            visual = new (reinterpret_cast<u_long128 *>(alloc->Alloc(3))) CVisualMDTVu1;
        }
    } else {
        if ((attr & kAttrShadow) != 0) {
            visual = new (reinterpret_cast<u_long128 *>(alloc->Alloc(3))) CVisualShadow;
        } else {
            visual = static_cast<CVisualMDTVu1 *>(new (reinterpret_cast<u_long128 *>(alloc->Alloc(2))) CVisualVu1);
        }
    }

    visual->Initialize();
    alloc->Align64();

    auto *model = reinterpret_cast<MDT_HEADER *>(data);

    // The block is built at the allocator's next address and reserved after,
    // since only the visual knows how large it turned out.
    if ((attr & kAttrShadow) != 0) {
        static_cast<CVisualShadow *>(visual)->CreateVUdataShadow(NextBlock(alloc), data);
        visual->vu_data_buffer[0] = visual->vu_data;
        visual->vu_data_buffer[1] = visual->vu_data;
        ArrangeShadowMDT(data);
    } else {
        visual->CreateVUdataFromMDT(NextBlock(alloc), data, 0, 0);
    }

    alloc->Alloc(visual->vu_size);
    dset_mds_packet += visual->vu_size;
    alloc->Align64();

    if ((attr & kAttrKeepModel) != 0) {
        alloc->Align64();
        u_char *copy = alloc->Alloc(static_cast<int>((model->size >> 4) + 1));
        std::memcpy(copy, data, model->size);
        visual->SetMDTDataAddress(reinterpret_cast<u_int *>(copy));

        if ((attr & kAttrCopyOnDraw) != 0) {
            visual->copy_on_draw = true;
        }

        if ((attr & kAttrDoubleBuffer) != 0 && (attr & kAttrCopyOnDraw) == 0) {
            alloc->Align64();
            visual->vu_data_buffer[0] = visual->vu_data;

            if ((attr & kAttrShadow) != 0) {
                static_cast<CVisualShadow *>(visual)->CreateVUdataShadow(NextBlock(alloc), data);
            } else {
                visual->CreateVUdataFromMDT(NextBlock(alloc), data, 0, 0);
            }

            alloc->Alloc(visual->vu_size);
            visual->vu_data_buffer[1] = visual->vu_data;
        } else {
            visual->vu_data_buffer[0] = visual->vu_data;
            visual->vu_data_buffer[1] = visual->vu_data;
        }
    }

    alloc->Align64();
    return visual;
}

void LoadMDSFileLOD(CFrameVu1 **frame, u_int **data, CDataAlloc2<1> *alloc, int attr) {
    for (int level = 0; level < 4; level++) {
        if (data[level] == nullptr) {
            frame[level] = nullptr;
        } else {
            frame[level] = LoadMDSFile(data[level], alloc, attr, nullptr, nullptr);
        }
    }
}

// The scene loader without drawing: each object carries a collision shape and
// the file's transform as-is.
CFrameVu1 *LoadCollisionFile(u_int *data, CDataAlloc2<1> *alloc) {
    auto *header = reinterpret_cast<MDS_HEADER *>(data);

    if (header->object_num == 0) {
        return nullptr;
    }

    u_char *block = alloc->Alloc(static_cast<int>(header->object_num * sizeof(CFrameVu1) / 16));
    auto   *frames = new (block) CFrameVu1[header->object_num];

    for (u_int i = 0; i < header->object_num; i++) {
        auto *object = AtOffset<MDS_OBJECT>(header, sizeof(MDS_HEADER) + (i * sizeof(MDS_OBJECT)));

        CFrameVu1 *node = &frames[i];
        node->Initialize();

        sceVu0FMATRIX matrix;

        for (int col = 0; col < 4; col++) {
            for (int row = 0; row < 4; row++) {
                matrix[row][col] = object->matrix[row][col];
            }
        }

        CopyName(node->name, object->name);
        node->SetTransMatrix(matrix);

        if (object->parent < 0) {
            node->SetParent(nullptr);
        } else {
            node->SetParent(&frames[object->parent]);
        }

        if (object->data_ofs != 0) {
            auto *model = AtOffset<MDT_HEADER>(header, object->data_ofs);

            node->SetCollision(CreateCollisionMDT(reinterpret_cast<u_int *>(model), alloc));
            SetFrameBounds(node, model);
        }
    }

    return frames;
}

// The model is copied into the allocator first, since the collision points
// into that copy; each triangle is stored with its bound so box queries can
// reject it without reading vertices.
CCollisionMDT *CreateCollisionMDT(u_int *data, CDataAlloc2<1> *alloc) {
    auto *source = reinterpret_cast<MDT_HEADER *>(data);
    auto *model = reinterpret_cast<MDT_HEADER *>(alloc->Alloc64(static_cast<int>((source->size >> 4) + 1)));
    std::memcpy(model, source, source->size);

    auto *collision = new (reinterpret_cast<u_long128 *>(alloc->Alloc(4))) CCollisionMDT;
    collision->model = model;
    collision->CreateBBox();

    auto      *vertex = AtOffset<u_long128>(model, model->vertex_ofs);
    auto      *info = AtOffset<u_long128>(model, model->info_ofs);
    auto      *poly_set = &AtOffset<MDT_COLLISION>(model, model->mesh_ofs)->set;
    MDT_CPOLY *poly = poly_set->poly;

    auto       *built = reinterpret_cast<CCPolyBox *>(alloc->Alloc(static_cast<int>(poly_set->num * sizeof(CCPolyBox) / 16)));
    const u_int poly_count = poly_set->num;

    for (u_int i = 0; i < poly_count; i++) {
        CCPoly &out = built[i].poly;

        std::memcpy(out.vertex[0], &vertex[poly[i].vertex[0]], sizeof(u_long128));
        std::memcpy(out.vertex[1], &vertex[poly[i].vertex[1]], sizeof(u_long128));
        std::memcpy(out.vertex[2], &vertex[poly[i].vertex[2]], sizeof(u_long128));

        const int index = poly[i].info_index;

        if (model->info_ofs == 0 || index < 0) {
            std::memset(&out.info, 0, 16);
        } else {
            std::memcpy(&out.info, &info[index], 16);
        }

        VectorMaxMin(built[i].box.max, built[i].box.min, out.vertex[0], out.vertex[1], out.vertex[2]);
        PlaneNormal(out.normal, out.vertex[0], out.vertex[1], out.vertex[2]);
    }

    collision->mesh = built;
    collision->mesh_count = poly_count;
    return collision;
}

CFrameVu1 *LoadMDSFile(u_int *data, int attr, int /*unused*/) {
    return LoadMDSFile(data, &VisualData, attr, nullptr, nullptr);
}

// Copies are assigned from their originals and then re-parented, so the
// subtree's links point into the copy.
CFrameVu1 *CopyFrameVu1(CFrameVu1 *frame, CDataAlloc2<1> *alloc) {
    if (frame == nullptr) {
        return nullptr;
    }

    auto *copy = new (reinterpret_cast<u_long128 *>(alloc->Alloc(sizeof(CFrameVu1) / 16))) CFrameVu1;
    copy->Initialize();
    *copy = *frame;

    for (CFrame *child = frame->child; child != nullptr; child = child->brother) {
        CopyFrameVu1(static_cast<CFrameVu1 *>(child), alloc)->SetParent(copy);
    }

    return copy;
}

CFrame *CopyFrame(CFrame *frame, CDataAlloc2<1> *alloc) {
    if (frame == nullptr) {
        return nullptr;
    }

    auto *copy = new (reinterpret_cast<u_long128 *>(alloc->Alloc(sizeof(CFrame) / 16))) CFrame;
    copy->Initialize();
    *copy = *frame;

    for (CFrame *child = frame->child; child != nullptr; child = child->brother) {
        CopyFrame(child, alloc)->SetParent(copy);
    }

    return copy;
}

void LoadLODData(CFrameVu1 ** /*frame*/, char ** /*name*/, u_int * /*data*/, int /*attr*/) {
}

CFrameVu1 *LoadCollisionFile(u_int *data) {
    return LoadCollisionFile(data, &VisualData);
}

int CCollision::GetPolygon(int /*index*/, sceVu0FMATRIX /*v0*/, sceVu0FMATRIX /*v1*/, sceVu0FMATRIX /*v2*/) {
    return 0;
}

int CCollision::GetMaxY(float * /*position*/) {
    return 0;
}

int CCollision::Intersection(float * /*from*/, float * /*to*/, float * /*hit*/) {
    return 0;
}

int CCollision::PickUpNearPoly(CCPoly * /*poly*/) {
    return 0;
}

int CCollision::PickUpNearPoly(CCPoly * /*poly*/, const CBoxVu0 & /*box*/) {
    return 0;
}

int CCollision::PickUpNearPoly(CCPoly * /*poly*/, float * /*position*/, float /*radius*/) {
    return 0;
}

void CCollision::Initialize() {
}

} // namespace dc::game
