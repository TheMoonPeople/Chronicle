#include "common.h"

#include <libvu0.h>

#include <cstddef>
#include <print>
#include <string>
#include <utility>

#include "btactstatus.hpp"
#include "btitem.hpp"
#include "btitem_port.hpp"
#include "btmisc.hpp"
#include "camerafollow.hpp"
#include "character.hpp"
#include "clsmes.hpp"
#include "collision.hpp"
#include "dataalloc.hpp"
#include "dataread.hpp"
#include "dispctrl.hpp"
#include "dngmessageman.hpp"
#include "dngstatusdata.hpp"
#include "dun/gameloop.hpp"
#include "dungeonmap.hpp"
#include "dungeonparts.hpp"
#include "edit.hpp"
#include "editloop3.hpp"
#include "frame.hpp"
#include "framevu1.hpp"
#include "gamepad.hpp"
#include "mainitemmodel.hpp"
#include "mathutil.hpp"
#include "mds.hpp"
#include "menu_draw.hpp"
#include "menu_dungeon.hpp"
#include "menu_save.hpp"
#include "monstorunit.hpp"
#include "motionmodel.hpp"
#include "nowload.hpp"
#include "savedata.hpp"
#include "snd.hpp"
#include "sysmes.hpp"
#include "texture.hpp"
#include "userstatus.hpp"
#include "vector.hpp"
#include "weaponeffect.hpp"

// Treasure-box, gate-key and escape presentations and the item-select window. Each _Init starts
// background reads into BtCashBuffer and each _Loop consumes the files once the reads finish.

// The file and frame-name parameters of the game APIs used here are non-const char *.
#pragma clang diagnostic ignored "-Wwritable-strings"

namespace {

constexpr const char *chara_files[] = {"dun/mainchara/c01d_ex00.chr", "dun/mainchara/c04b_ex00.chr",
                                       "dun/mainchara/c06b_ex00.chr", "dun/mainchara/c05a_ex00.chr",
                                       "dun/mainchara/c10b_ex00.chr", "dun/mainchara/c18a_ex00.chr"};

CDngStatusData *DngStatus() {
    return static_cast<CDngStatusData *>(UserStatus);
}

void ResetCashBuffer() {
    BtCashBuffer.base = reinterpret_cast<u_char *>(read_buffer);
    BtCashBuffer.limit = 0x445C0;
    BtCashBuffer.used = 0;
}

u_char *NextCashBlock() {
    return BtCashBuffer.base + static_cast<std::ptrdiff_t>(BtCashBuffer.used) * 16;
}

// Reserves a file's size rounded up past the next 64-byte boundary, in quadwords.
void CommitCashBlock(int size) {
    BtCashBuffer.Alloc((((size >> 6) + 1) << 6) >> 4);
}

std::pair<std::string, std::string> ItemPaths(int item_no) {
    item_no = TransWepNo(item_no);
    std::string model_path;
    if (item_no >= ITEM_WEAPON_START) {
        constexpr const char *prefixes[] = {"c01w", "c04w", "c06w", "c05w", "c10w", "c18w"};
        constexpr int         first_weapons[] = {ITEM_WEAPON_DAGGER_BROKEN, ITEM_WEAPON_WOODENSLINGSHOT_BROKEN,
                                                 ITEM_WEAPON_MALLET_BROKEN, ITEM_WEAPON_GOLD_RING_BROKEN,
                                                 ITEM_WEAPON_FIGHTING_STICK_BROKEN, ITEM_WEAPON_MACHINE_GUN_BROKEN};
        int                   chara = 0;
        while (chara < 5 && item_no >= first_weapons[chara + 1]) {
            ++chara;
        }
        const int offset = item_no - first_weapons[chara];
        model_path = "dun/item/main_wep/";
        model_path += prefixes[chara];
        if (offset < 10) {
            model_path += '0';
        }
        model_path += std::to_string(offset);
    } else {
        if (ITEM_NAME_TBL_NEW[item_no - ITEM_ATTACH_START] == nullptr) {
            item_no = ITEM_REGULAR_WATER;
        }
        model_path = "dun/item/main_data/";
        model_path += ITEM_NAME_TBL_NEW[item_no - ITEM_ATTACH_START];
    }
    const std::string base = model_path;
    model_path += ".mds";
    return {std::move(model_path), base + ".img"};
}

u_char *item_mds;
u_char *item_img;
u_char *escape_file;

} // namespace

