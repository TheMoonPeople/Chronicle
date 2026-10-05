#include <cstring>
#include <optional>
#include <vector>

#include "draw3d.hpp"
#include "mglib.hpp"
#include "renderinfo.hpp"
#include "texture.hpp"
#include "water.hpp"

// VU1 program 15 drew the CPU-built height field with FST texel coordinates into a copy of the
// frame's field: each vertex samples the copy where the vertex itself lands on the screen, shifted
// by the ripple's slope (uv_base (320, 120) plus distortion is that offset from the field's
// centre). The port draws the field from model space with the frame's mvp and computes the same
// texel per vertex from the logical frame position, through the copy's logical mapping: the copy
// is a frame target, so water past the logical frame samples the copy's own sides.

namespace {

constexpr float kFieldCentreX = 320.0f;
constexpr float kFieldCentreY = SCREEN_QUARTER_HEIGHT_F;

// The game copies the frame into "water" (dungeon) or "water_buff" (town, title) with MGMoveImage
// and then draws with "work"'s TEX0, which retail's VRAM layout puts at the copy's address. The
// port's registry keeps them apart, so the copy the game made last wins and "work" is the
// fallback.
PortTextureRef FrameCopy() {
    gfx::TextureHandle copy = Draw3DLastFrameCopy();
    if (std::optional<gfx::TextureInfo> info = gfx::GetTextureInfo(copy); copy != gfx::kNullTexture && info) {
        PortTextureRef ref;
        ref.binding.texture = copy;
        ref.width = info->width;
        ref.height = info->height;
        ref.valid = true;
        return ref;
    }
    char      name[] = "work";
    CTexture *work = TexManager.GetTexture(TexManager.GetTextureHandle(name, -1));
    return Draw3DResolveTex0(work->tex0);
}

// Retail's surface samples the copy of the frame wherever the ripple's slope sends it, and the copy
// holds everything drawn so far: a bank or a character standing in front of the water is pulled
// into it. The copy is also a field, half the frame's rows. Here the surface is refracted from the
// frame at its own resolution, and only from where the water itself shows:
//   - Draw3DFramePicture is the frame as the game's copy last saw it;
//   - "water visible" takes the surface drawn flat (each pixel sampling itself) under the frame's
//     depth test, so it holds the frame where water shows and transparent black elsewhere;
//   - "water refracted" takes the flat surface again and, over it, the rippled samples of "water
//     visible" by their coverage, so a sample that lands on something in front of the water gives
//     way to the pixel's own colour;
//   - the frame takes the surface with the game's colour and blend, sampling "water refracted"
//     flat.
// The three share the frame's depth buffer and pixels, so every sample is taken where the vertex
// lands (gfx::kMeshScreenUv), under a display render's camera too.
struct Refraction {
    gfx::TextureHandle frame = gfx::kNullTexture;
    gfx::TextureHandle visible = gfx::kNullTexture;
    gfx::TextureHandle refracted = gfx::kNullTexture;
};

bool PrepareRefraction(Refraction &refraction) {
    static gfx::TextureHandle pictured = gfx::kNullTexture;
    static unsigned           copies = 0;

    refraction.frame = Draw3DFramePicture();
    if (gfx::CurrentRenderTarget() != gfx::kMainTarget || refraction.frame == gfx::kNullTexture) {
        return false;
    }
    uint32_t width = static_cast<uint32_t>(gfx::kLogicalWidth);
    uint32_t height = static_cast<uint32_t>(gfx::kLogicalHeight);
    refraction.visible = gfx::NamedRenderTarget("water visible", width, height, true, true);
    refraction.refracted = gfx::NamedRenderTarget("water refracted", width, height, false, true);
    if (refraction.visible == gfx::kNullTexture || refraction.refracted == gfx::kNullTexture) {
        return false;
    }
    if (pictured != refraction.frame || copies != Draw3DFrameCopies()) {
        const uint8_t clear[4] = {};
        gfx::SetRenderTarget(refraction.visible);
        gfx::Clear(true, clear, false, 0.0f);
        gfx::SetRenderTarget(refraction.refracted);
        gfx::Clear(true, clear, false, 0.0f);
        gfx::SetRenderTarget(gfx::kMainTarget);
        pictured = refraction.frame;
        copies = Draw3DFrameCopies();
    }
    return true;
}

// The surface's vertices with the ripple's shift as an offset from where each lands, in the
// frame's normalised units; the shift down is in field rows, two to a row of the frame.
std::vector<gfx::Vertex3D> RippledVertices(const CWater &water, const std::vector<gfx::Vertex3D> &vertices) {
    gfx::LogicalMapping        mapping = gfx::GetLogicalMapping(gfx::kMainTarget);
    float                      across = mapping.pixel_width ? mapping.scale_x / static_cast<float>(mapping.pixel_width) : 0.0f;
    float                      down = mapping.pixel_height ? 2.0f * mapping.scale_y / static_cast<float>(mapping.pixel_height) : 0.0f;
    std::vector<gfx::Vertex3D> rippled = vertices;
    for (int i = 0; i < water.rows; i++) {
        const float *here = &water.height[i * water.columns];
        const float *above = i == 0 ? here : here - water.columns;
        for (int j = 0; j < water.columns; j++, here++, above++) {
            gfx::Vertex3D &vertex = rippled[static_cast<size_t>(i * water.columns + j)];
            vertex.uv[0] = water.distortion * (*above - *here) * across;
            vertex.uv[1] = water.distortion * (here[0] - here[1]) * down;
        }
    }
    return rippled;
}

void DrawRefracted(const CWater &water, const Refraction &refraction, const Draw3DVisual &visual,
                   gfx::MeshConstants constants, const gfx::MeshTransform &transform, const gfx::DrawState &state) {
    std::vector<gfx::Vertex3D> rippled = RippledVertices(water, visual.vertices);
    std::vector<gfx::Vertex3D> flat = rippled;
    for (gfx::Vertex3D &vertex : flat) {
        vertex.uv[0] = 0.0f;
        vertex.uv[1] = 0.0f;
    }

    gfx::MeshConstants plain = constants;
    plain.flags = gfx::kMeshScreenUv;
    constants.flags = gfx::kMeshVertexColor | gfx::kMeshScreenUv;

    gfx::DrawState copy = state;
    copy.blend = false;
    copy.depth_write = false;
    copy.texa_aem = false;
    copy.texa_ta0 = 0x80;
    gfx::DrawState over = copy;
    over.blend = true;

    gfx::TextureBinding binding;
    binding.filter = gfx::Filter::Linear;
    binding.wrap_u = gfx::Wrap::Clamp;
    binding.wrap_v = gfx::Wrap::Clamp;

    binding.texture = refraction.frame;
    gfx::SetRenderTarget(refraction.visible);
    gfx::DrawMeshImmediate(flat, visual.indices, plain, binding, copy, &transform);
    gfx::SetRenderTarget(refraction.refracted);
    gfx::DrawMeshImmediate(flat, visual.indices, plain, binding, copy, &transform);
    binding.texture = refraction.visible;
    over.alpha = {2, 1, 0, 1, 0x80};
    gfx::DrawMeshImmediate(rippled, visual.indices, plain, binding, over, &transform);
    over.alpha = {0, 2, 2, 1, 0x80};
    gfx::DrawMeshImmediate(rippled, visual.indices, plain, binding, over, &transform);

    gfx::SetRenderTarget(gfx::kMainTarget);
    gfx::DrawState surface = state;
    surface.texa_aem = false;
    surface.texa_ta0 = 0x80;
    binding.texture = refraction.refracted;
    gfx::DrawMeshImmediate(flat, visual.indices, constants, binding, surface, &transform);
}

} // namespace

