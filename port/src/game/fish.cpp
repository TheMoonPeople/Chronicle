#pragma name_counter 407

#include "fish.hpp"

#include <cstring>

#include "dataread.hpp"
#include "mathutil.hpp"

FishInfo fish_info[18] = {
    {17.0f, 10.0f, 20.0f, 20,  50,   {1.0f, 0.0f, 0.0f, 0.2f, 0.2f, 0.2f, 0.0f, 0.2f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}},
    {19.5f, 8.0f,  16.0f, 10,  30,   {0.0f, 0.5f, 0.5f, 0.2f, 0.2f, 0.2f, 0.0f, 0.2f, 0.0f, 0.0f, 0.5f, 0.5f, 1.0f}},
    {25.5f, 8.0f,  19.0f, 8,   25,   {0.0f, 0.5f, 0.5f, 0.2f, 0.2f, 0.2f, 0.0f, 0.2f, 0.0f, 1.0f, 0.5f, 0.5f, 0.0f}},
    {35.4f, 12.0f, 24.0f, 30,  60,   {0.5f, 0.0f, 0.0f, 0.2f, 0.2f, 0.2f, 0.0f, 0.2f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f}},
    {28.0f, 8.0f,  16.0f, 10,  40,   {0.0f, 0.5f, 0.5f, 0.2f, 0.2f, 0.2f, 0.0f, 0.2f, 0.0f, 0.0f, 0.5f, 0.5f, 1.0f}},
    {21.0f, 10.0f, 16.0f, 200, 400,  {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}},
    {20.0f, 6.0f,  12.0f, 15,  40,   {0.0f, 1.0f, 0.5f, 0.2f, 0.2f, 0.2f, 0.0f, 0.2f, 0.0f, 0.5f, 0.5f, 0.5f, 0.0f}},
    {20.0f, 6.0f,  10.0f, 20,  50,   {0.0f, 0.5f, 1.0f, 0.2f, 0.2f, 0.2f, 0.0f, 0.2f, 0.0f, 0.5f, 0.5f, 0.5f, 0.0f}},
    {1.0f,  1.0f,  2.0f,  1,   2,    {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}},
    {21.0f, 8.0f,  16.0f, 100, 200,  {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f}},
    {20.0f, 8.0f,  16.0f, 40,  80,   {0.0f, 0.5f, 0.5f, 0.2f, 0.2f, 0.2f, 0.0f, 0.2f, 0.0f, 0.5f, 1.0f, 0.5f, 0.5f}},
    {20.0f, 6.0f,  12.0f, 20,  40,   {1.0f, 0.5f, 0.5f, 0.2f, 0.2f, 0.2f, 0.0f, 0.2f, 0.0f, 0.0f, 0.5f, 0.5f, 0.0f}},
    {15.0f, 6.0f,  12.0f, 10,  30,   {1.0f, 0.5f, 0.5f, 0.2f, 0.2f, 0.2f, 0.0f, 0.2f, 0.0f, 0.0f, 0.5f, 0.5f, 0.0f}},
    {21.0f, 10.0f, 20.0f, 35,  70,   {0.0f, 0.0f, 0.0f, 0.2f, 0.2f, 0.2f, 0.0f, 0.2f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f}},
    {21.0f, 8.0f,  16.0f, 15,  40,   {0.0f, 0.0f, 0.0f, 0.2f, 0.2f, 0.2f, 0.0f, 0.2f, 0.0f, 1.0f, 0.5f, 0.5f, 0.0f}},
    {20.0f, 8.0f,  20.0f, 20,  40,   {0.0f, 0.0f, 0.0f, 0.2f, 0.2f, 0.2f, 0.0f, 0.2f, 0.0f, 0.5f, 1.0f, 0.5f, 0.0f}},
    {20.0f, 8.0f,  14.0f, 20,  40,   {0.0f, 0.0f, 0.0f, 0.2f, 0.2f, 0.2f, 0.0f, 0.2f, 0.0f, 0.5f, 0.5f, 1.0f, 1.0f}},
    {21.0f, 10.0f, 30.0f, 600, 1000, {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.5f, 0.0f, 0.0f, 0.0f}},
};

#include <libvu0.h>

#include <cmath>
#include <cstdlib>

#include "collision.hpp"
#include "mathutil.hpp"

int PickUpNearPoly(CCPoly *out, CBoxVu0 box, CCPoly *polys, int count);

