#include "stubs/gameutil.hpp"
#pragma helper_mask_gpr 0x30
#pragma helper_mask_fpr 0x1000
#pragma name_counter 25

#include "gameutil.hpp"

#include <eekernel.h>
#include <libgraph.h>
#include <libpkt.h>
#include <libvu0.h>
#include <sifdma.h>
#include <sifrpc.h>

#include <cmath>
#include <cstdio>
#include <cstring>

#include "camera.hpp"
#include "character.hpp"
#include "dataalloc.hpp"
#include "frame.hpp"
#include "framevu1.hpp"
#include "mathutil.hpp"
#include "mdt.hpp"
#include "mglib.hpp"
#include "rect.hpp"
#include "texture.hpp"
#include "visualvu1.hpp"

/* Shared helpers: the IOP midi bridge, motion interpolation, collision and
 * ground queries, and 2D sprite setup. */

static s32              sbuff[16]; // RPC argument and response storage shared by MIDI calls.
static sceSifClientData gCd;       // Client state bound to the EZMIDI IOP server.
static sceSifDmaData    transData; // Descriptor reused for synchronous EE-to-IOP transfers.

int ezMidiInit();

int ezMidi(int command, int argument);

int ezTransToIOP(void *iop_address, void *ee_address, int size);

/**
 * Interpolates between two quaternions along the shorter arc.
 *
 * @mangled QuatSlerp__FPfPffPf
 * @address 0x147B70
 * @size 0x1B0
 */
static void QuatSlerp(float *from, float *to, float t, float *out) {
    float to_x;
    float to_y;
    float to_z;
    float to_w;
    float cosine;
    float angle;
    float inv_sine;
    float scale_from;
    float scale_to;

    from[0] = -from[0];
    to[0] = -to[0];
    cosine = from[1] * to[1] + from[2] * to[2] + from[3] * to[3] + from[0] * to[0];

    if (cosine < 0.0f) {
        cosine = -cosine;
        to_x = -to[1];
        to_y = -to[2];
        to_z = -to[3];
        to_w = -to[0];
    } else {
        to_x = to[1];
        to_y = to[2];
        to_z = to[3];
        to_w = to[0];
    }

    if (1.0f - cosine > 0.001f) {
        angle = acosf(cosine);
        inv_sine = 1.0f / sinf(angle);
        scale_from = inv_sine * sinf((1.0f - t) * angle);
        scale_to = inv_sine * sinf(t * angle);
    } else {
        scale_from = 1.0f - t;
        scale_to = t;
    }

    out[1] = scale_from * from[1] + scale_to * to_x;
    out[2] = scale_from * from[2] + scale_to * to_y;
    out[3] = scale_from * from[3] + scale_to * to_z;
    out[0] = scale_from * from[0] + scale_to * to_w;
}

/**
 * Finds the last key of a driver that stands at or before a motion frame.
 */
static inline int SearchMotKey(Mot_List *list, u_int frame) {
    int middle;
    int low = 0;
    int high = list->key_count;

    while (low < high) {
        middle = (low + high) >> 1;

        if (list->keys[middle].frame <= frame) {
            low = middle + 1;
        } else {
            high = middle;
        }
    }

    return low - 1;
}

Mot_List *MotionProc(CFrame *frame, MOTION_STATE *state, Mot_List *list) {
    int   key;
    int   next;
    float t;

    key = SearchMotKey(list, state->frame);

    if (state->next_frame == state->frame + 1) {
        u_int time;
        u_int next_time;

        next = key + 1;
        next_time = list->keys[next].frame;
        time = list->keys[key].frame;

        if ((next_time != time + 1) != 0) {
            t = (state->time - (float) time) / ((float) next_time - (float) time);
        } else {
            t = state->blend;
        }
    } else {
        if (state->next_frame == list->keys[state->next_frame - 1].frame) {
            next = state->next_frame - 1;
        } else {
            int found = SearchMotKey(list, state->next_frame);

            next = found;
        }

        t = state->blend;
    }

    if (!(t < 0.0f) && t <= 1.0f && key < list->key_count && next < list->key_count) {
        float         one_minus_t;
        sceVu0FVECTOR value;
        sceVu0FVECTOR rotation;
        sceVu0FVECTOR from;
        sceVu0FVECTOR to;
        CFrameVu1    *target = &((CFrameVu1 *) frame)[list->frame];

        switch (list->type) {
            case MOTION_KEY_ROTATION:
                sceVu0CopyVector(from, list->keys[key].value);
                sceVu0CopyVector(to, list->keys[next].value);

                if (!(t <= 0.0001f) && t < 0.9999f) {
                    QuatSlerp(from, to, t, rotation);
                    target->SetTransMatrix(rotation);
                } else {
                    from[0] = -from[0];
                    to[0] = -to[0];

                    if (t <= 0.0001f) {
                        target->SetTransMatrix(from);
                    }

                    if (!(t < 0.9999f)) {
                        target->SetTransMatrix(to);
                    }
                }

                break;
            case MOTION_KEY_SCALE:
                if (!(t <= 0.0001f) && t < 0.9999f) {
                    sceVu0InterVectorXYZ(value, list->keys[next].value, list->keys[key].value, t);
                } else {
                    if (t <= 0.0001f) {
                        sceVu0CopyVectorXYZ(value, list->keys[key].value);
                    }

                    if (!(t < 0.9999f)) {
                        sceVu0CopyVectorXYZ(value, list->keys[next].value);
                    }
                }

                target->SetScale(value[0], value[1], value[2]);
                break;
            case MOTION_KEY_TRANSLATION:
                if (!(t <= 0.0001f) && t < 0.9999f) {
                    sceVu0InterVectorXYZ(value, list->keys[next].value, list->keys[key].value, t);
                } else {
                    if (t <= 0.0001f) {
                        sceVu0CopyVectorXYZ(value, list->keys[key].value);
                    }

                    if (!(t < 0.9999f)) {
                        sceVu0CopyVectorXYZ(value, list->keys[next].value);
                    }
                }

                target->local[3][0] = value[0];
                target->local[3][1] = value[1];
                target->local[3][2] = value[2];
                target->world_valid = false;
                break;
            case MOTION_KEY_VERTEX: {
                int            vertex;
                MDT_HEADER    *model = (MDT_HEADER *) target->GetVisual()->GetMDTDataAddress();
                sceVu0FVECTOR *vertices = (sceVu0FVECTOR *) ((u_char *) model + model->vertex_ofs);
                u_int          frame_no;

                target->attr.remake_pending = 1;
                frame_no = list->frame;

                if (!(t <= 0.0001f) && t < 0.9999f) {
                    while (frame_no == list->frame) {
                        vertex = list->target - 1;
                        sceVu0InterVectorXYZ(value, list->keys[next].value, list->keys[key].value, t);
                        sceVu0CopyVectorXYZ(vertices[vertex], value);
                        list = list->next;

                        switch ((int) list) {
                            case 0:
                                return NULL;
                        }
                    }
                } else {
                    if (t <= 0.0001f) {
                        while (frame_no == list->frame) {
                            vertex = list->target - 1;
                            sceVu0CopyVectorXYZ(vertices[vertex], list->keys[key].value);
                            list = list->next;

                            switch ((int) list) {
                                case 0:
                                    return NULL;
                            }
                        }
                    }

                    if (!(t < 0.9999f)) {
                        while (frame_no == list->frame) {
                            vertex = list->target - 1;
                            sceVu0CopyVectorXYZ(vertices[vertex], list->keys[next].value);
                            list = list->next;

                            switch ((int) list) {
                                case 0:
                                    return NULL;
                            }
                        }
                    }
                }

                return list;
            }
            case MOTION_KEY_CAMERA_POSITION:
                if (state->camera != NULL) {
                    sceVu0InterVectorXYZ(value, list->keys[next].value, list->keys[key].value, t);
                    frame->GetWorldPosition(value, value);
                    state->camera->SetPos(NULL, value[0], value[1], value[2]);
                }

                break;
            case MOTION_KEY_CAMERA_TARGET:
                if (state->camera != NULL) {
                    sceVu0InterVectorXYZ(value, list->keys[next].value, list->keys[key].value, t);
                    frame->GetWorldPosition(value, value);
                    state->camera->SetRef(NULL, value[0], value[1], value[2]);
                }

                break;
            case MOTION_KEY_MATERIAL_ALPHA: {
                MDT_HEADER   *model;
                MDT_MATERIAL *material;

                one_minus_t = 1.0f - t;
                model = (MDT_HEADER *) target->GetVisual()->GetMDTDataAddress();
                material = (MDT_MATERIAL *) ((u_char *) model + model->info_ofs);
                material[list->target].diffuse[3] = 1.0f - (one_minus_t * list->keys[key].value[0] + t * list->keys[next].value[0]);
                target->attr.remake_pending = 2;
                break;
            }
            case MOTION_KEY_MATERIAL_COLOR: {
                MDT_HEADER   *model = (MDT_HEADER *) target->GetVisual()->GetMDTDataAddress();
                MDT_MATERIAL *material = (MDT_MATERIAL *) ((u_char *) model + model->info_ofs);

                sceVu0InterVectorXYZ(material[list->target].diffuse, list->keys[next].value, list->keys[key].value, t);
                target->attr.remake_pending = 2;
                break;
            }
            case MOTION_KEY_CAMERA_ROLL:
                if (state->camera != NULL) {
                    one_minus_t = 1.0f - t;
                    state->camera->SetRoll(-(PI * ((one_minus_t * list->keys[key].value[0] + t * list->keys[next].value[0]) / 180.0f)));
                }

                break;
            case MOTION_KEY_CAMERA_FOV:
                if (state->camera != NULL) {
                    one_minus_t = 1.0f - t;
                    MGSetProjection(0.5f * (480.0f * (1.0f / tanf(PI * ((0.5f * (one_minus_t * list->keys[key].value[0] + t * list->keys[next].value[0])) / 180.0f)))));
                }

                break;
            case MOTION_KEY_VISIBLE:
                if (list->keys[key].value[0] < 1.0f) {
                    target->attr.draw_on = 0;
                } else {
                    target->attr.draw_on = 3;
                }

                break;
            case MOTION_KEY_UNK_33:
                if (list->keys[key].value[0] < 1.0f) {
                    target->attr.draw_on = 2;
                } else {
                    target->attr.draw_on = 1;
                }

                break;
        }
    }

    return list->next;
}