PC_OVERRIDE int CWater::CreateVUData(unsigned int *output, RenderInfo *info) {
    if (output == nullptr || rows < 2 || columns < 2) {
        return kDraw3DBlockQuads;
    }
    Draw3DVisual &visual = Draw3DRegisterVisual(output, true);
    visual.vertex_colour = true;

    sceVu0FMATRIX local_to_world;
    sceVu0FMATRIX local_to_eye;
    frame.GetLWMatrix(local_to_world);
    Draw3DMul(local_to_eye, info->view_scaled, local_to_world);

    PortTextureRef      copy = FrameCopy();
    gfx::LogicalMapping texels = {1.0f, 1.0f, 0.0f, 0.0f, copy.width, copy.height};
    if (copy.valid && gfx::GetTextureInfo(copy.binding.texture)) {
        texels = gfx::GetLogicalMapping(copy.binding.texture);
    }
    float inv_width = copy.valid && texels.pixel_width ? 1.0f / static_cast<float>(texels.pixel_width) : 0.0f;
    float inv_height = copy.valid && texels.pixel_height ? 1.0f / static_cast<float>(texels.pixel_height) : 0.0f;

    sceVu0FVECTOR row_step;
    sceVu0FVECTOR column_step;
    for (int i = 0; i < 3; i++) {
        row_step[i] = (vertex[1][i] - vertex[0][i]) / static_cast<float>(rows - 1);
        column_step[i] = (vertex[2][i] - vertex[0][i]) / static_cast<float>(columns - 1);
    }
    row_step[1] = column_step[1] = 0.0f;

    uint8_t rgba[4] = {color[0], color[1], color[2], color[3]};
    float   row_f = 0.0f;
    for (int i = 0; i < rows; i++, row_f += 1.0f) {
        sceVu0FVECTOR position = {vertex[0][0] + row_f * row_step[0], vertex[0][1] + row_f * row_step[1],
                                  vertex[0][2] + row_f * row_step[2], 1.0f};
        float        *here = &height[i * columns];
        float        *above = i == 0 ? here : here - columns;
        for (int j = 0; j < columns; j++, here++, above++) {
            gfx::Vertex3D out = {};
            std::memcpy(out.position, position, sizeof(out.position));
            std::memcpy(out.color, rgba, sizeof(rgba));

            sceVu0FVECTOR eye;
            sceVu0ApplyMatrix(eye, local_to_eye, position);
            float depth = eye[2] > 1.0f ? eye[2] : 1.0f;
            float screen_x = kFieldCentreX + info->scale[0] * eye[0] / depth;
            float screen_y = kFieldCentreY + info->scale[1] * eye[1] / depth * 0.5f;
            out.uv[0] = ((screen_x + distortion * (*above - *here)) * texels.scale_x + texels.offset_x) * inv_width;
            out.uv[1] = ((screen_y + distortion * (here[0] - here[1])) * texels.scale_y + texels.offset_y) * inv_height;
            visual.vertices.push_back(out);

            // Retail transforms the cell before lifting it, so each vertex carries the height of
            // the cell before it.
            position[0] += column_step[0];
            position[2] += column_step[2];
            position[1] = *here * height_scale;
        }
    }

    for (int i = 0; i < rows - 1; i++) {
        std::vector<uint32_t> strip;
        for (int j = 0; j < columns; j++) {
            strip.push_back(static_cast<uint32_t>(i * columns + j));
            strip.push_back(static_cast<uint32_t>((i + 1) * columns + j));
        }
        size_t first = visual.indices.size();
        Draw3DStripToList(visual.indices, 0, static_cast<uint32_t>(strip.size()));
        for (size_t k = first; k < visual.indices.size(); k++) {
            visual.indices[k] = strip[visual.indices[k]];
        }
    }

    Draw3DStrip strip;
    strip.index_count = static_cast<uint32_t>(visual.indices.size());
    visual.strips.push_back(strip);
    return kDraw3DBlockQuads;
}

