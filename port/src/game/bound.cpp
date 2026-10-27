#include "bound.hpp"

#include "chararead.hpp"
#include "frame.hpp"

namespace dc::game {

namespace {

// A non-positive extent keeps its previous reciprocal.
void SetExtent(CBound &bound, float half_width, float half_height, float half_depth) {
    bound.extent[0] = half_width;
    bound.extent[1] = half_height;
    bound.extent[2] = half_depth;

    if (!(bound.extent[0] <= 0.0f)) {
        bound.reciprocal[0] = 1.0f / half_width;
    }

    if (!(bound.extent[1] <= 0.0f)) {
        bound.reciprocal[1] = 1.0f / half_height;
    }

    if (!(bound.extent[2] <= 0.0f)) {
        bound.reciprocal[2] = 1.0f / half_depth;
    }
}

} // namespace

int CBound::InCheck(float *point, float *result) {
    result[0] = point[0] - position[0];
    result[1] = point[1] - position[1];
    result[2] = point[2] - position[2];
    result[3] = 0.0f;
    sceVu0ApplyMatrix(result, inverse, result);

    if (result[0] > extent[0]) {
        return 0;
    }

    if (result[1] > extent[1]) {
        return 0;
    }

    if (result[2] > extent[2]) {
        return 0;
    }

    if (result[0] < -extent[0]) {
        return 0;
    }

    if (result[1] < -extent[1]) {
        return 0;
    }

    if (result[2] < -extent[2]) {
        return 0;
    }

    result[0] = result[0] * reciprocal[0];
    result[1] = result[1] * reciprocal[1];
    result[2] = result[2] * reciprocal[2];
    float length = vuabs(result);

    if (length < 1.0f) {
        if (length == 0.0f) {
            result[0] = extent[0];
            result[1] = extent[1];
            result[2] = extent[2];
        } else {
            result[0] = result[0] * extent[0] / length;
            result[1] = result[1] * extent[1] / length;
            result[2] = result[2] * extent[2] / length;
        }

        result[3] = 0.0f;
        sceVu0ApplyMatrix(result, matrix, result);
        result[0] += position[0];
        result[1] += position[1];
        result[2] += position[2];
        result[3] = 1.0f;
        return 1;
    }

    return 0;
}

void CBound::SetDir(CFrame *frame, float *from_position, float *to_position, float *up_direction, float half_width, float half_height) {
    follow_mode = BoundFollowMode::Frame;
    from_frame = frame;
    to_frame = nullptr;
    ChangeDir(from_position, to_position, up_direction);
    SetExtent(*this, half_width, half_height, extent[2]);
}

void CBound::ChangeDir(float *from_position, float *to_position, float *up_direction) {
    sceVu0CopyVector(from, from_position);
    sceVu0CopyVector(to, to_position);
    sceVu0CopyVector(up, up_direction);
}

void CBound::UpDateDir() {
    sceVu0FVECTOR span;
    sceVu0FVECTOR world_from;
    sceVu0FVECTOR world_to;
    sceVu0FVECTOR world_up;
    sceVu0FMATRIX frame_matrix;

    if (from_frame != nullptr) {
        from_frame->GetLWMatrix(frame_matrix);
        sceVu0ApplyMatrix(world_from, frame_matrix, from);
        sceVu0ApplyMatrix(world_to, frame_matrix, to);
        up[3] = 0.0f;
        sceVu0ApplyMatrix(world_up, frame_matrix, up);
        sceVu0SubVector(span, world_from, world_to);
        position[0] = (world_from[0] + world_to[0]) / 2.0f;
        position[1] = (world_from[1] + world_to[1]) / 2.0f;
        position[2] = (world_from[2] + world_to[2]) / 2.0f;
    } else {
        sceVu0CopyVector(world_up, up);
        sceVu0SubVector(span, from, to);
        position[0] = (from[0] + to[0]) / 2.0f;
        position[1] = (from[1] + to[1]) / 2.0f;
        position[2] = (from[2] + to[2]) / 2.0f;
    }

    SetDir(span, world_up);
    SetExtent(*this, extent[0], extent[1], extent[2]);
}

void CBound::SetDir(float *direction, float *up_direction) {
    sceVu0FVECTOR origin = {0.0f, 0.0f, 0.0f, 1.0f};

    sceVu0CameraMatrix(inverse, origin, direction, up_direction);
    sceVu0TransposeMatrix(matrix, inverse);
}

void CBound::SetDir(float *new_direction) {
    sceVu0Normalize(direction, new_direction);
    float length = vuabs(direction);
    float saved_y = direction[1];
    direction[1] = 0.0f;
    float horizontal_length = vuabs(direction);
    direction[1] = saved_y;

    float horizontal_sine = 0.0f;
    float horizontal_cosine = 1.0f;

    if (horizontal_length != 0.0f) {
        horizontal_cosine = direction[2] / horizontal_length;
        horizontal_sine = -direction[0] / horizontal_length;
    }

    if (length == 0.0f) {
        length = 1.0f;
    }

    float horizontal_ratio = horizontal_length / length;
    float vertical_ratio = direction[1] / length;

    sceVu0UnitMatrix(inverse);
    inverse[0][0] = horizontal_cosine;
    inverse[0][1] = -horizontal_sine * vertical_ratio;
    inverse[0][2] = horizontal_sine * horizontal_ratio;
    inverse[0][3] = 0.0f;
    inverse[1][0] = 0.0f;
    inverse[1][1] = -horizontal_ratio;
    inverse[1][2] = -vertical_ratio;
    inverse[1][3] = 0.0f;
    inverse[2][0] = horizontal_sine;
    inverse[2][1] = horizontal_cosine * vertical_ratio;
    inverse[2][2] = -horizontal_cosine * horizontal_ratio;
    inverse[2][3] = 0.0f;
    sceVu0TransposeMatrix(matrix, inverse);
}

void CBound::UpDateDirPos() {
    sceVu0FMATRIX frame_matrix;
    sceVu0FVECTOR world_from;
    sceVu0FVECTOR world_to;
    sceVu0FVECTOR span;
    sceVu0FVECTOR end_extension;
    sceVu0FVECTOR start_extension;

    if (from_frame != nullptr) {
        from[3] = 1.0f;
        from_frame->GetLWMatrix(frame_matrix);
        sceVu0ApplyMatrix(world_from, frame_matrix, from);
    } else {
        sceVu0CopyVector(world_from, from);
    }

    if (to_frame != nullptr) {
        to[3] = 1.0f;
        to_frame->GetLWMatrix(frame_matrix);
        sceVu0ApplyMatrix(world_to, frame_matrix, to);
    } else {
        sceVu0CopyVector(world_to, to);
    }

    position[0] = (world_from[0] + world_to[0]) * 0.5f;
    position[1] = (world_from[1] + world_to[1]) * 0.5f;
    position[2] = (world_from[2] + world_to[2]) * 0.5f;
    sceVu0SubVector(span, world_to, world_from);
    sceVu0ScaleVector(end_extension, span, (to_stretch - 1.0f) * 0.5f);
    sceVu0ScaleVector(start_extension, span, (from_stretch - 1.0f) * 0.5f);
    sceVu0AddVector(world_to, world_to, end_extension);
    sceVu0SubVector(world_from, world_from, start_extension);
    sceVu0SubVector(span, world_to, world_from);

    SetExtent(*this, extent[0], extent[1], extent[2]);
    SetDir(span);
}

void CBound::UpDate() {
    switch (follow_mode) {
        case BoundFollowMode::None:
            break;
        case BoundFollowMode::Frame:
            UpDateDir();
            break;
        case BoundFollowMode::Span:
            UpDateDirPos();
            break;
    }

    if (next != nullptr) {
        next->UpDate();
    }
}

void CBound::InitParam() {
    position[0] = 0.0f;
    position[1] = 0.0f;
    position[2] = 0.0f;
    position[3] = 1.0f;
    next = nullptr;
    mask_bit = 0;
    friction = 0.5f;
    from_frame = to_frame = nullptr;
    sceVu0CopyVector(from, position);
    sceVu0CopyVector(to, position);
    to_stretch = from_stretch = 1.0f;
}

CBound::CBound(float half_width, float half_height, float half_depth) {
    InitParam();
    SetExtent(*this, half_width, half_height, half_depth);

    sceVu0FVECTOR forward = {0.0f, 0.0f, 1.0f, 0.0f};
    SetDir(forward);
}

} // namespace dc::game
