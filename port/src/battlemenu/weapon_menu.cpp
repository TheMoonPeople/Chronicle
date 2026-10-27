#include <array>

#include "battlemenu/internal.hpp"

namespace dc::battlemenu {

using namespace dc::game;
using namespace dc::localize;

namespace {

using detail::DecodeItemNamePort;
using detail::DecodeMesFromBuffer;
using detail::RefreshBtlMenuTextures;

/**
 * Bitflags indicating which customization actions are available for a weapon.
 */
enum class WeaponActionBit : int {
    Upgrade = 1 << 1,
    Synthesize = 1 << 2,
    BuildUpEntry = 1 << 3,
    BuildUpReady = 1 << 4,
};

/**
 * Returns a pointer to the weapon currently highlighted in the Weapon Menu.
 */
WEAPON_HAVE *GetNowSelectWeaponPort() {
    if (BtlMenuStatusPt == nullptr) {
        return nullptr;
    }
    return &BtlMenuStatusPt->chara_weapons[static_cast<int>(WepMenu.chara)][static_cast<int>(WepMenu.weapon_slot)];
}

/**
 * Computes the bitmask of available upgrade, synthesis, and build-up actions for a weapon.
 */
int NowWeaponStatusValuePort(WEAPON_HAVE *weapon) {
    if (weapon == nullptr) {
        return 0;
    }

    int status = 0;
    if (weapon->experience >= GetWeaponMaxExp(weapon) || GetNowItemNum(ITEM_POWERUP_POWDER, MenuItemPackPt) > 0) {
        status |= static_cast<int>(WeaponActionBit::Upgrade);
    }
    if (WeaponStatusBreakEnable(weapon) != 0) {
        status |= static_cast<int>(WeaponActionBit::Synthesize);
    }
    status |= static_cast<int>(WeaponActionBit::BuildUpEntry);

    int build_up = 0;
    if (WeaponStatusBuildUp(weapon, build_up) > 0 && GetNowWeaponAttachNum(weapon) == 0 && build_up > 0) {
        status |= static_cast<int>(WeaponActionBit::BuildUpReady);
    }
    if (IsNotBuildUpWeapon(weapon->item_no) != 0) {
        status &= ~static_cast<int>(WeaponActionBit::BuildUpEntry);
        status &= ~static_cast<int>(WeaponActionBit::BuildUpReady);
    }
    return status;
}

/**
 * Draws the names of the weapons either side of the cursor in the game's message window, laying them out again
 * whenever a name or level changes, and the name frame under them while the page is at its top level.
 */
void WeaponNameDrawPort(int y, int selected_only, int alpha) {
    std::array<int, 4> previous_mes{};
    std::array<int, 4> previous_level{};

    MenuTextureReload(AtoraNameMes.tex_block);
    int slot = WepMenu.weapon_slot - 1;

    for (int i = 0; i < 4; i++) {
        previous_mes[i] = AtoraNameMes.mes_no[i];
        previous_level[i] = AtoraNameMes.values[i];
    }

    int x = static_cast<int>(22.0f + WeaponPos + static_cast<float>(slot * 0xD6));
    AtoraNameMes.value_signed = true;

    for (int i = 0; i < 3; i++) {
        int mes_no = 999;
        int level = 0;

        const bool hidden = x < -140 || x > 630 || (selected_only == 1 && slot != WepMenu.weapon_slot) || slot < 0 || slot > 9 ||
                            ((WepMenu.state == WEP_STATE_STATUS_BREAK_EFFECT || WepMenu.state == WEP_STATE_BUILDUP_EFFECT) && slot == WepMenu.weapon_slot) ||
                            (WepMenu.mode > WEP_MENU_LIST && slot != WepMenu.weapon_slot) ||
                            (WepMenu.state != WEP_STATE_REPAIR_EFFECT && WepMenu.state >= WEP_STATE_STATUS_BREAK_EFFECT && WepMenu.state < WEP_STATE_WARNING) ||
                            (WepMenu.state == WEP_STATE_LEVELUP_EFFECT && slot != WepMenu.weapon_slot);
        if (!hidden) {
            mes_no = GetWeaponMsgNo(&DngWepHavePt[slot]);

            if (mes_no < 0 || slot < 0) {
                mes_no = 999;
            }

            if (mes_no > 100) {
                AtoraNameMes.GetMesWidth_system(mes_no);
                level = DngWepHavePt[slot].level;
            }
        }

        int width = AtoraNameMes.GetMesWidth_system(mes_no);
        AtoraNameMes.mes_no[i] = mes_no;
        AtoraNameMes.values[i] = level;
        int name_x = GetWeaponNamePutX(x + 0x42, width);

        if (level > 0) {
            name_x -= 8;

            if (level / 10 > 0) {
                name_x -= 8;
            }
        }

        AtoraNameMes.line_pos[i].x = name_x;
        AtoraNameMes.line_pos[i].y = y;
        x += 0xD6;
        slot++;
    }

    for (int i = 0; i < 4; i++) {
        if (AtoraNameMes.line_pos[i].y > 180) {
            AtoraNameMes.line_pos[i].y = y;
            AtoraNameMes.mes_no[i] = 999;
        }
    }

    for (int i = 0; i < 4; i++) {
        if (AtoraNameMes.mes_no[i] != previous_mes[i] || AtoraNameMes.values[i] != previous_level[i]) {
            AtoraNameMes.mes_made = -1;
            AtoraNameMes.MakeMesWin(150);
            break;
        }
    }

    AtoraNameMes.edge_alpha = alpha;
    AtoraNameMes.Step();
    AtoraNameMes.DrawMesWin();

    if (WepMenu.mode == WEP_MENU_LIST && WepMenu.state == WEP_STATE_IDLE) {
        CTexture *frame = TexManager.GetTexture(const_cast<char *>("frame_image"), -1);

        if (frame != nullptr) {
            TexManager.ReloadTexture(GetVif1Packet(), frame->block);
            reinterpret_cast<sceGsTex0 *>(&frame->tex0)->bits.tcc = 0;
            sceGsTexa texa = mgTexa;
            texa.AEM = 1;
            texa.TA0 = 0x80;
            MGSetGsTEXA(&texa);
            CRect_i_ rect(0, 0x14A, 0x104, 0x76);
            set2DSprite(GetVif1Packet(), frame, rect, rect, 0x40, 0x40, 0x40, alpha);
            MGSetGsTEXA(nullptr);
        }
    }
}

/**
 * Draws the three-tab weapon status, element, and anti-monster board.
 */
void DrawWeaponTagBoardPort(int x, int y, WEAPON_HAVE *weapon, int build, int weapon_no, int alpha) {
    const int page = WepMenu.tag_page;
    int       tag = 0;
    int       step = 1;
    int       wrapped = 0;

    for (int i = 0; i < 3; i++) {
        int brightness = 128;
        if (page == tag) {
            if (wrapped != 0) {
                tag = page;
            } else {
                tag = 2;
                step = -1;
                wrapped = 1;
            }
        }
        if (WepMenu.mode == WEP_MENU_ELEMENT && tag != WEP_TAG_ELEMENT) {
            brightness = 64;
        }
        const int top = tag * 0xAA;
        DrawMenu2DSprite(WepStatus, CRect_i_(x, y, 0xD4, 0xAA), CRect_i_(0, top, 0xD4, 0xAA),
                         brightness, brightness, brightness, alpha);
        tag += step;
    }

    if (weapon != nullptr) {
        switch (page) {
            case WEP_TAG_STATUS:
                DrawWeaponStatusTag(x, y, weapon, build, weapon_no, alpha);
                break;
            case WEP_TAG_ELEMENT:
                DrawWeaponElemTag(x, y, weapon, build, weapon_no, alpha);
                break;
            case WEP_TAG_VS_MONSTER:
                DrawWeaponVsMonster(x, y, weapon, build, weapon_no, alpha);
                break;
            default:
                break;
        }
    }
}

/**
 * Draws a weapon action confirmation prompt with YES and NO buttons.
 */
void DrawWeaponLeftConfirmDialog(int box_x, int box_y, int yes_x, int yes_y,
                                 std::string_view fallback_prompt, int alpha) {
    std::string prompt = DecodeMesFromBuffer(CommonMenuMes1.buff, CommonMenuMes1.mes_made);
    if (prompt.empty()) {
        prompt = std::string(fallback_prompt);
    }

    std::vector<std::string> lines = ui::WrapText(prompt, 220, 0.92f);
    if (lines.empty()) {
        lines.emplace_back(prompt);
    }

    int max_w = 0;
    for (const auto &line : lines) {
        max_w = std::max(max_w, ui::Label(line, ui::ColorWhite, ui::Alignment::Left, true, 0.92f).GetWidth());
    }

    constexpr int line_h = 20;
    const int     box_w = std::clamp(max_w + 44, 212, 260);
    const int     box_h = 34 + static_cast<int>(lines.size() - 1) * line_h;

    ui::HelpBox::Draw(box_x, box_y, box_w, box_h, alpha);

    for (size_t i = 0; i < lines.size(); ++i) {
        ui::Label(lines[i], ui::ColorWhite, ui::Alignment::Center, true, 0.92f)
            .Draw(box_x + box_w / 2, box_y + 10 + static_cast<int>(i) * line_h, alpha);
    }

    const ui::ButtonStyle btn_style = ui::ButtonStyle::Thin(ui::ButtonPreset::Terracotta);

    const std::string yes_txt = LocalizeText("battlemenu.quit.yes", "YES");
    const std::string no_txt = LocalizeText("battlemenu.quit.no", "NO");

    ui::BevelButton::Draw(ui::Rect(yes_x, yes_y, 0x60, 23), yes_txt, btn_style, alpha);
    ui::BevelButton::Draw(ui::Rect(yes_x, yes_y + 0x1C, 0x60, 23), no_txt, btn_style, alpha);
}

/**
 * Draws a centered warning message box over the Weapon Menu.
 */
void DrawWeaponCenteredWarningModal(int alpha) {
    const std::string msg = DecodeMesFromBuffer(CommonMenuMes1.buff, CommonMenuMes1.mes_made);
    if (msg.empty()) {
        CommonMenuMes1.auto_pos = MES_POS_CENTRE;
        MenuTextureReload(CommonMenuMes1.tex_block);
        CommonMenuMes1.Step();
        CommonMenuMes1.DrawMesWin();
        return;
    }

    std::vector<std::string> lines = ui::WrapText(msg, 320, 0.92f);
    int                      max_w = 0;
    for (const auto &line : lines) {
        max_w = std::max(max_w, ui::Label(line, ui::ColorWhite, ui::Alignment::Left, true, 0.92f).GetWidth());
    }

    constexpr int line_h = 22;
    const int     box_w = std::clamp(max_w + 52, 260, 380);
    const int     box_h = 38 + static_cast<int>(lines.size() - 1) * line_h;
    const int     box_x = (640 - box_w) / 2;
    const int     box_y = (480 - box_h) / 2 - 16;

    ui::HelpBox::Draw(box_x, box_y, box_w, box_h, alpha);

    for (size_t i = 0; i < lines.size(); ++i) {
        ui::Label(lines[i], ui::ColorWhite, ui::Alignment::Center, true, 0.92f)
            .Draw(320, box_y + 12 + static_cast<int>(i) * line_h, alpha);
    }
}

} // namespace

void PortDrawWeaponSelectDialog(int x, int y, int alpha) {
    const s8     shift = (BtlMenuNowLang > LANG_JAPANESE) ? 2 : 0;
    const int    left = x + shift;
    const int    top = y + shift;
    WEAPON_HAVE *weapon = GetNowSelectWeaponPort();
    if (weapon == nullptr) {
        return;
    }

    const int repair_items = GetNowItemNum(ITEM_REPAIR_POWDER, MenuItemPackPt);
    const int options = NowWeaponStatusValuePort(weapon);

    constexpr int btn_w = 0x60;
    constexpr int btn_h = 23;
    constexpr int row_stride = 0x1A;

    // While a weapon is being chosen to equip, its button shows pressed, as the game's lit EQUIP does.
    ui::ButtonStyle equip_style = ui::ButtonStyle::Thin(ui::ButtonPreset::Terracotta, 0x80);
    equip_style.pressed = WepMenu.state == WEP_STATE_EQUIP;
    ui::BevelButton::Draw(ui::Rect(left, top + 1, btn_w, btn_h), LocalizeText("weaponmenu.action.equip", "EQUIP"), equip_style, alpha);

    ui::BevelButton::Draw(
        ui::Rect(left, top + 1 + row_stride, btn_w, btn_h),
        LocalizeText("weaponmenu.action.customize", "CUSTOMIZE"),
        ui::ButtonStyle::Thin(ui::ButtonPreset::Terracotta, 0x80),
        alpha);

    const int attr_x = (WepMenu.mode == WEP_MENU_ELEMENT) ? (left + 0x28) : left;
    ui::BevelButton::Draw(
        ui::Rect(attr_x, top + 1 + row_stride * 2, btn_w, btn_h),
        LocalizeText("weaponmenu.action.attribute", "ATTRIBUTE"),
        ui::ButtonStyle::Thin(ui::ButtonPreset::Terracotta, 0x80),
        alpha);

    int repair_bright = 0x80;
    if (repair_items <= 0 || weapon->durability_f == static_cast<float>(weapon->durability)) {
        repair_bright = 0x48;
    }
    static int warm_cnt = 0;
    const int  default_no = GetDefaultWeaponNo(WepMenu.chara);
    if ((weapon->durability_f <= 0.1f * weapon->durability || weapon->item_no == default_no) && repair_items > 0) {
        repair_bright = std::abs(static_cast<int>(50.0f * std::sin(static_cast<float>(warm_cnt) / 12.0f))) + 0x78;
        warm_cnt = (warm_cnt + 1) % 1000000;
    }

    ui::BevelButton::Draw(
        ui::Rect(left, top + 1 + row_stride * 3, btn_w, btn_h),
        LocalizeText("weaponmenu.action.repair", "REPAIR"),
        ui::ButtonStyle::Thin(ui::ButtonPreset::Terracotta, repair_bright),
        alpha);

    static int level_blink_cnt = 0;
    level_blink_cnt = (level_blink_cnt + 1) % 1000000;

    int dest_y = top + 1 + row_stride * 4;
    for (int i = 1; i <= 3; i++) {
        int row_bright = 0x80;
        if (i == 3) {
            int build = 0;
            WeaponStatusBuildUp(weapon, build);
            const int attached = GetNowWeaponAttachNum(weapon);
            if (build > 0 && attached == 0) {
                row_bright += std::abs(static_cast<int>(40.0f * std::sin(static_cast<float>(level_blink_cnt) / 12.0f)));
            }
        }

        if (options & (1 << i)) {
            std::string      btn_label;
            ui::ButtonPreset btn_preset = ui::ButtonPreset::AmberGold;
            if (i == 1) {
                btn_label = LocalizeText("weaponmenu.action.upgrade", "UPGRADE");
                btn_preset = ui::ButtonPreset::AmberGold;
            } else if (i == 2) {
                btn_label = LocalizeText("weaponmenu.action.statusbreak", "STATUS BREAK");
                btn_preset = ui::ButtonPreset::SlateBlue;
            } else {
                btn_label = LocalizeText("weaponmenu.action.buildup", "BUILD-UP");
                btn_preset = ui::ButtonPreset::StoneGreen;
            }

            ui::BevelButton::Draw(
                ui::Rect(left, dest_y, btn_w, btn_h),
                btn_label,
                ui::ButtonStyle::Thin(btn_preset, row_bright),
                alpha);
            dest_y += row_stride;
        }
    }

    if (WepMenu.state == WEP_STATE_SCRAP_PROMPT || WepMenu.state == WEP_STATE_SCRAP_CONFIRM) {
        ui::BevelButton::Draw(
            ui::Rect(x + 0x64, y + 0x33, btn_w, btn_h),
            LocalizeText("weaponmenu.action.discard", "DUMP"),
            ui::ButtonStyle::Thin(ui::ButtonPreset::Terracotta, 0x80),
            0x80);
    }
}

void PortDrawBuildUpWeaponSelect(int x, int y, int cursor) {
    WEAPON_HAVE *weapon = GetNowSelectWeaponPort();
    if (weapon == nullptr) {
        return;
    }

    int       build = 0;
    const int count = WeaponStatusBuildUp(weapon, build);

    WEP_BUILDUP_INFO builds[8]{};
    EnableBuildUpModel(builds, weapon);

    std::string header = DecodeMesFromBuffer(CommonMenuMes1.buff, 0x75);
    if (header.empty()) {
        header = LocalizeText("weaponmenu.buildup.prompt", "Select weapon to build up to:");
    }

    std::vector<std::string> h_lines = ui::WrapText(header, 220, 0.86f);
    const int                header_h = std::max(1, static_cast<int>(h_lines.size())) * 20 + 14;
    constexpr int            box_w = 252;
    const int                box_h = header_h + count * 0x26 + 22;

    ui::HelpBox::Draw(x - 14, y - 12, box_w, box_h, 0x80);

    for (size_t i = 0; i < h_lines.size(); ++i) {
        ui::Label(h_lines[i], ui::ColorWhite, ui::Alignment::Left, true, 0.86f)
            .Draw(x + 4, y + static_cast<int>(i) * 20, 0x80);
    }

    int row_y = y + CommonMenuMes1.char_height + 0x1E;
    for (int i = 0; i < count; i++) {
        const bool selected = (i == cursor);
        const int  brightness = selected ? 0x88 : 0x68;

        std::string target_name = "???";
        u32         text_col = ui::ColorWhite;
        if (builds[i].enabled != 0 && builds[i].weapon_no > 0) {
            target_name = DecodeItemNamePort(builds[i].weapon_no, "???");
            text_col = selected ? ui::ColorYellow : ui::ColorWhite;
        }

        const ui::TextStyle banner_txt{
            .color = text_col,
            .drop_shadow = true,
            .align = ui::Alignment::Center,
            .scale = 0.90f,
        };

        ui::TrackerWindow::DrawStoneBanner(
            ui::Rect(x + 0x10, row_y, 0xCC, 0x1F),
            0x80,
            brightness,
            target_name,
            banner_txt);

        row_y += 0x26;
    }
}

void PortDrawAallWeapon(int x, int y, float depth, CCharacter *model,
                        WEAPON_HAVE *weapon, int equipped, int selected, int alpha) {
    if (weapon == nullptr) {
        return;
    }

    MenuTextureReload(MenuExtendReadBlock);
    float         spin = 0.0f;
    sceVu0FVECTOR position = {depth, -1.3f, 0.0f, 1.0f};

    if (model != nullptr) {
        if (equipped != 0) {
            position[1] = -1.3f + std::sin(WepFrameRate);
            WepFrameRate = std::min(static_cast<float>(PI), WepFrameRate + 0.34906587f);
            if (static_cast<float>(PI) != WepFrameRate) {
                spin = 0.0f;
            }
        }

        MENU_WEP_POLY_OFFSET *offset = &MenuWepPolyOffset[static_cast<int>(WepMenu.chara)];
        position[0] += offset->position[0];
        position[1] += offset->position[1];
        model->SetPosition(position);
        const float scale = offset->scale;
        model->SetScale(scale, scale, scale);

        if (selected != 0) {
            spin = static_cast<float>(PI_D * (CursorVibeCnt % 219 - 110) / 110);
        }

        if (MenuWepLevelUp.operation_kind == WEP_EFFECT_NONE ||
            MenuWepLevelUp.operation_kind == WEP_EFFECT_RECOVER ||
            MenuWepLevelUp.operation_kind == WEP_EFFECT_BUILDUP) {
            float rot_x = -HALF_PI;
            if (rot_x > PI_D) {
                rot_x -= PI;
            }
            if (spin > PI_D) {
                spin -= PI;
            }
            float rot_z = 0.62831855f;
            if (rot_z > PI_D) {
                rot_z -= PI;
            }
            model->SetRotation(rot_x, spin, rot_z);
        }

        model->Draw();
    }

    switch (WepMenu.mode) {
        case WEP_MENU_LIST:
        case WEP_MENU_ACTION:
        case WEP_MENU_ELEMENT:
        case WEP_MENU_LEVELUP_CONFIRM:
        case WEP_MENU_STATUS_BREAK_CONFIRM:
        case WEP_MENU_BUILDUP_SELECT:
        case WEP_MENU_BUILDUP_CONFIRM:
        case WEP_MENU_BUILDUP_PREVIEW:
            DrawWepStatus(x + 2, y + 0x66, weapon, equipped, alpha);
            DrawWepVolumeDisplay(x, y + 0x14, weapon, alpha);
            break;
        default:
            break;
    }

    // The plate; the weapon's name goes on it in the game's letters (WeaponNameDrawPort, once the weapons are drawn).
    ui::TrackerWindow::DrawStoneBanner(ui::Rect(x - 0x11, y - 0x31, 0xCC, 0x1F), alpha, 0x80, std::string{}, ui::TextStyle{});

    setbilinear(0);

    if (equipped != 0) {
        DrawMenu2DSprite(WepStatus, CRect_i_(x + 10, y - 0x15, 0x40, 0x17), CRect_i_(0xD4, 0x141, 0x40, 0x17), alpha);
    }

    WeaponStarDraw(x + 4, y + 0x3C, weapon, alpha);

    if (selected != 0) {
        MenuMes.draw_x = x + 0xA0;
        MenuMes.draw_y = y + 0xE;
    } else {
        MenuMes.draw_x = 0x280;
        MenuMes.draw_y = 0;
        setbilinear(0);
        MenuTextureReload(BtlMenuReadBlock);
        WeaponOptionStatusDraw(weapon, x + 0x50, y - 0x12, alpha);
    }

    constexpr float hole_scale[2] = {0.75f, 1.0f};
    const float     width = static_cast<float>(GetWeaponHoleNum(weapon->item_no) * 0x12 + 10) * hole_scale[selected];
    const int       draw_x = static_cast<int>(static_cast<float>(x + 0x52) - width);

    if (WepMenu.mode >= WEP_MENU_ATTACH_WEAPON) {
        DrawWepHole(draw_x, 0xB8, weapon, selected, alpha);
        MenuTextureReload(MenuShadowReadBlock);
        DrawWepAttach(draw_x + 0xE, 0xBE, weapon, selected, alpha);
    }
}

void PortBtlWeaponDraw(int x, float depth, int chara, int alpha) {
    const int     equipped_slot = BtlMenuStatusPt->equipped_weapon_slot[chara];
    const int     cursor = WepMenu.weapon_slot;
    sceVu0FVECTOR ref = {0.0f, 0.0f, -45.0f, 1.0f};
    sceVu0FVECTOR pos = {0.0f, 0.0f, 60.0f, 1.0f};
    sceVu0FVECTOR ambient;
    sceVu0FVECTOR bright;
    sceVu0FMATRIX light_dir;
    sceVu0FMATRIX light_col;

    MenuCamera.SetRef(ref);
    MenuCamera.SetPos(pos);
    MGGetPLight(light_dir, light_col);
    MGGetAmbient(ambient);
    bright[0] = bright[1] = bright[2] = 80.0f;
    sceVu0FVECTOR direction = {0.3f, 1.0f, 0.3f, 0.0f};
    sceVu0FVECTOR normal;
    sceVu0Normalize(normal, direction);
    sceVu0FMATRIX dir = {
        {normal[0], 0.0f, 0.0f, 0.0f},
        {normal[1], 0.0f, 0.0f, 0.0f},
        {normal[2], 0.0f, 0.0f, 0.0f},
        {0.0f,      0.0f, 0.0f, 0.0f}
    };
    sceVu0FMATRIX col = {
        {96.0f, 96.0f, 96.0f, 0.0f},
        {0.0f,  0.0f,  0.0f,  0.0f},
        {0.0f,  0.0f,  0.0f,  0.0f},
        {0.0f,  0.0f,  0.0f,  0.0f}
    };
    MGSetPLight(dir, col);

    for (int i = 0; i < 10; i++, x += 0xD6, depth += 16.0f) {
        int draw_alpha = alpha;

        if (x < -0x60 || (WepMenu.mode >= WEP_MENU_ATTACH_WEAPON && i != cursor)) {
            continue;
        }
        if (x >= 0x277) {
            break;
        }

        WEAPON_HAVE *weapon = &DngWepHavePt[i];
        const int    is_equipped = (i == equipped_slot) ? 1 : 0;
        const int    is_selected = (i == cursor) ? 1 : 0;

        if (is_selected != 0) {
            if (WepMenu.state == WEP_STATE_STATUS_BREAK_EFFECT) {
                if (MenuWepLevelUp.effect_state == WEP_EFFECT_STATE_BREAK_PLAY) {
                    draw_alpha = std::max(0, static_cast<int>(128.0f - 4.0f * MenuWepLevelUp.effect_timer));
                } else if (MenuWepLevelUp.effect_state == WEP_EFFECT_STATE_BREAK_DONE) {
                    draw_alpha = MenuWepLevelUp.lost_default_weapon ? 0 : std::min(0x80, static_cast<int>(MenuWepLevelUp.effect_timer));
                }
            }
            MenuMes.alpha = draw_alpha;
        } else {
            if (WepMenu.mode > WEP_MENU_LIST || WepMenu.state >= WEP_STATE_LEVELUP_EFFECT) {
                continue;
            }
        }

        bright[3] = static_cast<float>(draw_alpha);
        MGSetAmbient(bright);

        if (MenuExTextureReadFlag != MENU_EX_TEX_READY) {
            continue;
        }

        if (weapon->item_no >= ITEM_WEAPON_START) {
            CCharacter *frame = nullptr;
            if (GetNowTestNo() == 1) {
                if (is_equipped != 0) {
                    MenuTextureReload(MenuShadowReadBlock);
                    constexpr const char *shadows[6] = {"kagetoan", "kagesyao", "kagegoro", "kageruby", "kageunga", "kageozu"};
                    DrawMenu2DSprite(TexManager.GetTexture(const_cast<char *>(shadows[static_cast<int>(WepMenu.chara)]), -1),
                                     CRect_i_(x + 0x24, 0x96, 0x64, 0x63),
                                     CRect_i_(0, 0, 0x64, 0x64), draw_alpha * 9 / 40);
                    MenuTextureReload(MenuExtendReadBlock);
                }

                const int frame_no = GetMenuWeaponModelFrameNo(i);
                if (frame_no >= 0) {
                    frame = &DngWeaponFrm[frame_no];
                    MenuWeaponSpSet(frame, weapon);
                }
            }

            if (frame != nullptr) {
                DrawAallWeapon(x, 0xA0, depth, frame, weapon, is_equipped, is_selected, draw_alpha);
            }
        } else {
            if (is_selected == 0 || WepMenu.state != WEP_STATE_STATUS_BREAK_EFFECT) {
                DrawMenuNothing(x - 4, 0x82, 0xB4, 0x96, const_cast<char *>("wepstatus"), 0, draw_alpha);
            }
        }

        MenuMes.mode = MENU_CLSMES_OFF;

        switch (WepMenu.mode) {
            case WEP_MENU_ATTACH_TAGS:
            case WEP_MENU_ATTACH_SOCKETS:
            case WEP_MENU_ATTACH_BOARD:
            case WEP_MENU_ATTACH_WEAPON:
                MenuTextureReload(BtlMenuReadBlock);
                DrawBtlMenuLRCursor(x - 0x1C, 0xE6, 0xD4, draw_alpha);
                DrawWeaponTagBoardPort(x - 0xC, 0xF0, weapon, 0, 0, draw_alpha);
                [[fallthrough]];
            case WEP_MENU_LIST:
                setbilinear(0);
                MenuTextureReload(BtlMenuReadBlock);
                WeaponOptionStatusDraw(weapon, x + 0x50, 0x8E, draw_alpha);
                break;
            default:
                MenuMes.mode = MENU_CLSMES_OPTIONS;
                break;
        }
    }

    MGSetPLight(light_dir, light_col);
    MGSetAmbient(ambient);

    if (MenuExTextureReadFlag == MENU_EX_TEX_READY) {
        const bool tags = WepMenu.mode == WEP_MENU_ATTACH_TAGS || WepMenu.mode == WEP_MENU_ATTACH_WEAPON ||
                          WepMenu.mode == WEP_MENU_ATTACH_SOCKETS || WepMenu.mode == WEP_MENU_ATTACH_BOARD;
        WeaponNameDrawPort(0x72, tags ? 1 : 0, alpha);
    }

    switch (WepMenu.mode) {
        case WEP_MENU_ATTACH_TAGS:
        case WEP_MENU_ATTACH_SOCKETS:
        case WEP_MENU_ATTACH_BOARD:
        case WEP_MENU_ATTACH_WEAPON:
            break;
        default: {
            const int dim = (WepMenu.mode != WEP_MENU_LIST) ? 1 : 0;
            FadeTexX(0, 0xCC, 0x1AE, 0xD2, const_cast<char *>("frame_image"), dim);
            break;
        }
    }

    if (WepMenu.state != WEP_STATE_STATUS_BREAK_EFFECT) {
        MenuMes.Step();
        MenuMes.Draw1(MenuMes.draw_x, MenuMes.draw_y, MenuMes.alpha);
    }
}

void PortWeaponMenuDraw() {
    RefreshBtlMenuTextures();
    int alpha = 0x80;

    if (WepMenu.state == WEP_STATE_OPEN) {
        alpha = std::min(0x80, WepMenu.counter * 9);
    } else if (WepMenu.state == WEP_STATE_CLOSE) {
        alpha = std::max(0, 0x80 - WepMenu.counter * 9);
    }

    int attach_mode = 0;
    switch (WepMenu.mode) {
        case WEP_MENU_ATTACH_WEAPON:
        case WEP_MENU_ATTACH_SOCKETS:
        case WEP_MENU_ATTACH_BOARD:
        case WEP_MENU_ATTACH_TAGS:
            attach_mode = 1;
            [[fallthrough]];
        case WEP_MENU_LIST:
        case WEP_MENU_ACTION:
        case WEP_MENU_ELEMENT:
        case WEP_MENU_LEVELUP_CONFIRM:
        case WEP_MENU_STATUS_BREAK_CONFIRM:
        case WEP_MENU_BUILDUP_SELECT:
        case WEP_MENU_BUILDUP_CONFIRM:
        case WEP_MENU_BUILDUP_PREVIEW:
            for (int i = 0; i < 6; i++) {
                const int target = (SysChara[i].chara - WepMenu.chara) * 0x6E + 0x10E;
                SysChara[i].x += (static_cast<float>(target) - SysChara[i].x) / 4.0f;

                if (attach_mode == 0 || i == WepMenu.chara) {
                    CTexture *face = MenuCharaFace;
                    int       face_u = 0;
                    const int face_char = SysChara[i].chara;
                    int       face_v = face_char * 0x6A;

                    if (face_char > 2) {
                        face_u = 0x6A;
                        face_v = (face_char - 3) * 0x6A;
                    }
                    if (BtlMenuStatusPt->party_size <= i) {
                        face = BtStatus;
                        face_v = 0x8C;
                        face_u = 0x8C;
                    }

                    DrawMenu2DSprite(face, CRect_i_(SysChara[i].x, 0xE, 0x64, 0x64),
                                     CRect_i_(face_u, face_v, 0x6A, 0x6A), alpha, alpha, alpha, alpha);
                }
            }

            FadeTexX(0xDC, 0x32, 0x172, 0x32, const_cast<char *>("frame_image"), 0);

            if (attach_mode == 0) {
                static int arrow_ct = 0;
                int        arrow_alpha = alpha;
                if (WepMenu.mode == WEP_MENU_LIST) {
                    arrow_ct = (arrow_ct + 1) % 1000000;
                } else {
                    arrow_alpha = 0;
                }
                if (arrow_ct % 50 < 30) {
                    const int step = (arrow_ct % 30) / 10 * 2;
                    CRect_i_  arrow(0x60, 0x68, 10, 0x18);
                    DrawMenu2DSprite(BtStatus, CRect_i_(0x102 - step, 0x48, arrow.width, arrow.height), arrow, arrow_alpha);
                    arrow.x += arrow.width;
                    DrawMenu2DSprite(BtStatus, CRect_i_(step + 0x174, 0x48, arrow.width, arrow.height), arrow, arrow_alpha);
                }
            }
            break;
        default:
            break;
    }

    int   target_x = 0;
    float target_depth = 0.0f;
    if (WepMenu.mode >= WEP_MENU_ATTACH_WEAPON) {
        const int slot = WepMenu.weapon_slot;
        target_x = static_cast<int>(80.0f - static_cast<float>(slot * 0xD6));
        target_depth = 2.0f + (-11.0f + 16.0f * -slot);
    } else {
        target_x = 0xE8 - WepMenu.weapon_slot * 0xD6;
        target_depth = 2.0f + 16.0f * -WepMenu.weapon_slot;
    }

    WeaponPos += (static_cast<float>(target_x) - WeaponPos) / 4.0f;
    WepPolyPos += (target_depth - WepPolyPos) / 4.0f;

    int weapon_alpha = alpha;
    if (WepMenu.state == WEP_STATE_UNK_3) {
        weapon_alpha = alpha - 9;
    } else if (WepMenu.state == WEP_STATE_CHARA_CHANGE) {
        weapon_alpha = WepMenu.counter * 5;
    }
    weapon_alpha = std::clamp(weapon_alpha, 0, 0x80);

    BtlWeaponDraw(static_cast<int>(WeaponPos), WepPolyPos, WepMenu.chara, weapon_alpha);

    if (attach_mode == 0) {
        constexpr u8 repair_x[7] = {78, 78, 78, 26, 64, 64, 66};
        RepairAndLevelUpDraw(repair_x[BtlMenuNowLang], 0x164, alpha);
    }

    constexpr int    dialog_pos = 0x70;
    WEP_BUILDUP_INFO builds[4]{};

    switch (WepMenu.mode) {
        case WEP_MENU_LEVELUP_CONFIRM:
            DrawWeaponLeftConfirmDialog(0x46, 0x8C, 0x88, 0xDD,
                                        LocalizeText("weaponmenu.upgrade.confirm", "Upgrade. Confirm?"), 0x80);
            break;
        case WEP_MENU_BUILDUP_CONFIRM:
            DrawWeaponLeftConfirmDialog(0x46, 0x8C, 0x88, 0xDD,
                                        LocalizeText("weaponmenu.buildup.confirm", "Build-up. Confirm?"), 0x80);
            break;
        case WEP_MENU_STATUS_BREAK_CONFIRM:
            DrawWeaponLeftConfirmDialog(0x46, 0x88, 0x88, 0xF3,
                                        LocalizeText("weaponmenu.break.confirm", "Status break this weapon?\nWeapon disappears."), 0x80);
            break;
        case WEP_MENU_BUILDUP_PREVIEW:
            AllFadeForMenu(0x40);
            MenuTextureReload(BtlMenuReadBlock);
            DrawBtlMenuLRCursor(0x158, 0x6E, 0xD4, alpha);
            EnableBuildUpModel(builds, GetNowSelectWeaponPort());
            DrawWeaponTagBoardPort(0x16C, 0x78, GetNowSelectWeaponPort(), 1, builds[WepMenu.board.cursor].weapon_no, 0x80);
            [[fallthrough]];
        case WEP_MENU_BUILDUP_SELECT:
            DrawBuildUpWeaponSelect(0x58, 0x9C, WepMenu.board.cursor);
            break;
        case WEP_MENU_ELEMENT:
            DrawWeaponTagBoardPort(0x18A, 0x64, GetNowSelectWeaponPort(), 0, 0, 0x80);
            [[fallthrough]];
        case WEP_MENU_ACTION:
            if (GetNowSelectWeaponPort() != nullptr) {
                DrawWeaponSelectDialog(dialog_pos, dialog_pos, weapon_alpha);
            }
            break;
        case WEP_MENU_LIST:
            if (BtlMenuStatusPt->party_size > 1) {
                MenuTextureReload(BtlMenuReadBlock);
                DrawBtlMenuLRCursor(0xE6, 0x12, 0x96, alpha);
            }
            break;
        case WEP_MENU_ATTACH_WEAPON:
        case WEP_MENU_ATTACH_SOCKETS:
        case WEP_MENU_ATTACH_BOARD:
        case WEP_MENU_ATTACH_TAGS:
            MenuTextureReload(BtlMenuReadBlock);
            DrawPersonalBoard(0x154, 0x78, WepMenu.board.page, alpha, 0);
            CommonTrushDraw(0x232, 0x10C, alpha);
            if (BtlHaveItemPt->item_no >= ITEM_ATTACH_START) {
                MenuTextureReload(MenuShadowReadBlock);
                DrawMenuVibeItem(static_cast<int>(SysCur[0]), static_cast<int>(SysCur[1]), 2, -0xC, 0x80);
            }
            break;
        default:
            break;
    }

    const int slot = std::clamp(static_cast<int>(WepMenu.weapon_slot), 0, 9);
    WepMenu.weapon_slot = static_cast<s8>(slot);

    const int holes = GetWeaponHoleNum(DngWepHavePt[slot].item_no);
    s16       cursor_pos[12][2] = {
        {0xC0,  0xA5},
        {0x50,  0   },
        {0x16E, 0   },
        {0x68,  0   },
        {0x68,  0   },
        {0x4E,  0   },
        {0x68,  0   },
        {0x4E,  0   },
        {0x58,  0xAA},
        {0,     0xB8},
        {0,     0   },
        {0,     0   }
    };
    const s16 row = static_cast<s16>(WepMenu.board.cursor * 0x1A);
    cursor_pos[1][1] = dialog_pos + row;
    cursor_pos[2][1] = static_cast<s16>(WepMenu.element) * 0x18 + 0x86;
    const s16 dialog_y = row + 0xDC;
    cursor_pos[3][1] = dialog_y;
    cursor_pos[4][1] = row + 0xF2;
    const s16 build_y = CommonMenuMes1.char_height + static_cast<s16>(WepMenu.board.cursor * 0x26 + 0xBA);
    cursor_pos[5][1] = build_y;
    cursor_pos[6][1] = dialog_y;
    cursor_pos[7][1] = build_y;
    cursor_pos[9][0] = (0x90 - holes * 0x12) + static_cast<s16>(WepMenu.board.cursor * 0x2A);

    int cursor_x = cursor_pos[WepMenu.mode][0];
    int cursor_y = cursor_pos[WepMenu.mode][1];

    if (WepMenu.state == WEP_STATE_SCRAP_PROMPT) {
        cursor_x = 0xB4;
        cursor_y = dialog_pos + 0x2E;
    }
    if (WepMenu.state == WEP_STATE_SCRAP_CONFIRM) {
        cursor_x = 0x171;
        cursor_y = static_cast<int>(WepMenu.confirm[0]) * 0x1A + 0xBE;
    }

    RECT frames[12] = {
        {0xCD, 0x64, 0xCC, 0xC9},
        {0,    0,    0x60, 0x18},
        {0,    0,    0xC4, 0x18},
        {0,    0,    0x60, 0x18},
        {0,    0,    0x60, 0x18},
        {0,    0,    0xC0, 0x20},
        {0,    0,    0x60, 0x18},
        {0,    0,    0xC0, 0x20},
        {0,    0,    0x6E, 0x6E},
        {0,    0,    0x28, 0x28},
        {0,    0,    0x26, 0x26},
        {0xD8, 0x76, 0xB8, 0x98}
    };
    frames[1].x = cursor_x + 0x19;
    frames[1].y = cursor_y - 6;
    frames[2].x = cursor_x + 0x1A;
    frames[2].y = cursor_y - 6;
    frames[3].x = cursor_x + 0x18;
    frames[3].y = cursor_y - 6;
    frames[4].x = cursor_x + 0x18;
    frames[4].y = cursor_y - 6;
    frames[5].x = cursor_x + 0x18;
    frames[5].y = cursor_y - 8;
    frames[6].x = cursor_x + 0x18;
    frames[6].y = cursor_y - 6;
    frames[7].x = cursor_x + 0x18;
    frames[7].y = cursor_y - 8;
    frames[8].x = cursor_x + 0x12;
    frames[8].y = cursor_y - 0x18;
    frames[9].x = cursor_x + 0xC;
    frames[9].y = cursor_y - 8;
    frames[10].x = cursor_x + 0xC;
    frames[10].y = cursor_y - 8;

    RECT         frame = frames[WepMenu.mode];
    WEAPON_HAVE *selected = GetNowSelectWeaponPort();
    if (selected != nullptr && selected->item_no < ITEM_WEAPON_START) {
        frame = frames[11];
    }

    if (WepMenu.state == WEP_STATE_SCRAP_PROMPT) {
        frame.x = cursor_x + 0x19;
        frame.y = cursor_y - 4;
        frame.width = 0x60;
        frame.height = 0x18;
    } else if (WepMenu.state == WEP_STATE_SCRAP_CONFIRM) {
        frame.x = 0x190 - 6;
        frame.y = cursor_y - 4;
        frame.width = 0x60;
        frame.height = 0x18;
    }

    switch (WepMenu.mode) {
        case WEP_MENU_ACTION:
            if (WepMenu.board.cursor >= WEP_ACTION_LEVELUP &&
                WepMenu.state != WEP_STATE_SCRAP_PROMPT &&
                WepMenu.state != WEP_STATE_SCRAP_CONFIRM) {
                int       offset_y = 0;
                const int options = NowWeaponStatusValuePort(&DngWepHavePt[static_cast<int>(WepMenu.weapon_slot)]);
                for (int i = 4; WepMenu.board.cursor >= i; i++) {
                    if (options & (1 << (i - 3))) {
                        offset_y += 0x1A;
                    }
                }
                cursor_y = dialog_pos + 0x4E + offset_y;
                frame.y = cursor_y - 6;
            }
            break;
        case WEP_MENU_ATTACH_BOARD:
            if (WepMenu.board.cursor_area == PERSONAL_BOARD_AREA_CELLS) {
                int cell = WepMenu.board.cursor;
                cursor_x = (cell % 5) * 0x28 + 0x154;
                cell -= WepMenu.board.top_row * 5;
                cursor_y = (cell / 5) * 0x28 + 0x7E;
            } else if (WepMenu.board.cursor_area == PERSONAL_BOARD_AREA_TRASH) {
                cursor_x = 0x220;
                cursor_y = 0x110;
            }
            frame.x = cursor_x + 0xC;
            frame.y = cursor_y - 8;
            frame.height = 0x26;
            frame.width = 0x26;
            break;
        case WEP_MENU_ATTACH_TAGS: {
            const int sel = WepMenu.tag_row;
            if (WepMenu.tag_page == WEP_TAG_STATUS) {
                if (sel < 2) {
                    cursor_x = 0x24;
                    cursor_y = sel * 0x14 + 0x114;
                    frame.width = 0xCA;
                } else {
                    cursor_x = 0x2A;
                    cursor_y = (sel - 2) * 0x10 + 0x148;
                    frame.width = 0xC4;
                }
            } else if (WepMenu.tag_page == WEP_TAG_ELEMENT) {
                cursor_x = 0x2A;
                cursor_y = sel * 0x18 + 0x112;
                frame.width = 0xC4;
            } else if (WepMenu.tag_page == WEP_TAG_VS_MONSTER) {
                cursor_x = (sel / 5) * 0x68 + 0x28;
                cursor_y = (sel % 5) * 0x18 + 0x112;
                frame.width = 0x66;
            }
            frame.x = cursor_x + 0x1C;
            frame.y = cursor_y - 6;
            frame.height = 0x18;
            break;
        }
        default:
            break;
    }

    DNG_CONSUMABLE *consumables = BtlMenuStatusPt->consumable_items;
    int             cursor_alpha = 0x40;
    if (WepMenu.mode > WEP_MENU_LIST) {
        if (BtlHaveItemPt->item_no >= ITEM_ATTACH_START) {
            cursor_x += 0x12;
            cursor_y += 0xE;
            cursor_alpha = 0x80;
        } else if ((WepMenu.mode == WEP_MENU_ATTACH_SOCKETS && DngWepHavePt[slot].attach[WepMenu.board.cursor].item_no >= ITEM_ATTACH_START) ||
                   (WepMenu.mode == WEP_MENU_ATTACH_BOARD && consumables[WepMenu.board.cursor].id >= ITEM_ATTACH_START)) {
            cursor_x += 10;
            cursor_y += 0x18;
            cursor_alpha = 0x60;
        }
    }

    SysCur[0] += (static_cast<float>(cursor_x) - SysCur[0]) / 4.0f;
    SysCur[1] += (static_cast<float>(cursor_y) - SysCur[1]) / 4.0f;

    switch (WepMenu.state) {
        case WEP_STATE_OPEN:
        case WEP_STATE_CLOSE:
        case WEP_STATE_CHARA_CHANGE:
        case WEP_STATE_LEVELUP_EFFECT:
        case WEP_STATE_STATUS_BREAK_EFFECT:
        case WEP_STATE_BUILDUP_EFFECT:
        case WEP_STATE_REPAIR_EFFECT:
            CommonMenuMes1.auto_pos = MES_POS_NONE;
            return;
        case WEP_STATE_WARNING:
        case WEP_STATE_SCRAP_REFUSED:
            DrawWeaponCenteredWarningModal(0x80);
            return;
        default: {
            const int shadow = (WepMenu.mode == WEP_MENU_LIST) ? 1 : 0;

            if (WepMenu.state == WEP_STATE_SCRAP_CONFIRM) {
                DrawWeaponLeftConfirmDialog(0x154, 0x76, 0x190, 0xBE,
                                            LocalizeText("weaponmenu.discard.confirm", "Throw away this weapon?"), 0x80);
            }

            setbilinear(0);
            MenuTextureReload(BtlMenuReadBlock);
            DrawMenuWaku(frame.x, frame.y, frame.width, frame.height, shadow, StayTex, 0x80);
            const int big = (cursor_alpha >= 0x80) ? 0 : 1;
            DrawMenuObjectVibe(static_cast<int>(SysCur[0]), static_cast<int>(SysCur[1]), big, cursor_alpha);
            return;
        }
    }
}

} // namespace dc::battlemenu
