#pragma helper_mask_gpr 0x30
#pragma helper_mask_fpr 0x1000
#pragma name_counter 1008
#pragma argument_flag 0
#pragma argument_flag_ones 69, 109, 232, 233, 278, 307, 308, 353, 409, 424
#pragma argument_flag_ones 426, 480, 481, 497, 498, 499, 959

#include "dungeonmap.hpp"

#include <libvu0.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "camera.hpp"
#include "character.hpp"
#include "dataset.hpp"
#include "dun/gameloop.hpp"
#include "dungeonparts.hpp"
#include "frame.hpp"
#include "frameattr.hpp"
#include "framevu1.hpp"
#include "mathutil.hpp"
#include "mglib.hpp"
#include "rect.hpp"
#include "snd.hpp"
#include "texture.hpp"
#include "textureanime.hpp"
#include "userstatus.hpp"

/**
 * What a grid cell holds while a floor is being built, as bit flags.
 */
// clang-format off
enum BuildCellFlag {
    BUILD_CELL_ROOM          = 0x001, /**< The cell is part of a room's floor. */
    BUILD_CELL_CORRIDOR      = 0x002, /**< A corridor runs through the cell. */
    BUILD_CELL_DOOR          = 0x004, /**< A corridor leaves the room through the cell. */
    BUILD_CELL_HEAL_SPRING   = 0x008, /**< The cell holds a healing spring against the room's top wall. */
    BUILD_CELL_DIVIDING_WALL = 0x010, /**< A wall dividing the room runs through the cell. */
    BUILD_CELL_DIVIDING_DOOR = 0x020, /**< The gap in a dividing wall. */
    BUILD_CELL_PILLAR        = 0x040, /**< The cell holds a pillar. */
    BUILD_CELL_STAIR_DOWN    = 0x080, /**< The cell holds the stair down. */
    BUILD_CELL_STAIR_UP      = 0x100, /**< The cell holds the stair up. */
    BUILD_CELL_URA_ENTRANCE  = 0x200, /**< The cell holds the entrance to the back floor. */
    BUILD_CELL_ROOM_LINK     = 0x400, /**< The door that leads to the linked room. */
};

// clang-format on

/**
 * A single grid cell while a floor is being built.
 */
struct BUILD_MAP_INFO {
    s32 kind;      /**< MapPartsNo that the cell builds into, or while a corridor is laid, the room it leaves from. */
    s32 direction; /**< Rotation that the cell's map part uses. */
    s32 link[16];  /**< 1 for each room whose area or corridor the cell belongs to. */
    s32 content;   /**< What the cell holds, as BuildCellFlag bits. */
};

STATIC_ASSERT(sizeof(BUILD_MAP_INFO) == 0x4C);

/**
 * The four cells that touch one cell, as offsets in grid columns and rows.
 */
struct PARTS_ADJACENT {
    int offset[4][2]; /**< Column and row offset of each cell. */
};

/* Tables that translate the cells that touch a cell into the map part it
 * draws, one row of adjacency mask, map part and direction each. */

/** Gives the door that closes a corridor onto a room. */
s32 chainTableDoor[4][3] = {
    {14, MAP_PARTS_ROOM_DOOR_NORTH, 0},
    {13, MAP_PARTS_ROOM_DOOR_EAST,  0},
    {7,  MAP_PARTS_ROOM_DOOR_SOUTH, 0},
    {11, MAP_PARTS_ROOM_DOOR_WEST,  0},
};

/** Gives the corridor that joins the cells that touch a corridor cell. */
s32 chainTableRoad[15][3] = {
    {6,  MAP_PARTS_ROAD_STRAIGHT, 0},
    {9,  MAP_PARTS_ROAD_STRAIGHT, 1},
    {5,  MAP_PARTS_ROAD_CORNER,   0},
    {3,  MAP_PARTS_ROAD_CORNER,   1},
    {10, MAP_PARTS_ROAD_CORNER,   2},
    {12, MAP_PARTS_ROAD_CORNER,   3},
    {0,  MAP_PARTS_ROAD_CROSS,    0},
    {1,  MAP_PARTS_ROAD_TEE,      0},
    {2,  MAP_PARTS_ROAD_TEE,      1},
    {8,  MAP_PARTS_ROAD_TEE,      2},
    {4,  MAP_PARTS_ROAD_TEE,      3},
    {7,  MAP_PARTS_ROAD_DEAD_END, 0},
    {11, MAP_PARTS_ROAD_DEAD_END, 1},
    {14, MAP_PARTS_ROAD_DEAD_END, 2},
    {13, MAP_PARTS_ROAD_DEAD_END, 3},
};

/** Gives the wall that closes the edges of a room cell. */
s32 chainTableRoom[8][3] = {
    {1,  MAP_PARTS_ROOM_WALL_NORTH,        0},
    {2,  MAP_PARTS_ROOM_WALL_EAST,         0},
    {8,  MAP_PARTS_ROOM_WALL_SOUTH,        0},
    {4,  MAP_PARTS_ROOM_WALL_WEST,         0},
    {5,  MAP_PARTS_ROOM_CORNER_NORTH_WEST, 0},
    {3,  MAP_PARTS_ROOM_CORNER_NORTH_EAST, 0},
    {10, MAP_PARTS_ROOM_CORNER_SOUTH_EAST, 0},
    {12, MAP_PARTS_ROOM_CORNER_SOUTH_WEST, 0},
};

/** Gives the wall that divides a room. */
s32 chainTableDivid[6][3] = {
    {0xE8, MAP_PARTS_DIVIDE_END_NORTH,  0},
    {0xD4, MAP_PARTS_DIVIDE_END_EAST,   0},
    {0x71, MAP_PARTS_DIVIDE_END_SOUTH,  0},
    {0xB2, MAP_PARTS_DIVIDE_END_WEST,   0},
    {0xF9, MAP_PARTS_DIVIDE_WALL_NORTH, 0},
    {0xF6, MAP_PARTS_DIVIDE_WALL_EAST,  0},
};

/** Gives the door that opens a wall that divides a room. */
s32 chainTableDividDoor[2][3] = {
    {9, MAP_PARTS_DIVIDE_DOOR_NORTH, 0},
    {6, MAP_PARTS_DIVIDE_DOOR_EAST,  0},
};

void CDungeonMap::SetNPC(int npc_no, unsigned int *pack, int parts_no, sceVu0FVECTOR pos, sceVu0FVECTOR rot, int visible, int motion_no, CDataAlloc2<1> *alloc) {
    int i;

    if (this->npc[npc_no].used != 0) {
        return;
    }

    if (npc_no >= 4 || npc_no < 0) {
        return;
    }

    CFrameAttr attr;

    attr.fog_enable = true;

    for (i = 0; i < 32; i++) {
        BtNPCTexAnimeData[npc_no][i].Initialize();
    }

    this->npc[npc_no].chara.InitializeTexAnime(BtNPCTexAnimeData[npc_no], 32);
    this->npc[npc_no].chara.LoadPackData2(pack, info_cfg_literal, alloc, npc_no + 64, alloc, 0);

    if (this->npc[npc_no].chara.frame == NULL) {
        printf("******* NPCEntryErr\n");
    }

    this->npc[npc_no].chara.frame->SetAttr(attr, 1, 64);
    SetFrameAttr(this->npc[npc_no].chara.frame, 1);

    sceVu0CopyVector(this->npc[npc_no].pos, pos);
    sceVu0CopyVector(this->npc[npc_no].rot, rot);
    this->npc[npc_no].parts_no = parts_no;
    this->npc[npc_no].visible = visible;
    this->npc[npc_no].used = 1;
    this->npc[npc_no].motion_no = motion_no;
    this->npc[npc_no].chara.motion_no = motion_no;
    this->npc[npc_no].chara.motion_flags = 0;
    this->npc[npc_no].chara.motion_speed = -1.0f;
}

void CDungeonMap::ClearNPC_Cash() {
    for (int i = 0; i < 4; i++) {
        this->npc[i].draw_num = 0;
    }
}

void CDungeonMap::ReservNPC_Draw(int npc_no, float x, float y, float z, int direction) {
    if (this->npc[npc_no].used != 0 && this->npc[npc_no].visible != 0 && this->npc[npc_no].chara.frame != NULL) {
        this->npc[npc_no].draw_pos[this->npc[npc_no].draw_num][0] = x;
        this->npc[npc_no].draw_pos[this->npc[npc_no].draw_num][1] = y;
        this->npc[npc_no].draw_pos[this->npc[npc_no].draw_num][2] = z;
        this->npc[npc_no].draw_param[this->npc[npc_no].draw_num] = direction;
        this->npc[npc_no].draw_num++;
    }
}

void CDungeonMap::DrawNPCDraw() {
    float pos[4];
    float unused[4];
    int   i;
    int   j;

    for (i = 0; i < 4; i++) {
        for (j = 0; j < this->npc[i].draw_num; j++) {
            int   direction;
            float angle;
            float deg;
            float turn;

            if (this->npc[i].chara.frame == NULL) {
                break;
            }

            sceVu0CopyVector(pos, this->npc[i].draw_pos[j]);
            sceVu0CopyVector(unused, this->npc[i].draw_pos[j]);
            pos[0] += this->npc[i].pos[0];
            pos[1] += this->npc[i].pos[1];
            pos[2] += this->npc[i].pos[2];
            pos[3] = 1.0f;
            this->npc[i].chara.SetPosition(pos);

            direction = this->npc[i].draw_param[j];

            if (direction > 3) {
                direction -= 3;
            }

            if (direction == 3) {
                direction = -1;
            }

            deg = -90.0f * direction;
            angle = (PI * deg) / 180.0f;
            angle += this->npc[i].rot[1];

            if (!(angle <= PI)) {
                angle -= TWO_PI;
            }

            if (angle < -PI) {
                angle += TWO_PI;
            }

            this->npc[i].chara.SetRotation(0.0f, angle, 0.0f);
            TexManager.ReloadTexture(Vif1Packet, i + 64);
            this->npc[i].chara.TextureAnime(i + 64);
            this->npc[i].chara.Draw();
        }
    }
}

void CDungeonMap::StepNPC() {
    for (int i = 0; i < 4; i++) {
        if (this->npc[i].used != 0 && this->npc[i].visible != 0 && this->npc[i].chara.frame != NULL && this->npc[i].motion_no != -1) {
            this->npc[i].chara.Step();
        }
    }
}

void CDungeonMap::NPCSetMotion(int npc_no, int motion_no) {
    this->npc[npc_no].chara.motion_no = motion_no;
    this->npc[npc_no].chara.motion_flags = 0;
    this->npc[npc_no].chara.motion_speed = -1.0f;
}

void CDungeonMap::NPCSetMotion(int npc_no, int motion_no, float speed, int motion_flags) {
    this->npc[npc_no].chara.motion_no = motion_no;
    this->npc[npc_no].chara.motion_flags = motion_flags;
    this->npc[npc_no].chara.motion_speed = -1.0f;
    this->npc[npc_no].chara.motion_speed = speed;
#ifdef PAL
    this->npc[npc_no].chara.motion_speed = 6.0f * speed / 5.0f;
#endif
}

CFrame *CDungeonMap::GetFrameSearch(char *name) {
    for (int i = 0; i < 72; i++) {
        CFrame *frame = this->parts[i].GetSearchFrame(name);

        if (frame != NULL) {
            return frame;
        }
    }

    return NULL;
}

void CDungeonMap::DrawMapFreeStyle() {
    int i;
    int npc_no;

    this->ClearNPC_Cash();

    for (i = 0; this->parts[i].frame[0] != NULL; i++) {
        this->parts[i].pos[0] = 0.0;
        this->parts[i].pos[1] = 0.0;
        this->parts[i].pos[2] = 0.0;
        this->parts[i].pos[3] = 1.0;
        this->parts[i].direction = 0;
        this->parts[i].Draw();

        for (npc_no = 0; npc_no < 4; npc_no++) {
            if (this->npc[npc_no].parts_no == i) {
                this->ReservNPC_Draw(npc_no, 0.0, 0.0, 0.0, 0);
            }
        }
    }
}

void CDungeonMap::DrawMapCalc(int mode) {
    int row, col;
    int i;

    if (mode == 1) {

        for (row = 0; row < 20; row++) {
            for (col = 0; col < 20; col++) {
                int cell_no = col + row * 20;

                if (this->cells[cell_no].parts_no != MAP_PARTS_NONE) {
                    CDungeonParts *part;
                    int            dir = this->cells[cell_no].direction;

                    part = &this->parts[this->cells[cell_no].parts_no];
                    part->direction = dir;
                    part = &this->parts[this->cells[cell_no].parts_no];
                    part->pos[0] = 160.0f * col;
                    part->pos[1] = 0.0f;
                    part->pos[2] = 160.0f * row;
                    part->pos[3] = 1.0f;
                    this->parts[this->cells[cell_no].parts_no].DrawCalc(col, row, this->cells[cell_no].direction, mode);
                }
            }
        }
    } else {
        for (i = 0; this->parts[i].frame[0] != NULL; i++) {
            this->parts[i].DrawCalc(0, 0, 0, mode);
        }
    }
}

