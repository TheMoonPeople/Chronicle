#include "battlemenu/internal.hpp"

namespace {

using btlmenu_detail::DecodeMesFromBuffer;
using btlmenu_detail::RefreshBtlMenuTextures;

/**
 * Animated hand cursor for the move confirmation modal.
 */
UI::Cursor move_cursor;

/**
 * Previous move menu state used to detect when the modal opens.
 */
int last_move_state = -1;

/**
 * Extracts the title and description lines for the active move confirmation prompt.
 */
void ExtractMovePromptText(std::string &out_title, std::string &out_desc) {
    out_title.clear();
    out_desc.clear();

    if (CommonMenuMes3.buff != nullptr && CommonMenuMes3.mes_made > 0) {
        const std::string decoded = DecodeMesFromBuffer(CommonMenuMes3.buff, CommonMenuMes3.mes_made);
        const size_t      nl      = decoded.find('\n');
        if (nl != std::string::npos) {
            out_title = decoded.substr(0, nl);
            out_desc  = decoded.substr(nl + 1);
        } else {
            out_title = decoded;
        }
    }

    if (out_title.empty()) {
        switch (MenuMove.mode) {
            case MENU_MOVE_FIRST_DUNGEON:
                out_title = LocalizeText("battlemenu.move.enter_dungeon_1", "Enter");
                out_desc  = LocalizeText("battlemenu.move.enter_dungeon_2", "Divine Beast Cave?");
                break;
            case MENU_MOVE_INTERIOR_OUT:
                out_title = LocalizeText("battlemenu.move.go_outside", "Go outside?");
                break;
            case MENU_MOVE_DUNGEON_ESCAPE:
                out_title = LocalizeText("battlemenu.move.leave_dungeon", "Leave the dungeon?");
                break;
            default:
                break;
        }
    }
}

} // namespace

