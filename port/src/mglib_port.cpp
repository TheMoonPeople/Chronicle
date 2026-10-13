#include "mglib_port.hpp"

#include <algorithm>
#include <optional>

#include "draw3d.hpp"
#include "mglib.hpp"
#include "screen.hpp"

namespace {

// TEXA as vudata.cpp's My_DrawEnv leaves it before the game sends its own.
sceGsTexa InitialTexa() {
    sceGsTexa texa = {};
    texa.TA0 = 0x80;
    texa.AEM = 1;
    return texa;
}

MGPortRegisters g_current = {
    {},
    {},
    {},
    InitialTexa(), {0.0f, 0.0f, kScreenWidth, kScreenHeight}
};

constexpr int kGsOriginX = 27648;

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
