/* Retail compiled this unit before anything had passed two doubles to a helper,
 * so only $a0 was marked read at a helper call and SetEvent's loop counter
 * could take $a1. See re/ai/compiler/leaked_state.md; without this the counter
 * lands in $a2. Ignored by mwcc, read by scripts/build/statefix.py. */
#pragma helper_mask_gpr 0x10
#pragma helper_mask_fpr 0x1000
#pragma name_counter 648

#include "character.hpp"

#include <libvu0.h>

#include <cstring>

#include "chararead.hpp"
#include "cloth.hpp"
#include "frame.hpp"
#include "gameutil.hpp"
#include "mathutil.hpp"
#include "mglib.hpp"
#include "snd.hpp"
#include "texture.hpp"

/* External */

/* sbss */
s32 CCharacter::MotionStopFlag;

/* data */

#include "textureanime.hpp"
#include "visualvu1.hpp"

void zcross(float z, float *from, float *to, float *out) {
    float dz;
    float t;
    float dy;
    float dx;

    // Where the line between the two points crosses the plane at z.
    dx = to[0] - from[0];
    dy = to[1] - from[1];
    dz = to[2] - from[2];
    t = (z - from[2]) / dz;
    out[0] = from[0] + t * dx;
    out[1] = from[1] + t * dy;
    out[2] = z;
    out[3] = 1.0f;
}

/**
 * Clips a triangle against the near plane, giving back the polygon that survives.
 *
 * @mangled scissior__FPA4_fPA4_fPA4_ff
 * @address 0x137440
 * @size 0xA38
 */
