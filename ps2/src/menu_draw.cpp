#pragma helper_mask_gpr 0x30
#pragma helper_mask_fpr 0x1000
#pragma name_counter 553

#include "menu_draw.hpp"

#include <libvu0.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "battlemenu.hpp"
#include "camera.hpp"
#include "clsmes.hpp"
#include "dataalloc.hpp"
#include "dataread.hpp"
#include "dun/gameloop.hpp"
#include "editatra.hpp"
#include "gamepad.hpp"
#include "itemdata.hpp"
#include "mainitemmodel.hpp"
#include "mainselect.hpp"
#include "memcard.hpp"
#include "memorycardaccess.hpp"
#include "menu_dungeon.hpp"
#include "menu_inventory.hpp"
#include "menu_misc.hpp"
#include "menuitemstep.hpp"
#include "mglib.hpp"
#include "rect.hpp"
#include "savedata.hpp"
#include "shop.hpp"
#include "snd.hpp"
#include "texture.hpp"
#include "userstatus.hpp"

/** Player data used by personal-board item operations. */
CUserStatus *PerBoardStatusPt;

CTexture *PerBoardTex;

/** The personal inventory board the menu is working on. */
PERSONAL_BOARD *PerBoardPt;

char MenuGrobalDir[64];

/** Marks, one per item pack slot, of the items the menu offers to throw away. */
s8 MenuTrushMark[100];

#include "editloop.hpp"
#include "gameutil.hpp"
#include "menuetc.hpp"
#include "sysmes.hpp"

/** Directory that menu textures load from. */
static char AllMenuTextureDir[9] = "commenu/";

/** Path fragment of each selectable language's directory and file names. */
static char *ComMenuContryName[7] = {"a_jpn/", "a_usa/", "a_eng/", "a_fre/", "a_ger/", "a_ita/", "a_spa/"};

char *GetMenuTextureDir() {
    return AllMenuTextureDir;
}

int GetMenuLangFlag() {
    return LanguageCode;
}

char *GetNowSelectLanguage(int language) {
    if ((language < 0) || (language >= 7)) {
        language = 0;
    }

    return ComMenuContryName[language];
}

void GetPathReadDifferntLang(char *path) {
    strcpy(path, GetMenuTextureDir());
    strcat(path, GetNowSelectLanguage(GetMenuLangFlag()));
}

int LoadFileBGMenuData(char *name, u_long128 *buffer) {
    int size;

    if ((name == NULL) || (buffer == NULL)) {
        return -1;
    }

    GetPathReadDifferntLang(MenuGrobalDir);
    strcat(MenuGrobalDir, name);
    LoadFileBG(MenuGrobalDir, buffer, &size);
    return size;
}

int LoadFileMenuData(char *name, unsigned int *buffer) {
    int size;

    GetPathReadDifferntLang(MenuGrobalDir);
    strcat(MenuGrobalDir, name);
    LoadFile(MenuGrobalDir, buffer, &size);
    return size;
}

u_long128 *BtlMenuBufferSet(int mode) {
    u_long128 *buffer;

    switch (mode) {
        case BATTLE_MENU_MODE_DUNGEON:
            buffer = (u_long128 *) read_buffer;
            break;
        case BATTLE_MENU_MODE_TOWN:
            buffer = (u_long128 *) (EdMenuBuffer.base + EdMenuBuffer.used * 0x10);
    }

    return buffer;
}

u_long128 *MenuCalcBufAlignment(u_long128 *buffer) {
    int offset = (int) buffer;
    int remainder = offset & 0x3F;

    if ((offset < 0) && (remainder != 0)) {
        remainder -= 0x40;
    }

    if (remainder != 0) {
        offset = ((offset >> 6) + 1) << 6;
    }

    return (u_long128 *) offset;
}

int GetAtoraMaxVillage() {
    int max_village = 3;
    int village;

    for (village = 4; village >= 0; village--) {
        if (SaveData->VisitMap(village, 0) || SaveData->QuestDungeon(village, 0)) {
            max_village = village + 3;
            break;
        }
    }

    if (SaveData->QuestDungeon(5, 0)) {
        max_village = 8;
    }

    if ((max_village < 3) || (max_village > 8)) {
        max_village = 8;
    }

    return max_village;
}

int GetNowMapTransAtraMap(int map_no) {
    int village = 0;
    // clang-format off
    s16 map_to_village[35] = {
        1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 4, 3, 4, 5, 5,
        5, 3, 1, 4, 4, 4, 4, 1, 3, 2, 3, 4, 5, 2, 5,
        4, 3, 2, 2, 2,
    };
    // clang-format on

    if (map_no < 5) {
        village = map_no;
    } else if (map_no >= 0xB && map_no < 0x29) {
        village = map_to_village[map_no - 0xB];
    } else if (map_no >= 0xC8) {
        village = map_no - 0xC8;
    }

    if (GetAtoraMaxVillage() - 3 < village) {
        village = GetAtoraMaxVillage() - 3;
    }

    return village;
}

void MenuWorldTrans(CCamera *camera) {
    sceVu0FMATRIX camera_matrix;
    sceVu0FVECTOR eye_pos;
    sceVu0FMATRIX view_matrix;
    sceVu0FMATRIX unit_matrix;

    MGSetProjection(800.0f);
    camera->GetCameraMatrix(camera_matrix);
    camera->Step(1);
    camera->GetPos(eye_pos);
    sceVu0UnitMatrix(unit_matrix);
    sceVu0MulMatrix(view_matrix, unit_matrix, camera_matrix);
    MGSetViewMatrix(view_matrix, eye_pos);
}

void MenuPolygonDraw(int ambient_alpha, void (*draw)()) {
    float ref_pos[4] = {0.0f, 0.0f, -45.0f, 1.0f};
    float eye_pos[4] = {0.0f, 0.0f, 60.0f, 1.0f};

    MenuCamera.SetRef(ref_pos);
    MenuCamera.SetPos(eye_pos);

    float light_dir[4][4] = {
        {0.3f, 0.0f, 0.0f, 0.0f},
        {0.3f, 0.0f, 0.0f, 0.0f},
        {0.3f, 0.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 0.0f, 0.0f},
    };
    float light_colour[4][4] = {
        {96.0f, 96.0f, 96.0f, 0.0f},
        {0.0f,  0.0f,  0.0f,  0.0f},
        {0.0f,  0.0f,  0.0f,  0.0f},
        {0.0f,  0.0f,  0.0f,  0.0f},
    };
    float saved_ambient[4];
    float ambient[4];
    float saved_light_dir[4][4];
    float saved_light_colour[4][4];

    MGGetPLight(saved_light_dir, saved_light_colour);
    MGGetAmbient(saved_ambient);
    ambient[0] = ambient[1] = ambient[2] = 80.0f;
    ambient[3] = (float) ambient_alpha;

    float light_vector[4] = {0.3f, 1.0f, 0.3f, 0.0f};
    float light_normal[4];

    sceVu0Normalize(light_normal, light_vector);
    light_dir[0][0] = light_normal[0];
    light_dir[1][0] = light_normal[1];
    light_dir[2][0] = light_normal[2];
    MGSetPLight(light_dir, light_colour);
    MGSetAmbient(ambient);
    draw();
    MGSetPLight(saved_light_dir, saved_light_colour);
    MGSetAmbient(saved_ambient);
}

