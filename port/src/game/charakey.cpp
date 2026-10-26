#pragma helper_mask_gpr 0x30
#pragma helper_mask_fpr 0x1000
#pragma name_counter 836
#pragma argument_flag 0
#pragma argument_flag_ones 189, 248, 267, 286, 342, 348, 410, 430, 470, 490
#pragma argument_flag_ones 540, 545, 617, 622, 693, 699, 746, 747, 749

#include "common.h"

/* The per-character special-attack key handlers: Toan's, Ungaga's and Goro's,
 * with the two helpers they share. */

#include <cmath>

#include "btactstatus.hpp"
#include "btmisc.hpp"
#include "character.hpp"
#include "clothread.hpp"
#include "collisiondata.hpp"
#include "dataread.hpp"
#include "dun/gameloop.hpp"
#include "frame.hpp"
#include "framevu1.hpp"
#include "gamepad.hpp"
#include "itemdata.hpp"
#include "motionmodel.hpp"
#include "shot_effect.hpp"
#include "shot_freefuncs.hpp"
#include "snd.hpp"
#include "userstatus.hpp"
#include "weaponeffect.hpp"

void sound_play(float from, float to, float now, int se_no) {
    if (now >= from && now <= to) {
        SndSePlay(se_no, -1, 0);
    }
}

void basic_damage(int kind, int owner) {
    NowColData->hit[NowColData->now_hit].vs_monster = NowWeaponHave->vs_monster;
    NowColData->hit[NowColData->now_hit].weapon_flags = NowWeaponHave->flags;

    s8              elem = NowWeaponHave->best_elem;
    CCollisionData *attr_col = NowColData;

    attr_col->hit[attr_col->now_hit].flags = GetWeaponElementAttr(elem);

    CCollisionData *owner_col = NowColData;

    owner_col->hit[owner_col->now_hit].owner = owner;
    owner_col->hit[owner_col->now_hit].attack_no = kind;
}

void ToanKey_On() {
    if (BtActStatus.special_cooldown > 0) {
        return;
    }

    if (BtActStatus.action_on == 0) {
        sceVu0FVECTOR rotation;

        BtActStatus.refill_frames = 0;
        BtActStatus.action_no = 0x24;
        BtActStatus.action_on = 1;
        BtActStatus.action_step = 0;
        BtActStatus.charge_time = 1.0f;
        BtActStatus.charge_level = 0;
        driveNoInterpolate = 1;
        CharaMain.motion_type.state.time = 136.0f;
        BtActStatus.combo_window = 0;
        BtActStatus.combo_queued = 0;
        BtActStatus.frames_since_attack = 0;
        CharaMain.GetRotation(rotation);
        BtActStatus.swing_heading = rotation[1];
    } else {
        if (BtActStatus.action_no == 0x24 && BtActStatus.combo_window != 0 && BtActStatus.action_gauge >= 20.0f) {
            BtActStatus.combo_queued = 1;
            BtActStatus.refill_frames = 0;
        }

        if (BtActStatus.action_no == 0x25 && BtActStatus.combo_window != 0 && BtActStatus.action_gauge >= 20.0f) {
            BtActStatus.combo_queued = 1;
            BtActStatus.refill_frames = 0;
        }

        if (BtActStatus.action_no == 0x26 && BtActStatus.combo_window != 0 && BtActStatus.action_gauge >= 20.0f) {
            BtActStatus.combo_queued = 1;
            BtActStatus.refill_frames = 0;
        }

        if (BtActStatus.action_no == 0x27 && BtActStatus.combo_window != 0 && BtActStatus.action_gauge >= 20.0f) {
            BtActStatus.combo_queued = 1;
            BtActStatus.refill_frames = 0;
        }
    }
}

