#pragma once

#include "common.h"

namespace dc::game {

/**
 * MIDI ports the sound functions address.
 */
// clang-format off
enum MidiPort {
    MIDI_PORT_BGM        = 0,  /**< Background music. */
    MIDI_PORT_AMBIENT    = 1,  /**< Ambient loop. */
    MIDI_PORT_UNK_2      = 2,  /**< Sequence port with no caller. */
    MIDI_PORT_UNK_A      = 10, /**< Sound effect port. */
    MIDI_PORT_UNK_B      = 11, /**< Sound effect port. */
    MIDI_PORT_SE_SPECIAL = 12, /**< Special sound effects. */
    MIDI_PORT_UNK_D      = 13, /**< Sound effect port. */
    MIDI_PORT_SE_DEFAULT = 14, /**< Default sound effect port. */
    MIDI_PORT_SE_TITLE   = 15, /**< Title sound effects. */
};

// clang-format on

/**
 * One sequence the MIDI player holds, which is what the music is
 * played and faded through.
 */
struct MIDI_SEQUENCE {
    char name[12]; /**< File name of the sequence, without its extension. */
    s32  volume;   /**< Volume the sequence plays at, out of 256. */
};

/**
 * The sequence description table and how many entries it holds.
 */
struct SQ_INF_TABLE {
    MIDI_SEQUENCE sequence[400]; /**< Each sequence's description. */
    s32           count;         /**< Number of entries read. */
};

STATIC_ASSERT(sizeof(SQ_INF_TABLE) == 0x1904);

/**
 * One volume fade the sequencer advances once a frame.
 */
struct MIDI_FADE {
    s32   active;        /**< Non-zero while the fade runs. */
    s32   target_volume; /**< Volume the fade stops at. */
    float volume;        /**< Volume the fade has reached. */
    float step;          /**< Volume added each frame. */
};

STATIC_ASSERT(sizeof(MIDI_FADE) == 0x10);

/**
 * What the MIDI player holds for one port: the bank loaded into the
 * sound processor, the sequences loaded beside it, and its fades.
 */
struct MIDI_PORT {
    void          *bank;                 /**< IOP address of the port's bank header, or null. */
    s32            spu_address;          /**< Sound processor address the port's bank body loads to. */
    void          *sequence_address[10]; /**< IOP address of each loaded sequence. */
    MIDI_SEQUENCE *sequence[10];         /**< Description of each loaded sequence. */
    s32            sequence_count;       /**< Number of sequences loaded. */
    MIDI_FADE      fade[2];              /**< The port's two volume fades. */
    u8             unk_7C[4];
};

STATIC_ASSERT(sizeof(MIDI_PORT) == 0x80);

/**
 * What the MIDI player is doing, one record per port.
 */
struct MIDI_STATE {
    MIDI_PORT port[8]; /**< State of each port. */
};

STATIC_ASSERT(sizeof(MIDI_STATE) == 0x400);

/**
 * The bank a load is handing to the MIDI player: where its header
 * and body sit in IOP memory and where the body goes in the sound
 * processor.
 */
struct MIDI_BANK {
    void *hd_address;  /**< IOP address of the bank header. */
    void *bd_address;  /**< IOP address of the bank body. */
    s32   bd_size;     /**< Size of the bank body in bytes. */
    s32   spu_address; /**< Sound processor address the body loads to. */
    u8    unk_10[0x30];
};

STATIC_ASSERT(sizeof(MIDI_BANK) == 0x40);

/**
 * How one sound effect is played: the bank and program it comes
 * from and its volume.
 */
struct SE_INF {
    s8  bank;    /**< Bank the effect plays from. */
    s8  program; /**< Program within the bank. */
    s16 volume;  /**< Volume the effect plays at. */
};

/**
 * The sound-effect description table and how many entries it holds.
 */
struct SE_INF_TABLE {
    SE_INF entry[3000]; /**< Each effect's description. */
    s32    count;       /**< Number of entries read. */
};

STATIC_ASSERT(sizeof(SE_INF_TABLE) == 0x2EE4);

/**
 * The game's interface to the sound driver: banks, sequences, effects and fades.
 */
class CSound {
public:
    /**
     * Gives the MIDI player's state, one record per port.
     */
    MIDI_STATE *GetMidiState();

    /**
     * Gives the table describing each sound effect.
     */
    short *GetSeInfTbl();

