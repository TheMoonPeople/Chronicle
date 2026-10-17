#pragma once

// Keep the item picker's game logic available to the port's pointer wrapper.
#define EventItemSelectLoop PortEventItemSelectLoop

// memcard.cpp's SaveMenuFunc table names the save menu's steps, which this unit defines static:
// the PS2 link makes them global (object_fixups.json, globalize_symbols). Each gets a global
// symbol under the name the table references, which also keeps clang from dropping the static
// as unused.

static int SaveMenuKeySaveCheck();
static int SaveMenuKeySaveDecide();
static int SaveMenuKeySave();
static int SaveMenuKeyEndSave();
static int SaveMenuKeyLoadDecide();
static int SaveMenuKeyLoad();
static int SaveMenuKeyArart();
static int SaveMenuKeyNewDirSelect();
static int SaveMenuKeyNewDir();
static int SaveMenuKeyFormat();
static int SaveMenuKeyUnFormat();
static int SaveMenuKeyDifVersion();
static int SaveMenuKeyDelete();
static int SaveMenuKeyCopy();
static int SaveMenuKeyAfterEnding();
static int SaveMenuKeySaveDecideEnding();
static int SaveMenuKeySaveEnding();
static int SaveMenuKeyEndSaveEnding();

int PortExportSaveMenuKeySaveCheck() asm(PORT_ASM_NAME(_Z20SaveMenuKeySaveCheckv));
int PortExportSaveMenuKeySaveDecide() asm(PORT_ASM_NAME(_Z21SaveMenuKeySaveDecidev));
int PortExportSaveMenuKeySave() asm(PORT_ASM_NAME(_Z15SaveMenuKeySavev));
int PortExportSaveMenuKeyEndSave() asm(PORT_ASM_NAME(_Z18SaveMenuKeyEndSavev));
int PortExportSaveMenuKeyLoadDecide() asm(PORT_ASM_NAME(_Z21SaveMenuKeyLoadDecidev));
int PortExportSaveMenuKeyLoad() asm(PORT_ASM_NAME(_Z15SaveMenuKeyLoadv));
int PortExportSaveMenuKeyArart() asm(PORT_ASM_NAME(_Z16SaveMenuKeyArartv));
int PortExportSaveMenuKeyNewDirSelect() asm(PORT_ASM_NAME(_Z23SaveMenuKeyNewDirSelectv));
int PortExportSaveMenuKeyNewDir() asm(PORT_ASM_NAME(_Z17SaveMenuKeyNewDirv));
int PortExportSaveMenuKeyFormat() asm(PORT_ASM_NAME(_Z17SaveMenuKeyFormatv));
int PortExportSaveMenuKeyUnFormat() asm(PORT_ASM_NAME(_Z19SaveMenuKeyUnFormatv));
int PortExportSaveMenuKeyDifVersion() asm(PORT_ASM_NAME(_Z21SaveMenuKeyDifVersionv));
int PortExportSaveMenuKeyDelete() asm(PORT_ASM_NAME(_Z17SaveMenuKeyDeletev));
int PortExportSaveMenuKeyCopy() asm(PORT_ASM_NAME(_Z15SaveMenuKeyCopyv));
int PortExportSaveMenuKeyAfterEnding() asm(PORT_ASM_NAME(_Z22SaveMenuKeyAfterEndingv));
int PortExportSaveMenuKeySaveDecideEnding() asm(PORT_ASM_NAME(_Z27SaveMenuKeySaveDecideEndingv));
int PortExportSaveMenuKeySaveEnding() asm(PORT_ASM_NAME(_Z21SaveMenuKeySaveEndingv));
int PortExportSaveMenuKeyEndSaveEnding() asm(PORT_ASM_NAME(_Z24SaveMenuKeyEndSaveEndingv));

int PortExportSaveMenuKeySaveCheck() {
    return SaveMenuKeySaveCheck();
}

int PortExportSaveMenuKeySaveDecide() {
    return SaveMenuKeySaveDecide();
}

int PortExportSaveMenuKeySave() {
    return SaveMenuKeySave();
}

int PortExportSaveMenuKeyEndSave() {
    return SaveMenuKeyEndSave();
}

int PortExportSaveMenuKeyLoadDecide() {
    return SaveMenuKeyLoadDecide();
}

int PortExportSaveMenuKeyLoad() {
    return SaveMenuKeyLoad();
}

int PortExportSaveMenuKeyArart() {
    return SaveMenuKeyArart();
}

int PortExportSaveMenuKeyNewDirSelect() {
    return SaveMenuKeyNewDirSelect();
}

int PortExportSaveMenuKeyNewDir() {
    return SaveMenuKeyNewDir();
}

int PortExportSaveMenuKeyFormat() {
    return SaveMenuKeyFormat();
}

int PortExportSaveMenuKeyUnFormat() {
    return SaveMenuKeyUnFormat();
}

int PortExportSaveMenuKeyDifVersion() {
    return SaveMenuKeyDifVersion();
}

int PortExportSaveMenuKeyDelete() {
    return SaveMenuKeyDelete();
}

int PortExportSaveMenuKeyCopy() {
    return SaveMenuKeyCopy();
}

int PortExportSaveMenuKeyAfterEnding() {
    return SaveMenuKeyAfterEnding();
}

int PortExportSaveMenuKeySaveDecideEnding() {
    return SaveMenuKeySaveDecideEnding();
}

int PortExportSaveMenuKeySaveEnding() {
    return SaveMenuKeySaveEnding();
}

int PortExportSaveMenuKeyEndSaveEnding() {
    return SaveMenuKeyEndSaveEnding();
}
