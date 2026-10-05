#include "spritetable.hpp"

#include <array>

#include "draw2d_port.hpp"
#include "mglib.hpp"
#include "mglib_port.hpp"

// Layers draw from the last to the first, so layer 0 ends up on top. The table blends with mglib's
// ALPHA shadow whatever the register held, and leaves ALPHA, TEST and ZBUF at the shadows.
PC_OVERRIDE void CSpriteTable::DrawTable() {
    const draw2d::Services &services = draw2d::Get();
    gfx::DrawState          state = draw2d::SpriteState();
    MGPortApplyAlpha(state, mgAlpha);

    for (int layer = list_count - 1; layer >= 0; layer--) {
        for (SPRITE_TABLE *node = heads[layer]; node != tails[layer]; node = node->next) {
            const u_char r = static_cast<u_char>(node->red);
            const u_char g = static_cast<u_char>(node->green);
            const u_char b = static_cast<u_char>(node->blue);
            const u_char a = static_cast<u_char>(node->alpha);
            const float  x0 = node->x;
            const float  y0 = node->y;
            const float  x1 = node->x + node->width;
            const float  y1 = node->y + node->height;
            const float  u0 = node->u;
            const float  v0 = node->v;
            const float  u1 = node->u + node->u_width;
            const float  v1 = node->v + node->v_height;

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

// Retail compares the pointers as 32-bit integers, which a pool above 4 GiB defeats.
PC_OVERRIDE SPRITE_TABLE *CSpriteTable::GetNext() {
    SPRITE_TABLE *next = NULL;

    if (current < end) {
        next = current;
        current++;
    }

    return next;
}
