#include "stubs/menu_dungeon.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <format>
#include <print>
#include <string>

#include "battlemenu.hpp"
#include "btactstatus.hpp"
#include "btmisc.hpp"
#include "camera.hpp"
#include "clsmes.hpp"
#include "dataalloc.hpp"
#include "dataread.hpp"
#include "debugfont.hpp"
#include "dun/gameloop.hpp"
#include "editatra.hpp"
#include "gamepad.hpp"
#include "itemdata.hpp"
#include "mainitemmodel.hpp"
#include "mainselect.hpp"
#include "mds.hpp"
#include "memcard.hpp"
#include "memorycardaccess.hpp"
#include "menu_dungeon.hpp"
#include "menu_inventory.hpp"
#include "menu_misc.hpp"
#include "menuetc.hpp"
#include "menuitemstep.hpp"
#include "mglib.hpp"
#include "rect.hpp"
#include "savedata.hpp"
#include "shot_freefuncs.hpp"
#include "snd.hpp"
#include "texture.hpp"
#include "userstatus.hpp"

namespace {

/**
 * Steps of the character change menu, as CHARA_CHANGE_MENU::step holds them.
 */
// clang-format off
enum class CharaChangeStep : s8 {
    Select     = 0, /**< Choosing a party member on the ring. */
    Turning    = 1, /**< Turning the ring toward the newly selected member. */
    ItemPrompt = 2, /**< Asking whether to use Stand-in Powder. */
    Blocked    = 3, /**< Showing why the leader cannot change. */
    Unk4       = 4, /**< Handled as Blocked; never entered. */
    Loading    = 5, /**< Loading the new leader. */
    Loaded     = 6, /**< Finishing the new leader's load. */
    Unk7       = 7, /**< Counting down before closing; never entered. */
};

// clang-format on

/**
 * State of the character change menu and its character positions.
 */
struct CHARA_CHANGE_MENU {
    s8              selected;       /**< Party member the ring has turned to. */
    s8              cursor_row;     /**< Row the cursor is on in the message window. */
    s8              party_size;     /**< Number of party members on the ring. */
    CharaChangeStep step;           /**< Step of the menu's state machine. */
    float           turn_frame;     /**< Frames the ring has spent turning toward the newly selected member. */
    float           ring_pos[6][2]; /**< Each member's offset from the ring's centre. */
    s16             ring_slot[6];   /**< Each member's place on the ring, counted from the front. */
    float           turn_direction; /**< Direction the ring turns: 1 after selecting the previous member, -1 after the next. */
    s16             reset_on_open;  /**< Cleared when the menu opens; nothing reads it. */
    u8              unk_4a[2];
    s32             unk_4c; /**< Frames spent in the closing and fade steps. */
    u8              unk_50[2];
    s8              change_blocked; /**< Why the leader cannot change: 0 free, 1 in a restricted zone, 2 Osmond in a state that forbids it. */
    u8              unk_53;
    float           cursor_x; /**< Screen x of the cursor. */
    float           cursor_y; /**< Screen y of the cursor. */
    s16             mode;     /**< Mode the menu was opened with; 1 uses up an item when the leader changes. */
    u8              unk_5e[2];
};

STATIC_ASSERT(sizeof(CHARA_CHANGE_MENU) == 0x60);

/**
 * State of the debug item menu, which hands out any item on request.
 */
struct ITEM_AUTO_GET {
    s32 selection;  /**< Entry the cursor stands on. */
    s32 page;       /**< Page of 64 entries the cursor is on. */
    s32 show_model; /**< Toggled by the key that previews the selected item's model. */
};

STATIC_ASSERT(sizeof(ITEM_AUTO_GET) == 0xC);

CHARA_CHANGE_MENU ChangeMenu;

/** Texture of the floor list on the dungeon entrance board. */
CTexture *DunLogBoard2;

/** Number of floors available in each dungeon. */
constexpr std::array<int, 7> maxFloorTbl__4 = {15, 17, 18, 18, 15, 25, 100};

int  DunEnterMenuKey();
void DunEnterDraw();
void DunEnterBoard(int x, int y, int alpha);
void DrawGetAtoraNumBoard(int floor, int x, int y, int top, int bottom, int alpha);
void DrawDunNumberClip(int x, int y, int top, int bottom, int digit, int alpha);
void DrawDunEnterBack(int alpha);
void DrawDunEnterFloorName(int x, int y, int floor, int top, int bottom, int alpha);
void DrawItemDataView(int item_no);

/**
 * Adds one attachment's values into another, scaled by a factor.
 */
void PlusAttachmentVolume(ATTACH_LIST *base, ATTACH_LIST *add, float scale) {
    for (int i = 0; i < 4; i++) {
        base->status[i] += add->status[i] * scale;
    }

    for (int i = 0; i < 5; i++) {
        base->elem[i] += add->elem[i] * scale;
    }

    for (int i = 0; i < 10; i++) {
        base->vs_monster[i] += add->vs_monster[i] * scale;
    }
}

} // namespace

DUN_ENTER_MENU DEnterMenu;

int MenuEtcErrCnt;

/** Texture of the dungeon entrance board. */
CTexture *DunLogBoard;

CDngStatusData *DEnterStatusPt;

int GetWeaponAttachStatusUp(WEAPON_HAVE *weapon, int stat) {
    if (GetWeaponData(weapon->item_no) == nullptr || weapon->item_no < ITEM_WEAPON_START) {
        return 0;
    }

    int total = 0;

    for (int i = 0; i < 6; i++) {
        if (weapon->attach[i].item_no - ITEM_ATTACH_START < 0) {
            continue;
        }

        int multiplier = 1;

        if (weapon->attach_kind[i] == 3) {
            multiplier = 2;
        }

        ATTACH_LIST *attach = &weapon->attach[i];

        if (stat < 5) {
            total += attach->status[stat - 1] * multiplier;
        } else if (stat >= 8 && stat < 0xD) {
            total += attach->elem[stat - 8] * multiplier;
        } else {
            total += attach->vs_monster[stat - 14] * multiplier;
        }
    }

    return total;
}

void SetWeaponAttachStatus(WEAPON_HAVE *result) {
    if (result == nullptr) {
        return;
    }

    CDngStatusData *dng_status = SaveData->GetDngStatus();

    if (dng_status == nullptr) {
        std::println("cdng is NULL!!!!");
        return;
    }

    const int chara = dng_status->cur_chara;
    const int slot = dng_status->equipped_weapon_slot[chara];
    WeaponAllValueSet(&dng_status->chara_weapons[chara][slot], result, 0);
}

void WeaponAllValueSet(WEAPON_HAVE *weapon, WEAPON_HAVE *result, int full) {
    WEAPON_DATA *data = GetWeaponData(weapon->item_no);

    if (data == nullptr) {
        return;
    }

    ATTACH_LIST *attaches = weapon->attach;
    ATTACH_LIST  total{};
    float        rate = GetNowWeaponRate(weapon);

    if (full) {
        rate = 1.0f;
    }

    int flags = weapon->flags;
    std::memcpy(result, weapon, sizeof(WEAPON_HAVE));

    for (int i = 0; i < GetWeaponHoleNum(weapon->item_no); i++) {
        if (data->hole[i] <= 0) {
            break;
        }

        float scale = 1.0f;

        if (weapon->attach_kind[i] == 3) {
            scale = 2.0f;
        }

        PlusAttachmentVolume(&total, &attaches[i], scale);

        if (attaches[i].item_no == ITEM_ATTACH_SYNTHESIS_SPHERE && attaches[i].sphere_flags != 0 && attaches[i].sphere_flags != 1) {
            flags |= attaches[i].sphere_flags;
        }
    }

    result->flags = CheckWeaponOptionStatus(flags);
    int limit[4] = {data->attack_max, 99, 99, data->magic_max};
    limit[0] *= rate;
    limit[3] *= rate;
    int value = result->attack + total.status[0];
    value *= rate;

    if (limit[0] < value) {
        result->attack = limit[0];
    } else {
        result->attack = value;
    }

    value = result->endurance + total.status[1];
    value *= rate;

    if (limit[1] < value) {
        result->endurance = limit[1];
    } else {
        result->endurance = value;
    }

    value = result->speed + total.status[2];
    value *= rate;

    if (value >= limit[2]) {
        result->speed = limit[2];
    } else {
        result->speed = value;
    }

    value = result->magic + total.status[3];
    value *= rate;

    if (limit[3] < value) {
        result->magic = limit[3];
    } else {
        result->magic = value;
    }

    for (int i = 0; i < 5; i++) {
        value = result->elem[i] + total.elem[i];
        value *= rate;

        if (value >= 99) {
            result->elem[i] = 99;
        } else {
            result->elem[i] = value;
        }
    }

    for (int i = 0; i < 10; i++) {
        value = result->vs_monster[i] + total.vs_monster[i];
        value *= rate;

        if (value >= 99) {
            result->vs_monster[i] = 99;
        } else {
            result->vs_monster[i] = value;
        }
    }
}

