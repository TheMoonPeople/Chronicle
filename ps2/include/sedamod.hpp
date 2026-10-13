#pragma once

/**
 * @file
 * The Seda mod's hook into Toan's attacks (docs/SEDA.md). Toan's attack code times hits, its charged
 * attack's leap, its trails and sounds by frames of Toan's own motion. Another character in his place
 * plays its own motion's frames, so with the mod those times are first turned back into Toan's: the
 * same share of the way through the motion playing, in Toan's motion of the same number.
 *
 * Retail has none of this; without MOD_SEDA TOAN_TIME is the time itself and the weapon is drawn.
 */

#ifdef MOD_SEDA

/** The frame of Toan's motion that the main character's time in its playing motion stands for. */
float SedaToanTime(float time);

#define TOAN_TIME(time) SedaToanTime(time)

/** Whether the equipped weapon is drawn: not while the character in Toan's place shows its own. */
bool SedaEquippedDrawn();

class CCharacter;

/**
 * Whether the character in Toan's place leaves out a motion set its configuration reads: Toan's
 * other sets (events, presentations) are made for Toan's skeleton.
 */
bool SedaRefusesMotionSet(CCharacter *chara, int set);

#else

#define TOAN_TIME(time) (time)

#endif