void Get3DPosTo2DPos(CFrame *frame, int *screen) {
    float world[4];
    float origin[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    int   screen_pos[4];

    frame->GetWorldPosition(world, origin);
    MGRotTransPers2D(screen_pos, world, 0);
    screen[0] = screen_pos[0];
    screen[1] = screen_pos[1];
}

int GetMenuCommonFontW(int language, int style) {
    s8 font_widths[7] = {16, 11, 11, 11, 11, 11, 11};
    return font_widths[language];
}

int GetMenuCommonPutXY(ClsMes *mes, int x) {
    int size[4];

    if (mes == NULL) {
        return 0;
    }

    int put_x = x;
    mes->NeedMesWinWH(mes->mes_made, size);
    put_x -= size[2] >> 1;
    mes->text_x = put_x;
    PORT_RETURN(put_x);
}

/**
 * Resets a menu message window to its empty default layout.
 */
static inline void ResetMenuMes(ClsMes *mes) {
    mes->text_columns = MES_WIN_COLUMNS;
    mes->text_rows = 10;
    mes->text_len = 0;
    mes->text_width = 0;
    mes->text_height = 0;
    mes->fade = 0.0f;
    mes->fade_in = true;
    mes->text_rate = mes->text_rate_set;
    mes->waiting = false;
    mes->text_at = 0.0f;
    mes->text_no = 0;
    mes->text_from = 0;
    mes->page_from = 0;
    mes->InitMesWinTbl();
    mes->clut_now = mes->clut_default;
    mes->wait = 0;
    mes->blink = 0;
    mes->auto_page_wait = 0;
    mes->mes_made = -1;
    mes->edge_alpha = 0x80;

    for (int i = 0; i < 10; i++) {
        mes->mes_no[i] = -1;
    }

    for (int i = 0; i < 8; i++) {
        mes->values[i] = 0;
    }

    mes->value = 0;
    mes->value_signed = false;
    mes->value_show = true;
    mes->value_narrow = false;
    mes->space_width = -1;
    mes->space_area = -1;
    mes->cursor_row = -1;
    mes->cursor_y = 0;
    mes->cursor_lit = false;

    for (int i = 0; i < 10; i++) {
        mes->line_pos[i].x = -1;
        mes->line_pos[i].y = -1;
    }
}

void InitMenuMesSet(int mode, short *messages) {
    CommonMenuMes1.tex_buff = MesWinTexBuff_01;
    CommonMenuMes2.tex_buff = MesWinTexBuff_02;
    CommonMenuMes3.tex_buff = MesWinTexBuff_11;
    AtoraNameMes.tex_buff = MesWinTexBuff_12;
    ResetMenuMes(&CommonMenuMes1);
    ResetMenuMes(&CommonMenuMes2);
    ResetMenuMes(&CommonMenuMes3);
    ResetMenuMes(&AtoraNameMes);
    CommonMenuMes1.tex_block = 0x1A;
    CommonMenuMes2.tex_block = 0x1A;
    CommonMenuMes3.tex_block = 0x1A;
    AtoraNameMes.tex_block = 0x1A;
    CommonMenuMes1.SetBuff(messages);
    CommonMenuMes2.SetBuff(messages);
    CommonMenuMes3.SetBuff(messages);
    AtoraNameMes.SetBuff(messages);
    CommonMenuMes1.SetBuff_system(SystemMes);
    CommonMenuMes2.SetBuff_system(SystemMes);
    CommonMenuMes3.SetBuff_system(SystemMes);
    AtoraNameMes.SetBuff_system(SystemMes);
    CommonMenuMes1.mes_made = -1;
    CommonMenuMes2.mes_made = -1;
    CommonMenuMes3.mes_made = -1;
    AtoraNameMes.mes_made = -1;

    switch (GetMenuLangFlag()) {
        case LANG_JAPANESE:
            CommonMenuMes1.columns = 15;
            CommonMenuMes1.rows = 3;
            CommonMenuMes1.char_width = 0x10;
            CommonMenuMes1.char_height = 0x16;
            CommonMenuMes2.columns = 15;
            CommonMenuMes2.rows = 3;
            CommonMenuMes2.char_width = 0x10;
            CommonMenuMes2.char_height = 0x16;
            CommonMenuMes3.columns = 0x1D;
            CommonMenuMes3.rows = 4;
            CommonMenuMes3.char_width = 0x10;
            CommonMenuMes3.char_height = 0x16;
            AtoraNameMes.char_width = 0x10;
            AtoraNameMes.char_height = 0x16;
            break;
        default:
        case LANG_ENGLISH_US:
            CommonMenuMes1.rows = 3;
            CommonMenuMes1.char_width = 11;
            CommonMenuMes1.char_height = 0x14;
            CommonMenuMes1.narrow_gaiji_set = 2;
            CommonMenuMes2.rows = 4;
            CommonMenuMes2.char_width = 11;
            CommonMenuMes2.char_height = 0x14;
            CommonMenuMes2.narrow_gaiji_set = 2;
            CommonMenuMes3.columns = 0x1D;
            CommonMenuMes3.rows = 4;
            CommonMenuMes3.char_width = 11;
            CommonMenuMes3.char_height = 0x14;
            CommonMenuMes3.narrow_gaiji_set = 2;
            AtoraNameMes.char_width = 11;
            AtoraNameMes.narrow_gaiji_set = 2;
            break;
    }

    CommonMenuMes1.stay_frame = false;
    CommonMenuMes2.stay_frame = false;
    CommonMenuMes3.stay_frame = false;
    AtoraNameMes.stay_frame = false;
    CommonMenuMes1.auto_pos = MES_POS_NONE;
    CommonMenuMes2.auto_pos = MES_POS_NONE;
    CommonMenuMes3.auto_pos = MES_POS_NONE;
    AtoraNameMes.auto_pos = MES_POS_NONE;
    CommonMenuMes1.style = MES_EDGE_NONE;
    CommonMenuMes2.style = MES_EDGE_NONE;
    CommonMenuMes3.style = MES_EDGE_NONE;
    AtoraNameMes.style = MES_EDGE_NONE;

    switch (mode) {
        case MENU_MES_SET_ALLMENU:
            CommonMenuMes1.Preset(MES_PRESET_SYSTEM);
            CommonMenuMes2.Preset(MES_PRESET_SYSTEM);
            CommonMenuMes3.Preset(MES_PRESET_NAME);
            AtoraNameMes.Preset(MES_PRESET_NAME);
            CommonMenuMes1.value_show = false;
            CommonMenuMes2.value_show = false;
            CommonMenuMes3.value_show = false;
            AtoraNameMes.value_show = true;
            return;
        case MENU_MES_SET_EVENT_ITEM:
            CommonMenuMes2.Preset(MES_PRESET_SYSTEM);
            CommonMenuMes2.centre_rows = true;
            CommonMenuMes2.columns = 0x19;
            CommonMenuMes2.rows = 4;
            CommonMenuMes1.Preset(MES_PRESET_SYSTEM);
            CommonMenuMes1.centre_rows = true;
            return;
        case MENU_MES_SET_SHOP:
            CommonMenuMes1.Preset(MES_PRESET_SYSTEM);
            CommonMenuMes2.Preset(MES_PRESET_SYSTEM);
            CommonMenuMes3.Preset(MES_PRESET_FUKIDASHI);
            AtoraNameMes.Preset(MES_PRESET_SYSTEM);
            CommonMenuMes1.columns = 15;
            CommonMenuMes1.rows = 3;
            CommonMenuMes2.stay_frame = false;
            CommonMenuMes2.value_signed = true;
            CommonMenuMes2.value_show = false;
            CommonMenuMes3.columns = 0x1E;
            CommonMenuMes3.rows = 3;
            CommonMenuMes3.page_arrow = true;
            CommonMenuMes3.fade_speed = 0.1f;
            AtoraNameMes.stay_frame = true;
            AtoraNameMes.rows = 1;

            switch (GetMenuLangFlag()) {
                case LANG_JAPANESE:
                    CommonMenuMes3.char_width = 0x10;
                    CommonMenuMes3.char_height = 0x16;
                    return;
                default:
                case LANG_ENGLISH_US:
                    CommonMenuMes3.narrow_gaiji_set = 0;
                    CommonMenuMes3.char_width = 12;
                    CommonMenuMes3.char_height = 0x14;
                    return;
            }
        case MENU_MES_SET_NAME_ENTRY:
            CommonMenuMes2.Preset(MES_PRESET_SYSTEM);
            CommonMenuMes3.Preset(MES_PRESET_SYSTEM);
            AtoraNameMes.Preset(MES_PRESET_SYSTEM);
            CommonMenuMes2.style = MES_EDGE_DOUBLE;
            CommonMenuMes3.style = MES_EDGE_DOUBLE;
            AtoraNameMes.style = MES_EDGE_DOUBLE;
            CommonMenuMes2.rows = 3;
            CommonMenuMes3.rows = 2;
            AtoraNameMes.rows = 1;
            return;
        case MENU_MES_SET_ATLA_NAME:
            AtoraNameMes.Preset(MES_PRESET_NAME);
            AtoraNameMes.rows = 1;
            return;
        case MENU_MES_SET_WORLD_MAP:
            CommonMenuMes2.Preset(MES_PRESET_SYSTEM);
            CommonMenuMes3.Preset(MES_PRESET_NAME);
            CommonMenuMes3.rows = 1;
            return;
        case MENU_MES_SET_DUNGEON_ENTER:
            CommonMenuMes2.Preset(MES_PRESET_SYSTEM);
            CommonMenuMes2.char_width = 12;
            CommonMenuMes2.rows = 2;
            CommonMenuMes2.columns = 15;
        default:
            return;
    }
}

void DrawMenuClsMes(ClsMes *message, int x, int y) {
    if (message != NULL) {
        message->text_x = x;
        message->text_y = y;
        message->Step();
        message->DrawMesWin();
    }
}

void ComMenuSePlay(int sound) {
    if (sound >= 0) {
        SndSePlay(sound, -1, 0);
    }
}

void DrawMenu2DSprite(CTexture *texture, CRect_i_ screen, CRect_i_ texel, int alpha) {
    set2DSprite(GetVif1Packet(), texture, screen, texel, alpha);
}

void DrawMenu2DSprite(CTexture *texture, CRect_i_ screen, CRect_i_ texel, unsigned char r, unsigned char g, unsigned char b, int alpha) {
    set2DSprite(GetVif1Packet(), texture, screen, texel, r, g, b, alpha);
}

void DrawMenu2DSprite(CTexture *texture, CRect_i_ screen, CRect_i_ texel, spRGBA *top_left, spRGBA *top_right, spRGBA *bottom_left, spRGBA *bottom_right) {
    set2DSprite(GetVif1Packet(), texture, screen, texel, top_left, top_right, bottom_left, bottom_right, 1);
}

void MenuTextureReload(int block) {
    TexManager.ReloadTexture(GetVif1Packet(), block);
}

void MenuTextureDelete(int *blocks) {
    for (int i = 0; blocks[i] != -1; i++) {
        TexManager.DeleteTextureBlock(blocks[i]);
        printf("delete block: %d\n", blocks[i]);
    }
}

void AllFillBoxForMenu(unsigned char r, unsigned char g, unsigned char b, unsigned char alpha) {
    if (r < 0 || r > 0xFF) {
        r = 0x80;
    }

    if (g < 0 || g > 0xFF) {
        g = 0x80;
    }

    if (b < 0 || b > 0xFF) {
        b = 0x80;
    }

    if (alpha < 0 || alpha > 0xFF) {
        alpha = 0x80;
    }

    MGFillBox(CRect_i_(0, 0, 0x2800, (SCREEN_HEIGHT << 4)), r, g, b, alpha);
}

void AllFadeForMenu(int alpha) {
    AllFillBoxForMenu(0, 0, 0, (unsigned char) alpha);
}

void FrameImageDraw(int brightness, int alpha) {
    sceGsTexa texa;
    CTexture *texture = TexManager.GetTexture("frame_image", -1);

    if (texture != NULL) {
        TexManager.ReloadTexture(GetVif1Packet(), texture->block);
        ((sceGsTex0 *) &texture->tex0)->bits.tcc = 0;
        texa = mgTexa;
        texa.AEM = 1;
        texa.TA0 = 0x80;
        MGSetGsTEXA(&texa);
#ifdef PAL
        DrawMenu2DSprite(texture, MenuDispRc, MenuDispRc, brightness, brightness, brightness, alpha);
#else
        set2DSprite(GetVif1Packet(), texture, MenuDispRc, MenuDispRc, brightness, brightness, brightness, alpha);
#endif
        MGSetGsTEXA(NULL);
    }
}

void DrawMenuColorGradation(CRect_i_ &rect, spRGBA *top_left, spRGBA *top_right, spRGBA *bottom_left, spRGBA *bottom_right) {
    set2DSpriteC4(GetVif1Packet(), rect, top_left, top_right, bottom_left, bottom_right);
}

void DrawMenuSideGradation(CRect_i_ &rect, spRGBA *left, spRGBA *right) {
    set2DSpriteC4(GetVif1Packet(), rect, left, right, left, right);
}

/**
 * Draws the mark over an item that cannot be set.
 *
 * @mangled DrawDontSetItemMark__Fiiiii
 * @address 0x22D4C0
 * @size 0xDC
 */
static void DrawDontSetItemMark(int x, int y, int top, int bottom, int alpha) {
    int position;
    int length;
    int source;

    if (y < top - 31 || y > bottom - 1) {
        return;
    }

    position = y;
    length = 32;
    source = 0xC0;
    MenuTextureClip(position, source, length, top, bottom);
    DrawMenu2DSprite(PerBoardTex, CRect_i_(x, position, 32, length), CRect_i_(0x13C, source, 32, length), alpha);
}

void DrawIconParts(int item_no, int x, int y, int top, int bottom, int alpha, int number) {
    int            position;
    int            u;
    int            v;
    int            length;
    COM_ITEM_INFO *info;
    CTexture      *texture;

    if (y < top - 31 || y > bottom - 1) {
        return;
    }

    position = y;
    info = GetCommonItemInfo(item_no);

    if (info != NULL && info->icon_index >= 0) {
        texture = RetCTex(item_no, u, v);

        if (texture != NULL) {
            length = 32;
            MenuTextureClip(position, v, length, top, bottom);
            CRect_i_ source(u, v, 32, length);
            set2DSprite(GetVif1Packet(), texture, CRect_i_(x + 2, position + 1, 32, length), source, 0, 0, 0, alpha * 0x50 >> 7);
            DrawMenu2DSprite(texture, CRect_i_(x, position, 32, length), source, alpha);
            DrawAttachNumberOrWeapon(x, y, top, bottom, item_no, number, alpha, 0);
        }
    }
}

void DrawAttachNumberOrWeapon(int x, int y, int top, int bottom, int item_no, int value, int alpha, int vibe) {
    int draw_y;
    int u;
    int v;
    int height;

    TexManager.GetTexture("wepicon", -1);

    if (0 < value && item_no >= ITEM_ATTACH_STAT_START && item_no < ITEM_ATTACH_GEM_START) {
        int number_x = x + 12;
        draw_y = y + 10;
        u = (value - 1) * 0x12 + 0x84;
        v = 0xB0;
        height = 0x14;
        MenuTextureClip(draw_y, v, height, top, bottom - 2);

        if (draw_y < bottom) {
            CRect_i_ source(u, v, 0x12, height);

            if (vibe != 0) {
                DrawObjectVibe(number_x, draw_y, StayTex, source, 0x80, 0x80);
            } else {
                DrawMenu2DSprite(StayTex, CRect_i_(number_x, draw_y, 0x12, height), source, alpha);
            }
        }
    }

    if (item_no == ITEM_ATTACH_SYNTHESIS_SPHERE) {
        CTexture *texture = RetCTex((s16) value, u, v);

        if (texture != NULL) {
            height = 0x20;
            draw_y = y;
            MenuTextureClip(draw_y, v, height, top, bottom);
            CRect_i_ source(u, v, 0x20, height);

            if (vibe != 0) {
                DrawObjectVibe(x, draw_y, texture, source, 0x80, 0x80);
                return;
            }

            DrawMenu2DSprite(texture, CRect_i_(x, draw_y, 0x20, height), source, (alpha * 0x80) >> 7);
        }
    }
}

void FadeTexX(int left_x, int left_width, int right_x, int right_width, char *name, int dim) {
    CTexture texture = *TexManager.GetTexture(name, -1);
    spRGBA   left;
    spRGBA   right;

    // Retail tests the local copy's address, which is never null, so a missing texture is not caught here.
    if (&texture == NULL) {
        return;
    }

    ((sceGsTex0 *) &texture.tex0)->bits.tcc = 0;
    sceGsTexa texa = mgTexa;
    texa.AEM = 1;
    texa.TA0 = 0x80;
    MGSetGsTEXA(&texa);
    right.r = right.g = right.b = 0x40;
    left.r = left.g = left.b = 0x40;

    if (left_width != 0) {
        right.a = 0x80;
        left.a = 0x80;
        set2DSprite(GetVif1Packet(), &texture, CRect_i_(0, 0, left_x, SCREEN_HEIGHT), CRect_i_(0, 0, left_x, (SCREEN_HEIGHT - 1)), &left, &right, &left, &right, 1);
    }

    left.a = 0x80;
    right.a = 0;

    if (dim == 1) {
        right.a = 0x40;
    }

    set2DSprite(GetVif1Packet(), &texture, CRect_i_(left_x, 0, left_width, SCREEN_HEIGHT), CRect_i_(left_x, 0, left_width, (SCREEN_HEIGHT - 1)), &left, &right, &left, &right, 1);
    left.a = 0;
    right.a = 0x80;

    if (dim == 1) {
        left.a = 0x40;
    }

    set2DSprite(GetVif1Packet(), &texture, CRect_i_(right_x, 0, right_width, SCREEN_HEIGHT), CRect_i_(right_x, 0, right_width, (SCREEN_HEIGHT - 1)), &left, &right, &left, &right, 1);

    if (right_width != 0) {
        right.a = 0x80;
        left.a = 0x80;
        set2DSprite(GetVif1Packet(), &texture, CRect_i_(right_x + right_width, 0, 0x280 - right_x - right_width, SCREEN_HEIGHT), CRect_i_(right_x + right_width, 0, 0x280 - right_x - right_width, (SCREEN_HEIGHT - 1)), &left, &right, &left, &right, 1);
    }

    MGSetGsTEXA(NULL);
}

CTexture *RetCTex(short item_no, int &u, int &v) {
    CTexture      *texture;
    COM_ITEM_INFO *info = GetCommonItemInfo(item_no);

    if (info == NULL) {
        return NULL;
    }

    int icon = info->icon_index;

    if (icon < 0) {
        return NULL;
    }

    u = ((icon + 8) % 8) << 5;
    v = (icon >> 3) << 5;

    switch (info->kind) {
        case ITEMKIND_ITEM:
            texture = ItemIcon;
            break;
        case ITEMKIND_ATTACH:
        case ITEMKIND_WEAPON:
            texture = WepIcon;
            break;
    }

    return texture;
}

void MenuTextureClip(int &position, int &source, int &length, int minimum, int maximum) {
    if (position < minimum && position + length > minimum) {
        length = position + length - minimum;
        source += minimum - position;
        position = minimum;
    }

    if (position < maximum && position + length > maximum) {
        length = maximum - position;
    }
}

int GetNumberKeta(int value) {
    int digits = 1;

    while (value >= 10) {
        value /= 10;
        digits++;
    }

    return digits;
}

#ifdef PAL
MENU_ICON_INFO MenuIcon[7][14] = {
    {
     {0, 0, 0x246, 0, 0x250, 0x28, 0x1E0, 0x48, 0x5E, 0x1E, 0xB},
     {1, 0, 0x20C, 0, 0x220, 0x28, 0x1E0, 0x66, 0x3A, 0x1E, 0xC},
     {2, 0, 0x1D2, 0, 0x1F0, 0x28, 0x1E0, 0x84, 0x32, 0x1E, 0xD},
     {3, 0, 0x198, 0, 0x1C0, 0x28, 0x1E0, 0xA2, 0xA0, 0x1E, 0xE},
     {4, 0, 0x124, 0, 0x160, 0x28, 0x140, 0xC0, 0x72, 0x1E, 0x10},
     {5, 0, 0x15E, 0, 0x190, 0x28, 0x140, 0xA2, 0x4E, 0x1E, 0x18},
     {6, 0, 0xA0, 0x84, 0x1A, 0x48, 0x212, 0x84, 0x6E, 0x1E, 0x1A},
     {7, 0, 0x78, 0, 0xD0, 0x28, 0x1E0, 0xC0, 0xA0, 0x1E, 0xF},
     {8, 0, 0x40, 0, 0xA0, 0x28, 0xA0, 0x48, 0xA0, 0x1E, 0x11},
     {9, 0, 0x108, 0x84, 0xD8, 0x84, 0xA0, 0x66, 0xA0, 0x1E, 0x12},
     {0xA, 0, 0xEA, 0, 0x130, 0x28, 0x140, 0x66, 0x72, 0x1E, 0x16},
     {0xB, 0, 0xB0, 0, 0x100, 0x28, 0x140, 0x84, 0x8C, 0x1E, 0x17},
     {0xC, 0, 0x108, 0xAC, 0xD8, 0xA4, 0x220, 0x66, 0x60, 0x1E, 0x13},
     {0xD, 0, 0x198, 0, 0x1C0, 0x28, 0x140, 0x48, 0x9E, 0x1E, 0x15},
     },
    {
     {0, 0, 0x246, 0, 0x250, 0x28, 0x1E0, 0x48, 0x30, 0x1E, 0xB},
     {1, 0, 0x20C, 0, 0x220, 0x28, 0x1E0, 0x66, 0x4E, 0x1E, 0xC},
     {2, 0, 0x1D2, 0, 0x1F0, 0x28, 0x22E, 0x66, 0x4B, 0x1E, 0xD},
     {3, 0, 0x198, 0, 0x1C0, 0x28, 0x1E0, 0xA2, 0x90, 0x1E, 0xE},
     {4, 0, 0x124, 0, 0x160, 0x28, 0x170, 0x66, 0x4B, 0x1E, 0x10},
     {5, 0, 0x15E, 0, 0x190, 0x28, 0x170, 0x48, 0x35, 0x1E, 0x18},
     {6, 0, 0xA0, 0x84, 0x1A, 0x48, 0x1F1, 0x84, 0x5C, 0x1E, 0x1A},
     {7, 0, 0x78, 0, 0xD0, 0x28, 0x1E0, 0xC0, 0x98, 0x1E, 0xF},
     {8, 0, 0x40, 0, 0xA0, 0x28, 0x140, 0x84, 0xAA, 0x1E, 0x11},
     {9, 0, 0x108, 0x84, 0xD8, 0x84, 0x140, 0xA2, 0x8E, 0x1E, 0x12},
     {0xA, 0, 0xEA, 0, 0x130, 0x28, 0x140, 0xC0, 0x89, 0x1E, 0x16},
     {0xB, 0, 0xB0, 0, 0x100, 0x28, 0xA0, 0x66, 0xB4, 0x1E, 0x17},
     {0xC, 0, 0x108, 0xAC, 0xD8, 0xA4, 0x210, 0x48, 0x70, 0x1E, 0x13},
     {0xD, 0, 0x198, 0, 0x1C0, 0x28, 0xA0, 0x48, 0xAD, 0x1E, 0x15},
     },
    {
     {0, 0, 0x246, 0, 0x250, 0x28, 0x1E0, 0x48, 0x30, 0x1E, 0xB},
     {1, 0, 0x20C, 0, 0x220, 0x28, 0x1E0, 0x66, 0x4E, 0x1E, 0xC},
     {2, 0, 0x1D2, 0, 0x1F0, 0x28, 0x22E, 0x66, 0x4B, 0x1E, 0xD},
     {3, 0, 0x198, 0, 0x1C0, 0x28, 0x1E0, 0xA2, 0x90, 0x1E, 0xE},
     {4, 0, 0x124, 0, 0x160, 0x28, 0x170, 0x66, 0x4B, 0x1E, 0x10},
     {5, 0, 0x15E, 0, 0x190, 0x28, 0x170, 0x48, 0x35, 0x1E, 0x18},
     {6, 0, 0xA0, 0x84, 0x1A, 0x48, 0x1F1, 0x84, 0x5C, 0x1E, 0x1A},
     {7, 0, 0x78, 0, 0xD0, 0x28, 0x1E0, 0xC0, 0x98, 0x1E, 0xF},
     {8, 0, 0x40, 0, 0xA0, 0x28, 0x140, 0x84, 0xAA, 0x1E, 0x11},
     {9, 0, 0x108, 0x84, 0xD8, 0x84, 0x140, 0xA2, 0x8E, 0x1E, 0x12},
     {0xA, 0, 0xEA, 0, 0x130, 0x28, 0x140, 0xC0, 0x89, 0x1E, 0x16},
     {0xB, 0, 0xB0, 0, 0x100, 0x28, 0xA0, 0x66, 0xB4, 0x1E, 0x17},
     {0xC, 0, 0x108, 0xAC, 0xD8, 0xA4, 0x210, 0x48, 0x70, 0x1E, 0x13},
     {0xD, 0, 0x198, 0, 0x1C0, 0x28, 0xA0, 0x48, 0xAD, 0x1E, 0x15},
     },
    {
     {0, 0, 0x246, 0, 0x250, 0x28, 0x1D0, 0x48, 0x3C, 0x1E, 0xB},
     {1, 0, 0x20C, 0, 0x220, 0x28, 0x1E8, 0x66, 0x4A, 0x1E, 0xC},
     {2, 0, 0x1D2, 0, 0x1F0, 0x28, 0x232, 0x66, 0x4E, 0x1E, 0xD},
     {3, 0, 0x198, 0, 0x1C0, 0x28, 0x1EA, 0xA2, 0x96, 0x1E, 0xE},
     {4, 0, 0x124, 0, 0x160, 0x28, 0x170, 0x66, 0x4B, 0x1E, 0x10},
     {5, 0, 0x15E, 0, 0x190, 0x28, 0x168, 0x48, 0x50, 0x1E, 0x18},
     {6, 0, 0xA0, 0x84, 0x1A, 0x48, 0x224, 0x84, 0x5C, 0x1E, 0x1A},
     {7, 0, 0x78, 0, 0xD0, 0x28, 0x1EE, 0xC0, 0x92, 0x1E, 0xF},
     {8, 0, 0x40, 0, 0xA0, 0x28, 0x140, 0x84, 0xCC, 0x1E, 0x11},
     {9, 0, 0x108, 0x84, 0xD8, 0x84, 0x140, 0xA2, 0x94, 0x1E, 0x12},
     {0xA, 0, 0xEA, 0, 0x130, 0x28, 0x140, 0xC0, 0xA6, 0x1E, 0x16},
     {0xB, 0, 0xB0, 0, 0x100, 0x28, 0xA0, 0x66, 0x8E, 0x1E, 0x17},
     {0xC, 0, 0x108, 0xAC, 0xD8, 0xA4, 0x236, 0x48, 0x4A, 0x1E, 0x13},
     {0xD, 0, 0x198, 0, 0x1C0, 0x28, 0xA0, 0x48, 0x82, 0x1E, 0x15},
     },
    {
     {0, 0, 0x246, 0, 0x250, 0x28, 0x1AE, 0x66, 0x6E, 0x1E, 0xB},
     {1, 0, 0x20C, 0, 0x220, 0x28, 0x1B8, 0x4A, 0x46, 0x1E, 0xC},
     {2, 0, 0x1D2, 0, 0x1F0, 0x28, 0x21C, 0x66, 0x62, 0x1E, 0xD},
     {3, 0, 0x198, 0, 0x1C0, 0x28, 0x1F0, 0xA2, 0x8C, 0x1E, 0xE},
     {4, 0, 0x124, 0, 0x160, 0x28, 0x15E, 0x66, 0x50, 0x1E, 0x10},
     {5, 0, 0x15E, 0, 0x190, 0x28, 0x15E, 0x48, 0x5C, 0x1E, 0x18},
     {6, 0, 0xA0, 0x84, 0x1A, 0x48, 0x1F1, 0x84, 0x74, 0x1E, 0x1A},
     {7, 0, 0x78, 0, 0xD0, 0x28, 0x1EE, 0xC0, 0x90, 0x1E, 0xF},
     {8, 0, 0x40, 0, 0xA0, 0x28, 0x140, 0x84, 0x82, 0x1E, 0x11},
     {9, 0, 0x108, 0x84, 0xD8, 0x84, 0x140, 0xA2, 0x8E, 0x1E, 0x12},
     {0xA, 0, 0xEA, 0, 0x130, 0x28, 0x140, 0xC0, 0x9E, 0x1E, 0x16},
     {0xB, 0, 0xB0, 0, 0x100, 0x28, 0xA0, 0x66, 0xB4, 0x1E, 0x17},
     {0xC, 0, 0x108, 0xAC, 0xD8, 0xA4, 0x1FE, 0x48, 0x82, 0x1E, 0x13},
     {0xD, 0, 0x198, 0, 0x1C0, 0x28, 0xA0, 0x48, 0xAD, 0x1E, 0x15},
     },
    {
     {0, 0, 0x246, 0, 0x250, 0x28, 0x1CA, 0x48, 0x42, 0x1E, 0xB},
     {1, 0, 0x20C, 0, 0x220, 0x28, 0x1EA, 0x66, 0x3E, 0x1E, 0xC},
     {2, 0, 0x1D2, 0, 0x1F0, 0x28, 0x22E, 0x66, 0x52, 0x1E, 0xD},
     {3, 0, 0x198, 0, 0x1C0, 0x28, 0x1DC, 0xA2, 0xA4, 0x1E, 0xE},
     {4, 0, 0x124, 0, 0x160, 0x28, 0x16C, 0x66, 0x5A, 0x1E, 0x10},
     {5, 0, 0x15E, 0, 0x190, 0x28, 0x162, 0x48, 0x56, 0x1E, 0x18},
     {6, 0, 0xA0, 0x84, 0x1A, 0x48, 0x1F4, 0x84, 0x64, 0x1E, 0x1A},
     {7, 0, 0x78, 0, 0xD0, 0x28, 0x1E0, 0xC0, 0xA2, 0x1E, 0xF},
     {8, 0, 0x40, 0, 0xA0, 0x28, 0x140, 0x84, 0xAA, 0x1E, 0x11},
     {9, 0, 0x108, 0x84, 0xD8, 0x84, 0x140, 0xA2, 0x92, 0x1E, 0x12},
     {0xA, 0, 0xEA, 0, 0x130, 0x28, 0x140, 0xC0, 0x98, 0x1E, 0x16},
     {0xB, 0, 0xB0, 0, 0x100, 0x28, 0xA0, 0x66, 0xB6, 0x1E, 0x17},
     {0xC, 0, 0x108, 0xAC, 0xD8, 0xA4, 0x212, 0x48, 0x6E, 0x1E, 0x13},
     {0xD, 0, 0x198, 0, 0x1C0, 0x28, 0xA0, 0x48, 0xBC, 0x1E, 0x15},
     },
    {
     {0, 0, 0x246, 0, 0x250, 0x28, 0x1D2, 0x48, 0x4C, 0x1E, 0xB},
     {1, 0, 0x20C, 0, 0x220, 0x28, 0x1E4, 0x66, 0x3E, 0x1E, 0xC},
     {2, 0, 0x1D2, 0, 0x1F0, 0x28, 0x224, 0x66, 0x5A, 0x1E, 0xD},
     {3, 0, 0x198, 0, 0x1C0, 0x28, 0x1DA, 0xA2, 0x9E, 0x1E, 0xE},
     {4, 0, 0x124, 0, 0x160, 0x28, 0x16C, 0x66, 0x4E, 0x1E, 0x10},
     {5, 0, 0x15E, 0, 0x190, 0x28, 0x168, 0x48, 0x5E, 0x1E, 0x18},
     {6, 0, 0xA0, 0x84, 0x1A, 0x48, 0x1EE, 0x84, 0x6A, 0x1E, 0x1A},
     {7, 0, 0x78, 0, 0xD0, 0x28, 0x1D2, 0xC0, 0xAE, 0x1E, 0xF},
     {8, 0, 0x40, 0, 0xA0, 0x28, 0x140, 0x84, 0xAA, 0x1E, 0x11},
     {9, 0, 0x108, 0x84, 0xD8, 0x84, 0x14A, 0xA2, 0x80, 0x1E, 0x12},
     {0xA, 0, 0xEA, 0, 0x130, 0x28, 0x14E, 0xC0, 0x74, 0x1E, 0x16},
     {0xB, 0, 0xB0, 0, 0x100, 0x28, 0xA0, 0x66, 0xB0, 0x1E, 0x17},
     {0xC, 0, 0x108, 0xAC, 0xD8, 0xA4, 0x228, 0x48, 0x40, 0x1E, 0x13},
     {0xD, 0, 0x198, 0, 0x1C0, 0x28, 0xA4, 0x48, 0xB0, 0x1E, 0x15},
     },
};
#else
MENU_ICON_INFO MenuIcon[7][14] = {
    {
     {0, 0, 0x246, 0, 0x250, 0x28, 0x1E0, 0x48, 0x5E, 0x1E, 0xB},
     {1, 0, 0x20C, 0, 0x220, 0x28, 0x1E0, 0x66, 0x3A, 0x1E, 0xC},
     {2, 0, 0x1D2, 0, 0x1F0, 0x28, 0x1E0, 0x84, 0x32, 0x1E, 0xD},
     {3, 0, 0x198, 0, 0x1C0, 0x28, 0x1E0, 0xA2, 0xA0, 0x1E, 0xE},
     {4, 0, 0x124, 0, 0x160, 0x28, 0x140, 0xC0, 0x72, 0x1E, 0x10},
     {5, 0, 0x15E, 0, 0x190, 0x28, 0x140, 0xA2, 0x4E, 0x1E, 0x18},
     {6, 0, 0xA0, 0x84, 0x1A, 0x48, 0x212, 0x84, 0x6E, 0x1E, 0x1A},
     {7, 0, 0x78, 0, 0xD0, 0x28, 0x1E0, 0xC0, 0xA0, 0x1E, 0xF},
     {8, 0, 0x40, 0, 0xA0, 0x28, 0xA0, 0x48, 0xA0, 0x1E, 0x11},
     {9, 0, 0x108, 0x84, 0xD8, 0x84, 0xA0, 0x66, 0xA0, 0x1E, 0x12},
     {0xA, 0, 0xEA, 0, 0x130, 0x28, 0x140, 0x66, 0x72, 0x1E, 0x16},
     {0xB, 0, 0xB0, 0, 0x100, 0x28, 0x140, 0x84, 0x8C, 0x1E, 0x17},
     {0xC, 0, 0x108, 0xAC, 0xD8, 0xA4, 0x220, 0x66, 0x60, 0x1E, 0x13},
     {0xD, 0, 0x198, 0, 0x1C0, 0x28, 0x140, 0x48, 0x9E, 0x1E, 0x15},
     },
    {
     {0, 0, 0x246, 0, 0x250, 0x28, 0x1E0, 0x48, 0x30, 0x1E, 0xB},
     {1, 0, 0x20C, 0, 0x220, 0x28, 0x1E0, 0x66, 0x4E, 0x1E, 0xC},
     {2, 0, 0x1D2, 0, 0x1F0, 0x28, 0x22E, 0x66, 0x4B, 0x1E, 0xD},
     {3, 0, 0x198, 0, 0x1C0, 0x28, 0x1E0, 0xA2, 0x8E, 0x1E, 0xE},
     {4, 0, 0x124, 0, 0x160, 0x28, 0x170, 0x66, 0x4B, 0x1E, 0x10},
     {5, 0, 0x15E, 0, 0x190, 0x28, 0x170, 0x48, 0x35, 0x1E, 0x18},
     {6, 0, 0xA0, 0x84, 0x1A, 0x48, 0x1F1, 0x84, 0x5C, 0x1E, 0x1A},
     {7, 0, 0x78, 0, 0xD0, 0x28, 0x1E0, 0xC0, 0x98, 0x1E, 0xF},
     {8, 0, 0x40, 0, 0xA0, 0x28, 0x140, 0x84, 0xAA, 0x1E, 0x11},
     {9, 0, 0x108, 0x84, 0xD8, 0x84, 0x140, 0xA2, 0x8E, 0x1E, 0x12},
     {0xA, 0, 0xEA, 0, 0x130, 0x28, 0x140, 0xC0, 0x89, 0x1E, 0x16},
     {0xB, 0, 0xB0, 0, 0x100, 0x28, 0xA0, 0x66, 0xB4, 0x1E, 0x17},
     {0xC, 0, 0x108, 0xAC, 0xD8, 0xA4, 0x210, 0x48, 0x70, 0x1E, 0x13},
     {0xD, 0, 0x198, 0, 0x1C0, 0x28, 0xA0, 0x48, 0xAD, 0x1E, 0x15},
     },
    {
     {0, 0, 0x246, 0, 0x250, 0x28, 0x1E0, 0x48, 0x30, 0x1E, 0xB},
     {1, 0, 0x20C, 0, 0x220, 0x28, 0x1E0, 0x66, 0x4E, 0x1E, 0xC},
     {2, 0, 0x1D2, 0, 0x1F0, 0x28, 0x22E, 0x66, 0x4B, 0x1E, 0xD},
     {3, 0, 0x198, 0, 0x1C0, 0x28, 0x1E0, 0xA2, 0x85, 0x1E, 0xE},
     {4, 0, 0x124, 0, 0x160, 0x28, 0x170, 0x66, 0x4B, 0x1E, 0x10},
     {5, 0, 0x15E, 0, 0x190, 0x28, 0x170, 0x48, 0x35, 0x1E, 0x18},
     {6, 0, 0xA0, 0x84, 0x1A, 0x48, 0x1F1, 0x84, 0x5C, 0x1E, 0x1A},
     {7, 0, 0x78, 0, 0xD0, 0x28, 0x1E0, 0xC0, 0x98, 0x1E, 0xF},
     {8, 0, 0x40, 0, 0xA0, 0x28, 0x140, 0x84, 0xAA, 0x1E, 0x11},
     {9, 0, 0x108, 0x84, 0xD8, 0x84, 0x140, 0xA2, 0x8E, 0x1E, 0x12},
     {0xA, 0, 0xEA, 0, 0x130, 0x28, 0x140, 0xC0, 0x89, 0x1E, 0x16},
     {0xB, 0, 0xB0, 0, 0x100, 0x28, 0xA0, 0x66, 0xB4, 0x1E, 0x17},
     {0xC, 0, 0x108, 0xAC, 0xD8, 0xA4, 0x210, 0x48, 0x70, 0x1E, 0x13},
     {0xD, 0, 0x198, 0, 0x1C0, 0x28, 0xA0, 0x48, 0xAD, 0x1E, 0x15},
     },
    {
     {0, 0, 0x246, 0, 0x250, 0x28, 0x1E0, 0x48, 0x30, 0x1E, 0xB},
     {1, 0, 0x20C, 0, 0x220, 0x28, 0x1E0, 0x66, 0x4E, 0x1E, 0xC},
     {2, 0, 0x1D2, 0, 0x1F0, 0x28, 0x22E, 0x66, 0x4B, 0x1E, 0xD},
     {3, 0, 0x198, 0, 0x1C0, 0x28, 0x1E0, 0xA2, 0x85, 0x1E, 0xE},
     {4, 0, 0x124, 0, 0x160, 0x28, 0x170, 0x66, 0x4B, 0x1E, 0x10},
     {5, 0, 0x15E, 0, 0x190, 0x28, 0x170, 0x48, 0x35, 0x1E, 0x18},
     {6, 0, 0xA0, 0x84, 0x1A, 0x48, 0x1F1, 0x84, 0x5C, 0x1E, 0x1A},
     {7, 0, 0x78, 0, 0xD0, 0x28, 0x1E0, 0xC0, 0x98, 0x1E, 0xF},
     {8, 0, 0x40, 0, 0xA0, 0x28, 0x140, 0x84, 0xAA, 0x1E, 0x11},
     {9, 0, 0x108, 0x84, 0xD8, 0x84, 0x140, 0xA2, 0x8E, 0x1E, 0x12},
     {0xA, 0, 0xEA, 0, 0x130, 0x28, 0x140, 0xC0, 0x89, 0x1E, 0x16},
     {0xB, 0, 0xB0, 0, 0x100, 0x28, 0xA0, 0x66, 0xB4, 0x1E, 0x17},
     {0xC, 0, 0x108, 0xAC, 0xD8, 0xA4, 0x210, 0x48, 0x70, 0x1E, 0x13},
     {0xD, 0, 0x198, 0, 0x1C0, 0x28, 0xA0, 0x48, 0xAD, 0x1E, 0x15},
     },
    {
     {0, 0, 0x246, 0, 0x250, 0x28, 0x1E0, 0x48, 0x30, 0x1E, 0xB},
     {1, 0, 0x20C, 0, 0x220, 0x28, 0x1E0, 0x66, 0x4E, 0x1E, 0xC},
     {2, 0, 0x1D2, 0, 0x1F0, 0x28, 0x22E, 0x66, 0x4B, 0x1E, 0xD},
     {3, 0, 0x198, 0, 0x1C0, 0x28, 0x1E0, 0xA2, 0x85, 0x1E, 0xE},
     {4, 0, 0x124, 0, 0x160, 0x28, 0x170, 0x66, 0x4B, 0x1E, 0x10},
     {5, 0, 0x15E, 0, 0x190, 0x28, 0x170, 0x48, 0x35, 0x1E, 0x18},
     {6, 0, 0xA0, 0x84, 0x1A, 0x48, 0x1F1, 0x84, 0x5C, 0x1E, 0x1A},
     {7, 0, 0x78, 0, 0xD0, 0x28, 0x1E0, 0xC0, 0x98, 0x1E, 0xF},
     {8, 0, 0x40, 0, 0xA0, 0x28, 0x140, 0x84, 0xAA, 0x1E, 0x11},
     {9, 0, 0x108, 0x84, 0xD8, 0x84, 0x140, 0xA2, 0x8E, 0x1E, 0x12},
     {0xA, 0, 0xEA, 0, 0x130, 0x28, 0x140, 0xC0, 0x89, 0x1E, 0x16},
     {0xB, 0, 0xB0, 0, 0x100, 0x28, 0xA0, 0x66, 0xB4, 0x1E, 0x17},
     {0xC, 0, 0x108, 0xAC, 0xD8, 0xA4, 0x210, 0x48, 0x70, 0x1E, 0x13},
     {0xD, 0, 0x198, 0, 0x1C0, 0x28, 0xA0, 0x48, 0xAD, 0x1E, 0x15},
     },
    {
     {0, 0, 0x246, 0, 0x250, 0x28, 0x1E0, 0x48, 0x30, 0x1E, 0xB},
     {1, 0, 0x20C, 0, 0x220, 0x28, 0x1E0, 0x66, 0x4E, 0x1E, 0xC},
     {2, 0, 0x1D2, 0, 0x1F0, 0x28, 0x22E, 0x66, 0x4B, 0x1E, 0xD},
     {3, 0, 0x198, 0, 0x1C0, 0x28, 0x1E0, 0xA2, 0x85, 0x1E, 0xE},
     {4, 0, 0x124, 0, 0x160, 0x28, 0x170, 0x66, 0x4B, 0x1E, 0x10},
     {5, 0, 0x15E, 0, 0x190, 0x28, 0x170, 0x48, 0x35, 0x1E, 0x18},
     {6, 0, 0xA0, 0x84, 0x1A, 0x48, 0x1F1, 0x84, 0x5C, 0x1E, 0x1A},
     {7, 0, 0x78, 0, 0xD0, 0x28, 0x1E0, 0xC0, 0x98, 0x1E, 0xF},
     {8, 0, 0x40, 0, 0xA0, 0x28, 0x140, 0x84, 0xAA, 0x1E, 0x11},
     {9, 0, 0x108, 0x84, 0xD8, 0x84, 0x140, 0xA2, 0x8E, 0x1E, 0x12},
     {0xA, 0, 0xEA, 0, 0x130, 0x28, 0x140, 0xC0, 0x89, 0x1E, 0x16},
     {0xB, 0, 0xB0, 0, 0x100, 0x28, 0xA0, 0x66, 0xB4, 0x1E, 0x17},
     {0xC, 0, 0x108, 0xAC, 0xD8, 0xA4, 0x210, 0x48, 0x70, 0x1E, 0x13},
     {0xD, 0, 0x198, 0, 0x1C0, 0x28, 0xA0, 0x48, 0xAD, 0x1E, 0x15},
     },
    {
     {0, 0, 0x246, 0, 0x250, 0x28, 0x1E0, 0x48, 0x30, 0x1E, 0xB},
     {1, 0, 0x20C, 0, 0x220, 0x28, 0x1E0, 0x66, 0x4E, 0x1E, 0xC},
     {2, 0, 0x1D2, 0, 0x1F0, 0x28, 0x22E, 0x66, 0x4B, 0x1E, 0xD},
     {3, 0, 0x198, 0, 0x1C0, 0x28, 0x1E0, 0xA2, 0x85, 0x1E, 0xE},
     {4, 0, 0x124, 0, 0x160, 0x28, 0x170, 0x66, 0x4B, 0x1E, 0x10},
     {5, 0, 0x15E, 0, 0x190, 0x28, 0x170, 0x48, 0x35, 0x1E, 0x18},
     {6, 0, 0xA0, 0x84, 0x1A, 0x48, 0x1F1, 0x84, 0x5C, 0x1E, 0x1A},
     {7, 0, 0x78, 0, 0xD0, 0x28, 0x1E0, 0xC0, 0x98, 0x1E, 0xF},
     {8, 0, 0x40, 0, 0xA0, 0x28, 0x140, 0x84, 0xAA, 0x1E, 0x11},
     {9, 0, 0x108, 0x84, 0xD8, 0x84, 0x140, 0xA2, 0x8E, 0x1E, 0x12},
     {0xA, 0, 0xEA, 0, 0x130, 0x28, 0x140, 0xC0, 0x89, 0x1E, 0x16},
     {0xB, 0, 0xB0, 0, 0x100, 0x28, 0xA0, 0x66, 0xB4, 0x1E, 0x17},
     {0xC, 0, 0x108, 0xAC, 0xD8, 0xA4, 0x210, 0x48, 0x70, 0x1E, 0x13},
     {0xD, 0, 0x198, 0, 0x1C0, 0x28, 0xA0, 0x48, 0xAD, 0x1E, 0x15},
     },
};
#endif

MENU_ICON_INFO *GetMenuIconInfo(int icon) {
    return &MenuIcon[GetMenuLangFlag()][icon];
}

void DrawMainMenuIcon(int x, int y, int icon, int selected, int bright, int alpha) {
    if (StayTex == NULL) {
        StayTex = TexManager.GetTexture("stayframe", -1);

        if (StayTex == NULL) {
            return;
        }
    }

    MENU_ICON_INFO *info = GetMenuIconInfo(icon);

    if (info == NULL) {
        return;
    }

    int u = info->u;
    int v = info->v;
    s16 frame_size[2][2] = {
        {0x30, 0x20},
        {0x3A, 0x28}
    };
    int width = frame_size[selected][0];
    int height = frame_size[selected][1];

    if (selected) {
        u = info->selected_u;
        v = info->selected_v;

        if (icon == info->id) {
            width = 0x38;
        }
    }

    set2DSprite(GetVif1Packet(), StayTex, CRect_i_(x, y, width, height - 1), CRect_i_(u, v, width, height), bright, bright, bright, alpha);

    if (selected) {
        x += 0x44;
        y += 1;
    } else {
        x += 0x40;
        y -= 1;
    }

    set2DSprite(GetVif1Packet(), StayTex, CRect_i_(x, y, info->label_width, info->label_height - 1), CRect_i_(info->label_u, info->label_v, info->label_width, info->label_height), bright, bright, bright, alpha);
}

void DrawMenuVibeItem(int x, int y, int offset_x, int offset_y, int alpha) {
    int       item_x = x + offset_x;
    int       item_y = y + offset_y;
    s16       item_no = PerBoardPt->held_item.item_no;
    CRect_i_  shadow(0x80, 0x28, 0x20, 0x20);
    int       u;
    int       v;
    CTexture *texture = RetCTex(item_no, u, v);

    if (texture != NULL) {
        DrawObjectVibe(x + 4, y + 2, TexManager.GetTexture("StayTex", -1), shadow, 0, 0x50);
        CRect_i_ source(u, v, 0x20, 0x20);
        DrawObjectVibe(item_x + 4, item_y + 2, texture, source, 0, 0x50);
        DrawObjectVibe(item_x, item_y, texture, source, 0x80, 0x80);
        int number = GetAttachVolumeForMsg(&PerBoardPt->held_attach);

        if (item_no == ITEM_ATTACH_SYNTHESIS_SPHERE) {
            number = PerBoardPt->held_attach.sphere_weapon_no;
        }

        DrawAttachNumberOrWeapon(item_x, item_y, 0, 0x280, item_no, number, 0x80, 1);
    }
}

int GetMainMenuRightHelpWinLangOffset(float &x, float &y, float &width, float &height) {
    float offsets[7][4] = {
        {320.0f, 325.0f, 14.6f, 2.2f},
#ifdef PAL
        {318.0f, 352.0f, 15.6f, 2.7f},
        {318.0f, 352.0f, 15.6f, 2.7f},
        {318.0f, 352.0f, 15.6f, 2.7f},
        {318.0f, 352.0f, 15.6f, 2.7f},
        {318.0f, 352.0f, 15.6f, 2.7f},
        {318.0f, 352.0f, 15.6f, 2.7f},
#else
        {318.0f, 320.0f, 15.6f, 2.7f},
        {318.0f, 320.0f, 15.6f, 2.7f},
        {318.0f, 320.0f, 15.6f, 2.7f},
        {318.0f, 320.0f, 15.6f, 2.7f},
        {318.0f, 320.0f, 15.6f, 2.7f},
        {318.0f, 320.0f, 15.6f, 2.7f},
#endif
    };
    int lang = GetMenuLangFlag();

    x = offsets[lang][0];
    y = offsets[lang][1];
    width = offsets[lang][2];
    height = offsets[lang][3];
    return 0;
}

int GetMainMenuRightHelpMsgLangOffset(int &x, int &y) {
    s8 offsets[7][2] = {
        {18, 16},
        {20, 12},
        {20, 12},
        {20, 12},
#ifdef PAL
        {20, 14},
#else
        {20, 12},
#endif
        {20, 12},
        {20, 12},
    };
    int lang = GetMenuLangFlag();

    x = offsets[lang][0];
    y = offsets[lang][1];
    return 1;
}

void InitHaveData(IHAVEITEM *item) {
    memset(item, -1, 0x14U);
}

void InitHaveWep(WEAPON_HAVE *weapon) {
    memset(weapon, 0, 0xF8U);
    weapon->item_no = -1;
}

void InitHaveAttach(ATTACH_LIST *attachment) {
    memset(attachment, 0, 0x20U);
}

void MenuDataSwap(s16 *first, s16 *second) {
    s16 saved;

    if ((first == NULL) || (second == NULL)) {
        return;
    }

    saved = *first;
    *first = *second;
    *second = saved;
}

void MenuDataSwap(int *first, int *second) {
    int saved;

    if ((first == NULL) || (second == NULL)) {
        return;
    }

    saved = *first;
    *first = *second;
    *second = saved;
}

void MenuDataSwap(WEAPON_HAVE *first, WEAPON_HAVE *second) {
    WEAPON_HAVE saved;

    if ((first == NULL) || (second == NULL)) {
        return;
    }

    memcpy(&saved, first, sizeof(WEAPON_HAVE));
    memcpy(first, second, sizeof(WEAPON_HAVE));
    memcpy(second, &saved, sizeof(WEAPON_HAVE));
}

void MenuDataSwap(ATTACH_LIST *first, ATTACH_LIST *second) {
    ATTACH_LIST saved;

    if ((first == NULL) || (second == NULL)) {
        return;
    }

    memcpy(&saved, first, sizeof(ATTACH_LIST));
    memcpy(first, second, sizeof(ATTACH_LIST));
    memcpy(second, &saved, sizeof(ATTACH_LIST));
}

void SetMenuTrushMark(ITEM_PACK *items) {
    int quick_count = 0;
    int i;
    int slot;

    for (i = 0; i < 3; i++) {
        quick_count += items->quick_item_qty[i];
    }

    for (slot = items->num - 1; slot >= 0; slot--) {
        if (items->item[slot] < ITEM_DUNGEON_START && quick_count > 0) {
            MenuTrushMark[slot] = 1;
            quick_count--;
        } else {
            MenuTrushMark[slot] = 0;
        }
    }
}

void DeleteMenuTrushMark() {
    memset(MenuTrushMark, 0, sizeof(MenuTrushMark));
}

void InitPersonalBoardMode(CUserStatus *status, PERSONAL_BOARD *board, int mode, int page) {
    PerBoardStatusPt = status;
    PerBoardPt = board;

    if (PerBoardTex == NULL) {
        PerBoardTex = TexManager.GetTexture("perbrd", -1);
    }

    PerBoardPt->menu_kind = mode;
    PerBoardPt->page = page;
    PerBoardPt->cursor = 0;
    PerBoardPt->top_row = 0;
    PerBoardPt->item_pack = &PerBoardStatusPt->item_pack;
    PerBoardPt->consumables = PerBoardStatusPt->consumable_items;
    int rows = PersonalRetMax(PerBoardPt->page) / 5;

    if (rows <= 0) {
        rows = 1;
    }

    PerBoardPt->y = 0x7F - PerBoardPt->top_row * 0x28;
    PerBoardPt->scroll = 140.0f + 114.0f * PerBoardPt->top_row / rows;
    PerBoardPt->cursor_area = PERSONAL_BOARD_AREA_CELLS;
    InitHaveData(&PerBoardPt->held_item);
    InitHaveWep(&PerBoardPt->weapon);
    PerBoardPt->held_equipped_slot = -1;
    PerBoardPt->trash_anim = 0;
    PerBoardPt->trash_frame = 0;
    DeleteMenuTrushMark();
    s16 full = BOARD_PAGE_NONE;

    if (PerBoardStatusPt->overflow_flag != 0) {
        ITEM_PACK *pack = PerBoardPt->item_pack;

        if (pack->item[pack->num] >= ITEM_DUNGEON_START) {
            full = BOARD_PAGE_ITEM;
        }

        if (PerBoardPt->consumables[41].id >= ITEM_ATTACH_START) {
            full = BOARD_PAGE_ATTACH;
        }

        for (int i = 0; i < 6; i++) {
            if (((CUserStatus *) PerBoardStatusPt)->chara_weapons[i][10].item_no >= ITEM_WEAPON_START) {
                full = BOARD_PAGE_WEAPON;
                break;
            }
        }

        switch (full) {
            case BOARD_PAGE_ITEM:
                SetMenuTrushMark(pack);
                break;
        }
    }
}

int BoardModeChangeKey() {
    int board_mode = PerBoardPt->page;

    if (GamePad.Down(PAD_L2 | PAD_L1)) {
        switch (PerBoardPt->menu_kind) {
            case PERSONAL_BOARD_ITEM:
                PerBoardPt->page--;

                if (PerBoardPt->page < BOARD_PAGE_ITEM) {
                    PerBoardPt->page = BOARD_PAGE_ATTACH;
                }

                break;
            case PERSONAL_BOARD_ATLA:
            case PERSONAL_BOARD_WEAPON_ATTACH:
                break;
        }
    }

    if (GamePad.Down(PAD_R2 | PAD_R1)) {
        switch (PerBoardPt->menu_kind) {
            case PERSONAL_BOARD_ITEM:
                PerBoardPt->page++;

                if (PerBoardPt->page > BOARD_PAGE_ATTACH) {
                    PerBoardPt->page = BOARD_PAGE_ITEM;
                }

                break;
            case PERSONAL_BOARD_ATLA:
            case PERSONAL_BOARD_WEAPON_ATTACH:
                break;
        }
    }

    if (board_mode != PerBoardPt->page) {
        return 1;
    }

    return 0;
}

void PersonalBoardLimmitCheck() {
    int  page = PerBoardPt->page;
    int *cursor = &PerBoardPt->cursor;
    int  max = PersonalRetMax(page);
    int  top_row = PerBoardPt->top_row;
    int  last_top = max / 5 - 4;

    if (last_top < 0) {
        last_top = 0;
    }

    if (max - 1 < *cursor) {
        while (*cursor >= max) {
            *cursor -= 5;
        }

        PerBoardPt->top_row = *cursor / 5 - 3;

        if (PerBoardPt->top_row < 0) {
            PerBoardPt->top_row = 0;
        }

        PerBoardPt->y = 0x7F - PerBoardPt->top_row * 0x28;
        int rows = max / 5;

        if (rows <= 0) {
            rows = 4;
        }

        PerBoardPt->scroll = 140.0f + 114.0f * PerBoardPt->top_row / rows;
        return;
    }

    if (last_top < top_row) {
        if (last_top < 0) {
            last_top = 0;
        }

        while (last_top < PerBoardPt->top_row) {
            PerBoardPt->top_row--;
        }
    }
}

int PersonalBoardKeySub() {
    int  exit_left = 0;
    int  area = PerBoardPt->cursor_area;
    int  page = PerBoardPt->page;
    int *cursor = &PerBoardPt->cursor;
    int  max = PersonalRetMax(page);

    if (GamePad.Down(PAD_UP) != 0) {
        switch (PerBoardPt->cursor_area) {
            case PERSONAL_BOARD_AREA_CELLS:
                if (*cursor > 4) {
                    *cursor -= 5;
                }

                if (*cursor / 5 < PerBoardPt->top_row) {
                    PerBoardPt->top_row--;
                }

                break;
            case PERSONAL_BOARD_AREA_TRASH:
                break;
        }
    }

    if (GamePad.Down(PAD_DOWN) != 0) {
        switch (PerBoardPt->cursor_area) {
            case PERSONAL_BOARD_AREA_CELLS:
                if (*cursor < max - 5) {
                    *cursor += 5;
                }

                if (*cursor / 5 > PerBoardPt->top_row + 3) {
                    PerBoardPt->top_row++;
                }

                break;
            case PERSONAL_BOARD_AREA_TRASH:
                break;
        }
    }

    if (GamePad.Down(PAD_RIGHT) != 0) {
        switch (PerBoardPt->cursor_area) {
            case PERSONAL_BOARD_AREA_TRASH:
                break;
            case PERSONAL_BOARD_AREA_CELLS: {
                int column = *cursor % 5;

                if (column < 4) {
                    (*cursor)++;
                } else if (column == 4 && PerBoardPt->menu_kind != PERSONAL_BOARD_ATLA) {
                    PerBoardPt->cursor_area = PERSONAL_BOARD_AREA_TRASH;
                }

                break;
            }
        }
    }

    if (GamePad.Down(PAD_LEFT) != 0) {
        switch (PerBoardPt->cursor_area) {
            case PERSONAL_BOARD_AREA_TRASH:
                PerBoardPt->cursor_area = PERSONAL_BOARD_AREA_CELLS;
                break;
            case PERSONAL_BOARD_AREA_CELLS:
                if (*cursor % 5 == 0) {
                    exit_left = 1;
                } else {
                    (*cursor)--;
                }

                break;
        }
    }

    if (area != PerBoardPt->cursor_area) {
        ComMenuSePlay(MENU_SOUND_CURSOR);
    }

    return exit_left;
}

int PersonalBoardKey() {
    BoardModeChangeKey();
    PersonalBoardLimmitCheck();
    return PersonalBoardKeySub();
}

/**
 * Moves a selected item into the personal board.
 */
int PersonalBoardItemPush(IHAVEITEM *item, int board_pos) {
    int enabled = false;
    int kind = WhatIsKindofItem(item->item_no);

    if (kind == BOARD_PAGE_ITEM || kind < 0) {
        s16 *board_item = &PerBoardPt->item_pack->item[board_pos];
        s16 *board_volume = &PerBoardPt->item_pack->item_vol[board_pos];

        if (MenuTrushMark[board_pos] == 1) {
            enabled = false;
        } else {
            MenuDataSwap(board_item, &item->item_no);
            MenuDataSwap(board_volume, &item->volume);

            if (item->item_no >= ITEM_ATTACH_START) {
                item->from_page = BOARD_PAGE_ITEM;
                item->from_slot = PerBoardPt->cursor;
            }

            if (item->item_no >= ITEM_DUNGEON_START || *board_item >= ITEM_DUNGEON_START) {
                enabled = true;
            }
        }
    }

    return enabled;
}

int PersonalBoardWeaponPush(IHAVEITEM *item, int board_pos) {
    int          result = 0;
    int          kind = WhatIsKindofItem(item->item_no);
    int          chara = board_pos / 10;
    int          slot = board_pos % 10;
    CUserStatus *status = PerBoardStatusPt;
    WEAPON_HAVE *weapons = status->chara_weapons[chara];
    WEAPON_HAVE *weapon = &weapons[slot];
    int          weapon_no = weapon->item_no;

    if (kind == BOARD_PAGE_ITEM) {
        if (weapon_no < ITEM_WEAPON_START) {
            return 0;
        }

        if (item->item_no == ITEM_REPAIR_POWDER) {
            if (weapon->durability_f < weapon->durability) {
                weapon->durability_f = weapon->durability;

                if (weapon->item_no == GetDefaultWeaponNo(chara)) {
                    weapon->item_no++;
                    WepDataListToHaveCopy(weapon->item_no, weapon);
                }

                InitHaveData(item);
                result = 2;
            }
        }

        if (item->item_no == ITEM_POWERUP_POWDER) {
            if (weapon->experience < GetWeaponMaxExp(weapon) && weapon->level < 99) {
                int item_no = weapon->item_no;

                if (item_no != GetDefaultWeaponNo(chara)) {
                    if (item_no == ITEM_WEAPON_SERPENT_SWORD) {
                        if (GetMenuHebikiriFlag() != 0) {
                            result = 2;
                        }
                    } else {
                        result = 2;
                    }
                }
            }
        }
    }

    if (kind == BOARD_PAGE_WEAPON || kind < 0) {
        if (kind == BOARD_PAGE_WEAPON) {
            int owner = WhoIsWeaponEquip(item->item_no);

            if (owner != chara) {
                result = 0;
            } else {
                int held = item->item_no;
                int placed = weapon->item_no;
                MenuDataSwap(&PerBoardPt->weapon, weapon);
                item->item_no = placed;
                weapon->item_no = held;

                if (slot == PerBoardStatusPt->equipped_weapon_slot[owner]) {
                    PerBoardPt->held_equipped_slot = slot;
                    PerBoardStatusPt->equipped_weapon_slot[owner] = -1;
                } else if (PerBoardPt->held_equipped_slot >= 0) {
                    PerBoardStatusPt->equipped_weapon_slot[owner] = slot;
                    PerBoardPt->held_equipped_slot = -1;
                }

                result = 1;
                item->from_page = BOARD_PAGE_WEAPON;
                item->from_slot = board_pos;
            }
        } else {
            PerBoardPt->held_equipped_slot = -1;

            if (slot == PerBoardStatusPt->equipped_weapon_slot[chara]) {
                PerBoardPt->held_equipped_slot = slot;
                PerBoardStatusPt->equipped_weapon_slot[chara] = -1;
            }

            item->from_page = BOARD_PAGE_WEAPON;
            item->from_slot = board_pos;
            item->item_no = weapon->item_no;
            memcpy(&PerBoardPt->weapon, weapon, sizeof(WEAPON_HAVE));
            InitHaveWep(weapon);
            weapon->item_no = 0;

            if (weapon->item_no >= ITEM_WEAPON_START || item->item_no >= ITEM_WEAPON_START) {
                result = 1;
            }
        }
    }

    return result;
}

/**
 * Swaps an attachment between the held item and the personal board.
 */
int PersonalBoardAttachPush(IHAVEITEM *item, int board_pos) {
    int enabled = false;
    int kind = WhatIsKindofItem(item->item_no);

    if (kind == BOARD_PAGE_ATTACH || kind < 0) {
        item->from_page = BOARD_PAGE_ATTACH;
        item->from_slot = board_pos;
        CUserStatus    *status = PerBoardStatusPt;
        DNG_CONSUMABLE *consumables = status->consumable_items;
        ATTACH_LIST    *slot = (ATTACH_LIST *) &consumables[board_pos];
        s16             item_no = item->item_no;
        s16             slot_item_no = slot->item_no;

        MenuDataSwap(slot, &PerBoardPt->held_attach);
        item->item_no = slot_item_no;
        slot->item_no = item_no;

        if (item->item_no >= ITEM_ATTACH_START || slot->item_no >= ITEM_ATTACH_START) {
            enabled = true;
        }
    }

    return enabled;
}

int PersonalBoardItemGetorSwap(int board_pos) {
    int        result = 0;
    IHAVEITEM *item = &PerBoardPt->held_item;

    switch (PerBoardPt->page) {
        case BOARD_PAGE_ITEM:
            result = PersonalBoardItemPush(item, board_pos);
            break;
        case BOARD_PAGE_WEAPON:
            result = PersonalBoardWeaponPush(item, board_pos);
            break;
        case BOARD_PAGE_ATTACH:
            result = PersonalBoardAttachPush(item, board_pos);
            break;
    }

    return result;
}

void PersonalBoardItemCancel() {
    IHAVEITEM *have = &PerBoardPt->held_item;
    int        cell = have->from_slot;

    switch (have->from_page) {
        case BOARD_PAGE_ITEM: {
            s16 *item = &PerBoardPt->item_pack->item[cell];
            s16 *volume = &PerBoardPt->item_pack->item_vol[cell];
            MenuDataSwap(item, &have->item_no);
            MenuDataSwap(volume, &have->volume);
        } break;
        case BOARD_PAGE_WEAPON: {
            int chara;
            int slot;
            int held;
            held = have->item_no;

            if (held >= ITEM_WEAPON_START) {
                chara = cell / 10;
                slot = cell % 10;
                WEAPON_HAVE *weapons = (WEAPON_HAVE *) ((char *) PerBoardStatusPt + chara * sizeof(PerBoardStatusPt->chara_weapons[0]) + 0x450C);
                WEAPON_HAVE *weapon = &weapons[slot];
                int          placed = weapon->item_no;
                MenuDataSwap(&PerBoardPt->weapon, weapon);
                have->item_no = placed;
                weapon->item_no = held;

                if (PerBoardPt->held_equipped_slot >= 0) {
                    PerBoardStatusPt->equipped_weapon_slot[chara] = PerBoardPt->held_equipped_slot;
                    PerBoardPt->held_equipped_slot = -1;
                } else if (slot == PerBoardStatusPt->equipped_weapon_slot[chara]) {
                    PerBoardPt->held_equipped_slot = slot;
                    PerBoardStatusPt->equipped_weapon_slot[chara] = -1;
                }
            }

            break;
        }
        case BOARD_PAGE_ATTACH: {
            DNG_CONSUMABLE *items = PerBoardStatusPt->consumable_items;
            DNG_CONSUMABLE *item = &items[cell];
            int             held = have->item_no;
            int             placed = item->id;
            MenuDataSwap((ATTACH_LIST *) item, &PerBoardPt->held_attach);
            have->item_no = placed;
            item->id = held;
            break;
        }
    }

    if (have->item_no < ITEM_ATTACH_START) {
        InitHaveData(have);
        InitHaveWep(&PerBoardPt->weapon);
        PerBoardPt->held_equipped_slot = -1;
    }
}

int PersonalRetMax(int board_mode) {
    int max = 0;

    switch (board_mode) {
        case BOARD_PAGE_ITEM:
            max = PerBoardPt->item_pack->num;
            break;
        case BOARD_PAGE_WEAPON:
            max = 60;
            break;
        case BOARD_PAGE_ATTACH:
            max = 40;
            break;
        case BOARD_PAGE_ATLA_VILLAGE:
        case BOARD_PAGE_ATLA_VILLAGE + 1:
        case BOARD_PAGE_ATLA_VILLAGE + 2:
        case BOARD_PAGE_ATLA_VILLAGE + 3:
        case BOARD_PAGE_ATLA_VILLAGE + 4:
        case BOARD_PAGE_ATLA_VILLAGE + 5:
            max = 120;
            break;
    }

    return max;
}

void DrawPersonalBoard(int x, int y, int board_mode, int alpha, int unused) {
    int max = PersonalRetMax(board_mode);
    int top = y + 9;
    int bottom = top + 0xA0;
    int left = x + 0x13;
    int row_y = y - 0x11;
    int mark;

    row_y = row_y + 0x17 - PerBoardPt->top_row * 40;
    PerBoardPt->y += (row_y - PerBoardPt->y) / 4.0f;
    row_y = PerBoardPt->y;

    switch (PerBoardPt->menu_kind) {
        case PERSONAL_BOARD_ATLA:
        case PERSONAL_BOARD_ITEM:
            MenuTextureReload(PerBoardTex->block);
            break;
    }

    mark = 0;

    switch (board_mode) {
        case BOARD_PAGE_WEAPON:
            mark = 2;
            break;
    }

    DrawPerBoardDraw(mark, max, left, row_y, top, bottom, PerBoardTex, alpha);

    switch (PerBoardPt->menu_kind) {
        case PERSONAL_BOARD_ATLA:
            MenuTextureReload(PerBoardTex->block);
            break;
        case PERSONAL_BOARD_ITEM:
            MenuTextureReload(ItemMenuWeaponIconReadBlock);
            break;
    }

    CommonIconDraw(board_mode, max, left + 2, row_y + 6, top, bottom, alpha);
    MenuTextureReload(PerBoardTex->block);
    PersonalBoardOptionDraw(board_mode, max, x, y, PerBoardTex, alpha);
}

/**
 * Draws the equipped-weapon marker on a personal-board entry.
 *
 * @mangled DrawNowEquipWeaponMark__Fiiiii
 * @address 0x0022F9D0
 * @size 0x100
 */
static void DrawNowEquipWeaponMark(int x, int y, int top, int bottom, int alpha) {
    int position = y;
    int v = 0xC0;
    int length = 0x28;

    MenuTextureClip(position, v, length, top, bottom);

    if (position + length < top || bottom <= position) {
        return;
    }

    MenuTextureReload(PerBoardTex->block);
    DrawMenu2DSprite(PerBoardTex, CRect_i_(x, position, 0x28, length), CRect_i_(0x114, v, 0x28, length), alpha);
}

void CommonIconDraw(int page, int count, int x, int y, int top, int bottom, int alpha) {
    int       value;
    int       draw_x;
    int       weapon_x = x + 4;
    int       draw_y = y;
    CTexture *icon;

    switch (page) {
        case BOARD_PAGE_ATTACH: {
            if (PerBoardPt->menu_kind == PERSONAL_BOARD_WEAPON_ATTACH) {
                MenuTextureReload(MenuShadowReadBlock);
            } else {
                icon = TexManager.GetTexture("wepicon", -1);

                if (icon != NULL) {
                    MenuTextureReload(icon->block);
                    WepIcon = icon;
                }
            }

            draw_x = x + 2;
            ATTACH_LIST *attach = (ATTACH_LIST *) PerBoardPt->consumables;

            for (int i = 0; i < count; i++) {
                int id = attach[i].item_no;

                if (id >= ITEM_ATTACH_STAT_START && id < ITEM_ATTACH_GEM_START) {
                    value = attach[i].status[id - ITEM_ATTACH_STAT_START];
                }

                if (id == ITEM_ATTACH_SYNTHESIS_SPHERE) {
                    value = attach[i].sphere_weapon_no;
                }

                DrawIconParts(id, draw_x, draw_y, top, bottom, alpha, value);
                draw_x += 0x28;

                if (i % 5 == 4) {
                    draw_x = x + 2;
                    draw_y += 0x28;
                }
            }

            break;
        }
        case BOARD_PAGE_ITEM: {
            draw_x = x;
            int        quick;
            int        slot;
            ITEM_PACK *pack = PerBoardPt->item_pack;
            s16       *items = pack->item;
            quick = 0;

            for (slot = 0; slot < 3; slot++) {
                quick += pack->quick_item_qty[slot];
            }

            for (int i = 0; i < count; i++) {
                DrawIconParts(items[i], draw_x, draw_y, top, bottom, alpha, 0);

                if (MenuTrushMark[i] != 0) {
                    DrawDontSetItemMark(draw_x, draw_y, top, bottom, alpha);
                }

                draw_x += 0x28;

                if (i % 5 == 4) {
                    draw_x = x;
                    draw_y += 0x28;
                }
            }

            break;
        }
        case BOARD_PAGE_WEAPON: {
            int          party;
            int          item_no;
            WEAPON_HAVE *weapons;
            int          chara = 0;
            weapons = PerBoardStatusPt->chara_weapons[0];
            icon = TexManager.GetTexture("wepicon", -1);

            if (icon != NULL) {
                MenuTextureReload(icon->block);
                WepIcon = icon;
            }

            draw_x = weapon_x;
            party = PerBoardStatusPt->party_size;

            for (int i = 0; i < count; i++) {
                int slot = i % 10;
                item_no = weapons[slot].item_no;
                int equipped = PerBoardStatusPt->equipped_weapon_slot[i / 10];

                if (party >= i / 10 + 1 && equipped >= 0 && slot == equipped) {
                    DrawNowEquipWeaponMark(draw_x - 2, draw_y - 4, top + 1, bottom, alpha);

                    if (icon != NULL) {
                        MenuTextureReload(icon->block);
                    }
                }

                DrawIconParts(item_no, draw_x, draw_y, top, bottom, alpha, 0);
                draw_x += 0x28;

                if (i % 5 == 4) {
                    draw_x = weapon_x;
                    draw_y += 0x28;

                    if (i != 0 && slot == 9) {
                        chara++;
                        CUserStatus *status = PerBoardStatusPt;
                        weapons = status->chara_weapons[chara];
                    }
                }
            }

            break;
        }
        case BOARD_PAGE_ATLA_VILLAGE:
        case BOARD_PAGE_ATLA_VILLAGE + 1:
        case BOARD_PAGE_ATLA_VILLAGE + 2:
        case BOARD_PAGE_ATLA_VILLAGE + 3:
        case BOARD_PAGE_ATLA_VILLAGE + 4:
        case BOARD_PAGE_ATLA_VILLAGE + 5: {
            int left = weapon_x - 2;
            draw_x = left;

            for (int i = 0; i < 120; i++) {
                DrawAtoraParts(draw_x, draw_y, PerBoardPt->atla_elements[i], top, bottom - 4, alpha);
                draw_x += 0x28;

                if (i % 5 == 4) {
                    draw_x = left;
                    draw_y += 0x28;
                }
            }

            break;
        }
    }
}

void PersonalBoardDrawWaku(int x, int y, CTexture *texture, int alpha) {
    DrawMenu2DSprite(texture, CRect_i_(x, y + 1, 0x14, 0xBF), CRect_i_(0, 0, 0x14, 0xC0), alpha);
    DrawMenu2DSprite(texture, CRect_i_(x + 0x14, y + 1, 0xC8, 0x13), CRect_i_(0x14, 0, 0xC8, 0x14), alpha);
    DrawMenu2DSprite(texture, CRect_i_(x + 0xDC, y + 1, 0x24, 0xBF), CRect_i_(0xDC, 0, 0x24, 0xC0), alpha);
    DrawMenu2DSprite(texture, CRect_i_(x + 0x14, y + 0xA1, 0xC8, 0x1F), CRect_i_(0x14, 0xA0, 0xC8, 0x20), alpha);
}

void PersonalBoardOptionDraw(int board_mode, int count, int x, int y, CTexture *texture, int alpha) {
    PersonalBoardTagDraw(board_mode, x, y, texture, 0, alpha);
    PersonalBoardDrawWaku(x, y, texture, alpha);
    PersonalBoardScrlBarDraw(count, x, y, PerBoardPt->scroll, PerBoardPt->top_row, texture, alpha);
    PersonalBoardMaxDraw(count, x, y, texture, alpha);
}

void PersonalBoardTagDraw(int tag, int x, int y, CTexture *texture, int shift, int alpha) {
    int bright = 0x80;
    int top;
    int row;

    switch (PerBoardPt->menu_kind) {
        case PERSONAL_BOARD_WEAPON_ATTACH:
            bright = 0x40;
        case PERSONAL_BOARD_ITEM:
            for (int kind = 0, i = 0; i < 3; i++, kind++) {
                if (tag != kind) {
                    top = y - 0x30;
                    row = kind * 0x30;
                    set2DSprite(GetVif1Packet(), texture, CRect_i_(x, top + 2, 0x100, 0x2F), CRect_i_(0x100, row, 0x100, 0x30), bright, bright, bright, alpha);
                }
            }

            top = y - 0x30;
            row = tag * 0x30;
            DrawMenu2DSprite(texture, CRect_i_(x, top + 2, 0x100, 0x2F), CRect_i_(0x100, row, 0x100, 0x30), alpha);
            x = x + 0x12 + tag * 0x44;
            top = y - 0x4C;
            {
                s8 widths[7][3] = {
                    {0x34, 0x45, 0x4E},
                    {0x34, 0x45, 0x4E},
                    {0x34, 0x45, 0x4E},
                    {0x34, 0x45, 0x4E},
                    {0x34, 0x45, 0x4E},
                    {0x34, 0x45, 0x4E},
                    {0x34, 0x45, 0x4E},
                };
                s8 sources[7][3] = {
                    {0x00, 0x34, 0x79},
                    {0x00, 0x34, 0x79},
                    {0x00, 0x34, 0x79},
                    {0x00, 0x34, 0x79},
                    {0x00, 0x34, 0x79},
                    {0x00, 0x34, 0x79},
                    {0x00, 0x34, 0x79},
                };
                s8 offsets[7][3] = {
                    {2, -8, -16},
                    {2, -8, -16},
                    {2, -8, -16},
                    {2, -8, -16},
                    {2, -8, -16},
                    {2, -8, -16},
                    {2, -8, -16},
                };
                int lang = GetMenuLangFlag();
                int width = widths[lang][tag];
                int source = sources[lang][tag];

                x += offsets[lang][tag];
                x += shift;

                if (shift == 0) {
                    DrawMenu2DSprite(texture, CRect_i_(x, top + 1, width, 0x1B), CRect_i_(source, 0xC0, width, 0x1C), alpha);
                }
            }
            break;
        case PERSONAL_BOARD_ATLA:
            if (VillageBar != NULL) {
                MenuTextureReload(VillageBar->block);
                int last = 0;
                int village = 2;
                int max_village = GetAtoraMaxVillage();
                int step = 1;
                int count = 0;

                top = y - 0x30;

                while (step != 0) {
                    village += step;

                    if (max_village < village) {
                        village = max_village;
                    }

                    if (village == tag) {
                        if (step == 1 && village == max_village) {
                            last = 1;
                        } else if (step == -1) {
                            last = 1;
                        } else if (step == 1) {
                            step = -1;
                            village = max_village;
                        }
                    }

                    int index = village - 3;
                    int left = x + index * 0x1A;

                    if (last) {
                        DrawMenu2DSprite(VillageBar, CRect_i_(x, y - 0x12, 0x100, 0x13), CRect_i_(0, 0x90, 0x100, 0x14), alpha);
                        bright = 0x80;
                    }

                    int column = (index % 2) * 0x60;
                    row = (index >> 1) * 0x30;
                    set2DSprite(GetVif1Packet(), VillageBar, CRect_i_(left, top + 2, 0x60, 0x2F), CRect_i_(column, row, 0x60, 0x30), bright, bright, bright, alpha);

                    if (last) {
                        break;
                    }

                    if (++count >= 7) {
                        break;
                    }
                }

                top = y - 0x5A;
                row = (tag - 3) * 0x28;
                DrawMenu2DSprite(VillageName, CRect_i_(x, top + 1, 0x100, 0x27), CRect_i_(0, row, 0x100, 0x28), alpha);
            }

            break;
    }
}

void PersonalBoardScrlBarDraw(int count, int x, int y, float &scroll, unsigned char top_row, CTexture *texture, int alpha) {
    int bar_x = x + 0xE8;
    int rows = count / 5;

    if (rows <= 0) {
        rows = 1;
    }

    int length = (int) (456.0f / rows - 8.0f);

    if (114.0f < (float) length) {
        length = 0x69;
    }

    CRect_i_ source(0xAE, 0x14, 8, 4);
    float    bar_y = (float) (y + 0x16) + 114.0f * top_row / rows;
    bar_y = scroll + (bar_y - scroll) / 4.0f;
    scroll = bar_y;
    DrawMenu2DSprite(texture, CRect_i_(bar_x, (int) bar_y, 8, 4), source, alpha);
    source.y += 4;
    DrawMenu2DSprite(texture, CRect_i_(bar_x, (int) (4.0f + bar_y), 8, length + 1), source, alpha);
    source.y += 4;
    DrawMenu2DSprite(texture, CRect_i_(bar_x, (int) (4.0f + bar_y + length), 8, 4), source, alpha);
}

void PersonalBoardMaxDraw(int num, int x, int y, CTexture *texture, int alpha) {
    int left = x + 0xD2;
    int top = y - 0x28;

    DrawMenu2DSprite(texture, CRect_i_(left, top + 1, 0x30, 0x2F), CRect_i_(0x100, 0x90, 0x30, 0x30), alpha);
    RECT digits = {0x90, 0xDC, 12, 12};
    left = x + 0xF7;
    top = y - 0xE;

    if (PerBoardPt->menu_kind == PERSONAL_BOARD_ATLA) {
        left += 2;
    }

    DrawMenuNumber(num, left, top, texture, digits, 1, alpha);
    left = x + 0xF4;
    top = y - 0x22;

    int max = 0;
    int full = 0;
    int item;

    switch (PerBoardPt->page) {
        case BOARD_PAGE_ITEM:
        case BOARD_PAGE_WEAPON:
        case BOARD_PAGE_ATTACH:
            max = GetNowModeMaxNum(PerBoardPt->page, &full);
            item = PerBoardPt->held_item.item_no;

            if (item >= ITEM_ATTACH_START) {
                int kind = WhatIsKindofItem(item);

                if (kind == PerBoardPt->page) {
                    max++;
                }
            }

            if (num < max) {
                full = 1;
            }

            break;
        case BOARD_PAGE_ATLA_VILLAGE:
        case BOARD_PAGE_ATLA_VILLAGE + 1:
        case BOARD_PAGE_ATLA_VILLAGE + 2:
        case BOARD_PAGE_ATLA_VILLAGE + 3:
        case BOARD_PAGE_ATLA_VILLAGE + 4:
        case BOARD_PAGE_ATLA_VILLAGE + 5:
            for (int i = 0; i < num; i++) {
                if (PerBoardPt->atla_elements[i] >= 0) {
                    max++;
                }
            }

            if (0 <= PerBoardPt->held_item.item_no) {
                max++;
            }

            break;
    }

    if (full) {
        digits.y += digits.height;
    }

    DrawMenuNumber(max, left, top, texture, digits, 1, alpha);
}

/**
 * Draws the base layers of a personal inventory board.
 *
 * @mangled DrawPersonalBoardBase__FiiiiiP8CTexturei
 * @address 0x00230C00
 * @size 0x1C0
 */
static void DrawPersonalBoardBase(int x, int y, int top, int bottom, int count, CTexture *texture, int alpha) {
    int    v;
    int    length;
    spRGBA upper;
    spRGBA lower;
    int    step;
    int    i;

    if (y < top - 39 || y > bottom - 1) {
        return;
    }

    v = 0x14;
    length = 0x28;
    MenuTextureClip(y, v, length, top, bottom);
    step = 9;
    upper.r = upper.g = upper.b = 0x80;
    lower.r = lower.g = lower.b = 0x80 - step;
    lower.a = alpha;
    upper.a = alpha;

    for (i = 0; i < count; i++) {
        set2DSprite(GetVif1Packet(), texture, CRect_i_(x, y, 0x28, length), CRect_i_(0x20, v, 0x28, length), &upper, &lower, &upper, &lower, 1);
        upper.r = upper.g = upper.b = lower.r;
        lower.r = lower.g = lower.b = lower.r - step;
        step--;
        x += 0x28;
    }
}

void DrawPerBoardDraw(int mark, int count, int x, int y, int top, int bottom, CTexture *texture, int alpha) {
    int row_y = y;
    int board_x = x;

    for (int row = 0; row < 26; row++) {
        DrawPersonalBoardBase(board_x, row_y, top, bottom, 5, texture, alpha);
        int draw_mark = 0;

        switch (mark) {
            case 0:
                break;
            case 1:
                if (row == 2) {
                    draw_mark = 1;
                }

                break;
            case 2:
                if (row % 2 == 0 && row != 0) {
                    draw_mark = 1;
                }

                break;
        }

        if (draw_mark != 0) {
            int mark_x = x - 4;
            int mark_y = row_y - 8;
            int v = 0x3C;
            int height = 0x10;

            if (top <= mark_y + 0x10 && mark_y < bottom) {
                MenuTextureClip(mark_y, v, height, top, bottom);
                int    step = 9;
                spRGBA left;
                spRGBA right;
                left.r = left.g = left.b = 0x80;
                right.r = right.g = right.b = 0x80 - step;
                right.a = alpha;
                left.a = alpha;

                for (int i = 0; i < 5; i++) {
                    set2DSprite(GetVif1Packet(), texture, CRect_i_(mark_x, mark_y, 0x28, height), CRect_i_(0x20, v, 0x28, height), &left, &right, &left, &right, 1);
                    left.r = left.g = left.b = right.r;
                    right.r = right.g = right.b = right.r - step;
                    step--;
                    mark_x += 0x28;
                }
            }
        }

        row_y += 0x28;

        if (bottom < row_y) {
            break;
        }
    }
}

/* The trash can's animation fields, which CommonTrushDraw reaches as one record. */
struct PERSONAL_BOARD_TRASH {
    s16 anim; /**< One while the trash can plays its throw animation. */
    u8  unk_02[2];
    s32 frame; /**< Frame of the trash can's throw animation. */
};

void CommonTrushDraw(int x, int y, int alpha) {
    PERSONAL_BOARD       *board = PerBoardPt;
    PERSONAL_BOARD_TRASH *trash = (PERSONAL_BOARD_TRASH *) &board->trash_anim;
    CRect_i_              source(0x190, 0x90, 0x30, 0x30);
    CRect_i_              dest(x, y + 1, 0x30, 0x2F);

    switch (trash->anim) {
        case 1: {
            int frame = trash->frame;

            if (frame <= 3 || frame > 12) {
                source.x -= 0x30;
            } else if (frame > 6 && frame <= 9) {
                source.x += 0x30;
            }

            DrawMenu2DSprite(PerBoardTex, dest, source, alpha);
            trash->frame++;

            if (frame > 15) {
                trash->anim = 0;
                trash->frame = 0;
            }

            break;
        }
        default:
            if (board->cursor_area == PERSONAL_BOARD_AREA_TRASH) {
                source.x = 0x130;
            }

            DrawMenu2DSprite(PerBoardTex, dest, source, alpha);
            break;
    }
}

int IsEnableTrushThrow(int item_no) {
    int enable = false;

    if (item_no >= ITEM_WEAPON_START) {
        int          owner = WhoIsWeaponEquip(item_no);
        CUserStatus *status = PerBoardStatusPt;
        WEAPON_HAVE *weapons = status->chara_weapons[owner];
        int          default_no = GetDefaultWeaponNo(owner);

        for (int i = 0; i < 11; i++) {
            int weapon_no = weapons[i].item_no;

            if (weapon_no == default_no || weapon_no == default_no + 1) {
                enable = true;
                break;
            }
        }

        if (PerBoardStatusPt->party_size - 1 < owner) {
            enable = true;
        }

        if (item_no == ITEM_WEAPON_SERPENT_SWORD) {
            int flag = GetMenuHebikiriFlag();

            if (flag == 0) {
                enable = flag;
            }
        }

        if (PerBoardPt->held_equipped_slot >= 0) {
            enable = false;
        }
    }

    if (item_no >= ITEM_ATTACH_START && item_no < ITEM_WEAPON_START) {
        COM_ITEM_INFO *info = GetCommonItemInfo(item_no);

        if (info != NULL) {
            switch (info->kind) {
                case ITEMKIND_ATTACH:
                    if (GetAttachData(item_no) != NULL) {
                        enable = true;
                    }

                    break;
                case ITEMKIND_ITEM: {
                    ITEM_DATA *data = GetItemData(item_no);

                    if (data != NULL) {
                        if (data->kind_flags & 0x10) {
                            enable = false;
                        } else {
                            enable = true;
                        }
                    }

                    break;
                }
            }
        }
    }

    return enable;
}

void CommonMoneyBoardDraw(int x, int y, int money, int alpha) {
    int top = y + 2;

    DrawMenu2DSprite(PerBoardTex, CRect_i_(x, top, 0x1A, 0x1B), CRect_i_(0x160, 0xC0, 0x1A, 0x1C), alpha);
    DrawMenu2DSprite(PerBoardTex, CRect_i_(x + 0x1A, top, 0x2A, 0x1B), CRect_i_(0x17A, 0xC0, 0x20, 0x1C), alpha);
    DrawMenu2DSprite(PerBoardTex, CRect_i_(x + 0x44, top, 0x1A, 0x1B), CRect_i_(0x19A, 0xC0, 0x1A, 0x1C), alpha);
    RECT digits = {0, 0xDC, 12, 12};
    DrawMenuNumber(money, x + 0x55, y + 8, PerBoardTex, digits, 0, alpha);
}

int SearchBoardNowPosItemExist(int page, int cell) {
    int item_no = -1;

    switch (page) {
        case BOARD_PAGE_ITEM: {
            ITEM_PACK *pack = &PerBoardStatusPt->item_pack;

            if (pack != NULL) {
                item_no = pack->item[cell];
            }

            break;
        }
        case BOARD_PAGE_WEAPON: {
            int          chara = cell / 10;
            int          slot = cell % 10;
            CUserStatus *status = PerBoardStatusPt;
            WEAPON_HAVE *weapons = status->chara_weapons[chara];
            WEAPON_HAVE *weapon = &weapons[slot];

            if (weapon != NULL) {
                item_no = weapon->item_no;
            }

            break;
        }
        case BOARD_PAGE_ATTACH: {
            DNG_CONSUMABLE *items = PerBoardStatusPt->consumable_items;

            if (items != NULL) {
                item_no = items[cell].id;
            }

            break;
        }
    }

    return item_no;
}

int GetBoardSpace(int item_no, int *page) {
    int space = -1;

    if (item_no < ITEM_ATTACH_START) {
        return -1;
    }

    *page = WhatIsKindofItem(item_no);
    int max = PersonalRetMax(*page);

    switch (*page) {
        case BOARD_PAGE_ITEM: {
            int        quick;
            int        i;
            ITEM_PACK *pack = &PerBoardStatusPt->item_pack;
            quick = 0;

            for (i = 0; i < 3; i++) {
                quick += pack->quick_item_qty[i];
            }

            for (i = 0; i < max - quick; i++) {
                if (pack->item[i] < ITEM_DUNGEON_START && MenuTrushMark[i] == 0) {
                    space = i;
                    break;
                }
            }

            break;
        }
        case BOARD_PAGE_WEAPON: {
            int          owner = WhoIsWeaponEquip(item_no);
            int          i;
            CUserStatus *status = PerBoardStatusPt;
            WEAPON_HAVE *weapons = status->chara_weapons[owner];

            for (i = 0; i < 10; i++) {
                if (weapons[i].item_no < ITEM_WEAPON_START) {
                    space = i + owner * 10;
                    break;
                }
            }

            break;
        }
        case BOARD_PAGE_ATTACH: {
            int             i;
            DNG_CONSUMABLE *items = PerBoardStatusPt->consumable_items;

            for (i = 0; i < max; i++) {

                if (items[i].id < ITEM_ATTACH_START) {
                    space = i;
                    break;
                }
            }

            break;
        }
    }

    return space;
}

void SwapItem(ITEM_PACK *items, int first_pos, int second_pos) {
    s16 item = items->item[first_pos];
    s16 vol = items->item_vol[first_pos];

    items->item[first_pos] = items->item[second_pos];
    items->item_vol[first_pos] = items->item_vol[second_pos];
    items->item[second_pos] = item;
    items->item_vol[second_pos] = vol;
}

/** Rank of each item kind in the item board sort, rebuilt before each sort pass. */
static int sort_table[9] = {9, 0, 1, 2, 3, 4, 5, 6, 7};

int CompItem(int first_item_no, int second_item_no) {
    ITEM_DATA *first = GetItemData(first_item_no);
    ITEM_DATA *second = GetItemData(second_item_no);
    int        first_rank = 0;
    int        second_rank = 0;

    if (first != NULL) {
        first_rank = sort_table[first->sort_key];
    }

    if (second != NULL) {
        second_rank = sort_table[second->sort_key];
    }

    if (first_item_no < ITEM_DUNGEON_START) {
        first_rank = 9;
    }

    if (second_item_no < ITEM_DUNGEON_START) {
        second_rank = 9;
    }

    if (first_rank > second_rank) {
        return 1;
    }

    if (first_rank < second_rank) {
        return -1;
    }

    if (first_item_no > second_item_no) {
        return 1;
    }

    if (first_item_no < second_item_no) {
        return -1;
    }

    return 0;
}

/** Item kind the item board sort places first. */
static int sort_top_type = ITEM_SORT_FOOD;

int SeitonItemBoardSub(ITEM_PACK *items) {
    int i;
    int j;
    int type = sort_top_type;
    int swapped;

    for (i = 0; i < ITEM_SORT_COUNT; i++) {
        sort_table[type] = i;
        type++;

        if (type >= ITEM_SORT_COUNT) {
            type = ITEM_SORT_NONE;
        }
    }

    sort_table[ITEM_SORT_NONE] = 9;
    swapped = 0;

    for (i = 0; i < items->num - 1; i++) {
        for (j = i + 1; j < items->num; j++) {
            if (CompItem(items->item[i], items->item[j]) > 0) {
                SwapItem(items, i, j);
                swapped = 1;
            }
        }
    }

    return swapped;
}

void SeitonItemBoard(ITEM_PACK *items) {
    if (items != NULL) {
        for (int i = 0; i < ITEM_SORT_COUNT; i++) {
            if (SeitonItemBoardSub(items) != 0) {
                break;
            }

            sort_top_type++;

            if (sort_top_type >= ITEM_SORT_COUNT) {
                sort_top_type = ITEM_SORT_FOOD;
            }
        }
    }
}

int GetAttachKind(int item_no) {
    if ((item_no >= ITEM_ATTACH_START) && (item_no < ITEM_ATTACH_ATTACK)) {
        return ATTACHKIND_ELEMENT;
    }

    if ((item_no >= ITEM_ATTACH_ATTACK) && (item_no < ITEM_ATTACH_AMETHYST)) {
        return ATTACHKIND_STAT;
    }

    if ((item_no >= ITEM_ATTACH_AMETHYST) && (item_no < ITEM_ATTACH_GEM_END)) {
        return ATTACHKIND_GEM;
    }

    if ((item_no >= ITEM_ATTACH_DINOSLAYER) && (item_no <= ITEM_ATTACH_SLAYER_END)) {
        return ATTACHKIND_SLAYER;
    }

    return ATTACHKIND_OTHER;
}

/** Rank of each attachment kind in the attachment board sort, rebuilt before each sort pass. */
static int asort_table[5] = {5, 1, 2, 3, 4};

int CompAttach(ATTACH_LIST *first, ATTACH_LIST *second) {
    int first_rank = asort_table[GetAttachKind(first->item_no)];
    int second_rank = asort_table[GetAttachKind(second->item_no)];

    if (first->item_no < ITEM_ATTACH_START) {
        first_rank = 5;
    }

    if (second->item_no < ITEM_ATTACH_START) {
        second_rank = 5;
    }

    if (first_rank > second_rank) {
        return 1;
    }

    if (first_rank < second_rank) {
        return -1;
    }

    if (first->item_no > second->item_no) {
        return 1;
    }

    if (first->item_no < second->item_no) {
        return -1;
    }

    return 0;
}

/** Attachment kind the attachment board sort places first. */
static int asort_top_type = 1;

int SeitonAttachBoardSub(ATTACH_LIST *attachments) {
    int i;
    int j;
    int type = asort_top_type;
    int swapped;

    for (i = 0; i < 5; i++) {
        asort_table[type] = i;
        type++;

        if (type >= 5) {
            type = 0;
        }
    }

    asort_table[0] = 5;
    swapped = 0;

    for (i = 0; i < 39; i++) {
        for (j = i + 1; j < 40; j++) {
            if (CompAttach(&attachments[i], &attachments[j]) > 0) {
                MenuDataSwap(&attachments[i], &attachments[j]);
                swapped = 1;
            }
        }
    }

    return swapped;
}

int SeitonAttachBoard(ATTACH_LIST *attachments) {
    if (attachments == NULL) {
        return 0;
    }

    for (int i = 0; i < 5; i++) {
        if (SeitonAttachBoardSub(attachments) != 0) {
            break;
        }

        asort_top_type++;

        if (asort_top_type >= 5) {
            asort_top_type = 0;
        }
    }
    PORT_RETURN(1);
}

int WhatIsKindofItem(int item_no) {
    COM_ITEM_INFO *info = GetCommonItemInfo(item_no);

    if (info == NULL) {
        return BOARD_PAGE_NONE;
    }

    int board_kind[3] = {BOARD_PAGE_ATTACH, BOARD_PAGE_ITEM, BOARD_PAGE_WEAPON};
    return board_kind[info->kind];
}

int WhoIsWeaponEquip(int weapon_no) {
    COM_ITEM_INFO *info;
    WEAPON_DATA   *data;

    info = GetCommonItemInfo(weapon_no);

    if (info == NULL) {
        return -1;
    }

    if (info->kind != ITEMKIND_WEAPON) {
        return -1;
    }

    data = GetWeaponDataInfo((s32) info->index);

    if (data != NULL) {
        return (s8) data->owner;
    }

    return -1;
}

int GetWeaponHoleNum(int weapon_no) {
    COM_ITEM_INFO *info = GetCommonItemInfo(weapon_no);

    if (info == NULL) {
        return 0;
    }

    if (info->kind != ITEMKIND_WEAPON) {
        return 0;
    }

    WEAPON_DATA *data = GetWeaponData(weapon_no);

    if (data == NULL) {
        return 0;
    }

    int count = 0;

    for (int i = 0; i <= 5; i++) {
        if (data->hole[i] > 0) {
            count++;
        }
    }

    return count;
}

int GetNowWeaponAttachNum(WEAPON_HAVE *weapon) {
    int count = 0;

    if (weapon == NULL) {
        return 0;
    }

    WEAPON_DATA *data = GetWeaponData(weapon->item_no);

    if (data != NULL) {
        for (int i = 0; i < 6; i++) {
            if (data->hole[i] > 0 && weapon->attach[i].item_no >= ITEM_ATTACH_START) {
                count++;
            }
        }
    }

    return count;
}

int GetWeaponMaxExp(WEAPON_HAVE *weapon) {
    if (weapon == NULL) {
        return 0;
    }

    WEAPON_DATA *data = GetWeaponData(weapon->item_no);

    if (data == NULL) {
        return 1;
    }

    int experience = *(s8 *) &data->exp_base;

    for (int i = 0; i < weapon->level; i++) {
        experience += data->exp_per_level;
    }

    if (experience > 999) {
        experience = 999;
    }

    if (experience <= 0) {
        experience = 99;
    }

    return experience;
}

int GetNowItemNum(short item_no, ITEM_PACK *items) {
    int count = 0;
    int i;
    int j;

    for (i = 0; i < items->num; i++) {
        if (items->item[i] == item_no) {
            count++;
        }
    }

    for (j = 0; j < 3; j++) {
        if (items->quick_item_slot[j] == item_no) {
            count += items->quick_item_qty[j];
        }
    }

    return count;
}

void DeleteItemAfterUseItem(short item_no, ITEM_PACK *items) {
    int deleted = 0;
    int i;

    for (i = 0; i < items->num; i++) {
        if (items->item[i] == item_no) {
            items->item[i] = -1;
            deleted = 1;
            break;
        }
    }

    if (deleted == 0) {
        for (i = 0; i < 3; i++) {
            if (items->quick_item_slot[i] == item_no) {
                items->quick_item_qty[i]--;

                if (items->quick_item_qty[i] <= 0) {
                    items->quick_item_slot[i] = -1;
                }

                break;
            }
        }
    }
}

int GetNowModeMaxNum(int page, int *overflow) {
    CUserStatus *status = (CUserStatus *) SaveData->GetDngStatus();
    int          count = 0;

    switch (page) {
        case BOARD_PAGE_ITEM: {
            int        i;
            int        j;
            ITEM_PACK *pack = &status->item_pack;
            int        num;

            for (i = 0; i < (num = pack->num) + 3; i++) {
                if (pack->item[i] >= ITEM_DUNGEON_START) {
                    count++;
                }
            }

            for (j = 0; j < 3; j++) {
                if (pack->quick_item_slot[j] >= ITEM_DUNGEON_START) {
                    count += pack->quick_item_qty[j];
                }
            }

            if (overflow != NULL && num < count) {
                *overflow = true;
            }

            break;
        }
        case BOARD_PAGE_WEAPON:
            for (int chara = 0; chara < 6; chara++) {
                int          i;
                WEAPON_HAVE *weapons = status->chara_weapons[chara];

                for (i = 0; i < 11; i++) {
                    if (weapons[i].item_no >= ITEM_WEAPON_START) {
                        count++;

                        if (i == 10 && overflow != NULL) {
                            *overflow = chara + 1;
                        }
                    }
                }
            }

            break;
        case BOARD_PAGE_ATTACH:
            int             i;
            DNG_CONSUMABLE *items = status->consumable_items;

            for (i = 0; i < 43; i++) {
                if (items[i].id >= ITEM_ATTACH_START) {
                    count++;
                }
            }

            if (overflow != NULL && count > 40) {

                *overflow = true;
            }

            break;
    }

    return count;
}

void WepDataListToHaveCopy(int weapon_no, WEAPON_HAVE *weapon) {
    WEAPON_DATA *data;
    int          best;
    int          best_value;
    int          i;
    int          j;

    if (weapon != NULL && weapon_no > 0) {
        if (weapon_no < ITEM_ATTACH_START) {
            weapon_no += 0x100;
        }

        data = GetWeaponData(weapon_no);
        memset(weapon, 0, sizeof(WEAPON_HAVE));
        weapon->item_no = weapon_no;
        weapon->attack = data->attack;
        weapon->endurance = data->endurance;
        weapon->speed = data->speed;
        weapon->magic = data->magic;
        weapon->durability = data->durability;
        weapon->durability_f = weapon->durability;
        best = 5;
        best_value = 0;

        for (i = 0; i < 5; i++) {
            weapon->elem[i] = data->elem[i];

            if (weapon->elem[i] > best_value) {
                best_value = weapon->elem[i];
                best = i;
            }
        }

        weapon->best_elem = best;

        for (j = 0; j < 10; j++) {
            weapon->vs_monster[j] = data->vs_monster[j];
        }

        weapon->flags = data->flags;

        if (IsDefaultWeapon(weapon->item_no) < 0) {
            int chance = rand() % 1000;

            if (chance < 1000) {
                if (chance < 10) {
                    weapon->flags |= WEAPONFLAG_DURABLE;
                }

                if (chance > 989) {
                    weapon->flags |= WEAPONFLAG_FRAGILE;
                }
            }
        }
    }
}

void AttachDataListToHaveCopy(int attachment_no, ATTACH_LIST *attachment) {
    if ((attachment_no < ITEM_ATTACH_START) || (attachment_no >= ITEM_DUNGEON_START)) {
        return;
    }

    if (attachment != NULL) {
        memcpy(attachment, GetAttachData(attachment_no), 0x20U);
    }
}

int ItemDataToHaveCopy(int item_no) {
    int vol;

    if (item_no < ITEM_DUNGEON_START || item_no > ITEM_WEAPON_START - 1) {
        return 0;
    }

    ITEM_DATA *data = GetItemData(item_no);

    if (data == NULL) {
        return -1;
    }

    vol = data->vol;

    if (item_no == ITEM_DRAN_S_FEATHER) {
        int angle = rand() % 360;

        if (angle < 180) {
            angle += 180;
        }

        vol = angle * 60;
    }

    if (item_no >= ITEM_ANTI_FREEZE_AMULET && item_no <= ITEM_ANTIDOTE_AMULET) {
        vol = rand() % 5 + 3;
    }

    printf("vol = %d\n", vol);
    return vol;
}

void DrawFullSizePicture(CTexture *texture, int x, int y, int alpha) {
    if (texture != NULL) {
#ifdef PAL
        CRect_i_ rect(x, y, MenuDispRc.width, MenuDispRc.height);
        DrawMenu2DSprite(texture, rect, CRect_i_(0, 0, 640, 448), alpha);
#else
        DrawMenu2DSprite(texture, CRect_i_(x, y, MenuDispRc.width, MenuDispRc.height), MenuDispRc, alpha);
#endif
    }
}
