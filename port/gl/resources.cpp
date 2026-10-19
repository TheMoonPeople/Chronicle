// Textures, palettes, render targets and meshes, as Kazarov's resources.cpp keeps them: the same
// handles (a generation over a slot), the same target sizes and mappings, the same palette
// expansions. GL orders every call as it is made, so what the Vulkan renderer does with barriers,
// upload command buffers and deferred destruction is simply done in place here.

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>

#include "gl.hpp"

namespace gfx::detail {

namespace {

constexpr uint32_t kSlotBits = 16;
constexpr uint32_t kSlotMask = (1u << kSlotBits) - 1;

uint32_t MakeHandle(uint16_t generation, uint32_t slot) { return (static_cast<uint32_t>(generation) << kSlotBits) | slot; }

uint16_t NextGeneration(uint16_t generation) { return generation == 0xFFFF ? 1 : generation + 1; }

uint32_t TexelSize(TextureFormat format) { return format == TextureFormat::Index8 ? 1 : 4; }

GLenum GlFormat(TextureFormat format) { return format == TextureFormat::Index8 ? GL_R8 : GL_RGBA8; }

constexpr float kOpaqueBlack[4] = {0.0f, 0.0f, 0.0f, 1.0f};
constexpr float kZero[4] = {0.0f, 0.0f, 0.0f, 0.0f};

// Zeroed, as the image is cleared.
void KeepContents(GlTexture &texture) {
    uint32_t texel = TexelSize(texture.desc.format);
    texture.contents.resize(texture.desc.mip_levels);
    for (uint32_t mip = 0; mip < texture.desc.mip_levels; mip++) {
        uint32_t width = std::max(1u, texture.desc.width >> mip);
        uint32_t height = std::max(1u, texture.desc.height >> mip);
        texture.contents[mip].assign(static_cast<size_t>(width) * height * texel, 0);
    }
}

// The expansions made from a texture, once nothing draws it any more.
void ForgetExpansions(TextureHandle handle) {
    for (auto it = g.expansions.begin(); it != g.expansions.end();) {
        if (static_cast<TextureHandle>(it->first >> 32) == handle || static_cast<TextureHandle>(it->first) == handle) {
            TextureHandle rgba = it->second.rgba;
            it = g.expansions.erase(it);
            DestroyDoomedTexture(rgba);
        } else {
            ++it;
        }
    }
}

uint32_t AllocateTextureSlot() {
    if (!g.free_texture_slots.empty()) {
        uint32_t slot = g.free_texture_slots.back();
        g.free_texture_slots.pop_back();
        return slot;
    }
    if (gl.textures.size() >= kMaxTextures) {
        return 0;
    }
    gl.textures.emplace_back();
    return static_cast<uint32_t>(gl.textures.size() - 1);
}

bool CheckSize(uint32_t width, uint32_t height) {
    GLint limit = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &limit);
    if (width == 0 || height == 0 || width > static_cast<uint32_t>(limit) || height > static_cast<uint32_t>(limit)) {
        Error("texture size %ux%u is outside what the device supports (1..%d)", width, height, limit);
        return false;
    }
    return true;
}

uint32_t ScaledSize(uint32_t logical, float scale) {
    return std::max(1u, static_cast<uint32_t>(std::ceil(static_cast<float>(logical) * scale)));
}

// What a render target's image measures now: the main target's for one sharing its depth, the main
// target's width and the main frame's scale in rows for a frame target filling the window, its
// logical size at the render scale otherwise.
void TargetPixelSize(const GlTexture &texture, uint32_t &width, uint32_t &height) {
    if (texture.shares_main_depth) {
        width = gl.width;
        height = gl.height;
    } else if (GlFrameShaped(texture)) {
        width = gl.width;
        height = ScaledSize(texture.logical_height, MainMapping(gl.width, gl.height).scale_y);
    } else {
        width = ScaledSize(texture.logical_width, g.render_scale);
        height = ScaledSize(texture.logical_height, g.render_scale);
    }
}

// The pixel rect a mapping puts a logical rect on, for blits.
void MappedOffsets(const LogicalMapping &mapping, float w, float h, int32_t offsets[4]) {
    auto x = [&](float value) {
        return std::clamp(static_cast<int32_t>(std::lround(value * mapping.scale_x + mapping.offset_x)), 0,
                          static_cast<int32_t>(mapping.pixel_width));
    };
    auto y = [&](float value) {
        return std::clamp(static_cast<int32_t>(std::lround(value * mapping.scale_y + mapping.offset_y)), 0,
                          static_cast<int32_t>(mapping.pixel_height));
    };
    offsets[0] = x(0.0f);
    offsets[1] = y(0.0f);
    offsets[2] = x(w);
    offsets[3] = y(h);
}

void UploadMesh(GlMesh &mesh, GLintptr offset, const void *data, GLsizeiptr size) {
    glBindBuffer(GL_COPY_WRITE_BUFFER, mesh.buffer);
    glBufferSubData(GL_COPY_WRITE_BUFFER, offset, size, data);
    glBindBuffer(GL_COPY_WRITE_BUFFER, 0);
}

} // namespace

