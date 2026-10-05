#include <array>
#include <cmath>

#include "draw2d_port.hpp"
#include "editloop.hpp"
#include "rect.hpp"
#include "texture.hpp"

// The editor's own rotation in 12.4 units: offsets turned with the axes swapped at angle 0, the far
// edges a sixteenth short, y halved for the field after turning. Always point-sampled.
PC_OVERRIDE void set2DSpriteRot(sceVif1Packet *packet, CTexture *texture, const CRect_i_ &screen, const CRect_i_ &texel,
                                int pivot_x, int pivot_y, float angle, unsigned char alpha) {
    if (texture == nullptr) {
        return;
    }

    float x[4];
    float y[4];

    x[0] = x[2] = static_cast<float>(pivot_x * -16);
    x[1] = x[3] = static_cast<float>(((screen.width - pivot_x) << 4) - 1);
    y[0] = y[1] = static_cast<float>(pivot_y * -16);
    y[2] = y[3] = static_cast<float>(((screen.height - pivot_y) << 4) - 1);

    for (int i = 0; i < 4; i++) {
        const float turned_x = y[i] * cosf(angle) + x[i] * sinf(angle);
        const float turned_y = x[i] * cosf(angle) - y[i] * sinf(angle);

        x[i] = static_cast<float>(screen.x) + static_cast<float>(static_cast<int>(turned_x)) / 16.0f;
        y[i] = static_cast<float>(screen.y) + static_cast<float>(static_cast<int>(0.5f * turned_y)) / 8.0f;
    }

    const float u0 = static_cast<float>(texel.x);
    const float v0 = static_cast<float>(texel.y);
    const float v1 = static_cast<float>(texel.y + texel.height);

    // The second corner's u spans the screen width, not the texel width, as in retail.
    std::array<gfx::Vertex2D, 4> strip = {
        draw2d::Vertex(x[0], y[0], 0.0f, u0, v0, 0x80, 0x80, 0x80, alpha),
        draw2d::Vertex(x[1], y[1], 0.0f, static_cast<float>(texel.x + screen.width), v0, 0x80, 0x80, 0x80,
                       alpha),
        draw2d::Vertex(x[2], y[2], 0.0f, u0, v1, 0x80, 0x80, 0x80, alpha),
        draw2d::Vertex(x[3], y[3], 0.0f, static_cast<float>(texel.x + texel.width), v1, 0x80, 0x80, 0x80,
                       alpha),
    };
    draw2d::DrawTextured(gfx::Primitive::TriangleStrip, strip, texture->tex0, 0x41, draw2d::SpriteState());
    draw2d::RestoreTestZbuf();
}
