#pragma helper_mask_gpr 0x30
#pragma helper_mask_fpr 0x1000
#pragma name_counter 338

#include "common.h"

#include <libgraph.h>
#include <libpkt.h>
#include <libvu0.h>
#include <sifdev.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "actionseq.hpp"
#include "battlemenu.hpp"
#include "boxvu0.hpp"
#include "btmisc.hpp"
#include "btsysscript.hpp"
#include "camera.hpp"
#include "camerafollow.hpp"
#include "character.hpp"
#include "clsmes.hpp"
#include "dataalloc.hpp"
#include "dataread.hpp"
#include "debugfont.hpp"
#include "dngstatusdata.hpp"
#include "ebattle.hpp"
#include "editground.hpp"
#include "editloop.hpp"
#include "editloop3.hpp"
#include "editpartsinfo.hpp"
#include "effect.hpp"
#include "fishing.hpp"
#include "frame.hpp"
#include "framevu1.hpp"
#include "gamemode.hpp"
#include "gamepad.hpp"
#include "gameutil.hpp"
#include "mainselect.hpp"
#include "mapparts.hpp"
#include "mathutil.hpp"
#include "mdt.hpp"
#include "menu_draw.hpp"
#include "menu_misc.hpp"
#include "menu_save.hpp"
#include "mglib.hpp"
#include "nowload.hpp"
#include "npcharacter.hpp"
#include "objanime.hpp"
#include "rect.hpp"
#include "runscript.hpp"
#include "savedata.hpp"
#include "snd.hpp"
#include "spritetable.hpp"
#include "sysmes.hpp"
#include "textureanime.hpp"
#include "visualvu1.hpp"

/* Retail editloop3.cpp: editor event points, villagers, script opcodes and talk handling. */

/**
 * Visibility commands an event queues for one villager, as VILLAGER_APPEAR_STATE::action holds them.
 */
// clang-format off
enum VillagerAppearAction {
    VILLAGER_APPEAR_DEFAULT = 0, /**< No command; the villager follows its schedule. */
    VILLAGER_APPEAR_IN      = 1, /**< Villager enters: left off the map but let in. */
    VILLAGER_APPEAR_OUT     = 2, /**< Villager leaves: put on the map but not let back in. */
    VILLAGER_APPEAR_OFF     = 3, /**< Villager is kept off the map. */
    VILLAGER_APPEAR_ON      = 4, /**< Villager is put on the map. */
};

// clang-format on

/**
 * Steps of a villager conversation, as EdTalkMode's talk_mode holds them.
 */
// clang-format off
enum TalkModeState {
    TALK_MODE_SELECT  = 0, /**< Choice prompt is shown with its cursor. */
    TALK_MODE_MESSAGE = 1, /**< Plain message is shown until it is closed. */
};

// clang-format on

ED_EVENT_POINT *GetNewEventPoint(CMapParts *parts, EPARTS_FUNC_DATA *function, ED_EVENT_POINT *points, int count) {
    ED_EVENT_POINT *point = GetNewEventPoint(points, count);

    if (point == NULL) {
        printf("event over!!\n");
        return NULL;
    }

    point->enabled = true;

    if (parts->parts_no >= 0) {
        point->map_object = NULL;
        point->parts_no = parts->parts_no;
    } else {
        point->map_object = parts;
        point->parts_no = -1;
    }

    sceVu0CopyVector(point->position, function->position);
    sceVu0CopyVector(point->rotation, function->rotation);
    point->start_time = ConvertTime(function->start_time);
    point->end_time = ConvertTime(function->end_time);
    point->completion_flag = function->completion_flag;
    CFrame *frame = parts->frame[0];
    point->frame = NULL;

    if (frame != NULL) {
        point->frame = frame->SearchFrame(function->frame_name);
    }

    return point;
}

int EdInitEventPoint(CMapParts *parts, short *indices, EPARTS_FUNC_DATA *functions, int function_count, ED_EVENT_POINT *points, int point_count) {
    int has_extent;
    int created;
    int j;

    for (int i = 0; i < 8 && indices != NULL; i++) {
        int point_index = indices[i];

        if (point_index <= 0) {
            continue;
        }

        ED_EVENT_POINT *point = &points[point_index];

        if (point->event_type != ED_EVENT_POINT_DOOR) {
            continue;
        }

        point->enabled = true;

        if (parts->parts_no >= 0) {
            point->map_object = NULL;
            point->parts_no = parts->parts_no;
        } else {
            point->map_object = parts;
            point->parts_no = -1;
        }

        has_extent = 0;
        EPARTS_FUNC_DATA *function = functions;

        for (int j = 0; j < function_count; j++, function++) {
            int kind = function->kind;

            if (kind != EPARTS_FUNC_DOOR_EXTENT && kind != EPARTS_FUNC_DOOR_SIDE_B && kind != EPARTS_FUNC_DOOR_SIDE_A && kind != EPARTS_FUNC_DOOR) {
                kind = function->kind;
            } else if (function->link_id == points[point_index].map_no) {
                if (kind == EPARTS_FUNC_DOOR) {
                    sceVu0CopyVector(point->position, function->position);
                    sceVu0CopyVector(point->rotation, function->rotation);
                    point->trigger_range[2] = 10.0f;
                    point->trigger_range[1] = 10.0f;
                    point->trigger_range[0] = 10.0f;
                    point->minimum_progress = (int) function->values[0];
                    point->start_time = ConvertTime(function->start_time);
                    point->end_time = ConvertTime(function->end_time);
                    point->completion_flag = function->completion_flag;

                    if (has_extent == 0) {
                        sceVu0FVECTOR offset = {40.0f, 50.0f, -80.0f, 1.0f};
                        sceVu0FMATRIX matrix;
                        sceVu0UnitMatrix(matrix);
                        sceVu0RotMatrixY(matrix, matrix, point->rotation[1]);
                        sceVu0ApplyMatrix(offset, matrix, offset);
                        sceVu0AddVector(point->extent, offset, point->position);
                    }
                }

                if (function->kind == EPARTS_FUNC_DOOR_EXTENT) {
                    sceVu0CopyVector(point->extent, function->position);
                    has_extent = 1;
                }

                if (function->kind == EPARTS_FUNC_DOOR_SIDE_A) {
                    point->side = 1;
                }

                if (function->kind == EPARTS_FUNC_DOOR_SIDE_B) {
                    point->side = -1;
                }

                if (function->kind == EPARTS_FUNC_DOOR_SIDE_A || function->kind == EPARTS_FUNC_DOOR_SIDE_B) {
                    point->linked_value = (int) function->values[0];
                }
            }
        }
    }

    created = 0;
    EPARTS_FUNC_DATA *function = functions;

    for (int i = 0; i < function_count; i++, function++) {
        ED_EVENT_POINT *point = NULL;

        switch (function->kind) {
            case EPARTS_FUNC_ITEM_BOX:
                point = GetNewEventPoint(parts, function, points, point_count);

                if (point != NULL) {
                    point->event_type = ED_EVENT_POINT_ITEM_BOX;
                    point->trigger_range[2] = 15.0f;
                    point->trigger_range[1] = 15.0f;
                    point->trigger_range[0] = 15.0f;
                    point->side = (int) function->values[0];
                    point->linked_value = (int) function->values[1];
                    point->minimum_progress = (int) function->values[2];
                    point->secondary_progress = (int) function->values[3];
                }

                break;
            case EPARTS_FUNC_EVENT:
                if ((int) function->values[0] > 0) {
                    point = GetNewEventPoint(parts, function, points, point_count);

                    if (point != NULL) {
                        point->event_type = ED_EVENT_POINT_EVENT;
                        sceVu0CopyVector(point->trigger_range, function->parameters);
                        point->side = (int) function->values[0];
                        point->linked_value = (int) function->values[1];
                    }
                }

                break;
            case EPARTS_FUNC_LADDER_BOTTOM:
                point = GetNewEventPoint(parts, function, points, point_count);

                if (point != NULL) {
                    point->event_type = ED_EVENT_POINT_LADDER_BOTTOM;
                    point->trigger_range[2] = 6.0f;
                    point->trigger_range[1] = 6.0f;
                    point->trigger_range[0] = 6.0f;
                    point->trigger_range[3] = function->values[0];
                    point->side = (int) function->values[1];

                    for (j = 0; j < function_count; j++) {
                        EPARTS_FUNC_DATA *other = &functions[j];

                        if (other->kind == EPARTS_FUNC_LADDER_TOP && other->link_id == function->link_id) {
                            sceVu0CopyVector(point->extent, other->position);
                            point->linked_value = (int) other->values[1];
                            break;
                        }
                    }
                }

                break;
            case EPARTS_FUNC_LADDER_TOP:
                point = GetNewEventPoint(parts, function, points, point_count);

                if (point != NULL) {
                    point->event_type = ED_EVENT_POINT_LADDER_TOP;
                    point->trigger_range[2] = 6.0f;
                    point->trigger_range[1] = 6.0f;
                    point->trigger_range[0] = 6.0f;
                    point->trigger_range[3] = function->values[0];
                    point->linked_value = (int) function->values[1];

                    for (j = 0; j < function_count; j++) {
                        EPARTS_FUNC_DATA *other = &functions[j];

                        if (other->kind == EPARTS_FUNC_LADDER_BOTTOM && other->link_id == function->link_id) {
                            sceVu0CopyVector(point->extent, other->position);
                            point->side = (int) other->values[1];
                            break;
                        }
                    }
                }

                break;
        }

        if (point != NULL) {
            created++;
        }
    }

    return created;
}

int EdGetEvent(ED_EVENT_POINT *points, int count, ED_EVENT_PARAM *param, float *position, float *rotation, float time) {
    CEditGround    *ground = EdExchangeInfo.ground;
    int             i;
    ED_EVENT_POINT *point = points;
    float           nearest = -1.0f;

    for (i = 0; i < count; i++, point++) {
        if (!CheckEventPoint(point, time)) {
            continue;
        }

        sceVu0FVECTOR event_position;
        sceVu0FVECTOR event_rotation;
        sceVu0FVECTOR camera_position;
        sceVu0FVECTOR camera_rotation;
        sceVu0CopyVector(event_position, point->position);
        sceVu0CopyVector(camera_position, point->extent);
        sceVu0CopyVector(event_rotation, point->rotation);
        sceVu0CopyVector(camera_rotation, point->rotation);

        CMapObject *object = point->map_object;

        if (point->parts_no >= 0) {
            object = ground->GetPartsObject(point->parts_no);

            if (object == NULL) {
                continue;
            }

            GetPosRot(object, event_position, event_rotation);
            GetPosRot(object, camera_position, camera_rotation);
        } else {
            if (object != NULL) {
                GetPosRot(object, event_position, event_rotation);
            }

            if (object != NULL) {
                GetPosRot(object, camera_position, camera_rotation);
            }
        }

        float distance;

        if (point->event_type == ED_EVENT_POINT_ITEM_BOX) {
            sceVu0FVECTOR delta;
            sceVu0SubVector(delta, event_position, position);
            delta[1] = 0.0f;
            distance = DistVector(delta);

            if (event_position[1] < position[1] - 1.0f || !(event_position[1] <= position[1] + 20.0f)) {
                distance = 1000000.0f;
            }
        } else {
            distance = DistVector(event_position, position);
        }

        int selected = 0;

        if (distance < point->trigger_range[0]) {
            selected = 1;

            switch (point->event_type) {
                case ED_EVENT_POINT_LADDER_BOTTOM:
                case ED_EVENT_POINT_DOOR:
                    if (AngleCmp(rotation[1], event_rotation[1], 0.8f)) {
                        selected = 0;
                    }

                    break;
                case ED_EVENT_POINT_LADDER_TOP:
                    if (AngleCmp(AngleLimit(rotation[1] - 3.14f), event_rotation[1], 0.8f)) {
                        selected = 0;
                    }

                    break;
            }
        }

        if (selected && (nearest < 0.0f || !(nearest <= point->trigger_range[0]))) {
            nearest = point->trigger_range[0];
            param->kind = point->event_type;
            param->parts_no = point->parts_no;
            sceVu0CopyVector(param->position, event_position);
            sceVu0CopyVector(param->camera_pos, camera_position);
            sceVu0CopyVector(param->rotation, event_rotation);
            sceVu0FVECTOR offset = {40.0f, 50.0f, -80.0f, 1.0f};
            sceVu0FMATRIX matrix;
            sceVu0UnitMatrix(matrix);
            sceVu0RotMatrixY(matrix, matrix, event_rotation[1]);
            sceVu0ApplyMatrix(offset, matrix, offset);
            sceVu0AddVector(offset, offset, event_position);
            sceVu0CopyVector((float *) param->unk_40, offset);
            param->point = point;
        }
    }

    if (nearest > 0.0f) {
        return 1;
    }

    return 0;
}

void EdEventPointDraw(ED_EVENT_POINT *points, int count, float time) {
    if (EdDrawOffMap != 0) {
        return;
    }

    CEditGround    *ground = EdExchangeInfo.ground;
    CFrame         *marker = EdExchangeInfo.event_marker;
    C3DSprite      *effect = EdExchangeInfo.system_effect;
    ED_EVENT_POINT *point = points;
    static float    roty = 0.0f;
    roty += 0.05f;

    if (roty > PI_SHORT) {
        roty -= TWO_PI;
    }

    static float sys_eff_sc = 0.0f;
    static int   sys_eff_cnt = 0;

    if (sys_eff_sc > 1.0f) {
        sys_eff_sc = 0.0f;
        sys_eff_cnt = rand() % 20 + 10;
    }

    if (sys_eff_cnt < 0) {
        sys_eff_sc += 0.3f;
        sys_eff_cnt = 0;
    }

    sys_eff_cnt--;

    for (int i = 0; i < count; i++, point++) {
        if (CheckEventPoint(point, time) == 0) {
            continue;
        }

        switch (point->event_type) {
            case ED_EVENT_POINT_ITEM_BOX:
                break;
            case ED_EVENT_POINT_EVENT:
                if (point->linked_value <= 0) {
                    continue;
                }

                break;
            default:
                continue;
        }

        sceVu0FVECTOR position;
        sceVu0FVECTOR rotation;
        sceVu0CopyVector(position, point->position);
        sceVu0CopyVector(rotation, point->rotation);
        CMapObject *object = point->map_object;

        if (point->parts_no >= 0) {
            object = ground->GetPartsObject(point->parts_no);

            if (object == NULL) {
                continue;
            }

            GetPosRot(object, position, rotation);
        } else if (object != NULL) {
            GetPosRot(object, position, rotation);
        }

        if (point->event_type == ED_EVENT_POINT_ITEM_BOX && marker != NULL) {
            marker->SetPosition(position);
            marker->SetRotation(0.0f, rotation[1], 0.0f);
            MGDraw(marker);
        }

        if (point->event_type == ED_EVENT_POINT_EVENT) {
            sceVu0CopyVector(effect->position, position);
            effect->half_width = 2.0f * sys_eff_sc;
            effect->half_height = sys_eff_sc;
            effect->position[1] += 1.0f;
            effect->Draw();
        }
    }
}

int EdEventPointCpPoly(float *position, ED_EVENT_POINT *points, int count, CCPoly *polygons, float time) {
    CEditGround      *ground = EdExchangeInfo.ground;
    static CCharacter chara;
    chara.body_width = 3.0f;
    int i;
    int found = 0;

    for (i = 0; i < count; i++, points++) {
        if (points->event_type == ED_EVENT_POINT_ITEM_BOX && CheckEventPoint(points, time)) {
            sceVu0FVECTOR event_position;
            sceVu0FVECTOR event_rotation;
            sceVu0CopyVector(event_position, points->position);
            sceVu0CopyVector(event_rotation, points->rotation);
            CMapObject *object = points->map_object;

            if (points->parts_no >= 0) {
                object = ground->GetPartsObject(points->parts_no);

                if (object == NULL) {
                    continue;
                }

                GetPosRot(object, event_position, event_rotation);
            } else if (object != NULL) {
                GetPosRot(object, event_position, event_rotation);
            }

            chara.SetPosition(event_position);
            found += chara.PickUpPoly(position, polygons);
        }
    }

    return found;
}

int EdSearchEvent(ED_EVENT_PARAM *param, char *name, int map_no, float time) {
    int             i;
    CEditGround    *ground = EdExchangeInfo.ground;
    ED_EVENT_POINT *point = EditMapInfo->work.events.points;

    for (i = 0; i < 256; i++, point++) {
        if (CheckEventPoint(point, time) && strcmp(point->destination, name) == 0 && point->map_no == map_no) {
            sceVu0FVECTOR event_position;
            sceVu0FVECTOR event_rotation;
            sceVu0FVECTOR camera_position;
            sceVu0FVECTOR camera_rotation;
            sceVu0CopyVector(event_position, point->position);
            sceVu0CopyVector(camera_position, point->extent);
            sceVu0CopyVector(event_rotation, point->rotation);
            sceVu0CopyVector(camera_rotation, point->rotation);
            CMapObject *object = point->map_object;

            if (point->parts_no >= 0) {
                object = ground->GetPartsObject(point->parts_no);

                if (object != NULL) {
                    GetPosRot(object, event_position, event_rotation);
                }

                if (object != NULL) {
                    GetPosRot(object, camera_position, camera_rotation);
                }
            } else {
                if (object != NULL) {
                    GetPosRot(object, event_position, event_rotation);
                }

                if (object != NULL) {
                    GetPosRot(object, camera_position, camera_rotation);
                }
            }

            param->kind = point->event_type;
            param->parts_no = point->parts_no;
            sceVu0CopyVector(param->position, event_position);
            sceVu0CopyVector(param->camera_pos, camera_position);
            sceVu0CopyVector(param->rotation, event_rotation);
            param->point = point;
            return 1;
        }
    }

    return 0;
}

void EdMapJump(int kind, char *name) {
    if (name == NULL || *name == '\0') {
        return;
    }

    int index;

    if (kind >= 3) {
        index = kind;
    }

    char *extensions[5] = {"m", "e", "n", "", ""};
    StartReadBG();
    u_long128 *buffer = (u_long128 *) read_buffer;
    char       filename[64];
    int        size;
    sprintf(filename, "%s.scn", name);

    if (LoadFileBG(filename, buffer, &size) == 0) {
        name[strlen(name) - 1] = '\0';
        sprintf(filename, "%s.scn", name);

        if (LoadFileBG(filename, buffer, &size) == 0) {
            return;
        }
    }

    buffer += (((size >> 6) + 1) << 6) >> 4;
    sprintf(filename, "%s.cfg", name);
    LoadFileBG(filename, buffer, &size);
    buffer = (u_long128 *) (EdNPCBuffer.base + EdNPCBuffer.used * 16);
    sprintf(filename, "%s.pak", name);

    if (LoadFileBG(filename, buffer, &size) == 0) {
        sprintf(filename, "%s.img", name);

        if (LoadFileBG(filename, buffer, &size) == 0) {
            return;
        }
    }

    if (size > 0x200000) {
        printf("img size over!!!\n");

        while (1) {
        }
    }
}

void EdPartsObjectOnOff(CMapParts *parts, EDITPARTS_INFO *info, int mode) {
    char  storage[32][32];
    char *names[32];
    int   draw_flag[2] = {2, 1};
    int   hide_flag[2] = {4, 1};

    for (int element = 0; element < 6; element++) {
        if (info->elements[element].id < 0 || info->elements[element].id >= 36) {
            continue;
        }

        int enabled = info->elements[element].enabled != 0;

        for (int i = 0; i < 32; i++) {
            names[i] = storage[i];
            names[i][0] = '\0';
        }

        GetElementObjName(info, names, element, mode);

        char prefix;

        for (int i = 0; (prefix = names[i][0]) != '\0'; i++) {
            if (prefix == '*') {
                if (enabled) {
                    parts->FrameObjectOnOff(names[i] + 1, draw_flag[1]);
                } else {
                    parts->FrameObjectOnOff(names[i] + 1, 0);
                }
            }

            if (names[i][0] == '-') {
                parts->FrameObjectOnOff(names[i] + 1, draw_flag[!enabled]);
            } else {
                parts->FrameObjectOnOff(names[i], draw_flag[enabled]);
            }
        }
    }
}

/** Holds a pending visibility or movement command for one villager. */
struct VILLAGER_APPEAR_STATE {
    int action;      /**< Visibility transition to apply. */
    int priority;    /**< Priority of the command which last updated the state. */
    int destination; /**< Villager slot used by a movement command. */
};

static int appear_table[10];

static int select_table[10];

/** Pending appearance commands for the event villagers. */
static VILLAGER_APPEAR_STATE appear[16];

void EdInitVillagerControl() {
    for (int i = 0; i < 16; i++) {
        appear[i].action = VILLAGER_APPEAR_DEFAULT;
        appear[i].destination = -1;
        appear[i].priority = -1;
    }
}

void EdCreateVillagerTable(EDIT_MAP_INFO *info) {
    int candidates[16];
    int i;
    int period;
    int candidate_count = 0;

    for (i = 0; i < 16; i++) {
        candidates[i] = -1;
    }

    for (i = 0; i < 15; i++) {
        int villager = info->people_list[i];

        if (villager >= 0) {
            candidates[candidate_count++] = villager;
        }
    }

    if (candidate_count > 0) {
        for (period = 0; period < 4; period++) {
            for (i = 0; i < 100; i++) {
                int first = rand() % candidate_count;
                int second = rand() % candidate_count;
                int value = candidates[first];
                candidates[first] = candidates[second];
                candidates[second] = value;
            }

            for (i = 0; i < 10; i++) {
                info->time_tables[0][period][i] = candidates[i];
            }
        }
    }
}

void EdInitVillagerTable(float clock, EDIT_MAP_INFO *info) {
    for (int i = 0; i < 10; i++) {
        appear_table[i] = info->time_tables[0][EdGetTime(clock)][i];
    }
}

/** Saved state restored into the first ten villager entries. */
static VILLAGER_INFO restore_info[10];

void EdVillagerAppearOut(int index, int priority) {
    if (index < 0 || index >= 16) {
        return;
    }

    if (appear[index].priority > priority) {
        return;
    }

    appear[index].action = VILLAGER_APPEAR_OUT;
    appear[index].priority = priority;
}

void EdVillagerAppearIn(int index, int priority) {
    if (index < 0 || index >= 16) {
        return;
    }

    if (appear[index].priority > priority) {
        return;
    }

    appear[index].action = VILLAGER_APPEAR_IN;
    appear[index].priority = priority;
}

void EdVillagerAppearOff(int index, int priority) {
    if (index < 0 || index >= 16) {
        return;
    }

    if (appear[index].priority > priority) {
        return;
    }

    appear[index].action = VILLAGER_APPEAR_OFF;
    appear[index].priority = priority;
}

void EdVillagerAppearOn(int index, int priority) {
    if (index < 0 || index >= 16) {
        return;
    }

    if (appear[index].priority > priority) {
        return;
    }

    appear[index].action = VILLAGER_APPEAR_ON;
    appear[index].priority = priority;
}

void EdVillagerAppearMove(int index, int destination, int priority) {
    if (index < 0 || index >= 16) {
        return;
    }

    if (appear[index].priority > priority) {
        return;
    }

    appear[index].destination = destination;
}

static void RestoreVillagerInfo(VILLAGER_INFO *villagers) {
    for (int i = 0; i < 10; i++) {
        villagers[i] = restore_info[i];
    }
}

/**
 * Chooses the villagers on the map for the given time and fills the villager table.
 *
 * @mangled EdSelectVillager__FP13VILLAGER_INFOfP13EDIT_MAP_INFO
 * @address 0x1857E0
 * @size 0x30C
 * @note Returns the copy loop's final counter (10); callers ignore it.
 */
int EdSelectVillager(VILLAGER_INFO *villagers, float clock, EDIT_MAP_INFO *map_info) {
    EdInitVillagerTable(clock, map_info);
    EdGetTime(clock);

    int i;

    for (i = 0; i < 10; i++) {
        select_table[i] = -1;
    }

    int selected_count = 0;

    for (i = 0; i < 16 && selected_count < 6; i++) {
        int action = appear[i].action;

        if (action == VILLAGER_APPEAR_ON || action == VILLAGER_APPEAR_OUT) {
            select_table[selected_count++] = i;
        }
    }

    for (i = 0; i < 10 && selected_count < 6; i++) {
        int j;
        int duplicate;
        int index = appear_table[i];

        if (index < 0) {
            break;
        }

        int action = appear[index].action;

        if (action == VILLAGER_APPEAR_OFF || action == VILLAGER_APPEAR_IN) {
            continue;
        }

        duplicate = 0;

        for (j = 0; j < selected_count; j++) {
            int selected = select_table[j];

            if (selected == index) {
                duplicate = 1;
                break;
            }
        }

        if (duplicate == 0) {
            select_table[selected_count++] = index;
        }
    }

    for (i = 0; i < 10; i++) {
        int index = select_table[i];

        if (index < 0) {
            villagers[i].name[0] = '\0';
        } else {
            villagers[i] = map_info->villagers[index];
            VILLAGER_APPEAR_STATE *state = &appear[select_table[i]];

            if (state->action != VILLAGER_APPEAR_DEFAULT && state->destination >= 0) {
                villagers[i].initial_motion = state->destination;
            }

            restore_info[i] = villagers[i];
        }
    }

    if (i < 9) {
        villagers[i].name[0] = '\0';
    }

    return i;
}

int EdCheckVillagerIn(int index, VILLAGER_INFO *villager) {
    if (index < 0 || index >= 16) {
        return 0;
    }

    switch (appear[index].action) {
        case VILLAGER_APPEAR_OFF:
        case VILLAGER_APPEAR_OUT:
            return 0;
        case VILLAGER_APPEAR_DEFAULT:
            for (int i = 0; i < 10; i++) {
                if (index == select_table[i]) {
                    return 0;
                }
            }
        case VILLAGER_APPEAR_IN: {
            int parts_no = villager->character_no;

            if (parts_no < 0) {
                return 1;
            }

            EDITPARTS_INFO *parts = EditPartsInfo.GetPartsInfo(parts_no);

            if (parts == NULL) {
                return 0;
            }

            int model = villager->model_no;

            if (model < 0) {
                return 0;
            }

            return parts->elements[model].enabled != 0 ? 1 : 0;
        }
        default:
            return 0;
    }
}

int EdCheckVillager(int index, VILLAGER_INFO *villager, CEditGround *ground) {
    if (index < 0 || index >= 16) {
        return 0;
    }

    if (ground == NULL) {
        return 0;
    }

    int parts_no = villager->character_no;

    if (parts_no < 0) {
        return 1;
    }

    EDITPARTS_INFO *parts = EditPartsInfo.GetPartsInfo(parts_no);

    if (parts == NULL) {
        return 0;
    }

    if (ground->GetPartsObject(villager->character_no) == NULL) {
        return 0;
    }

    int model = villager->model_no;

    if (model < 0) {
        return 0;
    }

    return parts->elements[model].enabled != 0 ? 1 : 0;
}

static int GetRandomVillager(VILLAGER_INFO *villagers) {
    int candidates[10] = {-1};
    int count = 0;
    int i;

    for (i = 0; i < 10; i++) {
        if (villagers[i].placed == 0 && villagers[i].name[0] != '\0') {
            candidates[count++] = i;
        }
    }

    if (count <= 0) {
        return -1;
    }

    return candidates[rand() % count];
}

static int GetRandomMoveVillager(VILLAGER_INFO *villagers) {
    int candidates[10] = {-1};
    int count = 0;
    int i;

    for (i = 0; i < 10; i++) {
        if (villagers[i].placed == 0 && villagers[i].name[0] != '\0' && villagers[i].initial_motion != 0) {
            candidates[count++] = i;
        }
    }

    if (count <= 0) {
        return -1;
    }

    return candidates[rand() % count];
}

void EdInitVilager(VILLAGER_INFO *villagers, CEditGround *ground, u_long128 *buffer) {
    VILLAGER_INFO *info[10];
    char           path[64];
    char           directory[64];
    int            size;
    int            i;
    u_long128     *load_buffer = (u_long128 *) read_buffer;

    if (buffer != NULL) {
        load_buffer = buffer;
    }

    for (i = 0; i < 10; i++) {
        info[i] = &villagers[i];
        villagers[i].placed = false;
        EdVillager[i].Initialize();
        EdVillager[i].texture_block = i + 54;
        strcpy(EdVillager[i].resource_name, info[i]->name);
        GetEditDataDir(directory);
        sprintf(path, "%schara/%s.chr", directory, info[i]->name);

        if (buffer != NULL) {
            LoadFileBG(path, load_buffer, &size);
            load_buffer += (((size >> 6) + 1) << 6) >> 4;
        } else {
            EdLoadVillager(info[i]->name, &EdVillager[i], &EdVillagerBuffer);
        }
    }
}

int EdLoadVillager(u_int *pack, char *name, CNPCharacter *villager, CDataAlloc2<1> *arena) {
    char copied_name[32];
    char config_name[32];
    int  texture_set = villager->texture_block;
    strcpy(copied_name, name);
    villager->texture_block = texture_set;
    strcpy(villager->resource_name, copied_name);

    if (GetPackFile(pack, "info.cfg", NULL) != NULL) {
        sprintf(config_name, "info.cfg");
    } else {
        sprintf(config_name, "%s.cfg", name);
    }

    villager->LoadPackData2(pack, config_name, arena, texture_set, arena, 0);

    if (villager->frame != NULL) {
        CFrameAttr attr;
        attr.clip_enable = false;
        attr.fog_enable = true;
        villager->frame->SetAttr(attr, 1, 4);
    }

    villager->initialized = true;
    villager->alpha_step = 8;

    if (MapNo > 10) {
        villager->initialized = true;
        villager->draw_enabled = true;
        villager->event_status = 0;
        villager->ambient_offset[3] = 128.0f;
        sceVu0FVECTOR origin = {0.0f, 0.0f, 0.0f, 0.0f};
        sceVu0FVECTOR no_rotation = {0.0f, 0.0f, 0.0f, 0.0f};
        villager->SetPosition(origin);
        villager->SetRotation(no_rotation);
        villager->motion_no = 0;
        villager->motion_flags = 0;
        villager->motion_speed = -1.0f;
    }

    villager->texture_block = texture_set;
    return 1;
}

int EdLoadVillager(char *name, CNPCharacter *villager, CDataAlloc2<1> *arena) {
    char path[64];
    char directory[64];

    if (name == NULL || *name == '\0') {
        return 0;
    }

    GetEditDataDir(directory);
    sprintf(path, "%schara/%s.chr", directory, name);

    if (LoadFile2(path, (void *) read_buffer, NULL, 0) != 0) {
        return EdLoadVillager(read_buffer, name, villager, arena);
    }

    return 0;
}

void EdInitVillagerOnOff(CNPCharacter *characters, VILLAGER_INFO *villagers, CEditGround *ground) {
    for (int i = 0; i < 10; i++) {
        VILLAGER_INFO *villager = &villagers[i];

        if (villager->placed != 0) {
            int draw = false;
            int parts_no = villager->character_no;

            if (parts_no >= 0 && characters[i].villager_id >= 0) {
                characters[i].map_parts_no = parts_no;

                if (ground->GetPartsObject(villager->character_no) != NULL) {
                    EDITPARTS_INFO *parts = EditPartsInfo.GetPartsInfo(villager->character_no);

                    if (parts->obtained > 0 && parts->elements[villager->model_no].enabled != 0) {
                        draw = true;
                    }

                    if (EditPartsInfo.CheckComplete(villager->character_no) != 0 && EditPartsInfo.GetCompEvent(villager->character_no) == 0 && villager->hide_when_complete != 0) {
                        draw = false;
                    }
                }
            } else {
                characters[i].map_parts_no = parts_no;
                draw = true;
            }

            characters[i].draw_enabled = draw;
        }
    }
}

void EdInitVilagerPosition(CNPCharacter *villagers, VILLAGER_INFO *info, CEditGround *ground, float (*transform)[4]) {
    int i;

    if (MapNo > 10) {
        RestoreVillagerInfo(info);

        for (i = 0; i < 10; i++) {
            int draw = true;

            if (info[i].index >= 0) {
                VILLAGER_INFO *entry = &info[i];
                sceVu0FVECTOR  position;
                sceVu0FVECTOR  rotation;
                sceVu0FVECTOR  people_position;

                if (ground->GetPeoplePos(entry->index, people_position) != 0) {
                    rotation[2] = 0.0f;
                    rotation[0] = 0.0f;
                    rotation[1] = people_position[3];
                    people_position[3] = 1.0f;
                    entry->placed = true;
                    sceVu0CopyVector(position, people_position);
                    sceVu0CopyVector(entry->position, position);
                    sceVu0CopyVector(entry->rotation, rotation);
                } else {
                    draw = false;
                }

                villagers[i].ClearSeq();
                villagers[i].event_status = 0;
                villagers[i].villager_id = entry->index;
                villagers[i].SetPosition(position);
                villagers[i].SetRotation(rotation[0], rotation[1], rotation[2]);
                villagers[i].draw_enabled = draw;
            }
        }

        return;
    }

    int villager_count = 0;

    while (info[villager_count].name[0] != '\0') {
        villager_count++;
    }

    RestoreVillagerInfo(info);
    int stationary_target = villager_count >> 1;
    int moving_target = villager_count - stationary_target;
    int random_state = rand();

    for (i = 0; i < 10; i++) {
        info[i].placed = false;
    }

    while (info[villager_count].name[0] != '\0') {
        villager_count++;
    }

    int stationary_count = 0;

    for (i = 0; i < 10; i++) {
        if (info[i].placed == 0 && info[i].initial_motion == 0) {
            stationary_count++;
            info[i].placed = true;

            if (stationary_count >= stationary_target) {
                break;
            }
        }
    }

    while (stationary_count < stationary_target) {
        int index = GetRandomVillager(info);
        info[index].placed = true;
        info[index].initial_motion = 0;
        stationary_count++;
    }

    int moving_count = 0;

    while (moving_count < moving_target) {
        int selected = GetRandomMoveVillager(info);

        if (selected < 0) {
            break;
        }

        info[selected].placed = true;
        info[selected].initial_motion = 1;
        int           failed = 0;
        int           avoid_count = 0;
        sceVu0FVECTOR avoid_positions[11];

        if (transform != NULL) {
            sceVu0CopyVector(avoid_positions[avoid_count++], *transform);
        }

        for (i = 0; i < 10; i++) {
            if (info[selected].initial_motion != 0) {
                sceVu0CopyVector(avoid_positions[avoid_count++], info[i].position);
            }
        }

        for (i = 0; i < avoid_count; i++) {
            avoid_positions[i][3] = 200.0f;
        }

        sceVu0FVECTOR random_position;
        sceVu0FVECTOR reference;
        reference[3] = -1.0f;
        CMapObject *object = ground->GetPartsObject(info[selected].character_no);

        if (object != NULL) {
            sceVu0CopyVector(reference, object->pos);
            reference[3] = 200.0f;
        }

        for (;;) {
            if (ground->GetRandomPlanePos(random_position, avoid_positions, avoid_count, reference) != 0) {
                sceVu0CopyVector(info[selected].position, random_position);
                break;
            }

            if (reference[3] > 500.0f) {
                reference[3] = -1.0f;

                if (ground->GetRandomPlanePos(random_position, avoid_positions, avoid_count, reference) != 0) {
                    sceVu0CopyVector(info[selected].position, random_position);
                    break;
                }

                failed = 1;
                break;
            }

            reference[3] += 100.0f;
        }

        if (failed != 0) {
            info[selected].placed = false;
        }

        moving_count++;
    }

    int selected = GetRandomVillager(info);

    while (selected >= 0) {
        info[selected].placed = true;
        info[selected].initial_motion = 0;
        selected = GetRandomVillager(info);
    }

    for (i = 0; i < 10; i++) {
        VILLAGER_INFO *entry = &info[i];

        if (entry->placed != 0) {
            sceVu0FVECTOR position;
            sceVu0FVECTOR rotation;
            sceVu0CopyVector(position, entry->position);
            sceVu0CopyVector(rotation, entry->rotation);
            int draw = false;
            int parts_no = entry->character_no;

            if (parts_no >= 0) {
                villagers[i].map_parts_no = parts_no;

                if (ground->GetPartsObject(entry->character_no) != NULL) {
                    EDITPARTS_INFO *parts = EditPartsInfo.GetPartsInfo(entry->character_no);

                    if (parts->obtained > 0 && parts->elements[entry->model_no].enabled != 0) {
                        draw = true;
                    }

                    if (EditPartsInfo.CheckComplete(entry->character_no) != 0 && EditPartsInfo.GetCompEvent(entry->character_no) == 0 && entry->hide_when_complete != 0) {
                        draw = false;
                    }
                }
            } else {
                villagers[i].map_parts_no = parts_no;
                draw = true;
            }

            if (entry->initial_motion == 0) {
                sceVu0FVECTOR people_position;

                if (ground->GetPeoplePos(entry->index, people_position) != 0) {
                    rotation[2] = 0.0f;
                    rotation[0] = 0.0f;
                    rotation[1] = people_position[3];
                    people_position[3] = 1.0f;
                    sceVu0CopyVector(position, people_position);
                    sceVu0CopyVector(entry->position, position);
                    sceVu0CopyVector(entry->rotation, rotation);
                } else {
                    draw = false;
                }
            }

            CNPCharacter *character = &villagers[i];
            character->ClearSeq();
            villagers[i].event_status = entry->initial_motion;
            villagers[i].villager_id = entry->index;
            character->SetPosition(position);
            character->SetRotation(rotation[0], rotation[1], rotation[2]);
            villagers[i].draw_enabled = draw;
            villagers[i].SetMotion(0, 0);
        }
    }

    srand(random_state);
}

