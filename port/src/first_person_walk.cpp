#include "first_person_walk.hpp"

#include <libvu0.h>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <typeinfo>
#include <vector>

#include "btactstatus.hpp"
#include "collision.hpp"
#include "dranmapfield.hpp"
#include "dungeonmap.hpp"
#include "frame.hpp"
#include "gameutil.hpp"
#include "platform/input.hpp"

namespace {
constexpr int kPolys = 32768, kFrames = 4096;

int FrameBound(CFrame *frame, int &visits, int depth = 0) {
    if (!frame) {
        return 0;
    }
    if (++visits > kFrames || depth > 64) {
        return -1;
    }
    if (frame->flags == 4) {
        return 0;
    }
    int count = 0;
    if (frame->collision && (frame->flags & 1)) {
        auto *mesh = dynamic_cast<CCollisionMDT *>(frame->collision);
        if (!mesh || typeid(*frame->collision) != typeid(CCollisionMDT) ||
            mesh->mesh_count < 0 || mesh->mesh_count > kPolys ||
            (mesh->mesh_count && !mesh->mesh)) {
            return -1;
        }
        count = mesh->mesh_count;
    }
    if (!(frame->flags & 2) && !(frame->flags & 4)) {
        for (CFrame *child = frame->child; child; child = child->brother) {
            int n = FrameBound(child, visits, depth + 1);
            if (n < 0 || n > kPolys - count) {
                return -1;
            }
            count += n;
        }
    }
    return count;
}

} // namespace

bool FirstPersonDungeonStep(CDungeonMap &map, DRAN_MAP_FIELD_SET *fields, float *pos, float *velocity, MoveCheckInfo &info, int mode) {
    CBoxVu0 box{};
    for (int i = 0; i < 3; ++i) {
        if (!std::isfinite(pos[i]) || !std::isfinite(velocity[i])) {
            return false;
        }
        box.min[i] = std::min(pos[i], pos[i] + velocity[i]) - (i == 1 ? 40.0f : 20.0f);
        box.max[i] = std::max(pos[i], pos[i] + velocity[i]) + 20.0f;
    }
    static std::vector<CCPoly> polys;
    polys.clear();
    int  visits = 0;
    auto append = [&](CFrame *frame) {
        int bound = FrameBound(frame, visits);
        if (bound < 0 || bound > kPolys - static_cast<int>(polys.size())) {
            return false;
        }
        if (!bound) {
            return true;
        }
        auto start = polys.size();
        polys.resize(start + bound);
        int count = frame->PickUpNearPoly(polys.data() + start, box);
        if (count < 0 || count > bound) {
            return false;
        }
        polys.resize(start + count);
        return true;
    };
    auto part = [&](CFrame *frame, float x, float y, float z, int turn) {
        if (!frame) {
            return true;
        }
        if (turn < 0 || turn > 6) {
            return false;
        }
        if (turn > 3) {
            turn -= 3;
        }
        if (turn == 3) {
            turn = -1;
        }
        frame->SetRotation(0.0f, std::numbers::pi_v<float> * (-90.0f * turn) / 180.0f, 0.0f);
        frame->SetPosition(x, y, z);
        return append(frame);
    };
    if (map.map_type != 1) {
        for (auto &p : map.parts) {
            if (!p.frame[0]) {
                break;
            }
            if (!part(p.collision, p.frame_offset[0][0], p.frame_offset[0][1], p.frame_offset[0][2], static_cast<int>(p.frame_turn[0]) + p.collision_turn)) {
                return false;
            }
        }
    } else {
        for (int z = 0; z < 20; ++z) {
            for (int x = 0; x < 20; ++x) {
                auto &cell = map.cells[x + z * 20];
                if (cell.parts_no == -1) {
                    continue;
                }
                if (cell.parts_no < 0 || cell.parts_no >= 72 || cell.direction < 0 || cell.direction > 3) {
                    return false;
                }
                auto &p = map.parts[cell.parts_no];
                if (!part(p.collision, 160.0f * x, 0, 160.0f * z, cell.direction + p.collision_turn)) {
                    return false;
                }
            }
        }
    }
    for (auto &b : map.boxes) {
        if (b.used && map.box_collision_model) {
            map.box_collision_model->SetPosition(b.pos);
            if (!append(map.box_collision_model)) {
                return false;
            }
        }
    }
    if (map.atra_num < 0 || map.atra_num > 8) {
        return false;
    }
    for (int i = 0; i < map.atra_num; ++i) {
        if (map.atra[i].used && map.collision_model) {
            map.collision_model->SetPosition(map.atra[i].pos);
            if (!append(map.collision_model)) {
                return false;
            }
        }
    }
    if (fields) {
        if (fields->collision_count < 0 || fields->collision_count > 12) {
            return false;
        }
        for (int i = 0; i < fields->collision_count; ++i) {
            if (fields->state[i] > 1 && !append(fields->collision[i])) {
                return false;
            }
        }
    }
    sceVu0FVECTOR next;
    MoveCheck(pos, velocity, next, &info, polys.data(), static_cast<int>(polys.size()), mode);
    sceVu0CopyVector(pos, next);
    return true;
}

bool FirstPersonDungeonKeyboardVelocity(float *velocity, float heading, const BT_ACT_STATUS &status, bool held, bool alive, bool slow) {
    auto  keyboard = InputGetKeyboardMovement();
    bool  allowed = !status.movement_locked && status.can_act && !status.action_on && !status.action_busy && !held && alive;
    float speed = slow ? 0.3f : 0.6f;
    velocity[0] = allowed ? speed * (keyboard.x * cosf(heading) - keyboard.y * sinf(heading)) : 0.0f;
    velocity[2] = allowed ? speed * (-keyboard.y * cosf(heading) - keyboard.x * sinf(heading)) : 0.0f;
    return allowed;
}