int scissior(float out[][4], float first[][4], float second[][4], float near_z) {
    float first_distance[4];
    float second_distance[4];
    int   count;
    int   first_front;
    int   second_front;
    int   first_behind;
    int   second_behind;
    int   first_crosses;
    int   pair_crosses;
    int   second_crosses;
    int   i;
    int   crossing_count;

    count = 0;
    first_front = 0;
    second_front = 0;
    first_behind = 0;
    second_behind = 0;
    first_crosses = 0;
    pair_crosses = 0;
    second_crosses = 0;

    for (i = 0; i < 3; i++) {
        first_distance[i] = first[i][2] - near_z;
        second_distance[i] = second[i][2] - near_z;

        if (!(first_distance[i] <= 0.0f)) {
            first_front++;
        } else {
            first_behind++;
        }

        if (!(second_distance[i] <= 0.0f)) {
            second_front++;
        } else {
            second_behind++;
        }
    }

    if (first_distance[0] * first_distance[1] < 0.0f) {
        first_crosses++;
    }

    if (first_distance[1] * first_distance[2] < 0.0f) {
        first_crosses++;
    }

    if (first_distance[2] * first_distance[0] < 0.0f) {
        first_crosses++;
    }

    if (second_distance[0] * second_distance[1] < 0.0f) {
        second_crosses++;
    }

    if (second_distance[1] * second_distance[2] < 0.0f) {
        second_crosses++;
    }

    if (second_distance[2] * second_distance[0] < 0.0f) {
        second_crosses++;
    }

    if (first_distance[0] * second_distance[0] < 0.0f) {
        pair_crosses++;
    }

    if (first_distance[1] * second_distance[1] < 0.0f) {
        pair_crosses++;
    }

    if (first_distance[2] * second_distance[2] < 0.0f) {
        pair_crosses++;
    }

    crossing_count = first_crosses + second_crosses + pair_crosses;

    if (crossing_count == 0) {
        for (int j = 0; j < 5; j++) {
            *(u_long128 *) out[j] = *(u_long128 *) first[0];
        }

        return 0;
    }

    if (crossing_count == 4 && pair_crosses == 0) {
        if (first_distance[0] * first_distance[1] < 0.0f) {
            zcross(near_z, first[0], first[1], out[count++]);
        }

        if (first_distance[1] * first_distance[2] < 0.0f) {
            zcross(near_z, first[1], first[2], out[count++]);
        }

        if (first_distance[2] * first_distance[0] < 0.0f) {
            zcross(near_z, first[2], first[0], out[count++]);
        }

        if (second_distance[0] * second_distance[2] < 0.0f) {
            zcross(near_z, second[0], second[2], out[count++]);
        }

        if (second_distance[2] * second_distance[1] < 0.0f) {
            zcross(near_z, second[2], second[1], out[count++]);
        }

        if (second_distance[1] * second_distance[0] < 0.0f) {
            zcross(near_z, second[1], second[0], out[count++]);
        }
    } else if (crossing_count == 3 || crossing_count == 4) {
        if (first_distance[0] * second_distance[0] < 0.0f) {
            zcross(near_z, first[0], second[0], out[count++]);
        }

        if (first_distance[0] * first_distance[1] < 0.0f) {
            zcross(near_z, first[0], first[1], out[count++]);
        }

        if (second_distance[0] * second_distance[1] < 0.0f) {
            zcross(near_z, second[0], second[1], out[count++]);
        }

        if (first_distance[1] * second_distance[1] < 0.0f) {
            zcross(near_z, first[1], second[1], out[count++]);
        }

        if (second_distance[1] * second_distance[2] < 0.0f) {
            zcross(near_z, second[1], second[2], out[count++]);
        }

        if (first_distance[1] * first_distance[2] < 0.0f) {
            zcross(near_z, first[1], first[2], out[count++]);
        }

        if (first_distance[2] * second_distance[2] < 0.0f) {
            zcross(near_z, first[2], second[2], out[count++]);
        }

        if (first_distance[2] * first_distance[0] < 0.0f) {
            zcross(near_z, first[2], first[0], out[count++]);
        }

        if (second_distance[2] * second_distance[0] < 0.0f) {
            zcross(near_z, second[2], second[0], out[count++]);
        }
    } else if (crossing_count == 5) {
        int vertex = 0;
        int on_second = 0;

        if (first_distance[0] * second_distance[0] < 0.0f) {
            zcross(near_z, first[0], second[0], out[count++]);
        } else if (first_distance[1] * second_distance[1] < 0.0f) {
            zcross(near_z, first[1], second[1], out[count++]);
            vertex = 1;
        } else if (first_distance[2] * second_distance[2] < 0.0f) {
            zcross(near_z, first[2], second[2], out[count++]);
            vertex = 2;
        }

        for (int side = 0; side < 3; side++) {
            int next = vertex + 1;

            if (next > 2) {
                next = 0;
            }

            if (on_second == 0) {
                if (first_distance[vertex] * first_distance[next] < 0.0f) {
                    zcross(near_z, first[vertex], first[next], out[count++]);
                    on_second = 0;
                }

                if (second_distance[vertex] * second_distance[next] < 0.0f) {
                    zcross(near_z, second[vertex], second[next], out[count++]);
                    on_second = 1;
                }
            } else {
                if (second_distance[vertex] * second_distance[next] < 0.0f) {
                    zcross(near_z, second[vertex], second[next], out[count++]);
                    on_second = 1;
                }

                if (first_distance[vertex] * first_distance[next] < 0.0f) {
                    zcross(near_z, first[vertex], first[next], out[count++]);
                    on_second = 0;
                }
            }

            vertex++;

            if (vertex > 2) {
                vertex = 0;
            }
        }
    } else {
        printf("%d*******\n", crossing_count);
    }

    if (count == 0) {
        for (int j = 0; j < 5; j++) {
            *(u_long128 *) out[j] = *(u_long128 *) first[0];
        }
    } else {
        for (int j = count; j < 5; j++) {
            *(u_long128 *) out[j] = *(u_long128 *) out[j - count];
        }
    }

    return count;
}

void CVisualVu1::SetMDTDataAddress(unsigned int *data) {
}

unsigned int *CVisualVu1::GetMDTDataAddress() {
    return NULL;
}

void CCharacter::ClearPointLight() {
    int i;

    for (i = 0; i < CHARA_POINT_LIGHT_MAX; i++) {
        this->point_light[i].used = 0;
        this->point_light[i].source_w = -1.0f;
    }
}

int CCharacter::SetPointLight(float *pos, float inner_range, float outer_range, float red, float green, float blue, float alpha) {
    int i;

    // The light goes into the first slot that is free.
    for (i = 0; i < CHARA_POINT_LIGHT_MAX; i++) {
        if (this->point_light[i].used == 0) {
            this->point_light[i].used = 1;
            sceVu0CopyVector(this->point_light[i].pos, pos);
            this->point_light[i].source_w = pos[3];
            this->point_light[i].inner_range = inner_range;
            this->point_light[i].outer_range = outer_range;
            this->point_light[i].colour[0] = red;
            this->point_light[i].colour[1] = green;
            this->point_light[i].colour[2] = blue;
            this->point_light[i].colour[3] = alpha;
            return 1;
        }
    }

    return 0;
}

