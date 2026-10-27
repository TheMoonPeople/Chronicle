#include "render/draw3d.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <optional>

#include "render/screen.hpp"
#include "renderinfo.hpp"

namespace dc::render {

using namespace dc::game;

namespace {

// The GS screen centre MGSetRenderInfo puts at offset[0] and offset[1], and the depth range of
// its GS Z.
constexpr float kGsCentre = 2048.0f;
constexpr float kGsDepthRange = 16699999.0f;

bool                g_shadow_active = false;
gfx::TextureHandle  g_last_frame_copy = gfx::kNullTexture;
unsigned            g_frame_copies = 0;
gfx::TextureHandle  g_frame_picture = gfx::kNullTexture;
bool                g_frame_picture_pending = false;
Draw3DShadowProgram g_shadow_program = Draw3DShadowProgram::Every;

Draw3DTex0Resolver   g_tex0_resolver = nullptr;
Draw3DHandleResolver g_handle_resolver = nullptr;

} // namespace

void Draw3DSetResolvers(Draw3DTex0Resolver tex0, Draw3DHandleResolver handle) {
    g_tex0_resolver = tex0;
    g_handle_resolver = handle;
}

bool Draw3DHasTex0Resolver() {
    return g_tex0_resolver != nullptr;
}

PortTextureRef Draw3DResolveTex0(u_long tex0) {
    return g_tex0_resolver ? g_tex0_resolver(tex0) : PortTextureFromTex0(tex0, 0);
}

PortTextureRef Draw3DResolveHandle(int handle) {
    return g_handle_resolver ? g_handle_resolver(handle) : PortTextureFromHandle(handle);
}

void Draw3DNoteFrameCopy(gfx::TextureHandle copy) {
    g_last_frame_copy = copy;
    g_frame_copies++;
    g_frame_picture_pending = true;
}

void Draw3DSetLastFrameCopy(gfx::TextureHandle copy) {
    g_last_frame_copy = copy;
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
    // The picture is taken when first asked for after the game's copy: the water draws straight
    // after the copy, with nothing written to the frame in between.
    if (g_frame_picture_pending) {
        g_frame_picture_pending = false;
        g_frame_picture = gfx::NamedRenderTarget("frame copy picture", static_cast<uint32_t>(kScreenWidth),
                                                 static_cast<uint32_t>(kScreenHeight), false, false, true);
        if (g_frame_picture != gfx::kNullTexture && !gfx::SnapshotFrame(g_frame_picture)) {
            g_frame_picture = gfx::kNullTexture;
        }
    }
    if (g_frame_picture != gfx::kNullTexture && !gfx::GetTextureInfo(g_frame_picture)) {
        g_frame_picture = gfx::kNullTexture;
    }
    return g_frame_picture;
}

bool Draw3DShadowTargetActive() {
    return g_shadow_active;
}

void Draw3DSetShadowTargetActive(bool active) {
    g_shadow_active = active;
}

Draw3DShadowProgram Draw3DCurrentShadowProgram() {
    return g_shadow_program;
}

void Draw3DSetShadowProgram(Draw3DShadowProgram program) {
    g_shadow_program = program;
}

Draw3DExtent Draw3DVisibleExtent() {
    gfx::TextureHandle              target = gfx::CurrentRenderTarget();
    std::optional<gfx::TextureInfo> info = gfx::GetTextureInfo(target);
    gfx::LogicalRect                visible = gfx::VisibleLogicalRect(target);
    float                           width = info ? static_cast<float>(info->width) : kScreenWidth;
    float                           height = info ? static_cast<float>(info->height) : kScreenHeight;
    float                           wider = std::max({0.0f, -visible.x, visible.x + visible.w - width});
    float                           taller = std::max({0.0f, -visible.y, visible.y + visible.h - height});
    return {.half_width = kScreenWidth * 0.5f + wider,
            .half_height = SCREEN_HALF_HEIGHT_F + taller / MGPortTargetRowScale(target)};
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
    float               centre_x = info.offset[0] - (kGsCentre - kScreenWidth * 0.5f);
    float               centre_y = info.offset[1] - (kGsCentre - kScreenHeight * 0.5f);

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
                    const float * /*ambient*/, const float *specular) {
    for (int channel = 0; channel < 4; channel++) {
        constants.diffuse[channel] = diffuse[channel];
        constants.ambient_material[channel] = diffuse[channel];
        constants.specular[channel] = specular[channel];
    }
    constants.diffuse[3] = diffuse[3] * info.ambient[3] / 128.0f;
}

gfx::DrawState Draw3DState(const RenderInfo & /*info*/, bool fog) {
    gfx::DrawState state = MGPortDrawState();
    state.blend = true;
    state.fog = fog;
    float rows = MGPortTargetRowScale(gfx::CurrentRenderTarget());
    state.scissor_rect.y *= rows;
    state.scissor_rect.h *= rows;
    return state;
}

void Draw3DShadowCount(gfx::DrawState &state, bool subtract) {
    state.blend = false;
    state.depth_write = false;
    state.color_write_mask = 0;
    state.stencil_test = true;
    gfx::StencilFace face;
    face.pass = subtract ? gfx::StencilOp::DecrementClamp : gfx::StencilOp::IncrementClamp;
    state.stencil_front = state.stencil_back = face;
}

} // namespace dc::render
