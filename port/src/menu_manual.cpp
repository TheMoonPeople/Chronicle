#include "menu_manual.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include "clsmes.hpp"
#include "dataread.hpp"
#include "dngstatusdata.hpp"
#include "draw2d_port.hpp"
#include "eastking.hpp"
#include "gamepad.hpp"
#include "gametext.hpp"
#include "gameutil.hpp"
#include "localize.hpp"
#include "memcard.hpp"
#include "menu_draw.hpp"
#include "menu_journal_chest.hpp"
#include "menuetc.hpp"
#include "mglib.hpp"
#include "rect.hpp"
#include "savedata.hpp"
#include "snd.hpp"
#include "texture.hpp"
#include "ui_framework.hpp"
#include "userstatus.hpp"

#define MANUAL_MODE_TRACKER 100

extern int       SetManualMsgBuffer();
extern void      ExitManualMenu();
extern void      DrawPrevNextCursor();
extern int       GetNowManualPartTgaNum();
extern s16       ManualImgEnter();
extern CTexture *ManualMenuTex[6];
extern ClsMes   *ManualMsg;

namespace {

UI::Cursor s_menu_cursor;

s16 Port_ManualImgLoad() {
    char path[76];
    char format[76];
    int  size = 0;
    if (ReadBGSync() != 0 || ManualMenu.images_ready != 0) {
        BreakReadBG();
    }

    GetPathReadDifferntLang(format);
    std::strcat(format, "manual/m%d.pac");
    const int image_no = ManualMenu.entry + (ManualMenu.category + 1) * 10;
    std::snprintf(path, sizeof(path), format, image_no);

    u_long128 *buffer = MenuCalcBufAlignment(ManualMenu.load_buffer);
    StartReadBG();
    LoadFileBG(path, buffer, &size);
    buffer = MenuCalcBufAlignment(buffer + size / 16 + 1);

    if ((u32)(image_no - 11) <= 1 || (u32)(image_no - 21) <= 1 || image_no == 31 || image_no == 42) {
        GetPathReadDifferntLang(format);
        std::strcat(format, "manual/m%db.pac");
        std::snprintf(path, sizeof(path), format, image_no);
        LoadFileBG(path, buffer, &size);
    }
    ReadBG();
    ManualMenu.image_offset = 0;
    ManualMenu.image_reset  = 0;
    ManualMenu.images_ready = false;
    return ManualMenu.images_ready;
}

void Port_DrawManualMsg() {
    if (ManualMenu.messages_ready == 0) return;
    MenuTextureReload(CommonMenuMes3.tex_block);

    if (ManualMenu.selection_level < 0) {
        ManualMsg->Step();
    }

    int message_no = 0;
    if (ManualMenu.selection_level == -1) {
        CommonMenuMes3.auto_pos   = MES_POS_NONE;
        message_no                = ManualMenu.category * 1000 + 1000;
        CommonMenuMes3.stay_frame = true;
        CommonMenuMes3.text_x     = 0x86;
        CommonMenuMes3.text_y     = 0xEE;
    }

    if (ManualMenu.mode == MANUAL_MODE_READING) {
        CommonMenuMes3.stay_frame = true;
        CommonMenuMes3.auto_pos   = MES_POS_BOTTOM;
        message_no                = ManualMenu.category * 1000 + 1100 + (ManualMenu.entry + 1) * 10 + ManualMenu.message_page;
        CommonMenuMes3.text_x     = 0x78;
        CommonMenuMes3.text_y     = 0x158;
    }

    if (CommonMenuMes3.mes_made != message_no) {
        CommonMenuMes3.MakeMesWin(message_no);
    }
    if (message_no <= 0) {
        CommonMenuMes3.stay_frame = false;
    }

    CommonMenuMes3.Step();
    CommonMenuMes3.DrawMesWin();
}

void DrawCategoryMenu() {
    const int     alpha   = (ManualMenu.selection_level == -1) ? 0x40 : 0x80;
    constexpr int spacing = 28;

    UI::TrackerWindow::Draw(135, 85, 220, 170, alpha, UI::BLUE_STONE_MENU);
    MenuTextureReload(0x1A);

    const std::string categories[5] = {
        LocalizeText("manual.category.georama",        "Georama"),
        LocalizeText("manual.category.dungeon",        "Dungeon"),
        LocalizeText("manual.category.weapon",         "Weapon"),
        LocalizeText("manual.category.savings",        "Savings/Other"),
        LocalizeText("manual.category.miracle_chests", "Miracle Chests")
    };

    for (int i = 0; i < 5; i++) {
        const u32 color = (i == ManualMenu.category && ManualMenu.selection_level == -2)
                          ? UI::ColorYellow
                          : UI::ColorWhite;
        UI::Label(categories[i], color).Draw(160, 100 + (i * spacing), alpha);
    }

    if (ManualMenu.selection_level == -2) {
        s_menu_cursor.SetTarget(126.0f, static_cast<float>(100 + (ManualMenu.category * spacing) - 2));
        s_menu_cursor.Draw(0x80);
    }
}

bool IsMultiPageManualEntry() {
    return (ManualMenu.category == 0 && ManualMenu.entry == 4 && ManualMenu.message_page == 1) ||
           (ManualMenu.category == 1 && ManualMenu.entry == 1 && ManualMenu.message_page == 5) ||
           (ManualMenu.category == 2 && ManualMenu.entry == 3 && ManualMenu.message_page == 1);
}

} // namespace

