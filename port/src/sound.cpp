#include "sound.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "audio/mixer.hpp"
#include "audio/trace.hpp"
#include "dataread.hpp"
#include "platform/audio.hpp"

// The IOP's MIDI player is the port's audio::Mixer. Banks and sequences are copied into it rather
// than into IOP and SPU memory, so the SPU address bookkeeping below survives only to keep
// GetMidiState() reading as it did; nothing addresses sound memory by it.

namespace {

constexpr int kSlots = 8;
constexpr int kSequencesPerSlot = 10;
constexpr int kFirstEffectPort = MIDI_PORT_UNK_A;
constexpr int kLastEffectPort = MIDI_PORT_SE_TITLE;

MIDI_STATE   midi_state;
SQ_INF_TABLE sq_inf_tbl;
SE_INF_TABLE se_inf_tbl;

// What TransHdBd hands to the bind that follows it, as gBank did.
struct PendingBank {
    std::shared_ptr<audio::Bank> bank;
    int                          bd_size = 0;
    int                          spu_address = 0;
} g_bank;

std::shared_ptr<audio::Bank>   g_slot_banks[kSlots];
std::shared_ptr<audio::SqFile> g_slot_sequences[kSlots][kSequencesPerSlot];

// libmodmsin queued effect messages until Step() sent them; the port keeps that timing.
struct EffectMessage {
    int                         port;
    bool                        extended;
    std::uint32_t               message;
    std::array<std::uint8_t, 8> bytes;
};

std::vector<EffectMessage> g_effect_queue;

struct FadeRoute {
    int port;
    int slot;
    int fade;
};

constexpr FadeRoute kFadeRoutes[] = {
    {MIDI_PORT_BGM,        0, 0},
    {MIDI_PORT_AMBIENT,    2, 0},
    {MIDI_PORT_UNK_2,      4, 0},
    {MIDI_PORT_SE_TITLE,   1, 1},
    {MIDI_PORT_SE_DEFAULT, 4, 1},
    {MIDI_PORT_UNK_A,      3, 1},
    {MIDI_PORT_UNK_D,      5, 1},
    {MIDI_PORT_SE_SPECIAL, 6, 1},
    {MIDI_PORT_UNK_B,      7, 1},
};

audio::Mixer &Player() {
    return audio::DefaultMixer();
}

void RenderOutput(void *, float *out, int frames) {
    audio::DefaultMixer().Render(out, frames);
}

// LoadHdBd_* and LoadSeq_* take their buffers as int, as retail's 32-bit pointers were. Only this
// unit calls them, always with pointers below 4 GiB from the non-PIE image; LoadSoundFileFromPack
// itself passes real pointers to the helpers and never goes through an int.
const std::uint8_t *FromInt(int address) {
    return reinterpret_cast<const std::uint8_t *>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(address)));
}

int SequenceSlot(int port) {
    switch (port) {
        case MIDI_PORT_BGM:
            return 0;
        case MIDI_PORT_AMBIENT:
            return 2;
        case MIDI_PORT_UNK_2:
            return 4;
        default:
            return -1;
    }
}

// Retail sends a port outside 10-15 to the stream input channel its switch leaves unset, which
// reads as channel 0, port 10.
int EffectPort(int port) {
    return port >= kFirstEffectPort && port <= kLastEffectPort ? port : kFirstEffectPort;
}

void QueueProgram(int port, int bank) {
    g_effect_queue.push_back({EffectPort(port), false, static_cast<std::uint32_t>(((bank & 0x7F) << 8) | 0xC0), {}});
}

void QueueExtended(int port, std::array<std::uint8_t, 8> bytes) {
    g_effect_queue.push_back({EffectPort(port), true, 0, bytes});
}

