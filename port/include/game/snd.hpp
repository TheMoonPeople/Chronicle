#pragma once

#include "common.h"

#include <libgraph.h>

struct TAG_PARAM;

class CCamera;
class CRect_i_;
class CTexture;
struct i;
struct spRGBA;
struct sceVif1Packet;
struct RECT;

/** Draws a projected sprite with one colour and fog depth. */
void set3DSpriteFog(sceVif1Packet *packet, CTexture *texture, const CRect_i_ &source, int *top_left, int *bottom_right, spRGBA *colour);

/** Draws a projected quadrilateral with per-corner depth and a uniform alpha. */
void set3DSpriteFog(sceVif1Packet *packet, CTexture *texture, const CRect_i_ &source, int *top_left, int *top_right, int *bottom_left, int *bottom_right, unsigned char alpha);

/**
 * Loads the sound-effect metadata table used by the sound manager.
 *
 * @mangled SndInitSeTable__Fv
 * @address 0x159390
 * @size 0x298
 */
void SndInitSeTable();

/**
 * Loads one character voice bank.
 *
 * @mangled SndVoiceLoad__Fi
 * @address 0x15B3F0
 * @size 0x90
 */
int SndVoiceLoad(int set_no);

/**
 * @file
 * Declares the sound effect calls that the game makes while it draws, and the
 * sprite calls that retail's sound translation unit also carries.
 */

/**
 * Resets the sound manager: the background music, the ambient loop and the
 * sound-effect sequences start again from nothing, no sound-effect, voice or
 * music set counts as loaded, and the configuration file names are cleared.
 * None of the four arguments is read.
 *
 * @mangled SndInitialize__Fiiii
 * @address 0x159200
 * @size 0x8C
 */
void SndInitialize(int unused0, int unused1, int unused2, int unused3);

/**
 * Stops the music, the ambient loop, sound-effect ports 10 and 12 to 15 and both
 * voices, and resets the sound-effect sequences, the background music and the
 * ambient loop.
 *
 * @mangled SndExit__Fv
 * @address 0x159290
 * @size 0xC0
 */
void SndExit();

/**
 * Steps the background-music fade, the sound-effect sequences and the sound
 * driver.
 *
 * @mangled SndStep__Fv
 * @address 0x159350
 * @size 0x34
 */
void SndStep();

/**
 * Resets the background music so that no set counts as loaded, playing or
 * fading, and returns 1.
 *
 * @mangled SndBgmInit__Fv
 * @address 0x159930
 * @size 0x2C
 */
int SndBgmInit();

/**
 * Sets whether the background music is switched off, stopping it when it is.
 *
 * @mangled SndBgmDisable__Fi
 * @address 0x159B80
 * @size 0x2C
 */
void SndBgmDisable(int disable);

/**
 * Returns whether the background music is switched off.
 *
 * @mangled SndGetBgmDisableFlag__Fv
 * @address 0x159BB0
 * @size 0xC
 */
int SndGetBgmDisableFlag();

/**
 * Stops every sound effect, the ambient loop and the second voice, and resets
 * the ambient loop so that none counts as playing.
 *
 * @mangled SndStopAllSe__Fv
 * @address 0x15A450
 * @size 0xA0
 */
void SndStopAllSe();

/**
 * Stops the background music at once.
 *
 * @mangled SndBgmStop__Fv
 * @address 0x159BA0
 * @size 0x58
 */
void SndBgmStop();

/**
 * Stops the playing background music and marks it paused for SndBgmRePlay.
 *
 * @mangled SndBgmPause__Fv
 * @address 0x159E60
 * @size 0x48
 */
void SndBgmPause();

/**
 * Starts the background music again when a set is loaded and its play state
 * is 2.
 *
 * @mangled SndBgmRePlay__Fv
 * @address 0x159C00
 * @size 0x4C
 */
void SndBgmRePlay();

/**
 * Fades the background music out and then stops it.
 *
 * @mangled SndBgmFadeOutStop__Fv
 * @address 0x159C50
 * @size 0xA8
 */