int rand_check(float probability) {
    int   roll = rand();
    float threshold = probability * 1000.0f;
    (int) threshold;
    return roll % 1000 < (int) threshold ? 1 : 0;
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
    float   position[4];
    float   rotation[4];
    float   velocity[4];
    float   next_position[4];
    float   hit_position[4];
    CCPoly  nearby_polys[256];
    CBoxVu0 box;

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
        }
    }

    if (move_timer <= 0) {
        if (move_mode == FISH_MOVE_UNSET) {
            move_mode = FISH_MOVE_SWIM;
        } else {
            move_mode = (FishMoveMode) (rand() % 2);
        }

        move_timer = GetActCnt();
        target_speed = 0.0f;

        if (move_mode != FISH_MOVE_STOP) {
            target_yaw = TWO_PI * ((float) rand() / 2147483648.0f);
            target_yaw = AngleLimit(target_yaw);
            move_timer += 30;
            target_speed = 0.2f + (0.5f * (float) rand() / 2147483648.0f);
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
        target_yaw = food_yaw + 0.2f * ((float) rand() / 2147483648.0f);
        target_yaw = AngleLimit(target_yaw);
        target_speed = 0.0f;
    }

    if (action == FISH_ACTION_BATTLE) {
        target_yaw = TWO_PI * ((float) rand() / 2147483648.0f);
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

        if (speed > target_speed) {
            speed = target_speed;
        }
    }

    if (speed > target_speed) {
        speed -= speed_step;

        if (speed < target_speed) {
            speed = target_speed;
        }
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

    if (collision_polys != NULL) {
        box.max[0] = position[0] + 10.0f;
        box.max[1] = position[1] + 10.0f;
        box.max[2] = position[2];
        box.min[0] = position[0] - 10.0f;
        box.min[1] = position[1];
        box.min[2] = position[2] - 10.0f;
        int count = PickUpNearPoly(nearby_polys, box, collision_polys, collision_poly_count);

        if (CheckHit(nearby_polys, count, position, next_position, hit_position, 0, 0) < 0) {
            SetPosition(next_position);
        }

        GetPosition(position);
        CheckWidth(nearby_polys, count, position, 2.0f * angle_model_scale, next_position, 0);
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

int CFish::GetFP() {
    float points;

    if (size >= info.min_size) {
        points = info.min_fp + (info.max_fp - info.min_fp) * (size - info.min_size) / (info.max_size - info.min_size);
    } else {
        points = info.min_fp - 0.5f * (info.min_fp * (info.min_size - size) / (info.min_size - 0.5f * info.min_size));
    }

    return (int) points;
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
    if (model.frame != NULL) {
        model.frame->SetReference(reference);
    }

    if (angle_model.frame != NULL) {
        angle_model.frame->SetReference(reference);
    }
}

void CFish::DeleteReference() {
    if (model.frame != NULL) {
        model.frame->DeleteReference();
    }

    if (angle_model.frame != NULL) {
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
    collision_polys = NULL;
    collision_poly_count = 0;
    unk_78 = 0;
    target_yaw = 0.0f;
    unk_70 = 0;
    target_speed = 0.0f;
    speed = 0.0f;
    food_radius = -1.0f;
}

/* Nothing reads these: the link this file was built by removed them. They are here because
   the compiler writes a string constant where it first meets it, and the six the table in
   GetFishFileName shares with them stand ahead of the two LoadFish uses. */
static char *fish_file_01 = "chara/f01a.chr";
static char *fish_file_02 = "chara/f02a.chr";
static char *fish_file_03 = "chara/f03a.chr";
static char *fish_file_04 = "chara/f04a.chr";
static char *fish_file_05 = "chara/f05a.chr";
static char *fish_file_06 = "chara/f06a.chr";

void LoadFish(CFish *fish, int fish_kind, int texture_slot, CDataAlloc2<1> *alloc, int loaded_before) {
    if (fish_kind < 0 || fish_kind >= 18) {
        return;
    }

    LoadFile("chara/f00s.chr", read_buffer, NULL);
    fish->Initialize();
    fish->model.LoadPackData3(read_buffer, "info.cfg", alloc, texture_slot, alloc, loaded_before, 0);
    memcpy(&fish->info, &fish_info[fish_kind], sizeof(FishInfo));
    fish->SetScale();
    fish->fish_kind = fish_kind;
}

char *GetFishFileName(int fish_kind) {
    static char *name[18] = {
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

    if (fish_kind < 0 || fish_kind >= 18) {
        return NULL;
    }

    return name[fish_kind];
}
