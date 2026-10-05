#include <cstdio>
#include <cstring>

#include "audio/mixer.hpp"
#include "audio/trace.hpp"
#include "gameutil.hpp"

// The EZMIDI RPC as the port's mixer answers it. CSound (port/src/sound.cpp) talks to the mixer
// directly and never comes here; this keeps the command table answerable for anything else.
//
// Command words: the low nibble is the MIDI port.
//   0x00+p play            0x20+p stop            0x30+p rewind to song n
//   0x40+p set sequence    0xA0+p port attribute  0xB0+p port volume (256 full)
//   0xC0   stereo (1) / monaural (0)
//   0x8010 allocate stream input buffer  0x8090+p is playing  0x80E0+p port volume
//   0x9050+p bind bank     0x9070 copy bank body to sound memory

PC_OVERRIDE int ezMidiInit() {
    return 1;
}

PC_OVERRIDE int ezMidi(int command, int argument) {
    audio::Mixer &mixer = audio::DefaultMixer();
    const int     port = command & 0x0F;
    audio::Trace("ezMidi 0x%X 0x%X", command, argument);
    switch (command & ~0x0F) {
        case 0x00:
            mixer.Play(port);
            return 0;
        case 0x20:
            mixer.Stop(port);
            return 0;
        case 0x30:
            mixer.Rewind(port, argument);
            return 0;
        case 0xA0:
            mixer.SetAttribute(port, argument);
            return 0;
        case 0xB0:
            mixer.SetVolume(port, argument);
            return 0;
        case 0xC0:
            mixer.SetStereo(argument != 0);
            return 0;
        case 0x8090:
            return mixer.IsPlaying(port) ? 1 : 0;
        case 0x80E0:
            return mixer.Volume(port);
        default:
            // The rest carry IOP addresses, which the port has no equivalent of.
            std::fprintf(stderr, "ezMidi: command 0x%X takes an IOP address; ignored\n", command);
            return 0;
    }
}

PC_OVERRIDE int ezTransToIOP(void *iop_address, void *ee_address, int size) {
    std::memcpy(iop_address, ee_address, size);
    return 0;
}