static void GetNearVill(CCamera *camera, CCharacter *player, CNPCharacter *villagers, int *indices, float *distances) {
    sceVu0FVECTOR player_position;
    sceVu0FVECTOR camera_direction;
    sceVu0FVECTOR camera_offset;
    sceVu0FVECTOR camera_position;
    sceVu0FVECTOR villager_position;

    player->GetPosition(player_position);
    camera->GetDir(camera_direction);
    camera->GetPos(camera_position);
    int i;

    for (i = 0; i < 10; i++) {
        indices[i] = i;
        distances[i] = -1.0f;

        if (villagers[i].near_camera != 0) {
            villagers[i].near_camera = false;
            villagers[i].GetPosition(villager_position);
            sceVu0SubVector(camera_offset, villager_position, camera_position);

            if (sceVu0InnerProduct(camera_offset, camera_direction) > 0.0f) {
                distances[i] = DistVector(player_position, villager_position);
            }
        }
    }

    i = 0;

    while (i < 9) {
        int compare_index = i + 1;

        while (compare_index < 10) {
            if (distances[compare_index] >= 0.0f) {
                if (distances[i] > distances[compare_index] || distances[i] < 0.0f) {
                    int index = indices[i];
                    indices[i] = indices[compare_index];
                    indices[compare_index] = index;
                    float distance = distances[i];
                    distances[i] = distances[compare_index];
                    distances[compare_index] = distance;
                }
            }

            compare_index++;
        }

        i++;
    }
}

static void EdSetVillagerNextPos(CNPCharacter *villager, VILLAGER_INFO *info, CEditGround *ground);

/**
 * Advances and positions the selected villagers on the editable town map.
 *
 * @mangled EdMoveVillager__FP13VILLAGER_INFO
 * @address 0x186EF0
 * @size 0x5B4
 */
void EdMoveVillager(VILLAGER_INFO *villagers) {
    CEditGround *ground = EdExchangeInfo.ground;
    CCharacter  *player = EdExchangeInfo.player;
    CCamera     *camera = EdExchangeInfo.camera;
    int          i;

    for (i = 0; i < 10; i++) {
        int           offset;
        s32          *near_camera;
        sceVu0FVECTOR position;
        sceVu0FVECTOR rotation;
        offset = i * (int) sizeof(CNPCharacter);
        near_camera = (s32 *) ((char *) &EdVillager->near_camera + offset);
        *near_camera = false;
        VILLAGER_INFO *info = &villagers[i];

        if (info->placed == 0) {
            continue;
        }

        sceVu0CopyVector(position, info->position);
        sceVu0CopyVector(rotation, info->rotation);

        if (info->character_no >= 0 && (*(s32 *) ((char *) &EdVillager->draw_enabled + offset)) == 0) {
            continue;
        }

        if (info->character_no >= 0 && info->initial_motion == 0) {
            EDITPARTS_INFO *parts = EditPartsInfo.GetPartsInfo(info->character_no);

            if (parts->obtained == 0 || parts->elements[info->model_no].enabled == 0) {
                continue;
            }

            *near_camera = true;
            sceVu0CopyVector(position, info->position);
            sceVu0CopyVector(rotation, info->rotation);
        } else {
            *near_camera = true;
            sceVu0CopyVector(position, info->position);
            sceVu0CopyVector(rotation, info->rotation);
        }

        if (info->initial_motion == 0) {
            ((CCharacter *) ((char *) EdVillager + offset))->CCharacter::SetPosition(position);
            ((CCharacter *) ((char *) EdVillager + offset))->CCharacter::SetRotation(rotation[0], rotation[1], rotation[2]);
        } else {
            sceVu0FVECTOR player_position;
            sceVu0FVECTOR villager_position;
            player->GetPosition(player_position);
            CNPCharacter *npc = (CNPCharacter *) ((char *) EdVillager + offset);
            sceVu0CopyVector(villager_position, npc->pos);
            float distance = DistVector(player_position, villager_position);
            (*(s32 *) ((char *) &EdVillager->sequence_enabled + offset)) = 1;

            if (distance < 20.0f) {
                (*(s32 *) ((char *) &EdVillager->sequence_enabled + offset)) = 0;
                *(s32 *) ((char *) &EdVillager->motion_no + offset) = 0;
                *(s32 *) ((char *) &EdVillager->motion_flags + offset) = 0;
                *(float *) ((char *) &EdVillager->motion_speed + offset) = -1.0f;
            }

            EdSetVillagerNextPos(npc, info, ground);
        }
    }

    int   indices[10];
    float distances[10];
    GetNearVill(camera, player, EdVillager, indices, distances);

    for (int j = 0; j < 2; j++) {
        if (distances[j] < 0.0f) {
            break;
        }

        if (distances[j] < 150.0f) {
            EdVillager[indices[j]].near_camera = true;
        }
    }

    for (i = 0; i < 10; i++) {
        EdVillager[i].Step();
        EdVillager[i].ShadowStep();
        EdVillager[i].ClothStep(0);

        if (EdVillager[i].event_status != 0) {
            sceVu0FVECTOR position;
            sceVu0FVECTOR hit;
            sceVu0CopyVector(position, EdVillager[i].pos);
            float altitude = ground->GetAlt(position[0], position[1], position[2]);
            EdVillager[i].SetPosition(position[0], altitude, position[2]);
            EdVillager[i].FootSoundEnable(0);

            if (EdVillager[i].CheckDraw()) {
                EdVillager[i].FootSoundEnable(1);
                WorkBuffer__2->used = 0;
                CCPoly    *polys = (CCPoly *) WorkBuffer__2->Alloc(2000);
                int        count = ground->PickUpEditAreaPoly(polys, position[0], position[1], position[2]);
                CMapParts *parts = ground->GetParts(position[0], position[1], position[2]);

                if (parts != NULL) {
                    CBoxVu0 box;
                    box.max[0] = position[0] + 20.0f;
                    box.min[0] = position[0] - 20.0f;
                    box.max[2] = position[2] + 20.0f;
                    box.min[2] = position[2] - 20.0f;
                    box.max[1] = 1000.0f;
                    box.min[1] = -1000.0f;
                    CFrame *frame = parts->GetCollisionFrame();

                    if (frame != NULL) {
                        count += frame->PickUpNearPoly(polys + count, box);
                    }
                }

                if (count > 0) {
                    CCPoly poly;
                    position[1] += 30.0f;

                    if (GetFootPoly(position, 1000.0f, &poly, hit, polys, count, 0)) {
                        altitude = hit[1];
                        EdVillager[i].SetFootSoundID(poly.attr.foot_sound);
                    }
                }

                position[1] = altitude;
                EdVillager[i].SetPosition(position);
            }
        }
    }
}

void EdMoveVillagerSubMap(VILLAGER_INFO *villagers) {
    CCharacter *player = EdExchangeInfo.player;
    CCamera    *camera = EdExchangeInfo.camera;
    int         i;

    for (i = 0; i < 10; i++) {
        if (EdVillager[i].villager_id >= 0) {
            EdVillager[i].near_camera = false;
            EdVillager[i].SetPosition(villagers[i].position);
            EdVillager[i].SetRotation(villagers[i].rotation);

            if (EdVillager[i].frame != NULL) {
                EdVillager[i].near_camera = true;
            }
        }
    }

    int   indices[10];
    float distances[10];
    GetNearVill(camera, player, EdVillager, indices, distances);

    for (int i = 0; i < 2; i++) {
        if (distances[i] < 0.0f) {
            break;
        }

        if (distances[i] < 150.0f) {
            EdVillager[indices[i]].near_camera = true;
        }
    }

    for (i = 0; i < 10; i++) {
        if (EdVillager[i].villager_id < 0) {
            EdVillager[i].near_camera = true;
        }

        EdVillager[i].Step();
        EdVillager[i].ShadowStep();
        EdVillager[i].ClothStep(0);
    }
}

static void EdSetVillagerNextPos(CNPCharacter *villager, VILLAGER_INFO *info, CEditGround *ground) {
    if (villager->CheckSeq() != 0) {
        return;
    }

    sceVu0FVECTOR rotation;
    sceVu0FVECTOR position;
    sceVu0FVECTOR nearby_positions[10];
    sceVu0FVECTOR nearby_offsets[10];
    float         nearby_distances[10];
    sceVu0FVECTOR directions[4] = {
        {0.0f,    0.0f, 100.0f,  0.0f},
        {100.0f,  0.0f, 0.0f,    0.0f},
        {0.0f,    0.0f, -100.0f, 0.0f},
        {-100.0f, 0.0f, 0.0f,    0.0f},
    };
    int weights[5];
    int i;
    villager->GetPosition(position);
    villager->GetRotation(rotation);

    int nearby_count = 0;

    for (i = 0; i < 10; i++) {
        CNPCharacter *other = &EdVillager[i];

        if (villager != other) {
            int active = other->initialized != 0 && other->draw_enabled != 0;

            if (active && other->event_status != 0) {
                other->GetPosition(nearby_positions[nearby_count]);
                sceVu0SubVector(nearby_offsets[nearby_count], nearby_positions[nearby_count], position);
                float  distance = DistVector(nearby_offsets[nearby_count]);
                float *slot = &nearby_distances[nearby_count];
                *slot = distance;

                if (distance <= 210.0f) {
                    nearby_count++;
                }
            }
        }
    }

    int facing = 0;

    if (rotation[1] >= -QUARTER_PI_SHORT && rotation[1] < QUARTER_PI_SHORT) {
        facing = 0;
    }

    if (rotation[1] >= QUARTER_PI_SHORT && rotation[1] < 2.356194f) {
        facing = 1;
    }

    if (rotation[1] >= 2.356194f || rotation[1] < -2.356194f) {
        facing = 2;
    }

    if (rotation[1] >= -2.356194f && rotation[1] < -QUARTER_PI_SHORT) {
        facing = 3;
    }

    weights[0] = 40;
    weights[1] = 20;
    weights[2] = 5;
    weights[3] = 20;
    weights[4] = 15;
    int        total = weights[4];
    CMapParts *current_parts = ground->GetParts(position[0], position[1], position[2]);

    for (i = 0; i < 4; i++) {
        int blocked;
        int direction = i + facing;

        if (direction > 3) {
            direction -= 4;
        }

        float        *offset = directions[direction];
        sceVu0FVECTOR destination;
        sceVu0AddVector(destination, position, offset);
        int weight = weights[i];
        weights[i] = 0;
        blocked = 0;

        for (int j = 0; j < nearby_count; j++) {
            if ((float) (int) sceVu0InnerProduct(nearby_offsets[j], offset) > 0.0f) {
                blocked = 1;
                break;
            }
        }

        if (blocked == 0) {
            CBoxVu0 box;
            ground->GetPartsBox(&box, destination[0], destination[1], destination[2]);
            box.max[0] += 200.0f;
            box.max[2] += 200.0f;
            box.min[0] -= 200.0f;
            box.min[2] -= 200.0f;

            if (destination[0] <= box.max[0] && destination[0] >= box.min[0] && destination[2] <= box.max[2] && destination[2] >= box.min[2]) {
                CMapParts *next_parts = ground->GetParts(destination[0], destination[1], destination[2]);

                if (next_parts == NULL && ground->GetAreaCode(destination[0], destination[1], destination[2]) >= 0) {
                    weight /= 5;

                    if (weight <= 0) {
                        weight = 1;
                    }

                    total += weight;
                    weights[i] = weight;
                }

                if (next_parts != NULL && (next_parts->subtype == MAP_PARTS_SUBTYPE_ROAD || (current_parts != NULL && current_parts->subtype != MAP_PARTS_SUBTYPE_BRIDGE && next_parts->subtype == MAP_PARTS_SUBTYPE_BRIDGE))) {
                    total += weight;
                    weights[i] = weight;
                }
            }
        }
    }

    for (i = 0; i < 5; i++) {
        weights[i] = weights[i] * 100 / total;
    }

    for (i = 1; i < 5; i++) {
        weights[i] = weights[i] + weights[i - 1];
    }

    for (i = 0; i < 5; i++) {
        if (weights[i] == 0) {
            weights[i] = -1;
        }
    }

    int random = rand() % 100;

    for (i = 0; i < 4; i++) {
        if (random < weights[i]) {
            break;
        }
    }

    if (i < 4) {
        int direction = i + facing;

        if (direction > 3) {
            direction -= 4;
        }

        sceVu0FVECTOR destination;
        sceVu0AddVector(destination, position, directions[direction]);
        villager->SetSeq(destination, info->move_speed);
        return;
    }

    villager->SetWait(60);
}

int EdCheckTime(float time, float start, float end) {
    if (!(start <= end) && !(start <= time) && end <= time) {
        return 0;
    }

    if (start < end && (!(start <= time) || end <= time)) {
        return 0;
    }

    return 1;
}

int EdGetTime(float time) {
    int   period;
    float period_end;

    period = (int) (time / 3.0f);

    if (EdCheckTime(time, 11.5f, 2.5f) != 0) {
        period = ED_TIME_DAY;
    }

    if (EdCheckTime(time, 2.5f, 5.5f) != 0) {
        period = ED_TIME_EVENING;
    }

    if (EdCheckTime(time, 5.5f, 8.5f) != 0) {
        period = ED_TIME_NIGHT;
    }

    if (EdCheckTime(time, 8.5f, 11.5f) != 0) {
        period = ED_TIME_MORNING;
    }

    return period;
}

void EdLimitShadowLight(float light[][4], float scale) {
    light[1][0] = light[1][0] * scale;

    if (light[1][0] < 1.0f) {
        light[1][0] = 1.0f;
    }
}

void EdDrawSky(float clock, CFrameVu1 **sky, CFrame **sun, CFrameVu1 *clouds, CCamera *camera, int *follow_axes) {
    sceVu0FVECTOR ambient;
    sceVu0FVECTOR old_ambient;
    sceVu0FMATRIX identity;
    sceVu0FVECTOR position;
    sceVu0FVECTOR cloud_position;

    sceVu0UnitMatrix(identity);
    identity[3][1] = 2100.0f;

    CFrameVu1 *current_sky;
    CFrameVu1 *following_sky;
    int        transition;
    int        sky_index;
    int        next_sky;
    int        next_sun;
    CFrame    *current_sun;
    CFrame    *moon;
    int        sun_index;
    CFrame    *following_sun;
    int        previous_sky;

    sky_index = (int) (clock / 3.0f);
    next_sky = sky_index + 1;
    previous_sky = sky_index - 1;

    if (EditMapInfo->time_stop != 0) {
        clock = 0.0f;
        sky_index = 0;
        next_sky = 0;
        previous_sky = 0;
    }

    if (next_sky > 3) {
        next_sky = 0;
    }

    if (previous_sky < 0) {
        previous_sky = (int) previous_sky;
    }

    current_sky = sky[sky_index];
    following_sky = sky[next_sky];

    if (current_sky != NULL && following_sky == NULL) {
        following_sky = (CFrameVu1 *) following_sky;
    }

    camera->GetPos(position);

    if (follow_axes[0] == 0) {
        position[0] = 0.0f;
    }

    if (follow_axes[1] == 0) {
        position[1] = 0.0f;
    }

    if (follow_axes[2] == 0) {
        position[2] = 0.0f;
    }

    sceVu0CopyVector(cloud_position, position);
    cloud_position[1] -= 50.0f;

    mgRenderInfo.unlit = 1;

    if (clouds != NULL) {
        clouds->SetPosition(cloud_position);
    }

    if (clouds != NULL) {
        clouds->SetScale(1.0f, 1.0f, 1.0f);
    }

    MGDraw(clouds);

    if (clouds != NULL) {
        clouds->SetPosition(cloud_position);
    }

    if (clouds != NULL) {
        clouds->SetScale(1.0f, -1.0f, 1.0f);
    }

    MGDraw(clouds);
    mgRenderInfo.unlit = 0;

    MGGetAmbient(old_ambient);
    sceVu0CopyVector(ambient, old_ambient);
    transition = 0;
    sun_index = 2;

    if (clock >= 9.0f && clock < 12.0f) {
        sun_index = 3;
    }

    if (clock >= 0.0f && clock < 3.5f) {
        sun_index = 0;
    }

    if (clock >= 3.5f && clock < 5.5f) {
        sun_index = 1;
    }

    next_sun = sun_index + 1;

    if (next_sun > 3) {
        next_sun = 0;
    }

    current_sun = sun[sun_index];
    following_sun = sun[next_sun];
    moon = sun[2];

    if (EditMapInfo->time_stop != 0) {
        next_sun = 0;
        sun_index = 0;
        following_sun = sun[0];
        current_sun = following_sun;
        moon = following_sun;
    }

    if (current_sun != NULL && following_sun != NULL && moon == NULL) {
        moon = (CFrame *) moon;
    }

    if (clock > 11.5f && clock < 12.0f) {
        ambient[3] = 2.0f * (128.0f * (12.0f - clock));
        transition = 1;
    }

    if (clock > 3.0f && clock < 3.5f) {
        ambient[3] = 2.0f * (128.0f * (3.5f - clock));
        transition = 1;
    }

    float *ambient_alpha = &ambient[3];
    float  alpha = *ambient_alpha;
    float  adjusted_clock = clock;

    if (clock > 10.0f) {
        adjusted_clock = clock - 12.0f;
    }

    float sun_rotation = PI_SHORT * ((2.0f + adjusted_clock) / 6.0f) - HALF_PI;

    if (sun_rotation > PI_SHORT) {
        sun_rotation -= TWO_PI_SHORT;
    }

    float moon_rotation = 1.2f * (PI_SHORT * ((clock - 4.0f) / 6.0f) - HALF_PI);

    if (sun_index != 2) {
        float absolute_rotation = sun_rotation < 0.0f ? -sun_rotation : sun_rotation;

        if (absolute_rotation < 1.57f) {
            TexManager.ReloadTexture(Vif1Packet, 7);

            if (transition != 0) {
                *ambient_alpha = 128.0f - alpha;
                MGSetAmbient(ambient);

                if (next_sun != 2 && following_sun != NULL) {
                    following_sun->SetTransMatrix(identity);
                    following_sun->SetRotation(0.0f, 0.0f, sun_rotation);
                    following_sun->SetPosition(position);
                    MGDraw(following_sun);
                }
            }

            *ambient_alpha = alpha;

            if (sun_index != 2 && current_sun != NULL) {
                current_sun->SetTransMatrix(identity);
                MGSetAmbient(ambient);
                current_sun->SetRotation(0.0f, 0.0f, sun_rotation);
                current_sun->SetPosition(position);
                MGDraw(current_sun);
            }
        }
    }

    mgRenderInfo.unlit = 1;

    if (clouds != NULL) {
        clouds->SetPosition(cloud_position);
    }

    if (clouds != NULL) {
        clouds->SetScale(1.0f, -1.0f, 1.0f);
    }

    MGDraw(clouds);
    mgRenderInfo.unlit = 0;

    *ambient_alpha = 128.0f;
    transition = 0;

    if (clock > 2.0f && clock < 3.0f) {
        *ambient_alpha = 128.0f * (3.0f - clock);
        transition = 1;
    }

    if (clock > 5.0f && clock < 6.0f) {
        *ambient_alpha = 128.0f * (6.0f - clock);
        transition = 1;
    }

    if (clock > 8.0f && clock < 9.0f) {
        *ambient_alpha = 128.0f * (9.0f - clock);
        transition = 1;
    }

    if (clock > 11.0f) {
        *ambient_alpha = 128.0f * (12.0f - clock);
        transition = 1;
    }

    float sky_alpha = ambient[3];

    if (transition != 0) {
        *ambient_alpha = 128.0f - sky_alpha;
        MGSetAmbient(ambient);
        TexManager.ReloadTexture(Vif1Packet, next_sky + 3);

        if (following_sky != NULL) {
            following_sky->SetPosition(position);
        }

        MGDraw(following_sky);
    }

    *ambient_alpha = sky_alpha;
    MGSetAmbient(ambient);
    TexManager.ReloadTexture(Vif1Packet, sky_index + 3);

    if (current_sky != NULL) {
        current_sky->SetPosition(position);
    }

    MGDraw(current_sky);
    MGSetAmbient(old_ambient);

    if (sun_index == 2 && moon != NULL) {
        *ambient_alpha = 0.0f;

        if (clock > 5.5f && clock < 9.0) {
            TexManager.ReloadTexture(Vif1Packet, 7);
            *ambient_alpha = 128.0f;

            if (clock > 5.5f && clock < 6.0f) {
                *ambient_alpha = 128.0f - 2.0f * (128.0f * (6.0f - clock));
            }

            if (clock > 8.5f && clock < 9.0f) {
                *ambient_alpha = 2.0f * (128.0f * (9.0f - clock));
            }

            MGSetAmbient(ambient);
            moon->SetTransMatrix(identity);
            moon->SetRotation(0.0f, 0.0f, moon_rotation);
            moon->SetPosition(position);
            MGDraw(moon);
        }
    }

    MGSetAmbient(old_ambient);
}

void EdDrawLensFlare(float time, CFrame **sky) {
    int sky_index = (int) (time / 3.0f);

    if (!EdCheckTime(time, (0, 10.0f), 4.0f)) {
        return;
    }

    CFrame **sky_slot = &sky[sky_index];

    if (*sky_slot == NULL) {
        return;
    }

    sceVu0FVECTOR position;
    int           screen[4];
    sceVu0FVECTOR origin = {0.0f, 0.0f, 0.0f, 1.0f};
    CFrame       *light = NULL;

    if (sky_index == 0) {
        light = (*sky_slot)->SearchFrame("sun1");
    }

    if (sky_index == 1) {
        light = (*sky_slot)->SearchFrame("sun2");
    }

    if (sky_index == 3) {
        light = (*sky_slot)->SearchFrame("moon");
    }

    if (light != NULL) {
        light->GetWorldPosition(position, origin);
    }

    if (position[1] < -100.0f) {
        return;
    }

    float fraction = time - (int) time;
    float inverse = 1.0f - fraction;
    int   current = (int) time;
    int   next = current + 1;

    if (next >= 12) {
        next = 0;
    }

    static float color[12][3] = {
        {255.0f, 255.0f, 255.0f},
        {255.0f, 255.0f, 255.0f},
        {255.0f, 255.0f, 255.0f},
        {255.0f, 128.0f, 0.0f  },
        {255.0f, 128.0f, 0.0f  },
        {255.0f, 128.0f, 0.0f  },
        {255.0f, 255.0f, 255.0f},
        {255.0f, 255.0f, 255.0f},
        {255.0f, 255.0f, 255.0f},
        {255.0f, 255.0f, 255.0f},
        {255.0f, 255.0f, 255.0f},
        {255.0f, 255.0f, 255.0f}
    };
    unsigned char red = (unsigned char) (color[current][0] * inverse + color[next][0] * fraction);
    unsigned char green = (unsigned char) (color[current][1] * inverse + color[next][1] * fraction);
    unsigned char blue = (unsigned char) (color[current][2] * inverse + color[next][2] * fraction);

    if (!MGRotTransPers2D(screen, position, 0)) {
        return;
    }

    mgPickZBuff->enable = true;
    mgPickZBuff->x = screen[0];
    mgPickZBuff->y = screen[1];

    if (mgPickZBuff->z >= 0) {
        if (mgPickZBuff->z > screen[2] + 100) {
            return;
        }

        TexManager.ReloadTexture(GetVif1Packet(), 23);
        CTexture *texture = TexManager.GetTexture("lensfler", 23);

        if (texture != NULL) {
            LensFlare(texture, position, red, green, blue);
        }
    }
}

void EdSetLightParam(float clock, int fixed, EDIT_MAP_INFO *info, CFrameVu1 *sky) {
    int current = (int) clock;
    int next = (int) clock + 1;

    if (info->time_stop != 0) {
        next = current;
    }

    float current_weight = (float) next - clock;
    float next_weight = 1.0f - current_weight;

    if (next >= 12) {
        next = 0;
    }

    EDIT_FOG_INFO  fixed_fog;
    EDIT_FOG_INFO *current_fog;
    EDIT_FOG_INFO *next_fog;
    sceVu0FVECTOR  background;
    sceVu0FVECTOR  current_background;
    sceVu0FVECTOR  next_background;

    if (fixed != 0) {
        float zero = 0.0f;
        MGSetBGColor(zero, zero, zero, zero = 0.0f);
        fixed_fog = info->fog[current];
        fixed_fog.far_distance *= 10.0f;
        fixed_fog.near_distance *= 10.0f;
        current_fog = &fixed_fog;
        next_fog = current_fog;
    } else {
        current_fog = &info->fog[current];
        next_fog = &info->fog[next];
        sceVu0CopyVector(current_background, info->background_colour[current]);
        sceVu0CopyVector(next_background, info->background_colour[next]);
        sceVu0ScaleVector(current_background, current_background, current_weight);
        sceVu0ScaleVector(next_background, next_background, next_weight);
        sceVu0AddVector(background, current_background, next_background);
        MGSetBGColor(background[0], background[1], background[2], background[3]);
    }

    float fog_near = current_fog->near_distance * current_weight + next_fog->near_distance * next_weight;
    float fog_far = current_fog->far_distance * current_weight + next_fog->far_distance * next_weight;
    float fog_red = current_fog->red * current_weight + next_fog->red * next_weight;
    float fog_green = current_fog->green * current_weight + next_fog->green * next_weight;
    float fog_blue = current_fog->blue * current_weight + next_fog->blue * next_weight;
    (unsigned char) fog_red;
    (unsigned char) fog_green;
    (unsigned char) fog_blue;
    float fog_intensity = current_fog->intensity * current_weight + next_fog->intensity * next_weight;
    float fog_exponent = current_fog->exponent * current_weight + next_fog->exponent * next_weight;
    MGSetFogParm(fog_near, fog_far, (unsigned char) fog_red, (unsigned char) fog_green, (unsigned char) fog_blue, fog_intensity, fog_exponent);

    sceVu0FVECTOR ambient;
    sceVu0FVECTOR current_ambient;
    sceVu0FVECTOR next_ambient;
    sceVu0CopyVector(current_ambient, info->ambient[current]);
    sceVu0CopyVector(next_ambient, info->ambient[next]);
    sceVu0ScaleVector(current_ambient, current_ambient, current_weight);
    sceVu0ScaleVector(next_ambient, next_ambient, next_weight);
    sceVu0AddVector(ambient, current_ambient, next_ambient);
    MGSetAmbient(ambient);

    sceVu0FMATRIX direction;
    sceVu0FMATRIX current_direction;
    sceVu0FMATRIX next_direction;
    sceVu0FMATRIX colour;
    sceVu0FMATRIX current_colour;
    sceVu0FMATRIX next_colour;
    sceVu0CopyMatrix(current_colour, info->light_colour[current]);
    sceVu0CopyMatrix(next_colour, info->light_colour[next]);
    sceVu0TransposeMatrix(current_direction, info->light_direction[current]);
    sceVu0TransposeMatrix(next_direction, info->light_direction[next]);

    for (int i = 0; i < 4; i++) {
        sceVu0ScaleVector(current_colour[i], current_colour[i], current_weight);
        sceVu0ScaleVector(next_colour[i], next_colour[i], next_weight);
        sceVu0AddVector(colour[i], current_colour[i], next_colour[i]);
        sceVu0ScaleVector(current_direction[i], current_direction[i], current_weight);
        sceVu0ScaleVector(next_direction[i], next_direction[i], next_weight);
        sceVu0AddVector(direction[i], current_direction[i], next_direction[i]);
    }

    sceVu0Normalize(direction[0], direction[0]);
    sceVu0Normalize(direction[1], direction[1]);
    sceVu0Normalize(direction[2], direction[2]);
    sceVu0Normalize(direction[3], direction[3]);
    sceVu0TransposeMatrix(direction, direction);
    MGSetPLight(direction, colour);

    if (sky != NULL) {
        CVisualVu1 *visual = sky->GetVisual();

        if (visual != NULL) {
            MDT_HEADER *model = (MDT_HEADER *) visual->GetMDTDataAddress();

            if (model != NULL && model->colour_count != 0) {
                sceVu0FVECTOR *vertices = (sceVu0FVECTOR *) ((char *) model + model->colour_ofs);
                sceVu0FVECTOR  primary;
                sceVu0FVECTOR  secondary;
                sceVu0FVECTOR  second_current;
                sceVu0FVECTOR  second_next;
                MGGetBGColor(primary);
                sceVu0CopyVector(second_current, info->background_colour_2[current]);
                sceVu0CopyVector(second_next, info->background_colour_2[next]);
                sceVu0ScaleVector(second_current, second_current, current_weight);
                sceVu0ScaleVector(second_next, second_next, next_weight);
                sceVu0AddVector(secondary, second_current, second_next);
                primary[0] /= 128.0f;
                primary[1] /= 128.0f;
                primary[2] /= 128.0f;
                secondary[0] /= 128.0f;
                secondary[1] /= 128.0f;
                secondary[2] /= 128.0f;

                for (int i = 0; i < 32; i++) {
                    if (i % 4 < 2) {
                        vertices[i][0] = primary[0];
                        vertices[i][1] = primary[1];
                        vertices[i][2] = primary[2];
                        vertices[i][3] = 1.0f;
                    } else {
                        vertices[i][0] = secondary[0];
                        vertices[i][1] = secondary[1];
                        vertices[i][2] = secondary[2];
                        vertices[i][3] = 1.0f;
                    }
                }

                sky->attr.remake_pending = 1;
            }
        }
    }
}

int EdInitToEPInfo(INIT_PARTSINFO *init, EPARTS_INFO_HEADER *header) {
    header->header_size = 0x78;
    header->width = init->width;
    header->height = init->height;
    header->kind = init->kind;
    header->cell = NULL;

    for (int i = 0; i < 6; i++) {
        header->element_id[i] = init->element_id[i];
        header->element_name[i] = NULL;
    }

    u8 *write = (u8 *) header + header->header_size;
    header->cell = write;

    for (int i = 0; i < header->width * header->height; i++) {
        *write++ = init->cell[i][0];
    }

    for (int i = 0; i < 6; i++) {
        header->element_name[i] = (char *) write;
        strcpy((char *) write, init->element_name[i]);
        write += strlen(init->element_name[i]);
        *write++ = '\0';
        *write++ = '\0';
    }

    header->data_size = write - (u8 *) header;
    return header->data_size;
}

/** Colour and GS-scale alpha used to cover the screen during an editor fade. */
static float fade_col[4];

/** Direction and active-state flag for the editor fade. */
static int fade_in_out;

/** Target GS alpha for the active editor fade. */
static int fade_end;

/** GS alpha added on each frame of the active editor fade. */
static float fade_step;

ED_EVENT_INFO EdEventInfo;

OBJ_HANDLE ObjHandle[32];

void EdFadeInit() {
    fade_col[3] = 0.0f;
    fade_col[2] = 0.0f;
    fade_col[1] = 0.0f;
    fade_col[0] = 0.0f;
    fade_in_out = 0;
    fade_step = 0.0f;
    fade_end = 0;
}

/* A negative frame count means no step at all: the alpha is forced to the end this call starts
   from and stays there, which is how a colour is held over the screen rather than faded through. */
void EdFadeIn(int frames, float red, float green, float blue) {
    if (fade_in_out == 0 || frames < 0) {
        fade_col[3] = 128.0f;
    }

    fade_in_out = 1;
    fade_end = 0;

    if (frames < 0) {
        fade_step = 0.0f;
    } else {
        fade_step = 128.0f / (float) frames;
    }

    fade_col[0] = red;
    fade_col[1] = green;
    fade_col[2] = blue;
}

void EdFadeOut(int frames, float red, float green, float blue) {
    if (fade_in_out == 0 || frames < 0) {
        fade_col[3] = 0.0f;
    }

    fade_in_out = -1;
    fade_end = 0;

    if (frames < 0) {
        fade_step = 0.0f;
    } else {
        fade_step = 128.0f / (float) frames;
    }

    fade_col[0] = red;
    fade_col[1] = green;
    fade_col[2] = blue;
}

int EdFadeOutCheck() {
    return fade_end;
}

void EdGetFadeColor(float *colour) {
    colour[0] = fade_col[0];
    colour[1] = fade_col[1];
    colour[2] = fade_col[2];
    colour[3] = fade_col[3];
}

/* The rectangle is in the sixteenths of a pixel the coordinate registers count in, which is what
   the multiplications are; why the cover is 70 boxes rather than one the code does not say. */