void ToanKey_Play() {
    float time = CharaMain.motion_type.state.time;
    int   damage = NowWeaponHave->attack;

    if (StatusErrCheck(AILMENT_STAMINA) != 0) {
        damage *= 2;
    }

    int element_se = -1;

    if (NowWeaponHave->best_elem < 5) {
        element_se = NowWeaponHave->best_elem + 0x65;
    }

    if (element_se == 0x66) {
        element_se = 0x66;
    }

    sceVu0FVECTOR hit_pos;
    sceVu0FVECTOR chara_pos;

    getFramePos((CFrameVu1 *) NowWeapon->frame, "dcol1", hit_pos);
    sceVu0CopyVector(chara_pos, CharaMain.pos);

    if (time >= 825 && time <= 828.0f) {
        NowColData->Set(hit_pos, damage, 1, 2.8f, 0.0f, 2, 2, 0, 0);
        basic_damage(0, 0);
        NowColData->SetKickBack(chara_pos, 1.2f, 0.2f, 2);
    }

    if (time >= 833 && time <= 835) {
        NowColData->Set(hit_pos, damage, 1, 5.3f, 0.0f, 2, 2, 0, 0);
        basic_damage(0, 0);
        NowColData->SetKickBack(chara_pos, 1.2f, 0.2f, 2);
    }

    if (time >= 842 && time <= 844.0f) {
        NowColData->Set(hit_pos, damage, 1, 6.2f, 0.0f, 2, 2, 0, 0);
        basic_damage(0, 0);
        NowColData->SetKickBack(chara_pos, 1.5f, 0.3f, 2);
    }

    if (time >= 851 && time <= 854) {
        NowColData->Set(hit_pos, damage, 1, 6.2f, 0.0f, 2, 2, 0, 0);
        basic_damage(0, 0);
        NowColData->SetKickBack(chara_pos, 2.0f, 0.4f, 2);
    }

    if (time >= 862 && time <= 866) {
        damage *= 1.8f;
        NowColData->Set(hit_pos, damage, 1, 6.2f, 0.0f, 2, 2, 0, 0);
        basic_damage(0, 0);
        NowColData->SetKickBack(chara_pos, 3.0f, 0.3f, 2);
    }

    if (time >= 193.0f && time <= 196.0f) {
        damage *= 1.5;
        NowColData->Set(hit_pos, damage, 1, 6.0f, 0.0f, 2, 2, 0, 0);
        basic_damage(2, 0);
        NowColData->SetKickBack(chara_pos, 3.0f, 0.3f, 2);
    }

    if (time >= 720.0f && time <= 722) {
        damage *= 1.5;
        NowColData->Set(hit_pos, damage, 1, 12.0f, 0.0f, 2, 2, 0, 0);
        basic_damage(3, 0);
        NowColData->SetKickBack(chara_pos, 3.0f, 0.3f, 2);
    }

    int swing_motions[4] = {0x25, 0x26, 0x27, 0x28};

    for (int i = 0; i < 4; i++) {
        float swing_start = CharaMain.motion_type.motion_info[swing_motions[i]].start;
        float swing_end = CharaMain.motion_type.motion_info[swing_motions[i]].end;

        if (swing_start < time && swing_end > time) {
            BtActStatus.swinging = 1;
        }
    }

    sound_play(825, 825.3f, time, 0x190);
    sound_play(833, 833.3f, time, 0x191);
    sound_play(842, 842.3f, time, 0x190);
    sound_play(852.0f, 852.3f, time, 0x190);
    sound_play(864.0f, 864.3f, time, 0x191);
    sound_play(193.0f, 193.3f, time, 0x191);
    sound_play(717, 717.3f, time, 0x191);
    sound_play(825, 825.3f, time, 0x1A4);
    sound_play(833, 833.3f, time, 0x1A5);
    sound_play(842, 842.3f, time, 0x1A5);
    sound_play(852.0f, 852.3f, time, 0x1A4);
    sound_play(864.0f, 864.3f, time, 0x1A6);

    sceVu0FVECTOR forward = {0.0f, 0.0f, 1.0f, 1.0f};
    sceVu0FVECTOR rotation;
    sceVu0FMATRIX matrix;

    CharaMain.frame->GetRotation(rotation);
    sceVu0UnitMatrix(matrix);
    sceVu0RotMatrixY(matrix, matrix, rotation[1]);
    sceVu0ApplyMatrix(BtActStatus.move_vector, matrix, forward);

    if (BtActStatus.action_no == 0x24) {
        if (time >= 820.0f && time <= 820.5f) {
            BtActStatus.move_power = 0.17f;
            BtActStatus.move_power_decay = 0.0f;
        }

        BtActStatus.motion_no = 0x24;

        if (GamePad.On(PadInput_OK) == 0) {
            BtActStatus.action_step = 1;
        }

        float end = CharaMain.motion_type.motion_info[0x24].end;

        if (time >= 824.0f && time <= 825) {
            float tip = 64.0f;

            CWeaponFx.Set(8, 32.0f, tip);
        }

        BtActStatus.combo_window = 1;

        if (time >= end - 1.0f && time <= end) {
            if (BtActStatus.action_step == 0) {
                BtActStatus.action_no = 14;
                BtActStatus.motion_no = 14;
                BtActStatus.action_gauge = 0.0f;
                BtActStatus.move_power = 0.0f;
            } else if (BtActStatus.combo_queued == 1) {
                BtActStatus.action_gauge -= 25.0f;

                if (BtActStatus.action_gauge <= 0.0f) {
                    BtActStatus.action_gauge = 0.0f;
                }

                BtActStatus.action_no = 0x25;
                BtActStatus.motion_no = 0x25;
                driveNoInterpolate = 1;
                CharaMain.frame->SearchFrame("dcol1");
                BtActStatus.frames_since_attack = 0;
                BtActStatus.swing_heading = atan2f(BtActStatus.move_x, BtActStatus.move_z);
            } else {
                if (lockOnTargetFlag != 0) {
                    BtActStatus.motion_no = 0x12;
                }

                BtActStatus.unk_028 = 0;
                BtActStatus.motion_no = 0;
                BtActStatus.action_on = 0;
                BtActStatus.move_power = 0.0f;
                BtActStatus.move_power_decay = 0.0f;
                BtActStatus.special_cooldown = 15;
            }

            BtActStatus.combo_window = 0;
            BtActStatus.combo_queued = 0;
        }
    }

    if (BtActStatus.action_no == 0x25) {
        if (BtActStatus.stick_strength > 0.0f) {
            CharaMain.frame->SetRotation(0.0f, unitRotation((CFrameVu1 *) CharaMain.frame, BtActStatus.swing_heading), 0.0f);
        }

        BtActStatus.motion_no = 0x25;

        float end = CharaMain.motion_type.motion_info[0x25].end;

        if (time >= 832.0f && time <= 833) {
            CWeaponFx.Set(0xA, 32.0f, 32.0f);
        }

        BtActStatus.combo_window = 1;

        if (time >= end - 1.0f && time <= end) {
            if (BtActStatus.combo_queued == 1) {
                BtActStatus.action_gauge -= 25.0f;

                if (BtActStatus.action_gauge <= 0.0f) {
                    BtActStatus.action_gauge = 0.0f;
                }

                BtActStatus.action_no = 0x26;
                BtActStatus.motion_no = 0x26;
                driveNoInterpolate = 1;
                CharaMain.frame->SearchFrame("dcol1");
                BtActStatus.frames_since_attack = 0;
                BtActStatus.swing_heading = atan2f(BtActStatus.move_x, BtActStatus.move_z);
            } else {
                if (lockOnTargetFlag != 0) {
                    BtActStatus.motion_no = 0x12;
                }

                BtActStatus.unk_028 = 0;
                BtActStatus.motion_no = 0;
                BtActStatus.action_on = 0;
                BtActStatus.move_power = 0.0f;
                BtActStatus.move_power_decay = 0.0f;
            }

            BtActStatus.combo_window = 0;
            BtActStatus.combo_queued = 0;
        }
    }

    if (BtActStatus.action_no == 0x26) {
        if (BtActStatus.stick_strength > 0.0f) {
            CharaMain.frame->SetRotation(0.0f, unitRotation((CFrameVu1 *) CharaMain.frame, BtActStatus.swing_heading), 0.0f);
        }

        BtActStatus.motion_no = 0x26;

        float end = CharaMain.motion_type.motion_info[0x26].end;

        if (time >= 841 && time <= 841.5f) {
            CWeaponFx.Set(8, 32.0f, 64.0f);
        }

        BtActStatus.combo_window = 1;

        if (time >= end - 1.0f && time <= end) {
            if (BtActStatus.combo_queued == 1) {
                BtActStatus.action_gauge -= 25.0f;

                if (BtActStatus.action_gauge <= 0.0f) {
                    BtActStatus.action_gauge = 0.0f;
                }

                BtActStatus.action_no = 0x27;
                BtActStatus.motion_no = 0x27;
                driveNoInterpolate = 1;
                CharaMain.frame->SearchFrame("dcol1");
                BtActStatus.frames_since_attack = 0;
                BtActStatus.swing_heading = atan2f(BtActStatus.move_x, BtActStatus.move_z);
            } else {
                if (lockOnTargetFlag != 0) {
                    BtActStatus.motion_no = 0x12;
                }

                BtActStatus.unk_028 = 0;
                BtActStatus.motion_no = 0;
                BtActStatus.action_on = 0;
                BtActStatus.move_power = 0.0f;
                BtActStatus.move_power_decay = 0.0f;
            }

            BtActStatus.combo_window = 0;
            BtActStatus.combo_queued = 0;
        }
    }

    if (BtActStatus.action_no == 0x27) {
        if (BtActStatus.stick_strength > 0.0f) {
            CharaMain.frame->SetRotation(0.0f, unitRotation((CFrameVu1 *) CharaMain.frame, BtActStatus.swing_heading), 0.0f);
        }

        BtActStatus.motion_no = 0x27;

        float end = CharaMain.motion_type.motion_info[0x27].end;

        if (time >= 849 && time <= 849.5f) {
            CWeaponFx.Set(0xA, 32.0f, 32.0f);
        }

        BtActStatus.combo_window = 1;

        if (time >= end - 1.0f && time <= end) {
            if (BtActStatus.combo_queued == 1) {
                BtActStatus.action_gauge -= 25.0f;

                if (BtActStatus.action_gauge <= 0.0f) {
                    BtActStatus.action_gauge = 0.0f;
                }

                BtActStatus.action_no = 0x28;
                BtActStatus.motion_no = 0x28;
                driveNoInterpolate = 1;
                CharaMain.frame->SearchFrame("dcol1");
                BtActStatus.frames_since_attack = 0;
            } else {
                if (lockOnTargetFlag != 0) {
                    BtActStatus.motion_no = 0x12;
                }

                BtActStatus.unk_028 = 0;
                BtActStatus.motion_no = 0;
                BtActStatus.action_on = 0;
                BtActStatus.move_power = 0.0f;
                BtActStatus.move_power_decay = 0.0f;
            }

            BtActStatus.combo_window = 0;
            BtActStatus.combo_queued = 0;
        }
    }

    if (BtActStatus.action_no == 0x28) {
        if (time >= 867.5f && time <= 868.0f) {
            BtActStatus.move_power = 0.0f;
            BtActStatus.move_power_decay = 0.0f;
        }

        BtActStatus.motion_no = 0x28;

        if (time >= 861 && time <= 861.5f) {
            CWeaponFx.Set(0xE, 24.0f, 32.0f);
        }

        float end = CharaMain.motion_type.motion_info[0x28].end;

        if (time >= end - 1.0f && time <= end) {
            if (lockOnTargetFlag != 0) {
                BtActStatus.motion_no = 0x12;
            }

            BtActStatus.unk_028 = 0;
            BtActStatus.motion_no = 0;
            BtActStatus.action_on = 0;
            BtActStatus.move_power = 0.0f;
            BtActStatus.move_power_decay = 0.0f;
            BtActStatus.combo_window = 0;
            BtActStatus.combo_queued = 0;
        }
    }

    if (BtActStatus.action_no == 14) {
        if (GamePad.On(PadInput_OK) == 0) {
            BtActStatus.action_step = 1;
        }

        BtActStatus.motion_no = 14;
        BtActStatus.charge_time += 0.016666667f;

        if (BtActStatus.charge_time >= 1.5f && BtActStatus.charge_level == 0) {
            BtActStatus.charge_level = 1;
            setUnitAmbientAnime(15.0f, 1.0f, 0.0f, 122.0f, 208.0f);
        }

        if (UserStatus->skill_owned[0] != 0 && BtActStatus.charge_time >= 2.5f && BtActStatus.charge_level == 1) {
            BtActStatus.charge_level = 2;
            setUnitAmbientAnime(15.0f, 1.0f, 0.0f, 122.0f, 208.0f);
        }

        if (BtActStatus.action_step == 1) {
            if (BtActStatus.charge_time >= 1.5f) {
                int charged = 0;

                if (UserStatus->skill_owned[0] != 0 && BtActStatus.charge_time >= 2.5f) {
                    charged = 1;
                }

                if (BtActStatus.charge_time >= 3.0f) {
                    BtActStatus.charge_time = 3.0f;
                }

                if (charged == 0) {
                    BtActStatus.action_no = 0xF;
                    BtActStatus.motion_no = 0xF;
                    driveNoInterpolate = 1;
                    SwordDmgCheck1(2.0f, 0);
                }

                if (charged != 0) {
                    sceVu0FVECTOR shot_pos;
                    sceVu0FVECTOR shot_vector = {0.0001f, 0.9998f, 0.0001f, 1.0f};

                    sceVu0CopyVector(shot_pos, CharaMain.pos);
                    NowMainEffect->Set(shot_pos, shot_vector, 0, -1, 0, NULL, -1);
                    NowMainEffect->SetVsMonster(NowWeaponHave->vs_monster);
                    NowMainEffect->SetWepStatus(NowWeaponHave->flags);
                    NowMainEffect->SetDmg((int) (1.5f * (float) damage));
                    NowMainEffect->SetWait(45);
                    NowMainEffect->SetAttribute(GetWeaponElementAttr(NowWeaponHave->best_elem));
                    SwordDmgCheck1(3.0f, 0);
                    BtActStatus.action_no = 0x18;
                    BtActStatus.motion_no = 0x18;
                    driveNoInterpolate = 1;
                    SndSePlay(SE_CHARA_SHOUT_4, -1, 0);
                }
            } else {
                if (lockOnTargetFlag != 0) {
                    BtActStatus.motion_no = 0x12;
                }

                BtActStatus.unk_028 = 0;
                BtActStatus.motion_no = 0;
                BtActStatus.action_on = 0;
            }
        }
    }

    if (BtActStatus.action_no == 0x18) {
        if (time >= 720.0f && time <= 720.5f) {
            CWeaponFx.Set(0x1E, 28.0f, 16.0f);
        }

        BtActStatus.invincible_frames = 0x1770;
        BtActStatus.action_busy = 1;
        BtActStatus.can_act = 0;
        BtActStatus.motion_no = 0x18;

        float end = CharaMain.motion_type.motion_info[0x18].end;

        if (time >= end - 1.0f && time <= end) {
            if (lockOnTargetFlag != 0) {
                BtActStatus.motion_no = 0x12;
            }

            BtActStatus.unk_028 = 0;
            BtActStatus.motion_no = 0;
            BtActStatus.action_on = 0;
            BtActStatus.invincible_frames = 0;
            BtActStatus.action_busy = 0;
            BtActStatus.can_act = 1;
        }
    }

    if (BtActStatus.action_no == 0xF) {
        BtActStatus.motion_no = 0xF;
        BtActStatus.invincible_frames = 0x1770;
        BtActStatus.action_busy = 1;
        BtActStatus.can_act = 0;

        if (time >= 187.0f && time < 188.0f) {
            sceVu0FVECTOR shot_from;
            sceVu0FVECTOR shot_to;

            sceVu0CopyVector(shot_from, CharaMain.pos);
            getCharacterVector(shot_to, 0.0f);
            sceVu0ScaleVectorXYZ(shot_to, shot_to, 16.0f);
            shot_to[0] += shot_from[0];
            shot_to[1] = shot_from[1];
            shot_to[2] += shot_from[2];
            BtActStatus.jumping = 1;
            ParabolicInitialVector(&BtActStatus.jump_velocity_x, shot_from, shot_to, 0.1f, 40.0f);
            BtActStatus.jump_velocity_z = 0.0f;
            BtActStatus.jump_velocity_x = 0.0f;
        }

        float end = CharaMain.motion_type.motion_info[15].end;

        if (time >= end - 1.0f && time <= end) {
            BtActStatus.action_no = 0x10;
            BtActStatus.motion_no = 0x10;
            driveNoInterpolate = 1;
            CharaMain.motion_type.state.time = CharaMain.motion_type.motion_info[16].start;
        }
    }

    if (BtActStatus.action_no == 0x10) {
        BtActStatus.motion_no = 0x10;

        if (BtActStatus.jump_velocity_y <= -0.4f) {
            BtActStatus.action_no = 3;
            driveNoInterpolate = 1;
            CWeaponFx.Set(0x14, 16.0f, 64.0f);
            CharaMain.motion_type.state.time = CharaMain.motion_type.motion_info[3].start;
            SndSePlay(SE_CHARA_SHOUT_3, -1, 0);
        }
    }

    if (BtActStatus.action_no == 3) {
        BtActStatus.motion_no = 3;

        float end = CharaMain.motion_type.motion_info[3].end;

        if (time >= end - 1.0f && time <= end) {
            BtActStatus.action_no = 0x19;
            driveNoInterpolate = 1;
            CharaMain.motion_type.state.time = CharaMain.motion_type.motion_info[0x19].start;
        }
    }

    if (BtActStatus.action_no == 0x19) {
        BtActStatus.motion_no = 0x19;

        if (BtActStatus.ground_height < 2.0f) {
            BtActStatus.action_no = 0x11;
            BtActStatus.jumping = 0;
            driveNoInterpolate = 1;
            CharaMain.motion_type.state.time = CharaMain.motion_type.motion_info[0x11].start;
        }
    }

    if (BtActStatus.action_no == 0x11) {
        BtActStatus.motion_no = 0x11;

        float end = CharaMain.motion_type.motion_info[0x11].end;

        if (time >= end - 1.0f && time <= end) {
            if (lockOnTargetFlag != 0) {
                BtActStatus.motion_no = 0x12;
            }

            BtActStatus.unk_028 = 0;
            BtActStatus.motion_no = 0;
            BtActStatus.action_on = 0;
            BtActStatus.invincible_frames = 0;
            BtActStatus.action_busy = 0;
            BtActStatus.can_act = 1;
        }
    }
}

