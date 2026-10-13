#pragma helper_mask_gpr 0x30
#pragma helper_mask_fpr 0x1000
#pragma name_counter 348

#include "common.h"

#include <libpkt.h>
#include <libvu0.h>

#include <cmath>
#include <cstdio>
#include <cstring>

#include "boxvu0.hpp"
#include "camera.hpp"
#include "dataread.hpp"
#include "dsgeorama.hpp"
#include "editatra.hpp"
#include "mglib.hpp"
#include "rect.hpp"
#include "scriptinterpreter.hpp"
#include "snd.hpp"
#include "sound.hpp"
#include "texture.hpp"

/* The sound manager: BGM loading, playback and fading, and the SE table.
 * CSound itself is in ps2/src/sound.cpp. */

/**
 * Reads one sound configuration file through the script interpreter.
 *
 * @mangled LoadSoundInfo__FP8SND_INFOPci
 * @address 0x15BAB0
 * @size 0xF4
 */
static void LoadSoundInfo(SND_INFO *info, char *script, int script_size);

/**
 * Sets the reverberation the sound configuration asks for.
 *
 * @mangled CommandREVERBE__FPPv
 * @address 0x15BBB0
 * @size 0x28
 */
static void CommandREVERBE(void **arguments);

/**
 * Names the sound-effect table the configuration draws from.
 *
 * @mangled CommandTABLE__FPPv
 * @address 0x15BBE0
 * @size 0x28
 */
static void CommandTABLE(void **arguments);

/** The two basic sound-effect sets, one of which is loaded at a time. */
static SND_SE_INFO *basic_se_info[2] = {geo, dun};

/** The handler LoadSoundInfo calls for each of Command__3's tags. */
static void (*CommandExe__3[2])(void **arguments) = {CommandREVERBE, CommandTABLE};

/** Whether the sprites that follow draw with the bilinear filter. */
static int linear__2 = 1;

/** The buffer the sound loader is pointed at. */
unsigned int *snd_read_buf;

/** Nonzero keeps the background music from starting. */
int bgm_off;

/** The reverberation core's revision, read back after SetReverb. */
int snd_id;

/** The sound-effect set that is loaded, or -1 for none. */
int now_sound_set;

/** The voice set that is loaded, or -1 for none. */
int now_voice_set;

/** The background-music set that is loaded, or -1 for none. */
int now_bgm_no;

/** The background music's play state: 0 while it is stopped, 1 while it plays. */
int now_bgm_play;

/** The background-music volume that is set now. */
int now_bgm_vol;

/** The background-music fade that runs: 1 while it fades in, -1 while it fades out, 0 for none. */
int bgm_fade;

/** The background-music fade's current volume. */
float now_bgm_fade_vol;

/** How far the background-music fade moves each frame. */
float bgm_fade_step;

/** The volume the background-music fade ends at. */
int bgm_fade_vol;

/** The ambient loop that is playing, or -1 for none. */
int now_amb_no;

/** The ambient loop's volume that is set now. */
int now_amb_vol;

/** Whether the ambient loop plays: zero while it is stopped, one while it plays. */
int now_amb_play;

/** The menu sound effect that is playing, or -1 for none. */
int now_sp_no;

/** The chapter sound-effect set that is loaded, or -1 for none. */
int se_table_no;

/** The basic sound-effect set that is loaded, or -1 for none. */
int basic_se_table_no;

/** The background-music set that is loading in the background, or -1 for none. */
int load_bgm_no;

/** The buffer the loading background-music set reads into. */
unsigned int *load_bgm_adr;

/** The sound-effect set that is loading in the background, or -1 for none. */
int load_snd_set;

/** The buffer the loading sound-effect set reads into. */
unsigned int *load_snd_adr;

/** The voice set that is loading in the background, or -1 for none. */
int load_voice_set;

/** The buffer the loading voice set reads into. */
unsigned int *load_voice_adr;

/** The special-effect set that is loading in the background, or -1 for none. */
int load_sp_no;

/** The buffer the loading special-effect set reads into. */
unsigned int *load_sp_adr;

/** Whether the sound manager has been started once already. */
int init_snd;

/** The background-music set's configuration file name. */
char bgm_cfg_file[32];

/** The sound-effect set's configuration file name. */
char snd_cfg_file[32];

/** The voice set's configuration file name. */
char voice_cfg_file[32];

/** The special-effect set's configuration file name. */
char sp_cfg_file[32];

/** Where the camera the sound pans against stands. */
sceVu0FVECTOR camera_pos;

/** Which way the camera the sound pans against looks. */
sceVu0FVECTOR camera_dir;

/** The sound-effect sequence slots. */
SND_SE_SEQ se_seq[32];

/**
 * Advances every sound-effect sequence that is running.
 *
 * @mangled SndSeSeqStep__Fv
 * @address 0x15AFA0
 * @size 0xB8
 */
static void SndSeSeqStep();

EDIT_ELEMENT_ATRA *GetEditAtraData(int ground, int number) {
#ifdef MOD_DEMON_SHAFT_GEORAMA
    ground = DsgAssetIndex(ground);
#endif
    if (ground < 0 || ground >= 6) {
        return 0;
    }

    if (number < 0 || number >= 100) {
        return 0;
    }

    return &EditElementData[ground][number];
}

EDIT_PARTS_ATRA *GetEditAtraPartsData(int ground, int number) {
#ifdef MOD_DEMON_SHAFT_GEORAMA
    ground = DsgAssetIndex(ground);
#endif
    if (ground < 0 || ground >= 6) {
        return 0;
    }

    if (number < 0 || number >= 24) {
        return 0;
    }

    return &EditPartsData[ground].parts[number];
}

EDIT_ELEMENT_ATRA *GetEditAtraChipData(int ground, int number) {
    return GetEditAtraData(ground, number + 40);
}

void LensFlare(CTexture *texture, float *position, unsigned char red, unsigned char green, unsigned char blue) {
    sceVu0IVECTOR screen;

    if (texture == 0) {
        return;
    }

    int   visible = MGRotTransPers2D(screen, position, 0);
    int   screen_x = screen[0];
    int   screen_y = screen[1];
    float flare_offset[8] = {0.1f, 0.2f, 0.4f, 0.5f, 0.8f, 0.9f, 1.0f, 1.3f};
    float flare_size[8] = {0.1f, 0.2f, 1.0f, 0.3f, 2.0f, 0.5f, 3.8f, 0.5f};

    int size;
    int x;
    int y;
    int i;

    for (i = 0; i < 8; i++) {
        float offset = flare_offset[i];
        size = (int) (64.0f * flare_size[i]);
        float dx = (float) (320 - screen_x);
        dx *= offset;
        x = (int) dx;
        x += screen_x;
        x -= size >> 1;
        float dy = (float) (SCREEN_HALF_HEIGHT - screen_y);
        dy *= offset;
        y = (int) dy;
        y += screen_y;
        y -= size >> 1;

        if (visible && 0 <= screen_x && screen_x < 640 && 0 <= screen_y && screen_y < 448) {
            setbilinear(1);
            set2DSprite(Vif1Packet, texture, CRect_i_(x, y, size, size), CRect_i_(0, 0, 0x40, 0x40));
        }
    }

    if (visible && 0 <= screen_x && screen_x < 640 && 0 <= screen_y && screen_y < 448) {
        int center_x = 0;
        int center_y = 0;

        if (!(screen_x < 320)) {
            center_x = 320 - (screen_x - 320);
        }

        if (screen_x < 320) {
            center_x = screen_x;
        }

        if (!(screen_y < SCREEN_HALF_HEIGHT)) {
            center_y = SCREEN_HALF_HEIGHT - (screen_y - SCREEN_HALF_HEIGHT);
        }

        if (screen_y < SCREEN_HALF_HEIGHT) {
            center_y = screen_y;
        }

        float level = (float) ((center_x + center_y) >> 1) / 2.7f;
        int   alpha = (int) level;
        MGFillBox(CRect_i_(0, 0, 0x2800, (SCREEN_HALF_HEIGHT << 4)), red, green, blue, (unsigned char) (int) level);
    }
}

/**
 * Starts the sound manager once, and loads its effect table.
 *
 * @mangled SndInit__Fv
 * @address 0x1591A0
 * @size 0x60
 */
void SndInit() {
    if (init_snd == 0) {
        CSnd.Init(0, 0, 0, 0);
        init_snd = 1;
        SndInitialize(4, 0x1E, 4, 5);
    }
}

void SndInitialize(int unused0, int unused1, int unused2, int unused3) {
    snd_read_buf = read_buffer;
    SndBgmInit();
    SndAmbientInit();
    SndSeSeqInit();
    now_sound_set = -1;
    now_voice_set = -1;
    snd_id = 0;
    now_amb_no = -1;
    now_amb_vol = -1;
    now_sp_no = -1;
    se_table_no = -1;
    basic_se_table_no = -1;
    load_bgm_no = -1;
    load_bgm_adr = 0;
    bgm_cfg_file[0] = 0;
    load_snd_set = -1;
    load_snd_adr = 0;
    snd_cfg_file[0] = 0;
    load_voice_set = -1;
    load_voice_adr = 0;
    voice_cfg_file[0] = 0;
}

void SndExit() {
    CSnd.Stop(MIDI_PORT_BGM);
    CSnd.Stop(MIDI_PORT_SE_TITLE);
    CSnd.Stop(MIDI_PORT_AMBIENT);
    CSnd.Stop(MIDI_PORT_SE_DEFAULT);
    CSnd.Stop(MIDI_PORT_UNK_A);
    CSnd.Stop(MIDI_PORT_UNK_D);
    CSnd.Stop(MIDI_PORT_SE_SPECIAL);
    CSnd.StopVoice(0);
    CSnd.StopVoice(1);
    SndSeSeqInit();
    SndBgmInit();
    SndAmbientInit();
}

void SndStep() {
    SndBgmFadeInOut();
    SndSeSeqStep();
    CSnd.Step();
}

void SndInitSeTable() {
    int i;
    int j;

    for (i = 0; i < 2800; i++) {
        SND_SE_INFO *info = &se_info[i];

        if (info->bank >= 0) {
            info->vol_no = CSnd.GetSeNo(info->bank, info->prog);
        }
    }

    for (j = 0; j < 100; j++) {
        SND_SE_INFO *table = cap_se_info[j];

        if (table == 0) {
            continue;
        }

        for (i = 0; i < 100; i++) {
            SND_SE_INFO *info = &table[i];

            if (info->bank == -128) {
                break;
            }

            if (info->bank >= 0) {
                info->vol_no = CSnd.GetSeNo(info->bank, info->prog);
            }
        }
    }

    for (j = 0; j < 2; j++) {
        SND_SE_INFO *table = basic_se_info[j];

        if (table == 0) {
            continue;
        }

        for (i = 0; i < 200; i++) {
            SND_SE_INFO *info = &table[i];

            if (info->bank == -128) {
                break;
            }

            if (info->bank >= 0) {
                info->vol_no = CSnd.GetSeNo(info->bank, info->prog);
            }
        }
    }

    for (j = 0; j < 10; j++) {
        SND_SE_INFO *table = voice_info[j];

        if (table == 0) {
            continue;
        }

        for (i = 0; i < 100; i++) {
            SND_SE_INFO *info = &table[i];

            if (info->bank == -128) {
                break;
            }

            if (info->bank >= 0) {
                info->vol_no = CSnd.GetSeNo(info->bank, info->prog);
            }
        }
    }

    for (i = 0; i < 64; i++) {
        SND_SE_INFO *info = &special_se_info[i];

        if (info->bank == -128) {
            break;
        }

        if (info->bank >= 0) {
            info->vol_no = CSnd.GetSeNo(info->bank, info->prog);
        }
    }
}

void SndSetReadBuffer(unsigned int *buffer) {
    int misalign = (int) buffer % 64;

    if (misalign != 0) {
        buffer = (unsigned int *) ((int) buffer + (64 - misalign));
    }

    snd_read_buf = buffer;
}

/**
 * Reports whether any of the background sound loads is still running.
 *
 * @mangled SndSyncBG__Fv
 * @address 0x159670
 * @size 0x80
 */
int SndSyncBG() {
    if (SndBgmSyncBG()) {
        return 1;
    }

    if (SndSoundSyncBG()) {
        return 1;
    }

    if (SndSPSeSyncBG()) {
        return 1;
    }

    return SndVoiceSyncBG() ? 1 : 0;
}

void SndSetCamera(CCamera *camera) {
    camera->GetPos(camera_pos);
    camera->GetDir(camera_dir);
}

/**
 * Tells the sound where the camera stands and which way it looks.
 *
 * @mangled SndSetCamera__FPfPf
 * @address 0x159740
 * @size 0x50
 */
void SndSetCamera(float *position, float *rotation) {
    sceVu0CopyVector(camera_pos, position);
    sceVu0CopyVector(camera_dir, rotation);
}

/**
 * Builds the archive and configuration file names of one music set.
 *
 * @mangled GetBGMFile__FiPcPc
 * @address 0x159790
 * @size 0x7C
 */
static void GetBGMFile(int set_no, char *archive_name, char *config_name) {
    char name[16];

    sprintf(name, "bgm%d", set_no);
    sprintf(archive_name, "sound/bgm/%s.snd", name);
    sprintf(config_name, "%s.txt", name);
}

/**
 * Hands a loaded music set to the driver and reads its configuration.
 *
 * @mangled SetBGMFile__FiPUiPc
 * @address 0x159810
 * @size 0x114
 */
static void SetBGMFile(int set_no, unsigned int *buffer, char *config_name) {
    char          base_name[64];
    char         *dst;
    char          c;
    unsigned int *cfg_script;
    int           size;
    SND_INFO      info;

    SndBgmStop();
    CSnd.LoadSoundFileFromPack(config_name, buffer);
    now_bgm_no = set_no;
    now_bgm_play = 0;

    dst = base_name;

    while ((c = *config_name) != 0) {
        if (c == '.') {
            break;
        }

        *dst = c;
        config_name++;
        dst++;
    }

    *dst = 0;

    strcat(base_name, ".cfg");
    cfg_script = GetPackFile(buffer, base_name, &size);

    if (cfg_script != 0) {
        LoadSoundInfo(&info, (char *) cfg_script, size);
        CSnd.SetReverb(0, info.reverb_mode, info.reverb_depth);
        printf("core 0 rev = %d %d\n", info.reverb_mode, info.reverb_depth);
    }
}

int SndBgmInit() {
    now_bgm_no = -1;
    now_bgm_play = 0;
    now_bgm_vol = 0;
    bgm_fade = 0;
    bgm_fade_vol = 0;
    bgm_fade_step = 0.0f;
    now_bgm_fade_vol = 0.0f;
    return 1;
}

#ifdef PAL
void SndBgmDisable(int disable) {
    bgm_off = disable;

    if (disable != 0) {
        SndBgmStop();
    }
}

int SndGetBgmDisableFlag() {
    return bgm_off;
}
#endif

int SndBgmLoad(int set_no) {
    char archive_name[128];
    char config_name[16];

    if (now_bgm_no == set_no) {
        return 0;
    }

    GetBGMFile(set_no, archive_name, config_name);

    if (LoadFile2(archive_name, snd_read_buf, 0, 0)) {
        SetBGMFile(set_no, snd_read_buf, config_name);
        return 1;
    }

    return 0;
}

/**
 * Starts loading one music set in the background.
 *
 * @mangled SndBgmLoadBG__FiPUiPi
 * @address 0x1599F0
 * @size 0xC0
 */
int SndBgmLoadBG(int set_no, u_int *buffer, int *size) {
    char archive_name[128];

    if (size != 0) {
        *size = 0;
    }

    if (now_bgm_no == set_no) {
        return 0;
    }

    GetBGMFile(set_no, archive_name, bgm_cfg_file);
    printf("%d\n", set_no);

    if (LoadFileBG(archive_name, (u_long128 *) buffer, size)) {
        load_bgm_no = set_no;
        load_bgm_adr = buffer;
        return 1;
    }

    return 0;
}

int SndBgmSyncBG() {
    if (load_bgm_no < 0 || load_bgm_adr == 0) {
        return 0;
    }

    if (ReadBGSync()) {
        return 1;
    }

    SetBGMFile(load_bgm_no, load_bgm_adr, bgm_cfg_file);
    load_bgm_no = -1;
    load_bgm_adr = 0;
    return 0;
}

void SndBgmPlay(int track_no) {
    if (bgm_off == 0 && now_bgm_no >= 0 && now_bgm_play != 1) {
        CSnd.SQ_Play(MIDI_PORT_BGM, track_no);
        now_bgm_vol = SndGetDefaultBgmVol();
        now_bgm_play = 1;
    }
}

void SndBgmStop() {
    if (now_bgm_no >= 0 && now_bgm_play != 0) {
        CSnd.Stop(MIDI_PORT_BGM);
        CSnd.StopVoice(0);
        now_bgm_vol = 0;
        now_bgm_play = 0;
    }
}

#ifdef PAL
void SndBgmPause() {
    if (now_bgm_no >= 0 && now_bgm_play != 0) {
        CSnd.Stop(MIDI_PORT_BGM);
        now_bgm_play = 2;
    }
}
#endif

void SndBgmRePlay() {
    if (now_bgm_no >= 0 && now_bgm_play == 2) {
        CSnd.SQ_RePlay(MIDI_PORT_BGM);
        now_bgm_play = 1;
    }
}

void SndBgmFadeOutStop() {
    float volume = SndGetBgmVol();
    float step = volume / 10.0f;
    int   i;

    for (i = 0; i < 10; i++) {
        sceGsSyncV(0);
        volume -= step;
        SndSetBgmVol((int) volume);
        SndStep();
    }

    SndBgmStop();
}

int SndBgmCheck() {
    return now_bgm_play;
}

int SndGetBgmNo() {
    return now_bgm_no;
}

/**
 * Sets the background music's volume.
 *
 * @mangled SndSetBgmVol__Fi
 * @address 0x159D20
 * @size 0x70
 */
void SndSetBgmVol(int volume) {
    if (now_bgm_no >= 0 && now_bgm_vol != volume) {
        if (volume < 0 || volume > 127) {
            return;
        }

        if (now_bgm_play != 0) {
            now_bgm_vol = volume;
            CSnd.SetVol(MIDI_PORT_BGM, volume);
        }
    }
}

/**
 * Sets the background music's volume as a share of its default.
 *
 * @mangled SndSetBgmVolf__Ff
 * @address 0x159D90
 * @size 0x50
 */
void SndSetBgmVolf(float volume) {
    SndSetBgmVol((int) (volume * (float) SndGetDefaultBgmVol()));
}

int SndGetBgmVol() {
    return now_bgm_vol;
}

int SndGetDefaultBgmVol() {
    if (now_bgm_no < 0) {
        return 0;
    }

    return CSnd.GetMidiState()->port[0].sequence[0]->volume;
}

/**
 * Fades the background music up to a volume over a number of steps.
 *
 * @mangled SndBgmFadeIn__Fiii
 * @address 0x159E40
 * @size 0xC8
 */
void SndBgmFadeIn(int frames, int volume, int start_volume) {
    if (frames > 0) {
        if (volume < 0) {
            volume = SndGetDefaultBgmVol();
        }

        bgm_fade_vol = volume;

        if (start_volume < 0) {
            start_volume = SndGetBgmVol();
        }

        if (start_volume != SndGetDefaultBgmVol()) {
            now_bgm_fade_vol = (float) start_volume / (float) SndGetDefaultBgmVol();
            bgm_fade = 1;
            bgm_fade_step = ((float) bgm_fade_vol - now_bgm_fade_vol) / (float) frames;
        }
    }
}

/**
 * Fades the background music down to a volume over a number of steps.
 *
 * @mangled SndBgmFadeOut__Fii
 * @address 0x159F10
 * @size 0x74
 */
void SndBgmFadeOut(int frames, int volume) {
    if (frames > 0) {
        bgm_fade_vol = volume;
        now_bgm_fade_vol = (float) SndGetBgmVol();
        bgm_fade = -1;
        bgm_fade_step = ((float) bgm_fade_vol - now_bgm_fade_vol) / (float) frames;
    }
}

void SndBgmFadeInOut() {
    float step;

    if (bgm_fade != 0) {
        now_bgm_fade_vol += bgm_fade_step;
        step = bgm_fade_step;

        if ((step < 0.0f ? -step : step) < 0.0001f) {
            bgm_fade = 0;
        }

        if (bgm_fade > 0) {
            if (now_bgm_fade_vol >= (float) bgm_fade_vol) {
                now_bgm_fade_vol = (float) bgm_fade_vol;
                bgm_fade = 0;
            }
        } else if (now_bgm_fade_vol <= (float) bgm_fade_vol) {
            now_bgm_fade_vol = (float) bgm_fade_vol;
            bgm_fade = 0;
        }

        SndSetBgmVol((int) now_bgm_fade_vol);
    }
}

int SndCheckFade() {
    return bgm_fade == 0;
}

/**
 * Returns the table row for a sound effect, or zero when the number names no
 * row. The number selects between the fixed table, the current basic set, the
 * current chapter set and the current voice set.
 *
 * @mangled GetSeInfo__Fi
 * @address 0x15A0B0
 * @size 0x148
 */
static SND_SE_INFO *GetSeInfo(int se_no) {
    SND_SE_INFO *table;

    if (se_no < 0 || se_no >= 2800) {
        return 0;
    }

    if (basic_se_table_no >= 0 && se_no >= 100 && se_no < 300) {
        return basic_se_info[basic_se_table_no] + (se_no - 100);
    }

    if (se_no >= 300 && se_no < 400 && se_table_no >= 0) {
        table = cap_se_info[se_table_no];

        if (table != 0) {
            return table + (se_no - 300);
        }
    }

    if (se_no >= 400 && se_no < 500 && now_voice_set >= 0) {
        table = voice_info[now_voice_set];

        if (table != 0) {
            return table + (se_no - 400);
        }
    }

    return &se_info[se_no];
}

/**
 * Returns the port a sound effect plays on, or the default port when its row
 * does not name one.
 *
 * @mangled GetPortNo__Fi
 * @address 0x15A200
 * @size 0x38
 */
static int GetPortNo(int se_no) {
    SND_SE_INFO *info = GetSeInfo(se_no);

    if (info->port >= 0) {
        return info->port;
    }

    return MIDI_PORT_SE_DEFAULT;
}

/**
 * Builds the archive and configuration file names of one sound-effect set.
 *
 * @mangled GetSoundFile__FiPcPc
 * @address 0x15A240
 * @size 0x7C
 */
static void GetSoundFile(int set_no, char *archive_name, char *config_name) {
    char name[16];

    sprintf(name, "snd%d", set_no);
    sprintf(archive_name, "sound/set/%s.snd", name);
    sprintf(config_name, "%s.txt", name);
}

/**
 * Hands a loaded sound-effect set to the driver and sets its channel volumes.
 *
 * @mangled SetSoundFile__FiPUiPc
 * @address 0x15A2C0
 * @size 0x174
 */
static void SetSoundFile(int set_no, unsigned int *buffer, char *config_name) {
    char          base_name[64];
    char         *dst;
    char          c;
    SND_INFO      info;
    unsigned int *cfg_script;
    int           size;

    CSnd.LoadSoundFileFromPack(config_name, buffer);
    CSnd.SetVol(MIDI_PORT_SE_TITLE, 0x100);
    CSnd.SetVol(MIDI_PORT_SE_DEFAULT, 0x100);
    CSnd.SetVol(MIDI_PORT_UNK_A, 0x100);
    CSnd.SetVol(MIDI_PORT_UNK_D, 0x100);
    CSnd.SetVol(MIDI_PORT_SE_SPECIAL, 0x100);
    now_sound_set = set_no;
    snd_id = 0;
    now_amb_no = -1;
    SndStopAllSe();

    dst = base_name;

    while ((c = *config_name) != 0) {
        if (c == '.') {
            break;
        }

        *dst = c;
        config_name++;
        dst++;
    }

    *dst = 0;

    strcat(base_name, ".cfg");
    cfg_script = GetPackFile(buffer, base_name, &size);

    if (cfg_script != 0) {
        LoadSoundInfo(&info, (char *) cfg_script, size);
        CSnd.SetReverb(1, info.reverb_mode, info.reverb_depth);
        basic_se_table_no = info.se_table;
        se_table_no = info.se_table_type;
    }
}

int SndGetNowSetNo() {
    return now_sound_set;
}

void SndStopAllSe() {
    CSnd.Stop(MIDI_PORT_SE_TITLE);
    CSnd.Stop(MIDI_PORT_AMBIENT);
    CSnd.Stop(MIDI_PORT_SE_DEFAULT);
    CSnd.Stop(MIDI_PORT_UNK_A);
    CSnd.Stop(MIDI_PORT_UNK_D);
    CSnd.Stop(MIDI_PORT_SE_SPECIAL);
    CSnd.Stop(MIDI_PORT_UNK_B);
    SndAmbientInit();
    CSnd.StopVoice(1);
}

int SndSoundLoad(int set_no) {
    char archive_name[128];
    char config_name[32];

    if (now_sound_set == set_no) {
        return 0;
    }

    GetSoundFile(set_no, archive_name, config_name);

    if (LoadFile2(archive_name, snd_read_buf, 0, 0)) {
        SetSoundFile(set_no, snd_read_buf, config_name);
        return 1;
    }

    return 0;
}

/**
 * Starts loading one sound-effect set in the background.
 *
 * @mangled SndSoundLoadBG__FiPUiPi
 * @address 0x15A580
 * @size 0xAC
 */
int SndSoundLoadBG(int set_no, u_int *buffer, int *size) {
    char archive_name[128];

    if (size != 0) {
        *size = 0;
    }

    if (now_sound_set == set_no) {
        return 0;
    }

    GetSoundFile(set_no, archive_name, snd_cfg_file);

    if (LoadFileBG(archive_name, (u_long128 *) buffer, size)) {
        load_snd_set = set_no;
        load_snd_adr = buffer;
        return 1;
    }

    return 0;
}

