#include "visualshadow.hpp"

#include <libvu0.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <span>
#include <vector>

#include "dataalloc.hpp"
#include "dataset.hpp"
#include "draw3d.hpp"
#include "mdt.hpp"
#include "mglib.hpp"
#include "renderinfo.hpp"
#include "shadowclip.hpp"

// Retail's three shadow microprograms all build volumes: each caster triangle becomes a prism from
// the triangle down along light 0 to its drop onto the shadow plane, and the prism's faces are
// counted into the black shadow target in colour, 1 per channel, Cs + Cd for faces turned towards
// the eye and Cd - Cs for faces turned away, depth-tested against the scene and writing no depth
// (z-pass). A scene pixel inside a prism ends non-black and MGEndDrawShadow darkens it. The
// shadow target shares the main depth buffer, so the count here is retail's, face for face:
//
// - Vu_shadow (MGDrawShadowFast): every triangle of the shadow mesh, three sides and both caps.
// - Vu_shadow3 (MGDrawShadowFast2): the same for the triangles facing away from light 0 only.
// - Vu_shadow2 (MGDrawShadow): CreateVUdataShadowCLIP's records, the triangles facing away from
//   light 0 with sides only across edges that are not flat seams; the far cap is only ever
//   subtracted, and the prism's section by the near plane is added when the near plane cuts it, so
//   the count holds with the eye inside a volume.
//
// The prisms are built here in eye space with every face wound outwards. The adding draw culls the
// faces turned away and the subtracting draw those turned towards the eye, so the rasteriser routes
// each face by its screen winding as the microprograms do, under whatever camera the draw is
// replayed with: a display frame's interpolated view turns faces near the silhouette over, and a
// face routed by the tick's eye would then count on the wrong side.

