#include "common.h"

#include <libvu0.h>

#include <cstdint>
#include <cstring>

#include "boxvu0.hpp"
#include "dataalloc.hpp"
#include "edit.hpp"
#include "edit_in.hpp"
#include "editground.hpp"
#include "editloop.hpp"
#include "editloop3.hpp"
#include "editpartsinfo.hpp"
#include "eparts_port.hpp"
#include "frame.hpp"
#include "gamemode.hpp"
#include "mainselect.hpp"
#include "mapparts.hpp"
#include "mathutil.hpp"
#include "objanime.hpp"
#include "savedata.hpp"

// Retail's LoadPTS, with the .pts definition decoded into host records (eparts_port.hpp) where
// retail copied it into EPartsInfoBuff and relocated its pointer fields in place.

void CopyCMapParts(CMapParts *to, CMapParts *from, CDataAlloc2<1> *arena);

namespace {

CFrame *GetCameraFrame(CMapParts *parts) {
    if (parts->camera_frame == NULL) {
        return NULL;
    }

    parts->camera_frame->SetPosition(parts->pos[0], parts->pos[1], parts->pos[2]);
    parts->camera_frame->SetRotation(parts->rotation.x, parts->rotation.y, parts->rotation.z);
    return parts->camera_frame;
}

} // namespace

PC_OVERRIDE EPARTS_INFO_HEADER *LoadPTS(CMapParts *parts, unsigned int *archive, MAP_PARTS_INFO *info, OBJ_ANIME_SEQ *anime,
                                        EDIT_EFFECT_INFO *effects, EDIT_OBJECT_TIMER *timers, ED_EVENT_POINT *points,
                                        CMapParts *shared) {
    EPARTS_ARCHIVE     *record = (EPARTS_ARCHIVE *) archive;
    int                 i;
    EPARTS_INFO_HEADER *header;
    CFrame             *frame;

    const char      *definition = (char *) record + record->info_offset;
    EPartsDiscHeader source = EPartsReadHeader(definition);
    header = EPartsLoad(definition, &EPartsInfoBuff);

    parts->func_count = header->func_count;
    // Nothing reads func_data back; it keeps the bits retail stored.
    parts->func_data = static_cast<s32>(reinterpret_cast<std::uintptr_t>(header->func));

    if (shared != NULL) {
        CopyCMapParts(parts, shared, &DataBuffer__2);
    } else {
        char *names[9] = {NULL};

        if (record->lod0_size > 0) {
            names[0] = (char *) record + record->lod0_offset;
        } else {
            return header;
        }

        if (record->lod1_size > 0) {
            names[1] = (char *) record + record->lod1_offset;
        }

        if (record->lod2_size > 0) {
            names[2] = (char *) record + record->lod2_offset;
        }

        if (record->lod3_size > 0) {
            names[3] = (char *) record + record->lod3_offset;
        }

        if (record->shadow_size > 0) {
            names[4] = (char *) record + record->shadow_offset;
        }

        if (record->collision_size > 0) {
            names[5] = (char *) record + record->collision_offset;
        }

        if (record->shade_size > 0) {
            names[6] = (char *) record + record->shade_offset;
        }

        if (record->extra_size > 0) {
            names[7] = (char *) record + record->extra_offset;
        }

        if (record->camera_size > 0) {
            names[8] = (char *) record + record->camera_offset;
        }

        LoadMapObject(parts, (u_int **) names, &DataBuffer__2);
    }

    CFrame *frames[9];
    CBoxVu0 bound;
    CBoxVu0 part_bound;

    for (int k = 0; k < 9; k++) {
        frames[k] = NULL;
    }

    for (int k = 0; k < 4; k++) {
        CFrame *level = parts->frame[k];

        frames[k] = level;
    }

    frames[4] = parts->shadow_frame;
    frames[5] = parts->GetCollisionFrame();
    frames[6] = parts->shade_frame;
    frames[7] = parts->ripple_frame;
    frames[8] = GetCameraFrame(parts);

    if (frames[0] == NULL) {
        return NULL;
    }

    frames[0]->GetBoundBox(&bound, 1);
    frame = parts->GetCollisionFrame();

    if (frame != NULL) {
        frame->GetBoundBox(&part_bound, 1);
        VectorMaxMin(bound.max, bound.min, bound.max, bound.min, part_bound.max, part_bound.min);
    }

    frame = GetCameraFrame(parts);

    if (frame != NULL) {
        frame->GetBoundBox(&part_bound, 1);
        VectorMaxMin(bound.max, bound.min, bound.max, bound.min, part_bound.max, part_bound.min);
    }

    memcpy(&parts->bound, &bound, sizeof(CBoxVu0));

    for (int k = 0; k < 8; k++) {
        int anime_no = info->anime[k];

        if (anime_no > 0) {
            InitObjAnime(frames, &anime[anime_no]);
        }
    }

    EPARTS_FUNC_DATA *funcs = header->func;

    EditMapInfo->event_count += EdInitEventPoint(parts, info->events, funcs, header->func_count, points, 0x100);

    EPARTS_FUNC_DATA *func = header->func;

    for (i = 0; i < header->func_count; i++, func++) {
        if (func->completion_flag > 0 && SaveData->GetMapInitFlag(MapNo, func->completion_flag) == 0) {
            SaveData->SetMapInitFlag(MapNo, func->completion_flag, 1);
            SaveData->SetMapFlag(MapNo, func->completion_flag, !(char) func->unk_28[0]);
        }

        EnterPartsEffect(parts, func, effects, 0x40);

        if (EditMapInfo->obj_anime_count < 128 &&
            InitObjAnime(frames, 9, func, &anime[EditMapInfo->obj_anime_count]) != 0) {
            EditMapInfo->obj_anime_count++;
        }

        if (func->kind == EPARTS_FUNC_OBJ_TIMER) {
            EDIT_OBJECT_TIMER *timer = &timers[EditMapInfo->object_timer_count++];

            if (EditMapInfo->object_timer_count > 128) {
                continue;
            }

            strcpy(timer->name, (char *) func->frame_name);
            timer->start_time = ConvertTime(func->start_time);
            timer->end_time = ConvertTime(func->end_time);
            timer->object = parts;
        }

        if (func->kind == EPARTS_FUNC_VILLAGER) {
            pEditGround->people[pEditGround->people_count++] = func;
            func->parts = parts;
        }
    }

    sceVu0FVECTOR position = {0.0f, 0.0f, 0.0f, 1.0f};

    position[0] = source.position[0];
    position[1] = source.position[1];
    position[2] = source.position[2];
    parts->SetPosition(position);
    parts->SetRotation(source.rotation[0], source.rotation[1], source.rotation[2]);
    return header;
}
