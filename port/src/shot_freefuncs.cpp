#include "shot_freefuncs.hpp"

#include <algorithm>
#include <cmath>

#include "btactstatus.hpp"
#include "cloth.hpp"
#include "clothread.hpp"
#include "dun/gameloop.hpp"
#include "hitvalue.hpp"
#include "itemdata.hpp"
#include "mathutil.hpp"
#include "mglib.hpp"
#include "monstorunit.hpp"
#include "rect.hpp"
#include "snd.hpp"
#include "texture.hpp"
#include "userstatus.hpp"
#include "menu_inventory.hpp"

extern int statusAlarmRate;

namespace {

// A status bar's fill over the trough of its frame, which is cells caps and middles of four columns
// from x 0x30. The trough runs from the left cap's fourth column to the right cap's second, on the
// frame's rows 4 to 10. Retail places the fill a row and a column inside it and measures its length
// apart from the frame's, which the GS's field rows hide and full rows do not.
void StatusBarFill(int frame_y, int cells, float value, float max, spRGBA *left, spRGBA *right) {
    const int trough = cells * 4 - 5;
    const int length = std::clamp(static_cast<int>(static_cast<float>(trough) * value / max), 0, trough);

    set2DSpriteC4(Vif1Packet, CRect_i_(0x33, frame_y + 4, length, 7), left, right, left, right);
}

} // namespace

// Retail brackets the copy with TEXFLUSH; the renderer orders copies and draws itself.
PC_OVERRIDE void setItemToReserved(char *page_name, int x, int y, char *item_name, int dsax, int dsay) {
    CTexture *page = TexManager.GetTexture(page_name, -1);
    CTexture *item = TexManager.GetTexture(item_name, -1);

    int sbp = page->tex0 & 0x3FFF;
    int dbp = item->tex0 & 0x3FFF;
    int sbw = (page->tex0 >> 14) & 0x3F;
    int dbw = (item->tex0 >> 14) & 0x3F;

    MoveImageTest(Vif1Packet, sbp, sbw, 0x13, CRect_i_(x, y, 0x20, 0x20), dbp, dbw, 0x13, dsax, dsay, 0);
}

