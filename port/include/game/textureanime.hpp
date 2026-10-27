#pragma once

#include "common.h"

namespace dc::game {

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
     */
    void Initialize();

    /**
     * Constructs an inactive texture-animation record.
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
     * Number of animation groups a player tracks.
     */
    static constexpr int kGroupCount = 24;

    /**
     * Stops every texture animation from advancing while it is non-zero.
     */
    static s32 stop_anime;

    s32            enabled[kGroupCount]; /**< Enables playback for each animation group. */
    CTexAnimeData *first[kGroupCount];   /**< Points to each group's first animation record. */
    CTexAnimeData *last[kGroupCount];    /**< Points to each group's last animation record. */
    CTexAnimeData *current[kGroupCount]; /**< Points to the record being played by each group. */
    s32            frame[kGroupCount];   /**< Counts playback frames for each group's current record. */
    CTexAnimeData *data;                 /**< Holds the animation record pool. */
    s32            data_count;           /**< Gives the number of records in the pool. */
    u8             unk_1e8[8];

    /**
     * Uploads one animation group's current textures to the graphics synthesizer.
     */
    void TexAnime(int texture_block);

    /**
     * Assigns the record pool and clears every animation group.
     */
    void Initialize(CTexAnimeData *records, int count);

    /**
     * Creates an animation player using the supplied record pool.
     */
    CTextureAnime(CTexAnimeData *records = nullptr, int count = 0);

    /**
     * Allocates the next record from the texture-animation pool.
     */
    CTexAnimeData *NewTexAnimeData();

    /**
     * Starts a new animation group at the next available pool record.
     */
    CTexAnimeData *NewTexAnimeGroupData(int group);

    /**
     * Appends one texture-animation record to its selected group.
     */
    int EnterTexAnime(CTexAnimeData *source);

    /**
     * Stops and resets every animation group.
     */
    void DisableAll();

    /**
     * Enables playback of one animation group.
     */
    void Enable(int group);

    /**
     * Stops one animation group and resets its playback position.
     */
    void Disable(int group);

    /**
     * Runs the texture-animation commands of a configuration script held in memory.
     */
    void LoadCFGFile(char *script, int script_size);
};

STATIC_ASSERT(sizeof(CTextureAnime) == 0x1F0);

} // namespace dc::game
