#include "spritetable.hpp"

#include <libpkt.h>

#include <algorithm>

#include "mglib.hpp"
#include "rect.hpp"

void CSpriteTable::AddTable(int x, int y, MG_SPRITE *sprite, int layer, int align_flags) {
    layer = std::max(layer, 0);

    if (layer >= list_count) {
        layer = list_count - 1;
    }

    SPRITE_TABLE *&tail = tails[layer];
    SPRITE_TABLE  *node = tail;

    if (node == nullptr) {
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
        node->x -= node->width;
    }

    if (align_flags & 2) {
        node->y -= node->height;
    }

    if (align_flags & 4) {
        node->x -= static_cast<s16>(node->width >> 1);
    }

    if (align_flags & 8) {
        node->y -= static_cast<s16>(node->height >> 1);
    }

    node->next = GetNext();
    tail = node->next;
}

void CSpriteTable::AddTable(int x, int y, sceGsTex0 *tex0, RECT *source, int layer, int align_flags) {
    MG_SPRITE sprite{};
    *reinterpret_cast<sceGsTex0 *>(&sprite.tex0) = *tex0;
    sprite.source = *source;
    sprite.red = 128;
    sprite.green = 128;
    sprite.blue = 128;
    sprite.alpha = 128;
    AddTable(x, y, &sprite, layer, align_flags);
}

void CSpriteTable::Initialize(SPRITE_TABLE *new_pool, int entry_count, int layer_count) {
    list_count = std::min(layer_count, 16);

    pool = new_pool;
    pool_count = entry_count;
    end = new_pool + entry_count;
    ClearPointer();
}

void CSpriteTable::ClearPointer() {
    current = pool;

    for (int layer = 0; layer < list_count; layer++) {
        heads[layer] = GetNext();
        heads[layer]->next = nullptr;
        tails[layer] = heads[layer];
    }
}