/**
 * Working copy of the skinned frame's vertices that the bone weights move.
 */
static sceVu0FVECTOR def_vrtx[3000];
void SetMotionEX(CFrame *frame, tagMOTION_TYPE *motion, MOTION_INFO *info, MOTION_STATE *state, tagFRAME_INF *frame_info) {
    Mot_List *list;
    Mot_List *list2;

    if (state->playing_no != state->motion_no || state->blending != 0) {
        if (state->blending == 0) {
            state->blend = 0.0f;
        }

        state->blending = true;
        state->frame = state->time;
        state->blend += state->blend_step;
        state->playing_no = state->motion_no;

        if (!(state->blend < 1.0f)) {
            state->time = state->next_frame;
            state->frame = state->next_frame;
            state->blend = 0.0f;
            state->blending = false;
        }

        for (list = motion->proc_list; list != NULL;) {
            list = MotionProc(frame, state, list);
        }

        if (state->look_at != 0) {
            if (state->look_target != NULL) {
                LookAt(state->look_frame, state->look_target, state->look_constraint);
            } else {
                LookAt(state->look_frame, state->look_position, state->look_constraint);
            }
        }

        for (list = motion->proc_list2; list != NULL;) {
            list = MotionProc2(frame, motion, frame_info, list);
        }

        return;
    }

    state->playing_no = state->motion_no;
    state->frame = state->time;
    state->next_frame = state->frame + 1;
    state->blend = state->time - state->frame;

    for (list2 = motion->proc_list; list2 != NULL;) {
        list2 = MotionProc(frame, state, list2);
    }

    if (state->look_at != 0) {
        if (state->look_target != NULL) {
            LookAt(state->look_frame, state->look_target, state->look_constraint);
        } else {
            LookAt(state->look_frame, state->look_position, state->look_constraint);
        }
    }

    for (list2 = motion->proc_list2; list2 != NULL;) {
        list2 = MotionProc2(frame, motion, frame_info, list2);
    }

    state->time += info[state->motion_no].speed;

    if ((unsigned int) state->time >= info[state->motion_no].end) {
        state->time -= (float) info[state->motion_no].end - (float) info[state->motion_no].start;
        state->frame = info[state->motion_no].start;
        state->next_frame = state->frame + 1;
        state->blend = state->time - state->frame;
    }
}

/**
 * Takes a motion's animation data out of an arena and fills it from a file.
 *
 * @mangled CreateAnimeDataEX__FP14tagMOTION_TYPEP14CDataAlloc2_1_P16MOTION_FILE_INFO
 * @address 0x149090
 * @size 0x264
 */
int CreateAnimeDataEX(tagMOTION_TYPE *motion, CDataAlloc2<1> *arena, MOTION_FILE_INFO *files) {
    if (files[0].name != NULL) {
        motion->base_matrices = (sceVu0FMATRIX *) arena->Alloc((files[0].size >> 4) + 1);
        memcpy(motion->base_matrices, files[0].data, files[0].size);
    }

    if (files[1].name != NULL) {
        Mot_List      *list;
        Mot_File_List *record;
        u_char        *data;
        Mot_List      *reversed;
        Mot_List      *previous;
        Mot_List      *node;
        u_char        *keys;

        data = (u_char *) files[1].data;
        motion->proc_list = NULL;

        do {
            record = (Mot_File_List *) data;
            list = (Mot_List *) arena->Alloc(sizeof(Mot_List) / 16 + 1);
            list->frame = record->frame;
            list->target = record->target;
            list->key_count = record->key_count;
            list->type = record->type;
            list->keys = (Mot_Key *) arena->Alloc(list->key_count * sizeof(Mot_Key) / 16 + 1);
            data += sizeof(Mot_File_List);
            keys = data;
            data += list->key_count * sizeof(Mot_Key);
            memcpy(list->keys, keys, list->key_count * sizeof(Mot_Key));

            if (motion->proc_list == NULL) {
                list->next = NULL;
            } else {
                list->next = motion->proc_list;
            }

            motion->proc_list = list;
        } while (record->more != 0L);

        reversed = NULL;

        while ((node = motion->proc_list) != NULL) {
            previous = reversed;
            reversed = node;
            motion->proc_list = node->next;
            node->next = previous;
        }

        motion->proc_list = reversed;
    }

    if (files[2].name != NULL) {
        Mot_List      *list;
        Mot_File_List *record;
        u_char        *data;
        Mot_List      *reversed;
        Mot_List      *previous;
        Mot_List      *node;
        u_char        *keys;

        data = (u_char *) files[2].data;
        motion->proc_list2 = NULL;

        do {
            record = (Mot_File_List *) data;
            list = (Mot_List *) arena->Alloc(sizeof(Mot_List) / 16 + 1);
            list->frame = record->frame;
            list->target = record->target;
            list->key_count = record->key_count;
            list->type = record->type;
            list->keys = (Mot_Key *) arena->Alloc(list->key_count * sizeof(Mot_Key) / 16 + 1);
            data += sizeof(Mot_File_List);
            keys = data;
            data += list->key_count * sizeof(Mot_Key);
            memcpy(list->keys, keys, list->key_count * sizeof(Mot_Key));

            if (motion->proc_list2 == NULL) {
                list->next = NULL;
            } else {
                list->next = motion->proc_list2;
            }

            motion->proc_list2 = list;
        } while (record->more != 0L);

        reversed = NULL;

        while ((node = motion->proc_list2) != NULL) {
            previous = reversed;
            reversed = node;
            motion->proc_list2 = node->next;
            node->next = previous;
        }

        motion->proc_list2 = reversed;
    }

    return 1;
}

/**
 * Builds the per-frame animation table a model's motion needs.
 *
 * @mangled AnimeDataInit__FP6CFrameP14tagMOTION_TYPEP14CDataAlloc2_1_PP12tagFRAME_INF
 * @address 0x149300
 * @size 0x98
 */
void AnimeDataInit(CFrame *frame, tagMOTION_TYPE *motion, CDataAlloc2<1> *arena, tagFRAME_INF **frame_info) {
    *frame_info = (tagFRAME_INF *) arena->Alloc64((frame->GetFrameNum() + 10) * sizeof(tagFRAME_INF) / 16 + 1);
    AnimeDataInit(frame, motion, arena, *frame_info);
}

/**
 * Builds the per-frame animation table into storage already set aside.
 *
 * @mangled AnimeDataInit__FP6CFrameP14tagMOTION_TYPEP14CDataAlloc2_1_P12tagFRAME_INF
 * @address 0x1493A0
 * @size 0x318
 */