void CCharacter::InitializeTexAnime(CTexAnimeData *data, int count) {
    this->tex_anime.Initialize(data, count);
}

void CCharacter::ClearTexAnime() {
    this->tex_anime.DisableAll();
}

void CCharacter::TextureAnime(int step) {
    this->tex_anime.TexAnime(step);
}

void CCharacter::TexAnimeOn(int no) {
    this->tex_anime.Enable(no);
}

void CCharacter::TexAnimeOff(int no) {
    this->tex_anime.Disable(no);
}

int CCharacter::SetFootSound(float left_frame, float right_frame, int motion_no) {
    CHARA_FOOT_SOUND *sound;
    int               i;

    // One sound for each foot, each in the first slot that is free.
    for (i = 0; i < CHARA_FOOT_SOUND_MAX; i++) {
        sound = &this->foot_sound[i];

        if ((float) sound->frame < 0.0f) {
            sound->frame = (int) left_frame;
            sound->foot = 0;
            sound->motion_no = motion_no;
            break;
        }
    }

    for (i = 0; i < CHARA_FOOT_SOUND_MAX; i++) {
        sound = &this->foot_sound[i];

        if ((float) sound->frame < 0.0f) {
            sound->frame = (int) right_frame;
            sound->foot = 1;
            sound->motion_no = motion_no;
            break;
        }
    }

    return 1;
}

void CCharacter::SetFootSoundID(int id) {
    this->foot_sound_id = id;
}

void CCharacter::FootSoundEnable(int on) {
    this->foot_sound_enable = on;
}

void CCharacter::EventEnable(int on) {
    this->event_enable = on;
}

void CCharacter::SetEvent(float frame, int kind, int no, int motion_no) {
    CHARA_EVENT *event;
    int          i;

    for (i = 0; i < CHARA_EVENT_MAX; i++) {
        event = &this->event[i];

        if ((float) event->frame < 0.0f) {
            event->frame = (int) frame;
            event->kind = kind;
            event->no = no;
            event->motion_no = motion_no;
            break;
        }
    }
}

void CCharacter::ClearEvent(int motion_no) {
    int i;

    // A motion below zero clears every event rather than one motion\'s own.
    for (i = 0; i < CHARA_EVENT_MAX; i++) {
        if (motion_no < 0 || this->event[i].motion_no == motion_no) {
            this->event[i].no = -1;
            this->event[i].frame = -1;
            this->event[i].unk_10 = 0;
        }
    }
}

void CCharacter::SetMotionCamera(CCamera *camera) {
    int i;

    for (i = 0; i < CHARA_MOTION_MAX; i++) {
        if (this->motion[i] != NULL) {
            this->motion[i]->state.camera = camera;
        }
    }
}

tagMOTION_TYPE *CCharacter::GetMotionParam(int motion_no, int *out_index, int *out_start, int *out_end, int *out_set) {
    int             index;
    int             i;
    tagMOTION_TYPE *found;

    index = -1;
    found = NULL;

    // The motion numbers of the sets run one after another, so the set that
    // holds the number is the one whose range covers it.
    for (i = 0; i < CHARA_MOTION_MAX; i++) {
        if (this->motion[i] != NULL) {
            if (this->motion_start[i] > motion_no) {
                continue;
            }

            if (!(motion_no < this->motion_end[i])) {
                continue;
            }

            index = motion_no - this->motion_start[i];
            found = this->motion[i];

            if (out_start != NULL) {
                *out_start = this->motion_start[i];
            }

            if (out_end != NULL) {
                *out_end = this->motion_end[i];
            }

            if (out_set != NULL) {
                *out_set = i;
            }

            break;
        }
    }

    if (index < 0) {
        return NULL;
    }

    *out_index = index;
    return found;
}

