#include "mouse_collision.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <numbers>
#include <typeinfo>

#include "camerafollow.hpp"
#include "editarea.hpp"
#include "editground.hpp"
#include "frame.hpp"
#include "rect.hpp"

namespace {

constexpr int   kPolyBudget = 32768;
constexpr int   kFrameBudget = 4096;
constexpr float kStep = 0.03f;
constexpr float kMargin = 0.001f;

struct Vec {
    double x, y, z;

    Vec operator+(Vec b) const { return {x + b.x, y + b.y, z + b.z}; }

    Vec operator-(Vec b) const { return {x - b.x, y - b.y, z - b.z}; }

    Vec operator*(double s) const { return {x * s, y * s, z * s}; }
};

Vec V(const float *p) { return {p[0], p[1], p[2]}; }

double Dot(Vec a, Vec b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

Vec Cross(Vec a, Vec b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }

double Length(Vec a) { return std::sqrt(Dot(a, a)); }

bool Finite(Vec a) { return std::isfinite(a.x) && std::isfinite(a.y) && std::isfinite(a.z); }

double PointSegment(Vec p, Vec a, Vec b) {
    Vec    ab = b - a;
    double d = Dot(ab, ab);
    double t = d > 0 ? std::clamp(Dot(p - a, ab) / d, 0.0, 1.0) : 0;
    return Length(p - (a + ab * t));
}

double SegmentSegment(Vec a, Vec b, Vec c, Vec d) {
    Vec    u = b - a, v = d - c, w = a - c;
    double aa = Dot(u, u), bb = Dot(u, v), cc = Dot(v, v), dd = Dot(u, w), ee = Dot(v, w);
    double result = std::min({PointSegment(a, c, d), PointSegment(b, c, d),
                              PointSegment(c, a, b), PointSegment(d, a, b)});
    double det = aa * cc - bb * bb;
    if (det > 1e-20) {
        double s = (bb * ee - cc * dd) / det, t = (aa * ee - bb * dd) / det;
        if (s >= 0 && s <= 1 && t >= 0 && t <= 1) {
            result = std::min(result, Length(w + u * s - v * t));
        }
    }
    return result;
}

bool Inside(Vec p, Vec a, Vec b, Vec c, Vec normal) {
    double s0 = Dot(Cross(b - a, p - a), normal), s1 = Dot(Cross(c - b, p - b), normal),
           s2 = Dot(Cross(a - c, p - c), normal);
    return s0 >= -1e-12 && s1 >= -1e-12 && s2 >= -1e-12;
}

double PointTriangle(Vec p, Vec a, Vec b, Vec c) {
    Vec    n = Cross(b - a, c - a);
    double nn = Dot(n, n);
    double result = std::min({PointSegment(p, a, b), PointSegment(p, b, c), PointSegment(p, c, a)});
    if (nn > 1e-20) {
        double side = Dot(p - a, n);
        Vec    projected = p - n * (side / nn);
        if (Inside(projected, a, b, c, n)) {
            result = std::min(result, std::fabs(side) / std::sqrt(nn));
        }
    }
    return result;
}

double SegmentTriangle(Vec from, Vec to, const CCPoly &poly) {
    Vec    a = V(poly.vertex[0]), b = V(poly.vertex[1]), c = V(poly.vertex[2]);
    Vec    n = Cross(b - a, c - a);
    double denom = Dot(to - from, n);
    if (std::fabs(denom) > 1e-20) {
        double t = Dot(a - from, n) / denom;
        if (t >= 0 && t <= 1 && Inside(from + (to - from) * t, a, b, c, n)) {
            return 0;
        }
    }
    return std::min({PointTriangle(from, a, b, c), PointTriangle(to, a, b, c),
                     SegmentSegment(from, to, a, b), SegmentSegment(from, to, b, c),
                     SegmentSegment(from, to, c, a)});
}

// The frame routine can write at most every triangle in its eligible subtree. Prove that
// bound before it sees an output pointer; unknown collision implementations fail closed.
int FrameBound(CFrame *frame, int &visits, int depth = 0) {
    if (!frame || frame->flags == 4) {
        return 0;
    }
    if (++visits > kFrameBudget || depth > 64) {
        return -1;
    }
    int count = 0;
    if (frame->collision && (frame->flags & 1)) {
        auto *mesh = dynamic_cast<CCollisionMDT *>(frame->collision);
        if (!mesh || typeid(*frame->collision) != typeid(CCollisionMDT) ||
            mesh->mesh_count < 0 || mesh->mesh_count > kPolyBudget ||
            (mesh->mesh_count && !mesh->mesh)) {
            return -1;
        }
        count = mesh->mesh_count;
    }
    if (!(frame->flags & 2) && !(frame->flags & 4)) {
        for (CFrame *child = frame->child; child; child = child->brother) {
            int n = FrameBound(child, visits, depth + 1);
            if (n < 0 || n > kPolyBudget - count) {
                return -1;
            }
            count += n;
        }
    }
    return count;
}

Vec Orbit(Vec p, Vec ref, double angle) {
    Vec    d = p - ref;
    double c = std::cos(angle), s = std::sin(angle);
    return {ref.x + d.x * c + d.z * s, p.y, ref.z + d.z * c - d.x * s};
}

struct Bound {
    Vec min{INFINITY, INFINITY, INFINITY}, max{-INFINITY, -INFINITY, -INFINITY};

    void Add(Vec p, double radius = 0) {
        min = {std::min(min.x, p.x - radius), std::min(min.y, p.y), std::min(min.z, p.z - radius)};
        max = {std::max(max.x, p.x + radius), std::max(max.y, p.y), std::max(max.z, p.z + radius)};
    }

    Vec Centre() const { return (min + max) * 0.5; }

    double Radius() const { return Length(max - min) * 0.5; }
};

bool CorridorClear(CCameraFollow &camera, double middle, double half, CCPoly *polys, int count) {
    constexpr double turn = 2 * std::numbers::pi;
    double           gap = std::remainder(double(camera.next_angle) - camera.angle, turn);
    Vec              ref = V(camera.ref), next_ref = V(camera.next_ref), follow = V(camera.follow);
    Vec              eye = V(camera.pos), next_eye = V(camera.next_pos);
    if (!Finite(ref) || !Finite(next_ref) || !Finite(follow) || !Finite(eye) || !Finite(next_eye)) {
        return false;
    }
    Bound eyes, refs;
    // Rotation moves each point by at most this chord distance from the interval midpoint.
    auto add = [&](Vec p, Vec r) {
        double radius = std::hypot(p.x - r.x, p.z - r.z);
        eyes.Add(Orbit(p, r, middle), 2 * radius * std::sin(half / 2));
    };
    add(eye, ref);
    add(next_eye, next_ref);
    double a = camera.angle + middle + gap / 2;
    Vec    target{follow.x + camera.distance * std::sin(a), follow.y + camera.height,
                  follow.z + camera.distance * std::cos(a)};
    double reach = 2 * std::fabs(camera.distance) * std::sin(std::min(std::numbers::pi, std::fabs(gap) / 2 + half) / 2);
    eyes.Add(target, reach);
    refs.Add(ref);
    refs.Add(next_ref);
    refs.Add(follow);
    Vec   e = eyes.Centre(), r = refs.Centre();
    float ep[4] = {float(e.x), float(e.y), float(e.z), 1}, rp[4] = {float(r.x), float(r.y), float(r.z), 1};
    // CCamera eases coordinates separately (and may snap/limit a coordinate), so bounding each
    // coordinate encloses its subsequent states, including the generated follow destinations.
    return MouseCameraClear(ep, rp, float(eyes.Radius()), float(refs.Radius()), polys, count,
                            float((eyes.max.y - eyes.min.y) / 2));
}

double ClearPrefix(CCameraFollow &camera, double from, double to, CCPoly *polys, int count,
                   int &budget, int depth = 0) {
    if (--budget < 0) {
        return from;
    }
    if (CorridorClear(camera, (from + to) / 2, std::fabs(to - from) / 2, polys, count)) {
        return to;
    }
    // A conservative envelope can touch a wall while the true arc remains clear. Subdivide
    // that envelope before stopping; every accepted interval still has a clearance proof.
    if (depth == 12) {
        return from;
    }
    double middle = (from + to) / 2;
    double first = ClearPrefix(camera, from, middle, polys, count, budget, depth + 1);
    if (first != middle) {
        return first;
    }
    return ClearPrefix(camera, middle, to, polys, count, budget, depth + 1);
}

} // namespace

int MouseCameraPolys(CEditGround &ground, CBoxVu0 &box, int mask, std::vector<CCPoly> &polys) {
    polys.clear();
    int  visits = 0;
    auto part = [&](CMapParts &part) {
        if (part.handle < 0 || !part.camera_frame || !part.CheckBox(&box)) {
            return true;
        }
        CFrame *frame = part.camera_frame;
        int     bound = FrameBound(frame, visits);
        if (bound < 0 || bound > kPolyBudget - int(polys.size())) {
            return false;
        }
        if (!bound) {
            return true;
        }
        frame->SetPosition(part.pos[0], part.pos[1], part.pos[2]);
        frame->SetRotation(part.rotation.x, part.rotation.y, part.rotation.z);
        auto start = polys.size();
        polys.resize(start + bound);
        int found = frame->PickUpNearPoly(polys.data() + start, box);
        if (found < 0 || found > bound) {
            return false;
        }
        polys.resize(start + found);
        return true;
    };
    if (mask & 2) {
        for (auto &p : ground.parts) {
            if (!part(p)) {
                return -1;
            }
        }
    }
    if (mask & 1) {
        for (auto &p : ground.fixed_parts) {
            if (!part(p)) {
                return -1;
            }
        }
    }
    // Whole grid areas cover the entire sweep, rather than only the four cells by its centre.
    // Each cell writes at most two triangles; dimensions also protect the retail grid indexing.
    for (CEditArea *area : ground.areas) {
        if (!area) {
            continue;
        }
        if (area->width < 0 || area->width > 16 || area->height < 0 || area->height > 16) {
            return -1;
        }
        int bound = 2 * area->width * area->height;
        if (bound > kPolyBudget - int(polys.size())) {
            return -1;
        }
        auto start = polys.size();
        polys.resize(start + bound);
        if (bound) {
            CRect_i_ rect(0, 0, area->width, area->height);
            int      found = area->PickUpPoly(polys.data() + start, rect);
            if (found < 0 || found > bound) {
                return -1;
            }
            polys.resize(start + found);
        }
    }
    return int(polys.size());
}

bool MouseCameraClear(const float *eye, const float *look, float eye_radius, float look_radius,
                      CCPoly *polys, int count, float vertical_radius) {
    Vec e = V(eye), r = V(look);
    if (count < 0 || (count && !polys) || !Finite(e) || !Finite(r) ||
        !std::isfinite(eye_radius) || !std::isfinite(look_radius) || !std::isfinite(vertical_radius) ||
        eye_radius < 0 || look_radius < 0 || vertical_radius < 0) {
        return false;
    }
    double ray_margin = std::max(eye_radius, look_radius) + kMargin;
    Vec    down = e + Vec{0, -18, 0}, up = e + Vec{0, 15, 0};
    for (int i = 0; i < count; ++i) {
        const CCPoly &p = polys[i];
        if (!Finite(V(p.vertex[0])) || !Finite(V(p.vertex[1])) || !Finite(V(p.vertex[2]))) {
            return false;
        }
        if (SegmentTriangle(r, e, p) <= ray_margin || PointTriangle(e, V(p.vertex[0]), V(p.vertex[1]), V(p.vertex[2])) <= 10 + eye_radius + kMargin) {
            return false;
        }
        Vec    normal = Cross(V(p.vertex[1]) - V(p.vertex[0]), V(p.vertex[2]) - V(p.vertex[0]));
        double floor_min = std::min({double(p.vertex[0][1]), double(p.vertex[1][1]), double(p.vertex[2][1])});
        double floor_max = std::max({double(p.vertex[0][1]), double(p.vertex[1][1]), double(p.vertex[2][1])});
        if (floor_max < down.y - vertical_radius - kMargin || floor_min > up.y + vertical_radius + kMargin) {
            continue;
        }
        if (std::fabs(normal.y) > 0.5 * Length(normal) && SegmentTriangle(down, up, p) <= eye_radius + kMargin) {
            return false;
        }
    }
    return true;
}

float MouseCameraClamp(CCameraFollow &camera, float turn, CCPoly *polys, int count) {
    if (!std::isfinite(turn) || !std::isfinite(camera.angle) || !std::isfinite(camera.next_angle) ||
        !std::isfinite(camera.distance) || !std::isfinite(camera.height) ||
        !std::isfinite(camera.speed) || camera.speed < 1.0f ||
        !CorridorClear(camera, 0, 0, polys, count)) {
        return 0;
    }
    // A complete revolution covers any further revolutions; keep work bounded for huge deltas.
    double check = std::copysign(std::min(std::fabs(double(turn)), 2 * std::numbers::pi), turn);
    int    steps = std::max(1, int(std::ceil(std::fabs(check) / kStep)));
    double accepted = 0;
    // Bound both interval subdivision and total geometry work. Exhaustion drops the remainder
    // of this read rather than queuing a delayed turn.
    int budget = std::min(4096, 1000000 / std::max(count, 1));
    for (int i = 0; i < steps; ++i) {
        double end = check * (i + 1) / steps;
        double prefix = ClearPrefix(camera, accepted, end, polys, count, budget);
        if (prefix != end) {
            return std::nextafter(float(prefix), 0.0f);
        }
        accepted = end;
    }
    return turn;
}
