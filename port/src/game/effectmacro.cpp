#include "effectmacro.hpp"

#include <libgraph.h>
#include <libvu0.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <vector>

#include "effect.hpp"
#include "effectgroup.hpp"
#include "frame.hpp"
#include "mglib.hpp"
#include "rect.hpp"
#include "render/draw2d.hpp"
#include "render/draw3d.hpp"
#include "render/gs_state.hpp"
#include "render/screen.hpp"
#include "texture.hpp"

namespace {

sceVu0FVECTOR wind_dir = {0.0f, 0.0f, 0.0f, 0.0f};
int           effect_count;
CTexture     *smoke_tex;
CTexture     *sibuki_tex;
CTexture     *hamon_tex;

} // namespace

void EffectMacroStep(float *wind) {
    sceVu0CopyVector(wind_dir, wind);
    effect_count++;
    smoke_tex = TexManager.GetTexture("smoke", -1);
    sibuki_tex = TexManager.GetTexture("shibuki", -1);
    hamon_tex = TexManager.GetTexture("hamon", -1);
}

void EffectSmoke(CEffectGroup *group, float *position, float size, int period) {
    CEffectParam effect;

    // One puff every `period` frames, so a caller can thin its own smoke.
    if (effect_count % period != 0) {
        return;
    }

    effect.texel.x = effect.texel.y = effect.texel.width = effect.texel.height = 0;
    effect.Initialize();
    sceVu0CopyVector(effect.position, position);

    // Half the puffs drift, on a sine of their own in x and z.
    if (std::rand() % 2 != 0) {
        effect.position_oscillation_flags = 1;
        effect.position_oscillation_scale[0] = 0.3f * static_cast<float>(std::rand()) / 2.1474836e9f;
        effect.position_oscillation_rate[0] = PI / (30.0f + static_cast<float>(std::rand() * 20) / 2.1474836e9f);
        effect.position_oscillation_scale[2] = 0.3f * static_cast<float>(std::rand()) / 2.1474836e9f;
        effect.position_oscillation_rate[2] = PI / (30.0f + static_cast<float>(std::rand() * 20) / 2.1474836e9f);
    }

    effect.opacity_mode = EFFECT_OPACITY_FADE_OUT;
    effect.render_flags = 1;
    effect.opacity = 0.1f;
    sceVu0ScaleVector(effect.velocity, effect.velocity, 0.4f);
    effect.velocity[1] += 0.2f + 0.2f * static_cast<float>(std::rand()) / 2.1474836e9f;
    effect.scale_velocity[0] = 0.02f;
    effect.scale_velocity[1] = 0.02f;
    effect.scale[0] = size;
    effect.scale[1] = size;
    effect.lifetime = 0x78;
    effect.texture = smoke_tex;

    // The four puff shapes are the quarters of the texture.
    switch (std::rand() % 4) {
        case 0:
            effect.texel = CRect_i_(0, 0, 0x40, 0x40);
            break;
        case 1:
            effect.texel = CRect_i_(0x40, 0, 0x40, 0x40);
            break;
        case 2:
            effect.texel = CRect_i_(0, 0x40, 0x40, 0x40);
            break;
        case 3:
            effect.texel = CRect_i_(0x40, 0x40, 0x40, 0x40);
            break;
        default:
            break;
    }

    effect.width = 10.0f;
    effect.height = 10.0f;
    group->EnterEffect(&effect);
}

void EffectWaterSpray(CEffectGroup *group, float *position, float *extent, int period, int phase) {
    CEffectParam effect;

    if ((effect_count + phase) % period != 0) {
        return;
    }

    effect.texel.x = effect.texel.y = effect.texel.width = effect.texel.height = 0;
    effect.Initialize();
    sceVu0CopyVector(effect.position, position);
    effect.opacity_mode = EFFECT_OPACITY_FADE_OUT;
    effect.render_flags = 1;
    effect.opacity = 0.5f;

    float spread_x = 0.2f * extent[0];
    float spread_z = 0.2f * extent[2];
    float rise = 0.1f * extent[1] * (1.0f + static_cast<float>(std::rand()) / 2.1474836e9f);
    float width_grown = rise + 0.5f * (extent[0] + extent[2]);
    float height_grown = extent[1] + rise;

    effect.velocity[0] = spread_x * static_cast<float>(std::rand()) / 2.1474836e9f - 0.5f * spread_x;
    effect.velocity[2] = (spread_x * static_cast<float>(std::rand()) / 2.1474836e9f - 0.5f * spread_z) - 0.5f;
    effect.velocity[1] = 1.0f + 0.5f * static_cast<float>(std::rand()) / 2.1474836e9f;
    effect.acceleration[1] = -0.03f;
    effect.scale_velocity[0] = width_grown / 30.0f;
    effect.scale_velocity[1] = height_grown / 30.0f;
    effect.scale[0] = 0.2f * width_grown;
    effect.scale[1] = 0.2f * height_grown;
    effect.lifetime = 0x28;
    effect.texture = sibuki_tex;
    effect.texel = CRect_i_(0, 0, 0x40, 0x40);
    effect.width = 10.0f;
    effect.height = 10.0f;
    group->EnterEffect(&effect);
}