RS_STACKDATA *PortItemSelectResult;

void BtGetTreasureboxBig_Init() {
    int item_no = NowDngMap->boxes[NowDngMap->events[iventActive].index].item_no;

    ResetMovePower();

    if (DngStatus()->CheckWeaponRot(item_no) >= 10) {
        SndSePlay(SE_CHEST_OPEN_BIG, -1, 0);
        SetSystemMes(DngStatus()->CheckWeaponUser(item_no) + 0x49, -1, MES_POS_CENTRE, 0, nullptr, nullptr);
        int index = NowDngMap->events[iventActive].index;
        NowDngMap->boxes[index].lid_angle = -20.0f;
        SetMIniMapStatus(0);
        iventInfo = DNG_EVENT_NONE;
        CMonUnitHold = 1;
        CMonUnitHyde = 1;
        CEffectHold = 1;
        CEffectHyde = 1;
        DngMessMan.enabled = false;

        UserStatus->step_disable = 1;
        BtActStatus.motion_no = 0;
        BtActStatus.in_presentation = 1;
        BtGetTreasurebox_Sled = 10;
        autoCamTrial();
        return;
    }

    if (item_no == ITEM_WEAPON_STEVE && PlayerAllItemCheck(ITEM_WEAPON_STEVE) != 0) {
        item_no = ITEM_WEAPON_BONE_SLINGSHOT;
    }

    TreasureboxBig_itemType = TREASUREBOX_POSE_DEFAULT;

    if (DngStatus()->CheckWeaponUser(item_no) == CHARA_XIAO) {
        TreasureboxBig_itemType = TREASUREBOX_POSE_SLINGSHOT;
    }

    if (DngStatus()->CheckWeaponUser(item_no) == CHARA_OSMOND) {
        TreasureboxBig_itemType = TREASUREBOX_POSE_GUN;
    }

    if (DngStatus()->CheckWeaponUser(item_no) == CHARA_RUBY) {
        TreasureboxBig_itemType = TREASUREBOX_POSE_RING;
    }

    auto [model_path, texture_path] = ItemPaths(item_no);
    int size = 0;

    TreasureboxBig_itemNo = item_no;
    NowDngMap->events[iventActive].kind = DNG_EVENT_NONE;
    ResetCashBuffer();
    StartReadBG();
    item_mds = NextCashBlock();
    LoadFileBG(model_path.data(), reinterpret_cast<u_long128 *>(item_mds), &size);
    CommitCashBlock(size);
    item_img = NextCashBlock();
    LoadFileBG(texture_path.data(), reinterpret_cast<u_long128 *>(item_img), &size);
    CommitCashBlock(size);
    u_char *chr = NextCashBlock();
    itemOpenItemChr = reinterpret_cast<u_int *>(chr);
    LoadFileBG(const_cast<char *>(chara_files[static_cast<u_char>(UserStatus->cur_chara)]), reinterpret_cast<u_long128 *>(chr), &size);
    CommitCashBlock(size);
    SndSPSeLoadBG(2, reinterpret_cast<u_int *>(NextCashBlock()), &size);
    CommitCashBlock(size);
    itemOpenBig.frame->SetRotation(0.0f, 0.0f, 0.0f);
    DngMessMan.enabled = false;

    UserStatus->step_disable = 1;
    BtActStatus.motion_no = 0;
    BtActStatus.in_presentation = 1;
    BtGetTreasurebox_Sled = 0;
    autoCamTrial();
}

