#include "menu_draw.hpp"

#include <libvu0.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "battlemenu.hpp"
#include "camera.hpp"
#include "clsmes.hpp"
#include "dataalloc.hpp"
#include "dataread.hpp"
#include "dun/gameloop.hpp"
#include "editatra.hpp"
#include "editloop.hpp"
#include "gamepad.hpp"
#include "gameutil.hpp"
#include "gfx/gfx.hpp"
#include "itemdata.hpp"
#include "localize/localize.hpp"
#include "mainitemmodel.hpp"
#include "mainselect.hpp"
#include "memcard.hpp"
#include "memorycardaccess.hpp"
#include "menu_dungeon.hpp"
#include "menu_inventory.hpp"
#include "menu_misc.hpp"
#include "menuetc.hpp"
#include "menuitemstep.hpp"
#include "mglib.hpp"
#include "rect.hpp"
#include "savedata.hpp"
#include "shop.hpp"
#include "snd.hpp"
#include "sysmes.hpp"
#include "text/caption_text.hpp"
#include "text/textwipe.hpp"
#include "texture.hpp"
#include "ui/menu_pointer.hpp"
#include "ui/name_mouse.hpp"
#include "userstatus.hpp"

namespace dc::game {

using namespace dc::localize;
using namespace dc::text;
using namespace dc::ui;

CTexture *PerBoardTex;

/**
 * The personal inventory board the menu is working on.
 */
PERSONAL_BOARD *PerBoardPt;

namespace {

/** Player data used by personal-board item operations. */
CUserStatus *PerBoardStatusPt;

/** Marks, one per item pack slot, of the items the menu offers to throw away. */
s8 MenuTrushMark[100];

constexpr const char *AllMenuTextureDir = "commenu/";

/** Path fragment of each selectable language's directory and file names. */
constexpr std::array<const char *, 7> ComMenuContryName = {"a_jpn/", "a_usa/", "a_eng/", "a_fre/", "a_ger/", "a_ita/", "a_spa/"};

/** Rank of each item kind in the item board sort, rebuilt before each sort pass. */
int sort_table[9] = {9, 0, 1, 2, 3, 4, 5, 6, 7};

/** Item kind the item board sort places first. */
int sort_top_type = ITEM_SORT_FOOD;

/** Rank of each attachment kind in the attachment board sort, rebuilt before each sort pass. */
int asort_table[5] = {5, 1, 2, 3, 4};

/** Attachment kind the attachment board sort places first. */
int asort_top_type = 1;

std::string MenuLangPath(std::string_view name) {
    std::string path = GetMenuTextureDir();
    path += GetNowSelectLanguage(GetMenuLangFlag());
    path += name;
    return path;
}

} // namespace

const char *GetMenuTextureDir() {
    return AllMenuTextureDir;
}

int GetMenuLangFlag() {
    return LanguageCode;
}

const char *GetNowSelectLanguage(int language) {
    if (language < 0 || std::cmp_greater_equal(language, ComMenuContryName.size())) {
        language = 0;
    }

    return ComMenuContryName[language];
}

void GetPathReadDifferntLang(char *path) {
    const std::string prefix = MenuLangPath({});
    std::ranges::copy(prefix, path);
    path[prefix.size()] = '\0';
}

int LoadFileBGMenuData(const char *name, u_long128 *buffer) {
    if (name == nullptr || buffer == nullptr) {
        return -1;
    }

    std::string path = MenuLangPath(name);
    int         size = 0;
    LoadFileBG(path.data(), buffer, &size);
    return size;
}

int LoadFileMenuData(char *name, unsigned int *buffer) {
    std::string path = MenuLangPath(name);
    int         size = 0;
    LoadFile(path.data(), buffer, &size);
    return size;
}

u_long128 *BtlMenuBufferSet(int mode) {
    switch (mode) {
        case BATTLE_MENU_MODE_DUNGEON:
            return reinterpret_cast<u_long128 *>(read_buffer);
        case BATTLE_MENU_MODE_TOWN:
            return reinterpret_cast<u_long128 *>(EdMenuBuffer.base + static_cast<std::ptrdiff_t>(EdMenuBuffer.used) * 0x10);
        default:
            return nullptr;
    }
}

u_long128 *MenuCalcBufAlignment(u_long128 *buffer) {
    // Aligns to 64 bytes using the full pointer width.
    auto address = reinterpret_cast<std::uintptr_t>(buffer);
    return reinterpret_cast<u_long128 *>((address + 63) & ~std::uintptr_t{63});
}

int GetAtoraMaxVillage() {
    int max_village = 3;

    for (int village = 4; village >= 0; village--) {
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
    static constexpr std::array<s16, 35> map_to_village = {
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
    ambient[3] = static_cast<float>(ambient_alpha);

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

int GetMenuCommonFontW(int language, [[maybe_unused]] int style) {
    static constexpr std::array<s8, 7> font_widths = {16, 11, 11, 11, 11, 11, 11};
    return font_widths[language];
}

int GetMenuCommonPutXY(ClsMes *mes, int x) {
    if (mes == nullptr) {
        return 0;
    }

    int size[4];
    mes->NeedMesWinWH(mes->mes_made, size);
    const int put_x = x - (size[2] >> 1);
    mes->text_x = put_x;
    return put_x;
}

namespace {

/**
 * Resets a menu message window to its empty default layout.
 */
void ResetMenuMes(ClsMes *mes) {
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

    for (auto &mes_no : mes->mes_no) {
        mes_no = -1;
    }

    for (auto &value : mes->values) {
        value = 0;
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

    for (auto &line_pos : mes->line_pos) {
        line_pos.x = -1;
        line_pos.y = -1;
    }
}

} // namespace

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
            return;
        default:
            return;
    }
}

void DrawMenuClsMes(ClsMes *message, int x, int y) {
    if (message != nullptr) {
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

namespace {

// The boss floors' names on the floor-select board, a row each of a 220x24 strip of the board's sheet, drawn in the
// text font (English only: the port has no other language's names for them): where each painted name starts, from
// the strip's left, and how much the painting squeezed it.
struct BoardName {
    const char                       *english;
    float                             left;
    float                             squeeze;
    std::vector<caption::BoardAdjust> adjust; // where this name's painting departs from the others'
};

const BoardName kBoardNames[] = {
    {"DIVINE BEAST DRAN'S CHAMBER", 5.02f, 0.636f, {{U'B', 0.0f, 0.0f, 0.0f, -0.6f}, {U'R', 0.0f, 0.0f, 0.0f, -0.6f, 0}}                                                                                                                                               },
    {"DWELLING OF FOREST GUARD",    4.10f, 0.714f, {{U'R', 0.0f, -1.2f}, {U'D', -1.4f, 0.0f}, {U'N', 1.6f, -1.6f}, {U'A', 0.9f, 0.0f, 0.0f, -1.0f}}                                                                                                                    },
    {"CHURCH OF MEMORY",            3.57f, 0.954f, {{U'R', 0.0f, -2.0f, -2.0f}, {U'H', 0.0f, 0.5f}, {U'U', 0.0f, 0.0f, 0.8f}}                                                                                                                                          },
    {"COFFIN'S CHAMBER",            3.00f, 0.978f, {{U'N', 0.8f, -0.8f}, {U'S', -0.8f, 0.8f}, {U' ', 0.0f, 0.0f, 2.0f}, {U'R', 0.0f, -2.2f}, {U'E', 0.0f, -0.8f}}                                                                                                      },
    {"COLISEUM",                    3.70f, 0.944f, {{U'C', 0.8f, -0.8f}, {U'S', 0.8f, -0.8f}, {U'U', -0.8f, 0.8f}}                                                                                                                                                     },
    {"NIGHT OF INCIDENT",           3.26f, 1.000f, {{U'I', 0.0f, 0.0f, 0.0f, -0.8f}, {U'I', 0.0f, 0.0f, 0.0f, 0.8f, 1}, {U'G', -0.8f, 0.8f}, {U'C', 0.0f, 0.0f, 0.0f, -1.2f}, {U'D', -0.6f, 0.0f, 0.0f, 0.6f}, {U'E', 0.0f, -1.4f}, {U'N', 0.0f, 0.0f, 0.0f, -1.4f, 2}}},
    {"TOP FLOOR",                   4.46f, 0.956f, {{U'O', 0.0f, -0.6f}, {U'P', 1.6f, 0.0f}, {U'R', 0.0f, -2.2f}, {U'O', 0.0f, 0.0f, 0.0f, 0.6f, 1}, {U'O', 0.0f, 0.0f, 0.0f, 0.6f, 2}}                                                                                },
};

bool DrawBoardName(CTexture *texture, const CRect_i_ &screen, const CRect_i_ &texel, int alpha) {
    if (texture == nullptr || texture != DunLogBoard || texel.x != 0xFC || texel.width != 0xDC || texel.y < 0x20 ||
        texel.height <= 0 || texel.height > 0x18 || !caption::Ready() || !LocalizeLanguageFile().starts_with("en_")) {
        return false;
    }
    // The row the strip is cut from; the list clips a row at its edges by starting the strip lower or ending it sooner.
    const int row = (texel.y - 0x20) / 0x18;
    if (row >= std::ssize(kBoardNames) || texel.y + texel.height > 0x20 + (row + 1) * 0x18) {
        return false;
    }
    const auto       row_top = static_cast<float>(screen.y - (texel.y - (0x20 + row * 0x18)));
    const BoardName &name = kBoardNames[row];
    return caption::DrawBoardName(name.english, static_cast<float>(screen.x) + name.left, row_top + 4.17f, name.squeeze,
                                  name.adjust, static_cast<float>(screen.y), static_cast<float>(screen.y + screen.height),
                                  alpha);
}

// The dungeon-entry list's floor labels, in the 0x70 by 0x1C rows of the sheet "kaisou" at (u, v) the list cuts each
// dungeon's from (English: the other languages' painted words stay): each word's painted letters' box in its row (or,
// by_capitals, from where they run from and to, their capitals' top and baseline).
struct FloorLabel {
    int                               u, v;
    const char                       *english;
    float                             left, top, right, bottom;
    std::vector<caption::FloorAdjust> adjust;        // where this word's painting departs from the font's spacing
    bool                              by_capitals{}; // top and bottom are the capitals' top and the baseline
    float                             squeeze{};     // its letters this much as wide as tall as the font's, from left
};

const FloorLabel kFloorLabels[] = {
    {0x00, 0x00, "B", 29.0f, 5.0f, 43.4f, 23.2f},
    {0x80, 0x00, "Floor", 17.0f, 4.75f, 62.4f, 22.7f},
    {0x00, 0x20, "Territory", 6.0f, 4.0f, 69.4f, 24.2f, {{0, 0.6f}, {1, 0.0f, -2.5f}}},
    // Layer by its capitals: 18 pixels tall, half a pixel above Territory's baseline.
    {0x00, 0x40, "Layer", 16.0f, 4.4f, 62.4f, 22.4f, {}, true},
    // Level as Layer, its letters as wide, its L where the painting's is, the rest a little further on, the v on the line
    // (the font's dips below it).
    {0x00, 0x60, "Level", 16.0f, 4.4f, 60.4f, 22.4f, {{1, 0.0f, 0.6f}, {2, 0.0f, 1.0f, 0.5f}, {3, 0.0f, 0.2f}, {4, 0.0f, 0.8f}}, true, 0.852f},
    {0x00, 0x80, "No.", 3.0f, 5.0f, 20.4f, 22.9f, {{0, 0.25f}, {1, 0.25f, -0.25f}, {2, 0.0f, -0.55f, -3.15f}}}, // its N and o a little wider, its stop down by the o
    {0x00, 0x80, "Craters", 58.0f, 5.0f, 109.4f, 22.9f},
    {0x00, 0xA0, "Years ago", 50.0f, 4.75f, 110.4f, 24.2f, {{1, 0.0f, -1.1f}, {3, 0.0f, 0.25f}, {4, 0.0f, -0.5f, 0.25f}, {5, 0.0f, 0.5f}, {6, 0.0f, 0.0f, -1.1f}}}, // its g's top level with the a's
};

bool DrawFloorLabels(CTexture *texture, const CRect_i_ &screen, const CRect_i_ &texel, int alpha) {
    if (texture == nullptr || std::string_view(texture->name) != "kaisou" || texel.width != 0x70 || texel.height <= 0 ||
        texel.height > 0x1C || texel.y >= 0xC0 || !caption::Ready() || !LocalizeLanguageFile().starts_with("en_")) {
        return false;
    }
    // The row the strip is cut from; the list clips a row at its edges by starting the strip lower or ending it sooner.
    const int row_v = texel.y / 0x20 * 0x20;
    if (texel.y + texel.height > row_v + 0x1C) {
        return false;
    }
    const auto left = static_cast<float>(screen.x);
    const auto row_top = static_cast<float>(screen.y - (texel.y - row_v));
    bool       drew = false;
    for (const FloorLabel &label : kFloorLabels) {
        if (label.u == texel.x && label.v == row_v) {
            drew = caption::DrawFloorLabel(label.english, left + label.left, row_top + label.top, left + label.right,
                                           row_top + label.bottom, static_cast<float>(screen.y),
                                           static_cast<float>(screen.y + screen.height), alpha, label.adjust, label.by_capitals,
                                           label.squeeze) ||
                   drew;
        }
    }
    return drew;
}

// The name entry's painted title, REGISTER NAME (0xAE by 0x20 at 0xC0, 0x178 of "nametemp"), in the captions' look.
bool DrawNameTitle(CTexture *texture, const CRect_i_ &screen, const CRect_i_ &texel, int alpha) {
    if (texture == nullptr || std::string_view(texture->name) != "nametemp" || texel.x != 0xC0 || texel.y != 0x178 ||
        texel.width != 0xAE || texel.height != 0x20 || !LocalizeLanguageFile().starts_with("en_")) {
        return false;
    }
    // The painted letters run from about 3 to 164 of it, their capitals from row 8 to 27.6, the words 5 pixels apart.
    const float rows = static_cast<float>(screen.height) / static_cast<float>(texel.height);
    // English's RE, EG, GI, IS and ER closer, and ST, TE and NA closer still, as painted, NAME where it was.
    static constexpr float kClose = -0.8f;
    static constexpr float kCloser = -1.6f;
    static constexpr float kEnglish[] = {kClose + 0.4f, kClose, kClose, kClose, kCloser, kCloser, kClose, 0.1f - 2 * kClose, kCloser};
    // NAME half a pixel further on than the painting's.
    return caption::DrawTitle("REGISTER NAME", screen.x, screen.y, 3.2f, 163.7f, 8.0f * rows + 0.2f, 26.2f * rows + 1.4f, 0.1f,
                              kEnglish, alpha);
}

// A painted title the game draws whole with its icon (a strip of a texture): the icon as painted, the word in the
// captions' look over the painted word, whose cream letters run from left to right of the strip and whose capitals
// from top to bottom, a space space ems; the English word's letters (spaces not counted) moved on by after[i] and drawn
// narrow[i] times as wide. English, or a language with the word; the painted strip otherwise.
struct PaintedTitle {
    const char            *texture;
    int                    u, v, width, height;
    int                    icon_width;
    const char            *key;
    const char            *english;
    int                    left, right, top, bottom;
    float                  space = 0.1f;
    std::span<const float> after;
    std::span<const float> narrow;
};

// CHECK-INCOUNTER's letters are painted nearly touching, its dash a short one tucked under the K's leg.
constexpr float kCheckInAfter[] = {-0.6f, -0.6f, -0.6f, -0.6f, -2.2f, -1.0f, -0.6f, -0.6f, -0.6f, -0.6f, -0.6f, -0.6f, -0.6f, -0.6f};
constexpr float kCheckInNarrow[] = {1, 1, 1, 1, 1, 0.6f};

constexpr PaintedTitle kPaintedTitles[] = {
    // the shop's title, and the check-in counter's (painted CHECK-INCOUNTER)
    {"shopbrd", 0x14, 0x50, 0xA2, 0x28, 58, "shop.title.shop", "SHOP", 64, 117, 9, 27},
    {"shopbrd", 0x14, 0x78, 0xC8, 0x28, 58, "shop.title.check_in", "CHECK-IN COUNTER", 63, 194, 9, 28, 0.1f, kCheckInAfter, kCheckInNarrow},
    // the fishing points exchange's title, and the fishing log's
    {"fishbrd", 0, 0xD0, 0xF0, 0x28, 58, "fishing.title.exchange", "FP EXCHANGE", 72, 234, 12, 30, 0.35f},
    {"fprecbrd", 0, 0xD0, 0x10C, 0x28, 58, "fishing.title.log", "FISHING LOG", 70, 219, 11, 30, 0.25f},
};

bool DrawPaintedTitle(CTexture *texture, const CRect_i_ &screen, const CRect_i_ &texel, int alpha) {
    if (texture == nullptr || screen.width != texel.width || screen.height != texel.height) {
        return false;
    }
    for (const PaintedTitle &title : kPaintedTitles) {
        if (texel.x != title.u || texel.y != title.v || texel.width != title.width || texel.height != title.height ||
            std::string_view(texture->name) != title.texture) {
            continue;
        }
        std::string word = LocalizeText(title.key, "");
        const bool  english = word.empty() && LocalizeLanguageFile().starts_with("en_");
        if (english) {
            word = title.english;
        }
        if (word.empty() || !caption::Ready()) {
            return false;
        }
        set2DSprite(GetVif1Packet(), texture, CRect_i_(screen.x, screen.y, title.icon_width, screen.height),
                    CRect_i_(texel.x, texel.y, title.icon_width, texel.height), alpha);
        return caption::DrawTitle(word, screen.x, screen.y, static_cast<float>(title.left), static_cast<float>(title.right),
                                  static_cast<float>(title.top), static_cast<float>(title.bottom), title.space,
                                  english ? title.after : std::span<const float>{}, alpha, 0x80,
                                  english ? title.narrow : std::span<const float>{});
    }
    return false;
}

// The name alphabet's cells (the keyboard, the name being entered, the save slots, the ally card) drawn from the font.
bool DrawAlphabetCell(CTexture *texture, const CRect_i_ &screen, const CRect_i_ &texel, int r, int g, int b, int alpha) {
    return texture != nullptr && std::string_view(texture->name) == "alphabet" &&
           caption::DrawAlphabetCell(texel.x, texel.y, texel.width, texel.height, static_cast<float>(screen.x),
                                     static_cast<float>(screen.y), static_cast<float>(screen.width),
                                     static_cast<float>(screen.height), r, g, b, alpha);
}

} // namespace

void DrawMenu2DSprite(CTexture *texture, CRect_i_ screen, CRect_i_ texel, int alpha) {
    if (DrawAlphabetCell(texture, screen, texel, 0x80, 0x80, 0x80, alpha) || DrawBoardName(texture, screen, texel, alpha) ||
        DrawFloorLabels(texture, screen, texel, alpha) || DrawNameTitle(texture, screen, texel, alpha) ||
        DrawPaintedTitle(texture, screen, texel, alpha)) {
        return;
    }
    set2DSprite(GetVif1Packet(), texture, screen, texel, alpha);
}

void DrawMenu2DSprite(CTexture *texture, CRect_i_ screen, CRect_i_ texel, unsigned char r, unsigned char g,
                      unsigned char b, int alpha) {
    // Clamp 1:1 sprite source heights so adjacent atlas rows are not sampled when scaled.
    if (screen.width > 0 && screen.height > 0 &&
        texel.width == screen.width &&
        (texel.height == screen.height + 1 || texel.height == screen.height + 2)) {
        texel.height = screen.height;
        if (screen.width == 0x100 && screen.height == 0x78 && texel.x == 0 && texel.y == 0x78) {
            screen.height = 0x77;
            texel.height = 0x77;
        }
    }

    NameMouseHand(texture, screen, texel, r == 0 && g == 0 && b == 0);
    if (DrawAlphabetCell(texture, screen, texel, r, g, b, alpha) || DrawBoardName(texture, screen, texel, alpha) ||
        DrawFloorLabels(texture, screen, texel, alpha)) {
        return;
    }
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
    MGFillBox(CRect_i_(0, 0, 0x2800, (SCREEN_HEIGHT << 4)), r, g, b, alpha);
}

void AllFadeForMenu(int alpha) {
    AllFillBoxForMenu(0, 0, 0, static_cast<unsigned char>(alpha));
}

void FrameImageDraw(int brightness, int alpha) {
    CTexture *texture = TexManager.GetTexture("frame_image", -1);

    if (texture != nullptr) {
        TexManager.ReloadTexture(GetVif1Packet(), texture->block);
        reinterpret_cast<sceGsTex0 *>(&texture->tex0)->bits.tcc = 0;
        sceGsTexa texa = mgTexa;
        texa.AEM = 1;
        texa.TA0 = 0x80;
        MGSetGsTEXA(&texa);
        DrawMenu2DSprite(texture, MenuDispRc, MenuDispRc, brightness, brightness, brightness, alpha);
        MGSetGsTEXA(nullptr);
    }
}

void DrawMenuColorGradation(CRect_i_ &rect, spRGBA *top_left, spRGBA *top_right, spRGBA *bottom_left, spRGBA *bottom_right) {
    set2DSpriteC4(GetVif1Packet(), rect, top_left, top_right, bottom_left, bottom_right);
}

void DrawMenuSideGradation(CRect_i_ &rect, spRGBA *left, spRGBA *right) {
    set2DSpriteC4(GetVif1Packet(), rect, left, right, left, right);
}

namespace {

/**
 * Draws the mark over an item that cannot be set.
 */
void DrawDontSetItemMark(int x, int y, int top, int bottom, int alpha) {
    if (y < top - 31 || y > bottom - 1) {
        return;
    }

    int position = y;
    int length = 32;
    int source = 0xC0;
    MenuTextureClip(position, source, length, top, bottom);
    DrawMenu2DSprite(PerBoardTex, CRect_i_(x, position, 32, length), CRect_i_(0x13C, source, 32, length), alpha);
}

} // namespace

void DrawIconParts(int item_no, int x, int y, int top, int bottom, int alpha, int number) {
    if (y < top - 31 || y > bottom - 1) {
        return;
    }

    int                  position = y;
    const COM_ITEM_INFO *info = GetCommonItemInfo(item_no);

    if (info != nullptr && info->icon_index >= 0) {
        int       u = 0;
        int       v = 0;
        CTexture *texture = RetCTex(static_cast<short>(item_no), u, v);

        if (texture != nullptr) {
            int length = 32;
            MenuTextureClip(position, v, length, top, bottom);
            CRect_i_ source(u, v, 32, length);
            set2DSprite(GetVif1Packet(), texture, CRect_i_(x + 2, position + 1, 32, length), source, 0, 0, 0, alpha * 0x50 >> 7);
            DrawMenu2DSprite(texture, CRect_i_(x, position, 32, length), source, alpha);
            DrawAttachNumberOrWeapon(x, y, top, bottom, item_no, number, alpha, 0);
        }
    }
}

void DrawAttachNumberOrWeapon(int x, int y, int top, int bottom, int item_no, int value, int alpha, int vibe) {
    int u = 0;
    int v = 0;

    TexManager.GetTexture("wepicon", -1);

    if (0 < value && item_no >= ITEM_ATTACH_STAT_START && item_no < ITEM_ATTACH_GEM_START) {
        const int number_x = x + 12;
        int       draw_y = y + 10;
        u = (value - 1) * 0x12 + 0x84;
        v = 0xB0;
        int height = 0x14;
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
        CTexture *texture = RetCTex(static_cast<s16>(value), u, v);

        if (texture != nullptr) {
            int height = 0x20;
            int draw_y = y;
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

    reinterpret_cast<sceGsTex0 *>(&texture.tex0)->bits.tcc = 0;
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

    MGSetGsTEXA(nullptr);
}

CTexture *RetCTex(short item_no, int &u, int &v) {
    const COM_ITEM_INFO *info = GetCommonItemInfo(item_no);

    if (info == nullptr) {
        return nullptr;
    }

    const int icon = info->icon_index;

    if (icon < 0) {
        return nullptr;
    }

    u = (icon % 8) << 5;
    v = (icon >> 3) << 5;

    switch (info->kind) {
        case ITEMKIND_ITEM:
            return ItemIcon;
        case ITEMKIND_ATTACH:
        case ITEMKIND_WEAPON:
            return WepIcon;
        default:
            return nullptr;
    }
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

namespace {

/** Description of each main menu icon, for each of the seven languages. */
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

} // namespace

MENU_ICON_INFO *GetMenuIconInfo(int icon) {
    return &MenuIcon[GetMenuLangFlag()][icon];
}

void DrawMainMenuIcon(int x, int y, int icon, int selected, int bright, int alpha) {
    if (StayTex == nullptr) {
        StayTex = TexManager.GetTexture(const_cast<char *>("stayframe"), -1);
        if (StayTex == nullptr) {
            return;
        }
    }
    MENU_ICON_INFO *info = GetMenuIconInfo(icon);
    if (info == nullptr) {
        return;
    }

    static constexpr std::array<std::array<s16, 2>, 2> kFrameSize = {
        {{0x30, 0x20}, {0x3A, 0x28}}
    };

    int u = info->u;
    int v = info->v;
    int width = kFrameSize[selected][0];
    int height = kFrameSize[selected][1];
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
    auto painted = [&](CTexture *texture) {
        set2DSprite(GetVif1Packet(), texture, CRect_i_(x, y, info->label_width, info->label_height - 1), CRect_i_(info->label_u, info->label_v, info->label_width, info->label_height), bright, bright, bright, alpha);
    };
    if (caption::Ready()) {
        textwipe::Both([&](bool runtime) {
            if (!runtime || !caption::Draw(icon, x, y, info->label_width, alpha, bright)) {
                painted(StayTex);
            }
        });
        return;
    }
    painted(StayTex);
}

void DrawMenuVibeItem(int x, int y, int offset_x, int offset_y, [[maybe_unused]] int alpha) {
    float pointer_x = 0.0f;
    float pointer_y = 0.0f;

    if (MenuPointerHand(pointer_x, pointer_y)) {
        x = static_cast<int>(pointer_x);
        y = static_cast<int>(pointer_y);
    }

    int       item_x = x + offset_x;
    int       item_y = y + offset_y;
    s16       item_no = PerBoardPt->held_item.item_no;
    CRect_i_  shadow(0x80, 0x28, 0x20, 0x20);
    int       u = 0;
    int       v = 0;
    CTexture *texture = RetCTex(item_no, u, v);

    if (texture != nullptr) {
        DrawObjectVibe(x + 4, y + 2, TexManager.GetTexture(const_cast<char *>("StayTex"), -1), shadow, 0, 0x50);
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
    static constexpr float offsets[7][4] = {
        {320.0f, 325.0f, 14.6f, 2.2f},
        {318.0f, 352.0f, 15.6f, 2.7f},
        {318.0f, 352.0f, 15.6f, 2.7f},
        {318.0f, 352.0f, 15.6f, 2.7f},
        {318.0f, 352.0f, 15.6f, 2.7f},
        {318.0f, 352.0f, 15.6f, 2.7f},
        {318.0f, 352.0f, 15.6f, 2.7f},
    };
    const int lang = GetMenuLangFlag();

    x = offsets[lang][0];
    y = offsets[lang][1];
    width = offsets[lang][2];
    height = offsets[lang][3];
    return 0;
}

int GetMainMenuRightHelpMsgLangOffset(int &x, int &y) {
    static constexpr s8 offsets[7][2] = {
        {18, 16},
        {20, 12},
        {20, 12},
        {20, 12},
        {20, 14},
        {20, 12},
        {20, 12},
    };
    const int lang = GetMenuLangFlag();

    x = offsets[lang][0];
    y = offsets[lang][1];
    return 1;
}

void InitHaveData(IHAVEITEM *item) {
    memset(item, -1, sizeof(*item));
}

void InitHaveWep(WEAPON_HAVE *weapon) {
    memset(weapon, 0, sizeof(*weapon));
    weapon->item_no = -1;
}

void InitHaveAttach(ATTACH_LIST *attachment) {
    memset(attachment, 0, sizeof(*attachment));
}

void MenuDataSwap(s16 *first, s16 *second) {
    if (first == nullptr || second == nullptr) {
        return;
    }

    std::swap(*first, *second);
}

void MenuDataSwap(int *first, int *second) {
    if (first == nullptr || second == nullptr) {
        return;
    }

    std::swap(*first, *second);
}

// Byte copies keep the records' padding, which save data stores raw.
void MenuDataSwap(WEAPON_HAVE *first, WEAPON_HAVE *second) {
    if (first == nullptr || second == nullptr) {
        return;
    }

    WEAPON_HAVE saved{};
    memcpy(&saved, first, sizeof(WEAPON_HAVE));
    memcpy(first, second, sizeof(WEAPON_HAVE));
    memcpy(second, &saved, sizeof(WEAPON_HAVE));
}

void MenuDataSwap(ATTACH_LIST *first, ATTACH_LIST *second) {
    if (first == nullptr || second == nullptr) {
        return;
    }

    ATTACH_LIST saved{};
    memcpy(&saved, first, sizeof(ATTACH_LIST));
    memcpy(first, second, sizeof(ATTACH_LIST));
    memcpy(second, &saved, sizeof(ATTACH_LIST));
}

void SetMenuTrushMark(ITEM_PACK *items) {
    int quick_count = 0;

    for (const auto quantity : items->quick_item_qty) {
        quick_count += quantity;
    }

    for (int slot = items->num - 1; slot >= 0; slot--) {
        if (items->item[slot] < ITEM_DUNGEON_START && quick_count > 0) {
            MenuTrushMark[slot] = 1;
            quick_count--;
        } else {
            MenuTrushMark[slot] = 0;
        }
    }
}

void DeleteMenuTrushMark() {
    std::ranges::fill(MenuTrushMark, 0);
}

void InitPersonalBoardMode(CUserStatus *status, PERSONAL_BOARD *board, int mode, int page) {
    PerBoardStatusPt = status;
    PerBoardPt = board;

    if (PerBoardTex == nullptr) {
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

        for (const auto &weapons : PerBoardStatusPt->chara_weapons) {
            if (weapons[10].item_no >= ITEM_WEAPON_START) {
                full = BOARD_PAGE_WEAPON;
                break;
            }
        }

        if (full == BOARD_PAGE_ITEM) {
            SetMenuTrushMark(pack);
        }
    }
}

int BoardModeChangeKey() {
    const int board_mode = PerBoardPt->page;

    if (GamePad.Down(PAD_L2 | PAD_L1) && PerBoardPt->menu_kind == PERSONAL_BOARD_ITEM) {
        PerBoardPt->page--;

        if (PerBoardPt->page < BOARD_PAGE_ITEM) {
            PerBoardPt->page = BOARD_PAGE_ATTACH;
        }
    }

    if (GamePad.Down(PAD_R2 | PAD_R1) && PerBoardPt->menu_kind == PERSONAL_BOARD_ITEM) {
        PerBoardPt->page++;

        if (PerBoardPt->page > BOARD_PAGE_ATTACH) {
            PerBoardPt->page = BOARD_PAGE_ITEM;
        }
    }

    return board_mode != PerBoardPt->page ? 1 : 0;
}

void PersonalBoardLimmitCheck() {
    int      &cursor = PerBoardPt->cursor;
    const int max = PersonalRetMax(PerBoardPt->page);
    const int top_row = PerBoardPt->top_row;
    const int last_top = std::max(max / 5 - 4, 0);

    if (max - 1 < cursor) {
        while (cursor >= max) {
            cursor -= 5;
        }

        PerBoardPt->top_row = std::max(cursor / 5 - 3, 0);

        PerBoardPt->y = 0x7F - PerBoardPt->top_row * 0x28;
        int rows = max / 5;

        if (rows <= 0) {
            rows = 4;
        }

        PerBoardPt->scroll = 140.0f + 114.0f * PerBoardPt->top_row / rows;
        return;
    }

    if (last_top < top_row) {
        PerBoardPt->top_row = last_top;
    }
}

int PersonalBoardKeySub() {
    int       exit_left = 0;
    const int area = PerBoardPt->cursor_area;
    int      &cursor = PerBoardPt->cursor;
    const int max = PersonalRetMax(PerBoardPt->page);

    if (GamePad.Down(PAD_UP) != 0 && PerBoardPt->cursor_area == PERSONAL_BOARD_AREA_CELLS) {
        if (cursor > 4) {
            cursor -= 5;
        }

        if (cursor / 5 < PerBoardPt->top_row) {
            PerBoardPt->top_row--;
        }
    }

    if (GamePad.Down(PAD_DOWN) != 0 && PerBoardPt->cursor_area == PERSONAL_BOARD_AREA_CELLS) {
        if (cursor < max - 5) {
            cursor += 5;
        }

        if (cursor / 5 > PerBoardPt->top_row + 3) {
            PerBoardPt->top_row++;
        }
    }

    if (GamePad.Down(PAD_RIGHT) != 0 && PerBoardPt->cursor_area == PERSONAL_BOARD_AREA_CELLS) {
        const int column = cursor % 5;

        if (column < 4) {
            cursor++;
        } else if (column == 4 && PerBoardPt->menu_kind != PERSONAL_BOARD_ATLA) {
            PerBoardPt->cursor_area = PERSONAL_BOARD_AREA_TRASH;
        }
    }

    if (GamePad.Down(PAD_LEFT) != 0) {
        if (PerBoardPt->cursor_area == PERSONAL_BOARD_AREA_TRASH) {
            PerBoardPt->cursor_area = PERSONAL_BOARD_AREA_CELLS;
        } else if (PerBoardPt->cursor_area == PERSONAL_BOARD_AREA_CELLS) {
            if (cursor % 5 == 0) {
                exit_left = 1;
            } else {
                cursor--;
            }
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

int PersonalBoardItemPush(IHAVEITEM *item, int board_pos) {
    int       enabled = false;
    const int kind = WhatIsKindofItem(item->item_no);

    if (kind == BOARD_PAGE_ITEM || kind < 0) {
        s16 *board_item = &PerBoardPt->item_pack->item[board_pos];
        s16 *board_volume = &PerBoardPt->item_pack->item_vol[board_pos];

        if (MenuTrushMark[board_pos] != 1) {
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
    const int    kind = WhatIsKindofItem(item->item_no);
    const int    chara = board_pos / 10;
    const int    slot = board_pos % 10;
    WEAPON_HAVE *weapon = &PerBoardStatusPt->chara_weapons[chara][slot];

    if (kind == BOARD_PAGE_ITEM) {
        if (weapon->item_no < ITEM_WEAPON_START) {
            return 0;
        }

        if (item->item_no == ITEM_REPAIR_POWDER && weapon->durability_f < weapon->durability) {
            weapon->durability_f = weapon->durability;

            if (weapon->item_no == GetDefaultWeaponNo(chara)) {
                weapon->item_no++;
                WepDataListToHaveCopy(weapon->item_no, weapon);
            }

            InitHaveData(item);
            result = 2;
        }

        if (item->item_no == ITEM_POWERUP_POWDER && weapon->experience < GetWeaponMaxExp(weapon) && weapon->level < 99) {
            const int item_no = weapon->item_no;

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

    if (kind == BOARD_PAGE_WEAPON || kind < 0) {
        if (kind == BOARD_PAGE_WEAPON) {
            const int owner = WhoIsWeaponEquip(item->item_no);

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

int PersonalBoardAttachPush(IHAVEITEM *item, int board_pos) {
    int       enabled = false;
    const int kind = WhatIsKindofItem(item->item_no);

    if (kind == BOARD_PAGE_ATTACH || kind < 0) {
        item->from_page = BOARD_PAGE_ATTACH;
        item->from_slot = board_pos;
        auto     *slot = reinterpret_cast<ATTACH_LIST *>(&PerBoardStatusPt->consumable_items[board_pos]);
        const s16 item_no = item->item_no;
        const s16 slot_item_no = slot->item_no;

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
    IHAVEITEM *item = &PerBoardPt->held_item;

    switch (PerBoardPt->page) {
        case BOARD_PAGE_ITEM:
            return PersonalBoardItemPush(item, board_pos);
        case BOARD_PAGE_WEAPON:
            return PersonalBoardWeaponPush(item, board_pos);
        case BOARD_PAGE_ATTACH:
            return PersonalBoardAttachPush(item, board_pos);
        default:
            return 0;
    }
}

void PersonalBoardItemCancel() {
    IHAVEITEM *have = &PerBoardPt->held_item;
    const int  cell = have->from_slot;

    switch (have->from_page) {
        case BOARD_PAGE_ITEM: {
            s16 *item = &PerBoardPt->item_pack->item[cell];
            s16 *volume = &PerBoardPt->item_pack->item_vol[cell];
            MenuDataSwap(item, &have->item_no);
            MenuDataSwap(volume, &have->volume);
            break;
        }
        case BOARD_PAGE_WEAPON: {
            const int held = have->item_no;

            if (held >= ITEM_WEAPON_START) {
                const int    chara = cell / 10;
                const int    slot = cell % 10;
                WEAPON_HAVE *weapon = &PerBoardStatusPt->chara_weapons[chara][slot];
                const int    placed = weapon->item_no;
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
            DNG_CONSUMABLE *item = &PerBoardStatusPt->consumable_items[cell];
            const int       held = have->item_no;
            const int       placed = item->id;
            MenuDataSwap(reinterpret_cast<ATTACH_LIST *>(item), &PerBoardPt->held_attach);
            have->item_no = placed;
            item->id = held;
            break;
        }
        default:
            break;
    }

    if (have->item_no < ITEM_ATTACH_START) {
        InitHaveData(have);
        InitHaveWep(&PerBoardPt->weapon);
        PerBoardPt->held_equipped_slot = -1;
    }
}

int PersonalRetMax(int board_mode) {
    switch (board_mode) {
        case BOARD_PAGE_ITEM:
            return PerBoardPt->item_pack->num;
        case BOARD_PAGE_WEAPON:
            return 60;
        case BOARD_PAGE_ATTACH:
            return 40;
        case BOARD_PAGE_ATLA_VILLAGE:
        case BOARD_PAGE_ATLA_VILLAGE + 1:
        case BOARD_PAGE_ATLA_VILLAGE + 2:
        case BOARD_PAGE_ATLA_VILLAGE + 3:
        case BOARD_PAGE_ATLA_VILLAGE + 4:
        case BOARD_PAGE_ATLA_VILLAGE + 5:
            return 120;
        default:
            return 0;
    }
}

void DrawPersonalBoard(int x, int y, int board_mode, int alpha, [[maybe_unused]] int unused) {
    const int max = PersonalRetMax(board_mode);
    const int top = y + 9;
    const int bottom = top + 0xA0;
    const int left = x + 0x13;
    const int target_y = y - 0x11 + 0x17 - PerBoardPt->top_row * 40;

    PerBoardPt->y += (target_y - PerBoardPt->y) / 4.0f;
    const int row_y = static_cast<int>(PerBoardPt->y);

    if (PerBoardPt->menu_kind == PERSONAL_BOARD_ATLA || PerBoardPt->menu_kind == PERSONAL_BOARD_ITEM) {
        MenuTextureReload(PerBoardTex->block);
    }

    const int mark = board_mode == BOARD_PAGE_WEAPON ? 2 : 0;
    DrawPerBoardDraw(mark, max, left, row_y, top, bottom, PerBoardTex, alpha);

    switch (PerBoardPt->menu_kind) {
        case PERSONAL_BOARD_ATLA:
            MenuTextureReload(PerBoardTex->block);
            break;
        case PERSONAL_BOARD_ITEM:
            MenuTextureReload(ItemMenuWeaponIconReadBlock);
            break;
        default:
            break;
    }

    CommonIconDraw(board_mode, max, left + 2, row_y + 6, top, bottom, alpha);
    MenuTextureReload(PerBoardTex->block);
    PersonalBoardOptionDraw(board_mode, max, x, y, PerBoardTex, alpha);
}

namespace {

/**
 * Draws the equipped-weapon marker on a personal-board entry.
 */
void DrawNowEquipWeaponMark(int x, int y, int top, int bottom, int alpha) {
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

} // namespace

void CommonIconDraw(int page, int count, int x, int y, int top, int bottom, int alpha) {
    int       value = 0;
    int       draw_x = 0;
    const int weapon_x = x + 4;
    int       draw_y = y;
    CTexture *icon = nullptr;

    switch (page) {
        case BOARD_PAGE_ATTACH: {
            if (PerBoardPt->menu_kind == PERSONAL_BOARD_WEAPON_ATTACH) {
                MenuTextureReload(MenuShadowReadBlock);
            } else {
                icon = TexManager.GetTexture("wepicon", -1);

                if (icon != nullptr) {
                    MenuTextureReload(icon->block);
                    WepIcon = icon;
                }
            }

            draw_x = x + 2;
            const auto *attach = reinterpret_cast<ATTACH_LIST *>(PerBoardPt->consumables);

            for (int i = 0; i < count; i++) {
                const int id = attach[i].item_no;

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
            const s16 *items = PerBoardPt->item_pack->item;

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
            int                chara = 0;
            const WEAPON_HAVE *weapons = PerBoardStatusPt->chara_weapons[0];
            icon = TexManager.GetTexture("wepicon", -1);

            if (icon != nullptr) {
                MenuTextureReload(icon->block);
                WepIcon = icon;
            }

            draw_x = weapon_x;
            const int party = PerBoardStatusPt->party_size;

            for (int i = 0; i < count; i++) {
                const int slot = i % 10;
                const int item_no = weapons[slot].item_no;
                const int equipped = PerBoardStatusPt->equipped_weapon_slot[i / 10];

                if (party >= i / 10 + 1 && equipped >= 0 && slot == equipped) {
                    DrawNowEquipWeaponMark(draw_x - 2, draw_y - 4, top + 1, bottom, alpha);

                    if (icon != nullptr) {
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
                        weapons = PerBoardStatusPt->chara_weapons[chara];
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
            const int left = weapon_x - 2;
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
        default:
            break;
    }
}

void PersonalBoardDrawWaku(int x, int y, CTexture *texture, int alpha) {
    // Draw the top strip at 1:1 texel height so the frame border meets the clipped grid at y=129.
    DrawMenu2DSprite(texture, CRect_i_(x, y + 1, 0x14, 0xBF), CRect_i_(0, 0, 0x14, 0xC0), alpha);
    DrawMenu2DSprite(texture, CRect_i_(x + 0x14, y + 1, 0xC8, 0x14), CRect_i_(0x14, 0, 0xC8, 0x14), alpha);
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
    int top = 0;
    int row = 0;

    switch (PerBoardPt->menu_kind) {
        case PERSONAL_BOARD_WEAPON_ATTACH:
            bright = 0x40;
            [[fallthrough]];
        case PERSONAL_BOARD_ITEM:
            for (int kind = 0; kind < 3; kind++) {
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
                static constexpr s8 widths[7][3] = {
                    {0x34, 0x45, 0x4E},
                    {0x34, 0x45, 0x4E},
                    {0x34, 0x45, 0x4E},
                    {0x34, 0x45, 0x4E},
                    {0x34, 0x45, 0x4E},
                    {0x34, 0x45, 0x4E},
                    {0x34, 0x45, 0x4E},
                };
                static constexpr s8 sources[7][3] = {
                    {0x00, 0x34, 0x79},
                    {0x00, 0x34, 0x79},
                    {0x00, 0x34, 0x79},
                    {0x00, 0x34, 0x79},
                    {0x00, 0x34, 0x79},
                    {0x00, 0x34, 0x79},
                    {0x00, 0x34, 0x79},
                };
                static constexpr s8 offsets[7][3] = {
                    {2, -8, -16},
                    {2, -8, -16},
                    {2, -8, -16},
                    {2, -8, -16},
                    {2, -8, -16},
                    {2, -8, -16},
                    {2, -8, -16},
                };
                const int lang = GetMenuLangFlag();
                const int width = widths[lang][tag];
                const int source = sources[lang][tag];

                x += offsets[lang][tag];
                x += shift;

                if (shift == 0) {
                    DrawMenu2DSprite(texture, CRect_i_(x, top + 1, width, 0x1B), CRect_i_(source, 0xC0, width, 0x1C), alpha);
                }
            }
            break;
        case PERSONAL_BOARD_ATLA:
            if (VillageBar != nullptr) {
                MenuTextureReload(VillageBar->block);
                bool      last = false;
                int       village = 2;
                const int max_village = GetAtoraMaxVillage();
                int       step = 1;
                int       count = 0;

                top = y - 0x30;

                while (step != 0) {
                    village += step;

                    village = std::min(village, max_village);

                    if (village == tag) {
                        if ((step == 1 && village == max_village) || step == -1) {
                            last = true;
                        } else if (step == 1) {
                            step = -1;
                            village = max_village;
                        }
                    }

                    const int index = village - 3;
                    const int left = x + index * 0x1A;

                    if (last) {
                        DrawMenu2DSprite(VillageBar, CRect_i_(x, y - 0x12, 0x100, 0x13), CRect_i_(0, 0x90, 0x100, 0x14), alpha);
                        bright = 0x80;
                    }

                    const int column = (index % 2) * 0x60;
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
        default:
            break;
    }
}

void PersonalBoardScrlBarDraw(int count, int x, int y, float &scroll, unsigned char top_row, CTexture *texture, int alpha) {
    const int bar_x = x + 0xE8;
    int       rows = count / 5;

    if (rows <= 0) {
        rows = 1;
    }

    int length = static_cast<int>(456.0f / rows - 8.0f);

    if (114.0f < static_cast<float>(length)) {
        length = 0x69;
    }

    CRect_i_ source(0xAE, 0x14, 8, 4);
    float    bar_y = static_cast<float>(y + 0x16) + 114.0f * top_row / rows;
    bar_y = scroll + (bar_y - scroll) / 4.0f;
    scroll = bar_y;
    DrawMenu2DSprite(texture, CRect_i_(bar_x, static_cast<int>(bar_y), 8, 4), source, alpha);
    source.y += 4;
    DrawMenu2DSprite(texture, CRect_i_(bar_x, static_cast<int>(4.0f + bar_y), 8, length + 1), source, alpha);
    source.y += 4;
    DrawMenu2DSprite(texture, CRect_i_(bar_x, static_cast<int>(4.0f + bar_y + length), 8, 4), source, alpha);
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

    switch (PerBoardPt->page) {
        case BOARD_PAGE_ITEM:
        case BOARD_PAGE_WEAPON:
        case BOARD_PAGE_ATTACH: {
            max = GetNowModeMaxNum(PerBoardPt->page, &full);
            const int item = PerBoardPt->held_item.item_no;

            if (item >= ITEM_ATTACH_START && WhatIsKindofItem(item) == PerBoardPt->page) {
                max++;
            }

            if (num < max) {
                full = 1;
            }

            break;
        }
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
        default:
            break;
    }

    if (full != 0) {
        digits.y += digits.height;
    }

    DrawMenuNumber(max, left, top, texture, digits, 1, alpha);
}

namespace {

/**
 * Draws the base layers of a personal inventory board.
 */
void DrawPersonalBoardBase(int x, int y, int top, int bottom, int count, CTexture *texture, int alpha) {
    if (y < top - 39 || y > bottom - 1) {
        return;
    }

    int v = 0x14;
    int length = 0x28;
    MenuTextureClip(y, v, length, top, bottom);
    int    step = 9;
    spRGBA upper;
    spRGBA lower;
    upper.r = upper.g = upper.b = 0x80;
    lower.r = lower.g = lower.b = 0x80 - step;
    lower.a = alpha;
    upper.a = alpha;

    for (int i = 0; i < count; i++) {
        set2DSprite(GetVif1Packet(), texture, CRect_i_(x, y, 0x28, length), CRect_i_(0x20, v, 0x28, length), &upper, &lower, &upper, &lower, 1);
        upper.r = upper.g = upper.b = lower.r;
        lower.r = lower.g = lower.b = lower.r - step;
        step--;
        x += 0x28;
    }
}

} // namespace

void DrawPerBoardDraw(int mark, [[maybe_unused]] int count, int x, int y, int top, int bottom, CTexture *texture, int alpha) {
    int row_y = y;

    for (int row = 0; row < 26; row++) {
        DrawPersonalBoardBase(x, row_y, top, bottom, 5, texture, alpha);
        bool draw_mark = false;

        switch (mark) {
            case 1:
                draw_mark = row == 2;
                break;
            case 2:
                draw_mark = row % 2 == 0 && row != 0;
                break;
            default:
                break;
        }

        if (draw_mark) {
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

void CommonTrushDraw(int x, int y, int alpha) {
    PERSONAL_BOARD *board = PerBoardPt;
    CRect_i_        source(0x190, 0x90, 0x30, 0x30);
    const CRect_i_  dest(x, y + 1, 0x30, 0x2F);

    if (board->trash_anim == 1) {
        const int frame = board->trash_frame;

        if (frame <= 3 || frame > 12) {
            source.x -= 0x30;
        } else if (frame > 6 && frame <= 9) {
            source.x += 0x30;
        }

        DrawMenu2DSprite(PerBoardTex, dest, source, alpha);
        board->trash_frame++;

        if (frame > 15) {
            board->trash_anim = 0;
            board->trash_frame = 0;
        }
    } else {
        if (board->cursor_area == PERSONAL_BOARD_AREA_TRASH) {
            source.x = 0x130;
        }

        DrawMenu2DSprite(PerBoardTex, dest, source, alpha);
    }
}

int IsEnableTrushThrow(int item_no) {
    bool enable = false;

    if (item_no >= ITEM_WEAPON_START) {
        const int          owner = WhoIsWeaponEquip(item_no);
        const WEAPON_HAVE *weapons = PerBoardStatusPt->chara_weapons[owner];
        const int          default_no = GetDefaultWeaponNo(owner);

        for (int i = 0; i < 11; i++) {
            const int weapon_no = weapons[i].item_no;

            if (weapon_no == default_no || weapon_no == default_no + 1) {
                enable = true;
                break;
            }
        }

        if (PerBoardStatusPt->party_size - 1 < owner) {
            enable = true;
        }

        if (item_no == ITEM_WEAPON_SERPENT_SWORD && GetMenuHebikiriFlag() == 0) {
            enable = false;
        }

        if (PerBoardPt->held_equipped_slot >= 0) {
            enable = false;
        }
    }

    if (item_no >= ITEM_ATTACH_START && item_no < ITEM_WEAPON_START) {
        const COM_ITEM_INFO *info = GetCommonItemInfo(item_no);

        if (info != nullptr) {
            switch (info->kind) {
                case ITEMKIND_ATTACH:
                    if (GetAttachData(item_no) != nullptr) {
                        enable = true;
                    }

                    break;
                case ITEMKIND_ITEM: {
                    const ITEM_DATA *data = GetItemData(item_no);

                    if (data != nullptr) {
                        enable = (data->kind_flags & 0x10) == 0;
                    }

                    break;
                }
                default:
                    break;
            }
        }
    }

    return enable ? 1 : 0;
}

void CommonMoneyBoardDraw(int x, int y, int money, int alpha) {
    const int top = y + 2;

    DrawMenu2DSprite(PerBoardTex, CRect_i_(x, top, 0x1A, 0x1B), CRect_i_(0x160, 0xC0, 0x1A, 0x1C), alpha);
    DrawMenu2DSprite(PerBoardTex, CRect_i_(x + 0x1A, top, 0x2A, 0x1B), CRect_i_(0x17A, 0xC0, 0x20, 0x1C), alpha);
    DrawMenu2DSprite(PerBoardTex, CRect_i_(x + 0x44, top, 0x1A, 0x1B), CRect_i_(0x19A, 0xC0, 0x1A, 0x1C), alpha);
    RECT digits = {0, 0xDC, 12, 12};
    DrawMenuNumber(money, x + 0x55, y + 8, PerBoardTex, digits, 0, alpha);
}

int SearchBoardNowPosItemExist(int page, int cell) {
    switch (page) {
        case BOARD_PAGE_ITEM:
            return PerBoardStatusPt->item_pack.item[cell];
        case BOARD_PAGE_WEAPON:
            return PerBoardStatusPt->chara_weapons[cell / 10][cell % 10].item_no;
        case BOARD_PAGE_ATTACH:
            return PerBoardStatusPt->consumable_items[cell].id;
        default:
            return -1;
    }
}

int GetBoardSpace(int item_no, int *page) {
    if (item_no < ITEM_ATTACH_START) {
        return -1;
    }

    *page = WhatIsKindofItem(item_no);
    const int max = PersonalRetMax(*page);

    switch (*page) {
        case BOARD_PAGE_ITEM: {
            const ITEM_PACK *pack = &PerBoardStatusPt->item_pack;
            int              quick = 0;

            for (const auto quantity : pack->quick_item_qty) {
                quick += quantity;
            }

            for (int i = 0; i < max - quick; i++) {
                if (pack->item[i] < ITEM_DUNGEON_START && MenuTrushMark[i] == 0) {
                    return i;
                }
            }

            break;
        }
        case BOARD_PAGE_WEAPON: {
            const int          owner = WhoIsWeaponEquip(item_no);
            const WEAPON_HAVE *weapons = PerBoardStatusPt->chara_weapons[owner];

            for (int i = 0; i < 10; i++) {
                if (weapons[i].item_no < ITEM_WEAPON_START) {
                    return i + owner * 10;
                }
            }

            break;
        }
        case BOARD_PAGE_ATTACH: {
            const DNG_CONSUMABLE *items = PerBoardStatusPt->consumable_items;

            for (int i = 0; i < max; i++) {
                if (items[i].id < ITEM_ATTACH_START) {
                    return i;
                }
            }

            break;
        }
        default:
            break;
    }

    return -1;
}

void SwapItem(ITEM_PACK *items, int first_pos, int second_pos) {
    std::swap(items->item[first_pos], items->item[second_pos]);
    std::swap(items->item_vol[first_pos], items->item_vol[second_pos]);
}

int CompItem(int first_item_no, int second_item_no) {
    const ITEM_DATA *first = GetItemData(first_item_no);
    const ITEM_DATA *second = GetItemData(second_item_no);
    int              first_rank = 0;
    int              second_rank = 0;

    if (first != nullptr) {
        first_rank = sort_table[first->sort_key];
    }

    if (second != nullptr) {
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

int SeitonItemBoardSub(ITEM_PACK *items) {
    int type = sort_top_type;

    for (int rank = 0; rank < ITEM_SORT_COUNT; rank++) {
        sort_table[type] = rank;
        type++;

        if (type >= ITEM_SORT_COUNT) {
            type = ITEM_SORT_NONE;
        }
    }

    sort_table[ITEM_SORT_NONE] = 9;
    int swapped = 0;

    for (int i = 0; i < items->num - 1; i++) {
        for (int j = i + 1; j < items->num; j++) {
            if (CompItem(items->item[i], items->item[j]) > 0) {
                SwapItem(items, i, j);
                swapped = 1;
            }
        }
    }

    return swapped;
}

void SeitonItemBoard(ITEM_PACK *items) {
    if (items != nullptr) {
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
    if (item_no >= ITEM_ATTACH_START && item_no < ITEM_ATTACH_ATTACK) {
        return ATTACHKIND_ELEMENT;
    }

    if (item_no >= ITEM_ATTACH_ATTACK && item_no < ITEM_ATTACH_AMETHYST) {
        return ATTACHKIND_STAT;
    }

    if (item_no >= ITEM_ATTACH_AMETHYST && item_no < ITEM_ATTACH_GEM_END) {
        return ATTACHKIND_GEM;
    }

    if (item_no >= ITEM_ATTACH_DINOSLAYER && item_no <= ITEM_ATTACH_SLAYER_END) {
        return ATTACHKIND_SLAYER;
    }

    return ATTACHKIND_OTHER;
}

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

int SeitonAttachBoardSub(ATTACH_LIST *attachments) {
    int type = asort_top_type;

    for (int rank = 0; rank < 5; rank++) {
        asort_table[type] = rank;
        type++;

        if (type >= 5) {
            type = 0;
        }
    }

    asort_table[0] = 5;
    int swapped = 0;

    for (int i = 0; i < 39; i++) {
        for (int j = i + 1; j < 40; j++) {
            if (CompAttach(&attachments[i], &attachments[j]) > 0) {
                MenuDataSwap(&attachments[i], &attachments[j]);
                swapped = 1;
            }
        }
    }

    return swapped;
}

int SeitonAttachBoard(ATTACH_LIST *attachments) {
    if (attachments == nullptr) {
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
    return 1;
}

int WhatIsKindofItem(int item_no) {
    const COM_ITEM_INFO *info = GetCommonItemInfo(item_no);

    if (info == nullptr) {
        return BOARD_PAGE_NONE;
    }

    static constexpr std::array<int, 3> board_kind = {BOARD_PAGE_ATTACH, BOARD_PAGE_ITEM, BOARD_PAGE_WEAPON};
    return board_kind[info->kind];
}

int WhoIsWeaponEquip(int weapon_no) {
    const COM_ITEM_INFO *info = GetCommonItemInfo(weapon_no);

    if (info == nullptr || info->kind != ITEMKIND_WEAPON) {
        return -1;
    }

    const WEAPON_DATA *data = GetWeaponDataInfo(info->index);

    if (data != nullptr) {
        return static_cast<s8>(data->owner);
    }

    return -1;
}

int GetWeaponHoleNum(int weapon_no) {
    const COM_ITEM_INFO *info = GetCommonItemInfo(weapon_no);

    if (info == nullptr || info->kind != ITEMKIND_WEAPON) {
        return 0;
    }

    const WEAPON_DATA *data = GetWeaponData(weapon_no);

    if (data == nullptr) {
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
    if (weapon == nullptr) {
        return 0;
    }

    int                count = 0;
    const WEAPON_DATA *data = GetWeaponData(weapon->item_no);

    if (data != nullptr) {
        for (int i = 0; i < 6; i++) {
            if (data->hole[i] > 0 && weapon->attach[i].item_no >= ITEM_ATTACH_START) {
                count++;
            }
        }
    }

    return count;
}

int GetWeaponMaxExp(WEAPON_HAVE *weapon) {
    if (weapon == nullptr) {
        return 0;
    }

    const WEAPON_DATA *data = GetWeaponData(weapon->item_no);

    if (data == nullptr) {
        return 1;
    }

    // Only the low byte of the base is meaningful.
    int experience = static_cast<s8>(data->exp_base);

    for (int i = 0; i < weapon->level; i++) {
        experience += data->exp_per_level;
    }

    experience = std::min(experience, 999);

    if (experience <= 0) {
        experience = 99;
    }

    return experience;
}

int GetNowItemNum(short item_no, ITEM_PACK *items) {
    int count = 0;

    for (int i = 0; i < items->num; i++) {
        if (items->item[i] == item_no) {
            count++;
        }
    }

    for (int j = 0; j < 3; j++) {
        if (items->quick_item_slot[j] == item_no) {
            count += items->quick_item_qty[j];
        }
    }

    return count;
}

void DeleteItemAfterUseItem(short item_no, ITEM_PACK *items) {
    for (int i = 0; i < items->num; i++) {
        if (items->item[i] == item_no) {
            items->item[i] = -1;
            return;
        }
    }

    for (int i = 0; i < 3; i++) {
        if (items->quick_item_slot[i] == item_no) {
            items->quick_item_qty[i]--;

            if (items->quick_item_qty[i] <= 0) {
                items->quick_item_slot[i] = -1;
            }

            break;
        }
    }
}

int GetNowModeMaxNum(int page, int *overflow) {
    CUserStatus *status = SaveData->GetDngStatus();
    int          count = 0;

    switch (page) {
        case BOARD_PAGE_ITEM: {
            const ITEM_PACK *pack = &status->item_pack;
            const int        num = pack->num;

            for (int i = 0; i < num + 3; i++) {
                if (pack->item[i] >= ITEM_DUNGEON_START) {
                    count++;
                }
            }

            for (int j = 0; j < 3; j++) {
                if (pack->quick_item_slot[j] >= ITEM_DUNGEON_START) {
                    count += pack->quick_item_qty[j];
                }
            }

            if (overflow != nullptr && num < count) {
                *overflow = 1;
            }

            break;
        }
        case BOARD_PAGE_WEAPON:
            for (int chara = 0; chara < 6; chara++) {
                const WEAPON_HAVE *weapons = status->chara_weapons[chara];

                for (int i = 0; i < 11; i++) {
                    if (weapons[i].item_no >= ITEM_WEAPON_START) {
                        count++;

                        if (i == 10 && overflow != nullptr) {
                            *overflow = chara + 1;
                        }
                    }
                }
            }

            break;
        case BOARD_PAGE_ATTACH: {
            const DNG_CONSUMABLE *items = status->consumable_items;

            for (int i = 0; i < 43; i++) {
                if (items[i].id >= ITEM_ATTACH_START) {
                    count++;
                }
            }

            if (overflow != nullptr && count > 40) {
                *overflow = 1;
            }

            break;
        }
        default:
            break;
    }

    return count;
}

void WepDataListToHaveCopy(int weapon_no, WEAPON_HAVE *weapon) {
    if (weapon != nullptr && weapon_no > 0) {
        if (weapon_no < ITEM_ATTACH_START) {
            weapon_no += 0x100;
        }

        const WEAPON_DATA *data = GetWeaponData(weapon_no);
        memset(weapon, 0, sizeof(WEAPON_HAVE));
        weapon->item_no = weapon_no;
        weapon->attack = data->attack;
        weapon->endurance = data->endurance;
        weapon->speed = data->speed;
        weapon->magic = data->magic;
        weapon->durability = data->durability;
        weapon->durability_f = weapon->durability;
        int best = 5;
        int best_value = 0;

        for (int i = 0; i < 5; i++) {
            weapon->elem[i] = data->elem[i];

            if (weapon->elem[i] > best_value) {
                best_value = weapon->elem[i];
                best = i;
            }
        }

        weapon->best_elem = best;

        for (int monster = 0; monster < 10; monster++) {
            weapon->vs_monster[monster] = data->vs_monster[monster];
        }

        weapon->flags = data->flags;

        if (IsDefaultWeapon(weapon->item_no) < 0) {
            const int chance = rand() % 1000;

            if (chance < 10) {
                weapon->flags |= WEAPONFLAG_DURABLE;
            }

            if (chance > 989) {
                weapon->flags |= WEAPONFLAG_FRAGILE;
            }
        }
    }
}

void AttachDataListToHaveCopy(int attachment_no, ATTACH_LIST *attachment) {
    if (attachment == nullptr) {
        return;
    }

    ATTACH_DATA *data = GetAttachData(attachment_no);
    if (data != nullptr) {
        std::memcpy(attachment, data, sizeof(*attachment));
    }
}

int ItemDataToHaveCopy(int item_no) {
    if (item_no < ITEM_DUNGEON_START || item_no > ITEM_WEAPON_START - 1) {
        return 0;
    }

    const ITEM_DATA *data = GetItemData(item_no);

    if (data == nullptr) {
        return -1;
    }

    int vol = data->vol;

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
    if (texture == nullptr) {
        return;
    }
    // PAL artwork has 448 rows; the renderer expands full-screen sprites to the viewport.
    DrawMenu2DSprite(texture, CRect_i_(x, y, MenuDispRc.width, MenuDispRc.height),
                     CRect_i_(0, 0, 640, 448), alpha);
}

} // namespace dc::game
