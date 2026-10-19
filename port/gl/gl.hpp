#pragma once

// The GL renderer's internals (port/gl/README.md). It implements gfx.hpp in place of Kazarov's
// Vulkan units, over the display lists (displaylist.cpp) and TIM2 decoding (tim2.cpp) that build
// keeps; displaylist.cpp reads the renderer's state through context.hpp's Context, so the fields
// that are not Vulkan's (frame, target, layout, depth queries, display lists) live there as in the
// Vulkan renderer, and the GL objects live in State below.

#include <GL/glcorearb.h>
#include <SDL3/SDL_video.h>

#include <array>
#include <cstdint>
#include <unordered_map>
#include <vector>

#include "context.hpp"
#include "displaylist.hpp"

namespace gfx::detail {

// ---- Functions, loaded through SDL_GL_GetProcAddress (loader.cpp) ---------------------------------
// Named as GL names them, in a namespace of their own: the Switch's libGLESv2 defines some of the
// same names as functions.

#define KZ_GL_FUNCTIONS(X)                                                                                             \
    X(PFNGLGETSTRINGPROC, glGetString)                                                                                 \
    X(PFNGLGETSTRINGIPROC, glGetStringi)                                                                               \
    X(PFNGLGETINTEGERVPROC, glGetIntegerv)                                                                             \
    X(PFNGLGETFLOATVPROC, glGetFloatv)                                                                                 \
    X(PFNGLGETERRORPROC, glGetError)                                                                                   \
    X(PFNGLENABLEPROC, glEnable)                                                                                       \
    X(PFNGLDISABLEPROC, glDisable)                                                                                     \
    X(PFNGLVIEWPORTPROC, glViewport)                                                                                   \
    X(PFNGLSCISSORPROC, glScissor)                                                                                     \
    X(PFNGLCLEARBUFFERFVPROC, glClearBufferfv)                                                                         \
    X(PFNGLCLEARBUFFERFIPROC, glClearBufferfi)                                                                         \
    X(PFNGLCLEARBUFFERIVPROC, glClearBufferiv)                                                                         \
    X(PFNGLCOLORMASKPROC, glColorMask)                                                                                 \
    X(PFNGLDEPTHMASKPROC, glDepthMask)                                                                                 \
    X(PFNGLDEPTHFUNCPROC, glDepthFunc)                                                                                 \
    X(PFNGLSTENCILFUNCSEPARATEPROC, glStencilFuncSeparate)                                                             \
    X(PFNGLSTENCILOPSEPARATEPROC, glStencilOpSeparate)                                                                 \
    X(PFNGLSTENCILMASKSEPARATEPROC, glStencilMaskSeparate)                                                             \
    X(PFNGLCULLFACEPROC, glCullFace)                                                                                   \
    X(PFNGLFRONTFACEPROC, glFrontFace)                                                                                 \
    X(PFNGLBLENDFUNCSEPARATEPROC, glBlendFuncSeparate)                                                                 \
    X(PFNGLBLENDEQUATIONSEPARATEPROC, glBlendEquationSeparate)                                                         \
    X(PFNGLCLIPCONTROLPROC, glClipControl)                                                                             \
    X(PFNGLGENTEXTURESPROC, glGenTextures)                                                                             \
    X(PFNGLDELETETEXTURESPROC, glDeleteTextures)                                                                       \
    X(PFNGLBINDTEXTUREPROC, glBindTexture)                                                                             \
    X(PFNGLACTIVETEXTUREPROC, glActiveTexture)                                                                         \
    X(PFNGLTEXSTORAGE2DPROC, glTexStorage2D)                                                                           \
    X(PFNGLTEXSUBIMAGE2DPROC, glTexSubImage2D)                                                                         \
    X(PFNGLTEXPARAMETERIPROC, glTexParameteri)                                                                         \
    X(PFNGLPIXELSTOREIPROC, glPixelStorei)                                                                             \
    X(PFNGLCOPYIMAGESUBDATAPROC, glCopyImageSubData)                                                                   \
    X(PFNGLGENSAMPLERSPROC, glGenSamplers)                                                                             \
    X(PFNGLDELETESAMPLERSPROC, glDeleteSamplers)                                                                       \
    X(PFNGLSAMPLERPARAMETERIPROC, glSamplerParameteri)                                                                 \
    X(PFNGLSAMPLERPARAMETERFPROC, glSamplerParameterf)                                                                 \
    X(PFNGLBINDSAMPLERPROC, glBindSampler)                                                                             \
    X(PFNGLGENFRAMEBUFFERSPROC, glGenFramebuffers)                                                                     \
    X(PFNGLDELETEFRAMEBUFFERSPROC, glDeleteFramebuffers)                                                               \
    X(PFNGLBINDFRAMEBUFFERPROC, glBindFramebuffer)                                                                     \
    X(PFNGLFRAMEBUFFERTEXTURE2DPROC, glFramebufferTexture2D)                                                           \
    X(PFNGLCHECKFRAMEBUFFERSTATUSPROC, glCheckFramebufferStatus)                                                       \
    X(PFNGLBLITFRAMEBUFFERPROC, glBlitFramebuffer)                                                                     \
    X(PFNGLREADBUFFERPROC, glReadBuffer)                                                                               \
    X(PFNGLDRAWBUFFERSPROC, glDrawBuffers)                                                                             \
    X(PFNGLREADPIXELSPROC, glReadPixels)                                                                               \
    X(PFNGLGENBUFFERSPROC, glGenBuffers)                                                                               \
    X(PFNGLDELETEBUFFERSPROC, glDeleteBuffers)                                                                         \
    X(PFNGLBINDBUFFERPROC, glBindBuffer)                                                                               \
    X(PFNGLBUFFERDATAPROC, glBufferData)                                                                               \
    X(PFNGLBUFFERSUBDATAPROC, glBufferSubData)                                                                         \
    X(PFNGLBUFFERSTORAGEPROC, glBufferStorage)                                                                         \
    X(PFNGLMAPBUFFERRANGEPROC, glMapBufferRange)                                                                       \
    X(PFNGLUNMAPBUFFERPROC, glUnmapBuffer)                                                                             \
    X(PFNGLBINDBUFFERRANGEPROC, glBindBufferRange)                                                                     \
    X(PFNGLGENVERTEXARRAYSPROC, glGenVertexArrays)                                                                     \
    X(PFNGLDELETEVERTEXARRAYSPROC, glDeleteVertexArrays)                                                               \
    X(PFNGLBINDVERTEXARRAYPROC, glBindVertexArray)                                                                     \
    X(PFNGLENABLEVERTEXATTRIBARRAYPROC, glEnableVertexAttribArray)                                                     \
    X(PFNGLVERTEXATTRIBFORMATPROC, glVertexAttribFormat)                                                               \
    X(PFNGLVERTEXATTRIBIFORMATPROC, glVertexAttribIFormat)                                                             \
    X(PFNGLVERTEXATTRIBBINDINGPROC, glVertexAttribBinding)                                                             \
    X(PFNGLBINDVERTEXBUFFERPROC, glBindVertexBuffer)                                                                   \
    X(PFNGLDRAWARRAYSPROC, glDrawArrays)                                                                               \
    X(PFNGLDRAWELEMENTSPROC, glDrawElements)                                                                           \
    X(PFNGLCREATESHADERPROC, glCreateShader)                                                                           \
    X(PFNGLSHADERSOURCEPROC, glShaderSource)                                                                           \
    X(PFNGLCOMPILESHADERPROC, glCompileShader)                                                                         \
    X(PFNGLGETSHADERIVPROC, glGetShaderiv)                                                                             \
    X(PFNGLGETSHADERINFOLOGPROC, glGetShaderInfoLog)                                                                   \
    X(PFNGLDELETESHADERPROC, glDeleteShader)                                                                           \
    X(PFNGLCREATEPROGRAMPROC, glCreateProgram)                                                                         \
    X(PFNGLATTACHSHADERPROC, glAttachShader)                                                                           \
    X(PFNGLLINKPROGRAMPROC, glLinkProgram)                                                                             \
    X(PFNGLGETPROGRAMIVPROC, glGetProgramiv)                                                                           \
    X(PFNGLGETPROGRAMINFOLOGPROC, glGetProgramInfoLog)                                                                 \
    X(PFNGLDELETEPROGRAMPROC, glDeleteProgram)                                                                         \
    X(PFNGLUSEPROGRAMPROC, glUseProgram)                                                                               \
    X(PFNGLUNIFORM4FPROC, glUniform4f)                                                                                 \
    X(PFNGLUNIFORM4IVPROC, glUniform4iv)                                                                               \
    X(PFNGLUNIFORM1IPROC, glUniform1i)                                                                                 \
    X(PFNGLBINDBUFFERBASEPROC, glBindBufferBase)                                                                       \
    X(PFNGLDISPATCHCOMPUTEPROC, glDispatchCompute)                                                                     \
    X(PFNGLMEMORYBARRIERPROC, glMemoryBarrier)                                                                         \
    X(PFNGLFENCESYNCPROC, glFenceSync)                                                                                 \
    X(PFNGLCLIENTWAITSYNCPROC, glClientWaitSync)                                                                       \
    X(PFNGLDELETESYNCPROC, glDeleteSync)                                                                               \
    X(PFNGLFLUSHPROC, glFlush)                                                                                         \
    X(PFNGLFINISHPROC, glFinish)                                                                                       \
    X(PFNGLGENQUERIESPROC, glGenQueries)                                                                               \
    X(PFNGLDELETEQUERIESPROC, glDeleteQueries)                                                                         \
    X(PFNGLQUERYCOUNTERPROC, glQueryCounter)                                                                           \
    X(PFNGLGETQUERYOBJECTIVPROC, glGetQueryObjectiv)                                                                   \
    X(PFNGLGETQUERYOBJECTUI64VPROC, glGetQueryObjectui64v)

// Optional: null where the context lacks them.
#define KZ_GL_OPTIONAL_FUNCTIONS(X)                                                                                    \
    X(PFNGLCLEARTEXIMAGEPROC, glClearTexImage)                                                                         \
    X(PFNGLDEBUGMESSAGECALLBACKPROC, glDebugMessageCallback)

namespace glapi {
#define KZ_GL_DECLARE(type, name) extern type name;
KZ_GL_FUNCTIONS(KZ_GL_DECLARE)
KZ_GL_OPTIONAL_FUNCTIONS(KZ_GL_DECLARE)
#undef KZ_GL_DECLARE
} // namespace glapi

using namespace glapi;

// Loads every function through SDL; names the first that is missing, or returns nullptr.
const char *LoadGlFunctions();

// ---- Objects ---------------------------------------------------------------------------------------

struct GlImage {
    GLuint   texture = 0;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t mips = 1;
    GLenum   format = 0; // GL_RGBA8, GL_R8 or GL_DEPTH32F_STENCIL8
};

struct GlTexture {
    bool        live = false;
    uint16_t    generation = 0;
    TextureDesc desc;
    GlImage     image;
    GlImage     depth; // render targets that keep their own only
    bool        render_target = false;
    bool        shares_main_depth = false;
    bool        frame = false;
    uint32_t    logical_width = 0;
    uint32_t    logical_height = 0;
    // The frame serial in which a draw last sampled or drew into it.
    uint64_t last_draw_use = 0;
    // Destroyed while a list that may draw it was recorded: dead to the API, alive to replays and to
    // the recording that destroyed it.
    const DisplayList *doomed = nullptr;
    // RendererConfig::expand_palettes, for index textures and palettes: the texels last uploaded,
    // mip by mip, and a count of the uploads. Empty once the image is written any other way.
    std::vector<std::vector<uint8_t>> contents;
    uint32_t                          contents_version = 0;
};

struct GlMesh {
    bool     live = false;
    uint16_t generation = 0;
    GLuint   buffer = 0; // vertices, then indices
    uint32_t vertex_count = 0;
    uint32_t index_count = 0;
    const DisplayList *doomed = nullptr;
};

// A span of the streaming buffer, written once by the CPU and read by the GPU in the frame it is
// taken in.
struct StreamSpan {
    GLintptr offset = 0;
    uint8_t *data = nullptr;
};

// Programs: three texture modes, alpha test or not, per vertex shader.
enum VertexKind : uint32_t {
    kVertex2D,
    kVertexMesh,
    kVertexProg0,
    kVertexProg0Skinned,
    kVertexKindCount,
};

// Vertex array objects, one per vertex layout.
enum VertexLayout : uint32_t {
    kLayout2D,
    kLayoutMesh,
    kLayoutProg0,
    kLayoutSkinned,
    kLayoutCount,
};

// The fixed-function blend a GS equation maps to (shaders.cpp).
struct GlBlend {
    bool   enable = false;
    GLenum src = GL_ONE;
    GLenum dst = GL_ZERO;
    GLenum op = GL_FUNC_ADD;