int BtGetTreasureboxBig_Loop() {
    sceVu0FVECTOR position;
    sceVu0FVECTOR item_position;
    int           done = 0;

    switch (BtGetTreasurebox_Sled) {
        case 0:
            if (SndSPSeSyncBG() == 0 && ReadBGSync() == 0) {
                SetTempTexture(0x1C, reinterpret_cast<char *>(item_img));
                itemBoxModel = LoadMDSFile(reinterpret_cast<u_int *>(item_mds), &BtCashBuffer, 0, nullptr, nullptr);
                CharaMain.LoadPackData(itemOpenItemChr, "base2.cfg", &BtCashBuffer, &BtCashBuffer);
                BtActStatus.shadow_visible = false;
                BtGetTreasurebox_Sled++;
            }

            autoCamTrial();
            break;
        case 1: {
            SetMIniMapStatus(0);
            iventInfo = DNG_EVENT_NONE;
            CMonUnitHold = 1;
            CMonUnitHyde = 1;
            CEffectHold = 1;
            CEffectHyde = 1;
            int index = NowDngMap->events[iventActive].index;
            sceVu0CopyVector(position, NowDngMap->boxes[index].pos);
            NowDngMap->boxes[index].lid_angle = -30.0f;
            NowDngMap->boxes[index].closed = 0;
            itemOpenBig.frame->SetPosition(position);
            itemOpenBigFx.frame->SetPosition(position);
            itemOpenBigFx.frame->SetRotation(0.0f, 0.0f, 0.0f);
            sceVu0CopyVector(item_position, position);
            item_position[0] -= 5.0f;
            item_position[1] += 13.0f;

            switch (TreasureboxBig_itemType) {
                case TREASUREBOX_POSE_DEFAULT:
                    itemBoxModel->SetRotation(HALF_PI, 0.0f, HALF_PI);
                    itemBoxModel->SetPosition(item_position);
                    TreasureboxBig_itemScale = 1.0f;
                    break;
                case TREASUREBOX_POSE_SLINGSHOT:
                    itemBoxModel->SetRotation(-HALF_PI, -HALF_PI, 0.0f);
                    item_position[0] += 3.5f;
                    itemBoxModel->SetPosition(item_position);
                    TreasureboxBig_itemScale = 1.0f;
                    break;
                case TREASUREBOX_POSE_GUN:
                    itemBoxModel->SetRotation(-HALF_PI, PI_SHORT, 0.0f);
                    item_position[0] += 2.5f;
                    itemBoxModel->SetPosition(item_position);
                    TreasureboxBig_itemScale = 1.0f;
                    break;
                case TREASUREBOX_POSE_RING:
                    itemBoxModel->SetRotation(0.0f, HALF_PI_SHORT, 0.0f);
                    item_position[0] += 3.5f;
                    itemBoxModel->SetPosition(item_position);
                    TreasureboxBig_itemScale = 2.0f;
                    break;
                default:
                    break;
            }

            itemBoxModel->SetScale(0.01f, 0.01f, 0.01f);
            itemWeponScale = 0.1f;
            CharaMain.SetPosition(position);
            CharaMain.SetRotation(0.0f, 0.0f, 0.0f);
            BtActStatus.motion_no = 0x2C;
            itemOpenBig.motion.unk_78 = 10.0f;
            itemOpenBigFx.motion.state.time = 10.0f;
            itemOpenBigFlag = 1;
            SubCamera = MainCamera__4;
            NowCamera__3 = &SubCamera;
            NowCamera__3->FollowOff();
            autoCamTrial();
            BtGetTreasurebox_Sled++;
            break;
        }
        case 2: {
            CCameraFollow *camera = NowCamera__3;
            setCameraPassData(static_cast<CFrameVu1 *>(itemOpenBigFx.frame), camera, "cam", "int");
#ifdef PAL
            float pose_time = CharaMain.GetNowTime();

            if (!(pose_time < 79.0f) && pose_time <= 80.0f) {
                BtActStatus.motion_no = 0x2D;
            }

#endif
            float sound_time = itemOpenBigFx.motion.state.time;

            if (!(sound_time <= 14.0f) && sound_time < 15.0f) {
                SndSePlay(SE_CHEST_OPEN_BIG, -1, 0);
                SndSPSePlay(2, -1);
            }

            float grow_time = itemOpenBigFx.motion.state.time;

            if (!(grow_time <= 50.0f) && grow_time <= 55.0f) {
                float scale = TreasureboxBig_itemScale / 5.0f * (grow_time - 50.0f);
                itemWeponScale = scale;
                itemBoxModel->SetScale(scale, scale, scale);
            }

            if (!(itemOpenBigFx.motion.state.time < 79.0f)) {
                itemOpenBigFlag = 2;
                BtActStatus.motion_no = 0x2D;
                BtGetTreasurebox_Sled++;
                ItemGetMes(TreasureboxBig_itemNo, -1, 0x28, 1);
            }

            break;
        }
        case 3: {
            CCameraFollow *camera = NowCamera__3;
            setCameraPassData(static_cast<CFrameVu1 *>(itemOpenBigFx.frame), camera, "cam", "int");

            if (GamePad.Down(PAD_CIRCLE | PAD_CROSS) != 0) {
                BtActStatus.shadow_visible = true;
                TexManager.DeleteTextureBlock(0x1C);
                TexManager.CleanUpTextureList();
                DngStatus()->GetItem(TreasureboxBig_itemNo, 0);
                NowCamera__3->FollowOn();
                sceVu0CopyVector(position, CharaMain.pos);
                position[2] += 10.0f;
                CharaMain.SetPosition(position);
                CharaMain.SetRotation(0.0f, -PI, 0.0f);
                BtActStatus.motion_no = 0;
                DngMessMan.enabled = true;
                UserStatus->step_disable = 0;
                SetMIniMapStatus(1);
                CMonUnitHold = 0;
                CMonUnitHyde = 0;
                CEffectHold = 0;
                CEffectHyde = 0;
                itemOpenBigFlag = 0;
                BtActStatus.in_presentation = 0;
                ClearSystemMes();
                NowCamera__3 = &MainCamera__4;
                done = 1;
            }

            break;
        }
        case 10:
            if (GamePad.Down(PAD_CIRCLE | PAD_CROSS) != 0) {
                ClearSystemMes();
                DngMessMan.enabled = true;
                UserStatus->step_disable = 0;
                BtActStatus.in_presentation = 0;
                NowDngMap->boxes[NowDngMap->events[iventActive].index].lid_angle = 0.0f;
                SetMIniMapStatus(1);
                CMonUnitHold = 0;
                CMonUnitHyde = 0;
                CEffectHold = 0;
                CEffectHyde = 0;
                done = 1;
            }

            break;
        default:
            break;
    }

    return done;
}

