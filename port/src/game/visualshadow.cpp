#include "visualshadow.hpp"

#include <libvu0.h>

#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <span>
#include <utility>
#include <vector>

#include "character.hpp"
#include "dataalloc.hpp"
#include "dataset.hpp"
#include "mathutil.hpp"
#include "mdt.hpp"
#include "mglib.hpp"
#include "render/draw3d.hpp"
#include "render/shadowclip.hpp"
#include "renderinfo.hpp"

namespace dc::game {

using namespace dc::render;

// Retail's three shadow microprograms all build volumes: each caster triangle becomes a prism from
// the triangle down along light 0 to its drop onto the shadow plane, and the prism's faces are
// counted into the stencil, incremented for faces turned towards the eye and decremented for faces
// turned away, depth-tested against the scene and writing no depth (z-pass). That is retail's
// Cs + Cd and Cd - Cs of 1 in the black shadow target, saturating alike. A scene pixel inside a
// prism ends non-zero and MGEndDrawShadow darkens it. The count here is retail's, face for face:
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

bool Finite(Vec3 value) { return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z); }

bool FiniteMatrix(const float *matrix) {
    for (int i = 0; i < 16; i++) {
        if (!std::isfinite(matrix[i])) {
            return false;
        }
    }
    return true;
}

Vec3 operator-(Vec3 a, Vec3 b) { return {.x = a.x - b.x, .y = a.y - b.y, .z = a.z - b.z}; }

Vec3 operator+(Vec3 a, Vec3 b) { return {.x = a.x + b.x, .y = a.y + b.y, .z = a.z + b.z}; }

Vec3 operator*(Vec3 a, float s) { return {.x = a.x * s, .y = a.y * s, .z = a.z * s}; }

float Dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

Vec3 Cross(Vec3 a, Vec3 b) {
    return {.x = a.y * b.z - a.z * b.y, .y = a.z * b.x - a.x * b.z, .z = a.x * b.y - a.y * b.x};
}

float Length(Vec3 a) { return std::sqrt(Dot(a, a)); }

// A column-major [column][row] matrix applied to a point.
Vec3 Apply(const float m[4][4], Vec3 p) {
    return {.x = m[0][0] * p.x + m[1][0] * p.y + m[2][0] * p.z + m[3][0],
            .y = m[0][1] * p.x + m[1][1] * p.y + m[2][1] * p.z + m[3][1],
            .z = m[0][2] * p.x + m[1][2] * p.y + m[2][2] * p.z + m[3][2]};
}

enum class Count {
    // Added when it faces the eye, subtracted when it faces away.
    Both,
    // Subtracted when it faces away, left out when it faces the eye.
    AwayOnly,
};

// Triangles wound outwards (counter-clockwise on the target when turned towards the eye).
struct Volume {
    std::vector<gfx::Vertex3D> vertices;
    // Index lists retain face order while sharing each prism's six corners.
    std::vector<uint32_t> both;
    std::vector<uint32_t> away;
    std::vector<uint32_t> cap;
};

bool ShadowHashEnabled() {
    static const bool enabled = [] {
        const char *setting = std::getenv("DC_PERF_SHADOW_HASH");
        return setting != nullptr && std::strcmp(setting, "1") == 0;
    }();
    return enabled;
}

bool ShadowVolumePhasesEnabled() {
    static const bool enabled = [] {
        const char *setting = std::getenv("DC_PERF_GAME_PHASES");
        return setting != nullptr && std::strcmp(setting, "1") == 0;
    }();
    return enabled;
}

using ShadowVolumeClock = std::chrono::steady_clock;

struct ShadowVolumeSpan {
    double total = 0.0;
    double maximum = 0.0;
};

void NoteShadowVolumePhases(ShadowVolumeClock::time_point start, ShadowVolumeClock::time_point built,
                            ShadowVolumeClock::time_point assembled, ShadowVolumeClock::time_point drawn,
                            size_t vertices, size_t indices) {
    struct Samples {
        unsigned                        count = 0;
        std::array<ShadowVolumeSpan, 3> spans{};
        uint64_t                        vertices = 0;
        uint64_t                        indices = 0;
    };

    static Samples              samples;
    const std::array<double, 3> elapsed = {
        std::chrono::duration<double, std::milli>(built - start).count(),
        std::chrono::duration<double, std::milli>(assembled - built).count(),
        std::chrono::duration<double, std::milli>(drawn - assembled).count()};
    for (size_t i = 0; i < elapsed.size(); ++i) {
        samples.spans[i].total += elapsed[i];
        samples.spans[i].maximum = std::max(samples.spans[i].maximum, elapsed[i]);
    }
    samples.vertices += vertices;
    samples.indices += indices;
    if (++samples.count == 120) {
        std::fprintf(stderr,
                     "cpu precisevolume %u calls mean/max ms: build %.3f/%.3f assemble %.3f/%.3f "
                     "draw %.3f/%.3f vertices %llu indices %llu\n",
                     samples.count,
                     samples.spans[0].total / samples.count, samples.spans[0].maximum,
                     samples.spans[1].total / samples.count, samples.spans[1].maximum,
                     samples.spans[2].total / samples.count, samples.spans[2].maximum,
                     static_cast<unsigned long long>(samples.vertices), static_cast<unsigned long long>(samples.indices));
        samples = {};
    }
}