PORT_OVERRIDE void DrawMenuMove() {
    if (MenuMove.mode == MENU_MOVE_WORLD_MAP_DIRECT) {
        MenuWorldTrans(&MenuCamera);
        FrameImageDraw(0x40, 0x80);
    }

    RefreshBtlMenuTextures();

    int alpha = 0x80;
    if (MenuMove.state == MENU_MOVE_STATE_OPEN) {
        alpha = MenuMove.counter * 6;
    } else if (MenuMove.state == MENU_MOVE_STATE_CLOSE) {
        alpha = 0x80 - MenuMove.counter * 8;
    }
    alpha = std::clamp(alpha, 0, 0x80);

    if (MenuMove.mode == MENU_MOVE_FIRST_DUNGEON ||
        MenuMove.mode == MENU_MOVE_INTERIOR_OUT ||
        MenuMove.mode == MENU_MOVE_DUNGEON_ESCAPE) {
        if (MenuMove.mode == MENU_MOVE_DUNGEON_ESCAPE) {
            DrawEscapeItem(0x32, 0x160, alpha);
        }

        if (last_move_state != MenuMove.state && MenuMove.state == MENU_MOVE_STATE_OPEN) {
            constexpr int btn_x  = (640 - 100) / 2;
            constexpr int btn_y0 = 164 + 58 + 10;
            const int     sel_y  = (MenuMove.cursor == 0) ? btn_y0 : (btn_y0 + 26 + 6);
            move_cursor.SnapTo(static_cast<float>(btn_x - 34), static_cast<float>(sel_y + 3));
        }
        last_move_state = MenuMove.state;

        std::string prompt_title;
        std::string prompt_desc;
        ExtractMovePromptText(prompt_title, prompt_desc);

        UI::ConfirmModal::Draw(
            prompt_title,
            prompt_desc,
            LocalizeText("battlemenu.quit.yes", "YES"),
            LocalizeText("battlemenu.quit.no", "NO"),
            MenuMove.cursor,
            move_cursor,
            alpha
        );

        if (MenuMove.state == MENU_MOVE_STATE_INTERIOR_FADE && GetInteriorOutFlag() != 0) {
            AllFadeForMenu(std::min(0x80, static_cast<int>(MenuMove.counter * 3)));
        }

        if (MenuMove.state == MENU_MOVE_STATE_SELECT || MenuMove.state == MENU_MOVE_STATE_CONFIRM) {
            MenuMove.counter = 0;
        } else {
            MenuMove.counter++;
        }
        return;
    }

    int            x    = 0;
    int            y    = 0;
    WORLD_MAP_POS *next = reinterpret_cast<WORLD_MAP_POS *>(NextWorldPos);
    if (next != nullptr) {
        x = next->x;
        y = next->y;
    } else {
        x         = 0x150;
        y         = 0xB2;
        SysCur[0] = static_cast<float>(x);
        SysCur[1] = static_cast<float>(y);
    }

    const int map_alpha = (MenuMove.state == MENU_MOVE_STATE_CLOSE) ? alpha : 0x80;
    if (MenuMove.ready != 0 && MenuCharaFrame.frame != nullptr) {
        MenuTextureReload(MenuMove.tex_block);
        MenuPolygonDraw(map_alpha, +[]() {
            MenuCharaFrame.SetScale(2.2f, 2.2f, 2.2f);
            MenuCharaFrame.SetPosition(mapo);
            MenuCharaFrame.Step();
            MenuCharaFrame.Draw();
        });
    }

    SysCur[0] += (static_cast<float>(x) - SysCur[0]) / 3.0f;
    SysCur[1] += (static_cast<float>(y) - SysCur[1]) / 3.0f;

    bool arrived = true;
    if (!(std::abs(static_cast<int>(SysCur[0] - static_cast<float>(x))) <= 4.0f) &&
        !(std::abs(static_cast<int>(SysCur[1] - static_cast<float>(y))) <= 4.0f)) {
        arrived = false;
    }
    CommonMenuMes3.stay_frame = false;

    const float width     = static_cast<float>(CommonMenuMes3.GetMesWidth_system(CommonMenuMes3.mes_no[0]));
    CommonMenuMes3.text_x = static_cast<int>(static_cast<double>(x) - static_cast<double>(width) / 2.0);
    CommonMenuMes3.text_y = y - 0x22;

    x = static_cast<int>(SysCur[0]);
    y = static_cast<int>(SysCur[1]);

    if (MenuMove.state == MENU_MOVE_STATE_SELECT) {
        MenuTextureReload(MenuMove.tex_block);
        DrawMapCheck(0x80);
        const CRect_i_ texel(0xE2, 0, 0x1E, 0x20);
        DrawObjectVibe(x + 2, y + 2, MenuMoveTex, texel, 0xA, 0x50);
        DrawObjectVibe(x, y, MenuMoveTex, texel, 0x80, 0x80);
        CommonMenuMes3.stay_frame = false;
        MenuHelpWinDraw2(CommonMenuMes3.text_x - 0x18, CommonMenuMes3.text_y - 0xE, 1.0f + width, 0.0f, 0x80, 0, 0, StayTex);
    } else if (MenuMove.state == MENU_MOVE_STATE_CONFIRM) {
        MenuHelpWinDraw2(CommonMenuMes3.text_x - 0x18, CommonMenuMes3.text_y - 0xE, 1.0f + width, 0.0f, 0x80, 0, 0, StayTex);
    }

    MenuTextureReload(CommonMenuMes3.tex_block);
    if (arrived) {
        int mes_alpha = alpha;
        if (MenuMove.state == MENU_MOVE_STATE_ZOOM_IN) {
            mes_alpha = 0x80 - MenuMove.counter * 0xA;
        } else if (MenuMove.state == MENU_MOVE_STATE_ZOOM_OUT) {
            mes_alpha = MenuMove.counter * 6;
        }
        CommonMenuMes3.edge_alpha = std::clamp(mes_alpha, 0, 0x80);
        CommonMenuMes3.Step();
        CommonMenuMes3.DrawMesWin();
    }

    if (MenuMove.state == MENU_MOVE_STATE_CONFIRM) {
        float win_x = 0.0f;
        float win_y = 0.0f;
        float win_w = 0.0f;
        float win_h = 0.0f;
        GetMainMenuRightHelpWinLangOffset(win_x, win_y, win_w, win_h);
        if (GetMenuLangFlag() > LANG_JAPANESE) {
            win_x -= 12.0f;
            win_w += 1.2f;
        }
        MenuHelpWinDraw(static_cast<int>(win_x), static_cast<int>(win_y), win_w, win_h, 0x80);

        int text_x = 0;
        int text_y = 0;
        GetMainMenuRightHelpMsgLangOffset(text_x, text_y);
        CommonMenuMes2.text_x = static_cast<int>(win_x + static_cast<float>(text_x));
        CommonMenuMes2.text_y = static_cast<int>(win_y + static_cast<float>(text_y));
        CommonMenuMes2.Step();
        CommonMenuMes2.DrawMesWin();
    }

    if (MenuMove.mode == MENU_MOVE_WORLD_MAP_DIRECT) {
        MenuTextureReload(MenuMove.tex_block);
        const int title_w = (GetMenuLangFlag() > LANG_JAPANESE) ? 0xE0 : 0xD8;
        DrawMenu2DSprite(MenuMoveTex, CRect_i_(0x3C, 0x28, title_w, 0x28), CRect_i_(0, 0, title_w, 0x28), 0x80);
    }

    if (MenuMove.state == MENU_MOVE_STATE_SELECT || MenuMove.state == MENU_MOVE_STATE_CONFIRM) {
        MenuMove.counter = 0;
    } else {
        MenuMove.counter++;
    }
}