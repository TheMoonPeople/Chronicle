#include "mglib.hpp"
#include "types.h"

#include <libvu0.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <optional>

#include "collision.hpp"
#include "dataalloc.hpp"
#include "dataset.hpp"
#include "draw3d.hpp"
#include "frame.hpp"
#include "gameloop.hpp"
#include "mathutil.hpp"
#include "platform/clock.hpp"
#include "rect.hpp"
#include "renderinfo.hpp"
#include "texture.hpp"

// Retail defines the window rectangle as mgWindowRectStore and renames it at link time, which the
// port's merge has no equivalent of.
CRect_i_ mgWindowRect;

// Defined by ps2/src/mglib.cpp without a header declaration.
extern int           mgWaitVSync;
extern int           mgAdjustX;
extern int           mgAdjustY;
extern sceVu0FVECTOR mgZeroVector;
extern sceVu0FVECTOR mgUnitVector;
extern sceVu0FVECTOR mgZeroVector2;
extern sceVu0FVECTOR mgUnitVector2;

namespace {

// PAL's field buffer is half the frame's height. GS coordinates the game hands to the 2D units
// keep counting field rows (MGPortLogicalY doubles them), so the eye-to-GS step keeps the squeeze
// that retail baked into view_scaled.
constexpr float kFieldSqueeze = 0.5f;
constexpr float kGsCentre = 2048.0f;
constexpr float kGsDepthRange = 16699999.0f;

int                g_old_vcount;
sceVif1Packet      g_null_packet;
u_long128          g_null_packet_words[4];
unsigned int       g_draw_cursor[64];
std::optional<int> g_pick_pending[16];

gfx::TextureHandle  g_shadow_target = gfx::kNullTexture;
gfx::TextureHandle  g_last_frame_copy = gfx::kNullTexture;
unsigned            g_frame_copies = 0;
gfx::TextureHandle  g_frame_picture = gfx::kNullTexture;
gfx::TextureHandle  g_shadow_previous = gfx::kMainTarget;
Draw3DShadowProgram g_shadow_program = Draw3DShadowProgram::Every;

// Retail retargets FRAME_1 to the game's shadow_buf and keeps ZBUF, so the volumes count against
// the scene's depth. The port's equivalent is a target of its own that shares the main depth
// buffer; the game's shadow_buf is only ever read back by MGEndDrawShadow, which reads this.
constexpr const char *kShadowTargetName = "shadow volumes";

Draw3DTex0Resolver   g_tex0_resolver = nullptr;
Draw3DHandleResolver g_handle_resolver = nullptr;

// A camera that moves further or turns more than this between ticks has cut, and is not
// interpolated across.
constexpr float kCameraCutDistance = 200.0f;
constexpr float kCameraCutCosine = 0.7071f;

bool          g_cut_next_tick = true;
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
        {left,               top,                 -left,                      bottom - top                },
        {gfx::kLogicalWidth, top,                 right - gfx::kLogicalWidth, bottom - top                },
        {0.0f,               top,                 gfx::kLogicalWidth,         -top                        },
        {0.0f,               gfx::kLogicalHeight, gfx::kLogicalWidth,         bottom - gfx::kLogicalHeight},
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
    PortTextureRef ref = Draw3DResolveTex0(*reinterpret_cast<const u_long *>(&tex0));
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
    uint32_t                  width = static_cast<uint32_t>(std::abs(src.w));
    uint32_t                  height = static_cast<uint32_t>(std::abs(src.h));
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
        g_last_frame_copy = dst.texture;
        g_frame_copies++;
        g_frame_picture = gfx::NamedRenderTarget("frame copy picture", static_cast<uint32_t>(gfx::kLogicalWidth),
                                                 static_cast<uint32_t>(gfx::kLogicalHeight), false, false, true);
        if (g_frame_picture != gfx::kNullTexture && !gfx::SnapshotFrame(g_frame_picture)) {
            g_frame_picture = gfx::kNullTexture;
        }
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
    g_shadow_program = program;
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

// ---- Shared with the other 3D units ----------------------------------------------------------

void Draw3DSetResolvers(Draw3DTex0Resolver tex0, Draw3DHandleResolver handle) {
    g_tex0_resolver = tex0;
    g_handle_resolver = handle;
}

PortTextureRef Draw3DResolveTex0(u_long tex0) {
    return g_tex0_resolver ? g_tex0_resolver(tex0) : PortTextureFromTex0(tex0, 0);
}

PortTextureRef Draw3DResolveHandle(int handle) {
    return g_handle_resolver ? g_handle_resolver(handle) : PortTextureFromHandle(handle);
}

gfx::TextureHandle Draw3DLastFrameCopy() {
    if (g_last_frame_copy != gfx::kNullTexture && !gfx::GetTextureInfo(g_last_frame_copy)) {
        g_last_frame_copy = gfx::kNullTexture;
    }
    return g_last_frame_copy;
}

unsigned Draw3DFrameCopies() {
    return g_frame_copies;
}

gfx::TextureHandle Draw3DFramePicture() {
    if (g_frame_picture != gfx::kNullTexture && !gfx::GetTextureInfo(g_frame_picture)) {
        g_frame_picture = gfx::kNullTexture;
    }
    return g_frame_picture;
}

bool Draw3DShadowTargetActive() {
    return g_shadow_target != gfx::kNullTexture;
}

Draw3DShadowProgram Draw3DCurrentShadowProgram() {
    return g_shadow_program;
}

Draw3DExtent Draw3DVisibleExtent() {
    gfx::TextureHandle              target = gfx::CurrentRenderTarget();
    std::optional<gfx::TextureInfo> info = gfx::GetTextureInfo(target);
    gfx::LogicalRect                visible = gfx::VisibleLogicalRect(target);
    float                           width = info ? static_cast<float>(info->width) : gfx::kLogicalWidth;
    float                           height = info ? static_cast<float>(info->height) : gfx::kLogicalHeight;
    float                           wider = std::max({0.0f, -visible.x, visible.x + visible.w - width});
    float                           taller = std::max({0.0f, -visible.y, visible.y + visible.h - height});
    return {gfx::kLogicalWidth * 0.5f + wider, SCREEN_HALF_HEIGHT_F + taller / MGPortTargetRowScale(target)};
}

bool MGPortFrameTarget(std::string_view name) {
    while (name.starts_with('#')) {
        name.remove_prefix(1);
    }
    return name.starts_with("frame_") || name.starts_with("water_buff") || name == "water" ||
           name.starts_with("water#");
}

void Draw3DMul(float out[4][4], const float a[4][4], const float b[4][4]) {
    float result[4][4];
    for (int column = 0; column < 4; column++) {
        for (int row = 0; row < 4; row++) {
            result[column][row] = a[0][row] * b[column][0] + a[1][row] * b[column][1] + a[2][row] * b[column][2] +
                                  a[3][row] * b[column][3];
        }
    }
    std::memcpy(out, result, sizeof(result));
}

// The logical frame point of an eye-space (x, y, z) is (320 + sx x / z, 240 + sy y / z), the
// point MGRotTransPers2D gives, and that lands on the target through its logical mapping (with
// the field-height targets taking half the rows). Depth is a + b / z with the GS Z of
// MGSetRenderInfo, offset[2] + scale[2] / z, read through MGPortDepth.
void Draw3DEyeToClip(const RenderInfo &info, float clip[4][4]) {
    gfx::TextureHandle  target = gfx::CurrentRenderTarget();
    gfx::LogicalMapping mapping = gfx::GetLogicalMapping(target);
    float               rows = MGPortTargetRowScale(target);
    float               width = static_cast<float>(std::max(mapping.pixel_width, 1u));
    float               height = static_cast<float>(std::max(mapping.pixel_height, 1u));
    float               kx = 2.0f * mapping.scale_x / width;
    float               ox = 2.0f * mapping.offset_x / width - 1.0f;
    float               ky = 2.0f * mapping.scale_y * rows / height;
    float               oy = 2.0f * mapping.offset_y / height - 1.0f;
    float               centre_x = info.offset[0] - (kGsCentre - gfx::kLogicalWidth * 0.5f);
    float               centre_y = info.offset[1] - (kGsCentre - gfx::kLogicalHeight * 0.5f);

    std::memset(clip, 0, sizeof(float) * 16);
    clip[0][0] = kx * info.scale[0];
    clip[2][0] = kx * centre_x + ox;
    clip[1][1] = ky * info.scale[1];
    clip[2][1] = ky * centre_y + oy;
    clip[2][2] = (info.offset[2] - 1.0f) / kGsDepthRange;
    clip[3][2] = info.scale[2] / kGsDepthRange;
    clip[2][3] = 1.0f;
}

void Draw3DSceneConstants(gfx::MeshConstants &constants, const RenderInfo &info, const float model[4][4],
                          gfx::MeshTransform *transform) {
    float eye_to_clip[4][4];
    float world_to_clip[4][4];
    float model_to_clip[4][4];
    Draw3DEyeToClip(info, eye_to_clip);
    Draw3DMul(world_to_clip, eye_to_clip, info.view_scaled);
    bool projected = info.shadow_pass == 1 || info.shadow_pass == 2;
    if (transform != nullptr) {
        *transform = gfx::IdentityMeshTransform();
        std::memcpy(transform->projection, eye_to_clip, sizeof(transform->projection));
        std::memcpy(transform->view, info.view_scaled, sizeof(transform->view));
        std::memcpy(transform->model, model, sizeof(transform->model));
        if (projected) {
            std::memcpy(transform->middle, info.shadow, sizeof(transform->middle));
        }
    }
    if (projected) {
        float shadow[4][4];
        Draw3DMul(shadow, info.shadow, model);
        Draw3DMul(model_to_clip, world_to_clip, shadow);
    } else {
        Draw3DMul(model_to_clip, world_to_clip, model);
    }
    std::memcpy(constants.mvp, model_to_clip, sizeof(model_to_clip));

    // The model's axes rescaled to unit length; the shadow passes divide by the squared length,
    // which only matters to VU1's own shadow arithmetic and lighting is off there.
    for (int axis = 0; axis < 3; axis++) {
        float length2 = model[axis][0] * model[axis][0] + model[axis][1] * model[axis][1] + model[axis][2] * model[axis][2];
        float inverse = length2 > 0.0f ? 1.0f / std::sqrt(length2) : 0.0f;
        if (info.shadow_pass != 0) {
            inverse *= inverse;
        }
        for (int row = 0; row < 3; row++) {
            constants.normal_matrix[axis * 4 + row] = model[axis][row] * inverse;
        }
        constants.normal_matrix[axis * 4 + 3] = 0.0f;
    }

    // light_direction keeps each light reversed in a column (sceVu0NormalLightMatrix), the colours
    // one per row; both are in GS colour units, the renderer's in modulations of 0x80.
    for (int light = 0; light < 4; light++) {
        for (int row = 0; row < 3; row++) {
            constants.light_direction[light][row] = -info.light_direction[row][light];
        }
        constants.light_direction[light][3] = 0.0f;
        for (int channel = 0; channel < 3; channel++) {
            constants.light_color[light][channel] = info.light_color[light][channel] / 128.0f;
        }
        constants.light_color[light][3] = 0.0f;
    }
    constants.light_count = 4;
    for (int channel = 0; channel < 4; channel++) {
        constants.ambient[channel] = info.ambient[channel] / 128.0f;
    }

    constants.fog[0] = info.fog_a;
    constants.fog[1] = info.fog_b;
    constants.fog[2] = info.fog_far;
    constants.fog[3] = info.fog_near;
}

// The scene's ambient alpha is what the game fades models with (dungeonmap distance fades,
// monster palette alpha), so it scales the material's opacity; 128 leaves it as it is. Vu_prog0f
// multiplies the ambient colour and every light by the material's first vector alone, so the
// ambient light takes the diffuse colour; the second vector only tints the specular programs'
// highlight and never scales the ambient (the cloth's is 0.3 under a diffuse of 1).
void Draw3DMaterial(gfx::MeshConstants &constants, const RenderInfo &info, const float *diffuse,
                    const float *ambient, const float *specular) {
    for (int channel = 0; channel < 4; channel++) {
        constants.diffuse[channel] = diffuse[channel];
        constants.ambient_material[channel] = diffuse[channel];
        constants.specular[channel] = specular[channel];
    }
    constants.diffuse[3] = diffuse[3] * info.ambient[3] / 128.0f;
}

gfx::DrawState Draw3DState(const RenderInfo &info, bool fog) {
    gfx::DrawState state = MGPortDrawState();
    state.blend = true;
    state.fog = fog;
    float rows = MGPortTargetRowScale(gfx::CurrentRenderTarget());
    state.scissor_rect.y *= rows;
    state.scissor_rect.h *= rows;
    (void) info;
    return state;
}

void MGPortWorldToClip(float clip[4][4]) {
    float eye_to_clip[4][4];
    Draw3DEyeToClip(mgRenderInfo, eye_to_clip);
    Draw3DMul(clip, eye_to_clip, mgRenderInfo.view_scaled);
}

// ---- VSync group -----------------------------------------------------------------------------

PC_OVERRIDE int MGGetVSyncCount() {
    return static_cast<int>(ClockTickCount());
}

PC_OVERRIDE void MGInit() {
    DBuffID = 0;
    mgWaitVSync = 0;
    VSyncField__2 = 0;
    g_null_packet.pBase = reinterpret_cast<u_int *>(g_null_packet_words);
    g_null_packet.pCurrent = reinterpret_cast<u_int *>(g_null_packet_words);
    Vif1Packet = &g_null_packet;

    *reinterpret_cast<u_long128 *>(&GiftagAD) = 0;
    GiftagAD.EOP = 1;
    GiftagAD.NREG = 1;
    GiftagAD.REGS0 = SCE_GIF_PACKED_AD;

    mgTopVRAM = 0x1E00;
    mgZBufferAdr = 0x1400;

    mgWindowRect.x = 0;
    mgWindowRect.y = 0;
    mgWindowRect.width = 640;
    mgWindowRect.height = SCREEN_HALF_HEIGHT;
    MGPortCurrent().window = {0.0f, 0.0f, gfx::kLogicalWidth, gfx::kLogicalHeight};
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

PC_OVERRIDE void MGInitVSyncCallBack(int (*callback)(int)) {
    ClockSetTickCallback(callback);
}

void MGPortCutInterpolation() {
    g_cut_next_tick = true;
}

// A tick records its drawing as a display list; MGEndFrame renders it. A frame someone opened
// with gfx::BeginFrame is drawn into as it is.
PC_OVERRIDE void MGBeginFrame() {
    if (!gfx::InFrame()) {
        gfx::BeginRecording();
        if (g_cut_next_tick) {
            gfx::CutInterpolation();
            g_cut_next_tick = false;
        }
    }
    gfx::SetRenderTarget(gfx::kMainTarget);
    g_shadow_target = gfx::kNullTexture;

    Vif1Packet = &g_null_packet;
    ActiveData = DBuffID ? &ActiveData0 : &ActiveData1;
    ActiveData->used = 0;
    if (WorkBuffer) {
        WorkBuffer->used = 0;
    }
    Draw3DSweepVisuals();

    *reinterpret_cast<u_long128 *>(&GiftagAD) = 0;
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
PC_OVERRIDE void MGEndFrame() {
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
    }

    gfx::SetRenderTarget(gfx::kMainTarget);
    gfx::DisplayListRef list;
    if (gfx::Recording()) {
        list = gfx::EndRecording();
        GameRenderTick(list);
        sceVu0CopyMatrix(g_tick_view, mgRenderInfo.view_scaled);
        g_have_tick_view = true;
    } else {
        gfx::EndFrame();
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

    if (!mgWaitVSync) {
        ClockPump();
    }
    if (mgWaitVSync || ClockTickCount() == g_old_vcount) {
        ClockWaitNextTick(GamePresentBetweenTicks);
        ClockPump();
    }
    g_old_vcount = static_cast<int>(ClockTickCount());
    GamePresentTickEnd();

    DBuffID = !DBuffID;
}

PC_OVERRIDE void MGFlipWaitVSync(int wait) {
    mgWaitVSync = wait;
}

// The display position only moved the PAL picture on a television; the clamped values are kept
// for the save data's screen-position setting, and nothing moves.
PC_OVERRIDE void MGAdjustScreen(int x, int y) {
    if (x > 32 || x < -32) {
        x = 0;
    }
    if (y > 32 || y < -32) {
        y = 0;
    }
    mgAdjustX = (x >> 1) << 1;
    mgAdjustY = (y >> 1) << 1;
}

PC_OVERRIDE sceVif1Packet *GetVif1Packet() {
    return Vif1Packet;
}

// ---- Projection, camera, lights, fog ---------------------------------------------------------

PC_OVERRIDE void MGSetRenderInfo(float scale, float near_z, float far_z) {
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

PC_OVERRIDE void MGSetProjection(float scale) {
    MGSetRenderInfo(scale, mgRenderInfo.near[2], mgRenderInfo.far[2]);
}

PC_OVERRIDE float MGGetProjection() {
    return mgRenderInfo.projection;
}

// SCAX0 takes half of x, as retail sends it; the rectangle counts field rows.
PC_OVERRIDE void MGSetWindowRect(CRect_i_ rect) {
    mgWindowRect = rect;
    float left = static_cast<float>(rect.x >> 1);
    float right = static_cast<float>(rect.x + rect.width + 1);
    float top = static_cast<float>(rect.y * 2);
    float bottom = static_cast<float>((rect.y + rect.height + 1) * 2);
    MGPortCurrent().window = {left, top, right - left, bottom - top};
}

PC_OVERRIDE void MGSetWindowRect() {
    MGSetWindowRect(CRect_i_(0, 0, 640, SCREEN_HALF_HEIGHT));
    MGPortCurrent().window = {0.0f, 0.0f, gfx::kLogicalWidth, gfx::kLogicalHeight};
}

PC_OVERRIDE void MGSetPLight(sceVu0FMATRIX direction, sceVu0FMATRIX color) {
    sceVu0CopyMatrix(mgRenderInfo.light_direction, direction);
    sceVu0CopyMatrix(mgRenderInfo.light_color, color);
    for (int i = 0; i < 4; i++) {
        mgRenderInfo.light_color[i][3] = 0.0f;
        mgRenderInfo.light_direction[3][i] = 0.0f;
    }
}

PC_OVERRIDE void MGGetPLight(sceVu0FMATRIX direction, sceVu0FMATRIX color) {
    sceVu0CopyMatrix(direction, mgRenderInfo.light_direction);
    sceVu0CopyMatrix(color, mgRenderInfo.light_color);
}

PC_OVERRIDE void MGSetAmbient(float *ambient) {
    sceVu0CopyVector(mgRenderInfo.ambient, ambient);
}

PC_OVERRIDE void MGGetAmbient(float *ambient) {
    sceVu0CopyVector(ambient, mgRenderInfo.ambient);
}

PC_OVERRIDE void MGSetViewMatrix(sceVu0FMATRIX view) {
    SetViewMatrix(view);
}

PC_OVERRIDE void MGSetViewMatrix(sceVu0FMATRIX view, float *position) {
    SetViewMatrix(view);
    sceVu0CopyVector(mgRenderInfo.view_position, position);
    mgRenderInfo.position[0] = position[0];
    mgRenderInfo.position[1] = position[1];
    mgRenderInfo.position[2] = position[2];
    mgRenderInfo.position[3] = 0.0f;
}

PC_OVERRIDE void MGSetFogParm(float near_z, float far_z, u_char r, u_char g, u_char b, float far_fog, float near_fog) {
    mgRenderInfo.fog_far = far_fog;
    mgRenderInfo.fog_near = near_fog;
    mgRenderInfo.fog_a = ((far_fog + near_fog) + (far_fog - near_fog) * (far_z + near_z) / (far_z - near_z)) / 2.0f;
    mgRenderInfo.fog_b = -far_z * near_z * (far_fog - near_fog) / (far_z - near_z);
    mgRenderInfo.fog_red = r;
    mgRenderInfo.fog_green = g;
    mgRenderInfo.fog_blue = b;
}

PC_OVERRIDE void MGSetBGColor(float red, float green, float blue, float alpha) {
    mgClearBackFlag = !(red < 0.0f && green < 0.0f && blue < 0.0f && alpha < 0.0f);
    mgBackColor[0] = std::clamp(red, 0.0f, 255.0f);
    mgBackColor[1] = std::clamp(green, 0.0f, 255.0f);
    mgBackColor[2] = std::clamp(blue, 0.0f, 255.0f);
    mgBackColor[3] = std::clamp(alpha, 0.0f, 255.0f);
}

PC_OVERRIDE void MGSetBGColor(float *color) {
    MGSetBGColor(color[0], color[1], color[2], color[3]);
}

PC_OVERRIDE void MGGetBGColor(float *color) {
    sceVu0CopyVector(color, mgBackColor);
}

PC_OVERRIDE void MGScisioringForce(int force) {
    mgRenderInfo.scissoring = force;
}

// ---- CPU projections -------------------------------------------------------------------------

// view_scaled no longer halves y, so the field squeeze is applied here to keep the GS coordinates
// retail produced.
PC_OVERRIDE int MGRotTransPers(int *screen, float *position, int fog) {
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

// Logical frame pixels: retail doubled the squeezed y back, which with an unsqueezed view is the
// identity. The on-screen test still runs in GS field units, as retail's did.
PC_OVERRIDE int MGRotTransPers2D(int *screen, float *position, int fog) {
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

// view_screen keeps retail's GS field units, so the vertical bound is half the frame's half-height;
// both bounds are what the current target shows, which past a 4:3 window is wider than retail's.
PC_OVERRIDE int MGClipVertex(float *position) {
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

PC_OVERRIDE int MGClipBox(CBoxVu0 *box) {
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

PC_OVERRIDE void MGDraw(CFrame *frame) {
    if (frame) {
        frame->DrawVu1(g_draw_cursor, &mgRenderInfo);
    }
}

PC_OVERRIDE void MGSetGsTEST(sceGsTest *test) {
    MGPortCurrent().test = test ? *test : mgPixelTest;
}

PC_OVERRIDE void MGSetGsZBUF(sceGsZbuf *zbuf) {
    MGPortCurrent().zbuf = zbuf ? *zbuf : mgZBuffer;
}

PC_OVERRIDE void MGSetGsALPHA(sceGsAlpha *alpha) {
    MGPortCurrent().alpha = alpha ? *alpha : mgAlpha;
}

PC_OVERRIDE void MGSetGsTEXA(sceGsTexa *texa) {
    MGPortCurrent().texa = texa ? *texa : mgTexa;
}

PC_OVERRIDE void MGGetFBuffTex(sceGsTex0 *tex0) {
    *reinterpret_cast<u_long *>(tex0) = FrameTex0(kMGPortFrameTbp0);
}

PC_OVERRIDE void MGGetFBuffBackTex(sceGsTex0 *tex0) {
    *reinterpret_cast<u_long *>(tex0) = FrameTex0(kMGPortPreviousFrameTbp0);
}

// Moves between two of the texture manager's images (CLUT moves included) are its business; a move
// that reads or writes the frame scales the frame's field rows to its logical ones.
PC_OVERRIDE void MGMoveImage(sceGsTex0 *src, const CRect_i_ &rect, sceGsTex0 *dst, int dst_x, int dst_y, int direction) {
    if (rect.width <= 0 || rect.height <= 0) {
        return;
    }
    if (g_tex0_resolver == nullptr && !IsFrame(*src) && !IsFrame(*dst)) {
        PortMoveImage(*src, rect.x, rect.y, rect.width, rect.height, *dst, dst_x, dst_y);
        return;
    }
    std::optional<ResolvedRect> from = ResolveRect(*src, rect.x, rect.y, rect.width, rect.height);
    std::optional<ResolvedRect> to = ResolveRect(*dst, dst_x, dst_y, rect.width, rect.height);
    if (!from || !to || to->texture == gfx::kPreviousFrame) {
        return;
    }
    Blit(*from, *to, gfx::Filter::Nearest);
    (void) direction;
}

// The sprite of a stretch whose source TEX0 says PSMCT24: the source is sampled through a target
// without alpha, so TEXA supplies each texel's alpha before filtering, and written with blending
// off (retail's ALPHA there is (0 - 0) * FIX + Cs).
static void StretchColour24(gfx::TextureHandle src, gfx::Rect src_rect, gfx::TextureHandle dst, gfx::Rect dst_rect,
                            const sceGsTexa &texa) {
    uint32_t width = static_cast<uint32_t>(gfx::kLogicalWidth);
    uint32_t height = static_cast<uint32_t>(gfx::kLogicalHeight);
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

    float         left = static_cast<float>(dst_rect.x);
    float         top = static_cast<float>(dst_rect.y);
    float         right = left + static_cast<float>(dst_rect.w);
    float         bottom = top + static_cast<float>(dst_rect.h);
    float         u0 = static_cast<float>(src_rect.x);
    float         v0 = static_cast<float>(src_rect.y);
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

// One textured sprite in retail, so the source is sampled linearly; the half-pixel and field
// offsets (dx, dyy) existed only for interlacing.
//
// Both callers hand the frame over as PSMCT24, so the sprite reads no alpha from it: TEXA (AEM 1,
// TA0 0x80) makes every texel 0x80 but pure black, which is 0, and that is the alpha the copy
// carries. The title's smoke and the depth of field blend the copy back by it; a raw copy carries
// whatever the frame's last draw left, which feeds the title's trail its own decaying alpha.
PC_OVERRIDE void MGStretchMoveImage(sceGsTex0 *src, const CRect_i_ &src_rect, sceGsTex0 *dst, const CRect_i_ &dst_rect) {
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
                g_last_frame_copy = to->texture;
            }
        } else {
            Blit(*from, *to, gfx::Filter::Linear);
        }
    }
    MGPortRestoreRegisters();
    MGPortCurrent().texa.AEM = 1;
    MGPortCurrent().texa.TA0 = 128;
}

static void MoveFrameImage(gfx::TextureHandle frame, sceGsTex0 *dst) {
    PortTextureRef ref = Draw3DResolveTex0(*reinterpret_cast<u_long *>(dst));
    if (!ref.valid || ref.binding.texture == gfx::kNullTexture || ref.binding.texture == gfx::kMainTarget ||
        ref.binding.texture == gfx::kPreviousFrame) {
        return;
    }
    gfx::BlitTexture(frame, {0, 0, 640, SCREEN_HEIGHT}, ref.binding.texture, {0, 0, 640, SCREEN_HEIGHT},
                     gfx::Filter::Nearest);
}

// Retail weaves the two fields line by line into a full-height image; one blit of the frame does.
PC_OVERRIDE void MGMoveFrameBuffImage(sceGsTex0 *dst, int x, int y, int direction) {
    MoveFrameImage(gfx::kMainTarget, dst);
    (void) x;
    (void) y;
    (void) direction;
}

void MGPortMovePreviousFrameImage(sceGsTex0 *dst) {
    MoveFrameImage(gfx::kPreviousFrame, dst);
}

PC_OVERRIDE void MGFillBox(const CRect_i_ &rect, unsigned char r, unsigned char g, unsigned char b, unsigned char a) {
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

PC_OVERRIDE void MGClearZBuffer(int mode) {
    gfx::LogicalRect frame = {0.0f, 0.0f, gfx::kLogicalWidth, gfx::kLogicalHeight};
    uint8_t          unused[4] = {};
    gfx::Clear(false, unused, true, MGPortDepth(static_cast<unsigned>(mode)), &frame);
    MGPortRestoreRegisters();
}

PC_OVERRIDE void MGClearScreen(u_char r, u_char g, u_char b, u_char a) {
    gfx::LogicalRect frame = {0.0f, 0.0f, gfx::kLogicalWidth, gfx::kLogicalHeight};
    uint8_t          color[4] = {r, g, b, a};
    gfx::Clear(true, color, true, 0.0f, &frame);
    MGPortRestoreRegisters();
}

// ---- Shadows ---------------------------------------------------------------------------------

PC_OVERRIDE void MGDrawShadowFast(CFrame *frame, float *position, float *normal) {
    DrawShadowPass(frame, position, normal, 1, Draw3DShadowProgram::Every);
}

PC_OVERRIDE void MGDrawShadowFast2(CFrame *frame, float *position, float *normal) {
    DrawShadowPass(frame, position, normal, 1, Draw3DShadowProgram::AwayFromLight);
}

PC_OVERRIDE void MGDrawShadow(CFrame *frame, float *position, float *normal) {
    DrawShadowPass(frame, position, normal, 2, Draw3DShadowProgram::Clipped);
}

PC_OVERRIDE void MGDrawShade(CFrame *frame) {
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
// then count into it against the scene's depth.
PC_OVERRIDE void MGBeginDrawShadow(sceGsTex0 tex0) {
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
    (void) tex0;

    gfx::TextureHandle target = gfx::NamedRenderTarget(kShadowTargetName, 640, SCREEN_HEIGHT, false, true);
    if (target == gfx::kNullTexture) {
        g_shadow_target = gfx::kNullTexture;
        return;
    }
    if (g_shadow_target == gfx::kNullTexture) {
        g_shadow_previous = gfx::CurrentRenderTarget();
    }
    g_shadow_target = target;
    gfx::SetRenderTarget(target);
    uint8_t black[4] = {0, 0, 0, 0x80};
    gfx::Clear(true, black, false, 0.0f);
}

// Retail draws shadow_buf over the frame as a 24-bit texture with TEXA AEM 1, TA0 alpha and ALPHA
// (0 - Cd) * As + Cd: a black texel (no volume covered it) gets alpha 0 and leaves the frame, any
// other darkens it by alpha / 128 once, however many volumes counted there.
PC_OVERRIDE void MGEndDrawShadow(u_char alpha) {
    gfx::TextureHandle target = g_shadow_target;
    g_shadow_target = gfx::kNullTexture;

    sceGsTexa texa = mgTexa;
    texa.AEM = 1;
    texa.TA0 = alpha;
    MGPortRegisters &current = MGPortCurrent();
    current.texa = texa;

    if (target != gfx::kNullTexture) {
        gfx::SetRenderTarget(g_shadow_previous);
        gfx::DrawState state;
        state.blend = true;
        state.alpha = {2, 1, 0, 1, 64};
        state.depth_test = gfx::DepthTest::Always;
        state.depth_write = false;
        state.texa_aem = true;
        state.texa_ta0 = alpha;
        state.scissor = true;
        state.scissor_rect = current.window;
        float         xs[4] = {0.0f, gfx::kLogicalWidth, gfx::kLogicalWidth, 0.0f};
        float         ys[4] = {0.0f, 0.0f, gfx::kLogicalHeight, gfx::kLogicalHeight};
        gfx::Vertex2D quad[4] = {};
        for (int i = 0; i < 4; i++) {
            quad[i].x = quad[i].u = xs[i];
            quad[i].y = quad[i].v = ys[i];
            quad[i].color[0] = quad[i].color[1] = quad[i].color[2] = quad[i].color[3] = 0x80;
            quad[i].fog = 0xFF;
        }
        gfx::TextureBinding binding;
        binding.texture = target;
        binding.filter = gfx::Filter::Nearest;
        gfx::Draw2D(gfx::Primitive::Quads, quad, binding, state);
    }

    MGPortRestoreRegisters();
}
