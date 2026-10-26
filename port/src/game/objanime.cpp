#include "objanime.hpp"

#include <libvu0.h>

#include <cstdlib>
#include <cstring>

#include "candleeffect.hpp"
#include "editloop.hpp"
#include "effectmacro.hpp"
#include "fireomni.hpp"
#include "frame.hpp"
#include "snd.hpp"
#include "texture.hpp"

/**
 * The one fire effect the editor lends to every map part that burns.
 */
CFireOmni Fire;
/**
 * The one candle effect the editor lends to every map part with a candle.
 */
CCandleEffect Candle;
int           all_stop;

/**
 * Clears one object-animation sequence.
 *
 * @mangled Initialize__13OBJ_ANIME_SEQFv
 * @address 0x165C90
 * @size 0x14
 */
void OBJ_ANIME_SEQ::Initialize() {
    property = OBJ_ANIME_PROPERTY_NONE;
    completion_flag = 0;
}

/**
 * Constructs an object-animation sequence.
 *
 * @mangled __ct__13OBJ_ANIME_SEQFv
 * @address 0x165CB0
 * @size 0x30
 */
OBJ_ANIME_SEQ::OBJ_ANIME_SEQ() {
    Initialize();
}

void ObjAnimeAllStop() {
    all_stop = 1;
}

void ObjAnimeAllStart() {
    all_stop = 0;
}

/**
 * Attaches an object animation to one frame.
 *
 * @mangled InitObjAnime__FP6CFrameP13OBJ_ANIME_SEQ
 * @address 0x165D00
 * @size 0xC4
 */
int InitObjAnime(CFrame *frame, OBJ_ANIME_SEQ *sequence) {
    int i;

    for (i = 0; i < 10; i++) {
        sequence->frames[i] = NULL;
    }

    if (sequence->property <= OBJ_ANIME_PROPERTY_NONE) {
        return 0;
    }

    // An empty name means the animation drives the frame it was given.
    if (sequence->frame_name[0] != 0) {
        sequence->frames[0] = frame->SearchFrame(sequence->frame_name);
    } else {
        sequence->frames[i] = frame;
    }

    if (sequence->frames[0] == NULL) {
        return 0;
    }

    sceVu0CopyVector(sequence->current, sequence->from);
    return 1;
}

/**
 * Attaches an object animation to a list of frames.
 *
 * @mangled InitObjAnime__FPP6CFrameP13OBJ_ANIME_SEQ
 * @address 0x165DD0
 * @size 0xFC
 */
int InitObjAnime(CFrame **frames, OBJ_ANIME_SEQ *sequence) {
    int i;

    if (sequence->property <= OBJ_ANIME_PROPERTY_NONE) {
        return 0;
    }

    for (i = 0; i < 10; i++) {
        sequence->frames[i] = NULL;
    }

    for (i = 0; i < 4; i++) {
        sequence->frames[i] = NULL;

        if (frames[i] != NULL) {
            if (sequence->frame_name[0] != 0) {
                sequence->frames[i] = frames[i]->SearchFrame(sequence->frame_name);
            } else {
                sequence->frames[i] = frames[i];
            }
        }
    }

    sceVu0CopyVector(sequence->current, sequence->from);
    return 1;
}

/**
 * Attaches an object animation to a counted list of frames.
 *
 * @mangled InitObjAnime__FPP6CFrameiP13OBJ_ANIME_SEQ
 * @address 0x165ED0
 * @size 0x138
 */
int InitObjAnime(CFrame **frames, int count, OBJ_ANIME_SEQ *sequence) {
    int i;
    int found = 0;

    if (sequence->property <= OBJ_ANIME_PROPERTY_NONE) {
        return 0;
    }

    if (count > 10) {
        count = 10;
    }

    for (i = 0; i < 10; i++) {
        sequence->frames[i] = NULL;
    }

    // The named frames are packed down, so a list with holes still fills
    // the front of the sequence.
    for (i = 0; i < count; i++) {
        if (frames[i] != NULL) {
            if (sequence->frame_name[0] != 0) {
                sequence->frames[found] = frames[i]->SearchFrame(sequence->frame_name);
            } else {
                sequence->frames[found] = frames[i];
            }

            if (sequence->frames[found] != NULL) {
                found++;
            }
        }
    }

    sceVu0CopyVector(sequence->current, sequence->from);
    return 1;
}