void CDungeonMap::DrawMap(CCameraFollow *camera, CFrameVu1 *player) {
    float           cam_pos[4];
    float           view_delta[4];
    float           cam_ref[4];
    float           view[4];
    float           player_pos[4];
    float           sound_pos[4];
    float           cell_delta[4];
    float           ambient[4];
    float           old_ambient[4];
    ITEM_FREE_AREA *free_area;
    float           volume;
    float           pan;
    float           dist;
    float           delta_x;
    float           nearest;
    float           world_x;
    float           world_z;
    float           delta_z;
    int             sound_no;
    int             row;
    int             col;
    int             cell_no;
    int             draw;
    int             npc_no;
    int             area_no;
    int             rect_no;
    int             player_cell_x;
    int             player_cell_y;

    ((CCamera *) camera)->GetPos(cam_pos);
    ((CCamera *) camera)->GetRef(cam_ref);
    view_delta[0] = cam_ref[0] - cam_pos[0];
    view_delta[1] = cam_ref[1] - cam_pos[1];
    view_delta[2] = cam_ref[2] - cam_pos[2];
    view_delta[3] = 1.0f;
    sceVu0Normalize(view, view_delta);
    sceVu0CopyVector(player_pos, player->position);
    player_cell_x = (int) (player_pos[0] / 160.0f);
    player_cell_y = (int) (player_pos[2] / 160.0f);
    free_area = ItemFreeAreaAll[selectMapNo];

    this->ClearNPC_Cash();
    nearest = 10000.0f;

    for (row = 0; row < 20; row++) {
        for (col = 0; col < 20; col++) {
            world_x = 160.0f * col;
            delta_x = world_x - cam_pos[0];
            world_z = 160.0f * row;
            delta_z = world_z - cam_pos[2];

            cell_delta[0] = delta_x;
            cell_delta[1] = 0.0f;
            cell_delta[2] = delta_z;
            cell_delta[3] = 0.0f;
            dist = DistVector(cell_delta);
            cell_no = col + row * 20;
            this->cells[cell_no].camera_dist = dist;

            if (this->cells[cell_no].parts_no == MAP_PARTS_NONE) {
                this->cells[cell_no].visible = false;
                continue;
            }

            if (this->cells[cell_no].parts_no == MAP_PARTS_KEY_XIAO) {
                if (UserStatus->cur_georama == 2 && dist < nearest) {
                    nearest = dist;
                    sound_pos[0] = world_x;
                    sound_pos[1] = 0.0f;
                    sound_pos[2] = world_z;
                    sound_pos[3] = 1.0f;
                    sound_no = 65;
                }
            }

            // Part 75 is unknown
            if (this->cells[cell_no].parts_no == 75 && dist < nearest) {
                nearest = dist;
                sound_pos[0] = world_x;
                sound_pos[1] = 0.0f;
                sound_pos[2] = world_z;
                sound_pos[3] = 1.0f;
                sound_no = 75;
            }

            if (!(dist < 160.0f * this->draw_dist_scale)) {
                draw = false;
            } else {
                int georama;

                cam_ref[0] = delta_x - view_delta[0];
                cam_ref[1] = 0.0f;
                cam_ref[2] = delta_z - view_delta[2];
                cam_ref[3] = 1.0f;
                sceVu0Normalize(cam_ref, cam_ref);
                float facing = sceVu0InnerProduct(view, cam_ref);
                draw = true;
                georama = UserStatus->cur_georama;

                if (georama == 5 && this->cells[cell_no].parts_no == MAP_PARTS_URA_ROAD) {
                    facing = 1.0f;
                }

                if (facing <= 0.0f && !(dist < 160.0f)) {
                    draw = false;
                }

                if (this->cells[cell_no].parts_no >= MAP_PARTS_URA_ENTRANCE_NORTH && this->cells[cell_no].parts_no <= MAP_PARTS_URA_ENTRANCE_WEST && georama == 5) {
                    draw = true;
                }
            }

            this->cells[cell_no].visible = draw;

            if (this->cells[cell_no].parts_no == MAP_PARTS_NONE) {
                continue;
            }

            if (draw != 1) {
                continue;
            }

            int            direction = this->cells[cell_no].direction;
            CDungeonParts *direction_part = &this->parts[this->cells[cell_no].parts_no];
            direction_part->direction = direction;
            {
                CDungeonParts *position_part = &this->parts[this->cells[cell_no].parts_no];
                position_part->pos[0] = world_x;
                position_part->pos[1] = 0.0f;
                position_part->pos[2] = world_z;
                position_part->pos[3] = 1.0f;
            }

            for (npc_no = 0; npc_no < 4; npc_no++) {
                if (this->npc[npc_no].parts_no == this->cells[cell_no].parts_no) {
                    this->ReservNPC_Draw(npc_no, world_x, 0.0f, world_z, this->cells[cell_no].direction);
                }
            }

            if (UserStatus->cur_georama == 4) {
                MGGetAmbient(old_ambient);
                MGGetAmbient(ambient);

                if (dist <= 480.0f) {
                    ambient[3] = 128.0f;
                } else {
                    ambient[3] = 128.0f - (dist - 480.0f);

                    if (ambient[3] < 0.0f) {
                        ambient[3] = 0.0f;
                    }
                }

                MGSetAmbient(ambient);
            }

            this->parts[this->cells[cell_no].parts_no].Draw();

            if (UserStatus->cur_georama == 4) {
                MGSetAmbient(old_ambient);
            }

            // Debug status six shows each configured free area on its map part.
            if (DebugStatus[6] != 0) {
                for (area_no = 0; free_area[area_no].parts_no != MAP_PARTS_NONE; area_no++) {
                    if (free_area[area_no].parts_no != this->cells[cell_no].parts_no) {
                        continue;
                    }

                    for (rect_no = 0; rect_no < free_area[area_no].rect_num; rect_no++) {
                        float corner[4][4];
                        int   screen[4][4];
                        int   all_visible;
                        int   corner_no;
                        int   rotation;

                        rotation = (int) (float) free_area[area_no].direction;
                        rotation = rotation + this->cells[cell_no].direction;

                        if (rotation > 3) {
                            rotation -= 4;
                        }

                        all_visible = 1;
                        float radians = (PI * ((4 - rotation) * 90)) / 180.0f;
                        float x[4];
                        float z[4];
                        float height;
                        float left = free_area[area_no].rect[rect_no].x0;
                        x[0] = 10.0f * left;
                        height = 10.0f * free_area[area_no].rect[rect_no].y0;
                        z[0] = 10.0f * free_area[area_no].rect[rect_no].z0;
                        x[3] = 10.0f * free_area[area_no].rect[rect_no].x1;
                        z[3] = 10.0f * free_area[area_no].rect[rect_no].z1;
                        x[1] = x[3];
                        z[1] = z[0];
                        x[2] = x[0];
                        z[2] = z[3];

                        for (corner_no = 0; corner_no < 4; corner_no++) {
                            corner[corner_no][0] = -z[corner_no] * sinf(radians) - x[corner_no] * cosf(radians);
                            corner[corner_no][2] = -x[corner_no] * sinf(radians) + z[corner_no] * cosf(radians);
                            corner[corner_no][0] *= -1.0f;
                            corner[corner_no][0] += world_x;
                            corner[corner_no][1] = 2.0f + height;
                            corner[corner_no][2] += world_z;
                            corner[corner_no][3] = 1.0f;

                            if (MGRotTransPers(screen[corner_no], corner[corner_no], 0) == 0) {
                                all_visible = 0;
                            }
                        }

                        if (all_visible != 0) {
                            setColSprite(Vif1Packet, screen[0], screen[1], screen[2], screen[3], 0x80, 0, 0, 0x40);
                        }
                    }
                }
            }
        }
    }

    if (nearest < 10000.0f) {
        SndGetVolPan(&volume, &pan, sound_pos, 10.0f, 500.0f);
        SndSetSeVolf(sound_no, volume, 0);
        SndSetSePanf(sound_no, pan, 0);
    }
}

void CDungeonMap::DrawBGModel(CCamera *camera) {
    float pos[4];

    // The background follows the camera, so that it stays at the horizon.
    camera->GetPos(pos);
    pos[1] = 0.0f;

    for (int i = 0; i < 6; i++) {
        if (this->bg_model[i] != NULL) {
            this->bg_model[i]->SetPosition(pos);
            MGDraw(this->bg_model[i]);
        }
    }
}

void CDungeonMap::DrawDummyModel(CCamera *camera) {
    float pos[4];

    if (this->dummy_num == 0) {
        return;
    }

    // A dummy model only draws while the camera is near enough to it.
    camera->GetPos(pos);

    for (int i = 0; i < this->dummy_num; i++) {
        if (this->dummy_frame[this->dummy_model[i]] != NULL) {
            if (DistVector(this->dummy_pos[i], pos) < 160.0f * (1.0f + this->draw_dist_scale)) {
                this->dummy_frame[this->dummy_model[i]]->SetPosition(this->dummy_pos[i]);
                MGDraw(this->dummy_frame[this->dummy_model[i]]);
            }
        }
    }
}

void CDungeonMap::DrawMiniMap(float *pos, float angle) {
    int           status;
    unsigned char red;
    int           saved_map;
    int           saved_crystal;
    unsigned char green;
    unsigned char blue;
    int           row;
    int           col;
    int           direction;
    int           map_x;
    int           map_y;
    int           cellx;
    int           visible;
    int           show;
    float         player_screen_x;
    CTexture     *map_texture;
    CTexture     *symbol_texture;
    unsigned char alpha;
    int           masked;
    int           cell_no;
    CRect_i_      map_dst;
    CRect_i_      map_src;
    CRect_i_      player_dst;
    CRect_i_      player_src;
    CRect_i_      trap_dst;
    CRect_i_      trap_src;
    CRect_i_      box_dst;
    CRect_i_      box_src;
    CRect_i_      atra_dst;
    CRect_i_      atra_src;
    status = UserStatus->minimap_status;

    if (status == 3) {
        return;
    }

    unsigned char red_table[3] = {0x20, 0x40, 0x60};
    unsigned char green_table[3] = {0x60, 0x80, 0xA2};
    unsigned char blue_table[3] = {0x40, 0x60, 0xA2};

    red = red_table[status];
    green = green_table[status];
    blue = blue_table[status];
    saved_map = BtEquipMap;
    saved_crystal = BtEquipMasuisyou;

    if (DebugStatus[3] != 0) {
        BtEquipMap = 1;
        BtEquipMasuisyou = 1;
    }

    visible = BtEquipMap ? 0 : 1;
    map_texture = TexManager.GetTexture("minimap1", -1);
    symbol_texture = TexManager.GetTexture("itempack", -1);

    for (row = 0; row < 20; row++) {
        for (col = 0; col < 20; col++) {
            cell_no = col + row * 20;
            masked = this->mask[cell_no];
            show = masked;

            if (visible == 0 && masked == 0) {
                show = 1;
                alpha = red;
            } else {
                alpha = green;
            }

            if (this->cells[cell_no].parts_no != MAP_PARTS_NONE && show == 1) {
                direction = this->cells[cell_no].direction + this->parts[this->cells[cell_no].parts_no].direction_offset;
                map_src.x = (direction % 8) * 16;
                map_src.y = (direction / 8) * 16;
                map_src.width = 16;
                map_src.height = 16;
                map_dst.x = col * 16 + 380;
                map_dst.y = row * 16 + 64;
                map_dst.width = 16;
                map_dst.height = 16;
                set2DSprite(Vif1Packet, map_texture, map_dst, map_src, alpha, alpha, alpha, blue);
            }
        }
    }

    pos[0] *= 0.1f;
    pos[2] *= 0.1f;
    player_src.x = 88;
    player_src.y = 96;
    player_src.width = 12;
    player_src.height = 12;
    map_x = (int) (pos[2] + 64.0f + 8.0f);
    player_screen_x = pos[0] + 380.0f + 8.0f;
    map_y = (int) player_screen_x;
    player_dst.x = (int) player_screen_x;
    player_dst.y = map_x;
    player_dst.width = 12;
    player_dst.height = 12;
    set2DSprite(Vif1Packet, symbol_texture, player_dst, player_src, 7, 7, angle);

    for (int i = 0; i < 3; i++) {
        int cell;

        if (this->trap_circle[i].state == 0) {
            continue;
        }

        map_x = (int) (this->trap_circle[i].pos[0] * 0.1f);
        map_y = (int) (this->trap_circle[i].pos[2] * 0.1f);
        cellx = (int) ((80.0f + this->trap_circle[i].pos[0]) / 160.0f);
        cell = cellx + (int) ((80.0f + this->trap_circle[i].pos[2]) / 160.0f) * 20;
        show = this->mask[cell];

        if (BtEquipMasuisyou == 1) {
            show = 1;
        }

        if (this->cells[cell].parts_no != MAP_PARTS_NONE && show == 1) {
            trap_src.x = 88;
            trap_src.y = 112;
            trap_src.width = 8;
            trap_src.height = 8;
            trap_dst.x = map_x + 384;
            trap_dst.y = map_y + 68;
            trap_dst.width = 8;
            trap_dst.height = 8;
            set2DSprite(Vif1Packet, symbol_texture, trap_dst, trap_src, blue);
        }
    }

    for (int i = 0; i < 24; i++) {
        int cell;

        if (this->boxes[i].used == 0 || this->boxes[i].closed == 0) {
            continue;
        }

        map_x = (int) (this->boxes[i].pos[0] * 0.1f);
        map_y = (int) (this->boxes[i].pos[2] * 0.1f);
        cellx = (int) ((80.0f + this->boxes[i].pos[0]) / 160.0f);
        cell = cellx + (int) ((80.0f + this->boxes[i].pos[2]) / 160.0f) * 20;
        show = this->mask[cell];

        if (BtEquipMasuisyou == 1) {
            show = 1;
        }

        if (this->cells[cell].parts_no != MAP_PARTS_NONE && show == 1) {
            box_src.x = 80;
            box_src.y = 96;
            box_src.width = 8;
            box_src.height = 8;
            box_dst.x = map_x + 384;
            box_dst.y = map_y + 68;
            box_dst.width = 8;
            box_dst.height = 8;
            set2DSprite(Vif1Packet, symbol_texture, box_dst, box_src, blue);
        }
    }

    for (int i = 0; i < 8; i++) {
        int cell;

        if (this->atra[i].used == 0) {
            continue;
        }

        map_x = (int) (this->atra[i].pos[0] * 0.1f);
        map_y = (int) (this->atra[i].pos[2] * 0.1f);
        cellx = (int) ((80.0f + this->atra[i].pos[0]) / 160.0f);
        cell = cellx + (int) ((80.0f + this->atra[i].pos[2]) / 160.0f) * 20;
        show = this->mask[cell];

        if (BtEquipMasuisyou == 1) {
            show = 1;
        }

        if (this->cells[cell].parts_no != MAP_PARTS_NONE && show == 1) {
            atra_src.x = 72;
            atra_src.y = 104;
            atra_src.width = 8;
            atra_src.height = 8;
            atra_dst.x = map_x + 384;
            atra_dst.y = map_y + 68;
            atra_dst.width = 8;
            atra_dst.height = 8;
            set2DSprite(Vif1Packet, symbol_texture, atra_dst, atra_src, blue);
        }
    }

    BtEquipMap = saved_map;
    BtEquipMasuisyou = saved_crystal;
}

void CDungeonMap::checkMask(float x, float z) {
    int cell_x;
    int cell_z;
    int room;

    room = -1;
    cell_x = (int) ((80.0f + x) / 160.0f);
    cell_z = (int) ((80.0f + z) / 160.0f);

    // Find the room that holds the cell that the player stands in.
    for (int i = 0; i < this->room_num; i++) {
        if (cell_x >= this->rooms[i].x && cell_x < this->rooms[i].x + this->rooms[i].width) {
            if (cell_z >= this->rooms[i].y && cell_z < this->rooms[i].y + this->rooms[i].height) {
                room = i;
            }
        }
    }

    // Mark the entire room as seen when the player enters it for the first time.
    if (room != -1) {
        if (this->room_seen[room] == 0) {
            if (this->rooms[room].width != 0) {
                int col;
                int row;

                this->room_seen[room] = 1;

                for (row = this->rooms[room].y; row < this->rooms[room].y + this->rooms[room].height; row++) {
                    for (col = this->rooms[room].x; col < this->rooms[room].x + this->rooms[room].width; col++) {
                        this->mask[col + row * 20] = 1;
                    }
                }
            }
        }
    }

    this->mask[cell_x + cell_z * 20] = 1;
}

void CDungeonMap::FlushCheckMask() {
    int i;

    // Hide every cell of the mini map and forget every room that the player found.
    for (i = 0; i < 400; i++) {
        this->mask[i] = 0;
    }

    for (i = 0; i < 16; i++) {
        this->room_seen[i] = 0;
    }

    // The entrance should always show on the mini map.
    for (i = 0; i < 400; i++) {
        if (this->cells[i].parts_no == MAP_PARTS_STAIR_DOWN) {
            this->mask[i] = 1;
        }
    }
}

/**
 * Draws free-style fires and updates the sound for the nearest active fire.
 */
void CDungeonMap::DrawFireFreeStyle(CFrameVu1 *frame, CCameraFollow *camera) {
    float fire_pos[4];
    float part_pos[4];
    float cam_pos[4];
    float best_pos[4];
    float frame_pos[4];
    float vol;
    float pan;
    float near_dist;
    int   j;
    int   i;

    sceVu0CopyVector(frame_pos, frame->position);
    ((CCamera *) camera)->GetPos(cam_pos);
    sceVu0ScaleVectorXYZ(cam_pos, cam_pos, 0.1f);

    this->fire.SetTexture(TexManager.GetTexture("lightling", -1), TexManager.GetTexture("blender", -1));
    this->fire.FireCreate();

    near_dist = 16000.0f;

    for (i = 0; this->parts[i].frame[0] != NULL; i++) {
        sceVu0CopyVector(part_pos, this->parts[i].frame_offset[0]);

        for (j = 0; j < this->parts[i].fire_num; j++) {
            float angle;
            float turn;
            float rot_x;
            float rot_z;
            float dist;
            float world_z;
            float world_y;
            int   param;

            fire_pos[0] = this->parts[i].fire_pos[j][0];
            fire_pos[1] = this->parts[i].fire_pos[j][1];
            fire_pos[2] = this->parts[i].fire_pos[j][2];
            fire_pos[3] = 1.0f;
            param = this->parts[i].fire_param[j];

            // The point turns with the part that holds it.
            angle = this->parts[i].frame_turn[0];

            if (angle != 0.0f) {
                angle = (PI * (90.0f * (4.0f - angle))) / 180.0f;
                turn = -fire_pos[2] * sinf(angle);
                rot_x = turn - fire_pos[0] * cosf(angle);
                turn = -fire_pos[0] * sinf(angle);
                rot_z = turn + fire_pos[2] * cosf(angle);
                rot_x *= -1.0f;
            } else {
                rot_x = fire_pos[0];
                rot_z = fire_pos[2];
            }

            fire_pos[0] = part_pos[0] / 10.0f + rot_x;
            fire_pos[1] += part_pos[1] / 10.0f;
            fire_pos[2] = world_z = part_pos[2] / 10.0f + rot_z;
            world_y = fire_pos[1];

            this->fire.pos[0] = 10.0f * fire_pos[0];
            this->fire.pos[1] = 10.0f * world_y;
            this->fire.pos[2] = 10.0f * world_z;
            this->fire.pos[3] = 1.0f;
            this->fire.DrawFire(1, 1, (CCamera *) camera, frame_pos, 1.0f, param, 15.0f);

            // The crackle comes from the fire that is nearest to the camera.
            dist = DistVector(cam_pos, fire_pos);

            if (!(near_dist < dist)) {
                near_dist = dist;
                sceVu0CopyVector(best_pos, fire_pos);
                sceVu0ScaleVectorXYZ(best_pos, best_pos, 10.0f);
            }
        }
    }

    if (near_dist <= 30.0f) {
        SndGetVolPan(&vol, &pan, best_pos, 10.0f, 500.0f);
        SndSetSeVolf(SE_AMBIENT_CAVE_DRIPS, vol, 0);
        SndSetSePanf(53, pan, 0);
    } else {
        SndSetSeVol(53, 0, 0);
    }
}

