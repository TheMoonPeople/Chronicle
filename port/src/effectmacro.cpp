#include "effectmacro.hpp"

#include <libgraph.h>
#include <libvu0.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <vector>

#include "draw2d_port.hpp"
#include "draw3d.hpp"
#include "mglib.hpp"
#include "mglib_port.hpp"
#include "rect.hpp"
#include "screen.hpp"
#include "texture.hpp"

namespace {

// The jitter table. Retail reads rd[i][j + 1] for j up to 14, which runs one entry past the end of
// the last row; the spare entry stands in for that word and stays zero.
float g_jitter[21 * 15 + 1];

float &Jitter(int i, int j) { return g_jitter[i * 15 + j]; }

// The edit camera's near clip. The heat haze reaches the bottom of the view from here, so an
// object in front of the focus planes still takes up as much of the wobble as its depth gives.
constexpr float kHazeNear = 10.0f;

// The columns of the logical frame the effect covers (retail's screen was the frame; a target that
// shows past it is covered to its edges) and the texel width of the blurred image laid over them.
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
    const uint8_t clear[4] = {};
    gfx::SetRenderTarget(beyond);
    gfx::Clear(true, clear, false, 0.0f);
    gfx::DrawState state;
    state.depth_test = gfx::DepthTest::GEqual;
    state.texa_aem = false;
    state.texa_ta0 = 0x80;
    state.blend = feather;
    state.alpha = {0, 2, 2, 1, 0x80};
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
    state.alpha = {2, 1, 0, 1, 0x80};
    gfx::Draw2D(gfx::Primitive::Triangles, triangles, binding, state);
    for (gfx::Vertex2D &vertex : triangles) {
        vertex.color[0] = vertex.color[1] = vertex.color[2] = vertex.color[3];
        vertex.color[3] = 0x80;
    }
    state.alpha = {0, 2, 2, 1, 0x80};
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

// Retail shrinks the frame into frame_image twice (half width, then quarter width) and draws the
// copies back over it in bands whose depth is that of the focus planes, alpha blended and depth
// tested GEQUAL without writes: only what lies beyond a plane takes the blurred copy. The first
// pass alternates the near plane's two depths column by column, the second the far plane's, and
// with blur the second pass's columns wander sideways by up to blur sixteenths of a pixel (the
// towns' heat haze).
//
// The copies held the whole frame, so whatever stood nearer than a plane was blurred into the
// scenery around it, and the quarter-width copy came back as blocks. The ordinary depth-of-field
// passes retain their half- and quarter-size images of only the scenery beyond each plane. Heat
// haze instead uses two full-resolution targets sharing the frame's depth buffer. Each target gains
// coverage over eight depth planes. The wobble's target spans from the camera's near clip to beyond
// the far focus plane, its planes spaced in 1/z so the haze ramps up evenly down the frame: every
// object takes up as much of the wobble as its depth gives, and the background haze blends into the
// foreground rather than starting at a line partway up the view. The two outermost columns do not
// wander past the edges of the image.
PORT_OVERRIDE void DepthOfField(float *focus, int level, int alpha, int blur) {
    int phase;
    int i;
    int j;
    int k;

    gfx::TextureHandle target = gfx::CurrentRenderTarget();
    bool               haze = blur > 0;
    gfx::LogicalRect   visible = gfx::VisibleLogicalRect(target);
    int                left = std::min(0, static_cast<int>(std::floor(visible.x)));
    int                right = std::max(0x280, static_cast<int>(std::ceil(visible.x + visible.w)));
    uint32_t           half_width = static_cast<uint32_t>(right - left + 1) / 2;
    uint32_t           half_height = SCREEN_HALF_HEIGHT;
    uint32_t           quarter_width = (half_width + 1) / 2;
    uint32_t           quarter_height = half_height / 2;
    Span               span = {static_cast<float>(left), static_cast<float>(right),
                               haze ? static_cast<float>(left) : 0.0f,
                               haze ? static_cast<float>(right - left) : static_cast<float>(half_width),
                               haze ? 2.0f : 1.0f};

    sceVu0FVECTOR depth_point[4] = {
        {0.0f, 0.0f, focus[0],         1.0f},
        {0.0f, 0.0f, focus[0] + 30.0f, 1.0f},
        {0.0f, 0.0f, focus[1],         1.0f},
        {0.0f, 0.0f, focus[1] + 20.0f, 1.0f},
    };
    int screen[4][4] = {};

    for (i = 0; i < 4; i++) {
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

    for (phase = 0; phase < 1; phase++) {
        for (k = 0; k < 8; k++) {
            strip.clear();

            for (j = 0; j < 41; j++) {
                int   x = (j << 8) + 0x6C00;
                int   v = k << 9;
                int   v_bottom = std::min(v + 0x200, SCREEN_HALF_HEIGHT << 4);
                float across = static_cast<float>(j) / 40.0f;
                strip.push_back(StripVertex(span, x, v + GS_Y_OFFSET, screen[(j + phase) % 2][2], across, v, alpha));
                strip.push_back(StripVertex(span, x, v_bottom + GS_Y_OFFSET, screen[(j + phase) % 2][2], across,
                                            v_bottom, alpha));
            }

            AppendStrip(triangles, strip);
        }
    }

    gfx::DrawState state = services.draw_state();

    if (ready) {
        DrawBands(triangles, near_image, state);
    }

    triangles.clear();

    if (level >= 2) {
        if (blur > 0) {
            for (i = 0; i < 21; i++) {
                for (j = 0; j < 15; j++) {
                    if (blur > 0) {
                        Jitter(i, j) += 0.2f * (static_cast<float>(blur) * (static_cast<float>(rand()) / 2147483648.0f - 0.5f));

                        if (Jitter(i, j) < 0.0f) {
                            Jitter(i, j) = 0.0f;
                        }

                        if (Jitter(i, j) > static_cast<float>(blur)) {
                            Jitter(i, j) = static_cast<float>(blur);
                        }
                    } else {
                        Jitter(i, j) = 0.0f;
                    }
                }
            }
        }

        if (blur > 0) {
            alpha = 0x80;
        }

        for (j = 0; j < SCREEN_HALF_HEIGHT / 16; j++) {
            strip.clear();

            for (i = 0; i < 21; i++) {
                int   x = (i << 9) + 0x6C00;
                int   v = j << 8;
                int   y = v + GS_Y_OFFSET;
                int   y_bottom = y + 0x100;
                float across = static_cast<float>(i) / 20.0f;
                int   v_bottom = v + 0x100;

                if (blur > 0 && i > 0 && i < 20) {
                    strip.push_back(
                        StripVertex(span, x - static_cast<int>(Jitter(i, j)), y, screen[2 + i % 2][2], across, v, alpha));
                    strip.push_back(StripVertex(span, x - static_cast<int>(Jitter(i, j + 1)), y_bottom, screen[2 + i % 2][2],
                                                across, v_bottom, alpha));
                } else {
                    strip.push_back(StripVertex(span, x, y, screen[2 + i % 2][2], across, v, alpha));
                    strip.push_back(StripVertex(span, x, y_bottom, screen[2 + i % 2][2], across, v_bottom, alpha));
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