int InitDunEnterMenu(int texture_block, int dungeon, int requested_floor) {
    SaveData->QuestDungeon(dungeon, 1);
    DEnterStatusPt = reinterpret_cast<CDngStatusData *>(UserStatus);
    StartReadBG();
    u_long128 *buffer = MenuCalcBufAlignment(reinterpret_cast<u_long128 *>(read_buffer));
    DEnterMenu.dungeon = dungeon;
    std::array<char, 108> language_dir{};
    GetPathReadDifferntLang(language_dir.data());
    std::string path = std::format("{}dunenter/dunenter{}.pak", language_dir.data(), static_cast<int>(DEnterMenu.dungeon));
    int         size = 0;
    LoadFileBG(path.data(), buffer, &size);
    ReadBG();
    MenuEtcErrCnt = 0;
    DEnterMenu.texture_block = texture_block;
    DEnterMenu.textures_ready = 0;
    DEnterMenu.loading = true;
    DEnterMenu.counter = 0;
    DEnterMenu.state = DUN_ENTER_FADE_IN;
    DEnterMenu.requested_floor = requested_floor;

    if (requested_floor >= 0) {
        DEnterMenu.state = DUN_ENTER_DIRECT;
        return 1;
    }

    int first_open = -1;
    int floor = DEnterStatusPt->floor_reached[dungeon];

    if (dungeon == 5) {
        if (floor >= maxFloorTbl__4[dungeon]) {
            floor = maxFloorTbl__4[dungeon] - 1;
        }

        std::println("maxfloor = {}", floor);
    }

    if (floor >= 0) {
        DEnterMenu.floor_count = floor + 1;
    } else {
        DEnterMenu.floor_count = 1;
    }

    for (floor = 0; floor < maxFloorTbl__4[dungeon]; floor++) {
        DEnterMenu.max_atra[floor] = DEnterStatusPt->GetMaxAtraNum(dungeon, floor);
        DEnterMenu.collected_atra[floor] = DEnterStatusPt->GetAtraNum(dungeon, floor);
        DEnterMenu.kills[floor] = DEnterStatusPt->ChkKills(dungeon, floor);

        if (DEnterMenu.kills[floor] > 999) {
            DEnterMenu.kills[floor] = 999;
        }

        if (first_open == -1 && static_cast<u8>(DEnterMenu.max_atra[floor]) != static_cast<u8>(DEnterMenu.collected_atra[floor])) {
            first_open = floor;
        }
    }

    DEnterMenu.selected_floor = DEnterMenu.floor_count - 1;
    DEnterMenu.scroll_top = DEnterMenu.selected_floor;

    if (first_open > -1) {
        DEnterMenu.selected_floor = first_open;
        DEnterMenu.scroll_top = DEnterMenu.selected_floor;
    }

    if (DEnterMenu.scroll_top > DEnterMenu.floor_count - 5) {
        DEnterMenu.scroll_top = DEnterMenu.floor_count - 5;

        if (DEnterMenu.scroll_top < 0) {
            DEnterMenu.scroll_top = 0;
        }
    }

    std::println("DEnterMenu.select = {}", static_cast<int>(DEnterMenu.selected_floor));
    std::println("DEnterMenu.topline = {}", static_cast<int>(DEnterMenu.scroll_top));
    DEnterMenu.list_y = 118.0f - 40.0f * DEnterMenu.scroll_top;
    s8 lines = DEnterMenu.floor_count;

    if (lines < 5) {
        lines = 5;
    }

    DEnterMenu.scroll_bar_y = 122.0f + 108.0f * DEnterMenu.scroll_top / lines;
    GamePad.SetAutoRepeat(PAD_L2 | PAD_R2 | PAD_L1 | PAD_R1 | PAD_DPAD, 30, 5);
    GamePad.MenuModeOn(0x78);
    return 1;
}

namespace {

/**
 * Restores the pad and textures when the dungeon entrance menu closes.
 */
void ExitDunEnterMenu() {
    GamePad.AutoRepeatOff();
    GamePad.MenuModeOff();
    MenuTextureReload(DEnterMenu.texture_block);
    DngActiveItemTextureCopy();
    DngActiveWeaponTextureCopy();
    TexManager.DeleteTextureBlock(DEnterMenu.texture_block);
}

} // namespace

int DunEnterMenuLoop() {
    rand();
    ReadBG();
    int result = DunEnterMenuKey();
    DunEnterDraw();
    ItemVolumeStep.LoopStep(60);

    if (0 <= result) {
        ExitDunEnterMenu();
        ItemVolumeStep.CheckItemVolume();
        result = DEnterMenu.result;
    }

    return result;
}