PORT_OVERRIDE int MenuManualKey() {
    ReadBG();
    int closed = 0;

    switch (ManualMenu.mode) {
        case MANUAL_MODE_LOADING:
            if (SetManualMsgBuffer() != 0) {
                const char* new_list      = "Georama\nDungeon\nWeapon\nSavings/Other\nMiracle Chests";
                char*       target_buffer = reinterpret_cast<char*>(ManualMenu.common_message_buffer) + 0x48;
                std::strcpy(target_buffer, new_list);
                ManualMenu.mode = MANUAL_MODE_SELECT;
            }
            break;

        case MANUAL_MODE_CLOSING:
            if (++ManualMenu.transition_frame > 32) {
                ExitManualMenu();
                closed = 1;
            }
            break;

        case MANUAL_MODE_SELECT: {
            int  minimum   = 0;
            int  maximum   = 0;
            int *selection = nullptr;

            if (ManualMenu.selection_level == -2) {
                maximum   = 4;
                selection = &ManualMenu.category;
            } else if (ManualMenu.selection_level == -1) {
                constexpr int kEntryMax[] = {4, 4, 3, 2};
                maximum   = kEntryMax[ManualMenu.category];
                selection = &ManualMenu.entry;
            }

            if (selection != nullptr) {
                if (GamePad.Down(PAD_UP)) {
                    (*selection)--;
                    ComMenuSePlay(MENU_SOUND_CURSOR);
                }
                if (GamePad.Down(PAD_DOWN)) {
                    (*selection)++;
                    ComMenuSePlay(MENU_SOUND_CURSOR);
                }
                if (*selection < minimum) *selection = maximum;
                if (*selection > maximum) *selection = minimum;

                if (ManualMenu.selection_level == -2) {
                    ManualMsg->cursor_row     = *selection;
                    CommonMenuMes3.cursor_row = -1;
                } else {
                    CommonMenuMes3.cursor_row = *selection;
                }
            } else {
                ManualMenu.selection_level = -2;
                ManualMenu.entry           = 0;
            }

            if (GamePad.Down(PAD_CROSS)) {
                if (ManualMenu.selection_level == -1) {
                    ManualMenu.selection_level++;
                    ManualMenu.mode         = MANUAL_MODE_OPENING;
                    ManualMenu.message_page = 0;
                    if (ManualMenu.category != 4) {
                        Port_ManualImgLoad();
                    }
                } else if (ManualMenu.selection_level == -2) {
                    if (ManualMenu.category == 4) {
                        ManualMenu.mode = MANUAL_MODE_TRACKER;
                        JournalChest::Open();
                    } else {
                        ManualMenu.selection_level++;
                        CommonMenuMes3.cursor_row = 0;
                    }
                    ComMenuSePlay(MENU_SOUND_CONFIRM);
                }
            } else if (GamePad.Down(PAD_CIRCLE)) {
                if (ManualMenu.selection_level < -1) {
                    ManualMenu.mode = MANUAL_MODE_CLOSING;
                    ManualMsg->Preset(MES_PRESET_SYSTEM);
                    CommonMenuMes3.SetBuff(ManualMenu.common_message_buffer);
                    for (int slot = 0; slot < 10; slot++) {
                        CommonMenuMes3.mes_no[slot] = -1;
                    }
                    CommonMenuMes3.Preset(MES_PRESET_NAME);
                    CommonMenuMes3.char_width = ManualMenu.char_width;
                    ManualMsg->SetBuff(ManualMenu.menu_message_buffer);
                    ManualMsg->Preset(MES_PRESET_SYSTEM);
                } else if (ManualMenu.selection_level == -1 && selection != nullptr) {
                    ManualMenu.selection_level--;
                    *selection = 0;
                }
                ComMenuSePlay(MENU_SOUND_REFUSE);
            }
            break;
        }

        case MANUAL_MODE_TRACKER:
            if (JournalChest::HandleKey()) {
                ManualMenu.mode = MANUAL_MODE_SELECT;
            }
            break;

        case MANUAL_MODE_READING:
            CommonMenuMes3.cursor_row = -1;
            ManualMsg->cursor_row     = -1;
            if (GamePad.Down(PAD_R2 | PAD_R1 | PAD_CROSS | PAD_RIGHT)) {
                if (CommonMenuMes3.State() == CLSMES_PAGE_WAIT) {
                    CommonMenuMes3.GoNextPage();
                } else if (CommonMenuMes3.State() == CLSMES_SHOWN) {
                    const int image_count = GetNowManualPartTgaNum();
                    ManualMenu.image_page++;
                    ManualMenu.message_page++;
                    if (IsMultiPageManualEntry()) {
                        ManualMenu.image_page--;
                    }
                    if (ManualMenu.image_page >= image_count) {
                        ManualMenu.mode            = MANUAL_MODE_SELECT;
                        ManualMenu.selection_level = -1;
                    }
                }
                ComMenuSePlay(MENU_SOUND_CONFIRM);
            } else if (GamePad.Down(PAD_L2 | PAD_L1 | PAD_SQUARE | PAD_LEFT)) {
                if (IsMultiPageManualEntry()) {
                    ManualMenu.message_page--;
                } else if (ManualMenu.image_page > 0) {
                    ManualMenu.message_page--;
                    ManualMenu.image_page--;
                }
                ComMenuSePlay(MENU_SOUND_REFUSE);
            } else if (GamePad.Down(PAD_CIRCLE)) {
                ManualMenu.selection_level = -1;
                ManualMenu.mode            = MANUAL_MODE_SELECT;
                ComMenuSePlay(MENU_SOUND_REFUSE);
            }
            break;

        case MANUAL_MODE_OPENING:
            CommonMenuMes3.cursor_row = -1;
            ManualMsg->cursor_row     = -1;
            ManualMenu.transition_frame++;
            ManualImgEnter();
            if (ManualMenu.images_ready != 0 && ManualMenu.transition_frame > 32) {
                ManualMenu.mode             = MANUAL_MODE_SLIDE_IN;
                ManualMenu.transition_frame = 0;
            }
            break;

        case MANUAL_MODE_SLIDE_IN:
            CommonMenuMes3.cursor_row = -1;
            ManualMsg->cursor_row     = -1;
            if (++ManualMenu.transition_frame > 32) {
                ManualMenu.mode = MANUAL_MODE_READING;
            }
            break;
    }
    return closed;
}

