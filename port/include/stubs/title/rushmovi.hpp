#pragma once

// The unit's scene loading and drawing functions are title.cpp's global ones, which these bind to.
void PortTitleDataLoad() asm(PORT_ASM_NAME(_Z8DataLoadv));
void PortTitleDrawProcA() asm(PORT_ASM_NAME(_Z9DrawProcAv));
void PortTitleDrawProcB() asm(PORT_ASM_NAME(_Z9DrawProcBv));
void PortTitleDrawProcC() asm(PORT_ASM_NAME(_Z9DrawProcCv));
void PortTitleDrawProcD() asm(PORT_ASM_NAME(_Z9DrawProcDv));
void PortTitleDrawProcE() asm(PORT_ASM_NAME(_Z9DrawProcEv));
void PortTitleDrawProcF() asm(PORT_ASM_NAME(_Z9DrawProcFv));
void PortTitleDrawProcG() asm(PORT_ASM_NAME(_Z9DrawProcGv));
void PortTitleDrawProcH() asm(PORT_ASM_NAME(_Z9DrawProcHv));
void PortTitleDrawProcI() asm(PORT_ASM_NAME(_Z9DrawProcIv));
void PortTitleDrawProcTitle() asm(PORT_ASM_NAME(_Z13DrawProcTitlev));

namespace {
void DataLoad() {
    PortTitleDataLoad();
}

void DrawProcA() {
    PortTitleDrawProcA();
}

void DrawProcB() {
    PortTitleDrawProcB();
}

void DrawProcC() {
    PortTitleDrawProcC();
}

void DrawProcD() {
    PortTitleDrawProcD();
}

void DrawProcE() {
    PortTitleDrawProcE();
}

void DrawProcF() {
    PortTitleDrawProcF();
}

void DrawProcG() {
    PortTitleDrawProcG();
}

void DrawProcH() {
    PortTitleDrawProcH();
}

void DrawProcI() {
    PortTitleDrawProcI();
}

void DrawProcTitle() {
    PortTitleDrawProcTitle();
}
} // namespace
