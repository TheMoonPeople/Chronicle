#include "dun/gameloop.hpp"

#include <libvu0.h>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>

#include "battlemenu.hpp"
#include "btactstatus.hpp"
#include "btitem.hpp"
#include "btmisc.hpp"
#include "btsysscript.hpp"
#include "camera.hpp"
#include "camera_port.hpp"
#include "camerafollow.hpp"
#include "character.hpp"
#include "clothread.hpp"
#include "clsmes.hpp"
#include "collisiondata.hpp"
#include "dataread.hpp"
#include "debugfont.hpp"
#include "dispctrl.hpp"
#include "dngmessageman.hpp"
#include "dngstatusdata.hpp"
#include "dranmapfield.hpp"
#include "dun/gameloop_port.hpp"
#include "dungeoneventman.hpp"
#include "dungeonmap.hpp"
#include "edit.hpp"
#include "editloop3.hpp"
#include "effectmacro.hpp"
#include "frame.hpp"
#include "gamemode.hpp"
#include "gamepad.hpp"
#include "gameutil.hpp"
#include "gfx/gfx.hpp"
#include "healeffect.hpp"
#include "hit_machingun_effect.hpp"
#include "hitmark.hpp"
#include "hitvalue.hpp"
#include "itembombeffect.hpp"
#include "main.hpp"
#include "mainitemmodel.hpp"
#include "mainselect.hpp"
#include "mathutil.hpp"
#include "menu_draw.hpp"
#include "menu_dungeon.hpp"
#include "menu_misc.hpp"
#include "mglib.hpp"
#include "monstorunit.hpp"
#include "motionmodel.hpp"
#include "nowload.hpp"
#include "npcharacter.hpp"
#include "platform/config.hpp"
#include "platform/input.hpp"
#include "randomitem.hpp"
#include "rect.hpp"
#include "runeffect.hpp"
#include "shot_effect.hpp"
#include "shot_effect_pack.hpp"
#include "shot_firebar.hpp"
#include "shot_freefuncs.hpp"
#include "snd.hpp"
#include "stealitem.hpp"
#include "texture.hpp"
#include "textureanime.hpp"
#include "userstatus.hpp"
#include "weaponeffect.hpp"
#include "weaponelement.hpp"

// The dungeon's MainDraw and LoaderLoop, replaced to drop what the renderer has no use for: the VU1
// program call each opens with and MainDraw's wait for GIF path idle before the frame grab, which
// the renderer orders itself. Everything else is retail's, PAL's branch only.
//
// port/include/stubs/dun/gameloop.hpp renames the unit's MainDraw to DunMainDraw, apart from
// editloop's, as the PS2 link does.

// Defined by ps2/src/dun/gameloop.cpp without a header declaration.
struct BOMB_INFO {
    sceVu0FVECTOR pos;
    s32           unk_10;
    s32           aiming;
    s32           throw_step;
    s32           unk_1C;
};

extern BOMB_INFO       BombInfo;
extern CItemBombEffect CBomb__2[3];
extern CDebugFont      CDbgMsg;
extern CRunEffect      CRunFx__2;
extern CFrameVu1      *CharaFrame;
extern CCharacter      CharaHand;
extern ClsMes          DngMes1;
extern CHitValue       HitValue[32];
extern char           *MapInfoNameArea[7];
extern s32             MonstorNameOff;
extern CHitPointMark   MyHitPointMark[16];
extern CCharacter      NewChangeFx;
extern s32             NewChangeFxFlag;
extern CSHOT          *NowShotData;
extern CSHOT_FIREBAR   OzumondFire;
extern CTexture       *TEX_Floor1;
extern CTexture       *TEX_WepGage;
extern CHitMark        WeaponCrashEffect;
extern s32             autoDemo;
extern CFrame         *bicCursorFrame;
extern CFrame         *bombCursorFrame;
extern s32             exitMenuFlag;
extern char            floor_name[32];
extern s32             infoMap;
extern s32             itemNowSel;
extern s32             iventMarker;
extern sceVu0FVECTOR   iventPos;
extern s32             lightingMode;
extern s32             lockOnTargetDraw;
extern s32             oldMsgNo;
extern s32             rogoAlphaA[3];
extern s32             rogoSwitch2;
extern s32             rogoY3;
void                   unitBlowActionRot(float *velocity);

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
constexpr gfx::UiAnchor kStatusAnchor = {0x124, 0x1FC, 240.0f, 240.0f};
// The mini map hangs under the floor plate.
constexpr gfx::UiAnchor kMiniMapAnchor = gfx::UiAnchor::Side(1, -1);

