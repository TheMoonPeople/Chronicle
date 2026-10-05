#pragma helper_mask_gpr 0x30
#pragma helper_mask_fpr 0x1000
#pragma name_counter 885
#ifndef PAL
#pragma argument_flag_free 3600, 3601, 3602, 3603, 3604, 3605, 3606, 3607, 3608, 3609
#pragma argument_flag_free 3610, 3611, 3612, 3613, 3614, 3615, 3616, 3617, 3618, 3619
#pragma argument_flag_free 3620, 3621, 3622, 3623, 3624, 3625, 3626, 3627, 3628, 3629
#pragma argument_flag_free 3630, 3631, 3632, 3633, 3634, 3635, 3636, 3637, 3638, 3639
#pragma argument_flag_free 3640, 3641, 3642, 3643, 3644, 3645, 3646, 3647, 3648, 3649
#pragma argument_flag_free 3650, 3651, 3652, 3653, 3654, 3655, 3656, 3657, 3658, 3659
#pragma argument_flag_free 3660, 3661, 3662, 3663, 3664, 3665, 3666, 3667, 3668, 3669
#pragma argument_flag_free 3670, 3671, 3672, 3673, 3674, 3675, 3676, 3677, 3678, 3679
#pragma argument_flag_free 3680, 3681, 3682, 3683, 3684, 3685, 3686, 3687, 3688, 3689
#pragma argument_flag_free 3690, 3691, 3692, 3693, 3694, 3695, 3696, 3697, 3698, 3699
#pragma argument_flag_free 3700, 3701, 3702, 3703, 3704, 3705, 3706, 3707, 3708, 3709
#pragma argument_flag_free 3710, 3711, 3712, 3713, 3714, 3715, 3716, 3717, 3718, 3719
#pragma argument_flag_free 3720, 3721, 3722, 3723, 3724, 3725, 3726, 3727, 3728, 3729
#pragma argument_flag_free 3730, 3731, 3732, 3733, 3734, 3735, 3736, 3737, 3738, 3739
#pragma argument_flag_free 3740, 3741, 3742, 3743, 3744, 3745, 3746, 3747, 3748, 3749
#pragma argument_flag_free 3750, 3751, 3752, 3753, 3754, 3755, 3756, 3757, 3758, 3759
#pragma argument_flag_free 3760, 3761, 3762, 3763, 3764, 3765, 3766, 3767, 3768, 3769
#pragma argument_flag_free 3770, 3771, 3772, 3773, 3774, 3775, 3776, 3777, 3778, 3779
#pragma argument_flag_free 3780, 3781, 3782, 3783, 3784, 3785, 3786, 3787, 3788, 3789
#pragma argument_flag_free 3790, 3791, 3792, 3793, 3794, 3795, 3796, 3797, 3798, 3799
#pragma argument_flag_free 3800, 3801, 3802, 3803, 3804, 3805, 3806, 3807, 3808, 3809
#pragma argument_flag_free 3810, 3811, 3812, 3813, 3814, 3815, 3816, 3817, 3818, 3819
#pragma argument_flag_free 3820, 3821, 3822, 3823, 3824, 3825, 3826, 3827, 3828, 3829
#pragma argument_flag_free 3830, 3831, 3832, 3833, 3834, 3835, 3836, 3837, 3838, 3839
#pragma argument_flag_free 3840, 3841, 3842, 3843, 3844, 3845, 3846, 3847, 3848, 3849
#pragma argument_flag_free 3850, 3851, 3852, 3853, 3854, 3855, 3856, 3857, 3858, 3859
#pragma argument_flag_free 3860, 3861, 3862, 3863, 3864, 3865, 3866, 3867, 3868, 3869
#pragma argument_flag_free 3870, 3871, 3872, 3873, 3874, 3875, 3876, 3877, 3878, 3879
#pragma argument_flag_free 3880
#pragma argument_flag_ones 154, 451, 493, 573, 574, 575, 591, 592, 593, 594
#pragma argument_flag_ones 1155, 1173, 1174, 1175, 1176, 1239, 1249, 1250, 1285, 1289
#pragma argument_flag_ones 1314, 1318, 1326, 1330, 1339, 1364, 1366, 1411, 1456, 1459
#pragma argument_flag_ones 1460, 1464, 1471, 1473, 1489, 1491, 1505, 1755, 1756, 1810
#pragma argument_flag_ones 1811, 1815, 1816, 1817, 1877, 1880, 1906, 1937, 1988, 2045
#pragma argument_flag_ones 2197, 2243, 2244, 2362, 2411, 2426, 2433, 2444, 2450, 2473
#pragma argument_flag_ones 2474, 2475, 2476, 2477, 2521, 2578, 2581, 2582, 2614, 2624
#pragma argument_flag_ones 2700, 2897, 3024, 3309, 3337, 3513, 3515, 3587, 3589, 3684
#pragma argument_flag_ones 3685, 3687, 3688, 3891, 3911, 3922, 3943, 3946, 3947, 3999
#pragma argument_flag_ones 4058, 4069, 4077, 4097, 4100, 4135, 4142, 4157, 4160, 4185
#pragma argument_flag_ones 4213, 4220, 4234, 4253, 4265, 4269, 4270, 4340, 4346, 4355
#pragma argument_flag_ones 4367, 4393, 4415, 4422, 4429, 4701, 4702, 4703, 4746, 4749
#pragma argument_flag_ones 4810, 4811, 4813
#pragma argument_flag 0
#endif

#include "dun/gameloop.hpp"

#include <libvu0.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

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
#include "texture.hpp"
#include "textureanime.hpp"
#include "userstatus.hpp"
#include "vutext.hpp"
#include "weaponeffect.hpp"
#include "weaponelement.hpp"
#include "wind.hpp"

/**
 * Defines the life gauge that the locked-on enemy draws above itself.
 */
struct ENEMY_LIFE_GAGE {
    s32 life_max; /**< Specifies the life the enemy has at full health. */
    s32 life;     /**< Specifies the life the enemy has now. */
    s32 x;        /**< Specifies the left edge of the gauge, in screen pixels. */
    s32 y;        /**< Specifies the top edge of the gauge, in screen pixels. */
    s32 on;       /**< Whether an enemy is locked on. */
    s32 draw;     /**< Whether the gauge draws this frame. */
};

STATIC_ASSERT(sizeof(ENEMY_LIFE_GAGE) == 0x18);

/**
 * Names where the item the player is about to throw will land.
 */
struct BOMB_INFO {
    sceVu0FVECTOR pos; /**< Where the thrown item lands. */
    s32           unk_10;
    s32           aiming;     /**< Whether the player is still walking the landing point around. */
    s32           throw_step; /**< Step of the throw: 1 until the item leaves the hand, 2 while the motion ends, -1 when idle. */
    s32           unk_1C;
};

STATIC_ASSERT(sizeof(BOMB_INFO) == 0x20);

/**
 * Draws a window of debug text over the picture.
 */
class CDebugFont {
public:
    s32   x;            /**< Distance of the left edge from the left of the screen. */
    s32   y;            /**< Distance of the top edge from the top of the screen. */
    s32   width;        /**< Distance from the left edge to the right edge. */
    s32   height;       /**< Distance from the top edge to the bottom edge. */
    char *texture_name; /**< Name of the texture the window draws its glyphs from. */
    s32   alpha;        /**< Alpha the window composites its texture with. */
    s32   length;       /**< Characters the text holds now. */
    char  text[0x200];  /**< The text the window draws. */

    /**
     * @mangled Draw__10CDebugFontFv
     * @address 0x13DF40
     * @size 0x59C
     * @unknownret
     */
    void Draw();
};

STATIC_ASSERT(sizeof(CDebugFont) == 0x21C);

/* The weapon each character starts with. */
s32 defWeapon__6[6] = {ITEM_WEAPON_DAGGER_BROKEN, ITEM_WEAPON_WOODENSLINGSHOT_BROKEN, ITEM_WEAPON_MALLET_BROKEN, ITEM_WEAPON_GOLD_RING_BROKEN, ITEM_WEAPON_FIGHTING_STICK_BROKEN, ITEM_WEAPON_MACHINE_GUN_BROKEN};

/**
 * How far one character reaches when the game picks what to lock on to.
 */
static inline float CharaRangeRate(CUserStatus *status) {
    float range_rate[6] = {1.2f, 1.4f, 1.1f, 1.5f, 1.0f, 1.8f};

    return range_rate[status->cur_chara];
}

/**
 * How tall one character stands, and how wide their collision reaches.
 */
static inline float CharaHeight(CUserStatus *status) {
    float chara_height[6] = {16.0f, 14.0f, 16.0f, 16.0f, 18.0f, 15.0f};

    return chara_height[status->cur_chara];
}

/**
 * Names what the dungeon hands the Georama editor to follow.
 */
struct GAME_ENV {
    CFrame  *frame;  /**< Frame that the editor follows. */
    CCamera *camera; /**< Camera that the editor draws through. */
};

/**
 * Turns the stick into the motion the player's character walks with.
 *
 * @mangled keyCtrl__FffP11MOTION_INFO
 */
int keyCtrl(float x, float y, MOTION_INFO *motion);

/**
 * Puts one system message on the screen.
 *
 * @mangled SetSystemMes__FiiiiPiPi
 */
void SetSystemMes(int mes_no, int frames, int x, int y, int *result, int *unk);

/**
 * Tells the player that only Toan can take an atla.
 *
 * @mangled NotGetAtraMes__Fii
 */
void NotGetAtraMes(int chara, int frames);

/**
 * Tells the player that a character has died.
 *
 * @mangled DeadMes__Fii
 */
void DeadMes(int chara, int frames);

/**
 * Tells the player that the whole party has died.
 *
 * @mangled AllDeadMes__Fi
 */
void AllDeadMes(int frames);

/**
 * Runs the debug information overlay and gives back what the player chose.
 *
 * @mangled DebugInfomationIF__Fv
 */
int DebugInfomationIF();

/**
 * Puts every sound effect sequence the dungeon is playing back to silence.
 *
 * @mangled SndSeSeqAllStop__Fv
 */
void SndSeSeqAllStop();

/**
 * @mangled BtMiniChrSelect_Loop__Fv
 */
int BtMiniChrSelect_Loop();

/**
 * @mangled BtEscape_Loop__Fv
 */
int BtEscape_Loop();

/**
 * Starts the large treasure box opening.
 *
 * @mangled BtGetTreasureboxBig_Init__Fv
 */
void BtGetTreasureboxBig_Init();

/**
 * @mangled BtGetTreasureboxBig_Loop__Fv
 */
int BtGetTreasureboxBig_Loop();

/**
 * Starts the small treasure box opening.
 *
 * @mangled BtGetTreasureboxSmall_Init__Fi
 */
void BtGetTreasureboxSmall_Init(int dungeon);

/**
 * @mangled BtGetTreasureboxSmall_Loop__Fv
 */
int BtGetTreasureboxSmall_Loop();

/**
 * Starts the short atla pickup.
 *
 * @mangled BtAtraGetShort_Init__Fv
 */
void BtAtraGetShort_Init();

/**
 * @mangled BtAtraGetShort_Loop__Fii
 */
int BtAtraGetShort_Loop(int dungeon, int floor);

/**
 * @mangled BtGetGateKey_Loop__Fv
 */
int BtGetGateKey_Loop();

/**
 * Plays the action that each character fires with the action button.
 *
 * @mangled ToanKey_On__Fv
 */
void ToanKey_On();

/**
 * @mangled ToanKey_Play__Fv
 */
void ToanKey_Play();

/**
 * @mangled GoroKey_On__Fv
 */
void GoroKey_On();

/**
 * @mangled GoroKey_Play__Fv
 */
void GoroKey_Play();

/**
 * @mangled UngagaKey_On__Fv
 */
void UngagaKey_On();

/**
 * @mangled UngagaKey_Play__Fv
 */
void UngagaKey_Play();

/**
 * Takes every system message off the screen.
 *
 * @mangled ClearSystemMes__Fv
 */
void ClearSystemMes();

/**
 * Reads the models of the items the player can run out of a pack file.
 *
 * @mangled BtSetActiveItemModel__FPUi
 */
void BtSetActiveItemModel(unsigned int *pack);

/**
 * Forgets every gate key the player carried into the dungeon.
 *
 * @mangled ClearGateKeyStack__Fv
 */
void ClearGateKeyStack();

/**
 * @mangled SndAmbientSetVol__Fi
 */
void SndAmbientSetVol(int volume);

/**
 * Puts the battle music back to the start.
 *
 * @mangled BtBattleMusic_Init__Fv
 */
void BtBattleMusic_Init();

/**
 * Builds the list of Atlamillia the dungeon lays out.
 *
 * @mangled BtAtraListMake__Fi
 */
void BtAtraListMake(int dungeon);

/**
 * Puts the debug information overlay back to the start.
 *
 * @mangled DebugInfomationInit__Fv
 */
void DebugInfomationInit();

/**
 * @mangled LoadData__Fv__3
 * @address 0x1DBA930
 * @size 0x6A0
 * @unknownret
 * @note disambiguated by disassembler ("__3" suffix); real retail name has no suffix
 */
static void LoadData();

/* The overlay's large data, in the order retail's link lays it out. */

/* The battle status of every character in the party. */
BT_ACT_STATUS BtActStatus;

/* The cameras the dungeon draws through. */
CCameraFollow MainCamera__4(20.0f, 15.0f, 0.0f, 8.0f);
CCameraFollow SubCamera(60.0f, 20.0f, 0.0f, 4.0f);

/* The fade the dungeon draws over the picture. */
CDispCtrl DispFade__3;

/* The dungeon floor that the player is on. */
CDungeonMap MainDungeonMap;

/* The back dungeon's floor. */
CDungeonMap UraDungeonMap;

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

/* The effect the player's own character shoots. */
CSHOT_EFFECT CharaMainEffect;

/* The effect the player's own character breaks things with. */
CSHOT_EFFECT CharaMainEffectCrash;

/* The character that the player controls. */
CCharacter CharaMain;

/* The hand that the player's character holds an item out with. */
CCharacter CharaHand;

/* The weapon models the player's character can hold. */
CCharacter MainWeapon;
CCharacter DefaultWeapon;
CCharacter CrashWeapon;

/* The weapon that the player has equipped. */
WEAPON_HAVE WeaponHave;

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
CCharacter    Trap_Circle;
CTexAnimeData Trap_Circle_TexAnim[16];

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

/* The wind that blows across the dungeon. */
CWind kaze;

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

/* How square on to the player each monster of the floor stands. */
float MonstorPicupInner[36];

/* The damage numbers the dungeon draws. */
CHitValue HitValue[32];

/* The shots the player's character has in the air. */
CSHOT ShotData;

/* The shot effects the dungeon can run. */
CSHOT_EFFECT_PACK ShotEffect;

/* The machine gun that Osmond's rapid-fire action shoots. */
CSHOT_MACHINGUN OzumondShot;

/* The flame that Osmond's flamethrower action shoots. */
CSHOT_FIREBAR OzumondFire;

/* The bullets that Osmond's machine gun shoots. */
CHIT_MACHINGUN_EFFECT OzumondShotEffect;

/* The collisions of the floor the player is on. */
CCollisionData CColData;

/* The bombs the player has thrown. */
CItemBombEffect CBomb__2[3];

/* The shock wave the dungeon can run. */
CShockWave ShockWave;

/* How fast the player fell the frame before. */
sceVu0FVECTOR veloOld;

/* The texture animations every dungeon texture runs. */
CTextureAnime BtTexAnime;
CTexAnimeData BtTexAnimeData[96];

/* The texture animations of each character that walks the dungeon. */
CTexAnimeData BtNPCTexAnimeData[4][32];

/* One texture animation for every texture the player's model draws. */
CTexAnimeData CharaMainTexAnim[64];

/* The buffers the dungeon loads its models, messages and scripts into. */
CDataAlloc2<1> MainModelBuffer(-1);
CDataAlloc2<1> MapModelBuffer(-1);
CDataAlloc2<1> CharaModelBuffer(-1);
CDataAlloc2<1> WeaponModelBuffer(-1);
CDataAlloc2<1> WEffectModelBuffer(-1);
CDataAlloc2<1> MasekiModelBuffer(-1);
CDataAlloc2<1> BtMesBuffer(-1);
CDataAlloc2<1> BtSteebMesBuffer(-1);
CDataAlloc2<1> BtStartLogoBuffer(-1);
CDataAlloc2<1> MonstorModelBuffer(-1);

/* Script working memory, one block per monster on the floor. */
CDataAlloc2<1> MonstorScriptBuffer[16];

/* The buffers the dungeon's item cache and battle scripts run out of. */
CDataAlloc2<1> BtItemCashArea[6];
CDataAlloc2<1> BtScriptWorkBuffer(-1);
CDataAlloc2<1> BtCashBuffer(-1);
CDataAlloc2<1> BtScriptBuffer(-1);
CDataAlloc2<1> BtSystemScriptFileBuffer(-1);

/* The name of the texture that the floor number draws with. */
char floor_name[32];

/* Where the camera looks, relative to what it follows. */
sceVu0FVECTOR ref_off;

/* Where Ruby's shot starts, and the way it flies. */
sceVu0FVECTOR ruby_effect_pos;
sceVu0FVECTOR ruby_effect_vec;

/* The colour the ambient-colour animation drives the player's model to. */
float unitAmbientAnime_rgb[3];

/* The debug message overlay. */
CDebugFont CDbgMsg;

/* The overlay's initialised data, in the order retail's link lays it out. */

// clang-format off
/* The effect each playable character, and each of Ruby's and Osmond's forms, draws. */
BT_SHOT_EFFECT MyEntryEffect_TOAN = {
    "c01_fuusya", 0, 0,
    {0.0f, 0.0f, 0.0f, 0.0f}, {20.0f, 0.0f, 0.0f, 0.0f},
    160, 8, 8, 2, 2,
    {0, -1, -1, -1}, -1, {0, 0}, 0.0f, 0,
    {-1, -1, -1, -1},
};
// clang-format on

// clang-format off
BT_SHOT_EFFECT MyEntryEffect_SYAO = {
    "mgan01", 0, 0,
    {0.0f, 0.0f, 0.0f, 0.0f}, {1.5f, 0.0f, 0.0f, 0.0f},
    160, 8, 0, 2, 2,
    {0, -1, -1, -1}, -1, {0, 0}, 0.0f, 0,
    {-1, -1, -1, -1},
};
// clang-format on

// clang-format off
BT_SHOT_EFFECT MyEntryEffect_GORO = {
    "c06a_tameex", 0, 0,
    {0.0f, 0.0f, 0.0f, 0.0f}, {15.0f, 0.0f, 0.0f, 0.0f},
    160, 8, 0, 2, 2,
    {0, -1, -1, -1}, -1, {0, 0}, 0.0f, 0,
    {-1, -1, -1, -1},
};
// clang-format on

