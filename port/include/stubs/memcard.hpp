#pragma once

// menu_save.cpp calls ExitSaveSelect, and the platform save menu (port/src/memcard.cpp) runs the
// steps below, by their global names. Their definitions are local to this unit, so each gets a
// global symbol under its own name here.

namespace {

void ExitSaveSelect();
int  SaveMenuKeyFadeIn();
int  SaveMenuKeyFadeOut();
int  SaveMenuKeyModeSelect();
int  SaveMenuKeyCheckMcType();
int  SaveMenuKeyCheckMc();
int  SaveMenuKeyLoadConfig();

} // namespace

void PortExportExitSaveSelect() asm(PORT_ASM_NAME(_Z14ExitSaveSelectv));
int  PortExportSaveMenuKeyFadeIn() asm(PORT_ASM_NAME(_Z17SaveMenuKeyFadeInv));
int  PortExportSaveMenuKeyFadeOut() asm(PORT_ASM_NAME(_Z18SaveMenuKeyFadeOutv));
int  PortExportSaveMenuKeyModeSelect() asm(PORT_ASM_NAME(_Z21SaveMenuKeyModeSelectv));
int  PortExportSaveMenuKeyCheckMcType() asm(PORT_ASM_NAME(_Z22SaveMenuKeyCheckMcTypev));
int  PortExportSaveMenuKeyCheckMc() asm(PORT_ASM_NAME(_Z18SaveMenuKeyCheckMcv));
int  PortExportSaveMenuKeyLoadConfig() asm(PORT_ASM_NAME(_Z21SaveMenuKeyLoadConfigv));

void PortExportExitSaveSelect() {
    ExitSaveSelect();
}

int PortExportSaveMenuKeyFadeIn() {
    return SaveMenuKeyFadeIn();
}

int PortExportSaveMenuKeyFadeOut() {
    return SaveMenuKeyFadeOut();
}

int PortExportSaveMenuKeyModeSelect() {
    return SaveMenuKeyModeSelect();
}

int PortExportSaveMenuKeyCheckMcType() {
    return SaveMenuKeyCheckMcType();
}

int PortExportSaveMenuKeyCheckMc() {
    return SaveMenuKeyCheckMc();
}

int PortExportSaveMenuKeyLoadConfig() {
    return SaveMenuKeyLoadConfig();
}