void UngagaKey_On() {
    if (BtActStatus.special_cooldown > 0) {
        return;
    }

    CUserStatus *status = UserStatus;

    status->skill_owned[4] = 1;

    if (BtActStatus.action_on == 0) {
        sceVu0FVECTOR rotation;

        BtActStatus.refill_frames = 0;
        BtActStatus.action_no = 0x25;
        BtActStatus.action_on = 1;
        BtActStatus.action_step = 0;
        BtActStatus.charge_time = 1.0f;
        BtActStatus.charge_level = 0;
        driveNoInterpolate = 1;
        CharaMain.motion_type.state.time = 136.0f;
        BtActStatus.combo_window = 0;
        BtActStatus.combo_queued = 0;
        BtActStatus.frames_since_attack = 0;
        CharaMain.GetRotation(rotation);
        BtActStatus.swing_heading = rotation[1];
    } else {
        if (BtActStatus.action_no == 0x25 && BtActStatus.combo_window != 0 && BtActStatus.action_gauge >= 40.0f) {
            BtActStatus.combo_queued = 1;
            BtActStatus.refill_frames = 0;
        }

        if (BtActStatus.action_no == 0x26 && BtActStatus.combo_window != 0 && BtActStatus.action_gauge >= 40.0f) {
            BtActStatus.combo_queued = 1;
            BtActStatus.refill_frames = 0;
        }
    }
}