void SndBgmFadeOutStop();

/**
 * Stops the ambient loop.
 *
 * @mangled SndAmbientStop__Fv
 * @address 0x15B190
 * @size 0x44
 */
void SndAmbientStop();

/**
 * Starts an ambient loop at its default volume, unless that loop already plays.
 *
 * @mangled SndAmbientPlay__Fi
 * @address 0x15B110
 * @size 0x74
 */
void SndAmbientPlay(int ambient_no);

/** Sets the ambient loop's normalized volume. */
void SndAmbientSetVolf(float volume);

/**
 * Returns the default volume of the ambient loop that is playing, as its MIDI
 * state records it, or 64 when that loop has no state.
 *
 * @mangled SndGetAmbientDefaultVol__Fv
 * @address 0x15B2A0
 * @size 0x68
 */
int SndGetAmbientDefaultVol();

/**
 * Resets the ambient loop so that none counts as playing, and returns 1.
 *
 * @mangled SndAmbientInit__Fv
 * @address 0x15B0F0
 * @size 0x1C
 */
int SndAmbientInit();

/**
 * Works out the volume and the pan of a sound at a position.
 *
 * @mangled SndGetVolPan__FPfPfPfff
 * @address 0x15AC00
 * @size 0x1F8
 */
void SndGetVolPan(float *vol, float *pan, float *pos, float near, float far);

/**
 * Scales a sound effect's table volume by a normalized factor, and clamps the
 * result to the range the sound calls take.
 *
 * @mangled SndGetVolf__Fif
 * @address 0x15A970
 * @size 0xB4
 */
int SndGetVolf(int se_no, float vol);

/**
 * Converts a pan from -1 to 1, clamped to that range, into the pan from 1 to
 * 127 that the sound calls take.
 *
 * @mangled SndGetPanf__Ff
 * @address 0x15AA30
 * @size 0x74
 */
int SndGetPanf(float pan);

/**
 * Sets the volume of a sound effect.
 *
 * @mangled SndSetSeVol__Fiii
 * @address 0x15A900
 * @size 0x70
 */
void SndSetSeVol(int se_no, int vol, int voice);

/**
 * Sets the volume of a sound effect, as a share of the loudest volume.
 *
 * @mangled SndSetSeVolf__Fifi
 * @address 0x15AAB0
 * @size 0x5C
 */
void SndSetSeVolf(int se_no, float vol, int voice);

/**
 * Sets the pan of a sound effect, as a share of the widest pan.
 *
 * @mangled SndSetSePanf__Fifi
 * @address 0x15AB10
 * @size 0x80
 */
void SndSetSePanf(int se_no, float pan, int voice);

/**
 * Plays one sound effect with an integer volume and pan.
 *
 * @mangled SndSePlay__Fiii
 * @address 0x15A6B0
 * @size 0xA4
 */
void SndSePlay(int se_no, int vol, int voice);

/**
 * Plays a positional sound effect with distance attenuation.
 *
 * @mangled SndSePlay__FiPfff
 * @address 0x15A820
 * @size 0x90
 */
void SndSePlay(int se_no, float *position, float near, float far);

/**
 * Plays the sound of one foot at a world position.
 *
 * @mangled SndPlayFootSound__FiiPf
 * @address 0x15AB90
 * @size 0x68
 * @unknownret
 */
void SndPlayFootSound(int kind, int foot, float *position);

/**
 * Plays or stops one sound-effect sequence.
 *
 * @mangled SndSeSeqPlayStop__Fiii
 * @address 0x15AF20
 * @size 0x74
 */
int SndSeSeqPlayStop(int se_no, int length, int voice);

/**
 * Clears every sound-effect sequence slot.
 *
 * @mangled SndSeSeqInit__Fv
 * @address 0x15AED0
 * @size 0x4C
 */
void SndSeSeqInit();

/**
 * Turns the bilinear filter on or off for the sprites that follow.
 *
 * @mangled setbilinear__Fi
 * @address 0x15BC10
 * @size 0xC
 */
void setbilinear(int on);