namespace {

int DunEnterMenuKey() {
    int result = -1;

    switch (DEnterMenu.state) {
        case DUN_ENTER_FADE_IN:
        case DUN_ENTER_DIRECT:
            if (DEnterMenu.textures_ready == 0 && ReadBGSync() == 0) {
                LOADTEXTURE_INFO2 textures[] = {
                    {const_cast<char *>("#frame_menu_enter#640#" SCREEN_HEIGHT_STR "#4"), DEnterMenu.texture_block, 0},
                    {nullptr,                                                             DEnterMenu.texture_block, 0},
                    {nullptr,                                                             0,                        0},
                };
                BG_READ_INFO *archive = GetReadBGFile(0);
                textures[1].name = reinterpret_cast<char *>(GetPackFile(reinterpret_cast<u_int *>(archive->buffer), const_cast<char *>("dunenter.img"), nullptr));
                TexManager.DeleteTextureBlock(DEnterMenu.texture_block);
                TexManager.CleanUpTextureList();
                TexManager.LoadTextureBlockEX(-1, textures);
                DunLogBoard = TexManager.GetTexture("dunenter", DEnterMenu.texture_block);
                DunLogBoard2 = TexManager.GetTexture("kaisou", DEnterMenu.texture_block);
                InitMenuMesSet(MENU_MES_SET_DUNGEON_ENTER, reinterpret_cast<short *>(GetPackFile(reinterpret_cast<u_int *>(archive->buffer), const_cast<char *>("dunlog.bin"), nullptr)));
                CommonMenuMes2.mes_made = -1;
                CommonMenuMes2.MakeMesWin(10);
                DEnterMenu.textures_ready = 1;
                DEnterMenu.loading = false;
                std::println("tex enter end ");
            } else {
                MenuEtcErrCnt++;
            }

            if (DEnterMenu.textures_ready != 0) {
                DEnterMenu.counter++;
            }

            if (DEnterMenu.textures_ready != 0 && DEnterMenu.counter > 30) {
                DEnterMenu.state = DUN_ENTER_SELECT;
                DEnterMenu.counter = 0;

                if (DEnterMenu.requested_floor >= 0) {
                    DEnterMenu.state = DUN_ENTER_FADE_OUT;
                    DEnterMenu.result = DEnterMenu.requested_floor;
                    result = 1;
                }
            }

            break;
        case DUN_ENTER_FADE_OUT:
            DEnterMenu.counter++;

            if (DEnterMenu.counter > 46) {
                result = 1;
            }

            break;
        case DUN_ENTER_SELECT: {
            const int selected = DEnterMenu.selected_floor;
            const int scroll_top = DEnterMenu.scroll_top;

            if (GamePad.Down(PAD_UP)) {
                DEnterMenu.selected_floor--;

                if (DEnterMenu.selected_floor < DEnterMenu.scroll_top) {
                    DEnterMenu.scroll_top--;
                }
            } else if (GamePad.Down(PAD_DOWN)) {
                DEnterMenu.selected_floor++;

                if (DEnterMenu.scroll_top + 4 < DEnterMenu.selected_floor) {
                    DEnterMenu.scroll_top++;
                }
            } else if (GamePad.Down(PAD_R2 | PAD_R1 | PAD_RIGHT)) {
                DEnterMenu.scroll_top += 5;
                DEnterMenu.selected_floor += 5;
            } else if (GamePad.Down(PAD_L2 | PAD_L1 | PAD_LEFT)) {
                DEnterMenu.scroll_top -= 5;
                DEnterMenu.selected_floor -= 5;
            }

            if (DEnterMenu.scroll_top < 0 || DEnterMenu.selected_floor < 0) {
                DEnterMenu.selected_floor = 0;
                DEnterMenu.scroll_top = 0;
            }

            if (DEnterMenu.floor_count - 1 < DEnterMenu.selected_floor) {
                DEnterMenu.selected_floor = DEnterMenu.floor_count - 1;
                DEnterMenu.scroll_top = DEnterMenu.selected_floor - 4;

                if (DEnterMenu.scroll_top < 0) {
                    DEnterMenu.scroll_top = 0;
                }
            }

            if (DebugMode) {
                if (GamePad.Down2(PAD_R1)) {
                    int max_floor = maxFloorTbl__4[DEnterMenu.dungeon];
                    DEnterMenu.floor_count = max_floor;

                    for (int floor = 0; floor < DEnterMenu.floor_count; floor++) {
                        DEnterMenu.max_atra[floor] = DEnterStatusPt->GetMaxAtraNum(DEnterMenu.dungeon, floor);
                        DEnterMenu.collected_atra[floor] = DEnterStatusPt->GetAtraNum(DEnterMenu.dungeon, floor);
                    }

                    ComMenuSePlay(MENU_SOUND_CONFIRM);
                }

                if (GamePad.On2(PAD_CROSS) && DEnterMenu.kills[DEnterMenu.selected_floor] < 999) {
                    DEnterMenu.kills[DEnterMenu.selected_floor]++;
                }

                if (GamePad.On2(PAD_CIRCLE) && 0 < DEnterMenu.kills[DEnterMenu.selected_floor]) {
                    DEnterMenu.kills[DEnterMenu.selected_floor]--;
                }
            }

            if (DEnterMenu.selected_floor > DEnterMenu.floor_count - 1) {
                DEnterMenu.selected_floor = DEnterMenu.floor_count - 1;
            }

            const int count = DEnterMenu.floor_count;

            if (count > 5) {
                if (DEnterMenu.scroll_top > count - 5) {
                    DEnterMenu.scroll_top = count - 5;

                    if (DEnterMenu.scroll_top < 0) {
                        DEnterMenu.scroll_top = 0;
                        DEnterMenu.selected_floor = 0;
                    }
                }
            } else if (DEnterMenu.selected_floor > count - 1 || DEnterMenu.scroll_top > count - 1) {
                DEnterMenu.selected_floor = count - 1;
                DEnterMenu.scroll_top = 0;
            }

            if (DEnterMenu.scroll_top < 0 || DEnterMenu.selected_floor < 0) {
                DEnterMenu.scroll_top = 0;
                DEnterMenu.selected_floor = 0;
            }

            if (std::abs(DEnterMenu.scroll_top - scroll_top) > 4) {
                DEnterMenu.list_y = 118.0f - 40.0f * DEnterMenu.scroll_top;
                DEnterMenu.scroll_bar_y = 122.0f + 108.0f * DEnterMenu.scroll_top / DEnterMenu.floor_count;
            }

            if (selected != DEnterMenu.selected_floor || scroll_top != DEnterMenu.scroll_top) {
                ComMenuSePlay(MENU_SOUND_CURSOR);
            }

            if (GamePad.Down(PAD_CROSS)) {
                DEnterMenu.result = DEnterMenu.selected_floor;
                DEnterMenu.state = DUN_ENTER_FADE_OUT;
                ComMenuSePlay(MENU_SOUND_CONFIRM);
            }

            if (GamePad.Down(PAD_CIRCLE)) {
                ComMenuSePlay(MENU_SOUND_REFUSE);
            }

            break;
        }
        default:
            break;
    }

    return result;
}

void DunEnterDraw() {
    setbilinear(0);
    CRect_i_ screen(0, 0, 0x2800, (SCREEN_HEIGHT << 4));
    int      alpha = 0x80;

    if (DEnterMenu.state == DUN_ENTER_FADE_IN && DEnterMenu.textures_ready != 0) {
        alpha = DEnterMenu.counter * 6;
    }

    alpha = std::clamp(alpha, 0, 0x80);

    if (DEnterMenu.textures_ready != 0 && DEnterMenu.state != DUN_ENTER_DIRECT) {
        MenuTextureReload(DEnterMenu.texture_block);
        DrawDunEnterBack(alpha);
        DunEnterBoard(0x46, 0x5A, alpha);
        CursorVibeCnt++;

        if (CursorVibeCnt > 0x107AC0) {
            CursorVibeCnt = 0;
        }

        if (DEnterMenu.state == DUN_ENTER_SELECT) {
            const int   cursor_y = (DEnterMenu.selected_floor - DEnterMenu.scroll_top) * 40 + 0x74;
            static int  DunWakuCnt = 0;
            const float wave = 0.2f * DunWakuCnt;
            const int   left = 94.0f + wave;
            const int   top = cursor_y + wave;
            const int   right = 204.0f - wave;
            const int   bottom = (cursor_y + 0x18) - wave;
            RECT        corner = {0x40, 0x60, 0x10, 0x10};
            DrawMenu2DSprite(DunLogBoard, CRect_i_(left, top, corner.width, corner.height), CRect_i_(corner.x, corner.y, corner.width, corner.height), alpha);
            DrawMenu2DSprite(DunLogBoard, CRect_i_(right, top, corner.width, corner.height), CRect_i_(corner.x + corner.width, corner.y, corner.width, corner.height), alpha);
            DrawMenu2DSprite(DunLogBoard, CRect_i_(left, bottom, corner.width, corner.height), CRect_i_(corner.x, corner.y + corner.height, corner.width, corner.height), alpha);
            DrawMenu2DSprite(DunLogBoard, CRect_i_(right, bottom, corner.width, corner.height), CRect_i_(corner.x + corner.width, corner.y + corner.height, corner.width, corner.height), alpha);
            DunWakuCnt++;

            if (DunWakuCnt < 0 || DunWakuCnt >= 30) {
                DunWakuCnt = 0;
            }

            CRect_i_ cursor(0x20, 0x60, 0x20, 0x20);
            DrawObjectVibe(0x49, cursor_y + 2, DunLogBoard, cursor, 0, alpha);
            DrawObjectVibe(0x46, cursor_y, DunLogBoard, cursor, 0x80, alpha);
        }

        MenuTextureReload(CommonMenuMes2.tex_block);
        CommonMenuMes2.stay_frame = true;
        CommonMenuMes2.edge_alpha = alpha;
        CommonMenuMes2.text_y = 0x16c;
        const int half_width = CommonMenuMes2.char_width >> 1;
        GetMenuCommonPutXY(&CommonMenuMes2, 0x148 - half_width);
        CommonMenuMes2.Step();
        CommonMenuMes2.DrawMesWin();
    } else {
        MGFillBox(screen, 0, 0, 0, 0x80);
    }

    int cover = 0x80;

    switch (DEnterMenu.state) {
        case DUN_ENTER_FADE_IN:
            cover = 0x80 - DEnterMenu.counter * 6;
            break;
        case DUN_ENTER_FADE_OUT:
            cover = DEnterMenu.counter * 9;
            break;
        default:
            break;
    }

    cover = std::clamp(cover, 0, 0x80);

    switch (DEnterMenu.state) {
        case DUN_ENTER_FADE_IN:
        case DUN_ENTER_FADE_OUT:
            MGFillBox(screen, 0, 0, 0, cover);
            break;
        default:
            break;
    }

    setbilinear(1);
}

/**
 * Draws the frame of the dungeon entrance board.
 */
void DunEnterBoardWaku(int x, int y, int alpha) {
    DrawMenu2DSprite(DunLogBoard, CRect_i_(x, y - 1, 0x20, 0xFA), CRect_i_(0, 0, 0x20, 0xF8), alpha);
    DrawMenu2DSprite(DunLogBoard, CRect_i_(x + 0x20, y, 0x1CC, 0x20), CRect_i_(0x20, 0, 0x1B8, 0x20), alpha);
    DrawMenu2DSprite(DunLogBoard, CRect_i_(x + 0x1EC, y - 1, 0x28, 0xFA), CRect_i_(0x1D8, 0, 0x28, 0xF8), alpha);
    DrawMenu2DSprite(DunLogBoard, CRect_i_(x + 0x20, y + 0xD9, 0x1CC, 0x20), CRect_i_(0x20, 0xD8, 0x1B8, 0x20), alpha);

    // The scroll bar eases toward the list's scroll position and shrinks with a longer list.
    const int count = std::max<int>(DEnterMenu.floor_count, 5);

    int draw_x = x + 0x1FC;
    int draw_y = static_cast<int>(DEnterMenu.scroll_bar_y += (static_cast<int>(32.0f + y + 108.0f * DEnterMenu.scroll_top / count) - DEnterMenu.scroll_bar_y) / 4.0f);
    int height = static_cast<int>(540.0f / count);

    while (y + 0x98 < draw_y + (height + 8)) {
        height--;
    }

    DrawMenu2DSprite(DunLogBoard, CRect_i_(draw_x, draw_y, 8, 4), CRect_i_(0x60, 0x60, 8, 4), alpha);
    DrawMenu2DSprite(DunLogBoard, CRect_i_(draw_x, draw_y + 4, 8, height), CRect_i_(0x60, 0x64, 8, 4), alpha);
    DrawMenu2DSprite(DunLogBoard, CRect_i_(draw_x, draw_y + height + 4, 8, 4), CRect_i_(0x60, 0x68, 8, 4), alpha);

    draw_x = x + 0x28;
    draw_y = y - 0x14;
    const int dungeon = DEnterMenu.dungeon;
    const int icon = std::min(dungeon, 5);

    const int u = (icon / 3) << 8;
    const int v = (icon % 3) * 40 + 0x100;

    if (dungeon == 6) {
        DrawMenu2DSprite(DunLogBoard, CRect_i_(draw_x, draw_y, 0x80, 0x23), CRect_i_(0x20, 0x80, 0x80, 0x24), alpha);
        DrawMenu2DSprite(DunLogBoard, CRect_i_(draw_x + 0x80, draw_y, 0x80, 0x23), CRect_i_(0x20, 0xA4, 0x80, 0x24), alpha);
    } else {
        DrawMenu2DSprite(DunLogBoard, CRect_i_(draw_x, draw_y, 0x100, 0x28), CRect_i_(u, v, 0x100, 0x28), alpha);
    }
}

/**
 * Draws the clipped floor list and collection marks on the dungeon entrance board.
 */
void DunEnterBoard(int x, int y, int alpha) {
    float     lane = x;
    const int top = y + 0x19;
    const int bottom = y + 0xEF;
    const int target = y + 0x1C - DEnterMenu.scroll_top * 40;
    const int step = static_cast<int>(static_cast<float>(target) - DEnterMenu.list_y);
    DEnterMenu.list_y += step >> 2;

    if (std::abs(step) < 2) {
        DEnterMenu.list_y = target;
    }

    int row_y = static_cast<int>(DEnterMenu.list_y);

    CRect_i_  rule_source(0x20, 0x20, 0x28, 7);
    const int dim = (alpha * 80) >> 7;
    int       rule_x = static_cast<int>(32.0f + lane);

    if (DEnterMenu.scroll_top == 0) {
        DrawMenu2DSprite(DunLogBoard, CRect_i_(rule_x - 10, 0x73, 10, 3), rule_source, dim);

        for (int segment = 0; segment < 11; segment++) {
            DrawMenu2DSprite(DunLogBoard, CRect_i_(rule_x, 0x73, 0x28, 3), rule_source, dim);
            rule_x += 0x28;
        }

        DrawMenu2DSprite(DunLogBoard, CRect_i_(rule_x, 0x73, 10, 3), rule_source, dim);
    }

    const int column_x = x + 0x20;

    for (int i = 0; i < 100; i++) {
        int source = 0x20;
        int length = 0x28;
        int position = row_y;
        lane = column_x;

        if (!(row_y < bottom)) {
            break;
        }

        if (row_y + 0x28 <= top) {
            row_y += 0x28;
            continue;
        }

        MenuTextureClip(position, source, length, top, bottom);
        CRect_i_ edge_source(0x20, source, 10, length);
        DrawMenu2DSprite(DunLogBoard, CRect_i_(static_cast<int>(lane - 10.0f), position, 10, length), edge_source, dim);

        for (int segment = 0; segment < 12; segment++) {
            DrawMenu2DSprite(DunLogBoard, CRect_i_(static_cast<int>(lane), position, 0x28, length), CRect_i_(0x20, source, 0x28, length), dim);
            lane += 40.0f;
        }

        DrawMenu2DSprite(DunLogBoard, CRect_i_(static_cast<int>(lane), position, 10, length), edge_source, dim);

        if (DEnterMenu.floor_count - 1 < i) {
            row_y += 0x28;
            continue;
        }

        position = row_y + 4;
        DrawDunEnterFloorName(column_x, position, i + 1, top, bottom, alpha);
        const int dungeon = DEnterMenu.dungeon;
        const int max_floor = maxFloorTbl__4[dungeon];

        if (i == max_floor - 1) {
            const int draw_x = column_x + 0x82;
            position = row_y + 6;
            length = 0x18;
            source = dungeon * 24 + 0x20;
            MenuTextureClip(position, source, length, top, bottom);

            if (position < bottom) {
                DrawMenu2DSprite(DunLogBoard, CRect_i_(draw_x, position, 0xDC, length), CRect_i_(0xFC, source, 0xDC, length), alpha);
            }
        } else if (dungeon < 6) {
            int draw_x = column_x + 0x72;
            position = row_y;
            length = 0x20;
            source = 0x20;
            MenuTextureClip(position, source, length, top - 10, bottom);

            if (position < bottom) {
                int remaining = static_cast<u8>(DEnterMenu.max_atra[i]) - static_cast<u8>(DEnterMenu.collected_atra[i]);

                for (int mark = 0; mark < remaining; mark++) {
                    DrawMenu2DSprite(DunLogBoard, CRect_i_(draw_x, position, 0x20, length), CRect_i_(0x48, source, 0x20, length), alpha);
                    draw_x += 0x1A;
                }

                const int collected = static_cast<u8>(DEnterMenu.collected_atra[i]);

                for (int mark = 0; mark < collected; mark++) {
                    DrawMenu2DSprite(DunLogBoard, CRect_i_(draw_x, position, 0x20, length), CRect_i_(0x68, source, 0x20, length), alpha);
                    draw_x += 0x1A;
                }
            }
        }

        row_y += 0x28;
    }

    MenuTextureReload(DunLogBoard->block);
    row_y = static_cast<int>(DEnterMenu.list_y);
    row_y--;

    for (int floor = 0; floor < 100; floor++) {
        if (floor == maxFloorTbl__4[DEnterMenu.dungeon] - 1) {
            break;
        }

        if (DEnterMenu.floor_count - 1 < floor) {
            continue;
        }

        int board_x = x + 0x16E;

        if (row_y < bottom) {
            DrawGetAtoraNumBoard(floor, board_x, row_y, top, bottom, alpha);
        }

        row_y += 0x28;
    }

    DunEnterBoardWaku(x, y, alpha);
}

/**
 * Draws a number right to left, one digit at a time, clipped to the board.
 */
void DrawEnemyNum(int x, int y, int top, int bottom, int number, int alpha) {
    for (int digits = GetNumberKeta(number); 0 < digits; digits--) {
        int       position = y;
        const int digit = number % 10;
        x -= 11;
        const int u = digit * 12 + 0x20;
        int       source = 0x48;
        int       length = 12;
        MenuTextureClip(position, source, length, top, bottom);
        DrawMenu2DSprite(DunLogBoard, CRect_i_(x, position, 12, length - 1), CRect_i_(u, source, 12, length), alpha);
        number /= 10;
    }
}

/**
 * Returns where a floor row's kill count starts, given where its Atla count starts.
 */
int KillCountX(int x) {
    return x + 0x36;
}

void DrawGetAtoraNumBoard(int floor, int x, int y, int top, int bottom, int alpha) {
    int position = y;
    int source = 0x20;
    int length = 0x24;

    if (!(top < y + 0x24) || bottom <= y) {
        return;
    }

    MenuTextureClip(position, source, length, top, bottom);
    DrawMenu2DSprite(DunLogBoard, CRect_i_(x, position, 0x2B, length), CRect_i_(0x88, source, 0x2B, length), alpha);

    position = y + 8;
    const int collected = static_cast<u8>(DEnterMenu.collected_atra[floor]);

    if (top < position) {
        DrawDunNumberClip(x + 5, position, top, bottom, collected, alpha);
    }

    position = y + 0x10;
    const int maximum = static_cast<u8>(DEnterMenu.max_atra[floor]);
    DrawDunNumberClip(x + 0x1B, position, top, bottom, maximum, alpha);

    const int kills = DEnterMenu.kills[floor];
    x = KillCountX(x);
    position = y + 1;
    source = 0x20;
    length = 0x23;
    MenuTextureClip(position, source, length, top, bottom);

    if (position < bottom) {
        DrawMenu2DSprite(DunLogBoard, CRect_i_(x, position, 0x1E, length), CRect_i_(0xB3, source, 0x1E, length), alpha);
    }

    position = y + 0x10;

    if (position < bottom - 4) {
        DrawEnemyNum(x + 0x44, position, top, bottom - 6, kills, alpha);
    }
}

void DrawDunNumberClip(int x, int y, int top, int bottom, int digit, int alpha) {
    int       position = y;
    const int u = digit * 12 + 0x20;
    int       source = 0x48;
    int       length = 12;
    MenuTextureClip(position, source, length, top, bottom);

    if (position < bottom) {
        DrawMenu2DSprite(DunLogBoard, CRect_i_(x, position, 12, length), CRect_i_(u, source, 12, length), alpha);
    }
}

void DrawDunEnterBack(int alpha) {
    DrawFullSizePicture(TexManager.GetTexture("frame", -1), 0, 0, 0x80);
    MGFillBox(CRect_i_(0, 0, 0x2800, SCREEN_HEIGHT * 0x10), 10, 10, 10, (alpha * 4) >> 7);
}

} // namespace