void EdFadeInOut() {
    int      red;
    int      green;
    int      blue;
    int      alpha;
    int      x;
    int      y;
    CRect_i_ rect;

    if (fade_in_out == 0) {
        return;
    }

    red = (int) fade_col[0];
    green = (int) fade_col[1];
    blue = (int) fade_col[2];
    alpha = (int) fade_col[3];

    if (fade_in_out > 0) {
        fade_col[3] -= fade_step;

        if (fade_col[3] <= 0.0f) {
            fade_col[3] = 0.0f;
            fade_in_out = 0;
            fade_end = 1;
        }
    } else {
        fade_col[3] += fade_step;

        if (fade_col[3] >= 128.0f) {
            fade_col[3] = 128.0f;
            fade_end = 1;
        }
    }

    for (x = 0; x < 640; x += 64) {
        for (y = 0; y < SCREEN_HALF_HEIGHT; y += 32) {
            rect.x = x * 16;
            rect.y = y * 16;
            rect.width = 1024;
            rect.height = 512;
            MGFillBox(rect, red, green, blue, alpha);
        }
    }
}

static float GetMaxHeightCursor(float *position) {
    int height = 0;
    int count;

    for (count = 0; count < 8; count++) {
        EDIT_AREA_RECT_INFO *area = &EditMapInfo->edit_area_rects[count];

        if (area->maximum[0] == area->minimum[0] || area->maximum[2] == area->minimum[0]) {
            break;
        }
    }

    if (count == 0) {
        return 0.0f;
    }

    for (int i = 0; i < count; i++) {
        EDIT_AREA_RECT_INFO *area = &EditMapInfo->edit_area_rects[i];

        if (position[0] <= area->maximum[0] && position[2] <= area->maximum[2] && position[0] >= area->minimum[0] && position[2] >= area->minimum[2] && height < area->minimum[1]) {
            height = area->minimum[1];
        }
    }

    return height;
}

void LimitEditCursorPos(float *position, float *movement) {
    sceVu0FVECTOR previous;
    sceVu0FVECTOR clamped;
    sceVu0FVECTOR displacement;

    sceVu0CopyVector(previous, position);
    sceVu0AddVector(position, position, movement);

    int count;

    for (count = 0; count < 8; count++) {
        EDIT_AREA_RECT_INFO *area = &EditMapInfo->edit_area_rects[count];

        if (area->maximum[0] == area->minimum[0] || area->maximum[2] == area->minimum[0]) {
            break;
        }
    }

    if (count == 0) {
        position[1] = 0.0f;
        return;
    }

    for (int i = 0; i < count; i++) {
        EDIT_AREA_RECT_INFO *area = &EditMapInfo->edit_area_rects[i];

        if (position[0] <= area->maximum[0] && position[2] <= area->maximum[2] && position[0] >= area->minimum[0] && position[2] >= area->minimum[2]) {
            position[1] = GetMaxHeightCursor(position);
            return;
        }
    }

    float nearest_distance = -1.0f;
    int   nearest = -1;

    for (int i = 0; i < count; i++) {
        EDIT_AREA_RECT_INFO *area = &EditMapInfo->edit_area_rects[i];

        if (previous[0] <= area->maximum[0] && previous[2] <= area->maximum[2] && previous[0] >= area->minimum[0] && previous[2] >= area->minimum[2]) {
            VectorMin(clamped, position, area->maximum);
            VectorMax(clamped, clamped, area->minimum);
            sceVu0SubVector(displacement, clamped, position);
            displacement[1] = 0.0f;
            float distance = DistVector(displacement);

            if (nearest < 0) {
                nearest = i;
                nearest_distance = distance;
            } else if (distance < nearest_distance) {
                nearest = i;
                nearest_distance = distance;
            }
        }
    }

    if (nearest >= 0) {
        EDIT_AREA_RECT_INFO *area = &EditMapInfo->edit_area_rects[nearest];
        VectorMin(position, position, area->maximum);
        VectorMax(position, position, area->minimum);
        position[1] = GetMaxHeightCursor(position);
        return;
    }

    nearest = 0;
    EDIT_AREA_RECT_INFO *area = &EditMapInfo->edit_area_rects[0];
    float                max_x_distance;
    float                min_x_distance;
    float                min_z_distance;
    float                max_z_distance;
    float                x = position[0];
    max_x_distance = x - area->maximum[0];

    if (max_x_distance < 0.0f) {
        max_x_distance = -max_x_distance;
    } else {
        max_x_distance = max_x_distance;
    }

    min_x_distance = x - area->minimum[0];

    if (min_x_distance < 0.0f) {
        min_x_distance = -min_x_distance;
    } else {
        min_x_distance = min_x_distance;
    }

    float z = position[2];
    max_z_distance = z - area->maximum[2];

    if (max_z_distance < 0.0f) {
        max_z_distance = -max_z_distance;
    } else {
        max_z_distance = max_z_distance;
    }

    min_z_distance = z - area->minimum[2];

    if (min_z_distance < 0.0f) {
        min_z_distance = -min_z_distance;
    } else {
        min_z_distance = min_z_distance;
    }

    if (max_z_distance < min_z_distance) {
        max_z_distance = max_z_distance;
    } else {
        max_z_distance = min_z_distance;
    }

    if (max_x_distance < min_x_distance) {
        max_x_distance = max_x_distance;
    } else {
        max_x_distance = min_x_distance;
    }

    nearest_distance = max_x_distance + max_z_distance;

    for (int i = 1; i < count; i++) {
        area = &EditMapInfo->edit_area_rects[i];
        max_x_distance = x - area->maximum[0];

        if (max_x_distance < 0.0f) {
            max_x_distance = -max_x_distance;
        } else {
            max_x_distance = max_x_distance;
        }

        min_x_distance = x - area->minimum[0];

        if (min_x_distance < 0.0f) {
            min_x_distance = -min_x_distance;
        } else {
            min_x_distance = min_x_distance;
        }

        max_z_distance = z - area->maximum[2];

        if (max_z_distance < 0.0f) {
            max_z_distance = -max_z_distance;
        } else {
            max_z_distance = max_z_distance;
        }

        min_z_distance = z - area->minimum[2];

        if (min_z_distance < 0.0f) {
            min_z_distance = -min_z_distance;
        } else {
            min_z_distance = min_z_distance;
        }

        if (max_z_distance < min_z_distance) {
            max_z_distance = max_z_distance;
        } else {
            max_z_distance = min_z_distance;
        }

        if (max_x_distance < min_x_distance) {
            max_x_distance = max_x_distance;
        } else {
            max_x_distance = min_x_distance;
        }

        float distance = max_x_distance + max_z_distance;

        if (distance < nearest_distance) {
            nearest_distance = distance;
            nearest = i;
        }
    }

    area = &EditMapInfo->edit_area_rects[nearest];
    VectorMin(position, position, area->maximum);
    VectorMax(position, position, area->minimum);
    position[1] = GetMaxHeightCursor(position);
}

void EdSePlay(ED_SOUND_ID sound, int pan) {
    if (sound < 0 || sound >= ED_SOUND_COUNT) {
        return;
    }

    SndSePlay(sound, pan, 0);
}

/** Script-controlled sprites used by editor events. */
static ED_SPRITE Sprite[16];

static ED_SPRITE *GetSprite(int index) {
    if (index < 0 || index >= 16) {
        return NULL;
    }

    return &Sprite[index];
}

static void InitSprite(ED_SPRITE *sprite) {
    if (sprite == NULL) {
        return;
    }

    memset(sprite, 0, sizeof(ED_SPRITE));
    sprite->alpha = 128;
    sprite->blue = 128;
    sprite->green = 128;
    sprite->red = 128;
    sprite->height = -1;
    sprite->width = -1;
}

/** Object-animation slots used by editor event scripts. */
static OBJ_ANIME_SEQ obj_anime[16];

static OBJ_ANIME_SEQ *GetObjAnime(int index) {
    if (index < 0 || index >= 16) {
        return NULL;
    }

    return &obj_anime[index];
}

static void ClearObjAnime(int index) {
    if (index < 0) {
        for (int i = 0; i < 16; i++) {
            obj_anime[i].property = OBJ_ANIME_PROPERTY_NONE;
        }

        return;
    }

    OBJ_ANIME_SEQ *anime = GetObjAnime(index);

    if (anime != NULL) {
        anime->property = OBJ_ANIME_PROPERTY_NONE;
    }
}

/** Whether the current event uses the lightweight initialization path. */
int simple_event;

/** Whether an editor-event program has been installed. */
static int event_enable;

/** Whether a system event is currently being run. */
static int run_system_event;

/** Whether the current asynchronous load may continue without waiting. */
static int not_wait_load;

/** Whether event character motion is globally held. */
static int motion_stop_flag;

/** Menu mode requested by the active event. */
static int menu_mode;

/** State of the menu temporarily opened by an event. */
static int menu_mode_status;

/** Item-list string supplied to the editor's use-item menu. */
static char *p_use_item;

/** Map requested by the world-map event command. */
static int p_jump_map_no;

/** Whether execution of the current event has stopped. */
static int event_stop;

/** Whether advancement of the current editor event is paused. */
static int event_pause;

/** Whether the active script permits event skipping. */
static int skip_enable;

/** Whether event-local/world coordinate conversion is enabled. */
static int set_wl_matrix;

/** Character followed by the event camera. */
static CCharacter *follow_chara;

/** Character used as the event camera's reference target. */
static CCharacter *sync_camera_ref_chara;

/** Object used as the event camera's reference target. */
static OBJ_HANDLE *sync_camera_ref_obj;

/** Object used as the event camera's position target. */
static OBJ_HANDLE *sync_camera_pos_obj;

/** Shared destination buffer used by asynchronous event resource loads. */
static u_int *BaseBuffer;

/** Character-file slot selected by subsequent event load commands. */
static int actv_file;

/** Resource arena selected by subsequent event load commands. */
static int actv_buffer;

#ifdef PAL
void EdEventStopPlay() {
    event_stop = !event_stop;
}
#endif

void EdEventPause() {
    event_pause = !event_pause;
}

#ifdef PAL
int EdCheckEventPause() {
    return event_pause;
}
#endif

/** Directory prepended to relative event resource names. */
static char CurrentDir[0x40];

/** Origin of the event script's optional world coordinate system. */
static sceVu0FVECTOR world_pos;

/** Rotation of the event script's optional world coordinate system. */
static sceVu0FVECTOR world_rot;

/** Matrix that converts event-local coordinates to world coordinates. */
static sceVu0FMATRIX world_local;

/** Matrix that converts world coordinates to event-local coordinates. */
static sceVu0FMATRIX local_world;

/** Offset added to the event camera's synchronized reference target. */
static sceVu0FVECTOR sync_camera_ref_offset;

/** Integer scratch flags exposed to editor event scripts. */
static int work_flag[32];

/** Action records supplied to each editor-event sequencer. */
static ACT_SEQ asq_table[266];

/** Action sequencers available to event scripts. */
static CActionSeq ActSeq[10];

/** Texture-animation records supplied to event characters. */
static CTexAnimeData anime_data[320];

/**
 * Sprite-table entries shared by foreground and background event sprites.
 */
static SPRITE_TABLE sprite_table[32];

/** Foreground sprite command table for the active event. */
static CSpriteTable SpriteTable;

/** Background sprite command table for the active event. */
static CSpriteTable SpriteTableBack;

CCharacter SceneData;

CCamera DmmyCamera(4.0f);

/** Bytecode interpreter used by editor events. */
static CRunScript EdEventScript;

CRunScript SystemEventScript;

/** Archive slots populated by event character-file loads. */
static u_int *chr_file[16];

/** Saved point-light direction presets available to event scripts. */
static sceVu0FMATRIX save_l[2];

/** Saved point-light colour presets available to event scripts. */
static sceVu0FMATRIX save_c[2];

/** Saved ambient-light colour presets available to event scripts. */
static sceVu0FVECTOR save_a[2];

int (*ext_func__2[1500])(RS_STACKDATA *, int);

/** Describes one editor-event external function and its bytecode operation number. */
struct ED_EVENT_EXTERNAL_FUNCTION {
    int (*function)(RS_STACKDATA *, int); /**< Native function invoked by the bytecode operation. */
    int operation;                        /**< Bytecode operation number assigned to the function. */
};

void EdDrawOffAll();
void EdDrawOnAll();

static int SetWorkFlag(int index, int value) {
    if (index < 0 || index >= 32) {
        return 0;
    }

    work_flag[index] = value;
    return 1;
}

static int GetWorkFlag(int index) {
    if (index < 0 || index >= 32) {
        return 0;
    }

    return work_flag[index];
}

/**
 * Reads a script stack value as an integer, converting a tagged float when necessary.
 */
static int GetStackInt(RS_STACKDATA *stack) {
    if (stack->type == RS_FLOAT) {
        return (int) stack->f;
    }

    return stack->i;
}

/**
 * Reads a script stack value as a float, converting a tagged integer when necessary.
 */
static float GetStackFloat(RS_STACKDATA *stack) {
    if (stack->type == RS_INT) {
        return (float) stack->i;
    }

    return stack->f;
}

/**
 * Reads the string pointer stored in a script stack value.
 */
static char *GetStackString(RS_STACKDATA *stack) {
    return stack->s;
}

/**
 * Writes an integer through a script stack reference value.
 */
static void SetStack(RS_STACKDATA *stack, int value) {
    if (stack->type == RS_PTR) {
        stack->p->i = value;
    }
}

/**
 * Writes a float through a script stack reference value.
 */
static void SetStack(RS_STACKDATA *stack, float value) {
    if (stack->type == RS_PTR) {
        stack->p->f = value;
    }
}

static void PrintMemory() {
}

static OBJ_HANDLE *GetObjHandle(int index) {
    if (index < 0 || index >= 32) {
        return NULL;
    }

    return &ObjHandle[index];
}

static int SetObjHandle(int index, CMapParts *map_parts, char *frame_name) {
    OBJ_HANDLE *handle = GetObjHandle(index);

    if (handle == NULL) {
        return 0;
    }

    if (map_parts == NULL) {
        return 0;
    }

    if (frame_name == NULL) {
        return 0;
    }

    memset(handle, 0, sizeof(OBJ_HANDLE));
    int i = 0;
    handle->frames[0] = map_parts->frame[0];
    handle->frames[1] = map_parts->frame[1];
    handle->frames[2] = map_parts->frame[2];
    handle->frames[3] = map_parts->frame[3];
    handle->frames[4] = map_parts->GetCollisionFrame();
    handle->frames[5] = map_parts->shadow_frame;
    handle->frames[6] = map_parts->shade_frame;
    CFrame *extra_frame;

    if (map_parts->camera_frame == NULL) {
        extra_frame = NULL;
    } else {
        map_parts->camera_frame->SetPosition(map_parts->pos[0], map_parts->pos[1], map_parts->pos[2]);
        map_parts->camera_frame->SetRotation(map_parts->rotation.x, map_parts->rotation.y, map_parts->rotation.z);
        extra_frame = map_parts->camera_frame;
    }

    handle->frames[7] = extra_frame;
    handle->frames[8] = map_parts->ripple_frame;
    i += 9;

    for (; i < 12; i++) {
        handle->frames[i] = NULL;
    }

    if (frame_name[0] != '\0') {
        for (i = 0; i < 12; i++) {
            if (handle->frames[i] != NULL) {
                handle->frames[i] = handle->frames[i]->SearchFrame(frame_name);
            }
        }

        handle->map_parts = NULL;
    } else {
        handle->map_parts = map_parts;
    }

    return 1;
}

static int SetObjHandle(int index, CCharacter *character, char *frame_name) {
    OBJ_HANDLE *handle = GetObjHandle(index);

    if (handle == NULL) {
        return 0;
    }

    if (character == NULL) {
        return 0;
    }

    if (frame_name == NULL) {
        return 0;
    }

    memset(handle, 0, sizeof(OBJ_HANDLE));
    int i = 0;
    handle->frames[0] = character->frame;
    handle->frames[1] = character->shadow_frame;
    i += 2;

    for (; i < 12; i++) {
        handle->frames[i] = NULL;
    }

    if (frame_name[0] != '\0') {
        for (i = 0; i < 12; i++) {
            if (handle->frames[i] != NULL) {
                handle->frames[i] = handle->frames[i]->SearchFrame(frame_name);
            }
        }

        handle->map_parts = NULL;
    } else {
        handle->character = character;
    }

    return 1;
}

static int SetObjHandle(int index, CFrame *frame) {
    OBJ_HANDLE *handle = GetObjHandle(index);

    if (frame == NULL) {
        return 0;
    }

    memset(handle, 0, sizeof(OBJ_HANDLE));
    handle->frames[0] = frame;
    return 1;
}

static void obj_draw(OBJ_HANDLE *handle, int draw) {
    if (handle == NULL) {
        return;
    }

    int frame_draw = 2;
    int frame_flags = 4;

    if (draw != 0) {
        frame_draw = 1;
        frame_flags = 1;
    }

    if (handle->map_parts != NULL) {
        handle->map_parts->draw_on = draw;
        return;
    }

    for (int i = 0; i < 12; i++) {
        if (handle->frames[i] != NULL) {
            handle->frames[i]->attr.draw_on = frame_draw;
            handle->frames[i]->flags = frame_flags;
        }
    }
}

static void set_obj_pos(OBJ_HANDLE *handle, float *position) {
    if (handle->map_parts != NULL) {
        handle->map_parts->SetPosition(position);
        return;
    }

    if (handle->character != NULL) {
        handle->character->SetPosition(position);
        return;
    }

    for (int i = 0; i < 12; i++) {
        if (handle->frames[i] != NULL) {
            handle->frames[i]->SetPosition(position);
        }
    }
}

static void get_obj_pos(OBJ_HANDLE *handle, float *out_position) {
    if (handle->map_parts != NULL) {
        handle->map_parts->GetPosition(out_position);
        return;
    }

    if (handle->character != NULL) {
        handle->character->GetPosition(out_position);
        return;
    }

    for (int i = 0; i < 12; i++) {
        if (handle->frames[i] != NULL) {
            sceVu0CopyVector(out_position, handle->frames[i]->position);
            return;
        }
    }
}

static void get_obj_world_pos(OBJ_HANDLE *handle, float *out_position) {
    if (handle->map_parts != NULL) {
        handle->map_parts->GetPosition(out_position);
        return;
    }

    if (handle->character != NULL) {
        handle->character->GetPosition(out_position);
        return;
    }

    sceVu0FVECTOR origin = {0.0f, 0.0f, 0.0f, 1.0f};

    for (int i = 0; i < 12; i++) {
        if (handle->frames[i] != NULL) {
            handle->frames[i]->GetWorldPosition(out_position, origin);
            return;
        }
    }
}

static void set_obj_rot(OBJ_HANDLE *handle, float *rotation) {
    if (handle->map_parts != NULL) {
        handle->map_parts->SetRotation(rotation[0], rotation[1], rotation[2]);
        return;
    }

    if (handle->character != NULL) {
        handle->character->SetRotation(rotation[0], rotation[1], rotation[2]);
        return;
    }

    for (int i = 0; i < 12; i++) {
        if (handle->frames[i] != NULL) {
            handle->frames[i]->SetRotType(2);
            handle->frames[i]->SetRotation(rotation[0], rotation[1], rotation[2]);
        }
    }
}

static void get_obj_rot(OBJ_HANDLE *handle, float *out_rotation) {
    if (handle->map_parts != NULL) {
        handle->map_parts->GetRotation(out_rotation);
        return;
    }

    if (handle->character != NULL) {
        handle->character->GetRotation(out_rotation);
        return;
    }

    for (int i = 0; i < 12; i++) {
        if (handle->frames[i] != NULL) {
            handle->frames[i]->GetRotation(out_rotation);
            return;
        }
    }
}

static void set_obj_scale(OBJ_HANDLE *handle, float *scale) {
    if (handle->map_parts != NULL) {
        handle->map_parts->SetScale(scale);
        return;
    }

    if (handle->character != NULL) {
        handle->character->SetScale(scale);
        return;
    }

    for (int i = 0; i < 12; i++) {
        if (handle->frames[i] != NULL) {
            handle->frames[i]->SetScale(scale);
        }
    }
}

static void get_obj_scale(OBJ_HANDLE *handle, float *out_scale) {
    if (handle->map_parts != NULL) {
        handle->map_parts->GetScale(out_scale);
        return;
    }

    if (handle->character != NULL) {
        handle->character->GetScale(out_scale);
        return;
    }

    for (int i = 0; i < 12; i++) {
        CFrame *frame = handle->frames[i];

        if (frame != NULL) {
            out_scale[0] = frame->scale[0];
            out_scale[1] = frame->scale[1];
            out_scale[2] = frame->scale[2];
            return;
        }
    }
}

static int init_obj_anime(int anime_index, int handle_index, int type, int number, float *offset, float *range, float *speed) {
    OBJ_ANIME_SEQ *anime = GetObjAnime(anime_index);
    OBJ_HANDLE    *handle = GetObjHandle(handle_index);

    if (anime == NULL || handle == NULL) {
        return 0;
    }

    anime->property = type;
    anime->mode = number;
    anime->frame_name[0] = '\0';
    sceVu0CopyVector(anime->step, offset);
    sceVu0CopyVector(anime->from, range);
    sceVu0CopyVector(anime->to, speed);
    InitObjAnime(handle->frames, 12, anime);
}

static void sync_obj_obj(OBJ_HANDLE *source, OBJ_HANDLE *targets) {
    CFrame *reference = NULL;
    int     i;

    for (i = 0; i < 12; i++) {
        if (source->frames[i] != NULL) {
            reference = source->frames[i];
            break;
        }
    }

    if (reference != NULL) {
        for (i = 0; i < 12; i++) {
            if (targets->frames[i] != NULL) {
                targets->frames[i]->SetReference(reference);
            }
        }
    }
}

static void release_obj_obj(OBJ_HANDLE *handle) {
    for (int i = 0; i < 12; i++) {
        if (handle->frames[i] != NULL) {
            handle->frames[i]->DeleteReference();
        }
    }
}

static void set_attr_obj(OBJ_HANDLE *handle, CFrameAttr &attr, int children, int mask) {
    if (handle->character != NULL) {
        if (handle->character->frame != NULL) {
            handle->character->frame->SetAttr(attr, children, mask);
        }
    } else {
        for (int i = 0; i < 12; i++) {
            if (handle->frames[i] != NULL) {
                handle->frames[i]->SetAttr(attr, children, mask);
            }
        }
    }
}

static CActionSeq *GetActSeq(int index) {
    if (index < 0 || index >= 10) {
        return NULL;
    }

    return &ActSeq[index];
}

void turn_chara(CCharacter *character, float *position, float speed) {
    sceVu0FVECTOR current_position;
    sceVu0FVECTOR rotation;
    sceVu0FVECTOR direction;
    character->GetPosition(current_position);
    character->GetRotation(rotation);
    sceVu0SubVector(direction, position, current_position);
    float  angle = atan2f(direction[0], direction[2]);
    float *yaw = &rotation[1];
    *yaw = AngleInterpolate(*yaw, angle, speed, INTERPOLATE_STEP);
    character->SetRotation(rotation);
}

static CCharacter *GetScene(int index) {
    return &SceneData;
}

static CNPCharacter *GetNPC(int index) {
    if (index < 0 || index >= EdEventInfo.npc_count) {
        return NULL;
    }

    return &EdEventInfo.npcs[index];
}

static CCharacter *GetChara(int index) {
    if (index == -1) {
        return EdEventInfo.main_character;
    }

    if (index < 0 || index >= EdEventInfo.npc_count) {
        return NULL;
    }

    return &EdEventInfo.npcs[index];
}

static CFrame *GetItemFrame(int index) {
    if (index < 0 || index > 0) {
        return NULL;
    }

    return EdEventInfo.item_frame[index];
}

static void DeleteItemFrame(int index) {
    if (index < 0 || index > 0) {
        return;
    }

    CFrameVu1 **frames = EdEventInfo.item_frame;
    CFrameVu1 **frame = &frames[index];

    if (*frame != NULL) {
        (*frame)->DeleteReference();
        *frame = NULL;
    }
}

static VILLAGER_INFO *GetVillagerInfo(int index) {
    if (index < 0 || index >= 16) {
        return NULL;
    }

    return &EdEventInfo.villagers[index];
}

static void SetWorldCoord(float *position, float *rotation) {
    sceVu0CopyVector(world_pos, position);
    world_pos[3] = 1.0f;
    sceVu0CopyVector(world_rot, rotation);
    world_rot[3] = 0.0f;
    sceVu0UnitMatrix(world_local);
    sceVu0RotMatrixY(world_local, world_local, world_rot[1]);
    sceVu0CopyVector(world_local[3], world_pos);
    sceVu0InversMatrix(local_world, world_local);
    set_wl_matrix = 1;
}

static void GetWorldPos(float *out, float *position) {
    if (set_wl_matrix == 0) {
        sceVu0CopyVector(out, position);
        return;
    }

    position[3] = 1.0f;
    sceVu0ApplyMatrix(out, world_local, position);
}

static float GetWorldRotY(float rotation) {
    rotation += world_rot[1];

    if (rotation > PI_SHORT) {
        rotation -= TWO_PI_SHORT;
    }

    if (rotation < -PI_SHORT) {
        rotation += TWO_PI_SHORT;
    }

    return rotation;
}

static void GetWorldRot(float *out, float *rotation) {
    sceVu0CopyVector(out, rotation);
    out[1] = GetWorldRotY(out[1]);
}

static void GetLocalPos(float *out, float *position) {
    if (set_wl_matrix == 0) {
        sceVu0CopyVector(out, position);
        return;
    }

    position[3] = 1.0f;
    sceVu0ApplyMatrix(out, local_world, position);
}

static float GetLocalRotY(float rotation) {
    rotation -= world_rot[1];

    if (rotation > PI_SHORT) {
        rotation -= TWO_PI_SHORT;
    }

    if (rotation < -PI_SHORT) {
        rotation += TWO_PI_SHORT;
    }

    return rotation;
}

static void GetLocalRot(float *out, float *rotation) {
    sceVu0CopyVector(out, rotation);
    out[1] = GetLocalRotY(out[1]);
}

static void GetPosition(RS_STACKDATA *stack, float *position) {
    position[0] = GetStackFloat(stack++);
    position[1] = GetStackFloat(stack++);
    position[2] = GetStackFloat(stack);
    position[3] = 1.0f;
    GetWorldPos(position, position);
}

static void SetPosition(RS_STACKDATA *stack, float *position) {
    position[3] = 1.0f;
    GetLocalPos(position, position);
    SetStack(stack++, position[0]);
    SetStack(stack++, position[1]);
    SetStack(stack, position[2]);
}

static void GetRotation(RS_STACKDATA *stack, float *rotation) {
    rotation[0] = GetStackFloat(stack++);
    rotation[1] = GetStackFloat(stack++);
    rotation[2] = GetStackFloat(stack);
    rotation[3] = 0.0f;
    GetWorldRot(rotation, rotation);
}

static void SetRotation(RS_STACKDATA *stack, float *rotation) {
    rotation[3] = 0.0f;
    GetLocalRot(rotation, rotation);
    SetStack(stack++, rotation[0]);
    SetStack(stack++, rotation[1]);
    SetStack(stack, rotation[2]);
}

static void GetFileName(char *destination, char *name) {
    if (name[0] == '/') {
        strcpy(destination, name + 1);
        return;
    }

    strcpy(destination, CurrentDir);
    strcat(destination, name);
}

static int _TEST(RS_STACKDATA *stack, int argument_count) {
    return 1;
}

static int exch_ok_cancel(int buttons) {
    int confirm = buttons & ED_PAD_CONFIRM;
    int cancel = buttons & ED_PAD_CANCEL;
    buttons &= ~(ED_PAD_CONFIRM | ED_PAD_CANCEL);

    if (confirm) {
        buttons |= ED_PAD_CANCEL;
    }

    if (cancel) {
        buttons |= ED_PAD_CONFIRM;
    }

    return buttons;
}

static int _GET_PADON(RS_STACKDATA *stack, int argument_count) {
    if (argument_count <= 0) {
        return 0;
    }

    SetStack(stack, exch_ok_cancel(GamePad.GetPadOn()));
    return 1;
}

static int _GET_PADDOWN(RS_STACKDATA *stack, int argument_count) {
    if (argument_count <= 0) {
        return 0;
    }

    SetStack(stack, exch_ok_cancel(GamePad.GetPadDown()));
    return 1;
}

static int _GET_PADUP(RS_STACKDATA *stack, int argument_count) {
    if (argument_count <= 0) {
        return 0;
    }

    SetStack(stack, exch_ok_cancel(GamePad.GetPadUp()));
    return 1;
}

static int _GET_APAD(RS_STACKDATA *stack, int argument_count) {
    if (argument_count > 0) {
        SetStack(stack++, GamePad.GetLXf());
    }

    if (argument_count > 1) {
        SetStack(stack++, GamePad.GetLYf());
    }

    if (argument_count > 2) {
        SetStack(stack++, GamePad.GetRXf());
    }

    if (argument_count > 3) {
        SetStack(stack, GamePad.GetRYf());
    }

    return 1;
}

static int _GET_RANDOM(RS_STACKDATA *stack, int argument_count) {
    if (argument_count <= 0) {
        return 0;
    }

    SetStack(stack, rand());
    return 1;
}

static int _SET_RETURN_CODE(RS_STACKDATA *stack, int argument_count) {
    EdEventInfo.return_code = GetStackInt(stack);
    return 1;
}

static int _NEXT_EVENT(RS_STACKDATA *stack, int argument_count) {
    EdEventInfo.next_event = GetStackInt(stack);
    return 1;
}

static int _GOTO_INTERIOR(RS_STACKDATA *stack, int argument_count) {
    EdEventInfo.interior_entrance = GetStackInt(stack++);
    char *destination = GetStackString(stack++);
    strcpy(EdEventInfo.interior_name, destination);

    if (argument_count > 2) {
        EdEventInfo.interior_start_event = GetStackInt(stack);
    } else {
        EdEventInfo.interior_start_event = -1;
    }

    return EdEventInfo.return_code = ED_EVENT_RETURN_INTERIOR;
}

static int _SET_WORLD_COORD(RS_STACKDATA *stack, int argument_count) {
    if (argument_count == 6) {
        world_pos[0] = GetStackFloat(stack++);
        world_pos[1] = GetStackFloat(stack++);
        world_pos[2] = GetStackFloat(stack++);
        world_pos[3] = 1.0f;
        world_rot[0] = GetStackFloat(stack++);
        world_rot[1] = GetStackFloat(stack++);
        world_rot[2] = GetStackFloat(stack);
        world_rot[3] = 0.0f;
        SetWorldCoord(world_pos, world_rot);
    } else {
        world_pos[0] = world_pos[1] = world_pos[2] = 0.0f;
        world_rot[0] = world_rot[1] = world_rot[2] = 0.0f;
        sceVu0UnitMatrix(world_local);
        sceVu0UnitMatrix(local_world);
        set_wl_matrix = 0;
    }

    return 1;
}

static int _INITIALIZE(RS_STACKDATA *stack, int argument_count) {
    EdInitEventParam();
    return 1;
}

static int _EXIT_CODE(RS_STACKDATA *stack, int argument_count) {
    EdEventInfo.exit_code = GetStackInt(stack);
    return 1;
}

static int _DRAW_EXCLAMATION_MARK(RS_STACKDATA *stack, int argument_count) {
    EdEventInfo.draw_exclamation_mark = true;
    return 1;
}

static int _GOTO_USE_ITEM(RS_STACKDATA *stack, int argument_count) {
    int items[32];

    if (stack->type != RS_PTR) {
        return 0;
    }

    p_use_item = stack->s;
    stack++;
    int i;

    for (i = 0; i < argument_count - 1; i++) {
        items[i] = GetStackInt(stack++);
    }

    items[i] = -1;
    EdUseItemInit();
    EdSetUseItem(items);
    menu_mode = ED_MENU_USE_ITEM;
    return 1;
}

static int _NAME_REGISTRY(RS_STACKDATA *stack, int argument_count) {
    EdSetNameRegChara(GetStackInt(stack));
    menu_mode = ED_MENU_NAME_REGISTRY;
    return 1;
}

static int _WORLD_MAP(RS_STACKDATA *stack, int argument_count) {
    if (stack->type != RS_PTR) {
        return 0;
    }

    p_jump_map_no = stack->i;
    menu_mode = ED_MENU_WORLD_MAP;
    return 1;
}

static int _SKIP(RS_STACKDATA *stack, int argument_count) {
    skip_enable = GetStackInt(stack);
    return 1;
}

static int _GOTO_OUTSIDE(RS_STACKDATA *stack, int argument_count) {
    EdEventInfo.outside_map_no = GetStackInt(stack);
    EdEventInfo.return_code = ED_EVENT_RETURN_OUTSIDE;
    return 1;
}

static int _FINISH(RS_STACKDATA *stack, int argument_count) {
    EdEventAllClear();
    return 1;
}

static int _MAP_JUMP(RS_STACKDATA *stack, int argument_count) {
    int map = GetStackInt(stack++) - 1;
    int event = -1;

    if (argument_count >= 2) {
        event = GetStackInt(stack);
    }

    MapJump(map, event);
    EdEventInfo.return_code = ED_EVENT_RETURN_MAP_JUMP;
    return 1;
}

static int _GET_OLD_MAPNO(RS_STACKDATA *stack, int argument_count) {
    SetStack(stack, OldMapNo + 1);
    return 1;
}

static int _SET_DUNGEON_FLOOR(RS_STACKDATA *stack, int argument_count) {
    BtSetMapJumpFloor(GetStackInt(stack));
    return 1;
}

static int _GET_DUNGEON_FLOOR(RS_STACKDATA *stack, int argument_count) {
    SetStack(stack, BtMapJumpFloor);
    return 1;
}

static int _FADEOUT_TO_EVENT(RS_STACKDATA *stack, int argument_count) {
    EdEventInfo.fadeout_event_no = GetStackInt(stack);
    return 1;
}

static int _MAP_JUMP_BGM_STOP(RS_STACKDATA *stack, int argument_count) {
    EdEventInfo.map_jump_bgm_stop = GetStackInt(stack);
    return 1;
}

static int _MAP_JUMP_BGM_PLAY(RS_STACKDATA *stack, int argument_count) {
    EdEventInfo.map_jump_bgm_play = GetStackInt(stack);
    return 1;
}

static int _GOTO_FP_CHANGE(RS_STACKDATA *stack, int argument_count) {
    menu_mode = ED_MENU_FISHING_EXCHANGE;
    return 1;
}

static int _GOTO_FISH_RANKING(RS_STACKDATA *stack, int argument_count) {
    menu_mode = ED_MENU_FISH_RANKING;
    return 1;
}

static int _GOTO_CHANGE_ESA(RS_STACKDATA *stack, int argument_count) {
    if (stack->type != RS_PTR) {
        return 0;
    }

    p_use_item = stack->s;
    int items[32] = {ITEM_EVY, ITEM_MIMI, ITEM_PRICKLY, ITEM_THROBBING_CHERRY, ITEM_GOOEY_PEACH, ITEM_BOMB_NUTS, ITEM_POISONOUS_APPLE, ITEM_MELLOW_BANANA, ITEM_CARROT, ITEM_POTATO_CAKE, ITEM_MINON, ITEM_BATTAN, ITEM_PETITE_FISH, -1};
    EdUseItemInit();
    EdSetUseItem(items);
    menu_mode = ED_MENU_CHANGE_ESA;
    return 1;
}

