#include "shot_freefuncs.hpp"

#include "itemdata.hpp"

// These tables precede the template of CharaHeight's inline table in .data, so they stand
// above the header that defines it.

/**
 * Number of floors in each dungeon.
 */
static int maxFloorTbl[7] = {15, 17, 18, 18, 15, 25, 100};

/**
 * Each character's default weapon.
 */
static int defWeapon[6] = {ITEM_WEAPON_DAGGER_BROKEN, ITEM_WEAPON_WOODENSLINGSHOT_BROKEN, ITEM_WEAPON_MALLET_BROKEN, ITEM_WEAPON_GOLD_RING_BROKEN, ITEM_WEAPON_FIGHTING_STICK_BROKEN, ITEM_WEAPON_MACHINE_GUN_BROKEN};

#include <libvu0.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "btactstatus.hpp"
#include "btmisc.hpp"
#include "character.hpp"
#include "charaheight.hpp"
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

s32   DebugStatus[21];
float StatusColor[3];

/**
 * The water the party is standing in and the nearest water surface.
 */
CHECK_WATER_INFO CheckWaterInfo;

/**
 * The rings spreading across the water.
 */
WATER_WAVE_LING WaterWaveLing[6];

/**
 * The name of the map held in the jump cache.
 */
char BtCfgCash[64];

/**
 * The frames remaining before the next ring appears.
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
 * The period of the low-life warning pulse.
 */
int statusAlarmRate;

/**
 * The phase of the low-life warning pulse.
 */
float statusAlarmCounter;

/**
 * The colour of a full life bar.
 */
u8 statusRGBColor_life[4];

/**
 * The colour of a full weapon bar.
 */
u8 statusRGBColor_weapon[4];

/**
 * The colour of a bar below three tenths.
 */
u8 statusRGBColor_30[4];

/**
 * The warning colour of a bar below a seventh.
 */
u8 statusRGBColor_15[4];

/**
 * The second colour of a full life bar.
 */
u8 statusRGBColor_life_2[4];

/**
 * The second colour of a full weapon bar.
 */
u8 statusRGBColor_weapon_2[4];

/**
 * The second colour of a bar below three tenths.
 */
u8 statusRGBColor_30_2[4];

/**
 * The second warning colour of a bar below a seventh.
 */
u8 statusRGBColor_15_2[4];

/**
 * Clears the water-splash effects.
 *
 * @mangled WaterSplash_Init__Fv
 * @address 0x1AF360
 * @size 0x48
 */
void WaterSplash_Init() {
    int i;

    Water_Splash_actFlag = 0;
    CheckWaterInfo.in_water = false;

    for (i = 0; i < 6; i++) {
        WaterWaveLing[i].life = 0;
    }

    WaterWaveLingWait = 0;
}

/**
 * Reports whether the party stands in the water of the nearest map part, records the surface
 * point in CheckWaterInfo and starts the splash as they enter it.
 *
 * @mangled CheckHealingWater__Fv
 * @address 0x1AF3B0
 * @size 0x328
 */