/**
 * Draws a textured sprite using plain screen and texture rectangles.
 *
 * @mangled set2DSprite__FP13sceVif1PacketP8CTextureP4RECTP4RECTUc
 * @address 0x15F090
 * @size 0x68
 */
void set2DSprite(sceVif1Packet *packet, CTexture *texture, RECT *screen, RECT *texel, unsigned char alpha);

/**
 * Opens a run of sprites that all draw out of one texture.
 *
 * @mangled set2DSprite_Start__FP13sceVif1PacketP8CTexture
 * @address 0x14C220
 * @size 0x19C
 * @unknownret
 */
void set2DSprite_Start(sceVif1Packet *packet, CTexture *texture);

/**
 * Closes that run.
 *
 * @mangled set2DSprite_End__FP13sceVif1PacketP8CTexture
 * @address 0x14C4F0
 * @size 0xA4
 * @unknownret
 */
void set2DSprite_End(sceVif1Packet *packet, CTexture *texture);

/**
 * Sends the palette a texture draws through.
 *
 * @mangled SetClut__FP13sceVif1PacketP8CTextureP1i
 * @address 0x14C5A0
 * @size 0x60
 * @unknownret
 */
void SetClut(sceVif1Packet *packet, CTexture *texture, i *clut);

/**
 * Draws one part of a texture into a rectangle of the screen, at one alpha.
 *
 * @mangled set2DSprite__FP13sceVif1PacketP8CTextureRC8CRect_i_RC8CRect_i_Uc
 * @address 0x15C310
 * @size 0x318
 */
void set2DSprite(sceVif1Packet *packet, CTexture *texture, const CRect_i_ &screen, const CRect_i_ &texel, unsigned char alpha);

/**
 * Draws one part of a texture into a rectangle of the screen, at one colour.
 *
 * @mangled set2DSprite__FP13sceVif1PacketP8CTextureRC8CRect_i_RC8CRect_i_UcUcUcUc
 * @address 0x15C630
 * @size 0x350
 */
void set2DSprite(sceVif1Packet *packet, CTexture *texture, const CRect_i_ &screen, const CRect_i_ &texel, unsigned char red, unsigned char green, unsigned char blue, unsigned char alpha);

/**
 * Draws one part of a texture into a rectangle of the screen, shading each corner with its own colour.
 *
 * @mangled set2DSprite__FP13sceVif1PacketP8CTextureRC8CRect_i_RC8CRect_i_P6spRGBAP6spRGBAP6spRGBAP6spRGBAi
 * @address 0x15C980
 * @size 0x82C
 */
void set2DSprite(sceVif1Packet *packet, CTexture *texture, const CRect_i_ &screen, const CRect_i_ &texel, spRGBA *top_left, spRGBA *top_right, spRGBA *bottom_left, spRGBA *bottom_right, int mode);

/**
 * Draws one tinted texture rectangle rotated about a caller-supplied pivot.
 *
 * @mangled set2DSpriteRot__FP13sceVif1PacketP8CTextureRC8CRect_i_RC8CRect_i_iifUcUcUcUc
 * @address 0x15EAD0
 * @size 0x5BC
 */
void set2DSpriteRot(sceVif1Packet *packet, CTexture *texture, const CRect_i_ &screen, const CRect_i_ &texel, int pivot_x, int pivot_y, float angle, unsigned char red, unsigned char green, unsigned char blue, unsigned char alpha);

/**
 * Draws one part of a texture into a rectangle of the screen, turned about a point.
 *
 * @mangled set2DSprite__FP13sceVif1PacketP8CTextureRC8CRect_i_RC8CRect_i_iif
 * @address 0x15E540
 * @size 0x58C
 */
void set2DSprite(sceVif1Packet *packet, CTexture *texture, const CRect_i_ &screen, const CRect_i_ &texel, int pivot_x, int pivot_y, float angle);

/**
 * Draws a whole texture into a rectangle of the screen, turned about a point.
 *
 * @mangled set2DSprite__FP13sceVif1PacketP8CTextureRC8CRect_i_ii
 * @address 0x15BD00
 * @size 0x300
 */
