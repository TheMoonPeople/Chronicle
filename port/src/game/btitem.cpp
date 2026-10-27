#include "common.h"

#include <libvu0.h>

#include <array>
#include <cstddef>
#include <format>
#include <print>
#include <string>
#include <utility>

#include "btactstatus.hpp"
#include "btitem.hpp"
#include "btmisc.hpp"
#include "camerafollow.hpp"
#include "character.hpp"
#include "clsmes.hpp"
#include "collision.hpp"
#include "dataalloc.hpp"
#include "dataread.hpp"
#include "dataset.hpp"
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

namespace dc::game {

namespace {

/**
 * Weapon each party member is handed when first brought into the party.
 */
constexpr std::array<int, 6> defWeapon = {ITEM_WEAPON_DAGGER_BROKEN, ITEM_WEAPON_WOODENSLINGSHOT_BROKEN, ITEM_WEAPON_MALLET_BROKEN, ITEM_WEAPON_GOLD_RING_BROKEN, ITEM_WEAPON_FIGHTING_STICK_BROKEN, ITEM_WEAPON_MACHINE_GUN_BROKEN};

/**
 * Character file of each party member, as selectChrUnit loads it.
 */
// clang-format off
constexpr std::array<const char *, 6> charaNameTbl = {
    "dun/mainchara/c01d.chr", "dun/mainchara/c04b.chr", "dun/mainchara/c06b.chr",
    "dun/mainchara/c05a.chr", "dun/mainchara/c10b.chr", "dun/mainchara/c18a.chr",
};
// clang-format on

char BtAtraShortEffectFile[] = "dun/effect/saget.chr";

/**
 * Empty name the model table gives the items that have no model of their own.
 */
constexpr char no_item_name[] = "";

} // namespace

int   BtGetTreasurebox_Sled;
int   BtGetAtraBoll_Sled;
int   TreasureboxBig_itemNo;
int   TreasureboxBig_itemType;
float TreasureboxBig_itemScale;
int   BtGetTreasureboxSmall_itemNo;
int   BtGetTreasureboxSmall_itemVolume;
int   BtAtraGetID;
int   BtAtraGetNo;
int   BtMiniChrSelecter_Sled;
int   BtMiniChrSel_Type;
int   BtMiniChrSelectNo;

void selectChrUnit(int chara_no, int reload) {
    std::array<char, 64> weapon0_name{};
    std::array<char, 64> weapon1_name{};
    std::array<char, 64> weapon2_name{};
    std::array<char, 64> unused_cfg_name{};
    int                  size = 0;

    std::println("{} -> {}", UserStatus->cur_chara, chara_no);

    if (chara_no == UserStatus->cur_chara && reload == 0) {
        return;
    }

    UserStatus->cur_chara = chara_no;
    LoadFile(const_cast<char *>(charaNameTbl[chara_no]), read_buffer, &size);
    wait_now_loading_vsync();
    size = static_cast<int>(static_cast<u_int>(((size >> 6) + 1) << 6) >> 2);
    u_int *weapon0 = &read_buffer[size];
    BtGetWeaponNamePath3(weapon0_name.data(), unused_cfg_name.data(), defWeapon[chara_no]);
    BtGetWeaponNamePath3(weapon1_name.data(), unused_cfg_name.data(), defWeapon[chara_no] + 1);
    BtGetWeaponNamePath3(weapon2_name.data(), unused_cfg_name.data(), UserStatus->chara_weapons[chara_no][UserStatus->equipped_weapon_slot[chara_no]].item_no);
    std::string path = std::format("commenu/weapon/{}", weapon0_name.data());
    LoadFile(path.data(), weapon0, &size);
    // Weapons 1 and 2 are both placed using weapon 0's size.
    u_int *weapon1 = &weapon0[static_cast<u_int>(((size >> 6) + 1) << 6) >> 2];
    path = std::format("commenu/weapon/{}", weapon1_name.data());
    LoadFile(path.data(), weapon1, nullptr);
    u_int *weapon2 = &weapon1[static_cast<u_int>(((size >> 6) + 1) << 6) >> 2];
    path = std::format("commenu/weapon/{}", weapon2_name.data());
    LoadFile(path.data(), weapon2, nullptr);
    wait_now_loading_vsync();
    LoadChara2(chara_no, 0, read_buffer, weapon0, weapon1, weapon2);
    SetWeaponAttachStatus(NowWeaponHave);
    BT_SHOT_EFFECT *effect = Get_Main_EffectPtr(chara_no, NowWeaponHave->best_elem);
    std::string     effect_path = std::format("dun/mainchara/wep_eff/{}.chr", static_cast<const char *>(effect->model_name));
    std::println("[{}] {}", chara_no, effect_path);
    LoadFile(effect_path.data(), read_buffer, &size);
    wait_now_loading_vsync();
    std::println("fsize = {}", size);
    MainChara_Effect(effect, read_buffer, 0);
    CWeaponFx.InitSet(NowWeapon->frame, "dcol0", "dcol1");
    SetWeaponColor();
    SndVoiceLoad(UserStatus->cur_chara);
    LoadActiveItemIcon();
    nowUnitNow = UserStatus->cur_chara;

    if (UserStatus->CheckLife() == 0) {
        UserStatus->hp[UserStatus->cur_chara] = 1;
    }
}

void LoadActiveItemIcon() {
    LoadFileMenuData("itemlst.img", read_buffer);
    wait_now_loading_vsync();
    SetTempTexture(0x28, reinterpret_cast<char *>(read_buffer));
    BtItemListCashFlag = 1;
}

// Treasure-box, gate-key and escape presentations and the item-select window. Each _Init starts
// background reads into BtCashBuffer and each _Loop consumes the files once the reads finish.

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

/**
 * Starts the short presentation for picking up an Atla.
 */
void BtAtraGetShort_Init() {
    int  size = 0;
    auto next_block = [] { return reinterpret_cast<u_int *>(&BtCashBuffer.base[static_cast<std::ptrdiff_t>(BtCashBuffer.used) * 16]); };

    atraShortGetType = 1;
    BtCashBuffer.base = reinterpret_cast<u_char *>(read_buffer);
    BtCashBuffer.limit = 0x445C0;
    BtCashBuffer.used = 0;
    StartReadBG();
    itemOpenItemChr = next_block();
    LoadFileBG("dun/mainchara/c01d_ex00.chr", reinterpret_cast<u_long128 *>(itemOpenItemChr), &size);
    BtCashBuffer.Alloc((((size >> 6) + 1) << 6) >> 4);
    shortAtraEffectPtr = next_block();
    LoadFileBG(BtAtraShortEffectFile, reinterpret_cast<u_long128 *>(shortAtraEffectPtr), &size);
    BtCashBuffer.Alloc((((size >> 6) + 1) << 6) >> 4);
    SndSPSeLoadBG(1, next_block(), &size);
    BtCashBuffer.Alloc((((size >> 6) + 1) << 6) >> 4);
    DngMessMan.enabled = false;
    ResetMovePower();
    UserStatus->step_disable = 1;
    BtAtraGetNo = iventActive;
    iventActive = -1;
    BtGetAtraBoll_Sled = 0;
    BtActStatus.in_presentation = 1;
}

/**
 * Runs the Atla pickup presentation and reports when it ends.
 */
int BtAtraGetShort_Loop(int map_no, int floor) {
    sceVu0FVECTOR position;
    int           done = -1;

    switch (BtGetAtraBoll_Sled) {
        case 0:
            if (SndSPSeSyncBG() == 0 && ReadBGSync() == 0) {
                CharaMain.LoadPackData(itemOpenItemChr, "base2.cfg", &BtCashBuffer, &BtCashBuffer);
                BtActStatus.shadow_visible = false;
                shortAtraEffect.LoadPackData2(shortAtraEffectPtr, "info.cfg", &BtCashBuffer, 0x1C, &BtCashBuffer, 0);
                SetMIniMapStatus(0);
                iventInfo = DNG_EVENT_NONE;
                atraGetStatus = 1;
                CMonUnitHold = 1;
                CMonUnitHyde = 1;
                CEffectHold = 1;
                CEffectHyde = 1;
                sceVu0CopyVector(atraGetPos, CharaMain.pos);
                CharaMain.GetRotation(atraGetRot);
                driveNoInterpolate = 1;
                int index = NowDngMap->events[BtAtraGetNo].index;
                shortAtraEffect.SetPosition(NowDngMap->atra[index].pos);
                shortAtraEffect.SetRotation(0.0f, 0.0f, 0.0f);
                shortAtraEffect.SetMotion(0, 6);
                atraGetStatusRate__2 = 0.0f;
                auto *status = static_cast<CDngStatusData *>(UserStatus);
                int   atra_id = status->atra_registry[map_no][NowDngMap->atra[index].atra_no].id;
                BtAtraGetID = atra_id;
                NowDngMap->atra[NowDngMap->events[BtAtraGetNo].index].used = 0;
                NowDngMap->events[BtAtraGetNo].kind = DNG_EVENT_NONE;
                getAtraToSaveData(atra_id, NowDngMap->atra[index].atra_no, SaveData, map_no, floor);
                sceVu0CopyVector(position, NowDngMap->atra[index].pos);
                CharaMain.SetPosition(position);
                CharaMain.SetRotation(0.0f, 0.0f, 0.0f);
                BtActStatus.motion_no = 0x2E;
                driveNoInterpolate = 1;
                SubCamera = MainCamera__4;
                NowCamera__3 = &SubCamera;
                NowCamera__3->FollowOff();
                shortAtraEffect.SetMotionCamera(NowCamera__3);
                BtGetAtraBoll_Sled++;
            }

            autoCamTrial();
            break;
        case 1: {
            float pose_time = CharaMain.GetNowTime();

            if (!(pose_time < 129.0f) && pose_time <= 130.0f) {
                BtActStatus.motion_no = 0x2F;
            }

            if (atraGetStatusRate__2 < 256.0f) {
                atraGetStatusRate__2 += 2.0f;
            }

            float sound_time = shortAtraEffect.motion_type.state.time;

            if (!(sound_time < 53.0f) && sound_time < 54.0f) {
                SndSPSePlay(1, -1);
            }

            if (!(shortAtraEffect.motion_type.state.time < 129.5f)) {
                AtraGetMes(map_no, BtAtraGetID, 0x28);
                atraGetStatus = 2;
                atraGetMsgBord = 1;
                atraGetMsgBordRate = 0.0f;
                BtActStatus.motion_no = 0x2F;
                BtGetAtraBoll_Sled++;
            }

            break;
        }
        case 2:
            if (atraGetMsgBordRate < 128.0f) {
                atraGetMsgBordRate += 4.0f;
            } else {
                BtGetAtraBoll_Sled++;
            }

            break;
        case 3:
            if (GamePad.Down(PAD_CIRCLE | PAD_CROSS) != 0) {
                DispFade__3.FadeInit(0.0f);
                DispFade__3.FadeOutStart(8.0f);
                BtGetAtraBoll_Sled++;
            }

            break;
        case 4:
            if (DispFade__3.GetRate() == 128.0f) {
                DispFade__3.FadeInStart(8.0f);
                autoCamTrial();
                BtGetAtraBoll_Sled++;
            }

            break;
        case 5:
            BtActStatus.shadow_visible = true;
            ClearSystemMes();
            SetMIniMapStatus(1);
            DngMessMan.enabled = true;
            atraGetStatus = 0;
            NowCamera__3 = &MainCamera__4;
            CharaMain.SetRotation(0.0f, -PI, 0.0f);
            CMonUnitHold = 0;
            CMonUnitHyde = 0;
            CEffectHold = 0;
            CEffectHyde = 0;
            atraGetMsgBord = 0;
            NowCamera__3->SetSpeed(6.0f);
            NowCamera__3->FollowOn();
            UserStatus->step_disable = 0;
            BtActStatus.motion_no = 0;
            BtActStatus.in_presentation = 0;
            done = 1;
            break;
        default:
            break;
    }

    return done;
}

void BtMiniChrSelect_Init(int type) {
    SetMIniMapStatus(0);
    DngMessMan.enabled = false;
    BtMiniChrSelecter_Sled = 0;
    BtMiniChrSel_Type = type;
}

/**
 * Runs the small character-select window and reports the choice.
 */
int BtMiniChrSelect_Loop() {
    static int frameWait;
    int        done = 0;

    switch (BtMiniChrSelecter_Sled) {
        case 0:
            BtMiniChrSelecter_Sled++;
            frameWait = 0;
            driveStepHold = 1;
            frameCaputer = 1;
            break;
        case 1:
            frameWait++;

            if (frameWait >= 2) {
                BtMiniChrSelecter_Sled++;
            }

            break;
        case 2: {
            frameCaputer = 0;
            driveStepHold = 0;
            std::array<int, 2> blocks = {0x128, 0xD8};
            StartQuickChange(reinterpret_cast<u_long128 *>(read_buffer), 0x17, blocks.data(), BtMiniChrSel_Type);
            BtGameModeFlag = BT_GAME_MODE_CHARA_CHANGE;
            BtMiniChrSelecter_Sled++;
            autoCamTrial();
            break;
        }
        case 3:
            SetMIniMapStatus(1);
            done = 1;
            DngMessMan.enabled = true;
            nowUnitNow = UserStatus->cur_chara;
            ResetStatusInfo();
            autoCamTrial();
            break;
        default:
            break;
    }

    return done;
}

int BtMiniItemSelect_Sled;
int GateKey_itemNo;
int GateKey_Sled;
int gateItemFlag;
int escape_sled;

RS_STACKDATA *PortItemSelectResult;

void BtMiniItemSelect() {
    SetMIniMapStatus(0);
    DngMessMan.enabled = false;
    BtMiniItemSelect_Sled = 0;
    driveStepHold = 1;
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

void BtGetAttach_Init(int dungeon, int item_no) {
    int volume = 0;

    if (item_no >= ITEM_ATTACH_START && item_no < ITEM_ATTACH_SLAYER_END) {
        volume = createAttachVolume(item_no, dungeon);
    }

    BtGetTreasureboxSmall_itemNo = item_no;
    BtGetTreasureboxSmall_itemVolume = volume;
    static_cast<CDngStatusData *>(UserStatus)->GetItem(item_no, volume);
    ClearSystemMes();
    ItemGetMes(BtGetTreasureboxSmall_itemNo, BtGetTreasureboxSmall_itemVolume, 0x78, 0);
}

int BtGetAttach_Loop() {
    int done = 0;

    switch (GateKey_Sled) {
        case 0:
            SetMIniMapStatus(0);
            iventInfo = DNG_EVENT_NONE;
            CMonUnitHold = 1;
            CEffectHold = 1;
            ItemGetMes(BtGetTreasureboxSmall_itemNo, BtGetTreasureboxSmall_itemVolume, 0x28, 1);
            GateKey_Sled++;
            break;

        case 1:
            if (GamePad.Down(PAD_CIRCLE | PAD_CROSS) != 0) {
                DngMessMan.enabled = true;
                UserStatus->step_disable = 0;
                SetMIniMapStatus(1);
                CMonUnitHold = 0;
                CEffectHold = 0;
                gateItemFlag = 0;
                BtActStatus.in_presentation = 0;
                ClearSystemMes();
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
                EscapeEffect.LoadPackData2(reinterpret_cast<u_int *>(escape_file), "info.cfg", &BtCashBuffer, 0x1C, &BtCashBuffer, 0);
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

/**
 * Builds the models of the items in the active slots.
 */
void BtSetActiveItemModel(u_int *buffer) {
    const ITEM_PACK &pack = UserStatus->item_pack;

    for (int slot = 0; slot < 3; slot++) {
        int item_no = pack.quick_item_slot[slot];

        if (item_no == -1) {
            continue;
        }

        std::array<char, 64> model_path{};
        std::array<char, 64> texture_path{};
        int                  model_size = 0;
        int                  texture_size = 0;
        BtGetItemNamePath(model_path.data(), texture_path.data(), item_no);
        LoadFile(model_path.data(), buffer, &model_size);
        wait_now_loading_vsync();
        model_size = static_cast<int>(static_cast<u_int>(((model_size >> 6) + 1) << 6) >> 2);
        u_int *texture = &buffer[model_size];
        LoadFile(texture_path.data(), texture, &texture_size);
        wait_now_loading_vsync();

        if (activeItem.model[slot + 1] != -1) {
            activeItem.models->DeleteModel(activeItem.model[slot + 1]);
            activeItem.model[slot + 1] = -1;
        }

        activeItem.model[slot + 1] = activeItem.models->SetCashModel(item_no, buffer, texture, texture_size);
        activeItem.model[slot + 5] = 0;
        activeItem.item[slot + 1] = item_no;
    }
}

void ParabolicInitialVector(float *velocity, float *from, float *to, float gravity, float time) {
    velocity[0] = (to[0] - from[0]) / time;
    velocity[1] = (2.0f * (to[1] - from[1]) - time * (gravity * time)) / (2.0f * time);
    velocity[2] = (to[2] - from[2]) / time;
    velocity[3] = 1.0f;
    velocity[1] *= -1.0f;
}

void setShotVector(float *velocity, float speed, float angle_y, float angle_x) {
    sceVu0FMATRIX rotation;
    sceVu0FMATRIX identity;

    velocity[0] = 0.0f;
    velocity[1] = 0.0f;
    velocity[2] = speed;
    velocity[3] = 1.0f;
    sceVu0UnitMatrix(identity);
    sceVu0RotMatrixX(rotation, identity, angle_x);
    sceVu0RotMatrixY(rotation, rotation, angle_y);
    sceVu0ApplyMatrix(velocity, rotation, velocity);
}

const char *ITEM_NAME_TBL_NEW[175] = {
    "atfire",
    "atice",
    "atthunde",
    "atwind",
    "atholy",
    no_item_name,
    no_item_name,
    no_item_name,
    no_item_name,
    no_item_name,
    "atpower",
    "atdamage",
    "atspeed",
    "atmagic",
    "juelgnet",
    "juelamst",
    "juelaqua",
    "jueldaia",
    "juelemrd",
    "juelparl",
    "juelruby",
    "juelprdt",
    "juelsphr",
    "juelopal",
    "jueltpaz",
    "jueltrqu",
    "taiyou",
    no_item_name,
    no_item_name,
    no_item_name,
    "zatdino",
    "zatunded",
    "zatsea",
    "zatstorn",
    "zatplant",
    "zatbeast",
    "zatsky",
    "zatmetal",
    "zatmimic",
    "zatmaji",
    no_item_name,
    no_item_name,
    no_item_name,
    no_item_name,
    no_item_name,
    no_item_name,
    no_item_name,
    no_item_name,
    no_item_name,
    no_item_name,
    no_item_name,
    "mayokest",
    "mayokenr",
    "mayokenb",
    "mayokedk",
    "dounut",
    "skanacnd",
    "kusamoti",
    "majopafe",
    "sasorijk",
    "ninjncki",
    no_item_name,
    no_item_name,
    no_item_name,
    "mizufutu",
    "mizuoisi",
    "mizugoku",
    "pan",
    "chicken",
    "bin2drnk",
    "dokukesi",
    "seisui",
    "sekken",
    "mityheal",
    "cheese",
    no_item_name,
    no_item_name,
    no_item_name,
    "bakudan",
    "ishi",
    "masekifi",
    "masekiic",
    "masekitd",
    "masekiwd",
    "masekiho",
    "isiranbo",
    "nebapeac",
    "bomnuts",
    "dokuring",
    "banana",
    "konamedu",
    "konakata",
    "konawarp",
    "konakawa",
    "konaexit",
    "konafuka",
    "konarepe",
    "konalvup",
    "pocket",
    "edenfurt",
    "takaraky",
    "hyoutan",
    "konarepe_at",
    no_item_name,
    "turisao",
    "ninzin",
    "imodango",
    "minon",
    "battan",
    "puti",
    "savesyo",
    no_item_name,
    "ebi",
    no_item_name,
    "gkeydran",
    "hikaruis",
    "gkeymimi",
    "akaikimi",
    "togecchi",
    "candy",
    "hook",
    "oukenost",
    "kayaku",
    "tokeisin",
    "tongrmrn",
    "gkey_ds",
    "keytuno",
    "mikazuki",
    "orgelnej",
    "taiysirs",
    "tukisirs",
    "ticket",
    no_item_name,
    no_item_name,
    no_item_name,
    "keyhone",
    "keyhige",
    "keysenst",
    "keyishi",
    "handol",
    "keysikok",
    "fkey_ds",
    no_item_name,
    "torokoil",
    "taiysizk",
    "pitisakn",
    "kusasakn",
    "keyhitug",
    "yuukiita",
    "patapata",
    "key_eye",
    no_item_name,
    "map",
    "masuisyo",
    "dorahane",
    "keydokut",
    "hengemiz",
    "worldmap",
    "honepend",
    "tukifue",
    "mahoranp",
    "tukinoob",
    "kairing",
    "sosareij",
    "icebig",
    "icemid",
    "icesml",
    "keyhonoo",
    "hantmimi",
    "kouyaku",
    "fundtion",
    "haniwa",
    "manual",
    "pezutama",
    no_item_name,
};

void getCharacterVector(float *vector, float pitch) {
    sceVu0FVECTOR forward = {0.0f, 0.0f, 1.0f, 1.0f};
    sceVu0FVECTOR rotation;
    sceVu0FMATRIX matrix;
    sceVu0FMATRIX yaw;
    sceVu0FMATRIX tilt;

    CharaMain.frame->GetRotation(rotation);
    sceVu0UnitMatrix(yaw);
    sceVu0RotMatrixY(yaw, yaw, rotation[1]);
    sceVu0UnitMatrix(tilt);
    sceVu0RotMatrixX(tilt, tilt, pitch);
    sceVu0MulMatrix(matrix, yaw, tilt);
    sceVu0ApplyMatrix(vector, matrix, forward);
}

/**
 * Advances a thrown item along its arc.
 */
int ItemThrowStep(float *position, float *velocity) {
    sceVu0FVECTOR next_position;
    sceVu0FVECTOR direction;
    sceVu0FVECTOR hit_point;
    sceVu0FVECTOR normal;

    next_position[0] = position[0] + velocity[0];
    next_position[1] = position[1] + velocity[1];
    next_position[2] = position[2] + velocity[2];
    next_position[3] = 1.0f;
    velocity[1] -= 0.12f;
    sceVu0Normalize(direction, velocity);

    for (int unit_no = 0; unit_no < 16; unit_no++) {
        for (int i = 0; i < 16; i++) {
            if (NowMonstorUnit->effect[unit_no].timer[i] != 0 && DistVector(NowMonstorUnit->effect[unit_no].position[i], position) <= 1.5f + NowMonstorUnit->effect[unit_no].radius[i]) {
                return 2;
            }
        }
    }

    WorkBuffer->used = 0;
    auto *polys = reinterpret_cast<CCPoly *>(WorkBuffer->Alloc(2000));
    int   hit_poly = CheckHit(polys, setCollisionData(NowDngMap, polys, position, 20.0f, 1.5f), position, next_position, hit_point, 1, 4);

    if (hit_poly >= 0) {
        sceVu0CopyVector(normal, polys[hit_poly].normal);
        sceVu0Normalize(normal, normal);
        ReflectionPlane(normal, hit_point, position, velocity);
        sceVu0CopyVector(position, hit_point);
        position[0] += 1.3f * normal[0];
        position[1] += 1.3f * normal[1];
        position[2] += 1.3f * normal[2];
        velocity[0] *= 0.4f;
        velocity[1] *= 0.4f;
        velocity[2] *= 0.4f;
        return 1;
    }

    sceVu0CopyVector(position, next_position);
    return 0;
}

} // namespace dc::game
