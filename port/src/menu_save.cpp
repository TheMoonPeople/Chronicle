#include "menu_save.hpp"

#include <filesystem>
#include <system_error>

#include "memcard.hpp"
#include "memorycardaccess.hpp"
#include "save_slots.hpp"

// Message numbers of allmenu.mes the save screens show.
enum SaveMenuMessage {
    SAVE_MES_NONE = 0,
    SAVE_MES_WANT_TO_SAVE = 260,  /**< "Want to save?" */
    SAVE_MES_SAVED = 262,         /**< "Saving completed." */
    SAVE_MES_REPLACE = 264,       /**< "Data exist on file N. Replace it?" */
    SAVE_MES_SAVE_FAILED = 266,   /**< "Saving failed." */
    SAVE_MES_WANT_TO_LOAD = 270,  /**< "Want to load?" */
    SAVE_MES_LOAD_FAILED = 274,   /**< "Loading failed." */
    SAVE_MES_FORMAT_FAILED = 282, /**< "Formatting failed." */
    SAVE_MES_DELETE = 290,        /**< "Delete file?" */
    SAVE_MES_COPY = 292,          /**< "Want to copy file?" */
    SAVE_MES_OTHER_VERSION = 299, /**< "Data not loaded due to change in data format. Deleting No. N data." */
};

// Retail's, with no message that names the memory card. The card operations finish within the
// frame after the one that starts them and show nothing; the save after the ending asks "Want to
// save?" where retail asked to save the cleared data to the card; the alerts only a card raised
// say the save or the load failed.
PORT_OVERRIDE int GetSaveMenuMsgNo() {
    if (McAccess.GetFuncNo() != MC_OPERATION_IDLE) {
        return SAVE_MES_NONE;
    }

    switch (SaveMenu.key_no) {
        case SAVE_KEY_SAVE_DECIDE:
            return SAVE_MES_REPLACE;
        case SAVE_KEY_LOAD_DECIDE:
            return SAVE_MES_WANT_TO_LOAD;
        case SAVE_KEY_ALERT:
            switch (SaveMenu.alert_no) {
                case SAVE_ALERT_NONE:
                case SAVE_ALERT_UNK_3:
                case SAVE_ALERT_UNK_4:
                case SAVE_ALERT_UNK_5:
                    return SAVE_MES_NONE;
                case SAVE_ALERT_FORMAT_FAILED:
                    return SAVE_MES_FORMAT_FAILED;
                case SAVE_ALERT_SAVE_FAILED:
                    return SAVE_MES_SAVE_FAILED;
                case SAVE_ALERT_LOAD_FAILED:
                    return SAVE_MES_LOAD_FAILED;
                default:
                    return SaveMenu.access_kind == SAVE_ACCESS_LOAD ? SAVE_MES_LOAD_FAILED : SAVE_MES_SAVE_FAILED;
            }
        case SAVE_KEY_DELETE:
            return SAVE_MES_DELETE;
        case SAVE_KEY_COPY:
            return SAVE_MES_COPY;
        case SAVE_KEY_DIF_VERSION:
            return SAVE_MES_OTHER_VERSION;
        case SAVE_KEY_AFTER_ENDING:
            return SAVE_MES_WANT_TO_SAVE;
        case SAVE_KEY_END_SAVE:
        case SAVE_KEY_END_SAVE_ENDING:
            return SAVE_MES_SAVED;
        default:
            return SAVE_MES_NONE;
    }
}

// Retail's looked for a card holding the save directory and its configuration. Data exists once
// state.json or a save folder does.
PORT_OVERRIDE int InitExistData() {
    if (McAccess.InitForMC() != 0) {
        return 0;
    }

    McAccess.port = 0;
    McAccess.LoadSysConfig();
    std::error_code error;
    return std::filesystem::exists(SaveStatePath(), error) || !SaveSlotFiles().empty();
}
