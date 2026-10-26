#pragma once

/**
 * A TIM2 image as the texture registry reads it: a sixteen-byte file header followed by one
   picture header per picture, whose own header states how far past it the pixels start. Only the
   fields the reconstruction reads are named; the game's own name for the picture header is not in
   the image, so `TM2_head` is the only one of the two names it provides
   (docs/formats/tim2.md).
 */

#include "common.h"

/**
 * Pixel formats of a TIM2 picture, as TM2_head::image_type holds them.
 */
// clang-format off
enum TIM2_IMAGE_TYPE {
    TIM2_RGB16  = 1, /**< Sixteen-bit colour. */
    TIM2_RGB24  = 2, /**< Twenty-four-bit colour. */
    TIM2_RGB32  = 3, /**< Thirty-two-bit colour. */
    TIM2_IDTEX4 = 4, /**< Four-bit indexed colour. */
    TIM2_IDTEX8 = 5, /**< Eight-bit indexed colour. */
};

// clang-format on

/**
 * The file header, which the registry reads the first picture's size and format through as well:
   the game's own declaration names those three fields at their absolute offsets rather than
   reaching them through the picture header beside it, and both spellings appear in the same
   file.
 */
struct TM2_head {
    char    unk_00[35];
    u_char  image_type;   /**< TIM2 pixel-storage format. @see TIM2_IMAGE_TYPE. */
    u_short image_width;  /**< Image width in pixels. */
    u_short image_height; /**< Image height in pixels. */
};

/**
 * Describes one picture and its mip levels in a TIM2 image.
 */
struct TM2_picture {
    char    unk_00[8];
    u_int   image_size;  /**< Total byte size of the picture image data. */
    u_short header_size; /**< Byte size of this picture header. */
    u_short clut_colors; /**< Number of colours stored in the palette. */
    char    unk_10;
    u_char  mipmap_count; /**< Number of mip levels stored for the picture. */
    char    unk_12;
    u_char  image_type;   /**< TIM2 pixel-storage format. @see TIM2_IMAGE_TYPE. */
    u_short image_width;  /**< Base-level width in pixels. */
    u_short image_height; /**< Base-level height in pixels. */
    char    unk_18[8];
    u_long  tex1; /**< GS TEX1 register value used for sampling. */
    char    unk_28[24];
    u_int   mipmap_size[8]; /**< Byte size of each stored mip level. */
};