void CDungeonMap::DrawFire(CFrameVu1 *frame, CCameraFollow *camera) {
    float fire_pos[4];
    float frame_pos[4];
    float cam_near[4];
    float cam_pos[4];
    float best_pos[4];
    float frame_near[4];
    float vol;
    float pan;
    float near_dist;
    int   cam_col;
    int   cam_row;
    int   j;
    int   col;
    int   row;
    int   param;

    sceVu0CopyVector(frame_pos, frame->position);
    ((CCamera *) camera)->GetPos(cam_pos);
    ((CCamera *) camera)->GetPos(cam_near);
    sceVu0ScaleVectorXYZ(cam_near, cam_near, 0.1f);

    this->fire.SetTexture(TexManager.GetTexture("lightling", -1), TexManager.GetTexture("blender", -1));
    this->fire.FireCreate();

    sceVu0CopyVector(frame_near, frame->position);
    frame_near[0] /= 10.0f;
    frame_near[1] /= 10.0f;
    frame_near[2] /= 10.0f;

    cam_col = (int) (cam_pos[0] / 160.0f);
    cam_row = (int) (cam_pos[2] / 160.0f);
    near_dist = 16000.0f;

    // Only the cells around the camera show their fires.
    for (row = cam_row - 5; row < cam_row + 5; row++) {
        for (col = cam_col - 5; col < cam_col + 5; col++) {
            if (row >= 0 && row < 20 && col >= 0 && col < 20) {
                int cell_no = col + row * 20;

                if (this->cells[cell_no].parts_no != MAP_PARTS_NONE && this->cells[cell_no].camera_dist <= 160.0f * (3.0f + this->draw_dist_scale)) {
                    CDungeonParts *part;

                    for (j = 0; j < (part = &this->parts[this->cells[cell_no].parts_no])->fire_num; j++) {
                        float turn;
                        float rot_x;
                        float rot_z;
                        float height;
                        float angle;
                        float dist;

                        fire_pos[0] = part->fire_pos[j][0];
                        fire_pos[1] = part->fire_pos[j][1];
                        fire_pos[2] = part->fire_pos[j][2];
                        fire_pos[3] = 1.0f;
                        param = part->fire_param[j];

                        // The point turns with the part that holds it.
                        angle = (PI * (float) ((4 - this->cells[cell_no].direction) * 90)) / 180.0f;

                        if (this->cells[cell_no].direction != 0) {
                            turn = -fire_pos[2] * sinf(angle);
                            rot_x = turn - fire_pos[0] * cosf(angle);
                            turn = -fire_pos[0] * sinf(angle);
                            rot_z = turn + fire_pos[2] * cosf(angle);
                            rot_x *= -1.0f;
                        } else {
                            rot_x = fire_pos[0];
                            rot_z = fire_pos[2];
                        }

                        fire_pos[0] = rot_x + 16.0f * (float) col;
                        rot_z += 16.0f * (float) row;
                        fire_pos[2] = rot_z;
                        height = fire_pos[1];

                        this->fire.pos[0] = 10.0f * fire_pos[0];
                        this->fire.pos[1] = 10.0f * height;
                        this->fire.pos[2] = 10.0f * rot_z;
                        this->fire.pos[3] = 1.0f;
                        this->fire.DrawFire(1, 1, (CCamera *) camera, frame_pos, 1.0f, param, 15.0f);

                        // The crackle comes from the fire that is nearest to the camera.
                        dist = DistVector(cam_near, fire_pos);

                        if (!(near_dist < dist)) {
                            near_dist = dist;
                            sceVu0CopyVector(best_pos, fire_pos);
                            sceVu0ScaleVectorXYZ(best_pos, best_pos, 10.0f);
                        }
                    }
                }
            }
        }
    }

    if (near_dist <= 30.0f) {
        SndGetVolPan(&vol, &pan, best_pos, 10.0f, 500.0f);
        SndSetSeVolf(SE_AMBIENT_CAVE_DRIPS, vol, 0);
        SndSetSePanf(53, pan, 0);
    } else {
        SndSetSeVol(53, 0, 0);
    }
}

/* 189 of 202 instructions. Retail's loop test forms `&parts[parts_no]` and the
 * body reads the fire positions at 0x20 from it; mwcc carries only the scaled
 * index across the test and folds the 0x490 into the load displacement. A
 * `CDungeonParts *` local recovers the 0x20 offsets but forms the base inside
 * the body, which trades three differences for four. */
void CDungeonMap::DrawRaster(CFrameVu1 *frame) {
    float pos[4];
    int   i;
    int   col;
    int   row;
    int   cell_z;
    int   cell_x;

    sceVu0CopyVector(pos, frame->position);
    cell_x = (int) (pos[0] / 160.0f);
    cell_z = (int) (pos[2] / 160.0f);

    // Only the cells around the player cast a raster.
    for (row = cell_z - 4; row < cell_z + 4; row++) {
        for (col = cell_x - 4; col < cell_x + 4; col++) {
            if (row >= 0 && row < 20 && col >= 0 && col < 20) {
                int cell_no = col + row * 20;

                if (this->cells[cell_no].parts_no != MAP_PARTS_NONE && this->cells[cell_no].camera_dist <= 240.0f && this->cells[cell_no].visible == 1) {
                    CDungeonParts *part;

                    for (i = 0; i < (part = &this->parts[this->cells[cell_no].parts_no])->fire_num; i++) {
                        float x;
                        float y;
                        float z;
                        float rot_x;
                        float rot_z;
                        float angle;
                        int   dir;

                        x = part->fire_pos[i][0];
                        y = part->fire_pos[i][1];
                        z = part->fire_pos[i][2];
                        // The point turns with the part that holds it.
                        dir = this->cells[cell_no].direction;
                        angle = (PI * (float) ((4 - dir) * 90)) / 180.0f;

                        if (dir != 0) {
                            rot_x = -z * sinf(angle) - x * cosf(angle);
                            rot_z = -x * sinf(angle) + z * cosf(angle);
                            rot_x *= -1.0f;
                        } else {
                            rot_x = x;
                            rot_z = z;
                        }

                        x = rot_x + 16.0f * (float) col;
                        z = rot_z + 16.0f * (float) row;

                        this->fire.pos[0] = 10.0f * x;
                        this->fire.pos[1] = 10.0f * y;
                        this->fire.pos[2] = 10.0f * z;
                        this->fire.pos[3] = 1.0f;
                        this->fire.DrawRaster();
                    }
                }
            }
        }
    }
}

void CDungeonMap::DrawWater(float *pos, int mute) {
    static int  wait;
    static char init;

    float          vec[4];
    float          best_pos[4];
    float          vol;
    float          pan;
    float          near_dist;
    float          sound_dist;
    int            shake;
    int            col;
    int            row;
    PARTS_WATER   *best;
    PARTS_WATER   *water;
    CDungeonParts *part;
    int            best_col;
    int            best_row;

    best = NULL;
    near_dist = 320.0f;
    sound_dist = 300.0f;
    shake = false;

    // The surface only shakes once every thirty frames.
    if (init == 0) {
        wait = 0;
        init = 1;
    }

    if (wait >= 30) {
        wait = 0;
        shake = true;
    } else {
        wait++;
    }

    // One surface draws for the whole floor: the one nearest to the player.
    for (row = 0; row < 16; row++) {
        for (col = 0; col < 16; col++) {
            int   cell_no = col + row * 20;
            float dist;

            if (this->cells[cell_no].parts_no == MAP_PARTS_NONE) {
                continue;
            }

            part = &this->parts[this->cells[cell_no].parts_no];
            water = &part->water;

            if (this->parts[this->cells[cell_no].parts_no].water.used == 0) {
                continue;
            }

            vec[0] = 160.0f * (float) col;
            vec[1] = water->vertex[0][1];
            vec[2] = 160.0f * (float) row;
            dist = DistVector(pos, vec);

            if (dist < near_dist) {
                best = water;
                near_dist = dist;
                best_pos[0] = vec[0];
                best_pos[1] = vec[1];
                best_pos[2] = vec[2];
                best_col = col;
                best_row = row;
            }

            if (sound_dist >= dist) {
                sound_dist = dist;
            }
        }
    }

    if (best != NULL) {
        this->water.SetVertex(best->vertex[0], best->vertex[1], best->vertex[2], best->vertex[3]);
        this->water.frame.SetPosition(best_pos);
        u8 red = best->red;
        u8 green = best->green;
        u8 blue = best->blue;

        this->water.SetColor(red, green, blue, 0x80);

        if (shake != 0) {
            if (selectMapNo == DUNGEON_DIVINE_BEAST_CAVE && selectMapNo == DUNGEON_DEMON_SHAFT) {
                float height = 1.0f;

                this->water.SetParam(0.12f, 0.018f, height, 11.0f);
            } else {
                float speed = 11.0f;

                this->water.SetParam(0.12f, 0.018f, 0.0f, speed);
            }

            this->water.Shake(16, 4, 2.15f);
        }

        this->water.Hamon();
        DrawVu1__6CWaterFP10RenderInfoP13sceVif1PacketP1(&this->water, &mgRenderInfo, Vif1Packet, NULL);
    }

    if (best != NULL) {

        // A waterfall stands at the cell that the surface came from.
        if (best->has_fall != 0 && this->fall_model != NULL) {
            float x = 160.0f * (float) best_col;
            float z = 160.0f * (float) best_row;

            this->fall_model->SetPosition(x, 0.0f, z);
            MGDraw(this->fall_model);
        }

        if (sound_dist < 300.0f) {
            SndGetVolPan(&vol, &pan, best_pos, 50.0f, 300.0f);

            if (mute != 0) {
                vol = 0.0f;
            }

            SndSetSeVolf(SE_AMBIENT_DUNGEON_HISS, vol, 0);
            SndSetSePanf(61, pan, 0);
        } else {
            SndSetSeVol(61, 0, 0);
        }
    } else {
        float vol;
        float pan;

        if (sound_dist < 300.0f) {
            SndGetVolPan(&vol, &pan, best_pos, 50.0f, 300.0f);

            if (mute != 0) {
                vol = 0.0f;
            }

            SndSetSeVolf(SE_AMBIENT_TOWN_NOISE, vol, 0);
            SndSetSePanf(52, pan, 0);
        } else {
            SndSetSeVol(52, 0, 0);
        }
    }
}

void CDungeonMap::DrawItemBox(float *pos) {
    float lid[4];
    int   i;
    float angle;

    if (this->box_lid_model == NULL || this->box_body_model == NULL) {
        return;
    }

    if (this->chest_body_model == NULL || this->chest_lid_model == NULL) {
        return;
    }

    for (i = 0; i < 24; i++) {
        if (this->boxes[i].used == 0) {
            continue;
        }

        if (!(DistVector(this->boxes[i].pos, pos) <= 160.0f * this->draw_dist_scale)) {
            continue;
        }

        // The lid sits on the box, tilted by however far it has opened.
        switch (this->boxes[i].kind) {
            case TREASURE_BOX_LARGE:
                this->box_body_model->SetPosition(this->boxes[i].pos);
                sceVu0CopyVector(lid, this->box_body_model->position);
                lid[1] += 8.0f;
                lid[2] -= 5.0f;
                this->box_lid_model->SetPosition(lid);
                angle = this->boxes[i].lid_angle;
                this->box_lid_model->SetRotation((PI * angle) / 180.0f, 0.0f, 0.0f);
                MGDraw(this->box_body_model);
                MGDraw(this->box_lid_model);
                break;
            case TREASURE_BOX_SMALL:
                this->chest_body_model->SetPosition(this->boxes[i].pos);
                sceVu0CopyVector(lid, this->chest_body_model->position);
                lid[1] += 3.0f;
                lid[2] -= 3.0f;
                this->chest_lid_model->SetPosition(lid);
                angle = this->boxes[i].lid_angle;
                this->chest_lid_model->SetRotation((PI * angle) / 180.0f, 0.0f, 0.0f);
                MGDraw(this->chest_body_model);
                MGDraw(this->chest_lid_model);
                break;
        }
    }
}

void CDungeonMap::DrawAtraBoll(float *pos) {
    float draw_pos[4];

    if (this->atra_model == NULL) {
        return;
    }

    for (int i = 0; i < this->atra_num; i++) {
        if (this->atra[i].used != 0) {
            if (DistVector(this->atra[i].pos, pos) <= 160.0f * this->draw_dist_scale) {
                sceVu0CopyVector(draw_pos, this->atra[i].pos);

                // The ball bobs up and down on the spot.
                draw_pos[1] += sinf(this->atra[i].phase);
                this->atra_model->SetPosition(draw_pos);
                MGDraw(this->atra_model);

                if (this->atra[i].phase <= 360.0f) {
                    this->atra[i].phase += 0.1f;
                } else {
                    this->atra[i].phase = 0.0f;
                }
            }
        }
    }
}

int CDungeonMap::CreateCollision(CCPoly *poly, CBoxVu0 box, int num) {
    float   pos[4];
    CFrame *frame;

    sceVu0CopyVector(pos, CharaMain.frame->position);
    frame = this->collision_model;

    // Retail returns no value here, which leaves whatever v0 happens to hold.
    if (frame == NULL) {
        return;
    }

    for (int i = 0; i < this->atra_num; i++) {
        if (this->atra[i].used != 0) {
            if (DistVector(this->atra[i].pos, pos) <= 400.0f) {
                frame->SetPosition(this->atra[i].pos);
                num += frame->PickUpNearPoly(&poly[num], box);
            }
        }
    }

    return num;
}

/** Labels each room while a special linked room is selected. */
s32 scanRoomInfoWork[256];

/** Gives the 20 x 20 grid that the map builder works on. */
BUILD_MAP_INFO buildMapDat[400];

/** Takes a corridor that joinRoom lays out before it is committed. */
BUILD_MAP_INFO buildMapWork[400];

/** Gives the position and the extent of each room that the builder placed. */
ROOM_INFO roomStack[16];

/** Gives the number of rooms that the builder placed. */
s32 roomStackCnt;

static int buildRoom(int x, int y, int w, int h, int room_no) {
    int ok = 1;
    int i, j;

    // The room needs its own cells and a border of one cell around them, so
    // that two rooms never come out sharing a wall.
    for (j = -1; j < h + 2; j++) {
        for (i = -1; i < w + 2; i++) {
            if (i + x >= 0 && i + x < 20 && j + y >= 0 && j + y < 20) {
                if (buildMapDat[x + (j + y) * 20 + i].kind != MAP_PARTS_NONE) {
                    ok = 0;
                }
            }
        }
    }

    if (!ok) {
        return ok;
    }

    for (j = 0; j < h; j++) {
        for (i = 0; i < w; i++) {
            buildMapDat[x + (y + j) * 20 + i].kind = MAP_PARTS_ROOM_FLOOR;
            buildMapDat[x + (y + j) * 20 + i].link[room_no] = 1;
            buildMapDat[x + (y + j) * 20 + i].content |= BUILD_CELL_ROOM;
        }
    }

    return 1;
}

/**
 * Converts the builder flags and adjacency data into drawable map parts.
 */
