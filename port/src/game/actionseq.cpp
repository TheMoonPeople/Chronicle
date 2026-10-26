#include "actionseq.hpp"
#include "sce/libvu0.h"

#include <cmath>

#include "character.hpp"
#include "mathutil.hpp"

void CActionSeq::Initialize(ACT_SEQ *records, int count) {
    ClearSeq();
    pool = records;
    capacity = count;
}

CActionSeq::CActionSeq() {
    Initialize(NULL, 0);
}

void CActionSeq::ClearSeq() {
    character = NULL;
    move_tail = NULL;
    move_head = NULL;
    move_frame = 0;
    rotation_mode = 0;
    rotation_complete = 0;
    rotation_tail = NULL;
    rotation_head = NULL;
    rotation_frame = 0;
    motion_tail = NULL;
    motion_head = NULL;
    anime_tail = NULL;
    anime_head = NULL;
    rotation_delay = 0;
    anime_frame = 0;
    motion_trigger = 0;
    motion_delay = 0;
    anime_trigger = 0;
    anime_delay = 0;
}

ACT_SEQ *CActionSeq::GetNextSeq() {
    ACT_SEQ *sequence = pool;
    int      index = 0;

    for (; index < capacity; index++, sequence++) {
        if (sequence->operation == ACT_SEQ_UNUSED) {
            sequence->next = NULL;
            return sequence;
        }
    }

    return NULL;
}

void CActionSeq::SyncChara(CCharacter *target) {
    sceVu0FVECTOR location;
    sceVu0FVECTOR angles;
    character = target;

    if (target != NULL) {
        target->GetPosition(location);
        target->GetRotation(angles);
        SetPos(location);
        SetRot(angles);
    }
}

ACT_SEQ *CActionSeq::NextMoveSeq() {
    ACT_SEQ *sequence = GetNextSeq();

    if (sequence == NULL) {
        return NULL;
    }

    if (move_tail != NULL) {
        move_tail->next = sequence;
    }

    move_tail = sequence;
    sequence->next = NULL;

    if (move_head == NULL) {
        move_head = sequence;
    }

    return sequence;
}

ACT_SEQ *CActionSeq::NextMotionSeq() {
    ACT_SEQ *sequence = GetNextSeq();

    if (sequence == NULL) {
        return NULL;
    }

    if (motion_tail != NULL) {
        motion_tail->next = sequence;
    }

    motion_tail = sequence;
    sequence->next = NULL;

    if (motion_head == NULL) {
        motion_head = sequence;
    }

    return sequence;
}

ACT_SEQ *CActionSeq::NextAnimeSeq() {
    ACT_SEQ *sequence = GetNextSeq();

    if (sequence == NULL) {
        return NULL;
    }

    if (anime_tail != NULL) {
        anime_tail->next = sequence;
    }

    anime_tail = sequence;
    sequence->next = NULL;

    if (anime_head == NULL) {
        anime_head = sequence;
    }

    return sequence;
}

void CActionSeq::MoveSeq(float *destination, int frames) {
    ACT_SEQ *sequence = NextMoveSeq();

    if (sequence != NULL) {
        sequence->operation = ACT_SEQ_MOVE;
        sceVu0CopyVector(sequence->arguments.vector, destination);
        sceVu0CopyVector(queued_position, destination);
        sequence->duration = frames;
    }
}

void CActionSeq::MoveSeq(float *destination, float speed) {
    float duration = DistVector(destination, queued_position) / speed;
    int   frames = duration;

    if (duration - frames > 0.0) {
        frames++;
    }

    MoveSeq(destination, frames);
}

void CActionSeq::MoveSeq(int frames) {
    MoveSeq(queued_position, frames);
}

void CActionSeq::SetPos(float *destination) {
    ACT_SEQ *sequence = NextMoveSeq();

    if (sequence != NULL) {
        sequence->operation = ACT_SEQ_SET_POSITION;
        sceVu0CopyVector(sequence->arguments.vector, destination);
        sceVu0CopyVector(queued_position, destination);
        sequence->duration = 0;
    }
}

void CActionSeq::RotRefSeq(float *reference, float speed) {
    ACT_SEQ *sequence = NextMoveSeq();

    if (sequence != NULL) {
        sequence->operation = ACT_SEQ_ROTATE_REFERENCE;
        sceVu0CopyVector(sequence->arguments.vector, reference);
        sequence->arguments.vector[3] = speed;
        sequence->duration = 0;
    }
}

void CActionSeq::RotAngleSeq(float angle, float speed) {
    ACT_SEQ *sequence = NextMoveSeq();

    if (sequence != NULL) {
        sequence->operation = ACT_SEQ_ROTATE_ANGLE;
        sequence->arguments.vector[0] = 0;
        sequence->arguments.vector[1] = angle;
        sequence->arguments.vector[2] = 0;
        sequence->arguments.vector[3] = speed;
        sequence->duration = 0;
    }
}

