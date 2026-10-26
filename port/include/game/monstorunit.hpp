#pragma once

#include "common.h"

#include <libvu0.h>

#include "bt_shot_effect.hpp"
#include "character.hpp"
#include "dataalloc_fwd.hpp"
#include "runscript.hpp"

/**
 * What CMonstorUnit::CheckDmg reports.
 */
// clang-format off
enum MonsterDamageResult {
    MONSTER_DMG_NONE   = 0, /**< Not hit. */
    MONSTER_DMG_HIT    = 1, /**< Hit. */
    MONSTER_DMG_KILLED = 2, /**< Killed. */
};

// clang-format on

/**
 * Kinds of monster model, as MONSTOR_MODEL::kind holds them.
 */
// clang-format off
enum MonsterKind {
    MONSTER_KIND_NORMAL      = 0, /**< Ordinary monster. */
    MONSTER_KIND_NO_LOCK_ON  = 2, /**< Left alone by the lock-on cursor. */
    MONSTER_KIND_MIMIC_SMALL = 3, /**< Mimic in a small box. */
    MONSTER_KIND_MIMIC_LARGE = 4, /**< Mimic in a large box. */
};

// clang-format on

/**
 * Families of monster, as MONSTOR_MODEL::attachment_kind holds them: the slayer attachment the
 * monster drops (ITEM_ATTACH_SLAYER_START plus the family) and the anti-monster value of a weapon
 * that hits it.
 */
// clang-format off
enum MonsterFamily {
    MONSTER_FAMILY_DINO   = 0, /**< Dinoslayer. */
    MONSTER_FAMILY_UNDEAD = 1, /**< Undead Buster. */
    MONSTER_FAMILY_SEA    = 2, /**< Sea Killer. */
    MONSTER_FAMILY_STONE  = 3, /**< Stone Breaker. */
    MONSTER_FAMILY_PLANT  = 4, /**< Plant Buster. */
    MONSTER_FAMILY_BEAST  = 5, /**< Beast Buster. */
    MONSTER_FAMILY_SKY    = 6, /**< Sky Hunter. */
    MONSTER_FAMILY_METAL  = 7, /**< Metal Breaker. */
    MONSTER_FAMILY_MIMIC  = 8, /**< Mimic Breaker. */
    MONSTER_FAMILY_MAGE   = 9, /**< Mage Slayer. */
};

// clang-format on

class CDungeonMap;

/**
 * Records what one monster of the floor is doing.
 */
