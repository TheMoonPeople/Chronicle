#pragma helper_mask_gpr 0x30
#pragma helper_mask_fpr 0x1000
#pragma name_counter 604

#include "shot_utils.hpp"

#include <cstdio>

#include "charaheight.hpp"
#include "dataalloc.hpp"
#include "dataread.hpp"
#include "dranmapfield.hpp"
#include "dun/gameloop.hpp"
#include "dungeonmap.hpp"
#include "fishing.hpp"
#include "mathutil.hpp"
#include "mglib.hpp"
#include "monstorunit.hpp"
#include "rect.hpp"
#include "snd.hpp"
#include "texture.hpp"
#include "userstatus.hpp"

SHOT_COLLISION_RESULT checkCollision(float *hit_position, float *position, float *movement, s32 target_mode, float radius) {
    sceVu0FVECTOR destination;
    int           active;

    WorkBuffer__2->used = 0;
    CCPoly *polygons = (CCPoly *) WorkBuffer__2->Alloc(0x500);
    int     polygon_count = setCollisionData(NowDngMap, polygons, position, 20.0f, 1.5f);

    if (polygon_count >= 256) {
        printf(fishing_err_format, polygon_count);
    }

    destination[0] = position[0] + movement[0];
    destination[1] = position[1] + movement[1];
    destination[2] = position[2] + movement[2];

    if (target_mode != 2) {
        sceVu0FVECTOR player_position;

        sceVu0CopyVector(player_position, CharaMain.pos);
        player_position[1] += CharaHeight(UserStatus);

        if (DistVector(player_position, destination) <= radius + 6.0f) {
            sceVu0CopyVector(hit_position, player_position);
            return SHOT_COLLISION_PLAYER;
        }
    }

    if (target_mode != 1) {
        for (int monster_no = 0; monster_no < 16; monster_no++) {
            if (NowMonstorUnit->monster[monster_no].state == -1) {
                continue;
            }

            if (monster_no >= 0 && monster_no < 17) {
                active = NowMonstorUnit->monster[monster_no].revealed;
            }

            if (active == 0) {
                continue;
            }

            for (int sphere_no = 0; sphere_no < 16; sphere_no++) {
                if (NowMonstorUnit->effect[monster_no].timer[sphere_no] != 0 && DistVector(NowMonstorUnit->effect[monster_no].position[sphere_no], position) <= radius + NowMonstorUnit->effect[monster_no].radius[sphere_no]) {
                    sceVu0CopyVector(hit_position, NowMonstorUnit->effect[monster_no].position[sphere_no]);
                    return SHOT_COLLISION_MONSTER;
                }
            }
        }
    }

    int hit = CheckHit(polygons, polygon_count, position, destination, hit_position, 1, 4);

    if (hit >= 0) {
        CCPoly *poly = &polygons[hit];
        int     field_no = poly->attr.ground_kind;

        if (field_no > 0 && field_no < 13) {
            DRAN_MAP_FIELD_SET *field_set = (DRAN_MAP_FIELD_SET *) NowDranMapField;

            if (field_set->state[field_no - 1] >= 2) {
                ((DRAN_MAP_FIELD_SET *) NowDranMapField)->state[field_no - 1]--;
            }
        }

        return SHOT_COLLISION_MAP;
    }

    sceVu0CopyVector(hit_position, position);
    return SHOT_COLLISION_NONE;
}

void set3DCellModel(float *world, char *texture_name, float size, s32 x, s32 y, s32 width, s32 height, u8 blend) {
    sceVu0IVECTOR top_left;
    sceVu0IVECTOR top_right;
    sceVu0IVECTOR bottom_left;
    sceVu0IVECTOR bottom_right;
    CTexture     *texture = TexManager.GetTexture(texture_name, -1);

    world[3] = 1.0f;

    // The sprite stands twice as wide as it is tall.
    if (MGRotTransPers3DSprite(top_left, bottom_right, world, size, size / 2.0f, 0) == 1) {
        top_right[0] = bottom_right[0];
        top_right[1] = top_left[1];
        top_right[2] = top_left[2];
        bottom_left[0] = top_left[0];
        bottom_left[1] = bottom_right[1];
        bottom_left[2] = bottom_right[2];
        set3DSprite(Vif1Packet, texture, CRect_i_(x, y, width, height), top_left, top_right, bottom_left, bottom_right, blend);
    }
}
