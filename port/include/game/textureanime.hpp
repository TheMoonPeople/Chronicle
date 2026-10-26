#pragma once

#include "common.h"

class CTexture;

/**
 * Stores the texture attributes copied into one texture-animation endpoint.
 */
struct CTextureTexAnime {
    s16    block;  /**< Texture block containing the endpoint texture. */
    s16    width;  /**< Endpoint texture width in pixels. */
    s16    height; /**< Endpoint texture height in pixels. */
    s16    bpp;    /**< Endpoint texture bits per pixel. */
    u_long tex0;   /**< GS TEX0 register value used by the endpoint. */
    u_long tex1;   /**< GS TEX1 register value used by the endpoint. */

    /**
     * Copies the animation-relevant attributes from a registered texture.
     *
     * @mangled Copy__16CTextureTexAnimeFP8CTexture
     * @address 0x167060
     * @size 0x40
     */
    void Copy(CTexture *texture);
};

STATIC_ASSERT(sizeof(CTextureTexAnime) == 0x18);

/**
 * Animates the texture of one part of a model.
 */
class CTexAnimeData {
public:
    s16              kind;           /**< Record kind: -1 free, 0 rectangle copy, 1 scrolling copy. */
    s16              group;          /**< Animation group the record belongs to. */
    s16              duration;       /**< Frames the record plays for; zero also plays the next record, -1 holds forever. */
    s16              linked_group;   /**< Group enabled while the record plays, or -1 for none. */
    CTextureTexAnime first_texture;  /**< First texture endpoint used by the animation. */
    CTextureTexAnime second_texture; /**< Second texture endpoint used by the animation. */
    s16              source_x;       /**< Left edge of the rectangle copied from the first texture. */
    s16              source_y;       /**< Top edge of the rectangle copied from the first texture. */
    s16              source_width;   /**< Width of the rectangle copied from the first texture. */
    s16              source_height;  /**< Height of the rectangle copied from the first texture. */
    s16              dest_x;         /**< Left edge the rectangle is copied to in the second texture. */
    s16              dest_y;         /**< Top edge the rectangle is copied to in the second texture. */
    float            scroll_x_step;  /**< Horizontal texture-scroll increment per frame. */
    float            scroll_y_step;  /**< Vertical texture-scroll increment per frame. */
    float            scroll_x;       /**< Current horizontal texture-scroll offset. */
    float            scroll_y;       /**< Current vertical texture-scroll offset. */
    CTexAnimeData   *next;           /**< Links the record to the next animation in its group. */

    /**
     * Sets the animation to play no frame.
     *
     * @mangled Initialize__13CTexAnimeDataFv
     * @address 0x1670D0
     * @size 0x9C
     */
    void Initialize();

    /**
     * Constructs an inactive texture-animation record.
     *
     * @mangled __ct__13CTexAnimeDataFv
     * @address 0x1670A0
     * @size 0x30
     */
    CTexAnimeData();
};

STATIC_ASSERT(sizeof(CTexAnimeData) == 0x58);

/**
 * Plays groups of texture animation records from a caller-supplied pool.
 */
class CTextureAnime {
public:
    /**
     * Stops every texture animation from advancing while it is non-zero.
     *
     * @mangled stop_anime__13CTextureAnime
     */
    static s32 stop_anime;

    s32            enabled[24]; /**< Enables playback for each animation group. */
    CTexAnimeData *first[24];   /**< Points to each group's first animation record. */
    CTexAnimeData *last[24];    /**< Points to each group's last animation record. */
    CTexAnimeData *current[24]; /**< Points to the record being played by each group. */
    s32            frame[24];   /**< Counts playback frames for each group's current record. */
    CTexAnimeData *data;        /**< Holds the animation record pool. */
    s32            data_count;  /**< Gives the number of records in the pool. */
    u8             unk_1e8[8];

    /**
     * Uploads one animation group's current textures to the graphics synthesizer.
     *
     * @mangled TexAnime__13CTextureAnimeFi
     * @address 0x167170
     * @size 0x6B0
     */
    void TexAnime(int texture_block);

    /**
     * Assigns the record pool and clears every animation group.
     *
     * @mangled Initialize__13CTextureAnimeFP13CTexAnimeDatai
     * @address 0x167820
     * @size 0x48
     */
    void Initialize(CTexAnimeData *records, int count);

    /**
     * Creates an animation player using the supplied record pool.
     *
     * @mangled __ct__13CTextureAnimeFP13CTexAnimeDatai
     * @address 0x167870
     * @size 0x30
     */
    CTextureAnime(CTexAnimeData *records = 0, int count = 0);

    /**
     * Allocates the next record from the texture-animation pool.
     *
     * @mangled NewTexAnimeData__13CTextureAnimeFv
     * @address 0x1678A0
     * @size 0x60
     * @unknownret
     */
    CTexAnimeData *NewTexAnimeData();

    /**
     * Starts a new animation group at the next available pool record.
     *
     * @mangled NewTexAnimeGroupData__13CTextureAnimeFi
     * @address 0x167900
     * @size 0xDC
     * @unknownret
     */
    CTexAnimeData *NewTexAnimeGroupData(int group);

    /**
     * Appends one texture-animation record to its selected group.
     *
     * @mangled EnterTexAnime__13CTextureAnimeFP13CTexAnimeData
     * @address 0x1679E0
     * @size 0xF8
     * @unknownret
     */
    int EnterTexAnime(CTexAnimeData *source);

    /**
     * Stops and resets every animation group.
     *
     * @mangled DisableAll__13CTextureAnimeFv
     * @address 0x167AE0
     * @size 0x58
     */
    void DisableAll();

    /**
     * Enables playback of one animation group.
     *
     * @mangled Enable__13CTextureAnimeFi
     * @address 0x167B40
     * @size 0x34
     */
    void Enable(int group);

    /**
     * Stops one animation group and resets its playback position.
     *
     * @mangled Disable__13CTextureAnimeFi
     * @address 0x167B80
     * @size 0x3C
     */
    void Disable(int group);

    /**
     * Runs the texture-animation commands of a configuration script held in memory.
     *
     * @mangled LoadCFGFile__13CTextureAnimeFPci
     * @address 0x167BC0
     * @size 0xB4
     * @unknownret
     */
    void LoadCFGFile(char *script, int script_size);
};

STATIC_ASSERT(sizeof(CTextureAnime) == 0x1F0);

/**
 * Texture animation the configuration script being loaded fills.
 */
extern CTextureAnime *pTexAnime;
/**
 * Animation group the configuration script's records are entered into.
 */
extern int now_group;

/**
 * Constructor of CTextureAnime under the name main's array constructors call it by.
 *
 * @mangled __ct__13CTextureAnimeFP13CTexAnimeDatai
 * @address 0x167870
 * @size 0x30
 */
extern "C" void *GeneratedTextureAnimeCtor(void *self, CTexAnimeData *data, int count);