int SndSoundSyncBG() {
    if (load_snd_set < 0 || load_snd_adr == 0) {
        return 0;
    }

    if (ReadBGSync()) {
        return 1;
    }

    SetSoundFile(load_snd_set, load_snd_adr, snd_cfg_file);
    load_snd_set = -1;
    load_snd_adr = 0;
    return 0;
}

void SndSePlay(int se_no, int vol, int voice) {
    SND_SE_INFO *info = GetSeInfo(se_no);

    if (info != 0) {
        if (info->vol_no < 0) {
            vol = 127;
        }

        int        port = GetPortNo(se_no);
        static int system_snd_id = (int) 0.0f;

        if (vol < 0) {
            CSnd.SE_Play(port, info->vol_no, voice);
        } else {
            CSnd.SE_Play(port, info->bank, info->prog, vol, voice);
        }
    }
}

/**
 * Plays a sound effect at an explicit volume and pan.
 *
 * @mangled SndSePlay__Fiffi
 * @address 0x15A760
 * @size 0xBC
 */
void SndSePlay(int se_no, float volume, float pan, int voice) {
    SND_SE_INFO *info = GetSeInfo(se_no);

    if (info != 0) {
        int vol = SndGetVolf(se_no, volume);
        int hw_pan = SndGetPanf(pan);

        if (info->vol_no < 0) {
            vol = 127;
        }

        CSnd.SE_Play(GetPortNo(se_no), info->bank, info->prog, hw_pan, 127, vol, voice);
    }
}

void SndSePlay(int se_no, float *position, float near, float far) {
    float volume;
    float pan;

    if (near < 0.0f) {
        near = 20.0f;
    }

    if (far < 0.0f) {
        far = 500.0f;
    }

    SndGetVolPan(&volume, &pan, position, near, far);
    SndSePlay(se_no, volume, pan, 0);
}

/**
 * Stops a sounding effect.
 *
 * @mangled SndSeStop__Fii
 * @address 0x15A8B0
 * @size 0x50
 */
void SndSeStop(int se_no, int voice) {
    SND_SE_INFO *info = GetSeInfo(se_no);

    if (info != 0) {
        CSnd.SE_Stop(GetPortNo(se_no), info->bank, info->prog, voice);
    }
}

void SndSetSeVol(int se_no, int vol, int voice) {
    SND_SE_INFO *info;

    if (vol < 0 || vol > 127) {
        return;
    }

    info = GetSeInfo(se_no);

    if (info != 0) {
        CSnd.SE_SetVol(GetPortNo(se_no), info->bank, info->prog, vol, voice);
    }
}

int SndGetVolf(int se_no, float vol) {
    SND_SE_INFO *info = GetSeInfo(se_no);
    short       *table;
    int          level;

    if (info == 0) {
        return 0;
    }

    table = CSnd.GetSeInfTbl();
    level = 64;

    if (info->vol_no >= 0) {
        level = table[info->vol_no * 2 + 1];
    }

    level = (int) ((float) level * vol);

    if (level < 0) {
        level = 0;
    }

    if (level > 127) {
        level = 127;
    }

    return level;
}

int SndGetPanf(float pan) {
    if (pan < -1.0f) {
        pan = -1.0f;
    }

    if (pan > 1.0f) {
        pan = 1.0f;
    }

    return (int) (63.0f * pan) + 64;
}

void SndSetSeVolf(int se_no, float vol, int voice) {
    if (GetSeInfo(se_no) != 0) {
        SndSetSeVol(se_no, SndGetVolf(se_no, vol), voice);
    }
}

void SndSetSePanf(int se_no, float pan, int voice) {
    SND_SE_INFO *info = GetSeInfo(se_no);

    if (info != 0) {
        int hw_pan = SndGetPanf(pan);

        CSnd.SE_SetPan(GetPortNo(se_no), info->bank, info->prog, hw_pan, voice);
    }
}

void SndPlayFootSound(int kind, int foot, float *position) {
    float volume;
    float pan;
    int   se_no = kind * 4 + 500 + (foot > 0);

    float near = 50.0f;
    float far = 300.0f;
    SndGetVolPan(&volume, &pan, position, near, far);
    SndSePlay(se_no, volume, pan, 0);
}

/**
 * Calculates the volume and stereo balance of a sound at a world position.
 *
 * @mangled SndGetVolPan__FPfPfPfff
 * @address 0x15AC00
 * @size 0x1F8
 */
void SndGetVolPan(float *vol, float *pan, float *pos, float near, float far) {
    sceVu0FVECTOR to_source;
    sceVu0FVECTOR right;
    float         distance = DistVector(pos, camera_pos);
    float         level = 1.0f - (distance - near) / (far - near);
    float         side;
    float         weight;
    int           sign;

    if (distance > far) {
        level = 0.0f;
    }

    if (distance < near) {
        level = 1.0f;
    }

    *vol = level;
    *pan = 0.0f;

    sceVu0CopyVector(to_source, camera_dir);
    to_source[1] = 0.0f;
    sceVu0Normalize(to_source, to_source);
    right[0] = to_source[2];
    right[1] = 0.0f;
    right[2] = -to_source[0];

    sceVu0SubVector(to_source, pos, camera_pos);
    to_source[1] = 0.0f;
    sceVu0Normalize(to_source, to_source);
    side = -sceVu0InnerProduct(to_source, right);

    sign = 1;

    if (side < 0.0f) {
        sign = -1;
    }

    weight = side < 0.0f ? -side : side;
    weight *= weight;
    weight *= weight;
    *pan = 0.7f * ((float) sign * weight);
    level = *vol * (0.7f + 0.3f * (*pan < 0.0f ? -*pan : *pan));
    *vol = level;
    *vol *= 1.4f;
}

/**
 * Frees one sound-effect sequence slot.
 *
 * @mangled InitSeSeq__FP10SND_SE_SEQ
 * @address 0x15AE00
 * @size 0x10
 */
static void InitSeSeq(SND_SE_SEQ *seq) {
    seq->se_no = -1;
}

/**
 * Finds the sequence slot already playing a sound on a voice, and reports
 * through found that it did. Returns the first free slot instead when there is
 * no such slot, or zero when every slot is taken.
 *
 * @mangled GetSeSeq__FPiii
 * @address 0x15AE10
 * @size 0xC0
 */
static SND_SE_SEQ *GetSeSeq(int *found, int se_no, int voice) {
    int         i;
    SND_SE_SEQ *slot = 0;

    *found = 0;

    for (i = 0; i < 32; i++) {
        if (se_seq[i].se_no < 0) {
            slot = &se_seq[i];
            break;
        }
    }

    if (se_no >= 0) {
        for (i = 0; i < 32; i++) {
            if (se_seq[i].se_no == se_no && se_seq[i].voice == voice) {
                *found = 1;
                return &se_seq[i];
            }
        }
    }

    return slot;
}

void SndSeSeqInit() {
    int i;

    for (i = 0; i < 32; i++) {
        InitSeSeq(&se_seq[i]);
    }
}

int SndSeSeqPlayStop(int se_no, int length, int voice) {
    int         found;
    SND_SE_SEQ *slot = GetSeSeq(&found, se_no, voice);

    if (slot == 0) {
        return 0;
    }

    slot->se_no = se_no;
    slot->length = length;

    if (found) {
        slot->step = 1;
    } else {
        slot->step = 0;
    }

    slot->voice = voice;
    return 1;
}

static void SndSeSeqStep() {
    int i;

    for (i = 0; i < 32; i++) {
        SND_SE_SEQ *seq = &se_seq[i];

        if (seq->se_no >= 0) {
            if (seq->step == 0) {
                SndSePlay(seq->se_no, -1, seq->voice);
            }

            if (seq->step >= seq->length) {
                SndSeStop(seq->se_no, seq->voice);
                InitSeSeq(seq);
            }

            seq->step++;
        }
    }
}

/**
 * Stops every sound-effect sequence.
 *
 * @mangled SndSeSeqAllStop__Fv
 * @address 0x15B060
 * @size 0x84
 */
void SndSeSeqAllStop() {
    SND_SE_SEQ *seq;
    int         i;

    for (i = 0; i < 32; i++) {
        seq = &se_seq[i];

        if (seq->se_no >= 0) {
            SndSeStop(seq->se_no, seq->voice);
            InitSeSeq(seq);
            seq->step++;
        }
    }
}

int SndAmbientInit() {
    now_amb_no = -1;
    now_amb_play = 0;
    now_amb_vol = 0;
    return 1;
}

void SndAmbientPlay(int ambient_no) {
    if (now_amb_no != ambient_no || now_amb_play != 1) {
        SndAmbientStop();
        CSnd.SQ_Play(MIDI_PORT_AMBIENT, ambient_no);
        now_amb_no = ambient_no;
        now_amb_vol = SndGetAmbientDefaultVol();
        now_amb_play = 1;
    }
}

void SndAmbientStop() {
    if (now_amb_no >= 0 && now_amb_play != 0) {
        CSnd.Stop(MIDI_PORT_AMBIENT);
        now_amb_play = 0;
    }
}

/**
 * Sets the ambient loop's volume.
 *
 * @mangled SndAmbientSetVol__Fi
 * @address 0x15B1E0
 * @size 0x60
 */
void SndAmbientSetVol(int volume) {
    if (now_amb_no >= 0 && now_amb_vol != volume && volume >= 0) {
        if (volume > 127) {
            volume = 127;
        }

        now_amb_vol = volume;
        CSnd.SetVol(MIDI_PORT_AMBIENT, volume);
    }
}

/**
 * Sets the ambient loop's volume as a share of its default.
 *
 * @mangled SndAmbientSetVolf__Ff
 * @address 0x15B240
 * @size 0x5C
 */
void SndAmbientSetVolf(float volume) {
    if (now_amb_no >= 0) {
        int level = SndGetAmbientDefaultVol();

        SndAmbientSetVol((int) ((float) level * volume));
    }
}

int SndGetAmbientDefaultVol() {
    if (CSnd.GetMidiState()->port[2].sequence[now_amb_no] != 0) {
        return CSnd.GetMidiState()->port[2].sequence[now_amb_no]->volume;
    }

    return 64;
}

/**
 * Builds the archive and configuration file names of one voice set.
 *
 * @mangled GetVoiceFile__FiPcPc
 * @address 0x15B310
 * @size 0x7C
 */
static void GetVoiceFile(int set_no, char *archive_name, char *config_name) {
    char name[16];

    sprintf(name, "voice%d", set_no);
    sprintf(archive_name, "sound/voice/%s.snd", name);
    sprintf(config_name, "%s.txt", name);
}

/**
 * Hands a loaded voice set to the driver and reads its configuration.
 *
 * @mangled SetVoiceFile__FiPUiPc
 * @address 0x15B390
 * @size 0x54
 */
static void SetVoiceFile(int set_no, u_int *buffer, char *config_name) {
    CSnd.LoadSoundFileFromPack(config_name, buffer);
    CSnd.SetVol(MIDI_PORT_UNK_B, 0x100);
    now_voice_set = set_no;
}

int SndVoiceLoad(int set_no) {
    char archive_name[128];
    char config_name[32];

    if (now_voice_set == set_no) {
        return 0;
    }

    GetVoiceFile(set_no, archive_name, config_name);

    if (LoadFile2(archive_name, snd_read_buf, 0, 0)) {
        SetVoiceFile(set_no, snd_read_buf, config_name);
        return 1;
    }

    return 0;
}

/**
 * Starts loading one voice set in the background.
 *
 * @mangled SndVoiceLoadBG__FiPUiPi
 * @address 0x15B480
 * @size 0xAC
 */
int SndVoiceLoadBG(int set_no, u_int *buffer, int *size) {
    char archive_name[128];

    if (size != 0) {
        *size = 0;
    }

    if (now_voice_set == set_no) {
        return 0;
    }

    GetVoiceFile(set_no, archive_name, voice_cfg_file);

    if (LoadFileBG(archive_name, (u_long128 *) buffer, size)) {
        load_voice_set = set_no;
        load_voice_adr = buffer;
        return 1;
    }

    return 0;
}

int SndVoiceSyncBG() {
    if (load_voice_set < 0 || load_voice_adr == 0) {
        return 0;
    }

    if (ReadBGSync()) {
        return 1;
    }

    SetVoiceFile(load_voice_set, load_voice_adr, voice_cfg_file);
    load_voice_set = -1;
    load_voice_adr = 0;
    return 0;
}

/**
 * Returns the table row for a menu sound effect, or zero when the number names
 * no row.
 *
 * @mangled GetSPInfo__Fi
 * @address 0x15B5B0
 * @size 0x40
 */
static SND_SE_INFO *GetSPInfo(int se_no) {
    if (se_no < 0 || se_no >= 64) {
        return 0;
    }

    return &special_se_info[se_no];
}

/**
 * Builds the archive and configuration file names of one special-effect set.
 *
 * @mangled GetSPSeFile__FiPcPc
 * @address 0x15B5F0
 * @size 0x7C
 */
static void GetSPSeFile(int set_no, char *archive_name, char *config_name) {
    char name[16];

    sprintf(name, "sp%d", set_no);
    sprintf(archive_name, "sound/special/%s.snd", name);
    sprintf(config_name, "%s.txt", name);
}

/**
 * Hands a loaded special-effect set to the driver and reads its configuration.
 *
 * @mangled SetSPSeFile__FiPUiPc
 * @address 0x15B670
 * @size 0x54
 */
static void SetSPSeFile(int set_no, u_int *buffer, char *config_name) {
    CSnd.LoadSoundFileFromPack(config_name, buffer);
    CSnd.SetVol(MIDI_PORT_SE_SPECIAL, 0x100);
    now_sp_no = set_no;
}

int SndSPSeLoad(int set_no) {
    char archive_name[128];
    char config_name[32];

    if (now_sp_no == set_no) {
        return 0;
    }

    GetSPSeFile(set_no, archive_name, config_name);

    if (LoadFile2(archive_name, snd_read_buf, 0, 0)) {
        SetSPSeFile(set_no, snd_read_buf, config_name);
        return 1;
    }

    return 0;
}

int SndSPSeLoadBG(int set_no, u_int *buffer, int *size) {
    char archive_name[128];

    if (size != 0) {
        *size = 0;
    }

    GetSPSeFile(set_no, archive_name, sp_cfg_file);

    if (LoadFileBG(archive_name, (u_long128 *) buffer, size)) {
        load_sp_no = set_no;
        load_sp_adr = buffer;
        return 1;
    }

    return 0;
}

/**
 * Polls the special-effect set load and hands the file to the driver once it lands.
 *
 * @mangled SndSPSeSyncBG__Fv
 * @address 0x15B800
 * @size 0x80
 */
int SndSPSeSyncBG() {
    if (load_sp_no < 0 || load_sp_adr == 0) {
        return 0;
    }

    if (ReadBGSync()) {
        return 1;
    }

    SetSPSeFile(load_sp_no, load_sp_adr, sp_cfg_file);
    load_sp_no = -1;
    load_sp_adr = 0;
    return 0;
}

/**
 * Plays one special sound effect.
 *
 * @mangled SndSPSePlay__Fii
 * @address 0x15B880
 * @size 0x7C
 */
void SndSPSePlay(int se_no, int vol) {
    SND_SE_INFO *info = GetSPInfo(se_no);

    if (info != 0) {
        if (info->vol_no < 0) {
            vol = 64;
        }

        if (vol < 0) {
            CSnd.SE_Play(MIDI_PORT_SE_SPECIAL, info->vol_no, 0);
        } else {
            CSnd.SE_Play(MIDI_PORT_SE_SPECIAL, info->bank, info->prog, vol, 0);
        }
    }
}

void SndSPSeStop(int se_no) {
    SND_SE_INFO *info = GetSPInfo(se_no);

    if (info != 0) {
        CSnd.SE_Stop(MIDI_PORT_SE_SPECIAL, info->bank, info->prog, 0);
    }
}

/**
 * Sets a special sound effect's volume as a share of its table value.
 *
 * @mangled SndSetSPSeVolf__Fif
 * @address 0x15B950
 * @size 0xB0
 */
void SndSetSPSeVolf(int se_no, float volume) {
    SND_SE_INFO *info = GetSPInfo(se_no);

    if (info != 0) {
        short *table = CSnd.GetSeInfTbl();
        int    level = table[info->vol_no * 2 + 1];

        level = (int) ((float) level * volume);

        if (level < 0) {
            level = 0;
        }

        if (level > 127) {
            level = 127;
        }

        CSnd.SE_SetVol(MIDI_PORT_SE_SPECIAL, info->bank, info->prog, level, 0);
    }
}

/**
 * Sets a special sound effect's pan as a share of the widest pan.
 *
 * @mangled SndSetSPSePanf__Fif
 * @address 0x15BA00
 * @size 0xA8
 */
void SndSetSPSePanf(int se_no, float pan) {
    if (pan < -1.0f) {
        pan = -1.0f;
    }

    if (pan > 1.0f) {
        pan = 1.0f;
    }

    SND_SE_INFO *info = GetSPInfo(se_no);

    if (info != 0) {
        CSnd.SE_SetPan(MIDI_PORT_SE_SPECIAL, info->vol_no, (int) (63.0f * pan) + 64, 0);
    }
}