int AnimeDataInit(CFrame *frame, tagMOTION_TYPE *motion, CDataAlloc2<1> *arena, tagFRAME_INF *frame_info) {
    CFrameVu1 *current;
    u32        target;
    CFrame    *parent;
    Mot_List  *list = motion->proc_list2;
    u32        last = -1;
    int        i;
    int        count = frame->GetFrameNum();

    for (i = 0; i < count; i++) {
        current = &((CFrameVu1 *) frame)[i];
        tagFRAME_INF *info = &frame_info[i];
        sceVu0CopyMatrix(info->matrix, current->local);
        parent = current->parent;
        info->parent_frame = (CFrameVu1 *) parent - (CFrameVu1 *) frame;
    }

    for (; list != NULL; list = list->next) {
        if (list->type == MOTION_KEY_SKIP) {
            continue;
        }

        // Retail searches for each parent's index and never uses it.
        target = list->target;
        parent = ((CFrameVu1 *) frame)[target].parent;

        for (i = 0; i < target; i++) {
            if (parent == &((CFrameVu1 *) frame)[i]) {
                break;
            }
        }

        if (last == list->frame) {
            continue;
        }

        current = &((CFrameVu1 *) frame)[list->frame];
        parent = current->parent;

        for (i = 0; i < list->frame; i++) {
            if (parent == &((CFrameVu1 *) frame)[i]) {
                break;
            }
        }

        CVisualVu1 *visual = current->GetVisual();

        if (visual != NULL) {
            sceVu0FVECTOR *vertices;
            MDT_HEADER    *model = (MDT_HEADER *) visual->GetMDTDataAddress();
            vertices = (sceVu0FVECTOR *) ((u_char *) model + model->vertex_ofs);
            sceVu0FMATRIX matrix;

            frame_info[list->frame].base_vertices = (sceVu0FVECTOR *) arena->Alloc(model->vertex_num * sizeof(sceVu0FVECTOR) / 16 + 1);
            frame_info[list->frame].vertex_count = model->vertex_num;
            sceVu0UnitMatrix(matrix);
            sceVu0CopyMatrix(matrix, frame_info[list->frame].matrix);

            for (i = 0; i < (u32) model->vertex_num; i++) {
                sceVu0ApplyMatrix(frame_info[list->frame].base_vertices[i], matrix, vertices[i]);
            }
        }

        last = list->frame;
    }

    return 1;
}

int NextMotionTime_GET_EX(MOTION_INFO *info, MOTION_STATE *state) {
    int   playing_start = info[state->playing_no].start;
    float progress = (state->time - playing_start) / (info[state->playing_no].end - playing_start);
    int   start = info[state->motion_no].start;
    int   end = info[state->motion_no].end;
    int   time = start + progress * (end - start);

    if (end < time) {
        time = end;
    }

    if (time < start) {
        time = start;
    }

    return time;
}

/**
 * Builds a matrix that rotates by an angle around an arbitrary direction.
 */
static void SetRotationMatrixFromDir(sceVu0FMATRIX matrix, float *direction, float angle) {
    float         cosine = cosf(angle);
    float         sine = sinf(angle);
    float         inverse_cosine = 1.0f - cosine;
    sceVu0FVECTOR axis;

    sceVu0UnitMatrix(matrix);
    sceVu0Normalize(axis, direction);
    matrix[0][0] = cosine + inverse_cosine * (axis[0] * axis[0]);
    matrix[0][1] = inverse_cosine * (axis[0] * axis[1]) - axis[2] * sine;
    matrix[0][2] = inverse_cosine * (axis[0] * axis[2]) + axis[1] * sine;
    matrix[1][0] = inverse_cosine * (axis[1] * axis[0]) + axis[2] * sine;
    matrix[1][1] = cosine + inverse_cosine * (axis[1] * axis[1]);
    matrix[1][2] = inverse_cosine * (axis[1] * axis[2]) - axis[0] * sine;
    matrix[2][0] = inverse_cosine * (axis[2] * axis[0]) - axis[1] * sine;
    matrix[2][1] = inverse_cosine * (axis[2] * axis[1]) + axis[0] * sine;
    matrix[2][2] = cosine + inverse_cosine * (axis[2] * axis[2]);
}

int LookAt(CFrameVu1 *frame, float *target, _FRAMECONSTRAINT constraint) {
    sceVu0FVECTOR up;
    sceVu0FVECTOR local;
    sceVu0FVECTOR axis;
    sceVu0FVECTOR direction;
    sceVu0FVECTOR cross;
    sceVu0FVECTOR side;
    sceVu0FVECTOR plane;
    sceVu0FVECTOR delta;
    sceVu0FMATRIX parent;
    sceVu0FMATRIX matrix;
    sceVu0FMATRIX rotation;
    float         angle;

    sceVu0UnitMatrix(parent);
    frame->parent->GetLWMatrix(parent);
    sceVu0UnitMatrix(matrix);
    sceVu0CopyMatrix(matrix, frame->local);

    if (constraint == FRAME_CONSTRAINT_X) {
        sceVu0Normalize(up, matrix[0]);
    }

    if (constraint == FRAME_CONSTRAINT_Y) {
        sceVu0Normalize(up, matrix[1]);
    }

    if (constraint == FRAME_CONSTRAINT_Z) {
        sceVu0Normalize(up, matrix[2]);
    }

    up[3] = 1.0f;
    delta[3] = 1.0f;
    sceVu0SubVector(delta, target, parent[3]);
    local[0] = sceVu0InnerProduct(delta, parent[0]);
    local[1] = sceVu0InnerProduct(delta, parent[1]);
    local[2] = sceVu0InnerProduct(delta, parent[2]);
    local[3] = 1.0f;

    if (DistVector(local) > 0.0000001f) {
        sceVu0Normalize(direction, local);
        direction[3] = 1.0f;
        sceVu0Normalize(axis, up);
        axis[3] = 1.0f;
        sceVu0OuterProduct(cross, axis, direction);
        sceVu0Normalize(side, cross);
        side[3] = 1.0f;
        sceVu0OuterProduct(cross, side, axis);
        sceVu0Normalize(plane, cross);
        plane[3] = 1.0f;
        angle = sceVu0InnerProduct(direction, plane);

        if (angle < 0.99999f) {
            sceVu0UnitMatrix(rotation);
            angle = acosf(angle);

            if (angle > QUARTER_PI) {
                angle = QUARTER_PI;
            }

            if (acosf(sceVu0InnerProduct(axis, direction)) > HALF_PI) {
                SetRotationMatrixFromDir(rotation, side, -angle);
            } else {
                SetRotationMatrixFromDir(rotation, side, angle);
            }

            MulMatrix(matrix, rotation, matrix);
            frame->SetTransMatrix(matrix);
        }
    }

    return 0;
}

int LookAt(CFrameVu1 *frame, CFrameVu1 *target, _FRAMECONSTRAINT constraint) {
    sceVu0FMATRIX matrix;

    target->GetLWMatrix(matrix);
    return LookAt(frame, matrix[3], constraint);
}
/**
 * Collects the polygons of a set that meet a box.
 *
 * @mangled PickUpNearPoly__FP6CCPoly7CBoxVu0P6CCPolyi
 * @address 0x149C30
 * @size 0x118
 */
int PickUpNearPoly(CCPoly *out, CBoxVu0 box, CCPoly *poly, int count) {
    sceVu0FVECTOR poly_max;
    sceVu0FVECTOR poly_min;
    int           picked = 0;
    int           i;
    CCPoly       *src;
    CCPoly       *dst;

    vu_hold_box(box.max, box.min);
    src = poly;
    dst = out;

    for (i = 0; i < count; i++, src++) {
        VectorMaxMin(poly_max, poly_min, src->vertex[0], src->vertex[1], src->vertex[2]);

        if (vu_box_missed(poly_max, poly_min)) {
            continue;
        }

        memcpy(dst, src, sizeof(CCPoly));
        dst++;
        picked++;
    }

    return picked;
}

