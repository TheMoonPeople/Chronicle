#include "shot_effect.hpp"

#include <cstdio>
#include <cstdlib>

#include "collisiondata.hpp"
#include "dataread.hpp"
#include "dun/gameloop.hpp"
#include "hit_machingun_effect.hpp"
#include "itembombeffect.hpp"
#include "itemdata.hpp"
#include "mathutil.hpp"
#include "mds.hpp"
#include "mglib.hpp"
#include "nowload.hpp"
#include "shot_effect_pack.hpp"
#include "shot_utils.hpp"
#include "snd.hpp"
#include "texture.hpp"

int GetWeaponElementAttr(int element);

/**
 * Draws the twelve projectiles of one shot.
 *
 * @mangled draw__5CSHOTFv
 * @address 0x1ABC40
 * @size 0xCC
 */
void CSHOT::draw() {
    for (int shot = 0; shot < 12; shot++) {
        if (used[shot] != 0) {
            int texture_cell = NowWeaponHave->item_no - 299;
            texture_cell--;

            if (texture_cell <= 0) {
                texture_cell = 0;
            }

            int x = (texture_cell % 4) << 5;
            int y = (texture_cell / 4) << 5;
            set3DCellModel(pos[shot], "basefx01", size[shot], x, y, 32, 32, 128);
        }
    }
}

/**
 * Advances the twelve projectiles of one shot.
 *
 * @mangled step__5CSHOTFv
 * @address 0x1ABD10
 * @size 0x204
 */
void CSHOT::step() {
    sceVu0FVECTOR hit_position;

    for (int shot = 0; shot < 12; shot++) {
        if (used[shot] == 0) {
            continue;
        }

        if (halted[shot] == 0) {
            SHOT_COLLISION_RESULT result = checkCollision(hit_position, pos[shot], vector[shot], 2, 2.0f);

            if (result == SHOT_COLLISION_NONE) {
                pos[shot][0] += vector[shot][0];
                pos[shot][1] += vector[shot][1];
                pos[shot][2] += vector[shot][2];
            } else {
                NowColData->Set(pos[shot], damage[shot], 1, 3.0f, 0.0f, 2, 2, 0, 0);
                NowColData->SetUserID(1, 0);
                s8              weapon_element = NowWeaponHave->best_elem;
                CCollisionData *attr_col = NowColData;
                attr_col->hit[attr_col->now_hit].flags = GetWeaponElementAttr(weapon_element);
                NowColData->hit[NowColData->now_hit].weapon_flags = NowWeaponHave->flags;
                NowColData->hit[NowColData->now_hit].vs_monster = NowWeaponHave->vs_monster;
                used[shot] = 0;
            }
        }

        life[shot]--;

        if (life[shot] <= 0) {
            used[shot] = 0;
        }
    }
}

void CSHOT_EFFECT::Draw() {
    if (effect_data == NULL) {
        return;
    }

    TexManager.ReloadTexture(Vif1Packet, texture_block);

    for (int slot = 0; slot < 8; slot++) {
        if (active[slot] != 0) {
            CCharacter   *model = &chara[slot];
            sceVu0FVECTOR position;
            sceVu0FVECTOR jittered;

            model->Step();

            if (!(random_rate[slot] < 0.0f)) {
                // Drawing temporarily offsets the model by a random amount on each axis.
                model->GetPosition(position);
                float spread = random_rate[slot];
                jittered[0] = position[0] + 2.0f * (spread * (float) rand()) / 2147483648.0f - spread;
                spread = random_rate[slot];
                jittered[1] = position[1] + 2.0f * (spread * (float) rand()) / 2147483648.0f - spread;
                spread = random_rate[slot];
                jittered[2] = position[2] + 2.0f * (spread * (float) rand()) / 2147483648.0f - spread;
                jittered[3] = 1.0f;
                model->SetPosition(jittered);
            }

            model->Draw();

            if (!(random_rate[slot] < 0.0f)) {
                model->SetPosition(position);
            }
        }
    }
}

