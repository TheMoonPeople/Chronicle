/**
 * @file
 * Declares the sound manager the game drives (music, ambient loops, sound
 * effects, voices and their background loads), and the sprite calls that
 * retail's sound translation unit also carries.
 */

#pragma once

#include "common.h"

#include <libgraph.h>

struct sceVif1Packet;

namespace dc::game {

class CCamera;
class CRect_i_;
class CTexture;
struct i;
struct spRGBA;
struct RECT;

/**
 * Starts the sound driver the first time it is called, and resets the sound
 * manager.
 */
void SndInit();

/**
 * Resets the sound manager: the background music, the ambient loop and the
 * sound-effect sequences start again from nothing, no sound-effect, voice or
 * music set counts as loaded, and the configuration file names are cleared.
 * None of the four arguments is read.
 */
void SndInitialize(int unused0, int unused1, int unused2, int unused3);

/**
 * Stops the music, the ambient loop, sound-effect ports 10 and 12 to 15 and both
 * voices, and resets the sound-effect sequences, the background music and the
 * ambient loop.
 */
void SndExit();

/**
 * Steps the background-music fade, the sound-effect sequences and the sound
 * driver.
 */
void SndStep();

/**
 * Resolves the driver's sound number for every row of every sound-effect
 * table the sound manager knows.
 */
void SndInitSeTable();

/**
 * Returns 1 while any of the background sound loads is still running, and 0
 * once they have all landed.
 */
int SndSyncBG();

/**
 * Points the sound loader at its read buffer.
 */
void SndSetReadBuffer(unsigned int *buffer);

/**
 * Tells the sound where the camera stands, so that it can pan what it plays.
 */
void SndSetCamera(CCamera *camera);

/**
 * Tells the sound where the listener stands and which way it looks.
 */
void SndSetCamera(float *position, float *rotation);

/**
 * Resets the background music so that no set counts as loaded, playing or
 * fading, and returns 1.
 */
int SndBgmInit();

/**
 * Sets whether the background music is switched off, stopping it when it is.
 */
void SndBgmDisable(int disable);

/**
 * Returns whether the background music is switched off.
 */
int SndGetBgmDisableFlag();

/**
 * Reads one background-music set and hands it to the driver, and returns 1;
 * returns 0 when that set is already loaded or its file cannot be read.
 */
int SndBgmLoad(int set_no);

/**
 * Starts loading a background-music bank into a caller-provided buffer, and
 * returns whether it did: zero when that bank is already the one loaded, or
 * when the load could not be started.
 */
int SndBgmLoadBG(int set_no, u_int *buffer, int *size);

/**
 * Polls the background-music bank load, and hands the file to the driver once
 * it lands.
 */
int SndBgmSyncBG();

/**
 * Plays one track of the loaded background music set.
 */
void SndBgmPlay(int track_no);

/**
 * Stops the background music at once.
 */
void SndBgmStop();

/**
 * Stops the playing background music and marks it paused for SndBgmRePlay.
 */
void SndBgmPause();

/**
 * Starts the background music again when a set is loaded and it is paused.
 */
void SndBgmRePlay();

/**
 * Fades the background music out over ten frames and then stops it.
 */
void SndBgmFadeOutStop();

/**
 * Returns the play state of the background music: 0 while it is stopped, 1
 * while it plays and 2 while it is paused.
 */
int SndBgmCheck();

/**
 * Returns the number of the background music that is loaded now, or -1 when
 * no set counts as loaded.
 */
int SndGetBgmNo();

/**
 * Sets the background music's volume, from 0 to 127.
 */
void SndSetBgmVol(int volume);

/**
 * Sets the background music's volume as a share of its default.
 */
void SndSetBgmVolf(float volume);

/**
 * Returns the background-music volume that is set now.
 */
int SndGetBgmVol();

/**
 * Returns the volume of the background-music sequence the MIDI player holds,
 * or zero when no background-music set is loaded.
 */
int SndGetDefaultBgmVol();

/**
 * Fades the background music from a start volume up to a volume over a number
 * of frames; a negative volume or start volume means the default or current one.
 */
void SndBgmFadeIn(int frames, int volume, int start_volume);

/**
 * Fades the background music from its current volume down to a volume over a
 * number of frames.
 */
void SndBgmFadeOut(int frames, int volume);

/**
 * Steps the background-music fade, and applies the volume it reaches.
 */
void SndBgmFadeInOut();

/**
 * Returns whether the background music has no fade running.
 */
int SndCheckFade();

/**
 * Returns the sound-effect set that is loaded now, or -1 when none is.
 */
int SndGetNowSetNo();

/**
 * Stops every sound effect, the ambient loop and the second voice, and resets
 * the ambient loop so that none counts as playing.
 */
void SndStopAllSe();

/**
 * Reads one sound-effect set and hands it to the driver, and returns 1;
 * returns 0 when that set is already loaded or its file cannot be read.
 */
int SndSoundLoad(int set_no);

/**
 * Starts loading a sound-effect bank into a caller-provided buffer, and
 * returns whether it did: zero when that set is already the one loaded, or
 * when the load could not be started.
 */
int SndSoundLoadBG(int set_no, u_int *buffer, int *size);

/**
 * Polls the sound-effect bank load, and hands the file to the driver once it
 * lands.
 */
int SndSoundSyncBG();

/**
 * Plays one sound effect at a volume from 0 to 127, or at its table volume
 * when the volume is negative.
 */
void SndSePlay(int se_no, int vol, int voice);

/**
 * Plays a sound effect at a world position, attenuated between a near and a
 * far distance; a negative distance picks the default one.
 */
void SndSePlay(int se_no, float *position, float near, float far);

/**
 * Stops one playing sound-effect voice.
 */
void SndSeStop(int se_no, int voice);

/**
 * Sets the volume of a sound effect.
 */
void SndSetSeVol(int se_no, int vol, int voice);

/**
 * Scales a sound effect's table volume by a normalized factor, and clamps the
 * result to the range the sound calls take.
 */
int SndGetVolf(int se_no, float vol);

/**
 * Converts a pan from -1 to 1, clamped to that range, into the pan from 1 to
 * 127 that the sound calls take.
 */
int SndGetPanf(float pan);

/**
 * Sets the volume of a sound effect, as a share of its table volume.
 */
void SndSetSeVolf(int se_no, float vol, int voice);

/**
 * Sets the pan of a sound effect, as a share of the widest pan.
 */
void SndSetSePanf(int se_no, float pan, int voice);

/**
 * Plays the sound of one foot at a world position.
 */
void SndPlayFootSound(int kind, int foot, float *position);

/**
 * Works out the volume and the pan of a sound at a position, from the camera
 * SndSetCamera last set.
 */
void SndGetVolPan(float *vol, float *pan, float *pos, float near, float far);

/**
 * Clears every sound-effect sequence slot.
 */
void SndSeSeqInit();

/**
 * Starts a sound-effect sequence that stops after a number of steps, or
 * restarts the count of one already running; returns 0 when no slot is free.
 */
int SndSeSeqPlayStop(int se_no, int length, int voice);

/**
 * Stops every running sound-effect sequence and frees its slot.
 */
void SndSeSeqAllStop();

/**
 * Resets the ambient loop so that none counts as playing, and returns 1.
 */
int SndAmbientInit();

/**
 * Starts an ambient loop at its default volume, unless that loop already plays.
 */
void SndAmbientPlay(int ambient_no);

/**
 * Stops the ambient loop.
 */
void SndAmbientStop();

/**
 * Sets the ambient loop's volume, clamped to 127; a negative volume is ignored.
 */
void SndAmbientSetVol(int volume);

/**
 * Sets the ambient loop's volume as a share of its default.
 */
void SndAmbientSetVolf(float volume);

/**
 * Returns the default volume of the ambient loop that is playing, as its MIDI
 * state records it, or 64 when that loop has no state.
 */
int SndGetAmbientDefaultVol();

/**
 * Reads one character voice set and hands it to the driver, and returns 1;
 * returns 0 when that set is already loaded or its file cannot be read.
 */
int SndVoiceLoad(int set_no);

/**
 * Starts loading a voice bank into a caller-provided buffer, and returns
 * whether it did: zero when that set is already the one loaded, or when the
 * load could not be started.
 */
int SndVoiceLoadBG(int set_no, u_int *buffer, int *size);

/**
 * Polls the voice bank load, and hands the file to the driver once it lands.
 */
int SndVoiceSyncBG();

/**
 * Reads one special-effect set and hands it to the driver; returns 0 when the
 * set is already current or its file cannot be read, otherwise 1.
 */
int SndSPSeLoad(int set_no);

/**
 * Starts loading a special-effect sound bank into a caller-provided buffer, and
 * returns whether the load was started.
 */
int SndSPSeLoadBG(int set_no, u_int *buffer, int *size);

/**
 * Polls the special-effect bank load, and returns zero once it has landed and
 * been handed to the driver.
 */
int SndSPSeSyncBG();

/**
 * Plays one menu sound effect at a volume, or at its table volume when the
 * volume is negative.
 */
void SndSPSePlay(int se_no, int vol);

/**
 * Stops a playing special sound effect.
 */
void SndSPSeStop(int se_no);

/**
 * Sets the volume of a playing special sound effect, as a share of its table
 * volume.
 */
void SndSetSPSeVolf(int se_no, float volume);

/**
 * Sets the pan of a playing special sound effect, as a share of the widest
 * pan.
 */
void SndSetSPSePanf(int se_no, float pan);

/**
 * Turns the bilinear filter on or off for the sprites that follow.
 */
void setbilinear(int on);

/**
 * What setbilinear last set (retail's linear__2): the filter the 2D units' sprite batches use.
 */
int PortBilinear();

/**
 * Submits the blending register that sprites draw with.
 */
void setAlphaFlag(sceVif1Packet *packet, sceGsAlpha *alpha);

/**
 * Opens a run of sprites that all draw out of one texture.
 */
void set2DSprite_Start(sceVif1Packet *packet, CTexture *texture);

/**
 * Closes a run of sprites that set2DSprite_Start opened.
 */
void set2DSprite_End(sceVif1Packet *packet, CTexture *texture);

/**
 * Sends the palette a texture draws through.
 */
void SetClut(sceVif1Packet *packet, CTexture *texture, i *clut);

/**
 * Draws a textured sprite using plain screen and texture rectangles.
 */
void set2DSprite(sceVif1Packet *packet, CTexture *texture, RECT *screen, RECT *texel, unsigned char alpha);

/**
 * Draws a sprite with the default alpha.
 */
void set2DSprite(sceVif1Packet *packet, CTexture *texture, const CRect_i_ &screen, const CRect_i_ &texel);

/**
 * Draws one part of a texture into a rectangle of the screen, at one alpha.
 */
void set2DSprite(sceVif1Packet *packet, CTexture *texture, const CRect_i_ &screen, const CRect_i_ &texel, unsigned char alpha);

/**
 * Draws one part of a texture into a rectangle of the screen, at one colour.
 */
void set2DSprite(sceVif1Packet *packet, CTexture *texture, const CRect_i_ &screen, const CRect_i_ &texel, unsigned char red, unsigned char green, unsigned char blue, unsigned char alpha);

/**
 * Draws one part of a texture into a rectangle of the screen, shading each
 * corner with its own colour.
 */
void set2DSprite(sceVif1Packet *packet, CTexture *texture, const CRect_i_ &screen, const CRect_i_ &texel, spRGBA *top_left, spRGBA *top_right, spRGBA *bottom_left, spRGBA *bottom_right, int mode);

/**
 * Draws one part of a texture into a rectangle of the screen, turned about a point.
 */
void set2DSprite(sceVif1Packet *packet, CTexture *texture, const CRect_i_ &screen, const CRect_i_ &texel, int pivot_x, int pivot_y, float angle);

/**
 * Draws a whole texture into a rectangle of the screen, turned about a point.
 */
void set2DSprite(sceVif1Packet *packet, CTexture *texture, const CRect_i_ &screen, int u, int v);

/**
 * Draws one tinted texture rectangle rotated about a caller-supplied pivot.
 */
void set2DSpriteRot(sceVif1Packet *packet, CTexture *texture, const CRect_i_ &screen, const CRect_i_ &texel, int pivot_x, int pivot_y, float angle, unsigned char red, unsigned char green, unsigned char blue, unsigned char alpha);

/**
 * Draws an untextured rectangle with a colour at each of its four corners.
 */
void set2DSpriteC4(sceVif1Packet *packet, const CRect_i_ &screen, spRGBA *top_left, spRGBA *top_right, spRGBA *bottom_left, spRGBA *bottom_right);

/**
 * Draws one four-cornered shape of one colour.
 */
void setColSprite(sceVif1Packet *packet, int *top_left, int *top_right, int *bottom_left, int *bottom_right, unsigned char red, unsigned char green, unsigned char blue, unsigned char alpha);

/**
 * Draws a four-cornered shape of the screen in a flat colour, giving each
 * corner its own.
 */
void set3DColSprite(sceVif1Packet *packet, int *top_left, int *top_right, int *bottom_left, int *bottom_right, spRGBA *top_left_colour, spRGBA *top_right_colour, spRGBA *bottom_left_colour, spRGBA *bottom_right_colour);

/**
 * Draws one part of a texture into a four-cornered shape of the screen, at one
 * alpha.
 */
void set3DSprite(sceVif1Packet *packet, CTexture *texture, const CRect_i_ &source, int *top_left, int *top_right, int *bottom_left, int *bottom_right, unsigned char alpha);

/**
 * Draws one part of a texture into a projected quadrilateral with one colour.
 */
void set3DSprite(sceVif1Packet *packet, CTexture *texture, const CRect_i_ &source, int *top_left, int *top_right, int *bottom_left, int *bottom_right, spRGBA *colour);

/**
 * Draws a textured sprite in world space between two projected corners.
 */
void set3DSprite(sceVif1Packet *packet, CTexture *texture, const CRect_i_ &source, int *top_left, int *bottom_right, spRGBA *colour);

/**
 * Draws a projected quadrilateral with per-corner depth, fogged, at one alpha.
 */
void set3DSpriteFog(sceVif1Packet *packet, CTexture *texture, const CRect_i_ &source, int *top_left, int *top_right, int *bottom_left, int *bottom_right, unsigned char alpha);

/**
 * Draws a projected sprite between two corners with one colour and fog depth.
 */
void set3DSpriteFog(sceVif1Packet *packet, CTexture *texture, const CRect_i_ &source, int *top_left, int *bottom_right, spRGBA *colour);

/**
 * One row of a sound-effect table: what the sound driver is asked to play,
 * which port it plays on, and where its level comes from. A row the game does
 * not use holds -1 in its bank, program and port; -128 in all four ends a table.
 */
struct SND_SE_INFO {
    s8  bank; /**< Bank the sound driver loads the effect from. */
    s8  prog; /**< Program within that bank. */
    s8  unk_2;
    s8  port;   /**< Port to play on; negative asks for the default port. */
    s16 vol_no; /**< Driver sound number the level comes from, or negative to play at full volume. */
};

STATIC_ASSERT(sizeof(SND_SE_INFO) == 6);

/**
 * One sound-effect sequence slot: a sound the game starts once and stops again
 * a fixed number of steps later. A slot whose sound number is negative is free.
 */
struct SND_SE_SEQ {
    s16 se_no;  /**< Sound effect the slot plays, or -1 while the slot is free. */
    s16 length; /**< Steps the sound is left playing for. */
    s16 step;   /**< Steps taken so far; the sound starts on step zero. */
    s16 voice;  /**< Voice the sound plays on. */
};

STATIC_ASSERT(sizeof(SND_SE_SEQ) == 8);

/**
 * One sound configuration read out of a sound script file.
 */
struct SND_INFO {
    s32 reverb_mode;   /**< Reverberation mode the REVERBE tag sets. */
    s32 reverb_depth;  /**< Reverberation depth the REVERBE tag sets. */
    s32 se_table;      /**< Sound-effect table number the TABLE tag names. */
    s32 se_table_type; /**< Sound-effect table kind the TABLE tag names. */
};

STATIC_ASSERT(sizeof(SND_INFO) == 0x10);

/**
 * Reads one sound configuration script, a sound pack's .cfg, through the
 * script interpreter; a command the script lacks leaves its fields zero.
 */
void LoadSoundInfo(SND_INFO *info, char *script, int script_size);

} // namespace dc::game
