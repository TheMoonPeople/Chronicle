#pragma once

#include <string>
#include <utility>
#include <vector>

namespace UI {

/**
 * Returns the default English localization key-value pairs for the Battle Menu and Weapon Menu.
 */
std::vector<std::pair<std::string, std::string>> BattleMenuStrings();

/**
 * Populates the localized title, help description, button labels, and prompt for the Save/Load entry.
 */
void GetLocalizedSaveLoadStrings(std::string &out_title, std::string &out_desc,
                                 std::string &out_save_btn, std::string &out_load_btn,
                                 std::string &out_modal_prompt);

/**
 * Populates the localized help title and description for a Battle Menu ring slot.
 */
void GetBattleMenuHelpData(int index, int menu_mode, std::string &out_title, std::string &out_desc);

/**
 * Returns the unquoted caption string displayed beside a Battle Menu ring icon.
 */
std::string GetBattleMenuCaption(int index, int menu_mode, bool ingame_load_active);

} // namespace UI