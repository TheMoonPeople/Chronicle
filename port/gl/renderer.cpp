// The GL renderer's lifecycle and frames: the context, the main targets, the canonical and display
// renders of a display list, the present, depth queries and readback (Kazarov's renderer.cpp and
// readback.cpp, gfx/README.md). GL orders every call as it is made, so a frame is the calls between
// its open and its close; the streaming buffer's fences (stream.cpp) keep the CPU at most
// kStreamSegments frames ahead of the GPU.

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>

#include "gl.hpp"

namespace gfx {

namespace detail {

Context g;
State   gl;

void Fatal(const char *format, ...) {
    std::va_list args;
    va_start(args, format);
    std::fputs("gfx: ", stderr);
    std::vfprintf(stderr, format, args);
    std::fputc('\n', stderr);
    va_end(args);
    std::fflush(stderr);
    std::exit(1);
}

void Error(const char *format, ...) {
    if (g.quiet) {
        return;
    }
    std::va_list args;
    va_start(args, format);
    std::fputs("gfx: ", stderr);
    std::vfprintf(stderr, format, args);
    std::fputc('\n', stderr);
    va_end(args);
}

uint32_t SamplerIndex(Filter filter, Wrap wrap_u, Wrap wrap_v) {
    return static_cast<uint32_t>(filter) * 4 + static_cast<uint32_t>(wrap_u) * 2 + static_cast<uint32_t>(wrap_v);
}

namespace {

// A query reads at most this many pixels on a side, as in the Vulkan renderer.
constexpr uint32_t   kMaxQuerySide = 64;
constexpr GLsizeiptr kQueryBytes = kMaxQuerySide * kMaxQuerySide * sizeof(float);

constexpr float kOpaqueBlack[4] = {0.0f, 0.0f, 0.0f, 1.0f};
constexpr float kZero[4] = {0.0f, 0.0f, 0.0f, 0.0f};

void APIENTRY DebugCallback(GLenum, GLenum type, GLuint, GLenum severity, GLsizei, const GLchar *message,
                              const void *) {
    if (severity == GL_DEBUG_SEVERITY_NOTIFICATION) {
        return;
    }
    std::fprintf(stderr, "GL debug: %s\n", message);
    if (type == GL_DEBUG_TYPE_ERROR || type == GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR ||
        type == GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR || type == GL_DEBUG_TYPE_PERFORMANCE) {
        g.validation_messages++;
    }
}

void WindowPixels(uint32_t &width, uint32_t &height) {
    int w = 0;
    int h = 0;
    SDL_GetWindowSizeInPixels(g.window, &w, &h);
    width = static_cast<uint32_t>(std::max(w, 0));
    height = static_cast<uint32_t>(std::max(h, 0));
}

void CreateSamplers() {
    glGenSamplers(kSamplerCount, gl.samplers);
    for (uint32_t i = 0; i < kSamplerCount; i++) {
        bool   anisotropic = i >= kBaseSamplerCount;
        bool   linear = anisotropic || (i & 4);
        GLuint sampler = gl.samplers[i];
        glSamplerParameteri(sampler, GL_TEXTURE_MAG_FILTER, linear ? GL_LINEAR : GL_NEAREST);
        glSamplerParameteri(sampler, GL_TEXTURE_MIN_FILTER, linear ? GL_LINEAR_MIPMAP_LINEAR : GL_NEAREST_MIPMAP_NEAREST);
        glSamplerParameteri(sampler, GL_TEXTURE_WRAP_S, (i & 2) ? GL_REPEAT : GL_CLAMP_TO_EDGE);
        glSamplerParameteri(sampler, GL_TEXTURE_WRAP_T, (i & 1) ? GL_REPEAT : GL_CLAMP_TO_EDGE);
        if (anisotropic && gl.max_anisotropy > 1.0f) {
            float samples = static_cast<float>(2u << ((i - kBaseSamplerCount) / 4));
            glSamplerParameterf(sampler, GL_TEXTURE_MAX_ANISOTROPY, std::min(samples, gl.max_anisotropy));
        }
    }
    glGenSamplers(1, &gl.palette_sampler);
    glSamplerParameteri(gl.palette_sampler, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glSamplerParameteri(gl.palette_sampler, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glSamplerParameteri(gl.palette_sampler, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glSamplerParameteri(gl.palette_sampler, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

// The vertex layouts of Kazarov's pipelines.cpp. mesh_prog0.vert always reads the skin at locations
// 4 and 5; unskinned, they alias bytes of the vertex itself, which the shader then never uses.
void CreateVertexArrays() {
    glGenVertexArrays(kLayoutCount, gl.vertex_arrays);
    auto attribute = [](GLuint location, GLint size, GLenum type, bool normalized, size_t offset) {
        glEnableVertexAttribArray(location);
        glVertexAttribFormat(location, size, type, normalized ? GL_TRUE : GL_FALSE, static_cast<GLuint>(offset));
        glVertexAttribBinding(location, 0);
    };
    auto integer = [](GLuint location, GLint size, GLenum type, size_t offset) {
        glEnableVertexAttribArray(location);
        glVertexAttribIFormat(location, size, type, static_cast<GLuint>(offset));
        glVertexAttribBinding(location, 0);
    };
    glBindVertexArray(gl.vertex_arrays[kLayout2D]);
    attribute(0, 3, GL_FLOAT, false, offsetof(Vertex2D, x));
    attribute(1, 2, GL_FLOAT, false, offsetof(Vertex2D, u));
    attribute(2, 4, GL_UNSIGNED_BYTE, true, offsetof(Vertex2D, color));
    attribute(3, 1, GL_UNSIGNED_BYTE, true, offsetof(Vertex2D, fog));
    for (VertexLayout layout : {kLayoutMesh, kLayoutProg0, kLayoutSkinned}) {
        glBindVertexArray(gl.vertex_arrays[layout]);
        attribute(0, 3, GL_FLOAT, false, offsetof(Vertex3D, position));
        attribute(1, 3, GL_FLOAT, false, offsetof(Vertex3D, normal));
        attribute(2, 2, GL_FLOAT, false, offsetof(Vertex3D, uv));
        attribute(3, 4, GL_UNSIGNED_BYTE, true, offsetof(Vertex3D, color));
    }
    glBindVertexArray(gl.vertex_arrays[kLayoutProg0]);
    attribute(4, 4, GL_FLOAT, false, offsetof(Vertex3D, position));
    integer(5, 4, GL_UNSIGNED_BYTE, offsetof(Vertex3D, color));
    glBindVertexArray(gl.vertex_arrays[kLayoutSkinned]);
    attribute(4, 4, GL_FLOAT, false, sizeof(Vertex3D) + offsetof(Prog0Skin, weight));
    integer(5, 4, GL_UNSIGNED_BYTE, sizeof(Vertex3D) + offsetof(Prog0Skin, bone));
    glBindVertexArray(0);
}

void CreateMainTargetsGl(uint32_t width, uint32_t height) {
    gl.width = width;
    gl.height = height;
    for (GlImage &image : gl.main_color) {
        image = CreateGlImage(width, height, 1, GL_RGBA8);
        ClearGlImage(image, kOpaqueBlack);
    }
    gl.display_color = CreateGlImage(width, height, 1, GL_RGBA8);
    ClearGlImage(gl.display_color, kOpaqueBlack);
    for (GlImage *depth : {&gl.main_depth, &gl.display_depth}) {
        *depth = CreateGlImage(width, height, 1, GL_DEPTH32F_STENCIL8);
        ClearGlImage(*depth, kZero);
    }
}

void DestroyMainTargetsGl() {
    for (GlImage &image : gl.main_color) {
        DestroyGlImage(image);
    }
    DestroyGlImage(gl.main_depth);
    DestroyGlImage(gl.display_color);
    DestroyGlImage(gl.display_depth);
}

// The main targets follow the window's pixel size, losing their contents. False when the window has
// none (minimised); the targets then keep theirs.
bool SyncMainTargetsGl() {
    uint32_t width;
    uint32_t height;
    WindowPixels(width, height);
    g.resize_pending = false;
    if (width == 0 || height == 0) {
        return false;
    }
    if (width != gl.width || height != gl.height) {
        DestroyMainTargetsGl();
        CreateMainTargetsGl(width, height);
        RecreateSharedTargetsGl();
        RecreateRenderTargetsGl();
    }
    return true;
}

void ApplySwapInterval() {
    // Mailbox has no GL equivalent; it is FIFO, as where a Vulkan surface lacks it.
    bool vsync = g.config.present_mode != PresentMode::Immediate;
    if (!SDL_GL_SetSwapInterval(vsync ? 1 : 0)) {
        Error("SDL_GL_SetSwapInterval: %s", SDL_GetError());
    }
    gl.vsync = vsync;
}

bool OpenFrame() {
    g.in_frame = true;
    g.frame_serial++;
    g.target = kMainTarget;
    g.rendering = false;
    ResetDrawState();
    return true;
}

// Rows top to bottom from an image's framebuffer.
bool ReadImage(const GlImage &image, std::vector<uint8_t> &pixels) {
    bool   index = image.format == GL_R8;
    size_t texel = index ? 1 : 4;
    pixels.resize(static_cast<size_t>(image.width) * image.height * texel);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, FramebufferOf(image.texture, 0));
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, static_cast<GLsizei>(image.width), static_cast<GLsizei>(image.height), index ? GL_RED : GL_RGBA,
                 GL_UNSIGNED_BYTE, pixels.data());
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    gl.bound.framebuffer = UINT32_MAX;
    return true;
}

// The window shows source, turned the right way up.
void Present(const GlImage &source) {
    uint32_t width;
    uint32_t height;
    WindowPixels(width, height);
    if (width == 0 || height == 0) {
        return;
    }
    if ((g.config.present_mode != PresentMode::Immediate) != gl.vsync) {
        ApplySwapInterval();
    }
    glBindFramebuffer(GL_READ_FRAMEBUFFER, FramebufferOf(source.texture, 0));
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glDisable(GL_SCISSOR_TEST);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glBlitFramebuffer(0, 0, static_cast<GLint>(source.width), static_cast<GLint>(source.height), 0,
                      static_cast<GLint>(height), static_cast<GLint>(width), 0, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    gl.bound.framebuffer = UINT32_MAX;
    gl.bound.color_write_mask = -1;
    glEnable(GL_SCISSOR_TEST);
    SDL_GL_SwapWindow(g.window);
}

void HarvestDepthQueriesGl() {
    if (gl.depth_fence == nullptr) {
        return;
    }
    glClientWaitSync(gl.depth_fence, GL_SYNC_FLUSH_COMMANDS_BIT, GL_TIMEOUT_IGNORED);
    glDeleteSync(gl.depth_fence);
    gl.depth_fence = nullptr;
    glBindBuffer(GL_PIXEL_PACK_BUFFER, gl.depth_pack);
    const auto *data = static_cast<const uint8_t *>(
        glMapBufferRange(GL_PIXEL_PACK_BUFFER, 0, kDepthQueryCount * kQueryBytes, GL_MAP_READ_BIT));
    for (DepthQuery &query : g.depth_queries) {
        if (!query.recorded) {
            continue;
        }
        float farthest = 1.0f;
        if (data != nullptr) {
            for (uint32_t i = 0; i < query.texels; i++) {
                float value;
                std::memcpy(&value, data + query.offset + i * sizeof(float), sizeof(float));
                farthest = std::min(farthest, value);
            }
        }
        query.result = farthest;
        query.valid = data != nullptr;
        query.recorded = false;
    }
    if (data != nullptr) {
        glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
    }
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
    g.depth_queries_recorded = false;
}

// The queued queries read the main depth buffer into the pack buffer.
void RecordDepthQueriesGl() {
    GlImage       &depth = gl.main_depth;
    LogicalMapping mapping = MainMapping(depth.width, depth.height);
    bool           any = false;
    for (DepthQuery &query : g.depth_queries) {
        if (!query.queued) {
            continue;
        }
        query.queued = false;
        query.recorded = false;
        query.valid = false;
        float x0 = std::floor(query.rect.x * mapping.scale_x + mapping.offset_x);
        float y0 = std::floor(query.rect.y * mapping.scale_y + mapping.offset_y);
        float x1 = std::ceil((query.rect.x + query.rect.w) * mapping.scale_x + mapping.offset_x);
        float y1 = std::ceil((query.rect.y + query.rect.h) * mapping.scale_y + mapping.offset_y);
        x0 = std::max(x0, 0.0f);
        y0 = std::max(y0, 0.0f);
        x1 = std::min(x1, static_cast<float>(depth.width));
        y1 = std::min(y1, static_cast<float>(depth.height));
        if (x1 <= x0 || y1 <= y0) {
            continue;
        }
        uint32_t width = std::min(static_cast<uint32_t>(x1 - x0), kMaxQuerySide);
        uint32_t height = std::min(static_cast<uint32_t>(y1 - y0), kMaxQuerySide);
        int32_t  x = static_cast<int32_t>((x0 + x1) * 0.5f) - static_cast<int32_t>(width / 2);
        int32_t  y = static_cast<int32_t>((y0 + y1) * 0.5f) - static_cast<int32_t>(height / 2);
        x = std::clamp(x, 0, static_cast<int32_t>(depth.width - width));
        y = std::clamp(y, 0, static_cast<int32_t>(depth.height - height));
        if (!any) {
            if (gl.depth_pack == 0) {
                glGenBuffers(1, &gl.depth_pack);
                glBindBuffer(GL_PIXEL_PACK_BUFFER, gl.depth_pack);
                glBufferData(GL_PIXEL_PACK_BUFFER, kDepthQueryCount * kQueryBytes, nullptr, GL_STREAM_READ);
            }
            glBindBuffer(GL_PIXEL_PACK_BUFFER, gl.depth_pack);
            glBindFramebuffer(GL_READ_FRAMEBUFFER, FramebufferOf(gl.main_color[g.main_current].texture, depth.texture));
            glPixelStorei(GL_PACK_ALIGNMENT, 4);
            any = true;
        }
        query.offset = static_cast<VkDeviceSize>(&query - g.depth_queries.data()) * kQueryBytes;
        glReadPixels(x, y, static_cast<GLsizei>(width), static_cast<GLsizei>(height), GL_DEPTH_COMPONENT, GL_FLOAT,
                     reinterpret_cast<void *>(static_cast<uintptr_t>(query.offset)));
        query.texels = width * height;
        query.recorded = true;
    }
    if (any) {
        glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
        gl.bound.framebuffer = UINT32_MAX;
        gl.depth_fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
        g.depth_queries_recorded = true;
    }
}

// The answers are read as the frame closes, as the Vulkan renderer reads them, except on the Switch,
// whose GPU is a frame behind: there a frame's answers are taken as the next frame closes, so the
// CPU never waits for the GPU to finish the frame it has just been given.
void CloseDepthQueries() {
#ifdef __SWITCH__
    bool harvest_after = true;
#else
    bool harvest_after = false;
#endif
    if (harvest_after) {
        HarvestDepthQueriesGl();
        RecordDepthQueriesGl();
    } else {
        RecordDepthQueriesGl();
        HarvestDepthQueriesGl();
    }
}

// A frame that drew the game's state ends: kPreviousFrame becomes it.
void FinishFrame() {
    g.main_current ^= 1;
    StreamEndFrame();
}

} // namespace

GlImage &CurrentMainColorGl() { return gl.main_color[g.main_current]; }

GlImage &PreviousMainColorGl() { return gl.main_color[g.main_current ^ 1]; }

GlImage &MainColorTargetGl() { return g.display_pass ? gl.display_color : CurrentMainColorGl(); }

GlImage &MainDepthGl() { return g.display_pass ? gl.display_depth : gl.main_depth; }

// A display render of the newest list samples what its canonical render sampled, which the flip at
// the end of that render left as the current image.
GlImage &PreviousFrameImageGl() { return g.display_pass ? CurrentMainColorGl() : PreviousMainColorGl(); }

void PrepareRecording() { SyncMainTargetsGl(); }

bool OpenListFrame(bool canonical, bool present, bool base) {
    if (canonical) {
        // The game's state is drawn whether or not the window can show it: later ticks sample it.
        SyncMainTargetsGl();
        return OpenFrame();
    }
    if (present && !g.offscreen) {
        uint32_t width;
        uint32_t height;
        WindowPixels(width, height);
        if (width == 0 || height == 0) {
            return false;
        }
    }
    OpenFrame();
    g.display_pass = true;
    if (base) {
        const GlImage &source = PreviousMainColorGl();
        glBindFramebuffer(GL_READ_FRAMEBUFFER, FramebufferOf(source.texture, 0));
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, FramebufferOf(gl.display_color.texture, 0));
        glDisable(GL_SCISSOR_TEST);
        glBlitFramebuffer(0, 0, static_cast<GLint>(source.width), static_cast<GLint>(source.height), 0, 0,
                          static_cast<GLint>(gl.display_color.width), static_cast<GLint>(gl.display_color.height),
                          GL_COLOR_BUFFER_BIT, GL_NEAREST);
        glEnable(GL_SCISSOR_TEST);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        gl.bound.framebuffer = UINT32_MAX;
    }
    return true;
}

void CloseListFrame(bool canonical, bool present) {
    EndRendering();
    g.in_frame = false;
    if (canonical) {
        CloseDepthQueries();
        FinishFrame();
        g.canonical_newest = true;
        g.presented_display = false;
        return;
    }
    if (present && !g.offscreen) {
        Present(gl.display_color);
    }
    g.display_pass = false;
    g.target = kMainTarget;
    if (present) {
        g.presented_display = true;
    }
    StreamEndFrame();
}

} // namespace detail

using namespace detail;

float LogicalWidth() { return g.config.logical_width; }

float LogicalHeight() { return g.config.logical_height; }

LogicalRect LogicalFrame() { return {0.0f, 0.0f, LogicalWidth(), LogicalHeight()}; }

void RendererInit(SDL_Window *window, const RendererConfig &config) {
    static uint64_t instances = 0;
    g = {};
    gl = {};
    g.renderer_instance = ++instances;
    g.window = window;
    g.config = config;
    g.offscreen = config.offscreen;
    g.layout = config.layout;

    gl.context = SDL_GL_CreateContext(window);
    if (gl.context == nullptr) {
        Fatal("SDL_GL_CreateContext: %s", SDL_GetError());
    }
    if (!SDL_GL_MakeCurrent(window, gl.context)) {
        Fatal("SDL_GL_MakeCurrent: %s", SDL_GetError());
    }
    if (const char *missing = LoadGlFunctions(); missing != nullptr) {
        Fatal("GL: the context has no %s (OpenGL 4.3 and ARB_clip_control are needed)", missing);
    }
    glGetIntegerv(GL_MAJOR_VERSION, &gl.gl_major);
    glGetIntegerv(GL_MINOR_VERSION, &gl.gl_minor);
    if (gl.gl_major < 4 || (gl.gl_major == 4 && gl.gl_minor < 3)) {
        Fatal("GL: OpenGL 4.3 is needed; the context is %d.%d", gl.gl_major, gl.gl_minor);
    }
    std::fprintf(stderr, "GL: using %s (%s)%s\n", reinterpret_cast<const char *>(glGetString(GL_RENDERER)),
                 reinterpret_cast<const char *>(glGetString(GL_VERSION)), g.offscreen ? ", offscreen" : "");
    bool validation = config.validation;
#ifdef NDEBUG
    validation = false;
#endif
    if (validation && glDebugMessageCallback != nullptr) {
        glEnable(GL_DEBUG_OUTPUT);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        glDebugMessageCallback(DebugCallback, nullptr);
    }
    // Vulkan's depth range: z in [0, 1] as it is, reverse-Z cleared to 0.
    glClipControl(GL_LOWER_LEFT, GL_ZERO_TO_ONE);
    glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &gl.uniform_alignment);
    gl.uniform_alignment = std::max(gl.uniform_alignment, 16);
    glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &gl.max_anisotropy);
    glGetError(); // the anisotropy query fails where the extension is missing
    g.triangle_fans = true;
    g.separate_stencil_masks = true;
    g.dynamic_color_write_mask = true;
    g.sampler_anisotropy = gl.max_anisotropy > 1.0f;
    // displaylist.cpp renders a list only while a device is up.
    g.device = reinterpret_cast<VkDevice>(std::uintptr_t{1});

