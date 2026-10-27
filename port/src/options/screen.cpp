#include "options/screen.hpp"

#include <algorithm>
#include <format>
#include <memory>
#include <optional>

#include "app/gameloop.hpp"
#include "battlemenu/strings.hpp"
#include "clsmes.hpp"
#include "dataread.hpp"
#include "dialog/framework.hpp"
#include "gamepad.hpp"
#include "gameutil.hpp"
#include "gfx/gfx.hpp"
#include "localize/localize.hpp"
#include "mainselect.hpp"
#include "memcard.hpp"
#include "menu_draw.hpp"
#include "menu_manual.hpp"
#include "menuetc.hpp"
#include "platform/display.hpp"
#include "platform/input.hpp"
#include "platform/window.hpp"
#include "quest/quests.hpp"
#include "snd.hpp"
#include "sound.hpp"
#include "texture.hpp"
#include "title/title_menu.hpp"
#include "ui/area_card.hpp"
#include "ui/framework.hpp"

namespace dc::options {

using namespace dc::game;
using namespace dc::battlemenu;
using namespace dc::localize;
using namespace dc::platform;
using namespace dc::quest;

Screen g_screen;

namespace {

constexpr int kRepeatDelay = 30;
constexpr int kRepeatStep = 5;
constexpr int kBarFrames = 75;

constexpr int kDisplayNowHelp = 995;
constexpr int kDisplayHelp = 996;
constexpr int kSaveHelp = 997;
constexpr int kExitHelp = 999;
constexpr int kPageHelp = 900;
constexpr int kRowHelp = 1000;

constexpr const char *kGlyphL1 = "{L1}";
constexpr const char *kGlyphR1 = "{R1}";

constexpr const char *kShortcutsText = "{square} Reset page\n{triangle} Undo changes\n{circle} Close";
constexpr const char *kSaveHelpText = "Could not save.\nChanges apply now.\nClose to retry writing\nconfig.json.";
constexpr const char *kDisplayHelpText = "The display could not\nchange, and kept the\nmode it had. Try\nanother mode or size.";
constexpr const char *kTurnPageText = "{L1} {R1} turn the page.";
constexpr const char *kExitHelpText =
    "{cross} Close\n{square} This page's defaults\n{triangle} Undo every change\n{circle} Close from any row";
constexpr const char *kDisplayNowText = "The display could not\nchange as asked. It is\nnow %1";
constexpr const char *kFullscreenNowText = "fullscreen.";
constexpr const char *kWindowNowText = "a %1 x %2\nwindow.";

int HelpIndex(int page, int row) {
    int index = row;
    for (int p = 0; p < page; ++p) {
        index += RowCount(p);
    }
    return index;
}

const Row *CurrentRow() {
    return g_screen.row >= 0 && g_screen.row < RowCount(g_screen.page) ? &CurrentPage().rows[g_screen.row] : nullptr;
}

void ShowBar() {
    if (Scrolls(g_screen.page)) {
        g_screen.bar_frames = kBarFrames;
    }
}

void ScrollTo(int first) {
    int most = std::max(0, RowCount(g_screen.page) - kVisibleRows);
    first = std::clamp(first, 0, most);
    if (first != FirstRow()) {
        g_screen.first_row[g_screen.page] = first;
        ShowBar();
    }
}

void CursorToScroll() {
    if (CurrentRow() != nullptr) {
        g_screen.row = std::clamp(g_screen.row, FirstRow(), FirstRow() + kVisibleRows - 1);
    }
}

void ScrollToCursor() {
    if (CurrentRow() == nullptr) {
        return;
    }
    if (g_screen.row < FirstRow()) {
        ScrollTo(g_screen.row);
    } else if (g_screen.row >= FirstRow() + kVisibleRows) {
        ScrollTo(g_screen.row - kVisibleRows + 1);
    }
}

void ShowPageTab() {
    g_screen.first_tab = std::min(g_screen.first_tab, g_screen.page);
    while (g_screen.page > LastVisibleTab()) {
        ++g_screen.first_tab;
    }
}

void Apply(const Config &config) {
    g_screen.save_failed = !DisplayApply([&] { return ConfigChange(config); });
    ListResolutions();
}

void FollowWindow() {
    if (std::optional<bool> saved = DisplayTakePumpSave()) {
        g_screen.save_failed = !*saved;
        ListResolutions();
    }
}

void Step(const Row &row, int direction, bool wrap) {
    Config config = ConfigGet();
    g_screen.last_row_dir = (direction < 0) ? -1 : 1;
    if (StepRow(row, config, direction, wrap)) {
        Apply(config);
        ComMenuSePlay(MENU_SOUND_CURSOR);
    }
}

void ShowHelp() {
    ClsMes &mes = CommonMenuMes2;
    short  *buffer = GetTexts().help.Data();
    int     message = kExitHelp;
    if (DisplayGetWarning() == DisplayWarning::Kept) {
        message = kDisplayHelp;
    } else if (DisplayGetWarning() == DisplayWarning::Changed) {
        WindowMode  mode = DisplayShownMode();
        std::string now = mode.fullscreen ? LocalizeText("options.display.fullscreen_now", kFullscreenNowText)
                                          : LocalizeFormat(LocalizeText("options.display.window_now", kWindowNowText),
                                                           {std::to_string(mode.width), std::to_string(mode.height)});
        if (now != g_screen.display_now) {
            g_screen.display_now = now;
            GetTexts().help.Set(kDisplayNowHelp,
                                LocalizeFormat(LocalizeText("options.help.display_now", kDisplayNowText), {now}));
            buffer = GetTexts().help.Data();
            mes.mes_made = -1;
        }
        message = kDisplayNowHelp;
    } else if (g_screen.save_failed) {
        message = kSaveHelp;
    } else if (g_screen.row == kOnTabs) {
        message = kPageHelp + g_screen.page;
    } else if (const Row *row = CurrentRow()) {
        if (row->help != nullptr) {
            message = kRowHelp + HelpIndex(g_screen.page, g_screen.row);
        } else {
            buffer = g_screen.game_messages;
            message = row->game_help;
        }
    }
    if (mes.buff != buffer || mes.mes_made != message) {
        mes.SetBuff(buffer);
        mes.MakeMesWin(message);
    }
}

void Close() {
    g_screen.binding_row = -1;
    g_screen.binding_wait.clear();
    g_screen.save_failed = !ConfigChange(ConfigGet());
    g_screen.step = OPTION_STEP_FADE_OUT;
    g_screen.step_count = 0;
    InputSetMenuMouse(false);
    ComMenuSePlay(MENU_SOUND_REFUSE);
}

std::uint16_t HeldButtons() {
    const InputPadState &pad = InputGetPad(0);
    std::uint16_t        held = pad.buttons;
    int                  x = AxisCalibration(pad.left_x);
    int                  y = AxisCalibration(pad.left_y);
    if (x > 120) {
        held |= PAD_RIGHT;
    } else if (x < -120) {
        held |= PAD_LEFT;
    }
    if (y > 120) {
        held |= PAD_DOWN;
    } else if (y < -120) {
        held |= PAD_UP;
    }
    return held;
}

int Pressed(std::uint16_t held, int mask, bool repeat) {
    int fired = 0;
    for (int bit = 0; bit < 16; ++bit) {
        int button = 1 << bit;
        if ((mask & button) == 0) {
            continue;
        }
        if ((held & button) == 0) {
            g_screen.held[bit] = 0;
            continue;
        }
        int frames = ++g_screen.held[bit];
        if (frames == 1 || (repeat && frames > kRepeatDelay && (frames - kRepeatDelay) % kRepeatStep == 0)) {
            fired |= button;
        }
    }
    return fired;
}

void RunBindingCapture() {
    const Row *row = CurrentRow();
    if (row == nullptr || row->action == nullptr) {
        g_screen.binding_row = -1;
        g_screen.binding_wait.clear();
        return;
    }
    if (!g_screen.binding_wait.empty()) {
        if (InputBindSourceHeld(g_screen.binding_wait)) {
            g_screen.binding_prompt = InputBindingLabel(row->action);
            return;
        }
        g_screen.binding_wait.clear();
        g_screen.binding_row = -1;
        return;
    }
    if ((Pressed(HeldButtons(), PAD_CIRCLE, false) & PAD_CIRCLE) != 0) {
        g_screen.binding_row = -1;
        ComMenuSePlay(MENU_SOUND_REFUSE);
        return;
    }
    std::string key = InputTakeBindKey(InputActionTakesGamepad(row->action));
    if (key.empty()) {
        return;
    }
    if (key == "Escape") {
        g_screen.binding_row = -1;
        ComMenuSePlay(MENU_SOUND_REFUSE);
        return;
    }
    Config config = ConfigGet();
    SetBinding(config, row->action, key);
    Apply(config);
    g_screen.binding_prompt = InputBindingLabel(row->action);
    g_screen.binding_wait = key;
    ComMenuSePlay(MENU_SOUND_CONFIRM);
}

void BeginBinding(int row) {
    g_screen.binding_row = row;
    g_screen.binding_prompt = "Press a key...";
    g_screen.binding_wait.clear();
    InputBeginBindCapture();
    ComMenuSePlay(MENU_SOUND_CONFIRM);
}

void SetPage(int page) {
    int  count = static_cast<int>(Pages().size());
    bool on_exit = g_screen.row == RowCount(g_screen.page);
    if (page < g_screen.page) {
        g_screen.last_tab_dir = -1;
    } else if (page > g_screen.page) {
        g_screen.last_tab_dir = 1;
    }
    g_screen.page = (page + count) % count;
    ShowPageTab();
    if (g_screen.row != kOnTabs) {
        g_screen.row = on_exit ? RowCount(g_screen.page) : std::min(g_screen.row, RowCount(g_screen.page) - 1);
        ScrollToCursor();
    }
}

bool Inside(float x, float y, int left, int top, int right, int bottom) {
    return x >= left && x < right && y >= top && y < bottom;
}

void RunMouse() {
    InputMenuMouse mouse = InputTakeMenuMouse();
#ifdef __ANDROID__
    if (InputTouchOverlayVisible()) {
        mouse.dx = mouse.dy = mouse.wheel = 0.0f;
    }
#endif
    std::uint32_t clicked = mouse.buttons & ~g_screen.mouse_buttons;
    bool          moved = mouse.dx != 0.0f || mouse.dy != 0.0f;
    g_screen.mouse_buttons = mouse.buttons;

    if (g_screen.binding_row >= 0) {
        return;
    }
    if ((clicked & 2) != 0) {
        Close();
        return;
    }
    if (moved || clicked != 0) {
        if (!g_screen.pointing) {
            g_screen.pointing = true;
            g_screen.pointer_x = g_screen.cursor_x + 26.0f;
            g_screen.pointer_y = g_screen.cursor_y + 14.0f;
        }
        int   width = 0;
        int   height = 0;
        float scale = WindowSize(width, height) ? 1.0f / std::min(width / 640.0f, height / 480.0f) : 1.0f;
        g_screen.pointer_x = std::clamp(g_screen.pointer_x + mouse.dx * scale, 0.0f, 639.0f);
        g_screen.pointer_y = std::clamp(g_screen.pointer_y + mouse.dy * scale, 0.0f, 479.0f);
    }
    if (!g_screen.pointing) {
        return;
    }
    if (moved) {
        ShowBar();
    }

    float x = g_screen.pointer_x;
    float y = g_screen.pointer_y;
    int   rows = RowCount(g_screen.page);
    int   old_row = g_screen.row;
    int   old_page = g_screen.page;

    if (mouse.wheel != 0.0f && Inside(x, y, kLabelX - 8, kRowY, kBarX + 16, kRowY + kVisibleRows * kRowStep)) {
        ScrollTo(FirstRow() + (mouse.wheel > 0.0f ? -1 : 1));
        CursorToScroll();
        moved = true;
    }

    int over_row = -2;
    int over_tab = -1;
    for (int r = FirstRow(); r < std::min(rows, FirstRow() + kVisibleRows); ++r) {
        if (Inside(x, y, kLabelX - 8, RowTop(r) - 1, kValueRight + 8, RowTop(r) + kRowStep - 1)) {
            over_row = r;
        }
    }
    if (Inside(x, y, kExitX, kExitY, kExitX + kExitWidth, kExitY + kExitHeight)) {
        over_row = rows;
    }
    for (int t = g_screen.first_tab; t <= LastVisibleTab(); ++t) {
        if (Inside(x, y, TabX(t) - 8, kTabY - 6, TabX(t) + GetTexts().tabs[t].GetWidth() + 8, kTabY + 22)) {
            over_tab = t;
        }
    }
    int   bar_bottom = kRowY + kVisibleRows * kRowStep;
    Glyph glyph = Glyph::None;
    if (over_tab < 0 && Inside(x, y, L1X() - 4, kTabY - 6, kLabelX + kTabArrow, kTabY + 22)) {
        glyph = Glyph::L1;
    } else if (over_tab < 0 && Inside(x, y, kTabsRight - kTabArrow, kTabY - 6, R1X() + GetTexts().r1.GetWidth() + 4,
                                      kTabY + 22)) {
        glyph = Glyph::R1;
    } else if (Scrolls(g_screen.page) && Inside(x, y, kBarX - 6, kRowY - 4, kBarX + 14, kRowY + 14)) {
        glyph = Glyph::Up;
    } else if (Scrolls(g_screen.page) && Inside(x, y, kBarX - 6, bar_bottom - 18, kBarX + 14, bar_bottom)) {
        glyph = Glyph::Down;
    }
    g_screen.glyph = glyph;
    if (glyph == Glyph::Up || glyph == Glyph::Down) {
        ShowBar();
    }

    if (moved) {
        if (over_row != -2) {
            g_screen.row = over_row;
        } else if (over_tab >= 0 || glyph == Glyph::L1 || glyph == Glyph::R1) {
            g_screen.row = kOnTabs;
        }
    }

    if ((clicked & 1) != 0) {
        if (over_tab >= 0) {
            SetPage(over_tab);
        } else if (glyph == Glyph::L1 || glyph == Glyph::R1) {
            SetPage(g_screen.page + (glyph == Glyph::L1 ? -1 : 1));
        } else if (glyph == Glyph::Up || glyph == Glyph::Down) {
            ScrollTo(FirstRow() + (glyph == Glyph::Up ? -1 : 1));
            CursorToScroll();
        } else if (over_row == rows) {
            Close();
            return;
        } else if (over_row >= 0) {
            const Row &row = CurrentPage().rows[over_row];
            if (row.activate != nullptr) {
                if (x < kValueX) {
                    row.activate();
                }
            } else if (row.action != nullptr) {
                BeginBinding(over_row);
            } else if (x >= kValueX - 8) {
                int value_center = (kValueX + kValueRight) / 2;
                Step(row, x < value_center ? -1 : 1, false);
            }
        }
    }

    if (g_screen.page != old_page || g_screen.row != old_row) {
        ShowBar();
        ComMenuSePlay(MENU_SOUND_CURSOR);
    }
}

void RunKeys() {
    if (g_screen.binding_row >= 0) {
        RunBindingCapture();
        return;
    }
    int           old_page = g_screen.page;
    int           old_row = g_screen.row;
    std::uint16_t held = HeldButtons();
    int           moves = Pressed(held, PAD_DPAD, true);
    int           pages = Pressed(held, PAD_L1 | PAD_L2 | PAD_R1 | PAD_R2, false);
    int           actions = Pressed(held, PAD_CROSS | PAD_CIRCLE | PAD_SQUARE | PAD_TRIANGLE, false);

    if (moves != 0 || pages != 0 || actions != 0) {
        g_screen.pointing = false;
    }
    if ((pages & (PAD_L1 | PAD_L2)) != 0) {
        SetPage(g_screen.page - 1);
    } else if ((pages & (PAD_R1 | PAD_R2)) != 0) {
        SetPage(g_screen.page + 1);
    }
    int rows = RowCount(g_screen.page);

    if ((moves & PAD_DOWN) != 0) {
        g_screen.row = g_screen.row == rows ? kOnTabs : g_screen.row + 1;
    } else if ((moves & PAD_UP) != 0) {
        g_screen.row = g_screen.row == kOnTabs ? rows : g_screen.row - 1;
    }
    ScrollToCursor();

    int direction = 0;
    if ((moves & PAD_LEFT) != 0) {
        direction = -1;
    } else if ((moves & PAD_RIGHT) != 0) {
        direction = 1;
    }

    if ((actions & PAD_CIRCLE) != 0) {
        Close();
    } else if ((actions & PAD_SQUARE) != 0) {
        Config config = ConfigGet();
        ResetPage(CurrentPage(), config);
        Apply(config);
        ComMenuSePlay(MENU_SOUND_CONFIRM);
    } else if ((actions & PAD_TRIANGLE) != 0) {
        Apply(g_screen.opened);
        ComMenuSePlay(MENU_SOUND_CONFIRM);
    } else if (g_screen.row == kOnTabs) {
        if (direction != 0) {
            SetPage(g_screen.page + direction);
        }
    } else if (const Row *row = CurrentRow()) {
        if (direction != 0) {
            Step(*row, direction, false);
        } else if ((actions & PAD_CROSS) != 0) {
            if (row->activate != nullptr) {
                row->activate();
            } else if (row->action != nullptr) {
                BeginBinding(g_screen.row);
            } else {
                Step(*row, 1, true);
            }
        }
    } else if ((actions & PAD_CROSS) != 0) {
        Close();
    }

    if (g_screen.page != old_page || g_screen.row != old_row) {
        ShowBar();
        ComMenuSePlay(MENU_SOUND_CURSOR);
    }
}

} // namespace

Texts::Texts() {
    left.SetText("<");
    left.SetColor(ui::ColorWhite);
    right.SetText(">");
    right.SetColor(ui::ColorWhite);
    l1.SetText(kGlyphL1);
    l1.SetColor(ui::ColorWhite);
    r1.SetText(kGlyphR1);
    r1.SetColor(ui::ColorWhite);

    shortcuts.SetText(LocalizeText("options.shortcuts", kShortcutsText));
    shortcuts.SetColor(ui::ColorWhite);

    help.Set(kSaveHelp, LocalizeText("options.help.save_failed", kSaveHelpText));
    help.Set(kDisplayHelp, LocalizeText("options.help.display_kept", kDisplayHelpText));
    help.Set(kExitHelp, LocalizeText("options.help.exit", kExitHelpText));

    int index = 0;
    int page_index = 0;
    for (const Page &page : Pages()) {
        tabs.emplace_back(LocalizeText(PageKey(page.name), page.name), ui::ColorWhite);
        help.Set(kPageHelp + page_index++, LocalizeText(PageKey(page.name) + ".help", page.help) + "\n" +
                                               LocalizeText("options.help.turn_page", kTurnPageText));
        labels.emplace_back();
        values.emplace_back();
        for (const Row &row : page.rows) {
            labels.back().emplace_back(LocalizeText("options." + std::string(row.key) + ".label", row.label), ui::ColorWhite);
            values.back().emplace_back("", ui::ColorWhite, ui::Alignment::Center);
            if (row.help != nullptr) {
                help.Set(kRowHelp + index, LocalizeText("options." + std::string(row.key) + ".help", row.help));
            }
            ++index;
        }
    }
}

namespace {

std::unique_ptr<Texts> g_texts;
int                    g_texts_language = -1;

void RefreshTextsForLanguage() {
    if (g_texts && g_texts_language != LanguageCode) {
        g_texts.reset();
    }
}

} // namespace

Texts &GetTexts() {
    if (!g_texts) {
        g_texts = std::make_unique<Texts>();
        g_texts_language = LanguageCode;
    }
    return *g_texts;
}

const Page &CurrentPage() {
    return Pages()[g_screen.page];
}

int RowCount(int page) {
    return static_cast<int>(Pages()[page].rows.size());
}

int FirstRow() {
    return g_screen.first_row[g_screen.page];
}

int RowTop(int row) {
    return kRowY + (row - FirstRow()) * kRowStep;
}

bool Scrolls(int page) {
    return RowCount(page) > kVisibleRows;
}

int TabX(int tab) {
    int x = kLabelX + kTabArrow;
    for (int t = g_screen.first_tab; t < tab; ++t) {
        x += GetTexts().tabs[t].GetWidth() + kTabGap;
    }
    return x;
}

int LastVisibleTab() {
    int last = g_screen.first_tab;
    int count = static_cast<int>(Pages().size());
    while (last + 1 < count && TabX(last + 1) + GetTexts().tabs[last + 1].GetWidth() <= kTabsRight - kTabArrow) {
        ++last;
    }
    return last;
}

int L1X() {
    return kLabelX - GetTexts().l1.GetWidth() - 12;
}

int R1X() {
    return kTabsRight + 12;
}

int Open(int mode, int block_no, u_long128 *buffer) {
    if (buffer == nullptr) {
        buffer = reinterpret_cast<u_long128 *>(read_buffer);
    }

    u_long128 *data = MenuCalcBufAlignment(buffer);
    StartReadBG();

    if (LoadFileBGMenuData(const_cast<char *>("option.pac"), data) <= 0) {
        return 0;
    }

    RefreshTextsForLanguage();
    g_screen = Screen{};
    g_screen.open = true;
    g_screen.mode = mode;
    g_screen.block_no = block_no;

    if (mode == OPTION_OPEN_TITLE) {
        GamePad.SetAutoRepeat(PAD_DPAD, 30, 5);
        GamePad.MenuModeOn(120);
    }

    GetTexts();
    ListResolutions();
    g_screen.step = OPTION_STEP_FADE_IN;
    g_screen.first_row.assign(Pages().size(), 0);
    g_screen.cursor_x = kLabelX - 48.0f;
    g_screen.cursor_y = RowTop(0);
    g_screen.opened = ConfigGet();
    DisplayTakePumpSave();
    return 1;
}

int Run() {
    int result = 0;
    FollowWindow();

    switch (g_screen.step) {
        case OPTION_STEP_FADE_IN:
            if (!g_screen.texture_ready) {
                ReadBG();

                if (ReadBGSync() == 0) {
                    auto textures = std::make_unique<LOADTEXTURE_INFO2[]>(3);
                    textures[0] = {.name = const_cast<char *>("#frame_image_option#640#" SCREEN_HEIGHT_STR "#4"),
                                   .block_no = g_screen.block_no,
                                   .mipmap = 0};
                    textures[1] = {.name = nullptr, .block_no = g_screen.block_no, .mipmap = 0};
                    textures[2] = {.name = nullptr, .block_no = 0, .mipmap = 0};
                    BG_READ_INFO *file = GetReadBGFile(0);
                    textures[1].name = reinterpret_cast<char *>(GetPackFile(reinterpret_cast<u_int *>(file->buffer), const_cast<char *>("option.img"), nullptr));
                    TexManager.DeleteTextureBlock(g_screen.block_no);
                    TexManager.CleanUpTextureList();
                    TexManager.LoadTextureBlockEX(-1, textures.get());
                    g_screen.texture = TexManager.GetTexture(const_cast<char *>("option2"), -1);

                    if (g_screen.mode == OPTION_OPEN_TITLE) {
                        InitMenuMesSet(MENU_MES_SET_ALLMENU,
                                       reinterpret_cast<short *>(GetPackFile(reinterpret_cast<u_int *>(file->buffer), const_cast<char *>("allmenu.mes"), nullptr)));
                    }

                    g_screen.game_messages = CommonMenuMes2.buff;
                    ShowHelp();
                    g_screen.texture_ready = true;
                }
            }

            if (g_screen.texture_ready && g_screen.step_count > 12) {
                g_screen.step = OPTION_STEP_RUN;
                g_screen.step_count = 0;
                Pressed(HeldButtons(), 0xFFFF, false);
                InputSetMenuMouse(true);
                g_screen.mouse_buttons = InputTakeMenuMouse().buttons;
            }

            break;
        case OPTION_STEP_FADE_OUT:
            if (g_screen.step_count > 24) {
                if (g_screen.mode == OPTION_OPEN_TITLE) {
                    GamePad.AutoRepeatOff();
                    GamePad.MenuModeOff();
                    GamePad.SetAutoRepeat(PAD_UP | PAD_DOWN, 30, 9);
                    GamePad.MenuModeOn(120);
                }

                CommonMenuMes2.SetBuff(g_screen.game_messages);
                CommonMenuMes2.mes_made = -1;
                g_screen.open = false;
                result = 1;
            }

            break;
        default:
            if (g_screen.bar_frames > 0) {
                g_screen.bar_frames--;
            }
            RunMouse();
            if (g_screen.step == OPTION_STEP_RUN) {
                RunKeys();
            }
            ShowHelp();
            break;
    }

    return result;
}

std::vector<std::pair<std::string, std::string>> OptionStrings() {
    std::vector<std::pair<std::string, std::string>> out = {
        {"options.shortcuts",              kShortcutsText    },
        {"options.help.save_failed",       kSaveHelpText     },
        {"options.help.display_kept",      kDisplayHelpText  },
        {"options.help.turn_page",         kTurnPageText     },
        {"options.help.exit",              kExitHelpText     },
        {"options.help.display_now",       kDisplayNowText   },
        {"options.display.fullscreen_now", kFullscreenNowText},
        {"options.display.window_now",     kWindowNowText    },
    };
    for (auto &entry : RowStrings()) {
        out.push_back(std::move(entry));
    }
    for (auto &entry : ui::area_card::Strings()) {
        out.push_back(std::move(entry));
    }
    for (auto &entry : BattleMenuStrings()) {
        out.push_back(std::move(entry));
    }
    for (auto &entry : JournalStrings()) {
        out.push_back(std::move(entry));
    }
    for (auto &entry : title::menu::Strings()) {
        out.push_back(std::move(entry));
    }
    for (auto &entry : dialog::Strings()) {
        out.push_back(std::move(entry));
    }
    for (auto &entry : QuestStrings()) {
        out.push_back(std::move(entry));
    }
    return out;
}

} // namespace dc::options