GlImage CreateGlImage(uint32_t width, uint32_t height, uint32_t mips, GLenum format) {
    GlImage image;
    image.width = width;
    image.height = height;
    image.mips = mips;
    image.format = format;
    glGenTextures(1, &image.texture);
    glBindTexture(GL_TEXTURE_2D, image.texture);
    glTexStorage2D(GL_TEXTURE_2D, static_cast<GLsizei>(mips), format, static_cast<GLsizei>(width),
                   static_cast<GLsizei>(height));
    glBindTexture(GL_TEXTURE_2D, 0);
    gl.bound.textures[0] = gl.bound.textures[1] = UINT32_MAX;
    return image;
}

void DestroyGlImage(GlImage &image) {
    if (image.texture != 0) {
        ForgetFramebuffers(image.texture);
        glDeleteTextures(1, &image.texture);
        gl.bound.textures[0] = gl.bound.textures[1] = UINT32_MAX;
    }
    image = {};
}

GLuint FramebufferOf(GLuint color, GLuint depth) {
    uint64_t key = static_cast<uint64_t>(color) << 32 | depth;
    auto     it = gl.framebuffers.find(key);
    if (it != gl.framebuffers.end()) {
        return it->second;
    }
    GLuint framebuffer = 0;
    glGenFramebuffers(1, &framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color, 0);
    if (depth != 0) {
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, depth, 0);
    }
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        Fatal("GL: framebuffer incomplete (%#x)", status);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    gl.bound.framebuffer = UINT32_MAX;
    gl.framebuffers.emplace(key, framebuffer);
    return framebuffer;
}

void ForgetFramebuffers(GLuint texture) {
    for (auto it = gl.framebuffers.begin(); it != gl.framebuffers.end();) {
        if (static_cast<GLuint>(it->first >> 32) == texture || static_cast<GLuint>(it->first) == texture) {
            glDeleteFramebuffers(1, &it->second);
            it = gl.framebuffers.erase(it);
            gl.bound.framebuffer = UINT32_MAX;
        } else {
            ++it;
        }
    }
}

