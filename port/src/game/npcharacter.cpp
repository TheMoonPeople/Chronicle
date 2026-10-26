#include "npcharacter.hpp"

#include <cmath>

#include "collision.hpp"
#include "mathutil.hpp"
#include "mglib.hpp"

void CNPCharacter::Step() {
    if (!initialized || frame == NULL) {
        return;
    }

    if (near_camera || step_hidden) {
        CCharacter::Step();
    }

    int fade_step = alpha_step;
    int override_step = alpha_step_override;

    if (!(float(override_step) <= 0.0f)) {
        fade_step = override_step;
    }

    alpha_step_override = -1;

    if (near_camera) {
        ambient_offset[3] += float(fade_step);
    } else {
        ambient_offset[3] -= float(fade_step);
    }

    if (ambient_offset[3] < 0.0f) {
        ambient_offset[3] = 0;
    }

    if (!(ambient_offset[3] <= 128.0f)) {
        ambient_offset[3] = 128.0f;
    }

    PlaySeq();
}

void CNPCharacter::ShadowStep() {
    if (!initialized || frame == NULL) {
        return;
    }

    if (near_camera) {
        CCharacter::ShadowStep();
    }
}

void CNPCharacter::PlaySeq() {
    sceVu0FVECTOR position;
    sceVu0FVECTOR movement;
    sceVu0FVECTOR destination;
    sceVu0FVECTOR flat_position;

    if (!CheckSeq() || !sequence_enabled) {
        return;
    }

    NP_SEQUENCE *sequence = GetNowSeq();
    GetPosition(position);

    switch (sequence->operation) {
        case NP_SEQUENCE_MOVE: {
            sceVu0SubVector(movement, sequence->destination, position);
            movement[1] = 0;
            sceVu0Normalize(movement, movement);
            sceVu0ScaleVector(movement, movement, sequence->speed[0]);
            sceVu0CopyVector(flat_position, position);
            flat_position[1] = sequence->destination[1];

            if (DistVector(sequence->destination, flat_position) <= sequence->speed[0]) {
                sceVu0CopyVector(destination, sequence->destination);
                NextSeq();
                SetMotion(0, 0);
            } else {
                sceVu0AddVector(destination, position, movement);
            }

            float angle = AngleInterpolate(rotation.y, atan2f(movement[0], movement[2]), 0.1f, INTERPOLATE_STEP);
            SetRotation(0, angle, 0);
            SetPosition(destination);
            SetMotion(1, 0);
            return;
        }
        case NP_SEQUENCE_WAIT:
            sequence->wait_frames--;

            if (sequence->wait_frames < 0) {
                NextSeq();
            }

            SetMotion(0, 0);
            break;
    }
}

void CNPCharacter::ClearSeq() {
    read_index = 0;
    write_index = 0;
    sequence_enabled = 1;

    for (int index = 0; index < 8; index++) {
        sequences[index].operation = NP_SEQUENCE_UNUSED;
    }
}

int CNPCharacter::SetSeq(float *destination, float speed) {
    NP_SEQUENCE *sequence = GetNextSeq();
    sequence->operation = NP_SEQUENCE_MOVE;
    sceVu0CopyVector(sequence->destination, destination);
    sequence->speed[0] = speed;
    sequence->speed[1] = speed;
    sequence->speed[2] = speed;
    return 1;
}

int CNPCharacter::SetWait(int frames) {
    NP_SEQUENCE *sequence = GetNextSeq();
    sequence->operation = NP_SEQUENCE_WAIT;
    sequence->wait_frames = frames;
    return 1;
}

int CNPCharacter::CheckSeq() {
    return read_index != write_index;
}

NP_SEQUENCE *CNPCharacter::GetNextSeq() {
    NP_SEQUENCE *sequence = &sequences[write_index];
    write_index++;

    if (write_index >= 8) {
        write_index = 0;
    }

    return sequence;
}

NP_SEQUENCE *CNPCharacter::GetNowSeq() {
    return &sequences[read_index];
}

void CNPCharacter::NextSeq() {
    GetNowSeq()->operation = NP_SEQUENCE_UNUSED;

    if (CheckSeq()) {
        read_index++;

        if (read_index >= 8) {
            read_index = 0;
        }
    }
}