MOTION_INFO *CCharacter::GetMotionInfo(int motion_no) {
    tagMOTION_TYPE *motion;
    int             index;

    motion = GetMotionParam(motion_no, &index, NULL, NULL, NULL);

    if (motion == NULL) {
        return NULL;
    }

    if (motion->motion_info == NULL) {
        return NULL;
    }

    return &motion->motion_info[index];
}

float CCharacter::GetNowTime() {
    tagMOTION_TYPE *motion;
    int             index;

    motion = GetMotionParam(this->motion_no, &index, NULL, NULL, NULL);

    if (motion != NULL) {
        return motion->state.time;
    }

    return 0.0f;
}

void CCharacter::Step() {
    int             index;
    int             set_no;
    tagMOTION_TYPE *motion;
    float           old_time;
    float step;
    float             new_time;
    float             saved_speed;
    float             saved_blend_step;
    float             delta;
    float             abs_delta;
    MOTION_INFO      *motion_info;
    CHARA_FOOT_SOUND *sound;
    CHARA_EVENT      *event;
    int               foot_sound_enabled;
    sceVu0FVECTOR     position;

    this->motion_state = 0;

    if (this->motion_no < 0) {
        return;
    }

    index = -1;
    motion = GetMotionParam(this->motion_no, &index, NULL, NULL, &set_no);

    if (motion == NULL) {
        return;
    }

    if (motion->motion_info == NULL) {
        return;
    }

    old_time = motion->state.time;
    this->motion_state = 2;
    motion_info = &motion->motion_info[index];

    // The step the motion advances by this frame, once the character's own speed overrides it.
    step = motion_info->speed;

    if (this->motion_speed > 0.0f) {
        step = this->motion_speed;
    }

    if (motion_info != NULL) {
        if (old_time >= (float) motion_info->start && (float) motion_info->end - step - 0.01f <= old_time) {
            this->motion_state = 3;
        }
    }

    motion->state.motion_no = index;

    if (motion->state.motion_no != motion->state.playing_no) {
        if (this->motion_no == 1 || this->motion_no == 2) {
            motion->state.next_frame = NextMotionTime_GET_EX(motion->motion_info, &motion->state);
            motion->state.next_frame += 2;
        } else {
            motion->state.next_frame = motion->motion_info[index].start;
            this->motion_state = 1;
        }
    }

    saved_speed = motion->motion_info[index].speed;
    saved_blend_step = motion->state.blend_step;

    if (this->motion_speed > 0.0f) {
        motion->motion_info[index].speed = this->motion_speed;
    }

    if ((this->motion_flags & 1) || MotionStopFlag) {
        motion->motion_info[index].speed = 0.0f;
        motion->state.blend_step = 0.0f;
    }

    if (this->motion_flags & 2) {
        if (!(motion->state.time + step < (float) motion->motion_info[index].end)) {
            motion->motion_info[index].speed = 0.0f;
        }
    }

    if (this->motion_flags & 4) {
        motion->state.blend_step = 1.0f;
        motion->state.frame = motion->motion_info[index].start;
        motion->state.next_frame = motion->motion_info[index].start;
        motion->state.time = (float) motion->motion_info[index].start;
    }

    this->motion_flags &= ~4;
    SetMotionEX(this->frame, motion, motion->motion_info, &motion->state, motion->frame_info);

    if (motion->motion_info != NULL) {
        motion->motion_info[index].speed = saved_speed;
        motion->state.blend_step = saved_blend_step;
    }

    if (this->fade_out != 0) {
        this->fade[0] -= 0.08f;

        if (this->fade[0] < this->fade[3]) {
            this->fade[0] = this->fade[3];
        }
    } else {
        this->fade[0] += 0.08f;

        if (this->fade[0] > 1.0f) {
            this->fade[0] = 1.0f;
        }
    }

    if (this->ground_ambient_no >= 0 && this->ground_ambient_no < 2) {
        for (int j = 0; j < 3; j++) {
            if (this->ground_ambient[this->ground_ambient_no][j] > this->ambient_tint[j]) {
                this->ambient_tint[j] += 10.0f;

                if (this->ground_ambient[this->ground_ambient_no][j] < this->ambient_tint[j]) {
                    this->ambient_tint[j] = this->ground_ambient[this->ground_ambient_no][j];
                }
            }

            if (this->ground_ambient[this->ground_ambient_no][j] < this->ambient_tint[j]) {
                this->ambient_tint[j] -= 10.0f;

                if (this->ground_ambient[this->ground_ambient_no][j] > this->ambient_tint[j]) {
                    this->ambient_tint[j] = this->ground_ambient[this->ground_ambient_no][j];
                }
            }
        }
    } else {
        for (int j = 0; j < 3; j++) {
            this->ambient_tint[j] -= 10.0f;

            if (this->ambient_tint[j] < 0.0f) {
                this->ambient_tint[j] = 0.0f;
            }
        }
    }

    new_time = motion->state.time;
    GetWorldPosition(position);
    delta = old_time - new_time;
    abs_delta = delta < 0.0f ? -delta : delta;

    if (abs_delta < 1.0f) {
        for (int i = 0; i < CHARA_FOOT_SOUND_MAX && this->foot_sound_enable != 0 && this->foot_sound_id >= 0 && this->foot_sound_wait == 0; i++) {
            sound = &this->foot_sound[i];

            if ((float) sound->frame >= 0.0f && sound->motion_no == set_no && (float) sound->frame >= old_time && (float) sound->frame < new_time) {
                SndPlayFootSound(this->foot_sound_id, sound->foot, position);
                this->foot_sound_wait = 5;
            }
        }
    }

    delta = delta < 0.0f ? -delta : delta;

    if (delta < 1.0f) {
        for (int i = 0; i < CHARA_EVENT_MAX && this->event_enable != 0; i++) {
            event = &this->event[i];

            if ((float) event->frame < 0.0f || event->motion_no != set_no || (float) event->frame < old_time || (float) event->frame >= new_time) {
                continue;
            }

            switch (event->kind) {
                case CHARA_EVENT_FOOT_SOUND:
                    foot_sound_enabled = this->foot_sound_enable;

                    if (foot_sound_enabled != 0) {
                        SndPlayFootSound(this->foot_sound_id, event->no, position);
                    }

                    break;
                case CHARA_EVENT_SOUND_EFFECT:
                    SndSePlay(event->no, -1, 0);
                    break;
            }
        }
    }

    this->foot_sound_wait--;

    if (this->foot_sound_wait < 0) {
        this->foot_sound_wait = 0;
    }
}