int CheckHit(CCPoly *poly, int count, float *from, float *to, float *hit_point, int nearest, int mode) {
    sceVu0FVECTOR point;
    sceVu0FVECTOR diff;
    sceVu0FVECTOR poly_min;
    sceVu0FVECTOR poly_max;
    CBoxVu0       line;
    sceVu0FVECTOR offset;
    int           i;
    int           hit = -1;
    int           found = 0;
    float         best;
    float         from_side;
    float         to_side;
    float         dist;

    VectorMaxMin(line.max, line.min, from, to);
    vu_hold_box(line.max, line.min);

    for (i = 0; i < count; i++, poly++) {
        if (poly->attr.ignore_mask & mode) {
            continue;
        }

        VectorMaxMin(poly_max, poly_min, poly->vertex[0], poly->vertex[1], poly->vertex[2]);

        if (line.max[0] < poly_min[0] || line.max[1] < poly_min[1] || line.max[2] < poly_min[2]) {
            continue;
        }

        if (line.min[0] > poly_max[0] || line.min[1] > poly_max[1] || line.min[2] > poly_max[2]) {
            continue;
        }

        sceVu0SubVector(offset, from, poly->vertex[0]);
        from_side = sceVu0InnerProduct(poly->normal, offset);
        sceVu0SubVector(offset, to, poly->vertex[0]);
        to_side = sceVu0InnerProduct(poly->normal, offset);

        if (from_side > 0.0f && to_side > 0.0f) {
            continue;
        }

        if (from_side < 0.0f && to_side < 0.0f) {
            continue;
        }

        if (IntersectionPoint_line_poly3(from, to, poly->vertex[0], poly->vertex[1], poly->vertex[2], poly->normal, point) == 0) {
            continue;
        }

        if (nearest == 0) {
            hit = i;
            sceVu0CopyVector(hit_point, point);
            break;
        }

        diff[0] = from[0] - point[0];
        diff[1] = from[1] - point[1];
        diff[2] = from[2] - point[2];
        dist = diff[0] * diff[0] + diff[1] * diff[1] + diff[2] * diff[2];

        if (found == 0) {
            hit = i;
            best = dist;
            sceVu0CopyVector(hit_point, point);
        } else if (best > dist) {
            hit = i;
            best = dist;
            sceVu0CopyVector(hit_point, point);
        }

        found = 1;
    }

    return hit;
}

int CheckHitVertical(CCPoly *poly, int count, float *from, float depth, float *hit_point, int mode) {
    sceVu0FVECTOR to;
    float         best_y;
    int           i;
    int           best = -1;

    to[0] = from[0];
    to[1] = from[1] + depth;
    to[2] = from[2];

    for (i = 0; i < count; i++, poly++) {
        if (poly->attr.ignore_mask & mode) {
            continue;
        }

        if (!IntersectionPoint_line_poly3(from, to, poly->vertex[0], poly->vertex[1], poly->vertex[2], poly->normal, hit_point)) {
            continue;
        }

        if (depth <= 0.0f) {
            if (from[1] > hit_point[1] && (best < 0 || (best >= 0 && best_y <= hit_point[1]))) {
                best_y = hit_point[1];
                best = i;
            }
        } else {
            if (from[1] < hit_point[1] && (best < 0 || (best >= 0 && !(best_y < hit_point[1])))) {
                best_y = hit_point[1];
                best = i;
            }
        }
    }

    if (best >= 0) {
        hit_point[1] = best_y;
    }

    return best;
}

int CheckHits(CCPoly *poly, int count, float *from, float *to, int max, int *hit_poly, float (*hit_point)[4], int sort, int mode) {
    sceVu0FVECTOR point;
    sceVu0FVECTOR poly_min;
    sceVu0FVECTOR poly_max;
    CBoxVu0       line;
    sceVu0FVECTOR offset;
    sceVu0FVECTOR swap;
    int           i;
    int           j;
    int           hits;
    float         from_side;
    float         to_side;

    hits = 0;
    VectorMaxMin(line.max, line.min, from, to);
    vu_hold_box(line.max, line.min);

    for (i = 0; i < count; i++, poly++) {
        if (poly->attr.ignore_mask & mode) {
            continue;
        }

        VectorMaxMin(poly_max, poly_min, poly->vertex[0], poly->vertex[1], poly->vertex[2]);

        if (line.max[0] < poly_min[0] || line.max[1] < poly_min[1] || line.max[2] < poly_min[2]) {
            continue;
        }

        if (line.min[0] > poly_max[0] || line.min[1] > poly_max[1] || line.min[2] > poly_max[2]) {
            continue;
        }

        sceVu0SubVector(offset, from, poly->vertex[0]);
        from_side = sceVu0InnerProduct(poly->normal, offset);
        sceVu0SubVector(offset, to, poly->vertex[0]);
        to_side = sceVu0InnerProduct(poly->normal, offset);

        if (from_side > 0.0f && to_side > 0.0f) {
            continue;
        }

        if (from_side < 0.0f && to_side < 0.0f) {
            continue;
        }

        if (IntersectionPoint_line_poly3(from, to, poly->vertex[0], poly->vertex[1], poly->vertex[2], poly->normal, point) == 0) {
            continue;
        }

        if (hits >= max) {
            break;
        }

        hit_poly[hits] = i;
        sceVu0CopyVector(hit_point[hits], point);
        hit_point[hits][3] = DistVector(from, point);
        hits++;
    }

    if (sort == 0) {
        return hits;
    }

    if (sort > 0) {
        for (i = 0; i < hits - 1; i++) {
            for (j = i + 1; j < hits; j++) {
                if (hit_point[i][3] > hit_point[j][3]) {
                    int index = hit_poly[i];
                    hit_poly[i] = hit_poly[j];
                    hit_poly[j] = index;
                    sceVu0CopyVector(swap, hit_point[i]);
                    sceVu0CopyVector(hit_point[i], hit_point[j]);
                    sceVu0CopyVector(hit_point[j], swap);
                }
            }
        }
    }

    if (sort < 0) {
        for (i = 0; i < hits - 1; i++) {
            for (j = i + 1; j < hits; j++) {
                if (hit_point[i][3] > hit_point[j][3]) {
                    int index = hit_poly[i];
                    hit_poly[i] = hit_poly[j];
                    hit_poly[j] = index;
                    sceVu0CopyVector(swap, hit_point[i]);
                    sceVu0CopyVector(hit_point[i], hit_point[j]);
                    sceVu0CopyVector(hit_point[j], swap);
                }
            }
        }
    }

    return hits;
}

int MoveCheck(float *pos, float *velocity, float *out_pos, MoveCheckInfo *out_info, CCPoly *polys, int poly_num, int mode) {
    int           hit_wall;
    CCPoly       *wall;
    int           poly_no;
    int           i;
    float         drop;
    sceVu0FVECTOR hit;
    sceVu0FVECTOR from;
    sceVu0FVECTOR to;
    CCPoly        poly;
    sceVu0FVECTOR probe;
    sceVu0FVECTOR centre;

    out_pos[0] = pos[0];
    out_pos[1] = pos[1];
    out_pos[2] = pos[2];
    from[0] = pos[0] + velocity[0];
    from[1] = 17.0f + pos[1];
    // The probe steps along Z by the X velocity; retail uses velocity[0] for both horizontal coordinates.
    from[2] = pos[2] + velocity[0];
    to[0] = from[0];
    to[1] = 4.0f + pos[1] + velocity[1];
    to[2] = from[2];

    if (CheckHit(polys, poly_num, from, to, hit, 0, mode) >= 0) {
        velocity[2] = 0.0f;
        velocity[0] = 0.0f;
    }

    from[0] = pos[0];
    from[1] = 4.0f + pos[1];
    from[2] = pos[2];
    to[0] = from[0] + velocity[0];
    to[1] = from[1] + velocity[1];
    to[2] = from[2] + velocity[2];
    poly_no = CheckHit(polys, poly_num, from, to, hit, 0, mode);
    hit_wall = poly_no >= 0;

    if (hit_wall == 0) {
        from[0] = to[0];
        from[1] = to[1];
        from[2] = to[2];
        to[0] = from[0];
        to[1] = from[1] - 4.0f;
        to[2] = from[2];
    } else {
        wall = &polys[poly_no];
        sceVu0Normalize(wall->normal, wall->normal);

        if (wall->normal[1] < 0.5f && !(wall->normal[1] <= -0.5f)) {
            to[0] = pos[0];
            to[1] = pos[1];
            to[2] = pos[2];
        } else {
            from[0] = hit[0];
            from[1] = 4.0f + hit[1];
            from[2] = hit[2];
            to[0] = from[0];
            to[1] = from[1] - 4.0f;
            to[2] = from[2];
        }
    }

    out_info->ground_found = 0;
    out_info->landed = 0;
    drop = 2.0f;

    if (!(velocity[1] <= 0.1f)) {
        drop = 0.0f;
    }

    sceVu0CopyVector(probe, from);

    for (i = 0; i < 2; i++) {
        if (GetFootPoly(probe, 15.0f, &poly, hit, polys, poly_num, mode) != 0) {
            sceVu0Normalize(poly.normal, poly.normal);
            out_info->ground_poly = poly;
            out_info->poly = poly;
            out_info->ground_found = 1;
            *(u_long128 *) out_info->ground_point = *(u_long128 *) hit;

            if (!(hit[1] <= from[1] + velocity[1] - 4.0f - drop)) {
                out_info->landed = 1;

                if (poly.normal[1] < 0.5f && !(poly.normal[1] <= -0.5f)) {
                    i = 2;
                }
            } else {
                out_info->landed = 0;
            }

            break;
        }

        probe[0] += 0.01f;
        probe[2] += 0.001f;
    }

    if (out_info->landed != 0) {
        out_pos[0] = hit[0];
        out_pos[1] = hit[1];
        out_pos[2] = hit[2];
    } else {
        out_pos[0] = to[0];
        out_pos[1] = to[1];
        out_pos[2] = to[2];
    }

    *(u_long128 *) centre = *(u_long128 *) out_pos;
    centre[1] += 4.0f;

    if (CheckWidth(polys, poly_num, centre, 5.0f, to, mode) != 0) {
        out_pos[0] = to[0];
        out_pos[2] = to[2];
    }

    return hit_wall;
}