void mapPartsFilter() {
    int col;
    int row;
    int room_no;
    int table_no;
    int north;
    int south;
    int east;
    int west;

    for (row = 0; row < 20; row++) {
        for (col = 0; col < 20; col++) {
            // A corridor cell joins every neighbour that shares one of its rooms.
            if ((buildMapDat[col + row * 20].content & BUILD_CELL_CORRIDOR) == BUILD_CELL_CORRIDOR) {
                int mask = 15;
                int north = buildMapDat[col + (row - 1) * 20].content;
                int south = buildMapDat[col + (row + 1) * 20].content;
                int east = buildMapDat[col + row * 20 + 1].content;
                int west = buildMapDat[col + row * 20 - 1].content;

                int allow_north = 1;

                if ((north & (BUILD_CELL_ROOM | BUILD_CELL_DOOR)) == BUILD_CELL_ROOM) {
                    allow_north = 0;
                }

                if ((north & (BUILD_CELL_STAIR_DOWN | BUILD_CELL_STAIR_UP)) != 0 && buildMapDat[col + (row - 1) * 20].direction != 0) {
                    allow_north = 0;
                }

                int allow_south = 1;

                if ((south & (BUILD_CELL_ROOM | BUILD_CELL_DOOR)) == BUILD_CELL_ROOM) {
                    allow_south = 0;
                }

                if ((south & (BUILD_CELL_STAIR_DOWN | BUILD_CELL_STAIR_UP)) != 0 && buildMapDat[col + (row + 1) * 20].direction != 2) {
                    allow_south = 0;
                }

                int allow_east = 1;

                if ((east & (BUILD_CELL_ROOM | BUILD_CELL_DOOR)) == BUILD_CELL_ROOM) {
                    allow_east = 0;
                }

                if ((east & (BUILD_CELL_STAIR_DOWN | BUILD_CELL_STAIR_UP)) != 0 && buildMapDat[col + row * 20 + 1].direction != 1) {
                    allow_east = 0;
                }

                int allow_west = 1;

                if ((west & (BUILD_CELL_ROOM | BUILD_CELL_DOOR)) == BUILD_CELL_ROOM) {
                    allow_west = 0;
                }

                if ((west & (BUILD_CELL_STAIR_DOWN | BUILD_CELL_STAIR_UP)) != 0 && buildMapDat[col + row * 20 - 1].direction != 3) {
                    allow_west = 0;
                }

                for (room_no = 0; room_no < 16; room_no++) {
                    if (buildMapDat[col + row * 20].link[room_no] == 1) {
                        if (buildMapDat[col + (row - 1) * 20].link[room_no] == 1 && allow_north != 0) {
                            mask &= ~1;
                        }

                        if (buildMapDat[col + (row + 1) * 20].link[room_no] == 1 && allow_south != 0) {
                            mask &= ~8;
                        }

                        if (buildMapDat[col + row * 20 + 1].link[room_no] == 1 && allow_east != 0) {
                            mask &= ~2;
                        }

                        if (buildMapDat[col + row * 20 - 1].link[room_no] == 1 && allow_west != 0) {
                            mask &= ~4;
                        }
                    }
                }

                for (table_no = 0; table_no < 15; table_no++) {
                    if (mask == chainTableRoad[table_no][0]) {
                        buildMapDat[col + row * 20].kind = chainTableRoad[table_no][1];
                        buildMapDat[col + row * 20].direction = chainTableRoad[table_no][2];
                    }
                }
            }

            // A room cell joins the neighbouring room cells.
            if ((buildMapDat[col + row * 20].content & BUILD_CELL_ROOM) == BUILD_CELL_ROOM) {
                int mask = 15;
                north = row != 0 ? buildMapDat[col + (row - 1) * 20].content & BUILD_CELL_ROOM : 0;
                south = row != 19 ? buildMapDat[col + (row + 1) * 20].content & BUILD_CELL_ROOM : 0;
                east = col != 19 ? buildMapDat[col + row * 20 + 1].content & BUILD_CELL_ROOM : 0;
                west = col != 0 ? buildMapDat[col + row * 20 - 1].content & BUILD_CELL_ROOM : 0;

                if (north == BUILD_CELL_ROOM) {
                    mask &= ~1;
                }

                if (south == BUILD_CELL_ROOM) {
                    mask &= ~8;
                }

                if (east == BUILD_CELL_ROOM) {
                    mask &= ~2;
                }

                if (west == BUILD_CELL_ROOM) {
                    mask &= ~4;
                }

                for (table_no = 0; table_no < 8; table_no++) {
                    if (mask == chainTableRoom[table_no][0]) {
                        buildMapDat[col + row * 20].kind = chainTableRoom[table_no][1];
                        buildMapDat[col + row * 20].direction = chainTableRoom[table_no][2];
                    }
                }
            }

            // A door faces the room cell beside it.
            if ((buildMapDat[col + row * 20].content & (BUILD_CELL_DOOR | BUILD_CELL_ROOM_LINK)) == BUILD_CELL_DOOR) {
                int mask = 0;

                if ((buildMapDat[col + (row - 1) * 20].content & BUILD_CELL_ROOM) == BUILD_CELL_ROOM) {
                    mask |= 1;
                }

                if ((buildMapDat[col + (row + 1) * 20].content & BUILD_CELL_ROOM) == BUILD_CELL_ROOM) {
                    mask |= 8;
                }

                if ((buildMapDat[col + row * 20 + 1].content & BUILD_CELL_ROOM) == BUILD_CELL_ROOM) {
                    mask |= 2;
                }

                if ((buildMapDat[col + row * 20 - 1].content & BUILD_CELL_ROOM) == BUILD_CELL_ROOM) {
                    mask |= 4;
                }

                for (table_no = 0; table_no < 4; table_no++) {
                    if (mask == chainTableDoor[table_no][0]) {
                        buildMapDat[col + row * 20].kind = chainTableDoor[table_no][1];
                    }
                }
            }

            if ((buildMapDat[col + row * 20].content & BUILD_CELL_HEAL_SPRING) == BUILD_CELL_HEAL_SPRING) {
                buildMapDat[col + row * 20].kind = MAP_PARTS_HEAL_SPRING_NORTH;
                buildMapDat[col + row * 20].direction = 0;
            }

            if ((buildMapDat[col + row * 20].content & BUILD_CELL_PILLAR) == BUILD_CELL_PILLAR) {
                if ((100.0f * (float) rand()) / 2147483648.0f <= 50.0f) {
                    buildMapDat[col + row * 20].kind = MAP_PARTS_PILLAR_1;
                } else {
                    buildMapDat[col + row * 20].kind = MAP_PARTS_PILLAR_2;
                }

                buildMapDat[col + row * 20].direction = 0;
            }

            // A dividing wall joins the walls and rooms around it.
            if ((buildMapDat[col + row * 20].content & BUILD_CELL_DIVIDING_WALL) == BUILD_CELL_DIVIDING_WALL) {
                int mask = 0;

                north = south = east = west = 0;

                if (row != 0) {
                    north = buildMapDat[col + (row - 1) * 20].content;
                }

                if (row != 19) {
                    south = buildMapDat[col + (row + 1) * 20].content;
                }

                if (col != 19) {
                    east = buildMapDat[col + row * 20 + 1].content;
                }

                if (col != 0) {
                    west = buildMapDat[col + row * 20 - 1].content;
                }

                if ((north & BUILD_CELL_DIVIDING_WALL) == BUILD_CELL_DIVIDING_WALL) {
                    mask |= 1;
                }

                if ((south & BUILD_CELL_DIVIDING_WALL) == BUILD_CELL_DIVIDING_WALL) {
                    mask |= 8;
                }

                if ((east & BUILD_CELL_DIVIDING_WALL) == BUILD_CELL_DIVIDING_WALL) {
                    mask |= 2;
                }

                if ((west & BUILD_CELL_DIVIDING_WALL) == BUILD_CELL_DIVIDING_WALL) {
                    mask |= 4;
                }

                if ((north & BUILD_CELL_ROOM) == BUILD_CELL_ROOM) {
                    mask |= 0x10;
                }

                if ((south & BUILD_CELL_ROOM) == BUILD_CELL_ROOM) {
                    mask |= 0x80;
                }

                if ((east & BUILD_CELL_ROOM) == BUILD_CELL_ROOM) {
                    mask |= 0x20;
                }

                if ((west & BUILD_CELL_ROOM) == BUILD_CELL_ROOM) {
                    mask |= 0x40;
                }

                for (table_no = 0; table_no < 6; table_no++) {
                    if (mask == chainTableDivid[table_no][0]) {
                        buildMapDat[col + row * 20].kind = chainTableDivid[table_no][1];
                    }
                }
            }

            if ((buildMapDat[col + row * 20].content & BUILD_CELL_DIVIDING_DOOR) == BUILD_CELL_DIVIDING_DOOR) {
                int mask = 0;

                north = south = east = west = 0;

                if (row != 0) {
                    north = buildMapDat[col + (row - 1) * 20].content;
                }

                if (row != 19) {
                    south = buildMapDat[col + (row + 1) * 20].content;
                }

                if (col != 19) {
                    east = buildMapDat[col + row * 20 + 1].content;
                }

                if (col != 0) {
                    west = buildMapDat[col + row * 20 - 1].content;
                }

                if ((north & BUILD_CELL_DIVIDING_WALL) == BUILD_CELL_DIVIDING_WALL) {
                    mask |= 1;
                }

                if ((south & BUILD_CELL_DIVIDING_WALL) == BUILD_CELL_DIVIDING_WALL) {
                    mask |= 8;
                }

                if ((east & BUILD_CELL_DIVIDING_WALL) == BUILD_CELL_DIVIDING_WALL) {
                    mask |= 2;
                }

                if ((west & BUILD_CELL_DIVIDING_WALL) == BUILD_CELL_DIVIDING_WALL) {
                    mask |= 4;
                }

                for (table_no = 0; table_no < 2; table_no++) {
                    if (mask == chainTableDividDoor[table_no][0]) {
                        buildMapDat[col + row * 20].kind = chainTableDividDoor[table_no][1];
                    }
                }
            }

            if ((buildMapDat[col + row * 20].content & BUILD_CELL_STAIR_DOWN) == BUILD_CELL_STAIR_DOWN) {
                buildMapDat[col + row * 20].kind = MAP_PARTS_STAIR_DOWN;
            }

            if ((buildMapDat[col + row * 20].content & BUILD_CELL_STAIR_UP) == BUILD_CELL_STAIR_UP) {
                buildMapDat[col + row * 20].kind = MAP_PARTS_STAIR_UP;
            }

            if ((buildMapDat[col + row * 20].content & BUILD_CELL_URA_ENTRANCE) == BUILD_CELL_URA_ENTRANCE) {
                buildMapDat[col + row * 20].kind = MAP_PARTS_URA_ENTRANCE_NORTH;
            }
        }
    }
}

static void copyMapInfo(BUILD_MAP_INFO *dst, BUILD_MAP_INFO *src) {
    for (int i = 0; i < 400; i++) {
        dst[i].kind = src[i].kind;

        for (int j = 0; j < 16; j++) {
            dst[i].link[j] = src[i].link[j];
        }

        dst[i].content = src[i].content;
    }
}

/**
 * Joins two rooms with a corridor.
 *
 * @mangled joinRoom__Fii
 * @address 0x1C61C0
 * @size 0x878
 */
int joinRoom(int from_room, int to_room) {
    int          done;
    int          target_x;
    int          target_y;
    int          dir;
    int          distance;
    int          step;
    int          i;
    int          cx;
    int          cy;
    int          steps;
    register int dx;
    register int dy;

    copyMapInfo(buildMapWork, buildMapDat);

    // The corridor leaves the room by whichever side faces the other room.
    dx = roomStack[from_room].x - roomStack[to_room].x;
    dy = roomStack[from_room].y - roomStack[to_room].y;

    if (dx * dx > dy * dy) {
        if (dx < 0) {
            dir = MAP_DIR_EAST;
        } else {
            dir = MAP_DIR_WEST;
        }
    } else {
        if (dy < 0) {
            dir = MAP_DIR_SOUTH;
        } else {
            dir = MAP_DIR_NORTH;
        }
    }

    switch (dir) {
        case MAP_DIR_NORTH:
            cx = roomStack[from_room].x + (roomStack[from_room].width >> 1);
            cy = roomStack[from_room].y - 1;
            buildMapWork[cx + (cy + 1) * 20].content |= BUILD_CELL_DOOR;
            buildMapWork[cx + (cy + 1) * 20].link[from_room] = 1;
            steps = roomStack[from_room].y - (roomStack[to_room].y + roomStack[to_room].height);
            break;
        case MAP_DIR_SOUTH:
            cx = roomStack[from_room].x + (roomStack[from_room].width >> 1);
            buildMapWork[cx + (roomStack[from_room].y + roomStack[from_room].height - 1) * 20].content |= BUILD_CELL_DOOR;
            buildMapWork[cx + (roomStack[from_room].y + roomStack[from_room].height - 1) * 20].link[from_room] = 1;
            cy = roomStack[from_room].y + roomStack[from_room].height;
            steps = roomStack[to_room].y - cy;
            break;
        case MAP_DIR_EAST:
            cx = roomStack[from_room].x + roomStack[from_room].width;
            cy = roomStack[from_room].y + (roomStack[from_room].height >> 1);
            buildMapWork[cx + cy * 20 - 1].content |= BUILD_CELL_DOOR;
            buildMapWork[cx + cy * 20 - 1].link[from_room] = 1;
            steps = roomStack[to_room].x - (roomStack[from_room].x + roomStack[from_room].width);
            break;
        case MAP_DIR_WEST:
            cx = roomStack[from_room].x - 1;
            cy = roomStack[from_room].y + (roomStack[from_room].height >> 1);
            buildMapWork[cx + cy * 20 + 1].content |= BUILD_CELL_DOOR;
            buildMapWork[cx + cy * 20 + 1].link[from_room] = 1;
            steps = roomStack[from_room].x - (roomStack[to_room].x + roomStack[to_room].width);
            break;
    }

    target_x = roomStack[to_room].x + (roomStack[to_room].width >> 1);
    target_y = roomStack[to_room].y + (roomStack[to_room].height >> 1);
    steps = (steps >> 1) + 1;

    // Two rooms that share a wall still need a cell between them.
    if (roomStack[from_room].y == roomStack[to_room].y + roomStack[to_room].height) {
        if (dir == MAP_DIR_NORTH || dir == MAP_DIR_SOUTH) {
            if (steps < 2) {
                steps = 2;
            }
        }
    }

    if (roomStack[to_room].y == roomStack[from_room].y + roomStack[from_room].height) {
        if (dir == MAP_DIR_NORTH || dir == MAP_DIR_SOUTH) {
            if (steps < 2) {
                steps = 2;
            }
        }
    }

    done = 0;

    while (done == 0) {
        if (steps != 0) {
            if (buildMapWork[cx + cy * 20].kind == MAP_PARTS_NONE) {
                buildMapWork[cx + cy * 20].kind = from_room;
                buildMapWork[cx + cy * 20].link[from_room] = 1;
                buildMapWork[cx + cy * 20].content |= BUILD_CELL_CORRIDOR;
                steps--;

                if (steps != 0) {
                    switch (dir) {
                        case MAP_DIR_NORTH:
                            cy--;
                            break;
                        case MAP_DIR_SOUTH:
                            cy++;
                            break;
                        case MAP_DIR_EAST:
                            cx++;
                            break;
                        case MAP_DIR_WEST:
                            cx--;
                            break;
                    }
                }
            } else {
                if ((buildMapWork[cx + cy * 20].content & BUILD_CELL_ROOM) == BUILD_CELL_ROOM) {
                    buildMapWork[cx + cy * 20].content |= BUILD_CELL_DOOR;
                    buildMapWork[cx + cy * 20].link[from_room] = 1;
                } else {
                    buildMapWork[cx + cy * 20].link[from_room] = 1;
                }

                done = 1;
            }
        } else {

            // The corridor has come far enough out; turn and run at the room.
            switch (dir) {
                case MAP_DIR_NORTH:
                case MAP_DIR_SOUTH:
                    distance = target_x - cx;

                    if (distance < 0) {
                        distance = -distance;
                        step = -1;
                    } else {
                        step = 1;
                    }

                    for (i = 0; i < distance; i++) {
                        cx += step;

                        if (buildMapWork[cx + cy * 20].kind == MAP_PARTS_NONE) {
                            buildMapWork[cx + cy * 20].kind = from_room;
                            buildMapWork[cx + cy * 20].link[from_room] = 1;
                            buildMapWork[cx + cy * 20].content |= BUILD_CELL_CORRIDOR;
                        } else {
                            if ((buildMapWork[cx + cy * 20].content & BUILD_CELL_ROOM) == BUILD_CELL_ROOM) {
                                buildMapWork[cx + cy * 20].content |= BUILD_CELL_DOOR;
                                buildMapWork[cx + cy * 20].link[from_room] = 1;
                            } else {
                                buildMapWork[cx + cy * 20].link[from_room] = 1;
                            }

                            done = 1;
                        }
                    }

                    break;
                case MAP_DIR_EAST:
                case MAP_DIR_WEST:
                    distance = target_y - cy;

                    if (distance < 0) {
                        distance = -distance;
                        step = -1;
                    } else {
                        step = 1;
                    }

                    for (i = 0; i < distance; i++) {
                        cy += step;

                        if (buildMapWork[cx + cy * 20].kind == MAP_PARTS_NONE) {
                            buildMapWork[cx + cy * 20].kind = from_room;
                            buildMapWork[cx + cy * 20].link[from_room] = 1;
                            buildMapWork[cx + cy * 20].content |= BUILD_CELL_CORRIDOR;
                        } else {
                            if ((buildMapWork[cx + cy * 20].content & BUILD_CELL_ROOM) == BUILD_CELL_ROOM) {
                                buildMapWork[cx + cy * 20].content |= BUILD_CELL_DOOR;
                                buildMapWork[cx + cy * 20].link[from_room] = 1;
                            } else {
                                buildMapWork[cx + cy * 20].link[from_room] = 1;
                            }

                            done = 1;
                        }
                    }

                    break;
            }

            switch (dir) {
                case MAP_DIR_NORTH:
                    cy--;
                    break;
                case MAP_DIR_SOUTH:
                    cy++;
                    break;
                case MAP_DIR_EAST:
                    cx++;
                    break;
                case MAP_DIR_WEST:
                    cx--;
                    break;
            }

            steps = 999;
        }
    }

    copyMapInfo(buildMapDat, buildMapWork);
    return 1;
}