    CreateSamplers();
    CreateVertexArrays();
    InitStream();
    InitGlResources();
    BuildGlBlendTable();
    uint32_t width;
    uint32_t height;
    WindowPixels(width, height);
    // A window that starts minimised still needs main targets for render-to-texture work.
    CreateMainTargetsGl(std::max(width, 1u), std::max(height, 1u));
    g.render_scale = config.render_scale > 0.0f
                         ? config.render_scale
                         : std::max(1.0f, std::round(static_cast<float>(gl.height) / g.config.logical_height));
    CreatePrograms();
    ApplySwapInterval();
    ResetDrawState();
}

void RendererShutdown() {
    if (gl.context == nullptr) {
        return;
    }
    g.list.reset();
    glFinish();
    HarvestDepthQueriesGl();
    DestroyPrograms();
    ShutdownGlResources();
    DestroyMainTargetsGl();
    for (auto &[key, framebuffer] : gl.framebuffers) {
        glDeleteFramebuffers(1, &framebuffer);
    }
    gl.framebuffers.clear();
    ShutdownStream();
    if (gl.depth_pack != 0) {
        glDeleteBuffers(1, &gl.depth_pack);
    }
    glDeleteSamplers(kSamplerCount, gl.samplers);
    glDeleteSamplers(1, &gl.palette_sampler);
    glDeleteVertexArrays(kLayoutCount, gl.vertex_arrays);
    SDL_GL_MakeCurrent(g.window, nullptr);
    SDL_GL_DestroyContext(gl.context);
    uint32_t messages = g.validation_messages;
    g = {};
    gl = {};
    g.validation_messages = messages;
}

void RendererResize() { g.resize_pending = true; }

void SetAnisotropy(int samples) {
    uint32_t level = 0;
    while (level < kAnisotropyLevels && (2 << level) <= samples) {
        ++level;
    }
    g.anisotropy_level = level;
}

void SetPresentMode(PresentMode mode) { g.config.present_mode = mode; }

bool BeginFrame() {
    if (g.in_frame || g.list != nullptr) {
        Error("BeginFrame inside a frame");
        return true;
    }
    if (!SyncMainTargetsGl() && !g.offscreen) {
        return false;
    }
    return OpenFrame();
}

void EndFrame() {
    if (!g.in_frame) {
        return;
    }
    EndRendering();
    g.in_frame = false;
    CloseDepthQueries();
    g.canonical_newest = false;
    g.presented_display = false;
    if (!g.offscreen) {
        Present(CurrentMainColorGl());
    }
    FinishFrame();
}

bool PresentCanonical() {
    if (!g.canonical_newest || g.in_frame || g.list != nullptr) {
        return false;
    }
    g.presented_display = false;
    if (g.offscreen) {
        return true;
    }
    uint32_t width;
    uint32_t height;
    WindowPixels(width, height);
    if (width == 0 || height == 0) {
        return false;
    }
    Present(PreviousMainColorGl());
    return true;
}

bool InFrame() { return g.in_frame || g.list != nullptr; }

RendererFeatures ActiveRendererFeatures() {
    uint32_t version = static_cast<uint32_t>(gl.gl_major) << 22 | static_cast<uint32_t>(gl.gl_minor) << 12;
    return {version, g.offscreen, true, true, true, false};
}

// The GL renderer needs no surface: a headless run draws into its own targets in a window that is
// never shown.
bool HeadlessSurfaceAvailable() { return true; }

uint32_t ValidationMessageCount() { return g.validation_messages; }

uint32_t PipelineCount() { return gl.program_count; }

double PipelineCompileSeconds() { return gl.program_seconds; }

float RenderScale() { return g.render_scale; }

void SetFrameLayout(const FrameLayout &layout) {
    if (g.in_frame || g.list != nullptr) {
        Error("SetFrameLayout inside a frame");
        return;
    }
    if (!(layout.ui_scale > 0.0f) || !std::isfinite(layout.ui_scale)) {
        Error("SetFrameLayout: ui_scale %g", static_cast<double>(layout.ui_scale));
        return;
    }
    g.layout = layout;
    if (gl.context != nullptr) {
        RecreateRenderTargetsGl();
    }
}

FrameLayout CurrentFrameLayout() { return g.layout; }

void SetRenderScale(float scale) {
    if (scale <= 0.0f || scale == g.render_scale || g.in_frame || g.list != nullptr) {
        return;
    }
    g.render_scale = scale;
    RecreateRenderTargetsGl();
}

// ---- Readback ----------------------------------------------------------------------------------

void ReadDepth(uint32_t id, float x, float y, float w, float h) {
    if (id >= kDepthQueryCount) {
        Error("ReadDepth: query %u is past %u", id, kDepthQueryCount);
        return;
    }
    if (RecordingCalls()) {
        RecordEntry(ReadDepthEntry{
            id, LogicalRect{x, y, w, h}
        });
        return;
    }
    DepthQuery &query = g.depth_queries[id];
    query.rect = LogicalRect{x, y, w, h};
    query.queued = true;
}

std::optional<float> DepthResult(uint32_t id) {
    if (id >= kDepthQueryCount || !g.depth_queries[id].valid) {
        return std::nullopt;
    }
    return g.depth_queries[id].result;
}

bool ReadbackFrame(std::vector<uint8_t> &rgba, uint32_t &width, uint32_t &height) {
    if (g.in_frame || RecordingCalls()) {
        Error("ReadbackFrame inside a frame");
        return false;
    }
    const GlImage &image = g.presented_display ? gl.display_color : PreviousMainColorGl();
    width = image.width;
    height = image.height;
    return ReadImage(image, rgba);
}

bool ReadbackTexture(TextureHandle handle, std::vector<uint8_t> &pixels, uint32_t &width, uint32_t &height) {
    if (g.in_frame || RecordingCalls()) {
        Error("ReadbackTexture inside a frame");
        return false;
    }
    GlImage *image = ColorImageOfGl(handle);
    if (image == nullptr) {
        Error("ReadbackTexture: no texture %#x", handle);
        return false;
    }
    width = image->width;
    height = image->height;
    return ReadImage(*image, pixels);
}

namespace {

uint32_t Crc32(const uint8_t *data, size_t size, uint32_t crc = 0) {
    static const std::array<uint32_t, 256> table = [] {
        std::array<uint32_t, 256> entries{};
        for (uint32_t i = 0; i < 256; i++) {
            uint32_t value = i;
            for (int bit = 0; bit < 8; bit++) {
                value = (value & 1) ? 0xEDB88320u ^ (value >> 1) : value >> 1;
            }
            entries[i] = value;
        }
        return entries;
    }();
    crc = ~crc;
    for (size_t i = 0; i < size; i++) {
        crc = table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    }
    return ~crc;
}

void PutBigEndian(std::vector<uint8_t> &out, uint32_t value) {
    out.push_back(static_cast<uint8_t>(value >> 24));
    out.push_back(static_cast<uint8_t>(value >> 16));
    out.push_back(static_cast<uint8_t>(value >> 8));
    out.push_back(static_cast<uint8_t>(value));
}

void PutChunk(std::vector<uint8_t> &out, const char type[4], const std::vector<uint8_t> &data) {
    PutBigEndian(out, static_cast<uint32_t>(data.size()));
    size_t start = out.size();
    out.insert(out.end(), type, type + 4);
    out.insert(out.end(), data.begin(), data.end());
    PutBigEndian(out, Crc32(out.data() + start, out.size() - start));
}

} // namespace

bool WritePng(const std::filesystem::path &path, const uint8_t *rgba, uint32_t width, uint32_t height) {
    std::vector<uint8_t> raw;
    raw.reserve((static_cast<size_t>(width) * 3 + 1) * height);
    for (uint32_t y = 0; y < height; y++) {
        raw.push_back(0);
        const uint8_t *row = rgba + static_cast<size_t>(y) * width * 4;
        for (uint32_t x = 0; x < width; x++) {
            raw.insert(raw.end(), row + x * 4, row + x * 4 + 3);
        }
    }

    // zlib stream of stored deflate blocks: no compressor, and screenshots are for tests.
    std::vector<uint8_t> zlib = {0x78, 0x01};
    size_t               pos = 0;
    do {
        size_t length = std::min<size_t>(raw.size() - pos, 0xFFFF);
        bool   last = pos + length == raw.size();
        zlib.push_back(last ? 1 : 0);
        zlib.push_back(static_cast<uint8_t>(length));
        zlib.push_back(static_cast<uint8_t>(length >> 8));
        zlib.push_back(static_cast<uint8_t>(~length));
        zlib.push_back(static_cast<uint8_t>(~length >> 8));
        zlib.insert(zlib.end(), raw.begin() + static_cast<std::ptrdiff_t>(pos),
                    raw.begin() + static_cast<std::ptrdiff_t>(pos + length));
        pos += length;
    } while (pos < raw.size());
    uint32_t a = 1;
    uint32_t b = 0;
    for (uint8_t byte : raw) {
        a = (a + byte) % 65521;
        b = (b + a) % 65521;
    }
    PutBigEndian(zlib, (b << 16) | a);

    std::vector<uint8_t> header;
    PutBigEndian(header, width);
    PutBigEndian(header, height);
    header.insert(header.end(), {8, 2, 0, 0, 0});

    std::vector<uint8_t> png = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
    PutChunk(png, "IHDR", header);
    PutChunk(png, "IDAT", zlib);
    PutChunk(png, "IEND", {});

    std::error_code error;
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path(), error);
    }
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    return file && file.write(reinterpret_cast<const char *>(png.data()), static_cast<std::streamsize>(png.size()));
}

} // namespace gfx
