#pragma once

// The PS2 link tells the four FaceChange(int) definitions of the opening scenes apart by renaming
// them per object (object_fixups.json); op_c.cpp calls this unit's as FaceChangeD.
#define FaceChange FaceChangeD

// The three functions that index op_b's OP_NornMapObj are the port's (port/src/title/op_d.cpp),
// which lays the array out with the host's CMapObject. The state and the helpers they share with
// the rest of this unit are static here; these give them global names the port can reach.
class CCharacter;
class CFrameVu1;
class CSeireiKing;

extern CFrameVu1  *Hamon[4] asm("OpD_Hamon");
extern float       HScale[5] asm("OpD_HScale");
extern CCharacter  Effect asm("OpD_Effect");
extern CSeireiKing SeireiKing asm("OpD_SeireiKing");
extern int         amb3 asm("OpD_amb3");
extern CFrameVu1  *SkyFrame asm("OpD_SkyFrame");

static void SkyColor(CFrameVu1 *frame);
static void EffectAtraPrizum();
static void RollLight(float *target);
static void EffectSeireiKing(float size);
static void LensFreaProcess();
static void Setsumei();
static void HamonProcess();

void PortOpDSkyColor(CFrameVu1 *frame) asm("OpD_SkyColor");
void PortOpDEffectAtraPrizum() asm("OpD_EffectAtraPrizum");
void PortOpDRollLight(float *target) asm("OpD_RollLight");
void PortOpDEffectSeireiKing(float size) asm("OpD_EffectSeireiKing");
void PortOpDLensFreaProcess() asm("OpD_LensFreaProcess");
void PortOpDSetsumei() asm("OpD_Setsumei");
void PortOpDHamonProcess() asm("OpD_HamonProcess");

void PortOpDSkyColor(CFrameVu1 *frame) {
    SkyColor(frame);
}

void PortOpDEffectAtraPrizum() {
    EffectAtraPrizum();
}

void PortOpDRollLight(float *target) {
    RollLight(target);
}

void PortOpDEffectSeireiKing(float size) {
    EffectSeireiKing(size);
}

void PortOpDLensFreaProcess() {
    LensFreaProcess();
}

void PortOpDSetsumei() {
    Setsumei();
}

void PortOpDHamonProcess() {
    HamonProcess();
}
