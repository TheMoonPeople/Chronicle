#include "common.h"

#include <libvu0.h>

#include "btactstatus.hpp"
#include "btitem.hpp"
#include "btmisc.hpp"
#include "character.hpp"
#include "clsmes.hpp"
#include "dataalloc.hpp"
#include "dataread.hpp"
#include "dngmessageman.hpp"
#include "dngstatusdata.hpp"
#include "dun/gameloop.hpp"
#include "frame.hpp"
#include "framevu1.hpp"
#include "gamepad.hpp"
#include "menu_draw.hpp"
#include "menu_dungeon.hpp"
#include "menu_save.hpp"
#include "nowload.hpp"
#include "snd.hpp"
#include "sysmes.hpp"
#include "userstatus.hpp"

/* Battle item handling: treasure boxes, pickups and thrown items. */

#include <cstdio>
#include <cstring>

#include "btmisc.hpp"
#include "camerafollow.hpp"
#include "collision.hpp"
#include "dispctrl.hpp"
#include "dungeonmap.hpp"
#include "dungeonparts.hpp"
#include "edit.hpp"
#include "editloop3.hpp"
#include "mainitemmodel.hpp"
#include "mathutil.hpp"
#include "mds.hpp"
#include "monstorunit.hpp"
#include "motionmodel.hpp"
#include "savedata.hpp"
#include "texture.hpp"
#include "vector.hpp"
#include "weaponeffect.hpp"

/** Weapon each party member is handed when first brought into the party. */
static int defWeapon[6] = {ITEM_WEAPON_DAGGER_BROKEN, ITEM_WEAPON_WOODENSLINGSHOT_BROKEN, ITEM_WEAPON_MALLET_BROKEN, ITEM_WEAPON_GOLD_RING_BROKEN, ITEM_WEAPON_FIGHTING_STICK_BROKEN, ITEM_WEAPON_MACHINE_GUN_BROKEN};

// clang-format off
char *charaNameTbl[6] = {
    "dun/mainchara/c01d.chr", "dun/mainchara/c04b.chr", "dun/mainchara/c06b.chr",
    "dun/mainchara/c05a.chr", "dun/mainchara/c10b.chr", "dun/mainchara/c18a.chr",
};
// clang-format on

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

/**
 * Computes the quantity represented by an acquired attachment.
 */
int createAttachVolume(int item_no, int dungeon);

void selectChrUnit(int chara_no, int reload) {
    char weapon0_name[64];
    char weapon1_name[64];
    char weapon2_name[64];
    char path[64];
    char effect_path[64];
    int  size;

    printf("%d -> %d\n", UserStatus->cur_chara, chara_no);

    if (chara_no == UserStatus->cur_chara && reload == 0) {
        return;
    }

    UserStatus->cur_chara = chara_no;
    LoadFile(charaNameTbl[chara_no], read_buffer, &size);
    wait_now_loading_vsync();
    size = (u_int) (((size >> 6) + 1) << 6) >> 2;
    u_int *weapon0 = &read_buffer[size];
    int    default_weapon = defWeapon[chara_no];
    BtGetWeaponNamePath3(weapon0_name, path, default_weapon);
    BtGetWeaponNamePath3(weapon1_name, path, defWeapon[chara_no] + 1);
    BtGetWeaponNamePath3(weapon2_name, path, UserStatus->chara_weapons[chara_no][UserStatus->equipped_weapon_slot[chara_no]].item_no);
    sprintf(path, "commenu/weapon/%s", weapon0_name);
    LoadFile(path, weapon0, &size);
    u_int *weapon1 = &weapon0[(u_int) (((size >> 6) + 1) << 6) >> 2];
    sprintf(path, "commenu/weapon/%s", weapon1_name);
    LoadFile(path, weapon1, NULL);
    u_int *weapon2 = &weapon1[(u_int) (((size >> 6) + 1) << 6) >> 2];
    sprintf(path, "commenu/weapon/%s", weapon2_name);
    LoadFile(path, weapon2, NULL);
    wait_now_loading_vsync();
    LoadChara2(chara_no, 0, read_buffer, weapon0, weapon1, weapon2);
    SetWeaponAttachStatus(NowWeaponHave);
    BT_SHOT_EFFECT *effect = Get_Main_EffectPtr(chara_no, NowWeaponHave->best_elem);
    sprintf(effect_path, "dun/mainchara/wep_eff/%s.chr", (char *) effect);
    printf("[%d] %s\n", chara_no, effect_path);
    LoadFile(effect_path, read_buffer, &size);
    wait_now_loading_vsync();
    printf("fsize = %d\n", size);
    MainChara_Effect(effect, read_buffer, 0);
    CWeaponFx.InitSet(NowWeapon->frame, "dcol0", "dcol1");
    SetWeaponColor();
    SndVoiceLoad(UserStatus->cur_chara);
    unsigned int *buffer = read_buffer;
    LoadFileMenuData("itemlst.img", buffer);
    wait_now_loading_vsync();
    SetTempTexture(0x28, (char *) read_buffer);
    BtItemListCashFlag = 1;
    nowUnitNow = UserStatus->cur_chara;

    if (UserStatus->CheckLife() == 0) {
        CUserStatus *user = UserStatus;
        int          cur_chara = user->cur_chara;
        user->hp[cur_chara] = 1;
    }
}