void set2DSprite(sceVif1Packet *packet, CTexture *texture, const CRect_i_ &screen, int u, int v);

/**
 * Draws an untextured rectangle with a colour at each of its four corners.
 *
 * @mangled set2DSpriteC4__FP13sceVif1PacketRC8CRect_i_P6spRGBAP6spRGBAP6spRGBAP6spRGBA
 * @address 0x15E120
 * @size 0x41C
 */
void set2DSpriteC4(sceVif1Packet *packet, const CRect_i_ &screen, spRGBA *top_left, spRGBA *top_right, spRGBA *bottom_left, spRGBA *bottom_right);

/**
 * Draws one four-cornered shape of one colour.
 *
 * @mangled setColSprite__FP13sceVif1PacketPiPiPiPiUcUcUcUc
 * @address 0x15DED0
 * @size 0x250
 */
void setColSprite(sceVif1Packet *packet, int *top_left, int *top_right, int *bottom_left, int *bottom_right, unsigned char red, unsigned char green, unsigned char blue, unsigned char alpha);

/**
 * Draws a four-cornered shape of the screen in a flat colour, giving each
 * corner its own.
 *
 * @mangled set3DColSprite__FP13sceVif1PacketPiPiPiPiP6spRGBAP6spRGBAP6spRGBAP6spRGBA
 * @address 0x15D1B0
 * @size 0x2BC
 */
void set3DColSprite(sceVif1Packet *packet, int *top_left, int *top_right, int *bottom_left, int *bottom_right, spRGBA *top_left_colour, spRGBA *top_right_colour, spRGBA *bottom_left_colour, spRGBA *bottom_right_colour);

/**
 * Draws one part of a texture into a four-cornered shape of the screen, at one
 * alpha.
 *
 * @mangled set3DSprite__FP13sceVif1PacketP8CTextureRC8CRect_i_PiPiPiPiUc
 * @address 0x15D470
 * @size 0x34
 */
void set3DSprite(sceVif1Packet *packet, CTexture *texture, const CRect_i_ &source, int *top_left, int *top_right, int *bottom_left, int *bottom_right, unsigned char alpha);

/**
 * Draws one part of a texture into a projected quadrilateral with one colour.
 *
 * @mangled set3DSprite__FP13sceVif1PacketP8CTextureRC8CRect_i_PiPiPiPiP6spRGBA
 * @address 0x15D4B0
 * @size 0x2E0
 */
void set3DSprite(sceVif1Packet *packet, CTexture *texture, const CRect_i_ &source, int *top_left, int *top_right, int *bottom_left, int *bottom_right, spRGBA *colour);

/**
 * Draws a textured sprite in world space between two projected corners.
 *
 * @mangled set3DSprite__FP13sceVif1PacketP8CTextureRC8CRect_i_PiPiP6spRGBA
 * @address 0x15D790
 * @size 0x210
 */
void set3DSprite(sceVif1Packet *packet, CTexture *texture, const CRect_i_ &source, int *top_left, int *bottom_right, spRGBA *colour);

/**
 * Tells the sound where the camera stands, so that it can pan what it plays.
 *
 * @mangled SndSetCamera__FP7CCamera
 * @address 0x1596F0
 * @size 0x48
 */
void SndSetCamera(CCamera *camera);

/**
 * Sets the listener directly from position and rotation vectors.
 */
void SndSetCamera(float *position, float *rotation);

/**
 * Stops one playing sound-effect voice.
 */
void SndSeStop(int se_no, int voice);

/**
 * Sets the background music's volume.
 *
 * @mangled SndSetBgmVol__Fi
 * @address 0x159D20
 * @size 0x70
 * Sets the background-music volume.
 */
void SndSetBgmVol(int volume);

/**
 * Sets the background music's volume as a share of its default.
 *
 * @mangled SndSetBgmVolf__Ff
 * @address 0x159D90
 * @size 0x50
 */
void SndSetBgmVolf(float volume);

