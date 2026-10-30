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
#include "fishing/host.hpp"
#include "gameutil.hpp"
#include "mathutil.hpp"

namespace dc::game {

namespace {

constexpr int kMaxNearbyPolys = 256;

int GetActCnt() {
    int count = 20;
    count += rand() % 100;
    return count;
}

} // namespace

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

    // What the fish does about the bait is the current place's bite stage's (fishing/angler.cpp).
    if (action_timer < 0 || action == FISH_ACTION_NONE) {
        fishing::UpdateFishAction(*this);
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
        // Retail's 0.1, unless the bite stage hurries the fish it chose (fishing/angler.cpp).
        target_speed = fishing::ApproachSpeed(*this);

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

    // A fight that steers the fish (fishing/angler.cpp) holds it at the line's end, facing its way.
    if (action == FISH_ACTION_BATTLE && fishing::FightSteers()) {
        target_yaw = fishing::BattleHeading();
        target_speed = 0.0f;
        speed = 0.0f;
        turn_step *= 2.0f;
    } else if (action == FISH_ACTION_BATTLE) {
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

// A kind's row comes from its species (fishing/host.cpp).
void LoadFish(CFish *fish, int fish_kind, int texture_slot, CDataAlloc2<1> *alloc, int loaded_before) {
    const FishInfo *info = fishing::Info(fish_kind);
    if (info == nullptr) {
        return;
    }

    LoadFile("chara/f00s.chr", read_buffer, nullptr);
    fish->Initialize();
    fish->model.LoadPackData3(read_buffer, "info.cfg", alloc, texture_slot, alloc, loaded_before, 0);
    fish->info = *info;
    fish->SetScale();
    fish->fish_kind = fish_kind;
}

char *GetFishFileName(int fish_kind) {
    return fishing::ModelPath(fish_kind);
}

} // namespace dc::game