static int _SET_CURRENT_DIR(RS_STACKDATA *stack, int argument_count) {
    char *directory = GetStackString(stack);

    if (directory == NULL) {
        return 0;
    }

    strcpy(CurrentDir, directory);
    return 1;
}

static u_int *get_pack_file() {
    return chr_file[actv_file];
}

static CDataAlloc2<1> *get_buffer() {
    if (actv_buffer == 1) {
        return &EdEventExBuffer;
    }

    return &EdEventBuffer;
}

static int _ACTIVE_FILE_BUFFER(RS_STACKDATA *stack, int argument_count) {
    actv_file = GetStackInt(stack++);
    actv_buffer = GetStackInt(stack);

    if (actv_file < 0 || actv_file >= 16) {
        actv_file = 0;
    }

    return 1;
}

static int _LOAD_CHR_FILE(RS_STACKDATA *stack, int argument_count) {
    char path[128];
    int  size;
    StartReadBG();
    u_long128 *buffer = (u_long128 *) read_buffer;
    int        i;

    for (i = 0; i < 16; i++) {
        chr_file[i] = NULL;
    }

    for (i = 0; i < argument_count; i++) {
        char *name = GetStackString(stack++);

        if (name != NULL) {
            GetFileName(path, name);
            printf("%s\n", path);
            chr_file[i] = (u_int *) buffer;
            LoadFileBG(path, (u_long128 *) chr_file[i], &size);
            buffer += (((size >> 6) + 1) << 6) >> 4;
        }
    }

    return not_wait_load = 1;
}

static int _LOAD_SYNC(RS_STACKDATA *stack, int argument_count) {
    if (argument_count <= 0) {
        return 0;
    }

    SetStack(stack, ReadBGSync());
    not_wait_load = 0;
    return 1;
}

static int _LOAD_CHARA(RS_STACKDATA *stack, int argument_count) {
    RS_STACKDATA *name_stack = stack + 1;
    int           index = GetStackInt(stack);

    if (index == -1) {
        CCharacter *character = EdEventInfo.main_character;
        char       *name = GetStackString(name_stack);

        if (name == NULL) {
            return 0;
        }

        u_int *pack = get_pack_file();

        if (pack == NULL) {
            return 0;
        }

        CDataAlloc2<1> *first = get_buffer();
        CDataAlloc2<1> *second = get_buffer();
        character->LoadPackData2(pack, name, first, EdEventInfo.player_texture_block, second, 0);
        character->SetMotionCamera(EdEventInfo.camera);
        PrintMemory();
        return 1;
    }

    CNPCharacter *npc = GetNPC(index);

    if (npc == NULL) {
        return 0;
    }

    char *name = GetStackString(name_stack);

    if (name == NULL) {
        return 0;
    }

    u_int *pack = get_pack_file();

    if (pack == NULL) {
        return 0;
    }

    CDataAlloc2<1> *first = get_buffer();
    CDataAlloc2<1> *second = get_buffer();
    npc->LoadPackData2(pack, name, first, EdEventInfo.npc_texture_block + index, second, 0);
    npc->texture_block = EdEventInfo.npc_texture_block + index;
    npc->initialized = true;
    npc->draw_enabled = true;
    npc->near_camera = true;
    npc->ambient_offset[3] = 128.0f;
    CFrameAttr attr;
    attr.clip_enable = false;
    attr.fog_enable = true;
    EdEventInfo.npc_draw[index] = 1;
    EdEventInfo.npc_shadow_draw[index] = 1;
    npc->SetMotionCamera(EdEventInfo.camera);

    if (npc->frame != NULL) {
        npc->frame->SetAttr(attr, 1, 4);
    }

    PrintMemory();
    return 1;
}

static int _LOAD_CHARA_TEXTURE(RS_STACKDATA *stack, int argument_count) {
    LOADTEXTURE_INFO2 textures[16];
    int               size;
    int               index = GetStackInt(stack++);
    CNPCharacter     *npc = GetNPC(index);

    if (npc == NULL) {
        return 0;
    }

    if (npc->images[0] != NULL) {
        return 1;
    }

    int    block = EdEventInfo.npc_texture_block + index;
    int    i;
    u_int *pack = get_pack_file();

    if (pack == NULL) {
        return 0;
    }

    for (i = 0; i < argument_count - 1; i++) {
        u_int *file = GetPackFile(pack, GetStackString(stack++), &size);

        if (file != NULL) {
            void *copy = get_buffer()->Alloc((size >> 4) + 1);
            memcpy(copy, file, size);
            textures[i].block_no = block;
            textures[i].mipmap = false;
            textures[i].name = (char *) copy;
        }
    }

    textures[i].name = NULL;
    TexManager.DeleteTextureBlock(block);
    TexManager.CleanUpBuffer();
    TexManager.LoadTextureBlockEX(block, textures);
    npc->texture_block = block;
    return 1;
}

static int _LOAD_SPRITE_TEXTURE(RS_STACKDATA *stack, int argument_count) {
    LOADTEXTURE_INFO2 textures[16];
    int               size;
    int               i;
    u_int            *pack = get_pack_file();

    if (pack == NULL) {
        return 0;
    }

    for (i = 0; i < argument_count; i++) {
        u_int *file = GetPackFile(pack, GetStackString(stack++), &size);

        if (file != NULL) {
            void *copy = get_buffer()->Alloc((size >> 4) + 1);
            memcpy(copy, file, size);
            textures[i].block_no = 45;
            textures[i].mipmap = false;
            textures[i].name = (char *) copy;
        }
    }

    textures[i].name = NULL;
    TexManager.DeleteTextureBlock(45);
    TexManager.CleanUpBuffer();
    TexManager.LoadTextureBlockEX(45, textures);
    return 1;
}

static int _LOAD_BG_SPRITE_TEXTURE(RS_STACKDATA *stack, int argument_count) {
    LOADTEXTURE_INFO2 textures[16];
    int               size;
    int               i;
    u_int            *pack = get_pack_file();

    if (pack == NULL) {
        return 0;
    }

    for (i = 0; i < argument_count; i++) {
        u_int *file = GetPackFile(pack, GetStackString(stack++), &size);

        if (file != NULL) {
            void *copy = get_buffer()->Alloc((size >> 4) + 1);
            memcpy(copy, file, size);
            textures[i].block_no = 46;
            textures[i].mipmap = false;
            textures[i].name = (char *) copy;
        }
    }

    textures[i].name = NULL;
    TexManager.DeleteTextureBlock(46);
    TexManager.CleanUpBuffer();
    TexManager.LoadTextureBlockEX(46, textures);
    return 1;
}

static int _LOAD_TEXTURE(RS_STACKDATA *stack, int argument_count) {
    LOADTEXTURE_INFO2 textures[16];
    int               size;
    int               block = GetStackInt(stack++) + 45;

    if (block > 49 || block < 45) {
        return 0;
    }

    int    i;
    u_int *pack = get_pack_file();

    if (pack == NULL) {
        return 0;
    }

    for (i = 0; i < argument_count - 1; i++) {
        u_int *file = GetPackFile(pack, GetStackString(stack++), &size);

        if (file != NULL) {
            void *copy = get_buffer()->Alloc((size >> 4) + 1);
            memcpy(copy, file, size);
            textures[i].block_no = block;
            textures[i].mipmap = false;
            textures[i].name = (char *) copy;
        }
    }

    textures[i].name = NULL;
    TexManager.DeleteTextureBlock(block);
    TexManager.CleanUpBuffer();
    TexManager.LoadTextureBlockEX(block, textures);
    return 1;
}

/**
 * Activates and places the villager selected by an editor-event script.
 *
 * @mangled _LOAD_IN_VILLAGER__FP12RS_STACKDATAi
 * @address 0x18CD60
 * @size 0x1FC
 */
static int _LOAD_IN_VILLAGER(RS_STACKDATA *stack, int argument_count) {
    int           index = GetStackInt(stack++);
    CNPCharacter *npc = GetNPC(index);

    if (npc == NULL) {
        return 0;
    }

    VILLAGER_INFO *villager = GetVillagerInfo(GetStackInt(stack++));
    char          *name = GetStackString(stack++);

    if (name == NULL) {
        return 0;
    }

    npc->texture_block = index + 54;
    EdLoadVillager(name, npc, &EdVillagerBuffer);

    if (villager == NULL) {
        npc->map_parts_no = -1;
        npc->villager_id = -1;
    } else {
        memcpy(&EdVillagerInfo[index], villager, sizeof(VILLAGER_INFO));
        npc->map_parts_no = villager->character_no;
        npc->villager_id = villager->index;
    }

    npc->near_camera = true;
    npc->initialized = true;
    npc->draw_enabled = true;
    npc->event_status = 1;
    npc->ambient_offset[3] = 128.0f;
    sceVu0FVECTOR position = {0.0f, 0.0f, 0.0f, 0.0f};
    sceVu0FVECTOR rotation = {0.0f, 0.0f, 0.0f, 0.0f};

    if (argument_count > 3) {
        GetPosition(stack, position);
        stack += 3;
        rotation[1] = GetStackFloat(stack++);
        rotation[2] = 0.0f;
        rotation[0] = 0.0f;
        GetWorldRot(rotation, rotation);
    }

    npc->SetPosition(position);
    npc->SetRotation(rotation);
    npc->SetMotion(0, 0);
    return 1;
}

/**
 * Removes the villager selected by an editor-event script.
 *
 * @mangled _LOAD_OUT_VILLAGER__FP12RS_STACKDATAi
 * @address 0x18CF60
 * @size 0x1C0
 */
static int _LOAD_OUT_VILLAGER(RS_STACKDATA *stack, int argument_count) {
    int           index = GetStackInt(stack++);
    CNPCharacter *npc = GetNPC(index);

    if (npc == NULL) {
        return 0;
    }

    VILLAGER_INFO *villager = GetVillagerInfo(GetStackInt(stack++));
    char          *name = GetStackString(stack++);

    if (name == NULL) {
        return 0;
    }

    npc->texture_block = index + 54;
    EdLoadVillager(name, npc, &EdVillagerBuffer);
    npc->map_parts_no = -1;
    npc->villager_id = -1;

    if (villager != NULL) {
        villager->index = -1;
    }

    npc->near_camera = true;
    npc->initialized = true;
    npc->draw_enabled = true;
    npc->event_status = 0;
    npc->ambient_offset[3] = 128.0f;
    sceVu0FVECTOR position = {0.0f, 0.0f, 0.0f, 0.0f};
    sceVu0FVECTOR rotation = {0.0f, 0.0f, 0.0f, 0.0f};

    if (argument_count > 3) {
        GetPosition(stack, position);
        stack += 3;
        rotation[1] = GetStackFloat(stack++);
        rotation[2] = 0.0f;
        rotation[0] = 0.0f;
        GetWorldRot(rotation, rotation);
    }

    npc->SetPosition(position);
    npc->SetRotation(rotation);
    npc->SetMotion(0, 0);
    return 1;
}

static int _LOAD_VILLAGER(RS_STACKDATA *stack, int argument_count) {
    EdVillagerBuffer.used = 0;
    EdSelectVillager(EdVillagerInfo, EdEventInfo.current_time, EditMapInfo);
    EdInitVilager(EdVillagerInfo, EdEventInfo.edit_ground, NULL);
    EdInitVilagerPosition(EdVillager, EdVillagerInfo, EdEventInfo.edit_ground, NULL);
    return 1;
}

static int _CHECK_IN_VILLAGER(RS_STACKDATA *stack, int argument_count) {
    int index = GetStackInt(stack++);
    int in = EdCheckVillagerIn(index, &EdEventInfo.villagers[index]);
    SetStack(stack, in);
    return 1;
}

static int _APPEAR_VILLAGER_ON(RS_STACKDATA *stack, int argument_count) {
    EdVillagerAppearOn(GetStackInt(stack), 1);
    return 1;
}

static int _APPEAR_VILLAGER_OFF(RS_STACKDATA *stack, int argument_count) {
    EdVillagerAppearOff(GetStackInt(stack), 1);
    return 1;
}

static int _APPEAR_VILLAGER_OUT(RS_STACKDATA *stack, int argument_count) {
    EdVillagerAppearOut(GetStackInt(stack), 1);
    return 1;
}

static int _APPEAR_VILLAGER_IN(RS_STACKDATA *stack, int argument_count) {
    EdVillagerAppearIn(GetStackInt(stack), 1);
    return 1;
}

static int _APPEAR_VILLAGER_MOVE(RS_STACKDATA *stack, int argument_count) {
    RS_STACKDATA *next = (0, stack + 1);
    int           index = GetStackInt(stack);
    EdVillagerAppearMove(index, GetStackInt(next), 1);
    return 1;
}

static int _DELETE_CHARA(RS_STACKDATA *stack, int argument_count) {
    int           index = GetStackInt(stack);
    CNPCharacter *npc = GetNPC(index);

    if (npc == NULL) {
        return 0;
    }

    TexManager.DeleteTextureBlock(npc->texture_block);
    npc->Initialize();
    EdEventInfo.npc_draw[index] = 0;
    EdEventInfo.npc_shadow_draw[index] = 0;
    return 1;
}

static int _INIT_CHARA(RS_STACKDATA *stack, int argument_count) {
    for (int i = 0; i < argument_count; i++) {
        CNPCharacter *npc = GetNPC(GetStackInt(stack++));

        if (npc != NULL) {
            npc->Initialize();
        }
    }

    return 1;
}

static int _CLEAR_VILLAGER_BUFF(RS_STACKDATA *stack, int argument_count) {
    CDataAlloc2<1> *arena = (CDataAlloc2<1> *) BaseBuffer;
    int             used = arena->used;
    u_char         *base = arena->base + used * 16;
    int             limit = arena->limit - used;
    EdVillagerBuffer.base = base;
    EdVillagerBuffer.limit = limit;
    EdVillagerBuffer.used = 0;
    return 1;
}

static int _CLEAR_EVENT_BUFF(RS_STACKDATA *stack, int argument_count) {
    int     used = EdVillagerBuffer.used;
    u_char *base = EdVillagerBuffer.base + used * 16;
    int     limit = EdVillagerBuffer.limit - used;
    EdEventBuffer.base = base;
    EdEventBuffer.limit = limit;
    EdEventBuffer.used = 0;
    return 1;
}

static int _CLEAR_EVENT_EXBUFF(RS_STACKDATA *stack, int argument_count) {
    int     used = EdEventBuffer.used;
    u_char *base = EdEventBuffer.base + used * 16;
    int     limit = EdEventBuffer.limit - used;
    EdEventExBuffer.base = base;
    EdEventExBuffer.limit = limit;
    EdEventExBuffer.used = 0;
    return 1;
}

static int _LOAD_SCENE(RS_STACKDATA *stack, int argument_count) {
    CCharacter *scene = GetScene(GetStackInt(stack++));

    if (scene == NULL) {
        return 0;
    }

    char *name = GetStackString(stack++);

    if (name == NULL) {
        return 0;
    }

    u_int *pack = get_pack_file();

    if (pack == NULL) {
        return 0;
    }

    CDataAlloc2<1> *first = get_buffer();
    CDataAlloc2<1> *second = get_buffer();
    scene->LoadPackData(pack, name, first, second);
    scene->motion_type.state.camera = &DmmyCamera;

    if (argument_count >= 3) {
        scene->motion_speed = GetStackFloat(stack);
    }

    return 1;
}

static int _SYNC_SCENE_CHARA(RS_STACKDATA *stack, int argument_count) {
    CCharacter *scene = GetScene(GetStackInt(stack++));

    if (scene == NULL) {
        return 0;
    }

    char       *name = GetStackString(stack++);
    int         index = GetStackInt(stack);
    CCharacter *character = EdEventInfo.main_character;

    if (index >= 0) {
        character = GetNPC(index);
    }

    if (character == NULL) {
        return 0;
    }

    if (scene->frame == NULL) {
        return 0;
    }

    CFrame *reference = scene->frame->SearchFrame(name);

    if (reference == NULL) {
        return 0;
    }

    if (character->frame == NULL) {
        return 0;
    }

    character->frame->SetReference(reference);

    if (character->shadow_frame != NULL) {
        character->shadow_frame->SetReference(reference);
    }

    return 1;
}

static int _SYNC_SCENE_CAMERA(RS_STACKDATA *stack, int argument_count) {
    CCharacter *scene = GetScene(GetStackInt(stack));

    if (scene == NULL) {
        return 0;
    }

    if (EdEventInfo.camera != NULL) {
        EdEventInfo.camera->FollowOff();
        scene->motion_type.state.camera = EdEventInfo.camera;
    }

    return 1;
}

static int _RELEASE_SCENE_CHARA(RS_STACKDATA *stack, int argument_count) {
    int         index = GetStackInt(stack);
    CCharacter *character = EdEventInfo.main_character;

    if (index >= 0) {
        character = GetNPC(index);
    }

    if (character == NULL) {
        return 0;
    }

    if (character->frame != NULL) {
        character->frame->DeleteReference();
    }

    if (character->shadow_frame != NULL) {
        character->shadow_frame->DeleteReference();
    }

    return 1;
}

static int _RELEASE_SCENE_CAMERA(RS_STACKDATA *stack, int argument_count) {
    CCharacter *scene = GetScene(GetStackInt(stack));

    if (scene == NULL) {
        return 0;
    }

    scene->motion_type.state.camera = &DmmyCamera;
    return 1;
}

static int _SET_SCENE_POS(RS_STACKDATA *stack, int argument_count) {
    CCharacter *scene = GetScene(GetStackInt(stack++));

    if (scene == NULL) {
        return 0;
    }

    sceVu0FVECTOR position;
    GetPosition(stack, position);
    scene->SetPosition(position);
    return 1;
}

static int _SET_SCENE_ROT(RS_STACKDATA *stack, int argument_count) {
    CCharacter *scene = GetScene(GetStackInt(stack++));

    if (scene == NULL) {
        return 0;
    }

    sceVu0FVECTOR rotation;
    GetRotation(stack, rotation);
    scene->SetRotation(rotation);
    return 1;
}

static int _LOAD_ITEM_FILE(RS_STACKDATA *stack, int argument_count) {
    char model_path[128];
    char texture_path[128];
    int  size;
    EdGetItemFile(GetStackInt(stack), model_path, texture_path);

    if (model_path[0] == '\0' || texture_path[0] == '\0') {
        return 0;
    }

    StartReadBG();
    u_long128 *buffer = (u_long128 *) read_buffer;
    LoadFileBG(texture_path, buffer, &size);
    buffer += (((size >> 6) + 1) << 6) >> 4;
    LoadFileBG(model_path, buffer, &size);
    return 1;
}

static int _LOAD_ITEM(RS_STACKDATA *stack, int argument_count) {
    int index = GetStackInt(stack);

    if (index < 0 || index > 0) {
        return 0;
    }

    BG_READ_INFO *read = GetReadBGFile(0);

    if (read == NULL) {
        return 0;
    }

    int        size = read->size;
    u_long128 *source = read->buffer;
    void      *copy = get_buffer()->Alloc((size >> 4) + 1);
    memcpy(copy, source, size);
    LOADTEXTURE_INFO2 textures[2];
    textures[0].block_no = 40;
    textures[0].mipmap = false;
    textures[0].name = (char *) copy;
    textures[1].name = NULL;
    TexManager.DeleteTextureBlock(40);
    TexManager.LoadTextureBlockEX(40, textures);
    read = GetReadBGFile(1);

    if (read == NULL) {
        return 0;
    }

    EdEventInfo.item_frame[index] = LoadMDSFile((u_int *) read->buffer, get_buffer(), 0, NULL, NULL);
    return EdEventInfo.item_frame[index] != NULL ? 1 : 0;
}

static int _SYNC_CHARA_ITEM(RS_STACKDATA *stack, int argument_count) {
    CCharacter *character = GetChara(GetStackInt(stack++));
    char       *name = GetStackString(stack++);
    CFrame     *reference = NULL;

    if (name != NULL && name[0] != '\0') {
        reference = character->frame->SearchFrame(name);
    }

    if (reference == NULL) {
        reference = character->frame;
    }

    CFrame *item = GetItemFrame(GetStackInt(stack));

    if (item == NULL) {
        return 0;
    }

    item->SetReference(reference);
    return 1;
}

static int _RELEASE_CHARA_ITEM(RS_STACKDATA *stack, int argument_count) {
    CFrame *frame = GetItemFrame(GetStackInt(stack));

    if (frame != NULL) {
        frame->DeleteReference();
    }

    return 1;
}

static int _DELETE_ITEM(RS_STACKDATA *stack, int argument_count) {
    DeleteItemFrame(GetStackInt(stack));
    TexManager.DeleteTextureBlock(40);
    return 1;
}

static int _SYNC_CHARA_CHARA(RS_STACKDATA *stack, int argument_count) {
    CCharacter *source = GetChara(GetStackInt(stack++));

    if (source == NULL) {
        return 0;
    }

    char *name = GetStackString(stack++);

    if (name == NULL) {
        return 0;
    }

    CFrame *reference;

    if (name[0] != '\0') {
        reference = source->frame->SearchFrame(name);
    } else {
        reference = source->frame;
    }

    if (reference == NULL) {
        return 0;
    }

    CCharacter *target = GetChara(GetStackInt(stack));

    if (target == NULL) {
        return 0;
    }

    if (target->frame == NULL) {
        return 0;
    }

    target->frame->SetReference(reference);

    if (target->shadow_frame != NULL) {
        target->shadow_frame->SetReference(reference);
    }

    return 1;
}

static int _RELEASE_CHARA_CHARA(RS_STACKDATA *stack, int argument_count) {
    CCharacter *character = GetChara(GetStackInt(stack));

    if (character == NULL) {
        return 0;
    }

    if (character->frame != NULL) {
        character->frame->DeleteReference();
    }

    if (character->shadow_frame != NULL) {
        character->shadow_frame->DeleteReference();
    }

    return 1;
}

static int _GET_CHARA_POS(RS_STACKDATA *stack, int argument_count) {
    sceVu0FVECTOR position;

    if (argument_count < 3) {
        return 0;
    }

    EdEventInfo.main_character->GetPosition(position);
    SetPosition(stack, position);
    return 1;
}

static int _GET_CHARA_TALK_POS(RS_STACKDATA *stack, int argument_count) {
    int position[2];

    if (argument_count < 2) {
        return 0;
    }

    GetScrPosFromChar(EdEventInfo.main_character, position);
    SetStack(stack++, position[0]);
    SetStack(stack, position[1]);
    return 1;
}

static int _SET_CHARA_POS(RS_STACKDATA *stack, int argument_count) {
    sceVu0FVECTOR position;
    GetPosition(stack, position);
    EdEventInfo.main_character->SetPosition(position);
    return 1;
}

static int _SET_CHARA_ROT(RS_STACKDATA *stack, int argument_count) {
    sceVu0FVECTOR rotation;

    if (argument_count == 3) {
        GetRotation(stack, rotation);
        stack += 3;
    }

    if (argument_count == 1) {
        rotation[2] = 0.0f;
        rotation[0] = 0.0f;
        rotation[1] = GetStackFloat(stack);
        GetWorldRot(rotation, rotation);
    }

    EdEventInfo.main_character->SetRotation(rotation);
    return 1;
}

static int _GET_CHARA_ROT(RS_STACKDATA *stack, int argument_count) {
    sceVu0FVECTOR rotation;

    if (argument_count < 3) {
        return 0;
    }

    EdEventInfo.main_character->GetRotation(rotation);
    SetRotation(stack, rotation);
    return 1;
}

int _TURN_CHARA(RS_STACKDATA *stack, int argument_count) {
    sceVu0FVECTOR position;
    GetPosition(stack, position);
    turn_chara(EdEventInfo.main_character, position, GetStackFloat(stack += 3));
    return 1;
}

static int _GET_NPC_TALK_POS(RS_STACKDATA *stack, int argument_count) {
    int position[2];

    if (argument_count < 2) {
        return 0;
    }

    RS_STACKDATA *result = (0, stack + 1);
    int           index = GetStackInt(stack);

    if (index == -1) {
        return _GET_CHARA_TALK_POS(result, argument_count - 1);
    }

    if (index < 0 || index >= EdEventInfo.npc_count) {
        return 0;
    }

    GetScrPosFromChar(&EdEventInfo.npcs[index], position);
    SetStack(result++, position[0]);
    SetStack(result, position[1]);
    return 1;
}

static int _GET_NPC_POS(RS_STACKDATA *stack, int argument_count) {
    sceVu0FVECTOR position;

    if (argument_count < 4) {
        return 0;
    }

    RS_STACKDATA *result = (0, stack + 1);
    CCharacter   *character = GetChara(GetStackInt(stack));

    if (character == NULL) {
        return 0;
    }

    character->GetPosition(position);

    if (character->frame != NULL && character->frame->parent != NULL) {
        SetStack(result++, position[0]);
        SetStack(result++, position[1]);
        SetStack(result, position[2]);
        return 1;
    }

    SetPosition(result, position);
    return 1;
}

static int _SET_NPC_POS(RS_STACKDATA *stack, int argument_count) {
    sceVu0FVECTOR position;
    RS_STACKDATA *argument = (0, stack + 1);
    CCharacter   *character = GetChara(GetStackInt(stack));

    if (character == NULL) {
        return 0;
    }

    if (character->frame != NULL && character->frame->parent != NULL) {
        position[0] = GetStackFloat(argument++);
        position[1] = GetStackFloat(argument++);
        position[2] = GetStackFloat(argument);
        position[3] = 1.0f;
        character->SetPosition(position);
        return 1;
    }

    GetPosition(argument, position);
    character->SetPosition(position);
    return 1;
}

static int _SET_NPC_ROT(RS_STACKDATA *stack, int argument_count) {
    sceVu0FVECTOR rotation;
    CCharacter   *character = GetChara(GetStackInt(stack++));

    if (character == NULL) {
        return 0;
    }

    CFrame *parent = NULL;

    if (character->frame != NULL) {
        parent = character->frame->parent;
    }

    if (argument_count == 4) {
        if (parent != NULL) {
            rotation[0] = GetStackFloat(stack++);
            rotation[1] = GetStackFloat(stack++);
            rotation[2] = GetStackFloat(stack++);
        } else {
            GetRotation(stack, rotation);
            stack += 3;
        }
    }

    if (argument_count == 2) {
        rotation[2] = 0.0f;
        rotation[0] = 0.0f;
        rotation[1] = GetStackFloat(stack);

        if (parent == NULL) {
            GetWorldRot(rotation, rotation);
        }
    }

    character->SetRotation(rotation);
    return 1;
}

static int _GET_NPC_ROT(RS_STACKDATA *stack, int argument_count) {
    sceVu0FVECTOR rotation;

    if (argument_count < 4) {
        return 0;
    }

    RS_STACKDATA *result = (0, stack + 1);
    CCharacter   *character = GetChara(GetStackInt(stack));

    if (character == NULL) {
        return 0;
    }

    character->GetRotation(rotation);

    if (character->frame != NULL && character->frame->parent != NULL) {
        SetStack(result++, rotation[0]);
        SetStack(result++, rotation[1]);
        SetStack(result, rotation[2]);
        return 1;
    }

    SetRotation(result, rotation);
    return 1;
}

static int _SET_NPC_SCALE(RS_STACKDATA *stack, int argument_count) {
    CCharacter *character = GetChara(GetStackInt(stack++));

    if (character == NULL) {
        return 0;
    }

    float x = GetStackFloat(stack++);

    if (argument_count == 2) {
        character->SetScale(x, x, x);
    } else {
        float y = GetStackFloat(stack++);
        float z = GetStackFloat(stack);
        character->SetScale(x, y, z);
    }

    return 1;
}

static int _GET_NPC_SCALE(RS_STACKDATA *stack, int argument_count) {
    sceVu0FVECTOR scale;
    RS_STACKDATA *result = (0, stack + 1);
    CCharacter   *character = GetChara(GetStackInt(stack));

    if (character == NULL) {
        return 0;
    }

    character->GetScale(scale);
    SetStack(result++, scale[0]);
    SetStack(result++, scale[1]);
    SetStack(result, scale[2]);
    return 1;
}

static int _NPC_POS_INIT(RS_STACKDATA *stack, int argument_count) {
    sceVu0FVECTOR transform = {0.0f, 0.0f, 0.0f, 0.0f};

    for (int i = 0; i < argument_count; i++) {
        CCharacter *character = GetChara(GetStackInt(stack++));

        if (character != NULL) {
            character->SetPosition(transform);
            character->SetRotation(transform);
        }
    }

    return 1;
}

static int _GET_NPC_PARTS_NO(RS_STACKDATA *stack, int argument_count) {
    if (argument_count < 2) {
        return 0;
    }

    int index = GetStackInt(stack++);

    if (index < 0 || index >= 16) {
        return 0;
    }

    SetStack(stack++, EdEventInfo.villagers[index].character_no);
    return 1;
}

static int _SET_NPC_MOTION(RS_STACKDATA *stack, int argument_count) {
    int         index = GetStackInt(stack++);
    CCharacter *character;

    if (index == -1) {
        character = EdEventInfo.main_character;
    } else {
        character = GetNPC(index);
    }

    if (character == NULL) {
        return 0;
    }

    int   motion = GetStackInt(stack++);
    float speed = -1.0f;

    if (argument_count >= 3) {
        speed = GetStackFloat(stack++);
    }

    if (argument_count == 4) {
        character->SetMotion(motion, GetStackInt(stack));
    } else {
        character->SetMotion(motion, 0);
    }

    if (speed > 0.0f) {
        character->SetMotionSpeed(speed);
    }

    return 1;
}

static int _SET_NPC_ANIME(RS_STACKDATA *stack, int argument_count) {
    int         index = GetStackInt(stack++);
    CCharacter *character;

    if (index == -1) {
        character = EdEventInfo.main_character;
    } else {
        character = GetNPC(index);
    }

    if (character == NULL) {
        return 0;
    }

    int animation = GetStackInt(stack++);
    int enable = true;

    if (argument_count == 3) {
        enable = GetStackInt(stack);
    }

    if (animation == -1 && enable == 0) {
        character->ClearTexAnime();
    }

    if (enable != 0) {
        character->TexAnimeOn(animation);
    } else {
        character->TexAnimeOff(animation);
    }

    return 1;
}

static int _TURN_NPC(RS_STACKDATA *stack, int argument_count) {
    sceVu0FVECTOR position;
    int           index = GetStackInt(stack++);

    if (index == -1) {
        return _TURN_CHARA(stack, argument_count - 1);
    }

    CCharacter *character = GetNPC(index);

    if (character == NULL) {
        return 0;
    }

    GetPosition(stack, position);
    turn_chara(character, position, GetStackFloat(stack += 3));
    return 1;
}

int _NPC_DRAW(RS_STACKDATA *stack, int argument_count) {
    int draw = GetStackInt(stack++);

    for (int i = 0; i < argument_count - 1; i++) {
        int index = GetStackInt(stack++);

        if (GetChara(index) != NULL) {
            if (index < 0) {
                EdEventInfo.player_draw = draw;
            }

            if (index >= 0 && index < 16) {
                EdEventInfo.npc_draw[index] = draw;
            }
        }
    }

    return 1;
}

int _NPC_DRAW_SHADOW(RS_STACKDATA *stack, int argument_count) {
    int draw = GetStackInt(stack++);

    for (int i = 0; i < argument_count - 1; i++) {
        int index = GetStackInt(stack++);

        if (GetChara(index) != NULL) {
            if (index < 0) {
                EdEventInfo.player_shadow_draw = draw;
            }

            if (index >= 0 && index < 16) {
                EdEventInfo.npc_shadow_draw[index] = draw;
            }
        }
    }

    return 1;
}

int _SET_NPC_FOOT_SOUND(RS_STACKDATA *stack, int argument_count) {
    int mode = GetStackInt(stack++);

    for (int i = 0; i < argument_count - 1; i++) {
        int         index = GetStackInt(stack++);
        CCharacter *character = GetChara(index);

        if (character != NULL) {
            if (index < 0) {
                EdEventInfo.player_foot_sound = mode;
            }

            if (index >= 0 && index < 16) {
                EdEventInfo.npc_foot_sound[index] = mode;
            }

            if (mode == 2) {
                character->FootSoundEnable(1);
            }
        }
    }

    return 1;
}

static int _SET_NPC_FLOOR_ID(RS_STACKDATA *stack, int argument_count) {
    RS_STACKDATA *argument = (0, stack + 1);
    CCharacter   *character = GetChara(GetStackInt(stack));

    if (character == NULL) {
        return 0;
    }

    character->SetFootSoundID(GetStackInt(argument));
    return 1;
}

/**
 * Script opcode that steps a villager; the step happens elsewhere.
 *
 * @mangled _NPC_STEP__FP12RS_STACKDATAi
 * @address 0x18F0A0
 * @size 0xC
 */
static s32 _NPC_STEP(RS_STACKDATA *stack, s32 argument_count) {
    return 1;
}

static int _NPC_COL(RS_STACKDATA *stack, int argument_count) {
    int collision = GetStackInt(stack++);

    for (int i = 0; i < argument_count - 1; i++) {
        int index = GetStackInt(stack++);

        if (index >= 0) {
            EdEventInfo.npc_collision[index] = collision;
        }

        if (index == -1) {
            EdEventInfo.player_collision = collision;
        }
    }

    return 1;
}

static int _NPC_STOP(RS_STACKDATA *stack, int argument_count) {
    int stopped = GetStackInt(stack++);

    for (int i = 0; i < argument_count - 1; i++) {
        int index = GetStackInt(stack++);

        if (index >= 0) {
            EdEventInfo.npc_stop[index] = stopped;
        }

        if (index == -1) {
            EdEventInfo.player_stop = stopped;
        }
    }

    return 1;
}

static int _NPC_DRAW_BEFORE(RS_STACKDATA *stack, int argument_count) {
    int order = GetStackInt(stack++);
    int i = 0;

    while (i < argument_count - 1) {
        int index = GetStackInt(stack++);

        if (index >= 0) {
            EdEventInfo.npc_draw_before[index] = order;
        }

        i++;
    }

    return 1;
}

static int _INIT_NPC_CLOTH(RS_STACKDATA *stack, int argument_count) {
    int         index = GetStackInt(stack);
    CCharacter *character;

    if (index == -1) {
        character = EdEventInfo.main_character;
    } else {
        character = GetNPC(index);
    }

    if (character == NULL) {
        return 0;
    }

    character->ClothStep(-1);
    return 1;
}

static int _NPC_CLOTH_FLOOR(RS_STACKDATA *stack, int argument_count) {
    RS_STACKDATA *argument = (0, stack + 1);
    CCharacter   *character = GetChara(GetStackInt(stack));

    if (character == NULL) {
        return 0;
    }

    character->ClothFloor(GetStackInt(argument));
    return 1;
}

static int _NPC_CLOTH_STEP(RS_STACKDATA *stack, int argument_count) {
    CCharacter *character = GetChara(GetStackInt(stack++));

    if (character == NULL) {
        return 0;
    }

    int steps = 1;

    if (argument_count > 1) {
        steps = GetStackInt(stack);
    }

    for (int i = 0; i < steps; i++) {
        character->ClothStep(0);
    }

    return 1;
}