void ClearGlImage(const GlImage &image, const float rgba[4]) {
    bool depth = image.format == GL_DEPTH32F_STENCIL8;
    if (glClearTexImage != nullptr && !depth) {
        GLenum format = image.format == GL_R8 ? GL_RED : GL_RGBA;
        for (uint32_t mip = 0; mip < image.mips; mip++) {
            glClearTexImage(image.texture, static_cast<GLint>(mip), format, GL_FLOAT, rgba);
        }
        return;
    }
    // Through a framebuffer, with nothing masked or scissored.
    for (uint32_t mip = 0; mip < image.mips; mip++) {
        GLuint framebuffer = 0;
        glGenFramebuffers(1, &framebuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        glFramebufferTexture2D(GL_FRAMEBUFFER, depth ? GL_DEPTH_STENCIL_ATTACHMENT : GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                               image.texture, static_cast<GLint>(mip));
        if (depth) {
            GLenum none = GL_NONE;
            glDrawBuffers(1, &none);
        }
        glDisable(GL_SCISSOR_TEST);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glDepthMask(GL_TRUE);
        glStencilMaskSeparate(GL_FRONT_AND_BACK, 0xFF);
        if (depth) {
            glClearBufferfi(GL_DEPTH_STENCIL, 0, 0.0f, 0);
        } else {
            glClearBufferfv(GL_COLOR, 0, rgba);
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDeleteFramebuffers(1, &framebuffer);
    }
    ResetGlBound();
}

bool GlFrameShaped(const GlTexture &texture) { return texture.frame && FillsWindow(); }

LogicalMapping GlTextureMapping(const GlTexture &texture) {
    if (texture.shares_main_depth) {
        return MainMapping(texture.image.width, texture.image.height);
    }
    if (GlFrameShaped(texture)) {
        // The logical width lies over the main target's logical frame, whose sides it keeps.
        LogicalMapping main = MainMapping(gl.width, gl.height);
        float          rows = static_cast<float>(texture.image.height) / static_cast<float>(texture.logical_height);
        return LogicalMapping{main.scale_x * g.config.logical_width / static_cast<float>(texture.logical_width),
                              rows,
                              main.offset_x,
                              0.0f,
                              texture.image.width,
                              texture.image.height};
    }
    return LogicalMapping{static_cast<float>(texture.image.width) / static_cast<float>(texture.logical_width),
                          static_cast<float>(texture.image.height) / static_cast<float>(texture.logical_height),
                          0.0f,
                          0.0f,
                          texture.image.width,
                          texture.image.height};
}

void InitGlResources() {
    gl.textures.clear();
    gl.textures.resize(kFirstUserSlot);
    g.free_texture_slots.clear();
    gl.meshes.clear();
    gl.meshes.resize(1);
    g.free_mesh_slots.clear();
}

void ShutdownGlResources() {
    for (GlTexture &texture : gl.textures) {
        DestroyGlImage(texture.image);
        DestroyGlImage(texture.depth);
    }
    for (GlMesh &mesh : gl.meshes) {
        if (mesh.buffer != 0) {
            glDeleteBuffers(1, &mesh.buffer);
        }
    }
    gl.textures.clear();
    gl.meshes.clear();
}

GlTexture *LookupGlTexture(TextureHandle handle) {
    uint32_t slot = handle & kSlotMask;
    if (slot < kFirstUserSlot || slot >= gl.textures.size()) {
        return nullptr;
    }
    GlTexture &texture = gl.textures[slot];
    if (!texture.live || texture.generation != handle >> kSlotBits || (texture.doomed && !g.replaying)) {
        return nullptr;
    }
    return &texture;
}

GlMesh *LookupGlMesh(MeshHandle handle) {
    uint32_t slot = handle & kSlotMask;
    if (slot == 0 || slot >= gl.meshes.size()) {
        return nullptr;
    }
    GlMesh &mesh = gl.meshes[slot];
    if (!mesh.live || mesh.generation != handle >> kSlotBits || (mesh.doomed && !g.replaying)) {
        return nullptr;
    }
    return &mesh;
}

GlImage *ColorImageOfGl(TextureHandle handle) {
    if (handle == kMainTarget) {
        return &MainColorTargetGl();
    }
    if (handle == kPreviousFrame) {
        return &PreviousFrameImageGl();
    }
    GlTexture *texture = LookupGlTexture(handle);
    return texture ? &texture->image : nullptr;
}

void RecreateRenderTargetsGl() {
    for (uint32_t slot = kFirstUserSlot; slot < gl.textures.size(); slot++) {
        GlTexture &texture = gl.textures[slot];
        if (!texture.live || !texture.render_target || texture.shares_main_depth) {
            continue;
        }
        uint32_t width;
        uint32_t height;
        TargetPixelSize(texture, width, height);
        if (!CheckSize(width, height)) {
            width = texture.image.width;
            height = texture.image.height;
        }
        GlTexture resized = texture;
        resized.image.width = width;
        resized.image.height = height;
        LogicalMapping before = GlTextureMapping(texture);
        LogicalMapping after = GlTextureMapping(resized);
        bool           same_size = width == texture.image.width && height == texture.image.height;
        if (same_size && before.offset_x == after.offset_x && before.offset_y == after.offset_y) {
            continue;
        }
        GlImage old_image = texture.image;
        GlImage old_depth = texture.depth;
        texture.image = CreateGlImage(width, height, 1, GL_RGBA8);
        texture.depth = CreateGlImage(width, height, 1, GL_DEPTH32F_STENCIL8);
        // What lies outside the logical frame on either side is the frame's, not this image's.
        int32_t from[4];
        int32_t to[4];
        float   w = static_cast<float>(texture.logical_width);
        float   h = static_cast<float>(texture.logical_height);
        MappedOffsets(before, w, h, from);
        MappedOffsets(after, w, h, to);
        ClearGlImage(texture.image, kOpaqueBlack);
        ClearGlImage(texture.depth, kZero);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, FramebufferOf(old_image.texture, 0));
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, FramebufferOf(texture.image.texture, 0));
        glDisable(GL_SCISSOR_TEST);
        glBlitFramebuffer(from[0], from[1], from[2], from[3], to[0], to[1], to[2], to[3], GL_COLOR_BUFFER_BIT,
                          GL_LINEAR);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        ResetGlBound();
        DestroyGlImage(old_image);
        DestroyGlImage(old_depth);
    }
}