void BtGetTreasureboxSmall_Init(int dungeon) {
    int item_no = NowDngMap->boxes[NowDngMap->events[iventActive].index].item_no;

    ResetMovePower();
    int refusal = DngStatus()->CheckItemGet(item_no);

    if (refusal != 0) {
        SndSePlay(SE_CHEST_OPEN_SMALL, -1, 0);

        if (refusal == 1) {
            SetSystemMes(0x48, -1, MES_POS_CENTRE, 0, nullptr, nullptr);
        }

        if (refusal == 2) {
            SetSystemMes(0x51, -1, MES_POS_CENTRE, 0, nullptr, nullptr);
        }

        int index = NowDngMap->events[iventActive].index;
        NowDngMap->boxes[index].lid_angle = -20.0f;
        SetMIniMapStatus(0);
        iventInfo = DNG_EVENT_NONE;
        CMonUnitHold = 1;
        CMonUnitHyde = 1;
        CEffectHold = 1;
        CEffectHyde = 1;
        DngMessMan.enabled = false;

        UserStatus->step_disable = 1;
        BtActStatus.in_presentation = 1;
        BtActStatus.motion_no = 0;
        BtGetTreasurebox_Sled = 10;
        autoCamTrial();
        return;
    }

    int size = 0;

    NowDngMap->events[iventActive].kind = DNG_EVENT_NONE;

    if (ITEM_NAME_TBL_NEW[item_no - ITEM_ATTACH_START] == nullptr) {
        item_no = ITEM_ATTACH_PERIDOT;
    }

    int volume = createAttachVolume(item_no, dungeon);
    std::println("get attach vol = {}", volume);
    BtGetTreasureboxSmall_itemNo = item_no;
    BtGetTreasureboxSmall_itemVolume = volume;
    auto [model_path, texture_path] = ItemPaths(item_no);
    std::println("[{}]{}", item_no, model_path);
    std::println("[{}]{}", item_no, texture_path);
    ResetCashBuffer();
    StartReadBG();
    item_mds = NextCashBlock();
    LoadFileBG(model_path.data(), reinterpret_cast<u_long128 *>(item_mds), &size);
    CommitCashBlock(size);
    item_img = NextCashBlock();
    LoadFileBG(texture_path.data(), reinterpret_cast<u_long128 *>(item_img), &size);
    CommitCashBlock(size);
    u_char *chr = NextCashBlock();
    itemOpenItemChr = reinterpret_cast<u_int *>(chr);
    LoadFileBG(const_cast<char *>(chara_files[static_cast<u_char>(UserStatus->cur_chara)]), reinterpret_cast<u_long128 *>(chr), &size);
    CommitCashBlock(size);
    SndSPSeLoadBG(2, reinterpret_cast<u_int *>(NextCashBlock()), &size);
    CommitCashBlock(size);
    itemOpenSmall.frame->SetRotation(0.0f, 0.0f, 0.0f);
    DngMessMan.enabled = false;

    UserStatus->step_disable = 1;
    BtActStatus.in_presentation = 1;
    BtActStatus.motion_no = 0;
    BtGetTreasurebox_Sled = 0;
    autoCamTrial();
}