int TransHdBdData(const std::uint8_t *hd, int hd_size, const std::uint8_t *bd, int bd_size) {
    if (bd_size == 0 || bd == nullptr) {
        std::printf("BD LOAD Err!!!!!!!!!!!!!!!!!!!!!\n");
        g_bank.bd_size = 0;
        g_bank.bank.reset();
        return -1;
    }
    if (hd_size == 0 || hd == nullptr) {
        std::printf("HD LOAD Err!!!!!!!!!!!!!!!!!!!!!\n");
        g_bank.bd_size = 0;
        g_bank.bank.reset();
        return -1;
    }
    g_bank.bank = audio::Bank::Create({hd, static_cast<std::size_t>(hd_size)}, {bd, static_cast<std::size_t>(bd_size)});
    g_bank.bd_size = bd_size;
    if (g_bank.bank == nullptr) {
        std::printf("sound: unreadable HD bank (%d bytes)\n", hd_size);
        return -1;
    }
    const audio::HdBank &header = g_bank.bank->Header();
    audio::Trace("bank: hd %d bytes, bd %d bytes: %zu programs, %zu sample sets, %zu samples, %zu vags",
                 hd_size, bd_size, header.programs.size(), header.sample_sets.size(), header.samples.size(),
                 header.vags.size());
    return 0;
}

void FreeSlot(int slot) {
    MIDI_PORT &state = midi_state.port[slot];
    g_slot_banks[slot].reset();
    state.bank = nullptr;
    for (auto &sequence : g_slot_sequences[slot]) {
        sequence.reset();
    }
    state.sequence_count = 0;
}

void TakeBank(int slot) {
    g_slot_banks[slot] = g_bank.bank;
    midi_state.port[slot].bank = g_bank.bank.get();
}

void BindPort(int slot, int port, int attribute) {
    Player().BindBank(port, g_slot_banks[slot]);
    Player().SetAttribute(port, attribute);
    Player().SetVolume(port, 0);
}

int LoadSlotA(const std::uint8_t *hd, int hd_size, const std::uint8_t *bd, int bd_size) {
    if (midi_state.port[0].bank != nullptr) {
        Player().Stop(MIDI_PORT_BGM);
        FreeSlot(0);
    }
    g_bank.spu_address = midi_state.port[0].spu_address;
    TransHdBdData(hd, hd_size, bd, bd_size);
    TakeBank(0);
    BindPort(0, MIDI_PORT_BGM, 0x3020);
    return 0;
}

int LoadSlotC(const std::uint8_t *hd, int hd_size, const std::uint8_t *bd, int bd_size) {
    if (midi_state.port[1].bank != nullptr) {
        Player().Stop(MIDI_PORT_SE_TITLE);
        Player().Stop(MIDI_PORT_AMBIENT);
        FreeSlot(1);
        // Retail frees the ambient bank here without unbinding port 1; the port unbinds it so
        // nothing plays from a freed bank.
        FreeSlot(2);
        Player().BindBank(MIDI_PORT_AMBIENT, nullptr);
        midi_state.port[2].spu_address = midi_state.port[1].spu_address = 0x7D010;
    }
    g_bank.spu_address = midi_state.port[1].spu_address;
    TransHdBdData(hd, hd_size, bd, bd_size);
    TakeBank(1);
    midi_state.port[2].spu_address = midi_state.port[1].spu_address + g_bank.bd_size + 0x10;
    BindPort(1, MIDI_PORT_SE_TITLE, 0x3070);
    return 0;
}

int LoadSlotE(const std::uint8_t *hd, int hd_size, const std::uint8_t *bd, int bd_size) {
    if (midi_state.port[2].bank != nullptr) {
        Player().Stop(MIDI_PORT_AMBIENT);
        FreeSlot(2);
    }
    g_bank.spu_address = midi_state.port[2].spu_address;
    TransHdBdData(hd, hd_size, bd, bd_size);
    TakeBank(2);
    BindPort(2, MIDI_PORT_AMBIENT, 0x3060);
    return 0;
}

int LoadSlotG(const std::uint8_t *hd, int hd_size, const std::uint8_t *bd, int bd_size) {
    if (midi_state.port[3].bank != nullptr) {
        Player().Stop(MIDI_PORT_UNK_A);
        FreeSlot(3);
    }
    g_bank.spu_address = midi_state.port[4].spu_address - (bd_size + 0x10);
    midi_state.port[3].spu_address = g_bank.spu_address;
    TransHdBdData(hd, hd_size, bd, bd_size);
    TakeBank(3);
    BindPort(3, MIDI_PORT_UNK_A, 0x3040);
    return 0;
}