// clang-format off
BT_SHOT_EFFECT MyEntryEffect_RUBY_F1 = {
    "c05_f03", 0, 1,
    {2.5f, 2.5f, 0.0f, 0.0f}, {5.0f, 5.0f, 0.0f, 0.0f},
    180, 8, 1, 3, 2,
    {0, 1, 2, -1}, -1, {0, 0}, 0.0f, 0,
    {-1, -1, 101, -1},
};
// clang-format on

// clang-format off
BT_SHOT_EFFECT MyEntryEffect_RUBY_C1 = {
    "c05_r03", 0, 1,
    {2.5f, 2.5f, 0.0f, 0.0f}, {5.0f, 5.0f, 0.0f, 0.0f},
    180, 8, 2, 3, 2,
    {0, 1, 2, -1}, -1, {0, 0}, 0.0f, 0,
    {-1, -1, 102, -1},
};
// clang-format on

// clang-format off
BT_SHOT_EFFECT MyEntryEffect_RUBY_T1 = {
    "c05_t03", 0, 1,
    {2.5f, 2.5f, 0.0f, 0.0f}, {5.0f, 5.0f, 0.0f, 0.0f},
    180, 8, 4, 3, 2,
    {0, 1, 2, -1}, -1, {0, 0}, 0.0f, 0,
    {-1, -1, 103, -1},
};
// clang-format on

// clang-format off
BT_SHOT_EFFECT MyEntryEffect_RUBY_W1 = {
    "c05_e03", 0, 1,
    {2.5f, 2.5f, 0.0f, 0.0f}, {5.0f, 5.0f, 0.0f, 0.0f},
    180, 8, 8, 3, 2,
    {0, 1, 2, -1}, -1, {0, 0}, 0.0f, 0,
    {-1, -1, 104, -1},
};
// clang-format on

// clang-format off
BT_SHOT_EFFECT MyEntryEffect_RUBY_H1 = {
    "c05_h03", 0, 1,
    {2.5f, 2.5f, 0.0f, 0.0f}, {5.0f, 5.0f, 0.0f, 0.0f},
    180, 8, 16, 3, 2,
    {0, 1, 2, -1}, -1, {0, 0}, 0.0f, 0,
    {-1, -1, 105, -1},
};
// clang-format on

// clang-format off
BT_SHOT_EFFECT MyEntryEffect_UNGAGA = {
    "c10a_ex", 0, 1,
    {0.2f, 0.9f, 0.2f, 0.2f}, {8.0f, 8.0f, 6.0f, 6.0f},
    90, 20, 8, 3, 2,
    {0, 1, 2, 2}, -1, {0, 0}, 0.0f, 0,
    {-1, -1, -1, -1},
};
// clang-format on

// clang-format off
BT_SHOT_EFFECT MyEntryEffect_OZUMOND_M = {
    "mgan01", 0, 0,
    {0.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 0.0f},
    10, 8, 0, 3, 2,
    {1, -1, -1, -1}, -1, {0, 0}, 0.0f, 0,
    {-1, -1, -1, -1},
};
// clang-format on

// clang-format off
BT_SHOT_EFFECT MyEntryEffect_OZUMOND_H = {
    "rgan01", 0, 1,
    {3.5f, 3.5f, 0.2f, 0.2f}, {0.0f, 4.5f, 0.0f, 0.0f},
    180, 8, 0, 3, 2,
    {0, 1, 2, -1}, -1, {0, 0}, 0.0f, 0,
    {-1, -1, -1, -1},
};
// clang-format on

// clang-format off
BT_SHOT_EFFECT MyEntryEffect_OZUMOND_H2 = {
    "rgan02", 0, 1,
    {3.5f, 3.5f, 0.2f, 0.2f}, {0.0f, 4.5f, 0.0f, 0.0f},
    180, 8, 0, 3, 2,
    {0, 1, 2, -1}, -1, {0, 0}, 0.0f, 0,
    {-1, -1, -1, -1},
};
// clang-format on

// clang-format off
BT_SHOT_EFFECT MyEntryEffect_OZUMOND_V2 = {
    "mgan02", 0, 0,
    {0.0f, 0.5f, 0.2f, 0.2f}, {0.0f, 1.5f, 0.0f, 0.0f},
    10, 8, 0, 3, 2,
    {1, -1, -1, -1}, -1, {0, 0}, 0.0f, 0,
    {-1, -1, -1, -1},
};
// clang-format on

// clang-format off
BT_SHOT_EFFECT MyEntryEffect_OZUMOND_G = {
    "mgan03", 0, 0,
    {0.0f, 0.5f, 0.2f, 0.2f}, {0.0f, 1.5f, 0.0f, 0.0f},
    10, 8, 0, 3, 2,
    {1, -1, -1, -1}, -1, {0, 0}, 0.0f, 0,
    {-1, -1, -1, -1},
};
// clang-format on

// clang-format off
BT_SHOT_EFFECT MyEntryEffect_OZUMOND_S = {
    "mgan04", 0, 0,
    {0.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 0.0f},
    10, 8, 0, 3, 2,
    {1, -1, -1, -1}, -1, {0, 0}, 0.0f, 0,
    {-1, -1, -1, -1},
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
    "maseki_ex", 0, 1,
    {0.0f, 0.0f, 0.0f, 0.0f}, {10.0f, 0.0f, 0.0f, 0.0f},
    3, 35, 1, 3, 2,
    {0, -1, -1, -1}, -1, {0, 0}, 0.0f, 0,
    {-1, -1, -1, -1},
};
// clang-format on

// clang-format off
BT_SHOT_EFFECT MyEntryEffect_Maseki01 = {
    "maseki_ex", 0, 1,
    {0.0f, 0.0f, 0.0f, 0.0f}, {10.0f, 0.0f, 0.0f, 0.0f},
    3, 35, 2, 3, 2,
    {1, -1, -1, -1}, -1, {0, 0}, 0.0f, 0,
    {-1, -1, -1, -1},
};
// clang-format on

// clang-format off
BT_SHOT_EFFECT MyEntryEffect_Maseki02 = {
    "maseki_ex", 0, 1,
    {0.0f, 0.0f, 0.0f, 0.0f}, {10.0f, 0.0f, 0.0f, 0.0f},
    3, 35, 4, 3, 2,
    {3, -1, -1, -1}, -1, {0, 0}, 0.0f, 0,
    {-1, -1, -1, -1},
};
// clang-format on

// clang-format off
BT_SHOT_EFFECT MyEntryEffect_Maseki03 = {
    "maseki_ex", 0, 1,
    {0.0f, 0.0f, 0.0f, 0.0f}, {10.0f, 0.0f, 0.0f, 0.0f},
    3, 35, 8, 3, 2,
    {2, -1, -1, -1}, -1, {0, 0}, 0.0f, 0,
    {-1, -1, -1, -1},
};
// clang-format on

// clang-format off
BT_SHOT_EFFECT MyEntryEffect_Maseki04 = {
    "maseki_ex", 0, 1,
    {0.0f, 0.0f, 0.0f, 0.0f}, {10.0f, 0.0f, 0.0f, 0.0f},
    3, 35, 16, 3, 2,
    {4, -1, -1, -1}, -1, {0, 0}, 0.0f, 0,
    {-1, -1, -1, -1},
};
// clang-format on

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

/* The ambient light every field starts from. */
sceVu0FVECTOR def_ambientlight = {64.0f, 64.0f, 64.0f, 128.0f};

/* The light colours every field starts from. */
sceVu0FMATRIX def_lightcolor = {
    {120.0f, 120.0f, 120.0f, 128.0f},
    {0.0f,   0.0f,   0.0f,   0.0f  },
    {0.0f,   0.0f,   0.0f,   0.0f  },
    {0.0f,   0.0f,   0.0f,   0.0f  }
};

/* How the treasure-box models, and the effects over them, play their opening motions. */
MOTION_INFO itemOpenSmall_info = {10, 80, 0.5f, 0};
MOTION_INFO itemOpenSmallFx_info = {10, 80, 0.5f, 0};
MOTION_INFO itemOpenBig_info = {10, 80, 0.5f, 0};
MOTION_INFO itemOpenBigFx_info = {10, 80, 0.5f, 0};

/* How fast the player is being thrown. */
sceVu0FVECTOR velo__2 = {0.0f, 0.0f, 0.0f, 0.0f};

/* Where the right stick is pushing the camera. */
sceVu0FVECTOR velo2 = {0.0f, 0.0f, 0.0f, 0.0f};

/* The way the last blow throws the player. */
sceVu0FVECTOR blowVelo = {0.0f, 0.0f, 0.0f, 0.0f};

/* Every texture the dungeon loads on its way in, and the block each goes to. */
LOADTEXTURE_INFO2 texdata__2[] = {
    {"#frame_image#640#" SCREEN_HEIGHT_STR "#4",     1,  0},
    {"dun/etc/cursor.img",                           1,  0},
    {"gatekey00.img",                                1,  0},
    {"#water#640#" HALF_BUFFER_HEIGHT_STR "#4",      13, 0},
    {"dun/etc/atrtx.img",                            22, 0},
    {"wepready.img",                                 2,  0},
    {"#shadow_buf#640#" HALF_BUFFER_HEIGHT_STR "#3", 15, 0},
    {"dun/effect/basefx00.img",                      18, 0},
    {"basefx01.img",                                 18, 0},
    {"dun/d01/effect/fx_foot.img",                   18, 0},
    {"dun/effect/bombfx00.img",                      19, 0},
    {"pause.img",                                    7,  0},
    {"#dbgwork#640#" SCREEN_HEIGHT_STR "#3",         12, 0},
    {"check/ankfont.img",                            12, 0},
#ifndef PAL
    {"#mes_frame_buff#640#448#4",                    26, 0},
    {"#fontbase#512#256#1",                          26, 0},
    {"#fukidashibase#640#224#4",                     26, 0},
    {"meswin/syst04.img",                            26, 0},
    {"meswin/gaiji.img",                             26, 0},
    {"meswin/fuki256.img",                           26, 0},
#endif
    {"element.img",                                  18, 0},
    {"igetfx.img",                                   22, 0},
    {0,                                              0,  0},
};

/* The textures the loading screen itself draws with. */
LOADTEXTURE_INFO texdata2[3] = {
    {"#dbgwork#640#" SCREEN_HEIGHT_STR "#3", 12, 0},
    {"check/ankfont.img",                    12, 0},
    {"",                                     0,  0},
};

/* The overlay's initialised small data, in the order retail's link lays it out. */

/* How far the camera is allowed to pull back. */
s32 camera_dist_mode__3 = 1;

/* The colour the fog draws, on the front and the back floors. */
u8 main_fogColor[3] = {0x60, 0xA0, 0xEF};
u8 sub_fogColor[3] = {0x60, 0xA0, 0xEF};

/* How long the floor title has been on screen, in frames. */
s32 startCnt2 = -1;

/* Which character the menu had the cursor on when it opened. */
s32 oldUnitNow = -1;

/* Distances the camera keeps from what it follows. */
float camera_up_near_dist = 60.0f;
float camera_near_dist__2 = 10.0f;
float camera_far_dist__2 = 80.0f;

/* The colours the enemy life gauge draws the life it has left with. */
spRGBA ELife1 = {0xFF, 0x00, 0x00, 0x80};
spRGBA ELife2 = {0xFF, 0xB4, 0x00, 0x80};

/* The colours the enemy life gauge draws its border with. */
spRGBA ELifeB1 = {0x80, 0x9B, 0xFF, 0x80};
spRGBA ELifeB2 = {0x80, 0x9B, 0xFF, 0x80};

/* The colour the enemy life gauge draws the life it has lost with. */
spRGBA ELifeN1 = {0x00, 0x00, 0x20, 0x80};

/* The overlay's own small data, in the order retail's link lays it out. */

/* What the dungeon hands the Georama editor to follow. */
GAME_ENV GameEnv;

/* The player's dungeon progress and party. */
CUserStatus *UserStatus;

/* How long the dungeon has been waiting for the player, in frames. */
s32 timeOutCount;
s32 BtRubyDoorKey;

/* Whether the debug overlay draws. */
s32 BtDebugFlag;

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
s32 faceEyeCount;
s32 cameraAuto;

/* Whether the camera was moving itself before that. */
s32 cameraAutoOld;

/* Whether the player is in the back dungeon rather than the main one. */
s32 BtUraDongeon;

/** Provides the current dungeon map. */
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

/* Whether the monsters draw this frame. */
s32 MonstorViewFlag;

/* The effect the player's character is shooting now. */
CSHOT_EFFECT *NowMainEffect;

/* The model of the weapon that the player has equipped. */
CCharacter *NowWeapon;

/* The weapon that the player has equipped. */
WEAPON_HAVE *NowWeaponHave;

/* The items lying on the floor the dungeon is drawing. */
CRandomItem *RandomItem;

/** Provides the model used when an item is stolen. */
CFrameVu1 *StealModel;

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
s32    itemOpenItemMds;
s32    itemOpenItemImg;
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

/* The effect data Osmond's default weapon shoots. */
unsigned int *ozumond_default_effect;

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
s32 BtEventData;

/* Where the pack file the dungeon read last went. */
unsigned int *old_read_buffer;

/* Which dungeon event is playing, or zero while the player has control. */
s32 BtEventMode;

/* The texture that the floor number draws with. */
CTexture *TEX_Floor1;
CTexture *TEX_WepGage;
CTexture *TEX_Stayframe;
CTexture *TEX_Shadow;

/* The texture a dropped pile of gold draws with. */
CTexture *GoldTex;

/* How long the camera waits before it moves itself, in frames. */
s32 defCameraWait;

/* Whether the monster names are hidden. */
s32 MonstorNameOff;

#ifdef PAL
void LoadBaseTexture() {
    LOADTEXTURE_INFO2 info[96];
    int               size;
    int               i;
    char             *name;

    sprintf(texdata__2[2].name, "gatekey0%d.img", selectMapNo + 1);

    char path[64] = "dun/pack/dun/pack/teximg2.pac";

    if (LanguageCode > LANG_JAPANESE) {
        sprintf(path, "dun/pack/teximg2_%d.pac", LanguageCode);
    }

    LoadFile(path, (void *) read_buffer, NULL);
    wait_now_loading_vsync();

    i = 0;

    while ((name = texdata__2[i].name) != NULL) {
        // A name that starts with '#' asks the manager for a blank page of
        // that size rather than for a file inside the pack.
        if (name[0] == '#') {
            info[i].name = name;
        } else {
            u_int *found = GetPackFile(read_buffer, name, &size);

            if (found != NULL) {
                info[i].name = (char *) found;
            } else {
                printf("Error::Pack->FileNotFound [%d]%s!!\n", i, name);
                exit__2(-1);
            }
        }

        info[i].block_no = texdata__2[i].block_no;
        i++;
    }

    info[i].name = NULL;
    TexManager.LoadTextureBlock(-1, info);

    // The message window's pages, and the three images the language's own
    // pack supplies in place of the names written here.
    LOADTEXTURE_INFO2 mes_info[8] = {
        {"#mes_frame_buff#640#480#4", 0x1A, 0},
        {"#fukidashibase#640#224#4",  0x1A, 0},
        {"#fontbase#512#256#1",       0x1A, 0},
        {"meswin/gaiji.img",          0x1A, 0},
        {"meswin/fuki256.img",        0x1A, 0},
        {"meswin/syst04.img",         0x1A, 0},
    };
    char mes_path[64] = "meswin/mes_tex.pak";

    if (LanguageCode > LANG_JAPANESE) {
        sprintf(mes_path, "meswin/mes_tex_%d.pak", LanguageCode);
    }

    LoadFile(mes_path, read_buffer, NULL);
    wait_now_loading_vsync();
    mes_info[3].name = (char *) GetPackFile(read_buffer, "gaiji.img", NULL);
    mes_info[4].name = (char *) GetPackFile(read_buffer, "fuki256.img", NULL);
    mes_info[5].name = (char *) GetPackFile(read_buffer, "syst04.img", NULL);
    TexManager.LoadTextureBlock(-1, mes_info);
}
#else
void LoadBaseTexture() {
    LOADTEXTURE_INFO2 info[97];
    int               size;
    int               i;
    char             *name;

    sprintf(texdata__2[2].name, "gatekey0%d.img", selectMapNo + 1);

    unsigned int *pack = read_buffer;

    LoadFile("dun/pack/teximg2.pac", pack, NULL);
    wait_now_loading_vsync();

    i = 0;

    while ((name = texdata__2[i].name) != NULL) {
        // A name that starts with '#' asks the manager for a blank page of
        // that size rather than for a file inside the pack.
        if (name[0] == '#') {
            info[i].name = name;
        } else {
            u_int *found = GetPackFile(read_buffer, name, &size);

            if (found != NULL) {
                info[i].name = (char *) found;
            } else {
                printf("Error::Pack->FileNotFound [%d]%s!!\n", i, name);
                exit__2(-1);
            }
        }

        info[i].block_no = texdata__2[i].block_no;
        i++;
    }

    info[i].name = NULL;
    TexManager.LoadTextureBlock(-1, info);
}
#endif

int LoadTempTexture(char **files, int block, char *buffer) {
    LOADTEXTURE_INFO2 info[7] = {0};
    int               i;
    int               total;
    int               size;

    i = 0;
    total = 0;

    while (files[i] != NULL) {
        LoadFile(files[i], buffer, &size);
        wait_now_loading_vsync();
        total += size;
        info[i].block_no = block;
        info[i].name = buffer;
        buffer += ((size >> 6) + 1) << 6;
        i++;
    }

    TexManager.DeleteTextureBlock(block);
    TexManager.LoadTextureBlockEX(block, info);
    return total;
}

void SetTempTexture(int block, char *name) {
    LOADTEXTURE_INFO2 info[2] = {0};

    info[0].block_no = block;
    info[0].name = name;
    TexManager.DeleteTextureBlock(block);
    TexManager.LoadTextureBlockEX(block, info);
}

void MemoryMapDump() {
    printf("MainVisualData  \t%d/%d\n", MainModelBuffer.used, 0x9470);
    printf("MotionData      \t%d/%d\n", MotionData.used, 10000);
    printf("CharaModelBuffer\t%d/%d\n", CharaModelBuffer.used, 0x33450);
    printf("MasekiModelBuffer\t%d/%d\n", MasekiModelBuffer.used, 0xFDE8);
    printf("MapVisualData   \t%d/%d\n", MapModelBuffer.used, 0xA7F80);
    s32 monster_size = MonstorModelBuffer.limit;

    printf("MonstorModelBuffer\t%d/%d\n", MonstorModelBuffer.used, monster_size);
    printf("SystemScriptBuffer\t%d/%d\n", BtSystemScriptFileBuffer.used, 0x88B8);
    printf("StartLogoBuffer\t    %d/%d\n", BtStartLogoBuffer.used, 5000);
    printf("BtMesBuffer\t        %d/%d\n", BtMesBuffer.used, 0x2904);
    printf("BtSteebMesBuffer\t%d/%d\n", BtSteebMesBuffer.used, 0xDAC);
    TexManager.print_buff_info();
}

