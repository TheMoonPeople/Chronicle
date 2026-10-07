#include "camera_zoom.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

#include "camerafollow.hpp"
#include "collision.hpp"
#include "dungeonmap.hpp"
#include "mouse_collision.hpp"
#include "platform/config.hpp"
#include "platform/input.hpp"

extern float camera_near_dist;
extern float camera_far_dist;

namespace {
constexpr float kMinDistance = 30.0f;
constexpr float kMaxDistance = 140.0f;
constexpr float kWheelStep = 8.0f;

struct Zoom {
    CCameraFollow *camera = nullptr;
    const void    *scene = nullptr;
    int            key = 0;
    float          requested = 0.0f;
    std::uint64_t  read = std::numeric_limits<std::uint64_t>::max();
};

Zoom                g_town, g_dungeon;
bool                g_town_scope = false;
float               g_near = 0.0f, g_far = 0.0f;
std::vector<CCPoly> g_polys;

void Prepare(Zoom &zoom, CCameraFollow *camera, const void *scene, int key, float normal) {
    if (zoom.camera != camera || zoom.scene != scene || zoom.key != key) {
        zoom = {camera, scene, key, normal};
    }
}

void Read(Zoom &zoom, float normal) {
    const auto &input = InputGetMouseLook();
    if (zoom.read == input.read) {
        return;
    }
    zoom.read = input.read;
    if (input.zoom_reset) {
        zoom.requested = normal;
    } else if (std::isfinite(input.zoom)) {
        zoom.requested = std::clamp(zoom.requested - input.zoom * kWheelStep, kMinDistance, kMaxDistance);
    }
}

CBoxVu0 Box(const CCameraFollow &camera, float requested) {
    CBoxVu0 box{};
    float   radius = std::max({std::fabs(camera.distance), requested,
                               std::hypot(camera.pos[0] - camera.ref[0], camera.pos[2] - camera.ref[2]),
                               std::hypot(camera.next_pos[0] - camera.next_ref[0], camera.next_pos[2] - camera.next_ref[2])}) +
                     20.0f;
    for (int i = 0; i < 3; ++i) {
        box.min[i] = std::min({camera.pos[i], camera.next_pos[i], camera.ref[i], camera.next_ref[i], camera.follow[i]}) - radius - std::fabs(camera.height);
        box.max[i] = std::max({camera.pos[i], camera.next_pos[i], camera.ref[i], camera.next_ref[i], camera.follow[i]}) + radius + std::fabs(camera.height);
    }
    return box;
}

void ApplyTown() {
    auto   *camera = g_town.camera;
    auto   *ground = const_cast<CEditGround *>(static_cast<const CEditGround *>(g_town.scene));
    CBoxVu0 box = Box(*camera, g_town.requested);
    int     count = MouseCameraPolys(*ground, box, 0xFFFF, g_polys);
    float   accepted = MouseCameraClampDistance(*camera, g_town.requested, g_polys.data(), count);
    camera->SetDistance(accepted);
    // The retail clear-camera branch otherwise assigns 70 every frame before Step.
    // Its wall/floor rules run normally using this validated distance for the current update.
    camera_near_dist = accepted;
    camera_far_dist = accepted + (g_far - g_near);
}
} // namespace

void TownZoomBegin(CCameraFollow *camera, CEditGround *ground, int map) {
    TownZoomEnd();
    if (!ConfigGet().mouse_zoom) {
        g_town = {};
        return;
    }
    if (!camera || !ground || !camera->follow_on || CCamera::StopCamera) {
        return;
    }
    g_near = camera_near_dist;
    g_far = camera_far_dist;
    Prepare(g_town, camera, ground, map, g_near);
    g_town_scope = true;
    ApplyTown();
}

void TownZoomRead(CCameraFollow *camera) {
    if (!g_town_scope || g_town.camera != camera || !camera->follow_on || CCamera::StopCamera) {
        return;
    }
    Read(g_town, g_near);
    ApplyTown();
}

void TownZoomEnd() {
    if (g_town_scope) {
        camera_near_dist = g_near;
        camera_far_dist = g_far;
        g_town_scope = false;
    }
}

void DungeonZoomApply(CCameraFollow *camera, CDungeonMap *map, bool lock_on, float previous_distance) {
    if (!ConfigGet().mouse_zoom) {
        if (g_dungeon.camera == camera && camera && camera->follow_on && !lock_on && !CCamera::StopCamera) {
            // Turning the option off returns to the dungeon camera's normal starting distance.
            g_dungeon.requested = 60.0f;
        } else {
            g_dungeon = {};
            return;
        }
    }
    if (!camera || !map || !camera->follow_on || CCamera::StopCamera || lock_on) {
        return;
    }
    // Retail's distance relaxation can expand beyond the last validated zoom destination.
    // Keep its collision contraction; validate every expansion ourselves before Step uses it.
    camera->SetDistance(std::min(camera->distance, previous_distance));
    Prepare(g_dungeon, camera, map, map->map_seed, 60.0f);
    if (ConfigGet().mouse_zoom) {
        Read(g_dungeon, 60.0f);
    }
    CBoxVu0 box = Box(*camera, g_dungeon.requested);
    int     count = MouseCameraPolys(*map, box, g_polys);
    // Dungeon's default eye can be only 7.6 units above a floor (6-unit follow point + 1.6
    // height). Keep that valid: walls retain ten-unit clearance and floors retain five.
    camera->SetDistance(MouseCameraClampDistance(*camera, g_dungeon.requested, g_polys.data(), count, 5.0f));
    if (!ConfigGet().mouse_zoom) {
        g_dungeon = {};
    }
}

float DungeonZoomNearDistance(CCameraFollow *camera, float normal, bool lock_on) {
    if (!camera || !ConfigGet().mouse_zoom || lock_on || g_dungeon.camera != camera ||
        !camera->follow_on || CCamera::StopCamera) {
        return normal;
    }
    // Retail lifts the eye whenever it comes within 60 units of the player. With an
    // intentionally closer zoom that would keep lifting it into a steep overhead view.
    return std::min(normal, g_dungeon.requested);
}