int BtGetTreasureboxSmall_Loop() {
    int done = 0;

    switch (BtGetTreasurebox_Sled) {
        case 0:
            if (ReadBGSync() == 0) {
                SndSPSeSyncBG();
                SetTempTexture(0x1C, reinterpret_cast<char *>(item_img));
                itemBoxModel = LoadMDSFile(reinterpret_cast<u_int *>(item_mds), &BtCashBuffer, 0, nullptr, nullptr);
                CharaMain.LoadPackData(itemOpenItemChr, "base2.cfg", &BtCashBuffer, &BtCashBuffer);
                BtActStatus.shadow_visible = false;
                BtGetTreasurebox_Sled++;
            }

            autoCamTrial();
            break;
        case 1: {
            sceVu0FVECTOR position;

            SetMIniMapStatus(0);
            iventInfo = DNG_EVENT_NONE;
            CMonUnitHold = 1;
            CMonUnitHyde = 1;
            CEffectHold = 1;
            CEffectHyde = 1;
            int index = NowDngMap->events[iventActive].index;
            sceVu0CopyVector(position, NowDngMap->boxes[index].pos);
            NowDngMap->boxes[index].lid_angle = -30.0f;
            NowDngMap->boxes[index].closed = 0;
            itemOpenSmall.frame->SetPosition(position);
            itemOpenSmallFx.frame->SetPosition(position);
            itemOpenSmallFx.frame->SetRotation(0.0f, 0.0f, 0.0f);
            position[1] += 13.0f;
            itemBoxModel->SetPosition(position);
            position[1] -= 13.0f;
            itemBoxModel->SetScale(0.01f, 0.01f, 0.01f);
            itemNormalScale = 0.1f;
            CharaMain.SetPosition(position);
            CharaMain.SetRotation(0.0f, 0.0f, 0.0f);
            BtActStatus.motion_no = 0x2A;
            itemOpenSmall.motion.state.time = 10.0f;
            itemOpenSmallFx.motion.state.time = 10.0f;
            itemOpenSmallFlag = 1;
            SubCamera = MainCamera__4;
            NowCamera__3 = &SubCamera;
            NowCamera__3->FollowOff();
            autoCamTrial();
            BtGetTreasurebox_Sled++;
            break;
        }
        case 2: {
            setCameraPassData(static_cast<CFrameVu1 *>(itemOpenSmallFx.frame), NowCamera__3, "cam", "int");
#ifdef PAL
            float pose_time = CharaMain.GetNowTime();

            if (!(pose_time < 79.0f) && pose_time <= 80.0f) {
                BtActStatus.motion_no = 0x2B;
            }

#endif
            float sound_time = itemOpenSmallFx.motion.state.time;

            if (!(sound_time <= 19.0f) && sound_time < 20.4f) {
                SndSePlay(SE_CHEST_OPEN_SMALL, -1, 0);
                SndSPSePlay(2, -1);
            }

            float grow_time = itemOpenSmallFx.motion.state.time;

            if (!(grow_time <= 50.0f) && grow_time <= 55.0f) {
                float scale = 0.2f * (grow_time - 50.0f);
                itemNormalScale = scale;
                itemBoxModel->SetScale(scale, scale, scale);
            }

            if (!(itemOpenSmallFx.motion.state.time < 79.0f)) {
                BtActStatus.motion_no = 0x2B;
                itemOpenSmallFlag = 2;

                if (BtGetTreasureboxSmall_itemVolume != 0) {
                    ItemGetMes(BtGetTreasureboxSmall_itemNo, BtGetTreasureboxSmall_itemVolume, 0x28, 1);
                } else {
                    ItemGetMes(BtGetTreasureboxSmall_itemNo, -1, 0x28, 1);
                }

                BtGetTreasurebox_Sled++;
            }

            break;
        }
        case 3:
            setCameraPassData(static_cast<CFrameVu1 *>(itemOpenSmallFx.frame), NowCamera__3, "cam", "int");

            if (GamePad.Down(PAD_CIRCLE | PAD_CROSS) != 0) {
                BtActStatus.shadow_visible = true;
                TexManager.DeleteTextureBlock(0x1C);
                TexManager.CleanUpTextureList();

                if (static_cast<unsigned int>(BtGetTreasureboxSmall_itemNo - ITEM_MAP) < 2U) {
                    if (BtGetTreasureboxSmall_itemNo == ITEM_MAP) {
                        BtEquipMap = 1;
                    }

                    if (BtGetTreasureboxSmall_itemNo == ITEM_MAGICAL_CRYSTAL) {
                        BtEquipMasuisyou = 1;
                    }
                } else {
                    DngStatus()->GetItem(BtGetTreasureboxSmall_itemNo, BtGetTreasureboxSmall_itemVolume);
                }

                sceVu0FVECTOR position;

                sceVu0CopyVector(position, CharaMain.pos);
                position[2] += 10.0f;
                CharaMain.SetPosition(position);
                CharaMain.SetRotation(0.0f, -PI, 0.0f);
                ClearSystemMes();
                BtActStatus.motion_no = 0;
                DngMessMan.enabled = true;
                UserStatus->step_disable = 0;
                BtActStatus.in_presentation = 0;
                SetMIniMapStatus(1);
                CMonUnitHold = 0;
                CMonUnitHyde = 0;
                CEffectHold = 0;
                CEffectHyde = 0;
                itemOpenSmallFlag = 0;
                NowCamera__3 = &MainCamera__4;
                done = 1;
            }

            break;
        case 10:
            if (GamePad.Down(PAD_CIRCLE | PAD_CROSS) != 0) {
                ClearSystemMes();
                DngMessMan.enabled = true;
                UserStatus->step_disable = 0;
                BtActStatus.in_presentation = 0;
                NowDngMap->boxes[NowDngMap->events[iventActive].index].lid_angle = 0.0f;
                SetMIniMapStatus(1);
                CMonUnitHold = 0;
                CMonUnitHyde = 0;
                CEffectHold = 0;
                CEffectHyde = 0;
                done = 1;
            }

            break;
        default:
            break;
    }

    return done;
}