int LoadSlotI(const std::uint8_t *hd, int hd_size, const std::uint8_t *bd, int bd_size) {
    if (midi_state.port[4].bank != nullptr) {
        Player().Stop(MIDI_PORT_UNK_2);
        Player().Stop(MIDI_PORT_SE_DEFAULT);
        FreeSlot(4);
        FreeSlot(3);
        Player().BindBank(MIDI_PORT_UNK_A, nullptr);
        midi_state.port[3].spu_address = midi_state.port[4].spu_address = 0x16E900;
    }
    g_bank.spu_address = midi_state.port[4].spu_address - (bd_size + 0x10);
    TransHdBdData(hd, hd_size, bd, bd_size);
    midi_state.port[4].spu_address = g_bank.spu_address;
    TakeBank(4);
    BindPort(4, MIDI_PORT_UNK_2, 0x3050);
    BindPort(4, MIDI_PORT_SE_DEFAULT, 0x3040);
    return 0;
}

int LoadSlotSimple(int slot, int port, int attribute, const std::uint8_t *hd, int hd_size, const std::uint8_t *bd,
                   int bd_size) {
    if (midi_state.port[slot].bank != nullptr) {
        Player().Stop(port);
        FreeSlot(slot);
    }
    g_bank.spu_address = midi_state.port[slot].spu_address;
    TransHdBdData(hd, hd_size, bd, bd_size);
    TakeBank(slot);
    BindPort(slot, port, attribute);
    return 0;
}

int LoadSequence(int slot, const std::uint8_t *data, int size) {
    MIDI_PORT &state = midi_state.port[slot];
    if (state.sequence_count >= kSequencesPerSlot) {
        std::printf("sound: more than %d sequences on slot %d\n", kSequencesPerSlot, slot);
        return -1;
    }
    auto sequence = data != nullptr && size > 0 ? audio::SqFile::Create({data, static_cast<std::size_t>(size)}) : nullptr;
    if (sequence == nullptr) {
        std::printf("sound: unreadable SQ sequence (%d bytes)\n", size);
    } else {
        audio::Trace("sequence: slot %d #%d, %d bytes, %zu songs", slot, state.sequence_count, size,
                     sequence->Songs().size());
    }
    g_slot_sequences[slot][state.sequence_count] = sequence;
    state.sequence_address[state.sequence_count] = sequence.get();
    return 0;
}

void RegisterSequence(int slot, const char *name) {
    MIDI_PORT &state = midi_state.port[slot];
    int        i;
    for (i = 0; i < sq_inf_tbl.count; i++) {
        if (std::strncmp(name, sq_inf_tbl.sequence[i].name, 9) == 0) {
            break;
        }
    }
    if (i < sq_inf_tbl.count && state.sequence_count < kSequencesPerSlot) {
        state.sequence[state.sequence_count] = &sq_inf_tbl.sequence[i];
        state.sequence_count++;
    } else {
        std::printf("################SQ_TBL NOT FOUND NEME=%s ##################\n", name);
    }
}

void StartSequence(int port, int seq_no, const int *volume) {
    const int slot = SequenceSlot(port);
    if (slot < 0) {
        return;
    }
    MIDI_PORT &state = midi_state.port[slot];
    if (seq_no < 0 || seq_no >= state.sequence_count) {
        std::printf("###############NOT FOUND SEQ_NO=%d #####################\n", seq_no);
        return;
    }
    audio::Trace("SQ_Play port %d seq %d (%.9s) volume %d%s", port, seq_no, state.sequence[seq_no]->name,
                 volume != nullptr ? *volume : state.sequence[seq_no]->volume,
                 volume != nullptr ? "" : " (table)");
    if (volume != nullptr) {
        Player().Stop(port);
    }
    Player().SetSequence(port, g_slot_sequences[slot][seq_no]);
    Player().SetVolume(port, volume != nullptr ? *volume : state.sequence[seq_no]->volume);
    Player().Rewind(port, 0);
    Player().Play(port);
}

} // namespace

