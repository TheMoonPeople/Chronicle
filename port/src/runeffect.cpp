#include "runeffect.hpp"

#include <libgraph.h>

#include <array>
#include <cstdlib>
#include <optional>
#include <vector>

#include "draw2d_port.hpp"
#include "mglib.hpp"
#include "mglib_port.hpp"
#include "rect.hpp"
#include "snd.hpp"
#include "texture.hpp"

namespace {

// TEX1 0x61: LCM 1, linear magnification and minification.
constexpr u_long kTex1Linear = 0x61;

// The render target the game names by FRAME_1. FRAME_1 holds the base in pages (TBP0 / 32); the
// registry's keys are not page multiples, so the key is taken whole from the caller's TBP0.
class TargetScope {
public:
    TargetScope(int destination, int width, int format) {
        u_long         tex0 = SCE_GS_SET_TEX0(static_cast<u_long>(destination), static_cast<u_long>(width),
                                              static_cast<u_long>(format), 10, 10, 1, 0, 0, 0, 0, 0, 0);
        PortTextureRef ref = draw2d::Get().texture(tex0, 0);
        if (!ref.valid || ref.binding.texture == gfx::kNullTexture || ref.binding.texture == gfx::kPreviousFrame) {
            return;
        }
        if (ref.binding.texture != gfx::kMainTarget) {
            std::optional<gfx::TextureInfo> info = gfx::GetTextureInfo(ref.binding.texture);
            if (!info || !info->render_target) {
                return;
            }
        }
        previous_ = gfx::CurrentRenderTarget();
        gfx::SetRenderTarget(ref.binding.texture);
        bound_ = true;
    }

    ~TargetScope() {
        if (bound_) {
            gfx::SetRenderTarget(previous_);
        }
    }

    TargetScope(const TargetScope &) = delete;
    TargetScope &operator=(const TargetScope &) = delete;

    bool Bound() const { return bound_; }

private:
    gfx::TextureHandle previous_ = gfx::kMainTarget;
    bool               bound_ = false;
};

// The current registers as a draw on the bound target: the window scissor follows the target's
// rows, as the GS clips with SCISSOR_1 in the bound frame's own rows.
gfx::DrawState TargetState(bool blend) {
    gfx::DrawState state = draw2d::Get().draw_state();
    state.blend = blend;
    state.fog = false;
    state.cull = gfx::CullMode::None;
    const float rows = draw2d::RowScale();
    state.scissor_rect.y *= rows;
    state.scissor_rect.h *= rows;
    return state;
}

// A GS sprite from 12.4 window coordinates to 12.4 texel coordinates, as a quad on the bound target.
std::array<gfx::Vertex2D, 4> GsSprite(int x0, int y0, int x1, int y1, int u0, int v0, int u1, int v1,
                                      float v_scale) {
    const float rows = draw2d::RowScale();
    const float left = MGPortLogicalX(x0);
    const float right = MGPortLogicalX(x1);
    const float top = MGPortLogicalY(y0) * rows;
    const float bottom = MGPortLogicalY(y1) * rows;
    const float s0 = static_cast<float>(u0) / 16.0f;
    const float s1 = static_cast<float>(u1) / 16.0f;
    const float t0 = static_cast<float>(v0) / 16.0f * v_scale;
    const float t1 = static_cast<float>(v1) / 16.0f * v_scale;
    return {
        draw2d::Vertex(left, top, 0.0f, s0, t0, 0x80, 0x80, 0x80, 0x80),
        draw2d::Vertex(right, top, 0.0f, s1, t0, 0x80, 0x80, 0x80, 0x80),
        draw2d::Vertex(right, bottom, 0.0f, s1, t1, 0x80, 0x80, 0x80, 0x80),
        draw2d::Vertex(left, bottom, 0.0f, s0, t1, 0x80, 0x80, 0x80, 0x80),
    };
}

void TexturedSprite(u_long tex0, const CRect_i_ &destination, const CRect_i_ &source, bool blend) {
    gfx::DrawState  state = TargetState(blend);
    draw2d::Texture texture;
    if (!draw2d::Resolve(tex0, kTex1Linear, state, texture)) {
        return;
    }
    std::array<gfx::Vertex2D, 4> quad =
        GsSprite((destination.x << 4) + 0x6C00, (destination.y << 4) + GS_Y_OFFSET,
                 ((destination.x + destination.width) << 4) + 0x6C00,
                 ((destination.y + destination.height) << 4) + GS_Y_OFFSET, source.x << 4, source.y << 4,
                 (source.x + source.width) << 4, (source.y + source.height) << 4, texture.v_scale);
    gfx::Draw2D(gfx::Primitive::Quads, quad, texture.binding, state);
}

void RestoreRegisters() {
    const draw2d::Services &services = draw2d::Get();
    services.set_test(nullptr);
    services.set_zbuf(nullptr);
    services.set_alpha(nullptr);
}

} // namespace

