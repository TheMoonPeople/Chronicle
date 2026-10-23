#pragma once

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

#include "battlemenu.hpp"
#include "battlemenu/strings.hpp"
#include "btactstatus.hpp"
#include "camera.hpp"
#include "caption_text.hpp"
#include "character.hpp"
#include "clsmes.hpp"
#include "dataread.hpp"
#include "dngstatusdata.hpp"
#include "draw2d_port.hpp"
#include "dun/gameloop.hpp"
#include "eastking.hpp"
#include "gamepad.hpp"
#include "gametext.hpp"
#include "itemdata.hpp"
#include "localize.hpp"
#include "mainselect.hpp"
#include "map.hpp"
#include "memcard.hpp"
#include "menu_draw.hpp"
#include "menu_dungeon.hpp"
#include "menu_inventory.hpp"
#include "menu_manual.hpp"
#include "menu_misc.hpp"
#include "menu_mouse.hpp"
#include "menu_save.hpp"
#include "menuetc.hpp"
#include "mglib.hpp"
#include "rect.hpp"
#include "snd.hpp"
#include "texture.hpp"
#include "ui_framework.hpp"
#include "userstatus.hpp"
#include "weapon_buildup.hpp"
#include "weaponlevelup.hpp"

void PortDrawBattleMenuMain();
void PortDrawBattleMenu();
int  PortBattleMenuSelect();
void PortDrawMenuMove();
void PortDrawWeaponSelectDialog(int x, int y, int alpha);
void PortDrawBuildUpWeaponSelect(int x, int y, int cursor);
void PortDrawAallWeapon(int x, int y, float depth, CCharacter *model, WEAPON_HAVE *weapon,
                        int equipped, int selected, int alpha);
void PortBtlWeaponDraw(int x, float depth, int chara, int alpha);
void PortWeaponMenuDraw();

/**
 * Initializes the Battle Menu character selection page and turntable.
 */
extern void Btl_InitMenuChara(u_long128 *buffer);

/**
 * Initializes the Battle Menu weapon selection page for the given character.
 */
extern void Btl_InitWeaponSelect(int mode, int chara);

/**
 * Initializes the Battle Menu item inventory page for the given character.
 */
extern void Btl_InitItemMode(int mode, int chara);

/**
 * Initializes the Battle Menu journal and manual viewer page.
 */
extern void Btl_BattleManualInit(int *blocks, u_long128 *buffer);

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

namespace btlmenu_detail {

/**
 * Refreshes cached pointers to the resident Battle Menu textures.
 */
inline void RefreshBtlMenuTextures() {
    MenuCharaFace = TexManager.GetTexture(const_cast<char *>("charaface"), BtlMenuReadBlock);
    BtStatus = TexManager.GetTexture(const_cast<char *>("btstatus2"), BtlMenuReadBlock);
    WepIcon = TexManager.GetTexture(const_cast<char *>("wepicon"), -1);
    ItemIcon = TexManager.GetTexture(const_cast<char *>("itemicon"), BtlMenuReadBlock);
    PerBoardTex = TexManager.GetTexture(const_cast<char *>("perbrd"), BtlMenuReadBlock);
    WepStatus = TexManager.GetTexture(const_cast<char *>("wepstatus"), BtlMenuExtBlock);
    VillageName = TexManager.GetTexture(const_cast<char *>("vilname"), -1);
    VillageBar = TexManager.GetTexture(const_cast<char *>("viltag"), -1);
}

/**
 * Strips inline control tags and trailing whitespace from a decoded message string.
 */
inline std::string CleanDecodedText(std::string decoded) {
    for (size_t pos = 0; (pos = decoded.find('{', pos)) != std::string::npos;) {
        const size_t end = decoded.find('}', pos);
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
                std::string  name = DecodeMesFromBuffer(SystemMes, info->msg + 0x64);
                const size_t nl = name.find('\n');
                if (nl != std::string::npos) {
                    name.resize(nl);
                }
                if (!name.empty()) {
                    return name;
                }
            }
        }
    }
    return std::string(fallback);
}

} // namespace btlmenu_detail
