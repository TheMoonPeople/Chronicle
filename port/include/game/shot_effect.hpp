#pragma once

#include "common.h"

#include <libvu0.h>

#include "character.hpp"
#include "dataalloc_fwd.hpp"

// Forward declarations for the types these declarations name. The skeleton
// headers are generated from the retail symbol table, which knows the type
// names but not where they live.
#include "bt_shot_effect.hpp"

/**
 * Manages the eight active projectile-effect slots.
 */
class CSHOT_EFFECT {
public:
    BT_SHOT_EFFECT *effect_data;        /**< Description shared by the active effect slots. */
    CCharacter      template_chara;     /**< Model read from the pack that every slot's character is copied from. */
    CCharacter      chara[8];           /**< Model that each projectile-effect slot draws and animates. */
    sceVu0FVECTOR   velocity[8];        /**< Movement applied to each projectile-effect slot. */
    s16             source_id[8];       /**< Source identifier supplied when each slot starts. */
    s32             phase_delay[8];     /**< Remaining delay before each slot changes phase. */
    s16             phase[8];           /**< Current animation and collision phase of each slot. */
    s16             active[8];          /**< Nonzero while each effect slot is active. */
    s32             damage[8];          /**< Damage dealt by each effect slot. */
    s32             weapon_status[8];   /**< Weapon status carried by each effect slot. */
    s16             user_id[8];         /**< Primary user identifier of each effect slot. */
    s16             user_id_2[8];       /**< Secondary user identifier of each effect slot. */
    s32             user_sub_id[8];     /**< Secondary collision-owner value of each effect slot. */
    char           *vs_monster[8];      /**< Monster-effectiveness table of each effect slot. */
    s32             loop[8];            /**< Whether each effect slot loops. */
    float           random_rate[8];     /**< Randomization rate of each effect slot. */
    s32             life_time[8];       /**< Lifetime of each effect slot. */
    s32             enemy_attribute[8]; /**< Enemy attribute of each effect slot. */
    char            no_sound[8];        /**< Whether sound is suppressed for each effect slot. */
    char            wait[8];            /**< Delay applied to each effect slot. */
    char            wait_state[8];      /**< Secondary delay state of each effect slot. */
    s32             status;             /**< Shared runtime status reset whenever a shot starts. */
    s32             slot_count;         /**< Number of effect slots initialized by the loader. */
    s32             current_slot;       /**< Currently selected effect slot, or -1. */
    s32             texture_block;      /**< Texture block the effect's model was loaded into and is reloaded from before drawing. */
    u8              unk_A158[8];

    /**
     * Draws every active projectile-effect slot.
     *
     * @mangled Draw__12CSHOT_EFFECTFv
     * @address 0x1ABF20
     * @size 0x25C
     */
    void Draw();

    /**
     * Advances every active projectile-effect slot: plays its motions through their phases, moves
     * it, records its hit, and ends it when its last motion finishes.
     *
     * @mangled Step__12CSHOT_EFFECTFv
     * @address 0x1AC180
     * @size 0x9B0
     */
    void Step();

    /**
     * Moves eligible active projectile effects into their ending phase.
     *
     * @mangled EndEffect__12CSHOT_EFFECTFv
     * @address 0x1ACB30
     * @size 0xD4
     */
    void EndEffect();

    /**
     * Disables one projectile-effect slot, or every slot when given -1.
     *
     * @mangled OffEffect__12CSHOT_EFFECTFi
     * @address 0x1ACC10
     * @size 0x60
     */
    void OffEffect(int slot);

    /**
     * Loads the effect's model file into the pack, reads its model from it and gives every slot
     * a copy; returns 1, or 0 when a description is already loaded.
     *
     * @mangled Entry__12CSHOT_EFFECTFP14BT_SHOT_EFFECTPUiiP14CDataAlloc2_1_i
     * @address 0x1ACC70
     * @size 0x5E4
     */
    int Entry(BT_SHOT_EFFECT *description, unsigned int *pack, int tex_block, CDataAlloc2<1> *allocator, int slots);

    /**
     * Reads the effect's model from a pack that is already loaded and gives every slot a copy;
     * returns 1, or 0 when a description is already loaded.
     *
     * @mangled Entry2__12CSHOT_EFFECTFP14BT_SHOT_EFFECTPUiiP14CDataAlloc2_1_i
     * @address 0x1AD260
     * @size 0x5A8
     */
    int Entry2(BT_SHOT_EFFECT *description, unsigned int *pack, int tex_block, CDataAlloc2<1> *allocator, int slots);

    /**
     * Gives every slot a fresh copy of the template character and switches to a replacement
     * description; returns 1.
     *
     * @mangled ReEntry__12CSHOT_EFFECTFP14BT_SHOT_EFFECTP14CDataAlloc2_1_
     * @address 0x1AD810
     * @size 0x508
     */
    int ReEntry(BT_SHOT_EFFECT *description, CDataAlloc2<1> *allocator);

    /**
     * Selects whether the current projectile effect loops.
     *
     * @mangled SetLoop__12CSHOT_EFFECTFi
     * @address 0x1ADD20
     * @size 0x34
     */
    void SetLoop(int loop);

