#include "shop.hpp"
#include <libvu0.h>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "battlemenu.hpp"
#include "camera.hpp"
#include "character.hpp"
#include "clsmes.hpp"
#include "dataalloc.hpp"
#include "dataread.hpp"
#include "editloop.hpp"
#include "gamepad.hpp"
#include "gamepad_port.hpp"
#include "itemdata.hpp"
#include "mainselect.hpp"
#include "mathutil.hpp"
#include "memcard.hpp"
#include "menu_draw.hpp"
#include "menu_dungeon.hpp"
#include "menu_inventory.hpp"
#include "menu_misc.hpp"
#include "menuetc.hpp"
#include "menuitemstep.hpp"
#include "mglib.hpp"
#include "rect.hpp"
#include "savedata.hpp"
#include "shop_menu.hpp"
#include "snd.hpp"
#include "stockitem.hpp"
#include "texture.hpp"
#include "userstatus.hpp"
#include "weaponlevelup.hpp"

// Retail's FishingExchangeKey, with the attachment slots of the board's page addressed on the whole
// pointer, and ChargeShopBoardDraw, drawing as many stock cells as it fills; with the static helpers
// they call.

// Defined by ps2/src/shop.cpp without a header declaration.
struct FishMenuWork {
    s16        tex_block;
    s16        tex_block2;
    s16        ready;
    s16        warning;
    s32        point;
    s32        cursor_y;
    s16        mode;
    s16        confirm;
    s16        cursor;
    s16        fade_mode;
    s16        top;
    u8         unk_1A[6];
    s32        fade_count;
    u_long128 *buffer;
};

extern FishMenuWork FishMenu;
extern CTexture    *FishMenuTex;


extern ShopMenuWork ShopMenu;
extern CTexture    *ShopBoard;
extern CStockItem  *ShopStockPt;
extern s16          ChargeShopMax[3];

// The bodies are retail's, spelled as MWCC took them.
#pragma clang diagnostic ignored "-Wwritable-strings"

namespace {

FISH_EXCHANGE_ITEM *GetExchangeItemList(int index) {
    return &exitemlst[index];
}

int AlreadyGetMardanWeapon() {
    int flag = SaveData->GetGameFlag(0xCA);

    if (flag == 0) {
        if (GetMardanGareyanFlag() > 0) {
            return 1;
        }

        return 2;
    }

    if (flag == 1) {
        return 0;
    }

    return flag;
}

int FishMenuTextureLoad() {
    int done = 0;

    if (FishMenu.ready == 0) {
        if (ReadBGSync() == 0) {
            BG_READ_INFO     *file = GetReadBGFile(0);
            LOADTEXTURE_INFO2 texture[3] = {
                {FishFrameImage, 0, 0},
                {NULL,           0, 0},
                {NULL,           0, 0},
            };
            texture[0].block_no = FishMenu.tex_block;
            texture[1].block_no = FishMenu.tex_block;
            texture[1].name = (char *) GetPackFile((u_int *) file->buffer, "fishing.img", NULL);
            TexManager.DeleteTextureBlock(FishMenu.tex_block);
            TexManager.CleanUpTextureList();
            TexManager.LoadTextureBlockEX(-1, texture);
            FishMenuTex = TexManager.GetTexture("fishbrd", -1);
            WepIcon = TexManager.GetTexture("wepicon", -1);
            ItemIcon = TexManager.GetTexture("itemicon", -1);
            InitMenuMesSet(MENU_MES_SET_ALLMENU, (short *) GetPackFile((u_int *) file->buffer, FishMessageFile, NULL));
            CommonMenuMes2.mes_made = -1;
            AtoraNameMes.Preset(MES_PRESET_SYSTEM);

            for (int i = 0; i < 5; i++) {
                AtoraNameMes.mes_no[i] = GetExchangeItemList(i)->item_no + 100;
            }

            AtoraNameMes.narrow_gaiji = true;
            AtoraNameMes.style = MES_EDGE_DOUBLE;
            AtoraNameMes.value_signed = false;
            AtoraNameMes.value_show = true;
            AtoraNameMes.mes_made = -1;
            AtoraNameMes.MakeMesWin(0xC8);
            AtoraNameMes.Step();
            FishMenu.ready = 1;
            CommonMenuMes3.value_signed = false;
            CommonMenuMes3.value_show = true;
            CommonMenuMes3.stay_frame = true;
            CommonMenuMes3.value = 0;
            int digits = GetNumberKeta(0);
            CommonMenuMes3.mes_made = -1;
            CommonMenuMes3.MakeMesWin(digits + 0xCD);
            FishMenu.cursor_y = 0x7E;
            done = 1;
        }
    } else {
        done = 1;
    }

    return done;
}

void ShopIconDraw(short *item_no, unsigned char *value, int count, int x, int y, int clip_top, int clip_bottom, int alpha) {
    int draw_x = x;
    int draw_y = y;
    int i;

    for (i = 0; i < count; i++) {
        DrawIconParts(item_no[i], draw_x, draw_y, clip_top, clip_bottom, alpha, value[i]);
        draw_x += 0x28;

        if (i % 5 == 4) {
            draw_x = x;
            draw_y += 0x28;
        }
    }
}

void ChargeShopMaxDraw(int max, int x, int y, int alpha) {
    int left = x + 0xD2;
    int top = y - 0x28;

    DrawMenu2DSprite(ShopBoard, CRect_i_(left, top + 1, 0x30, 0x2F), CRect_i_(0x100, 0x90, 0x30, 0x30), alpha);
    RECT digits = {0x90, 0xDC, 12, 13};
    left = x + 0xF7;
    top = y - 0xE;
    DrawMenuNumber(max, left, top, ShopBoard, digits, 1, alpha);
    left = x + 0xF2;
    top = y - 0x22;
    int count = 0;
    int i;

    switch (ShopMenu.board.page) {
        case BOARD_PAGE_ITEM:
            for (i = 0; i < 60; i++) {
                if (ShopStockPt->dungeon_items[i] >= ITEM_DUNGEON_START) {
                    count++;
                }
            }

            break;
        case BOARD_PAGE_WEAPON:
            for (i = 0; i < 30; i++) {
                if (ShopStockPt->weapons[i].item_no >= ITEM_WEAPON_START) {
                    count++;
                }
            }

            break;
        case BOARD_PAGE_ATTACH:
            for (i = 0; i < 30; i++) {
                if (ShopStockPt->attachments[i].item_no >= ITEM_ATTACH_START) {
                    count++;
                }
            }

            break;
    }

    DrawMenuNumber(count, left, top, ShopBoard, digits, 1, alpha);
}

} // namespace