    /**
     * Gives the bank entry one sound effect plays from.
     */
    int GetSeNo(int bank, int program);

    /**
     * Silences one hardware voice.
     */
    void StopVoice(int core);

    /**
     * Sets the reverberation mode and depth of one channel.
     */
    void SetReverb(int core, int mode, int depth);

    /**
     * Reads one sound file out of an archive.
     */
    int LoadSoundFileFromPack(char *name, unsigned int *pack);

    /**
     * Reads the sequence description table.
     */
    int LoadSqInf(char *name, unsigned int *buffer);

    /**
     * Reads the sound-effect description table.
     */
    int LoadSeInf(char *name, unsigned int *buffer);

    /**
     * Starts the sound system and its sequencer.
     */
    int Init(int mode0, int mode1, int depth0, int depth1);

    /**
     * Starts a sequence playing.
     */
    void SQ_Play(int port, int seq_no);

    /**
     * Starts a sequence playing at a given volume.
     */
    void SQ_Play(int port, int seq_no, int volume);

    /**
     * Starts a sequence again from its beginning.
     */
    void SQ_RePlay(int port);

    /**
     * Plays one sound effect with every parameter given.
     */
    void SE_Play(int port, int bank, int program, int pan, int velocity, int volume, int voice);

    /**
     * Plays one sound effect, taking its parameters from the description table.
     */
    void SE_Play(int port, int se_no, int voice);

    /**
     * Plays a sound effect with the default pan and priority settings.
     */
    void SE_Play(int port, int bank, int program, int volume, int voice);

    /**
     * Plays one sound effect at the middle pan and full volume.
     */
    void SE_Play(int port, int bank, int program, int voice);

    /**
     * Sets the volume of a sounding effect.
     */
    void SE_SetVol(int port, int bank, int program, int volume, int voice);

    /**
     * Sets the stereo position of a sounding effect.
     */
    void SE_SetPan(int port, int bank, int program, int pan, int voice);

    /**
     * Sets the stereo position of a sounding effect, naming it through the description
     * table.
     */
    void SE_SetPan(int port, int se_no, int pan, int voice);

    /**
     * Stops a sounding effect.
     */
    void SE_Stop(int port, int bank, int program, int voice);

    /**
     * Fades a channel towards a volume over a number of steps.
     */
    void Fade(int port, float step, int volume);

    /**
     * Advances the fades and the sequencer by one frame.
     */
    void Step();

    /**
     * Stops a sequence.
     */
    void Stop(int port);

    /**
     * Sets a sequence's volume.
     */
    void SetVol(int port, int volume);

    /**
     * Chooses between stereo and monaural output.
     */
    void SetStereoMode(int mode);

    /**
     * Loads the ambient bank into the sound processor.
     */
    int LoadHdBd_A(int hd, int hd_size, int bd, int bd_size);

    /**
     * Loads the common bank into the sound processor.
     */
    int LoadHdBd_C(int hd, int hd_size, int bd, int bd_size);

    /**
     * Loads the effect bank into the sound processor.
     */
    int LoadHdBd_E(int hd, int hd_size, int bd, int bd_size);

    /**
     * Loads the georama bank into the sound processor.
     */
    int LoadHdBd_G(int hd, int hd_size, int bd, int bd_size);

    /**
     * Loads the interior bank into the sound processor.
     */
    int LoadHdBd_I(int hd, int hd_size, int bd, int bd_size);

    /**
     * Loads the music bank into the sound processor.
     */
    int LoadHdBd_M(int hd, int hd_size, int bd, int bd_size);

    /**
     * Loads the quest bank into the sound processor.
     */
    int LoadHdBd_Q(int hd, int hd_size, int bd, int bd_size);

    /**
     * Loads the system bank into the sound processor.
     */
    int LoadHdBd_S(int hd, int hd_size, int bd, int bd_size);

    /**
     * Loads the ambient sequence set through the sound processor's heap.
     */
    int LoadSeq_A(int address, int size);

    /**
     * Loads the effect sequence set through the sound processor's heap.
     */
    int LoadSeq_E(int address, int size);

    /**
     * Loads the interior sequence set through the sound processor's heap.
     */
    int LoadSeq_I(int address, int size);
};

STATIC_ASSERT(sizeof(CSound) == 1);

/**
 * The sound driver the whole game plays through. It holds nothing of its own:
 * every call reaches the platform sound backend.
 */
extern CSound CSnd;

} // namespace dc::game