// Retail's but for the life and weapon bars' fills, PAL's branch only.
PC_OVERRIDE void topStatusInfo(int y, int selected_item, int floor) {
    int       alpha;
    CTexture *icons;
    CTexture *frame;
    int       cells;
    spRGBA   *upper;
    spRGBA   *lower;
    y -= 10;
    y += BtActStatus.hud_shake_y;
    icons = TexManager.GetTexture("itempack", -1);
    frame = TexManager.GetTexture("stayframe", -1);

    float life = UserStatus->hp[UserStatus->cur_chara];
    float life_max = UserStatus->max_hp[UserStatus->cur_chara];
    int   length = (int) (0.74f * life_max);

    if (length < 2) {
        length = 2;
    }

    cells = (length >> 2) + 1;

    if (cells < 2) {
        cells = 2;
    }

    alpha = 0x80;

    if (life <= 0.2f * life_max) {
        alpha = statusAlarmRate;
    }

    upper = (spRGBA *) BtGetStatusPal(0, life_max, life);
    lower = (spRGBA *) BtGetStatusPal2(0, life_max, life);
    set2DSprite(Vif1Packet, icons, CRect_i_(0x30, y, 4, 0x10), CRect_i_(0x68, 0x70, 4, 0x10), alpha);

    for (int i = 0; i < cells - 2; i++) {
        set2DSprite(Vif1Packet, icons, CRect_i_(i * 4 + 0x34, y, 4, 0x10), CRect_i_(0x6C, 0x70, 4, 0x10), alpha);
    }

    set2DSprite(Vif1Packet, icons, CRect_i_((cells - 1) * 4 + 0x30, y, 4, 0x10), CRect_i_(0x70, 0x70, 4, 0x10), alpha);

    if (life > 0.0f) {
        StatusBarFill(y, cells, life, life_max, upper, lower);
    }

    cells = (cells - 1) * 4 + 0x38;
    cells += ValuePrint(cells, y + 7, (int) life, 0, alpha) * 10;
    set2DSprite(Vif1Packet, frame, CRect_i_(cells, y + 7, 0xC, 0xC), CRect_i_(0x78, 0xB0, 0xC, 0xC), alpha);
    ValuePrint(cells + 10, y + 7, (int) life_max, 0, alpha);
    set2DSprite(Vif1Packet, icons, CRect_i_(0x20, y, 0x10, 0x10), CRect_i_(0x40, 0xA0, 0x10, 0x10), alpha);

    WEAPON_HAVE *weapon = &UserStatus->chara_weapons[UserStatus->cur_chara][UserStatus->equipped_weapon_slot[UserStatus->cur_chara]];
    float        durability_max = weapon->durability;
    float        durability = weapon->durability_f;
    float        bar = 1.4949495f * durability_max;
    length = (int) bar;

    if (length < 2) {
        length = 2;
    }

    cells = (length >> 2) + 2;

    if (cells < 2) {
        cells = 2;
    }

    alpha = 0x80;

    if (durability <= 0.2f * durability_max) {
        alpha = statusAlarmRate;
    }

    upper = (spRGBA *) BtGetStatusPal(1, durability_max, durability);
    lower = (spRGBA *) BtGetStatusPal2(1, durability_max, durability);
    set2DSprite(Vif1Packet, icons, CRect_i_(0x30, y + 0x11, 4, 0x10), CRect_i_(0x68, 0x70, 4, 0x10), alpha);

    for (int i = 0; i < cells - 2; i++) {
        set2DSprite(Vif1Packet, icons, CRect_i_(i * 4 + 0x34, y + 0x11, 4, 0x10), CRect_i_(0x6C, 0x70, 4, 0x10), alpha);
    }

    set2DSprite(Vif1Packet, icons, CRect_i_((cells - 1) * 4 + 0x30, y + 0x11, 4, 0x10), CRect_i_(0x70, 0x70, 4, 0x10), alpha);

    if (durability > 0.0f) {
        StatusBarFill(y + 0x11, cells, durability, durability_max, upper, lower);
    }

    int width;
    int exp_max = GetWeaponMaxExp(weapon);
    width = (int) bar;
    MGFillBox(CRect_i_(0x340, ((y + 0x1F) >> 1) * 16, (width - 1) * 16, 0x10), 0x40, 0x40, 0x40, alpha);

    if (weapon->experience > 0) {
        int filled = (int) ((float) weapon->experience * ((float) width / (float) exp_max));
        MGFillBox(CRect_i_(0x340, ((y + 0x1F) >> 1) * 16, (filled - 1) * 16, 0x10), 0, 0xA2, 0xFF, alpha);
    }

    cells = (cells - 1) * 4 + 0x38;
    int shown = (int) durability;

    if (durability - (float) shown > 0.0f) {
        shown++;
    }

    cells += ValuePrint(cells, y + 0x18, shown, 0, alpha) * 10;
    set2DSprite(Vif1Packet, frame, CRect_i_(cells, y + 0x18, 0xC, 0xC), CRect_i_(0x78, 0xB0, 0xC, 0xC), alpha);
    ValuePrint(cells + 10, y + 0x18, (int) durability_max, 0, alpha);
    set2DSprite(Vif1Packet, icons, CRect_i_(0x20, y + 0x10, 0x10, 0x10), CRect_i_(0x40, 0xB0, 0x10, 0x10), alpha);

    float water_max = UserStatus->water_max[UserStatus->cur_chara];
    float water = UserStatus->water_now[UserStatus->cur_chara];
    alpha = 0x80;

    if (water < 0.15f * water_max) {
        alpha = statusAlarmRate;
    }

    set2DSprite(Vif1Packet, icons, CRect_i_(0x20, y + 0x22, 0x12, 0x14), CRect_i_(0x64, 0x84, 0x12, 0x14), alpha);
    int drops = (int) water_max / 10;

    for (cells = 0; cells < drops; cells++) {
        set2DSprite(Vif1Packet, icons, CRect_i_(cells * 0x12 + 0x32, y + 0x22, 0x12, 0x14), CRect_i_(0x64, 0x98, 0x12, 0x14), alpha);
    }

    if ((int) water_max % 10 != 0) {
        set2DSprite(Vif1Packet, icons, CRect_i_(drops * 0x12 + 0x32, y + 0x22, 0x12, 0x14), CRect_i_(0x64, 0xAC, 0x18, 0x14), alpha);
    }

    int full_drops = (int) water / 10;

    for (cells = 0; cells < full_drops; cells++) {
        set2DSprite(Vif1Packet, icons, CRect_i_(cells * 0x12 + 0x33, y + 0x22, 0x10, 0x14), CRect_i_(0, 0x48, 0x10, 0x14), 0x50);
    }

    int leftover = (int) water % 10;

    if (leftover != 0) {
        set2DSprite(Vif1Packet, icons, CRect_i_(full_drops * 0x12 + 0x33, y + 0x22, 0x10, 0x14), CRect_i_((3 - (int) ((float) leftover / 2.5f)) * 16, 0x48, 0x10, 0x14), 0x50);
    }

    static float popupYRate = -PI;
    popupYRate += 0.20943952f;

    if (!(popupYRate < PI)) {
        popupYRate -= TWO_PI;
    }

    static float popupRGBRate = -PI;
    popupRGBRate += SIXTEENTH_PI;

    if (!(popupRGBRate < 0.0f)) {
        popupRGBRate -= PI;
    }

    ITEM_PACK *pack = &UserStatus->item_pack;

    for (int i = 0; i < 3; i++) {
        int bob = 0;
        int glow = 0;

        if (BtBySpeedFlag != 0 && pack->quick_item_slot[i] == ITEM_UNUSED_156) {
            glow = (int) (64.0f * sinf(popupRGBRate)) + 0x3F;
            bob = 0;
        }

        set2DSprite(Vif1Packet, icons, CRect_i_(i * 40 + 0x126, y + 2, 0x24, 0x24), CRect_i_(0x24, 0x5C, 0x24, 0x24), glow + 0x80);

        if (pack->quick_item_slot[i] != -1) {
            set2DSprite(Vif1Packet, icons, CRect_i_(i * 40 + 0x128, y + 4 + bob, 0x1F, 0x1F), CRect_i_((i << 5) + 0x20, 0, 0x1F, 0x1F));

            if (pack->quick_item_qty[i] > 1) {
                int tens = pack->quick_item_qty[i] / 10;

                if (tens > 0) {
                    set2DSprite(Vif1Packet, frame, CRect_i_(i * 40 + 0x12F, y + 0x17, 0xC, 0xC), CRect_i_(tens * 12, 0xD4, 0xC, 0xC));
                }

                set2DSprite(Vif1Packet, frame, CRect_i_(i * 40 + 0x13B, y + 0x17, 0xC, 0xC), CRect_i_(pack->quick_item_qty[i] % 10 * 12, 0xD4, 0xC, 0xC));
            }
        }
    }

    set2DSprite(Vif1Packet, icons, CRect_i_(0x1FC, y, 0x66, 0x29), CRect_i_(0x9A, 1, 0x66, 0x29));

    if (BtUraDongeon != 0) {
        set2DSprite(Vif1Packet, icons, CRect_i_(0x23C, y, 0x26, 0x29), CRect_i_(0xDA, 0x2B, 0x26, 0x29));
    }

    if (BtEquipMasuisyou != 0) {
        set2DSprite(Vif1Packet, icons, CRect_i_(0x1FE, y + 5, 0x20, 0x20), CRect_i_(0, 0xA0, 0x20, 0x20));
    }

    if (BtEquipMap != 0) {
        set2DSprite(Vif1Packet, icons, CRect_i_(0x21E, y + 5, 0x20, 0x20), CRect_i_(0x20, 0xA0, 0x20, 0x20));
    }

    int last_floor = maxFloorTbl__3[UserStatus->cur_georama];

    if (floor + 1 == last_floor) {
        set2DSprite(Vif1Packet, frame, CRect_i_(0x23C, y + 0x12, 0x26, 0x11), CRect_i_(0x78, 0x9E, 0x26, 0x12));
    } else {
        int x;

        if (floor + 1 >= 10) {
            x = 0x245;
        } else {
            x = 0x23F;
        }

        if (floor + 1 >= 10) {
            set2DSprite(Vif1Packet, frame, CRect_i_(x - 4, y + 0xA, 0xE, 0x11), CRect_i_((floor + 1) / 10 * 12, 0x9E, 0xC, 0x12));
        }

        set2DSprite(Vif1Packet, frame, CRect_i_(x + 9, y + 0xA, 0xE, 0x11), CRect_i_((floor + 1) % 10 * 12, 0x9E, 0xC, 0x12));
    }

    selected_item *= 40;
    set2DSprite(Vif1Packet, frame, CRect_i_(selected_item + 0xFC, y, 0x10, 0x10), CRect_i_(0x4A, 0x48, 0x10, 0x10));
    set2DSprite(Vif1Packet, frame, CRect_i_(selected_item + 0x113, y, 0x10, 0x10), CRect_i_(0x5A, 0x48, 0x10, 0x10));
    set2DSprite(Vif1Packet, frame, CRect_i_(selected_item + 0xFC, y + 0x16, 0x10, 0x10), CRect_i_(0x4A, 0x58, 0x10, 0x10));
    set2DSprite(Vif1Packet, frame, CRect_i_(selected_item + 0x113, y + 0x16, 0x10, 0x10), CRect_i_(0x5A, 0x58, 0x10, 0x10));

    y -= BtActStatus.hud_shake_y;
    alpha = y + 0x6A;
    set2DSprite(Vif1Packet, icons, CRect_i_(0x18, 0x1AC, 0x2A, 0x1D), CRect_i_(0x78, 0x37, 0x2A, 0x1D), alpha);
    set2DSprite(Vif1Packet, icons, CRect_i_(0x45, 0x1A8, 0x2B, 0x10), CRect_i_(0xA2, 0x37, 0x2B, 0x10), alpha);
    int chara = UserStatus->cur_chara;
    int u = 0;
    int v = 0;
    int item = UserStatus->chara_weapons[chara][UserStatus->equipped_weapon_slot[chara]].item_no;

    if (item == defWeapon__2[UserStatus->cur_chara] + 1) {
        v = 0x20;
    }

    if (item == defWeapon__2[UserStatus->cur_chara]) {
        v = u = 0x20;
    }

    set2DSprite(Vif1Packet, icons, CRect_i_(0x1D, 0x1A4, 0x20, 0x20), CRect_i_(u, v, 0x20, 0x20), alpha);
}