    /**
     * Starts one shot effect and gives back the slot it went into; with no
     * description loaded or no free slot it returns without a value.
     *
     * @mangled Set__12CSHOT_EFFECTFPfPfiiiP6CFramei
     * @address 0x1ADD60
     * @size 0x458
     */
    int Set(float *position, float *target, int owner, int sub_id, int source, CFrame *parent, int initial_phase);

    /**
     * Delays the current projectile effect and clears its secondary wait state.
     *
     * @mangled SetWait__12CSHOT_EFFECTFi
     * @address 0x1AE1C0
     * @size 0x4C
     */
    void SetWait(int wait);

    /**
     * Suppresses sound for the current projectile effect.
     *
     * @mangled SetNoSound__12CSHOT_EFFECTFv
     * @address 0x1AE210
     * @size 0x34
     */
    void SetNoSound();

    /**
     * Sets the randomization rate for the current projectile effect.
     *
     * @mangled SetRandomRate__12CSHOT_EFFECTFf
     * @address 0x1AE250
     * @size 0x34
     */
    void SetRandomRate(float rate);

    /**
     * Sets the lifetime of the current projectile effect.
     *
     * @mangled SetLifeTime__12CSHOT_EFFECTFi
     * @address 0x1AE290
     * @size 0x34
     */
    void SetLifeTime(int life_time);

    /**
     * Sets the enemy attribute of the current projectile effect.
     *
     * @mangled SetEnemyAttr__12CSHOT_EFFECTFi
     * @address 0x1AE2D0
     * @size 0x34
     */
    void SetEnemyAttr(int attribute);

    /**
     * Sets the damage dealt by the current projectile effect.
     *
     * @mangled SetDmg__12CSHOT_EFFECTFi
     * @address 0x1AE310
     * @size 0x34
     */
    void SetDmg(int damage);

    /**
     * Sets the source effect's attribute when an effect slot is selected.
     *
     * @mangled SetAttribute__12CSHOT_EFFECTFi
     * @address 0x1AE350
     * @size 0x28
     */
    void SetAttribute(int attribute);

    /**
     * Sets the weapon status of the current projectile effect.
     *
     * @mangled SetWepStatus__12CSHOT_EFFECTFi
     * @address 0x1AE380
     * @size 0x34
     */
    void SetWepStatus(int status);

    /**
     * Sets the monster-effectiveness table of the current projectile effect.
     *
     * @mangled SetVsMonster__12CSHOT_EFFECTFPc
     * @address 0x1AE3C0
     * @size 0x34
     */
    void SetVsMonster(char *effectiveness);

    /**
     * Sets the secondary user identifier of the current projectile effect.
     *
     * @mangled SetUserID2__12CSHOT_EFFECTFi
     * @address 0x1AE400
     * @size 0x34
     */
    void SetUserID2(int id);

    /**
     * Clears the loaded effect and resets all projectile-effect slots.
     *
     * @mangled Initialize__12CSHOT_EFFECTFv
     * @address 0x1AE440
     * @size 0x80
     */
    void Initialize();
} __attribute__((aligned(16)));

STATIC_ASSERT(sizeof(CSHOT_EFFECT) == 0xA160);

class CSHOT {
public:
    u8            unk_000[0x40];
    sceVu0FVECTOR pos[12]; /**< Where each shot is. */
    sceVu0FVECTOR unk_100[12];
    sceVu0FVECTOR vector[12]; /**< The way each shot flies, and how fast. */
    s32           halted[12]; /**< Nonzero while each shot is held in place and skips collision and movement. */
    s32           life[12];   /**< How long each shot has left, in frames. */
    s32           damage[12]; /**< What each shot takes off what it hits. */
    float         size[12];   /**< Sprite size each shot is drawn at. */
    s32           unk_340[12];
    s32           unk_370[12];
    s32           start_flag[12]; /**< Flag cleared whenever a slot takes a new shot. */
    s32           used[12];       /**< 1 while the slot holds a shot. */

    /**
     * Draws the twelve projectiles of one shot.
     *
     * @mangled draw__5CSHOTFv
     * @address 0x1ABC40
     * @size 0xCC
     */
    void draw();

    /**
     * Advances the twelve projectiles of one shot.
     *
     * @mangled step__5CSHOTFv
     * @address 0x1ABD10
     * @size 0x204
     */
    void step();
};

STATIC_ASSERT(sizeof(CSHOT) == 0x400);

class CSHOT_MACHINGUN {
public:
    float position[16][4]; /**< Positions of the sixteen rapid-fire projectiles. */
    float velocity[16][4]; /**< Movement vectors of the sixteen rapid-fire projectiles. */
    s32   damage[16];      /**< Damage each rapid-fire projectile deals on a monster hit. */
    s32   element[16];     /**< Element each rapid-fire projectile was fired with. */
    s32   age[16];         /**< Frames each rapid-fire projectile has flown; 0 while the slot is free. */

    /**
     * Starts one rapid-fire projectile and returns its slot, or -1 if full.
     *
     * @mangled Set__15CSHOT_MACHINGUNFPfPfii
     * @address 0x1AE660
     * @size 0xEC
     */
    int Set(float *origin, float *direction, int damage, int element);

    /**
     * Advances the sixteen rapid-fire projectiles.
     *
     * @mangled Step__15CSHOT_MACHINGUNFv
     * @address 0x1AE750
     * @size 0x230
     */
    void Step();
};

STATIC_ASSERT(sizeof(CSHOT_MACHINGUN) == 0x2C0);