/**
 * Screen position of the character change ring's centre.
 */
int QuickCharaPos[2];

/**
 * Texture block the character change menu loads its pictures into.
 */
s16 CharaChangeTexBlock;

/**
 * Set once the character change menu's files have been read.
 */
s16 CharaChangeReadFlag;

/**
 * Status of the party the character change menu picks from.
 */
CDngStatusData *ChangeStatusDataPt;

char chara_change_frame_image[] __attribute__((section(".rodata"))) = "#frame_image#640#" SCREEN_HEIGHT_STR "#4";

namespace {

/**
 * Buffer past the character change menu's pictures, where its models are read.
 */
u_long128 *chara_change_buf;

/**
 * Texture of the character change menu's portraits.
 */
CTexture *QuickCharaTex;

/** Model the item preview shows. */
CFrame *ItemPolyView;

/** Set while the debug item preview is shown. */
int MDebugItemPolyViewFlag;

/** Set once the item preview's files have been read. */
int polyreadflag;

void DrawDunEnterFloorName(int x, int y, int floor, int top, int bottom, int alpha) {
    int position = y;

    if (y < bottom) {
        int height = 0x1C;
        int u = 0;
        int source = DEnterMenu.dungeon * 32;

        if (DEnterMenu.dungeon >= 6) {
            u = 0x80;
            source = (DEnterMenu.dungeon - 6) * 32;
        }

        MenuTextureClip(position, source, height, top, bottom);

        if (position + height >= top) {
            DrawMenu2DSprite(DunLogBoard2, CRect_i_(x, position, 0x70, height), CRect_i_(u, source, 0x70, height), alpha);
        }

        if (DEnterMenu.dungeon == 5) {
            floor = BtGetFloorLevel(floor - 1);
        }

        int offset = 0;

        if (floor < 10) {
            offset = -6;
        }

        if (DEnterMenu.dungeon == 5 || DEnterMenu.dungeon == 6) {
            if (floor < 10) {
                offset = -12;
            }

            if (floor >= 10 && floor < 100) {
                offset = -6;
            }

            if (floor >= 100) {
                offset = 0;

                if (DEnterMenu.dungeon == 6 && GetMenuLangFlag() > LANG_JAPANESE) {
                    offset += 8;
                }
            }
        }

        s8 name_width[7][7] = {
            {0x50, 0x6E, 0x48, 0x68, 0x38, 0x30, 0x50},
            {0x52, 0x68, 0x60, 0x60, 0x38, 0x30, 0x64},
            {0x52, 0x68, 0x60, 0x60, 0x38, 0x30, 0x64},
            {0x52, 0x68, 0x60, 0x62, 0x38, 0x30, 0x66},
            {0x5C, 0x68, 0x60, 0x60, 0x2E, 0x30, 0x64},
            {0x52, 0x68, 0x60, 0x60, 0x38, 0x30, 0x64},
            {0x52, 0x68, 0x60, 0x60, 0x38, 0x50, 0x64},
        };
        x = offset + (x + name_width[GetMenuLangFlag()][DEnterMenu.dungeon]);
        RECT digits = {0, 0xC0, 0x10, 0x1C};

        if (y + height >= top) {
            DrawMenuNumber(floor, x, y, digits, DunLogBoard2, 0, top, bottom, alpha);
        }
    }
}

/**
 * Places the character change menu's cursor on its selected row.
 */
void SetChangeMenuCursor() {
    ChangeMenu.cursor_x = CommonMenuMes3.text_x - 0x1C;
    ChangeMenu.cursor_y = CommonMenuMes3.text_y - 4 + ChangeMenu.cursor_row * 24;
}

/**
 * Makes a character the party's leader.
 */
void SetStatusChara(CDngStatusData *status, s8 chara) {
    status->cur_chara = chara;
}

} // namespace

float changeMenu_long = 60.0f;