PC_OVERRIDE MIDI_STATE *CSound::GetMidiState() {
    return &midi_state;
}

PC_OVERRIDE short *CSound::GetSeInfTbl() {
    return reinterpret_cast<short *>(&se_inf_tbl);
}

PC_OVERRIDE int CSound::GetSeNo(int bank, int program) {
    for (int i = 0; i < se_inf_tbl.count; i++) {
        if (bank == se_inf_tbl.entry[i].bank && program == se_inf_tbl.entry[i].program) {
            return i;
        }
    }
    return -1;
}

PC_OVERRIDE void CSound::StopVoice(int core) {
    audio::Trace("StopVoice core %d", core);
    Player().KeyOffCore(core);
}

PC_OVERRIDE void CSound::SetReverb(int core, int mode, int depth) {
    audio::Trace("SetReverb core %d mode %d depth %d", core, mode, depth);
    Player().SetReverb(core, mode, depth);
}

PC_OVERRIDE void set_spu(int mode0, int mode1, int depth0, int depth1) {
    Player().SetReverb(0, mode0, depth0);
    Player().SetReverb(1, mode1, depth1);
}

PC_OVERRIDE int TransHdBd(int hd, int hd_size, int bd, int bd_size) {
    return TransHdBdData(FromInt(hd), hd_size, FromInt(bd), bd_size);
}

PC_OVERRIDE int CSound::LoadSoundFileFromPack(char *name, unsigned int *pack) {
    int size = 0;
    std::printf("SND_INF= %s \n", name);
    const char *list = reinterpret_cast<const char *>(GetPackFile(pack, name, &size));
    if (list == nullptr) {
        std::printf("sound: %s not in pack\n", name);
        return 0;
    }

    // Each line names one member; its first nine characters are a six-character stem whose last
    // letter picks the slot, and the extension.
    const std::string text(list, strnlen(list, size));
    std::size_t       start = 0;
    while (start < text.size()) {
        std::size_t end = text.find('\n', start);
        if (end == std::string::npos) {
            end = text.size();
        }
        const std::string line = text.substr(start, end - start);
        start = end + 1;
        if (line.empty()) {
            continue;
        }

        char name_hd[10] = {};
        char name_bd[10] = {};
        std::strncpy(name_hd, line.c_str(), 9);
        if (std::strlen(name_hd) < 9) {
            continue;
        }
        const char  kind = name_hd[5];
        const char *ext = &name_hd[6];

        if (std::strncmp(ext, ".sq", 3) == 0) {
            const int slot = kind == 'a' ? 0 : kind == 'e' ? 2
                                           : kind == 'i'   ? 4
                                                           : -1;
            if (slot >= 0) {
                int         sq_size = 0;
                const auto *sq = reinterpret_cast<const std::uint8_t *>(GetPackFile(pack, name_hd, &sq_size));
                audio::Trace("load %s (slot letter %c)", name_hd, kind);
                LoadSequence(slot, sq, sq_size);
                RegisterSequence(slot, name_hd);
            }
            continue;
        }
        if (std::strncmp(ext, ".hd", 3) != 0) {
            continue;
        }
        audio::Trace("load %s (slot letter %c)", name_hd, kind);
        std::memcpy(name_bd, name_hd, 7);
        std::strcpy(&name_bd[7], "bd");
        int         hd_size = 0;
        int         bd_size = 0;
        const auto *hd = reinterpret_cast<const std::uint8_t *>(GetPackFile(pack, name_hd, &hd_size));
        const auto *bd = reinterpret_cast<const std::uint8_t *>(GetPackFile(pack, name_bd, &bd_size));
        switch (kind) {
            case 'a':
                LoadSlotA(hd, hd_size, bd, bd_size);
                break;
            case 'c':
                LoadSlotC(hd, hd_size, bd, bd_size);
                break;
            case 'e':
                LoadSlotE(hd, hd_size, bd, bd_size);
                break;
            case 'g':
                LoadSlotG(hd, hd_size, bd, bd_size);
                break;
            case 'i':
                LoadSlotI(hd, hd_size, bd, bd_size);
                break;
            case 'm':
                LoadSlotSimple(5, MIDI_PORT_UNK_D, 0x3080, hd, hd_size, bd, bd_size);
                break;
            case 'q':
                LoadSlotSimple(6, MIDI_PORT_SE_SPECIAL, 0x3090, hd, hd_size, bd, bd_size);
                break;
            case 's':
                LoadSlotSimple(7, MIDI_PORT_UNK_B, 0x3090, hd, hd_size, bd, bd_size);
                break;
            default:
                break;
        }
    }
    return 0;
}