static int _SET_NPC_AMBIENT(RS_STACKDATA *stack, int argument_count) {
    CCharacter *character = GetChara(GetStackInt(stack++));

    if (character == NULL) {
        return 0;
    }

    sceVu0FVECTOR ambient = {0.0f, 0.0f, 0.0f, 128.0f};

    if (argument_count == 1) {
        sceVu0CopyVector(character->ambient_offset, ambient);
        return 1;
    }

    if ((unsigned int) (argument_count - 2) < 2) {
        float value = GetStackInt(stack++);
        character->ambient_offset[0] = value;
        character->ambient_offset[1] = value;
        character->ambient_offset[2] = value;
    }

    if (argument_count >= 4) {
        character->ambient_offset[0] = GetStackInt(stack++);
        character->ambient_offset[1] = GetStackInt(stack++);
        character->ambient_offset[2] = GetStackInt(stack++);
    }

    if (argument_count >= 5) {
        character->ambient_offset[3] = GetStackInt(stack);
    }

    return 1;
}

static int _SET_NPC_BODY_SIZE(RS_STACKDATA *stack, int argument_count) {
    CCharacter *character = GetChara(GetStackInt(stack++));

    if (character == NULL) {
        return 0;
    }

    if (argument_count > 1) {
        character->body_width = GetStackFloat(stack++);
    }

    if (argument_count > 2) {
        character->body_height = GetStackFloat(stack++);
    }

    if (argument_count > 3) {
        character->body_depth = GetStackFloat(stack);
    }

    return 1;
}

static int _GET_NPC_BODY_SIZE(RS_STACKDATA *stack, int argument_count) {
    CCharacter *character = GetChara(GetStackInt(stack++));

    if (character == NULL) {
        return 0;
    }

    if (argument_count > 1) {
        SetStack(stack++, character->body_width);
    }

    if (argument_count > 2) {
        SetStack(stack++, character->body_height);
    }

    if (argument_count > 3) {
        SetStack(stack, character->body_depth);
    }

    return 1;
}

static int _NPC_PLIGHT_INIT(RS_STACKDATA *stack, int argument_count) {
    CCharacter *character = GetChara(GetStackInt(stack));

    if (character == NULL) {
        return 0;
    }

    character->ClearPointLight();
    return 1;
}

static int _SET_NPC_PLIGHT(RS_STACKDATA *stack, int argument_count) {
    sceVu0FVECTOR position;
    float         light[5];
    CCharacter   *character = GetChara(GetStackInt(stack++));

    if (character == NULL) {
        return 0;
    }

    GetPosition(stack, position);
    stack += 3;

    for (int i = 0; i < 5; i++) {
        light[i] = GetStackFloat(stack++);
    }

    return character->SetPointLight(position, light[0], light[1], light[2], light[3], light[4], 128.0f);
}

static int _SGET_NPC_TALK_MES(RS_STACKDATA *stack, int argument_count) {
    if (argument_count < 2) {
        return 0;
    }

    RS_STACKDATA *result = (0, stack + 1);
    SV_GRD_NPC   *npc = SaveData->GetGrdNPCData(NowEditMap, GetStackInt(stack));

    if (npc == NULL) {
        return 0;
    }

    SetStack(result, npc->talk_message);
    return 1;
}

static int _SSET_NPC_TALK_MES(RS_STACKDATA *stack, int argument_count) {
    RS_STACKDATA *next = (0, stack + 1);
    SV_GRD_NPC   *npc = SaveData->GetGrdNPCData(NowEditMap, GetStackInt(stack));

    if (npc == NULL) {
        return 0;
    }

    npc->talk_message = GetStackInt(next);
    return 1;
}

static ClsMes *GetMes(int index) {
    if (index < 0 || index >= 8) {
        return NULL;
    }

    return EdEventInfo.messages[index];
}

static int _MES_MAKE(RS_STACKDATA *stack, int argument_count) {
    ClsMes *message = GetMes(GetStackInt(stack++));

    if (message == NULL) {
        return 0;
    }

    if (EdEventInfo.camera != NULL) {
        sceVu0FMATRIX view;
        sceVu0FVECTOR eye;
        EdEventInfo.camera->GetCameraMatrix(view);
        EdEventInfo.camera->GetPos(eye);
        MGSetViewMatrix(view, eye);
    }

    int message_no = GetStackInt(stack++);

    if (argument_count >= 3) {
        message->value = GetStackInt(stack++);
    }

    if (argument_count >= 4) {
        for (int i = 0; i < 4; i++) {
            message->mes_no[i] = -1;
        }

        for (int i = 4; argument_count >= i; i++) {
            int slot_message = GetStackInt(stack++);
            message->mes_no[i - 4] = slot_message;
            printf("%d %d\n", i - 4, slot_message);
        }
    }

    message->MakeMesWin(message_no);
    return 1;
}

static int _MES_CLOSE(RS_STACKDATA *stack, int argument_count) {
    ClsMes *message = GetMes(GetStackInt(stack));

    if (message == NULL) {
        return 0;
    }

    message->text_rate = message->text_rate_set;
    message->mes_made = -1;
    message->fade_in = false;
    return 1;
}

static int _MES_NEXTPAGE(RS_STACKDATA *stack, int argument_count) {
    ClsMes *message = GetMes(GetStackInt(stack));

    if (message == NULL) {
        return 0;
    }

    message->GoNextPage();
    return 1;
}

int _SET_MES_AUTOSET(RS_STACKDATA *stack, int argument_count) {
    ClsMes *message = GetMes(GetStackInt(stack++));

    if (message == NULL) {
        return 0;
    }

    if (EdEventInfo.camera != NULL) {
        sceVu0FMATRIX view;
        sceVu0FVECTOR eye;
        EdEventInfo.camera->GetCameraMatrix(view);
        EdEventInfo.camera->GetPos(eye);
        MGSetViewMatrix(view, eye);
    }

    int position[4];
    int talk_position[4];

    if (argument_count == 5) {
        for (int i = 0; i < 4; i++) {
            position[i] = GetStackInt(stack++);
        }

        message->AutoSet(position);
        return 1;
    }

    if (argument_count == 3) {
        int         first_index = GetStackInt(stack++);
        int         second_index = GetStackInt(stack);
        CCharacter *speaker = first_index == -1 ? EdEventInfo.main_character : GetNPC(first_index);
        CCharacter *listener = second_index == -1 ? EdEventInfo.main_character : GetNPC(second_index);
        message->AutoSetSub(speaker, listener, talk_position);
        message->AutoSet(talk_position);
        return 1;
    }

    return 0;
}

static int _SET_MES_SHIPPO(RS_STACKDATA *stack, int argument_count) {
    ClsMes *message = GetMes(GetStackInt(stack++));

    if (message == NULL) {
        return 0;
    }

    message->tail_on = GetStackInt(stack++);

    if (argument_count > 2) {
        message->tail_length = GetStackInt(stack++);
    }

    if (argument_count > 3) {
        message->tail_half_width = GetStackInt(stack);
    }

    return 1;
}

static int _SET_MES_POS(RS_STACKDATA *stack, int argument_count) {
    RS_STACKDATA *next = (0, stack + 1);
    ClsMes       *message = GetMes(GetStackInt(stack));

    if (message == NULL) {
        return 0;
    }

    message->auto_pos = GetStackInt(next);
    return 1;
}

static int _SET_MES_DRAWSPEED(RS_STACKDATA *stack, int argument_count) {
    RS_STACKDATA *next = (0, stack + 1);
    ClsMes       *message = GetMes(GetStackInt(stack));

    if (message == NULL) {
        return 0;
    }

    message->text_rate = GetStackFloat(next);
    return 1;
}

static int _SET_MES_CURSOR(RS_STACKDATA *stack, int argument_count) {
    RS_STACKDATA *next = (0, stack + 1);
    ClsMes       *message = GetMes(GetStackInt(stack));

    if (message == NULL) {
        return 0;
    }

    message->cursor_row = GetStackInt(next);
    return 1;
}

static int _SET_MES_OKURI(RS_STACKDATA *stack, int argument_count) {
    RS_STACKDATA *next = (0, stack + 1);
    ClsMes       *message = GetMes(GetStackInt(stack));

    if (message == NULL) {
        return 0;
    }

    message->page_arrow = GetStackInt(next);
    return 1;
}

static int _SET_MES_WIN_FLAG(RS_STACKDATA *stack, int argument_count) {
    RS_STACKDATA *next = (0, stack + 1);
    ClsMes       *message = GetMes(GetStackInt(stack));

    if (message == NULL) {
        return 0;
    }

    message->stay_frame = GetStackInt(next);
    return 1;
}

static int _CHECK_MES_COMPLETE(RS_STACKDATA *stack, int argument_count) {
    if (argument_count < 2) {
        return 0;
    }

    RS_STACKDATA *result = (0, stack + 1);
    ClsMes       *message = GetMes(GetStackInt(stack));

    if (message == NULL) {
        return 0;
    }

    SetStack(result, message->State() == CLSMES_SHOWN);
    return 1;
}

static int _CHECK_MES_WAIT(RS_STACKDATA *stack, int argument_count) {
    if (argument_count < 2) {
        return 0;
    }

    RS_STACKDATA *result = (0, stack + 1);
    ClsMes       *message = GetMes(GetStackInt(stack));

    if (message == NULL) {
        return 0;
    }

    SetStack(result, message->State() == CLSMES_PAGE_WAIT);
    return 1;
}

static int _CHECK_MES(RS_STACKDATA *stack, int argument_count) {
    if (argument_count < 2) {
        return 0;
    }

    RS_STACKDATA *result = (0, stack + 1);
    ClsMes       *message = GetMes(GetStackInt(stack));

    if (message == NULL) {
        return 0;
    }

    SetStack(result, message->State() == CLSMES_CLOSED);
    return 1;
}

static int _SET_MES_FUKIDASHI(RS_STACKDATA *stack, int argument_count) {
    RS_STACKDATA *next = (0, stack + 1);
    ClsMes       *message = GetMes(GetStackInt(stack));

    if (message == NULL) {
        return 0;
    }

    message->SetMesFukidashi(GetStackInt(next));
    return 1;
}

static int _ITEM_GET_MES(RS_STACKDATA *stack, int argument_count) {
    int item = GetStackInt(stack++);
    int attachment = -1;

    if (GetAddAttachItem(item) != 0) {
        attachment = 3;
    }

    if (argument_count == 2) {
        attachment = GetStackInt(stack);
    }

    ItemGetMes(item, attachment, 40, 1);
    return 1;
}

static int _INIT_SYS_MES(RS_STACKDATA *stack, int argument_count) {
    ClearSystemMes();
    return 1;
}

static int _SKILL_GET_MES(RS_STACKDATA *stack, int argument_count) {
    TecGetMes(GetStackInt(stack), 40);
    return 1;
}

static int _ADD_MAXITEM_MES(RS_STACKDATA *stack, int argument_count) {
    int amount = GetStackInt(stack);

    if (amount < 0) {
        amount = EdAddMaxItem(0);
    }

    MaxUpMes(amount, 40);
    return 1;
}

static int _CHECK_COMPLETE_PARTS(RS_STACKDATA *stack, int argument_count) {
    if (argument_count < 2) {
        return 0;
    }

    RS_STACKDATA *result = (0, stack + 1);
    SetStack(result, (0, EditPartsInfo.CheckComplete(GetStackInt(stack))));
    return 1;
}

static int _GET_EDIT_PARTS_POS(RS_STACKDATA *stack, int argument_count) {
    sceVu0FVECTOR position;
    sceVu0FVECTOR rotation;
    CMapParts    *parts = EdEventInfo.edit_ground->GetPartsObject(GetStackInt(stack++));

    if (parts == NULL) {
        return 0;
    }

    if (argument_count < 4) {
        return 0;
    }

    parts->GetPosition(position);
    parts->GetRotation(rotation);
    SetStack(stack++, position[0]);
    SetStack(stack++, position[1]);
    SetStack(stack++, position[2]);

    if (argument_count == 7) {
        SetStack(stack++, rotation[0]);
        SetStack(stack++, rotation[1]);
        SetStack(stack, rotation[2]);
    }

    return 1;
}

static int _GET_NOW_TIME(RS_STACKDATA *stack, int argument_count) {
    SetStack(stack, (0, EdGetTime(EdEventInfo.current_time)));
    return 1;
}

static int _SET_CLOCK(RS_STACKDATA *stack, int argument_count) {
    EdSetClock(GetStackFloat(stack));
    return 1;
}

static int _GET_CLOCK(RS_STACKDATA *stack, int argument_count) {
    SetStack(stack, EdGetClock());
    return 1;
}

static int _SGET_CMP_EVENT(RS_STACKDATA *stack, int argument_count) {
    RS_STACKDATA *result = (0, stack + 1);
    int           event = GetStackInt(stack);
    SetStack(result, (0, EditPartsInfo.GetCompEvent(event)));
    return 1;
}

static int _SSET_CMP_EVENT(RS_STACKDATA *stack, int argument_count) {
    RS_STACKDATA *next = (0, stack + 1);
    int           event = GetStackInt(stack);
    EditPartsInfo.SetCompEvent(event, GetStackInt(next));
    return 1;
}

static int _SCHECK_REQUEST(RS_STACKDATA *stack, int argument_count) {
    RS_STACKDATA *result = (0, stack + 1);
    int           request = GetStackInt(stack);
    SetStack(result, (0, EditPartsInfo.GetRequest(request)));
    return 1;
}

static int _CHECK_PLACE(RS_STACKDATA *stack, int argument_count) {
    SetStack(stack, EdInteriorFlag);
    return 1;
}

static int _CHECK_VILLAGER(RS_STACKDATA *stack, int argument_count) {
    RS_STACKDATA *result = (0, stack + 1);
    int           index = GetStackInt(stack);
    SetStack(result, EdCheckVillager(index, &EdEventInfo.villagers[index], EdEventInfo.edit_ground));
    return 1;
}

static int _DELETE_ROBO_PARTS(RS_STACKDATA *stack, int argument_count) {
    EdDeleteE05RoboParts();
    return 1;
}

static int _HOBJ_FIXPARTS(RS_STACKDATA *stack, int argument_count) {
    if (EdEventInfo.fixed_parts == NULL) {
        return 1;
    }

    if (EdEventInfo.fixed_parts_count <= 0) {
        return 1;
    }

    int handle = GetStackInt(stack++);
    int index = GetStackInt(stack++);

    if (index < 0 || index >= EdEventInfo.fixed_parts_count) {
        return 0;
    }

    int result = 1;
    return SetObjHandle(handle, &EdEventInfo.fixed_parts[index], GetStackString(stack)) ? result : 0;
}

static int _HOBJ_EDITPARTS(RS_STACKDATA *stack, int argument_count) {
    if (EdEventInfo.edit_parts == NULL) {
        return 1;
    }

    if (EdEventInfo.edit_parts_count <= 0) {
        return 1;
    }

    int handle = GetStackInt(stack++);
    int index = GetStackInt(stack++);

    if (index < 0 || index >= EdEventInfo.edit_parts_count) {
        return 0;
    }

    int result = 1;
    return SetObjHandle(handle, &EdEventInfo.edit_parts[index], GetStackString(stack)) ? result : 0;
}

static int _HOBJ_INTERIORPARTS(RS_STACKDATA *stack, int argument_count) {
    if (EdEventInfo.interior_parts == NULL) {
        return 1;
    }

    if (EdEventInfo.interior_parts_count <= 0) {
        return 1;
    }

    int handle = GetStackInt(stack++);
    int index = GetStackInt(stack++);

    if (index < 0 || index >= EdEventInfo.interior_parts_count) {
        return 0;
    }

    int result = 1;
    return SetObjHandle(handle, &EdEventInfo.interior_parts[index], GetStackString(stack)) ? result : 0;
}

static int _HOBJ_CHARA(RS_STACKDATA *stack, int argument_count) {
    int         handle_index = GetStackInt(stack++);
    CCharacter *character = GetChara(GetStackInt(stack++));

    if (character == NULL) {
        return 0;
    }

    return SetObjHandle(handle_index, character, GetStackString(stack)) != 0 ? 1 : 0;
}

static int _HOBJ_BT_HOBJ(RS_STACKDATA *stack, int argument_count) {
    OBJ_HANDLE *handle;
    int         i;
    int         count;
    count = argument_count;
    handle = GetObjHandle(GetStackInt(stack++));

    if (handle == NULL) {
        return 0;
    }

    memset(handle, 0, sizeof(OBJ_HANDLE));

    if (12 < count) {
        count = 12;
    }

    for (i = 0; i < count - 1; i++) {
        BT_OBJ_HANDLE *object = GetObjHDL(GetStackInt(stack++));

        if (object == NULL) {
            return 0;
        }

        if (object->type == 1) {
            if (2 < count) {
                return 0;
            }

            handle->character = object->character;
        }

        if (object->type == 0) {
            handle->frames[i] = object->frame;
        }
    }

    return 1;
}

static int _HOBJ_ITEM(RS_STACKDATA *stack, int argument_count) {
    RS_STACKDATA *next = (0, stack + 1);
    int           handle_index = GetStackInt(stack);
    CFrame       *frame = GetItemFrame(GetStackInt(next));

    if (frame == NULL) {
        return 0;
    }

    return SetObjHandle(handle_index, frame) != 0 ? 1 : 0;
}

static int _OBJ_DRAW(RS_STACKDATA *stack, int argument_count) {
    int draw = GetStackInt(stack++);

    for (int i = 0; i < argument_count - 1; i++) {
        OBJ_HANDLE *handle = GetObjHandle(GetStackInt(stack++));

        if (handle != NULL) {
            obj_draw(handle, draw);
        }
    }

    return 1;
}

static int _SET_OBJ_POS(RS_STACKDATA *stack, int argument_count) {
    sceVu0FVECTOR position;
    OBJ_HANDLE   *handle = GetObjHandle(GetStackInt(stack++));

    if (handle == NULL) {
        return 0;
    }

    position[0] = GetStackFloat(stack++);
    position[1] = GetStackFloat(stack++);
    position[2] = GetStackFloat(stack);
    position[3] = 1.0f;
    set_obj_pos(handle, position);
    return 1;
}

static int _GET_OBJ_POS(RS_STACKDATA *stack, int argument_count) {
    sceVu0FVECTOR position;
    OBJ_HANDLE   *handle = GetObjHandle(GetStackInt(stack++));

    if (handle == NULL) {
        return 0;
    }

    if (argument_count < 4) {
        return 0;
    }

    get_obj_pos(handle, position);
    SetStack(stack++, position[0]);
    SetStack(stack++, position[1]);
    SetStack(stack, position[2]);
    return 1;
}

static int _SET_OBJ_ROT(RS_STACKDATA *stack, int argument_count) {
    sceVu0FVECTOR rotation;
    OBJ_HANDLE   *handle = GetObjHandle(GetStackInt(stack++));

    if (handle == NULL) {
        return 0;
    }

    rotation[0] = AngleLimit(GetStackFloat(stack++));
    rotation[1] = AngleLimit(GetStackFloat(stack++));
    rotation[2] = AngleLimit(GetStackFloat(stack));
    rotation[3] = 0.0f;
    set_obj_rot(handle, rotation);
    return 1;
}

static int _GET_OBJ_ROT(RS_STACKDATA *stack, int argument_count) {
    sceVu0FVECTOR rotation;
    OBJ_HANDLE   *handle = GetObjHandle(GetStackInt(stack++));

    if (handle == NULL) {
        return 0;
    }

    if (argument_count < 4) {
        return 0;
    }

    get_obj_rot(handle, rotation);
    SetStack(stack++, rotation[0]);
    SetStack(stack++, rotation[1]);
    SetStack(stack, rotation[2]);
    return 1;
}

static int _SET_OBJ_SCALE(RS_STACKDATA *stack, int argument_count) {
    RS_STACKDATA *arguments = (0, stack + 1);
    OBJ_HANDLE   *handle = GetObjHandle(GetStackInt(stack));

    if (handle == NULL) {
        return 0;
    }

    sceVu0FVECTOR scale;
    scale[0] = GetStackFloat(arguments++);
    scale[1] = GetStackFloat(arguments++);
    scale[2] = GetStackFloat(arguments);
    scale[3] = 0.0f;
    set_obj_scale(handle, scale);
    return 1;
}

static int _GET_OBJ_SCALE(RS_STACKDATA *stack, int argument_count) {
    sceVu0FVECTOR scale;
    OBJ_HANDLE   *handle = GetObjHandle(GetStackInt(stack++));

    if (handle == NULL) {
        return 0;
    }

    if (argument_count < 4) {
        return 0;
    }

    get_obj_scale(handle, scale);
    SetStack(stack++, scale[0]);
    SetStack(stack++, scale[1]);
    SetStack(stack, scale[2]);
    return 1;
}

static int _SYNC_OBJ_OBJ(RS_STACKDATA *stack, int argument_count) {
    RS_STACKDATA *next = (0, stack + 1);
    OBJ_HANDLE   *source = GetObjHandle(GetStackInt(stack));

    if (source == NULL) {
        return 0;
    }

    OBJ_HANDLE *target = GetObjHandle(GetStackInt(next));

    if (target == NULL) {
        return 0;
    }

    sync_obj_obj(source, target);
    return 1;
}

static int _RELEASE_OBJ_OBJ(RS_STACKDATA *stack, int argument_count) {
    OBJ_HANDLE *handle = GetObjHandle(GetStackInt(stack));

    if (handle == NULL) {
        return 0;
    }

    release_obj_obj(handle);
    return 1;
}

static int _SET_OBJ_FOG(RS_STACKDATA *stack, int argument_count) {
    OBJ_HANDLE *handle = GetObjHandle(GetStackInt(stack++));

    if (handle == NULL) {
        return 0;
    }

    CFrameAttr attr;
    attr.fog_enable = GetStackInt(stack++);
    set_attr_obj(handle, attr, GetStackInt(stack), 0x40);
    return 1;
}

static int _GET_TALKNPC_INFO_ID(RS_STACKDATA *stack, int argument_count) {
    if (argument_count <= 0) {
        return 0;
    }

    SetStack(stack, EdNowTalkCharaInfoID());
    return 1;
}

static int _GET_TALKNPC_ID(RS_STACKDATA *stack, int argument_count) {
    if (argument_count <= 0) {
        return 0;
    }

    SetStack(stack, EdEventInfo.talk_npc_id);
    return 1;
}

/**
 * Pushes the active conversation villager's event status onto the script stack.
 *
 * @mangled _GET_TALKNPC_STATUS__FP12RS_STACKDATAi
 * @address 0x191530
 * @size 0x74
 */
static int _GET_TALKNPC_STATUS(RS_STACKDATA *stack, int argument_count) {
    int index = EdEventInfo.talk_npc_id;
    int status = 0;

    if (index >= 0 && index < EdEventInfo.npc_count) {
        CNPCharacter *npc = &EdEventInfo.npcs[index];
        status = npc->event_status;
    }

    SetStack(stack, status);
    return 1;
}

static int _SET_TALK_CAMERA(RS_STACKDATA *stack, int argument_count) {
    /**
     * Camera offsets from the speakers' midpoint for each of the three talk-camera views.
     */
    static sceVu0FVECTOR vv[3] = {
        {-18.8f, 7.1f, -21.3f, 1.0f},
        {21.0f,  6.5f, -5.7f,  1.0f},
        {22.6f,  6.9f, 16.5f,  1.0f},
    };

    int           view = GetStackInt(stack);
    sceVu0FVECTOR player_position;
    sceVu0FVECTOR npc_position;
    sceVu0FVECTOR direction;
    EdEventInfo.main_character->GetPosition(player_position);
    EdEventInfo.npcs[EdEventInfo.talk_npc_id].GetPosition(npc_position);

    if (EdEventInfo.npcs[EdEventInfo.talk_npc_id].event_status == 0) {
        view = 0;
    }

    sceVu0SubVector(direction, npc_position, player_position);
    float         angle = atan2f(direction[0], direction[2]);
    sceVu0FVECTOR midpoint;
    sceVu0AddVector(midpoint, player_position, npc_position);
    sceVu0ScaleVector(midpoint, midpoint, 0.5f);
    midpoint[1] += 15.0f;
    CCameraFollow *camera = EdEventInfo.camera;

    if (camera != NULL) {
        camera->FollowOff();
        camera->SetNextRef(NULL, midpoint[0], midpoint[1], midpoint[2]);
        camera->SetRef(midpoint);
    }

    sceVu0FVECTOR camera_position;
    sceVu0FMATRIX rotation;
    sceVu0UnitMatrix(rotation);
    sceVu0RotMatrixY(rotation, rotation, angle);
    sceVu0ApplyMatrix(camera_position, rotation, vv[view]);
    sceVu0AddVector(camera_position, midpoint, camera_position);

    if (camera != NULL) {
        camera->SetPos(camera_position);
    }

    if (camera != NULL) {
        camera->SetNextPos(NULL, camera_position[0], camera_position[1], camera_position[2]);
    }

    if (camera != NULL) {
        sceVu0FMATRIX view_matrix;
        sceVu0FVECTOR eye;
        camera->GetCameraMatrix(view_matrix);
        camera->GetPos(eye);
        MGSetViewMatrix(view_matrix, eye);
    }

    return 1;
}

static int _SET_TALK_MES(RS_STACKDATA *stack, int argument_count) {
    for (int i = 0; i < 16; i++) {
        EdEventInfo.talk_messages[i] = -1;
    }

    for (int i = 0; i < argument_count; i++) {
        if (i >= 16) {
            return 0;
        }

        EdEventInfo.talk_messages[i] = GetStackInt(stack++);
    }

    return 1;
}

static int _SET_TALK_SELECT_MES(RS_STACKDATA *stack, int argument_count) {
    RS_STACKDATA *next = (0, stack + 1);
    EdEventInfo.talk_select_prompt = GetStackInt(stack);
    EdEventInfo.talk_select_message = GetStackInt(next);
    return 1;
}

static int _EVERY_TALK_EVENT(RS_STACKDATA *stack, int argument_count) {
    int villager_id = GetStackInt(stack++);

    if (villager_id < 0 || villager_id >= 16) {
        return 0;
    }

    int event_no = villager_id + 0x104;

    if (argument_count > 1) {
        event_no = GetStackInt(stack);
    }

    for (int i = 0; i < EdEventInfo.npc_count; i++) {
        CNPCharacter *npc = GetNPC(i);

        if (npc->villager_id == villager_id) {
            npc->recurring_talk_event = event_no;
        }
    }

    return 1;
}

static CCameraFollow *GetCamera() {
    CCameraFollow *camera = EdEventInfo.camera;

    if (camera == NULL) {
        return NULL;
    }

    camera->FollowOff();
    return camera;
}

static int _SET_CAMERA(RS_STACKDATA *stack, int argument_count) {
    sceVu0FVECTOR  position;
    sceVu0FVECTOR  reference;
    CCameraFollow *camera = EdEventInfo.camera;

    if (camera == NULL) {
        return 0;
    }

    GetPosition(stack, position);
    GetPosition(stack + 3, reference);
    float speed = GetStackFloat(stack += 6);
    follow_chara = NULL;
    camera->FollowOff();
    camera->SetNextPos(NULL, position[0], position[1], position[2]);
    camera->SetNextRef(NULL, reference[0], reference[1], reference[2]);
    camera->SetSpeed(speed);

    if (speed <= 1.0f) {
        camera->SetPos(position);
        camera->SetRef(reference);
        camera->Step(-1);
    }

    return 1;
}

static int _SET_CAMERA_POS(RS_STACKDATA *stack, int argument_count) {
    CCameraFollow *camera = EdEventInfo.camera;

    if (camera == NULL) {
        return 0;
    }

    follow_chara = NULL;
    camera->FollowOff();
    sceVu0FVECTOR position;
    GetPosition(stack, position);
    camera->SetNextPos(NULL, position[0], position[1], position[2]);
    return 1;
}

static int _GET_CAMERA_POS(RS_STACKDATA *stack, int argument_count) {
    if (argument_count < 3) {
        return 0;
    }

    if (EdEventInfo.camera == NULL) {
        return 0;
    }

    sceVu0FVECTOR position;
    EdEventInfo.camera->GetPos(position);
    SetPosition(stack, position);
    return 1;
}

static int _SET_CAMERA_REF(RS_STACKDATA *stack, int argument_count) {
    CCameraFollow *camera = EdEventInfo.camera;

    if (camera == NULL) {
        return 0;
    }

    follow_chara = NULL;
    camera->FollowOff();
    sceVu0FVECTOR position;
    GetPosition(stack, position);
    camera->SetNextRef(NULL, position[0], position[1], position[2]);
    return 1;
}

static int _GET_CAMERA_REF(RS_STACKDATA *stack, int argument_count) {
    if (argument_count < 3) {
        return 0;
    }

    if (EdEventInfo.camera == NULL) {
        return 0;
    }

    sceVu0FVECTOR position;
    EdEventInfo.camera->GetRef(position);
    SetPosition(stack, position);
    return 1;
}

static int _SET_CAMERA_SPEED(RS_STACKDATA *stack, int argument_count) {
    CCameraFollow *camera = EdEventInfo.camera;

    if (camera == NULL) {
        return 0;
    }

    camera->SetSpeed((float) GetStackInt(stack));
    return 1;
}

static int _SET_FOLLOW_CAMERA(RS_STACKDATA *stack, int argument_count) {
    CCameraFollow *camera = EdEventInfo.camera;

    if (camera == NULL) {
        return 0;
    }

    float         values[4];
    sceVu0FVECTOR position[2];

    if (stack->type == RS_INT) {
        int index = GetStackInt(stack++);

        if (index == -1) {
            follow_chara = EdEventInfo.main_character;
        } else {
            follow_chara = GetNPC(index);
        }

        if (follow_chara != NULL) {
            follow_chara->GetPosition(position[1]);
        }
    } else {
        GetPosition(stack, position[1]);
        stack += 3;
        follow_chara = NULL;
    }

    for (int i = 0; i < 4; i++) {
        values[i] = GetStackFloat(stack++);
    }

    sceVu0FVECTOR rotation;
    rotation[1] = values[2];
    GetWorldRot(rotation, rotation);
    camera->FollowOn();
    camera->SetFollow(position[1][0], position[1][1], position[1][2]);
    camera->SetHeight(values[0]);
    camera->SetDistance(values[1]);
    camera->SetAngle(rotation[1]);
    camera->SetAngleSoon(rotation[1]);
    camera->SetSpeed(values[3]);

    if (values[3] <= 1.0f) {
        camera->Step(-1);
    }

    camera->FollowOn();
    return 1;
}

static int _ADD_CAMERA_ANGLE(RS_STACKDATA *stack, int argument_count) {
    CCameraFollow *camera = EdEventInfo.camera;

    if (camera == NULL) {
        return 0;
    }

    camera->AddAngle(GetStackFloat(stack));
    camera->FollowOn();
    return 1;
}

static int _ADD_CAMERA_HEIGHT(RS_STACKDATA *stack, int argument_count) {
    CCameraFollow *camera = EdEventInfo.camera;

    if (camera == NULL) {
        return 0;
    }

    camera->AddHeight(GetStackFloat(stack));
    camera->FollowOn();
    return 1;
}

static int _ADD_CAMERA_DIST(RS_STACKDATA *stack, int argument_count) {
    CCameraFollow *camera = EdEventInfo.camera;

    if (camera == NULL) {
        return 0;
    }

    camera->AddDistance(GetStackFloat(stack));
    camera->FollowOn();
    return 1;
}

static int _CAMERA_STEP(RS_STACKDATA *stack, int argument_count) {
    CCameraFollow *camera = EdEventInfo.camera;

    if (camera == NULL) {
        return 1;
    }

    if (follow_chara != NULL) {
        sceVu0FVECTOR position;
        follow_chara->GetPosition(position);
        camera->SetFollow(position[0], position[1] + 0.8f * follow_chara->body_height, position[2]);
    }

    return 1;
}

static int _SET_PROJECTION(RS_STACKDATA *stack, int argument_count) {
    EdEventInfo.projection = GetStackFloat(stack);
    return 1;
}

static int _ITEM_GET_CAMERA(RS_STACKDATA *stack, int argument_count) {
    CCameraFollow *camera = EdEventInfo.camera;

    if (camera == NULL) {
        return 0;
    }

    float distance = GetStackFloat(stack);

    if (distance < 0.0f) {
        return 1;
    }

    if (distance < 10.0f) {
        distance = 10.0f;
    }

    camera->FollowOff();
    CCharacter   *character = GetChara(-1);
    sceVu0FVECTOR character_position;
    sceVu0FVECTOR character_rotation;
    character->GetPosition(character_position);
    character_position[3] = 1.0f;
    character->GetRotation(character_rotation);
    sceVu0FMATRIX rotation;
    sceVu0UnitMatrix(rotation);
    sceVu0RotMatrixY(rotation, rotation, character_rotation[1]);
    sceVu0CopyVector(rotation[3], character_position);
    sceVu0FVECTOR camera_position = {0.0f, 16.0f, distance, 1.0f};
    sceVu0FVECTOR camera_reference = {0.0f, 15.0f, 0.0f, 1.0f};
    sceVu0ApplyMatrix(camera_position, rotation, camera_position);
    sceVu0ApplyMatrix(camera_reference, rotation, camera_reference);
    camera->SetPos(camera_position);
    camera->SetNextPos(NULL, camera_position[0], camera_position[1], camera_position[2]);
    camera->SetRef(camera_reference);
    camera->SetNextRef(NULL, camera_reference[0], camera_reference[1], camera_reference[2]);
    return 1;
}

static int _SET_CAMERA_ANGLE(RS_STACKDATA *stack, int argument_count) {
    CCamera *camera = GetCamera();

    if (camera == NULL) {
        return 0;
    }

    float yaw = AngleLimit(GetStackFloat(stack++));
    yaw = GetLocalRotY(yaw);
    sceVu0FVECTOR position;
    sceVu0FVECTOR reference;
    sceVu0FVECTOR direction;
    camera->GetPos(position);
    camera->GetRef(reference);
    sceVu0SubVector(direction, reference, position);

    if (argument_count == 1) {
        direction[1] = 0.0f;
    }

    float distance = DistVector(direction);
    float pitch = 0.0f;

    if (argument_count == 2) {
        pitch = AngleLimit(GetStackFloat(stack));
    }

    sceVu0FVECTOR offset = {0.0f, 0.0f, distance, 0.0f};
    sceVu0FMATRIX matrix;
    sceVu0FMATRIX rotation;
    sceVu0UnitMatrix(matrix);
    sceVu0RotMatrixX(rotation, matrix, pitch);
    sceVu0RotMatrixY(rotation, rotation, yaw);
    sceVu0ApplyMatrix(offset, rotation, offset);
    sceVu0AddVector(offset, offset, position);
    reference[0] = offset[0];
    reference[2] = offset[2];

    if (argument_count == 2) {
        reference[1] = offset[1];
    }

    camera->SetRef(reference);
    return 1;
}

