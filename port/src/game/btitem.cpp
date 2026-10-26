#include "common.h"

#include <libvu0.h>

#include <array>
#include <cstddef>
#include <format>
#include <print>
#include <string>

#include "btactstatus.hpp"
#include "btitem.hpp"
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
    LoadFileBG(BtAtraShortCharaFile, reinterpret_cast<u_long128 *>(itemOpenItemChr), &size);
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

void BtMiniItemSelect() {
    SetMIniMapStatus(0);
    DngMessMan.enabled = false;
    BtMiniItemSelect_Sled = 0;
    driveStepHold = 1;
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

const char *const ITEM_NAME_TBL_NEW[] = {
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

    WorkBuffer__2->used = 0;
    auto *polys = reinterpret_cast<CCPoly *>(WorkBuffer__2->Alloc(2000));
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