namespace {

bool IsTableDelimiter(char c) {
    return c == ' ' || c == ';' || c == ',' || c == '\t' || c == '\n' || c == '\r';
}

// The tables are delimited text after one header line. Retail terminates the text at the size
// LoadFile reports, so nothing past it, sector padding included, is read.
std::vector<std::string> ReadTable(char *name, unsigned int *buffer, int &size) {
    size = 0;
    LoadFile(name, buffer, &size);
    const std::string_view   text(reinterpret_cast<const char *>(buffer), strnlen(reinterpret_cast<const char *>(buffer), size));
    std::vector<std::string> tokens;
    std::size_t              at = text.find_first_of("\n\r");
    while (at < text.size()) {
        while (at < text.size() && IsTableDelimiter(text[at])) {
            at++;
        }
        const std::size_t begin = at;
        while (at < text.size() && !IsTableDelimiter(text[at])) {
            at++;
        }
        if (at > begin) {
            tokens.emplace_back(text.substr(begin, at - begin));
        }
    }
    return tokens;
}

int Number(const std::string &token) {
    return std::atoi(token.substr(0, 4).c_str());
}

} // namespace

PC_OVERRIDE int CSound::LoadSqInf(char *name, unsigned int *buffer) {
    int        size;
    const auto tokens = ReadTable(name, buffer, size);
    for (std::size_t i = 0; i + 1 < tokens.size() && sq_inf_tbl.count < 400; i += 2) {
        MIDI_SEQUENCE &sequence = sq_inf_tbl.sequence[sq_inf_tbl.count];
        std::memset(sequence.name, 0, sizeof(sequence.name));
        std::strncpy(sequence.name, tokens[i].c_str(), 9);
        sequence.volume = std::atoi(tokens[i + 1].c_str());
        sq_inf_tbl.count++;
    }
    return size;
}

PC_OVERRIDE int CSound::LoadSeInf(char *name, unsigned int *buffer) {
    int        size;
    const auto tokens = ReadTable(name, buffer, size);
    se_inf_tbl.count = 0;
    for (std::size_t i = 0; i + 2 < tokens.size() && se_inf_tbl.count < 3000; i += 3) {
        SE_INF &entry = se_inf_tbl.entry[se_inf_tbl.count];
        entry.bank = Number(tokens[i]);
        entry.program = Number(tokens[i + 1]);
        entry.volume = Number(tokens[i + 2]);
        se_inf_tbl.count++;
    }
    return size;
}

PC_OVERRIDE int CSound::Init(int mode0, int mode1, int depth0, int depth1) {
    static bool output_started = false;
    if (!output_started) {
        output_started = true;
        if (!AudioOutputStart(Player().Rate(), RenderOutput, nullptr)) {
            std::printf("sound: no audio output, playing silently\n");
        }
    }
    set_spu(mode0, mode1, depth0, depth1);
    g_effect_queue.clear();

    static constexpr int kSpuAddress[kSlots] = {0x5010, 0x7D010, 0x7D010, 0x16E900, 0x16E900, 0x16E900, 0x18AE20, 0x1B6D40};
    for (int slot = 0; slot < kSlots; slot++) {
        MIDI_PORT &state = midi_state.port[slot];
        state.bank = nullptr;
        state.spu_address = kSpuAddress[slot];
        for (void *&address : state.sequence_address) {
            address = nullptr;
        }
        state.sequence_count = 0;
        state.fade[0].active = false;
        state.fade[1].active = false;
    }
    return 0;
}