void CSHOT_EFFECT::Step() {
    for (int slot = 0; slot < 8; slot++) {
        if (active[slot] == 0) {
            continue;
        }

        int          motion = effect_data->motion[phase[slot]];
        float        time = chara[slot].motion_type.state.time;
        MOTION_INFO *info = chara[slot].motion_type.motion_info;
        float        end = (float) info[motion].end;

        // A looping effect only steps its motion forward until it reaches the loop phase.
        if (loop[slot] != -1) {
            if (phase[slot] < loop[slot] && time >= end - 1.0f && time <= end) {
                phase[slot]++;
                chara[slot].SetMotion(effect_data->motion[phase[slot]], 4);
            }

            continue;
        }

        // The starting motion hands over to the flying one when it finishes.
        if (phase[slot] == 0 && time >= end - 1.0f && time <= end) {
            phase[slot]++;
            motion = effect_data->motion[phase[slot]];

            if (motion == -1) {
                active[slot] = 0;
                continue;
            }

            chara[slot].motion_type.state.time = (float) chara[slot].motion_type.motion_info[motion].start;
            chara[slot].SetMotion(effect_data->motion[phase[slot]], 4);
        }

        sceVu0FVECTOR hit_position;
        sceVu0FVECTOR position;
        chara[slot].GetPosition(position);
        chara[slot].GetPosition(hit_position);

        SHOT_COLLISION_RESULT result = SHOT_COLLISION_NONE;

        if (phase[slot] < 2) {
            result = checkCollision(hit_position, position, velocity[slot], effect_data->target, effect_data->radius[phase[slot]]);
        }

        if (life_time[slot] > 0) {
            life_time[slot]--;
        }

        if (!(effect_data->radius[phase[slot]] <= 0.0f) && life_time[slot] != 0) {
            // After each hit the effect waits its delay before it can hit again.
            if (wait_state[slot] <= 0) {
                int hit = NowColData->Set(hit_position, damage[slot], 2, effect_data->radius[phase[slot]], 1.0f, effect_data->target, effect_data->hit_kind, effect_data->hit_flags, 0);

                if (hit != -1) {
                    NowColData->SetUserID(user_id[slot], user_sub_id[slot]);
                    NowColData->hit[NowColData->now_hit].weapon_flags = weapon_status[slot];
                    NowColData->hit[NowColData->now_hit].vs_monster = vs_monster[slot];
                    NowColData->hit[NowColData->now_hit].monster_no = user_id_2[slot];
                    NowColData->hit[NowColData->now_hit].target_kind = enemy_attribute[slot];

                    // Hits of kind 3 throw along the flight, or at the player once it stops.
                    if (effect_data->hit_kind == 3) {
                        velocity[slot][3] = 1.0f;

                        if (effect_data->speed[phase[slot]] <= 0.0f) {
                            sceVu0FVECTOR player;
                            sceVu0FVECTOR direction;
                            sceVu0CopyVector(player, CharaMain.pos);
                            direction[0] = player[0] - position[0];
                            direction[1] = 0.0f;
                            direction[2] = player[2] - position[2];
                            direction[3] = 1.0f;
                            sceVu0Normalize(direction, direction);
                            NowColData->SetVelocity(hit, direction, 1.0f);
                        } else {
                            NowColData->SetVelocity(hit, velocity[slot], 1.0f);
                        }
                    }

                    wait_state[slot] = wait[slot];
                }
            } else {
                wait_state[slot]--;
            }
        }

        if (result == SHOT_COLLISION_NONE) {
            position[0] += velocity[slot][0];
            position[1] += velocity[slot][1];
            position[2] += velocity[slot][2];
            chara[slot].SetPosition(position);

            // A flying effect that runs out of time skips straight to its last phase.
            if (phase[slot] == 1 && phase_delay[slot] != -1) {
                phase_delay[slot]--;

                if (phase_delay[slot] == -1) {
                    phase[slot] += 2;
                    motion = effect_data->motion[phase[slot]];

                    if (motion == -1) {
                        active[slot] = 0;

                        switch (effect_data->end_effect) {
                            case SHOT_END_EFFECT_BOMB:
                                SetBombEffect(hit_position, effect_data->target, effect_data->bomb_damage, effect_data->bomb_scale);
                                break;
                        }

                        continue;
                    }

                    if (phase[slot] != -1) {
                        chara[slot].motion_type.state.time = (float) chara[slot].motion_type.motion_info[motion].start;
                        chara[slot].SetMotion(effect_data->motion[phase[slot]], 6);
                        sceVu0Normalize(velocity[slot], velocity[slot]);
                        sceVu0ScaleVectorXYZ(velocity[slot], velocity[slot], effect_data->speed[phase[slot]]);

                        if (effect_data->sound[phase[slot]] != -1 && no_sound[slot] == 0) {
                            SndSePlay(effect_data->sound[phase[slot]], -1, 0);
                        }
                    }
                }
            }
        } else if (phase[slot] == 1) {
            // A flying effect that hits something moves on to its impact phase.
            phase[slot]++;
            motion = effect_data->motion[phase[slot]];

            if (motion == -1) {
                active[slot] = 0;

                switch (effect_data->end_effect) {
                    case SHOT_END_EFFECT_BOMB:
                        SetBombEffect(hit_position, effect_data->target, effect_data->bomb_damage, effect_data->bomb_scale);
                        break;
                }

                continue;
            }

            if (motion != -1) {
                chara[slot].motion_type.state.time = (float) chara[slot].motion_type.motion_info[motion].start;
                chara[slot].SetMotion(effect_data->motion[phase[slot]], 6);
                sceVu0Normalize(velocity[slot], velocity[slot]);
                sceVu0ScaleVectorXYZ(velocity[slot], velocity[slot], effect_data->speed[phase[slot]]);

                if (effect_data->sound[phase[slot]] != -1 && no_sound[slot] == 0) {
                    SndSePlay(effect_data->sound[phase[slot]], -1, 0);
                }
            }
        }

        // The effect ends when the motion of its impact or last phase finishes.
        if ((phase[slot] == 2 || phase[slot] == 3) && chara[slot].motion_type.state.time >= end - 1.0f && chara[slot].motion_type.state.time < end) {
            active[slot] = 0;

            switch (effect_data->end_effect) {
                case SHOT_END_EFFECT_BOMB:
                    SetBombEffect(hit_position, effect_data->target, effect_data->bomb_damage, effect_data->bomb_scale);
                    break;
            }
        }
    }
}