namespace {

struct Vec3 {
    float x;
    float y;
    float z;
};

Vec3 operator-(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }

Vec3 operator+(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }

Vec3 operator*(Vec3 a, float s) { return {a.x * s, a.y * s, a.z * s}; }

float Dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

Vec3 Cross(Vec3 a, Vec3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }

float Length(Vec3 a) { return std::sqrt(Dot(a, a)); }

// A column-major [column][row] matrix applied to a point.
Vec3 Apply(const float m[4][4], Vec3 p) {
    return {m[0][0] * p.x + m[1][0] * p.y + m[2][0] * p.z + m[3][0],
            m[0][1] * p.x + m[1][1] * p.y + m[2][1] * p.z + m[3][1],
            m[0][2] * p.x + m[1][2] * p.y + m[2][2] * p.z + m[3][2]};
}

enum class Count {
    // Added when it faces the eye, subtracted when it faces away.
    Both,
    // Subtracted when it faces away, left out when it faces the eye.
    AwayOnly,
};

// Triangles wound outwards (counter-clockwise on the target when turned towards the eye).
struct Volume {
    // Count::Both faces, drawn by both passes.
    std::vector<gfx::Vertex3D> both;
    // Count::AwayOnly faces, drawn by the subtracting pass.
    std::vector<gfx::Vertex3D> away;
    // Near-plane sections, turned towards the eye and drawn by the adding pass.
    std::vector<gfx::Vertex3D> cap;
};

void Emit(std::vector<gfx::Vertex3D> &out, std::span<const Vec3> polygon) {
    for (size_t i = 1; i + 1 < polygon.size(); i++) {
        for (Vec3 corner : {polygon[0], polygon[i], polygon[i + 1]}) {
            gfx::Vertex3D vertex = {};
            vertex.position[0] = corner.x;
            vertex.position[1] = corner.y;
            vertex.position[2] = corner.z;
            vertex.color[0] = vertex.color[1] = vertex.color[2] = vertex.color[3] = 0x80;
            out.push_back(vertex);
        }
    }
}

// A planar face of a convex prism; inside is any point strictly inside the prism.
void AddFace(Volume &volume, std::vector<Vec3> polygon, Vec3 inside, Count count) {
    Vec3 normal = {0.0f, 0.0f, 0.0f};
    Vec3 centre = {0.0f, 0.0f, 0.0f};
    for (size_t i = 0; i < polygon.size(); i++) {
        normal = normal + Cross(polygon[i], polygon[(i + 1) % polygon.size()]);
        centre = centre + polygon[i];
    }
    centre = centre * (1.0f / static_cast<float>(polygon.size()));
    if (Length(normal) == 0.0f) {
        return;
    }
    if (Dot(normal, centre - inside) < 0.0f) {
        std::reverse(polygon.begin(), polygon.end());
    }
    Emit(count == Count::Both ? volume.both : volume.away, polygon);
}

// The triangle's prism down to its drop on the plane. A prism the light grazes has no inside and
// counts nothing, which is also what the microprograms make of it.
void AddPrism(Volume &volume, const Vec3 top[3], const Vec3 drop[3], const bool sides[3], Count far_cap) {
    Vec3  normal = Cross(top[1] - top[0], top[2] - top[0]);
    Vec3  centre_top = (top[0] + top[1] + top[2]) * (1.0f / 3.0f);
    Vec3  centre_drop = (drop[0] + drop[1] + drop[2]) * (1.0f / 3.0f);
    Vec3  height = centre_drop - centre_top;
    float area = Length(normal);
    float span = Length(height);
    if (area == 0.0f || span == 0.0f || std::fabs(Dot(normal, height)) <= 1e-4f * area * span) {
        return;
    }
    Vec3 inside = (centre_top + centre_drop) * 0.5f;
    AddFace(volume, {top[0], top[1], top[2]}, inside, Count::Both);
    AddFace(volume, {drop[0], drop[1], drop[2]}, inside, far_cap);
    for (int edge = 0; edge < 3; edge++) {
        int next = (edge + 1) % 3;
        if (sides[edge]) {
            AddFace(volume, {top[edge], top[next], drop[next], drop[edge]}, inside, Count::Both);
        }
    }
}

// The prism's section by the plane z = depth, added wherever it covers: the eye is inside the
// volume there, which the faces in front of the scene cannot count.
void AddNearCap(Volume &volume, const Vec3 top[3], const Vec3 drop[3], float depth) {
    const Vec3          *corners[6] = {&top[0], &top[1], &top[2], &drop[0], &drop[1], &drop[2]};
    static constexpr int kEdges[9][2] = {
        {0, 1},
        {1, 2},
        {2, 0},
        {3, 4},
        {4, 5},
        {5, 3},
        {0, 3},
        {1, 4},
        {2, 5}
    };
    std::vector<Vec3> section;
    for (const auto &edge : kEdges) {
        Vec3  a = *corners[edge[0]];
        Vec3  b = *corners[edge[1]];
        float da = a.z - depth;
        float db = b.z - depth;
        if ((da < 0.0f) != (db < 0.0f)) {
            Vec3 point = a + (b - a) * (da / (da - db));
            point.z = depth;
            section.push_back(point);
        }
    }
    if (section.size() < 3) {
        return;
    }
    Vec3 mean = {0.0f, 0.0f, 0.0f};
    for (Vec3 point : section) {
        mean = mean + point;
    }
    mean = mean * (1.0f / static_cast<float>(section.size()));
    std::sort(section.begin(), section.end(), [mean](Vec3 a, Vec3 b) {
        return std::atan2(a.y - mean.y, a.x - mean.x) > std::atan2(b.y - mean.y, b.x - mean.x);
    });
    Emit(volume.cap, section);
}

struct Projection {
    float model_to_eye[4][4];
    float drop_to_eye[4][4];
    float eye_to_clip[4][4];
    Vec3  local_light;
    // The volumes are built in this tick's eye space; for the display list they ride on the model
    // and the camera as if fixed to the model: eye_to_clip * view * model * (view * model)^-1.
    gfx::MeshTransform transform;
    bool               has_transform;
};

Projection Project(const RenderInfo &info, float (*matrix)[4]) {
    Projection projection;
    float      drop[4][4];
    Draw3DMul(projection.model_to_eye, info.view_scaled, matrix);
    Draw3DMul(drop, info.shadow, matrix);
    Draw3DMul(projection.drop_to_eye, info.view_scaled, drop);
    Draw3DEyeToClip(info, projection.eye_to_clip);
    projection.transform = gfx::IdentityMeshTransform();
    gfx::MeshTransform &transform = projection.transform;
    std::memcpy(transform.projection, projection.eye_to_clip, sizeof(transform.projection));
    std::memcpy(transform.view, info.view_scaled, sizeof(transform.view));
    std::memcpy(transform.model, matrix, sizeof(transform.model));
    projection.has_transform = gfx::InvertAffineTransform(&projection.model_to_eye[0][0], transform.local);
    Vec3 light = {info.light_direction[0][0], info.light_direction[1][0], info.light_direction[2][0]};
    projection.local_light = {Dot({matrix[0][0], matrix[0][1], matrix[0][2]}, light),
                              Dot({matrix[1][0], matrix[1][1], matrix[1][2]}, light),
                              Dot({matrix[2][0], matrix[2][1], matrix[2][2]}, light)};
    return projection;
}

void Corners(const Projection &projection, const Vec3 local[3], Vec3 top[3], Vec3 drop[3]) {
    for (int i = 0; i < 3; i++) {
        top[i] = Apply(projection.model_to_eye, local[i]);
        drop[i] = Apply(projection.drop_to_eye, local[i]);
    }
}

bool FacesAwayFromLight(const Projection &projection, const Vec3 local[3]) {
    return Dot(Cross(local[1] - local[0], local[2] - local[0]), projection.local_light) <= 0.0f;
}

// Eye-space depth where the projection puts depth 1, the near plane.
float NearPlane(const Projection &projection) {
    float a = projection.eye_to_clip[2][2];
    float b = projection.eye_to_clip[3][2];
    return a < 1.0f ? b / (1.0f - a) : 0.0f;
}

void DrawCount(const std::vector<gfx::Vertex3D> &shared, const std::vector<gfx::Vertex3D> &own,
               const Projection &projection, const RenderInfo &info, bool add) {
    std::vector<gfx::Vertex3D> faces = shared;
    faces.insert(faces.end(), own.begin(), own.end());
    if (faces.empty()) {
        return;
    }
    gfx::MeshConstants constants = {};
    std::memcpy(constants.mvp, projection.eye_to_clip, sizeof(constants.mvp));
    constants.flags = gfx::kMeshShadow;
    // Flat colour 1 in every channel, retail's FTOI0 of 1.0: a modulation of 1 / 0x80.
    constants.diffuse[0] = constants.diffuse[1] = constants.diffuse[2] = 1.0f / 128.0f;
    constants.diffuse[3] = 0.5f;

    gfx::DrawState state = Draw3DState(info, false);
    state.cull = add ? gfx::CullMode::Back : gfx::CullMode::Front;
    state.alpha = add ? gfx::GsBlend{0, 2, 2, 1, 0x80} : gfx::GsBlend{2, 0, 2, 1, 0x80};
    std::vector<uint32_t> indices(faces.size());
    for (uint32_t i = 0; i < indices.size(); i++) {
        indices[i] = i;
    }
    gfx::DrawMeshImmediate(faces, indices, constants, {}, state,
                           projection.has_transform ? &projection.transform : nullptr);
}

void DrawVolume(const Volume &volume, const Projection &projection, const RenderInfo &info) {
    DrawCount(volume.both, volume.cap, projection, info, true);
    DrawCount(volume.both, volume.away, projection, info, false);
    // The GS is left with the subtracting pass's ALPHA.
    MGPortRegisters &current = MGPortCurrent();
    current.alpha.bits.a = 2;
    current.alpha.bits.b = 0;
    current.alpha.bits.c = 2;
    current.alpha.bits.d = 1;
    current.alpha.bits.fix = 0x80;
}

} // namespace