PC_OVERRIDE void CSound::SQ_Play(int port, int seq_no) {
    StartSequence(port, seq_no, nullptr);
}

PC_OVERRIDE void CSound::SQ_Play(int port, int seq_no, int volume) {
    StartSequence(port, seq_no, &volume);
}

PC_OVERRIDE void CSound::SQ_RePlay(int port) {
    audio::Trace("SQ_RePlay port %d", port);
    Player().Play(port);
}

PC_OVERRIDE void CSound::SE_Play(int port, int bank, int program, int pan, int velocity, int volume, int voice) {
    audio::Trace("SE_Play port %d bank %d program %d pan %d velocity %d volume %d voice %d", port, bank,
                 program, pan, velocity, volume, voice);
    QueueProgram(port, bank);
    QueueExtended(port, {0xF9, 0, 0, static_cast<std::uint8_t>(volume), 0});
    QueueExtended(port, {0xF9, 1, 0, static_cast<std::uint8_t>(pan), 0});
    QueueExtended(port, {0xFD, 0x10, 0, static_cast<std::uint8_t>(program), static_cast<std::uint8_t>(voice),
                         static_cast<std::uint8_t>(velocity), 0});
}

PC_OVERRIDE void CSound::SE_Play(int port, int se_no, int voice) {
    if (se_no < 0 || se_no >= se_inf_tbl.count) {
        return;
    }
    const SE_INF &entry = se_inf_tbl.entry[se_no];
    SE_Play(port, entry.bank, entry.program, 0x40, 0x7F, entry.volume, voice);
}

PC_OVERRIDE void CSound::SE_Play(int port, int bank, int program, int volume, int voice) {
    SE_Play(port, bank, program, 0x40, 0x7F, volume, voice);
}

PC_OVERRIDE void CSound::SE_Play(int port, int bank, int program, int voice) {
    SE_Play(port, bank, program, 0x40, 0x7F, 0x7F, voice);
}

PC_OVERRIDE void CSound::SE_SetVol(int port, int bank, int program, int volume, int voice) {
    audio::Trace("SE_SetVol port %d bank %d program %d volume %d voice %d", port, bank, program, volume,
                 voice);
    QueueProgram(port, bank);
    QueueExtended(port, {0xFD, 0, 0, static_cast<std::uint8_t>(program), static_cast<std::uint8_t>(voice),
                         static_cast<std::uint8_t>(volume), 0});
}

PC_OVERRIDE void CSound::SE_SetPan(int port, int bank, int program, int pan, int voice) {
    audio::Trace("SE_SetPan port %d bank %d program %d pan %d voice %d", port, bank, program, pan, voice);
    QueueProgram(port, bank);
    QueueExtended(port, {0xFD, 1, 0, static_cast<std::uint8_t>(program), static_cast<std::uint8_t>(voice),
                         static_cast<std::uint8_t>(pan), 0});
}

PC_OVERRIDE void CSound::SE_SetPan(int port, int se_no, int pan, int voice) {
    if (se_no < 0 || se_no >= se_inf_tbl.count) {
        return;
    }
    SE_SetPan(port, se_inf_tbl.entry[se_no].bank, se_inf_tbl.entry[se_no].program, pan, voice);
}

PC_OVERRIDE void CSound::SE_Stop(int port, int bank, int program, int voice) {
    audio::Trace("SE_Stop port %d bank %d program %d voice %d", port, bank, program, voice);
    QueueProgram(port, bank);
    QueueExtended(port, {0xFD, 0x10, 0, static_cast<std::uint8_t>(program), static_cast<std::uint8_t>(voice), 0, 0});
}

PC_OVERRIDE void CSound::Fade(int port, float step, int volume) {
    audio::Trace("Fade port %d step %g to %d", port, step, volume);
    for (const FadeRoute &route : kFadeRoutes) {
        if (route.port == port) {
            MIDI_FADE &fade = midi_state.port[route.slot].fade[route.fade];
            fade.active = true;
            fade.target_volume = volume;
            fade.step = step;
            fade.volume = Player().Volume(port);
            return;
        }
    }
}

