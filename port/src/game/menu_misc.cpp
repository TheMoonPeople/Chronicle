#include "menu_misc.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <format>
#include <iterator>
#include <memory>
#include <print>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include "battlemenu.hpp"
#include "bt_shot_effect.hpp"
#include "btactstatus.hpp"
#include "btmisc.hpp"
#include "character.hpp"
#include "clsmes.hpp"
#include "dataalloc.hpp"
#include "dataread.hpp"
#include "dngstatusdata.hpp"
#include "dun/gameloop.hpp"
#include "editloop.hpp"
#include "frame.hpp"
#include "gamepad.hpp"
#include "itemdata.hpp"
#include "mainselect.hpp"
#include "memcard.hpp"
#include "menu_draw.hpp"
#include "menu_dungeon.hpp"
#include "menu_inventory.hpp"
#include "menu_manual.hpp"
#include "menuitemstep.hpp"
#include "mglib.hpp"
#include "rect.hpp"
#include "savedata.hpp"
#include "shot_effect.hpp"
#include "snd.hpp"
#include "texture.hpp"
#include "userstatus.hpp"
#include "weapon_buildup.hpp"
#include "weaponeffect.hpp"
#include "weaponlevelup.hpp"

namespace dc::game {

int MenuWeaponModelData[42];
int MenuWeaponModelInfo[10][2];

namespace {

// Defined straight after MenuWeaponModelInfo: InitMenuWeaponModelReference clears twelve entries
// of that ten-entry table, and the two extra land here.
float MenuCharaOldAmbient[4];

constexpr std::array<int, 6> defWeapon = {ITEM_WEAPON_DAGGER_BROKEN, ITEM_WEAPON_WOODENSLINGSHOT_BROKEN, ITEM_WEAPON_MALLET_BROKEN, ITEM_WEAPON_GOLD_RING_BROKEN, ITEM_WEAPON_FIGHTING_STICK_BROKEN, ITEM_WEAPON_MACHINE_GUN_BROKEN};

constexpr std::string_view                MenuWepDir = "commenu/weapon/";
constexpr std::string_view                readFilePath = "dun/mainchara/c";
constexpr std::array<std::string_view, 6> charaFile = {"01d", "04b", "06b", "05a", "10b", "18a"};

int             MenuItemUseVolume;
u_long128      *WeaponRead_Buf;
s16             MenuCharaEffectReadFlag;
s16             MenuCharaOldEffect;
BT_SHOT_EFFECT *WepEffectMenuPt;
int             CharaFileBGReadNo;
u_long128      *CharaChangeBaseBuf;
u_long128      *menucharReadbuf;
u_long128      *menud0wepReadBuf;
u_long128      *menud1wepReadBuf;
u_long128      *menud2wepReadBuf;
u_long128      *MenuWepIconCharaChangePtr;
u_long128      *MenuVoiceLoadPtr;
ClsMes         *CharaNameMes;
ClsMes         *DngMenuMes;
s16             CharaNameDrawFlag;
s16             CharaNameDrawCase;
CTexture       *DngEscapeTex;
s16             DngEscapeBlock;
s16             DngEscapeEndFlag;
int             MenuWeaponTestCase = 1;

/**
 * The darkness drawn over the dungeon escape prompt, from 0 (none) to 0x80 (black).
 */
s16 DngEscapeAlpha = 0x80;

/**
 * The dungeon escape prompt's chosen answer, 1 or 2.
 */
s16 DngEscapeSelect = 1;

} // namespace

s16        charachangeid;
u_long128 *WepEffectMenuReadBuf;
u_long128 *MenuWeaponModelBuildBuffer;

int NowGetGameFlagForBtlMenu(int game_flag) {
    switch (game_flag) {
        case MENU_MOVE_DUNGEON_ESCAPE:
            break;
        case MENU_MOVE_WORLD_MAP_DIRECT:
        case MENU_MOVE_WORLD_MAP:
            if (EdInteriorFlag == 1) {
                game_flag = MENU_MOVE_INTERIOR_OUT;
            } else if (BtlMenuStatusPt->special_flag_238 == 0) {
                if (SaveData->QuestDungeon(0, 0) == 0) {
                    game_flag = MENU_MOVE_LOCKED;
                } else if (BtlMenuStatusPt->special_flag_238 == 0) {
                    game_flag = MENU_MOVE_FIRST_DUNGEON;
                }
            }

            break;
        default:
            break;
    }

    return game_flag;
}

int GetMenuHebikiriFlag() {
    return SaveData->GetGameFlag(0x30);
}

void EquipDefaultWeapon(int chara_no) {
    CDngStatusData *status = SaveData->GetDngStatus();
    WEAPON_HAVE    *weapons = status->chara_weapons[chara_no];
    int             default_weapon_no = GetDefaultWeaponNo(chara_no);

    for (int slot = 0; slot < 10; slot++) {
        if (weapons[slot].item_no == default_weapon_no || weapons[slot].item_no == default_weapon_no + 1) {
            status->equipped_weapon_slot[chara_no] = slot;
            break;
        }
    }
}

void DrawMenuNothing(int x, int y, int width, int height, char *name, int custom, int alpha) {
    CTexture *texture = TexManager.GetTexture(name, -1);
    int       inner_w = width - 0x24;
    int       inner_h = height - 0x24;
    int       u = 0;
    int       v = 0;

    if (custom == 0) {
        u = 0xD4;
        v = 0x159;
    }

    DrawMenu2DSprite(texture, CRect_i_(x, y, 0x12, 0x12), CRect_i_(u, v, 0x12, 0x12), alpha);
    DrawMenu2DSprite(texture, CRect_i_(x + 0x12, y, inner_w, 0x12), CRect_i_(u + 0x10, v, 4, 0x12), alpha);
    DrawMenu2DSprite(texture, CRect_i_(x + 0x12 + inner_w, y, 0x12, 0x12), CRect_i_(u + 0x12, v, 0x12, 0x12), alpha);
    DrawMenu2DSprite(texture, CRect_i_(x, y + 0x12, 0x12, inner_h), CRect_i_(u, v + 0xE, 0x12, 4), alpha);
    DrawMenu2DSprite(texture, CRect_i_(x + 0x12 + inner_w, y + 0x12, 0x12, inner_h), CRect_i_(u + 0x12, v + 0xE, 0x12, 4), alpha);
    DrawMenu2DSprite(texture, CRect_i_(x, y + 0x12 + inner_h, 0x12, 0x12), CRect_i_(u, v + 0x12, 0x12, 0x12), alpha);
    DrawMenu2DSprite(texture, CRect_i_(x + 0x12, y + 0x12 + inner_h, inner_w, 0x12), CRect_i_(u + 0x10, v + 0x12, 4, 0x12), alpha);
    DrawMenu2DSprite(texture, CRect_i_(x + 0x12 + inner_w, y + 0x12 + inner_h, 0x12, 0x12), CRect_i_(u + 0x12, v + 0x12, 0x12, 0x12), alpha);
    DrawMenu2DSprite(texture, CRect_i_(x - 0x33 + (inner_w >> 1), y - 1 + (inner_h >> 1), 0x86, 0x22), CRect_i_(0xFA, 0x1D2, 0x86, 0x22), alpha);
}

int GetMenuItemUseVolume() {
    return MenuItemUseVolume;
}

int ItemUseFunc(CUserStatus *status, int item_no, int chara, int target, WEAPON_HAVE *weapon) {
    int         used = 0;
    WEAPON_HAVE level_up;

    MenuItemUseVolume = 0;
    int   now_hp = status->hp[chara];
    int   max_hp = status->max_hp[chara];
    int   now_water = static_cast<int>(status->water_now[chara]);
    float max_water = status->water_max[chara];
    int   old_condition = status->ailments[chara];
    printf("trueNo = %d\n", item_no);
    int        value_no = 0;
    ITEM_DATA *data = GetItemData(item_no);

    if (data == nullptr) {
        return 0;
    }

    if ((data->kind_flags & ITEMKINDF_CONSUMABLE) && target == ITEM_USE_ON_CHARA) {
        if (item_no != ITEM_MELLOW_BANANA) {
            if ((data->use_flags & ITEMUSE_HEAL_HP) && 0 < now_hp && now_hp < max_hp) {
                int add = (&data->vol)[value_no];
                value_no++;
                status->SetNextLife(chara, now_hp + add, 5.0f);
                used = 1;
            }

            if ((data->use_flags & ITEMUSE_DRINK) && 2.0f + now_water < max_water) {
                int add = (&data->vol)[value_no];
                value_no++;
                float next = status->water_now[chara];
                float limit = status->water_max[chara];
                next += add;

                if (limit < next) {
                    next = limit;
                }

                status->water_now[chara] = next;
                used = 1;
            }
        } else if (now_hp > 0 && now_hp < max_hp) {
            value_no++;
            status->SetNextLife(chara, now_hp + 200, 5.0f);
            status->AddDrink(chara, -20, 1.0f);
            used = 1;
        }
    }

    if ((data->kind_flags & ITEMKINDF_RANDOM) && (data->use_flags & ITEMUSE_STATUS) && item_no >= ITEM_FLUFFY_DOUGHNUT && item_no < ITEM_DUMMY) {
        if (chara == item_no - ITEM_FLUFFY_DOUGHNUT) {
            int old = status->defense[chara];
            int range = data->vol_range;

            if (range <= 0) {
                printf("value=0\n");
                range = 1;
            }

            int add = data->vol + rand() % range;
            MenuItemUseVolume = add;
            old += add;

            if (old >= 99) {
                old = 99;
            }

            status->defense[chara] = old;
            used = 1;
        } else {
            used = 0;
        }
    }

    if (data->kind_flags & 1) {
        if (target == ITEM_USE_ON_CHARA) {
            if (data->use_flags & ITEMUSE_STATUS) {
                switch (item_no) {
                    case ITEM_POCKET: {
                        ITEM_PACK *pack = &status->item_pack;
                        int        num = pack->num;

                        if (num < 0x60) {
                            pack->num += (&data->vol)[value_no];
                            MenuItemUseVolume = pack->num;

                            if (pack->num > 100) {
                                pack->num = 100;
                            }

                            for (int i = num; i < pack->num; i++) {
                                if (pack->item[i] < ITEM_DUNGEON_START) {
                                    pack->item[i] = -1;
                                }
                            }

                            used = 1;
                        }

                        break;
                    }
                    case ITEM_FRUIT_OF_EDEN: {
                        s16 limit[6] = {0xAA, 0x8C, 0xAA, 0x8C, 0xB4, 0xA0};

                        if ((old_condition & AILMENT_KNOCKED_OUT) || now_hp <= 0 || !(static_cast<s16>(max_hp) < limit[chara])) {
                            used = 0;
                        } else {
                            status->max_hp[chara] = max_hp + (&data->vol)[value_no];
                            s16 next = status->max_hp[chara];

                            if (next >= limit[chara]) {
                                next = limit[chara];
                                status->max_hp[chara] = limit[chara];
                            }

                            status->SetNextLife(chara, next, 0.0f);
                            used = 1;
                        }

                        break;
                    }
                    case ITEM_GOURD:
                        if (max_water <= 90.0f) {
                            float next = max_water + (&data->vol)[value_no];
                            status->water_max[chara] = next;
                            status->water_now[chara] = next;
                            used = 1;
                        }

                        break;
                    default:
                        break;
                }
            }

            printf("prev status = %d\n", old_condition);
            u32 flags = data->use_flags;

            if ((old_condition & AILMENT_KNOCKED_OUT) || now_hp <= 0) {
                if (item_no == ITEM_REVIVAL_POWDER) {
                    status->ailments[chara] = 0;
                    status->hp[chara] = max_hp >> 1;
                    used = 1;
                }
            } else {
                if ((flags & 0x4000) && (old_condition & AILMENT_FREEZE)) {
                    status->ailments[chara] ^= AILMENT_FREEZE;
                    used = 1;
                }

                if ((data->use_flags & 0x8000) && (old_condition & AILMENT_POISON)) {
                    status->ailments[chara] ^= AILMENT_POISON;
                    used = 1;
                }

                if ((data->use_flags & 0x10000) && (old_condition & AILMENT_CURSE)) {
                    status->ailments[chara] ^= AILMENT_CURSE;
                    used = 1;
                }

                if ((data->use_flags & 0x20000) && (old_condition & AILMENT_GOO)) {
                    status->ailments[chara] ^= AILMENT_GOO;
                    used = 1;
                }

                if (item_no == ITEM_MIGHTY_HEALING && (old_condition & (AILMENT_FREEZE | AILMENT_POISON | AILMENT_CURSE | AILMENT_GOO))) {
                    if (status->ailments[chara] & AILMENT_STAMINA) {
                        status->ailments[chara] = 0;
                        status->ailments[chara] |= AILMENT_STAMINA;
                    } else {
                        status->ailments[chara] = 0;
                    }

                    used = 1;
                }

                if (BtlMenuMode == BATTLE_MENU_MODE_DUNGEON && (data->use_flags & 0x1000)) {
                    int now = status->ailments[chara];

                    if (!(now & AILMENT_FREEZE)) {
                        status->ailments[chara] = now | AILMENT_STAMINA;
                        status->ailment_frames[chara] = 0x708;
                        used = 1;
                    }
                }
            }
        }

        if (target == ITEM_USE_ON_BOARD_WEAPON || target == ITEM_USE_ON_EQUIP_WEAPON) {
            if (weapon == nullptr) {
                return used;
            }

            int default_no = GetDefaultWeaponNo(chara);

            if (item_no == ITEM_REPAIR_POWDER && weapon->durability_f < weapon->durability) {
                weapon->durability_f = weapon->durability;
                int weapon_no = weapon->item_no;

                if (weapon_no == default_no) {
                    weapon->item_no = weapon_no + 1;
                    WepDataListToHaveCopy(weapon->item_no, weapon);
                }

                used = 2;
            }

            if (item_no == ITEM_POWERUP_POWDER) {
                if (weapon->item_no == ITEM_WEAPON_SERPENT_SWORD && GetMenuHebikiriFlag() == 0) {
                    return 0;
                }

                if (weapon->level >= 99) {
                    return 0;
                }

                if (weapon->item_no != default_no) {
                    int exp = weapon->experience;

                    if (exp < GetWeaponMaxExp(weapon)) {
                        WeaponLevelUpValueCalc(weapon, &level_up, 1, 0);
                        memcpy(weapon, &level_up, sizeof(WEAPON_HAVE));
                        weapon->level++;
                        memset(weapon->attach, 0, sizeof(weapon->attach));
                        used = 1;
                    }
                }
            }
        }
    }

    if (BtlMenuMode == BATTLE_MENU_MODE_DUNGEON && !(old_condition & AILMENT_KNOCKED_OUT) && now_hp > 0 && (data->kind_flags & 2) && target == ITEM_USE_ON_CHARA) {
        int before = status->ailments[chara];

        if (data->use_flags & 0x100) {
            if (old_condition & AILMENT_FREEZE) {
                status->ailments[chara] = before ^ AILMENT_FREEZE;
            } else {
                status->ailments[chara] = before | AILMENT_FREEZE;
                status->ailments[chara] &= ~(AILMENT_STAMINA | AILMENT_POISON | AILMENT_GOO);
                status->ailment_frames[chara] = 300;
            }

            ComMenuSePlay(SE_STATUS_AILMENT);
        }

        if (data->use_flags & 0x200) {
            int now = status->ailments[chara];

            if (!(now & (AILMENT_FREEZE | AILMENT_STAMINA))) {
                status->ailments[chara] = now | AILMENT_POISON;
                status->ailments[chara] &= ~AILMENT_GOO;
                ComMenuSePlay(SE_STATUS_AILMENT);
            }
        }

        if (data->use_flags & 0x400) {
            status->ailments[chara] |= AILMENT_CURSE;
            ComMenuSePlay(SE_STATUS_AILMENT);
        }

        if (data->use_flags & 0x800) {
            int now = status->ailments[chara];

            if (!(now & (AILMENT_FREEZE | AILMENT_STAMINA | AILMENT_POISON))) {
                status->ailments[chara] = now | AILMENT_GOO;
                ComMenuSePlay(SE_STATUS_AILMENT);
            }
        }

        if (before != status->ailments[chara]) {
            used = 1;
        }
    }

    return used;
}

float GetNowWeaponRate(WEAPON_HAVE *weapon) {
    float rate = 1.0f;

    if (weapon != nullptr && weapon->item_no == ITEM_WEAPON_LAMB_S_SWORD && weapon->durability_f <= 0.2f * weapon->durability) {
        rate = 1.5f;
    }

    return rate;
}

int WeaponStatusBreakEnable(WEAPON_HAVE *weapon) {
    if (weapon == nullptr) {
        return 0;
    }

    return weapon->level >= 5 ? 1 : 0;
}

int WeaponStatusBuildUp(WEAPON_HAVE *weapon, int &enabled_count) {
    if (weapon == nullptr) {
        return 0;
    }

    WEP_BUILDUP_INFO build_info[8];
    EnableBuildUpModel(build_info, weapon);

    int total = 0;

    while (build_info[total].weapon_no != -1 && total < 5) {
        total++;
    }

    for (int i = 0; i < total; i++) {
        if (build_info[i].enabled == 1) {
            enabled_count++;
        }
    }

    return total;
}

void MenuWeaponSpSet(CCharacter *chara, WEAPON_HAVE *weapon) {
    if (chara == nullptr || weapon == nullptr) {
        return;
    }

    if (weapon->item_no != ITEM_WEAPON_LAMB_S_SWORD) {
        return;
    }

    CFrame *whole = chara->frame->SearchFrame("w15a");
    CFrame *broken = chara->frame->SearchFrame("w15b");

    if (whole == nullptr || broken == nullptr) {
        return;
    }

    int show_broken = 2;
    int show_whole = 1;

    if (weapon->durability_f <= 0.2 * weapon->durability) {
        show_broken = 1;
        show_whole = 2;
    }

    whole->attr.draw_on = show_whole;
    broken->attr.draw_on = show_broken;
}

void SetMenuCharaEffectReadFlag(int flag) {
    MenuCharaEffectReadFlag = flag;
}

int GetMenuCharaEffectReadFlag() {
    return MenuCharaEffectReadFlag;
}

BT_SHOT_EFFECT *GetDngWepEffectPointer() {
    return WepEffectMenuPt;
}

u_long128 *GetWepEffectMenuReadBuf() {
    return WepEffectMenuReadBuf;
}

void SetOldEffectKind(int kind) {
    MenuCharaOldEffect = kind;
}

namespace {

void SetWepEffectMenuReadBuf(u_long128 *buffer) {
    WepEffectMenuReadBuf = buffer;
}

std::string WeaponEffectPath(const BT_SHOT_EFFECT *effect) {
    const char *name = effect->model_name;
    return std::format("dun/mainchara/wep_eff/{}.chr", name);
}

} // namespace

BT_SHOT_EFFECT *DngWepEffectReadStart() {
    int          size = 0;
    CUserStatus *status = BtlMenuStatusPt;
    int          chara = status->cur_chara;
    int          slot = status->equipped_weapon_slot[chara];
    WEAPON_HAVE *weapons = status->chara_weapons[chara];
    WEAPON_HAVE *equipped = &weapons[slot];

    WepEffectMenuPt = Get_Main_EffectPtr(chara, equipped->best_elem);
    std::string path = WeaponEffectPath(WepEffectMenuPt);
    WepEffectMenuReadBuf = MenuCalcBufAlignment(WepEffectMenuReadBuf);
    LoadFileBG(path.data(), WepEffectMenuReadBuf, &size);
    SetMenuCharaEffectReadFlag(1);
    return GetDngWepEffectPointer();
}

void MenuWeaponEffectSet(int effect_no) {
    SetMenuCharaEffectReadFlag(0);
    MainChara_Effect(WepEffectMenuPt, reinterpret_cast<unsigned int *>(WepEffectMenuReadBuf), effect_no);
}

int GetNowTestNo() {
    return MenuWeaponTestCase;
}

int StartReadWepMDS(u_long128 *buffer, int chara) {
    if (ReadBGSync() == 1) {
        BreakReadBG();
    }

    constexpr std::array<std::string_view, 6> numbers = {"01", "04", "06", "05", "10", "18"};
    constexpr std::array<std::string_view, 6> names = {"kgetoan", "kgesyao", "kgegoro", "kgeruby", "kgeunga", "kgeozu"};
    int                                       size = 0;

    std::string path = std::format("commenu/c{}wtes.chr", numbers[chara]);
    WeaponRead_Buf = MenuCalcBufAlignment(buffer);
    StartReadBG();

    if (LoadFileBG(path.data(), buffer, &size) == 0) {
        return 0;
    }

    buffer += (size >> 4) + 1;
    buffer = MenuCalcBufAlignment(buffer);
    std::array<char, 64> lang_dir{};
    GetPathReadDifferntLang(lang_dir.data());
    std::string image = std::format("{}{}2.img", lang_dir.data(), names[chara]);

    if (LoadFileBG(image.data(), buffer, &size) == 0) {
        return 0;
    }

    if (BtlMenuMode == BATTLE_MENU_MODE_DUNGEON) {
        buffer += (size >> 4) + 1;
        SetWepEffectMenuReadBuf(buffer);
        DngWepEffectReadStart();
    }

    return 1;
}

namespace {

// Each weapon's .chr address, kept here rather than in the int-sized MenuWeaponModelData table.
std::array<u_int *, 42> weapon_model_files;

u_int **GetMenuWeaponModelData(int index) {
    return &weapon_model_files[index];
}

void InitMenuWeaponModelData() {
    memset(MenuWeaponModelData, 0, sizeof(MenuWeaponModelData));
    weapon_model_files.fill(nullptr);
}

int *GetMenuWeaponModelInfo(int index) {
    return MenuWeaponModelInfo[index];
}

std::pair<std::string, std::string> WeaponNames(int chara, int weapon_index) {
    constexpr std::array<std::string_view, 6> prefixes = {"c01w", "c04w", "c06w", "c05w", "c10w", "c18w"};
    if (chara < 0 || std::cmp_greater_equal(chara, prefixes.size()) || weapon_index < 0) {
        return {};
    }
    std::string number = std::to_string(weapon_index);
    if (weapon_index < 10) {
        number.insert(0, "0");
    }
    const std::string base = std::string(prefixes[chara]) + number;
    return {base + ".chr", base + ".cfg"};
}

} // namespace

void InitMenuWeaponModelReference() {
    for (int i = 0; i < 12; i++) {
        int *entry = GetMenuWeaponModelInfo(i);
        entry[0] = -1;
        entry[1] = -1;
    }
}

void SetMenuWeaponModelReference(int index, int frame_no, int value) {
    int *entry = GetMenuWeaponModelInfo(index);
    entry[0] = frame_no;
    entry[1] = value;
}

int GetMenuWeaponModelFrameNo(int index) {
    return MenuWeaponModelInfo[index][0];
}

int EnterWeaponModel(int chara, int texture_block, [[maybe_unused]] int weapon_slot) {
    if (chara < 0 || chara >= 6) {
        return 1;
    }
    BG_READ_INFO                             *pack = GetReadBGFile(0);
    BG_READ_INFO                             *shadow = GetReadBGFile(1);
    BG_READ_INFO                             *effect = GetReadBGFile(2);
    const int                                 weapon_max = std::clamp<int>(MenuCharaWeaponMax[chara], 0, static_cast<int>(std::size(weapon_model_files)));
    constexpr std::array<std::string_view, 6> names = {"c01", "c04", "c06", "c05", "c10", "c18"};
    std::vector<int>                          order(static_cast<std::size_t>(weapon_max));

    switch (GetNowTestNo()) {
        case 0:
            break;
        case 1: {
            auto texture = std::make_unique<LOADTEXTURE_INFO2[]>(3);
            texture[0] = {.name = const_cast<char *>("#frame_menuwep_dmy#640#" SCREEN_HEIGHT_STR "#4"), .block_no = 0, .mipmap = 0};
            texture[1] = {.name = nullptr, .block_no = 0, .mipmap = 0};
            texture[2] = {.name = nullptr, .block_no = 0, .mipmap = 0};
            texture[0].block_no = MenuShadowReadBlock;
            texture[1].block_no = MenuShadowReadBlock;
            texture[1].name = reinterpret_cast<char *>(shadow->buffer);
            TexManager.DeleteTextureBlock(MenuShadowReadBlock);
            TexManager.CleanUpTextureList();
            TexManager.LoadTextureBlockEX(-1, texture.get());
            WepIcon = TexManager.GetTexture(const_cast<char *>("wepicon"), MenuShadowReadBlock);

            for (int i = 0; i < weapon_max; i++) {
                order[i] = i;
            }

            InitMenuWeaponModelData();

            for (int i = 0; i < weapon_max; i++) {
                const int   weapon_no = order[i];
                std::string chr{names[chara]};

                if (0 <= weapon_no && weapon_no <= 9) {
                    chr += "w0" + std::to_string(weapon_no);
                } else if (weapon_no >= 10) {
                    chr += "w" + std::to_string(weapon_no);
                } else {
                    chr += "w01";
                }

                chr += ".chr";
                u_int *file = GetPackFile(reinterpret_cast<u_int *>(pack->buffer), chr.data(), nullptr);

                if (file != nullptr) {
                    *GetMenuWeaponModelData(i) = file;
                }
            }

            u_long128 *build = nullptr;

            if (effect != nullptr) {
                SetWepEffectMenuReadBuf(effect->buffer);
                build = effect->buffer + 0x2D01;
            } else {
                build = shadow->buffer + (shadow->size >> 4) + 1;
            }

            MenuWeaponModelBuildBuffer = build;
            WeaponModelBuildFunc(chara, texture_block);
            break;
        }
        default:
            break;
    }

    return 1;
}

void WeaponModelBuildFunc(int chara, int texture_block) {
    if (chara < 0 || chara >= 6 || BtlMenuStatusPt == nullptr) {
        return;
    }
    std::println("weapon model build func start");
    InitMenuWeaponModelReference();
    std::array<LOADTEXTURE_INFO2, 2> textures = {
        LOADTEXTURE_INFO2{.name = const_cast<char *>("#frame_menuwep#640#" SCREEN_HEIGHT_STR "#4"), .block_no = texture_block, .mipmap = 0},
        LOADTEXTURE_INFO2{.name = nullptr,                                                          .block_no = 0,             .mipmap = 0},
    };
    TexManager.DeleteTextureBlock(texture_block);
    TexManager.CleanUpTextureList();
    TexManager.LoadTextureBlockEX(-1, textures.data());
    printf("modelbuildbuffer = %p\n", MenuWeaponModelBuildBuffer);
    MenuExCashBuffer.base = reinterpret_cast<u_char *>(MenuWeaponModelBuildBuffer);
    MenuExCashBuffer.limit = 0xEC00;
    MenuExCashBuffer.used = 0;
    WEAPON_HAVE *weapons = static_cast<CUserStatus *>(BtlMenuStatusPt)->chara_weapons[chara];
    const int    default_no = GetDefaultWeaponNo(chara);
    int          next = 2;
    u_int      **data = GetMenuWeaponModelData(0);
    auto [name, cfg] = WeaponNames(chara, 0);
    DngWeaponFrm[0].LoadPackData3(*data, cfg.data(), &MenuExCashBuffer, texture_block, &MenuExCashBuffer, 1, 0);
    data = GetMenuWeaponModelData(1);
    std::tie(name, cfg) = WeaponNames(chara, 1);
    DngWeaponFrm[1].LoadPackData3(*data, cfg.data(), &MenuExCashBuffer, texture_block, &MenuExCashBuffer, 1, 0);

    for (int i = 0; i < 10; i++) {
        WEAPON_HAVE *weapon = &weapons[i];

        if (weapon == nullptr) {
            SetMenuWeaponModelReference(i, -2, -1);
            continue;
        }

        int item_no = weapon->item_no;

        if (item_no < ITEM_WEAPON_START) {
            SetMenuWeaponModelReference(i, -2, -1);
            continue;
        }

        unsigned int kind = item_no - default_no;

        if (item_no < default_no || kind >= std::size(weapon_model_files)) {
            SetMenuWeaponModelReference(i, -2, -1);
            continue;
        }

        if (kind < 2U) {
            SetMenuWeaponModelReference(i, kind, kind);
            continue;
        }

        bool found = false;

        for (int j = 0; j < 10; j++) {
            int *info = GetMenuWeaponModelInfo(j);

            if (std::cmp_equal(info[1], kind)) {
                found = true;
                SetMenuWeaponModelReference(i, info[0], info[1]);
                break;
            }
        }

        if (!found) {
            u_int **pack = GetMenuWeaponModelData(kind);

            if (*pack == nullptr) {
                std::println("{} pack data is NULL", kind);
            } else {
                std::tie(name, cfg) = WeaponNames(chara, static_cast<int>(kind));
                DngWeaponFrm[next].LoadPackData3(*pack, cfg.data(), &MenuExCashBuffer, texture_block, &MenuExCashBuffer, 1, 0);
                SetMenuWeaponModelReference(i, next, kind);
                next++;
            }
        }
    }

    WepMenuEffectReadBuf = reinterpret_cast<CWeaponLevelUp *>(MenuWeaponModelBuildBuffer + 0xEC01);
    WepMenuEffectReadBuf = reinterpret_cast<CWeaponLevelUp *>(MenuCalcBufAlignment(reinterpret_cast<u_long128 *>(WepMenuEffectReadBuf)));
    printf("read buffer           = %p\n", read_buffer);
    printf("model build buffer    = %p\n", MenuWeaponModelBuildBuffer);
    std::println("WeaponBuffer Size     = {}", MenuExCashBuffer.limit);
    printf("WeaponBuffer address  = %p\n", MenuExCashBuffer.base + static_cast<std::ptrdiff_t>(MenuExCashBuffer.used) * 16);
    printf("WepMenuEffectReadBuf = %p\n", WepMenuEffectReadBuf);
}

int DngWeaponEquipModelBuild(int chara, int texture_block, [[maybe_unused]] u_long128 *read_buffer) {
    if (chara < 0 || chara >= 6) {
        return 0;
    }
    TexManager.DeleteTextureBlock(texture_block);
    u_int **first = GetMenuWeaponModelData(0);
    u_int **second = GetMenuWeaponModelData(1);
    int     kind = 0;

    if (UserStatus != nullptr) {
        const int slot = UserStatus->equipped_weapon_slot[chara];
        if (slot < 0 || slot >= 10) {
            return 0;
        }
        kind = UserStatus->chara_weapons[chara][slot].item_no;
        kind -= GetDefaultWeaponNo(chara);
        if (kind < 0 || std::cmp_greater_equal(kind, weapon_model_files.size())) {
            return 0;
        }
    }

    u_int **equipped = GetMenuWeaponModelData(kind);

    if (equipped == nullptr) {
        equipped = second;
    } else if (*equipped == nullptr) {
        *equipped = *second;
    }

    LoadWeapon2(*first, *second, *equipped, chara, 1);
    MenuWeaponEffectSet(1);
    return 1;
}

namespace {

/**
 * Returns the motion speed step a character's status bits select.
 */
int GetNowMotionStepCnt(int status) {
    int step = 0;

    if (status & 0x40) {
        step = 1;
    }

    if ((status & 4) || (status & 2)) {
        step = 2;
    }

    return step;
}

} // namespace

int GetNowActiveCharaStatus(int chara_no) {
    if (BtlMenuStatusPt == nullptr) {
        return 0;
    }

    return BtlMenuStatusPt->GetActiveCharaStatus(chara_no);
}

void SetNowCharaMotionNo(int chara) {
    int          status = GetNowActiveCharaStatus(chara);
    CUserStatus *user_status = BtlMenuStatusPt;
    float        max_hp = user_status->max_hp[chara];
    float        hp = user_status->hp[chara];
    int          motion = MenuCharaFrame.motion_no;
    float        speed[3] = {0.1f, 0.05f, 0.0f};
    int          next = motion;

    if (!(hp < 0.3f * max_hp)) {
        next = 0;
    }

    if ((status & 0x10) || (status & 2) || hp < 0.3f * max_hp) {
        next = 1;
    }

    if (next != motion) {
        MenuCharaFrame.SetMotion(next, 0);
        motion = next;
    }

    MOTION_INFO *info = MenuCharaFrame.GetMotionInfo(motion);
    info->speed = speed[GetNowMotionStepCnt(status)];
}

void SetItemMenuColor(int chara) {
    float red = 0.0f;
    float green = 0.0f;
    float blue = 0.0f;
    int   should_tint = 0;

    MGGetAmbient(MenuCharaOldAmbient);
    int status = GetNowActiveCharaStatus(chara);

    if (status & 4) {
        red = 83.0f;
        green = 104.0f;
        blue = 95.0f;
        should_tint = 1;
    }

    if (status & 0x40) {
        red = 36.0f;
        green = 148.0f;
        blue = 195.0f;
        should_tint = 1;
    }

    if (status & 0x10) {
        red = 156.0f;
        green = 122.0f;
        blue = 182.0f;
        should_tint = 1;
    }

    if (status & 0x20) {
        red = 171.0f;
        green = 40.0f;
        blue = 125.0f;
        should_tint = 1;
    }

    if (status & 8) {
        red = 230.0f;
        green = 168.0f;
        blue = 92.0f;
        should_tint = 1;
    }

    if (should_tint == 1) {
        float ambient[4] = {red, green, blue, 128.0f};
        MGSetAmbient(ambient);
    }
}

void SetItemMenuOldAmbient() {
    MGSetAmbient(MenuCharaOldAmbient);
}

int StartLoadCharaMDS(u_long128 *buffer, int chara, int read_no) {
    int size = 0;

    std::string path = std::format("{}dungeon/c0{}model.pak", GetMenuTextureDir(), chara + 1);
    buffer = MenuCalcBufAlignment(buffer);
    CharaFileBGReadNo = read_no;

    if (read_no == 0) {
        StartReadBG();
    }

    if (LoadFileBG(path.data(), buffer, &size) == 0) {
        return 1;
    }

    return 0;
}

void MenuCharaMDSBuild2(int chara, int texture_block) {
    int size = 0;

    std::string       image_name = std::format("c0{}dmenu.img", chara + 1);
    LOADTEXTURE_INFO2 textures[3] = {
        {.name = const_cast<char *>("#frame_menu_chara#640#" SCREEN_HEIGHT_STR "#4"), .block_no = 0, .mipmap = 0},
        {.name = nullptr,                                                             .block_no = 0, .mipmap = 0},
        {.name = nullptr,                                                             .block_no = 0, .mipmap = 0},
    };
    textures[0].block_no = texture_block;
    textures[1].block_no = texture_block;
    BG_READ_INFO *file = GetReadBGFile(CharaFileBGReadNo);
    auto         *pack = reinterpret_cast<u_int *>(file->buffer);
    textures[1].name = reinterpret_cast<char *>(GetPackFile(pack, image_name.data(), &size));
    TexManager.DeleteTextureBlock(texture_block);
    TexManager.CleanUpTextureList();
    TexManager.LoadTextureBlockEX(-1, textures);
    // The model is built in the space straight after the read pack file.
    auto       *model = reinterpret_cast<u_char *>(file->buffer + (file->size >> 4) + 1);
    std::string config_name = std::format("c0{}dmenu.cfg", chara + 1);
    MenuCharaFrame.Initialize();
    MenuExCashBuffer.base = model;
    MenuExCashBuffer.limit = 0xDC00;
    MenuExCashBuffer.used = 0;
    MenuCharaFrame.LoadPackData(pack, config_name.data(), &MenuExCashBuffer, &MenuExCashBuffer, nullptr);
    CFrameAttr attr;
    attr.fog_enable = true;
    attr.clip_enable = false;
    attr.program_option = 0;

    if (MenuCharaFrame.frame != nullptr) {
        MenuCharaFrame.frame->SetAttr(attr, 1, 4);
    }

    sceVu0FVECTOR position[6] = {
        {-4.8f, -13.0f, 0.0f, 1.0f},
        {-5.2f, -12.8f, 0.0f, 1.0f},
        {-5.0f, -11.4f, 0.0f, 1.0f},
        {-5.0f, -15.0f, 0.0f, 1.0f},
        {-5.0f, -16.0f, 0.0f, 1.0f},
        {-5.0f, -15.0f, 0.0f, 1.0f}
    };
    float scale[6][3] = {
        {1.0f, 1.0f, 1.0f},
        {1.0f, 1.0f, 1.0f},
        {1.0f, 1.0f, 1.0f},
        {1.0f, 1.0f, 1.0f},
        {0.9f, 0.9f, 0.9f},
        {0.9f, 0.9f, 0.9f}
    };
    MenuCharaFrame.SetPosition(position[chara]);
    SetNowCharaMotionNo(chara);
    MenuCharaFrame.SetScale(scale[chara]);
    MenuCharaFrame.Step();

    if (chara == 0) {
        MenuCharaFrame.ClothStep(-1);
        MenuCharaFrame.ClothStep(0);
    }
}

namespace {

std::string GetCharaChangeReadCharaFilePath(int chara_no) {
    return std::format("{}{}{}", readFilePath, charaFile[chara_no], ".chr");
}

/**
 * Returns the path of one of a character's menu weapon model files.
 */
std::string MenuWeaponFilePath(int chara, int weapon_index) {
    std::array<char, 64> name{};
    std::array<char, 16> cfg{};
    BtGetWeaponNamePath2(name.data(), cfg.data(), chara, weapon_index);
    return std::format("{}{}", MenuWepDir, name.data());
}

} // namespace

int CharaChangeInitToGL(u_long128 *buffer, int chara) {
    int size = 0;

    CharaChangeBaseBuf = buffer;
    charachangeid = chara;
    std::string chara_path = GetCharaChangeReadCharaFilePath(chara);
    CharaChangeBaseBuf = MenuCalcBufAlignment(CharaChangeBaseBuf);
    menucharReadbuf = CharaChangeBaseBuf;
    StartReadBG();
    LoadFileBG(chara_path.data(), menucharReadbuf, &size);
    menud0wepReadBuf = menucharReadbuf + (size >> 4) + 1;
    menud0wepReadBuf = MenuCalcBufAlignment(menud0wepReadBuf);
    std::string path = MenuWeaponFilePath(chara, 0);
    LoadFileBG(path.data(), menud0wepReadBuf, &size);
    menud1wepReadBuf = menud0wepReadBuf + (size >> 4) + 1;
    menud1wepReadBuf = MenuCalcBufAlignment(menud1wepReadBuf);
    path = MenuWeaponFilePath(chara, 1);
    LoadFileBG(path.data(), menud1wepReadBuf, &size);
    menud2wepReadBuf = menud1wepReadBuf + (size >> 4) + 1;
    menud2wepReadBuf = MenuCalcBufAlignment(menud2wepReadBuf);
    WEAPON_HAVE *equipped = &UserStatus->chara_weapons[chara][UserStatus->equipped_weapon_slot[chara]];
    int          kind = equipped->item_no - defWeapon[chara];

    if (kind < 0) {
        kind = 0;
    }

    path = MenuWeaponFilePath(chara, kind);
    LoadFileBG(path.data(), menud2wepReadBuf, &size);
    WepEffectMenuReadBuf = menud2wepReadBuf + (size >> 4) + 1;
    WepEffectMenuReadBuf = MenuCalcBufAlignment(WepEffectMenuReadBuf);

    if (UserStatus == nullptr) {
        printf("USerStatus is NULL\n");
        return -1;
    }

    int          slot = UserStatus->equipped_weapon_slot[charachangeid];
    WEAPON_HAVE *weapons = UserStatus->chara_weapons[charachangeid];
    WEAPON_HAVE *weapon = &weapons[slot];
    WepEffectMenuPt = Get_Main_EffectPtr(charachangeid, weapon->best_elem);
    path = WeaponEffectPath(WepEffectMenuPt);
    LoadFileBG(path.data(), WepEffectMenuReadBuf, &size);
    return 1;
}

void CharaChangeInitToGL2(int load_icon) {
    int size = 0;

    MenuWepIconCharaChangePtr = CharaChangeBaseBuf;

    if (load_icon != 0) {
        MenuWepIconCharaChangePtr = MenuCalcBufAlignment(MenuWepIconCharaChangePtr);
        size = LoadFileBGMenuData("wepicon.img", MenuWepIconCharaChangePtr);
        MenuWepIconCharaChangePtr = MenuWepIconCharaChangePtr + (size >> 4) + 1;
    }

    MenuVoiceLoadPtr = MenuWepIconCharaChangePtr;
    MenuVoiceLoadPtr = MenuCalcBufAlignment(MenuVoiceLoadPtr);

    if (SndVoiceLoadBG(charachangeid, reinterpret_cast<u_int *>(MenuVoiceLoadPtr), &size) == 0) {
        printf("*** voice read err \n");
    }
}

void BtMenuLoadChara() {
    s8           chara = charachangeid;
    CUserStatus *status = UserStatus;
    status->cur_chara = chara;
    LoadChara2(charachangeid, 0, reinterpret_cast<u_int *>(menucharReadbuf), reinterpret_cast<u_int *>(menud0wepReadBuf), reinterpret_cast<u_int *>(menud1wepReadBuf), reinterpret_cast<u_int *>(menud2wepReadBuf));
    SetWeaponAttachStatus(NowWeaponHave);
    CWeaponFx.InitSet(NowWeapon->frame, "dcol0", "dcol1");
    SetWeaponColor();
    MainChara_Effect(WepEffectMenuPt, reinterpret_cast<u_int *>(WepEffectMenuReadBuf), 0);
}

void BtMenuLoad2(int load_texture) {
    if (load_texture != 0) {
        BG_READ_INFO     *file = GetReadBGFile(0);
        LOADTEXTURE_INFO2 textures[] = {
            {.name = const_cast<char *>("#frame_image#640#" SCREEN_HEIGHT_STR "#4"), .block_no = MenuShadowReadBlock, .mipmap = 0},
            {.name = reinterpret_cast<char *>(file->buffer),                         .block_no = MenuShadowReadBlock, .mipmap = 0},
            {.name = nullptr,                                                        .block_no = 0,                   .mipmap = 0},
        };
        TexManager.DeleteTextureBlock(MenuShadowReadBlock);
        TexManager.LoadTextureBlockEX(-1, textures);
        MenuTextureReload(MenuShadowReadBlock);
        DngActiveWeaponTextureCopy();
    }

    SndVoiceSyncBG();
    BtActStatus.player_visible = 1;
    BtActStatus.motion_no = 0;
    BtActStatus.action_on = 0;
}

int EastKingCheckComplete() {
    if (SaveData == nullptr) {
        return 0;
    }

    int complete = 1;

    for (int i = 0; i < 12; i++) {
        if (SaveData->GetGameFlag(i + 0xE6) == 0) {
            complete = 0;
        }
    }

    return complete;
}

void SetMonsterNameDrawFlag(int flag) {
    CharaNameDrawFlag = flag;
}

int GetMonsterNameDrawFlag() {
    return CharaNameDrawFlag;
}

void MonsterNameInit(ClsMes *mes, short *message_buffer, unsigned char *texture_buffer) {
    CharaNameMes = nullptr;

    if (mes == nullptr) {
        printf("cls init failed\n");
        return;
    }

    CharaNameMes = mes;
    mes->text_columns = MES_WIN_COLUMNS;
    mes->text_rows = 10;
    mes->text_len = 0;
    mes->text_width = 0;
    mes->text_height = 0;
    mes->fade = 0.0f;
    mes->fade_in = true;
    mes->text_rate = mes->text_rate_set;
    mes->waiting = false;
    mes->text_at = 0.0f;
    mes->text_no = 0;
    mes->text_from = 0;
    mes->page_from = 0;
    mes->InitMesWinTbl();
    mes->clut_now = mes->clut_default;
    mes->wait = 0;
    mes->blink = 0;
    mes->auto_page_wait = 0;
    mes->mes_made = -1;
    mes->edge_alpha = 0x80;

    for (auto &mes_no : mes->mes_no) {
        mes_no = -1;
    }

    for (auto &value : mes->values) {
        value = 0;
    }

    mes->value = 0;
    mes->value_signed = false;
    mes->value_show = true;
    mes->value_narrow = false;
    mes->space_width = -1;
    mes->space_area = -1;
    mes->cursor_row = -1;
    mes->cursor_y = 0;
    mes->cursor_lit = false;

    for (auto &line_pos : mes->line_pos) {
        line_pos.x = -1;
        line_pos.y = -1;
    }

    CharaNameMes->Preset(MES_PRESET_NAME);
    CharaNameMes->tex_block = 0x1A;
    CharaNameMes->tex_buff = texture_buffer;
    CharaNameMes->rows = 1;
    CharaNameMes->SetBuff(message_buffer);
    CharaNameMes->SetBuff_system(SystemMes);
    CharaNameDrawFlag = 1;
    int *config = static_cast<int *>(SaveData->GetConfigData());

    if (config != nullptr) {
        CharaNameDrawFlag = !config[8];
    }

    CharaNameDrawCase = 0;
}

void MonsterNameMake(int mes_no) {
    if (CharaNameMes != nullptr) {
        int mes = mes_no + 3000;

        if (CharaNameMes->mes_made != mes) {
            CharaNameMes->MakeMesWin(mes);

            CharaNameMes->stay_frame = mes != 3000;
        }
    }
}

void MonsterNamePosSet(int x, int y) {
    if (CharaNameMes != nullptr) {
        int columns = CharaNameMes->text_columns;
        CharaNameMes->text_x = x - columns * 14 / 2;

        if (y % 2 != 0) {
            y++;
        }

        CharaNameMes->text_y = y;
    }
}

void MonsterNameDraw() {
    if (CharaNameMes == nullptr || GetMonsterNameDrawFlag() == 0) {
        return;
    }

    if (static_cast<int *>(SaveData->GetConfigData())[8] == 0) {
        ClsMes *mes = CharaNameMes;

        if (mes->mes_made >= 0) {
            int width = mes->text_columns;
            width = mes->char_width * width + 0x20;

            if (mes->text_x < 0x22 || mes->text_x >= 0x26D || mes->text_y < 0x1E || mes->text_y >= 0x199 || width >= 0xFB || width < 10) {
                SetMonsterNameDrawFlag(0);
                return;
            }

            mes->cursor_row = -1;
            MenuTextureReload(CharaNameMes->tex_block);
            setbilinear(0);
            CharaNameMes->Step();
            CharaNameMes->DrawMesWin();
        }
    }
}

void DngEscapeMsgInit(ClsMes *title, ClsMes *choice, int dungeon) {
    int file_size = 0;

    if (title == nullptr || choice == nullptr) {
        return;
    }

    if (UserStatus == nullptr) {
        return;
    }

    int         georama_no = UserStatus->cur_georama + 1;
    std::string path = std::format("{}d0{}o.img", GetMenuTextureDir(), georama_no);
    LoadFile(path.data(), read_buffer, &file_size);
    int               block = 0x17;
    char             *image = reinterpret_cast<char *>(read_buffer);
    LOADTEXTURE_INFO2 texture[3] = {
        {.name = "#frame_image#640#480#4", .block_no = block, .mipmap = 0},
        {.name = image,                    .block_no = block, .mipmap = 0},
        {.name = nullptr,                  .block_no = 0,     .mipmap = 0},
    };
    std::array<int, 4> size{};
    TexManager.DeleteTextureBlock(block);
    TexManager.CleanUpTextureList();
    TexManager.LoadTextureBlockEX(-1, texture);
    GamePad.MenuModeOn(0x78);
    GamePad.SetAutoRepeat(PAD_DPAD, 0x1E, 5);
    std::string name = std::format("d0{}o", georama_no);
    DngEscapeTex = TexManager.GetTexture(name.data(), -1);
    CharaNameMes = title;
    DngMenuMes = choice;
    int    lang = GetMenuLangFlag();
    u_char title_x[7] = {0xB4, 0xA0, 0xA0, 0xA0, 0xA0, 0xA0, 0xA0};
    CharaNameMes->text_x = title_x[lang];
    CharaNameMes->text_y = 0x8C;
    CharaNameMes->mes_made = -1;
    CharaNameMes->MakeMesWin(dungeon + 0x14);
    int right = 0x280;
    DngMenuMes->mes_made = -1;
    DngMenuMes->MakeMesWin(12);
    DngMenuMes->NeedMesWinWH(DngMenuMes->mes_made, size.data());
    right -= size[2];
    DngMenuMes->text_x = right - 0x26;
    DngMenuMes->text_y = 0x140;
    DngEscapeSelect = 1;
    DngEscapeEndFlag = 0;
    DngEscapeAlpha = 0x80;
    DngEscapeBlock = block;
}

void DngEscapeMsgDraw() {
    AllFadeForMenu(0x80);

    if (CharaNameMes == nullptr || DngMenuMes == nullptr) {
        return;
    }

    MenuTextureReload(DngEscapeBlock);
    DrawFullSizePicture(DngEscapeTex, 0, 0, 0x80);
    MenuTextureReload(CharaNameMes->tex_block);
    setbilinear(0);
    CharaNameMes->stay_frame = true;
    CharaNameMes->Step();
    DngMenuMes->Step();
    CharaNameMes->DrawMesWin();
    DngMenuMes->DrawMesWin();

    if (DngEscapeEndFlag != 0) {
        DngEscapeAlpha += 3;

        if (DngEscapeAlpha > 0x80) {
            DngEscapeAlpha = 0x80;
        }
    } else {
        DngEscapeAlpha -= 3;

        if (DngEscapeAlpha < 0) {
            DngEscapeAlpha = 0;
        }
    }

    AllFadeForMenu(DngEscapeAlpha);
}

int DngEscapeMsgLoop() {
    int result = 0;

    if (DngEscapeEndFlag == 0) {
        if (GamePad.Down(PAD_UP | PAD_DOWN) != 0) {
            if (DngEscapeSelect == 2) {
                DngEscapeSelect = 1;
            } else if (DngEscapeSelect == 1) {
                DngEscapeSelect = 2;
            }

            ComMenuSePlay(MENU_SOUND_CURSOR);
        }

        int     row = DngEscapeSelect - 1;
        ClsMes *mes = CharaNameMes;
        mes->cursor_row = row;

        if (GamePad.Down(PAD_CROSS) != 0) {
            DngEscapeEndFlag = 1;
            ComMenuSePlay(MENU_SOUND_CONFIRM);
        }

        if (GamePad.Down(PAD_CIRCLE) != 0) {
            ComMenuSePlay(MENU_SOUND_REFUSE);
        }
    } else if (DngEscapeAlpha >= 0x80) {
        result = DngEscapeSelect;
        GamePad.AutoRepeatOff();
        GamePad.MenuModeOff();
        ItemVolumeStep.CheckItemVolume();
        ClsMes *mes = CharaNameMes;
        mes->cursor_row = -1;
    }

    ItemVolumeStep.LoopStep(0x3C);
    DngEscapeMsgDraw();
    return result;
}

int CheckItemThrow(int *items, int *values) {
    int        found = 0;
    ITEM_PACK *pack = &BtlMenuStatusPt->item_pack;

    for (int i = 0; i < 3; i++) {
        int item_no = pack->item[pack->num + i];

        if (item_no >= ITEM_DUNGEON_START) {
            found++;

            if (items != nullptr) {
                items[i] = item_no;
            }

            if (values != nullptr) {
                values[i] = 0;
            }
        }
    }

    for (WEAPON_HAVE *weapons : BtlMenuStatusPt->chara_weapons) {
        int item_no = weapons[10].item_no;

        if (item_no >= ITEM_WEAPON_START) {
            found++;

            if (items != nullptr) {
                items[3] = item_no;
            }

            if (values != nullptr) {
                values[3] = weapons[10].level;
            }

            break;
        }
    }

    auto *attachments = reinterpret_cast<ATTACH_LIST *>(BtlMenuStatusPt->consumable_items);

    for (int i = 0; i < 3; i++) {
        int item_no = attachments[i + 40].item_no;

        if (item_no >= ITEM_ATTACH_START) {
            found++;

            if (items != nullptr) {
                items[i + 4] = item_no;
            }

            if (values != nullptr && item_no >= ITEM_ATTACH_STAT_START && item_no < ITEM_ATTACH_GEM_START) {
                values[i + 4] = (&attachments[i + 40].item_no)[item_no - ITEM_ATTACH_STAT_START + 4];
            }

            if (values != nullptr && item_no == ITEM_ATTACH_SYNTHESIS_SPHERE) {
                values[i + 4] = attachments[i + 40].sphere_weapon_no;
            }
        }
    }

    return found > 0 ? 1 : 0;
}

void SetWeaponElementStatus(WEAPON_HAVE *weapon) {
    int best = 0;

    for (int i = 1; i < 5; i++) {
        if (weapon->elem[i] > weapon->elem[best]) {
            best = i;
        }
    }

    weapon->best_elem = best;
}

int CheckWeaponOptionStatus(int options) {
    // Each pair of opposed options cancels out when both are set.
    if ((options & 2) && (options & 4)) {
        options &= ~6;
    }

    if ((options & 8) && (options & 0x10)) {
        options &= ~0x18;
    }

    if ((options & 0x100) && (options & 0x200)) {
        options &= ~0x300;
    }

    if ((options & 0x400) && (options & 0x800)) {
        options &= ~0xC00;
    }

    return options;
}

int IsWeaponOptionGoodOrBad(int option) {
    // Whether each of the fourteen weapon options is a benefit (1) or a drawback (0).
    s16 good_or_bad[14] = {0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 1, 1, 1, 1};
    return good_or_bad[option];
}

int DefaultWeaponOptionSet(int weapon_no) {
    WEAPON_DATA *data = GetWeaponData(weapon_no);

    if (data == nullptr) {
        return 1;
    }

    return data->flags;
}

void WeaponOptionStatusDraw(WEAPON_HAVE *weapon, int x, int y, int alpha) {
    int draw_x = x;
    int draw_y = y;
    int flags = weapon->flags;

    for (const ATTACH_LIST &attachment : weapon->attach) {
        int option = attachment.sphere_flags;

        if (option != 0 && option != 1) {
            flags |= option;
        }
    }

    flags = CheckWeaponOptionStatus(flags);
    CTexture *texture = TexManager.GetTexture("charaface", -1);

    if (texture == nullptr) {
        return;
    }

    int      count = 0;
    CRect_i_ source(0xEC, 0x50, 0x14, 0x14);

    for (int bit = 1; bit <= 13; bit++) {
        if (bit == 8) {
            source.x -= source.width;
        }

        if (flags & (1 << bit)) {
            int row = bit;

            if (bit > 7) {
                row = bit - 7;
            }

            if (bit == 8 && row == 1) {
                row++;
            }

            if (bit == 9 && row == 2) {
                row--;
            }

            source.y = source.height * (row - 1) + 0x50;
            DrawMenu2DSprite(texture, CRect_i_(draw_x, draw_y, 0x14, 0x15), source, alpha);
            draw_x += source.width;
            count++;

            if (count == 5) {
                draw_x = x;
                draw_y += source.height;
            }
        }
    }
}

void WeaponStarDraw(int x, int y, WEAPON_HAVE *weapon, int alpha) {
    if (weapon == nullptr) {
        return;
    }

    CTexture *texture = TexManager.GetTexture("wepstatus", -1);
    int       stars = weapon->synthesis_count;

    if (stars > 49) {
        stars = 49;
    }

    int      draw_x = x + 0x4C;
    CRect_i_ big_star(0x108, 0x176, 0x18, 0x18);

    for (int i = 0; i < stars / 10; i++, draw_x += big_star.width) {
        DrawMenu2DSprite(texture, CRect_i_(draw_x, y, big_star.width, big_star.height), big_star, alpha);
    }

    x -= 6;

    for (int i = 0; i < stars % 10; i++, x += 0x14) {
        DrawMenu2DSprite(texture, CRect_i_(x, y + 0x16, 0x14, 0x14), CRect_i_(0x10A, 0x162, 0x14, 0x14), alpha);
    }
}

namespace {

/**
 * Lowers one randomly chosen value of a weapon's status array, not below zero.
 */
void LocalWeaponDataChange(char *values, int count, int base, int range) {
    if (values == nullptr) {
        printf("target is NULL\n");
        return;
    }

    int   selected = rand() % count;
    int   reduction = base + rand() % range;
    char &value = values[selected];
    value -= reduction;

    if (value < 0) {
        value = 0;
    }
}

} // namespace

int WeaponDataChangeByRGate(WEAPON_HAVE *weapon, int kind) {
    if (weapon == nullptr) {
        return -1;
    }

    int is_default = 0;

    for (int default_weapon : defWeapon) {
        if (weapon->item_no == default_weapon) {
            is_default = 1;
        }
    }

    switch (kind) {
        case RGATE_ABS_FULL:
            printf("abs full\n");

            if (is_default == 1) {
                printf("this weapon default\n");
            } else {
                weapon->experience = GetWeaponMaxExp(weapon);
            }

            break;
        case RGATE_STAT_DOWN:
            if (IsDefaultWeapon(weapon->item_no) >= 0) {
                printf("default Weapon\n");
            } else {
                int stat = rand() % 4;
                int loss = rand() % 3 + 2;

                switch (stat) {
                    case 0:
                        weapon->attack -= loss;

                        if (weapon->attack <= 0) {
                            weapon->attack = 1;
                        }

                        break;
                    case 1:
                        weapon->endurance -= loss;

                        if (weapon->endurance < 0) {
                            weapon->endurance = 0;
                        }

                        break;
                    case 2:
                        weapon->speed -= loss;

                        if (weapon->speed < 0) {
                            weapon->speed = 0;
                        }

                        break;
                    case 3:
                        weapon->magic -= loss;

                        if (weapon->magic < 0) {
                            weapon->magic = 0;
                        }

                        break;
                    default:
                        break;
                }

                LocalWeaponDataChange(weapon->elem, 5, 2, 3);
                LocalWeaponDataChange(weapon->vs_monster, 3, 2, 2);
                LocalWeaponDataChange(&weapon->vs_monster[3], 3, 2, 2);
                LocalWeaponDataChange(&weapon->vs_monster[6], 4, 2, 2);
            }

            break;
        case RGATE_WHP_UP:
            if (IsDefaultWeapon(weapon->item_no) >= 0) {
                printf("this weapon No.%d is default\n", weapon->item_no);
            } else {
                int add = rand() % 3 + 3;
                weapon->durability += add;

                if (weapon->durability > 99) {
                    weapon->durability = 99;
                }

                printf("WHp up is %d\n", add);
                printf("now MaxWHp is %d\n", weapon->durability);
            }

            break;
        case RGATE_WHP_DOWN:
            if (IsDefaultWeapon(weapon->item_no) < 0) {
                int loss = rand() % 3 + 3;
                weapon->durability -= loss;
                float durability = weapon->durability;

                if (durability < weapon->durability_f) {
                    weapon->durability_f = durability;
                }

                if (weapon->durability <= 0) {
                    weapon->durability = 1;
                }

                printf("WHp down is %d\n", loss);
                printf("now MaxWHp is %d\n", weapon->durability);
            }

            break;
        case RGATE_WHP_CURE:
            printf("whp cure\n");

            if (is_default == 1) {
                if (UserStatus != nullptr) {
                    int chara = UserStatus->cur_chara;
                    EquipWeaponFrame(&DefaultWeapon, chara, CharaMainHandViewFlag);

                    if (chara == 5) {
                        NowMainEffect = &CharaMainEffectCrash;
                        BtActStatus.gun_type = 0;
                    }

                    int          slot = UserStatus->equipped_weapon_slot[chara];
                    WEAPON_HAVE *weapons = UserStatus->chara_weapons[chara];
                    MenuWeaponSpSet(&MainWeapon, &weapons[slot]);
                    printf("equip default Weapon\n");
                }
            } else {
                weapon->durability_f = weapon->durability;
            }

            break;
        case RGATE_WHP_QUARTER: {
            float durability = weapon->durability_f / 4.0f;
            weapon->durability_f = durability;

            if (durability < 1.0f) {
                weapon->durability_f = 1.0f;
            }

            break;
        }
        default:
            printf("now %d  ??? \n", kind);
            break;
    }

    return 1;
}

} // namespace dc::game
