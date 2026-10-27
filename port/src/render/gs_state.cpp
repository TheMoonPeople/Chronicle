#include "render/gs_state.hpp"

#include <algorithm>
#include <bit>
#include <optional>

#include "mglib.hpp"
#include "render/draw3d.hpp"
#include "render/screen.hpp"

namespace dc::render {

using namespace dc::game;

namespace {

// TEXA as vudata.cpp's My_DrawEnv leaves it before the game sends its own.
constexpr sceGsTexa InitialTexa() {
    sceGsTexa texa = {};
    texa.TA0 = 0x80;
    texa.AEM = 1;
    return texa;
}

constinit MGPortRegisters g_current = {
    .test = {},
    .zbuf = {},
    .alpha = {},
    .texa = InitialTexa(),
    .window = {.x = 0.0f, .y = 0.0f, .w = kScreenWidth, .h = kScreenHeight},
};

constexpr int kGsOriginX = 27648;

bool g_cut_next_tick = true;

} // namespace

MGPortRegisters &MGPortCurrent() {
    return g_current;
}

void MGPortRestoreRegisters() {
    g_current.test = mgPixelTest;
    g_current.zbuf = mgZBuffer;
    g_current.alpha = mgAlpha;
}

gfx::DrawState MGPortDrawState() {
    gfx::DrawState state;
    MGPortApplyTest(state, g_current.test);
    MGPortApplyZbuf(state, g_current.zbuf);
    MGPortApplyAlpha(state, g_current.alpha);
    MGPortApplyTexa(state, g_current.texa);
    state.fog_color[0] = mgRenderInfo.fog_red;
    state.fog_color[1] = mgRenderInfo.fog_green;
    state.fog_color[2] = mgRenderInfo.fog_blue;
    state.scissor = true;
    state.scissor_rect = g_current.window;
    return state;
}

void MGPortApplyTest(gfx::DrawState &state, const sceGsTest &test) {
    state.alpha_test = test.ATE != 0;
    state.alpha_func = static_cast<gfx::AlphaFunc>(test.ATST);
    state.alpha_ref = static_cast<uint8_t>(test.AREF);
    // ZTE 0 is a mode the GS documents as forbidden; the game only clears it with ZTST ALWAYS.
    state.depth_test = test.ZTE ? static_cast<gfx::DepthTest>(test.ZTST) : gfx::DepthTest::Always;
}

void MGPortApplyZbuf(gfx::DrawState &state, const sceGsZbuf &zbuf) {
    state.depth_write = zbuf.ZMSK == 0;
}

void MGPortApplyAlpha(gfx::DrawState &state, const sceGsAlpha &alpha) {
    state.alpha.a = static_cast<uint8_t>(alpha.A);
    state.alpha.b = static_cast<uint8_t>(alpha.B);
    state.alpha.c = static_cast<uint8_t>(alpha.C);
    state.alpha.d = static_cast<uint8_t>(alpha.D);
    state.alpha.fix = static_cast<uint8_t>(alpha.FIX);
}

void MGPortApplyTexa(gfx::DrawState &state, const sceGsTexa &texa) {
    state.texa_aem = texa.AEM != 0;
    state.texa_ta0 = static_cast<uint8_t>(texa.TA0);
}

float MGPortLogicalX(int gs_x) {
    return static_cast<float>(gs_x - kGsOriginX) / 16.0f;
}

// A field row is two logical rows.
float MGPortLogicalY(int gs_y) {
    return static_cast<float>(gs_y - GS_Y_OFFSET) / 8.0f;
}

// MGSetRenderInfo puts the near plane at 16,700,000 and the far one at 1, both through
// offset + scale / z; the renderer's depth runs 1 at the near plane to 0 at the far one through
// the same a + b / z, so the two are one affine map apart. Z 0 is the cleared buffer.
float MGPortDepth(unsigned gs_z) {
    return std::max(0.0f, static_cast<float>((static_cast<double>(gs_z) - 1.0) / 16699999.0));
}

int MGPortFrameRowScale(gfx::TextureHandle texture) {
    return texture == gfx::kMainTarget || texture == gfx::kPreviousFrame ? 2 : 1;
}

float MGPortTargetRowScale(gfx::TextureHandle target) {
    if (target == gfx::kMainTarget) {
        return 1.0f;
    }
    std::optional<gfx::TextureInfo> info = gfx::GetTextureInfo(target);
    // The game's frame-sized targets are 448 or 480 rows; its field copies 224, 240 or 256.
    return info && info->height < 400 ? 0.5f : 1.0f;
}

void MGPortWorldToClip(float clip[4][4]) {
    float eye_to_clip[4][4];
    Draw3DEyeToClip(mgRenderInfo, eye_to_clip);
    Draw3DMul(clip, eye_to_clip, mgRenderInfo.view_scaled);
}

bool MGPortFrameTarget(std::string_view name) {
    while (name.starts_with('#')) {
        name.remove_prefix(1);
    }
    return name.starts_with("frame_") || name.starts_with("water_buff") || name == "water" ||
           name.starts_with("water#");
}

void MoveFrameImage(gfx::TextureHandle frame, sceGsTex0 *dst) {
    PortTextureRef ref = Draw3DResolveTex0(std::bit_cast<u_long>(*dst));
    if (!ref.valid || ref.binding.texture == gfx::kNullTexture || ref.binding.texture == gfx::kMainTarget ||
        ref.binding.texture == gfx::kPreviousFrame) {
        return;
    }
    gfx::BlitTexture(frame, {0, 0, 640, SCREEN_HEIGHT}, ref.binding.texture, {0, 0, 640, SCREEN_HEIGHT},
                     gfx::Filter::Nearest);
}

void MGPortMovePreviousFrameImage(sceGsTex0 *dst) {
    MoveFrameImage(gfx::kPreviousFrame, dst);
}

void MGPortCutInterpolation() {
    g_cut_next_tick = true;
}

bool MGPortTakeInterpolationCut() {
    bool cut = g_cut_next_tick;
    g_cut_next_tick = false;
    return cut;
}

} // namespace dc::render
