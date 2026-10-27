#include "dun/gameloop.hpp"

#include <libvu0.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <format>
#include <iterator>
#include <memory>
#include <print>
#include <string>
#include <string_view>

#include "battlemenu.hpp"
#include "btactstatus.hpp"
#include "btitem.hpp"
#include "btmisc.hpp"
#include "btsysscript.hpp"
#include "camera.hpp"
#include "camerafollow.hpp"
#include "character.hpp"
#include "clothread.hpp"
#include "clsmes.hpp"
#include "collision.hpp"
#include "collisiondata.hpp"
#include "dataalloc.hpp"
#include "dataread.hpp"
#include "dataset.hpp"
#include "debugfont.hpp"
#include "dispctrl.hpp"
#include "dngmessageman.hpp"
#include "dngstatusdata.hpp"
#include "dranmapfield.hpp"
#include "dungeonparts.hpp"
#include "dungeoneventman.hpp"
#include "dungeonmap.hpp"
#include "edit.hpp"
#include "editloop3.hpp"
#include "frame.hpp"
#include "frameattr.hpp"
#include "framevu1.hpp"
#include "gamemode.hpp"
#include "gamepad.hpp"
#include "gameutil.hpp"
#include "gfx/gfx.hpp"
#include "healeffect.hpp"
#include "hit_machingun_effect.hpp"
#include "hitmark.hpp"
#include "hitvalue.hpp"
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

