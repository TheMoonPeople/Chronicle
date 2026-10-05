#include "dun/gameloop.hpp"

#include <libvu0.h>

#include <cmath>
#include <cstdio>
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
#include "collisiondata.hpp"
#include "debugfont.hpp"
#include "dispctrl.hpp"
#include "dngmessageman.hpp"
#include "dngstatusdata.hpp"
#include "dranmapfield.hpp"
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
#include "npcharacter.hpp"
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

#include "platform/config.hpp"

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

// Draws the floor's monsters. CMonstorUnit::DrawMonstor draws the ones taking part; a dormant one
// within video.detail_distance of the player is passed off as taking part for the length of the
// call, so that its model draws and animates without the monster waking.
static void DrawMonstorDetail() {
    CMonstorUnit *unit = NowMonstorUnit;
    float         detail = ConfigDetailDistance();
    bool          dormant[16];

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

namespace {

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

PC_OVERRIDE void DunMainDraw() {
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
        water_tex = *(sceGsTex0 *) &NamedTexture("water")->tex0;
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

        if (rogoSwitch2 == 1 && BtEventInfo.floor_title_off == 0) {
            TEX_Floor1 = TexManager.GetTexture(floor_name, -1);
            TexManager.ReloadTexture(Vif1Packet, 8);
            StartMessageDraw(TEX_Floor1, selectMapNo, UserStatus->cur_floor, BtUraDongeon, rogoAlphaA[2]);
        }

        TexManager.ReloadTexture(Vif1Packet, 2);

        gfx::SetUiAnchor(kStatusAnchor);

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
        TexManager.ReloadTexture(Vif1Packet, 0x17);
        MGMoveFrameBuffImage((sceGsTex0 *) &NamedTexture("frame_image")->tex0, 0, 0, 0);
    }
}

PC_OVERRIDE int LoaderLoop() {
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

    TexManager.ReloadTexture(Vif1Packet, 12);
    CDbgMsg.Draw();
    return chosen;
}