void EffectHamon(CEffectGroup *group, float *position, float size) {
    CEffectParam effect;

    effect.texel.x = effect.texel.y = effect.texel.width = effect.texel.height = 0;
    effect.Initialize();
    sceVu0CopyVector(effect.position, position);
    effect.opacity_mode = EFFECT_OPACITY_FADE_OUT;
    effect.render_flags = 1;
    effect.opacity = 1.0f;
    effect.draw_mode = 1;
    effect.lifetime = 0x28;
    // The ring starts at nothing and spreads for its whole life.
    effect.scale_velocity[0] = 0.025f;
    effect.scale_velocity[1] = 0.025f;
    effect.scale_velocity[2] = 0.025f;
    effect.scale[0] = 0.0f;
    effect.scale[1] = 0.0f;
    effect.scale[2] = 0.0f;
    effect.texture = hamon_tex;
    effect.texel = CRect_i_(0, 0, 0x20, 0x20);
    effect.width = size;
    effect.height = size;
    group->EnterEffect(&effect);
}

namespace {

// 21 columns of 15 rows, row-major; a band's bottom edge at row 15 reads the next column's first
// entry, and the spare entry keeps such a read in bounds past the last column.
std::array<float, (21 * 15) + 1> g_jitter{};

float &Jitter(int column, int row) { return g_jitter[(column * 15) + row]; }

// The edit camera's near clip. The heat haze reaches the bottom of the view from here, so an
// object in front of the focus planes still takes up as much of the wobble as its depth gives.
constexpr float kHazeNear = 10.0f;

// The columns of the logical frame the effect covers (a target that shows past the frame is covered
// to its edges) and the texel width of the blurred image laid over them.
struct Span {
    float left;
    float right;
    float sample_left;
    float texels;
    float row_scale;
};

// A band vertex: x in 12.4 GS units of the logical frame, stretched over the span; across is how
// far over the blurred image it samples, v its row there.
gfx::Vertex2D StripVertex(const Span &span, int x, int y, unsigned z, float across, int v, int alpha) {
    const float place = MGPortLogicalX(x) / kScreenWidth;
    return draw2d::Vertex(span.left + place * (span.right - span.left), MGPortLogicalY(y) * draw2d::RowScale(),
                          MGPortDepth(z & 0xFFFFFF), span.sample_left + across * span.texels,
                          static_cast<float>(v) / 16.0f * span.row_scale, 0x80,
                          0x80, 0x80, static_cast<u_char>(alpha));
}

std::array<gfx::Vertex2D, 4> Quad(float x0, float y0, float x1, float y1, float z, float u0, float v0, float u1,
                                  float v1) {
    return {
        draw2d::Vertex(x0, y0, z, u0, v0, 0x80, 0x80, 0x80, 0x80),
        draw2d::Vertex(x1, y0, z, u1, v0, 0x80, 0x80, 0x80, 0x80),
        draw2d::Vertex(x1, y1, z, u1, v1, 0x80, 0x80, 0x80, 0x80),
        draw2d::Vertex(x0, y1, z, u0, v1, 0x80, 0x80, 0x80, 0x80),
    };
}

// Accumulate the frame beyond several nearby depth planes into a premultiplied image. With heat
// haze, the planes are spread from `front` to `back` so the effect gains coverage gradually from
// the nearest depth it reaches to the farthest; the depths are spaced in 1/z, which is what the
// screen's rows are linear in, so the haze ramps at an even rate down the frame instead of fading
// out in a band of a few rows. Each plane contributes an equal fraction.
void DrawBeyond(gfx::TextureHandle beyond, gfx::TextureHandle snapshot, float front, float back, bool feather) {
    const std::array<uint8_t, 4> clear{};
    gfx::SetRenderTarget(beyond);
    gfx::Clear(true, clear.data(), false, 0.0f);
    gfx::DrawState state;
    state.depth_test = gfx::DepthTest::GEqual;
    state.texa_aem = false;
    state.texa_ta0 = 0x80;
    state.blend = feather;
    state.alpha = {.a = 0, .b = 2, .c = 2, .d = 1, .fix = 0x80};
    gfx::TextureBinding binding;
    binding.texture = snapshot;
    binding.filter = gfx::Filter::Nearest;
    const int samples = feather ? 8 : 1;
    for (int i = 0; i < samples; i++) {
        float         t = static_cast<float>(i) / 7.0f;
        float         plane = feather ? 1.0f / (1.0f / front + (1.0f / back - 1.0f / front) * t) : front;
        sceVu0FVECTOR point = {0.0f, 0.0f, plane, 1.0f};
        sceVu0ApplyMatrix(point, mgRenderInfo.screen, point);
        point[2] /= point[3];
        float                        depth = MGPortDepth(static_cast<unsigned>(static_cast<int>(point[2])) & 0xFFFFFF);
        std::array<gfx::Vertex2D, 4> quad = Quad(0.0f, 0.0f, kScreenWidth, kScreenHeight, depth, 0.0f,
                                                 0.0f, kScreenWidth, kScreenHeight);
        if (feather) {
            for (gfx::Vertex2D &vertex : quad) {
                vertex.color[0] = vertex.color[1] = vertex.color[2] = 0x10;
                vertex.color[3] = static_cast<u_char>((i + 1) * 0x10);
            }
        }
        gfx::Draw2D(gfx::Primitive::Quads, quad, binding, state);
    }
}

// src's texel rect resized over all of a width by height target.
void Resize(gfx::TextureHandle src, float u0, float v0, float u1, float v1, gfx::TextureHandle dst, uint32_t width,
            uint32_t height) {
    gfx::SetRenderTarget(dst);
    gfx::TextureBinding binding;
    binding.texture = src;
    binding.filter = gfx::Filter::Linear;
    std::array<gfx::Vertex2D, 4> quad =
        Quad(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, u0, v0, u1, v1);
    gfx::Draw2D(gfx::Primitive::Quads, quad, binding, gfx::DrawState{});
}

// The bands over the frame from a premultiplied image at the vertices' alpha: the frame is first
// scaled down by the image's coverage, then the image added, so where the image is transparent
// (beside something nearer than the plane) the frame keeps its own colour.
void DrawBands(std::vector<gfx::Vertex2D> &triangles, gfx::TextureHandle image, gfx::DrawState state) {
    if (triangles.empty()) {
        return;
    }
    gfx::TextureBinding binding;
    binding.texture = image;
    binding.filter = gfx::Filter::Linear;
    state.blend = true;
    state.fog = false;
    state.cull = gfx::CullMode::None;
    state.alpha = {.a = 2, .b = 1, .c = 0, .d = 1, .fix = 0x80};
    gfx::Draw2D(gfx::Primitive::Triangles, triangles, binding, state);
    for (gfx::Vertex2D &vertex : triangles) {
        vertex.color[0] = vertex.color[1] = vertex.color[2] = vertex.color[3];
        vertex.color[3] = 0x80;
    }
    state.alpha = {.a = 0, .b = 2, .c = 2, .d = 1, .fix = 0x80};
    gfx::Draw2D(gfx::Primitive::Triangles, triangles, binding, state);
}

// One GS triangle strip as a triangle list, so that every band of the effect goes in one draw.
void AppendStrip(std::vector<gfx::Vertex2D> &triangles, const std::vector<gfx::Vertex2D> &strip) {
    for (size_t i = 2; i < strip.size(); i++) {
        triangles.push_back(strip[i - 2]);
        triangles.push_back(strip[i - 1]);
        triangles.push_back(strip[i]);
    }
}

} // namespace