void UngagaKey_Play() {
    float         time = CharaMain.motion_type.state.time;
    sceVu0FVECTOR hit_left;
    sceVu0FVECTOR hit_right;
    sceVu0FVECTOR chara_pos;

    getFramePos((CFrameVu1 *) NowWeapon->frame, "dcol0", hit_left);
    getFramePos((CFrameVu1 *) NowWeapon->frame, "dcol1", hit_right);
    sceVu0CopyVector(chara_pos, CharaMain.pos);

    int element_se;

    if (NowWeaponHave->best_elem < 5) {
        element_se = NowWeaponHave->best_elem + 0x65;
    }

    int damage = NowWeaponHave->attack;

    if (StatusErrCheck(AILMENT_STAMINA) != 0) {
        damage *= 2;
    }

    if (time >= 674 && time <= 677) {
        NowColData->Set(hit_left, damage, 1, 6.0f, 0, 2, 2, 0, 0);
        basic_damage(0, 4);
        NowColData->SetKickBack(chara_pos, 1.2f, 0.1f, 2);
        NowColData->Set(hit_right, damage, 1, 6.0f, 0.0f, 2, 2, 0, 0);
        basic_damage(0, 4);
        NowColData->SetKickBack(chara_pos, 1.2f, 0.1f, 2);
    }

    if (time >= 686 && time <= 692.0f) {
        NowColData->Set(hit_left, damage, 1, 6.0f, 0.0f, 2, 2, 0, 0);
        basic_damage(0, 4);
        NowColData->SetKickBack(chara_pos, 1.5f, 0.1f, 2);
        NowColData->Set(hit_right, damage, 1, 6.0f, 0.0f, 2, 2, 0, 0);
        basic_damage(0, 4);
        NowColData->SetKickBack(chara_pos, 1.5f, 0.1f, 2);
    }

    if (time >= 700.0f && time <= 704.0f) {
        NowColData->Set(hit_left, damage, 1, 6.0f, 0.0f, 2, 2, 0, 0);
        basic_damage(0, 4);
        NowColData->SetKickBack(chara_pos, 2.0f, 0.1f, 2);
        NowColData->Set(hit_right, damage, 1, 6.0f, 0.0f, 2, 2, 0, 0);
        basic_damage(0, 4);
        NowColData->SetKickBack(chara_pos, 2.0f, 0.1f, 2);
    }

    int swing_motions[3] = {0x25, 0x26, 0x27};

    for (int i = 0; i < 3; i++) {
        float swing_start = CharaMain.motion_type.motion_info[swing_motions[i]].start;
        float swing_end = CharaMain.motion_type.motion_info[swing_motions[i]].end;

        if (swing_start < time && swing_end > time) {
            BtActStatus.swinging = 1;
        }
    }

    if (time >= 674 && time <= 674.5f) {
        CWeaponFx.Set(0x10, 16.0f, 64.0f);
    }

    if (time >= 686 && time <= 686.5f) {
        CWeaponFx.Set(0x10, 16.0f, 64.0f);
    }

    if (time >= 699 && time <= 699.5f) {
        CWeaponFx.Set(0x10, 16.0f, 64.0f);
    }

    sound_play(675, 675.3f, time, 0x1A4);
    sound_play(687, 687.3f, time, 0x1A5);
    sound_play(700.0f, 700.3f, time, 0x1A5);

    sceVu0FVECTOR forward = {0.0f, 0.0f, 1.0f, 1.0f};
    sceVu0FVECTOR rotation;
    sceVu0FMATRIX matrix;

    CharaMain.frame->GetRotation(rotation);
    sceVu0UnitMatrix(matrix);
    sceVu0RotMatrixY(matrix, matrix, rotation[1]);
    sceVu0ApplyMatrix(BtActStatus.move_vector, matrix, forward);

    if (BtActStatus.action_no == 0x25) {
        if (GamePad.On(PadInput_OK) == 0) {
            BtActStatus.action_step = 1;
        }

        BtActStatus.motion_no = 0x25;

        float end = CharaMain.motion_type.motion_info[0x25].end;

        BtActStatus.combo_window = 1;

        if (time >= end - 1.0f && time <= end) {
            if (BtActStatus.combo_queued == 1) {
                BtActStatus.action_gauge -= 20.0f;

                if (BtActStatus.action_gauge <= 0.0f) {
                    BtActStatus.action_gauge = 0.0f;
                }

                BtActStatus.action_no = 0x26;
                BtActStatus.motion_no = 0x26;
                driveNoInterpolate = 1;
                BtActStatus.frames_since_attack = 0;
                BtActStatus.combo_window = 0;
                BtActStatus.combo_queued = 0;
                BtActStatus.swing_heading = atan2f(BtActStatus.move_x, BtActStatus.move_z);
            } else {
                if (BtActStatus.action_step == 0) {
                    BtActStatus.motion_no = 13;
                    BtActStatus.action_no = 13;
                    driveNoInterpolate = 1;
                    return;
                }

                if (lockOnTargetFlag != 0) {
                    BtActStatus.motion_no = 0x12;
                }

                BtActStatus.unk_028 = 0;
                BtActStatus.motion_no = 0;
                BtActStatus.action_on = 0;
                BtActStatus.move_power = 0.0f;
                BtActStatus.move_power_decay = 0.0f;
                BtActStatus.special_cooldown = 20;
            }
        }
    }

    if (BtActStatus.action_no == 0x26) {
        if (time >= 679 && time <= 679.5f) {
            BtActStatus.move_power = 0.25f;
            BtActStatus.move_power_decay = 0.0f;
        }

        if (GamePad.On(PadInput_OK) == 0) {
            BtActStatus.action_step = 1;
        }

        BtActStatus.motion_no = 0x26;

        float end = CharaMain.motion_type.motion_info[0x26].end;

        BtActStatus.combo_window = 1;

        if (time >= end - 0.5f && time <= end) {
            if (BtActStatus.combo_queued == 1) {
                BtActStatus.action_gauge -= 20.0f;

                if (BtActStatus.action_gauge <= 0.0f) {
                    BtActStatus.action_gauge = 0.0f;
                }

                BtActStatus.action_no = 0x27;
                BtActStatus.motion_no = 0x27;
                driveNoInterpolate = 1;
                BtActStatus.frames_since_attack = 0;
                BtActStatus.combo_window = 0;
                BtActStatus.combo_queued = 0;
            } else {
                if (lockOnTargetFlag != 0) {
                    BtActStatus.motion_no = 0x12;
                }

                BtActStatus.unk_028 = 0;
                BtActStatus.motion_no = 0;
                BtActStatus.action_on = 0;
                BtActStatus.move_power = 0.0f;
                BtActStatus.move_power_decay = 0.0f;
            }
        }
    }

    if (BtActStatus.action_no == 0x27) {
        if (time >= 699 && time <= 699.5f) {
            BtActStatus.move_power = 0.0f;
            BtActStatus.move_power_decay = 0.0f;
        }

        if (GamePad.On(PadInput_OK) == 0) {
            BtActStatus.action_step = 1;
        }

        BtActStatus.motion_no = 0x27;

        float end = CharaMain.motion_type.motion_info[0x27].end;

        BtActStatus.combo_window = 1;

        if (time >= end - 1.0f && time <= end) {
            if (lockOnTargetFlag != 0) {
                BtActStatus.motion_no = 0x12;
            }

            BtActStatus.unk_028 = 0;
            BtActStatus.motion_no = 0;
            BtActStatus.action_on = 0;
            BtActStatus.move_power = 0.0f;
            BtActStatus.move_power_decay = 0.0f;
        }
    }

    if (BtActStatus.action_no == 13) {
        if (GamePad.On(PadInput_OK) == 0) {
            BtActStatus.action_step = 1;
        }

        BtActStatus.motion_no = 13;

        if (BtActStatus.action_step != 0) {
            BtActStatus.unk_028 = 0;
            BtActStatus.motion_no = 0;
            BtActStatus.action_on = 0;

            if (lockOnTargetFlag != 0) {
                BtActStatus.motion_no = 0x12;
            }
        }

        float end = CharaMain.motion_type.motion_info[13].end;

        if (time >= end - 2.0f && time <= end) {
            BtActStatus.motion_no = 14;
            BtActStatus.action_no = 14;
            SndSePlay(SE_CHARA_SHOUT_3, -1, 0);
        }
    }

    if (BtActStatus.action_no == 14) {
        if (GamePad.On(PadInput_OK) == 0) {
            BtActStatus.action_step = 1;
        }

        BtActStatus.motion_no = 14;

        static int wait;
        static s8  init;

        if (init == 0) {
            wait = 0;
            init = 1;
        }

        wait++;

        if (wait >= 30) {
            sceVu0FVECTOR effect_pos;
            sceVu0FVECTOR effect_vector;

            SndSeSeqPlayStop(0x192, 30, 1);
            wait = 0;
            getFramePos((CFrameVu1 *) CharaMain.frame, "weapon", effect_pos);
            getCharacterVector(effect_vector, 0.0f);
            effect_vector[0] += effect_pos[0];
            effect_vector[1] += effect_pos[1];
            effect_vector[2] += effect_pos[2];
            NowMainEffect->Set(effect_pos, effect_vector, 4, 2, 0, NULL, -1);
            NowMainEffect->SetVsMonster(NowWeaponHave->vs_monster);
            NowMainEffect->SetWepStatus(NowWeaponHave->flags);
            NowMainEffect->SetAttribute(GetWeaponElementAttr(NowWeaponHave->best_elem));
            NowMainEffect->SetDmg((int) (1.5f * (float) damage));
            NowMainEffect->SetWait(45);
            SwordDmgCheck1(0.8f, 0);
        }

        if (BtActStatus.action_step != 0) {
            BtActStatus.motion_no = 0xF;
            BtActStatus.action_no = 0xF;
        }
    }

    if (BtActStatus.action_no == 0xF) {
        BtActStatus.motion_no = 0xF;

        float end = CharaMain.motion_type.motion_info[15].end;

        if (time >= end - 2.0f && time <= end) {
            BtActStatus.unk_028 = 0;
            BtActStatus.motion_no = 0;
            BtActStatus.action_on = 0;

            if (lockOnTargetFlag != 0) {
                BtActStatus.motion_no = 0x12;
            }

            NowMainEffect->EndEffect();
        }
    }
}