struct MONSTOR {
    s32           state;                /**< 2 while the monster stands on the floor and takes part. */
    s32           last_attacker;        /**< Character credited with the kill, or -1. */
    s32           stop_timer;           /**< Steps left before the stop status wears off; the monster cannot move or act meanwhile. */
    s32           poison_timer;         /**< Steps left until poison next takes a tenth of the monster's life. */
    s32           anger_timer;          /**< Steps left of anger, which doubles the damage the monster deals and halves the damage it takes. */
    s32           slow_timer;           /**< Steps left of the slowing status, which halves the monster's motion and movement speed. */
    float         player_distance;      /**< Distance from the player, measured by CheckViewLevel. */
    s32           base_model;           /**< Index of the loaded model the monster was set up from. */
    s32           max_hp;               /**< Life the monster has at full health. */
    s32           hp;                   /**< Life the monster has left. */
    s16           attachment_kind;      /**< Family of the monster. @see MonsterFamily. */
    s16           attachment_weight[5]; /**< Weight of each attachment kind in the drop choice and in elemental damage. */
    s32           money;                /**< Least amount of money the monster drops. */
    s32           money_chance;         /**< Percentage chance that the monster drops money. */
    s32           stolen_money;         /**< Money the monster has drained from the player and drops when defeated. */
    s16           kind;                 /**< Kind of monster model. @see MonsterKind. */
    s16           name_no;              /**< Identifies the name the lock-on cursor shows. */
    float         body_radius;          /**< Radius the player and the walls keep from the monster. */
    float         collision_radius;     /**< Radius the monster keeps from other monsters. */
    CCPoly       *collision_poly;       /**< Polygons the monster tests its movement against this step. */
    s32           collision_poly_count; /**< Polygons in collision_poly. */
    u8            unk_054[0x0C];
    sceVu0FVECTOR movement;       /**< Direction the monster moves in. */
    sceVu0FVECTOR turn_target;    /**< Point the monster turns to face. */
    float         movement_speed; /**< Distance the monster moves each step. */
    float         turn_speed;     /**< Angle the monster turns each step; zero once the turn is done. */
    s32           falls;          /**< Nonzero while the monster keeps to the ground beneath it and bounces off it. */
    u8            unk_08C[4];
    s16           defense;  /**< Amount taken off the damage of each hit. */
    s16           hardness; /**< Wear the monster puts on the weapon that hits it. */
    s16           unk_094;
    u8            unk_096[2];
    s32           invincible_timer; /**< Steps left in which the monster takes no hits. */
    u8            unk_09C[4];
    s16           drop_item; /**< Key or attachment the monster drops when defeated, or -1. */
    u8            unk_0A2[2];
    float         clip_distance;       /**< Distance past which the monster stops taking part and being drawn. */
    s32           collision_off_timer; /**< Steps left in which the monster moves through the player, other monsters and walls. */
    s16           shot_effect;         /**< Effect the first projectile fires, or -1. */
    s16           shot_effect2;        /**< Effect the second projectile fires, or -1. */
    s32           exp;                 /**< Experience the weapon that defeats the monster gains. */
    s16           attachment_count;    /**< Attachment characters the monster draws with. */
    u8            unk_0B6[2];
    float         ground_distance;  /**< Height of the monster above the ground beneath it. */
    float         ground_y;         /**< Height of the ground beneath the monster. */
    s32           last_hit_damage;  /**< Damage the last hit did, or -1 once a new motion starts. */
    s32           last_hit_id;      /**< Identifies the attack that last hit the monster, or -1. */
    s32           hit_element;      /**< Element of the attack that last hit the monster; 5 for none. */
    float         shadow_length;    /**< Distance the monster's shadow is cast below it. */
    s16           shadow_visible;   /**< Nonzero while the monster's shadow draws. */
    s16           shadow_enabled;   /**< Nonzero when the script lets the monster's shadow draw. */
    s16           revealed;         /**< 0 while the monster hides in a treasure chest, 1 once opened until its appearance script runs, -1 while in play. */
    s16           free_fall;        /**< Nonzero lets a falling monster stay above the ground; zero keeps it on the ground. */
    s16           steal_item;       /**< Item the player can steal from the monster, or -1. */
    s16           drops_items;      /**< Nonzero when the monster can drop a key, an attachment or money. */
    s16           item_damage_rate; /**< Percentage of damage taken from hits no character owns. */
    s16           status_chance;    /**< Percentage chance that a status hit takes hold. */
    s16           rare_item;        /**< Item the monster sometimes drops in place of a key, or -1. */
    u8            unk_0E2[2];
    s32           invincible_blocked;     /**< Nonzero to refuse invincibility to a living monster; cleared at each damage check. */
    s32           view_held;              /**< Nonzero while the view limit holds the monster out of play. */
    s16           requested_motion;       /**< Motion the script asked the monster to play. */
    s16           requested_motion_flags; /**< Flags of the requested motion. */
    float         requested_motion_speed; /**< Speed of the requested motion; below zero for the motion's own. */
    s16           motion_reset_pending;   /**< Nonzero while the requested motion waits to be restarted after a status ends. */
    s16           event_flag2;            /**< Value returned when CheckEventFlag2 consumes the event. */
    s16           event_flag2_pending;    /**< Nonzero until CheckEventFlag2 consumes the event. */
    u8            unk_0FA[2];
    CFrame       *lockon_frame;    /**< Frame the lock-on cursor stands on, or zero for the monster's position. */
    sceVu0FVECTOR lockon_position; /**< World position of lockon_frame. */
    float         lockon_scale_x;  /**< Width scale of the lock-on cursor. */
    float         lockon_scale_y;  /**< Height scale of the lock-on cursor. */
    float         lock_range;      /**< Distance up to which the monster can be locked on to. */
    s16           lockon_enabled;  /**< Nonzero while the monster can be locked on to. */
    u8            unk_11E[2];
    float         palette_alpha;      /**< Alpha of the monster's ambient light. */
    float         palette_alpha_step; /**< Amount the alpha falls by each step. */
    s32           palette_delay;      /**< Steps to wait before the alpha starts falling. */
    u8            unk_12C[0x04];
    sceVu0FVECTOR palette_target;           /**< Colour the palette cycle blends towards. */
    sceVu0FVECTOR palette_color;            /**< Ambient colour the monster draws with. */
    sceVu0FVECTOR palette_override;         /**< Colour that replaces the ambient for one draw. */
    s32           palette_override_pending; /**< Nonzero while the override colour waits to be applied. */
    s32           palette_cycles;           /**< Blend cycles left towards the target colour. */
    float         palette_step;             /**< Amount the blend moves by each step; negated at each end. */
    float         palette_blend;            /**< Fraction of the way from the ambient to the target colour. */
    sceVu0FVECTOR knockback_direction;      /**< Direction a hit throws the monster in. */
    sceVu0FVECTOR knockback;                /**< Knockback speed, the amount it falls by each step, and the scale hits apply to it. */
};