void StartQuickChange(u_long128 *buffer, int texture_block, int *positions, int mode) {
    LOADTEXTURE_INFO2 table[] = {
        {chara_change_frame_image, 0, 0},
        {nullptr,                  0, 0},
    };
    table[0].block_no = texture_block;
    TexManager.DeleteTextureBlock(texture_block);
    TexManager.CleanUpTextureList();
    TexManager.LoadTextureBlockEX(-1, table);
    chara_change_buf = MenuCalcBufAlignment(buffer);
    StartReadBG();
    LoadFileBGMenuData("quickchr.pac", chara_change_buf);
    CharaChangeReadFlag = 0;
    ReadBG();
    QuickCharaPos[0] = positions[0];
    QuickCharaPos[1] = positions[1];
    ChangeStatusDataPt = reinterpret_cast<CDngStatusData *>(UserStatus);

    if (ChangeStatusDataPt == nullptr) {
        return;
    }

    ChangeMenu.selected = ChangeStatusDataPt->cur_chara;
    ItemVolumeStep.CheckItemVolume();
    ChangeMenu.unk_4c = 0;
    StayTex = TexManager.GetTexture("stayframe", -1);
    CommonMenuMes3.text_x = 200;
    CommonMenuMes3.text_y = 190;
    changeMenu_long = 60.0f;
    ChangeMenu.mode = mode;
    ChangeMenu.cursor_row = 0;
    ChangeMenu.change_blocked = 0;
    auto     *status = reinterpret_cast<CUserStatus *>(ChangeStatusDataPt);
    const int zone = status->res_limit_zone_current;

    if (zone < 6 && zone >= 0) {
        ChangeMenu.change_blocked = 1;
    } else if (status->cur_chara == CHARA_OSMOND && BtActStatus.ground_kind == 10) {
        ChangeMenu.change_blocked = 2;
    }

    if (ChangeMenu.mode != 0) {
        ChangeMenu.step = CharaChangeStep::ItemPrompt;
        SetChangeMenuCursor();
    } else {
        ChangeMenu.step = CharaChangeStep::Select;

        if (ChangeMenu.change_blocked == 1) {
            ChangeMenu.cursor_row = 1;
            SetChangeMenuCursor();
        } else if (ChangeMenu.change_blocked == 2) {
            std::println(" not change area ");
            ChangeMenu.cursor_row = 1;
            SetChangeMenuCursor();
        }
    }

    ChangeMenu.party_size = ChangeStatusDataPt->party_size;
    ChangeMenu.reset_on_open = 0;
    float step = TWO_PI / ChangeMenu.party_size;

    for (int i = 0; i < ChangeMenu.party_size; i++) {
        ChangeMenu.ring_slot[i] = i - ChangeMenu.selected;

        if (ChangeMenu.ring_slot[i] < 0) {
            ChangeMenu.ring_slot[i] += ChangeMenu.party_size;
        }

        float angle = PI + step * ChangeMenu.ring_slot[i];
        ChangeMenu.ring_pos[i][0] = changeMenu_long * cos(angle);
        ChangeMenu.ring_pos[i][1] = changeMenu_long * sin(angle);
    }

    GamePad.SetAutoRepeat(PAD_DPAD, 30, 5);
    GamePad.MenuModeOn(0x78);
    CharaChangeTexBlock = texture_block;
}

int CharaChangeLoop() {
    if (ReadBGSync() == 0 && CharaChangeReadFlag == 0) {
        LOADTEXTURE_INFO2 table[] = {
            {chara_change_frame_image, 0, 0},
            {nullptr,                  0, 0},
            {nullptr,                  0, 0},
        };
        table[0].block_no = CharaChangeTexBlock;
        table[1].block_no = CharaChangeTexBlock;
        BG_READ_INFO *file = GetReadBGFile(0);

        if (file == nullptr) {
            return 1;
        }

        table[1].name = reinterpret_cast<char *>(GetPackFile(reinterpret_cast<u_int *>(file->buffer), const_cast<char *>("quickchr.img"), nullptr));
        TexManager.DeleteTextureBlock(CharaChangeTexBlock);
        TexManager.CleanUpTextureList();
        TexManager.LoadTextureBlockEX(-1, table);
        QuickCharaTex = TexManager.GetTexture("quickchara", -1);
        chara_change_buf = file->buffer + (file->size >> 4) + 4;
        chara_change_buf = MenuCalcBufAlignment(chara_change_buf);
        CharaChangeReadFlag = 1;
        InitMenuMesSet(MENU_MES_SET_ALLMENU, reinterpret_cast<s16 *>(GetPackFile(reinterpret_cast<u_int *>(file->buffer), const_cast<char *>("qchr.mes"), nullptr)));
        CommonMenuMes3.auto_pos = MES_POS_NONE;
        CommonMenuMes3.Preset(MES_PRESET_SYSTEM);
        int message = 1;

        if (ChangeMenu.change_blocked == 1) {
            message = 4;
        }

        if (ChangeMenu.change_blocked == 2) {
            message = 5;
        }

        CommonMenuMes3.MakeMesWin(message);
        CommonMenuMes1.Preset(MES_PRESET_SYSTEM);
        CommonMenuMes1.MakeMesWin(3);
    }

    int result = 0;

    if (CharaChangeReadFlag != 0) {
        result = CharaChangeKey();
    }

    CharaChangeDraw();
    ItemVolumeStep.LoopStep(60);

    if (result != 0) {
        MenuTextureReload(CharaChangeTexBlock);
        DngActiveWeaponTextureCopy();
        CharaChangeReadFlag = 0;
        ItemVolumeStep.CheckItemVolume();
        GamePad.MenuModeOff();
        GamePad.AutoRepeatOff();
        TexManager.DeleteTextureBlock(CharaChangeTexBlock);
        TexManager.CleanUpTextureList();
        LOADTEXTURE_INFO2 restore[] = {
            {chara_change_frame_image, 0, 0},
            {nullptr,                  0, 0},
        };
        restore[0].block_no = CharaChangeTexBlock;
        TexManager.LoadTextureBlockEX(-1, restore);
    }

    return result;
}

int CharaChangeKey() {
    int result = 0;

    ReadBG();

    switch (ChangeMenu.step) {
        case CharaChangeStep::Loading:
            ChangeMenu.unk_4c++;

            if (ReadBGSync() == 0 && ChangeMenu.unk_4c > 0x10) {
                BtMenuLoadChara();
                StartReadBG();
                CharaChangeInitToGL2(0);
                ChangeMenu.step = CharaChangeStep::Loaded;
            }

            break;
        case CharaChangeStep::Loaded:
            ChangeMenu.unk_4c++;

            if (ReadBGSync() == 0 && ChangeMenu.unk_4c > 0x10) {
                BtMenuLoad2(0);
                LockOffTargte();
                InitReadBG();
                return 1;
            }

            break;
        case CharaChangeStep::Blocked:
        case CharaChangeStep::Unk4: {
            int message = 1;

            if (ChangeMenu.change_blocked == 1) {
                message = 4;
            }

            if (ChangeMenu.change_blocked == 2) {
                message = 5;
            }

            if (CommonMenuMes3.mes_made != message) {
                CommonMenuMes3.MakeMesWin(message);
            }

            if (GamePad.Down(PAD_CIRCLE | PAD_CROSS)) {
                if (ChangeMenu.mode != 0) {
                    ChangeMenu.step = CharaChangeStep::ItemPrompt;
                    CommonMenuMes3.MakeMesWin(ChangeMenu.change_blocked == 0 ? 1 : 2);
                } else {
                    ChangeMenu.step = CharaChangeStep::Select;
                }

                ComMenuSePlay(MENU_SOUND_REFUSE);
            }

            break;
        }
        case CharaChangeStep::Turning:
            ChangeMenu.turn_frame += 1.0f;

            if (ChangeMenu.turn_frame >= 21.0f) {
                if (ChangeMenu.turn_direction > 0.0f) {
                    for (s16 &slot : ChangeMenu.ring_slot) {
                        slot++;

                        if (slot == ChangeMenu.party_size) {
                            slot = 0;
                        }
                    }
                } else {
                    for (int i = 0; i < ChangeMenu.party_size; i++) {
                        ChangeMenu.ring_slot[i]--;

                        if (ChangeMenu.ring_slot[i] == -1) {
                            ChangeMenu.ring_slot[i] = ChangeMenu.party_size - 1;
                        }
                    }
                }

                ChangeMenu.step = CharaChangeStep::Select;
                ChangeMenu.turn_frame = 0.0f;
                ChangeMenu.turn_direction = 0.0f;
            }

            break;
        case CharaChangeStep::ItemPrompt: {
            int message = 1;

            if (ChangeMenu.change_blocked == 1 || ChangeMenu.change_blocked == 2) {
                message = 2;
            }

            if (CommonMenuMes3.mes_made != message) {
                CommonMenuMes3.MakeMesWin(message);
            }

            if (GamePad.Down(PAD_UP | PAD_DOWN)) {
                if (ChangeMenu.cursor_row > 0) {
                    ChangeMenu.cursor_row = 0;
                } else {
                    ChangeMenu.cursor_row = 1;
                }

                ComMenuSePlay(MENU_SOUND_CURSOR);
            }

            if (DebugMode && GamePad.Down2(PAD_CROSS)) {
                ChangeStatusDataPt->GetItem(ITEM_STAND_IN_POWDER, 0);
            }

            if (GamePad.Down(PAD_CROSS)) {
                ITEM_PACK *pack = &ChangeStatusDataPt->item_pack;
                const int  count = GetNowItemNum(ITEM_STAND_IN_POWDER, pack);

                if (ChangeMenu.cursor_row != 0) {
                    result = 2;
                    ComMenuSePlay(MENU_SOUND_CONFIRM);
                } else if (ChangeMenu.change_blocked == 1 || ChangeMenu.change_blocked == 2) {
                    ComMenuSePlay(MENU_SOUND_REFUSE);
                    ChangeMenu.step = CharaChangeStep::Blocked;
                    return 0;
                } else if (count > 0) {
                    ChangeMenu.step = CharaChangeStep::Select;
                    ComMenuSePlay(MENU_SOUND_CONFIRM);
                } else {
                    ComMenuSePlay(MENU_SOUND_REFUSE);
                }
            } else if (GamePad.Down(PAD_CIRCLE)) {
                ComMenuSePlay(MENU_SOUND_REFUSE);
            }

            break;
        }
        case CharaChangeStep::Select: {
            if (GamePad.Down(PAD_CROSS)) {
                if (ChangeMenu.selected > ChangeMenu.party_size - 1) {
                    ComMenuSePlay(MENU_SOUND_REFUSE);
                    ChangeMenu.step = CharaChangeStep::Blocked;
                    return 0;
                }

                if (ChangeMenu.change_blocked == 1) {
                    ComMenuSePlay(MENU_SOUND_REFUSE);
                    CommonMenuMes3.MakeMesWin(4);
                    ChangeMenu.step = CharaChangeStep::Blocked;
                    return 0;
                }

                if (ChangeMenu.change_blocked == 2) {
                    CommonMenuMes3.MakeMesWin(5);
                    ChangeMenu.step = CharaChangeStep::Blocked;
                    ComMenuSePlay(MENU_SOUND_REFUSE);
                    return 0;
                }

                CDngStatusData *status = ChangeStatusDataPt;
                int             hp = status->hp[ChangeMenu.selected];
                int             flags = status->GetActiveCharaStatus(ChangeMenu.selected);

                if (hp <= 0 || (flags & AILMENT_KNOCKED_OUT)) {
                    ComMenuSePlay(MENU_SOUND_REFUSE);
                    return 0;
                }

                if (ChangeMenu.selected != status->cur_chara) {
                    ComMenuSePlay(MENU_SOUND_CONFIRM);
                    SetStatusChara(ChangeStatusDataPt, ChangeMenu.selected);

                    if (ChangeMenu.mode == 1) {
                        ChangeStatusDataPt->LostItem(ITEM_STAND_IN_POWDER);
                    }

                    ChangeMenu.unk_4c = 0;
                    ChangeMenu.step = CharaChangeStep::Loading;
                    ChangeMenu.mode = 0;
                    return 0;
                }

                ComMenuSePlay(MENU_SOUND_CONFIRM);
            } else if (GamePad.Down(PAD_CIRCLE)) {
                if (ChangeStatusDataPt->hp[ChangeStatusDataPt->cur_chara] < 0 || ChangeMenu.mode != 0) {
                    ChangeMenu.step = CharaChangeStep::ItemPrompt;
                    ChangeMenu.cursor_row = 0;
                    ChangeMenu.cursor_x = CommonMenuMes3.text_x - 0x1E;
                    ChangeMenu.cursor_y = CommonMenuMes3.text_y + 0x10 + ChangeMenu.cursor_row * 24;
                } else {
                    result = 1;
                }

                ComMenuSePlay(MENU_SOUND_REFUSE);
            }

            int previous = ChangeMenu.selected;

            if (GamePad.Down(PAD_UP | PAD_RIGHT)) {
                ChangeMenu.turn_direction = 1.0f;
                ChangeMenu.selected--;

                if (ChangeMenu.selected < 0) {
                    ChangeMenu.selected = ChangeMenu.party_size - 1;
                }
            }

            if (GamePad.Down(PAD_DOWN | PAD_LEFT)) {
                ChangeMenu.turn_direction = -1.0f;
                ChangeMenu.selected++;

                if (ChangeMenu.party_size - 1 < ChangeMenu.selected) {
                    ChangeMenu.selected = 0;
                }
            }

            if (previous != ChangeMenu.selected) {
                ChangeMenu.step = CharaChangeStep::Turning;
                BreakReadBG();
                u_long128 *buffer = chara_change_buf;
                CharaChangeInitToGL(buffer, ChangeMenu.selected);
                ComMenuSePlay(MENU_SOUND_CURSOR);
            }

            break;
        }
        case CharaChangeStep::Unk7:
            ChangeMenu.unk_4c += 7;

            if (ChangeMenu.unk_4c > 0xFA) {
                result = 2;
            }

            break;
    }

    return result;
}