void CActionSeq::RotMoveSeq(float speed) {
    ACT_SEQ *sequence = NextMoveSeq();

    if (sequence != NULL) {
        sequence->operation = ACT_SEQ_ROTATE_MOVEMENT;
        sequence->arguments.vector[3] = speed;
        sequence->duration = 0;
    }
}

void CActionSeq::ClearRotSeq() {
    ACT_SEQ *sequence = NextMoveSeq();

    if (sequence != NULL) {
        sequence->operation = ACT_SEQ_CLEAR_ROTATION;
        sequence->duration = 0;
    }
}

void CActionSeq::WaitRotSeq() {
    ACT_SEQ *sequence = NextMoveSeq();

    if (sequence != NULL) {
        sequence->operation = ACT_SEQ_WAIT_ROTATION;
        sequence->duration = 0;
    }
}

void CActionSeq::SetRot(float *angles) {
    ACT_SEQ *sequence = NextMoveSeq();

    if (sequence != NULL) {
        sequence->operation = ACT_SEQ_SET_ROTATION;
        sceVu0CopyVector(sequence->arguments.vector, angles);
        sequence->duration = 0;
    }
}

void CActionSeq::SetDelayRot(int delay) {
    ACT_SEQ *sequence = NextMoveSeq();

    if (sequence != NULL) {
        sequence->operation = ACT_SEQ_DELAY_ROTATION;
        sequence->duration = delay;
    }
}

void CActionSeq::MotionTrg(int delay) {
    ACT_SEQ *sequence = NextMoveSeq();

    if (sequence != NULL) {
        sequence->operation = ACT_SEQ_TRIGGER_MOTION;
        sequence->duration = delay;
    }
}

void CActionSeq::AnimeTrg(int delay) {
    ACT_SEQ *sequence = NextMoveSeq();

    if (sequence != NULL) {
        sequence->operation = ACT_SEQ_TRIGGER_ANIMATION;
        sequence->duration = delay;
    }
}

void CActionSeq::MotionSeq(int motion, int mode, float speed, int flags) {
    ACT_SEQ *sequence = NextMotionSeq();

    if (sequence != NULL) {
        sequence->operation = ACT_SEQ_MOTION;
        sequence->duration = 0;
        sequence->arguments.animation.id = motion;
        sequence->arguments.animation.mode = mode;
        sequence->arguments.animation.playback.speed = speed;
        sequence->arguments.animation.flags = flags & 4;
    }
}

void CActionSeq::AnimeSeq(int animation, int enabled, int frames, int disable_after) {
    ACT_SEQ *sequence = NextAnimeSeq();

    if (sequence != NULL) {
        sequence->operation = ACT_SEQ_ANIMATION;
        sequence->duration = frames;
        sequence->arguments.animation.id = animation;
        sequence->arguments.animation.mode = enabled;
        sequence->arguments.animation.playback.disable_after = 0;

        if (frames > 0) {
            sequence->arguments.animation.playback.disable_after = disable_after;
        }
    }
}

int CActionSeq::CheckEnd() {
    int motion_finished = 0;

    if (motion_head != NULL) {
        if (motion_head->arguments.animation.mode != 7) {
            motion_finished = 1;
        }
    } else {
        motion_finished = 1;
    }

    return move_head == NULL && motion_finished && anime_head == NULL;
}

/**
 * Releases an action record and returns the next queued record.
 */
static ACT_SEQ *DeleteSeq(ACT_SEQ *sequence) {
    if (sequence == NULL) {
        return NULL;
    }

    ACT_SEQ *next = sequence->next;
    sequence->operation = ACT_SEQ_UNUSED;
    sequence->next = NULL;
    return next;
}

/**
 * Advances queued actions and updates the bound character transform.
 *
 * @mangled Play__10CActionSeqFv
 * @address 0x1554A0
 * @size 0xB04
 */