void CCharacter::ShadowStep() {
    int             index;
    int             i;
    tagMOTION_TYPE *motion;
    tagMOTION_TYPE *shadow;
    int             motion_no;
    int             start;
    float           speed;

    motion_no = this->motion_no;

    if (motion_no < 0) {
        return;
    }

    index = -1;
    shadow = NULL;

    for (i = 0; i < CHARA_MOTION_MAX; i++) {
        if (this->motion[i] == NULL) {
            continue;
        }

        start = this->motion_start[i];

        if (start > motion_no) {
            continue;
        }

        if (motion_no >= this->motion_end[i]) {
            continue;
        }

        index = motion_no - start;
        shadow = this->shadow_motion[i];
        motion = this->motion[i];
    }

    if (shadow == NULL) {
        return;
    }

    if (index < 0) {
        return;
    }

    if (motion->motion_info == NULL || shadow->motion_info == NULL) {
        return;
    }

    speed = motion->motion_info[index].speed;

    if ((this->motion_flags & 1) || MotionStopFlag) {
        shadow->motion_info[index].speed = 0.0f;
    }

    if (this->motion_flags & 2) {
        // The shadow stops on the last frame rather than running past it.
        if (!(shadow->state.time + speed + 0.001f < (float) shadow->motion_info[index].end)) {
            shadow->motion_info[index].speed = 0.0f;
        }
    }

    if (this->shadow_frame == NULL) {
        return;
    }

    // The shadow plays the motion of the model, at the frame the model stands on.
    shadow->state.time = motion->state.time;
    shadow->state.next_frame = motion->state.next_frame;
    shadow->state.motion_no = motion->state.motion_no;
    shadow->state.blend = motion->state.blend;
    shadow->state.blend_step = motion->state.blend_step;
    shadow->state.next_frame = motion->state.next_frame;
    shadow->state.playing_no = motion->state.playing_no;
    shadow->state.blending = motion->state.blending;
    SetMotionEX(this->shadow_frame, shadow, shadow->motion_info, &shadow->state, shadow->frame_info);
    shadow->motion_info[index].speed = speed;
}