/**
 * Describes the surface of a collision triangle, in the shape that CCPoly
 * carries alongside its plane.
 */
struct CCPolyAttr {
    s16 ground_kind; /**< What the surface is made of. */
    s16 foot_sound;  /**< Sound the character's feet play on it. */
    s16 light;       /**< Light or ambience the surface puts the character in. */
    s16 ignore_mask; /**< Collision query modes that pass through the surface. */
    u8  unk_48[8];
};

/**
 * Finds the polygon under a position, within a drop of the given height, and
 * gives back the surface it is made of.
 *
 * @mangled GetFootPoly__FPffP6CCPolyPfP6CCPolyii
 * @address 0x14ABB0
 * @size 0x1DC
 */
int GetFootPoly(float *position, float depth, CCPoly *found, float *ground, CCPoly *polys, int count, int mode) {
    int           hits;
    int           i;
    int           hit_no[32];
    sceVu0FVECTOR from;
    sceVu0FVECTOR to;
    sceVu0FVECTOR hit_point[64];
    CCPolyAttr    attr;
    CCPolyAttr    saved;

    sceVu0CopyVector(from, position);
    sceVu0CopyVector(to, position);
    to[1] -= depth;
    hits = CheckHits(polys, count, from, to, 32, hit_no, hit_point, 1, mode);
    attr.ground_kind = 0;
    attr.foot_sound = 0;
    attr.light = 0;
    saved = attr;

    if (hits > 0) {
        *found = polys[hit_no[0]];
        sceVu0CopyVector(ground, hit_point[0]);
    }

    for (i = 0; i < hits; i++) {
        CCPolyAttr *hit_attr = (CCPolyAttr *) &polys[hit_no[i]].attr;

        if (attr.ground_kind == 0) {
            attr.ground_kind = hit_attr->ground_kind;
        }

        if (attr.foot_sound == 0) {
            attr.foot_sound = hit_attr->foot_sound;
        }

        if (attr.light == 0) {
            attr.light = hit_attr->light;
        }
    }

    found->info = *(CCPolyInfo *) &attr;
    return hits > 0;
}

int GetEventPoly(float *position, float *velocity, CCPoly *found, int *found_no, float *hit, CCPoly *polys, int count, int mode) {
    int           hits;
    int           i;
    int           hit_no[32];
    sceVu0FVECTOR from;
    sceVu0FVECTOR to;
    sceVu0FVECTOR hit_point[32];
    CCPolyAttr    attr;

    sceVu0CopyVector(from, position);
    sceVu0AddVector(to, position, velocity);
    hits = CheckHits(polys, count, from, to, 32, hit_no, hit_point, 1, mode);
    attr.ground_kind = 0;
    attr.foot_sound = 0;
    attr.light = 0;
    *found_no = -1;

    if (hits > 0) {
        *found = polys[hit_no[0]];
        *found_no = hit_no[0];
        sceVu0CopyVector(hit, hit_point[0]);
    }

    for (i = 0; i < hits; i++) {
        CCPolyAttr *hit_attr = (CCPolyAttr *) &polys[hit_no[i]].attr;

        if (attr.ground_kind == 0) {
            attr.ground_kind = hit_attr->ground_kind;
        }

        if (attr.foot_sound == 0) {
            attr.foot_sound = hit_attr->foot_sound;
        }

        if (attr.light == 0) {
            attr.light = hit_attr->light;
        }
    }

    found->info = *(CCPolyInfo *) &attr;
    return attr.ground_kind;
}

int CheckWidth(CCPoly *polys, int count, float *position, float radius, float *out, int flags) {
    int           hit;
    int           hit_plus;
    int           hit_minus;
    int           poly_no;
    float         step;
    sceVu0FVECTOR to;
    sceVu0FVECTOR hit_a;
    sceVu0FVECTOR hit_b;
    sceVu0FVECTOR pos;
    sceVu0FVECTOR normal;

    hit = 0;
    step = radius / 1.4142135f;
    sceVu0CopyVector(pos, position);
    sceVu0CopyVector(out, position);
    hit_minus = 0;
    hit_plus = 0;
    to[0] = pos[0] + step;
    to[1] = pos[1];
    to[2] = pos[2] + step;
    poly_no = CheckHit(polys, count, pos, to, hit_a, 0, flags);

    if (poly_no != -1) {
        sceVu0Normalize(normal, polys[poly_no].normal);

        if (normal[1] < 0.5f && !(normal[1] <= -0.5f)) {
            hit_plus = 1;
            hit = 1;
        }
    }

    to[0] = pos[0] - step;
    to[1] = pos[1];
    to[2] = pos[2] - step;
    poly_no = CheckHit(polys, count, pos, to, hit_b, 0, flags);

    if (poly_no != -1) {
        sceVu0Normalize(normal, polys[poly_no].normal);

        if (normal[1] < 0.5f && !(normal[1] <= -0.5f)) {
            hit = 1;
            hit_minus = 1;
        }
    }

    if (hit_plus != 0 && hit_minus != 0) {
        out[0] = 0.5f * (hit_a[0] + hit_b[0]);
        out[2] = 0.5f * (hit_a[2] + hit_b[2]);
    } else {
        if (hit_plus != 0) {
            out[0] = hit_a[0] - step;
            out[2] = hit_a[2] - step;
        }

        if (hit_minus != 0) {
            out[0] = hit_b[0] + step;
            out[2] = hit_b[2] + step;
        }
    }

    sceVu0CopyVector(pos, out);
    hit_minus = 0;
    hit_plus = 0;
    to[0] = pos[0] + step;
    to[1] = pos[1];
    to[2] = pos[2] - step;
    poly_no = CheckHit(polys, count, pos, to, hit_a, 0, flags);

    if (poly_no != -1) {
        sceVu0Normalize(normal, polys[poly_no].normal);

        if (normal[1] < 0.5f && !(normal[1] <= -0.5f)) {
            hit_plus = 1;
            hit = 1;
        }
    }

    to[0] = pos[0] - step;
    to[1] = pos[1];
    to[2] = pos[2] + step;
    poly_no = CheckHit(polys, count, pos, to, hit_b, 0, flags);

    if (poly_no != -1) {
        sceVu0Normalize(normal, polys[poly_no].normal);

        if (normal[1] < 0.5f && !(normal[1] <= -0.5f)) {
            hit = 1;
            hit_minus = 1;
        }
    }

    if (hit_plus != 0 && hit_minus != 0) {
        out[0] = 0.5f * (hit_a[0] + hit_b[0]);
        out[2] = 0.5f * (hit_a[2] + hit_b[2]);
    } else {
        if (hit_plus != 0) {
            out[0] = hit_a[0] - step;
            out[2] = hit_a[2] + step;
        }

        if (hit_minus != 0) {
            out[0] = hit_b[0] + step;
            out[2] = hit_b[2] - step;
        }
    }

    sceVu0CopyVector(pos, out);
    hit_minus = 0;
    hit_plus = 0;
    to[0] = pos[0] + radius;
    to[1] = pos[1];
    to[2] = pos[2];
    poly_no = CheckHit(polys, count, pos, to, hit_a, 0, flags);

    if (poly_no != -1) {
        sceVu0Normalize(normal, polys[poly_no].normal);

        if (normal[1] < 0.5f && !(normal[1] <= -0.5f)) {
            hit_plus = 1;
            hit = 1;
        }
    }

    to[0] = pos[0] - radius;
    to[1] = pos[1];
    to[2] = pos[2];
    poly_no = CheckHit(polys, count, pos, to, hit_b, 0, flags);

    if (poly_no != -1) {
        sceVu0Normalize(normal, polys[poly_no].normal);

        if (normal[1] < 0.5f && !(normal[1] <= -0.5f)) {
            hit = 1;
            hit_minus = 1;
        }
    }

    if (hit_plus != 0 && hit_minus != 0) {
        out[0] = 0.5f * (hit_a[0] + hit_b[0]);
    } else {
        if (hit_plus != 0) {
            out[0] = hit_a[0] - radius;
        }

        if (hit_minus != 0) {
            out[0] = hit_b[0] + radius;
        }
    }

    sceVu0CopyVector(pos, out);
    hit_minus = 0;
    hit_plus = 0;
    to[0] = pos[0];
    to[1] = pos[1];
    to[2] = pos[2] + radius;
    poly_no = CheckHit(polys, count, pos, to, hit_a, 0, flags);

    if (poly_no != -1) {
        sceVu0Normalize(normal, polys[poly_no].normal);

        if (normal[1] < 0.5f && !(normal[1] <= -0.5f)) {
            hit = 1;
            hit_plus = 1;
        }
    }

    to[0] = pos[0];
    to[1] = pos[1];
    to[2] = pos[2] - radius;
    poly_no = CheckHit(polys, count, pos, to, hit_b, 0, flags);

    if (poly_no != -1) {
        sceVu0Normalize(normal, polys[poly_no].normal);

        if (normal[1] < 0.5f && !(normal[1] <= -0.5f)) {
            hit = 1;
            hit_minus = 1;
        }
    }

    if (hit_plus != 0 && hit_minus != 0) {
        out[2] = 0.5f * (hit_a[2] + hit_b[2]);
    } else {
        if (hit_plus != 0) {
            out[2] = hit_a[2] - radius;
        }

        if (hit_minus != 0) {
            out[2] = hit_b[2] + radius;
        }
    }

    return hit;
}