// Blurred images of the scenery beyond each focus plane are drawn back over the frame in bands
// whose depth is that of the plane, alpha blended and depth tested GEQUAL without writes, so only
// what lies beyond a plane takes the blur. The near pass alternates the near plane's two depths
// column by column, the far pass the far plane's; with blur the far pass's columns wander sideways
// by up to blur sixteenths of a pixel (the towns' heat haze).
//
// Plain depth of field uses half- and quarter-size images. Heat haze uses two full-resolution
// targets sharing the frame's depth buffer, each gaining coverage over eight depth planes; the
// wobble's target spans from the camera's near clip to beyond the far focus plane, so every object
// takes up as much of the wobble as its depth gives. The two outermost columns do not wander past
// the edges of the image.
void DepthOfField(float *focus, int level, int alpha, int blur) {
    gfx::TextureHandle target = gfx::CurrentRenderTarget();
    bool               haze = blur > 0;
    gfx::LogicalRect   visible = gfx::VisibleLogicalRect(target);
    int                left = std::min(0, static_cast<int>(std::floor(visible.x)));
    int                right = std::max(0x280, static_cast<int>(std::ceil(visible.x + visible.w)));
    uint32_t           half_width = static_cast<uint32_t>(right - left + 1) / 2;
    uint32_t           half_height = SCREEN_HALF_HEIGHT;
    uint32_t           quarter_width = (half_width + 1) / 2;
    uint32_t           quarter_height = half_height / 2;
    Span               span = {
        .left = static_cast<float>(left),
        .right = static_cast<float>(right),
        .sample_left = haze ? static_cast<float>(left) : 0.0f,
        .texels = haze ? static_cast<float>(right - left) : static_cast<float>(half_width),
        .row_scale = haze ? 2.0f : 1.0f,
    };

    sceVu0FVECTOR depth_point[4] = {
        {0.0f, 0.0f, focus[0],         1.0f},
        {0.0f, 0.0f, focus[0] + 30.0f, 1.0f},
        {0.0f, 0.0f, focus[1],         1.0f},
        {0.0f, 0.0f, focus[1] + 20.0f, 1.0f},
    };
    int screen[4][4] = {};

    for (int i = 0; i < 4; i++) {
        sceVu0ApplyMatrix(depth_point[i], mgRenderInfo.screen, depth_point[i]);
        depth_point[i][2] /= depth_point[i][3];
        screen[i][2] = static_cast<int>(depth_point[i][2]);
    }

    if (haze) {
        // The wobble's image spans from the near clip to beyond the far focus plane, so the haze
        // ramps in over the whole view rather than starting at a line partway up the frame; its
        // bands test at that image's front. The near pass keeps its own front.
        sceVu0FVECTOR near_front = {0.0f, 0.0f, focus[0] - 84.0f, 1.0f};
        sceVu0ApplyMatrix(near_front, mgRenderInfo.screen, near_front);
        screen[0][2] = screen[1][2] = static_cast<int>(near_front[2] / near_front[3]);
        sceVu0FVECTOR far_front = {0.0f, 0.0f, kHazeNear, 1.0f};
        sceVu0ApplyMatrix(far_front, mgRenderInfo.screen, far_front);
        screen[2][2] = screen[3][2] = static_cast<int>(far_front[2] / far_front[3]);
    }

    gfx::TextureHandle snapshot = gfx::NamedRenderTarget("dof frame", 0x280, SCREEN_HEIGHT, false, false, true);
    gfx::TextureHandle beyond = haze ? gfx::kNullTexture : gfx::NamedRenderTarget("dof beyond", 0x280, SCREEN_HEIGHT, true, true);
    gfx::TextureHandle near_image = haze ? gfx::NamedRenderTarget("dof near", 0x280, SCREEN_HEIGHT, true, true) : gfx::NamedRenderTarget("dof near", half_width, half_height, true);
    gfx::TextureHandle far_image = gfx::kNullTexture;
    bool               ready = snapshot != gfx::kNullTexture && (haze || beyond != gfx::kNullTexture) &&
                               near_image != gfx::kNullTexture && target == gfx::kMainTarget &&
                               gfx::SnapshotFrame(snapshot);

    if (ready) {
        DrawBeyond(haze ? near_image : beyond, snapshot, haze ? focus[0] - 84.0f : focus[0],
                   haze ? focus[0] + 84.0f : focus[0], haze);
        if (!haze) {
            Resize(beyond, span.left, 0.0f, span.right, kScreenHeight, near_image, half_width, half_height);
        }

        if (level >= 2) {
            if (haze) {
                far_image = gfx::NamedRenderTarget("dof far", 0x280, SCREEN_HEIGHT, true, true);
                if (far_image != gfx::kNullTexture) {
                    DrawBeyond(far_image, snapshot, kHazeNear, focus[1] + 84.0f, true);
                }
            } else {
                gfx::TextureHandle half = gfx::NamedRenderTarget("dof far half", half_width, half_height, true);
                gfx::TextureHandle quarter = gfx::NamedRenderTarget("dof far quarter", quarter_width, quarter_height, true);
                far_image = gfx::NamedRenderTarget("dof far", half_width, half_height, true);

                if (half != gfx::kNullTexture && quarter != gfx::kNullTexture && far_image != gfx::kNullTexture) {
                    DrawBeyond(beyond, snapshot, focus[1], focus[1], false);
                    Resize(beyond, span.left, 0.0f, span.right, kScreenHeight, half, half_width, half_height);
                    Resize(half, 0.0f, 0.0f, static_cast<float>(half_width), static_cast<float>(half_height), quarter,
                           quarter_width, quarter_height);
                    Resize(quarter, 0.0f, 0.0f, static_cast<float>(quarter_width), static_cast<float>(quarter_height),
                           far_image, half_width, half_height);
                } else {
                    far_image = gfx::kNullTexture;
                }
            }
        }

        gfx::SetRenderTarget(target);
    }

    MGPortRestoreRegisters();
    MGPortCurrent().texa.AEM = 1;
    MGPortCurrent().texa.TA0 = 128;

    const draw2d::Services &services = draw2d::Get();
    sceGsTest               test = mgPixelTest;
    test.bits.ate = 0;
    test.bits.aref = 0;
    test.bits.atst = 1;
    test.bits.zte = 1;
    test.bits.ztst = 2;
    services.set_test(&test);
    sceGsZbuf zbuffer = mgZBuffer;
    zbuffer.bits.zmsk = 1;
    services.set_zbuf(&zbuffer);

    std::vector<gfx::Vertex2D> triangles;
    std::vector<gfx::Vertex2D> strip;

    for (int row = 0; row < 8; row++) {
        strip.clear();

        for (int column = 0; column < 41; column++) {
            const int   x = (column << 8) + 0x6C00;
            const int   v = row << 9;
            const int   v_bottom = std::min(v + 0x200, SCREEN_HALF_HEIGHT << 4);
            const float across = static_cast<float>(column) / 40.0f;
            const int   z = screen[column % 2][2];
            strip.push_back(StripVertex(span, x, v + GS_Y_OFFSET, z, across, v, alpha));
            strip.push_back(StripVertex(span, x, v_bottom + GS_Y_OFFSET, z, across, v_bottom, alpha));
        }

        AppendStrip(triangles, strip);
    }

    gfx::DrawState state = services.draw_state();

    if (ready) {
        DrawBands(triangles, near_image, state);
    }

    triangles.clear();

    if (level >= 2) {
        if (blur > 0) {
            for (int column = 0; column < 21; column++) {
                for (int row = 0; row < 15; row++) {
                    float &jitter = Jitter(column, row);
                    jitter += 0.2f * (static_cast<float>(blur) *
                                      ((static_cast<float>(std::rand()) / 2147483648.0f) - 0.5f));
                    jitter = std::max(jitter, 0.0f);
                    jitter = std::min(jitter, static_cast<float>(blur));
                }
            }
        }

        if (blur > 0) {
            alpha = 0x80;
        }

        for (int row = 0; row < SCREEN_HALF_HEIGHT / 16; row++) {
            strip.clear();

            for (int column = 0; column < 21; column++) {
                const int   x = (column << 9) + 0x6C00;
                const int   v = row << 8;
                const int   y = v + GS_Y_OFFSET;
                const int   y_bottom = y + 0x100;
                const float across = static_cast<float>(column) / 20.0f;
                const int   v_bottom = v + 0x100;
                const int   z = screen[2 + (column % 2)][2];

                if (blur > 0 && column > 0 && column < 20) {
                    strip.push_back(
                        StripVertex(span, x - static_cast<int>(Jitter(column, row)), y, z, across, v, alpha));
                    strip.push_back(StripVertex(span, x - static_cast<int>(Jitter(column, row + 1)), y_bottom, z,
                                                across, v_bottom, alpha));
                } else {
                    strip.push_back(StripVertex(span, x, y, z, across, v, alpha));
                    strip.push_back(StripVertex(span, x, y_bottom, z, across, v_bottom, alpha));
                }
            }

            AppendStrip(triangles, strip);
        }
    }

    if (ready && far_image != gfx::kNullTexture) {
        DrawBands(triangles, far_image, state);
    }

    services.set_test(nullptr);
    services.set_zbuf(nullptr);
}
