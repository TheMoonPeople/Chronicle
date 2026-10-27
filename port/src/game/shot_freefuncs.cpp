#include "shot_freefuncs.hpp"

#include <libvu0.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <numbers>
#include <print>
#include <string>

#include "btactstatus.hpp"
#include "btmisc.hpp"
#include "character.hpp"
#include "charaheight.hpp"
#include "cloth.hpp"
#include "clothread.hpp"
#include "clsmes.hpp"
#include "dataalloc.hpp"
#include "dataread.hpp"
#include "dranmapfield.hpp"
#include "dun/gameloop.hpp"
#include "dungeoneventman.hpp"
#include "dungeonmap.hpp"
#include "frame.hpp"
#include "healeffect.hpp"
#include "hitvalue.hpp"
#include "itemdata.hpp"
#include "mainselect.hpp"
#include "mathutil.hpp"
#include "menu_inventory.hpp"
#include "mglib.hpp"
#include "monstorunit.hpp"
#include "motionmodel.hpp"
#include "nowload.hpp"
#include "rect.hpp"
#include "snd.hpp"
#include "texture.hpp"
#include "userstatus.hpp"

namespace dc::game {

s32   DebugStatus[21];
float StatusColor[3];
int   statusAlarmRate;

namespace {

/**
 * Number of floors in each dungeon.
 */
constexpr std::array<int, 7> maxFloorTbl = {15, 17, 18, 18, 15, 25, 100};

/**
 * Number of ripple slots on the water.
 */
constexpr int WATER_WAVE_LING_COUNT = 6;

/**
 * The water the party is standing in and the nearest water surface.
 */
CHECK_WATER_INFO CheckWaterInfo;

/**
 * Rings spreading across the water.
 */
std::array<WATER_WAVE_LING, WATER_WAVE_LING_COUNT> WaterWaveLing;

/**
 * Name of the map held in the jump cache.
 */
std::string BtCfgCash;

/**
 * Frames remaining before the next ring appears.
 */
int WaterWaveLingWait;

/**
 * Frames remaining before healing water restores the party again.
 */
int healingSpeed;

/**
 * Whether the healing effect still owes the scene its ambient colour back.
 */
int healingSpeed_flg;

/**
 * Phase of the low-life warning pulse.
 */
float statusAlarmCounter;

using StatusColorRgba = std::array<u8, 4>;

StatusColorRgba statusRGBColor_life;     /**< Life bar above a fifth full. */
StatusColorRgba statusRGBColor_weapon;   /**< Weapon bar above a fifth full. */
StatusColorRgba statusRGBColor_30;       /**< Bar above 15% and at most a fifth full. */
StatusColorRgba statusRGBColor_15;       /**< Bar at most 15% full; its alpha pulses. */
StatusColorRgba statusRGBColor_life_2;   /**< Second colour of a life bar above a fifth full. */
StatusColorRgba statusRGBColor_weapon_2; /**< Second colour of a weapon bar above a fifth full. */
StatusColorRgba statusRGBColor_30_2;     /**< Second colour of a bar above 15% and at most a fifth full. */
StatusColorRgba statusRGBColor_15_2;     /**< Second colour of a bar at most 15% full. */

/**
 * Steps since poison last took its toll.
 */
int poison_counter;

/**
 * Whether the jump cache holds a map.
 */
int BtCfgFlag;

/**
 * Message file number loaded for the cached map jump.
 */
int BtSteebMsgNo;

/**
 * Reports whether the party stands in the water of the nearest map part, records the surface
 * point in CheckWaterInfo and starts the splash as they enter it.
 */
int CheckHealingWater() {
    float        position[4];
    float        water_position[4];
    PARTS_WATER *nearest_water = nullptr;
    float        nearest_distance = 160.0f;
    float        water_x = 0.0f;
    float        water_y = 0.0f;
    float        water_z = 0.0f;

    sceVu0CopyVector(position, CharaMain.pos);

    for (int row = 0; row < 16; row++) {
        for (int column = 0; column < 16; column++) {
            const int parts_no = NowDngMap->cells[(row * 20) + column].parts_no;

            if (parts_no != -1) {
                CDungeonParts *part = &NowDngMap->parts[parts_no];
                PARTS_WATER   *water = &part->water;

                if (part->water.used != 0) {
                    water_position[0] = 160.0f * static_cast<float>(column);
                    water_position[1] = water->vertex[0][1];
                    water_position[2] = 160.0f * static_cast<float>(row);

                    const float distance = DistVector(position, water_position);

                    if (distance < nearest_distance) {
                        nearest_water = water;
                        nearest_distance = distance;
                        water_x = water_position[0];
                        water_y = water_position[1];
                        water_z = water_position[2];
                    }
                }
            }
        }
    }

    if (nearest_distance < 160.0f && position[0] >= water_x + nearest_water->vertex[0][0] && position[0] < water_x + nearest_water->vertex[3][0] && position[2] >= water_z + nearest_water->vertex[0][2] && position[2] < water_z + nearest_water->vertex[3][2] && position[1] < water_y) {
        position[1] = water_y;
        sceVu0CopyVector(CheckWaterInfo.surface_position, position);
        BtActStatus.in_water = true;

        if (Water_Splash_actFlag == 0 && CheckWaterInfo.in_water == 0 && CheckWaterInfo.dry_position[1] - CheckWaterInfo.surface_position[1] > 0.5f) {
            Water_Splash.SetPosition(CheckWaterInfo.surface_position);
            Water_Splash.SetMotion(0, 6);
            SndSePlay(SE_WATER_SPLASH, -1, 0);
            Water_Splash_actFlag = 1;
        }

        CheckWaterInfo.in_water = true;
        return 1;
    }

    sceVu0CopyVector(CheckWaterInfo.dry_position, position);
    CheckWaterInfo.in_water = false;
    return 0;
}

/**
 * Reports whether the party stands in the healing zone of the map part under them.
 */
int CheckHealZone() {
    float position[4];
    float center[4];
    float low[4];
    float high[4];

    sceVu0CopyVector(position, CharaMain.pos);
    BtActStatus.in_water = false;

    const int column = static_cast<int>((80.0f + position[0]) / 160.0f);
    const int row = static_cast<int>((80.0f + position[2]) / 160.0f);
    const int parts_no = NowDngMap->cells[(row * 20) + column].parts_no;

    if (parts_no == MAP_PARTS_NONE) {
        return 0;
    }

    CDungeonParts *part = &NowDngMap->parts[parts_no];

    if (part->heal_on == 0) {
        return 0;
    }

    sceVu0CopyVector(center, part->heal_pos);
    center[0] += 160.0f * static_cast<float>(column);
    center[2] += 160.0f * static_cast<float>(row);
    sceVu0CopyVector(low, center);
    sceVu0CopyVector(high, center);
    low[0] -= part->heal_width / 2.0f;
    low[2] -= part->heal_depth / 2.0f;
    high[0] += part->heal_width / 2.0f;
    high[2] += part->heal_depth / 2.0f;

    if (position[0] >= low[0] && position[0] <= high[0] && position[2] >= low[2] && position[2] < high[2] && position[1] < center[1]) {
        BtActStatus.in_water = true;
        return 1;
    }

    return 0;
}

} // namespace

void WaterSplash_Init() {
    Water_Splash_actFlag = 0;
    CheckWaterInfo.in_water = false;

    for (WATER_WAVE_LING &ling : WaterWaveLing) {
        ling.life = 0;
    }

    WaterWaveLingWait = 0;
}

void HealingWater() {
    if (CheckHealZone() || CheckHealingWater()) {
        if (healingSpeed <= 0) {
            const int chara = UserStatus->cur_chara;
            UserStatus->AddDrink(chara, 0xFF, 255.0f);
            UserStatus->AddNowLife(chara, 0xFF, 255.0f);
            healingSpeed = 10;
        } else {
            healingSpeed--;
        }

        if (HealEffect.active == 0) {
            const int max_hp = UserStatus->max_hp[UserStatus->cur_chara];
            const int now_hp = UserStatus->hp[UserStatus->cur_chara];

            if (now_hp != max_hp) {
                float position[4];
                sceVu0CopyVector(position, CharaMain.pos);
                HealEffect.Set(position);
                SndSePlay(SE_CHARA_RECOVER, -1, 0);
                healingSpeed_flg = 1;
            }
        } else if (healingSpeed_flg) {
            setUnitAmbientAnime(60.0f, 1.0f, 0.0f, 122.0f, 208.0f);
            healingSpeed_flg = 0;
        }
    }
}

void DrawWaterLing() {
    float      corner[4][4];
    int        screen[4][4];
    sceGsAlpha alpha;
    sceGsZbuf  zbuf;

    CTexture *ripple = TexManager.GetTexture("d00e01", -1);

    alpha = mgAlpha;
    alpha.bits.a = 0;
    alpha.bits.b = 2;
    alpha.bits.c = 0;
    alpha.bits.d = 1;
    MGSetGsALPHA(&alpha);

    zbuf = mgZBuffer;
    zbuf.bits.zmsk = 1;
    MGSetGsZBUF(&zbuf);

    for (const WATER_WAVE_LING &ling : WaterWaveLing) {
        if (ling.life > 0) {
            const float radius = ling.radius;
            const float x = ling.center[0];
            const float left = x - radius;
            corner[0][0] = left;
            const float y = ling.center[1];
            corner[0][1] = y;
            const float z = ling.center[2];
            const float near = z - radius;
            corner[0][2] = near;
            const float right = radius + x;
            corner[1][0] = right;
            corner[1][1] = y;
            corner[1][2] = near;
            corner[2][0] = left;
            corner[2][1] = y;
            const float far = radius + z;
            corner[2][2] = far;
            corner[3][0] = right;
            corner[3][1] = y;
            corner[3][2] = far;

            bool visible = true;

            for (int corner_no = 0; corner_no < 4; corner_no++) {
                corner[corner_no][3] = 1.0f;

                if (MGRotTransPers(screen[corner_no], corner[corner_no], 0) == 0) {
                    visible = false;
                }
            }

            if (visible) {
                const float fade = 2.8444445f * static_cast<float>(ling.life);
                set3DSprite(Vif1Packet, ripple, CRect_i_(0, 64, 64, 64), screen[0], screen[1], screen[2], screen[3], static_cast<unsigned char>(fade));
            }
        }
    }

    MGSetGsALPHA(nullptr);
    MGSetGsZBUF(nullptr);
}

void StepWaterLing() {
    if (CheckWaterInfo.in_water != 0) {
        WaterWaveLingWait++;

        if (WaterWaveLingWait >= 20) {
            WaterWaveLingWait = 0;

            for (WATER_WAVE_LING &ling : WaterWaveLing) {
                if (ling.life == 0) {
                    sceVu0CopyVector(ling.center, CheckWaterInfo.surface_position);
                    ling.radius = 2.0f;
                    ling.life = 45;
                    break;
                }
            }
        }
    }

    for (WATER_WAVE_LING &ling : WaterWaveLing) {
        if (ling.life > 0) {
            ling.radius += 0.2f;
            ling.life--;
        }
    }
}

float SetBattleStyle(int map_no, int preserve_bgm) {
    float         nearest = 10000.0f;
    sceVu0FVECTOR position;
    sceVu0FVECTOR player;
    float         bgm_volume = 0.0f;
    float         ambient_volume = 0.0f;

    sceVu0CopyVector(player, CharaMain.pos);

    for (int monster_no = 0; monster_no < 16; monster_no++) {
        if (NowMonstorUnit->monster[monster_no].state == -1) {
            continue;
        }

        if (NowMonstorUnit->monster[monster_no].revealed == 0) {
            continue;
        }

        NowMonstorUnit->chara[monster_no][0].GetPosition(position);
        nearest = std::min(nearest, DistVector(player, position));
    }

    if (map_no != 5) {
        const int floors = maxFloorTbl[map_no];

        if (UserStatus->cur_floor < floors - 1) {
            BtBattleMusic_Excg(nearest, &bgm_volume, &ambient_volume);

            if (ambient_volume > 0.0f) {
                SndAmbientPlay(0);
            }

            if (preserve_bgm == 0) {
                SndSetBgmVolf(bgm_volume);
            }

            SndAmbientSetVolf(ambient_volume);
        }
    }

    return nearest;
}

int ValuePrint(int x, int y, int value, int palette, unsigned char alpha) {
    CTexture *texture = TexManager.GetTexture("stayframe", -1);
    const int source_y = (palette * 12) + 0xB0;
    int       hundreds_shown = 0;
    int       digit = value / 100;

    if (digit > 0) {
        set2DSprite(Vif1Packet, texture, CRect_i_(x, y, 12, 12), CRect_i_(digit * 12, source_y, 12, 12), alpha);
        value -= digit * 100;
        x += 12;
        hundreds_shown++;
    }

    digit = value / 10;
    set2DSprite(Vif1Packet, texture, CRect_i_(x, y, 12, 12), CRect_i_(digit * 12, source_y, 12, 12), alpha);
    digit = value % 10;
    set2DSprite(Vif1Packet, texture, CRect_i_(x + 12, y, 12, 12), CRect_i_(digit * 12, source_y, 12, 12), alpha);
    return hundreds_shown + 2;
}

void BtStatusAlarmInit() {
    statusAlarmRate = 128;
    statusAlarmCounter = 0.0f;

    statusRGBColor_life = {0x40, 0xC1, 0x82, 0x80};
    statusRGBColor_weapon = {0xFF, 0x80, 0x00, 0x80};
    statusRGBColor_30 = {0xFF, 0x00, 0xC6, 0x80};
    statusRGBColor_15 = {0xFF, 0x00, 0x00, 0x80};
    statusRGBColor_life_2 = {0x00, 0x81, 0xFF, 0x80};
    statusRGBColor_weapon_2 = {0xFF, 0x20, 0x00, 0x80};
    statusRGBColor_30_2 = {0x3F, 0x00, 0x31, 0x80};
    statusRGBColor_15_2 = {0x3F, 0x00, 0x00, 0x80};
}

void BtStatusAlarmAnime() {
    statusAlarmCounter += 0.10471976f;

    if (!(statusAlarmCounter < std::numbers::pi_v<float>) ) {
        statusAlarmCounter -= std::numbers::pi_v<float>;
    }

    statusAlarmRate = 192 - static_cast<int>(2.0f * (64.0f * std::sin(statusAlarmCounter)));
    statusRGBColor_15[3] = static_cast<u8>(192 - static_cast<int>(2.0f * (64.0f * std::sin(statusAlarmCounter))));
}

u8 *BtGetStatusPal(int bar, float max, float value) {
    if (!(value <= 0.2f * max)) {
        return bar == 0 ? statusRGBColor_life.data() : statusRGBColor_weapon.data();
    }

    if (value <= 0.3f * max && !(value <= 0.15f * max)) {
        return statusRGBColor_30.data();
    }

    return statusRGBColor_15.data();
}

u8 *BtGetStatusPal2(int bar, float max, float value) {
    if (!(value <= 0.2f * max)) {
        return bar == 0 ? statusRGBColor_life_2.data() : statusRGBColor_weapon_2.data();
    }

    if (value <= 0.3f * max && !(value <= 0.15f * max)) {
        return statusRGBColor_30_2.data();
    }

    return statusRGBColor_15_2.data();
}

namespace {

// Fills a status bar's trough: the frame is `cells` four-pixel columns from x 0x30, and the trough
// runs from the left cap's fourth column to the right cap's second on the frame's rows 4 to 10.
void StatusBarFill(int frame_y, int cells, float value, float max, spRGBA *left, spRGBA *right) {
    const int trough = cells * 4 - 5;
    const int length = std::clamp(static_cast<int>(static_cast<float>(trough) * value / max), 0, trough);

    set2DSpriteC4(Vif1Packet, CRect_i_(0x33, frame_y + 4, length, 7), left, right, left, right);
}

} // namespace

void topStatusInfo(int y, int selected_item, int floor) {
    const int chara = UserStatus->cur_chara;
    if (chara < 0 || chara >= 6) {
        return;
    }

    const int weapon_slot = UserStatus->equipped_weapon_slot[chara];
    if (weapon_slot < 0 || weapon_slot >= 11) {
        return;
    }

    const int georama = UserStatus->cur_georama;
    if (georama < 0 || georama >= 7) {
        return;
    }

    y -= 10;
    y += BtActStatus.hud_shake_y;
    CTexture *icons = TexManager.GetTexture(const_cast<char *>("itempack"), -1);
    CTexture *frame = TexManager.GetTexture(const_cast<char *>("stayframe"), -1);

    float life = UserStatus->hp[chara];
    float life_max = UserStatus->max_hp[chara];
    int   length = static_cast<int>(0.74f * life_max);

    length = std::max(length, 2);

    int cells = (length >> 2) + 1;

    cells = std::max(cells, 2);

    int alpha = 0x80;

    if (life <= 0.2f * life_max) {
        alpha = statusAlarmRate;
    }

    auto *upper = reinterpret_cast<spRGBA *>(BtGetStatusPal(0, life_max, life));
    auto *lower = reinterpret_cast<spRGBA *>(BtGetStatusPal2(0, life_max, life));
    set2DSprite(Vif1Packet, icons, CRect_i_(0x30, y, 4, 0x10), CRect_i_(0x68, 0x70, 4, 0x10), alpha);

    for (int i = 0; i < cells - 2; i++) {
        set2DSprite(Vif1Packet, icons, CRect_i_(i * 4 + 0x34, y, 4, 0x10), CRect_i_(0x6C, 0x70, 4, 0x10), alpha);
    }

    set2DSprite(Vif1Packet, icons, CRect_i_((cells - 1) * 4 + 0x30, y, 4, 0x10), CRect_i_(0x70, 0x70, 4, 0x10), alpha);

    if (life > 0.0f) {
        StatusBarFill(y, cells, life, life_max, upper, lower);
    }

    cells = (cells - 1) * 4 + 0x38;
    cells += ValuePrint(cells, y + 7, static_cast<int>(life), 0, alpha) * 10;
    set2DSprite(Vif1Packet, frame, CRect_i_(cells, y + 7, 0xC, 0xC), CRect_i_(0x78, 0xB0, 0xC, 0xC), alpha);
    ValuePrint(cells + 10, y + 7, static_cast<int>(life_max), 0, alpha);
    set2DSprite(Vif1Packet, icons, CRect_i_(0x20, y, 0x10, 0x10), CRect_i_(0x40, 0xA0, 0x10, 0x10), alpha);

    WEAPON_HAVE *weapon = &UserStatus->chara_weapons[chara][weapon_slot];
    float        durability_max = weapon->durability;
    float        durability = weapon->durability_f;
    float        bar = 1.4949495f * durability_max;
    length = static_cast<int>(bar);

    length = std::max(length, 2);

    cells = (length >> 2) + 2;

    cells = std::max(cells, 2);

    alpha = 0x80;

    if (durability <= 0.2f * durability_max) {
        alpha = statusAlarmRate;
    }

    upper = reinterpret_cast<spRGBA *>(BtGetStatusPal(1, durability_max, durability));
    lower = reinterpret_cast<spRGBA *>(BtGetStatusPal2(1, durability_max, durability));
    set2DSprite(Vif1Packet, icons, CRect_i_(0x30, y + 0x11, 4, 0x10), CRect_i_(0x68, 0x70, 4, 0x10), alpha);

    for (int i = 0; i < cells - 2; i++) {
        set2DSprite(Vif1Packet, icons, CRect_i_(i * 4 + 0x34, y + 0x11, 4, 0x10), CRect_i_(0x6C, 0x70, 4, 0x10), alpha);
    }

    set2DSprite(Vif1Packet, icons, CRect_i_((cells - 1) * 4 + 0x30, y + 0x11, 4, 0x10), CRect_i_(0x70, 0x70, 4, 0x10), alpha);

    if (durability > 0.0f) {
        StatusBarFill(y + 0x11, cells, durability, durability_max, upper, lower);
    }

    int exp_max = GetWeaponMaxExp(weapon);
    int width = static_cast<int>(bar);
    MGFillBox(CRect_i_(0x340, ((y + 0x1F) >> 1) * 16, (width - 1) * 16, 0x10), 0x40, 0x40, 0x40, alpha);

    if (weapon->experience > 0) {
        int filled = static_cast<int>(static_cast<float>(weapon->experience) * (static_cast<float>(width) / static_cast<float>(exp_max)));
        MGFillBox(CRect_i_(0x340, ((y + 0x1F) >> 1) * 16, (filled - 1) * 16, 0x10), 0, 0xA2, 0xFF, alpha);
    }

    cells = (cells - 1) * 4 + 0x38;
    int shown = static_cast<int>(durability);

    if (durability - static_cast<float>(shown) > 0.0f) {
        shown++;
    }

    cells += ValuePrint(cells, y + 0x18, shown, 0, alpha) * 10;
    set2DSprite(Vif1Packet, frame, CRect_i_(cells, y + 0x18, 0xC, 0xC), CRect_i_(0x78, 0xB0, 0xC, 0xC), alpha);
    ValuePrint(cells + 10, y + 0x18, static_cast<int>(durability_max), 0, alpha);
    set2DSprite(Vif1Packet, icons, CRect_i_(0x20, y + 0x10, 0x10, 0x10), CRect_i_(0x40, 0xB0, 0x10, 0x10), alpha);

    float water_max = UserStatus->water_max[chara];
    float water = UserStatus->water_now[chara];
    alpha = 0x80;

    if (water < 0.15f * water_max) {
        alpha = statusAlarmRate;
    }

    set2DSprite(Vif1Packet, icons, CRect_i_(0x20, y + 0x22, 0x12, 0x14), CRect_i_(0x64, 0x84, 0x12, 0x14), alpha);
    int drops = static_cast<int>(water_max) / 10;

    for (cells = 0; cells < drops; cells++) {
        set2DSprite(Vif1Packet, icons, CRect_i_(cells * 0x12 + 0x32, y + 0x22, 0x12, 0x14), CRect_i_(0x64, 0x98, 0x12, 0x14), alpha);
    }

    if (static_cast<int>(water_max) % 10 != 0) {
        set2DSprite(Vif1Packet, icons, CRect_i_(drops * 0x12 + 0x32, y + 0x22, 0x12, 0x14), CRect_i_(0x64, 0xAC, 0x18, 0x14), alpha);
    }

    int full_drops = static_cast<int>(water) / 10;

    for (cells = 0; cells < full_drops; cells++) {
        set2DSprite(Vif1Packet, icons, CRect_i_(cells * 0x12 + 0x33, y + 0x22, 0x10, 0x14), CRect_i_(0, 0x48, 0x10, 0x14), 0x50);
    }

    int leftover = static_cast<int>(water) % 10;

    if (leftover != 0) {
        set2DSprite(Vif1Packet, icons, CRect_i_(full_drops * 0x12 + 0x33, y + 0x22, 0x10, 0x14), CRect_i_((3 - static_cast<int>(static_cast<float>(leftover) / 2.5f)) * 16, 0x48, 0x10, 0x14), 0x50);
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
            glow = static_cast<int>(64.0f * std::sin(popupRGBRate)) + 0x3F;
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

    int last_floor = maxFloorTbl__3[georama];

    if (floor + 1 == last_floor) {
        set2DSprite(Vif1Packet, frame, CRect_i_(0x23C, y + 0x12, 0x26, 0x11), CRect_i_(0x78, 0x9E, 0x26, 0x12));
    } else {
        const int x = floor + 1 >= 10 ? 0x245 : 0x23F;

        int number_y = y + 0x12;

        if (floor + 1 >= 10) {
            set2DSprite(Vif1Packet, frame, CRect_i_(x - 4, number_y, 0xE, 0x11), CRect_i_((floor + 1) / 10 * 12, 0x9E, 0xC, 0x12));
        }

        set2DSprite(Vif1Packet, frame, CRect_i_(x + 9, number_y, 0xE, 0x11), CRect_i_((floor + 1) % 10 * 12, 0x9E, 0xC, 0x12));
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
    int u = 0;
    int v = 0;
    int item = UserStatus->chara_weapons[chara][weapon_slot].item_no;

    if (item == defWeapon__2[chara] + 1) {
        v = 0x20;
    }

    if (item == defWeapon__2[chara]) {
        u = 0x20;
        v = 0x20;
    }

    set2DSprite(Vif1Packet, icons, CRect_i_(0x1D, 0x1A4, 0x20, 0x20), CRect_i_(u, v, 0x20, 0x20), alpha);
}

int StatusErrCheck(int status) {
    return (UserStatus->ailments[UserStatus->cur_chara] & status) ? 1 : 0;
}

int BtStatusErrColorSet() {
    int       ailing = 0;
    const int status = UserStatus->ailments[UserStatus->cur_chara];

    if (status & AILMENT_FREEZE) {
        StatusColor[0] = 127.5f;
        StatusColor[1] = 127.5f;
        StatusColor[2] = 127.5f;
        ailing = 1;
    }

    if (status & AILMENT_GOO) {
        StatusColor[0] = 50.0f;
        StatusColor[1] = 75.0f;
        StatusColor[2] = 127.5f;
        ailing = 1;
    }

    if (status & AILMENT_POISON) {
        StatusColor[0] = 47.0f;
        StatusColor[1] = 0.5f;
        StatusColor[2] = 63.75f;
        ailing = 1;
    }

    if (status & AILMENT_STAMINA) {
        StatusColor[0] = 127.5f;
        StatusColor[1] = 80.0f;
        StatusColor[2] = 15.0f;
        ailing = 1;
    }

    return ailing;
}

void BtStatusErrStep() {
    CUserStatus *status = UserStatus;
    const int    flags = status->ailments[status->cur_chara];

    poison_counter++;

    // Poison takes 4% of the character's max HP every 180 steps and shows the loss over them.
    if ((flags & AILMENT_POISON) && poison_counter >= 180) {
        const int chara = status->cur_chara;
        const int max_hp = status->max_hp[chara];
        const int damage = static_cast<int>(0.04 * max_hp);
        status->AddNowLife(chara, static_cast<s16>(-damage), 10.0f);

        float position[4] = {0.0f, 0.0f, 0.0f, 1.0f};
        position[1] = CharaHeight(UserStatus);
        HitValueEntry(NowHitValue, position, damage, HIT_VALUE_PLAYER, CharaMain.frame);
    }

    if (poison_counter >= 180) {
        poison_counter = 0;
    }

    BtStatusErrColorSet();
}

void BtSetStatusErr(int status) {
    const int chara = UserStatus->cur_chara;

    switch (status) {
        case AILMENT_FREEZE:
            UserStatus->ailments[chara] |= status;
            UserStatus->ailments[chara] &= ~(AILMENT_STAMINA | AILMENT_POISON | AILMENT_GOO);
            UserStatus->ailment_frames[chara] = 300;
            SndSePlay(SE_STATUS_AILMENT, -1, 0);
            break;

        case AILMENT_STAMINA:
            if (!(UserStatus->ailments[chara] & AILMENT_FREEZE)) {
                UserStatus->ailments[chara] |= status;
                UserStatus->ailment_frames[chara] = 1800;
            }

            break;

        case AILMENT_POISON:
            if (!(UserStatus->ailments[chara] & (AILMENT_FREEZE | AILMENT_STAMINA))) {
                UserStatus->ailments[chara] |= status;
                UserStatus->ailments[chara] &= ~AILMENT_GOO;
                SndSePlay(SE_STATUS_AILMENT, -1, 0);
            }

            break;

        case AILMENT_CURSE:
            UserStatus->ailments[chara] |= status;
            SndSePlay(SE_STATUS_AILMENT, -1, 0);
            break;

        case AILMENT_GOO:
            if (!(UserStatus->ailments[chara] & (AILMENT_FREEZE | AILMENT_STAMINA | AILMENT_POISON))) {
                UserStatus->ailments[chara] |= status;
                SndSePlay(SE_STATUS_AILMENT, -1, 0);
            }

            break;

        default:
            break;
    }
}

void BtStatusErrDraw(int y) {
    CTexture                                   *texture = TexManager.GetTexture("itempack", -1);
    constexpr std::array<int, 5>                status_flags = {AILMENT_FREEZE, AILMENT_STAMINA, AILMENT_POISON, AILMENT_CURSE, AILMENT_GOO};
    constexpr std::array<std::array<int, 2>, 5> icon_cells = {
        {
         {1, 0},
         {0, 1},
         {1, 1},
         {1, 2},
         {0, 2},
         }
    };
    const int status = UserStatus->ailments[UserStatus->cur_chara];

    for (int icon = 4; icon >= 0; icon--) {
        if (status & status_flags[icon]) {
            set2DSprite(Vif1Packet, texture, CRect_i_(430, y - 10, 62, 35), CRect_i_((icon_cells[icon][0] * 62) + 132, (icon_cells[icon][1] * 36) + 84, 62, 36));
        }
    }
}

void setItemToReserved(char *page_name, int x, int y, char *item_name, int dsax, int dsay) {
    CTexture *page = TexManager.GetTexture(page_name, -1);
    CTexture *item = TexManager.GetTexture(item_name, -1);

    int sbp = page->tex0 & 0x3FFF;
    int dbp = item->tex0 & 0x3FFF;
    int sbw = (page->tex0 >> 14) & 0x3F;
    int dbw = (item->tex0 >> 14) & 0x3F;

    MoveImageTest(Vif1Packet, sbp, sbw, 0x13, CRect_i_(x, y, 0x20, 0x20), dbp, dbw, 0x13, dsax, dsay, 0);
}

void BtMapJumpCashClear() {
    BtCfgCash.clear();
    BtCfgFlag = 0;
    BtSteebMsgNo = -1;
}

int BtMapJumpLoad(char *map_name) {
    int mes_size = 0;

    CUserStatus *status = UserStatus;
    int          msg_no = status->cur_georama;
    const int    floor = status->cur_floor;

    // From dungeon 6 on, the message file also changes every twenty floors.
    if (msg_no >= 6) {
        if (floor >= 20) {
            msg_no++;
        }

        if (floor >= 40) {
            msg_no++;
        }

        if (floor >= 60) {
            msg_no++;
        }

        if (floor >= 80) {
            msg_no++;
        }
    }

    if (msg_no != BtSteebMsgNo) {
        BtSteebMesBuffer.used = 0;
        u_char *mes = &BtSteebMesBuffer.base[static_cast<std::ptrdiff_t>(BtSteebMesBuffer.used) * 16];

        std::string mes_path = status->cur_georama == 6 ? std::format("dun/message/ww_mes/steve07_{}_{}.mes", msg_no - 5, LanguageCode)
                                                        : std::format("dun/message/ww_mes/steve0{}_{}.mes", msg_no + 1, LanguageCode);

        LoadFile(mes_path.data(), mes, &mes_size);
        wait_now_loading_vsync();
        DngMesStb.SetBuff(reinterpret_cast<short *>(mes));
        BtSteebMesBuffer.Alloc((((mes_size >> 6) + 1) << 6) >> 4);
        BtSteebMesBuffer.Align64();
        BtSteebMsgNo = msg_no;
    }

    TexManager.DeleteTextureBlock(42);
    MainMonstorUnit.CleanViewMonstor(0);
    NowDngMap = &MainDungeonMap;
    NowEventMan = &DngEventMan;

    if (BtCfgCash == map_name && BtCfgFlag != 0) {
        return 0;
    }

    BtCfgCash = map_name;

    MapModelBuffer.used = 0;
    u_int *pack = read_buffer;
    read_buffer = old_read_buffer;

    // NowDranMapField points at DranMapField.field, the start of the whole set.
    auto *field_set = reinterpret_cast<DRAN_MAP_FIELD_SET *>(NowDranMapField);

    for (int field_no = 0; field_no < 12; field_no++) {
        field_set->field[field_no].Initialize();
        field_set->collision[field_no] = nullptr;
        field_set->state[field_no] = 3;
    }

    field_set->field_count = 0;
    field_set->collision_count = 0;

    MainDungeonMap.initSubmap(&MapModelBuffer);

    for (CDungeonEvent &event : DngEventMan.slot) {
        event.name[0] = '\0';
        event.script_no = -1;
        event.parts_id = -1;
        event.switch_on = false;
        event.enabled = false;
    }

    for (CDungeonEventData &data : DngEventMan.event) {
        data.event = nullptr;
        data.switch_on = false;
        data.enabled = false;
        data.hold = 0;
        data.chara_done = -1;
    }

    std::string mpd_path = std::format("dun/mpd_pack/{}.mpd", map_name);
    std::string cfg_path = std::format("dun/{}.cfg", map_name);
    TEIGIAnalyz(cfg_path.data());

    int size = 0;
    LoadFile(mpd_path.data(), read_buffer, &size);
    wait_now_loading_vsync();

    if (size >= 6400000) {
        std::println("MAPDATA BUFFER OVER!! {}", size);
        exit__2(1);
    }

    TEIGIImgLoad(read_buffer, &MapModelBuffer);
    TEIGIMdsLoad(read_buffer, 0);

    CFrameAttr attr;
    attr.fog_enable = true;
    attr.clip_depth = 100.0f;
    attr.clip_enable = false;
    attr.program_option = 0;
    NowDngMap->box_body_model = LoadMDSFilePack(read_buffer, "ibox_0.mds", &MapModelBuffer);
    NowDngMap->box_lid_model = LoadMDSFilePack(read_buffer, "ibox_t.mds", &MapModelBuffer);
    NowDngMap->box_collision_model = LoadCollisionFilePack(read_buffer, "ibox_a.mds", &MapModelBuffer);
    NowDngMap->box_body_model->SetAttr(attr, 1, 0);
    NowDngMap->box_lid_model->SetAttr(attr, 1, 0);
    NowDngMap->chest_body_model = LoadMDSFilePack(read_buffer, "iboxs_0.mds", &MapModelBuffer);
    NowDngMap->chest_lid_model = LoadMDSFilePack(read_buffer, "iboxs_t.mds", &MapModelBuffer);
    NowDngMap->chest_collision_model = LoadCollisionFilePack(read_buffer, "iboxs_a.mds", &MapModelBuffer);
    NowDngMap->chest_body_model->SetAttr(attr, 1, 0);
    NowDngMap->chest_lid_model->SetAttr(attr, 1, 0);

    itemOpenSmall.LoadPack(read_buffer, "dun/etc/itemget_s/d01i02m", &MapModelBuffer, &MapModelBuffer, &itemOpenSmall_info, 0);
    itemOpenBig.LoadPack(read_buffer, "dun/etc/itemget_b/d01i01m", &MapModelBuffer, &MapModelBuffer, &itemOpenBig_info, 0);

    // The monsters get whatever the map left of its buffer.
    const int map_used = MapModelBuffer.used;
    MonstorModelBuffer.base = &MapModelBuffer.base[static_cast<std::ptrdiff_t>(map_used) * 16];
    MonstorModelBuffer.limit = 0xA7F80 - map_used;
    MonstorModelBuffer.used = 0;
    read_buffer = pack;

    // Only the low 32 bits of each arena address are shown.
    const auto low_address = [](CDataAlloc2<1> &arena) { return static_cast<u32>(reinterpret_cast<std::uintptr_t>(&arena.base[static_cast<std::ptrdiff_t>(arena.used) * 16])); };
    std::println("TotalMem = {}", 0xA7F80);
    std::println("MapVisualData   [{:x}]  {}/{}", low_address(MapModelBuffer), MapModelBuffer.used, MapModelBuffer.limit);
    std::println("MonstorData     [{:x}]  {}/{}", low_address(MonstorModelBuffer), MonstorModelBuffer.used, MonstorModelBuffer.limit);
    BtCfgFlag = 1;
    return 1;
}

void BtSet3DCellModel(float *world, CTexture *texture, float size, int x, int y, int width, int height, int alpha) {
    int top_left[4];
    int top_right[4];
    int bottom_left[4];
    int bottom_right[4];

    world[3] = 1.0f;

    if (MGRotTransPers3DSprite(top_left, bottom_right, world, size, size / 2.0f, 0) == 1) {
        top_right[0] = bottom_right[0];
        top_right[1] = top_left[1];
        top_right[2] = top_left[2];
        bottom_left[0] = top_left[0];
        bottom_left[1] = bottom_right[1];
        bottom_left[2] = bottom_right[2];

        set3DSprite(Vif1Packet, texture, CRect_i_(x, y, width, height), top_left, top_right, bottom_left, bottom_right, static_cast<unsigned char>(alpha));
    }
}

} // namespace dc::game
