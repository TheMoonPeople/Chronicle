#include "common.h"

#include <libvu0.h>

#include <cstring>

#include "edit.hpp"
#include "editground.hpp"
#include "editloop3.hpp"
#include "editloop.hpp"
#include "editpartsinfo.hpp"
#include "boxvu0.hpp"
#include "camera.hpp"
#include "character.hpp"
#include "collision.hpp"
#include "dataalloc.hpp"
#include "dataread.hpp"
#include "frame.hpp"
#include "gameutil.hpp"
#include "mapparts.hpp"
#include "mathutil.hpp"
#include "npcharacter.hpp"

#include "platform/config.hpp"

void PortEdSetVillagerNextPos(CNPCharacter *villager, VILLAGER_INFO *info, CEditGround *ground);

namespace {

// Retail's GetNearVill and the choice its callers make from it. Of the villagers flagged to draw,
// the two nearest the player that stand in front of the camera stay flagged where they are within
// 150 of the player, and so does every one within video.detail_distance of the player; the rest
// fade out.
void KeepNearVillagers(CCamera *camera, CCharacter *player) {
    sceVu0FVECTOR player_position;
    sceVu0FVECTOR camera_direction;
    sceVu0FVECTOR camera_offset;
    sceVu0FVECTOR camera_position;
    sceVu0FVECTOR villager_position;
    int           nearest[2] = {-1, -1};
    float         nearest_distance[2] = {150.0f, 150.0f};
    float         detail = ConfigDetailDistance();

    player->GetPosition(player_position);
    camera->GetDir(camera_direction);
    camera->GetPos(camera_position);

    for (int i = 0; i < 10; i++) {
        if (EdVillager[i].near_camera == 0) {
            continue;
        }

        EdVillager[i].GetPosition(villager_position);
        float distance = DistVector(player_position, villager_position);
        EdVillager[i].near_camera = distance < detail;
        sceVu0SubVector(camera_offset, villager_position, camera_position);

        if (!(sceVu0InnerProduct(camera_offset, camera_direction) > 0.0f)) {
            continue;
        }

        if (distance < nearest_distance[0]) {
            nearest[1] = nearest[0];
            nearest_distance[1] = nearest_distance[0];
            nearest[0] = i;
            nearest_distance[0] = distance;
        } else if (distance < nearest_distance[1]) {
            nearest[1] = i;
            nearest_distance[1] = distance;
        }
    }

    for (int i = 0; i < 2; i++) {
        if (nearest[i] >= 0) {
            EdVillager[nearest[i]].near_camera = true;
        }
    }
}

} // namespace

// Retail's EdMoveVillager, with KeepNearVillagers in place of GetNearVill and the choice of the
// two nearest.
PC_OVERRIDE void EdMoveVillager(VILLAGER_INFO *villagers) {
    CEditGround *ground = EdExchangeInfo.ground;
    CCharacter  *player = EdExchangeInfo.player;
    CCamera     *camera = EdExchangeInfo.camera;
    int          i;

    for (i = 0; i < 10; i++) {
        sceVu0FVECTOR  position;
        sceVu0FVECTOR  rotation;
        CNPCharacter  *npc = &EdVillager[i];
        VILLAGER_INFO *info = &villagers[i];
        npc->near_camera = false;

        if (info->placed == 0) {
            continue;
        }

        if (info->character_no >= 0 && npc->draw_enabled == 0) {
            continue;
        }

        if (info->character_no >= 0 && info->initial_motion == 0) {
            EDITPARTS_INFO *parts = EditPartsInfo.GetPartsInfo(info->character_no);

            if (parts->obtained == 0 || parts->elements[info->model_no].enabled == 0) {
                continue;
            }
        }

        npc->near_camera = true;
        sceVu0CopyVector(position, info->position);
        sceVu0CopyVector(rotation, info->rotation);

        if (info->initial_motion == 0) {
            npc->CCharacter::SetPosition(position);
            npc->CCharacter::SetRotation(rotation[0], rotation[1], rotation[2]);
        } else {
            sceVu0FVECTOR player_position;
            sceVu0FVECTOR villager_position;
            player->GetPosition(player_position);
            sceVu0CopyVector(villager_position, npc->pos);
            float distance = DistVector(player_position, villager_position);
            npc->sequence_enabled = 1;

            if (distance < 20.0f) {
                npc->sequence_enabled = 0;
                npc->motion_no = 0;
                npc->motion_flags = 0;
                npc->motion_speed = -1.0f;
            }

            PortEdSetVillagerNextPos(npc, info, ground);
        }
    }

    KeepNearVillagers(camera, player);

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

// Retail's EdMoveVillagerSubMap, with KeepNearVillagers in place of GetNearVill and the choice of
// the two nearest.
PC_OVERRIDE void EdMoveVillagerSubMap(VILLAGER_INFO *villagers) {
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

    KeepNearVillagers(camera, player);

    for (i = 0; i < 10; i++) {
        if (EdVillager[i].villager_id < 0) {
            EdVillager[i].near_camera = true;
        }

        EdVillager[i].Step();
        EdVillager[i].ShadowStep();
        EdVillager[i].ClothStep(0);
    }
}

// Retail's EdInitToEPInfo lays the cell map and names out 0x78 bytes in, past its own header; the
// host header is 0xA0 bytes, so 0x78 lands on its element names and function table. They start past
// the host's header instead.
PC_OVERRIDE int EdInitToEPInfo(INIT_PARTSINFO *init, EPARTS_INFO_HEADER *header) {
    header->header_size = sizeof(EPARTS_INFO_HEADER);
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
