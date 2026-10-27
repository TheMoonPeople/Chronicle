#include "stealitem.hpp"

#include <cmath>

#include "character.hpp"
#include "dun/gameloop.hpp"
#include "frame.hpp"
#include "framevu1.hpp"
#include "mathutil.hpp"
#include "mglib.hpp"

namespace dc::game {

void CStealItem::Initialize(CFrameVu1 *model) {
    this->frame = model;

    for (int i = 0; i < STEAL_ITEM_MAX; i++) {
        this->state[i] = StealItemState::None;
        this->aux_state[i] = -1;
    }
}

void CStealItem::Set(float *position, int item_no) {
    int slot = -1;

    for (int i = 0; i < STEAL_ITEM_MAX; i++) {
        if (this->state[i] == StealItemState::None) {
            slot = i;
            break;
        }
    }

    if (slot == -1) {
        return;
    }

    sceVu0CopyVector(this->pos[slot], position);
    this->base_height[slot] = position[1];
    this->state[slot] = StealItemState::Rise;
    this->speed[slot] = 0.5f;
    this->phase[slot] = 0.0f;
    this->item[slot] = item_no;
    this->angle = 0.0f;
}

void CStealItem::Step() {
    sceVu0FVECTOR target;

    sceVu0CopyVector(target, CharaMain.pos);
    target[1] = 15.0f;

    this->angle += 0.31415927f;

    if (this->angle >= TWO_PI) {
        this->angle -= TWO_PI;
    }

    auto move_towards_target = [&](int slot) {
        sceVu0FVECTOR direction;
        direction[0] = target[0] - this->pos[slot][0];
        direction[1] = target[1] - this->pos[slot][1];
        direction[2] = target[2] - this->pos[slot][2];
        direction[3] = 1.0f;
        sceVu0Normalize(direction, direction);
        sceVu0ScaleVectorXYZ(direction, direction, this->speed[slot]);
        this->pos[slot][0] += direction[0];
        this->pos[slot][1] += direction[1];
        this->pos[slot][2] += direction[2];
    };

    for (int i = 0; i < STEAL_ITEM_MAX; i++) {
        switch (this->state[i]) {
            case StealItemState::Rise:
                this->speed[i] += 0.05f;
                this->pos[i][1] = this->base_height[i] + 12.0f * sinf(this->phase[i]);
                this->phase[i] += 0.034906585f;
                move_towards_target(i);

                if (this->phase[i] >= HALF_PI) {
                    this->state[i] = StealItemState::Home;
                }

                break;

            case StealItemState::Home:
                if (DistVector(target, this->pos[i]) <= 5.0f) {
                    this->state[i] = StealItemState::Arrived;
                    break;
                }

                this->speed[i] += 0.02f;
                move_towards_target(i);
                break;

            case StealItemState::Arrived:
            case StealItemState::None:
                break;
        }
    }
}

void CStealItem::Draw() {
    for (int i = 0; i < STEAL_ITEM_MAX; i++) {
        if (this->state[i] == StealItemState::None) {
            continue;
        }

        this->frame->SetPosition(this->pos[i]);
        this->frame->SetRotation(this->angle, 0.0f, 0.0f);
        MGDraw(this->frame);
    }
}

int CStealItem::checkEvent() {
    for (int i = 0; i < STEAL_ITEM_MAX; i++) {
        if (this->state[i] == StealItemState::Arrived) {
            this->state[i] = StealItemState::None;
            return this->item[i];
        }
    }

    return -1;
}

} // namespace dc::game