static int _GET_CAMERA_ANGLE(RS_STACKDATA *stack, int argument_count) {
    CCamera *camera = GetCamera();

    if (camera == NULL) {
        return 0;
    }

    sceVu0FVECTOR position;
    sceVu0FVECTOR reference;
    sceVu0FVECTOR direction;
    sceVu0FVECTOR horizontal;
    camera->GetPos(position);
    camera->GetRef(reference);
    sceVu0SubVector(direction, reference, position);
    horizontal[0] = direction[0];
    horizontal[1] = 0.0f;
    horizontal[2] = direction[2];
    horizontal[3] = 0.0f;
    float distance = DistVector(horizontal);
    sceVu0Normalize(horizontal, horizontal);
    float yaw = atan2f(horizontal[0], horizontal[2]);
    float pitch = -atan2f(direction[1], distance);

    if (argument_count > 0) {
        SetStack(stack++, GetLocalRotY(yaw));
    }

    if (argument_count >= 2) {
        SetStack(stack, pitch);
    }

    return 1;
}

static int _SET_CAMERA_ROTATE(RS_STACKDATA *stack, int argument_count) {
    CCamera *camera = GetCamera();

    if (camera == NULL) {
        return 0;
    }

    float yaw = AngleLimit(GetStackFloat(stack));
    yaw = GetWorldRotY(yaw);
    sceVu0FVECTOR position;
    sceVu0FVECTOR reference;
    sceVu0FVECTOR direction;
    camera->GetPos(position);
    camera->GetRef(reference);
    sceVu0SubVector(direction, reference, position);
    direction[1] = 0.0f;
    float         distance = DistVector(direction);
    sceVu0FVECTOR offset = {0.0f, 0.0f, distance, 0.0f};
    sceVu0FMATRIX matrix;
    sceVu0FMATRIX rotation;
    sceVu0UnitMatrix(matrix);
    sceVu0RotMatrixY(rotation, matrix, yaw);
    sceVu0ApplyMatrix(offset, rotation, offset);
    sceVu0AddVector(offset, offset, reference);
    position[0] = offset[0];
    position[2] = offset[2];
    camera->SetPos(position);
    return 1;
}

static int _GET_CAMERA_ROTATE(RS_STACKDATA *stack, int argument_count) {
    sceVu0FVECTOR  position;
    sceVu0FVECTOR  reference;
    sceVu0FVECTOR  direction;
    CCameraFollow *camera = GetCamera();

    if (camera == NULL) {
        return 0;
    }

    camera->GetPos(position);
    camera->GetRef(reference);
    sceVu0SubVector(direction, position, reference);
    SetStack(stack, GetLocalRotY(atan2f(direction[0], direction[2])));
    return 1;
}

/**
 * Requests a camera reset on the next event frame, facing back along the reset yaw.
 *
 * @mangled _RESET_CAMERA__FP12RS_STACKDATAi
 * @address 0x1928F0
 * @size 0x38
 */
static int _RESET_CAMERA(RS_STACKDATA *stack, int argument_count) {
    EdEventInfo.reset_camera_angle = GetStackInt(stack);
    EdEventInfo.reset_camera_yaw = PI_SHORT;
    return 1;
}

static int _RESET_CAMERA_ANGLE(RS_STACKDATA *stack, int argument_count) {
    EdEventInfo.reset_camera_angle = 1;
    EdEventInfo.reset_camera_yaw = AngleLimit(GetStackFloat(stack));
    return 1;
}

static int _SYNC_CAMERA_REF_CHARA(RS_STACKDATA *stack, int argument_count) {
    sync_camera_ref_chara = GetChara(GetStackInt(stack++));

    if (argument_count == 4) {
        sync_camera_ref_offset[0] = GetStackFloat(stack++);
        sync_camera_ref_offset[1] = GetStackFloat(stack++);
        sync_camera_ref_offset[2] = GetStackFloat(stack);
    } else {
        sync_camera_ref_offset[0] = 0.0f;
        sync_camera_ref_offset[1] = 0.0f;
        sync_camera_ref_offset[2] = 0.0f;
    }

    return 1;
}

static int _RELEASE_CAMERA_REF_CHARA(RS_STACKDATA *stack, int argument_count) {
    sync_camera_ref_chara = NULL;
    return 1;
}

static int _SYNC_CAMERA_REF_OBJ(RS_STACKDATA *stack, int argument_count) {
    sync_camera_ref_obj = GetObjHandle(GetStackInt(stack++));

    if (argument_count == 4) {
        sync_camera_ref_offset[0] = GetStackFloat(stack++);
        sync_camera_ref_offset[1] = GetStackFloat(stack++);
        sync_camera_ref_offset[2] = GetStackFloat(stack);
    } else {
        sync_camera_ref_offset[0] = 0.0f;
        sync_camera_ref_offset[1] = 0.0f;
        sync_camera_ref_offset[2] = 0.0f;
    }

    return 1;
}

static int _RELEASE_CAMERA_REF_OBJ(RS_STACKDATA *stack, int argument_count) {
    sync_camera_ref_obj = NULL;
    return 1;
}

static int _SYNC_CAMERA_POS_OBJ(RS_STACKDATA *stack, int argument_count) {
    sync_camera_pos_obj = GetObjHandle(GetStackInt(stack));
    return 1;
}

static int _RELEASE_CAMERA_POS_OBJ(RS_STACKDATA *stack, int argument_count) {
    sync_camera_pos_obj = NULL;
    return 1;
}

static int _SET_CAMERA_ROLL(RS_STACKDATA *stack, int argument_count) {
    CCameraFollow *camera = GetCamera();

    if (camera == NULL) {
        return 0;
    }

    camera->SetRoll(GetStackFloat(stack));
    return 1;
}

static int _GET_CAMERA_ROLL(RS_STACKDATA *stack, int argument_count) {
    CCameraFollow *camera = GetCamera();

    if (camera == NULL) {
        return 0;
    }

    SetStack(stack, camera->GetRoll());
    return 1;
}

static int _FADE_IN(RS_STACKDATA *stack, int argument_count) {
    int red;
    int frames = (0, GetStackInt(stack++));
    int blue = 0;
    int green = 0;
    red = 0;

    if (argument_count == 4) {
        red = GetStackInt(stack++);
        green = GetStackInt(stack++);
        blue = GetStackInt(stack);
    }

    EdFadeIn(frames, (float) red, (float) green, (float) blue);
    return 1;
}

static int _FADE_OUT(RS_STACKDATA *stack, int argument_count) {
    int red;
    int frames = (0, GetStackInt(stack++));
    int blue = 0;
    int green = 0;
    red = 0;

    if (argument_count == 4) {
        red = GetStackInt(stack++);
        green = GetStackInt(stack++);
        blue = GetStackInt(stack);
    }

    EdFadeOut(frames, (float) red, (float) green, (float) blue);
    return 1;
}

static int _CHECK_FADE(RS_STACKDATA *stack, int argument_count) {
    if (argument_count <= 0) {
        return 0;
    }

    SetStack(stack, EdFadeOutCheck());
    return 1;
}

static int _INIT_SPRITE(RS_STACKDATA *stack, int argument_count) {
    SpriteTable.ClearPointer();
    SpriteTableBack.ClearPointer();
    return 1;
}

static int _DRAW_SPRITE(RS_STACKDATA *stack, int argument_count) {
    RECT  source;
    int   x = GetStackInt(stack++);
    int   y = GetStackInt(stack++);
    char *name = GetStackString(stack++);
    source.x = GetStackInt(stack++);
    source.y = GetStackInt(stack++);
    source.width = GetStackInt(stack++);
    source.height = GetStackInt(stack);
    CTexture *texture = TexManager.GetTexture(name, -1);

    if (texture == NULL) {
        return 0;
    }

    SpriteTable.AddTable(x, y, (sceGsTex0 *) &texture->tex0, &source, 0, 0);
    return 1;
}

static int _DRAW_BG_SPRITE(RS_STACKDATA *stack, int argument_count) {
    RECT  source;
    int   x = GetStackInt(stack++);
    int   y = GetStackInt(stack++);
    char *name = GetStackString(stack++);
    source.x = GetStackInt(stack++);
    source.y = GetStackInt(stack++);
    source.width = GetStackInt(stack++);
    source.height = GetStackInt(stack);
    CTexture *texture = TexManager.GetTexture(name, -1);

    if (texture == NULL) {
        return 0;
    }

    SpriteTableBack.AddTable(x, y, (sceGsTex0 *) &texture->tex0, &source, 0, 0);
    return 1;
}

static int _DRAW_BACK(RS_STACKDATA *stack, int argument_count) {
    EdEventInfo.suppress_background = ((GetStackInt(stack++) != 0) ^ 1) & 0xFF;
    float red = -1.0f;

    if (argument_count > 1) {
        red = GetStackFloat(stack++);
    }

    float blue = red;
    float green = red;

    if (argument_count == 4) {
        green = GetStackFloat(stack++);
        blue = GetStackFloat(stack);
    }

    EdEventInfo.background_color[0] = red;
    EdEventInfo.background_color[1] = green;
    EdEventInfo.background_color[2] = blue;
    return 1;
}

static int _DRAW_SHADOW(RS_STACKDATA *stack, int argument_count) {
    EdEventInfo.suppress_shadows = ((GetStackInt(stack) != 0) ^ 1) & 0xFF;
    return 1;
}

/**
 * Stores the editor ground's event clipping plane from script arguments.
 *
 * @mangled _SET_CLIP_POINT__FP12RS_STACKDATAi
 * @address 0x1931F0
 * @size 0x8C
 */
static int _SET_CLIP_POINT(RS_STACKDATA *stack, int argument_count) {
    sceVu0FVECTOR plane;
    GetPosition(stack, plane);
    stack += 3;

    if (EdEventInfo.edit_ground != NULL) {
        float        distance = GetStackFloat(stack++);
        CEditGround *ground = EdEventInfo.edit_ground;
        sceVu0CopyVector(ground->clip_plane, plane);
        ground->clip_plane[3] = distance;
    }

    return 1;
}

static int _DRAW_EDIT_WATER(RS_STACKDATA *stack, int argument_count) {
    if (EdEventInfo.edit_ground != NULL) {
        EdEventInfo.edit_ground->suppress_water = ((GetStackInt(stack) != 0) ^ 1) & 0xFF;
    }

    return 1;
}

static int _DRAW_WATER_SURFACE(RS_STACKDATA *stack, int argument_count) {
    int draw = GetStackInt(stack++);

    for (int i = 0; i < argument_count - 1; i++) {
        if (EdEventInfo.edit_ground != NULL) {
            int index = GetStackInt(stack++);

            if (index >= 0 && index < 4) {
                EdEventInfo.edit_ground->water_surfaces[index].draw = draw;
            }
        }
    }

    return 1;
}

static int _SCREEN_FILTER(RS_STACKDATA *stack, int argument_count) {
    int red = GetStackInt(stack++);

    if (red < 0) {
        EdEventInfo.screen_filter = false;
        return 1;
    }

    int green = GetStackInt(stack++);
    int blue = GetStackInt(stack++);
    int alpha = GetStackInt(stack);

    if (alpha < 0) {
        EdEventInfo.screen_filter = false;
        return 1;
    }

    EdEventInfo.screen_filter = true;
    EdEventInfo.screen_filter_color[0] = red;
    EdEventInfo.screen_filter_color[1] = green;
    EdEventInfo.screen_filter_color[2] = blue;
    EdEventInfo.screen_filter_color[3] = alpha;
    return 1;
}

static int _DRAW_THUNDER(RS_STACKDATA *stack, int argument_count) {
    EdThunderEffectFlag = GetStackInt(stack);
    return 1;
}

static int _SET_LIGHT(RS_STACKDATA *stack, int argument_count) {
    EdEventInfo.lighting_override = true;
    int index = GetStackInt(stack++);

    if (index < 0 || index > 3) {
        return 0;
    }

    sceVu0FVECTOR direction;
    direction[0] = GetStackFloat(stack++);
    direction[1] = GetStackFloat(stack++);
    direction[2] = GetStackFloat(stack++);
    direction[3] = 0.0f;
    sceVu0Normalize(direction, direction);
    EdEventInfo.light_direction[0][index] = direction[0];
    EdEventInfo.light_direction[1][index] = direction[1];
    EdEventInfo.light_direction[2][index] = direction[2];

    if (argument_count > 4) {
        EdEventInfo.light_color[index][0] = GetStackFloat(stack++);
        EdEventInfo.light_color[index][1] = GetStackFloat(stack++);
        EdEventInfo.light_color[index][2] = GetStackFloat(stack);
    }

    return 1;
}

static int _SET_LIGHT_COLOR(RS_STACKDATA *stack, int argument_count) {
    EdEventInfo.lighting_override = true;
    int index = GetStackInt(stack++);

    if (index < 0 || index > 3) {
        return 0;
    }

    EdEventInfo.light_color[index][0] = GetStackFloat(stack++);
    EdEventInfo.light_color[index][1] = GetStackFloat(stack++);
    EdEventInfo.light_color[index][2] = GetStackFloat(stack);
    return 1;
}

static int _SET_AMBIENT(RS_STACKDATA *stack, int argument_count) {
    EdEventInfo.lighting_override = true;
    EdEventInfo.ambient_color[0] = GetStackFloat(stack++);
    EdEventInfo.ambient_color[1] = GetStackFloat(stack++);
    EdEventInfo.ambient_color[2] = GetStackFloat(stack++);
    EdEventInfo.ambient_color[3] = 128.0f;

    if (argument_count > 3) {
        EdEventInfo.ambient_color[3] = GetStackFloat(stack);
    }

    return 1;
}

static int _SAVE_LIGHT(RS_STACKDATA *stack, int argument_count) {
    int slot = GetStackInt(stack);

    if (slot < 0 || slot > 1) {
        return 0;
    }

    sceVu0CopyMatrix(save_l[slot], EdEventInfo.light_direction);
    sceVu0CopyMatrix(save_c[slot], EdEventInfo.light_color);
    sceVu0CopyVector(save_a[slot], EdEventInfo.ambient_color);
    return 1;
}

static int _LOAD_LIGHT(RS_STACKDATA *stack, int argument_count) {
    int slot = GetStackInt(stack);

    if (slot < 0) {
        MGGetPLight(EdEventInfo.light_direction, EdEventInfo.light_color);
        MGGetAmbient(EdEventInfo.ambient_color);
        return 1;
    }

    if (slot > 1) {
        return 0;
    }

    sceVu0CopyMatrix(EdEventInfo.light_direction, save_l[slot]);
    sceVu0CopyMatrix(EdEventInfo.light_color, save_c[slot]);
    sceVu0CopyVector(EdEventInfo.ambient_color, save_a[slot]);
    return 1;
}

static int _SET_DOF_LEVEL(RS_STACKDATA *stack, int argument_count) {
    EdSetDOFLevel(GetStackInt(stack));
    return 1;
}

static int _SP_INIT(RS_STACKDATA *stack, int argument_count) {
    int index = GetStackInt(stack);

    if (index < 0) {
        for (int i = 0;; i++) {
            ED_SPRITE *sprite = GetSprite(i);

            if (sprite == NULL) {
                break;
            }

            InitSprite(sprite);
        }

        return 1;
    }

    ED_SPRITE *sprite = GetSprite(index);

    if (sprite == NULL) {
        return 0;
    }

    InitSprite(sprite);
    return 1;
}

static int _SP_SET_TEX(RS_STACKDATA *stack, int argument_count) {
    ED_SPRITE *sprite = GetSprite(GetStackInt(stack++));

    if (sprite == NULL) {
        return 0;
    }

    sprite->enabled = true;
    char *name = GetStackString(stack++);
    sprite->source_x = GetStackInt(stack++);
    sprite->source_y = GetStackInt(stack++);
    sprite->source_width = GetStackInt(stack++);
    sprite->source_height = GetStackInt(stack++);

    if (argument_count > 6) {
        sprite->bilinear = GetStackInt(stack);
    }

    CTexture *texture = TexManager.GetTexture(name, -1);

    if (texture == NULL) {
        return 0;
    }

    sprite->texture = *texture;
    return 1;
}

static int _SP_SET_POS(RS_STACKDATA *stack, int argument_count) {
    ED_SPRITE *sprite = GetSprite(GetStackInt(stack++));

    if (sprite == NULL) {
        return 0;
    }

    sprite->x = (float) GetStackInt(stack++);
    sprite->y = (float) GetStackInt(stack++);
    sprite->height = -1;
    sprite->width = -1;

    if (argument_count > 3) {
        sprite->width = GetStackInt(stack++);
    }

    if (argument_count > 4) {
        sprite->height = GetStackInt(stack);
    }

    return 1;
}

static int _SP_SET_RGBA(RS_STACKDATA *stack, int argument_count) {
    ED_SPRITE *sprite = GetSprite(GetStackInt(stack++));

    if (sprite == NULL) {
        return 0;
    }

    sprite->red = GetStackInt(stack++);
    sprite->green = GetStackInt(stack++);
    sprite->blue = GetStackInt(stack++);
    sprite->alpha = 128;

    if (argument_count > 4) {
        sprite->alpha = GetStackInt(stack);
    }

    return 1;
}

static int _SP_SET_ROT(RS_STACKDATA *stack, int argument_count) {
    ED_SPRITE    *sprite;
    RS_STACKDATA *arguments = (0, stack + 1);
    sprite = GetSprite(GetStackInt(stack));

    if (sprite == NULL) {
        return 0;
    }

    sprite->rotated = true;
    sprite->rotation_x = GetStackInt(arguments++);
    sprite->rotation_y = GetStackInt(arguments++);
    sprite->rotation = GetStackFloat(arguments);
    return 1;
}

static int _SP_SET_MOVE(RS_STACKDATA *stack, int argument_count) {
    RS_STACKDATA *arguments = (0, stack + 1);
    ED_SPRITE    *sprite = GetSprite(GetStackInt(stack));

    if (sprite == NULL) {
        return 0;
    }

    sprite->move_x = GetStackFloat(arguments++);
    sprite->move_y = GetStackFloat(arguments);
    return 1;
}

static int _SP_DRAW(RS_STACKDATA *stack, int argument_count) {
    int layer = GetStackInt(stack++);

    for (int i = 0; i < argument_count - 1; i++) {
        ED_SPRITE *sprite = GetSprite(GetStackInt(stack++));

        if (sprite != NULL) {
            sprite->layer = layer;
        }
    }

    return 1;
}

static int _INIT_DRAW_DAY(RS_STACKDATA *stack, int argument_count) {
    EdInitDrawDay();
    return 1;
}

static int _DRAW_DAY(RS_STACKDATA *stack, int argument_count) {
    EdStartDrawDay();
    return 1;
}

static int _MAP_TITLE_OFF(RS_STACKDATA *stack, int argument_count) {
    now_loading_off();
    return 1;
}

static int _SET_WIND(RS_STACKDATA *stack, int argument_count) {
    EdEventInfo.wind[0] = GetStackFloat(stack++);
    EdEventInfo.wind[1] = GetStackFloat(stack++);
    EdEventInfo.wind[2] = GetStackFloat(stack++);
    EdEventInfo.wind[3] = GetStackFloat(stack);
    return 1;
}

static int _ASQ_INIT(RS_STACKDATA *stack, int argument_count) {
    int index = GetStackInt(stack);

    if (index < 0) {
        for (int i = 0; i < 10; i++) {
            CActionSeq *sequence = GetActSeq(index);

            if (sequence != NULL) {
                sequence->ClearSeq();
            }
        }

        return 1;
    }

    CActionSeq *sequence = GetActSeq(index);

    if (sequence == NULL) {
        return 0;
    }

    sequence->ClearSeq();
    return 1;
}

int _ASQ_SYNC_CHARA(RS_STACKDATA *stack, int argument_count) {
    int         sequence_index = GetStackInt(stack++);
    int         character_index = GetStackInt(stack);
    CActionSeq *sequence = GetActSeq(sequence_index);

    if (sequence == NULL) {
        return 0;
    }

    CCharacter *character = GetNPC(character_index);

    if (character_index == -1) {
        character = EdEventInfo.main_character;
    }

    if (character == NULL) {
        return 0;
    }

    sequence->SyncChara(character);
    return 1;
}

static int _ASQ_SET_POS(RS_STACKDATA *stack, int argument_count) {
    RS_STACKDATA *arguments = (0, stack + 1);
    CActionSeq   *sequence = GetActSeq(GetStackInt(stack));

    if (sequence == NULL) {
        return 0;
    }

    sceVu0FVECTOR position;
    GetPosition(arguments, position);
    sequence->SetPos(position);
    return 1;
}

static int _ASQ_MOVE(RS_STACKDATA *stack, int argument_count) {
    sceVu0FVECTOR position;
    CActionSeq   *sequence = GetActSeq(GetStackInt(stack++));

    if (sequence == NULL) {
        return 0;
    }

    if (argument_count == 5) {
        GetPosition(stack, position);
        stack += 3;
        sequence->MoveSeq(position, GetStackInt(stack++));
    }

    if (argument_count == 2) {
        sequence->MoveSeq(GetStackInt(stack));
    }

    return 1;
}

int _ASQ_MOVE_STEP(RS_STACKDATA *stack, int argument_count) {
    CActionSeq *sequence = GetActSeq(GetStackInt(stack++));

    if (sequence == NULL) {
        return 0;
    }

    sceVu0FVECTOR position;
    GetPosition(stack, position);
    sequence->MoveSeq(position, GetStackFloat(stack += 3));
    return 1;
}

int _ASQ_ROT_REF(RS_STACKDATA *stack, int argument_count) {
    CActionSeq *sequence = GetActSeq(GetStackInt(stack++));

    if (sequence == NULL) {
        return 0;
    }

    sceVu0FVECTOR position;
    GetPosition(stack, position);
    sequence->RotRefSeq(position, GetStackFloat(stack += 3));
    return 1;
}

static int _ASQ_ROT_ANGLE(RS_STACKDATA *stack, int argument_count) {
    sceVu0FVECTOR rotation;
    CActionSeq   *sequence = GetActSeq(GetStackInt(stack++));

    if (sequence == NULL) {
        return 0;
    }

    rotation[3] = 0.0f;
    rotation[2] = 0.0f;
    rotation[0] = 0.0f;
    rotation[1] = GetStackFloat(stack++);
    GetWorldRot(rotation, rotation);
    sequence->RotAngleSeq(rotation[1], GetStackFloat(stack));
    return 1;
}

static int _ASQ_CLEAR_ROT(RS_STACKDATA *stack, int argument_count) {
    CActionSeq *sequence = GetActSeq(GetStackInt(stack));

    if (sequence == NULL) {
        return 0;
    }

    sequence->ClearRotSeq();
    return 1;
}

static int _ASQ_WAIT_ROT(RS_STACKDATA *stack, int argument_count) {
    CActionSeq *sequence = GetActSeq(GetStackInt(stack));

    if (sequence == NULL) {
        return 0;
    }

    sequence->WaitRotSeq();
    return 1;
}

static int _ASQ_ROT_MOVE(RS_STACKDATA *stack, int argument_count) {
    RS_STACKDATA *next = (0, stack + 1);
    CActionSeq   *sequence = GetActSeq(GetStackInt(stack));

    if (sequence == NULL) {
        return 0;
    }

    sequence->RotMoveSeq(GetStackFloat(next));
    return 1;
}

static int _ASQ_SET_ROT(RS_STACKDATA *stack, int argument_count) {
    sceVu0FVECTOR rotation;
    CActionSeq   *sequence = GetActSeq(GetStackInt(stack++));

    if (sequence == NULL) {
        return 0;
    }

    if (argument_count == 4) {
        GetRotation(stack, rotation);
        stack += 3;
    }

    if (argument_count == 2) {
        rotation[2] = 0.0f;
        rotation[0] = 0.0f;
        rotation[1] = GetStackFloat(stack);
        GetWorldRot(rotation, rotation);
    }

    sequence->SetRot(rotation);
    return 1;
}

static int _ASQ_DELAY_ROT(RS_STACKDATA *stack, int argument_count) {
    RS_STACKDATA *next = (0, stack + 1);
    CActionSeq   *sequence = GetActSeq(GetStackInt(stack));

    if (sequence == NULL) {
        return 0;
    }

    sequence->SetDelayRot(GetStackInt(next));
    return 1;
}

static int _ASQ_MOTION_TRG(RS_STACKDATA *stack, int argument_count) {
    CActionSeq *sequence = GetActSeq(GetStackInt(stack++));

    if (sequence == NULL) {
        return 0;
    }

    int trigger = 0;

    if (argument_count == 2) {
        trigger = GetStackInt(stack);
    }

    sequence->MotionTrg(trigger);
    return 1;
}

static int asq_motion_play(int mode, RS_STACKDATA *stack, int argument_count) {
    CActionSeq *sequence = GetActSeq(GetStackInt(stack++));

    if (sequence == NULL) {
        return 0;
    }

    int   motion = GetStackInt(stack++);
    float speed = -1.0f;

    if (argument_count >= 3) {
        speed = GetStackFloat(stack++);
    }

    int flags = 0;

    if (argument_count >= 4) {
        flags = (int) GetStackFloat(stack);
    }

    flags &= 4;
    sequence->MotionSeq(motion, mode, speed, flags);
    return 1;
}

static int _ASQ_MOTION_PLAY(RS_STACKDATA *stack, int argument_count) {
    asq_motion_play(5, stack, argument_count);
}

static int _ASQ_MOTION_STOP(RS_STACKDATA *stack, int argument_count) {
    asq_motion_play(6, stack, argument_count);
}

static int _ASQ_MOTION_NEXT(RS_STACKDATA *stack, int argument_count) {
    asq_motion_play(7, stack, argument_count);
}

static int _ASQ_ANIME_TRG(RS_STACKDATA *stack, int argument_count) {
    CActionSeq *sequence = GetActSeq(GetStackInt(stack++));

    if (sequence == NULL) {
        return 0;
    }

    int trigger = 0;

    if (argument_count == 2) {
        trigger = GetStackInt(stack);
    }

    sequence->AnimeTrg(trigger);
    return 1;
}

static int _ASQ_ANIME(RS_STACKDATA *stack, int argument_count) {
    CActionSeq *sequence = GetActSeq(GetStackInt(stack++));

    if (sequence == NULL) {
        return 0;
    }

    int enable = true;
    int start = 0;
    int end = 0;
    int animation = GetStackInt(stack++);

    if (argument_count >= 3) {
        enable = GetStackInt(stack++);
    }

    if (argument_count >= 4) {
        start = GetStackInt(stack++);
    }

    if (argument_count >= 5) {
        end = GetStackInt(stack);
    }

    sequence->AnimeSeq(animation, enable, start, end);
    return 1;
}

/**
 * Pushes whether an editor action-sequence slot is still running onto the script stack.
 *
 * @mangled _ASQ_CHECK__FP12RS_STACKDATAi
 * @address 0x194B70
 * @size 0x74
 */
static int _ASQ_CHECK(RS_STACKDATA *stack, int argument_count) {
    CActionSeq *sequence = GetActSeq(GetStackInt(stack++));

    if (sequence == NULL) {
        return 0;
    }

    int end = sequence->CheckEnd();
    SetStack(stack++, ((end != 0) ^ 1) & 0xFF);
    return 1;
}

static int _OBJ_ANIME_INIT(RS_STACKDATA *stack, int argument_count) {
    for (int i = 0; i < argument_count; i++) {
        ClearObjAnime(GetStackInt(stack++));
    }

    return 1;
}

/**
 * Starts an object animation with up to three rotation keys, converting radians to degrees.
 *
 * @mangled _OBJ_ANIME__FP12RS_STACKDATAi
 * @address 0x194C70
 * @size 0x29C
 */
static int _OBJ_ANIME(RS_STACKDATA *stack, int argument_count) {
    int           anime_index = GetStackInt(stack++);
    int           handle_index = GetStackInt(stack++);
    int           angle_mode = GetStackInt(stack++);
    int           playback_mode = GetStackInt(stack++);
    sceVu0FVECTOR offset = {0.0f, 0.0f, 0.0f, 0.0f};
    sceVu0FVECTOR range;
    sceVu0FVECTOR speed;
    sceVu0CopyVector(range, offset);
    sceVu0CopyVector(speed, offset);

    offset[0] = GetStackFloat(stack++);
    offset[1] = GetStackFloat(stack++);
    offset[2] = GetStackFloat(stack++);

    if (angle_mode == 0) {
        offset[0] = 180.0f * offset[0] / PI_SHORT;
        offset[1] = 180.0f * offset[1] / PI_SHORT;
        offset[2] = 180.0f * offset[2] / PI_SHORT;
    }

    if (argument_count >= 10) {
        range[0] = GetStackFloat(stack++);
        range[1] = GetStackFloat(stack++);
        range[2] = GetStackFloat(stack++);

        if (angle_mode == 0) {
            range[0] = 180.0f * range[0] / PI_SHORT;
            range[1] = 180.0f * range[1] / PI_SHORT;
            range[2] = 180.0f * range[2] / PI_SHORT;
        }
    }

    if (argument_count >= 13) {
        speed[0] = GetStackFloat(stack++);
        speed[1] = GetStackFloat(stack++);
        speed[2] = GetStackFloat(stack);

        if (angle_mode == 0) {
            speed[0] = 180.0f * speed[0] / PI_SHORT;
            speed[1] = 180.0f * speed[1] / PI_SHORT;
            speed[2] = 180.0f * speed[2] / PI_SHORT;
        }
    }

    init_obj_anime(anime_index, handle_index, angle_mode, playback_mode, offset, range, speed);
    return 1;
}

static int _SSET_GAME_FLAG(RS_STACKDATA *stack, int argument_count) {
    if (argument_count != 2) {
        return 0;
    }

    RS_STACKDATA *next = (0, stack + 1);
    int           flag = GetStackInt(stack);
    SaveData->SetGameFlag(flag, GetStackInt(next));
    return 1;
}

static int _SGET_GAME_FLAG(RS_STACKDATA *stack, int argument_count) {
    if (argument_count != 2) {
        return 0;
    }

    RS_STACKDATA *result = (0, stack + 1);
    int           flag = GetStackInt(stack);
    SetStack(result, SaveData->GetGameFlag(flag));
    return 1;
}

static int _SSET_GAME_INT_FLAG(RS_STACKDATA *stack, int argument_count) {
    if (argument_count != 2) {
        return 0;
    }

    RS_STACKDATA *next = (0, stack + 1);
    int           flag = GetStackInt(stack);
    SaveData->SetGameIntFlag(flag, GetStackInt(next));
    return 1;
}

static int _SGET_GAME_INT_FLAG(RS_STACKDATA *stack, int argument_count) {
    if (argument_count != 2) {
        return 0;
    }

    RS_STACKDATA *result = (0, stack + 1);
    int           flag = GetStackInt(stack);
    SetStack(result, SaveData->GetGameIntFlag(flag));
    return 1;
}

static int _SITEM_GET(RS_STACKDATA *stack, int argument_count) {
    int attachment = -1;
    int item = GetStackInt(stack);

    if (GetAddAttachItem(item) != 0) {
        attachment = 3;
    }

    EdGetItem(item, 1, attachment);
    return 1;
}

static int _SSET_MAP_FLAG(RS_STACKDATA *stack, int argument_count) {
    RS_STACKDATA *next = (0, stack + 1);
    int           flag = GetStackInt(stack);
    EdSetMapFlag(flag, ((GetStackInt(next) != 0) ^ 1) & 0xFF);
    return 1;
}

static int _SGET_MAP_FLAG(RS_STACKDATA *stack, int argument_count) {
    RS_STACKDATA *result = (0, stack + 1);
    int           flag = GetStackInt(stack);
    SetStack(result, ((EdGetMapFlag(flag) != 0) ^ 1) & 0xFF);
    return 1;
}

static int _SET_WORK_FLAG(RS_STACKDATA *stack, int argument_count) {
    RS_STACKDATA *next = (0, stack + 1);
    int           flag = GetStackInt(stack);
    SetWorkFlag(flag, GetStackInt(next));
}

static int _GET_WORK_FLAG(RS_STACKDATA *stack, int argument_count) {
    RS_STACKDATA *result = (0, stack + 1);
    int           flag = GetStackInt(stack);
    SetStack(result, (0, GetWorkFlag(flag)));
    return 1;
}

static int _SITEM_CHECK(RS_STACKDATA *stack, int argument_count) {
    RS_STACKDATA *result = (0, stack + 1);
    int           item = GetStackInt(stack);
    SetStack(result, EdCheckItem(item));
    return 1;
}

static int _SITEM_CHECK_ALL(RS_STACKDATA *stack, int argument_count) {
    RS_STACKDATA *result = (0, stack + 1);
    int           item = GetStackInt(stack);
    SetStack(result, PlayerAllItemCheck(item));
    return 1;
}

static int _SGET_DUNGEON_STATUS(RS_STACKDATA *stack, int argument_count) {
    CDngStatusData *status = SaveData->GetDngStatus();
    int             dungeon = GetStackInt(stack++);

    if (dungeon < 0 || dungeon >= 6) {
        return 0;
    }

    SetStack(stack++, SaveData->QuestDungeon(dungeon, 0));
    SetStack(stack, status->floor_reached[dungeon]);
    return 1;
}

/**
 * Writes the current active-party count to an event-script stack reference.
 */
static int _SGET_PARTY_NUM(RS_STACKDATA *stack, int argument_count) {
    CDngStatusData *status = SaveData->GetDngStatus();
    SetStack(stack++, status->party_size);
    return 1;
}

static int _SSET_PARTY_NUM(RS_STACKDATA *stack, int argument_count) {
    CDngStatusData *status = SaveData->GetDngStatus();
    int             party_size = GetStackInt(stack);
    status->SetPartySize(party_size);
    return 1;
}

static int _SSET_REQUEST_EVENT_FLAG(RS_STACKDATA *stack, int argument_count) {
    RS_STACKDATA    *next = (0, stack + 1);
    SV_GEORAMA_DATA *georama = SaveData->GetGrdData(GetStackInt(stack) - 1);

    if (georama == NULL) {
        return 0;
    }

    georama->request_event_flag = GetStackInt(next);
    return 1;
}

/**
 * Updates a map's visit count and optionally returns the resulting count to the script.
 *
 * @mangled _SADD_VISIT_MAP__FP12RS_STACKDATAi
 * @address 0x195540
 * @size 0xC4
 */
static int _SADD_VISIT_MAP(RS_STACKDATA *stack, int argument_count) {
    int map = GetStackInt(stack++) - 1;
    int add = 1;

    if (argument_count > 1) {
        add = GetStackInt(stack++);
    }

    int visits = map < 200 ? SaveData->VisitMap(map, add) : SaveData->QuestDungeon(map, add);

    if (argument_count > 2) {
        SetStack(stack, visits);
    }

    return 1;
}

static int _SSKILL_GET(RS_STACKDATA *stack, int argument_count) {
    CDngStatusData *status = SaveData->GetDngStatus();
    int             character = GetStackInt(stack);

    if (character < 0 || character >= 6) {
        return 0;
    }

    status->skill_owned[character] = 1;
    return 1;
}