enum class FastShadowCount : size_t {
    Triangles,
    Selected,
    Prisms,
    Vertices,
    Indices,
    AwayOnlyCalls,
    EveryCalls,
    Count,
};

using FastShadowCounts = std::array<uint64_t, static_cast<size_t>(FastShadowCount::Count)>;

void NoteFastShadowPhases(ShadowVolumeClock::time_point start, ShadowVolumeClock::time_point built,
                          ShadowVolumeClock::time_point drawn, const FastShadowCounts &counts) {
    struct Samples {
        unsigned                        count = 0;
        std::array<ShadowVolumeSpan, 2> spans{};
        FastShadowCounts                counts{};
    };

    static Samples              samples;
    const std::array<double, 2> elapsed = {
        std::chrono::duration<double, std::milli>(built - start).count(),
        std::chrono::duration<double, std::milli>(drawn - built).count()};
    for (size_t i = 0; i < elapsed.size(); ++i) {
        samples.spans[i].total += elapsed[i];
        samples.spans[i].maximum = std::max(samples.spans[i].maximum, elapsed[i]);
    }
    for (size_t i = 0; i < counts.size(); ++i) {
        samples.counts[i] += counts[i];
    }
    if (++samples.count == 120) {
        const auto count = [&](FastShadowCount kind) {
            return static_cast<unsigned long long>(samples.counts[static_cast<size_t>(kind)]);
        };
        std::fprintf(stderr,
                     "cpu fastvolume %u valid calls mean/max ms: build %.3f/%.3f draw %.3f/%.3f "
                     "triangles %llu selected %llu prisms %llu vertices %llu indices %llu away_only_calls %llu every_calls %llu\n",
                     samples.count, samples.spans[0].total / samples.count, samples.spans[0].maximum,
                     samples.spans[1].total / samples.count, samples.spans[1].maximum,
                     count(FastShadowCount::Triangles), count(FastShadowCount::Selected), count(FastShadowCount::Prisms),
                     count(FastShadowCount::Vertices), count(FastShadowCount::Indices), count(FastShadowCount::AwayOnlyCalls),
                     count(FastShadowCount::EveryCalls));
        samples = {};
    }
}

constexpr uint64_t kShadowHashOffset = 14695981039346656037ULL;
constexpr uint64_t kShadowHashPrime = 1099511628211ULL;

enum class ShadowHashSection : uint32_t {
    Call,
    Vertices,
    Both,
    Away,
    Cap,
};

void HashShadowWord(uint64_t &hash, uint32_t value) {
    for (unsigned shift = 0; shift < 32; shift += 8) {
        hash = (hash ^ ((value >> shift) & 0xffu)) * kShadowHashPrime;
    }
}

void HashShadowSize(uint64_t &hash, uint64_t value) {
    HashShadowWord(hash, static_cast<uint32_t>(value));
    HashShadowWord(hash, static_cast<uint32_t>(value >> 32));
}

void NoteFastShadowHash(const Volume &volume, bool away_only) {
    static uint64_t hash = kShadowHashOffset;
    static uint64_t calls = 0;
    static unsigned count = 0;
    HashShadowWord(hash, static_cast<uint32_t>(ShadowHashSection::Call));
    HashShadowSize(hash, ++calls);
    HashShadowWord(hash, away_only ? 1u : 0u);
    HashShadowWord(hash, static_cast<uint32_t>(ShadowHashSection::Vertices));
    HashShadowSize(hash, volume.vertices.size());
    for (const gfx::Vertex3D &vertex : volume.vertices) {
        for (float coordinate : vertex.position) {
            HashShadowWord(hash, std::bit_cast<uint32_t>(coordinate));
        }
    }
    const auto indices = [&](ShadowHashSection section, const std::vector<uint32_t> &values) {
        HashShadowWord(hash, static_cast<uint32_t>(section));
        HashShadowSize(hash, values.size());
        for (uint32_t value : values) {
            HashShadowWord(hash, value);
        }
    };
    indices(ShadowHashSection::Both, volume.both);
    indices(ShadowHashSection::Away, volume.away);
    indices(ShadowHashSection::Cap, volume.cap);
    if (++count == 120) {
        std::fprintf(stderr, "cpu fastshadowhash valid_calls %llu-%llu n%u geometry %016llx\n",
                     static_cast<unsigned long long>(calls - count + 1), static_cast<unsigned long long>(calls), count,
                     static_cast<unsigned long long>(hash));
        hash = kShadowHashOffset;
        count = 0;
    }
}

