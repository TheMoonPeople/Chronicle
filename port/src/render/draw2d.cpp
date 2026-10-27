#include "render/draw2d.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <span>

#include "mglib.hpp"
#include "rect.hpp"
#include "render/gs_state.hpp"
#include "render/screen.hpp"
#include "snd.hpp"
#include "text/textwipe.hpp"
#include "texture.hpp"

namespace dc::render::draw2d {

using namespace dc::game;
using namespace dc::text;

namespace {

gfx::DrawState RealDrawState() { return MGPortDrawState(); }

PortTextureRef RealTexture(u_long tex0, u_long tex1) { return PortTextureFromTex0(tex0, tex1); }

CTexture *RealFindTexture(const char *name) { return TexManager.GetTexture(const_cast<char *>(name), -1); }

constexpr Services kRealServices = {
    .draw_state = RealDrawState,
    .texture = RealTexture,
    .find_texture = RealFindTexture,
    .set_test = MGSetGsTEST,
    .set_zbuf = MGSetGsZBUF,
    .set_alpha = MGSetGsALPHA,
    .set_texa = MGSetGsTEXA,
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

namespace {

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

} // namespace

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

namespace {

// The TEX1 snd's sprites write: LCM 1, MMIN 1 and MMAG from setbilinear.
u_long SpriteTex1() { return (static_cast<u_long>(PortBilinear()) << 5) | 0x41; }

} // namespace

void TurnedSprite(CTexture *texture, const CRect_i_ &screen, const CRect_i_ &texel, int pivot_x, int pivot_y,
                  float angle, float scale) {
    if (texture == nullptr) {
        return;
    }

    std::array<float, 4> x{};
    std::array<float, 4> y{};

    x[0] = x[2] = static_cast<float>(-pivot_x);
    x[1] = x[3] = static_cast<float>((screen.x + screen.width + 1) - (screen.x + pivot_x));
    y[0] = y[1] = static_cast<float>(-pivot_y);
    y[2] = y[3] = static_cast<float>((screen.y + screen.height + 1) - (screen.y + pivot_y));

    for (int i = 0; i < 4; i++) {
        const float turned_x = y[i] * cosf(angle) + x[i] * sinf(angle);
        const float turned_y = x[i] * cosf(angle) - y[i] * sinf(angle);

        x[i] = turned_x * scale + static_cast<float>(screen.x);
        y[i] = turned_y * scale + static_cast<float>(screen.y);
    }

    const auto u0 = static_cast<float>(texel.x);
    const auto v0 = static_cast<float>(texel.y);
    const auto v1 = static_cast<float>(texel.y + texel.height);

    // The second corner's u spans the screen width, not the texel width, as in retail.
    std::array<gfx::Vertex2D, 4> strip = {
        draw2d::Vertex(x[0], y[0], 0.0f, u0, v0, 0x80, 0x80, 0x80, 0x80),
        draw2d::Vertex(x[1], y[1], 0.0f, static_cast<float>(texel.x + screen.width), v0, 0x80, 0x80, 0x80,
                       0x80),
        draw2d::Vertex(x[2], y[2], 0.0f, u0, v1, 0x80, 0x80, 0x80, 0x80),
        draw2d::Vertex(x[3], y[3], 0.0f, static_cast<float>(texel.x + texel.width), v1, 0x80, 0x80, 0x80,
                       0x80),
    };
    draw2d::DrawTextured(gfx::Primitive::TriangleStrip, strip, texture->tex0, SpriteTex1(),
                         draw2d::SpriteState());
    draw2d::RestoreTestZbuf();
}

} // namespace dc::render::draw2d