// The shadow mesh's triangles, three model-space corners each, for the fast programs to extrude.
PC_OVERRIDE int CVisualShadow::CreateVUdataShadow(u_int *block, u_int *model_data) {
    vu_data = block;
    vu_size = kDraw3DBlockQuads;
    Draw3DVisual &visual = Draw3DRegisterVisual(block, false);

    MDT_HEADER    *model = reinterpret_cast<MDT_HEADER *>(model_data);
    MDT_SHADOW    *shadow = reinterpret_cast<MDT_SHADOW *>(reinterpret_cast<u_char *>(model) + model->mesh_ofs);
    sceVu0FVECTOR *vertices = reinterpret_cast<sceVu0FVECTOR *>(reinterpret_cast<u_char *>(model) + model->vertex_ofs);
    MDT_SVERTEX   *corner = reinterpret_cast<MDT_SVERTEX *>(shadow->shape);
    for (int shape = 0; shape < shadow->shape_num; shape++) {
        int index_num = reinterpret_cast<MDT_SSHAPE *>(corner)->index_num;
        corner = reinterpret_cast<MDT_SSHAPE *>(corner)->vertex;
        for (int emitted = 0; emitted < index_num; emitted += 3, corner += 3) {
            for (int i = 0; i < 3; i++) {
                gfx::Vertex3D vertex = {};
                std::memcpy(vertex.position, vertices[corner[i].index], sizeof(vertex.position));
                visual.vertices.push_back(vertex);
            }
        }
    }
    return vu_size;
}

PC_OVERRIDE int CVisualShadow::RemakeData(u_int *block) {
    if (data == nullptr) {
        return 0;
    }
    return CreateVUdataShadow(vu_data_buffer[DBuffID], data);
}