/**
 * Returns the background-music volume that is set now.
 *
 * @mangled SndGetBgmVol__Fv
 * @address 0x159DE0
 * @size 0xC
 */
int SndGetBgmVol();

/**
 * Returns the play state of the background music: 0 while it is stopped and 1
 * while it plays.
 *
 * @mangled SndBgmCheck__Fv
 * @address 0x159D00
 * @size 0xC
 */
int SndBgmCheck();

/**
 * Returns the number of the background music that is loaded now, or -1 when
 * no set counts as loaded.
 *
 * @mangled SndGetBgmNo__Fv
 * @address 0x159D10
 * @size 0xC
 */
int SndGetBgmNo();

/**
 * Returns whether the background music has no fade running.
 *
 * @mangled SndCheckFade__Fv
 * @address 0x15A090
 * @size 0x14
 */
int SndCheckFade();

/** Polls the background sound loader. */
int SndSyncBG();

/**
 * Returns the volume of the background-music sequence the MIDI player holds,
 * or zero when no background-music set is loaded.
 *
 * @mangled SndGetDefaultBgmVol__Fv
 * @address 0x159DF0
 * @size 0x48
 */
int SndGetDefaultBgmVol();

/**
 * Polls the background-music bank load, and hands the file to the driver once it lands.
 *
 * @mangled SndBgmSyncBG__Fv
 * @address 0x159AB0
 * @size 0x80
 */
int SndBgmSyncBG();

/**
 * Polls the sound-effect bank load, and hands the file to the driver once it lands.
 *
 * @mangled SndSoundSyncBG__Fv
 * @address 0x15A630
 * @size 0x80
 */
int SndSoundSyncBG();

/**
 * Polls the voice bank load, and hands the file to the driver once it lands.
 *
 * @mangled SndVoiceSyncBG__Fv
 * @address 0x15B530
 * @size 0x80
 */
int SndVoiceSyncBG();

/** Plays one menu sound effect. */
void SndSPSePlay(int se_no, int vol);

/**
 * Stops a playing special sound effect.
 *
 * @mangled SndSPSeStop__Fi
 * @address 0x15B900
 * @size 0x44
 */
void SndSPSeStop(int se_no);

/** Sets the normalized volume of a playing special sound effect. */
void SndSetSPSeVolf(int se_no, float volume);

/** Sets the normalized pan of a playing special sound effect. */
void SndSetSPSePanf(int se_no, float pan);
/** Returns zero once the background sound-effect load has finished. */
int SndSPSeSyncBG();

/**
 * Reads one background-music set and hands it to the driver, and returns 1;
 * returns 0 when that set is already loaded or its file cannot be read.
 *
 * @mangled SndBgmLoad__Fi
 * @address 0x159960
 * @size 0x90
 */
int SndBgmLoad(int set_no);

/**
 * Plays one track of the loaded background music set.
 *
 * @mangled SndBgmPlay__Fi
 * @address 0x159B30
 * @size 0x68
 */
void SndBgmPlay(int track_no);

/**
 * Returns the sound-effect set that is loaded now, or -1 when none is.
 *
 * @mangled SndGetNowSetNo__Fv
 * @address 0x15A440
 * @size 0xC
 */
int SndGetNowSetNo();

/**
 * Points the sound loader at its read buffer.
 *
 * @mangled SndSetReadBuffer__FPUi
 * @address 0x159630
 * @size 0x38
 */
void SndSetReadBuffer(unsigned int *buffer);

/**
 * Reads one sound-effect set and hands it to the driver, and returns 1;
 * returns 0 when that set is already loaded or its file cannot be read.
 *
 * @mangled SndSoundLoad__Fi
 * @address 0x15A4F0
 * @size 0x90
 */
int SndSoundLoad(int set_no);

/**
 * Starts loading a background-music bank into a caller-provided buffer, and
 * returns whether it did: zero when that bank is already the one loaded, or
 * when the load could not be started.
 */
int SndBgmLoadBG(int set_no, u_int *buffer, int *size);

/**
 * Starts loading a sound-effect bank into a caller-provided buffer, and
 * returns whether it did: zero when that set is already the one loaded, or
 * when the load could not be started.
 */