static int _SGET_EQUIP_WEAPON(RS_STACKDATA *stack, int argument_count) {
    CDngStatusData *status = SaveData->GetDngStatus();
    int             character = GetStackInt(stack++);

    if (character < 0 || character >= 6) {
        return 0;
    }

    SetStack(stack, TransWepNoNewToOld(status->chara_weapons[character][status->equipped_weapon_slot[character]].item_no));
    return 1;
}

static int _SADD_MAXITEM(RS_STACKDATA *stack, int argument_count) {
    EdAddMaxItem(GetStackInt(stack));
    return 1;
}

static int _SITEM_LOST(RS_STACKDATA *stack, int argument_count) {
    CDngStatusData *status = SaveData->GetDngStatus();
    status->LostItem(GetStackInt(stack));
    return 1;
}

static int _SATRA_PARTS_GET(RS_STACKDATA *stack, int argument_count) {
    RS_STACKDATA *next = (0, stack + 1);
    int           georama = GetStackInt(stack) - 1;
    SaveData->AtraPartsGet(georama, GetStackInt(next));
}

static int _SATRA_CHIP_GET(RS_STACKDATA *stack, int argument_count) {
    RS_STACKDATA *next = (0, stack + 1);
    int           georama = GetStackInt(stack) - 1;
    SaveData->AtraChipGet(georama, GetStackInt(next));
}

int _SGET_REQUEST(RS_STACKDATA *stack, int argument_count) {
    RS_STACKDATA    *result = (0, stack + 1);
    SV_GEORAMA_DATA *georama = SaveData->GetGrdData(GetStackInt(stack) - 1);

    if (georama == NULL) {
        SetStack(result, 0);
        return 0;
    }

    int i;
    int completed = 0;
    i = 0;

    while (i < 24) {
        if (georama->request_complete[i] != 0) {
            completed++;
        }

        i++;
    }

    int total = georama->request_count;

    if (completed == total) {
        SetStack(result, 100);
    } else {
        SetStack(result++, completed * 100 / total);
    }

    return 1;
}

static int _SGET_ATRA_PARTS_NUM(RS_STACKDATA *stack, int argument_count) {
    int           count = 0;
    RS_STACKDATA *result = (0, stack + 1);
    int           georama = GetStackInt(stack) - 1;

    for (int i = 0; i < 24; i++) {
        SV_EDIT_PARTS_INFO *part = SaveData->GetEditPartsInfo(georama, i);

        if (part != NULL && part->obtained != 0) {
            count++;
        }
    }

    SetStack(result, count);
    return 1;
}

static int _SGET_DAY(RS_STACKDATA *stack, int argument_count) {
    SetStack(stack, SaveData->GetDay());
    return 1;
}

static int _SSET_TIME(RS_STACKDATA *stack, int argument_count) {
    SaveData->SetNowTime(GetStackFloat(stack));
    return 1;
}

static int _SGET_TIME(RS_STACKDATA *stack, int argument_count) {
    SetStack(stack, SaveData->GetNowTime());
    return 1;
}

static int _SADD_TIME(RS_STACKDATA *stack, int argument_count) {
    SaveData->AddNowTime(GetStackFloat(stack));
    return 1;
}

static int _SEQUIP_DEFAULT_WEAPON(RS_STACKDATA *stack, int argument_count) {
    EquipDefaultWeapon(GetStackInt(stack));
    return 1;
}

static int _LOAD_SND_SYNC(RS_STACKDATA *stack, int argument_count) {
    SetStack(stack, SndSyncBG());
    return 1;
}

static int _LOAD_SOUND_SET(RS_STACKDATA *stack, int argument_count) {
    StartReadBG();
    SndSoundLoadBG(GetStackInt(stack), read_buffer, NULL);
}

static int _LOAD_VOICE_SET(RS_STACKDATA *stack, int argument_count) {
    StartReadBG();
    SndVoiceLoadBG(GetStackInt(stack), read_buffer, NULL);
}

static int _LOAD_BGM(RS_STACKDATA *stack, int argument_count) {
    StartReadBG();
    SndBgmLoadBG(GetStackInt(stack), read_buffer, NULL);
}

static int _DELETE_BGM(RS_STACKDATA *stack, int argument_count) {
    SndBgmStop();
    SndBgmInit();
    return 1;
}

static int _PLAY_BGM(RS_STACKDATA *stack, int argument_count) {
    SndBgmPlay(GetStackInt(stack));
    return 1;
}

static int _STOP_BGM(RS_STACKDATA *stack, int argument_count) {
    SndBgmStop();
    return 1;
}

static int _SET_BGM_VOL(RS_STACKDATA *stack, int argument_count) {
    SndSetBgmVol(GetStackInt(stack));
    return 1;
}

static int _SET_BGM_VOLF(RS_STACKDATA *stack, int argument_count) {
    SndSetBgmVolf(GetStackFloat(stack));
    return 1;
}

static int _GET_BGM_VOL(RS_STACKDATA *stack, int argument_count) {
    SetStack(stack, SndGetBgmVol());
    return 1;
}

static int _GET_BGM_NO(RS_STACKDATA *stack, int argument_count) {
    SetStack(stack, SndGetBgmNo());
    return 1;
}

static int _SET_OUT_BGM_NO(RS_STACKDATA *stack, int argument_count) {
    EdBeforeInBgmNo = GetStackInt(stack);
    return 1;
}

static int _GET_OUT_BGM_NO(RS_STACKDATA *stack, int argument_count) {
    SetStack(stack, EdBeforeInBgmNo);
    return 1;
}

static int _BGM_FADE_IN(RS_STACKDATA *stack, int argument_count) {
    int bgm = GetStackInt(stack++);
    int frames = -1;
    int mode = 0;

    if (argument_count > 1) {
        frames = GetStackInt(stack++);
    }

    if (argument_count > 2) {
        mode = GetStackInt(stack);
    }

    SndBgmFadeIn(bgm, frames, mode);
    return 1;
}

static int _BGM_FADE_OUT(RS_STACKDATA *stack, int argument_count) {
    int bgm = GetStackInt(stack++);
    int frames = 0;

    if (argument_count > 1) {
        frames = GetStackInt(stack);
    }

    SndBgmFadeOut(bgm, frames);
    return 1;
}

static int _CHECK_BGM_FADE(RS_STACKDATA *stack, int argument_count) {
    SetStack(stack, SndCheckFade());
    return 1;
}

static int _PLAY_AMB_BGM(RS_STACKDATA *stack, int argument_count) {
    SndAmbientPlay(GetStackInt(stack));
    return 1;
}

static int _STOP_AMB_BGM(RS_STACKDATA *stack, int argument_count) {
    SndAmbientStop();
    return 1;
}

static int _SET_AMB_VOLF(RS_STACKDATA *stack, int argument_count) {
    SndAmbientSetVolf(GetStackFloat(stack));
    return 1;
}

static int _PLAY_SE(RS_STACKDATA *stack, int argument_count) {
    int sound = GetStackInt(stack++);

    if (argument_count >= 4) {
        float position[4];
        GetPosition(stack, position);
        stack += 3;
        float near = -1.0f;
        float far = -1.0f;

        if (argument_count >= 5) {
            near = GetStackFloat(stack++);
        }

        if (argument_count >= 6) {
            far = GetStackFloat(stack);
        }

        SndSePlay(sound, position, near, far);
    } else {
        int voice = -1;

        if (argument_count == 2) {
            voice = GetStackInt(stack);
        }

        SndSePlay(sound, voice, 0);
    }

    return 1;
}

static int _STOP_SE(RS_STACKDATA *stack, int argument_count) {
    SndSeStop(GetStackInt(stack), 0);
    return 1;
}

static int _SET_SE_VOL(RS_STACKDATA *stack, int argument_count) {
    RS_STACKDATA *next = (0, stack + 1);
    int           sound = GetStackInt(stack);
    SndSetSeVol(sound, GetStackInt(next), 0);
    return 1;
}

static int _LOAD_SPECIAL_SE(RS_STACKDATA *stack, int argument_count) {
    StartReadBG();
    SndSPSeLoadBG(GetStackInt(stack), read_buffer, NULL);
    return 1;
}

static int _PLAY_SPECIAL_SE(RS_STACKDATA *stack, int argument_count) {
    int sound = GetStackInt(stack++);
    int voice = -1;

    if (argument_count >= 2) {
        voice = GetStackInt(stack);
    }

    SndSPSePlay(sound, voice);
    return 1;
}

static int _STOP_SPECIAL_SE(RS_STACKDATA *stack, int argument_count) {
    SndSPSeStop(GetStackInt(stack));
    return 1;
}

static int _SET_SPECIAL_SE_VOL(RS_STACKDATA *stack, int argument_count) {
    int   sound = GetStackInt(stack++);
    float position[4];
    GetPosition(stack, position);
    stack += 3;
    float near = 10.0f;
    float far = 500.0f;

    if (argument_count >= 5) {
        near = GetStackFloat(stack++);
    }

    if (argument_count >= 6) {
        far = GetStackFloat(stack);
    }

    float volume;
    float pan;
    SndGetVolPan(&volume, &pan, position, near, far);
    SndSetSPSeVolf(sound, volume);
    SndSetSPSePanf(sound, pan);
    return 1;
}

static int _SOUND_OFF_COUNT(RS_STACKDATA *stack, int argument_count) {
    EdEventInfo.sound_off_count = GetStackInt(stack);

    if (EdEventInfo.sound_off_count > 10) {
        EdEventInfo.sound_off_count = 10;
    }

    EdSetSoundOffCount(EdEventInfo.sound_off_count);
    return 1;
}

static int _GET_V_ARG(RS_STACKDATA *stack, int argument_count) {
    int index = GetStackInt(stack++);

    if (index < 0 || index >= 4) {
        return 0;
    }

    for (int i = 0; i < argument_count - 1; i++) {
        SetStack(stack++, EdEventInfo.vector_arguments[index][i]);
    }

    return 1;
}

static int _GET_I_ARG(RS_STACKDATA *stack, int argument_count) {
    int index = GetStackInt(stack++);

    if (index < 0 || index >= 8) {
        return 0;
    }

    SetStack(stack++, EdEventInfo.integer_arguments[index]);
    return 1;
}

static int _GET_F_ARG(RS_STACKDATA *stack, int argument_count) {
    int index = GetStackInt(stack++);

    if (index < 0 || index >= 8) {
        return 0;
    }

    SetStack(stack++, EdEventInfo.flag_arguments[index]);
    return 1;
}

static int _EB_INIT(RS_STACKDATA *stack, int argument_count) {
    EBInit(GetStackFloat(stack));
    return 1;
}

static int _EB_SET_MOTION(RS_STACKDATA *stack, int argument_count) {
    int motions[128];

    if (argument_count < 2) {
        return 0;
    }

    CCharacter *character = GetChara(GetStackInt(stack++));
    int         i = 0;

    while (i < argument_count - 1) {
        motions[i++] = GetStackInt(stack++);
    }

    motions[i] = -1;
    EBSetMotion(character, motions);
    return 1;
}

static int _EB_SET_KEY(RS_STACKDATA *stack, int argument_count) {
    float time = GetStackFloat(stack++);
    int   key = GetStackInt(stack++);
    int   mode = 0;

    if (argument_count >= 3) {
        mode = GetStackInt(stack);
    }

    EBSetKey(time, key, mode);
    return 1;
}

static int _EB_LOOP(RS_STACKDATA *stack, int argument_count) {
    SetStack(stack, EBLoop());
    return 1;
}

static int _EB_INTRO_START(RS_STACKDATA *stack, int argument_count) {
#ifdef PAL
    EBInitialize();
#endif
    EBInitIntro();
    return 1;
}

static int _EB_INTRO_LOOP(RS_STACKDATA *stack, int argument_count) {
    SetStack(stack, EBIntroLoop());
    return 1;
}

static int _EB_INTRO_END(RS_STACKDATA *stack, int argument_count) {
    EBExit();
    return 1;
}

static int _EB_DEBUG(RS_STACKDATA *stack, int argument_count) {
    EBDebug(GetStackInt(stack));
    return 1;
}

static int _EB_FINISH_SOUND(RS_STACKDATA *stack, int argument_count) {
    RS_STACKDATA *next = (0, stack + 1);
    int           fade_bgm = GetStackInt(stack);
    EBFinishSound(fade_bgm, GetStackInt(next));
    return 1;
}

static int _LOAD_MAIN_CHARA(RS_STACKDATA *stack, int argument_count) {
    char           *model = GetStackString(stack++);
    char           *motion = GetStackString(stack++);
    int             use_villager_arena = GetStackInt(stack);
    CDataAlloc2<1> *arena = NULL;

    if (use_villager_arena == 1) {
        arena = &EdVillagerBuffer;
    }

    EdLoadMainChara(model, motion, arena);
    return 1;
}

static int _LOAD_FISHING_DATA(RS_STACKDATA *stack, int argument_count) {
    int fish_set = GetStackInt(stack++);
    FishingInit();
    FishingLoad(&EdVillagerBuffer, 8);
    FishingLoadFish(fish_set, &EdVillagerBuffer, 54);

    CBoxVu0 bounds;
    bounds.min[0] = GetStackFloat(stack++);
    bounds.min[1] = -1000.0f;
    bounds.min[2] = GetStackFloat(stack++);
    bounds.max[0] = GetStackFloat(stack++);
    bounds.max[1] = 1000.0f;
    bounds.max[2] = GetStackFloat(stack++);
    FishingSetRect(bounds);
    float water_near = GetStackFloat(stack++);
    FishingSetWaterLevel(water_near, GetStackFloat(stack));

    CCPoly polygons[1024];
    int    polygon_count = 0;

    if (EdEventInfo.edit_ground != NULL) {
        polygon_count = EdEventInfo.edit_ground->PickUpPoly(polygons, bounds, 0);
        printf("poly_num = %d\n", polygon_count);

        if (polygon_count > 1024) {
            printf("CPOLY OVER *****************\n");

            while (1) {
            }
        }
    }

    FishingSetCPoly(polygons, polygon_count);
    return 1;
}

/**
 * Initializes the fishing line at the player's rod and requests fishing mode.
 */
/** Homogeneous origin used to resolve the fishing rod's world position. */
static sceVu0FVECTOR fishing_line_origin = {0.0f, 0.0f, 0.0f, 1.0f};

static int _GOTO_FISHING(RS_STACKDATA *stack, int argument_count) {
    CCharacter *character = GetChara(-1);

    if (character == NULL) {
        return 0;
    }

    sceVu0FVECTOR position;
    character->GetPosition(position);

    if (character->frame != NULL) {
        /** Homogeneous origin copied into the aligned stack work vector. */
        struct FishingOrigin {
            sceVu0FVECTOR value; /**< Homogeneous origin vector. */
        };

        FishingOrigin origin = *(FishingOrigin *) fishing_line_origin;
        CFrame       *rod = character->frame->SearchFrame("sao");

        if (rod != NULL) {
            rod->GetWorldPosition(position, origin.value);
        }

        FishLineInit(position);
    }

    EdEventInfo.return_code = ED_EVENT_RETURN_FISHING;
    return 1;
}

static int _INIT_FISH(RS_STACKDATA *stack, int argument_count) {
    CBoxVu0 bounds;
    bounds.min[0] = GetStackFloat(stack++);
    bounds.min[1] = 0.0f;
    bounds.min[2] = GetStackFloat(stack++);
    bounds.max[0] = GetStackFloat(stack++);
    // Retail redundantly clears the first bound's Y coordinate here.
    bounds.min[1] = 0.0f;
    bounds.max[2] = GetStackFloat(stack);
    FishingInitFish(bounds);
    return 1;
}

static int _EXIT_FISHING(RS_STACKDATA *stack, int argument_count) {
    FishingExit();
    return 1;
}

static int _SET_FISHING_ESA(RS_STACKDATA *stack, int argument_count) {
    int item = GetStackInt(stack);
    FishingLoadEsa(item, EdEventInfo.item_frame[0], 40);
    return 1;
}

#ifdef PAL
static int _GET_TV_MODE(RS_STACKDATA *stack, int argument_count) {
    SetStack(stack, 1);
    return 1;
}

static int _GET_LANG_CODE(RS_STACKDATA *stack, int argument_count) {
    SetStack(stack, LanguageCode);
    return 1;
}
#endif

/** Retail's ordered registry of editor-event external functions. */
static ED_EVENT_EXTERNAL_FUNCTION ext_func_info[] = {
    {_GET_PADON,                0   },
    {_GET_PADDOWN,              1   },
    {_GET_PADUP,                2   },
    {_GET_APAD,                 903 },
    {_SET_RETURN_CODE,          3   },
    {_GET_RANDOM,               4   },
    {_NEXT_EVENT,               5   },
    {_GOTO_INTERIOR,            6   },
    {_SET_WORLD_COORD,          7   },
    {_INITIALIZE,               8   },
    {_EXIT_CODE,                9   },
    {_DRAW_EXCLAMATION_MARK,    10  },
    {_GOTO_USE_ITEM,            11  },
    {_SKIP,                     12  },
    {_GOTO_OUTSIDE,             13  },
    {_FINISH,                   14  },
    {_GET_OLD_MAPNO,            16  },
    {_MAP_JUMP,                 15  },
    {_SET_DUNGEON_FLOOR,        21  },
    {_GET_DUNGEON_FLOOR,        22  },
    {_FADEOUT_TO_EVENT,         17  },
    {_NAME_REGISTRY,            18  },
    {_WORLD_MAP,                20  },
    {_MAP_JUMP_BGM_STOP,        23  },
    {_MAP_JUMP_BGM_PLAY,        27  },
    {_GOTO_FP_CHANGE,           24  },
    {_GOTO_CHANGE_ESA,          25  },
    {_GOTO_FISH_RANKING,        26  },
    {_SET_CURRENT_DIR,          32  },
    {_ACTIVE_FILE_BUFFER,       44  },
    {_LOAD_CHR_FILE,            33  },
    {_LOAD_SYNC,                34  },
    {_LOAD_CHARA,               35  },
    {_DELETE_CHARA,             47  },
    {_INIT_CHARA,               48  },
    {_LOAD_SPRITE_TEXTURE,      56  },
    {_LOAD_BG_SPRITE_TEXTURE,   72  },
    {_LOAD_TEXTURE,             74  },
    {_LOAD_CHARA_TEXTURE,       36  },
    {_LOAD_IN_VILLAGER,         37  },
    {_LOAD_OUT_VILLAGER,        71  },
    {_LOAD_VILLAGER,            57  },
    {_CHECK_IN_VILLAGER,        58  },
    {_APPEAR_VILLAGER_ON,       59  },
    {_APPEAR_VILLAGER_OFF,      60  },
    {_APPEAR_VILLAGER_OUT,      61  },
    {_APPEAR_VILLAGER_IN,       62  },
    {_APPEAR_VILLAGER_MOVE,     70  },
    {_CLEAR_VILLAGER_BUFF,      38  },
    {_CLEAR_EVENT_BUFF,         39  },
    {_CLEAR_EVENT_EXBUFF,       40  },
    {_SYNC_SCENE_CHARA,         42  },
    {_LOAD_SCENE,               41  },
    {_SYNC_SCENE_CAMERA,        43  },
    {_RELEASE_SCENE_CHARA,      45  },
    {_RELEASE_SCENE_CAMERA,     46  },
    {_SET_SCENE_POS,            73  },
    {_SET_SCENE_ROT,            75  },
    {_LOAD_ITEM_FILE,           49  },
    {_LOAD_ITEM,                50  },
    {_SYNC_CHARA_ITEM,          51  },
    {_RELEASE_CHARA_ITEM,       52  },
    {_DELETE_ITEM,              53  },
    {_SYNC_CHARA_CHARA,         54  },
    {_RELEASE_CHARA_CHARA,      55  },
    {_GET_CHARA_POS,            64  },
    {_GET_CHARA_TALK_POS,       65  },
    {_TURN_CHARA,               66  },
    {_SET_CHARA_POS,            67  },
    {_SET_CHARA_ROT,            68  },
    {_GET_CHARA_ROT,            69  },
    {_GET_NPC_TALK_POS,         130 },
    {_GET_NPC_POS,              131 },
    {_GET_NPC_PARTS_NO,         132 },
    {_SET_NPC_MOTION,           133 },
    {_SGET_NPC_TALK_MES,        134 },
    {_SSET_NPC_TALK_MES,        135 },
    {_TURN_NPC,                 136 },
    {_SET_NPC_POS,              137 },
    {_SET_NPC_ROT,              138 },
    {_GET_NPC_ROT,              139 },
    {_SET_NPC_SCALE,            147 },
    {_GET_NPC_SCALE,            148 },
    {_NPC_POS_INIT,             153 },
    {_NPC_DRAW,                 140 },
    {_NPC_STEP,                 141 },
    {_NPC_COL,                  142 },
    {_INIT_NPC_CLOTH,           143 },
    {_NPC_CLOTH_STEP,           160 },
    {_NPC_DRAW_SHADOW,          144 },
    {_SET_NPC_ANIME,            145 },
    {_SET_NPC_AMBIENT,          146 },
    {_SET_NPC_FOOT_SOUND,       149 },
    {_SET_NPC_FLOOR_ID,         152 },
    {_NPC_STOP,                 150 },
    {_NPC_DRAW_BEFORE,          151 },
    {_SET_NPC_BODY_SIZE,        154 },
    {_GET_NPC_BODY_SIZE,        155 },
    {_NPC_CLOTH_FLOOR,          156 },
    {_NPC_PLIGHT_INIT,          158 },
    {_SET_NPC_PLIGHT,           159 },
    {_MES_MAKE,                 192 },
    {_MES_CLOSE,                193 },
    {_MES_NEXTPAGE,             194 },
    {_SET_MES_AUTOSET,          195 },
    {_SET_MES_SHIPPO,           196 },
    {_SET_MES_POS,              197 },
    {_SET_MES_DRAWSPEED,        198 },
    {_SET_MES_CURSOR,           199 },
    {_SET_MES_OKURI,            203 },
    {_SET_MES_FUKIDASHI,        204 },
    {_CHECK_MES_COMPLETE,       200 },
    {_CHECK_MES_WAIT,           201 },
    {_CHECK_MES,                202 },
    {_ITEM_GET_MES,             206 },
    {_INIT_SYS_MES,             207 },
    {_SKILL_GET_MES,            208 },
    {_SET_MES_WIN_FLAG,         205 },
    {_ADD_MAXITEM_MES,          209 },
    {_CHECK_COMPLETE_PARTS,     256 },
    {_GET_EDIT_PARTS_POS,       257 },
    {_GET_NOW_TIME,             258 },
    {_SET_CLOCK,                261 },
    {_GET_CLOCK,                262 },
    {_SGET_CMP_EVENT,           259 },
    {_SSET_CMP_EVENT,           260 },
    {_CHECK_PLACE,              263 },
    {_CHECK_VILLAGER,           267 },
    {_SCHECK_REQUEST,           268 },
    {_DELETE_ROBO_PARTS,        269 },
    {_HOBJ_FIXPARTS,            320 },
    {_HOBJ_EDITPARTS,           321 },
    {_HOBJ_INTERIORPARTS,       322 },
    {_HOBJ_CHARA,               330 },
    {_HOBJ_BT_HOBJ,             332 },
    {_HOBJ_ITEM,                336 },
    {_OBJ_DRAW,                 323 },
    {_SET_OBJ_POS,              324 },
    {_GET_OBJ_POS,              325 },
    {_SET_OBJ_ROT,              326 },
    {_GET_OBJ_ROT,              327 },
    {_SET_OBJ_SCALE,            328 },
    {_GET_OBJ_SCALE,            329 },
    {_SYNC_OBJ_OBJ,             333 },
    {_RELEASE_OBJ_OBJ,          334 },
    {_SET_OBJ_FOG,              335 },
    {_GET_TALKNPC_INFO_ID,      384 },
    {_GET_TALKNPC_ID,           385 },
    {_GET_TALKNPC_STATUS,       389 },
    {_SET_TALK_CAMERA,          386 },
    {_SET_TALK_MES,             387 },
    {_SET_TALK_SELECT_MES,      388 },
    {_EVERY_TALK_EVENT,         390 },
    {_SET_CAMERA,               416 },
    {_SET_FOLLOW_CAMERA,        417 },
    {_ADD_CAMERA_ANGLE,         418 },
    {_ADD_CAMERA_HEIGHT,        419 },
    {_ADD_CAMERA_DIST,          420 },
    {_CAMERA_STEP,              421 },
    {_SET_CAMERA_POS,           422 },
    {_GET_CAMERA_POS,           423 },
    {_SET_CAMERA_REF,           424 },
    {_GET_CAMERA_REF,           425 },
    {_SET_CAMERA_SPEED,         426 },
    {_SET_PROJECTION,           427 },
    {_ITEM_GET_CAMERA,          428 },
    {_SET_CAMERA_ANGLE,         429 },
    {_GET_CAMERA_ANGLE,         430 },
    {_SET_CAMERA_ROTATE,        431 },
    {_GET_CAMERA_ROTATE,        432 },
    {_RESET_CAMERA,             433 },
    {_RESET_CAMERA_ANGLE,       436 },
    {_SYNC_CAMERA_REF_CHARA,    434 },
    {_RELEASE_CAMERA_REF_CHARA, 435 },
    {_SYNC_CAMERA_REF_OBJ,      437 },
    {_RELEASE_CAMERA_REF_OBJ,   438 },
    {_SYNC_CAMERA_POS_OBJ,      439 },
    {_RELEASE_CAMERA_POS_OBJ,   440 },
    {_SET_CAMERA_ROLL,          441 },
    {_GET_CAMERA_ROLL,          442 },
    {_FADE_IN,                  500 },
    {_FADE_OUT,                 501 },
    {_CHECK_FADE,               502 },
    {_INIT_SPRITE,              503 },
    {_DRAW_SPRITE,              504 },
    {_DRAW_BG_SPRITE,           516 },
    {_DRAW_BACK,                505 },
    {_DRAW_SHADOW,              506 },
    {_SET_CLIP_POINT,           507 },
    {_DRAW_EDIT_WATER,          508 },
    {_DRAW_WATER_SURFACE,       509 },
    {_SCREEN_FILTER,            510 },
    {_DRAW_THUNDER,             528 },
    {_SET_LIGHT,                511 },
    {_SET_LIGHT_COLOR,          512 },
    {_SET_AMBIENT,              513 },
    {_SAVE_LIGHT,               514 },
    {_LOAD_LIGHT,               515 },
    {_SET_DOF_LEVEL,            517 },
    {_SP_INIT,                  518 },
    {_SP_SET_TEX,               519 },
    {_SP_SET_POS,               520 },
    {_SP_SET_RGBA,              521 },
    {_SP_SET_ROT,               522 },
    {_SP_SET_MOVE,              526 },
    {_SP_DRAW,                  523 },
    {_INIT_DRAW_DAY,            524 },
    {_DRAW_DAY,                 525 },
    {_MAP_TITLE_OFF,            527 },
    {_SET_WIND,                 529 },
    {_ASQ_INIT,                 600 },
    {_ASQ_SYNC_CHARA,           601 },
    {_ASQ_SET_POS,              602 },
    {_ASQ_MOVE,                 603 },
    {_ASQ_MOVE_STEP,            604 },
    {_ASQ_ROT_REF,              605 },
    {_ASQ_CLEAR_ROT,            606 },
    {_ASQ_WAIT_ROT,             607 },
    {_ASQ_ROT_MOVE,             608 },
    {_ASQ_ROT_ANGLE,            618 },
    {_ASQ_SET_ROT,              609 },
    {_ASQ_DELAY_ROT,            610 },
    {_ASQ_CHECK,                611 },
    {_ASQ_MOTION_TRG,           612 },
    {_ASQ_MOTION_PLAY,          613 },
    {_ASQ_MOTION_STOP,          614 },
    {_ASQ_MOTION_NEXT,          615 },
    {_ASQ_ANIME_TRG,            616 },
    {_ASQ_ANIME,                617 },
    {_OBJ_ANIME_INIT,           650 },
    {_OBJ_ANIME,                651 },
    {_SSET_GAME_FLAG,           700 },
    {_SGET_GAME_FLAG,           701 },
    {_SSET_GAME_INT_FLAG,       711 },
    {_SGET_GAME_INT_FLAG,       712 },
    {_SITEM_GET,                702 },
    {_SSET_MAP_FLAG,            703 },
    {_SGET_MAP_FLAG,            704 },
    {_SET_WORK_FLAG,            705 },
    {_GET_WORK_FLAG,            706 },
    {_SITEM_CHECK,              707 },
    {_SITEM_CHECK_ALL,          724 },
    {_SGET_DUNGEON_STATUS,      708 },
    {_SGET_PARTY_NUM,           709 },
    {_SSET_PARTY_NUM,           710 },
    {_SSET_REQUEST_EVENT_FLAG,  713 },
    {_SADD_VISIT_MAP,           714 },
    {_SSKILL_GET,               715 },
    {_SGET_EQUIP_WEAPON,        716 },
    {_SADD_MAXITEM,             717 },
    {_SITEM_LOST,               718 },
    {_SATRA_PARTS_GET,          719 },
    {_SATRA_CHIP_GET,           720 },
    {_SGET_REQUEST,             264 },
    {_SGET_ATRA_PARTS_NUM,      265 },
    {_SGET_DAY,                 266 },
    {_SSET_TIME,                721 },
    {_SGET_TIME,                722 },
    {_SADD_TIME,                723 },
    {_SEQUIP_DEFAULT_WEAPON,    725 },
    {_LOAD_SND_SYNC,            800 },
    {_LOAD_SOUND_SET,           816 },
    {_LOAD_VOICE_SET,           822 },
    {_LOAD_BGM,                 801 },
    {_DELETE_BGM,               815 },
    {_PLAY_BGM,                 802 },
    {_STOP_BGM,                 803 },
    {_SET_BGM_VOL,              804 },
    {_SET_BGM_VOLF,             825 },
    {_GET_BGM_VOL,              805 },
    {_GET_BGM_NO,               806 },
    {_SET_OUT_BGM_NO,           813 },
    {_GET_OUT_BGM_NO,           814 },
    {_BGM_FADE_IN,              817 },
    {_BGM_FADE_OUT,             818 },
    {_CHECK_BGM_FADE,           819 },
    {_PLAY_AMB_BGM,             820 },
    {_STOP_AMB_BGM,             821 },
    {_SET_AMB_VOLF,             824 },
    {_PLAY_SE,                  807 },
    {_STOP_SE,                  808 },
    {_SET_SE_VOL,               809 },
    {_LOAD_SPECIAL_SE,          810 },
    {_PLAY_SPECIAL_SE,          811 },
    {_STOP_SPECIAL_SE,          812 },
    {_SET_SPECIAL_SE_VOL,       823 },
    {_SOUND_OFF_COUNT,          826 },
    {_GET_V_ARG,                900 },
    {_GET_I_ARG,                901 },
    {_GET_F_ARG,                902 },
    {_TEST,                     1000},
    {_EB_INIT,                  950 },
    {_EB_SET_KEY,               951 },
    {_EB_LOOP,                  952 },
    {_EB_SET_MOTION,            953 },
    {_EB_DEBUG,                 954 },
    {_EB_INTRO_START,           955 },
    {_EB_INTRO_LOOP,            956 },
    {_EB_INTRO_END,             957 },
    {_EB_FINISH_SOUND,          958 },
    {_LOAD_MAIN_CHARA,          999 },
    {_LOAD_FISHING_DATA,        998 },
    {_GOTO_FISHING,             997 },
    {_INIT_FISH,                996 },
    {_EXIT_FISHING,             995 },
    {_SET_FISHING_ESA,          994 },
#ifdef PAL
    {_GET_TV_MODE,              1001},
    {_GET_LANG_CODE,            1002},
#endif
    {NULL,                      -1  },
};

int EdSetEventScript(char *common_script, char *map_script, CDataAlloc2<1> *allocator) {
    if (common_script == NULL) {
        event_enable = 0;
        return 0;
    }

    event_enable = 1;
    RS_STACKDATA *stack = (RS_STACKDATA *) allocator->Alloc(64);
    RS_CALLDATA  *calls = (RS_CALLDATA *) allocator->Alloc(384);
    EdEventScript.load((RS_PROG_HEADER *) common_script, stack, 128, calls, 512);

    if (map_script == NULL) {
        map_script = common_script + ((int *) common_script)[5];
    }

    if (EdEventInfo.messages[1] != NULL) {
        EdEventInfo.messages[1]->SetBuff((short *) map_script);
    }

    int i;

    for (i = 0; i < 1500; i++) {
        ext_func__2[i] = NULL;
    }

    for (i = 0;; i++) {
        if (ext_func_info[i].function == NULL) {
            break;
        }

        int j;

        for (j = 0; j < i; j++) {
            if (ext_func_info[i].operation == ext_func_info[j].operation) {
                printf("same ext_func_no!!!\n");

                while (1) {
                }
            }
        }

        int operation = ext_func_info[i].operation;

        if (operation < 0 || operation >= 1500) {
            printf("ext func over!!");
        } else {
            ext_func__2[operation] = ext_func_info[i].function;
        }
    }

    EdEventScript.ext_func(ext_func__2, 1500);
    simple_event = 0;
    return 1;
}

int EdInitEventParamSimple() {
    int i;
    EdEventInfo.next_event = -1;
    EdEventInfo.fadeout_event_no = -1;
    EdEventInfo.interior_entrance = -1;
    EdEventInfo.projection = -1.0f;
    EdEventInfo.return_code = ED_EVENT_RETURN_NONE;
    motion_stop_flag = 0;
    menu_mode = ED_MENU_NONE;
    CurrentDir[0] = '\0';
    actv_file = 0;
    actv_buffer = 0;
    follow_chara = NULL;
    sync_camera_ref_chara = NULL;
    sync_camera_ref_obj = NULL;
    sync_camera_pos_obj = NULL;
    sync_camera_ref_offset[0] = 0.0f;
    sync_camera_ref_offset[1] = 0.0f;
    sync_camera_ref_offset[2] = 0.0f;
    EdEventInfo.player_collision = false;
    EdEventInfo.player_draw = true;
    EdEventInfo.player_shadow_draw = true;
    EdEventInfo.player_foot_sound = 1;
    EdEventInfo.player_stop = false;

    for (i = 0; i < 16; i++) {
        EdEventInfo.npc_collision[i] = 0;
        EdEventInfo.npc_draw[i] = 1;
        EdEventInfo.npc_shadow_draw[i] = 1;
        EdEventInfo.npc_foot_sound[i] = 1;
        EdEventInfo.npc_stop[i] = 0;
        EdEventInfo.npc_draw_before[i] = 0;
    }

    for (i = 0; i < 266; i++) {
        asq_table[i].operation = ACT_SEQ_UNUSED;
        asq_table[i].next = NULL;
    }

    for (i = 0; i < 10; i++) {
        ActSeq[i].Initialize(asq_table, 266);
    }

    SpriteTable.Initialize(sprite_table, 32, 4);
    SpriteTable.ClearPointer();
    SpriteTableBack.Initialize(sprite_table, 32, 4);
    SpriteTableBack.ClearPointer();

    for (i = 45; i <= 49; i++) {
        TexManager.DeleteTextureBlock(i);
    }

    memset(ObjHandle, 0, sizeof(ObjHandle));
    event_stop = 0;
    event_pause = 0;

    for (i = 0; i <= 0; i++) {
        EdEventInfo.item_frame[i] = NULL;
    }

    SceneData.Initialize();

    for (i = 0; i < 16; i++) {
        ED_SPRITE *sprite = GetSprite(i);

        if (sprite != NULL) {
            InitSprite(sprite);
        } else {
            break;
        }
    }

    ClearObjAnime(-1);
    EdEventInfo.wind[3] = -1.0f;
    EdEventInfo.sound_off_count = 0;
    return 1;
}

