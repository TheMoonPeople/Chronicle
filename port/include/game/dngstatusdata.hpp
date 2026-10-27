#pragma once

#include "userstatus.hpp"

namespace dc::game {

/**
 * Extends the party status with dungeon-specific save data.
 */
class CDngStatusData : public CUserStatus {
public:
    char unk_8B20[0x17C];

    /**
     * Returns the number of active party members.
     */
    s8 GetPartySize() const { return party_size; }

    /**
     * Sets the number of active party members.
     */
    void SetPartySize(s8 size) { party_size = size; }

    /**
     * Moves the party to a floor, remembering the previous floor and the deepest one reached.
     */
    void SetNowFloor(int floor);

    /**
     * Returns the slot holding an item among the quick slots, pack, attachments or party weapons, or -1.
     */
    int SearchItemIndexNo(int item_id);

    /**
     * Empties the first pack slot holding an item and returns that slot, or -1 if none holds it.
     */
    int LostItem(int item_id);

    /**
     * Removes every gate key and floor key item from the pack.
     */
    void LostGateKey();

    /**
     * Returns how many party members have life left.
     */
    int GetLiveUnit();

    /**
     * Returns 1 if the pack is full, 2 if the attachments are full, or 0 if the item can be picked up.
     */
    int CheckItemGet(int item_id);

    /**
     * Returns the character a weapon belongs to, judged by its item id, or -1.
     */
    int CheckWeaponUser(int weapon_id);

    /**
     * Returns how many weapons the owner of a weapon carries, or -1 if the item is not a weapon.
     */
    int CheckWeaponRot(int weapon_id);

    /**
     * Forgets how deep the party has reached in the Demon Shaft.
     */
    void ClearDeamonShaft();

    /**
     * Puts an item in the pack, the attachments or its owner's weapons and returns its slot, or -1.
     */
    int GetItem(int item_id, int qty);

    /**
     * Returns the quick slot holding an item, or -1.
     */
    int CheckActItemSlot(int item_id);

    /**
     * Returns 0 if a character has their starting weapon equipped, 1 otherwise.
     */
    int CheckDefaultWeapon(int chara_no);

    /**
     * Halves the Gilda carried by the party after a death.
     */
    void SetDead();

    /**
     * Fences the party into the current floor's restriction zone, if the floor has one.
     */
    void SetResLimmitZone();

    /**
     * Places thirst and fixed-weapon restriction zones on random floors of dungeons one to five.
     */
    void InitResLimmitZone();

    /**
     * Resets the party, inventory and dungeon progress to the state of a new game.
     */
    void Initialize();

    /**
     * Counts one more monster defeated on the current floor.
     */
    void AddKills();

    /**
     * Returns how many monsters have been defeated on a dungeon floor.
     */
    s16 ChkKills(int georama_no, int floor);

    /**
     * Returns how many atla slots of a dungeon floor have been collected.
     */
    int GetAtraNum(int georama_no, int floor);

    /**
     * Returns how many atla slots a dungeon floor has.
     */
    int GetMaxAtraNum(int georama_no, int floor);

    /**
     * Puts an atla in a dungeon floor's first free slot and returns that slot, or -1 if none is free.
     */
    int SetGetAtra(int georama_no, int floor, int atra_id);

    /**
     * Copies a dungeon floor's eight atla slots into out_list.
     */
    void SetCopyAtraList(int georama_no, int floor, int *out_list);

    /**
     * Marks the floor slot asking for an atla, or failing that one asking for any atla, collected.
     */
    void GetAtraData(int georama_no, int floor, int atra_id);

    /**
     * Returns one character's active battle-menu status value.
     */
    s32 GetActiveCharaStatus(int chara_no) { return ailments[chara_no]; }

    /**
     * Returns the weapon slot one character has equipped.
     */
    s8 GetEquipWeaponSlot(int chara_no) { return equipped_weapon_slot[chara_no]; }
};

static_assert(sizeof(CDngStatusData) == 0x8C9C);

} // namespace dc::game