// clang-format off
SND_SE_INFO se_info[2801] = {
    {122, 24, 0, 13, 0}, {122, 25, 0, 13, 0}, {122, 26, 0, 13, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {122, 29, 0, 13, 0}, {122, 30, 0, 13, 0}, {-1, -1, 0, -1, 0},
    {122, 32, 0, 13, 0}, {122, 33, 0, 13, 0}, {122, 34, 0, 13, 0}, {-1, -1, 0, -1, 0},
    {122, 36, 0, 13, 0}, {122, 37, 0, 13, 0}, {122, 38, 0, 13, 0}, {122, 39, 0, 13, 0},
    {-1, -1, 0, -1, 0}, {122, 41, 0, 13, 0}, {-1, -1, 0, -1, 0}, {122, 43, 0, 13, 0},
    {-1, -1, 0, -1, 0}, {122, 45, 0, 13, 0}, {122, 46, 0, 13, 0}, {122, 47, 0, 13, 0},
    {122, 48, 0, 13, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {16, 20, 1, 15, 0}, {16, 21, 1, 15, 0},
    {16, 22, 1, 15, 0}, {16, 23, 1, 15, 0}, {16, 24, 1, 15, 0}, {16, 25, 1, 15, 0},
    {16, 26, 1, 15, 0}, {16, 27, 1, 15, 0}, {16, 28, 1, 15, 0}, {16, 29, 1, 15, 0},
    {16, 30, 1, 15, 0}, {16, 31, 1, 15, 0}, {16, 32, 1, 15, 0}, {16, 33, 1, 15, 0},
    {16, 34, 1, 15, 0}, {16, 35, 1, 15, 0}, {16, 36, 1, 15, 0}, {16, 37, 1, 15, 0},
    {16, 38, 1, 15, 0}, {16, 39, 1, 15, 0}, {16, 40, 1, 15, 0}, {16, 41, 1, 15, 0},
    {16, 42, 1, 15, 0}, {16, 43, 1, 15, 0}, {16, 44, 1, 15, 0}, {16, 45, 1, 15, 0},
    {16, 46, 1, 15, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {21, 20, 0, 14, 0}, {21, 21, 0, 14, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {21, 24, 0, 14, 0}, {21, 25, 0, 14, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {21, 28, 0, 14, 0}, {21, 29, 0, 14, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {21, 32, 0, 14, 0}, {21, 33, 0, 14, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {21, 36, 0, 14, 0}, {21, 37, 0, 14, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {21, 40, 0, 14, 0}, {21, 41, 0, 14, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {21, 44, 0, 14, 0}, {21, 45, 0, 14, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {21, 48, 0, 14, 0}, {21, 49, 0, 14, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {21, 52, 0, 14, 0}, {21, 53, 0, 14, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {21, 56, 0, 14, 0}, {21, 57, 0, 14, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {21, 60, 0, 14, 0}, {21, 61, 0, 14, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {21, 64, 0, 14, 0}, {21, 65, 0, 14, 0}, {-1, -1, 0, -1, 0}, {21, 67, 0, 14, 0},
    {21, 68, 0, 14, 0}, {21, 69, 0, 14, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {21, 72, 0, 14, 0}, {21, 73, 0, 14, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {21, 76, 0, 14, 0}, {21, 77, 0, 14, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {21, 80, 0, 14, 0}, {21, 81, 0, 14, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {53, 20, 0, 10, 0}, {53, 21, 0, 10, 0}, {53, 22, 0, 10, 0},
    {53, 23, 0, 10, 0}, {53, 24, 0, 10, 0}, {53, 25, 0, 10, 0}, {53, 26, 0, 10, 0},
    {53, 27, 0, 10, 0}, {53, 28, 0, 10, 0}, {53, 29, 0, 10, 0}, {53, 30, 0, 10, 0},
    {53, 31, 0, 10, 0}, {53, 32, 0, 10, 0}, {53, 33, 0, 10, 0}, {53, 34, 0, 10, 0},
    {53, 35, 0, 10, 0}, {53, 36, 0, 10, 0}, {53, 37, 0, 10, 0}, {53, 38, 0, 10, 0},
    {54, 20, 0, 10, 0}, {54, 21, 0, 10, 0}, {54, 22, 0, 10, 0}, {54, 23, 0, 10, 0},
    {54, 24, 0, 10, 0}, {54, 25, 0, 10, 0}, {54, 26, 0, 10, 0}, {54, 27, 0, 10, 0},
    {54, 28, 0, 10, 0}, {54, 29, 0, 10, 0}, {54, 30, 0, 10, 0}, {54, 31, 0, 10, 0},
    {54, 32, 0, 10, 0}, {54, 33, 0, 10, 0}, {54, 34, 0, 10, 0}, {54, 35, 0, 10, 0},
    {54, 36, 0, 10, 0}, {54, 37, 0, 10, 0}, {54, 38, 0, 10, 0}, {55, 20, 0, 10, 0},
    {55, 21, 0, 10, 0}, {55, 22, 0, 10, 0}, {55, 23, 0, 10, 0}, {55, 24, 0, 10, 0},
    {55, 25, 0, 10, 0}, {55, 26, 0, 10, 0}, {55, 27, 0, 10, 0}, {55, 28, 0, 10, 0},
    {55, 29, 0, 10, 0}, {55, 30, 0, 10, 0}, {55, 31, 0, 10, 0}, {55, 32, 0, 10, 0},
    {55, 33, 0, 10, 0}, {55, 34, 0, 10, 0}, {55, 35, 0, 10, 0}, {55, 36, 0, 10, 0},
    {55, 37, 0, 10, 0}, {55, 38, 0, 10, 0}, {56, 20, 0, 10, 0}, {56, 21, 0, 10, 0},
    {56, 22, 0, 10, 0}, {56, 23, 0, 10, 0}, {56, 24, 0, 10, 0}, {56, 25, 0, 10, 0},
    {56, 26, 0, 10, 0}, {56, 27, 0, 10, 0}, {56, 28, 0, 10, 0}, {56, 29, 0, 10, 0},
    {56, 30, 0, 10, 0}, {56, 31, 0, 10, 0}, {56, 32, 0, 10, 0}, {56, 33, 0, 10, 0},
    {56, 34, 0, 10, 0}, {56, 35, 0, 10, 0}, {56, 36, 0, 10, 0}, {56, 37, 0, 10, 0},
    {56, 38, 0, 10, 0}, {57, 20, 0, 10, 0}, {57, 21, 0, 10, 0}, {57, 22, 0, 10, 0},
    {57, 23, 0, 10, 0}, {57, 24, 0, 10, 0}, {57, 25, 0, 10, 0}, {57, 26, 0, 10, 0},
    {57, 27, 0, 10, 0}, {57, 28, 0, 10, 0}, {57, 29, 0, 10, 0}, {57, 30, 0, 10, 0},
    {57, 31, 0, 10, 0}, {57, 32, 0, 10, 0}, {57, 33, 0, 10, 0}, {57, 34, 0, 10, 0},
    {57, 35, 0, 10, 0}, {57, 36, 0, 10, 0}, {57, 37, 0, 10, 0}, {57, 38, 0, 10, 0},
    {58, 20, 0, 10, 0}, {58, 21, 0, 10, 0}, {58, 22, 0, 10, 0}, {58, 23, 0, 10, 0},
    {58, 24, 0, 10, 0}, {58, 25, 0, 10, 0}, {58, 26, 0, 10, 0}, {58, 27, 0, 10, 0},
    {58, 28, 0, 10, 0}, {58, 29, 0, 10, 0}, {58, 30, 0, 10, 0}, {58, 31, 0, 10, 0},
    {58, 32, 0, 10, 0}, {58, 33, 0, 10, 0}, {58, 34, 0, 10, 0}, {58, 35, 0, 10, 0},
    {58, 36, 0, 10, 0}, {58, 37, 0, 10, 0}, {58, 38, 0, 10, 0}, {59, 20, 0, 10, 0},
    {59, 21, 0, 10, 0}, {59, 22, 0, 10, 0}, {59, 23, 0, 10, 0}, {59, 24, 0, 10, 0},
    {59, 25, 0, 10, 0}, {59, 26, 0, 10, 0}, {59, 27, 0, 10, 0}, {59, 28, 0, 10, 0},
    {59, 29, 0, 10, 0}, {59, 30, 0, 10, 0}, {59, 31, 0, 10, 0}, {59, 32, 0, 10, 0},
    {59, 33, 0, 10, 0}, {59, 34, 0, 10, 0}, {59, 35, 0, 10, 0}, {59, 36, 0, 10, 0},
    {59, 37, 0, 10, 0}, {59, 38, 0, 10, 0}, {60, 20, 0, 10, 0}, {60, 21, 0, 10, 0},
    {60, 22, 0, 10, 0}, {60, 23, 0, 10, 0}, {60, 24, 0, 10, 0}, {60, 25, 0, 10, 0},
    {60, 26, 0, 10, 0}, {60, 27, 0, 10, 0}, {60, 28, 0, 10, 0}, {60, 29, 0, 10, 0},
    {60, 30, 0, 10, 0}, {60, 31, 0, 10, 0}, {60, 32, 0, 10, 0}, {60, 33, 0, 10, 0},
    {60, 34, 0, 10, 0}, {60, 35, 0, 10, 0}, {60, 36, 0, 10, 0}, {60, 37, 0, 10, 0},
    {60, 38, 0, 10, 0}, {61, 20, 1, 10, 0}, {61, 21, 0, 10, 0}, {61, 22, 0, 10, 0},
    {61, 23, 0, 10, 0}, {61, 24, 0, 10, 0}, {61, 25, 0, 10, 0}, {61, 26, 0, 10, 0},
    {61, 27, 0, 10, 0}, {61, 28, 0, 10, 0}, {61, 29, 0, 10, 0}, {61, 30, 0, 10, 0},
    {61, 31, 0, 10, 0}, {61, 32, 0, 10, 0}, {61, 33, 0, 10, 0}, {61, 34, 0, 10, 0},
    {61, 35, 0, 10, 0}, {61, 36, 0, 10, 0}, {61, 37, 0, 10, 0}, {61, 38, 0, 10, 0},
    {62, 20, 0, 10, 0}, {62, 21, 0, 10, 0}, {62, 22, 0, 10, 0}, {62, 23, 0, 10, 0},
    {62, 24, 0, 10, 0}, {62, 25, 0, 10, 0}, {62, 26, 0, 10, 0}, {62, 27, 0, 10, 0},
    {62, 28, 0, 10, 0}, {62, 29, 0, 10, 0}, {62, 30, 0, 10, 0}, {62, 31, 0, 10, 0},
    {62, 32, 0, 10, 0}, {62, 33, 0, 10, 0}, {62, 34, 0, 10, 0}, {62, 35, 0, 10, 0},
    {62, 36, 0, 10, 0}, {62, 37, 0, 10, 0}, {62, 38, 0, 10, 0}, {63, 20, 0, 10, 0},
    {63, 21, 0, 10, 0}, {63, 22, 0, 10, 0}, {63, 23, 0, 10, 0}, {63, 24, 0, 10, 0},
    {63, 25, 0, 10, 0}, {63, 26, 0, 10, 0}, {63, 27, 0, 10, 0}, {63, 28, 0, 10, 0},
    {63, 29, 0, 10, 0}, {63, 30, 0, 10, 0}, {63, 31, 0, 10, 0}, {63, 32, 0, 10, 0},
    {63, 33, 0, 10, 0}, {63, 34, 0, 10, 0}, {63, 35, 0, 10, 0}, {63, 36, 0, 10, 0},
    {63, 37, 0, 10, 0}, {63, 38, 0, 10, 0}, {64, 20, 1, 10, 0}, {64, 21, 0, 10, 0},
    {64, 22, 0, 10, 0}, {64, 23, 0, 10, 0}, {64, 24, 0, 10, 0}, {64, 25, 0, 10, 0},
    {64, 26, 0, 10, 0}, {64, 27, 0, 10, 0}, {64, 28, 0, 10, 0}, {64, 29, 0, 10, 0},
    {64, 30, 0, 10, 0}, {64, 31, 0, 10, 0}, {64, 32, 0, 10, 0}, {64, 33, 0, 10, 0},
    {64, 34, 0, 10, 0}, {64, 35, 0, 10, 0}, {64, 36, 0, 10, 0}, {64, 37, 0, 10, 0},
    {64, 38, 0, 10, 0}, {65, 20, 0, 10, 0}, {65, 21, 0, 10, 0}, {65, 22, 0, 10, 0},
    {65, 23, 0, 10, 0}, {65, 24, 0, 10, 0}, {65, 25, 0, 10, 0}, {65, 26, 0, 10, 0},
    {65, 27, 0, 10, 0}, {65, 28, 0, 10, 0}, {65, 29, 0, 10, 0}, {65, 30, 0, 10, 0},
    {65, 31, 0, 10, 0}, {65, 32, 0, 10, 0}, {65, 33, 0, 10, 0}, {65, 34, 0, 10, 0},
    {65, 35, 0, 10, 0}, {65, 36, 0, 10, 0}, {65, 37, 0, 10, 0}, {65, 38, 0, 10, 0},
    {66, 20, 0, 10, 0}, {66, 21, 0, 10, 0}, {66, 22, 0, 10, 0}, {66, 23, 0, 10, 0},
    {66, 24, 0, 10, 0}, {66, 25, 0, 10, 0}, {66, 26, 0, 10, 0}, {66, 27, 0, 10, 0},
    {66, 28, 0, 10, 0}, {66, 29, 0, 10, 0}, {66, 30, 0, 10, 0}, {66, 31, 0, 10, 0},
    {66, 32, 0, 10, 0}, {66, 33, 0, 10, 0}, {66, 34, 0, 10, 0}, {66, 35, 0, 10, 0},
    {66, 36, 0, 10, 0}, {66, 37, 0, 10, 0}, {66, 38, 0, 10, 0}, {67, 20, 0, 10, 0},
    {67, 21, 0, 10, 0}, {67, 22, 0, 10, 0}, {67, 23, 0, 10, 0}, {67, 24, 0, 10, 0},
    {67, 25, 0, 10, 0}, {67, 26, 0, 10, 0}, {67, 27, 0, 10, 0}, {67, 28, 0, 10, 0},
    {67, 29, 0, 10, 0}, {67, 30, 0, 10, 0}, {67, 31, 0, 10, 0}, {67, 32, 0, 10, 0},
    {67, 33, 0, 10, 0}, {67, 34, 0, 10, 0}, {67, 35, 0, 10, 0}, {67, 36, 0, 10, 0},
    {67, 37, 0, 10, 0}, {67, 38, 0, 10, 0}, {68, 20, 0, 10, 0}, {68, 21, 0, 10, 0},
    {68, 22, 0, 10, 0}, {68, 23, 0, 10, 0}, {68, 24, 0, 10, 0}, {68, 25, 0, 10, 0},
    {68, 26, 0, 10, 0}, {68, 27, 0, 10, 0}, {68, 28, 0, 10, 0}, {68, 29, 0, 10, 0},
    {68, 30, 0, 10, 0}, {68, 31, 0, 10, 0}, {68, 32, 0, 10, 0}, {68, 33, 0, 10, 0},
    {68, 34, 0, 10, 0}, {68, 35, 0, 10, 0}, {68, 36, 0, 10, 0}, {68, 37, 0, 10, 0},
    {68, 38, 0, 10, 0}, {69, 20, 0, 10, 0}, {69, 21, 0, 10, 0}, {69, 22, 0, 10, 0},
    {69, 23, 0, 10, 0}, {69, 24, 0, 10, 0}, {69, 25, 0, 10, 0}, {69, 26, 0, 10, 0},
    {69, 27, 0, 10, 0}, {69, 28, 0, 10, 0}, {69, 29, 0, 10, 0}, {69, 30, 0, 10, 0},
    {69, 31, 0, 10, 0}, {69, 32, 0, 10, 0}, {69, 33, 0, 10, 0}, {69, 34, 0, 10, 0},
    {69, 35, 0, 10, 0}, {69, 36, 0, 10, 0}, {69, 37, 0, 10, 0}, {69, 38, 0, 10, 0},
    {70, 20, 0, 10, 0}, {70, 21, 0, 10, 0}, {70, 22, 0, 10, 0}, {70, 23, 0, 10, 0},
    {70, 24, 0, 10, 0}, {70, 25, 0, 10, 0}, {70, 26, 0, 10, 0}, {70, 27, 0, 10, 0},
    {70, 28, 0, 10, 0}, {70, 29, 0, 10, 0}, {70, 30, 0, 10, 0}, {70, 31, 0, 10, 0},
    {70, 32, 0, 10, 0}, {70, 33, 0, 10, 0}, {70, 34, 0, 10, 0}, {70, 35, 0, 10, 0},
    {70, 36, 0, 10, 0}, {70, 37, 0, 10, 0}, {70, 38, 0, 10, 0}, {71, 20, 0, 10, 0},
    {71, 21, 0, 10, 0}, {71, 22, 0, 10, 0}, {71, 23, 0, 10, 0}, {71, 24, 0, 10, 0},
    {71, 25, 0, 10, 0}, {71, 26, 0, 10, 0}, {71, 27, 0, 10, 0}, {71, 28, 0, 10, 0},
    {71, 29, 0, 10, 0}, {71, 30, 0, 10, 0}, {71, 31, 0, 10, 0}, {71, 32, 0, 10, 0},
    {71, 33, 0, 10, 0}, {71, 34, 0, 10, 0}, {71, 35, 0, 10, 0}, {71, 36, 0, 10, 0},
    {71, 37, 0, 10, 0}, {71, 38, 0, 10, 0}, {72, 20, 0, 10, 0}, {72, 21, 0, 10, 0},
    {72, 22, 0, 10, 0}, {72, 23, 0, 10, 0}, {72, 24, 0, 10, 0}, {72, 25, 0, 10, 0},
    {72, 26, 0, 10, 0}, {72, 27, 0, 10, 0}, {72, 28, 0, 10, 0}, {72, 29, 0, 10, 0},
    {72, 30, 0, 10, 0}, {72, 31, 0, 10, 0}, {72, 32, 0, 10, 0}, {72, 33, 0, 10, 0},
    {72, 34, 0, 10, 0}, {72, 35, 0, 10, 0}, {72, 36, 0, 10, 0}, {72, 37, 0, 10, 0},
    {72, 38, 0, 10, 0}, {73, 20, 0, 10, 0}, {73, 21, 0, 10, 0}, {73, 22, 0, 10, 0},
    {73, 23, 0, 10, 0}, {73, 24, 0, 10, 0}, {73, 25, 0, 10, 0}, {73, 26, 0, 10, 0},
    {73, 27, 0, 10, 0}, {73, 28, 0, 10, 0}, {73, 29, 0, 10, 0}, {73, 30, 0, 10, 0},
    {73, 31, 0, 10, 0}, {73, 32, 0, 10, 0}, {73, 33, 0, 10, 0}, {73, 34, 0, 10, 0},
    {73, 35, 0, 10, 0}, {73, 36, 0, 10, 0}, {73, 37, 0, 10, 0}, {73, 38, 0, 10, 0},
    {74, 20, 0, 10, 0}, {74, 21, 0, 10, 0}, {74, 22, 0, 10, 0}, {74, 23, 0, 10, 0},
    {74, 24, 0, 10, 0}, {74, 25, 0, 10, 0}, {74, 26, 0, 10, 0}, {74, 27, 0, 10, 0},
    {74, 28, 0, 10, 0}, {74, 29, 0, 10, 0}, {74, 30, 0, 10, 0}, {74, 31, 0, 10, 0},
    {74, 32, 0, 10, 0}, {74, 33, 0, 10, 0}, {74, 34, 0, 10, 0}, {74, 35, 0, 10, 0},
    {74, 36, 0, 10, 0}, {74, 37, 0, 10, 0}, {74, 38, 0, 10, 0}, {75, 20, 0, 10, 0},
    {75, 21, 0, 10, 0}, {75, 22, 0, 10, 0}, {75, 23, 0, 10, 0}, {75, 24, 0, 10, 0},
    {75, 25, 0, 10, 0}, {75, 26, 0, 10, 0}, {75, 27, 0, 10, 0}, {75, 28, 0, 10, 0},
    {75, 29, 0, 10, 0}, {75, 30, 0, 10, 0}, {75, 31, 0, 10, 0}, {75, 32, 0, 10, 0},
    {75, 33, 0, 10, 0}, {75, 34, 0, 10, 0}, {75, 35, 0, 10, 0}, {75, 36, 0, 10, 0},
    {75, 37, 0, 10, 0}, {75, 38, 0, 10, 0}, {76, 20, 0, 10, 0}, {76, 21, 0, 10, 0},
    {76, 22, 0, 10, 0}, {76, 23, 0, 10, 0}, {76, 24, 0, 10, 0}, {76, 25, 0, 10, 0},
    {76, 26, 0, 10, 0}, {76, 27, 0, 10, 0}, {76, 28, 0, 10, 0}, {76, 29, 0, 10, 0},
    {76, 30, 0, 10, 0}, {76, 31, 0, 10, 0}, {76, 32, 0, 10, 0}, {76, 33, 0, 10, 0},
    {76, 34, 0, 10, 0}, {76, 35, 0, 10, 0}, {76, 36, 0, 10, 0}, {76, 37, 0, 10, 0},
    {76, 38, 0, 10, 0}, {77, 20, 1, 10, 0}, {77, 21, 0, 10, 0}, {77, 22, 0, 10, 0},
    {77, 23, 0, 10, 0}, {77, 24, 0, 10, 0}, {77, 25, 0, 10, 0}, {77, 26, 0, 10, 0},
    {77, 27, 0, 10, 0}, {77, 28, 0, 10, 0}, {77, 29, 0, 10, 0}, {77, 30, 0, 10, 0},
    {77, 31, 0, 10, 0}, {77, 32, 0, 10, 0}, {77, 33, 0, 10, 0}, {77, 34, 0, 10, 0},
    {77, 35, 0, 10, 0}, {77, 36, 0, 10, 0}, {77, 37, 0, 10, 0}, {77, 38, 0, 10, 0},
    {78, 20, 0, 10, 0}, {78, 21, 0, 10, 0}, {78, 22, 0, 10, 0}, {78, 23, 0, 10, 0},
    {78, 24, 0, 10, 0}, {78, 25, 0, 10, 0}, {78, 26, 0, 10, 0}, {78, 27, 0, 10, 0},
    {78, 28, 0, 10, 0}, {78, 29, 0, 10, 0}, {78, 30, 0, 10, 0}, {78, 31, 0, 10, 0},
    {78, 32, 0, 10, 0}, {78, 33, 0, 10, 0}, {78, 34, 0, 10, 0}, {78, 35, 0, 10, 0},
    {78, 36, 0, 10, 0}, {78, 37, 0, 10, 0}, {78, 38, 0, 10, 0}, {79, 20, 0, 10, 0},
    {79, 21, 0, 10, 0}, {79, 22, 0, 10, 0}, {79, 23, 0, 10, 0}, {79, 24, 0, 10, 0},
    {79, 25, 0, 10, 0}, {79, 26, 0, 10, 0}, {79, 27, 0, 10, 0}, {79, 28, 0, 10, 0},
    {79, 29, 0, 10, 0}, {79, 30, 0, 10, 0}, {79, 31, 0, 10, 0}, {79, 32, 0, 10, 0},
    {79, 33, 0, 10, 0}, {79, 34, 0, 10, 0}, {79, 35, 0, 10, 0}, {79, 36, 0, 10, 0},
    {79, 37, 0, 10, 0}, {79, 38, 0, 10, 0}, {80, 20, 0, 10, 0}, {80, 21, 0, 10, 0},
    {80, 22, 0, 10, 0}, {80, 23, 0, 10, 0}, {80, 24, 0, 10, 0}, {80, 25, 0, 10, 0},
    {80, 26, 0, 10, 0}, {80, 27, 0, 10, 0}, {80, 28, 0, 10, 0}, {80, 29, 0, 10, 0},
    {80, 30, 0, 10, 0}, {80, 31, 0, 10, 0}, {80, 32, 0, 10, 0}, {80, 33, 0, 10, 0},
    {80, 34, 0, 10, 0}, {80, 35, 0, 10, 0}, {80, 36, 0, 10, 0}, {80, 37, 0, 10, 0},
    {80, 38, 0, 10, 0}, {81, 20, 1, 10, 0}, {81, 21, 0, 10, 0}, {81, 22, 0, 10, 0},
    {81, 23, 0, 10, 0}, {81, 24, 0, 10, 0}, {81, 25, 0, 10, 0}, {81, 26, 0, 10, 0},
    {81, 27, 0, 10, 0}, {81, 28, 0, 10, 0}, {81, 29, 0, 10, 0}, {81, 30, 0, 10, 0},
    {81, 31, 0, 10, 0}, {81, 32, 0, 10, 0}, {81, 33, 0, 10, 0}, {81, 34, 0, 10, 0},
    {81, 35, 0, 10, 0}, {81, 36, 0, 10, 0}, {81, 37, 0, 10, 0}, {81, 38, 0, 10, 0},
    {82, 20, 0, 10, 0}, {82, 21, 0, 10, 0}, {82, 22, 0, 10, 0}, {82, 23, 0, 10, 0},
    {82, 24, 0, 10, 0}, {82, 25, 0, 10, 0}, {82, 26, 0, 10, 0}, {82, 27, 0, 10, 0},
    {82, 28, 0, 10, 0}, {82, 29, 0, 10, 0}, {82, 30, 0, 10, 0}, {82, 31, 0, 10, 0},
    {82, 32, 0, 10, 0}, {82, 33, 0, 10, 0}, {82, 34, 0, 10, 0}, {82, 35, 0, 10, 0},
    {82, 36, 0, 10, 0}, {82, 37, 0, 10, 0}, {82, 38, 0, 10, 0}, {83, 20, 0, 10, 0},
    {83, 21, 0, 10, 0}, {83, 22, 0, 10, 0}, {83, 23, 0, 10, 0}, {83, 24, 0, 10, 0},
    {83, 25, 0, 10, 0}, {83, 26, 0, 10, 0}, {83, 27, 0, 10, 0}, {83, 28, 0, 10, 0},
    {83, 29, 0, 10, 0}, {83, 30, 0, 10, 0}, {83, 31, 0, 10, 0}, {83, 32, 0, 10, 0},
    {83, 33, 0, 10, 0}, {83, 34, 0, 10, 0}, {83, 35, 0, 10, 0}, {83, 36, 0, 10, 0},
    {83, 37, 0, 10, 0}, {83, 38, 0, 10, 0}, {84, 20, 1, 10, 0}, {84, 21, 0, 10, 0},
    {84, 22, 0, 10, 0}, {84, 23, 0, 10, 0}, {84, 24, 0, 10, 0}, {84, 25, 0, 10, 0},
    {84, 26, 0, 10, 0}, {84, 27, 0, 10, 0}, {84, 28, 0, 10, 0}, {84, 29, 0, 10, 0},
    {84, 30, 0, 10, 0}, {84, 31, 0, 10, 0}, {84, 32, 0, 10, 0}, {84, 33, 0, 10, 0},
    {84, 34, 0, 10, 0}, {84, 35, 0, 10, 0}, {84, 36, 0, 10, 0}, {84, 37, 0, 10, 0},
    {84, 38, 0, 10, 0}, {85, 20, 0, 10, 0}, {85, 21, 0, 10, 0}, {85, 22, 0, 10, 0},
    {85, 23, 0, 10, 0}, {85, 24, 0, 10, 0}, {85, 25, 0, 10, 0}, {85, 26, 0, 10, 0},
    {85, 27, 0, 10, 0}, {85, 28, 0, 10, 0}, {85, 29, 0, 10, 0}, {85, 30, 0, 10, 0},
    {85, 31, 0, 10, 0}, {85, 32, 0, 10, 0}, {85, 33, 0, 10, 0}, {85, 34, 0, 10, 0},
    {85, 35, 0, 10, 0}, {85, 36, 0, 10, 0}, {85, 37, 0, 10, 0}, {85, 38, 0, 10, 0},
    {86, 20, 0, 10, 0}, {86, 21, 0, 10, 0}, {86, 22, 0, 10, 0}, {86, 23, 0, 10, 0},
    {86, 24, 0, 10, 0}, {86, 25, 0, 10, 0}, {86, 26, 0, 10, 0}, {86, 27, 0, 10, 0},
    {86, 28, 0, 10, 0}, {86, 29, 0, 10, 0}, {86, 30, 0, 10, 0}, {86, 31, 0, 10, 0},
    {86, 32, 0, 10, 0}, {86, 33, 0, 10, 0}, {86, 34, 0, 10, 0}, {86, 35, 0, 10, 0},
    {86, 36, 0, 10, 0}, {86, 37, 0, 10, 0}, {86, 38, 0, 10, 0}, {87, 20, 0, 10, 0},
    {87, 21, 0, 10, 0}, {87, 22, 0, 10, 0}, {87, 23, 0, 10, 0}, {87, 24, 0, 10, 0},
    {87, 25, 0, 10, 0}, {87, 26, 0, 10, 0}, {87, 27, 0, 10, 0}, {87, 28, 0, 10, 0},
    {87, 29, 0, 10, 0}, {87, 30, 0, 10, 0}, {87, 31, 0, 10, 0}, {87, 32, 0, 10, 0},
    {87, 33, 0, 10, 0}, {87, 34, 0, 10, 0}, {87, 35, 0, 10, 0}, {87, 36, 0, 10, 0},
    {87, 37, 0, 10, 0}, {87, 38, 0, 10, 0}, {88, 20, 0, 10, 0}, {88, 21, 0, 10, 0},
    {88, 22, 0, 10, 0}, {88, 23, 0, 10, 0}, {88, 24, 0, 10, 0}, {88, 25, 0, 10, 0},
    {88, 26, 0, 10, 0}, {88, 27, 0, 10, 0}, {88, 28, 0, 10, 0}, {88, 29, 0, 10, 0},
    {88, 30, 0, 10, 0}, {88, 31, 0, 10, 0}, {88, 32, 0, 10, 0}, {88, 33, 0, 10, 0},
    {88, 34, 0, 10, 0}, {88, 35, 0, 10, 0}, {88, 36, 0, 10, 0}, {88, 37, 0, 10, 0},
    {88, 38, 0, 10, 0}, {89, 20, 0, 10, 0}, {89, 21, 0, 10, 0}, {89, 22, 0, 10, 0},
    {89, 23, 0, 10, 0}, {89, 24, 0, 10, 0}, {89, 25, 0, 10, 0}, {89, 26, 0, 10, 0},
    {89, 27, 0, 10, 0}, {89, 28, 0, 10, 0}, {89, 29, 0, 10, 0}, {89, 30, 0, 10, 0},
    {89, 31, 0, 10, 0}, {89, 32, 0, 10, 0}, {89, 33, 0, 10, 0}, {89, 34, 0, 10, 0},
    {89, 35, 0, 10, 0}, {89, 36, 0, 10, 0}, {89, 37, 0, 10, 0}, {89, 38, 0, 10, 0},
    {90, 20, 0, 10, 0}, {90, 21, 0, 10, 0}, {90, 22, 0, 10, 0}, {90, 23, 0, 10, 0},
    {90, 24, 0, 10, 0}, {90, 25, 0, 10, 0}, {90, 26, 0, 10, 0}, {90, 27, 0, 10, 0},
    {90, 28, 0, 10, 0}, {90, 29, 0, 10, 0}, {90, 30, 0, 10, 0}, {90, 31, 0, 10, 0},
    {90, 32, 0, 10, 0}, {90, 33, 0, 10, 0}, {90, 34, 0, 10, 0}, {90, 35, 0, 10, 0},
    {90, 36, 0, 10, 0}, {90, 37, 0, 10, 0}, {90, 38, 0, 10, 0}, {91, 20, 0, 10, 0},
    {91, 21, 0, 10, 0}, {91, 22, 0, 10, 0}, {91, 23, 0, 10, 0}, {91, 24, 0, 10, 0},
    {91, 25, 0, 10, 0}, {91, 26, 0, 10, 0}, {91, 27, 0, 10, 0}, {91, 28, 0, 10, 0},
    {91, 29, 0, 10, 0}, {91, 30, 0, 10, 0}, {91, 31, 0, 10, 0}, {91, 32, 0, 10, 0},
    {91, 33, 0, 10, 0}, {91, 34, 0, 10, 0}, {91, 35, 0, 10, 0}, {91, 36, 0, 10, 0},
    {91, 37, 0, 10, 0}, {91, 38, 0, 10, 0}, {92, 20, 1, 10, 0}, {92, 21, 0, 10, 0},
    {92, 22, 0, 10, 0}, {92, 23, 0, 10, 0}, {92, 24, 0, 10, 0}, {92, 25, 0, 10, 0},
    {92, 26, 0, 10, 0}, {92, 27, 0, 10, 0}, {92, 28, 0, 10, 0}, {92, 29, 0, 10, 0},
    {92, 30, 0, 10, 0}, {92, 31, 0, 10, 0}, {92, 32, 0, 10, 0}, {92, 33, 0, 10, 0},
    {92, 34, 0, 10, 0}, {92, 35, 0, 10, 0}, {92, 36, 0, 10, 0}, {92, 37, 0, 10, 0},
    {92, 38, 0, 10, 0}, {93, 20, 0, 10, 0}, {93, 21, 0, 10, 0}, {93, 22, 0, 10, 0},
    {93, 23, 0, 10, 0}, {93, 24, 0, 10, 0}, {93, 25, 0, 10, 0}, {93, 26, 0, 10, 0},
    {93, 27, 0, 10, 0}, {93, 28, 0, 10, 0}, {93, 29, 0, 10, 0}, {93, 30, 0, 10, 0},
    {93, 31, 0, 10, 0}, {93, 32, 0, 10, 0}, {93, 33, 0, 10, 0}, {93, 34, 0, 10, 0},
    {93, 35, 0, 10, 0}, {93, 36, 0, 10, 0}, {93, 37, 0, 10, 0}, {93, 38, 0, 10, 0},
    {94, 20, 0, 10, 0}, {94, 21, 0, 10, 0}, {94, 22, 0, 10, 0}, {94, 23, 0, 10, 0},
    {94, 24, 0, 10, 0}, {94, 25, 0, 10, 0}, {94, 26, 0, 10, 0}, {94, 27, 0, 10, 0},
    {94, 28, 0, 10, 0}, {94, 29, 0, 10, 0}, {94, 30, 0, 10, 0}, {94, 31, 0, 10, 0},
    {94, 32, 0, 10, 0}, {94, 33, 0, 10, 0}, {94, 34, 0, 10, 0}, {94, 35, 0, 10, 0},
    {94, 36, 0, 10, 0}, {94, 37, 0, 10, 0}, {94, 38, 0, 10, 0}, {95, 20, 0, 10, 0},
    {95, 21, 0, 10, 0}, {95, 22, 0, 10, 0}, {95, 23, 0, 10, 0}, {95, 24, 0, 10, 0},
    {95, 25, 0, 10, 0}, {95, 26, 0, 10, 0}, {95, 27, 0, 10, 0}, {95, 28, 0, 10, 0},
    {95, 29, 0, 10, 0}, {95, 30, 0, 10, 0}, {95, 31, 0, 10, 0}, {95, 32, 0, 10, 0},
    {95, 33, 0, 10, 0}, {95, 34, 0, 10, 0}, {95, 35, 0, 10, 0}, {95, 36, 0, 10, 0},
    {95, 37, 0, 10, 0}, {95, 38, 0, 10, 0}, {96, 20, 0, 10, 0}, {96, 21, 0, 10, 0},
    {96, 22, 0, 10, 0}, {96, 23, 0, 10, 0}, {96, 24, 0, 10, 0}, {96, 25, 0, 10, 0},
    {96, 26, 0, 10, 0}, {96, 27, 0, 10, 0}, {96, 28, 0, 10, 0}, {96, 29, 0, 10, 0},
    {96, 30, 0, 10, 0}, {96, 31, 0, 10, 0}, {96, 32, 0, 10, 0}, {96, 33, 0, 10, 0},
    {96, 34, 0, 10, 0}, {96, 35, 0, 10, 0}, {96, 36, 0, 10, 0}, {96, 37, 0, 10, 0},
    {96, 38, 0, 10, 0}, {97, 20, 0, 10, 0}, {97, 21, 0, 10, 0}, {97, 22, 0, 10, 0},
    {97, 23, 0, 10, 0}, {97, 24, 0, 10, 0}, {97, 25, 0, 10, 0}, {97, 26, 0, 10, 0},
    {97, 27, 0, 10, 0}, {97, 28, 0, 10, 0}, {97, 29, 0, 10, 0}, {97, 30, 0, 10, 0},
    {97, 31, 0, 10, 0}, {97, 32, 0, 10, 0}, {97, 33, 0, 10, 0}, {97, 34, 0, 10, 0},
    {97, 35, 0, 10, 0}, {97, 36, 0, 10, 0}, {97, 37, 0, 10, 0}, {97, 38, 0, 10, 0},
    {98, 20, 0, 10, 0}, {98, 21, 0, 10, 0}, {98, 22, 0, 10, 0}, {98, 23, 0, 10, 0},
    {98, 24, 0, 10, 0}, {98, 25, 0, 10, 0}, {98, 26, 0, 10, 0}, {98, 27, 0, 10, 0},
    {98, 28, 0, 10, 0}, {98, 29, 0, 10, 0}, {98, 30, 0, 10, 0}, {98, 31, 0, 10, 0},
    {98, 32, 0, 10, 0}, {98, 33, 0, 10, 0}, {98, 34, 0, 10, 0}, {98, 35, 0, 10, 0},
    {98, 36, 0, 10, 0}, {98, 37, 0, 10, 0}, {98, 38, 0, 10, 0}, {99, 20, 0, 10, 0},
    {99, 21, 0, 10, 0}, {99, 22, 0, 10, 0}, {99, 23, 0, 10, 0}, {99, 24, 0, 10, 0},
    {99, 25, 0, 10, 0}, {99, 26, 0, 10, 0}, {99, 27, 0, 10, 0}, {99, 28, 0, 10, 0},
    {99, 29, 0, 10, 0}, {99, 30, 0, 10, 0}, {99, 31, 0, 10, 0}, {99, 32, 0, 10, 0},
    {99, 33, 0, 10, 0}, {99, 34, 0, 10, 0}, {99, 35, 0, 10, 0}, {99, 36, 0, 10, 0},
    {99, 37, 0, 10, 0}, {99, 38, 0, 10, 0}, {100, 20, 0, 10, 0}, {100, 21, 0, 10, 0},
    {100, 22, 0, 10, 0}, {100, 23, 0, 10, 0}, {100, 24, 0, 10, 0}, {100, 25, 0, 10, 0},
    {100, 26, 0, 10, 0}, {100, 27, 0, 10, 0}, {100, 28, 0, 10, 0}, {100, 29, 0, 10, 0},
    {100, 30, 0, 10, 0}, {100, 31, 0, 10, 0}, {100, 32, 0, 10, 0}, {100, 33, 0, 10, 0},
    {100, 34, 0, 10, 0}, {100, 35, 0, 10, 0}, {100, 36, 0, 10, 0}, {100, 37, 0, 10, 0},
    {100, 38, 0, 10, 0}, {101, 20, 0, 10, 0}, {101, 21, 0, 10, 0}, {101, 22, 0, 10, 0},
    {101, 23, 0, 10, 0}, {101, 24, 0, 10, 0}, {101, 25, 0, 10, 0}, {101, 26, 0, 10, 0},
    {101, 27, 0, 10, 0}, {101, 28, 0, 10, 0}, {101, 29, 0, 10, 0}, {101, 30, 0, 10, 0},
    {101, 31, 0, 10, 0}, {101, 32, 0, 10, 0}, {101, 33, 0, 10, 0}, {101, 34, 0, 10, 0},
    {101, 35, 0, 10, 0}, {101, 36, 0, 10, 0}, {101, 37, 0, 10, 0}, {101, 38, 0, 10, 0},
    {102, 20, 0, 10, 0}, {102, 21, 0, 10, 0}, {102, 22, 0, 10, 0}, {102, 23, 0, 10, 0},
    {102, 24, 0, 10, 0}, {102, 25, 0, 10, 0}, {102, 26, 0, 10, 0}, {102, 27, 0, 10, 0},
    {102, 28, 0, 10, 0}, {102, 29, 0, 10, 0}, {102, 30, 0, 10, 0}, {102, 31, 0, 10, 0},
    {102, 32, 0, 10, 0}, {102, 33, 0, 10, 0}, {102, 34, 0, 10, 0}, {102, 35, 0, 10, 0},
    {102, 36, 0, 10, 0}, {102, 37, 0, 10, 0}, {102, 38, 0, 10, 0}, {103, 20, 0, 10, 0},
    {103, 21, 0, 10, 0}, {103, 22, 0, 10, 0}, {103, 23, 0, 10, 0}, {103, 24, 0, 10, 0},
    {103, 25, 0, 10, 0}, {103, 26, 0, 10, 0}, {103, 27, 0, 10, 0}, {103, 28, 0, 10, 0},
    {103, 29, 0, 10, 0}, {103, 30, 0, 10, 0}, {103, 31, 0, 10, 0}, {103, 32, 0, 10, 0},
    {103, 33, 0, 10, 0}, {103, 34, 0, 10, 0}, {103, 35, 0, 10, 0}, {103, 36, 0, 10, 0},
    {103, 37, 0, 10, 0}, {103, 38, 0, 10, 0}, {104, 20, 0, 10, 0}, {104, 21, 0, 10, 0},
    {104, 22, 0, 10, 0}, {104, 23, 0, 10, 0}, {104, 24, 0, 10, 0}, {104, 25, 0, 10, 0},
    {104, 26, 0, 10, 0}, {104, 27, 0, 10, 0}, {104, 28, 0, 10, 0}, {104, 29, 0, 10, 0},
    {104, 30, 0, 10, 0}, {104, 31, 0, 10, 0}, {104, 32, 0, 10, 0}, {104, 33, 0, 10, 0},
    {104, 34, 0, 10, 0}, {104, 35, 0, 10, 0}, {104, 36, 0, 10, 0}, {104, 37, 0, 10, 0},
    {104, 38, 0, 10, 0}, {53, 50, 0, 10, 0}, {53, 51, 0, 10, 0}, {53, 52, 0, 10, 0},
    {53, 53, 0, 10, 0}, {53, 54, 0, 10, 0}, {53, 55, 0, 10, 0}, {53, 56, 0, 10, 0},
    {53, 57, 0, 10, 0}, {53, 58, 0, 10, 0}, {53, 59, 0, 10, 0}, {53, 60, 0, 10, 0},
    {53, 61, 0, 10, 0}, {53, 62, 0, 10, 0}, {53, 63, 0, 10, 0}, {53, 64, 0, 10, 0},
    {53, 65, 0, 10, 0}, {53, 66, 0, 10, 0}, {53, 67, 0, 10, 0}, {53, 68, 0, 10, 0},
    {53, 80, 0, 10, 0}, {53, 81, 0, 10, 0}, {53, 82, 0, 10, 0}, {53, 83, 0, 10, 0},
    {53, 84, 0, 10, 0}, {53, 85, 0, 10, 0}, {53, 86, 0, 10, 0}, {53, 87, 0, 10, 0},
    {53, 88, 0, 10, 0}, {53, 89, 0, 10, 0}, {53, 90, 0, 10, 0}, {53, 91, 0, 10, 0},
    {53, 92, 0, 10, 0}, {53, 93, 0, 10, 0}, {53, 94, 0, 10, 0}, {53, 95, 0, 10, 0},
    {53, 96, 0, 10, 0}, {53, 97, 0, 10, 0}, {53, 98, 0, 10, 0}, {107, 20, 0, 10, 0},
    {107, 21, 0, 10, 0}, {107, 22, 0, 10, 0}, {107, 23, 0, 10, 0}, {107, 24, 0, 10, 0},
    {107, 25, 0, 10, 0}, {107, 26, 0, 10, 0}, {107, 27, 0, 10, 0}, {107, 28, 0, 10, 0},
    {107, 29, 0, 10, 0}, {107, 30, 0, 10, 0}, {107, 31, 0, 10, 0}, {107, 32, 0, 10, 0},
    {107, 33, 0, 10, 0}, {107, 34, 0, 10, 0}, {107, 35, 0, 10, 0}, {107, 36, 0, 10, 0},
    {107, 37, 0, 10, 0}, {107, 38, 0, 10, 0}, {108, 20, 0, 10, 0}, {108, 21, 0, 10, 0},
    {108, 22, 0, 10, 0}, {108, 23, 0, 10, 0}, {108, 24, 0, 10, 0}, {108, 25, 0, 10, 0},
    {108, 26, 0, 10, 0}, {108, 27, 0, 10, 0}, {108, 28, 0, 10, 0}, {108, 29, 0, 10, 0},
    {108, 30, 0, 10, 0}, {108, 31, 0, 10, 0}, {108, 32, 0, 10, 0}, {108, 33, 0, 10, 0},
    {108, 34, 0, 10, 0}, {108, 35, 0, 10, 0}, {108, 36, 0, 10, 0}, {108, 37, 0, 10, 0},
    {108, 38, 0, 10, 0}, {109, 20, 0, 10, 0}, {109, 21, 0, 10, 0}, {109, 22, 0, 10, 0},
    {109, 23, 0, 10, 0}, {109, 24, 0, 10, 0}, {109, 25, 0, 10, 0}, {109, 26, 0, 10, 0},
    {109, 27, 0, 10, 0}, {109, 28, 0, 10, 0}, {109, 29, 0, 10, 0}, {109, 30, 0, 10, 0},
    {109, 31, 0, 10, 0}, {109, 32, 0, 10, 0}, {109, 33, 0, 10, 0}, {109, 34, 0, 10, 0},
    {109, 35, 0, 10, 0}, {109, 36, 0, 10, 0}, {109, 37, 0, 10, 0}, {109, 38, 0, 10, 0},
    {110, 20, 1, 10, 0}, {110, 21, 0, 10, 0}, {110, 22, 0, 10, 0}, {110, 23, 0, 10, 0},
    {110, 24, 0, 10, 0}, {110, 25, 0, 10, 0}, {110, 26, 0, 10, 0}, {110, 27, 0, 10, 0},
    {110, 28, 0, 10, 0}, {110, 29, 0, 10, 0}, {110, 30, 0, 10, 0}, {110, 31, 0, 10, 0},
    {110, 32, 0, 10, 0}, {110, 33, 0, 10, 0}, {110, 34, 0, 10, 0}, {110, 35, 0, 10, 0},
    {110, 36, 0, 10, 0}, {110, 37, 0, 10, 0}, {110, 38, 0, 10, 0}, {111, 20, 0, 10, 0},
    {111, 21, 0, 10, 0}, {111, 22, 0, 10, 0}, {111, 23, 0, 10, 0}, {111, 24, 0, 10, 0},
    {111, 25, 0, 10, 0}, {111, 26, 0, 10, 0}, {111, 27, 0, 10, 0}, {111, 28, 0, 10, 0},
    {111, 29, 0, 10, 0}, {111, 30, 0, 10, 0}, {111, 31, 0, 10, 0}, {111, 32, 0, 10, 0},
    {111, 33, 0, 10, 0}, {111, 34, 0, 10, 0}, {111, 35, 0, 10, 0}, {111, 36, 0, 10, 0},
    {111, 37, 0, 10, 0}, {111, 38, 0, 10, 0}, {112, 20, 0, 10, 0}, {112, 21, 0, 10, 0},
    {112, 22, 0, 10, 0}, {112, 23, 0, 10, 0}, {112, 24, 0, 10, 0}, {112, 25, 0, 10, 0},
    {112, 26, 0, 10, 0}, {112, 27, 0, 10, 0}, {112, 28, 0, 10, 0}, {112, 29, 0, 10, 0},
    {112, 30, 0, 10, 0}, {112, 31, 0, 10, 0}, {112, 32, 0, 10, 0}, {112, 33, 0, 10, 0},
    {112, 34, 0, 10, 0}, {112, 35, 0, 10, 0}, {112, 36, 0, 10, 0}, {112, 37, 0, 10, 0},
    {112, 38, 0, 10, 0}, {113, 20, 0, 10, 0}, {113, 21, 0, 10, 0}, {113, 22, 0, 10, 0},
    {113, 23, 0, 10, 0}, {113, 24, 0, 10, 0}, {113, 25, 0, 10, 0}, {113, 26, 0, 10, 0},
    {113, 27, 0, 10, 0}, {113, 28, 0, 10, 0}, {113, 29, 0, 10, 0}, {113, 30, 0, 10, 0},
    {113, 31, 0, 10, 0}, {113, 32, 0, 10, 0}, {113, 33, 0, 10, 0}, {113, 34, 0, 10, 0},
    {113, 35, 0, 10, 0}, {113, 36, 0, 10, 0}, {113, 37, 0, 10, 0}, {113, 38, 0, 10, 0},
    {114, 20, 0, 10, 0}, {114, 21, 0, 10, 0}, {114, 22, 0, 10, 0}, {114, 23, 0, 10, 0},
    {114, 24, 0, 10, 0}, {114, 25, 0, 10, 0}, {114, 26, 0, 10, 0}, {114, 27, 0, 10, 0},
    {114, 28, 0, 10, 0}, {114, 29, 0, 10, 0}, {114, 30, 0, 10, 0}, {114, 31, 0, 10, 0},
    {114, 32, 0, 10, 0}, {114, 33, 0, 10, 0}, {114, 34, 0, 10, 0}, {114, 35, 0, 10, 0},
    {114, 36, 0, 10, 0}, {114, 37, 0, 10, 0}, {114, 38, 0, 10, 0}, {115, 20, 1, 10, 0},
    {115, 21, 0, 10, 0}, {115, 22, 0, 10, 0}, {115, 23, 0, 10, 0}, {115, 24, 0, 10, 0},
    {115, 25, 0, 10, 0}, {115, 26, 0, 10, 0}, {115, 27, 0, 10, 0}, {115, 28, 0, 10, 0},
    {115, 29, 0, 10, 0}, {115, 30, 0, 10, 0}, {115, 31, 0, 10, 0}, {115, 32, 0, 10, 0},
    {115, 33, 0, 10, 0}, {115, 34, 0, 10, 0}, {115, 35, 0, 10, 0}, {115, 36, 0, 10, 0},
    {115, 37, 0, 10, 0}, {115, 38, 0, 10, 0}, {116, 20, 1, 10, 0}, {116, 21, 0, 10, 0},
    {116, 22, 0, 10, 0}, {116, 23, 1, 10, 0}, {116, 24, 0, 10, 0}, {116, 25, 0, 10, 0},
    {116, 26, 0, 10, 0}, {116, 27, 0, 10, 0}, {116, 28, 0, 10, 0}, {116, 29, 0, 10, 0},
    {116, 30, 0, 10, 0}, {116, 31, 0, 10, 0}, {116, 32, 0, 10, 0}, {116, 33, 0, 10, 0},
    {116, 34, 0, 10, 0}, {116, 35, 0, 10, 0}, {116, 36, 0, 10, 0}, {116, 37, 0, 10, 0},
    {116, 38, 0, 10, 0}, {117, 20, 0, 10, 0}, {117, 21, 0, 10, 0}, {117, 22, 0, 10, 0},
    {117, 23, 0, 10, 0}, {117, 24, 0, 10, 0}, {117, 25, 0, 10, 0}, {117, 26, 0, 10, 0},
    {117, 27, 0, 10, 0}, {117, 28, 0, 10, 0}, {117, 29, 0, 10, 0}, {117, 30, 0, 10, 0},
    {117, 31, 0, 10, 0}, {117, 32, 0, 10, 0}, {117, 33, 0, 10, 0}, {117, 34, 0, 10, 0},
    {117, 35, 0, 10, 0}, {117, 36, 0, 10, 0}, {117, 37, 0, 10, 0}, {117, 38, 0, 10, 0},
    {118, 20, 0, 10, 0}, {118, 21, 1, 10, 0}, {118, 22, 1, 10, 0}, {118, 23, 0, 10, 0},
    {118, 24, 0, 10, 0}, {118, 25, 0, 10, 0}, {118, 26, 0, 10, 0}, {118, 27, 0, 10, 0},
    {118, 28, 0, 10, 0}, {118, 29, 0, 10, 0}, {118, 30, 0, 10, 0}, {118, 31, 0, 10, 0},
    {118, 32, 0, 10, 0}, {118, 33, 0, 10, 0}, {118, 34, 0, 10, 0}, {118, 35, 0, 10, 0},
    {118, 36, 0, 10, 0}, {118, 37, 0, 10, 0}, {118, 38, 0, 10, 0}, {119, 20, 0, 10, 0},
    {119, 21, 0, 10, 0}, {119, 22, 0, 10, 0}, {119, 23, 0, 10, 0}, {119, 24, 0, 10, 0},
    {119, 25, 0, 10, 0}, {119, 26, 0, 10, 0}, {119, 27, 0, 10, 0}, {119, 28, 0, 10, 0},
    {119, 29, 0, 10, 0}, {119, 30, 0, 10, 0}, {119, 31, 0, 10, 0}, {119, 32, 0, 10, 0},
    {119, 33, 0, 10, 0}, {119, 34, 0, 10, 0}, {119, 35, 0, 10, 0}, {119, 36, 0, 10, 0},
    {119, 37, 1, 10, 0}, {119, 38, 0, 10, 0}, {58, 50, 0, 10, 0}, {58, 51, 0, 10, 0},
    {58, 52, 0, 10, 0}, {58, 53, 0, 10, 0}, {58, 54, 0, 10, 0}, {58, 55, 0, 10, 0},
    {58, 56, 0, 10, 0}, {58, 57, 0, 10, 0}, {58, 58, 0, 10, 0}, {58, 59, 0, 10, 0},
    {58, 60, 0, 10, 0}, {58, 61, 0, 10, 0}, {58, 62, 0, 10, 0}, {58, 63, 0, 10, 0},
    {58, 64, 0, 10, 0}, {58, 65, 0, 10, 0}, {58, 66, 0, 10, 0}, {58, 67, 0, 10, 0},
    {58, 68, 0, 10, 0}, {63, 50, 0, 10, 0}, {63, 51, 0, 10, 0}, {63, 52, 0, 10, 0},
    {63, 53, 0, 10, 0}, {63, 54, 0, 10, 0}, {63, 55, 0, 10, 0}, {63, 56, 0, 10, 0},
    {63, 57, 0, 10, 0}, {63, 58, 0, 10, 0}, {63, 59, 0, 10, 0}, {63, 60, 0, 10, 0},
    {63, 61, 0, 10, 0}, {63, 62, 0, 10, 0}, {63, 63, 0, 10, 0}, {63, 64, 0, 10, 0},
    {63, 65, 0, 10, 0}, {63, 66, 0, 10, 0}, {63, 67, 0, 10, 0}, {63, 68, 0, 10, 0},
    {63, 80, 0, 10, 0}, {63, 81, 0, 10, 0}, {63, 82, 0, 10, 0}, {63, 83, 0, 10, 0},
    {63, 84, 0, 10, 0}, {63, 85, 0, 10, 0}, {63, 86, 0, 10, 0}, {63, 87, 0, 10, 0},
    {63, 88, 0, 10, 0}, {63, 89, 0, 10, 0}, {63, 90, 0, 10, 0}, {63, 91, 0, 10, 0},
    {63, 92, 0, 10, 0}, {63, 93, 0, 10, 0}, {63, 94, 0, 10, 0}, {63, 95, 0, 10, 0},
    {63, 96, 0, 10, 0}, {63, 97, 0, 10, 0}, {63, 98, 0, 10, 0}, {64, 50, 1, 10, 0},
    {64, 51, 0, 10, 0}, {64, 52, 0, 10, 0}, {64, 53, 0, 10, 0}, {64, 54, 0, 10, 0},
    {64, 55, 0, 10, 0}, {64, 56, 0, 10, 0}, {64, 57, 0, 10, 0}, {64, 58, 0, 10, 0},
    {64, 59, 0, 10, 0}, {64, 60, 0, 10, 0}, {64, 61, 0, 10, 0}, {64, 62, 0, 10, 0},
    {64, 63, 0, 10, 0}, {64, 64, 0, 10, 0}, {64, 65, 0, 10, 0}, {64, 66, 0, 10, 0},
    {64, 67, 0, 10, 0}, {64, 68, 0, 10, 0}, {72, 50, 0, 10, 0}, {72, 51, 0, 10, 0},
    {72, 52, 0, 10, 0}, {72, 53, 0, 10, 0}, {72, 54, 0, 10, 0}, {72, 55, 0, 10, 0},
    {72, 56, 0, 10, 0}, {72, 57, 0, 10, 0}, {72, 58, 0, 10, 0}, {72, 59, 0, 10, 0},
    {72, 60, 0, 10, 0}, {72, 61, 0, 10, 0}, {72, 62, 0, 10, 0}, {72, 63, 0, 10, 0},
    {72, 64, 0, 10, 0}, {72, 65, 0, 10, 0}, {72, 66, 0, 10, 0}, {72, 67, 0, 10, 0},
    {72, 68, 0, 10, 0}, {75, 50, 0, 10, 0}, {75, 51, 0, 10, 0}, {75, 52, 0, 10, 0},
    {75, 53, 0, 10, 0}, {75, 54, 0, 10, 0}, {75, 55, 0, 10, 0}, {75, 56, 0, 10, 0},
    {75, 57, 0, 10, 0}, {75, 58, 0, 10, 0}, {75, 59, 0, 10, 0}, {75, 60, 0, 10, 0},
    {75, 61, 0, 10, 0}, {75, 62, 0, 10, 0}, {75, 63, 0, 10, 0}, {75, 64, 0, 10, 0},
    {75, 65, 0, 10, 0}, {75, 66, 0, 10, 0}, {75, 67, 0, 10, 0}, {75, 68, 0, 10, 0},
    {76, 50, 0, 10, 0}, {76, 51, 0, 10, 0}, {76, 52, 0, 10, 0}, {76, 53, 0, 10, 0},
    {76, 54, 0, 10, 0}, {76, 55, 0, 10, 0}, {76, 56, 0, 10, 0}, {76, 57, 0, 10, 0},
    {76, 58, 0, 10, 0}, {76, 59, 0, 10, 0}, {76, 60, 0, 10, 0}, {76, 61, 0, 10, 0},
    {76, 62, 0, 10, 0}, {76, 63, 0, 10, 0}, {76, 64, 0, 10, 0}, {76, 65, 0, 10, 0},
    {76, 66, 0, 10, 0}, {76, 67, 0, 10, 0}, {76, 68, 0, 10, 0}, {82, 50, 0, 10, 0},
    {82, 51, 0, 10, 0}, {82, 52, 0, 10, 0}, {82, 53, 0, 10, 0}, {82, 54, 0, 10, 0},
    {82, 55, 0, 10, 0}, {82, 56, 0, 10, 0}, {82, 57, 0, 10, 0}, {82, 58, 0, 10, 0},
    {82, 59, 0, 10, 0}, {82, 60, 0, 10, 0}, {82, 61, 0, 10, 0}, {82, 62, 0, 10, 0},
    {82, 63, 0, 10, 0}, {82, 64, 0, 10, 0}, {82, 65, 0, 10, 0}, {82, 66, 0, 10, 0},
    {82, 67, 0, 10, 0}, {82, 68, 0, 10, 0}, {82, 80, 0, 10, 0}, {82, 81, 0, 10, 0},
    {82, 82, 0, 10, 0}, {82, 83, 0, 10, 0}, {82, 84, 0, 10, 0}, {82, 85, 0, 10, 0},
    {82, 86, 0, 10, 0}, {82, 87, 0, 10, 0}, {82, 88, 0, 10, 0}, {82, 89, 0, 10, 0},
    {82, 90, 0, 10, 0}, {82, 91, 0, 10, 0}, {82, 92, 0, 10, 0}, {82, 93, 0, 10, 0},
    {82, 94, 0, 10, 0}, {82, 95, 0, 10, 0}, {82, 96, 0, 10, 0}, {82, 97, 0, 10, 0},
    {82, 98, 0, 10, 0}, {83, 50, 0, 10, 0}, {83, 51, 0, 10, 0}, {83, 52, 0, 10, 0},
    {83, 53, 0, 10, 0}, {83, 54, 0, 10, 0}, {83, 55, 0, 10, 0}, {83, 56, 0, 10, 0},
    {83, 57, 0, 10, 0}, {83, 58, 0, 10, 0}, {83, 59, 0, 10, 0}, {83, 60, 0, 10, 0},
    {83, 61, 0, 10, 0}, {83, 62, 0, 10, 0}, {83, 63, 0, 10, 0}, {83, 64, 0, 10, 0},
    {83, 65, 0, 10, 0}, {83, 66, 0, 10, 0}, {83, 67, 0, 10, 0}, {83, 68, 0, 10, 0},
    {84, 50, 0, 10, 0}, {84, 51, 0, 10, 0}, {84, 52, 0, 10, 0}, {84, 53, 0, 10, 0},
    {84, 54, 0, 10, 0}, {84, 55, 0, 10, 0}, {84, 56, 0, 10, 0}, {84, 57, 0, 10, 0},
    {84, 58, 0, 10, 0}, {84, 59, 0, 10, 0}, {84, 60, 0, 10, 0}, {84, 61, 0, 10, 0},
    {84, 62, 0, 10, 0}, {84, 63, 0, 10, 0}, {84, 64, 0, 10, 0}, {84, 65, 0, 10, 0},
    {84, 66, 0, 10, 0}, {84, 67, 0, 10, 0}, {84, 68, 0, 10, 0}, {95, 50, 0, 10, 0},
    {95, 51, 0, 10, 0}, {95, 52, 0, 10, 0}, {95, 53, 0, 10, 0}, {95, 54, 0, 10, 0},
    {95, 55, 0, 10, 0}, {95, 56, 0, 10, 0}, {95, 57, 0, 10, 0}, {95, 58, 0, 10, 0},
    {95, 59, 0, 10, 0}, {95, 60, 0, 10, 0}, {95, 61, 0, 10, 0}, {95, 62, 0, 10, 0},
    {95, 63, 0, 10, 0}, {95, 64, 0, 10, 0}, {95, 65, 0, 10, 0}, {95, 66, 0, 10, 0},
    {95, 67, 0, 10, 0}, {95, 68, 0, 10, 0}, {111, 50, 0, 10, 0}, {111, 51, 0, 10, 0},
    {111, 52, 0, 10, 0}, {111, 53, 0, 10, 0}, {111, 54, 0, 10, 0}, {111, 55, 0, 10, 0},
    {111, 56, 0, 10, 0}, {111, 57, 0, 10, 0}, {111, 58, 0, 10, 0}, {111, 59, 0, 10, 0},
    {111, 60, 0, 10, 0}, {111, 61, 0, 10, 0}, {111, 62, 0, 10, 0}, {111, 63, 0, 10, 0},
    {111, 64, 0, 10, 0}, {111, 65, 0, 10, 0}, {111, 66, 0, 10, 0}, {111, 67, 0, 10, 0},
    {111, 68, 0, 10, 0}, {111, 80, 0, 10, 0}, {111, 81, 0, 10, 0}, {111, 82, 0, 10, 0},
    {111, 83, 0, 10, 0}, {111, 84, 0, 10, 0}, {111, 85, 0, 10, 0}, {111, 86, 0, 10, 0},
    {111, 87, 0, 10, 0}, {111, 88, 0, 10, 0}, {111, 89, 0, 10, 0}, {111, 90, 0, 10, 0},
    {111, 91, 0, 10, 0}, {111, 92, 0, 10, 0}, {111, 93, 0, 10, 0}, {111, 94, 0, 10, 0},
    {111, 95, 0, 10, 0}, {111, 96, 0, 10, 0}, {111, 97, 0, 10, 0}, {111, 98, 0, 10, 0},
    {86, 50, 0, 10, 0}, {86, 51, 0, 10, 0}, {86, 52, 0, 10, 0}, {86, 53, 0, 10, 0},
    {86, 54, 0, 10, 0}, {86, 55, 0, 10, 0}, {86, 56, 0, 10, 0}, {86, 57, 0, 10, 0},
    {86, 58, 0, 10, 0}, {86, 59, 0, 10, 0}, {86, 60, 0, 10, 0}, {86, 61, 0, 10, 0},
    {86, 62, 0, 10, 0}, {86, 63, 0, 10, 0}, {86, 64, 0, 10, 0}, {86, 65, 0, 10, 0},
    {86, 66, 0, 10, 0}, {86, 67, 0, 10, 0}, {86, 68, 0, 10, 0}, {86, 80, 0, 10, 0},
    {86, 81, 0, 10, 0}, {86, 82, 0, 10, 0}, {86, 83, 0, 10, 0}, {86, 84, 0, 10, 0},
    {86, 85, 0, 10, 0}, {86, 86, 0, 10, 0}, {86, 87, 0, 10, 0}, {86, 88, 0, 10, 0},
    {86, 89, 0, 10, 0}, {86, 90, 0, 10, 0}, {86, 91, 0, 10, 0}, {86, 92, 0, 10, 0},
    {86, 93, 0, 10, 0}, {86, 94, 0, 10, 0}, {86, 95, 0, 10, 0}, {86, 96, 0, 10, 0},
    {86, 97, 0, 10, 0}, {86, 98, 0, 10, 0}, {87, 50, 0, 10, 0}, {87, 51, 0, 10, 0},
    {87, 52, 0, 10, 0}, {87, 53, 0, 10, 0}, {87, 54, 0, 10, 0}, {87, 55, 0, 10, 0},
    {87, 56, 0, 10, 0}, {87, 57, 0, 10, 0}, {87, 58, 0, 10, 0}, {87, 59, 0, 10, 0},
    {87, 60, 0, 10, 0}, {87, 61, 0, 10, 0}, {87, 62, 0, 10, 0}, {87, 63, 0, 10, 0},
    {87, 64, 0, 10, 0}, {87, 65, 0, 10, 0}, {87, 66, 0, 10, 0}, {87, 67, 0, 10, 0},
    {87, 68, 0, 10, 0}, {87, 80, 0, 10, 0}, {87, 81, 0, 10, 0}, {87, 82, 0, 10, 0},
    {87, 83, 0, 10, 0}, {87, 84, 0, 10, 0}, {87, 85, 0, 10, 0}, {87, 86, 0, 10, 0},
    {87, 87, 0, 10, 0}, {87, 88, 0, 10, 0}, {87, 89, 0, 10, 0}, {87, 90, 0, 10, 0},
    {87, 91, 0, 10, 0}, {87, 92, 0, 10, 0}, {87, 93, 0, 10, 0}, {87, 94, 0, 10, 0},
    {87, 95, 0, 10, 0}, {87, 96, 0, 10, 0}, {87, 97, 0, 10, 0}, {87, 98, 0, 10, 0},
    {90, 50, 0, 10, 0}, {90, 51, 0, 10, 0}, {90, 52, 0, 10, 0}, {90, 53, 0, 10, 0},
    {90, 54, 0, 10, 0}, {90, 55, 0, 10, 0}, {90, 56, 0, 10, 0}, {90, 57, 0, 10, 0},
    {90, 58, 0, 10, 0}, {90, 59, 0, 10, 0}, {90, 60, 0, 10, 0}, {90, 61, 0, 10, 0},
    {90, 62, 0, 10, 0}, {90, 63, 0, 10, 0}, {90, 64, 0, 10, 0}, {90, 65, 0, 10, 0},
    {90, 66, 0, 10, 0}, {90, 67, 0, 10, 0}, {90, 68, 0, 10, 0}, {91, 50, 0, 10, 0},
    {91, 51, 0, 10, 0}, {91, 52, 0, 10, 0}, {91, 53, 0, 10, 0}, {91, 54, 0, 10, 0},
    {91, 55, 0, 10, 0}, {91, 56, 0, 10, 0}, {91, 57, 0, 10, 0}, {91, 58, 0, 10, 0},
    {91, 59, 0, 10, 0}, {91, 60, 0, 10, 0}, {91, 61, 0, 10, 0}, {91, 62, 0, 10, 0},
    {91, 63, 0, 10, 0}, {91, 64, 0, 10, 0}, {91, 65, 0, 10, 0}, {91, 66, 0, 10, 0},
    {91, 67, 0, 10, 0}, {91, 68, 0, 10, 0}, {83, 80, 0, 10, 0}, {83, 81, 0, 10, 0},
    {83, 82, 0, 10, 0}, {83, 83, 0, 10, 0}, {83, 84, 0, 10, 0}, {83, 85, 0, 10, 0},
    {83, 86, 0, 10, 0}, {83, 87, 0, 10, 0}, {83, 88, 0, 10, 0}, {83, 89, 0, 10, 0},
    {83, 90, 0, 10, 0}, {83, 91, 0, 10, 0}, {83, 92, 0, 10, 0}, {83, 93, 0, 10, 0},
    {83, 94, 0, 10, 0}, {83, 95, 0, 10, 0}, {83, 96, 0, 10, 0}, {83, 97, 0, 10, 0},
    {83, 98, 0, 10, 0}, {54, 50, 0, 10, 0}, {54, 51, 0, 10, 0}, {54, 52, 0, 10, 0},
    {54, 53, 0, 10, 0}, {54, 54, 0, 10, 0}, {54, 55, 0, 10, 0}, {54, 56, 0, 10, 0},
    {54, 57, 0, 10, 0}, {54, 58, 0, 10, 0}, {54, 59, 0, 10, 0}, {54, 60, 0, 10, 0},
    {54, 61, 0, 10, 0}, {54, 62, 0, 10, 0}, {54, 63, 0, 10, 0}, {54, 64, 0, 10, 0},
    {54, 65, 0, 10, 0}, {54, 66, 0, 10, 0}, {54, 67, 0, 10, 0}, {54, 68, 0, 10, 0},
    {54, 80, 0, 10, 0}, {54, 81, 0, 10, 0}, {54, 82, 0, 10, 0}, {54, 83, 0, 10, 0},
    {54, 84, 0, 10, 0}, {54, 85, 0, 10, 0}, {54, 86, 0, 10, 0}, {54, 87, 0, 10, 0},
    {54, 88, 0, 10, 0}, {54, 89, 0, 10, 0}, {54, 90, 0, 10, 0}, {54, 91, 0, 10, 0},
    {54, 92, 0, 10, 0}, {54, 93, 0, 10, 0}, {54, 94, 0, 10, 0}, {54, 95, 0, 10, 0},
    {54, 96, 0, 10, 0}, {54, 97, 0, 10, 0}, {54, 98, 0, 10, 0}, {55, 50, 0, 10, 0},
    {55, 51, 0, 10, 0}, {55, 52, 0, 10, 0}, {55, 53, 0, 10, 0}, {55, 54, 0, 10, 0},
    {55, 55, 0, 10, 0}, {55, 56, 0, 10, 0}, {55, 57, 0, 10, 0}, {55, 58, 0, 10, 0},
    {55, 59, 0, 10, 0}, {55, 60, 0, 10, 0}, {55, 61, 0, 10, 0}, {55, 62, 0, 10, 0},
    {55, 63, 0, 10, 0}, {55, 64, 0, 10, 0}, {55, 65, 0, 10, 0}, {55, 66, 0, 10, 0},
    {55, 67, 0, 10, 0}, {55, 68, 0, 10, 0}, {57, 50, 0, 10, 0}, {57, 51, 0, 10, 0},
    {57, 52, 0, 10, 0}, {57, 53, 0, 10, 0}, {57, 54, 0, 10, 0}, {57, 55, 0, 10, 0},
    {57, 56, 0, 10, 0}, {57, 57, 0, 10, 0}, {57, 58, 0, 10, 0}, {57, 59, 0, 10, 0},
    {57, 60, 0, 10, 0}, {57, 61, 0, 10, 0}, {57, 62, 0, 10, 0}, {57, 63, 0, 10, 0},
    {57, 64, 0, 10, 0}, {57, 65, 0, 10, 0}, {57, 66, 0, 10, 0}, {57, 67, 0, 10, 0},
    {57, 68, 0, 10, 0}, {59, 50, 0, 10, 0}, {59, 51, 0, 10, 0}, {59, 52, 0, 10, 0},
    {59, 53, 0, 10, 0}, {59, 54, 0, 10, 0}, {59, 55, 0, 10, 0}, {59, 56, 0, 10, 0},
    {59, 57, 1, 10, 0}, {59, 58, 0, 10, 0}, {59, 59, 0, 10, 0}, {59, 60, 0, 10, 0},
    {59, 61, 0, 10, 0}, {59, 62, 0, 10, 0}, {59, 63, 0, 10, 0}, {59, 64, 0, 10, 0},
    {59, 65, 0, 10, 0}, {59, 66, 0, 10, 0}, {59, 67, 0, 10, 0}, {59, 68, 0, 10, 0},
    {62, 50, 0, 10, 0}, {62, 51, 0, 10, 0}, {62, 52, 0, 10, 0}, {62, 53, 0, 10, 0},
    {62, 54, 0, 10, 0}, {62, 55, 1, 10, 0}, {62, 56, 0, 10, 0}, {62, 57, 0, 10, 0},
    {62, 58, 0, 10, 0}, {62, 59, 0, 10, 0}, {62, 60, 0, 10, 0}, {62, 61, 0, 10, 0},
    {62, 62, 0, 10, 0}, {62, 63, 0, 10, 0}, {62, 64, 0, 10, 0}, {62, 65, 0, 10, 0},
    {62, 66, 0, 10, 0}, {62, 67, 0, 10, 0}, {62, 68, 0, 10, 0}, {62, 80, 1, 10, 0},
    {62, 81, 0, 10, 0}, {62, 82, 0, 10, 0}, {62, 83, 0, 10, 0}, {62, 84, 0, 10, 0},
    {62, 85, 0, 10, 0}, {62, 86, 1, 10, 0}, {62, 87, 0, 10, 0}, {62, 88, 0, 10, 0},
    {62, 89, 0, 10, 0}, {62, 90, 0, 10, 0}, {62, 91, 0, 10, 0}, {62, 92, 0, 10, 0},
    {62, 93, 0, 10, 0}, {62, 94, 0, 10, 0}, {62, 95, 0, 10, 0}, {62, 96, 0, 10, 0},
    {62, 97, 0, 10, 0}, {62, 98, 0, 10, 0}, {64, 80, 0, 10, 0}, {64, 81, 0, 10, 0},
    {64, 82, 0, 10, 0}, {64, 83, 0, 10, 0}, {64, 84, 0, 10, 0}, {64, 85, 0, 10, 0},
    {64, 86, 0, 10, 0}, {64, 87, 0, 10, 0}, {64, 88, 0, 10, 0}, {64, 89, 0, 10, 0},
    {64, 90, 0, 10, 0}, {64, 91, 0, 10, 0}, {64, 92, 0, 10, 0}, {64, 93, 0, 10, 0},
    {64, 94, 0, 10, 0}, {64, 95, 0, 10, 0}, {64, 96, 0, 10, 0}, {64, 97, 0, 10, 0},
    {64, 98, 0, 10, 0}, {105, 20, 0, 10, 0}, {105, 21, 0, 10, 0}, {105, 22, 0, 10, 0},
    {105, 23, 0, 10, 0}, {105, 24, 0, 10, 0}, {105, 25, 0, 10, 0}, {105, 26, 0, 10, 0},
    {105, 27, 0, 10, 0}, {105, 28, 0, 10, 0}, {105, 29, 0, 10, 0}, {105, 30, 0, 10, 0},
    {105, 31, 0, 10, 0}, {105, 32, 0, 10, 0}, {105, 33, 0, 10, 0}, {105, 34, 0, 10, 0},
    {105, 35, 0, 10, 0}, {105, 36, 0, 10, 0}, {105, 37, 0, 10, 0}, {105, 38, 0, 10, 0},
    {105, 50, 0, 10, 0}, {105, 51, 0, 10, 0}, {105, 52, 0, 10, 0}, {105, 53, 0, 10, 0},
    {105, 54, 0, 10, 0}, {105, 55, 0, 10, 0}, {105, 56, 0, 10, 0}, {105, 57, 0, 10, 0},
    {105, 58, 0, 10, 0}, {105, 59, 0, 10, 0}, {105, 60, 0, 10, 0}, {105, 61, 0, 10, 0},
    {105, 62, 0, 10, 0}, {105, 63, 0, 10, 0}, {105, 64, 0, 10, 0}, {105, 65, 0, 10, 0},
    {105, 66, 0, 10, 0}, {105, 67, 0, 10, 0}, {105, 68, 0, 10, 0}, {105, 80, 0, 10, 0},
    {105, 81, 0, 10, 0}, {105, 82, 0, 10, 0}, {105, 83, 0, 10, 0}, {105, 84, 0, 10, 0},
    {105, 85, 0, 10, 0}, {105, 86, 0, 10, 0}, {105, 87, 0, 10, 0}, {105, 88, 0, 10, 0},
    {105, 89, 0, 10, 0}, {105, 90, 0, 10, 0}, {105, 91, 0, 10, 0}, {105, 92, 0, 10, 0},
    {105, 93, 0, 10, 0}, {105, 94, 0, 10, 0}, {105, 95, 0, 10, 0}, {105, 96, 0, 10, 0},
    {105, 97, 0, 10, 0}, {105, 98, 0, 10, 0}, {106, 20, 0, 10, 0}, {106, 21, 0, 10, 0},
    {106, 22, 0, 10, 0}, {106, 23, 0, 10, 0}, {106, 24, 0, 10, 0}, {106, 25, 0, 10, 0},
    {106, 26, 0, 10, 0}, {106, 27, 0, 10, 0}, {106, 28, 0, 10, 0}, {106, 29, 0, 10, 0},
    {106, 30, 0, 10, 0}, {106, 31, 0, 10, 0}, {106, 32, 0, 10, 0}, {106, 33, 0, 10, 0},
    {106, 34, 0, 10, 0}, {106, 35, 0, 10, 0}, {106, 36, 0, 10, 0}, {106, 37, 0, 10, 0},
    {106, 38, 0, 10, 0}, {106, 50, 0, 10, 0}, {106, 51, 0, 10, 0}, {106, 52, 0, 10, 0},
    {106, 53, 0, 10, 0}, {106, 54, 0, 10, 0}, {106, 55, 0, 10, 0}, {106, 56, 0, 10, 0},
    {106, 57, 0, 10, 0}, {106, 58, 0, 10, 0}, {106, 59, 0, 10, 0}, {106, 60, 0, 10, 0},
    {106, 61, 0, 10, 0}, {106, 62, 0, 10, 0}, {106, 63, 0, 10, 0}, {106, 64, 0, 10, 0},
    {106, 65, 0, 10, 0}, {106, 66, 0, 10, 0}, {106, 67, 0, 10, 0}, {106, 68, 0, 10, 0},
    {88, 50, 0, 10, 0}, {88, 51, 0, 10, 0}, {88, 52, 0, 10, 0}, {88, 53, 0, 10, 0},
    {88, 54, 0, 10, 0}, {88, 55, 0, 10, 0}, {88, 56, 0, 10, 0}, {88, 57, 0, 10, 0},
    {88, 58, 0, 10, 0}, {88, 59, 0, 10, 0}, {88, 60, 0, 10, 0}, {88, 61, 0, 10, 0},
    {88, 62, 0, 10, 0}, {88, 63, 0, 10, 0}, {88, 64, 0, 10, 0}, {88, 65, 0, 10, 0},
    {88, 66, 0, 10, 0}, {88, 67, 0, 10, 0}, {88, 68, 0, 10, 0}, {89, 50, 0, 10, 0},
    {89, 51, 0, 10, 0}, {89, 52, 0, 10, 0}, {89, 53, 0, 10, 0}, {89, 54, 0, 10, 0},
    {89, 55, 0, 10, 0}, {89, 56, 0, 10, 0}, {89, 57, 0, 10, 0}, {89, 58, 0, 10, 0},
    {89, 59, 0, 10, 0}, {89, 60, 0, 10, 0}, {89, 61, 0, 10, 0}, {89, 62, 0, 10, 0},
    {89, 63, 0, 10, 0}, {89, 64, 0, 10, 0}, {89, 65, 0, 10, 0}, {89, 66, 0, 10, 0},
    {89, 67, 0, 10, 0}, {89, 68, 0, 10, 0}, {120, 20, 0, 10, 0}, {120, 21, 0, 10, 0},
    {120, 22, 0, 10, 0}, {120, 23, 0, 10, 0}, {120, 24, 0, 10, 0}, {120, 25, 0, 10, 0},
    {120, 26, 0, 10, 0}, {120, 27, 0, 10, 0}, {120, 28, 0, 10, 0}, {120, 29, 0, 10, 0},
    {120, 30, 0, 10, 0}, {120, 31, 0, 10, 0}, {120, 32, 0, 10, 0}, {120, 33, 0, 10, 0},
    {120, 34, 0, 10, 0}, {120, 35, 0, 10, 0}, {120, 36, 0, 10, 0}, {120, 37, 0, 10, 0},
    {120, 38, 1, 10, 0}, {121, 20, 0, 10, 0}, {121, 21, 0, 10, 0}, {121, 22, 0, 10, 0},
    {121, 23, 0, 10, 0}, {121, 24, 0, 10, 0}, {121, 25, 0, 10, 0}, {121, 26, 0, 10, 0},
    {121, 27, 1, 10, 0}, {121, 28, 1, 10, 0}, {121, 29, 0, 10, 0}, {121, 30, 0, 10, 0},
    {121, 31, 0, 10, 0}, {121, 32, 0, 10, 0}, {121, 33, 0, 10, 0}, {121, 34, 0, 10, 0},
    {121, 35, 0, 10, 0}, {121, 36, 0, 10, 0}, {121, 37, 0, 10, 0}, {121, 38, 0, 10, 0},
    {121, 39, 0, 10, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0},
};
// clang-format on

// clang-format off
SND_SE_INFO geo[199] = {
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {38, 38, 0, 14, 0}, {38, 39, 0, 14, 0}, {38, 40, 0, 14, 0}, {38, 41, 0, 14, 0},
    {38, 42, 0, 14, 0}, {38, 43, 0, 14, 0}, {38, 44, 0, 14, 0}, {38, 45, 0, 14, 0},
    {38, 46, 0, 14, 0}, {38, 47, 0, 14, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {38, 56, 0, 14, 0}, {38, 57, 0, 14, 0},
    {38, 58, 0, 14, 0}, {38, 59, 0, 14, 0}, {38, 60, 0, 14, 0}, {38, 61, 0, 14, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {38, 63, 0, 14, 0}, {38, 64, 0, 14, 0}, {38, 65, 0, 14, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {38, 66, 0, 14, 0}, {-1, -1, 0, -1, 0}, {-128, -128, -128, -128, 0},
};
// clang-format on

// clang-format off
SND_SE_INFO dun[199] = {
    {-1, -1, 0, -1, 0}, {30, 21, 0, 14, 0}, {30, 22, 0, 14, 0}, {30, 23, 0, 14, 0},
    {30, 24, 0, 14, 0}, {30, 25, 0, 14, 0}, {-1, -1, 0, -1, 0}, {30, 27, 0, 14, 0},
    {30, 28, 0, 14, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {30, 31, 0, 14, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {30, 38, 0, 14, 0}, {30, 39, 0, 14, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {30, 43, 0, 14, 0},
    {30, 44, 0, 14, 0}, {30, 45, 0, 14, 0}, {30, 46, 0, 14, 0}, {30, 47, 0, 14, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {36, 20, 0, 14, 0}, {36, 21, 0, 14, 0},
    {-1, -1, 0, -1, 0}, {36, 23, 0, 14, 0}, {36, 24, 0, 14, 0}, {36, 25, 0, 14, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {36, 89, 0, 14, 0},
    {36, 90, 0, 14, 0}, {36, 91, 0, 14, 0}, {36, 92, 0, 14, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {38, 36, 0, 14, 0}, {38, 37, 0, 14, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {38, 40, 0, 14, 0}, {38, 41, 0, 14, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {38, 51, 0, 14, 0}, {38, 52, 0, 14, 0}, {38, 53, 0, 14, 0},
    {38, 54, 0, 14, 0}, {38, 55, 0, 14, 0}, {38, 56, 0, 14, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {38, 62, 0, 14, 0}, {44, 23, 0, 14, 0}, {44, 24, 0, 14, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {38, 66, 0, 14, 0}, {-1, -1, 0, -1, 0}, {-128, -128, -128, -128, 0},
};
// clang-format on

// clang-format off
/** The sound-effect set cap_se_info names as chapter set 0. */
SND_SE_INFO cap0[92] = {
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {44, 23, 0, 10, 0},
    {44, 24, 0, 10, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {44, 74, 0, 10, 0}, {44, 75, 0, 10, 0},
    {44, 76, 0, 10, 0}, {44, 77, 1, 10, 0}, {44, 78, 0, 10, 0}, {44, 79, 0, 10, 0},
    {44, 80, 0, 10, 0}, {44, 81, 1, 10, 0}, {44, 82, 0, 10, 0}, {44, 83, 0, 10, 0},
    {44, 84, 1, 10, 0}, {44, 85, 0, 10, 0}, {-1, -1, 0, -1, 0}, {44, 87, 0, 10, 0},
    {44, 88, 0, 10, 0}, {44, 89, 0, 10, 0}, {-1, -1, 0, -1, 0}, {-128, -128, -128, -128, 0},
};
// clang-format on

// clang-format off
/** The sound-effect set cap_se_info names as chapter set 1. */
SND_SE_INFO cap1[73] = {
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {44, 82, 0, 10, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {45, 53, 0, 10, 0}, {45, 54, 0, 10, 0}, {45, 55, 1, 10, 0},
    {45, 56, 0, 10, 0}, {45, 57, 1, 10, 0}, {-1, -1, 0, -1, 0}, {45, 59, 0, 10, 0},
    {45, 60, 1, 10, 0}, {45, 61, 0, 10, 0}, {45, 62, 0, 10, 0}, {45, 63, 0, 10, 0},
    {45, 64, 0, 10, 0}, {45, 65, 0, 10, 0}, {45, 66, 0, 10, 0}, {45, 67, 0, 10, 0},
    {45, 68, 0, 10, 0}, {45, 69, 0, 10, 0}, {45, 70, 0, 10, 0}, {-1, -1, 0, -1, 0},
    {-128, -128, -128, -128, 0},
};
// clang-format on

// clang-format off
/** The sound-effect set cap_se_info names as chapter set 2. */
SND_SE_INFO cap2[101] = {
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {44, 82, 0, 10, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {45, 100, 0, 10, 0}, {45, 101, 0, 10, 0},
    {45, 102, 0, 10, 0}, {-1, -1, 0, -1, 0}, {45, 104, 0, 10, 0}, {45, 105, 0, 10, 0},
    {45, 106, 0, 10, 0}, {45, 107, 0, 10, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {45, 87, 1, 10, 0},
    {45, 88, 0, 10, 0}, {45, 89, 0, 10, 0}, {45, 90, 0, 10, 0}, {45, 91, 0, 10, 0},
    {45, 92, 0, 10, 0}, {45, 93, 0, 10, 0}, {-1, -1, 0, -1, 0}, {45, 95, 1, 10, 0},
    {45, 96, 0, 10, 0}, {45, 97, 0, 10, 0}, {45, 98, 0, 10, 0}, {45, 99, 0, 10, 0},
    {-128, -128, -128, -128, 0},
};
// clang-format on

// clang-format off
/** The sound-effect set cap_se_info names as chapter set 3. */
SND_SE_INFO cap3[44] = {
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {44, 82, 0, 10, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {44, 23, 0, 10, 0},
    {44, 24, 0, 10, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {45, 34, 0, 10, 0}, {45, 35, 0, 10, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {45, 39, 0, 10, 0},
    {-1, -1, 0, -1, 0}, {45, 41, 0, 10, 0}, {-1, -1, 0, -1, 0}, {-128, -128, -128, -128, 0},
};
// clang-format on

// clang-format off
/** The sound-effect set cap_se_info names as chapter set 4. */
SND_SE_INFO cap4[36] = {
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {44, 82, 0, 10, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {46, 25, 0, 10, 0}, {46, 26, 0, 10, 0}, {46, 27, 0, 10, 0},
    {-1, -1, 0, -1, 0}, {45, 29, 0, 10, 0}, {45, 30, 1, 10, 0}, {45, 31, 0, 10, 0},
    {45, 32, 0, 10, 0}, {46, 29, 0, 10, 0}, {-1, -1, 0, -1, 0}, {-128, -128, -128, -128, 0},
};
// clang-format on

// clang-format off
/** The sound-effect set cap_se_info names as chapter set 10. */
SND_SE_INFO cap10[85] = {
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {46, 50, 0, 10, 0}, {46, 51, 0, 10, 0},
    {46, 52, 0, 10, 0}, {46, 53, 0, 10, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {46, 56, 1, 10, 0}, {46, 57, 0, 10, 0}, {46, 58, 0, 10, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {44, 82, 0, 10, 0}, {-1, -1, 0, -1, 0},
    {-128, -128, -128, -128, 0},
};
// clang-format on

// clang-format off
/** The sound-effect set cap_se_info names as chapter set 11. */
SND_SE_INFO cap11[101] = {
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {44, 82, 0, 10, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {45, 100, 0, 10, 0}, {45, 101, 0, 10, 0},
    {45, 102, 0, 10, 0}, {-1, -1, 0, -1, 0}, {45, 104, 0, 10, 0}, {45, 105, 0, 10, 0},
    {45, 106, 0, 10, 0}, {45, 107, 0, 10, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {45, 87, 1, 10, 0},
    {45, 88, 0, 10, 0}, {45, 89, 0, 10, 0}, {45, 90, 0, 10, 0}, {45, 91, 0, 10, 0},
    {45, 92, 0, 10, 0}, {45, 93, 0, 10, 0}, {-1, -1, 0, -1, 0}, {45, 95, 1, 10, 0},
    {45, 96, 0, 10, 0}, {45, 97, 0, 10, 0}, {45, 98, 0, 10, 0}, {45, 99, 0, 10, 0},
    {-128, -128, -128, -128, 0},
};
// clang-format on

// clang-format off
/** The sound-effect set cap_se_info names as chapter set 12. */
SND_SE_INFO cap12[31] = {
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {44, 110, 1, 10, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {45, 26, 0, 10, 0}, {-1, -1, 0, -1, 0},
    {45, 28, 0, 10, 0}, {-1, -1, 0, -1, 0}, {-128, -128, -128, -128, 0},
};
// clang-format on

// clang-format off
/** The sound-effect set cap_se_info names as chapter set 13. */
SND_SE_INFO cap13[28] = {
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {44, 110, 1, 10, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {45, 23, 0, 10, 0},
    {45, 24, 0, 10, 0}, {45, 25, 0, 10, 0}, {-1, -1, 0, -1, 0}, {-128, -128, -128, -128, 0},
};
// clang-format on

// clang-format off
/** The sound-effect set cap_se_info names as chapter set 14. */
SND_SE_INFO cap14[36] = {
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {44, 82, 0, 10, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {46, 25, 0, 10, 0}, {46, 26, 0, 10, 0}, {46, 27, 0, 10, 0},
    {-1, -1, 0, -1, 0}, {45, 29, 0, 10, 0}, {45, 30, 1, 10, 0}, {45, 31, 0, 10, 0},
    {45, 32, 0, 10, 0}, {46, 29, 0, 10, 0}, {-1, -1, 0, -1, 0}, {-128, -128, -128, -128, 0},
};
// clang-format on

// clang-format off
/** The sound-effect set cap_se_info names as chapter set 15. */
SND_SE_INFO cap15[14] = {
    {44, 108, 1, 10, 0}, {44, 109, 0, 10, 0}, {44, 110, 1, 10, 0}, {44, 111, 0, 10, 0},
    {44, 112, 0, 10, 0}, {44, 113, 0, 10, 0}, {44, 114, 0, 10, 0}, {44, 115, 0, 10, 0},
    {44, 116, 0, 10, 0}, {44, 117, 0, 10, 0}, {44, 118, 0, 10, 0}, {44, 119, 0, 10, 0},
    {-1, -1, 0, -1, 0}, {-128, -128, -128, -128, 0},
};
// clang-format on

// clang-format off
/** The sound-effect set cap_se_info names as chapter set 16. */
SND_SE_INFO cap16[92] = {
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {44, 23, 0, 10, 0},
    {44, 24, 0, 10, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {44, 74, 0, 10, 0}, {44, 75, 0, 10, 0},
    {44, 76, 0, 10, 0}, {44, 77, 1, 10, 0}, {44, 78, 0, 10, 0}, {44, 79, 0, 10, 0},
    {44, 80, 0, 10, 0}, {44, 81, 1, 10, 0}, {44, 82, 0, 10, 0}, {44, 83, 0, 10, 0},
    {44, 84, 1, 10, 0}, {44, 85, 0, 10, 0}, {-1, -1, 0, -1, 0}, {44, 87, 0, 10, 0},
    {44, 88, 0, 10, 0}, {44, 89, 0, 10, 0}, {-1, -1, 0, -1, 0}, {-128, -128, -128, -128, 0},
};
// clang-format on

// clang-format off
/** The sound-effect set cap_se_info names as chapter set 17. */
SND_SE_INFO cap17[11] = {
    {44, 100, 1, 10, 0}, {44, 101, 0, 10, 0}, {44, 102, 0, 10, 0}, {44, 103, 0, 10, 0},
    {44, 104, 0, 10, 0}, {44, 105, 0, 10, 0}, {44, 106, 0, 10, 0}, {44, 107, 0, 10, 0},
    {44, 108, 0, 10, 0}, {-1, -1, 0, -1, 0}, {-128, -128, -128, -128, 0},
};
// clang-format on

// clang-format off
/** The sound-effect set cap_se_info names as chapter set 20. */
SND_SE_INFO cap20[76] = {
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {44, 73, 0, 10, 0}, {-1, -1, 0, -1, 0}, {-128, -128, -128, -128, 0},
};
// clang-format on

// clang-format off
/** The sound-effect set cap_se_info names as chapter set 21. */
SND_SE_INFO cap21[2] = {
    {-1, -1, 0, -1, 0}, {-128, -128, -128, -128, 0},
};
// clang-format on

// clang-format off
/** The sound-effect set cap_se_info names as chapter set 22. */
SND_SE_INFO cap22[95] = {
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {44, 86, 0, 10, 0}, {44, 87, 0, 10, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {44, 90, 0, 10, 0}, {44, 91, 0, 10, 0},
    {44, 92, 0, 10, 0}, {-1, -1, 0, -1, 0}, {-128, -128, -128, -128, 0},
};
// clang-format on

// clang-format off
/** The sound-effect set cap_se_info names as chapter set 25. */
SND_SE_INFO cap25[29] = {
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {44, 25, 0, 10, 0}, {44, 26, 0, 10, 0}, {-1, -1, 0, -1, 0},
    {-128, -128, -128, -128, 0},
};
// clang-format on

// clang-format off
/** The sound-effect set cap_se_info names as chapter set 26. */
SND_SE_INFO cap26[2] = {
    {-1, -1, 0, -1, 0}, {-128, -128, -128, -128, 0},
};
// clang-format on

// clang-format off
/** The sound-effect set cap_se_info names as chapter set 27. */
SND_SE_INFO cap27[101] = {
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {44, 93, 0, 10, 0}, {44, 94, 0, 10, 0}, {44, 95, 0, 10, 0},
    {44, 96, 0, 10, 0}, {44, 97, 1, 10, 0}, {44, 98, 0, 10, 0}, {44, 99, 0, 10, 0},
    {-128, -128, -128, -128, 0},
};
// clang-format on

// clang-format off
/** The sound-effect set cap_se_info names as chapter set 30. */
SND_SE_INFO cap30[2] = {
    {-1, -1, 0, -1, 0}, {-128, -128, -128, -128, 0},
};
// clang-format on

// clang-format off
/** The sound-effect set cap_se_info names as chapter set 31. */
SND_SE_INFO cap31[15] = {
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {45, 112, 0, 10, 0}, {-1, -1, 0, -1, 0}, {-128, -128, -128, -128, 0},
};
// clang-format on

// clang-format off
/** The sound-effect set cap_se_info names as chapter set 32. */
SND_SE_INFO cap32[95] = {
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {44, 86, 0, 10, 0}, {44, 87, 0, 10, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {44, 90, 0, 10, 0}, {44, 91, 0, 10, 0},
    {44, 92, 0, 10, 0}, {-1, -1, 0, -1, 0}, {-128, -128, -128, -128, 0},
};
// clang-format on

// clang-format off
/** The sound-effect set cap_se_info names as chapter set 35. */
SND_SE_INFO cap35[2] = {
    {-1, -1, 0, -1, 0}, {-128, -128, -128, -128, 0},
};
// clang-format on

// clang-format off
/** The sound-effect set cap_se_info names as chapter set 36. */
SND_SE_INFO cap36[2] = {
    {-1, -1, 0, -1, 0}, {-128, -128, -128, -128, 0},
};
// clang-format on

// clang-format off
/** The sound-effect set cap_se_info names as chapter set 37. */
SND_SE_INFO cap37[95] = {
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {44, 86, 0, 10, 0}, {44, 87, 0, 10, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {44, 90, 0, 10, 0}, {44, 91, 0, 10, 0},
    {44, 92, 0, 10, 0}, {-1, -1, 0, -1, 0}, {-128, -128, -128, -128, 0},
};
// clang-format on

// clang-format off
/** The sound-effect set cap_se_info names as chapter set 40. */
SND_SE_INFO cap40[32] = {
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {46, 23, 0, 10, 0}, {46, 24, 1, 10, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {46, 29, 1, 10, 0}, {-1, -1, 0, -1, 0}, {-128, -128, -128, -128, 0},
};
// clang-format on

// clang-format off
/** The sound-effect set cap_se_info names as chapter set 41. */
SND_SE_INFO cap41[2] = {
    {-1, -1, 0, -1, 0}, {-128, -128, -128, -128, 0},
};
// clang-format on

// clang-format off
/** The sound-effect set cap_se_info names as chapter set 42. */
SND_SE_INFO cap42[95] = {
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {44, 86, 0, 10, 0}, {44, 87, 0, 10, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {44, 90, 0, 10, 0}, {44, 91, 0, 10, 0},
    {44, 92, 0, 10, 0}, {-1, -1, 0, -1, 0}, {-128, -128, -128, -128, 0},
};
// clang-format on

// clang-format off
/** The sound-effect set cap_se_info names as chapter set 45. */
SND_SE_INFO cap45[2] = {
    {-1, -1, 0, -1, 0}, {-128, -128, -128, -128, 0},
};
// clang-format on

// clang-format off
/** The sound-effect set cap_se_info names as chapter set 46. */
SND_SE_INFO cap46[2] = {
    {-1, -1, 0, -1, 0}, {-128, -128, -128, -128, 0},
};
// clang-format on

// clang-format off
/** The sound-effect set cap_se_info names as chapter set 47. */
SND_SE_INFO cap47[95] = {
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {44, 86, 0, 10, 0}, {44, 87, 0, 10, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {44, 90, 0, 10, 0}, {44, 91, 0, 10, 0},
    {44, 92, 0, 10, 0}, {-1, -1, 0, -1, 0}, {-128, -128, -128, -128, 0},
};
// clang-format on

// clang-format off
/** The sound-effect set cap_se_info names as chapter set 50. */
SND_SE_INFO cap50[43] = {
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {47, 75, 0, 10, 0},
    {47, 76, 0, 10, 0}, {47, 77, 0, 10, 0}, {47, 78, 0, 10, 0}, {47, 79, 0, 10, 0},
    {47, 80, 0, 10, 0}, {47, 81, 0, 10, 0}, {47, 82, 0, 10, 0}, {47, 83, 0, 10, 0},
    {47, 84, 0, 10, 0}, {47, 85, 0, 10, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {47, 88, 0, 10, 0}, {47, 89, 0, 10, 0}, {47, 90, 0, 10, 0}, {47, 91, 0, 10, 0},
    {47, 92, 0, 10, 0}, {47, 93, 0, 10, 0}, {47, 94, 0, 10, 0}, {-1, -1, 0, -1, 0},
    {47, 96, 0, 10, 0}, {47, 97, 0, 10, 0}, {-1, -1, 0, -1, 0}, {47, 99, 0, 10, 0},
    {47, 100, 1, 10, 0}, {-1, -1, 0, -1, 0}, {-128, -128, -128, -128, 0},
};
// clang-format on

// clang-format off
/** The sound-effect set cap_se_info names as chapter set 51. */
SND_SE_INFO cap51[75] = {
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {47, 64, 0, 10, 0}, {47, 65, 0, 10, 0}, {47, 66, 0, 10, 0}, {47, 67, 0, 10, 0},
    {47, 68, 0, 10, 0}, {47, 69, 0, 10, 0}, {47, 70, 1, 10, 0}, {47, 71, 0, 10, 0},
    {47, 72, 0, 10, 0}, {-1, -1, 0, -1, 0}, {-128, -128, -128, -128, 0},
};
// clang-format on

// clang-format off
/** The sound-effect set cap_se_info names as chapter set 52. */
SND_SE_INFO cap52[79] = {
    {47, 23, 0, 10, 0}, {47, 24, 1, 10, 0}, {47, 25, 0, 10, 0}, {47, 26, 0, 10, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {44, 74, 0, 10, 0}, {44, 75, 0, 10, 0},
    {44, 76, 0, 10, 0}, {-1, -1, 0, -1, 0}, {-128, -128, -128, -128, 0},
};
// clang-format on

// clang-format off
/** The sound-effect set cap_se_info names as chapter set 53. */
SND_SE_INFO cap53[22] = {
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {47, 105, 0, 10, 0},
    {47, 106, 0, 10, 0}, {47, 107, 0, 10, 0}, {47, 108, 0, 10, 0}, {47, 109, 0, 10, 0},
    {-1, -1, 0, -1, 0}, {-128, -128, -128, -128, 0},
};
// clang-format on

// clang-format off
/** The sound-effect set cap_se_info names as chapter set 54. */
SND_SE_INFO cap54[35] = {
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {47, 32, 0, 10, 0}, {-1, -1, 0, -1, 0}, {-128, -128, -128, -128, 0},
};
// clang-format on

// clang-format off
/** The sound-effect set cap_se_info names as chapter set 55. */
SND_SE_INFO cap55[47] = {
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {47, 33, 0, 10, 0}, {47, 34, 0, 10, 0}, {47, 35, 0, 10, 0},
    {47, 36, 0, 10, 0}, {47, 37, 0, 10, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {47, 42, 0, 10, 0}, {47, 43, 0, 10, 0},
    {47, 44, 0, 10, 0}, {-1, -1, 0, -1, 0}, {-128, -128, -128, -128, 0},
};
// clang-format on

// clang-format off
/** The sound-effect set cap_se_info names as chapter set 56. */
SND_SE_INFO cap56[55] = {
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {47, 33, 0, 10, 0}, {47, 34, 0, 10, 0}, {47, 35, 0, 10, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {47, 42, 0, 10, 0}, {47, 43, 0, 10, 0},
    {-1, -1, 0, -1, 0}, {47, 45, 0, 10, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {47, 48, 0, 10, 0}, {47, 49, 0, 10, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {47, 52, 0, 10, 0}, {-1, -1, 0, -1, 0}, {-128, -128, -128, -128, 0},
};
// clang-format on

// clang-format off
/** The sound-effect set cap_se_info names as chapter set 57. */
SND_SE_INFO cap57[79] = {
    {44, 108, 1, 10, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {47, 56, 0, 10, 0}, {47, 57, 1, 10, 0}, {47, 58, 0, 10, 0}, {47, 59, 0, 10, 0},
    {47, 60, 0, 10, 0}, {47, 61, 0, 10, 0}, {47, 62, 0, 10, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {44, 74, 0, 10, 0}, {44, 75, 0, 10, 0},
    {44, 76, 0, 10, 0}, {-1, -1, 0, -1, 0}, {-128, -128, -128, -128, 0},
};
// clang-format on

// clang-format off
/** The sound-effect set cap_se_info names as chapter set 58. */
SND_SE_INFO cap58[99] = {
    {44, 108, 1, 10, 0}, {-1, -1, 0, -1, 0}, {44, 110, 1, 10, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {44, 113, 0, 10, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {45, 45, 0, 10, 0}, {47, 25, 0, 10, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {45, 60, 0, 10, 0}, {-1, -1, 0, -1, 0}, {45, 62, 0, 10, 0}, {45, 63, 0, 10, 0},
    {45, 64, 0, 10, 0}, {45, 65, 0, 10, 0}, {45, 66, 0, 10, 0}, {-1, -1, 0, -1, 0},
    {45, 68, 0, 10, 0}, {45, 69, 0, 10, 0}, {45, 70, 0, 10, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {44, 74, 0, 10, 0}, {44, 75, 0, 10, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {44, 95, 0, 10, 0},
    {44, 96, 0, 10, 0}, {-1, -1, 0, -1, 0}, {-128, -128, -128, -128, 0},
};
// clang-format on

// clang-format off
/** The sound-effect set cap_se_info names as chapter set 59. */
SND_SE_INFO cap59[2] = {
    {-1, -1, 0, -1, 0}, {-128, -128, -128, -128, 0},
};
// clang-format on

// clang-format off
/** The sound-effect set cap_se_info names as chapter set 60. */
SND_SE_INFO cap60[2] = {
    {-1, -1, 0, -1, 0}, {-128, -128, -128, -128, 0},
};
// clang-format on

// clang-format off
/** The sound-effect set cap_se_info names as chapter set 61. */
SND_SE_INFO cap61[2] = {
    {-1, -1, 0, -1, 0}, {-128, -128, -128, -128, 0},
};
// clang-format on

// clang-format off
/** The sound-effect set cap_se_info names as chapter set 62. */
SND_SE_INFO cap62[2] = {
    {-1, -1, 0, -1, 0}, {-128, -128, -128, -128, 0},
};
// clang-format on

// clang-format off
/** The sound-effect set cap_se_info names as chapter set 63. */
SND_SE_INFO cap63[2] = {
    {-1, -1, 0, -1, 0}, {-128, -128, -128, -128, 0},
};
// clang-format on

// clang-format off
/** The sound-effect set cap_se_info names as chapter set 64. */
SND_SE_INFO cap64[2] = {
    {-1, -1, 0, -1, 0}, {-128, -128, -128, -128, 0},
};
// clang-format on

// clang-format off
/** The sound-effect set cap_se_info names as chapter set 65. */
SND_SE_INFO cap65[2] = {
    {-1, -1, 0, -1, 0}, {-128, -128, -128, -128, 0},
};
// clang-format on

// clang-format off
SND_SE_INFO *cap_se_info[101] = {
    cap0, cap1, cap2, cap3, cap4, 0, 0, 0,
    0, 0, cap10, cap11, cap12, cap13, cap14, cap15,
    cap16, cap17, 0, 0, cap20, cap21, cap22, 0,
    0, cap25, cap26, cap27, 0, 0, cap30, cap31,
    cap32, 0, 0, cap35, cap36, cap37, 0, 0,
    cap40, cap41, cap42, 0, 0, cap45, cap46, cap47,
    0, 0, cap50, cap51, cap52, cap53, cap54, cap55,
    cap56, cap57, cap58, cap59, cap60, cap61, cap62, cap63,
    cap64, cap65, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0,
};
// clang-format on

// clang-format off
/** The sound-effect set voice_info names as voice set 0. */
SND_SE_INFO voice0[54] = {
    {23, 20, 0, 11, 0}, {23, 21, 0, 11, 0}, {23, 22, 0, 11, 0}, {23, 23, 0, 11, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {23, 40, 0, 11, 0}, {23, 41, 0, 11, 0}, {23, 42, 0, 11, 0}, {23, 43, 0, 11, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {23, 50, 0, 11, 0}, {23, 51, 0, 11, 0},
    {23, 52, 0, 11, 0}, {23, 53, 0, 11, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {23, 60, 0, 11, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {23, 70, 0, 11, 0}, {23, 71, 0, 11, 0},
    {-1, -1, 0, -1, 0}, {-128, -128, -128, -128, 0},
};
// clang-format on

// clang-format off
/** The sound-effect set voice_info names as voice set 1. */
SND_SE_INFO voice1[54] = {
    {24, 20, 0, 11, 0}, {24, 21, 0, 11, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {24, 40, 0, 11, 0}, {24, 41, 0, 11, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {24, 50, 0, 11, 0}, {24, 51, 0, 11, 0},
    {24, 52, 0, 11, 0}, {24, 53, 0, 11, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {24, 60, 0, 11, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {24, 70, 0, 11, 0}, {24, 71, 0, 11, 0},
    {-1, -1, 0, -1, 0}, {-128, -128, -128, -128, 0},
};
// clang-format on

// clang-format off
/** The sound-effect set voice_info names as voice set 2. */
SND_SE_INFO voice2[54] = {
    {25, 20, 0, 11, 0}, {-1, -1, 0, -1, 0}, {25, 22, 0, 11, 0}, {25, 23, 0, 11, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {25, 40, 0, 11, 0}, {25, 41, 0, 11, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {25, 50, 0, 11, 0}, {25, 51, 0, 11, 0},
    {25, 52, 0, 11, 0}, {25, 53, 0, 11, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {25, 60, 0, 11, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {25, 70, 0, 11, 0}, {25, 71, 0, 11, 0},
    {-1, -1, 0, -1, 0}, {-128, -128, -128, -128, 0},
};
// clang-format on

// clang-format off
/** The sound-effect set voice_info names as voice set 3. */
SND_SE_INFO voice3[54] = {
    {26, 20, 0, 11, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {26, 23, 1, 11, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {26, 40, 0, 11, 0}, {26, 41, 0, 11, 0}, {26, 42, 0, 11, 0}, {26, 43, 0, 11, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {26, 50, 0, 11, 0}, {26, 51, 0, 11, 0},
    {26, 52, 0, 11, 0}, {26, 53, 0, 11, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {26, 60, 0, 11, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {26, 70, 0, 11, 0}, {26, 71, 0, 11, 0},
    {-1, -1, 0, -1, 0}, {-128, -128, -128, -128, 0},
};
// clang-format on

// clang-format off
/** The sound-effect set voice_info names as voice set 4. */
SND_SE_INFO voice4[54] = {
    {27, 20, 0, 11, 0}, {27, 21, 0, 11, 0}, {27, 22, 1, 11, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {27, 40, 0, 11, 0}, {27, 41, 0, 11, 0}, {27, 42, 0, 11, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {27, 50, 0, 11, 0}, {27, 51, 0, 11, 0},
    {27, 52, 0, 11, 0}, {27, 53, 0, 11, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {27, 60, 0, 11, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {27, 70, 0, 11, 0}, {27, 71, 0, 11, 0},
    {-1, -1, 0, -1, 0}, {-128, -128, -128, -128, 0},
};
// clang-format on

// clang-format off
/** The sound-effect set voice_info names as voice set 5. */
SND_SE_INFO voice5[64] = {
    {28, 20, 1, 11, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {28, 24, 1, 11, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {28, 29, 0, 11, 0}, {28, 30, 0, 11, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {28, 33, 0, 11, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {28, 40, 0, 11, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {28, 50, 0, 11, 0}, {28, 51, 0, 11, 0},
    {28, 52, 0, 11, 0}, {28, 53, 0, 11, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {28, 60, 0, 11, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {28, 70, 0, 11, 0}, {28, 71, 0, 11, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {28, 80, 1, 11, 0}, {28, 81, 0, 11, 0}, {-1, -1, 0, -1, 0}, {-128, -128, -128, -128, 0},
};
// clang-format on

// clang-format off
/** The sound-effect set voice_info names as voice set 6. */
SND_SE_INFO voice6[7] = {
    {29, 20, 0, 11, 0}, {29, 21, 0, 11, 0}, {29, 22, 0, 11, 0}, {29, 23, 0, 11, 0},
    {29, 24, 0, 11, 0}, {-1, -1, 0, -1, 0}, {-128, -128, -128, -128, 0},
};
// clang-format on

// clang-format off
SND_SE_INFO *voice_info[11] = {
    voice0, voice1, voice2, voice3, voice4, voice5, voice6, 0,
    0, 0, 0,
};
// clang-format on

// clang-format off
SND_SE_INFO special_se_info[65] = {
    {-1, -1, 0, -1, 0}, {124, 20, 0, 12, 0}, {124, 21, 0, 12, 0}, {124, 22, 0, 12, 0},
    {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0}, {124, 25, 0, 12, 0}, {-1, -1, 0, -1, 0},
    {124, 27, 0, 12, 0}, {124, 28, 0, 12, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {124, 31, 0, 12, 0}, {124, 32, 0, 12, 0}, {124, 33, 0, 12, 0}, {124, 34, 0, 12, 0},
    {124, 35, 0, 12, 0}, {124, 36, 0, 12, 0}, {124, 37, 0, 12, 0}, {124, 38, 0, 12, 0},
    {124, 39, 0, 12, 0}, {124, 40, 0, 12, 0}, {-1, -1, 0, -1, 0}, {-1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0}, {124, 44, 0, 12, 0}, {124, 45, 0, 12, 0}, {124, 46, 0, 12, 0},
    {124, 47, 0, 12, 0}, {124, 48, 0, 12, 0}, {124, 49, 0, 12, 0}, {124, 50, 0, 12, 0},
    {124, 51, 0, 12, 0}, {124, 52, 0, 12, 0}, {124, 53, 1, 12, 0}, {124, 54, 0, 12, 0},
    {124, 55, 0, 12, 0}, {124, 56, 1, 12, 0}, {124, 57, 0, 12, 0}, {124, 58, 0, 12, 0},
    {124, 59, 0, 12, 0}, {124, 60, 0, 12, 0}, {-1, -1, 0, -1, 0}, {124, 62, 0, 12, 0},
    {-1, -1, 0, -1, 0}, {124, 64, 0, 12, 0}, {124, 65, 0, 12, 0}, {124, 66, 0, 12, 0},
    {124, 67, 0, 12, 0}, {-1, -1, 0, -1, 0}, {124, 69, 0, 12, 0}, {124, 70, 0, 12, 0},
    {124, 71, 0, 12, 0}, {124, 72, 0, 12, 0}, {124, 73, 0, 12, 0}, {124, 74, 0, 12, 0},
    {124, 75, 0, 12, 0}, {124, 76, 0, 12, 0}, {124, 77, 0, 12, 0}, {124, 78, 0, 12, 0},
    {124, 79, 0, 12, 0}, {124, 80, 1, 12, 0}, {124, 81, 1, 12, 0}, {-1, -1, 0, -1, 0},
    {-128, -128, -128, -128, 0},
};
// clang-format on

TAG_PARAM Command__3[2] = {
    {"REVERBE", {1, 1, -1}},
    {"TABLE",   {1, 1, -1}},
};

int se_list;

SND_INFO *SoundInfo;

/**
 * Reads one sound configuration file through the script interpreter.
 *
 * @mangled LoadSoundInfo__FP8SND_INFOPci
 * @address 0x15BAB0
 * @size 0xF4
 */
void LoadSoundInfo(SND_INFO *info, char *script, int script_size) {
    u8 *clear;
    u8 *data = (u8 *) script;
    clear = (u8 *) info;
    memset(info, 0, sizeof(SND_INFO));

    for (u_int i = 0; i < sizeof(SND_INFO); i++) {
        *clear++ = 0;
    }

    se_list = 0;
    SoundInfo = info;
    CScriptInterpreter interpreter;
    interpreter.SetScript((char *) data, script_size);
    interpreter.SetTAG((TAG_PARAM *) Command__3, 2);

    for (;;) {
        int tag = interpreter.GetNextTAG();

        if (tag < 0) {
            break;
        }

        CommandExe__3[tag](interpreter.arguments);
    }
}

static void CommandREVERBE(void **arguments) {
    SoundInfo->reverb_mode = *(s32 *) arguments[0];
    SoundInfo->reverb_depth = *(s32 *) arguments[1];
}

static void CommandTABLE(void **arguments) {
    SoundInfo->se_table = *(s32 *) arguments[0];
    SoundInfo->se_table_type = *(s32 *) arguments[1];
}

void setbilinear(int on) {
    linear__2 = on;
}

void setAlphaFlag(sceVif1Packet *packet, sceGsAlpha *alpha) {
    sceGifTag tag;

    *(u_long128 *) &tag = 0;
    tag.EOP = 1;
    tag.NREG = 1;
    tag.REGS0 = SCE_GIF_PACKED_AD;

    sceVif1PkCnt(packet, 0);
    sceVif1PkOpenDirectCode(packet, 0);
    sceVif1PkOpenGifTag(packet, *(u_long128 *) &tag);
    sceVif1PkAddGsAD(packet, SCE_GS_ALPHA_1, *(u_long *) alpha);
    sceVif1PkCloseGifTag(packet);
    sceVif1PkCloseDirectCode(packet);
}

void set2DSprite(sceVif1Packet *packet, CTexture *texture, const CRect_i_ &screen, int u, int v) {
    sceGsTest test;
    sceGsZbuf zbuf;
    float     q;

    if (texture == 0) {
        return;
    }

    q = 1.0f;
    sceVif1PkCnt(packet, 0);
    sceVif1PkOpenDirectCode(packet, 0);
    sceVif1PkOpenGifTag(packet, *(u_long128 *) &GiftagAD);
    sceVif1PkAddGsAD(packet, SCE_GS_TEX1_1, ((u_long) linear__2 << 5) | 0x41);
    sceVif1PkAddGsAD(packet, SCE_GS_PRIM, SCE_GS_SET_PRIM(SCE_GS_PRIM_SPRITE, 0, 1, 0, 1, 0, 1, 0, 0));
    test = mgPixelTest;
    test.bits.ate = 0;
    test.bits.aref = 0;
    test.bits.atst = SCE_GS_ALWAYS;
    test.bits.zte = 1;
    test.bits.ztst = SCE_GS_ALWAYS;
    sceVif1PkAddGsAD(packet, SCE_GS_TEST_1, *(u_long *) &test);
    zbuf = mgZBuffer;
    zbuf.bits.zmsk = 1;
    sceVif1PkAddGsAD(packet, SCE_GS_ZBUF_1, *(u_long *) &zbuf);
    sceVif1PkAddGsAD(packet, SCE_GS_RGBAQ, SCE_GS_SET_RGBAQ(0x80, 0x80, 0x80, 0x80, *(u_int *) &q));
    sceVif1PkAddGsAD(packet, SCE_GS_TEX0_1, texture->tex0);
    sceVif1PkAddGsAD(packet, SCE_GS_UV, SCE_GS_SET_UV(u << 4, v << 4));
    sceVif1PkAddGsAD(packet, SCE_GS_XYZF2, SCE_GS_SET_XYZF2((screen.x << 4) + 27648, (screen.y << 3) + GS_Y_OFFSET, 0, 0));
    sceVif1PkAddGsAD(packet, SCE_GS_UV, SCE_GS_SET_UV((u + screen.width) << 4, (v + screen.height) << 4));
    sceVif1PkAddGsAD(packet, SCE_GS_XYZF2, SCE_GS_SET_XYZF2(((screen.x + screen.width) << 4) + 27647, ((screen.y + screen.height) << 3) + GS_Y_OFFSET, 0, 0));
    sceVif1PkAddGsAD(packet, SCE_GS_TEST_1, *(u_long *) &mgPixelTest);
    sceVif1PkAddGsAD(packet, SCE_GS_ZBUF_1, *(u_long *) &mgZBuffer);
    sceVif1PkCloseGifTag(packet);
    sceVif1PkCloseDirectCode(packet);
}

void set2DSprite(sceVif1Packet *packet, CTexture *texture, const CRect_i_ &screen, const CRect_i_ &texel) {
    sceGsTest test;
    sceGsZbuf zbuf;
    float     q;

    if (texture == 0) {
        return;
    }

    q = 1.0f;
    sceVif1PkCnt(packet, 0);
    sceVif1PkOpenDirectCode(packet, 0);
    sceVif1PkOpenGifTag(packet, *(u_long128 *) &GiftagAD);
    sceVif1PkAddGsAD(packet, SCE_GS_TEX1_1, ((u_long) linear__2 << 5) | 0x41);
    sceVif1PkAddGsAD(packet, SCE_GS_PRIM, SCE_GS_SET_PRIM(SCE_GS_PRIM_SPRITE, 0, 1, 0, 1, 1, 1, 0, 0));
    test = mgPixelTest;
    test.bits.ate = 0;
    test.bits.aref = 0;
    test.bits.atst = SCE_GS_ALWAYS;
    test.bits.zte = 1;
    test.bits.ztst = SCE_GS_ALWAYS;
    sceVif1PkAddGsAD(packet, SCE_GS_TEST_1, *(u_long *) &test);
    zbuf = mgZBuffer;
    zbuf.bits.zmsk = 1;
    sceVif1PkAddGsAD(packet, SCE_GS_ZBUF_1, *(u_long *) &zbuf);
    sceVif1PkAddGsAD(packet, SCE_GS_RGBAQ, SCE_GS_SET_RGBAQ(0x80, 0x80, 0x80, 0x80, *(u_int *) &q));
    sceVif1PkAddGsAD(packet, SCE_GS_TEX0_1, texture->tex0);
    sceVif1PkAddGsAD(packet, SCE_GS_UV, SCE_GS_SET_UV(texel.x << 4, texel.y << 4));
    sceVif1PkAddGsAD(packet, SCE_GS_XYZF2, SCE_GS_SET_XYZF2((screen.x << 4) + 27648, (screen.y << 3) + GS_Y_OFFSET, 0, 0));
    sceVif1PkAddGsAD(packet, SCE_GS_UV, SCE_GS_SET_UV((texel.x + texel.width) << 4, (texel.y + texel.height) << 4));
    sceVif1PkAddGsAD(packet, SCE_GS_XYZF2, SCE_GS_SET_XYZF2(((screen.x + screen.width) << 4) + 27647, ((screen.y + screen.height) << 3) + GS_Y_OFFSET, 0, 0));
    sceVif1PkAddGsAD(packet, SCE_GS_TEST_1, *(u_long *) &mgPixelTest);
    sceVif1PkAddGsAD(packet, SCE_GS_ZBUF_1, *(u_long *) &mgZBuffer);
    sceVif1PkCloseGifTag(packet);
    sceVif1PkCloseDirectCode(packet);
}

void set2DSprite(sceVif1Packet *packet, CTexture *texture, const CRect_i_ &screen, const CRect_i_ &texel, unsigned char alpha) {
    sceGsTest test;
    sceGsZbuf zbuf;
    float     q;

    if (texture == 0) {
        return;
    }

    q = 1.0f;
    sceVif1PkCnt(packet, 0);
    sceVif1PkOpenDirectCode(packet, 0);
    sceVif1PkOpenGifTag(packet, *(u_long128 *) &GiftagAD);
    sceVif1PkAddGsAD(packet, SCE_GS_TEX1_1, ((u_long) linear__2 << 5) | 0x41);
    sceVif1PkAddGsAD(packet, SCE_GS_PRIM, SCE_GS_SET_PRIM(SCE_GS_PRIM_SPRITE, 0, 1, 0, 1, 1, 1, 0, 0));
    test = mgPixelTest;
    test.bits.ate = 0;
    test.bits.aref = 0;
    test.bits.atst = SCE_GS_ALWAYS;
    test.bits.zte = 1;
    test.bits.ztst = SCE_GS_ALWAYS;
    sceVif1PkAddGsAD(packet, SCE_GS_TEST_1, *(u_long *) &test);
    zbuf = mgZBuffer;
    zbuf.bits.zmsk = 1;
    sceVif1PkAddGsAD(packet, SCE_GS_ZBUF_1, *(u_long *) &zbuf);
    sceVif1PkAddGsAD(packet, SCE_GS_RGBAQ, SCE_GS_SET_RGBAQ(0x80, 0x80, 0x80, alpha, *(u_int *) &q));
    sceVif1PkAddGsAD(packet, SCE_GS_TEX0_1, texture->tex0);
    sceVif1PkAddGsAD(packet, SCE_GS_UV, SCE_GS_SET_UV(texel.x << 4, texel.y << 4));
    sceVif1PkAddGsAD(packet, SCE_GS_XYZF2, SCE_GS_SET_XYZF2((screen.x << 4) + 27648, (screen.y << 3) + GS_Y_OFFSET, 0, 0));
    sceVif1PkAddGsAD(packet, SCE_GS_UV, SCE_GS_SET_UV((texel.x + texel.width) << 4, (texel.y + texel.height) << 4));
    sceVif1PkAddGsAD(packet, SCE_GS_XYZF2, SCE_GS_SET_XYZF2(((screen.x + screen.width) << 4) + 27647, ((screen.y + screen.height) << 3) + GS_Y_OFFSET, 0, 0));
    sceVif1PkAddGsAD(packet, SCE_GS_TEST_1, *(u_long *) &mgPixelTest);
    sceVif1PkAddGsAD(packet, SCE_GS_ZBUF_1, *(u_long *) &mgZBuffer);
    sceVif1PkCloseGifTag(packet);
    sceVif1PkCloseDirectCode(packet);
}

void set2DSprite(sceVif1Packet *packet, CTexture *texture, const CRect_i_ &screen, const CRect_i_ &texel, unsigned char red, unsigned char green, unsigned char blue, unsigned char alpha) {
    sceGsTest test;
    sceGsZbuf zbuf;
    float     q;

    if (texture == 0) {
        return;
    }

    q = 1.0f;
    sceVif1PkCnt(packet, 0);
    sceVif1PkOpenDirectCode(packet, 0);
    sceVif1PkOpenGifTag(packet, *(u_long128 *) &GiftagAD);
    sceVif1PkAddGsAD(packet, SCE_GS_TEX1_1, ((u_long) linear__2 << 5) | 0x41);
    sceVif1PkAddGsAD(packet, SCE_GS_PRIM, SCE_GS_SET_PRIM(SCE_GS_PRIM_SPRITE, 0, 1, 0, 1, 1, 1, 0, 0));
    test = mgPixelTest;
    test.bits.ate = 0;
    test.bits.aref = 0;
    test.bits.atst = SCE_GS_ALWAYS;
    test.bits.zte = 1;
    test.bits.ztst = SCE_GS_ALWAYS;
    sceVif1PkAddGsAD(packet, SCE_GS_TEST_1, *(u_long *) &test);
    zbuf = mgZBuffer;
    zbuf.bits.zmsk = 1;
    sceVif1PkAddGsAD(packet, SCE_GS_ZBUF_1, *(u_long *) &zbuf);
    sceVif1PkAddGsAD(packet, SCE_GS_RGBAQ, SCE_GS_SET_RGBAQ(red, green, blue, alpha, *(u_int *) &q));
    sceVif1PkAddGsAD(packet, SCE_GS_TEX0_1, texture->tex0);
    sceVif1PkAddGsAD(packet, SCE_GS_UV, SCE_GS_SET_UV(texel.x << 4, texel.y << 4));
    sceVif1PkAddGsAD(packet, SCE_GS_XYZF2, SCE_GS_SET_XYZF2((screen.x << 4) + 27648, (screen.y << 3) + GS_Y_OFFSET, 0, 0));
    sceVif1PkAddGsAD(packet, SCE_GS_UV, SCE_GS_SET_UV((texel.x + texel.width) << 4, (texel.y + texel.height) << 4));
    sceVif1PkAddGsAD(packet, SCE_GS_XYZF2, SCE_GS_SET_XYZF2(((screen.x + screen.width) << 4) + 27647, ((screen.y + screen.height) << 3) + GS_Y_OFFSET, 0, 0));
    sceVif1PkAddGsAD(packet, SCE_GS_TEST_1, *(u_long *) &mgPixelTest);
    sceVif1PkAddGsAD(packet, SCE_GS_ZBUF_1, *(u_long *) &mgZBuffer);
    sceVif1PkCloseGifTag(packet);
    sceVif1PkCloseDirectCode(packet);
}

void set2DSprite(sceVif1Packet *packet, CTexture *texture, const CRect_i_ &screen, const CRect_i_ &texel, spRGBA *top_left, spRGBA *top_right, spRGBA *bottom_left, spRGBA *bottom_right, int mode) {
    sceGsTest test;
    sceGsZbuf zbuf;
    float     q;

    if (texture == 0) {
        return;
    }

    q = 1.0f;
    sceVif1PkCnt(packet, 0);
    sceVif1PkOpenDirectCode(packet, 0);
    sceVif1PkOpenGifTag(packet, *(u_long128 *) &GiftagAD);
    sceVif1PkAddGsAD(packet, SCE_GS_TEX1_1, ((u_long) linear__2 << 5) | 0x41);
    sceVif1PkAddGsAD(packet, SCE_GS_PRIM, SCE_GS_SET_PRIM(4, 1, 1, 0, 1, 0, 1, 0, 0));
    test = mgPixelTest;
    test.bits.ate = 0;
    test.bits.aref = 0;
    test.bits.atst = SCE_GS_ALWAYS;
    test.bits.zte = 1;
    test.bits.ztst = SCE_GS_ALWAYS;
    sceVif1PkAddGsAD(packet, SCE_GS_TEST_1, *(u_long *) &test);
    zbuf = mgZBuffer;
    zbuf.bits.zmsk = 1;
    sceVif1PkAddGsAD(packet, SCE_GS_ZBUF_1, *(u_long *) &zbuf);
    sceVif1PkAddGsAD(packet, SCE_GS_TEX0_1, texture->tex0);

    if (mode != 0) {
        sceVif1PkAddGsAD(packet, SCE_GS_RGBAQ, SCE_GS_SET_RGBAQ(top_left->r, top_left->g, top_left->b, top_left->a, *(u_int *) &q));
        sceVif1PkAddGsAD(packet, SCE_GS_UV, SCE_GS_SET_UV(texel.x << 4, texel.y << 4));
        sceVif1PkAddGsAD(packet, SCE_GS_XYZF2, SCE_GS_SET_XYZF2((screen.x << 4) + 27648, (screen.y << 3) + GS_Y_OFFSET, 0, 0));
        sceVif1PkAddGsAD(packet, SCE_GS_RGBAQ, SCE_GS_SET_RGBAQ(top_right->r, top_right->g, top_right->b, top_right->a, *(u_int *) &q));
        sceVif1PkAddGsAD(packet, SCE_GS_UV, SCE_GS_SET_UV((texel.x + texel.width) << 4, texel.y << 4));
        sceVif1PkAddGsAD(packet, SCE_GS_XYZF2, SCE_GS_SET_XYZF2(((screen.x + screen.width) << 4) + 27647, (screen.y << 3) + GS_Y_OFFSET, 0, 0));
        sceVif1PkAddGsAD(packet, SCE_GS_RGBAQ, SCE_GS_SET_RGBAQ(bottom_left->r, bottom_left->g, bottom_left->b, bottom_left->a, *(u_int *) &q));
        sceVif1PkAddGsAD(packet, SCE_GS_UV, SCE_GS_SET_UV(texel.x << 4, (texel.y + texel.height) << 4));
        sceVif1PkAddGsAD(packet, SCE_GS_XYZF2, SCE_GS_SET_XYZF2((screen.x << 4) + 27648, ((screen.y + screen.height) << 3) + GS_Y_OFFSET, 0, 0));
        sceVif1PkAddGsAD(packet, SCE_GS_RGBAQ, SCE_GS_SET_RGBAQ(bottom_right->r, bottom_right->g, bottom_right->b, bottom_right->a, *(u_int *) &q));
        sceVif1PkAddGsAD(packet, SCE_GS_UV, SCE_GS_SET_UV((texel.x + texel.width) << 4, (texel.y + texel.height) << 4));
        sceVif1PkAddGsAD(packet, SCE_GS_XYZF2, SCE_GS_SET_XYZF2(((screen.x + screen.width) << 4) + 27647, ((screen.y + screen.height) << 3) + GS_Y_OFFSET, 0, 0));
    } else {
        sceVif1PkAddGsAD(packet, SCE_GS_RGBAQ, SCE_GS_SET_RGBAQ(top_right->r, top_right->g, top_right->b, top_right->a, *(u_int *) &q));
        sceVif1PkAddGsAD(packet, SCE_GS_UV, SCE_GS_SET_UV((texel.x + texel.width) << 4, texel.y << 4));
        sceVif1PkAddGsAD(packet, SCE_GS_XYZF2, SCE_GS_SET_XYZF2(((screen.x + screen.width) << 4) + 27647, (screen.y << 3) + GS_Y_OFFSET, 0, 0));
        sceVif1PkAddGsAD(packet, SCE_GS_RGBAQ, SCE_GS_SET_RGBAQ(top_left->r, top_left->g, top_left->b, top_left->a, *(u_int *) &q));
        sceVif1PkAddGsAD(packet, SCE_GS_UV, SCE_GS_SET_UV(texel.x << 4, texel.y << 4));
        sceVif1PkAddGsAD(packet, SCE_GS_XYZF2, SCE_GS_SET_XYZF2((screen.x << 4) + 27648, (screen.y << 3) + GS_Y_OFFSET, 0, 0));
        sceVif1PkAddGsAD(packet, SCE_GS_RGBAQ, SCE_GS_SET_RGBAQ(bottom_right->r, bottom_right->g, bottom_right->b, bottom_right->a, *(u_int *) &q));
        sceVif1PkAddGsAD(packet, SCE_GS_UV, SCE_GS_SET_UV((texel.x + texel.width) << 4, (texel.y + texel.height) << 4));
        sceVif1PkAddGsAD(packet, SCE_GS_XYZF2, SCE_GS_SET_XYZF2(((screen.x + screen.width) << 4) + 27647, ((screen.y + screen.height) << 3) + GS_Y_OFFSET, 0, 0));
        sceVif1PkAddGsAD(packet, SCE_GS_RGBAQ, SCE_GS_SET_RGBAQ(bottom_left->r, bottom_left->g, bottom_left->b, bottom_left->a, *(u_int *) &q));
        sceVif1PkAddGsAD(packet, SCE_GS_UV, SCE_GS_SET_UV(texel.x << 4, (texel.y + texel.height) << 4));
        sceVif1PkAddGsAD(packet, SCE_GS_XYZF2, SCE_GS_SET_XYZF2((screen.x << 4) + 27648, ((screen.y + screen.height) << 3) + GS_Y_OFFSET, 0, 0));
    }

    sceVif1PkAddGsAD(packet, SCE_GS_TEST_1, *(u_long *) &mgPixelTest);
    sceVif1PkAddGsAD(packet, SCE_GS_ZBUF_1, *(u_long *) &mgZBuffer);
    sceVif1PkCloseGifTag(packet);
    sceVif1PkCloseDirectCode(packet);
}

void set3DColSprite(sceVif1Packet *packet, int *top_left, int *top_right, int *bottom_left, int *bottom_right, spRGBA *top_left_colour, spRGBA *top_right_colour, spRGBA *bottom_left_colour, spRGBA *bottom_right_colour) {
    float q = 1.0f;

    sceVif1PkCnt(packet, 0);
    sceVif1PkOpenDirectCode(packet, 0);
    sceVif1PkOpenGifTag(packet, *(u_long128 *) &GiftagAD);
    sceVif1PkAddGsAD(packet, SCE_GS_PRIM, SCE_GS_SET_PRIM(4, 1, 0, 0, 1, 0, 1, 0, 0));
    sceVif1PkAddGsAD(packet, SCE_GS_RGBAQ, SCE_GS_SET_RGBAQ(top_left_colour->r, top_left_colour->g, top_left_colour->b, top_left_colour->a, *(u_int *) &q));
    sceVif1PkAddGsAD(packet, SCE_GS_XYZF2, SCE_GS_SET_XYZF2(top_left[0], top_left[1], top_left[2], 0));
    sceVif1PkAddGsAD(packet, SCE_GS_RGBAQ, SCE_GS_SET_RGBAQ(top_right_colour->r, top_right_colour->g, top_right_colour->b, top_right_colour->a, *(u_int *) &q));
    sceVif1PkAddGsAD(packet, SCE_GS_XYZF2, SCE_GS_SET_XYZF2(top_right[0], top_right[1], top_right[2], 0));
    sceVif1PkAddGsAD(packet, SCE_GS_RGBAQ, SCE_GS_SET_RGBAQ(bottom_left_colour->r, bottom_left_colour->g, bottom_left_colour->b, bottom_left_colour->a, *(u_int *) &q));
    sceVif1PkAddGsAD(packet, SCE_GS_XYZF2, SCE_GS_SET_XYZF2(bottom_left[0], bottom_left[1], bottom_left[2], 0));
    sceVif1PkAddGsAD(packet, SCE_GS_RGBAQ, SCE_GS_SET_RGBAQ(bottom_right_colour->r, bottom_right_colour->g, bottom_right_colour->b, bottom_right_colour->a, *(u_int *) &q));
    sceVif1PkAddGsAD(packet, SCE_GS_XYZF2, SCE_GS_SET_XYZF2(bottom_right[0], bottom_right[1], bottom_right[2], 0));
    sceVif1PkCloseGifTag(packet);
    sceVif1PkCloseDirectCode(packet);
}

void set3DSprite(sceVif1Packet *packet, CTexture *texture, const CRect_i_ &source, int *top_left, int *top_right, int *bottom_left, int *bottom_right, unsigned char alpha) {
    spRGBA colour = {0x80, 0x80, 0x80, 0};

    colour.a = alpha;
    set3DSprite(packet, texture, source, top_left, top_right, bottom_left, bottom_right, &colour);
}

/**
 * Draws a textured sprite in world space, with four corner positions and colours.
 *
 * @mangled set3DSprite__FP13sceVif1PacketP8CTextureRC8CRect_i_PiPiPiPiP6spRGBA
 * @address 0x15D4B0
 * @size 0x2E0
 */
void set3DSprite(sceVif1Packet *packet, CTexture *texture, const CRect_i_ &source, int *top_left, int *top_right, int *bottom_left, int *bottom_right, spRGBA *colour) {
    float q;

    if (texture == 0) {
        return;
    }

    q = 1.0f;
    sceVif1PkCnt(packet, 0);
    sceVif1PkOpenDirectCode(packet, 0);
    sceVif1PkOpenGifTag(packet, *(u_long128 *) &GiftagAD);
    sceVif1PkAddGsAD(packet, SCE_GS_TEX1_1, SCE_GS_SET_TEX1(1, 0, 1, 1, 0, 0, 0));
    sceVif1PkAddGsAD(packet, SCE_GS_PRIM, SCE_GS_SET_PRIM(4, 0, 1, 0, 1, 0, 1, 0, 0));
    sceVif1PkAddGsAD(packet, SCE_GS_RGBAQ, SCE_GS_SET_RGBAQ(colour->r, colour->g, colour->b, colour->a, *(u_int *) &q));
    sceVif1PkAddGsAD(packet, SCE_GS_TEX0_1, texture->tex0);
    sceVif1PkAddGsAD(packet, SCE_GS_UV, SCE_GS_SET_UV(source.x << 4, source.y << 4));
    sceVif1PkAddGsAD(packet, SCE_GS_XYZF2, SCE_GS_SET_XYZF2(top_left[0], top_left[1], top_left[2], 0));
    sceVif1PkAddGsAD(packet, SCE_GS_UV, SCE_GS_SET_UV((source.x + source.width) << 4, source.y << 4));
    sceVif1PkAddGsAD(packet, SCE_GS_XYZF2, SCE_GS_SET_XYZF2(top_right[0], top_right[1], top_right[2], 0));
    sceVif1PkAddGsAD(packet, SCE_GS_UV, SCE_GS_SET_UV(source.x << 4, (source.y + source.height) << 4));
    sceVif1PkAddGsAD(packet, SCE_GS_XYZF2, SCE_GS_SET_XYZF2(bottom_left[0], bottom_left[1], bottom_left[2], 0));
    sceVif1PkAddGsAD(packet, SCE_GS_UV, SCE_GS_SET_UV((source.x + source.width) << 4, (source.y + source.height) << 4));
    sceVif1PkAddGsAD(packet, SCE_GS_XYZF2, SCE_GS_SET_XYZF2(bottom_right[0], bottom_right[1], bottom_right[2], 0));
    sceVif1PkAddGsAD(packet, SCE_GS_TEST_1, *(u_long *) &mgPixelTest);
    sceVif1PkCloseGifTag(packet);
    sceVif1PkCloseDirectCode(packet);
}

/**
 * Draws a textured sprite in world space between two projected corners.
 *
 * @mangled set3DSprite__FP13sceVif1PacketP8CTextureRC8CRect_i_PiPiP6spRGBA
 * @address 0x15D790
 * @size 0x210
 */
void set3DSprite(sceVif1Packet *packet, CTexture *texture, const CRect_i_ &source, int *top_left, int *bottom_right, spRGBA *colour) {
    float q;

    if (texture == 0) {
        return;
    }

    q = 1.0f;
    sceVif1PkCnt(packet, 0);
    sceVif1PkOpenDirectCode(packet, 0);
    sceVif1PkOpenGifTag(packet, *(u_long128 *) &GiftagAD);
    sceVif1PkAddGsAD(packet, SCE_GS_TEX1_1, SCE_GS_SET_TEX1(1, 0, 1, 1, 0, 0, 0));
    sceVif1PkAddGsAD(packet, SCE_GS_PRIM, SCE_GS_SET_PRIM(SCE_GS_PRIM_SPRITE, 0, 1, 0, 1, 0, 1, 0, 0));
    sceVif1PkAddGsAD(packet, SCE_GS_RGBAQ, SCE_GS_SET_RGBAQ(colour->r, colour->g, colour->b, colour->a, *(u_int *) &q));
    sceVif1PkAddGsAD(packet, SCE_GS_TEX0_1, texture->tex0);
    sceVif1PkAddGsAD(packet, SCE_GS_UV, SCE_GS_SET_UV(source.x << 4, source.y << 4));
    sceVif1PkAddGsAD(packet, SCE_GS_XYZF2, SCE_GS_SET_XYZF2(top_left[0], top_left[1], top_left[2], 0));
    sceVif1PkAddGsAD(packet, SCE_GS_UV, SCE_GS_SET_UV((source.x + source.width) << 4, (source.y + source.height) << 4));
    sceVif1PkAddGsAD(packet, SCE_GS_XYZF2, SCE_GS_SET_XYZF2(bottom_right[0], bottom_right[1], bottom_right[2], 0));
    sceVif1PkAddGsAD(packet, SCE_GS_TEST_1, *(u_long *) &mgPixelTest);
    sceVif1PkCloseGifTag(packet);
    sceVif1PkCloseDirectCode(packet);
}

/**
 * Draws a fogged sprite in world space, with four corner positions.
 *
 * @mangled set3DSpriteFog__FP13sceVif1PacketP8CTextureRC8CRect_i_PiPiPiPiUc
 * @address 0x15D9A0
 * @size 0x2FC
 */
void set3DSpriteFog(sceVif1Packet *packet, CTexture *texture, const CRect_i_ &source, int *top_left, int *top_right, int *bottom_left, int *bottom_right, unsigned char alpha) {
    float q;

    if (texture == 0) {
        return;
    }

    q = 1.0f;
    sceVif1PkCnt(packet, 0);
    sceVif1PkOpenDirectCode(packet, 0);
    sceVif1PkOpenGifTag(packet, *(u_long128 *) &GiftagAD);
    sceVif1PkAddGsAD(packet, SCE_GS_TEX1_1, SCE_GS_SET_TEX1(1, 0, 1, 1, 0, 0, 0));
    sceVif1PkAddGsAD(packet, SCE_GS_PRIM, SCE_GS_SET_PRIM(4, 0, 1, 1, 1, 0, 1, 0, 0));
    sceVif1PkAddGsAD(packet, SCE_GS_RGBAQ, SCE_GS_SET_RGBAQ(0x80, 0x80, 0x80, alpha, *(u_int *) &q));
    sceVif1PkAddGsAD(packet, SCE_GS_TEX0_1, texture->tex0);
    sceVif1PkAddGsAD(packet, SCE_GS_UV, SCE_GS_SET_UV(source.x << 4, source.y << 4));
    sceVif1PkAddGsAD(packet, SCE_GS_XYZF2, SCE_GS_SET_XYZF2(top_left[0], top_left[1], top_left[2], top_left[3]));
    sceVif1PkAddGsAD(packet, SCE_GS_UV, SCE_GS_SET_UV((source.x + source.width) << 4, source.y << 4));
    sceVif1PkAddGsAD(packet, SCE_GS_XYZF2, SCE_GS_SET_XYZF2(top_right[0], top_right[1], top_right[2], top_right[3]));
    sceVif1PkAddGsAD(packet, SCE_GS_UV, SCE_GS_SET_UV(source.x << 4, (source.y + source.height) << 4));
    sceVif1PkAddGsAD(packet, SCE_GS_XYZF2, SCE_GS_SET_XYZF2(bottom_left[0], bottom_left[1], bottom_left[2], bottom_left[3]));
    sceVif1PkAddGsAD(packet, SCE_GS_UV, SCE_GS_SET_UV((source.x + source.width) << 4, (source.y + source.height) << 4));
    sceVif1PkAddGsAD(packet, SCE_GS_XYZF2, SCE_GS_SET_XYZF2(bottom_right[0], bottom_right[1], bottom_right[2], bottom_right[3]));
    sceVif1PkAddGsAD(packet, SCE_GS_TEST_1, *(u_long *) &mgPixelTest);
    sceVif1PkCloseGifTag(packet);
    sceVif1PkCloseDirectCode(packet);
}

/**
 * Draws a fogged sprite in world space between two projected corners.
 *
 * @mangled set3DSpriteFog__FP13sceVif1PacketP8CTextureRC8CRect_i_PiPiP6spRGBA
 * @address 0x15DCA0
 * @size 0x228
 */
void set3DSpriteFog(sceVif1Packet *packet, CTexture *texture, const CRect_i_ &source, int *top_left, int *bottom_right, spRGBA *colour) {
    float q;

    if (texture == 0) {
        return;
    }

    q = 1.0f;
    sceVif1PkCnt(packet, 0);
    sceVif1PkOpenDirectCode(packet, 0);
    sceVif1PkOpenGifTag(packet, *(u_long128 *) &GiftagAD);
    sceVif1PkAddGsAD(packet, SCE_GS_TEX1_1, SCE_GS_SET_TEX1(1, 0, 1, 1, 0, 0, 0));
    sceVif1PkAddGsAD(packet, SCE_GS_PRIM, SCE_GS_SET_PRIM(SCE_GS_PRIM_SPRITE, 0, 1, 1, 1, 0, 1, 0, 0));
    sceVif1PkAddGsAD(packet, SCE_GS_RGBAQ, SCE_GS_SET_RGBAQ(colour->r, colour->g, colour->b, colour->a, *(u_int *) &q));
    sceVif1PkAddGsAD(packet, SCE_GS_TEX0_1, texture->tex0);
    sceVif1PkAddGsAD(packet, SCE_GS_UV, SCE_GS_SET_UV(source.x << 4, source.y << 4));
    sceVif1PkAddGsAD(packet, SCE_GS_XYZF2, SCE_GS_SET_XYZF2(top_left[0], top_left[1], top_left[2], top_left[3]));
    sceVif1PkAddGsAD(packet, SCE_GS_UV, SCE_GS_SET_UV((source.x + source.width) << 4, (source.y + source.height) << 4));
    sceVif1PkAddGsAD(packet, SCE_GS_XYZF2, SCE_GS_SET_XYZF2(bottom_right[0], bottom_right[1], bottom_right[2], bottom_right[3]));
    sceVif1PkAddGsAD(packet, SCE_GS_TEST_1, *(u_long *) &mgPixelTest);
    sceVif1PkCloseGifTag(packet);
    sceVif1PkCloseDirectCode(packet);
}

void setColSprite(sceVif1Packet *packet, int *top_left, int *top_right, int *bottom_left, int *bottom_right, unsigned char red, unsigned char green, unsigned char blue, unsigned char alpha) {
    float q = 1.0f;

    sceVif1PkCnt(packet, 0);
    sceVif1PkOpenDirectCode(packet, 0);
    sceVif1PkOpenGifTag(packet, *(u_long128 *) &GiftagAD);
    sceVif1PkAddGsAD(packet, SCE_GS_TEX1_1, SCE_GS_SET_TEX1(1, 0, 1, 1, 0, 0, 0));
    sceVif1PkAddGsAD(packet, SCE_GS_PRIM, SCE_GS_SET_PRIM(4, 0, 0, 0, 1, 0, 0, 0, 0));
    sceVif1PkAddGsAD(packet, SCE_GS_RGBAQ, SCE_GS_SET_RGBAQ(red, green, blue, alpha, *(u_int *) &q));
    sceVif1PkAddGsAD(packet, SCE_GS_XYZ2, (u_long) top_left[0] | ((u_long) top_left[1] << 16) | ((u_long) top_left[2] << 32));
    sceVif1PkAddGsAD(packet, SCE_GS_XYZ2, (u_long) top_right[0] | ((u_long) top_right[1] << 16) | ((u_long) top_right[2] << 32));
    sceVif1PkAddGsAD(packet, SCE_GS_XYZ2, (u_long) bottom_left[0] | ((u_long) bottom_left[1] << 16) | ((u_long) bottom_left[2] << 32));
    sceVif1PkAddGsAD(packet, SCE_GS_XYZ2, (u_long) bottom_right[0] | ((u_long) bottom_right[1] << 16) | ((u_long) bottom_right[2] << 32));
    sceVif1PkCloseGifTag(packet);
    sceVif1PkCloseDirectCode(packet);

    sceVif1PkCnt(packet, 0);
    sceVif1PkOpenDirectCode(packet, 0);
    sceVif1PkOpenGifTag(packet, *(u_long128 *) &GiftagAD);
    sceVif1PkAddGsAD(packet, SCE_GS_TEXFLUSH, 0);
    sceVif1PkCloseGifTag(packet);
    sceVif1PkCloseDirectCode(packet);
}

void set2DSpriteC4(sceVif1Packet *packet, const CRect_i_ &screen, spRGBA *top_left, spRGBA *top_right, spRGBA *bottom_left, spRGBA *bottom_right) {
    sceGsTest test;
    sceGsZbuf zbuf;
    float     q = 1.0f;

    sceVif1PkCnt(packet, 0);
    sceVif1PkOpenDirectCode(packet, 0);
    sceVif1PkOpenGifTag(packet, *(u_long128 *) &GiftagAD);
    sceVif1PkAddGsAD(packet, SCE_GS_TEX1_1, SCE_GS_SET_TEX1(1, 0, 1, 1, 0, 0, 0));
    sceVif1PkAddGsAD(packet, SCE_GS_PRIM, SCE_GS_SET_PRIM(4, 1, 0, 0, 1, 0, 0, 0, 0));
    test = mgPixelTest;
    test.bits.ate = 0;
    test.bits.aref = 0;
    test.bits.atst = SCE_GS_ALWAYS;
    test.bits.zte = 1;
    test.bits.ztst = SCE_GS_ALWAYS;
    sceVif1PkAddGsAD(packet, SCE_GS_TEST_1, *(u_long *) &test);
    zbuf = mgZBuffer;
    zbuf.bits.zmsk = 1;
    sceVif1PkAddGsAD(packet, SCE_GS_ZBUF_1, *(u_long *) &zbuf);
    sceVif1PkAddGsAD(packet, SCE_GS_RGBAQ, SCE_GS_SET_RGBAQ(top_left->r, top_left->g, top_left->b, top_left->a, *(u_int *) &q));
    sceVif1PkAddGsAD(packet, SCE_GS_XYZF2, SCE_GS_SET_XYZF2((screen.x << 4) + 27648, (screen.y << 3) + GS_Y_OFFSET, 0, 0));
    sceVif1PkAddGsAD(packet, SCE_GS_RGBAQ, SCE_GS_SET_RGBAQ(top_right->r, top_right->g, top_right->b, top_right->a, *(u_int *) &q));
    sceVif1PkAddGsAD(packet, SCE_GS_XYZF2, SCE_GS_SET_XYZF2(((screen.x + screen.width) << 4) + 27647, (screen.y << 3) + GS_Y_OFFSET, 0, 0));
    sceVif1PkAddGsAD(packet, SCE_GS_RGBAQ, SCE_GS_SET_RGBAQ(bottom_left->r, bottom_left->g, bottom_left->b, bottom_left->a, *(u_int *) &q));
    sceVif1PkAddGsAD(packet, SCE_GS_XYZF2, SCE_GS_SET_XYZF2((screen.x << 4) + 27648, ((screen.y + screen.height) << 3) + GS_Y_OFFSET, 0, 0));
    sceVif1PkAddGsAD(packet, SCE_GS_RGBAQ, SCE_GS_SET_RGBAQ(bottom_right->r, bottom_right->g, bottom_right->b, bottom_right->a, *(u_int *) &q));
    sceVif1PkAddGsAD(packet, SCE_GS_XYZF2, SCE_GS_SET_XYZF2(((screen.x + screen.width) << 4) + 27647, ((screen.y + screen.height) << 3) + GS_Y_OFFSET, 0, 0));
    sceVif1PkAddGsAD(packet, SCE_GS_TEST_1, *(u_long *) &mgPixelTest);
    sceVif1PkAddGsAD(packet, SCE_GS_ZBUF_1, *(u_long *) &mgZBuffer);
    sceVif1PkCloseGifTag(packet);
    sceVif1PkCloseDirectCode(packet);
}

void set2DSprite(sceVif1Packet *packet, CTexture *texture, const CRect_i_ &screen, const CRect_i_ &texel, int pivot_x, int pivot_y, float angle) {
    float     x[4];
    float     y[4];
    sceGsTest test;
    sceGsZbuf zbuf;
    float     q;
    int       i;

    if (texture == 0) {
        return;
    }

    q = 1.0f;
    sceVif1PkCnt(packet, 0);
    sceVif1PkOpenDirectCode(packet, 0);
    sceVif1PkOpenGifTag(packet, *(u_long128 *) &GiftagAD);
    sceVif1PkAddGsAD(packet, SCE_GS_TEX1_1, ((u_long) linear__2 << 5) | 0x41);
    sceVif1PkAddGsAD(packet, SCE_GS_PRIM, SCE_GS_SET_PRIM(4, 0, 1, 0, 1, 1, 1, 0, 0));
    test = mgPixelTest;
    test.bits.ate = 0;
    test.bits.aref = 0;
    test.bits.atst = SCE_GS_ALWAYS;
    test.bits.zte = 1;
    test.bits.ztst = SCE_GS_ALWAYS;
    sceVif1PkAddGsAD(packet, SCE_GS_TEST_1, *(u_long *) &test);
    zbuf = mgZBuffer;
    zbuf.bits.zmsk = 1;

    x[0] = x[2] = -pivot_x;
    x[1] = x[3] = (screen.x + screen.width + 1) - (screen.x + pivot_x);
    y[0] = y[1] = -pivot_y;
    y[2] = y[3] = (screen.y + screen.height + 1) - (screen.y + pivot_y);

    for (i = 0; i < 4; i++) {
        float turned_x = y[i] * cosf(angle) + x[i] * sinf(angle);
        float turned_y = x[i] * cosf(angle) - y[i] * sinf(angle);

        x[i] = (((int) turned_x + screen.x) << 4) + 27648;
        y[i] = (((int) turned_y + screen.y) << 3) + GS_Y_OFFSET;
    }

    sceVif1PkAddGsAD(packet, SCE_GS_ZBUF_1, *(u_long *) &zbuf);
    sceVif1PkAddGsAD(packet, SCE_GS_RGBAQ, SCE_GS_SET_RGBAQ(0x80, 0x80, 0x80, 0x80, *(u_int *) &q));
    sceVif1PkAddGsAD(packet, SCE_GS_TEX0_1, texture->tex0);
    sceVif1PkAddGsAD(packet, SCE_GS_UV, SCE_GS_SET_UV(texel.x << 4, texel.y << 4));
    sceVif1PkAddGsAD(packet, SCE_GS_XYZF2, SCE_GS_SET_XYZF2(x[0], y[0], 0, 0));
    sceVif1PkAddGsAD(packet, SCE_GS_UV, SCE_GS_SET_UV((texel.x + screen.width) << 4, texel.y << 4));
    sceVif1PkAddGsAD(packet, SCE_GS_XYZF2, SCE_GS_SET_XYZF2(x[1], y[1], 0, 0));
    sceVif1PkAddGsAD(packet, SCE_GS_UV, SCE_GS_SET_UV(texel.x << 4, (texel.y + texel.height) << 4));
    sceVif1PkAddGsAD(packet, SCE_GS_XYZF2, SCE_GS_SET_XYZF2(x[2], y[2], 0, 0));
    sceVif1PkAddGsAD(packet, SCE_GS_UV, SCE_GS_SET_UV((texel.x + texel.width) << 4, (texel.y + texel.height) << 4));
    sceVif1PkAddGsAD(packet, SCE_GS_XYZF2, SCE_GS_SET_XYZF2(x[3], y[3], 0, 0));
    sceVif1PkAddGsAD(packet, SCE_GS_TEST_1, *(u_long *) &mgPixelTest);
    sceVif1PkAddGsAD(packet, SCE_GS_ZBUF_1, *(u_long *) &mgZBuffer);
    sceVif1PkCloseGifTag(packet);
    sceVif1PkCloseDirectCode(packet);
}

void set2DSpriteRot(sceVif1Packet *packet, CTexture *texture, const CRect_i_ &screen, const CRect_i_ &texel, int pivot_x, int pivot_y, float angle, unsigned char red, unsigned char green, unsigned char blue, unsigned char alpha) {
    float     x[4];
    float     y[4];
    sceGsTest test;
    sceGsZbuf zbuf;
    float     q;
    int       i;

    q = 1.0f;
    sceVif1PkCnt(packet, 0);
    sceVif1PkOpenDirectCode(packet, 0);
    sceVif1PkOpenGifTag(packet, *(u_long128 *) &GiftagAD);
    sceVif1PkAddGsAD(packet, SCE_GS_TEX1_1, ((u_long) linear__2 << 5) | 0x41);
    sceVif1PkAddGsAD(packet, SCE_GS_PRIM, SCE_GS_SET_PRIM(4, 0, 1, 0, 1, 1, 1, 0, 0));
    test = mgPixelTest;
    test.bits.ate = 0;
    test.bits.aref = 0;
    test.bits.atst = SCE_GS_ALWAYS;
    test.bits.zte = 1;
    test.bits.ztst = SCE_GS_ALWAYS;
    sceVif1PkAddGsAD(packet, SCE_GS_TEST_1, *(u_long *) &test);
    zbuf = mgZBuffer;
    zbuf.bits.zmsk = 1;

    x[0] = x[2] = pivot_x * -16;
    x[1] = x[3] = ((screen.width - pivot_x) << 4) - 1;
    y[0] = y[1] = pivot_y * -16;
    y[2] = y[3] = ((screen.height - pivot_y) << 4) - 1;

    for (i = 0; i < 4; i++) {
        float turned_x = -y[i] * sinf(angle) - x[i] * cosf(angle);
        float turned_y = -x[i] * sinf(angle) + y[i] * cosf(angle);

        x[i] = (int) turned_x + (screen.x << 4) + 27648;
        y[i] = (int) (0.5f * turned_y) + (screen.y << 3) + GS_Y_OFFSET;
    }

    sceVif1PkAddGsAD(packet, SCE_GS_ZBUF_1, *(u_long *) &zbuf);
    sceVif1PkAddGsAD(packet, SCE_GS_RGBAQ, SCE_GS_SET_RGBAQ(red, green, blue, alpha, *(u_int *) &q));
    sceVif1PkAddGsAD(packet, SCE_GS_TEX0_1, texture->tex0);
    sceVif1PkAddGsAD(packet, SCE_GS_UV, SCE_GS_SET_UV(texel.x << 4, texel.y << 4));
    sceVif1PkAddGsAD(packet, SCE_GS_XYZF2, SCE_GS_SET_XYZF2(x[0], y[0], 0, 0));
    sceVif1PkAddGsAD(packet, SCE_GS_UV, SCE_GS_SET_UV((texel.x + screen.width) << 4, texel.y << 4));
    sceVif1PkAddGsAD(packet, SCE_GS_XYZF2, SCE_GS_SET_XYZF2(x[1], y[1], 0, 0));
    sceVif1PkAddGsAD(packet, SCE_GS_UV, SCE_GS_SET_UV(texel.x << 4, (texel.y + texel.height) << 4));
    sceVif1PkAddGsAD(packet, SCE_GS_XYZF2, SCE_GS_SET_XYZF2(x[2], y[2], 0, 0));
    sceVif1PkAddGsAD(packet, SCE_GS_UV, SCE_GS_SET_UV((texel.x + texel.width) << 4, (texel.y + texel.height) << 4));
    sceVif1PkAddGsAD(packet, SCE_GS_XYZF2, SCE_GS_SET_XYZF2(x[3], y[3], 0, 0));
    sceVif1PkAddGsAD(packet, SCE_GS_TEST_1, *(u_long *) &mgPixelTest);
    sceVif1PkAddGsAD(packet, SCE_GS_ZBUF_1, *(u_long *) &mgZBuffer);
    sceVif1PkCloseGifTag(packet);
    sceVif1PkCloseDirectCode(packet);
}

/**
 * Draws a textured sprite in screen space.
 *
 * @mangled set2DSprite__FP13sceVif1PacketP8CTextureP4RECTP4RECTUc
 * @address 0x15F090
 * @size 0x68
 */
void set2DSprite(sceVif1Packet *packet, CTexture *texture, RECT *screen, RECT *texel, unsigned char alpha) {
    set2DSprite(packet, texture, CRect_i_(screen->x, screen->y, screen->width, screen->height), CRect_i_(texel->x, texel->y, texel->width, texel->height), alpha);
}
