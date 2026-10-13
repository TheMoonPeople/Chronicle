#include "draw2d_port.hpp"

#include <algorithm>
#include <span>

#include "mglib.hpp"
#include "mglib_port.hpp"
#include "screen.hpp"
#include "texture.hpp"
#include "textwipe.hpp"

namespace draw2d {

namespace {

gfx::DrawState RealDrawState() { return MGPortDrawState(); }

PortTextureRef RealTexture(u_long tex0, u_long tex1) { return PortTextureFromTex0(tex0, tex1); }

CTexture *RealFindTexture(const char *name) { return TexManager.GetTexture(const_cast<char *>(name), -1); }

constexpr Services kRealServices = {
    RealDrawState, RealTexture, RealFindTexture, MGSetGsTEST, MGSetGsZBUF, MGSetGsALPHA, MGSetGsTEXA,
};

const Services *g_services = &kRealServices;

constexpr const char *kFrameSnapshot = "draw2d_frame_snapshot";

} // namespace

const Services &Get() { return *g_services; }

void SetServices(const Services *services) { g_services = services != nullptr ? services : &kRealServices; }

gfx::Filter FilterFromTex1(u_long tex1) {
    // Every TEX1 the 2D units write has LCM=1 and K=0, so LOD is 0 and MMAG alone decides.
    return (tex1 >> 5) & 1 ? gfx::Filter::Linear : gfx::Filter::Nearest;
}

bool Resolve(u_long tex0, u_long tex1, gfx::DrawState &state, Texture &texture) {
    PortTextureRef ref = Get().texture(tex0, tex1);
    if (!ref.valid || ref.binding.texture == gfx::kNullTexture) {
        return false;
    }

    texture.binding = ref.binding;
    texture.binding.filter = FilterFromTex1(tex1);
    // SetEnv leaves CLAMP_1 at clamp for both axes and no 2D unit changes it.
    texture.binding.wrap_u = gfx::Wrap::Clamp;
    texture.binding.wrap_v = gfx::Wrap::Clamp;
    texture.v_scale = 1.0f;

    if (texture.binding.texture == gfx::kMainTarget || texture.binding.texture == gfx::kPreviousFrame) {
        texture.v_scale = 2.0f;
    }

    if (texture.binding.texture == gfx::kMainTarget) {
        gfx::TextureHandle snapshot =
            gfx::NamedRenderTarget(kFrameSnapshot, static_cast<uint32_t>(kScreenWidth),
                                   static_cast<uint32_t>(kScreenHeight), true, false, true);
        if (snapshot == gfx::kNullTexture || !gfx::SnapshotFrame(snapshot)) {
            return false;
        }
        texture.binding.texture = snapshot;
    }

    if (texture.binding.texture == gfx::CurrentRenderTarget()) {
        return false;
    }

    if (((tex0 >> 34) & 1) == 0) {
        texture.binding.ignore_alpha = true;
        state.texa_aem = false;
        state.texa_ta0 = 0x80;
    }

    return true;
}

gfx::DrawState SpriteState() {
    gfx::DrawState state = Get().draw_state();
    state.blend = true;
    state.alpha_test = false;
    state.depth_test = gfx::DepthTest::Always;
    state.depth_write = false;
    state.fog = false;
    state.cull = gfx::CullMode::None;
    return state;
}

void RestoreTestZbuf() {
    Get().set_test(nullptr);
    Get().set_zbuf(nullptr);
}

float RowScale() { return gfx::CurrentRenderTarget() == gfx::kMainTarget ? 1.0f : 0.5f; }

gfx::Vertex2D Vertex(float x, float y, float z, float u, float v, u_char r, u_char g, u_char b, u_char a,
                     u_char fog) {
    gfx::Vertex2D vertex = {};
    vertex.x = x;
    vertex.y = y;
    vertex.z = z;
    vertex.u = u;
    vertex.v = v;
    vertex.color[0] = r;
    vertex.color[1] = g;
    vertex.color[2] = b;
    vertex.color[3] = a;
    vertex.fog = fog;
    return vertex;
}

constexpr float kEdgeSettle = 1.0f / 256.0f;

bool ScaledOnMainTarget() {
    if (gfx::CurrentRenderTarget() != gfx::kMainTarget) {
        return false;
    }
    gfx::LogicalMapping mapping = gfx::GetUiMapping(gfx::kMainTarget);
    return mapping.scale_x != 1.0f || mapping.scale_y != 1.0f;
}

void SettleEdgeSamples(gfx::Primitive primitive, std::span<gfx::Vertex2D> vertices) {
    // A strip of four vertices is one sprite; a longer one is not a sprite to settle.
    bool sprites = primitive == gfx::Primitive::Quads ||
                   (primitive == gfx::Primitive::TriangleStrip && vertices.size() == 4);
    if (!sprites) {
        return;
    }
    for (size_t first = 0; first + 4 <= vertices.size(); first += 4) {
        std::span<gfx::Vertex2D> sprite = vertices.subspan(first, 4);
        float                    u_min = sprite[0].u;
        float                    v_min = sprite[0].v;
        for (const gfx::Vertex2D &vertex : sprite) {
            u_min = std::min(u_min, vertex.u);
            v_min = std::min(v_min, vertex.v);
        }
        for (gfx::Vertex2D &vertex : sprite) {
            if (vertex.u == u_min) {
                vertex.u += kEdgeSettle;
            }
            if (vertex.v == v_min) {
                vertex.v += kEdgeSettle;
            }
        }
    }
}

void DrawTextured(gfx::Primitive primitive, std::span<gfx::Vertex2D> vertices, u_long tex0, u_long tex1,
                  gfx::DrawState state) {
    Texture texture;
    if (vertices.empty() || !Resolve(tex0, tex1, state, texture)) {
        return;
    }

    if (texture.binding.filter == gfx::Filter::Nearest && ScaledOnMainTarget()) {
        SettleEdgeSamples(primitive, vertices);
    }
    const float rows = RowScale();
    for (gfx::Vertex2D &vertex : vertices) {
        vertex.y *= rows;
        vertex.v *= texture.v_scale;
    }
    textwipe::Draw2D(primitive, vertices, texture.binding, state);
}

void DrawUntextured(gfx::Primitive primitive, std::span<gfx::Vertex2D> vertices,
                    const gfx::DrawState &state) {
    if (vertices.empty()) {
        return;
    }

    const float rows = RowScale();
    for (gfx::Vertex2D &vertex : vertices) {
        vertex.y *= rows;
    }
    textwipe::Draw2D(primitive, vertices, {}, state);
}

} // namespace draw2d
