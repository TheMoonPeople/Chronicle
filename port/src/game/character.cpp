#include "character.hpp"

#include <libvu0.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <print>
#include <span>

#include "chararead.hpp"
#include "cloth.hpp"
#include "frame.hpp"
#include "gameutil.hpp"
#include "mathutil.hpp"
#include "mglib.hpp"
#include "platform/arena.hpp"
#include "snd.hpp"
#include "texture.hpp"
#include "textureanime.hpp"
#include "visualvu1.hpp"

namespace dc::game {

s32 CCharacter::MotionStopFlag;

namespace {

// Where the line between the two points crosses the plane at z.
void zcross(float z, const float *from, const float *to, float *out) {
    const float dx = to[0] - from[0];
    const float dy = to[1] - from[1];
    const float dz = to[2] - from[2];
    const float t = (z - from[2]) / dz;
    out[0] = from[0] + t * dx;
    out[1] = from[1] + t * dy;
    out[2] = z;
    out[3] = 1.0f;
}

} // namespace

int scissior(float out[][4], float first[][4], float second[][4], float near_z) {
    std::array<float, 3> first_distance{};
    std::array<float, 3> second_distance{};
    int                  count = 0;
    int                  first_crosses = 0;
    int                  pair_crosses = 0;
    int                  second_crosses = 0;

    for (int i = 0; i < 3; i++) {
        first_distance[i] = first[i][2] - near_z;
        second_distance[i] = second[i][2] - near_z;
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

    const int crossing_count = first_crosses + second_crosses + pair_crosses;

    if (crossing_count == 0) {
        for (int j = 0; j < 5; j++) {
            std::memcpy(out[j], first[0], sizeof(out[j]));
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
        std::println("{}*******", crossing_count);
    }

    if (count == 0) {
        for (int j = 0; j < 5; j++) {
            std::memcpy(out[j], first[0], sizeof(out[j]));
        }
    } else {
        for (int j = count; j < 5; j++) {
            std::memcpy(out[j], out[j - count], sizeof(out[j]));
        }
    }

    return count;
}

void CVisualVu1::SetMDTDataAddress(unsigned int * /*data*/) {}

unsigned int *CVisualVu1::GetMDTDataAddress() {
    return nullptr;
}

void CCharacter::ClearPointLight() {
    for (CFakePointLight &light : this->point_light) {
        light.used = 0;
        light.source_w = -1.0f;
    }
}

int CCharacter::SetPointLight(float *pos, float inner_range, float outer_range, float red, float green, float blue, float alpha) {
    // The light goes into the first slot that is free.
    for (CFakePointLight &light : this->point_light) {
        if (light.used == 0) {
            light.used = 1;
            sceVu0CopyVector(light.pos, pos);
            light.source_w = pos[3];
            light.inner_range = inner_range;
            light.outer_range = outer_range;
            light.colour[0] = red;
            light.colour[1] = green;
            light.colour[2] = blue;
            light.colour[3] = alpha;
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
    // One sound for each foot, each in the first slot that is free.
    for (CHARA_FOOT_SOUND &sound : this->foot_sound) {
        if (static_cast<float>(sound.frame) < 0.0f) {
            sound.frame = static_cast<int>(left_frame);
            sound.foot = 0;
            sound.motion_no = motion_no;
            break;
        }
    }

    for (CHARA_FOOT_SOUND &sound : this->foot_sound) {
        if (static_cast<float>(sound.frame) < 0.0f) {
            sound.frame = static_cast<int>(right_frame);
            sound.foot = 1;
            sound.motion_no = motion_no;
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
    for (CHARA_EVENT &slot : this->event) {
        if (static_cast<float>(slot.frame) < 0.0f) {
            slot.frame = static_cast<int>(frame);
            slot.kind = kind;
            slot.no = no;
            slot.motion_no = motion_no;
            break;
        }
    }
}

void CCharacter::ClearEvent(int motion_no) {
    // A motion below zero clears every event rather than one motion's own.
    for (CHARA_EVENT &slot : this->event) {
        if (motion_no < 0 || slot.motion_no == motion_no) {
            slot.no = -1;
            slot.frame = -1;
            slot.unk_10 = 0;
        }
    }
}

void CCharacter::SetMotionCamera(CCamera *camera) {
    for (tagMOTION_TYPE *set : this->motion) {
        if (set != nullptr) {
            set->state.camera = camera;
        }
    }
}

tagMOTION_TYPE *CCharacter::GetMotionParam(int motion_no, int *out_index, int *out_start, int *out_end, int *out_set) {
    int             index = -1;
    tagMOTION_TYPE *found = nullptr;

    // The motion numbers of the sets run one after another, so the set that
    // holds the number is the one whose range covers it.
    for (int i = 0; i < CHARA_MOTION_MAX; i++) {
        if (this->motion[i] != nullptr) {
            if (this->motion_start[i] > motion_no) {
                continue;
            }

            if (!(motion_no < this->motion_end[i])) {
                continue;
            }

            index = motion_no - this->motion_start[i];
            found = this->motion[i];

            if (out_start != nullptr) {
                *out_start = this->motion_start[i];
            }

            if (out_end != nullptr) {
                *out_end = this->motion_end[i];
            }

            if (out_set != nullptr) {
                *out_set = i;
            }

            break;
        }
    }

    if (index < 0) {
        return nullptr;
    }

    *out_index = index;
    return found;
}

MOTION_INFO *CCharacter::GetMotionInfo(int motion_no) {
    int             index = 0;
    tagMOTION_TYPE *motion = GetMotionParam(motion_no, &index, nullptr, nullptr, nullptr);

    if (motion == nullptr) {
        return nullptr;
    }

    if (motion->motion_info == nullptr) {
        return nullptr;
    }

    return &motion->motion_info[index];
}

float CCharacter::GetNowTime() {
    int                   index = 0;
    const tagMOTION_TYPE *motion = GetMotionParam(this->motion_no, &index, nullptr, nullptr, nullptr);

    if (motion != nullptr) {
        return motion->state.time;
    }

    return 0.0f;
}

void CCharacter::Step() {
    this->motion_state = 0;

    if (this->motion_no < 0) {
        return;
    }

    int             index = -1;
    int             set_no = 0;
    tagMOTION_TYPE *motion = GetMotionParam(this->motion_no, &index, nullptr, nullptr, &set_no);

    if (motion == nullptr) {
        return;
    }

    if (motion->motion_info == nullptr) {
        return;
    }

    const float old_time = motion->state.time;
    this->motion_state = 2;
    const MOTION_INFO *motion_info = &motion->motion_info[index];

    // The step the motion advances by this frame, once the character's own speed overrides it.
    float step = motion_info->speed;

    if (this->motion_speed > 0.0f) {
        step = this->motion_speed;
    }

    if (motion_info != nullptr && old_time >= static_cast<float>(motion_info->start) && static_cast<float>(motion_info->end) - step - 0.01f <= old_time) {
        this->motion_state = 3;
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

    const float saved_speed = motion->motion_info[index].speed;
    const float saved_blend_step = motion->state.blend_step;

    if (this->motion_speed > 0.0f) {
        motion->motion_info[index].speed = this->motion_speed;
    }

    if ((this->motion_flags & 1) || MotionStopFlag) {
        motion->motion_info[index].speed = 0.0f;
        motion->state.blend_step = 0.0f;
    }

    if ((this->motion_flags & 2) != 0 && !(motion->state.time + step < static_cast<float>(motion->motion_info[index].end))) {
        motion->motion_info[index].speed = 0.0f;
    }

    if (this->motion_flags & 4) {
        motion->state.blend_step = 1.0f;
        motion->state.frame = motion->motion_info[index].start;
        motion->state.next_frame = motion->motion_info[index].start;
        motion->state.time = static_cast<float>(motion->motion_info[index].start);
    }

    this->motion_flags &= ~4;
    SetMotionEX(this->frame, motion, motion->motion_info, &motion->state, motion->frame_info);

    if (motion->motion_info != nullptr) {
        motion->motion_info[index].speed = saved_speed;
        motion->state.blend_step = saved_blend_step;
    }

    if (this->fade_out != 0) {
        this->fade[0] -= 0.08f;
        this->fade[0] = std::max(this->fade[0], this->fade[3]);
    } else {
        this->fade[0] += 0.08f;
        this->fade[0] = std::min(this->fade[0], 1.0f);
    }

    if (this->ground_ambient_no >= 0 && this->ground_ambient_no < 2) {
        const float *target = this->ground_ambient[this->ground_ambient_no];

        for (int j = 0; j < 3; j++) {
            if (target[j] > this->ambient_tint[j]) {
                this->ambient_tint[j] += 10.0f;
                this->ambient_tint[j] = std::min(this->ambient_tint[j], target[j]);
            }

            if (target[j] < this->ambient_tint[j]) {
                this->ambient_tint[j] -= 10.0f;
                this->ambient_tint[j] = std::max(this->ambient_tint[j], target[j]);
            }
        }
    } else {
        for (int j = 0; j < 3; j++) {
            this->ambient_tint[j] -= 10.0f;
            this->ambient_tint[j] = std::max(this->ambient_tint[j], 0.0f);
        }
    }

    const float   new_time = motion->state.time;
    sceVu0FVECTOR position;
    GetWorldPosition(position);
    float       delta = old_time - new_time;
    const float abs_delta = delta < 0.0f ? -delta : delta;

    if (abs_delta < 1.0f) {
        for (int i = 0; i < CHARA_FOOT_SOUND_MAX && this->foot_sound_enable != 0 && this->foot_sound_id >= 0 && this->foot_sound_wait == 0; i++) {
            const CHARA_FOOT_SOUND *sound = &this->foot_sound[i];

            if (static_cast<float>(sound->frame) >= 0.0f && sound->motion_no == set_no && static_cast<float>(sound->frame) >= old_time && static_cast<float>(sound->frame) < new_time) {
                SndPlayFootSound(this->foot_sound_id, sound->foot, position);
                this->foot_sound_wait = 5;
            }
        }
    }

    delta = delta < 0.0f ? -delta : delta;

    if (delta < 1.0f) {
        for (int i = 0; i < CHARA_EVENT_MAX && this->event_enable != 0; i++) {
            const CHARA_EVENT *event = &this->event[i];

            if (static_cast<float>(event->frame) < 0.0f || event->motion_no != set_no || static_cast<float>(event->frame) < old_time || static_cast<float>(event->frame) >= new_time) {
                continue;
            }

            switch (static_cast<CharaEventKind>(event->kind)) {
                case CharaEventKind::FootSound:
                    if (this->foot_sound_enable != 0) {
                        SndPlayFootSound(this->foot_sound_id, event->no, position);
                    }

                    break;
                case CharaEventKind::SoundEffect:
                    SndSePlay(event->no, -1, 0);
                    break;
                default:
                    break;
            }
        }
    }

    this->foot_sound_wait--;
    this->foot_sound_wait = std::max(this->foot_sound_wait, 0);
}

void CCharacter::ShadowStep() {
    const int motion_no = this->motion_no;

    if (motion_no < 0) {
        return;
    }

    int             index = -1;
    tagMOTION_TYPE *motion = nullptr;
    tagMOTION_TYPE *shadow = nullptr;

    for (int i = 0; i < CHARA_MOTION_MAX; i++) {
        if (this->motion[i] == nullptr) {
            continue;
        }

        const int start = this->motion_start[i];

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

    if (shadow == nullptr) {
        return;
    }

    if (index < 0) {
        return;
    }

    if (motion->motion_info == nullptr || shadow->motion_info == nullptr) {
        return;
    }

    const float speed = motion->motion_info[index].speed;

    if ((this->motion_flags & 1) || MotionStopFlag) {
        shadow->motion_info[index].speed = 0.0f;
    }

    // The shadow stops on the last frame rather than running past it.
    if ((this->motion_flags & 2) != 0 && !(shadow->state.time + speed + 0.001f < static_cast<float>(shadow->motion_info[index].end))) {
        shadow->motion_info[index].speed = 0.0f;
    }

    if (this->shadow_frame == nullptr) {
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

// CCharacter::wind holds a 32-bit image address of a CWind, so the pointer is recovered through
// the image rather than cast back directly.
void CCharacter::ClothStep(int step) {
    if (MotionStopFlag != 0) {
        return;
    }

    sceVu0FVECTOR world_pos;
    sceVu0CopyVector(world_pos, this->pos);

    if (this->frame != nullptr) {
        this->frame->SetPosition(this->pos[0], this->pos[1], this->pos[2]);
        this->frame->SetRotation(this->rotation.x, this->rotation.y, this->rotation.z);
        CFrame *root = this->frame->parent;

        if (root != nullptr) {
            this->pos[3] = 1.0f;
            root->GetWorldPosition(world_pos, this->pos);
        }
    }

    for (CCloth *piece : std::span(this->cloth, CHARA_CLOTH_MAX)) {
        if (piece != nullptr) {
            piece->wind = PortImagePointer(this->wind);
            piece->floor_y = world_pos[1];
            piece->Step(step);
        }
    }
}

void CCharacter::ClothFloor(int floor) {
    for (int i = 0; i < CHARA_CLOTH_MAX; i++) {
        if (this->cloth[i] != nullptr) {
            this->cloth[i]->floor_on = floor;
        }
    }
}

void CCharacter::SetPosition(float x, float y, float z) {
    CObject::SetPosition(x, y, z);

    if (this->frame != nullptr) {
        this->frame->SetPosition(x, y, z);
    }

    if (this->shadow_frame != nullptr) {
        this->shadow_frame->SetPosition(x, y, z);
    }
}

void CCharacter::GetWorldPosition(float *out_position) {
    GetPosition(out_position);
    out_position[3] = 1.0f;

    if (this->frame->parent != nullptr) {
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

    if (this->frame != nullptr) {
        this->frame->SetRotation(x, y, z);
    }

    if (this->shadow_frame != nullptr) {
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

    if (this->frame != nullptr) {
        this->frame->SetScale(x, y, z);
    }

    if (this->shadow_frame != nullptr) {
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

    if (this->frame != nullptr) {
        this->frame->SetPosition(this->pos[0], this->pos[1], this->pos[2]);
        this->frame->SetRotation(this->rotation.x, this->rotation.y, this->rotation.z);
        this->frame->SetScale(this->scale[0], this->scale[1], this->scale[2]);
    }

    const bool fading = this->fade[0] < 1.0f;

    // The scene keeps the lights it had, so that the character can put them
    // back once it has drawn.
    MGGetAmbient(ambient);
    MGGetPLight(light_direction, light_colour);
    sceVu0CopyMatrix(saved_light_colour, light_colour);
    sceVu0CopyMatrix(saved_light_direction, light_direction);
    sceVu0CopyVector(saved_ambient, ambient);

    int light_slot = 3;

    for (CFakePointLight &light : this->point_light) {
        // Only a slot that the scene leaves free can take one of the character's lights.
        if (saved_light_direction[0][light_slot] != 0.0f) {
            break;
        }

        if (saved_light_direction[1][light_slot] != 0.0f) {
            break;
        }

        if (saved_light_direction[2][light_slot] != 0.0f) {
            break;
        }

        if (light.used == 0) {
            continue;
        }

        sceVu0FVECTOR pos;
        sceVu0FVECTOR direction;
        GetPosition(pos);
        sceVu0SubVector(direction, light.pos, pos);
        const float distance = DistVector(direction);

        if (!(distance <= light.outer_range)) {
            continue;
        }

        sceVu0Normalize(direction, direction);
        light_direction[0][light_slot] = direction[0];
        light_direction[1][light_slot] = direction[1];
        light_direction[2][light_slot] = direction[2];

        // The light gives its whole colour up to the inner range and fades
        // away over the rest.
        float level = 1.0f;

        if (!(distance <= light.inner_range)) {
            level = level - (distance - light.inner_range) / (light.outer_range - light.inner_range);
        }

        light_colour[light_slot][0] = light.colour[0] * level;
        light_colour[light_slot][1] = light.colour[1] * level;
        light_colour[light_slot][2] = light.colour[2] * level;
        light_colour[light_slot][3] = light.colour[3];

        light_slot--;

        if (light_slot <= 2) {
            break;
        }
    }

    if (fading) {
        sceVu0ScaleVectorXYZ(ambient, ambient, this->fade[0]);

        for (float *colour : light_colour) {
            sceVu0ScaleVectorXYZ(colour, colour, this->fade[0]);
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

    for (int cloth_no = 0; cloth_no < CHARA_CLOTH_MAX; cloth_no++) {
        if (this->cloth[cloth_no] != nullptr) {
            this->cloth[cloth_no]->Draw();
        }
    }

    MGSetAmbient(saved_ambient);
    MGSetPLight(saved_light_direction, saved_light_colour);
}

void CCharacter::DrawShadow() {
    if (this->shadow_frame == nullptr) {
        return;
    }

    sceVu0FVECTOR light_direction = {0.0f, 1.0f, 0.0f, 0.0f};
    sceVu0FVECTOR transform;

    this->shadow_frame->SetScale(this->scale[0], this->scale[1], this->scale[2]);
    GetRotation(transform);
    this->shadow_frame->SetRotation(transform[0], transform[1], transform[2]);
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
    ReadInfo(this, pack, name, model_alloc, motion_alloc, texture_alloc, 0, nullptr, 0, 0);
}

void CCharacter::LoadPackData2(unsigned int *pack, char *name, CDataAlloc2<1> *alloc, int texture_block_no, CDataAlloc2<1> *image_alloc, int visual_type) {
    ReadInfo(this, pack, name, alloc, alloc, alloc, texture_block_no, image_alloc, visual_type, 0);
}

void CCharacter::LoadPackData3(unsigned int *pack, char *name, CDataAlloc2<1> *alloc, int texture_block_no, CDataAlloc2<1> *image_alloc, int keep_textures, int visual_type) {
    ReadInfo(this, pack, name, alloc, alloc, alloc, texture_block_no, image_alloc, visual_type, keep_textures);
}

void CCharacter::DeleteExtendTexture(int block_no) {
    if (this->config != nullptr) {
        this->tex_anime.LoadCFGFile(this->config, this->config_size);
    }

    // A block that the character loaded textures into goes back to what the
    // character alone asks for.
    if (this->images[1] != nullptr || this->images[2] != nullptr || this->images[3] != nullptr) {
        std::array<LOADTEXTURE_INFO2, 2> info{};
        info[0].block_no = block_no;
        info[0].mipmap = false;
        info[0].name = reinterpret_cast<char *>(this->images[0]);
        info[1].block_no = 0;
        info[1].mipmap = false;
        info[1].name = nullptr;
        TexManager.DeleteTextureBlock(block_no);
        TexManager.LoadTextureBlockEX(block_no, info.data());
    }
}

void CCharacter::DeleteExtendMotion() {
    // The first set is the one the character was loaded with; the rest come
    // from packs that a caller added, and go with them.
    for (int i = 1; i < CHARA_MOTION_MAX; i++) {
        this->motion[i] = nullptr;
        this->shadow_motion[i] = nullptr;
        this->motion_start[i] = -1;
        this->motion_end[i] = -1;
    }
}

void CCharacter::Initialize() {
    CObject::Initialize(1.0f);
    this->frame = nullptr;
    this->motion_no = 0;
    this->shadow_frame = nullptr;
    this->motion_speed = -1.0f;
    this->frame_info = nullptr;
    this->shadow_frame_info = nullptr;
    this->motion_type.motion_info = nullptr;
    this->unk_C6C = 0.0f;
    this->motion_type.unk_78 = 0;
    this->motion_type.frame_info = nullptr;
    this->motion_type.unk_6C = 0;
    this->motion_type.unk_68 = 0;
    this->motion_type.unk_70 = 0;
    this->motion_type.unk_74 = 0;
    this->shadow_motion_type = this->motion_type;
    this->shadow_motion_type.frame_info = nullptr;
    this->motion_flags = 0;

    this->cloth = this->cloth_buf;

    for (int j = 0; j < CHARA_CLOTH_MAX; j++) {
        this->cloth[j] = nullptr;
    }

    this->wind = 0;

    for (int i = 0; i < CHARA_MOTION_MAX; i++) {
        this->motion[i] = nullptr;
        this->shadow_motion[i] = nullptr;
        this->motion_start[i] = -1;
        this->motion_end[i] = -1;
        std::memset(this->motion_storage[i].storage, 0, sizeof(this->motion_storage[i].storage));
        std::memset(this->shadow_motion_storage[i].storage, 0, sizeof(this->shadow_motion_storage[i].storage));
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

    for (float *ground : this->ground_ambient) {
        sceVu0CopyVector(ground, zero);
    }

    sceVu0CopyVector(this->ambient_tint, zero);

    // The first set of motions is the one the model was loaded with.
    this->motion[0] = &this->motion_type;
    this->shadow_motion[0] = &this->shadow_motion_type;
    this->motion_start[0] = 0;
    this->motion_end[0] = 256;

    for (CHARA_UNK_1068 &range : this->unk_1068) {
        range.unk_04 = -1.0f;
        range.unk_00 = -1;
        range.unk_0C = 0;
    }

    for (CHARA_FOOT_SOUND &sound : this->foot_sound) {
        sound.foot = -1;
        sound.frame = -1;
    }

    this->foot_sound_wait = 0;
    this->foot_sound_id = 0;
    this->foot_sound_enable = 1;
    ClearEvent(-1);
    this->event_enable = 1;
    this->motion_state = 0;
    this->config = nullptr;
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

} // namespace dc::game