void CSHOT_EFFECT::EndEffect() {
    for (int slot = 0; slot < 8; slot++) {
        if (active[slot] != 0 && (phase[slot] == 1 || phase[slot] == 0)) {
            phase[slot] = 2;
            int motion = effect_data->motion[phase[slot]];

            if (motion != -1) {
                chara[slot].motion_type.state.time = (float) chara[slot].motion_type.motion_info[motion].start;
                chara[slot].SetMotion(effect_data->motion[phase[slot]], 6);
            }
        }
    }
}

void CSHOT_EFFECT::OffEffect(s32 slot) {
    if (slot != -1) {
        active[slot] = 0;
        return;
    }

    for (s32 i = 0; i < 8; i++) {
        active[i] = 0;
    }
}

int CSHOT_EFFECT::Entry(BT_SHOT_EFFECT *description, unsigned int *pack, int tex_block, CDataAlloc2<1> *allocator, int slots) {
    char name[64];

    if (effect_data != NULL) {
        return 0;
    }

    sprintf(name, "dun/effect/%s.chr", description->model_name);
    LoadFile(name, pack, NULL);
    wait_now_loading_vsync();
    texture_block = tex_block;
    sprintf(name, "%s.cfg", description->model_name);

    template_chara.Initialize();
    template_chara.LoadPackData3(pack, name, allocator, texture_block, allocator, 1, 0x10);

    slot_count = slots;

    for (int slot = 0; slot < slots; slot++) {
        chara[slot] = template_chara;
        chara[slot].motion[0] = &chara[slot].motion_type;
        chara[slot].frame = (CFrame *) CopyFrameVu1((CFrameVu1 *) template_chara.frame, allocator);
    }

    effect_data = description;
    return effect_data == NULL ? 0 : 1;
}

int CSHOT_EFFECT::Entry2(BT_SHOT_EFFECT *description, unsigned int *pack, int tex_block, CDataAlloc2<1> *allocator, int slots) {
    char name[64];

    if (effect_data != NULL) {
        return 0;
    }

    sprintf(name, "%s.cfg", description->model_name);
    texture_block = tex_block;
    template_chara.Initialize();
    template_chara.LoadPackData3(pack, name, allocator, tex_block, allocator, 1, 0x10);

    slot_count = slots;

    for (int slot = 0; slot < slots; slot++) {
        chara[slot] = template_chara;
        chara[slot].motion[0] = &chara[slot].motion_type;
        chara[slot].frame = (CFrame *) CopyFrameVu1((CFrameVu1 *) template_chara.frame, allocator);
    }

    effect_data = description;
    return effect_data == NULL ? 0 : 1;
}