void CActionSeq::Play() {
    int      animation_shown;
    ACT_SEQ *sequence;

    if (move_head == NULL && motion_head == NULL && anime_head == NULL) {
        return;
    }

    while ((sequence = move_head) != NULL) {
        switch (sequence->operation) {
            case ACT_SEQ_TRIGGER_MOTION:
                if (motion_trigger != 0) {
                    motion_head = DeleteSeq(motion_head);

                    if (motion_head == NULL) {
                        motion_tail = NULL;
                    }
                }

                if (motion_head != NULL) {
                    motion_trigger = 1;
                    motion_delay = sequence->duration;
                }

                move_head = DeleteSeq(sequence);
                continue;
            case ACT_SEQ_TRIGGER_ANIMATION:
                anime_trigger = 1;
                anime_delay = sequence->duration;
                move_head = DeleteSeq(sequence);
                continue;
            case ACT_SEQ_SET_POSITION:
                sceVu0CopyVector(position, sequence->arguments.vector);
                sceVu0CopyVector(start_position, sequence->arguments.vector);
                move_head = DeleteSeq(sequence);
                move_frame = 0;
                continue;
            case ACT_SEQ_SET_ROTATION:
                // The rotation is written twice; the start rotation keeps its old value.
                sceVu0CopyVector(rotation, sequence->arguments.vector);
                sceVu0CopyVector(rotation, sequence->arguments.vector);
                move_head = DeleteSeq(sequence);
                move_frame = 0;
                continue;
            case ACT_SEQ_ROTATE_REFERENCE:
                rotation_mode = 1;
                rotation_complete = 0;
                sceVu0CopyVector(rotation_target, sequence->arguments.vector);
                move_head = DeleteSeq(sequence);
                continue;
            case ACT_SEQ_ROTATE_ANGLE:
                rotation_mode = 3;
                rotation_complete = 0;
                sceVu0CopyVector(rotation_target, sequence->arguments.vector);
                move_head = DeleteSeq(sequence);
                continue;
            case ACT_SEQ_DELAY_ROTATION:
                rotation_delay = sequence->duration;
                move_head = DeleteSeq(sequence);
                continue;
            case ACT_SEQ_ROTATE_MOVEMENT:
                rotation_mode = 2;
                rotation_complete = 0;
                sceVu0CopyVector(rotation_target, sequence->arguments.vector);
                move_head = DeleteSeq(sequence);
                continue;
            case ACT_SEQ_CLEAR_ROTATION:
                rotation_mode = 0;
                rotation_complete = 0;
                move_head = DeleteSeq(sequence);
                continue;
            case ACT_SEQ_WAIT_ROTATION:
                if (rotation_complete != 0) {
                    rotation_mode = 0;
                    rotation_complete = 0;
                    move_head = DeleteSeq(sequence);
                }

                break;
            case ACT_SEQ_MOVE:
                move_frame++;

                if (move_frame >= sequence->duration) {
                    sceVu0CopyVector(position, sequence->arguments.vector);
                    sceVu0CopyVector(start_position, sequence->arguments.vector);
                    move_head = DeleteSeq(sequence);
                    move_frame = 0;
                } else {
                    sceVu0FVECTOR offset;

                    sceVu0SubVector(offset, sequence->arguments.vector, start_position);
                    sceVu0ScaleVector(offset, offset, (float) move_frame / (float) sequence->duration);
                    sceVu0AddVector(position, start_position, offset);
                }

                break;
        }

        break;
    }

    if (move_head == NULL) {
        move_tail = NULL;
    }

    while ((sequence = rotation_head) != NULL) {
        if (sequence->operation == ACT_SEQ_SET_ROTATION) {
            sceVu0CopyVector(rotation, sequence->arguments.vector);
            sceVu0CopyVector(start_rotation, sequence->arguments.vector);
            rotation_head = DeleteSeq(sequence);
            rotation_frame = 0;
            continue;
        }

        rotation_frame++;

        if (rotation_frame >= sequence->duration) {
            sceVu0CopyVector(rotation, sequence->arguments.vector);
            sceVu0CopyVector(start_rotation, sequence->arguments.vector);
            rotation_head = DeleteSeq(sequence);
            rotation_frame = 0;
        } else {
            sceVu0FVECTOR offset;

            sceVu0SubVector(offset, sequence->arguments.vector, start_rotation);
            sceVu0ScaleVector(offset, offset, (float) rotation_frame / (float) sequence->duration);
            sceVu0AddVector(rotation, start_rotation, offset);
        }

        break;
    }

    if (rotation_mode != 0 && rotation_delay == 0) {
        sceVu0FVECTOR direction = {0.0f, 0.0f, 0.0f, 0.0f};

        if (rotation_mode == 1) {
            sceVu0SubVector(direction, rotation_target, position);
        }

        if (rotation_mode == 4) {
            sceVu0CopyVector(direction, rotation_target);
        }

        if (rotation_mode == 2) {
            sceVu0SubVector(direction, position, start_position);
        }

        if (rotation_mode == 3) {
            rotation[1] = AngleInterpolate(rotation[1], rotation_target[1], rotation_target[3], INTERPOLATE_STEP);

            if (AngleCmp(rotation[1], rotation_target[1], 0.001f) == 0) {
                rotation_complete = 1;
            }
        } else if (direction[0] != 0.0f || direction[2] != 0.0f) {
            float target_yaw = atan2f(direction[0], direction[2]);

            rotation[1] = AngleInterpolate(rotation[1], target_yaw, rotation_target[3], INTERPOLATE_STEP);

            if (AngleCmp(rotation[1], target_yaw, 0.001f) == 0) {
                rotation_complete = 1;
            }
        } else {
            rotation_complete = 1;
        }
    }

    rotation_delay--;

    if (rotation_delay < 0) {
        rotation_delay = 0;
    }

    if (character != NULL) {
        float motion_time = character->GetNowTime();

        if (motion_head != NULL && motion_head->arguments.animation.id == character->motion_no) {
            MOTION_INFO *motion_info = character->GetMotionInfo(motion_head->arguments.animation.id);

            if (motion_info != NULL && motion_head->arguments.animation.mode == 7) {
                // The last step is the one the character plays at, once its speed is set.
                float step = motion_info->speed;

                if (character->motion_speed > 0.0f) {
                    step = character->motion_speed;
                }

                if (!(motion_info->end <= motion_time) && motion_info->start < motion_time && motion_info->end - step - 0.01f <= motion_time) {
                    int             motion_index;
                    tagMOTION_TYPE *motion_type = character->GetMotionParam(character->motion_no, &motion_index, NULL, NULL, NULL);

                    if (motion_type != NULL) {
                        motion_type->state.time = (int) (0.999f + motion_type->state.time);
                    }

                    motion_head = DeleteSeq(motion_head);

                    if (motion_head == NULL) {
                        motion_tail = NULL;
                    }
                }
            }
        }
    }

    if (motion_trigger != 0 && motion_delay <= 0) {
        ACT_SEQ *queued_motion = motion_head;

        if (queued_motion != NULL && character != NULL) {
            int flags = 0;

            switch (queued_motion->arguments.animation.mode) {
                case 6:
                case 7:
                    flags = 2;
            }

            if (character->motion_no != queued_motion->arguments.animation.id) {
                flags |= queued_motion->arguments.animation.flags;
            }

            character->SetMotion(queued_motion->arguments.animation.id, flags);
            character->SetMotionSpeed(queued_motion->arguments.animation.playback.speed);
        }
    }

    if (anime_trigger != 0 && anime_delay <= 0 && character != NULL) {
        animation_shown = 0;

        while ((sequence = anime_head) != NULL) {
            switch (sequence->operation) {
                case ACT_SEQ_ANIMATION:
                    if (sequence->duration == 0) {
                        if (sequence->arguments.animation.id == -1 && sequence->arguments.animation.mode == 0) {
                            character->ClearTexAnime();
                        }

                        if (sequence->arguments.animation.mode != 0) {
                            character->TexAnimeOn(sequence->arguments.animation.id);
                        } else {
                            character->TexAnimeOff(sequence->arguments.animation.id);
                        }

                        anime_head = DeleteSeq(sequence);
                        anime_frame = 0;
                        continue;
                    }

                    if (anime_frame >= sequence->duration) {
                        if (sequence->arguments.animation.playback.disable_after != 0) {
                            character->TexAnimeOff(sequence->arguments.animation.id);
                        }

                        anime_head = DeleteSeq(sequence);
                        anime_frame = 0;
                        continue;
                    }

                    if (sequence->arguments.animation.id == -1 && sequence->arguments.animation.mode == 0) {
                        character->ClearTexAnime();
                    }

                    if (sequence->arguments.animation.mode != 0) {
                        character->TexAnimeOn(sequence->arguments.animation.id);
                    } else {
                        character->TexAnimeOff(sequence->arguments.animation.id);
                    }

                    animation_shown = 1;
                    break;
                default:
                    anime_head = DeleteSeq(sequence);
                    break;
            }

            if (animation_shown != 0) {
                break;
            }
        }

        anime_frame++;

        if (anime_head == NULL) {
            anime_tail = NULL;
            anime_trigger = 0;
        }
    }

    motion_delay--;

    if (motion_delay < 0) {
        motion_delay = 0;
    }

    anime_delay--;

    if (anime_delay < 0) {
        anime_delay = 0;
    }

    if (character != NULL) {
        sceVu0FVECTOR wrapped_rotation;
        float         turns;
        int           whole_turns;

        character->SetPosition(position);
        wrapped_rotation[0] = rotation[0];
        wrapped_rotation[1] = rotation[1];
        wrapped_rotation[2] = rotation[2];
        wrapped_rotation[3] = 0.0f;
        // Wrap the yaw into [-pi, pi]; the first truncation's result is unused.
        turns = wrapped_rotation[1] / TWO_PI;
        whole_turns = (int) turns;
        wrapped_rotation[1] -= PI * (2.0f * (int) turns);

        if (wrapped_rotation[1] > PI) {
            wrapped_rotation[1] -= TWO_PI;
        }

        if (wrapped_rotation[1] < -PI) {
            wrapped_rotation[1] += TWO_PI;
        }

        character->SetRotation(wrapped_rotation);
    }
}
