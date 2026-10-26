#pragma once

#include "common.h"

#include "character.hpp"
#include "itemdata.hpp"

/**
 * Weapon effects the menus play, as CWeaponLevelUp::operation_kind holds them.
 */
// clang-format off
enum WepEffectKind {
    WEP_EFFECT_NONE                = -1, /**< None. */
    WEP_EFFECT_LEVELUP             = 0,  /**< Level-up. */
    WEP_EFFECT_STATUS_BREAK        = 1,  /**< Status break. */
    WEP_EFFECT_BUILDUP             = 2,  /**< Build-up. */
    WEP_EFFECT_RECOVER             = 3,  /**< Repair. */
    WEP_EFFECT_CURE_HEAL_HP        = 4,  /**< Item that restores hit points. */
    WEP_EFFECT_CURE_DRINK          = 5,  /**< Drink. */
    WEP_EFFECT_CURE_REPAIR         = 6,  /**< Repair Powder. */
    WEP_EFFECT_UNK_7               = 7,  /**< Item effect with no caller. */
    WEP_EFFECT_CURE_STATUS         = 8,  /**< Item that grants a status effect. */
    WEP_EFFECT_CURE_AILMENT        = 9,  /**< Item that cures an ailment. */
    WEP_EFFECT_CURE_REVIVAL_POWDER = 10, /**< Revival Powder. */
    WEP_EFFECT_CURE_GOURD          = 11, /**< Gourd. */
    WEP_EFFECT_CURE_FRUIT_OF_EDEN  = 12, /**< Fruit of Eden. */
    WEP_EFFECT_CURE_POCKET         = 13, /**< Pocket. */
    WEP_EFFECT_KIND_COUNT          = 14, /**< Number of effects. */
};

// clang-format on

/**
 * Steps of a weapon effect, as CWeaponLevelUp::effect_state holds them; each item effect's load step is followed by its play step.
 */
// clang-format off
enum WepEffectState {
    WEP_EFFECT_STATE_IDLE                     = 0,  /**< Idle. */
    WEP_EFFECT_STATE_LEVELUP_LOAD             = 1,  /**< Loading the level-up effect. */
    WEP_EFFECT_STATE_LEVELUP_ORBIT            = 2,  /**< Level-up effect orbiting. */
    WEP_EFFECT_STATE_LEVELUP_DONE             = 3,  /**< Level-up effect finished. */
    WEP_EFFECT_STATE_BREAK_LOAD               = 4,  /**< Loading the status break effect. */
    WEP_EFFECT_STATE_BREAK_PLAY               = 5,  /**< Status break effect playing. */
    WEP_EFFECT_STATE_BREAK_DONE               = 6,  /**< Status break effect finished. */
    WEP_EFFECT_STATE_BUILDUP_LOAD             = 7,  /**< Loading the build-up effect. */
    WEP_EFFECT_STATE_BUILDUP_PLAY             = 8,  /**< Build-up effect playing. */
    WEP_EFFECT_STATE_BUILDUP_DONE             = 9,  /**< Build-up effect finished. */
    WEP_EFFECT_STATE_RECOVER_LOAD             = 10, /**< Loading the repair effect. */
    WEP_EFFECT_STATE_RECOVER_DONE             = 11, /**< Repair effect finished. */
    WEP_EFFECT_STATE_CURE_HEAL_HP_LOAD        = 12, /**< Loading the hit point item effect. */
    WEP_EFFECT_STATE_CURE_HEAL_HP_PLAY        = 13, /**< Hit point item effect playing. */
    WEP_EFFECT_STATE_CURE_DRINK_LOAD          = 14, /**< Loading the drink effect. */
    WEP_EFFECT_STATE_CURE_DRINK_PLAY          = 15, /**< Drink effect playing. */
    WEP_EFFECT_STATE_CURE_REPAIR_LOAD         = 16, /**< Loading the Repair Powder effect. */
    WEP_EFFECT_STATE_CURE_REPAIR_PLAY         = 17, /**< Repair Powder effect playing. */
    WEP_EFFECT_STATE_UNK_12                   = 18, /**< Loading WEP_EFFECT_UNK_7. */
    WEP_EFFECT_STATE_UNK_13                   = 19, /**< Playing WEP_EFFECT_UNK_7. */
    WEP_EFFECT_STATE_CURE_STATUS_LOAD         = 20, /**< Loading the status item effect. */
    WEP_EFFECT_STATE_CURE_STATUS_PLAY         = 21, /**< Status item effect playing. */
    WEP_EFFECT_STATE_CURE_AILMENT_LOAD        = 22, /**< Loading the cure effect. */
    WEP_EFFECT_STATE_CURE_AILMENT_PLAY        = 23, /**< Cure effect playing. */
    WEP_EFFECT_STATE_CURE_REVIVAL_POWDER_LOAD = 24, /**< Loading the Revival Powder effect. */
    WEP_EFFECT_STATE_CURE_REVIVAL_POWDER_PLAY = 25, /**< Revival Powder effect playing. */
    WEP_EFFECT_STATE_CURE_GOURD_LOAD          = 26, /**< Loading the Gourd effect. */
    WEP_EFFECT_STATE_CURE_GOURD_PLAY          = 27, /**< Gourd effect playing. */
    WEP_EFFECT_STATE_CURE_FRUIT_OF_EDEN_LOAD  = 28, /**< Loading the Fruit of Eden effect. */
    WEP_EFFECT_STATE_CURE_FRUIT_OF_EDEN_PLAY  = 29, /**< Fruit of Eden effect playing. */
    WEP_EFFECT_STATE_CURE_POCKET_LOAD         = 30, /**< Loading the Pocket effect. */
    WEP_EFFECT_STATE_CURE_POCKET_PLAY         = 31, /**< Pocket effect playing. */
};