int CSHOT_EFFECT::ReEntry(BT_SHOT_EFFECT *description, CDataAlloc2<1> *allocator) {
    for (int slot = 0; slot < slot_count; slot++) {
        chara[slot] = template_chara;
        chara[slot].motion[0] = &chara[slot].motion_type;
        chara[slot].frame = (CFrame *) CopyFrameVu1((CFrameVu1 *) template_chara.frame, allocator);
    }

    effect_data = description;
    return 1;
}

void CSHOT_EFFECT::SetLoop(s32 loop) {
    s32 slot;

    slot = this->current_slot;

    if (slot != -1) {
        this->loop[slot] = loop;
    }
}

int CSHOT_EFFECT::Set(float *position, float *target, int owner, int sub_id, int source, CFrame *parent, int initial_phase) {
    int slot = -1;

    current_slot = -1;

    if (effect_data == NULL) {
        printf("******** shot err ***********\n");
        return;
    }

    for (int i = 0; i < slot_count; i++) {
        if (active[i] == 0) {
            slot = i;
            break;
        }
    }

    if (slot == -1) {
        return;
    }

    // An effect with no first-phase motion starts in its flying phase.
    int start_phase = 0;

    if (effect_data->motion[0] == -1) {
        start_phase++;
    }

    phase[slot] = start_phase;

    if (initial_phase != -1) {
        phase[slot] = initial_phase;
    }

    if (effect_data->stationary == 0) {
        position[3] = 1.0f;
        target[3] = 1.0f;
        sceVu0SubVector(velocity[slot], target, position);
        sceVu0Normalize(velocity[slot], velocity[slot]);

        if (parent == NULL) {
            if (effect_data->face_flight != 0) {
                sceVu0FMATRIX rotation;
                LookAtMatrixZ(rotation, velocity[slot]);
                chara[slot].frame->SetTransMatrix(rotation);
            }

            chara[slot].frame->DeleteReference();
        } else {
            chara[slot].frame->SetReference(parent);
            chara[slot].SetPosition(0.0f, 0.0f, 0.0f);
            chara[slot].SetRotation(0.0f, -HALF_PI, 0.0f);
        }

        sceVu0ScaleVectorXYZ(velocity[slot], velocity[slot], effect_data->speed[phase[slot]]);
    }

    if (phase[slot] == 0) {
        chara[slot].SetMotion(effect_data->motion[0], 6);
        chara[slot].motion_type.state.time = (float) chara[slot].motion_type.motion_info[effect_data->motion[0]].start;
    } else {
        chara[slot].SetMotion(effect_data->motion[1], 4);
        chara[slot].motion_type.state.time = (float) chara[slot].motion_type.motion_info[effect_data->motion[1]].start;
    }

    if (parent == NULL) {
        chara[slot].SetPosition(position);
    }

    damage[slot] = effect_data->damage;
    phase_delay[slot] = effect_data->life_time;
    active[slot] = 1;
    user_id[slot] = owner;
    user_sub_id[slot] = sub_id;
    weapon_status[slot] = 1;
    source_id[slot] = source;
    loop[slot] = -1;
    random_rate[slot] = -1.0f;
    life_time[slot] = -1;
    status = 0;
    current_slot = slot;
    user_id_2[slot] = -1;
    enemy_attribute[slot] = -1;
    no_sound[slot] = 0;
    wait[slot] = 0;
    wait_state[slot] = 0;
    return slot;
}

void CSHOT_EFFECT::SetWait(s32 wait) {
    s32 slot;

    slot = this->current_slot;

    if (slot != -1) {
        this->wait[slot] = (u8) wait;
        this->wait_state[this->current_slot] = 0;
    }
}

void CSHOT_EFFECT::SetNoSound() {
    s32 slot;

    slot = this->current_slot;

    if (slot != -1) {
        this->no_sound[slot] = 1;
    }
}

void CSHOT_EFFECT::SetRandomRate(float rate) {
    s32 slot;

    slot = this->current_slot;

    if (slot != -1) {
        this->random_rate[slot] = rate;
    }
}

void CSHOT_EFFECT::SetLifeTime(s32 life_time) {
    s32 slot;

    slot = this->current_slot;

    if (slot != -1) {
        this->life_time[slot] = life_time;
    }
}

void CSHOT_EFFECT::SetEnemyAttr(s32 attribute) {
    s32 slot;

    slot = this->current_slot;

    if (slot != -1) {
        this->enemy_attribute[slot] = attribute;
    }
}

void CSHOT_EFFECT::SetDmg(s32 damage) {
    s32 slot;

    slot = this->current_slot;

    if (slot != -1) {
        this->damage[slot] = damage;
    }
}

