#include "dungeoneventman.hpp"

#include <cmath>
#include <iterator>
#include <print>

#include "collisiondata.hpp"
#include "dun/gameloop.hpp"
#include "dungeonmap.hpp"
#include "frame.hpp"
#include "mathutil.hpp"

namespace dc::game {

namespace {

constexpr int kMapCellStride = 20;

bool IsActive(const CDungeonEventData &event_data) {
    return event_data.event != nullptr && event_data.enabled != 0;
}

float WrapQuarterTurns(float direction) {
    if (direction > 3.0f) {
        direction -= 3.0f;
    }

    if (direction == 3.0f) {
        direction = -1.0f;
    }

    return direction;
}

} // namespace

CDungeonEvent *CDungeonEventMan::SearchPartsID(int index, int parts_id) {
    const int slot_parts_id = slot[index].enabled != 0 ? slot[index].parts_id : -1;

    if (parts_id == slot_parts_id) {
        return &slot[index];
    }

    return nullptr;
}

CDungeonEvent *CDungeonEventMan::SearchSlot() {
    for (auto &definition : slot) {
        if (definition.enabled == 0) {
            return &definition;
        }
    }

    return nullptr;
}

void CDungeonEventMan::SearchDataSwitch(int script_no, int enable) {
    for (auto &event_data : event) {
        if (!IsActive(event_data) || script_no != event_data.event->script_no) {
            continue;
        }

        if (enable != 0) {
            event_data.Start();
        } else {
            event_data.Stop();
        }
    }
}

void CDungeonEventMan::SearchItemEventHold(int script_no) {
    for (auto &event_data : event) {
        if (IsActive(event_data) && script_no == event_data.event->script_no) {
            event_data.hold = 0;
        }
    }
}

int CDungeonEventMan::GetDataNum() {
    int count = 0;

    for (const auto &event_data : event) {
        if (!IsActive(event_data)) {
            count++;
        }
    }

    return count;
}

CDungeonEventData *CDungeonEventMan::SearchDataSlot() {
    for (auto &event_data : event) {
        if (!IsActive(event_data)) {
            return &event_data;
        }
    }

    return nullptr;
}

CDungeonEventData *CDungeonEventMan::CheckCollisionDataHit(int index) {
    const int chara_no = event[index].event->chara_no;
    const int key_id = event[index].event->key_id;

    if (chara_no == -1) {
        return nullptr;
    }

    for (int i = 0; i < std::ssize(NowColData->active); i++) {
        if (NowColData->active[i] == 0) {
            continue;
        }

        if (NowColData->hit[i].owner != chara_no) {
            continue;
        }

        if (key_id != -1 && NowColData->hit[i].attack_no != key_id) {
            continue;
        }

        sceVu0FVECTOR hit_position;
        sceVu0CopyVector(hit_position, NowColData->hit[i].pos);
        const float radius = event[index].event->radius;

        if (DistVector(hit_position, event[index].pos) <= radius) {
            return &event[index];
        }
    }

    return nullptr;
}

CDungeonEventData *CDungeonEventMan::SearchDataSlotPos(float *position) {
    for (int i = 0; i < std::ssize(event); i++) {
        CDungeonEventData &event_data = event[i];

        if (event_data.CheckSwitch() == 0) {
            continue;
        }

        const float radius = event_data.event->radius;

        if (DistVector(event_data.pos, position) <= radius && event_data.event->chara_no == -1) {
            return &event_data;
        }

        CDungeonEventData *collision_event = CheckCollisionDataHit(i);

        if (collision_event != nullptr) {
            return collision_event;
        }
    }

    return nullptr;
}

CDungeonEventData *CDungeonEventMan::SearchDataSlotPos2(float *position) {
    sceVu0FVECTOR event_position;
    sceVu0FVECTOR target_position;

    sceVu0CopyVector(target_position, position);
    const float target_height = target_position[1];
    target_position[1] = 0.0f;

    for (auto &event_data : event) {
        if (event_data.CheckSwitch() == 0) {
            continue;
        }

        sceVu0CopyVector(event_position, event_data.pos);
        float height_difference = target_height - event_position[1];
        height_difference = height_difference < 0.0f ? -height_difference : height_difference;

        if (height_difference < 40.0f) {
            event_position[1] = 0.0f;
            const float radius = event_data.event->radius;

            if (DistVector(event_position, target_position) <= radius + 10.0f) {
                return &event_data;
            }
        }
    }

    return nullptr;
}

void CDungeonEventMan::SetupEvent(CDungeonMap *map, int mode) {
    sceVu0FVECTOR local_origin = {0.0f, 0.0f, 0.0f, 0.0f};
    sceVu0FMATRIX matrix;

    if (mode == 1) {
        for (int row = 0; row < 16; row++) {
            for (int column = 0; column < 16; column++) {
                const MAP_CELL &cell = map->cells[column + row * kMapCellStride];
                const int       parts_id = cell.parts_no;

                if (parts_id == MAP_PARTS_NONE) {
                    continue;
                }

                for (int slot_no = 0; slot_no < std::ssize(slot); slot_no++) {
                    CDungeonEvent *definition = SearchPartsID(slot_no, parts_id);

                    if (definition == nullptr) {
                        continue;
                    }

                    CDungeonEventData *runtime_event = SearchDataSlot();

                    if (runtime_event == nullptr) {
                        std::println("** eventdata err ");

                        for (;;) {
                        }
                    }

                    runtime_event->Set(definition);
                    auto direction = static_cast<float>(cell.direction);
                    direction += static_cast<float>(cell.parts_no == MAP_PARTS_NONE ? 0 : map->parts[cell.parts_no].collision_turn);
                    direction = WrapQuarterTurns(direction);

                    direction = (PI * (-90.0f * direction)) / 180.0f;
                    definition->placement_frame->SetRotation(0.0f, direction, 0.0f);
                    definition->trigger_frame->GetLWMatrix(matrix);
                    runtime_event->dir[0] = 0.0f;
                    runtime_event->dir[1] = atan2f(matrix[2][0], matrix[2][2]);
                    runtime_event->dir[2] = 0.0f;
                    runtime_event->dir[3] = 1.0f;
                    definition->placement_frame->SetPosition(160.0f * static_cast<float>(column), 0.0f, 160.0f * static_cast<float>(row));
                    definition->trigger_frame->GetWorldPosition(runtime_event->pos, local_origin);
                }
            }
        }

        return;
    }

    for (int parts_id = 0; map->parts[parts_id].frame[0] != nullptr; parts_id++) {
        for (int slot_no = 0; slot_no < std::ssize(slot); slot_no++) {
            CDungeonEvent *definition = SearchPartsID(slot_no, parts_id);

            if (definition == nullptr) {
                continue;
            }

            CDungeonEventData *runtime_event = SearchDataSlot();
            runtime_event->Set(definition);
            float direction = map->parts[parts_id].frame_turn[0];
            direction += static_cast<float>(parts_id == MAP_PARTS_NONE ? 0 : map->parts[parts_id].collision_turn);
            direction = WrapQuarterTurns(direction);

            const float angle = (PI * (-90.0f * direction)) / 180.0f;
            definition->placement_frame->SetRotation(0.0f, angle, 0.0f);
            definition->trigger_frame->GetLWMatrix(matrix);
            runtime_event->dir[0] = 0.0f;
            runtime_event->dir[1] = atan2f(matrix[2][0], matrix[2][2]);
            runtime_event->dir[2] = 0.0f;
            runtime_event->dir[3] = 1.0f;
            definition->trigger_frame->GetWorldPosition(runtime_event->pos, local_origin);
        }
    }
}

} // namespace dc::game