void RecreateSharedTargetsGl() {
    for (uint32_t slot = kFirstUserSlot; slot < gl.textures.size(); slot++) {
        GlTexture &texture = gl.textures[slot];
        if (!texture.live || !texture.shares_main_depth) {
            continue;
        }
        DestroyGlImage(texture.image);
        texture.image = CreateGlImage(gl.width, gl.height, 1, GL_RGBA8);
        ClearGlImage(texture.image, kOpaqueBlack);
    }
}

void DestroyDoomedTexture(TextureHandle handle) {
    uint32_t slot = handle & kSlotMask;
    if (slot < kFirstUserSlot || slot >= gl.textures.size()) {
        return;
    }
    GlTexture &texture = gl.textures[slot];
    if (!texture.live || texture.generation != handle >> kSlotBits) {
        return;
    }
    if (g.target == handle) {
        g.target = kMainTarget;
        gl.bound.framebuffer = UINT32_MAX;
    }
    DestroyGlImage(texture.image);
    DestroyGlImage(texture.depth);
    texture.contents = {};
    texture.live = false;
    texture.doomed = false;
    g.free_texture_slots.push_back(slot);
    ForgetExpansions(handle);
}

void DestroyDoomedMesh(MeshHandle handle) {
    uint32_t slot = handle & kSlotMask;
    if (slot == 0 || slot >= gl.meshes.size()) {
        return;
    }
    GlMesh &mesh = gl.meshes[slot];
    if (!mesh.live || mesh.generation != handle >> kSlotBits) {
        return;
    }
    glDeleteBuffers(1, &mesh.buffer);
    for (uint32_t layout = 0; layout < kLayoutCount; layout++) {
        if (gl.bound.vertex_buffer[layout] == mesh.buffer) {
            gl.bound.vertex_buffer[layout] = 0;
        }
        if (gl.bound.element_buffer[layout] == mesh.buffer) {
            gl.bound.element_buffer[layout] = 0;
        }
    }
    mesh.buffer = 0;
    mesh.live = false;
    mesh.doomed = false;
    g.free_mesh_slots.push_back(slot);
}

} // namespace gfx::detail

