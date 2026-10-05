#include <cstring>

#include "cloth.hpp"
#include "draw3d.hpp"
#include "mglib.hpp"
#include "renderinfo.hpp"
#include "texture.hpp"

namespace {

// CCloth::Step carries the grid along rigidly, without simulating, when its root moved further than
// this in a tick (a cut, a warp, a turn on the spot in an event). The vertices of the two ticks are
// then not one motion, and blended they pass through each other, so such a tick is not blended.
constexpr float kClothResetDistance = 10.0f;

} // namespace

// The grid is rebuilt every frame from the simulation, so it is drawn as an immediate triangle
// list: one strip per row pair, each row drawn from the far side (wound the other way) when
// polygon_divide says so, all with the fixed cloth material and the model's texture. The vertices
// are world positions under an identity model, so the draw is known by the cloth and the display
// list interpolates its vertices between ticks as it does the matrices of the body it hangs from.
PC_OVERRIDE int CCloth::CreateVUData(u_int *packet) {
    if (packet == nullptr) {
        return kDraw3DBlockQuads;
    }
    Draw3DVisual &visual = Draw3DRegisterVisual(packet, true);

    Draw3DStrip strip;
    float       cloth_ambient[4] = {0.3f, 0.3f, 0.3f, 0.0f};
    std::memcpy(strip.ambient, cloth_ambient, sizeof(strip.ambient));
    for (float &value : strip.specular) {
        value = 1.0f;
    }
    float     su = 1.0f;
    float     sv = 1.0f;
    CTexture *texture = TexManager.GetTexture(material.texture, -1);
    if (texture != nullptr) {
        strip.texture = TexManager.GetTextureHandle(material.texture, -1);
        strip.tex0 = texture->tex0;
        strip.tex1 = texture->tex1;
        if (texture->width > 0 && texture->height > 0) {
            su = static_cast<float>(1u << ((texture->tex0 >> 26) & 0xF)) / static_cast<float>(texture->width);
            sv = static_cast<float>(1u << ((texture->tex0 >> 30) & 0xF)) / static_cast<float>(texture->height);
        }
    }

    auto add = [&](int i, int j) {
        gfx::Vertex3D vertex = {};
        std::memcpy(vertex.position, point[i][j], sizeof(vertex.position));
        std::memcpy(vertex.normal, normal_grid[i][j], sizeof(vertex.normal));
        vertex.uv[0] = texture_coord[i][j][0] * su;
        vertex.uv[1] = texture_coord[i][j][1] * sv;
        vertex.color[0] = vertex.color[1] = vertex.color[2] = vertex.color[3] = 0x80;
        visual.vertices.push_back(vertex);
    };

    for (int i = 0; i < num_i - 1; i++) {
        uint32_t first = static_cast<uint32_t>(visual.vertices.size());
        for (int j = 0; j < num_j; j++) {
            if (polygon_divide[i] != 0) {
                add(i + 1, j);
                add(i, j);
            } else {
                add(i, j);
                add(i + 1, j);
            }
        }
        Draw3DStripToList(visual.indices, first, static_cast<uint32_t>(num_j * 2));
    }
    strip.index_count = static_cast<uint32_t>(visual.indices.size());
    visual.strips.push_back(strip);
    return kDraw3DBlockQuads;
}

PC_OVERRIDE int CCloth::DrawVu1(u_int *packet, float (*matrix)[4], RenderInfo *info, VU1_PROGRAM program, u_long128 *draw_state,
                                int unknown1, int unknown2) {
    CreateVUData(vu_block[DBuffID]);
    vu_data = vu_block[DBuffID];
    if (const Draw3DVisual *visual = Draw3DFindVisual(vu_data)) {
        Draw3DIdentityScope identity(this, kClothResetDistance, true);
        Draw3DDrawVisual(*visual, matrix, *info, program);
    }
    return 0;
}

PC_OVERRIDE int CCloth::DrawVu1(sceVif1Packet *packet, float (*matrix)[4], RenderInfo *info, VU1_PROGRAM program,
                                u_long128 *draw_state, int unknown1, int unknown2) {
    return CCloth::DrawVu1(static_cast<u_int *>(nullptr), matrix, info, program, draw_state, unknown1, unknown2);
}