/**
 * Loads the image used by the active item icons.
 *
 * @mangled LoadActiveItemIcon__Fv
 * @address 0x1D13A0
 * @size 0x4C
 */
void LoadActiveItemIcon() {
    unsigned int *buffer = read_buffer;
    LoadFileMenuData("itemlst.img", buffer);
    wait_now_loading_vsync();
    SetTempTexture(0x28, (char *) read_buffer);
    BtItemListCashFlag = 1;
}

/**
 * Opens the large treasure chest and starts its presentation.
 *
 * @mangled BtGetTreasureboxBig_Init__Fv
 * @address 0x1D13F0
 * @size 0x418
 */
void BtGetTreasureboxBig_Init();

/**
 * Runs the large treasure chest's presentation and reports when it ends.
 *
 * @mangled BtGetTreasureboxBig_Loop__Fv
 * @address 0x1D1810
 * @size 0x7A8
 */
int BtGetTreasureboxBig_Loop();

/**
 * Opens the small treasure chest and starts its presentation.
 *
 * @mangled BtGetTreasureboxSmall_Init__Fi
 * @address 0x1D1FC0
 * @size 0x4A0
 */
void BtGetTreasureboxSmall_Init(int dungeon);

/**
 * Runs the small treasure chest's presentation and reports when it ends.
 *
 * @mangled BtGetTreasureboxSmall_Loop__Fv
 * @address 0x1D2460
 * @size 0x690
 */
int BtGetTreasureboxSmall_Loop();

/**
 * Starts the short presentation for picking up an Atla.
 *
 * @mangled BtAtraGetShort_Init__Fv
 * @address 0x1D2AF0
 * @size 0x180
 */
void BtAtraGetShort_Init() {
    int size;

    atraShortGetType = 1;
    BtCashBuffer.base = (u_char *) read_buffer;
    BtCashBuffer.limit = 0x445C0;
    BtCashBuffer.used = 0;
    StartReadBG();
    LoadFileBG(BtAtraShortCharaFile, (u_long128 *) (itemOpenItemChr = (u_int *) (BtCashBuffer.base + BtCashBuffer.used * 16)), &size);
    BtCashBuffer.Alloc((((size >> 6) + 1) << 6) >> 4);
    LoadFileBG(BtAtraShortEffectFile, (u_long128 *) (shortAtraEffectPtr = (u_int *) (BtCashBuffer.base + BtCashBuffer.used * 16)), &size);
    BtCashBuffer.Alloc((((size >> 6) + 1) << 6) >> 4);
    SndSPSeLoadBG(1, (u_int *) (BtCashBuffer.base + BtCashBuffer.used * 16), &size);
    BtCashBuffer.Alloc((((size >> 6) + 1) << 6) >> 4);
    DngMessMan.enabled = false;
    ResetMovePower();
    CUserStatus *status = UserStatus;
    int          one = 1;
    status->step_disable = one;
    BtAtraGetNo = iventActive;
    iventActive = -1;
    BtGetAtraBoll_Sled = 0;
    BtActStatus.in_presentation = one;
}