/**
 * Attaches an object animation to the frames one function point names.
 *
 * @mangled InitObjAnime__FPP6CFrameiP16EPARTS_FUNC_DATAP13OBJ_ANIME_SEQ
 * @address 0x166010
 * @size 0x160
 */
int InitObjAnime(CFrame **frames, int count, EPARTS_FUNC_DATA *func, OBJ_ANIME_SEQ *sequence) {
    sceVu0FVECTOR span;

    if (func->kind != EPARTS_FUNC_OBJ_ANIME) {
        return 0;
    }

    sequence->property = (int) func->position[0];
    sequence->mode = (int) func->position[1];
    strcpy(sequence->frame_name, (char *) func->frame_name);
    sceVu0CopyVector(sequence->from, func->rotation);
    int steps = (int) func->position[2];

    if (steps > 0) {
        sceVu0SubVector(span, func->values, func->rotation);
        sceVu0ScaleVector(sequence->step, span, 1.0f / (float) steps);
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

/**
 * Reports whether an animated value has passed its target in the direction it moves.
 *
 * @mangled end_check__Ffff
 * @address 0x166170
 * @size 0x68
 */
static int end_check(float value, float target, float step) {
    return step > 0.0f ? (value > target ? 1 : 0) : (value < target ? 1 : 0);
}

/**
 * Advances one object animation by a frame.
 *
 * @mangled ObjAnimePlay__FP13OBJ_ANIME_SEQ
 * @address 0x1661E0
 * @size 0x78C
 */
void ObjAnimePlay(OBJ_ANIME_SEQ *sequence) {
    sceVu0FVECTOR position;
    sceVu0FVECTOR scale;
    sceVu0FVECTOR rotation;
    int           i;
    float         value;

    if (all_stop != 0) {
        return;
    }

    if (sequence->property <= OBJ_ANIME_PROPERTY_NONE) {
        return;
    }

    switch (sequence->property) {
        case OBJ_ANIME_PROPERTY_ROTATION:
            if (sequence->frames[0] != NULL) {
                float x = (PI * sequence->current[0]) / 180.0f;
                float y = (PI * sequence->current[1]) / 180.0f;
                float z = (PI * sequence->current[2]) / 180.0f;
                sequence->frames[0]->SetRotType(2);
                sequence->frames[0]->SetRotation(x, y, z);
            }

            break;
        case OBJ_ANIME_PROPERTY_POSITION:
            if (sequence->frames[0] != NULL) {
                sequence->frames[0]->SetPosition(sequence->current);
            }

            break;
        case OBJ_ANIME_PROPERTY_SCALE:
            if (sequence->frames[0] != NULL) {
                sequence->frames[0]->SetScale(sequence->current);
            }

            break;
        case OBJ_ANIME_PROPERTY_COLOR:
            if (sequence->frames[0] != NULL) {
                CFrameAttr *attr = &sequence->frames[0]->attr;
                sceVu0CopyVector(attr->color, sequence->current);
                attr->use_color = true;
            }

            break;
    }

    if (sequence->frames[0] != NULL) {
        sceVu0CopyVector(position, sequence->frames[0]->position);
        CFrame *frame = sequence->frames[0];
        scale[0] = frame->scale[0];
        scale[1] = frame->scale[1];
        scale[2] = frame->scale[2];
        sequence->frames[0]->GetRotation(rotation);

        for (i = 1; i < 10; i++) {
            if (sequence->frames[i] != NULL) {
                sequence->frames[i]->SetRotType(2);
                sequence->frames[i]->SetRotation(rotation[0], rotation[1], rotation[2]);
                sequence->frames[i]->SetPosition(position);
                sequence->frames[i]->SetScale(scale);

                if (sequence->property == OBJ_ANIME_PROPERTY_COLOR) {
                    CFrameAttr *attr = &sequence->frames[i]->attr;
                    sceVu0CopyVector(attr->color, sequence->current);
                    attr->use_color = true;
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

            if (end_check(sequence->current[0], sequence->to[0], sequence->step[0]) != 0) {
                sequence->current[0] = sequence->from[0];
            }

            if (end_check(sequence->current[1], sequence->to[1], sequence->step[1]) != 0) {
                sequence->current[1] = sequence->from[1];
            }

            if (end_check(sequence->current[2], sequence->to[2], sequence->step[2]) != 0) {
                sequence->current[2] = sequence->from[2];
            }

            break;
        case OBJ_ANIME_MODE_PING_PONG:
            // Reverse at either end, swapping the two ends round.
            sceVu0AddVector(sequence->current, sequence->current, sequence->step);

            for (int i = 0; i < 3; i++) {
                if (end_check(sequence->current[i], sequence->to[i], sequence->step[i]) != 0) {
                    sequence->step[i] *= -1.0f;
                    sequence->current[i] = sequence->to[i];
                    value = sequence->from[i];
                    sequence->from[i] = sequence->to[i];
                    sequence->to[i] = value;
                }
            }

            break;
        case OBJ_ANIME_MODE_ONCE:
            // Stop at the far end.
            sceVu0AddVector(sequence->current, sequence->current, sequence->step);

            for (int i = 0; i < 3; i++) {
                if (end_check(sequence->current[i], sequence->to[i], sequence->step[i]) != 0) {
                    sequence->current[i] = sequence->to[i];
                    sequence->step[i] = 0.0f;
                }
            }

            break;
        case OBJ_ANIME_MODE_RANDOM:
            for (i = 0; i < 3; i++) {
                value = sequence->to[i] - sequence->from[i];
                value *= (float) rand() / 2.1474836e9f;
                sequence->current[i] = sequence->from[i] + value;
            }

            break;
        case OBJ_ANIME_MODE_RANDOM_UNIFORM:
            value = sequence->to[0] - sequence->from[0];
            value *= (float) rand() / 2.1474836e9f;
            sequence->current[1] = sequence->current[0] = sequence->from[0] + value;
            sequence->current[2] = sequence->current[0];
            break;
        case OBJ_ANIME_MODE_RANDOM_WALK:
            for (i = 0; i < 3; i++) {
                value = sequence->step[i] * (((float) rand() / 2.1474836e9f) - 0.5f);
                sequence->current[i] += value;

                if (sequence->current[i] < sequence->from[i]) {
                    sequence->current[i] = sequence->from[i];
                }

                if (sequence->current[i] > sequence->to[i]) {
                    sequence->current[i] = sequence->to[i];
                }
            }

            break;
        case OBJ_ANIME_MODE_RANDOM_WALK_UNIFORM:
            value = sequence->step[0] * (((float) rand() / 2.1474836e9f) - 0.5f);
            sequence->current[0] += value;

            if (sequence->current[0] < sequence->from[0]) {
                sequence->current[0] = sequence->from[0];
            }

            if (sequence->current[0] > sequence->to[0]) {
                sequence->current[0] = sequence->to[0];
            }

            sequence->current[1] = sequence->current[0];
            sequence->current[2] = sequence->current[0];
            break;
    }

    if (sequence->property == OBJ_ANIME_PROPERTY_ROTATION) {
        if (sequence->current[0] > 180.0f) {
            sequence->current[0] -= 360.0f;
        }

        if (sequence->current[1] > 180.0f) {
            sequence->current[1] -= 360.0f;
        }

        if (sequence->current[2] > 180.0f) {
            sequence->current[2] -= 360.0f;
        }

        if (sequence->current[0] < -180.0f) {
            sequence->current[0] += 360.0f;
        }

        if (sequence->current[1] < -180.0f) {
            sequence->current[1] += 360.0f;
        }

        if (sequence->current[2] < -180.0f) {
            sequence->current[2] += 360.0f;
        }
    }
}

void InitEditEffect(CFrame *frame, EDIT_EFFECT_INFO *effect) {
    if (frame == NULL) {
        effect->frame = NULL;
        return;
    }

    effect->frame = frame->SearchFrame(effect->frame_name);

    if (effect->frame == NULL) {
        effect->frame = frame;
    }
}

/**
 * Attaches an editor effect to the frame a function point names.
 *
 * @mangled InitEditEffect__FP6CFrameP16EPARTS_FUNC_DATAP16EDIT_EFFECT_INFO
 * @address 0x1669D0
 * @size 0x1E0
 */
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

    if ((u_char) func->frame_name[0] != 0) {
        strcpy(effect->frame_name, (char *) func->frame_name);
    } else {
        strcpy(effect->frame_name, frame->name);
    }

    effect->start = ConvertTime(func->start_time);
    effect->end = ConvertTime(func->end_time);

    for (int i = 0; i < 4; i++) {
        (&effect->offset)[0][i] = (&func->position)[0][i];
        (&effect->offset)[1][i] = (&func->position)[1][i];
        (&effect->offset)[2][i] = (&func->position)[2][i];
        (&effect->offset)[3][i] = (&func->position)[3][i];
    }

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
    CFrame *frame = effect->frame;

    if (frame == NULL) {
        return 1;
    }

    if (!(frame->attr.draw_on & 1)) {
        return 0;
    }

    for (frame = frame->parent; frame != NULL; frame = frame->parent) {
        if (!(frame->attr.draw_on & 1)) {
            return 0;
        }
    }

    return 1;
}

/**
 * Advances the editor's shared fire, candle and flame effects.
 *
 * @mangled EditEffectStep__Fv
 * @address 0x166D10
 * @size 0xC4
 */
void EditEffectStep() {
    Fire.FireStep();
    Candle.Step();
    Candle.SetTexture(TexManager.GetTexture("rousoku", -1));
    Fire.SetTexture(TexManager.GetTexture("lightling", -1), TexManager.GetTexture("blender", -1));
}

/**
 * Rebuilds the editor's fire texture for the frame.
 *
 * @mangled EditEffectStep2__Fv
 * @address 0x166DE0
 * @size 0x28
 */
void EditEffectStep2() {
    Fire.FireCreate();
}

/**
 * Draws one editor effect, choosing the kind from its record.
 *
 * @mangled DrawEditEffect__FP16EDIT_EFFECT_INFOP7CCameraP12CEffectGroup
 * @address 0x166E10
 * @size 0x24C
 */
void DrawEditEffect(EDIT_EFFECT_INFO *effect, CCamera *camera, CEffectGroup *group) {
    sceVu0FVECTOR position;
    float         scale;
    int           fire_kind;

    if (effect == NULL) {
        return;
    }

    if (effect->kind <= EDIT_EFFECT_NONE) {
        return;
    }

    if (effect->frame == NULL) {
        return;
    }

    effect->frame->GetWorldPosition(position, effect->offset);
    scale = effect->colour[0];

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
        case EDIT_EFFECT_FIRE_LARGE:
        case EDIT_EFFECT_FIRE_SMALL:
        case EDIT_EFFECT_FIRE_MEDIUM: {
            float z = 0.1f * position[2];
            float y = 0.1f * position[1];
            float x = 0.1f * position[0];
            Fire.pos[0] = 10.0f * x;
            Fire.pos[1] = 10.0f * y;
            Fire.pos[2] = 10.0f * z;
        }
            Fire.pos[3] = 1.0f;
            position[3] = 1.0f;

            if (effect->kind == EDIT_EFFECT_FIRE_LARGE) {
                fire_kind = 3;
            }

            if (effect->kind == EDIT_EFFECT_FIRE_SMALL) {
                fire_kind = 1;
            }

            if (effect->kind == EDIT_EFFECT_FIRE_MEDIUM) {
                fire_kind = 2;
            }

            if (effect->kind == EDIT_EFFECT_CANDLE) {
                fire_kind = 2;
            }

            setbilinear(1);
            Fire.DrawFire(1, 1, camera, position, scale, fire_kind, 15.0f);
            break;
        case EDIT_EFFECT_SMOKE:
            if (group != NULL) {
                EffectSmoke(group, position, effect->colour[0], 13);
            }

            break;
    }
}
