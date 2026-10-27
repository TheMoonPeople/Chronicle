#include "actionseq.hpp"
#include "sce/libvu0.h"

#include <algorithm>
#include <cmath>

#include "character.hpp"
#include "mathutil.hpp"

namespace dc::game {

void CActionSeq::Initialize(ACT_SEQ *records, int count) {
    ClearSeq();
    pool = records;
    capacity = count;
}

CActionSeq::CActionSeq() {
    Initialize(nullptr, 0);
}

void CActionSeq::ClearSeq() {
    character = nullptr;
    move_tail = nullptr;
    move_head = nullptr;
    move_frame = 0;
    rotation_mode = ActSeqRotationMode::None;
    rotation_complete = 0;
    rotation_tail = nullptr;
    rotation_head = nullptr;
    rotation_frame = 0;
    motion_tail = nullptr;
    motion_head = nullptr;
    anime_tail = nullptr;
    anime_head = nullptr;
    rotation_delay = 0;
    anime_frame = 0;
    motion_trigger = 0;
    motion_delay = 0;
    anime_trigger = 0;
    anime_delay = 0;
}

ACT_SEQ *CActionSeq::GetNextSeq() {
    for (int index = 0; index < capacity; index++) {
        ACT_SEQ &sequence = pool[index];

        if (sequence.operation == ACT_SEQ_UNUSED) {
            sequence.next = nullptr;
            return &sequence;
        }
    }

    return nullptr;
}

void CActionSeq::SyncChara(CCharacter *target) {
    sceVu0FVECTOR location;
    sceVu0FVECTOR angles;
    character = target;

    if (target != nullptr) {
        target->GetPosition(location);
        target->GetRotation(angles);
        SetPos(location);
        SetRot(angles);
    }
}

ACT_SEQ *CActionSeq::NextMoveSeq() {
    ACT_SEQ *sequence = GetNextSeq();

    if (sequence == nullptr) {
        return nullptr;
    }

    if (move_tail != nullptr) {
        move_tail->next = sequence;
    }

    move_tail = sequence;
    sequence->next = nullptr;

    if (move_head == nullptr) {
        move_head = sequence;
    }

    return sequence;
}

ACT_SEQ *CActionSeq::NextMotionSeq() {
    ACT_SEQ *sequence = GetNextSeq();

    if (sequence == nullptr) {
        return nullptr;
    }

    if (motion_tail != nullptr) {
        motion_tail->next = sequence;
    }

    motion_tail = sequence;
    sequence->next = nullptr;

    if (motion_head == nullptr) {
        motion_head = sequence;
    }

    return sequence;
}

ACT_SEQ *CActionSeq::NextAnimeSeq() {
    ACT_SEQ *sequence = GetNextSeq();

    if (sequence == nullptr) {
        return nullptr;
    }

    if (anime_tail != nullptr) {
        anime_tail->next = sequence;
    }

    anime_tail = sequence;
    sequence->next = nullptr;

    if (anime_head == nullptr) {
        anime_head = sequence;
    }

    return sequence;
}

void CActionSeq::MoveSeq(float *destination, int frames) {
    ACT_SEQ *sequence = NextMoveSeq();

    if (sequence != nullptr) {
        sequence->operation = ACT_SEQ_MOVE;
        sceVu0CopyVector(sequence->arguments.vector, destination);
        sceVu0CopyVector(queued_position, destination);
        sequence->duration = frames;
    }
}

void CActionSeq::MoveSeq(float *destination, float speed) {
    float duration = DistVector(destination, queued_position) / speed;
    int   frames = static_cast<int>(duration);

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

    if (sequence != nullptr) {
        sequence->operation = ACT_SEQ_SET_POSITION;
        sceVu0CopyVector(sequence->arguments.vector, destination);
        sceVu0CopyVector(queued_position, destination);
        sequence->duration = 0;
    }
}

void CActionSeq::RotRefSeq(float *reference, float speed) {
    ACT_SEQ *sequence = NextMoveSeq();

    if (sequence != nullptr) {
        sequence->operation = ACT_SEQ_ROTATE_REFERENCE;
        sceVu0CopyVector(sequence->arguments.vector, reference);
        sequence->arguments.vector[3] = speed;
        sequence->duration = 0;
    }
}

void CActionSeq::RotAngleSeq(float angle, float speed) {
    ACT_SEQ *sequence = NextMoveSeq();

    if (sequence != nullptr) {
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

    if (sequence != nullptr) {
        sequence->operation = ACT_SEQ_ROTATE_MOVEMENT;
        sequence->arguments.vector[3] = speed;
        sequence->duration = 0;
    }
}

void CActionSeq::ClearRotSeq() {
    ACT_SEQ *sequence = NextMoveSeq();

    if (sequence != nullptr) {
        sequence->operation = ACT_SEQ_CLEAR_ROTATION;
        sequence->duration = 0;
    }
}

void CActionSeq::WaitRotSeq() {
    ACT_SEQ *sequence = NextMoveSeq();

    if (sequence != nullptr) {
        sequence->operation = ACT_SEQ_WAIT_ROTATION;
        sequence->duration = 0;
    }
}

void CActionSeq::SetRot(float *angles) {
    ACT_SEQ *sequence = NextMoveSeq();

    if (sequence != nullptr) {
        sequence->operation = ACT_SEQ_SET_ROTATION;
        sceVu0CopyVector(sequence->arguments.vector, angles);
        sequence->duration = 0;
    }
}

void CActionSeq::SetDelayRot(int delay) {
    ACT_SEQ *sequence = NextMoveSeq();

    if (sequence != nullptr) {
        sequence->operation = ACT_SEQ_DELAY_ROTATION;
        sequence->duration = delay;
    }
}

void CActionSeq::MotionTrg(int delay) {
    ACT_SEQ *sequence = NextMoveSeq();

    if (sequence != nullptr) {
        sequence->operation = ACT_SEQ_TRIGGER_MOTION;
        sequence->duration = delay;
    }
}

void CActionSeq::AnimeTrg(int delay) {
    ACT_SEQ *sequence = NextMoveSeq();

    if (sequence != nullptr) {
        sequence->operation = ACT_SEQ_TRIGGER_ANIMATION;
        sequence->duration = delay;
    }
}

void CActionSeq::MotionSeq(int motion, int mode, float speed, int flags) {
    ACT_SEQ *sequence = NextMotionSeq();

    if (sequence != nullptr) {
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

    if (sequence != nullptr) {
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
    bool motion_finished = false;

    if (motion_head != nullptr) {
        if (motion_head->arguments.animation.mode != 7) {
            motion_finished = true;
        }
    } else {
        motion_finished = true;
    }

    return move_head == nullptr && motion_finished && anime_head == nullptr;
}

namespace {

/**
 * Releases an action record and returns the next queued record.
 */
ACT_SEQ *DeleteSeq(ACT_SEQ *sequence) {
    if (sequence == nullptr) {
        return nullptr;
    }

    ACT_SEQ *next = sequence->next;
    sequence->operation = ACT_SEQ_UNUSED;
    sequence->next = nullptr;
    return next;
}

} // namespace

void CActionSeq::Play() {
    if (move_head == nullptr && motion_head == nullptr && anime_head == nullptr) {
        return;
    }

    for (ACT_SEQ *sequence = move_head; sequence != nullptr; sequence = move_head) {
        switch (sequence->operation) {
            case ACT_SEQ_TRIGGER_MOTION:
                if (motion_trigger != 0) {
                    motion_head = DeleteSeq(motion_head);

                    if (motion_head == nullptr) {
                        motion_tail = nullptr;
                    }
                }

                if (motion_head != nullptr) {
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
                rotation_mode = ActSeqRotationMode::FaceReference;
                rotation_complete = 0;
                sceVu0CopyVector(rotation_target, sequence->arguments.vector);
                move_head = DeleteSeq(sequence);
                continue;
            case ACT_SEQ_ROTATE_ANGLE:
                rotation_mode = ActSeqRotationMode::TurnToAngle;
                rotation_complete = 0;
                sceVu0CopyVector(rotation_target, sequence->arguments.vector);
                move_head = DeleteSeq(sequence);
                continue;
            case ACT_SEQ_DELAY_ROTATION:
                rotation_delay = sequence->duration;
                move_head = DeleteSeq(sequence);
                continue;
            case ACT_SEQ_ROTATE_MOVEMENT:
                rotation_mode = ActSeqRotationMode::FaceMovement;
                rotation_complete = 0;
                sceVu0CopyVector(rotation_target, sequence->arguments.vector);
                move_head = DeleteSeq(sequence);
                continue;
            case ACT_SEQ_CLEAR_ROTATION:
                rotation_mode = ActSeqRotationMode::None;
                rotation_complete = 0;
                move_head = DeleteSeq(sequence);
                continue;
            case ACT_SEQ_WAIT_ROTATION:
                if (rotation_complete != 0) {
                    rotation_mode = ActSeqRotationMode::None;
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
                    sceVu0ScaleVector(offset, offset, static_cast<float>(move_frame) / static_cast<float>(sequence->duration));
                    sceVu0AddVector(position, start_position, offset);
                }

                break;
            default:
                break;
        }

        break;
    }

    if (move_head == nullptr) {
        move_tail = nullptr;
    }

    for (ACT_SEQ *sequence = rotation_head; sequence != nullptr; sequence = rotation_head) {
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
            sceVu0ScaleVector(offset, offset, static_cast<float>(rotation_frame) / static_cast<float>(sequence->duration));
            sceVu0AddVector(rotation, start_rotation, offset);
        }

        break;
    }

    if (rotation_mode != ActSeqRotationMode::None && rotation_delay == 0) {
        sceVu0FVECTOR direction = {0.0f, 0.0f, 0.0f, 0.0f};

        if (rotation_mode == ActSeqRotationMode::FaceReference) {
            sceVu0SubVector(direction, rotation_target, position);
        }

        if (rotation_mode == ActSeqRotationMode::FaceDirection) {
            sceVu0CopyVector(direction, rotation_target);
        }

        if (rotation_mode == ActSeqRotationMode::FaceMovement) {
            sceVu0SubVector(direction, position, start_position);
        }

        if (rotation_mode == ActSeqRotationMode::TurnToAngle) {
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
    rotation_delay = std::max(rotation_delay, 0);

    if (character != nullptr) {
        float motion_time = character->GetNowTime();

        if (motion_head != nullptr && motion_head->arguments.animation.id == character->motion_no) {
            MOTION_INFO *motion_info = character->GetMotionInfo(motion_head->arguments.animation.id);

            if (motion_info != nullptr && motion_head->arguments.animation.mode == 7) {
                // The last step is the one the character plays at, once its speed is set.
                float step = motion_info->speed;

                if (character->motion_speed > 0.0f) {
                    step = character->motion_speed;
                }

                if (!(motion_info->end <= motion_time) && motion_info->start < motion_time && motion_info->end - step - 0.01f <= motion_time) {
                    int             motion_index = 0;
                    tagMOTION_TYPE *motion_type = character->GetMotionParam(character->motion_no, &motion_index, nullptr, nullptr, nullptr);

                    if (motion_type != nullptr) {
                        motion_type->state.time = static_cast<int>(0.999f + motion_type->state.time);
                    }

                    motion_head = DeleteSeq(motion_head);

                    if (motion_head == nullptr) {
                        motion_tail = nullptr;
                    }
                }
            }
        }
    }

    if (motion_trigger != 0 && motion_delay <= 0) {
        ACT_SEQ *queued_motion = motion_head;

        if (queued_motion != nullptr && character != nullptr) {
            int flags = 0;

            if (queued_motion->arguments.animation.mode == 6 || queued_motion->arguments.animation.mode == 7) {
                flags = 2;
            }

            if (character->motion_no != queued_motion->arguments.animation.id) {
                flags |= queued_motion->arguments.animation.flags;
            }

            character->SetMotion(queued_motion->arguments.animation.id, flags);
            character->SetMotionSpeed(queued_motion->arguments.animation.playback.speed);
        }
    }

    if (anime_trigger != 0 && anime_delay <= 0 && character != nullptr) {
        for (ACT_SEQ *sequence = anime_head; sequence != nullptr; sequence = anime_head) {
            if (sequence->operation != ACT_SEQ_ANIMATION) {
                anime_head = DeleteSeq(sequence);
                continue;
            }

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

            break;
        }

        anime_frame++;

        if (anime_head == nullptr) {
            anime_tail = nullptr;
            anime_trigger = 0;
        }
    }

    motion_delay--;
    motion_delay = std::max(motion_delay, 0);

    anime_delay--;
    anime_delay = std::max(anime_delay, 0);

    if (character != nullptr) {
        sceVu0FVECTOR wrapped_rotation;

        character->SetPosition(position);
        wrapped_rotation[0] = rotation[0];
        wrapped_rotation[1] = rotation[1];
        wrapped_rotation[2] = rotation[2];
        wrapped_rotation[3] = 0.0f;
        // Wrap the yaw into [-pi, pi].
        float turns = wrapped_rotation[1] / TWO_PI;
        wrapped_rotation[1] -= PI * (2.0f * static_cast<float>(static_cast<int>(turns)));

        if (wrapped_rotation[1] > PI) {
            wrapped_rotation[1] -= TWO_PI;
        }

        if (wrapped_rotation[1] < -PI) {
            wrapped_rotation[1] += TWO_PI;
        }

        character->SetRotation(wrapped_rotation);
    }
}

} // namespace dc::game
