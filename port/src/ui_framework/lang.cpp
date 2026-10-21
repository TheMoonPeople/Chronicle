#include "ui_framework/lang.hpp"

#include "battlemenu.hpp"
#include "localize.hpp"
#include "menu_draw.hpp"
#include "menu_dungeon.hpp"
#include "menu_misc.hpp"
#include "menuetc.hpp"

namespace UI {

namespace {

/**
 * Localization keys and fallback English text for a Battle Menu ring entry.
 */
struct HelpSpec {
    const char *title_key; /**< Localization key for the quoted entry title. */
    const char *title_def; /**< Default English quoted title string. */
    const char *desc_key;  /**< Localization key for the help description body. */
    const char *desc_def;  /**< Default English help description string. */
};

} // namespace

std::vector<std::pair<std::string, std::string>> BattleMenuStrings() {
    return {
        {"battlemenu.item.title",         "\"Item\""                                     },
        {"battlemenu.item.help",          "Use current item or\nSet active items."       },
        {"battlemenu.weapon.title",       "\"Weapon\""                                   },
        {"battlemenu.weapon.help",        "Change and customise\nweapons."               },
        {"battlemenu.allies.title",       "\"Allies\""                                   },
        {"battlemenu.allies.help",        "Change controllable\ncharacter."              },
        {"battlemenu.georama.title",      "\"Georama Parts\""                            },
        {"battlemenu.georama.help",       "Check acquired\ngeorama parts."               },
        {"battlemenu.dungeon.title",      "\"Go to Dungeon\""                            },
        {"battlemenu.dungeon.help",       "Go to Divine\nBeast Cave."                    },
        {"battlemenu.world_map.title",    "\"World Map\""                                },
        {"battlemenu.world_map.help",     "Move to another\narea on world map."          },
        {"battlemenu.go_outside.title",   "\"Go Outside\""                               },
        {"battlemenu.go_outside.help",    "Go outside of\nthe building."                 },
        {"battlemenu.leave_dungeon.title", "\"Leave Dungeon\""                             },
        {"battlemenu.escape.help",        "Escape from the\ncurrent dungeon."            },
        {"battlemenu.save.title",         "\"Save\""                                     },
        {"battlemenu.save.help",          "Record current\ngame progress."               },
        {"battlemenu.option.title",       "\"Options\""                                  },
        {"battlemenu.option.help",        "Change game\nconfigurations."                 },
        {"battlemenu.journal.title",      "\"Journal\""                                  },
        {"battlemenu.journal.help",       "Check manuals and\nchest tracker."            },
        {"battlemenu.quit.title",         "\"Quit\""                                       },
        {"battlemenu.quit.help",          "Exit the game and\nreturn to desktop."        },
        {"battlemenu.quit.confirm_title", "\"Quit\""                                       },
        {"battlemenu.quit.confirm_desc",  "Return to desktop?"                           },
        {"battlemenu.quit.yes",           "YES"                                          },
        {"battlemenu.quit.no",            "NO"                                           },
        {"weaponmenu.action.equip",       "EQUIP"                                        },
        {"weaponmenu.action.customize",   "CUSTOMIZE"                                    },
        {"weaponmenu.action.attribute",   "ATTRIBUTE"                                    },
        {"weaponmenu.action.repair",      "REPAIR"                                       },
        {"weaponmenu.action.upgrade",     "UPGRADE"                                      },
        {"weaponmenu.action.statusbreak", "STATUS BREAK"                                 },
        {"weaponmenu.action.buildup",     "BUILD-UP"                                     },
        {"weaponmenu.action.discard",     "DUMP"                                         },
        {"weaponmenu.upgrade.confirm",    "Upgrade. Confirm?"                            },
        {"weaponmenu.buildup.confirm",    "Build-up. Confirm?"                           },
        {"weaponmenu.break.confirm",      "Status break this weapon?\nWeapon disappears."},
        {"weaponmenu.discard.confirm",    "Throw away this weapon?"                      },
        {"weaponmenu.buildup.prompt",     "Select weapon to build up to:"                },
    };
}

void GetLocalizedSaveLoadStrings(std::string &out_title, std::string &out_desc,
                                 std::string &out_save_btn, std::string &out_load_btn,
                                 std::string &out_modal_prompt) {
    out_save_btn = LocalizeText("battlemenu.save.title", "\"Save\"");
    if (out_save_btn.size() >= 2 && out_save_btn.front() == '"' && out_save_btn.back() == '"') {
        out_save_btn = out_save_btn.substr(1, out_save_btn.size() - 2);
    }
    // In capitals, as the load button's are; the menu title it comes from is not. Only the ASCII letters:
    // the bytes of a UTF-8 character stay as they are.
    for (char &c : out_save_btn) {
        if (c >= 'a' && c <= 'z') {
            c = static_cast<char>(c - 'a' + 'A');
        }
    }

    switch (GetMenuLangFlag()) {
        case LANG_FRENCH:
            out_load_btn = "CHARGER";
            out_title = "\"Sauver/Charger\"";
            out_desc = "Sauvegarder ou\ncharger une partie.";
            out_modal_prompt = "Choisir une action :";
            break;
        case LANG_GERMAN:
            out_load_btn = "LADEN";
            out_title = "\"Speichern/Laden\"";
            out_desc = "Fortschritt speichern\noder Spielstand laden.";
            out_modal_prompt = "Aktion wählen:";
            break;
        case LANG_ITALIAN:
            out_load_btn = "CARICA";
            out_title = "\"Salva/Carica\"";
            out_desc = "Salva i progressi o\ncarica una partita.";
            out_modal_prompt = "Scegli un'azione:";
            break;
        case LANG_SPANISH:
            out_load_btn = "CARGAR";
            out_title = "\"Guardar/Cargar\"";
            out_desc = "Guardar progreso o\ncargar una partida.";
            out_modal_prompt = "Elige una acción:";
            break;
        default:
            out_load_btn = "LOAD";
            out_title = "\"Save/Load\"";
            out_desc = "Save Progress or\nLoad a save.";
            out_modal_prompt = "Select an action:";
            break;
    }
}

void GetBattleMenuHelpData(int index, int menu_mode, std::string &out_title, std::string &out_desc) {
    out_title.clear();
    out_desc.clear();

    static constexpr HelpSpec common_top[4] = {
        {"battlemenu.item.title",    "\"Item\"",          "battlemenu.item.help",    "Use current item or\nSet active items."},
        {"battlemenu.weapon.title",  "\"Weapon\"",        "battlemenu.weapon.help",  "Change and customise\nweapons."        },
        {"battlemenu.allies.title",  "\"Allies\"",        "battlemenu.allies.help",  "Change controllable\ncharacter."       },
        {"battlemenu.georama.title", "\"Georama Parts\"", "battlemenu.georama.help", "Check acquired\ngeorama parts."        },
    };
    static constexpr HelpSpec option_spec = {"battlemenu.option.title", "\"Options\"", "battlemenu.option.help", "Change game\nconfigurations."};
    static constexpr HelpSpec journal_spec = {"battlemenu.journal.title", "\"Journal\"", "battlemenu.journal.help", "Check manuals and\nchest tracker."};
    static constexpr HelpSpec quit_spec    = {"battlemenu.quit.title",    "\"Quit\"", "battlemenu.quit.help",    "Exit the game and\nreturn to desktop."};

    const HelpSpec *spec = nullptr;

    if (index >= 0 && index < 4) {
        spec = &common_top[index];
    } else if (index == 4) {
        if (menu_mode == BATTLE_MENU_MODE_DUNGEON) {
            // Leaving always works, by half the money, an Escape Powder or for nothing once the floor is clear.
            out_title = LocalizeText("battlemenu.leave_dungeon.title", "\"Leave Dungeon\"");
            out_desc  = LocalizeText("battlemenu.escape.help", "Escape from the\ncurrent dungeon.");
        } else {
            switch (NowGetGameFlagForBtlMenu(BtlMenuMode)) {
                case MENU_MOVE_WORLD_MAP:
                    out_title = LocalizeText("battlemenu.world_map.title", "\"World Map\"");
                    out_desc = LocalizeText("battlemenu.world_map.help", "Move to another\narea on world map.");
                    break;
                case MENU_MOVE_INTERIOR_OUT:
                    out_title = LocalizeText("battlemenu.go_outside.title", "\"Go Outside\"");
                    out_desc = LocalizeText("battlemenu.go_outside.help", "Go outside of\nthe building.");
                    break;
                default:
                    out_title = LocalizeText("battlemenu.dungeon.title", "\"Go to Dungeon\"");
                    out_desc = LocalizeText("battlemenu.dungeon.help", "Go to Divine\nBeast Cave.");
                    break;
            }
        }
        return;
    } else if (menu_mode == BATTLE_MENU_MODE_DUNGEON) {
        if (index == 5) {
            spec = &option_spec;
        } else if (index == 6) {
            spec = &journal_spec;
        } else if (index == 7) {
            spec = &quit_spec;
        }
    } else {
        if (index == 5) {
            std::string save_btn;
            std::string load_btn;
            std::string modal_prompt;
            GetLocalizedSaveLoadStrings(out_title, out_desc, save_btn, load_btn, modal_prompt);
            return;
        } else if (index == 6) {
            spec = &option_spec;
        } else if (index == 7) {
            spec = &journal_spec;
        } else if (index == 8) {
            spec = &quit_spec;
        }
    }

    if (spec != nullptr) {
        out_title = LocalizeText(spec->title_key, spec->title_def);
        out_desc = LocalizeText(spec->desc_key, spec->desc_def);
    }
}

std::string GetBattleMenuCaption(int index, int menu_mode, bool ingame_load_active) {
    const int  menu_flag = BattleMenuFlag;
    const bool in_main_ring = (menu_flag == BTLMENU_STATE_MAIN ||
                               menu_flag == BTLMENU_STATE_APPEAR ||
                               menu_flag == BTLMENU_STATE_EXIT);
    if (!in_main_ring && menu_mode != BATTLE_MENU_MODE_DUNGEON && index == 5) {
        std::string title;
        std::string desc;
        std::string save_btn;
        std::string load_btn;
        std::string prompt;
        GetLocalizedSaveLoadStrings(title, desc, save_btn, load_btn, prompt);
        return ingame_load_active ? load_btn : save_btn;
    }

    std::string title;
    std::string desc;
    GetBattleMenuHelpData(index, menu_mode, title, desc);
    if (title.size() >= 2 && title.front() == '"' && title.back() == '"') {
        title = title.substr(1, title.size() - 2);
    }
    return title;
}

} // namespace UI