float CharaHeight(CUserStatus *status) {
    float chara_height[6] = {16.0f, 14.0f, 16.0f, 16.0f, 18.0f, 15.0f};
    return chara_height[static_cast<int>(status->cur_chara)];
}

CTexture *NamedTexture(const char *name) {
    return TexManager.GetTexture(const_cast<char *>(name), -1);
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

PORT_OVERRIDE void DunMainDraw() {
    sceVu0FMATRIX camera;
    sceVu0FVECTOR eye;
    sceVu0FMATRIX view;
    sceVu0FMATRIX unit;
    sceGsTex0     frame_tex;
    sceGsTex0     water_tex;
    int           i;

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
        water_tex = *reinterpret_cast<sceGsTex0 *>(&NamedTexture("water")->tex0);
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

        for (i = 0; i < 32; i++) {
            HitValue[i].Draw();
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

    for (i = 0; i < 1; i++) {
        if (EdEventInfo.item_frame[i] != nullptr) {
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
        TexManager.ReloadTexture(Vif1Packet, 0x17);
        MGMoveFrameBuffImage(reinterpret_cast<sceGsTex0 *>(&NamedTexture("frame_image")->tex0), 0, 0, 0);
    }
}

extern LOADTEXTURE_INFO2 texdata__2[];

// Retail formats the floor's gate key image over the third name in texdata__2, a string literal the
// PS2 left writable and a host need not; the port formats it into a buffer of its own.
PORT_OVERRIDE void LoadBaseTexture() {
    std::size_t info_count = 0;
    while (texdata__2[info_count].name != nullptr) {
        info_count++;
    }
    auto        info = std::make_unique<LOADTEXTURE_INFO2[]>(info_count + 1);
    int         size;
    int         i;
    char       *name;
    std::string gate_key = "gatekey0" + std::to_string(selectMapNo + 1) + ".img";
    std::string path = "dun/pack/teximg2";
    if (LanguageCode > LANG_JAPANESE) {
        path += "_" + std::to_string(LanguageCode);
    }
    path += ".pac";

    LoadFile(path.data(), reinterpret_cast<void *>(read_buffer), nullptr);
    wait_now_loading_vsync();

    i = 0;

    while ((name = texdata__2[i].name) != nullptr) {
        if (i == 2) {
            name = gate_key.data();
        }

        // A name that starts with '#' asks the manager for a blank page of that size rather than
        // for a file inside the pack.
        if (name[0] == '#') {
            info[i].name = name;
        } else {
            u_int *found = GetPackFile(read_buffer, name, &size);

            if (found != nullptr) {
                info[i].name = reinterpret_cast<char *>(found);
            } else {
                printf("Error::Pack->FileNotFound [%d]%s!!\n", i, name);
                exit__2(-1);
            }
        }

        info[i].block_no = texdata__2[i].block_no;
        i++;
    }

    info[i].name = nullptr;
    TexManager.LoadTextureBlock(-1, info.get());

    // The message window's pages, and the three images the language's own pack supplies in place of
    // the names written here.
    LOADTEXTURE_INFO2 mes_info[8] = {
        {const_cast<char *>("#mes_frame_buff#640#480#4"), 0x1A, 0},
        {const_cast<char *>("#fukidashibase#640#224#4"),  0x1A, 0},
        {const_cast<char *>("#fontbase#512#256#1"),       0x1A, 0},
        {const_cast<char *>("meswin/gaiji.img"),          0x1A, 0},
        {const_cast<char *>("meswin/fuki256.img"),        0x1A, 0},
        {const_cast<char *>("meswin/syst04.img"),         0x1A, 0},
    };

    std::string mes_path = "meswin/mes_tex";
    if (LanguageCode > LANG_JAPANESE) {
        mes_path += "_" + std::to_string(LanguageCode);
    }
    mes_path += ".pak";

    LoadFile(mes_path.data(), read_buffer, nullptr);
    wait_now_loading_vsync();

    mes_info[3].name = reinterpret_cast<char *>(GetPackFile(read_buffer, const_cast<char *>("gaiji.img"), nullptr));
    mes_info[4].name = reinterpret_cast<char *>(GetPackFile(read_buffer, const_cast<char *>("fuki256.img"), nullptr));
    mes_info[5].name = reinterpret_cast<char *>(GetPackFile(read_buffer, const_cast<char *>("syst04.img"), nullptr));
    TexManager.LoadTextureBlock(-1, mes_info);
}

PORT_OVERRIDE int LoaderLoop() {
    int        i;
    int        chosen = 0;
    static int nowCursor = 0;

    int length = snprintf(CDbgMsg.text, sizeof(CDbgMsg.text), "- MapInfomationFile Loader -\n");

    if (length < 0 || length >= static_cast<int>(sizeof(CDbgMsg.text))) {
        CDbgMsg.length = 0;
        return 0;
    }

    for (i = 0; i < 7; i++) {
        const char *name = MapInfoNameArea[i] != nullptr ? MapInfoNameArea[i] : "";
        const int   remaining = static_cast<int>(sizeof(CDbgMsg.text)) - length;

        if (nowCursor == i) {
            int written = snprintf(&CDbgMsg.text[length], remaining, ">>[%2d] %s\n", i + 1, name);

            if (written < 0) {
                break;
            }

            length += written < remaining ? written : remaining - 1;
        } else {
            int written = snprintf(&CDbgMsg.text[length], remaining, "  [%2d] %s\n", i + 1, name);

            if (written < 0) {
                break;
            }

            length += written < remaining ? written : remaining - 1;
        }

        if (length >= static_cast<int>(sizeof(CDbgMsg.text)) - 1) {
            break;
        }
    }

    CDbgMsg.length = length;

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

PORT_OVERRIDE int BtCheckDamageProc() {
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

            roll = static_cast<int>(100.0f * static_cast<float>(rand()) / 2147483648.0f);

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
                int slot = (static_cast<CDngStatusData *>(UserStatus))->CheckActItemSlot(ITEM_ANTI_FREEZE_AMULET);

                if (slot == -1) {
                    BtSetStatusErr(AILMENT_FREEZE);
                } else {
                    // &who->active_item_vol[slot], spelled so the index is added first.
                    CUserStatus   *who = UserStatus;
                    std::uintptr_t address = slot * sizeof(s32);

                    address += reinterpret_cast<std::uintptr_t>(who);
                    s32 *vol = &reinterpret_cast<CUserStatus *>(address)->active_item_vol[0];

                    if (--(*vol) <= 0) {
                        DelActiveItem(slot + 1);
                        DngMessMan.message = 0xB7;
                        DngMessMan.insert_mes_1 = GetCommonItemDataSystemMsg(ITEM_ANTI_FREEZE_AMULET);
                        DngMessMan.timer = 0xB4;
                        DngMessMan.steev_window = false;
                    }
                }
            }

            if ((hit->flags & 0x200) && roll < 0x41 && (UserStatus->ailments[static_cast<int>(UserStatus->cur_chara)] & AILMENT_POISON) == 0) {
                int slot = (static_cast<CDngStatusData *>(UserStatus))->CheckActItemSlot(ITEM_ANTIDOTE_AMULET);

                if (slot == -1) {
                    BtSetStatusErr(AILMENT_POISON);
                    DngMessMan.message = 0xBB;
                    DngMessMan.timer = 0xB4;
                    DngMessMan.steev_window = false;
                } else {
                    CUserStatus   *who = UserStatus;
                    std::uintptr_t address = slot * sizeof(s32);

                    address += reinterpret_cast<std::uintptr_t>(who);
                    s32 *vol = &reinterpret_cast<CUserStatus *>(address)->active_item_vol[0];

                    if (--(*vol) <= 0) {
                        DelActiveItem(slot + 1);
                        DngMessMan.message = 0xB7;
                        DngMessMan.insert_mes_1 = GetCommonItemDataSystemMsg(ITEM_ANTIDOTE_AMULET);
                        DngMessMan.timer = 0xB4;
                        DngMessMan.steev_window = false;
                    }
                }
            }

            if ((hit->flags & 0x400) && roll < 0x41 && (UserStatus->ailments[static_cast<int>(UserStatus->cur_chara)] & AILMENT_CURSE) == 0) {
                int slot = (static_cast<CDngStatusData *>(UserStatus))->CheckActItemSlot(ITEM_ANTICURSEAMULET);

                if (slot == -1) {
                    BtSetStatusErr(AILMENT_CURSE);
                    DngMessMan.message = 0xB9;
                    DngMessMan.timer = 0xB4;
                    DngMessMan.steev_window = false;
                } else {
                    CUserStatus   *who = UserStatus;
                    std::uintptr_t address = slot * sizeof(s32);

                    address += reinterpret_cast<std::uintptr_t>(who);
                    s32 *vol = &reinterpret_cast<CUserStatus *>(address)->active_item_vol[0];

                    if (--(*vol) <= 0) {
                        DelActiveItem(slot + 1);
                        DngMessMan.message = 0xB7;
                        DngMessMan.insert_mes_1 = GetCommonItemDataSystemMsg(ITEM_ANTICURSEAMULET);
                        DngMessMan.timer = 0xB4;
                        DngMessMan.steev_window = false;
                    }
                }
            }

            if ((hit->flags & 0x800) && roll < 0x41 && (UserStatus->ailments[static_cast<int>(UserStatus->cur_chara)] & AILMENT_GOO) == 0) {
                int slot = (static_cast<CDngStatusData *>(UserStatus))->CheckActItemSlot(ITEM_ANTIGOO_AMULET);

                if (slot == -1) {
                    BtSetStatusErr(AILMENT_GOO);
                    DngMessMan.message = 0xBA;
                    DngMessMan.timer = 0xB4;
                    DngMessMan.steev_window = false;
                } else {
                    CUserStatus   *who = UserStatus;
                    std::uintptr_t address = slot * sizeof(s32);

                    address += reinterpret_cast<std::uintptr_t>(who);
                    s32 *vol = &reinterpret_cast<CUserStatus *>(address)->active_item_vol[0];

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
                damage = UserStatus->max_hp[static_cast<int>(UserStatus->cur_chara)] >> 1;
            }

            if (NowColData->hit[hit_no].kind == 3) {
                sceVu0CopyVector(blow, NowColData->hit[hit_no].pos);
                GamePad.SetVibration(1, 0xE6, 0x16);

                if (BtActStatus.action_no == 9) {
                    HitBlowVelocity(hit_no, blowVelo);
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

                    HitBlowVelocity(hit_no, blowVelo);
                    unitBlowActionRot(blowVelo);

                    if (NowColData->hit[hit_no].target_mask != 3) {
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
                        CharaMain.motion_type.state.time = static_cast<float>(CharaMain.motion_type.motion_info[4].start);
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

        status->hp[static_cast<int>(status->cur_chara)] = status->max_hp[static_cast<int>(status->cur_chara)];
    }

    return taken;
}

PORT_OVERRIDE void EyeCamera() {
    sceVu0FVECTOR pos;
    sceVu0FVECTOR ref;
    sceVu0FMATRIX rotation;
    sceVu0FMATRIX unit;
    float         stick_x;
    float         stick_y;

    auto keyboard = InputGetKeyboardMovement();
    bool walking = keyboard.x || keyboard.y;
    stick_x = walking ? 0.0f : GamePad.GetLXf();
    stick_y = walking ? 0.0f : -GamePad.GetLYf();

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