// This frame's volume of the selected triangles, in eye space: the faces both passes draw, those
// only subtracted and the near-plane sections, as the record's three strips.
PC_OVERRIDE int CVisualShadow::CreateVUdataShadowCLIP(u_int *block, u_int *model_data, RenderInfo *info, float (*matrix)[4]) {
    if (model_data == nullptr) {
        return 0;
    }
    vu_data = block;
    vu_size = kDraw3DBlockQuads;
    Draw3DVisual &visual = Draw3DRegisterVisual(block, true);

    int                             count = ShadowClipBuild(nullptr, 0, model_data, info, matrix, 0);
    std::vector<ShadowClipTriangle> triangles(static_cast<size_t>(count));
    ShadowClipBuild(triangles.data(), count, model_data, info, matrix, 0);

    Projection projection = Project(*info, matrix);
    float      near_cap = NearPlane(projection) * 1.001f;
    Volume     volume;
    for (const ShadowClipTriangle &triangle : triangles) {
        Vec3 local[3];
        for (int i = 0; i < 3; i++) {
            local[i] = {triangle.local[i][0], triangle.local[i][1], triangle.local[i][2]};
        }
        Vec3 top[3];
        Vec3 drop[3];
        Corners(projection, local, top, drop);
        bool sides[3] = {triangle.edges[0] == 0, triangle.edges[1] == 0, triangle.edges[2] == 0};
        AddPrism(volume, top, drop, sides, Count::AwayOnly);
        AddNearCap(volume, top, drop, near_cap);
    }

    Draw3DStrip both;
    both.index_count = static_cast<uint32_t>(volume.both.size());
    Draw3DStrip away;
    away.first_index = both.index_count;
    away.index_count = static_cast<uint32_t>(volume.away.size());
    Draw3DStrip cap;
    cap.first_index = away.first_index + away.index_count;
    cap.index_count = static_cast<uint32_t>(volume.cap.size());
    visual.strips = {both, away, cap};
    visual.vertices = std::move(volume.both);
    visual.vertices.insert(visual.vertices.end(), volume.away.begin(), volume.away.end());
    visual.vertices.insert(visual.vertices.end(), volume.cap.begin(), volume.cap.end());
    return vu_size;
}

PC_OVERRIDE int CVisualShadow::DrawVu1(u_int *packet, float (*matrix)[4], RenderInfo *info, VU1_PROGRAM program,
                                       u_long128 *draw_state, int unknown1, int unknown2) {
    if (info->shadow_pass != 1 && info->shadow_pass != 2) {
        return CVisualMDTVu1::DrawVu1(packet, matrix, info, program, draw_state, unknown1, unknown2);
    }
    if (!Draw3DShadowTargetActive()) {
        return 0;
    }
    Projection projection = Project(*info, matrix);

    if (info->shadow_pass == 2) {
        u_int *saved_primary = vu_data_buffer[0];
        u_int *saved_secondary = vu_data_buffer[1];
        u_int  saved_size = vu_size;
        ActiveData->Align64();
        ActiveData->Alloc(CreateVUdataShadowCLIP(reinterpret_cast<u_int *>(ActiveData->base + ActiveData->used * 16), data,
                                                 info, matrix));
        if (const Draw3DVisual *visual = Draw3DFindVisual(vu_data); visual && visual->strips.size() == 3) {
            Volume volume;
            auto   begin = visual->vertices.begin();
            volume.both.assign(begin, begin + visual->strips[1].first_index);
            volume.away.assign(begin + visual->strips[1].first_index, begin + visual->strips[2].first_index);
            volume.cap.assign(begin + visual->strips[2].first_index, visual->vertices.end());
            DrawVolume(volume, projection, *info);
        }
        vu_data_buffer[0] = saved_primary;
        vu_data_buffer[1] = saved_secondary;
        vu_size = saved_size;
        return 0;
    }

    vu_data = vu_data_buffer[DBuffID];
    const Draw3DVisual *visual = Draw3DFindVisual(vu_data);
    if (visual == nullptr) {
        return 0;
    }
    bool   away_only = Draw3DCurrentShadowProgram() == Draw3DShadowProgram::AwayFromLight;
    bool   sides[3] = {true, true, true};
    Volume volume;
    for (size_t first = 0; first + 3 <= visual->vertices.size(); first += 3) {
        Vec3 local[3];
        for (int i = 0; i < 3; i++) {
            const float *p = visual->vertices[first + i].position;
            local[i] = {p[0], p[1], p[2]};
        }
        if (away_only && !FacesAwayFromLight(projection, local)) {
            continue;
        }
        Vec3 top[3];
        Vec3 drop[3];
        Corners(projection, local, top, drop);
        AddPrism(volume, top, drop, sides, Count::Both);
    }
    DrawVolume(volume, projection, *info);
    return 0;
}

PC_OVERRIDE int CVisualShadow::DrawVu1(sceVif1Packet *packet, float (*matrix)[4], RenderInfo *info, VU1_PROGRAM program,
                                       u_long128 *draw_state, int unknown1, int unknown2) {
    return CVisualShadow::DrawVu1(static_cast<u_int *>(nullptr), matrix, info, program, draw_state, unknown1, unknown2);
}