// Into the target: a flat 0x80 sprite over the first rect (retail's PRIM there has TME off, so its
// UVs, the bottom one taken from x, sample nothing), the first texture blended over it with the
// ALPHA the caller left, then the second added as Cs + Cd * As.
PC_OVERRIDE void blendTextuer(sceVif1Packet *packet, int destination, int width, int format, CTexture *first_texture,
                              const CRect_i_ &first_destination, const CRect_i_ &first_source, CTexture *second_texture,
                              const CRect_i_ &second_destination, const CRect_i_ &second_source) {
    const draw2d::Services &services = draw2d::Get();

    {
        TargetScope target(destination, width, format);

        sceGsTest test = mgPixelTest;
        test.bits.ate = 0;
        test.bits.zte = 1;
        test.bits.ztst = 1;
        services.set_test(&test);
        sceGsZbuf zbuffer = mgZBuffer;
        zbuffer.bits.zmsk = 1;
        services.set_zbuf(&zbuffer);

        if (target.Bound()) {
            std::array<gfx::Vertex2D, 4> flat =
                GsSprite((first_destination.x << 4) + 0x6C00, (first_destination.y << 4) + GS_Y_OFFSET,
                         ((first_destination.x + first_destination.width) << 4) + 0x6C00,
                         ((first_destination.y + first_destination.height) << 4) + GS_Y_OFFSET, 0, 0, 0, 0, 1.0f);
            gfx::Draw2D(gfx::Primitive::Quads, flat, {}, TargetState(false));
            TexturedSprite(first_texture->tex0, first_destination, first_source, true);
        }

        sceGsAlpha alpha = mgAlpha;
        alpha.bits.a = 1;
        alpha.bits.b = 2;
        alpha.bits.c = 0;
        alpha.bits.d = 0;
        services.set_alpha(&alpha);

        if (target.Bound()) {
            TexturedSprite(second_texture->tex0, second_destination, second_source, true);
        }
    }

    RestoreRegisters();
}

// Into the target: two-row strips of the frame, each shifted sideways by the wave scaled with depth
// (the shimmer), then the texture's alpha over them with ALPHA (0 - 0) * FIX + Cd, which keeps the
// colour and leaves the texture's alpha behind as the strips' coverage.
PC_OVERRIDE void blendTextuerTest(sceVif1Packet *packet, int destination, int width, int format, const CRect_i_ &source,
                                  CTexture *texture, const CRect_i_ &texture_destination, const CRect_i_ &texture_source,
                                  float depth, float phase) {
    const draw2d::Services &services = draw2d::Get();
    sceGsTex0               frame;
    MGGetFBuffTex(&frame);
    const u_long frame_tex0 = *reinterpret_cast<u_long *>(&frame);
    depth /= 10000;

    {
        sceGsTest test = mgPixelTest;
        test.bits.ate = 0;
        test.bits.zte = 1;
        test.bits.ztst = 1;
        services.set_test(&test);
        sceGsZbuf zbuffer = mgZBuffer;
        zbuffer.bits.zmsk = 1;
        services.set_zbuf(&zbuffer);

        // One snapshot of the frame serves every strip; it is taken before the target is bound.
        gfx::DrawState  resolved = TargetState(false);
        draw2d::Texture frame_texture;
        bool            have_frame = draw2d::Resolve(frame_tex0, kTex1Linear, resolved, frame_texture);

        TargetScope    target(destination, width, format);
        gfx::DrawState strip_state = TargetState(false);
        strip_state.texa_aem = resolved.texa_aem;
        strip_state.texa_ta0 = resolved.texa_ta0;

        std::vector<gfx::Vertex2D> strips;
        int                        top = 0;
        int                        bottom = 0;
        int                        count = 0;
        float                      amplitude = 0.0f;

        for (int y = 0; y < source.height; y += 2) {
            int index = count + static_cast<int>(phase);

            if (index >= 8) {
                index -= 8;
            }

            int offset = static_cast<int>(16.0f * (depth * (amplitude * waveAnimeCnt[index])));
            bottom += 2;
            int left = (source.x << 4) + offset;

            if (left < 0) {
                left = 0;
            }

            int right = offset + ((source.x + source.width) << 4);

            if (right > 0x27F0) {
                right = 0x27F0;
            }

            std::array<gfx::Vertex2D, 4> quad =
                GsSprite(0x6C00, (y << 4) + GS_Y_OFFSET, (source.width << 4) + 0x6C00, ((y + 2) << 4) + GS_Y_OFFSET,
                         left, (source.y + top) << 4, right, (source.y + bottom) << 4, frame_texture.v_scale);
            strips.insert(strips.end(), quad.begin(), quad.end());
            top = bottom;
            count++;

            if (count >= 8) {
                amplitude = 1.3f * rand() / 2147483648.0f;
            }

            if (count >= 8) {
                count = 0;
            }
        }

        if (target.Bound() && have_frame && !strips.empty()) {
            gfx::Draw2D(gfx::Primitive::Quads, strips, frame_texture.binding, strip_state);
        }

        sceGsAlpha alpha = mgAlpha;
        alpha.bits.a = 2;
        alpha.bits.b = 2;
        alpha.bits.c = 2;
        alpha.bits.d = 1;
        services.set_alpha(&alpha);

        if (target.Bound()) {
            TexturedSprite(texture->tex0, texture_destination, texture_source, true);
        }
    }

    RestoreRegisters();
}