PC_OVERRIDE void CSound::Step() {
    for (const FadeRoute &route : kFadeRoutes) {
        MIDI_FADE &fade = midi_state.port[route.slot].fade[route.fade];
        if (!fade.active) {
            continue;
        }
        fade.volume += fade.step;
        if (fade.step > 0.0f && fade.volume > fade.target_volume) {
            fade.volume = fade.target_volume;
            fade.active = false;
        }
        if (fade.step < 0.0f && fade.volume < fade.target_volume) {
            fade.volume = fade.target_volume;
            fade.active = false;
        }
        SetVol(route.port, static_cast<int>(fade.volume));
    }

    for (const EffectMessage &message : g_effect_queue) {
        if (message.extended) {
            Player().HsMessage(message.port, message.bytes);
        } else {
            Player().ShortMessage(message.port, message.message);
        }
    }
    g_effect_queue.clear();
}

PC_OVERRIDE void CSound::Stop(int port) {
    audio::Trace("Stop port %d", port);
    Player().Stop(port);
}

PC_OVERRIDE void CSound::SetVol(int port, int volume) {
    if (Player().Volume(port) != volume) {
        audio::Trace("SetVol port %d volume %d", port, volume);
    }
    Player().SetVolume(port, volume);
}

// Assumed from the option menu, whose first (default, zero) choice sends 1: 1 is stereo.
PC_OVERRIDE void CSound::SetStereoMode(int mode) {
    audio::Trace("SetStereoMode %d", mode);
    Player().SetStereo(mode != 0);
}

PC_OVERRIDE int CSound::LoadHdBd_A(int hd, int hd_size, int bd, int bd_size) {
    return LoadSlotA(FromInt(hd), hd_size, FromInt(bd), bd_size);
}

PC_OVERRIDE int CSound::LoadHdBd_C(int hd, int hd_size, int bd, int bd_size) {
    return LoadSlotC(FromInt(hd), hd_size, FromInt(bd), bd_size);
}

PC_OVERRIDE int CSound::LoadHdBd_E(int hd, int hd_size, int bd, int bd_size) {
    return LoadSlotE(FromInt(hd), hd_size, FromInt(bd), bd_size);
}

PC_OVERRIDE int CSound::LoadHdBd_G(int hd, int hd_size, int bd, int bd_size) {
    return LoadSlotG(FromInt(hd), hd_size, FromInt(bd), bd_size);
}

PC_OVERRIDE int CSound::LoadHdBd_I(int hd, int hd_size, int bd, int bd_size) {
    return LoadSlotI(FromInt(hd), hd_size, FromInt(bd), bd_size);
}

PC_OVERRIDE int CSound::LoadHdBd_M(int hd, int hd_size, int bd, int bd_size) {
    return LoadSlotSimple(5, MIDI_PORT_UNK_D, 0x3080, FromInt(hd), hd_size, FromInt(bd), bd_size);
}

PC_OVERRIDE int CSound::LoadHdBd_Q(int hd, int hd_size, int bd, int bd_size) {
    return LoadSlotSimple(6, MIDI_PORT_SE_SPECIAL, 0x3090, FromInt(hd), hd_size, FromInt(bd), bd_size);
}

PC_OVERRIDE int CSound::LoadHdBd_S(int hd, int hd_size, int bd, int bd_size) {
    return LoadSlotSimple(7, MIDI_PORT_UNK_B, 0x3090, FromInt(hd), hd_size, FromInt(bd), bd_size);
}

PC_OVERRIDE int CSound::LoadSeq_A(int address, int size) {
    return LoadSequence(0, FromInt(address), size);
}

PC_OVERRIDE int CSound::LoadSeq_E(int address, int size) {
    return LoadSequence(2, FromInt(address), size);
}

PC_OVERRIDE int CSound::LoadSeq_I(int address, int size) {
    return LoadSequence(4, FromInt(address), size);
}