namespace dc::game {

using namespace dc::input;
using namespace dc::platform;

namespace {

/* The weapon each character starts with. */
constexpr std::array<s32, 6> defWeapon__6 = {ITEM_WEAPON_DAGGER_BROKEN, ITEM_WEAPON_WOODENSLINGSHOT_BROKEN, ITEM_WEAPON_MALLET_BROKEN, ITEM_WEAPON_GOLD_RING_BROKEN, ITEM_WEAPON_FIGHTING_STICK_BROKEN, ITEM_WEAPON_MACHINE_GUN_BROKEN};

/**
 * How far one character reaches when the game picks what to lock on to.
 */
float CharaRangeRate(CUserStatus *status) {
    constexpr std::array<float, 6> range_rate = {1.2f, 1.4f, 1.1f, 1.5f, 1.0f, 1.8f};

    return range_rate[status->cur_chara];
}

/**
 * How tall one character stands, and how wide their collision reaches.
 */
float CharaHeight(CUserStatus *status) {
    constexpr std::array<float, 6> chara_height = {16.0f, 14.0f, 16.0f, 16.0f, 18.0f, 15.0f};

    return chara_height[status->cur_chara];
}

/**
 * First free byte of an arena, past the quadwords it has handed out.
 */
u_char *ArenaFree(CDataAlloc2<1> &arena) {
    return arena.base + (static_cast<std::ptrdiff_t>(arena.used) * 16);
}

/**
 * Puts every placed event of a floor back to idle.
 */
void ClearEventData(CDungeonEventMan &man) {
    for (CDungeonEventData &state : man.event) {
        state.event = nullptr;
        state.switch_on = false;
        state.enabled = false;
        state.hold = 0;
        state.chara_done = -1;
    }
}

/**
 * Empties a floor of its events, treasure boxes, Atla and room links before it is rebuilt.
 */
void ClearMapObjects(CDungeonMap &map) {
    for (DUNGEON_EVENT &event : map.events) {
        event.kind = DNG_EVENT_NONE;
        event.reset_flag = 0;
    }

    for (TREASURE_BOX &box : map.boxes) {
        box.used = 0;
        box.lid_angle = 0.0f;
        box.trap_no = 0;
    }

    map.box_num = 0;

    for (ATRA_BOLL &atra : map.atra) {
        atra.used = 0;
    }

    map.atra_num = 0;

    for (ROOM_LINK_RESULT &link : map.room_link) {
        link.used = 0;
    }
}

/**
 * Frees every item slot of a floor and sets the texture its gold draws with.
 */
void ClearRandomItems(CRandomItem &items, CTexture *gold) {
    for (int slot = 0; slot < 32; slot++) {
        items.id[slot] = -1;
        items.pickup_event[slot] = -1;
        items.pickup_blocked[slot] = 0;
    }

    items.gold_texture = gold;
}

/**
 * Gives the weapon trail the grey it draws with when the weapon has no element.
 */
void SetWeaponTrailDefaultColour() {
    CWeaponFx.colour[0].r = CWeaponFx.colour[2].r = 0x40;
    CWeaponFx.colour[0].g = CWeaponFx.colour[2].g = 0x40;
    CWeaponFx.colour[0].b = CWeaponFx.colour[2].b = 0x40;
    CWeaponFx.colour[1].r = CWeaponFx.colour[3].r = 6;
    CWeaponFx.colour[1].g = CWeaponFx.colour[3].g = 6;
    CWeaponFx.colour[1].b = CWeaponFx.colour[3].b = 6;
}

/**
 * Empties the weapon trail and puts its colour back to the default.
 */
void ClearWeaponTrail() {
    for (auto &used : CWeaponFx.used) {
        used = 0;
    }

    SetWeaponTrailDefaultColour();
    CWeaponFx.cursor = 0;
    CWeaponFx.timer = 0;
}

void LoadData();

} // namespace

/* The battle status of every character in the party. */
BT_ACT_STATUS BtActStatus;

/* The cameras the dungeon draws through. */
CCameraFollow MainCamera__4(20.0f, 15.0f, 0.0f, 8.0f);
CCameraFollow SubCamera(60.0f, 20.0f, 0.0f, 4.0f);

/* The fade the dungeon draws over the picture. */
CDispCtrl DispFade__3;

/* The dungeon floor that the player is on. */
// init_priority keeps this static's place in the order rand() is drawn from; see app/runtime.cpp.
CDungeonMap MainDungeonMap __attribute__((init_priority(65011)));

/* The back dungeon's floor. */
// init_priority keeps this static's place in the order rand() is drawn from; see app/runtime.cpp.
CDungeonMap UraDungeonMap __attribute__((init_priority(65012)));

/* The Dran fields of the floor that the player is on. */
DRAN_MAP_FIELD_SET DranMapField;

/* The events of the floor that the player is on. */
CDungeonEventMan DngEventMan;

/* The events of the back dungeon's floor. */
CDungeonEventMan UraEventMan;

/* The lighting the dungeon draws the field and the models under. */
sceVu0FMATRIX main_light;
sceVu0FMATRIX main_lightcolor;
sceVu0FVECTOR main_ambientlight;
sceVu0FMATRIX sub_light;
sceVu0FMATRIX sub_lightcolor;
sceVu0FVECTOR sub_ambientlight;

/* The monsters of the floor that the player is on. */
CMonstorUnit MainMonstorUnit;

/* The life gauge of the enemy the player has locked on to. */
ENEMY_LIFE_GAGE EnemyLifeGage;

/* The trail that the player's weapon leaves as it swings. */
CWeaponEffect CWeaponFx;

/* The element trails the player's weapon leaves. */
CWeaponElement CWeaponElFx[4];

/* The five Atlamillia effects. */
CSHOT_EFFECT MasekiEffect[5];

namespace {

/* The effect the player's own character shoots. */
CSHOT_EFFECT CharaMainEffect;

} // namespace

/* The effect the player's own character breaks things with. */
CSHOT_EFFECT CharaMainEffectCrash;

/* The character that the player controls. */
CCharacter CharaMain;

/* The hand that the player's character holds an item out with. */
CCharacter CharaHand;

/* The weapon models the player's character can hold. */
CCharacter MainWeapon;
CCharacter DefaultWeapon;

namespace {

CCharacter CrashWeapon;

/* The weapon that the player has equipped. */
WEAPON_HAVE WeaponHave;

} // namespace

/* The messages the dungeon shows over the picture. */
CDngMessageMan DngMessMan;

/* The items lying on the floor that the player is on. */
CRandomItem MainRandomItem;

/* The items lying on the matching floor of the back dungeon. */
CRandomItem SubRandomItem;

/* The item that a monster stole. */
CStealItem StealItem;

/* The characters that walk the dungeon alongside the player. */
CNPCharacter NPCUnit[6];

/* The splash the player's feet throw up in water. */
CCharacter Water_Splash;

/* The circle a trap draws on the floor, and its texture animations. */
CCharacter Trap_Circle;

namespace {

CTexAnimeData Trap_Circle_TexAnim[16];

} // namespace

/* The effect that plays as the player escapes the dungeon. */
CCharacter EscapeEffect;

/* The alpha of each of the three floor-title logos. */
s32 rogoAlphaA[3];

/* How long each of the three floor-title logos stays at full brightness. */
s32 rogoAlphaW[3];

/* The effect that plays as the player changes character. */
CCharacter NewChangeFx;

/* Where the player took the Atlamillia they last picked up. */
sceVu0FVECTOR atraGetPos;

/* Which way the player faced as they took it. */
sceVu0FVECTOR atraGetRot;

/* The effect that a short Atlamillia use lights the player with. */
CCharacter shortAtraEffect;

/* The models an opened treasure box shows, and the effects that play over them. */
CMotionModel itemOpenSmall;
CMotionModel itemOpenSmallFx;
CMotionModel itemOpenBig;
CMotionModel itemOpenBigFx;

/* The debug text window the dungeon draws. */
CDebugFont DbgMsg;

/* Where the item the player is about to throw will land. */
BOMB_INFO BombInfo;

/* The dust the player kicks up as they run. */
CRunEffect CRunFx__2;

namespace {

/* The wind that blows across the dungeon. */
// init_priority keeps this static's place in the order rand() is drawn from; see app/runtime.cpp.
CWind kaze __attribute__((init_priority(65013)));

} // namespace

/* Where the marker over an event stands. */
sceVu0FVECTOR iventPos;

/* The message windows the dungeon draws over the picture. */
ClsMes DngMes1;
ClsMes DngMes2;
ClsMes DngMesStb;
ClsMes BtEventMes0;
ClsMes BtEventMes1;

/* The marks that a weapon throws off as it breaks. */
CHitMark WeaponCrashEffect;

/* The marks the dungeon throws off a hit. */
CHitMark      HitMark[16];
CHitPointMark HitPointMark[16];

/* The marks that the hits the player took have left. */
CHitPointMark MyHitPointMark[16];

/* The effect that heals the player. */
CHealEffect HealEffect;

/* The models of the items lying on the floor. */
CMainItemModel mainItemModel;

/* The models of the items the player is running. */
CActiveItemPack activeItem;

namespace {

/* How square on to the player each monster of the floor stands. */
float MonstorPicupInner[36];

} // namespace

/* The damage numbers the dungeon draws. */
CHitValue HitValue[32];

namespace {

/* The shots the player's character has in the air. */
CSHOT ShotData;

/* The shot effects the dungeon can run. */
CSHOT_EFFECT_PACK ShotEffect;

/* The machine gun that Osmond's rapid-fire action shoots. */
CSHOT_MACHINGUN OzumondShot;

} // namespace

/* The flame that Osmond's flamethrower action shoots. */
CSHOT_FIREBAR OzumondFire;

/* The bullets that Osmond's machine gun shoots. */
CHIT_MACHINGUN_EFFECT OzumondShotEffect;

namespace {

/* The collisions of the floor the player is on. */
CCollisionData CColData;

} // namespace

/* The bombs the player has thrown. */
CItemBombEffect CBomb__2[3];

namespace {

/* The shock wave the dungeon can run. */
CShockWave ShockWave;

} // namespace

/* How fast the player fell the frame before. */
sceVu0FVECTOR veloOld;

/* The texture animations every dungeon texture runs. */
CTextureAnime BtTexAnime;
CTexAnimeData BtTexAnimeData[96];

/* The texture animations of each character that walks the dungeon. */
CTexAnimeData BtNPCTexAnimeData[4][32];

namespace {

/* One texture animation for every texture the player's model draws. */
CTexAnimeData CharaMainTexAnim[64];

/* The buffers the dungeon loads its models, messages and scripts into. */
CDataAlloc2<1> MainModelBuffer(-1);

} // namespace

CDataAlloc2<1> MapModelBuffer(-1);

namespace {

CDataAlloc2<1> CharaModelBuffer(-1);
CDataAlloc2<1> WeaponModelBuffer(-1);
CDataAlloc2<1> WEffectModelBuffer(-1);
CDataAlloc2<1> MasekiModelBuffer(-1);
CDataAlloc2<1> BtMesBuffer(-1);

} // namespace

CDataAlloc2<1> BtSteebMesBuffer(-1);

namespace {

CDataAlloc2<1> BtStartLogoBuffer(-1);

} // namespace

CDataAlloc2<1> MonstorModelBuffer(-1);

/* Script working memory, one block per monster on the floor. */
CDataAlloc2<1> MonstorScriptBuffer[16];

/* The buffers the dungeon's item cache and battle scripts run out of. */
CDataAlloc2<1> BtItemCashArea[6];
CDataAlloc2<1> BtScriptWorkBuffer(-1);
CDataAlloc2<1> BtCashBuffer(-1);

namespace {

CDataAlloc2<1> BtScriptBuffer(-1);

} // namespace

CDataAlloc2<1> BtSystemScriptFileBuffer(-1);

/* The name of the texture that the floor number draws with. */
char floor_name[32];

/* Where the camera looks, relative to what it follows. */
sceVu0FVECTOR ref_off;

namespace {

/* Where Ruby's shot starts, and the way it flies. */
sceVu0FVECTOR ruby_effect_pos;
sceVu0FVECTOR ruby_effect_vec;

/* The colour the ambient-colour animation drives the player's model to. */
float unitAmbientAnime_rgb[3];

} // namespace

/* The debug message overlay. */
CDebugFont CDbgMsg;

namespace {

// clang-format off
/* The effect each playable character, and each of Ruby's and Osmond's forms, draws. */
BT_SHOT_EFFECT MyEntryEffect_TOAN = {
    .model_name = "c01_fuusya", .stationary = 0, .face_flight = 0,
    .speed = {0.0f, 0.0f, 0.0f, 0.0f}, .radius = {20.0f, 0.0f, 0.0f, 0.0f},
    .life_time = 160, .damage = 8, .hit_flags = 8, .hit_kind = 2, .target = 2,
    .motion = {0, -1, -1, -1}, .end_effect = -1, .unk_056 = {0, 0}, .bomb_scale = 0.0f, .bomb_damage = 0,
    .sound = {-1, -1, -1, -1},
};
// clang-format on

// clang-format off
BT_SHOT_EFFECT MyEntryEffect_SYAO = {
    .model_name = "mgan01", .stationary = 0, .face_flight = 0,
    .speed = {0.0f, 0.0f, 0.0f, 0.0f}, .radius = {1.5f, 0.0f, 0.0f, 0.0f},
    .life_time = 160, .damage = 8, .hit_flags = 0, .hit_kind = 2, .target = 2,
    .motion = {0, -1, -1, -1}, .end_effect = -1, .unk_056 = {0, 0}, .bomb_scale = 0.0f, .bomb_damage = 0,
    .sound = {-1, -1, -1, -1},
};
// clang-format on

// clang-format off
BT_SHOT_EFFECT MyEntryEffect_GORO = {
    .model_name = "c06a_tameex", .stationary = 0, .face_flight = 0,
    .speed = {0.0f, 0.0f, 0.0f, 0.0f}, .radius = {15.0f, 0.0f, 0.0f, 0.0f},
    .life_time = 160, .damage = 8, .hit_flags = 0, .hit_kind = 2, .target = 2,
    .motion = {0, -1, -1, -1}, .end_effect = -1, .unk_056 = {0, 0}, .bomb_scale = 0.0f, .bomb_damage = 0,
    .sound = {-1, -1, -1, -1},
};
// clang-format on

// clang-format off
BT_SHOT_EFFECT MyEntryEffect_RUBY_F1 = {
    .model_name = "c05_f03", .stationary = 0, .face_flight = 1,
    .speed = {2.5f, 2.5f, 0.0f, 0.0f}, .radius = {5.0f, 5.0f, 0.0f, 0.0f},
    .life_time = 180, .damage = 8, .hit_flags = 1, .hit_kind = 3, .target = 2,
    .motion = {0, 1, 2, -1}, .end_effect = -1, .unk_056 = {0, 0}, .bomb_scale = 0.0f, .bomb_damage = 0,
    .sound = {-1, -1, 101, -1},
};
// clang-format on

// clang-format off
BT_SHOT_EFFECT MyEntryEffect_RUBY_C1 = {
    .model_name = "c05_r03", .stationary = 0, .face_flight = 1,
    .speed = {2.5f, 2.5f, 0.0f, 0.0f}, .radius = {5.0f, 5.0f, 0.0f, 0.0f},
    .life_time = 180, .damage = 8, .hit_flags = 2, .hit_kind = 3, .target = 2,
    .motion = {0, 1, 2, -1}, .end_effect = -1, .unk_056 = {0, 0}, .bomb_scale = 0.0f, .bomb_damage = 0,
    .sound = {-1, -1, 102, -1},
};
// clang-format on

// clang-format off
BT_SHOT_EFFECT MyEntryEffect_RUBY_T1 = {
    .model_name = "c05_t03", .stationary = 0, .face_flight = 1,
    .speed = {2.5f, 2.5f, 0.0f, 0.0f}, .radius = {5.0f, 5.0f, 0.0f, 0.0f},
    .life_time = 180, .damage = 8, .hit_flags = 4, .hit_kind = 3, .target = 2,
    .motion = {0, 1, 2, -1}, .end_effect = -1, .unk_056 = {0, 0}, .bomb_scale = 0.0f, .bomb_damage = 0,
    .sound = {-1, -1, 103, -1},
};
// clang-format on

// clang-format off
BT_SHOT_EFFECT MyEntryEffect_RUBY_W1 = {
    .model_name = "c05_e03", .stationary = 0, .face_flight = 1,
    .speed = {2.5f, 2.5f, 0.0f, 0.0f}, .radius = {5.0f, 5.0f, 0.0f, 0.0f},
    .life_time = 180, .damage = 8, .hit_flags = 8, .hit_kind = 3, .target = 2,
    .motion = {0, 1, 2, -1}, .end_effect = -1, .unk_056 = {0, 0}, .bomb_scale = 0.0f, .bomb_damage = 0,
    .sound = {-1, -1, 104, -1},
};
// clang-format on

// clang-format off
BT_SHOT_EFFECT MyEntryEffect_RUBY_H1 = {
    .model_name = "c05_h03", .stationary = 0, .face_flight = 1,
    .speed = {2.5f, 2.5f, 0.0f, 0.0f}, .radius = {5.0f, 5.0f, 0.0f, 0.0f},
    .life_time = 180, .damage = 8, .hit_flags = 16, .hit_kind = 3, .target = 2,
    .motion = {0, 1, 2, -1}, .end_effect = -1, .unk_056 = {0, 0}, .bomb_scale = 0.0f, .bomb_damage = 0,
    .sound = {-1, -1, 105, -1},
};
// clang-format on

// clang-format off
BT_SHOT_EFFECT MyEntryEffect_UNGAGA = {
    .model_name = "c10a_ex", .stationary = 0, .face_flight = 1,
    .speed = {0.2f, 0.9f, 0.2f, 0.2f}, .radius = {8.0f, 8.0f, 6.0f, 6.0f},
    .life_time = 90, .damage = 20, .hit_flags = 8, .hit_kind = 3, .target = 2,
    .motion = {0, 1, 2, 2}, .end_effect = -1, .unk_056 = {0, 0}, .bomb_scale = 0.0f, .bomb_damage = 0,
    .sound = {-1, -1, -1, -1},
};
// clang-format on

// clang-format off
BT_SHOT_EFFECT MyEntryEffect_OZUMOND_M = {
    .model_name = "mgan01", .stationary = 0, .face_flight = 0,
    .speed = {0.0f, 0.0f, 0.0f, 0.0f}, .radius = {0.0f, 0.0f, 0.0f, 0.0f},
    .life_time = 10, .damage = 8, .hit_flags = 0, .hit_kind = 3, .target = 2,
    .motion = {1, -1, -1, -1}, .end_effect = -1, .unk_056 = {0, 0}, .bomb_scale = 0.0f, .bomb_damage = 0,
    .sound = {-1, -1, -1, -1},
};
// clang-format on

// clang-format off
BT_SHOT_EFFECT MyEntryEffect_OZUMOND_H = {
    .model_name = "rgan01", .stationary = 0, .face_flight = 1,
    .speed = {3.5f, 3.5f, 0.2f, 0.2f}, .radius = {0.0f, 4.5f, 0.0f, 0.0f},
    .life_time = 180, .damage = 8, .hit_flags = 0, .hit_kind = 3, .target = 2,
    .motion = {0, 1, 2, -1}, .end_effect = -1, .unk_056 = {0, 0}, .bomb_scale = 0.0f, .bomb_damage = 0,
    .sound = {-1, -1, -1, -1},
};
// clang-format on

// clang-format off
BT_SHOT_EFFECT MyEntryEffect_OZUMOND_H2 = {
    .model_name = "rgan02", .stationary = 0, .face_flight = 1,
    .speed = {3.5f, 3.5f, 0.2f, 0.2f}, .radius = {0.0f, 4.5f, 0.0f, 0.0f},
    .life_time = 180, .damage = 8, .hit_flags = 0, .hit_kind = 3, .target = 2,
    .motion = {0, 1, 2, -1}, .end_effect = -1, .unk_056 = {0, 0}, .bomb_scale = 0.0f, .bomb_damage = 0,
    .sound = {-1, -1, -1, -1},
};
// clang-format on

// clang-format off
BT_SHOT_EFFECT MyEntryEffect_OZUMOND_V2 = {
    .model_name = "mgan02", .stationary = 0, .face_flight = 0,
    .speed = {0.0f, 0.5f, 0.2f, 0.2f}, .radius = {0.0f, 1.5f, 0.0f, 0.0f},
    .life_time = 10, .damage = 8, .hit_flags = 0, .hit_kind = 3, .target = 2,
    .motion = {1, -1, -1, -1}, .end_effect = -1, .unk_056 = {0, 0}, .bomb_scale = 0.0f, .bomb_damage = 0,
    .sound = {-1, -1, -1, -1},
};
// clang-format on

// clang-format off
BT_SHOT_EFFECT MyEntryEffect_OZUMOND_G = {
    .model_name = "mgan03", .stationary = 0, .face_flight = 0,
    .speed = {0.0f, 0.5f, 0.2f, 0.2f}, .radius = {0.0f, 1.5f, 0.0f, 0.0f},
    .life_time = 10, .damage = 8, .hit_flags = 0, .hit_kind = 3, .target = 2,
    .motion = {1, -1, -1, -1}, .end_effect = -1, .unk_056 = {0, 0}, .bomb_scale = 0.0f, .bomb_damage = 0,
    .sound = {-1, -1, -1, -1},
};
// clang-format on

// clang-format off
BT_SHOT_EFFECT MyEntryEffect_OZUMOND_S = {
    .model_name = "mgan04", .stationary = 0, .face_flight = 0,
    .speed = {0.0f, 0.0f, 0.0f, 0.0f}, .radius = {0.0f, 0.0f, 0.0f, 0.0f},
    .life_time = 10, .damage = 8, .hit_flags = 0, .hit_kind = 3, .target = 2,
    .motion = {1, -1, -1, -1}, .end_effect = -1, .unk_056 = {0, 0}, .bomb_scale = 0.0f, .bomb_damage = 0,
    .sound = {-1, -1, -1, -1},
};
// clang-format on

/* The effect table each playable character draws its own effects from. */
BT_SHOT_EFFECT *MyEffectEntry_Tbl[15] = {
    &MyEntryEffect_TOAN,
    &MyEntryEffect_SYAO,
    &MyEntryEffect_GORO,
    &MyEntryEffect_RUBY_F1,
    &MyEntryEffect_RUBY_C1,
    &MyEntryEffect_RUBY_T1,
    &MyEntryEffect_RUBY_W1,
    &MyEntryEffect_RUBY_H1,
    &MyEntryEffect_UNGAGA,
    &MyEntryEffect_OZUMOND_M,
    &MyEntryEffect_OZUMOND_H,
    &MyEntryEffect_OZUMOND_H2,
    &MyEntryEffect_OZUMOND_V2,
    &MyEntryEffect_OZUMOND_G,
    &MyEntryEffect_OZUMOND_S,
};

// clang-format off
/* The effect each of the five Atlamillia draws as it is taken. */
BT_SHOT_EFFECT MyEntryEffect_Maseki00 = {
    .model_name = "maseki_ex", .stationary = 0, .face_flight = 1,
    .speed = {0.0f, 0.0f, 0.0f, 0.0f}, .radius = {10.0f, 0.0f, 0.0f, 0.0f},
    .life_time = 3, .damage = 35, .hit_flags = 1, .hit_kind = 3, .target = 2,
    .motion = {0, -1, -1, -1}, .end_effect = -1, .unk_056 = {0, 0}, .bomb_scale = 0.0f, .bomb_damage = 0,
    .sound = {-1, -1, -1, -1},
};
// clang-format on

// clang-format off
BT_SHOT_EFFECT MyEntryEffect_Maseki01 = {
    .model_name = "maseki_ex", .stationary = 0, .face_flight = 1,
    .speed = {0.0f, 0.0f, 0.0f, 0.0f}, .radius = {10.0f, 0.0f, 0.0f, 0.0f},
    .life_time = 3, .damage = 35, .hit_flags = 2, .hit_kind = 3, .target = 2,
    .motion = {1, -1, -1, -1}, .end_effect = -1, .unk_056 = {0, 0}, .bomb_scale = 0.0f, .bomb_damage = 0,
    .sound = {-1, -1, -1, -1},
};
// clang-format on

// clang-format off
BT_SHOT_EFFECT MyEntryEffect_Maseki02 = {
    .model_name = "maseki_ex", .stationary = 0, .face_flight = 1,
    .speed = {0.0f, 0.0f, 0.0f, 0.0f}, .radius = {10.0f, 0.0f, 0.0f, 0.0f},
    .life_time = 3, .damage = 35, .hit_flags = 4, .hit_kind = 3, .target = 2,
    .motion = {3, -1, -1, -1}, .end_effect = -1, .unk_056 = {0, 0}, .bomb_scale = 0.0f, .bomb_damage = 0,
    .sound = {-1, -1, -1, -1},
};
// clang-format on

// clang-format off
BT_SHOT_EFFECT MyEntryEffect_Maseki03 = {
    .model_name = "maseki_ex", .stationary = 0, .face_flight = 1,
    .speed = {0.0f, 0.0f, 0.0f, 0.0f}, .radius = {10.0f, 0.0f, 0.0f, 0.0f},
    .life_time = 3, .damage = 35, .hit_flags = 8, .hit_kind = 3, .target = 2,
    .motion = {2, -1, -1, -1}, .end_effect = -1, .unk_056 = {0, 0}, .bomb_scale = 0.0f, .bomb_damage = 0,
    .sound = {-1, -1, -1, -1},
};
// clang-format on

// clang-format off
BT_SHOT_EFFECT MyEntryEffect_Maseki04 = {
    .model_name = "maseki_ex", .stationary = 0, .face_flight = 1,
    .speed = {0.0f, 0.0f, 0.0f, 0.0f}, .radius = {10.0f, 0.0f, 0.0f, 0.0f},
    .life_time = 3, .damage = 35, .hit_flags = 16, .hit_kind = 3, .target = 2,
    .motion = {4, -1, -1, -1}, .end_effect = -1, .unk_056 = {0, 0}, .bomb_scale = 0.0f, .bomb_damage = 0,
    .sound = {-1, -1, -1, -1},
};
// clang-format on

} // namespace

/* A matrix of zeros, which every light of a field starts from. */
sceVu0FMATRIX ZeroMatrix = {
    {0.0f, 0.0f, 0.0f, 0.0f},
    {0.0f, 0.0f, 0.0f, 0.0f},
    {0.0f, 0.0f, 0.0f, 0.0f},
    {0.0f, 0.0f, 0.0f, 0.0f}
};

/* How far the fog reaches and how it thickens, on the front and the back floors. */
float main_fogRate[4] = {1000.0f, 3500.0f, 0.0f, 255.0f};
float sub_fogRate[4] = {1000.0f, 3500.0f, 0.0f, 255.0f};

namespace {

/* The ambient light every field starts from. */
sceVu0FVECTOR def_ambientlight = {64.0f, 64.0f, 64.0f, 128.0f};

/* The light colours every field starts from. */
sceVu0FMATRIX def_lightcolor = {
    {120.0f, 120.0f, 120.0f, 128.0f},
    {0.0f,   0.0f,   0.0f,   0.0f  },
    {0.0f,   0.0f,   0.0f,   0.0f  },
    {0.0f,   0.0f,   0.0f,   0.0f  }
};

} // namespace

/* How the treasure-box models, and the effects over them, play their opening motions. */
MOTION_INFO itemOpenSmall_info = {.start = 10, .end = 80, .speed = 0.5f, .unk_0C = 0};

namespace {

MOTION_INFO itemOpenSmallFx_info = {.start = 10, .end = 80, .speed = 0.5f, .unk_0C = 0};

} // namespace

MOTION_INFO itemOpenBig_info = {.start = 10, .end = 80, .speed = 0.5f, .unk_0C = 0};

namespace {

MOTION_INFO itemOpenBigFx_info = {.start = 10, .end = 80, .speed = 0.5f, .unk_0C = 0};

} // namespace

/* How fast the player is being thrown. */
sceVu0FVECTOR velo__2 = {0.0f, 0.0f, 0.0f, 0.0f};

/* Where the right stick is pushing the camera. */
sceVu0FVECTOR velo2 = {0.0f, 0.0f, 0.0f, 0.0f};

/* The way the last blow throws the player. */
sceVu0FVECTOR blowVelo = {0.0f, 0.0f, 0.0f, 0.0f};

/* Every texture the dungeon loads on its way in, and the block each goes to. */
LOADTEXTURE_INFO2 texdata__2[] = {
    {.name = "#frame_image#640#" SCREEN_HEIGHT_STR "#4",     .block_no = 1,  .mipmap = 0},
    {.name = "dun/etc/cursor.img",                           .block_no = 1,  .mipmap = 0},
    {.name = "gatekey00.img",                                .block_no = 1,  .mipmap = 0},
    {.name = "#water#640#" HALF_BUFFER_HEIGHT_STR "#4",      .block_no = 13, .mipmap = 0},
    {.name = "dun/etc/atrtx.img",                            .block_no = 22, .mipmap = 0},
    {.name = "wepready.img",                                 .block_no = 2,  .mipmap = 0},
    {.name = "#shadow_buf#640#" HALF_BUFFER_HEIGHT_STR "#3", .block_no = 15, .mipmap = 0},
    {.name = "dun/effect/basefx00.img",                      .block_no = 18, .mipmap = 0},
    {.name = "basefx01.img",                                 .block_no = 18, .mipmap = 0},
    {.name = "dun/d01/effect/fx_foot.img",                   .block_no = 18, .mipmap = 0},
    {.name = "dun/effect/bombfx00.img",                      .block_no = 19, .mipmap = 0},
    {.name = "pause.img",                                    .block_no = 7,  .mipmap = 0},
    {.name = "#dbgwork#640#" SCREEN_HEIGHT_STR "#3",         .block_no = 12, .mipmap = 0},
    {.name = "check/ankfont.img",                            .block_no = 12, .mipmap = 0},
    {.name = "element.img",                                  .block_no = 18, .mipmap = 0},
    {.name = "igetfx.img",                                   .block_no = 22, .mipmap = 0},
    {.name = nullptr,                                        .block_no = 0,  .mipmap = 0},
};

namespace {

/* The textures the loading screen itself draws with. */
LOADTEXTURE_INFO texdata2[3] = {
    {.name = "#dbgwork#640#" SCREEN_HEIGHT_STR "#3", .block_no = 12, .mipmap = 0},
    {.name = "check/ankfont.img",                    .block_no = 12, .mipmap = 0},
    {.name = "",                                     .block_no = 0,  .mipmap = 0},
};

/* How far the camera is allowed to pull back. */
s32 camera_dist_mode__3 = 1;

} // namespace

/* The colour the fog draws, on the front and the back floors. */
u8 main_fogColor[3] = {0x60, 0xA0, 0xEF};
u8 sub_fogColor[3] = {0x60, 0xA0, 0xEF};

/* How long the floor title has been on screen, in frames. */
s32 startCnt2 = -1;

/* Which character the menu had the cursor on when it opened. */
s32 oldUnitNow = -1;

/* Distances the camera keeps from what it follows. */
float camera_up_near_dist = 60.0f;

namespace {

float camera_near_dist__2 = 10.0f;
float camera_far_dist__2 = 80.0f;

/* The colours the enemy life gauge draws the life it has left with. */
spRGBA ELife1 = {.r = 0xFF, .g = 0x00, .b = 0x00, .a = 0x80};
spRGBA ELife2 = {.r = 0xFF, .g = 0xB4, .b = 0x00, .a = 0x80};

/* The colours the enemy life gauge draws its border with. */
spRGBA ELifeB1 = {.r = 0x80, .g = 0x9B, .b = 0xFF, .a = 0x80};
spRGBA ELifeB2 = {.r = 0x80, .g = 0x9B, .b = 0xFF, .a = 0x80};

/* The colour the enemy life gauge draws the life it has lost with. */
spRGBA ELifeN1 = {.r = 0x00, .g = 0x00, .b = 0x20, .a = 0x80};

} // namespace

/* What the dungeon hands the Georama editor to follow. */
GAME_ENV GameEnv;

/* The player's dungeon progress and party. */
CUserStatus *UserStatus;

namespace {

/* How long the dungeon has been waiting for the player, in frames. */
s32 timeOutCount;

} // namespace

s32 BtRubyDoorKey;

namespace {

/* Whether the debug overlay draws. */
s32 BtDebugFlag;

} // namespace

/* Whether the game is playing itself for the attract loop. */
s32 autoDemo;

/* Whether the player has finished every floor of the dungeon. */
s32 BtAllClear;
s32 oldMsgNo;
s32 oldMsgNo2;

/* Polygons the step test has picked up this frame. */
s32 colPolyNum;

/* Whether the player is pressing the button that fires an action. */
s32 PadInput_OK;

/* Which button fires an action and which cancels one. */
s32 PadInput_NO;

/* The shot effect Ruby's charge is running, or -1 for none. */
s32 ruby_effect_id;

/* The camera that the dungeon draws through. */
CCameraFollow *NowCamera__3;

/* The heading the eye camera looks along. */
float viewAngleH__2;

/* The pitch the eye camera looks along. */
float viewAngleV__2;

/* Which way the player is looking. */
s32 viewMode__2;

namespace {

s32 faceEyeCount;

} // namespace

s32 cameraAuto;

/* Whether the camera was moving itself before that. */
s32 cameraAutoOld;

/* Whether the player is in the back dungeon rather than the main one. */
s32 BtUraDongeon;

/* Provides the current dungeon map. */
CDungeonMap *NowDngMap;

/* The Dran field being drawn. */
CDranMapField *NowDranMapField;

/* The events of the floor the dungeon is drawing. */
CDungeonEventMan *NowEventMan;
u8                main_bgColor[3];
u8                sub_bgColor[3];

/* Which lighting the field draws under. */
s32 lightingMode;

/* The monsters of the floor that the player is on. */
CMonstorUnit *NowMonstorUnit;

/* Whether an effect is held over from the floor the player came from. */
s32 CEffectHold;

/* Whether the effects are hidden this frame. */
s32 CEffectHyde;

/* Whether a monster is held over from the floor the player came from. */
s32 CMonUnitHold;

/* Whether the monsters are hidden this frame. */
s32 CMonUnitHyde;

/* Whether the player's character holds an item out. */
s32 CharaMainHandViewFlag;

namespace {

/* Whether the monsters draw this frame. */
s32 MonstorViewFlag;

} // namespace

/* The effect the player's character is shooting now. */
CSHOT_EFFECT *NowMainEffect;

/* The model of the weapon that the player has equipped. */
CCharacter *NowWeapon;

/* The weapon that the player has equipped. */
WEAPON_HAVE *NowWeaponHave;

/* The items lying on the floor the dungeon is drawing. */
CRandomItem *RandomItem;

namespace {

/* Provides the model used when an item is stolen. */
CFrameVu1 *StealModel;

} // namespace

/* The frame that the player's model hangs off. */
CFrameVu1 *CharaFrame;

/* Whether the water splash is playing. */
s32 Water_Splash_actFlag;

/* Whether the player is escaping the dungeon. */
s32 EscapeFlag;

/* Whether the floor title is on screen. */
s32 rogoSwitch2;

/* Where the floor title stood before the menu took the screen. */
s32 oldRogoY3;

/* The vertical position of the floor title, in screen pixels. */
s32 rogoY3;

/* Whether the character-change effect is playing. */
s32 NewChangeFxFlag;

/* Which short Atlamillia use is running. */
s32   atraShortGetType;
float atraGetStatusRate__2;
s32   atraGetStatus;

/* Whether the Atlamillia message board is up. */
s32    atraGetMsgBord;
float  atraGetMsgBordRate;
u_int *shortAtraEffectPtr;
float  itemNormalScale;
s32    itemOpenSmallFlag;
u_int *itemOpenItemChr;

/* The model the item a gate holds draws with. */
CFrame *itemBoxModel;
float   itemWeponScale;
s32     itemOpenBigFlag;

/* How long the dungeon has waited before it opens the menu, in frames. */
s32 battleMenuWait;

/* Which item the mini item menu has the cursor on. */
s32 miniItemSelNo;

/* Which of the dungeon's loops runs this frame, as a BtGameMode. */
s32 BtGameModeFlag;

/* Whether the dungeon draws anything at all this frame. */
s32 BtAllDrawFlag;

/* Whether the models hold their step this frame. */
s32 driveStepHold;

/* Whether the picture is being captured this frame. */
s32 frameCaputer;

/* Whether the player's model skips interpolation on its next drive step. */
s32 driveNoInterpolate;

/* Which character the player is walking as. */
s32     nowUnitNow;
CFrame *cursorFrame;
CFrame *bombCursorFrame;
CFrame *bicCursorFrame;

/* Whether the lock-on cursor draws. */
s32 lockOnTargetDraw;

/* Which monster is locked on, or -1 for none. */
s32 lockOnTargetNo;

/* Whether an enemy is locked on. */
s32 lockOnTargetFlag;

/* Which way the player is shifting the lock-on cursor, or -1 for neither. */
s32 targetCursorShiftNo;

/* How many monsters the player could lock on to. */
s32 targetCursorShiftRot;

/* The floor the player chose. */
s32 selectMapNo;

/* Which of the running items the mini item menu has the cursor on. */
s32 itemNowSel;

/* Whether the player has the map of this floor equipped. */
s32 BtEquipMap;

/* Whether the player has a sun stone equipped. */
s32 BtEquipMasuisyou;
s32 iventInfo;
s32 iventMarker;

/* Which event the player is standing on, or -1 for none. */
s32 iventActive;

/* Whether the mini-map draws; SetMIniMapStatus takes it away and gives it back. */
s32 infoMap;

/* What the mini-map drew before SetMIniMapStatus took it away. */
s32 infoMapOld;

/* How hard the player is pushing the stick. */
float stickVector;

/* How hard the player is pushing the right stick. */
float stickVector2;

/* Whether the player is running on a speed boost. */
s32 BtBySpeedFlag;

/* Whether the player asked to leave through the menu. */
s32 exitMenuFlag;

/* Whether the item list the menu caches is still good. */
s32 BtItemListCashFlag;

/* Which item that event holds, or -1 for none. */
s32 BtEventItemNo;

/* The damage numbers the dungeon draws. */
CHitValue *NowHitValue;

/* The shots the player's character has in the air. */
CSHOT *NowShotData;

/* The shot effects the dungeon has running. */
CSHOT_EFFECT_PACK *NowShotEffect;

namespace {

/* The effect data Osmond's default weapon shoots. */
unsigned int *ozumond_default_effect;

} // namespace

/* The collisions the dungeon tests against. */
CCollisionData *NowColData;

/* The bombs the dungeon has running. */
CItemBombEffect *NowBombEffect;

/* The shock wave the dungeon has running. */
CShockWave *NowShockWave;

/* What the game is doing now, as a GameTask. */
s32 gameTask;

/* Why the dungeon is being left, or zero while the player is still in it. */
s32 existFlag;

/* Whether the player is leaving the trial dungeon. */
s32 tryalExit;
s32 DeadKeyWait;
s32 DeadKeyStartWait;

/* Where the pack file the dungeon read last went. */
unsigned int *old_read_buffer;

/* Which dungeon event is playing, or zero while the player has control. */
s32 BtEventMode;

/* The texture that the floor number draws with. */
CTexture *TEX_Floor1;
CTexture *TEX_WepGage;

namespace {

CTexture *TEX_Stayframe;
CTexture *TEX_Shadow;

/* The texture a dropped pile of gold draws with. */
CTexture *GoldTex;

} // namespace

/* How long the camera waits before it moves itself, in frames. */
s32 defCameraWait;

/* Whether the monster names are hidden. */
s32 MonstorNameOff;

// The third entry of texdata__2 stands for the floor's gate key image, whose name is built here.
void LoadBaseTexture() {
    std::size_t info_count = 0;
    while (texdata__2[info_count].name != nullptr) {
        info_count++;
    }
    auto        info = std::make_unique<LOADTEXTURE_INFO2[]>(info_count + 1);
    int         size = 0;
    std::string gate_key = std::format("gatekey0{}.img", selectMapNo + 1);
    std::string path = "dun/pack/teximg2";
    if (LanguageCode > LANG_JAPANESE) {
        path += std::format("_{}", LanguageCode);
    }
    path += ".pac";

    LoadFile(path.data(), read_buffer, nullptr);
    wait_now_loading_vsync();

    for (std::size_t i = 0; i < info_count; i++) {
        char *name = i == 2 ? gate_key.data() : texdata__2[i].name;

        // A name that starts with '#' asks the manager for a blank page of that size rather than
        // for a file inside the pack.
        if (name[0] == '#') {
            info[i].name = name;
        } else {
            u_int *found = GetPackFile(read_buffer, name, &size);

            if (found != nullptr) {
                info[i].name = reinterpret_cast<char *>(found);
            } else {
                std::println("Error::Pack->FileNotFound [{}]{}!!", i, name);
                exit__2(-1);
            }
        }

        info[i].block_no = texdata__2[i].block_no;
    }

    info[info_count].name = nullptr;
    TexManager.LoadTextureBlock(-1, info.get());

    // The message window's pages, and the three images the language's own pack supplies in place of
    // the names written here.
    std::array<LOADTEXTURE_INFO2, 8> mes_info = {
        {
         {.name = const_cast<char *>("#mes_frame_buff#640#480#4"), .block_no = 0x1A, .mipmap = 0},
         {.name = const_cast<char *>("#fukidashibase#640#224#4"), .block_no = 0x1A, .mipmap = 0},
         {.name = const_cast<char *>("#fontbase#512#256#1"), .block_no = 0x1A, .mipmap = 0},
         {.name = const_cast<char *>("meswin/gaiji.img"), .block_no = 0x1A, .mipmap = 0},
         {.name = const_cast<char *>("meswin/fuki256.img"), .block_no = 0x1A, .mipmap = 0},
         {.name = const_cast<char *>("meswin/syst04.img"), .block_no = 0x1A, .mipmap = 0},
         }
    };

    std::string mes_path = "meswin/mes_tex";
    if (LanguageCode > LANG_JAPANESE) {
        mes_path += std::format("_{}", LanguageCode);
    }
    mes_path += ".pak";

    LoadFile(mes_path.data(), read_buffer, nullptr);
    wait_now_loading_vsync();

    mes_info[3].name = reinterpret_cast<char *>(GetPackFile(read_buffer, const_cast<char *>("gaiji.img"), nullptr));
    mes_info[4].name = reinterpret_cast<char *>(GetPackFile(read_buffer, const_cast<char *>("fuki256.img"), nullptr));
    mes_info[5].name = reinterpret_cast<char *>(GetPackFile(read_buffer, const_cast<char *>("syst04.img"), nullptr));
    TexManager.LoadTextureBlock(-1, mes_info.data());
}

int LoadTempTexture(char **files, int block, char *buffer) {
    std::array<LOADTEXTURE_INFO2, 7> info{};
    int                              total = 0;

    for (int i = 0; files[i] != nullptr; i++) {
        int size = 0;

        LoadFile(files[i], buffer, &size);
        wait_now_loading_vsync();
        total += size;
        info[i].block_no = block;
        info[i].name = buffer;
        // Each file starts on the next 64-byte boundary after the previous one.
        buffer += ((size >> 6) + 1) << 6;
    }

    TexManager.DeleteTextureBlock(block);
    TexManager.LoadTextureBlockEX(block, info.data());
    return total;
}

void SetTempTexture(int block, char *name) {
    std::array<LOADTEXTURE_INFO2, 2> info{};

    info[0].block_no = block;
    info[0].name = name;
    TexManager.DeleteTextureBlock(block);
    TexManager.LoadTextureBlockEX(block, info.data());
}

void MemoryMapDump() {
    std::println("MainVisualData  \t{}/{}", MainModelBuffer.used, 0x9470);
    std::println("MotionData      \t{}/{}", MotionData.used, 10000);
    std::println("CharaModelBuffer\t{}/{}", CharaModelBuffer.used, 0x33450);
    std::println("MasekiModelBuffer\t{}/{}", MasekiModelBuffer.used, 0xFDE8);
    std::println("MapVisualData   \t{}/{}", MapModelBuffer.used, 0xA7F80);
    std::println("MonstorModelBuffer\t{}/{}", MonstorModelBuffer.used, MonstorModelBuffer.limit);
    std::println("SystemScriptBuffer\t{}/{}", BtSystemScriptFileBuffer.used, 0x88B8);
    std::println("StartLogoBuffer\t    {}/{}", BtStartLogoBuffer.used, 5000);
    std::println("BtMesBuffer\t        {}/{}", BtMesBuffer.used, 0x2904);
    std::println("BtSteebMesBuffer\t{}/{}", BtSteebMesBuffer.used, 0xDAC);
    TexManager.print_buff_info();
}

void GameInit() {
    int         size = 0;
    std::string name;
    short      *mes = nullptr;

    BtDebugFlag = 0;
    exitMenuFlag = 0;
    BtAllDrawFlag = 0;
    selectMapNo = main_select_menu_no;
    UserStatus = SaveData->GetDngStatus();
    UserStatus->Init();

    if (BtEventInfo.no_status_recover != 1) {
        CUserStatus *status = UserStatus;

        for (int i = 0; i < 6; i++) {
            status->hp[i] = status->max_hp[i];
            status->ailments[i] = 0;
            status->ailment_frames[i] = 0;
        }

        status->res_limit_zone_current = RES_LIMIT_ZONE_NONE;
    }

    int          map = selectMapNo;
    CUserStatus *status = UserStatus;

    status->cur_georama = map;

    sceVu0CopyMatrix(main_light, ZeroMatrix);
    sceVu0CopyMatrix(main_lightcolor, def_lightcolor);
    sceVu0CopyVector(main_ambientlight, def_ambientlight);
    sceVu0CopyMatrix(sub_light, ZeroMatrix);
    sceVu0CopyMatrix(sub_lightcolor, def_lightcolor);
    sceVu0CopyVector(sub_ambientlight, def_ambientlight);

    InitializeDataBuffer();
    SetDataBuffer(&MainModelBuffer, 0x9470);
    SetDataBuffer(&MotionData, 10000);
    SetDataBuffer(&CharaModelBuffer, 0x33450);
    SetDataBuffer(&MasekiModelBuffer, 0xFDE8);
    SetDataBuffer(&TextureData, 0xB3B0);
    SetDataBuffer(&BtSystemScriptFileBuffer, 0x88B8);
    SetDataBuffer(&BtMesBuffer, 0x2904);
    SetDataBuffer(&BtSteebMesBuffer, 0xDAC);
    SetDataBuffer(&BtStartLogoBuffer, 5000);
    SetDataBuffer(&BtScriptWorkBuffer, 100000);

    for (auto &arena : BtItemCashArea) {
        SetDataBuffer(&arena, 2501);
    }

    for (auto &arena : MonstorScriptBuffer) {
        SetDataBuffer(&arena, 1875);
    }

    SetDataBuffer(&MapModelBuffer, 0xA7F80);
    SetPacketReadBuffer(0x9344, 0x445C0);
    old_read_buffer = read_buffer;

    MainCamera__4.SetDistance(60.0f);
    MainCamera__4.SetHeight(0.0f);
    MainCamera__4.SetFollow(0.0f, 0.0f, 0.0f);
    MainCamera__4.Step(10);
    MainCamera__4.SetSpeed(8.0f);
    MainCamera__4.FollowOn();
    SubCamera.SetDistance(60.0f);
    SubCamera.SetHeight(0.0f);
    SubCamera.SetFollow(0.0f, 0.0f, 0.0f);
    SubCamera.Step(10);
    SubCamera.SetSpeed(8.0f);
    SubCamera.FollowOn();
    NowCamera__3 = &MainCamera__4;

    NowDngMap = &MainDungeonMap;
    NowDngMap->initalize();
    BtTexAnime.Initialize(BtTexAnimeData, 96);

    for (CDungeonEvent &slot : DngEventMan.slot) {
        slot.name[0] = '\0';
        slot.script_no = -1;
        slot.parts_id = -1;
        slot.switch_on = false;
        slot.enabled = false;
    }

    ClearEventData(DngEventMan);

    NowEventMan = &DngEventMan;
    lightingMode = 0;

    for (int i = 0; i < 12; i++) {
        CDranMapField *field = &DranMapField.field[i];

        field->Initialize();
        DranMapField.collision[i] = nullptr;
        DranMapField.state[i] = 3;
    }

    DranMapField.field_count = 0;
    DranMapField.collision_count = 0;
    NowDranMapField = DranMapField.field;
    NowColData = &CColData;

    for (auto &used : ShotData.used) {
        used = 0;
    }

    NowShotData = &ShotData;
    HealEffect.active = false;
    WaterSplash_Init();

    for (auto &effect : ShotEffect.effect) {
        effect.Initialize();
    }

    NowShotEffect = &ShotEffect;

    for (auto &effect : MasekiEffect) {
        effect.Initialize();
    }

    for (int i = 0; i < 16; i++) {
        OzumondShot.damage[i] = 1;
        OzumondShot.age[i] = 0;
    }

    for (int i = 0; i < 64; i++) {
        OzumondFire.damage[i] = 1;
        OzumondFire.state[i] = -1;
    }

    OzumondFire.start_index = 0;
    EscapeFlag = 0;
    WeaponCrashEffect.Initialize();

    for (auto &mark : HitMark) {
        mark.Initialize();
    }

    for (auto &timer : OzumondShotEffect.timer) {
        timer = -1;
    }

    NewChangeFxFlag = 0;

    for (auto &value : HitValue) {
        value.active = false;
        value.frame = nullptr;
    }

    NowHitValue = HitValue;

    MainMonstorUnit.model_count = 0;
    MainMonstorUnit.current_monster = 0;

    for (int i = 0; i < 16; i++) {
        MainMonstorUnit.script[i] = &MonstorScriptBuffer[i];
    }

    MainMonstorUnit.requested_event = -1;
    MainMonstorUnit.CleanViewMonstor(0);
    BtSetEventExtendTable();
    NowMonstorUnit = &MainMonstorUnit;
    NowWeaponHave = &WeaponHave;
    SetWeaponAttachStatus(NowWeaponHave);
    BtEventMode = 0;
    EdEventInfo.projection = -1.0f;
    EdEventInfo.screen_filter = false;
    EdFadeInit();
    ClearGateKeyStack();
    TexManager.Initialize(0x3FE0);
    LoadBaseTexture();

    LoadFileMenuData("itempack.img", read_buffer);
    wait_now_loading_vsync();
    TexManager.EnterIMGFile(reinterpret_cast<u_char *>(read_buffer), -1, 0, 0);
    LoadFileMenuData("stayframe.img", read_buffer);
    wait_now_loading_vsync();
    TexManager.EnterFixTextureZ(reinterpret_cast<u_char *>(read_buffer));

    mainItemModel.Initialize();

    for (int i = 0; i < 4; i++) {
        activeItem.item[i] = -1;
        activeItem.model[i] = -1;
    }

    activeItem.model[8] = -1;
    activeItem.now = 1;
    activeItem.models = &mainItemModel;
    BtSetActiveItemModel(read_buffer);

    BtActStatus.action_on = 0;
    BtActStatus.main_motion = -1;
    BtActStatus.hand_motion = -1;
    BtActStatus.motion_no = 0;
    BtActStatus.jumping = 0;
    BtActStatus.action_gauge = 100.0f;
    BtActStatus.gauge_exhausted = 0;
    BtActStatus.gauge_flash = 0;
    BtActStatus.recoil_frames = 0;
    BtActStatus.invincible_frames = 0;
    BtActStatus.shadow_visible = true;
    BtActStatus.weapon_visible = 1;
    BtActStatus.charge_level = 0;
    BtActStatus.guard_mode = 0;
    BtActStatus.can_act = 1;
    BtActStatus.unk_066 = 1;
    BtActStatus.slow_walking = 0;
    BtActStatus.camera_hold = 0;
    BtActStatus.action_busy = 0;
    BtActStatus.movement_locked = 0;
    BtActStatus.in_presentation = 0;
    BtActStatus.in_water = false;
    BtActStatus.action_no = 0;
    BtActStatus.gauge_exhausted = 0;
    BtActStatus.refill_frames = 10;
    BtActStatus.swapped_on_death = 0;
    BtActStatus.combo_window = 0;
    BtActStatus.combo_queued = 0;
    BtActStatus.move_vector[0] = 0.0f;
    BtActStatus.move_vector[1] = 0.0f;
    BtActStatus.move_vector[2] = 0.0f;
    BtActStatus.move_vector[3] = 0.0f;
    BtActStatus.move_power = 0.0f;
    BtActStatus.move_power_decay = 0.0f;
    BtActStatus.hud_shake_frames = 0;
    BtActStatus.hud_shake_y = 0;
    BtActStatus.frames_since_attack = 3600;
    BtActStatus.camera_shake_frames = 0;
    BtActStatus.fast_camera_frames = 0;
    BtActStatus.event_block_frames = 0;
    BtActStatus.swinging = 0;
    EdEventInfo.player_draw = true;
    EdEventInfo.player_shadow_draw = true;
    camera_dist_mode__3 = 2;
    cameraAuto = 0;
    faceEyeCount = 0;
    DispFade__3.FadeInit(128.0f);
    DispFade__3.FadeInStart(1.0f);

    switch (LanguageCode) {
        case LANG_JAPANESE:
            PadInput_OK = 0x20;
            PadInput_NO = 0x40;
            break;
        case LANG_ENGLISH_US:
        default:
            PadInput_OK = 0x40;
            PadInput_NO = 0x20;
            break;
    }

    DngMes1.Preset(MES_PRESET_SYSTEM);
    DngMes1.page_arrow = false;
    DngMes1.centre_rows = true;
    DngMes1.tex_block = 26;
    DngMes1.stay_frame = true;
    DngMes1.columns = 15;
    DngMes1.rows = 3;
    DngMes1.text_x = 48;
    DngMes1.text_y = 368;
    DngMes1.char_width = 11;
    DngMes1.char_height = 22;
    DngMes1.style = MES_EDGE_NONE;
    DngMes1.tex_buff = MesWinTexBuff_01;

    DngMes2.Preset(MES_PRESET_SYSTEM);
    DngMes2.page_arrow = false;
    DngMes2.centre_rows = true;
    DngMes2.tex_block = 26;
    DngMes2.stay_frame = false;
    DngMes2.columns = 15;
    DngMes2.rows = 3;
    DngMes2.text_x = 320;
    DngMes2.text_y = 346;
    DngMes2.char_width = 11;
    DngMes2.char_height = 22;
    DngMes2.tex_buff = MesWinTexBuff_02;

    DngMesStb.text_columns = MES_WIN_COLUMNS;
    DngMesStb.text_rows = 10;
    DngMesStb.text_len = 0;
    DngMesStb.text_width = 0;
    DngMesStb.text_height = 0;
    DngMesStb.fade = 0;
    DngMesStb.fade_in = true;
    DngMesStb.text_rate = DngMesStb.text_rate_set;
    DngMesStb.waiting = false;
    DngMesStb.text_at = 0;
    DngMesStb.text_no = 0;
    DngMesStb.text_from = 0;
    DngMesStb.page_from = 0;
    DngMesStb.InitMesWinTbl();
    DngMesStb.clut_now = DngMesStb.clut_default;
    DngMesStb.wait = 0;
    DngMesStb.blink = 0;
    DngMesStb.auto_page_wait = 0;
    DngMesStb.mes_made = -1;
    DngMesStb.edge_alpha = 128;

    for (auto &mes_no : DngMesStb.mes_no) {
        mes_no = -1;
    }

    for (auto &value : DngMesStb.values) {
        value = 0;
    }

    DngMesStb.value = 0;
    DngMesStb.value_signed = false;
    DngMesStb.value_show = true;
    DngMesStb.value_narrow = false;
    DngMesStb.space_width = -1;
    DngMesStb.space_area = -1;
    DngMesStb.cursor_row = -1;
    DngMesStb.cursor_y = 0;
    DngMesStb.cursor_lit = false;

    for (auto &pos : DngMesStb.line_pos) {
        pos.x = -1;
        pos.y = -1;
    }

    DngMesStb.Preset(MES_PRESET_NAME);
    DngMesStb.columns = 21;
    DngMesStb.rows = 4;
    DngMesStb.text_rate = 1.0f;
    DngMesStb.text_rate_set = 1.0f;
    DngMesStb.fade_speed = 1.0f;
    DngMesStb.page_arrow = false;
    DngMesStb.tex_block = 26;
    DngMesStb.tail_on = false;
    DngMesStb.clut_now = Color2Clut(FONT_COLOR_WHITE) & 0xFF;
    DngMesStb.columns = 15;
    DngMesStb.char_width = 12;
    DngMesStb.char_height = 24;
    DngMesStb.tex_buff = MesWinTexBuff_11;

    BtEventMes0.char_width = 12;
    BtEventMes0.char_height = 20;
    BtEventMes0.columns = 21;
    BtEventMes0.rows = 4;
    BtEventMes0.text_rate = 0.3f;
    BtEventMes0.text_rate_set = 0.3f;
    BtEventMes0.tex_block = 26;
    BtEventMes0.style = MES_EDGE_NONE;
    BtEventMes0.page_arrow = false;
    BtEventMes0.centre_rows = true;
    BtEventMes0.fade_speed = 0.1f;
    BtEventMes0.page_arrow = true;
    BtEventMes0.tex_buff = MesWinTexBuff_01;

    BtEventMes1.columns = 22;
    BtEventMes1.rows = 5;
    BtEventMes1.char_width = 12;
    BtEventMes1.char_height = 24;
    BtEventMes1.text_rate = 0.3f;
    BtEventMes1.text_rate_set = 0.3f;
    BtEventMes1.tex_block = 26;
    BtEventMes1.style = MES_EDGE_NONE;
    BtEventMes1.page_arrow = false;
    BtEventMes1.centre_rows = true;
    BtEventMes1.fade_speed = 0.1f;
    BtEventMes1.page_arrow = true;
    BtEventMes1.tex_buff = MesWinTexBuff_02;

    mes = reinterpret_cast<short *>(ArenaFree(BtMesBuffer));
    name = std::format("dun/message/ww_mes/dunmsd00_{}.mes", LanguageCode);
    LoadFile(name.data(), mes, &size);
    wait_now_loading_vsync();
    DngMes1.SetBuff(mes);
    DngMes2.SetBuff(mes);
    MonsterNameInit(&DngMes2, mes, MesWinTexBuff_02);
    BtEventMes0.SetBuff(mes);
    BtMesBuffer.Alloc((((size >> 6) + 1) << 6) >> 4);
    BtMesBuffer.Align64();

    mes = reinterpret_cast<short *>(ArenaFree(BtMesBuffer));
    name = std::format("dun/message/ww_mes/font18_{}.mes", LanguageCode);
    LoadFile(name.data(), mes, &size);
    wait_now_loading_vsync();
    BtEventMes1.SetBuff(mes);
    BtMesBuffer.Alloc((((size >> 6) + 1) << 6) >> 4);
    BtMesBuffer.Align64();

    mes = reinterpret_cast<short *>(ArenaFree(BtMesBuffer));
    LoadFile("meswin/system14e.bin", mes, &size);
    wait_now_loading_vsync();
    BtMesBuffer.Alloc((size >> 4) + 1);
    DngMes1.SetBuff_system(SystemMes);
    DngMes2.SetBuff_system(SystemMes);
    BtEventMes0.SetBuff_system(mes);
    ClearSystemMes();

    DngMessMan.message = -1;
    DngMessMan.timer = 0;
    DngMessMan.enabled = true;
    oldMsgNo = -1;
    oldMsgNo2 = -1;

    MainWeapon.Initialize();
    DefaultWeapon.Initialize();
    CrashWeapon.Initialize();

    ClearWeaponTrail();

    for (auto &element : CWeaponElFx) {
        element.Initialize();
    }

    SndSetReadBuffer(read_buffer);

    if (EdEventInfo.map_jump_bgm_play != 0) {
        SndBgmLoad(selectMapNo + 100);
        SndBgmPlay(0);
    }

    SndSoundLoad(selectMapNo + 100);

    if (selectMapNo == DUNGEON_DEMON_SHAFT) {
        SndSePlay(SE_AMBIENT_DUNGEON_HISS, 0, 0);
    }

    if (selectMapNo == DUNGEON_DIVINE_BEAST_CAVE) {
        SndSePlay(SE_AMBIENT_DUNGEON_HISS, 0, 0);
        SndSePlay(SE_AMBIENT_CAVE_DRIPS, 0, 0);
    }

    if (selectMapNo == DUNGEON_SHIPWRECK) {
        SndSePlay(SE_AMBIENT_SHIPWRECK, 0, 0);
    }

    if (selectMapNo >= DUNGEON_SUN_MOON_TEMPLE) {
        SndSePlay(SE_AMBIENT_DEEP_DUNGEON, 0, 0);
    }

    SndAmbientPlay(0);
    SndAmbientSetVol(0);
    BtBattleMusic_Init();

    LoadData();
    LoadStartLogo(selectMapNo);
    initBlendCnt(8, 1.0f);
    atraGetStatus = 0;
    atraGetStatusRate__2 = 0.0f;
    atraGetMsgBord = 0;
    CMonUnitHold = 0;
    CMonUnitHyde = 0;
    CEffectHold = 0;
    CEffectHyde = 0;
    itemOpenSmallFlag = 0;
    itemOpenBigFlag = 0;
    gateItemFlag = 0;
    driveStepHold = 0;
    driveNoInterpolate = 0;
    int dungeon = selectMapNo;

    if (UserStatus->floor_reached[dungeon] == -1) {
        BtAtraListMake(dungeon);
    }

    UserStatus->water_drain_disable = 0;
    UserStatus->step_disable = 0;
    nowUnitNow = UserStatus->cur_chara;
    rogoY3 = 32;
    BtStatusAlarmInit();

    for (auto &bomb : CBomb__2) {
        bomb.Initialize();
    }

    NowBombEffect = CBomb__2;
    ShockWave.active = false;
    NowShockWave = &ShockWave;
    BombInfo.aiming = false;
    frameCaputer = 0;
    BtActStatus.player_visible = 1;
    viewMode__2 = 0;
    itemNowSel = 1;
    iventInfo = DNG_EVENT_NONE;
    iventMarker = 0;
    int minimap = UserStatus->minimap_status;

    if (minimap != 3) {
        infoMap = 1;
        infoMapOld = 1;
    } else {
        infoMap = 0;
        infoMapOld = 0;
    }

    BtActStatus.player_visible = 1;
    CharaMainHandViewFlag = 0;
    MonstorViewFlag = 1;
    RsetUnitAmbientAnime();
    GamePad.AutoRepeatOff();
    GamePad.MenuModeOff();
    lockOnTargetFlag = 0;
    lockOnTargetNo = -1;
    EnemyLifeGage.on = 0;
    EnemyLifeGage.draw = true;
    existFlag = 0;
    tryalExit = 0;
    timeOutCount = 0;
    TEX_WepGage = TexManager.GetTexture("wepready", -1);
    TEX_Stayframe = TexManager.GetTexture("stayframe", -1);
    TEX_Shadow = TexManager.GetTexture("shadow_buf", -1);
    std::string gate_name = std::format("gatekey0{}", selectMapNo + 1);

    GoldTex = TexManager.GetTexture(gate_name.data(), -1);

    if (GoldTex == nullptr) {
        std::println("******** ERR");
    }

    BtMapJumpCashClear();

    CDungeonMap *map_now = NowDngMap;

    map_now->draw_dist_scale = 4.0f;
    static_cast<CDngStatusData *>(UserStatus)->SetNowFloor(0);

    CUserStatus *status_now = UserStatus;

    status_now->prev_floor = -1;

    DbgMsg.texture_name = "dbgwork";
    DbgMsg.x = 32;
    DbgMsg.y = 80;
    DbgMsg.width = 256;
    DbgMsg.height = 224;
    DbgMsg.alpha = 96;
    DebugInfomationInit();
    EdDSetFont(&DbgMsg);
    StealItem.Initialize(StealModel);
    BtItemListCashFlag = 0;
    defCameraWait = 0;
    MonstorNameOff = 0;
    BtSystemScriptEventInfoInit();
    BtSystemScriptInit();
    BtSystemScriptLoad(selectMapNo);
    MemoryMapDump();
    BtGameModeFlag = BT_GAME_MODE_PLAY;
    BtEventInfo.script_no = 100;
    BtEventInfo.ext_memory = false;
    gameTask = GAME_TASK_SCRIPT_START;
}

int GameLoop() {
    switch (BtGameModeFlag) {
        case BT_GAME_MODE_PLAY:
            DunMoveChara();
            break;
        case BT_GAME_MODE_BATTLE_MENU:
            if (BattleMenuCursor() == 0) {
                BtGameModeFlag = BT_GAME_MODE_PLAY;
            }

            break;
        default:
            break;
    }

    switch (BtGameModeFlag) {
        case BT_GAME_MODE_PLAY:
            DunMainDraw();
            break;
        case BT_GAME_MODE_BATTLE_MENU:
            BattleMenuDraw();
            break;
        case BT_GAME_MODE_ITEM_SELECT:
            if (EventItemSelectLoop(&miniItemSelNo) != 0) {
                BtGameModeFlag = BT_GAME_MODE_PLAY;
            }

            break;
        case BT_GAME_MODE_ENTRANCE_MENU: {
            int floor = DunEnterMenuLoop();

            if (floor != -1) {
                if (BtEventInfo.entrance_result != nullptr) {
                    BtEventInfo.entrance_result->i = floor;
                }

                BtEventInfo.entrance_result = nullptr;
                static_cast<CDngStatusData *>(UserStatus)->SetNowFloor(floor);
                BtGameModeFlag = BT_GAME_MODE_PLAY;
            }

            break;
        }
        case BT_GAME_MODE_ESCAPE_MSG: {
            int answer = DngEscapeMsgLoop();

            if (answer > 0) {
                if (BtEventInfo.escape_result != nullptr) {
                    BtEventInfo.escape_result->i = answer;
                }

                BtEventInfo.escape_result = nullptr;
                BtGameModeFlag = BT_GAME_MODE_PLAY;
            }

            break;
        }
        case BT_GAME_MODE_CHARA_CHANGE: {
            int chara = CharaChangeLoop();

            BtMiniChrSelectNo = chara;

            if (chara != 0) {
                BtGameModeFlag = BT_GAME_MODE_PLAY;
            }

            break;
        }
        default:
            break;
    }

    if (existFlag != 0) {
        while (ReadBGSync() == 1) {
        }

        // A floor the player is thrown out of keeps its music; one they leave
        // on their own starts the town music again.
        if (EdEventInfo.map_jump_bgm_stop != 0) {
            SndExit();
            SndBgmInit();
        } else {
            SndStopAllSe();
        }

        std::array<float, 4> fade{};

        EdGetFadeColor(fade.data());
        MGSetBGColor(fade.data());
        static_cast<CDngStatusData *>(UserStatus)->LostGateKey();

        if (BtEventInfo.no_status_recover == 0) {
            CUserStatus *status = UserStatus;

            for (int i = 0; i < 6; i++) {
                status->hp[i] = status->max_hp[i];
                status->ailments[i] = 0;
                status->ailment_frames[i] = 0;
            }

            status->res_limit_zone_current = RES_LIMIT_ZONE_NONE;
        }

        UserStatus->cur_chara = CHARA_TOAN;

        CUserStatus *leaving = UserStatus;

        leaving->prev_floor = -1;
        return existFlag;
    }

    // In debug mode, Start pressed while Select is held leaves the dungeon.
    if (DebugMode != 0 && GamePad.On(PAD_SELECT) != 0 && GamePad.Down(PAD_START) != 0) {
        while (ReadBGSync() == 1) {
        }

        SndExit();

        CUserStatus *leaving = UserStatus;

        leaving->prev_floor = -1;
        return 1;
    }

    if (tryalExit != 0) {
        while (ReadBGSync() == 1) {
        }

        SndExit();

        CUserStatus *leaving = UserStatus;

        leaving->prev_floor = -1;
        return 1;
    }

    SndStep();
    return 0;
}

void Draw_MainUnitShadow() {
    float light[4][4];
    float colour[4][4];
    float saved[4][4];

    // The shadows are drawn under a dimmed light and the real one goes back
    // afterwards.
    MGGetPLight(light, colour);
    sceVu0CopyMatrix(saved, light);
    light[0][0] *= 0.2f;
    light[2][0] *= 0.2f;
    MGSetPLight(light, colour);

    TexManager.ReloadTexture(Vif1Packet, 15);
    MGBeginDrawShadow(std::bit_cast<sceGsTex0>(TexManager.GetTexture("shadow_buf", -1)->tex0));

    if (BtActStatus.shadow_visible != 0 && EdEventInfo.player_shadow_draw != 0 && EdEventInfo.player_draw != 0 && BtActStatus.player_visible != 0) {
        CharaMain.DrawShadow();
    }

    if (CMonUnitHyde == 0 && BtEventMode == 0) {
        NowMonstorUnit->DrawShadowMonstor();
    }

    if (BtEventMode != 0) {
        for (int i = 0; i < 6; i++) {
            if (EdEventInfo.npc_shadow_draw[i] != 0 && EdEventInfo.npc_draw[i] != 0) {
                NPCUnit[i].DrawShadow();
            }
        }
    }

    MGEndDrawShadow(0x40);
    sceVu0CopyMatrix(light, saved);
    MGSetPLight(light, colour);
}

void Draw_MainUnit() {
    float         light[4][4];
    float         colour[4][4];
    float         saved_light[4][4];
    float         saved_colour[4][4];
    sceVu0FVECTOR up;
    sceVu0FVECTOR stood;
    sceVu0FVECTOR towards;
    sceVu0FVECTOR world;
    sceVu0FMATRIX place;
    sceVu0FVECTOR ambient;
    sceVu0FVECTOR saved_ambient;

    if (BtActStatus.player_visible == 0 && EdEventInfo.player_draw != 0 && EdEventInfo.player_shadow_draw != 0) {
        return;
    }

    MGGetPLight(light, colour);
    sceVu0CopyMatrix(saved_light, light);
    sceVu0CopyMatrix(saved_colour, colour);

    // The Atlamillia lights the player from wherever its effect stands.
    if (atraGetStatus != 0) {
        CFrame *lamp = shortAtraEffect.frame->SearchFrame("light01");

        sceVu0CopyMatrix(place, lamp->local);
        up[0] = place[3][0];
        up[1] = place[3][1];
        up[2] = place[3][2];
        lamp->GetWorldPosition(world, up);
        sceVu0CopyVector(stood, CharaFrame->position);

        towards[0] = world[0] - stood[0];
        towards[1] = world[1] - stood[1] - 12.0f;
        towards[2] = world[2] - stood[2];

        float dist = sqrt(towards[0] * towards[0] + towards[1] * towards[1] + towards[2] * towards[2]);

        dist = std::max(dist, 10.0f);

        float power = 5.0f * (100.0f / (dist * dist));

        sceVu0Normalize(towards, towards);
        towards[0] *= power;
        towards[1] *= power;
        towards[2] *= power;
        towards[3] = 0.0f;

        light[0][3] = towards[0];
        light[1][3] = towards[1];
        light[2][3] = towards[2];
        light[3][3] = 0.0f;

        colour[3][0] = 0.8359375f * atraGetStatusRate__2;
        colour[3][1] = 0.9765625f * atraGetStatusRate__2;
        colour[3][2] = 0.66015625f * atraGetStatusRate__2;
        colour[3][3] = 0.5f * atraGetStatusRate__2;
    }

    MGSetPLight(light, colour);
    MGGetAmbient(saved_ambient);
    MGGetAmbient(ambient);

    if (BtEventMode == 0 && BtActStatus.in_presentation == 0) {
        // A speed boost pulses the model blue.
        if (BtBySpeedFlag != 0) {
            static float rate = 0.0f;

            if (rate >= PI) {
                rate -= PI;
            } else {
                rate += 0.09817477f;
            }

            ambient[0] = 96.0f;
            ambient[1] = 96.0f;
            ambient[2] = 96.0f + 96.0f * sinf(rate);
        }

        if (BtStatusErrColorSet() != 0) {
            ambient[0] = StatusColor[0];
            ambient[1] = StatusColor[1];
            ambient[2] = StatusColor[2];
        }

        float damage = setUnitDamageColor(0);

        if (damage != 128.0f) {
            ambient[0] = damage;
            ambient[1] = damage;
            ambient[2] = damage;
            ambient[3] = 128.0f;
        }

        unitAmbientAnime(ambient);
    }

    MGSetAmbient(ambient);
    TexManager.ReloadTexture(Vif1Packet, 0x11);
    CharaMain.TextureAnime(0x11);
    CharaMain.Draw();

    if (NowWeapon != nullptr && NowWeapon->frame != nullptr && BtActStatus.weapon_visible != 0) {
        TexManager.ReloadTexture(Vif1Packet, 0x1D);
        NowWeapon->Draw();
    }

    MGSetAmbient(saved_ambient);
    sceVu0CopyMatrix(light, saved_light);
    sceVu0CopyMatrix(colour, saved_colour);
    MGSetPLight(light, colour);
}

// The renderer orders the frame grab itself, so DunMainDraw neither starts a VU1 program nor waits for
// GIF path idle.

namespace {

// Draws the floor's monsters. CMonstorUnit::DrawMonstor draws the ones taking part; a dormant one
// within video.detail_distance of the player is passed off as taking part for the length of the
// call, so that its model draws and animates without the monster waking.
void DrawMonstorDetail() {
    CMonstorUnit *unit = NowMonstorUnit;
    float         detail = ConfigDetailDistance();
    auto          dormant = std::make_unique<bool[]>(16);

    for (int i = 0; i < 16; i++) {
        MONSTOR *monster = &unit->monster[i];
        dormant[i] = monster->state == 1 && monster->revealed != 0 && monster->player_distance < detail;

        if (dormant[i]) {
            monster->state = 2;
        }
    }

    unit->DrawMonstor();

    for (int i = 0; i < 16; i++) {
        if (dormant[i]) {
            unit->monster[i].state = 1;
        }
    }
}

// Where the status panel's pieces go in a window wider or taller than the frame: life, weapon and
// water (left of the quick items at 0x124) to the left edge, the floor plate (from 0x1FC) to the
// right edge, the quick items between them stay centred; the panel to the top edge, the weapon's
// portrait and gauge to the bottom edge.
constexpr gfx::UiAnchor kStatusAnchor = {.left_until = 0x124, .right_from = 0x1FC, .top_until = 240.0f, .bottom_from = 240.0f};
// The mini map hangs under the floor plate.
constexpr gfx::UiAnchor kMiniMapAnchor = gfx::UiAnchor::Side(1, -1);

CTexture *NamedTexture(const char *name) {
    return TexManager.GetTexture(const_cast<char *>(name), -1);
}

} // namespace

void DunMainDraw() {
    sceVu0FMATRIX camera;
    sceVu0FVECTOR eye;
    sceVu0FMATRIX view;
    sceVu0FMATRIX unit;
    sceGsTex0     frame_tex;
    sceGsTex0     water_tex;

    EdEventInfo.main_texture_animation = nullptr;
    NowCamera__3->GetPos(eye);
    NowCamera__3->GetCameraMatrix(camera);
    sceVu0UnitMatrix(unit);
    sceVu0MulMatrix(view, unit, camera);
    MGSetViewMatrix(view, eye);
    SndSetCamera(NowCamera__3);

    if (BtAllDrawFlag == 0) {
        if (BtItemListCashFlag != 0) {
            TexManager.ReloadTexture(Vif1Packet, 0x28);
            DngActiveItemTextureCopy();
            DngActiveWeaponTextureCopy();
            BtItemListCashFlag = 0;
        }

        EdFadeInOut();
        return;
    }

    if (lightingMode == 0) {
        MGSetPLight(main_light, main_lightcolor);
        MGSetAmbient(main_ambientlight);
        MGSetBGColor(main_bgColor[0], main_bgColor[1], main_bgColor[2], 128.0f);
        MGSetFogParm(main_fogRate[0], main_fogRate[1], main_fogColor[0], main_fogColor[1], main_fogColor[2], main_fogRate[2], main_fogRate[3]);
    } else {
        MGSetPLight(sub_light, sub_lightcolor);
        MGSetAmbient(sub_ambientlight);
        MGSetBGColor(sub_bgColor[0], sub_bgColor[1], sub_bgColor[2], 128.0f);
        MGSetFogParm(sub_fogRate[0], sub_fogRate[1], sub_fogColor[0], sub_fogColor[1], sub_fogColor[2], sub_fogRate[2], sub_fogRate[3]);
    }

    if (BtItemListCashFlag != 0) {
        TexManager.ReloadTexture(Vif1Packet, 0x28);
        DngActiveItemTextureCopy();
        DngActiveWeaponTextureCopy();
        BtItemListCashFlag = 0;
    }

    TexManager.ReloadTexture(Vif1Packet, 3);
    BtTexAnime.TexAnime(3);
    NowDngMap->DrawBGModel(NowCamera__3);
    NowDngMap->DrawDummyModel(NowCamera__3);

    if (NowDngMap->map_type == 1) {
        NowDngMap->DrawMap(NowCamera__3, CharaFrame);
    } else {
        NowDngMap->DrawMapFreeStyle();
    }

    // Deliberately non-virtual: the map field's own draw, not an override.
    NowDranMapField->CDranMapField::Draw();

    if (itemOpenSmallFlag == 0 && itemOpenBigFlag == 0) {
        sceVu0FVECTOR box_pos;

        sceVu0CopyVector(box_pos, CharaFrame->position);
        NowDngMap->DrawItemBox(box_pos);
    } else {
        if (itemOpenSmallFlag != 0) {
            itemOpenSmall.Draw();
        }

        if (itemOpenBigFlag != 0) {
            itemOpenBig.Draw();
        }
    }

    if (BtEventMode != 0) {
        sceVu0FVECTOR focus = {200.0f, 500.0f, 0.0f, 0.0f};

        DepthOfField(focus, 3, 0x40, 0);
    }

    if (atraGetStatus == 0) {
        sceVu0FVECTOR atra_pos;

        TexManager.ReloadTexture(Vif1Packet, 0x16);
        sceVu0CopyVector(atra_pos, CharaFrame->position);
        NowDngMap->DrawAtraBoll(atra_pos);
    }

    Draw_MainUnitShadow();

    if (BtActStatus.player_visible != 0 && EdEventInfo.player_draw != 0) {
        Draw_MainUnit();
    }

    if (CMonUnitHyde == 0 && BtEventMode == 0) {
        DrawMonstorDetail();
    }

    if (CharaMainHandViewFlag != 0) {
        TexManager.ReloadTexture(Vif1Packet, 0x11);

        if (UserStatus->cur_chara == CHARA_XIAO) {
            CharaHand.Draw();
        }

        if (NowWeapon != nullptr && UserStatus->cur_chara == CHARA_XIAO && NowWeapon->frame != nullptr && BtActStatus.weapon_visible != 0) {
            TexManager.ReloadTexture(Vif1Packet, 0x1D);
            NowWeapon->Draw();
        }
    }

    if (BtEventMode != 0) {
        for (int i = 0; i < 6; i++) {
            if (EdEventInfo.npc_draw[i] != 0) {
                TexManager.ReloadTexture(Vif1Packet, i + 0x20);
                NPCUnit[i].TextureAnime(NPCUnit[i].texture_block);
                NPCUnit[i].Draw();

                if (i == BtEventInfo.bee_npc) {
                    DrawBee(NPCUnit[BtEventInfo.bee_npc].frame, 0xF);
                }
            }
        }
    }

    NowDngMap->DrawNPCDraw();
    mainItemModel.Draw();
    TexManager.ReloadTexture(Vif1Packet, 6);
    DrawWaterLing();

    if (Water_Splash_actFlag != 0) {
        Water_Splash.Draw();
    }

    if (NowDngMap->map_type == 1) {
        CRect_i_      area;
        sceVu0FVECTOR water_pos;

        TexManager.ReloadTexture(Vif1Packet, 0xD);
        MGGetFBuffTex(&frame_tex);
        area.x = 0;
        area.y = 0;
        area.width = 0x280;
        area.height = SCREEN_HALF_HEIGHT;
        water_tex = std::bit_cast<sceGsTex0>(NamedTexture("water")->tex0);
        MGMoveImage(&frame_tex, area, &water_tex, 0, 0, 0);
        sceVu0CopyVector(water_pos, CharaFrame->position);
        NowDngMap->DrawWater(water_pos, driveStepHold);
    }

    if (CEffectHyde == 0 && BtEventMode == 0) {
        TexManager.ReloadTexture(Vif1Packet, 0x13);
        NowShockWave->Draw(NowCamera__3);

        for (CItemBombEffect &bomb : CBomb__2) {
            bomb.Draw(NowCamera__3);
        }

        for (int i = 0; i < 5; i++) {
            MasekiEffect[i].Draw();
        }

        CSHOT_EFFECT *shot = NowShotEffect->effect;

        for (int i = 0; i < 5; i++) {
            shot[i].Draw();
        }

        NowMainEffect->Draw();
    }

    sceVu0FVECTOR raster_pos;

    sceVu0CopyVector(raster_pos, CharaFrame->position);
    TexManager.ReloadTexture(Vif1Packet, 0xE);
    NowDngMap->DrawRaster(CharaFrame);

    if (NowDngMap->map_type == 1) {
        NowDngMap->DrawFire(CharaFrame, NowCamera__3);
    } else {
        NowDngMap->DrawFireFreeStyle(CharaFrame, NowCamera__3);
    }

    if (atraGetStatus != 0 && atraShortGetType != 0) {
        TexManager.ReloadTexture(Vif1Packet, 0x1C);
        shortAtraEffect.Draw();
    }

    if (gateItemFlag != 0) {
        TexManager.ReloadTexture(Vif1Packet, 0x1C);
        MGDraw(itemBoxModel);
    }

    if (itemOpenSmallFlag != 0) {
        sceVu0FVECTOR ambient;
        sceVu0FVECTOR lift;
        sceVu0FVECTOR held;

        MGGetAmbient(ambient);
        TexManager.ReloadTexture(Vif1Packet, 0x16);
        itemOpenSmallFx.Draw();
        TexManager.ReloadTexture(Vif1Packet, 0x1C);
        sceVu0CopyVector(lift, itemBoxModel->position);
        sceVu0CopyVector(held, lift);

        static float itemposr = -PI;

        itemposr += 0.10471976f;

        if (itemposr >= PI) {
            itemposr -= TWO_PI;
        }

        lift[1] += 0.5f * sinf(itemposr);
        itemBoxModel->SetPosition(lift);
        MGDraw(itemBoxModel);
        MGSetAmbient(ambient);
        itemBoxModel->SetPosition(held);
    }

    if (itemOpenBigFlag != 0) {
        sceVu0FVECTOR lift;
        sceVu0FVECTOR held;

        TexManager.ReloadTexture(Vif1Packet, 0x16);
        MGDraw(itemOpenBigFx.frame);
        sceVu0CopyVector(lift, itemBoxModel->position);
        sceVu0CopyVector(held, lift);

        static float itemposr = -PI;

        itemposr += 0.10471976f;

        if (itemposr >= PI) {
            itemposr -= TWO_PI;
        }

        lift[1] += 0.03f * sinf(itemposr);
        itemBoxModel->SetPosition(lift);
        TexManager.ReloadTexture(Vif1Packet, 0x1C);
        MGDraw(itemBoxModel);
    }

    if (NewChangeFxFlag != 0) {
        sceVu0FVECTOR pos;
        sceVu0FVECTOR rot;

        TexManager.ReloadTexture(Vif1Packet, 0xB);
        sceVu0CopyVector(pos, CharaMain.pos);
        CharaMain.GetRotation(rot);
        NewChangeFx.SetPosition(pos);
        NewChangeFx.SetRotation(rot);
        NewChangeFx.Draw();
    }

    if (EscapeFlag != 0) {
        TexManager.ReloadTexture(Vif1Packet, 0x1C);
        EscapeEffect.Draw();
    }

    NowDngMap->DrawTrapCircle();
    TexManager.ReloadTexture(Vif1Packet, 1);

    if (CMonUnitHyde == 0 && BtEventMode == 0) {
        NowMonstorUnit->DrawMonstorCursor();
    }

    if (BtEventInfo.event_marker != 0) {
        sceVu0FVECTOR mark;

        sceVu0CopyVector(mark, CharaMain.pos);
        mark[1] += 22.0f;

        static float bic_posr = -PI_SHORT;

        bic_posr += 0.104719736f;

        if (bic_posr >= PI_SHORT) {
            bic_posr -= TWO_PI_SHORT;
        }

        mark[1] += 0.5f * sinf(bic_posr);

        mark[1] += CharaHeight(UserStatus) - 15.0f;
        bicCursorFrame->SetPosition(mark);
        MGDraw(bicCursorFrame);
        BtEventInfo.event_marker = false;
    }

    if (BtAllClear == 0 && DebugStatus[10] == 0) {
        setbilinear(0);

        if (autoDemo == 0 && CMonUnitHyde == 0) {
            setTargetCursor(lockOnTargetFlag);

            if (lockOnTargetDraw != 0) {
                MGDraw(cursorFrame);
                lockOnTargetDraw = 0;
            }

            setbilinear(0);
        }

        if (iventInfo != DNG_EVENT_NONE && iventMarker != 0) {
            sceVu0CopyVector(iventPos, CharaMain.pos);
            iventPos[1] += 22.0f;

            static float bic_posr = -PI_SHORT;

            bic_posr += 0.104719736f;

            if (bic_posr >= PI_SHORT) {
                bic_posr -= TWO_PI_SHORT;
            }

            iventPos[1] += 0.5f * sinf(bic_posr);

            iventPos[1] += CharaHeight(UserStatus) - 15.0f;
            bicCursorFrame->SetPosition(iventPos);
        }

        if (BombInfo.aiming != 0) {
            bombCursorFrame->SetPosition(BombInfo.pos);
            MGDraw(bombCursorFrame);
        }

        if (CEffectHyde == 0 && BtEventMode == 0) {
            MGSetGsZBUF(nullptr);
            RandomItem->Draw();
            StealItem.Draw();
        }

        if (autoDemo == 0) {
            sceGsAlpha blend;
            sceGsZbuf  depth;

            TexManager.ReloadTexture(Vif1Packet, 0x12);

            blend = mgAlpha;
            blend.bits.a = 0;
            blend.bits.b = 2;
            blend.bits.c = 0;
            blend.bits.d = 1;
            MGSetGsALPHA(&blend);

            depth = mgZBuffer;
            depth.bits.zmsk = 1;
            MGSetGsZBUF(&depth);

            for (CWeaponElement &element_fx : CWeaponElFx) {
                element_fx.Draw();
            }

            WeaponCrashEffect.Draw();

            for (int i = 0; i < 16; i++) {
                HitMark[i].Draw();
                HitPointMark[i].Draw();
                MyHitPointMark[i].Draw();
            }

            HealEffect.Draw();

            if (CEffectHyde == 0 && BtEventMode == 0) {
                NowShotData->draw();
                OzumondShotEffect.Draw();
                OzumondFire.Draw();
            }

            CWeaponFx.Draw();
            CRunFx__2.Step();
            CRunFx__2.Draw();
            MGSetGsALPHA(nullptr);
            MGSetGsZBUF(nullptr);
        }

        if (rogoSwitch2 == 1 && BtEventInfo.floor_title_off == 0) {
            TEX_Floor1 = TexManager.GetTexture(floor_name, -1);
            TexManager.ReloadTexture(Vif1Packet, 8);
            StartMessageDraw(TEX_Floor1, selectMapNo, UserStatus->cur_floor, BtUraDongeon, rogoAlphaA[2]);
        }

        TexManager.ReloadTexture(Vif1Packet, 2);

        gfx::SetUiAnchor(kStatusAnchor);

        int gauge_alpha = rogoY3 + 0x60;

        if (static_cast<int>(BtActStatus.action_gauge) >= 100) {
            if (BtActStatus.gauge_flash == 0) {
                set2DSprite(Vif1Packet, TEX_WepGage, CRect_i_(0x28, SCREEN_HEIGHT - 0x34, 0x80, 0x1C), CRect_i_(0, 0, 0x80, 0x1C), gauge_alpha);
            } else {
                set2DSprite(Vif1Packet, TEX_WepGage, CRect_i_(0x28, SCREEN_HEIGHT - 0x34, 0x80, 0x1C), CRect_i_(0, 0x1C, 0x80, 0x1C), gauge_alpha);
            }
        }

        float wear = BtActStatus.action_gauge;

        if (static_cast<int>(wear) < 100) {
            int left = static_cast<int>(wear);

            set2DSprite(Vif1Packet, TEX_WepGage, CRect_i_(0x28, SCREEN_HEIGHT - 0x34, 0x80, 0x1C), CRect_i_(0, 0x54, 0x80, 0x1C), gauge_alpha);
            set2DSprite(Vif1Packet, TEX_WepGage, CRect_i_(0x42, SCREEN_HEIGHT - 0x34, left, 0x1C), CRect_i_(0x1A, 0x38, left, 0x1C), gauge_alpha);
        }

        if (infoMap != 0 && NowDngMap->map_type == 1) {
            sceVu0FVECTOR map_pos;
            sceVu0FVECTOR map_rot;

            gfx::UiAnchorScope map_anchor(kMiniMapAnchor);

            TexManager.ReloadTexture(Vif1Packet, 0x1F);
            sceVu0CopyVector(map_pos, CharaFrame->position);
            CharaFrame->GetRotation(map_rot);
            NowDngMap->DrawMiniMap(map_pos, map_rot[1]);
            NowMonstorUnit->DrawMapSymbol(map_pos);
            RandomItem->MapSymbolDraw();
        }

        topStatusInfo(rogoY3, itemNowSel, UserStatus->cur_floor);
        BtStatusErrDraw(rogoY3);
        gfx::SetUiAnchor({});

        for (CHitValue &value : HitValue) {
            value.Draw();
        }

        if (exitMenuFlag != 0) {
            TexManager.ReloadTexture(Vif1Packet, 7);
            set2DSprite(Vif1Packet, NamedTexture("pause"), CRect_i_(0x100, 0xCC, 0x80, 0x28), 0, 0);
        }

        setbilinear(1);
    }

    if (MonstorNameOff == 0) {
        MonsterNameDraw();
    }

    SetMonsterNameDrawFlag(0);

    if (EdEventInfo.screen_filter != 0) {
        sceVu0FVECTOR fade;

        sceVu0CopyVector(fade, EdEventInfo.screen_filter_color);
        MGFillBox(CRect_i_(0, 0, 0x2800, 0xE00), static_cast<int>(fade[0]), static_cast<int>(fade[1]), static_cast<int>(fade[2]), static_cast<int>(fade[3]));
    }

    if (BtEventMode != 0) {
        EdEventSpriteDraw();
        EBDraw();
        TexManager.ReloadTexture(Vif1Packet, 0x1A);
        BtEventMes0.DrawMesWin();
        BtEventMes1.DrawMesWin();
    }

    if (EdEventInfo.item_frame[0] != nullptr) {
        TexManager.ReloadTexture(Vif1Packet, 0x28);
        MGDraw(EdEventInfo.item_frame[0]);
    }

    setbilinear(0);
    TexManager.ReloadTexture(Vif1Packet, 0x1A);
    SystemMesStep();
    SystemMesDraw();

    if (DngMessMan.hide_timer <= 0 && DngMessMan.enabled != 0) {
        if (DngMessMan.steev_window == 0) {
            int mes_no = DngMessMan.message;

            DngMes1.mes_no[0] = DngMessMan.insert_mes_1;
            DngMes1.mes_no[1] = DngMessMan.insert_mes_2;
            DngMes1.values[0] = DngMessMan.insert_value_1;
            DngMes1.values[1] = DngMessMan.insert_value_2;
            DngMes1.value_signed = true;
            DngMes1.value_show = false;

            if (mes_no != oldMsgNo) {
                oldMsgNo = mes_no;
                Mes1MakeFlg = 1;
            }

            if (Mes1MakeFlg != 0) {
                Mes1MakeFlg = DngMes1.MakeMesWin(mes_no);
            }

            if (mes_no != -1) {
                DngMes1.text_x = 0x136;
                DngMes1.text_y = 0x154;
                DngMes1.auto_pos = MES_POS_BOTTOM_RIGHT;
                DngMes1.DrawMesWin();
            }
        } else {
            int mes_no = DngMessMan.message;
            int pos[4];

            DngMesStb.value_signed = true;
            DngMesStb.value_show = false;

            if (mes_no != oldMsgNo) {
                oldMsgNo = mes_no;
                Mes1MakeFlg = 1;
            }

            if (Mes1MakeFlg != 0) {
                Mes1MakeFlg = DngMesStb.MakeMesWin(mes_no);
            }

            if (mes_no != -1) {
                DngMesStb.auto_pos = MES_POS_BOTTOM;
                DngMesStb.AutoSet(pos);
                DngMesStb.DrawMesWin();
            }
        }
    }

    if (BtEventMode != 0) {
        if (DebugStatus[4] != 0) {
            TexManager.ReloadTexture(Vif1Packet, 0xC);
            EdDDrawFont();
        }
    } else {
        TexManager.ReloadTexture(Vif1Packet, 0xC);
        DebugInfomationDraw();
    }

    DispFade__3.FadeIn(Vif1Packet);
    DispFade__3.FadeOut(Vif1Packet);
    EdFadeInOut();

    if (frameCaputer != 0) {
        TexManager.ReloadTexture(Vif1Packet, 0x17);
        MGMoveFrameBuffImage(reinterpret_cast<sceGsTex0 *>(&NamedTexture("frame_image")->tex0), 0, 0, 0);
    }
}

/* The camera height and angle DunMoveChara saves to restore later. */
float oldCameraHeight;
float oldCameraAngle;

/* How hard the player is turning the camera. */
float inputH1;

void motionDrive() {
    sceVu0FVECTOR pos;
    sceVu0FVECTOR rotation;
    sceVu0FVECTOR hand;
    int           chara = UserStatus->cur_chara;

    if (BtAllDrawFlag == 0) {
        return;
    }

    if (BtEventMode == 0) {
        NowMonstorUnit->Step(driveStepHold | CMonUnitHold);
    }

    if (BtActStatus.player_visible != 0 && EdEventInfo.player_draw != 0) {
        sceVu0CopyVector(pos, CharaFrame->position);
        CharaFrame->GetRotation(rotation);
        CharaMain.SetPosition(pos);
        CharaMain.SetRotation(rotation[0], rotation[1], rotation[2]);

        int blend = 0;

        if (driveNoInterpolate != 0) {
            blend = 4;
            driveNoInterpolate = 0;
        }

        // A held step, an event or a status ailment restarts the motion
        // outright; otherwise it only changes where it differs.
        if (driveStepHold != 0 || EdEventInfo.player_stop != 0 || BtActStatus.movement_locked != 0) {
            CharaMain.motion_no = BtActStatus.motion_no;
            CharaMain.motion_flags = 1;
            CharaMain.motion_speed = -1.0f;

            if (chara == CHARA_XIAO) {
                CCharacter *weapon = NowWeapon;

                weapon->motion_no = BtActStatus.motion_no;
                weapon->motion_flags = 1;
                weapon->motion_speed = -1.0f;
            }

            BtActStatus.main_motion = -1;
        } else if (BtActStatus.main_motion != BtActStatus.motion_no) {
            CharaMain.motion_no = BtActStatus.motion_no;
            CharaMain.motion_flags = blend;
            CharaMain.motion_speed = -1.0f;

            if (chara == CHARA_XIAO) {
                CCharacter *weapon = NowWeapon;

                weapon->motion_no = BtActStatus.motion_no;
                weapon->motion_flags = blend;
                weapon->motion_speed = -1.0f;
            }

            BtActStatus.main_motion = BtActStatus.motion_no;
        }

        // Osmond's tail keeps turning on its own.
        if (UserStatus->cur_chara == CHARA_OSMOND) {
            CFrame *tail = CharaMain.frame->SearchFrame("tukene1");
            CFrame *tail_shadow = CharaMain.shadow_frame->SearchFrame("obj46");

            if (tail != nullptr) {
                static float y = 0.0f;

                y += 0.35699910f;

                if (!(y < PI_SHORT)) {
                    y = y - TWO_PI_SHORT;
                }

                tail->SetRotType(2);
                tail->SetRotation(0.0f, AngleLimit(y), 0.0f);

                if (tail_shadow != nullptr) {
                    tail_shadow->SetRotType(2);
                    tail_shadow->SetRotation(0.0f, AngleLimit(y), 0.0f);
                }
            }
        }

        if (lockOnTargetFlag != 0 && UserStatus->cur_chara == CHARA_XIAO && CharaMainHandViewFlag == 0) {
            CharaMain.motion_type.state.look_frame = static_cast<CFrameVu1 *>(CharaFrame->SearchFrame("jnt2_1"));
            sceVu0CopyVector(CharaMain.motion_type.state.look_position, BtActStatus.target_position);
            CharaMain.motion_type.state.look_at = true;
            CharaMain.motion_type.state.look_constraint = FRAME_CONSTRAINT_X;
        } else {
            CharaMain.motion_type.state.look_at = false;
        }

        CharaMain.Step();

        if (chara == CHARA_XIAO) {
            NowWeapon->Step();
        }

        if (BtActStatus.shadow_visible != 0) {
            CharaMain.ShadowStep();
        }
    }

    if ((CharaMainHandViewFlag != 0 && UserStatus->cur_chara == CHARA_XIAO) || UserStatus->cur_chara == CHARA_RUBY) {
        sceVu0CopyVector(hand, CharaFrame->position);

        float height = CharaHeight(UserStatus);

        hand[1] += height - 3.0f;
        CharaHand.SetPosition(hand);

        int blend = 0;

        if (driveNoInterpolate != 0) {
            blend = 4;
            driveNoInterpolate = 0;
        }

        if (driveStepHold != 0 || BtActStatus.movement_locked != 0) {
            CharaHand.motion_no = BtActStatus.motion_no;
            CharaHand.motion_flags = 1;
            CharaHand.motion_speed = -1.0f;

            if (chara == CHARA_XIAO) {
                CCharacter *weapon = NowWeapon;

                weapon->motion_no = BtActStatus.motion_no;
                weapon->motion_flags = 1;
                weapon->motion_speed = -1.0f;
            }

            BtActStatus.hand_motion = -1;
        } else if (BtActStatus.hand_motion != BtActStatus.motion_no) {
            CharaHand.motion_no = BtActStatus.motion_no;
            CharaHand.motion_flags = blend;
            CharaHand.motion_speed = -1.0f;

            if (chara == CHARA_XIAO) {
                CCharacter *weapon = NowWeapon;

                weapon->motion_no = BtActStatus.motion_no;
                weapon->motion_flags = blend;
                weapon->motion_speed = -1.0f;
            }

            BtActStatus.hand_motion = BtActStatus.motion_no;
        }

        if (chara == CHARA_XIAO) {
            NowWeapon->Step();
        }

        CharaHand.Step();
    }

    if (BtEventMode != 0) {
        for (int i = 0; i < 6; i++) {
            if (EdEventInfo.npc_stop[i] == 0) {
                NPCUnit[i].ShadowStep();
                NPCUnit[i].Step();
            }
        }
    }

    if (driveStepHold == 1) {
        return;
    }

    if (BtActStatus.event_block_frames > 0) {
        BtActStatus.event_block_frames--;
    }

    // A weapon that is nearly broken keeps saying so.
    if (BtEventMode == 0) {
        CUserStatus *status = UserStatus;
        s8           owner = status->cur_chara;
        WEAPON_HAVE *weapon = &status->chara_weapons[owner][status->equipped_weapon_slot[owner]];

        if (weapon->durability_f <= 10.0f && weapon->item_no != defWeapon__6[owner]) {
            static int warning_cnt = 0;
            auto       wait = static_cast<int>(weapon->durability_f) * 10;

            if (wait < 11) {
                wait = 10;
            }

            if (warning_cnt <= 0) {
                warning_cnt = wait;
                SndSePlay(SE_WARNING, -1, 0);
            } else {
                warning_cnt--;
            }
        }
    }

    if (BtActStatus.player_visible != 0 && EdEventInfo.player_draw != 0) {
        CharaMain.ClothStep(0);
    }

    if (BtActStatus.camera_hold == 0) {
        NowCamera__3->Step(1);
    }

    if (Water_Splash_actFlag != 0) {
        Water_Splash.Step();

        auto end = static_cast<float>(Water_Splash.motion_type.motion_info[0].end);

        if (Water_Splash.motion_type.state.time >= end - 0.5f && Water_Splash.motion_type.state.time < end) {
            Water_Splash_actFlag = 0;
        }
    }

    StepWaterLing();
    NowDngMap->StepTrapCircle();
    Trap_Circle.Step();

    if (EscapeFlag != 0) {
        EscapeEffect.Step();
    }

    ItemVolumeStep.LoopStep(60);
    NowDngMap->StepNPC();
    NowDranMapField->CDranMapField::Step();
    DngMesStb.Step();

    if (DngMessMan.timer > 0) {
        DngMessMan.timer--;

        if (DngMessMan.timer == 0) {
            DngMessMan.steev_window = false;
            DngMessMan.message = -1;
        }
    }

    if (DngMessMan.hide_timer > 0) {
        DngMessMan.hide_timer--;
    } else {
        DngMessMan.hide_timer = 0;
    }

    // Every hit that is still in the air counts down and drifts.
    CCollisionData *collision = NowColData;

    for (int i = 0; i < 96; i++) {
        if (collision->active[i] == 0) {
            continue;
        }

        auto &hit = collision->hit[i];

        if (hit.phase > 0) {
            hit.phase--;
        } else {
            hit.phase = hit.ready_phase;
        }

        hit.life--;

        if (hit.life <= 0) {
            collision->active[i] = 0;
            hit.scale_rate = 0.0f;
            continue;
        }

        hit.radius += hit.unk_40;

        if (hit.scale_rate != 0.0f) {
            sceVu0ScaleVectorXYZ(hit.base_point, hit.base_point, hit.scale_rate);
        }
    }

    if (StatusErrCheck(AILMENT_FREEZE) != 0 || StatusErrCheck(AILMENT_STAMINA) != 0) {
        CUserStatus *status = UserStatus;
        s8           owner = status->cur_chara;

        if (status->ailment_frames[owner] > 0) {
            status->ailment_frames[owner]--;

            if (status->ailment_frames[owner] == 0) {
                status->ailments[owner] = 0;
                BtActStatus.movement_locked = 0;
            }
        }
    }

    if (BtActStatus.special_cooldown > 0) {
        BtActStatus.special_cooldown--;
    }

    // The action gauge fills back up at a rate the weapon sets.
    if (static_cast<int>(BtActStatus.action_gauge) < 100) {
        if (BtActStatus.refill_frames < 10) {
            BtActStatus.refill_frames++;
        }

        auto gain = static_cast<float>(NowWeaponHave->speed) / 30.0f;

        if (StatusErrCheck(AILMENT_GOO) != 0) {
            gain /= 2.0f;
        }

        if (StatusErrCheck(AILMENT_STAMINA) != 0) {
            gain *= 2.0f;
        }

        gain = std::max(gain, 1.0f);

        s8 who = UserStatus->cur_chara;

        // Toan and Ungaga recharge slowly, and not at all on a frame they swing.
        if (who == CHARA_TOAN || who == CHARA_UNGAGA) {
            if (BtActStatus.swinging == 0) {
                gain *= 0.25f;
            } else {
                gain = 0.0f;
            }

            BtActStatus.swinging = 0;
        }

        if (who == CHARA_OSMOND) {
            if (BtActStatus.gun_type != 1) {
                if (BtActStatus.gauge_exhausted == 0) {
                    gain = 0.0f;
                } else {
                    gain *= 0.5f;
                }
            } else {
                gain *= 1.5f;
            }
        }

        if (who == CHARA_XIAO || who == CHARA_RUBY) {
            gain *= 1.5f;
        }

        BtActStatus.action_gauge += gain;

        if (!(static_cast<int>(BtActStatus.action_gauge) < 100)) {
            BtActStatus.action_gauge = 100.0f;
            BtActStatus.gauge_flash = 10;
            BtActStatus.gauge_exhausted = 0;
        }
    } else if (BtActStatus.gauge_flash > 0) {
        BtActStatus.gauge_flash--;
    }

    if (BtActStatus.hud_shake_frames > 0) {
        BtActStatus.hud_shake_y = static_cast<int>(16.0f * static_cast<float>(rand()) / 2147483648.0f) - 8;
        BtActStatus.hud_shake_frames--;
    }

    BtActStatus.frames_since_attack++;

    if (!(BtActStatus.frames_since_attack < 3600)) {
        BtActStatus.frames_since_attack = 3600;
    }

    if (UserStatus->step_disable == 0) {
        BtStatusErrStep();
    }

    CUserStatus *status = UserStatus;
    float        water_max = status->water_max[status->cur_chara];
    float        water_now = status->water_now[status->cur_chara];
    static int   heal_counter = 0;

    heal_counter++;

    if (!(heal_counter < 240)) {
        if ((NowWeaponHave->flags & 0x800) && status->hp[status->cur_chara] > 0) {
            status->AddNowLife(status->cur_chara, 1, 255.0f);
        }

        heal_counter = 0;
    }

    UserStatus->Step(0);
    float dry = UserStatus->water_now[UserStatus->cur_chara];

    DngMessMan.SetStatus_Dry(water_max, water_now, dry);
    BtStatusAlarmAnime();

    for (int i = 0; i < FrameObjAnimCnt; i++) {
        ObjAnimePlay(&FrameObjAnim[i]);
    }

    CDungeonMap *map = NowDngMap;

    map->fire.FireStep();
    map->fire.RasterStep();
    CWeaponFx.Step();

    for (auto &element : CWeaponElFx) {
        element.Step();
    }

    for (auto &value : HitValue) {
        value.Step();
    }

    mainItemModel.Step();
    WeaponCrashEffect.Step();

    for (int i = 0; i < 16; i++) {
        HitMark[i].Step();
        HitPointMark[i].Step();
        MyHitPointMark[i].Step();
    }

    if (NewChangeFxFlag != 0) {
        NewChangeFx.Step();

        auto end = static_cast<float>(NewChangeFx.motion_type.motion_info[0].end);

        if (NewChangeFx.motion_type.state.time >= end - 0.3f && NewChangeFx.motion_type.state.time < end) {
            NewChangeFxFlag = 0;
        }
    }

    if (atraShortGetType != 0 && atraGetStatus != 0) {
        shortAtraEffect.Step();
    }

    if (itemOpenSmallFlag == 1) {
        itemOpenSmall.Step();
        itemOpenSmallFx.Step();
    }

    if (itemOpenBigFlag == 1) {
        itemOpenBig.Step();
        itemOpenBigFx.Step();
    }

    if (CEffectHold != 0 || BtEventMode != 0) {
        return;
    }

    if (BtActStatus.camera_shake_frames > 0) {
        float shake = BtActStatus.camera_shake;

        BtActStatus.camera_shake_offset = 2.0f * (shake * static_cast<float>(rand())) / 2147483648.0f - shake;
        BtActStatus.camera_shake -= BtActStatus.camera_shake_decay;
        BtActStatus.camera_shake_frames--;
    } else {
        BtActStatus.camera_shake_offset = 0.0f;
    }

    if (BtActStatus.fast_camera_frames > 0) {
        BtActStatus.fast_camera_frames--;
    }

    int          picked_up = RandomItem->CheckPosition();
    CUserStatus *collector = UserStatus;
    int          total = collector->money + picked_up;

    if (!(total < 0xFFFF)) {
        collector->money = 0xFFFF;
    } else {
        collector->money = total;
    }

    RandomItem->Step();
    StealItem.Step();

    for (auto &bomb : CBomb__2) {
        bomb.Step();
    }

    for (auto &effect : MasekiEffect) {
        effect.Step();
    }

    NowShockWave->Step();

    CSHOT_EFFECT *effects = NowShotEffect->effect;

    for (int i = 0; i < 5; i++) {
        effects[i].Step();
    }

    OzumondShot.Step();
    OzumondFire.Step();
    OzumondShotEffect.Step();
    NowMainEffect->Step();
    NowShotData->step();
    HealEffect.Step();
}

void BtCleatRandomMap() {
    NowDngMap = &MainDungeonMap;
    NowEventMan = &DngEventMan;
    static_cast<CDngStatusData *>(UserStatus)->LostGateKey();

    ClearEventData(*NowEventMan);
    ClearMapObjects(*NowDngMap);
    ClearRandomItems(MainRandomItem, GoldTex);
    ClearRandomItems(SubRandomItem, GoldTex);
    RandomItem = &MainRandomItem;

    NowDngMap->buildRandomMap(6, 1);
    NowDngMap->buildEventData(UserStatus->cur_floor, NowDngMap->map_type, 1);
    NowDngMap->FlushCheckMask();
    NowDngMap->DrawMapCalc(NowDngMap->map_type);
    NowEventMan->SetupEvent(NowDngMap, NowDngMap->map_type);

    // The back floor starts as a copy of the front one and is rebuilt on its own.
    NowDngMap = &UraDungeonMap;
    NowEventMan = &UraEventMan;
    UraDungeonMap = MainDungeonMap;
    UraEventMan = DngEventMan;

    ClearEventData(UraEventMan);
    ClearMapObjects(UraDungeonMap);

    UraDungeonMap.buildRandomMap(6, 0);
    UraDungeonMap.buildEventData(UserStatus->cur_floor, UraDungeonMap.map_type, 0);
    UraDungeonMap.FlushCheckMask();
    UraDungeonMap.DrawMapCalc(UraDungeonMap.map_type);
    UraEventMan.SetupEvent(&UraDungeonMap, UraDungeonMap.map_type);

    NowDngMap = &MainDungeonMap;
    NowEventMan = &DngEventMan;
    UserStatus->ClearEventFlag();
    BtUraDongeon = 0;
    BtEquipMap = 0;
    BtEquipMasuisyou = 0;
}

void BtCleatFreeMap() {
    MainDungeonMap.buildEventData(UserStatus->cur_floor, MainDungeonMap.map_type, 1);
    MainDungeonMap.FlushCheckMask();
    MainDungeonMap.DrawMapCalc(MainDungeonMap.map_type);
    DngEventMan.SetupEvent(&MainDungeonMap, MainDungeonMap.map_type);

    ClearRandomItems(MainRandomItem, GoldTex);
    RandomItem = &MainRandomItem;

    UserStatus->ClearEventFlag();
    BtUraDongeon = 0;
    NowDngMap = &MainDungeonMap;
    NowEventMan = &DngEventMan;
    BtEquipMap = 0;
    BtEquipMasuisyou = 0;
}

void BtArrengeMonstor() {
    NowMonstorUnit->CleanViewMonstor(BtUraDongeon);
    NowMonstorUnit->ArrangementPos(&MainDungeonMap, 15, -1, 0);
}

void BtLoadMonstor(int ura) {
    MonstorModelBuffer.used = 0;

    CMonstorUnit *unit = NowMonstorUnit;

    unit->model_count = 0;
    unit->current_monster = 0;

    for (int slot = 0; slot < 16; slot++) {
        unit->script[slot] = &MonstorScriptBuffer[slot];
    }

    unit->requested_event = -1;
    unit->CleanViewMonstor(BtUraDongeon);
    BtSetEventExtendTable();

    for (auto &anime : MonsterTexAnim) {
        anime.Initialize();
    }

    TexManager.DeleteTextureBlock(0x2A);
    TexManager.DeleteTextureBlock(0x26);
    TexManager.CleanUpBuffer();
    TexManager.CleanUpTextureList();

    LoadFile("dun/etc/atrcol.mds", read_buffer, nullptr);
    wait_now_loading_vsync();
    NowMonstorUnit->collision = LoadCollisionFile(read_buffer, &MonstorModelBuffer);

    CSHOT_EFFECT *effects = NowShotEffect->effect;

    for (int i = 0; i < 5; i++) {
        effects[i].Initialize();
    }

    BT_ENEMY_FLOOR       *floors = ura == 0 ? BtEnemyLayoutList[selectMapNo] : BtUraEnemyLayoutList[selectMapNo];
    const BT_ENEMY_FLOOR &layout = floors[UserStatus->cur_floor];
    int                   count = 0;

    for (const auto &entry : layout.monster) {
        if (entry.monster_no == -1) {
            break;
        }

        count += NowMonstorUnit->SetupBaseModel(count, entry.monster_no, 0x26, &MonstorModelBuffer);
    }

    MemoryMapDump();
}

void EquipReAttach(CCharacter *equipment, int held_out) {
    CFrame *hand = held_out != 0 ? CharaHand.frame->SearchFrame("pati1") : CharaMain.frame->SearchFrame("weapon");

    equipment->SetPosition(0.0f, 0.0f, 0.0f);
    equipment->SetRotation(0.0f, 0.0f, 0.0f);
    equipment->frame->SetReference(hand);
}

void EquipWeaponFrame(CCharacter *weapon, int chara, int held_out) {
    NowWeapon = weapon;

    CFrame *hand = held_out != 0 && chara == CHARA_XIAO ? CharaHand.frame->SearchFrame("pati1") : CharaMain.frame->SearchFrame("weapon");

    if (hand == nullptr) {
        std::println("no weapon frame !!");
        return;
    }

    float zero = 0.0f;

    NowWeapon->SetPosition(zero, zero, zero);
    NowWeapon->SetRotation(zero, zero, zero);
    NowWeapon->frame->SetReference(hand);
    NowWeapon->Step();

    ClearWeaponTrail();
    CWeaponFx.InitSet(NowWeapon->frame, "dcol0", "dcol1");
    SetWeaponColor();
}

void LoadWeapon2(unsigned int *crash_data, unsigned int *default_data, unsigned int *main_data, int chara, int reload) {
    std::array<char, 64> weapon_name{};
    std::array<char, 64> weapon_model_path{};

    // A reload gives the weapon blocks back before the new models go in.
    if (reload != 0) {
        TexManager.DeleteTextureBlock(0x1D);
        TexManager.DeleteTextureBlock(0x10);
        TexManager.CleanUpBuffer();
        TexManager.CleanUpTextureList();
        WeaponModelBuffer.used = 0;
        WEffectModelBuffer.used = 0;
    }

    BtGetWeaponNamePath2(weapon_name.data(), weapon_model_path.data(), chara, 0);
    CrashWeapon.Initialize();
    CrashWeapon.LoadPackData3(crash_data, weapon_model_path.data(), &WeaponModelBuffer, 0x1D, &WeaponModelBuffer, 1, 0);

    BtGetWeaponNamePath2(weapon_name.data(), weapon_model_path.data(), chara, 1);
    DefaultWeapon.Initialize();
    DefaultWeapon.LoadPackData3(default_data, weapon_model_path.data(), &WeaponModelBuffer, 0x1D, &WeaponModelBuffer, 1, 0);

    CUserStatus *status = UserStatus;
    s8           owner = status->cur_chara;

    // The weapon's item number, counted from its character's first weapon.
    int model_no = status->chara_weapons[owner][status->equipped_weapon_slot[owner]].item_no;

    model_no -= defWeapon__6[chara];
    BtGetWeaponNamePath2(weapon_name.data(), weapon_model_path.data(), chara, model_no);
    MainWeapon.Initialize();
    MainWeapon.LoadPackData3(main_data, weapon_model_path.data(), &WeaponModelBuffer, 0x1D, &WeaponModelBuffer, 1, 0);
    EquipWeaponFrame(&MainWeapon, chara, CharaMainHandViewFlag);

    // The effect models take whatever the weapon models leave.
    WEffectModelBuffer.base = ArenaFree(WeaponModelBuffer);
    WEffectModelBuffer.limit = 0x33450 - CharaModelBuffer.used - WeaponModelBuffer.used;
    WEffectModelBuffer.used = 0;

    MenuWeaponSpSet(NowWeapon, &status->chara_weapons[owner][status->equipped_weapon_slot[owner]]);
    BtActStatus.action_gauge = 100.0f;
    BtActStatus.gauge_exhausted = 0;
}

void SwordDmgCheck1(float amount, int kind) {
    CUserStatus *status = UserStatus;
    s8           chara = status->cur_chara;
    WEAPON_HAVE *weapon = &status->chara_weapons[chara][status->equipped_weapon_slot[chara]];
    float        was = weapon->durability_f;
    auto         warn = static_cast<float>(weapon->durability * 0.2);

    int broke = BattleSubWeaponDmg(amount, kind);

    // The Chronicle sword says so once, as its durability crosses a fifth.
    if (weapon->item_no == ITEM_WEAPON_LAMB_S_SWORD && weapon->durability_f <= warn && was >= warn) {
        SetWeaponAttachStatus(NowWeaponHave);
        DngMessMan.message = 0xB8;
        DngMessMan.timer = 0xF0;
        DngMessMan.steev_window = false;
        SndSePlay(SE_POWER_UP, -1, 0);
        MenuWeaponSpSet(NowWeapon, weapon);
    }

    if (broke == 0) {
        return;
    }

    float         spread[4] = {0.0f, 1.0f, 0.0f, 1.0f};
    float         offset[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    sceVu0FVECTOR world;
    sceVu0FVECTOR pos;

    sceVu0CopyVector(pos, CharaMain.pos);

    CFrame *hand = CharaMain.frame->SearchFrame("weapon");

    if (hand != nullptr) {
        hand->GetWorldPosition(world, offset);
        WeaponCrashEffect.Set(world, spread, 4, 1.5f, 0.01f, 0.01f, 0.3f, 0x20, pos[1]);
    }

    // 1 leaves the broken weapon in hand, 2 the character's default one.
    if (broke == 1 || broke == 2) {
        EquipWeaponFrame(broke == 1 ? &CrashWeapon : &DefaultWeapon, UserStatus->cur_chara, CharaMainHandViewFlag);
        s8 owner = UserStatus->cur_chara;

        if (owner == CHARA_OSMOND || owner == CHARA_RUBY) {
            NowMainEffect = &CharaMainEffectCrash;
            BtActStatus.gun_type = 0;
            BtActStatus.action_on = 0;
        }
    }
}

namespace {

// clang-format off
/* The colour each weapon element gives the trail the weapon leaves. */
u8 wep_rgb[5][3] = {
    {0xFF, 0x3C, 0x00}, {0x64, 0xC8, 0xFF}, {0xFF, 0xFF, 0x64}, {0x8A, 0xFF, 0xBC}, {0xE3, 0x96, 0xAD},
};
// clang-format on

} // namespace

void SetWeaponColor() {
    s8 element = NowWeaponHave->best_elem;

    if (element == WEAPON_ELEMENT_NONE) {
        SetWeaponTrailDefaultColour();
    } else if (element >= 0 && element < 6) {
        u8 blue = wep_rgb[element][2] >> 2;
        u8 green = wep_rgb[element][1] >> 2;
        u8 red = wep_rgb[element][0] >> 2;

        CWeaponFx.colour[0].r = CWeaponFx.colour[2].r = red;
        CWeaponFx.colour[0].g = CWeaponFx.colour[2].g = green;
        CWeaponFx.colour[0].b = CWeaponFx.colour[2].b = blue;
        CWeaponFx.colour[1].r = CWeaponFx.colour[3].r = red / 10.0f;
        CWeaponFx.colour[1].g = CWeaponFx.colour[3].g = green / 10.0f;
        CWeaponFx.colour[1].b = CWeaponFx.colour[3].b = blue / 10.0f;
    }
}

BT_SHOT_EFFECT *Get_Main_EffectPtr(int chara, int form) {
    switch (chara) {
        case CHARA_TOAN:
            return MyEffectEntry_Tbl[0];
        case CHARA_XIAO:
            return MyEffectEntry_Tbl[1];
        case CHARA_GORO:
            return MyEffectEntry_Tbl[2];
        case CHARA_RUBY:
            // Ruby draws a different set for each of her forms.
            if (form < 0 || form > 4) {
                form = 0;
            }

            return MyEffectEntry_Tbl[3 + form];
        case CHARA_UNGAGA:
            return MyEffectEntry_Tbl[8];
        case CHARA_OSMOND: {
            // Osmond draws from the weapon he has equipped rather than a form.
            s32 weapon = UserStatus->chara_weapons[chara][UserStatus->equipped_weapon_slot[chara]].item_no;
            int entry = 0;

            BtActStatus.gun_type = 0;

            if (weapon == ITEM_WEAPON_JACKAL) {
                entry = 3;
                BtActStatus.gun_type = 0;
            }

            if (weapon == ITEM_WEAPON_BLESSING_GUN) {
                entry = 0;
                BtActStatus.gun_type = 2;
            }

            if (weapon == ITEM_WEAPON_SKUNK) {
                entry = 0;
                BtActStatus.gun_type = 2;
            }

            if (weapon == ITEM_WEAPON_G_CRUSHER) {
                entry = 4;
                BtActStatus.gun_type = 0;
            }

            if (weapon == ITEM_WEAPON_HEXA_BLASTER) {
                entry = 1;
                BtActStatus.gun_type = 1;
            }

            if (weapon == ITEM_WEAPON_STAR_BREAKER) {
                entry = 5;
                BtActStatus.gun_type = 0;
            }

            if (weapon == ITEM_WEAPON_SUPERNOVA) {
                entry = 2;
                BtActStatus.gun_type = 1;
            }

            if (weapon == ITEM_WEAPON_SNAIL) {
                entry = 2;
                BtActStatus.gun_type = 1;
            }

            return MyEffectEntry_Tbl[9 + entry];
        }
        default:
            break;
    }

    return nullptr;
}

void MainChara_Effect(BT_SHOT_EFFECT *effect, unsigned int *data, int reload) {
    CharaMainEffect.Initialize();
    CharaMainEffectCrash.Initialize();

    if (effect == nullptr) {
        return;
    }

    // A reload gives the effect block back before the new models go in.
    if (reload != 0) {
        TexManager.DeleteTextureBlock(0x10);
        TexManager.CleanUpBuffer();
        TexManager.CleanUpTextureList();
        WEffectModelBuffer.used = 0;
    }

    if (CharaMainEffect.Entry2(effect, data, 0x10, &WEffectModelBuffer, 6) == 0) {
        std::println("effect entry err!!");
    } else {
        std::println("set eff sucsess !");
    }

    if (UserStatus->cur_chara == CHARA_OSMOND) {
        CharaMainEffectCrash.Entry2(MyEffectEntry_Tbl[9], ozumond_default_effect, 0x10, &WEffectModelBuffer, 6);
    }

    if (UserStatus->cur_chara == CHARA_RUBY) {
        CharaMainEffectCrash.Entry2(MyEffectEntry_Tbl[3], ozumond_default_effect, 0x10, &WEffectModelBuffer, 6);
    }

    NowMainEffect = &CharaMainEffect;
}

void LoadChara2(int chara, int keep_place, unsigned int *chara_data, unsigned int *crash_data, unsigned int *default_data, unsigned int *main_data) {
    CFrameAttr    frame_attr;
    sceVu0FVECTOR pos;
    sceVu0FVECTOR rotation;

    frame_attr.fog_enable = true;
    frame_attr.clip_enable = false;
    frame_attr.program_option = 0;

    // The model goes back where the one it replaces stood.
    if (keep_place == 0) {
        sceVu0CopyVector(pos, CharaFrame->position);
        CharaFrame->GetRotation(rotation);
    }

    TexManager.DeleteTextureBlock(0x11);
    TexManager.DeleteTextureBlock(0x1D);
    TexManager.DeleteTextureBlock(0x10);
    TexManager.CleanUpBuffer();
    TexManager.CleanUpTextureList();
    CharaModelBuffer.used = 0;

    CharaMain.Initialize();

    for (CTexAnimeData &anime : CharaMainTexAnim) {
        anime.Initialize();
    }

    CharaMain.InitializeTexAnime(CharaMainTexAnim, 64);
    CharaMain.LoadPackData2(chara_data, "base.cfg", &CharaModelBuffer, 0x11, &CharaModelBuffer, 0);
    NowWeapon = nullptr;
    CharaHand.Initialize();

    if (chara == CHARA_XIAO || chara == CHARA_RUBY) {
        CFrameAttr hand_attr;

        hand_attr.clip_enable = true;
        hand_attr.unk_09 = 0;
        CharaHand.LoadPackData(chara_data, "hand_up.cfg", &CharaModelBuffer, &CharaModelBuffer, nullptr);
        CharaHand.frame->SetAttr(hand_attr, 1, 0);
    }

    CharaMain.frame->SetAttr(frame_attr, 1, 4);

    if (keep_place == 0) {
        CharaMain.frame->SetPosition(pos);
        CharaMain.frame->SetRotation(rotation[0], rotation[1], rotation[2]);
        CharaMain.SetPosition(pos);
        CharaMain.SetRotation(rotation[0], rotation[1], rotation[2]);
    }

    CharaFrame = static_cast<CFrameVu1 *>(CharaMain.frame);

    if (chara == CHARA_OSMOND) {
        std::array<LOADTEXTURE_INFO2, 2> info = {
            {
             {.name = nullptr, .block_no = 0x46, .mipmap = 0},
             {.name = nullptr, .block_no = 0x46, .mipmap = 0},
             }
        };
        int size = 0;

        u_int  *hand_texture = GetPackFile(chara_data, "c05w_h.img", &size);
        u_char *at = ArenaFree(CharaModelBuffer);

        memcpy(at, hand_texture, size);
        CharaModelBuffer.Alloc((((size >> 6) + 1) << 6) >> 4);
        info[0].name = reinterpret_cast<char *>(at);
        TexManager.DeleteTextureBlock(0x46);
        TexManager.LoadTextureBlockEX(0x46, info.data());

        u_int *effect = GetPackFile(chara_data, "mgan01.chr", &size);

        ozumond_default_effect = reinterpret_cast<unsigned int *>(ArenaFree(CharaModelBuffer));
        memcpy(ozumond_default_effect, effect, size);
        CharaModelBuffer.Alloc((((size >> 6) + 1) << 6) >> 4);
    }

    if (chara == CHARA_RUBY) {
        int    size = 0;
        u_int *effect = GetPackFile(chara_data, "c05_f03.chr", &size);

        ozumond_default_effect = reinterpret_cast<unsigned int *>(ArenaFree(CharaModelBuffer));
        memcpy(ozumond_default_effect, effect, size);
        CharaModelBuffer.Alloc((((size >> 6) + 1) << 6) >> 4);
    }

    // The weapon models take whatever the character models leave.
    WeaponModelBuffer.base = ArenaFree(CharaModelBuffer);
    WeaponModelBuffer.limit = 0x33450 - CharaModelBuffer.used;
    WeaponModelBuffer.used = 0;
    LoadWeapon2(crash_data, default_data, main_data, chara, 0);
    MemoryMapDump();
}

namespace {

/**
 * Loads Toan, his weapons and the models and effects every dungeon floor shares.
 */
void LoadData() {
    CFrameAttr           frame_attr;
    std::array<char, 64> weapon_name{};
    std::array<char, 64> weapon_effect_name{};
    int                  character_size = 0;
    int                  weapon_0_size = 0;
    int                  weapon_1_size = 0;
    int                  equipped_weapon_size = 0;

    frame_attr.fog_enable = true;
    frame_attr.clip_depth = 100.0f;
    frame_attr.clip_enable = false;
    frame_attr.program_option = 0;

    UserStatus->cur_chara = 0;
    BtGetWeaponNamePath3(weapon_name.data(), weapon_effect_name.data(), UserStatus->chara_weapons[0][UserStatus->equipped_weapon_slot[0]].item_no);
    std::println("[{}] {}", UserStatus->chara_weapons[0][UserStatus->equipped_weapon_slot[0]].item_no, weapon_name.data());

    std::string weapon_model_path = std::string("commenu/weapon/") + weapon_name.data();

    // The character and weapon files load one after another, each on a 64-byte boundary.
    LoadFile("dun/mainchara/c01d.chr", read_buffer, &character_size);
    unsigned int  aligned_size = ((character_size >> 6) + 1) << 6;
    unsigned int *weapon_data_0 = read_buffer + (aligned_size >> 2);

    LoadFile("commenu/weapon/c01w00.chr", weapon_data_0, &weapon_0_size);
    aligned_size = ((weapon_0_size >> 6) + 1) << 6;
    unsigned int *weapon_data_1 = weapon_data_0 + (aligned_size >> 2);

    LoadFile("commenu/weapon/c01w01.chr", weapon_data_1, &weapon_1_size);
    aligned_size = ((weapon_1_size >> 6) + 1) << 6;
    unsigned int *equipped_weapon_data = weapon_data_1 + (aligned_size >> 2);

    LoadFile(weapon_model_path.data(), equipped_weapon_data, &equipped_weapon_size);
    wait_now_loading_vsync();
    LoadChara2(UserStatus->cur_chara, 1, read_buffer, weapon_data_0, weapon_data_1, equipped_weapon_data);
    float chara_z = 150.0f;

    CharaFrame->SetPosition(200.0f, 0.0f, chara_z);

    BT_SHOT_EFFECT *main_effect = Get_Main_EffectPtr(UserStatus->cur_chara, 0);
    LoadFile("dun/mainchara/wep_eff/c01_fuusya.chr", read_buffer, nullptr);
    wait_now_loading_vsync();
    MainChara_Effect(main_effect, read_buffer, 0);

    if (SndVoiceLoad(0) != 0) {
        std::println("voice err ");
    } else {
        std::println("snd load");
    }

    LoadFile("dun/pack/maindat.pac", read_buffer, nullptr);
    wait_now_loading_vsync();
    cursorFrame = LoadMDSFilePack(read_buffer, "dun/etc/cursor.mds", &MainModelBuffer);
    bombCursorFrame = LoadMDSFilePack(read_buffer, "dun/etc/bomblock.mds", &MainModelBuffer);
    bicCursorFrame = LoadMDSFilePack(read_buffer, "dun/etc/bic.mds", &MainModelBuffer);
    bicCursorFrame->SetScale(0.7f, 0.7f, 0.7f);

    NowDngMap->atra_model = LoadMDSFilePack(read_buffer, "dun/etc/atr.mds", &MainModelBuffer);
    NowDngMap->collision_model = LoadCollisionFilePack(read_buffer, "dun/etc/atrcol.mds", &MainModelBuffer);
    NowDngMap->atra_model->SetAttr(frame_attr, 1, 0);

    NewChangeFx.LoadPackData2(GetPackFile(read_buffer, "ceffe_s.chr", nullptr), "info.cfg", &MainModelBuffer, 11, &MainModelBuffer, 0);
    itemOpenSmallFx.LoadPack(read_buffer, "igetfx", &MainModelBuffer, &MotionData, &itemOpenSmallFx_info, 0);
    itemOpenBigFx.LoadPack(read_buffer, "igetfxb", &MainModelBuffer, &MotionData, &itemOpenBigFx_info, 0);

    Water_Splash.LoadPackData2(GetPackFile(read_buffer, "water_sp.chr", nullptr), "info.cfg", &MainModelBuffer, 6, &MainModelBuffer, 0);

    unsigned int *effect_data = GetPackFile(read_buffer, "trap01.chr", nullptr);
    Trap_Circle.InitializeTexAnime(Trap_Circle_TexAnim, 16);
    Trap_Circle.LoadPackData2(effect_data, "info.cfg", &MainModelBuffer, 71, &MainModelBuffer, 0);
    Trap_Circle.frame->SetAttr(frame_attr, 1, 0x40);

    std::string   steal_model_name = std::format("kara0{}.mds", selectMapNo + 1);
    unsigned int *steal_model_data = GetPackFile(read_buffer, steal_model_name.data(), nullptr);
    StealModel = static_cast<CFrameVu1 *>(LoadMDSFile(steal_model_data, &MainModelBuffer, 0, nullptr, nullptr));

    MasekiModelBuffer.used = 0;
    LoadFile("dun/effect/maseki_ex.chr", read_buffer, nullptr);
    wait_now_loading_vsync();
    MasekiEffect[0].Entry2(&MyEntryEffect_Maseki00, read_buffer, 6, &MasekiModelBuffer, 3);

    memcpy(&MasekiEffect[1], &MasekiEffect[0], sizeof(CSHOT_EFFECT));
    MasekiEffect[1].ReEntry(&MyEntryEffect_Maseki01, &MasekiModelBuffer);
    memcpy(&MasekiEffect[2], &MasekiEffect[0], sizeof(CSHOT_EFFECT));
    MasekiEffect[2].ReEntry(&MyEntryEffect_Maseki02, &MasekiModelBuffer);
    memcpy(&MasekiEffect[3], &MasekiEffect[0], sizeof(CSHOT_EFFECT));
    MasekiEffect[3].ReEntry(&MyEntryEffect_Maseki03, &MasekiModelBuffer);
    memcpy(&MasekiEffect[4], &MasekiEffect[0], sizeof(CSHOT_EFFECT));
    MasekiEffect[4].ReEntry(&MyEntryEffect_Maseki04, &MasekiModelBuffer);
}

} // namespace

namespace {

// Spends one use of the quick-use item in slot, dropping it and announcing so once none are left.
void SpendQuickItem(int slot, int item_id) {
    if (--UserStatus->active_item_vol[slot] <= 0) {
        DelActiveItem(slot + 1);
        DngMessMan.message = 0xB7;
        DngMessMan.insert_mes_1 = GetCommonItemDataSystemMsg(item_id);
        DngMessMan.timer = 0xB4;
        DngMessMan.steev_window = false;
    }
}

} // namespace

void HitBlowVelocity(int hit_no, float *velocity) {
    COLLISION_HIT &hit = NowColData->hit[hit_no];
    sceVu0CopyVector(velocity, hit.velocity);

    if (hit.ready_phase != 10) {
        return;
    }

    float away_x = CharaMain.pos[0] - hit.pos[0];
    float away_z = CharaMain.pos[2] - hit.pos[2];
    float length = std::sqrt(away_x * away_x + away_z * away_z);
    if (length < 0.1f) {
        sceVu0FVECTOR rotation;
        CharaFrame->GetRotation(rotation);
        away_x = -std::sin(rotation[1]);
        away_z = -std::cos(rotation[1]);
        length = 1.0f;
    }

    velocity[0] = away_x / length;
    velocity[1] = 0.0f;
    velocity[2] = away_z / length;
}

int BtCheckDamageProc() {
    sceVu0FVECTOR pos;
    sceVu0FVECTOR from;
    sceVu0FVECTOR blow;
    int           taken = 0;
    int           blown = 0;

    static int dmgSnd = 0;

    if (dmgSnd > 0) {
        dmgSnd--;
    }

    // The debug menu can hold the player at full health.
    if (DebugStatus[20] == 2) {
        CUserStatus *status = UserStatus;

        status->hp[static_cast<int>(status->cur_chara)] = status->max_hp[static_cast<int>(status->cur_chara)];
        return 0;
    }

    if (BtActStatus.recoil_frames == 0 && BtActStatus.invincible_frames == 0) {
        sceVu0CopyVector(pos, CharaMain.pos);

        int hit_no = NowColData->CheckHitUser(pos, 1, CharaHeight(UserStatus));

        if (hit_no != -1) {
            taken = 1;

            if (BtActStatus.action_on == 6) {
                blown = taken;
            }

            BombInfo.aiming = false;
            BombInfo.throw_step = -1;
            BtActStatus.can_act = 1;
            BtActStatus.action_on = 0;
            BtActStatus.action_busy = 0;

            if (activeItem.model[8] != -1) {
                activeItem.models->AllReleasItem();
                activeItem.model[8] = -1;
            }

            BtActStatus.move_power = 0.0f;
            BtActStatus.gauge_exhausted = 0;
            BtActStatus.action_gauge = 100.0f;
            CMonUnitHold = 0;
            CEffectHold = 0;

            if (ruby_effect_id != -1 && UserStatus->cur_chara == CHARA_RUBY) {
                NowMainEffect->OffEffect(ruby_effect_id);
                ruby_effect_id = -1;
            }

            int            damage = NowColData->Get(hit_no)->damage;
            int            guard = UserStatus->defense[static_cast<int>(UserStatus->cur_chara)];
            COLLISION_HIT *hit = &NowColData->hit[hit_no];

            if (StatusErrCheck(AILMENT_STAMINA) != 0) {
                guard *= 2;
            }

            damage = std::max(damage - guard, 0);

            int owner = hit->owner;

            int monster = -1;

            if (owner != -1) {
                monster = (owner - 200) / 5;
                CMonstorUnit *unit = NowMonstorUnit;

                if (monster >= 0 && monster < 16) {
                    unit->monster[monster].last_hit_damage = damage;
                }

                NowMonstorUnit->chara[monster][0].GetPosition(from);
            }

            int roll = static_cast<int>(100.0f * static_cast<float>(rand()) / 2147483648.0f);

            // A hit that drains takes a fifth of the player's money and gives it to
            // whatever landed it.
            if ((hit->flags & 0x40000) && roll < 20 && monster != -1) {
                u16 had = UserStatus->money;

                if (had > 10) {
                    int drained = had / 5;

                    UserStatus->money -= drained;
                    SndSePlay(SE_ITEM_GET, -1, 0);
                    NowMonstorUnit->monster[monster].stolen_money += drained;
                    DngMessMan.message = 0xB5;
                    DngMessMan.timer = 0xB4;
                    DngMessMan.steev_window = false;
                }
            }

            if ((hit->flags & 0x1000) && roll < 0x41) {
                BtSetStatusErr(AILMENT_STAMINA);
            }

            if (hit->flags & 0x100000) {
                BtSetStatusErr(AILMENT_FREEZE);
            }

            if ((hit->flags & 0x100) && roll < 0x41) {
                int slot = static_cast<CDngStatusData *>(UserStatus)->CheckActItemSlot(ITEM_ANTI_FREEZE_AMULET);

                if (slot == -1) {
                    BtSetStatusErr(AILMENT_FREEZE);
                } else {
                    SpendQuickItem(slot, ITEM_ANTI_FREEZE_AMULET);
                }
            }

            if ((hit->flags & 0x200) && roll < 0x41 && (UserStatus->ailments[static_cast<int>(UserStatus->cur_chara)] & AILMENT_POISON) == 0) {
                int slot = static_cast<CDngStatusData *>(UserStatus)->CheckActItemSlot(ITEM_ANTIDOTE_AMULET);

                if (slot == -1) {
                    BtSetStatusErr(AILMENT_POISON);
                    DngMessMan.message = 0xBB;
                    DngMessMan.timer = 0xB4;
                    DngMessMan.steev_window = false;
                } else {
                    SpendQuickItem(slot, ITEM_ANTIDOTE_AMULET);
                }
            }

            if ((hit->flags & 0x400) && roll < 0x41 && (UserStatus->ailments[static_cast<int>(UserStatus->cur_chara)] & AILMENT_CURSE) == 0) {
                int slot = static_cast<CDngStatusData *>(UserStatus)->CheckActItemSlot(ITEM_ANTICURSEAMULET);

                if (slot == -1) {
                    BtSetStatusErr(AILMENT_CURSE);
                    DngMessMan.message = 0xB9;
                    DngMessMan.timer = 0xB4;
                    DngMessMan.steev_window = false;
                } else {
                    SpendQuickItem(slot, ITEM_ANTICURSEAMULET);
                }
            }

            if ((hit->flags & 0x800) && roll < 0x41 && (UserStatus->ailments[static_cast<int>(UserStatus->cur_chara)] & AILMENT_GOO) == 0) {
                int slot = static_cast<CDngStatusData *>(UserStatus)->CheckActItemSlot(ITEM_ANTIGOO_AMULET);

                if (slot == -1) {
                    BtSetStatusErr(AILMENT_GOO);
                    DngMessMan.message = 0xBA;
                    DngMessMan.timer = 0xB4;
                    DngMessMan.steev_window = false;
                } else {
                    SpendQuickItem(slot, ITEM_ANTIGOO_AMULET);
                }
            }

            // A hit that halves takes half of what the player can hold.
            if (hit->flags & 0x80000) {
                damage = UserStatus->max_hp[static_cast<int>(UserStatus->cur_chara)] >> 1;
            }

            if (hit->kind == 3) {
                sceVu0CopyVector(blow, hit->pos);
                GamePad.SetVibration(1, 0xE6, 0x16);

                if (BtActStatus.action_no == 9) {
                    HitBlowVelocity(hit_no, blowVelo);
                    velo__2[0] = blowVelo[0] / 10.0f;
                    velo__2[2] = blowVelo[2] / 10.0f;

                    if (hit->target_mask != 3) {
                        NowColData->active[hit_no] = 0;
                    }

                    if (dmgSnd <= 0) {
                        SndSePlay(SE_HIT_BLOCKED, -1, 0);
                        dmgSnd = 30;
                    }
                } else {
                    SndSePlay(SE_CHARA_HURT, -1, 0);
                    SndSePlay(SE_PLAYER_HIT, -1, 0);
                    BtActStatus.hud_shake_frames = 15;
                    setUnitAmbientAnime(160.0f, 1.0f, 255.0f, 0.0f, 0.0f);
                    UserStatus->AddNowLife(UserStatus->cur_chara, -damage, 10.0f);

                    float number_pos[4] = {0.0f, 0.0f, 0.0f, 1.0f};

                    number_pos[1] = CharaHeight(UserStatus);
                    HitValueEntry(NowHitValue, number_pos, damage, HIT_VALUE_PLAYER, CharaMain.frame);

                    HitBlowVelocity(hit_no, blowVelo);
                    unitBlowActionRot(blowVelo);

                    if (hit->target_mask != 3) {
                        NowColData->active[hit_no] = 0;
                    }

                    if (damage < UserStatus->hp[static_cast<int>(UserStatus->cur_chara)]) {
                        BtActStatus.action_on = 5;
                        BtActStatus.motion_no = 6;
                        CharaMain.motion_type.state.time = static_cast<float>(CharaMain.motion_type.motion_info[6].start);
                    } else {
                        BtActStatus.action_on = 4;
                        BtActStatus.motion_no = 4;
                        CharaMain.motion_type.state.time = static_cast<float>(CharaMain.motion_type.motion_info[4].start);
                    }

                    driveNoInterpolate = 1;
                    lockOnTargetFlag = 0;
                    BtActStatus.recoil_frames = 0xA0;
                }
            }

            if (hit->kind == 2 || hit->kind == 4) {
                sceVu0FVECTOR at;
                sceVu0FVECTOR away;

                sceVu0CopyVector(at, hit->pos);
                GamePad.SetVibration(1, 0xDC, 0xC);

                if (blown != 0) {
                    sceVu0CopyVector(away, CharaMain.pos);

                    if (monster != -1) {
                        away[0] -= from[0];
                        away[1] = 0.0f;
                        away[2] -= from[2];
                    } else {
                        away[0] -= at[0];
                        away[1] = 0.0f;
                        away[2] -= at[2];
                    }

                    sceVu0CopyVector(MyHitPointMark[0].pos, at);
                    MyHitPointMark[0].on = 1;
                    MyHitPointMark[0].blink = 0;
                    MyHitPointMark[0].timer = 0x10;
                    velo__2[0] = away[0] / 2.0f;
                    velo__2[2] = away[2] / 2.0f;

                    if (hit->target_mask != 3) {
                        NowColData->active[hit_no] = 0;
                    }

                    if (dmgSnd <= 0) {
                        SndSePlay(SE_HIT_BLOCKED, -1, 0);
                        dmgSnd = 30;
                    }
                } else {
                    SndSePlay(SE_CHARA_HURT_HEAVY, -1, 0);
                    SndSePlay(SE_PLAYER_HIT, -1, 0);
                    BtActStatus.hud_shake_frames = 25;
                    setUnitAmbientAnime(80.0f, 1.0f, 255.0f, 0.0f, 0.0f);
                    UserStatus->AddNowLife(UserStatus->cur_chara, -damage, 10.0f);

                    if (hit->target_mask != 3) {
                        NowColData->active[hit_no] = 0;
                    }

                    float heavy_number_pos[4] = {0.0f, 0.0f, 0.0f, 1.0f};

                    heavy_number_pos[1] = CharaHeight(UserStatus);
                    HitValueEntry(NowHitValue, heavy_number_pos, damage, HIT_VALUE_PLAYER, CharaMain.frame);

                    static int     cnt = 0;
                    CHitPointMark *spot = &MyHitPointMark[cnt];

                    sceVu0CopyVector(spot->pos, at);
                    spot->on = 1;
                    spot->blink = 0;
                    spot->timer = 0x10;

                    if (cnt >= 15) {
                        cnt = 0;
                    } else {
                        cnt++;
                    }

                    if (!(stickVector < 0.8f)) {
                        BtActStatus.action_on = 0;
                        BtActStatus.motion_no = 0x1D;
                    } else {
                        BtActStatus.action_on = 4;
                        BtActStatus.motion_no = 4;
                        CharaMain.motion_type.state.time = static_cast<float>(CharaMain.motion_type.motion_info[4].start);
                    }

                    driveNoInterpolate = 1;
                    BtActStatus.recoil_frames = 0x50;

                    if (hit->kind == 4) {
                        BtActStatus.recoil_frames = 8;
                        setUnitAmbientAnime(10.0f, 1.0f, 255.0f, 0.0f, 0.0f);
                    } else {
                        setUnitAmbientAnime(80.0f, 1.0f, 255.0f, 0.0f, 0.0f);
                    }
                }
            }
        }
    }

    if (DebugStatus[20] > 0) {
        CUserStatus *status = UserStatus;

        status->hp[static_cast<int>(status->cur_chara)] = status->max_hp[static_cast<int>(status->cur_chara)];
    }

    return taken;
}

namespace {

/**
 * Spends one use of the selected active item, taking it away on its last use.
 */
void SpendActiveItem() {
    s16 *slots = UserStatus->active_item;

    // The counts follow the three item numbers.
    if (slots[itemNowSel + 3] == 1) {
        slots[itemNowSel] = -1;
        slots[itemNowSel + 3] = 0;
    } else {
        slots[itemNowSel + 3]--;
    }
}

/**
 * Drops the model of the item the player holds out.
 */
void DropHeldItemModel() {
    if (activeItem.model[8] != -1) {
        activeItem.models->DeleteModel(activeItem.model[8]);
    }

    activeItem.model[8] = -1;
}

} // namespace

void BattleActionThlow() {
    sceVu0FVECTOR stood;
    sceVu0FVECTOR aim;

    float facing = NowCamera__3->GetAngle();

    if (BtActStatus.action_on != 2) {
        return;
    }

    // The stick walks the landing point around while the button is held.
    if (BombInfo.aiming != 0) {
        if (GamePad.On(PAD_SQUARE) != 0) {
            float stick_x = GamePad.GetLXf();
            float stick_y = GamePad.GetLYf();

            sceVu0CopyVector(stood, CharaMain.pos);
            aim[0] = BombInfo.pos[0] + stick_x * cos(facing) + stick_y * sinf(facing);
            aim[2] = BombInfo.pos[2] + stick_y * cos(facing) - stick_x * sinf(facing);
            aim[1] = BombInfo.pos[1];
            aim[3] = 1.0f;

            float away = DistVector(aim, stood);

            if (!(away < 30.0f) && away <= 120.0f) {
                sceVu0CopyVector(BombInfo.pos, aim);
            }

            float heading = atan2f(aim[0] - stood[0], aim[2] - stood[2]);
            float turned = unitRotation(CharaFrame, heading);
            float zero = 0.0f;

            CharaFrame->SetRotation(zero, turned, zero);
            BtActStatus.motion_no = 0x1A;
        } else {
            BombInfo.aiming = false;
            BombInfo.throw_step = 1;
            BtActStatus.motion_no = 0x1B;
        }
    }

    if (BombInfo.throw_step == 1) {
        constexpr std::array<float, 6> throw_frame = {548.0f, 452.0f, 267.0f, 347.0f, 188.0f, 321.0f};
        auto                           end = static_cast<float>(CharaMain.motion_type.motion_info[27].end);
        float                          start = throw_frame[UserStatus->cur_chara];

        if (CharaMain.motion_type.state.time >= start && CharaMain.motion_type.state.time <= end) {
            SndSePlay(SE_THROW, -1, 0);

            SpendActiveItem();

            if (lockOnTargetFlag == 0) {
                sceVu0FVECTOR vector;
                sceVu0FVECTOR from;

                DropHeldItemModel();

                float offset[4] = {0.0f, 0.0f, 0.0f, 1.0f};

                CharaMain.frame->SearchFrame("item")->GetWorldPosition(from, offset);
                DistVector(from, BombInfo.pos);
                ParabolicInitialVector(vector, from, BombInfo.pos, 0.12f, 20.0f);
                sceVu0ScaleVectorXYZ(vector, vector, 0.8f);

                if (activeItem.model[8] == -1) {
                    activeItem.models->SetThrowModel(activeItem.model[activeItem.now], from, vector);
                }

                BombInfo.throw_step++;
            } else {
                sceVu0FVECTOR lock_vector;
                sceVu0FVECTOR lock_from;

                DropHeldItemModel();

                float   lock_offset[4] = {0.0f, 0.0f, 0.0f, 1.0f};
                CFrame *hand = CharaMain.frame->SearchFrame("item");

                if (hand != nullptr) {
                    hand->GetWorldPosition(lock_from, lock_offset);
                }

                lock_vector[0] = BtActStatus.target_position[0] - lock_from[0];
                lock_vector[1] = 5.0f + (BtActStatus.target_position[1] - lock_from[1]);
                lock_vector[2] = BtActStatus.target_position[2] - lock_from[2];
                sceVu0Normalize(lock_vector, lock_vector);
                lock_vector[0] *= 4.5f;
                lock_vector[1] *= 4.5f;
                lock_vector[2] *= 4.5f;

                if (activeItem.model[8] == -1) {
                    activeItem.models->SetThrowModel(activeItem.model[activeItem.now], lock_from, lock_vector);
                }

                BombInfo.throw_step++;
            }
        }

        BtActStatus.motion_no = 0x1B;
    }

    if (BombInfo.throw_step == 2) {
        BtActStatus.motion_no = 0x1B;

        auto  end = static_cast<float>(CharaMain.motion_type.motion_info[27].end);
        float now = CharaMain.motion_type.state.time;

        if (now >= end - 2.0f && now <= end) {
            BtActStatus.action_on = 0;
            BtActStatus.can_act = 1;
            BtActStatus.unk_028 = 0;
            driveNoInterpolate = 1;

            if (lockOnTargetFlag != 0) {
                BtActStatus.motion_no = 0x12;
            }

            BtActStatus.action_busy = 0;
            BtActStatus.motion_no = 0;
        }
    }
}

void BattleActionDrink() {
    if (BtActStatus.action_on != 3) {
        return;
    }

    constexpr std::array<float, 6> drink_frame = {628.0f, 474.0f, 285.0f, 474.0f, 474.0f, 474.0f};

    BtActStatus.motion_no = 0x1C;

    float frame = drink_frame[UserStatus->cur_chara];
    float now = CharaMain.motion_type.state.time;

    if (now >= frame && now < frame + 0.3f) {
        SndSePlay(SE_DRINK, -1, 0);
    }

    if (now >= frame + 10.0f && now < frame + 10.3f) {
        setUnitAmbientAnime(64.0f, 1.0f, 0.0f, 122.0f, 208.0f);
        SndSePlay(SE_EFFECT_CHIME, -1, 0);
    }

    auto end = static_cast<float>(CharaMain.motion_type.motion_info[28].end);

    if (CharaMain.motion_type.state.time >= end - 1.0f && CharaMain.motion_type.state.time <= end) {
        s32 running = activeItem.item[activeItem.now];

        usedActiveItem(UserStatus, running);

        SpendActiveItem();
        DropHeldItemModel();
        SndSePlay(SE_CHARA_RECOVER, -1, 0);

        BtActStatus.action_on = 0;
        BtActStatus.can_act = 1;
        driveNoInterpolate = 1;
        BtActStatus.action_busy = 0;
        BtActStatus.motion_no = 0;
        CMonUnitHold = 0;
        CEffectHold = 0;
        DngMessMan.message = -1;
        DngMessMan.timer = 0;
        DngMessMan.steev_window = false;
        DngMessMan.steev_index = 0;
        DngMessMan.hide_timer = 0;
    }
}

namespace {

/* How long until Ozumond's action plays its sound again, in frames. */
s32 ozumond_snd_cnt;

/* How long until the player can be damaged again, in frames. */
s32 dmg_check_wait;

/**
 * Ends the running action; a locked-on player turns back to the target unless told not to.
 */
void EndAction(bool face_target = true) {
    BtActStatus.unk_028 = 0;
    BtActStatus.motion_no = 0;
    BtActStatus.action_on = 0;

    if (lockOnTargetFlag != 0 && face_target) {
        BtActStatus.motion_no = 0x12;
    }
}

/**
 * Puts a shot from the player's character in the first free slot, if there is one.
 */
void SpawnShot(sceVu0FVECTOR world, sceVu0FVECTOR vector, int damage) {
    CSHOT *shots = NowShotData;

    for (int i = 0; i < 12; i++) {
        if (shots->used[i] == 0) {
            sceVu0CopyVector(shots->pos[i], world);
            sceVu0CopyVector(shots->vector[i], vector);
            shots->life[i] = 0x78;
            shots->damage[i] = damage;
            shots->size[i] = 1.0f;
            shots->halted[i] = 0;
            shots->start_flag[i] = 0;
            shots->used[i] = 1;
            break;
        }
    }
}

} // namespace

void BattleActionOn_Jinn() {
    if (BtActStatus.action_on == 0 && static_cast<s32>(BtActStatus.action_gauge) == 100) {
        BtActStatus.action_step = 0;
        BtActStatus.charge_time = 0.0f;
        BtActStatus.action_no = 11;
        BtActStatus.action_on = 1;
        BtActStatus.action_gauge = 0.0f;
        driveNoInterpolate = 1;
        SndSePlay(SE_CHARA_ACTION, -1, 0);
    }
}

void BattleActionPlay_Jinn(CCharacter *chara, int aimed) {
    float now = chara->motion_type.state.time;
    int   damage = NowWeaponHave->attack;

    // A poisoned weapon hits twice as hard.
    if (StatusErrCheck(AILMENT_STAMINA) != 0) {
        damage *= 2;
    }

    if (BtActStatus.action_no == 11) {
        if (GamePad.On(PadInput_OK) == 0) {
            BtActStatus.action_step = 1;
        }

        BtActStatus.motion_no = 11;

        auto end = static_cast<float>(chara->motion_type.motion_info[11].end);

        // Letting go before the wind-up ends holds the swing; keeping it
        // pressed goes straight on to the throw.
        if (now >= end - 2.0f && now <= end) {
            if (BtActStatus.action_step == 0) {
                BtActStatus.action_no = 12;
            } else {
                BtActStatus.action_no = 13;
            }
        }
    }

    if (BtActStatus.action_no == 13) {
        BtActStatus.motion_no = 13;

        if (aimed == 0) {
            if (now > 251.0f && now < 252.0f) {
                SndSePlay(SE_CHARA_ACTION_2, -1, 0);
                SndSePlay(SE_CHARA_SHOUT_2, -1, 0);

                sceVu0FVECTOR world;
                sceVu0FVECTOR vector;
                float         offset[4] = {0.0f, 0.0f, 0.0f, 1.0f};
                CFrame       *hand = NowWeapon->frame->SearchFrame("eff30");

                if (hand != nullptr) {
                    hand->GetWorldPosition(world, offset);

                    if (lockOnTargetFlag == 0) {
                        float y = CharaMain.GetRotation()->y;

                        setShotVector(vector, 5.0f, y, 0.0f);
                    } else {
                        vector[0] = BtActStatus.target_position[0] - world[0];
                        vector[1] = BtActStatus.target_position[1] - world[1];
                        vector[2] = BtActStatus.target_position[2] - world[2];
                        sceVu0Normalize(vector, vector);
                        vector[0] *= 5.0f;
                        vector[1] *= 5.0f;
                        vector[2] *= 5.0f;
                    }

                    SpawnShot(world, vector, damage);

                    BtActStatus.frames_since_attack = 0;
                }

                SwordDmgCheck1(1.0f, 0);
            }
        } else {
            if (now > 21.0f && now < 21.5f) {
                SndSePlay(SE_CHARA_ACTION_2, -1, 0);
                SndSePlay(SE_CHARA_SHOUT_2, -1, 0);

                sceVu0FVECTOR world;
                sceVu0FVECTOR vector;
                float         offset[4] = {0.0f, 0.0f, 0.0f, 1.0f};
                CFrame       *hand = NowWeapon->frame->SearchFrame("eff30");

                if (hand != nullptr) {
                    hand->GetWorldPosition(world, offset);
                    setShotVector(vector, 3.5f, viewAngleH__2, viewAngleV__2);

                    SpawnShot(world, vector, damage);

                    BtActStatus.frames_since_attack = 0;
                }

                SwordDmgCheck1(1.0f, 0);
            }
        }

        auto end = static_cast<float>(chara->motion_type.motion_info[13].end);

        if (now >= end - 2.0f && now <= end) {
            EndAction();
        }
    }

    if (BtActStatus.action_no == 12) {
        if (GamePad.On(PadInput_OK) == 0) {
            BtActStatus.action_step = 1;
        }

        BtActStatus.motion_no = 12;
        driveNoInterpolate = 1;

        if (BtActStatus.action_step != 0) {
            driveNoInterpolate = 1;
            BtActStatus.action_no = 13;
        }
    }
}

void BattleActionOn_Ruby() {
    if (BtActStatus.action_on == 0 && static_cast<s32>(BtActStatus.action_gauge) == 100) {
        BtActStatus.action_step = 0;
        BtActStatus.action_no = 11;
        BtActStatus.action_on = 1;
        BtActStatus.action_gauge = 0.0f;
        BtActStatus.charge_time = 0.0f;
        driveNoInterpolate = 1;
    }
}

void BattleActionShotRuby(CCharacter *chara, int aimed, float scale, int repeat) {
    WEAPON_HAVE *weapon = NowWeaponHave;
    int          element = weapon->best_elem;
    int          damage = (weapon->attack >> 1) + (weapon->magic >> 1);

    damage = static_cast<int>(static_cast<float>(damage) * scale);

    // A poisoned weapon hits twice as hard.
    if (StatusErrCheck(AILMENT_STAMINA) != 0) {
        damage *= 2;
    }

    // A repeat shot reuses the start and the way the first one took.
    if (repeat != 0) {
        if (NowMainEffect->Set(ruby_effect_pos, ruby_effect_vec, 3, element, 0, nullptr, 1) != -1) {
            NowMainEffect->SetWepStatus(NowWeaponHave->flags);
            NowMainEffect->SetVsMonster(NowWeaponHave->vs_monster);
            NowMainEffect->SetDmg(damage);
            NowMainEffect->SetRandomRate(2.0f);
            NowMainEffect->SetNoSound();
        }

        return;
    }

    if (aimed == 0) {
        getFramePos(static_cast<CFrameVu1 *>(chara->frame), "weapon", ruby_effect_pos);

        if (lockOnTargetFlag == 0) {
            getCharacterVector(ruby_effect_vec, 0.0f);
            ruby_effect_vec[0] += ruby_effect_pos[0];
            ruby_effect_vec[1] += ruby_effect_pos[1];
            ruby_effect_vec[2] += ruby_effect_pos[2];
        } else {
            sceVu0CopyVector(ruby_effect_vec, BtActStatus.target_position);
        }

        if (NowMainEffect->Set(ruby_effect_pos, ruby_effect_vec, 3, element, 0, nullptr, 1) == -1) {
            return;
        }

        NowMainEffect->SetWepStatus(NowWeaponHave->flags);
        NowMainEffect->SetVsMonster(NowWeaponHave->vs_monster);
        NowMainEffect->SetDmg(damage);
        return;
    }

    float offset[4] = {0.0f, 0.0f, 0.0f, 1.0f};

    chara->frame->SearchFrame("weapon")->GetWorldPosition(ruby_effect_pos, offset);
    setShotVector(ruby_effect_vec, 3.5f, viewAngleH__2, viewAngleV__2);
    ruby_effect_vec[0] += ruby_effect_pos[0];
    ruby_effect_vec[1] += ruby_effect_pos[1];
    ruby_effect_vec[2] += ruby_effect_pos[2];

    if (NowMainEffect->Set(ruby_effect_pos, ruby_effect_vec, 3, element, 0, nullptr, 1) != -1) {
        NowMainEffect->SetWepStatus(NowWeaponHave->flags);
        NowMainEffect->SetVsMonster(NowWeaponHave->vs_monster);
        NowMainEffect->SetDmg(damage);
    }
}

void BattleActionPlay_Ruby(CCharacter *chara, int aimed) {
    float now = chara->motion_type.state.time;
    int   element = NowWeaponHave->best_elem;

    if (BtActStatus.action_no == 11) {
        if (GamePad.On(PadInput_OK) == 0) {
            BtActStatus.action_step = 1;
        }

        BtActStatus.motion_no = 11;
        ruby_effect_id = -1;

        // Holding the button past the wind-up starts a charge; the shot leaves either way.
        if (now >= 126.0f && now < 126.3f && BtActStatus.action_step == 0) {
            sceVu0FVECTOR pos;
            sceVu0FVECTOR aim;

            BtActStatus.motion_no = 14;
            BtActStatus.action_no = 14;
            BtActStatus.charge_time = 0.0f;
            ruby_effect_id = NowMainEffect->Set(pos, aim, 3, element, 0, chara->frame->SearchFrame("item"), 0);
            NowMainEffect->SetLoop(1);
        }

        if (now >= 128.0f && now < 128.3f) {
            BattleActionShotRuby(chara, aimed, 1.0f, 0);
            BtActStatus.frames_since_attack = 0;
            SwordDmgCheck1(0.8f, 0);
            SndSePlay(SE_CHARA_ACTION, -1, 0);
            SndSePlay(SE_CHARA_SHOUT, -1, 0);
        }

        auto end = static_cast<float>(chara->motion_type.motion_info[11].end);

        if (now >= end - 1.0f && now <= end) {
            EndAction(aimed == 0);
        }
    }

    if (BtActStatus.action_no == 14) {
        if (GamePad.On(PadInput_OK) == 0) {
            BtActStatus.action_step = 1;
        }

        BtActStatus.motion_no = 14;
        SndSeSeqPlayStop(0x193, 5, 0);

        BtActStatus.charge_time += 1.0f;

        BtActStatus.charge_time = std::min(BtActStatus.charge_time, 60.0f);

        if (BtActStatus.charge_time >= 58.0f && BtActStatus.charge_time < 58.5f) {
            setUnitAmbientAnime(15.0f, 1.0f, 0.0f, 122.0f, 208.0f);
        }

        if (BtActStatus.action_step != 0) {
            BtActStatus.action_no = 16;
            BtActStatus.action_gauge = 0.0f;
        }
    }

    if (BtActStatus.action_no == 16) {
        BtActStatus.motion_no = 16;

        // A full charge throws two shots; a short one throws a single
        // stronger shot a little later.
        if (BtActStatus.charge_time >= 60.0f) {
            if (now >= 216.0f && now < 217.5f && ruby_effect_id != -1) {
                NowMainEffect->OffEffect(ruby_effect_id);
                ruby_effect_id = -1;
            }

            if (aimed == 0) {
                if (now >= 216.0f && now < 216.4f) {
                    BattleActionShotRuby(chara, aimed, 2.2f, 0);
                    BattleActionShotRuby(chara, aimed, 2.2f, 1);
                    BtActStatus.frames_since_attack = 0;
                    SwordDmgCheck1(1.8f, 0);
                    SndSePlay(SE_CHARA_ACTION, -1, 0);
                    SndSePlay(SE_CHARA_SHOUT_3, -1, 0);
                }
            } else {
                if (now >= 216.0f && now < 216.3f) {
                    BattleActionShotRuby(chara, aimed, 2.2f, 0);
                    BattleActionShotRuby(chara, aimed, 2.2f, 1);
                    BtActStatus.frames_since_attack = 0;
                    SwordDmgCheck1(1.8f, 0);
                    SndSePlay(SE_CHARA_ACTION, -1, 0);
                    SndSePlay(SE_CHARA_SHOUT_3, -1, 0);
                }
            }
        } else {
            if (now >= 216.5f && now < 217.5f && ruby_effect_id != -1) {
                NowMainEffect->OffEffect(ruby_effect_id);
                ruby_effect_id = -1;
            }

            if (now >= 216.5f && now < 216.9f) {
                BattleActionShotRuby(chara, aimed, 1.5f, 0);
                BtActStatus.frames_since_attack = 0;
                SwordDmgCheck1(1.2f, 0);
                SndSePlay(SE_CHARA_ACTION, -1, 0);
                SndSePlay(SE_CHARA_SHOUT_3, -1, 0);
            }
        }

        auto end = static_cast<float>(chara->motion_type.motion_info[16].end);

        if (now >= end - 1.0f && now <= end) {
            EndAction(aimed == 0);
        }
    }
}

void BattleActionOn_Ozumond() {
    if (BtActStatus.action_on == 0 && BtActStatus.gauge_exhausted == 0) {
        BtActStatus.action_no = 11;
        BtActStatus.action_on = 1;
        BtActStatus.action_step = 0;
        BtActStatus.charge_level = 0;
        BtActStatus.charge_time = 1.0f;
        driveNoInterpolate = 1;
        SndSePlay(SE_CHARA_SHOUT, -1, 0);
        ozumond_snd_cnt = 30;
    }
}

void BattleActionOn_Ozumond_H() {
    if (BtActStatus.action_on == 0 && static_cast<s32>(BtActStatus.action_gauge) == 100) {
        BtActStatus.action_no = 11;
        BtActStatus.action_on = 1;
        BtActStatus.action_step = 0;
        BtActStatus.charge_level = 0;
        BtActStatus.action_gauge = 0.0f;
        BtActStatus.charge_time = 30.0f;
        driveNoInterpolate = 1;
        SndSePlay(SE_CHARA_SHOUT, -1, 0);
        ozumond_snd_cnt = 30;
    }
}

void BattleActionOn_Ozumond_F() {
    if (BtActStatus.action_on == 0 && BtActStatus.gauge_exhausted == 0) {
        BtActStatus.action_no = 0;
        BtActStatus.action_on = 1;
        BtActStatus.action_step = 0;
        BtActStatus.charge_level = 0;
        driveNoInterpolate = 1;
        dmg_check_wait = 0;
        SndSePlay(SE_CHARA_SHOUT, -1, 0);
        ozumond_snd_cnt = 30;
    }
}

void BattleActionPlay_Ozumond(int aimed) {
    sceVu0FVECTOR pos;
    sceVu0FVECTOR aim;
    sceVu0FVECTOR shot;

    WEAPON_HAVE *weapon = NowWeaponHave;
    int          damage = (weapon->attack >> 1) + (weapon->magic >> 1);

    // A poisoned weapon hits twice as hard.
    if (StatusErrCheck(AILMENT_STAMINA) != 0) {
        damage *= 2;
    }

    float drain = 1.5f - 0.01f * static_cast<float>(NowWeaponHave->speed);

    if (BtActStatus.action_no != 11) {
        return;
    }

    if (GamePad.On(PadInput_OK) == 0) {
        BtActStatus.action_step = 1;
    }

    SndSeSeqPlayStop(0x190, 5, 0);

    // The action runs until the gauge the weapon drains runs out.
    BtActStatus.action_gauge -= drain;

    if (BtActStatus.action_gauge <= 0.0f) {
        EndAction();
        BtActStatus.gauge_exhausted = 1;
    }

    BtActStatus.motion_no = 11;

    if (aimed == 0) {
        getFramePos(static_cast<CFrameVu1 *>(NowWeapon->frame), "dcol0", pos);
        getCharacterVector(aim, 0.0f);
    } else {
        setShotVector(aim, 3.5f, viewAngleH__2, viewAngleV__2);
        sceVu0CopyVector(pos, CharaMain.pos);
        pos[1] += 10.0f;
    }

    sceVu0CopyVector(shot, aim);
    aim[0] += pos[0];
    aim[1] += pos[1];
    aim[2] += pos[2];

    if (lockOnTargetFlag != 0) {
        shot[0] = BtActStatus.target_position[0] - pos[0];
        shot[1] = BtActStatus.target_position[1] - pos[1];
        shot[2] = BtActStatus.target_position[2] - pos[2];
    }

    static int s_cnt = 0;

    s_cnt++;

    if (s_cnt >= 10) {
        s_cnt = 0;

        if (aimed == 0) {
            NowMainEffect->Set(pos, aim, -1, -1, 0, NowWeapon->frame->SearchFrame("dcol0"), -1);
        }
    }

    static int msg_cnt = 0;

    msg_cnt++;

    if (msg_cnt >= 10) {
        msg_cnt = 0;
        sceVu0Normalize(shot, shot);
        sceVu0ScaleVectorXYZ(shot, shot, 10.0f);
        OzumondShot.Set(pos, shot, damage, 0);
        BtActStatus.frames_since_attack = 0;
        SwordDmgCheck1(0.5f, 0);
    }

    if (BtActStatus.action_step == 1) {
        EndAction();

        NowMainEffect->OffEffect(-1);
    }
}

void BattleActionPlay_Ozumond_H(int aimed) {
    static int    wait_cnt;
    sceVu0FVECTOR pos;
    sceVu0FVECTOR aim;
    sceVu0FVECTOR shot;

    WEAPON_HAVE *weapon = NowWeaponHave;
    int          element = weapon->best_elem;
    int          damage = weapon->attack;

    // A poisoned weapon hits twice as hard.
    if (StatusErrCheck(AILMENT_STAMINA) != 0) {
        damage *= 2;
    }

    if (BtActStatus.action_no != 11) {
        return;
    }

    if (GamePad.On(PadInput_OK) == 0) {
        BtActStatus.action_step = 1;
        wait_cnt = 10;
    }

    BtActStatus.motion_no = 11;

    wait_cnt++;

    if (wait_cnt >= 10) {
        wait_cnt = 0;
        SndSeSeqPlayStop(0x199, 5, 0);

        if (aimed == 0) {
            getFramePos(static_cast<CFrameVu1 *>(NowWeapon->frame), "dcol0", pos);
            getCharacterVector(aim, 0.0f);
        } else {
            setShotVector(aim, 3.5f, viewAngleH__2, viewAngleV__2);
            sceVu0CopyVector(pos, CharaMain.pos);
            pos[1] += 10.0f;
        }

        sceVu0CopyVector(shot, aim);
        aim[0] += pos[0];
        aim[1] += pos[1];
        aim[2] += pos[2];

        float *target = lockOnTargetFlag != 0 ? BtActStatus.target_position : aim;

        if (NowMainEffect->Set(pos, target, 5, element, 0, nullptr, -1) != -1) {
            NowMainEffect->SetVsMonster(NowWeaponHave->vs_monster);
            NowMainEffect->SetWepStatus(NowWeaponHave->flags);
            NowMainEffect->SetAttribute(GetWeaponElementAttr(NowWeaponHave->best_elem));
            NowMainEffect->SetDmg(damage);
        }

        BtActStatus.action_step = 1;
        BtActStatus.frames_since_attack = 0;
        SwordDmgCheck1(2.0f, 0);
    }

    if (BtActStatus.action_step == 1) {
        EndAction();
    }
}

void BattleActionPlay_Ozumond_F(int aimed) {
    static int    wait_cnt = 0;
    sceVu0FVECTOR start;
    sceVu0FVECTOR start_dir;
    sceVu0FVECTOR pos;
    sceVu0FVECTOR aim;

    GetWeaponElementAttr(NowWeaponHave->best_elem);

    int damage = NowWeaponHave->attack;

    // A poisoned weapon hits twice as hard.
    if (StatusErrCheck(AILMENT_STAMINA) != 0) {
        damage *= 2;
    }

    if (BtActStatus.action_no == 0) {
        BtActStatus.motion_no = 11;
        dmg_check_wait++;

        if (dmg_check_wait >= 8) {
            dmg_check_wait = 0;
            SwordDmgCheck1(1.5f, 0);
            BtActStatus.action_no = 1;
            BtActStatus.frames_since_attack = 0;

            if (aimed == 0) {
                getFramePos(static_cast<CFrameVu1 *>(NowWeapon->frame), "dcol0", start);
                getCharacterVector(start_dir, 0.0f);
            } else {
                setShotVector(start_dir, 1.0f, viewAngleH__2, viewAngleV__2);
                sceVu0CopyVector(start, CharaMain.pos);
                start[1] += 10.0f;
            }

            OzumondFire.Init(start, start_dir, 0, 0);
        }

        if (GamePad.On(PadInput_OK) == 0) {
            EndAction();

            wait_cnt = 10;
        }
    }

    if (BtActStatus.action_no != 1) {
        return;
    }

    BtActStatus.motion_no = 11;
    SndSeSeqPlayStop(0x194, 5, 0);

    // The action runs until the gauge the weapon drains runs out.
    BtActStatus.action_gauge -= 0.5f;

    if (BtActStatus.action_gauge <= 0.0f) {
        EndAction();
        BtActStatus.gauge_exhausted = 1;
    }

    if (aimed == 0) {
        getFramePos(static_cast<CFrameVu1 *>(NowWeapon->frame), "dcol0", pos);
        getCharacterVector(aim, 0.0f);
    } else {
        setShotVector(aim, 3.5f, viewAngleH__2, viewAngleV__2);
        sceVu0CopyVector(pos, CharaMain.pos);
        pos[1] += 10.0f;
    }

    OzumondFire.Set(pos, aim, damage, NowWeaponHave->best_elem);

    dmg_check_wait++;

    if (dmg_check_wait >= 30) {
        dmg_check_wait = 0;
        SwordDmgCheck1(2.2f, 0);
        BtActStatus.frames_since_attack = 0;
    }

    if (GamePad.On(PadInput_OK) == 0) {
        EndAction();

        OzumondFire.Rset();
        wait_cnt = 10;
        dmg_check_wait = 0;
    }
}

namespace {

/* How far back the camera can go before something blocks its view. */
float camera_far_dist_limmit;

/**
 * Pulls the camera in to a point that blocks its view and swings it round a little.
 */
void CameraAutoMove(CCameraFollow *camera, float *position, float from, float to) {
    float reference[4];
    float offset[4];

    camera->GetRef(reference);
    offset[0] = position[0] - reference[0];
    offset[2] = position[2] - reference[2];
    offset[1] = 0.0f;
    camera->SetDistance(0.1f + DistVector(offset));

    float step = 0.0f;
    float over = camera->GetDistance() - camera_near_dist__2;

    if (over < step) {
        step = -over / 20.0f;
    }

    if (over > 0.0f) {
        step = over / 30.0f;
    }

    step = std::min(step, 2.0f);
    step = std::max(step, 0.05f);

    if (over < 0.0f) {
        step *= 2.0f;
    }

    NowCamera__3->SetAngle(atan2f(offset[0], offset[2]));

    if (from < to) {
        NowCamera__3->AddAngle(0.2f * -step);
    } else {
        NowCamera__3->AddAngle(0.2f * step);
    }

    if (camera->GetDistance() < 0.8f * camera_near_dist__2) {
        camera->AddHeight(1.0f);
    }
}

} // namespace

void autoCamTrial() {
    sceVu0FVECTOR eye;
    sceVu0FVECTOR look;
    sceVu0FVECTOR along;
    CBoxVu0       box;
    sceVu0FVECTOR pos;
    sceVu0FVECTOR at;
    sceVu0FVECTOR ref;
    sceVu0FVECTOR reach;
    sceVu0FVECTOR forward;
    sceVu0FVECTOR towards;
    int           hit_poly[32];
    float         hit_point[32][4];
    sceVu0FVECTOR facing;
    sceVu0FVECTOR facing_last;
    sceVu0FVECTOR stood;
    sceVu0FVECTOR chara;
    sceVu0FVECTOR near_chara;
    sceVu0FVECTOR locked;
    float         world[4][4];

    if (DebugStatus[5] == 0) {
        return;
    }

    NowCamera__3->GetPos(eye);
    NowCamera__3->GetRef(look);

    along[0] = look[0] - eye[0];
    along[1] = look[1] - eye[1];
    along[2] = look[2] - eye[2];
    along[3] = 1.0f;
    sceVu0Normalize(along, along);
    along[0] *= 30.0f;
    along[1] *= 30.0f;
    along[2] *= 30.0f;

    // Every polygon the floor puts near the camera goes into one list, which
    // the line from the eye to what it looks at is then tested against.
    WorkBuffer->used = 0;

    auto *poly = reinterpret_cast<CCPoly *>(WorkBuffer->Alloc(0x280));
    int   count = 0;

    box.max[0] = look[0] > eye[0] ? look[0] : eye[0];
    box.max[1] = look[1] > eye[1] ? look[1] : eye[1];
    box.max[2] = look[2] > eye[2] ? look[2] : eye[2];
    box.min[0] = look[0] < eye[0] ? look[0] : eye[0];
    box.min[1] = look[1] < eye[1] ? look[1] : eye[1];
    box.min[2] = look[2] < eye[2] ? look[2] : eye[2];

    if (NowDngMap->map_type != 1) {
        for (int i = 0; NowDngMap->parts[i].frame[0] != nullptr; i++) {
            CDungeonParts *part = &NowDngMap->parts[i];
            CFrame        *frame = part->camera_collision;

            if (frame != nullptr) {
                sceVu0CopyVector(eye, part->frame_offset[0]);

                auto turn = static_cast<int>(part->frame_turn[0]);

                turn += part->camera_collision_turn;

                if (turn > 3) {
                    turn -= 3;
                }

                if (turn == 3) {
                    turn = -1;
                }

                frame->SetRotation(0.0f, PI * (-90.0f * turn) / 180.0f, 0.0f);
                frame->SetPosition(eye);
                count += frame->PickUpNearPoly(&poly[count], box);
            }
        }
    } else {
        for (int j = 0; j < 20; j++) {
            for (int i = 0; i < 20; i++) {
                CDungeonMap *map = NowDngMap;
                MAP_CELL    *cell = &map->cells[i + j * 20];
                s32          parts_no = cell->parts_no;
                CFrame      *frame = parts_no == -1 ? nullptr : map->parts[parts_no].camera_collision;

                if (frame == nullptr) {
                    continue;
                }

                if (!(cell->camera_dist <= 240.0f)) {
                    continue;
                }

                int turn = cell->direction;

                turn += parts_no == -1 ? 0 : map->parts[parts_no].camera_collision_turn;

                if (turn > 3) {
                    turn -= 3;
                }

                if (turn == 3) {
                    turn = -1;
                }

                frame->SetRotation(0.0f, PI * (-90.0f * turn) / 180.0f, 0.0f);
                frame->SetPosition(160.0f * i, 0.0f, 160.0f * j);
                count += frame->PickUpNearPoly(&poly[count], box);
            }
        }
    }

    NowCamera__3->GetPos(pos);
    NowCamera__3->GetRef(ref);
    sceVu0SubVector(forward, ref, pos);
    sceVu0SubVector(reach, pos, forward);
    sceVu0SubVector(reach, pos, ref);
    sceVu0Normalize(reach, reach);
    sceVu0ScaleVectorXYZ(reach, reach, 80.0f);
    reach[0] += ref[0];
    reach[1] += ref[1];
    reach[2] += ref[2];

    int hits = CheckHits(poly, count, ref, reach, 32, hit_poly, hit_point, 1, 0);

    camera_far_dist_limmit = 80.0f;

    if (hits > 0) {
        camera_far_dist_limmit = DistVector(ref, hit_point[0]);
    }

    if (hits > 0) {
        int last = -1;

        sceVu0InnerProduct(forward, poly[hit_poly[0]].normal);

        for (int i = 0; i < hits; i++) {
            sceVu0SubVector(towards, hit_point[i], pos);

            if (sceVu0InnerProduct(forward, towards) < 0.0f) {
                break;
            }

            last = i;
        }

        if (last >= 0) {
            sceVu0CopyVector(facing, poly[hit_poly[last]].normal);

            if (sceVu0InnerProduct(forward, facing) > 0.0f) {
                if (last + 1 < hits) {
                    float away = DistVector(pos, hit_point[last + 1]);

                    if (away - DistVector(pos, hit_point[last]) < 0.0f) {
                        CameraAutoMove(NowCamera__3, hit_point[last + 1], 1000000.0f, 1000000.0f);
                    } else {
                        CameraAutoMove(NowCamera__3, hit_point[last], 1000000.0f, 1000000.0f);
                    }
                } else if (camera_far_dist_limmit >= camera_near_dist__2 && camera_far_dist_limmit < camera_far_dist__2) {
                    NowCamera__3->SetPos(hit_point[0]);
                } else {
                    CameraAutoMove(NowCamera__3, hit_point[last], 1000000.0f, 1000000.0f);
                }
            }
        } else {
            sceVu0CopyVector(facing_last, poly[hit_poly[0]].normal);

            if (sceVu0InnerProduct(forward, facing_last) < 0.0f) {
                CameraAutoMove(NowCamera__3, hit_point[0], 1000000.0f, 1000000.0f);
            }
        }
    }

    // The camera keeps its head above whatever floor is under it.
    sceVu0CopyVector(reach, pos);
    reach[1] += 20.0f;

    if (CheckHitVertical(poly, count, reach, -100.0f, at, 0) >= 0 && pos[1] - at[1] < 25.0f) {
        pos[1] = 25.0f + at[1];
        NowCamera__3->SetHeight(pos[1] - ref[1] - 0.01f);
    }

    sceVu0CopyVector(chara, CharaMain.pos);
    NowCamera__3->GetPos(stood);

    if (DistVector(chara, stood) < camera_up_near_dist) {
        NowCamera__3->AddHeight(0.5f);

        if (NowCamera__3->GetHeight() <= 1.6f) {
            NowCamera__3->SetHeight(1.6f);
        }
    } else {
        if (NowCamera__3->GetHeight() <= 1.6f) {
            NowCamera__3->SetHeight(1.6f);
        }

        if (!(NowCamera__3->GetHeight() <= 5.0f)) {
            float rate = 0.05f;
            float over = NowCamera__3->GetHeight() - 5.0f;
            float step = over * rate;

            step = std::max(step, 0.15f);

            if (!(step <= 0.5f)) {
                step = 0.5f;
            }

            NowCamera__3->AddHeight(-step);
        }
    }

    if (!(NowCamera__3->GetDistance() <= camera_far_dist__2)) {
        NowCamera__3->AddDistance(-(NowCamera__3->GetDistance() - camera_far_dist__2) / 10.0f);
    }

    if (NowCamera__3->GetDistance() < camera_near_dist__2) {
        NowCamera__3->AddDistance(-(NowCamera__3->GetDistance() - camera_near_dist__2) / 10.0f);
    }

    if (NowCamera__3->GetDistance() < camera_far_dist_limmit) {
        NowCamera__3->AddDistance(-(NowCamera__3->GetDistance() - camera_far_dist_limmit) / 10.0f);
    }

    if (BtActStatus.slow_walking == 0 && NowCamera__3->GetDistance() < camera_far_dist__2) {
        float turn = inputH1;

        inputH1 = inputH1 * (DistVector(ref_off) / 1.3f / 2.0f);

        if (!(inputH1 <= 0.1f)) {
            turn = std::max(turn, 0.4f);

            if (!(turn <= 1.0f)) {
                turn = 1.0f;
            }

            NowCamera__3->AddAngle(2.5f * (-DEG_TO_RAD * turn));
        }

        if (inputH1 < -0.1f) {
            if (!(turn <= -0.4f)) {
                turn = -0.4f;
            }

            turn = std::max(turn, -1.0f);

            NowCamera__3->AddAngle(2.5f * (-DEG_TO_RAD * turn));
        }
    }

    // A locked-on monster pulls the camera round behind the player.
    if (lockOnTargetFlag != 0) {
        sceVu0CopyVector(near_chara, CharaMain.pos);
        NowMonstorUnit->chara[lockOnTargetNo][0].GetPosition(locked);

        float away = DistVector(near_chara, locked);

        if (!(away <= 30.0f)) {
            CharaMain.frame->GetLWMatrix(world);

            auto want = static_cast<float>(atan2f(world[2][0], world[2][2]) - PI_D);

            if (!(want <= PI_SHORT)) {
                want -= TWO_PI_SHORT;
            }

            if (want < -PI_SHORT) {
                want += TWO_PI_SHORT;
            }

            float angle = NowCamera__3->GetAngle();
            float range = 70.0f * ((60.0f - away) / 30.0f);

            range += 20.0f;

            if (!(range < 90.0f)) {
                range = 90.0f;
            }

            range = std::max(range, 0.0f);

            if (!(away <= 80.0f)) {
                range = 10.0f;
            }

            if (AngleCmp(want, angle, DEG_TO_RAD_SHORT * range) != 0) {
                angle = AngleInterpolate(angle, want, EIGHTH_PI_SHORT, INTERPOLATE_STEP);
                NowCamera__3->SetAngle(angle);

                if (AngleCmp(want, angle, HALF_PI_SHORT) != 0) {
                    NowCamera__3->SetSpeed(8.0f);
                }
            }
        }
    }
}

void DelActiveItem(int slot) {
    s16 *slots = UserStatus->active_item;

    // The last use takes the item away; anything else just spends one.
    if (slots[slot + 3] == 1) {
        slots[slot] = -1;
        slots[slot + 3] = 0;

        if (activeItem.model[slot] != -1) {
            activeItem.models->DeleteModel(activeItem.model[slot]);
            activeItem.model[slot] = -1;
        }
    } else {
        slots[slot + 3]--;
    }
}

int Run_TrapCircle(MAP_TRAP_CIRCLE *trap) {
    if (trap == nullptr) {
        return;
    }

    CUserStatus *status = UserStatus;
    int          had = status->money;
    int          had_copy = status->money;
    s8           chara = status->cur_chara;
    WEAPON_HAVE *weapon = &status->chara_weapons[chara][status->equipped_weapon_slot[chara]];
    int          element = -1;
    int          se = 0;
    int          kind = trap->kind;
    s16          item_no = weapon->item_no;

    // The first two weapons of each character turn every trap into the stamina trap.
    if (item_no == defWeapon__6[chara] || item_no == defWeapon__6[chara] + 1) {
        kind = TRAP_CIRCLE_STAMINA;
    }

    switch (kind) {
        case TRAP_CIRCLE_STAMINA:
            BtSetStatusErr(AILMENT_STAMINA);
            se = 0xE1;
            break;
        case TRAP_CIRCLE_MONEY_UP: {
            had_copy *= 1.2f;
            int added = had_copy + 10;

            if (had + added >= 0xFFFF) {
                status->money = 0xFFFF;
            } else {
                status->money += added;
            }

            se = 0xE1;
            break;
        }
        case TRAP_CIRCLE_ABS_FULL:
            element = RGATE_ABS_FULL;
            se = 0xE1;
            break;
        case TRAP_CIRCLE_WHP_UP:
            element = RGATE_WHP_UP;
            se = 0xE1;
            break;
        case TRAP_CIRCLE_WHP_CURE:
            element = RGATE_WHP_CURE;
            se = 0xE1;
            break;
        case TRAP_CIRCLE_ALL_MONSTER_STAMINA:
            NowMonstorUnit->AllBin2();
            se = 0xE2;
            break;
        case TRAP_CIRCLE_MONEY_DOWN: {
            had_copy = static_cast<int>(had_copy - 0.2f * had_copy);

            had_copy = std::max(had_copy, 0);

            status->money = had_copy;
            se = 0xE2;
            break;
        }
        case TRAP_CIRCLE_STAT_DOWN:
            element = RGATE_STAT_DOWN;
            se = 0xE2;
            break;
        case TRAP_CIRCLE_WHP_DOWN:
            element = RGATE_WHP_DOWN;
            se = 0xE2;
            break;
        case TRAP_CIRCLE_WHP_QUARTER:
            element = RGATE_WHP_QUARTER;
            se = 0xE2;
            break;
        default:
            break;
    }

    if (element != -1) {
        WeaponDataChangeByRGate(weapon, element);
        SetWeaponAttachStatus(NowWeaponHave);
    }

    SndSePlay(se, -1, 0);
    return kind;
}

void LockOffTargte() {
    lockOnTargetDraw = 0;
    lockOnTargetNo = -1;
    lockOnTargetFlag = 0;
    targetCursorShiftNo = -1;
    targetCursorShiftRot = 0.0f;
    EnemyLifeGage.on = 0;
}

void DrawTargetLife() {
    CRect_i_ border;
    CRect_i_ ground;
    CRect_i_ life;

    if (EnemyLifeGage.on == 0) {
        return;
    }

    if (EnemyLifeGage.draw == 0) {
        return;
    }

    // The twelfth word of the options hides the gauge.
    if (static_cast<s32 *>(SaveData->GetConfigData())[11] != 0) {
        return;
    }

    s32 x = EnemyLifeGage.x - 32;

    EnemyLifeGage.y = (EnemyLifeGage.y >> 1) << 1;

    auto width = static_cast<s32>(64.0f / EnemyLifeGage.life_max * EnemyLifeGage.life);

    if (x >= 577 || x < 0) {
        return;
    }

    if (EnemyLifeGage.y >= 481 || EnemyLifeGage.y < 0) {
        return;
    }

    border.x = x - 1;
    border.y = EnemyLifeGage.y - 1;
    border.width = 66;
    border.height = 6;
    set2DSpriteC4(Vif1Packet, border, &ELifeB1, &ELifeB2, &ELifeB1, &ELifeB2);

    ground.x = x;
    ground.y = EnemyLifeGage.y;
    ground.width = 64;
    ground.height = 4;
    set2DSpriteC4(Vif1Packet, ground, &ELifeN1, &ELifeN1, &ELifeN1, &ELifeN1);

    life.x = x;
    life.y = EnemyLifeGage.y;
    life.width = width;
    life.height = 4;
    set2DSpriteC4(Vif1Packet, life, &ELife1, &ELife2, &ELife1, &ELife2);
}

namespace {

/* How long the lock-on cursor stays on screen, in frames. */
float targetCursorCnt;

} // namespace

void DrawtargetCursor(float *world, float width, float height, float alpha) {
    int       top_left[4];
    int       bottom_right[4];
    CTexture *texture = TexManager.GetTexture("d01e04", -1);

    setbilinear(0);
    world[3] = 1.0f;

    if (MGRotTransPers3DSprite(top_left, bottom_right, world, width, height, 0) != 0) {
        // The two corners arrive in the fixed-point screen space the sprite
        // calls take, so they come back to pixels here.
        top_left[0] -= 0x6C08;
        top_left[1] -= (GS_Y_OFFSET + 8);
        bottom_right[0] -= 0x6C08;
        bottom_right[1] -= (GS_Y_OFFSET + 8);
        top_left[0] >>= 4;
        top_left[1] >>= 3;
        bottom_right[0] >>= 4;
        bottom_right[1] >>= 3;

        static int rate = static_cast<int>(16.0f * alpha);

        set2DSprite(Vif1Packet, texture, CRect_i_(top_left[0], top_left[1], 0x10, 0x10), CRect_i_(0, 0x20, 0x10, 0x10), rate);
        set2DSprite(Vif1Packet, texture, CRect_i_(bottom_right[0], top_left[1], 0x10, 0x10), CRect_i_(0, 0, 0x10, 0x10), rate);
        set2DSprite(Vif1Packet, texture, CRect_i_(top_left[0], bottom_right[1], 0x10, 0x10), CRect_i_(0, 0x30, 0x10, 0x10), rate);
        set2DSprite(Vif1Packet, texture, CRect_i_(bottom_right[0], bottom_right[1], 0x10, 0x10), CRect_i_(0, 0x10, 0x10, 0x10), rate);
    }

    setbilinear(1);
    DrawTargetLife();
}

int SetNearLockOnTarget(int from, int nearest_only) {
    CUserStatus  *status = UserStatus;
    sceVu0FVECTOR at;
    sceVu0FVECTOR towards;
    sceVu0FVECTOR pos[16];
    float         dist[16];
    int           found[16];
    float         rate = CharaRangeRate(status);

    CharaFrame->GetRotation(at);

    sceVu0FVECTOR forward = {0.0f, 0.0f, 1.0f, 1.0f};
    sceVu0FMATRIX unit;
    sceVu0FMATRIX turn;
    int           order[16];
    int           screen[4];

    sceVu0UnitMatrix(unit);
    sceVu0RotMatrixY(turn, unit, at[1]);
    sceVu0ApplyMatrix(forward, turn, forward);
    sceVu0Normalize(forward, forward);

    int count = 0;

    sceVu0CopyVector(at, CharaFrame->position);

    for (int &monster : found) {
        monster = -1;
    }

    for (int i = 0; i < 16; i++) {
        MonstorPicupInner[i] = 0.0f;

        CMonstorUnit *monsters = NowMonstorUnit;

        if (monsters->monster[i].state != 2) {
            continue;
        }

        if (monsters->monster[i].hp <= 0) {
            continue;
        }

        if (monsters->monster[i].revealed == 0) {
            continue;
        }

        if (monsters->monster[i].lockon_enabled == 0) {
            continue;
        }

        monsters->chara[i][0].GetPosition(pos[i]);

        float away = DistVector(at, pos[i]);

        if (!(away < NowMonstorUnit->monster[i].lock_range * rate)) {
            continue;
        }

        towards[0] = pos[i][0] - at[0];
        towards[2] = pos[i][2] - at[2];
        towards[1] = 0.0f;
        towards[3] = 1.0f;
        sceVu0Normalize(towards, towards);

        MonstorPicupInner[i] = sceVu0InnerProduct(forward, towards);

        if (MonstorPicupInner[i] < 0.25f) {
            continue;
        }

        found[count] = i;
        dist[count] = away;
        count++;
    }

    if (count == 0) {
        LockOffTargte();
        return 0;
    }

    for (int &monster : order) {
        monster = -1;
    }

    // The nearest one that is still free goes into each place in turn.
    for (int place = 0; place < count; place++) {
        int nearest = 0;

        while (found[nearest] == -1) {
            nearest++;
        }

        float best = dist[nearest];

        for (int other = 0; other < count; other++) {
            if (other == place) {
                continue;
            }

            if (found[other] == -1) {
                continue;
            }

            if (dist[other] < best) {
                best = dist[other];
                nearest = other;
            }
        }

        order[place] = found[nearest];
        found[nearest] = -1;
    }

    int target = 0;

    if (nearest_only == 0) {
        if (count == 1) {
            target = order[0];
        } else {
            if (from >= count) {
                from = 0;
            }

            if (lockOnTargetNo == order[from]) {
                from++;
            }

            if (from >= count) {
                from = 0;
            }

            target = order[from];
        }
    } else {
        target = order[0];
    }

    if (target == -1) {
        LockOffTargte();
        return 0;
    }

    if (lockOnTargetNo == target && nearest_only == 0) {
        return count;
    }

    if (target != -1) {
        NowMonstorUnit->chara[target][0].GetPosition(at);
        at[1] += NowMonstorUnit->chara[target][0].body_height;
        cursorFrame->SetPosition(at);
        lockOnTargetDraw = 1;
        targetCursorCnt = 8.0f;
        at[3] = 1.0f;

        if (MGRotTransPers(screen, at, 0) != 0) {
            SetMonsterNameDrawFlag(1);
            MonsterNamePosSet((screen[0] - 0x6C00) >> 4, (screen[1] - GS_Y_OFFSET) >> 4);
            EnemyLifeGage.x = (screen[0] - 0x6C00) >> 4;
            EnemyLifeGage.y = ((screen[1] - GS_Y_OFFSET) >> 4) + 42;
        } else {
            SetMonsterNameDrawFlag(0);
            EnemyLifeGage.on = 0;
        }

        if (lockOnTargetNo != target) {
            DngMes2.mes_made = -1;
            MonsterNameMake(NowMonstorUnit->monster[target].name_no);
        }
    }

    lockOnTargetNo = target;
    return count;
}

void setTargetCursor(int on) {
    CUserStatus  *status = UserStatus;
    sceVu0FVECTOR target;
    sceVu0FVECTOR stood;
    sceVu0FVECTOR flat_stood;
    sceVu0FVECTOR flat_target;
    int           screen[4];

    float rate = CharaRangeRate(status);

    if (on != 0) {
        int keep = 1;

        if (NowMonstorUnit->monster[lockOnTargetNo].state != 2 || NowMonstorUnit->monster[lockOnTargetNo].hp <= 0) {
            keep = 0;
        }

        if (NowMonstorUnit->monster[lockOnTargetNo].lockon_enabled == 0) {
            keep = 0;
        }

        sceVu0CopyVector(stood, CharaFrame->position);
        NowMonstorUnit->chara[lockOnTargetNo][0].GetPosition(target);

        float away = DistVector(stood, target);

        if (!(away < NowMonstorUnit->monster[lockOnTargetNo].lock_range * rate)) {
            keep = 0;
        }

        // A monster the player stands well above stays locked on only while it is
        // near enough on the floor.
        if (NowMonstorUnit->monster[lockOnTargetNo].kind != MONSTER_KIND_NO_LOCK_ON) {
            sceVu0CopyVector(flat_stood, stood);
            sceVu0CopyVector(flat_target, target);
            flat_target[1] = 0.0f;
            flat_stood[1] = 0.0f;

            if (DistVector(flat_stood, flat_target) <= 8.0f + NowMonstorUnit->monster[lockOnTargetNo].body_radius && !(stood[1] <= 10.0f + target[1])) {
                keep = 0;
            }

            float drop = stood[1] - target[1];

            drop = drop < 0.0f ? -drop : drop;

            if (!(drop < 80.0f)) {
                keep = 0;
            }
        }

        target[1] += 8.0f;
        target[3] = 1.0f;

        if (NowMonstorUnit->monster[lockOnTargetNo].lockon_frame != nullptr) {
            sceVu0CopyVector(target, NowMonstorUnit->monster[lockOnTargetNo].lockon_position);
        }

        sceVu0CopyVector(BtActStatus.target_position, target);

        float width = (12.0f + targetCursorCnt) * NowMonstorUnit->monster[lockOnTargetNo].lockon_scale_x;
        float height = (6.0f + targetCursorCnt / 2.0f) * NowMonstorUnit->monster[lockOnTargetNo].lockon_scale_y;

        if (NowMonstorUnit->monster[lockOnTargetNo].name_no > 0 && NowMonstorUnit->monster[lockOnTargetNo].kind != MONSTER_KIND_NO_LOCK_ON) {
            EnemyLifeGage.life_max = NowMonstorUnit->monster[lockOnTargetNo].max_hp;
            EnemyLifeGage.life = NowMonstorUnit->monster[lockOnTargetNo].hp;
            EnemyLifeGage.on = 1;
            EnemyLifeGage.x = 0x280;
            EnemyLifeGage.y = 0;
        }

        target[1] += 10.0f;
        target[3] = 1.0f;

        if (MGRotTransPers(screen, target, 0) != 0) {
            MonsterNameMake(NowMonstorUnit->monster[lockOnTargetNo].name_no);
            MonsterNamePosSet((screen[0] - 0x6C00) >> 4, (screen[1] - GS_Y_OFFSET) >> 4);
            EnemyLifeGage.x = (screen[0] - 0x6C00) >> 4;
            EnemyLifeGage.y = ((screen[1] - GS_Y_OFFSET) >> 4) + 42;
            SetMonsterNameDrawFlag(1);
        } else {
            SetMonsterNameDrawFlag(0);
            EnemyLifeGage.on = 0;
        }

        DrawtargetCursor(BtActStatus.target_position, width, height, targetCursorCnt);

        // Steve says something about the monster now and then, but only while the
        // player holds a weapon that talks to him.
        s16 weapon = NowWeaponHave->item_no;

        if ((weapon == ITEM_WEAPON_STEVE || weapon == ITEM_WEAPON_SUPER_STEVE) && lockOnTargetFlag != 0 && NowMonstorUnit->monster[lockOnTargetNo].name_no > 0 && static_cast<int>(100.0f * static_cast<float>(rand()) / 2147483648.0f) < 3) {
            DngMessMan.SetSteevMes(NowMonstorUnit->monster[lockOnTargetNo].name_no * 10 + 4000);
        }

        if (keep != 0) {
            if (targetCursorCnt > 0.0f) {
                targetCursorCnt -= 0.5f;
            } else {
                targetCursorCnt = 0.0f;
            }
        } else {
            if (targetCursorCnt < 8.0f) {
                targetCursorCnt += 0.25f;
            } else {
                SndSePlay(MENU_SOUND_REFUSE, -1, 0);
                targetCursorShiftNo = 0;
                targetCursorShiftRot = 0;
                lockOnTargetFlag = 0;
                lockOnTargetNo = -1;
                EnemyLifeGage.on = 0;
                return;
            }
        }

        return;
    }

    targetCursorShiftRot = SetNearLockOnTarget(0, 1);
}

void unitBlowActionRot(float *velocity) {
    sceVu0FVECTOR rotation;

    CharaFrame->GetRotation(rotation);
    rotation[1] = atan2f(velocity[0], velocity[2]);
    rotation[1] -= PI;

    if (rotation[1] <= -PI) {
        rotation[1] += TWO_PI;
    }

    CharaFrame->SetRotation(rotation[0], rotation[1], rotation[2]);
}

float setUnitDamageColor(int hit) {
    static float dmgColor = 128.0f;
    static float dmgVec = 0.0f;
    static int   dmgFlag;

    if (hit != 0) {
        dmgFlag = 1;
        dmgVec = 0.0f;
    }

    if (dmgFlag != 0) {
        dmgVec += EIGHTH_PI;
        dmgColor = 128.0f + 80.0f * sinf(dmgVec);

        // The flash runs for three full turns of the sine and then stops.
        if (dmgVec >= 9.424778f) {
            dmgFlag = 0;
            dmgColor = 128.0f;
        }
    }

    return dmgColor;
}

namespace {

/* How fast the ambient-colour animation runs. */
float unitAmbientAnime_Speed;

/* How far through the ambient-colour animation the player's model is. */
float unitAmbientAnime_Count;

/* How much longer the ambient-colour animation keeps repeating. */
s32 unitAmbientAnime_Timer;

/* Whether the ambient-colour animation is running on the player's model. */
s32 unitAmbientAnime_flag;

} // namespace

void RsetUnitAmbientAnime() {
    unitAmbientAnime_flag = 0;
}

void setUnitAmbientAnime(float speed, float time, float red, float green, float blue) {
    unitAmbientAnime_rgb[0] = red;
    unitAmbientAnime_rgb[1] = green;
    unitAmbientAnime_rgb[2] = blue;
    unitAmbientAnime_Count = 0;
    unitAmbientAnime_Speed = speed;
    unitAmbientAnime_Timer = static_cast<s32>(time);
    unitAmbientAnime_flag = 1;
}

int unitAmbientAnime(float *colour) {
    if (unitAmbientAnime_flag == 0) {
        return 0;
    }

    colour[0] = 64.0f + unitAmbientAnime_rgb[0] * sinf(unitAmbientAnime_Count);
    colour[1] = 64.0f + unitAmbientAnime_rgb[1] * sinf(unitAmbientAnime_Count);
    colour[2] = 64.0f + unitAmbientAnime_rgb[2] * sinf(unitAmbientAnime_Count);
    colour[3] = 128.0f;

    unitAmbientAnime_Count += PI / unitAmbientAnime_Speed;

    if (unitAmbientAnime_Count > PI) {
        unitAmbientAnime_Timer--;

        if (unitAmbientAnime_Timer <= 0) {
            unitAmbientAnime_flag = 0;
        } else {
            unitAmbientAnime_Count -= PI;
            unitAmbientAnime_Timer--;
        }
    }

    return 1;
}

int LoadStartLogo(int map) {
    char *dungeon_name[7] = {
        "dname00.img",
        "dname01.img",
        "dname02.img",
        "dname03.img",
        "dname04.img",
        "dname05.img",
        "dname06.img",
    };
    char *floor_texture[7] = {
        "floor00",
        "floor01",
        "floor02",
        "floor03",
        "floor04",
        "floor05",
        "floor06",
    };
    // Only the one texture is loaded; the nullptr after it ends the list.
    std::array<char *, 5> files{};

    files[0] = NameExchg(dungeon_name[map], 0);
    BtStartLogoBuffer.used = 0;

    int size = LoadTempTexture(files.data(), 8, reinterpret_cast<char *>(BtStartLogoBuffer.base));
    int blocks = (((size >> 6) + 1) << 6) >> 4;

    BtStartLogoBuffer.Alloc(blocks);

    char *name = floor_texture[map];

    TEX_Floor1 = TexManager.GetTexture(name, -1);

    const std::string_view floor = name;

    std::ranges::copy(floor, floor_name);
    floor_name[floor.size()] = '\0';
    return blocks;
}

// clang-format off
/* The names of the seven dungeons. */
char *MapInfoNameArea[7] = {
    "Cave of the Sacred Beast", "Forest of the Own", "Sunken Ship", "Shrine of Sun and Moon", "MoonOcean",
    "DarkHeven Castle", "Deamon Shaft",
};
// clang-format on

void FloorTitleOn() {
    rogoAlphaA[2] = 0;
    rogoSwitch2 = 1;
    rogoY3 = -96;
    startCnt2 = 0;
}

void SetMIniMapStatus(int on) {
    switch (on) {
        case 0:
            infoMapOld = infoMap;
            infoMap = 0;
            break;
        case 1:
            infoMap = infoMapOld;
            break;
        default:
            break;
    }
}

void ResetStatusInfo() {
    BtActStatus.action_on = 0;
    BtActStatus.main_motion = -1;
    BtActStatus.hand_motion = -1;
    BtActStatus.motion_no = 0;
    BtActStatus.jumping = 0;
    BtActStatus.gauge_flash = 0;
    BtActStatus.recoil_frames = 0;
    BtActStatus.invincible_frames = 0;
    BtActStatus.charge_level = 0;
    BtActStatus.guard_mode = 0;
    BtActStatus.can_act = 1;
    BtActStatus.unk_066 = 1;
    BtActStatus.slow_walking = 0;
    BtActStatus.camera_hold = 0;
    BtActStatus.action_busy = 0;
    BtActStatus.movement_locked = 0;
    BtActStatus.action_no = 0;
    BtActStatus.frames_since_attack = 3600;
    BtActStatus.combo_window = 0;
    BtActStatus.combo_queued = 0;
    BtActStatus.move_power = 0.0f;
    BtActStatus.move_power_decay = 0.0f;
    BtActStatus.camera_shake_frames = 0;
    BtActStatus.refill_frames = 10;
    CMonUnitHold = 0;
    CEffectHold = 0;
    BtActStatus.hud_shake_frames = 0;
    BtActStatus.hud_shake_y = 0;
    BtActStatus.event_block_frames = 0;
}

void ResetMovePower() {
    BtActStatus.move_power = 0.0f;
    BtActStatus.move_power_decay = 0.0f;
}

void LoaderInit() {
    BufferAllClear();
    MGSetBGColor(32.0f, 96.0f, 12.0f, 128.0f);
    TexManager.Initialize(0x3FE0);
    TexManager.EnterTextureFile(texdata2);
    TexManager.LoadTextureBlock(-1, read_buffer);

    CDbgMsg.texture_name = "dbgwork";
    CDbgMsg.x = 32;
    CDbgMsg.y = 16;
    CDbgMsg.width = 256;
    CDbgMsg.height = 224;
    CDbgMsg.alpha = 96;
    EdEventInfo.map_jump_bgm_play = true;
}

int LoaderLoop() {
    int        chosen = 0;
    static int nowCursor = 0;

    std::string menu = "- MapInfomationFile Loader -\n";

    for (int i = 0; i < 7; i++) {
        std::string_view name = MapInfoNameArea[i] != nullptr ? MapInfoNameArea[i] : "";
        std::string_view marker = nowCursor == i ? ">>" : "  ";

        menu += std::format("{}[{:2}] {}\n", marker, i + 1, name);
    }

    // The overlay's buffer keeps a terminator, so overlong text is cut one byte short of its size.
    std::size_t length = std::min(menu.size(), sizeof(CDbgMsg.text) - 1);
    std::ranges::copy_n(menu.begin(), static_cast<std::ptrdiff_t>(length), std::begin(CDbgMsg.text));
    CDbgMsg.text[length] = '\0';
    CDbgMsg.length = static_cast<int>(length);

    if (GamePad.Down(PAD_UP) != 0 && nowCursor != 0) {
        nowCursor--;
    }

    if (GamePad.Down(PAD_DOWN) != 0 && nowCursor != 6) {
        nowCursor++;
    }

    if (GamePad.Down(PAD_CIRCLE) != 0) {
        return 2;
    }

    if (GamePad.Down(PAD_START) != 0 || GamePad.Down(PAD_CROSS) != 0) {
        selectMapNo = nowCursor;
        main_select_menu_no = nowCursor;
        MapJump(selectMapNo + 200, -1);
        MGSetBGColor(0.0f, 0.0f, 0.0f, 128.0f);
        chosen = 1;
    }

    TexManager.ReloadTexture(Vif1Packet, 12);
    CDbgMsg.Draw();
    return chosen;
}

void InitEyeCamera() {
    sceVu0FVECTOR pos;

    viewAngleH__2 = CharaMain.GetRotation()->y;
    viewAngleV__2 = 0.0f;
    sceVu0CopyVector(pos, CharaMain.pos);
}

void EyeCamera() {
    sceVu0FVECTOR pos;
    sceVu0FVECTOR ref;
    sceVu0FMATRIX rotation;
    sceVu0FMATRIX unit;

    auto  keyboard = InputGetKeyboardMovement();
    bool  walking = keyboard.x || keyboard.y;
    float stick_x = walking ? 0.0f : GamePad.GetLXf();
    float stick_y = walking ? 0.0f : -GamePad.GetLYf();

    if (stick_x > 0.0f) {
        float rate = 0.02f;

        viewAngleH__2 -= stick_x * rate;

        if (viewAngleH__2 < -PI) {
            viewAngleH__2 += TWO_PI;
        }
    }

    if (stick_x < -0.0f) {
        float rate = 0.02f;

        viewAngleH__2 -= stick_x * rate;

        if (viewAngleH__2 > PI) {
            viewAngleH__2 -= TWO_PI;
        }
    }

    if (stick_y > 0.0f && viewAngleV__2 < 0.65f) {
        float rate = 0.02f;

        viewAngleV__2 += stick_y * rate;
    }

    if (stick_y < -0.0f && viewAngleV__2 > -1.0f) {
        float rate = 0.02f;

        viewAngleV__2 += stick_y * rate;
    }

    viewAngleH__2 = MouseLookEyeAngleH(viewAngleH__2);
    viewAngleV__2 = MouseLookEyeAngleV(viewAngleV__2);

    ref[0] = 0.0f;
    ref[1] = 0.0f;
    ref[2] = 10.0f;
    ref[3] = 0.0f;

    sceVu0UnitMatrix(unit);
    sceVu0RotMatrixX(rotation, unit, viewAngleV__2);
    sceVu0RotMatrixY(rotation, rotation, viewAngleH__2);
    sceVu0ApplyMatrix(ref, rotation, ref);
    sceVu0CopyVector(pos, CharaMain.pos);

    CUserStatus *status = UserStatus;
    float        height = CharaHeight(status);

    pos[1] += height - 1.5f;
    ref[0] += pos[0];
    ref[1] += pos[1];
    ref[2] += pos[2];

    NowCamera__3->SetPos(pos);
    NowCamera__3->SetRef(ref);
}

} // namespace dc::game