static void setRoomObject() {
    int i;
    int x;
    int y;
    int found;
    int found_x;
    int found_y;
    int num;

    num = 0;

    for (i = 0; i < roomStackCnt; i++) {

        // Only two rooms on the floor take an object against their top wall.
        if (num < 2) {
            found = 0;

            for (x = 1; x < roomStack[i].width - 1; x++) {
                if ((buildMapDat[roomStack[i].x + (x + roomStack[i].y * 20)].content & (BUILD_CELL_ROOM | BUILD_CELL_DOOR)) == BUILD_CELL_ROOM) {
                    found_x = x + roomStack[i].x;
                    found_y = roomStack[i].y;
                    found = 1;
                }
            }

            if ((100.0f * (float) rand()) / 2147483648.0f <= 30.0f) {
                if (found == 1) {
                    buildMapDat[found_x + found_y * 20].content |= BUILD_CELL_HEAL_SPRING;
                    num++;
                }
            }
        }

        // Every room can take an object anywhere inside it.
        found = 0;

        for (y = 1; y < roomStack[i].height - 1; y++) {
            for (x = 1; x < roomStack[i].width - 1; x++) {
                if ((buildMapDat[roomStack[i].x + (x + (y + roomStack[i].y) * 20)].content & (BUILD_CELL_ROOM | BUILD_CELL_DOOR | BUILD_CELL_HEAL_SPRING | BUILD_CELL_PILLAR)) == BUILD_CELL_ROOM) {
                    found_x = x + roomStack[i].x;
                    found_y = y + roomStack[i].y;
                    found = 1;
                }
            }
        }

        if ((100.0f * (float) rand()) / 2147483648.0f <= 30.0f) {
            if (found == 1) {
                buildMapDat[found_x + found_y * 20].content |= BUILD_CELL_PILLAR;
            }
        }
    }
}

static void setRoomDivid() {
    int line_x[64];
    int line_y[64];
    int line_len[64];
    int line_dir[64];
    int i;
    int line_num;
    int ok;
    int x;
    int y;
    int line_no;

    for (i = 0; i < roomStackCnt; i++) {
        line_num = 0;

        // A room can be divided along any row that is free from end to end.
        for (y = 1; y < roomStack[i].height - 1; y++) {
            ok = 1;

            for (x = 0; x < roomStack[i].width; x++) {
                if ((buildMapDat[roomStack[i].x + (x + (y + roomStack[i].y) * 20)].content & (BUILD_CELL_ROOM | BUILD_CELL_DOOR | BUILD_CELL_HEAL_SPRING | BUILD_CELL_PILLAR)) != BUILD_CELL_ROOM) {
                    ok = 0;
                }
            }

            if (ok == 1) {
                line_x[line_num] = roomStack[i].x;
                line_y[line_num] = y + roomStack[i].y;
                line_len[line_num] = roomStack[i].width;
                line_dir[line_num] = 0;
                line_num++;
            }
        }

        // ...or along any such column.
        for (x = 1; x < roomStack[i].width - 1; x++) {
            ok = 1;

            for (y = 0; y < roomStack[i].height; y++) {
                if ((buildMapDat[roomStack[i].x + (x + (y + roomStack[i].y) * 20)].content & (BUILD_CELL_ROOM | BUILD_CELL_DOOR | BUILD_CELL_HEAL_SPRING | BUILD_CELL_PILLAR)) != BUILD_CELL_ROOM) {
                    ok = 0;
                }
            }

            if (ok == 1) {
                line_x[line_num] = x + roomStack[i].x;
                line_y[line_num] = roomStack[i].y;
                line_len[line_num] = roomStack[i].height;
                line_dir[line_num] = 1;
                line_num++;
            }
        }

        if (line_num == 0) {
            continue;
        }

        if (!((100.0f * (float) rand()) / 2147483648.0f <= 40.0f)) {
            continue;
        }

        // The wall runs the whole length of the line, with one gap in it.
        line_no = (int) (((float) line_num * (float) rand()) / 2147483648.0f);

        if (line_dir[line_no] == 0) {
            for (x = line_x[line_no]; x < line_x[line_no] + line_len[line_no]; x++) {
                buildMapDat[x + line_y[line_no] * 20].content |= BUILD_CELL_DIVIDING_WALL;
            }

            buildMapDat[line_x[line_no] + line_y[line_no] * 20 + (int) (((float) (line_len[line_no] - 2) * (float) rand()) / 2147483648.0f) + 1]
                .content |= BUILD_CELL_DIVIDING_DOOR;
        } else {
            for (y = line_y[line_no]; y < line_y[line_no] + line_len[line_no]; y++) {
                buildMapDat[y * 20 + line_x[line_no]].content |= BUILD_CELL_DIVIDING_WALL;
            }

            buildMapDat[line_x[line_no] + (line_y[line_no] + (int) (((float) (line_len[line_no] - 2) * (float) rand()) / 2147483648.0f) + 1) * 20]
                .content |= BUILD_CELL_DIVIDING_DOOR;
        }
    }
}

static void setUnderDungeonStart() {
    int safe[120];
    int safe_num;
    int i;
    int x;
    int y;
    int searching;
    int tries;
    int pick;

    safe_num = 0;

    for (i = 0; i < 120; i++) {
        safe[i] = -1;
    }

    // A room cell with no door and no dividing wall can hold the way down.
    for (y = 0; y < 20; y++) {
        for (x = 0; x < 20; x++) {
            if ((buildMapDat[x + y * 20].content & (BUILD_CELL_ROOM | BUILD_CELL_DOOR | BUILD_CELL_DIVIDING_WALL)) == BUILD_CELL_ROOM && safe_num < 60) {
                safe[safe_num * 2] = x;
                safe[safe_num * 2 + 1] = y;
                safe_num++;
            }
        }
    }

    // Keep picking cells until one comes out with an empty cell above it and a
    // door-free room cell on both sides.
    searching = 1;
    tries = 0;

    while (searching) {
        pick = (int) (((float) safe_num * (float) rand()) / 2147483648.0f);
        printf("[s]%d\n", pick);
        x = safe[pick * 2];
        y = safe[pick * 2 + 1];

        if (buildMapDat[x + (y - 1) * 20].content == 0) {
            if ((buildMapDat[x + y * 20 + 1].content & (BUILD_CELL_ROOM | BUILD_CELL_DOOR)) == BUILD_CELL_ROOM) {
                if ((buildMapDat[x + y * 20 - 1].content & (BUILD_CELL_ROOM | BUILD_CELL_DOOR)) == BUILD_CELL_ROOM) {
                    buildMapDat[x + y * 20].content = BUILD_CELL_ROOM | BUILD_CELL_URA_ENTRANCE;
                    buildMapDat[x + y * 20].kind = MAP_PARTS_ROOM_GATE_NORTH;
                    buildMapDat[x + y * 20].direction = 0;
                    searching = 0;
                }
            }
        }

        tries++;

        if (tries >= 5000) {
            searching = 0;
        }
    }

    if (searching == 0) {
        printf("safeNum = %d, escapeCnt = %d\n", safe_num, tries);
    }
}

static void setStair() {
    int safe[120];
    int safe_num;
    int i;
    int x;
    int y;
    int pick;

    // The stair down stands above a corridor that has a free cell over it.
    safe_num = 0;

    for (i = 0; i < 120; i++) {
        safe[i] = -1;
    }

    for (y = 0; y < 20; y++) {
        for (x = 0; x < 20; x++) {
            if ((buildMapDat[x + y * 20].content & BUILD_CELL_CORRIDOR) == BUILD_CELL_CORRIDOR) {
                if (buildMapDat[x + (y - 1) * 20].content == 0) {
                    safe[safe_num * 2] = x;
                    safe[safe_num * 2 + 1] = y;
                    safe_num++;
                }
            }
        }
    }

    pick = (int) (((float) safe_num * (float) rand()) / 2147483648.0f);

    if (safe_num <= 0) {
        printf("ERR: DownStair!!\n");
        y = 1;
        x = 1;
    } else {
        x = safe[pick * 2];
        y = safe[pick * 2 + 1];
    }

    buildMapDat[x + (y - 1) * 20].content = BUILD_CELL_STAIR_DOWN;
    buildMapDat[x + (y - 1) * 20].kind = buildMapDat[x + y * 20].kind;
    buildMapDat[x + (y - 1) * 20].link[buildMapDat[x + y * 20].kind] = 1;
    buildMapDat[x + (y - 1) * 20].direction = 0;

    // The stair up stands below one that has a free cell under it.
    safe_num = 0;

    for (i = 0; i < 120; i++) {
        safe[i] = -1;
    }

    for (y = 0; y < 20; y++) {
        for (x = 0; x < 20; x++) {
            if ((buildMapDat[x + y * 20].content & BUILD_CELL_CORRIDOR) == BUILD_CELL_CORRIDOR) {
                if (buildMapDat[x + (y + 1) * 20].content == 0) {
                    safe[safe_num * 2] = x;
                    safe[safe_num * 2 + 1] = y;
                    safe_num++;
                }
            }
        }
    }

    pick = (int) (((float) safe_num * (float) rand()) / 2147483648.0f);

    if (safe_num <= 0) {
        printf("ERR: UpStair!!\n");
        y = 1;
        x = 1;
    } else {
        x = safe[pick * 2];
        y = safe[pick * 2 + 1];
    }

    buildMapDat[x + (y + 1) * 20].content = BUILD_CELL_STAIR_UP;
    buildMapDat[x + (y + 1) * 20].kind = buildMapDat[x + y * 20].kind;
    buildMapDat[x + (y + 1) * 20].link[buildMapDat[x + y * 20].kind] = 1;
    buildMapDat[x + (y + 1) * 20].direction = 2;
}

void CDungeonMap::initTrapCircle() {
    for (int i = 0; i < 3; i++) {
        this->trap_circle[i].state = 0;
    }
}

float *CDungeonMap::CheckTrapCircle(float *pos, float dist) {
    for (int i = 0; i < 3; i++) {
        if (this->trap_circle[i].state != 0) {
            if (DistVector(this->trap_circle[i].pos, pos) < dist) {
                return this->trap_circle[i].pos;
            }
        }
    }

    return NULL;
}

void CDungeonMap::SetupTrapCircle(float *pos) {
    for (int i = 0; i < 3; i++) {
        bool is_free = !this->trap_circle[i].state;

        // Populate the first free slot with a trap circle of a random kind.
        if (is_free) {
            sceVu0CopyVector(this->trap_circle[i].pos, pos);
            this->trap_circle[i].state = 1;
            this->trap_circle[i].kind = (int) ((10.0f * (float) rand()) / 2147483648.0f);

            if (this->trap_circle[i].kind >= TRAP_CIRCLE_COUNT) {
                this->trap_circle[i].kind = 9;
            }

            this->trap_circle[i].timer = 0.0f;
            break;
        }
    }
}

void CDungeonMap::DrawTrapCircle() {
    float pos[4];
    int   loaded;
    int   i;

    loaded = false;
    sceVu0CopyVector(pos, CharaMain.pos);

    for (i = 0; i < 3; i++) {
        if (this->trap_circle[i].state != 0) {
            if (DistVector(this->trap_circle[i].pos, pos) <= 160.0f * this->draw_dist_scale) {
                float wide;
                float tall;

                // The texture is only wanted once, however many circles draw.
                if (!loaded) {
                    loaded = true;
                    TexManager.ReloadTexture(Vif1Packet, 71);
                    Trap_Circle.TextureAnime(71);
                }

                tall = 1.0f + sinf(this->trap_circle[i].timer);
                wide = 1.0f + sinf(this->trap_circle[i].timer) / 2.0f;
                Trap_Circle.SetScale(wide, tall, wide);
                Trap_Circle.SetPosition(this->trap_circle[i].pos);
                Trap_Circle.Draw();
            }
        }
    }
}

MAP_TRAP_CIRCLE *CDungeonMap::DistTrapCircle() {
    float pos[4];

    sceVu0CopyVector(pos, CharaMain.pos);

    for (int i = 0; i < 3; i++) {
        if (this->trap_circle[i].state == 1) {
            if (DistVector(this->trap_circle[i].pos, pos) <= 5.0f) {
                this->trap_circle[i].state = 2;
                return &this->trap_circle[i];
            }
        }
    }

    return NULL;
}

void CDungeonMap::StepTrapCircle() {
    for (int i = 0; i < 3; i++) {
        if (this->trap_circle[i].state == 2) {
            this->trap_circle[i].timer += 0.10471976f;

            if (this->trap_circle[i].timer >= THREE_HALVES_PI) {
                this->trap_circle[i].state = 0;
            }
        }
    }
}

int CDungeonMap::CheckTreasureBox(float *pos, float dist) {
    for (int i = 0; i < this->box_num; i++) {
        if (this->boxes[i].used != 0 && DistVector(this->boxes[i].pos, pos) < dist) {
            return 0;
        }
    }

    return 1;
}

int CDungeonMap::CheckAtra(float *pos, float dist) {
    // Demon Shaft does not have atla, only the first five maps do.
    if (selectMapNo >= DUNGEON_DEMON_SHAFT) {
        return 1;
    }

    for (int i = 0; i < this->atra_num; i++) {
        if (this->atra[i].used != 0 && DistVector(this->atra[i].pos, pos) < dist) {
            return 0;
        }
    }

    return 1;
}

void CDungeonMap::SetAtraBoll(float *pos, int atra_no) {
    int i;
    int event_no;

    sceVu0CopyVector(this->atra[this->atra_num].pos, pos);
    this->atra[this->atra_num].phase = 0.0f;
    this->atra[this->atra_num].atra_no = atra_no;
    this->atra[this->atra_num].used = 1;

    for (i = 0; i < 48; i++) {
        if (this->events[i].kind == DNG_EVENT_NONE) {
            event_no = i;
            goto found;
        }
    }

    printf("ivent buffer over !!\n");
    exit__2(-1);

found:
    this->events[event_no].kind = DNG_EVENT_ATRA;
    this->events[event_no].placed_flag = 0;
    this->events[event_no].index = this->atra_num;
    sceVu0CopyVector(this->events[event_no].pos, pos);
    this->events[event_no].radius = 13.0f;
    this->atra_num++;
}

/**
 * Adds a treasure box to an open slot and creates its matching map event.
 * The event stores the box coordinates and interaction radius.
 */