int BtMiniItemSelect_Loop() {
    int done = 0;

    switch (BtMiniItemSelect_Sled) {
        case 0:
            BtMiniItemSelect_Sled++;
            frameCaputer = 1;
            break;

        case 1:
            BtMiniItemSelect_Sled++;
            break;

        case 2: {
            frameCaputer = 0;
            driveStepHold = 0;
            miniItemSelNo = -1;

            ITEM_PACK *pack = &UserStatus->item_pack;

            InitEventItemSelect(0x18, BtEventInfo.item_select_list, pack, 0xB4, 0xD2, BtEventInfo.item_select_filtered, 0);
            BtGameModeFlag = BT_GAME_MODE_ITEM_SELECT;
            BtMiniItemSelect_Sled++;
            break;
        }

        case 3:
            if (BtEventInfo.item_select_result != 0) {
                PortItemSelectResult->i = miniItemSelNo;
            }

            BtEventInfo.item_select_result = 0;
            SetMIniMapStatus(1);
            done = 1;
            DngMessMan.enabled = done;
            break;
        default:
            break;
    }

    return done;
}

void BtGetGateKey_Init(int item_no) {
    auto [model_path, texture_path] = ItemPaths(item_no);
    int size = 0;

    GateKey_itemNo = item_no;
    ResetCashBuffer();
    StartReadBG();
    item_mds = NextCashBlock();
    LoadFileBG(model_path.data(), reinterpret_cast<u_long128 *>(item_mds), &size);
    CommitCashBlock(size);
    item_img = NextCashBlock();
    LoadFileBG(texture_path.data(), reinterpret_cast<u_long128 *>(item_img), &size);
    CommitCashBlock(size);
    ResetMovePower();
    DngMessMan.enabled = false;

    UserStatus->step_disable = 1;
    BtActStatus.motion_no = 0;
    BtActStatus.in_presentation = 1;
    GateKey_Sled = 0;
    autoCamTrial();
}

