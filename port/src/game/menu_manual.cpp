#include "menu_manual.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <format>
#include <iterator>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "clsmes.hpp"
#include "dataread.hpp"
#include "dngstatusdata.hpp"
#include "eastking.hpp"
#include "gamepad.hpp"
#include "gameutil.hpp"
#include "localize/localize.hpp"
#include "memcard.hpp"
#include "menu_draw.hpp"
#include "menuetc.hpp"
#include "mglib.hpp"
#include "rect.hpp"
#include "render/draw2d.hpp"
#include "savedata.hpp"
#include "snd.hpp"
#include "text/gametext.hpp"
#include "texture.hpp"
#include "ui/framework.hpp"
#include "ui/journal_chest.hpp"
#include "userstatus.hpp"

namespace dc::game {

using namespace dc::localize;
using namespace dc::text;

namespace {

constexpr int ENTRIES_PER_CATEGORY = 6;

/** Number of page images in each manual entry, by category and entry. */
constexpr std::array<s8, 24> ManualTgaNum = {2, 6, 4, 1, 2, 0, 1, 5, 3, 3, 3, 0, 3, 4, 3, 1, 0, 0, 2, 1, 5, 0, 0, 0};

/** Texture name prefixes of each category's page images. */
constexpr std::array<std::string_view, 4> ManualImagePrefixes = {"a_", "b_", "c_", "d_"};

/**
 * Empties a message window and puts its layout back to the defaults.
 */
void ResetManualMessage(ClsMes *message) {
    message->text_columns = MES_WIN_COLUMNS;
    message->text_rows = 10;
    message->text_len = 0;
    message->text_width = 0;
    message->text_height = 0;
    message->fade = 0.0f;
    message->fade_in = true;
    message->text_rate = message->text_rate_set;
    message->waiting = false;
    message->text_at = 0.0f;
    message->text_no = 0;
    message->text_from = 0;
    message->page_from = 0;
    message->InitMesWinTbl();
    message->clut_now = message->clut_default;
    message->wait = 0;
    message->blink = 0;
    message->auto_page_wait = 0;
    message->mes_made = -1;
    message->edge_alpha = 0x80;

    for (s32 &message_no : message->mes_no) {
        message_no = -1;
    }

    for (s32 &value : message->values) {
        value = 0;
    }

    message->value = 0;
    message->value_signed = false;
    message->value_show = true;
    message->value_narrow = false;
    message->space_width = -1;
    message->space_area = -1;
    message->cursor_row = -1;
    message->cursor_y = 0;
    message->cursor_lit = false;

    for (MES_LINE_POS &position : message->line_pos) {
        position.x = -1;
        position.y = -1;
    }
}

} // namespace

MANUAL_MENU_STATE ManualMenu;

CTexture *ManualMenuTex[6];

int GetNowManualPartTgaNum() {
    return ManualTgaNum[(ManualMenu.category * ENTRIES_PER_CATEGORY) + ManualMenu.entry];
}

int GetGameFlagForManualMenu() {
    CDngStatusData *status = SaveData->GetDngStatus();

    if (status == nullptr) {
        return 1;
    }

    int result = 0;

    if (status->SearchItemIndexNo(ITEM_MANUAL) >= 0) {
        result++;
    }

    return result;
}

namespace {

// Port-only mode for the Miracle Chests tracker, outside the retail ManualMenuMode range.
constexpr s16 kManualModeTracker = 100;

ui::Cursor s_menu_cursor;

s16 Port_ManualImgLoad() {
    int size = 0;
    if (ReadBGSync() != 0 || ManualMenu.images_ready != 0) {
        BreakReadBG();
    }

    const int         image_no = ManualMenu.entry + (ManualMenu.category + 1) * 10;
    const std::string prefix = std::string(GetMenuTextureDir()) + GetNowSelectLanguage(GetMenuLangFlag()) + "manual/m";
    std::string       path = prefix + std::to_string(image_no) + ".pac";

    u_long128 *buffer = MenuCalcBufAlignment(ManualMenu.load_buffer);
    StartReadBG();
    LoadFileBG(path.data(), buffer, &size);
    buffer = MenuCalcBufAlignment(buffer + size / 16 + 1);

    if ((image_no >= 11 && image_no <= 12) || (image_no >= 21 && image_no <= 22) || image_no == 31 || image_no == 42) {
        path = prefix + std::to_string(image_no) + "b.pac";
        LoadFileBG(path.data(), buffer, &size);
    }
    ReadBG();
    ManualMenu.image_offset = 0;
    ManualMenu.image_reset = 0;
    ManualMenu.images_ready = false;
    return ManualMenu.images_ready;
}

} // namespace

s16 ManualImgEnter() {
    if (ManualMenu.images_ready == 0 && ReadBGSync() == 0) {
        LOADTEXTURE_INFO2 image_table[] = {
            {.name = "#frame_image#640#480#4", .block_no = 0, .mipmap = 0},
            {.name = nullptr,                  .block_no = 0, .mipmap = 0},
            {.name = nullptr,                  .block_no = 0, .mipmap = 0},
        };
        image_table[0].block_no = ManualMenu.image_texture_block;
        image_table[1].block_no = ManualMenu.image_texture_block;
        BG_READ_INFO *primary = GetReadBGFile(0);
        std::string   pack_name = std::format("{}_{}.img", ManualMenu.category + 1, ManualMenu.entry + 1);
        image_table[1].name =
            reinterpret_cast<char *>(GetPackFile(reinterpret_cast<u_int *>(primary->buffer), pack_name.data(), nullptr));
        TexManager.DeleteTextureBlock(ManualMenu.image_texture_block);
        TexManager.CleanUpTextureList();
        TexManager.LoadTextureBlockEX(-1, image_table);

        BG_READ_INFO *extra = GetReadBGFile(1);

        if (extra != nullptr) {
            LOADTEXTURE_INFO2 extra_table[] = {
                {.name = "#frame_image1#640#" SCREEN_HEIGHT_STR "#4", .block_no = 0, .mipmap = 0},
                {.name = nullptr,                                     .block_no = 0, .mipmap = 0},
                {.name = nullptr,                                     .block_no = 0, .mipmap = 0},
            };
            extra_table[0].block_no = ManualMenu.extra_texture_block;
            extra_table[1].block_no = ManualMenu.extra_texture_block;
            std::string extra_name = std::format("{}_{}b.img", ManualMenu.category + 1, ManualMenu.entry + 1);
            extra_table[1].name =
                reinterpret_cast<char *>(GetPackFile(reinterpret_cast<u_int *>(extra->buffer), extra_name.data(), nullptr));
            TexManager.DeleteTextureBlock(ManualMenu.extra_texture_block);
            TexManager.CleanUpTextureList();
            TexManager.LoadTextureBlockEX(-1, extra_table);
        }

        int image_count = GetNowManualPartTgaNum();

        for (int image = 0; image < image_count; image++) {
            std::string texture_name =
                std::format("{}{}_{}", ManualImagePrefixes[ManualMenu.category], ManualMenu.entry + 1, image + 1);
            ManualMenuTex[image] = TexManager.GetTexture(texture_name.data(), -1);
        }

        ManualMenu.message_page = 0;
        ManualMenu.image_page = 0;
        ManualMenu.images_ready = true;
    }

    return ManualMenu.images_ready;
}

void DrawPrevNextCursor() {
    CTexture *texture = TexManager.GetTexture("mncursor", -1);

    if (texture == nullptr) {
        return;
    }

    CRect_i_                     texel(0x40, 0, 0x20, 0x20);
    constexpr std::array<int, 2> x = {0x40, 0x222};
    int                          image_count = GetNowManualPartTgaNum();

    for (int cursor = 0; cursor < 2; cursor++, texel.y += 0x20) {
        if (cursor == 0 && ManualMenu.image_page == 0) {
            continue;
        }

        if (cursor == 1 && ManualMenu.image_page == image_count - 1) {
            return;
        }

        CRect_i_ screen(x[cursor], 0x106, texel.width, texel.height);
        DrawMenu2DSprite(texture, screen, texel, 0x80);
    }
}

namespace {

void Port_DrawManualMsg() {
    if (ManualMenu.messages_ready == 0) {
        return;
    }
    MenuTextureReload(CommonMenuMes3.tex_block);

    if (ManualMenu.selection_level < 0) {
        ManualMsg->Step();
    }

    int message_no = 0;
    if (ManualMenu.selection_level == -1) {
        CommonMenuMes3.auto_pos = MES_POS_NONE;
        message_no = ManualMenu.category * 1000 + 1000;
        CommonMenuMes3.stay_frame = true;
        CommonMenuMes3.text_x = 0x86;
        CommonMenuMes3.text_y = 0xEE;
    }

    if (ManualMenu.mode == MANUAL_MODE_READING) {
        CommonMenuMes3.stay_frame = true;
        CommonMenuMes3.auto_pos = MES_POS_BOTTOM;
        message_no = ManualMenu.category * 1000 + 1100 + (ManualMenu.entry + 1) * 10 + ManualMenu.message_page;
        CommonMenuMes3.text_x = 0x78;
        CommonMenuMes3.text_y = 0x158;
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

} // namespace

int InitMenuManual(int *texture_blocks, u_long128 *load_buffer) {
    ManualMenu.load_buffer = load_buffer;
    ManualMenu.load_buffer = MenuCalcBufAlignment(ManualMenu.load_buffer);
    u_long128 *buffer = ManualMenu.load_buffer;
    StartReadBG();
    LoadFileBGMenuData("manual/mndata.pak", buffer);
    ReadBG();
    ManualMenu.messages_ready = false;
    ManualMenu.images_ready = false;
    ManualMenu.common_texture_block = texture_blocks[0];
    ManualMenu.image_texture_block = texture_blocks[1];
    ManualMenu.extra_texture_block = texture_blocks[2];
    ManualMenu.mode = MANUAL_MODE_LOADING;
    ManualMenu.category = 0;
    ManualMenu.selection_level = -2;
    ManualMenu.entry = 0;
    ManualMenu.transition_frame = 0;
    ManualMsg = &EastKingMsgCls;
    ManualMsg->edge_alpha = 0;
    ManualMsg->unk_02C = 0x10;
    ManualMsg->init_030 = 0x10;
    ManualMenu.common_message_buffer = CommonMenuMes3.buff;
    ManualMenu.menu_message_buffer = CommonMenuMes1.buff;
    ManualMenu.char_width = CommonMenuMes3.char_width;
    return 1;
}

int SetManualMsgBuffer() {
    if (ManualMenu.messages_ready == 0 && ReadBGSync() == 0) {
        BG_READ_INFO     *archive = GetReadBGFile(0);
        LOADTEXTURE_INFO2 font_table[] = {
            {.name = "#frame_image#640#480#4", .block_no = 0, .mipmap = 0},
            {.name = nullptr,                  .block_no = 0, .mipmap = 0},
            {.name = nullptr,                  .block_no = 0, .mipmap = 0},
        };
        font_table[0].block_no = ManualMenu.common_texture_block;
        font_table[1].block_no = ManualMenu.common_texture_block;
        char font_name[] = "mncursor.img";
        font_table[1].name =
            reinterpret_cast<char *>(GetPackFile(reinterpret_cast<u_int *>(archive->buffer), font_name, nullptr));
        TexManager.DeleteTextureBlock(ManualMenu.common_texture_block);
        TexManager.CleanUpTextureList();
        TexManager.LoadTextureBlockEX(-1, font_table);
        ManualMenu.load_buffer = archive->buffer + (archive->size >> 4) + 1;

        ResetManualMessage(ManualMsg);
        ManualMsg->Preset(MES_PRESET_NAME);
        ManualMsg->char_width = GetMenuCommonFontW(GetMenuLangFlag(), -1);
        ManualMsg->char_height = 0x16;
        ManualMsg->tex_block = 0x1A;
        ManualMsg->tex_buff = CommonMenuMes1.tex_buff;

        for (s32 &message_no : ManualMsg->mes_no) {
            message_no = -1;
        }

        char messages_name[] = "manual.bin";
        s16 *messages =
            reinterpret_cast<s16 *>(GetPackFile(reinterpret_cast<u_int *>(archive->buffer), messages_name, nullptr));
        ManualMsg->SetBuff(messages);
        ManualMsg->mes_made = -1;
        ManualMsg->stay_frame = true;
        ManualMsg->MakeMesWin(100);

        ResetManualMessage(&CommonMenuMes3);
        CommonMenuMes3.Preset(MES_PRESET_NAME);
        CommonMenuMes3.centre_rows = false;
        CommonMenuMes3.SetBuff(messages);
        CommonMenuMes3.rows = 3;
        CommonMenuMes3.char_width = ManualMsg->char_width;

        for (s32 &message_no : CommonMenuMes3.mes_no) {
            message_no = -1;
        }

        ManualMenu.messages_ready = true;
    }

    return ManualMenu.messages_ready;
}

void ExitManualMenu() {
    int texture_blocks[] = {ManualMenu.common_texture_block, ManualMenu.image_texture_block,
                            ManualMenu.extra_texture_block, -1};
    MenuTextureDelete(texture_blocks);
    CommonMenuMes3.centre_rows = true;
    CommonMenuMes3.cursor_row = -1;
    CommonMenuMes3.stay_frame = false;
    ManualMsg->cursor_row = -1;
    ManualMsg->stay_frame = false;
}

int GetNowManualMenuMode() {
    return ManualMenu.mode;
}

namespace {

void DrawCategoryMenu() {
    const int     alpha = (ManualMenu.selection_level == -1) ? 0x40 : 0x80;
    constexpr int spacing = 28;

    ui::TrackerWindow::Draw(135, 85, 220, 170, alpha, ui::BLUE_STONE_MENU);
    MenuTextureReload(0x1A);

    const std::array<std::string, 5> categories = {
        LocalizeText("manual.category.georama", "Georama"),
        LocalizeText("manual.category.dungeon", "Dungeon"),
        LocalizeText("manual.category.weapon", "Weapon"),
        LocalizeText("manual.category.savings", "Savings/Other"),
        LocalizeText("manual.category.miracle_chests", "Miracle Chests")};

    for (int i = 0; i < 5; i++) {
        const u32 color = (i == ManualMenu.category && ManualMenu.selection_level == -2)
                              ? ui::ColorYellow
                              : ui::ColorWhite;
        ui::Label(categories[i], color).Draw(160, 100 + (i * spacing), alpha);
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

int MenuManualKey() {
    ReadBG();
    int closed = 0;

    switch (ManualMenu.mode) {
        case MANUAL_MODE_LOADING:
            if (SetManualMsgBuffer() != 0) {
                constexpr std::string_view k_category_list = "Georama\nDungeon\nWeapon\nSavings/Other\nMiracle Chests";
                const std::span            target_buffer(reinterpret_cast<char *>(ManualMenu.common_message_buffer) + 0x48, k_category_list.size() + 1);
                std::ranges::copy(k_category_list, target_buffer.begin());
                target_buffer.back() = '\0';
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
            int  minimum = 0;
            int  maximum = 0;
            int *selection = nullptr;

            if (ManualMenu.selection_level == -2) {
                maximum = 4;
                selection = &ManualMenu.category;
            } else if (ManualMenu.selection_level == -1) {
                constexpr std::array k_entry_max = {4, 4, 3, 2};
                if (ManualMenu.category < 0 || std::cmp_greater_equal(ManualMenu.category, k_entry_max.size())) {
                    ManualMenu.category = 0;
                    ManualMenu.entry = 0;
                }
                maximum = k_entry_max[ManualMenu.category];
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
                if (*selection < minimum) {
                    *selection = maximum;
                }
                if (*selection > maximum) {
                    *selection = minimum;
                }

                if (ManualMenu.selection_level == -2) {
                    ManualMsg->cursor_row = *selection;
                    CommonMenuMes3.cursor_row = -1;
                } else {
                    CommonMenuMes3.cursor_row = *selection;
                }
            } else {
                ManualMenu.selection_level = -2;
                ManualMenu.entry = 0;
            }

            if (GamePad.Down(PAD_CROSS)) {
                if (ManualMenu.selection_level == -1) {
                    ManualMenu.selection_level++;
                    ManualMenu.mode = MANUAL_MODE_OPENING;
                    ManualMenu.message_page = 0;
                    if (ManualMenu.category != 4) {
                        Port_ManualImgLoad();
                    }
                } else if (ManualMenu.selection_level == -2) {
                    if (ManualMenu.category == 4) {
                        ManualMenu.mode = kManualModeTracker;
                        ui::journal_chest::Open();
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
                    std::ranges::fill(CommonMenuMes3.mes_no, -1);
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

        case kManualModeTracker:
            if (ui::journal_chest::HandleKey()) {
                ManualMenu.mode = MANUAL_MODE_SELECT;
            }
            break;

        case MANUAL_MODE_READING:
            CommonMenuMes3.cursor_row = -1;
            ManualMsg->cursor_row = -1;
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
                        ManualMenu.mode = MANUAL_MODE_SELECT;
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
                ManualMenu.mode = MANUAL_MODE_SELECT;
                ComMenuSePlay(MENU_SOUND_REFUSE);
            }
            break;

        case MANUAL_MODE_OPENING:
            CommonMenuMes3.cursor_row = -1;
            ManualMsg->cursor_row = -1;
            ManualMenu.transition_frame++;
            ManualImgEnter();
            if (ManualMenu.images_ready != 0 && ManualMenu.transition_frame > 32) {
                ManualMenu.mode = MANUAL_MODE_SLIDE_IN;
                ManualMenu.transition_frame = 0;
            }
            break;

        case MANUAL_MODE_SLIDE_IN:
            CommonMenuMes3.cursor_row = -1;
            ManualMsg->cursor_row = -1;
            if (++ManualMenu.transition_frame > 32) {
                ManualMenu.mode = MANUAL_MODE_READING;
            }
            break;

        default:
            break;
    }
    return closed;
}

void MenuManualDraw() {
    if (ManualMenu.mode == MANUAL_MODE_SELECT) {
        DrawCategoryMenu();
        if (ManualMenu.selection_level == -2) {
            return;
        }
    }

    if (ManualMenu.mode == kManualModeTracker) {
        ui::journal_chest::Draw();
        return;
    }

    if (ManualMenu.mode >= MANUAL_MODE_OPENING && ManualMenu.mode < MANUAL_MODE_COUNT && ManualMenu.images_ready != 0) {
        MenuTextureReload(ManualMenu.image_texture_block);
        const int  image_count = std::clamp(GetNowManualPartTgaNum(), 0, static_cast<int>(std::size(ManualMenuTex)));
        const int  target = -(ManualMenu.image_page * 0x280);
        const auto distance = static_cast<float>(target - ManualMenu.image_offset);
        ManualMenu.image_offset += static_cast<int>(distance / 4.0f);
        if (std::abs(static_cast<int>(distance)) <= 3) {
            ManualMenu.image_offset = target;
        }

        int x = ManualMenu.image_offset;
        for (int image = 0; image < image_count; image++, x += 0x280) {
            if (image == 3) {
                MenuTextureReload(ManualMenu.extra_texture_block);
            }
            if (x > 0x280) {
                break;
            }

            int source_x = 0;
            int screen_x = x;
            int width = 0x280;
            MenuTextureClip(screen_x, source_x, width, 0, 0x280);
            width = std::clamp(width, 0, 0x280);

            CRect_i_  screen(screen_x, 0, width, SCREEN_HEIGHT);
            CRect_i_  texel(source_x, 0, width, 0x1C0);
            CTexture *texture = ManualMenuTex[image];
            if (texture != nullptr) {
                DrawMenu2DSprite(texture, screen, texel, 100, 100, 100, 0x80);
            } else {
                int previous = image - 1;
                while (previous >= 0 && ManualMenuTex[previous] == nullptr) {
                    --previous;
                }
                if (previous >= 0) {
                    DrawMenu2DSprite(ManualMenuTex[previous], screen, texel, 0x50, 0x50, 0x50, 0x80);
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

std::vector<std::pair<std::string, std::string>> JournalStrings() {
    std::vector<std::pair<std::string, std::string>> out = {
        {"manual.category.georama",        "Georama"       },
        {"manual.category.dungeon",        "Dungeon"       },
        {"manual.category.weapon",         "Weapon"        },
        {"manual.category.savings",        "Savings/Other" },
        {"manual.category.miracle_chests", "Miracle Chests"},
    };
    ui::journal_chest::AppendStrings(out);
    return out;
}

} // namespace dc::game
