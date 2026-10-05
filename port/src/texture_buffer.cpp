#include <cstdint>

#include "texture.hpp"

// Retail's buffer arithmetic with the pointers kept whole: it went through int, which only holds
// an address below 2 GiB.

PC_OVERRIDE void CTextureManager::SetBuffer(u_long128 *buffer, int size) {
    int misalignment;
    int skipped_quads;

    this->buffer = buffer;
    buffer_size = size;
    misalignment = static_cast<int>(reinterpret_cast<std::uintptr_t>(this->buffer) & 0x7f);

    if (misalignment != 0) {
        skipped_quads = (128 - misalignment) >> 4;
        this->buffer += skipped_quads;
        buffer_size -= skipped_quads;
    }

    buffer_used = 0;
}

PC_OVERRIDE int CTextureManager::CleanUpBuffer() {
    u_long128 *block_start[72];
    int        block_order[72];
    int        i;
    int        j;

    for (i = 0; i < 72; i++) {
        block_start[i] = blocks[i].buffer;
        block_order[i] = i;
    }

    for (i = 0; i < 71; i++) {
        for (j = i + 1; j < 72; j++) {
            u_long128 **lower = &block_start[i];
            u_long128 **upper = &block_start[j];
            int        *lower_block = &block_order[i];
            int        *upper_block = &block_order[j];

            if (*lower > *upper) {
                u_long128 *swap_start = *lower;
                int        swap_block = *lower_block;
                *lower = *upper;
                *lower_block = *upper_block;
                *upper = swap_start;
                *upper_block = swap_block;
            }
        }
    }

    buffer_used = 0;

    for (i = 0; i < 72; i++) {
        int        block_no = block_order[i];
        u_long128 *destination;
        u_long128 *source;
        int        block_quads;
        int        k;
        int        misalignment;
        int        shift;

        if (block_no <= 0) {
            continue;
        }

        source = block_start[i];

        if (source == 0) {
            continue;
        }

        block_quads = blocks[block_no].buffer_end - blocks[block_no].buffer;
        destination = buffer + buffer_used;
        shift = (source - destination) * 16;
        blocks[block_no].buffer = destination;
        blocks[block_no].buffer_end = destination + block_quads;

        for (k = 0; k < block_quads; k++) {
            *destination = *source;
            source++;
            destination++;
        }

        for (j = 0; j < 196 && shift != 0; j++) {
            CTexture *tex = &textures[j];
            int       level;

            if (tex->block != block_no) {
                continue;
            }

            for (level = 0; level < 4; level++) {
                u_int **images = tex->image;
                u_int **slot = &images[level];

                if (tex->image[level] == 0) {
                    continue;
                }

                *slot = (u_int *) ((char *) *slot - shift);
            }

            if (tex->clut != 0) {
                tex->clut = (u_int *) ((char *) tex->clut - shift);
            }
        }

        buffer_used += block_quads;
        misalignment = block_quads % 8;

        if (misalignment != 0) {
            buffer_used += 8 - misalignment;
        }
    }

    return 1;
}
