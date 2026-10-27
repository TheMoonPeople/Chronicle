#include "fish.hpp"

#include <libvu0.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <memory>
#include <utility>

#include "collision.hpp"
#include "dataread.hpp"
#include "gameutil.hpp"
#include "mathutil.hpp"

namespace dc::game {

namespace {

constexpr std::array<FishInfo, 18> fish_info = {
    {
     {17.0f, 10.0f, 20.0f, 20, 50, {1.0f, 0.0f, 0.0f, 0.2f, 0.2f, 0.2f, 0.0f, 0.2f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}},
     {19.5f, 8.0f, 16.0f, 10, 30, {0.0f, 0.5f, 0.5f, 0.2f, 0.2f, 0.2f, 0.0f, 0.2f, 0.0f, 0.0f, 0.5f, 0.5f, 1.0f}},
     {25.5f, 8.0f, 19.0f, 8, 25, {0.0f, 0.5f, 0.5f, 0.2f, 0.2f, 0.2f, 0.0f, 0.2f, 0.0f, 1.0f, 0.5f, 0.5f, 0.0f}},
     {35.4f, 12.0f, 24.0f, 30, 60, {0.5f, 0.0f, 0.0f, 0.2f, 0.2f, 0.2f, 0.0f, 0.2f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f}},
     {28.0f, 8.0f, 16.0f, 10, 40, {0.0f, 0.5f, 0.5f, 0.2f, 0.2f, 0.2f, 0.0f, 0.2f, 0.0f, 0.0f, 0.5f, 0.5f, 1.0f}},
     {21.0f, 10.0f, 16.0f, 200, 400, {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}},
     {20.0f, 6.0f, 12.0f, 15, 40, {0.0f, 1.0f, 0.5f, 0.2f, 0.2f, 0.2f, 0.0f, 0.2f, 0.0f, 0.5f, 0.5f, 0.5f, 0.0f}},
     {20.0f, 6.0f, 10.0f, 20, 50, {0.0f, 0.5f, 1.0f, 0.2f, 0.2f, 0.2f, 0.0f, 0.2f, 0.0f, 0.5f, 0.5f, 0.5f, 0.0f}},
     {1.0f, 1.0f, 2.0f, 1, 2, {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}},
     {21.0f, 8.0f, 16.0f, 100, 200, {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f}},
     {20.0f, 8.0f, 16.0f, 40, 80, {0.0f, 0.5f, 0.5f, 0.2f, 0.2f, 0.2f, 0.0f, 0.2f, 0.0f, 0.5f, 1.0f, 0.5f, 0.5f}},
     {20.0f, 6.0f, 12.0f, 20, 40, {1.0f, 0.5f, 0.5f, 0.2f, 0.2f, 0.2f, 0.0f, 0.2f, 0.0f, 0.0f, 0.5f, 0.5f, 0.0f}},
     {15.0f, 6.0f, 12.0f, 10, 30, {1.0f, 0.5f, 0.5f, 0.2f, 0.2f, 0.2f, 0.0f, 0.2f, 0.0f, 0.0f, 0.5f, 0.5f, 0.0f}},
     {21.0f, 10.0f, 20.0f, 35, 70, {0.0f, 0.0f, 0.0f, 0.2f, 0.2f, 0.2f, 0.0f, 0.2f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f}},
     {21.0f, 8.0f, 16.0f, 15, 40, {0.0f, 0.0f, 0.0f, 0.2f, 0.2f, 0.2f, 0.0f, 0.2f, 0.0f, 1.0f, 0.5f, 0.5f, 0.0f}},
     {20.0f, 8.0f, 20.0f, 20, 40, {0.0f, 0.0f, 0.0f, 0.2f, 0.2f, 0.2f, 0.0f, 0.2f, 0.0f, 0.5f, 1.0f, 0.5f, 0.0f}},
     {20.0f, 8.0f, 14.0f, 20, 40, {0.0f, 0.0f, 0.0f, 0.2f, 0.2f, 0.2f, 0.0f, 0.2f, 0.0f, 0.5f, 0.5f, 1.0f, 1.0f}},
     {21.0f, 10.0f, 30.0f, 600, 1000, {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.5f, 0.0f, 0.0f, 0.0f}},
     }
};

constexpr int kMaxNearbyPolys = 256;

constexpr std::array<char *, 18> fish_file_names = {
    "chara/f01a.chr",
    "chara/f02a.chr",
    "chara/f03a.chr",
    "chara/f04a.chr",
    "chara/f05a.chr",
    "chara/f06a.chr",
    "chara/f07a.chr",
    "chara/f08a.chr",
    "chara/f09a.chr",
    "chara/f10a.chr",
    "chara/f11a.chr",
    "chara/f12a.chr",
    "chara/f13a.chr",
    "chara/f14a.chr",
    "chara/f15a.chr",
    "chara/f16a.chr",
    "chara/f17a.chr",
    "chara/f18a.chr",
};

int rand_check(float probability) {
    int   roll = rand();
    float threshold = probability * 1000.0f;
    return roll % 1000 < static_cast<int>(threshold) ? 1 : 0;
}

int GetActCnt() {
    int count = 20;
    count += rand() % 100;
    return count;
}

int GetActCnt2() {
    int count = 20;
    count += rand() % 30;
    return count;
}

} // namespace

int CFish::FindFood() {
    float position[4];

    if (food_kind < 0) {
        return 0;
    }

    GetPosition(position);

    if (food_radius <= 0.0f) {
        return 0;
    }

    if (DistVector(food_position, position) > food_radius) {
        return 0;
    }

    return rand_check(info.food_affinity[food_kind] * 0.3f) != 0 ? 1 : 0;
}

int CFish::EatFood() {
    float position[4];

    GetPosition(position);

    if (food_radius <= 0.0f) {
        return 0;
    }

    if (DistVector(food_position, position) > 1.0f) {
        return 0;
    }

    return rand_check(info.food_affinity[food_kind] * 0.5f) != 0 ? 1 : 0;
}

int CFish::BiteHook() {
    float position[4];

    GetPosition(position);

    if (food_radius <= 0.0f) {
        return 0;
    }

    if (DistVector(food_position, position) > 1.0f) {
        return 0;
    }

    return rand_check(info.food_affinity[food_kind] * 0.5f) != 0 ? 1 : 0;
}

int CFish::LeaveFood() {
    if (food_kind < 0) {
        return 1;
    }

    return rand_check(0.1f) != 0 ? 1 : 0;
}

int CFish::LeaveHook() {
    if (food_kind < 0) {
        return 1;
    }

    return rand_check(0.1f) != 0 ? 1 : 0;
}

void CFish::SetCPoly(CCPoly *polys, int count) {
    collision_polys = polys;
    collision_poly_count = count;
}

void CFish::Step() {
    float position[4];
    float rotation[4];
    float velocity[4];
    float next_position[4];
    float hit_position[4];

    if (fish_kind < 0) {
        return;
    }

    float turn_step = 0.05f;
    float speed_step = 0.01f;
    GetPosition(position);

    if (action_timer < 0 || action == FISH_ACTION_NONE) {
        action_timer = GetActCnt2();

        switch (action) {
            case FISH_ACTION_NONE:
                if (FindFood()) {
                    action = FISH_ACTION_APPROACH_FOOD;
                }

                break;
            case FISH_ACTION_APPROACH_FOOD:
                if (EatFood()) {
                    action = FISH_ACTION_EAT_FOOD;
                }

                if (LeaveFood()) {
                    action = FISH_ACTION_LEAVE;
                    action_timer >>= 1;
                }

                break;
            case FISH_ACTION_EAT_FOOD:
                if (BiteHook()) {
                    action = FISH_ACTION_BITE_HOOK;
                }

                if (LeaveHook()) {
                    action = FISH_ACTION_LEAVE;
                }

                break;
            case FISH_ACTION_BITE_HOOK:
                action = FISH_ACTION_LEAVE;
                action_timer *= 2;
                break;
            case FISH_ACTION_LEAVE:
                action = FISH_ACTION_NONE;
                break;
            default:
                break;
        }
    }

    if (move_timer <= 0) {
        if (move_mode == FISH_MOVE_UNSET) {
            move_mode = FISH_MOVE_SWIM;
        } else {
            move_mode = static_cast<FishMoveMode>(rand() % 2);
        }

        move_timer = GetActCnt();
        target_speed = 0.0f;

        if (move_mode != FISH_MOVE_STOP) {
            target_yaw = TWO_PI * (static_cast<float>(rand()) / 2147483648.0f);
            target_yaw = AngleLimit(target_yaw);
            move_timer += 30;
            target_speed = 0.2f + (0.5f * static_cast<float>(rand()) / 2147483648.0f);
        }
    }

    if (action == FISH_ACTION_APPROACH_FOOD) {
        target_yaw = atan2f(food_position[0] - position[0], food_position[2] - position[2]);
        target_speed = 0.1f;

        if (DistVector(food_position, position) < 1.0f) {
            target_speed = 0.0f;
        }
    }

    if (action == FISH_ACTION_EAT_FOOD) {
        target_yaw = atan2f(food_position[0] - position[0], food_position[2] - position[2]);
        target_speed = 0.0f;
    }

    if (action == FISH_ACTION_BITE_HOOK) {
        float food_yaw = atan2f(food_position[0] - position[0], food_position[2] - position[2]);
        target_yaw = food_yaw + 0.2f * (static_cast<float>(rand()) / 2147483648.0f);
        target_yaw = AngleLimit(target_yaw);
        target_speed = 0.0f;
    }

    if (action == FISH_ACTION_BATTLE) {
        target_yaw = TWO_PI * (static_cast<float>(rand()) / 2147483648.0f);
        target_yaw = AngleLimit(target_yaw);
        target_speed = 0.5f;
        turn_step *= 2.0f;
        speed_step *= 2.0f;
    }

    if (action == FISH_ACTION_LEAVE) {
        target_yaw = atan2f(food_position[0] - position[0], food_position[2] - position[2]);
        target_yaw = AngleLimit(target_yaw - PI);
        target_speed = 0.5f;
    }

    --move_timer;
    --action_timer;
    GetRotation(rotation);
    rotation[2] = 0.0f;
    rotation[0] = 0.0f;
    rotation[1] = AngleInterpolate(rotation[1], target_yaw, turn_step, INTERPOLATE_STEP);
    SetRotation(rotation);

    if (speed < target_speed) {
        speed += speed_step;
        speed = std::min(speed, target_speed);
    }

    if (speed > target_speed) {
        speed -= speed_step;
        speed = std::max(speed, target_speed);
    }

    velocity[0] = speed * sinf(rotation[1]);
    velocity[1] = 0.0f;
    velocity[2] = speed * cosf(rotation[1]);
    velocity[3] = 0.0f;

    switch (move_mode) {
        case FISH_MOVE_STOP:
            SetMotion(6, 0);
            break;
        case FISH_MOVE_SWIM:
            SetMotion(0, 0);
            break;
        case FISH_MOVE_FAST:
            SetMotion(1, 0);
            break;
        default:
            break;
    }

    if (action == FISH_ACTION_BITE_HOOK || action == FISH_ACTION_BATTLE) {
        SetMotion(1, 0);
    }

    angle_model.SetScale(angle_model_scale, angle_model_scale, angle_model_scale);
    model.SetScale(model_scale, model_scale, model_scale);

    if (use_angle_model) {
        angle_model.Step();
    } else {
        model.Step();
    }

    GetPosition(position);
    sceVu0AddVector(next_position, position, velocity);

    if (collision_polys != nullptr) {
        auto    nearby_polys = std::make_unique_for_overwrite<CCPoly[]>(kMaxNearbyPolys);
        CBoxVu0 box;
        box.max[0] = position[0] + 10.0f;
        box.max[1] = position[1] + 10.0f;
        box.max[2] = position[2];
        box.min[0] = position[0] - 10.0f;
        box.min[1] = position[1];
        box.min[2] = position[2] - 10.0f;
        int count = PickUpNearPoly(nearby_polys.get(), box, collision_polys, collision_poly_count);

        if (CheckHit(nearby_polys.get(), count, position, next_position, hit_position, 0, 0) < 0) {
            SetPosition(next_position);
        }

        GetPosition(position);
        CheckWidth(nearby_polys.get(), count, position, 2.0f * angle_model_scale, next_position, 0);
        SetPosition(next_position);
    }

    if (move_mode == FISH_MOVE_ANGLE) {
        float zero[4] = {0.0f, 0.0f, 0.0f, 0.0f};
        SetPosition(zero);
        SetRotation(zero);
    }
}

void CFish::Draw() {
    if (fish_kind < 0) {
        return;
    }

    if (use_angle_model != 0) {
        angle_model.Draw();
    } else {
        model.Draw();
    }
}

void CFish::SetFoodPos(int kind, float *position, float radius) {
    food_kind = kind;
    sceVu0CopyVector(food_position, position);
    food_radius = radius;
}

void CFish::SetAngleMode() {
    move_mode = FISH_MOVE_ANGLE;
    action = FISH_ACTION_ANGLE;
    target_speed = 0.0f;
}

void CFish::SetBattleMode() {
    move_mode = FISH_MOVE_BATTLE;
    action = FISH_ACTION_BATTLE;
}

namespace {

// CFish::SetScale's deviation is the sum of 12 uniform draws less 6. For a sum x in [2, 3], this
// is 12! times the chance the sum is at most x; the sum is symmetric about 6, so it also gives
// the chance of the top of the roll.
double LowTail(double x) {
    return std::pow(x, 12) - 12.0 * std::pow(x - 1.0, 12) + 66.0 * std::pow(x - 2.0, 12);
}

} // namespace

float FishSizeLift(float range) {
    // A display centimetre is 0.1 of size, and the roll moves a quarter of the range per unit.
    double centimetre = 0.4 / range;
    if (!(centimetre < 1.0)) {
        return 0.0f;
    }
    double at_max = LowTail(2.0);                      // the deviation reaches 4: clamped
    double below = LowTail(2.0 + centimetre) - at_max; // the centimetre below the largest
    return static_cast<float>(std::fmax(0.0, 1.0 - below / at_max));
}

float SmoothFishSize(float size, float base_size, float max_size) {
    if (max_size <= base_size || size <= base_size || size >= max_size) {
        return size;
    }
    float range = max_size - base_size;
    float t = (size - base_size) / range;
    float lifted = size + FishSizeLift(range) * range * std::pow(t, kFishSizeLiftExponent) * (1.0f - t);
    // Only a clamped roll is the largest size: a lifted one stays a hundredth of a centimetre under,
    // so FishingGetAngleFishSize's size * 10 cannot round up into the largest's centimetre. A roll
    // already that close keeps its own size, as retail.
    return std::fmax(size, std::fmin(lifted, max_size - 0.001f));
}

// Retail's roll (info.min_size is the kind's usual size, the roll's centre), with the sizes above it
// ramped into the largest (fish.hpp) before the model scales are taken.
void CFish::SetScale() {
    float deviation = nrnd();
    size = info.min_size;

    if (deviation >= 0.0f) {
        size += deviation * (info.max_size - info.min_size) / 4.0f;
    } else {
        size += deviation * (info.max_size - info.min_size) / 8.0f;
    }

    size = std::max(size, 0.5f * info.min_size);
    size = std::min(size, info.max_size);

    size = SmoothFishSize(size, info.min_size, info.max_size);

    angle_model_scale = size / info.model_size;
    model_scale = size / 25.0f;
}

int CFish::GetFP() {
    if (size >= info.min_size) {
        return static_cast<int>(info.min_fp + (info.max_fp - info.min_fp) * (size - info.min_size) / (info.max_size - info.min_size));
    }

    return static_cast<int>(info.min_fp - 0.5f * (info.min_fp * (info.min_size - size) / (info.min_size - 0.5f * info.min_size)));
}

void CFish::SetMotion(int motion_no, int flags) {
    model.motion_no = motion_no;
    model.motion_flags = flags;
    model.motion_speed = -1.0f;
    angle_model.motion_no = motion_no;
    angle_model.motion_flags = flags;
    angle_model.motion_speed = -1.0f;
}

void CFish::SetPosition(float *position) {
    model.SetPosition(position);
    angle_model.SetPosition(position);
}

void CFish::GetPosition(float *position) { angle_model.GetPosition(position); }

void CFish::SetRotation(float *rotation) {
    model.SetRotation(rotation);
    angle_model.SetRotation(rotation);
}

void CFish::GetRotation(float *rotation) { angle_model.GetRotation(rotation); }

void CFish::SetReference(CFrame *reference) {
    if (model.frame != nullptr) {
        model.frame->SetReference(reference);
    }

    if (angle_model.frame != nullptr) {
        angle_model.frame->SetReference(reference);
    }
}

void CFish::DeleteReference() {
    if (model.frame != nullptr) {
        model.frame->DeleteReference();
    }

    if (angle_model.frame != nullptr) {
        angle_model.frame->DeleteReference();
    }
}

void CFish::Initialize() {
    fish_kind = -1;
    action = FISH_ACTION_NONE;
    move_mode = FISH_MOVE_UNSET;
    move_timer = 0;
    action_timer = 0;
    use_angle_model = false;
    angle_model_scale = 1.0f;
    size = 10.0f;
    model_scale = 1.0f;
    model.Initialize();
    angle_model.Initialize();
    collision_polys = nullptr;
    collision_poly_count = 0;
    unk_78 = 0;
    target_yaw = 0.0f;
    unk_70 = 0;
    target_speed = 0.0f;
    speed = 0.0f;
    food_radius = -1.0f;
}

void LoadFish(CFish *fish, int fish_kind, int texture_slot, CDataAlloc2<1> *alloc, int loaded_before) {
    if (fish_kind < 0 || std::cmp_greater_equal(fish_kind, fish_info.size())) {
        return;
    }

    LoadFile("chara/f00s.chr", read_buffer, nullptr);
    fish->Initialize();
    fish->model.LoadPackData3(read_buffer, "info.cfg", alloc, texture_slot, alloc, loaded_before, 0);
    fish->info = fish_info[fish_kind];
    fish->SetScale();
    fish->fish_kind = fish_kind;
}

char *GetFishFileName(int fish_kind) {
    if (fish_kind < 0 || std::cmp_greater_equal(fish_kind, fish_file_names.size())) {
        return nullptr;
    }

    return fish_file_names[fish_kind];
}

} // namespace dc::game