int CheckCameraWidth(CCPoly *polys, int count, float *position, float radius, float *out, int flags) {
    int           hit;
    int           hit_plus;
    int           hit_minus;
    int           poly_no;
    float         step;
    sceVu0FVECTOR to;
    sceVu0FVECTOR hit_a;
    sceVu0FVECTOR hit_b;
    sceVu0FVECTOR pos;
    sceVu0FVECTOR normal;

    hit = 0;
    step = radius / 1.4142135f;
    sceVu0CopyVector(pos, position);
    sceVu0CopyVector(out, position);
    hit_minus = 0;
    hit_plus = 0;
    to[0] = pos[0] + step;
    to[1] = pos[1];
    to[2] = pos[2] + step;
    poly_no = CheckHit(polys, count, pos, to, hit_a, 0, flags);

    if (poly_no != -1) {
        sceVu0Normalize(normal, polys[poly_no].normal);

        if (normal[1] < 0.5f && !(normal[1] <= -0.5f) && normal[0] * step + normal[2] * step < 0.0f) {
            hit_plus = 1;
            hit = 1;
        }
    }

    to[0] = pos[0] - step;
    to[1] = pos[1];
    to[2] = pos[2] - step;
    poly_no = CheckHit(polys, count, pos, to, hit_b, 0, flags);

    if (poly_no != -1) {
        sceVu0Normalize(normal, polys[poly_no].normal);

        if (normal[1] < 0.5f && !(normal[1] <= -0.5f) && normal[0] * -step + normal[2] * -step < 0.0f) {
            hit = 1;
            hit_minus = 1;
        }
    }

    if (hit_plus != 0 && hit_minus != 0) {
        out[0] = 0.5f * (hit_a[0] + hit_b[0]);
        out[2] = 0.5f * (hit_a[2] + hit_b[2]);
    } else {
        if (hit_plus != 0) {
            out[0] = hit_a[0] - step;
            out[2] = hit_a[2] - step;
        }

        if (hit_minus != 0) {
            out[0] = hit_b[0] + step;
            out[2] = hit_b[2] + step;
        }
    }

    sceVu0CopyVector(pos, out);
    hit_minus = 0;
    hit_plus = 0;
    to[0] = pos[0] + step;
    to[1] = pos[1];
    to[2] = pos[2] - step;
    poly_no = CheckHit(polys, count, pos, to, hit_a, 0, flags);

    if (poly_no != -1) {
        sceVu0Normalize(normal, polys[poly_no].normal);

        if (normal[1] < 0.5f && !(normal[1] <= -0.5f) && normal[0] * step + normal[2] * -step < 0.0f) {
            hit_plus = 1;
            hit = 1;
        }
    }

    to[0] = pos[0] - step;
    to[1] = pos[1];
    to[2] = pos[2] + step;
    poly_no = CheckHit(polys, count, pos, to, hit_b, 0, flags);

    if (poly_no != -1) {
        sceVu0Normalize(normal, polys[poly_no].normal);

        if (normal[1] < 0.5f && !(normal[1] <= -0.5f) && normal[0] * -step + normal[2] * step < 0.0f) {
            hit = 1;
            hit_minus = 1;
        }
    }

    if (hit_plus != 0 && hit_minus != 0) {
        out[0] = 0.5f * (hit_a[0] + hit_b[0]);
        out[2] = 0.5f * (hit_a[2] + hit_b[2]);
    } else {
        if (hit_plus != 0) {
            out[0] = hit_a[0] - step;
            out[2] = hit_a[2] + step;
        }

        if (hit_minus != 0) {
            out[0] = hit_b[0] + step;
            out[2] = hit_b[2] - step;
        }
    }

    sceVu0CopyVector(pos, out);
    hit_minus = 0;
    hit_plus = 0;
    to[0] = pos[0] + radius;
    to[1] = pos[1];
    to[2] = pos[2];
    poly_no = CheckHit(polys, count, pos, to, hit_a, 0, flags);

    if (poly_no != -1) {
        sceVu0Normalize(normal, polys[poly_no].normal);

        if (normal[1] < 0.5f && !(normal[1] <= -0.5f) && normal[0] * radius < 0.0f) {
            hit_plus = 1;
            hit = 1;
        }
    }

    to[0] = pos[0] - radius;
    to[1] = pos[1];
    to[2] = pos[2];
    poly_no = CheckHit(polys, count, pos, to, hit_b, 0, flags);

    if (poly_no != -1) {
        sceVu0Normalize(normal, polys[poly_no].normal);

        if (normal[1] < 0.5f && !(normal[1] <= -0.5f) && normal[0] * -radius < 0.0f) {
            hit = 1;
            hit_minus = 1;
        }
    }

    if (hit_plus != 0 && hit_minus != 0) {
        out[0] = 0.5f * (hit_a[0] + hit_b[0]);
    } else {
        if (hit_plus != 0) {
            out[0] = hit_a[0] - radius;
        }

        if (hit_minus != 0) {
            out[0] = hit_b[0] + radius;
        }
    }

    sceVu0CopyVector(pos, out);
    hit_minus = 0;
    hit_plus = 0;
    to[0] = pos[0];
    to[1] = pos[1];
    to[2] = pos[2] + radius;
    poly_no = CheckHit(polys, count, pos, to, hit_a, 0, flags);

    if (poly_no != -1) {
        sceVu0Normalize(normal, polys[poly_no].normal);

        if (normal[1] < 0.5f && !(normal[1] <= -0.5f) && normal[2] * radius < 0.0f) {
            hit = 1;
            hit_plus = 1;
        }
    }

    to[0] = pos[0];
    to[1] = pos[1];
    to[2] = pos[2] - radius;
    poly_no = CheckHit(polys, count, pos, to, hit_b, 0, flags);

    if (poly_no != -1) {
        sceVu0Normalize(normal, polys[poly_no].normal);

        if (normal[1] < 0.5f && !(normal[1] <= -0.5f) && normal[2] * -radius < 0.0f) {
            hit = 1;
            hit_minus = 1;
        }
    }

    if (hit_plus != 0 && hit_minus != 0) {
        out[2] = 0.5f * (hit_a[2] + hit_b[2]);
    } else {
        if (hit_plus != 0) {
            out[2] = hit_a[2] - radius;
        }

        if (hit_minus != 0) {
            out[2] = hit_b[2] + radius;
        }
    }

    return hit;
}

// clang-format off
u32 MesWinClut[256] = {
    0x00000000, 0x80304045, 0x80BFBFBF, 0x8040BDBD,
    0x80BDBD40, 0x8040BD40, 0xFF304045, 0x8066CEE7,
    0x808F8F8F, 0x808F8F8F, 0x808F8F8F, 0x808F8F8F,
    0x808F8F8F, 0x808F8F8F, 0x808F8F8F, 0x808F8F8F,
    0x808F8F8F, 0x80BF3FBF, 0x808F8F8F, 0x808F8F8F,
};
// clang-format on