namespace gfx {

using namespace detail;

TextureHandle CreateTexture(const TextureDesc &desc) {
    if (!CheckSize(desc.width, desc.height)) {
        return kNullTexture;
    }
    uint32_t max_mips = std::bit_width(std::max(desc.width, desc.height));
    if (desc.mip_levels == 0 || desc.mip_levels > max_mips) {
        Error("texture %ux%u cannot have %u mip levels", desc.width, desc.height, desc.mip_levels);
        return kNullTexture;
    }
    uint32_t slot = AllocateTextureSlot();
    if (slot == 0) {
        Error("out of texture slots (%u)", kMaxTextures);
        return kNullTexture;
    }
    GlTexture &texture = gl.textures[slot];
    texture.live = true;
    texture.generation = NextGeneration(texture.generation);
    texture.desc = desc;
    texture.render_target = false;
    texture.shares_main_depth = false;
    texture.frame = false;
    texture.logical_width = desc.width;
    texture.logical_height = desc.height;
    texture.last_draw_use = 0;
    texture.contents.clear();
    texture.contents_version = 0;
    if (g.config.expand_palettes && desc.format == TextureFormat::Index8) {
        KeepContents(texture);
    }
    texture.image = CreateGlImage(desc.width, desc.height, desc.mip_levels, GlFormat(desc.format));
    ClearGlImage(texture.image, kZero);
    return MakeHandle(texture.generation, slot);
}

TextureHandle CreatePalette() {
    TextureHandle palette = CreateTexture(TextureDesc{256, 1, TextureFormat::Rgba8, 1, true});
    GlTexture    *texture = LookupGlTexture(palette);
    if (texture != nullptr && g.config.expand_palettes) {
        KeepContents(*texture);
    }
    return palette;
}

TextureHandle CreateRenderTarget(uint32_t logical_width, uint32_t logical_height, bool has_alpha, bool share_main_depth,
                                 bool frame_target) {
    if (share_main_depth) {
        logical_width = static_cast<uint32_t>(g.config.logical_width);
        logical_height = static_cast<uint32_t>(g.config.logical_height);
        frame_target = false;
    }
    GlTexture shape;
    shape.shares_main_depth = share_main_depth;
    shape.frame = frame_target;
    shape.logical_width = logical_width;
    shape.logical_height = logical_height;
    uint32_t width = 0;
    uint32_t height = 0;
    if (logical_width != 0 && logical_height != 0) {
        TargetPixelSize(shape, width, height);
    }
    if (logical_width == 0 || logical_height == 0 || !CheckSize(width, height)) {
        return kNullTexture;
    }
    uint32_t slot = AllocateTextureSlot();
    if (slot == 0) {
        Error("out of texture slots (%u)", kMaxTextures);
        return kNullTexture;
    }
    GlTexture &texture = gl.textures[slot];
    texture.live = true;
    texture.generation = NextGeneration(texture.generation);
    texture.desc = TextureDesc{logical_width, logical_height, TextureFormat::Rgba8, 1, has_alpha};
    texture.render_target = true;
    texture.shares_main_depth = share_main_depth;
    texture.frame = frame_target;
    texture.logical_width = logical_width;
    texture.logical_height = logical_height;
    texture.last_draw_use = 0;
    texture.contents.clear();
    texture.contents_version = 0;
    texture.image = CreateGlImage(width, height, 1, GL_RGBA8);
    ClearGlImage(texture.image, kOpaqueBlack);
    if (!share_main_depth) {
        texture.depth = CreateGlImage(width, height, 1, GL_DEPTH32F_STENCIL8);
        ClearGlImage(texture.depth, kZero);
    }
    return MakeHandle(texture.generation, slot);
}

TextureHandle NamedRenderTarget(std::string_view name, uint32_t logical_width, uint32_t logical_height, bool has_alpha,
                                bool share_main_depth, bool frame_target) {
    TextureHandle existing = FindNamedRenderTarget(name);
    if (existing != kNullTexture) {
        const GlTexture &texture = *LookupGlTexture(existing);
        bool             same_size = share_main_depth ||
                         (texture.logical_width == logical_width && texture.logical_height == logical_height);
        bool same_shape =
            texture.shares_main_depth == share_main_depth && (share_main_depth || texture.frame == frame_target);
        if (same_size && same_shape && texture.desc.has_alpha == has_alpha) {
            return existing;
        }
        DestroyTexture(existing);
    }
    TextureHandle created = CreateRenderTarget(logical_width, logical_height, has_alpha, share_main_depth, frame_target);
    if (created != kNullTexture) {
        g.named_targets.emplace(std::string(name), created);
    }
    return created;
}

TextureHandle FindNamedRenderTarget(std::string_view name) {
    auto it = g.named_targets.find(name);
    return it == g.named_targets.end() ? kNullTexture : it->second;
}

void DestroyTexture(TextureHandle handle) {
    GlTexture *texture = LookupGlTexture(handle);
    if (texture == nullptr) {
        return;
    }
    std::erase_if(g.named_targets, [handle](const auto &entry) { return entry.second == handle; });
    if (RecordingCalls()) {
        if (g.record_target == handle) {
            SetRenderTarget(kMainTarget);
        }
        texture->doomed = true;
        RecordDoomedTexture(handle);
        return;
    }
    if (g.target == handle) {
        SetRenderTarget(kMainTarget);
    }
    DestroyDoomedTexture(handle);
}

namespace {

bool UpdateTextureImpl(TextureHandle handle, uint32_t mip, uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                       const void *pixels, uint32_t row_length, bool record) {
    GlTexture *texture = LookupGlTexture(handle);
    if (texture == nullptr || pixels == nullptr) {
        Error("UpdateTexture: no texture %#x", handle);
        return false;
    }
    GlImage &image = texture->image;
    uint32_t mip_width = std::max(1u, image.width >> mip);
    uint32_t mip_height = std::max(1u, image.height >> mip);
    if (mip >= image.mips || w == 0 || h == 0 || x + w > mip_width || y + h > mip_height) {
        Error("UpdateTexture: %ux%u at %u,%u is outside mip %u of %ux%u", w, h, x, y, mip, mip_width, mip_height);
        return false;
    }
    uint32_t texel = TexelSize(texture->desc.format);
    uint32_t stride = (row_length ? row_length : w) * texel;
    uint32_t row = w * texel;
    if (record && RecordingCalls()) {
        UpdateTextureEntry entry{handle, mip, x, y, w, h, std::vector<uint8_t>(static_cast<size_t>(row) * h)};
        for (uint32_t i = 0; i < h; i++) {
            std::memcpy(entry.pixels.data() + static_cast<size_t>(i) * row,
                        static_cast<const uint8_t *>(pixels) + static_cast<size_t>(i) * stride, row);
        }
        RecordEntry(std::move(entry));
        return true;
    }

    if (!texture->contents.empty()) {
        uint8_t *contents = texture->contents[mip].data();
        for (uint32_t i = 0; i < h; i++) {
            std::memcpy(contents + (static_cast<size_t>(y + i) * mip_width + x) * texel,
                        static_cast<const uint8_t *>(pixels) + static_cast<size_t>(i) * stride, row);
        }
        texture->contents_version++;
    }

    glBindTexture(GL_TEXTURE_2D, image.texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, static_cast<GLint>(row_length));
    glTexSubImage2D(GL_TEXTURE_2D, static_cast<GLint>(mip), static_cast<GLint>(x), static_cast<GLint>(y),
                    static_cast<GLsizei>(w), static_cast<GLsizei>(h),
                    texture->desc.format == TextureFormat::Index8 ? GL_RED : GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    gl.bound.textures[0] = gl.bound.textures[1] = UINT32_MAX;
    return true;
}

// A pair that changes again this soon after its last expansion counts as churning; churning this
// often, it keeps the shader lookup for kChurnRest frames before it is tried again.
constexpr uint64_t kChurnFrames = 60;
constexpr uint32_t kChurnLimit = 3;
constexpr uint64_t kChurnRest = 600;
// Texels expanded in a frame beyond the first pair, so a scene's first frame spreads its expansions
// over the next few instead of stalling on them.
constexpr size_t kExpansionBudget = 1 << 20;

} // namespace

TextureHandle detail::PaletteExpansionGl(TextureHandle index_handle, TextureHandle palette_handle) {
    const GlTexture *index = LookupGlTexture(index_handle);
    const GlTexture *palette = LookupGlTexture(palette_handle);
    if (index == nullptr || palette == nullptr || index->contents.empty() || palette->contents.empty()) {
        return kNullTexture;
    }
    Expansion &expansion = g.expansions[static_cast<uint64_t>(index_handle) << 32 | palette_handle];
    GlTexture *rgba = LookupGlTexture(expansion.rgba);
    if (rgba != nullptr && expansion.index_version == index->contents_version &&
        expansion.palette_version == palette->contents_version) {
        return expansion.rgba;
    }
    // Drawn earlier this frame, the expansion holds what those draws need until the frame ends.
    if (g.frame_serial < expansion.lookup_until || (rgba != nullptr && rgba->last_draw_use == g.frame_serial)) {
        return kNullTexture;
    }
    if (rgba != nullptr) {
        expansion.churn = g.frame_serial - expansion.expanded < kChurnFrames ? expansion.churn + 1 : 0;
        if (expansion.churn >= kChurnLimit) {
            expansion.churn = 0;
            expansion.lookup_until = g.frame_serial + kChurnRest;
            return kNullTexture;
        }
    }
    if (g.expansion_frame != g.frame_serial) {
        g.expansion_frame = g.frame_serial;
        g.expansion_texels = 0;
    }
    size_t texels = static_cast<size_t>(index->desc.width) * index->desc.height;
    if (g.expansion_texels != 0 && g.expansion_texels + texels > kExpansionBudget) {
        return kNullTexture;
    }
    g.expansion_texels += texels;

    TextureHandle rgba_handle = expansion.rgba;
    if (rgba == nullptr) {
        TextureDesc desc = index->desc;
        desc.format = TextureFormat::Rgba8;
        desc.has_alpha = true;
        rgba_handle = CreateTexture(desc);
        if (rgba_handle == kNullTexture) {
            return kNullTexture;
        }
        // Creating it may have moved the textures and the expansions.
        index = LookupGlTexture(index_handle);
        palette = LookupGlTexture(palette_handle);
    }
    const auto           *colors = reinterpret_cast<const uint32_t *>(palette->contents[0].data());
    std::vector<uint32_t> texels_rgba;
    for (uint32_t mip = 0; mip < index->contents.size(); mip++) {
        const std::vector<uint8_t> &indices = index->contents[mip];
        texels_rgba.resize(indices.size());
        for (size_t i = 0; i < indices.size(); i++) {
            texels_rgba[i] = colors[indices[i]];
        }
        UpdateTextureImpl(rgba_handle, mip, 0, 0, std::max(1u, index->desc.width >> mip),
                          std::max(1u, index->desc.height >> mip), texels_rgba.data(), 0, false);
    }
    Expansion &made = g.expansions[static_cast<uint64_t>(index_handle) << 32 | palette_handle];
    made.rgba = rgba_handle;
    made.index_version = index->contents_version;
    made.palette_version = palette->contents_version;
    made.expanded = g.frame_serial;
    return rgba_handle;
}

void detail::ForgetContentsGl(GlTexture *texture) {
    if (texture != nullptr && !texture->contents.empty()) {
        texture->contents = {};
        texture->contents_version++;
    }
}

bool UpdateTexture(TextureHandle handle, uint32_t mip, uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                   const void *pixels, uint32_t row_length) {
    return UpdateTextureImpl(handle, mip, x, y, w, h, pixels, row_length, true);
}

bool UpdateTextureNow(TextureHandle handle, uint32_t mip, uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                      const void *pixels, uint32_t row_length) {
    return UpdateTextureImpl(handle, mip, x, y, w, h, pixels, row_length, false);
}

bool UpdatePalette(TextureHandle palette, const uint32_t *rgba, uint32_t first, uint32_t count) {
    if (first + count > 256) {
        Error("UpdatePalette: entries %u..%u are past 256", first, first + count);
        return false;
    }
    return UpdateTexture(palette, 0, first, 0, count, 1, rgba);
}

std::optional<TextureInfo> GetTextureInfo(TextureHandle handle) {
    if (handle == kMainTarget || handle == kPreviousFrame) {
        const GlImage &image = *ColorImageOfGl(handle);
        return TextureInfo{static_cast<uint32_t>(g.config.logical_width),
                           static_cast<uint32_t>(g.config.logical_height),
                           image.width,
                           image.height,
                           TextureFormat::Rgba8,
                           1,
                           true,
                           true,
                           false,
                           false};
    }
    GlTexture *texture = LookupGlTexture(handle);
    if (texture == nullptr) {
        return std::nullopt;
    }
    return TextureInfo{texture->logical_width,   texture->logical_height,   texture->image.width,
                       texture->image.height,    texture->desc.format,      texture->desc.mip_levels,
                       texture->desc.has_alpha,  texture->render_target,    texture->shares_main_depth,
                       texture->frame};
}

void ConvertPs2Alpha(uint32_t *rgba, size_t count) {
    uint8_t *bytes = reinterpret_cast<uint8_t *>(rgba);
    for (size_t i = 0; i < count; i++) {
        uint8_t &alpha = bytes[i * 4 + 3];
        alpha = static_cast<uint8_t>(std::min(alpha * 2, 255));
    }
}

MeshHandle CreateMesh(std::span<const Vertex3D> vertices, std::span<const uint32_t> indices) {
    if (vertices.empty() || indices.empty()) {
        Error("CreateMesh: empty mesh");
        return kNullMesh;
    }
    uint32_t slot;
    if (!g.free_mesh_slots.empty()) {
        slot = g.free_mesh_slots.back();
        g.free_mesh_slots.pop_back();
    } else if (gl.meshes.size() < kMaxMeshes) {
        gl.meshes.emplace_back();
        slot = static_cast<uint32_t>(gl.meshes.size() - 1);
    } else {
        Error("out of mesh slots (%u)", kMaxMeshes);
        return kNullMesh;
    }
    GlMesh    &mesh = gl.meshes[slot];
    GLsizeiptr vertex_bytes = static_cast<GLsizeiptr>(vertices.size_bytes());
    GLsizeiptr index_bytes = static_cast<GLsizeiptr>(indices.size_bytes());
    mesh.live = true;
    mesh.generation = NextGeneration(mesh.generation);
    mesh.vertex_count = static_cast<uint32_t>(vertices.size());
    mesh.index_count = static_cast<uint32_t>(indices.size());
    glGenBuffers(1, &mesh.buffer);
    glBindBuffer(GL_COPY_WRITE_BUFFER, mesh.buffer);
    glBufferData(GL_COPY_WRITE_BUFFER, vertex_bytes + index_bytes, nullptr, GL_STATIC_DRAW);
    glBufferSubData(GL_COPY_WRITE_BUFFER, 0, vertex_bytes, vertices.data());
    glBufferSubData(GL_COPY_WRITE_BUFFER, vertex_bytes, index_bytes, indices.data());
    glBindBuffer(GL_COPY_WRITE_BUFFER, 0);
    return MakeHandle(mesh.generation, slot);
}

bool UpdateMeshVertices(MeshHandle handle, uint32_t first, std::span<const Vertex3D> vertices) {
    GlMesh *mesh = LookupGlMesh(handle);
    if (mesh == nullptr || first + vertices.size() > mesh->vertex_count) {
        Error("UpdateMeshVertices: bad mesh %#x or range", handle);
        return false;
    }
    if (vertices.empty()) {
        return true;
    }
    if (RecordingCalls()) {
        RecordEntry(UpdateMeshEntry{handle, first, std::vector<Vertex3D>(vertices.begin(), vertices.end())});
        return true;
    }
    UploadMesh(*mesh, static_cast<GLintptr>(first * sizeof(Vertex3D)), vertices.data(),
               static_cast<GLsizeiptr>(vertices.size_bytes()));
    return true;
}

void DestroyMesh(MeshHandle handle) {
    GlMesh *mesh = LookupGlMesh(handle);
    if (mesh == nullptr) {
        return;
    }
    if (RecordingCalls()) {
        mesh->doomed = true;
        RecordDoomedMesh(handle);
        return;
    }
    DestroyDoomedMesh(handle);
}

} // namespace gfx