void CCharacter::ClothFloor(int floor) {
    int i;

    for (i = 0; i < CHARA_CLOTH_MAX; i++) {
        if (this->cloth[i] != NULL) {
            this->cloth[i]->floor_on = floor;
        }
    }
}

void CCharacter::SetPosition(float x, float y, float z) {
    CObject::SetPosition(x, y, z);

    if (this->frame != NULL) {
        this->frame->SetPosition(x, y, z);
    }

    if (this->shadow_frame != NULL) {
        this->shadow_frame->SetPosition(x, y, z);
    }
}

void CCharacter::GetWorldPosition(float *out_position) {
    GetPosition(out_position);
    out_position[3] = 1.0f;

    if (this->frame->parent != NULL) {
        this->frame->parent->GetWorldPosition(out_position, out_position);
    }
}

void CCharacter::GetPosition(float *out_position) {
    sceVu0CopyVector(out_position, this->pos);
}

void CCharacter::SetPosition(float *position) {
    CCharacter::SetPosition(position[0], position[1], position[2]);
}

void CCharacter::SetPosition(CVector3_f_ position) {
    CCharacter::SetPosition(position.x, position.y, position.z);
}

void CCharacter::SetRotation(float x, float y, float z) {
    CObject::SetRotation(x, y, z);

    if (this->frame != NULL) {
        this->frame->SetRotation(x, y, z);
    }

    if (this->shadow_frame != NULL) {
        this->shadow_frame->SetRotation(x, y, z);
    }
}

void CCharacter::SetRotation(float *rotation) {
    CCharacter::SetRotation(rotation[0], rotation[1], rotation[2]);
}

void CCharacter::SetRotation(CVector3_f_ rotation) {
    CCharacter::SetRotation(rotation.x, rotation.y, rotation.z);
}

void CCharacter::SetScale(float x, float y, float z) {
    CObject::SetScale(x, y, z);

    if (this->frame != NULL) {
        this->frame->SetScale(x, y, z);
    }

    if (this->shadow_frame != NULL) {
        this->shadow_frame->SetScale(x, y, z);
    }
}

void CCharacter::SetScale(float *scale) {
    CCharacter::SetScale(scale[0], scale[1], scale[2]);
}

void CCharacter::Draw() {
    sceVu0FVECTOR ambient;
    sceVu0FVECTOR saved_ambient;
    float         light_direction[4][4];
    float         light_colour[4][4];
    float         saved_light_colour[4][4];
    float         saved_light_direction[4][4];
    sceVu0FVECTOR pos;
    sceVu0FVECTOR direction;
    int           fading;
    int           light_slot;
    int           i;
    int           colour_no;
    int           cloth_no;
    float         distance;
    float         level;

    if (this->frame != NULL) {
        this->frame->SetPosition(this->pos[0], this->pos[1], this->pos[2]);
        this->frame->SetRotation(this->rotation.x, this->rotation.y, this->rotation.z);
        this->frame->SetScale(this->scale[0], this->scale[1], this->scale[2]);
    }

    fading = this->fade[0] < 1.0f;

    // The scene keeps the lights it had, so that the character can put them
    // back once it has drawn.
    MGGetAmbient(ambient);
    MGGetPLight(light_direction, light_colour);
    sceVu0CopyMatrix(saved_light_colour, light_colour);
    sceVu0CopyMatrix(saved_light_direction, light_direction);
    sceVu0CopyVector(saved_ambient, ambient);

    light_slot = 3;

    for (i = 0; i < CHARA_POINT_LIGHT_MAX; i++) {
        // Only a light_slot that the scene leaves free can take one of its own.
        if (saved_light_direction[0][light_slot] != 0.0f) {
            break;
        }

        if (saved_light_direction[1][light_slot] != 0.0f) {
            break;
        }

        if (saved_light_direction[2][light_slot] != 0.0f) {
            break;
        }

        if (this->point_light[i].used == 0) {
            continue;
        }

        GetPosition(pos);
        sceVu0SubVector(direction, this->point_light[i].pos, pos);
        distance = DistVector(direction);

        if (!(distance <= this->point_light[i].outer_range)) {
            continue;
        }

        sceVu0Normalize(direction, direction);
        light_direction[0][light_slot] = direction[0];
        light_direction[1][light_slot] = direction[1];
        light_direction[2][light_slot] = direction[2];

        // The light_slot gives its whole colour up to the inner range and fades
        // away over the rest.
        level = 1.0f;

        if (!(distance <= this->point_light[i].inner_range)) {
            level = level - (distance - this->point_light[i].inner_range) / (this->point_light[i].outer_range - this->point_light[i].inner_range);
        }

        light_colour[light_slot][0] = this->point_light[i].colour[0] * level;
        light_colour[light_slot][1] = this->point_light[i].colour[1] * level;
        light_colour[light_slot][2] = this->point_light[i].colour[2] * level;
        light_colour[light_slot][3] = this->point_light[i].colour[3];

        light_slot--;

        if (light_slot <= 2) {
            break;
        }
    }

    if (fading) {
        sceVu0ScaleVectorXYZ(ambient, ambient, this->fade[0]);

        for (colour_no = 0; colour_no < 4; colour_no++) {
            sceVu0ScaleVectorXYZ(light_colour[colour_no], light_colour[colour_no], this->fade[0]);
        }
    }

    ambient[0] += this->ambient_offset[0] + this->ambient_tint[0];
    ambient[1] += this->ambient_offset[1] + this->ambient_tint[1];
    ambient[2] += this->ambient_offset[2] + this->ambient_tint[2];

    if (!(this->ambient_offset[3] < 0.0f)) {
        ambient[3] = this->ambient_offset[3];
    }

    MGSetAmbient(ambient);
    MGSetPLight(light_direction, light_colour);
    MGDraw(this->frame);

    for (cloth_no = 0; cloth_no < CHARA_CLOTH_MAX; cloth_no++) {
        if (this->cloth[cloth_no] != NULL) {
            this->cloth[cloth_no]->Draw();
        }
    }

    MGSetAmbient(saved_ambient);
    MGSetPLight(saved_light_direction, saved_light_colour);
}