void CharaChangeDraw() {
    setbilinear(0);
    FrameImageDraw(100, 0x80);

    if (CharaChangeReadFlag != 0) {
        MenuTextureReload(CharaChangeTexBlock);
        TexManager.GetTexture("stayframe", -1);
        int alpha = 0x80;

        switch (ChangeMenu.step) {
            case CharaChangeStep::Loaded:
            case CharaChangeStep::Loading:
                alpha = std::max(0x80 - ChangeMenu.unk_4c * 4, 0);

                break;
            default:
                break;
        }

        const float step = TWO_PI / ChangeMenu.party_size;

        switch (ChangeMenu.step) {
            case CharaChangeStep::Loaded:
            case CharaChangeStep::Loading: {
                changeMenu_long += ChangeMenu.unk_4c;
                float turn = step / 20.0f;

                if (ChangeMenu.turn_direction < 0.0f) {
                    turn = -turn;
                }

                for (int i = 0; i < ChangeMenu.party_size; i++) {
                    const float angle = PI + (step * ChangeMenu.ring_slot[i] + turn * ChangeMenu.unk_4c);
                    ChangeMenu.ring_pos[i][0] = changeMenu_long * cos(angle);
                    ChangeMenu.ring_pos[i][1] = changeMenu_long * sin(angle);
                }

                break;
            }
            case CharaChangeStep::Turning: {
                float turn = step / 20.0f;

                if (ChangeMenu.turn_direction < 0.0f) {
                    turn = -turn;
                }

                for (int i = 0; i < ChangeMenu.party_size; i++) {
                    const float angle = PI + (step * ChangeMenu.ring_slot[i] + turn * ChangeMenu.turn_frame);
                    ChangeMenu.ring_pos[i][0] = changeMenu_long * cos(angle);
                    ChangeMenu.ring_pos[i][1] = changeMenu_long * sin(angle);
                }

                break;
            }
            default:
                break;
        }

        CTexture *portraits = QuickCharaTex;

        for (int chara = 0; chara < ChangeMenu.party_size; chara++) {
            float x = QuickCharaPos[0] + ChangeMenu.ring_pos[chara][0];
            float y = QuickCharaPos[1] + ChangeMenu.ring_pos[chara][1];
            int   shade = 0x80;
            int   u = chara * 0x30;
            int   v = 0;

            if (chara > 2) {
                u = (chara - 3) * 0x30;
                v = 0x30;
            }

            if (ChangeStatusDataPt != nullptr && ChangeStatusDataPt->hp[chara] <= 0) {
                shade = 0x40;
            }

            DrawMenu2DSprite(portraits, CRect_i_(x, y, 0x30, 0x30), CRect_i_(u, v, 0x30, 0x30), shade, shade, shade, alpha);
        }

        if (ChangeMenu.step != CharaChangeStep::Loaded && ChangeMenu.step != CharaChangeStep::Loading && ChangeMenu.step != CharaChangeStep::Blocked && ChangeMenu.step != CharaChangeStep::Unk4) {
            DrawMenuWaku(QuickCharaPos[0] - changeMenu_long - 11.0f, QuickCharaPos[1] - 12, 0x34, 0x34, 0, StayTex, 0x80);
        }

        int cursor_x = 0;
        int cursor_y = 0;

        if (ChangeMenu.step == CharaChangeStep::ItemPrompt || ChangeMenu.step == CharaChangeStep::Blocked || ChangeMenu.step == CharaChangeStep::Unk4) {
            AllFadeForMenu(0x40);
            CommonMenuMes3.stay_frame = true;
            CommonMenuMes3.auto_pos = MES_POS_CENTRE;
            MenuTextureReload(CommonMenuMes3.tex_block);
            CommonMenuMes3.Step();
            CommonMenuMes3.DrawMesWin();
            cursor_x = CommonMenuMes3.text_x - 0x1E;
            cursor_y = CommonMenuMes3.text_y + 0x10 + ChangeMenu.cursor_row * 0x18;
            ChangeMenu.cursor_x += (cursor_x - ChangeMenu.cursor_x) / 4.0f;
            ChangeMenu.cursor_y += (cursor_y - ChangeMenu.cursor_y) / 4.0f;
            cursor_x = ChangeMenu.cursor_x;
            cursor_y = ChangeMenu.cursor_y;
        } else {
            CommonMenuMes3.auto_pos = MES_POS_NONE;
            cursor_x = QuickCharaPos[0] - changeMenu_long - 32.0f;
            cursor_y = QuickCharaPos[1] + 6;

            if (cursor_x < -40 || cursor_x > 640 || cursor_y < -40 || cursor_y > 450) {
                cursor_x = 650;
                cursor_y = 450;
            }
        }

        CursorVibeCnt++;

        if (CursorVibeCnt > 1080000) {
            CursorVibeCnt = 0;
        }

        if (ChangeMenu.mode != 0) {
            MenuTextureReload(CharaChangeTexBlock);
            DrawMenu2DSprite(QuickCharaTex, CRect_i_(0x5C, 0x138, 0x1A, 0x1B), CRect_i_(0xA6, 0, 0x1A, 0x1C), alpha);
            DrawMenu2DSprite(QuickCharaTex, CRect_i_(0x76, 0x138, 0x30, 0x1B), CRect_i_(0xC0, 0, 0x20, 0x1C), alpha);
            DrawMenu2DSprite(QuickCharaTex, CRect_i_(0xA6, 0x138, 0x1A, 0x1B), CRect_i_(0xE0, 0, 0x1A, 0x1C), alpha);
            int  value = static_cast<u16>(ChangeStatusDataPt->money_signed);
            RECT value_digits = {0x70, 0x74, 0xC, 0xC};
            DrawMenuNumber(value, 0xB7, 0x140, QuickCharaTex, value_digits, 0, alpha);
            int   lang = GetMenuLangFlag();
            float help_width[7] = {10.6f, 10.6f, 10.6f, 14.0f, 12.0f, 11.8f, 10.7f};
            MenuHelpWinDraw(100, 0x154, help_width[lang], 0.9f, alpha);
            u8  count_shift[7] = {0, 0, 0, 56, 28, 16, 0};
            int count_x = count_shift[lang];
            DrawMenu2DSprite(QuickCharaTex, CRect_i_(0x76, 0x164, 0x20, 0x20), CRect_i_(0, 0x60, 0x20, 0x20), alpha);
            DrawMenu2DSprite(QuickCharaTex, CRect_i_(count_x + 0x104, 0x16C, 0x10, 0x10), CRect_i_(0x20, 0x60, 0x10, 0x10), alpha);
            MenuTextureReload(CommonMenuMes1.tex_block);

            if (lang == LANG_JAPANESE) {
                DrawMenuClsMes(&CommonMenuMes1, 0xA0, 0x154);
            } else {
                CommonMenuMes1.line_pos[0].x = 0xA0;
                CommonMenuMes1.line_pos[0].y = 0x15E;
                CommonMenuMes1.line_pos[1].x = 0xA6;
                CommonMenuMes1.line_pos[1].y = 0x172;
                CommonMenuMes1.Step();
                CommonMenuMes1.DrawMesWin();
            }

            RECT       count_digits = {0, 0x9E, 0xC, 0x12};
            ITEM_PACK *pack = &ChangeStatusDataPt->item_pack;
            int        count = GetNowItemNum(ITEM_STAND_IN_POWDER, pack);
            count_x += 0x122;
            count_x += (count_digits.width >> 1) * GetNumberKeta(count);
            DrawMenuNumber(count, count_x, 0x16C, count_digits, StayTex, 0, 0, SCREEN_HEIGHT, alpha);
        }

        if (ChangeMenu.step != CharaChangeStep::Loaded && ChangeMenu.step != CharaChangeStep::Loading && ChangeMenu.step != CharaChangeStep::Blocked && ChangeMenu.step != CharaChangeStep::Unk4) {
            DrawMenuObjectVibe(cursor_x, cursor_y, 1, 0x40);
        }

        setbilinear(1);
    }
}