STATIC_ASSERT(sizeof(MONSTOR) == 0x190);

/**
 * The characters one monster draws with: its model and up to two attachments.
 */
typedef CCharacter CMonstorChara[3];

STATIC_ASSERT(sizeof(CMonstorChara) == 0x3510);

/**
 * Frame windows and retrigger delays for one monster's sounds.
 */
struct MONSTOR_SOUND {
    float start[16];      /**< Motion frame each sound starts on. */
    float end[16];        /**< Motion frame each sound stops on. */
    s32   id[16];         /**< Sound effect each window plays, or -1. */
    s32   cooldown[16];   /**< Steps before each sound may play again. */
    float sequence_start; /**< Motion frame the sound sequence starts on. */
    float sequence_end;   /**< Motion frame the sound sequence stops on. */
    s32   sequence_step;  /**< Step of the sound sequence to play. */
    s32   sequence_id;    /**< Sound sequence to play, or -1. */
};

STATIC_ASSERT(sizeof(MONSTOR_SOUND) == 0x110);

/**
 * Model, script and combat parameters of one kind of monster.
 */
struct MONSTOR_MODEL {
    char  model_name[4][16];    /**< Model file of the monster and of up to three attachments. */
    char  script_name[16];      /**< Script file that drives the monster. */
    s32   max_hp;               /**< Life the monster has at full health. */
    s16   attachment_kind;      /**< Family of the monster. @see MonsterFamily. */
    s16   attachment_weight[5]; /**< Weight of each attachment kind in the drop choice and in elemental damage. */
    float collision_radius;     /**< Radius the monster keeps from other monsters. */
    s16   defense;              /**< Amount taken off the damage of each hit. */
    s16   hardness;             /**< Wear the monster puts on the weapon that hits it. */
    s16   shot_effect[2];       /**< Entry effects the monster fires, or -1. */
    s16   exp;                  /**< Experience the weapon that defeats the monster gains. */
    u8    unk_06E[2];
    s32   money;        /**< Least amount of money the monster drops. */
    s32   money_chance; /**< Percentage chance that the monster drops money. */
    s16   kind;         /**< Kind of monster model. @see MonsterKind. */
    u8    unk_07A[2];
    s16   name_no; /**< Identifies the name the lock-on cursor shows. */
    u8    unk_07E[2];
    s16   steal_item;          /**< Item the player can steal from the monster, or -1. */
    s16   drops_items;         /**< Nonzero when the monster can drop a key, an attachment or money. */
    s16   item_damage_rate;    /**< Percentage of damage taken from hits no character owns. */
    s16   status_chance;       /**< Percentage chance that a status hit takes hold. */
    s16   rare_item;           /**< Item the monster sometimes drops in place of a key, or -1. */
    s16   effect_parameter[6]; /**< Percentage of damage taken from each attacker. */
    u8    unk_096[2];
    float knockback_scale; /**< Scale applied to the knockback of each hit. */
};

STATIC_ASSERT(sizeof(MONSTOR_MODEL) == 0x9C);

/**
 * Hits one monster can take, one slot per collision sphere.
 */
struct MONSTOR_EFFECT_STATE {
    sceVu0FVECTOR position[16];          /**< World position of each sphere. */
    CFrame       *frame[16];             /**< Frame each sphere follows. */
    float         radius[16];            /**< Radius of each sphere. */
    float         motion_start[16];      /**< Motion frame each sphere starts taking hits on; 0 for always. */
    float         motion_end[16];        /**< Motion frame each sphere stops taking hits on. */
    s32           timer[16];             /**< Nonzero while each sphere is in use. */
    s32           body_parameter[16][5]; /**< Values the script sets on each sphere, 100 by default. */
    s32           parameter[16][6];      /**< Percentage of damage each sphere takes from each attacker. */
    s32           hit_slot;              /**< Sphere the last hit landed on. */
    s32           hit_attributes;        /**< Weapon flags of the last hit. */
    u8            unk_508[8];
};