int BtGetGateKey_Loop() {
    sceVu0FVECTOR eye;
    sceVu0FVECTOR ref;
    sceVu0FVECTOR ahead;
    int           done = 0;

    switch (GateKey_Sled) {
        case 0:
            if (ReadBGSync() == 0) {
                SetTempTexture(0x1C, reinterpret_cast<char *>(item_img));
                itemBoxModel = LoadMDSFile(reinterpret_cast<u_int *>(item_mds), &BtCashBuffer, 0, nullptr, nullptr);
                itemBoxModel->SetReference(CharaMain.frame->SearchFrame("item"));
                itemBoxModel->SetPosition(0.0f, 0.0f, 0.0f);
                itemBoxModel->SetRotation(0.0f, 0.0f, 0.0f);
                itemBoxModel->SetScale(1.0f, 1.0f, 1.0f);
                gateItemFlag = 1;
                GateKey_Sled++;
            }

            autoCamTrial();
            break;
        case 1:
            SetMIniMapStatus(0);
            iventInfo = DNG_EVENT_NONE;
            CMonUnitHold = 1;
            CMonUnitHyde = 1;
            CEffectHold = 1;
            CEffectHyde = 1;
            BtActStatus.motion_no = 0x22;
            SubCamera.FollowOff();
            sceVu0CopyVector(ref, CharaMain.pos);
            ref[1] += 12.0f;
            SubCamera.SetRef(ref);
            sceVu0CopyVector(eye, CharaMain.pos);
            getCharacterVector(ahead, 0.0f);
            sceVu0ScaleVectorXYZ(ahead, ahead, 40.0f);
            eye[0] += ahead[0];
            eye[1] += 20.0f + ahead[1];
            eye[2] += ahead[2];
            SubCamera.SetPos(eye);
            NowCamera__3 = &SubCamera;
            NowCamera__3->Step(-1);
            NowCamera__3->FollowOff();
            SndSePlay(SE_GATE_KEY_GET, -1, 0);
            autoCamTrial();
            GateKey_Sled++;
            break;
        case 2: {
            float end = CharaMain.motion_type.motion_info[34].end;
            float time = CharaMain.motion_type.state.time;

            if (!(time < end - 0.5f) && time <= end) {
                ItemGetMes(GateKey_itemNo, -1, 0x28, 1);
                BtActStatus.motion_no = 0x23;
                GateKey_Sled++;
            }

            break;
        }
        case 3:
            if (GamePad.Down(PAD_CIRCLE | PAD_CROSS) != 0) {
                TexManager.DeleteTextureBlock(0x1C);
                TexManager.CleanUpTextureList();
                DngMessMan.enabled = true;
                UserStatus->step_disable = 0;
                SetMIniMapStatus(1);
                CMonUnitHold = 0;
                CMonUnitHyde = 0;
                CEffectHold = 0;
                CEffectHyde = 0;
                gateItemFlag = 0;
                BtActStatus.in_presentation = 0;
                ClearSystemMes();
                NowCamera__3 = &MainCamera__4;
                done = 1;
            }

            break;
        default:
            break;
    }

    return done;
}