// Draws the 60 cells it fills; retail drew 128 from 120-cell arrays.
PORT_OVERRIDE void ChargeShopBoardDraw(int x, int y, int alpha) {
    s16 items[60];
    u8  values[60];
    int top = y + 9;
    int bottom = y + 0xA9;
    int board_y;
    int left = x + 0x14;

    board_y = y + 6 - ShopMenu.stock_top_row * 0x28;
    ShopMenu.stock_y += ((float) board_y - ShopMenu.stock_y) / 4.0f;
    board_y = ShopMenu.stock_y;
    DrawPerBoardDraw(0, 0x64, left, board_y, top, bottom, ShopBoard, alpha);

    int board_mode = ShopMenu.board.page;
    int i = 0;

    switch (board_mode) {
        case BOARD_PAGE_WEAPON:
            for (i = 0; i < 30; i++) {
                items[i] = ShopStockPt->weapons[i].item_no;
                values[i] = 0;
            }

            break;
        case BOARD_PAGE_ITEM:
            for (i = 0; i < 60; i++) {
                items[i] = ShopStockPt->dungeon_items[i];
                values[i] = 0;
            }

            break;
        case BOARD_PAGE_ATTACH:
            for (i = 0; i < 30; i++) {
                items[i] = ShopStockPt->attachments[i].item_no;

                if (items[i] >= ITEM_ATTACH_STAT_START && items[i] < ITEM_ATTACH_GEM_START) {
                    values[i] = (&ShopStockPt->attachments[i].item_no)[items[i] - ITEM_ATTACH_STAT_START + 4];
                } else if (items[i] == ITEM_ATTACH_SYNTHESIS_SPHERE) {
                    values[i] = ShopStockPt->attachments[i].sphere_weapon_no;
                } else {
                    values[i] = 0;
                }
            }

            break;
    }

    for (; i < 60; i++) {
        items[i] = -1;
        values[i] = 0;
    }

    ShopIconDraw(items, values, 60, left + 4, board_y + 6, top, bottom, 0x80);

    int tag = 0;

    switch (board_mode) {
        case BOARD_PAGE_ITEM:
            tag += 0xC;
            break;
        case BOARD_PAGE_WEAPON:
            tag += 0xA;
            break;
        case BOARD_PAGE_ATTACH:
            tag = 1;
            break;
    }

    PersonalBoardTagDraw(board_mode, x, y, ShopBoard, tag, alpha);
    PersonalBoardDrawWaku(x, y, ShopBoard, alpha);
    PersonalBoardScrlBarDraw(ChargeShopMax[board_mode], x, y, ShopMenu.stock_scroll, ShopMenu.stock_top_row, ShopBoard, alpha);
    ChargeShopMaxDraw(ChargeShopMax[board_mode], x, y, alpha);
}