void GameInit() {
    int    size;
    char   name[64];
    char   gate_name[32];
    short *mes;

    BtDebugFlag = 0;
    exitMenuFlag = 0;
    BtAllDrawFlag = 0;
    selectMapNo = main_select_menu_no;
    UserStatus = (CUserStatus *) SaveData->GetDngStatus();
    UserStatus->Init();

    if (BtEventInfo.no_status_recover != 1) {
        int          i;
        CUserStatus *status = UserStatus;

        for (i = 0; i < 6; i++) {
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

    for (int i = 0; i < 6; i++) {
        SetDataBuffer(&BtItemCashArea[i], 2501);
    }

    for (int i = 0; i < 16; i++) {
        SetDataBuffer(&MonstorScriptBuffer[i], 1875);
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

    for (int i = 0; i < 64; i++) {
        CDungeonEvent *slot = &DngEventMan.slot[i];

        slot->name[0] = '\0';
        slot->script_no = -1;
        slot->parts_id = -1;
        slot->switch_on = false;
        slot->enabled = false;
    }

    for (int i = 0; i < 96; i++) {
        CDungeonEventData *state = &DngEventMan.event[i];

        state->event = NULL;
        state->switch_on = false;
        state->enabled = false;
        state->hold = 0;
        state->chara_done = -1;
    }

    NowEventMan = &DngEventMan;
    lightingMode = 0;

    for (int i = 0; i < 12; i++) {
        CDranMapField *field = &DranMapField.field[i];

        field->Initialize();
        DranMapField.collision[i] = 0;
        DranMapField.state[i] = 3;
    }

    DranMapField.field_count = 0;
    DranMapField.collision_count = 0;
    NowDranMapField = DranMapField.field;
    NowColData = &CColData;

    for (int i = 0; i < 12; i++) {
        ShotData.used[i] = 0;
    }

    NowShotData = &ShotData;
    HealEffect.active = false;
    WaterSplash_Init();

    for (int i = 0; i < 5; i++) {
        ShotEffect.effect[i].Initialize();
    }

    NowShotEffect = &ShotEffect;

    for (int i = 0; i < 5; i++) {
        MasekiEffect[i].Initialize();
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

    for (int i = 0; i < 16; i++) {
        HitMark[i].Initialize();
    }

    for (int i = 0; i < 16; i++) {
        OzumondShotEffect.timer[i] = -1;
    }

    NewChangeFxFlag = 0;

    for (int i = 0; i < 32; i++) {
        HitValue[i].active = false;
        HitValue[i].frame = NULL;
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

    LoadFileMenuData("itempack.img", (unsigned int *) read_buffer);
    wait_now_loading_vsync();
    TexManager.EnterIMGFile((unsigned char *) read_buffer, -1, 0, 0);
    LoadFileMenuData("stayframe.img", (unsigned int *) read_buffer);
    wait_now_loading_vsync();
    TexManager.EnterFixTextureZ((unsigned char *) read_buffer);

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

    for (int i = 0; i < 10; i++) {
        DngMesStb.mes_no[i] = -1;
    }

    for (int i = 0; i < 8; i++) {
        DngMesStb.values[i] = 0;
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

    for (int i = 0; i < 10; i++) {
        DngMesStb.line_pos[i].x = -1;
        DngMesStb.line_pos[i].y = -1;
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

    mes = (short *) (BtMesBuffer.base + BtMesBuffer.used * 16);
    sprintf(name, "dun/message/ww_mes/dunmsd00_%d.mes", LanguageCode);
    LoadFile(name, mes, &size);
    wait_now_loading_vsync();
    DngMes1.SetBuff(mes);
    DngMes2.SetBuff(mes);
    MonsterNameInit(&DngMes2, mes, MesWinTexBuff_02);
    BtEventMes0.SetBuff(mes);
    BtMesBuffer.Alloc((((size >> 6) + 1) << 6) >> 4);
    BtMesBuffer.Align64();

    mes = (short *) (BtMesBuffer.base + BtMesBuffer.used * 16);
    sprintf(name, "dun/message/ww_mes/font18_%d.mes", LanguageCode);
    LoadFile(name, mes, &size);
    wait_now_loading_vsync();
    BtEventMes1.SetBuff(mes);
    BtMesBuffer.Alloc((((size >> 6) + 1) << 6) >> 4);
    BtMesBuffer.Align64();

    mes = (short *) (BtMesBuffer.base + BtMesBuffer.used * 16);
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

    for (int i = 0; i < 32; i++) {
        CWeaponFx.used[i] = 0;
    }

    CWeaponFx.colour[0].r = CWeaponFx.colour[2].r = 0x40;
    CWeaponFx.colour[0].g = CWeaponFx.colour[2].g = 0x40;
    CWeaponFx.colour[0].b = CWeaponFx.colour[2].b = 0x40;
    CWeaponFx.colour[1].r = CWeaponFx.colour[3].r = 6;
    CWeaponFx.colour[1].g = CWeaponFx.colour[3].g = 6;
    CWeaponFx.colour[1].b = CWeaponFx.colour[3].b = 6;
    CWeaponFx.cursor = 0;
    CWeaponFx.timer = 0;

    for (int i = 0; i < 4; i++) {
        CWeaponElFx[i].Initialize();
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

    for (int i = 0; i < 3; i++) {
        CBomb__2[i].Initialize();
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
    sprintf(gate_name, "gatekey0%d", selectMapNo + 1);
    GoldTex = TexManager.GetTexture(gate_name, -1);

    if (GoldTex == NULL) {
        printf("******** ERR\n");
    }

    BtMapJumpCashClear();

    CDungeonMap *map_now = NowDngMap;

    map_now->draw_dist_scale = 4.0f;
    ((CDngStatusData *) UserStatus)->SetNowFloor(0);

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
    float fade[4];
    int   i;

    switch (BtGameModeFlag) {
        case BT_GAME_MODE_PLAY:
            MoveChara();
            break;
        case BT_GAME_MODE_BATTLE_MENU:
            if (BattleMenuCursor() == 0) {
                BtGameModeFlag = BT_GAME_MODE_PLAY;
            }

            break;
    }

    switch (BtGameModeFlag) {
        case BT_GAME_MODE_PLAY:
            MainDraw();
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
                if (BtEventInfo.entrance_result != NULL) {
                    BtEventInfo.entrance_result->i = floor;
                }

                BtEventInfo.entrance_result = NULL;
                ((CDngStatusData *) UserStatus)->SetNowFloor(floor);
                BtGameModeFlag = BT_GAME_MODE_PLAY;
            }

            break;
        }
        case BT_GAME_MODE_ESCAPE_MSG: {
            int answer = DngEscapeMsgLoop();

            if (answer > 0) {
                if (BtEventInfo.escape_result != NULL) {
                    BtEventInfo.escape_result->i = answer;
                }

                BtEventInfo.escape_result = NULL;
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

        EdGetFadeColor(fade);
        MGSetBGColor(fade);
        ((CDngStatusData *) UserStatus)->LostGateKey();

        if (BtEventInfo.no_status_recover == 0) {
            CUserStatus *status = UserStatus;

            for (i = 0; i < 6; i++) {
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

#ifdef PAL
    // In debug mode, Start pressed while Select is held leaves the dungeon.
    if (DebugMode != 0 && GamePad.On(PAD_SELECT) != 0 && GamePad.Down(PAD_START) != 0) {
        while (ReadBGSync() == 1) {
        }

        SndExit();

        CUserStatus *leaving = UserStatus;

        leaving->prev_floor = -1;
        return 1;
    }
#endif

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
    int   i;

    // The shadows are drawn under a dimmed light and the real one goes back
    // afterwards.
    MGGetPLight(light, colour);
    sceVu0CopyMatrix(saved, light);
    light[0][0] *= 0.2f;
    light[2][0] *= 0.2f;
    MGSetPLight(light, colour);

    TexManager.ReloadTexture(Vif1Packet, 15);
    MGBeginDrawShadow(*(sceGsTex0 *) &TexManager.GetTexture("shadow_buf", -1)->tex0);

    if (BtActStatus.shadow_visible != 0 && EdEventInfo.player_shadow_draw != 0 && EdEventInfo.player_draw != 0 && BtActStatus.player_visible != 0) {
        CharaMain.DrawShadow();
    }

    if (CMonUnitHyde == 0 && BtEventMode == 0) {
        NowMonstorUnit->DrawShadowMonstor();
    }

    if (BtEventMode != 0) {
        for (i = 0; i < 6; i++) {
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

        if (dist <= 10.0f) {
            dist = 10.0f;
        }

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

    if (NowWeapon != NULL && NowWeapon->frame != NULL && BtActStatus.weapon_visible != 0) {
        TexManager.ReloadTexture(Vif1Packet, 0x1D);
        NowWeapon->Draw();
    }

    MGSetAmbient(saved_ambient);
    sceVu0CopyMatrix(light, saved_light);
    sceVu0CopyMatrix(colour, saved_colour);
    MGSetPLight(light, colour);
}

void MainDraw() {
    sceVu0FMATRIX camera;
    sceVu0FVECTOR eye;
    sceVu0FMATRIX view;
    sceVu0FMATRIX unit;
    sceGsTex0     frame_tex;
    sceGsTex0     water_tex;
    int           i;

    EdEventInfo.main_texture_animation = NULL;
    NowCamera__3->GetPos(eye);
    NowCamera__3->GetCameraMatrix(camera);
    sceVu0UnitMatrix(unit);
    sceVu0MulMatrix(view, unit, camera);
    MGSetViewMatrix(view, eye);
    SndSetCamera(NowCamera__3);
    sceVif1PkCall(Vif1Packet, (u_long128 *) Vu_prog0f, 0);

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

    // The map field's own draw, not the virtual one it inherits: retail binds
    // this call at compile time.
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
        NowMonstorUnit->DrawMonstor();
    }

    if (CharaMainHandViewFlag != 0) {
        TexManager.ReloadTexture(Vif1Packet, 0x11);

        if (UserStatus->cur_chara == CHARA_XIAO) {
            CharaHand.Draw();
        }

        if (NowWeapon != NULL && UserStatus->cur_chara == CHARA_XIAO && NowWeapon->frame != NULL && BtActStatus.weapon_visible != 0) {
            TexManager.ReloadTexture(Vif1Packet, 0x1D);
            NowWeapon->Draw();
        }
    }

    if (BtEventMode != 0) {
        for (i = 0; i < 6; i++) {
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
        water_tex = *(sceGsTex0 *) &TexManager.GetTexture("water", -1)->tex0;
        MGMoveImage(&frame_tex, area, &water_tex, 0, 0, 0);
        sceVu0CopyVector(water_pos, CharaFrame->position);
        NowDngMap->DrawWater(water_pos, driveStepHold);
    }

    if (CEffectHyde == 0 && BtEventMode == 0) {
        TexManager.ReloadTexture(Vif1Packet, 0x13);
        NowShockWave->Draw(NowCamera__3);

        for (i = 0; i < 3; i++) {
            CBomb__2[i].Draw(NowCamera__3);
        }

        for (i = 0; i < 5; i++) {
            MasekiEffect[i].Draw();
        }

        CSHOT_EFFECT *shot = NowShotEffect->effect;

        for (i = 0; i < 5; i++) {
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
        int shift;
        int floor_no;
        int digit_x;

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
            MGSetGsZBUF(NULL);
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

            for (i = 0; i < 4; i++) {
                CWeaponElFx[i].Draw();
            }

            WeaponCrashEffect.Draw();

            for (i = 0; i < 16; i++) {
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
            MGSetGsALPHA(NULL);
            MGSetGsZBUF(NULL);
        }

#ifdef PAL
        if (rogoSwitch2 == 1 && BtEventInfo.floor_title_off == 0) {
            TEX_Floor1 = TexManager.GetTexture(floor_name, -1);
            floor_no = 0;

            if (UserStatus->cur_floor >= 9) {
                floor_no = -0x24;
            }

            TexManager.ReloadTexture(Vif1Packet, 8);
            StartMessageDraw(TEX_Floor1, selectMapNo, UserStatus->cur_floor, BtUraDongeon, rogoAlphaA[2]);
        }
#else
        if (rogoSwitch2 == 1 && BtEventInfo.floor_title_off == 0) {
            TEX_Floor1 = TexManager.GetTexture(floor_name, -1);
            shift = 0;
            floor_no = 0;

            if (UserStatus->cur_floor >= 9) {
                floor_no = -0x24;
            }

            TexManager.ReloadTexture(Vif1Packet, 8);

            if (LanguageCode == LANG_JAPANESE) {
                switch (selectMapNo) {
                    case DUNGEON_DIVINE_BEAST_CAVE:
                        set2DSprite(Vif1Packet, TEX_Floor1, CRect_i_(floor_no + 0xE2, 0xAA, 0x72, 0x32), CRect_i_(0, 0, 0x72, 0x32), rogoAlphaA[2]);
                        set2DSprite(Vif1Packet, TEX_Floor1, CRect_i_(0x17D, 0xAA, 0x4C, 0x32), CRect_i_(0x72, 0, 0x4C, 0x32), rogoAlphaA[2]);
                        break;
                    case DUNGEON_WISE_OWL_FOREST:
                        set2DSprite(Vif1Packet, TEX_Floor1, CRect_i_(0xBE, 0xAA, 0xBE, 0x32), CRect_i_(0, 0, 0xBE, 0x32), rogoAlphaA[2]);
                        shift = 0x40;
                        break;
                    case DUNGEON_SHIPWRECK:
                        set2DSprite(Vif1Packet, TEX_Floor1, CRect_i_(floor_no + 0xF5, 0xAA, 0x4C, 0x32), CRect_i_(0, 0, 0x4C, 0x32), rogoAlphaA[2]);
                        set2DSprite(Vif1Packet, TEX_Floor1, CRect_i_(0x159, 0xAA, 0x4C, 0x32), CRect_i_(0x4C, 0, 0x4C, 0x32), rogoAlphaA[2]);
                        shift = -0x24;
                        break;
                    case DUNGEON_SUN_MOON_TEMPLE:
                        set2DSprite(Vif1Packet, TEX_Floor1, CRect_i_(0xE8, 0xAA, 0x98, 0x32), CRect_i_(0, 0, 0x98, 0x32), rogoAlphaA[2]);
                        shift = 0x40;
                        break;
                    case DUNGEON_MOON_SEA:
                        set2DSprite(Vif1Packet, TEX_Floor1, CRect_i_(floor_no + 0xD7, 0xAA, 0x4C, 0x32), CRect_i_(0, 0, 0x4C, 0x32), rogoAlphaA[2]);
                        set2DSprite(Vif1Packet, TEX_Floor1, CRect_i_(0x13B, 0xAA, 0xBE, 0x32), CRect_i_(0x4C, 0, 0xBE, 0x32), rogoAlphaA[2]);
                        shift = -0x40;
                        break;
                    case DUNGEON_GALLERY_OF_TIME:
                        set2DSprite(Vif1Packet, TEX_Floor1, CRect_i_(0x135, 0xAA, 0x98, 0x32), CRect_i_(0, 0, 0x98, 0x32), rogoAlphaA[2]);
                        shift = -0x40;
                        break;
                }

                if (BtUraDongeon != 0) {
                    set2DSprite(Vif1Packet, TEX_Floor1, CRect_i_(0x120, 0xE6, 0x40, 0x38), CRect_i_(0x13A, 0x78, 0x40, 0x38), rogoAlphaA[2]);
                } else if (UserStatus->res_limit_zone_current >= 0) {
                    set2DSprite(Vif1Packet, TEX_Floor1, CRect_i_(0xD4, 0xE6, 0xD8, 0x30), CRect_i_(0, 0x78, 0xD8, 0x30), rogoAlphaA[2]);
                }
            }

            if (LanguageCode > LANG_JAPANESE) {
                switch (selectMapNo) {
                    case DUNGEON_DIVINE_BEAST_CAVE:
                        set2DSprite(Vif1Packet, TEX_Floor1, CRect_i_(floor_no + 0x122, 0xAA, 0x2D, 0x32), CRect_i_(0, 0, 0x2D, 0x32), rogoAlphaA[2]);
                        shift = -0x14;
                        break;
                    case DUNGEON_WISE_OWL_FOREST:
                        set2DSprite(Vif1Packet, TEX_Floor1, CRect_i_(0xA0, 0xAA, 0xDA, 0x32), CRect_i_(0, 0, 0xDA, 0x32), rogoAlphaA[2]);
                        shift = 0x40;

                        if (UserStatus->cur_floor < 9) {
                            shift = 0x20;
                        }

                        break;
                    case DUNGEON_SHIPWRECK:
                        set2DSprite(Vif1Packet, TEX_Floor1, CRect_i_(0xC8, 0xAA, 0x91, 0x32), CRect_i_(0, 0, 0x91, 0x32), rogoAlphaA[2]);

                        if (UserStatus->cur_floor >= 9) {
                            shift = 0x20;
                        }

                        break;
                    case DUNGEON_SUN_MOON_TEMPLE:
                        set2DSprite(Vif1Packet, TEX_Floor1, CRect_i_(floor_no + 0xDC, 0xAA, 0x8E, 0x32), CRect_i_(0, 0, 0x8E, 0x32), rogoAlphaA[2]);
                        shift = 0x10;
                        break;
                    case DUNGEON_MOON_SEA:
                        set2DSprite(Vif1Packet, TEX_Floor1, CRect_i_(floor_no + 0xC3, 0xAA, 0x4B, 0x32), CRect_i_(0, 0, 0x4B, 0x32), rogoAlphaA[2]);
                        set2DSprite(Vif1Packet, TEX_Floor1, CRect_i_(0x13B, 0xAA, 0xC0, 0x32), CRect_i_(0x60, 0, 0xC0, 0x32), rogoAlphaA[2]);
                        shift = -0x40;
                        break;
                    case DUNGEON_GALLERY_OF_TIME:
                        set2DSprite(Vif1Packet, TEX_Floor1, CRect_i_(0x108, 0xAA, 0xE1, 0x32), CRect_i_(0, 0, 0xE1, 0x32), rogoAlphaA[2]);
                        shift = -0x80;
                        break;
                    case DUNGEON_DEMON_SHAFT:
                        set2DSprite(Vif1Packet, TEX_Floor1, CRect_i_(floor_no + 0xDC, 0xAA, 0x8E, 0x32), CRect_i_(0, 0, 0x8E, 0x32), rogoAlphaA[2]);
                        shift = 0x10;
                        break;
                }

                if (BtUraDongeon != 0) {
                    set2DSprite(Vif1Packet, TEX_Floor1, CRect_i_(0x110, 0xE6, 0x60, 0x38), CRect_i_(0x110, 0x78, 0x60, 0x38), rogoAlphaA[2]);
                } else if (UserStatus->res_limit_zone_current >= 0) {
                    set2DSprite(Vif1Packet, TEX_Floor1, CRect_i_(0xD4, 0xE6, 0xF0, 0x30), CRect_i_(0, 0x78, 0xF0, 0x30), rogoAlphaA[2]);
                }
            }

            floor_no = UserStatus->cur_floor + 1;

            if (selectMapNo == DUNGEON_GALLERY_OF_TIME) {
                floor_no = BtGetFloorLevel(floor_no - 1);
            }

            if (floor_no < 10) {
                digit_x = floor_no % 10 * 0x26;
                set2DSprite(Vif1Packet, TEX_Floor1, CRect_i_(shift + 0x15A, 0xAA, 0x26, 0x32), CRect_i_(digit_x, 0x32, 0x26, 0x32), rogoAlphaA[2]);
            }

            if (floor_no >= 10 && floor_no < 100) {
                digit_x = floor_no / 10 * 0x26;
                set2DSprite(Vif1Packet, TEX_Floor1, CRect_i_(shift + 0x136, 0xAA, 0x26, 0x32), CRect_i_(digit_x, 0x32, 0x26, 0x32), rogoAlphaA[2]);
                digit_x = floor_no % 10 * 0x26;
                set2DSprite(Vif1Packet, TEX_Floor1, CRect_i_(shift + 0x15A, 0xAA, 0x26, 0x32), CRect_i_(digit_x, 0x32, 0x26, 0x32), rogoAlphaA[2]);
            }

            if (floor_no >= 100) {
                int digit = floor_no / 100;

                digit_x = digit * 0x26;
                floor_no -= digit * 100;
                set2DSprite(Vif1Packet, TEX_Floor1, CRect_i_(shift + 0x112, 0xAA, 0x26, 0x32), CRect_i_(digit_x, 0x32, 0x26, 0x32), rogoAlphaA[2]);
                digit_x = floor_no / 10 * 0x26;
                set2DSprite(Vif1Packet, TEX_Floor1, CRect_i_(shift + 0x136, 0xAA, 0x26, 0x32), CRect_i_(digit_x, 0x32, 0x26, 0x32), rogoAlphaA[2]);
                digit_x = floor_no % 10 * 0x26;
                set2DSprite(Vif1Packet, TEX_Floor1, CRect_i_(shift + 0x15A, 0xAA, 0x26, 0x32), CRect_i_(digit_x, 0x32, 0x26, 0x32), rogoAlphaA[2]);
            }
        }

#endif

        TexManager.ReloadTexture(Vif1Packet, 2);

        int gauge_alpha = rogoY3 + 0x60;

        if ((int) BtActStatus.action_gauge >= 100) {
            if (BtActStatus.gauge_flash == 0) {
                set2DSprite(Vif1Packet, TEX_WepGage, CRect_i_(0x28, SCREEN_HEIGHT - 0x34, 0x80, 0x1C), CRect_i_(0, 0, 0x80, 0x1C), gauge_alpha);
            } else {
                set2DSprite(Vif1Packet, TEX_WepGage, CRect_i_(0x28, SCREEN_HEIGHT - 0x34, 0x80, 0x1C), CRect_i_(0, 0x1C, 0x80, 0x1C), gauge_alpha);
            }
        }

        float wear = BtActStatus.action_gauge;

        if ((int) wear < 100) {
            int left = (int) wear;

            set2DSprite(Vif1Packet, TEX_WepGage, CRect_i_(0x28, SCREEN_HEIGHT - 0x34, 0x80, 0x1C), CRect_i_(0, 0x54, 0x80, 0x1C), gauge_alpha);
            set2DSprite(Vif1Packet, TEX_WepGage, CRect_i_(0x42, SCREEN_HEIGHT - 0x34, left, 0x1C), CRect_i_(0x1A, 0x38, left, 0x1C), gauge_alpha);
        }

        if (infoMap != 0 && NowDngMap->map_type == 1) {
            sceVu0FVECTOR map_pos;
            sceVu0FVECTOR map_rot;

            TexManager.ReloadTexture(Vif1Packet, 0x1F);
            sceVu0CopyVector(map_pos, CharaFrame->position);
            CharaFrame->GetRotation(map_rot);
            NowDngMap->DrawMiniMap(map_pos, map_rot[1]);
            NowMonstorUnit->DrawMapSymbol(map_pos);
            RandomItem->MapSymbolDraw();
        }

        topStatusInfo(rogoY3, itemNowSel, UserStatus->cur_floor);
        BtStatusErrDraw(rogoY3);

        for (i = 0; i < 32; i++) {
            HitValue[i].Draw();
        }

        if (exitMenuFlag != 0) {
            TexManager.ReloadTexture(Vif1Packet, 7);
            set2DSprite(Vif1Packet, TexManager.GetTexture("pause", -1), CRect_i_(0x100, 0xCC, 0x80, 0x28), 0, 0);
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
        MGFillBox(CRect_i_(0, 0, 0x2800, 0xE00), (int) fade[0], (int) fade[1], (int) fade[2], (int) fade[3]);
    }

    if (BtEventMode != 0) {
        EdEventSpriteDraw();
        EBDraw();
        TexManager.ReloadTexture(Vif1Packet, 0x1A);
        BtEventMes0.DrawMesWin();
        BtEventMes1.DrawMesWin();
    }

    for (i = 0; i < 1; i++) {
        if (EdEventInfo.item_frame[i] != NULL) {
            TexManager.ReloadTexture(Vif1Packet, i + 0x28);
            MGDraw(EdEventInfo.item_frame[i]);
        }
    }

    setbilinear(0);
    TexManager.ReloadTexture(Vif1Packet, 0x1A);
    SystemMesStep();
    SystemMesDraw();

    int showing;

    if (DngMessMan.hide_timer > 0) {
        showing = 0;
    } else {
        showing = DngMessMan.enabled;
    }

    if (showing != 0) {
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
        sceGsSyncPath(0, 0);
        TexManager.ReloadTexture(Vif1Packet, 0x17);
        MGMoveFrameBuffImage((sceGsTex0 *) &TexManager.GetTexture("frame_image", -1)->tex0, 0, 0, 0);
    }
}

float oldCameraHeight;
float oldCameraAngle;

/* How hard the player is turning the camera. */
float inputH1;

/**
 * Drops the model an active item slot shows, once its last use is gone.
 */
static inline void DeleteItemModel(int slot) {
    if (activeItem.model[slot] != -1) {
        activeItem.models->DeleteModel(activeItem.model[slot]);
        activeItem.model[slot] = -1;
    }
}

/**
 *              Runs the dungeon for one frame.
 *
 * `gameTask` holds the GameTask the dungeon is running. Every task ends by
 * stepping the models.
 */
void MoveChara() {
    /* Where the camera looks relative to what it follows, before the floor's
       own offset is added. */
    static sceVu0FVECTOR reference = {0.0f, 7.5f, 0.0f, 0.0f};
    sceVu0FVECTOR        pos;
    float                move_z;
    float                move_x;
    float                ly;
    float                lx;
    float                angle;
    float                rx;
    float                ry;
    CFrame              *collision;
    int                  entry_no;

    // An event that sets its own draw distance keeps it; anything else draws
    // the whole floor.
    if (BtEventMode != 0 && EdEventInfo.projection > 0.0f) {
        MGSetRenderInfo(EdEventInfo.projection, 1.0f, (float) 0xFFFF);
    } else {
        MGSetRenderInfo(800.0f, 1.0f, (float) 0xFFFF);
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
                } else if (GamePad.Down(PAD_TRIANGLE) != 0) {
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
                    } else {
                        goto walk;
                    }
                } else {
                walk:
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

                    if (GamePad.Down(PAD_SELECT) != 0 && UserStatus->party_size >= 2 && BtActStatus.action_on == 0) {
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
                        static int    cnt;
                        static char   init;
                        sceVu0FVECTOR mask_pos;

                        if (init == 0) {
                            cnt = 0;
                            init = 1;
                        }

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
                            float         face;

                            locked->GetPosition(target);
                            atan2f(pos[0] - target[0], pos[2] - target[2]);
                            face = atan2f(target[0] - pos[0], target[2] - pos[2]);

                            if (BtActStatus.stick_strength > 0.0f) {
                                CharaFrame->SetRotation(0.0f, face, 0.0f);
                            } else {
                                CharaFrame->SetRotation(0.0f, unitRotation(CharaFrame, face), 0.0f);
                            }

                            if (BtActStatus.action_on == 0) {
                                float turn;
                                float rate;
                                int   motion;

                                BtActStatus.motion_no = 0x12;
                                CharaMain.motion_type.motion_info[18].speed = 0.2f;
                                turn = atan2f(move_x, move_z);
                                turn = face - turn;

                                if (turn < -PI_SHORT) {
                                    turn += TWO_PI_SHORT;
                                }

                                if (turn > PI_SHORT) {
                                    turn -= TWO_PI_SHORT;
                                }

                                // Which strafe motion plays comes from the angle
                                // between the way the character faces and the way
                                // the stick points, in five bands around it.
                                motion = 0x12;

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
                                    rate = 0.2f + BtActStatus.stick_strength;

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
                            float end;

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

                        int script;

                        if (NowMonstorUnit->requested_event != -1) {
                            BtEventInfo.script_no = NowMonstorUnit->requested_event;
                            BtEventInfo.ext_memory = true;
                            ResetStatusInfo();
                            driveStepHold = 1;
                            EdFadeInit();
                            float r, g, b;
#ifdef PAL
                            b = g = r = 0.0f;
#else
                            r = g = b = 0.0f;
#endif
                            EdFadeOut(0x78, r, g, b);
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
                            int              event;

                            if (trap != NULL) {
                                SetSystemMes(Run_TrapCircle(trap) + 0x12C, 0x96, MES_POS_BOTTOM, 0, NULL, NULL);
                                DngMessMan.hide_timer = 0x96;
                            }

                            if (UserStatus->CheckLife() != 0) {
                                event = RandomItem->checkErr();

                                if (event != 0) {
                                    ClearSystemMes();

                                    if (event == 1) {
                                        SetSystemMes(0x48, 0x78, MES_POS_BOTTOM, 0, NULL, NULL);
                                    }

                                    if (event == 2) {
                                        SetSystemMes(0x51, 0x78, MES_POS_BOTTOM, 0, NULL, NULL);
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
                                                ((CDngStatusData *) UserStatus)->GetItem(event, 0);
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

                                    goto steal;
                                }
                            } else {
                            steal:
                                if (UserStatus->CheckLife() != 0) {
                                    int stolen = StealItem.checkEvent();

                                    if (stolen != -1) {
                                        BtGetAttach_Init(selectMapNo, stolen);
                                        DngMessMan.hide_timer = 0x78;
                                        SndSePlay(SE_ITEM_GET, -1, 0);
                                        autoCamTrial();
                                    }
                                }

                                if (BtActStatus.can_act != 0 && BtActStatus.movement_locked == 0) {
                                    CDungeonEventData *state = NowEventMan->SearchDataSlotPos2(pos);
                                    sceVu0FVECTOR      slot_pos;
                                    sceVu0FVECTOR      slot_dir;

                                    if (state != NULL && state->event->chara_no != -1) {
                                        s8  chara = UserStatus->cur_chara;
                                        int done = state->chara_done;

                                        if (done == chara) {
                                            state = NULL;
                                        }
                                    }

                                    BtEventInfo.script_no = -1;

                                    if (state != NULL && state->event->script_no != -1) {
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
                                        } else if (state->hold != 0 && GamePad.Down(PAD_SQUARE) != 0) {
                                            gameTask = GAME_TASK_SCRIPT_START;
                                            BtEventInfo.action_mode = 2;
                                        } else if (GamePad.Down(PadInput_OK) != 0) {
                                            gameTask = GAME_TASK_SCRIPT_START;
                                            BtEventInfo.action_mode = 1;
                                        } else {
                                            goto action;
                                        }
                                    } else {
                                        goto action;
                                    }
                                } else {
                                action:
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
                                        SetSystemMes(0x47, 0x5A, MES_POS_BOTTOM, 0, NULL, NULL);
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
                                        SetSystemMes(0x50, 0x5A, MES_POS_BOTTOM, 0, NULL, NULL);
                                        DngMessMan.hide_timer = 0x5A;
                                        autoCamTrial();
                                    } else {
                                        BtEventItemNo = -1;

                                        if ((iventActive = NowDngMap->GetActiveIvent(CharaFrame)) != -1 && BtActStatus.can_act != 0 && BtActStatus.movement_locked == 0) {
                                            iventInfo = NowDngMap->events[iventActive].kind;
                                            BtEventInfo.event_marker = true;
                                            iventMarker = 1;
                                        } else {
                                            iventActive = -1;
                                            iventInfo = DNG_EVENT_NONE;
                                            iventMarker = 0;
                                        }

                                        if (GamePad.Down(PadInput_OK) != 0 && BtActStatus.movement_locked == 0 && BtActStatus.event_block_frames == 0) {
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
                                                                goto opened;
                                                            }

                                                            if (NowDngMap->boxes[entry_no].trap_no == 5) {
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
                                                        opened:
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

                                                goto play;
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
                                                }

                                                goto play;
                                            }
                                        } else {
                                        play:
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
                                                        // A running item that
                                                        // boosts is spent by the
                                                        // frame while the button
                                                        // is held.
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
                                                int  used;
                                                s16 *slots = UserStatus->active_item;

                                                used = checkItemUsed(itemNowSel - 1);

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

                                                if (state != NULL && BtActStatus.can_act != 0 && state->event->chara_no != -1) {
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
                                                    } else {
                                                        goto step;
                                                    }
                                                } else {
                                                step:
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

                                                        if (BtActStatus.jumping > 0 && BtActStatus.jumping == 1) {
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
                                                                CCPoly       *polys;
                                                                int           mode;

                                                                WorkBuffer__2->used = 0;
                                                                polys = (CCPoly *) WorkBuffer__2->Alloc(0x7D0);
                                                                bound.max[0] = 20.0f + pos[0];
                                                                bound.max[1] = 20.0f + pos[1];
                                                                bound.max[2] = 20.0f + pos[2];
                                                                bound.min[0] = pos[0] - 20.0f;
                                                                bound.min[1] = pos[1] - 40.0f;
                                                                bound.min[2] = pos[2] - 20.0f;
                                                                // A random floor
                                                                // is built from map
                                                                // parts, so the
                                                                // polygons come off
                                                                // each part's own
                                                                // collision model.
                                                                int i = 0;

                                                                colPolyNum = 0;

                                                                for (; NowDngMap->parts[i].frame[0] != NULL; i++) {
                                                                    int turn;

                                                                    collision = i == -1 ? NULL : NowDngMap->parts[i].collision;

                                                                    if (collision != NULL) {
                                                                        CDungeonParts *part = &NowDngMap->parts[i];

                                                                        sceVu0CopyVector(parts_pos, part->frame_offset[0]);

                                                                        CDungeonMap   *map = NowDngMap;
                                                                        CDungeonParts *turn_part = &map->parts[i];

                                                                        turn = (int) turn_part->frame_turn[0];
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
                                                                mode = 1;

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
                                                                int           mode;
                                                                int           x;
                                                                int           z;
                                                                CFrame       *collision;

                                                                WorkBuffer__2->used = 0;
                                                                CCPoly *polys = (CCPoly *) WorkBuffer__2->Alloc(0x7D0);
                                                                bound.max[0] = 20.0f + pos[0];
                                                                bound.max[1] = 20.0f + pos[1];
                                                                bound.max[2] = 20.0f + pos[2];
                                                                bound.min[0] = pos[0] - 20.0f;
                                                                bound.min[1] = pos[1] - 40.0f;
                                                                bound.min[2] = pos[2] - 20.0f;
                                                                colPolyNum = 0;

                                                                for (z = 0; z < 20; z++) {
                                                                    for (x = 0; x < 20; x++) {
                                                                        CDungeonMap *cmap = NowDngMap;
                                                                        int          parts_no = cmap->cells[x + z * 20]
                                                                                                    .parts_no;
                                                                        MAP_CELL    *cell;
                                                                        int          turn;

                                                                        collision = parts_no == -1 ? NULL : NowDngMap->parts[parts_no].collision;
                                                                        cell = &NowDngMap
                                                                                    ->cells[x + z * 20];

                                                                        if (selectMapNo == DUNGEON_GALLERY_OF_TIME && parts_no >= 0x28 && parts_no < 0x2C) {
                                                                            cell->camera_dist -= 160.0f;
                                                                        }

                                                                        if (collision != NULL && cell->camera_dist <= 240.0f) {
                                                                            int dir = NowDngMap->cells[x + z * 20].direction;
                                                                            int base = parts_no == -1 ? 0 : NowDngMap->parts[parts_no].collision_turn;

                                                                            turn = dir;
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

                                                                for (int i = 0; i < 24; i++) {
                                                                    if (NowDngMap->boxes[i].used != 0) {
                                                                        collision = NowDngMap->box_collision_model;

                                                                        collision->SetPosition(NowDngMap->boxes[i].pos);
                                                                        colPolyNum += collision->PickUpNearPoly(&polys[colPolyNum], bound);
                                                                    }
                                                                }

                                                                colPolyNum = NowDngMap->CreateCollision(polys, bound, colPolyNum);
                                                                NowMonstorUnit->MoveCheck(pos, velo__2, lockOnTargetFlag);
                                                                mode = 1;

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
                                                            static int  snd_cnt;
                                                            static char init;
                                                            static int  id_cnt;
                                                            static char init2;

                                                            if (init == 0) {
                                                                snd_cnt = 0;
                                                                init = 1;
                                                            }

                                                            if (init2 == 0) {
                                                                id_cnt = 0;
                                                                init2 = 1;
                                                            }

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
                                                            float time;

                                                            if (CharaMain.motion_type.state.time >= (float) 0x12F && CharaMain.motion_type.state.time <= 318.0f) {
                                                                sceVu0FVECTOR run_pos;

                                                                sceVu0CopyVector(run_pos, CharaFrame->position);
                                                                CRunFx__2.Set(run_pos);
                                                            }

                                                            if (BtActStatus.movement_locked == 0) {
                                                                int dust = 0;

                                                                time = CharaMain.motion_type.state.time;

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
                                                            float turn = GamePad.GetRXf();

                                                            NowCamera__3->AddHeight(-GamePad.GetRYf());

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
                                                                    float         dist;

                                                                    locked->GetPosition(target);
                                                                    dist = DistVector(target, pos);
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

                                                                autoCamTrial();
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
                int           i;
                CSHOT_EFFECT *effects = NowShotEffect->effect;

                for (i = 0; i < 5; i++) {
                    effects[i].Initialize();
                }

                NowMonstorUnit->CleanViewMonstor(BtUraDongeon);

                CMonstorUnit *unit = (CMonstorUnit *) NowMonstorUnit;

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
                int           i;
                CSHOT_EFFECT *effects;

                printf("********** ext mem !!!\n");
                TexManager.DeleteTextureBlock(0x2A);
                TexManager.DeleteTextureBlock(0x26);
                TexManager.CleanUpBuffer();
                TexManager.CleanUpTextureList();
                effects = NowShotEffect->effect;

                for (i = 0; i < 5; i++) {
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
                    InitDunEnterMenu(0x17, selectMapNo, -1);
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
                    break;
                case BT_REQUEST_RUN_SCRIPT:
                    BtEventInfo.request = BT_REQUEST_NONE;
                    BtSystemScriptAfter();
                    BtEventInfo.script_no = BtEventInfo.chained_script_no;
                    gameTask = GAME_TASK_SCRIPT_START;
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

                    for (int i = 0; i < 8; i++) {
                        int atra_no = NowDngMap->atra[i].atra_no;

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
                    }

                    driveStepHold = 0;
                    CMonUnitHold = 0;
                    CEffectHold = 0;
                    gameTask = GAME_TASK_PLAY;
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
                int map_no;

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
                int next_task;
                int leaving;
                int hand_ok;

                if (StatusErrCheck(AILMENT_FREEZE) != 0) {
                    BtActStatus.movement_locked = 1;
                }

                HealingWater();

                CUserStatus *drain_status = UserStatus;

                hand_ok = 1;
                drain_status->water_drain_disable = 1;
                next_task = GAME_TASK_PLAY;
                leaving = 0;

                if (NowDngMap->map_type != 1 && selectMapNo == DUNGEON_DIVINE_BEAST_CAVE) {
                    sceVu0FVECTOR moved;
                    MoveCheckInfo info;
                    sceVu0FVECTOR parts_pos;
                    CBoxVu0       bound;
                    CCPoly        foot;
                    CCPoly       *polys;
                    int           mode;

                    sceVu0CopyVector(pos, CharaMain.pos);
                    velo__2[2] = 0.0f;
                    velo__2[0] = 0.0f;
                    WorkBuffer__2->used = 0;
                    polys = (CCPoly *) WorkBuffer__2->Alloc(0x7D0);
                    bound.max[0] = 20.0f + pos[0];
                    bound.max[1] = 20.0f + pos[1];
                    bound.max[2] = 20.0f + pos[2];
                    bound.min[0] = pos[0] - 20.0f;
                    bound.min[1] = pos[1] - 40.0f;
                    bound.min[2] = pos[2] - 20.0f;
                    int          i = 0;
                    int          offset;
                    unsigned int address;

                    colPolyNum = 0;

                    for (; (address = (unsigned int) NowDngMap, offset = i * (int) sizeof(CDungeonParts), ((CDungeonMap *) (offset + address))->parts[0].frame[0] != NULL); i++) {
                        CFrame *collision;
                        int     turn;

                        collision = i == -1 ? NULL : (address = (unsigned int) NowDngMap, ((CDungeonMap *) (offset + address))->parts[0].collision);

                        if (collision != NULL) {
                            CDungeonParts *part = &((CDungeonMap *) ((char *) NowDngMap + offset))->parts[0];

                            sceVu0CopyVector(parts_pos, part->frame_offset[0]);

                            CDungeonParts *turn_part = &((CDungeonMap *) ((char *) NowDngMap + offset))->parts[0];

                            turn = (int) turn_part->frame_turn[0];
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

                    colPolyNum = NowDranMapField->AddCollision(polys, colPolyNum, bound);
                    mode = 1;

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

                    CharaMain.SetPosition(pos);

                    if (pos[1] <= -30.0f) {
                        leaving = 1;
                    }
                }

                if (GamePad.Down(PadInput_OK) != 0 && BtActStatus.movement_locked == 0 && BtActStatus.in_water != 0) {
                    SetSystemMes(0x47, 0x5A, MES_POS_BOTTOM, 0, NULL, NULL);
                    DngMessMan.hide_timer = 0x5A;
                    hand_ok = 0;
                }

                if (GamePad.Down(PadInput_OK) != 0 && BtActStatus.ground_kind == 0xA && BtActStatus.movement_locked == 0 && UserStatus->cur_chara == CHARA_OSMOND) {
                    SetSystemMes(0x50, 0x5A, MES_POS_BOTTOM, 0, NULL, NULL);
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
                        }
                    }
                }

                SetBattleStyle(selectMapNo, 0);

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
                    } else {
                        goto eye_event;
                    }
                } else {
                    CDungeonEventData *state;

                eye_event:
                    state = NowEventMan->SearchDataSlotPos(pos);

                    if (state != NULL && state->event->chara_no != -1) {
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

                    if (UserStatus->CheckLife() != 0 && NowMonstorUnit->GetMonstorNum() <= 0) {
                        if (BtEventInfo.clear_script_no != -1) {
                            BtEventInfo.script_no = BtEventInfo.clear_script_no;
                            BtEventInfo.ext_memory = BtEventInfo.clear_script_ext_memory;
                            BtEventInfo.clear_script_no = -1;
                            BtEventInfo.clear_script_ext_memory = 0;
                            BtActStatus.action_busy = 0;
                            printf("dead script !!\n");
                            next_task = GAME_TASK_SCRIPT_START;
                            leaving = 1;
                        }
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
                if (BtMiniChrSelectNo == 2) {
                    ClearSystemMes();
                    driveStepHold = 1;
                    gameTask = GAME_TASK_EXIT_FADE_START;
                    ((CDngStatusData *) UserStatus)->SetDead();
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

                    goto chr_selected;
                }
            } else {
            chr_selected:
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
            int slot;

            CUserStatus *step_status = UserStatus;

            step_status->step_disable = 1;
            BtActStatus.camera_hold = 1;
            DngMessMan.enabled = false;

            if (ruby_effect_id != -1 && UserStatus->cur_chara == CHARA_RUBY) {
                NowMainEffect->OffEffect(ruby_effect_id);
                ruby_effect_id = -1;
            }

            slot = ((CDngStatusData *) UserStatus)->CheckActItemSlot(ITEM_REVIVAL_POWDER);

            if (slot != -1) {
                s8 chara;

                setUnitAmbientAnime(64.0f, 1.0f, 0.0f, 122.0f, 208.0f);
                chara = UserStatus->cur_chara;
                UserStatus->hp[chara] = UserStatus->max_hp[chara] >> 1;
                UserStatus->ailments[UserStatus->cur_chara] = 0;
                UserStatus->ailment_frames[UserStatus->cur_chara] = 0;
                SetSystemMes(0x3E, -1, MES_POS_BOTTOM, 0, NULL, NULL);
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

                    if (((CDngStatusData *) UserStatus)->GetLiveUnit() != 0 && UserStatus->party_size > 1) {
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
                    ((CDngStatusData *) UserStatus)->SetDead();
                }
#else
                gameTask = GAME_TASK_EXIT_FADE_START;
                ((CDngStatusData *) UserStatus)->SetDead();
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
            case -1:
                break;
        }

        if (rogoY3 >= 0x20 && UserStatus->minimap_status != 3) {
            infoMap = 1;
            infoMapOld = 1;
        }
    }

    motionDrive();
}

void motionDrive() {
    sceVu0FVECTOR pos;
    sceVu0FVECTOR rotation;
    sceVu0FVECTOR hand;
    int           chara = UserStatus->cur_chara;
    int           i;

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

            if (tail != NULL) {
                static float y = 0.0f;

                y += 0.35699910f;

                if (!(y < PI_SHORT)) {
                    y = y - TWO_PI_SHORT;
                }

                tail->SetRotType(2);
                tail->SetRotation(0.0f, AngleLimit(y), 0.0f);

                if (tail_shadow != NULL) {
                    tail_shadow->SetRotType(2);
                    tail_shadow->SetRotation(0.0f, AngleLimit(y), 0.0f);
                }
            }
        }

        if (lockOnTargetFlag != 0 && UserStatus->cur_chara == CHARA_XIAO && CharaMainHandViewFlag == 0) {
            CharaMain.motion_type.state.look_frame = (CFrameVu1 *) CharaFrame->SearchFrame("jnt2_1");
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
        WEAPON_HAVE *weapon;
        CUserStatus *status;
        s8          *slots;
        s8          *check_slots;
        s8           owner;
        s8           check_owner;

        status = UserStatus;
        owner = check_owner = status->cur_chara;
        slots = status->equipped_weapon_slot;
        check_slots = status->equipped_weapon_slot;
        weapon = &status->chara_weapons[owner][(s8) slots[owner]];

        if (weapon->durability_f <= 10.0f && status->chara_weapons[check_owner][(s8) check_slots[check_owner]].item_no != defWeapon__6[(s8) owner]) {
            static int  warning_cnt;
            static char init;
            int         wait;

            if (init == 0) {
                warning_cnt = 0;
                init = 1;
            }

            wait = (int) weapon->durability_f * 10;

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

        float end = (float) Water_Splash.motion_type.motion_info[0].end;

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

    for (i = 0; i < 96; i++) {
        if (collision->active[i] == 0) {
            continue;
        }

        if (collision->hit[i].phase > 0) {
            collision->hit[i].phase--;
        } else {
            collision->hit[i].phase = collision->hit[i].ready_phase;
        }

        collision->hit[i].life--;

        if (collision->hit[i].life <= 0) {
            collision->active[i] = 0;
            collision->hit[i].scale_rate = 0.0f;
            continue;
        }

        collision->hit[i].radius += collision->hit[i].unk_40;

        if (collision->hit[i].scale_rate != 0.0f) {
            sceVu0ScaleVectorXYZ(collision->hit[i].base_point, collision->hit[i].base_point, collision->hit[i].scale_rate);
        }
    }

    if (StatusErrCheck(AILMENT_FREEZE) != 0 || StatusErrCheck(AILMENT_STAMINA) != 0) {
        CUserStatus *status = UserStatus;
        s8           owner = status->cur_chara;
        s16         *left = status->ailment_frames;

        if (left[status->cur_chara] > 0) {
            left[owner]--;

            CUserStatus *now = UserStatus;
            s8           who = now->cur_chara;

            if (now->ailment_frames[now->cur_chara] == 0) {
                now->ailments[who] = 0;
                BtActStatus.movement_locked = 0;
            }
        }
    }

    if (BtActStatus.special_cooldown > 0) {
        BtActStatus.special_cooldown--;
    }

    // The action gauge fills back up at a rate the weapon sets.
    if ((int) BtActStatus.action_gauge < 100) {
        if (BtActStatus.refill_frames < 10) {
            BtActStatus.refill_frames++;
        }

        float gain = (float) NowWeaponHave->speed / 30.0f;

        if (StatusErrCheck(AILMENT_GOO) != 0) {
            gain /= 2.0f;
        }

        if (StatusErrCheck(AILMENT_STAMINA) != 0) {
            gain *= 2.0f;
        }

        if (gain < 1.0f) {
            gain = 1.0f;
        }

        CUserStatus *status = UserStatus;
        s8          *who_ref = &status->cur_chara;

        if (status->cur_chara == CHARA_TOAN) {
            if (BtActStatus.swinging == 0) {
                gain *= 0.25f;
            } else {
                gain = 0.0f;
            }

            BtActStatus.swinging = 0;
        }

        if (*who_ref == CHARA_UNGAGA) {
            if (BtActStatus.swinging == 0) {
                gain *= 0.25f;
            } else {
                gain = 0.0f;
            }

            BtActStatus.swinging = 0;
        }

        s8 who = *who_ref;
        s8 who_copy = *who_ref;

        if (*who_ref == CHARA_OSMOND) {
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

        if (who == CHARA_XIAO) {
            gain *= 1.5f;
        }

        if (who_copy == CHARA_RUBY) {
            gain *= 1.5f;
        }

        BtActStatus.action_gauge += gain;

        if (!((int) BtActStatus.action_gauge < 100)) {
            BtActStatus.action_gauge = 100.0f;
            BtActStatus.gauge_flash = 10;
            BtActStatus.gauge_exhausted = 0;
        }
    } else if (BtActStatus.gauge_flash > 0) {
        BtActStatus.gauge_flash--;
    }

    if (BtActStatus.hud_shake_frames > 0) {
        BtActStatus.hud_shake_y = (int) (16.0f * (float) rand() / 2147483648.0f) - 8;
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
    s8          *healed_ref = &status->cur_chara;
    float        water_max = status->water_max[*healed_ref];
    float        water_now = status->water_now[*healed_ref];
    static int   heal_counter = 0;

    heal_counter++;

    if (!(heal_counter < 240)) {
        if ((NowWeaponHave->flags & 0x800) && status->hp[*healed_ref] > 0) {
            status->AddNowLife((s8) *healed_ref, 1, 255.0f);
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

    for (int i = 0; i < 4; i++) {
        CWeaponElFx[i].Step();
    }

    for (int i = 0; i < 32; i++) {
        HitValue[i].Step();
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

        float end = (float) NewChangeFx.motion_type.motion_info[0].end;

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

        BtActStatus.camera_shake_offset = 2.0f * (shake * (float) rand()) / 2147483648.0f - shake;
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

    for (int i = 0; i < 3; i++) {
        CBomb__2[i].Step();
    }

    for (int i = 0; i < 5; i++) {
        MasekiEffect[i].Step();
    }

    NowShockWave->Step();

    CSHOT_EFFECT *effects = NowShotEffect->effect;

    for (i = 0; i < 5; i++) {
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
    CTexture         *gold;
    int               i;
    int               item_slot;
    int               j;
    int               k;
    CDungeonEventMan *man;
    CDungeonMap      *map;

    NowDngMap = &MainDungeonMap;
    NowEventMan = &DngEventMan;
    ((CDngStatusData *) UserStatus)->LostGateKey();

    man = NowEventMan;

    for (i = 0; i < 96; i++) {
        CDungeonEventData *slot = &man->event[i];

        slot->event = NULL;
        slot->switch_on = false;
        slot->enabled = false;
        slot->hold = 0;
        slot->chara_done = -1;
    }

    map = NowDngMap;

    for (i = 0; i < 48; i++) {
        map->events[i].kind = DNG_EVENT_NONE;
        map->events[i].reset_flag = 0;
    }

    for (i = 0; i < 24; i++) {
        map->boxes[i].used = 0;
        map->boxes[i].lid_angle = 0.0f;
        map->boxes[i].trap_no = 0;
    }

    map->box_num = 0;

    for (i = 0; i < 8; i++) {
        map->atra[i].used = 0;
    }

    map->atra_num = 0;

    for (i = 0; i < 4; i++) {
        map->room_link[i].used = 0;
    }

    gold = GoldTex;

    for (item_slot = 0; item_slot < 32; item_slot++) {
        MainRandomItem.id[item_slot] = -1;
        MainRandomItem.pickup_event[item_slot] = -1;
        MainRandomItem.pickup_blocked[item_slot] = 0;
    }

    MainRandomItem.gold_texture = gold;

    for (item_slot = 0; item_slot < 32; item_slot++) {
        SubRandomItem.id[item_slot] = -1;
        SubRandomItem.pickup_event[item_slot] = -1;
        SubRandomItem.pickup_blocked[item_slot] = 0;
    }

    SubRandomItem.gold_texture = GoldTex;
    RandomItem = &MainRandomItem;

    NowDngMap->buildRandomMap(6, 1);

    s32 map_no = NowDngMap->map_type;

    NowDngMap->buildEventData(UserStatus->cur_floor, map_no, 1);
    NowDngMap->FlushCheckMask();
    NowDngMap->DrawMapCalc(NowDngMap->map_type);

    s32 event_no = NowDngMap->map_type;

    NowEventMan->SetupEvent(NowDngMap, event_no);

    NowDngMap = &UraDungeonMap;
    NowEventMan = &UraEventMan;
    UraDungeonMap = MainDungeonMap;
    UraEventMan = DngEventMan;

    for (k = 0; k < 96; k++) {
        CDungeonEventData *slot = &UraEventMan.event[k];

        slot->event = NULL;
        slot->switch_on = false;
        slot->enabled = false;
        slot->hold = 0;
        slot->chara_done = -1;
    }

    for (j = 0; j < 48; j++) {
        UraDungeonMap.events[j].kind = DNG_EVENT_NONE;
        UraDungeonMap.events[j].reset_flag = 0;
    }

    for (j = 0; j < 24; j++) {
        UraDungeonMap.boxes[j].used = 0;
        UraDungeonMap.boxes[j].lid_angle = 0.0f;
        UraDungeonMap.boxes[j].trap_no = 0;
    }

    UraDungeonMap.box_num = 0;

    for (j = 0; j < 8; j++) {
        UraDungeonMap.atra[j].used = 0;
    }

    UraDungeonMap.atra_num = 0;

    for (j = 0; j < 4; j++) {
        UraDungeonMap.room_link[j].used = 0;
    }

    UraDungeonMap.buildRandomMap(6, 0);

    s32 ura_map_no = UraDungeonMap.map_type;

    UraDungeonMap.buildEventData(UserStatus->cur_floor, ura_map_no, 0);
    UraDungeonMap.FlushCheckMask();
    s32 ura_draw_no = UraDungeonMap.map_type;

    UraDungeonMap.DrawMapCalc(ura_draw_no);

    s32 ura_event_no = UraDungeonMap.map_type;

    UraEventMan.SetupEvent(&UraDungeonMap, ura_event_no);

    NowDngMap = &MainDungeonMap;
    NowEventMan = &DngEventMan;
    UserStatus->ClearEventFlag();
    BtUraDongeon = 0;
    BtEquipMap = 0;
    BtEquipMasuisyou = 0;
}

void BtCleatFreeMap() {
    int i;
    s32 map_no = MainDungeonMap.map_type;

    MainDungeonMap.buildEventData(UserStatus->cur_floor, map_no, 1);
    MainDungeonMap.FlushCheckMask();
    s32 draw_no = MainDungeonMap.map_type;

    MainDungeonMap.DrawMapCalc(draw_no);

    s32 event_no = MainDungeonMap.map_type;

    DngEventMan.SetupEvent(&MainDungeonMap, event_no);

    CTexture *gold = GoldTex;

    for (i = 0; i < 32; i++) {
        MainRandomItem.id[i] = -1;
        MainRandomItem.pickup_event[i] = -1;
        MainRandomItem.pickup_blocked[i] = 0;
    }

    MainRandomItem.gold_texture = gold;
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
    int             count;
    BT_ENEMY_FLOOR *layout;
    int             monster;
    int             i;
    int             slot;

    MonstorModelBuffer.used = 0;

    s32           ura_dungeon = BtUraDongeon;
    CMonstorUnit *unit = NowMonstorUnit;

    unit->model_count = 0;
    unit->current_monster = 0;

    for (slot = 0; slot < 16; slot++) {
        unit->script[slot] = &MonstorScriptBuffer[slot];
    }

    unit->requested_event = -1;
    unit->CleanViewMonstor(ura_dungeon);
    BtSetEventExtendTable();

    for (i = 0; i < 320; i++) {
        MonsterTexAnim[i].Initialize();
    }

    TexManager.DeleteTextureBlock(0x2A);
    TexManager.DeleteTextureBlock(0x26);
    TexManager.CleanUpBuffer();
    TexManager.CleanUpTextureList();

    LoadFile("dun/etc/atrcol.mds", read_buffer, NULL);
    wait_now_loading_vsync();
    NowMonstorUnit->collision = LoadCollisionFile(read_buffer, &MonstorModelBuffer);

    CSHOT_EFFECT *effects = NowShotEffect->effect;

    for (i = 0; i < 5; i++) {
        effects[i].Initialize();
    }

    count = 0;

    if (ura == 0) {
        layout = BtEnemyLayoutList[selectMapNo];
    } else {
        layout = BtUraEnemyLayoutList[selectMapNo];
    }

    layout += UserStatus->cur_floor;

    for (monster = 0; monster < 9; monster++) {
        if (layout->monster[monster].monster_no == -1) {
            break;
        }

        count += NowMonstorUnit->SetupBaseModel(count, layout->monster[monster].monster_no, 0x26, &MonstorModelBuffer);
    }

    MemoryMapDump();
}

void EquipReAttach(CCharacter *equipment, int held_out) {
    CFrame *hand;

    if (held_out != 0) {
        hand = CharaHand.frame->SearchFrame("pati1");
    } else {
        hand = CharaMain.frame->SearchFrame("weapon");
    }

    equipment->SetPosition(0.0f, 0.0f, 0.0f);
    equipment->SetRotation(0.0f, 0.0f, 0.0f);
    equipment->frame->SetReference(hand);
}

void EquipWeaponFrame(CCharacter *weapon, int chara, int held_out) {
    CFrame *hand;
    int     i;

    NowWeapon = weapon;

    if (held_out != 0 && chara == CHARA_XIAO) {
        hand = CharaHand.frame->SearchFrame("pati1");
    } else {
        hand = CharaMain.frame->SearchFrame("weapon");
    }

    if (hand == NULL) {
        printf("no weapon frame !!\n");
        return;
    }

    float zero = 0.0f;

    NowWeapon->SetPosition(zero, zero, zero);
    NowWeapon->SetRotation(zero, zero, zero);
    NowWeapon->frame->SetReference(hand);
    NowWeapon->Step();

    for (i = 0; i < 32; i++) {
        CWeaponFx.used[i] = 0;
    }

    CWeaponFx.colour[0].r = CWeaponFx.colour[2].r = 0x40;
    CWeaponFx.colour[0].g = CWeaponFx.colour[2].g = 0x40;
    CWeaponFx.colour[0].b = CWeaponFx.colour[2].b = 0x40;
    CWeaponFx.colour[1].r = CWeaponFx.colour[3].r = 6;
    CWeaponFx.colour[1].g = CWeaponFx.colour[3].g = 6;
    CWeaponFx.colour[1].b = CWeaponFx.colour[3].b = 6;
    CWeaponFx.cursor = 0;
    CWeaponFx.timer = 0;
    CWeaponFx.InitSet(NowWeapon->frame, "dcol0", "dcol1");
    SetWeaponColor();
}

void LoadWeapon2(unsigned int *crash_data, unsigned int *default_data, unsigned int *main_data, int chara, int reload) {
    int  weapon_kind[6] = {1, 4, 6, 5, 10, 7};
    int  weapon_first[6] = {ITEM_WEAPON_DAGGER_BROKEN, ITEM_WEAPON_WOODENSLINGSHOT_BROKEN, ITEM_WEAPON_MALLET_BROKEN, ITEM_WEAPON_GOLD_RING_BROKEN, ITEM_WEAPON_FIGHTING_STICK_BROKEN, ITEM_WEAPON_MACHINE_GUN_BROKEN};
    char weapon_name[64];
    char weapon_model_path[64];
    // The kind each character's weapon counts as; nothing reads it any more.
    int *kind = &weapon_kind[chara];

    // A reload gives the weapon blocks back before the new models go in.
    if (reload != 0) {
        TexManager.DeleteTextureBlock(0x1D);
        TexManager.DeleteTextureBlock(0x10);
        TexManager.CleanUpBuffer();
        TexManager.CleanUpTextureList();
        WeaponModelBuffer.used = 0;
        WEffectModelBuffer.used = 0;
    }

    BtGetWeaponNamePath2(weapon_name, weapon_model_path, chara, 0);
    CrashWeapon.Initialize();
    CrashWeapon.LoadPackData3(crash_data, weapon_model_path, &WeaponModelBuffer, 0x1D, &WeaponModelBuffer, 1, 0);

    BtGetWeaponNamePath2(weapon_name, weapon_model_path, chara, 1);
    DefaultWeapon.Initialize();
    DefaultWeapon.LoadPackData3(default_data, weapon_model_path, &WeaponModelBuffer, 0x1D, &WeaponModelBuffer, 1, 0);

    CUserStatus *status = UserStatus;
    s8           owner = status->cur_chara;

    // The weapon's item number, counted from its character's first weapon.
    int model_no = status->chara_weapons[owner][status->equipped_weapon_slot[owner]].item_no;

    model_no -= weapon_first[chara];
    BtGetWeaponNamePath2(weapon_name, weapon_model_path, chara, model_no);
    MainWeapon.Initialize();
    MainWeapon.LoadPackData3(main_data, weapon_model_path, &WeaponModelBuffer, 0x1D, &WeaponModelBuffer, 1, 0);
    EquipWeaponFrame(&MainWeapon, chara, CharaMainHandViewFlag);

    // The effect models take whatever the weapon models leave.
    u8 *free_start;
    u8 *weapon_end;
    int used = WeaponModelBuffer.used;

    weapon_end = WeaponModelBuffer.base + used * 16;
    free_start = weapon_end;
    s64 free_size = 0x33450 - CharaModelBuffer.used - used;

    WEffectModelBuffer.base = free_start;
    WEffectModelBuffer.limit = free_size;
    WEffectModelBuffer.used = 0;

    CUserStatus *equipped = UserStatus;
    s8           chara_no = equipped->cur_chara;

    WEAPON_HAVE *have = &equipped->chara_weapons[chara_no][equipped->equipped_weapon_slot[chara_no]];

    MenuWeaponSpSet(NowWeapon, have);
    BtActStatus.action_gauge = 100.0f;
    BtActStatus.gauge_exhausted = 0;
}

void SwordDmgCheck1(float amount, int kind) {
    int          broke;
    CUserStatus *status = UserStatus;
    s8           chara = status->cur_chara;
    WEAPON_HAVE *weapon = &status->chara_weapons[chara][status->equipped_weapon_slot[chara]];
    float        was = weapon->durability_f;
    float        warn = (float) (weapon->durability * 0.2);

    broke = BattleSubWeaponDmg(amount, kind);

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

    if (hand != NULL) {
        hand->GetWorldPosition(world, offset);
        WeaponCrashEffect.Set(world, spread, 4, 1.5f, 0.01f, 0.01f, 0.3f, 0x20, pos[1]);
    }

    if (broke == 1) {
        EquipWeaponFrame(&CrashWeapon, UserStatus->cur_chara, CharaMainHandViewFlag);
        s8 owner = UserStatus->cur_chara;

        if (owner == 5 || (s8) owner == 3) {
            NowMainEffect = &CharaMainEffectCrash;
            BtActStatus.gun_type = 0;
            BtActStatus.action_on = 0;
        }
    }

    if (broke == 2) {
        EquipWeaponFrame(&DefaultWeapon, UserStatus->cur_chara, CharaMainHandViewFlag);
        s8 owner = UserStatus->cur_chara;

        if (owner == 5 || (s8) owner == 3) {
            NowMainEffect = &CharaMainEffectCrash;
            BtActStatus.gun_type = 0;
            BtActStatus.action_on = 0;
        }
    }
}

// clang-format off
/* The colour each weapon element gives the trail the weapon leaves. */
u8 wep_rgb[5][3] = {
    {0xFF, 0x3C, 0x00}, {0x64, 0xC8, 0xFF}, {0xFF, 0xFF, 0x64}, {0x8A, 0xFF, 0xBC}, {0xE3, 0x96, 0xAD},
};
// clang-format on

void SetWeaponColor() {
    u8 red, green, blue;
    s8 element = NowWeaponHave->best_elem;

    if (element == WEAPON_ELEMENT_NONE) {
        CWeaponFx.colour[0].r = CWeaponFx.colour[2].r = 0x40;
        CWeaponFx.colour[0].g = CWeaponFx.colour[2].g = 0x40;
        CWeaponFx.colour[0].b = CWeaponFx.colour[2].b = 0x40;
        CWeaponFx.colour[1].r = CWeaponFx.colour[3].r = 6;
        CWeaponFx.colour[1].g = CWeaponFx.colour[3].g = 6;
        CWeaponFx.colour[1].b = CWeaponFx.colour[3].b = 6;
    } else if (element >= 0 && element < 6) {
        blue = wep_rgb[element][2] >> 2;
        green = wep_rgb[element][1] >> 2;
        red = wep_rgb[element][0] >> 2;

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
    }

    return NULL;
}

void MainChara_Effect(BT_SHOT_EFFECT *effect, unsigned int *data, int reload) {
    CharaMainEffect.Initialize();
    CharaMainEffectCrash.Initialize();

    if (effect == NULL) {
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
        printf("effect entry err!!\n");
    } else {
        printf("set eff sucsess !\n");
    }

    if (UserStatus->cur_chara == CHARA_OSMOND) {
        unsigned int *crash = ozumond_default_effect;

        CharaMainEffectCrash.Entry2(MyEffectEntry_Tbl[9], crash, 0x10, &WEffectModelBuffer, 6);
    }

    if (UserStatus->cur_chara == CHARA_RUBY) {
        unsigned int *crash = ozumond_default_effect;

        CharaMainEffectCrash.Entry2(MyEffectEntry_Tbl[3], crash, 0x10, &WEffectModelBuffer, 6);
    }

    NowMainEffect = &CharaMainEffect;
}

void LoadChara2(int chara, int keep_place, unsigned int *chara_data, unsigned int *crash_data, unsigned int *default_data, unsigned int *main_data) {
    CFrameAttr    frame_attr;
    sceVu0FVECTOR pos;
    sceVu0FVECTOR rotation;
    int           i;

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

    for (i = 0; i < 64; i++) {
        CharaMainTexAnim[i].Initialize();
    }

    CharaMain.InitializeTexAnime(CharaMainTexAnim, 64);
    CharaMain.LoadPackData2(chara_data, "base.cfg", &CharaModelBuffer, 0x11, &CharaModelBuffer, 0);
    NowWeapon = NULL;
    CharaHand.Initialize();

    if (chara == CHARA_XIAO) {
        CFrameAttr hand_attr;

        hand_attr.clip_enable = true;
        hand_attr.unk_09 = 0;
        CharaHand.LoadPackData(chara_data, "hand_up.cfg", &CharaModelBuffer, &CharaModelBuffer, NULL);
        CharaHand.frame->SetAttr(hand_attr, 1, 0);
    }

    if (chara == CHARA_RUBY) {
        CFrameAttr hand_attr;

        hand_attr.clip_enable = true;
        hand_attr.unk_09 = 0;
        CharaHand.LoadPackData(chara_data, "hand_up.cfg", &CharaModelBuffer, &CharaModelBuffer, NULL);
        CharaHand.frame->SetAttr(hand_attr, 1, 0);
    }

    CharaMain.frame->SetAttr(frame_attr, 1, 4);

    if (keep_place == 0) {
        CharaMain.frame->SetPosition(pos);
        CharaMain.frame->SetRotation(rotation[0], rotation[1], rotation[2]);
        CharaMain.SetPosition(pos);
        CharaMain.SetRotation(rotation[0], rotation[1], rotation[2]);
    }

    CharaFrame = (CFrameVu1 *) CharaMain.frame;

    if (chara == CHARA_OSMOND) {
        LOADTEXTURE_INFO2 info[2] = {
            {NULL, 0x46, 0},
            {NULL, 0x46, 0}
        };
        int size;

        u_int *hand_texture = GetPackFile(chara_data, "c05w_h.img", &size);
        u8    *at = CharaModelBuffer.base + CharaModelBuffer.used * 16;

        memcpy(at, hand_texture, size);
        size = (((size >> 6) + 1) << 6) >> 4;
        CharaModelBuffer.Alloc(size);
        info[0].name = (char *) at;
        TexManager.DeleteTextureBlock(0x46);
        TexManager.LoadTextureBlockEX(0x46, info);

        u_int *effect = GetPackFile(chara_data, "mgan01.chr", &size);

        ozumond_default_effect = (unsigned int *) (CharaModelBuffer.base + CharaModelBuffer.used * 16);
        memcpy(ozumond_default_effect, effect, size);
        size = (((size >> 6) + 1) << 6) >> 4;
        CharaModelBuffer.Alloc(size);
    }

    if (chara == CHARA_RUBY) {
        int    size;
        u_int *effect = GetPackFile(chara_data, "c05_f03.chr", &size);

        ozumond_default_effect = (unsigned int *) (CharaModelBuffer.base + CharaModelBuffer.used * 16);
        memcpy(ozumond_default_effect, effect, size);
        size = (((size >> 6) + 1) << 6) >> 4;
        CharaModelBuffer.Alloc(size);
    }

    // The weapon models take whatever the character models leave.
    s32 used = CharaModelBuffer.used;
    u8 *free_start = CharaModelBuffer.base + used * 16;
    s64 free_size = 0x33450 - used;

    WeaponModelBuffer.base = free_start;
    WeaponModelBuffer.limit = free_size;
    WeaponModelBuffer.used = 0;
    LoadWeapon2(crash_data, default_data, main_data, chara, 0);
    MemoryMapDump();
}

static void LoadData() {
    CFrameAttr frame_attr;
    char       weapon_model_path[64];
    char       weapon_name[64];
    char       weapon_effect_name[64];
    char       steal_model_name[64];
    int        character_size;
    int        weapon_0_size;
    int        weapon_1_size;
    int        equipped_weapon_size;

    // Configure the model attributes used by the loaded dungeon objects.
    frame_attr.fog_enable = true;
    frame_attr.clip_depth = 100.0f;
    frame_attr.clip_enable = false;
    frame_attr.program_option = 0;

    UserStatus->cur_chara = 0;
    BtGetWeaponNamePath3(weapon_name, weapon_effect_name, UserStatus->chara_weapons[0][UserStatus->equipped_weapon_slot[0]].item_no);
    printf("[%d] %s\n", UserStatus->chara_weapons[0][UserStatus->equipped_weapon_slot[0]].item_no, weapon_name);

    strcpy(weapon_model_path, "commenu/weapon/");
    strcat(weapon_model_path, weapon_name);

    // Load the character and weapon data into consecutive 64-byte regions.
    LoadFile("dun/mainchara/c01d.chr", read_buffer, &character_size);
    unsigned int  aligned_size = ((character_size >> 6) + 1) << 6;
    unsigned int *weapon_data_0 = read_buffer + (aligned_size >> 2);

    LoadFile("commenu/weapon/c01w00.chr", weapon_data_0, &weapon_0_size);
    aligned_size = ((weapon_0_size >> 6) + 1) << 6;
    unsigned int *weapon_data_1 = weapon_data_0 + (aligned_size >> 2);

    LoadFile("commenu/weapon/c01w01.chr", weapon_data_1, &weapon_1_size);
    aligned_size = ((weapon_1_size >> 6) + 1) << 6;
    unsigned int *equipped_weapon_data = weapon_data_1 + (aligned_size >> 2);

    LoadFile(weapon_model_path, equipped_weapon_data, &equipped_weapon_size);
    wait_now_loading_vsync();
    LoadChara2(UserStatus->cur_chara, 1, read_buffer, weapon_data_0, weapon_data_1, equipped_weapon_data);
    float chara_z = 150.0f;

    CharaFrame->SetPosition(200.0f, 0.0f, chara_z);

    BT_SHOT_EFFECT *main_effect = Get_Main_EffectPtr(UserStatus->cur_chara, 0);
    LoadFile("dun/mainchara/wep_eff/c01_fuusya.chr", read_buffer, NULL);
    wait_now_loading_vsync();
    MainChara_Effect(main_effect, read_buffer, 0);

    if (SndVoiceLoad(0) != 0) {
        printf("voice err \n");
    } else {
        printf("snd load\n");
    }

    // Load the shared dungeon pack and its models.
    unsigned int *main_pack = read_buffer;

    LoadFile("dun/pack/maindat.pac", main_pack, NULL);
    wait_now_loading_vsync();
    cursorFrame = LoadMDSFilePack(read_buffer, "dun/etc/cursor.mds", &MainModelBuffer);
    bombCursorFrame = LoadMDSFilePack(read_buffer, "dun/etc/bomblock.mds", &MainModelBuffer);
    bicCursorFrame = LoadMDSFilePack(read_buffer, "dun/etc/bic.mds", &MainModelBuffer);
    bicCursorFrame->SetScale(0.7f, 0.7f, 0.7f);

    NowDngMap->atra_model = LoadMDSFilePack(read_buffer, "dun/etc/atr.mds", &MainModelBuffer);
    NowDngMap->collision_model = LoadCollisionFilePack(read_buffer, "dun/etc/atrcol.mds", &MainModelBuffer);
    NowDngMap->atra_model->SetAttr(frame_attr, 1, 0);

    NewChangeFx.LoadPackData2((unsigned int *) GetPackFile(read_buffer, "ceffe_s.chr", NULL), "info.cfg", &MainModelBuffer, 11, &MainModelBuffer, 0);
    itemOpenSmallFx.LoadPack(read_buffer, "igetfx", &MainModelBuffer, &MotionData, &itemOpenSmallFx_info, 0);
    itemOpenBigFx.LoadPack(read_buffer, "igetfxb", &MainModelBuffer, &MotionData, &itemOpenBigFx_info, 0);

    Water_Splash.LoadPackData2((unsigned int *) GetPackFile(read_buffer, "water_sp.chr", NULL), "info.cfg", &MainModelBuffer, 6, &MainModelBuffer, 0);

    unsigned int *effect_data = (unsigned int *) GetPackFile(read_buffer, "trap01.chr", NULL);
    Trap_Circle.InitializeTexAnime(Trap_Circle_TexAnim, 16);
    Trap_Circle.LoadPackData2(effect_data, "info.cfg", &MainModelBuffer, 71, &MainModelBuffer, 0);
    Trap_Circle.frame->SetAttr(frame_attr, 1, 0x40);

    sprintf(steal_model_name, "kara0%d.mds", selectMapNo + 1);
    unsigned int *steal_model_data = (unsigned int *) GetPackFile(read_buffer, steal_model_name, NULL);
    StealModel = (CFrameVu1 *) LoadMDSFile(steal_model_data, &MainModelBuffer, 0, NULL, NULL);

    MasekiModelBuffer.used = 0;
    LoadFile("dun/effect/maseki_ex.chr", read_buffer, NULL);
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

        status->hp[status->cur_chara] = status->max_hp[status->cur_chara];
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
            int            guard = UserStatus->defense[UserStatus->cur_chara];
            int            monster;
            int            roll;
            COLLISION_HIT *hit = &NowColData->hit[hit_no];

            if (StatusErrCheck(AILMENT_STAMINA) != 0) {
                guard *= 2;
            }

            damage -= guard;

            if (damage <= 0) {
                damage = 0;
            }

            int owner = NowColData->hit[hit_no].owner;

            monster = -1;

            if (owner != -1) {
                monster = (owner - 200) / 5;
                CMonstorUnit *unit = NowMonstorUnit;

                if (monster >= 0 && monster < 16) {
                    unit->monster[monster].last_hit_damage = damage;
                }

                NowMonstorUnit->chara[monster][0].GetPosition(from);
            }

            roll = (int) (100.0f * (float) rand() / 2147483648.0f);

            // A hit that drains takes a fifth of the player's money and gives it to
            // whatever landed it.
            if ((hit->flags & 0x40000) && roll < 20 && monster != -1) {
                u16 *wallet = &UserStatus->money;
                u16  had = UserStatus->money;

                if (had > 10) {
                    int drained = had / 5;

                    *wallet -= drained;
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
                int slot = ((CDngStatusData *) UserStatus)->CheckActItemSlot(ITEM_ANTI_FREEZE_AMULET);

                if (slot == -1) {
                    BtSetStatusErr(AILMENT_FREEZE);
                } else {
                    // &who->active_item_vol[slot], spelled so the index is added first.
                    CUserStatus *who = UserStatus;
                    unsigned int address = slot * sizeof(s32);

                    address += (unsigned int) who;
                    s32 *vol = &((CUserStatus *) address)->active_item_vol[0];

                    if (--(*vol) <= 0) {
                        DelActiveItem(slot + 1);
                        DngMessMan.message = 0xB7;
                        DngMessMan.insert_mes_1 = GetCommonItemDataSystemMsg(ITEM_ANTI_FREEZE_AMULET);
                        DngMessMan.timer = 0xB4;
                        DngMessMan.steev_window = false;
                    }
                }
            }

            if ((hit->flags & 0x200) && roll < 0x41 && (UserStatus->ailments[UserStatus->cur_chara] & AILMENT_POISON) == 0) {
                int slot = ((CDngStatusData *) UserStatus)->CheckActItemSlot(ITEM_ANTIDOTE_AMULET);

                if (slot == -1) {
                    BtSetStatusErr(AILMENT_POISON);
                    DngMessMan.message = 0xBB;
                    DngMessMan.timer = 0xB4;
                    DngMessMan.steev_window = false;
                } else {
                    CUserStatus *who = UserStatus;
                    unsigned int address = slot * sizeof(s32);

                    address += (unsigned int) who;
                    s32 *vol = &((CUserStatus *) address)->active_item_vol[0];

                    if (--(*vol) <= 0) {
                        DelActiveItem(slot + 1);
                        DngMessMan.message = 0xB7;
                        DngMessMan.insert_mes_1 = GetCommonItemDataSystemMsg(ITEM_ANTIDOTE_AMULET);
                        DngMessMan.timer = 0xB4;
                        DngMessMan.steev_window = false;
                    }
                }
            }

            if ((hit->flags & 0x400) && roll < 0x41 && (UserStatus->ailments[UserStatus->cur_chara] & AILMENT_CURSE) == 0) {
                int slot = ((CDngStatusData *) UserStatus)->CheckActItemSlot(ITEM_ANTICURSEAMULET);

                if (slot == -1) {
                    BtSetStatusErr(AILMENT_CURSE);
                    DngMessMan.message = 0xB9;
                    DngMessMan.timer = 0xB4;
                    DngMessMan.steev_window = false;
                } else {
                    CUserStatus *who = UserStatus;
                    unsigned int address = slot * sizeof(s32);

                    address += (unsigned int) who;
                    s32 *vol = &((CUserStatus *) address)->active_item_vol[0];

                    if (--(*vol) <= 0) {
                        DelActiveItem(slot + 1);
                        DngMessMan.message = 0xB7;
                        DngMessMan.insert_mes_1 = GetCommonItemDataSystemMsg(ITEM_ANTICURSEAMULET);
                        DngMessMan.timer = 0xB4;
                        DngMessMan.steev_window = false;
                    }
                }
            }

            if ((hit->flags & 0x800) && roll < 0x41 && (UserStatus->ailments[UserStatus->cur_chara] & AILMENT_GOO) == 0) {
                int slot = ((CDngStatusData *) UserStatus)->CheckActItemSlot(ITEM_ANTIGOO_AMULET);

                if (slot == -1) {
                    BtSetStatusErr(AILMENT_GOO);
                    DngMessMan.message = 0xBA;
                    DngMessMan.timer = 0xB4;
                    DngMessMan.steev_window = false;
                } else {
                    CUserStatus *who = UserStatus;
                    unsigned int address = slot * sizeof(s32);

                    address += (unsigned int) who;
                    s32 *vol = &((CUserStatus *) address)->active_item_vol[0];

                    if (--(*vol) <= 0) {
                        DelActiveItem(slot + 1);
                        DngMessMan.message = 0xB7;
                        DngMessMan.insert_mes_1 = GetCommonItemDataSystemMsg(ITEM_ANTIGOO_AMULET);
                        DngMessMan.timer = 0xB4;
                        DngMessMan.steev_window = false;
                    }
                }
            }

            // A hit that halves takes half of what the player can hold.
            if (hit->flags & 0x80000) {
                damage = UserStatus->max_hp[UserStatus->cur_chara] >> 1;
            }

            if (NowColData->hit[hit_no].kind == 3) {
                sceVu0CopyVector(blow, NowColData->hit[hit_no].pos);
                GamePad.SetVibration(1, 0xE6, 0x16);

                if (BtActStatus.action_no == 9) {
                    sceVu0CopyVector(blowVelo, NowColData->hit[hit_no].velocity);
                    velo__2[0] = blowVelo[0] / 10.0f;
                    velo__2[2] = blowVelo[2] / 10.0f;

                    if (NowColData->hit[hit_no].target_mask != 3) {
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

                    sceVu0CopyVector(blowVelo, NowColData->hit[hit_no].velocity);
                    unitBlowActionRot(blowVelo);

                    if (NowColData->hit[hit_no].target_mask != 3) {
                        NowColData->active[hit_no] = 0;
                    }

                    if (damage < UserStatus->hp[UserStatus->cur_chara]) {
                        BtActStatus.action_on = 5;
                        BtActStatus.motion_no = 6;
                        CharaMain.motion_type.state.time = (float) CharaMain.motion_type.motion_info[6].start;
                    } else {
                        BtActStatus.action_on = 4;
                        BtActStatus.motion_no = 4;
                        CharaMain.motion_type.state.time = (float) CharaMain.motion_type.motion_info[4].start;
                    }

                    driveNoInterpolate = 1;
                    lockOnTargetFlag = 0;
                    BtActStatus.recoil_frames = 0xA0;
                }
            }

            COLLISION_HIT *hits = NowColData->hit;
            int            kind = hits[hit_no].kind;

            if (kind == 2 || hits[hit_no].kind == 4) {
                sceVu0FVECTOR at;
                sceVu0FVECTOR away;

                sceVu0CopyVector(at, hits[hit_no].pos);
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

                    if (NowColData->hit[hit_no].target_mask != 3) {
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

                    if (NowColData->hit[hit_no].target_mask != 3) {
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
                        CharaMain.motion_type.state.time = (float) CharaMain.motion_type.motion_info[4].start;
                    }

                    driveNoInterpolate = 1;
                    BtActStatus.recoil_frames = 0x50;

                    if (NowColData->hit[hit_no].kind == 4) {
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

        status->hp[status->cur_chara] = status->max_hp[status->cur_chara];
    }

    return taken;
}

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
            float turned = unitRotation((CFrameVu1 *) CharaFrame, heading);
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
        float throw_frame[6] = {548.0f, 452.0f, 267.0f, 347.0f, 188.0f, 321.0f};
        float end = (float) CharaMain.motion_type.motion_info[27].end;
        float start = throw_frame[UserStatus->cur_chara];

        if (CharaMain.motion_type.state.time >= start && CharaMain.motion_type.state.time <= end) {
            SndSePlay(SE_THROW, -1, 0);

            s16 *slots = UserStatus->active_item;

            if (slots[itemNowSel + 3] == 1) {
                slots[itemNowSel] = -1;
                slots[itemNowSel + 3] = 0;
            } else {
                slots[itemNowSel + 3]--;
            }

            if (lockOnTargetFlag == 0) {
                sceVu0FVECTOR vector;
                sceVu0FVECTOR from;

                if (activeItem.model[8] != -1) {
                    activeItem.models->DeleteModel(activeItem.model[8]);
                }

                activeItem.model[8] = -1;

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

                if (activeItem.model[8] != -1) {
                    activeItem.models->DeleteModel(activeItem.model[8]);
                }

                activeItem.model[8] = -1;

                float   lock_offset[4] = {0.0f, 0.0f, 0.0f, 1.0f};
                CFrame *hand = CharaMain.frame->SearchFrame("item");

                if (hand != NULL) {
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

        float end = (float) CharaMain.motion_type.motion_info[27].end;
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
    float frame;
    float now;

    if (BtActStatus.action_on != 3) {
        return;
    }

    float drink_frame[6] = {628.0f, 474.0f, 285.0f, 474.0f, 474.0f, 474.0f};

    BtActStatus.motion_no = 0x1C;
    frame = drink_frame[UserStatus->cur_chara];
    now = CharaMain.motion_type.state.time;

    if (now >= frame && now < frame + 0.3f) {
        SndSePlay(SE_DRINK, -1, 0);
    }

    if (now >= frame + 10.0f && now < frame + 10.3f) {
        setUnitAmbientAnime(64.0f, 1.0f, 0.0f, 122.0f, 208.0f);
        SndSePlay(SE_EFFECT_CHIME, -1, 0);
    }

    float end = (float) CharaMain.motion_type.motion_info[28].end;

    if (CharaMain.motion_type.state.time >= end - 1.0f && CharaMain.motion_type.state.time <= end) {
        s32 running = activeItem.item[activeItem.now];

        usedActiveItem(UserStatus, running);

        s16 *slots = UserStatus->active_item;

        // The last use takes the item away; anything else just spends one.
        if (slots[itemNowSel + 3] == 1) {
            slots[itemNowSel] = -1;
            slots[itemNowSel + 3] = 0;
        } else {
            slots[itemNowSel + 3]--;
        }

        if (activeItem.model[8] != -1) {
            activeItem.models->DeleteModel(activeItem.model[8]);
        }

        activeItem.model[8] = -1;
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

/* How long until Ozumond's action plays its sound again, in frames. */
s32 ozumond_snd_cnt;

/* How long until the player can be damaged again, in frames. */
s32 dmg_check_wait;

void BattleActionOn_Jinn() {
    if (BtActStatus.action_on == 0 && (s32) BtActStatus.action_gauge == 100) {
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
    int   i;

    // A poisoned weapon hits twice as hard.
    if (StatusErrCheck(AILMENT_STAMINA) != 0) {
        damage *= 2;
    }

    if (BtActStatus.action_no == 11) {
        if (GamePad.On(PadInput_OK) == 0) {
            BtActStatus.action_step = 1;
        }

        BtActStatus.motion_no = 11;

        float end = (float) chara->motion_type.motion_info[11].end;

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

                if (hand != NULL) {
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

                    CSHOT *shots = NowShotData;

                    for (i = 0; i < 12; i++) {
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

                    BtActStatus.frames_since_attack = 0;
                }

                SwordDmgCheck1(1.0f, 0);
            }
        } else {
#ifdef PAL_TIMING
            if (now > 21.0f && now < 21.8f) {
#else
            if (now > 21.0f && now < 21.5f) {
#endif
                SndSePlay(SE_CHARA_ACTION_2, -1, 0);
                SndSePlay(SE_CHARA_SHOUT_2, -1, 0);

                sceVu0FVECTOR world;
                sceVu0FVECTOR vector;
                float         offset[4] = {0.0f, 0.0f, 0.0f, 1.0f};
                CFrame       *hand = NowWeapon->frame->SearchFrame("eff30");

                if (hand != NULL) {
                    hand->GetWorldPosition(world, offset);
                    setShotVector(vector, 3.5f, viewAngleH__2, viewAngleV__2);

                    CSHOT *shots = NowShotData;

                    for (i = 0; i < 12; i++) {
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

                    BtActStatus.frames_since_attack = 0;
                }

                SwordDmgCheck1(1.0f, 0);
            }
        }

        float end = (float) chara->motion_type.motion_info[13].end;

        if (now >= end - 2.0f && now <= end) {
            BtActStatus.unk_028 = 0;
            BtActStatus.motion_no = 0;
            BtActStatus.action_on = 0;

            if (lockOnTargetFlag != 0) {
                BtActStatus.motion_no = 0x12;
            }

            // Retail tests the task here and does nothing with the answer.
            if (gameTask == GAME_TASK_EYE_CAMERA) {
                now = now;
            }
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
    if (BtActStatus.action_on == 0 && (s32) BtActStatus.action_gauge == 100) {
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

    damage = (int) ((float) damage * scale);

    // A poisoned weapon hits twice as hard.
    if (StatusErrCheck(AILMENT_STAMINA) != 0) {
        damage *= 2;
    }

    // A repeat shot reuses the start and the way the first one took.
    if (repeat != 0) {
        if (NowMainEffect->Set(ruby_effect_pos, ruby_effect_vec, 3, element, 0, NULL, 1) != -1) {
            NowMainEffect->SetWepStatus(NowWeaponHave->flags);
            NowMainEffect->SetVsMonster(NowWeaponHave->vs_monster);
            NowMainEffect->SetDmg(damage);
            NowMainEffect->SetRandomRate(2.0f);
            NowMainEffect->SetNoSound();
        }

        return;
    }

    if (aimed == 0) {
        int shot_no;

        getFramePos((CFrameVu1 *) chara->frame, "weapon", ruby_effect_pos);

        if (lockOnTargetFlag == 0) {
            getCharacterVector(ruby_effect_vec, 0.0f);
            ruby_effect_vec[0] += ruby_effect_pos[0];
            ruby_effect_vec[1] += ruby_effect_pos[1];
            ruby_effect_vec[2] += ruby_effect_pos[2];
            shot_no = NowMainEffect->Set(ruby_effect_pos, ruby_effect_vec, 3, element, 0, NULL, 1);
        } else {
            sceVu0CopyVector(ruby_effect_vec, BtActStatus.target_position);
            shot_no = NowMainEffect->Set(ruby_effect_pos, ruby_effect_vec, 3, element, 0, NULL, 1);
        }

        if (shot_no == -1) {
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

    if (NowMainEffect->Set(ruby_effect_pos, ruby_effect_vec, 3, element, 0, NULL, 1) != -1) {
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

        if (aimed == 0) {
#ifdef PAL_TIMING
            if (now >= 125.5f && now <= 126.5f && BtActStatus.action_step == 0) {
#else
            if (now >= 126.0f && now < 126.3f && BtActStatus.action_step == 0) {
#endif
                sceVu0FVECTOR pos;
                sceVu0FVECTOR aim;

                BtActStatus.motion_no = 14;
                BtActStatus.action_no = 14;
                BtActStatus.charge_time = 0.0f;
                ruby_effect_id = NowMainEffect->Set(pos, aim, 3, element, 0, chara->frame->SearchFrame("item"), 0);
                NowMainEffect->SetLoop(1);
            }

#ifdef PAL_TIMING
            if (now >= 128.0f && now < 128.4f) {
#else
            if (now >= 128.0f && now < 128.3f) {
#endif
                BattleActionShotRuby(chara, aimed, 1.0f, 0);
                BtActStatus.frames_since_attack = 0;
                SwordDmgCheck1(0.8f, 0);
                SndSePlay(SE_CHARA_ACTION, -1, 0);
                SndSePlay(SE_CHARA_SHOUT, -1, 0);
            }
        } else {
#ifdef PAL_TIMING
            if (now >= 126.0f && now <= 126.5f && BtActStatus.action_step == 0) {
#else
            if (now >= 126.0f && now < 126.3f && BtActStatus.action_step == 0) {
#endif
                sceVu0FVECTOR pos;
                sceVu0FVECTOR aim;

                BtActStatus.motion_no = 14;
                BtActStatus.action_no = 14;
                BtActStatus.charge_time = 0.0f;
                ruby_effect_id = NowMainEffect->Set(pos, aim, 3, element, 0, chara->frame->SearchFrame("item"), 0);
                NowMainEffect->SetLoop(1);
            }

#ifdef PAL_TIMING
            if (now >= 128.0f && now < 128.4f) {
#else
            if (now >= 128.0f && now < 128.3f) {
#endif
                BattleActionShotRuby(chara, aimed, 1.0f, 0);
                BtActStatus.frames_since_attack = 0;
                SwordDmgCheck1(0.8f, 0);
                SndSePlay(SE_CHARA_ACTION, -1, 0);
                SndSePlay(SE_CHARA_SHOUT, -1, 0);
            }
        }

        float end = (float) chara->motion_type.motion_info[11].end;

        if (now >= end - 1.0f && now <= end) {
            BtActStatus.unk_028 = 0;
            BtActStatus.motion_no = 0;
            BtActStatus.action_on = 0;

            if (lockOnTargetFlag != 0 && aimed == 0) {
                BtActStatus.motion_no = 0x12;
            }
        }
    }

    if (BtActStatus.action_no == 14) {
        if (GamePad.On(PadInput_OK) == 0) {
            BtActStatus.action_step = 1;
        }

        BtActStatus.motion_no = 14;
        SndSeSeqPlayStop(0x193, 5, 0);

        BtActStatus.charge_time += 1.0f;

        if (BtActStatus.charge_time >= 60.0f) {
            BtActStatus.charge_time = 60.0f;
        }

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
#ifdef PAL_TIMING
                if (now >= 216.0f && now < 216.5f) {
#else
                if (now >= 216.0f && now < 216.4f) {
#endif
                    BattleActionShotRuby(chara, aimed, 2.2f, 0);
                    BattleActionShotRuby(chara, aimed, 2.2f, 1);
                    BtActStatus.frames_since_attack = 0;
                    SwordDmgCheck1(1.8f, 0);
                    SndSePlay(SE_CHARA_ACTION, -1, 0);
                    SndSePlay(SE_CHARA_SHOUT_3, -1, 0);
                }
            } else {
#ifdef PAL_TIMING
                if (now >= 216.0f && now < 216.5f) {
#else
                if (now >= 216.0f && now < 216.3f) {
#endif
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

        float end = (float) chara->motion_type.motion_info[16].end;

        if (now >= end - 1.0f && now <= end) {
            BtActStatus.unk_028 = 0;
            BtActStatus.motion_no = 0;
            BtActStatus.action_on = 0;

            if (lockOnTargetFlag != 0 && aimed == 0) {
                BtActStatus.motion_no = 0x12;
            }
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
    if (BtActStatus.action_on == 0 && (s32) BtActStatus.action_gauge == 100) {
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
    float         drain;

    WEAPON_HAVE *weapon = NowWeaponHave;
    int          damage = (weapon->attack >> 1) + (weapon->magic >> 1);

    // A poisoned weapon hits twice as hard.
    if (StatusErrCheck(AILMENT_STAMINA) != 0) {
        damage *= 2;
    }

    drain = 1.5f - 0.01f * (float) NowWeaponHave->speed;

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
        BtActStatus.unk_028 = 0;
        BtActStatus.motion_no = 0;
        BtActStatus.action_on = 0;
        BtActStatus.gauge_exhausted = 1;

        if (lockOnTargetFlag != 0) {
            BtActStatus.motion_no = 0x12;
        }
    }

    BtActStatus.motion_no = 11;

    if (aimed == 0) {
        getFramePos((CFrameVu1 *) NowWeapon->frame, "dcol0", pos);
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
        BtActStatus.unk_028 = 0;
        BtActStatus.motion_no = 0;
        BtActStatus.action_on = 0;

        if (lockOnTargetFlag != 0) {
            BtActStatus.motion_no = 0x12;
        }

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
            getFramePos((CFrameVu1 *) NowWeapon->frame, "dcol0", pos);
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

        int shot_no;

        if (lockOnTargetFlag != 0) {
            shot_no = NowMainEffect->Set(pos, BtActStatus.target_position, 5, element, 0, NULL, -1);
        } else {
            shot_no = NowMainEffect->Set(pos, aim, 5, element, 0, NULL, -1);
        }

        if (shot_no != -1) {
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
        BtActStatus.unk_028 = 0;
        BtActStatus.motion_no = 0;
        BtActStatus.action_on = 0;

        if (lockOnTargetFlag != 0) {
            BtActStatus.motion_no = 0x12;
        }
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
                getFramePos((CFrameVu1 *) NowWeapon->frame, "dcol0", start);
                getCharacterVector(start_dir, 0.0f);
            } else {
                setShotVector(start_dir, 1.0f, viewAngleH__2, viewAngleV__2);
                sceVu0CopyVector(start, CharaMain.pos);
                start[1] += 10.0f;
            }

            OzumondFire.Init(start, start_dir, 0, 0);
        }

        if (GamePad.On(PadInput_OK) == 0) {
            BtActStatus.unk_028 = 0;
            BtActStatus.motion_no = 0;
            BtActStatus.action_on = 0;

            if (lockOnTargetFlag != 0) {
                BtActStatus.motion_no = 0x12;
            }

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
        BtActStatus.unk_028 = 0;
        BtActStatus.motion_no = 0;
        BtActStatus.action_on = 0;
        BtActStatus.gauge_exhausted = 1;

        if (lockOnTargetFlag != 0) {
            BtActStatus.motion_no = 0x12;
        }
    }

    if (aimed == 0) {
        getFramePos((CFrameVu1 *) NowWeapon->frame, "dcol0", pos);
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
        BtActStatus.unk_028 = 0;
        BtActStatus.motion_no = 0;
        BtActStatus.action_on = 0;

        if (lockOnTargetFlag != 0) {
            BtActStatus.motion_no = 0x12;
        }

        OzumondFire.Rset();
        wait_cnt = 10;
        dmg_check_wait = 0;
    }
}

float camera_far_dist_limmit;

/* The overlay's own copy of the camera walk is static, so only `autoCamTrial`
 * names it. It compiles to retail's code exactly, but while that caller is
 * still supplied by its marker the marker's `jal` needs the disambiguated
 * global the reference dump carries, so both flip together. */
static void CameraAutoMove(CCameraFollow *camera, CCPoly *poly, float *position, float from, float to) {
    float reference[4];
    float offset[4];
    float step;

    camera->GetRef(reference);
    offset[0] = position[0] - reference[0];
    offset[2] = position[2] - reference[2];
    offset[1] = 0.0f;
    camera->SetDistance(0.1f + DistVector(offset));

    step = 0.0f;
    float over = camera->GetDistance() - camera_near_dist__2;

    if (over < step) {
        step = -over / 20.0f;
    }

    if (over > 0.0f) {
        step = over / 30.0f;
    }

    if (step > 2.0f) {
        step = 2.0f;
    }

    if (step < 0.05f) {
        step = 0.05f;
    }

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

void autoCamTrial() {
    static int    cameraDilay = 0;
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
    CCPoly       *poly;
    int           i;
    int           count;

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
    WorkBuffer__2->used = 0;

    poly = (CCPoly *) WorkBuffer__2->Alloc(0x280);

    count = 0;

    box.max[0] = look[0] > eye[0] ? look[0] : eye[0];
    box.max[1] = look[1] > eye[1] ? look[1] : eye[1];
    box.max[2] = look[2] > eye[2] ? look[2] : eye[2];
    box.min[0] = look[0] < eye[0] ? look[0] : eye[0];
    box.min[1] = look[1] < eye[1] ? look[1] : eye[1];
    box.min[2] = look[2] < eye[2] ? look[2] : eye[2];

    if (NowDngMap->map_type != 1) {
        CFrame *frame;
        int     i;

        i = 0;

        while (NowDngMap->parts[i].frame[0] != NULL) {
            if (i == -1) {
                frame = NULL;
            } else {
                frame = NowDngMap->parts[i].camera_collision;
            }

            if (frame != NULL) {
                CDungeonParts *part = &NowDngMap->parts[i];

                sceVu0CopyVector(eye, part->frame_offset[0]);

                CDungeonMap *map = NowDngMap;
                int          turn = (int) ((CDungeonMap *) map)->parts[i].frame_turn[0];

                turn += i == -1 ? 0 : map->parts[i].camera_collision_turn;

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

            i++;
        }
    } else {
        CFrame *frame;
        int     i;
        int     j;

        for (j = 0; j < 20; j++) {
            for (i = 0; i < 20; i++) {
                CDungeonMap *map = NowDngMap;
                s32          parts_no = map->cells[i + j * 20].parts_no;

                if (parts_no == -1) {
                    frame = NULL;
                } else {
                    frame = map->parts[parts_no].camera_collision;
                }

                MAP_CELL *cell = &map->cells[i + j * 20];

                if (frame == NULL) {
                    continue;
                }

                if (!(cell->camera_dist <= 240.0f)) {
                    continue;
                }

                int turn = map->cells[i + j * 20].direction;

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

    static int cnt = 0;

    cnt++;

    if (cnt > 60) {
        cnt = 0;
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
        int last;

        sceVu0InnerProduct(forward, poly[hit_poly[0]].normal);
        last = -1;

        for (i = 0; i < hits; i++) {
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
                        CameraAutoMove(NowCamera__3, &poly[hit_poly[last + 1]], hit_point[last + 1], 1000000.0f, 1000000.0f);
                    } else {
                        CameraAutoMove(NowCamera__3, &poly[hit_poly[last]], hit_point[last], 1000000.0f, 1000000.0f);
                    }
                } else if (camera_far_dist_limmit >= camera_near_dist__2 && camera_far_dist_limmit < camera_far_dist__2) {
                    NowCamera__3->SetPos(hit_point[0]);
                } else {
                    CameraAutoMove(NowCamera__3, &poly[hit_poly[last]], hit_point[last], 1000000.0f, 1000000.0f);
                }
            }
        } else {
            sceVu0CopyVector(facing_last, poly[hit_poly[0]].normal);

            if (sceVu0InnerProduct(forward, facing_last) < 0.0f) {
                CameraAutoMove(NowCamera__3, &poly[hit_poly[0]], hit_point[0], 1000000.0f, 1000000.0f);
            }
        }
    }

    // The camera keeps its head above whatever floor is under it.
    sceVu0CopyVector(reach, pos);
    reach[1] += 20.0f;

    if (CheckHitVertical(poly, count, reach, -100.0f, at, 0) >= 0) {
        if (pos[1] - at[1] < 25.0f) {
            pos[1] = 25.0f + at[1];
            NowCamera__3->SetHeight(pos[1] - ref[1] - 0.01f);
        }
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

            if (step < 0.15f) {
                step = 0.15f;
            }

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
            if (turn < 0.4f) {
                turn = 0.4f;
            }

            if (!(turn <= 1.0f)) {
                turn = 1.0f;
            }

            NowCamera__3->AddAngle(2.5f * (-DEG_TO_RAD * turn));
        }

        if (inputH1 < -0.1f) {
            if (!(turn <= -0.4f)) {
                turn = -0.4f;
            }

            if (turn < -1.0f) {
                turn = -1.0f;
            }

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

            float want = (float) (atan2f(world[2][0], world[2][2]) - PI_D);

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

            if (range < 0.0f) {
                range = 0.0f;
            }

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
    // The two arrays are adjacent, and retail walks the second one off the
    // first rather than off the save data again.
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

#define CHARA_WEAPONS_OFFSET 0x450c
STATIC_ASSERT((int) &((CUserStatus *) 0)->chara_weapons == CHARA_WEAPONS_OFFSET);

#pragma opt_propagation off

int Run_TrapCircle(MAP_TRAP_CIRCLE *trap) {
    if (trap == NULL) {
        return;
    }

    int          element;
    int          se;
    int          kind;
    WEAPON_HAVE *weapon;
    CUserStatus *status;
    int          had;
    u16         *wallet;
    s8           chara;
    s8          *slots;
    s8           slot;

    status = UserStatus;
    wallet = &status->money;
    had = status->money;
    int had_copy = *wallet;
    chara = status->cur_chara;
    slots = status->equipped_weapon_slot;
    slot = slots[chara];
    int slot_bytes = slot * (int) sizeof(WEAPON_HAVE);
    int row_bytes = chara * (int) sizeof(status->chara_weapons[0]);
    weapon = (WEAPON_HAVE *) ((char *) status + row_bytes + slot_bytes + CHARA_WEAPONS_OFFSET);
    element = -1;
    se = 0;
    kind = trap->kind;

    s16 item_no = status->chara_weapons[status->cur_chara][slot].item_no;

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
            had_copy = (int) (had_copy - 0.2f * had_copy);

            if (had_copy <= 0) {
                had_copy = 0;
            }

            *wallet = had_copy;
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
    }

    if (element != -1) {
        WeaponDataChangeByRGate(weapon, element);
        SetWeaponAttachStatus(NowWeaponHave);
    }

    SndSePlay(se, -1, 0);
    return kind;
}

#pragma opt_propagation reset

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
    s32      x;
    s32      width;

    if (EnemyLifeGage.on == 0) {
        return;
    }

    if (EnemyLifeGage.draw == 0) {
        return;
    }

    if (((s32 *) SaveData->GetConfigData())[11] != 0) {
        return;
    }

    x = EnemyLifeGage.x - 32;
    EnemyLifeGage.y = (EnemyLifeGage.y >> 1) << 1;
    width = (s32) (64.0f / EnemyLifeGage.life_max * EnemyLifeGage.life);

    if (x >= 577 || x < 0) {
        return;
    }

    if (EnemyLifeGage.y >= 481 || EnemyLifeGage.y < 0) {
        return;
    }

    s32 top = EnemyLifeGage.y - 1;

    border.x = x - 1;
    border.y = top;
    border.width = 66;
    border.height = 6;
    set2DSpriteC4(Vif1Packet, border, &ELifeB1, &ELifeB2, &ELifeB1, &ELifeB2);

    s32 ground_top = EnemyLifeGage.y;

    ground.x = x;
    ground.y = ground_top;
    ground.width = 64;
    ground.height = 4;
    set2DSpriteC4(Vif1Packet, ground, &ELifeN1, &ELifeN1, &ELifeN1, &ELifeN1);

    s32 life_top = EnemyLifeGage.y;

    life.x = x;
    life.y = life_top;
    life.width = width;
    life.height = 4;
    set2DSpriteC4(Vif1Packet, life, &ELife1, &ELife2, &ELife1, &ELife2);
}

/* How long the lock-on cursor stays on screen, in frames. */
float targetCursorCnt;

void DrawtargetCursor(float *world, float width, float height, float alpha) {
    int       top_left[4];
    int       bottom_right[4];
    CTexture *texture;

    texture = TexManager.GetTexture("d01e04", -1);
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

        static int rate = (int) (16.0f * alpha);

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
    float         away;
    float         rate;
    int           place;
    int           i;
    int           count;
    int           slot;
    int           nearest;
    int           other;

    rate = CharaRangeRate(status);

    CharaFrame->GetRotation(at);

    // Declared here rather than with the rest: mwcc emits a local array's
    // template copy where the declaration stands, and retail's lands after
    // the two calls above, not before them. The slots still come out where
    // they do above, because everything ahead of these is still ahead of them.
    sceVu0FVECTOR forward = {0.0f, 0.0f, 1.0f, 1.0f};
    sceVu0FMATRIX unit;
    sceVu0FMATRIX turn;
    int           order[16];
    int           screen[4];

    sceVu0UnitMatrix(unit);
    sceVu0RotMatrixY(turn, unit, at[1]);
    sceVu0ApplyMatrix(forward, turn, forward);
    sceVu0Normalize(forward, forward);

    count = 0;
    sceVu0CopyVector(at, CharaFrame->position);

    for (i = 0; i < 16; i++) {
        found[i] = -1;
    }

    for (i = 0; i < 16; i++) {
        int on;

        MonstorPicupInner[i] = 0.0f;

        CMonstorUnit *monsters = NowMonstorUnit;

        if (monsters->monster[i].state != 2) {
            continue;
        }

        if (monsters->monster[i].hp <= 0) {
            continue;
        }

        if (i >= 0 && i < 17) {
            on = monsters->monster[i].revealed;
        }

        if (on == 0) {
            continue;
        }

        if (monsters->monster[i].lockon_enabled == 0) {
            continue;
        }

        monsters->chara[i][0].GetPosition(pos[i]);

        away = DistVector(at, pos[i]);

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

    // Split from the loop below: mwcc emits the initialisation where the
    // statement stands, which is ahead of the loop that clears order[].
    place = 0;

    for (slot = 0; slot < 16; slot++) {
        order[slot] = -1;
    }

    // The nearest one that is still free goes into each place in turn.
    for (; place < count; place++) {
        nearest = 0;

        while (found[nearest] == -1) {
            nearest++;
        }

        float best = dist[nearest];

        for (other = 0; other < count; other++) {
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

    int target;

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
    float         width;
    float         height;
    int           keep;

    float rate = CharaRangeRate(status);

    if (on != 0) {
        keep = 1;

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

        if (NowMonstorUnit->monster[lockOnTargetNo].lockon_frame != 0) {
            sceVu0CopyVector(target, NowMonstorUnit->monster[lockOnTargetNo].lockon_position);
        }

        sceVu0CopyVector(BtActStatus.target_position, target);

        width = (12.0f + targetCursorCnt) * NowMonstorUnit->monster[lockOnTargetNo].lockon_scale_x;
        height = (6.0f + targetCursorCnt / 2.0f) * NowMonstorUnit->monster[lockOnTargetNo].lockon_scale_y;

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

        if ((weapon == ITEM_WEAPON_STEVE || weapon == ITEM_WEAPON_SUPER_STEVE) && lockOnTargetFlag != 0 && NowMonstorUnit->monster[lockOnTargetNo].name_no > 0 && (int) (100.0f * (float) rand() / 2147483648.0f) < 3) {
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

/* How fast the ambient-colour animation runs. */
float unitAmbientAnime_Speed;

/* How far through the ambient-colour animation the player's model is. */
float unitAmbientAnime_Count;

/* How long the ambient-colour animation runs for, in frames. */
s32 unitAmbientAnime_Timer;

/* Whether the ambient-colour animation is running on the player's model. */
s32 unitAmbientAnime_flag;

void RsetUnitAmbientAnime() {
    unitAmbientAnime_flag = 0;
}

void setUnitAmbientAnime(float speed, float time, float red, float green, float blue) {
    unitAmbientAnime_rgb[0] = red;
    unitAmbientAnime_rgb[1] = green;
    unitAmbientAnime_rgb[2] = blue;
    unitAmbientAnime_Count = 0;
    unitAmbientAnime_Speed = speed;
    unitAmbientAnime_Timer = (s32) time;
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
    char *files[5];
    int   size;
    int   blocks;

    // Only the one texture is loaded; the NULL after it ends the list.
    files[0] = NameExchg(dungeon_name[map], 0);
    files[1] = NULL;
    BtStartLogoBuffer.used = 0;
    size = LoadTempTexture(files, 8, (char *) BtStartLogoBuffer.base);
    blocks = (((size >> 6) + 1) << 6) >> 4;
    BtStartLogoBuffer.Alloc(blocks);

    char *name = floor_texture[map];

    TEX_Floor1 = TexManager.GetTexture(name, -1);
    strcpy(floor_name, name);
    return blocks;
}

// clang-format off
/* The map info file each dungeon loads its floors from. */
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
    char       name[96];
    int        i;
    int        chosen = 0;
    static int nowCursor = 0;

    CDbgMsg.length = sprintf(&CDbgMsg.text[CDbgMsg.length], "- MapInfomationFile Loader -\n");

    for (i = 0; i < 7; i++) {
        strcpy(name, MapInfoNameArea[i]);

        if (nowCursor == i) {
            CDbgMsg.length += sprintf(&CDbgMsg.text[CDbgMsg.length], ">>[%2d] %s\n", i + 1, name);
        } else {
            CDbgMsg.length += sprintf(&CDbgMsg.text[CDbgMsg.length], "  [%2d] %s\n", i + 1, name);
        }
    }

    if (GamePad.Down(PAD_UP) != 0 && nowCursor != 0) {
        nowCursor--;
    }

    if (GamePad.Down(PAD_DOWN) != 0 && nowCursor != 6) {
        nowCursor++;
    }

    if (GamePad.Down(PAD_START) != 0 || GamePad.Down(PAD_CIRCLE | PAD_CROSS) != 0) {
        selectMapNo = nowCursor;
        main_select_menu_no = nowCursor;
        MapJump(selectMapNo + 200, -1);
        MGSetBGColor(0.0f, 0.0f, 0.0f, 128.0f);
        chosen = 1;
    }

    sceVif1PkCall(Vif1Packet, (u_long128 *) Vu_prog0f, 0);
    sceVif1PkTerminate(Vif1Packet);
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
    float         stick_x;
    float         stick_y;

    stick_x = GamePad.GetLXf();
    stick_y = -GamePad.GetLYf();

    // The heading wraps at half a turn either way; the pitch stops short of
    // straight up and straight down.
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

    if (stick_y > 0.0f) {
        if (viewAngleV__2 < 0.65f) {
            float rate = 0.02f;

            viewAngleV__2 += stick_y * rate;
        }
    }

    if (stick_y < -0.0f) {
        if (viewAngleV__2 > -1.0f) {
            float rate = 0.02f;

            viewAngleV__2 += stick_y * rate;
        }
    }

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