void CSHOT_EFFECT::SetAttribute(s32 attribute) {
    if (this->current_slot != -1) {
        this->effect_data->hit_flags = attribute;
    }
}

void CSHOT_EFFECT::SetWepStatus(s32 status) {
    s32 slot;

    slot = this->current_slot;

    if (slot != -1) {
        this->weapon_status[slot] = status;
    }
}

void CSHOT_EFFECT::SetVsMonster(s8 *effectiveness) {
    s32 slot;

    slot = this->current_slot;

    if (slot != -1) {
        this->vs_monster[slot] = effectiveness;
    }
}

void CSHOT_EFFECT::SetUserID2(s32 id) {
    s32 slot;

    slot = this->current_slot;

    if (slot != -1) {
        this->user_id_2[slot] = (s16) id;
    }
}

void CSHOT_EFFECT::Initialize() {
    effect_data = NULL;

    for (s32 i = 0; i < 8; i++) {
        active[i] = 0;
        user_id[i] = -1;
        user_sub_id[i] = -1;
        slot_count = 4;
    }

    current_slot = -1;
}

int CSHOT_EFFECT_PACK::Entry(BT_SHOT_EFFECT *description, unsigned int *resource, int texture_block, CDataAlloc2<1> *allocator, int slots) {
    for (int index = 0; index < 5; index++) {
        if (effect[index].effect_data == description) {
            return index;
        }
    }

    for (int index = 0; index < 5; index++) {
        if (effect[index].Entry(description, resource, texture_block, allocator, slots) != 0) {
            return index;
        }
    }

    return -1;
}

void CSHOT_EFFECT_PACK::SetUserID2(s32 id) {
    if (current_effect != -1) {
        effect[current_effect].SetUserID2(id);
    }
}

void CSHOT_EFFECT_PACK::SetDmg(s32 damage) {
    if (current_effect != -1) {
        effect[current_effect].SetDmg(damage);
    }
}

/**
 * Starts one rapid-fire projectile from a position along a heading.
 *
 * @mangled Set__15CSHOT_MACHINGUNFPfPfii
 * @address 0x1AE660
 * @size 0xEC
 */
int CSHOT_MACHINGUN::Set(float *origin, float *direction, int damage, int element) {
    int slot = -1;

    for (int i = 0; i < 16; i++) {
        if (this->age[i] == 0) {
            slot = i;
            break;
        }
    }

    if (slot == -1) {
        return -1;
    }

    sceVu0CopyVector(this->position[slot], origin);
    sceVu0CopyVector(this->velocity[slot], direction);
    this->damage[slot] = damage;
    this->element[slot] = element;
    this->age[slot] = 1;
    return slot;
}

/**
 * Advances the sixteen rapid-fire projectiles.
 *
 * @mangled Step__15CSHOT_MACHINGUNFv
 * @address 0x1AE750
 * @size 0x230
 */
void CSHOT_MACHINGUN::Step() {
    int                   slot;
    SHOT_COLLISION_RESULT result;
    float                *pos;
    int                   weapon_element;
    CCollisionData       *col;

    for (slot = 0; slot < 16; slot++) {
        if (age[slot] <= 0) {
            continue;
        }

        age[slot]++;

        if (age[slot] >= 240) {
            age[slot] = 0;
            continue;
        }

        pos = position[slot];
        result = checkCollision(pos, pos, velocity[slot], 2, 2.0f);

        if (result == SHOT_COLLISION_MAP) {
            OzumondShotEffect.Set(pos);
            age[slot] = 0;
        }

        if (result == SHOT_COLLISION_MONSTER) {
            NowColData->Set(pos, damage[slot], 2, 4.0f, 1.0f, 2, 2, 0, 0);
            weapon_element = NowWeaponHave->best_elem;
            col = NowColData;
            col->hit[col->now_hit].flags = GetWeaponElementAttr(weapon_element);
            NowColData->hit[NowColData->now_hit].vs_monster = NowWeaponHave->vs_monster;
            NowColData->hit[NowColData->now_hit].weapon_flags = NowWeaponHave->flags;
            CCollisionData *target = NowColData;
            target->hit[target->now_hit].owner = 5;
            target->hit[target->now_hit].attack_no = 6;
            age[slot] = 0;
        }

        pos[0] += velocity[slot][0];
        position[slot][1] += velocity[slot][1];
        position[slot][2] += velocity[slot][2];
    }
}
