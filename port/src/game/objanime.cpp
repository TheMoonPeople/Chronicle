#include "objanime.hpp"

#include <libvu0.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <span>
#include <string_view>
#include <utility>

#include "candleeffect.hpp"
#include "editloop.hpp"
#include "effectmacro.hpp"
#include "fireomni.hpp"
#include "frame.hpp"
#include "snd.hpp"
#include "texture.hpp"

namespace dc::game {

namespace {

/**
 * The one fire effect the editor lends to every map part that burns.
 */
// init_priority keeps this static's place in the order rand() is drawn from; see app/runtime.cpp.
CFireOmni Fire __attribute__((init_priority(65007)));

/**
 * The one candle effect the editor lends to every map part with a candle.
 */
CCandleEffect Candle;

bool all_stop;

/**
 * Reports whether an animated value has passed its target in the direction it moves.
 */
bool end_check(float value, float target, float step) {
    return step > 0.0f ? value > target : value < target;
}

/**
 * Divides rand() by 2^31, the range of the PS2 library's generator.
 */
float RandomFraction() {
    return static_cast<float>(std::rand()) / 2.1474836e9f;
}

void CopyFrameName(char (&destination)[16], std::string_view name) {
    std::ranges::copy(name, destination);
    destination[name.size()] = '\0';
}

} // namespace

void OBJ_ANIME_SEQ::Initialize() {
    property = OBJ_ANIME_PROPERTY_NONE;
    completion_flag = 0;
}

OBJ_ANIME_SEQ::OBJ_ANIME_SEQ() {
    Initialize();
}

void ObjAnimeAllStop() {
    all_stop = true;
}

void ObjAnimeAllStart() {
    all_stop = false;
}

int InitObjAnime(CFrame *frame, OBJ_ANIME_SEQ *sequence) {
    int i = 0;

    for (i = 0; i < 10; i++) {
        sequence->frames[i] = nullptr;
    }

    if (sequence->property <= OBJ_ANIME_PROPERTY_NONE) {
        return 0;
    }

    if (sequence->frame_name[0] != '\0') {
        sequence->frames[0] = frame->SearchFrame(sequence->frame_name);
    } else {
        // i is 10 here: an unnamed frame lands one past the list, over completion_flag, and is never driven.
        sequence->frames[i] = frame;
    }

    if (sequence->frames[0] == nullptr) {
        return 0;
    }

    sceVu0CopyVector(sequence->current, sequence->from);
    return 1;
}

int InitObjAnime(CFrame **frames, OBJ_ANIME_SEQ *sequence) {
    if (sequence->property <= OBJ_ANIME_PROPERTY_NONE) {
        return 0;
    }

    std::ranges::fill(sequence->frames, nullptr);

    for (int i = 0; i < 4; i++) {
        if (frames[i] != nullptr) {
            if (sequence->frame_name[0] != '\0') {
                sequence->frames[i] = frames[i]->SearchFrame(sequence->frame_name);
            } else {
                sequence->frames[i] = frames[i];
            }
        }
    }

    sceVu0CopyVector(sequence->current, sequence->from);
    return 1;
}

int InitObjAnime(CFrame **frames, int count, OBJ_ANIME_SEQ *sequence) {
    if (sequence->property <= OBJ_ANIME_PROPERTY_NONE) {
        return 0;
    }

    count = std::min(count, 10);
    std::ranges::fill(sequence->frames, nullptr);

    // The found frames are packed down, so a list with holes still fills the front of the sequence.
    int found = 0;

    for (CFrame *source : std::span(frames, static_cast<std::size_t>(std::max(count, 0)))) {
        if (source != nullptr) {
            if (sequence->frame_name[0] != '\0') {
                sequence->frames[found] = source->SearchFrame(sequence->frame_name);
            } else {
                sequence->frames[found] = source;
            }

            if (sequence->frames[found] != nullptr) {
                found++;
            }
        }
    }

    sceVu0CopyVector(sequence->current, sequence->from);
    return 1;
}

int InitObjAnime(CFrame **frames, int count, EPARTS_FUNC_DATA *func, OBJ_ANIME_SEQ *sequence) {
    if (func->kind != EPARTS_FUNC_OBJ_ANIME) {
        return 0;
    }

    sequence->property = static_cast<int>(func->position[0]);
    sequence->mode = static_cast<int>(func->position[1]);
    CopyFrameName(sequence->frame_name, func->frame_name);
    sceVu0CopyVector(sequence->from, func->rotation);
    const int steps = static_cast<int>(func->position[2]);

    if (steps > 0) {
        sceVu0FVECTOR span;
        sceVu0SubVector(span, func->values, func->rotation);
        sceVu0ScaleVector(sequence->step, span, 1.0f / static_cast<float>(steps));
    } else {
        sceVu0CopyVector(sequence->step, func->parameters);
    }

    sceVu0CopyVector(sequence->to, func->values);
    InitObjAnime(frames, count, sequence);

    if (steps > 0) {
        sceVu0CopyVector(sequence->current, func->parameters);
    }

    sequence->completion_flag = func->completion_flag;
    return 1;
}

void ObjAnimePlay(OBJ_ANIME_SEQ *sequence) {
    if (all_stop) {
        return;
    }

    if (sequence->property <= OBJ_ANIME_PROPERTY_NONE) {
        return;
    }

    CFrame *const lead = sequence->frames[0];

    switch (sequence->property) {
        case OBJ_ANIME_PROPERTY_ROTATION:
            if (lead != nullptr) {
                const float x = (PI * sequence->current[0]) / 180.0f;
                const float y = (PI * sequence->current[1]) / 180.0f;
                const float z = (PI * sequence->current[2]) / 180.0f;
                lead->SetRotType(2);
                lead->SetRotation(x, y, z);
            }

            break;
        case OBJ_ANIME_PROPERTY_POSITION:
            if (lead != nullptr) {
                lead->SetPosition(sequence->current);
            }

            break;
        case OBJ_ANIME_PROPERTY_SCALE:
            if (lead != nullptr) {
                lead->SetScale(sequence->current);
            }

            break;
        case OBJ_ANIME_PROPERTY_COLOR:
            if (lead != nullptr) {
                sceVu0CopyVector(lead->attr.color, sequence->current);
                lead->attr.use_color = true;
            }

            break;
        default:
            break;
    }

    if (lead != nullptr) {
        sceVu0FVECTOR position;
        sceVu0FVECTOR scale;
        sceVu0FVECTOR rotation;
        sceVu0CopyVector(position, lead->position);
        scale[0] = lead->scale[0];
        scale[1] = lead->scale[1];
        scale[2] = lead->scale[2];
        lead->GetRotation(rotation);

        for (CFrame *follower : std::span(sequence->frames).subspan(1)) {
            if (follower != nullptr) {
                follower->SetRotType(2);
                follower->SetRotation(rotation[0], rotation[1], rotation[2]);
                follower->SetPosition(position);
                follower->SetScale(scale);

                if (sequence->property == OBJ_ANIME_PROPERTY_COLOR) {
                    sceVu0CopyVector(follower->attr.color, sequence->current);
                    follower->attr.use_color = true;
                }
            }
        }
    }

    switch (sequence->mode) {
        case OBJ_ANIME_MODE_LINEAR:
            sceVu0AddVector(sequence->current, sequence->current, sequence->step);
            break;
        case OBJ_ANIME_MODE_WRAP:
            sceVu0AddVector(sequence->current, sequence->current, sequence->step);

            for (int axis = 0; axis < 3; axis++) {
                if (end_check(sequence->current[axis], sequence->to[axis], sequence->step[axis])) {
                    sequence->current[axis] = sequence->from[axis];
                }
            }

            break;
        case OBJ_ANIME_MODE_PING_PONG:
            sceVu0AddVector(sequence->current, sequence->current, sequence->step);

            for (int axis = 0; axis < 3; axis++) {
                if (end_check(sequence->current[axis], sequence->to[axis], sequence->step[axis])) {
                    sequence->step[axis] *= -1.0f;
                    sequence->current[axis] = sequence->to[axis];
                    std::swap(sequence->from[axis], sequence->to[axis]);
                }
            }

            break;
        case OBJ_ANIME_MODE_ONCE:
            sceVu0AddVector(sequence->current, sequence->current, sequence->step);

            for (int axis = 0; axis < 3; axis++) {
                if (end_check(sequence->current[axis], sequence->to[axis], sequence->step[axis])) {
                    sequence->current[axis] = sequence->to[axis];
                    sequence->step[axis] = 0.0f;
                }
            }

            break;
        case OBJ_ANIME_MODE_RANDOM:
            for (int axis = 0; axis < 3; axis++) {
                float offset = sequence->to[axis] - sequence->from[axis];
                offset *= RandomFraction();
                sequence->current[axis] = sequence->from[axis] + offset;
            }

            break;
        case OBJ_ANIME_MODE_RANDOM_UNIFORM: {
            float offset = sequence->to[0] - sequence->from[0];
            offset *= RandomFraction();
            sequence->current[0] = sequence->from[0] + offset;
            sequence->current[1] = sequence->current[0];
            sequence->current[2] = sequence->current[0];
            break;
        }
        case OBJ_ANIME_MODE_RANDOM_WALK:
            for (int axis = 0; axis < 3; axis++) {
                const float offset = sequence->step[axis] * (RandomFraction() - 0.5f);
                sequence->current[axis] += offset;

                sequence->current[axis] = std::max(sequence->current[axis], sequence->from[axis]);
                sequence->current[axis] = std::min(sequence->current[axis], sequence->to[axis]);
            }

            break;
        case OBJ_ANIME_MODE_RANDOM_WALK_UNIFORM: {
            const float offset = sequence->step[0] * (RandomFraction() - 0.5f);
            sequence->current[0] += offset;

            sequence->current[0] = std::max(sequence->current[0], sequence->from[0]);
            sequence->current[0] = std::min(sequence->current[0], sequence->to[0]);

            sequence->current[1] = sequence->current[0];
            sequence->current[2] = sequence->current[0];
            break;
        }
        default:
            break;
    }

    if (sequence->property == OBJ_ANIME_PROPERTY_ROTATION) {
        for (float &angle : std::span(sequence->current).first<3>()) {
            if (angle > 180.0f) {
                angle -= 360.0f;
            }

            if (angle < -180.0f) {
                angle += 360.0f;
            }
        }
    }
}

void InitEditEffect(CFrame *frame, EDIT_EFFECT_INFO *effect) {
    if (frame == nullptr) {
        effect->frame = nullptr;
        return;
    }

    effect->frame = frame->SearchFrame(effect->frame_name);

    if (effect->frame == nullptr) {
        effect->frame = frame;
    }
}

int InitEditEffect(CFrame *frame, EPARTS_FUNC_DATA *func, EDIT_EFFECT_INFO *effect) {
    switch (func->kind) {
        case EPARTS_FUNC_FIRE_LARGE:
            effect->kind = EDIT_EFFECT_FIRE_LARGE;
            break;
        case EPARTS_FUNC_FIRE_MEDIUM:
        case EPARTS_FUNC_UNK_D:
            effect->kind = EDIT_EFFECT_FIRE_MEDIUM;
            break;
        case EPARTS_FUNC_FIRE_SMALL:
            effect->kind = EDIT_EFFECT_FIRE_SMALL;
            break;
        case EPARTS_FUNC_SMOKE:
            effect->kind = EDIT_EFFECT_SMOKE;
            break;
        case EPARTS_FUNC_UNK_E:
            effect->kind = EDIT_EFFECT_UNK_5;
            break;
        case EPARTS_FUNC_CANDLE:
            effect->kind = EDIT_EFFECT_CANDLE;
            break;
        case EPARTS_FUNC_SOUND:
            effect->kind = EDIT_EFFECT_SOUND;
            break;
        default:
            return 0;
    }

    effect->map_flag = func->completion_flag;

    if (func->frame_name[0] != '\0') {
        CopyFrameName(effect->frame_name, func->frame_name);
    } else {
        CopyFrameName(effect->frame_name, frame->name);
    }

    effect->start = ConvertTime(func->start_time);
    effect->end = ConvertTime(func->end_time);

    // The marker's four vectors map onto offset, unk_40, colour and the sound parameters.
    sceVu0CopyVector(effect->offset, func->position);
    std::memcpy(effect->unk_40, func->rotation, sizeof(effect->unk_40));
    sceVu0CopyVector(effect->colour, func->parameters);
    effect->sound_no = func->values[0];
    effect->near_distance = func->values[1];
    effect->far_distance = func->values[2];
    std::memcpy(effect->unk_6C, &func->values[3], sizeof(effect->unk_6C));

    effect->offset[3] = 1.0f;
    InitEditEffect(frame, effect);
    return 1;
}

int CheckEditEffect(EDIT_EFFECT_INFO *effect, float time) {
    if (effect->kind <= EDIT_EFFECT_NONE) {
        return 0;
    }

    if (effect->map_flag > 0 && EdGetMapFlag(effect->map_flag)) {
        return 0;
    }

    // An effect whose start is later than its end runs across midnight.
    if (effect->start > effect->end && effect->start > time && effect->end <= time) {
        return 0;
    }

    if (effect->start < effect->end && (effect->start > time || effect->end <= time)) {
        return 0;
    }

    // The effect draws only while its frame and every parent above it are shown.
    for (const CFrame *frame = effect->frame; frame != nullptr; frame = frame->parent) {
        if ((frame->attr.draw_on & 1) == 0) {
            return 0;
        }
    }

    return 1;
}

void EditEffectStep() {
    Fire.FireStep();
    Candle.Step();
    Candle.SetTexture(TexManager.GetTexture("rousoku", -1));
    Fire.SetTexture(TexManager.GetTexture("lightling", -1), TexManager.GetTexture("blender", -1));
}

void EditEffectStep2() {
    Fire.FireCreate();
}

void DrawEditEffect(EDIT_EFFECT_INFO *effect, CCamera *camera, CEffectGroup *group) {
    if (effect == nullptr) {
        return;
    }

    if (effect->kind <= EDIT_EFFECT_NONE) {
        return;
    }

    if (effect->frame == nullptr) {
        return;
    }

    sceVu0FVECTOR position;
    effect->frame->GetWorldPosition(position, effect->offset);
    float scale = effect->colour[0];

    switch (effect->kind) {
        case EDIT_EFFECT_CANDLE:
            setbilinear(1);
            Candle.SetPosition(position);
            Candle.SetScale(effect->colour[0], effect->colour[1]);
            Candle.Draw();
            scale *= 0.1f;
            position[1] -= 4.0f;

            if (effect->sound_no > 0.0f) {
                break;
            }

            [[fallthrough]];
        case EDIT_EFFECT_FIRE_LARGE:
        case EDIT_EFFECT_FIRE_SMALL:
        case EDIT_EFFECT_FIRE_MEDIUM: {
            const float z = 0.1f * position[2];
            const float y = 0.1f * position[1];
            const float x = 0.1f * position[0];
            Fire.pos[0] = 10.0f * x;
            Fire.pos[1] = 10.0f * y;
            Fire.pos[2] = 10.0f * z;
            Fire.pos[3] = 1.0f;
            position[3] = 1.0f;

            // Medium fires and candles draw two layers.
            int fire_kind = 2;

            if (effect->kind == EDIT_EFFECT_FIRE_LARGE) {
                fire_kind = 3;
            } else if (effect->kind == EDIT_EFFECT_FIRE_SMALL) {
                fire_kind = 1;
            }

            setbilinear(1);
            Fire.DrawFire(1, 1, camera, position, scale, fire_kind, 15.0f);
            break;
        }
        case EDIT_EFFECT_SMOKE:
            if (group != nullptr) {
                EffectSmoke(group, position, effect->colour[0], 13);
            }

            break;
        default:
            break;
    }
}

} // namespace dc::game
