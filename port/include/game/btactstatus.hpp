#pragma once

#include "common.h"

#include <libvu0.h>

/**
 * Defines what the player is doing in the dungeon this frame.
 */
struct BT_ACT_STATUS {
    s32           player_visible;    /**< 1 while the player character is drawn and animated; 0 in the first-person view. */
    s32           main_motion;       /**< Motion last handed to the player character, or -1 to hand it over again. */
    s32           hand_motion;       /**< Motion last handed to the first-person hand model, or -1 to hand it over again. */
    s32           motion_no;         /**< Motion the player character plays this frame. */
    s32           action_on;         /**< Whether an action is running. */
    s32           action_no;         /**< Which action is running. */
    s32           action_step;       /**< How far through the action the player is. */
    float         charge_time;       /**< How long the running action has been charged. */
    s32           recoil_frames;     /**< Frames left of the player's recoil from a hit, during which no further hit lands. */
    s32           invincible_frames; /**< Frames left in which no hit lands on the player. */
    s32           unk_028;
    s32           unk_02C;
    float         jump_velocity_x; /**< X component of the velocity a jumping action carries the player along. */
    float         jump_velocity_y; /**< Y component of the jump velocity, pulled down by gravity each frame. */
    float         jump_velocity_z; /**< Z component of the jump velocity. */
    float         unk_03C;
    s32           jumping;       /**< 1 while the jump velocity carries the player. */
    float         ground_height; /**< Height of the player above the ground beneath them. */
    float         action_gauge;  /**< Weapon action gauge, from 0 to 100, that special actions spend. */
    s32           gauge_flash;   /**< Frames left of the flash the action gauge shows on filling. */
    s32           unk_050;
    s32           shadow_visible; /**< 1 while the player character's shadow is drawn and animated. */
    s32           weapon_visible; /**< 1 while the player's weapon is drawn. */
    s32           charge_level;   /**< Charge stages the running action has reached; each flashes the character. */
    s32           guard_mode;     /**< 5 while the player holds a guard and may walk slowly; 0 otherwise. */
    s16           can_act;        /**< 1 while the player may use an item, start an event or begin an action. */
    s16           unk_066;
    s32           slow_walking; /**< 1 while the player walks slowly through a charge or guard, which stops the camera turning. */
    s32           camera_hold;  /**< 1 while the fallen player's message holds the camera still. */
    s32           action_busy;  /**< 1 while an item or special action holds the player, blocking the first-person view. */
    u8            unk_074[0xC];
    sceVu0FVECTOR target_position; /**< World position of the monster the player is locked on to. */
    s16           foot_sound;      /**< Foot sound set of the ground the player stands on. */
    s16           ground_kind;     /**< Kind of ground the player stands on. */
    s16           in_water;        /**< 1 while the player stands in water deep enough to stop actions. */
    s16           unk_096;
    s32           movement_locked; /**< 1 while a status ailment or a script holds the player still. */
    s32           in_presentation; /**< 1 while a pickup or escape presentation runs, which suppresses the player's status tint. */
    s32           gun_type;        /**< Which of Osmond's three firing styles his equipped gun uses. */
    s32           gauge_exhausted; /**< 1 once a gun has drained the action gauge, until it fills again. */
    s32           monstor_target;  /**< Character the monsters last hit, or -1. */
    u8            unk_0AC[8];
    float         move_x;              /**< World X component of the stick input, turned by the camera angle. */
    float         move_z;              /**< World Z component of the stick input, turned by the camera angle. */
    float         stick_strength;      /**< How far the stick is pushed. */
    sceVu0FVECTOR input_direction;     /**< Horizontal world direction the stick points. */
    sceVu0FVECTOR script_move_vector;  /**< Vector _GET_MOVE_VEC hands to a monster script. */
    s32           frames_since_attack; /**< Frames since the player last attacked, up to 3600; a guarding monster the player does not face reads it as 3600. */
    s32           swapped_on_death;    /**< 1 when a party member was changed in after one fell, giving the newcomer a spell of invincibility. */
    s32           combo_window;        /**< 1 while the running swing accepts the press for the next swing of a combo. */
    s32           unk_0EC;
    s32           combo_queued;  /**< 1 once the next swing of the combo has been asked for. */
    float         swing_heading; /**< Heading the next swing of the combo turns the player to. */
    s32           refill_frames; /**< Frames, up to ten, the action gauge has been refilling since the last combo press. */
    s32           unk_0FC;
    sceVu0FVECTOR move_vector;         /**< Specifies the way the player's action pushes them. */
    float         move_power;          /**< Specifies how hard the player's action pushes them along. */
    float         move_power_decay;    /**< Specifies how much move power one frame takes away. */
    float         camera_shake;        /**< Height the camera shakes by. */
    float         camera_shake_decay;  /**< Amount the camera shake loses each frame. */
    float         camera_shake_offset; /**< Height the shake adds to the camera this frame. */
    s32           camera_shake_frames; /**< Frames left of the camera shake. */
    s32           fast_camera_frames;  /**< Frames left in which the lock-on camera follows at its fast speed. */
    u8            unk_12C[0x18];
    s16           hud_shake_frames;   /**< Frames remaining in the status panel shake. */
    s16           hud_shake_y;        /**< Vertical pixel offset applied to the status panel shake. */
    s16           event_block_frames; /**< Frames left before the confirm button may start a map event again. */
    s16           swinging;           /**< Set each frame a special swing is in motion, stopping the action gauge refilling. */
    s16           special_cooldown;   /**< Frames before another special attack may start. */
    s16           unk_14E;
};

STATIC_ASSERT(sizeof(BT_ACT_STATUS) == 0x150);

/** What the player is doing in the dungeon this frame. */
extern "C" BT_ACT_STATUS BtActStatus;