int DngActItemModelReadStart(u_long128 *buffer) {
    std::array<char, 64> model_path{};
    std::array<char, 76> texture_path{};
    int                  size = 0;
    ITEM_PACK           *pack = &BtlMenuStatusPt->item_pack;

    if (pack == nullptr) {
        return 1;
    }

    u_long128 *model_buffer = MenuCalcBufAlignment(buffer);
    u_long128 *texture_buffer = MenuCalcBufAlignment(model_buffer + 0xFA1);
    StartReadBG();

    for (s16 &item_no : pack->quick_item_slot) {
        if (item_no >= ITEM_DUNGEON_START) {
            printf("mds = %p\n", static_cast<void *>(model_buffer));
            printf("img = %p\n", static_cast<void *>(texture_buffer));
            BtGetItemNamePath(model_path.data(), texture_path.data(), item_no);
            LoadFileBG(model_path.data(), model_buffer, &size);
            model_buffer += (((size >> 6) + 1) << 6) >> 4;
            LoadFileBG(texture_path.data(), texture_buffer, &size);
            texture_buffer += (((size >> 6) + 1) << 6) >> 4;
        }

        if (item_no < ITEM_DUNGEON_START) {
            item_no = -1;
        }
    }

    return 0;
}

int DngActItemModelBuild(int wait) {
    if (ReadBGSync() != 0) {
        if (wait != 0) {
            while (ReadBGSync() != 0) {
            }
        } else {
            ReadBG();
            return 0;
        }
    }

    ITEM_PACK *pack = &BtlMenuStatusPt->item_pack;
    int        items[3] = {pack->quick_item_slot[0], pack->quick_item_slot[1], pack->quick_item_slot[2]};
    int        read_index = 0;

    for (int i = 0; i < 3; i++) {
        if (items[i] < ITEM_DUNGEON_START) {
            continue;
        }

        BG_READ_INFO *model = GetReadBGFile(read_index * 2);
        BG_READ_INFO *texture = GetReadBGFile(read_index * 2 + 1);
        read_index++;

        if (model == nullptr || texture == nullptr) {
            continue;
        }

        if (activeItem.model[i + 1] != -1) {
            activeItem.models->DeleteModel(activeItem.model[i + 1]);
            activeItem.model[i + 1] = -1;
        }

        activeItem.model[i + 1] = activeItem.models->SetCashModel(items[i], reinterpret_cast<u_int *>(model->buffer), reinterpret_cast<u_int *>(texture->buffer), texture->size);
        activeItem.model[i + 5] = 0;
        activeItem.item[i + 1] = items[i];
    }

    return 1;
}

int DngActiveItemTextureCopy() {
    ITEM_PACK *pack = &UserStatus->item_pack;

    if (pack == nullptr) {
        return -1;
    }

    char source[] = "itemicon";

    for (int i = 0; i < 3; i++) {
        int item_no = pack->quick_item_slot[i];

        if (item_no >= ITEM_DUNGEON_START) {
            COM_ITEM_INFO *info = GetCommonItemInfo(item_no);

            if (info != nullptr) {
                int icon = info->icon_index;

                if (icon >= 0 && icon <= 256) {
                    int u = (icon % 8) * 32;
                    int v = (icon >> 3) * 32;
                    setItemToReserved(source, u, v, "itempack", i * 32 + 32, 0);
                }
            }
        }
    }

    return 1;
}

namespace {

/**
 * Frame image of the item view window.
 */
char item_view_frame_image[] __attribute__((section(".rodata"))) = "#frame_menuwep#640#" SCREEN_HEIGHT_STR "#4";

/**
 * Texture the debug item menu's text overlay draws its glyphs from.
 */
char debug_item_font_texture[] __attribute__((section(".rodata"))) = "dbgwork_menu";

} // namespace

int DngActiveWeaponTextureCopy() {
    int          chara = UserStatus->cur_chara;
    char         source[] = "wepicon";
    int          slot = UserStatus->equipped_weapon_slot[chara];
    WEAPON_HAVE *weapon = &UserStatus->chara_weapons[chara][slot];
    int          default_no = GetDefaultWeaponNo(chara);
    s16          pos[3][2] = {
        {0,  0 },
        {0,  32},
        {32, 32}
    };
    s16 weapons[3] = {weapon->item_no, static_cast<s16>(default_no + 1), static_cast<s16>(default_no)};

    for (int i = 0; i < 3; i++) {
        COM_ITEM_INFO *info = GetCommonItemInfo(weapons[i]);

        if (info != nullptr) {
            int icon = info->icon_index;
            int u = (icon % 8) * 32;
            int v = (icon >> 3) * 32;
            setItemToReserved(source, u, v, "itempack", pos[i][0], pos[i][1]);
        }
    }

    return 1;
}

s32 GetWeaponMsgNo(WEAPON_HAVE *weapon) {
    if (weapon == nullptr) {
        return 0;
    }

    const s16 item_no = weapon->item_no;

    if (item_no < ITEM_WEAPON_START) {
        return 0x3E7;
    }

    return GetCommonItemInfo(item_no)->msg + 0x64;
}

int GetWeaponMsgNo2(s32 item_no) {
    COM_ITEM_INFO *info = GetCommonItemInfo(item_no);

    if (info != nullptr) {
        return info->msg;
    }

    return 0;
}

void DrawWepAttach(int x, int y, WEAPON_HAVE *weapon, [[maybe_unused]] int selected, int alpha) {
    const int holes = GetWeaponHoleNum(weapon->item_no);

    if (holes <= 0) {
        return;
    }

    for (int i = 0; i < holes; i++) {
        const int item_no = weapon->attach[i].item_no;

        if (item_no >= ITEM_ATTACH_START) {
            const float size = 32.0f;
            int         u = 0;
            int         v = 0;
            CTexture   *texture = RetCTex(item_no, u, v);
            CRect_i_    src(u, v, 32, 32);
            CRect_i_    dest(x, y, size, size);
            DrawMenu2DSprite(texture, dest, src, alpha);
            int volume = GetAttachVolumeForMsg(&weapon->attach[i]);

            if (item_no == ITEM_ATTACH_SYNTHESIS_SPHERE) {
                volume = weapon->attach[i].sphere_weapon_no;
            }

            DrawAttachNumberOrWeapon(x, y, 0, 0x280, item_no, volume, alpha, 0);
        }

        x += 42.0f;
    }
}

int GetAtraTipNowHave(int tip_no, int georama_no) {
    int have = 0;

    if (tip_no < 0x28) {
        SV_EDIT_PARTS_INFO *plot_info = SaveData->GetEditPartsInfo(georama_no, tip_no);

        if (plot_info != nullptr && plot_info->obtained != 0) {
            have = 1;
        }
    } else {
        const s16 *elem = SaveData->GetElemData(georama_no);

        for (int i = 0; i < 0x80; i++) {
            if (elem[i] == tip_no - 0x28) {
                have = 1;
                break;
            }
        }

        if (have == 0) {
            for (int plot = 0; plot < 0x18; plot++) {
                SV_EDIT_PARTS_INFO *info = SaveData->GetEditPartsInfo(georama_no, plot);
                EDIT_PARTS_ATRA    *atra = GetEditAtraPartsData(georama_no, plot);

                for (int slot = 0; slot < 6; slot++) {
                    const int id = atra->elements[slot].id;

                    if (id >= 0 && id == tip_no - 0x28 && info->element_enabled[slot] != 0) {
                        have = 1;
                    }
                }
            }
        }
    }

    return have;
}

int GetDispVolumeForFloat(float volume) {
    const int whole = static_cast<int>(volume);
    int       shown = whole;

    if (volume - static_cast<float>(whole) > 0.0) {
        shown += 1;
    }

    return shown;
}

int InitItemPolygonView(int item_no, u_long128 *buffer) {
    std::array<char, 64> model_path{};
    std::array<char, 76> texture_path{};
    int                  size = 0;

    if (item_no < ITEM_ATTACH_START || item_no >= ITEM_WEAPON_START) {
        return 0;
    }

    BtGetItemNamePath(model_path.data(), texture_path.data(), item_no);
    StartReadBG();
    MDebugItemPolyViewFlag = 0;
    buffer = MenuCalcBufAlignment(buffer);

    if (LoadFileBG(model_path.data(), buffer, &size) == 0) {
        return 1;
    }

    u_long128 *texture_buffer = MenuCalcBufAlignment(buffer + (size >> 4) + 1);

    if (LoadFileBG(texture_path.data(), texture_buffer, &size) == 0) {
        return 1;
    }

    ItemPolyView = nullptr;
    return 0;
}

/** Rotation the debug item preview turns its model to. */
extern float menudebugrot[4];

/** Scale the debug item preview draws its model at. */
extern float menudebugrscale[4];