void CCharacter::DrawShadow() {
    sceVu0FVECTOR transform;

    if (this->shadow_frame == NULL) {
        return;
    }

    sceVu0FVECTOR light_direction = {0.0f, 1.0f, 0.0f, 0.0f};

    this->shadow_frame->SetScale(this->scale[0], this->scale[1], this->scale[2]);
    GetRotation(transform);

    CFrame *shadow = this->shadow_frame;
    shadow->SetRotation(transform[0], transform[1], transform[2]);
    GetPosition(transform);
    this->shadow_frame->SetPosition(transform);
    // The shadow lies below the feet rather than inside the model.
    transform[1] -= 12.8f;
    MGDrawShadowFast(this->shadow_frame, transform, light_direction);
}

void CCharacter::LoadPackData(unsigned int *pack, char *name, CDataAlloc2<1> *model_alloc, CDataAlloc2<1> *texture_alloc) {
    LoadPackData(pack, name, model_alloc, model_alloc, texture_alloc);
}

void CCharacter::LoadPackData(unsigned int *pack, char *name, CDataAlloc2<1> *model_alloc, CDataAlloc2<1> *motion_alloc, CDataAlloc2<1> *texture_alloc) {
    ReadInfo(this, pack, name, model_alloc, motion_alloc, texture_alloc, 0, NULL, 0, 0);
}

void CCharacter::LoadPackData2(unsigned int *pack, char *name, CDataAlloc2<1> *alloc, int texture_block_no, CDataAlloc2<1> *image_alloc, int visual_type) {
    ReadInfo(this, pack, name, alloc, alloc, alloc, texture_block_no, image_alloc, visual_type, 0);
}

void CCharacter::LoadPackData3(unsigned int *pack, char *name, CDataAlloc2<1> *alloc, int texture_block_no, CDataAlloc2<1> *image_alloc, int keep_textures, int visual_type) {
    ReadInfo(this, pack, name, alloc, alloc, alloc, texture_block_no, image_alloc, visual_type, keep_textures);
}

void CCharacter::DeleteExtendTexture(int block_no) {
    LOADTEXTURE_INFO2 info[2];

    if (this->config != NULL) {
        this->tex_anime.LoadCFGFile(this->config, this->config_size);
    }

    // A block that the character loaded textures into goes back to what the
    // character alone asks for.
    if (this->images[1] != 0 || this->images[2] != 0 || this->images[3] != 0) {
        info[0].block_no = block_no;
        info[0].mipmap = false;
        info[0].name = (char *) this->images[0];
        info[1].block_no = 0;
        info[1].mipmap = false;
        info[1].name = NULL;
        TexManager.DeleteTextureBlock(block_no);
        TexManager.LoadTextureBlockEX(block_no, info);
    }
}