    bool operator==(const GlBlend &) const = default;
};

// GL state as last set, so draws set only what changes. -1: unknown.
struct GlBound {
    GLuint   program = 0;
    GLuint   framebuffer = UINT32_MAX;
    uint32_t viewport_w = 0;
    uint32_t viewport_h = 0;
    int      blend_enable = -1;
    GlBlend  blend;
    bool     blend_known = false;
    int      cull = -1;
    int      depth_test = -1;
    int      depth_write = -1;
    int      depth_func = -1;
    int      stencil_test = -1;
    int      stencil_ops[2] = {-1, -1};
    int      stencil_func[2] = {-1, -1};
    int      stencil_write_mask[2] = {-1, -1};
    int      color_write_mask = -1;
    int      scissor[4] = {-1, -1, -1, -1};
    GLuint   textures[2] = {UINT32_MAX, UINT32_MAX};
    GLuint   samplers[2] = {UINT32_MAX, UINT32_MAX};
    uint32_t layout = UINT32_MAX;
    GLuint   vertex_buffer[kLayoutCount] = {};
    GLintptr vertex_offset[kLayoutCount] = {};
    GLuint   element_buffer[kLayoutCount] = {};
    int      clip_distances = -1;
};

struct State {
    SDL_GLContext context = nullptr;
    int           gl_major = 0;
    int           gl_minor = 0;
    GLint         uniform_alignment = 256;
    float         max_anisotropy = 1.0f;
    bool          clip_control = false;