PORT_OVERRIDE int FishingExchangeKey() {
    int result = 0;

    ReadBG();

#ifdef PAL
    // Debug shortcut: adds a Mardan Garayan catch.
    if (DebugMode && GamePad.Down2(PAD_TRIANGLE)) {
        s32 &caught = SaveData->mardan_garayan_caught;

        caught++;
        SetFishMardanGarayanNum(1);
    }

#endif
    int mardan = AlreadyGetMardanWeapon();
    int party = SaveData->GetDngStatus()->party_size;

    if (party <= 0) {
        party = 1;
    }

    int last_prize = party + 0x19;
    int last_top = party + 0x16;

    if (mardan == 1) {
        last_prize++;
        last_top++;
    }

    int count;

    switch (FishMenu.fade_mode) {
        case FISH_EXCHANGE_FADE_IN:
            FishMenuTextureLoad();
            FishMenu.fade_count++;

            if (FishMenu.ready != 0 && FishMenu.fade_count > 32) {
                FishMenu.fade_mode = FISH_EXCHANGE_SELECT;
            }

            break;
        case FISH_EXCHANGE_FADE_OUT:
            FishMenu.fade_count++;

            if (FishMenu.fade_count > 32) {
                result = 1;
            }

            break;
        case FISH_EXCHANGE_SELECT: {
            int cursor = FishMenu.cursor;
            int top = FishMenu.top;

            if (GamePad.Down(PAD_R2 | PAD_R1 | PAD_RIGHT) != 0) {
                FishMenu.top += 5;
                FishMenu.cursor += 5;
            }

            if (GamePad.Down(PAD_L2 | PAD_L1 | PAD_LEFT) != 0) {
                FishMenu.top -= 5;
                FishMenu.cursor -= 5;

                if (FishMenu.cursor < 0 || FishMenu.top < 0) {
                    FishMenu.cursor = 0;
                    FishMenu.top = 0;
                }
            }

            if (GamePad.Down(PAD_UP) != 0) {
                FishMenu.cursor--;

                if (FishMenu.cursor < 0) {
                    FishMenu.cursor = 0;
                }

                if (FishMenu.cursor < FishMenu.top) {
                    FishMenu.top--;
                }

                if (FishMenu.top < 0) {
                    FishMenu.top = 0;
                    FishMenu.cursor = 0;
                }
            }

            if (GamePad.Down(PAD_DOWN) != 0) {
                FishMenu.cursor++;

                if (last_prize < FishMenu.cursor) {
                    FishMenu.cursor = last_prize;
                }

                if (FishMenu.top < FishMenu.cursor - 4) {
                    FishMenu.top++;

                    if (last_top < FishMenu.top) {
                        FishMenu.top = last_top - 1;
                    }
                }
            }

#ifdef PAL
            // Debug shortcut: raises or lowers the points to spend.
            if (DebugMode) {
                if (GamePad.On2(PAD_CROSS) && FishMenu.point < 9999) {
                    FishMenu.point++;
                }

                if (GamePad.On2(PAD_CIRCLE) && FishMenu.point > 0) {
                    FishMenu.point--;
                }
            }

#endif
            if (last_prize < FishMenu.cursor) {
                FishMenu.cursor = last_prize;
            }

            if (last_top < FishMenu.top) {
                FishMenu.top = last_top - 1;
            }

            if (cursor != FishMenu.cursor) {
                ComMenuSePlay(MENU_SOUND_CURSOR);
            }

            FISH_EXCHANGE_ITEM *prize = GetExchangeItemList(FishMenu.cursor);

            if (mardan == 1 && FishMenu.cursor == last_prize) {
                prize = GetExchangeItemList(0x20);
            }

            if (prize != NULL) {
                COM_ITEM_INFO *info = GetCommonItemInfo(prize->item_no);

                if (info != NULL) {
                    int mes_no = info->msg + 500;

                    if (CommonMenuMes2.mes_made != mes_no) {
                        CommonMenuMes2.MakeMesWin(mes_no);
                    }
                }
            }

            if (GamePad.Down(PAD_CROSS) != 0) {
                CUserStatus *status = (CUserStatus *) SaveData->GetDngStatus();
                int          full = 0;
                int          kind = WhatIsKindofItem(prize->item_no);
                count = 0;

                switch (kind) {
                    case BOARD_PAGE_ITEM: {
                        int        i;
                        int        k;
                        ITEM_PACK *pack = &status->item_pack;

                        for (i = 0; i < 3; i++) {
                            count += pack->quick_item_qty[i];
                        }

                        for (k = 0; k < pack->num; k++) {
                            if (pack->item[k] >= ITEM_DUNGEON_START) {
                                count++;
                            }
                        }

                        if (count >= pack->num) {
                            full = 1;
                        }

                        break;
                    }
                    case BOARD_PAGE_WEAPON: {
                        int          i;
                        int          owner = WhoIsWeaponEquip(prize->item_no);
                        int          held = 0;
                        WEAPON_HAVE *weapons = status->chara_weapons[owner];

                        for (i = 0; i < 10; i++) {
                            if (weapons[i].item_no >= ITEM_WEAPON_START) {
                                held++;
                            }
                        }

                        if (held >= 10) {
                            full = 1;
                        }

                        break;
                    }
                    case BOARD_PAGE_ATTACH: {
                        int             held;
                        DNG_CONSUMABLE *attach = status->consumable_items;
                        std::uintptr_t  entry;
                        held = 0;

                        for (; count < 40; count++) {
                            entry = count * sizeof(DNG_CONSUMABLE);
                            entry = (std::uintptr_t) attach + entry;

                            if (((DNG_CONSUMABLE *) entry)->id >= ITEM_ATTACH_START) {
                                held++;
                            }
                        }

                        if (held >= 40) {
                            full = 1;
                        }

                        break;
                    }
                }

                if (prize->price > FishMenu.point) {
                    FishMenu.fade_mode = FISH_EXCHANGE_REFUSE;
                    FishMenu.warning = 14;
                    ComMenuSePlay(MENU_SOUND_REFUSE);
                } else if (full != 0) {
                    FishMenu.warning = kind;
                    FishMenu.fade_mode = FISH_EXCHANGE_REFUSE;
                    ComMenuSePlay(MENU_SOUND_REFUSE);
                } else {
                    FishMenu.confirm = 1;
                    FishMenu.cursor_y = FishMenu.confirm * 0x24 + 0x102;
                    FishMenu.fade_mode = FISH_EXCHANGE_CONFIRM;
                    ComMenuSePlay(MENU_SOUND_CONFIRM);
                }
            } else if (GamePad.Down(PAD_CIRCLE) != 0) {
                FishMenu.fade_mode = FISH_EXCHANGE_FADE_OUT;
                FishMenu.fade_count = 0;
                ComMenuSePlay(MENU_SOUND_REFUSE);
            }

            if (top != FishMenu.top) {
                FISH_EXCHANGE_ITEM *entry = GetExchangeItemList(FishMenu.top);

                for (int i = 0; i < 5; i++) {
                    if (mardan == 1 && FishMenu.cursor == FishMenu.top + i && FishMenu.cursor == last_prize) {
                        entry = GetExchangeItemList(0x20);
                    }

                    if (entry == NULL) {
                        printf("getinfo is NULL\n");
                    } else {
                        COM_ITEM_INFO *info = GetCommonItemInfo(entry->item_no);

                        if (info != NULL) {
                            AtoraNameMes.mes_no[i] = info->msg + 100;
                            entry++;
                        }
                    }
                }

                AtoraNameMes.mes_made = -1;
                AtoraNameMes.MakeMesWin(0xC8);
            }

            break;
        }
        case FISH_EXCHANGE_CONFIRM: {
            if (GamePad.Down(PAD_UP | PAD_DOWN) != 0) {
                if (FishMenu.confirm != 0) {
                    FishMenu.confirm = 0;
                } else {
                    FishMenu.confirm = 1;
                }

                ComMenuSePlay(MENU_SOUND_CURSOR);
            }

            FISH_EXCHANGE_ITEM *prize;

            if (GamePad.Down(PAD_CROSS) != 0) {
                int enough = 1;
                prize = GetExchangeItemList(FishMenu.cursor);

                if (mardan == 1 && FishMenu.cursor == last_prize) {
                    prize = GetExchangeItemList(0x20);
                }

                if (prize == NULL) {
                    ComMenuSePlay(MENU_SOUND_REFUSE);
                    break;
                }

                int price = prize->price;

                if (FishMenu.point < price) {
                    enough = 0;
                }

                if (FishMenu.confirm == 0) {
                    if (enough != 0) {
                        ComMenuSePlay(SE_PURCHASE);
                        ((CDngStatusData *) SaveData->GetDngStatus())->GetItem(prize->item_no, 0);
                        FishMenu.point -= price;

                        if (prize->item_no == ITEM_WEAPON_MARDAN_EINS) {
                            SetAlreadyGetMardanWeapon(1);
                            ClearFishMardanGarayanNum();
                            FishMenu.top--;
                            FishMenu.cursor--;
                        }

                        if (FishMenu.point < 0) {
                            FishMenu.point = 0;
                        }
                    } else {
                        ComMenuSePlay(MENU_SOUND_REFUSE);
                    }
                } else {
                    ComMenuSePlay(MENU_SOUND_REFUSE);
                }

                FishMenu.cursor_y = (FishMenu.cursor - FishMenu.top) * 0x22 + 0x7E;
                FishMenu.fade_mode = FISH_EXCHANGE_SELECT;
            } else if (GamePad.Down(PAD_CIRCLE) != 0) {
                ComMenuSePlay(MENU_SOUND_REFUSE);
                FishMenu.fade_mode = FISH_EXCHANGE_SELECT;
            }

            prize = GetExchangeItemList(FishMenu.cursor);

            if (mardan == 1 && FishMenu.cursor == last_prize) {
                prize = GetExchangeItemList(0x20);
            }

            if (prize != NULL) {
                COM_ITEM_INFO *info = GetCommonItemInfo(prize->item_no);

                if (info != NULL) {
                    int msg = info->msg;

                    if (CommonMenuMes1.mes_made != 0xCA || CommonMenuMes1.mes_no[0] != msg + 100 || CommonMenuMes1.values[0] != prize->price) {
                        CommonMenuMes1.mes_made = -1;
                        CommonMenuMes1.mes_no[0] = msg + 100;
                        CommonMenuMes1.values[0] = prize->price;
                        CommonMenuMes1.MakeMesWin(0xCA);
                    }
                }
            }

            break;
        }
        case FISH_EXCHANGE_REFUSE: {
            if (GamePad.Down(PAD_CIRCLE | PAD_CROSS) != 0) {
                FishMenu.fade_mode = FISH_EXCHANGE_SELECT;
                ComMenuSePlay(MENU_SOUND_REFUSE);
            }

            if (CommonMenuMes1.mes_made != FishMenu.warning + 0xCB) {
                CommonMenuMes1.MakeMesWin(FishMenu.warning + 0xCB);
            }

            break;
        }
    }

    CursorVibeCnt++;

    if (CursorVibeCnt >= 0x066FF300) {
        CursorVibeCnt = 0;
    }

    return result;
}

// Cross and triangle wait a frame when that frame's press moves the cursor off the stock board (#109).
PORT_OVERRIDE int CommonShopLoop() {
    int done = 0;

    ShopMenu.key_used = 0;

    switch (ChargeOrShopFlag) {
        case SHOP_KIND_CHARGE:
            done = ChargeShopLoop();
            break;
        case SHOP_KIND_ITEM: {
            int  cursor = ShopMenu.board.cursor;
            bool leaving = ShopMenu.ready != 0 && ShopMenu.talk_mode == SHOP_TALK_NONE && ShopMenu.side == SHOP_SIDE_STOCK &&
                           ((GamePad.Down(PAD_RIGHT) && cursor % 5 == 4) || (GamePad.Down(PAD_DOWN) && cursor >= 25));
            PadHoldDown(leaving ? PAD_CROSS | PAD_TRIANGLE : 0);
            done = ItemShopLoop2();
            PadHoldDown(0);
            break;
        }
    }

    return done;
}