// Retail sets ZBUF around the dust with raw register writes; the sprites read the register state.
PC_OVERRIDE void CRunEffect::Draw() {
    int           top_left[4];
    int           bottom_right[4];
    int           top_right[4];
    int           bottom_left[4];
    sceVu0FVECTOR light;

    CTexture *first = TexManager.GetTexture(const_cast<char *>("fx_foot"), -1);
    CTexture *second = TexManager.GetTexture(const_cast<char *>("fx_foot2"), -1);

    if (first == NULL || second == NULL) {
        return;
    }

    sceVu0FVECTOR normal = {0.0f, 1.0f, 0.0f, 0.0f};
    MGCalcColor(light, normal);
    light[0] += 80.0f;
    light[1] += 60.0f;
    light[2] += 40.0f;

    if (!(light[0] <= 255.0f)) {
        light[0] = 255.0f;
    }

    if (!(light[1] <= 255.0f)) {
        light[1] = 255.0f;
    }

    if (!(light[2] <= 255.0f)) {
        light[2] = 255.0f;
    }

    spRGBA colour;

    if (lighting != 0) {
        colour.r = (int) light[0];
        colour.g = (int) light[1];
        colour.b = (int) light[2];
        colour.a = 0x80;
    } else {
        colour.r = 0x80;
        colour.g = 0x80;
        colour.b = 0x80;
        colour.a = 0x80;
    }

    sw ^= 1;

    sceGsZbuf zbuffer = mgZBuffer;
    zbuffer.bits.zmsk = 1;
    draw2d::Get().set_zbuf(&zbuffer);

    for (int i = 0; i < 8; i++) {
        if (life[i] == 0) {
            continue;
        }

        if (MGRotTransPers3DSprite(top_left, bottom_right, position[i], 12.0f - 0.5f * life[i],
                                   5.5f - 0.25f * life[i], 0) != 1) {
            continue;
        }

        top_right[0] = bottom_right[0];
        top_right[1] = top_left[1];
        top_right[2] = top_left[2];
        top_right[3] = top_left[3];
        bottom_left[0] = top_left[0];
        bottom_left[1] = bottom_right[1];
        bottom_left[2] = bottom_right[2];
        bottom_left[3] = bottom_right[3];
        colour.a = life[i] * 10;

        if (sw != 0) {
            set3DSprite(Vif1Packet, first, CRect_i_(0, 0, 0x20, 0x20), top_left, top_right, bottom_left,
                        bottom_right, &colour);
        } else {
            set3DSprite(Vif1Packet, second, CRect_i_(0, 0, 0x20, 0x20), top_left, top_right, bottom_left,
                        bottom_right, &colour);
        }
    }

    draw2d::Get().set_zbuf(nullptr);
}