// clang-format on

class CMenuItemStep;

/**
 * Stores the state used by the weapon enhancement effect menu.
 *
 * The first attachment-shaped record is also used as the temporary status
 * break item written into the dungeon consumable inventory.
 */
class CWeaponLevelUp {
public:
    WEAPON_HAVE  preview;             /**< Working copy of the weapon holding the level-up or build-up result the menu shows. */
    s16          status_item_no;      /**< Item id of the synthesis item a status break produces. */
    s16          status_weapon_no;    /**< Item id of the weapon broken down into the synthesis item. */
    s16          status_flags;        /**< Special-behaviour flags carried by the synthesis item. */
    s8           status_weapon_level; /**< Level of the weapon broken down, shown in the break message. */
    u8           unk_ff;
    s16          status_stats[4];    /**< Attack, endurance, speed and magic carried over, at 60% of the weapon's. */
    s8           status_elements[5]; /**< Fire, ice, thunder, wind and holy values carried over, at 60% of the weapon's. */
    s8           status_monster[10]; /**< Monster-effectiveness values carried over, at 60% of the weapon's. */
    u8           unk_117[9];
    CCharacter   effect;               /**< Model whose motions play the menu effect. */
    s32          reserved_word;        /**< Cleared on initialisation and never read. */
    WEAPON_HAVE *weapon;               /**< Weapon the active effect changes. */
    CCharacter  *chara;                /**< Character whose weapon the active effect changes. */
    s16          effect_motion;        /**< Motion number the effect model plays. */
    s16          buildup_weapon_no;    /**< Item id of the weapon a build-up turns the weapon into. */
    s8           buildup_complete;     /**< Set once the build-up has rewritten the weapon. */
    s8           lost_default_weapon;  /**< Set when a status break reset a character's default weapon instead of removing it. */
    s16          message_no;           /**< Character named in a cure message, or a message id of 0x190 and above shown on its own. */
    s32          message_value;        /**< Number shown in a cure message. */
    s16          synthesis_count;      /**< Number of synthesis items the level-up absorbed. */
    s16          reserved_half;        /**< Cleared on initialisation and never read. */
    s16          attachment_icons[5];  /**< Item ids of the attachments orbiting the level-up effect. */
    s16          attachment_values[5]; /**< Values drawn beside the orbiting attachment icons. */
    s16          effect_active;        /**< Nonzero while the effect plays, which holds back the result message. */
    s16          operation_kind;       /**< Active effect. @see WepEffectKind. */
    s16          texture_block;        /**< Texture block the effect's textures load into. */
    u8           unk_1306[2];
    float        effect_x;     /**< Horizontal position a cure effect plays at. */
    float        effect_y;     /**< Vertical position a cure effect plays at. */
    float        effect_timer; /**< Frames since the effect started, negative before it has. */
    s16          effect_state; /**< Step of the effect state machine, starting at a per-operation base. @see WepEffectState. */
    s16          snd_volume;   /**< Background-music volume the fade has reached. */
    s16          snd_from;     /**< Background-music volume the fade started from, restored afterwards. */
    s16          snd_to;       /**< Background-music volume the fade ends at. */
    s16          snd_step;     /**< Signed volume change per frame, zero once the fade ends. */
    u8           unk_131e[0xA];
    u_long128   *effect_buffer; /**< Next free position in the buffer the effect package and sound load into. */
    u8           unk_132c[4];

    /**
     * Loads the package and sound data for one menu effect.
     *
     * @mangled CMenuEffectDataLoad__14CWeaponLevelUpFP1i
     * @address 0x00236000
     * @size 0x1E4
     */
    void CMenuEffectDataLoad(CWeaponLevelUp *load_buffer, int kind);