STATIC_ASSERT(sizeof(MONSTOR_EFFECT_STATE) == 0x510);

/**
 * Hits one monster gives, one slot per collision sphere.
 */
struct MONSTOR_EFFECT_STATE2 {
    sceVu0FVECTOR position[16];     /**< World position of each sphere. */
    CFrame       *frame[16];        /**< Frame each sphere follows. */
    float         radius[16];       /**< Radius of each sphere. */
    s32           damage[16];       /**< Damage each sphere deals. */
    s32           kind[16];         /**< Kind of hit each sphere deals. */
    float         angle[16];        /**< Direction each sphere throws the player in, in degrees; 0 for away. */
    s32           flags[16];        /**< What each sphere's hit does besides damage. */
    float         motion_start[16]; /**< Motion frame each sphere starts hitting on. */
    float         motion_end[16];   /**< Motion frame each sphere stops hitting on. */
    s32           active[16];       /**< Nonzero while each sphere is in use. */
    s32           last_slot;        /**< Sphere the script set up last, or -1 when none was found. */
    u8            unk_344[0xC];
};

STATIC_ASSERT(sizeof(MONSTOR_EFFECT_STATE2) == 0x350);

/**
 * Spheres that block the player's movement around one monster.
 */
struct MONSTOR_EFFECT_STATE3 {
    sceVu0FVECTOR position[12]; /**< World position of each sphere. */
    CFrame       *frame[12];    /**< Frame each sphere follows. */
    float         radius[12];   /**< Radius of each sphere. */
    s32           timer[12];    /**< Nonzero while each sphere is in use. */
    s32           count;        /**< Spheres in use. */
    u8            unk_154[0x0C];
};

STATIC_ASSERT(sizeof(MONSTOR_EFFECT_STATE3) == 0x160);

/**
 * A projectile one monster is about to fire.
 */
struct MONSTOR_EVENT_STATE {
    sceVu0FVECTOR local_position;  /**< Direction the projectile leaves in. */
    sceVu0FVECTOR position;        /**< World position the projectile leaves from. */
    CFrame       *frame;           /**< Frame the projectile leaves from. */
    s32           timer;           /**< 1 once asked for, 2 once positioned, 0 once fired. */
    s32           damage_override; /**< Damage that replaces the effect's own, or -1. */
    u8            unk_02C[4];
};

STATIC_ASSERT(sizeof(MONSTOR_EVENT_STATE) == 0x30);

/**
 * Motion frames during which one monster guards against hits.
 */
struct MONSTOR_GUARD_WINDOW {
    s16   active[3]; /**< Nonzero while each window is in use. */
    u8    unk_006[2];
    float motion_start[3]; /**< Motion frame each window opens on. */
    float motion_end[3];   /**< Motion frame each window closes on. */
};

STATIC_ASSERT(sizeof(MONSTOR_GUARD_WINDOW) == 0x20);

/**
 * Runs the monsters of one dungeon floor.
 */
class CMonstorUnit {
public:
    CDataAlloc2<1>       *script[16];       /**< Script working memory for each monster on the floor. */
    CFrame               *collision;        /**< Collision model that every monster on the floor shares. */
    s32                   back_dungeon;     /**< Nonzero on the back dungeon, where monsters stay angry, give double experience and carry no key. */
    s32                   model_count;      /**< Kinds of monster loaded for the floor. */
    s32                   alive_count;      /**< Monsters alive on the floor. */
    s32                   script_state[16]; /**< 1 while each monster's script is running. */
    s32                   current_monster;  /**< Index of the monster being stepped or drawn. */
    s32                   requested_event;  /**< Event a monster's script asked to run, or -1. */
    s32                   paused;           /**< Nonzero while the monsters are paused; their motions restart when play resumes. */
    s32                   unk_09C;
    CMonstorChara         base_chara[9];   /**< Characters loaded for each kind of monster on the floor. */
    MONSTOR_MODEL         model[9];        /**< Parameters of each kind of monster on the floor. */
    char                 *script_data[9];  /**< Script of each kind of monster on the floor. */
    MONSTOR               monster[16];     /**< What each monster of the floor is doing. */
    CMonstorChara         chara[16];       /**< The characters each monster draws with. */
    CRunScript            interpreter[16]; /**< Script interpreter of each monster. */
    MONSTOR_EFFECT_STATE  effect[16];      /**< Hits each monster can take. */
    MONSTOR_EFFECT_STATE2 effect2[16];     /**< Hits each monster gives. */
    MONSTOR_EFFECT_STATE3 effect3[16];     /**< Spheres that block the player around each monster. */
    MONSTOR_SOUND         sound[16];       /**< Sounds each monster plays. */
    MONSTOR_EVENT_STATE   event[16];       /**< First projectile each monster fires. */
    MONSTOR_EVENT_STATE   event2[16];      /**< Second projectile each monster fires. */

