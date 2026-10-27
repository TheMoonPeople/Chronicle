#include "dranmapfield.hpp"

#include <print>

#include "boxvu0.hpp"
#include "collision.hpp"
#include "dataalloc.hpp"
#include "dungeonparts.hpp"
#include "frame.hpp"
#include "framevu1.hpp"
#include "mds.hpp"
#include "snd.hpp"

namespace dc::game {

namespace {

constexpr int MaxDranFields = 12;

// Every CDranMapField method runs on DRAN_MAP_FIELD_SET::field, so `this` is the start of the set.
DRAN_MAP_FIELD_SET *FieldSet(CDranMapField *field) {
    return reinterpret_cast<DRAN_MAP_FIELD_SET *>(field);
}

} // namespace

void CDranMapField::LoadModel(unsigned int *pack, CDataAlloc2<1> *arena) {
    DRAN_MAP_FIELD_SET *set = FieldSet(this);

    if (set->field_count < MaxDranFields) {
        CCharacter &character = set->field[set->field_count].character;
        character.Initialize();
        character.LoadPackData(pack, info_cfg_literal, arena, arena);
        character.SetPosition(0.0f, 0.0f, 0.0f);
        character.SetRotation(0.0f, 0.0f, 0.0f);
        set->field_count++;
    } else {
        std::print(" ************* over!!\n");
    }
}

int CDranMapField::AddCollision(CCPoly *poly, int count, CBoxVu0 box) {
    DRAN_MAP_FIELD_SET *set = FieldSet(this);

    if (set->collision_count == 0) {
        return count;
    }

    for (int i = 0; i < set->collision_count; i++) {
        if (set->collision[i] != nullptr && set->state[i] > 1) {
            count += set->collision[i]->PickUpNearPoly(&poly[count], box);
        }
    }

    return count;
}

void CDranMapField::LoadCollision(unsigned int *pack, CDataAlloc2<1> *arena) {
    DRAN_MAP_FIELD_SET *set = FieldSet(this);

    if (set->collision_count < MaxDranFields) {
        CFrame *collision = LoadCollisionFile(pack, arena);
        set->collision[set->collision_count] = collision;
        collision->SetPosition(0.0f, 0.0f, 0.0f);
        collision->SetRotation(0.0f, 0.0f, 0.0f);
        set->collision_count++;
    } else {
        std::print(" ************* over!!\n");
    }
}

void CDranMapField::Draw() {
    DRAN_MAP_FIELD_SET *set = FieldSet(this);

    for (int i = 0; i < set->field_count; i++) {
        if (set->field[i].character.frame != nullptr && set->state[i] != 0) {
            set->field[i].character.Draw();
        }
    }
}

void CDranMapField::Step() {
    DRAN_MAP_FIELD_SET *set = FieldSet(this);

    for (int i = 0; i < set->field_count; i++) {
        CCharacter &character = set->field[i].character;
        if (character.frame == nullptr || set->state[i] <= 0) {
            continue;
        }

        // Two starts the model moving, and the motion runs while it stands at one.
        if (set->state[i] == 2) {
            SndSePlay(SE_DRAN_FIELD_START, -1, 0);
            set->state[i]--;
        }

        if (set->state[i] == 1) {
            character.Step();

            if (!(character.GetNowTime() < 59.0f)) {
                set->state[i]--;
            }
        }
    }
}

} // namespace dc::game
