// The Switch's GL renderer (DC_SWITCH_RENDERER=gl), kept in reserve should NVK fail on a console. For
// now it is a null backend: every gfx.hpp call is answered and handles are real, but nothing is
// drawn and frames present nothing, so a GL build runs the game without its picture. That already
// tells input, audio, files and the game's own logic apart from the renderer when NVK will not
// start. Drawing goes in here over the same display lists (displaylist.cpp) and TIM2 decoding
// (tim2.cpp) the Vulkan renderer uses, which this build keeps.
//
// displaylist.cpp reads the renderer's state through context.hpp's Context, so this backend keeps
// one too; its Vulkan handles stay empty, except that a renderer counts as up when g.device is set.

#include "context.hpp"
#include "displaylist.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <string>
#include <unordered_map>

namespace gfx {

namespace detail {

Context g;

void Error(const char *format, ...) {
    va_list arguments;
    va_start(arguments, format);
    std::fputs("gfx: ", stderr);
    std::vfprintf(stderr, format, arguments);
    std::fputc('\n', stderr);
    va_end(arguments);
}

bool OpenListFrame(bool, bool, bool) { return true; }

void PrepareRecording() {}

void CloseListFrame(bool, bool) {}

void DestroyDoomedTexture(TextureHandle) {}

void DestroyDoomedMesh(MeshHandle) {}

void Draw2DSided(Primitive, std::span<const Vertex2D>, const TextureBinding &, const DrawState &, UiSide) {}

} // namespace detail

using detail::g;

namespace {

struct State {
    std::unordered_map<TextureHandle, TextureInfo>   textures;
    std::unordered_map<std::string, TextureHandle> named;
    TextureHandle                                  next_texture = kPreviousFrame + 1;
    MeshHandle                                     next_mesh = kNullMesh + 1;
    TextureHandle                                  target = kMainTarget;
    FrameLayout                                    layout;
    UiAnchor                                       anchor;
} s;

void WindowPixels(uint32_t &width, uint32_t &height) {
    int w = 0;
    int h = 0;
    if (g.window != nullptr) {
        SDL_GetWindowSizeInPixels(g.window, &w, &h);
    }
    width = static_cast<uint32_t>(std::max(w, 1));
    height = static_cast<uint32_t>(std::max(h, 1));
}

TextureHandle AddTexture(const TextureInfo &info) {
    TextureHandle handle = s.next_texture++;
    s.textures[handle] = info;
    return handle;
}

TextureInfo TargetInfo(uint32_t width, uint32_t height, bool has_alpha, bool share_main_depth, bool frame_target) {
    return {width, height, width, height, TextureFormat::Rgba8, 1, has_alpha, true, share_main_depth, frame_target};
}

} // namespace

void RendererInit(SDL_Window *window, const RendererConfig &config) {
    g.window = window;
    g.config = config;
    s.layout = config.layout;
    // displaylist.cpp renders a list only while a device is up.
    g.device = reinterpret_cast<VkDevice>(std::uintptr_t{1});
    g.renderer_instance++;
    std::fprintf(stderr, "gfx: the GL renderer draws nothing yet (DC_SWITCH_RENDERER=gl)\n");
}

RendererFeatures ActiveRendererFeatures() { return {0, g.config.offscreen, true, true, true, false}; }

bool HeadlessSurfaceAvailable() { return false; }

void RendererShutdown() {
    g.device = VK_NULL_HANDLE;
    s = {};
}

void RendererResize() {}

void SetPresentMode(PresentMode mode) { g.config.present_mode = mode; }

void SetAnisotropy(int) {}

bool BeginFrame() {
    g.in_frame = true;
    s.target = kMainTarget;
    return true;
}

void EndFrame() { g.in_frame = false; }

bool InFrame() { return g.in_frame; }

uint32_t PipelineCount() { return 0; }

void SetFrameLayout(const FrameLayout &layout) { s.layout = layout; }

FrameLayout CurrentFrameLayout() { return s.layout; }

TextureHandle CreateTexture(const TextureDesc &desc) {
    return AddTexture({desc.width, desc.height, desc.width, desc.height, desc.format, desc.mip_levels, desc.has_alpha,
                       false, false, false});
}

TextureHandle CreatePalette() { return AddTexture({256, 1, 256, 1, TextureFormat::Rgba8, 1, true, false, false, false}); }

TextureHandle CreateRenderTarget(uint32_t logical_width, uint32_t logical_height, bool has_alpha, bool share_main_depth,
                                 bool frame_target) {
    return AddTexture(TargetInfo(logical_width, logical_height, has_alpha, share_main_depth, frame_target));
}

TextureHandle NamedRenderTarget(std::string_view name, uint32_t logical_width, uint32_t logical_height, bool has_alpha,
                                bool share_main_depth, bool frame_target) {
    auto found = s.named.find(std::string(name));
    if (found != s.named.end()) {
        return found->second;
    }
    TextureHandle handle = CreateRenderTarget(logical_width, logical_height, has_alpha, share_main_depth, frame_target);
    s.named.emplace(std::string(name), handle);
    return handle;
}

TextureHandle FindNamedRenderTarget(std::string_view name) {
    auto found = s.named.find(std::string(name));
    return found == s.named.end() ? kNullTexture : found->second;
}

void DestroyTexture(TextureHandle texture) {
    s.textures.erase(texture);
    std::erase_if(s.named, [texture](const auto &entry) { return entry.second == texture; });
}

bool UpdateTexture(TextureHandle texture, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, const void *, uint32_t) {
    return s.textures.contains(texture);
}

bool UpdateTextureNow(TextureHandle texture, uint32_t mip, uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                      const void *pixels, uint32_t row_length) {
    return UpdateTexture(texture, mip, x, y, w, h, pixels, row_length);
}

bool UpdatePalette(TextureHandle palette, const uint32_t *, uint32_t, uint32_t) { return s.textures.contains(palette); }

std::optional<TextureInfo> GetTextureInfo(TextureHandle texture) {
    if (texture == kMainTarget || texture == kPreviousFrame) {
        uint32_t width;
        uint32_t height;
        WindowPixels(width, height);
        auto logical_w = static_cast<uint32_t>(g.config.logical_width);
        auto logical_h = static_cast<uint32_t>(g.config.logical_height);
        return TextureInfo{logical_w, logical_h, width, height, TextureFormat::Rgba8, 1, true, true, false, true};
    }
    auto found = s.textures.find(texture);
    return found == s.textures.end() ? std::nullopt : std::optional(found->second);
}

void ConvertPs2Alpha(uint32_t *rgba, size_t count) {
    auto *bytes = reinterpret_cast<uint8_t *>(rgba);
    for (size_t i = 0; i < count; i++) {
        bytes[i * 4 + 3] = static_cast<uint8_t>(std::min(bytes[i * 4 + 3] * 2, 255));
    }
}

MeshHandle CreateMesh(std::span<const Vertex3D>, std::span<const uint32_t>) { return s.next_mesh++; }

bool UpdateMeshVertices(MeshHandle mesh, uint32_t, std::span<const Vertex3D>) { return mesh != kNullMesh; }

void DestroyMesh(MeshHandle) {}

float LogicalWidth() { return g.config.logical_width; }

float LogicalHeight() { return g.config.logical_height; }

LogicalRect LogicalFrame() { return {0.0f, 0.0f, g.config.logical_width, g.config.logical_height}; }

void Draw2D(Primitive, std::span<const Vertex2D>, const TextureBinding &, const DrawState &) {}

void DrawMesh(MeshHandle, uint32_t, uint32_t, const MeshConstants &, const TextureBinding &, const DrawState &,
              const MeshTransform *) {}

void DrawMeshImmediate(std::span<const Vertex3D>, std::span<const uint32_t>, const MeshConstants &,
                       const TextureBinding &, const DrawState &, const MeshTransform *) {}

void DrawMeshProg0(MeshHandle, uint32_t, uint32_t, const MeshConstants &, const Prog0Constants &,
                   const TextureBinding &, const DrawState &, const MeshTransform *) {}

void DrawMeshImmediateProg0(std::span<const Vertex3D>, std::span<const uint32_t>, const MeshConstants &,
                            const Prog0Constants &, const TextureBinding &, const DrawState &, const MeshTransform *) {}

void SetRenderTarget(TextureHandle target) { s.target = target; }

TextureHandle CurrentRenderTarget() { return s.target; }

// The main target letterboxes the logical frame at the display aspect, centred on whole pixels;
// a render target maps it onto its own pixels.
LogicalMapping GetLogicalMapping(TextureHandle target) {
    std::optional<TextureInfo> info = GetTextureInfo(target);
    if (!info) {
        return {1.0f, 1.0f, 0.0f, 0.0f, 1, 1};
    }
    if (target != kMainTarget && target != kPreviousFrame) {
        return {static_cast<float>(info->pixel_width) / g.config.logical_width,
                static_cast<float>(info->pixel_height) / g.config.logical_height, 0.0f, 0.0f, info->pixel_width,
                info->pixel_height};
    }
    float width = static_cast<float>(info->pixel_width);
    float height = static_cast<float>(info->pixel_height);
    float frame_w = std::min(width, height * g.config.display_aspect);
    float frame_h = frame_w / g.config.display_aspect;
    return {frame_w / g.config.logical_width, frame_h / g.config.logical_height,
            static_cast<float>(static_cast<int>((width - frame_w) / 2)),
            static_cast<float>(static_cast<int>((height - frame_h) / 2)), info->pixel_width, info->pixel_height};
}

LogicalMapping GetUiMapping(TextureHandle target) { return GetLogicalMapping(target); }

void SetUiAnchor(const UiAnchor &anchor) { s.anchor = anchor; }

UiAnchor CurrentUiAnchor() { return s.anchor; }

LogicalRect VisibleLogicalRect(TextureHandle target) {
    LogicalMapping mapping = GetLogicalMapping(target);
    return {-mapping.offset_x / mapping.scale_x, -mapping.offset_y / mapping.scale_y,
            static_cast<float>(mapping.pixel_width) / mapping.scale_x,
            static_cast<float>(mapping.pixel_height) / mapping.scale_y};
}

void Clear(bool, const uint8_t[4], bool, float, const LogicalRect *) {}

void ClearStencil(uint8_t, const LogicalRect *) {}

bool CopyTexture(TextureHandle, Rect, TextureHandle, int32_t, int32_t) { return true; }

bool BlitTexture(TextureHandle, Rect, TextureHandle, Rect, Filter) { return true; }

bool SnapshotFrame(TextureHandle) { return true; }

void ReadDepth(uint32_t, float, float, float, float) {}

std::optional<float> DepthResult(uint32_t) { return std::nullopt; }

bool ReadbackFrame(std::vector<uint8_t> &, uint32_t &, uint32_t &) { return false; }

bool ReadbackTexture(TextureHandle, std::vector<uint8_t> &, uint32_t &, uint32_t &) { return false; }

bool WritePng(const std::filesystem::path &, const uint8_t *, uint32_t, uint32_t) { return false; }

bool PresentCanonical() { return true; }

} // namespace gfx