namespace {

/** Position the debug item preview draws its model at. */
float menudebugpos[4] = {4.0f, 0.0f, 0.0f, 1.0f};

/**
 * Buffer the item preview's model is read into.
 */
CDataAlloc2<1> MenuItemCashBuffer(-1);

/**
 * Sets up the item preview's model and textures once they have been read.
 */
int EnterItemPolygonView() {
    if (ReadBGSync() == 0 && polyreadflag == 0 && MDebugItemPolyViewFlag == 0) {
        LOADTEXTURE_INFO2 table[] = {
            {item_view_frame_image, 0, 0},
            {nullptr,               0, 0},
            {nullptr,               0, 0},
        };
        table[0].block_no = BtlMenuExReadBlock;
        table[1].block_no = BtlMenuExReadBlock;
        BG_READ_INFO *model = GetReadBGFile(0);
        BG_READ_INFO *texture = GetReadBGFile(1);
        table[1].name = reinterpret_cast<char *>(texture->buffer);
        TexManager.DeleteTextureBlock(BtlMenuExReadBlock);
        TexManager.CleanUpTextureList();
        TexManager.LoadTextureBlockEX(-1, table);

        if (model == nullptr) {
            return 0;
        }

        if (texture == nullptr) {
            return 0;
        }

        MenuItemCashBuffer.base = reinterpret_cast<u_char *>(texture->buffer + (texture->size >> 4) + 1);
        MenuItemCashBuffer.limit = 0x6080;
        MenuItemCashBuffer.Reset();
        MDebugItemPolyViewFlag = 1;
        ItemPolyView = reinterpret_cast<CFrame *>(LoadMDSFile(reinterpret_cast<u_int *>(model->buffer), &MenuItemCashBuffer, 0, nullptr, nullptr));
        constexpr std::array<float, 4> position = {4.0f, 0.0f, 0.0f, 1.0f};
        constexpr std::array<float, 4> rotation = {0.0f, 0.0f, 0.0f, 0.0f};
        constexpr std::array<float, 4> scale = {3.0f, 3.0f, 3.0f, 0.0f};
        std::ranges::copy(position, menudebugpos);
        std::ranges::copy(rotation, menudebugrot);
        std::ranges::copy(scale, menudebugrscale);
        return 1;
    }

    return 0;
}

/**
 * Turns and scales the item model under pad control, then draws it.
 */
void LocalDrawItemPolygonView() {
    if (MDebugItemPolyViewFlag != 0 && ItemPolyView != nullptr && polyreadflag != 0) {
        const float turn = GamePad.GetRXf();
        const float tilt = GamePad.GetRYf();
        menudebugrot[0] += tilt / 32.0f;
        menudebugrot[1] += turn / 32.0f;

        for (int i = 0; i < 3; i++) {
            if (menudebugrot[i] < -PI) {
                menudebugrot[i] += TWO_PI;
            }

            if (menudebugrot[i] > PI) {
                menudebugrot[i] -= TWO_PI;
            }
        }

        if (GamePad.Down2(PAD_CIRCLE)) {
            MDebugItemPolyViewFlag = 0;
        }

        if (GamePad.On2(PAD_TRIANGLE)) {
            for (int i = 0; i < 3; i++) {
                if (menudebugrscale[i] <= 5.0f) {
                    menudebugrscale[i] += 0.1f;
                }
            }
        }

        if (GamePad.On2(PAD_SQUARE)) {
            for (int i = 0; i < 3; i++) {
                if (0.1f < menudebugrscale[i]) {
                    menudebugrscale[i] -= 0.1f;
                }
            }
        }

        ItemPolyView->SetRotation(menudebugrot[0], menudebugrot[1], menudebugrot[2]);
        ItemPolyView->SetScale(menudebugrscale);
        ItemPolyView->SetPosition(menudebugpos);
        MGDraw(ItemPolyView);
    }
}

/**
 * Debug text the dungeon menus print their item data into.
 */
CDebugFont MenuDbgMsg;

/** State of the debug item menu. */
ITEM_AUTO_GET ItemAutoGet;

/**
 * Maps a debug item selection to its spreadsheet item number.
 */
int ConvDebugSelectToExcelListNo(int selection) {
    int item_no = selection + 0x81;

    if ((item_no > ITEM_WEAPON_SCORPION) && (item_no < ITEM_WEAPON_SUPER_STEVE)) {
        return -1;
    }

    if ((selection >= 0xF8) && (selection < 0x120)) {
        item_no = selection - 0xA7;
    }

    return item_no;
}

} // namespace

void DrawItemPolygonView() {
    MenuTextureReload(BtlMenuExReadBlock);
    MenuPolygonDraw(0x80, LocalDrawItemPolygonView);
}

void InitDebugItemGet() {
    ItemAutoGet.selection = 0x6A;
    ItemAutoGet.page = ItemAutoGet.selection >> 6;
    ItemAutoGet.show_model = 0;
    MenuDbgMsg.length = 0;
    MenuDbgMsg.texture_name = debug_item_font_texture;
    MenuDbgMsg.x = 0x15E;
    MenuDbgMsg.y = 0x50;
    MenuDbgMsg.width = 0x100;
    MenuDbgMsg.height = 0xE0;
    MenuDbgMsg.alpha = 0x60;
}

int DebugItemGetKey() {
    int       result = 0;
    const int selection = ItemAutoGet.selection;
    const int page = ItemAutoGet.page;

    if (GamePad.Down(PAD_UP)) {
        ItemAutoGet.selection -= 8;
    }

    if (GamePad.Down(PAD_DOWN)) {
        ItemAutoGet.selection += 8;
    }

    if (GamePad.Down(PAD_RIGHT)) {
        ItemAutoGet.selection += 1;
    }

    if (GamePad.Down(PAD_LEFT)) {
        ItemAutoGet.selection -= 1;
    }

    if (GamePad.Down(PAD_SELECT)) {
        if (ItemAutoGet.show_model) {
            ItemAutoGet.show_model = 0;
        } else {
            ItemAutoGet.show_model = 1;
        }

        polyreadflag = InitItemPolygonView(ConvDebugSelectToExcelListNo(ItemAutoGet.selection), BtlMenuReadBuf);
    }

    if (GamePad.Down(PAD_L2 | PAD_L1)) {
        ItemAutoGet.selection -= 0x40;
    }

    if (GamePad.Down(PAD_R2 | PAD_R1)) {
        ItemAutoGet.selection += 0x40;
    }

    if (ItemAutoGet.selection < 0) {
        ItemAutoGet.selection += 0x140;
    }

    if (ItemAutoGet.selection > 0x13F) {
        ItemAutoGet.selection -= 0x140;
    }

    ItemAutoGet.page = ItemAutoGet.selection >> 6;

    if (selection != ItemAutoGet.selection || page != ItemAutoGet.page) {
        ComMenuSePlay(MENU_SOUND_CURSOR);
    }

    if (GamePad.Down(PAD_CROSS)) {
        ComMenuSePlay(MENU_SOUND_CONFIRM);
        const int count = rand() % 3 + 1;
        BtlMenuStatusPt->GetItem(ConvDebugSelectToExcelListNo(ItemAutoGet.selection), count);
    }

    if (GamePad.Down(PAD_CIRCLE)) {
        result = -1;
        CommonMenuMes2.mes_made = result;
        ComMenuSePlay(MENU_SOUND_REFUSE);
    }

    if (GamePad.Down2(PAD_CROSS)) {
        for (int item_no = ITEM_WEAPON_START; item_no < ITEM_WEAPON_END; item_no++) {
            WEAPON_DATA *data = GetWeaponData(item_no);

            if (data != nullptr && data->flags != 0 && data->flags != 1) {
                BtlMenuStatusPt->GetItem(item_no, 0);
            }
        }

        ComMenuSePlay(MENU_SOUND_CONFIRM);
    }

    return result;
}

void DebugItemGetDraw() {
    CTexture *sheets[5] = {ItemIcon, ItemIcon, WepIcon, WepIcon, WepIcon};
    int       sheet_y[5] = {0, 0x100, 0, 0x100, 0x200};
    CTexture *sheet = sheets[ItemAutoGet.page];
    int       y = sheet_y[ItemAutoGet.page];
    int       height = 0x100;

    if (ItemAutoGet.page == 4) {
        height = 0x80;
    }

    if (sheet != nullptr) {
        MenuTextureReload(sheet->block);
    }

    MGFillBox(CRect_i_(0x460, 0x280, 0x1000, 0x800), 0, 0, 0, 0x60);
    DrawMenu2DSprite(sheet, CRect_i_(0x46, 0x50, 0x100, height), CRect_i_(0, y, 0x100, height), 0x80);
    int item_no = ConvDebugSelectToExcelListNo(ItemAutoGet.selection);

    if (ItemAutoGet.show_model) {
        DrawItemDataView(item_no);
    }

    int cell = ItemAutoGet.selection - ItemAutoGet.page * 64;
    DrawMenuObjectVibe((cell % 8) * 32 + 0x2E, (cell / 8) * 32 + 0x50, 1, 0x40);
    COM_ITEM_INFO *info = GetCommonItemInfo(item_no);

    if (info != nullptr) {
        int message = info->msg + 500;

        if (CommonMenuMes2.mes_made != message) {
            CommonMenuMes2.MakeMesWin(message);
        }
    }
}

namespace {

void DrawItemDataView(int item_no) {
    int block = 0;

    switch (BtlMenuMode) {
        case BATTLE_MENU_MODE_DUNGEON:
            block = 0xC;
            break;
        case BATTLE_MENU_MODE_TOWN:
            block = 0x1F;
            break;
        default:
            break;
    }

    MenuTextureReload(block);

    if (0 <= item_no && item_no < ITEM_ATTACH_START) {
        return;
    }

    if (MDebugItemPolyViewFlag == 0) {
        const std::string lines = std::format("-----ItemData View-----\nItemNo   :{}\ntype:    :\n", item_no);
        const std::size_t room = std::size(MenuDbgMsg.text) - 1 - static_cast<std::size_t>(MenuDbgMsg.length);
        const std::size_t count = std::min(lines.size(), room);
        std::ranges::copy_n(lines.begin(), static_cast<std::ptrdiff_t>(count), &MenuDbgMsg.text[MenuDbgMsg.length]);
        MenuDbgMsg.length += static_cast<int>(count);
        MenuDbgMsg.text[MenuDbgMsg.length] = '\0';
    }

    if (GamePad.Down(PAD_TRIANGLE)) {
        polyreadflag = InitItemPolygonView(ConvDebugSelectToExcelListNo(ItemAutoGet.selection), BtlMenuReadBuf);
    }

    if (polyreadflag == 0) {
        polyreadflag = EnterItemPolygonView();
    }

    if (polyreadflag != 0) {
        DrawItemPolygonView();
    }
}

} // namespace
