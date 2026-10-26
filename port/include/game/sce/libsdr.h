#pragma once

/* Commands sceSdRemote forwards to the IOP sound library. */
#define rSdInit 0x8000
#define rSdSetParam 0x8010
#define rSdSetSwitch 0x8030
#define rSdSetAddr 0x8050
#define rSdSetCoreAttr 0x8070
#define rSdSetEffectAttr 0x8130

/* Voice switch, parameter and address registers, combined with a core number. */
#define SD_S_KOFF (0x16 << 8)
#define SD_P_MVOLL ((0x09 << 8) + (0x01 << 7))
#define SD_P_MVOLR ((0x0A << 8) + (0x01 << 7))
#define SD_P_EVOLL ((0x0B << 8) + (0x01 << 7))
#define SD_P_EVOLR ((0x0C << 8) + (0x01 << 7))
#define SD_A_EEA (0x1D << 8)

/* Core attributes, combined with a core number. */
#define SD_C_EFFECT_ENABLE (1 << 1)
#define SD_C_SPDIF_MODE (5 << 1)

/* Values of the attributes above. */
#define SD_SPDIF_COPY_PROHIBIT 0x80
#define SD_REV_MODE_CLEAR_WA 0x100

/**
 * Describes the reverberation of one core.
 */
struct sceSdEffectAttr {
    int core;      /**< Core the attributes apply to. */
    int mode;      /**< Reverberation type, with the work-area clear flag. */
    short depth_L; /**< Left reverberation depth. */
    short depth_R; /**< Right reverberation depth. */
    int delay;     /**< Echo and delay time. */
    int feedback;  /**< Echo and delay feedback. */
};

extern "C" {

/**
 * Connects the EE client to the IOP sound library.
 */
int sceSdRemoteInit(void);

/**
 * Runs one IOP sound library command and returns its result.
 */
int sceSdRemote(int arg, int command, ...);

}