int CheckHealingWater() {
    float        position[4];
    float        water_position[4];
    PARTS_WATER *nearest_water;
    float        nearest_distance;
    float        water_x;
    float        water_y;
    float        water_z;
    int          row;
    int          column;

    nearest_distance = 160.0f;
    sceVu0CopyVector(position, CharaMain.pos);

    for (row = 0; row < 16; row++) {
        for (column = 0; column < 16; column++) {
            int parts_no = NowDngMap->cells[row * 20 + column].parts_no;

            if (parts_no != -1) {
                CDungeonParts *part = &NowDngMap->parts[parts_no];
                PARTS_WATER   *water = &part->water;

                if (part->water.used != 0) {
                    water_position[0] = 160.0f * column;
                    water_position[1] = water->vertex[0][1];
                    water_position[2] = 160.0f * row;

                    float distance = DistVector(position, water_position);

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
 * Holds the position and extent of a dungeon part's healing zone.
 */
struct HEAL_ZONE {
    float pos[4]; /**< Centre of the healing zone. */
    float width;  /**< Width of the healing zone. */
    float depth;  /**< Depth of the healing zone. */
    s32   on;     /**< Whether the healing zone is active. */
};

/**
 * Reports whether the party stands in a healing zone.
 *
 * @mangled CheckHealZone__Fv
 * @address 0x1AF6E0
 * @size 0x29C
 */
int CheckHealZone() {
    float          position[4];
    float          center[4];
    float          low[4];
    float          high[4];
    int            column;
    int            row;
    int            parts_no;
    int            hit;
    CDungeonParts *part;
    HEAL_ZONE     *zone;

    sceVu0CopyVector(position, CharaMain.pos);
    BtActStatus.in_water = false;

    column = (80.0f + position[0]) / 160.0f;
    row = (80.0f + position[2]) / 160.0f;
    parts_no = NowDngMap->cells[row * 20 + column].parts_no;

    if (parts_no == MAP_PARTS_NONE) {
        return 0;
    }

    part = &NowDngMap->parts[parts_no];
    zone = (HEAL_ZONE *) part->heal_pos;
    hit = 0;

    if (part->heal_on == 0) {
        return 0;
    }

    sceVu0CopyVector(center, zone->pos);
    center[0] += 160.0f * column;
    center[2] += 160.0f * row;
    sceVu0CopyVector(low, center);
    sceVu0CopyVector(high, center);
    low[0] -= zone->width / 2.0f;
    low[2] -= zone->depth / 2.0f;
    high[0] += zone->width / 2.0f;
    high[2] += zone->depth / 2.0f;

    if (position[0] >= low[0] && position[0] <= high[0] && position[2] >= low[2] && position[2] < high[2] && position[1] < center[1]) {
        BtActStatus.in_water = true;
        return 1;
    }

    if (zone == NULL) {
        return 0;
    }

    return zone->on ? hit : 0;
}

/**
 * Restores the party while they stand in healing water.
 *
 * @mangled HealingWater__Fv
 * @address 0x1AF980
 * @size 0x158
 */
void HealingWater() {
    float position[4];
    int   chara;
    int   max_hp;
    int   now_hp;

    if (CheckHealZone() || CheckHealingWater()) {
        if (healingSpeed <= 0) {
            chara = UserStatus->cur_chara;
            UserStatus->AddDrink(chara, 0xFF, 255.0f);
            UserStatus->AddNowLife(chara, 0xFF, 255.0f);
            healingSpeed = 10;
        } else {
            healingSpeed--;
        }

        if (HealEffect.active == 0) {
            max_hp = UserStatus->max_hp[UserStatus->cur_chara];
            now_hp = UserStatus->hp[UserStatus->cur_chara];

            if (!(now_hp == max_hp)) {
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

/**
 * Draws the rings spreading on the water.
 *
 * @mangled DrawWaterLing__Fv
 * @address 0x1AFAE0
 * @size 0x27C
 */
void DrawWaterLing() {
    float      corner[4][4];
    int        screen[4][4];
    sceGsAlpha alpha;
    sceGsZbuf  zbuf;
    CTexture  *ripple;
    int        i;
    int        visible;
    int        j;
    float      radius;
    float      x;
    float      y;
    float      near;
    float      left;
    float      z;
    float      right;
    float      far;

    ripple = TexManager.GetTexture("d00e01", -1);

    alpha = mgAlpha;
    alpha.bits.a = 0;
    alpha.bits.b = 2;
    alpha.bits.c = 0;
    alpha.bits.d = 1;
    MGSetGsALPHA(&alpha);

    zbuf = mgZBuffer;
    zbuf.bits.zmsk = 1;
    MGSetGsZBUF(&zbuf);

    for (i = 0; i < 6; i++) {
        if (WaterWaveLing[i].life > 0) {
            radius = WaterWaveLing[i].radius;
            x = WaterWaveLing[i].center[0];
            left = x - radius;
            corner[0][0] = left;
            y = WaterWaveLing[i].center[1];
            corner[0][1] = y;
            z = WaterWaveLing[i].center[2];
            near = z - radius;
            corner[0][2] = near;
            right = radius + x;
            corner[1][0] = right;
            corner[1][1] = y;
            corner[1][2] = near;
            corner[2][0] = left;
            corner[2][1] = y;
            far = radius + z;
            corner[2][2] = far;
            corner[3][0] = right;
            corner[3][1] = y;
            corner[3][2] = far;

            visible = true;

            for (j = 0; j < 4; j++) {
                corner[j][3] = 1.0f;

                if (MGRotTransPers(screen[j], corner[j], 0) == 0) {
                    visible = false;
                }
            }

            if (visible) {
                float fade = 2.8444445f * WaterWaveLing[i].life;
                set3DSprite(Vif1Packet, ripple, CRect_i_(0, 64, 64, 64), screen[0], screen[1], screen[2], screen[3], fade);
            }
        }
    }

    MGSetGsALPHA(NULL);
    MGSetGsZBUF(NULL);
}

/**
 * Advances the rings spreading on the water.
 *
 * @mangled StepWaterLing__Fv
 * @address 0x1AFD60
 * @size 0x130
 */
void StepWaterLing() {
    int i;

    if (CheckWaterInfo.in_water != 0) {
        WaterWaveLingWait++;

        if (WaterWaveLingWait >= 20) {
            WaterWaveLingWait = 0;

            for (i = 0; i < 6; i++) {
                if (WaterWaveLing[i].life == 0) {
                    sceVu0CopyVector(WaterWaveLing[i].center, CheckWaterInfo.surface_position);
                    WaterWaveLing[i].radius = 2.0f;
                    WaterWaveLing[i].life = 45;
                    break;
                }
            }
        }
    }

    for (i = 0; i < 6; i++) {
        if (WaterWaveLing[i].life > 0) {
            WaterWaveLing[i].radius += 0.2f;
            WaterWaveLing[i].life--;
        }
    }
}

/**
 * Mixes the battle music against the ambience by the distance to the nearest active monster
 * and returns that distance.
 *
 * @mangled SetBattleStyle__Fii
 * @address 0x1AFE90
 * @size 0x1D0
 */
void BtBattleMusic_Excg(float distance, float *field_volume, float *battle_volume);

static inline int MonstorAliveCheck(int no, int alive) {
    if (no >= 0 && no < 17) {
        alive = NowMonstorUnit->monster[no].revealed;
    }

    return alive;
}

float SetBattleStyle(int map_no, int preserve_bgm) {
    float         nearest = 10000;
    sceVu0FVECTOR position;
    sceVu0FVECTOR player;
    float         bgm_volume;
    float         ambient_volume;
    int           alive;

    sceVu0CopyVector(player, CharaMain.pos);

    for (int i = 0; i < 16; i++) {
        if (NowMonstorUnit->monster[i].state == -1) {
            continue;
        }

        alive = MonstorAliveCheck(i, alive);

        if (alive == 0) {
            continue;
        }

        NowMonstorUnit->chara[i][0].GetPosition(position);
        float distance = DistVector(player, position);

        if (distance < nearest) {
            nearest = distance;
        }
    }

    if (map_no != 5) {
        CUserStatus *status;
        int          floors = maxFloorTbl[map_no];
        status = UserStatus;

        if (status->cur_floor < floors - 1) {
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

/**
 * Draws a three-digit value out of the number sheet.
 *
 * @mangled ValuePrint__FiiiiUc
 * @address 0x1B0060
 * @size 0x1F8
 */
int ValuePrint(int x, int y, int value, int palette, unsigned char alpha) {
    CTexture *texture = TexManager.GetTexture("stayframe", -1);
    int       source_y = palette * 12 + 0xB0;
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

/**
 * Clears the pulse that warns of low life.
 *
 * @mangled BtStatusAlarmInit__Fv
 * @address 0x1B0260
 * @size 0xB8
 */
void BtStatusAlarmInit() {
    statusAlarmRate = 128;
    statusAlarmCounter = 0.0f;

    statusRGBColor_life[0] = 0x40;
    statusRGBColor_life[1] = 0xC1;
    statusRGBColor_life[2] = 0x82;
    statusRGBColor_life[3] = 0x80;

    statusRGBColor_weapon[0] = 0xFF;
    statusRGBColor_weapon[1] = 0x80;
    statusRGBColor_weapon[2] = 0x00;
    statusRGBColor_weapon[3] = 0x80;

    statusRGBColor_30[0] = 0xFF;
    statusRGBColor_30[1] = 0x00;
    statusRGBColor_30[2] = 0xC6;
    statusRGBColor_30[3] = 0x80;

    statusRGBColor_15[0] = 0xFF;
    statusRGBColor_15[1] = 0x00;
    statusRGBColor_15[2] = 0x00;
    statusRGBColor_15[3] = 0x80;

    statusRGBColor_life_2[0] = 0x00;
    statusRGBColor_life_2[1] = 0x81;
    statusRGBColor_life_2[2] = 0xFF;
    statusRGBColor_life_2[3] = 0x80;

    statusRGBColor_weapon_2[0] = 0xFF;
    statusRGBColor_weapon_2[1] = 0x20;
    statusRGBColor_weapon_2[2] = 0x00;
    statusRGBColor_weapon_2[3] = 0x80;

    statusRGBColor_30_2[0] = 0x3F;
    statusRGBColor_30_2[1] = 0x00;
    statusRGBColor_30_2[2] = 0x31;
    statusRGBColor_30_2[3] = 0x80;

    statusRGBColor_15_2[0] = 0x3F;
    statusRGBColor_15_2[1] = 0x00;
    statusRGBColor_15_2[2] = 0x00;
    statusRGBColor_15_2[3] = 0x80;
}

/**
 * Advances the pulse that warns of low life.
 *
 * @mangled BtStatusAlarmAnime__Fv
 * @address 0x1B0320
 * @size 0xC8
 */
void BtStatusAlarmAnime() {
    statusAlarmCounter += 0.10471976f;

    if (!(statusAlarmCounter < PI)) {
        statusAlarmCounter -= PI;
    }

    statusAlarmRate = 192 - (int) (2.0f * (64.0f * sinf(statusAlarmCounter)));
    statusRGBColor_15[3] = 192 - (int) (2.0f * (64.0f * sinf(statusAlarmCounter)));
}

/**
 * Chooses the colour a status bar draws in from how full it is.
 *
 * @mangled BtGetStatusPal__Fiff
 * @address 0x1B03F0
 * @size 0x80
 */
u8 *BtGetStatusPal(int bar, float max, float value) {
    u8 *color;

    if (!(value <= 0.2f * max)) {
        if (bar == 0) {
            color = statusRGBColor_life;
        } else {
            color = statusRGBColor_weapon;
        }

        return color;
    }

    if (value <= 0.3f * max && !(value <= 0.15f * max)) {
        return statusRGBColor_30;
    }

    return statusRGBColor_15;
}

/**
 * Chooses the second colour a status bar draws in from how full it is.
 *
 * @mangled BtGetStatusPal2__Fiff
 * @address 0x1B0470
 * @size 0x80
 */
u8 *BtGetStatusPal2(int bar, float max, float value) {
    u8 *color;

    if (!(value <= 0.2f * max)) {
        if (bar == 0) {
            color = statusRGBColor_life_2;
        } else {
            color = statusRGBColor_weapon_2;
        }

        return color;
    }

    if (value <= 0.3f * max && !(value <= 0.15f * max)) {
        return statusRGBColor_30_2;
    }

    return statusRGBColor_15_2;
}

/**
 * Draws the dungeon HUD with life, weapon, water, quick-item and floor indicators.
 *
 * @mangled topStatusInfo__Fiii
 * @address 0x1B04F0
 * @size 0x1438
 */
void topStatusInfo(int y, int selected_item, int floor);

/**
 * Reports whether the party is suffering one status ailment.
 *
 * @mangled StatusErrCheck__Fi
 * @address 0x1B1930
 * @size 0x28
 */
int StatusErrCheck(int status) {
    return (UserStatus->ailments[UserStatus->cur_chara] & status) ? 1 : 0;
}

/**
 * Chooses the tint the party's status ailment gives them.
 *
 * @mangled BtStatusErrColorSet__Fv
 * @address 0x1B1960
 * @size 0xE8
 */
int BtStatusErrColorSet() {
    int status;
    int ailing;

    ailing = 0;
    status = UserStatus->ailments[UserStatus->cur_chara];

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

/**
 * Steps since poison last took its toll.
 */
int poison_counter;

/**
 * Advances the party's status ailments and applies what they cost.
 *
 * @mangled BtStatusErrStep__Fv
 * @address 0x1B1A50
 * @size 0x154
 */
void BtStatusErrStep() {
    int          flags;
    CUserStatus *status = UserStatus;
    s8          *cur_chara = &status->cur_chara;

    flags = status->ailments[*cur_chara];
    poison_counter++;

    // Poison takes 4% of the character's max HP every 180 steps and shows the loss over them.
    if ((flags & AILMENT_POISON) && poison_counter >= 180) {
        int chara = *cur_chara;
        int max_hp = status->max_hp[chara];
        int damage = 0.04 * max_hp;
        status->AddNowLife((s8) *cur_chara, -damage, 10.0f);

        float position[4] = {0.0f, 0.0f, 0.0f, 1.0f};
        position[1] = CharaHeight(UserStatus);
        HitValueEntry(NowHitValue, position, damage, HIT_VALUE_PLAYER, CharaMain.frame);
    }

    if (poison_counter >= 180) {
        poison_counter = 0;
    }

    BtStatusErrColorSet();
}

/**
 * Inflicts one status ailment on the party.
 *
 * @mangled BtSetStatusErr__Fi
 * @address 0x1B1BB0
 * @size 0x1CC
 */
void BtSetStatusErr(int status) {
    int chara;

    chara = UserStatus->cur_chara;

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
    }
}

/**
 * Draws the icons of the party's status ailments.
 *
 * @mangled BtStatusErrDraw__Fi
 * @address 0x1B1D80
 * @size 0x16C
 */
void BtStatusErrDraw(int y) {
    CTexture *texture = TexManager.GetTexture("itempack", -1);
    int       status_flags[5] = {AILMENT_FREEZE, AILMENT_STAMINA, AILMENT_POISON, AILMENT_CURSE, AILMENT_GOO};
    int       icon_cells[5][2] = {
        {1, 0},
        {0, 1},
        {1, 1},
        {1, 2},
        {0, 2}
    };
    int status = UserStatus->ailments[UserStatus->cur_chara];

    for (int icon = 4; icon >= 0; icon--) {
        if (status & status_flags[icon]) {
            set2DSprite(Vif1Packet, texture, CRect_i_(430, y - 10, 62, 35), CRect_i_(icon_cells[icon][0] * 62 + 132, icon_cells[icon][1] * 36 + 84, 62, 36));
        }
    }
}

/**
 * Draws one item into the reserved slot area.
 *
 * @mangled setItemToReserved__FPciiPcii
 * @address 0x1B1EF0
 * @size 0x1CC
 */
void setItemToReserved(char *page_name, int x, int y, char *item_name, int dsax, int dsay);

/**
 * Whether the jump cache holds a map.
 */
int BtCfgFlag;

/**
 * The message shown for the cached map jump.
 */
int BtSteebMsgNo;

/**
 * Clears the cached map-jump data.
 *
 * @mangled BtMapJumpCashClear__Fv
 * @address 0x1B20C0
 * @size 0x1C
 */
void BtMapJumpCashClear() {
    BtCfgCash[0] = '\0';
    BtCfgFlag = 0;
    BtSteebMsgNo = -1;
}

/**
 * Size, in quadwords, of the arena an allocator was given.
 */
static inline int ArenaLimit(CDataAlloc2<1> &arena) {
    return arena.limit;
}

/**
 * Loads the steeb message file for the current floor if it changed, then reads the map a jump
 * leads to, its treasure-box models and item-get motions; returns 0 when that map is already
 * loaded, 1 otherwise.
 *
 * @mangled BtMapJumpLoad__FPc
 * @address 0x1B20E0
 * @size 0x70C
 */
int BtMapJumpLoad(char *map_name) {
    char mpd_path[64];
    char cfg_path[64];
    char mes_path[64];
    int  mes_size;
    int  size;

    CUserStatus *status = UserStatus;
    int          msg_no = status->cur_georama;
    int          floor = status->cur_floor;

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
        u_char *mes = BtSteebMesBuffer.base + BtSteebMesBuffer.used * 16;

        if (status->cur_georama == 6) {
            sprintf(mes_path, "dun/message/ww_mes/steve07_%d_%d.mes", msg_no - 5, LanguageCode);
        } else {
            sprintf(mes_path, "dun/message/ww_mes/steve0%d_%d.mes", msg_no + 1, LanguageCode);
        }

        LoadFile(mes_path, mes, &mes_size);
        wait_now_loading_vsync();
        DngMesStb.SetBuff((short *) mes);
        BtSteebMesBuffer.Alloc((((mes_size >> 6) + 1) << 6) >> 4);
        BtSteebMesBuffer.Align64();
        BtSteebMsgNo = msg_no;
    }

    TexManager.DeleteTextureBlock(42);
    MainMonstorUnit.CleanViewMonstor(0);
    NowDngMap = &MainDungeonMap;
    NowEventMan = &DngEventMan;

    if (strcmp(BtCfgCash, map_name) == 0 && BtCfgFlag != 0) {
        return 0;
    }

    strcpy(BtCfgCash, map_name);

    MapModelBuffer.used = 0;
    u_int *pack = read_buffer;
    read_buffer = old_read_buffer;

    int                 i;
    DRAN_MAP_FIELD_SET *field_set = (DRAN_MAP_FIELD_SET *) NowDranMapField;

    for (i = 0; i < 12; i++) {
        field_set->field[i].Initialize();
        field_set->collision[i] = NULL;
        field_set->state[i] = 3;
    }

    field_set->field_count = 0;
    field_set->collision_count = 0;

    MainDungeonMap.initSubmap(&MapModelBuffer);

    for (int slot = 0; slot < 64; slot++) {
        CDungeonEvent *event = &DngEventMan.slot[slot];
        event->name[0] = '\0';
        event->script_no = -1;
        event->parts_id = -1;
        event->switch_on = false;
        event->enabled = false;
    }

    for (int record = 0; record < 96; record++) {
        CDungeonEventData *data = &DngEventMan.event[record];
        data->event = NULL;
        data->switch_on = false;
        data->enabled = false;
        data->hold = 0;
        data->chara_done = -1;
    }

    strcpy(mpd_path, "dun/mpd_pack/");
    strcat(mpd_path, map_name);
    strcat(mpd_path, ".mpd");
    strcpy(cfg_path, "dun/");
    strcat(cfg_path, map_name);
    strcat(cfg_path, ".cfg");
    TEIGIAnalyz(cfg_path);

    size = 0;
    LoadFile(mpd_path, (void *) read_buffer, &size);
    wait_now_loading_vsync();

    if (size >= 6400000) {
        printf("MAPDATA BUFFER OVER!! %d\n", size);
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
    int     map_used = MapModelBuffer.used;
    u_char *monster_base = MapModelBuffer.base + map_used * 16;
    long    monster_size = 0xA7F80 - map_used;
    MonstorModelBuffer.base = monster_base;
    MonstorModelBuffer.limit = monster_size;
    MonstorModelBuffer.used = 0;
    read_buffer = pack;

    printf("TotalMem = %d\n", 0xA7F80);
    printf("MapVisualData   [%x]  %d/%d\n", MapModelBuffer.base + MapModelBuffer.used * 16, MapModelBuffer.used, ArenaLimit(MapModelBuffer));
    printf("MonstorData     [%x]  %d/%d\n", MonstorModelBuffer.base + MonstorModelBuffer.used * 16, MonstorModelBuffer.used, ArenaLimit(MonstorModelBuffer));
    BtCfgFlag = 1;
    return 1;
}

/**
 * Draws a textured cell in world space.
 *
 * @mangled BtSet3DCellModel__FPfP8CTexturefiiiii
 * @address 0x1B27F0
 * @size 0x108
 */
void BtSet3DCellModel(float *world, CTexture *texture, float size, int x, int y, int width, int height, int alpha) {
    int      top_left[4];
    int      top_right[4];
    int      bottom_left[4];
    int      bottom_right[4];
    CRect_i_ source;

    world[3] = 1.0f;

    if (MGRotTransPers3DSprite(top_left, bottom_right, world, size, size / 2.0f, 0) == 1) {
        top_right[0] = bottom_right[0];
        top_right[1] = top_left[1];
        top_right[2] = top_left[2];
        bottom_left[0] = top_left[0];
        bottom_left[1] = bottom_right[1];
        bottom_left[2] = bottom_right[2];

        source.x = x;
        source.y = y;
        source.width = width;
        source.height = height;

        set3DSprite(Vif1Packet, texture, source, top_left, top_right, bottom_left, bottom_right, alpha);
    }
}