char BtAtraShortEffectFile[] __attribute__((section(".rodata"))) = "dun/effect/saget.chr";

/**
 * Runs the Atla pickup presentation and reports when it ends.
 *
 * @mangled BtAtraGetShort_Loop__Fii
 * @address 0x1D2C70
 * @size 0x61C
 */
int BtAtraGetShort_Loop(int map_no, int floor) {
    sceVu0FVECTOR position;
    int           done = -1;

    switch (BtGetAtraBoll_Sled) {
        case 0:
            if (SndSPSeSyncBG() == 0 && ReadBGSync() == 0) {
                CharaMain.LoadPackData((u_int *) itemOpenItemChr, "base2.cfg", &BtCashBuffer, &BtCashBuffer);
                BtActStatus.shadow_visible = false;
                shortAtraEffect.LoadPackData2((u_int *) shortAtraEffectPtr, "info.cfg", &BtCashBuffer, 0x1C, &BtCashBuffer, 0);
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
                CDngStatusData *status = (CDngStatusData *) UserStatus;
                int             atra_id = status->atra_registry[map_no][NowDngMap->atra[index].atra_no].id;
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
    }

    return done;
}

/**
 * Opens the small character-select window in the given selection mode.
 *
 * @mangled BtMiniChrSelect_Init__Fi
 * @address 0x1D3290
 * @size 0x40
 */
void BtMiniChrSelect_Init(int type) {
    SetMIniMapStatus(0);
    DngMessMan.enabled = false;
    BtMiniChrSelecter_Sled = 0;
    BtMiniChrSel_Type = type;
}

/**
 * Runs the small character-select window and reports the choice.
 *
 * @mangled BtMiniChrSelect_Loop__Fv
 * @address 0x1D32D0
 * @size 0x128
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
            int blocks[2] = {0x128, 0xD8};
            StartQuickChange((u_long128 *) read_buffer, 0x17, blocks, BtMiniChrSel_Type);
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
    }

    return done;
}

int BtMiniItemSelect_Sled;
int GateKey_itemNo;
int GateKey_Sled;
int gateItemFlag;
int escape_sled;

/**
 * Opens the small item-select window.
 *
 * @mangled BtMiniItemSelect__Fv
 * @address 0x1D3400
 * @size 0x38
 */
void BtMiniItemSelect() {
    SetMIniMapStatus(0);
    DngMessMan.enabled = false;
    BtMiniItemSelect_Sled = 0;
    driveStepHold = 1;
}

/**
 * Runs one frame of the small item-select window, and reports when it closes.
 *
 * @mangled BtMiniItemSelect_Loop__Fv
 * @address 0x1D3440
 * @size 0x118
 */
int BtMiniItemSelect_Loop();

/**
 * Starts the gate-key pickup presentation for the given item.
 *
 * @mangled BtGetGateKey_Init__Fi
 * @address 0x1D3560
 * @size 0x13C
 */
void BtGetGateKey_Init(int item_no);

/**
 * Runs the gate-key presentation and reports when it ends.
 *
 * @mangled BtGetGateKey_Loop__Fv
 * @address 0x1D36A0
 * @size 0x3D0
 */
int BtGetGateKey_Loop();

void BtGetAttach_Init(int dungeon, int item_no) {
    int volume = 0;

    if (item_no >= ITEM_ATTACH_START && item_no < ITEM_ATTACH_SLAYER_END) {
        volume = createAttachVolume(item_no, dungeon);
    }

    BtGetTreasureboxSmall_itemNo = item_no;
    BtGetTreasureboxSmall_itemVolume = volume;
    ((CDngStatusData *) UserStatus)->GetItem(item_no, volume);
    ClearSystemMes();
    ItemGetMes(BtGetTreasureboxSmall_itemNo, BtGetTreasureboxSmall_itemVolume, 0x78, 0);
}

/**
 * Runs one frame of the attachment pickup message, and reports when it ends.
 *
 * @mangled BtGetAttach_Loop__Fv
 * @address 0x1D3B00
 * @size 0xF0
 */
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
    }

    return done;
}