void CCharacter::DeleteExtendMotion() {
    int i;

    // The first set is the one the character was loaded with; the rest come
    // from packs that a caller added, and go with them.
    for (i = 1; i < CHARA_MOTION_MAX; i++) {
        this->motion[i] = NULL;
        this->shadow_motion[i] = NULL;
        this->motion_start[i] = -1;
        this->motion_end[i] = -1;
    }
}

void CCharacter::Initialize() {
    CObject::Initialize(1.0f);
    this->frame = NULL;
    this->motion_no = 0;
    this->shadow_frame = NULL;
    this->motion_speed = -1.0f;
    this->frame_info = 0;
    this->shadow_frame_info = 0;
    this->motion_type.motion_info = NULL;
    this->unk_C6C = 0;
    this->motion_type.unk_78 = 0;
    this->motion_type.frame_info = NULL;
    this->motion_type.unk_6C = 0;
    this->motion_type.unk_68 = 0;
    this->motion_type.unk_70 = 0;
    this->motion_type.unk_74 = 0;
    this->shadow_motion_type = this->motion_type;
    this->shadow_motion_type.frame_info = NULL;
    this->motion_flags = 0;

    this->cloth = this->cloth_buf;

    for (int j = 0; j < CHARA_CLOTH_MAX; j++) {
        this->cloth[j] = NULL;
    }

    this->wind = 0;

    for (int i = 0; i < CHARA_MOTION_MAX; i++) {
        this->motion[i] = NULL;
        this->shadow_motion[i] = NULL;
        this->motion_start[i] = -1;
        this->motion_end[i] = -1;
        memset(this->motion_storage[i].storage, 0, sizeof(this->motion_storage[i].storage));
        memset(this->shadow_motion_storage[i].storage, 0, sizeof(this->shadow_motion_storage[i].storage));
    }

    this->body_width = 7.0f;
    this->body_height = 17.0f;
    this->body_depth = 60.0f;
    this->fade_out = 0;
    this->fade[0] = 1.0f;
    this->fade[1] = 0.0f;
    this->fade[2] = 0.0f;
    this->fade[3] = 0.7f;
    this->ambient_offset[0] = 0.0f;
    this->ambient_offset[1] = 0.0f;
    this->ambient_offset[2] = 0.0f;
    this->ambient_offset[3] = -1.0f;

    sceVu0FVECTOR zero = {0.0f, 0.0f, 0.0f, 0.0f};

    for (int i = 0; i < 2; i++) {
        sceVu0CopyVector(this->ground_ambient[i], zero);
    }

    sceVu0CopyVector(this->ambient_tint, zero);

    // The first set of motions is the one the model was loaded with.
    this->motion[0] = &this->motion_type;
    this->shadow_motion[0] = &this->shadow_motion_type;
    this->motion_start[0] = 0;
    this->motion_end[0] = 256;

    for (int j = 0; j < 16; j++) {
        this->unk_1068[j].unk_04 = -1.0f;
        this->unk_1068[j].unk_00 = -1;
        this->unk_1068[j].unk_0C = 0;
    }

    for (int j = 0; j < CHARA_FOOT_SOUND_MAX; j++) {
        this->foot_sound[j].foot = -1;
        this->foot_sound[j].frame = -1;
    }

    this->foot_sound_wait = 0;
    this->foot_sound_id = 0;
    this->foot_sound_enable = 1;
    ClearEvent(-1);
    this->event_enable = 1;
    this->motion_state = 0;
    this->config = NULL;
    ClearPointLight();
}

void CMainChara::Draw() {
    CCharacter::Draw();
}

void CMainChara::Initialize() {
    CCharacter::Initialize();
    this->unk_11B0 = 0;
    this->unk_11B4 = 0;
    this->unk_11B8 = 0;
    this->unk_11C0 = 0;
    this->unk_11C4 = 0;
    this->unk_11C8 = 0;
    this->unk_11CC = 0;
}
