#include "hitvalue.hpp"

#include <libvu0.h>

#include <cmath>

#include "dngmessageman.hpp"
#include "dun/gameloop.hpp"
#include "frame.hpp"
#include "itemdata.hpp"
#include "menu_dungeon.hpp"
#include "menu_inventory.hpp"
#include "mglib.hpp"
#include "rect.hpp"
#include "savedata.hpp"
#include "snd.hpp"
#include "texture.hpp"
#include "userstatus.hpp"

#include "platform/config.hpp"

// Preserve retail wear, repair feedback and weapon-break behavior; optionally search the bag.
PORT_OVERRIDE int BattleSubWeaponDmg(float amount, int kind) {
    int          default_weapons[6] = {ITEM_WEAPON_DAGGER_BROKEN, ITEM_WEAPON_WOODENSLINGSHOT_BROKEN, ITEM_WEAPON_MALLET_BROKEN, ITEM_WEAPON_GOLD_RING_BROKEN, ITEM_WEAPON_FIGHTING_STICK_BROKEN, ITEM_WEAPON_MACHINE_GUN_BROKEN};
    int          chara_no = UserStatus->cur_chara;
    int          default_weapon;
    WEAPON_HAVE *weapon = &UserStatus->chara_weapons[chara_no][UserStatus->equipped_weapon_slot[chara_no]];

    if (weapon->item_no == ITEM_WEAPON_SERPENT_SWORD && SaveData->GetGameFlag(0x30) == 0) {
        return 0;
    }

    if (weapon->flags & 0x200) {
        amount *= 0.5f;
    }

    if (weapon->flags & 0x100) {
        amount *= 2.0f;
    }

    default_weapon = default_weapons[chara_no];

    if (default_weapon == UserStatus->chara_weapons[chara_no][UserStatus->equipped_weapon_slot[chara_no]].item_no) {
        return 0;
    }

    float wear = 1.5f - 0.01f * NowWeaponHave->endurance;
    float old_durability = weapon->durability_f;
    wear *= amount;
    wear += 0.1f * kind;
    weapon->durability_f = old_durability - wear;

    if (weapon->durability_f <= 0.0f) {
        int powder_slot = ((CDngStatusData *) UserStatus)->CheckActItemSlot(ITEM_AUTO_REPAIR_POWDER);

        bool repaired = false;
        if (powder_slot != -1) {
            DelActiveItem(powder_slot + 1);
            repaired = true;
        } else if (ConfigGet().inventory_auto_repair) {
            // Keep retail's quick-slot priority and model cleanup. Only the carried bag is added.
            ITEM_PACK &pack = UserStatus->item_pack;
            for (int slot = 0; slot < pack.num; ++slot) {
                if (pack.item[slot] == ITEM_AUTO_REPAIR_POWDER) {
                    DeleteItemAfterUseItem(ITEM_AUTO_REPAIR_POWDER, &pack);
                    repaired = true;
                    break;
                }
            }
        }

        if (repaired) {
            weapon->durability_f = weapon->durability;
            DngMessMan.message = 0xBC;
            DngMessMan.timer = 0xF0;
            DngMessMan.steev_window = false;
            SndSePlay(SE_WEAPON_REPAIR, -1, 0);
        }
    }

    if (weapon->durability_f <= 0.0f) {
        weapon->durability_f = 0.0f;
        SndSePlay(SE_WEAPON_BREAK, -1, 0);

        int now_item = UserStatus->chara_weapons[chara_no][UserStatus->equipped_weapon_slot[chara_no]].item_no;

        if (default_weapon + 1 == now_item) {
            DngMessMan.insert_mes_1 = GetCommonItemDataSystemMsg(weapon->item_no);
            DngMessMan.insert_value_1 = weapon->level;
            DngMessMan.message = 0xA1;
            DngMessMan.timer = 0x1E0;
            DngMessMan.steev_window = false;
            WepDataListToHaveCopy(default_weapon, weapon);
            SetWeaponAttachStatus(NowWeaponHave);
            NowWeaponHave->durability_f = 0.0f;
            weapon->durability_f = 0.0f;
            return 1;
        }

        if (default_weapon + 1 != (s16) now_item) {
            WEAPON_HAVE *replacement = UserStatus->chara_weapons[chara_no];

            for (int slot = 0; slot < 10; replacement++, slot++) {
                if (replacement->item_no == default_weapon) {
                    UserStatus->equipped_weapon_slot[chara_no] = slot;
                    SetWeaponAttachStatus(NowWeaponHave);
                    DngMessMan.insert_mes_1 = GetCommonItemDataSystemMsg(weapon->item_no);
                    DngMessMan.insert_value_1 = weapon->level;
                    DngMessMan.insert_mes_2 = GetCommonItemDataSystemMsg(replacement->item_no);
                    DngMessMan.insert_value_2 = replacement->level;
                    DngMessMan.message = 0xA0;
                    DngMessMan.timer = 0x1E0;
                    DngMessMan.steev_window = false;
                    weapon->item_no = -1;
                    return 1;
                }

                if (replacement->item_no == default_weapon + 1) {
                    UserStatus->equipped_weapon_slot[chara_no] = slot;
                    SetWeaponAttachStatus(NowWeaponHave);
                    DngMessMan.insert_mes_1 = GetCommonItemDataSystemMsg(weapon->item_no);
                    DngMessMan.insert_value_1 = weapon->level;
                    DngMessMan.insert_mes_2 = GetCommonItemDataSystemMsg(replacement->item_no);
                    DngMessMan.insert_value_2 = replacement->level;
                    DngMessMan.message = 0xA0;
                    DngMessMan.timer = 0x1E0;
                    DngMessMan.steev_window = false;

                    if (chara_no == 3) {
                        replacement->best_elem = 0;
                    }

                    weapon->item_no = -1;
                    return 2;
                }
            }
        }
    }

    if (0.1f * weapon->durability <= old_durability && 0.1f * weapon->durability > weapon->durability_f) {
        DngMessMan.insert_mes_1 = GetCommonItemDataSystemMsg(weapon->item_no);
        DngMessMan.insert_value_1 = weapon->level;
        DngMessMan.message = 0x97;
        DngMessMan.timer = 0xF0;
        DngMessMan.steev_window = false;
    }

    if (0.05f * weapon->durability <= old_durability && 0.05f * weapon->durability > weapon->durability_f) {
        DngMessMan.insert_mes_1 = GetCommonItemDataSystemMsg(weapon->item_no);
        DngMessMan.insert_value_1 = weapon->level;
        DngMessMan.message = 0x98;
        DngMessMan.timer = 0xF0;
        DngMessMan.steev_window = false;
    }

    return 0;
}