int CDungeonMap::SetTreasureBox(float *pos, int item_no, int kind, int trap_no) {
    int box_no;
    int event_no;
    int i;
    int j;

    box_no = -1;

    for (i = 0; i < 24; i++) {
        if (this->boxes[i].used == 0) {
            box_no = i;
            break;
        }
    }

    if (box_no == -1) {
        return -1;
    }

    sceVu0CopyVector(this->boxes[box_no].pos, pos);
    this->boxes[box_no].used = 1;
    this->boxes[box_no].lid_angle = 0;
    this->boxes[box_no].kind = kind;
    this->boxes[box_no].item_no = item_no;
    this->boxes[box_no].closed = 1;
    this->boxes[box_no].trap_no = trap_no;

    for (j = 0; j < 48; j++) {
        if (this->events[j].kind == DNG_EVENT_NONE) {
            event_no = j;
            goto found;
        }
    }

    printf("ivent buffer over !!\n");
    exit__2(-1);

found:
    this->events[event_no].kind = DNG_EVENT_TREASURE_BOX;
    this->events[event_no].placed_flag = 0;
    this->events[event_no].origin_x = (int) pos[0];
    this->events[event_no].origin_z = (int) pos[2];
    this->events[event_no].index = this->box_num;

    // The event sits in front of the box, and a small box stands nearer to it.
    switch (kind) {
        case TREASURE_BOX_LARGE:
            this->events[event_no].pos[0] = pos[0];
            this->events[event_no].pos[1] = pos[1];
            this->events[event_no].pos[2] = pos[2] + 10.0f;
            this->events[event_no].radius = 8.0f;
            break;
        case TREASURE_BOX_SMALL:
            this->events[event_no].pos[0] = pos[0];
            this->events[event_no].pos[1] = pos[1];
            this->events[event_no].pos[2] = pos[2] + 8.0f;
            this->events[event_no].radius = 8.0f;
            break;
    }

    this->box_num++;
    return event_no;
}

/**
 * Places treasure boxes, trap circles, and atla events on one floor.
 */
void CDungeonMap::buildEventData(int floor_no, int enabled, int place_atla) {
    float box_pos[4];
    float object_pos[4];
    int   atra_no[8]; /* BtAtraFloorCyoice fills and counts all eight of a floor's atla slots. */
    int   special;
    int   object_count;
    int   valid;
    int   event_no;
    int   event_x;
    int   event_y;
    int   target_count;

    if (enabled != 1) {
        return;
    }

    special = 0;

    if (place_atla == 0) {
        special = 1;
    }

    object_count = 0;
    target_count = (int) ((4.0f * (float) rand()) / 2147483648.0f) + 1;

    while (object_count < target_count + 3) {
        SearchiDoPutArea(this->cells, 0, 0, 20, 20, box_pos);
        valid = 1;

        if (this->CheckTreasureBox(box_pos, 20.0f) == 0) {
            valid = 0;
        }

        if (this->CheckAtra(box_pos, 20.0f) == 0) {
            valid = 0;
        }

        if (this->CheckTrapCircle(box_pos, 20.0f) != NULL) {
            valid = 0;
        }

        if (valid != 0) {
            if ((100.0f * (float) rand()) / 2147483648.0f >= 88.0f && object_count >= 2) {
                int item_no = PresetSmallItemNo_Get(selectMapNo, floor_no, special, 0);

                if (item_no != -1) {
                    int trap_no = (int) ((6.0f * (float) rand()) / 2147483648.0f);

                    if (trap_no < 0 || trap_no >= 6) {
                        trap_no = 1;
                    }

                    this->SetTreasureBox(box_pos, item_no, TREASURE_BOX_LARGE, trap_no);
                    object_count++;
                }
            } else {
                int item_no = PresetSmallItemNo_Get(selectMapNo, floor_no, special, 1);

                if (place_atla != 0) {
                    if (object_count == 0) {
                        item_no = ITEM_MAP;
                    }

                    if (object_count == 1) {
                        item_no = ITEM_MAGICAL_CRYSTAL;
                    }
                }

                if (item_no != -1) {
                    this->SetTreasureBox(box_pos, item_no, TREASURE_BOX_SMALL, 0);
                    object_count++;
                }
            }
        }
    }

    this->initTrapCircle();

    for (int i = 0; i < 3; i++) {
        if ((int) ((100.0f * (float) rand()) / 2147483648.0f) < 21) {
            SearchiDoPutArea(this->cells, 0, 0, 20, 20, object_pos);
            valid = 1;

            if (this->CheckTreasureBox(object_pos, 20.0f) == 0) {
                valid = 0;
            }

            if (this->CheckAtra(object_pos, 20.0f) == 0) {
                valid = 0;
            }

            if (this->CheckTrapCircle(object_pos, 20.0f) != NULL) {
                valid = 0;
            }

            if (valid != 0) {
                this->SetupTrapCircle(object_pos);
            }
        }
    }

    if (place_atla != 0) {
        object_count = 0;
        int atra_count = BtAtraFloorCyoice(selectMapNo, floor_no, atra_no);

        while (object_count < atra_count) {
            SearchiDoPutArea(this->cells, 0, 0, 20, 20, object_pos);
            valid = 1;

            if (this->CheckTreasureBox(object_pos, 20.0f) == 0) {
                valid = 0;
            }

            if (this->CheckAtra(object_pos, 20.0f) == 0) {
                valid = 0;
            }

            if (this->CheckTrapCircle(object_pos, 20.0f) != NULL) {
                valid = 0;
            }

            if (valid != 0) {
                sceVu0CopyVector(this->atra[this->atra_num].pos, object_pos);
                this->atra[this->atra_num].phase = 0.0f;
                this->atra[this->atra_num].atra_no = atra_no[object_count];
                this->atra[this->atra_num].used = 1;

                for (int k = 0; k < 48; k++) {
                    if (this->events[k].kind == DNG_EVENT_NONE) {
                        event_no = k;
                        goto found_atla_event;
                    }
                }

                printf("ivent buffer over !!\n");
                exit__2(-1);

            found_atla_event:
                this->events[event_no].kind = DNG_EVENT_ATRA;
                this->events[event_no].placed_flag = 0;
                this->events[event_no].origin_x = event_x;
                this->events[event_no].origin_z = event_y;
                this->events[event_no].index = this->atra_num;
                sceVu0CopyVector(this->events[event_no].pos, object_pos);
                this->events[event_no].radius = 13.0f;
                this->atra_num++;
                object_count++;
            }
        }
    }
}

void CDungeonMap::SetMimicEvent(float x, float y, float z, int item_no, int kind) {
    float pos[4];
    int   box_no;
    int   event_no;
    int   i;
    int   j;

    pos[0] = x;
    pos[1] = y;
    pos[2] = z;

    box_no = -1;

    for (i = 0; i < 24; i++) {
        if (this->boxes[i].used == 0) {
            box_no = i;
            break;
        }
    }

    if (box_no == -1) {
        return;
    }

    for (j = 0; j < 48; j++) {
        if (this->events[j].kind == DNG_EVENT_NONE) {
            event_no = j;
            goto found;
        }
    }

    printf("ivent buffer over !!\n");
    exit__2(-1);

found:
    if (event_no == -1) {
        return;
    }

    // The mimic looks like a treasure box, so it takes a box slot as well.
    this->events[event_no].kind = DNG_EVENT_MIMIC;
    this->events[event_no].placed_flag = 0;
    this->events[event_no].index = box_no;

    sceVu0CopyVector(this->boxes[box_no].pos, pos);
    this->boxes[box_no].used = 1;
    this->boxes[box_no].lid_angle = 0;
    this->boxes[box_no].kind = kind;
    this->boxes[box_no].item_no = item_no;
    this->boxes[box_no].closed = 1;

    switch (kind) {
        case TREASURE_BOX_LARGE:
            this->events[event_no].pos[0] = x;
            this->events[event_no].pos[1] = y;
            this->events[event_no].pos[2] = z + 10.0f;
            this->events[event_no].radius = 8.0f;
            break;
        case TREASURE_BOX_SMALL:
            this->events[event_no].pos[0] = x;
            this->events[event_no].pos[1] = y;
            this->events[event_no].pos[2] = z + 8.0f;
            this->events[event_no].radius = 8.0f;
            break;
    }
}

void CDungeonMap::RsetMimicEvent() {
    for (int i = 0; i < 48; i++) {
        if (this->events[i].kind != DNG_EVENT_NONE) {
            if (this->events[i].kind == DNG_EVENT_MIMIC) {
                this->events[i].kind = DNG_EVENT_NONE;
                this->boxes[this->events[i].index].used = 0;
            }
        }
    }
}

int CDungeonMap::GetActiveIvent(CFrameVu1 *frame) {
    float pos[4];

    sceVu0CopyVector(pos, frame->position);

    // The grid cell that the player stands on. Calculated but not used.
    (void) (int) ((float) (int) (80.0f + pos[0]) / 160.0f);
    (void) (int) ((float) (int) (80.0f + pos[2]) / 160.0f);

    for (int i = 0; i < 48; i++) {
        if (this->events[i].kind != DNG_EVENT_NONE && DistVector(this->events[i].pos, pos) <= this->events[i].radius) {
            return i;
        }
    }

    return -1;
}

void CDungeonMap::buildDummyModel() {
    float pos[4];
    float stair_pos[4];
    int   i;
    int   j;
    int   kind;
    int   num;
    int   tries;
    int   x;
    int   z;
    int   shape;

    num = 0;
    tries = 0;
    this->dummy_num = 0;

    while (num < 8) {
        x = (int) ((21.0f * (float) rand()) / 2147483648.0f) - 1;
        z = (int) ((21.0f * (float) rand()) / 2147483648.0f) - 1;
        shape = (int) ((3.0f * (float) rand()) / 2147483648.0f);
        kind = -1;

        // Each kind covers a different footprint, and only goes down where
        // every cell it would cover is empty.
        switch (shape) {
            case 0:
                kind = 0;

                for (j = 0; j < 2; j++) {
                    for (i = 0; i < 2; i++) {
                        if (this->cells[x + (i + (j + z) * 20)].parts_no != MAP_PARTS_NONE) {
                            kind = -1;
                        }
                    }
                }

                if (kind != -1) {
                    pos[0] = 80.0f + 160.0f * x;
                    pos[1] = 0.0f;
                    pos[2] = 80.0f + 160.0f * z;
                    pos[3] = 1.0f;
                }

                break;
            case 1:
                kind = 1;

                for (j = 0; j < 2; j++) {
                    for (i = 0; i < 2; i++) {
                        if (this->cells[x + (i + (j + z) * 20)].parts_no != MAP_PARTS_NONE) {
                            kind = -1;
                        }
                    }
                }

                if (kind != -1) {
                    pos[0] = 160.0f + 160.0f * x;
                    pos[1] = 0.0f;
                    pos[2] = 160.0f + 160.0f * z;
                    pos[3] = 1.0f;
                }

                break;
            case 2:
                kind = 2;

                for (j = 0; j < 3; j++) {
                    for (i = 0; i < 3; i++) {
                        if (this->cells[x + (i + (j + z) * 20)].parts_no != MAP_PARTS_NONE) {
                            kind = -1;
                        }
                    }
                }

                if (kind != -1) {
                    pos[0] = 240.0f + 160.0f * x;
                    pos[1] = 0.0f;
                    pos[2] = 240.0f + 160.0f * z;
                    pos[3] = 1.0f;
                }

                break;
        }

        if (kind != -1) {

            // A bigger model needs more room around it.
            for (x = 0; x < num; x++) {
                if (DistVector(pos, this->dummy_pos[x]) < 40.0f * kind) {
                    kind = -1;
                    break;
                }
            }

            for (x = 0; x < 20; x++) {
                for (z = 0; z < 20; z++) {
                    if (this->cells[z + x * 20].parts_no == MAP_PARTS_URA_ENTRANCE_NORTH) {
                        stair_pos[0] = 80.0f + 160.0f * z;
                        stair_pos[1] = 0.0f;
                        stair_pos[2] = 80.0f + 160.0f * x;
                        stair_pos[3] = 1.0f;

                        if (DistVector(stair_pos, pos) < 160.0f * (kind + 2)) {
                            x = 21;
                            z = x;
                            kind = -1;
                        }
                    }
                }
            }

            if (kind != -1) {
                if (this->dummy_num < 8) {
                    sceVu0CopyVector(this->dummy_pos[this->dummy_num], pos);
                    this->dummy_model[this->dummy_num] = kind;
                    this->dummy_num++;
                }

                num++;
            }
        }

        if (tries > 10000) {
            break;
        }

        tries++;
    }
}

/**
 * Selects a special door, labels its connected area, and places its key item.
 */