int EdInitEventParam() {
    EdInitEventParamSimple();
    CDataAlloc2<1> *allocator = (CDataAlloc2<1> *) BaseBuffer;

    if (allocator != NULL) {
        int     used = allocator->used;
        u_char *base = allocator->base + used * 16;
        int     limit = allocator->limit - used;
        EdVillagerBuffer.base = base;
        EdVillagerBuffer.limit = limit;
        EdVillagerBuffer.used = 0;
    }

    for (int i = 0; i < 320; i++) {
        anime_data[i].Initialize();
    }

    if (EdEventInfo.main_texture_animation == NULL) {
        EdEventInfo.main_character->InitializeTexAnime(anime_data, 320);
    }

    for (int i = 0; i < EdEventInfo.npc_count; i++) {
        EdEventInfo.npcs[i].Initialize();
        EdEventInfo.npcs[i].InitializeTexAnime(anime_data, 320);
        TexManager.DeleteTextureBlock(EdEventInfo.npc_texture_block + i);
    }

    if (EdEventInfo.main_character != NULL) {
        EdEventInfo.main_character->SetMotion(0, 0);
    }

    return 1;
}

/**
 * Initializes the editor event runtime and starts the requested script program.
 *
 * @mangled EdEventInit__FiP14CDataAlloc2_1_Pc
 * @address 0x1973B0
 * @size 0x2E8
 */
int EdRunEvent(int program, CDataAlloc2<1> *arena);

int EdEventInit(int event_number, CDataAlloc2<1> *arena, char *program) {
    if (program == NULL) {
        return 0;
    }

    run_system_event = 0;

    for (int i = 0; i < 8; i++) {
        ClsMes *message = EdEventInfo.messages[i];

        if (message == NULL) {
            continue;
        }

        message->text_columns = MES_WIN_COLUMNS;
        message->text_rows = 10;
        message->text_len = 0;
        message->text_width = 0;
        message->text_height = 0;
        message->fade = 0.0f;
        message->fade_in = true;
        message->text_rate = message->text_rate_set;
        message->waiting = false;
        message->text_at = 0.0f;
        message->text_no = 0;
        message->text_from = 0;
        message->page_from = 0;
        message->InitMesWinTbl();
        message->clut_now = message->clut_default;
        message->wait = 0;
        message->blink = 0;
        message->auto_page_wait = 0;
        message->mes_made = -1;
        message->edge_alpha = 0x80;

        for (int j = 0; j < 10; j++) {
            message->mes_no[j] = -1;
        }

        for (int j = 0; j < 8; j++) {
            message->values[j] = 0;
        }

        message->value = 0;
        message->value_signed = false;
        message->value_show = true;
        message->value_narrow = false;
        message->space_width = -1;
        message->space_area = -1;
        message->cursor_row = -1;
        message->cursor_y = 0;
        message->cursor_lit = false;

        for (int j = 0; j < 10; j++) {
            message->line_pos[j].x = -1;
            message->line_pos[j].y = -1;
        }
    }

    EdEventScript.reload((RS_PROG_HEADER *) program);
    EdEventInfo.return_code = ED_EVENT_RETURN_NONE;
    EdEventInfo.event_no = event_number;
    EdEventInfo.exit_code = 0;
    EdEventInfo.draw_exclamation_mark = false;
    EdEventInfo.reset_camera_angle = 0;
    EdEventInfo.outside_map_no = -1;
    EdEventInfo.suppress_background = false;
    EdEventInfo.screen_filter = false;
    EdEventInfo.lighting_override = false;
    EdEventInfo.suppress_shadows = false;
    EdEventInfo.return_code = ED_EVENT_RETURN_NONE;
    EdEventInfo.map_jump_bgm_stop = true;
    EdEventInfo.map_jump_bgm_play = true;
    MGGetPLight(EdEventInfo.light_direction, EdEventInfo.light_color);
    MGGetAmbient(EdEventInfo.ambient_color);

    if (event_enable == 0) {
        return 0;
    }

    EdInitEventParamSimple();
    BaseBuffer = (u_int *) arena;

    if (EdRunEvent(event_number, arena) <= 0) {
        simple_event = 1;
        GamePad.AutoRepeatOff();
        GamePad.MenuModeOff();
        return 0;
    }

    simple_event = 0;
    skip_enable = 0;
    return 1;
}

/**
 * Resets event-script workspace and starts the requested script program.
 */
void RunEvent(CRunScript *script, int program, CDataAlloc2<1> *arena) {
    int     used = arena->used;
    u_char *base = arena->base + used * 16;
    int     limit = arena->limit - used;
    EdEventBuffer.base = base;
    EdEventBuffer.limit = limit;
    EdEventBuffer.used = 0;
    world_rot[3] = 0.0f;
    world_rot[2] = 0.0f;
    world_rot[1] = 0.0f;
    world_rot[0] = 0.0f;
    sceVu0CopyVector(world_pos, world_rot);
    sceVu0UnitMatrix(world_local);
    sceVu0UnitMatrix(local_world);
    set_wl_matrix = 0;
    script->run(program);
}

/**
 * Starts an event script using the shared event-script runner.
 */
int EdRunEvent(int program, CDataAlloc2<1> *arena) {
    RunEvent(&EdEventScript, program, arena);
}

int EdResumeEvent() {
    EdEventScript.resume();
    return 1;
}

int EdEventSkip() {
    EdEventScript.skip();
    return 1;
}

int EdEventAllClear() {
    EdEventInfo.main_character->ClearTexAnime();

    if (EdEventInfo.main_texture_animation != NULL) {
        for (int i = 0; i < EdEventInfo.main_texture_animation_count; i++) {
            EdEventInfo.main_texture_animation[i].Initialize();
        }

        EdEventInfo.main_character->InitializeTexAnime(EdEventInfo.main_texture_animation, EdEventInfo.main_texture_animation_count);
    }

    EdEventInfo.main_character->DeleteExtendTexture(EdEventInfo.player_texture_block);
    EdEventInfo.main_character->DeleteExtendMotion();
    EdEventInfo.main_character->SetMotion(0, 0);

    for (int i = 0; i < EdEventInfo.npc_count; i++) {
        CNPCharacter *npc = GetNPC(i);

        if (npc != NULL) {
            if (npc->texture_block >= 54) {
                TexManager.DeleteTextureBlock(npc->texture_block);
            }

            npc->Initialize();
            npc->InitializeTexAnime(NULL, 0);
        }
    }

    EdEventInfo.main_character->SetMotionCamera(EdEventInfo.camera);

    for (int i = 0; i <= 0; i++) {
        DeleteItemFrame(i);
    }

    SceneData.Initialize();

    for (int i = 45; i <= 49; i++) {
        TexManager.DeleteTextureBlock(i);
    }

    for (int i = 0; i < 10; i++) {
        ActSeq[i].ClearSeq();
    }

    return 1;
}

int EdEventFinish() {
    EdEventInfo.suppress_background = false;
    EdEventInfo.suppress_shadows = false;
    EdEventInfo.screen_filter = false;
    EdEventInfo.lighting_override = false;
    EdEventInfo.wind[3] = -1.0f;
    EdEventInfo.main_character->frame->DeleteReference();
    CFrame *shadow = EdEventInfo.main_character->shadow_frame;

    if (shadow != NULL) {
        shadow->DeleteReference();
    }

    return 1;
}

/**
 * Steps the editor's event system and reports the action requested by the script.
 *
 * @mangled EdEventMode__FP13CCameraFollowi
 * @address 0x197AD0
 * @size 0xCFC
 */
int EdEventMode(CCameraFollow *camera, int kind) {
    int result;

#ifdef PAL
    // Debug builds toggle the event debugger with one button and pause the event with another.
    if (DebugMode != 0 && GamePad.Down(PAD_R3) != 0) {
        EdEventStopPlay();
    }

    if (DebugMode != 0 && menu_mode == ED_MENU_NONE && GamePad.Down(PAD_START) != 0) {
        if (EdCheckEventPause() == 0) {
            SndBgmPause();
        } else {
            SndBgmRePlay();
        }

        EdEventPause();
    }

#endif
    if (event_stop != 0) {
        static int    mode = 0;
        static char  *mode_name[] = {"! CAMERA POS\n", "# CAMERA REF\n", "$ CHARACTER\n"};
        static int    select_chara = -1;
        sceVu0FVECTOR position;
        sceVu0FVECTOR reference;
        char          text[128];
        sceVu0FVECTOR chara_position;
        sceVu0FVECTOR chara_rotation;
        int          *collision;
        CCharacter   *chara;

        EdDDebug(1);
        collision = &EdEventInfo.player_collision;

        if (select_chara >= 0) {
            collision = &EdEventInfo.npc_collision[select_chara];
        }

        camera->GetPos(position);
        camera->GetRef(reference);

        switch (mode) {
            case 0:
                camera->FollowOff();
                EdDMoveCamera(position, reference);
                camera->SetPos(position);
                camera->SetRef(reference);
                break;
            case 1:
                camera->FollowOff();
                EdDMoveCameraRef(position, reference);
                camera->SetPos(position);
                camera->SetRef(reference);
                break;
            case 2:
                if (GamePad.Down(PAD_RIGHT) != 0) {
                    select_chara++;

                    if (select_chara >= EdEventInfo.npc_count) {
                        select_chara = -1;
                    }
                }

                if (GamePad.Down(PAD_LEFT) != 0) {
                    select_chara--;

                    if (select_chara < -1) {
                        select_chara = EdEventInfo.npc_count - 1;
                    }
                }

                if (select_chara == -1) {
                    EdDMoveChara(EdEventInfo.main_character, camera);
                } else {
                    EdDMoveChara(&EdEventInfo.npcs[select_chara], camera);
                }

                if (GamePad.Down(PAD_CROSS) != 0) {
                    *collision = !*collision;
                }

                break;
        }

        EdDPrint(mode_name[mode]);
        chara = EdEventInfo.main_character;

        if (select_chara >= 0) {
            chara = &EdEventInfo.npcs[select_chara];
        }

        chara->GetPosition(chara_position);
        chara->GetRotation(chara_rotation);
        GetLocalPos(chara_position, chara_position);
        GetLocalRot(chara_rotation, chara_rotation);
        sprintf(text, "select chara %d\ncollision %d\n", select_chara, *collision);
        EdDPrint(text);
        EdDPrintVector(" pos = ", chara_position);
        EdDPrintVector(" rot = ", chara_rotation);
        sprintf(text, " amb %5.1f %5.1f %5.1f %5.1f\n", chara->ambient_offset[0], chara->ambient_offset[1], chara->ambient_offset[2], chara->ambient_offset[3]);
        EdDPrint(text);
        EdDPrint("camera\n");
        GetLocalPos(position, position);
        GetLocalPos(reference, reference);
        EdDPrintVector(" pos = ", position);
        EdDPrintVector(" ref = ", reference);
        float angle = camera->GetAngle();
        chara_rotation[2] = 0.0f;
        chara_rotation[0] = 0.0f;
        chara_rotation[1] = angle;
        GetLocalRot(chara_rotation, chara_rotation);
        sprintf(text, " angle = %4.3f\n", chara_rotation[1]);
        EdDPrint(text);
        sprintf(text, " height = %6.2f\n", position[1] - reference[1]);
        EdDPrint(text);
        position[1] = 0.0f;
        reference[1] = 0.0f;
        sprintf(text, " distance = %6.2f\n", DistVector(position, reference));
        EdDPrint(text);
        sprintf(text, " projection = %7.1f\n", MGGetProjection());
        EdDPrint(text);
        EdDCheck();

        if (GamePad.Down(PAD_SELECT) != 0) {
            EdOutPutFile();
        }

        if (GamePad.Down(PAD_UP) != 0) {
            mode--;
        }

        if (GamePad.Down(PAD_DOWN) != 0) {
            mode++;
        }

        if (mode < 0) {
            mode = 2;
        }

        if (mode > 2) {
            mode = 0;
        }

        if (EdEventInfo.projection > 0.0f) {
            MGSetProjection(EdEventInfo.projection);
        }

        if (EdEventInfo.suppress_background != 0 && EdEventInfo.background_color[0] >= 0.0f) {
            MGSetBGColor(EdEventInfo.background_color[0], EdEventInfo.background_color[1], EdEventInfo.background_color[2], 128.0f);
        }

        return 0;
    }

    if (event_pause != 0) {
        EdPauseFlag = 1;
        CCharacter::MotionStopFlag = 1;
        CTextureAnime::stop_anime = 1;
        ObjAnimeAllStop();
        CCamera::StopCamera = 1;

        if (GamePad.Down2(PAD_CROSS) != 0 && menu_mode == ED_MENU_NONE) {
            SndBgmRePlay();
            BreakReadBG();
            EdEventSkip();
            EdEventPause();
        }

        if (EdEventInfo.projection > 0.0f) {
            MGSetProjection(EdEventInfo.projection);
        }

        if (EdEventInfo.suppress_background != 0 && EdEventInfo.background_color[0] >= 0.0f) {
            MGSetBGColor(EdEventInfo.background_color[0], EdEventInfo.background_color[1], EdEventInfo.background_color[2], 128.0f);
        }

        if (GamePad.Down(PAD_CIRCLE) != 0) {
            SndBgmRePlay();
            EdEventPause();
        }

        return 0;
    }

    EdPauseFlag = 0;

    if (motion_stop_flag != 0) {
        CCharacter::MotionStopFlag = 1;
        CTextureAnime::stop_anime = 1;
        ObjAnimeAllStop();
        CCamera::StopCamera = 1;
    } else {
        CCharacter::MotionStopFlag = 0;
        CTextureAnime::stop_anime = 0;
        ObjAnimeAllStart();
        CCamera::StopCamera = 0;
    }

    ReadBG();
    EdEventInfo.camera = camera;

    if (EdEventInfo.world_coord_enable != 0) {
        SetWorldCoord((float *) &EdEventInfo.world_coord[8], (float *) &EdEventInfo.world_coord[24]);
    }

    for (int i = 0; i < 8; i++) {
        if (EdEventInfo.messages[i] != NULL) {
            EdEventInfo.messages[i]->Step();
        }
    }

    if (SceneData.frame != NULL) {
        float speed = SceneData.motion_speed;
        SceneData.SetMotion(0, 0);
        SceneData.motion_speed = speed;
#ifdef PAL_TIMING
        SceneData.motion_speed = 6.0f * speed / 5.0f;
#endif
        SceneData.Step();
    }

    if (menu_mode == ED_MENU_NONE) {
        menu_mode_status = -1;
        EdResumeEvent();

        if (menu_mode != ED_MENU_NONE) {
            menu_mode_status = 0;

            if (EdInitMenu(menu_mode) == 0) {
                menu_mode = ED_MENU_NONE;
            }

            motion_stop_flag = 1;
        }
    } else {
        switch (menu_mode_status) {
            case 0:
                motion_stop_flag = 1;

                if (EdInitModeFinish(EdEventInfo.camera, TexManager.GetTexture("frame_image", -1)) != 0) {
                    menu_mode_status = 1;
                    motion_stop_flag = 0;
                    MenuMapJumpMode = -1;
                }

                break;
            case 1:
            case 2:
                EdDrawOffAll();

                if (EdMenuMode() != 0) {
                    switch (menu_mode) {
                        case ED_MENU_USE_ITEM:
                        case ED_MENU_CHANGE_ESA: {
                            int item = EdGetUseItem();

                            if (p_use_item != NULL) {
                                ((RS_STACKDATA *) p_use_item)->i = item;
                            }

                            p_use_item = NULL;
                            break;
                        }
                        case ED_MENU_NAME_REGISTRY:
                            break;
                        case ED_MENU_WORLD_MAP:
                            if (p_jump_map_no != 0) {
                                ((RS_STACKDATA *) p_jump_map_no)->i = MenuMapJumpMode + 1;
                            }

                            p_jump_map_no = 0;
                            break;
                    }

                    menu_mode = ED_MENU_NONE;
                    EdDrawOnAll();
                    EdExitMenu();
                }

                break;
        }
    }

    for (int i = 0; i < 10; i++) {
        ActSeq[i].Play();
    }

    for (int i = 0; i < 16; i++) {
        ObjAnimePlay(&obj_anime[i]);
    }

    result = EdEventInfo.return_code;

    if (result > 0) {
        EdEventFinish();
        GamePad.AutoRepeatOff();
        GamePad.MenuModeOff();
    }

    if (EdEventInfo.projection > 0.0f) {
        MGSetProjection(EdEventInfo.projection);
    }

    if (EdEventInfo.suppress_background != 0 && EdEventInfo.background_color[0] >= 0.0f) {
        MGSetBGColor(EdEventInfo.background_color[0], EdEventInfo.background_color[1], EdEventInfo.background_color[2], 128.0f);
    }

    if (camera != NULL) {
        if (follow_chara != NULL) {
            sceVu0FVECTOR position;
            follow_chara->GetPosition(position);
            camera->SetFollow(position[0], position[1] + 0.8f * follow_chara->body_height, position[2]);
        }

        if (sync_camera_ref_chara != NULL) {
            sceVu0FVECTOR reference = {0.0f, 0.0f, 0.0f, 1.0f};
            sync_camera_ref_chara->GetPosition(reference);
            sceVu0AddVector(reference, reference, sync_camera_ref_offset);
            camera->SetRef(reference[0], reference[1] + 0.8f * sync_camera_ref_chara->body_height, reference[2]);
        }

        if (sync_camera_ref_obj != NULL) {
            sceVu0FVECTOR reference;
            get_obj_world_pos(sync_camera_ref_obj, reference);
            sceVu0AddVector(reference, reference, sync_camera_ref_offset);
            camera->SetRef(reference[0], reference[1], reference[2]);
        }

        if (sync_camera_pos_obj != NULL) {
            sceVu0FVECTOR position;
            get_obj_world_pos(sync_camera_pos_obj, position);
            camera->SetPos(position);
        }
    }

    if (result == 0 && EdEventScript.IsEnd() != 0) {
        EdEventFinish();
        GamePad.AutoRepeatOff();
        GamePad.MenuModeOff();
        result = 1;
    }

    return result;
}

int EdEventNPCStep() {
    int wind = EdEventInfo.main_character->wind;

    if (EdEventInfo.player_stop == 0) {
        CVector3_f_ velocity;
        velocity.x = 0.0f;
        velocity.y = 0.0f;
        velocity.z = 0.0f;
        EdEventInfo.main_character->SetVelocity(velocity);
        EdEventInfo.main_character->Step();
        EdEventInfo.main_character->ShadowStep();
    }

    for (int i = 0; i < EdEventInfo.npc_count; i++) {
        EdEventInfo.npcs[i].wind = wind;

        if (EdEventInfo.npc_stop[i] == 0) {
            ((CNPCharacter *) EdEventInfo.npcs)[i].sequence_enabled = 0;
            EdEventInfo.npcs[i].Step();
            EdEventInfo.npcs[i].ShadowStep();
        }
    }

    if (EdEventInfo.player_stop == 0) {
        EdEventInfo.main_character->ClothStep(0);
    }

    for (int i = 0; i < EdEventInfo.npc_count; i++) {
        EdEventInfo.npcs[i].wind = wind;

        if (EdEventInfo.npc_stop[i] == 0) {
            EdEventInfo.npcs[i].ClothStep(0);
        }
    }

    return 1;
}

int EdEventSpriteDraw() {
    for (int block = 45; block <= 49; block++) {
        int texture_loaded = 0;

        for (int i = 0; i < 16; i++) {
            ED_SPRITE *sprite = GetSprite(i);

            if (sprite == NULL) {
                break;
            }

            if (sprite->enabled == 0 || sprite->layer == 0) {
                continue;
            }

            CRect_i_ screen;
            CRect_i_ texel;
            screen.x = screen.y = screen.width = screen.height = 0;
            texel.x = texel.y = texel.width = texel.height = 0;
            screen.x = (int) sprite->x;
            screen.y = (int) sprite->y;
            screen.width = sprite->width;
            screen.height = sprite->height;
            texel.x = sprite->source_x;
            texel.y = sprite->source_y;
            texel.width = sprite->source_width;
            texel.height = sprite->source_height;

            if (screen.width < 0) {
                screen.width = texel.width;
            }

            if (screen.height < 0) {
                screen.height = texel.height;
            }

            if (sprite->texture.block != block) {
                continue;
            }

            if (texture_loaded == 0) {
                TexManager.ReloadTexture(GetVif1Packet(), block);
                texture_loaded = 1;
            }

            setbilinear(sprite->bilinear);
            sprite->x += sprite->move_x;
            sprite->y += sprite->move_y;

            if (sprite->rotated == 0) {
                set2DSprite(GetVif1Packet(), &sprite->texture, screen, texel, (u_char) sprite->red, (u_char) sprite->green, (u_char) sprite->blue, (u_char) sprite->alpha);
            } else {
                set2DSpriteRot(GetVif1Packet(), &sprite->texture, screen, texel, sprite->rotation_x, sprite->rotation_y, sprite->rotation, (u_char) sprite->red, (u_char) sprite->green, (u_char) sprite->blue, (u_char) sprite->alpha);
            }
        }
    }

    TexManager.ReloadTexture(GetVif1Packet(), 45);
    SpriteTable.DrawTable();
    return 1;
}

int EdEventBackSpriteDraw() {
    TexManager.ReloadTexture(GetVif1Packet(), 46);
    SpriteTableBack.DrawTable();
    return 1;
}

int EdSearchNearNPC(CCharacter *character, CNPCharacter *npcs, int count) {
    sceVu0FVECTOR direction;
    sceVu0FVECTOR character_rotation;
    sceVu0FVECTOR npc_rotation;
    sceVu0FVECTOR character_position;
    sceVu0FVECTOR npc_position;
    int           i;
    int           nearest = -1;
    float         nearest_distance = 0.0f;
    character->GetRotation(character_rotation);

    for (i = 0; i < count; i++) {
        if (npcs[i].villager_id < 0) {
            continue;
        }

        int available = npcs[i].initialized != 0 && npcs[i].draw_enabled != 0;

        if (!available) {
            continue;
        }

        npcs[i].talk_target = false;
        character->GetPosition(character_position);
        npcs[i].GetPosition(npc_position);
        float height = character_position[1] - npc_position[1];

        if (height < 0.0f) {
            height = -height;
        } else {
            height = height;
        }

        if (height > character->body_height) {
            continue;
        }

        npc_position[1] = 0.0f;
        character_position[1] = 0.0f;
        float distance = DistVector(character_position, npc_position);

        if (distance > 5.0f + 1.2f * npcs[i].body_width) {
            continue;
        }

        character->GetDir(npcs[i], direction);
        float character_angle = atan2f(direction[0], direction[2]);
        npcs[i].GetDir(*character, direction);
        float npc_angle = atan2f(direction[0], direction[2]);
        npcs[i].GetRotation(npc_rotation);
        int talk_direction = EdEventInfo.villagers[npcs[i].villager_id].talk_direction;

        if (AngleCmp(npc_angle, npc_rotation[1], HALF_PI_SHORT) != 0 && talk_direction == 0 && npcs[i].event_status == 0) {
            continue;
        }

        if (nearest < 0) {
            nearest_distance = distance;
            nearest = i;
        } else if (distance < nearest_distance) {
            nearest_distance = distance;
            nearest = i;
        }
    }

    return nearest;
}

/** Villager involved in the active conversation. */
static CNPCharacter *talk_villager__2;

/** Metadata identifier of the character involved in the active conversation. */
static int talk_chara_info_id;

/** Camera placement variant selected for the current conversation. */
static int talk_camera;

/** Conversation state currently being processed. */
static int talk_mode;

/** Selected choice in the active conversation prompt. */
static int talk_select;

/** Whether the active conversation message still needs to be created. */
static int TalkMesMake;

/** Message number the active conversation is showing. */
static int TalkMesNo;

int EdTalkModeInit(CNPCharacter *villager, int character_info_id) {
    if (villager->villager_id < 0) {
        return 0;
    }

    talk_villager__2 = villager;

    if (character_info_id < 0) {
        talk_chara_info_id = villager->villager_id;
    } else {
        talk_chara_info_id = character_info_id;
    }

    talk_mode = TALK_MODE_SELECT;
    TalkMesMake = 1;
    TalkMesNo = 0;
    talk_camera = 0;

    if (villager->event_status != 0) {
        talk_camera = rand() % 3;
    }

    talk_select = 0;
    EdEventInfo.talk_npc_id = (villager - EdVillager);
    EdEventInfo.talk_select_prompt = -1;
    return 1;
}

CNPCharacter *EdNowTalkChara() {
    return talk_villager__2;
}

int EdNowTalkCharaInfoID() {
    return talk_chara_info_id;
}

/**
 * Closes the conversation window and leaves it ready for the next message.
 */
static inline void EdCloseTalkMes() {
    EditMes1.text_rate = EditMes1.text_rate_set;
    EditMes1.mes_made = -1;
    EditMes1.fade_in = false;
    EditMes1.text_columns = MES_WIN_COLUMNS;
    EditMes1.text_rows = 0xA;
    EditMes1.text_len = 0;
    EditMes1.text_width = 0;
    EditMes1.text_height = 0;
    EditMes1.fade = 0.0f;
    EditMes1.fade_in = true;
    EditMes1.text_rate = EditMes1.text_rate_set;
    EditMes1.waiting = false;
    EditMes1.text_at = 0.0f;
    EditMes1.text_no = 0;
    EditMes1.text_from = 0;
    EditMes1.page_from = 0;
    EditMes1.InitMesWinTbl();
    EditMes1.clut_now = EditMes1.clut_default;
    EditMes1.wait = 0;
    EditMes1.blink = 0;
    EditMes1.auto_page_wait = 0;
    EditMes1.mes_made = -1;
    EditMes1.edge_alpha = 0x80;

    for (int i = 0; i < 10; i++) {
        EditMes1.mes_no[i] = -1;
    }

    for (int i = 0; i < 8; i++) {
        EditMes1.values[i] = 0;
    }

    EditMes1.value = 0;
    EditMes1.value_signed = false;
    EditMes1.value_show = true;
    EditMes1.value_narrow = false;
    EditMes1.space_width = -1;
    EditMes1.space_area = -1;
    EditMes1.cursor_row = -1;
    EditMes1.cursor_y = 0;
    EditMes1.cursor_lit = false;

    for (int i = 0; i < 10; i++) {
        EditMes1.line_pos[i].x = -1;
        EditMes1.line_pos[i].y = -1;
    }
}

/**
 * Runs the talking-to-a-villager mode for one frame.
 *
 * @mangled EdTalkMode__FP10CCharacterP13CCameraFollowiPi
 * @address 0x199090
 * @size 0xEBC
 */
int EdTalkMode(CCharacter *player, CCameraFollow *camera, int mode, int *selection) {
    /**
     * Camera offsets from the speakers' midpoint for each of the three talk-camera views.
     */
    static sceVu0FVECTOR vv[3] = {
        {-18.8f, 7.1f, -21.3f, 1.0f},
        {21.0f,  6.5f, -5.7f,  1.0f},
        {22.6f,  6.9f, 16.5f,  1.0f},
    };

    GamePad.MenuModeOn(0x78);
    ReadBG();

    if (camera != NULL) {
        camera->FollowOff();
    }

    EditMes1.Step();

    CNPCharacter *villager = talk_villager__2;
    villager->near_camera = true;
    sceVu0FVECTOR player_position;
    sceVu0FVECTOR villager_position;
    sceVu0FVECTOR player_rotation;
    sceVu0FVECTOR villager_rotation;
    sceVu0FVECTOR direction;
    player->GetPosition(player_position);
    player->GetRotation(player_rotation);
    villager->GetPosition(villager_position);
    villager->GetRotation(villager_rotation);
    sceVu0SubVector(direction, villager_position, player_position);
    float player_yaw = atan2f(direction[0], direction[2]);
    float villager_yaw = atan2f(-direction[0], -direction[2]);
    player_rotation[1] = AngleInterpolate(player_rotation[1], player_yaw, 0.1f, INTERPOLATE_STEP);
    player->SetRotation(player_rotation);
    villager_rotation[1] = AngleInterpolate(villager_rotation[1], villager_yaw, 0.1f, INTERPOLATE_STEP);

    if (EdInteriorFlag == 0 || EdEventInfo.villagers[talk_chara_info_id].talk_rotation == 0) {
        villager->SetRotation(villager_rotation);
    }

    sceVu0FVECTOR reference;
    sceVu0AddVector(reference, player_position, villager_position);
    sceVu0ScaleVector(reference, reference, 0.5f);
    reference[1] += 15.0f;

    if (camera != NULL) {
        camera->FollowOff();
        camera->SetRef(reference);
    }

    sceVu0FVECTOR camera_position;
    sceVu0FMATRIX rotation;
    sceVu0UnitMatrix(rotation);
    sceVu0RotMatrixY(rotation, rotation, player_yaw);
    sceVu0ApplyMatrix(camera_position, rotation, vv[talk_camera]);
    sceVu0AddVector(camera_position, reference, camera_position);

    if (camera != NULL) {
        camera->SetPos(camera_position);
    }

    if (camera != NULL) {
        sceVu0FMATRIX view;
        sceVu0FVECTOR eye;
        camera->GetCameraMatrix(view);
        camera->GetPos(eye);
        MGSetViewMatrix(view, eye);
    }

    if (EdEventInfo.talk_select_prompt < 0) {
        talk_mode = TALK_MODE_MESSAGE;
        TalkMesNo = EdEventInfo.talk_messages[0];
    } else {
        TalkMesNo = EdEventInfo.talk_select_message;
    }

    int choice_count = 0;

    for (;;) {
        if (EdEventInfo.talk_messages[choice_count] < 0) {
            break;
        }

        choice_count++;
    }

    SaveData->GetGrdNPCData(MapNo, EdNowTalkCharaInfoID());

    switch (talk_mode) {
        case TALK_MODE_MESSAGE:
            if (GamePad.Down(PAD_CIRCLE | PAD_CROSS) != 0) {
                if (EditMes1.State() == CLSMES_SHOWN) {
                    EdCloseTalkMes();

                    if (camera != NULL) {
                        camera->FollowOn();
                    }

                    villager->SetMotion(0, 0);
                    GamePad.AutoRepeatOff();
                    GamePad.MenuModeOff();
                    return 1;
                }

                if (EditMes1.State() == CLSMES_PAGE_WAIT) {
                    EditMes1.GoNextPage();
                } else {
                    EditMes1.text_rate = 0.0f;
                }
            }

            EditMes1.cursor_row = -1;
            villager->SetMotion(3, 0);
            break;
        case TALK_MODE_SELECT:
            if (GamePad.Down(PAD_UP) != 0) {
                talk_select--;

                if (talk_select < 0) {
                    talk_select = choice_count - 1;
                }
            }

            if (GamePad.Down(PAD_DOWN) != 0) {
                talk_select++;

                if (talk_select > choice_count - 1) {
                    talk_select = 0;
                }
            }

            if (GamePad.Down(PAD_CROSS) != 0) {
                if (EditMes1.State() == CLSMES_SHOWN) {
                    talk_mode = TALK_MODE_MESSAGE;
                    TalkMesMake = 1;
                    TalkMesNo = EdEventInfo.talk_messages[talk_select];

                    if (EdEventInfo.talk_select_prompt == 0 && talk_select == 3) {
                        SV_GRD_NPC *npc = SaveData->GetGrdNPCData(MapNo, EdNowTalkCharaInfoID());

                        if (npc != NULL) {
                            npc->flags |= 2;
                        }
                    }

                    if ((talk_select == 0 && EdEventInfo.talk_select_prompt > 0 && EdEventInfo.talk_select_prompt != 4) || (talk_select > 0 && EdEventInfo.talk_select_prompt == 4)) {
                        EdCloseTalkMes();

                        if (camera != NULL) {
                            camera->FollowOn();
                        }

                        villager->SetMotion(0, 0);
                        GamePad.AutoRepeatOff();
                        GamePad.MenuModeOff();

                        if (EdEventInfo.talk_select_prompt == 4) {
                            EdSetShopNo(EdEventInfo.talk_messages[talk_select]);

                            if (talk_select == 1) {
                                return 2;
                            }

                            if (talk_select == 2) {
                                return 3;
                            }

                            if (talk_select == 3) {
                                return 2;
                            }

                            if (talk_select == 4) {
                                return 2;
                            }
                        }

                        EdSetShopNo(EdEventInfo.talk_messages[0]);

                        if (EdEventInfo.talk_select_prompt == 1) {
                            return 2;
                        }

                        if (EdEventInfo.talk_select_prompt == 2) {
                            return 3;
                        }

                        if (EdEventInfo.talk_select_prompt == 3) {
                            if (selection != NULL) {
                                *selection = EdEventInfo.talk_messages[0];
                            }

                            return 4;
                        }

                        return 1;
                    }
                } else {
                    EditMes1.text_rate = 0.0f;
                }
            }

            if (GamePad.Down(PAD_CIRCLE) != 0) {
                GamePad.AutoRepeatOff();
                GamePad.MenuModeOff();
                EdCloseTalkMes();

                if (camera != NULL) {
                    camera->FollowOn();
                }

                villager->SetMotion(0, 0);
                return 1;
            }

            EditMes1.cursor_row = talk_select;
            villager->SetMotion(0, 0);
            break;
    }

    int talk_position[4];

    if (talk_mode == TALK_MODE_SELECT) {
        if (mode != 0) {
            EditMes1.tail_on = false;
            EditMes1.auto_pos = MES_POS_BOTTOM;
        }

        EditMes1.AutoSetSub(player, villager, talk_position);
        EditMes1.AutoSet(talk_position);
    } else {
        EditMes1.tail_on = true;
        EditMes1.auto_pos = MES_POS_AUTO;
        EditMes1.AutoSetSub(villager, player, talk_position);
        EditMes1.AutoSet(talk_position);
    }

    switch (talk_mode) {
        case TALK_MODE_SELECT:
            if (TalkMesMake != 0) {
                EditMes1.text_rate = 0.0f;
                TalkMesMake = EditMes1.MakeMesWin(TalkMesNo);
            }

            break;
        case TALK_MODE_MESSAGE:
            if (TalkMesMake != 0) {
                TalkMesMake = EditMes1.MakeMesWin(TalkMesNo);
            }

            break;
    }

    return 0;
}

ED_SPRITE::ED_SPRITE() {
}

int CheckPartsInfo(EDITPARTS_INFO *info) {
    if (info->obtained == 0) {
        return 0;
    }

    if (info->stock <= 0) {
        return 0;
    }

    return 1;
}