    union {
        s16                  event_flags[16][16]; /**< Guard windows of each monster, as the words set-up clears. */
        MONSTOR_GUARD_WINDOW guard[16];           /**< Guard windows of each monster. */
    };

    /**
     * Index of the monster whose behaviour is being evaluated.
     */
    int GetCurrentMonsterIndex() { return current_monster; }

    /**
     * Counts the monsters present on the floor.
     *
     * @mangled GetMonstorNum__12CMonstorUnitFv
     * @address 0x1D7A40
     * @size 0x5C
     */
    int GetMonstorNum();

    /**
     * Draws the monsters' symbols on the minimap.
     *
     * @mangled DrawMapSymbol__12CMonstorUnitFPf
     * @address 0x1D7AA0
     * @size 0x1D8
     * @unknownret
     */
    void DrawMapSymbol(float *offset);

    /**
     * Gives the floor's key to one monster.
     *
     * @mangled SetKey__12CMonstorUnitFv
     * @address 0x1D7C80
     * @size 0x2A4
     * @unknownret
     */
    void SetKey();

    /**
     * Returns and clears the event a defeated monster raised, or -1.
     *
     * @mangled CheckEventFlag2__12CMonstorUnitFv
     * @address 0x1D7F30
     * @size 0x88
     */
    int CheckEventFlag2();

    /**
     * Places monsters on the floor, apart from the others.
     *
     * @mangled ArrangementPos__12CMonstorUnitFP11CDungeonMapiii
     * @address 0x1D7FC0
     * @size 0x398
     */
    void ArrangementPos(CDungeonMap *map, int count, int model_no, int unused);

    /**
     * Starts a timed status on every monster of the floor.
     *
     * @mangled AllBin2__12CMonstorUnitFv
     * @address 0x1D8360
     * @size 0x4C
     */
    void AllBin2();

    /**
     * Applies the monster's palette to the ambient light before it draws.
     *
     * @mangled PalletSet__12CMonstorUnitFv
     * @address 0x1D83B0
     * @size 0x1C0
     */
    void PalletSet();

    /**
     * Advances the monster's palette fade and colour cycle.
     *
     * @mangled PalletStep__12CMonstorUnitFv
     * @address 0x1D8570
     * @size 0x418
     */
    void PalletStep();

    /**
     * Plays the monster's motion sounds that fall on the current frame.
     *
     * @mangled SoundCheck__12CMonstorUnitFv
     * @address 0x1D8990
     * @size 0x338
     */
    void SoundCheck();

    /**
     * Steps and draws every monster in play.
     *
     * @mangled DrawMonstor__12CMonstorUnitFv
     * @address 0x1D8CD0
     * @size 0x534
     * @unknownret
     */
    void DrawMonstor();

    /**
     * Draws the cursor over monsters that the view limit put on hold.
     *
     * @mangled DrawMonstorCursor__12CMonstorUnitFv
     * @address 0x1D9210
     * @size 0xE0
     * @unknownret
     */
    void DrawMonstorCursor();

    /**
     * Draws the shadows of the monsters in play.
     *
     * @mangled DrawShadowMonstor__12CMonstorUnitFv
     * @address 0x1D9800
     * @size 0x19C
     * @unknownret
     */
    void DrawShadowMonstor();

    /**
     * Wakes monsters near the player, and limits how many are active at once.
     *
     * @mangled CheckViewLevel__12CMonstorUnitFv
     * @address 0x1D99A0
     * @size 0x3BC
     */
    void CheckViewLevel();

    /**
     * Chooses the attachment a defeated monster drops, or -1.
     *
     * @mangled SelectAttachi__12CMonstorUnitFv
     * @address 0x1D9D60
     * @size 0x1B0
     */
    int SelectAttachi();