void CDungeonMap::GetRoomLinkInfo() {
    int   door_pos[32][2];
    int   room_list[32];
    float pos[4];
    int   door_count;
    int   stair_x;
    int   stair_y;
    int   door_part;
    int   result_no;
    int   direction_mask;
    int   room_count;
    int   x;
    int   propagation_mask;
    int   active;
    int   y;

    result_no = 0;

    for (int i = 0; i < 4; i++) {
        this->room_link[i].used = 0;
    }

    for (int i = 0; i < 256; i++) {
        scanRoomInfoWork[i] = -1;
    }

    // Record the stair cell and every candidate room-to-corridor door.
    door_count = 0;

    for (y = 0; y < 16; y++) {
        for (x = 0; x < 16; x++) {
            if (this->cells[x + y * 20].parts_no == MAP_PARTS_STAIR_DOWN) {
                stair_x = x;
                stair_y = y;
                break;
            }
        }
    }

    for (y = 0; y < 16; y++) {
        for (x = 0; x < 16; x++) {
            if ((buildMapDat[x + y * 20].content & BUILD_CELL_DOOR) != 0) {
                door_pos[door_count][0] = x;
                door_pos[door_count][1] = y;
                door_count++;

                if (door_count >= 32) {
                    y = 16;
                    break;
                }
            }
        }
    }

    int door_no = (int) (((float) door_count * (float) rand()) / 2147483648.0f);
    x = door_pos[door_no][0];
    y = door_pos[door_no][1];
    scanRoomInfoWork[x + y * 16] = 9;
    buildMapDat[x + y * 20].content |= BUILD_CELL_ROOM_LINK;
    this->room_link[0].door_x = x;
    this->room_link[0].door_y = y;
    this->room_link[0].unk_14 = 0;

    direction_mask = 0;

    if ((buildMapDat[x + (y - 1) * 20].content & BUILD_CELL_CORRIDOR) == BUILD_CELL_CORRIDOR) {
        direction_mask = MAP_DIR_NORTH;
    }

    if ((buildMapDat[x + (y + 1) * 20].content & BUILD_CELL_CORRIDOR) == BUILD_CELL_CORRIDOR) {
        direction_mask = MAP_DIR_SOUTH;
    }

    if ((buildMapDat[x + y * 20 + 1].content & BUILD_CELL_CORRIDOR) == BUILD_CELL_CORRIDOR) {
        direction_mask = MAP_DIR_EAST;
    }

    if ((buildMapDat[x + y * 20 - 1].content & BUILD_CELL_CORRIDOR) == BUILD_CELL_CORRIDOR) {
        direction_mask = MAP_DIR_WEST;
    }

    switch (direction_mask) {
        case MAP_DIR_NORTH:
            door_part = MAP_PARTS_ROOM_GATE_NORTH;
            scanRoomInfoWork[x + (y - 1) * 16] = 0;
            scanRoomInfoWork[x + (y + 1) * 16] = 1;
            break;
        case MAP_DIR_EAST:
            door_part = MAP_PARTS_ROOM_GATE_EAST;
            scanRoomInfoWork[x + y * 16 + 1] = 0;
            scanRoomInfoWork[x + y * 16 - 1] = 1;
            break;
        case MAP_DIR_SOUTH:
            door_part = MAP_PARTS_ROOM_GATE_SOUTH;
            scanRoomInfoWork[x + (y - 1) * 16] = 0;
            scanRoomInfoWork[x + (y + 1) * 16] = 1;
            break;
        case MAP_DIR_WEST:
            door_part = MAP_PARTS_ROOM_GATE_WEST;
            scanRoomInfoWork[x + y * 16 + 1] = 0;
            scanRoomInfoWork[x + y * 16 - 1] = 1;
            break;
        default:
            printf("ERROR !!\n");
            return;
    }

    // Spread each side label only through cells that share a room membership.
    active = true;

    while (active != 0) {
        active = false;

        for (y = 1; y < 15; y++) {
            for (x = 1; x < 15; x++) {
                if (scanRoomInfoWork[x + y * 16] == -1 || scanRoomInfoWork[x + y * 16] == 9) {
                    continue;
                }

                switch (buildMapDat[x + y * 20].content & (BUILD_CELL_ROOM | BUILD_CELL_CORRIDOR | BUILD_CELL_DOOR)) {
                    case BUILD_CELL_CORRIDOR:
                        propagation_mask = BUILD_CELL_CORRIDOR | BUILD_CELL_DOOR | BUILD_CELL_STAIR_DOWN;
                        break;
                    case BUILD_CELL_ROOM:
                        propagation_mask = BUILD_CELL_ROOM | BUILD_CELL_DOOR;
                        break;
                    case BUILD_CELL_ROOM | BUILD_CELL_DOOR:
                        propagation_mask = BUILD_CELL_ROOM | BUILD_CELL_CORRIDOR;
                        break;
                    default:
                        direction_mask = 0;
                        break;
                }

                if (buildMapDat[x + (y - 1) * 20].kind != MAP_PARTS_NONE && scanRoomInfoWork[x + (y - 1) * 16] == -1 && (propagation_mask & buildMapDat[x + (y - 1) * 20].content) != 0) {
                    int shared = 0;

                    for (int i = 0; i < 16; i++) {
                        if (buildMapDat[x + y * 20].link[i] == 1 && buildMapDat[x + (y - 1) * 20].link[i] == 1) {
                            shared = 1;
                        }
                    }

                    if (shared != 0) {
                        scanRoomInfoWork[x + (y - 1) * 16] = scanRoomInfoWork[x + y * 16];
                        active = true;
                    }
                }

                if (buildMapDat[x + (y + 1) * 20].kind != MAP_PARTS_NONE && scanRoomInfoWork[x + (y + 1) * 16] == -1 && (propagation_mask & buildMapDat[x + (y + 1) * 20].content) != 0) {
                    int shared = 0;

                    for (int i = 0; i < 16; i++) {
                        if (buildMapDat[x + y * 20].link[i] == 1 && buildMapDat[x + (y + 1) * 20].link[i] == 1) {
                            shared = 1;
                        }
                    }

                    if (shared != 0) {
                        scanRoomInfoWork[x + (y + 1) * 16] = scanRoomInfoWork[x + y * 16];
                        active = true;
                    }
                }

                if (buildMapDat[x + y * 20 - 1].kind != MAP_PARTS_NONE && scanRoomInfoWork[x + y * 16 - 1] == -1 && (propagation_mask & buildMapDat[x + y * 20 - 1].content) != 0) {
                    int shared = 0;

                    for (int i = 0; i < 16; i++) {
                        if (buildMapDat[x + y * 20].link[i] == 1 && buildMapDat[x + y * 20 - 1].link[i] == 1) {
                            shared = 1;
                        }
                    }

                    if (shared != 0) {
                        scanRoomInfoWork[x + y * 16 - 1] = scanRoomInfoWork[x + y * 16];
                        active = true;
                    }
                }

                if (buildMapDat[x + y * 20 + 1].kind != MAP_PARTS_NONE && scanRoomInfoWork[x + y * 16 + 1] == -1 && (propagation_mask & buildMapDat[x + y * 20 + 1].content) != 0) {
                    int shared = 0;

                    for (int i = 0; i < 16; i++) {
                        if (buildMapDat[x + y * 20].link[i] == 1 && buildMapDat[x + y * 20 + 1].link[i] == 1) {
                            shared = 1;
                        }
                    }

                    if (shared != 0) {
                        scanRoomInfoWork[x + y * 16 + 1] = scanRoomInfoWork[x + y * 16];
                        active = true;
                    }
                }
            }
        }
    }

    // The item goes in a room on the stair's side of the door.
    int target_label = scanRoomInfoWork[stair_x + (stair_y + 1) * 16];
    room_count = 0;
    active = true;

    for (int i = 0; i < this->room_num; i++) {
        if (target_label == scanRoomInfoWork[this->rooms[i].x + (this->rooms[i].y + 1) * 16 + 1]) {
            room_list[room_count++] = i;
        }
    }

    if (room_count == 0) {
        printf("//////--------> SET NOOMAL DOOR PUTERR!![%d] ///////// \n", target_label);
        active = false;
    }

    while (active != 0) {
        int valid;

        int list_no = (int) (((float) room_count * (float) rand()) / 2147483648.0f);

        if (list_no < 0 || list_no > room_count) {
            list_no = 0;
        }

        SearchiDoPutArea(this->cells, this->rooms[room_list[list_no]].x, this->rooms[room_list[list_no]].y, this->rooms[room_list[list_no]].width, this->rooms[room_list[list_no]].height, pos);
        valid = 1;

        if (this->CheckTreasureBox(pos, 20.0f) == 0) {
            valid = 0;
        }

        if (this->CheckAtra(pos, 20.0f) == 0) {
            valid = 0;
        }

        if (this->CheckTrapCircle(pos, 20.0f) != NULL) {
            valid = 0;
        }

        if (valid != 0) {
            if (this->SetTreasureBox(pos, selectMapNo + ITEM_BONE_KEY, TREASURE_BOX_SMALL, 0) == -1) {
                active = false;
                return;
            }

            this->room_link[0].item_x = (pos[0] + 80.0f) / 160.0f;
            this->room_link[0].item_y = (pos[2] + 80.0f) / 160.0f;
            this->cells[this->room_link[result_no].door_x + this->room_link[result_no].door_y * 20].parts_no = door_part;
            this->cells[this->room_link[result_no].door_x + this->room_link[result_no].door_y * 20].direction = 0;
            this->room_link[0].used = 1;
            result_no++;
            active = false;
        }
    }
}

void CDungeonMap::SetUnderLoad() {
    for (int i = 0; i < 20; i++) {
        this->cells[i * 20 + 19].parts_no = MAP_PARTS_URA_ROAD;
        this->cells[i * 20 + 19].direction = 0;
    }
}

int CDungeonMap::CreatPartsList(int *list, int max, int first_part, int last_part) {
    int            num = 0;
    PARTS_ADJACENT adjacent = {
        {{0, -1}, {0, 1}, {-1, 0}, {1, 0}}
    };
    int row;
    int col;
    int free;
    int i;

    for (row = 1; row < 15; row++) {
        for (col = 1; col < 15; col++) {
            if (last_part != -1) {

                // Any cell that holds a part of a kind in the range will do.
                if (this->cells[col + row * 20].parts_no >= first_part && this->cells[col + row * 20].parts_no <= last_part) {
                    list[num * 2] = col;
                    list[num * 2 + 1] = row;
                    num++;

                    if (num >= max - 1) {
                        row = 16;
                        break;
                    }
                }
            } else {
                free = 1;

                // A cell that touches one that is already spoken for will not.
                for (i = 0; i < 4; i++) {
                    int dx = adjacent.offset[i][0];
                    int dy = adjacent.offset[i][1];
                    int cell_no = dx + (col + (row + dy) * 20);

                    if (this->cells[cell_no].parts_no != MAP_PARTS_NONE && (buildMapDat[cell_no].content & BUILD_CELL_ROOM_LINK) == BUILD_CELL_ROOM_LINK) {
                        free = 0;
                    }
                }

                if (this->cells[col + row * 20].parts_no == first_part && free != 0) {
                    list[num * 2] = col;
                    list[num * 2 + 1] = row;
                    num++;

                    if (num >= max - 1) {
                        row = 16;
                        break;
                    }
                }
            }
        }
    }

    return num;
}

/* The integer address accumulation is a rare pointer-arithmetic exception.
 * Retail adds the cell offset to `this` with the offset on the left, and MWCC
 * emits that order only while the address accumulates in an integer: written as
 * `&this->cells[no]` the operands of that one `addu` come out the other way
 * round at every site. `->cells[0]` keeps the member offset symbolic. */
void CDungeonMap::BuildCharaSpecialParts() {
    int          list[128];
    int          roll;
    int          num;
    int          floor;
    int          pick;
    MAP_CELL    *cell;
    unsigned int address;

    if (selectMapNo < DUNGEON_DEMON_SHAFT) {
        int zone = UserStatus->res_limit_zone_current;

        // A resurrection zone puts that character's door on the floor and
        // nothing else.
        if (zone >= 0 && zone < 6) {
            if (zone == RES_LIMIT_ZONE_XIAO) {
                if ((int) ((100.0f * (float) rand()) / 2147483648.0f) >= 50) {
                    this->SetCharaDoor(CHARA_TOAN);
                } else {
                    this->SetCharaDoor(CHARA_TOAN);
                }

                return;
            }

            this->SetCharaDoor(zone);
            return;
        }

        // The first floors of the first dungeon never take a special part.
        roll = (int) ((100.0f * (float) rand()) / 2147483648.0f);

        if (selectMapNo == DUNGEON_DIVINE_BEAST_CAVE && UserStatus->cur_floor < 8) {
            roll = 0;
        }

        if (UserStatus->party_size >= 2 && roll > 0x32) {
            num = this->CreatPartsList(list, 0x40, 0, -1);

            if (num > 0) {
                pick = (int) (((float) num * (float) rand()) / 2147483648.0f);
                this->cells[list[pick * 2] + list[pick * 2 + 1] * 20].parts_no = MAP_PARTS_KEY_XIAO;
            }
        }

        // Which special part a floor takes depends on the dungeon, how far the
        // party has come and a roll of the dice.
        roll = (int) ((100.0f * (float) rand()) / 2147483648.0f);
        floor = UserStatus->cur_floor;

        switch (selectMapNo) {
            case DUNGEON_WISE_OWL_FOREST:
                if (UserStatus->party_size >= 3 && roll > 0xA && floor >= 9) {
                    num = this->CreatPartsList(list, 0x40, MAP_PARTS_ROOM_DOOR_NORTH, MAP_PARTS_ROOM_DOOR_WEST);

                    if (num > 0) {
                        pick = (int) (((float) num * (float) rand()) / 2147483648.0f);
                        address = (list[pick * 2] + list[pick * 2 + 1] * 20) * sizeof(MAP_CELL);
                        address += (unsigned int) this;
                        cell = &((CDungeonMap *) address)->cells[0];
                        cell->parts_no += MAP_PARTS_KEY_DOOR_GORO_NORTH - MAP_PARTS_ROOM_DOOR_NORTH;
                        return;
                    }
                }

                break;
            case DUNGEON_SHIPWRECK:
                if (UserStatus->party_size >= 3 && roll < 0x28) {
                    num = this->CreatPartsList(list, 0x40, MAP_PARTS_ROOM_DOOR_NORTH, MAP_PARTS_ROOM_DOOR_WEST);

                    if (num > 0) {
                        pick = (int) (((float) num * (float) rand()) / 2147483648.0f);
                        address = (list[pick * 2] + list[pick * 2 + 1] * 20) * sizeof(MAP_CELL);
                        address += (unsigned int) this;
                        cell = &((CDungeonMap *) address)->cells[0];
                        cell->parts_no += MAP_PARTS_KEY_DOOR_GORO_NORTH - MAP_PARTS_ROOM_DOOR_NORTH;
                        return;
                    }
                }

                if (UserStatus->party_size >= 4 && roll >= 0x28 && floor >= 9) {
                    num = this->CreatPartsList(list, 0x40, MAP_PARTS_ROOM_DOOR_NORTH, MAP_PARTS_ROOM_DOOR_WEST);

                    if (num > 0) {
                        pick = (int) (((float) num * (float) rand()) / 2147483648.0f);
                        address = (list[pick * 2] + list[pick * 2 + 1] * 20) * sizeof(MAP_CELL);
                        address += (unsigned int) this;
                        cell = &((CDungeonMap *) address)->cells[0];
                        cell->parts_no += MAP_PARTS_KEY_DOOR_RUBY_NORTH - MAP_PARTS_ROOM_DOOR_NORTH;
                        return;
                    }
                }

                break;
            case DUNGEON_SUN_MOON_TEMPLE:
                if (UserStatus->party_size >= 3 && roll < 0x14) {
                    num = this->CreatPartsList(list, 0x40, MAP_PARTS_ROOM_DOOR_NORTH, MAP_PARTS_ROOM_DOOR_WEST);

                    if (num > 0) {
                        pick = (int) (((float) num * (float) rand()) / 2147483648.0f);
                        address = (list[pick * 2] + list[pick * 2 + 1] * 20) * sizeof(MAP_CELL);
                        address += (unsigned int) this;
                        cell = &((CDungeonMap *) address)->cells[0];
                        cell->parts_no += MAP_PARTS_KEY_DOOR_GORO_NORTH - MAP_PARTS_ROOM_DOOR_NORTH;
                        return;
                    }
                }

                if (UserStatus->party_size >= 4 && roll < 0x28) {
                    num = this->CreatPartsList(list, 0x40, MAP_PARTS_ROOM_DOOR_NORTH, MAP_PARTS_ROOM_DOOR_WEST);

                    if (num > 0) {
                        pick = (int) (((float) num * (float) rand()) / 2147483648.0f);
                        address = (list[pick * 2] + list[pick * 2 + 1] * 20) * sizeof(MAP_CELL);
                        address += (unsigned int) this;
                        cell = &((CDungeonMap *) address)->cells[0];
                        cell->parts_no += MAP_PARTS_KEY_DOOR_RUBY_NORTH - MAP_PARTS_ROOM_DOOR_NORTH;
                        return;
                    }
                }

                if (UserStatus->party_size >= 5 && roll >= 0x28 && floor >= 9) {
                    num = this->CreatPartsList(list, 0x40, 0, -1);

                    if (num > 0) {
                        pick = (int) (((float) num * (float) rand()) / 2147483648.0f);
                        this->cells[list[pick * 2] + list[pick * 2 + 1] * 20].parts_no = MAP_PARTS_KEY_UNGAGA;
                        return;
                    }
                }

                break;
            case DUNGEON_MOON_SEA:
                if (UserStatus->party_size >= 3 && roll < 0xA) {
                    num = this->CreatPartsList(list, 0x40, MAP_PARTS_ROOM_DOOR_NORTH, MAP_PARTS_ROOM_DOOR_WEST);

                    if (num > 0) {
                        pick = (int) (((float) num * (float) rand()) / 2147483648.0f);
                        address = (list[pick * 2] + list[pick * 2 + 1] * 20) * sizeof(MAP_CELL);
                        address += (unsigned int) this;
                        cell = &((CDungeonMap *) address)->cells[0];
                        cell->parts_no += MAP_PARTS_KEY_DOOR_GORO_NORTH - MAP_PARTS_ROOM_DOOR_NORTH;
                        return;
                    }
                }

                if (UserStatus->party_size >= 4 && roll < 0x14) {
                    num = this->CreatPartsList(list, 0x40, MAP_PARTS_ROOM_DOOR_NORTH, MAP_PARTS_ROOM_DOOR_WEST);

                    if (num > 0) {
                        pick = (int) (((float) num * (float) rand()) / 2147483648.0f);
                        address = (list[pick * 2] + list[pick * 2 + 1] * 20) * sizeof(MAP_CELL);
                        address += (unsigned int) this;
                        cell = &((CDungeonMap *) address)->cells[0];
                        cell->parts_no += MAP_PARTS_KEY_DOOR_RUBY_NORTH - MAP_PARTS_ROOM_DOOR_NORTH;
                        return;
                    }
                }

                if (UserStatus->party_size >= 5 && roll < 0x1E) {
                    num = this->CreatPartsList(list, 0x40, 0, -1);

                    if (num > 0) {
                        pick = (int) (((float) num * (float) rand()) / 2147483648.0f);
                        this->cells[list[pick * 2] + list[pick * 2 + 1] * 20].parts_no = MAP_PARTS_KEY_UNGAGA;
                        return;
                    }
                }

                if (UserStatus->party_size >= 6 && roll > 0x28 && floor >= 8) {
                    num = this->CreatPartsList(list, 0x40, 0, -1);

                    if (num > 0) {
                        pick = (int) (((float) num * (float) rand()) / 2147483648.0f);
                        this->cells[list[pick * 2] + list[pick * 2 + 1] * 20].parts_no = MAP_PARTS_KEY_OSMOND;
                        return;
                    }
                }

                break;
            case DUNGEON_GALLERY_OF_TIME:
                if (UserStatus->party_size >= 3 && roll < 0xA) {
                    num = this->CreatPartsList(list, 0x40, MAP_PARTS_ROOM_DOOR_NORTH, MAP_PARTS_ROOM_DOOR_WEST);

                    if (num > 0) {
                        pick = (int) (((float) num * (float) rand()) / 2147483648.0f);
                        address = (list[pick * 2] + list[pick * 2 + 1] * 20) * sizeof(MAP_CELL);
                        address += (unsigned int) this;
                        cell = &((CDungeonMap *) address)->cells[0];
                        cell->parts_no += MAP_PARTS_KEY_DOOR_GORO_NORTH - MAP_PARTS_ROOM_DOOR_NORTH;
                        return;
                    }
                }

                if (UserStatus->party_size >= 4 && roll < 0x14) {
                    num = this->CreatPartsList(list, 0x40, MAP_PARTS_ROOM_DOOR_NORTH, MAP_PARTS_ROOM_DOOR_WEST);

                    if (num > 0) {
                        pick = (int) (((float) num * (float) rand()) / 2147483648.0f);
                        address = (list[pick * 2] + list[pick * 2 + 1] * 20) * sizeof(MAP_CELL);
                        address += (unsigned int) this;
                        cell = &((CDungeonMap *) address)->cells[0];
                        cell->parts_no += MAP_PARTS_KEY_DOOR_RUBY_NORTH - MAP_PARTS_ROOM_DOOR_NORTH;
                        return;
                    }
                }

                if (UserStatus->party_size >= 5 && roll < 0x1E) {
                    num = this->CreatPartsList(list, 0x40, 0, -1);

                    if (num > 0) {
                        pick = (int) (((float) num * (float) rand()) / 2147483648.0f);
                        this->cells[list[pick * 2] + list[pick * 2 + 1] * 20].parts_no = MAP_PARTS_KEY_UNGAGA;
                        return;
                    }
                }

                if (UserStatus->party_size >= 6 && roll > 0x28) {
                    num = this->CreatPartsList(list, 0x40, 0, -1);

                    if (num > 0) {
                        pick = (int) (((float) num * (float) rand()) / 2147483648.0f);
                        this->cells[list[pick * 2] + list[pick * 2 + 1] * 20].parts_no = MAP_PARTS_KEY_OSMOND;
                    }
                }

                break;
        }
    }
}