PC_OVERRIDE extern "C" int DrawVu1__6CWaterFP10RenderInfoP13sceVif1PacketP1(CWater *water, RenderInfo *info,
                                                                            sceVif1Packet *draw_packet, void *parent_info) {
    if (water->CheckClip() != 0) {
        return 0;
    }
    u_int *block = water->packet[!DBuffID + 1];
    if (block == nullptr) {
        return 0;
    }

    sceGsTest test = mgPixelTest;
    test.bits.ate = 0;
    test.bits.date = 0;
    MGPortCurrent().test = test;

    sceVu0FMATRIX local_to_world;
    water->frame.GetLWMatrix(local_to_world);
    water->CreateVUData(block, &mgRenderInfo);
    info->fog_enabled = false;

    const Draw3DVisual *visual = Draw3DFindVisual(block);
    if (visual != nullptr && !visual->indices.empty()) {
        Draw3DIdentityScope identity(water, kDraw3DTeleportDistance);
        gfx::MeshConstants  constants = {};
        gfx::MeshTransform  transform;
        Draw3DSceneConstants(constants, *info, local_to_world, &transform);
        constants.flags = gfx::kMeshVertexColor;
        for (float &value : constants.diffuse) {
            value = 1.0f;
        }
        gfx::DrawState state = Draw3DState(*info, false);
        Refraction     refraction;
        if (PrepareRefraction(refraction)) {
            DrawRefracted(*water, refraction, *visual, constants, transform, state);
        } else {
            PortTextureRef      copy = FrameCopy();
            gfx::TextureBinding binding;
            if (copy.valid && copy.binding.texture != gfx::CurrentRenderTarget() &&
                copy.binding.texture != gfx::kMainTarget) {
                binding = copy.binding;
                binding.filter = gfx::Filter::Linear;
                binding.wrap_u = gfx::Wrap::Clamp;
                binding.wrap_v = gfx::Wrap::Clamp;
            }
            gfx::DrawMeshImmediate(visual->vertices, visual->indices, constants, binding, state, &transform);
        }
    }

    MGPortCurrent().test = mgPixelTest;
    return 0;
}