    /**
     * Clears the menu's tables and runtime state.
     *
     * @mangled Initialize__14CWeaponLevelUpFv
     * @address 0x002361F0
     * @size 0x90
     */
    void Initialize();

    /**
     * Calculates and starts the weapon level-up effect.
     *
     * @mangled SetLevelUpValue__14CWeaponLevelUpFP11WEAPON_HAVEP10CCharacterP1i
     * @address 0x00236280
     * @size 0x4E8
     */
    void SetLevelUpValue(WEAPON_HAVE *have, CCharacter *character, CWeaponLevelUp *load_buffer, int tex_block);

    /**
     * Applies the completed level-up values to the weapon.
     *
     * @mangled SetLevelUpWeaponData__14CWeaponLevelUpFv
     * @address 0x00236770
     * @size 0x158
     */
    void SetLevelUpWeaponData();

    /**
     * Calculates and starts the status-break effect.
     *
     * @mangled SetStatusBreak__14CWeaponLevelUpFP11WEAPON_HAVEP10CCharacterP1i
     * @address 0x002368D0
     * @size 0x358
     */
    void SetStatusBreak(WEAPON_HAVE *have, CCharacter *character, CWeaponLevelUp *load_buffer, int tex_block);

    /**
     * Calculates and starts the weapon build-up effect.
     *
     * @mangled SetBuildUp__14CWeaponLevelUpFP11WEAPON_HAVEP10CCharacterP1i
     * @address 0x00236C30
     * @size 0xCC
     */
    void SetBuildUp(WEAPON_HAVE *have, CCharacter *character, CWeaponLevelUp *load_buffer, int tex_block);

    /**
     * Starts the weapon recovery effect.
     *
     * @mangled WepRecover__14CWeaponLevelUpFP11WEAPON_HAVEP10CCharacterP1i
     * @address 0x00236D00
     * @size 0x8C
     */
    void WepRecover(WEAPON_HAVE *have, CCharacter *character, CWeaponLevelUp *load_buffer, int tex_block);

    /**
     * Starts an effect for curing a weapon-related status.
     *
     * @mangled CureEffect__14CWeaponLevelUpFiiP1ii
     * @address 0x00236D90
     * @size 0x100
     */
    void CureEffect(int x, int y, CWeaponLevelUp *load_buffer, int tex_block, int kind);

    /**
     * Resets the background-music fade state.
     *
     * @mangled InitSnd__14CWeaponLevelUpFv
     * @address 0x00236E90
     * @size 0x1C
     */
    void InitSnd();

    /**
     * Starts a background-music fade from one volume to another.
     *
     * @mangled SetSnd__14CWeaponLevelUpFiii
     * @address 0x00236EB0
     * @size 0x3C
     */
    void SetSnd(int from, int to, int step);

    /**
     * Advances the active background-music fade by one step.
     *
     * @mangled StepSnd__14CWeaponLevelUpFv
     * @address 0x00236EF0
     * @size 0x98
     */
    void StepSnd();

    /**
     * Restores the current background-music volume and clears the fade.
     *
     * @mangled CheckSnd__14CWeaponLevelUpFv
     * @address 0x00236F90
     * @size 0x78
     */
    void CheckSnd();

    /**
     * Advances the active weapon effect state machine.
     *
     * @mangled Step__14CWeaponLevelUpFv
     * @address 0x00237010
     * @size 0x91C
     */
    void Step();

    /**
     * Draws the active weapon effect and its attachment icons.
     *
     * @mangled Draw__14CWeaponLevelUpFv
     * @address 0x00237930
     * @size 0x36C
     */
    void Draw();

    /**
     * Draws the result message for the active weapon effect.
     *
     * @mangled DrawMes__14CWeaponLevelUpFv
     * @address 0x00237CA0
     * @size 0x404
     */
    void DrawMes();
};

STATIC_ASSERT(sizeof(CWeaponLevelUp) == 0x1330);

/** State of the weapon menu's repair, level-up and build-up effects. */
extern CWeaponLevelUp MenuWepLevelUp;

/**
 * Adds scaled attachment values to an accumulated attachment record.
 *
 * @mangled AttachMentValuePlus__FP11ATTACH_LISTP11ATTACH_LISTf
 * @address 0x00235A10
 * @size 0x1EC
 */
void AttachMentValuePlus(ATTACH_LIST *total, ATTACH_LIST *attach, float scale);

/**
 * Calculates the weapon values produced by one or more level-ups.
 *
 * @mangled WeaponLevelUpValueCalc__FP11WEAPON_HAVEP11WEAPON_HAVEii
 * @address 0x00235C00
 * @size 0x3FC
 */
void WeaponLevelUpValueCalc(WEAPON_HAVE *src, WEAPON_HAVE *dst, int levels, int unused);

/** Whether an interior is being entered, and the item-volume step to check. */
extern CMenuItemStep ItemVolumeStep;
