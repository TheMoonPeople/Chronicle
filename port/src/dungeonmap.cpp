#include "dungeonmap.hpp"
#include <libvu0.h>
#include <cmath>
#include <cstdint>
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

// Retail's, with the cell picked for a character's key door addressed on the whole pointer.

PC_OVERRIDE void CDungeonMap::BuildCharaSpecialParts() {
    int          list[128];
    int          roll;
    int          num;
    int          floor;
    int          pick;
    MAP_CELL    *cell;
    std::uintptr_t address;

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
                        address += (std::uintptr_t) this;
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
                        address += (std::uintptr_t) this;
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
                        address += (std::uintptr_t) this;
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
                        address += (std::uintptr_t) this;
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
                        address += (std::uintptr_t) this;
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
                        address += (std::uintptr_t) this;
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
                        address += (std::uintptr_t) this;
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
                        address += (std::uintptr_t) this;
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
                        address += (std::uintptr_t) this;
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

PC_OVERRIDE int CDungeonMap::SetCharaDoor(int chara_no) {
    int          list[128];
    int          num;
    int          pick;
    MAP_CELL    *cell;
    std::uintptr_t address;

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
                address += (std::uintptr_t) this;
                cell = &((CDungeonMap *) address)->cells[0];
                cell->parts_no += MAP_PARTS_KEY_DOOR_XIAO_NORTH - MAP_PARTS_ROOM_DOOR_NORTH;
            }

            break;
        case CHARA_GORO:
            num = this->CreatPartsList(list, 0x40, MAP_PARTS_ROOM_DOOR_NORTH, MAP_PARTS_ROOM_DOOR_WEST);

            if (num > 0) {
                pick = (int) (((float) num * (float) rand()) / 2147483648.0f);
                address = (list[pick * 2] + list[pick * 2 + 1] * 20) * sizeof(MAP_CELL);
                address += (std::uintptr_t) this;
                cell = &((CDungeonMap *) address)->cells[0];
                cell->parts_no += MAP_PARTS_KEY_DOOR_GORO_NORTH - MAP_PARTS_ROOM_DOOR_NORTH;
            }

            break;
        case CHARA_RUBY:
            num = this->CreatPartsList(list, 0x40, MAP_PARTS_ROOM_DOOR_NORTH, MAP_PARTS_ROOM_DOOR_WEST);

            if (num > 0) {
                pick = (int) (((float) num * (float) rand()) / 2147483648.0f);
                address = (list[pick * 2] + list[pick * 2 + 1] * 20) * sizeof(MAP_CELL);
                address += (std::uintptr_t) this;
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