uint32_t AddVertex(Volume &volume, Vec3 corner) {
    gfx::Vertex3D vertex = {};
    vertex.position[0] = corner.x;
    vertex.position[1] = corner.y;
    vertex.position[2] = corner.z;
    vertex.color[0] = vertex.color[1] = vertex.color[2] = vertex.color[3] = 0x80;
    volume.vertices.push_back(vertex);
    return static_cast<uint32_t>(volume.vertices.size() - 1);
}

void Emit(std::vector<uint32_t> &out, std::span<const uint32_t> polygon) {
    for (size_t i = 1; i + 1 < polygon.size(); i++) {
        out.insert(out.end(), {polygon[0], polygon[i], polygon[i + 1]});
    }
}

// A face of a prism, wound outwards when as_given (else reversed).
void AddFace(Volume &volume, std::initializer_list<uint32_t> points, bool as_given, Count count) {
    std::array<uint32_t, 4> polygon{};
    std::ranges::copy(points, polygon.begin());
    if (!as_given) {
        std::ranges::reverse(polygon.begin(), polygon.begin() + points.size());
    }
    Emit(count == Count::Both ? volume.both : volume.away,
         std::span<const uint32_t>(polygon.data(), points.size()));
}

// Whether the triangle's prism has an inside, and if so whether its top is wound against the drop
// direction (down). A prism the light grazes has no inside and counts nothing, which is also what
// the microprograms make of it.
bool PrismThickness(const Vec3 top[3], const Vec3 drop[3], bool &down) {
    for (int i = 0; i < 3; i++) {
        if (!Finite(top[i]) || !Finite(drop[i])) {
            return false;
        }
    }
    Vec3  normal = Cross(top[1] - top[0], top[2] - top[0]);
    Vec3  centre_top = (top[0] + top[1] + top[2]) * (1.0f / 3.0f);
    Vec3  centre_drop = (drop[0] + drop[1] + drop[2]) * (1.0f / 3.0f);
    Vec3  height = centre_drop - centre_top;
    float area = Length(normal);
    float span = Length(height);
    float thickness = Dot(normal, height);
    if (!std::isfinite(area) || !std::isfinite(span) || area == 0.0f || span == 0.0f ||
        !std::isfinite(thickness) || std::fabs(thickness) <= 1e-4f * area * span) {
        return false;
    }
    // Every face's outward winding follows from the one sign of the prism's thickness: a triangle
    // extruded against its normal keeps its own winding on top, and each side runs down its edge.
    // A test per face, of the side the prism's centre lies on, is not used: for a prism the light
    // nearly grazes the centre lies almost on the long sides, float error turns one inwards, and it
    // counts with the wrong sign, a streak from the triangle to the ground.
    down = thickness < 0.0f;
    return true;
}

// The triangle's prism down to its drop on the plane.
void AddPrism(Volume &volume, const Vec3 top[3], const Vec3 drop[3], const bool sides[3], Count far_cap) {
    bool down = false;
    if (!PrismThickness(top, drop, down)) {
        return;
    }
    const auto first = static_cast<uint32_t>(volume.vertices.size());
    for (int i = 0; i < 3; i++) {
        AddVertex(volume, top[i]);
    }
    for (int i = 0; i < 3; i++) {
        AddVertex(volume, drop[i]);
    }
    AddFace(volume, {first, first + 1, first + 2}, down, Count::Both);
    AddFace(volume, {first + 3, first + 4, first + 5}, !down, far_cap);
    for (uint32_t edge = 0; edge < 3; edge++) {
        uint32_t next = (edge + 1) % 3;
        if (sides[edge]) {
            AddFace(volume, {first + edge, first + 3 + edge, first + 3 + next, first + next}, down, Count::Both);
        }
    }
}

