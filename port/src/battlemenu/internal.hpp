#pragma once

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

#include "battlemenu.hpp"
#include "battlemenu/strings.hpp"
#include "btactstatus.hpp"
#include "camera.hpp"
#include "character.hpp"
#include "clsmes.hpp"
#include "dataread.hpp"
#include "dngstatusdata.hpp"
#include "dun/gameloop.hpp"
#include "eastking.hpp"
#include "gamepad.hpp"
#include "itemdata.hpp"
#include "localize/localize.hpp"
#include "mainselect.hpp"
#include "map.hpp"
#include "memcard.hpp"
#include "menu_draw.hpp"
#include "menu_dungeon.hpp"
#include "menu_inventory.hpp"
#include "menu_manual.hpp"
#include "menu_misc.hpp"
#include "menu_save.hpp"
#include "menuetc.hpp"
#include "mglib.hpp"
#include "rect.hpp"
#include "render/draw2d.hpp"
#include "snd.hpp"
#include "text/caption_text.hpp"
#include "text/gametext.hpp"
#include "texture.hpp"
#include "ui/framework.hpp"
#include "ui/menu_mouse.hpp"
#include "userstatus.hpp"
#include "weapon_buildup.hpp"
#include "weaponlevelup.hpp"

namespace dc::battlemenu {

/**
 * Draws the Battle Menu's main ring and eases the system cursor towards the selected icon.
 */
void PortDrawBattleMenuMain();

/**
 * Draws the Battle Menu and its active page.
 */
void PortDrawBattleMenu();

/**
 * Runs one frame of Battle Menu input and returns BattleMenuSelect's result.
 */
int PortBattleMenuSelect();

/**
 * Draws the Battle Menu's move page and its world map.
 */
void PortDrawMenuMove();

/**
 * Draws the selected weapon's action dialog at (x, y) with the given alpha.
 */
void PortDrawWeaponSelectDialog(int x, int y, int alpha);

/**
 * Draws the build-up choices of the selected weapon at (x, y) with cursor on the highlighted one.
 */
void PortDrawBuildUpWeaponSelect(int x, int y, int cursor);

/**
 * Draws one weapon of a character's weapon list, its model at depth and its row at (x, y).
 */
void PortDrawAallWeapon(int x, int y, float depth, CCharacter *model, WEAPON_HAVE *weapon,
                        int equipped, int selected, int alpha);

/**
 * Draws chara's weapon list with its models lit for the menu camera.
 */
void PortBtlWeaponDraw(int x, float depth, int chara, int alpha);

/**
 * Draws the Battle Menu's weapon page, fading it as it opens and closes.
 */
void PortWeaponMenuDraw();

/**
 * Returns whether a Battle Menu modal confirmation dialog is currently active.
 */
bool BattleMenuQuitConfirmActive();

/**
 * Returns a pointer to the active Battle Menu modal's selected option index.
 */
int *BattleMenuQuitConfirmChoicePtr();

/**
 * Returns the active Battle Menu modal's button under the logical point (x, y): 0, 1, or -1 for neither.
 */
int BattleMenuQuitConfirmHit(float x, float y);

/**
 * Consumes and returns whether an in-game save load is ready to reload the town.
 */
bool TakeBattleMenuLoadRequest();

/**
 * Consumes and returns whether Quit was confirmed, so the main loop goes back to the title screen.
 */
bool TakeBattleMenuTitleRequest();

/**
 * Page identifier for the in-game Load Game action on the Battle Menu ring.
 */
inline constexpr int BTLMENU_PAGE_LOAD = 99;

/**
 * Page identifier for the Quit Game action on the Battle Menu ring.
 */
inline constexpr int BTLMENU_PAGE_QUIT = 100;

namespace detail {

/**
 * Refreshes cached pointers to the resident Battle Menu textures.
 */
inline void RefreshBtlMenuTextures() {
    MenuCharaFace = TexManager.GetTexture(const_cast<char *>("charaface"), BtlMenuReadBlock);
    BtStatus = TexManager.GetTexture(const_cast<char *>("btstatus2"), BtlMenuReadBlock);
    WepIcon = TexManager.GetTexture(const_cast<char *>("wepicon"), -1);
    game::ItemIcon = TexManager.GetTexture(const_cast<char *>("itemicon"), BtlMenuReadBlock);
    PerBoardTex = TexManager.GetTexture(const_cast<char *>("perbrd"), BtlMenuReadBlock);
    WepStatus = TexManager.GetTexture(const_cast<char *>("wepstatus"), BtlMenuExtBlock);
    VillageName = TexManager.GetTexture(const_cast<char *>("vilname"), -1);
    VillageBar = TexManager.GetTexture(const_cast<char *>("viltag"), -1);
}

/**
 * Strips inline control tags and trailing whitespace from a decoded message string.
 */
inline std::string CleanDecodedText(std::string decoded) {
    for (std::size_t pos = 0; (pos = decoded.find('{', pos)) != std::string::npos;) {
        const std::size_t end = decoded.find('}', pos);
        if (end != std::string::npos) {
            decoded.erase(pos, end - pos + 1);
        } else {
            break;
        }
    }
    while (!decoded.empty() && (decoded.back() == '\n' || decoded.back() == '\r' || decoded.back() == ' ')) {
        decoded.pop_back();
    }
    return decoded;
}

/**
 * Decodes a single message entry by index from a raw message buffer.
 */
inline std::string DecodeMesFromBuffer(short *mes_buffer, int mes_no) {
    if (mes_buffer == nullptr || mes_no < 0) {
        return {};
    }
    short *saved_buff = CommonMenuMes3.buff;
    CommonMenuMes3.buff = mes_buffer;
    const short *codes = CommonMenuMes3.GetTextLineDataTop(mes_no);
    CommonMenuMes3.buff = saved_buff;
    if (codes == nullptr) {
        return {};
    }
    return CleanDecodedText(GameTextDecode(codes));
}

/**
 * Decodes the localized display name of an item or weapon from the system message table.
 */
inline std::string DecodeItemNamePort(int item_no, std::string_view fallback = {}) {
    if (item_no > 0 && SystemMes != nullptr) {
        if (COM_ITEM_INFO *info = GetCommonItemInfo(item_no)) {
            if (info->msg > 0) {
                std::string       name = DecodeMesFromBuffer(SystemMes, info->msg + 0x64);
                const std::size_t newline = name.find('\n');
                if (newline != std::string::npos) {
                    name.resize(newline);
                }
                if (!name.empty()) {
                    return name;
                }
            }
        }
    }
    return std::string(fallback);
}

} // namespace detail

} // namespace dc::battlemenu

// DC_TRANSITIONAL_BEGIN
#ifndef DC_NO_TRANSITIONAL_USING
using namespace dc::battlemenu;
namespace btlmenu_detail = dc::battlemenu::detail;
#endif
// DC_TRANSITIONAL_END