    // Two main colour images, swapped every frame: one is drawn, the other is the previous frame.
    std::array<GlImage, 2> main_color;
    GlImage                main_depth;
    GlImage                display_color;
    GlImage                display_depth;
    uint32_t               width = 0; // the main targets' size
    uint32_t               height = 0;

    std::vector<GlTexture> textures;
    std::vector<GlMesh>    meshes;
    // Framebuffers by colour and depth texture.
    std::unordered_map<uint64_t, GLuint> framebuffers;
    GLuint                               read_framebuffer = 0;
    GLuint                               draw_framebuffer = 0;

    GLuint samplers[kSamplerCount] = {};
    GLuint palette_sampler = 0;
    GLuint vertex_arrays[kLayoutCount] = {};
    // Indexed by ProgramIndex.
    std::vector<GLuint> programs;
    // Copies drawn through the 3D pipeline (CopyByDraw): the program, an empty vertex array, and
    // scratch images, one per format, for copies within one image.
    GLuint  copy_program = 0;
    GLuint  copy_vertex_array = 0;
    GlImage scratch_rgba;
    GlImage scratch_index;
    uint32_t            program_count = 0;
    double              program_seconds = 0.0;

    // The streaming buffer: kStreamSegments segments, one per frame in flight, each fenced when the
    // frame that wrote it is submitted.
    GLuint      stream = 0;
    uint8_t    *stream_data = nullptr;
    GLsizeiptr  stream_size = 0;
    bool        stream_persistent = false;
    uint32_t    segment = 0;
    GLsizeiptr  segment_used = 0;
    std::vector<GLsync> segment_fences;
    // Without persistent mapping: what this segment has taken so far, uploaded before the GPU reads it.
    std::vector<uint8_t> staging;
    GLsizeiptr           staged_from = 0;

