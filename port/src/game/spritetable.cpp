#include "spritetable.hpp"

#include <libpkt.h>

#include <algorithm>
#include <array>

#include "mglib.hpp"
#include "rect.hpp"
#include "render/draw2d.hpp"
#include "render/gs_state.hpp"

namespace dc::game {

using namespace dc::render;

// Layers draw from the last to the first, so layer 0 ends up on top. The table blends with mglib's
// ALPHA shadow whatever the register held, and leaves ALPHA, TEST and ZBUF at the shadows.
void CSpriteTable::DrawTable() {
    const draw2d::Services &services = draw2d::Get();
    gfx::DrawState          state = draw2d::SpriteState();
    MGPortApplyAlpha(state, mgAlpha);

    for (int layer = list_count - 1; layer >= 0; layer--) {
        for (SPRITE_TABLE *node = heads[layer]; node != tails[layer]; node = node->next) {
            const auto  r = static_cast<u_char>(node->red);
            const auto  g = static_cast<u_char>(node->green);
            const auto  b = static_cast<u_char>(node->blue);
            const auto  a = static_cast<u_char>(node->alpha);
            const float x0 = node->x;
            const float y0 = node->y;
            const float x1 = node->x + node->width;
            const float y1 = node->y + node->height;
            const float u0 = node->u;
            const float v0 = node->v;
            const float u1 = node->u + node->u_width;
            const float v1 = node->v + node->v_height;

            std::array<gfx::Vertex2D, 4> quad = {
                draw2d::Vertex(x0, y0, 0.0f, u0, v0, r, g, b, a),
                draw2d::Vertex(x1, y0, 0.0f, u1, v0, r, g, b, a),
                draw2d::Vertex(x1, y1, 0.0f, u1, v1, r, g, b, a),
                draw2d::Vertex(x0, y1, 0.0f, u0, v1, r, g, b, a),
            };
            draw2d::DrawTextured(gfx::Primitive::Quads, quad, node->tex0, 1, state);
        }
    }

    services.set_alpha(nullptr);
    draw2d::RestoreTestZbuf();
}

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

// Retail compares the pointers as 32-bit integers, which a pool above 4 GiB defeats.
SPRITE_TABLE *CSpriteTable::GetNext() {
    SPRITE_TABLE *next = nullptr;

    if (current < end) {
        next = current;
        current++;
    }

    return next;
}

void CSpriteTable::ClearPointer() {
    current = pool;

    for (int layer = 0; layer < list_count; layer++) {
        heads[layer] = GetNext();
        heads[layer]->next = nullptr;
        tails[layer] = heads[layer];
    }
}

} // namespace dc::game