void CNPCharacter::Draw() {
    sceVu0FVECTOR saved_ambient;
    sceVu0FVECTOR ambient;

    if (!initialized || frame == NULL) {
        return;
    }

    if (!(ambient_offset[3] <= 0.0f)) {
        MGGetAmbient(saved_ambient);
        sceVu0CopyVector(ambient, saved_ambient);
        ambient[0] += ambient_offset[0];
        ambient[1] += ambient_offset[1];
        ambient[2] += ambient_offset[2];
        ambient[3] = ambient_offset[3];
        MGSetAmbient(ambient);
        CCharacter::Draw();
        MGSetAmbient(saved_ambient);
    }
}

void CNPCharacter::DrawShadow() {
    if (!initialized || frame == NULL) {
        return;
    }

    if (near_camera) {
        CCharacter::DrawShadow();
    }
}

int CNPCharacter::CheckDraw() {
    if (!initialized || frame == NULL) {
        return 0;
    }

    if (ambient_offset[3] <= 0.0f) {
        return 0;
    }

    return 1;
}

int CNPCharacter::PickUpPoly(float *position, CCPoly *polygons) {
    if (initialized) {
        return CCharacter::PickUpPoly(position, polygons);
    }

    return 0;
}

int CCharacter::PickUpPoly(float *position, CCPoly *polygons) {
    sceVu0FVECTOR origin;
    sceVu0FVECTOR direction;
    sceVu0FVECTOR centre;
    sceVu0FVECTOR upper_left;
    sceVu0FVECTOR upper_right;
    sceVu0FVECTOR lower_left;
    sceVu0FVECTOR lower_right;
    GetPosition(origin);
    float radius = body_width;

    if (radius < 2.0f) {
        radius = 2.0f;
    }

    sceVu0SubVector(direction, position, origin);
    direction[1] = 0;
    float distance = DistVector(direction);

    if (!(distance <= 4.0f * radius) || distance < radius) {
        return 0;
    }

    if (origin[1] + body_height < position[1]) {
        return 0;
    }

    if (!(origin[1] - body_height <= position[1])) {
        return 0;
    }

    sceVu0Normalize(direction, direction);
    upper_left[0] = 20.0f * -direction[2];
    upper_left[1] = 30.0f;
    upper_left[2] = 20.0f * direction[0];
    upper_left[3] = 1.0f;
    sceVu0CopyVector(upper_right, upper_left);
    upper_right[0] = -upper_left[0];
    upper_right[2] = -upper_left[2];
    sceVu0CopyVector(lower_left, upper_left);
    lower_left[1] = -30.0f;
    sceVu0CopyVector(lower_right, upper_right);
    lower_right[1] = -30.0f;
    sceVu0ScaleVector(centre, direction, radius);
    sceVu0AddVector(centre, centre, origin);
    sceVu0AddVector(upper_left, upper_left, centre);
    sceVu0AddVector(upper_right, upper_right, centre);
    sceVu0AddVector(lower_left, lower_left, centre);
    sceVu0AddVector(lower_right, lower_right, centre);
    sceVu0CopyVector(polygons[0].vertex[0], upper_left);
    sceVu0CopyVector(polygons[0].vertex[1], upper_right);
    sceVu0CopyVector(polygons[0].vertex[2], lower_left);
    sceVu0CopyVector(polygons[0].normal, direction);
    sceVu0CopyVector(polygons[1].vertex[0], lower_left);
    sceVu0CopyVector(polygons[1].vertex[1], upper_right);
    sceVu0CopyVector(polygons[1].vertex[2], lower_right);
    sceVu0CopyVector(polygons[1].normal, direction);
    return 2;
}

void CNPCharacter::Initialize() {
    CCharacter::Initialize();
    initialized = false;
    near_camera = false;
    ambient_offset[0] = 0;
    ambient_offset[1] = 0;
    ambient_offset[2] = 0;
    ambient_offset[3] = 0;
    alpha_step = 0;
    alpha_step_override = -1;
    body_width = 7.0f;
    texture_block = 0;
    talk_target = false;
    event_status = 0;
    draw_enabled = false;
    resource_name[0] = 0;
    ClearSeq();
    map_parts_no = 0;
    unk_1480 = 0;
    step_hidden = 0;
    recurring_talk_event = -1;
}

CNPCharacter::CNPCharacter() {
    Initialize();
}