void GoroKey_On() {
    if (BtActStatus.action_on == 0) {
        if (BtActStatus.action_gauge >= 100.0f) {
            sceVu0FVECTOR rotation;

            BtActStatus.action_no = 0x24;
            BtActStatus.action_on = 1;
            BtActStatus.action_step = 0;
            BtActStatus.charge_time = 1.0f;
            BtActStatus.action_gauge = 0.0f;
            BtActStatus.charge_level = 0;
            driveNoInterpolate = 1;
            CharaMain.motion_type.state.time = 136.0f;
            BtActStatus.combo_window = 0;
            BtActStatus.combo_queued = 0;
            BtActStatus.frames_since_attack = 0;
            CharaMain.GetRotation(rotation);
            BtActStatus.swing_heading = rotation[1];
        }

        return;
    }
}

void GoroKey_Play() {
    float time = CharaMain.motion_type.state.time;
    int   damage = NowWeaponHave->attack;

    if (StatusErrCheck(AILMENT_STAMINA) != 0) {
        damage *= 2;
    }

    int element_se = -1;

    if (NowWeaponHave->best_elem < 5) {
        element_se = NowWeaponHave->best_elem + 0x65;
    }

    if (element_se == 0x66) {
        element_se = 0x66;
    }

    sceVu0FVECTOR shot_vector = {0.001f, 1.0f, 0.001f, 1.0f};
    sceVu0FVECTOR hit_pos;
    sceVu0FVECTOR chara_pos;

    getFramePos((CFrameVu1 *) NowWeapon->frame, "dcol0", hit_pos);
    sceVu0CopyVector(chara_pos, CharaMain.pos);

    if (time >= 99.0f && time <= 102.0f) {
        NowColData->Set(hit_pos, damage, 1, 5.0f, 0.0f, 2, 2, 0, 0);
        basic_damage(0, 2);
        NowColData->SetKickBack(chara_pos, 2.5f, 0.1f, 2);
    }

    if (time >= 462.0f && time <= 482.0f) {
        damage *= 1.5;
        NowColData->Set(hit_pos, damage, 1, 5.0f, 0.0f, 2, 2, 0, 0);

        NowColData->hit[NowColData->now_hit].vs_monster = NowWeaponHave->vs_monster;
        NowColData->hit[NowColData->now_hit].weapon_flags = NowWeaponHave->flags;

        s8              elem = NowWeaponHave->best_elem;
        CCollisionData *attr_col = NowColData;

        attr_col->hit[attr_col->now_hit].flags = GetWeaponElementAttr(elem);

        CCollisionData *owner_col = NowColData;

        owner_col->hit[owner_col->now_hit].owner = 2;
        owner_col->hit[owner_col->now_hit].attack_no = 1;
    }

    if (time >= 482.0f && time < 482.4f) {
        NowMainEffect->Set(hit_pos, shot_vector, 2, -1, 0, NULL, -1);
        NowMainEffect->SetWepStatus(NowWeaponHave->flags);
        NowMainEffect->SetVsMonster(NowWeaponHave->vs_monster);
        NowMainEffect->SetDmg((int) (1.2f * (float) damage));
        NowMainEffect->SetWait(45);
        NowMainEffect->SetAttribute(GetWeaponElementAttr(NowWeaponHave->best_elem));
        NowColData->SetKickBack(chara_pos, 2.5f, 0.1f, 2);
        BtActStatus.camera_shake = 8.0f;
        BtActStatus.camera_shake_decay = 0.1f;
        BtActStatus.camera_shake_frames = 30;
    }

    if (time >= 103.0f && time < 103.4f) {
        BtActStatus.camera_shake = 2.0f;
        BtActStatus.camera_shake_decay = 0.01f;
        BtActStatus.camera_shake_frames = 20;
    }

    if (time >= 98.0f && time < 98.3f) {
        SndSePlay(SE_CHARA_SHOUT, -1, 0);
    }

    if (time >= 471 && time < 471.4f) {
        SndSePlay(SE_CHARA_SHOUT_2, -1, 0);
    }

    if (time >= 471 && time < 471.4f) {
        SndSePlay(SE_CHARA_ACTION_3, -1, 0);
    }

    if (time >= 478.0f && time < 478.4f) {
        SndSePlay(SE_CHARA_ACTION_3, -1, 0);
    }

    if (time >= 481 && time < 481.4f) {
        SndSePlay(SE_CHARA_ACTION_4, -1, 0);
    }

    sceVu0FVECTOR forward = {0.0f, 0.0f, 1.0f, 1.0f};
    sceVu0FVECTOR rotation;
    sceVu0FMATRIX matrix;

    CharaMain.frame->GetRotation(rotation);
    sceVu0UnitMatrix(matrix);
    sceVu0RotMatrixY(matrix, matrix, rotation[1]);
    sceVu0ApplyMatrix(BtActStatus.move_vector, matrix, forward);

    if (BtActStatus.action_no == 0x24) {
        if (time >= 94.5f && time <= 95.0f) {
            BtActStatus.move_power = 0.25f;
            BtActStatus.move_power_decay = 0.01f;
        }

        BtActStatus.motion_no = 0x24;

        if (GamePad.On(PadInput_OK) == 0) {
            BtActStatus.action_step = 1;
        }

        float end = CharaMain.motion_type.motion_info[36].end;

        BtActStatus.combo_window = 1;

        if (time >= 95.0f && time <= 95.5f && BtActStatus.action_step == 0) {
            BtActStatus.action_step = 0;
            BtActStatus.motion_no = 13;
            BtActStatus.action_no = 13;
            BtActStatus.move_power = 0.0f;
            BtActStatus.move_power_decay = 0.0f;
            return;
        }

        if (time >= end - 1.0f && time <= end) {
            if (lockOnTargetFlag != 0) {
                BtActStatus.motion_no = 0x12;
            }

            BtActStatus.unk_028 = 0;
            BtActStatus.motion_no = 0;
            BtActStatus.action_on = 0;
            BtActStatus.move_power = 0.0f;
            BtActStatus.move_power_decay = 0.0f;
            BtActStatus.combo_window = 0;
            BtActStatus.combo_queued = 0;
        }
    }

    if (BtActStatus.action_no == 13) {
        BtActStatus.action_step++;

        if (BtActStatus.action_step >= 45) {
            if (BtActStatus.action_step == 45) {
                setUnitAmbientAnime(15.0f, 1.0f, 0.0f, 122.0f, 208.0f);
            }

            BtActStatus.action_step = 46;
        }

        BtActStatus.motion_no = 13;

        if (GamePad.On(PadInput_OK) == 0) {
            if (BtActStatus.action_step < 45) {
                sceVu0FVECTOR restart_rotation;

                BtActStatus.action_on = 1;
                BtActStatus.action_step = 0;
                BtActStatus.charge_time = 1.0f;
                BtActStatus.action_gauge -= 15.0f;
                BtActStatus.charge_level = 0;
                BtActStatus.combo_window = 0;
                BtActStatus.combo_queued = 0;
                BtActStatus.frames_since_attack = 0;
                CharaMain.GetRotation(restart_rotation);
                BtActStatus.swing_heading = restart_rotation[1];
                BtActStatus.action_no = 0x24;
            } else {
                BtActStatus.action_no = 14;
                BtActStatus.action_gauge = 0.0f;
                BtActStatus.invincible_frames = 1;
                BtActStatus.action_busy = 1;
                BtActStatus.can_act = 0;
                BtActStatus.invincible_frames = 0x3E7;
                SwordDmgCheck1(2.0f, 0);
                SndSePlay(SE_GORO_SMASH, -1, 0);
            }
        }
    }

    if (BtActStatus.action_no == 14) {
        BtActStatus.motion_no = 14;

        float end = CharaMain.motion_type.motion_info[14].end;

        if (time >= end - 1.0f && time <= end) {
            BtActStatus.unk_028 = 0;
            BtActStatus.motion_no = 0;
            BtActStatus.action_on = 0;
            BtActStatus.invincible_frames = 0;
            BtActStatus.action_busy = 0;
            BtActStatus.invincible_frames = 0;
            BtActStatus.can_act = 1;

            if (lockOnTargetFlag != 0) {
                BtActStatus.motion_no = 0x12;
            }
        }
    }
}
