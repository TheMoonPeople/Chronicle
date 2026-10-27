// DunMoveChara, split from dun/gameloop.cpp for size.

#include "dun/gameloop.hpp"

#include <libvu0.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "app/gameloop.hpp"
#include "battlemenu.hpp"
#include "btactstatus.hpp"
#include "btitem.hpp"
#include "btmisc.hpp"
#include "btsysscript.hpp"
#include "camera.hpp"
#include "camerafollow.hpp"
#include "charakey.hpp"
#include "character.hpp"
#include "clothread.hpp"
#include "clsmes.hpp"
#include "collision.hpp"
#include "collisiondata.hpp"
#include "dataalloc.hpp"
#include "dataread.hpp"
#include "dataset.hpp"
#include "dispctrl.hpp"
#include "dngmessageman.hpp"
#include "dngstatusdata.hpp"
#include "dranmapfield.hpp"
#include "dungeoneventman.hpp"
#include "dungeonmap.hpp"
#include "edit.hpp"
#include "editloop3.hpp"
#include "frame.hpp"
#include "frameattr.hpp"
#include "gamemode.hpp"
#include "gamepad.hpp"
#include "gameutil.hpp"
#include "healeffect.hpp"
#include "hit_machingun_effect.hpp"
#include "hitmark.hpp"
#include "hitvalue.hpp"
#include "input/camera_zoom.hpp"
#include "input/first_person_walk.hpp"
#include "input/mouse_look.hpp"
#include "itembombeffect.hpp"
#include "itemdata.hpp"
#include "main.hpp"
#include "mainitemmodel.hpp"
#include "mainselect.hpp"
#include "mathutil.hpp"
#include "mds.hpp"
#include "menu_draw.hpp"
#include "menu_dungeon.hpp"
#include "menu_misc.hpp"
#include "menu_save.hpp"
#include "menuitemstep.hpp"
#include "mglib.hpp"
#include "monstorunit.hpp"
#include "motionmodel.hpp"
#include "nowload.hpp"
#include "npcharacter.hpp"
#include "objanime.hpp"
#include "platform/config.hpp"
#include "platform/input.hpp"
#include "randomitem.hpp"
#include "rect.hpp"
#include "runeffect.hpp"
#include "runscript_opcodes.hpp"
#include "savedata.hpp"
#include "shot_effect.hpp"
#include "shot_effect_pack.hpp"
#include "shot_firebar.hpp"
#include "shot_freefuncs.hpp"
#include "snd.hpp"
#include "stealitem.hpp"
#include "sysmes.hpp"
#include "texture.hpp"
#include "textureanime.hpp"
#include "userstatus.hpp"
#include "vutext.hpp"
#include "weaponeffect.hpp"
#include "weaponelement.hpp"
#include "wind.hpp"

#pragma clang diagnostic ignored "-Wwritable-strings"
#pragma clang diagnostic ignored "-Wchar-subscripts"
#pragma clang diagnostic ignored "-Wunused-value"

