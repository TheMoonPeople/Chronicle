#include "options/screen.hpp"

#include <algorithm>
#include <format>
#include <memory>
#include <optional>

#include "clsmes.hpp"
#include "dataread.hpp"
#include "gamepad.hpp"
#include "gameutil.hpp"
#include "localize.hpp"
#include "mainselect.hpp"
#include "memcard.hpp"
#include "menu_draw.hpp"
#include "menuetc.hpp"
#include "platform/display.hpp"
#include "platform/input.hpp"
#include "platform/window.hpp"
#include "snd.hpp"
#include "sound.hpp"
#include "texture.hpp"

#define PAD_GLYPH_L1 "{L1}"
#define PAD_GLYPH_R1 "{R1}"
#define PAD_GLYPH_CIRCLE "{circle}"
#define PAD_GLYPH_TRIANGLE "{triangle}"
#define PAD_GLYPH_CROSS "{cross}"
#define PAD_GLYPH_SQUARE "{square}"

namespace options {

Screen g_screen;

namespace {

// Retail's menu auto-repeat: frames a direction is held before it repeats, and between repeats.
constexpr int kRepeatDelay = 30;
constexpr int kRepeatStep = 5;

// Frames the scroll bar stays after the list last moved.
constexpr int kBarFrames = 75;

constexpr int kDisplayNowHelp = 995;
constexpr int kDisplayHelp = 996;
constexpr int kSaveHelp = 997;
constexpr int kExitHelp = 999;
constexpr int kPageHelp = 900;
constexpr int kRowHelp = 1000;

// The screen's fixed texts in English, with the keys their translations have.
constexpr const char *kShortcutsText = PAD_GLYPH_SQUARE " Reset page\n" PAD_GLYPH_TRIANGLE " Undo changes\n" PAD_GLYPH_CIRCLE
                                                         " Close";
constexpr const char *kSaveHelpText = "Could not save.\nChanges apply now.\nClose to retry writing\nconfig.json.";
constexpr const char *kDisplayHelpText = "The display could not\nchange, and kept the\nmode it had. Try\nanother mode or size.";
constexpr const char *kTurnPageText = PAD_GLYPH_L1 " " PAD_GLYPH_R1 " turn the page.";
constexpr const char *kExitHelpText = PAD_GLYPH_CROSS " Close\n" PAD_GLYPH_SQUARE " This page's defaults\n" PAD_GLYPH_TRIANGLE
                                                      " Undo every change\n" PAD_GLYPH_CIRCLE " Close from any row";
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
    if (g_screen.page < g_screen.first_tab) {
        g_screen.first_tab = g_screen.page;
    }
    while (g_screen.page > LastVisibleTab()) {
        ++g_screen.first_tab;
    }
}

void Apply(const Config &config) {
    g_screen.save_failed = !DisplayApply(config);
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

// The pad as held: the d-pad, and the left stick as the menus' MenuModeOn(120) reads it.
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

// A binding row is waiting for its new key: the cursor stays on it and the rest of the menu ignores
// input until a key, mouse button or gamepad button arrives, or the player cancels. After a binding
// the menu keeps ignoring input until the key is let go, so the press that made the binding does not
// also press the action it was given to.
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
    // Show what it became while the source is still down.
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
    std::uint32_t  clicked = mouse.buttons & ~g_screen.mouse_buttons;
    bool           moved = mouse.dx != 0.0f || mouse.dy != 0.0f;
    g_screen.mouse_buttons = mouse.buttons;

    if (g_screen.binding_row >= 0) {
        // A rebind is waiting for a button: keep the mouse out of the menu, but let its buttons
        // reach the capture through the input layer.
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
        if (Inside(x, y, TabX(t) - 8, kTabY - 6, TabX(t) + GetTexts().tabs[t].Width() + 8, kTabY + 22)) {
            over_tab = t;
        }
    }
    int   bar_bottom = kRowY + kVisibleRows * kRowStep;
    Glyph glyph = Glyph::None;
    if (over_tab < 0 && Inside(x, y, L1X() - 4, kTabY - 6, kLabelX + kTabArrow, kTabY + 22)) {
        glyph = Glyph::L1;
    } else if (over_tab < 0 && Inside(x, y, kTabsRight - kTabArrow, kTabY - 6, R1X() + GetTexts().r1.Width() + 4,
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
            if (row.action != nullptr) {
                BeginBinding(over_row);
            } else if (x >= kValueX - 8 && x < kValueX + 24) {
                Step(row, -1, false);
            } else if (x >= kValueRight - 16) {
                Step(row, 1, false);
            } else if (x >= kValueX) {
                Step(row, 1, true);
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

    int direction = (moves & PAD_LEFT) != 0 ? -1 : (moves & PAD_RIGHT) != 0 ? 1 : 0;

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
            if (row->action != nullptr) {
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
    left.Set("<");
    right.Set(">");
    l1.Set(PAD_GLYPH_L1);
    r1.Set(PAD_GLYPH_R1);
    shortcuts.Set(LocalizeText("options.shortcuts", kShortcutsText));
    l1.RefreshGlyphs();
    r1.RefreshGlyphs();
    shortcuts.RefreshGlyphs();
    help.Set(kSaveHelp, LocalizeText("options.help.save_failed", kSaveHelpText));
    help.Set(kDisplayHelp, LocalizeText("options.help.display_kept", kDisplayHelpText));
    help.Set(kExitHelp, LocalizeText("options.help.exit", kExitHelpText));
    int index = 0;
    for (int p = 0; p < static_cast<int>(Pages().size()); ++p) {
        const Page &page = Pages()[p];
        tabs.emplace_back().Set(LocalizeText(PageKey(page.name), page.name));
        tabs.back().RefreshGlyphs();
        help.Set(kPageHelp + p, LocalizeText(PageKey(page.name) + ".help", page.help) + "\n" +
                                    LocalizeText("options.help.turn_page", kTurnPageText));
        labels.emplace_back();
        values.emplace_back();
        for (const Row &row : page.rows) {
            labels.back().emplace_back().Set(LocalizeText("options." + std::string(row.key) + ".label", row.label));
            labels.back().back().RefreshGlyphs();
            values.back().emplace_back();
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

} // namespace

Texts &GetTexts() {
    if (!g_texts) {
        g_texts = std::make_unique<Texts>();
        g_texts_language = LanguageCode;
    }
    return *g_texts;
}

// The screen's text is laid out for a language; opening the screen in another lays it out again.
void RefreshTextsForLanguage() {
    if (g_texts && g_texts_language != LanguageCode) {
        g_texts.reset();
    }
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
        x += GetTexts().tabs[t].Width() + kTabGap;
    }
    return x;
}

int LastVisibleTab() {
    int last = g_screen.first_tab;
    int count = static_cast<int>(Pages().size());
    while (last + 1 < count && TabX(last + 1) + GetTexts().tabs[last + 1].Width() <= kTabsRight - kTabArrow) {
        ++last;
    }
    return last;
}

int L1X() {
    return kLabelX - GetTexts().l1.Width() - 12;
}

int R1X() {
    return kTabsRight + 12;
}

int Open(int mode, int block_no, u_long128 *buffer) {
    if (buffer == nullptr) {
        buffer = (u_long128 *) read_buffer;
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
    // A display warning outlives the screen; a save result from before it opened does not.
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
                    LOADTEXTURE_INFO2 textures[3] = {
                        {const_cast<char *>("#frame_image_option#640#" SCREEN_HEIGHT_STR "#4"), 0, 0},
                        {nullptr,                                                               0, 0},
                        {nullptr,                                                               0, 0}
                    };
                    textures[0].block_no = g_screen.block_no;
                    textures[1].block_no = g_screen.block_no;
                    BG_READ_INFO *file = GetReadBGFile(0);
                    textures[1].name = (char *) GetPackFile((u_int *) file->buffer, const_cast<char *>("option.img"), nullptr);
                    TexManager.DeleteTextureBlock(g_screen.block_no);
                    TexManager.CleanUpTextureList();
                    TexManager.LoadTextureBlockEX(-1, textures);
                    g_screen.texture = TexManager.GetTexture(const_cast<char *>("option2"), -1);

                    if (g_screen.mode == OPTION_OPEN_TITLE) {
                        InitMenuMesSet(MENU_MES_SET_ALLMENU,
                                       (short *) GetPackFile((u_int *) file->buffer, const_cast<char *>("allmenu.mes"), nullptr));
                    }

                    g_screen.game_messages = CommonMenuMes2.buff;
                    ShowHelp();
                    g_screen.texture_ready = true;
                }
            }

            if (g_screen.texture_ready && g_screen.step_count > 12) {
                g_screen.step = OPTION_STEP_RUN;
                g_screen.step_count = 0;
                // What is still held from the menu that opened the screen fires only once let go.
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

} // namespace options

std::vector<std::pair<std::string, std::string>> OptionStrings() {
    std::vector<std::pair<std::string, std::string>> out = {
        {"options.shortcuts",             options::kShortcutsText    },
        {"options.help.save_failed",      options::kSaveHelpText     },
        {"options.help.display_kept",     options::kDisplayHelpText  },
        {"options.help.turn_page",        options::kTurnPageText     },
        {"options.help.exit",             options::kExitHelpText     },
        {"options.help.display_now",      options::kDisplayNowText   },
        {"options.display.fullscreen_now", options::kFullscreenNowText},
        {"options.display.window_now",    options::kWindowNowText    },
    };
    for (auto &entry : options::RowStrings()) {
        out.push_back(std::move(entry));
    }
    return out;
}