s32 GaijiDataTbl[][8] = {
    {-768, 0,   176, 110, 22, 0, 3, 8},
    {-767, 0,   154, 110, 22, 0, 3, 8},
    {-766, 32,  132, 32,  22, 0, 3, 2},
    {-765, 64,  132, 32,  22, 0, 3, 2},
    {-764, 0,   132, 32,  22, 0, 3, 2},
    {-763, 96,  132, 32,  22, 0, 3, 2},
    {-762, 0,   22,  22,  22, 0, 3, 2},
    {-761, 22,  22,  22,  22, 0, 3, 2},
    {-760, 66,  22,  22,  22, 0, 3, 2},
    {-759, 44,  22,  22,  22, 0, 3, 2},
    {-758, 0,   66,  22,  22, 0, 3, 2},
    {-757, 44,  66,  22,  22, 0, 3, 2},
    {-756, 22,  66,  22,  22, 0, 3, 2},
    {-755, 0,   88,  22,  22, 0, 2, 2},
    {-754, 22,  88,  22,  22, 0, 2, 2},
    {-753, 44,  88,  22,  22, 0, 2, 2},
    {-752, 66,  66,  22,  22, 0, 2, 2},
    {-751, 66,  88,  22,  22, 0, 2, 2},
    {-750, 0,   204, 26,  26, 0, 2, 2},
    {-749, 26,  204, 26,  26, 0, 2, 2},
    {-748, 0,   230, 26,  26, 0, 2, 2},
    {-747, 26,  230, 26,  26, 0, 2, 2},
    {-746, 0,   110, 22,  22, 0, 2, 2},
    {-745, 22,  110, 66,  22, 8, 2, 5},
    {-744, 0,   44,  56,  22, 0, 2, 5},
    {-743, 88,  224, 40,  32, 0, 2, 4},
    {-742, 88,  22,  22,  22, 0, 2, 2},
    {-741, 100, 44,  22,  22, 0, 0, 2},
    {-740, 56,  44,  22,  22, 0, 0, 2},
    {-739, 100, 66,  22,  22, 0, 0, 2},
    {-738, 78,  44,  22,  22, 0, 0, 2},
    {-737, 52,  204, 25,  26, 0, 0, 3},
    {-736, 52,  230, 33,  18, 0, 0, 3},
    {-735, 128, 0,   14,  20, 0, 3, 1},
    {-734, 142, 0,   14,  20, 0, 3, 1},
    {-733, 156, 0,   14,  20, 0, 3, 1},
    {-732, 170, 0,   14,  20, 0, 3, 1},
    {-731, 184, 0,   14,  20, 0, 3, 1},
    {-730, 198, 0,   14,  20, 0, 3, 1},
    {-729, 212, 0,   14,  20, 0, 3, 1},
    {-728, 226, 0,   14,  20, 0, 3, 1},
    {-727, 240, 0,   14,  20, 0, 3, 1},
    {-726, 128, 20,  14,  20, 0, 3, 1},
    {-725, 142, 20,  14,  20, 0, 3, 1},
    {-724, 156, 20,  14,  20, 0, 3, 1},
    {-723, 170, 20,  14,  20, 0, 3, 1},
    {-722, 184, 20,  14,  20, 0, 3, 1},
    {-721, 198, 20,  14,  20, 0, 3, 1},
    {-720, 212, 20,  14,  20, 0, 3, 1},
    {-719, 226, 20,  14,  20, 0, 3, 1},
    {-718, 240, 20,  14,  20, 0, 3, 1},
    {-717, 128, 40,  14,  20, 0, 3, 1},
    {-716, 142, 40,  14,  20, 0, 3, 1},
    {-715, 156, 40,  14,  20, 0, 3, 1},
    {-714, 170, 40,  14,  20, 0, 3, 1},
    {-713, 184, 40,  14,  20, 0, 3, 1},
    {-712, 198, 40,  14,  20, 0, 3, 1},
    {-711, 212, 40,  14,  20, 0, 3, 1},
    {-710, 226, 40,  14,  20, 0, 3, 1},
    {-709, 240, 40,  14,  20, 0, 3, 1},
    {-708, 128, 60,  14,  20, 0, 3, 1},
    {-707, 142, 60,  14,  20, 0, 3, 1},
    {-706, 156, 60,  14,  20, 0, 3, 1},
    {-705, 170, 60,  14,  20, 0, 3, 1},
    {-704, 184, 60,  14,  20, 0, 3, 1},
    {-703, 198, 60,  14,  20, 0, 3, 1},
    {-702, 212, 60,  14,  20, 0, 3, 1},
    {-701, 226, 60,  14,  20, 0, 3, 1},
    {-700, 240, 60,  14,  20, 0, 3, 1},
    {-699, 128, 80,  14,  20, 0, 3, 1},
    {-698, 142, 80,  14,  20, 0, 3, 1},
    {-697, 156, 80,  14,  20, 0, 3, 1},
    {-696, 170, 80,  14,  20, 0, 3, 1},
    {-695, 184, 80,  14,  20, 0, 3, 1},
    {-694, 198, 80,  14,  20, 0, 3, 1},
    {-693, 212, 80,  14,  20, 0, 3, 1},
    {-692, 226, 80,  14,  20, 0, 3, 1},
    {-691, 240, 80,  14,  20, 0, 3, 1},
    {-690, 128, 100, 14,  20, 0, 3, 1},
    {-689, 142, 100, 14,  20, 0, 3, 1},
    {-688, 156, 100, 14,  20, 0, 3, 1},
    {-687, 170, 100, 14,  20, 0, 3, 1},
    {-686, 184, 100, 14,  20, 0, 3, 1},
    {-685, 198, 100, 14,  20, 0, 3, 1},
    {-684, 212, 100, 14,  20, 0, 3, 1},
    {-683, 226, 100, 14,  20, 0, 3, 1},
    {-682, 240, 100, 14,  20, 0, 3, 1},
    {-681, 128, 120, 14,  20, 0, 3, 1},
    {-680, 142, 120, 14,  20, 0, 3, 1},
    {-679, 156, 120, 14,  20, 0, 3, 1},
    {-678, 170, 120, 14,  20, 0, 3, 1},
    {-677, 184, 120, 14,  20, 0, 3, 1},
    {-676, 198, 120, 14,  20, 0, 3, 1},
    {-675, 212, 120, 14,  20, 0, 3, 1},
    {-674, 226, 120, 14,  20, 0, 3, 1},
    {-673, 240, 120, 14,  20, 0, 3, 1},
    {-672, 128, 140, 14,  20, 0, 3, 1},
    {-671, 142, 140, 14,  20, 0, 3, 1},
    {-670, 156, 140, 14,  20, 0, 3, 1},
    {-669, 170, 140, 14,  20, 0, 3, 1},
    {-668, 184, 140, 14,  20, 0, 3, 1},
    {-667, 198, 140, 14,  20, 0, 3, 1},
    {-666, 212, 140, 14,  20, 0, 3, 1},
    {-665, 226, 140, 14,  20, 0, 3, 1},
    {-664, 240, 140, 14,  20, 0, 3, 1},
    {-663, 128, 160, 14,  20, 0, 3, 1},
    {-662, 142, 160, 14,  20, 0, 3, 1},
    {-661, 156, 160, 14,  20, 0, 3, 1},
    {-660, 170, 160, 14,  20, 0, 3, 1},
    {-659, 184, 160, 14,  20, 0, 3, 1},
    {-658, 198, 160, 14,  20, 0, 3, 1},
    {-657, 212, 160, 14,  20, 0, 3, 1},
    {-656, 226, 160, 14,  20, 0, 3, 1},
    {-655, 240, 160, 14,  20, 0, 3, 1},
    {-654, 128, 180, 14,  20, 0, 3, 1},
    {-653, 142, 180, 14,  20, 0, 3, 1},
    {-652, 156, 180, 14,  20, 0, 3, 1},
    {-651, 170, 180, 14,  20, 0, 3, 1},
    {-650, 184, 180, 14,  20, 0, 3, 1},
    {-649, 198, 180, 14,  20, 0, 3, 1},
    {-648, 212, 180, 14,  20, 0, 3, 1},
    {-647, 226, 200, 14,  20, 0, 3, 1},
    {-646, 198, 200, 14,  20, 0, 3, 1},
    {-645, 184, 200, 14,  20, 0, 3, 1},
    {-644, 128, 0,   14,  20, 0, 3, 1},
    {-643, 142, 220, 14,  20, 0, 3, 1},
    {-642, 184, 0,   14,  20, 0, 3, 1},
    {-641, 184, 0,   14,  20, 0, 3, 1},
    {-640, 198, 20,  14,  20, 0, 3, 1},
    {-639, 156, 40,  14,  20, 0, 3, 1},
    {-638, 212, 200, 14,  20, 0, 3, 1},
    {-637, 240, 40,  14,  20, 0, 3, 1},
    {-636, 240, 40,  14,  20, 0, 3, 1},
    {-635, 240, 40,  14,  20, 0, 3, 1},
    {-634, 240, 40,  14,  20, 0, 3, 1},
    {-633, 156, 220, 14,  20, 0, 3, 1},
    {-632, 170, 60,  14,  20, 0, 3, 1},
    {-631, 170, 60,  14,  20, 0, 3, 1},
    {-630, 170, 60,  14,  20, 0, 3, 1},
    {-629, 170, 60,  14,  20, 0, 3, 1},
    {-628, 128, 220, 14,  20, 0, 3, 1},
    {-627, 128, 220, 14,  20, 0, 3, 1},
    {-626, 128, 220, 14,  20, 0, 3, 1},
    {-625, 128, 220, 14,  20, 0, 3, 1},
    {-624, 170, 80,  14,  20, 0, 3, 1},
    {-623, 184, 80,  14,  20, 0, 3, 1},
    {-622, 184, 80,  14,  20, 0, 3, 1},
    {-621, 184, 80,  14,  20, 0, 3, 1},
    {-620, 184, 80,  14,  20, 0, 3, 1},
    {-619, 142, 100, 14,  20, 0, 3, 1},
    {-618, 142, 100, 14,  20, 0, 3, 1},
    {-617, 142, 100, 14,  20, 0, 3, 1},
    {-616, 142, 100, 14,  20, 0, 3, 1},
    {-615, 156, 40,  14,  20, 0, 3, 1},
    {-614, 128, 0,   14,  20, 0, 3, 1},
    {-613, 240, 200, 14,  20, 0, 3, 1},
    {-612, 198, 20,  14,  20, 0, 3, 1},
    {-611, 156, 120, 14,  20, 0, 3, 1},
    {-610, 156, 120, 14,  20, 0, 3, 1},
    {-609, 156, 120, 14,  20, 0, 3, 1},
    {-608, 128, 0,   14,  20, 0, 3, 1},
    {-607, 128, 0,   14,  20, 0, 3, 1},
    {-606, 240, 0,   14,  20, 0, 3, 1},
    {-605, 240, 0,   14,  20, 0, 3, 1},
    {-604, 240, 0,   14,  20, 0, 3, 1},
    {-603, 240, 0,   14,  20, 0, 3, 1},
    {-602, 156, 40,  14,  20, 0, 3, 1},
    {-601, 156, 40,  14,  20, 0, 3, 1},
    {-600, 184, 0,   14,  20, 0, 3, 1},
    {-599, 184, 0,   14,  20, 0, 3, 1},
    {-598, 198, 20,  14,  20, 0, 3, 1},
    {-597, 198, 20,  14,  20, 0, 3, 1},
    {-596, 184, 20,  14,  20, 0, 3, 1},
    {-595, 156, 120, 14,  20, 0, 3, 1},
    {-594, 156, 120, 14,  20, 0, 3, 1},
    {-1,   96,  96,  32,  32, 0, 0, 1},
};