PORT_OVERRIDE void MenuManualDraw() {
    if (ManualMenu.mode == MANUAL_MODE_SELECT) {
        DrawCategoryMenu();
        if (ManualMenu.selection_level == -2) {
            return;
        }
    }

    if (ManualMenu.mode == MANUAL_MODE_TRACKER) {
        JournalChest::Draw();
        return;
    }

    if (ManualMenu.mode >= MANUAL_MODE_OPENING && ManualMenu.mode < MANUAL_MODE_COUNT && ManualMenu.images_ready != 0) {
        MenuTextureReload(ManualMenu.image_texture_block);
        const int   image_count = GetNowManualPartTgaNum();
        const int   target      = -(ManualMenu.image_page * 0x280);
        const float distance    = static_cast<float>(target - ManualMenu.image_offset);
        ManualMenu.image_offset += static_cast<int>(distance / 4.0f);
        if (std::abs(static_cast<int>(distance)) <= 3) {
            ManualMenu.image_offset = target;
        }

        int x = ManualMenu.image_offset;
        for (int image = 0; image < image_count; image++, x += 0x280) {
            if (image == 3) MenuTextureReload(ManualMenu.extra_texture_block);
            if (x > 0x280) break;

            int source_x = 0;
            int screen_x = x;
            int width    = 0x280;
            MenuTextureClip(screen_x, source_x, width, 0, 0x280);
            width = std::clamp(width, 0, 0x280);

            CRect_i_  screen(screen_x, 0, width, SCREEN_HEIGHT);
            CRect_i_  texel(source_x, 0, width, 0x1C0);
            CTexture *texture = ManualMenuTex[image];
            if (texture != nullptr) {
                DrawMenu2DSprite(texture, screen, texel, 100, 100, 100, 0x80);
            } else {
                int previous = image - 1;
                while (previous > 0 && ManualMenuTex[previous] == nullptr) {
                    --previous;
                }
                if (previous >= 0) {
                    DrawMenu2DSprite(texture, screen, texel, 0x50, 0x50, 0x50, 0x80);
                }
            }
        }
        MenuTextureReload(ManualMenu.common_texture_block);
        DrawPrevNextCursor();
    }

    if (ManualMenu.messages_ready != 0 && ManualMenu.mode != MANUAL_MODE_CLOSING) {
        Port_DrawManualMsg();
    }
}

std::vector<std::pair<std::string, std::string>> UI::JournalStrings() {
    std::vector<std::pair<std::string, std::string>> out = {
        {"manual.category.georama",        "Georama"       },
        {"manual.category.dungeon",        "Dungeon"       },
        {"manual.category.weapon",         "Weapon"        },
        {"manual.category.savings",        "Savings/Other" },
        {"manual.category.miracle_chests", "Miracle Chests"},
    };
    JournalChest::AppendStrings(out);
    return out;
}