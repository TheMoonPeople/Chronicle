#include "spritetable.hpp"

#include <libpkt.h>

#include "mglib.hpp"
#include "rect.hpp"

void CSpriteTable::AddTable(int x, int y, MG_SPRITE *sprite, int layer, int align_flags) {
    if (layer < 0) {
        layer = 0;
    }

    if (layer >= list_count) {
        layer = list_count - 1;
    }

    SPRITE_TABLE *node;
    SPRITE_TABLE *(&tail_entries)[16] = tails;
    SPRITE_TABLE **tail = &tail_entries[layer];
    node = *tail;

    if (node == NULL) {
        return;
    }

    node->tex0 = sprite->tex0;
    node->x = x;
    node->y = y;
    node->width = sprite->source.width;
    node->height = sprite->source.height;

    if (node->height == 448) {
        node->height = 480;
    }

    node->u = sprite->source.x;
    node->v = sprite->source.y;
    node->u_width = sprite->source.width;
    node->v_height = sprite->source.height;
    node->red = sprite->red;
    node->green = sprite->green;
    node->blue = sprite->blue;
    node->alpha = sprite->alpha;

    if (align_flags & 1) {
        s16 width = node->width;
        node->x -= width;
    }

    if (align_flags & 2) {
        s16 height = node->height;
        node->y -= height;
    }

    if (align_flags & 4) {
        node->x -= (s16) (node->width >> 1);
    }

    if (align_flags & 8) {
        node->y -= (s16) (node->height >> 1);
    }

    node->next = GetNext();
    *tail = node->next;
}

void CSpriteTable::AddTable(int x, int y, sceGsTex0 *tex0, RECT *source, int layer, int align_flags) {
    union {
        MG_SPRITE sprite;     /**< Sprite handed to the other AddTable. */
        u_long128 padding[3]; /**< Pads the sprite's stack slot to 48 bytes. */
    } sprite_buffer;

    *(sceGsTex0 *) &sprite_buffer.sprite.tex0 = *tex0;
    sprite_buffer.sprite.source = *source;
    sprite_buffer.sprite.red = 128;
    sprite_buffer.sprite.green = 128;
    sprite_buffer.sprite.blue = 128;
    sprite_buffer.sprite.alpha = 128;
    AddTable(x, y, &sprite_buffer.sprite, layer, align_flags);
}

void CSpriteTable::Initialize(SPRITE_TABLE *new_pool, int entry_count, int layer_count) {
    list_count = layer_count;

    if (list_count > 16) {
        list_count = 16;
    }

    pool = new_pool;
    pool_count = entry_count;
    end = new_pool + entry_count;
    ClearPointer();
}

void CSpriteTable::ClearPointer() {
    current = pool;

    for (int layer = 0; layer < list_count; layer++) {
        heads[layer] = GetNext();
        heads[layer]->next = NULL;
        tails[layer] = heads[layer];
    }
}