int SndSoundLoadBG(int set_no, u_int *buffer, int *size);

/**
 * Starts loading a voice bank into a caller-provided buffer, and returns
 * whether it did: zero when that set is already the one loaded, or when the
 * load could not be started.
 */
int SndVoiceLoadBG(int set_no, u_int *buffer, int *size);

/**
 * Reads one special-effect set and hands it to SetSPSeFile; returns 0 when the set is
 * already current or its file cannot be read, otherwise 1.
 *
 * @mangled SndSPSeLoad__Fi
 * @address 0x15B6D0
 * @size 0x90
 */
int SndSPSeLoad(int set_no);

/**
 * Starts loading a special-effect sound bank into a caller-provided buffer, and
 * returns whether the load was started.
 *
 * @mangled SndSPSeLoadBG__FiPUiPi
 * @address 0x15B760
 * @size 0x94
 */
int SndSPSeLoadBG(int set_no, u_int *buffer, int *size);

/** Fades a background-music stream in. */
void SndBgmFadeIn(int frames, int volume, int start_volume);

/** Fades a background-music stream out. */
void SndBgmFadeOut(int frames, int volume);

/**
 * Steps the background-music fade, and applies the volume it reaches.
 *
 * @mangled SndBgmFadeInOut__Fv
 * @address 0x159F90
 * @size 0xF4
 */
void SndBgmFadeInOut();

/**
 * Submits the blending register that sprites draw with.
 *
 * @mangled setAlphaFlag__FP13sceVif1PacketP10sceGsAlpha
 * @address 0x15BC20
 * @size 0xDC
 */
void setAlphaFlag(sceVif1Packet *packet, sceGsAlpha *alpha);

/**
 * Draws a sprite with the default alpha.
 *
 * @mangled set2DSprite__FP13sceVif1PacketP8CTextureRC8CRect_i_RC8CRect_i_
 * @address 0x15C000
 * @size 0x304
 */
void set2DSprite(sceVif1Packet *packet, CTexture *texture, const CRect_i_ &screen, const CRect_i_ &texel);

/** The two script tags LoadSoundInfo recognises. */
extern TAG_PARAM Command__3[];

/**
 * One row of a sound-effect table: what the sound driver is asked to play,
 * which port it plays on, and where its level comes from. A row the game does
 * not use holds -1 in its bank, program and port.
 */
struct SND_SE_INFO {
    s8  bank; /**< Bank the sound driver loads the effect from. */
    s8  prog; /**< Program within that bank. */
    s8  unk_2;
    s8  port;   /**< Port to play on; negative asks for the default port. */
    s16 vol_no; /**< Row of the sound object's effect table to take the level
                     from, or negative to play at full volume. */
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

/** One sound configuration read out of a sound script file. */
struct SND_INFO {
    s32 reverb_mode;   /**< Reverberation mode the REVERBE tag sets. */
    s32 reverb_depth;  /**< Reverberation depth the REVERBE tag sets. */
    s32 se_table;      /**< Sound-effect table number the TABLE tag names. */
    s32 se_table_type; /**< Sound-effect table kind the TABLE tag names. */
};

STATIC_ASSERT(sizeof(SND_INFO) == 0x10);

/** The sound configuration the command handlers fill in. */
extern SND_INFO *SoundInfo;

/** Count of entries the TABLE command has read this file. */
extern int se_list;

/** The basic sound-effect set the town maps use. */
extern SND_SE_INFO geo[199];

/** The basic sound-effect set the dungeon maps use. */
extern SND_SE_INFO dun[199];

/** The fixed sound-effect table, addressed by sound number. */
extern SND_SE_INFO se_info[2801];

/** The chapter sound-effect sets; entries the game never loads are zero. */
extern SND_SE_INFO *cap_se_info[101];

/** The character voice sets; entries the game never loads are zero. */
extern SND_SE_INFO *voice_info[11];

/** The menu sound-effect table, addressed by menu sound number. */
extern SND_SE_INFO special_se_info[65];