    /**
     * Applies the hits a monster gives and takes this step. @see MonsterDamageResult.
     *
     * @mangled CheckDmg__12CMonstorUnitFv
     * @address 0x1D9F10
     * @size 0x2904
     */
    int CheckDmg();

    /**
     * Stops the player's movement where it would run into a monster.
     *
     * @mangled MoveCheck__12CMonstorUnitFPfPfi
     * @address 0x1DC820
     * @size 0x5A8
     * @unknownret
     */
    void MoveCheck(float *position, float *movement, int flat);

    /**
     * Stops the monster's movement where it would run into the player.
     *
     * @mangled MoveCheck2__12CMonstorUnitFv
     * @address 0x1DCDD0
     * @size 0x36C
     */
    void MoveCheck2();

    /**
     * Stops the monster's movement where it would run into another monster.
     *
     * @mangled MoveChecMonster__12CMonstorUnitFv
     * @address 0x1DD140
     * @size 0x3FC
     */
    void MoveChecMonster();

    /**
     * Runs every monster's script, movement, damage and rewards for one frame.
     *
     * @mangled Step__12CMonstorUnitFi
     * @address 0x1DD540
     * @size 0x24A4
     */
    void Step(int pause);

    /**
     * Clears every monster and effect on the floor.
     *
     * @mangled CleanViewMonstor__12CMonstorUnitFi
     * @address 0x1DF9F0
     * @size 0x4A0
     */
    void CleanViewMonstor(int back_floor);

    /**
     * Loads the models one monster needs, and says how many it took.
     *
     * @mangled SetupBaseModel__12CMonstorUnitFiiiP14CDataAlloc2_1_
     * @address 0x1DFE90
     * @size 0x414
     * @unknownret
     */
    int SetupBaseModel(int slot, int model_no, int texture_block, CDataAlloc2<1> *alloc);

    /**
     * Puts a loaded monster model on the floor at a position.
     *
     * @mangled SetupViewMonstor__12CMonstorUnitFiPfi
     * @address 0x1E02B0
     * @size 0x138C
     */
    int SetupViewMonstor(int model_no, float *position, int event_flag);
};

STATIC_ASSERT(sizeof(CMonstorUnit) == 0x60750);

/**
 * Animation frame and phase of one rendered bee.
 */
struct BEE_STATE {
    s32   row;   /**< Row of the bee's texture cell. */
    float phase; /**< Column of the bee's texture cell, advanced each draw. */
};

STATIC_ASSERT(sizeof(BEE_STATE) == 8);

/** Animation state of every bee drawn around the bee monster. */
extern BEE_STATE BeeTbl[800];

/** Number of floors in each dungeon. */
extern "C" int maxFloorTbl__3[7];

/** Texture animations shared by every monster model. */
extern "C" CTexAnimeData MonsterTexAnim[320];

/**
 * Names one monster of a floor's enemy layout.
 */
struct BT_ENEMY_LAYOUT {
    s32 unk_00;
    s32 monster_no; /**< Identifies the monster, or -1 where the list ends. */
    s32 unk_08;
};

/**
 * Names every monster one floor lays out.
 */
struct BT_ENEMY_FLOOR {
    BT_ENEMY_LAYOUT monster[9]; /**< The monsters the floor can hold. */
    s32             unk_6C;
};

STATIC_ASSERT(sizeof(BT_ENEMY_LAYOUT) == 0x0C);
STATIC_ASSERT(sizeof(BT_ENEMY_FLOOR) == 0x70);

/**
 * Scatters the bees over their frames and hides the frames themselves.
 *
 * @mangled InitBee__FP6CFramei
 * @address 0x1D9420
 * @size 0x164
 */
void InitBee(CFrame *frame, int count);

/** Model, script and combat parameters of every kind of monster. */
extern "C" MONSTOR_MODEL MonstorTable[167];

/** Projectile effects the monsters fire, indexed by MONSTOR_MODEL::shot_effect. */
extern "C" BT_SHOT_EFFECT *BtEntryEffectTbl[35];

/** The monsters each floor of each dungeon lays out. */
extern "C" BT_ENEMY_FLOOR *BtEnemyLayoutList[];

/** The same for the back dungeon. */
extern "C" BT_ENEMY_FLOOR *BtUraEnemyLayoutList[];
