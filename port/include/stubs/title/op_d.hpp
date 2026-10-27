#pragma once

// The PS2 link tells the four FaceChange(int) definitions of the opening scenes apart by renaming
// them per object (object_fixups.json); op_c.cpp calls this unit's as FaceChangeD.
#define FaceChange FaceChangeD

// OpD_InitProcess, OpD_InitProcess2 and OpD_DrawProcess are defined in port/src/title/op_d.cpp,
// which reaches the state and helpers below by these assembler names.
class CCharacter;
class CFrameVu1;
class CSeireiKing;

/**
 * Ripple model on the hall floor, one frame per ripple drawn.
 */
extern CFrameVu1 *Hamon[4] asm("OpD_Hamon");

/**
 * Current scale of each ripple; the extra entry staggers the start.
 */
extern float HScale[5] asm("OpD_HScale");

/**
 * Prism model the spirit king turns above the girl.
 */
extern CCharacter Effect asm("OpD_Effect");

/**
 * Spark trail of the spirit king.
 */
extern CSeireiKing SeireiKing asm("OpD_SeireiKing");

/**
 * Ambient step the script raises in the hall.
 */
extern int amb3 asm("OpD_amb3");

/**
 * Sky model of the meadow, which holds the frame the lens flare aims at.
 */
extern CFrameVu1 *SkyFrame asm("OpD_SkyFrame");

/**
 * Writes the sky dome's two alternating vertex colours into its model.
 */
void SkyColor(CFrameVu1 *frame) asm("OpD_SkyColor");

/**
 * Draws the prism and the spark trail the spirit king carries.
 */
void EffectAtraPrizum() asm("OpD_EffectAtraPrizum");

/**
 * Aims the second point light from the spirit king at the given position.
 */
void RollLight(float *target) asm("OpD_RollLight");

/**
 * Steps and draws the spirit king's trail at the given size.
 */
void EffectSeireiKing(float size) asm("OpD_EffectSeireiKing");

/**
 * Draws the sun's lens flare when the sun projects onto the screen.
 */
void LensFreaProcess() asm("OpD_LensFreaProcess");

/**
 * Draws the meadow caption and its cross-fading pieces and star trails.
 */
void Setsumei() asm("OpD_Setsumei");

/**
 * Grows, fades and draws the hall floor's ripples.
 */
void HamonProcess() asm("OpD_HamonProcess");