// The prism's section by the plane z = depth, added wherever it covers: the eye is inside the
// volume there, which the faces in front of the scene cannot count.
void AddNearCap(Volume &volume, const Vec3 top[3], const Vec3 drop[3], float depth) {
    if (!std::isfinite(depth)) {
        return;
    }
    for (int i = 0; i < 3; i++) {
        if (!Finite(top[i]) || !Finite(drop[i])) {
            return;
        }
    }
    const std::array<const Vec3 *, 6> corners = {&top[0], &top[1], &top[2], &drop[0], &drop[1], &drop[2]};
    static constexpr int              kEdges[9][2] = {
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
    std::array<Vec3, 9> section{};
    size_t              section_count = 0;
    for (const auto &edge : kEdges) {
        Vec3  a = *corners[edge[0]];
        Vec3  b = *corners[edge[1]];
        float da = a.z - depth;
        float db = b.z - depth;
        if ((da < 0.0f) != (db < 0.0f)) {
            Vec3 point = a + (b - a) * (da / (da - db));
            point.z = depth;
            if (!Finite(point)) {
                return;
            }
            section[section_count++] = point;
        }
    }
    if (section_count < 3) {
        return;
    }
    Vec3 mean = {.x = 0.0f, .y = 0.0f, .z = 0.0f};
    for (size_t i = 0; i < section_count; ++i) {
        mean = mean + section[i];
    }
    mean = mean * (1.0f / static_cast<float>(section_count));
    if (!Finite(mean)) {
        return;
    }
    std::sort(section.begin(), section.begin() + section_count, [mean](Vec3 a, Vec3 b) {
        return std::atan2(a.y - mean.y, a.x - mean.x) > std::atan2(b.y - mean.y, b.x - mean.x);
    });
    std::array<uint32_t, 9> indices{};
    for (size_t i = 0; i < section_count; i++) {
        indices[i] = AddVertex(volume, section[i]);
    }
    Emit(volume.cap, std::span<const uint32_t>(indices.data(), section_count));
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
    bool               valid;
};

Projection Project(const RenderInfo &info, float (*matrix)[4]) {
    Projection projection = {};
    Vec3       normal = {.x = info.shadow_normal[0], .y = info.shadow_normal[1], .z = info.shadow_normal[2]};
    Vec3       light = {
        .x = info.light_direction[0][0], .y = info.light_direction[1][0], .z = info.light_direction[2][0]};
    float normal_length = Length(normal);
    float light_length = Length(light);
    if (!std::isfinite(normal_length) || !std::isfinite(light_length) || normal_length == 0.0f ||
        light_length == 0.0f || std::fabs(Dot(normal, light)) <= 1e-3f * normal_length * light_length) {
        return projection;
    }
    float drop[4][4];
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
    auto row_dot = [&](int row) {
        return Dot({.x = matrix[row][0], .y = matrix[row][1], .z = matrix[row][2]}, light);
    };
    projection.local_light = {.x = row_dot(0), .y = row_dot(1), .z = row_dot(2)};
    projection.valid = FiniteMatrix(&projection.model_to_eye[0][0]) &&
                       FiniteMatrix(&projection.drop_to_eye[0][0]) &&
                       FiniteMatrix(&projection.eye_to_clip[0][0]) && Finite(projection.local_light);
    projection.has_transform = projection.has_transform && FiniteMatrix(transform.local);
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

void ClearVolume(Volume &volume) {
    volume.vertices.clear();
    volume.both.clear();
    volume.away.clear();
    volume.cap.clear();
}

uint32_t PositionBits(float value) {
    // Adding zero folds -0 into +0 so both signs share a key.
    return std::bit_cast<uint32_t>(value + 0.0f);
}

// For each triangle of a position list and each of its edges, the one other triangle that has the
// same two corner positions, as triangle * 4 + its edge, or -1 when none or several do.
void FindEdgeNeighbours(const std::vector<gfx::Vertex3D> &vertices, std::vector<std::array<int32_t, 3>> &out) {
    using Key = std::array<uint32_t, 3>;
    const size_t                          corners = vertices.size() / 3 * 3;
    std::vector<std::pair<Key, uint32_t>> positions;
    positions.reserve(corners);
    for (size_t i = 0; i < corners; i++) {
        const float *p = vertices[i].position;
        positions.emplace_back(Key{PositionBits(p[0]), PositionBits(p[1]), PositionBits(p[2])},
                               static_cast<uint32_t>(i));
    }
    std::ranges::sort(positions);
    std::vector<uint32_t> id(corners);
    uint32_t              next_id = 0;
    for (size_t i = 0; i < corners; i++) {
        if (i != 0 && positions[i].first != positions[i - 1].first) {
            next_id++;
        }
        id[positions[i].second] = next_id;
    }

    struct EdgeRef {
        uint64_t key;
        uint32_t triangle;
        uint32_t edge;

        auto operator<=>(const EdgeRef &other) const = default;
    };

    std::vector<EdgeRef> edges;
    edges.reserve(corners);
    out.assign(corners / 3, std::array<int32_t, 3>{-1, -1, -1});
    for (size_t triangle = 0; triangle < corners / 3; triangle++) {
        for (uint32_t edge = 0; edge < 3; edge++) {
            uint32_t a = id[triangle * 3 + edge];
            uint32_t b = id[triangle * 3 + (edge + 1) % 3];
            if (a != b) {
                edges.push_back({.key = (static_cast<uint64_t>(std::min(a, b)) << 32) | std::max(a, b),
                                 .triangle = static_cast<uint32_t>(triangle),
                                 .edge = edge});
            }
        }
    }
    std::ranges::sort(edges);
    for (size_t first = 0; first < edges.size();) {
        size_t last = first + 1;
        while (last < edges.size() && edges[last].key == edges[first].key) {
            last++;
        }
        if (last - first == 2 && edges[first].triangle != edges[last - 1].triangle) {
            const EdgeRef &a = edges[first];
            const EdgeRef &b = edges[last - 1];
            out[a.triangle][a.edge] = static_cast<int32_t>(b.triangle * 4 + b.edge);
            out[b.triangle][b.edge] = static_cast<int32_t>(a.triangle * 4 + a.edge);
        }
        first = last;
    }
}

struct Prism {
    Vec3 top[3];
    Vec3 drop[3];
    bool selected;
    bool solid;
};

// Whether the side of prism a across its edge (corner edge to edge + 1) is cancelled by the
// neighbour's: both prisms lie on opposite sides of the plane through the edge and the drop
// direction, so they share that quad with opposite outward windings and the adding and subtracting
// passes count it out. Anything less certain keeps the side.
bool SideCancelled(const Prism &a, const Prism &b, int edge) {
    int   next = (edge + 1) % 3;
    Vec3  base = a.top[edge];
    Vec3  normal = Cross(a.top[next] - base, a.drop[edge] - base);
    Vec3  far_corner = a.drop[next] - base;
    float scale = Length(normal);
    if (!std::isfinite(scale) || scale == 0.0f ||
        std::fabs(Dot(normal, far_corner)) > 1e-5f * scale * Length(far_corner)) {
        return false;
    }
    // The side of the plane a prism lies on, from its corners off the shared edge; 0 if unclear.
    auto side = [&](const Prism &prism) {
        float result = 0.0f;
        for (const Vec3 *corners : {prism.top, prism.drop}) {
            for (int i = 0; i < 3; i++) {
                Vec3  offset = corners[i] - base;
                float distance = Dot(normal, offset);
                if (std::fabs(distance) > 1e-5f * scale * Length(offset)) {
                    if (result != 0.0f && (distance < 0.0f) != (result < 0.0f)) {
                        return 0.0f;
                    }
                    result = distance;
                }
            }
        }
        return result;
    };
    float side_a = side(a);
    float side_b = side(b);
    return side_a != 0.0f && side_b != 0.0f && (side_a < 0.0f) != (side_b < 0.0f);
}

void DrawCount(const Volume &volume, const Projection &projection, const RenderInfo &info, bool add) {
    // DrawMeshImmediate copies recorded vertices and indices, so the pass list can be reused.
    static thread_local std::vector<uint32_t> indices;
    const std::span<const uint32_t>           own = add ? volume.cap : volume.away;
    indices.clear();
    indices.reserve(volume.both.size() + own.size());
    indices.insert(indices.end(), volume.both.begin(), volume.both.end());
    indices.insert(indices.end(), own.begin(), own.end());
    if (indices.empty()) {
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
    Draw3DShadowCount(state, !add);
    gfx::DrawMeshImmediate(volume.vertices, indices, constants, {}, state,
                           projection.has_transform ? &projection.transform : nullptr);
}

void DrawVolume(const Volume &volume, const Projection &projection, const RenderInfo &info) {
    DrawCount(volume, projection, info, true);
    DrawCount(volume, projection, info, false);
    // The GS is left with the subtracting pass's ALPHA.
    MGPortRegisters &current = MGPortCurrent();
    current.alpha.bits.a = 2;
    current.alpha.bits.b = 0;
    current.alpha.bits.c = 2;
    current.alpha.bits.d = 1;
    current.alpha.bits.fix = 0x80;
}

bool DirectShadowClipEnabled() {
    static const bool enabled = [] {
        const char *setting = std::getenv("DC_SHADOW_CLIP_DIRECT");
        return setting != nullptr && std::strcmp(setting, "1") == 0;
    }();
    return enabled;
}

int BuildSelectedShadowVolume(Volume &volume, const Projection &projection, unsigned int *model_data,
                              const RenderInfo &info, float (*matrix)[4], float near_cap) {
    sceVu0FVECTOR light = {info.light_direction[0][0], info.light_direction[1][0], info.light_direction[2][0], 0.0f};
    sceVu0FMATRIX transpose;
    sceVu0FVECTOR local_light;
    sceVu0TransposeMatrix(transpose, matrix);
    sceVu0ApplyMatrix(local_light, transpose, light);
    auto *model = reinterpret_cast<MDT_HEADER *>(model_data);
    auto *shadow = reinterpret_cast<MDT_SHADOW *>(reinterpret_cast<unsigned char *>(model) + model->mesh_ofs);
    auto *vertices = reinterpret_cast<sceVu0FVECTOR *>(reinterpret_cast<unsigned char *>(model) + model->vertex_ofs);
    auto *corner = reinterpret_cast<MDT_SVERTEX *>(shadow->shape);
    int   selected = 0;
    for (int shape = 0; shape < shadow->shape_num; ++shape) {
        const int index_count = reinterpret_cast<MDT_SSHAPE *>(corner)->index_num;
        corner = reinterpret_cast<MDT_SSHAPE *>(corner)->vertex;
        for (int emitted = 0; emitted < index_count; emitted += 3, corner += 3) {
            const int     a = corner[0].index;
            const int     b = corner[1].index;
            const int     c = corner[2].index;
            sceVu0FVECTOR edge0;
            sceVu0FVECTOR edge1;
            sceVu0FVECTOR face;
            sceVu0SubVector(edge0, vertices[b], vertices[a]);
            sceVu0SubVector(edge1, vertices[c], vertices[a]);
            sceVu0OuterProduct(face, edge0, edge1);
            if (sceVu0InnerProduct(face, local_light) > 0.0f) {
                continue;
            }
            const Vec3 local[3] = {
                {vertices[a][0], vertices[a][1], vertices[a][2]},
                {vertices[b][0], vertices[b][1], vertices[b][2]},
                {vertices[c][0], vertices[c][1], vertices[c][2]}
            };
            Vec3 top[3];
            Vec3 drop[3];
            Corners(projection, local, top, drop);
            const bool sides[3] = {corner[0].edge == 0, corner[1].edge == 0, corner[2].edge == 0};
            AddPrism(volume, top, drop, sides, Count::AwayOnly);
            AddNearCap(volume, top, drop, near_cap);
            ++selected;
        }
    }
    return selected;
}

} // namespace

int CVisualShadow::DrawVu1(u_int *packet, float (*matrix)[4], RenderInfo *info, VU1_PROGRAM program,
                           u_long128 *draw_state, int unknown1, int unknown2) {
    if (info->shadow_pass != 1 && info->shadow_pass != 2) {
        return CVisualMDTVu1::DrawVu1(packet, matrix, info, program, draw_state, unknown1, unknown2);
    }
    if (!Draw3DShadowTargetActive()) {
        return 0;
    }
    Projection projection = Project(*info, matrix);
    if (!projection.valid) {
        return 0;
    }

    if (info->shadow_pass == 2) {
        u_int     *saved_primary = vu_data_buffer[0];
        u_int     *saved_secondary = vu_data_buffer[1];
        u_int      saved_size = vu_size;
        const bool trace = ShadowVolumePhasesEnabled();
        const auto start = trace ? ShadowVolumeClock::now() : ShadowVolumeClock::time_point{};
        ActiveData->Align64();
        ActiveData->Alloc(CreateVUdataShadowCLIP(reinterpret_cast<u_int *>(ActiveData->base + static_cast<ptrdiff_t>(ActiveData->used) * 16), data,
                                                 info, matrix));
        const auto built = trace ? ShadowVolumeClock::now() : ShadowVolumeClock::time_point{};
        auto       assembled = built;
        auto       drawn = built;
        size_t     vertices = 0;
        size_t     indices = 0;
        if (const Draw3DVisual *visual = Draw3DFindVisual(vu_data);
            visual && visual->strips.size() == 3 && !visual->indices.empty()) {
            Volume volume;
            volume.vertices = visual->vertices;
            auto begin = visual->indices.begin();
            volume.both.assign(begin, begin + visual->strips[1].first_index);
            volume.away.assign(begin + visual->strips[1].first_index, begin + visual->strips[2].first_index);
            volume.cap.assign(begin + visual->strips[2].first_index, visual->indices.end());
            if (trace) {
                assembled = ShadowVolumeClock::now();
                vertices = volume.vertices.size();
                indices = volume.both.size() + volume.away.size() + volume.cap.size();
            }
            DrawVolume(volume, projection, *info);
        } else if (trace) {
            assembled = ShadowVolumeClock::now();
        }
        if (trace) {
            drawn = ShadowVolumeClock::now();
            NoteShadowVolumePhases(start, built, assembled, drawn, vertices, indices);
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
    const bool       trace_fast = ShadowVolumePhasesEnabled();
    const auto       fast_start = trace_fast ? ShadowVolumeClock::now() : ShadowVolumeClock::time_point{};
    FastShadowCounts fast_counts{};
    bool             away_only = Draw3DCurrentShadowProgram() == Draw3DShadowProgram::AwayFromLight;
    // Every selected triangle's prism is built first; each side is then left out where the
    // neighbouring triangle's prism lies across it and has the same quad with the opposite winding.
    static thread_local std::vector<Prism> prisms;
    static thread_local Volume             volume;
    const size_t                           triangles = visual->vertices.size() / 3;
    const bool                             linked = visual->shadow_neighbours.size() == triangles;
    prisms.resize(triangles);
    for (size_t triangle = 0; triangle < triangles; triangle++) {
        Prism &prism = prisms[triangle];
        Vec3   local[3];
        for (int i = 0; i < 3; i++) {
            const float *p = visual->vertices[triangle * 3 + i].position;
            local[i] = {.x = p[0], .y = p[1], .z = p[2]};
        }
        prism.selected = !away_only || FacesAwayFromLight(projection, local);
        prism.solid = false;
        if (prism.selected) {
            if (trace_fast) {
                ++fast_counts[static_cast<size_t>(FastShadowCount::Selected)];
            }
            Corners(projection, local, prism.top, prism.drop);
            bool down = false;
            prism.solid = PrismThickness(prism.top, prism.drop, down);
        }
    }
    ClearVolume(volume);
    volume.both.reserve(triangles * 24);
    for (size_t triangle = 0; triangle < triangles; triangle++) {
        const Prism &prism = prisms[triangle];
        if (!prism.solid) {
            continue;
        }
        if (trace_fast) {
            ++fast_counts[static_cast<size_t>(FastShadowCount::Prisms)];
        }
        bool sides[3] = {true, true, true};
        for (int edge = 0; linked && edge < 3; edge++) {
            int32_t link = visual->shadow_neighbours[triangle][edge];
            if (link < 0) {
                continue;
            }
            const auto other = static_cast<size_t>(link / 4);
            if (!prisms[other].solid) {
                continue;
            }
            // Both triangles of a pair take the lower one's verdict, so a marginal quad is never
            // kept by one and dropped by the other.
            sides[edge] = triangle < other ? !SideCancelled(prism, prisms[other], edge)
                                           : !SideCancelled(prisms[other], prism, link % 4);
        }
        AddPrism(volume, prism.top, prism.drop, sides, Count::Both);
    }
    const auto fast_built = trace_fast ? ShadowVolumeClock::now() : ShadowVolumeClock::time_point{};
    DrawVolume(volume, projection, *info);
    if (trace_fast) {
        const auto fast_drawn = ShadowVolumeClock::now();
        fast_counts[static_cast<size_t>(FastShadowCount::Triangles)] = triangles;
        fast_counts[static_cast<size_t>(FastShadowCount::Vertices)] = volume.vertices.size();
        fast_counts[static_cast<size_t>(FastShadowCount::Indices)] = volume.both.size() + volume.away.size() + volume.cap.size();
        fast_counts[static_cast<size_t>(FastShadowCount::AwayOnlyCalls)] = away_only ? 1 : 0;
        fast_counts[static_cast<size_t>(FastShadowCount::EveryCalls)] = away_only ? 0 : 1;
        NoteFastShadowPhases(fast_start, fast_built, fast_drawn, fast_counts);
        if (ShadowHashEnabled()) {
            NoteFastShadowHash(volume, away_only);
        }
    }
    return 0;
}

int CVisualShadow::DrawVu1(sceVif1Packet * /*packet*/, float (*matrix)[4], RenderInfo *info, VU1_PROGRAM program,
                           u_long128 *draw_state, int unknown1, int unknown2) {
    return CVisualShadow::DrawVu1(static_cast<u_int *>(nullptr), matrix, info, program, draw_state, unknown1, unknown2);
}

// The shadow mesh's triangles, three model-space corners each, for the fast programs to extrude.
int CVisualShadow::CreateVUdataShadow(u_int *block, u_int *model_data) {
    if (block == nullptr || model_data == nullptr) {
        return 0;
    }
    vu_data = block;
    vu_size = kDraw3DBlockQuads;
    Draw3DVisual &visual = Draw3DRegisterVisual(block, false);

    auto *model = reinterpret_cast<MDT_HEADER *>(model_data);
    auto *shadow = reinterpret_cast<MDT_SHADOW *>(reinterpret_cast<u_char *>(model) + model->mesh_ofs);
    auto *vertices = reinterpret_cast<sceVu0FVECTOR *>(reinterpret_cast<u_char *>(model) + model->vertex_ofs);
    auto *corner = reinterpret_cast<MDT_SVERTEX *>(shadow->shape);
    for (int shape = 0; shape < shadow->shape_num; shape++) {
        int index_num = reinterpret_cast<MDT_SSHAPE *>(corner)->index_num;
        corner = reinterpret_cast<MDT_SSHAPE *>(corner)->vertex;
        for (int emitted = 0; emitted + 3 <= index_num; emitted += 3, corner += 3) {
            Vec3 positions[3];
            bool valid = true;
            for (int i = 0; i < 3; i++) {
                int index = corner[i].index;
                if (index < 0 || index >= model->vertex_num) {
                    valid = false;
                    break;
                }
                positions[i] = {.x = vertices[index][0], .y = vertices[index][1], .z = vertices[index][2]};
                valid = valid && Finite(positions[i]);
            }
            if (!valid) {
                continue;
            }
            for (const Vec3 &position : positions) {
                gfx::Vertex3D vertex = {};
                vertex.position[0] = position.x;
                vertex.position[1] = position.y;
                vertex.position[2] = position.z;
                visual.vertices.push_back(vertex);
            }
        }
    }
    FindEdgeNeighbours(visual.vertices, visual.shadow_neighbours);
    return vu_size;
}

int CVisualShadow::RemakeData(u_int * /*block*/) {
    if (data == nullptr) {
        return 0;
    }
    return CreateVUdataShadow(vu_data_buffer[DBuffID], data);
}

// This frame's volume of the selected triangles, in eye space: the faces both passes draw, those
// only subtracted and the near-plane sections, as the record's three strips.
int CVisualShadow::CreateVUdataShadowCLIP(u_int *block, u_int *model_data, RenderInfo *info, float (*matrix)[4]) {
    if (model_data == nullptr) {
        return 0;
    }
    vu_data = block;
    vu_size = kDraw3DBlockQuads;
    Draw3DVisual &visual = Draw3DRegisterVisual(block, true);

    Projection projection = Project(*info, matrix);
    if (!projection.valid) {
        return 0;
    }
    float  near_cap = NearPlane(projection) * 1.001f;
    Volume volume;
    if (DirectShadowClipEnabled()) {
        if (BuildSelectedShadowVolume(volume, projection, model_data, *info, matrix, near_cap) == 0) {
            return vu_size;
        }
    } else {
        int count = ShadowClipBuild(nullptr, 0, model_data, info, matrix, 0);
        if (count <= 0) {
            return vu_size;
        }
        std::vector<ShadowClipTriangle> triangles(static_cast<size_t>(count));
        ShadowClipBuild(triangles.data(), count, model_data, info, matrix, 0);

        for (const ShadowClipTriangle &triangle : triangles) {
            Vec3 local[3];
            for (int i = 0; i < 3; i++) {
                local[i] = {.x = triangle.local[i][0], .y = triangle.local[i][1], .z = triangle.local[i][2]};
            }
            Vec3 top[3];
            Vec3 drop[3];
            Corners(projection, local, top, drop);
            bool sides[3] = {triangle.edges[0] == 0, triangle.edges[1] == 0, triangle.edges[2] == 0};
            AddPrism(volume, top, drop, sides, Count::AwayOnly);
            AddNearCap(volume, top, drop, near_cap);
        }
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
    visual.vertices = std::move(volume.vertices);
    visual.indices = std::move(volume.both);
    visual.indices.insert(visual.indices.end(), volume.away.begin(), volume.away.end());
    visual.indices.insert(visual.indices.end(), volume.cap.begin(), volume.cap.end());
    return vu_size;
}

} // namespace dc::game