/* The constants that the functions above name. These stand here rather than
 * at the foot of the file because MWCC's own .rodata for SetCharaDoor's jump
 * table is spliced in at this point, and retail puts the table after all of
 * the constants below and before the two that follow the function. */

/* The integer address accumulation is the same exception as above, and for the
 * same reason. */
int CDungeonMap::SetCharaDoor(int chara_no) {
    int          list[128];
    int          num;
    int          pick;
    MAP_CELL    *cell;
    unsigned int address;

    num = 0;

    // Each character's door stands on a map part that suits it.
    switch (chara_no) {
        case CHARA_TOAN:
            if (UserStatus->party_size >= 2) {
                num = this->CreatPartsList(list, 0x40, 0, -1);

                if (num > 0) {
                    pick = (int) (((float) num * (float) rand()) / 2147483648.0f);
                    this->cells[list[pick * 2] + list[pick * 2 + 1] * 20].parts_no = MAP_PARTS_KEY_XIAO;
                }
            }

            break;
        case CHARA_XIAO:
            num = this->CreatPartsList(list, 0x40, MAP_PARTS_ROOM_DOOR_NORTH, MAP_PARTS_ROOM_DOOR_WEST);

            if (num > 0) {
                pick = (int) (((float) num * (float) rand()) / 2147483648.0f);
                address = (list[pick * 2] + list[pick * 2 + 1] * 20) * sizeof(MAP_CELL);
                address += (unsigned int) this;
                cell = &((CDungeonMap *) address)->cells[0];
                cell->parts_no += MAP_PARTS_KEY_DOOR_XIAO_NORTH - MAP_PARTS_ROOM_DOOR_NORTH;
            }

            break;
        case CHARA_GORO:
            num = this->CreatPartsList(list, 0x40, MAP_PARTS_ROOM_DOOR_NORTH, MAP_PARTS_ROOM_DOOR_WEST);

            if (num > 0) {
                pick = (int) (((float) num * (float) rand()) / 2147483648.0f);
                address = (list[pick * 2] + list[pick * 2 + 1] * 20) * sizeof(MAP_CELL);
                address += (unsigned int) this;
                cell = &((CDungeonMap *) address)->cells[0];
                cell->parts_no += MAP_PARTS_KEY_DOOR_GORO_NORTH - MAP_PARTS_ROOM_DOOR_NORTH;
            }

            break;
        case CHARA_RUBY:
            num = this->CreatPartsList(list, 0x40, MAP_PARTS_ROOM_DOOR_NORTH, MAP_PARTS_ROOM_DOOR_WEST);

            if (num > 0) {
                pick = (int) (((float) num * (float) rand()) / 2147483648.0f);
                address = (list[pick * 2] + list[pick * 2 + 1] * 20) * sizeof(MAP_CELL);
                address += (unsigned int) this;
                cell = &((CDungeonMap *) address)->cells[0];
                cell->parts_no += MAP_PARTS_KEY_DOOR_RUBY_NORTH - MAP_PARTS_ROOM_DOOR_NORTH;
            }

            break;
        case CHARA_UNGAGA:
            num = this->CreatPartsList(list, 0x40, 0, -1);

            if (num > 0) {
                pick = (int) (((float) num * (float) rand()) / 2147483648.0f);
                this->cells[list[pick * 2] + list[pick * 2 + 1] * 20].parts_no = MAP_PARTS_KEY_UNGAGA;
            }

            break;
        case CHARA_OSMOND:
            num = this->CreatPartsList(list, 0x40, 0, -1);

            if (num > 0) {
                pick = (int) (((float) num * (float) rand()) / 2147483648.0f);
                this->cells[list[pick * 2] + list[pick * 2 + 1] * 20].parts_no = MAP_PARTS_KEY_OSMOND;
            }

            break;
    }

    return num;
}

/* MWCC emits its own jump table for the switch, which lands where retail keeps
 * @3191, and a .data template for `gate_keys`, which is retail's @3162 at the end
 * of this unit's data run. */
void CDungeonMap::buildRandomMap(int room_max, int first_build) {
    int w;
    int h;
    int j;
    int i;
    int col;
    int row;
    int x;
    int y;
    int done;
    int retry;
    int dist;
    int near_no;
    int near_dist;

    // A floor that is built for the first time picks the seed it is built from.
    if (first_build != 0) {
        this->map_seed = (int) ((float) rand() / (float) 100000);
        srand(this->map_seed);
    }

    for (i = 0; i < 48; i++) {
        this->events[i].kind = DNG_EVENT_NONE;
        this->events[i].reset_flag = 0;
    }

    for (i = 0; i < 24; i++) {
        this->boxes[i].used = 0;
        this->boxes[i].lid_angle = 0;
        this->boxes[i].trap_no = 0;
    }

    this->box_num = 0;

    for (i = 0; i < 8; i++) {
        this->atra[i].used = 0;
    }

    this->atra_num = 0;

    for (i = 0; i < 4; i++) {
        this->room_link[i].used = 0;
    }

    for (row = 0; row < 20; row++) {
        for (col = 0; col < 20; col++) {
            buildMapDat[col + row * 20].kind = MAP_PARTS_NONE;
            buildMapDat[col + row * 20].direction = 0;

            for (j = 0; j < 16; j++) {
                buildMapDat[col + row * 20].link[j] = 0;
            }

            buildMapDat[col + row * 20].content = 0;
            this->cells[col + row * 20].direction = 0;
        }
    }

    // Two rooms have to stand before any other can join to one.
    roomStackCnt = 0;
    retry = 0;

    while (roomStackCnt < 2 && retry < 0x40) {
        done = 0;
        retry = 0;

        while (done == 0) {
            w = (int) ((2.0f * (float) rand()) / 2147483648.0f) + 3;
            h = (int) ((2.0f * (float) rand()) / 2147483648.0f) + 3;
            x = (int) (((float) (16 - w) * (float) rand()) / 2147483648.0f);
            y = (int) (((float) (16 - h) * (float) rand()) / 2147483648.0f);

            if (buildRoom(x, y, w, h, roomStackCnt) == 1) {
                roomStack[roomStackCnt].x = x;
                roomStack[roomStackCnt].y = y;
                roomStack[roomStackCnt].width = w;
                roomStack[roomStackCnt].height = h;
                roomStackCnt++;
                done = 1;
            } else {
                retry++;

                if (retry >= 0x40) {
                    done = 1;
                }
            }
        }
    }

    joinRoom(0, 1);

    // Every further room joins to whichever room its middle is nearest to.
    while (roomStackCnt <= room_max - 1 && retry < 0x200) {
        done = 0;
        retry = 0;

        while (done == 0) {
            w = (int) ((2.0f * (float) rand()) / 2147483648.0f) + 3;
            h = (int) ((2.0f * (float) rand()) / 2147483648.0f) + 3;
            x = (int) (((float) (16 - w) * (float) rand()) / 2147483648.0f);
            y = (int) (((float) (16 - h) * (float) rand()) / 2147483648.0f);

            if (buildRoom(x, y, w, h, roomStackCnt) == 1) {
                roomStack[roomStackCnt].x = x;
                roomStack[roomStackCnt].y = y;
                roomStack[roomStackCnt].width = w;
                roomStack[roomStackCnt].height = h;
                roomStackCnt++;
                near_no = -1;
                near_dist = 10000;

                for (j = 0; j < roomStackCnt - 1; j++) {
                    w = (roomStack[roomStackCnt - 1].x + roomStack[roomStackCnt - 1].width / 2) - (roomStack[j].x + roomStack[j].width / 2);
                    h = (roomStack[roomStackCnt - 1].y + roomStack[roomStackCnt - 1].height / 2) - (roomStack[j].y + roomStack[j].height / 2);

                    dist = (int) sqrt((double) (w * w + h * h));

                    if (dist < near_dist) {
                        near_no = j;
                        near_dist = dist;
                    }
                }

                joinRoom(roomStackCnt - 1, near_no);
                done = 1;
            } else {
                retry++;

                if (retry >= 0x200) {
                    done = 1;
                }
            }
        }
    }

    setRoomObject();

    if (first_build != 0) {
        setRoomDivid();
    }

    if (first_build != 0) {
        setUnderDungeonStart();
    } else {
        setUnderDungeonStart();
    }

    if (first_build != 0) {
        setStair();
    }

    mapPartsFilter();

    this->room_num = roomStackCnt;
    memcpy(this->rooms, roomStack, 0x100);

    for (row = 0; row < 20; row++) {
        for (col = 0; col < 20; col++) {
            this->cells[col + row * 20].parts_no = buildMapDat[col + row * 20].kind;
            this->cells[col + row * 20].direction = buildMapDat[col + row * 20].direction;
        }
    }

    if (selectMapNo == DUNGEON_MOON_SEA) {
        this->buildDummyModel();
    }

    if ((int) ((100.0f * (float) rand()) / 2147483648.0f) < 0x33 && first_build != 0) {
        this->GetRoomLinkInfo();
    }

    if (first_build != 0) {
        this->BuildCharaSpecialParts();
    }

    // Which key opens the floor's gate.
    switch (selectMapNo) {
        case DUNGEON_DIVINE_BEAST_CAVE:
            this->gate_key_no = ITEM_DRAN_S_CREST;
            break;
        case DUNGEON_SHIPWRECK:
            this->gate_key_no = ITEM_HOOK;
            break;
        case DUNGEON_SUN_MOON_TEMPLE:
            this->gate_key_no = ITEM_KING_S_SLATE;
            break;
        case DUNGEON_MOON_SEA:
            this->gate_key_no = ITEM_GUN_POWDER;
            break;
        case DUNGEON_GALLERY_OF_TIME:
            this->gate_key_no = ITEM_CLOCK_HANDS;
            break;
        case DUNGEON_DEMON_SHAFT:
            this->gate_key_no = ITEM_BLACK_KNIGHT_CREST;
            break;
    }

    if (selectMapNo == DUNGEON_WISE_OWL_FOREST) {
        int pick = (int) ((3.0f * (float) rand()) / 2147483648.0f);
        int gate_keys[3] = {0xC4, 0xC6, 0xCD};

        if (pick < 0 || pick > 2) {
            pick = 0;
        }

        this->gate_key_no = gate_keys[pick];
    }

    this->SetUnderLoad();
    printf("map build success!!\n");
}

void CDungeonMap::initSubmap(CDataAlloc2<1> *alloc) {
    int i;
    int j;

    for (i = 0; i < 6; i++) {
        this->bg_model[i] = NULL;
    }

    for (i = 0; i < 3; i++) {
        this->dummy_frame[i] = NULL;
    }

    this->box_lid_model = NULL;
    this->box_body_model = NULL;
    this->box_collision_model = NULL;
    this->chest_lid_model = NULL;
    this->chest_body_model = NULL;
    this->chest_collision_model = NULL;
    this->fall_model = NULL;

    for (i = 0; i < 24; i++) {
        this->boxes[i].used = 0;
    }

    for (i = 0; i < this->atra_num; i++) {
        this->atra[i].used = 0;
    }

    this->bg_model_num = 0;
    this->dummy_frame_num = 0;
    this->dummy_num = 0;
    this->water.SetSize(32, 32, alloc);

    for (i = 0; i < 72; i++) {
        this->parts[i].initalize();
    }

    for (i = 0; i < 400; i++) {
        this->cells[i].parts_no = MAP_PARTS_NONE;
        this->cells[i].camera_dist = -1.0f;
        this->mask[i] = 0;
        this->cells[i].visible = false;
    }

    for (i = 0; i < 16; i++) {
        this->room_seen[i] = 0;
    }

    for (i = 0; i < 4; i++) {
        this->npc[i].chara.Initialize();
        this->npc[i].used = 0;
        this->npc[i].draw_num = 0;
    }

    for (j = 0; j < 48; j++) {
        this->events[j].kind = DNG_EVENT_NONE;
        this->events[j].reset_flag = 0;
    }

    for (j = 0; j < 24; j++) {
        this->boxes[j].used = 0;
        this->boxes[j].lid_angle = 0;
        this->boxes[j].trap_no = 0;
    }

    this->box_num = 0;

    for (j = 0; j < 8; j++) {
        this->atra[j].used = 0;
    }

    this->atra_num = 0;

    for (j = 0; j < 4; j++) {
        this->room_link[j].used = 0;
    }
}

void CDungeonMap::initalize() {
    int i;
    int j;

    for (i = 0; i < 6; i++) {
        this->bg_model[i] = NULL;
    }

    for (i = 0; i < 3; i++) {
        this->dummy_frame[i] = NULL;
    }

    this->box_lid_model = NULL;
    this->box_body_model = NULL;
    this->box_collision_model = NULL;
    this->chest_lid_model = NULL;
    this->chest_body_model = NULL;
    this->chest_collision_model = NULL;
    this->fall_model = NULL;

    for (i = 0; i < 24; i++) {
        this->boxes[i].used = 0;
    }

    for (i = 0; i < this->atra_num; i++) {
        this->atra[i].used = 0;
    }

    this->bg_model_num = 0;
    this->dummy_frame_num = 0;
    this->dummy_num = 0;

    for (i = 0; i < 72; i++) {
        this->parts[i].initalize();
    }

    for (i = 0; i < 400; i++) {
        this->cells[i].parts_no = MAP_PARTS_NONE;
        this->cells[i].camera_dist = -1.0f;
        this->mask[i] = 0;
        this->cells[i].visible = false;
    }

    for (i = 0; i < 16; i++) {
        this->room_seen[i] = 0;
    }

    for (i = 0; i < 4; i++) {
        this->npc[i].chara.Initialize();
        this->npc[i].used = 0;
        this->npc[i].draw_num = 0;
    }

    for (j = 0; j < 48; j++) {
        this->events[j].kind = DNG_EVENT_NONE;
        this->events[j].reset_flag = 0;
    }

    for (j = 0; j < 24; j++) {
        this->boxes[j].used = 0;
        this->boxes[j].lid_angle = 0;
        this->boxes[j].trap_no = 0;
    }

    this->box_num = 0;

    for (j = 0; j < 8; j++) {
        this->atra[j].used = 0;
    }

    this->atra_num = 0;

    for (j = 0; j < 4; j++) {
        this->room_link[j].used = 0;
    }

    this->draw_dist_scale = 4.0f;
}