void BtEscape_Init() {
    int size = 0;

    ResetCashBuffer();
    StartReadBG();
    escape_file = NextCashBlock();
    LoadFileBG("dun/effect/escape.chr", reinterpret_cast<u_long128 *>(escape_file), &size);
    CommitCashBlock(size);
    SndSPSeLoadBG(8, reinterpret_cast<u_int *>(NextCashBlock()), &size);
    CommitCashBlock(size);
    DngMessMan.enabled = false;

    UserStatus->step_disable = 1;
    CMonUnitHold = 1;
    CMonUnitHyde = 1;
    CEffectHold = 1;
    CEffectHyde = 1;
    SetMIniMapStatus(0);
    iventInfo = DNG_EVENT_NONE;
    ResetMovePower();
    BtActStatus.motion_no = 0;
    BtActStatus.in_presentation = 1;
    escape_sled = 0;
    autoCamTrial();
}

int BtEscape_Loop() {
    sceVu0FVECTOR position;
    sceVu0FVECTOR rotation;
    int           done = 0;

    switch (escape_sled) {
        case 0:
            if (SndSPSeSyncBG() == 0 && ReadBGSync() == 0) {
                EscapeEffect.LoadPackData2(reinterpret_cast<u_int *>(escape_file), BtEffectInfoFile, &BtCashBuffer, 0x1C, &BtCashBuffer, 0);
                EscapeEffect.SetMotion(0, 6);
                EscapeEffect.motion_type.state.time = 10.0f;
                EscapeFlag = 1;
                sceVu0CopyVector(position, CharaMain.pos);
                CharaMain.GetRotation(rotation);
                EscapeEffect.SetPosition(position);
                EscapeEffect.SetRotation(rotation);
                EdFadeInit();
                EdFadeOut(0x78, 0.0f, 0.0f, 0.0f);
                SndSPSePlay(8, -1);
                escape_sled++;
            }

            autoCamTrial();
            break;
        case 1:
            if (EdFadeOutCheck() != 0) {
                done = 1;
            }

            autoCamTrial();
            break;
        default:
            break;
    }

    return done;
}