// clang-format off
u32 FontColorTbl[16] = {
    0x00000000,      FONT_COLOR_BROWN, FONT_COLOR_WHITE,        FONT_COLOR_YELLOW,
    FONT_COLOR_CYAN, FONT_COLOR_GREEN, FONT_COLOR_BROWN_OPAQUE, FONT_COLOR_GOLD,
    FONT_COLOR_GREY, FONT_COLOR_GREY,  FONT_COLOR_GREY,         FONT_COLOR_GREY,
    FONT_COLOR_GREY, FONT_COLOR_GREY,  FONT_COLOR_GREY,         0x00000000,
};
// clang-format on

int Mes1MakeFlg = 1;
int Mes2MakeFlg = 1;
int MesAbsDrawOff;

/** Texture buffer the first common menu message window draws its glyphs from. */
u8 MesWinTexBuff_01[0x100];
/** Texture buffer the second common menu message window draws its glyphs from. */
u8 MesWinTexBuff_02[0x100];
/** Texture buffer the third common menu message window draws its glyphs from. */
u8 MesWinTexBuff_11[0x100];
/** Texture buffer the name message window draws its glyphs from. */
u8 MesWinTexBuff_12[0x100];

static s32        linear;   // Nonzero selects linear filtering for sprite batches.
static u_long128 *data_top; // First quadword of the open sprite batch.
static u_long128 *pdata;    // Current write cursor of the open sprite batch.
static u_int     *dma_cnt;  // DMA and VIF tag words patched when the batch closes.

void set2DSprite_Start(sceVif1Packet *packet, CTexture *texture);

void set2DSprite_Core(sceVif1Packet *packet, CTexture *texture, const CRect_i_ &screen, const CRect_i_ &texel, u8 red, u8 green, u8 blue, u8 alpha);

void set2DSprite_End(sceVif1Packet *packet, CTexture *texture);

void SetClut(sceVif1Packet *packet, CTexture *texture, i *clut);

float LinerInterpolation(float from, float to, float at) {
    return from + (at * (to - from));
}

void AreaAddPos(int *area, int *pos, int *out) {
    int left = area[0];
    int top = area[1];
    int right = area[2];
    int bottom = area[3];
    int x = pos[0];
    int y = pos[1];

    if (x < left) {
        left = x;
    }

    if (right < x) {
        right = x;
    }

    if (y < top) {
        top = y;
    }

    if (bottom < y) {
        bottom = y;
    }

    out[0] = left;
    out[1] = top;
    out[2] = right;
    out[3] = bottom;
}

void RollPos(float *centre, float *point, float angle, float *out) {
    float centre_x = centre[0];
    float centre_y = centre[1];
    float point_x = point[0];
    float point_y = point[1];

    out[0] = centre_x + ((point_x - centre_x) * cos(angle) - (point_y - centre_y) * sin(angle));
    out[1] = centre_y - ((point_x - centre_x) * sin(angle) + (point_y - centre_y) * cos(angle));
}

int CheckPosInOutForRect(RECT *rect, int x, int y) {
    s32 top;
    s32 left;

    left = rect->x;

    if (x < left) {
        return 0;
    }

    if ((left + rect->width) < x) {
        return 0;
    }

    top = rect->y;

    if (y < top) {
        return 0;
    }

    return ((top + rect->height) < y) ? 0 : 1;
}

float GetDisPosToRect(RECT *rect, int x, int y) {
    float dx = rect->x + (rect->width >> 1) - x;
    float dy = rect->y + (rect->height >> 1) - y;

    return sqrt(dx * dx + dy * dy);
}

void GetScrPosFromChar(CCharacter *chara, int *out_pos) {
    float position[4];
    int   screen[4];

    chara->GetPosition(position);
    position[1] += 0.85f * chara->body_height;
    position[3] = 1.0f;
    MGRotTransPers2D(screen, position, 0);
    out_pos[0] = screen[0];
    out_pos[1] = screen[1];
}

unsigned int Color2Clut(unsigned int colour) {
    for (int i = 0; i < 16; i++) {
        if (colour == FontColorTbl[i]) {
            return i;
        }
    }

    return 0;
}

int NameRegistCodeJtoE(int code) {
    int table[109] = {
        -683, -682, -681, -680, -679, -678, -677, -676, -675, -674,
        -673, -672, -679, -671, -670, -669, -254, -668, -667, -666,
        -665, -664, -679, -679, -679, -679, -679, -679, -657, -656,
        -655, -654, -653, -652, -651, -650, -649, -648, -679, -679,
        -679, -661, -660, -659, -658, -679, -644, -614, -608, -607,
        -679, -634, -636, -637, -635, -679, -606, -605, -604, -603,
        -679, -625, -627, -628, -626, -679, -639, -615, -602, -601,
        -679, -616, -618, -619, -617, -679, -600, -641, -642, -599,
        -679, -629, -631, -632, -630, -679, -640, -612, -598, -597,
        -679, -620, -622, -623, -621, -679, -596, -624, -643, -633,
        -613, -647, -595, -594, -679, -679, -638, -646, -645};

    if (code >= 0xD5 && code < 0x142) {
        return table[code - 0xD5];
    }

    if (code >= 0xA1 && code < 0xBB) {
        return code - 0x380;
    }

    if (code >= 0xBB && code < 0xD5) {
        return code - 0x380;
    }

    return -0x2A7;
}