namespace dc::game {

using namespace dc::input;
using namespace dc::platform;

namespace {

// The lock-on taken into the quick change (game.swap_keeps_lock_on), or -1.
s32 quickChangeLockOn = -1;
s32 quickChangeShiftNo;
s32 quickChangeShiftRot;

void DeleteItemModel(int slot) {
    if (activeItem.model[slot] != -1) {
        activeItem.models->DeleteModel(activeItem.model[slot]);
        activeItem.model[slot] = -1;
    }
}

} // namespace

void DunMoveChara() {
    // Where the camera looks relative to what it follows, before the floor's own offset is added.
    static sceVu0FVECTOR reference = {0.0f, 7.5f, 0.0f, 0.0f};
    sceVu0FVECTOR        pos;
    float                move_z = 0.0f;
    float                move_x = 0.0f;
    float                ly = 0.0f;
    float                lx = 0.0f;
    float                angle = 0.0f;
    float                rx = 0.0f;
    float                ry = 0.0f;
    CFrame              *collision = nullptr;
    int                  entry_no = 0;

    // An event that sets its own draw distance keeps it; anything else draws
    // the whole floor.
    if (BtEventMode != 0 && EdEventInfo.projection > 0.0f) {
        MGSetRenderInfo(EdEventInfo.projection, 1.0f, static_cast<float>(0xFFFF));
    } else {
        MGSetRenderInfo(800.0f, 1.0f, static_cast<float>(0xFFFF));
    }

    sceVu0CopyVector(pos, CharaFrame->position);
    angle = NowCamera__3->GetAngleH();
    GameEnv.camera = NowCamera__3;
    GameEnv.frame = CharaFrame;
    lx = GamePad.GetLXf();
    inputH1 = lx;
    ly = GamePad.GetLYf();
    rx = GamePad.GetRXf();
    ry = GamePad.GetRYf();
    {
        sceVu0FVECTOR forward = {0.0f, 0.0f, 1.0f, 1.0f};
        sceVu0FVECTOR rotation;
        sceVu0FVECTOR dir;
        sceVu0FMATRIX rot_matrix;

        CharaMain.frame->GetRotation(rotation);
        sceVu0UnitMatrix(rot_matrix);
        sceVu0RotMatrixY(rot_matrix, rot_matrix, rotation[1]);
        sceVu0ApplyMatrix(dir, rot_matrix, forward);
        sceVu0Normalize(BtActStatus.input_direction, dir);
    }
    // The stick is read in screen space, so it turns with the camera before
    // anything acts on it.
    BtActStatus.input_direction[0] = lx * cos(angle) + ly * sinf(angle);
    BtActStatus.input_direction[2] = ly * cos(angle) - lx * sinf(angle);
    BtActStatus.input_direction[1] = 0.0f;
    BtActStatus.input_direction[3] = 1.0f;

    if (GamePad.Down2(PAD_CROSS) != 0) {
        driveStepHold ^= 1;
    }

    if (GamePad.Down2(PAD_SQUARE) != 0) {
        BtAllClear ^= 1;
        MesAbsDrawOff = BtAllClear;
    }

    GamePad.Down(PAD_SELECT);

    switch (gameTask) {
        case GAME_TASK_PLAY:
            if (CheckTrialEnd() != 0) {
                tryalExit = 1;
            } else if (GamePad.Down(PAD_START) != 0) {
                printf("pause!!\n");
                exitMenuFlag = 1;
                driveStepHold = 1;
                CMonUnitHold = 1;
                CEffectHold = 1;
                PlayTimeCountFlag(0);
                SndSePlay(MENU_SOUND_CONFIRM, -1, 0);
                gameTask = GAME_TASK_PAUSE;
            } else {
                move_x = lx * cos(angle) + ly * sinf(angle);
                BtActStatus.move_x = move_x;
                move_z = ly * cos(angle) - lx * sinf(angle);
                BtActStatus.move_z = move_z;

                if (StatusErrCheck(AILMENT_GOO) != 0) {
                    move_x /= 2.0f;
                    move_z /= 2.0f;
                }

                BtActStatus.movement_locked = 0;

                if (StatusErrCheck(AILMENT_FREEZE) != 0) {
                    lx = 0.0f;
                    move_x = 0.0f;
                    ly = 0.0f;
                    move_z = 0.0f;
                    BtActStatus.movement_locked = 1;
                }

                velo__2[0] = move_x;
                velo__2[2] = move_z;
                velo2[0] = rx * cos(angle) + ry * sinf(angle);
                velo2[2] = ry * cos(angle) - rx * sinf(angle);
                stickVector = DistVector(velo__2);
                BtActStatus.stick_strength = stickVector;
                stickVector2 = DistVector(velo__2);
                BtActStatus.guard_mode = 0;

                if (lockOnTargetFlag != 0) {
                    velo__2[0] = move_x;
                    velo__2[2] = move_z;

                    if (BtActStatus.fast_camera_frames > 0) {
                        NowCamera__3->SetSpeed(32.0f);
                    } else {
                        NowCamera__3->SetSpeed(14.0f);
                    }
                } else {
                    NowCamera__3->SetSpeed(8.0f);
                    defCameraWait = 0;

                    if (BtBySpeedFlag != 0) {
                        run_speed = 3.0f;
                    } else {
                        run_speed = 1.45f;
                    }

                    velo__2[0] = move_x * run_speed;
                    velo__2[2] = move_z * run_speed;
                }

#ifdef PAL
                // In debug mode, Select outside an event opens the debug menu.
                if (DebugMode != 0 && GamePad.Down(PAD_R3) != 0 && BtEventMode == 0) {
                    DebugStatus[0] = 1;
                    gameTask = GAME_TASK_DEBUG_OPEN;
                    SndSePlay(MENU_SOUND_CONFIRM, -1, 0);
                    break;
                }

#endif
                if (stickVector <= 0.01f) {
                    CUserStatus *drain = UserStatus;

                    drain->water_drain_disable = 1;
                } else {
                    UserStatus->water_drain_disable = 0;
                }

                if (UserStatus->overflow_flag != 0) {
                    UserStatus->overflow_flag = false;
                    gameTask = GAME_TASK_MENU_OPEN;
                    EnemyLifeGage.draw = false;
                    driveStepHold = 1;
                    SetMIniMapStatus(0);
                    iventInfo = DNG_EVENT_NONE;
                    rogoSwitch2 = 0;
                    SndSePlay(MENU_SOUND_CONFIRM, -1, 0);
                } else {
                    bool should_walk = true;

                    if (GamePad.Down(PAD_TRIANGLE) != 0) {
                        int ok = 0;

                        if (move_x == 0.0f && move_z == 0.0f && BtActStatus.action_on == 0) {
                            ok = 1;
                        }

                        if (UserStatus->hp[UserStatus->cur_chara] <= 0) {
                            ok = 0;
                        }

                        if (BtActStatus.movement_locked != 0) {
                            ok = 1;
                        }

                        if (ok != 0) {
                            UserStatus->overflow_flag = false;
                            gameTask = GAME_TASK_MENU_OPEN;
                            EnemyLifeGage.draw = false;
                            driveStepHold = 1;
                            SetMIniMapStatus(0);
                            iventInfo = DNG_EVENT_NONE;
                            rogoSwitch2 = 0;
                            SndSePlay(MENU_SOUND_CONFIRM, -1, 0);
                            should_walk = false;
                        }
                    }

                    if (should_walk) {
                        if ((move_x != 0.0f || move_z != 0.0f) && (BtActStatus.action_on == 0 || UserStatus->cur_chara == CHARA_OSMOND)) {
                            CharaFrame->SetRotation(0.0f, unitRotation(CharaFrame, atan2f(move_x, move_z)), 0.0f);
                        }
                        CharaMain.motion_type.motion_info->speed = 0.05f;
                        BtActStatus.motion_no = keyCtrl(lx, ly, CharaMain.motion_type.motion_info);

                        if (UserStatus->hp[UserStatus->cur_chara] <= 0) {
                            BtActStatus.motion_no = 0x17;
                        }

                        if (BtActStatus.recoil_frames != 0) {
                            if (stickVector >= 0.8f && UserStatus->cur_chara == CHARA_TOAN) {
                                BtActStatus.motion_no = 0x1D;
                            }

                            BtActStatus.recoil_frames--;
                        }

                        if (BtActStatus.invincible_frames > 0) {
                            BtActStatus.invincible_frames--;
                        }

                        // SELECT opens the party, as retail does; with game.element_quick_select on,
                        // D-pad Up the same menu as the element picker (game/menu_dungeon.cpp), for a
                        // party of any size and a weapon with more than one element to pick between.
                        bool party = GamePad.Down(PAD_SELECT) != 0 && UserStatus->party_size >= 2;
                        bool elements = !party && ConfigGet().element_quick_select && GamePad.Down(PAD_UP) != 0 &&
                                        ElementPickerHasChoice();

                        if ((party || elements) && BtActStatus.action_on == 0) {
                            quickChangeLockOn = ConfigGet().swap_keeps_lock_on && lockOnTargetFlag != 0 ? lockOnTargetNo : -1;
                            quickChangeShiftNo = targetCursorShiftNo;
                            quickChangeShiftRot = targetCursorShiftRot;
                            QuickChangeOpenElements(elements);
                            DngMessMan.enabled = false;
                            DngMessMan.message = -1;
                            DngMessMan.timer = 0;
                            DngMessMan.steev_window = false;
                            DngMessMan.steev_index = 0;
                            DngMessMan.hide_timer = 0;
                            MonstorNameOff = 0;
                            EnemyLifeGage.draw = false;
                            BtMiniChrSelect_Init(0);
                            oldUnitNow = UserStatus->cur_chara;
                            gameTask = GAME_TASK_CHARA_SELECT_LOOP;
                            autoCamTrial();
                        } else {
                            static int    cnt = 0;
                            sceVu0FVECTOR mask_pos;

                            sceVu0CopyVector(mask_pos, CharaFrame->position);

                            if (cnt >= 5) {
                                NowDngMap->checkMask(mask_pos[0], mask_pos[2]);
                                cnt = 0;
                            }

                            cnt++;

                            if (BtActStatus.action_on == 0) {
                                if (GamePad.Down(PAD_LEFT) != 0) {
                                    SndSePlay(MENU_SOUND_CURSOR, -1, 0);

                                    if (itemNowSel < 2) {
                                        itemNowSel = 3;
                                    } else {
                                        itemNowSel--;
                                    }

                                    activeItem.now = itemNowSel;
                                }

                                if (GamePad.Down(PAD_RIGHT) != 0) {
                                    SndSePlay(MENU_SOUND_CURSOR, -1, 0);

                                    if (itemNowSel >= 3) {
                                        itemNowSel = 1;
                                    } else {
                                        itemNowSel++;
                                    }

                                    activeItem.now = itemNowSel;
                                }
                            }

                            if (GamePad.Down(PadInput_NO) != 0) {
                                if (lockOnTargetNo == -1) {
                                    sceVu0FMATRIX look;

                                    CharaFrame->GetLWMatrix(look);
                                    NowCamera__3->SetAngle(atan2f(look[2][0], look[2][2]) - PI_D);
                                } else if (lockOnTargetFlag == 0) {
                                    if (lockOnTargetNo != -1) {
                                        lockOnTargetFlag = 1;
                                        SndSePlay(SE_LOCK_ON, -1, 0);
                                    }
                                } else {
                                    lockOnTargetFlag = 0;
                                    SndSePlay(MENU_SOUND_REFUSE, -1, 0);
                                }
                            }

                            if (GamePad.Down(PAD_L1) != 0 && BtActStatus.movement_locked == 0 && lockOnTargetFlag != 0) {
                                if (targetCursorShiftNo >= targetCursorShiftRot - 1) {
                                    targetCursorShiftNo = 0;
                                    targetCursorShiftRot = SetNearLockOnTarget(0, 0);
                                    SndSePlay(SE_LOCK_ON, -1, 0);
                                } else {
                                    targetCursorShiftNo++;
                                    targetCursorShiftRot = SetNearLockOnTarget(targetCursorShiftNo, 0);
                                    SndSePlay(SE_LOCK_ON, -1, 0);
                                }
                            }

                            if (SetBattleStyle(selectMapNo, 0) <= 60.0f && move_x == 0.0f && move_z == 0.0f) {
                                BtActStatus.motion_no = 0x12;
                            }

                            if (lockOnTargetFlag != 0) {
                                CCharacter   *locked = &NowMonstorUnit->chara[lockOnTargetNo][0];
                                sceVu0FVECTOR target;

                                locked->GetPosition(target);
                                float face = atan2f(target[0] - pos[0], target[2] - pos[2]);

                                if (BtActStatus.stick_strength > 0.0f) {
                                    CharaFrame->SetRotation(0.0f, face, 0.0f);
                                } else {
                                    CharaFrame->SetRotation(0.0f, unitRotation(CharaFrame, face), 0.0f);
                                }

                                if (BtActStatus.action_on == 0) {
                                    BtActStatus.motion_no = 0x12;
                                    CharaMain.motion_type.motion_info[18].speed = 0.2f;
                                    float turn = atan2f(move_x, move_z);
                                    turn = face - turn;

                                    if (turn < -PI_SHORT) {
                                        turn += TWO_PI_SHORT;
                                    }

                                    if (turn > PI_SHORT) {
                                        turn -= TWO_PI_SHORT;
                                    }

                                    // The strafe motion comes from the angle between facing and stick
                                    // direction, in five bands.
                                    int motion = 0x12;

                                    if (turn > -1.2f && turn < 1.2f) {
                                        motion = 0x15;
                                    }

                                    if (turn < -2.6f || turn > 2.6f) {
                                        motion = 0x16;
                                    }

                                    if (turn > 1.2f && turn < 2.6f) {
                                        motion = 0x13;
                                    }

                                    if (turn < -1.2f && turn > -2.6f) {
                                        motion = 0x14;
                                    }

                                    if (BtActStatus.stick_strength > 0.0f) {
                                        BtActStatus.motion_no = motion;
                                        float rate = 0.2f + BtActStatus.stick_strength;

                                        if (rate >= 1.0f) {
                                            rate = 1.0f;
                                        }

                                        CharaMain.motion_type.motion_info[motion].speed = rate;
                                    }
                                }
                            }

                            if (UserStatus->cur_chara == CHARA_XIAO && BtActStatus.action_no == 0xC && (move_x != 0.0f || move_z != 0.0f)) {
                                CharaFrame->SetRotation(0.0f, unitRotation(CharaFrame, atan2f(move_x, move_z)), 0.0f);
                            }

                            if (GamePad.On(PAD_R1) != 0 && lockOnTargetFlag != 0 && BtActStatus.movement_locked == 0 && BtActStatus.in_water == 0 && BtActStatus.action_on == 0) {
                                BtActStatus.action_on = 6;
                                BtActStatus.action_no = 8;
                                driveNoInterpolate = 1;
                            }

                            if (BtActStatus.action_on == 6) {
                                float end = 0.0f;

                                if (lockOnTargetFlag == 0) {
                                    BtActStatus.action_on = 0;
                                    BtActStatus.action_no = 0x12;
                                }

                                if (BtActStatus.action_no == 8) {
                                    BtActStatus.motion_no = 8;
                                    end = CharaMain.motion_type.motion_info[8].end;

                                    if (CharaMain.motion_type.state.time >= end - 1.0f && CharaMain.motion_type.state.time < end) {
                                        BtActStatus.action_no = 9;
                                        BtActStatus.motion_no = 9;
                                        driveNoInterpolate = 1;
                                    }
                                }

                                if (BtActStatus.action_no == 9) {
                                    BtActStatus.motion_no = 9;
                                    BtActStatus.guard_mode = 5;

                                    if (GamePad.On(PAD_R1) == 0) {
                                        BtActStatus.action_no = 0xA;
                                        BtActStatus.motion_no = 0xA;
                                        driveNoInterpolate = 1;
                                    }
                                }

                                if (BtActStatus.action_no == 0xA) {
                                    BtActStatus.motion_no = 0xA;
                                    end = CharaMain.motion_type.motion_info[10].end;

                                    if (CharaMain.motion_type.state.time >= end - 1.5f && CharaMain.motion_type.state.time < end) {
                                        BtActStatus.action_on = 0;
                                    }
                                }
                            }

                            int script = 0;

                            if (NowMonstorUnit->requested_event != -1) {
                                BtEventInfo.script_no = NowMonstorUnit->requested_event;
                                BtEventInfo.ext_memory = true;
                                ResetStatusInfo();
                                driveStepHold = 1;
                                EdFadeInit();
                                EdFadeOut(0x78, 0.0f, 0.0f, 0.0f);
                                BtEventInfo.fade_on_start = false;
                                gameTask = GAME_TASK_FLOOR_FADE_RESET;
                            } else if (UserStatus->CheckLife() != 0 && BtActStatus.action_on == 0 && (script = NowMonstorUnit->CheckEventFlag2()) != -1) {
                                BtEventInfo.script_no = script;
                                BtEventInfo.ext_memory = false;
                                gameTask = GAME_TASK_SCRIPT_START;
                            } else if (UserStatus->CheckLife() != 0 && BtActStatus.action_on == 0 && NowMonstorUnit->GetMonstorNum() <= 0 && (script = BtEventInfo.clear_script_no) != -1) {
                                BtEventInfo.script_no = script;
                                BtEventInfo.ext_memory = BtEventInfo.clear_script_ext_memory;
                                BtEventInfo.clear_script_no = -1;
                                BtEventInfo.clear_script_ext_memory = 0;
                                printf("dead script !!\n");
                                gameTask = GAME_TASK_SCRIPT_START;
                            } else {
                                MAP_TRAP_CIRCLE *trap = NowDngMap->DistTrapCircle();
                                int              event = 0;

                                if (trap != nullptr) {
                                    SetSystemMes(Run_TrapCircle(trap) + 0x12C, 0x96, MES_POS_BOTTOM, 0, nullptr, nullptr);
                                    DngMessMan.hide_timer = 0x96;
                                }

                                bool run_steal = false;
                                if (UserStatus->CheckLife() != 0) {
                                    event = RandomItem->checkErr();

                                    if (event != 0) {
                                        ClearSystemMes();

                                        if (event == 1) {
                                            SetSystemMes(0x48, 0x78, MES_POS_BOTTOM, 0, nullptr, nullptr);
                                        }

                                        if (event == 2) {
                                            SetSystemMes(0x51, 0x78, MES_POS_BOTTOM, 0, nullptr, nullptr);
                                        }

                                        DngMessMan.hide_timer = 0x78;
                                    } else {
                                        event = RandomItem->checkEvent();

                                        if (event != -1) {
                                            switch (event) {
                                                case ITEM_DRAN_S_CREST:
                                                case ITEM_SHINY_STONE:
                                                case ITEM_RED_BERRY:
                                                case ITEM_HOOK:
                                                case ITEM_KING_S_SLATE:
                                                case ITEM_GUN_POWDER:
                                                case ITEM_CLOCK_HANDS:
                                                case ITEM_POINTY_CHESTNUT:
                                                case ITEM_BLACK_KNIGHT_CREST:
                                                    static_cast<CDngStatusData *>(UserStatus)->GetItem(event, 0);
                                                    BtGetGateKey_Init(event);
                                                    gameTask = GAME_TASK_GATE_KEY_LOOP;
                                                    break;
                                                default:
                                                    BtGetAttach_Init(selectMapNo, event);
                                                    DngMessMan.hide_timer = 0x78;
                                                    SndSePlay(SE_ITEM_GET, -1, 0);
                                                    autoCamTrial();
                                                    break;
                                            }
                                        }

                                        run_steal = true;
                                    }
                                } else {
                                    run_steal = true;
                                }

                                if (run_steal) {
                                    if (UserStatus->CheckLife() != 0) {
                                        int stolen = StealItem.checkEvent();

                                        if (stolen != -1) {
                                            BtGetAttach_Init(selectMapNo, stolen);
                                            DngMessMan.hide_timer = 0x78;
                                            SndSePlay(SE_ITEM_GET, -1, 0);
                                            autoCamTrial();
                                        }
                                    }

                                    bool run_action = true;
                                    if (BtActStatus.can_act != 0 && BtActStatus.movement_locked == 0) {
                                        CDungeonEventData *state = NowEventMan->SearchDataSlotPos2(pos);
                                        sceVu0FVECTOR      slot_pos;
                                        sceVu0FVECTOR      slot_dir;

                                        if (state != nullptr && state->event->chara_no != -1) {
                                            s8  chara = UserStatus->cur_chara;
                                            int done = state->chara_done;

                                            if (done == chara) {
                                                state = nullptr;
                                            }
                                        }

                                        BtEventInfo.script_no = -1;

                                        if (state != nullptr && state->event->script_no != -1) {
                                            BtEventInfo.script_no = state->event->script_no;
                                            BtEventInfo.ext_memory = state->event->ext_mem;
                                            BtEventInfo.event_marker = true;
                                            BtEventInfo.action_mode = 0;
                                            sceVu0CopyVector(slot_pos, state->pos);
                                            sceVu0CopyVector(slot_dir, state->dir);
                                            sceVu0CopyVector(BtEventInfo.position, slot_pos);
                                            sceVu0CopyVector(BtEventInfo.direction, slot_dir);
                                        }

                                        if (BtEventInfo.script_no != -1) {
                                            if (state->event->chara_no != -1 && GamePad.Down(PadInput_OK) != 0) {
                                                if (UserStatus->cur_chara == state->event->chara_no) {
                                                    state->chara_done = UserStatus->cur_chara;
                                                }

                                                gameTask = GAME_TASK_SCRIPT_START;
                                                BtEventInfo.action_mode = 0;
                                                BtEventInfo.chara_help = true;
                                            }

                                            if (state->event->fade != 0) {
                                                EdFadeInit();
                                                EdFadeOut(0x78, 0.0f, 0.0f, 0.0f);
                                                BtEventInfo.event_marker = false;
                                                gameTask = GAME_TASK_SCRIPT_RESTART;
                                                run_action = false;
                                            } else if (state->hold != 0 && GamePad.Down(PAD_SQUARE) != 0) {
                                                gameTask = GAME_TASK_SCRIPT_START;
                                                BtEventInfo.action_mode = 2;
                                                run_action = false;
                                            } else if (GamePad.Down(PadInput_OK) != 0) {
                                                gameTask = GAME_TASK_SCRIPT_START;
                                                BtEventInfo.action_mode = 1;
                                                run_action = false;
                                            }
                                        }
                                    }

                                    if (run_action) {
                                        if (BtActStatus.in_water != 0) {
                                            BtActStatus.action_on = 0;
                                            BtActStatus.unk_028 = 0;
                                            BtActStatus.jumping = 0;
                                            BtActStatus.action_busy = 0;
                                            BtActStatus.can_act = 1;
                                            BtActStatus.action_no = 0;
                                            BtActStatus.action_gauge = 100.0f;
                                            BtActStatus.gauge_exhausted = 0;
                                            ResetMovePower();
                                        }

                                        if (GamePad.Down(PadInput_OK | 0x80) != 0 && BtActStatus.movement_locked == 0 && BtActStatus.in_water != 0) {
                                            SetSystemMes(0x47, 0x5A, MES_POS_BOTTOM, 0, nullptr, nullptr);
                                            DngMessMan.hide_timer = 0x5A;
                                            autoCamTrial();
                                            BtActStatus.action_on = 0;
                                            BtActStatus.unk_028 = 0;
                                            BtActStatus.jumping = 0;
                                            BtActStatus.action_busy = 0;
                                            BtActStatus.can_act = 1;
                                            BtActStatus.action_gauge = 100.0f;
                                            BtActStatus.gauge_exhausted = 0;
                                            ResetMovePower();
                                        } else if (GamePad.Down(PadInput_OK) != 0 && BtActStatus.ground_kind == 0xA && BtActStatus.movement_locked == 0 && UserStatus->cur_chara == CHARA_OSMOND) {
                                            SetSystemMes(0x50, 0x5A, MES_POS_BOTTOM, 0, nullptr, nullptr);
                                            DngMessMan.hide_timer = 0x5A;
                                            autoCamTrial();
                                        } else {
                                            BtEventItemNo = -1;

                                            iventActive = NowDngMap->GetActiveIvent(CharaFrame);

                                            if (iventActive != -1 && BtActStatus.can_act != 0 && BtActStatus.movement_locked == 0) {
                                                iventInfo = NowDngMap->events[iventActive].kind;
                                                BtEventInfo.event_marker = true;
                                                iventMarker = 1;
                                            } else {
                                                iventActive = -1;
                                                iventInfo = DNG_EVENT_NONE;
                                                iventMarker = 0;
                                            }

                                            bool run_play = true;
                                            if (GamePad.Down(PadInput_OK) != 0 && BtActStatus.movement_locked == 0 && BtActStatus.event_block_frames == 0) {
                                                run_play = false;
                                                if (iventActive != -1 && BtActStatus.recoil_frames == 0) {
                                                    int done = 0;

                                                    switch (NowDngMap->events[iventActive].kind) {
                                                        case DNG_EVENT_TREASURE_BOX:
                                                            BtActStatus.motion_no = 0;
                                                            entry_no = NowDngMap->events[iventActive].index;

                                                            if (NowDngMap->boxes[entry_no].kind == TREASURE_BOX_LARGE) {
                                                                if (NowDngMap->boxes[entry_no].trap_no == 0) {
                                                                    gameTask = GAME_TASK_BOX_BIG_INIT;
                                                                    done = 1;
                                                                    SndSePlay(MENU_SOUND_CONFIRM, -1, 0);
                                                                }

                                                                else if (NowDngMap->boxes[entry_no].trap_no == 5) {
                                                                    sceVu0FVECTOR box_pos;
                                                                    sceVu0FVECTOR box_dir = {0.0f, 0.0f,
                                                                                             0.0f, 1.0f};

                                                                    sceVu0CopyVector(box_pos, NowDngMap->events[iventActive].pos);
                                                                    sceVu0CopyVector(BtEventInfo.position, box_pos);
                                                                    sceVu0CopyVector(BtEventInfo.direction, box_dir);
                                                                    NowDngMap->events[iventActive].kind = DNG_EVENT_NONE;
                                                                    BtEventInfo.script_no = 0x10;
                                                                    BtEventInfo.ext_memory = false;
                                                                    BtEventInfo.treasure_box = entry_no;
                                                                    BtEventInfo.action_mode = 1;
                                                                    gameTask = GAME_TASK_SCRIPT_START;
                                                                    ResetMovePower();
                                                                    done = 1;
                                                                } else {
                                                                    BtEventInfo.script_no = 0xF;
                                                                    BtEventInfo.ext_memory = false;
                                                                    BtEventInfo.treasure_box = entry_no;
                                                                    BtEventInfo.action_mode = 1;
                                                                    gameTask = GAME_TASK_SCRIPT_START;
                                                                    ResetMovePower();
                                                                    done = 1;
                                                                }
                                                            } else {
                                                                done = 1;
                                                                gameTask = GAME_TASK_BOX_SMALL_INIT;
                                                                SndSePlay(MENU_SOUND_CONFIRM, -1, 0);
                                                            }

                                                            break;
                                                        case DNG_EVENT_ATRA:
                                                            BtActStatus.motion_no = 0;
                                                            entry_no = NowDngMap->events[iventActive].index;

                                                            if (NowDngMap->atra[entry_no].used != 0) {
                                                                s8 chara = UserStatus->cur_chara;

                                                                if (UserStatus->cur_chara == CHARA_TOAN) {
                                                                    gameTask = GAME_TASK_ATRA_GET_INIT;
                                                                    done = 1;
                                                                    SndSePlay(MENU_SOUND_CONFIRM, -1, 0);
                                                                } else {
                                                                    NotGetAtraMes(chara, 0x5A);
                                                                    DngMessMan.hide_timer = 0x5A;
                                                                }
                                                            }

                                                            break;
                                                        case DNG_EVENT_MIMIC:
                                                            entry_no = NowDngMap->events[iventActive].index;

                                                            int           item_no = NowDngMap->boxes[entry_no].item_no;
                                                            CMonstorUnit *unit = NowMonstorUnit;

                                                            if (item_no >= 0 && item_no < 0x11) {
                                                                unit->monster[item_no].revealed = 1;
                                                            }

                                                            NowDngMap->boxes[entry_no].used = 0;
                                                            NowDngMap->events[iventActive].kind = DNG_EVENT_NONE;
                                                            done = 1;
                                                            break;
                                                    }

                                                    if (done != 0) {
                                                        break;
                                                    }

                                                    run_play = true;
                                                } else {
                                                    switch (UserStatus->cur_chara) {
                                                        case CHARA_TOAN:
                                                            ToanKey_On();
                                                            break;
                                                        case CHARA_XIAO:
                                                            BattleActionOn_Jinn();
                                                            break;
                                                        case CHARA_GORO:
                                                            GoroKey_On();
                                                            break;
                                                        case CHARA_RUBY:
                                                            BattleActionOn_Ruby();
                                                            break;
                                                        case CHARA_UNGAGA:
                                                            UngagaKey_On();
                                                            break;
                                                        case CHARA_OSMOND:
                                                            if (BtActStatus.gun_type == 0) {
                                                                BattleActionOn_Ozumond();
                                                            }

                                                            if (BtActStatus.gun_type == 1) {
                                                                BattleActionOn_Ozumond_H();
                                                            }

                                                            if (BtActStatus.gun_type == 2) {
                                                                BattleActionOn_Ozumond_F();
                                                            }

                                                            break;
                                                        default:
                                                            break;
                                                    }

                                                    run_play = true;
                                                }
                                            }

                                            if (run_play) {
                                                if (BtActStatus.action_on == 1) {
                                                    switch (UserStatus->cur_chara) {
                                                        case CHARA_TOAN:
                                                            ToanKey_Play();
                                                            break;
                                                        case CHARA_XIAO:
                                                            BattleActionPlay_Jinn(&CharaMain, 0);
                                                            break;
                                                        case CHARA_GORO:
                                                            GoroKey_Play();
                                                            break;
                                                        case CHARA_RUBY:
                                                            BattleActionPlay_Ruby(&CharaMain, 0);
                                                            break;
                                                        case CHARA_UNGAGA:
                                                            UngagaKey_Play();
                                                            break;
                                                        case CHARA_OSMOND:
                                                            if (BtActStatus.gun_type == 0) {
                                                                BattleActionPlay_Ozumond(0);
                                                            }

                                                            if (BtActStatus.gun_type == 1) {
                                                                BattleActionPlay_Ozumond_H(0);
                                                            }

                                                            if (BtActStatus.gun_type == 2) {
                                                                BattleActionPlay_Ozumond_F(0);
                                                            }

                                                            break;
                                                        default:
                                                            break;
                                                    }
                                                }
                                                BtBySpeedFlag = 0;

                                                if (GamePad.On(PAD_SQUARE) != 0 && BtActStatus.movement_locked == 0 && lockOnTargetFlag == 0) {
                                                    if (DebugStatus[5] == 0) {
                                                        BtBySpeedFlag = 1;
                                                    }

                                                    if (BtActStatus.action_on == 0 && gameTask != GAME_TASK_UNK_F0 && BtActStatus.can_act != 0) {
                                                        s16 *slots = UserStatus->active_item;

                                                        if (activeItem.CheckStatusType() == ACTIVE_ITEM_FEATHER && slots[itemNowSel + 3] > 0) {
                                                            // A boosting running item is spent each frame the button is held.
                                                            BtBySpeedFlag = 1;
                                                            int slot = itemNowSel - 1;
                                                            int left = UserStatus->active_item_vol[slot] - 1;

                                                            UserStatus->active_item_vol[slot] = left;

                                                            if (left <= 0) {
                                                                DelActiveItem(itemNowSel);
                                                                DngMessMan.message = 0xB6;
                                                                DngMessMan.insert_mes_1 = GetCommonItemDataSystemMsg(-1);
                                                                DngMessMan.timer = 0xB4;
                                                                DngMessMan.steev_window = false;
                                                            }
                                                        }
                                                    }
                                                }

                                                if (GamePad.Down(PAD_SQUARE) != 0 && BtActStatus.movement_locked == 0 && BtActStatus.action_on == 0 && gameTask != GAME_TASK_UNK_F0 && BtActStatus.can_act != 0) {
                                                    s16 *slots = UserStatus->active_item;
                                                    int  used = checkItemUsed(itemNowSel - 1);

                                                    if (activeItem.CheckStatusType() == ACTIVE_ITEM_DRINK && used != 0 && slots[itemNowSel + 3] > 0) {
                                                        CMonUnitHold = 1;
                                                        CEffectHold = 1;
                                                        BtActStatus.action_on = 3;
                                                        BtActStatus.can_act = 0;
                                                        BtActStatus.action_busy = 1;

                                                        if (activeItem.model[8] == -1) {
                                                            activeItem.model[8] = activeItem.models->SetHandModel(activeItem.model[activeItem.now]);
                                                        }
                                                    }

                                                    if (activeItem.CheckStatusType() == ACTIVE_ITEM_EAT && used != 0 && slots[itemNowSel + 3] > 0) {
                                                        setUnitAmbientAnime(64.0f, 1.0f, 0.0f, 122.0f, 208.0f);
                                                        SndSePlay(SE_EFFECT_CHIME, -1, 0);

                                                        s32 taken = activeItem.item[activeItem.now];

                                                        usedActiveItem(UserStatus, taken);

                                                        s16 *inner = UserStatus->active_item;

                                                        if (inner[itemNowSel + 3] == 1) {
                                                            inner[itemNowSel] = -1;
                                                            inner[itemNowSel + 3] = 0;
                                                            DeleteItemModel(itemNowSel);
                                                        } else {
                                                            inner[itemNowSel + 3]--;
                                                        }

                                                        SndSePlay(SE_CHARA_RECOVER, -1, 0);
                                                    }

                                                    if (activeItem.CheckStatusType() == ACTIVE_ITEM_ACTION && BtActStatus.can_act != 0 && used != 0 && BtActStatus.action_on != 2 && slots[itemNowSel + 3] > 0) {
                                                        printf("throw !!\n");

                                                        if (lockOnTargetFlag == 0) {
                                                            sceVu0FVECTOR hand;

                                                            BtActStatus.action_on = 2;
                                                            BombInfo.aiming = true;
                                                            BombInfo.throw_step = -1;
                                                            sceVu0CopyVector(hand, CharaMain.pos);
                                                            getCharacterVector(BombInfo.pos, 0.0f);
                                                            BombInfo.pos[0] += hand[0] + 30.0f * BombInfo.pos[0];
                                                            BombInfo.pos[1] = hand[1];
                                                            BombInfo.pos[2] += hand[2] + 30.0f * BombInfo.pos[2];
                                                            BtActStatus.motion_no = 0x1A;
                                                            BtActStatus.can_act = 0;
                                                            BtActStatus.action_busy = 1;

                                                            if (activeItem.model[8] == -1) {
                                                                activeItem.model[8] = activeItem.models->SetHandModel(activeItem.model[activeItem.now]);
                                                            }
                                                        } else {
                                                            BombInfo.throw_step = 1;
                                                            BtActStatus.action_on = 2;
                                                            BtActStatus.motion_no = 0x1B;
                                                            BtActStatus.can_act = 0;
                                                            BtActStatus.action_busy = 1;

                                                            if (activeItem.model[8] == -1) {
                                                                activeItem.model[8] = activeItem.models->SetHandModel(activeItem.model[activeItem.now]);
                                                            }
                                                        }
                                                    }
                                                }

                                                {
                                                    CDungeonEventData *state = NowEventMan->SearchDataSlotPos(pos);
                                                    sceVu0FVECTOR      slot_pos;
                                                    sceVu0FVECTOR      slot_dir;
                                                    bool               run_step = true;

                                                    if (state != nullptr && BtActStatus.can_act != 0 && state->event->chara_no != -1) {
                                                        BtEventInfo.script_no = -1;

                                                        if (state->event->script_no != -1) {
                                                            BtEventInfo.script_no = state->event->script_no;
                                                            BtEventInfo.ext_memory = state->event->ext_mem;
                                                            BtEventInfo.action_mode = 0;
                                                            sceVu0CopyVector(slot_pos, state->pos);
                                                            sceVu0CopyVector(slot_dir, state->dir);
                                                            sceVu0CopyVector(BtEventInfo.position, slot_pos);
                                                            sceVu0CopyVector(BtEventInfo.direction, slot_dir);
                                                        }

                                                        if (BtEventInfo.script_no != -1) {
                                                            gameTask = GAME_TASK_SCRIPT_START;
                                                            BtEventInfo.action_mode = 1;
                                                            BtEventInfo.chara_help = false;
                                                            run_step = false;
                                                        }
                                                    }

                                                    if (run_step) {
                                                        BattleActionThlow();
                                                        BattleActionDrink();

                                                        int ok = 1;

                                                        if (BtActStatus.action_on != 0) {
                                                            ok = 0;
                                                        }

                                                        BtActStatus.slow_walking = 0;

                                                        if (BtActStatus.action_on == 1) {
                                                            int slow = 0;

                                                            if (BtActStatus.action_no == 0xE && UserStatus->cur_chara == CHARA_TOAN) {
                                                                slow = 1;
                                                            }

                                                            if (BtActStatus.action_no == 0xD && UserStatus->cur_chara == CHARA_GORO) {
                                                                slow = 1;
                                                            }

                                                            if (slow != 0) {
                                                                if (stickVector >= 0.1f) {
                                                                    velo__2[0] *= 0.4f;
                                                                    velo__2[2] *= 0.4f;
                                                                    BtActStatus.motion_no = 0x1E;
                                                                    CharaMain.motion_type.motion_info[30]
                                                                        .speed = 0.2f + stickVector / 2.0f;
                                                                }

                                                                ok = 1;
                                                                BtActStatus.slow_walking = 1;
                                                            }
                                                        }

                                                        if (BtActStatus.guard_mode == 5) {
                                                            if (stickVector >= 0.1f) {
                                                                velo__2[0] *= 0.4f;
                                                                velo__2[2] *= 0.4f;
                                                                BtActStatus.motion_no = 0x21;
                                                                CharaMain.motion_type.motion_info[33]
                                                                    .speed = 0.2f + stickVector / 2.0f;
                                                            }

                                                            ok = 1;
                                                            BtActStatus.slow_walking = 1;
                                                        }

                                                        if (UserStatus->cur_chara == CHARA_OSMOND) {
                                                            ok = 1;
                                                        }

                                                        if (ok == 0 || BtActStatus.can_act == 0) {
                                                            velo__2[2] = 0.0f;
                                                            velo__2[1] = 0.0f;
                                                            velo__2[0] = 0.0f;
                                                        }

                                                        BtCheckDamageProc();

                                                        if (BtActStatus.action_on == 4) {
                                                            float end = CharaMain.motion_type.motion_info[4].end;

                                                            if (CharaMain.motion_type.state.time >= end - 2.0f && CharaMain.motion_type.state.time <= end) {
                                                                BtActStatus.action_on = 0;
                                                            } else {
                                                                BtActStatus.motion_no = 4;
                                                            }
                                                        }

                                                        if (BtActStatus.action_on == 5) {
                                                            float end = CharaMain.motion_type.motion_info[6].end;

                                                            if (CharaMain.motion_type.state.time >= end - 2.0f && CharaMain.motion_type.state.time <= end) {
                                                                BtActStatus.action_on = 0;
                                                            } else {
                                                                BtActStatus.motion_no = 6;
                                                            }
                                                        }

                                                        if (UserStatus->CheckLife() <= 0 && BtActStatus.invincible_frames == 0 && BtActStatus.action_on != 5) {
                                                            UserStatus->hp[UserStatus->cur_chara] = 0;
                                                            DeadKeyWait = 0x168;
                                                            DeadKeyStartWait = 0x3C;
                                                            BtActStatus.movement_locked = 0;
                                                            UserStatus->ailments[UserStatus->cur_chara] = 0;
                                                            UserStatus->ailment_frames[UserStatus->cur_chara] = 0;
                                                            LockOffTargte();
                                                            gameTask = GAME_TASK_DEAD;
                                                        } else {
                                                            if (BtActStatus.action_on == 5) {
                                                                velo__2[0] = blowVelo[0];
                                                                velo__2[2] = blowVelo[2];

                                                                for (int i = 0; i < 3; i++) {
                                                                    float speed = blowVelo[i];

                                                                    float step = 0.018f;

                                                                    if (speed < 0.0f) {
                                                                        blowVelo[i] = speed + step;

                                                                        if (blowVelo[i] >= -0.01f) {
                                                                            blowVelo[i] = 0.0f;
                                                                        }
                                                                    } else {
                                                                        blowVelo[i] = speed - 0.018f;

                                                                        if (blowVelo[i] <= 0.01f) {
                                                                            blowVelo[i] = 0.0f;
                                                                        }
                                                                    }
                                                                }
                                                            }

                                                            if (BtActStatus.jumping == 1) {
                                                                velo__2[0] += BtActStatus.jump_velocity_x;
                                                                velo__2[1] += BtActStatus.jump_velocity_y;
                                                                velo__2[2] += BtActStatus.jump_velocity_z;
                                                                BtActStatus.jump_velocity_y -= 0.1f;
                                                            }

                                                            if (BtActStatus.move_power > 0.0f) {
                                                                sceVu0FVECTOR push;

                                                                sceVu0ScaleVectorXYZ(push, BtActStatus.move_vector, BtActStatus.move_power);
                                                                velo__2[0] += push[0];
                                                                velo__2[2] += push[2];
                                                                BtActStatus.move_power -= BtActStatus.move_power_decay;

                                                                if (BtActStatus.move_power <= 0.0f) {
                                                                    BtActStatus.move_power = 0.0f;
                                                                }
                                                            }

                                                            BtActStatus.ground_kind = 0;

                                                            if (DebugStatus[5] != 0) {
                                                                if (NowDngMap->map_type != 1) {
                                                                    sceVu0FVECTOR moved;
                                                                    MoveCheckInfo info;
                                                                    sceVu0FVECTOR parts_pos;
                                                                    CBoxVu0       bound;
                                                                    CCPoly        foot;

                                                                    WorkBuffer->used = 0;
                                                                    auto *polys = reinterpret_cast<CCPoly *>(WorkBuffer->Alloc(0x7D0));
                                                                    bound.max[0] = 20.0f + pos[0];
                                                                    bound.max[1] = 20.0f + pos[1];
                                                                    bound.max[2] = 20.0f + pos[2];
                                                                    bound.min[0] = pos[0] - 20.0f;
                                                                    bound.min[1] = pos[1] - 40.0f;
                                                                    bound.min[2] = pos[2] - 20.0f;
                                                                    // A random floor is built from map parts, so the polygons
                                                                    // come off each part's own collision model.
                                                                    int i = 0;

                                                                    colPolyNum = 0;

                                                                    for (; NowDngMap->parts[i].frame[0] != nullptr; i++) {
                                                                        collision = NowDngMap->parts[i].collision;

                                                                        if (collision != nullptr) {
                                                                            CDungeonParts *part = &NowDngMap->parts[i];

                                                                            sceVu0CopyVector(parts_pos, part->frame_offset[0]);

                                                                            CDungeonMap   *map = NowDngMap;
                                                                            CDungeonParts *turn_part = &map->parts[i];

                                                                            int turn = static_cast<int>(turn_part->frame_turn[0]);
                                                                            turn += turn_part->collision_turn;

                                                                            if (turn > 3) {
                                                                                turn -= 3;
                                                                            }

                                                                            if (turn == 3) {
                                                                                turn = -1;
                                                                            }

                                                                            float rot = PI * (-90.0f * turn) / 180.0f;

                                                                            collision->SetRotation(0.0f, rot, 0.0f);
                                                                            collision->SetPosition(parts_pos);
                                                                            colPolyNum += collision->PickUpNearPoly(&polys[colPolyNum], bound);
                                                                        }
                                                                    }

                                                                    for (entry_no = 0; entry_no < 24; entry_no++) {
                                                                        if (NowDngMap->boxes[entry_no].used != 0) {
                                                                            collision = NowDngMap->box_collision_model;

                                                                            collision->SetPosition(NowDngMap->boxes[entry_no].pos);
                                                                            colPolyNum += collision->PickUpNearPoly(&polys[colPolyNum], bound);
                                                                        }
                                                                    }

                                                                    colPolyNum = NowDngMap->CreateCollision(polys, bound, colPolyNum);
                                                                    colPolyNum = NowDranMapField->AddCollision(polys, colPolyNum, bound);
                                                                    NowMonstorUnit->MoveCheck(pos, velo__2, lockOnTargetFlag);
                                                                    int mode = 1;

                                                                    if (UserStatus->cur_chara == CHARA_OSMOND) {
                                                                        mode = 8;
                                                                    }

                                                                    MoveCheck(pos, velo__2, moved, &info, polys, colPolyNum, mode);

                                                                    if (colPolyNum >= 0x190) {
                                                                        printf("er -> %d\n", colPolyNum);
                                                                    }

                                                                    sceVu0SubVector(ref_off, moved, pos);
                                                                    veloOld[1] = velo__2[1];
                                                                    velo__2[1] -= 0.1f;

                                                                    if (velo__2[1] < -10.0f) {
                                                                        velo__2[1] = -10.0f;
                                                                    }

                                                                    sceVu0CopyVector(pos, moved);

                                                                    if (info.landed != 0) {
                                                                        velo__2[1] = 0.0f;
                                                                        foot = info.poly;
                                                                        BtActStatus.foot_sound = foot.attr.foot_sound;
                                                                        BtActStatus.ground_kind = foot.attr.ground_kind;
                                                                    }

                                                                    if (info.ground_found != 0) {
                                                                        BtActStatus.ground_height = pos[1] - info.ground_point[1];
                                                                    }
                                                                } else {
                                                                    sceVu0FVECTOR moved;
                                                                    MoveCheckInfo info;
                                                                    CBoxVu0       bound;
                                                                    CCPoly        foot;
                                                                    CFrame       *collision = nullptr;

                                                                    WorkBuffer->used = 0;
                                                                    auto *polys = reinterpret_cast<CCPoly *>(WorkBuffer->Alloc(0x7D0));
                                                                    bound.max[0] = 20.0f + pos[0];
                                                                    bound.max[1] = 20.0f + pos[1];
                                                                    bound.max[2] = 20.0f + pos[2];
                                                                    bound.min[0] = pos[0] - 20.0f;
                                                                    bound.min[1] = pos[1] - 40.0f;
                                                                    bound.min[2] = pos[2] - 20.0f;
                                                                    colPolyNum = 0;

                                                                    for (int z = 0; z < 20; z++) {
                                                                        for (int x = 0; x < 20; x++) {
                                                                            CDungeonMap *cmap = NowDngMap;
                                                                            int          parts_no = cmap->cells[x + z * 20]
                                                                                                        .parts_no;

                                                                            collision = parts_no == -1 ? nullptr : NowDngMap->parts[parts_no].collision;
                                                                            MAP_CELL *cell = &NowDngMap->cells[x + z * 20];

                                                                            if (selectMapNo == DUNGEON_GALLERY_OF_TIME && parts_no >= 0x28 && parts_no < 0x2C) {
                                                                                cell->camera_dist -= 160.0f;
                                                                            }

                                                                            if (collision != nullptr && cell->camera_dist <= 240.0f) {
                                                                                int dir = NowDngMap->cells[x + z * 20].direction;
                                                                                int base = parts_no == -1 ? 0 : NowDngMap->parts[parts_no].collision_turn;

                                                                                int turn = dir;
                                                                                turn += base;

                                                                                if (turn > 3) {
                                                                                    turn -= 3;
                                                                                }

                                                                                if (turn == 3) {
                                                                                    turn = -1;
                                                                                }

                                                                                float rot = PI * (-90.0f * turn) / 180.0f;

                                                                                collision->SetRotation(0.0f, rot, 0.0f);
                                                                                collision->SetPosition(160.0f * x, 0.0f, 160.0f * z);
                                                                                colPolyNum += collision
                                                                                                  ->PickUpNearPoly(&polys[colPolyNum], bound);
                                                                            }
                                                                        }
                                                                    }

                                                                    for (TREASURE_BOX &box : NowDngMap->boxes) {
                                                                        if (box.used != 0) {
                                                                            collision = NowDngMap->box_collision_model;

                                                                            collision->SetPosition(box.pos);
                                                                            colPolyNum += collision->PickUpNearPoly(&polys[colPolyNum], bound);
                                                                        }
                                                                    }

                                                                    colPolyNum = NowDngMap->CreateCollision(polys, bound, colPolyNum);
                                                                    NowMonstorUnit->MoveCheck(pos, velo__2, lockOnTargetFlag);
                                                                    int mode = 1;

                                                                    if (UserStatus->cur_chara == CHARA_OSMOND) {
                                                                        mode = 8;
                                                                    }

                                                                    MoveCheck(pos, velo__2, moved, &info, polys, colPolyNum, mode);

                                                                    if (colPolyNum >= 0x190) {
                                                                        printf("er -> %d\n", colPolyNum);
                                                                    }

                                                                    sceVu0SubVector(ref_off, moved, pos);
                                                                    veloOld[1] = velo__2[1];
                                                                    velo__2[1] -= 0.1f;

                                                                    if (velo__2[1] < -10.0f) {
                                                                        velo__2[1] = -10.0f;
                                                                    }

                                                                    sceVu0CopyVector(pos, moved);

                                                                    if (info.landed != 0) {
                                                                        velo__2[1] = 0.0f;
                                                                        foot = info.poly;
                                                                        BtActStatus.foot_sound = foot.attr.foot_sound;
                                                                        BtActStatus.ground_kind = foot.attr.ground_kind;
                                                                    }

                                                                    if (info.ground_found != 0) {
                                                                        BtActStatus.ground_height = pos[1] - info.ground_point[1];
                                                                    }
                                                                }
                                                            } else {
                                                                pos[0] += velo__2[0];
                                                                pos[1] += velo__2[1];
                                                                pos[2] += velo__2[2];
                                                            }

                                                            if (UserStatus->cur_chara != CHARA_OSMOND) {
                                                                CharaMain.FootSoundEnable(1);
                                                                CharaMain.EventEnable(1);
                                                                CharaMain.SetFootSoundID(BtActStatus.foot_sound);
                                                            } else {
                                                                static int snd_cnt = 0;
                                                                static int id_cnt = 0;

                                                                snd_cnt++;

                                                                if (snd_cnt >= 5) {
                                                                    SndSeSeqPlayStop(0x1CC, 5, id_cnt);
                                                                    snd_cnt = 0;
                                                                    id_cnt++;

                                                                    if (id_cnt >= 0xA) {
                                                                        id_cnt = 0;
                                                                    }
                                                                }
                                                            }

                                                            if (pos[1] <= -30.0f && selectMapNo == DUNGEON_DIVINE_BEAST_CAVE) {
                                                                UserStatus->AddNowLife(UserStatus->cur_chara, -0x28, 10.0f);
                                                                setUnitAmbientAnime(120.0f, 1.0f, 255.0f, 0.0f, 0.0f);
                                                                ResetStatusInfo();
                                                                BtEventInfo.script_no = 5;
                                                                BtEventInfo.ext_memory = false;
                                                                EdFadeOut(0x78, 0.0f, 0.0f, 0.0f);
                                                                gameTask = GAME_TASK_SCRIPT_RESTART;
                                                            } else {
                                                                if (CharaMain.motion_type.state.time >= static_cast<float>(0x12F) && CharaMain.motion_type.state.time <= 318.0f) {
                                                                    sceVu0FVECTOR run_pos;

                                                                    sceVu0CopyVector(run_pos, CharaFrame->position);
                                                                    CRunFx__2.Set(run_pos);
                                                                }

                                                                if (BtActStatus.movement_locked == 0) {
                                                                    int dust = 0;

                                                                    float time = CharaMain.motion_type.state.time;

                                                                    if (time >= 75.0f && time <= 76.0f) {
                                                                        dust = 1;
                                                                    }

                                                                    if (time >= 77.0f && time <= 78.0f) {
                                                                        dust = 1;
                                                                    }

                                                                    if (time >= 85.0f && time <= 86.0f) {
                                                                        dust = 1;
                                                                    }

                                                                    if (time >= 87.0f && time <= 88.0f) {
                                                                        dust = 1;
                                                                    }

                                                                    if (CharaMain.motion_no == 2) {
                                                                        if (DistVector(velo__2) >= 0.4f) {
                                                                            if (time >= 38.0f && time <= 39.0f) {
                                                                                dust = 1;
                                                                            }

                                                                            if (time >= 45.0f && time <= 46.0f) {
                                                                                dust = 1;
                                                                            }
                                                                        } else {
                                                                            dust = 0;
                                                                        }
                                                                    }

                                                                    if (dust != 0) {
                                                                        sceVu0FVECTOR step_pos;

                                                                        sceVu0CopyVector(step_pos, CharaFrame->position);

                                                                        if (UserStatus->cur_chara != CHARA_OSMOND) {
                                                                            CRunFx__2.Set(step_pos);
                                                                        }
                                                                    }
                                                                }

                                                                HealingWater();
                                                                float turn = MouseLookTurn(NowCamera__3, 0.04f, GamePad.GetRXf());

                                                                NowCamera__3->AddHeight(-MouseLookRise(NowCamera__3, GamePad.GetRYf(), 30.0f));

                                                                if (NowCamera__3->GetHeight() >= 30.0f) {
                                                                    NowCamera__3->SetHeight(30.0f);
                                                                }

                                                                NowCamera__3->AddAngle(0.04f * -turn);

                                                                if (lockOnTargetFlag == 0) {
                                                                    if (GamePad.On(PAD_R1) != 0) {
                                                                        NowCamera__3->AddAngle(-0.034906585f);
                                                                    }

                                                                    if (GamePad.On(PAD_L1) != 0) {
                                                                        NowCamera__3->AddAngle(0.034906585f);
                                                                    }
                                                                }

                                                                if (GamePad.On(PAD_L2) != 0) {
                                                                    sceVu0FMATRIX look;

                                                                    CharaFrame->GetLWMatrix(look);
                                                                    NowCamera__3->SetAngle(atan2f(look[2][0], look[2][2]) - PI_D);
                                                                }

                                                                if (GamePad.Down(PAD_R2) != 0 && BtActStatus.recoil_frames == 0 && BtActStatus.action_busy == 0) {
                                                                    oldCameraAngle = NowCamera__3->GetAngle();
                                                                    oldCameraHeight = NowCamera__3->GetHeight();
                                                                    NowCamera__3->FollowOff();
                                                                    BtActStatus.player_visible = 0;
                                                                    viewMode__2 = 1;
                                                                    InitEyeCamera();

                                                                    if (UserStatus->cur_chara == CHARA_XIAO) {
                                                                        EquipReAttach(NowWeapon, 1);
                                                                    }

                                                                    BtActStatus.unk_028 = 0;
                                                                    BtActStatus.motion_no = 0;
                                                                    BtActStatus.action_on = 0;
                                                                    BtActStatus.action_no = 0;
                                                                    LockOffTargte();
                                                                    ResetMovePower();

                                                                    if (ruby_effect_id != -1 && UserStatus->cur_chara == CHARA_RUBY) {
                                                                        NowMainEffect->OffEffect(ruby_effect_id);
                                                                        ruby_effect_id = -1;
                                                                    }

                                                                    gameTask = GAME_TASK_EYE_CAMERA;
                                                                } else {
                                                                    sceVu0FVECTOR follow;

                                                                    if (pos[1] < -100.0f) {
                                                                        pos[1] += 200.0f;
                                                                    }

                                                                    CharaFrame->SetPosition(pos[0], pos[1], pos[2]);
                                                                    sceVu0CopyVector(follow, pos);
                                                                    follow[0] += 7.0f * ref_off[0];
                                                                    follow[1] += BtActStatus.camera_shake_offset + (6.0f + (2.0f * ref_off[1] + reference[1]));
                                                                    follow[2] += 7.0f * ref_off[2];
                                                                    NowCamera__3->SetFollow(follow[0], follow[1], follow[2]);

                                                                    if (lockOnTargetFlag != 0) {
                                                                        CCharacter   *locked = &NowMonstorUnit
                                                                                                    ->chara[lockOnTargetNo][0];
                                                                        sceVu0FVECTOR to_target;
                                                                        sceVu0FVECTOR target;

                                                                        locked->GetPosition(target);
                                                                        float dist = DistVector(target, pos);
                                                                        to_target[0] = target[0] - pos[0];
                                                                        to_target[1] = target[1] - pos[1];
                                                                        to_target[2] = target[2] - pos[2];
                                                                        to_target[3] = 1.0f;

                                                                        if (dist >= 20.0f) {
                                                                            dist = 20.0f;
                                                                        }

                                                                        sceVu0Normalize(to_target, to_target);
                                                                        sceVu0ScaleVectorXYZ(to_target, to_target, dist);
                                                                        to_target[1] = 0.0f;
                                                                        NowCamera__3->SetFollow(pos[0] + to_target[0], BtActStatus.camera_shake_offset + (to_target[1] + (6.0f + pos[1] + reference[1])), pos[2] + to_target[2]);
                                                                    }

                                                                    float normal_near = camera_up_near_dist;
                                                                    float zoom_start_distance = NowCamera__3->GetDistance();
                                                                    camera_up_near_dist = DungeonZoomNearDistance(NowCamera__3, normal_near, lockOnTargetFlag != 0);
                                                                    autoCamTrial();
                                                                    camera_up_near_dist = normal_near;
                                                                    DungeonZoomApply(NowCamera__3, NowDngMap, lockOnTargetFlag != 0, zoom_start_distance);
                                                                }
                                                            }
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            break;
        case GAME_TASK_SCRIPT_RESTART:
            gameTask = GAME_TASK_SCRIPT_START;
            break;
        case GAME_TASK_FLOOR_FADE_RESET:
            if (EdFadeOutCheck() != 0) {
                CSHOT_EFFECT *effects = NowShotEffect->effect;

                for (int i = 0; i < 5; i++) {
                    effects[i].Initialize();
                }

                NowMonstorUnit->CleanViewMonstor(BtUraDongeon);

                CMonstorUnit *unit = NowMonstorUnit;

                unit->requested_event = -1;
                driveStepHold = 0;
                gameTask = GAME_TASK_SCRIPT_START;
            }

            break;
        case GAME_TASK_SCRIPT_START:
            LockOffTargte();
            CharaMain.motion_no = 0;
            CharaMain.motion_flags = 0;
            CharaMain.motion_speed = -1.0f;
            BtActStatus.action_on = 0;
            DngMessMan.enabled = false;
            DngMessMan.message = -1;
            DngMessMan.timer = 0;
            DngMessMan.steev_window = false;
            DngMessMan.steev_index = 0;
            DngMessMan.hide_timer = 0;
            DngMes1.mes_made = -1;
            DngMes2.mes_made = -1;
            DngMesStb.mes_made = -1;
            BtEventInfo.running_script_no = BtEventInfo.script_no;
            BtEventInfo.script_no = -1;
            ResetMovePower();
            BtSystemScriptInit();

            if (BtEventInfo.ext_memory == 0) {
                printf("********** system mem !!!\n");
                BtCashBuffer.base = BtScriptWorkBuffer.base;
                BtCashBuffer.limit = 0x186A0;
                BtCashBuffer.used = 0;
            }

            if (BtEventInfo.ext_memory != 0) {
                printf("********** ext mem !!!\n");
                TexManager.DeleteTextureBlock(0x2A);
                TexManager.DeleteTextureBlock(0x26);
                TexManager.CleanUpBuffer();
                TexManager.CleanUpTextureList();
                CSHOT_EFFECT *effects = NowShotEffect->effect;

                for (int i = 0; i < 5; i++) {
                    effects[i].Initialize();
                }

                MainMonstorUnit.CleanViewMonstor(BtUraDongeon);
                MonstorModelBuffer.used = 0;
                u8 *cash = MonstorModelBuffer.base;
                u32 cash_size = MonstorModelBuffer.limit;

                BtCashBuffer.base = cash;
                BtCashBuffer.limit = cash_size + 0x88B8;
                BtCashBuffer.used = 0;
                read_buffer = old_read_buffer + 0x88B80 / 4;
            }

            if (BtSystemScriptRun(BtEventInfo.running_script_no, &BtCashBuffer) == 0) {
                BtSystemScriptAfter();
                gameTask = GAME_TASK_PLAY;
            } else {
                gameTask++;
            }

            break;
        case GAME_TASK_SCRIPT_RUN:
            if (EdEventMode(NowCamera__3, 0) > 0) {
                BtSystemScriptAfter();
                read_buffer = old_read_buffer;
                DngMes1.mes_made = -1;
                DngMes2.mes_made = -1;
                DngMesStb.mes_made = -1;
                Mes1MakeFlg = 1;
                Mes2MakeFlg = 1;
                DngMessMan.message = -1;
                DngMessMan.timer = 0;
                DngMessMan.steev_window = false;
                DngMessMan.steev_index = 0;
                DngMessMan.hide_timer = 0;
                DngMessMan.enabled = true;
                gameTask = GAME_TASK_PLAY;
                printf("exit script\n");

                if (EdEventInfo.return_code == ED_EVENT_RETURN_MAP_JUMP) {
                    existFlag = 1;
                }
            }

            SetBattleStyle(selectMapNo, 1);

            switch (BtEventInfo.request) {
                case BT_REQUEST_ITEM_WINDOW:
                    BtEventInfo.request = BT_REQUEST_NONE;
                    BtMiniItemSelect();
                    gameTask = GAME_TASK_SCRIPT_ITEM_SELECT;
                    break;
                case BT_REQUEST_URA_DUNGEON:
                    BtEventInfo.request = BT_REQUEST_NONE;
                    gameTask = GAME_TASK_URA_SWAP;
                    break;
                case BT_REQUEST_ENTRANCE_WINDOW:
                    BtEventInfo.request = BT_REQUEST_NONE;
                    InitDunEnterMenu(0x17, selectMapNo, app::GameTakeResumeFloor());
                    BtGameModeFlag = BT_GAME_MODE_ENTRANCE_MENU;
                    break;
                case BT_REQUEST_ESCAPE_WINDOW:
                    BtEventInfo.request = BT_REQUEST_NONE;
                    DngEscapeMsgInit(&DngMes2, &DngMes1, 0);
                    BtGameModeFlag = BT_GAME_MODE_ESCAPE_MSG;
                    break;
                case BT_REQUEST_GO_DUNGEON:
                    BtEventInfo.request = BT_REQUEST_NONE;
                    BtSystemScriptAfter();
                    read_buffer = old_read_buffer;
                    ClearGateKeyStack();
                    SaveData->AddNowTime(1.0f);
                    DngMes1.mes_made = -1;
                    DngMes2.mes_made = -1;
                    DngMesStb.mes_made = -1;
                    Mes1MakeFlg = 1;
                    Mes2MakeFlg = 1;
                    DngMessMan.message = -1;
                    DngMessMan.timer = 0;
                    DngMessMan.steev_window = false;
                    DngMessMan.steev_index = 0;
                    DngMessMan.hide_timer = 0;
                    DngMessMan.enabled = true;

                    if (UserStatus->res_limit_zone_current != RES_LIMIT_ZONE_NONE) {
                        DngMessMan.LimmitZone();
                        SndSPSePlay(0x1B, -1);
                    }

                    if (UserStatus->CheckLife() <= 0) {
                        s8 chara = UserStatus->cur_chara;

                        UserStatus->hp[chara] = 1;
                    }

                    rogoSwitch2 = 1;
                    infoMap = 0;
                    infoMapOld = 0;
                    rogoY3 = -0x60;
                    startCnt2 = 0;
                    gameTask = GAME_TASK_PLAY;
                    printf("go dungeon\n");
                    app::GameAutosaveFloor();
                    break;
                case BT_REQUEST_RUN_SCRIPT:
                    BtEventInfo.request = BT_REQUEST_NONE;
                    BtSystemScriptAfter();
                    BtEventInfo.script_no = BtEventInfo.chained_script_no;
                    gameTask = GAME_TASK_SCRIPT_START;
                    break;
                default:
                    break;
            }

            break;
        case GAME_TASK_SCRIPT_ITEM_SELECT:
            if (BtMiniItemSelect_Loop() != 0) {
                gameTask = GAME_TASK_SCRIPT_RUN;
                DngMes1.mes_made = -1;
                DngMes2.mes_made = -1;
                DngMesStb.mes_made = -1;
            }

            break;
        case GAME_TASK_URA_SWAP: {
            int           ura = BtUraDongeon;
            CMonstorUnit *unit = NowMonstorUnit;

            unit->model_count = 0;
            unit->current_monster = 0;

            for (int i = 0; i < 16; i++) {
                unit->script[i] = &MonstorScriptBuffer[i];
            }

            unit->requested_event = -1;
            unit->CleanViewMonstor(ura);
            BtSetEventExtendTable();

            if (BtUraDongeon == 0) {
                BtUraDongeon = 1;
                NowDngMap = &UraDungeonMap;
                NowEventMan = &UraEventMan;
                NowDngMap->RsetMimicEvent();
                RandomItem = &SubRandomItem;
                NowMonstorUnit->CleanViewMonstor(BtUraDongeon);

                int           ura = BtUraDongeon;
                CMonstorUnit *unit_now = NowMonstorUnit;

                unit_now->model_count = 0;
                unit_now->current_monster = 0;

                for (int i = 0; i < 16; i++) {
                    unit_now->script[i] = &MonstorScriptBuffer[i];
                }

                unit_now->requested_event = -1;
                unit_now->CleanViewMonstor(ura);
                BtSetEventExtendTable();
                BtLoadMonstor(1);
                NowMonstorUnit->ArrangementPos(&UraDungeonMap, 8, -1, 0);
                NowDngMap->DrawMapCalc(NowDngMap->map_type);
                lightingMode = 1;
            } else {
                BtUraDongeon = 0;
                NowDngMap = &MainDungeonMap;
                NowEventMan = &DngEventMan;
                NowMonstorUnit = &MainMonstorUnit;
                NowDngMap->RsetMimicEvent();
                RandomItem = &MainRandomItem;
                MainMonstorUnit.CleanViewMonstor(BtUraDongeon);

                int ura = BtUraDongeon;

                MainMonstorUnit.model_count = 0;
                MainMonstorUnit.current_monster = 0;

                for (int i = 0; i < 16; i++) {
                    MainMonstorUnit.script[i] = &MonstorScriptBuffer[i];
                }

                MainMonstorUnit.requested_event = -1;
                MainMonstorUnit.CleanViewMonstor(ura);
                BtSetEventExtendTable();
                BtLoadMonstor(0);
                MainMonstorUnit.ArrangementPos(&MainDungeonMap, 0xF, -1, 0);
                NowDngMap->DrawMapCalc(NowDngMap->map_type);
                lightingMode = 0;
            }

            gameTask = GAME_TASK_SCRIPT_RUN;
            break;
        }
        case GAME_TASK_DEBUG_OPEN:
            driveStepHold = 1;
            CMonUnitHold = 1;
            CEffectHold = 1;
            DebugStatus[0] = 1;
            GamePad.SetAutoRepeat(PAD_L2 | PAD_R2 | PAD_L1 | PAD_R1 | PAD_DPAD, 0x14, 3);
            gameTask++;
            break;
        case GAME_TASK_DEBUG_MENU:
            switch (DebugInfomationIF()) {
                case DEBUG_INFO_UNK_28:
                    GamePad.AutoRepeatOff();
                    driveStepHold = 0;
                    CMonUnitHold = 0;
                    CEffectHold = 0;
                    gameTask++;
                    break;
                case DEBUG_INFO_UNK_3C:
                    GamePad.AutoRepeatOff();
                    driveStepHold = 0;
                    CMonUnitHold = 0;
                    CEffectHold = 0;
                    gameTask = GAME_TASK_DEBUG_RELOAD_MONSTER;
                    break;
                case DEBUG_INFO_RELOAD_ENEMY:
                    GamePad.AutoRepeatOff();
                    driveStepHold = 0;
                    CMonUnitHold = 0;
                    CEffectHold = 0;
                    gameTask = GAME_TASK_DEBUG_RELOAD_MONSTER_SET;
                    break;
                case DEBUG_INFO_SET_CHR_KEY:
                    GamePad.AutoRepeatOff();
                    driveStepHold = 0;
                    CMonUnitHold = 0;
                    CEffectHold = 0;
                    gameTask = GAME_TASK_PLAY;
                    break;
                case DEBUG_INFO_CLOSE:
                    GamePad.AutoRepeatOff();
                    driveStepHold = 0;
                    CMonUnitHold = 0;
                    CEffectHold = 0;
                    lightingMode = DebugStatus[16];
                    gameTask = GAME_TASK_PLAY;
                    break;
                case DEBUG_INFO_FLOOR_ATRA_GET:
                    GamePad.AutoRepeatOff();

                    for (const ATRA_BOLL &ball : NowDngMap->atra) {
                        int atra_no = ball.atra_no;

                        if (atra_no != -1) {
                            int atra = UserStatus->atra_registry[selectMapNo][atra_no].id;

                            getAtraToSaveData(atra, atra_no, SaveData, selectMapNo, UserStatus->cur_floor);
                        }
                    }

                    driveStepHold = 0;
                    CMonUnitHold = 0;
                    CEffectHold = 0;
                    gameTask = GAME_TASK_PLAY;
                    break;
                case DEBUG_INFO_EVENT_TEST:
                    GamePad.AutoRepeatOff();
                    driveStepHold = 0;
                    CMonUnitHold = 0;
                    CEffectHold = 0;
                    gameTask++;
                    break;
                case DEBUG_INFO_SET_STATUS:
                    GamePad.AutoRepeatOff();

                    switch (DebugStatus[17]) {
                        case 0:
                            UserStatus->ailments[UserStatus->cur_chara] = 0;
                            UserStatus->ailment_frames[UserStatus->cur_chara] = 0;
                            break;
                        case 1:
                            BtSetStatusErr(AILMENT_FREEZE);
                            break;
                        case 2:
                            BtSetStatusErr(AILMENT_STAMINA);
                            break;
                        case 3:
                            BtSetStatusErr(AILMENT_POISON);
                            break;
                        case 4:
                            BtSetStatusErr(AILMENT_CURSE);
                            break;
                        case 5:
                            BtSetStatusErr(AILMENT_GOO);
                            break;
                        default:
                            break;
                    }

                    driveStepHold = 0;
                    CMonUnitHold = 0;
                    CEffectHold = 0;
                    gameTask = GAME_TASK_PLAY;
                    break;
                default:
                    break;
            }

            break;
        case GAME_TASK_DEBUG_RUN_EVENT:
            BtEventInfo.script_no = DebugStatus[11];
            BtEventInfo.ext_memory = DebugStatus[12];
            BtEventInfo.action_mode = 0;
            BtSystemScriptLoad(selectMapNo);
            gameTask = GAME_TASK_SCRIPT_START;
            break;
        case GAME_TASK_DEBUG_RELOAD_MONSTER: {
            int           ura = BtUraDongeon;
            CMonstorUnit *unit = NowMonstorUnit;

            unit->model_count = 0;
            unit->current_monster = 0;

            for (int i = 0; i < 16; i++) {
                unit->script[i] = &MonstorScriptBuffer[i];
            }

            unit->requested_event = -1;
            unit->CleanViewMonstor(ura);
            BtSetEventExtendTable();
            BtLoadMonstor(0);
            NowMonstorUnit->CleanViewMonstor(BtUraDongeon);
            NowMonstorUnit->ArrangementPos(NowDngMap, DebugStatus[13], -1, 0);
            gameTask = GAME_TASK_PLAY;
            break;
        }
        case GAME_TASK_DEBUG_RELOAD_MONSTER_SET: {
            int           ura = BtUraDongeon;
            CMonstorUnit *unit = NowMonstorUnit;

            unit->model_count = 0;
            unit->current_monster = 0;

            for (int i = 0; i < 16; i++) {
                unit->script[i] = &MonstorScriptBuffer[i];
            }

            unit->requested_event = -1;
            unit->CleanViewMonstor(ura);
            BtSetEventExtendTable();
            BtLoadMonstor(0);
            NowMonstorUnit->CleanViewMonstor(BtUraDongeon);
            NowMonstorUnit->ArrangementPos(NowDngMap, DebugStatus[15], DebugStatus[14], 0);
            gameTask = GAME_TASK_PLAY;
            break;
        }
        case GAME_TASK_PAUSE:
            if (GamePad.Down(PAD_START) != 0) {
                driveStepHold = 0;
                exitMenuFlag = 0;
                CMonUnitHold = 0;
                CEffectHold = 0;
                SndSePlay(MENU_SOUND_CONFIRM, -1, 0);
                PlayTimeCountFlag(1);

                if (viewMode__2 != 0) {
                    gameTask = GAME_TASK_EYE_CAMERA;
                } else {
                    gameTask = GAME_TASK_PLAY;
                }
            }

            break;
        case GAME_TASK_MAPJUMP_FADE_START:
            DispFade__3.FadeInit(0.0f);
            DispFade__3.FadeOutStart(8.0f);
            autoCamTrial();
            gameTask++;
            break;
        case GAME_TASK_MAPJUMP_FADE_WAIT:
            autoCamTrial();

            if (DispFade__3.GetRate() >= 128.0f) {
                MapJump(0x320, -1);
                existFlag = 1;
            }

            break;
        case GAME_TASK_ESCAPE_INIT:
            BtEscape_Init();
            gameTask++;
            break;
        case GAME_TASK_ESCAPE_LOOP:
            if (BtEscape_Loop() != 0) {
                DispFade__3.FadeInit(128.0f);
                gameTask = GAME_TASK_EXIT_FADE_WAIT;
            }

            break;
        case GAME_TASK_EXIT_FADE_START:
            DispFade__3.FadeInit(0.0f);
            DispFade__3.FadeOutStart(8.0f);
            autoCamTrial();
            gameTask++;
            break;
        case GAME_TASK_EXIT_FADE_WAIT:
            autoCamTrial();

            if (DispFade__3.GetRate() >= 128.0f) {
                int map_no = 0;

                switch (selectMapNo) {
                    case DUNGEON_DIVINE_BEAST_CAVE:
                        map_no = 0;
                        break;
                    case DUNGEON_WISE_OWL_FOREST:
                        map_no = 1;
                        break;
                    case DUNGEON_SHIPWRECK:
                        map_no = 0x13;
                        break;
                    case DUNGEON_SUN_MOON_TEMPLE:
                        map_no = 0x2A;
                        break;
                    case DUNGEON_MOON_SEA:
                        map_no = 0x17;
                        break;
                    case DUNGEON_GALLERY_OF_TIME:
                        map_no = 0x26;
                        break;
                    case DUNGEON_DEMON_SHAFT:
                        map_no = 0x3C;
                        break;
                    default:
                        break;
                }

                MapJump(map_no, -1);
                existFlag = 2;
            }

            break;
        case GAME_TASK_SYSMES_WAIT:
            if (GamePad.Down(PadInput_OK | PadInput_NO) != 0) {
                ClearSystemMes();
                driveStepHold = 0;
                CMonUnitHold = 0;
                CMonUnitHyde = 0;
                CEffectHold = 0;
                CEffectHyde = 0;
                gameTask = GAME_TASK_PLAY;
            }

            autoCamTrial();
            break;
        case GAME_TASK_GATE_KEY_LOOP:
            if (BtGetGateKey_Loop() != 0) {
                gameTask = GAME_TASK_PLAY;
            }

            autoCamTrial();
            break;
        case GAME_TASK_ATTACH_LOOP:
            if (BtGetAttach_Loop() != 0) {
                gameTask = GAME_TASK_PLAY;
            }

            break;
        case GAME_TASK_EYE_CAMERA:
            EyeCamera();

            if (GamePad.Down(PAD_START) != 0) {
                exitMenuFlag = 1;
                driveStepHold = 1;
                CMonUnitHold = 1;
                CEffectHold = 1;
                PlayTimeCountFlag(0);
                SndSePlay(MENU_SOUND_CONFIRM, -1, 0);
                gameTask = GAME_TASK_PAUSE;
            } else {
                if (StatusErrCheck(AILMENT_FREEZE) != 0) {
                    BtActStatus.movement_locked = 1;
                }

                HealingWater();

                CUserStatus *drain_status = UserStatus;

                int hand_ok = 1;
                drain_status->water_drain_disable = 1;
                int next_task = GAME_TASK_PLAY;
                int leaving = 0;

                bool can_walk = FirstPersonDungeonKeyboardVelocity(velo__2, viewAngleH__2, BtActStatus,
                                                                   driveStepHold || CMonUnitHold, UserStatus->CheckLife() > 0, StatusErrCheck(AILMENT_GOO) != 0);
                NowMonstorUnit->MoveCheck(pos, velo__2, lockOnTargetFlag);
                MoveCheckInfo info{};
                sceVu0FVECTOR before;
                sceVu0CopyVector(before, pos);
                auto *fields = NowDngMap->map_type != 1 && selectMapNo == DUNGEON_DIVINE_BEAST_CAVE ? reinterpret_cast<DRAN_MAP_FIELD_SET *>(NowDranMapField) : nullptr;
                if (FirstPersonDungeonStep(*NowDngMap, fields, pos, velo__2, info, UserStatus->cur_chara == CHARA_OSMOND ? 8 : 1)) {
                    sceVu0SubVector(ref_off, pos, before);
                    veloOld[1] = velo__2[1];
                    velo__2[1] = info.landed ? 0.0f : std::max(-10.0f, velo__2[1] - 0.1f);
                    if (info.landed) {
                        BtActStatus.foot_sound = info.poly.attr.foot_sound;
                        BtActStatus.ground_kind = info.poly.attr.ground_kind;
                    }
                    if (info.ground_found) {
                        BtActStatus.ground_height = pos[1] - info.ground_point[1];
                    }
                    CharaMain.SetPosition(pos);
                    sceVu0FVECTOR eye;
                    sceVu0FVECTOR look;
                    NowCamera__3->GetPos(eye);
                    NowCamera__3->GetRef(look);
                    sceVu0AddVector(eye, eye, ref_off);
                    sceVu0AddVector(look, look, ref_off);
                    NowCamera__3->SetPos(eye);
                    NowCamera__3->SetRef(look);
                    static float walked = 0.0f;
                    if (can_walk && info.landed && UserStatus->cur_chara != CHARA_OSMOND) {
                        walked += hypotf(ref_off[0], ref_off[2]);
                        if (walked >= 10.0f) {
                            static int foot = 0;
                            SndPlayFootSound(BtActStatus.foot_sound, foot, pos);
                            foot ^= 1;
                            walked = fmodf(walked, 10.0f);
                        }
                    } else {
                        walked = 0.0f;
                    }
                    if (NowDngMap->map_type != 1 && selectMapNo == DUNGEON_DIVINE_BEAST_CAVE && pos[1] <= -30.0f) {
                        leaving = 1;
                    }
                }

                if (GamePad.Down(PadInput_OK) != 0 && BtActStatus.movement_locked == 0 && BtActStatus.in_water != 0) {
                    SetSystemMes(0x47, 0x5A, MES_POS_BOTTOM, 0, nullptr, nullptr);
                    DngMessMan.hide_timer = 0x5A;
                    hand_ok = 0;
                }

                if (GamePad.Down(PadInput_OK) != 0 && BtActStatus.ground_kind == 0xA && BtActStatus.movement_locked == 0 && UserStatus->cur_chara == CHARA_OSMOND) {
                    SetSystemMes(0x50, 0x5A, MES_POS_BOTTOM, 0, nullptr, nullptr);
                    DngMessMan.hide_timer = 0x5A;
                    hand_ok = 0;
                }

                s8 who = UserStatus->cur_chara;

                if ((UserStatus->cur_chara == CHARA_XIAO || who == CHARA_RUBY || who == CHARA_OSMOND) && hand_ok != 0 && BtActStatus.movement_locked == 0) {
                    float head = viewAngleH__2;

                    if (head - PI <= -PI) {
                        head = viewAngleH__2;
                    }

                    CharaHand.SetRotation(viewAngleV__2, head, 0.0f);
                    CharaMainHandViewFlag = 1;

                    switch (UserStatus->cur_chara) {
                        case CHARA_XIAO:
                            if (GamePad.Down(PadInput_OK) != 0) {
                                BattleActionOn_Jinn();
                            }

                            break;
                        case CHARA_RUBY:
                            if (GamePad.Down(PadInput_OK) != 0) {
                                BattleActionOn_Ruby();
                            }

                            break;
                        case CHARA_OSMOND:
                            if (GamePad.Down(PadInput_OK) != 0) {
                                if (BtActStatus.gun_type == 0) {
                                    BattleActionOn_Ozumond();
                                }

                                if (BtActStatus.gun_type == 1) {
                                    BattleActionOn_Ozumond_H();
                                }

                                if (BtActStatus.gun_type == 2) {
                                    BattleActionOn_Ozumond_F();
                                }
                            }

                            break;
                        default:
                            break;
                    }

                    if (BtActStatus.action_on == 1) {
                        switch (UserStatus->cur_chara) {
                            case CHARA_XIAO:
                                BattleActionPlay_Jinn(&CharaHand, 1);
                                break;
                            case CHARA_RUBY:
                                BattleActionPlay_Ruby(&CharaHand, 1);
                                break;
                            case CHARA_OSMOND:
                                if (BtActStatus.gun_type == 0) {
                                    BattleActionPlay_Ozumond(1);
                                }

                                if (BtActStatus.gun_type == 1) {
                                    BattleActionPlay_Ozumond_H(1);
                                }

                                if (BtActStatus.gun_type == 2) {
                                    BattleActionPlay_Ozumond_F(1);
                                }

                                break;
                            default:
                                break;
                        }
                    }
                }

                SetBattleStyle(selectMapNo, 0);

                bool check_eye_event = true;
                if (GamePad.Down(PAD_TRIANGLE) != 0) {
                    int ok = 1;

                    if (BtActStatus.action_on != 0) {
                        ok = 0;
                    }

                    if (BtActStatus.movement_locked != 0) {
                        ok = 1;
                    }

                    if (UserStatus->hp[UserStatus->cur_chara] <= 0) {
                        ok = 0;
                    }

                    if (ok != 0) {
                        gameTask = GAME_TASK_MENU_OPEN;
                        driveStepHold = 1;
                        SetMIniMapStatus(0);
                        iventInfo = DNG_EVENT_NONE;
                        rogoSwitch2 = 0;
                        SndSePlay(MENU_SOUND_CONFIRM, -1, 0);
                        check_eye_event = false;
                    }
                }

                if (check_eye_event) {
                    CDungeonEventData *state = NowEventMan->SearchDataSlotPos(pos);

                    if (state != nullptr && state->event->chara_no != -1) {
                        sceVu0FVECTOR slot_pos;
                        sceVu0FVECTOR slot_dir;

                        BtEventInfo.script_no = -1;

                        if (state->event->script_no != -1) {
                            BtEventInfo.script_no = state->event->script_no;
                            BtEventInfo.ext_memory = state->event->ext_mem;
                            BtEventInfo.event_marker = true;
                            BtEventInfo.action_mode = 0;
                            sceVu0CopyVector(slot_pos, state->pos);
                            sceVu0CopyVector(slot_dir, state->dir);
                            sceVu0CopyVector(BtEventInfo.position, slot_pos);
                            sceVu0CopyVector(BtEventInfo.direction, slot_dir);
                        }

                        if (BtEventInfo.script_no != -1) {
                            leaving = 1;
                            BtEventInfo.action_mode = 1;
                            BtEventInfo.chara_help = false;
                            next_task = GAME_TASK_SCRIPT_START;
                        }
                    }

                    if (NowMonstorUnit->requested_event != -1) {
                        BtEventInfo.script_no = NowMonstorUnit->requested_event;
                        BtEventInfo.ext_memory = true;
                        ResetStatusInfo();
                        driveStepHold = 1;
                        EdFadeInit();
                        EdFadeOut(0x78, 0.0f, 0.0f, 0.0f);
                        BtEventInfo.fade_on_start = false;
                        next_task = GAME_TASK_FLOOR_FADE_RESET;
                        leaving = 1;
                    }

                    if (UserStatus->CheckLife() != 0) {
                        int event = NowMonstorUnit->CheckEventFlag2();

                        if (event != -1) {
                            BtEventInfo.script_no = event;
                            BtEventInfo.ext_memory = false;
                            BtActStatus.action_busy = 0;
                            next_task = GAME_TASK_SCRIPT_START;
                            leaving = 1;
                        }
                    }

                    if (UserStatus->CheckLife() != 0 && NowMonstorUnit->GetMonstorNum() <= 0 && BtEventInfo.clear_script_no != -1) {
                        BtEventInfo.script_no = BtEventInfo.clear_script_no;
                        BtEventInfo.ext_memory = BtEventInfo.clear_script_ext_memory;
                        BtEventInfo.clear_script_no = -1;
                        BtEventInfo.clear_script_ext_memory = 0;
                        BtActStatus.action_busy = 0;
                        printf("dead script !!\n");
                        next_task = GAME_TASK_SCRIPT_START;
                        leaving = 1;
                    }

                    if (UserStatus->CheckLife() != 0) {
                        int stolen = StealItem.checkEvent();

                        if (stolen != -1) {
                            BtGetAttach_Init(selectMapNo, stolen);
                            DngMessMan.hide_timer = 0x78;
                            SndSePlay(SE_ITEM_GET, -1, 0);
                        }
                    }

                    if (UserStatus->CheckLife() <= 0 && BtActStatus.invincible_frames == 0) {
                        UserStatus->hp[UserStatus->cur_chara] = 0;
                        DeadKeyWait = 0x168;
                        DeadKeyStartWait = 0x3C;
                        LockOffTargte();
                        next_task = GAME_TASK_DEAD;
                        leaving = 1;
                    }

                    entry_no = BtCheckDamageProc();

                    if (entry_no != 0) {
                        leaving = 1;
                    }

                    if (GamePad.Down(PAD_R2) != 0 || GamePad.Down(PAD_L1) != 0 || GamePad.Down(PadInput_NO) != 0 || leaving != 0) {
                        sceVu0FVECTOR chara_rot;

                        CharaMainHandViewFlag = 0;
                        NowCamera__3->FollowOn();
                        NowCamera__3->SetHeight(oldCameraHeight);
                        NowCamera__3->SetSpeed(1.0f);
                        NowCamera__3->Step(1);
                        NowCamera__3->SetSpeed(8.0f);
                        CharaMain.GetRotation(chara_rot);
                        chara_rot[1] = viewAngleH__2;
                        CharaMain.SetRotation(chara_rot);
                        BtActStatus.player_visible = 1;
                        viewMode__2 = 0;
                        cameraAuto = cameraAutoOld;

                        if (UserStatus->cur_chara == CHARA_XIAO) {
                            EquipReAttach(NowWeapon, 0);
                        }

                        if (ruby_effect_id != -1 && UserStatus->cur_chara == CHARA_RUBY) {
                            NowMainEffect->OffEffect(ruby_effect_id);
                            ruby_effect_id = -1;
                        }

                        if (entry_no == 0) {
                            BtActStatus.unk_028 = 0;
                            BtActStatus.motion_no = 0;
                            BtActStatus.action_on = 0;
                        }

                        gameTask = next_task;
                    }
                }
            }

            break;
        case GAME_TASK_MENU_OPEN:
            oldRogoY3 = rogoY3;
            rogoY3 = -0x60;
            iventInfo = DNG_EVENT_NONE;
            oldMsgNo2 = -1;
            oldMsgNo = -1;
            battleMenuWait = 0;
            MonstorNameOff = 1;
            NowCamera__3->SetSpeed(0.0f);
            DngMessMan.enabled = false;
            DngMessMan.message = -1;
            DngMessMan.timer = 0;
            DngMessMan.steev_window = false;
            DngMessMan.steev_index = 0;
            DngMessMan.hide_timer = 0;
            ClearSystemMes();
            driveStepHold = 1;
            gameTask++;
            break;
        case GAME_TASK_MENU_WAIT_FRAME:
            if (battleMenuWait < 3) {
                battleMenuWait++;
            } else {
                frameCaputer = 1;
                gameTask += 2;
            }

            break;
        case GAME_TASK_MENU_SKIP:
            gameTask++;
            break;
        case GAME_TASK_MENU_INIT: {
            frameCaputer = 0;

#ifdef PAL
            s32 menu[6] = {0x17, 0x18, 0x19, 0x28, 0x29};
#else
            s32 menu[5] = {0x17, 0x18, 0x19, 0x28, 0x29};
#endif

            BattleMenuInit(menu, BATTLE_MENU_MODE_DUNGEON);
            BtGameModeFlag = BT_GAME_MODE_BATTLE_MENU;
            oldUnitNow = nowUnitNow;
            NowCamera__3->SetSpeed(8.0f);
            driveStepHold = 0;
            rogoY3 = 0x20;
            DngMes1.mes_made = -1;
            DngMes2.mes_made = -1;
            DngMesStb.mes_made = -1;
            Mes1MakeFlg = 1;
            Mes2MakeFlg = 1;
            DngMessMan.enabled = true;
            DngMessMan.message = -1;
            DngMessMan.timer = 0;
            DngMessMan.steev_window = false;
            DngMessMan.steev_index = 0;
            DngMessMan.hide_timer = 0xA;
            MonstorNameOff = 0;
            EnemyLifeGage.draw = true;
            autoCamTrial();
            gameTask++;
            break;
        }
        case GAME_TASK_MENU_CLOSE:
            if (MenuMapJumpMode != 0) {
                gameTask = GAME_TASK_ESCAPE_INIT;
                autoCamTrial();
            } else {
                if (UserStatus->minimap_status != 3) {
                    infoMap = 1;
                    infoMapOld = 1;
                } else {
                    infoMap = 0;
                    infoMapOld = 0;
                }

                nowUnitNow = UserStatus->cur_chara;

                if (oldUnitNow != nowUnitNow) {
                    gameTask = GAME_TASK_CHARA_CHANGED;
                    autoCamTrial();

                    if (viewMode__2 != 0) {
                        sceVu0FVECTOR chara_rot;

                        CharaMainHandViewFlag = 0;
                        NowCamera__3->FollowOn();
                        NowCamera__3->SetHeight(oldCameraHeight);
                        NowCamera__3->SetSpeed(1.0f);
                        NowCamera__3->Step(1);
                        NowCamera__3->SetSpeed(8.0f);
                        CharaMain.GetRotation(chara_rot);
                        chara_rot[1] = viewAngleH__2;
                        CharaMain.SetRotation(chara_rot);
                        BtActStatus.player_visible = 1;
                        viewMode__2 = 0;
                        cameraAuto = cameraAutoOld;

                        if (UserStatus->cur_chara == CHARA_XIAO) {
                            EquipReAttach(NowWeapon, 0);
                        }
                    }
                } else {
                    CWeaponFx.InitSet(NowWeapon->frame, "dcol0", "dcol1");
                    SetWeaponAttachStatus(NowWeaponHave);
                    SetWeaponColor();
                    autoCamTrial();

                    if (viewMode__2 != 0) {
                        gameTask = GAME_TASK_EYE_CAMERA;
                    } else {
                        gameTask = GAME_TASK_PLAY;
                    }
                }
            }

            break;
        case GAME_TASK_CHARA_CHANGED:
            BtActStatus.action_on = 0;
            BtActStatus.motion_no = 0;
            gameTask = GAME_TASK_PLAY;
            NewChangeFx.motion_no = 0;
            NewChangeFx.motion_flags = 6;
            NewChangeFx.motion_speed = -1.0f;
            NewChangeFx.motion_type.state.time = 1.0f;
            NewChangeFxFlag = 1;
            SndSeSeqAllStop();
            printf("se stop !!\n");

            if (BtActStatus.swapped_on_death != 0) {
                BtActStatus.invincible_frames = 0xA0;
                setUnitAmbientAnime(160.0f, 1.0f, 255.0f, 0.0f, 0.0f);
                SndSePlay(SE_SPARKLE, -1, 0);
            } else {
                setUnitAmbientAnime(90.0f, 1.0f, 250.0f, 250.0f, 250.0f);
                SndSePlay(SE_SPARKLE, -1, 0);
            }

            BtActStatus.swapped_on_death = 0;
            autoCamTrial();
            break;
        case GAME_TASK_UNK_123:
            break;
        case GAME_TASK_CHARA_SELECT_LOOP:
            if (BtMiniChrSelect_Loop() != 0) {
                // CharaChangeKey drops the lock-on; it comes back if its monster can still be locked on.
                if (quickChangeLockOn >= 0) {
                    MONSTOR &locked = NowMonstorUnit->monster[quickChangeLockOn];

                    if (locked.state == 2 && locked.hp > 0 && locked.lockon_enabled != 0) {
                        lockOnTargetNo = quickChangeLockOn;
                        lockOnTargetFlag = 1;
                        targetCursorShiftNo = quickChangeShiftNo;
                        targetCursorShiftRot = quickChangeShiftRot;
                    }

                    quickChangeLockOn = -1;
                }

                if (BtMiniChrSelectNo == 2) {
                    ClearSystemMes();
                    driveStepHold = 1;
                    gameTask = GAME_TASK_EXIT_FADE_START;
                    static_cast<CDngStatusData *>(UserStatus)->SetDead();
                } else {
                    if (oldUnitNow == nowUnitNow) {
                        DngMes1.mes_made = -1;
                        DngMes2.mes_made = -1;
                        DngMesStb.mes_made = -1;
                        Mes1MakeFlg = 1;
                        Mes2MakeFlg = 1;
                        DngMessMan.enabled = true;
                        MonstorNameOff = 0;
                        EnemyLifeGage.draw = true;
                        gameTask = GAME_TASK_PLAY;
                    } else {
                        DngMes1.mes_made = -1;
                        DngMes2.mes_made = -1;
                        DngMesStb.mes_made = -1;
                        Mes1MakeFlg = 1;
                        Mes2MakeFlg = 1;
                        DngMessMan.enabled = true;
                        MonstorNameOff = 0;
                        EnemyLifeGage.draw = true;
                        gameTask = GAME_TASK_CHARA_CHANGED;
                    }

                    autoCamTrial();
                }
            } else {
                autoCamTrial();
            }

            break;
        case GAME_TASK_BOX_BIG_INIT:
            BtGetTreasureboxBig_Init();
            gameTask++;
            break;
        case GAME_TASK_BOX_BIG_LOOP:
            if (BtGetTreasureboxBig_Loop() != 0) {
                gameTask = GAME_TASK_PLAY;
            }

            break;
        case GAME_TASK_BOX_SMALL_INIT:
            BtGetTreasureboxSmall_Init(selectMapNo);
            gameTask++;
            break;
        case GAME_TASK_BOX_SMALL_LOOP:
            if (BtGetTreasureboxSmall_Loop() != 0) {
                gameTask = GAME_TASK_PLAY;
            }

            break;
        case GAME_TASK_ATRA_GET_INIT:
            BtAtraGetShort_Init();
            iventMarker = 0;
            gameTask++;
            break;
        case GAME_TASK_ATRA_GET_LOOP:
            if (BtAtraGetShort_Loop(selectMapNo, UserStatus->cur_floor) == 1) {
                gameTask = GAME_TASK_PLAY;
            }

            break;
        case GAME_TASK_ATRA_REFUSE_INIT: {
            CMonUnitHold = 1;
            CEffectHold = 1;

            CUserStatus *user = UserStatus;

            user->step_disable = 1;
            DngMessMan.enabled = false;
            NotGetAtraMes(UserStatus->cur_chara, -1);
            gameTask++;
            break;
        }
        case GAME_TASK_ATRA_REFUSE_WAIT:
            if (GamePad.Down(PadInput_OK | PadInput_NO) != 0) {
                CMonUnitHold = 0;
                CEffectHold = 0;
                UserStatus->step_disable = 0;
                DngMessMan.enabled = true;
                ClearSystemMes();
                gameTask = GAME_TASK_PLAY;
            }

            break;
        case GAME_TASK_DEAD: {
            CUserStatus *step_status = UserStatus;

            step_status->step_disable = 1;
            BtActStatus.camera_hold = 1;
            DngMessMan.enabled = false;

            if (ruby_effect_id != -1 && UserStatus->cur_chara == CHARA_RUBY) {
                NowMainEffect->OffEffect(ruby_effect_id);
                ruby_effect_id = -1;
            }

            int slot = static_cast<CDngStatusData *>(UserStatus)->CheckActItemSlot(ITEM_REVIVAL_POWDER);

            if (slot != -1) {
                setUnitAmbientAnime(64.0f, 1.0f, 0.0f, 122.0f, 208.0f);
                s8 chara = UserStatus->cur_chara;
                UserStatus->hp[chara] = UserStatus->max_hp[chara] >> 1;
                UserStatus->ailments[UserStatus->cur_chara] = 0;
                UserStatus->ailment_frames[UserStatus->cur_chara] = 0;
                SetSystemMes(0x3E, -1, MES_POS_BOTTOM, 0, nullptr, nullptr);
                BombInfo.aiming = false;
                BombInfo.throw_step = -1;
                BtActStatus.can_act = 1;
                BtActStatus.action_on = 0;
                BtActStatus.action_busy = 0;

                if (activeItem.model[8] != -1) {
                    activeItem.models->AllReleasItem();
                    activeItem.model[8] = -1;
                }

                BtActStatus.motion_no = 0;
                BtActStatus.camera_hold = 0;
                DngMessMan.enabled = false;
                CMonUnitHold = 1;
                CEffectHold = 1;
                printf("itemno = %d\n", slot);
                gameTask = GAME_TASK_REVIVE_WAIT;
                DelActiveItem(slot + 1);
                autoCamTrial();
            } else {
                BtActStatus.motion_no = 0x17;

                float start = CharaMain.motion_type.motion_info[23].start;
                float end = CharaMain.motion_type.motion_info[23].end;

                if (CharaMain.motion_type.state.time >= 2.0f + start && CharaMain.motion_type.state.time < 2.3f + start) {
                    SndSePlay(SE_CHARA_KNOCKED_OUT, -1, 0);
                }

                if (CharaMain.motion_type.state.time >= end - 2.0f && CharaMain.motion_type.state.time <= end) {
                    CharaMain.motion_no = BtActStatus.motion_no;
                    CharaMain.motion_flags = 1;
                    CharaMain.motion_speed = -1.0f;
                    BombInfo.aiming = false;
                    BombInfo.throw_step = -1;
                    BtActStatus.can_act = 1;
                    BtActStatus.action_on = 0;
                    BtActStatus.action_busy = 0;

                    if (activeItem.model[8] != -1) {
                        activeItem.models->AllReleasItem();
                        activeItem.model[8] = -1;
                    }

                    if (static_cast<CDngStatusData *>(UserStatus)->GetLiveUnit() != 0 && UserStatus->party_size > 1) {
                        gameTask += 2;
                    } else {
                        gameTask++;
                    }
                }

                autoCamTrial();
            }

            break;
        }
        case GAME_TASK_DEAD_GAMEOVER:
            DeadKeyWait--;

            if (DeadKeyStartWait > 0) {
                DeadKeyStartWait--;

                if (DeadKeyStartWait == 0) {
                    if (UserStatus->party_size == 1) {
                        DeadMes(UserStatus->cur_chara, 0x168);
                    } else {
                        AllDeadMes(0x168);
                    }
                }
            }

            if (GamePad.Down(PAD_CIRCLE | PAD_CROSS) != 0 || DeadKeyWait <= 0) {
                ClearSystemMes();
                BtActStatus.camera_hold = 0;

#ifdef PAL
                // In debug mode the party gets up again at full life.
                if (DebugMode != 0) {
                    CUserStatus *status = UserStatus;

                    status->hp[status->cur_chara] = status->max_hp[status->cur_chara];
                    BtActStatus.motion_no = 0;
                    UserStatus->step_disable = 0;
                    DngMessMan.enabled = true;
                    gameTask = GAME_TASK_PLAY;
                    CMonUnitHold = 0;
                    CEffectHold = 0;
                } else {
                    gameTask = GAME_TASK_EXIT_FADE_START;
                    static_cast<CDngStatusData *>(UserStatus)->SetDead();
                }
#else
                gameTask = GAME_TASK_EXIT_FADE_START;
                static_cast<CDngStatusData *>(UserStatus)->SetDead();
#endif
            }

            autoCamTrial();
            break;
        case GAME_TASK_DEAD_CHANGE_CHARA:
            BtActStatus.motion_no = 0x17;

            if (DeadKeyStartWait > 0) {
                DeadKeyStartWait--;

                if (DeadKeyStartWait == 0) {
                    DeadMes(UserStatus->cur_chara, 0x168);
                }
            }

            if (GamePad.Down(PAD_CIRCLE | PAD_CROSS) != 0 || DeadKeyWait <= 0) {
                ClearSystemMes();
                BtMiniChrSelect_Init(1);
                UserStatus->step_disable = 0;
                oldUnitNow = UserStatus->cur_chara;
                BtActStatus.camera_hold = 0;
                BtActStatus.swapped_on_death = 1;
                gameTask = GAME_TASK_CHARA_SELECT_LOOP;
            }

            autoCamTrial();
            break;
        case GAME_TASK_REVIVE_WAIT: {
            s8 chara = UserStatus->cur_chara;

            s16 max_hp = UserStatus->max_hp[chara];

            if (!(UserStatus->hp[chara] < (max_hp >> 1))) {
                CUserStatus *stepper = UserStatus;

                stepper->step_disable = 1;
                gameTask++;
            }

            autoCamTrial();
            break;
        }
        case GAME_TASK_REVIVE_DONE:
            if (GamePad.Down(PAD_CIRCLE | PAD_CROSS) != 0) {
                ClearSystemMes();
                UserStatus->step_disable = 0;
                DngMessMan.enabled = true;
                CMonUnitHold = 0;
                CEffectHold = 0;
                gameTask = GAME_TASK_PLAY;
                BtActStatus.invincible_frames = 0xA0;
                setUnitAmbientAnime(160.0f, 1.0f, 255.0f, 0.0f, 0.0f);
            }

            autoCamTrial();
            break;
        default:
            break;
    }

    if (rogoSwitch2 != 0) {
        switch (startCnt2) {
            case 0:
                if (rogoAlphaA[2] < 0x80) {
                    rogoAlphaA[2] += 4;
                } else {
                    rogoAlphaW[2] = 0x78;
                    startCnt2++;
                }

                if (rogoY3 < 0x20) {
                    rogoY3 += 2;
                }

                break;
            case 1:
                if (rogoAlphaW[2] > 0) {
                    rogoAlphaW[2]--;
                } else {
                    startCnt2++;
                }

                if (rogoY3 < 0x20) {
                    rogoY3 += 2;
                }

                break;
            case 2:
                if (rogoAlphaA[2] > 0) {
                    rogoAlphaA[2] -= 4;
                } else if (rogoY3 >= 0x20) {
                    startCnt2++;
                }

                if (rogoY3 < 0x20) {
                    rogoY3 += 2;
                }

                break;
            case 3:
                rogoSwitch2 = 0;
                startCnt2 = -1;
                break;
            default:
                break;
        }

        if (rogoY3 >= 0x20 && UserStatus->minimap_status != 3) {
            infoMap = 1;
            infoMapOld = 1;
        }
    }

    motionDrive();
}

} // namespace dc::game