/**
 * Starts the presentation that carries the party off the floor.
 *
 * @mangled BtEscape_Init__Fv
 * @address 0x1D3BF0
 * @size 0x14C
 */
void BtEscape_Init();

/**
 * Runs the escape presentation and reports when it ends.
 *
 * @mangled BtEscape_Loop__Fv
 * @address 0x1D3D40
 * @size 0x18C
 */
int BtEscape_Loop();

/**
 * Builds the models of the items in the active slots.
 *
 * @mangled BtSetActiveItemModel__FPUi
 * @address 0x1D3ED0
 * @size 0x1B0
 */
void BtSetActiveItemModel(u_int *buffer) {
    u_int     *texture;
    int        i;
    int        size;
    ITEM_PACK *pack;
    int        item_no;
    char       model_path[64];
    char       texture_path[64];
    int        model_size;
    int        texture_size;
    pack = &UserStatus->item_pack;

    for (i = 0; i < 3; i++) {
        item_no = pack->quick_item_slot[i];

        if (item_no == -1) {
            continue;
        }

        BtGetItemNamePath(model_path, texture_path, item_no);
        LoadFile(model_path, buffer, &model_size);
        wait_now_loading_vsync();
        model_size = (u_int) (((model_size >> 6) + 1) << 6) >> 2;
        LoadFile(texture_path, &buffer[model_size], &texture_size);
        wait_now_loading_vsync();

        if (activeItem.model[i + 1] != -1) {
            activeItem.models->DeleteModel(activeItem.model[i + 1]);
            activeItem.model[i + 1] = -1;
        }

        size = texture_size;
        texture = &buffer[model_size];

        if (activeItem.model[i + 1] != -1) {
            activeItem.models->DeleteModel(activeItem.model[i + 1]);
        }

        activeItem.model[i + 1] = activeItem.models->SetCashModel(item_no, buffer, texture, size);
        activeItem.model[i + 5] = 0;
        activeItem.item[i + 1] = item_no;
    }
}

/**
 * Computes the velocity needed to move an object between two points in time.
 *
 * @mangled ParabolicInitialVector__FPfPfPfff
 * @address 0x1D4080
 * @size 0x7C
 */
void ParabolicInitialVector(float *velocity, float *from, float *to, float gravity, float time) {
    velocity[0] = (to[0] - from[0]) / time;
    velocity[1] = (2.0f * (to[1] - from[1]) - time * (gravity * time)) / (2.0f * time);
    velocity[2] = (to[2] - from[2]) / time;
    velocity[3] = 1.0f;
    float negative_one = -1.0f;
    velocity[1] = velocity[1] * negative_one;
}

/**
 * Builds the velocity of a shot fired at the given speed and angles.
 *
 * @mangled setShotVector__FPffff
 * @address 0x1D4100
 * @size 0x98
 */
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

char no_item_name[] __attribute__((section(".rodata"))) = "";

char *ITEM_NAME_TBL_NEW[] = {
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
 *
 * @mangled ItemThrowStep__FPfPf
 * @address 0x1D4260
 * @size 0x2D4
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

    WorkBuffer__2->used = 0;
    CCPoly *polys = (CCPoly *) WorkBuffer__2->Alloc(2000);
    int     hit_poly = CheckHit(polys, setCollisionData(NowDngMap, polys, position, 20.0f, 1.5f), position, next_position, hit_point, 1, 4);

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
