#include "mglib.hpp"
#include "types.h"

#include <libdma.h>
#include <libgraph.h>
#include <libvu0.h>

#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <optional>

#include "app/gameloop.hpp"
#include "collision.hpp"
#include "dataalloc.hpp"
#include "dataset.hpp"
#include "frame.hpp"
#include "mathutil.hpp"
#include "platform/clock.hpp"
#include "rect.hpp"
#include "render/draw3d.hpp"
#include "render/gs_state.hpp"
#include "render/screen.hpp"
#include "renderinfo.hpp"
#include "text/textwipe.hpp"
#include "texture.hpp"
#include "vu0_ops.hpp"

namespace dc::game {

using namespace dc::render;
using namespace dc::text;

int            DBuffID;
int            mgWaitVSync;
sceVif1Packet *Vif1Packet;
int            VSyncField__2;
int            mgClearBackFlag;
int            mgAdjustX;
int            mgAdjustY;
sceGsTex1      mgTEX1Env;
sceGsTest      mgPixelTest;
sceGsZbuf      mgZBuffer;
sceGsAlpha     mgAlpha;
sceGsTexa      mgTexa;
int            mgTopVRAM;
int            mgZBufferAdr;

MG_PICKZ      mgPickZBuff[16];
sceGifTag     GiftagAD;
sceVu0FVECTOR mgBackColor;

RenderInfo      mgRenderInfo;
CTextureManager TexManager;

sceVu0FVECTOR mgZeroVector;
sceVu0FVECTOR mgUnitVector;
sceVu0FVECTOR mgUnitVector2;
sceVu0FVECTOR mgZeroVector2;
sceVu0FMATRIX mgUnitMatrix;
sceVu0FMATRIX mgZeroMatrix;

CRect_i_ mgWindowRect;

namespace {

using EndFrameClock = std::chrono::steady_clock;

void NoteEndFramePhases(const std::array<EndFrameClock::time_point, 5> &stamps, uint32_t picks) {
    static std::array<double, 4> totals{};
    static std::array<double, 4> maxima{};
    static uint32_t              frames = 0;
    static uint32_t              depth_picks = 0;
    for (size_t i = 0; i < totals.size(); i++) {
        const double elapsed = std::chrono::duration<double, std::milli>(stamps[i + 1] - stamps[i]).count();
        totals[i] += elapsed;
        maxima[i] = std::max(maxima[i], elapsed);
    }
    depth_picks += picks;
    if (++frames == 120) {
        std::fprintf(stderr, "cpu endframe %u frames: lease %.3f/%.3f preDraw %.3f/%.3f draw %.3f/%.3f depth %.3f/%.3f picks %u\n",
                     frames, totals[0] / frames, maxima[0], totals[1] / frames, maxima[1],
                     totals[2] / frames, maxima[2], totals[3] / frames, maxima[3], depth_picks);
        frames = 0;
        depth_picks = 0;
        totals.fill(0.0);
        maxima.fill(0.0);
    }
}

// PAL's field buffer is half the frame's height. GS coordinates the game hands to the 2D units
// keep counting field rows (MGPortLogicalY doubles them), so the eye-to-GS step keeps the squeeze
// that retail baked into view_scaled.
constexpr float kFieldSqueeze = 0.5f;
constexpr float kGsCentre = 2048.0f;
constexpr float kGsDepthRange = 16699999.0f;

int                g_old_vcount;
sceVif1Packet      g_null_packet;
u_long128          g_null_packet_words[4];
std::optional<int> g_pick_pending[16];

// A camera that moves further or turns more than this between ticks has cut, and is not
// interpolated across.
constexpr float kCameraCutDistance = 200.0f;
constexpr float kCameraCutCosine = 0.7071f;

bool          g_have_tick_view = false;
sceVu0FMATRIX g_tick_view;

// The logical frame's rect of a GS sprite whose corners are 12.4 window offsets on the field.
gfx::LogicalRect FieldRect(int x, int y, int width, int height) {
    float left = static_cast<float>(x) / 16.0f;
    float top = static_cast<float>(y) / 8.0f;
    return {left, top, static_cast<float>(width) / 16.0f, static_cast<float>(height) / 8.0f};
}

// The bars around the letterboxed frame on the main target, which nothing else draws to.
void ClearBars() {
    gfx::LogicalMapping mapping = gfx::GetLogicalMapping(gfx::kMainTarget);
    if (mapping.pixel_width == 0 || mapping.pixel_height == 0) {
        return;
    }
    float            left = -mapping.offset_x / mapping.scale_x;
    float            top = -mapping.offset_y / mapping.scale_y;
    float            right = (static_cast<float>(mapping.pixel_width) - mapping.offset_x) / mapping.scale_x;
    float            bottom = (static_cast<float>(mapping.pixel_height) - mapping.offset_y) / mapping.scale_y;
    uint8_t          black[4] = {0, 0, 0, 0x80};
    gfx::LogicalRect bars[4] = {
        {left,         top,           -left,                bottom - top          },
        {kScreenWidth, top,           right - kScreenWidth, bottom - top          },
        {0.0f,         top,           kScreenWidth,         -top                  },
        {0.0f,         kScreenHeight, kScreenWidth,         bottom - kScreenHeight},
    };
    for (const gfx::LogicalRect &bar : bars) {
        if (bar.w > 0.0f && bar.h > 0.0f) {
            gfx::Clear(true, black, true, 0.0f, &bar);
        }
    }
}

u_long FrameTex0(unsigned tbp0) {
    return SCE_GS_SET_TEX0(tbp0, 10, SCE_GS_PSMCT32, 10, 8, 0, 0, 0, 0, 0, 0, 0);
}

struct ResolvedRect {
    gfx::TextureHandle texture;
    gfx::Rect          rect;
};

// A rect in a TEX0 buffer's own rows, in the logical units gfx takes for that texture.
std::optional<ResolvedRect> ResolveRect(const sceGsTex0 &tex0, int x, int y, int width, int height) {
    PortTextureRef ref = Draw3DResolveTex0(std::bit_cast<u_long>(tex0));
    if (!ref.valid || ref.binding.texture == gfx::kNullTexture) {
        return std::nullopt;
    }
    int rows = MGPortFrameRowScale(ref.binding.texture);
    return ResolvedRect{
        ref.binding.texture, {x, y * rows, width, height * rows}
    };
}

// gfx refuses a blit within one image; this goes through a scratch texture instead.
void BlitWithin(gfx::TextureHandle texture, gfx::Rect src, gfx::Rect dst, gfx::Filter filter) {
    static gfx::TextureHandle scratch = gfx::kNullTexture;
    static uint32_t           scratch_width = 0;
    static uint32_t           scratch_height = 0;
    static bool               scratch_frame = false;
    auto                      width = static_cast<uint32_t>(std::abs(src.w));
    auto                      height = static_cast<uint32_t>(std::abs(src.h));
    // A move of the frame's whole width carries the sides past the logical frame through a frame
    // target of its own.
    std::optional<gfx::TextureInfo> info = gfx::GetTextureInfo(texture);

    bool frame = info && (info->frame_target || info->shares_main_depth || texture == gfx::kMainTarget);
    frame = frame && width >= info->width;
    if (scratch == gfx::kNullTexture || scratch_width < width || scratch_height < height ||
        scratch_frame != frame) {
        if (scratch != gfx::kNullTexture) {
            gfx::DestroyTexture(scratch);
        }
        scratch_width = std::max(width, scratch_width);
        scratch_height = std::max(height, scratch_height);
        scratch_frame = frame;
        scratch = gfx::CreateRenderTarget(scratch_width, scratch_height, true, false, frame);
    }
    gfx::Rect middle = {0, 0, static_cast<int32_t>(width), static_cast<int32_t>(height)};
    gfx::BlitTexture(texture, src, scratch, middle, gfx::Filter::Nearest);
    gfx::BlitTexture(scratch, middle, texture, dst, filter);
}

bool IsFrame(const sceGsTex0 &tex0) {
    return tex0.TBP0 == kMGPortFrameTbp0 || tex0.TBP0 == kMGPortPreviousFrameTbp0;
}

void Blit(const ResolvedRect &src, const ResolvedRect &dst, gfx::Filter filter) {
    if (src.texture == gfx::kMainTarget && dst.texture != gfx::kMainTarget) {
        Draw3DNoteFrameCopy(dst.texture);
    }
    if (src.texture == dst.texture) {
        BlitWithin(src.texture, src.rect, dst.rect, filter);
    } else if (src.rect.w == dst.rect.w && src.rect.h == dst.rect.h && src.texture != gfx::kMainTarget &&
               src.texture != gfx::kPreviousFrame && dst.texture != gfx::kMainTarget) {
        gfx::CopyTexture(src.texture, src.rect, dst.texture, dst.rect.x, dst.rect.y);
    } else {
        gfx::BlitTexture(src.texture, src.rect, dst.texture, dst.rect, filter);
    }
}

// The view against the last one the previous tick drew with.
void CheckCameraCut(sceVu0FMATRIX view) {
    if (!gfx::Recording() || !g_have_tick_view) {
        return;
    }
    float before[16];
    float after[16];
    if (!gfx::InvertAffineTransform(&g_tick_view[0][0], before) ||
        !gfx::InvertAffineTransform(&view[0][0], after)) {
        return;
    }
    float dx = after[12] - before[12];
    float dy = after[13] - before[13];
    float dz = after[14] - before[14];
    float forward = before[8] * after[8] + before[9] * after[9] + before[10] * after[10];
    float lengths = std::sqrt((before[8] * before[8] + before[9] * before[9] + before[10] * before[10]) *
                              (after[8] * after[8] + after[9] * after[9] + after[10] * after[10]));
    float moved = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (moved > kCameraCutDistance || forward < kCameraCutCosine * lengths) {
        gfx::CutCameraInterpolation();
    }
}

void SetViewMatrix(sceVu0FMATRIX view) {
    CheckCameraCut(view);
    sceVu0CopyMatrix(mgRenderInfo.view, view);
    mgRenderInfo.position[0] = view[3][0];
    mgRenderInfo.position[1] = view[3][1];
    mgRenderInfo.position[2] = view[3][2];
    mgRenderInfo.position[3] = 0.0f;

    // Progressive frames need no field squeeze in eye space: x and y keep one scale.
    sceVu0CopyMatrix(mgRenderInfo.view_scaled, view);

    sceVu0FMATRIX screen;
    sceVu0UnitMatrix(screen);
    screen[0][0] = mgRenderInfo.scale[0];
    screen[1][1] = mgRenderInfo.scale[0] * kFieldSqueeze;
    screen[2][2] = mgRenderInfo.offset[2];
    screen[3][3] = 0.0f;
    screen[2][1] = kGsCentre;
    screen[2][0] = kGsCentre;
    screen[2][3] = 1.0f;
    screen[3][2] = mgRenderInfo.scale[2];
    MulMatrix(mgRenderInfo.view_screen, screen, mgRenderInfo.view_scaled);
    sceVu0CopyMatrix(mgRenderInfo.screen, screen);

    sceVu0FVECTOR direction = {-mgRenderInfo.view[0][2], -mgRenderInfo.view[1][2], -mgRenderInfo.view[2][2], 0.0f};
    sceVu0FVECTOR flat = {direction[0], 0.0f, direction[2], 0.0f};
    sceVu0Normalize(flat, flat);
    mgRenderInfo.yaw = atan2f(flat[0], flat[2]);
    mgRenderInfo.pitch = -atan2f(direction[1], sqrtf(direction[0] * direction[0] + direction[2] * direction[2]));
}

float Fog(float w) {
    float density = mgRenderInfo.fog_a + mgRenderInfo.fog_b * w;
    density = std::max(density, mgRenderInfo.fog_far);
    return std::min(density, mgRenderInfo.fog_near);
}

void DrawShadowPass(CFrame *frame, float *position, float *normal, int pass, Draw3DShadowProgram program) {
    if (!frame) {
        return;
    }
    Draw3DSetShadowProgram(program);
    sceVu0CopyVector(mgRenderInfo.shadow_point, position);
    sceVu0CopyVector(mgRenderInfo.shadow_normal, normal);

    sceGsZbuf zbuf = mgZBuffer;
    sceGsTest test = mgPixelTest;
    zbuf.bits.zmsk = 1;
    MGSetGsZBUF(&zbuf);
    test.bits.ate = 0;
    test.bits.date = 0;
    MGSetGsTEST(&test);

    sceVu0FMATRIX perspective;
    sceVu0FMATRIX viewport;
    sceVu0CopyMatrix(perspective, mgRenderInfo.perspective);
    sceVu0CopyMatrix(viewport, mgRenderInfo.viewport);
    mgRenderInfo.shadow_pass = pass;
    MGDraw(frame);
    mgRenderInfo.shadow_pass = 0;
    sceVu0CopyMatrix(mgRenderInfo.perspective, perspective);
    sceVu0CopyMatrix(mgRenderInfo.viewport, viewport);
}

} // namespace

// ---- VSync group -----------------------------------------------------------------------------

int MGGetVSyncCount() {
    return static_cast<int>(ClockTickCount());
}

void MGInit() {
    DBuffID = 0;
    mgWaitVSync = 0;
    VSyncField__2 = 0;
    g_null_packet.pBase = reinterpret_cast<u_int *>(g_null_packet_words);
    g_null_packet.pCurrent = reinterpret_cast<u_int *>(g_null_packet_words);
    Vif1Packet = &g_null_packet;

    GiftagAD = {};
    GiftagAD.EOP = 1;
    GiftagAD.NREG = 1;
    GiftagAD.REGS0 = SCE_GIF_PACKED_AD;

    mgTopVRAM = 0x1E00;
    mgZBufferAdr = 0x1400;

    mgWindowRect.x = 0;
    mgWindowRect.y = 0;
    mgWindowRect.width = 640;
    mgWindowRect.height = SCREEN_HALF_HEIGHT;
    MGPortCurrent().window = {0.0f, 0.0f, kScreenWidth, kScreenHeight};
    MGAdjustScreen(0, 0);

    mgBackColor[0] = 0.0f;
    mgBackColor[1] = 0.0f;
    mgBackColor[2] = 0.0f;
    mgBackColor[3] = 128.0f;
    mgClearBackFlag = 1;

    mgTEX1Env = {};
    mgTEX1Env.LCM = 1;
    mgTEX1Env.MMAG = 1;
    mgTEX1Env.MMIN = 1;
    mgTEX1Env.MTBA = 1;

    // What sceGsSetDefDBuff(..., SCE_GS_ZGEQUAL, SCE_GS_PSMZ24, 0) puts in the draw environment.
    mgPixelTest = {};
    mgPixelTest.bits.zte = 1;
    mgPixelTest.bits.ztst = 2;
    mgPixelTest.bits.ate = 1;
    mgPixelTest.bits.atst = 5;
    mgZBuffer = {};
    mgZBuffer.bits.zbp = mgZBufferAdr >> 5;
    mgZBuffer.bits.psm = SCE_GS_PSMZ24 & 0xF;

    mgAlpha = {};
    mgAlpha.bits.a = 0;
    mgAlpha.bits.b = 1;
    mgAlpha.bits.c = 0;
    mgAlpha.bits.d = 1;

    mgTexa = {};
    mgTexa.AEM = 1;

    MGPortRestoreRegisters();

    mgZeroVector[0] = mgZeroVector[1] = mgZeroVector[2] = mgZeroVector[3] = 0.0f;
    mgUnitVector[0] = mgUnitVector[1] = mgUnitVector[2] = mgUnitVector[3] = 1.0f;
    sceVu0CopyVector(mgZeroVector2, mgZeroVector);
    sceVu0CopyVector(mgUnitVector2, mgUnitVector);
    mgZeroVector2[3] = 1.0f;
    mgUnitVector2[3] = 0.0f;
    sceVu0UnitMatrix(mgUnitMatrix);
    sceVu0UnitMatrix(mgZeroMatrix);
    mgZeroMatrix[0][0] = mgZeroMatrix[1][1] = mgZeroMatrix[2][2] = mgZeroMatrix[3][3] = 0.0f;

    CreateSinTable();

    ClockSetTickCallback(nullptr);
    ClockReset();
    g_old_vcount = 0;
}

void MGInitVSyncCallBack(int (*callback)(int)) {
    ClockSetTickCallback(callback);
}

// The renderer builds no VIF packets; nothing reads the packet builders.
void MGInitVif1Packet(u_long128 * /*buffer0*/, u_long128 * /*buffer1*/) {}

// A tick records its drawing as a display list; MGEndFrame renders it. A frame someone opened
// with gfx::BeginFrame is drawn into as it is.
void MGBeginFrame() {
    if (!gfx::InFrame()) {
        gfx::BeginRecording();
        if (MGPortTakeInterpolationCut()) {
            gfx::CutInterpolation();
        }
    }
    gfx::SetRenderTarget(gfx::kMainTarget);
    Draw3DSetShadowTargetActive(false);

    Vif1Packet = &g_null_packet;
    ActiveData = DBuffID ? &ActiveData0 : &ActiveData1;
    ActiveData->used = 0;
    if (WorkBuffer) {
        WorkBuffer->used = 0;
    }
    Draw3DSweepVisuals();

    GiftagAD = {};
    GiftagAD.EOP = 1;
    GiftagAD.NREG = 1;
    GiftagAD.REGS0 = SCE_GIF_PACKED_AD;

    mgZeroVector[0] = mgZeroVector[1] = mgZeroVector[2] = mgZeroVector[3] = 0.0f;
    mgUnitVector[0] = mgUnitVector[1] = mgUnitVector[2] = mgUnitVector[3] = 1.0f;
    sceVu0CopyVector(mgZeroVector2, mgZeroVector);
    sceVu0CopyVector(mgUnitVector2, mgUnitVector);
    mgZeroVector2[3] = 1.0f;
    mgUnitVector2[3] = 0.0f;
    sceVu0UnitMatrix(mgUnitMatrix);
    sceVu0UnitMatrix(mgZeroMatrix);
    mgZeroMatrix[0][0] = mgZeroMatrix[1][1] = mgZeroMatrix[2][2] = mgZeroMatrix[3][3] = 0.0f;

    ClearBars();
    if (mgClearBackFlag) {
        MGClearScreen(static_cast<u_char>(mgBackColor[0]), static_cast<u_char>(mgBackColor[1]),
                      static_cast<u_char>(mgBackColor[2]), static_cast<u_char>(mgBackColor[3]));
    } else {
        MGClearZBuffer(0);
    }
}

// Retail waits for the next VSync unless one already passed while the frame was built and the
// game asked not to wait twice (mgWaitVSync 0). The count only moves when the clock is pumped, so
// it is pumped first to see whether a tick went by.
void MGEndFrame() {
    static const bool                        trace_phases = std::getenv("DC_PERF_END_FRAME") != nullptr;
    std::array<EndFrameClock::time_point, 5> stamps{};
    uint32_t                                 depth_picks = 0;
    if (trace_phases) {
        stamps[0] = EndFrameClock::now();
    }
    {
        gfx::RenderLease lease;
        if (trace_phases) {
            stamps[1] = EndFrameClock::now();
        }
        for (int i = 0; i < 16; i++) {
            g_pick_pending[i].reset();
            if (!mgPickZBuff[i].enable) {
                continue;
            }
            if (mgPickZBuff[i].x < 4 || mgPickZBuff[i].x > 636 || mgPickZBuff[i].y < 4 ||
                mgPickZBuff[i].y > SCREEN_HEIGHT - 4) {
                mgPickZBuff[i].z = -1;
                continue;
            }
            // Retail reads 8x8 of the field buffer, which is 8x16 of the frame.
            gfx::ReadDepth(static_cast<uint32_t>(i), static_cast<float>(mgPickZBuff[i].x - 4),
                           static_cast<float>(mgPickZBuff[i].y - 8), 8.0f, 16.0f);
            g_pick_pending[i] = i;
            ++depth_picks;
        }

        gfx::SetRenderTarget(gfx::kMainTarget);
        gfx::DisplayListRef list;
        if (gfx::Recording()) {
            textwipe::DrawDivider();
            list = gfx::EndRecording();
            if (trace_phases) {
                stamps[2] = EndFrameClock::now();
            }
            app::GameRenderTick(list);
            sceVu0CopyMatrix(g_tick_view, mgRenderInfo.view_scaled);
            g_have_tick_view = true;
        } else {
            if (trace_phases) {
                stamps[2] = EndFrameClock::now();
            }
            gfx::EndFrame();
        }
        if (trace_phases) {
            stamps[3] = EndFrameClock::now();
        }

        for (int i = 0; i < 16; i++) {
            if (!g_pick_pending[i]) {
                continue;
            }
            std::optional<float> depth = gfx::DepthResult(static_cast<uint32_t>(i));
            if (!depth) {
                mgPickZBuff[i].z = -1;
            } else if (*depth <= 0.0f) {
                mgPickZBuff[i].z = 0;
            } else {
                mgPickZBuff[i].z = static_cast<int>(std::lround(1.0 + static_cast<double>(*depth) * kGsDepthRange));
            }
        }
        if (trace_phases) {
            stamps[4] = EndFrameClock::now();
        }
    }
    if (trace_phases) {
        NoteEndFramePhases(stamps, depth_picks);
    }
    if (!mgWaitVSync) {
        ClockPump();
    }
    if (mgWaitVSync || ClockTickCount() == g_old_vcount) {
        ClockWaitNextTick(app::GamePresentBetweenTicks);
        ClockPump();
    }
    g_old_vcount = static_cast<int>(ClockTickCount());
    app::GamePresentTickEnd();

    DBuffID = !DBuffID;
}

void MGFlipWaitVSync(int wait) {
    mgWaitVSync = wait;
}

// The display position only moved the PAL picture on a television; the clamped values are kept
// for the save data's screen-position setting, and nothing moves.
void MGAdjustScreen(int x, int y) {
    if (x > 32 || x < -32) {
        x = 0;
    }
    if (y > 32 || y < -32) {
        y = 0;
    }
    mgAdjustX = (x >> 1) << 1;
    mgAdjustY = (y >> 1) << 1;
}

sceVif1Packet *GetVif1Packet() {
    return Vif1Packet;
}

// ---- Projection, camera, lights, fog ---------------------------------------------------------

void MGSetRenderInfo(float scale, float near_z, float far_z) {
    float w = 1.0f;
    float z_range = 16699999;
    float near_far = near_z * far_z;
    float z_scale = near_far * z_range / (far_z - near_z);
    float z_offset = -(16700000 * near_z - w * far_z) / (far_z - near_z);

    mgRenderInfo.projection = scale;
    mgRenderInfo.scale[0] = scale;
    mgRenderInfo.scale[1] = scale;
    mgRenderInfo.scale[2] = z_scale;
    mgRenderInfo.offset[0] = kGsCentre;
    mgRenderInfo.offset[1] = kGsCentre;
    mgRenderInfo.offset[2] = z_offset;
    mgRenderInfo.near[0] = 0.0f;
    mgRenderInfo.near[1] = 0.0f;
    mgRenderInfo.near[2] = near_z;
    mgRenderInfo.far[0] = 4095.9f;
    mgRenderInfo.far[1] = 4095.9f;
    mgRenderInfo.far[2] = far_z;

    mgRenderInfo.clip_min[0] = 0.0f;
    mgRenderInfo.clip_min[1] = 0.0f;
    mgRenderInfo.clip_min[2] = 0.0f;
    mgRenderInfo.clip_min[3] = near_z;
    mgRenderInfo.clip_max[0] = 4095.9f;
    mgRenderInfo.clip_max[1] = 4095.9f;
    mgRenderInfo.clip_max[2] = 0.0f;
    mgRenderInfo.clip_max[3] = far_z;

    sceVu0FMATRIX perspective;
    sceVu0UnitMatrix(perspective);
    float half_height = 2047 * near_z / scale;
    perspective[0][0] = 2.0f * near_z / (half_height + half_height);
    perspective[1][1] = perspective[0][0];
    perspective[2][2] = (far_z + near_z) / (far_z - near_z);
    perspective[3][2] = -2.0f * (far_z * near_z) / (far_z - near_z);
    perspective[2][3] = 1.0f;
    perspective[3][3] = 0.0f;
    sceVu0CopyMatrix(mgRenderInfo.perspective, perspective);

    float z_max = 16700000;
    sceVu0UnitMatrix(mgRenderInfo.viewport);
    mgRenderInfo.viewport[0][0] = half_height * (scale * w) / near_z;
    mgRenderInfo.viewport[1][1] = mgRenderInfo.viewport[0][0];
    mgRenderInfo.viewport[2][2] = (-z_max + w) / 2.0f;
    mgRenderInfo.viewport[3][2] = 8350000.5f;
    mgRenderInfo.viewport[3][0] = kGsCentre;
    mgRenderInfo.viewport[3][1] = kGsCentre;
    mgRenderInfo.viewport[3][3] = w;
}

void MGSetProjection(float scale) {
    MGSetRenderInfo(scale, mgRenderInfo.near[2], mgRenderInfo.far[2]);
}

float MGGetProjection() {
    return mgRenderInfo.projection;
}

// SCAX0 takes half of x, as retail sends it; the rectangle counts field rows.
void MGSetWindowRect(CRect_i_ rect) {
    mgWindowRect = rect;
    auto left = static_cast<float>(rect.x >> 1);
    auto right = static_cast<float>(rect.x + rect.width + 1);
    auto top = static_cast<float>(rect.y * 2);
    auto bottom = static_cast<float>((rect.y + rect.height + 1) * 2);
    MGPortCurrent().window = {left, top, right - left, bottom - top};
}

void MGSetWindowRect() {
    MGSetWindowRect(CRect_i_(0, 0, 640, SCREEN_HALF_HEIGHT));
    MGPortCurrent().window = {0.0f, 0.0f, kScreenWidth, kScreenHeight};
}

void MGSetPLight(sceVu0FMATRIX direction, sceVu0FMATRIX color) {
    sceVu0CopyMatrix(mgRenderInfo.light_direction, direction);
    sceVu0CopyMatrix(mgRenderInfo.light_color, color);
    for (int i = 0; i < 4; i++) {
        mgRenderInfo.light_color[i][3] = 0.0f;
        mgRenderInfo.light_direction[3][i] = 0.0f;
    }
}

void MGGetPLight(sceVu0FMATRIX direction, sceVu0FMATRIX color) {
    sceVu0CopyMatrix(direction, mgRenderInfo.light_direction);
    sceVu0CopyMatrix(color, mgRenderInfo.light_color);
}

void MGSetAmbient(float *ambient) {
    sceVu0CopyVector(mgRenderInfo.ambient, ambient);
}

void MGGetAmbient(float *ambient) {
    sceVu0CopyVector(ambient, mgRenderInfo.ambient);
}

void MGSetViewMatrix(sceVu0FMATRIX view) {
    SetViewMatrix(view);
}

void MGSetViewMatrix(sceVu0FMATRIX view, float *position) {
    SetViewMatrix(view);
    sceVu0CopyVector(mgRenderInfo.view_position, position);
    mgRenderInfo.position[0] = position[0];
    mgRenderInfo.position[1] = position[1];
    mgRenderInfo.position[2] = position[2];
    mgRenderInfo.position[3] = 0.0f;
}

void MGSetFogParm(float near_z, float far_z, u_char r, u_char g, u_char b, float far_fog, float near_fog) {
    mgRenderInfo.fog_far = far_fog;
    mgRenderInfo.fog_near = near_fog;
    mgRenderInfo.fog_a = ((far_fog + near_fog) + (far_fog - near_fog) * (far_z + near_z) / (far_z - near_z)) / 2.0f;
    mgRenderInfo.fog_b = -far_z * near_z * (far_fog - near_fog) / (far_z - near_z);
    mgRenderInfo.fog_red = r;
    mgRenderInfo.fog_green = g;
    mgRenderInfo.fog_blue = b;
}

void MGSetBGColor(float red, float green, float blue, float alpha) {
    mgClearBackFlag = !(red < 0.0f && green < 0.0f && blue < 0.0f && alpha < 0.0f);
    mgBackColor[0] = std::clamp(red, 0.0f, 255.0f);
    mgBackColor[1] = std::clamp(green, 0.0f, 255.0f);
    mgBackColor[2] = std::clamp(blue, 0.0f, 255.0f);
    mgBackColor[3] = std::clamp(alpha, 0.0f, 255.0f);
}

void MGSetBGColor(float *color) {
    MGSetBGColor(color[0], color[1], color[2], color[3]);
}

void MGGetBGColor(float *color) {
    sceVu0CopyVector(color, mgBackColor);
}

void MGScisioringForce(int force) {
    mgRenderInfo.scissoring = force;
}

// ---- CPU projections -------------------------------------------------------------------------

// view_scaled is unsqueezed, so the field squeeze is applied here to give GS field coordinates.
int MGRotTransPers(int *screen, float *position, int fog) {
    sceVu0FVECTOR point;
    int           visible = true;

    sceVu0ApplyMatrix(point, mgRenderInfo.view_scaled, position);
    if (point[2] < 1.0f) {
        visible = false;
        point[2] = 1.0f;
    }
    float w = 1.0f / point[2];
    point[0] *= w;
    point[1] *= w;
    point[0] *= mgRenderInfo.scale[0];
    point[1] *= mgRenderInfo.scale[1] * kFieldSqueeze;
    point[2] = w * mgRenderInfo.scale[2];
    point[0] += mgRenderInfo.offset[0];
    point[1] += mgRenderInfo.offset[1];
    point[2] += mgRenderInfo.offset[2];

    screen[0] = static_cast<int>(16.0f * point[0]);
    screen[1] = static_cast<int>(16.0f * point[1]);
    screen[2] = static_cast<int>(point[2]);
    if (fog) {
        screen[3] = static_cast<int>(Fog(w));
    }
    if (point[0] < 0.0f || point[1] < 0.0f || point[0] > 4095 || point[1] > 4095) {
        visible = false;
    }
    return visible;
}

// Logical frame pixels; the on-screen test runs in GS field units.
int MGRotTransPers2D(int *screen, float *position, int fog) {
    sceVu0FVECTOR point;
    int           visible = true;

    sceVu0ApplyMatrix(point, mgRenderInfo.view_scaled, position);
    if (point[2] < 1.0f) {
        visible = false;
        point[2] = 1.0f;
    }
    float w = 1.0f / point[2];
    point[0] *= w;
    point[1] *= w;
    point[0] *= mgRenderInfo.scale[0];
    point[1] *= mgRenderInfo.scale[1];
    point[2] = w * mgRenderInfo.scale[2];
    point[0] += mgRenderInfo.offset[0];
    point[1] += mgRenderInfo.offset[1];
    point[2] += mgRenderInfo.offset[2];

    screen[0] = static_cast<int>(point[0]);
    screen[1] = static_cast<int>(point[1]);
    screen[2] = static_cast<int>(point[2]);
    if (fog) {
        screen[3] = static_cast<int>(Fog(w));
    }
    if (point[0] < 0.0f || point[1] < 0.0f || point[0] > 4095 || point[1] > 4095) {
        visible = false;
    }
    screen[0] -= 1728;
    screen[1] -= 2048 - SCREEN_HALF_HEIGHT;
    return visible;
}

namespace {

void StoreCorner(int *out, const float corner[4], std::int32_t w) {
    out[0] = vu0::Ftoi(corner[0], 16.0f);
    out[1] = vu0::Ftoi(corner[1], 16.0f);
    out[2] = vu0::Ftoi(corner[2], 1.0f);
    out[3] = w;
}

} // namespace

// Visibility is retail's sticky zero and sign flags, so a corner exactly on 0 or 4096 hides the
// sprite. Without fog, retail stores the clip w lane untouched by vftoi, so the corners' w is its
// float bits. The fog clamp takes fog_near before fog_far, the reverse of MGRotTransPers2D.
int MGRotTransPers3DSprite(int *top_left, int *bottom_right, float *position, float width, float height, int fog) {
    float half[2] = {0.5f * width * mgRenderInfo.scale[0], 0.5f * height * mgRenderInfo.scale[1]};
    float point[4];

    vu0::Apply(point, mgRenderInfo.view_screen, position);
    float q = vu0::Div(1.0f, point[3]);
    for (int i = 0; i < 3; i++) {
        point[i] = vu0::Mul(point[i], q);
    }
    half[0] = vu0::Mul(half[0], q);
    half[1] = vu0::Mul(half[1], q);

    float near_corner[4];
    float far_corner[4];
    vu0::Copy(near_corner, point);
    vu0::Copy(far_corner, point);
    near_corner[0] = near_corner[0] - half[0];
    near_corner[1] = near_corner[1] - half[1];
    far_corner[0] = far_corner[0] + half[0];
    far_corner[1] = far_corner[1] + half[1];

    int status = 0;
    for (const float *corner : {near_corner, far_corner}) {
        status |= vu0::StatusFlags(corner[0]) | vu0::StatusFlags(corner[1]) | vu0::StatusFlags(corner[3]);
        status |= vu0::StatusFlags(4096.0f - corner[0]) | vu0::StatusFlags(4096.0f - corner[1]);
    }

    auto w = std::bit_cast<std::int32_t>(point[3]);
    if (fog) {
        float density = mgRenderInfo.fog_a + vu0::Mul(mgRenderInfo.fog_b, q);
        density = vu0::Min(density, mgRenderInfo.fog_near);
        density = vu0::Max(density, mgRenderInfo.fog_far);
        w = vu0::Ftoi(density, 1.0f);
    }

    StoreCorner(top_left, near_corner, w);
    StoreCorner(bottom_right, far_corner, w);

    return (status & (vu0::kStickyZero | vu0::kStickySign)) == 0;
}

// light_direction holds one light per column (sceVu0NormalLightMatrix's layout). The clamps run on
// all four lanes, so the colour's w is clamped to 255 like the rest.
void MGCalcColor(float *color, float *normal) {
    color[0] = 255.0f;
    float ceiling = color[0];

    float n[4];
    vu0::Copy(n, normal);
    const float (*direction)[4] = mgRenderInfo.light_direction;
    const float (*intensity)[4] = mgRenderInfo.light_color;
    const float *ambient = mgRenderInfo.ambient;

    float facing[4];
    for (int i = 0; i < 4; i++) {
        facing[i] = direction[0][i] * n[0];
        facing[i] = facing[i] + direction[1][i] * n[1];
        facing[i] = facing[i] + direction[2][i] * n[2];
        facing[i] = vu0::Max(facing[i], 0.0f);
    }

    float sum[4];
    for (int i = 0; i < 4; i++) {
        sum[i] = ambient[i];
        sum[i] = sum[i] + intensity[0][i] * facing[0];
        sum[i] = sum[i] + intensity[1][i] * facing[1];
        sum[i] = sum[i] + intensity[2][i] * facing[2];
        sum[i] = sum[i] + intensity[3][i] * facing[3];
        sum[i] = vu0::Min(sum[i], ceiling);
    }

    vu0::Copy(color, sum);
}

// view_screen keeps retail's GS field units, so the vertical bound is half the frame's half-height;
// both bounds are what the current target shows, which past a 4:3 window is wider than retail's.
int MGClipVertex(float *position) {
    sceVu0FVECTOR point;
    int           outside = 0;
    Draw3DExtent  extent = Draw3DVisibleExtent();
    float         half_width = extent.half_width;
    float         half_height = extent.half_height * kFieldSqueeze;

    sceVu0ApplyMatrix(point, mgRenderInfo.view_screen, position);
    if (0.0f == point[3]) {
        point[3] = 1.0f;
    }
    float w = 1.0f / point[3];
    point[0] *= w;
    point[1] *= w;

    if (point[3] < mgRenderInfo.near[2]) {
        outside |= 0x20;
    }
    if (point[3] > mgRenderInfo.far[2]) {
        outside |= 0x10;
    }

    float dy = point[1] - mgRenderInfo.offset[1];
    float dx = point[0] - mgRenderInfo.offset[0];
    if (point[3] > 0.0f) {
        outside |= (dy < -half_height ? 0x8 : 0) | (dy > half_height ? 0x4 : 0);
        outside |= (dx < -half_width ? 0x2 : 0) | (dx > half_width ? 0x1 : 0);
    } else {
        outside |= (dy > -half_height ? 0x8 : 0) | (dy < half_height ? 0x4 : 0);
        outside |= (dx > -half_width ? 0x2 : 0) | (dx < half_width ? 0x1 : 0);
    }
    return outside;
}

int MGClipBox(CBoxVu0 *box) {
    sceVu0FVECTOR corner;
    float        *extreme[2] = {box->min, box->max};
    int           outside = 63;

    for (int i = 0; i < 8; i++) {
        corner[3] = 1.0f;
        corner[0] = extreme[(i & 1) != 0][0];
        corner[1] = extreme[(i & 2) != 0][1];
        corner[2] = extreme[(i & 4) != 0][2];
        int bits = MGClipVertex(corner);
        if (!bits) {
            return 0;
        }
        outside &= bits;
        if (!outside) {
            return 0;
        }
    }
    return 1;
}

// ---- Drawing ---------------------------------------------------------------------------------

void MGDraw(CFrame *frame) {
    if (frame) {
        frame->DrawVu1(static_cast<unsigned int *>(nullptr), &mgRenderInfo);
    }
}

void MGSetGsTEST(sceGsTest *test) {
    MGPortCurrent().test = test ? *test : mgPixelTest;
}

void MGSetGsZBUF(sceGsZbuf *zbuf) {
    MGPortCurrent().zbuf = zbuf ? *zbuf : mgZBuffer;
}

void MGSetGsALPHA(sceGsAlpha *alpha) {
    MGPortCurrent().alpha = alpha ? *alpha : mgAlpha;
}

void MGSetGsTEXA(sceGsTexa *texa) {
    MGPortCurrent().texa = texa ? *texa : mgTexa;
}

void MGGetFBuffTex(sceGsTex0 *tex0) {
    *tex0 = std::bit_cast<sceGsTex0>(FrameTex0(kMGPortFrameTbp0));
}

void MGGetFBuffBackTex(sceGsTex0 *tex0) {
    *tex0 = std::bit_cast<sceGsTex0>(FrameTex0(kMGPortPreviousFrameTbp0));
}

// Moves between two of the texture manager's images (CLUT moves included) are its business; a move
// that reads or writes the frame scales the frame's field rows to its logical ones.
void MGMoveImage(sceGsTex0 *src, const CRect_i_ &rect, sceGsTex0 *dst, int dst_x, int dst_y, int /*direction*/) {
    if (rect.width <= 0 || rect.height <= 0) {
        return;
    }
    if (!Draw3DHasTex0Resolver() && !IsFrame(*src) && !IsFrame(*dst)) {
        PortMoveImage(*src, rect.x, rect.y, rect.width, rect.height, *dst, dst_x, dst_y);
        return;
    }
    std::optional<ResolvedRect> from = ResolveRect(*src, rect.x, rect.y, rect.width, rect.height);
    std::optional<ResolvedRect> to = ResolveRect(*dst, dst_x, dst_y, rect.width, rect.height);
    if (!from || !to || to->texture == gfx::kPreviousFrame) {
        return;
    }
    Blit(*from, *to, gfx::Filter::Nearest);
}

// The sprite of a stretch whose source TEX0 says PSMCT24: the source is sampled through a target
// without alpha, so TEXA supplies each texel's alpha before filtering, and written with blending
// off (retail's ALPHA there is (0 - 0) * FIX + Cs).
namespace {

void StretchColour24(gfx::TextureHandle src, gfx::Rect src_rect, gfx::TextureHandle dst, gfx::Rect dst_rect,
                     const sceGsTexa &texa) {
    auto width = static_cast<uint32_t>(kScreenWidth);
    auto height = static_cast<uint32_t>(kScreenHeight);
    if (src != gfx::kMainTarget) {
        std::optional<gfx::TextureInfo> info = gfx::GetTextureInfo(src);
        if (!info) {
            return;
        }
        width = info->width;
        height = info->height;
    }
    std::optional<gfx::TextureInfo> source = gfx::GetTextureInfo(src);
    bool                            frame = src == gfx::kMainTarget;
    frame = frame || (source && (source->frame_target || source->shares_main_depth));
    gfx::TextureHandle colour =
        gfx::NamedRenderTarget("stretch source PSMCT24", width, height, false, false, frame);
    if (colour == gfx::kNullTexture) {
        return;
    }
    gfx::Rect whole = {0, 0, static_cast<int32_t>(width), static_cast<int32_t>(height)};
    if (src == gfx::kMainTarget ? !gfx::SnapshotFrame(colour)
                                : !gfx::BlitTexture(src, whole, colour, whole, gfx::Filter::Nearest)) {
        return;
    }

    gfx::DrawState state;
    state.texa_aem = texa.AEM != 0;
    state.texa_ta0 = static_cast<uint8_t>(texa.TA0);
    gfx::TextureBinding binding;
    binding.texture = colour;
    binding.filter = gfx::Filter::Linear;

    auto          left = static_cast<float>(dst_rect.x);
    auto          top = static_cast<float>(dst_rect.y);
    float         right = left + static_cast<float>(dst_rect.w);
    float         bottom = top + static_cast<float>(dst_rect.h);
    auto          u0 = static_cast<float>(src_rect.x);
    auto          v0 = static_cast<float>(src_rect.y);
    float         u1 = u0 + static_cast<float>(src_rect.w);
    float         v1 = v0 + static_cast<float>(src_rect.h);
    gfx::Vertex2D corners[4] = {};
    float         xs[4] = {left, right, right, left};
    float         ys[4] = {top, top, bottom, bottom};
    float         us[4] = {u0, u1, u1, u0};
    float         vs[4] = {v0, v0, v1, v1};
    for (int i = 0; i < 4; i++) {
        corners[i].x = xs[i];
        corners[i].y = ys[i];
        corners[i].u = us[i];
        corners[i].v = vs[i];
        std::memset(corners[i].color, 0x80, sizeof(corners[i].color));
    }

    gfx::TextureHandle previous = gfx::CurrentRenderTarget();
    gfx::SetRenderTarget(dst);
    gfx::Draw2D(gfx::Primitive::Quads, corners, binding, state);
    gfx::SetRenderTarget(previous);
}

} // namespace

// One textured sprite in retail, so the source is sampled linearly; the half-pixel and field
// offsets (dx, dyy) existed only for interlacing.
//
// Both callers hand the frame over as PSMCT24, so the sprite reads no alpha from it: TEXA (AEM 1,
// TA0 0x80) makes every texel 0x80 but pure black, which is 0, and that is the alpha the copy
// carries. The title's smoke and the depth of field blend the copy back by it; a raw copy carries
// whatever the frame's last draw left, which feeds the title's trail its own decaying alpha.
void MGStretchMoveImage(sceGsTex0 *src, const CRect_i_ &src_rect, sceGsTex0 *dst, const CRect_i_ &dst_rect) {
    std::optional<ResolvedRect> from =
        ResolveRect(*src, src_rect.x / 16, src_rect.y / 16, src_rect.width / 16, src_rect.height / 16);
    std::optional<ResolvedRect> to =
        ResolveRect(*dst, dst_rect.x / 16, dst_rect.y / 16, dst_rect.width / 16, dst_rect.height / 16);
    if (from && to && from->rect.w != 0 && from->rect.h != 0 && to->rect.w != 0 && to->rect.h != 0 &&
        to->texture != gfx::kPreviousFrame) {
        if (src->PSM == SCE_GS_PSMCT24 && to->texture != gfx::kMainTarget && from->texture != to->texture) {
            sceGsTexa texa = MGPortCurrent().texa;
            texa.AEM = 1;
            texa.TA0 = 128;
            StretchColour24(from->texture, from->rect, to->texture, to->rect, texa);
            if (from->texture == gfx::kMainTarget) {
                Draw3DSetLastFrameCopy(to->texture);
            }
        } else {
            Blit(*from, *to, gfx::Filter::Linear);
        }
    }
    MGPortRestoreRegisters();
    MGPortCurrent().texa.AEM = 1;
    MGPortCurrent().texa.TA0 = 128;
}

// Retail weaves the two fields line by line into a full-height image; one blit of the frame does.
void MGMoveFrameBuffImage(sceGsTex0 *dst, int /*x*/, int /*y*/, int /*direction*/) {
    MoveFrameImage(gfx::kMainTarget, dst);
}

void MGFillBox(const CRect_i_ &rect, unsigned char r, unsigned char g, unsigned char b, unsigned char a) {
    sceGsTest test = mgPixelTest;
    test.bits.ate = 1;
    test.bits.aref = 0;
    test.bits.atst = 1;
    test.bits.zte = 1;
    test.bits.ztst = 1;
    sceGsZbuf zbuf = mgZBuffer;
    zbuf.bits.zmsk = 1;

    MGPortRegisters &current = MGPortCurrent();
    current.test = test;
    current.zbuf = zbuf;
    gfx::DrawState state = MGPortDrawState();
    state.blend = true;

    gfx::LogicalRect box = FieldRect(rect.x, rect.y, rect.width, rect.height);
    gfx::Vertex2D    corners[4] = {};
    float            xs[4] = {box.x, box.x + box.w, box.x + box.w, box.x};
    float            ys[4] = {box.y, box.y, box.y + box.h, box.y + box.h};
    for (int i = 0; i < 4; i++) {
        corners[i].x = xs[i];
        corners[i].y = ys[i];
        corners[i].color[0] = r;
        corners[i].color[1] = g;
        corners[i].color[2] = b;
        corners[i].color[3] = a;
        corners[i].fog = 0xFF;
    }
    gfx::Draw2D(gfx::Primitive::Quads, corners, {}, state);

    current.test = mgPixelTest;
    current.zbuf = mgZBuffer;
}

void MGClearZBuffer(int mode) {
    gfx::LogicalRect frame = {0.0f, 0.0f, kScreenWidth, kScreenHeight};
    uint8_t          unused[4] = {};
    gfx::Clear(false, unused, true, MGPortDepth(static_cast<unsigned>(mode)), &frame);
    MGPortRestoreRegisters();
}

void MGClearScreen(u_char r, u_char g, u_char b, u_char a) {
    gfx::LogicalRect frame = {0.0f, 0.0f, kScreenWidth, kScreenHeight};
    uint8_t          color[4] = {r, g, b, a};
    gfx::Clear(true, color, true, 0.0f, &frame);
    MGPortRestoreRegisters();
}

// ---- Shadows ---------------------------------------------------------------------------------

void MGDrawShadowFast(CFrame *frame, float *position, float *normal) {
    DrawShadowPass(frame, position, normal, 1, Draw3DShadowProgram::Every);
}

void MGDrawShadowFast2(CFrame *frame, float *position, float *normal) {
    DrawShadowPass(frame, position, normal, 1, Draw3DShadowProgram::AwayFromLight);
}

void MGDrawShadow(CFrame *frame, float *position, float *normal) {
    DrawShadowPass(frame, position, normal, 2, Draw3DShadowProgram::Clipped);
}

void MGDrawShade(CFrame *frame) {
    if (!frame) {
        return;
    }
    sceGsZbuf zbuf = mgZBuffer;
    sceGsTest test = mgPixelTest;
    zbuf.bits.zmsk = 1;
    MGSetGsZBUF(&zbuf);
    test.bits.ate = 0;
    test.bits.date = 0;
    MGSetGsTEST(&test);
    sceGsAlpha alpha = mgAlpha;
    alpha.bits.a = 2;
    alpha.bits.b = 2;
    alpha.bits.c = 2;
    alpha.bits.d = 0;
    MGSetGsALPHA(&alpha);

    mgRenderInfo.shadow_pass = 8;
    MGDraw(frame);
    mgRenderInfo.shadow_pass = 0;
}

// Retail points FRAME_1 at shadow_buf and clears it black with a sprite, Z untouched; the volumes
// then count into it against the scene's depth. The port counts in the stencil of the target being
// drawn, which holds the scene's depth: the stencil is the shadow_buf, a count of 0 its black.
void MGBeginDrawShadow(sceGsTex0 /*tex0*/) {
    sceGsTest test = mgPixelTest;
    test.bits.ate = 1;
    test.bits.aref = 0;
    test.bits.atst = SCE_GS_ALWAYS;
    test.bits.zte = 1;
    test.bits.ztst = SCE_GS_ALWAYS;
    sceGsZbuf zbuf = mgZBuffer;
    zbuf.bits.zmsk = 1;
    MGPortRegisters &current = MGPortCurrent();
    current.alpha = mgAlpha;
    current.test = test;
    current.zbuf = zbuf;

    Draw3DSetShadowTargetActive(true);
    gfx::ClearStencil(0);
}

// Retail draws shadow_buf over the frame as a 24-bit texture with TEXA AEM 1, TA0 alpha and ALPHA
// (0 - Cd) * As + Cd: a black texel (no volume covered it) gets alpha 0 and leaves the frame, any
// other darkens it by alpha / 128 once, however many volumes counted there.
void MGEndDrawShadow(u_char alpha) {
    bool active = Draw3DShadowTargetActive();
    Draw3DSetShadowTargetActive(false);

    sceGsTexa texa = mgTexa;
    texa.AEM = 1;
    texa.TA0 = alpha;
    MGPortRegisters &current = MGPortCurrent();
    current.texa = texa;

    if (active) {
        gfx::DrawState state;
        state.blend = true;
        state.alpha = {2, 1, 0, 1, 64};
        state.depth_test = gfx::DepthTest::Always;
        state.depth_write = false;
        state.stencil_test = true;
        state.stencil_front = state.stencil_back = {.compare = gfx::CompareOp::NotEqual,
                                                    .reference = 0,
                                                    .compare_mask = 0xFF,
                                                    .write_mask = 0};
        state.scissor = true;
        state.scissor_rect = current.window;
        float         xs[4] = {0.0f, kScreenWidth, kScreenWidth, 0.0f};
        float         ys[4] = {0.0f, 0.0f, kScreenHeight, kScreenHeight};
        gfx::Vertex2D quad[4] = {};
        for (int i = 0; i < 4; i++) {
            quad[i].x = xs[i];
            quad[i].y = ys[i];
            quad[i].color[0] = quad[i].color[1] = quad[i].color[2] = 0x80;
            quad[i].color[3] = alpha;
            quad[i].fog = 0xFF;
        }
        gfx::Draw2D(gfx::Primitive::Quads, quad, {}, state);
    }

    MGPortRestoreRegisters();
}

} // namespace dc::game