    // Depth queries: a compute program reduces each query's rect to its farthest depth into a slot of
    // a persistently mapped buffer, read once the slot's fence has passed (renderer.cpp).
    GLuint      depth_program = 0;
    GLuint      depth_results = 0;
    const float *depth_results_data = nullptr;
    struct DepthSlot {
        GLsync   fence = nullptr;
        uint32_t queries = 0; // bit per query
        uint64_t serial = 0;
    } depth_slots[3];
    uint32_t depth_slot = 0;
    uint64_t depth_serial = 0;

    GlBound bound;
    bool    vsync = true;
};

extern State gl;

inline constexpr uint32_t kStreamSegments = 3;

// renderer.cpp
GlImage &CurrentMainColorGl();
GlImage &PreviousMainColorGl();
// What kMainTarget and its depth are in the frame being drawn: the display image in a display render.
GlImage &MainColorTargetGl();
GlImage &MainDepthGl();
// What kPreviousFrame samples.
GlImage &PreviousFrameImageGl();

// resources.cpp
void       InitGlResources();
void       ShutdownGlResources();
GlTexture *LookupGlTexture(TextureHandle handle);
GlMesh    *LookupGlMesh(MeshHandle handle);
GlImage   *ColorImageOfGl(TextureHandle handle);
GlImage    CreateGlImage(uint32_t width, uint32_t height, uint32_t mips, GLenum format);
void       DestroyGlImage(GlImage &image);
// A framebuffer drawing into color with depth (0: none).
GLuint FramebufferOf(GLuint color, GLuint depth);
// Every framebuffer that names texture is deleted with it.
void           ForgetFramebuffers(GLuint texture);
LogicalMapping GlTextureMapping(const GlTexture &texture);
bool           GlFrameShaped(const GlTexture &texture);
void           RecreateRenderTargetsGl();
void           RecreateSharedTargetsGl();
TextureHandle  PaletteExpansionGl(TextureHandle index, TextureHandle palette);
void           ForgetContentsGl(GlTexture *texture);
// Clears an image: colour to rgba (0..1), depth to 0 with stencil 0.
void ClearGlImage(const GlImage &image, const float rgba[4]);

// stream.cpp
void       InitStream();
void       ShutdownStream();
StreamSpan StreamAllocate(size_t size, size_t alignment);
// Makes the CPU's writes so far visible to the GPU (without persistent mapping, uploads them).
void StreamFlush();
// The frame is submitted: its segment is fenced and the next one is waited for.
void StreamEndFrame();
GLuint StreamBuffer();

// shaders.cpp
void     CreatePrograms();
void     DestroyPrograms();
uint32_t ProgramIndex(VertexKind kind, TextureMode mode, bool alpha_test);
void     BuildGlBlendTable();
// Maps a GS equation to the fixed-function blend and what the shader writes (BlendMapping, whose
// slot indexes GlBlendOf).
const GlBlend &GlBlendOf(uint16_t slot);

// draw.cpp
// Forgets what GL state is bound (a new frame, or GL calls made outside the draw path).
void ResetGlBound();
void BindDrawFramebuffer(GLuint framebuffer, uint32_t width, uint32_t height);

} // namespace gfx::detail
